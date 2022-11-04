/*=============================================================================
	DrawGouraud.cpp: Unreal XOpenGL DrawGouraud routines.
	Used for drawing meshes.

	VertLists are only supported by 227 so far, it pushes verts in a huge
	list instead of vertice by vertice. Currently this method improves
	performance 10x and more compared to unbuffered calls. Buffering
	catches up quite some.
	Copyright 2014-2021 Oldunreal

	Todo:
        * On a long run this should be replaced with a more mode
          modern mesh rendering method, but this requires also quite some
          rework in Render.dll and will be not compatible with other
          UEngine1 games.

	Revision history:
		* Created by Smirftsch
		* Added buffering to DrawGouraudPolygon
		* implemented proper usage of persistent buffers.
		* Added bindless texture support.

=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"
//#include "UnStaticLight.h"

enum DrawGouraudTexCoordsIndices
{
	DIFFUSE_INFO,			// UMult, VMult, Diffuse, Alpha
	DETAIL_MACRO_INFO,		// Detail UMult, Detail VMult, Macro UMult, Macro VMult
	MISC_INFO,				// BumpMap Specular, Gamma, texture format, Unused
	EDITOR_DRAWCOLOR,
	DISTANCE_FOG_COLOR,
	DISTANCE_FOG_INFO
};

UXOpenGLRenderDevice::DrawGouraudShaderDrawParams* UXOpenGLRenderDevice::DrawGouraudGetDrawParamsRef()
{
	return UsingShaderDrawParameters ?
		&reinterpret_cast<DrawGouraudShaderDrawParams*>(DrawGouraudSSBORange.Buffer)[DrawGouraudBufferData.Index * MAX_DRAWGOURAUD_BATCH + DrawGouraudMultiDrawCount] :
		&DrawGouraudDrawParams;
}

inline glm::vec4 FPlaneToVec4(FPlane Plane)
{
	return glm::vec4(Plane.X, Plane.Y, Plane.Z, Plane.W);
}

inline void UXOpenGLRenderDevice::DrawGouraudBufferVert( DrawGouraudBufferedVert* Vert, FTransTexture* P, DrawGouraudBuffer& BufferData )
{
	Vert->Point  = glm::vec3(P->Point.X, P->Point.Y, P->Point.Z);
	Vert->Normal = glm::vec3(P->Normal.X, P->Normal.Y, P->Normal.Z);
	Vert->UV     = glm::vec2(P->U, P->V);
	Vert->Light  = glm::vec4(P->Light.X, P->Light.Y, P->Light.Z, P->Light.W);
	Vert->Fog    = glm::vec4(P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W);
}

void UXOpenGLRenderDevice::DrawGouraudSetState(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags)
{
	SetProgram(GouraudPolyVert_Prog);

	// stijn: this absolutely kills performance on mac. You're updating global state here for every gouraud mesh/complex surface!
#if ENGINE_VERSION==227 && !__APPLE__
    // Update FrameCoords for shaders.
    // Depending on HUD calls for SetSceneNode SetSceneNode is unreliable, so have to ensure proper updating here.
    // This is only essential (so far) for NormalMaps/Parallax/HWLighting, so this may be kept disabled for non 227 builds.
    // However, even in debugmode I can't see any ḿeasurable performance difference at all (on my machine).
    // In order to have any (future) calculations depending on FrameCoords working correctly you may want to enable this in general.
    if (BumpMaps)
        UpdateCoords(Frame);
#endif

	DWORD NextPolyFlags = SetPolyFlags(PolyFlags);
	UBOOL NoNearZ = (GUglyHackFlags & HACKFLAGS_NoNearZ) == HACKFLAGS_NoNearZ;
	DrawFlags NoNearZFlag = NoNearZ ? DF_NoNearZ : DF_None;

	// Check if the uniforms will change
	if (!UsingShaderDrawParameters ||
		// force flush the buffer if we're rendering a mesh that is in zone 0 but shouldn't be...
		((DrawGouraudDrawParams.PolyFlags()^PolyFlags) & PF_ForceViewZone) ||
		// Check if the blending mode will change
		WillBlendStateChange(DrawGouraudDrawParams.PolyFlags(), NextPolyFlags) ||
		// Check if the texture will change
		WillTextureStateChange(0, Info, NextPolyFlags) ||
		// Check if NoNearZ will change
		((DrawGouraudDrawParams.DrawFlags()^NoNearZFlag) & DF_NoNearZ) ||
		// Check if we have room left in the multi-draw array
		DrawGouraudMultiDrawCount+1 >= MAX_DRAWGOURAUD_BATCH)
	{
		if (DrawGouraudBufferData.IndexOffset > 0)
		{
			unclockFast(Stats.GouraudPolyCycles);
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
			clockFast(Stats.GouraudPolyCycles);
			WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
		}

		SetBlend(NextPolyFlags, false);

		if (NoNearZ && (StoredFovAngle != Viewport->Actor->FovAngle || StoredFX != Frame->FX || StoredFY != Frame->FY || !StoredbNearZ))
			SetProjection(Frame, 1);
	}

	DrawGouraudDrawParams.PolyFlags() = NextPolyFlags;

	// Editor Support.
	if (GIsEditor)
	{
		if (HitTesting()) // UED only.
		{
			DrawGouraudDrawParams.HitTesting() = 1;
			DrawGouraudDrawParams.DrawData[EDITOR_DRAWCOLOR] = FPlaneToVec4(HitColor);
		}
		else
		{
			DrawGouraudDrawParams.HitTesting() = 0;
			DrawGouraudDrawParams.DrawData[EDITOR_DRAWCOLOR] = glm::vec4(0.f, 0.f, 0.f, Info.Texture->Alpha);
		}

		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawGouraudDrawParams.RendMap() = Frame->Viewport->Actor->RendMap;
	}

	SetTexture(0, Info, DrawGouraudDrawParams.PolyFlags(), 0, DF_DiffuseTexture);
	DrawGouraudDrawParams.DrawData[DIFFUSE_INFO] = glm::vec4(TexInfo[0].UMult, TexInfo[0].VMult, Info.Texture->Diffuse, Info.Texture->Alpha);
	DrawGouraudDrawParams.TexNum[0] = TexInfo[0].TexNum;

	DrawGouraudDrawParams.DrawFlags() = DF_DiffuseTexture | NoNearZFlag;

	if (Info.Texture->DetailTexture && DetailTextures)
	{
#if XOPENGL_MODIFIED_LOCK
		DrawGouraudDetailTextureInfo = Info.Texture->DetailTexture->GetTexture(INDEX_NONE, this);
#else
		Info.Texture->DetailTexture->Lock(DrawGouraudDetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
#endif
		DrawGouraudDrawParams.DrawFlags() |= DF_DetailTexture;

		SetTexture(1, FTEXTURE_GET(DrawGouraudDetailTextureInfo), Info.Texture->DetailTexture->PolyFlags, 0.0, DF_DetailTexture);
		DrawGouraudDrawParams.DrawData[DETAIL_MACRO_INFO] = glm::vec4(TexInfo[1].UMult, TexInfo[1].VMult, 0.f, 0.f);
		DrawGouraudDrawParams.TexNum[1] = TexInfo[1].TexNum;
	}
	else
	{
		DrawGouraudDrawParams.DrawData[DETAIL_MACRO_INFO] = glm::vec4(0.f, 0.f, 0.f, 0.f);
	}

#if ENGINE_VERSION==227
	if (Info.Texture->BumpMap && BumpMaps)
	{
#if XOPENGL_MODIFIED_LOCK
		DrawGouraudBumpMapInfo = Info.Texture->BumpMap->GetTexture(INDEX_NONE, this);
#else
		Info.Texture->BumpMap->Lock(DrawGouraudBumpMapInfo, Frame->Viewport->CurrentTime, -1, this);
#endif
		DrawGouraudDrawParams.DrawFlags() |= DF_BumpMap;

		SetTexture(2, FTEXTURE_GET(DrawGouraudBumpMapInfo), Info.Texture->BumpMap->PolyFlags, 0.0, DF_BumpMap);
		DrawGouraudDrawParams.DrawData[MISC_INFO] = glm::vec4(Info.Texture->BumpMap->Specular, Gamma, Info.Texture->Format, 0.f);
		DrawGouraudDrawParams.TexNum[2] = TexInfo[2].TexNum; //using Base Texture UV.
	}
	else
	{
#endif // ENGINE_VERSION
		DrawGouraudDrawParams.DrawData[MISC_INFO] = glm::vec4(0.f, Gamma, Info.Texture->Format, 0.f);
#if ENGINE_VERSION==227
	}
#endif

	if (Info.Texture->MacroTexture && MacroTextures)
	{
#if XOPENGL_MODIFIED_LOCK
		DrawGouraudMacroTextureInfo = Info.Texture->MacroTexture->GetTexture(INDEX_NONE, this);
#else
		Info.Texture->MacroTexture->Lock(DrawGouraudMacroTextureInfo, Frame->Viewport->CurrentTime, -1, this);
#endif
		DrawGouraudDrawParams.DrawFlags() |= DF_MacroTexture;

		SetTexture(3, FTEXTURE_GET(DrawGouraudMacroTextureInfo), Info.Texture->MacroTexture->PolyFlags, 0.0, DF_MacroTexture);
		DrawGouraudDrawParams.TexNum[3] = TexInfo[3].TexNum;

		DrawGouraudDrawParams.DrawData[DETAIL_MACRO_INFO].z = TexInfo[3].UMult;
		DrawGouraudDrawParams.DrawData[DETAIL_MACRO_INFO].w = TexInfo[3].VMult;
	}

	DrawGouraudDrawParams.DrawData[DISTANCE_FOG_COLOR] = DistanceFogColor;
	DrawGouraudDrawParams.DrawData[DISTANCE_FOG_INFO]  = DistanceFogValues;
}

void UXOpenGLRenderDevice::DrawGouraudReleaseState(FTextureInfo& Info)
{
	if (NoBuffering)
	{
		unclockFast(Stats.GouraudPolyCycles);
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
		clockFast(Stats.GouraudPolyCycles);
		WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
	}

#if !XOPENGL_MODIFIED_LOCK
	if (DrawGouraudDrawParams.DrawFlags() & DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(DrawGouraudDetailTextureInfo);

	if (DrawGouraudDrawParams.DrawFlags() & DF_BumpMap)
		Info.Texture->BumpMap->Unlock(DrawGouraudBumpMapInfo);

	if (DrawGouraudDrawParams.DrawFlags() & DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(DrawGouraudMacroTextureInfo);
#endif
}

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3 || /*Frame->Recursion > MAX_FRAME_RECURSION ||*/ NoDrawGouraud) //reject invalid.
		return;

	clockFast(Stats.GouraudPolyCycles);

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i]->U, Pts[i]->V, UM, VM);
	}
#endif

	if (DrawGouraudBufferData.IndexOffset >= DRAWGOURAUDPOLY_SIZE - DrawGouraudStrideSize * (NumPts - 2) * 3)
	{
		unclockFast(Stats.GouraudPolyCycles);
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
		debugf(NAME_DevGraphics, TEXT("DrawGouraudPolygon overflow!"));
		clockFast(Stats.GouraudPolyCycles);
		WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);

		// just in case...
		if (DrawGouraudStrideSize * (NumPts - 2) * 3 >= DRAWGOURAUDPOLY_SIZE)
		{
			GWarn->Logf(TEXT("DrawGouraudPolygon poly too big!"));
			return;
		}
	}

	DrawGouraudSetState(Frame, Info, PolyFlags);

	if (UsingShaderDrawParameters)
		memcpy(DrawGouraudGetDrawParamsRef(), &DrawGouraudDrawParams, sizeof(DrawGouraudDrawParams));
	DrawGouraudMultiDrawPolyListArray[DrawGouraudMultiDrawCount] = DrawGouraudMultiDrawVertices;

	auto Out = reinterpret_cast<DrawGouraudBufferedVert*>(
		&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset]);

	for ( INT i=0; i<NumPts-2; i++ )
    {
        DrawGouraudBufferVert(Out++, Pts[0    ], DrawGouraudBufferData);
		DrawGouraudBufferVert(Out++, Pts[i + 1], DrawGouraudBufferData);
		DrawGouraudBufferVert(Out++, Pts[i + 2], DrawGouraudBufferData);
    }

	DrawGouraudMultiDrawVertices      += (NumPts - 2) * 3;
	DrawGouraudBufferData.IndexOffset += (NumPts - 2) * 3 * (sizeof(DrawGouraudBufferedVert) / sizeof(FLOAT));
	DrawGouraudMultiDrawVertexCountArray[DrawGouraudMultiDrawCount++] = (NumPts - 2) * 3;

	DrawGouraudReleaseState(Info);
	unclockFast(Stats.GouraudPolyCycles);
	unguard;
}

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);
	if (NumPts < 3 || /*Frame->Recursion > MAX_FRAME_RECURSION ||*/ NoDrawGouraudList)		//reject invalid.
		return;

	clockFast(Stats.GouraudPolyCycles);

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i].U, Pts[i].V, UM, VM);
	}
#endif

	DrawGouraudSetState(Frame, Info, PolyFlags);

	if (UsingShaderDrawParameters)
		memcpy(DrawGouraudGetDrawParamsRef(), &DrawGouraudDrawParams, sizeof(DrawGouraudDrawParams));
	DrawGouraudMultiDrawPolyListArray[DrawGouraudMultiDrawCount] = DrawGouraudMultiDrawVertices;

	auto Out = reinterpret_cast<DrawGouraudBufferedVert*>(
		&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset]);
	auto End = reinterpret_cast<DrawGouraudBufferedVert*>(
		&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DRAWGOURAUDPOLY_SIZE - 1]);

	INT PolyListSize = 0;
    for (INT i = 0; i < NumPts; i++)
    {
		// stijn: this was the previous condition but this was all wrong! if we flush the buffer before the triangle we
		// were pushing is complete, all subsequent triangles we push will be borked as well!!
		//if ( DrawGouraudListBufferData.IndexOffset >= (DRAWGOURAUDPOLY_SIZE - DrawGouraudStrideSize))
		if ((i % 3 == 0) && (Out + 2 > End))
		{
			DrawGouraudMultiDrawVertices += PolyListSize;
			DrawGouraudBufferData.IndexOffset += PolyListSize * (sizeof(DrawGouraudBufferedVert) / sizeof(FLOAT));
			DrawGouraudMultiDrawVertexCountArray[DrawGouraudMultiDrawCount++] = PolyListSize;

			unclockFast(Stats.GouraudPolyCycles);
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
			debugf(NAME_DevGraphics, TEXT("DrawGouraudPolyList overflow!"));
			clockFast(Stats.GouraudPolyCycles);
			WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);

			DrawGouraudSetState(Frame, Info, PolyFlags);
			Out = reinterpret_cast<DrawGouraudBufferedVert*>(
				&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset]);

			if (UsingShaderDrawParameters)
				memcpy(DrawGouraudGetDrawParamsRef(), &DrawGouraudDrawParams, sizeof(DrawGouraudDrawParams));

			PolyListSize = 0;
		}

		DrawGouraudBufferVert(Out++, &Pts[i], DrawGouraudBufferData);
		PolyListSize++;
    }

	DrawGouraudMultiDrawVertices      += PolyListSize;
	DrawGouraudBufferData.IndexOffset += PolyListSize * (sizeof(DrawGouraudBufferedVert) / sizeof(FLOAT));
	DrawGouraudMultiDrawVertexCountArray[DrawGouraudMultiDrawCount++] = PolyListSize;

	DrawGouraudReleaseState(Info);
	unclockFast(Stats.GouraudPolyCycles);
	CHECK_GL_ERROR();

	unguard;
}
#endif

// stijn: This is the UT extended renderer interface. This does not map directly onto DrawGouraudPolyList because DrawGouraudTriangles pushes info out earlier
#if UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	INT StartOffset = 0;
	INT i = 0;

	if (Frame->NearClip.W != 0.0)
	{
		if (DrawGouraudBufferData.IndexOffset > 0)
		{
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
			WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
		}
		PushClipPlane(Frame->NearClip);
	}

	for (; i < NumPts; i += 3)
	{
		if (Frame->Mirror == -1.0)
			Exchange(Pts[i+2], Pts[i]);

		// Environment mapping.
		if (PolyFlags & PF_Environment)
		{
			FLOAT UScale = Info.UScale * Info.USize / 256.0f;
			FLOAT VScale = Info.VScale * Info.VSize / 256.0f;

			for (INT j = 0; j < 3; j++)
			{
				FVector T = Pts[i+j].Point.UnsafeNormal().MirrorByVector(Pts[i+j].Normal).TransformVectorBy(Frame->Uncoords);
				Pts[i+j].U = (T.X + 1.0f) * 0.5f * 256.0f * UScale;
				Pts[i+j].V = (T.Y + 1.0f) * 0.5f * 256.0f * VScale;
			}
		}

		// If outcoded, skip it.
		if (Pts[i].Flags & Pts[i + 1].Flags & Pts[i + 2].Flags)
		{
			// stijn: push the triangles we've already processed (if any)
			if (i - StartOffset > 0)
			{
				DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);
				StartOffset = i + 3;
			}
			continue;
		}

		// Backface reject it.
		if ((PolyFlags & PF_TwoSided) && FTriple(Pts[i].Point, Pts[i+1].Point, Pts[i+2].Point) <= 0.0)
		{
			if (!(PolyFlags & PF_TwoSided))
			{
				// stijn: push the triangles we've already processed (if any)
				if (i - StartOffset > 0)
				{
					DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);
					StartOffset = i + 3;
				}
				continue;
			}
			Exchange(Pts[i+2], Pts[i]);
		}
	}

	// stijn: push the remaining triangles
	if (i - StartOffset > 0)
		DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);

	if (Frame->NearClip.W != 0.0)
	{
		if (DrawGouraudBufferData.IndexOffset > 0)
		{
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
			WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
		}
		PopClipPlane();
	}
}
#endif


void UXOpenGLRenderDevice::DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer& BufferData)
{
	clockFast(Stats.GouraudPolyCycles);

	// Using one huge buffer instead of 3, interleaved data.
	// This is to reduce overhead by reducing API calls.
	GLuint TotalSize = BufferData.IndexOffset;
	CHECK_GL_ERROR();

	checkSlow(ActiveProgram == GouraudPolyVert_Prog);

	if (!UsingPersistentBuffersGouraud)
	{
	    if (UseBufferInvalidation)
        {
            glInvalidateBufferData(DrawGouraudVertBuffer);
            CHECK_GL_ERROR();
        }
#if defined(__LINUX_ARM__) || MACOSX
        glBufferData(GL_ARRAY_BUFFER, TotalSize * sizeof(float), DrawGouraudBufferRange.Buffer, GL_DYNAMIC_DRAW);
#else
		glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(float), DrawGouraudBufferRange.Buffer);
#endif

		if (UsingShaderDrawParameters)
		{
			if (UseBufferInvalidation)
			{
				glInvalidateBufferData(DrawGouraudSSBO);
				CHECK_GL_ERROR();
			}

#if defined(__LINUX_ARM__) || MACOSX
			glBufferData(GL_SHADER_STORAGE_BUFFER, DrawGouraudMultiDrawCount * sizeof(DrawGouraudShaderDrawParams), DrawGouraudSSBORange.Buffer, GL_DYNAMIC_DRAW);
#else
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, DrawGouraudMultiDrawCount * sizeof(DrawGouraudShaderDrawParams), DrawGouraudSSBORange.Buffer);
#endif
		}

		// GL > 4.5 for later use maybe.
		/*
		if (ActiveProgram == GouraudPolyVertList_Prog)
			glNamedBufferData(DrawGouraudVertListBuffer, sizeof(float) * TotalSize, verts, GL_STREAM_DRAW);
		else if (ActiveProgram == GouraudPolyVert_Prog)
			glNamedBufferData(DrawGouraudVertBuffer, sizeof(float) * TotalSize, verts, GL_STREAM_DRAW);
		CHECK_GL_ERROR();
		*/
	}
/*
	else
	{
        if (ActiveProgram == GouraudPolyVertList_Prog)
            glFlushMappedNamedBufferRange(DrawGouraudVertListBuffer, sizeof(float) * BufferData.BeginOffset, TotalSize * sizeof(float));
        else if (ActiveProgram == GouraudPolyVert_Prog)
            glFlushMappedNamedBufferRange(DrawGouraudVertBuffer, sizeof(float) * BufferData.BeginOffset, TotalSize * sizeof(float));
	}
*/

	GLintptr BeginOffset = BufferData.BeginOffset * sizeof(float);

	if (BeginOffset != PrevDrawGouraudBeginOffset)
	{
		// Coords
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)    BeginOffset);
		// Normals
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ sizeof(FLOAT) * 3));
		// UV
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ sizeof(FLOAT) * 6));
		// LightColor
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ sizeof(FLOAT) * 8));
		// FogColor
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ sizeof(FLOAT) * 12));
		CHECK_GL_ERROR();

		PrevDrawGouraudBeginOffset = BeginOffset;
	}

	if (!UsingShaderDrawParameters)
	{
		glUniform1uiv(DrawGouraudDrawFlags, 4, &DrawGouraudDrawParams._DrawFlags[0]);
		glUniform1uiv(DrawGouraudTexNum, 4, reinterpret_cast<const GLuint*>(&DrawGouraudDrawParams.TexNum[0]));
		glUniform4fv(DrawGouraudDrawData, ARRAY_COUNT(DrawGouraudDrawParams.DrawData), reinterpret_cast<const GLfloat*>(DrawGouraudDrawParams.DrawData));

	}

    CHECK_GL_ERROR();

	// Draw
	if (OpenGLVersion == GL_Core)
	{
		glMultiDrawArrays(Mode, DrawGouraudMultiDrawPolyListArray, DrawGouraudMultiDrawVertexCountArray, DrawGouraudMultiDrawCount);
	}
	else
	{
		for (INT i = 0; i < DrawGouraudMultiDrawCount; ++i)
			glDrawArrays(Mode, DrawGouraudMultiDrawPolyListArray[i], DrawGouraudMultiDrawVertexCountArray[i]);
	}

	// reset
	DrawGouraudMultiDrawVertices = DrawGouraudMultiDrawCount = 0;

	if (UsingPersistentBuffersGouraud)
	{
		LockBuffer(DrawGouraudBufferRange, BufferData.Index);
		BufferData.Index = (BufferData.Index + 1) % NUMBUFFERS;
		CHECK_GL_ERROR();
	}

	BufferData.BeginOffset = BufferData.Index * DRAWGOURAUDPOLY_SIZE;
    BufferData.IndexOffset = 0;

	unclockFast(Stats.GouraudPolyCycles);
}

BYTE UXOpenGLRenderDevice::PushClipPlane(const FPlane& Plane)
{
	guard(UXOpenGLRenderDevice::PushClipPlane);
	if (NumClipPlanes == MaxClippingPlanes)
		return 2;

	glEnable(GL_CLIP_DISTANCE0 + NumClipPlanes);
	glm::vec4 ClipParams = glm::vec4(NumClipPlanes, 1.f, 0.f, 0.f);
	glm::vec4 ClipPlane = glm::vec4(Plane.X, Plane.Y, Plane.Z, Plane.W);

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalClipPlaneUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(ClipParams));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(ClipPlane));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	CHECK_GL_ERROR();
	++NumClipPlanes;

	return 1;
	unguard;
}
BYTE UXOpenGLRenderDevice::PopClipPlane()
{
	guard(UXOpenGLRenderDevice::PopClipPlane);
	if (NumClipPlanes == 0)
		return 2;

	--NumClipPlanes;
	glDisable(GL_CLIP_DISTANCE0 + NumClipPlanes);

	glm::vec4 ClipParams = glm::vec4(NumClipPlanes, 0.f, 0.f, 0.f);
	glm::vec4 ClipPlane = glm::vec4(0.f, 0.f, 0.f, 0.f);

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalClipPlaneUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(ClipParams));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(ClipPlane));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	CHECK_GL_ERROR();

	return 1;
	unguard;
}

void UXOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf)
{
	guard(UOpenGLRenderDevice::PreDrawGouraud);

	if (FogSurf.IsValid())
	{
		DistanceFogColor = glm::vec4(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, FogSurf.FogColor.W);
		DistanceFogValues = glm::vec4(FogSurf.FogDistanceStart, FogSurf.FogDistanceEnd, FogSurf.FogDensity, (GLfloat)(FogSurf.FogMode));

		bFogEnabled = true;
	}
	else if (bFogEnabled)
		ResetFog();

	unguard;
}

void UXOpenGLRenderDevice::PostDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf)
{
	guard(UOpenGLRenderDevice::PostDrawGouraud);

#if ENGINE_VERSION==227
	ResetFog();
#endif // ENGINE_VERSION

	unguard;
}

//
// Program Switching
//
void UXOpenGLRenderDevice::DrawGouraudEnd(INT NextProgram)
{
	if (DrawGouraudBufferData.IndexOffset > 0)
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);

	CHECK_GL_ERROR();

	// Clean up
	for (INT i = 0; i < 5; ++i)
		glDisableVertexAttribArray(i);
	CHECK_GL_ERROR();
}

void UXOpenGLRenderDevice::DrawGouraudStart()
{
	clockFast(Stats.GouraudPolyCycles);
	WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);

	glUseProgram(DrawGouraudProg);

#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
	if (UseAA && NoAATiles && PrevProgram != ComplexSurfaceSinglePass_Prog)
		glEnable(GL_MULTISAMPLE);
#endif

	glBindVertexArray(DrawGouraudPolyVertsVao);
	glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
	if (UsingShaderDrawParameters)
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBO);

	for (INT i = 0; i < 5; ++i)
		glEnableVertexAttribArray(i);

	DrawGouraudDrawParams.PolyFlags() = 0;// SetPolyFlags(CurrentAdditionalPolyFlags | CurrentPolyFlags);
	PrevDrawGouraudBeginOffset		  = -1;

	CHECK_GL_ERROR();
	unclockFast(Stats.GouraudPolyCycles);
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

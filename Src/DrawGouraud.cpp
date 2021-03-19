/*=============================================================================
	DrawGouraud.cpp: Unreal XOpenGL DrawGouraud routines.
	Used for drawing meshes.

	VertLists are only supported by 227 so far, it pushes verts in a huge
	list instead of vertice by vertice. Currently this method improves
	performance 10x and more compared to unbuffered calls. Buffering
	catches up quite some.
	Copyright 2014-2017 Oldunreal

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

inline void UXOpenGLRenderDevice::BufferGouraudPolygonPoint( FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& BufferData )
{
	// Verts
	DrawGouraudTemp[0] = P->Point.X;
	DrawGouraudTemp[1] = P->Point.Y;
	DrawGouraudTemp[2] = P->Point.Z;
	BufferData.VertSize += FloatsPerVertex; // Points

	// Textures
	DrawGouraudTemp[3] = P->U;
	DrawGouraudTemp[4] = P->V;

	DrawGouraudTemp[5] = P->Normal.X;
	DrawGouraudTemp[6] = P->Normal.Y;
	DrawGouraudTemp[7] = P->Normal.Z;
	DrawGouraudTemp[8] = P->Normal.W;

	// Light color
	DrawGouraudTemp[9] = P->Light.X;
	DrawGouraudTemp[10] = P->Light.Y;
	DrawGouraudTemp[11] = P->Light.Z;
	DrawGouraudTemp[12] = P->Light.W;

	// Vertex Fog color
	DrawGouraudTemp[13] = P->Fog.X;
	DrawGouraudTemp[14] = P->Fog.Y;
	DrawGouraudTemp[15] = P->Fog.Z;
	DrawGouraudTemp[16] = P->Fog.W;

	DrawGouraudTemp[17] = (GLfloat)BufferData.TexNum[0];
	DrawGouraudTemp[18] = (GLfloat)BufferData.TexNum[1];
	DrawGouraudTemp[19] = (GLfloat)BufferData.TexNum[2];

	DrawGouraudTemp[20] = BufferData.TexUMult;
	DrawGouraudTemp[21] = BufferData.TexVMult;
	DrawGouraudTemp[22] = BufferData.MacroTexUMult;
	DrawGouraudTemp[23] = BufferData.MacroTexVMult;

	DrawGouraudTemp[24] = BufferData.DetailTexUMult;
	DrawGouraudTemp[25] = BufferData.DetailTexVMult;
	DrawGouraudTemp[26] = BufferData.TextureFormat;
	DrawGouraudTemp[27] = (GLfloat)BufferData.DrawFlags;

	// Additional texture info. Added here to keep batched drawing.
	DrawGouraudTemp[28] = BufferData.TextureDiffuse;
	DrawGouraudTemp[29] = BufferData.BumpTextureSpecular;
	DrawGouraudTemp[30] = BufferData.TextureAlpha;
	DrawGouraudTemp[31] = (GLfloat)BufferData.TexNum[3];

    BufferData.IndexOffset += 32;
}
inline void UXOpenGLRenderDevice::BufferGouraudPolygonVert(FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& BufferData)
{
	// Verts
	DrawGouraudTemp[0] = P->Point.X;
	DrawGouraudTemp[1] = P->Point.Y;
	DrawGouraudTemp[2] = P->Point.Z;
	BufferData.VertSize += FloatsPerVertex; // Points
}

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3 || Frame->Recursion > MAX_FRAME_RECURSION || NoDrawGouraud) //reject invalid.
		return;

	SetProgram(GouraudPolyVert_Prog);

	if ((GUglyHackFlags & HACKFLAGS_NoNearZ) && (StoredFovAngle != Viewport->Actor->FovAngle || StoredFX != Frame->FX || StoredFY != Frame->FY || !StoredbNearZ))
		SetProjection(Frame, 1);

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i]->U, Pts[i]->V, UM, VM);
	}
#endif

	if ((DrawGouraudBufferData.VertSize > 0) && ((!UsingBindlessTextures && (TexInfo[0].CurrentCacheID != Info.CacheID)) || (DrawGouraudBufferData.PrevPolyFlags != PolyFlags)))
	{
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
	}

	DrawGouraudBufferData.PrevPolyFlags = PolyFlags;
	DrawGouraudBufferData.PolyFlags = SetFlags(PolyFlags);
	SetBlend(DrawGouraudBufferData.PolyFlags, false);
	SetTexture(0, Info, DrawGouraudBufferData.PolyFlags, 0, GouraudPolyVert_Prog, NORMALTEX);

    DrawGouraudBufferData.TexNum[0] = TexInfo[0].TexNum;
	DrawGouraudBufferData.Alpha = Info.Texture->Alpha;
	DrawGouraudBufferData.TextureDiffuse = Info.Texture->Diffuse;
	DrawGouraudBufferData.TexUMult = TexInfo[0].UMult;
	DrawGouraudBufferData.TexVMult = TexInfo[0].VMult;

	if (GIsEditor)
	{
		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawGouraudBufferData.RendMap = Frame->Viewport->Actor->RendMap;
	}

	DrawGouraudBufferData.DrawFlags = DF_DiffuseTexture;

	FTextureInfo DetailTextureInfo;
	FTextureInfo MacroTextureInfo;
	FTextureInfo BumpMapInfo;

	if (Info.Texture)
    {
        DrawGouraudBufferData.TextureFormat = (GLfloat)Info.Texture->Format;
        DrawGouraudBufferData.TextureAlpha = (GLfloat)Info.Texture->Alpha;

        if (Info.Texture->DetailTexture && DetailTextures)
        {

            Info.Texture->DetailTexture->Lock(DetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_DetailTexture;

            SetTexture(1, DetailTextureInfo, Info.Texture->DetailTexture->PolyFlags, 0.0, GouraudPolyVert_Prog, DETAILTEX);
            DrawGouraudBufferData.TexNum[1] = TexInfo[1].TexNum;

            DrawGouraudBufferData.DetailTexUMult = TexInfo[1].UMult;
            DrawGouraudBufferData.DetailTexVMult = TexInfo[1].VMult;
        }

    #if ENGINE_VERSION==227
        if (Info.Texture->BumpMap && BumpMaps)
        {
            Info.Texture->BumpMap->Lock(BumpMapInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_BumpMap;

            SetTexture(2, BumpMapInfo, Info.Texture->BumpMap->PolyFlags, 0.0, GouraudPolyVert_Prog, BUMPMAP);
            DrawGouraudBufferData.TexNum[2] = TexInfo[2].TexNum; //using Base Texture UV.

            DrawGouraudBufferData.BumpTextureSpecular = Info.Texture->BumpMap->Specular;
        }
    #endif // ENGINE_VERSION

        if (Info.Texture->MacroTexture && MacroTextures)
        {

            Info.Texture->MacroTexture->Lock(MacroTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_MacroTexture;

            SetTexture(3, MacroTextureInfo, Info.Texture->MacroTexture->PolyFlags, 0.0, GouraudPolyVert_Prog, MACROTEX);
            DrawGouraudBufferData.TexNum[3] = TexInfo[3].TexNum;

            DrawGouraudBufferData.MacroTexUMult = TexInfo[3].UMult;
            DrawGouraudBufferData.MacroTexVMult = TexInfo[3].VMult;
        }
    }

	clockFast(Stats.GouraudPolyCycles);
    CHECK_GL_ERROR();

    if (UsingPersistentBuffersGouraud && DrawGouraudBufferRange.Sync[DrawGouraudBufferData.Index])
        WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
    CHECK_GL_ERROR();

    for ( INT i=0; i<NumPts-2; i++ )
    {
        BufferGouraudPolygonPoint(&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset], Pts[0], DrawGouraudBufferData);
        BufferGouraudPolygonPoint(&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset], Pts[i + 1], DrawGouraudBufferData);
        BufferGouraudPolygonPoint(&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset], Pts[i + 2], DrawGouraudBufferData);
		if ( DrawGouraudBufferData.IndexOffset >= (DRAWGOURAUDPOLY_SIZE - DrawGouraudStrideSize))
        {
            DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
            debugf(NAME_DevGraphics, TEXT("DrawGouraudPolygon overflow!"));
        }
    }
    CHECK_GL_ERROR();

    if (NoBuffering)
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);

	if (DrawGouraudBufferData.DrawFlags & DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(DetailTextureInfo);

	if (DrawGouraudBufferData.DrawFlags & DF_BumpMap)
		Info.Texture->BumpMap->Unlock(BumpMapInfo);

    if (DrawGouraudBufferData.DrawFlags & DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(MacroTextureInfo);

	CHECK_GL_ERROR();
	unclockFast(Stats.GouraudPolyCycles);
	unguard;
}

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);
	if (NumPts < 3 || Frame->Recursion > MAX_FRAME_RECURSION || NoDrawGouraudList)		//reject invalid.
		return;

    SetProgram(GouraudPolyVertList_Prog);

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i].U, Pts[i].V, UM, VM);
	}
#endif

	bool TextureChanged = 
		(!UsingBindlessTextures && TexInfo[0].CurrentCacheID != Info.CacheID) ||
		(UsingBindlessTextures && GetBindlessTexNum(Info.Texture, PolyFlags) != TexInfo[0].TexNum);

	if ( DrawGouraudListBufferData.VertSize > 0 && 
		(TextureChanged || DrawGouraudListBufferData.PrevPolyFlags != PolyFlags))
	{
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
	}
    DrawGouraudListBufferData.PrevPolyFlags = PolyFlags;

    bool bInverseOrder = false;

	if ((GUglyHackFlags & HACKFLAGS_NoNearZ) && (StoredFovAngle != Viewport->Actor->FovAngle || StoredFX != Frame->FX || StoredFY != Frame->FY || !StoredbNearZ))
        SetProjection(Frame, 1);

    DrawGouraudListBufferData.PolyFlags = SetFlags(PolyFlags);
    SetBlend(DrawGouraudListBufferData.PolyFlags, bInverseOrder);
    DrawGouraudListBufferData.DrawFlags = DF_DiffuseTexture;
	SetTexture(0, Info, DrawGouraudListBufferData.PolyFlags, 0, GouraudPolyVertList_Prog, NORMALTEX);
	
    DrawGouraudListBufferData.TexNum[0] = TexInfo[0].TexNum;
	DrawGouraudListBufferData.Alpha = Info.Texture->Alpha;
	DrawGouraudListBufferData.TextureDiffuse = Info.Texture->Diffuse;
	DrawGouraudListBufferData.TexUMult = TexInfo[0].UMult;
	DrawGouraudListBufferData.TexVMult = TexInfo[0].VMult;

	if (GIsEditor)
	{
		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawGouraudListBufferData.RendMap = Frame->Viewport->Actor->RendMap;
	}

	FTextureInfo DetailTextureInfo;
	FTextureInfo BumpMapInfo;
	FTextureInfo MacroTextureInfo;

	if (Info.Texture)
    {
        DrawGouraudListBufferData.TextureFormat = (GLfloat)Info.Texture->Format;
        DrawGouraudListBufferData.TextureAlpha = (GLfloat)Info.Texture->Alpha;

        if (Info.Texture->DetailTexture && DetailTextures)
        {
            Info.Texture->DetailTexture->Lock(DetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudListBufferData.DrawFlags |= DF_DetailTexture;

            SetTexture(1, DetailTextureInfo, Info.Texture->DetailTexture->PolyFlags, 0.0, GouraudPolyVertList_Prog, DETAILTEX);
            DrawGouraudListBufferData.TexNum[1] = TexInfo[1].TexNum;

            DrawGouraudListBufferData.DetailTexUMult = TexInfo[1].UMult;
            DrawGouraudListBufferData.DetailTexVMult = TexInfo[1].VMult;
        }

        if (Info.Texture->BumpMap && BumpMaps)
        {
            Info.Texture->BumpMap->Lock(BumpMapInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudListBufferData.DrawFlags |= DF_BumpMap;

            SetTexture(2, BumpMapInfo, Info.Texture->BumpMap->PolyFlags, 0.0, GouraudPolyVertList_Prog, BUMPMAP);
            DrawGouraudListBufferData.TexNum[2] = TexInfo[2].TexNum; //using Base Texture UV.

            DrawGouraudListBufferData.BumpTextureSpecular = Info.Texture->BumpMap->Specular;
        }

        if (Info.Texture->MacroTexture && MacroTextures)
        {
            Info.Texture->MacroTexture->Lock(MacroTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudListBufferData.DrawFlags |= DF_MacroTexture;
			
            SetTexture(3, MacroTextureInfo, Info.Texture->MacroTexture->PolyFlags, 0.0, GouraudPolyVertList_Prog, MACROTEX);
            DrawGouraudListBufferData.TexNum[3] = TexInfo[3].TexNum;

            DrawGouraudListBufferData.MacroTexUMult = TexInfo[3].UMult;
            DrawGouraudListBufferData.MacroTexVMult = TexInfo[3].VMult;
        }
    }

	CHECK_GL_ERROR();
	//debugf(TEXT("PolyList: VertSize %i TexSize %i, ColorSize %i"), VertSize, TexSize, ColorSize); // VertSize 12288 TexSize 8192, ColorSize 16384

	clockFast(Stats.GouraudPolyListCycles);
    if (UsingPersistentBuffersGouraud && DrawGouraudListBufferRange.Sync[DrawGouraudListBufferData.Index])
        WaitBuffer(DrawGouraudListBufferRange, DrawGouraudListBufferData.Index);

    for (INT i = 0; i < NumPts; i++)
    {
        FTransTexture* P = &Pts[i];
        BufferGouraudPolygonPoint(&DrawGouraudListBufferRange.Buffer[DrawGouraudListBufferData.BeginOffset + DrawGouraudListBufferData.IndexOffset], P, DrawGouraudListBufferData);

		// stijn: this was the previous condition but this was all wrong! if we flush the buffer before the triangle we
		// were pushing is complete, all subsequent triangles we push will be borked as well!!
		//if ( DrawGouraudListBufferData.IndexOffset >= (DRAWGOURAUDPOLYLIST_SIZE - DrawGouraudStrideSize))
        if ( DrawGouraudListBufferData.IndexOffset >= (DRAWGOURAUDPOLYLIST_SIZE - 3 * DrawGouraudStrideSize) && i%3==2)
        {
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
            debugf(NAME_DevGraphics, TEXT("DrawGouraudPolyList overflow!"));
        }
    }

    if (NoBuffering)
        DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);

	if (DrawGouraudListBufferData.DrawFlags & DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(DetailTextureInfo);

	if (DrawGouraudListBufferData.DrawFlags & DF_BumpMap)
		Info.Texture->BumpMap->Unlock(BumpMapInfo);

    if (DrawGouraudListBufferData.DrawFlags & DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(MacroTextureInfo);

	unclockFast(Stats.GouraudPolyListCycles);
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

	// stijn: flush current polylist buffer even if it doesn't meet the criteria in DrawGouraudPolyList
	SetProgram(GouraudPolyVertList_Prog);

	if (Frame->NearClip.W != 0.0)
		PushClipPlane(Frame->NearClip);

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
		if (DrawGouraudListBufferData.VertSize > 0)
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
		PopClipPlane();
	}
}
#endif


void UXOpenGLRenderDevice::DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer& BufferData)
{
	// Using one huge buffer instead of 3, interleaved data.
	// This is to reduce overhead by reducing API calls.
	GLuint TotalSize = BufferData.IndexOffset;
	CHECK_GL_ERROR();

	checkSlow(ActiveProgram == GouraudPolyVertList_Prog || ActiveProgram == GouraudPolyVert_Prog);

	if(!UsingPersistentBuffersGouraud)
	{
	    if (UseBufferInvalidation)
        {
            if(ActiveProgram == GouraudPolyVertList_Prog)
                glInvalidateBufferData(DrawGouraudVertListBuffer);
            else if (ActiveProgram == GouraudPolyVert_Prog)
                glInvalidateBufferData(DrawGouraudVertBuffer);
            CHECK_GL_ERROR();
        }
#ifdef __LINUX_ARM__
            if (ActiveProgram == GouraudPolyVertList_Prog)
                glBufferData(GL_ARRAY_BUFFER, sizeof(float) * TotalSize, DrawGouraudListBufferRange.Buffer, GL_DYNAMIC_DRAW);
            else if (ActiveProgram == GouraudPolyVert_Prog)
                glBufferData(GL_ARRAY_BUFFER, TotalSize * sizeof(float), DrawGouraudBufferRange.Buffer, GL_DYNAMIC_DRAW);
#else
		if (ActiveProgram == GouraudPolyVertList_Prog)
			glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(float), DrawGouraudListBufferRange.Buffer);
		else if (ActiveProgram == GouraudPolyVert_Prog)
			glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(float), DrawGouraudBufferRange.Buffer);
#endif




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
		glVertexAttribPointer(VERTEX_COORD_ATTRIB,		3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)	BeginOffset);
		glVertexAttribPointer(TEXTURE_COORD_ATTRIB,		2, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3));
		glVertexAttribPointer(NORMALS_ATTRIB,			4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3_2));
		glVertexAttribPointer(COLOR_ATTRIB,				4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3_2_4));
		glVertexAttribPointer(FOGMAP_COORD_ATTRIB,		4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3_2_4_4));
		glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB,	3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3_2_4_4_4));
		glVertexAttribPointer(TEXTURE_COORD_ATTRIB2,	4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3_2_4_4_4_3));// Tex U/V Mult, MacroTex U/V Mult
		glVertexAttribPointer(TEXTURE_COORD_ATTRIB3,	4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset	+ FloatSize3_2_4_4_4_3_4));// DetailTex U/V Mult, Texture Format, DrawFlags
		glVertexAttribPointer(TEXTURE_ATTRIB,			4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (GLvoid*)(	BeginOffset + FloatSize3_2_4_4_4_3_4_4));// Additional texture information
		CHECK_GL_ERROR();

		PrevDrawGouraudBeginOffset = BeginOffset;
	}

    //PolyFlags
	glUniform1ui(DrawGouraudPolyFlags, BufferData.PolyFlags);

    // Gamma
    glUniform1f(DrawGouraudGamma, Gamma);
	CHECK_GL_ERROR();

	//set DrawColor if any
	if (GIsEditor)
	{
		if (HitTesting()) // UED only.
		{
			glUniform4f(DrawGouraudDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
			glUniform1i(DrawGouraudbHitTesting, true);
		}
		else
		{
			if (BufferData.Alpha > 0.f)
				glUniform4f(DrawGouraudDrawColor, 0.f, 0.f, 0.f, BufferData.Alpha);
			else glUniform4f(DrawGouraudDrawColor, BufferData.DrawColor.X, BufferData.DrawColor.Y, BufferData.DrawColor.Z, BufferData.DrawColor.W);
			glUniform1i(DrawGouraudbHitTesting, false);
		}
		glUniform1ui(DrawGouraudRendMap, BufferData.RendMap);
		CHECK_GL_ERROR();
	}

    CHECK_GL_ERROR();

	// Draw
	glDrawArrays(Mode, 0, (BufferData.VertSize / FloatsPerVertex));

	if(UsingPersistentBuffersGouraud)
	{
		if (ActiveProgram == GouraudPolyVertList_Prog)
		{
			BufferData.BeginOffset = BufferData.Index * DRAWGOURAUDPOLYLIST_SIZE;
			LockBuffer(DrawGouraudListBufferRange, BufferData.Index);
		}
		else if (ActiveProgram == GouraudPolyVert_Prog)
		{
			BufferData.BeginOffset = BufferData.Index * DRAWGOURAUDPOLY_SIZE;
			LockBuffer(DrawGouraudBufferRange, BufferData.Index);
		}
		BufferData.Index = (BufferData.Index + 1) % NUMBUFFERS;
		CHECK_GL_ERROR();
	}

    BufferData.VertSize = 0;
    BufferData.IndexOffset = 0;
    BufferData.PolyFlags = 0;
	BufferData.Alpha = 0.f;
}

//
// Program Switching
//
void UXOpenGLRenderDevice::DrawGouraudEnd(INT NextProgram)
{
	const bool GouraudToGouraudSwitch =
		(ActiveProgram == GouraudPolyVert_Prog && NextProgram == GouraudPolyVertList_Prog) ||
		(ActiveProgram == GouraudPolyVertList_Prog && NextProgram == GouraudPolyVert_Prog);

	if (ActiveProgram == GouraudPolyVert_Prog)
	{
		if (DrawGouraudBufferData.VertSize > 0)
		{
			if (DrawGouraudBufferRange.Sync[DrawGouraudBufferData.Index])
				WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
		}
	}
	else// if (ActiveProgram == GouraudPolyVertList_Prog)
	{
		if (DrawGouraudListBufferData.VertSize > 0)
		{
			// stijn: was this intentionally missing?
			// if (DrawGouraudListBufferRange.Sync[DrawGouraudListBufferData.Index])
			//     WaitBuffer(DrawGouraudListBufferRange, DrawGouraudListBufferData.Index);
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
		}
	}

	CHECK_GL_ERROR();

	if (!GouraudToGouraudSwitch)
	{
		// Clean up
		glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
		glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
		glDisableVertexAttribArray(NORMALS_ATTRIB);
		glDisableVertexAttribArray(COLOR_ATTRIB);
		glDisableVertexAttribArray(FOGMAP_COORD_ATTRIB);
		glDisableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
		glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
		glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
		glDisableVertexAttribArray(TEXTURE_ATTRIB);
		CHECK_GL_ERROR();
	}
}

void UXOpenGLRenderDevice::DrawGouraudStart()
{
	const bool GouraudToGouraudSwitch =
		(ActiveProgram == GouraudPolyVert_Prog && PrevProgram == GouraudPolyVertList_Prog) ||
		(ActiveProgram == GouraudPolyVertList_Prog && PrevProgram == GouraudPolyVert_Prog);

	if (!GouraudToGouraudSwitch)
		glUseProgram(DrawGouraudProg);

#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
	if (UseAA && NoAATiles && PrevProgram != ComplexSurfaceSinglePass_Prog && !GouraudToGouraudSwitch)
		glEnable(GL_MULTISAMPLE);
#endif

	if (ActiveProgram == GouraudPolyVert_Prog)
	{
		glBindVertexArray(DrawGouraudPolyVertsVao);
		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
	}
	else
	{
		glBindVertexArray(DrawGouraudPolyVertListSingleBufferVao);
		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertListBuffer);
	}

	// stijn: Mesa wants us to re-enable these after calling glBindVertexArray
    //	if (!GouraudToGouraudSwitch)
	{
		glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
		glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
		glEnableVertexAttribArray(NORMALS_ATTRIB);
		glEnableVertexAttribArray(COLOR_ATTRIB);
		glEnableVertexAttribArray(FOGMAP_COORD_ATTRIB);//here for VertexFogColor
		glEnableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
		glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
		glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
		glEnableVertexAttribArray(TEXTURE_ATTRIB);
	}

	PrevDrawGouraudBeginOffset = -1;

	CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

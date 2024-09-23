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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	Helpers
-----------------------------------------------------------------------------*/

static void BufferVert(UXOpenGLRenderDevice::DrawGouraudVertex* Vert, FTransTexture* P, glm::uint DrawID)
{
	Vert->Coords		= glm::vec3(P->Point.X, P->Point.Y, P->Point.Z);
	Vert->DrawID		= DrawID;
	Vert->Normals		= glm::vec4(P->Normal.X, P->Normal.Y, P->Normal.Z, 0.f);
	Vert->TexCoords     = glm::vec2(P->U, P->V);
	Vert->LightColor	= glm::vec4(P->Light.X, P->Light.Y, P->Light.Z, P->Light.W);
	Vert->FogColor		= glm::vec4(P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W);
}

static void SetTextureHelper
(
	UXOpenGLRenderDevice* RenDev, 
	INT Multi, 
	UTexture* Texture, 
	FSceneNode* Frame, 
	FTEXTURE_PTR& CachedInfo,
	glm::uvec4* TexHandles
)
{
#if XOPENGL_MODIFIED_LOCK
	 CachedInfo = Texture->GetTexture(INDEX_NONE, RenDev);
#else
	 Texture->Lock(CachedInfo, Frame->Viewport->CurrentTime, -1, RenDev);
#endif

	 RenDev->SetTexture(Multi, FTEXTURE_GET(CachedInfo), Texture->PolyFlags, 0.f);
	 StoreTexHandle(Multi, TexHandles, RenDev->TexInfo[Multi].BindlessTexHandle);
}

void UXOpenGLRenderDevice::PrepareGouraudCall(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags)
{
	auto Shader = dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);

	// Gather options
	DWORD Options = ShaderOptions::OPT_None;
	DWORD NextPolyFlags = GetPolyFlagsAndShaderOptions(PolyFlags, Options, FALSE);
	UBOOL NoNearZ = (GUglyHackFlags & HACKFLAGS_NoNearZ) == HACKFLAGS_NoNearZ;
	if (NoNearZ)
		Options |= ShaderOptions::OPT_NoNearZ;
	Options |= ShaderOptions::OPT_DiffuseTexture;

	if (Info.Texture && Info.Texture->DetailTexture && DetailTextures)
		Options |= ShaderOptions::OPT_DetailTexture;

	if (Info.Texture && Info.Texture->MacroTexture && MacroTextures)
		Options |= ShaderOptions::OPT_MacroTexture;

#if ENGINE_VERSION==227
	Options |= ShaderOptions::OPT_DistanceFog;
	if (Info.Texture && Info.Texture->BumpMap && BumpMaps)
		Options |= ShaderOptions::OPT_BumpMap;
#endif

	if (GIsEditor && NextPolyFlags & PF_Selected)
		Options |= ShaderOptions::OPT_Selected;

	Shader->SelectShaderSpecialization(Options);

	const bool CanBuffer = !Shader->DrawBuffer.IsFull() && Shader->ParametersBuffer.CanBuffer(1);

	// Check if the global state will change
	if (WillBlendStateChange(CurrentBlendPolyFlags, NextPolyFlags) || // Check if the blending mode will change
		WillTextureStateChange(0, Info, NextPolyFlags) || // Check if the texture will change
		StoredbNearZ != NoNearZ ||  // Force a flush if we're switching between NearZ and NoNearZ
		!CanBuffer // Check if we have room left in the multi-draw array
	)
	{
		// Dispatch buffered data
		Shader->Flush(!CanBuffer);

		SetBlend(NextPolyFlags);

		if (NoNearZ &&
			(StoredFovAngle != Frame->Viewport->Actor->FovAngle ||
				StoredFX != Frame->FX ||
				StoredFY != Frame->FY ||
				!StoredbNearZ))
		{
			SetProjection(Frame, 1);
		}
	}	
	
	DrawGouraudParameters* DrawCallParams = Shader->ParametersBuffer.GetCurrentElementPtr();

	const FLOAT TextureAlpha =
#if ENGINE_VERSION==227
		1.f;
#else
		(Info.Texture && Info.Texture->Alpha > 0.f) ? Info.Texture->Alpha : 1.f;
#endif
	
	DrawCallParams->DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : glm::vec4(0.f, 0.f, 0.f, TextureAlpha);

	SetTexture(DiffuseTextureIndex, Info, NextPolyFlags, 0.0);
	DrawCallParams->DiffuseInfo = glm::vec4(TexInfo[DiffuseTextureIndex].UMult, TexInfo[DiffuseTextureIndex].VMult, Info.Texture ? Info.Texture->Diffuse : 1.f, TextureAlpha);
	StoreTexHandle(DiffuseTextureIndex, DrawCallParams->TexHandles, TexInfo[DiffuseTextureIndex].BindlessTexHandle);

	DrawCallParams->DetailMacroInfo = glm::vec4(0.f, 0.f, 0.f, 0.f);
	if (Info.Texture && Info.Texture->DetailTexture && DetailTextures)
	{
		SetTextureHelper(this, DetailTextureIndex, Info.Texture->DetailTexture, Frame, Shader->DetailTextureInfo, DrawCallParams->TexHandles);
		DrawCallParams->DetailMacroInfo.x = TexInfo[DetailTextureIndex].UMult;
		DrawCallParams->DetailMacroInfo.y = TexInfo[DetailTextureIndex].VMult;
	}

	DrawCallParams->MiscInfo = glm::vec4(0.f, 0.f, 0.f, 0.f);
#if ENGINE_VERSION==227
	if (Info.Texture && Info.Texture->BumpMap && BumpMaps)
	{
		SetTextureHelper(this, BumpMapIndex, Info.Texture->BumpMap, Frame, Shader->BumpMapInfo, DrawCallParams->TexHandles);
		DrawCallParams->MiscInfo.x = Info.Texture->BumpMap->Specular;
	}
#endif

	if (Info.Texture && Info.Texture->MacroTexture && MacroTextures)
	{
		SetTextureHelper(this, MacroTextureIndex, Info.Texture->MacroTexture, Frame, Shader->MacroTextureInfo, DrawCallParams->TexHandles);
		DrawCallParams->DetailMacroInfo.z = TexInfo[MacroTextureIndex].UMult;
		DrawCallParams->DetailMacroInfo.w = TexInfo[MacroTextureIndex].VMult;
	}

#if ENGINE_VERSION==227
	DrawCallParams->DistanceFogColor = DistanceFogColor;
	DrawCallParams->DistanceFogInfo  = DistanceFogValues;
	DrawCallParams->DistanceFogMode = DistanceFogMode;
#else
	DrawCallParams->DistanceFogMode = -1;
#endif	
}

void UXOpenGLRenderDevice::FinishGouraudCall(FTextureInfo& Info)
{
#if !XOPENGL_MODIFIED_LOCK
	auto Shader = dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);
	if (Shader->LastOptions.HasOption(ShaderOptions::OPT_DetailTexture))
		Info.Texture->DetailTexture->Unlock(Shader->DetailTextureInfo);

	if (Shader->LastOptions.HasOption(ShaderOptions::OPT_BumpMap))
		Info.Texture->BumpMap->Unlock(Shader->BumpMapInfo);

	if (Shader->LastOptions.HasOption(ShaderOptions::OPT_MacroTexture))
		Info.Texture->MacroTexture->Unlock(Shader->MacroTextureInfo);
#endif
}

/*-----------------------------------------------------------------------------
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NoDrawGouraud)
		return;

	auto Shader = dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);

    STAT(clockFast(Stats.GouraudPolyCycles));
	SetProgram(Gouraud_Prog);

	if (NumPts < 3 /*|| Frame->Recursion > MAX_FRAME_RECURSION*/) //reject invalid.
		return;

	auto InVertexCount = NumPts - 2;
	auto OutVertexCount = InVertexCount * 3;

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i]->U, Pts[i]->V, UM, VM);
	}
#endif

	if (!Shader->VertBuffer.CanBuffer(OutVertexCount)) // we check the available capacity of the parameters and draw buffer elsewhere
	{
		Shader->Flush(true);

		// just in case...
		if (OutVertexCount >= Shader->VertexBufferSize)
		{
			GWarn->Logf(TEXT("DrawGouraudPolygon poly too big!"));
			return;
		}
	}

	PrepareGouraudCall(Frame, Info, PolyFlags);

	Shader->DrawBuffer.StartDrawCall();
	auto Out = Shader->VertBuffer.GetCurrentElementPtr();
	const auto DrawID = Shader->DrawBuffer.GetDrawID();

	// Unfan and buffer
	for (INT i = 0; i < InVertexCount; i++)
	{
		BufferVert(Out++, Pts[0    ], DrawID);
		BufferVert(Out++, Pts[i + 1], DrawID);
		BufferVert(Out++, Pts[i + 2], DrawID);
	}

	Shader->DrawBuffer.EndDrawCall(OutVertexCount);
	Shader->VertBuffer.Advance(OutVertexCount);
	Shader->ParametersBuffer.Advance(1);

	FinishGouraudCall(Info);
    STAT(unclockFast(Stats.GouraudPolyCycles));
	unguard;
}

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);

	if (NoDrawGouraudList)
		return;

	auto Shader = dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);

    STAT(clockFast(Stats.GouraudPolyCycles));
	SetProgram(Gouraud_Prog);

	if (NumPts < 3 /*|| Frame->Recursion > MAX_FRAME_RECURSION*/) //reject invalid.
		return;

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i].U, Pts[i].V, UM, VM);
	}
#endif

	PrepareGouraudCall(Frame, Info, PolyFlags);

	Shader->DrawBuffer.StartDrawCall();
	auto Out = Shader->VertBuffer.GetCurrentElementPtr();
	auto End = Shader->VertBuffer.GetLastElementPtr();
	auto DrawID = Shader->DrawBuffer.GetDrawID();

	INT PolyListSize = 0;
	for (INT i = 0; i < NumPts; i++)
	{
		// Polylists can be bigger than the vertex buffer so check here if we
		// need to split the mesh up into separate drawcalls
		if ((i % 3 == 0) && (Out + 2 > End))
		{
			Shader->DrawBuffer.EndDrawCall(PolyListSize);
			Shader->VertBuffer.Advance(PolyListSize);
			Shader->ParametersBuffer.Advance(1); // advance so Flush automatically restores the drawcall params of the _current_ drawcall

			Shader->Flush(true);
			//debugf(NAME_DevGraphics, TEXT("DrawGouraudPolyList overflow!"));

			Shader->DrawBuffer.StartDrawCall();
			Out = Shader->VertBuffer.GetCurrentElementPtr();
			End = Shader->VertBuffer.GetLastElementPtr();
			DrawID = Shader->DrawBuffer.GetDrawID();

			PolyListSize = 0;
		}

		BufferVert(Out++, &Pts[i], DrawID);
		PolyListSize++;
	}

	Shader->DrawBuffer.EndDrawCall(PolyListSize);
	Shader->VertBuffer.Advance(PolyListSize);
	Shader->ParametersBuffer.Advance(1);

	FinishGouraudCall(Info);
    STAT(unclockFast(Stats.GouraudPolyCycles));
	unguard;
}
#endif

// stijn: This is the UT extended renderer interface. This does not map directly onto DrawGouraudPolyList because DrawGouraudTriangles pushes info out earlier
#if UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudTriangles);

	if (NoDrawGouraudList)
		return;

    STAT(clockFast(Stats.GouraudPolyCycles));

	INT StartOffset = 0;
	INT i = 0;

	if (Frame->NearClip.W != 0.0)
		PushClipPlane(Frame->NearClip);

	for (; i < NumPts; i += 3)
	{
		if (Frame->Mirror == -1.0)
			Exchange(Pts[i + 2], Pts[i]);

		// Environment mapping.
		if (PolyFlags & PF_Environment)
		{
			FLOAT UScale = Info.UScale * Info.USize / 256.0f;
			FLOAT VScale = Info.VScale * Info.VSize / 256.0f;

			for (INT j = 0; j < 3; j++)
			{
				FVector T = Pts[i + j].Point.UnsafeNormal().MirrorByVector(Pts[i + j].Normal).TransformVectorBy(Frame->Uncoords);
				Pts[i + j].U = (T.X + 1.0f) * 0.5f * 256.0f * UScale;
				Pts[i + j].V = (T.Y + 1.0f) * 0.5f * 256.0f * VScale;
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
		if ((PolyFlags & PF_TwoSided) && FTriple(Pts[i].Point, Pts[i + 1].Point, Pts[i + 2].Point) <= 0.0)
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
			Exchange(Pts[i + 2], Pts[i]);
		}
	}

	// stijn: push the remaining triangles
	if (i - StartOffset > 0)
		DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);

	if (Frame->NearClip.W != 0.0)
		PopClipPlane();

    STAT(unclockFast(Stats.GouraudPolyCycles));
	unguard;
}
#endif

#if ENGINE_VERSION==227
void UXOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf)
{
	guard(UOpenGLRenderDevice::PreDrawGouraud);

	if (FogSurf.IsValid())
	{
		DistanceFogColor = glm::vec4(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, FogSurf.FogColor.W);
		DistanceFogValues = glm::vec4(FogSurf.FogDistanceStart, FogSurf.FogDistanceEnd, FogSurf.FogDensity, 0);
		DistanceFogMode = FogSurf.FogMode;

		bFogEnabled = true;
	}
	else if (bFogEnabled)
		ResetFog();

	unguard;
}

void UXOpenGLRenderDevice::PostDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf)
{
	guard(UOpenGLRenderDevice::PostDrawGouraud);
	ResetFog();
	unguard;
}
#endif // ENGINE_VERSION

/*-----------------------------------------------------------------------------
	Gouraud Mesh Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawGouraudProgram::DrawGouraudProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWGOURAUDPOLY_SIZE * 12;
	ParametersBufferSize			= DRAWGOURAUDPOLY_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::GouraudParametersIndex;
	NumTextureSamplers				= 6;
	DrawMode						= GL_TRIANGLES;
	NumVertexAttributes				= 6;
	UseSSBOParametersBuffer			= RenDev->UsingShaderDrawParameters;
	ParametersInfo					= DrawGouraudParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= RenDev->UsingGeometryShaders ? &BuildGeometryShader : nullptr; // optional
	FragmentShaderFunc				= &BuildFragmentShader;
}

void UXOpenGLRenderDevice::DrawGouraudProgram::CreateInputLayout()
{
	using Vert = DrawGouraudVertex;
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT,   sizeof(Vert), (GLvoid*)(offsetof(Vert, DrawID)));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, Normals)));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, TexCoords)));
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, LightColor)));
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, FogColor)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildCommonSpecializations()
{
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_Environment));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_Environment | ShaderOptions::OPT_Translucent));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_Modulated));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_Translucent));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_NoNearZ));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

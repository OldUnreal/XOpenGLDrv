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
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	if (NoDrawGouraud)
		return;

	clockFast(Stats.GouraudPolyCycles);
	SetProgram(Gouraud_Prog);
	dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog])->DrawGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags, Span);
	unclockFast(Stats.GouraudPolyCycles);
}

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	if (NoDrawGouraudList)
		return;

	clockFast(Stats.GouraudPolyCycles);
	SetProgram(Gouraud_Prog);
	dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog])->DrawGouraudPolyList(Frame, Info, Pts, NumPts, PolyFlags, Span);
	unclockFast(Stats.GouraudPolyCycles);
}
#endif

#if UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	if (NoDrawGouraudList)
		return;

	clockFast(Stats.GouraudPolyCycles);
	SetProgram(Gouraud_Prog);
	dynamic_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog])->DrawGouraudTriangles(Frame, Info, Pts, NumPts, PolyFlags, DataFlags, Span);
	unclockFast(Stats.GouraudPolyCycles);
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
	ShaderProgram Implementation
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawGouraudProgram::BufferVert(BufferedVert* Vert, FTransTexture* P)
{
	Vert->Point  = glm::vec3(P->Point.X, P->Point.Y, P->Point.Z);
	Vert->Normal = glm::vec3(P->Normal.X, P->Normal.Y, P->Normal.Z);
	Vert->UV     = glm::vec2(P->U, P->V);
	Vert->Light  = glm::vec4(P->Light.X, P->Light.Y, P->Light.Z, P->Light.W);
	Vert->Fog    = glm::vec4(P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W);
}

void UXOpenGLRenderDevice::DrawGouraudProgram::SetTexture(INT Multi, UTexture* Texture, DWORD DrawFlags, FSceneNode* Frame, FTEXTURE_PTR& CachedInfo)
{
#if XOPENGL_MODIFIED_LOCK
	 CachedInfo = Texture->GetTexture(INDEX_NONE, RenDev);
#else
	 Texture->Lock(CachedInfo, Frame->Viewport->CurrentTime, -1, RenDev);
#endif
	 DrawCallParams.DrawFlags |= DrawFlags;

	 RenDev->SetTexture(Multi, FTEXTURE_GET(CachedInfo), Texture->PolyFlags, 0.f, DrawFlags);
	 StoreTexHandle(Multi, DrawCallParams.TexHandles, RenDev->TexInfo[Multi].BindlessTexHandle);
}

void UXOpenGLRenderDevice::DrawGouraudProgram::PrepareDrawCall(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags)
{
	// stijn: this absolutely kills performance on mac. You're updating global state here for every gouraud mesh/complex surface!
#if ENGINE_VERSION==227 && !__APPLE__
    // Update FrameCoords for shaders.
    // Depending on HUD calls for SetSceneNode SetSceneNode is unreliable, so have to ensure proper updating here.
    // This is only essential (so far) for NormalMaps/Parallax/HWLighting, so this may be kept disabled for non 227 builds.
    // However, even in debugmode I can't see any ḿeasurable performance difference at all (on my machine).
    // In order to have any (future) calculations depending on FrameCoords working correctly you may want to enable this in general.
    if (RenDev->BumpMaps)
		RenDev->UpdateCoords(Frame);
#endif

	DWORD NextPolyFlags = SetPolyFlags(PolyFlags);
	UBOOL NoNearZ = (GUglyHackFlags & HACKFLAGS_NoNearZ) == HACKFLAGS_NoNearZ;
	DrawFlags NoNearZFlag = NoNearZ ? DF_NoNearZ : DF_None;

	// Check if the uniforms will change
	if (// force flush the buffer if we're rendering a mesh that is in zone 0 but shouldn't be...
		((DrawCallParams.PolyFlags^PolyFlags) & PF_ForceViewZone) ||
		// Check if the blending mode will change
		RenDev->WillBlendStateChange(DrawCallParams.PolyFlags, NextPolyFlags) ||
		// Check if the texture will change
		RenDev->WillTextureStateChange(0, Info, NextPolyFlags) ||
		// Check if NoNearZ will change
		((DrawCallParams.DrawFlags^NoNearZFlag) & DF_NoNearZ) ||
		// Check if we have room left in the multi-draw array
		DrawBuffer.IsFull())
	{
		// Dispatch buffered data
		Flush(true);

		RenDev->SetBlend(NextPolyFlags, false);

		if (NoNearZ &&
			(RenDev->StoredFovAngle != RenDev->Viewport->Actor->FovAngle ||
			 RenDev->StoredFX != Frame->FX ||
			 RenDev->StoredFY != Frame->FY ||
			 !RenDev->StoredbNearZ))
		{
			RenDev->SetProjection(Frame, 1);
		}
	}

	DrawCallParams.PolyFlags = NextPolyFlags;

	const FLOAT TextureAlpha =
#if ENGINE_VERSION==227
		1.f;
#else
		Info.Texture ? Info.Texture->Alpha : 1.f;
#endif

	DrawCallParams.HitTesting = GIsEditor && RenDev->HitTesting();	
	
	// Editor Support.
	if (GIsEditor)
	{
		DrawCallParams.DrawColor = DrawCallParams.HitTesting
			? FPlaneToVec4(RenDev->HitColor)
			: glm::vec4(0.f, 0.f, 0.f, TextureAlpha);		

		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawCallParams.RendMap = Frame->Viewport->Actor->RendMap;
	}

	RenDev->SetTexture(0, Info, DrawCallParams.PolyFlags, 0, DF_DiffuseTexture);
	DrawCallParams.DiffuseInfo = glm::vec4(RenDev->TexInfo[0].UMult, RenDev->TexInfo[0].VMult, Info.Texture ? Info.Texture->Diffuse : 1.f, TextureAlpha);
	DrawCallParams.DrawFlags = DF_DiffuseTexture | NoNearZFlag;
	StoreTexHandle(0, DrawCallParams.TexHandles, RenDev->TexInfo[0].BindlessTexHandle);

	DrawCallParams.DetailMacroInfo = glm::vec4(0.f, 0.f, 0.f, 0.f);
	if (Info.Texture && Info.Texture->DetailTexture && RenDev->DetailTextures)
	{
		SetTexture(1, Info.Texture->DetailTexture, DF_DetailTexture, Frame, DetailTextureInfo);
		DrawCallParams.DetailMacroInfo.x = RenDev->TexInfo[1].UMult;
		DrawCallParams.DetailMacroInfo.y = RenDev->TexInfo[1].VMult;
	}

	DrawCallParams.MiscInfo = glm::vec4(0.f, RenDev->GetViewportGamma(Frame->Viewport), 0.f, 0.f);
#if ENGINE_VERSION==227
	if (Info.Texture && Info.Texture->BumpMap && RenDev->BumpMaps)
	{
		SetTexture(2, Info.Texture->BumpMap, DF_BumpMap, Frame, BumpMapInfo);
		DrawCallParams.MiscInfo.x = Info.Texture->BumpMap->Specular;
	}
#endif

	if (Info.Texture && Info.Texture->MacroTexture && RenDev->MacroTextures)
	{
		SetTexture(3, Info.Texture->MacroTexture, DF_MacroTexture, Frame, MacroTextureInfo);
		DrawCallParams.DetailMacroInfo.z = RenDev->TexInfo[3].UMult;
		DrawCallParams.DetailMacroInfo.w = RenDev->TexInfo[3].VMult;
	}

	DrawCallParams.DistanceFogColor = RenDev->DistanceFogColor;
	DrawCallParams.DistanceFogInfo  = RenDev->DistanceFogValues;
	DrawCallParams.DistanceFogMode = RenDev->DistanceFogMode;
}

void UXOpenGLRenderDevice::DrawGouraudProgram::FinishDrawCall(FTextureInfo& Info)
{
	if (RenDev->NoBuffering)
		Flush(true);

#if !XOPENGL_MODIFIED_LOCK
	if (DrawCallParams.DrawFlags & DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(DetailTextureInfo);

	if (DrawCallParams.DrawFlags & DF_BumpMap)
		Info.Texture->BumpMap->Unlock(BumpMapInfo);

	if (DrawCallParams.DrawFlags & DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(MacroTextureInfo);
#endif
}

void UXOpenGLRenderDevice::DrawGouraudProgram::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3 /*|| Frame->Recursion > MAX_FRAME_RECURSION*/ ) //reject invalid.
		return;

	auto InVertexCount  = NumPts - 2;
	auto OutVertexCount = InVertexCount * 3;

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i]->U, Pts[i]->V, UM, VM);
	}
#endif

	if (!VertBuffer.CanBuffer(OutVertexCount))
	{
		Flush(true);

		// just in case...
		if (sizeof(BufferedVert) * OutVertexCount >= DRAWGOURAUDPOLY_SIZE)
		{
			GWarn->Logf(TEXT("DrawGouraudPolygon poly too big!"));
			return;
		}
	}

	PrepareDrawCall(Frame, Info, PolyFlags);
	if (RenDev->UsingShaderDrawParameters)
		memcpy(ParametersBuffer.GetCurrentElementPtr(), &DrawCallParams, sizeof(DrawCallParams));
	
	DrawBuffer.StartDrawCall();
	auto Out = VertBuffer.GetCurrentElementPtr();

	// Unfan and buffer
	for ( INT i=0; i < InVertexCount; i++ )
    {
        BufferVert(Out++, Pts[0    ]);
		BufferVert(Out++, Pts[i + 1]);
		BufferVert(Out++, Pts[i + 2]);
    }

	DrawBuffer.EndDrawCall(OutVertexCount);
	VertBuffer.Advance(OutVertexCount);
	if (RenDev->UsingShaderDrawParameters)
		ParametersBuffer.Advance(1);

	FinishDrawCall(Info);

	unguard;
}

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudProgram::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);
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

	PrepareDrawCall(Frame, Info, PolyFlags);

	if (RenDev->UsingShaderDrawParameters)
		memcpy(ParametersBuffer.GetCurrentElementPtr(), &DrawCallParams, sizeof(DrawCallParams));

	DrawBuffer.StartDrawCall();
	auto Out = VertBuffer.GetCurrentElementPtr();
	auto End = VertBuffer.GetLastElementPtr();

	INT PolyListSize = 0;
    for (INT i = 0; i < NumPts; i++)
    {
		// Polylists can be bigger than the vertex buffer so check here if we
		// need to split the mesh up into separate drawcalls
		if ((i % 3 == 0) && (Out + 2 > End))
		{
			DrawBuffer.EndDrawCall(PolyListSize);
			VertBuffer.Advance(PolyListSize);
			if (RenDev->UsingShaderDrawParameters)
				ParametersBuffer.Advance(1);
			
			Flush(true);
			debugf(NAME_DevGraphics, TEXT("DrawGouraudPolyList overflow!"));

			Out = VertBuffer.GetCurrentElementPtr();
			End = VertBuffer.GetLastElementPtr();

			if (RenDev->UsingShaderDrawParameters)
				memcpy(ParametersBuffer.GetCurrentElementPtr(), &DrawCallParams, sizeof(DrawCallParams));

			PolyListSize = 0;
		}

		BufferVert(Out++, &Pts[i]);
		PolyListSize++;
    }

	DrawBuffer.EndDrawCall(PolyListSize);
	VertBuffer.Advance(PolyListSize);
	if (RenDev->UsingShaderDrawParameters)
		ParametersBuffer.Advance(1);
	
	FinishDrawCall(Info);
	unguard;
}
#endif

// stijn: This is the UT extended renderer interface. This does not map directly onto DrawGouraudPolyList because DrawGouraudTriangles pushes info out earlier
#if UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudProgram::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	INT StartOffset = 0;
	INT i = 0;

	if (Frame->NearClip.W != 0.0)
	{
		Flush(true);
		RenDev->PushClipPlane(Frame->NearClip);
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
		Flush(true);
		RenDev->PopClipPlane();
	}
}
#endif

void UXOpenGLRenderDevice::DrawGouraudProgram::Flush(bool Wait)
{
	if (VertBuffer.Size() == 0)
		return;

	VertBuffer.BufferData(RenDev->UseBufferInvalidation, true, GL_STREAM_DRAW);

	if (!RenDev->UsingShaderDrawParameters)
	{
		auto Out = ParametersBuffer.GetElementPtr(0);
		memcpy(Out, &DrawCallParams, sizeof(DrawCallParameters));
	}

	// We might have to rebind the parametersbuffer here because
	// PushClipPlane/PopClipPlane can also bind GL_UNIFORM_BUFFER
	ParametersBuffer.Bind();
	ParametersBuffer.BufferData(RenDev->UseBufferInvalidation, false, DRAWCALL_BUFFER_USAGE_PATTERN);

	if (!VertBuffer.IsInputLayoutCreated())
		CreateInputLayout();

	DrawBuffer.Draw(GL_TRIANGLES, RenDev);

	if (RenDev->UsingShaderDrawParameters)
		ParametersBuffer.Rotate(false);

	VertBuffer.Lock();
	VertBuffer.Rotate(Wait);

	DrawBuffer.Reset(
		VertBuffer.BeginOffsetBytes() / sizeof(BufferedVert),
		ParametersBuffer.BeginOffsetBytes() / sizeof(DrawCallParameters));
}

void UXOpenGLRenderDevice::DrawGouraudProgram::CreateInputLayout()
{
	using Vert = BufferedVert;
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(0));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, Normal)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, UV)));
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, Light)));
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, Fog)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawGouraudProgram::DeactivateShader()
{
	Flush(false);

	for (INT i = 0; i < 5; ++i)
		glDisableVertexAttribArray(i);
}

void UXOpenGLRenderDevice::DrawGouraudProgram::ActivateShader()
{
	VertBuffer.Wait();
	
	glUseProgram(ShaderProgramObject);

	VertBuffer.Bind();
	ParametersBuffer.Bind();

	for (INT i = 0; i < 5; ++i)
		glEnableVertexAttribArray(i);

	DrawCallParams.PolyFlags = 0;// SetPolyFlags(CurrentAdditionalPolyFlags | CurrentPolyFlags);
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BindShaderState()
{
	ShaderProgram::BindShaderState();

	if (!RenDev->UsingShaderDrawParameters)
		BindUniform(GouraudParametersIndex, "DrawCallParameters");

	// Bind regular texture samplers to their respective TMUs
	for (INT i = 0; i < 4; i++)
	{
		GLint MultiTextureUniform;
		GetUniformLocation(MultiTextureUniform, appToAnsi(*FString::Printf(TEXT("Texture%i"), i)));
		if (MultiTextureUniform != -1)
			glUniform1i(MultiTextureUniform, i);
	}
}

void UXOpenGLRenderDevice::DrawGouraudProgram::MapBuffers()
{
	VertBuffer.GenerateVertexBuffer(RenDev);
	VertBuffer.MapVertexBuffer(RenDev->UsingPersistentBuffersGouraud, DRAWGOURAUDPOLY_SIZE);

	if (RenDev->UsingShaderDrawParameters)
	{
		ParametersBuffer.GenerateSSBOBuffer(RenDev, GouraudParametersIndex);
		ParametersBuffer.MapSSBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, MAX_DRAWGOURAUD_BATCH, DRAWCALL_BUFFER_USAGE_PATTERN);
	}
	else
	{
		ParametersBuffer.GenerateUBOBuffer(RenDev, GouraudParametersIndex);
		ParametersBuffer.MapUBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, 1, DRAWCALL_BUFFER_USAGE_PATTERN);
		ParametersBuffer.Advance(1);
	}
}

void UXOpenGLRenderDevice::DrawGouraudProgram::UnmapBuffers()
{
	VertBuffer.DeleteBuffer();
	ParametersBuffer.DeleteBuffer();
}

bool UXOpenGLRenderDevice::DrawGouraudProgram::BuildShaderProgram()
{
	return ShaderProgram::BuildShaderProgram(
		BuildVertexShader,
		RenDev->UsingGeometryShaders ? BuildGeometryShader : nullptr,
		BuildFragmentShader,
		EmitHeader);
	}

UXOpenGLRenderDevice::DrawGouraudProgram::~DrawGouraudProgram()
{
	DeleteShader();
	DrawGouraudProgram::UnmapBuffers();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

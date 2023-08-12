/*=============================================================================
	DrawTile.cpp: Unreal XOpenGL for DrawTile routines.
	Used f.e. for HUD drawing.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch
        * Added support for bindless textures
        * removed some blending changes (PF_TwoSided PF_Unlit)
        * Added batching
=============================================================================*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	if (NoDrawTile)
		return;

	clockFast(Stats.TileBufferCycles);
	SetProgram(Tile_Prog);
	dynamic_cast<DrawTileProgram*>(Shaders[Tile_Prog])->DrawTile(Frame, Info, X, Y, XL, YL, U, V, UL, VL, Span, Z, Color, Fog, PolyFlags);
	unclockFast(Stats.TileBufferCycles);
}

/*-----------------------------------------------------------------------------
    ShaderProgram Implementation
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawTileProgram::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UXOpenGLRenderDevice::DrawTile);

	/*
	From SoftDrv DrawTile, no idea if ever of use here:
	- PF_Renderhint indicates nonscaled sprites, but a lot of calls without that flag set
	are also unscaled -> therefore currently we use explicit detection.
	// If PolyFlags & PF_RenderHint, this will be a non-scaled sprite (ie font character, hud icon).
	// 1:1 scaling detector
	if ( EqualPositiveFloat(XL,UL) && EqualPositiveFloat(YL,VL) ) PolyFlags |= PF_RenderHint;

	Set by WrappedPrint and execDrawTextClipped only.
	*/

	// Calculate new drawcall parameters
#if ENGINE_VERSION==227
    if (PolyFlags & (PF_AlphaBlend | PF_Translucent | PF_Modulated | PF_Highlighted)) // Make sure occlusion is correctly set.
#else
    if (PolyFlags & (PF_Translucent | PF_Modulated))
#endif
        PolyFlags &= ~PF_Occlude;
    else PolyFlags |= PF_Occlude;

	PolyFlags &= ~(PF_RenderHint | PF_Unlit); // Using PF_RenderHint internally for CW/CCW switch.

    DWORD NextPolyFlags = SetPolyFlags(PolyFlags);

	BOOL bHitTesting = GIsEditor && RenDev->HitTesting();

	// Hack to render HUD on top of everything in 469
#if UNREAL_TOURNAMENT_OLDUNREAL
    UBOOL ShouldDepthTest = ((GUglyHackFlags & HACKFLAGS_PostRender) == 0) || Abs(1.f - Z) > SMALL_NUMBER;
#endif

	// Color
	if (PolyFlags & PF_Modulated)
	{
		Color.X = 1.0f;
		Color.Y = 1.0f;
		Color.Z = 1.0f;
		Color.W = 1.0f;
	}
#if ENGINE_VERSION==227
	else if (!(PolyFlags & PF_AlphaBlend))
		Color.W = 1.0f;
#else
    if (Info.Texture && Info.Texture->Alpha > 0.f)
        Color.W = Info.Texture->Alpha;
	else Color.W = 1.0f;
#endif

	glm::vec4 DrawColor = FPlaneToVec4(Color);
	const auto CanBuffer = 
		(RenDev->OpenGLVersion == GL_Core ? VertBufferCore.CanBuffer(3) : VertBufferES.CanBuffer(6)) &&
		(!RenDev->UsingShaderDrawParameters || ParametersBuffer.CanBuffer(1)) &&
		!DrawBuffer.IsFull();

	// Check if the draw call parameters will change
	if ((DrawCallParams.DrawColor != DrawColor || DrawCallParams.PolyFlags != NextPolyFlags || DrawCallParams.HitTesting != bHitTesting) ||
		// Check if global GL state will change
        WillBlendStateChange(DrawCallParams.BlendPolyFlags, PolyFlags) || // orig polyflags here as intended!
		// Check if bound sampler state will change
        RenDev->WillTextureStateChange(0, Info, PolyFlags) ||
        // Check if we have space to batch more data
        !CanBuffer
#if UNREAL_TOURNAMENT_OLDUNREAL
        // Check if the depth testing mode will change
        || ShouldDepthTest != DrawCallParams.DepthTested
#endif
        )
	{
		Flush(true);

		// Set new GL state
		DrawCallParams.BlendPolyFlags = PolyFlags;
        RenDev->SetBlend(PolyFlags, false); // yes, we use the original polyflags here!

#if UNREAL_TOURNAMENT_OLDUNREAL
		if (ShouldDepthTest)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		DrawCallParams.DepthTested = ShouldDepthTest;
#endif
    }

	// Bind texture or fetch its bindless handle
    RenDev->SetTexture(0, Info, PolyFlags, 0, DF_DiffuseTexture);

    // Buffer new drawcall parameters
	const auto& TexInfo = RenDev->TexInfo[0];
	DrawCallParams.DrawColor	= DrawColor;
	DrawCallParams.HitColor		= FPlaneToVec4(RenDev->HitColor);
	DrawCallParams.PolyFlags	= NextPolyFlags;
	DrawCallParams.HitTesting	= bHitTesting;
	DrawCallParams.Gamma		= RenDev->GetViewportGamma(Frame->Viewport);
	StoreTexHandle(0, DrawCallParams.TexHandles, TexInfo.BindlessTexHandle);
	
	if (RenDev->UsingShaderDrawParameters)
		memcpy(ParametersBuffer.GetCurrentElementPtr(), &DrawCallParams, sizeof(DrawCallParameters));
	
	if (GIsEditor && 
		Frame->Viewport->Actor && 
		(Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.0f; // Probably just needed because projection done below assumes non ortho projection.
	}

    // Buffer the tile
	DrawBuffer.StartDrawCall();
	if (VertBufferES.IsBound())
	{
		// ES doesn't have geo shaders so we manually emit two triangles here
		auto Out = VertBufferES.GetCurrentElementPtr();

        // Vertex 0
		Out[0].Point = glm::vec3(RenDev->RFX2*Z*(X - Frame->FX2), RenDev->RFY2*Z*(Y - Frame->FY2), Z);
		Out[0].UV    = glm::vec2((U)*TexInfo.UMult, (V)*TexInfo.VMult);

        // Vertex 1
		Out[1].Point = glm::vec3(RenDev->RFX2*Z*(X + XL - Frame->FX2), RenDev->RFY2*Z*(Y - Frame->FY2), Z);
		Out[1].UV    = glm::vec2((U + UL)*TexInfo.UMult, (V)*TexInfo.VMult);

        // Vertex 2
		Out[2].Point = glm::vec3(RenDev->RFX2*Z*(X + XL - Frame->FX2), RenDev->RFY2*Z*(Y + YL - Frame->FY2), Z);
		Out[2].UV    = glm::vec2((U + UL)*TexInfo.UMult, (V + VL)*TexInfo.VMult);

        // Vertex 0
		Out[3].Point = glm::vec3(RenDev->RFX2*Z*(X - Frame->FX2), RenDev->RFY2*Z*(Y - Frame->FY2), Z);
		Out[3].UV    = glm::vec2((U)*TexInfo.UMult, (V)*TexInfo.VMult);

        // Vertex 2
		Out[4].Point = glm::vec3(RenDev->RFX2*Z*(X + XL - Frame->FX2), RenDev->RFY2*Z*(Y + YL - Frame->FY2), Z);
		Out[4].UV    = glm::vec2((U + UL)*TexInfo.UMult, (V + VL)*TexInfo.VMult);

        // Vertex 3
		Out[5].Point = glm::vec3(RenDev->RFX2*Z*(X - Frame->FX2), RenDev->RFY2*Z*(Y + YL - Frame->FY2), Z);
		Out[5].UV    = glm::vec2((U)*TexInfo.UMult, (V + VL)*TexInfo.VMult);

#if ENGINE_VERSION==227
        if (Info.Modifier)
        {
			for (INT i = 0; i < 6; ++i)
				Info.Modifier->TransformPoint(Out[i].UV.x, Out[i].UV.y);
		}
#endif

		VertBufferES.Advance(6);
		DrawBuffer.EndDrawCall(6);
	}
	else
	{
		// Our Core geo shader emits the triangles. We only need to pass the tile origin and dimensions
		auto Out = VertBufferCore.GetCurrentElementPtr();

		Out->Point = glm::vec3(X, Y, Z);
		Out->TexCoords0 = glm::vec4(RenDev->RFX2, RenDev->RFY2, Frame->FX2, Frame->FY2);
		Out->TexCoords1 = glm::vec4(U, V, UL, VL);
		Out->TexCoords2 = glm::vec4(XL, YL, TexInfo.UMult, TexInfo.VMult);

#if ENGINE_VERSION==227 && 0 // TODO - Need to push 2x3 matrix transformation to UV mapping.
        if (Info.Modifier)
        {
        }
#endif

		VertBufferCore.Advance(3);
		DrawBuffer.EndDrawCall(3);
	}

	if (RenDev->UsingShaderDrawParameters)
		ParametersBuffer.Advance(1);

	if (RenDev->NoBuffering)
		Flush(true);

	unguard;
}

void UXOpenGLRenderDevice::DrawTileProgram::Flush(bool Wait)
{
	if (VertBufferES.Size() == 0 && VertBufferCore.Size() == 0)
		return;

    if (VertBufferES.IsBound())
	{
		VertBufferES.BufferData(RenDev->UseBufferInvalidation, false, GL_STREAM_DRAW);

		if (!VertBufferES.IsInputLayoutCreated())
			CreateInputLayout();
	}
	else
	{
		VertBufferCore.BufferData(RenDev->UseBufferInvalidation, false, GL_STREAM_DRAW);

		if (!VertBufferCore.IsInputLayoutCreated())
			CreateInputLayout();
	}

	// Push drawcall parameters
	if (!RenDev->UsingShaderDrawParameters)
	{
		auto Out = ParametersBuffer.GetElementPtr(0);
		memcpy(Out, &DrawCallParams, sizeof(DrawCallParams));
	}
	ParametersBuffer.Bind();
	ParametersBuffer.BufferData(RenDev->UseBufferInvalidation, false, DRAWCALL_BUFFER_USAGE_PATTERN);
		
	if (VertBufferES.IsBound())
	{
		DrawBuffer.Draw(GL_TRIANGLES, RenDev);

		if (RenDev->UsingShaderDrawParameters)
			ParametersBuffer.Rotate(false);

		VertBufferES.Lock();
		VertBufferES.Rotate(Wait);
	}
	else
	{
		DrawBuffer.Draw(GL_TRIANGLES, RenDev);

		if (RenDev->UsingShaderDrawParameters)
			ParametersBuffer.Rotate(false);

		VertBufferCore.Lock();
		VertBufferCore.Rotate(Wait);
	}

	DrawBuffer.Reset(
		VertBufferCore.BeginOffsetBytes() / sizeof(BufferedVertCore) + VertBufferES.BeginOffsetBytes() / sizeof(BufferedVertES),
		ParametersBuffer.BeginOffsetBytes() / sizeof(DrawCallParameters));
}

void UXOpenGLRenderDevice::DrawTileProgram::CreateInputLayout()
{
	if (VertBufferES.IsBound())
	{
		using Vert = BufferedVertES;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(0));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, UV)));
		VertBufferES.SetInputLayoutCreated();
	}
	else
	{
		using Vert = BufferedVertCore;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(0));
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, TexCoords0)));
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, TexCoords1)));
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, TexCoords2)));
		VertBufferCore.SetInputLayoutCreated();
	}
}

void UXOpenGLRenderDevice::DrawTileProgram::DeactivateShader()
{
	Flush(false);

	for (INT i = 0; i < (RenDev->UsingGeometryShaders ? 4 : 2); ++i)
		glDisableVertexAttribArray(i);

#if UNREAL_TOURNAMENT_OLDUNREAL
    glEnable(GL_DEPTH_TEST);
#endif

	if (RenDev->UseAA && RenDev->NoAATiles)
		glEnable(GL_MULTISAMPLE);
}

void UXOpenGLRenderDevice::DrawTileProgram::ActivateShader()
{
#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
    if (RenDev->UseAA && RenDev->NoAATiles)
        glDisable(GL_MULTISAMPLE);
#endif

#if UNREAL_TOURNAMENT_OLDUNREAL
    if (GUglyHackFlags & HACKFLAGS_PostRender) // ugly hack to make the HUD always render on top of weapons
        glDisable(GL_DEPTH_TEST);
#endif

    glUseProgram(ShaderProgramObject);
	if (!RenDev->UsingGeometryShaders)
	{
		VertBufferES.Bind();
		VertBufferES.Wait();
		for (INT i = 0; i < 2; ++i)
			glEnableVertexAttribArray(i);
	}
	else
	{
		VertBufferCore.Bind();
		VertBufferCore.Wait();
		for (INT i = 0; i < 4; ++i)
			glEnableVertexAttribArray(i);
	}

	ParametersBuffer.Bind();
	DrawCallParams.BlendPolyFlags = 0;// RenDev->CurrentPolyFlags | RenDev->CurrentAdditionalPolyFlags;
	DrawCallParams.PolyFlags = 0;
}

void UXOpenGLRenderDevice::DrawTileProgram::BindShaderState()	
{
	ShaderProgram::BindShaderState();

	if (!RenDev->UsingShaderDrawParameters)
		BindUniform(TileParametersIndex, "DrawCallParameters");

	GLint Texture;
	GetUniformLocation(Texture, "Texture0");
	if (Texture != -1)
		glUniform1i(Texture, 0);
}

void UXOpenGLRenderDevice::DrawTileProgram::MapBuffers()
{
	if (!RenDev->UsingGeometryShaders)
	{
		VertBufferES.GenerateVertexBuffer(RenDev);
		VertBufferES.MapVertexBuffer(RenDev->UsingPersistentBuffersTile, DRAWTILE_SIZE);
	}
	else
	{
		VertBufferCore.GenerateVertexBuffer(RenDev);
		VertBufferCore.MapVertexBuffer(RenDev->UsingPersistentBuffersTile, DRAWTILE_SIZE);
	}

	if (RenDev->UsingShaderDrawParameters)
	{
		ParametersBuffer.GenerateSSBOBuffer(RenDev, TileParametersIndex);
		ParametersBuffer.MapSSBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, MAX_DRAWTILE_BATCH, DRAWCALL_BUFFER_USAGE_PATTERN);
	}
	else
	{
		ParametersBuffer.GenerateUBOBuffer(RenDev, TileParametersIndex);
		ParametersBuffer.MapUBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, 1, DRAWCALL_BUFFER_USAGE_PATTERN);
		ParametersBuffer.Advance(1);
	}
}

void UXOpenGLRenderDevice::DrawTileProgram::UnmapBuffers()
{
	if (!RenDev->UsingGeometryShaders)
		VertBufferES.DeleteBuffer();
	else
		VertBufferCore.DeleteBuffer();

	ParametersBuffer.DeleteBuffer();
}

bool UXOpenGLRenderDevice::DrawTileProgram::BuildShaderProgram()
{
	return ShaderProgram::BuildShaderProgram(
		BuildVertexShader,
		RenDev->UsingGeometryShaders ? BuildGeometryShader : nullptr,
		BuildFragmentShader, 
		EmitHeader);
}

UXOpenGLRenderDevice::DrawTileProgram::~DrawTileProgram()
{
	DeleteShader();
	DrawTileProgram::UnmapBuffers();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

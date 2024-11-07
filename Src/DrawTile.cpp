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

    STAT(clockFast(Stats.TileBufferCycles));
	SetProgram(Tile_Prog);

	auto ShaderCore = dynamic_cast<DrawTileCoreProgram*>(Shaders[Tile_Prog]);
	auto ShaderES   = dynamic_cast<DrawTileESProgram*>  (Shaders[Tile_Prog]);
	
	DWORD Options = ShaderOptions::OPT_None;
	DWORD NextPolyFlags = GetPolyFlagsAndShaderOptions(PolyFlags, Options, TRUE);
	Options |= ShaderOptions::OPT_DiffuseTexture;
	PolyFlags &= ~(PF_RenderHint | PF_Unlit); // Using PF_RenderHint internally for CW/CCW switch.

	if (GIsEditor && NextPolyFlags & PF_Selected)
		Options |= ShaderOptions::OPT_Selected;

	// Hack to render HUD on top of everything in 469
#if UNREAL_TOURNAMENT_OLDUNREAL
	BOOL ShouldDepthTest = ((GUglyHackFlags & HACKFLAGS_PostRender) == 0) || Abs(1.f - Z) > SMALL_NUMBER;
	BOOL& DepthTesting = ShaderCore ? ShaderCore->DepthTesting : ShaderES->DepthTesting;
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

	glm::vec4 DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : FPlaneToVec4(Color);
	bool CanBuffer = false;
	DrawTileParameters* DrawCallParams = nullptr;

	if (ShaderCore)
	{
		CanBuffer = ShaderCore->VertBuffer.CanBuffer(3) && 
			ShaderCore->ParametersBuffer.CanBuffer(1) &&
			!ShaderCore->DrawBuffer.IsFull();
		ShaderCore->SelectShaderSpecialization(ShaderOptions(Options));
		DrawCallParams = ShaderCore->ParametersBuffer.GetCurrentElementPtr();
	}
	else
	{
		CanBuffer = ShaderES->VertBuffer.CanBuffer(6) &&
			ShaderES->ParametersBuffer.CanBuffer(1) &&
			!ShaderES->DrawBuffer.IsFull();
		ShaderES->SelectShaderSpecialization(ShaderOptions(Options));
		DrawCallParams = ShaderES->ParametersBuffer.GetCurrentElementPtr();
	}	

	// Check if global GL state will change
	if (WillBlendStateChange(CurrentBlendPolyFlags, PolyFlags) || 
		// Check if bound sampler state will change
		WillTextureStateChange(0, Info, NextPolyFlags) ||
		// Check if we have space to batch more data
		!CanBuffer
#if UNREAL_TOURNAMENT_OLDUNREAL
		// Check if the depth testing mode will change
		|| ShouldDepthTest != DepthTesting
#endif
		)
	{
		if (ShaderCore)
		{
			ShaderCore->Flush(!CanBuffer);
			DrawCallParams = ShaderCore->ParametersBuffer.GetCurrentElementPtr();
		}
		else
		{
			ShaderES->Flush(!CanBuffer);
			DrawCallParams = ShaderES->ParametersBuffer.GetCurrentElementPtr();
		}

		// Set new GL state
		SetBlend(PolyFlags); // yes, we use the original polyflags here!

#if UNREAL_TOURNAMENT_OLDUNREAL
		if (DepthTesting != ShouldDepthTest)
		{
			if (ShouldDepthTest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
			DepthTesting = ShouldDepthTest;
		}
#endif
	}

	// Bind texture or fetch its bindless handle
	SetTexture(DiffuseTextureIndex, Info, PolyFlags, 0);	

	// Buffer new drawcall parameters
	const auto& TexInfo = this->TexInfo[DiffuseTextureIndex];
	DrawCallParams->DrawColor = DrawColor;
	StoreTexHandle(DiffuseTextureIndex, DrawCallParams->TexHandles, TexInfo.BindlessTexHandle);

	if (GIsEditor &&
		Frame->Viewport->Actor &&
		(Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.0f; // Probably just needed because projection done below assumes non ortho projection.
	}

	// Buffer the tile
	if (ShaderES)
	{
		ShaderES->DrawBuffer.StartDrawCall();
		// ES doesn't have geo shaders so we manually emit two triangles here
		auto Out = ShaderES->VertBuffer.GetCurrentElementPtr();
		const auto DrawID = ShaderES->DrawBuffer.GetDrawID();

		// Vertex 0
		Out[0].Coords = glm::vec3(RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z);
		Out[0].DrawID = DrawID;
		Out[0].TexCoords = glm::vec2((U)*TexInfo.UMult, (V)*TexInfo.VMult);

		// Vertex 1
		Out[1].Coords = glm::vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z);
		Out[1].DrawID = DrawID;
		Out[1].TexCoords = glm::vec2((U + UL) * TexInfo.UMult, (V)*TexInfo.VMult);

		// Vertex 2
		Out[2].Coords = glm::vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z);
		Out[2].DrawID = DrawID;
		Out[2].TexCoords = glm::vec2((U + UL) * TexInfo.UMult, (V + VL) * TexInfo.VMult);

		// Vertex 0
		Out[3].Coords = glm::vec3(RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z);
		Out[3].DrawID = DrawID;
		Out[3].TexCoords = glm::vec2((U)*TexInfo.UMult, (V)*TexInfo.VMult);

		// Vertex 2
		Out[4].Coords = glm::vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z);
		Out[4].DrawID = DrawID;
		Out[4].TexCoords = glm::vec2((U + UL) * TexInfo.UMult, (V + VL) * TexInfo.VMult);

		// Vertex 3
		Out[5].Coords = glm::vec3(RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z);
		Out[5].DrawID = DrawID;
		Out[5].TexCoords = glm::vec2((U)*TexInfo.UMult, (V + VL) * TexInfo.VMult);

#if ENGINE_VERSION==227
		if (Info.Modifier)
		{
			for (INT i = 0; i < 6; ++i)
				Info.Modifier->TransformPoint(Out[i].TexCoords.x, Out[i].TexCoords.y);
		}
#endif

		ShaderES->VertBuffer.Advance(6);
		ShaderES->DrawBuffer.EndDrawCall(6);
		ShaderES->ParametersBuffer.Advance(1);
	}
	else
	{
		ShaderCore->DrawBuffer.StartDrawCall();
		// Our Core geo shader emits the triangles. We only need to pass the tile origin and dimensions
		auto Out = ShaderCore->VertBuffer.GetCurrentElementPtr();
		const auto DrawID = ShaderCore->DrawBuffer.GetDrawID();

		Out->Coords = glm::vec3(X, Y, Z);
		Out->TexCoords0 = glm::vec4(RFX2, RFY2, Frame->FX2, Frame->FY2);
		Out->TexCoords1 = glm::vec4(U, V, UL, VL);
		Out->TexCoords2 = glm::vec4(XL, YL, TexInfo.UMult, TexInfo.VMult);
		(Out++)->DrawID = DrawID;
		(Out++)->DrawID = DrawID;
		Out->DrawID = DrawID;

#if ENGINE_VERSION==227 && 0 // TODO - Need to push 2x3 matrix transformation to UV mapping.
		if (Info.Modifier)
		{
		}
#endif

		ShaderCore->VertBuffer.Advance(3);
		ShaderCore->DrawBuffer.EndDrawCall(3);
		ShaderCore->ParametersBuffer.Advance(1);
	}	

	STAT(unclockFast(Stats.TileBufferCycles));
}

/*-----------------------------------------------------------------------------
	OpenGL ES Tile Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawTileESProgram::DrawTileESProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWTILE_SIZE * 6; // 6 vertices per draw call
	ParametersBufferSize			= DRAWTILE_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::TileParametersIndex;
	NumTextureSamplers				= 1;
	DrawMode						= GL_TRIANGLES;
	NumVertexAttributes				= 3;
	UseSSBOParametersBuffer			= RenDev->UsingShaderDrawParameters; // heh. You never know...
	ParametersInfo					= DrawTileParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= nullptr;
	FragmentShaderFunc				= &BuildFragmentShader;
	DepthTesting					= FALSE;
}

void UXOpenGLRenderDevice::DrawTileESProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DrawTileVertexES), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(DrawTileVertexES), (GLvoid*)(offsetof(DrawTileVertexES, DrawID)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DrawTileVertexES), (GLvoid*)(offsetof(DrawTileVertexES, TexCoords)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawTileESProgram::DeactivateShader()
{
	ShaderProgramImpl::DeactivateShader();

#if UNREAL_TOURNAMENT_OLDUNREAL
	if (!DepthTesting)
		glEnable(GL_DEPTH_TEST);
#endif

	if (RenDev->UseAA && RenDev->NoAATiles)
		glEnable(GL_MULTISAMPLE);
}

void UXOpenGLRenderDevice::DrawTileESProgram::ActivateShader()
{
	ShaderProgramImpl::ActivateShader();

#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
	if (RenDev->UseAA && RenDev->NoAATiles)
		glDisable(GL_MULTISAMPLE);
#endif

#if UNREAL_TOURNAMENT_OLDUNREAL
	if (GUglyHackFlags & HACKFLAGS_PostRender) // ugly hack to make the HUD always render on top of weapons
	{
		DepthTesting = FALSE;
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		DepthTesting = TRUE;
	}
#endif
}

void UXOpenGLRenderDevice::DrawTileESProgram::BuildCommonSpecializations()
{
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Translucent));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Translucent|ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Translucent|ShaderOptions::OPT_AlphaBlended));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_AlphaBlended));
}

/*-----------------------------------------------------------------------------
	OpenGL Core Tile Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawTileCoreProgram::DrawTileCoreProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWTILE_SIZE * 3;
	ParametersBufferSize			= DRAWTILE_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::TileParametersIndex;
	NumTextureSamplers				= 1;
	DrawMode						= GL_TRIANGLES;
	NumVertexAttributes				= 5;
	UseSSBOParametersBuffer			= RenDev->UsingShaderDrawParameters;
	ParametersInfo					= DrawTileParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= &BuildGeometryShader;
	FragmentShaderFunc				= &BuildFragmentShader;
	DepthTesting					= FALSE;
}

void UXOpenGLRenderDevice::DrawTileCoreProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DrawTileVertexCore), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(DrawTileVertexCore), (GLvoid*)(offsetof(DrawTileVertexCore, DrawID)));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(DrawTileVertexCore), (GLvoid*)(offsetof(DrawTileVertexCore, TexCoords0)));
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(DrawTileVertexCore), (GLvoid*)(offsetof(DrawTileVertexCore, TexCoords1)));
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(DrawTileVertexCore), (GLvoid*)(offsetof(DrawTileVertexCore, TexCoords2)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawTileCoreProgram::DeactivateShader()
{
	ShaderProgramImpl::DeactivateShader();

#if UNREAL_TOURNAMENT_OLDUNREAL
	if (!DepthTesting)
		glEnable(GL_DEPTH_TEST);
#endif

	if (RenDev->UseAA && RenDev->NoAATiles)
		glEnable(GL_MULTISAMPLE);
}

void UXOpenGLRenderDevice::DrawTileCoreProgram::ActivateShader()
{
	ShaderProgramImpl::ActivateShader();

#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
    if (RenDev->UseAA && RenDev->NoAATiles)
        glDisable(GL_MULTISAMPLE);
#endif

#if UNREAL_TOURNAMENT_OLDUNREAL
	if (GUglyHackFlags & HACKFLAGS_PostRender) // ugly hack to make the HUD always render on top of weapons
	{
		DepthTesting = FALSE;
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		DepthTesting = TRUE;
	}
#endif
}

void UXOpenGLRenderDevice::DrawTileCoreProgram::BuildCommonSpecializations()
{
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Translucent));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Translucent|ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_Translucent|ShaderOptions::OPT_AlphaBlended));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture|ShaderOptions::OPT_AlphaBlended));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

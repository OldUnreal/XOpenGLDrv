/*=============================================================================
	DrawSimple.cpp: Unreal XOpenGL for simple drawing routines.
	In use mostly for UED2.

	Line flags:
    * LINE_None: Solid line
	* LINE_Transparent: Transparent/dotted line
	* LINE_DepthCued: Honors Z-Ordering (Seems not to be implemented in SoftDrv?)

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch
=============================================================================*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	Helpers
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::PrepareSimpleCall
(
	ShaderProgram* Shader,
	glm::uint& OldLineFlags,
	glm::uint LineFlags, 
	glm::uint& OldBlendMode,
	glm::uint BlendMode
)
{
	if (OldLineFlags != LineFlags || OldBlendMode != BlendMode)
	{
		Shader->Flush(false);

		OldLineFlags = LineFlags;
		OldBlendMode = BlendMode;

		// Set GL state
		SetDepth(OldLineFlags);
		SetBlend(OldBlendMode);
	}
}

/*-----------------------------------------------------------------------------
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{
	guard(UXOpenGLRenderDevice::Draw2DLine);

	if (NoDrawSimple)
		return;

    STAT(clockFast(Stats.Draw2DLine));
	SetProgram(Simple_Line_Prog);

	auto Shader = dynamic_cast<DrawSimpleLineProgram*>(Shaders[Simple_Line_Prog]);
	
	Color.W = 1.f; //Unfortunately this is usually set to 0.
	const auto DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : FPlaneToVec4(Color);
	constexpr auto BlendMode = PF_AlphaBlend;

	if (Shader->DrawBuffer.IsFull() || !Shader->ParametersBuffer.CanBuffer(1) || !Shader->VertBuffer.CanBuffer(2))
		Shader->Flush(true);

	PrepareSimpleCall(Shader,
		Shader->OldLineFlags, LineFlags,
		Shader->OldBlendMode, BlendMode);

	Shader->SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_None));

	Shader->ParametersBuffer.GetCurrentElementPtr()->DrawColor = DrawColor;

	Shader->DrawBuffer.StartDrawCall();
	auto Out = Shader->VertBuffer.GetCurrentElementPtr();
	const auto DrawID = Shader->DrawBuffer.GetDrawID();
	(Out  )->Coords = glm::vec3(RFX2 * P1.Z * (P1.X - Frame->FX2), RFY2 * P1.Z * (P1.Y - Frame->FY2), P1.Z);
	(Out++)->DrawID = DrawID;
	(Out  )->Coords = glm::vec3(RFX2 * P2.Z * (P2.X - Frame->FX2), RFY2 * P2.Z * (P2.Y - Frame->FY2), P2.Z);
	(Out  )->DrawID = DrawID;
	Shader->VertBuffer.Advance(2);
	Shader->DrawBuffer.EndDrawCall(2);
	Shader->ParametersBuffer.Advance(1);

    STAT(unclockFast(Stats.Draw2DLine));
	unguard;
}

void UXOpenGLRenderDevice::Draw3DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{
	guard(UXOpenGLRenderDevice::Draw3DLine);

	if (NoDrawSimple)
		return;

    STAT(clockFast(Stats.Draw3DLine));
	SetProgram(Simple_Line_Prog);

	auto Shader = dynamic_cast<DrawSimpleLineProgram*>(Shaders[Simple_Line_Prog]);
	
	Color.W = 1.f; //Unfortunately this is usually set to 0.

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);

	if (Frame->Viewport->IsOrtho())
	{
		// Zoom.
		P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
		P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
		P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
		P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2f)
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL)
			Draw2DPoint(Frame, Color, LineFlags & LINE_DepthCued, P1.X - 1.f, P1.Y - 1.f, P1.X + 1.f, P1.Y + 1.f, P1.Z);
	}
	else
	{
		const auto DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : FPlaneToVec4(Color);
		constexpr auto BlendMode = PF_AlphaBlend;

		if (Shader->DrawBuffer.IsFull() || !Shader->ParametersBuffer.CanBuffer(1) || !Shader->VertBuffer.CanBuffer(2))
			Shader->Flush(true);

		PrepareSimpleCall(Shader,
			Shader->OldLineFlags, LineFlags,
			Shader->OldBlendMode, BlendMode);

		Shader->SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_None));

		Shader->ParametersBuffer.GetCurrentElementPtr()->DrawColor = DrawColor;

		Shader->DrawBuffer.StartDrawCall();
		auto Out = Shader->VertBuffer.GetCurrentElementPtr();
		const auto DrawID = Shader->DrawBuffer.GetDrawID();
		(Out  )->Coords = glm::vec3(P1.X, P1.Y, P1.Z);
		(Out++)->DrawID = DrawID;
		(Out  )->Coords = glm::vec3(P2.X, P2.Y, P2.Z);
		(Out  )->DrawID = DrawID;
		Shader->VertBuffer.Advance(2);
		Shader->DrawBuffer.EndDrawCall(2);
		Shader->ParametersBuffer.Advance(1);
	}

    STAT(unclockFast(Stats.Draw3DLine));
	unguard;
}

void UXOpenGLRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
{
	guard(UXOpenGLRenderDevice::Draw2DPoint);

	if (NoDrawSimple)
		return;

    STAT(clockFast(Stats.Draw2DPoint));
	SetProgram(Simple_Triangle_Prog);

	auto Shader = dynamic_cast<DrawSimpleTriangleProgram*>(Shaders[Simple_Triangle_Prog]);
	
	Color.W = 1.f; //Unfortunately this is usually set to 0.
	const auto DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : FPlaneToVec4(Color);
	constexpr auto BlendMode = PF_AlphaBlend;

	if (Frame->Viewport->IsOrtho())
		Z = 1.f;
	else if (Z < 0.f)
		Z = -Z;

	if (Shader->DrawBuffer.IsFull() || !Shader->ParametersBuffer.CanBuffer(1) || !Shader->VertBuffer.CanBuffer(6))
		Shader->Flush(true);

	PrepareSimpleCall(Shader,
		Shader->OldLineFlags, LineFlags,
		Shader->OldBlendMode, BlendMode);

	Shader->SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_None));

	Shader->ParametersBuffer.GetCurrentElementPtr()->DrawColor = DrawColor;

	Shader->DrawBuffer.StartDrawCall();
	auto Out = Shader->VertBuffer.GetCurrentElementPtr();
	const auto DrawID = Shader->DrawBuffer.GetDrawID();
	(Out  )->Coords = glm::vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	(Out++)->DrawID = DrawID;
	(Out  )->Coords = glm::vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	(Out++)->DrawID = DrawID;
	(Out  )->Coords = glm::vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	(Out++)->DrawID = DrawID;
	(Out  )->Coords = glm::vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	(Out++)->DrawID = DrawID;
	(Out  )->Coords = glm::vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	(Out++)->DrawID = DrawID;
	(Out  )->Coords = glm::vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	(Out  )->DrawID = DrawID;
	Shader->VertBuffer.Advance(6);
	Shader->DrawBuffer.EndDrawCall(6);
	Shader->ParametersBuffer.Advance(1);

    STAT(unclockFast(Stats.Draw2DPoint));
	unguard;
}

void UXOpenGLRenderDevice::EndFlash()
{
	guard(UXOpenGLRenderDevice::EndFlash);

	if (NoDrawSimple)
		return;

	SetProgram(Simple_Triangle_Prog);

	auto Shader = dynamic_cast<DrawSimpleTriangleProgram*>(Shaders[Simple_Triangle_Prog]);
	
	if (FlashScale != FPlane(0.5, 0.5, 0.5, 0) || FlashFog != FPlane(0, 0, 0, 0))
	{
		const auto DrawColor = glm::vec4(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.f, 1.f));
		constexpr auto BlendMode = PF_Highlighted;
		constexpr DWORD LineFlags = LINE_Transparent;

		if (Shader->DrawBuffer.IsFull() || !Shader->ParametersBuffer.CanBuffer(1) || !Shader->VertBuffer.CanBuffer(6))
			Shader->Flush(true);

		PrepareSimpleCall(Shader,
			Shader->OldLineFlags, LineFlags,
			Shader->OldBlendMode, BlendMode);

		Shader->SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_None));

		Shader->ParametersBuffer.GetCurrentElementPtr()->DrawColor = DrawColor;

		const FLOAT RFX2 = 2.f * RProjZ / Viewport->SizeX;
		const FLOAT RFY2 = 2.f * RProjZ * Aspect / Viewport->SizeY;

		Shader->DrawBuffer.StartDrawCall();
		auto Out = Shader->VertBuffer.GetCurrentElementPtr();
		const auto DrawID = Shader->DrawBuffer.GetDrawID();
		(Out  )->Coords = glm::vec3(RFX2 * (-Viewport->SizeX / 2.0), RFY2 * (-Viewport->SizeY / 2.0), 1.f);
		(Out++)->DrawID = DrawID;
		(Out  )->Coords = glm::vec3(RFX2 * (+Viewport->SizeX / 2.0), RFY2 * (-Viewport->SizeY / 2.0), 1.f);
		(Out++)->DrawID = DrawID;
		(Out  )->Coords = glm::vec3(RFX2 * (+Viewport->SizeX / 2.0), RFY2 * (+Viewport->SizeY / 2.0), 1.f);
		(Out++)->DrawID = DrawID;
		(Out  )->Coords = glm::vec3(RFX2 * (-Viewport->SizeX / 2.0), RFY2 * (-Viewport->SizeY / 2.0), 1.f);
		(Out++)->DrawID = DrawID;
		(Out  )->Coords = glm::vec3(RFX2 * (+Viewport->SizeX / 2.0), RFY2 * (+Viewport->SizeY / 2.0), 1.f);
		(Out++)->DrawID = DrawID;
		(Out  )->Coords = glm::vec3(RFX2 * (-Viewport->SizeX / 2.0), RFY2 * (+Viewport->SizeY / 2.0), 1.f);
		(Out  )->DrawID = DrawID;
		Shader->VertBuffer.Advance(6);
		Shader->DrawBuffer.EndDrawCall(6);
		Shader->ParametersBuffer.Advance(1);
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Simple Line Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawSimpleLineProgram::DrawSimpleLineProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWSIMPLE_SIZE * 2; // 2 vertices per draw call
	ParametersBufferSize			= DRAWSIMPLE_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::SimpleLineParametersIndex;
	NumTextureSamplers				= 0;
	DrawMode						= GL_LINES;
	NumVertexAttributes				= 2;
	UseSSBOParametersBuffer			= false;
	ParametersInfo					= DrawSimpleParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= nullptr;
	FragmentShaderFunc				= &BuildFragmentShader;
}

void UXOpenGLRenderDevice::DrawSimpleLineProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DrawSimpleVertex), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(DrawSimpleVertex), (GLvoid*)(offsetof(DrawSimpleVertex, DrawID)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawSimpleLineProgram::ActivateShader()
{
	ShaderProgramImpl::ActivateShader();
	OldLineFlags = RenDev->CurrentLineFlags;
	OldBlendMode = RenDev->CurrentBlendPolyFlags;
}

void UXOpenGLRenderDevice::DrawSimpleLineProgram::DeactivateShader()
{
	ShaderProgramImpl::DeactivateShader();
	if (RenDev->CurrentLineFlags != OldLineFlags)
		RenDev->SetDepth(OldLineFlags);
	if (RenDev->CurrentBlendPolyFlags != OldBlendMode)
		RenDev->SetBlend(OldBlendMode);
}

void UXOpenGLRenderDevice::DrawSimpleLineProgram::BuildCommonSpecializations()
{
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_None));
}

/*-----------------------------------------------------------------------------
	Simple Triangle Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawSimpleTriangleProgram::DrawSimpleTriangleProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWSIMPLE_SIZE * 6; // 6 vertices per draw call
	ParametersBufferSize			= DRAWSIMPLE_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::SimpleTriangleParametersIndex;
	NumTextureSamplers				= 0;
	DrawMode						= GL_TRIANGLES;
	NumVertexAttributes				= 2;
	UseSSBOParametersBuffer			= false;
	ParametersInfo					= DrawSimpleParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= nullptr;
	FragmentShaderFunc				= &BuildFragmentShader;
}

void UXOpenGLRenderDevice::DrawSimpleTriangleProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DrawSimpleVertex), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(DrawSimpleVertex), (GLvoid*)(offsetof(DrawSimpleVertex, DrawID)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawSimpleTriangleProgram::ActivateShader()
{
	ShaderProgramImpl::ActivateShader();
	OldLineFlags = RenDev->CurrentLineFlags;
	OldBlendMode = RenDev->CurrentBlendPolyFlags;
}

void UXOpenGLRenderDevice::DrawSimpleTriangleProgram::DeactivateShader()
{
	ShaderProgramImpl::DeactivateShader();
	if (RenDev->CurrentLineFlags != OldLineFlags)
		RenDev->SetDepth(OldLineFlags);
	if (RenDev->CurrentBlendPolyFlags != OldBlendMode)
		RenDev->SetBlend(OldBlendMode);
}

void UXOpenGLRenderDevice::DrawSimpleTriangleProgram::BuildCommonSpecializations()
{
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_None));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

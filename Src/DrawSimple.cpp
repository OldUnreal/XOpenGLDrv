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
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{
	if (NoDrawSimple)
		return;

	clockFast(Stats.Draw2DLine);
	SetProgram(Simple_Prog);
	dynamic_cast<DrawSimpleProgram*>(Shaders[Simple_Prog])->Draw2DLine(Frame, Color, LineFlags, P1, P2);
	unclockFast(Stats.Draw2DLine);
}

void UXOpenGLRenderDevice::Draw3DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{
	if (NoDrawSimple)
		return;

	clockFast(Stats.Draw3DLine);
	SetProgram(Simple_Prog);
	dynamic_cast<DrawSimpleProgram*>(Shaders[Simple_Prog])->Draw3DLine(Frame, Color, LineFlags, P1, P2);
	unclockFast(Stats.Draw3DLine);
}

void UXOpenGLRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
{
	if (NoDrawSimple)
		return;

	clockFast(Stats.Draw2DPoint);
	SetProgram(Simple_Prog);
	dynamic_cast<DrawSimpleProgram*>(Shaders[Simple_Prog])->Draw2DPoint(Frame, Color, LineFlags, X1, Y1, X2, Y2, Z);
	unclockFast(Stats.Draw2DPoint);
}

void UXOpenGLRenderDevice::EndFlash()
{
	if (NoDrawSimple)
		return;

	SetProgram(Simple_Prog);
	dynamic_cast<DrawSimpleProgram*>(Shaders[Simple_Prog])->EndFlash();
}

/*-----------------------------------------------------------------------------
	ShaderProgram Implementation
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawSimpleProgram::PrepareDrawCall(glm::uint LineFlags, const glm::vec4& DrawColor, glm::uint BlendMode, BufferObject<BufferedVert>& OutBuffer, INT VertexCount)
{
	if (DrawCallParams.LineFlags != LineFlags ||
		DrawCallParams.DrawColor != DrawColor ||
		DrawCallParams.BlendMode != BlendMode ||
		!OutBuffer.CanBuffer(VertexCount))
	{
		Flush(true);

		DrawCallParams.LineFlags = LineFlags;
		DrawCallParams.DrawColor = DrawColor;
		DrawCallParams.BlendMode = BlendMode;
	}
	DrawCallParams.HitTesting = GIsEditor && RenDev->HitTesting();
}

void UXOpenGLRenderDevice::DrawSimpleProgram::Draw2DLine(const FSceneNode* Frame, FPlane& Color, DWORD LineFlags, const FVector& P1, const FVector& P2)
{
	guard(UXOpenGLRenderDevice::Draw2DLine);

	Color.W = 1.f; //Unfortunately this is usually set to 0.
	const auto DrawColor = (GIsEditor && RenDev->HitTesting()) ? FPlaneToVec4(RenDev->HitColor) : FPlaneToVec4(Color);
	constexpr auto BlendMode = PF_AlphaBlend;

	if (LineDrawBuffer.IsFull())
		Flush(true);

	PrepareDrawCall(LineFlags, DrawColor, BlendMode, LineVertBuffer, 2);
	DrawCallParams.Gamma = 1.f;

	LineDrawBuffer.StartDrawCall();
	auto Out = LineVertBuffer.GetCurrentElementPtr();
	(Out++)->Point = glm::vec3(RenDev->RFX2 * P1.Z * (P1.X - Frame->FX2), RenDev->RFY2 * P1.Z * (P1.Y - Frame->FY2), P1.Z);
	(Out  )->Point = glm::vec3(RenDev->RFX2 * P2.Z * (P2.X - Frame->FX2), RenDev->RFY2 * P2.Z * (P2.Y - Frame->FY2), P2.Z);
	LineVertBuffer.Advance(2);
	LineDrawBuffer.EndDrawCall(2);

	if (RenDev->NoBuffering)
		Flush(true);

	unguard;
}

void UXOpenGLRenderDevice::DrawSimpleProgram::Draw3DLine( FSceneNode* Frame, FPlane& Color, DWORD LineFlags, FVector& P1, FVector& P2 )
{
	guard(UXOpenGLRenderDevice::Draw3DLine);

	Color.W = 1.f; //Unfortunately this is usually set to 0.
	const auto DrawColor = (GIsEditor && RenDev->HitTesting()) ? FPlaneToVec4(RenDev->HitColor) : FPlaneToVec4(Color);
	constexpr auto BlendMode = PF_AlphaBlend;

	P1 = P1.TransformPointBy( Frame->Coords );
	P2 = P2.TransformPointBy( Frame->Coords );

	if( Frame->Viewport->IsOrtho() )
	{
		// Zoom.
		P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
		P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
		P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
		P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2f)
			Draw2DLine( Frame, Color, LineFlags, P1, P2 );
		else if( Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL )
			Draw2DPoint(Frame, Color, LineFlags&LINE_DepthCued, P1.X - 1.f, P1.Y - 1.f, P1.X + 1.f, P1.Y + 1.f, P1.Z);
	}
	else
	{
		if (LineDrawBuffer.IsFull())
			Flush(true);

		PrepareDrawCall(LineFlags, DrawColor, BlendMode, LineVertBuffer, 2);
		DrawCallParams.Gamma = 1.f;


		LineDrawBuffer.StartDrawCall();
		auto Out = LineVertBuffer.GetCurrentElementPtr();
		(Out++)->Point = glm::vec3(P1.X, P1.Y, P1.Z);
		(Out  )->Point = glm::vec3(P2.X, P2.Y, P2.Z);
		LineVertBuffer.Advance(2);
		LineDrawBuffer.EndDrawCall(2);

		if (RenDev->NoBuffering)
			Flush(true);
	}

	unguard;
}

void UXOpenGLRenderDevice::DrawSimpleProgram::EndFlash()
{
	guard(UXOpenGLRenderDevice::EndFlash);

	if( RenDev->FlashScale != FPlane(0.5,0.5,0.5,0) || RenDev->FlashFog != FPlane(0,0,0,0) )
	{
		const auto DrawColor = glm::vec4(RenDev->FlashFog.X, RenDev->FlashFog.Y, RenDev->FlashFog.Z, 1.0f - Min(RenDev->FlashScale.X*2.f,1.f));
		constexpr auto BlendMode = PF_Highlighted;
		constexpr DWORD LineFlags = LINE_Transparent;

		if (TriangleDrawBuffer.IsFull())
			Flush(true);

		PrepareDrawCall(LineFlags, DrawColor, BlendMode, TriangleVertBuffer, 4);
		DrawCallParams.Gamma = RenDev->GetViewportGamma(RenDev->Viewport);

		const FLOAT RFX2 = 2.f * RenDev->RProjZ                  / RenDev->Viewport->SizeX;
		const FLOAT RFY2 = 2.f * RenDev->RProjZ * RenDev->Aspect / RenDev->Viewport->SizeY;

		TriangleDrawBuffer.StartDrawCall();
		auto Out = TriangleVertBuffer.GetCurrentElementPtr();
		(Out++)->Point = glm::vec3(RFX2 * (-RenDev->Viewport->SizeX / 2.0), RFY2 * (-RenDev->Viewport->SizeY / 2.0), 1.f);
		(Out++)->Point = glm::vec3(RFX2 * (+RenDev->Viewport->SizeX / 2.0), RFY2 * (-RenDev->Viewport->SizeY / 2.0), 1.f);
		(Out++)->Point = glm::vec3(RFX2 * (+RenDev->Viewport->SizeX / 2.0), RFY2 * (+RenDev->Viewport->SizeY / 2.0), 1.f);
		(Out++)->Point = glm::vec3(RFX2 * (-RenDev->Viewport->SizeX / 2.0), RFY2 * (-RenDev->Viewport->SizeY / 2.0), 1.f);
		(Out++)->Point = glm::vec3(RFX2 * (+RenDev->Viewport->SizeX / 2.0), RFY2 * (+RenDev->Viewport->SizeY / 2.0), 1.f);
		(Out  )->Point = glm::vec3(RFX2 * (-RenDev->Viewport->SizeX / 2.0), RFY2 * (+RenDev->Viewport->SizeY / 2.0), 1.f);
		TriangleVertBuffer.Advance(6);
		TriangleDrawBuffer.EndDrawCall(6);

		if (RenDev->NoBuffering)
			Flush(true);
	}

	unguard;
}

void UXOpenGLRenderDevice::DrawSimpleProgram::Draw2DPoint(const FSceneNode* Frame, FPlane& Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
{
	guard(UXOpenGLRenderDevice::Draw2DPoint);

	Color.W = 1.f; //Unfortunately this is usually set to 0.
	const auto DrawColor = (GIsEditor && RenDev->HitTesting()) ? FPlaneToVec4(RenDev->HitColor) : FPlaneToVec4(Color);
	constexpr auto BlendMode = PF_AlphaBlend;

	if (Frame->Viewport->IsOrtho())
		Z = 1.f;
	else if (Z < 0.f)
		Z = -Z;

	if (TriangleDrawBuffer.IsFull())
		Flush(true);

	PrepareDrawCall( LineFlags, DrawColor, BlendMode, TriangleVertBuffer, 4);
	DrawCallParams.Gamma = 1.f;

	TriangleDrawBuffer.StartDrawCall();
	auto Out = TriangleVertBuffer.GetCurrentElementPtr();
	(Out++)->Point = glm::vec3(RenDev->RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RenDev->RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	(Out++)->Point = glm::vec3(RenDev->RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RenDev->RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	(Out++)->Point = glm::vec3(RenDev->RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RenDev->RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	(Out++)->Point = glm::vec3(RenDev->RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RenDev->RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	(Out++)->Point = glm::vec3(RenDev->RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RenDev->RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	(Out  )->Point = glm::vec3(RenDev->RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RenDev->RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	TriangleVertBuffer.Advance(6);
	TriangleDrawBuffer.EndDrawCall(6);

	if (RenDev->NoBuffering)
		Flush(true);

	unguard;
}

void UXOpenGLRenderDevice::DrawSimpleProgram::Flush(bool Wait)
{
	guard(UXOpenGLRenderDevice::DrawSimpleProgram::Flush);

	if (LineVertBuffer.IsEmpty() && TriangleVertBuffer.IsEmpty())
		return;

	// Set GL state
	RenDev->SetDepth(DrawCallParams.LineFlags);
	RenDev->SetBlend(DrawCallParams.BlendMode, false);
	
	// Pass drawcall params
	auto Out = ParametersBuffer.GetElementPtr(0);
	memcpy(Out, &DrawCallParams, sizeof(DrawCallParameters));
	ParametersBuffer.Bind();
	ParametersBuffer.BufferData(RenDev->UseBufferInvalidation, false, DRAWCALL_BUFFER_USAGE_PATTERN);
	
	if (LineVertBuffer.Size() > 0)
	{
		LineVertBuffer.Bind();

		//if (!LineVertBuffer.IsInputLayoutCreated())
		{
			CreateInputLayout();
			glEnableVertexAttribArray(0);
			LineVertBuffer.SetInputLayoutCreated();
		}

		LineVertBuffer.BufferData(false, true, GL_STREAM_DRAW);

		LineDrawBuffer.Draw(GL_LINES, RenDev);

		LineVertBuffer.Lock();
		LineVertBuffer.Rotate(Wait);

		LineDrawBuffer.Reset(LineVertBuffer.BeginOffsetBytes() / sizeof(BufferedVert));
	}

	if (TriangleVertBuffer.Size() > 0)
	{
		TriangleVertBuffer.Bind();
	
		//if (!TriangleVertBuffer.IsInputLayoutCreated())
		{
			CreateInputLayout();
			glEnableVertexAttribArray(0);
			TriangleVertBuffer.SetInputLayoutCreated();
		}

		TriangleVertBuffer.BufferData(false, true, GL_STREAM_DRAW);

		TriangleDrawBuffer.Draw(GL_TRIANGLES, RenDev);

		TriangleVertBuffer.Lock();
		TriangleVertBuffer.Rotate(Wait);

		TriangleDrawBuffer.Reset(TriangleVertBuffer.BeginOffsetBytes() / sizeof(BufferedVert));
	}

	unguard;
}

void UXOpenGLRenderDevice::DrawSimpleProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BufferedVert), (GLvoid*)(0));
}

void UXOpenGLRenderDevice::DrawSimpleProgram::DeactivateShader()
{
	Flush(false);

	if (LineVertBuffer.IsBound() || TriangleVertBuffer.IsBound())
		glDisableVertexAttribArray(0);
}

void UXOpenGLRenderDevice::DrawSimpleProgram::ActivateShader()
{
	glUseProgram(ShaderProgramObject);
	// Note: we don't enable any vertex attrib array here because we haven't bound any vertex buffers yet

	ParametersBuffer.Bind();
}

void UXOpenGLRenderDevice::DrawSimpleProgram::BindShaderState()
{
	ShaderProgram::BindShaderState();

	BindUniform(SimpleParametersIndex, "DrawCallParameters");
}

void UXOpenGLRenderDevice::DrawSimpleProgram::MapBuffers()
{
	for (const auto Buffer : { &LineVertBuffer, &TriangleVertBuffer })
	{
		Buffer->GenerateVertexBuffer(RenDev);
		Buffer->MapVertexBuffer(RenDev->UsingPersistentBuffersSimple, DRAWSIMPLE_SIZE);
	}

	ParametersBuffer.GenerateUBOBuffer(RenDev, SimpleParametersIndex);
	ParametersBuffer.MapUBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, 1, DRAWCALL_BUFFER_USAGE_PATTERN);
	ParametersBuffer.Advance(1);
}

void UXOpenGLRenderDevice::DrawSimpleProgram::UnmapBuffers()
{
	LineVertBuffer.DeleteBuffer();
	TriangleVertBuffer.DeleteBuffer();
	ParametersBuffer.DeleteBuffer();
}

bool UXOpenGLRenderDevice::DrawSimpleProgram::BuildShaderProgram()
{
	return ShaderProgram::BuildShaderProgram(
		BuildVertexShader,
		nullptr,//BuildGeometryShader,
		BuildFragmentShader,
		EmitHeader);
}

UXOpenGLRenderDevice::DrawSimpleProgram::~DrawSimpleProgram()
{
	DeleteShader();
	DrawSimpleProgram::UnmapBuffers();
}

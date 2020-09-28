/*=============================================================================
	DrawSimple.cpp: Unreal XOpenGL for simple drawing routines.
	In use mostly for UED2.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
Line flags:

LINE_None
* Solid line.

LINE_Transparent
* Transparent/dotted line.

LINE_DepthCued
* Honors Z-Ordering.
* Seems not to be implemented in SoftDrv?
-----------------------------------------------------------------------------*/

inline void UXOpenGLRenderDevice::BufferLines(FLOAT* DrawLinesTemp, FLOAT* LineData)
{
	DrawLinesTemp[0] = LineData[0];
	DrawLinesTemp[1] = LineData[1];
	DrawLinesTemp[2] = LineData[2];
	DrawLinesTemp[3] = LineData[3];
	DrawLinesTemp[4] = LineData[4];
	DrawLinesTemp[5] = LineData[5];
}


void UXOpenGLRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{
	guard(UXOpenGLRenderDevice::Draw2DLine);

	if (NoDrawSimple)
		return;

	SetProgram(Simple_Prog);
	SetBlend(PF_AlphaBlend, false);
	CHECK_GL_ERROR();

	clockFast(Stats.Draw2DLine);

	//Unfortunately this is usually set to 0.
	Color.W = 1.f;

	if (DrawLinesBufferData.VertSize > 0 && (DrawLinesBufferData.LineFlags != LineFlags || DrawLinesBufferData.DrawColor != Color || DrawLinesBufferData.VertSize >= DRAWSIMPLE_SIZE-18))
		DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);

	Draw2DLineVertsBuf[0] = RFX2*P1.Z*(P1.X - Frame->FX2);
	Draw2DLineVertsBuf[1] = RFY2*P1.Z*(P1.Y - Frame->FY2);
	Draw2DLineVertsBuf[2] = P1.Z;

	Draw2DLineVertsBuf[3] = RFX2*P2.Z*(P2.X - Frame->FX2);
	Draw2DLineVertsBuf[4] = RFY2*P2.Z*(P2.Y - Frame->FY2);
	Draw2DLineVertsBuf[5] = P2.Z;

	if (NoBuffering)
	{
		DrawSimpleGeometryVerts(Draw2DLineMode, 6, GL_LINES, LineFlags, Color, false);
	}
	else if (HitTesting())
	{
		if (DrawLinesBufferData.VertSize > 0)
			DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);
		DrawSimpleGeometryVerts(Draw2DLineMode, 6, GL_LINES, LineFlags, HitColor, false);
	}
	else
	{
		BufferLines(&DrawLinesVertsBuf[DrawLinesBufferData.VertSize], Draw2DLineVertsBuf);
		DrawLinesBufferData.DrawColor = Color;
		DrawLinesBufferData.VertSize += 6;
		DrawLinesBufferData.LineFlags = LineFlags;
	}

	unclockFast(Stats.Draw2DLine);
	unguard;
}

void UXOpenGLRenderDevice::Draw3DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{
	guard(UXOpenGLRenderDevice::Draw3DLine);

	if (NoDrawSimple)
		return;

	clockFast(Stats.Draw3DLine);

	SetProgram(Simple_Prog);
	SetBlend(PF_AlphaBlend, false);
	CHECK_GL_ERROR();

	//Unfortunately this is usually set to 0.
	Color.W = 1.f;

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
		if (DrawLinesBufferData.VertSize > 0 && (DrawLinesBufferData.LineFlags != LineFlags || DrawLinesBufferData.DrawColor != Color || DrawLinesBufferData.VertSize >= DRAWSIMPLE_SIZE-18))
			DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);

		Draw3DLineVertsBuf[0] = P1.X;
		Draw3DLineVertsBuf[1] = P1.Y;
		Draw3DLineVertsBuf[2] = P1.Z;

		Draw3DLineVertsBuf[3] = P2.X;
		Draw3DLineVertsBuf[4] = P2.Y;
		Draw3DLineVertsBuf[5] = P2.Z;

		if (NoBuffering)
		{
  			DrawSimpleGeometryVerts(Draw3DLineMode, 6, GL_LINES, LineFlags, Color, false);
		}
		else if (HitTesting())
		{
			if (DrawLinesBufferData.VertSize > 0)
				DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);
			DrawSimpleGeometryVerts(Draw3DLineMode, 6, GL_LINES, LineFlags, HitColor, false);
		}
		else
		{
			BufferLines(&DrawLinesVertsBuf[DrawLinesBufferData.VertSize], Draw3DLineVertsBuf);
			DrawLinesBufferData.DrawColor = Color;
			DrawLinesBufferData.VertSize += 6;
			DrawLinesBufferData.LineFlags = LineFlags;
		}
	}

	unclockFast(Stats.Draw3DLine);

	unguard;
}

void UXOpenGLRenderDevice::EndFlash()
{
	guard(UXOpenGLRenderDevice::EndFlash);

	if (NoDrawSimple)
		return;

	SetProgram(No_Prog);

	if( FlashScale!=FPlane(0.5,0.5,0.5,0) || FlashFog!=FPlane(0,0,0,0) )
	{
		SetProgram(Simple_Prog);
		SetBlend(PF_Highlighted, false);
		FPlane Color(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0-Min(FlashScale.X*2.f,1.f));

		FLOAT RFX2 = 2.0*RProjZ       /Viewport->SizeX;
		FLOAT RFY2 = 2.0*RProjZ*Aspect/Viewport->SizeY;

		EndFlashVertsBuf[0] = RFX2*(-Viewport->SizeX / 2.0);
		EndFlashVertsBuf[1] = RFY2*(-Viewport->SizeY / 2.0);
		EndFlashVertsBuf[2] = 1.f;

		EndFlashVertsBuf[3] = RFX2*(+Viewport->SizeX / 2.0);
		EndFlashVertsBuf[4] = RFY2*(-Viewport->SizeY / 2.0);
		EndFlashVertsBuf[5] = 1.f;

		EndFlashVertsBuf[6] = RFX2*(+Viewport->SizeX / 2.0);
		EndFlashVertsBuf[7] = RFY2*(+Viewport->SizeY / 2.0);
		EndFlashVertsBuf[8] = 1.f;

		EndFlashVertsBuf[9] = RFX2*(-Viewport->SizeX / 2.0);
		EndFlashVertsBuf[10] = RFY2*(+Viewport->SizeY / 2.0);
		EndFlashVertsBuf[11] = 1.f;

		DrawSimpleGeometryVerts(DrawEndFlashMode, 12, GL_TRIANGLE_FAN, 0, Color, false);
	}

	unguard;
}

void UXOpenGLRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
{
	guard(UXOpenGLRenderDevice::Draw2DPoint);

	if (NoDrawSimple)
		return;

	SetProgram(Simple_Prog);
	SetBlend(PF_AlphaBlend, false);
	CHECK_GL_ERROR();

	clockFast(Stats.Draw2DPoint);

	//Unfortunately this is usually set to 0.
	Color.W = 1.f;

	if (Frame->Viewport->IsOrtho())
		Z = 1.f;
	else if (Z < 0.0)
		Z = -Z;

	Draw2DPointVertsBuf[0] = RFX2*Z*(X1 - Frame->FX2-0.5f);
	Draw2DPointVertsBuf[1] = RFY2*Z*(Y1 - Frame->FY2-0.5f);
	Draw2DPointVertsBuf[2] = Z;

	Draw2DPointVertsBuf[3] = RFX2*Z*(X2 - Frame->FX2+0.5f);
	Draw2DPointVertsBuf[4] = RFY2*Z*(Y1 - Frame->FY2-0.5f);
	Draw2DPointVertsBuf[5] = Z;

	Draw2DPointVertsBuf[6] = RFX2*Z*(X2 - Frame->FX2+0.5f);
	Draw2DPointVertsBuf[7] = RFY2*Z*(Y2 - Frame->FY2+0.5f);
	Draw2DPointVertsBuf[8] = Z;

	Draw2DPointVertsBuf[9] = RFX2*Z*(X1 - Frame->FX2-0.5f);
	Draw2DPointVertsBuf[10] = RFY2*Z*(Y2 - Frame->FY2+0.5f);
	Draw2DPointVertsBuf[11] = Z;

	DrawSimpleGeometryVerts(Draw2DPointMode, 12, GL_TRIANGLE_FAN, LineFlags, Color, false);

	unclockFast(Stats.Draw2DPoint);

	unguard;
}

void UXOpenGLRenderDevice::DrawSimpleGeometryVerts(DrawSimpleMode DrawMode, GLuint Size, INT Mode, DWORD LineFlags, FPlane DrawColor, bool BufferedDraw)
{
	guard(UXOpenGLRenderDevice::DrawSimpleGeometryVerts);

	// Set depth mode.
	SetDepth(LineFlags);

	//Compensate coloring for sRGB.
	if (GIsEditor)
        DrawColor = FOpenGLGammaDecompress_sRGB(DrawColor);

	switch (DrawMode)
	{
        case DrawLineMode:
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * Size, DrawLinesVertsBuf, GL_DYNAMIC_DRAW);
            break;
        case Draw2DPointMode:
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * Size, Draw2DPointVertsBuf, GL_DYNAMIC_DRAW);
            break;
        case Draw2DLineMode:
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * Size, Draw2DLineVertsBuf, GL_DYNAMIC_DRAW);
            break;
        case Draw3DLineMode:
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * Size, Draw3DLineVertsBuf, GL_DYNAMIC_DRAW);
            break;
        case DrawEndFlashMode:
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * Size, EndFlashVertsBuf, GL_DYNAMIC_DRAW);
            break;
	}
	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glVertexAttribPointer(VERTEX_COORD_ATTRIB,3,GL_FLOAT, GL_FALSE, sizeof(float) * FloatsPerVertex, 0);
	CHECK_GL_ERROR();

	if (HitTesting()) // UED selecting support.
	{
		glUniform4f(DrawSimpleDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
		glUniform1i(DrawSimplebHitTesting, true);
		CHECK_GL_ERROR();
	}
	else
	{
		glUniform4f(DrawSimpleDrawColor, DrawColor.X, DrawColor.Y, DrawColor.Z, DrawColor.W);
		glUniform1i(DrawSimplebHitTesting, false);
		CHECK_GL_ERROR();
	}

	// Gamma
	glUniform1f(DrawSimpleGamma, Gamma);
	CHECK_GL_ERROR();

	glUniform1ui(DrawSimpleLineFlags, LineFlags);
	CHECK_GL_ERROR();

	// Draw
	glDrawArrays(Mode, 0, Size/FloatsPerVertex);
	CHECK_GL_ERROR();

	// Clean up
	glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
	CHECK_GL_ERROR();

	if (BufferedDraw)
	{
		DrawLinesBufferData.DrawColor = FPlane(0.f, 0.f, 0.f, 0.f);
		DrawLinesBufferData.LineFlags = -1;
		DrawLinesBufferData.VertSize = 0;
	}

	unguard;
}

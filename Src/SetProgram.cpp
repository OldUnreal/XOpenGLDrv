/*=============================================================================
	SetProgram.cpp: Unreal XOpenGL program switching.
	Requires at least an OpenGL 4.1 context.

	Copyright 2014-2016 Oldunreal

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

void UXOpenGLRenderDevice::SetProgram( INT CurrentProgram )
{
	guard(UXOpenGLRenderDevice::SetProgram);
	STAT(clock(Stats.ProgramCycles));

	if (ActiveProgram != CurrentProgram)
	{
		//debugf(TEXT("ActiveProgram %i CurrentProgram %i"),ActiveProgram,CurrentProgram);
		DrawGouraudIndex = 0;
		if (ActiveProgram == GouraudPolyVert_Prog)
		{
			FPlane DrawColor = FPlane(0.f, 0.f, 0.f, 0.f);
			if(DrawGouraudBufferData.VertSize > 0)
				DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData, DrawColor);
		}
		else if (ActiveProgram == Simple_Prog)
		{
			if (DrawLinesBufferData.VertSize > 0)
				DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);
		}
		switch (CurrentProgram)
		{
			case Simple_Prog:
			{
				glBindVertexArray(0);
				glUseProgram(0);

//				if(UseAA && NoAATiles)
//					glDisable(GL_MULTISAMPLE);

				glUseProgram(DrawSimpleProg);
				glBindVertexArray(DrawSimpleGeometryVertsVao);
				glBindBuffer(GL_ARRAY_BUFFER, DrawSimpleVertBuffer);
				ActiveProgram = Simple_Prog;
				CHECK_GL_ERROR();
				break;
			}
			case Tile_Prog:
			{
				glBindVertexArray(0);
				glUseProgram(0);

//				if(UseAA && NoAATiles)
//					glDisable(GL_MULTISAMPLE);

				glUseProgram(DrawTileProg);
				glBindVertexArray(DrawTileVertsVao);
				glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);

				ActiveProgram = Tile_Prog;
				CHECK_GL_ERROR();
				break;
			}
			case GouraudPolyVert_Prog:
			{
				glBindVertexArray(0);
				if (ActiveProgram != GouraudPolyVertList_Prog) // Same program but different VAO.
				{
					glUseProgram(0);
					glUseProgram(DrawGouraudProg);
				}

//				if(UseAA && NoAATiles)
//					glEnable(GL_MULTISAMPLE);

				glBindVertexArray(DrawGouraudPolyVertsVao);
				glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBufferSingle);

				ActiveProgram = GouraudPolyVert_Prog;
				CHECK_GL_ERROR();
				break;
			}
			case GouraudPolyVertList_Prog:
			{
				glBindVertexArray(0);
				if (ActiveProgram != GouraudPolyVert_Prog) // Same program but different VAO.
				{
					glUseProgram(0);
					glUseProgram(DrawGouraudProg);
				}

//				if(UseAA && NoAATiles)
//					glEnable(GL_MULTISAMPLE);

				glBindVertexArray(DrawGouraudPolyVertListSingleBufferVao);
				glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBufferListSingle);
				ActiveProgram = GouraudPolyVertList_Prog;
				CHECK_GL_ERROR();
				break;
			}
			case ComplexSurfaceSinglePass_Prog:
			{
				glBindVertexArray(0);
				glUseProgram(0);

//				if(UseAA)
//					glEnable(GL_MULTISAMPLE);

				glUseProgram(DrawComplexSinglePassProg);
				glBindVertexArray(DrawComplexVertsSinglePassVao);
				glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBufferSinglePass);
				ActiveProgram = ComplexSurfaceSinglePass_Prog;
				CHECK_GL_ERROR();
				break;
			}
			default:
			{
				break;
			}
		}
	}

	CHECK_GL_ERROR();
	STAT(unclock(Stats.ProgramCycles));
	unguard;
}

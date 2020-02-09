/*=============================================================================
	SetProgram.cpp: Unreal XOpenGL program switching.
	Requires at least an OpenGL 4.1 context.

	Copyright 2014-2016 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Include GLM
#ifdef _MSC_VER
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

void UXOpenGLRenderDevice::SetProgram( INT CurrentProgram )
{
	guard(UXOpenGLRenderDevice::SetProgram);
	CHECK_GL_ERROR();
    STAT(clockFast(Stats.ProgramCycles));

	if (ActiveProgram != CurrentProgram)
	{
		// cleanup active program
		switch (ActiveProgram)
		{
			case GouraudPolyVert_Prog:
			case GouraudPolyVertList_Prog:
			{
				if (ActiveProgram == GouraudPolyVert_Prog && DrawGouraudBufferData.VertSize > 0)
				{
					if (UsePersistentBuffersGouraud)
						WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
					DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
				}
				else if (ActiveProgram == GouraudPolyVertList_Prog && DrawGouraudListBufferData.VertSize > 0)
				{
					DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
				}

				glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
				glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
				glDisableVertexAttribArray(NORMALS_ATTRIB);
				glDisableVertexAttribArray(COLOR_ATTRIB);
				glDisableVertexAttribArray(FOGMAP_COORD_ATTRIB);
				glDisableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
				glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
				glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
				glDisableVertexAttribArray(TEXTURE_ATTRIB);
				break;
			}
			case ComplexSurfaceSinglePass_Prog:
			{
				if (DrawComplexBufferData.VertSize > 0)
					DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
				glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
				glDisableVertexAttribArray(NORMALS_ATTRIB);
				break;
			}
			case Simple_Prog:
			{
				if (DrawLinesBufferData.VertSize > 0)
					DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);
				//DrawSimpleCleanup();
				break;
			}
			case Tile_Prog:
			{
				if (DrawTileBufferData.VertSize > 0)
					DrawTileVerts(DrawTileBufferData);
				// Clean up
				glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
				glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
				glDisableVertexAttribArray(COLOR_ATTRIB);
				if (OpenGLVersion == GL_Core)
				{
					glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
					glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
				}
				break;
			}
			default:
				break;
		}

		CHECK_GL_ERROR();
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		switch (CurrentProgram)
		{
            case No_Prog:
            {
                glBindVertexArray(0);
				glUseProgram(0);
				ActiveProgram = No_Prog;

				if (SyncToDraw)
                    glFinish();

				CHECK_GL_ERROR();
				break;
            }
			case Simple_Prog:
			{
				glBindVertexArray(0);
				glUseProgram(0);

				#ifndef __EMSCRIPTEN__
				if(UseAA && NoAATiles)
					glDisable(GL_MULTISAMPLE);
                #endif

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

				#ifndef __EMSCRIPTEN__
				if(UseAA && NoAATiles)
					glDisable(GL_MULTISAMPLE);
                #endif

				glUseProgram(DrawTileProg);
				glBindVertexArray(DrawTileVertsVao);
                glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);

				ActiveProgram = Tile_Prog;
				CHECK_GL_ERROR();

				if (OpenGLVersion == GL_ES)
				{
					glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
					glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
					glEnableVertexAttribArray(COLOR_ATTRIB);
					glEnableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);

					if (!UsePersistentBuffersTile)
					{
						const PTRINT BeginOffset = 0;
						glVertexAttribPointer(VERTEX_COORD_ATTRIB,		3, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (void*)(BeginOffset));
						glVertexAttribPointer(TEXTURE_COORD_ATTRIB,		2, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (void*)(BeginOffset + FloatSize3));
						glVertexAttribPointer(COLOR_ATTRIB,				4, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (void*)(BeginOffset + FloatSize3_2));
						glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB,	1, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (void*)(BeginOffset + FloatSize3_2_4));
					}
					CHECK_GL_ERROR();
				}
				else
				{
					glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
					glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
					glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
					glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
					glEnableVertexAttribArray(COLOR_ATTRIB);
					glEnableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);

					if (!UsePersistentBuffersTile)
					{
						const PTRINT BeginOffset = 0;
						glVertexAttribPointer(VERTEX_COORD_ATTRIB,		3, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (void*) BeginOffset);
						glVertexAttribPointer(TEXTURE_COORD_ATTRIB,		4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (void*)(BeginOffset + FloatSize3));
						glVertexAttribPointer(TEXTURE_COORD_ATTRIB2,	4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (void*)(BeginOffset + FloatSize3_4));
						glVertexAttribPointer(TEXTURE_COORD_ATTRIB3,	4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (void*)(BeginOffset + FloatSize3_4_4));
						glVertexAttribPointer(COLOR_ATTRIB,				4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (void*)(BeginOffset + FloatSize3_4_4_4));
						glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB,	1, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (void*)(BeginOffset + FloatSize3_4_4_4_4));
					}
					CHECK_GL_ERROR();
				}
				break;
			}
			case GouraudPolyVert_Prog:
			case GouraudPolyVertList_Prog:
			{
				glBindVertexArray(0);
				if ((CurrentProgram == GouraudPolyVert_Prog && ActiveProgram != GouraudPolyVertList_Prog)
					|| (CurrentProgram == GouraudPolyVertList_Prog && ActiveProgram != GouraudPolyVert_Prog)) // Same program but different VAO.
				{
					glUseProgram(0);
					glUseProgram(DrawGouraudProg);
				}

				#ifndef __EMSCRIPTEN__
				if(UseAA && NoAATiles)
					glEnable(GL_MULTISAMPLE);
                #endif

				if (CurrentProgram == GouraudPolyVert_Prog)
				{
					glBindVertexArray(DrawGouraudPolyVertsVao);
					glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
				}
				else
				{
					glBindVertexArray(DrawGouraudPolyVertListSingleBufferVao);
					glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertListBuffer);
				}

				ActiveProgram = CurrentProgram;
				CHECK_GL_ERROR();

				glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
				glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
				glEnableVertexAttribArray(NORMALS_ATTRIB);
				glEnableVertexAttribArray(COLOR_ATTRIB);
				glEnableVertexAttribArray(FOGMAP_COORD_ATTRIB);//here for VertexFogColor
				glEnableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
				glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
				glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
				glEnableVertexAttribArray(TEXTURE_ATTRIB);

				if (!UsePersistentBuffersGouraud)
				{
					const PTRINT BeginOffset = 0;
					glVertexAttribPointer(VERTEX_COORD_ATTRIB,		3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)	BeginOffset);
					glVertexAttribPointer(TEXTURE_COORD_ATTRIB,		2, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3));
					glVertexAttribPointer(NORMALS_ATTRIB,			4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2));
					glVertexAttribPointer(COLOR_ATTRIB,				4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4));
					glVertexAttribPointer(FOGMAP_COORD_ATTRIB,		4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4));
					glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB,	3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4_4));
					glVertexAttribPointer(TEXTURE_COORD_ATTRIB2,	4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4_4_3));// Tex U/V Mult, MacroTex U/V Mult
					glVertexAttribPointer(TEXTURE_COORD_ATTRIB3,	4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4_4_3_4));// DetailTex U/V Mult, Texture Format, DrawFlags
					glVertexAttribPointer(TEXTURE_ATTRIB,			4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset + FloatSize3_2_4_4_4_3_4_4));// Additional texture information
					CHECK_GL_ERROR();
				}

				// Gamma
				glUniform1f(DrawGouraudGamma, Gamma);
				CHECK_GL_ERROR();
				break;
			}
			case ComplexSurfaceSinglePass_Prog:
			{
				glBindVertexArray(0);
				glUseProgram(0);

				if(UseAA)
					glEnable(GL_MULTISAMPLE);

				glUseProgram(DrawComplexProg);
				glBindVertexArray(DrawComplexVertsSinglePassVao);
                glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
				//glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DRAWCOMPLEX_SIZE, 0, GL_STREAM_DRAW);
				ActiveProgram = ComplexSurfaceSinglePass_Prog;
				CHECK_GL_ERROR();

				glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
				glEnableVertexAttribArray(NORMALS_ATTRIB);// SurfNormals
				break;
			}
			default:
			{
			    ActiveProgram = CurrentProgram;

                if (SyncToDraw)
                    glFinish();

				break;
			}
		}
	}

	CHECK_GL_ERROR();
	STAT(unclockFast(Stats.ProgramCycles));
	unguard;
}

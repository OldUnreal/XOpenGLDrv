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

void UXOpenGLRenderDevice::DrawProgram()
{
    guard(UXOpenGLRenderDevice::EndProgram);

    if (ActiveProgram == GouraudPolyVert_Prog)
    {
        if (DrawGouraudBufferData.VertSize > 0)
        {
            if (DrawGouraudBufferRange.Sync[DrawGouraudBufferData.Index])
                WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
            DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
        }

        CHECK_GL_ERROR();
    }
    else if(ActiveProgram == GouraudPolyVertList_Prog)
    {
        if (DrawGouraudListBufferData.VertSize > 0)
            DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);

        CHECK_GL_ERROR();
    }
    else if(ActiveProgram == ComplexSurfaceSinglePass_Prog)
    {
        if (DrawComplexBufferData.VertSize > 0)
            DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);

        CHECK_GL_ERROR();
    }
    else if (ActiveProgram == Simple_Prog)
    {
        if (DrawLinesBufferData.VertSize > 0)
            DrawSimpleGeometryVerts(DrawLineMode, DrawLinesBufferData.VertSize, GL_LINES, DrawLinesBufferData.LineFlags, DrawLinesBufferData.DrawColor, true);

        CHECK_GL_ERROR();
    }
    else if (ActiveProgram == Tile_Prog)
    {
        if (DrawTileBufferData.VertSize > 0)
            DrawTileVerts(DrawTileBufferData);

        CHECK_GL_ERROR();
    }

    if (SyncToDraw)
        glFinish();

    unguard;
}

void UXOpenGLRenderDevice::SetProgram( INT CurrentProgram )
{
	guard(UXOpenGLRenderDevice::SetProgram);
	CHECK_GL_ERROR();
    STAT(clockFast(Stats.ProgramCycles));

    if (ActiveProgram != CurrentProgram)
    {
        DrawProgram();
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        switch (CurrentProgram)
        {
            case No_Prog:
            {
                glBindVertexArray(0);
                glUseProgram(0);
                ActiveProgram = No_Prog;

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

                #ifndef __EMSCRIPTEN__
                if(UseAA && NoAATiles)
                    glEnable(GL_MULTISAMPLE);
                #endif

                glBindVertexArray(DrawGouraudPolyVertsVao);
                glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);

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

                #ifndef __EMSCRIPTEN__
                if(UseAA && NoAATiles)
                    glEnable(GL_MULTISAMPLE);
                #endif

                glBindVertexArray(DrawGouraudPolyVertListSingleBufferVao);
                glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertListBuffer);

                ActiveProgram = GouraudPolyVertList_Prog;

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
                break;
            }
            default:
            {
                ActiveProgram = CurrentProgram;
                break;
            }
        }
    }

	CHECK_GL_ERROR();
	STAT(unclockFast(Stats.ProgramCycles));
	unguard;
}

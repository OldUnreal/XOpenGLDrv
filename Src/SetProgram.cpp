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

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

void UXOpenGLRenderDevice::SetProgram( INT NextProgram )
{
	guard(UXOpenGLRenderDevice::SetProgram);
	CHECK_GL_ERROR();
    STAT(clockFast(Stats.ProgramCycles));

    if (ActiveProgram != NextProgram)
    {
    	// Flush the old program
        switch (ActiveProgram)
        {	        	        
	        case Simple_Prog:
	        {
                DrawSimpleEnd(NextProgram);
	            break;
	        }
	        case Tile_Prog:
	        {
                DrawTileEnd(NextProgram);
	            break;
	        }
	        case GouraudPolyVert_Prog:
	        case GouraudPolyVertList_Prog:
	        {
                DrawGouraudEnd(NextProgram);
	            break;
	        }
	        case ComplexSurfaceSinglePass_Prog:
	        {
                DrawComplexEnd(NextProgram);
	            break;
	        }
            case No_Prog:
	        default:
	        {
	            break;
	        }
        }

    	// Switch and initialize the new program
        PrevProgram = ActiveProgram;
        ActiveProgram = NextProgram;        

        switch (NextProgram)
        {
            case Simple_Prog:
            {
                DrawSimpleStart();
                break;
            }
            case Tile_Prog:
            {
                DrawTileStart();
                break;
            }
            case GouraudPolyVert_Prog:
            case GouraudPolyVertList_Prog:
            {
                DrawGouraudStart();
                break;
            }
            case ComplexSurfaceSinglePass_Prog:
            {
                DrawComplexStart();
                break;
            }
            case No_Prog:
            default:
            {
            	// stijn: is this really necessary?
            	//glBindBuffer(GL_ARRAY_BUFFER, 0);
                //glBindVertexArray(0);
                //glUseProgram(0);

                //CHECK_GL_ERROR();
                break;
            }
        }
    }
	
	STAT(unclockFast(Stats.ProgramCycles));
	unguard;
}

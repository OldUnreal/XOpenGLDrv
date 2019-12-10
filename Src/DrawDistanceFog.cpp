/*=============================================================================
	DrawDistanceFog.cpp: Unreal XOpenGL DistanceFog implementation.
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

void UXOpenGLRenderDevice::ResetFog()
{
	guard(UOpenGLRenderDevice::ResetFog);

	//Reset Fog
	DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	DistanceFogValues = glm::vec4(0.f,0.f,0.f,-1.f);

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalDistanceFogUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(DistanceFogColor));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(DistanceFogValues));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	CHECK_GL_ERROR();

	bFogEnabled = false;

	unguard;
}

void UXOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf)
{
	guard(UOpenGLRenderDevice::PreDrawGouraud);

	if(ActiveProgram == GouraudPolyVertList_Prog) //bZoneBasedFog Fog.
    {
        if (DrawGouraudListBufferData.VertSize > 0 &&
        ( DrawGouraudListBufferData.FogSurf.FogColor != FogSurf.FogColor
        || DrawGouraudListBufferData.FogSurf.FogMode != FogSurf.FogMode
        || DrawGouraudListBufferData.FogSurf.FogDistanceStart != FogSurf.FogDistanceStart
        || DrawGouraudListBufferData.FogSurf.FogDistanceEnd != FogSurf.FogDistanceEnd))
        {
            DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
        }
        DrawGouraudListBufferData.FogSurf = FogSurf;
    }
    else if(ActiveProgram == ComplexSurfaceSinglePass_Prog)
    {
        if (DrawGouraudBufferData.VertSize > 0 &&
        ( DrawGouraudBufferData.FogSurf.FogColor != FogSurf.FogColor
        || DrawGouraudBufferData.FogSurf.FogMode != FogSurf.FogMode
        || DrawGouraudBufferData.FogSurf.FogDistanceStart != FogSurf.FogDistanceStart
        || DrawGouraudBufferData.FogSurf.FogDistanceEnd != FogSurf.FogDistanceEnd))
        {
            DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
        }
        DrawGouraudBufferData.FogSurf = FogSurf;
    }

	DistanceFogColor = glm::vec4(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, FogSurf.FogColor.W);
	DistanceFogValues = glm::vec4(FogSurf.FogDistanceStart, FogSurf.FogDistanceEnd, FogSurf.FogDensity, (GLfloat)FogSurf.FogMode);

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalDistanceFogUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(DistanceFogColor));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(DistanceFogValues));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	CHECK_GL_ERROR();

	bFogEnabled = true;

	unguard;
}

void UXOpenGLRenderDevice::PostDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf)
{
	guard(UOpenGLRenderDevice::PostDrawGouraud);
#if ENGINE_VERSION==227

	DistanceFogColor = glm::vec4(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, FogSurf.FogColor.W);
	DistanceFogValues = glm::vec4(FogSurf.FogDistanceStart, FogSurf.FogDistanceEnd, FogSurf.FogDensity, (GLfloat)FogSurf.FogMode);

	if (!Frame->Viewport->Actor->bDistanceFogEnabled && !Frame->Viewport->Actor->Region.Zone->bDistanceFog)
		ResetFog();
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, GlobalDistanceFogUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(DistanceFogColor));
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(DistanceFogValues));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		CHECK_GL_ERROR();
	}

#endif // ENGINE_VERSION

	unguard;
}

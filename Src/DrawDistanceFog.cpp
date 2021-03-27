/*=============================================================================
	DrawDistanceFog.cpp: Unreal XOpenGL DistanceFog implementation.
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

void UXOpenGLRenderDevice::ResetFog()
{
	guard(UOpenGLRenderDevice::ResetFog);

	//Reset Fog
    DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
    DistanceFogValues = glm::vec4(0.f,0.f,0.f,-1.f);

    bFogEnabled = false;
	bFogUniformsStale = true;

	unguard;
}

void UXOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf)
{
	guard(UOpenGLRenderDevice::PreDrawGouraud);

    if (FogSurf.IsValid())
    {
        DistanceFogColor = glm::vec4(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, FogSurf.FogColor.W);
        DistanceFogValues = glm::vec4(FogSurf.FogDistanceStart, FogSurf.FogDistanceEnd, FogSurf.FogDensity, (GLfloat)(FogSurf.FogMode));        

        bFogEnabled = true;
        bFogUniformsStale = true;
    }
    else if (bFogEnabled)
        ResetFog();

	unguard;
}

void UXOpenGLRenderDevice::PostDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf)
{
	guard(UOpenGLRenderDevice::PostDrawGouraud);

#if ENGINE_VERSION==227
    ResetFog();
#endif // ENGINE_VERSION

	unguard;
}

void UXOpenGLRenderDevice::SetDistanceFogUniformData()
{
    glBindBuffer(GL_UNIFORM_BUFFER, GlobalDistanceFogUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(DistanceFogColor));
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(DistanceFogValues));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    CHECK_GL_ERROR();
	bFogUniformsStale = false;	
}


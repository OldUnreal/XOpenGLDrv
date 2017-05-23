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
	DrawGouraudFogSurface.FogColor = FPlane(1.f,1.f,1.f,1.f);
	DrawGouraudFogSurface.FogMode = 0;
	DrawGouraudFogSurface.FogDensity = 0.f;
	DrawGouraudFogSurface.FogDistanceEnd = 0.f;
	DrawGouraudFogSurface.FogDistanceStart = 0.f;

	DrawComplexFogSurface.FogColor = FPlane(1.f,1.f,1.f,1.f);
	DrawComplexFogSurface.FogMode = 0;
	DrawComplexFogSurface.FogDensity = 0.f;
	DrawComplexFogSurface.FogDistanceEnd = 0.f;
	DrawComplexFogSurface.FogDistanceStart = 0.f;

	unguard;
}


void UXOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf) 
{
	guard(UOpenGLRenderDevice::PreDrawGouraud);
	DrawGouraudFogSurface.FogColor = FogSurf.FogColor;
	DrawGouraudFogSurface.FogMode = FogSurf.FogMode;
	DrawGouraudFogSurface.FogDensity = FogSurf.FogDensity;
	DrawGouraudFogSurface.FogDistanceEnd = FogSurf.FogDistanceEnd;
	DrawGouraudFogSurface.FogDistanceStart = FogSurf.FogDistanceStart;

	DrawComplexFogSurface.FogColor = FogSurf.FogColor;
	DrawComplexFogSurface.FogMode = FogSurf.FogMode;
	DrawComplexFogSurface.FogDensity = FogSurf.FogDensity;
	DrawComplexFogSurface.FogDistanceEnd = FogSurf.FogDistanceEnd;
	DrawComplexFogSurface.FogDistanceStart = FogSurf.FogDistanceStart;
	unguard;
}

void UXOpenGLRenderDevice::PostDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf) 
{
	guard(UOpenGLRenderDevice::PostDrawGouraud);

	ResetFog();

	unguard;
}
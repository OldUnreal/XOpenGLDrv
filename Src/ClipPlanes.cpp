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

// Clipping planes!
BYTE UXOpenGLRenderDevice::PushClipPlane(const FPlane& Plane)
{
    guard(UXOpenGLRenderDevice::PushClipPlane);
	if (NumClipPlanes == MaxClippingPlanes)
		return 2;

	glEnable(GL_CLIP_DISTANCE0 + NumClipPlanes);
	glm::vec4 ClipParams = glm::vec4(NumClipPlanes, 1.f, 0.f, 0.f);
	glm::vec4 ClipPlane = glm::vec4(Plane.X, Plane.Y, Plane.Z, Plane.W);

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalClipPlaneUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(ClipParams));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(ClipPlane));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	CHECK_GL_ERROR();
	++NumClipPlanes;

	return 1;
	unguard;
}
BYTE UXOpenGLRenderDevice::PopClipPlane()
{
    guard(UXOpenGLRenderDevice::PopClipPlane);
	if (NumClipPlanes == 0)
		return 2;

	--NumClipPlanes;
	glDisable(GL_CLIP_DISTANCE0 + NumClipPlanes);

	glm::vec4 ClipParams = glm::vec4(NumClipPlanes, 0.f, 0.f, 0.f);
	glm::vec4 ClipPlane = glm::vec4(0.f, 0.f, 0.f, 0.f);

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalClipPlaneUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(ClipParams));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), sizeof(glm::vec4), glm::value_ptr(ClipPlane));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	CHECK_GL_ERROR();

	return 1;
	unguard;
}

/*=============================================================================
	PersistentBuffers.cpp: Handling for persisten GL buffers.
	Copyright 2014-2016 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

void UXOpenGLRenderDevice::LockBuffer(BufferRange& Buffer)
{
    guard(UXOpenGLRenderDevice::LockBuffer);

	if (Buffer.Sync)
		glDeleteSync(Buffer.Sync);

	Buffer.Sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	CHECK_GL_ERROR();

	unguard;
}

void UXOpenGLRenderDevice::WaitBuffer(BufferRange& Buffer)
{
    guard(UXOpenGLRenderDevice::WaitBuffer);

	GLbitfield WaitFlags = 0;
	GLuint64 WaitDuration = 0;
	INT Count = 0;
	GLenum WaitReturn = GL_UNSIGNALED;
	while (1)
	{
		WaitReturn = glClientWaitSync(Buffer.Sync, WaitFlags, WaitDuration);
		if (WaitReturn == GL_ALREADY_SIGNALED || WaitReturn == GL_CONDITION_SATISFIED)
			return;
		if (WaitReturn == GL_WAIT_FAILED)
		{
			GWarn->Logf(TEXT("XOpenGL: glClientWaitSync GL_WAIT_FAILED"));
			return;
		}
		//GWarn->Logf(TEXT("Wait! Count %i, %f %x %i"), Count, appSeconds().GetFloat(),WaitReturn, DrawTileIndex);
		WaitFlags = GL_SYNC_FLUSH_COMMANDS_BIT; //Failsafe, triple buffering should avoid this ever being set (expensive).
		WaitDuration = 100000;
		Count++;
	}
	CHECK_GL_ERROR();

	unguard;
}
void UXOpenGLRenderDevice::MapBuffers()
{
    guard(UXOpenGLRenderDevice::MapBuffers);

	GLbitfield PersistentBufferFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

	if (!bDrawGouraudBuffers)
	{
		debugf(TEXT("Mapping persistent DrawGouraudBuffer"));
		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBufferSingle);
		glBufferStorage(GL_ARRAY_BUFFER, 3 * DRAWGOURAUDPOLYLIST_SIZE, 0, PersistentBufferFlags);
		for (INT i = 0; i < 3; i++)
		{
			DrawGouraudBufferRange[i].DrawGouraudPMB = (float*)glMapBufferRange(GL_ARRAY_BUFFER, i*DRAWGOURAUDPOLYLIST_SIZE, DRAWGOURAUDPOLYLIST_SIZE, PersistentBufferFlags);
		}
		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBufferListSingle);
		glBufferStorage(GL_ARRAY_BUFFER, 3 * DRAWGOURAUDPOLYLIST_SIZE, 0, PersistentBufferFlags);
		for (INT i = 0; i < 3; i++)
		{
			DrawGouraudBufferRange[i].DrawGouraudListPMB = (float*)glMapBufferRange(GL_ARRAY_BUFFER, i*DRAWGOURAUDPOLYLIST_SIZE, DRAWGOURAUDPOLYLIST_SIZE, PersistentBufferFlags);
		}
		bDrawGouraudBuffers = true;
	}

	unguard;
}

/*=============================================================================
	PersistentBuffers.cpp: Handling for persistent GL buffers.
	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

void UXOpenGLRenderDevice::LockBuffer(BufferRange& Buffer, GLuint Index)
{
    guard(UXOpenGLRenderDevice::LockBuffer);

	if (Buffer.Sync[Index])
		glDeleteSync(Buffer.Sync[Index]);

	Buffer.Sync[Index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	CHECK_GL_ERROR();

	unguard;
}

void UXOpenGLRenderDevice::WaitBuffer(BufferRange& Buffer, GLuint Index)
{
    guard(UXOpenGLRenderDevice::WaitBuffer);

    CHECK_GL_ERROR();
	GLuint64 WaitDuration = 0;
	GLenum WaitReturn = GL_UNSIGNALED;
	if (Buffer.Sync[Index])
    {
        while (1)
        {
            WaitReturn = glClientWaitSync(Buffer.Sync[Index], GL_SYNC_FLUSH_COMMANDS_BIT, WaitDuration);
            CHECK_GL_ERROR();

            if (WaitReturn == GL_ALREADY_SIGNALED || WaitReturn == GL_CONDITION_SATISFIED)
                return;
            if (WaitReturn == GL_WAIT_FAILED)
            {
                GWarn->Logf(TEXT("XOpenGL: glClientWaitSync[%i] GL_WAIT_FAILED"),Index);
                return;
            }
            //GWarn->Logf(TEXT("Wait! Count %i, %f %x"), Count, appSeconds().GetFloat(),WaitReturn);
            Stats.StallCount++;
			WaitDuration = 500000;
        }
    }
	CHECK_GL_ERROR();
	unguard;
}
void UXOpenGLRenderDevice::MapBuffers()
{
    guard(UXOpenGLRenderDevice::MapBuffers);

    debugf(NAME_DevGraphics,TEXT("Mapping Buffers"));

	// These arrays are either fixed values or estimated based on logging of values on a couple of maps.
	// Static set to avoid using new/delete in every render pass. Should be more than sufficient.
	Draw2DLineVertsBuf = new FLOAT[18];
	Draw2DPointVertsBuf = new FLOAT[36];
	Draw3DLineVertsBuf = new FLOAT[18];
	EndFlashVertsBuf = new FLOAT[36];
	DrawLinesVertsBuf = new FLOAT[DRAWSIMPLE_SIZE]; // unexpectedly this size is easy to reach in UED.


	GLbitfield PersistentBufferFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
#ifndef __LINUX_ARM__
	if (UsePersistentBuffersGouraud)
    {
        // DrawGouraud
        debugf(TEXT("Mapping persistent DrawGouraudBuffer"));

        GLsizeiptr DrawGouraudBufferSize=(NUMBUFFERS * DRAWGOURAUDPOLY_SIZE * sizeof(float));

        glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
        glBufferStorage(GL_ARRAY_BUFFER, DrawGouraudBufferSize, 0, PersistentBufferFlags);
		DrawGouraudBufferRange.Buffer = (float*)glMapNamedBufferRange(DrawGouraudVertBuffer, 0, DrawGouraudBufferSize, PersistentBufferFlags);// | GL_MAP_UNSYNCHRONIZED_BIT);
		CHECK_GL_ERROR();

		GLsizeiptr DrawGouraudBufferListSize = (NUMBUFFERS * DRAWGOURAUDPOLYLIST_SIZE * sizeof(float));

        glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertListBuffer);
		glBufferStorage(GL_ARRAY_BUFFER, DrawGouraudBufferListSize, 0, PersistentBufferFlags);
		DrawGouraudListBufferRange.Buffer = (float*)glMapNamedBufferRange(DrawGouraudVertListBuffer, 0, DrawGouraudBufferListSize, PersistentBufferFlags);// | GL_MAP_UNSYNCHRONIZED_BIT);
		CHECK_GL_ERROR();
    }
	else
#endif
	{

		DrawGouraudBufferRange.Buffer = new FLOAT[DRAWGOURAUDPOLY_SIZE];
		DrawGouraudListBufferRange.Buffer = new FLOAT[DRAWGOURAUDPOLYLIST_SIZE];

		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
		glBufferData(GL_ARRAY_BUFFER, DRAWGOURAUDPOLY_SIZE * sizeof(float), DrawGouraudBufferRange.VertBuffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertListBuffer);
		glBufferData(GL_ARRAY_BUFFER, DRAWGOURAUDPOLYLIST_SIZE * sizeof(float), DrawGouraudListBufferRange.VertBuffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
#ifndef __LINUX_ARM__
    if (UsePersistentBuffersComplex)
    {
        // DrawComplexSurface
        debugf(TEXT("Mapping persistent DrawComplexSurfaceBuffer"));

        GLsizeiptr DrawComplexBufferSize=(NUMBUFFERS * DRAWCOMPLEX_SIZE * sizeof(float));

        glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
        glBufferStorage(GL_ARRAY_BUFFER, DrawComplexBufferSize, 0, PersistentBufferFlags);
		DrawComplexSinglePassRange.Buffer = (float*)glMapNamedBufferRange(DrawComplexVertBuffer, 0, DrawComplexBufferSize, PersistentBufferFlags);// | GL_MAP_UNSYNCHRONIZED_BIT);
		CHECK_GL_ERROR();
		glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
	else
#endif
	{
		glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
		DrawComplexSinglePassRange.Buffer = new FLOAT[DRAWCOMPLEX_SIZE];
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DRAWCOMPLEX_SIZE, DrawComplexSinglePassRange.Buffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
#ifndef __LINUX_ARM__
    if (UsePersistentBuffersTile)
    {
        // DrawComplexSurface
        debugf(TEXT("Mapping persistent DrawTileBuffer"));

        GLsizeiptr DrawTileBufferSize=(NUMBUFFERS * DRAWTILE_SIZE * sizeof(float));

        glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);
        glBufferStorage(GL_ARRAY_BUFFER, DrawTileBufferSize, 0, PersistentBufferFlags);
		DrawTileRange.Buffer = (float*)glMapNamedBufferRange(DrawTileVertBuffer, 0, DrawTileBufferSize, PersistentBufferFlags);// | GL_MAP_UNSYNCHRONIZED_BIT);
        CHECK_GL_ERROR();
    }
	else
#endif
	{
		glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);
		DrawTileRange.Buffer = new FLOAT[DRAWTILE_SIZE];
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DRAWTILE_SIZE, DrawTileRange.Buffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

    //Bindless textures
    if (UseBindlessTextures)
    {
        debugf(TEXT("Mapping persistent BindlessTexturesBuffer"));

        glBindBuffer(GL_UNIFORM_BUFFER, GlobalTextureHandlesUBO);
        glBufferStorage(GL_UNIFORM_BUFFER, sizeof(GLuint64) * NUMTEXTURES * 2, 0, PersistentBufferFlags);
        #ifndef __LINUX_ARM__
		GlobalUniformTextureHandles.UniformBuffer = (GLuint64*)glMapNamedBufferRange(GlobalTextureHandlesUBO, 0, sizeof(GLuint64) * NUMTEXTURES * 2, PersistentBufferFlags);// | GL_MAP_UNSYNCHRONIZED_BIT);
		#else
        GlobalUniformTextureHandles.UniformBuffer = (GLuint64*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(GLuint64) * NUMTEXTURES * 2, PersistentBufferFlags);// | GL_MAP_UNSYNCHRONIZED_BIT);
		#endif
        if (!GlobalUniformTextureHandles.UniformBuffer)
            GWarn->Logf(TEXT("Mapping of GlobalUniformTextureHandles failed!"));
        CHECK_GL_ERROR();
    }

    bMappedBuffers = true;

    CHECK_GL_ERROR();
	unguard;
}
void UXOpenGLRenderDevice::UnMapBuffers()
{
	guard(UXOpenGLRenderDevice::UnMapBuffers);

	debugf(NAME_DevGraphics,TEXT("UnMapping Buffers"));

	GLint IsMapped = 0;

	if (UseBindlessTextures)
	{
#ifndef __LINUX_ARM__
		if (UsePersistentBuffersGouraud)
		{
			glGetNamedBufferParameteriv(DrawGouraudVertBuffer, GL_BUFFER_MAPPED, &IsMapped);
			if (IsMapped == GL_TRUE)
			{
				glUnmapNamedBuffer(DrawGouraudVertBuffer);
				DrawGouraudBufferRange.Buffer = 0;
			}

			glGetNamedBufferParameteriv(DrawGouraudVertListBuffer, GL_BUFFER_MAPPED, &IsMapped);
			if (IsMapped == GL_TRUE)
			{
				glUnmapNamedBuffer(DrawGouraudVertListBuffer);
				DrawGouraudListBufferRange.Buffer = 0;
			}
		}
		if (UsePersistentBuffersComplex)
		{
			glGetNamedBufferParameteriv(DrawComplexVertBuffer, GL_BUFFER_MAPPED, &IsMapped);
			if (IsMapped == GL_TRUE)
			{
				glUnmapNamedBuffer(DrawComplexVertBuffer);
				DrawComplexSinglePassRange.Buffer = 0;
			}
		}
		if (UsePersistentBuffersTile)
		{
			glGetNamedBufferParameteriv(DrawTileVertBuffer, GL_BUFFER_MAPPED, &IsMapped);
			if (IsMapped == GL_TRUE)
			{
				glUnmapNamedBuffer(DrawTileVertBuffer);
				DrawTileRange.Buffer = 0;
			}
		}
		glGetNamedBufferParameteriv(GlobalTextureHandlesUBO, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(GlobalTextureHandlesUBO);
			GlobalUniformTextureHandles.UniformBuffer = 0;
		}
#else
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        GlobalUniformTextureHandles.UniformBuffer = 0;
#endif
	}
	else
	{
		if (DrawTileRange.Buffer)
			delete[] DrawTileRange.Buffer;

		if (DrawGouraudBufferRange.Buffer)
			delete[] DrawGouraudBufferRange.Buffer;

	#if ENGINE_VERSION==227
		if (DrawGouraudListBufferRange.Buffer)
			delete[] DrawGouraudListBufferRange.Buffer;
	#endif

		if (DrawComplexSinglePassRange.Buffer)
			delete[] DrawComplexSinglePassRange.Buffer;
	}

	if (Draw2DLineVertsBuf)
        delete[] Draw2DLineVertsBuf;

 	if (Draw2DPointVertsBuf)
        delete[] Draw2DPointVertsBuf;

	if (Draw3DLineVertsBuf)
        delete[] Draw3DLineVertsBuf;

	if (EndFlashVertsBuf)
        delete[] EndFlashVertsBuf;

	if (DrawLinesVertsBuf)
        delete[]  DrawLinesVertsBuf;

	bMappedBuffers = false;

	CHECK_GL_ERROR();

	unguard;
}

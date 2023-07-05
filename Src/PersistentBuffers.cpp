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

	if (!UsingPersistentBuffers && (&Buffer != &GlobalTextureHandlesRange || BindlessHandleStorage != STORE_UBO))
		return;

	if (Buffer.Sync[Index])
		glDeleteSync(Buffer.Sync[Index]);
	Buffer.Sync[Index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	CHECK_GL_ERROR();

	unguard;
}

void UXOpenGLRenderDevice::WaitBuffer(BufferRange& Buffer, GLuint Index)
{
    guard(UXOpenGLRenderDevice::WaitBuffer);

	if (!UsingPersistentBuffers && (&Buffer != &GlobalTextureHandlesRange || BindlessHandleStorage != STORE_UBO))
		return;

    CHECK_GL_ERROR();
	GLuint64 WaitDuration = 0;
	GLenum WaitReturn;
	if (Buffer.Sync[Index])
    {
        while (1)
        {
            WaitReturn = glClientWaitSync(Buffer.Sync[Index], GL_SYNC_FLUSH_COMMANDS_BIT, WaitDuration);
            CHECK_GL_ERROR();

			if (WaitReturn == GL_ALREADY_SIGNALED || WaitReturn == GL_CONDITION_SATISFIED)
			{
				return;
			}
            if (WaitReturn == GL_WAIT_FAILED)
            {
                GWarn->Logf(TEXT("XOpenGL: glClientWaitSync[%i] GL_WAIT_FAILED"),Index);
                return;
            }
            //GWarn->Logf(TEXT("Wait! Count %i, %f %x"), Count, appSeconds().GetFloat(),WaitReturn);
            //Stats.StallCount++;
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
	if (UsingPersistentBuffersGouraud)
    {
        // DrawGouraud
        debugf(NAME_DevGraphics, TEXT("Mapping persistent DrawGouraudBuffer"));

        GLsizeiptr DrawGouraudBufferSize = NUMBUFFERS * DRAWGOURAUDPOLY_SIZE * sizeof(FLOAT);

        glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
        glBufferStorage(GL_ARRAY_BUFFER, DrawGouraudBufferSize, 0, PersistentBufferFlags);
		DrawGouraudBufferRange.Buffer = (float*)glMapNamedBufferRange(DrawGouraudVertBuffer, 0, DrawGouraudBufferSize, PersistentBufferFlags);
		CHECK_GL_ERROR();

		if (UsingShaderDrawParameters)
		{
			GLsizeiptr DrawGouraudSSBOSize = NUMBUFFERS * MAX_DRAWGOURAUD_BATCH * sizeof(DrawGouraudShaderDrawParams);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBO);
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBOSize, NULL, PersistentBufferFlags);
			DrawGouraudSSBORange.Buffer = (float*)glMapNamedBufferRange(DrawGouraudSSBO, 0, DrawGouraudSSBOSize, PersistentBufferFlags);
			CHECK_GL_ERROR();
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
    }
	else
	{
		DrawGouraudBufferRange.Buffer = new FLOAT[DRAWGOURAUDPOLY_SIZE];
		glBindBuffer(GL_ARRAY_BUFFER, DrawGouraudVertBuffer);
		glBufferData(GL_ARRAY_BUFFER, DRAWGOURAUDPOLY_SIZE * sizeof(float), DrawGouraudBufferRange.VertBuffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (UsingShaderDrawParameters)
		{
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBO);
			DrawGouraudSSBORange.Buffer = (FLOAT*)new DrawGouraudShaderDrawParams[MAX_DRAWGOURAUD_BATCH];
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DrawGouraudShaderDrawParams) * MAX_DRAWGOURAUD_BATCH, DrawGouraudSSBORange.Buffer, GL_STREAM_DRAW);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}

    if (UsingPersistentBuffersComplex)
    {
        // DrawComplexSurface
        debugf(NAME_DevGraphics, TEXT("Mapping persistent DrawComplexSurfaceBuffer"));

        GLsizeiptr DrawComplexBufferSize=(NUMBUFFERS * DRAWCOMPLEX_SIZE * sizeof(float));

        glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
        glBufferStorage(GL_ARRAY_BUFFER, DrawComplexBufferSize, 0, PersistentBufferFlags);
		DrawComplexSinglePassRange.Buffer = (float*)glMapNamedBufferRange(DrawComplexVertBuffer, 0, DrawComplexBufferSize, PersistentBufferFlags);
		CHECK_GL_ERROR();
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (UsingShaderDrawParameters)
		{
			GLsizeiptr DrawComplexSSBOSize = NUMBUFFERS * MAX_DRAWCOMPLEX_BATCH * sizeof(DrawComplexShaderDrawParams);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBO);
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBOSize, NULL, PersistentBufferFlags);
			DrawComplexSSBORange.Buffer = (float*)glMapNamedBufferRange(DrawComplexSSBO, 0, DrawComplexSSBOSize, PersistentBufferFlags);
			CHECK_GL_ERROR();
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
    }
	else
	{
		glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
		DrawComplexSinglePassRange.Buffer = new FLOAT[DRAWCOMPLEX_SIZE];
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DRAWCOMPLEX_SIZE, DrawComplexSinglePassRange.Buffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (UsingShaderDrawParameters)
		{
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBO);
			DrawComplexSSBORange.Buffer = (FLOAT*)(new DrawComplexShaderDrawParams[MAX_DRAWCOMPLEX_BATCH]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DrawComplexShaderDrawParams) * MAX_DRAWCOMPLEX_BATCH, DrawComplexSSBORange.Buffer, GL_STREAM_DRAW);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}

    if (UsingPersistentBuffersTile)
    {
        // DrawComplexSurface
        debugf(NAME_DevGraphics, TEXT("Mapping persistent DrawTileBuffer"));

        GLsizeiptr DrawTileBufferSize = (NUMBUFFERS * DRAWTILE_SIZE * sizeof(float));

        glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);
        glBufferStorage(GL_ARRAY_BUFFER, DrawTileBufferSize, 0, PersistentBufferFlags);
		DrawTileRange.Buffer = (float*)glMapNamedBufferRange(DrawTileVertBuffer, 0, DrawTileBufferSize, PersistentBufferFlags);
        CHECK_GL_ERROR();
    }
	else
	{
		glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);
		DrawTileRange.Buffer = new FLOAT[DRAWTILE_SIZE];
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DRAWTILE_SIZE, DrawTileRange.Buffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

    // Bindless textures
    if (UsingBindlessTextures && (BindlessHandleStorage == STORE_UBO || BindlessHandleStorage == STORE_SSBO))
    {
		GLenum Target = (BindlessHandleStorage == STORE_UBO) ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
		if (BindlessHandleStorage == STORE_SSBO)
			PersistentBufferFlags = PersistentBufferFlags & ~GL_MAP_COHERENT_BIT;

		debugf(NAME_DevGraphics, TEXT("Mapping BindlessTexturesBuffer"));

		glBindBuffer(Target, GlobalTextureHandlesBufferObject);
		glBufferStorage(Target, sizeof(GLuint64)* MaxBindlessTextures * 2, 0, PersistentBufferFlags);
#ifndef __LINUX_ARM__
		GlobalTextureHandlesRange.Int64Buffer = (GLuint64*)glMapNamedBufferRange(GlobalTextureHandlesBufferObject, 0, sizeof(GLuint64) * MaxBindlessTextures * 2, PersistentBufferFlags);
#else
		GlobalTextureHandlesRange.Int64Buffer = (GLuint64*)glMapBufferRange(Target, 0, sizeof(GLuint64) * MaxBindlessTextures * 2, PersistentBufferFlags);
#endif
		if (!GlobalTextureHandlesRange.Int64Buffer)
		{
			GWarn->Logf(TEXT("Mapping of GlobalTextureHandlesRange failed! Disabling UsingBindlessTextures. Try reducing MaxBindlessTextures!"));
			UsingBindlessTextures = false;
		}
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

	if (UsingPersistentBuffersGouraud)
	{
		glGetNamedBufferParameteriv(DrawGouraudVertBuffer, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(DrawGouraudVertBuffer);
			DrawGouraudBufferRange.Buffer = nullptr;
		}

		glGetNamedBufferParameteriv(DrawGouraudSSBO, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(DrawGouraudSSBO);
			DrawGouraudSSBORange.Buffer = nullptr;
		}
		CHECK_GL_ERROR();
	}
	else
	{
		delete[] DrawGouraudBufferRange.Buffer;
		delete[] DrawGouraudSSBORange.Buffer;
	}

	if (UsingPersistentBuffersComplex)
	{
		glGetNamedBufferParameteriv(DrawComplexVertBuffer, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(DrawComplexVertBuffer);
			DrawComplexSinglePassRange.Buffer = nullptr;
		}

		glGetNamedBufferParameteriv(DrawComplexSSBO, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(DrawComplexSSBO);
			DrawComplexSSBORange.Buffer = nullptr;
		}
		CHECK_GL_ERROR();
	}
	else
	{
		delete[] DrawComplexSinglePassRange.Buffer;
		delete[] DrawComplexSSBORange.Buffer;
	}

	if (UsingPersistentBuffersTile)
	{
		glGetNamedBufferParameteriv(DrawTileVertBuffer, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(DrawTileVertBuffer);
			DrawTileRange.Buffer = nullptr;
		}
		CHECK_GL_ERROR();
	}
	else
	{
		delete[] DrawTileRange.Buffer;
	}

	if (UsingBindlessTextures && (BindlessHandleStorage == STORE_UBO || BindlessHandleStorage == STORE_SSBO))
	{
#ifndef __LINUX_ARM__
		glGetNamedBufferParameteriv(GlobalTextureHandlesBufferObject, GL_BUFFER_MAPPED, &IsMapped);
		if (IsMapped == GL_TRUE)
		{
			glUnmapNamedBuffer(GlobalTextureHandlesBufferObject);
			GlobalTextureHandlesRange.Int64Buffer = nullptr;
		}
		CHECK_GL_ERROR();
#else
		GLenum Target = (BindlessHandleStorage == STORE_UBO) ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
		glUnmapBuffer(Target);
		GlobalTextureHandlesRange.Int64Buffer = nullptr;
#endif
	}

	delete[] Draw2DLineVertsBuf;
	delete[] Draw2DPointVertsBuf;
	delete[] Draw3DLineVertsBuf;
    delete[] EndFlashVertsBuf;
	delete[] DrawLinesVertsBuf;

	bMappedBuffers = false;

	CHECK_GL_ERROR();

	unguard;
}

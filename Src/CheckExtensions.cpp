/*=============================================================================
CheckExtensions.cpp: Check if extensions are available.
Copyright 2014-2017 Oldunreal

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


UBOOL UXOpenGLRenderDevice::GLExtensionSupported(FString ExtensionName)
{
    #if SDL2BUILD
        return SDL_GL_ExtensionSupported(appToAnsi(*ExtensionName));
    #else
        return AllExtensions.InStr(*FString::Printf(TEXT("%ls "), *ExtensionName)) != -1;
    #endif
}

void UXOpenGLRenderDevice::CheckExtensions()
{
	guard(UXOpenGLRenderDevice::CheckExtensions);

	#ifdef __LINUX_ARM__
        if (GenerateMipMaps)
        {
            if (GLExtensionSupported(TEXT("GL_EXT_texture_storage"))  && GenerateMipMaps)
            {
                debugf(TEXT("XOpenGL: GL_EXT_texture_storage found. GenerateMipMaps enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_EXT_texture_storage not found. GenerateMipMaps disabled."));
                GenerateMipMaps = false;
            }
        }

        if (UseBindlessTextures)
        {
            if (GLExtensionSupported(TEXT("GL_IMG_bindless_texture")))
            {
                debugf(TEXT("XOpenGL: GL_IMG_bindless_texture found. UseBindlessTextures enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_IMG_bindless_texture not found. UseBindlessTextures disabled"));
                UseBindlessTextures = false;
            }
        }
        NVIDIAMemoryInfo = false; // found no such info available...yet?
        AMDMemoryInfo = false;
	#else
        if (UsePersistentBuffers)
        {
            if (GLExtensionSupported(TEXT("GL_ARB_buffer_storage")))
            {
                debugf(TEXT("XOpenGL: GL_ARB_buffer_storage found. UsePersistentBuffers enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_ARB_buffer_storage not found. UsePersistentBuffers and UseBindlessTextures disabled."));
                UsePersistentBuffers = false;
                UseBindlessTextures = false;
            }
        }

        if (UseBufferInvalidation)
        {
            if (GLExtensionSupported(TEXT("GL_ARB_invalidate_subdata")))
            {
                debugf(TEXT("XOpenGL: GL_ARB_invalidate_subdata found. UseBufferInvalidation enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_ARB_invalidate_subdata not found. UseBufferInvalidation disabled."));
                UseBufferInvalidation = false;
            }
        }

        if (GenerateMipMaps)
        {
            if (GLExtensionSupported(TEXT("GL_ARB_texture_storage")))
            {
                debugf(TEXT("XOpenGL: GL_ARB_texture_storage found. GenerateMipMaps enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_ARB_texture_storage not found. GenerateMipMaps disabled."));
                GenerateMipMaps = false;
            }
        }

        if (UseBindlessTextures)
        {
            if (GLExtensionSupported(TEXT("GL_ARB_bindless_texture")))
            {
                debugf(TEXT("XOpenGL: GL_ARB_bindless_texture found. UseBindlessTextures enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_ARB_bindless_texture not found. UseBindlessTextures disabled."));
                UseBindlessTextures = false;
            }
        }

		if (UseShaderDrawParameters)
		{
            if (GLExtensionSupported(TEXT("GL_ARB_shader_draw_parameters")))
            {
                debugf(TEXT("XOpenGL: GL_ARB_shader_draw_parameters found. UseShaderDrawParameters enabled."));
            }
            else
            {
                debugf(TEXT("XOpenGL: GL_ARB_shader_draw_parameters not found. UseShaderDrawParameters disabled."));
                UseShaderDrawParameters = false;
            }
		}

        if (GLExtensionSupported(TEXT("GL_NVX_gpu_memory_info")))
        {
            debugf(TEXT("XOpenGL: GL_NVX_gpu_memory_info found."));
            NVIDIAMemoryInfo = true;
        }
        else NVIDIAMemoryInfo = false;

        if (GLExtensionSupported(TEXT("GL_ATI_meminfo")))
        {
            debugf(TEXT("XOpenGL: GL_ATI_meminfo found."));
            AMDMemoryInfo = true;
        }
        else AMDMemoryInfo = false;

#ifndef SDL2BUILD // not worth the hassle with GLX, let SDL check if it works for now.
        if (GLExtensionSupported(TEXT("WGL_EXT_swap_control")))
        {
            debugf(TEXT("XOpenGL: WGL_EXT_swap_control found."));
            SwapControlExt = true;
        }
        else
        {
            debugf(TEXT("XOpenGL: WGL_EXT_swap_control not found. Can't set VSync options."));
            SwapControlExt = false;
        }

        if (GLExtensionSupported(TEXT("WGL_EXT_swap_control_tear")))
        {
            debugf(TEXT("XOpenGL: WGL_EXT_swap_control_tear found."));
            SwapControlTearExt = true;
        }
        else
        {
            debugf(TEXT("WGL_EXT_swap_control_tear is not supported by device."));
            SwapControlTearExt = false;
        }
#endif

        INT MaxTextureImageUnits = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits);
        debugf(TEXT("XOpenGL: MaxTextureImageUnits: %i"), MaxTextureImageUnits);

        INT MaxImageUnits = 0;
        glGetIntegerv(GL_MAX_IMAGE_UNITS, &MaxImageUnits);
        debugf(TEXT("XOpenGL: MaxImageUnits: %i"), MaxImageUnits);
	#endif

    if (GLExtensionSupported(TEXT("GL_KHR_debug")) && UseOpenGLDebug)
    {
        GWarn->Logf(TEXT("OpenGL debugging extension found!"));
    }
    else if (UseOpenGLDebug)
    {
        GWarn->Logf(TEXT("OpenGL debugging extension not found! Disabling UseOpenGLDebug"));
        UseOpenGLDebug = 0;
    }

	INT MaxCombinedTextureImageUnits = 0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &MaxCombinedTextureImageUnits);
	debugf(TEXT("XOpenGL: MaxCombinedTextureImageUnits: %i"), MaxCombinedTextureImageUnits);

	INT MaxElementsVertices = 0;
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &MaxElementsVertices);
	debugf(TEXT("XOpenGL: MaxElementsVertices: %i"), MaxElementsVertices);

	INT MaxUniformBufferBindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &MaxUniformBufferBindings);
	debugf(TEXT("XOpenGL: MaxUniformBufferBindings: %i"), MaxUniformBufferBindings);

	if (OpenGLVersion == GL_Core)
	{
		INT MaxDualSourceDrawBuffers;
		glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &MaxDualSourceDrawBuffers);
		debugf(TEXT("XOpenGL: MaxDualSourceDrawBuffers: %i"), MaxDualSourceDrawBuffers);
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTextureSize);
	debugf(TEXT("XOpenGL: MaxTextureSize: %i"), MaxTextureSize);

	if (!GL_EXT_texture_filter_anisotropic)
	{
		debugf(TEXT("XOpenGL: Anisotropic filter extension not found!"));
		MaxAnisotropy = 0.f;
	}

	if (!GL_EXT_texture_lod_bias)
	{
		debugf(TEXT("XOpenGL: Texture lod bias extension not found!"));
		LODBias = 0;
	}

	if (MaxAnisotropy < 0)
	{
		MaxAnisotropy = 0;
	}

	if (MaxAnisotropy)
	{
		FLOAT tmp;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &tmp);

		if (tmp <= 0.f)
			tmp = 0.f; // seems in Linux ARM with ODROID-XU4 the extension check fails. Better safe than sorry.

		debugf(TEXT("XOpenGL: MaxAnisotropy = (%f/%f)"), MaxAnisotropy, tmp);

		if (MaxAnisotropy > tmp)
			MaxAnisotropy = tmp;

		UseTrilinear = true; // Anisotropic filtering doesn't make much sense without trilinear filtering
	}

	if (NumAASamples < 0)
	{
		NumAASamples = 0;
	}

	if (NumAASamples)
	{
		INT NumberOfAASamples = 0, MaxAASamples;
		glGetIntegerv(GL_MAX_SAMPLES, &MaxAASamples);
		glGetIntegerv(GL_SAMPLES, &NumberOfAASamples);
		debugf(TEXT("XOpenGL: NumAASamples: (%i/%i)"), NumberOfAASamples, MaxAASamples);

		if (NumAASamples>MaxAASamples)
			NumAASamples = MaxAASamples;
	}

	if (NumAASamples < 2 && UseAA)
		NumAASamples = 2;

	if (GenerateMipMaps && !UsePrecache)
	{
		debugf(TEXT("XOpenGL: Enabling UsePrecache for GenerateMipMaps."));
		UsePrecache = 1;
	}

    // Extensions
    NumberOfExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
    debugf(TEXT("XOpenGL: GL_NUM_EXTENSIONS found: %i"), NumberOfExtensions);

    // Clipping Planes
    MaxClippingPlanes = 0;
    glGetIntegerv(GL_MAX_CLIP_DISTANCES, &MaxClippingPlanes);
    debugf(TEXT("XOpenGL: GL_MAX_CLIP_DISTANCES found: %i"), MaxClippingPlanes);


	CHECK_GL_ERROR();

	unguard;
}

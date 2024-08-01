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
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_EXT_texture_storage found. GenerateMipMaps enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_EXT_texture_storage not found. GenerateMipMaps disabled."));
            GenerateMipMaps = false;
        }
    }

    if (UseBindlessTextures)
    {
        if (GLExtensionSupported(TEXT("GL_IMG_bindless_texture")))
        {
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_IMG_bindless_texture found. UseBindlessTextures enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_IMG_bindless_texture not found. UseBindlessTextures disabled"));
            UseBindlessTextures = false;
        }
    }
	//usually we would assume this extension to be supported in general, but it seems not every driver really does in ES mode. (RasPi, AMD Radeon R5 Graphics, ???)
    if (GLExtensionSupported(TEXT("GL_EXT_clip_cull_distance")) || GLExtensionSupported(TEXT("GL_ARB_cull_distance")))
    {
        debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_cull_distance / GL_EXT_clip_cull_distance found."));
    }
    else
    {
        GWarn->Logf(TEXT("XOpenGL: GL_ARB_cull_distance / GL_EXT_clip_cull_distance not found."));
        SupportsClipDistance = false; // have to disable this functionality.
    }

    SupportsNVIDIAMemoryInfo = false; // found no such info available...yet?
    SupportsAMDMemoryInfo = false;
#else
    if (UsePersistentBuffers)
    {
        if (GLExtensionSupported(TEXT("GL_ARB_buffer_storage")))
        {
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_buffer_storage found. UsePersistentBuffers enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_ARB_buffer_storage not found. UsePersistentBuffers disabled."));
            UsePersistentBuffers = false;
        }
    }

    if (UseBufferInvalidation)
    {
        if (GLExtensionSupported(TEXT("GL_ARB_invalidate_subdata")))
        {
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_invalidate_subdata found. UseBufferInvalidation enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_ARB_invalidate_subdata not found. UseBufferInvalidation disabled."));
            UseBufferInvalidation = false;
        }
    }

    if (GenerateMipMaps)
    {
        if (GLExtensionSupported(TEXT("GL_ARB_texture_storage")))
        {
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_texture_storage found. GenerateMipMaps enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_ARB_texture_storage not found. GenerateMipMaps disabled."));
            GenerateMipMaps = false;
        }
    }

    if (UseBindlessTextures)
    {
        if (GLExtensionSupported(TEXT("GL_ARB_bindless_texture")))
        {
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_bindless_texture found. UseBindlessTextures enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_ARB_bindless_texture not found. UseBindlessTextures disabled."));
            UseBindlessTextures = false;
        }
    }

    if (GLExtensionSupported(TEXT("GL_ARB_shader_storage_buffer_object")))
    {
        SupportsSSBO = true;
    }    

	if (GLExtensionSupported(TEXT("GL_ARB_gpu_shader_int64")))
	{
        SupportsGLSLInt64 = true;
	}

    //usually we would assume this extension to be supported in general, but it seems not every driver really does in ES mode. (RasPi, AMD Radeon R5 Graphics, ???)
    if (GLExtensionSupported(TEXT("GL_EXT_clip_cull_distance")) || GLExtensionSupported(TEXT("GL_ARB_cull_distance")))
    {
        debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_cull_distance / GL_EXT_clip_cull_distance found."));
    }
    else
    {
        GWarn->Logf(TEXT("XOpenGL: GL_ARB_cull_distance / GL_EXT_clip_cull_distance not found."));
        SupportsClipDistance = false; // have to disable this functionality.
    }

    if (GLExtensionSupported(TEXT("GL_NVX_gpu_memory_info")))
    {
        debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_NVX_gpu_memory_info found."));
        SupportsNVIDIAMemoryInfo = true;
    }
    else SupportsNVIDIAMemoryInfo = false;

    if (GLExtensionSupported(TEXT("GL_ATI_meminfo")))
    {
        debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ATI_meminfo found."));
        SupportsAMDMemoryInfo = true;
    }
    else SupportsAMDMemoryInfo = false;

    if (UseShaderDrawParameters)
    {
        if (GLExtensionSupported(TEXT("GL_ARB_shader_draw_parameters")) && SupportsSSBO)
        {
            debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_ARB_shader_draw_parameters and GL_ARB_shader_storage_buffer_object found. UseShaderDrawParameters enabled."));
        }
        else
        {
            GWarn->Logf(TEXT("XOpenGL: GL_ARB_shader_draw_parameters or GL_ARB_shader_storage_buffer_object not found. UseShaderDrawParameters disabled."));
            UseShaderDrawParameters = false;
        }

        glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &MaxSSBOBlockSize);
    }

# ifndef SDL2BUILD // not worth the hassle with GLX, let SDL check if it works for now.
    if (GLExtensionSupported(TEXT("WGL_EXT_swap_control")))
    {
        debugf(NAME_DevGraphics, TEXT("XOpenGL: WGL_EXT_swap_control found."));
        SupportsSwapControl = true;
    }
    else
    {
        GWarn->Logf(TEXT("XOpenGL: WGL_EXT_swap_control not found. Can't set VSync options."));
        SupportsSwapControl = false;
    }

    if (GLExtensionSupported(TEXT("WGL_EXT_swap_control_tear")))
    {
        debugf(NAME_DevGraphics, TEXT("XOpenGL: WGL_EXT_swap_control_tear found."));
        SupportsSwapControlTear = true;
    }
    else
    {
        GWarn->Logf(TEXT("WGL_EXT_swap_control_tear is not supported by device."));
        SupportsSwapControlTear = false;
    }
# else
    // Just some additional info to check on...
    INT r=0, g=0, b=0, a=0, db=0, srgb=0, dbu=0;
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: SDL_GL RED_SIZE:%i GREEN_SIZE:%i BLUE_SIZE:%i ALPHA_SIZE:%i"),r,g,b,a);

    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &db);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: SDL_GL_DEPTH_SIZE DesiredDepthBits: %i, provided: %i"),DesiredDepthBits, db);

    if (UseSRGBTextures)
    {
        SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &srgb);
        debugf(NAME_DevGraphics, TEXT("XOpenGL: SDL_GL_FRAMEBUFFER_SRGB_CAPABLE: %i"),srgb);
    }
# endif

    INT MaxTextureImageUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxTextureImageUnits: %i"), MaxTextureImageUnits);

    INT MaxVertexTextureImageUnits = 0;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &MaxVertexTextureImageUnits);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxVertexTextureImageUnits: %i"), MaxVertexTextureImageUnits);

    INT MaxImageUnits = 0;
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &MaxImageUnits);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxImageUnits: %i"), MaxImageUnits);
#endif

    if (GLExtensionSupported(TEXT("GL_KHR_debug")) && UseOpenGLDebug)
    {
        GWarn->Logf(TEXT("XOpenGL: OpenGL debugging extension found!"));
    }
    else if (UseOpenGLDebug)
    {
        GWarn->Logf(TEXT("XOpenGL: OpenGL debugging extension not found! Disabling UseOpenGLDebug"));
        UseOpenGLDebug = 0;
    }

	INT MaxCombinedTextureImageUnits = 0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &MaxCombinedTextureImageUnits);
	debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxCombinedTextureImageUnits: %i"), MaxCombinedTextureImageUnits);

	INT MaxElementsVertices = 0;
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &MaxElementsVertices);
	debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxElementsVertices: %i"), MaxElementsVertices);

	INT MaxUniformBufferBindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &MaxUniformBufferBindings);
	debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxUniformBufferBindings: %i"), MaxUniformBufferBindings);

	if (OpenGLVersion == GL_Core)
	{
		INT MaxDualSourceDrawBuffers;
		glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &MaxDualSourceDrawBuffers);
		debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxDualSourceDrawBuffers: %i"), MaxDualSourceDrawBuffers);
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTextureSize);
	debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxTextureSize: %i"), MaxTextureSize);

	if (!GLExtensionSupported(TEXT("GL_EXT_texture_lod_bias")))
	{
		GWarn->Logf(TEXT("XOpenGL: Texture lod bias extension not found!"));
		LODBias = 0;
	}

    if (!GLExtensionSupported(TEXT("GL_EXT_texture_compression_s3tc")))
	{
		GWarn->Logf(TEXT("XOpenGL: GL_EXT_texture_compression_s3tc extension not found!"));
        SupportsS3TC = false;
	}

    if (UseSRGBTextures && !GLExtensionSupported(TEXT("GL_EXT_texture_sRGB")) && !GLExtensionSupported(TEXT("GL_EXT_sRGB"))) //GL_EXT_sRGB for ES
	{
		GWarn->Logf(TEXT("XOpenGL: GL_EXT_texture_sRGB extension not found, UseSRGBTextures disabled!"));
		UseSRGBTextures = 0;
	}

    if (!GLExtensionSupported(TEXT("GL_EXT_texture_filter_anisotropic")))
	{
		GWarn->Logf(TEXT("XOpenGL: Anisotropic filter extension not found!"));
		MaxAnisotropy = 0.f;
	}
	if (MaxAnisotropy > 0.f)
	{
		FLOAT tmp;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &tmp);

		if (tmp < 0.f)
			tmp = 0.f; // seems in Linux ARM with ODROID-XU4 the extension check fails. Better safe than sorry.

		debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxAnisotropy = (%f/%f)"), MaxAnisotropy, tmp);

		if (MaxAnisotropy > tmp)
			MaxAnisotropy = tmp;

		UseTrilinear = true; // Anisotropic filtering doesn't make much sense without trilinear filtering
	}

	if (UseAA)
	{
        if (NumAASamples < 2)
        {
            debugf(TEXT("XOpenGL: NumAASamples was set < 2 but UseAA enabled, increasing to minimum value of 2"));
            NumAASamples = 2;
        }

        INT MaxAASamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &MaxAASamples);
        if (NumAASamples>MaxAASamples)
        {
            debugf(TEXT("XOpenGL: NumAASamples was set > maximum samples supported, setting to %i"),MaxAASamples);
			NumAASamples = MaxAASamples;
        }

		INT NumberOfAASamples = 0;
#ifdef SDL2BUILD
        INT AABuffers = 0;
        SDL_GL_GetAttribute( SDL_GL_MULTISAMPLEBUFFERS, &AABuffers );
        SDL_GL_GetAttribute( SDL_GL_MULTISAMPLESAMPLES, &NumberOfAASamples );
        debugf(NAME_DevGraphics, TEXT("XOpenGL: SDL_GL_MULTISAMPLEBUFFERS: %i, requested NumAASamples: %i, provided NumAASamples/MaxSamples: (%i/%i)"), AABuffers, NumAASamples, NumberOfAASamples, MaxAASamples);
#else
        glGetIntegerv(GL_MAX_SAMPLES, &MaxAASamples);
		glGetIntegerv(GL_SAMPLES, &NumberOfAASamples);
		debugf(NAME_DevGraphics, TEXT("XOpenGL: NumAASamples: (%i/%i)"), NumberOfAASamples, MaxAASamples);
#endif
	}

	if (GenerateMipMaps && !UsePrecache)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: Enabling UsePrecache for GenerateMipMaps."));
		UsePrecache = 1;
	}

    // Extensions
    NumberOfExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_NUM_EXTENSIONS found: %i"), NumberOfExtensions);

    // Clipping Planes
    MaxClippingPlanes = 0;
    glGetIntegerv(GL_MAX_CLIP_DISTANCES, &MaxClippingPlanes);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: GL_MAX_CLIP_DISTANCES found: %i"), MaxClippingPlanes);

    MaxUniformBlockSize = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &MaxUniformBlockSize); //Check me!!! For whatever reason this appears to return on (some?) AMD drivers a value of 572657868
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxUniformBlockSize: %i"), MaxUniformBlockSize);

    INT MaxOutputComponents = 0;
    glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &MaxOutputComponents);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxVertexOutputComponents: %i"), MaxOutputComponents);

    INT MaxVaryingComponents = 0;
    glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &MaxVaryingComponents);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxVaryingComponents: %i"), MaxVaryingComponents);

    INT MaxVaryingVectors = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &MaxVaryingVectors);
    debugf(NAME_DevGraphics, TEXT("XOpenGL: MaxVaryingVectors: %i"), MaxVaryingVectors);

	CHECK_GL_ERROR();

	unguard;
}
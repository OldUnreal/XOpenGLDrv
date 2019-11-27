/*=============================================================================
CheckExtensions.cpp: Check if extensions are available.
Copyright 2014-2017 Oldunreal

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

#ifdef WINBUILD
    UBOOL UXOpenGLRenderDevice::GLExtensionSupported(FString Extension_Name)
    {
        // pointer to function which returns pointer to string with list of all wgl extensions
        PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtensionsStringEXT = nullptr;

        // determine pointer to function
        wglGetExtensionsStringEXT = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGEXTPROC>(wglGetProcAddress("wglGetExtensionsStringEXT"));
        if (appStrstr(appFromAnsi(wglGetExtensionsStringEXT()), *Extension_Name) == 0)
            return 0;

        // extension supported
        return 1;
    }
#elif SDL2BUILD
    UBOOL UXOpenGLRenderDevice::GLExtensionSupported(FString Extension_Name)
    {
        // extension supported
        return SDL_GL_ExtensionSupported(appToAnsi(*Extension_Name));
    }
#endif

void UXOpenGLRenderDevice::CheckExtensions()
{
	guard(UXOpenGLRenderDevice::CheckExtensions);

	if (!GLExtensionSupported(TEXT("GL_ARB_buffer_storage")) && UsePersistentBuffers)
	{
		debugf(TEXT("XOpenGL: GL_ARB_buffer_storage not found. Disabling UsePersistentBuffers."));
		UsePersistentBuffers = 0;
	}

	if (!GLExtensionSupported(TEXT("GL_ARB_invalidate_subdata")) && UseBufferInvalidation)
	{
		debugf(TEXT("XOpenGL: GL_ARB_invalidate_subdata not found. Disabling UseBufferInvalidation."));
		UseBufferInvalidation = 0;
	}

	#ifdef __LINUX_ARM__
        if (!GLExtensionSupported(TEXT("GL_EXT_texture_storage"))  && GenerateMipMaps)
        {
            debugf(TEXT("XOpenGL: GL_EXT_texture_storage not found. Disabling GenerateMipMaps."));
            GenerateMipMaps = 0;
        }
        if (!GLExtensionSupported(TEXT("GL_IMG_bindless_texture"))  && UseBindlessTextures)
        {
            debugf(TEXT("XOpenGL: GL_IMG_bindless_texture not found. Disabling UseBindlessTextures."));
            UseBindlessTextures = 0;
        }
        NVIDIAMemoryInfo = 0; // found no such info available...yet?
        AMDMemoryInfo = 0;
	#else
        if (!GLExtensionSupported(TEXT("GL_ARB_texture_storage")) && GenerateMipMaps)
        {
            debugf(TEXT("XOpenGL: GL_ARB_texture_storage not found. Disabling GenerateMipMaps."));
            GenerateMipMaps = 0;
        }
        if (!GLExtensionSupported(TEXT("GL_ARB_bindless_texture")) && UseBindlessTextures)
        {
            debugf(TEXT("XOpenGL: GL_ARB_bindless_texture not found. Disabling UseBindlessTextures."));
            UseBindlessTextures = 0;
        }
        if (GLExtensionSupported(TEXT("GL_NVX_gpu_memory_info")))
        {
            debugf(TEXT("XOpenGL: GL_NVX_gpu_memory_info found."));
            NVIDIAMemoryInfo = 1;
        }
        if (GLExtensionSupported(TEXT("GL_ATI_meminfo")))
        {
            debugf(TEXT("XOpenGL: GL_ATI_meminfo found."));
            AMDMemoryInfo = 1;
        }
        if (GLExtensionSupported(TEXT("WGL_EXT_swap_control")))
        {
            debugf(TEXT("XOpenGL: WGL_EXT_swap_control found."));
            SwapControlExt = 1;
        }
        if (GLExtensionSupported(TEXT("WGL_EXT_swap_control_tear")))
        {
            debugf(TEXT("XOpenGL: WGL_EXT_swap_control_tear found."));
            SwapControlTearExt = 1;
        }

        INT MaxTextureImageUnits = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits);
        debugf(TEXT("XOpenGL: MaxTextureImageUnits: %i"), MaxTextureImageUnits);

        INT MaxImageUnits = 0;
        glGetIntegerv(GL_MAX_IMAGE_UNITS, &MaxImageUnits);
        debugf(TEXT("XOpenGL: MaxImageUnits: %i"), MaxImageUnits);
	#endif
	if (UseBindlessTextures && !UsePersistentBuffers)
	{
		if (!GLExtensionSupported(TEXT("GL_ARB_buffer_storage")))
		{
			debugf(TEXT("XOpenGL: Missing GL_ARB_buffer_storage. Disabling UseBindlessTextures."));
			UseBindlessTextures = 0;
		}
	}
    if (!GLExtensionSupported(TEXT("GL_KHR_debug")))
    {
        GWarn->Logf(TEXT("OpenGL debugging extension not found!"));
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
		float tmp;
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
		int NumberOfAASamples = 0, MaxAASamples;
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

	CHECK_GL_ERROR();

	unguard;
}

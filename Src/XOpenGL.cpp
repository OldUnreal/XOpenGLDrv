/*=============================================================================
XOpenGL.cpp: Unreal XOpenGL implementation for OpenGL 3.3+ and GL ES 3.0+

Copyright 2014-2019 Oldunreal

Revision history:
* Created by Smirftsch
* lots of experience and ideas from UTGLR OpenGL by Chris Dohnal
* improved texture handling code by Sebastian Kaufel
* information, ideas, additions by Sebastian Kaufel
* UED selection code by Sebastian Kaufel
* TOpenGLMap tempate based on TMap template using a far superior hash function by Sebastian Kaufel
* Added persistent buffers. However, these seem to be only fast at a given draw buffer size (currently only DrawGouraud).
* Bumpmap and Heightmap support
* Passing DrawFlags and TextureFormat to shader
Todo:
* fixes, cleanups, (AZDO) optimizations, etc, etc, etc.
=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"
#include "UnShader.h"

#ifdef _WIN32
#include <comutil.h>
#endif

#ifdef __LINUX__
#define SDL2BUILD 1
#elif MACOSX
#define SDL2BUILD 1
#elif defined(__EMSCRIPTEN__)
#define SDL2BUILD 1
#endif


/*-----------------------------------------------------------------------------
UXOpenGLDrv.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UXOpenGLRenderDevice);

#if ENGINE_VERSION!=227
#ifdef _WIN32
#pragma comment(lib,"Winmm")
void TimerBegin()
{
	// This will increase the precision of the kernel interrupt
	// timer. Although this will slightly increase resource usage
	// this will also increase the precision of sleep calls and
	// this will in turn increase the stability of the framerate
	timeBeginPeriod(1);
}
void TimerEnd()
{
	// Restore the kernel timer config
	timeEndPeriod(1);
}
#endif
#endif

void UXOpenGLRenderDevice::StaticConstructor()
{
	guard(UXOpenGLRenderDevice::StaticConstructor);
	UEnum* VSyncs = new(GetClass(), TEXT("VSyncs"))UEnum(NULL);

	// Enum members.
	new(VSyncs->Names)FName(TEXT("Off"));
	new(VSyncs->Names)FName(TEXT("On"));
	new(VSyncs->Names)FName(TEXT("Adaptive"));

	UEnum* OpenGLVersions = new(GetClass(), TEXT("OpenGLVersions"))UEnum(NULL);
	new(OpenGLVersions->Names)FName(TEXT("Core"));
	new(OpenGLVersions->Names)FName(TEXT("ES"));

	new(GetClass(), TEXT("OpenGLVersion"), RF_Public)UByteProperty(CPP_PROPERTY(OpenGLVersion), TEXT("Options"), CPF_Config, OpenGLVersions);
	new(GetClass(), TEXT("UseVSync"), RF_Public)UByteProperty(CPP_PROPERTY(UseVSync), TEXT("Options"), CPF_Config, VSyncs);
	new(GetClass(), TEXT("RefreshRate"), RF_Public)UIntProperty(CPP_PROPERTY(RefreshRate), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NumAASamples"), RF_Public)UIntProperty(CPP_PROPERTY(NumAASamples), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetScreenshots"), RF_Public)UFloatProperty(CPP_PROPERTY(GammaOffsetScreenshots), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("LODBias"), RF_Public)UFloatProperty(CPP_PROPERTY(LODBias), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("MaxAnisotropy"), RF_Public)UFloatProperty(CPP_PROPERTY(MaxAnisotropy), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoFiltering"), RF_Public)UBoolProperty(CPP_PROPERTY(NoFiltering), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("ShareLists"), RF_Public)UBoolProperty(CPP_PROPERTY(ShareLists), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("AlwaysMipmap"), RF_Public)UBoolProperty(CPP_PROPERTY(AlwaysMipmap), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public)UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseTrilinear"), RF_Public)UBoolProperty(CPP_PROPERTY(UseTrilinear), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseAA"), RF_Public)UBoolProperty(CPP_PROPERTY(UseAA), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GammaCorrectScreenshots"), RF_Public)UBoolProperty(CPP_PROPERTY(GammaCorrectScreenshots), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("MacroTextures"), RF_Public)UBoolProperty(CPP_PROPERTY(MacroTextures), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("BumpMaps"), RF_Public)UBoolProperty(CPP_PROPERTY(BumpMaps), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoAATiles"), RF_Public)UBoolProperty(CPP_PROPERTY(NoAATiles), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GenerateMipMaps"), RF_Public)UBoolProperty(CPP_PROPERTY(GenerateMipMaps), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("SyncToDraw"), RF_Public)UBoolProperty(CPP_PROPERTY(SyncToDraw), TEXT("Options"), CPF_Config);


#if ENGINE_VERSION==227
	new(GetClass(), TEXT("UseHWLighting"), RF_Public)UBoolProperty(CPP_PROPERTY(UseHWLighting), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseHWClipping"), RF_Public)UBoolProperty(CPP_PROPERTY(UseHWClipping), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseEnhancedLightmaps"), RF_Public)UBoolProperty(CPP_PROPERTY(UseEnhancedLightmaps), TEXT("Options"), CPF_Config);
	//new(GetClass(),TEXT("UseMeshBuffering"),		RF_Public)UBoolProperty	( CPP_PROPERTY(UseMeshBuffering			), TEXT("Options"), CPF_Config);
#endif

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	// OpenGL 4
	new(GetClass(), TEXT("UsePersistentBuffers"), RF_Public)UBoolProperty(CPP_PROPERTY(UsePersistentBuffers), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseBindlessTextures"), RF_Public)UBoolProperty(CPP_PROPERTY(UseBindlessTextures), TEXT("Options"), CPF_Config);

	// Debug Options
	new(GetClass(), TEXT("DebugLevel"), RF_Public)UIntProperty(CPP_PROPERTY(DebugLevel), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("UseOpenGLDebug"), RF_Public)UBoolProperty(CPP_PROPERTY(UseOpenGLDebug), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("NoBuffering"), RF_Public)UBoolProperty(CPP_PROPERTY(NoBuffering), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("NoDrawComplexSurface"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawComplexSurface), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("NoDrawGouraud"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawGouraud), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("NoDrawGouraudList"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawGouraudList), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("NoDrawTile"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawTile), TEXT("DebugOptions"), CPF_Config);
	new(GetClass(), TEXT("NoDrawSimple"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawSimple), TEXT("DebugOptions"), CPF_Config);
#else
	new(GetClass(), TEXT("DebugLevel"), RF_Public)UIntProperty(CPP_PROPERTY(DebugLevel), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseOpenGLDebug"), RF_Public)UBoolProperty(CPP_PROPERTY(UseOpenGLDebug), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoBuffering"), RF_Public)UBoolProperty(CPP_PROPERTY(NoBuffering), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoDrawComplexSurface"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawComplexSurface), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoDrawGouraud"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawGouraud), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoDrawTile"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawTile), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoDrawSimple"), RF_Public)UBoolProperty(CPP_PROPERTY(NoDrawSimple), TEXT("Options"), CPF_Config);
#endif
	new(GetClass(), TEXT("UseBufferInvalidation"), RF_Public)UBoolProperty(CPP_PROPERTY(UseBufferInvalidation), TEXT("Options"), CPF_Config);

	//new(GetClass(),TEXT("EnableShadows"),			RF_Public)UBoolProperty ( CPP_PROPERTY(EnableShadows			), TEXT("Options"), CPF_Config);

	// Defaults.
	RefreshRate = 60;
	NumAASamples = 4;
	GammaOffsetScreenshots = 0.7f;
	LODBias = 0.f;
	MaxAnisotropy = 4.f;
	UseHWClipping = 1;
	UsePrecache = 1;
	ShareLists = 1;
	UseAA = 1;
	GammaCorrectScreenshots = 1;
	MacroTextures = 1;
	BumpMaps = 1;
	UseTrilinear = 1;
	NoAATiles = 1;
	UseMeshBuffering = 0; //Buffer (Static)Meshes for drawing.
#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	UseBindlessTextures = 1;
#else
	UseBindlessTextures = 0;
#endif
	UseHWLighting = 0;
	AlwaysMipmap = 0;
	NoFiltering = 0;
	UseSRGBTextures = 0;
	EnvironmentMaps = 0;
	GenerateMipMaps = 0;
	//EnableShadows = 0;
	UseEnhancedLightmaps = 0;

	SyncToDraw = 0;
	MaxTextureSize = 4096;
#ifdef __EMSCRIPTEN__
	OpenGLVersion = GL_ES;
#elif __LINUX_ARM__
	OpenGLVersion = GL_ES;
#else
	OpenGLVersion = GL_Core;
#endif
	UseVSync = VS_Adaptive;

	UseOpenGLDebug = 0;
	DebugLevel = 2;
	NoBuffering = 0;
	NoDrawComplexSurface = 0;
	NoDrawGouraud = 0;
	NoDrawGouraudList = 0;
	NoDrawTile = 0;
	NoDrawSimple = 0;

	DetailTextures = 1;
	DescFlags |= RDDESCF_Certified;
	HighDetailActors = 1;
	Coronas = 1;
	ShinySurfaces = 1;
	VolumetricLighting = 1;

	unguard;
}

UBOOL UXOpenGLRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UXOpenGLRenderDevice::Init);
	//it is really a bad habit of the UEngine1 games to call init again each time a fullscreen change is needed.

	Viewport = InViewport;
	NeedsInit = true;
	bMappedBuffers = false;
	bInitializedShaders = false;
	TexNum = 1;
	iPixelFormat = 0;

	LastZMode = 255;
	NumClipPlanes = 0;

#if _WIN32
    hRC = NULL;
    #if ENGINE_VERSION!=227
	TimerBegin();
	#endif // ENGINE_VERSION
#endif

	// Driver flags.
	FullscreenOnly = 0;
	SpanBased = 0;
	SupportsTC = 1;
	SupportsFogMaps = 1;
	SupportsDistanceFog = 1;
	SupportsLazyTextures = 0;
	PrefersDeferredLoad = 0;
#if ENGINE_VERSION==227
	SupportsHDLightmaps = UseEnhancedLightmaps;
#endif

	// Extensions & other inits.
	ActiveProgram = -1;
	AMDMemoryInfo = false;
	NVIDIAMemoryInfo = false;
	BufferCount = 0;
	TexNum = 1;
	SwapControlExt = false;
	SwapControlTearExt = false;

	// Verbose Logging
	debugf(NAME_DevLoad, TEXT("XOpenGL: Current settings"));

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	debugf(NAME_DevLoad, TEXT("UseBindlessTextures %i"), UseBindlessTextures);
	debugf(NAME_DevLoad, TEXT("UseHWLighting %i"), UseHWLighting);
	debugf(NAME_DevLoad, TEXT("UseHWClipping %i"), UseHWClipping);
#endif
	debugf(NAME_DevLoad, TEXT("UseTrilinear %i"), UseTrilinear);
	debugf(NAME_DevLoad, TEXT("UsePrecache %i"), UsePrecache);
	debugf(NAME_DevLoad, TEXT("UseAA %i"), UseAA);
	debugf(NAME_DevLoad, TEXT("NumAASamples %i"), NumAASamples);

	debugf(NAME_DevLoad, TEXT("RefreshRate %i"), RefreshRate);
	debugf(NAME_DevLoad, TEXT("GammaOffsetScreenshots %f"), GammaOffsetScreenshots);
	debugf(NAME_DevLoad, TEXT("LODBias %f"), LODBias);
	debugf(NAME_DevLoad, TEXT("MaxAnisotropy %f"), MaxAnisotropy);
	debugf(NAME_DevLoad, TEXT("ShareLists %i"), ShareLists);
	debugf(NAME_DevLoad, TEXT("AlwaysMipmap %i"), AlwaysMipmap);
	debugf(NAME_DevLoad, TEXT("NoFiltering %i"), NoFiltering);
	//debugf(NAME_DevLoad, TEXT("UseSRGBTextures %i"),UseSRGBTextures);

	debugf(NAME_DevLoad, TEXT("GammaCorrectScreenshots %i"), GammaCorrectScreenshots);
	debugf(NAME_DevLoad, TEXT("MacroTextures %i"), MacroTextures);
	debugf(NAME_DevLoad, TEXT("BumpMaps %i"), BumpMaps);
	debugf(NAME_DevLoad, TEXT("EnvironmentMaps %i"), EnvironmentMaps);
	debugf(NAME_DevLoad, TEXT("NoAATiles %i"), NoAATiles);
	debugf(NAME_DevLoad, TEXT("GenerateMipMaps %i"), GenerateMipMaps);
	//debugf(NAME_DevLoad, TEXT("EnableShadows %i"), EnableShadows);
	debugf(NAME_DevLoad, TEXT("SyncToDraw %i"), SyncToDraw);

	debugf(NAME_DevLoad, TEXT("DetailTextures %i"), DetailTextures);
	debugf(NAME_DevLoad, TEXT("DescFlags %i"), DescFlags);
	debugf(NAME_DevLoad, TEXT("HighDetailActors %i"), HighDetailActors);
	debugf(NAME_DevLoad, TEXT("Coronas %i"), Coronas);
	debugf(NAME_DevLoad, TEXT("ShinySurfaces %i"), ShinySurfaces);
	debugf(NAME_DevLoad, TEXT("VolumetricLighting %i"), VolumetricLighting);

	if (OpenGLVersion == GL_Core)
		debugf(NAME_DevLoad, TEXT("OpenGL version: GL_Core"));
	else debugf(NAME_DevLoad, TEXT("OpenGL version: GL_ES"));

	if (UseVSync == VS_Adaptive)
		debugf(NAME_DevLoad, TEXT("UseVSync: VS_Adaptive"));
	else if (UseVSync == VS_On)
		debugf(NAME_DevLoad, TEXT("UseVSync: VS_On"));
	else if (UseVSync == VS_Off)
		debugf(NAME_DevLoad, TEXT("UseVSync: VS_Off"));

	debugf(NAME_DevLoad, TEXT("XOpenGL: Debug settings:"));
	debugf(NAME_DevLoad, TEXT("UseOpenGLDebug %i"), UseOpenGLDebug);
	debugf(NAME_DevLoad, TEXT("DebugLevel %i"), DebugLevel);
	debugf(NAME_DevLoad, TEXT("NoBuffering %i"), NoBuffering);
	debugf(NAME_DevLoad, TEXT("NoDrawComplexSurface %i"), NoDrawComplexSurface);
	debugf(NAME_DevLoad, TEXT("NoDrawGouraud %i"), NoDrawGouraud);
	debugf(NAME_DevLoad, TEXT("NoDrawGouraudList %i"), NoDrawGouraudList);
	debugf(NAME_DevLoad, TEXT("NoDrawTile %i"), NoDrawTile);
	debugf(NAME_DevLoad, TEXT("NoDrawSimple %i"), NoDrawSimple);

#if ENGINE_VERSION==227
	// new in 227j. This will tell render to use the abilities a new rendev provides instead of using internal (CPU intense) functions.
	//In use so far for DetailTextures on meshes.
	LegacyRenderingOnly = 0;

	// Make use of DrawGouraudPolyList for Meshes and leave the clipping up for the rendev. Instead of pushing vert by vert this uses a huge list.
	SupportsNoClipRender = UseHWClipping;
#else
	UseHWClipping = 0;
#endif
	UseMeshBuffering = 0;

	if (NoBuffering)
		UsePersistentBuffers = false;

	EnvironmentMaps = 0; //not yet implemented.

	if (ShareLists && !SharedBindMap)
		SharedBindMap = new TOpenGLMap<QWORD, UXOpenGLRenderDevice::FCachedTexture>;

	BindMap = ShareLists ? SharedBindMap : &LocalBindMap;

#ifdef SDL2BUILD
	Window =  (SDL_Window*) Viewport->GetWindow();
#elif QTBUILD

#else
	INT i = 0;
	// Get list of device modes.
	for (i = 0;; i++)
	{
		DEVMODEW Tmp;
		appMemzero(&Tmp, sizeof(Tmp));
		Tmp.dmSize = sizeof(Tmp);
		if (!EnumDisplaySettingsW(NULL, i, &Tmp)) {
			break;
		}
		Modes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
		//debugf(NAME_Init, TEXT("Found resolution mode: Width %i, Height %i, BitsPerPixel %i, Frequency %i"),Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency);
	}

	hWnd = (HWND)Viewport->GetWindow();
	hDC = GetDC(hWnd);

#endif

	// Set resolution, also creates context and checks extensions.
	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen))
	{
		GWarn->Logf(TEXT("XOpenGL: SetRes failed!"));
		return 0;
	}

	if (GIsEditor)
	{
		ShareLists = 1;
		UsingBindlessTextures = false;
		UsingPersistentBuffers = false;
	}
	else
	{
#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
        // Doing after extensions have been checked.
		UsingBindlessTextures = UseBindlessTextures ? true : false;
		UsingPersistentBuffers = UsePersistentBuffers ? true : false;
#else
		UsingBindlessTextures = false;
		UsingPersistentBuffers = false;
#endif
	}

	LogLevel = DebugLevel;

	// For matrix setup in SetSceneNode()
	bIsOrtho = false;
	StoredFovAngle = 0;
	StoredFX = 0;
	StoredFY = 0;
	StoredOrthoFovAngle = 0;
	StoredOrthoFX = 0;
	StoredOrthoFY = 0;

	// Set view matrix.
	viewMat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));

	// Initial proj matrix (updated to actual values in SetSceneNode).
	projMat = glm::mat4(1.0f);

	// Identity
	modelMat = glm::mat4(1.0f);

	UsingPersistentBuffersTile = false;
	UsingPersistentBuffersComplex = false;//unless being able to batch bigger amount of draw calls this is significantly slower. Unfortunately can't handle enough textures right now. With LightMaps it easily reaches 12k and more.
	UsingPersistentBuffersGouraud = UsingPersistentBuffers;

	// Init shaders
	InitShaders();

	// Map (persistent) buffers
	MapBuffers();

	if (UseHWLighting)
		InViewport->GetOuterUClient()->NoLighting = 1; // Disable (Engine) lighting.

	// stijn: This ResetFog call was missing and caused the black screen bug in UT469
	if (bMappedBuffers)
		ResetFog();

	return 1;
	unguard;
}

void UXOpenGLRenderDevice::PostEditChange()
{
	guard(UXOpenGLRenderDevice::PostEditChange);
	debugf(NAME_Dev, TEXT("XOpenGL: PostEditChange"));

	if (AllContexts.Num() == 0)
	{
		CheckExtensions();
		UnMapBuffers();

		DeleteShaderBuffers();
		InitShaders();

		MapBuffers();
	}

	Flush(UsePrecache);

	unguard;
}

#ifdef WINBUILD
LRESULT CALLBACK WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uiMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, uiMsg, wParam, lParam);
	}

	return 0;
}
#endif
UBOOL UXOpenGLRenderDevice::SetWindowPixelFormat()
{
	guard(UXOpenGLRenderDevice::SetWindowPixelFormat);
	#ifdef _WIN32
	if (!SetPixelFormat(hDC, iPixelFormat, &pfd))
	{
		debugf(NAME_Init, TEXT("XOpenGL: Setting PixelFormat %i failed!"), iPixelFormat);
		iPixelFormat = ChoosePixelFormat(hDC, &pfd);
		if (!SetPixelFormat(hDC, iPixelFormat, &pfd))
			GWarn->Logf(TEXT("XOpenGL: SetPixelFormat %i failed. Restart may required."), iPixelFormat);
		return 0;
	}
	else debugf(NAME_Init, TEXT("XOpenGL: Setting PixelFormat: %i"), iPixelFormat);
	#endif
	return 1;
	unguard;
}

UBOOL UXOpenGLRenderDevice::CreateOpenGLContext(UViewport* Viewport, INT NewColorBytes)
{
	guard(UXOpenGLRenderDevice::CreateOpenGLContext);

#ifdef _WIN32
	if (hRC)// || CurrentGLContext)
	{
		MakeCurrent();
		return 1;
	}
#endif

	debugf(TEXT("XOpenGL: Creating new OpenGL context."));

	DesiredColorBits = NewColorBytes <= 2 ? 16 : 32;
	DesiredStencilBits = NewColorBytes <= 2 ? 0 : 8;
	DesiredDepthBits = NewColorBytes <= 2 ? 16 : 24;

	INT MajorVersion = 3;
	INT MinorVersion = 3;

	if (OpenGLVersion == GL_ES)
	{
		MajorVersion = 3;
		MinorVersion = 1;
	}
	else if (UseBindlessTextures || UsePersistentBuffers)
	{
		MajorVersion = 4;
		MinorVersion = 5;
	}

	iPixelFormat = 0;


#ifdef SDL2BUILD
InitContext:

	// Tell SDL what kind of context we want
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, MajorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, MinorVersion);

	if (UseOpenGLDebug)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	if (OpenGLVersion == GL_ES)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // SDL_GL_CONTEXT_PROFILE_COMPATIBILITY

	// not checking for any existing SDL context, create a new one, since using
	// SDL for splash already and it's getting confused.
	//

	// Init global GL.
	// no need to do SDL_Init here, done in SDL2Drv (USDL2Viewport::ResizeViewport).
	Window = (SDL_Window*)Viewport->GetWindow();

	if (!Window)
		appErrorf(TEXT("XOpenGL: No SDL Window found!"));

	if (!glContext)
		glContext = SDL_GL_CreateContext(Window);

	if (glContext == NULL)
	{
        if (OpenGLVersion == GL_Core)
        {
            if (UseBindlessTextures || UsePersistentBuffers)
            {
                if (MajorVersion == 3 && MinorVersion == 3) // already 3.3
                {
                    appErrorf(TEXT("XOpenGL: Failed to init OpenGL %i.%i context. SDL_GL_CreateContext: %ls"), MajorVersion, MinorVersion, appFromAnsi(SDL_GetError()));
                    return 0;
                }
                else
                {
                    //Try with lower context, disable 4.5 features.
                    MajorVersion = 3;
                    MinorVersion = 3;

                    UsingBindlessTextures = false;
                    UsingPersistentBuffers = false;

                    debugf(NAME_Init, TEXT("OpenGL %i.%i failed to initialize. Disabling UsingBindlessTextures and UsingPersistentBuffers, switching to 3.3 context."), MajorVersion, MinorVersion);

                    goto InitContext;
                }
            }
            return 0;
        }
        else
        {
            appErrorf(TEXT("XOpenGL: No OpenGL ES %i.%i context support."), MajorVersion, MinorVersion);
            return 0;
        }
	}

	MakeCurrent();

#ifdef GLAD
	if (OpenGLVersion == GL_ES)
		gladLoadGLES2Loader(SDL_GL_GetProcAddress);
	else
		gladLoadGLLoader(SDL_GL_GetProcAddress);
#else
# define GL_PROC(ext,fntype,fnname)										\
	if (SUPPORTS_##ext) {												\
		if ((fnname = (fntype) SDL_GL_GetProcAddress(#fnname)) == NULL) { \
			debugf(NAME_Init, TEXT("Missing OpenGL entry point '%ls' for '%ls'"), #fnname, #ext); \
			SUPPORTS_##ext = 0;											\
		}																\
	}
# include "XOpenGLFuncs.h"
#endif

	Description = appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER));
	debugf(NAME_Init, TEXT("GL_VENDOR     : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_VENDOR)));
	debugf(NAME_Init, TEXT("GL_RENDERER   : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER)));
	debugf(NAME_Init, TEXT("GL_VERSION    : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_VERSION)));
	debugf(NAME_Init, TEXT("GL_SHADING_LANGUAGE_VERSION    : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_SHADING_LANGUAGE_VERSION)));

	glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
	for (INT i = 0; i<NumberOfExtensions; i++)
	{
		FString ExtensionString = appFromAnsi((const ANSICHAR *)glGetStringi(GL_EXTENSIONS, i));
		debugf(NAME_DevLoad, TEXT("GL_EXTENSIONS(%i) : %ls"), i, *ExtensionString);
	}
	if (OpenGLVersion == GL_ES)
		debugf(NAME_Init, TEXT("XOpenGL: OpenGL ES %i.%i context initialized!"), MajorVersion, MinorVersion);
	else
		debugf(NAME_Init, TEXT("XOpenGL: OpenGL %i.%i core context initialized!"), MajorVersion, MinorVersion);

	if (UseOpenGLDebug)
	{
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(&UXOpenGLRenderDevice::DebugCallback, NULL);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GWarn->Logf(TEXT("XOpenGL: OpenGL debugging enabled, this can cause severe performance drain!"));
	}

	if (NoBuffering)
	{
		GWarn->Logf(TEXT("XOpenGL: NoBuffering enabled, this WILL cause severe performance drain! This debugging option is useful to find performance critical situations, but should not be used in general."));
	}
#elif QTBUILD

#else
	if (NeedsInit)
	{
		PIXELFORMATDESCRIPTOR temppfd{};
		temppfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		temppfd.nVersion = 1;
		temppfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		temppfd.iPixelType = PFD_TYPE_RGBA;
		temppfd.cColorBits = DesiredColorBits;
		temppfd.cDepthBits = DesiredDepthBits;
		//temppfd.cStencilBits = DesiredStencilBits;
		temppfd.cAlphaBits = 0;
		temppfd.iLayerType = PFD_MAIN_PLANE;
		DWORD Style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		WNDCLASSEX WndClassEx;
		memset(&WndClassEx, 0, sizeof(WNDCLASSEX));

		WndClassEx.cbSize = sizeof(WNDCLASSEX);
		WndClassEx.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
		WndClassEx.lpfnWndProc = WndProc;
		WndClassEx.hInstance = hInstance;
		WndClassEx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		WndClassEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		WndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
		WndClassEx.lpszClassName = L"Win32OpenGLWindow";

		if (RegisterClassEx(&WndClassEx) == 0)
		{
			debugf(TEXT("XOpenGL: RegisterClassEx failed!"));
		}
		HWND TemphWnd = CreateWindowEx(WS_EX_APPWINDOW, WndClassEx.lpszClassName, L"InitWIndow", Style, 0, 0, Viewport->SizeX, Viewport->SizeY, NULL, NULL, hInstance, NULL);
		HDC TemphDC = GetDC(TemphWnd);
		INT nPixelFormat = ChoosePixelFormat(TemphDC, &temppfd);
		if (!nPixelFormat) {
			pfd.cDepthBits = 24;
			nPixelFormat = ChoosePixelFormat(TemphDC, &pfd);
		}
		if (!nPixelFormat) {
			pfd.cDepthBits = 16;
			nPixelFormat = ChoosePixelFormat(TemphDC, &pfd);
		}
		check(nPixelFormat);
		verify(SetPixelFormat(TemphDC, nPixelFormat, &temppfd));

		// oldstyle context init
		HGLRC tempContext = wglCreateContext(TemphDC);
		wglMakeCurrent(TemphDC, tempContext);

		wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
		if (wglChoosePixelFormatARB == nullptr) {
			appErrorf(TEXT("wglGetProcAddress() for wglChoosePixelFormatARB failed."));
			return 0;
		}

		wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
		if (wglCreateContextAttribsARB == nullptr) {
			appErrorf(TEXT("wglGetProcAddress() for wglCreateContextAttribsARB failed."));
			return 0;
		}

		//init
		if (!gladLoadGL())
		{
			appErrorf(TEXT("XOpenGL: Init failed!"));
		}
		else debugf(NAME_Init, TEXT("XOpenGL: Successfully initialized."));
		CHECK_GL_ERROR();
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(tempContext);
		ReleaseDC(TemphWnd, TemphDC);
		DestroyWindow(TemphWnd);
		NeedsInit = false;

	}
	//Now init pure OpenGL >= 3.3 context.
InitContext:
	{
		memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = DesiredColorBits;
		pfd.cDepthBits = DesiredDepthBits;
		pfd.iLayerType = PFD_MAIN_PLANE;

		debugf(NAME_Init, TEXT("XOpenGL: DesiredColorBits: %i"), DesiredColorBits);
		debugf(NAME_Init, TEXT("XOpenGL: DesiredDepthBits: %i"), DesiredDepthBits);

		if (UseAA && !GIsEditor)
		{
			while (NumAASamples > 0)
			{
				UINT NumFormats = 0;
				INT iPixelFormatAttribList[] =
				{
					WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
					WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
					WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
					WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
					WGL_COLOR_BITS_ARB, DesiredColorBits,
					WGL_DEPTH_BITS_ARB, DesiredDepthBits,
					WGL_STENCIL_BITS_ARB, 0,//DesiredStencilBits,
					WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
					WGL_SAMPLES_ARB, NumAASamples,
					0 // End of attributes list
				};

				if (wglChoosePixelFormatARB(hDC, iPixelFormatAttribList, NULL, 1, &iPixelFormat, &NumFormats) == TRUE && NumFormats > 0)
					break;
				NumAASamples--;
			}
		}
		else
		{
			UINT NumFormats = 0;
			INT iPixelFormatAttribList[] =
			{
				WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
				WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
				WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
				WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
				WGL_COLOR_BITS_ARB, DesiredColorBits,
				WGL_DEPTH_BITS_ARB, DesiredDepthBits,
				WGL_STENCIL_BITS_ARB, 0,//DesiredStencilBits,
				0 // End of attributes list
			};
			wglChoosePixelFormatARB(hDC, iPixelFormatAttribList, NULL, 1, &iPixelFormat, &NumFormats);
		}

		INT ContextFlags = 0;
		if (!GL_KHR_debug)
		{
			GWarn->Logf(TEXT("OpenGL debugging extension not found!"));
			UseOpenGLDebug = 0;
		}
		if (UseOpenGLDebug)
			ContextFlags = WGL_CONTEXT_CORE_PROFILE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB;
		else ContextFlags = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

		INT iContextAttribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, MajorVersion,
			WGL_CONTEXT_MINOR_VERSION_ARB, MinorVersion,
			WGL_CONTEXT_FLAGS_ARB, ContextFlags,
			0 // End of attributes list
		};
		SetWindowPixelFormat();
		hRC = wglCreateContextAttribsARB(hDC, 0, iContextAttribs);
	}

	if (hRC)
	{
		MakeCurrent();
		Description = appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER));
		debugf(NAME_Init, TEXT("GL_VENDOR     : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_VENDOR)));
		debugf(NAME_Init, TEXT("GL_RENDERER   : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER)));
		debugf(NAME_Init, TEXT("GL_VERSION    : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_VERSION)));
		debugf(NAME_Init, TEXT("GL_SHADING_LANGUAGE_VERSION    : %ls"), appFromAnsi((const ANSICHAR *)glGetString(GL_SHADING_LANGUAGE_VERSION)));

		int NumberOfExtensions = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
		for (INT i = 0; i < NumberOfExtensions; i++)
		{
			FString ExtensionString = appFromAnsi((const ANSICHAR*)glGetStringi(GL_EXTENSIONS, i));
			AllExtensions += ExtensionString;
			AllExtensions += TEXT(" ");
		}

		// Extension list does not necessarily contain (all) wglExtensions !!
		PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = nullptr;
		wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));

		if (wglGetExtensionsStringARB)
			AllExtensions += appFromAnsi(wglGetExtensionsStringARB(hDC));

		

		FString ExtensionString = AllExtensions;
		FString SplitString;
		INT i = 0;
		while (ExtensionString.Split(TEXT(" "), &SplitString, &ExtensionString, 0))
		{
			if (SplitString.Len())
			{
				debugf(NAME_DevLoad, TEXT("GL_EXTENSIONS(%i): %ls"),i, *SplitString);
				i++;
			}
		}
		NumberOfExtensions = i;

		if (OpenGLVersion == GL_Core)
			debugf(NAME_Init, TEXT("OpenGL %i.%i context initialized!"), MajorVersion, MinorVersion);
		else
			debugf(NAME_Init, TEXT("OpenGL ES %i.%i context initialized!"), MajorVersion, MinorVersion);
	}
	else
	{
		if (OpenGLVersion == GL_Core)
		{
			if (UseBindlessTextures || UsePersistentBuffers)
			{
				if (MajorVersion == 3 && MinorVersion == 3) // already 3.3
				{
					appErrorf(TEXT("XOpenGL: Failed to init OpenGL %i.%i context."), MajorVersion, MinorVersion);
					return 0;
				}
				else
				{
					//Try with lower context, disable 4.5 features.
					MajorVersion = 3;
					MinorVersion = 3;

					UsingBindlessTextures = false;
					UsingPersistentBuffers = false;

					debugf(NAME_Init, TEXT("OpenGL %i.%i failed to initialize. Disabling UsingBindlessTextures and UsingPersistentBuffers, switching to 3.3 context."), MajorVersion, MinorVersion);

					goto InitContext;
				}
			}
			return 0;
		}
		else
		{
			appErrorf(TEXT("XOpenGL: No OpenGL ES %i.%i context support."), MajorVersion, MinorVersion);
			return 0;
		}
	}


	if (ShareLists && AllContexts.Num())
		verify(wglShareLists(AllContexts(0), hRC) == 1);
	AllContexts.AddItem(hRC);

	if (UseOpenGLDebug)
	{
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(&UXOpenGLRenderDevice::DebugCallback, NULL);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GWarn->Logf(TEXT("OpenGL debugging enabled, this can cause severe performance drain!"));
	}
	else
		debugf(TEXT("XOpenGL: OpenGL debugging disabled."));
#endif

	// Check and validate extensions & settings.
	CheckExtensions();

	return 1;
	unguard;
}

void UXOpenGLRenderDevice::MakeCurrent()
{
	guard(UOpenGLRenderDevice::MakeCurrent);
#ifdef SDL2BUILD
	if (CurrentGLContext != glContext)
	{
		//debugf(TEXT("XOpenGL: MakeCurrent"));
		INT Result = SDL_GL_MakeCurrent(Window, glContext);
		if (Result != 0)
			debugf(TEXT("XOpenGL: MakeCurrent failed with: %ls\n"), appFromAnsi(SDL_GetError()));
		CurrentGLContext = glContext;
	}
#elif QTBUILD

#else
	if (!hRC && !CurrentGLContext)
		appErrorf(TEXT("No valid GL Context!"));

	if (!hRC && CurrentGLContext)
		hRC = CurrentGLContext;
	check(hRC);

	if (CurrentGLContext != hRC || GIsEditor)
	{
		check(hDC);
		if (!wglMakeCurrent(hDC, hRC))
			appGetLastError();
		CurrentGLContext = hRC;
	}

	CLEAR_GL_ERROR();

#endif
	unguard;
}

void UXOpenGLRenderDevice::SetPermanentState()
{
	// Set permanent state.
	glEnable(GL_DEPTH_TEST);
	CHECK_GL_ERROR();
	glDepthMask(GL_TRUE);
	CHECK_GL_ERROR();
	glPolygonOffset(-1.0f, -1.0f);
	CHECK_GL_ERROR();
	glBlendFunc(GL_ONE, GL_ZERO);
	CHECK_GL_ERROR();
	glEnable(GL_BLEND);
	CHECK_GL_ERROR();
	glDisable(GL_CLIP_DISTANCE0);
    CHECK_GL_ERROR();

	/*
	GLES 3 supports sRGB functionality, but it does not expose the
	GL_FRAMEBUFFER_SRGB enable/disable bit.  Instead the implementation
	is     */

#ifndef __EMSCRIPTEN__
	if (OpenGLVersion == GL_Core || GIsEditor)
		glEnable(GL_FRAMEBUFFER_SRGB);
#endif

	/*
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	On some implementations, when you call glEnable(GL_POINT_SMOOTH) or glEnable(GL_LINE_SMOOTH) and you use shaders at the same time, your rendering speed goes down to 0.1 FPS.
	This is because the driver does software rendering. This would happen on AMD/ATI GPUs/drivers.
	*/

#if ENGINE_VERSION==227
	// Culling
	/*
	Can't keep this for other UEngine games despite the performance gain. Most noticeable is that decals are wrong faced in UT and probably in other games as well.
	However, most gain is when using static meshes anyway, so this shouldn't be much of a problem for the other UEngine games.
	*/
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	CHECK_GL_ERROR();
#endif
}

UBOOL UXOpenGLRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UXOpenGLRenderDevice::SetRes);
	
	// If not fullscreen, and color bytes hasn't changed, do nothing.
#ifdef SDL2BUILD
	if (glContext && CurrentGLContext && glContext == CurrentGLContext)
#else
	if (hRC && CurrentGLContext && hRC == CurrentGLContext)
#endif
	{
		if (!Fullscreen && !WasFullscreen && NewColorBytes == static_cast<INT>(Viewport->ColorBytes))
		{
			// Change window size.
			if (!Viewport->ResizeViewport(BLIT_HardwarePaint | BLIT_OpenGL, NewX, NewY, NewColorBytes))
			{
				return 0;
			}

			// stijn: force a switch to our context if we're in the editor
#if WIN32
			if (GIsEditor)
			{
				wglMakeCurrent(NULL, NULL);
				MakeCurrent();
			}
#endif
			glViewport(0, 0, NewX, NewY);
			return 1;
		}
	}

#ifdef WINBUILD
	// Change display settings.
	if (Fullscreen)
	{
		INT FindX = NewX, FindY = NewY, BestError = MAXINT;
		for (INT i = 0; i < Modes.Num(); i++)
		{
			if (Modes(i).Z == (NewColorBytes * 8))
			{
				INT Error
					= (Modes(i).X - FindX)*(Modes(i).X - FindX)
					+ (Modes(i).Y - FindY)*(Modes(i).Y - FindY);
				if (Error < BestError)
				{
					NewX = Modes(i).X;
					NewY = Modes(i).Y;
					BestError = Error;
				}
			}
		}
		DEVMODEA dma;
		DEVMODEW dmw;
		bool tryNoRefreshRate = true;

		ZeroMemory(&dma, sizeof(dma));
		dma.dmSize = sizeof(dma);
		ZeroMemory(&dmw, sizeof(dmw));
		dmw.dmSize = sizeof(dmw);

		dma.dmPelsWidth = dmw.dmPelsWidth = NewX;
		dma.dmPelsHeight = dmw.dmPelsHeight = NewY;
		dma.dmBitsPerPel = dmw.dmBitsPerPel = NewColorBytes * 8;
		dma.dmFields = dmw.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;// | DM_BITSPERPEL;

# if !UNREAL_TOURNAMENT_OLDUNREAL
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, NewX, NewY, SWP_SHOWWINDOW);
# endif

		debugf(TEXT("XOpenGL: Fullscreen NewX %i NewY %i"), NewX, NewY);

		if (RefreshRate)
		{
			dma.dmDisplayFrequency = dmw.dmDisplayFrequency = RefreshRate;
			dma.dmFields |= DM_DISPLAYFREQUENCY;
			dmw.dmFields |= DM_DISPLAYFREQUENCY;

			if (ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				debugf(TEXT("XOpenGL: ChangeDisplaySettings failed: %ix%i, %i Hz"), NewX, NewY, RefreshRate);
				dma.dmFields &= ~DM_DISPLAYFREQUENCY;
				dmw.dmFields &= ~DM_DISPLAYFREQUENCY;
			}
			else
			{
				debugf(TEXT("ChangeDisplaySettings with RefreshRate: %ix%i, %i Hz"), NewX, NewY, RefreshRate);
				tryNoRefreshRate = false;
			}
		}
		if (tryNoRefreshRate)
		{
			if (ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				debugf(TEXT("XOpenGL: ChangeDisplaySettings failed: %ix%i"), NewX, NewY);
				return 0;
			}
			debugf(TEXT("XOpenGL: ChangeDisplaySettings: %ix%i"), NewX, NewY);
		}
	}
	else UnsetRes();

	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
	if (!Result)
	{
		debugf(TEXT("XOpenGL: Change window size failed!"));
		if (Fullscreen)
		{
			TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL, 0), ChangeDisplaySettingsA(NULL, 0));
		}
		return 0;
	}

	// (Re)init OpenGL rendering context.
	if (hRC && hRC != CurrentGLContext)
		MakeCurrent();
	else
		CreateOpenGLContext(Viewport, NewColorBytes);
#else

	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
	if (!Result)
	{
		debugf(TEXT("XOpenGL: Change window size failed!"));
		return 0;
	}

	if (glContext && glContext != CurrentGLContext)
		MakeCurrent();
	else
		CreateOpenGLContext(Viewport, NewColorBytes);
#endif

	// Flush textures.
	Flush(1);

	// Set permanent state... (OpenGL Presets)
	SetPermanentState();
	CHECK_GL_ERROR();

	// Verify hardware defaults.
	CurrentPolyFlags = static_cast<DWORD>(PF_Occlude);

	// Remember fullscreenness.
	WasFullscreen = Fullscreen;
	return 1;

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::UnsetRes()
{
	guard(UXOpenGLRenderDevice::UnsetRes);

#ifdef WINBUILD
	if (WasFullscreen)
	{
		ChangeDisplaySettingsW(NULL, 0);
	}
#elif SDL2BUILD

#else

#endif
	unguard;
}

void UXOpenGLRenderDevice::SwapControl()
{
#ifdef SDL2BUILD
	guard(SwapControl);
	switch (UseVSync)
	{
	case VS_Off:
		if (SDL_GL_SetSwapInterval(0) != 0)
			debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
		else debugf(NAME_Init, TEXT("XOpenGL: VSync Off"));
		break;
	case VS_On:
		if (SDL_GL_SetSwapInterval(1) != 0)
			debugf(NAME_Init, TEXT("XOpenGL: Setting VSync on has failed."));
		else  debugf(NAME_Init, TEXT("XOpenGL: VSync On"));
		break;
	case VS_Adaptive:
		if (SDL_GL_SetSwapInterval(-1) != 0)
        {
			debugf(NAME_Init, TEXT("XOpenGL: Setting VSync adaptive has failed. Falling back to SwapInterval 0 (VSync Off)."));
			if (SDL_GL_SetSwapInterval(0) != 0)
                debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
        }
		else debugf(NAME_Init, TEXT("XOpenGL: VSync Adaptive"));
		break;
	default:
		if (SDL_GL_SetSwapInterval(0) != 0)
			debugf(NAME_Init, TEXT("XOpenGL: Setting default VSync off has failed."));
		else debugf(NAME_Init, TEXT("XOpenGL: VSync Off (default)"));
	}
	unguard;
#else
	guard(SwapControl);
	if (SwapControlExt)
	{
		PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		if (!wglSwapIntervalEXT)
			return;
		switch (UseVSync)
		{
		case VS_On:
			if (wglSwapIntervalEXT(1) != 1)
				debugf(NAME_Init, TEXT("XOpenGL: Setting VSync on has failed."));
			else  debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: On"));
			break;
		case VS_Off:
			if (wglSwapIntervalEXT(0) != 1)
				debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
			else debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Off"));
			break;

		case VS_Adaptive:
			if (!SwapControlTearExt)
			{
				debugf(NAME_Init, TEXT("XOpenGL: WGL_EXT_swap_control_tear is not supported by device. Falling back to SwapInterval 0 (VSync Off)."));
				if (wglSwapIntervalEXT(0) != 1)
					debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
				else debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Off"));
				break;
			}
			if (wglSwapIntervalEXT(-1) != 1)
				debugf(NAME_Init, TEXT("XOpenGL: Setting VSync adaptive has failed."));
			else debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Adaptive"));
			break;
		default:
			if (wglSwapIntervalEXT(0) != 1)
				debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
			else debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Off (default)"));
		}
	}
	else
		debugf(NAME_Init, TEXT("XOpenGL: WGL_EXT_swap_control is not supported."));
	unguard;
#endif // SDL2BUILD
}


void UXOpenGLRenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UXOpenGLRenderDevice::Flush);
	CHECK_GL_ERROR();

	debugf(TEXT("XOpenGL: Flush"));

#if WIN32
	if (GIsEditor)
	{
		wglMakeCurrent(NULL, NULL);
		MakeCurrent();
	}
#endif

	// Create a list of static lights.
	if (StaticLightList.Num())
		StaticLightList.Empty();

	NumStaticLights = 0;

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	for (TObjectIterator<UTexture> It; It; ++It)
	{
		FCachedTexture* FCachedTextureInfo = (FCachedTexture*)It->TextureHandle;
		if (FCachedTextureInfo)
			delete FCachedTextureInfo;
		It->TextureHandle = NULL;
	}
#endif // ENGINE_VERSION

	TArray<GLuint> Binds;
	for (TOpenGLMap<QWORD, FCachedTexture>::TIterator It(*BindMap); It; ++It)
	{
		CHECK_GL_ERROR();
		for (INT i = 0; i < 2; i++)
		{
			glDeleteSamplers(1, &It.Value().Sampler[i]);

			#ifdef __LINUX_ARM__
			if (UsingBindlessTextures && It.Value().TexHandle[i] && glIsTextureHandleResidentNV(It.Value().TexHandle[i]))
				glMakeTextureHandleNonResidentNV(It.Value().TexHandle[i]);
            #else
            if (UsingBindlessTextures && It.Value().TexHandle[i] && glIsTextureHandleResidentARB(It.Value().TexHandle[i]))
                glMakeTextureHandleNonResidentARB(It.Value().TexHandle[i]);
            #endif
			It.Value().TexHandle[i] = 0;
			It.Value().TexNum[i] = 1;
			if (It.Value().Ids[i])
			{
				glDeleteTextures(1, &It.Value().Ids[i]);
				Binds.AddItem(It.Value().Ids[i]);
			}
		}
		CHECK_GL_ERROR();
	}
	BindMap->Empty();

	if (Binds.Num())
	{
		//debugf(TEXT("Binds.Num() %i"),Binds.Num());
		glDeleteTextures(Binds.Num(), (GLuint*)&Binds(0));
	}
	CHECK_GL_ERROR();

	for (INT i = 0; i < 8; i++) // Also reset all multi textures.
		SetNoTexture(i);

	if (AllowPrecache && UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	CHECK_GL_ERROR();

	if (Viewport && Viewport->GetOuterUClient())
		SetGamma(Viewport->GetOuterUClient()->Brightness);

	CurrentPolyFlags = 0;
	CurrentAdditionalPolyFlags = 0;

    SetProgram(No_Prog);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();

	SwapControl();

	StoredOrthoFovAngle = 0;
	StoredOrthoFX = 0;
	StoredOrthoFY = 0;
	StoredFovAngle = 0;
	StoredFX = 0;
	StoredFY = 0;
	appMemzero(&Stats, sizeof(Stats));
	TexNum = 1;

	if (bMappedBuffers)
        ResetFog();

	if (SyncToDraw)
		debugf(TEXT("XOpenGL: SyncToDraw enabled"));
	else debugf(TEXT("XOpenGL: SyncToDraw disabled"));

	CHECK_GL_ERROR();
	unguard;
}

UBOOL UXOpenGLRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UXOpenGLRenderDevice::Exec);
	if (URenderDevice::Exec(Cmd, Ar))
	{
		return 1;
	}
	if (ParseCommand(&Cmd, TEXT("GetRes")))
	{
#ifdef WINBUILD
		TArray<FPlane> Relevant;
		INT i;
		for (i = 0; i<Modes.Num(); i++)
			if (Modes(i).Z == Viewport->ColorBytes * 8)
				if
					((Modes(i).X != 320 || Modes(i).Y != 200)
					&& (Modes(i).X != 640 || Modes(i).Y != 400))
					Relevant.AddUniqueItem(FPlane(Modes(i).X, Modes(i).Y, 0, 0));
		appQsort(&Relevant(0), Relevant.Num(), sizeof(FPlane), (QSORT_COMPARE)CompareRes);
		FString Str;
		for (i = 0; i<Relevant.Num(); i++)
			Str += FString::Printf(TEXT("%ix%i "), (INT)Relevant(i).X, (INT)Relevant(i).Y);
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
#else
		FString Str = TEXT("");
		// Available fullscreen video modes
		INT display_count = 0, display_index = 0, i = 0;
		SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

		if ((display_count = SDL_GetNumDisplayModes(0)) < 1)
		{
			debugf(NAME_Init, TEXT("No available fullscreen video modes"));
		}
		//debugf(TEXT("SDL_GetNumVideoDisplays returned: %i"), display_count);

		INT PrevW = 0, PrevH = 0;
		for (i = 0; i < display_count; i++)
		{
			mode.format = 0;
			mode.w = 0;
			mode.h = 0;
			mode.refresh_rate = 0;
			mode.driverdata = 0;
			if (SDL_GetDisplayMode(display_index, i, &mode) != 0)
				debugf(TEXT("SDL_GetDisplayMode failed: %ls"), SDL_GetError());
			//debugf(TEXT("SDL_GetDisplayMode(0, 0, &mode):\t\t%i bpp\t%i x %i"), SDL_BITSPERPIXEL(mode.format), mode.w, mode.h);
			if (mode.w != PrevW || mode.h != PrevH)
				Str += FString::Printf(TEXT("%ix%i "), mode.w, mode.h);
			PrevW = mode.w;
			PrevH = mode.h;
		}
		// Send the resolution string to the engine.
		Ar.Log(*Str.LeftChop(1));
		return 1;
}
#endif
	else if (ParseCommand(&Cmd, TEXT("CrashGL")))
	{
		appErrorf(TEXT("XOpenGL: Forced OpenGL crash!"));
	}
	else if (ParseCommand(&Cmd, TEXT("VideoFlush")))
	{
		debugf(NAME_Dev, TEXT("XOpenGL: VideoFlush"));
		Flush(1);
		return 1;
	}
	return 0;
	unguard;
}

void UXOpenGLRenderDevice::UpdateCoords(FSceneNode* Frame)
{
    guard(UXOpenGLRenderDevice::UpdateCoords);

    // Update Coords:  World to Viewspace projection.
	FrameCoords[0] = glm::vec4(Frame->Coords.Origin.X, Frame->Coords.Origin.Y, Frame->Coords.Origin.Z, 0.0f);
	FrameCoords[1] = glm::vec4(Frame->Coords.XAxis.X, Frame->Coords.XAxis.Y, Frame->Coords.XAxis.Z, 0.0f);
	FrameCoords[2] = glm::vec4(Frame->Coords.YAxis.X, Frame->Coords.YAxis.Y, Frame->Coords.YAxis.Z, 0.0f);
	FrameCoords[3] = glm::vec4(Frame->Coords.ZAxis.X, Frame->Coords.ZAxis.Y, Frame->Coords.ZAxis.Z, 0.0f);

	// And UnCoords: Viewspace to World projection.
	FrameUncoords[0] = glm::vec4(Frame->Uncoords.Origin.X, Frame->Uncoords.Origin.Y, Frame->Uncoords.Origin.Z, 0.0f);
	FrameUncoords[1] = glm::vec4(Frame->Uncoords.XAxis.X, Frame->Uncoords.XAxis.Y, Frame->Uncoords.XAxis.Z, 0.0f);
	FrameUncoords[2] = glm::vec4(Frame->Uncoords.YAxis.X, Frame->Uncoords.YAxis.Y, Frame->Uncoords.YAxis.Z, 0.0f);
	FrameUncoords[3] = glm::vec4(Frame->Uncoords.ZAxis.X, Frame->Uncoords.ZAxis.Y, Frame->Uncoords.ZAxis.Z, 0.0f);

    glBindBuffer(GL_UNIFORM_BUFFER, GlobalCoordsUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(FrameCoords));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(FrameUncoords));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	CHECK_GL_ERROR();

	unguard;
}

void UXOpenGLRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guard(UXOpenGLRenderDevice::SetSceneNode);

	Level = Frame->Level;
	if (Level && (!NumStaticLights || GIsEditor))
	{
		if (GIsEditor)
			StaticLightList.Empty();
		for (INT i = 0; i < Level->Actors.Num(); ++i)
		{
			AActor* Actor = Level->Actors(i);
			if (Actor && !Actor->bDeleteMe && (Actor->bStatic || (Actor->bNoDelete && !Actor->bMovable)) && Actor->LightBrightness>0 && Actor->LightType && Actor->LightRadius)
            {
#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
                StaticLightList.AddItem(Actor);
#else
                if (UseHWLighting)
                    StaticLightList.AddItem(Actor);
                else if (Actor->NormalLightRadius) //for normal mapping only add lights with normallightradius set. Needs performance tests if not.
                    StaticLightList.AddItem(Actor);
#endif
            }
		}
		NumStaticLights = StaticLightList.Num();

		for (INT i = 0; i < NumStaticLights; i++)
		{
			//debugf(TEXT("StaticLightList: %ls %f %f %f"), StaticLightList(i)->GetName(), StaticLightList(i)->Location.X, StaticLightList(i)->Location.Y, StaticLightList(i)->Location.Z);

			StaticLightData.LightPos[i] = glm::vec4(StaticLightList(i)->Location.X, StaticLightList(i)->Location.Y, StaticLightList(i)->Location.Z, 1.f);

			FPlane RGBColor = FGetHSV(StaticLightList(i)->LightHue, StaticLightList(i)->LightSaturation, StaticLightList(i)->LightBrightness);

#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
			StaticLightData.LightData1[i] = glm::vec4(RGBColor.X, RGBColor.Y, RGBColor.Z, StaticLightList(i)->LightCone);
#else
			StaticLightData.LightData1[i] = glm::vec4(RGBColor.R, RGBColor.G, RGBColor.B, StaticLightList(i)->LightCone);
#endif
			StaticLightData.LightData2[i] = glm::vec4(StaticLightList(i)->LightEffect, StaticLightList(i)->LightPeriod, StaticLightList(i)->LightPhase, StaticLightList(i)->LightRadius);
			StaticLightData.LightData3[i] = glm::vec4(StaticLightList(i)->LightType, StaticLightList(i)->VolumeBrightness, StaticLightList(i)->VolumeFog, StaticLightList(i)->VolumeRadius);
#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
			StaticLightData.LightData4[i] = glm::vec4(StaticLightList(i)->WorldLightRadius(), NumStaticLights, (GLfloat)StaticLightList(i)->Region.ZoneNumber, (GLfloat)(Frame->Viewport->Actor ? Frame->Viewport->Actor->Region.ZoneNumber : 0.f));
			StaticLightData.LightData5[i] = glm::vec4(StaticLightList(i)->LightRadius * 10, 1.0, 0.0, 0.0);
#else
			StaticLightData.LightData4[i] = glm::vec4(StaticLightList(i)->WorldLightRadius(), NumStaticLights, (GLfloat)StaticLightList(i)->Region.ZoneNumber, (GLfloat)(Frame->Viewport->Actor ? Frame->Viewport->Actor->CameraRegion.ZoneNumber : 0.f));
			StaticLightData.LightData5[i] = glm::vec4(StaticLightList(i)->NormalLightRadius, (GLfloat)StaticLightList(i)->bZoneNormalLight, 0.0, 0.0);
#endif
			if (i == MAX_LIGHTS - 1)
				break;
		}

		glBindBuffer(GL_UNIFORM_BUFFER, GlobalStaticLightInfoUBO);
		CHECK_GL_ERROR();

		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4) * MAX_LIGHTS, StaticLightData.LightPos);
		CHECK_GL_ERROR();
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS, sizeof(glm::vec4) * MAX_LIGHTS, StaticLightData.LightData1);
		CHECK_GL_ERROR();
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 2, sizeof(glm::vec4) * MAX_LIGHTS, StaticLightData.LightData2);
		CHECK_GL_ERROR();
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 3, sizeof(glm::vec4) * MAX_LIGHTS, StaticLightData.LightData3);
		CHECK_GL_ERROR();
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 4, sizeof(glm::vec4) * MAX_LIGHTS, StaticLightData.LightData4);
		CHECK_GL_ERROR();
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 5, sizeof(glm::vec4) * MAX_LIGHTS, StaticLightData.LightData5);
		CHECK_GL_ERROR();

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		CHECK_GL_ERROR();
	}
	CHECK_GL_ERROR();

	//Flush Buffers.
	SetProgram(No_Prog);

#if ENGINE_VERSION==227
	if (bFogEnabled)
		ResetFog();
#endif // ENGINE_VERSION

	//avoid some overhead, only calculate and set again if something was really changed.
	if (Frame->Viewport->IsOrtho() && (!bIsOrtho || StoredOrthoFovAngle != Viewport->Actor->FovAngle || StoredOrthoFX != Frame->FX || StoredOrthoFY != Frame->FY))
		SetOrthoProjection(Frame);
	else if (StoredFovAngle != Viewport->Actor->FovAngle || StoredFX != Frame->FX || StoredFY != Frame->FY || StoredbNearZ)
		SetProjection(Frame, 0);
	/*
	else
	{
        UpdateCoords(Frame);
	}
	*/

	/*
	m_3x3_inv_transp = glm::inverseTranspose(glm::mat3(modelviewMat));

	//viewMat inverted
	viewMatinv = glm::inverse(viewMat);
	glUseProgram(DrawComplexProg);//FIXME: use shared UBO
	glUniformMatrix4fv(g_programprojMat, 1, GL_FALSE, glm::value_ptr(projMat));
	glUniformMatrix4fv(g_programviewMat, 1, GL_FALSE, glm::value_ptr(viewMat));
	glUniformMatrix4fv(g_programmodelMat, 1, GL_FALSE, glm::value_ptr(modelMat));
	glUniformMatrix4fv(g_programmodelviewMat, 1, GL_FALSE, glm::value_ptr(modelviewMat));
	glUniformMatrix4fv(g_programmodelviewprojMat, 1, GL_FALSE, glm::value_ptr(modelviewprojMat));
	glUniformMatrix3fv(g_programNormMat, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));
	glUniformMatrix4fv(g_programviewMatinv, 1, GL_FALSE, glm::value_ptr(viewMatinv));
	glUniformMatrix4fv(g_programmodelviewprojMat, 1, GL_FALSE, glm::value_ptr(modelviewprojMat));
	CHECK_GL_ERROR();
	*/

	// Set clip planes.
	SetSceneNodeHit(Frame);

	// Disable clipping
	while (NumClipPlanes > 0)
	{
		PopClipPlane();
	}
	unguard;
}

void UXOpenGLRenderDevice::SetOrthoProjection(FSceneNode* Frame)
{
	guard(UXOpenGLRenderDevice::SetOrthoProjection);

	// Precompute stuff.
	FLOAT zFar = (GIsEditor && Frame->Viewport->Actor->RendMap == REN_Wire) ? 131072.0 : 65336.f;

	StoredFovAngle = StoredFX = StoredFY = 0; //ensure Matrix is updated again if not ortho.

	StoredOrthoFovAngle = Viewport->Actor->FovAngle;
	StoredOrthoFX = Frame->FX;
	StoredOrthoFY = Frame->FY;

	Aspect = Frame->FY / Frame->FX;
	RProjZ = appTan(Viewport->Actor->FovAngle * PI / 360.0);
	RFX2 = 2.0*RProjZ / Frame->FX;
	RFY2 = 2.0*RProjZ*Aspect / Frame->FY;
	projMat = glm::ortho(-RProjZ, +RProjZ, -Aspect*RProjZ, +Aspect*RProjZ, -zFar, zFar);

	// Set viewport and projection.
	glViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);
	bIsOrtho = true;

	//modelviewprojMat=projMat*viewMat*modelMat; //yes, this is right.
	//modelviewMat=viewMat*modelMat;

	glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(modelMat));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMat));
	glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projMat));
	glBufferSubData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(lightSpaceMat));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	UpdateCoords(Frame);

	unguard;
}

void UXOpenGLRenderDevice::SetProjection(FSceneNode* Frame, UBOOL bNearZ)
{
	guard(UXOpenGLRenderDevice::SetProjection);

	// Precompute stuff.
    FLOAT zNear = 0.5f;
    StoredbNearZ = 0;

	if (bNearZ)
    {
        zNear = 0.4f;
        StoredbNearZ = 1;
    }

    FLOAT zFar = (GIsEditor && Frame->Viewport->Actor->RendMap == REN_Wire) ? 131072.0 : 65336.f;

	StoredFovAngle = Viewport->Actor->FovAngle;
	StoredFX = Frame->FX;
	StoredFY = Frame->FY;

	Aspect = Frame->FY / Frame->FX;
	RProjZ = appTan(Viewport->Actor->FovAngle * PI / 360.0);
	RFX2 = 2.0*RProjZ / Frame->FX;
	RFY2 = 2.0*RProjZ*Aspect / Frame->FY;
	projMat = glm::frustum(-RProjZ*zNear, +RProjZ*zNear, -Aspect*RProjZ*zNear, +Aspect*RProjZ*zNear, zNear, zFar);

	// Set viewport and projection.
	glViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);

	//modelviewprojMat=projMat*viewMat*modelMat;
	//modelviewMat=viewMat*modelMat;
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(modelMat));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMat));
	glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projMat));
	glBufferSubData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(lightSpaceMat));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	UpdateCoords(Frame);

	unguard;
}

void UXOpenGLRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UXOpenGLRenderDevice::Lock);

	check(LockCount == 0);
	++LockCount;
	CLEAR_GL_ERROR();

	MakeCurrent();
	CHECK_GL_ERROR();

	// Compensate UED coloring for sRGB.
	if (GIsEditor)
		ScreenClear = FOpenGLGammaDecompress_sRGB(ScreenClear);

	// Clear the Z buffer if needed.
	glClearColor(ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);
	CHECK_GL_ERROR();

	if (glClearDepthf)
		glClearDepthf(1.f);
#ifndef __EMSCRIPTEN__
    #ifndef __LINUX_ARM__
	else
		glClearDepth(1.0);
    #endif
#endif

	if (glDepthRangef)
		glDepthRangef(0.f, 1.f);
#ifndef __EMSCRIPTEN__
    #ifndef __LINUX_ARM__
	else
		glDepthRange(0.0, 1.0);
    #endif
#endif

	CHECK_GL_ERROR();
	glPolygonOffset(-1.f, -1.f);
	SetBlend(PF_Occlude, false);
	glClear(GL_DEPTH_BUFFER_BIT | ((RenderLockFlags&LOCKR_ClearScreen) ? GL_COLOR_BUFFER_BIT : 0));
	CHECK_GL_ERROR();

#if ENGINE_VERSION==227
	LastZMode = 255;
	SetZTestMode(ZTEST_LessEqual);
#else
	glDepthFunc(GL_LEQUAL);
#endif

	// Remember stuff.
	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	// Lock hits.
	HitData = InHitData;
	HitSize = InHitSize;

	// Reset stats.
	appMemzero(&Stats, sizeof(Stats));

	LockHit(InHitData, InHitSize);

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::Unlock(UBOOL Blit)
{
	guard(UXOpenGLRenderDevice::Unlock);

	// Unlock and render.
	check(LockCount == 1);

#ifdef SDL2BUILD
	if (Blit)
	{
		SetProgram(No_Prog);
		SDL_GL_SwapWindow(Window);

		/*
		GLsync SwapFence = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		glClientWaitSync( SwapFence, GL_SYNC_FLUSH_COMMANDS_BIT, 20000000 );
		glDeleteSync( SwapFence );
		*/
	}
#elif QTBUILD

#else
	if (Blit)
	{
		SetProgram(No_Prog);
		verify(SwapBuffers(hDC));
	}
#endif
	//appSleep(0);
	--LockCount;

	// Hits.
	UnlockHit(Blit);
	unguard;
}

void UXOpenGLRenderDevice::BuildGammaRamp(FLOAT GammaCorrection, FGammaRamp& Ramp)
{
#if ENGINE_VERSION==300 // NERF
#define OPENGL_SRGB_GAMMA_OFFSET 0.1f
#elif ENGINE_VERSION==1100 // DEUSEX
#define OPENGL_SRGB_GAMMA_OFFSET 0.2f
#else
#define OPENGL_SRGB_GAMMA_OFFSET 0.15f
#endif

	// HACK HACK.
	if (UseSRGBTextures)
		GammaCorrection += OPENGL_SRGB_GAMMA_OFFSET;

	for (INT i = 0; i<256; i++)
	{
		Ramp.red[i] = Clamp(appRound(appPow(OriginalRamp.red[i] / 65535.f, 0.4f / GammaCorrection)*65535.f), 0, 65535);
		Ramp.green[i] = Clamp(appRound(appPow(OriginalRamp.green[i] / 65535.f, 0.4f / GammaCorrection)*65535.f), 0, 65535);
		Ramp.blue[i] = Clamp(appRound(appPow(OriginalRamp.blue[i] / 65535.f, 0.4f / GammaCorrection)*65535.f), 0, 65535);
	}
}

void UXOpenGLRenderDevice::BuildGammaRamp(FLOAT GammaCorrection, FByteGammaRamp &Ramp) {

	//Parameter clamping

	FLOAT RedGamma = 1.0f / (2.5f * (GammaCorrection));
	FLOAT GreenGamma = 1.0f / (2.5f * (GammaCorrection));
	FLOAT BlueGamma = 1.0f / (2.5f * (GammaCorrection));

	for (GLuint u = 0; u < 256; u++)
	{
		INT iVal, iValRed, iValGreen, iValBlue;

		//Initial value
		iVal = u;

		//Brightness
		iVal += GammaCorrection;

		//Clamping
		if (iVal < 0)
			iVal = 0;
		if (iVal > 255)
			iVal = 255;

		//Gamma
		iValRed = (int)appRound((float)appPow(iVal / 255.0f, RedGamma) * 255.0f);
		iValGreen = (int)appRound((float)appPow(iVal / 255.0f, GreenGamma) * 255.0f);
		iValBlue = (int)appRound((float)appPow(iVal / 255.0f, BlueGamma) * 255.0f);

		//Save results
		Ramp.red[u] = (BYTE)iValRed;
		Ramp.green[u] = (BYTE)iValGreen;
		Ramp.blue[u] = (BYTE)iValBlue;
	}
	return;
}

void UXOpenGLRenderDevice::SetGamma(FLOAT GammaCorrection)
{
	guard(UXOpenGLRenderDevice::SetGamma);

	GammaCorrection += 0.1f; // change range from 0.0-0.9 to 0.1 to 1.0
	Gamma = GammaCorrection;

	unguard;
}

void UXOpenGLRenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UXOpenGLRenderDevice::ClearZ);
	CHECK_GL_ERROR();
	SetSceneNode(Frame);
	SetBlend(PF_Occlude, false);
	glClear(GL_DEPTH_BUFFER_BIT);
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::GetStats(TCHAR* Result)
{
	guard(UXOpenGLRenderDevice::GetStats);
	appSprintf // stijn: mem safety NOT OK
		(
		Result,
		TEXT("XOpenGL stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f GouraudList=%04.1f Tile Buffer/Draw=%04.1f/%04.1f"),
		GSecondsPerCycle * 1000.f * Stats.BindCycles,
		GSecondsPerCycle * 1000.f * Stats.ImageCycles,
		GSecondsPerCycle * 1000.f * Stats.ComplexCycles,
		GSecondsPerCycle * 1000.f * Stats.GouraudPolyCycles,
		GSecondsPerCycle * 1000.f * Stats.GouraudPolyListCycles,
		GSecondsPerCycle * 1000.f * Stats.TileBufferCycles,
		GSecondsPerCycle * 1000.f * Stats.TileDrawCycles
		);
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::ReadPixels(FColor* Pixels)
{
	guard(UXOpenGLRenderDevice::ReadPixels);

	INT x, y;
	INT SizeX, SizeY;

	SizeX = Viewport->SizeX;
	SizeY = Viewport->SizeY;

	glReadPixels(0, 0, SizeX, SizeY, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
	for (INT i = 0; i<SizeY / 2; i++)
	{
		for (INT j = 0; j<SizeX; j++)
		{
			Exchange(Pixels[j + i*SizeX].R, Pixels[j + (SizeY - 1 - i)*SizeX].B);
			Exchange(Pixels[j + i*SizeX].G, Pixels[j + (SizeY - 1 - i)*SizeX].G);
			Exchange(Pixels[j + i*SizeX].B, Pixels[j + (SizeY - 1 - i)*SizeX].R);
		}
	}

	//Gamma correct screenshots
	if (GammaCorrectScreenshots)
	{
		FByteGammaRamp Ramp;
		BuildGammaRamp(GammaOffsetScreenshots, Ramp);
		for (y = 0; y < SizeY; y++)
		{
			for (x = 0; x < SizeX; x++)
			{
				Pixels[x + y * SizeX].R = Ramp.red[Pixels[x + y * SizeX].R];
				Pixels[x + y * SizeX].G = Ramp.green[Pixels[x + y * SizeX].G];
				Pixels[x + y * SizeX].B = Ramp.blue[Pixels[x + y * SizeX].B];
			}
		}
	}
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UXOpenGLRenderDevice::PrecacheTexture);
	SetTexture(0, Info, PolyFlags, 0.0, 0, NORMALTEX);
	unguard;
}

FString GetTrueFalse(UBOOL Value)
{
	return Value == 1 ? TEXT("True") : TEXT("False");
}

void UXOpenGLRenderDevice::Exit()
{
	guard(UXOpenGLRenderDevice::Exit);
	debugf(NAME_Exit, TEXT("XOpenGL: Exit"));

	if (!GIsEditor)
		Flush(0);

#ifdef SDL2BUILD

	UnMapBuffers();
	DeleteShaderBuffers();
	if (SharedBindMap)
	{
		SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
		delete SharedBindMap;
		SharedBindMap = NULL;
	}
#ifndef __EMSCRIPTEN__  // !!! FIXME: upsets Emscripten.
	//Unmap persistent buffer.
	glUnmapBuffer(GL_ARRAY_BUFFER);
#endif

	CurrentGLContext = NULL;
# if !UNREAL_TOURNAMENT_OLDUNREAL
	SDL_GL_DeleteContext(glContext);
# endif
#elif QTBUILD

#else
	// Shut down this GL context. May fail if window was already destroyed.
	check(hRC)
	CurrentGLContext = NULL;
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	verify(AllContexts.RemoveItem(hRC) == 1);
	hRC = NULL;
	if (WasFullscreen)
	{
		ChangeDisplaySettingsW(NULL, 0);
	}

	if (hDC)
		ReleaseDC(hWnd, hDC);

	if (AllContexts.Num() == 0)
	{
		UnMapBuffers();
		DeleteShaderBuffers();
		if (SharedBindMap)
		{
			SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
			delete SharedBindMap;
			SharedBindMap = NULL;
		}
#ifndef __EMSCRIPTEN__  // !!! FIXME: upsets Emscripten.
		//Unmap persistent buffer.
		glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
	}

	// Shut down global GL.
	if (AllContexts.Num() == 0)
		AllContexts.~TArray<HGLRC>();
#endif

#if _WIN32 && ENGINE_VERSION!=227
	TimerEnd();
#endif

	//Why isn't this set automatically??
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("Description"), *FString::Printf(TEXT("%ls"), *Description));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoDrawSimple"), *GetTrueFalse(NoDrawSimple));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoDrawTile"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoDrawTile)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoDrawGouraudList"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoDrawGouraudList)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoDrawGouraud"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoDrawGouraud)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoDrawComplexSurface"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoDrawComplexSurface)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoBuffering"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoBuffering)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseOpenGLDebug"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseOpenGLDebug)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseHWClipping"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseHWClipping)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseHWLighting"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseHWLighting)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseBindlessTextures"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseBindlessTextures)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("SyncToDraw"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(SyncToDraw)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UsePersistentBuffers"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UsePersistentBuffers)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GenerateMipMaps"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(GenerateMipMaps)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseBufferInvalidation"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseBufferInvalidation)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoAATiles"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoAATiles)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("DetailTextures"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(DetailTextures)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("MacroTextures"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(MacroTextures)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("BumpMaps"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(BumpMaps)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GammaCorrectScreenshots"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(GammaCorrectScreenshots)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseAA"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseAA)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseTrilinear"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseTrilinear)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UsePrecache"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UsePrecache)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("AlwaysMipmap"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(AlwaysMipmap)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("ShareLists"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(ShareLists)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoFiltering"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoFiltering)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("HighDetailActors"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(HighDetailActors)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("Coronas"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(Coronas)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("ShinySurfaces"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(ShinySurfaces)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("VolumetricLighting"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(VolumetricLighting)));

	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("MaxAnisotropy"), *FString::Printf(TEXT("%f"), MaxAnisotropy));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("LODBias"), *FString::Printf(TEXT("%f"), LODBias));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GammaOffsetScreenshots"), *FString::Printf(TEXT("%f"), GammaOffsetScreenshots));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("DebugLevel"), *FString::Printf(TEXT("%i"), DebugLevel));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NumAASamples"), *FString::Printf(TEXT("%i"), NumAASamples));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("RefreshRate"), *FString::Printf(TEXT("%i"), RefreshRate));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("DescFlags"), *FString::Printf(TEXT("%i"), DescFlags));

	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseVSync"), *FString::Printf(TEXT("%ls"), UseVSync == VS_Off ? TEXT("Off") : UseVSync == VS_On ? TEXT("On") : TEXT("Adaptive")));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("OpenGLVersion"), *FString::Printf(TEXT("%ls"), OpenGLVersion == GL_Core ? TEXT("Core") : TEXT("ES")));
	unguard;
}

void UXOpenGLRenderDevice::ShutdownAfterError()
{
	guard(UXOpenGLRenderDevice::ShutdownAfterError);
	debugf(NAME_Exit, TEXT("XOpenGL: ShutdownAfterError"));

#if XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP
	Flush(0);
	UnMapBuffers();
	DeleteShaderBuffers();
	if (SharedBindMap)
	{
		SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
		delete SharedBindMap;
		SharedBindMap = NULL;
	}

# ifndef __EMSCRIPTEN__  // !!! FIXME: upsets Emscripten.
	//Unmap persistent buffer.
	glUnmapBuffer(GL_ARRAY_BUFFER);
# endif
#endif

#ifdef SDL2BUILD
	CurrentGLContext = NULL;
# if !UNREAL_TOURNAMENT_OLDUNREAL
	SDL_GL_DeleteContext(glContext);
# endif
#elif QTBUILD

#else
# if XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP
	// Shut down this GL context. May fail if window was already destroyed.
	CurrentGLContext = NULL;
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	AllContexts.RemoveItem(hRC);
	hRC = NULL;
# endif

	if (WasFullscreen)
		ChangeDisplaySettingsW(NULL, 0);

	if (hDC)
		ReleaseDC(hWnd, hDC);

#endif

#if _WIN32 && ENGINE_VERSION!=227
	TimerEnd();
#endif

	unguard;
}

void UXOpenGLRenderDevice::DrawStats(FSceneNode* Frame)
{
	guard(UXOpenGLRenderDevice::DrawStats);
	INT CurY = 48;
	UCanvas* Canvas = Frame->Viewport->Canvas;

#if ENGINE_VERSION<400
	Canvas->DrawColor = FColor(255, 255, 255);
#else
	Canvas->Color = FColor(255, 255, 255);
#endif
	Canvas->CurX = 392;
	Canvas->CurY = CurY;
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("[XOpenGL]"));

	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Cycles: %05.2f"),GSecondsPerCycle * 1000);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Bind____________= %05.2f"), GSecondsPerCycle * 1000 * Stats.BindCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Image___________= %05.2f"), GSecondsPerCycle * 1000 * Stats.ImageCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Blend___________= %05.2f"), GSecondsPerCycle * 1000 * Stats.BlendCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Program_________= %05.2f"), GSecondsPerCycle * 1000 * Stats.ProgramCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Draw2DLine______= %05.2f"), GSecondsPerCycle * 1000 * Stats.Draw2DLine);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Draw3DLine______= %05.2f"), GSecondsPerCycle * 1000 * Stats.Draw3DLine);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Draw2DPoint_____= %05.2f"), GSecondsPerCycle * 1000 * Stats.Draw2DPoint);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Tile_Buffer/Draw= %05.2f/%05.2f"), (GSecondsPerCycle * 1000 * Stats.TileBufferCycles), (GSecondsPerCycle * 1000 * Stats.TileDrawCycles));
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("ComplexSurface__= %05.2f"), GSecondsPerCycle * 1000 * Stats.ComplexCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("GouraudPoly_____= %05.2f"), GSecondsPerCycle * 1000 * Stats.GouraudPolyCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("GouraudPolyList_= %05.2f"), GSecondsPerCycle * 1000 * Stats.GouraudPolyListCycles);
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 24);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Other:"));
	#if ENGINE_VERSION==227
	Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Number of static Lights: %i"), NumStaticLights);
    #endif
    Canvas->CurX = 400;
	Canvas->CurY = (CurY += 12);
	Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Persistent buffer stalls = %i"), Stats.StallCount);

#ifndef __LINUX_ARM__
	if (NVIDIAMemoryInfo)
	{
		GLint CurrentAvailableVideoMemory = 0;
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &CurrentAvailableVideoMemory);
		GLint TotalAvailableVideoMemory = 0;
		glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &TotalAvailableVideoMemory);
		FLOAT Percent = (FLOAT)CurrentAvailableVideoMemory / (FLOAT)TotalAvailableVideoMemory * 100.0f;
		Canvas->CurX = 400;
		Canvas->CurY = (CurY += 12);
		Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("NVidia VRAM (%d MB) Used (%d MB) Usage: %f%%"), TotalAvailableVideoMemory / 1024, (TotalAvailableVideoMemory - CurrentAvailableVideoMemory) / 1024, 100.0f - Percent);
		glGetError();
	}
	if (AMDMemoryInfo)
	{
		GLint CurrentAvailableTextureMemory = 0;
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &CurrentAvailableTextureMemory);
		GLint CurrentAvailableVBOMemory = 0;
		glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, &CurrentAvailableVBOMemory);
		GLint CurrentAvailableRenderbufferMemory = 0;
		glGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, &CurrentAvailableRenderbufferMemory);
		Canvas->CurX = 400;
		Canvas->CurY = (CurY += 12);
		Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("AMD CurrentAvailableTextureMemory (%d MB) CurrentAvailableVBOMemory (%d MB) CurrentAvailableRenderbufferMemory (%d MB)"), CurrentAvailableTextureMemory / 1024, CurrentAvailableVBOMemory / 1024, CurrentAvailableRenderbufferMemory / 1024);
		glGetError();
	}
#endif
	if (UsingBindlessTextures)
	{
		Canvas->CurX = 400;
		Canvas->CurY = (CurY += 12);
		Canvas->WrappedPrintf(Canvas->MedFont, 0, TEXT("Number of bindless Textures in use: %i"), TexNum);
	}

	unguard;
}

// Static variables.
#ifdef SDL2BUILD
SDL_GLContext		UXOpenGLRenderDevice::CurrentGLContext = NULL;
TArray<SDL_GLContext> UXOpenGLRenderDevice::AllContexts;
#elif QTBUILD

#else
HGLRC				UXOpenGLRenderDevice::CurrentGLContext = NULL;
TArray<HGLRC>		UXOpenGLRenderDevice::AllContexts;
PFNWGLCHOOSEPIXELFORMATARBPROC UXOpenGLRenderDevice::wglChoosePixelFormatARB = nullptr;
PFNWGLCREATECONTEXTATTRIBSARBPROC UXOpenGLRenderDevice::wglCreateContextAttribsARB = nullptr;
#endif
INT					UXOpenGLRenderDevice::LockCount = 0;
INT					UXOpenGLRenderDevice::LogLevel = 0;
DWORD				UXOpenGLRenderDevice::ComposeSize = 0;
BYTE*				UXOpenGLRenderDevice::Compose = NULL;

// Shaderprogs
//INT					UXOpenGLRenderDevice::ActiveProgram = -1;

// Gamma
FLOAT				UXOpenGLRenderDevice::Gamma = 0.f;

TOpenGLMap<QWORD, UXOpenGLRenderDevice::FCachedTexture> *UXOpenGLRenderDevice::SharedBindMap;

void autoInitializeRegistrantsXOpenGLDrv(void)
{
	UXOpenGLRenderDevice::StaticClass();
}

/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/

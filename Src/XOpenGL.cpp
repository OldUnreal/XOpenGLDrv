/*=============================================================================
	XOpenGL.cpp: Unreal XOpenGL implementation for OpenGL 3.3+ and GL ES 3.0+

	Copyright 2014-2021 Oldunreal

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

#ifndef _WIN32
#include <sys/time.h>
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

	UEnum* ParallaxVersions = new(GetClass(), TEXT("Parallax"))UEnum(NULL);
	new(ParallaxVersions->Names)FName(TEXT("Disabled"));
	new(ParallaxVersions->Names)FName(TEXT("Basic"));
	new(ParallaxVersions->Names)FName(TEXT("Occlusion"));
	new(ParallaxVersions->Names)FName(TEXT("Relief"));

	new(GetClass(), TEXT("OpenGLVersion"), RF_Public)UByteProperty(CPP_PROPERTY(OpenGLVersion), TEXT("Options"), CPF_Config, OpenGLVersions);
	new(GetClass(), TEXT("UseVSync"), RF_Public)UByteProperty(CPP_PROPERTY(UseVSync), TEXT("Options"), CPF_Config, VSyncs);
	new(GetClass(), TEXT("RefreshRate"), RF_Public)UIntProperty(CPP_PROPERTY(RefreshRate), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NumAASamples"), RF_Public)UIntProperty(CPP_PROPERTY(NumAASamples), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("DetailMax"), RF_Public)UIntProperty(CPP_PROPERTY(DetailMax), TEXT("Options"), CPF_Config);
#if UTGLRFRAMELIMIT // now in Engine (for 227 as well).
	new(GetClass(), TEXT("FrameRateLimit"), RF_Public)UIntProperty(CPP_PROPERTY(FrameRateLimit), TEXT("Options"), CPF_Config);
#endif
	new(GetClass(), TEXT("GammaOffsetScreenshots"), RF_Public)UFloatProperty(CPP_PROPERTY(GammaOffsetScreenshots), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("LODBias"), RF_Public)UFloatProperty(CPP_PROPERTY(LODBias), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("MaxAnisotropy"), RF_Public)UFloatProperty(CPP_PROPERTY(MaxAnisotropy), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GammaMultiplier"), RF_Public)UFloatProperty(CPP_PROPERTY(GammaMultiplier), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GammaMultiplierUED"), RF_Public)UFloatProperty(CPP_PROPERTY(GammaMultiplierUED), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("NoFiltering"), RF_Public)UBoolProperty(CPP_PROPERTY(NoFiltering), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("ShareLists"), RF_Public)UBoolProperty(CPP_PROPERTY(ShareLists), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("AlwaysMipmap"), RF_Public)UBoolProperty(CPP_PROPERTY(AlwaysMipmap), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public)UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseTrilinear"), RF_Public)UBoolProperty(CPP_PROPERTY(UseTrilinear), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseAA"), RF_Public)UBoolProperty(CPP_PROPERTY(UseAA), TEXT("Options"), CPF_Config);
	//new(GetClass(), TEXT("UseAASmoothing"), RF_Public)UBoolProperty(CPP_PROPERTY(UseAASmoothing), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GammaCorrectScreenshots"), RF_Public)UBoolProperty(CPP_PROPERTY(GammaCorrectScreenshots), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("MacroTextures"), RF_Public)UBoolProperty(CPP_PROPERTY(MacroTextures), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("BumpMaps"), RF_Public)UBoolProperty(CPP_PROPERTY(BumpMaps), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("ParallaxVersion"), RF_Public)UByteProperty(CPP_PROPERTY(ParallaxVersion), TEXT("Options"), CPF_Config, ParallaxVersions);
	new(GetClass(), TEXT("NoAATiles"), RF_Public)UBoolProperty(CPP_PROPERTY(NoAATiles), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GenerateMipMaps"), RF_Public)UBoolProperty(CPP_PROPERTY(GenerateMipMaps), TEXT("Options"), CPF_Config);

	new(GetClass(), TEXT("OneXBlending"), RF_Public)UBoolProperty(CPP_PROPERTY(OneXBlending), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("ActorXBlending"), RF_Public)UBoolProperty(CPP_PROPERTY(ActorXBlending), TEXT("Options"), CPF_Config);

	// Experimental stuff (still being worked on).
	new(GetClass(), TEXT("UseSRGBTextures"), RF_Public)UBoolProperty(CPP_PROPERTY(UseSRGBTextures), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("SimulateMultiPass"), RF_Public)UBoolProperty(CPP_PROPERTY(SimulateMultiPass), TEXT("Options"), CPF_Config);

#if ENGINE_VERSION==227
	// new(GetClass(), TEXT("UseHWLighting"), RF_Public)UBoolProperty(CPP_PROPERTY(UseHWLighting), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseHWClipping"), RF_Public)UBoolProperty(CPP_PROPERTY(UseHWClipping), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseEnhancedLightmaps"), RF_Public)UBoolProperty(CPP_PROPERTY(UseEnhancedLightmaps), TEXT("Options"), CPF_Config);
	//new(GetClass(),TEXT("UseMeshBuffering"),		RF_Public)UBoolProperty	( CPP_PROPERTY(UseMeshBuffering			), TEXT("Options"), CPF_Config);
#endif

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	// OpenGL 4
	new(GetClass(), TEXT("UsePersistentBuffers"), RF_Public)UBoolProperty(CPP_PROPERTY(UsePersistentBuffers), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseBindlessTextures"), RF_Public)UBoolProperty(CPP_PROPERTY(UseBindlessTextures), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("UseShaderDrawParameters"), RF_Public)UBoolProperty(CPP_PROPERTY(UseShaderDrawParameters), TEXT("Options"), CPF_Config);
	
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

#if UNREAL_TOURNAMENT_OLDUNREAL && !defined(__LINUX_ARM__)
	new(GetClass(), TEXT("UseLightmapAtlas"), RF_Public)UBoolProperty(CPP_PROPERTY(UseLightmapAtlas), TEXT("Options"), CPF_Config);
	FindField<UBoolProperty>(GetClass(), TEXT("UseLightmapAtlas"))->PropertyFlags |= CPF_EditConst; // Do not allow modification in runtime
#endif
	//new(GetClass(),TEXT("EnableShadows"),			RF_Public)UBoolProperty ( CPP_PROPERTY(EnableShadows			), TEXT("Options"), CPF_Config);

	// Defaults.
	RefreshRate = 0;
#if UTGLRFRAMELIMIT
	FrameRateLimit = 60;
#endif
	NumAASamples = 4;
	GammaOffsetScreenshots = 0.7f;
	LODBias = 0.f;
	MaxAnisotropy = 4.f;
	UseHWClipping = 1;
	UsePrecache = 1;
	ShareLists = 1;
	UseAA = 1;
	UseAASmoothing = 0;
	GammaCorrectScreenshots = 1;
	MacroTextures = 1;
	BumpMaps = 1;
	GammaMultiplier = 1.75f;
	GammaMultiplierUED  = 1.75f;
#ifdef __EMSCRIPTEN__
	ParallaxVersion = Parallax_Disabled;
#elif __LINUX_ARM__
	ParallaxVersion = Parallax_Disabled;
#else
	ParallaxVersion = Parallax_Disabled;
#endif
	UseTrilinear = 1;
	NoAATiles = 1;
	UseMeshBuffering = 0; //Buffer (Static)Meshes for drawing.
#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	UseBindlessTextures = 1;
	UseShaderDrawParameters = 1; // setting this to true slightly improves performance on nvidia cards
#else
	UseBindlessTextures = 0;
	UseShaderDrawParameters = 0;
#endif
	UseHWLighting = 0;
	AlwaysMipmap = 0;
	NoFiltering = 0;
	UseSRGBTextures = 0;
	EnvironmentMaps = 0;
	GenerateMipMaps = 0;
	//EnableShadows = 0;

	OneXBlending = 0;
	ActorXBlending = 0;

	MaxTextureSize = 4096;
#ifdef __EMSCRIPTEN__
	OpenGLVersion = GL_ES;
    UseEnhancedLightmaps = 0; // CHECK ME! GL_ES and UseEnhancedLightmaps don't like each other very much (yet?).
#elif __LINUX_ARM__
	OpenGLVersion = GL_ES;
    UseEnhancedLightmaps = 0;
#else
	OpenGLVersion = GL_Core;
    UseEnhancedLightmaps = 1;
#endif
	UseVSync = VS_Adaptive;

#if UNREAL_TOURNAMENT_OLDUNREAL
	DetailMax = 2;
#endif

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

#if UNREAL_TOURNAMENT_OLDUNREAL && !defined(__LINUX_ARM__)
	UseLightmapAtlas = 1;
	SupportsUpdateTextureRect = 1;
#endif

	unguard;
}

UBOOL UXOpenGLRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UXOpenGLRenderDevice::Init);
	//it is really a bad habit of the UEngine1 games to call init again each time a fullscreen change is needed.

	Viewport = InViewport;
	bContextInitialized = false;
	bGlobalBuffersMapped = false;
	iPixelFormat = 0;

	DetailMax = Clamp(DetailMax,0,3);

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
	SupportsTC = TRUE;
	SupportsFogMaps = 1;
	SupportsDistanceFog = 1;
	SupportsLazyTextures = 0;
	PrefersDeferredLoad = 0;
#if ENGINE_VERSION==227
	SupportsHDLightmaps = UseEnhancedLightmaps;
	UnsupportHDLightFlags = 0;
	SupportsAlphaBlend = 1;
	SupportsNewBTC = TRUE;
	DecompFormat = TEXF_RGBA8_;
#endif

	// Extensions & other inits.
	ActiveProgram = No_Prog;
	SupportsAMDMemoryInfo = false;
	SupportsNVIDIAMemoryInfo = false;
	SupportsSwapControl = false;
	SupportsSwapControlTear = false;
	SupportsClipDistance = true;
	SupportsS3TC = true; //assume nowadays every hardware setup supports this, but its checked later anyway.

	if (ParallaxVersion != Parallax_Disabled) // Not sure if Parallax makes much sense at all without BumpMaps, but for now we need it enabled to have the necessary informations from the vertex shader.
        BumpMaps = 1;

#if XOPENGL_TEXTUREHANDLE_SUPPORT
	BindlessList = NULL;
#endif

	// Verbose Logging
	debugf(NAME_DevLoad, TEXT("XOpenGL: Current settings"));

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	debugf(NAME_DevLoad, TEXT("UseBindlessTextures %i"), UseBindlessTextures);
	debugf(NAME_DevLoad, TEXT("UseShaderDrawParameters %i"), UseShaderDrawParameters);
	debugf(NAME_DevLoad, TEXT("UseHWLighting %i"), UseHWLighting);
	debugf(NAME_DevLoad, TEXT("UseHWClipping %i"), UseHWClipping);
#endif
	debugf(NAME_DevLoad, TEXT("UseTrilinear %i"), UseTrilinear);
	debugf(NAME_DevLoad, TEXT("UsePrecache %i"), UsePrecache);
	debugf(NAME_DevLoad, TEXT("UseAA %i"), UseAA);
	//debugf(NAME_DevLoad, TEXT("UseAASmoothing %i"), UseAASmoothing);
	debugf(NAME_DevLoad, TEXT("NumAASamples %i"), NumAASamples);

	debugf(NAME_DevLoad, TEXT("RefreshRate %i"), RefreshRate);
	debugf(NAME_DevLoad, TEXT("GammaOffsetScreenshots %f"), GammaOffsetScreenshots);
	debugf(NAME_DevLoad, TEXT("LODBias %f"), LODBias);
	debugf(NAME_DevLoad, TEXT("MaxAnisotropy %f"), MaxAnisotropy);
	debugf(NAME_DevLoad, TEXT("ShareLists %i"), ShareLists);
	debugf(NAME_DevLoad, TEXT("AlwaysMipmap %i"), AlwaysMipmap);
	debugf(NAME_DevLoad, TEXT("NoFiltering %i"), NoFiltering);
	debugf(NAME_DevLoad, TEXT("UseSRGBTextures %i"),UseSRGBTextures);
	debugf(NAME_DevLoad, TEXT("SimulateMultiPass %i"),SimulateMultiPass);
	debugf(NAME_DevLoad, TEXT("GammaMultiplier %f"),GammaMultiplier);
    debugf(NAME_DevLoad, TEXT("GammaMultiplierUED %f"),GammaMultiplierUED);

	debugf(NAME_DevLoad, TEXT("GammaCorrectScreenshots %i"), GammaCorrectScreenshots);
	debugf(NAME_DevLoad, TEXT("MacroTextures %i"), MacroTextures);
	debugf(NAME_DevLoad, TEXT("BumpMaps %i"), BumpMaps);
	debugf(NAME_DevLoad, TEXT("ParallaxVersion %i (%ls)"),ParallaxVersion, ParallaxVersion == Parallax_Basic ? TEXT("Basic") : ParallaxVersion == Parallax_Occlusion ? TEXT("Occlusion") : ParallaxVersion == Parallax_Relief ? TEXT("Relief") : TEXT("Disabled"));
	debugf(NAME_DevLoad, TEXT("EnvironmentMaps %i"), EnvironmentMaps);
	debugf(NAME_DevLoad, TEXT("NoAATiles %i"), NoAATiles);
	debugf(NAME_DevLoad, TEXT("GenerateMipMaps %i"), GenerateMipMaps);
	//debugf(NAME_DevLoad, TEXT("UseLightmapAtlas %i"), UseLightmapAtlas);

	//debugf(NAME_DevLoad, TEXT("EnableShadows %i"), EnableShadows);

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
		SupportedDisplayModes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
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
		UsingPersistentBuffers = UsePersistentBuffers ? true : false;
		UsingShaderDrawParameters = UseShaderDrawParameters ? true : false;

		if (OpenGLVersion == GL_ES)
        {
            if (UseShaderDrawParameters)
                GWarn->Logf(TEXT("OpenGL ES does not support gl_DrawID at this time, disabling UseShaderDrawParameters"));
            UsingShaderDrawParameters = false;

            if (SimulateMultiPass)
                GWarn->Logf(TEXT("OpenGL ES does not support SimulateMultiPass at this time, disabling SimulateMultiPass"));
            SimulateMultiPass = false;

			SupportsGLSLInt64 = SupportsSSBO = false;
        }

		// Bindless Textures
		UsingBindlessTextures = UseBindlessTextures ? true : false;
#else
		UsingBindlessTextures = false;
		UsingPersistentBuffers = false;
		UsingShaderDrawParameters = false;
#endif
	}

	if (OpenGLVersion == GL_Core
#if MACOSX
		&& 0
#endif
		)
	{
		// macOS performance tanks when we use these
		UsingGeometryShaders = true;
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

	UsingPersistentBuffersTile = UsingPersistentBuffers;
	UsingPersistentBuffersComplex = UsingPersistentBuffers;//unless being able to batch bigger amount of draw calls this is significantly slower. Unfortunately can't handle enough textures right now. With LightMaps it easily reaches 12k and more.
	UsingPersistentBuffersGouraud = UsingPersistentBuffers;
	UsingPersistentBuffersDrawcallParams = UsingPersistentBuffers; // setting this to true fixes shaderdrawparameters on AMD GPUs, but drastically reduces performance
	UsingPersistentBuffersSimple = UsingPersistentBuffers;

	// Init shaders
	InitShaders();

	const auto GlobalMatrices = GlobalMatricesBuffer.GetElementPtr(0);
	check(GlobalMatrices);

	// Set view matrix.
	GlobalMatrices->viewMat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));

	// Initial proj matrix (updated to actual values in SetSceneNode).
	GlobalMatrices->projMat = glm::mat4(1.0f);

	// Identity
	GlobalMatrices->modelMat = glm::mat4(1.0f);

	if (UseHWLighting)
		InViewport->GetOuterUClient()->NoLighting = 1; // Disable (Engine) lighting.

	// stijn: This ResetFog call was missing and caused the black screen bug in UT469
	if (bGlobalBuffersMapped)
		ResetFog();

	if (UseAA)
		glEnable(GL_MULTISAMPLE);

	return 1;
	unguard;
}

void UXOpenGLRenderDevice::PostEditChange()
{
	guard(UXOpenGLRenderDevice::PostEditChange);
	debugf(NAME_DevGraphics, TEXT("XOpenGL: PostEditChange"));

	MakeCurrent();

	// stijn: We shouldn't re-check extensions here unless we recreate the entire renderer!!
	//CheckExtensions();
	RecompileShaders();

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
		GWarn->Logf(TEXT("XOpenGL: Setting PixelFormat %i failed!"), iPixelFormat);
		iPixelFormat = ChoosePixelFormat(hDC, &pfd);
		if (!SetPixelFormat(hDC, iPixelFormat, &pfd))
			GWarn->Logf(TEXT("XOpenGL: SetPixelFormat %i failed. Restart may required."), iPixelFormat);
		return 0;
	}
	else debugf(NAME_DevGraphics, TEXT("XOpenGL: Setting PixelFormat: %i"), iPixelFormat);
	#endif
	return 1;
	unguard;
}

#ifdef SDL2BUILD
UBOOL UXOpenGLRenderDevice::SetSDLAttributes()
{
    guard(UXOpenGLRenderDevice::SetSDLAttributes);
    INT SDLError = 0;

	SDLError = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, DesiredColorBits);

    if (UseAA)
    {
        SDLError = SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);
        SDLError = SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, NumAASamples);
    }
	SDLError = SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, DesiredDepthBits);

    SDLError = SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,UseSRGBTextures); // CheckMe!!! Does this work in GL ES?

	if (UseOpenGLDebug)
		SDLError = SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	if (OpenGLVersion == GL_ES)
		SDLError = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else
		SDLError = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // SDL_GL_CONTEXT_PROFILE_COMPATIBILITY

    if (SDLError != 0)
        debugf(NAME_DevLoad, TEXT("XOpenGL: SDL Error in SetSDLAttributes (probably non fatal): %ls"), appFromAnsi(SDL_GetError()));

    return !SDLError;

    unguard;
}
#endif


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

	debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: Creating new OpenGL context."));

	INT MajorVersion = 4;
	INT MinorVersion = 5;

	if (OpenGLVersion == GL_ES)
	{
		MajorVersion = 3;
		MinorVersion = 1;
	}
	else
    {
        if (UseOpenGLDebug)
        {
            MajorVersion = 4;
            MinorVersion = 3;
        }
#if MACOSX
		MajorVersion = 4;
		MinorVersion = 1;
#else

        if (UseBindlessTextures || UsePersistentBuffers)
        {
            MajorVersion = 4;
            MinorVersion = 5;
        }
        if (UseShaderDrawParameters)
		{
			MajorVersion = 4;
			MinorVersion = 6;
		}
#endif
    }

	iPixelFormat = 0;


#ifdef SDL2BUILD
InitContext:

    if (glContext)
        SDL_GL_DeleteContext(glContext);

    INT SDLError = 0;
	// Tell SDL what kind of context we want
    SDLError = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, MajorVersion);
	SDLError = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, MinorVersion);

    if (SDLError != 0)
        debugf(NAME_DevLoad, TEXT("XOpenGL: SDL Error in CreateOpenGLContext (probably non fatal): %ls"), appFromAnsi(SDL_GetError()));

	// not checking for any existing SDL context, create a new one, since using
	// SDL for splash already and it's getting confused.
	//

	// Init global GL.
	// no need to do SDL_Init here, done in SDL2Drv (USDL2Viewport::ResizeViewport).

	Window = (SDL_Window*)Viewport->GetWindow();

	if (!Window)
		appErrorf(TEXT("XOpenGL: No SDL Window found!"));

	glContext = SDL_GL_CreateContext(Window);

	if (glContext == NULL)
	{
	    appErrorf(TEXT("XOpenGL: SDL Error in CreateOpenGLContext: %ls"), appFromAnsi(SDL_GetError()));
        if (OpenGLVersion == GL_Core)
        {
            if (UseBindlessTextures || UsePersistentBuffers || UseShaderDrawParameters)
            {
                if (MajorVersion == 3 && MinorVersion == 3) // already 3.3
                {
                    appErrorf(TEXT("XOpenGL: Failed to init minimum OpenGL (%i.%i context). SDL_GL_CreateContext: %ls"), MajorVersion, MinorVersion, appFromAnsi(SDL_GetError()));
                    return 0;
                }
                else
                {
                    //Safemode, try with lowest supported context, disable >3.3 features.
                    MajorVersion = 3;
                    MinorVersion = 3;

                    UsingBindlessTextures = false;
                    UsingPersistentBuffers = false;
					UsingShaderDrawParameters = false;
					UseOpenGLDebug = false;

                    debugf(NAME_Init, TEXT("XOpenGL: %i.%i context failed to initialize. Disabling UseShaderDrawParameters,UseBindlessTextures,UsePersistentBuffers and UseOpenGLDebug, switching to 3.3 context (min. version)."), MajorVersion, MinorVersion);

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

	if (OpenGLVersion == GL_ES)
		gladLoadGLES2Loader(SDL_GL_GetProcAddress);
	else
		gladLoadGLLoader(SDL_GL_GetProcAddress);

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
		glDebugMessageCallback(UXOpenGLRenderDevice::DebugCallback, NULL);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		GWarn->Logf(TEXT("XOpenGL: OpenGL debugging enabled, this can cause severe performance drain!"));
	}

	if (NoBuffering)
	{
		GWarn->Logf(TEXT("XOpenGL: NoBuffering enabled, this WILL cause severe performance drain! This debugging option is useful to find performance critical situations, but should not be used in general."));
	}

	AllContexts.AddItem(glContext);
#else
	if (!bContextInitialized)
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
			GWarn->Logf(TEXT("XOpenGL: RegisterClassEx failed!"));
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
		bContextInitialized = true;

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
					//Safemode, try with lowest supported context, disable >3.3 features.
					MajorVersion = 3;
					MinorVersion = 3;

					UsingBindlessTextures = false;
					UsingPersistentBuffers = false;
					UsingShaderDrawParameters = false;
                    UseOpenGLDebug = false;

                    debugf(NAME_Init, TEXT("XOpenGL: %i.%i context failed to initialize. Disabling UseShaderDrawParameters,UseBindlessTextures,UsePersistentBuffers and UseOpenGLDebug, switching to 3.3 context (min. version)."), MajorVersion, MinorVersion);
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
		check(wglShareLists(AllContexts(0), hRC) == 1);
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
		debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: OpenGL debugging disabled."));
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
	if (!CurrentGLContext || CurrentGLContext != glContext)
	{
		//debugf(TEXT("XOpenGL: MakeCurrent"));
		INT Result = SDL_GL_MakeCurrent(Window, glContext);
		if (Result != 0)
			debugf(TEXT("XOpenGL: MakeCurrent failed with: %ls\n"), appFromAnsi(SDL_GetError()));
		CurrentGLContext = glContext;
	}
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
	glDepthMask(GL_TRUE);
	glPolygonOffset(-1.0f, -1.0f);
	glBlendFunc(GL_ONE, GL_ZERO);
	glEnable(GL_BLEND);
	glDisable(GL_CLIP_DISTANCE0);

	/*
	GLES 3 supports sRGB functionality, but it does not expose the GL_FRAMEBUFFER_SRGB enable/disable bit.
	*/

#ifndef __EMSCRIPTEN__
	if (UseSRGBTextures && OpenGLVersion == GL_Core)
		glEnable(GL_FRAMEBUFFER_SRGB);
#endif

    if (UseAA && UseAASmoothing)
    {
        glEnable( GL_LINE_SMOOTH );
        glEnable( GL_POLYGON_SMOOTH );
        glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
        glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
        /*
        On some implementations, when you call glEnable(GL_POINT_SMOOTH) or glEnable(GL_LINE_SMOOTH) and you use shaders at the same time, your rendering speed goes down to 0.1 FPS.
        This is because the driver does software rendering. This would happen on AMD/ATI GPUs/drivers.
        */
    }
    if ( GenerateMipMaps ) // Is there really a visible difference at all?
    {
        if (OpenGLVersion == GL_ES)
            glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST); //this particular setting is GL ES only.

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 16); // set maximum to...? Most common is 8 I think. However if really want to benefit from it maybe using some more here.
    }

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

	DesiredColorBits = NewColorBytes <= 2 ? 16 : 32;
#if __APPLE__
	DesiredStencilBits = 0;
	DesiredDepthBits = 32;
#else
	DesiredStencilBits = NewColorBytes <= 2 ? 0 : 8;
	DesiredDepthBits = NewColorBytes <= 2 ? 16 : 24;
#endif

	debugf(NAME_DevGraphics, TEXT("XOpenGL: DesiredColorBits %i,DesiredStencilBits %i, DesiredDepthBits %i "), DesiredColorBits, DesiredStencilBits, DesiredDepthBits);

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

#ifndef SDL2BUILD
	// Change display settings.
	if (Fullscreen)
	{
		INT FindX = NewX, FindY = NewY, BestError = MAXINT;
		for (INT i = 0; i < SupportedDisplayModes.Num(); i++)
		{
			if (SupportedDisplayModes(i).Z == (NewColorBytes * 8))
			{
				INT Error
					= (SupportedDisplayModes(i).X - FindX)*(SupportedDisplayModes(i).X - FindX)
					+ (SupportedDisplayModes(i).Y - FindY)*(SupportedDisplayModes(i).Y - FindY);
				if (Error < BestError)
				{
					NewX = SupportedDisplayModes(i).X;
					NewY = SupportedDisplayModes(i).Y;
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

		debugf(NAME_DevGraphics, TEXT("XOpenGL: Fullscreen NewX %i NewY %i"), NewX, NewY);

		if (RefreshRate)
		{
			dma.dmDisplayFrequency = dmw.dmDisplayFrequency = RefreshRate;
			dma.dmFields |= DM_DISPLAYFREQUENCY;
			dmw.dmFields |= DM_DISPLAYFREQUENCY;

			if (ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				GWarn->Logf(TEXT("XOpenGL: ChangeDisplaySettings failed: %ix%i, %i Hz"), NewX, NewY, RefreshRate);
				dma.dmFields &= ~DM_DISPLAYFREQUENCY;
				dmw.dmFields &= ~DM_DISPLAYFREQUENCY;
			}
			else
			{
				debugf(NAME_DevGraphics, TEXT("ChangeDisplaySettings with RefreshRate: %ix%i, %i Hz"), NewX, NewY, RefreshRate);
				tryNoRefreshRate = false;
			}
		}
		if (tryNoRefreshRate)
		{
			if (ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				GWarn->Logf(TEXT("XOpenGL: ChangeDisplaySettings failed: %ix%i"), NewX, NewY);
				return 0;
			}
			debugf(NAME_DevGraphics, TEXT("XOpenGL: ChangeDisplaySettings: %ix%i"), NewX, NewY);
		}
	}
	else UnsetRes();

	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
	if (!Result)
	{
		GWarn->Logf(TEXT("XOpenGL: Change window size failed!"));
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

    if (!Window)
        SetSDLAttributes(); // Those have to been set BEFORE window creation in SDL2Drv.

	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
	if (!Result)
	{
		debugf(TEXT("XOpenGL: Change window size failed!"));
		return 0;
	}

	if (glContext)
    {
        if (glContext != CurrentGLContext)
            MakeCurrent();
    }
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
		else
        {
            debugf(NAME_Init, TEXT("XOpenGL: VSync Off"));
        }
		break;
	case VS_On:
		if (SDL_GL_SetSwapInterval(1) != 0)
			debugf(NAME_Init, TEXT("XOpenGL: Setting VSync on has failed."));
		else
        {
            debugf(NAME_Init, TEXT("XOpenGL: VSync On"));
        }
		break;
	case VS_Adaptive:
		if (SDL_GL_SetSwapInterval(-1) != 0)
        {
			debugf(NAME_Init, TEXT("XOpenGL: Setting VSync adaptive has failed. Falling back to SwapInterval 0 (VSync Off)."));
			if (SDL_GL_SetSwapInterval(0) != 0)
                debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
        }
		else
        {
            debugf(NAME_Init, TEXT("XOpenGL: VSync Adaptive"));
        }
		break;
	default:
		if (SDL_GL_SetSwapInterval(0) != 0)
			debugf(NAME_Init, TEXT("XOpenGL: Setting default VSync off has failed."));
		else
        {
            debugf(NAME_Init, TEXT("XOpenGL: VSync Off (default)"));
        }
	}
	unguard;
#else
	guard(SwapControl);
	if (SupportsSwapControl)
	{
		PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		if (!wglSwapIntervalEXT)
			return;
		if (GIsEditor) // Remove logspam in Editor Window
		{
			switch (UseVSync)
			{
			case VS_On:
				wglSwapIntervalEXT(1);
				break;
			case VS_Off:
				wglSwapIntervalEXT(0);
				break;

			case VS_Adaptive:
				if (!SupportsSwapControlTear)
					wglSwapIntervalEXT(0);
				else
					wglSwapIntervalEXT(-1);
				break;
			default:
				wglSwapIntervalEXT(0);
			}
		}
		else
		{
			switch (UseVSync)
			{
			case VS_On:
				if (wglSwapIntervalEXT(1) != 1)
					debugf(NAME_Init, TEXT("XOpenGL: Setting VSync on has failed."));
				else
                {
                    debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: On"));
                }
				break;
			case VS_Off:
				if (wglSwapIntervalEXT(0) != 1)
					debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
				else
                {
                    debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Off"));
                }
				break;

			case VS_Adaptive:
				if (!SupportsSwapControlTear)
				{
					debugf(NAME_Init, TEXT("XOpenGL: WGL_EXT_swap_control_tear is not supported by device. Falling back to SwapInterval 0 (VSync Off)."));
					if (wglSwapIntervalEXT(0) != 1)
						debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
					else
                    {
                        debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Off"));
                    }
					break;
				}
				if (wglSwapIntervalEXT(-1) != 1)
					debugf(NAME_Init, TEXT("XOpenGL: Setting VSync adaptive has failed."));
				else
				{
                    debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Adaptive"));
				}
				break;
			default:
				if (wglSwapIntervalEXT(0) != 1)
					debugf(NAME_Init, TEXT("XOpenGL: Setting VSync off has failed."));
				else
                {
                    debugf(NAME_Init, TEXT("XOpenGL: Setting VSync: Off (default)"));
                }
			}
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

	debugf(NAME_DevGraphics, TEXT("XOpenGL: Flush"));

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

	if (LightList.Num())
		LightList.Empty();

	NumStaticLights = 0;
	NumLights = 0;

	TArray<GLuint> Binds;
	for (TOpenGLMap<QWORD, FCachedTexture>::TIterator It(*BindMap); It; ++It)
	{
		CHECK_GL_ERROR();
		glDeleteSamplers(1, &It.Value().Sampler);

		if (UsingBindlessTextures)
		{
			if (It.Value().BindlessTexHandle && glIsTextureHandleResidentARB(It.Value().BindlessTexHandle))
				glMakeTextureHandleNonResidentARB(It.Value().BindlessTexHandle);
		}

		It.Value().BindlessTexHandle = 0;
		if (It.Value().Id)
		{
			glDeleteTextures(1, &It.Value().Id);
			Binds.AddItem(It.Value().Id);
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

	SwapControl();

	StoredOrthoFovAngle = 0;
	StoredOrthoFX = 0;
	StoredOrthoFY = 0;
	StoredFovAngle = 0;
	StoredFX = 0;
	StoredFY = 0;
	appMemzero(&Stats, sizeof(Stats));

	if (bGlobalBuffersMapped)
        ResetFog();

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
		for (i = 0; i< SupportedDisplayModes.Num(); i++)
			if (SupportedDisplayModes(i).Z == Viewport->ColorBytes * 8)
				if
					((SupportedDisplayModes(i).X != 320 || SupportedDisplayModes(i).Y != 200)
					&& (SupportedDisplayModes(i).X != 640 || SupportedDisplayModes(i).Y != 400))
					Relevant.AddUniqueItem(FPlane(SupportedDisplayModes(i).X, SupportedDisplayModes(i).Y, 0, 0));
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
		debugf(NAME_DevGraphics, TEXT("XOpenGL: VideoFlush"));
		Flush(1);
		return 1;
	}
	return 0;
	unguard;
}

void UXOpenGLRenderDevice::UpdateCoords(FSceneNode* Frame)
{
    guard(UXOpenGLRenderDevice::UpdateCoords);

	auto Coords = GlobalCoordsBuffer.GetElementPtr(0); 

    // Update Coords:  World to Viewspace projection.
	Coords->FrameCoords[0] = glm::vec4(Frame->Coords.Origin.X, Frame->Coords.Origin.Y, Frame->Coords.Origin.Z, 0.0f);
	Coords->FrameCoords[1] = glm::vec4(Frame->Coords.XAxis.X, Frame->Coords.XAxis.Y, Frame->Coords.XAxis.Z, 0.0f);
	Coords->FrameCoords[2] = glm::vec4(Frame->Coords.YAxis.X, Frame->Coords.YAxis.Y, Frame->Coords.YAxis.Z, 0.0f);
	Coords->FrameCoords[3] = glm::vec4(Frame->Coords.ZAxis.X, Frame->Coords.ZAxis.Y, Frame->Coords.ZAxis.Z, 0.0f);

	// And UnCoords: Viewspace to World projection.
	Coords->FrameUncoords[0] = glm::vec4(Frame->Uncoords.Origin.X, Frame->Uncoords.Origin.Y, Frame->Uncoords.Origin.Z, 0.0f);
	Coords->FrameUncoords[1] = glm::vec4(Frame->Uncoords.XAxis.X, Frame->Uncoords.XAxis.Y, Frame->Uncoords.XAxis.Z, 0.0f);
	Coords->FrameUncoords[2] = glm::vec4(Frame->Uncoords.YAxis.X, Frame->Uncoords.YAxis.Y, Frame->Uncoords.YAxis.Z, 0.0f);
	Coords->FrameUncoords[3] = glm::vec4(Frame->Uncoords.ZAxis.X, Frame->Uncoords.ZAxis.Y, Frame->Uncoords.ZAxis.Z, 0.0f);

	GlobalCoordsBuffer.Bind();
	GlobalCoordsBuffer.BufferData(false, false, GL_DYNAMIC_DRAW);
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guard(UXOpenGLRenderDevice::SetSceneNode);

	auto Level = Frame->Level;
	// stijn: this is also very very slow. FPS goes down by 50% by pushing these uniforms every frame
#if !__APPLE__
	if ( !UseHWLighting && Level && (!NumStaticLights || GIsEditor))
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
                if (UseHWLighting || Actor->NormalLightRadius) //for normal mapping only add lights with normallightradius set. Needs performance tests if not.
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
			StaticLightData.LightData5[i] = glm::vec4(StaticLightList(i)->NormalLightRadius, (GLfloat)StaticLightList(i)->bZoneNormalLight, StaticLightList(i)->LightBrightness, 0.0);
#endif
			if (i == MAX_LIGHTS - 1)
				break;
		}
		
		StaticLightInfoBuffer.Bind();
		StaticLightInfoBuffer.BufferData(false, false, GL_DYNAMIC_DRAW);
		CHECK_GL_ERROR();
	}
	else if (UseHWLighting && Level)
	{
		LightList.Empty();

		for (INT i = 0; i < Level->Actors.Num(); ++i)
		{
			AActor* Actor = Level->Actors(i);
			if (Actor && !Actor->bDeleteMe && Actor->LightBrightness && Actor->LightType && Actor->LightRadius)
				LightList.AddItem(Actor);
		}
		NumLights = LightList.Num();

		for (INT i = 0; i < NumLights; i++)
		{
			//debugf(TEXT("LightList: %ls %f %f %f"), LightList(i)->GetName(), LightList(i)->Location.X, LightList(i)->Location.Y, LightList(i)->Location.Z);

			StaticLightData.LightPos[i] = glm::vec4(LightList(i)->Location.X, LightList(i)->Location.Y, LightList(i)->Location.Z, 1.f);

			FPlane RGBColor = FGetHSV(LightList(i)->LightHue, LightList(i)->LightSaturation, LightList(i)->LightBrightness);

#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
			StaticLightData.LightData1[i] = glm::vec4(RGBColor.X, RGBColor.Y, RGBColor.Z, LightList(i)->LightCone);
#else
			StaticLightData.LightData1[i] = glm::vec4(RGBColor.R, RGBColor.G, RGBColor.B, LightList(i)->LightCone);
#endif
			StaticLightData.LightData2[i] = glm::vec4(LightList(i)->LightEffect, LightList(i)->LightPeriod, LightList(i)->LightPhase, LightList(i)->LightRadius);
			StaticLightData.LightData3[i] = glm::vec4(LightList(i)->LightType, LightList(i)->VolumeBrightness, LightList(i)->VolumeFog, LightList(i)->VolumeRadius);
#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
			StaticLightData.LightData4[i] = glm::vec4(LightList(i)->WorldLightRadius(), NumLights, (GLfloat)LightList(i)->Region.ZoneNumber, (GLfloat)(Frame->Viewport->Actor ? Frame->Viewport->Actor->Region.ZoneNumber : 0.f));
			StaticLightData.LightData5[i] = glm::vec4(LightList(i)->LightRadius * 10, 1.0, 0.0, 0.0);
#else
			StaticLightData.LightData4[i] = glm::vec4(LightList(i)->WorldLightRadius(), NumLights, (GLfloat)LightList(i)->Region.ZoneNumber, (GLfloat)(Frame->Viewport->Actor ? Frame->Viewport->Actor->CameraRegion.ZoneNumber : 0.f));
			StaticLightData.LightData5[i] = glm::vec4(LightList(i)->NormalLightRadius, (GLfloat)LightList(i)->bZoneNormalLight, LightList(i)->LightBrightness, 0.0);
#endif
			if (i == MAX_LIGHTS - 1)
				break;
		}

		StaticLightInfoBuffer.Bind();
		StaticLightInfoBuffer.BufferData(false, false, GL_DYNAMIC_DRAW);
		CHECK_GL_ERROR();
	}
	CHECK_GL_ERROR();
#endif

	//Flush Buffers.
	SetProgram(No_Prog);

#if ENGINE_VERSION==227
	if (bFogEnabled)
		ResetFog();
#endif // ENGINE_VERSION

	//avoid some overhead, only calculate and set again if something was really changed.
	if (Frame->Viewport->IsOrtho() && (!bIsOrtho || StoredOrthoFovAngle != Viewport->Actor->FovAngle || StoredOrthoFX != Frame->FX || GIsEditor || StoredOrthoFY != Frame->FY))
		SetOrthoProjection(Frame);
	else if (StoredFovAngle != Viewport->Actor->FovAngle || StoredFX != Frame->FX || StoredFY != Frame->FY || GIsEditor || StoredbNearZ)
		SetProjection(Frame, 0);

	// Set clip planes.
	SetSceneNodeHit(Frame);

	// Disable clipping
#if ENGINE_VERSION==227
	while (NumClipPlanes > 0)
		PopClipPlane();
#endif

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

	const auto GlobalMatrices = GlobalMatricesBuffer.GetElementPtr(0);
	GlobalMatrices->projMat = glm::ortho(-RProjZ, +RProjZ, -Aspect*RProjZ, +Aspect*RProjZ, -zFar, zFar);

	// Set viewport and projection.
	glViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);
	CHECK_GL_ERROR();
	bIsOrtho = true;

	GlobalMatrices->modelviewprojMat = GlobalMatrices->projMat * GlobalMatrices->viewMat * GlobalMatrices->modelMat; //yes, this is right.
	GlobalMatrices->modelviewMat = GlobalMatrices->viewMat * GlobalMatrices->modelMat;

	GlobalMatricesBuffer.Bind();
	GlobalMatricesBuffer.BufferData(false, false, GL_STATIC_DRAW);
	CHECK_GL_ERROR();

	UpdateCoords(Frame);
	unguard;
}

void UXOpenGLRenderDevice::SetProjection(FSceneNode* Frame, UBOOL bNearZ)
{
	guard(UXOpenGLRenderDevice::SetProjection);

	// Precompute stuff.
#if UNREAL_TOURNAMENT_OLDUNREAL
	FLOAT zNear = 0.5f;
#else
    FLOAT zNear = 1.0f;
#endif
    StoredbNearZ = 0;

	if (bNearZ)
    {
#if UNREAL_TOURNAMENT_OLDUNREAL
		zNear = 0.5f;
#else
		zNear = 0.7f;
#endif
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

	const auto GlobalMatrices = GlobalMatricesBuffer.GetElementPtr(0);
	GlobalMatrices->projMat = glm::frustum(-RProjZ*zNear, +RProjZ*zNear, -Aspect*RProjZ*zNear, +Aspect*RProjZ*zNear, zNear, zFar);

	// Set viewport and projection.
	glViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);
	CHECK_GL_ERROR();

	GlobalMatrices->modelviewprojMat = GlobalMatrices->projMat * GlobalMatrices->viewMat * GlobalMatrices->modelMat;
	GlobalMatrices->modelviewMat = GlobalMatrices->viewMat * GlobalMatrices->modelMat;

	GlobalMatricesBuffer.Bind();
	GlobalMatricesBuffer.BufferData(false, false, GL_STATIC_DRAW);
	CHECK_GL_ERROR();

	UpdateCoords(Frame);
	unguard;
}

BYTE UXOpenGLRenderDevice::PushClipPlane(const FPlane& Plane)
{
	guard(UXOpenGLRenderDevice::PushClipPlane);
	if (NumClipPlanes == MaxClippingPlanes)
		return 2;

	glEnable(GL_CLIP_DISTANCE0 + NumClipPlanes);

	const auto ClipPlaneInfo = GlobalClipPlaneBuffer.GetElementPtr(0);
	ClipPlaneInfo->ClipParams = glm::vec4(NumClipPlanes, 1.f, 0.f, 0.f);
	ClipPlaneInfo->ClipPlane = glm::vec4(Plane.X, Plane.Y, Plane.Z, Plane.W);

	GlobalClipPlaneBuffer.Bind();
	GlobalClipPlaneBuffer.BufferData(false, false, GL_DYNAMIC_DRAW);
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

	const auto ClipPlaneInfo = GlobalClipPlaneBuffer.GetElementPtr(0);
	ClipPlaneInfo->ClipParams = glm::vec4(NumClipPlanes, 0.f, 0.f, 0.f);
	ClipPlaneInfo->ClipPlane = glm::vec4(0.f, 0.f, 0.f, 0.f);

	GlobalClipPlaneBuffer.Bind();
	GlobalClipPlaneBuffer.BufferData(false, false, GL_DYNAMIC_DRAW);
	CHECK_GL_ERROR();

	return 1;
	unguard;
}


#define FPS_DEBUG 0
#if FPS_DEBUG
unsigned long long XFrameCounter = 0;
DOUBLE LastFrameLog = 0.0;
#endif

static INT LockCount = 0;
void UXOpenGLRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UXOpenGLRenderDevice::Lock);

#if FPS_DEBUG
	struct timeval TimeOfDay;
	gettimeofday(&TimeOfDay, NULL);
	DOUBLE NewTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;
	if (NewTime - LastFrameLog > 1.0)
	{
		debugf(TEXT("%d FPS"), XFrameCounter);
		XFrameCounter = 0;
		LastFrameLog = NewTime;
	}
	XFrameCounter++;
#endif

	check(LockCount == 0);
	++LockCount;
	CLEAR_GL_ERROR();

	MakeCurrent();
	CHECK_GL_ERROR();

	// Clear the Z buffer if needed.
	glClearColor(ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);
	CHECK_GL_ERROR();

	if (glClearDepthf)
		glClearDepthf(1.f);
#ifndef __EMSCRIPTEN__
	else
		glClearDepth(1.0);
#endif
	if (glDepthRangef)
		glDepthRangef(0.f, 1.f);
#ifndef __EMSCRIPTEN__
	else
		glDepthRange(0.0, 1.0);
#endif

	CHECK_GL_ERROR();
	glPolygonOffset(-1.f, -1.f);
    SetBlend(PF_Occlude, false);
#if MACOSX // stijn: on macOS, it's much faster to just clear everything
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
#else
	glClear(GL_DEPTH_BUFFER_BIT | ((RenderLockFlags & LOCKR_ClearScreen) ? GL_COLOR_BUFFER_BIT : 0));
#endif
	CHECK_GL_ERROR();

	LastZMode = ZTEST_LessEqual;
	glDepthFunc(GL_LEQUAL);

	// Remember stuff.
	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	// Lock hits.
	HitData = InHitData;
	HitSize = InHitSize;

	// Reset stats.
	appMemzero(&Stats, sizeof(Stats));

	LockHit(InHitData, InHitSize);
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
	}
#else
	if (Blit)
	{
		SetProgram(No_Prog);
		verify(SwapBuffers(hDC));
	}
#endif

    // Check for optional frame rate limit
    // The implementation below is plain wrong in many ways, but been working ever since in UTGLR's.
    // I am not happy with this solution, but it will do the trick for now...
#if UTGLRFRAMELIMIT
	if (FrameRateLimit >= 20)
    {
		FTime curFrameTimestamp;

		float timeDiff = 0.f;
		float rcpFrameRateLimit = 0.f;

		curFrameTimestamp = appSeconds();
		timeDiff = curFrameTimestamp - prevFrameTimestamp;
		prevFrameTimestamp = curFrameTimestamp;

		rcpFrameRateLimit = 1.0f / FrameRateLimit;
		if (timeDiff < rcpFrameRateLimit)
        {
			float sleepTime;

			sleepTime = rcpFrameRateLimit - timeDiff;
			appSleep(sleepTime);

			prevFrameTimestamp = appSeconds();
		}
    }
#endif

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
	Gamma = GammaCorrection * (GIsEditor ? GammaMultiplierUED : GammaMultiplier);

	unguard;
}

FLOAT UXOpenGLRenderDevice::GetViewportGamma(UViewport* Viewport) const
{
	if (Viewport->IsOrtho())
		return 1.f;
	return Gamma;
}

void UXOpenGLRenderDevice::SetProgram(INT NextProgram)
{
	guard(UXOpenGLRenderDevice::SetProgram);

	if (ActiveProgram != NextProgram)
	{
		CHECK_GL_ERROR();
		
		// Flush the old program
		Shaders[ActiveProgram]->DeactivateShader();

		CHECK_GL_ERROR();

		// Switch and initialize the new program
		PrevProgram = ActiveProgram;
		ActiveProgram = NextProgram;

		Shaders[ActiveProgram]->ActivateShader();

		CHECK_GL_ERROR();
	}

	unguard;
}

void UXOpenGLRenderDevice::ResetFog()
{
	guard(UOpenGLRenderDevice::ResetFog);
		
	//Reset Fog
	DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	DistanceFogValues = glm::vec4(0.f, 0.f, 0.f, -1.f);
	DistanceFogMode = -1;

	bFogEnabled = false;

	unguard;
}

void UXOpenGLRenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UXOpenGLRenderDevice::ClearZ);

	// stijn: force a flush before we clear the depth buffer
	const auto CurrentProgram = ActiveProgram;
	SetProgram(No_Prog);
	SetProgram(CurrentProgram);

	// stijn: you have a serious problem in Engine/Render if you need this SetSceneNode call here!
#if !__APPLE__
	SetSceneNode(Frame);
#endif
	SetBlend(PF_Occlude, false);
	glClear(GL_DEPTH_BUFFER_BIT);

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
	SetTexture(0, Info, PolyFlags, 0.0, DF_DiffuseTexture);
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

	if (!GIsEditor && !GIsRequestingExit)
		Flush(0);

#ifdef SDL2BUILD
	ResetShaders();
	if (SharedBindMap)
	{
		SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
		delete SharedBindMap;
		SharedBindMap = NULL;
	}
	CurrentGLContext = NULL;

# if !UNREAL_TOURNAMENT_OLDUNREAL
	SDL_GL_DeleteContext(glContext);
# endif

	AllContexts.RemoveItem(glContext);
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
	
	ResetShaders();
	if (AllContexts.Num() == 0 && SharedBindMap)
	{
		SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
		delete SharedBindMap;
		SharedBindMap = NULL;
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
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseShaderDrawParameters"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseShaderDrawParameters)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UsePersistentBuffers"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UsePersistentBuffers)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GenerateMipMaps"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(GenerateMipMaps)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseBufferInvalidation"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseBufferInvalidation)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoAATiles"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoAATiles)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("DetailTextures"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(DetailTextures)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("MacroTextures"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(MacroTextures)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("BumpMaps"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(BumpMaps)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("ParallaxVersion"), *FString::Printf(TEXT("%ls"), ParallaxVersion == Parallax_Basic ? TEXT("Basic") : ParallaxVersion == Parallax_Occlusion ? TEXT("Occlusion") : ParallaxVersion == Parallax_Relief ? TEXT("Relief") : TEXT("None")));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GammaCorrectScreenshots"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(GammaCorrectScreenshots)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseAA"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseAA)));
	//GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseAASmoothing"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseAASmoothing)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseTrilinear"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseTrilinear)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UsePrecache"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UsePrecache)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("AlwaysMipmap"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(AlwaysMipmap)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("ShareLists"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(ShareLists)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NoFiltering"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(NoFiltering)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("HighDetailActors"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(HighDetailActors)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("Coronas"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(Coronas)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("ShinySurfaces"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(ShinySurfaces)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("VolumetricLighting"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(VolumetricLighting)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("SimulateMultiPass"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(SimulateMultiPass)));

	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("MaxAnisotropy"), *FString::Printf(TEXT("%f"), MaxAnisotropy));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("LODBias"), *FString::Printf(TEXT("%f"), LODBias));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GammaOffsetScreenshots"), *FString::Printf(TEXT("%f"), GammaOffsetScreenshots));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("DebugLevel"), *FString::Printf(TEXT("%i"), DebugLevel));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("NumAASamples"), *FString::Printf(TEXT("%i"), NumAASamples));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("RefreshRate"), *FString::Printf(TEXT("%i"), RefreshRate));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("DescFlags"), *FString::Printf(TEXT("%i"), DescFlags));

	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseVSync"), *FString::Printf(TEXT("%ls"), UseVSync == VS_Off ? TEXT("Off") : UseVSync == VS_On ? TEXT("On") : TEXT("Adaptive")));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("OpenGLVersion"), *FString::Printf(TEXT("%ls"), OpenGLVersion == GL_Core ? TEXT("Core") : TEXT("ES")));

	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GammaMultiplier"), *FString::Printf(TEXT("%f"), GammaMultiplier));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("GammaMultiplierUED"), *FString::Printf(TEXT("%f"), GammaMultiplierUED));

	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("OneXBlending"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(OneXBlending)));
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("ActorXBlending"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(ActorXBlending)));

#if UNREAL_TOURNAMENT_OLDUNREAL && !defined(__LINUX_ARM__)
	GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("UseLightmapAtlas"), *FString::Printf(TEXT("%ls"), *GetTrueFalse(UseLightmapAtlas)));
#endif
	unguard;
}

void UXOpenGLRenderDevice::ShutdownAfterError()
{
	guard(UXOpenGLRenderDevice::ShutdownAfterError);
	debugf(NAME_Exit, TEXT("XOpenGL: ShutdownAfterError"));

#if XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP
	Flush(0);
	ResetShaders();
 	if (SharedBindMap)
	{
		SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
		delete SharedBindMap;
		SharedBindMap = NULL;
	}
#endif

#ifdef SDL2BUILD
	CurrentGLContext = NULL;
# if !UNREAL_TOURNAMENT_OLDUNREAL
	SDL_GL_DeleteContext(glContext);
# endif
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

void UXOpenGLRenderDevice::GetStats(TCHAR* Result)
{
	guard(UXOpenGLRenderDevice::GetStats);
	const double msPerCycle = GSecondsPerCycle * 1000.0f;
	FString StatsString = *FString::Printf(TEXT("XOpenGL stats:\nBind=%04.1f\nImage=%04.1f\nComplex=%04.1f\nGouraud=%04.1f\nTile Buffer/Draw=%04.1f/%04.1f\nDraw2DLine=%04.1f\nDraw3DLine=%04.1f\nDraw2DPoint=%04.1f\nPersistent buffer stalls: %i\n"),
		msPerCycle * Stats.BindCycles,
		msPerCycle * Stats.ImageCycles,
		msPerCycle * Stats.ComplexCycles,
		msPerCycle * Stats.GouraudPolyCycles,
		msPerCycle * Stats.TileBufferCycles,
		msPerCycle * Stats.TileDrawCycles,
		msPerCycle * Stats.Draw2DLine,
		msPerCycle * Stats.Draw3DLine,
		msPerCycle * Stats.Draw2DPoint,
		Stats.StallCount
	);

#if ENGINE_VERSION==227
    StatsString += *FString::Printf(TEXT("NumStaticLights %i\n"),NumStaticLights);
#endif

#ifndef __LINUX_ARM__
	if (SupportsNVIDIAMemoryInfo)
	{
		GLint CurrentAvailableVideoMemory = 0;
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &CurrentAvailableVideoMemory);
		GLint TotalAvailableVideoMemory = 0;
		glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &TotalAvailableVideoMemory);
		FLOAT Percent = (FLOAT)CurrentAvailableVideoMemory / (FLOAT)TotalAvailableVideoMemory * 100.0f;
		StatsString += *FString::Printf(TEXT("\nNVidia VRAM=%d MB\nUsed=%d MB\nUsage: %f%%"), TotalAvailableVideoMemory / 1024, (TotalAvailableVideoMemory - CurrentAvailableVideoMemory) / 1024, 100.0f - Percent);
		glGetError();
	}
	if (SupportsAMDMemoryInfo)
	{
		GLint CurrentAvailableTextureMemory = 0;
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &CurrentAvailableTextureMemory);
		GLint CurrentAvailableVBOMemory = 0;
		glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, &CurrentAvailableVBOMemory);
		GLint CurrentAvailableRenderbufferMemory = 0;
		glGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, &CurrentAvailableRenderbufferMemory);
		StatsString += *FString::Printf(TEXT("\nAMD CurrentAvailableTextureMemory=%d MB\nCurrentAvailableVBOMemory=%d MB\nCurrentAvailableRenderbufferMemory=%d MB"), CurrentAvailableTextureMemory / 1024, CurrentAvailableVBOMemory / 1024, CurrentAvailableRenderbufferMemory / 1024);
		glGetError();
	}
#endif
	appSprintf(Result,*StatsString);
	CHECK_GL_ERROR();
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
	if (SupportsNVIDIAMemoryInfo)
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
	if (SupportsAMDMemoryInfo)
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
	unguard;
}

// Static variables.
#ifdef SDL2BUILD
SDL_GLContext		UXOpenGLRenderDevice::CurrentGLContext = NULL;
TArray<SDL_GLContext> UXOpenGLRenderDevice::AllContexts;
#else
HGLRC				UXOpenGLRenderDevice::CurrentGLContext = NULL;
TArray<HGLRC>		UXOpenGLRenderDevice::AllContexts;
PFNWGLCHOOSEPIXELFORMATARBPROC UXOpenGLRenderDevice::wglChoosePixelFormatARB = nullptr;
PFNWGLCREATECONTEXTATTRIBSARBPROC UXOpenGLRenderDevice::wglCreateContextAttribsARB = nullptr;
#endif
INT					UXOpenGLRenderDevice::LogLevel = 0;
DWORD				UXOpenGLRenderDevice::ComposeSize = 0;
BYTE*				UXOpenGLRenderDevice::Compose = NULL;

TOpenGLMap<QWORD, UXOpenGLRenderDevice::FCachedTexture> *UXOpenGLRenderDevice::SharedBindMap;

void autoInitializeRegistrantsXOpenGLDrv(void)
{
	UXOpenGLRenderDevice::StaticClass();
}

/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/

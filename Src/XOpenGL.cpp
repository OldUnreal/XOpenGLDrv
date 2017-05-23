/*=============================================================================
	XOpenGL.cpp: Unreal XOpenGL implementation for OpenGL 3.3+ and GL ES 3.0+

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch
		* lots of experience and ideas from UTGLR OpenGL by Chris Dohnal
		* improved texture handling code by Sebastian Kaufel
		* information, ideas, additions by Sebastian Kaufel
		* UED selection code by Sebastian Kaufel
		* TOpenGLMap tempate based on TMap template using a far superior hash function by Sebastian Kaufel
    Todo:
		* fixes, cleanups, (AZDO) optimizations, etc, etc, etc.
		* fix up and implement proper usage of persistent buffers.
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
	UEnum* VSyncs = new( GetClass(), TEXT("VSyncs"))UEnum(NULL);

	// Enum members.
	new( VSyncs->Names )FName( TEXT("Adaptive" ) );
	new( VSyncs->Names )FName( TEXT("Off")       );
	new( VSyncs->Names )FName( TEXT("On" )       );

	UEnum* OpenGLVersions = new(GetClass(), TEXT("OpenGLVersions"))UEnum(NULL);
	new(OpenGLVersions->Names)FName(TEXT("Core"));
	new(OpenGLVersions->Names)FName(TEXT("ES"));

	new(GetClass(),TEXT("OpenGLVersion"),			RF_Public)UByteProperty ( CPP_PROPERTY(OpenGLVersion			), TEXT("Options"), CPF_Config, OpenGLVersions);
	new(GetClass(),TEXT("VSync"),                   RF_Public)UByteProperty ( CPP_PROPERTY(VSync                    ), TEXT("Options"), CPF_Config, VSyncs);
	new(GetClass(),TEXT("RefreshRate"),				RF_Public)UIntProperty	( CPP_PROPERTY(RefreshRate				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("DebugLevel"),				RF_Public)UIntProperty	( CPP_PROPERTY(DebugLevel				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("NumAASamples"),			RF_Public)UIntProperty	( CPP_PROPERTY(NumAASamples				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("GammaOffsetScreenshots"),	RF_Public)UFloatProperty( CPP_PROPERTY(GammaOffsetScreenshots	), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("LODBias"),					RF_Public)UFloatProperty( CPP_PROPERTY(LODBias					), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("MaxAnisotropy"),			RF_Public)UFloatProperty( CPP_PROPERTY(MaxAnisotropy			), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("NoFiltering"),				RF_Public)UBoolProperty	( CPP_PROPERTY(NoFiltering				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("ShareLists"),				RF_Public)UBoolProperty ( CPP_PROPERTY(ShareLists				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("AlwaysMipmap"),			RF_Public)UBoolProperty ( CPP_PROPERTY(AlwaysMipmap				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("UsePrecache"),				RF_Public)UBoolProperty ( CPP_PROPERTY(UsePrecache				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("UseTrilinear"),			RF_Public)UBoolProperty	( CPP_PROPERTY(UseTrilinear				), TEXT("Options"), CPF_Config);
	//new(GetClass(),TEXT("UseSRGBTextures"),			RF_Public)UBoolProperty	( CPP_PROPERTY(UseSRGBTextures			), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("UseOpenGLDebug"),			RF_Public)UBoolProperty	( CPP_PROPERTY(UseOpenGLDebug			), TEXT("Options"), CPF_Config);
    new(GetClass(),TEXT("UseAA"),					RF_Public)UBoolProperty	( CPP_PROPERTY(UseAA					), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("GammaCorrectScreenshots"),	RF_Public)UBoolProperty	( CPP_PROPERTY(GammaCorrectScreenshots	), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("MacroTextures"),			RF_Public)UBoolProperty	( CPP_PROPERTY(MacroTextures			), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("BumpMaps"),				RF_Public)UBoolProperty	( CPP_PROPERTY(BumpMaps					), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("NoBuffering"),				RF_Public)UBoolProperty	( CPP_PROPERTY(NoBuffering				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("NoAATiles"),				RF_Public)UBoolProperty	( CPP_PROPERTY(NoAATiles				), TEXT("Options"), CPF_Config);
	new(GetClass(),TEXT("UseBufferInvalidation"),	RF_Public)UBoolProperty	( CPP_PROPERTY(UseBufferInvalidation	), TEXT("Options"), CPF_Config);
	new(GetClass(), TEXT("GenerateMipMaps"),		RF_Public)UBoolProperty ( CPP_PROPERTY(GenerateMipMaps			), TEXT("Options"), CPF_Config);
	//new(GetClass(),TEXT("UsePersistentBuffers"),	RF_Public)UBoolProperty	( CPP_PROPERTY(UsePersistentBuffers		), TEXT("Options"), CPF_Config);
#if ENGINE_VERSION==227
	new(GetClass(),TEXT("UseHWClipping"),			RF_Public)UBoolProperty ( CPP_PROPERTY(UseHWClipping			), TEXT("Options"), CPF_Config);
	//new(GetClass(),TEXT("UseHWLighting"),			RF_Public)UBoolProperty ( CPP_PROPERTY(UseHWLighting			), TEXT("Options"), CPF_Config);
	//new(GetClass(),TEXT("UseMeshBuffering"),		RF_Public)UBoolProperty	( CPP_PROPERTY(UseMeshBuffering			), TEXT("Options"), CPF_Config);
#endif

	//Defaults
	GammaCorrectScreenshots  = 1;
	VSync                    = VS_Adaptive;

	#ifdef __EMSCRIPTEN__
	OpenGLVersion			 = GL_ES;
	#else
	OpenGLVersion			 = GL_Core;
	#endif

	unguard;
}

UBOOL UXOpenGLRenderDevice::Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
{
	guard(UXOpenGLRenderDevice::Init);
	//it is really a bad habit of the UEngine1 games to call init again each time a fullscreen change is needed.

	#if _WIN32 && ENGINE_VERSION!=227
		TimerBegin();
	#endif

	#define GL_EXT(ext) SUPPORTS_##ext = 0;
	#ifndef __EMSCRIPTEN__
	#define GL_PROC(ext,fntype,fnname) fnname = NULL;
	#endif
	#include "XOpenGLFuncs.h"

	// Driver flags.
	FullscreenOnly			= 0;
	SpanBased				= 0;
	SupportsTC				= 1;
	SupportsFogMaps			= 1;
	SupportsDistanceFog		= 1;
	SupportsLazyTextures	= 0;
	PrefersDeferredLoad		= 0;

#if ENGINE_VERSION==227
	// new in 227j. This will tell render to use the abilities a new rendev provides instead of using internal (CPU intense) functions.
	//In use so far for DetailTextures on meshes.
	LegacyRenderingOnly		= 0;

	// Make use of DrawGouraudPolyList for Meshes and leave the clipping up for the rendev. Instead of pushing vert by vert this uses a huge list.
	SupportsNoClipRender	= UseHWClipping;
#endif
	UseMeshBuffering		 = 0;
	UseHWLighting			 = 0;
	UsePersistentBuffers	 = 0;

	EnvironmentMaps = 0; //not yet implemented.

	// These arrays are either fixed values or estimated based on logging of values on a couple of maps.
	// Static set to avoid using new/delete in every render pass. Should be more than sufficient.

	Draw2DLineVertsBuf = new FLOAT[18];
	Draw2DPointVertsBuf = new FLOAT[36];
	Draw3DLineVertsBuf = new FLOAT[18];
	EndFlashVertsBuf = new FLOAT[36];
	DrawLinesVertsBuf = new FLOAT[DRAWSIMPLE_SIZE]; // unexpectedly this size is easy to reach in UED.

	//DrawTile
	DrawTileVertsBuf = new FLOAT[DRAWTILE_SIZE]; // unexpectedly this size is easy to reach in UED.

	//DrawGouraud
	DrawGouraudBuf = new FLOAT[DRAWGOURAUDPOLYLIST_SIZE];
#if ENGINE_VERSION==227
	DrawGouraudPolyListVertsBuf = new FLOAT[16384];
	DrawGouraudPolyListVertNormalsBuf = new FLOAT[16384];
	DrawGouraudPolyListTexBuf = new FLOAT[16384];
	DrawGouraudPolyListLightColorBuf = new FLOAT[32768];
	DrawGouraudPolyListSingleBuf = new FLOAT[DRAWGOURAUDPOLYLIST_SIZE];
#endif
	DrawGouraudIndex = 0;

	// DrawComplex
	DrawComplexVertsSingleBuf = new FLOAT[DRAWCOMPLEX_SIZE];

	DrawGouraudCounter=0;
	DrawGouraudBufferElements=0;
	DrawGouraudBufferTexture = NULL;
	DrawGouraudTotalSize = 0;
	BufferCount = 0;

	if (ShareLists && !SharedBindMap)
		SharedBindMap = new TOpenGLMap<QWORD,UXOpenGLRenderDevice::FCachedTexture>;

	BindMap = ShareLists ? SharedBindMap : &LocalBindMap;
	Viewport = InViewport;

	ActiveProgram		= -1;

#ifdef SDL2BUILD
    SDL_Window* Window = (SDL_Window*) Viewport->GetWindow();
#elif QTBUILD

#else
    INT i=0;
	// Get list of device modes.
	for (i = 0; ; i++)
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
	hDC = GetDC( hWnd );

#endif

	// Set resolution
	if( !SetRes( NewX, NewY, NewColorBytes, Fullscreen ) )
	{
		GWarn->Logf(TEXT("XOpenGL: SetRes failed!") );
		return 0;
	}

	LogLevel=DebugLevel;

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
	projMat	= glm::mat4(1.0f);

	// Identity
	modelMat = glm::mat4(1.0f);

#ifndef __EMSCRIPTEN__
	if (OpenGLVersion == GL_Core)
	{
		if (!GL_ARB_buffer_storage && UsePersistentBuffers)
		{
			debugf(TEXT("XOpenGL: GL_ARB_buffer_storage not found. Disabling UsePersistentBuffers."));
			UsePersistentBuffers = 0;
		}
		if (!GL_ARB_invalidate_subdata && UseBufferInvalidation)
		{
			debugf(TEXT("XOpenGL: GL_ARB_invalidate_subdata not found. Disabling UseBufferInvalidation."));
			UseBufferInvalidation = 0;
		}
	}
	else
#endif
	{
		UsePersistentBuffers = 0;
		UseBufferInvalidation = 0;
	}

	if(NumAASamples < 2 && UseAA)
		NumAASamples = 2;

	if (GenerateMipMaps && !UsePrecache)
	{
		debugf(TEXT("XOpenGL: Enabling UsePrecache for GenerateMipMaps."));
	}

	// Init shaders
	InitShaders();
	CHECK_GL_ERROR();

	// Map persistent buffers
	if (UsePersistentBuffers)
		MapBuffers();

	return 1;
	unguard;
}

void UXOpenGLRenderDevice::PostEditChange()
{
	guard(UXOpenGLRenderDevice::PostEditChange);
	Flush( 0 );
	unguard;
}

#ifdef WINBUILD
LRESULT CALLBACK WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uiMsg)
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

void UXOpenGLRenderDevice::CreateOpenGLContext(UViewport* Viewport, INT NewColorBytes)
{
	guard(UXOpenGLRenderDevice::CreateOpenGLContext);
	debugf(TEXT("XOpenGL: Creating new OpenGL context."));

	INT DesiredColorBits   = NewColorBytes<=2 ? 16 : 32;
	INT DesiredStencilBits = NewColorBytes<=2 ? 0  : 8;
	INT DesiredDepthBits   = NewColorBytes<=2 ? 16 : 24;

	INT MajorVersion = 3;
	INT MinorVersion = 3;

	if (OpenGLVersion == GL_ES)
	{
		MajorVersion = 2;
		MinorVersion = 0;
	}

#ifdef SDL2BUILD
    // Init global GL.
	// no need to do SDL_Init here, done in SDL2Drv (USDL2Viewport::ResizeViewport).
	SDL_Window* Window = (SDL_Window*) Viewport->GetWindow();

	if (!Window)
        appErrorf(TEXT("XOpenGL: No SDL Window found!"));

	//Request OpenGL 3.3 context.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, MajorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, MinorVersion);

	if (OpenGLVersion == GL_ES)
		SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else
		SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	if (!glContext)
		glContext = SDL_GL_GetCurrentContext();

	if (!glContext)
		glContext = SDL_GL_CreateContext(Window);

	if (glContext == NULL)
		appErrorf(TEXT("XOpenGL: Error failed creating SDL_GL_CreateContext: %s"),appFromAnsi(SDL_GetError()));

	MakeCurrent();

	UBOOL SUPPORTS_GL = 1;
	UBOOL SUPPORTS_GLCORE = 1;
	UBOOL SUPPORTS_GLES = 1;
	#define GL_EXT(name) SUPPORTS_##name = SDL_GL_ExtensionSupported(#name) ? 1: 0;
	#ifndef __EMSCRIPTEN__
		#define GL_PROC(ext,fntype,fnname) \
			if (SUPPORTS_##ext) { \
				if ((fnname = (fntype) SDL_GL_GetProcAddress(#fnname)) == NULL) { \
					debugf(NAME_Init, TEXT("Missing OpenGL entry point '%s' for '%s'"), #fnname, #ext); \
					SUPPORTS_##ext = 0; \
				} \
			}
	#endif
	#include "XOpenGLFuncs.h"

#ifndef __EMSCRIPTEN__
	if ((!glDepthRangef) && (!glDepthRange))
	{
		SUPPORTS_GL = 0;
	}

	if ((!glClearDepthf) && (!glClearDepth))
	{
		SUPPORTS_GL = 0;
	}
#endif

	if (!SUPPORTS_GL)
	{
		appErrorf(TEXT("Missing needed core OpenGL functionality."));
	}

	debugf(NAME_Init, TEXT("GL_VENDOR     : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_VENDOR)));
	debugf(NAME_Init, TEXT("GL_RENDERER   : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER)));
	debugf(NAME_Init, TEXT("GL_VERSION    : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_VERSION)));
	debugf(NAME_Init, TEXT("GL_SHADING_LANGUAGE_VERSION    : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_SHADING_LANGUAGE_VERSION)));

	int NumberOfExtensions=0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
	for (INT i = 0; i<NumberOfExtensions; i++)
	{
		FString ExtensionString = appFromAnsi((const ANSICHAR *)glGetStringi(GL_EXTENSIONS, i));
		debugf(NAME_DevLoad, TEXT("GL_EXTENSIONS(%i) : %s"), i, *ExtensionString);
	}
	if (OpenGLVersion != GL_Core)
		debugf(NAME_Init, TEXT("OpenGL ES %i.%i context initialized!"), MajorVersion,MinorVersion);
	else
		debugf(NAME_Init, TEXT("OpenGL %i.%i core context initialized!"), MajorVersion,MinorVersion);

#if 0
	if (UseOpenGLDebug)
	{
	    glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(&UXOpenGLRenderDevice::DebugCallback, NULL);

		GWarn->Logf(TEXT("OpenGL debugging enabled, this can cause severe performance drain!"));
	}
#endif
#elif QTBUILD

#else
	GLenum err;
	if (NeedGlewInit)
	{
		PIXELFORMATDESCRIPTOR temppfd =
		{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,
			DesiredColorBits,
			0, 0, 0, 0, 0, 0,
			0, 0,
			0, 0, 0, 0, 0,
			DesiredDepthBits,
			0,//DesiredStencilBits,
			0,
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};
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

		if(RegisterClassEx(&WndClassEx) == 0)
		{
			debugf(TEXT("XOpenGL: RegisterClassEx failed!"));
		}
		HWND TemphWnd = CreateWindowEx(WS_EX_APPWINDOW, WndClassEx.lpszClassName,  L"InitWIndow", Style, 0, 0, Viewport->SizeX, Viewport->SizeY, NULL, NULL, hInstance, NULL);
		HDC TemphDC = GetDC( TemphWnd );
		INT nPixelFormat = ChoosePixelFormat( TemphDC, &temppfd );
		check(nPixelFormat);
		verify(SetPixelFormat( TemphDC, nPixelFormat, &temppfd ));

		// oldstyle context to init glew.
		HGLRC tempContext = wglCreateContext(TemphDC);
		wglMakeCurrent(TemphDC, tempContext);

		//init glew
		glewExperimental = GL_TRUE;
		err = glewInit();
		if (GLEW_OK != err)
			appErrorf(TEXT("XOpenGL: Init glew failed: %s"), appFromAnsi((char*)glewGetErrorString(err)));
		else debugf(NAME_Init, TEXT("XOpenGL: Glew successfully initialized."));
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(tempContext);
		ReleaseDC(TemphWnd, TemphDC);
		DestroyWindow(TemphWnd);
		NeedGlewInit = false;
	}
	//Now init pure OpenGL >= 3.3 context.
	if (WGLEW_ARB_create_context && WGLEW_ARB_pixel_format)
	{
		PIXELFORMATDESCRIPTOR pfd;
		memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = DesiredColorBits;
		pfd.cDepthBits = DesiredDepthBits;
		pfd.iLayerType = PFD_MAIN_PLANE;

		INT iPixelFormat;
		if (UseAA && !GIsEditor)
		{
			while(NumAASamples > 0)
			{
				UINT NumFormats = 0;
				INT iPixelFormatAttribList[]=
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

				if(wglChoosePixelFormatARB(hDC, iPixelFormatAttribList, NULL, 1, &iPixelFormat, &NumFormats) == TRUE && NumFormats > 0)
					break;
				NumAASamples--;
			}
		}
		else
		{
			UINT NumFormats = 0;
			INT iPixelFormatAttribList[]=
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


		INT ContextFlags=0;
		if (UseOpenGLDebug)
			 ContextFlags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB;
		else ContextFlags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

		INT iContextAttribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, MajorVersion,
			WGL_CONTEXT_MINOR_VERSION_ARB, MinorVersion,
			WGL_CONTEXT_FLAGS_ARB, ContextFlags,
			0 // End of attributes list
		};
		debugf(NAME_Init, TEXT("XOpenGL: DesiredColorBits: %i"), DesiredColorBits);
		debugf(NAME_Init, TEXT("XOpenGL: DesiredDepthBits: %i"), DesiredDepthBits);
		if (!SetPixelFormat(hDC, iPixelFormat, &pfd))
		{
			debugf(NAME_Init, TEXT("XOpenGL: Setting PixelFormat %i failed!"), iPixelFormat);
			iPixelFormat = ChoosePixelFormat(hDC, &pfd);
			if (!SetPixelFormat(hDC, iPixelFormat, &pfd))
				GWarn->Logf(TEXT("XOpenGL: SetPixelFormat %i failed. Restart may required."),iPixelFormat);
			//return;
		}
		else debugf(NAME_Init, TEXT("XOpenGL: Setting PixelFormat: %i"), iPixelFormat);
		hRC = wglCreateContextAttribsARB(hDC, 0, iContextAttribs);
	}
	else  appErrorf(TEXT("XOpenGL: Failed to init pure OpenGL >= 3.3 context with: %s"), appFromAnsi((char*)glewGetErrorString(err)));

	if(hRC)
	{
		MakeCurrent();
		DescFlags = 0;
		Description = appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER));
		GConfig->SetString(TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("Description"), *FString::Printf(TEXT("%s"), Description)); //Why isn't this set automatically??
		debugf(NAME_Init, TEXT("GL_VENDOR     : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_VENDOR)));
		debugf(NAME_Init, TEXT("GL_RENDERER   : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER)));
		debugf(NAME_Init, TEXT("GL_VERSION    : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_VERSION)));
		debugf(NAME_Init, TEXT("GL_SHADING_LANGUAGE_VERSION    : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_SHADING_LANGUAGE_VERSION)));
		debugf(NAME_Init, TEXT("GLEW Version  : %s"), appFromAnsi((const ANSICHAR *)glewGetString(GLEW_VERSION)));

		int NumberOfExtensions=0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
		for (INT i = 0; i<NumberOfExtensions; i++)
		{
			FString ExtensionString = appFromAnsi((const ANSICHAR *)glGetStringi(GL_EXTENSIONS, i));
			debugf(NAME_DevLoad, TEXT("GL_EXTENSIONS(%i) : %s"), i, ExtensionString);
		}
		if (OpenGLVersion == GL_Core)
			debugf(NAME_Init, TEXT("OpenGL %i.%i context initialized!"), MajorVersion,MinorVersion);
		else
			debugf(NAME_Init, TEXT("OpenGL ES %i.%i context initialized!"), MajorVersion, MinorVersion);
	}
	else
	{
		if (OpenGLVersion == GL_Core)
			appErrorf(TEXT("XOpenGL: No OpenGL %i.%i context support."), MajorVersion, MinorVersion);
		else
			appErrorf(TEXT("XOpenGL: No OpenGL ES %i.%i context support."), MajorVersion, MinorVersion);
	}


	if( ShareLists && AllContexts.Num() )
		verify(wglShareLists(AllContexts(0),hRC)==1);
	AllContexts.AddItem(hRC);
	NumContexts++;

	if (UseOpenGLDebug)
	{
		glDebugMessageCallbackARB(&UXOpenGLRenderDevice::DebugCallback, NULL);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		GWarn->Logf(TEXT("XOpenGL: OpenGL debugging enabled, this can cause severe performance drain!"));
	}
	else
		debugf(TEXT("XOpenGL: OpenGL debugging disabled."));
#endif
	unguard;
}

void UXOpenGLRenderDevice::MakeCurrent(void) {
	guard(UOpenGLRenderDevice::MakeCurrent);
#ifdef SDL2BUILD
    if (CurrentGLContext != glContext)
	{
		debugf(TEXT("XOpenGL: MakeCurrent"));
		SDL_Window* Window = (SDL_Window*) Viewport->GetWindow();
		INT Result=SDL_GL_MakeCurrent(Window, glContext);
		if (Result != 0)
            debugf( TEXT("XOpenGL: MakeCurrent failed with: %s\n"), appFromAnsi(SDL_GetError()) );
		else CurrentGLContext=glContext;
	}
#elif QTBUILD

#else
	check(hRC);
	check(hDC);
	if (CurrentGLContext != hRC)
	{
		debugf(NAME_Dev, TEXT("XOpenGL: MakeCurrent"));
		verify(wglMakeCurrent(hDC, hRC));
		CurrentGLContext = hRC;
	}
#endif
	unguard;
}

void UXOpenGLRenderDevice::SetPermanentState()
{
	// Set permanent state.
	glEnable( GL_DEPTH_TEST );
	CHECK_GL_ERROR();
	glDepthMask( GL_TRUE );
	CHECK_GL_ERROR();
	glPolygonOffset(-1.0f, -1.0f);
	CHECK_GL_ERROR();
	glBlendFunc( GL_ONE, GL_ZERO );
	CHECK_GL_ERROR();
	glEnable( GL_BLEND );
	CHECK_GL_ERROR();

	/*
	GLES 3 supports sRGB functionality, but it does not expose the
	GL_FRAMEBUFFER_SRGB enable/disable bit.  Instead the implementation
	is     */
	#ifndef __EMSCRIPTEN__
	if (OpenGLVersion == GL_Core)
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

UBOOL UXOpenGLRenderDevice::SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
{
	guard(UXOpenGLRenderDevice::SetRes);
	debugf(NAME_Dev, TEXT("XOpenGL: SetRes"));

	// If not fullscreen, and color bytes hasn't changed, do nothing.
	#ifdef SDL2BUILD
	if (glContext && CurrentGLContext && glContext == CurrentGLContext)
    #else
    if (hRC && CurrentGLContext && hRC == CurrentGLContext)
    #endif
	{
		if(!Fullscreen && !WasFullscreen && NewColorBytes==Viewport->ColorBytes )
		{
			// Change window size.
			if( !Viewport->ResizeViewport( BLIT_HardwarePaint|BLIT_OpenGL, NewX, NewY, NewColorBytes ) )
			{
				return 0;
			}
			glViewport( 0, 0, NewX, NewY );
			return 1;
		}
	}
	else
	{
		#ifdef SDL2BUILD
            debugf(TEXT("XOpenGL: SetRes requires ResizeViewport"));

			INT DesiredColorBits   = NewColorBytes<=2 ? 16 : 32;
			INT DesiredStencilBits = NewColorBytes<=2 ? 0  : 8;
			INT DesiredDepthBits   = NewColorBytes<=2 ? 16 : 24;

			INT MajorVersion = 3;
			INT MinorVersion = 3;

			if (OpenGLVersion == GL_ES)
			{
				MajorVersion = 2;
				MinorVersion = 0;
			}

			//Request OpenGL 3.3 context.
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, MajorVersion);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, MinorVersion);

			if (OpenGLVersion == GL_ES)
				SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			else
				SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

            if(!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes)) //need to create temporary SDL2 window for context creation.
                appErrorf(TEXT("XOpenGL: ResizeViewport failed!"));
            if (glContext && glContext != CurrentGLContext)
                MakeCurrent();
		#else
            // (Re)init OpenGL rendering context.
            if (hRC && hRC != CurrentGLContext)
                MakeCurrent();
        #endif
		else CreateOpenGLContext(Viewport, NewColorBytes);
		CHECK_GL_ERROR();

		// Set permanent state... (OpenGL Presets)
		SetPermanentState();
		CHECK_GL_ERROR();
	}

#ifdef WINBUILD
	// Change display settings.
	if (Fullscreen)
    {
		INT FindX = NewX, FindY = NewY, BestError = MAXINT;
		for (INT i = 0; i < Modes.Num(); i++)
        {
			if (Modes(i).Z==(NewColorBytes*8))
			{
				INT Error
				=	(Modes(i).X-FindX)*(Modes(i).X-FindX)
				+	(Modes(i).Y-FindY)*(Modes(i).Y-FindY);
				if (Error < BestError)
				{
					NewX      = Modes(i).X;
					NewY      = Modes(i).Y;
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
		dma.dmFields = dmw.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;// | DM_BITSPERPEL;

		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, NewX, NewY, SWP_SHOWWINDOW);

		debugf(TEXT("XOpenGL: Fullscreen NewX %i NewY %i"),NewX,NewY);

		if (RefreshRate)
        {
			dma.dmDisplayFrequency = dmw.dmDisplayFrequency = RefreshRate;
			dma.dmFields |= DM_DISPLAYFREQUENCY;
			dmw.dmFields |= DM_DISPLAYFREQUENCY;

			if (TCHAR_CALL_OS(ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN), ChangeDisplaySettingsA(&dma, CDS_FULLSCREEN)) != DISP_CHANGE_SUCCESSFUL)
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
            if (TCHAR_CALL_OS(ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN), ChangeDisplaySettingsA(&dma, CDS_FULLSCREEN)) != DISP_CHANGE_SUCCESSFUL)
			{
                debugf(TEXT("XOpenGL: ChangeDisplaySettings failed: %ix%i"), NewX, NewY);
				return 0;
			}
			debugf(TEXT("XOpenGL: ChangeDisplaySettings: %ix%i"), NewX, NewY);
        }
	}
	else UnsetRes();
#endif
	// Change window size. FIXME! Why double calling? SDL2 won't work with that.
	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
	if (!Result)
	{
		debugf(TEXT("XOpenGL: Change window size failed!"));
        #ifdef WINBUILD
		if (Fullscreen)
		{
			TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL, 0), ChangeDisplaySettingsA(NULL, 0));
		}
        #endif
		return 0;
	}

	// Verify hardware defaults.
	CurrentPolyFlags = PF_Occlude;

	// Set & Store Gamma
	SetGamma( Viewport->GetOuterUClient()->Brightness );

#ifdef _WIN32

	guard(SwapControl);
	if ( WGL_EXT_swap_control && wglSwapIntervalEXT )
	{

		switch ( VSync )
		{
			case VS_On:
				wglSwapIntervalEXT( 1 );
				break;
			case VS_Off:
				wglSwapIntervalEXT( 0 );
				break;

			case VS_Adaptive:
				if ( !WGL_EXT_swap_control_tear )
				{
					debugf( TEXT("XOpenGL: WGL_EXT_swap_control_tear is not supported by device. Falling back to SwapInterval 0.") );
					wglSwapIntervalEXT( 0 );
					break;
				}
				wglSwapIntervalEXT( -1 );
				break;
		}
	}
	else
		debugf( TEXT("XOpenGL: WGL_EXT_swap_control is not supported.") );
	unguard;
#endif // _WIN32

	INT MaxCombinedTextureImageUnits=0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &MaxCombinedTextureImageUnits);
	debugf(TEXT("XOpenGL: MaxCombinedTextureImageUnits: %i"), MaxCombinedTextureImageUnits);

	INT MaxElementsVertices=0;
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &MaxElementsVertices);
	debugf(TEXT("XOpenGL: MaxElementsVertices: %i"), MaxElementsVertices);

	#ifndef __EMSCRIPTEN__
	if (OpenGLVersion == GL_Core)
	{
		INT MaxDualSourceDrawBuffers;
		glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &MaxDualSourceDrawBuffers);
		debugf(TEXT("XOpenGL: MaxDualSourceDrawBuffers: %i"), MaxDualSourceDrawBuffers);
	}
	#endif

	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &MaxTextureSize );
	debugf(TEXT("XOpenGL: MaxTextureSize: %i"), MaxTextureSize);

	if( SUPPORTS_GL_WEBGL_compressed_texture_s3tc )
	{
		SUPPORTS_GL_EXT_texture_compression_s3tc = 1;
	}
	if( SUPPORTS_GL_ANGLE_texture_compression_dxt )
	{
		SUPPORTS_GL_EXT_texture_compression_s3tc = 1;
	}

	if (SUPPORTS_GL_EXT_texture_compression_s3tc)
	{
		SUPPORTS_GL_EXT_texture_compression_dxt1 = 1;
		SUPPORTS_GL_EXT_texture_compression_dxt3 = 1;
		SUPPORTS_GL_EXT_texture_compression_dxt5 = 1;
	}

	debugf(TEXT("XOpenGL: s3tc support: %sDXT1 %sDXT3 %sDXT5"),
		SUPPORTS_GL_EXT_texture_compression_dxt1 ? "" : "!",
		SUPPORTS_GL_EXT_texture_compression_dxt3 ? "" : "!",
		SUPPORTS_GL_EXT_texture_compression_dxt5 ? "" : "!"
	);

	if( !SUPPORTS_GL_EXT_texture_filter_anisotropic )
	{
		debugf(TEXT("XOpenGL: Anisotropic filter extension not found!"));
		MaxAnisotropy  = 0.f;
	}

	if( !SUPPORTS_GL_EXT_texture_lod_bias )
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
		GLfloat tmp;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &tmp);

        if (tmp <= 0.f)
            tmp = 0.f; // seems in Linux ARM with ODROID-XU4 the extension check fails. Better safe than sorry.

		debugf(TEXT("XOpenGL: MaxAnisotropy = (%f/%f)"), MaxAnisotropy,tmp);

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
		int NumberOfAASamples=0, MaxAASamples;
		glGetIntegerv(GL_MAX_SAMPLES, &MaxAASamples);
		glGetIntegerv(GL_SAMPLES,&NumberOfAASamples);
		debugf(TEXT("XOpenGL: NumAASamples: (%i/%i)"), NumberOfAASamples,MaxAASamples);

		if(NumAASamples>MaxAASamples)
			NumAASamples=MaxAASamples;
	}

	// Flush textures.
	Flush(1);

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
	if( WasFullscreen )
	{
		TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL,0),ChangeDisplaySettingsA(NULL,0));
	}
#elif SDL2BUILD

#else

#endif
	Flush(1);
	unguard;
}

void UXOpenGLRenderDevice::Flush( UBOOL AllowPrecache )
{
	guard(UXOpenGLRenderDevice::Flush);
	CHECK_GL_ERROR();

	if ( AllowPrecache && UsePrecache )
	{
		TArray<GLuint> Binds;
		for (TOpenGLMap<QWORD, FCachedTexture>::TIterator It(*BindMap); It; ++It)
		{
			glDeleteSamplers(1, &It.Value().Sampler);
			CHECK_GL_ERROR();
			for (INT i = 0; i < 2; i++)
				if (It.Value().Ids[i])
					Binds.AddItem(It.Value().Ids[i]);
		}
		BindMap->Empty();

		if (Binds.Num())
		{
			//debugf(TEXT("Binds.Num() %i"),Binds.Num());
			glDeleteTextures(Binds.Num(), (GLuint*)&Binds(0));
		}

		for (INT i = 0; i < 8; i++) // Also reset all multi textures.
			SetNoTexture(i);

		if (!GIsEditor)
			PrecacheOnFlip = 1;
	}

	if (Viewport && Viewport->GetOuterUClient())
		SetGamma( Viewport->GetOuterUClient()->Brightness );

	CurrentPolyFlags = 0;
	CurrentAdditionalPolyFlags = 0;
	ActiveProgram = -1;

	StoredOrthoFovAngle = 0;
	StoredOrthoFX = 0;
	StoredOrthoFY = 0;
	StoredFovAngle = 0;
	StoredFX = 0;
	StoredFY = 0;

	ResetFog();

	glFlush();
	glFinish();

	debugf(NAME_Dev,TEXT("XOpenGL: Flush"));

	CHECK_GL_ERROR();
	unguard;
}

UBOOL UXOpenGLRenderDevice::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UXOpenGLRenderDevice::Exec);
	if( URenderDevice::Exec( Cmd, Ar ) )
	{
		return 1;
	}
	if( ParseCommand(&Cmd,TEXT("GetRes")) )
	{
#ifdef WINBUILD
		TArray<FPlane> Relevant;
		INT i;
		for( i=0; i<Modes.Num(); i++ )
			if( Modes(i).Z==Viewport->ColorBytes*8 )
				if
				(	(Modes(i).X!=320 || Modes(i).Y!=200)
				&&	(Modes(i).X!=640 || Modes(i).Y!=400) )
				Relevant.AddUniqueItem(FPlane(Modes(i).X,Modes(i).Y,0,0));
		appQsort( &Relevant(0), Relevant.Num(), sizeof(FPlane), (QSORT_COMPARE)CompareRes );
		FString Str;
		for( i=0; i<Relevant.Num(); i++ )
			Str += FString::Printf( TEXT("%ix%i "), (INT)Relevant(i).X, (INT)Relevant(i).Y );
		Ar.Log( *Str.LeftChop(1) );
		return 1;
	}
#else
		FString Str = TEXT("");
		// Available fullscreen video modes
		INT display_count = 0, display_index = 0, mode_index = 0, i=0;
		SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

		if ((display_count = SDL_GetNumDisplayModes(0)) < 1)
		{
			debugf(NAME_Init, TEXT("No available fullscreen video modes"));
		}
		//debugf(TEXT("SDL_GetNumVideoDisplays returned: %i"), display_count);

		 for(i = 0; i < display_count; i++)
		 {
			mode.format = 0;
			mode.w = 0;
			mode.h = 0;
			mode.refresh_rate = 0;
			mode.driverdata = 0;
			if (SDL_GetDisplayMode(display_index, i, &mode) != 0)
				debugf(TEXT("SDL_GetDisplayMode failed: %s"), SDL_GetError());
			//debugf(TEXT("SDL_GetDisplayMode(0, 0, &mode):\t\t%i bpp\t%i x %i"), SDL_BITSPERPIXEL(mode.format), mode.w, mode.h);
			Str += FString::Printf(TEXT("%ix%i "), mode.w, mode.h);
		}
		// Send the resolution string to the engine.
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
#endif
	if( ParseCommand(&Cmd,TEXT("CrashGL")) )
	{
		appErrorf(TEXT("XOpenGL: Forced OpenGL crash!"));
	}
	if (ParseCommand(&Cmd, TEXT("VideoFlush")))
	{
		Flush(1);
		return 1;
	}
	return 0;
	unguard;
}

void UXOpenGLRenderDevice::SetSceneNode( FSceneNode* Frame )
{
	guard(UOpenGLDriver::SetSceneNode);

	//Flush buffers
	SetProgram( -1 );

	// Precompute stuff.
	FLOAT zNear	= 1.f;
	FLOAT zFar	= (GIsEditor && Frame->Viewport->Actor->RendMap==REN_Wire) ? 131072.0 : 65336.f;
	bool UpdateMatrix=false;

	//avoid some overhead, only calculate and set again if something was really changed.
	if (Frame->Viewport->IsOrtho() && (!bIsOrtho || StoredOrthoFovAngle != Viewport->Actor->FovAngle || StoredOrthoFX != Frame->FX || StoredOrthoFY != Frame->FY))
	{
		StoredFovAngle = StoredFX = StoredFY = 0; //ensure Matrix is updated again if not ortho.

		StoredOrthoFovAngle = Viewport->Actor->FovAngle;
		StoredOrthoFX = Frame->FX;
		StoredOrthoFY = Frame->FY;

		Aspect      = Frame->FY/Frame->FX;
		RProjZ      = appTan( Viewport->Actor->FovAngle * PI/360.0 );
		RFX2        = 2.0*RProjZ/Frame->FX;
		RFY2        = 2.0*RProjZ*Aspect/Frame->FY;
		projMat		= glm::ortho(-RProjZ, +RProjZ, -Aspect*RProjZ, +Aspect*RProjZ, -zFar, zFar);

		// Set viewport and projection.
		glViewport( Frame->XB, Viewport->SizeY-Frame->Y-Frame->YB, Frame->X, Frame->Y );
		bIsOrtho = true;
		UpdateMatrix = true;
	}
	else if (StoredFovAngle != Viewport->Actor->FovAngle || StoredFX != Frame->FX || StoredFY != Frame->FY)
	{
		StoredFovAngle = Viewport->Actor->FovAngle;
		StoredFX = Frame->FX;
		StoredFY = Frame->FY;

		Aspect      = Frame->FY/Frame->FX;
		RProjZ      = appTan( Viewport->Actor->FovAngle * PI/360.0 );
		RFX2        = 2.0*RProjZ/Frame->FX;
		RFY2        = 2.0*RProjZ*Aspect/Frame->FY;
		projMat		= glm::frustum(-RProjZ*zNear, +RProjZ*zNear, -Aspect*RProjZ*zNear, +Aspect*RProjZ*zNear, 1.0f*zNear, zFar);

		//debugf(TEXT("NewModelMat"));
		// Set viewport and projection.
		glViewport( Frame->XB, Viewport->SizeY-Frame->Y-Frame->YB, Frame->X, Frame->Y );
		UpdateMatrix = true;
	}

	//ModelViewProjectionMatrix
	if (UpdateMatrix)
	{
		modelviewprojMat=projMat*viewMat*modelMat; //yes, this is right.
		modelviewMat=viewMat*modelMat;
		glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(modelviewprojMat));
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(modelviewMat));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		CHECK_GL_ERROR();
	}

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
	unguard;
}

void UXOpenGLRenderDevice::Lock( FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
{
	guard(UXOpenGLRenderDevice::Lock);
	check(LockCount==0);
	++LockCount;

	// Make this context current.
	MakeCurrent();

	// Compensate UED coloring for sRGB.
	if (GIsEditor)
        ScreenClear = FOpenGLGammaDecompress_sRGB(ScreenClear);

	// Clear the Z buffer if needed.
	glClearColor(ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);

	if (glClearDepthf)
		glClearDepthf( 1.f );
#ifndef __EMSCRIPTEN__
	else
		glClearDepth( 1.0 );
#endif

	if (glDepthRangef)
		glDepthRangef( 0.f, 1.f );
#ifndef __EMSCRIPTEN__
	else
		glDepthRange( 0.0, 1.0 );
#endif

	glPolygonOffset(-1.f, -1.f);
	SetBlend( PF_Occlude, 0);
	glClear( GL_DEPTH_BUFFER_BIT | ((RenderLockFlags&LOCKR_ClearScreen)?GL_COLOR_BUFFER_BIT:0) );

	glDepthFunc( GL_LEQUAL );

	// Remember stuff.
	FlashScale = InFlashScale;
	FlashFog   = InFlashFog;

	// Lock hits.
	HitData    = InHitData;
	HitSize    = InHitSize;

	// Reset stats.
	appMemzero(&Stats, sizeof(Stats));

	LockHit(InHitData, InHitSize);

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::Unlock( UBOOL Blit )
{
	guard(UXOpenGLRenderDevice::Unlock);

	// Unlock and render.
	check(LockCount==1);

#ifdef SDL2BUILD
    if( Blit )
	{
		SDL_Window* Window = (SDL_Window*) Viewport->GetWindow();
		SDL_GL_SwapWindow(Window);
	}
#elif QTBUILD

#else
    if( Blit )
		verify(SwapBuffers( hDC ));
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
	if ( UseSRGBTextures )
		GammaCorrection += OPENGL_SRGB_GAMMA_OFFSET;

	for( INT i=0; i<256; i++ )
	{
		Ramp.red[i]   = Clamp( appRound(appPow(OriginalRamp.red[i]/65535.f,0.4f/GammaCorrection)*65535.f), 0, 65535 );
		Ramp.green[i] = Clamp( appRound(appPow(OriginalRamp.green[i]/65535.f,0.4f/GammaCorrection)*65535.f), 0, 65535 );
		Ramp.blue[i]  = Clamp( appRound(appPow(OriginalRamp.blue[i]/65535.f,0.4f/GammaCorrection)*65535.f), 0, 65535 );
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

	GammaCorrection += 0.1; // change range from 0.0-0.9 to 0.1 to 1.0
	Gamma = GammaCorrection;

	unguard;
}

void UXOpenGLRenderDevice::ClearZ( FSceneNode* Frame )
{
	guard(UXOpenGLRenderDevice::ClearZ);
	CHECK_GL_ERROR();
	SetSceneNode(Frame);
	SetBlend( PF_Occlude, 0 );
	glClear( GL_DEPTH_BUFFER_BIT );
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::GetStats( TCHAR* Result )
{
	guard(UXOpenGLRenderDevice::GetStats);
	appSprintf
	(
		Result,
		TEXT("XOpenGL stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f GouraudList=%04.1f Tile=%04.1f"),
		GSecondsPerCycle*1000 * Stats.BindCycles,
		GSecondsPerCycle*1000 * Stats.ImageCycles,
		GSecondsPerCycle*1000 * Stats.ComplexCycles,
		GSecondsPerCycle*1000 * Stats.GouraudPolyCycles,
		GSecondsPerCycle * 1000 * Stats.GouraudPolyListCycles,
		GSecondsPerCycle*1000 * Stats.TileCycles
	);
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::ReadPixels( FColor* Pixels )
{
	guard(UXOpenGLRenderDevice::ReadPixels);

	MakeCurrent();

	INT x, y;
	INT SizeX, SizeY;

	SizeX = Viewport->SizeX;
	SizeY = Viewport->SizeY;

	glReadPixels( 0, 0, SizeX, SizeY, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
	for( INT i=0; i<SizeY/2; i++ )
	{
		for( INT j=0; j<SizeX; j++ )
		{
			Exchange( Pixels[j+i*SizeX].R, Pixels[j+(SizeY-1-i)*SizeX].B );
			Exchange( Pixels[j+i*SizeX].G, Pixels[j+(SizeY-1-i)*SizeX].G );
			Exchange( Pixels[j+i*SizeX].B, Pixels[j+(SizeY-1-i)*SizeX].R );
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

void UXOpenGLRenderDevice::PrecacheTexture( FTextureInfo& Info, DWORD PolyFlags )
{
	guard(UXOpenGLRenderDevice::PrecacheTexture);
	SetTexture( 0, Info, PolyFlags, 0.0, 0);
	unguard;
}

void UXOpenGLRenderDevice::DeleteBuffers()
{
	// Clean up buffers.
	// DrawSimple
	if (Draw2DLineVertsBuf)
		delete[] Draw2DLineVertsBuf;
	if (Draw2DPointVertsBuf)
		delete[] Draw2DPointVertsBuf;
	if (Draw3DLineVertsBuf)
		delete[] Draw3DLineVertsBuf;
	if (EndFlashVertsBuf)
		delete[] EndFlashVertsBuf;
	if (DrawLinesVertsBuf)
		delete[] DrawLinesVertsBuf;

	// DrawTile;
	if (DrawTileVertsBuf)
		delete[] DrawTileVertsBuf;

	// DrawGouraud
	if (DrawGouraudBuf)
		delete[] DrawGouraudBuf;
#if ENGINE_VERSION==227
	if (DrawGouraudPolyListVertsBuf)
		delete[] DrawGouraudPolyListVertsBuf;
	if (DrawGouraudPolyListVertNormalsBuf)
		delete[] DrawGouraudPolyListVertNormalsBuf;
	if (DrawGouraudPolyListTexBuf)
		delete[] DrawGouraudPolyListTexBuf;
	if (DrawGouraudPolyListLightColorBuf)
		delete[] DrawGouraudPolyListLightColorBuf;
	if (DrawGouraudPolyListSingleBuf)
		delete[] DrawGouraudPolyListSingleBuf;
#endif

	// DrawComplex Interleaved
	if (DrawComplexVertsSingleBuf)
		delete[] DrawComplexVertsSingleBuf;
}

void UXOpenGLRenderDevice::Exit()
{
	guard(UXOpenGLRenderDevice::Exit);
	//debugf(NAME_Exit, TEXT("XOpenGL: Exit"));

	Flush(0);

	DeleteShaderBuffers();
	delete SharedBindMap;
	SharedBindMap = NULL;

#ifndef __EMSCRIPTEN__  // !!! FIXME: upsets Emscripten.
	//Unmap persistent buffer.
	glUnmapBuffer(GL_ARRAY_BUFFER);
#endif

	DeleteBuffers();

#ifdef SDL2BUILD
	CurrentGLContext = NULL;
	SDL_GL_DeleteContext(glContext);
#elif QTBUILD

#else
	// Shut down this GL context. May fail if window was already destroyed.
	check(hRC)
	CurrentGLContext = NULL;
	verify(wglMakeCurrent( NULL, NULL ));
	verify(wglDeleteContext( hRC ));
	verify(AllContexts.RemoveItem(hRC)==1);
	hRC = NULL;
	if( WasFullscreen )
	{
		TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL,0),ChangeDisplaySettingsA(NULL,0));
	}

	if( hDC )
		ReleaseDC(hWnd,hDC);

	// Shut down global GL.
	if (NumContexts == 0)
	{
		if (SharedBindMap)
			SharedBindMap->~TOpenGLMap<QWORD, FCachedTexture>();
		AllContexts.~TArray<HGLRC>();
	}
#endif

	#if _WIN32 && ENGINE_VERSION!=227
		TimerEnd();
	#endif

	unguard;
}

void UXOpenGLRenderDevice::ShutdownAfterError()
{
	guard(UXOpenGLRenderDevice::ShutdownAfterError);
	debugf(NAME_Exit, TEXT("XOpenGL: ShutdownAfterError"));

	Flush(0);

	DeleteShaderBuffers();
	if (SharedBindMap)
		SharedBindMap->~TOpenGLMap<QWORD,FCachedTexture>();

#ifndef __EMSCRIPTEN__  // !!! FIXME: upsets Emscripten.
	//Unmap persistent buffer.
	glUnmapBuffer(GL_ARRAY_BUFFER);
#endif

	DeleteBuffers();

#ifdef SDL2BUILD
	CurrentGLContext = NULL;
	SDL_GL_DeleteContext(glContext);
#elif QTBUILD

#else
	// Shut down this GL context. May fail if window was already destroyed.
	check(hRC)
	CurrentGLContext = NULL;
	verify(wglMakeCurrent( NULL, NULL ));
	verify(wglDeleteContext( hRC ));
	verify(AllContexts.RemoveItem(hRC)==1);
	hRC = NULL;
	if( WasFullscreen )
		TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL,0),ChangeDisplaySettingsA(NULL,0));

	if( hDC )
		ReleaseDC(hWnd,hDC);

	// Shut down global GL.
	AllContexts.~TArray<HGLRC>();
#endif

	#if _WIN32 && ENGINE_VERSION!=227
		TimerEnd();
	#endif

	unguard;
}

void UXOpenGLRenderDevice::DrawStats( FSceneNode* Frame )
{
	guard(UXOpenGLRenderDevice::DrawStats);

	INT CurY = 32;

	Frame->Viewport->Canvas->Color = FColor(255,255,255);
	Frame->Viewport->Canvas->CurX=10;
	Frame->Viewport->Canvas->CurY=CurY;
	Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->SmallFont,0, TEXT("XOpenGL Statistics"));

#if ENGINE_VERSION==227
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->SmallFont, 0, TEXT("Number of virtual Lights: %i"),LightNum);
#endif
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Cycles:"));
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Bind____________= %05.2f"), GSecondsPerCycle * 1000 * Stats.BindCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Image___________= %05.2f"), GSecondsPerCycle * 1000 * Stats.ImageCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Blend___________= %05.2f"), GSecondsPerCycle * 1000 * Stats.BlendCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Program_________= %05.2f"), GSecondsPerCycle * 1000 * Stats.ProgramCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Tile____________= %05.2f"), GSecondsPerCycle * 1000 * Stats.TileCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("ComplexSurface__= %05.2f"), GSecondsPerCycle * 1000 * Stats.ComplexCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("GouraudPoly_____= %05.2f"), GSecondsPerCycle * 1000 * Stats.GouraudPolyCycles);
	Frame->Viewport->Canvas->CurX = 10;
	Frame->Viewport->Canvas->CurY = (CurY += 8);
	Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("GouraudPolyList_= %05.2f"), GSecondsPerCycle * 1000 * Stats.GouraudPolyListCycles);

	unguard;
}

// Static variables.
#ifdef SDL2BUILD
SDL_GLContext		UXOpenGLRenderDevice::CurrentGLContext    = NULL;
TArray<SDL_GLContext> UXOpenGLRenderDevice::AllContexts;
#elif QTBUILD

#else
HGLRC				UXOpenGLRenderDevice::CurrentGLContext    = NULL;
TArray<HGLRC>		UXOpenGLRenderDevice::AllContexts;
#endif
INT					UXOpenGLRenderDevice::LockCount     = 0;
INT					UXOpenGLRenderDevice::LogLevel    = 0;
DWORD				UXOpenGLRenderDevice::ComposeSize    = 0;
BYTE*				UXOpenGLRenderDevice::Compose        = NULL;

bool				UXOpenGLRenderDevice::NeedGlewInit = true;
INT					UXOpenGLRenderDevice::NumContexts = 0;

// Shaderprogs
INT					UXOpenGLRenderDevice::ActiveProgram	= -1;

// Gamma
FLOAT				UXOpenGLRenderDevice::Gamma	= 0.f;

TOpenGLMap<QWORD,UXOpenGLRenderDevice::FCachedTexture> *UXOpenGLRenderDevice::SharedBindMap;

void autoInitializeRegistrantsXOpenGLDrv(void)
{
    UXOpenGLRenderDevice::StaticClass();
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

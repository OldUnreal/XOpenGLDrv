/*=============================================================================
	XOpenGLDrv.h: Unreal OpenGL support header.

	Copyright 2014-2021 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Enables CHECK_GL_ERROR(). Deprecated, should use UseOpenGLDebug=True instead, but still may be handy to track something specific down.
// #define DEBUGGL 1

#pragma once

#ifdef _MSC_VER
#pragma warning(disable: 4351)
#endif

#ifdef WIN32
	#define WINBUILD 1
	#include <windows.h>
	#include <mmsystem.h>

	extern "C"
	{
		#include "glad.h"
	}
	#include "glext.h" // from https://khronos.org/registry/OpenGL/index_gl.php
	#define WGL_WGLEXT_PROTOTYPES
	#include "wglext.h"

#else
	extern "C"
	{
		#include "glad.h"
	}
    #include <unistd.h>
#endif

#if !_WIN32
	#if SDL2BUILD
		#include <SDL2/SDL.h>
	#elif SDL3BUILD
		#include <SDL3/SDL.h>
	#else
		#error XOpenGLDrv requires either SDL2BUILD or SDL3BUILD to be enabled.
	#endif
#endif


#include "XOpenGLTemplate.h" //thanks han!

#if ENGINE_VERSION==436 || ENGINE_VERSION==430
#define clockFast(Timer)   {Timer -= appCycles();}
#define unclockFast(Timer) {Timer += appCycles()-34;}
#elif UNREAL_OLDUNREAL
// stijn: Do we want to release resources (e.g., bound textures) if the game crashes?
// None of the original renderer or audio devices did this because you can easily trigger
// another crash during cleanup. This would change your crash message and hide the original
// cause of the crash.
#define XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP 1
#define XOPENGL_MODIFIED_LOCK 1

#define FGetHSV FPlane::GetHSV
#elif UNREAL_TOURNAMENT_OLDUNREAL
// stijn: Just do what other devices do!
#define XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP 0
#endif

#if MACOSX
#undef STAT
#define STAT(x)
#endif

#if XOPENGL_MODIFIED_LOCK
#define FTEXTURE_PTR FTextureInfo*
#define FTEXTURE_GET(ptr) *ptr
#define TEXTURE_SCALE_NAME DrawScale
#else
#define FTEXTURE_PTR FTextureInfo
#define FTEXTURE_GET(ptr) ptr
#define TEXTURE_SCALE_NAME Scale
#endif

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/
#define MAX_FRAME_RECURSION 4

// stijn: per-drawcall data absolutely needs to use GL_STREAM_DRAW or GL_DYNAMIC_DRAW on mac
#define DRAWCALL_BUFFER_USAGE_PATTERN GL_STREAM_DRAW
#define VERTEX_BUFFER_USAGE_PATTERN GL_STREAM_DRAW
// stijn: this still works reasonably well with GL_STATIC_DRAW, but GL_DYNAMIC_DRAW still gives us a minor performance boost
#define UNIFORM_BUFFER_USAGE_PATTERN GL_DYNAMIC_DRAW

#define DRAWSIMPLE_SIZE 1024
#define DRAWTILE_SIZE 1024
#define DRAWCOMPLEX_SIZE 1024
#define DRAWGOURAUDPOLY_SIZE 1024
#define NUMBUFFERS 8

#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
# define MAX_LIGHTS 256
#else
# define MAX_LIGHTS 512
#endif

// necessary defines for GLES (f.e. when building with glad). Only needed to build. Do NOT use these functions for ES. Check if maybe existing some day.

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

enum ELightMode
{
	LM_Unreal = 0,
	LM_Bright = 1,
};

enum EOpenGLVersion
{
	GL_Core = 0,
	GL_ES = 1,
};

enum EVSync
{
	VS_Off      = 0,
	VS_On       = 1,
    VS_Adaptive = 2,
};

enum DrawSimpleMode
{
    DrawLineMode        = 0,
	Draw2DPointMode     = 1,
	Draw2DLineMode      = 2,
	Draw3DLineMode      = 3,
	DrawEndFlashMode    = 4,
};

enum eParallaxVersion
{
    Parallax_Disabled	= 0,
    Parallax_Basic		= 1,
    Parallax_Occlusion	= 2,
    Parallax_Relief		= 3,
};

// stijn: missing defs in UT469 tree
#ifdef UNREAL_TOURNAMENT_OLDUNREAL
//#define PF_AlphaBlend 0x20000
#define TEXF_RGBA8 TEXF_BGRA8 // stijn: really BGRA8

enum ERenderZTest
{
	ZTEST_Less,
	ZTEST_Equal,
	ZTEST_LessEqual,
	ZTEST_Greater,
	ZTEST_GreaterEqual,
	ZTEST_NotEqual,
	ZTEST_Always
};
#endif

#ifndef REN_WalkableSurfs
#define REN_WalkableSurfs 7
#endif

#ifdef DEBUGGL
#define CHECK_GL_ERROR() CheckGLError(__FILE__, __LINE__)
#define CLEAR_GL_ERROR() glGetError()
#else
#define CHECK_GL_ERROR()
#define CLEAR_GL_ERROR()
#endif

inline int RGBA_MAKE( BYTE r, BYTE g, BYTE b, BYTE a)
{
	return (a << 24) | (b <<16) | ( g<< 8) | r;
}

inline FLOAT FOpenGLGammaCompress_sRGB_Inner(FLOAT C)
{
	if (C <= 0.00304f)
		return 12.92f*C;
	return (1.055f*appPow(C, 1.0f / 2.4f) - 0.055f);

}

inline FPlane FOpenGLGammaCompress_sRGB(FPlane& Color)
{
	return FPlane
	(
		FOpenGLGammaCompress_sRGB_Inner(Color.X),
		FOpenGLGammaCompress_sRGB_Inner(Color.Y),
		FOpenGLGammaCompress_sRGB_Inner(Color.Z),
		Color.W
	);
}

inline FLOAT FOpenGLGammaDecompress_sRGB_Inner(FLOAT C)
{
	if (C <= 0.03928f)
		return C / 12.92f;
	return appPow((C + 0.055f) / 1.055f, 2.4f);
}

inline FPlane FOpenGLGammaDecompress_sRGB(FPlane& Color)
{
	return FPlane
	(
		FOpenGLGammaDecompress_sRGB_Inner(Color.X),
		FOpenGLGammaDecompress_sRGB_Inner(Color.Y),
		FOpenGLGammaDecompress_sRGB_Inner(Color.Z),
		Color.W
	);
}

inline FString GetPolyFlagString(DWORD PolyFlags)
{
    FString String=TEXT("");
    if (PolyFlags & PF_Invisible)
        String+=TEXT("PF_Invisible ");
    if (PolyFlags & PF_Masked)
        String+=TEXT("PF_Masked ");
    if (PolyFlags & PF_Translucent)
        String+=TEXT("PF_Translucent ");
    if (PolyFlags & PF_NotSolid)
        String+=TEXT("PF_NotSolid ");
    if (PolyFlags & PF_Environment)
        String+=TEXT("PF_Environment ");
    if (PolyFlags & PF_Semisolid)
        String+=TEXT("PF_Semisolid ");
    if (PolyFlags & PF_Modulated)
        String+=TEXT("PF_Modulated ");
    if (PolyFlags & PF_FakeBackdrop)
        String+=TEXT("PF_FakeBackdrop ");
    if (PolyFlags & PF_TwoSided)
        String+=TEXT("PF_TwoSided ");
    if (PolyFlags & PF_AutoUPan)
        String+=TEXT("PF_AutoUPan ");
    if (PolyFlags & PF_AutoVPan)
        String+=TEXT("PF_AutoVPan ");
    if (PolyFlags & PF_NoSmooth)
        String+=TEXT("PF_NoSmooth ");
    if (PolyFlags & PF_BigWavy)
        String+=TEXT("PF_BigWavy ");
    if (PolyFlags & PF_SpecialPoly)
        String+=TEXT("PF_SpecialPoly ");
    if (PolyFlags & PF_Flat)
        String+=TEXT("PF_Flat ");
    if (PolyFlags & PF_ForceViewZone)
        String+=TEXT("PF_ForceViewZone ");
    if (PolyFlags & PF_LowShadowDetail)
        String+=TEXT("PF_LowShadowDetail ");
    if (PolyFlags & PF_NoMerge)
        String+=TEXT("PF_NoMerge ");
    if (PolyFlags & PF_AlphaBlend)
        String+=TEXT("PF_AlphaBlend ");
    if (PolyFlags & PF_DirtyShadows)
        String+=TEXT("PF_DirtyShadows ");
    if (PolyFlags & PF_BrightCorners)
        String+=TEXT("PF_BrightCorners ");
    if (PolyFlags & PF_SpecialLit)
        String+=TEXT("PF_SpecialLit ");
#if UNREAL_OLDUNREAL
    if (PolyFlags & PF_Gouraud)
        String+=TEXT("PF_Gouraud ");
#endif
    if (PolyFlags & PF_NoBoundRejection)
        String+=TEXT("PF_NoBoundRejection ");
    if (PolyFlags & PF_Unlit)
        String+=TEXT("PF_Unlit ");
    if (PolyFlags & PF_HighShadowDetail)
        String+=TEXT("PF_HighShadowDetail ");
    if (PolyFlags & PF_Portal)
        String+=TEXT("PF_Portal ");
    if (PolyFlags & PF_Mirrored)
        String+=TEXT("PF_Mirrored ");
    if (PolyFlags & PF_Memorized)
        String+=TEXT("PF_Memorized ");
    if (PolyFlags & PF_Selected)
        String+=TEXT("PF_Selected ");
    if (PolyFlags & PF_Highlighted)
        String+=TEXT("PF_Highlighted ");
    if (PolyFlags & PF_FlatShaded)
        String+=TEXT("PF_FlatShaded ");
    if (PolyFlags & PF_Selected)
        String+=TEXT("PF_Selected ");
    if (PolyFlags & PF_EdProcessed)
        String+=TEXT("PF_EdProcessed ");
    if (PolyFlags & PF_EdCut)
        String+=TEXT("PF_EdCut ");
    if (PolyFlags & PF_RenderFog)
        String+=TEXT("PF_RenderFog ");
    if (PolyFlags & PF_Occlude)
        String+=TEXT("PF_Occlude ");
    if (PolyFlags & PF_RenderHint)
        String+=TEXT("PF_RenderHint ");

    return String;
}

// Error checking
inline int	CheckGLError(const char* file, int line)
{
	GLenum glErr = glGetError();
	if (glErr != GL_NO_ERROR)
	{
		const TCHAR* Msg = TEXT("Unknown");
		switch (glErr)
		{
            case GL_NO_ERROR:           Msg = TEXT("GL_NO_ERROR");          break;
            case GL_INVALID_ENUM:       Msg = TEXT("GL_INVALID_ENUM");      break;
            case GL_INVALID_VALUE:      Msg = TEXT("GL_INVALID_VALUE");     break;
            case GL_INVALID_OPERATION:  Msg = TEXT("GL_INVALID_OPERATION"); break;
            case GL_STACK_OVERFLOW:     Msg = TEXT("GL_STACK_OVERFLOW");    break;
            case GL_STACK_UNDERFLOW:    Msg = TEXT("GL_STACK_UNDERFLOW");   break;
            case GL_OUT_OF_MEMORY:      Msg = TEXT("GL_OUT_OF_MEMORY");     break;
		};
		GWarn->Logf(TEXT("XOpenGL Error: %ls (%i) file %ls at line %i"), Msg, glErr, appFromAnsi(file), line);
	}
	return 1;
}

inline glm::vec4 FPlaneToVec4(FPlane Plane)
{
	return glm::vec4(Plane.X, Plane.Y, Plane.Z, Plane.W);
}

#ifndef END_LINE
#define END_LINE "\n"
#endif

// This is Higor's FShaderWriter. We could (and should? move it elsewhere
// because OpenGLDrv also uses it). Renamed to FShaderWriterX (for now) because
// it clashes with FShaderWriter in statically linked builds.
class FShaderWriterX
{
public:
	TArray<ANSICHAR> Data;

	FShaderWriterX()
	{
		Data.Reserve(1000);
		Data.AddNoCheck();
		Data(0) = '\0';
	}

#if __cplusplus > 201103L || _MSVC_LANG > 201103L
	template < INT Size > FShaderWriterX& operator<<(const char(&Input)[Size])
	{
		if (Size > 1)
		{
			INT i = Data.Add(Size - 1) - 1;
			appMemcpy(&Data(i), Input, Size);
		}
		check(Data.Last() == '\0');
		return *this;
	}
#endif

	FShaderWriterX& operator<<(const char* Input)
	{
		const char* InputEnd = Input;
		while (*InputEnd != '\0')
			InputEnd++;
		if (InputEnd != Input)
		{
			INT Len = (INT)(InputEnd - Input);
			INT i = Data.Add(Len) - 1;
			check(Len > 0);
			//check(Len < 4096);
			appMemcpy(&Data(i), Input, Len + 1);
		}
		check(Data.Last() == '\0');
		return *this;
	}

	FShaderWriterX& operator<<(INT Input)
	{
		TCHAR Buffer[16];
		appSprintf(Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}

	FShaderWriterX& operator<<(EPolyFlags Input)
	{
		TCHAR Buffer[16];
		appSprintf(Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}


	FShaderWriterX& operator<<(FLOAT Input)
	{
		TCHAR Buffer[32];
		appSprintf(Buffer, TEXT("%f"), Input);
		return *this << appToAnsi(Buffer);
	}

	const char* operator*()
	{
		return &Data(0);
	}

	GLsizei Length()
	{
		return (GLsizei)(Data.Num() - 1);
	}

	void Reset()
	{
		Data.EmptyNoRealloc();
		Data.AddNoCheck();
		Data(0) = '\0';
	}
};

/*-----------------------------------------------------------------------------
	XOpenGLDrv.
-----------------------------------------------------------------------------*/

#if UNREAL_TOURNAMENT_OLDUNREAL
class UXOpenGLRenderDevice : public URenderDeviceOldUnreal469
#else
class UXOpenGLRenderDevice : public URenderDevice
#endif
{
#if UNREAL_OLDUNREAL
	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config, XOpenGLDrv)
#elif ENGINE_VERSION==430
	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config, XOpenGLDrv)
#elif UNREAL_TOURNAMENT_OLDUNREAL
	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDeviceOldUnreal469, CLASS_Config, XOpenGLDrv)
#elif ENGINE_VERSION>=436 && ENGINE_VERSION < 1100
    DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config, XOpenGLDrv)
#else
	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config)
#endif

	//
	// Renderer Options. Most of these are configurable through the game ini
	//
	BITFIELD NoFiltering;
	BITFIELD ShareLists;
	BITFIELD AlwaysMipmap;
	BITFIELD UsePrecache;
	BITFIELD UseTrilinear;
	BITFIELD UseAA;
	BITFIELD UseAASmoothing;
	BITFIELD GammaCorrectScreenshots;
	BITFIELD MacroTextures;
	BITFIELD BumpMaps;
	BITFIELD NoAATiles;
	BITFIELD GenerateMipMaps;
	BITFIELD SimulateMultiPass;
	BITFIELD UseOpenGLDebug;
	BITFIELD NoDrawComplexSurface;
	BITFIELD NoDrawGouraud;
	BITFIELD NoDrawGouraudList;
	BITFIELD NoDrawTile;
	BITFIELD NoDrawSimple;
	BITFIELD UseHWLighting;
	BITFIELD UseHWClipping;
	BITFIELD UseEnhancedLightmaps;
	BITFIELD OneXBlending;
	BITFIELD ActorXBlending;

	//OpenGL 4 Config
	BITFIELD UseBindlessTextures;
	BITFIELD UsePersistentBuffers;
	BITFIELD UseBufferInvalidation;
	BITFIELD UseShaderDrawParameters;
	BITFIELD UseIndirectDraw;
	BITFIELD UseShaderCache;
#if _WIN32
	BITFIELD ReduceMouseLag; // Present through a low-latency DXGI flip-model swapchain (WGL_NV_DX_interop)
#endif

	// Not really in use...(yet)
	BITFIELD UseMeshBuffering; //Buffer (Static)Meshes for drawing.
	BITFIELD UseSRGBTextures;
	BITFIELD EnvironmentMaps;

	FLOAT GammaMultiplier;
	FLOAT GammaMultiplierUED;
	FLOAT GammaOffsetScreenshots;
	FLOAT LODBias;

	// Miscellaneous
	INT RefreshRate;
	FLOAT MaxAnisotropy;
	INT DebugLevel;
	INT NumAASamples;
	INT DetailMax;
	BYTE OpenGLVersion;
	BYTE ParallaxVersion;
	BYTE UseVSync;

	// Not configurable
	bool	UsingPersistentBuffers;
	bool	UsingShaderDrawParameters;
	bool	UsingIndirectDraw;
	bool    UsingGeometryShaders;
	static INT LogLevel; // Verbosity level of the GL debug logging

	//
	// Window, OS, and global GL context state
	//
#ifdef _WIN32
	HGLRC glContext;
	HWND hWnd;
	HDC hDC;
    PIXELFORMATDESCRIPTOR pfd;
    static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
    static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
	static PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
	TArray<FPlane> SupportedDisplayModes;

# if !defined(_USING_V110_SDK71_)
	// DXGI low-latency swapchain (ReduceMouseLag) via WGL_NV_DX_interop.
	static PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
	static PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
	static PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
	static PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
	static PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
	static PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;
	static UBOOL SUPPORTS_WGL_NV_DX_interop;

	UBOOL  DXGISupportsTearing;
	UBOOL  UsingDXGISwapchain;
	DWORD  DXGISwapChainFlags;
	void*  pD3D11Device;   // ID3D11Device*
	void*  pDXGISwapChain; // IDXGISwapChain*
	HANDLE hDXDevice;      // WGL DX interop device handle
	HANDLE hDXBackBuffer;  // WGL DX interop backbuffer object handle
	GLuint DXGIInteropTextureGL;
	void*  DXGIInteropTextureD3D; // ID3D11Texture2D*
	GLuint DXGIFramebuffer;
	INT    DXGIWidth;
	INT    DXGIHeight;
# endif
#else
	SDL_GLContext glContext;
	SDL_Window* Window;
#endif
	static INT NumDevices;
	static BOOL SelectedGLVersion;
	static INT SelectedMajorVersion;
	static INT SelectedMinorVersion;

	UBOOL WasFullscreen;

	INT CachedPhysicalSizeX;
	INT CachedPhysicalSizeY;

	GLuint RenderFBO;
	GLuint RenderColorTexture;
	GLuint RenderColorMSAA;       // multisample color renderbuffer (UseAA only)
	GLuint RenderResolvedFBO;     // single-sample resolve target (UseAA only)
	GLuint RenderResolvedDepthAttachment; // single-sample depth/stencil for RenderResolvedFBO, so hit
	                                       // testing can render directly into it instead of MSAA (UseAA only)
	GLuint RenderDepthAttachment;
	INT    RenderFBOWidth;
	INT    RenderFBOHeight;
	UBOOL  RenderFBOBound;

	// Context specifics.
	INT DesiredColorBits;
	INT DesiredStencilBits;
	INT DesiredDepthBits;
	INT iPixelFormat;

	//Gamma
	struct FGammaRamp
	{
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	};
	struct FByteGammaRamp
	{
		BYTE red[256];
		BYTE green[256];
		BYTE blue[256];
	};
	FGammaRamp OriginalRamp; // to restore original value at exit or crash.
	FLOAT Gamma;

#if !_WIN32
	static SDL_GLContext CurrentGLContext;
	static TArray<SDL_GLContext> AllContexts;
#else
	static TArray<HGLRC> AllContexts;
	static HGLRC   CurrentGLContext;
#endif

	//
	// Hardware Capabilities and Constraints
	//
	FString AllExtensions;
	INT		MaxClippingPlanes;
	INT		NumberOfExtensions;
	INT		MaxUniformBlockSize;
	INT		MaxSSBOBlockSize;
	bool	SupportsAMDMemoryInfo;
	bool	SupportsNVIDIAMemoryInfo;
	bool	SupportsSwapControl;
	bool	SupportsSwapControlTear;
	bool	SupportsS3TC;
	bool	SupportsSSBO;
	bool	SupportsGLSLInt64;
	bool	SupportsClipDistance;
	bool	IsAMD; // stijn: GL_VENDOR identifies this as an AMD/ATI GPU. These GPUs don't like how we use bindless textures in XOpenGLDrv

	//
	// Framerate Limiter
	//
#if UNREAL_OLDUNREAL
	FTime prevFrameTimestamp;
	INT FrameRateLimit;
#endif

	//
	// Current GL Context and Renderer State
	//
	GLuint NumClipPlanes;
	BYTE LastZMode;
	DWORD CachedPolyFlags; // The last set of polyflags we derived drawflags for
	DWORD CachedDrawFlags; // And the corresponding drawflags

	// Lock variables.
	FPlane FlashScale, FlashFog;
	FLOAT RProjZ, Aspect;
	DWORD CurrentBlendPolyFlags;
	DWORD CurrentLineFlags;
	FLOAT RFX2, RFY2;

	// Stored uniforms
	FLOAT StoredFovAngle;
	FLOAT StoredFX;
	FLOAT StoredFY;
	FLOAT StoredOrthoFovAngle;
	FLOAT StoredOrthoFX;
	FLOAT StoredOrthoFY;
	UBOOL StoredbNearZ;
	FLOAT StoredGamma;
	UBOOL StoredOneXBlending;
	UBOOL StoredActorXBlending;
#if _WIN32
	UBOOL StoredUsingDXGISwapchain;
#endif
	bool bIsOrtho;

	//
	// Performance Statistics
	//
	struct FGLStats
	{
		DWORD BindCycles;
		DWORD ImageCycles;
		DWORD BlendCycles;
		DWORD ComplexCycles;
		DWORD Draw2DLine;
		DWORD Draw3DLine;
		DWORD Draw2DPoint;
		DWORD GouraudPolyCycles;
		DWORD TileBufferCycles;
		DWORD TileDrawCycles;
		DWORD TriangleCycles;
		DWORD Resample7777Cycles;
		INT StallCount;
	} Stats;

	//
	// Texture State
	//

	// Information about a cached texture.
	struct FCachedTexture
	{
		GLuint Id;					
		INT BaseMip;
		INT MaxLevel;
		GLuint Sampler;				// Sampler object
		GLuint64 BindlessTexHandle;	// Bindless handle
		INT RealtimeChangeCount{};
	};

	// All currently cached textures.
	TOpenGLMap<QWORD,FCachedTexture> LocalBindMap, *BindMap;
	static TOpenGLMap<QWORD, FCachedTexture>* SharedBindMap; // Shared between GL contexts (e.g., in UED)	

	// Describes a currently active (and potentially bound to a TMU) texture
	struct FTexInfo
	{
		QWORD CurrentCacheID{};
		FLOAT UMult{};
		FLOAT VMult{};
		FLOAT UPan{};
		FLOAT VPan{};
		GLuint64 BindlessTexHandle{};
		INT RealTimeChangeCount{};
	} TexInfo[9];
	
	bool UsingBindlessTextures;		// Are we currently using bindless textures?

	//
	// Hit Testing State
	//
	TArray<BYTE> HitStack;
	TArray<BYTE> HitMem;
	TArray<INT>  HitMemOffs;
	FPlane HitColor;
	BYTE*  HitData;
	INT*   HitSize;
	INT    HitCount;

	//
	// Scratch buffer for texture composition, hit testing, readpixels, etc
	//
	static DWORD ComposeSize;
	static BYTE* Compose;

	//
	// BoundBuffers represent BufferObjects that are currently bound to a buffer
	// binding target. Each buffer binding target can only be bound to one
	// buffer at any given time. Thus, whenever we bind a new BufferObject to a
	// binding target, we need to unbind the current BufferObject bound to that
	// target first.
	//
	class BoundBuffer
	{
	public:
		virtual void Bind() = 0;
		virtual void Unbind() = 0;
	};

	BoundBuffer* UBOPoint{};      // aka GL_UNIFORM_BUFFER
	BoundBuffer* SSBOPoint{};     // aka GL_SHADER_STORAGE_BUFFER
	BoundBuffer* ArrayPoint{};    // aka GL_ARRAY_BUFFER
	BoundBuffer* IndirectPoint{}; // aka GL_DRAW_INDIRECT_BUFFER

	//
	// A BufferObject describes a GPU-mapped buffer object. If we're using persistent
	// buffers, we will subdivide this object into up to <NUMBUFFERS> sub-buffers and
	// only activate/pin one sub-buffer at a time while writing.
	//
	// If we're not using persistent buffers, we will allocate a temporary buffer in
	// RAM and ask the GPU driver to copy data over to the backing buffer on the GPU
	// side.
	//
	template<typename T> class BufferObject : public BoundBuffer
	{
		friend class ShaderProgam;

	public:		
		T* Buffer{};                // CPU-accessible mapping of the _entire_ buffer object
		
		BufferObject() = default;
		~BufferObject()
		{
			DeleteBuffer();
		}
		
		// Current size in number of elements
		size_t Size()
		{
			return NextElemIndex;
		}

		// Current size in bytes
		size_t SizeBytes()
		{
			return NextElemIndex * sizeof(T);
		}

		// Offset of the first element of the active sub-buffer, in number of bytes
		GLuint SubBufferOffsetBytes()
		{
			return SubBufferOffset * sizeof(T);
		}

		// Offset of the to-be-buffered region relative to the start of the current sub-buffer, in number of bytes
		GLuint UnbufferedRegionOffsetBytes()
		{
			return FirstUnbufferedElemIndex * sizeof(T);
		}

		// Moves the NextElemIndex forward after buffering @ElementCount elements
		void Advance(GLuint ElementCount)
		{
			NextElemIndex += ElementCount;
		}

		// Returns true if the currently active sub-buffer still has room for @ElementCount elements
		bool CanBuffer(GLuint ElementCount)
		{
			return static_cast<GLint>(SubBufferSize) - static_cast<GLint>(NextElemIndex) >= static_cast<GLint>(ElementCount);
		}

		// Returns true if we have no buffered data in the currently active sub-buffer
		bool IsEmpty()
		{
			return NextElemIndex == 0;
		}

		// Rotates @Index, @SubBufferOffset, and @NextElemIndex so they point to the start of the next sub-buffer
		// if @Wait is true, this function will wait until the GPU has signaled the next sub-buffer
		// Returns true if Index points to a new sub-buffer after this call
		bool Rotate(bool Wait)
		{
			NextElemIndex = 0;
			FirstUnbufferedElemIndex = 0;

			if (bPersistentBuffer)
			{
				Index = (Index + 1) % SubBufferCount;
				SubBufferOffset = Index * SubBufferSize;
				this->Wait();
				return true;
			}

			glBufferData(BufferType, SubBufferSize * sizeof(T), nullptr, ExpectedUsage);
			return true;
		}

		// Position-math-only rotation, no implicit wait -- caller is responsible for waiting on a
		// shared fence (see ShaderProgramImpl::WaitRotation) before writing into the new sub-buffer.
		// Used by buffers that always rotate in lockstep with siblings sharing one fence instead of
		// each maintaining its own; Rotate(bool) above is untouched and still used where a buffer
		// fences itself independently.
		bool Rotate()
		{
			NextElemIndex = 0;
			FirstUnbufferedElemIndex = 0;

			if (bPersistentBuffer)
			{
				Index = (Index + 1) % SubBufferCount;
				SubBufferOffset = Index * SubBufferSize;
				return true;
			}

			glBufferData(BufferType, SubBufferSize * sizeof(T), nullptr, ExpectedUsage);
			return true;
		}

		GLuint GetIndex() const { return Index; }

		T* GetElementPtr(GLuint ElemIndex)
		{
			checkSlow(ElemIndex < NextElemIndex);
			return &Buffer[SubBufferOffset + ElemIndex];
		}

		// Returns a pointer to the element we're currently writing
		T* GetCurrentElementPtr()
		{
			return &Buffer[SubBufferOffset + NextElemIndex];
		}

		// Absolute index (i.e. relative to the whole buffer object, not just the currently active
		// sub-buffer) of the element we're currently writing. This is the value a MultiDrawBuffer's
		// StartDrawCall needs, and it's what a vertex's DrawID must equal to correctly index this
		// buffer's contents from the GLSL side -- see MultiDrawBuffer::StartDrawCall.
		GLuint CurrentAbsolutePosition() const
		{
			return SubBufferOffset + NextElemIndex;
		}

		T* GetLastElementPtr()
		{
			return &Buffer[SubBufferOffset + SubBufferSize - 1];
		}

		// Generates a VBO and VAO for this buffer object
		void GenerateVertexBuffer(UXOpenGLRenderDevice* RenDev)
		{
			BindingPoint = &RenDev->ArrayPoint;
			glGenBuffers(1, &BufferObjectName);
			glGenVertexArrays(1, &VaoObjectName);
		}

		// Generates and binds an SSBO for this buffer object
		void GenerateSSBOBuffer(UXOpenGLRenderDevice* RenDev, const GLuint BindingIndex)
		{
			BindingPoint = &RenDev->SSBOPoint;
			this->BindingIndex = BindingIndex;
			glGenBuffers(1, &BufferObjectName);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, BufferObjectName);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BindingIndex, BufferObjectName);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		// Generates a UBO for this buffer object
		void GenerateUBOBuffer(UXOpenGLRenderDevice* RenDev, const GLuint BindingIndex)
		{
			BindingPoint = &RenDev->UBOPoint;
			this->BindingIndex = BindingIndex;
			glGenBuffers(1, &BufferObjectName);
			glBindBuffer(GL_UNIFORM_BUFFER, BufferObjectName);
			glBindBufferBase(GL_UNIFORM_BUFFER, BindingIndex, BufferObjectName);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		// Generates an indirect-draw command buffer (aka GL_DRAW_INDIRECT_BUFFER) for this buffer object.
		// Not an indexed binding point, so unlike GenerateSSBOBuffer/GenerateUBOBuffer there's no
		// glBindBufferBase step.
		void GenerateIndirectBuffer(UXOpenGLRenderDevice* RenDev)
		{
			BindingPoint = &RenDev->IndirectPoint;
			glGenBuffers(1, &BufferObjectName);
		}

		// Creates a CPU-accessible mapping for this buffer
		void MapVertexBuffer(bool Persistent, GLuint BufferSize, bool UseInvalidation = false)
		{
			MapBuffer(GL_ARRAY_BUFFER, Persistent, BufferSize, VERTEX_BUFFER_USAGE_PATTERN, UseInvalidation);
		}

		void MapSSBOBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=DRAWCALL_BUFFER_USAGE_PATTERN, bool UseInvalidation = false)
		{
			MapBuffer(GL_SHADER_STORAGE_BUFFER, Persistent, BufferSize, ExpectedUsage, UseInvalidation);
		}

		void MapUBOBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=DRAWCALL_BUFFER_USAGE_PATTERN, bool UseInvalidation = false)
		{
			MapBuffer(GL_UNIFORM_BUFFER, Persistent, BufferSize, ExpectedUsage, UseInvalidation);
		}

		void MapIndirectBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=DRAWCALL_BUFFER_USAGE_PATTERN, bool UseInvalidation = false)
		{
			MapBuffer(GL_DRAW_INDIRECT_BUFFER, Persistent, BufferSize, ExpectedUsage, UseInvalidation);
		}

		// Binds and unbinds the buffer so we can write to it
		void Bind()
		{
			if (bBound)
				return;

			if (*BindingPoint)
				(*BindingPoint)->Unbind();
			if (BufferType == GL_ARRAY_BUFFER)
				glBindVertexArray(VaoObjectName);
			glBindBuffer(BufferType, BufferObjectName);
			bBound = true;
			*BindingPoint = this;
		}

		void Unbind()
		{
			if (!bBound)
				return;
			glBindBuffer(BufferType, 0);
			bBound = false;
			*BindingPoint = nullptr;
		}

		bool IsBound() const
		{
			return bBound;
		}

		bool IsInputLayoutCreated() const
		{
			return bInputLayoutCreated;
		}

		void SetInputLayoutCreated()
		{
			bInputLayoutCreated = true;
		}

		// Raw GL names, needed when a second VAO wants to reference this buffer's VBO as one of its
		// own attribute sources (see DrawGouraudProgram's dual-VAO normals setup).
		GLuint GetBufferObjectName() const { return BufferObjectName; }
		GLuint GetVaoObjectName() const { return VaoObjectName; }

		void RebindBufferBase(const GLuint BindingIndex)
		{
			Bind();
			glBindBufferBase(BufferType, BindingIndex, BufferObjectName);
			Unbind();
		}

		//
		// Moves data over to the GPU by reinitializing or updating the backing buffer
		// @ExpectedUsage is the expected usage pattern for the buffer data. We will ignore this value if @Reinitialize is false
		//
		// This function is a no-op if we're using persistent buffers!
		//
		void BufferData(bool Replace)
		{
			const auto UnbufferedRegionOffset = Replace ? 0 : UnbufferedRegionOffsetBytes();
			const auto Size = SizeBytes() - UnbufferedRegionOffset;

			// stijn: the drivers for these platforms can't deal with the way we use glBufferSubData
#if MACOSX || __LINUX_ARM__ || __LINUX_ARM64__
			Replace = true;
#endif

			if (!bPersistentBuffer)
			{
				if (Replace)
					glBufferData(BufferType, Size, Buffer, ExpectedUsage);
				else
				{
					// stijn: we're about to fully overwrite [UnbufferedRegionOffset, UnbufferedRegionOffset+Size),
					// a range we've never written since the last orphan/Rotate(). If we let the driver know that,
					// it can skip synchronizing this glBufferSubData call against GPU reads of this range.
					// This can improve performance on some drivers.
					if (bUseInvalidation && Size > 0)
						glInvalidateBufferSubData(BufferObjectName, UnbufferedRegionOffset, Size);
					glBufferSubData(BufferType, UnbufferedRegionOffset, Size, &Buffer[FirstUnbufferedElemIndex]);
				}
			}
			else
			{
				// stijn: We only need this if we map the buffer with GL_MAP_FLUSH_EXPLICIT_BIT:
				// glFlushMappedNamedBufferRange(BufferObjectName, SubBufferOffsetBytes() + UnbufferedRegionOffset, Size);
				// stijn: And we need this if we allocate/map the buffer without GL_MAP_COHERENT_BIT:
				// glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
			}		

			FirstUnbufferedElemIndex = NextElemIndex;
		}

		// Unmaps and deallocates the buffer
		void DeleteBuffer()
		{
			if (bPersistentBuffer)
			{
				GLint IsMapped;
				glGetNamedBufferParameteriv(BufferObjectName, GL_BUFFER_MAPPED, &IsMapped);
				if (IsMapped == GL_TRUE)
					glUnmapNamedBuffer(BufferObjectName);
			}
			else
			{
				delete[] Buffer;
			}
			
			Buffer = nullptr;

			if (VaoObjectName)
				glDeleteBuffers(1, &VaoObjectName);
			if (BufferObjectName)
				glDeleteBuffers(1, &BufferObjectName);

			delete[] Sync;
			Sync = nullptr;
			bBound = bInputLayoutCreated = false;
			BindingPoint = nullptr;
			NextElemIndex = Index = SubBufferOffset = 0;
			BufferObjectName = VaoObjectName = 0;
		}

		// Inserts a fence that makes the GPU signal the active sub-buffer when
		// it is done with the draw call that uses said buffer
		void Lock()
		{
			if (!bPersistentBuffer)
				return;

			glDeleteSync(Sync[Index]);
			Sync[Index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		// Blocks until the GPU has signaled the active sub-buffer
		void Wait()
		{
			if (!bPersistentBuffer || !Sync[Index])
				return;

			while (1)
			{
				GLenum WaitReturn = glClientWaitSync(Sync[Index], GL_SYNC_FLUSH_COMMANDS_BIT, 1);
				if (WaitReturn == GL_ALREADY_SIGNALED || WaitReturn == GL_CONDITION_SATISFIED)
				{
					return;
				}
				if (WaitReturn == GL_WAIT_FAILED)
				{
					GWarn->Logf(TEXT("XOpenGL: glClientWaitSync[%i] GL_WAIT_FAILED"), Index);
					return;
				}
			}
		}

		GLuint FirstUnbufferedElemIndex{};	// Index of the first buffer element we haven't pushed to the GPU yet (relative to the start of the current sub-buffer)
		GLuint SubBufferOffset{};			// Global index of the first buffer element of the sub-buffer we're currently writing to (relative to the start of the _entire_ buffer)
		GLuint NextElemIndex{};				// Index of the next buffer element we're going to write within the currently active sub-buffer (relative to the start of the sub-buffer)

	private:
		void MapBuffer(GLenum Target, bool Persistent, GLuint BufferSize, GLenum _ExpectedUsage, bool UseInvalidation = false)
		{
			// stijn: NOTE: nvidia persistent buffers seem to be coherent by default!
			constexpr GLbitfield PersistentBufferFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

			SubBufferSize = BufferSize;
			BufferType = Target;
			ExpectedUsage = _ExpectedUsage;

			// Invalidation only makes sense for the non-persistent glBufferSubData path -- persistent
			// buffers are synchronized explicitly via fences instead (see Lock/Wait)
			bUseInvalidation = UseInvalidation && !Persistent;

			// Allocate and pin buffers
			bPersistentBuffer = Persistent;
			if (bPersistentBuffer)
			{
				SubBufferCount = NUMBUFFERS;
				Sync = new GLsync [SubBufferCount];
				memset(Sync, 0, sizeof(GLsync) * SubBufferCount);

				glBindBuffer(Target, BufferObjectName);
				glBufferStorage(Target, SubBufferCount * BufferSize * sizeof(T), nullptr, PersistentBufferFlags);
				Buffer = static_cast<T*>(glMapNamedBufferRange(BufferObjectName, 0, SubBufferCount * BufferSize * sizeof(T), PersistentBufferFlags/* | GL_MAP_FLUSH_EXPLICIT_BIT*/));
				glBindBuffer(Target, 0);
			}
			else
			{
				SubBufferCount = 1;
				const GLuint RealBufferSize = Max(1u, BufferSize);
				Buffer = new T[RealBufferSize];
				memset(Buffer, 0, RealBufferSize * sizeof(T));
				glBindBuffer(Target, BufferObjectName);
				glBufferData(Target, BufferSize * sizeof(T), nullptr, ExpectedUsage);
				glBindBuffer(Target, 0);
			}
		}

		GLsync* Sync{};					// OpenGL sync objects. One for each sub-buffer
		GLuint Index{};					// Index of the sub-buffer we're currently writing to. This will always be 0 if we're not using persistent buffers
		GLuint BufferObjectName{};		// OpenGL name of the buffer object
		GLuint VaoObjectName{};			// (Optional) OpenGL name of the VAO we associated with the buffer
		bool   bPersistentBuffer{};     // true if we persistently map this buffer into system RAM
		bool   bUseInvalidation{};      // true if we should hint the driver that partial updates can discard old contents
		GLenum ExpectedUsage{};			//

		//
		// Buffer dimensions
		//
		GLuint SubBufferSize{};			// Size of each of the sub-buffers that comprise this buffer object (in number of T-sized elements)
		GLuint SubBufferCount{};		// Number of sub-buffers

		bool   bBound{};                // True if currently bound
		bool   bInputLayoutCreated{};   // 
		GLenum BufferType{};            // GL target
		BoundBuffer** BindingPoint{};   // The binding point that needs to be unbound before we can bind this buffer
		GLuint BindingIndex{};
	};
    
	// Layout mandated by the GL spec for glMultiDrawArraysIndirect: 4x GLuint, {count, instanceCount,
	// first, baseInstance}. BaseInstance is always 0 here -- this renderer's per-draw indexing is the
	// emulated DrawID vertex attribute (baked from ParametersBuffer's absolute position at write time),
	// not gl_InstanceID/gl_BaseInstance.
	struct DrawArraysIndirectCommand
	{
		GLuint Count;
		GLuint InstanceCount;
		GLuint First;
		GLuint BaseInstance;
	};
	static_assert(sizeof(DrawArraysIndirectCommand) == 16, "Invalid indirect command size");

	//
	// Helper class for glMultiDrawArrays / glMultiDrawArraysIndirect batching
	//
	class MultiDrawBuffer
	{
	public:
		MultiDrawBuffer()
		{
			FirstArray.AddZeroed(1024);
			CountArray.AddZeroed(1024);
			Capacity = 1024;
		}

		MultiDrawBuffer(INT MaxMultiDraw)
		{
			FirstArray.AddZeroed(MaxMultiDraw);
			CountArray.AddZeroed(MaxMultiDraw);
			Capacity = MaxMultiDraw;
		}

		// @AbsoluteFirstVertex must be the absolute position (SubBufferOffset + NextElemIndex) of
		// the shared VertBuffer at the moment this draw's vertices are about to be written --
		// callers no longer get this for free from internal relative counters, because multiple
		// MultiDrawBuffers can now interleave writes into the same shared VertBuffer/ParametersBuffer
		// (see MultiSpecializationShaderProgramImpl), and a relative counter would go stale the
		// instant another MultiDrawBuffer's writes advanced the shared cursor in between.
		void StartDrawCall(GLint AbsoluteFirstVertex)
		{
			FirstArray(TotalCommands) = AbsoluteFirstVertex;
			if (bUseIndirect)
			{
				auto* Cmd = CommandBuffer.GetCurrentElementPtr();
				Cmd->First = AbsoluteFirstVertex;
				Cmd->InstanceCount = 1;
				Cmd->BaseInstance = 0;
			}
		}

		void EndDrawCall(INT Vertices)
		{
			CountArray(TotalCommands) = Vertices;
			if (bUseIndirect)
			{
				// Same slot StartDrawCall just wrote -- CommandBuffer's cursor hasn't moved yet.
				CommandBuffer.GetCurrentElementPtr()->Count = Vertices;
				CommandBuffer.Advance(1);
			}
			TotalVertices += Vertices;
			TotalCommands++;
		}

		bool IsFull() const { return TotalCommands + 1 >= FirstArray.Num(); }

		void Reset()
		{
			TotalCommands = TotalVertices = 0;
		}

		// Lazily allocates the persistently-mapped GL_DRAW_INDIRECT_BUFFER this MultiDrawBuffer will
		// submit commands from. Mirrors how FirstArray/CountArray are already owned 1:1 by this
		// instance -- each MultiDrawBuffer (the single ShaderProgram::DrawBuffer, or one of the 8
		// pooled SpecializationBatch::DrawBuffers) gets its own small buffer rather than sharing one
		// across a shader, because glMultiDrawArraysIndirect requires its commands to be contiguous,
		// and multiple pooled batches routinely interleave writes into shared buffers otherwise.
		void MapCommandBuffer(UXOpenGLRenderDevice* RenDev)
		{
			if (CommandBuffer.Buffer)
				return;
			CommandBuffer.GenerateIndirectBuffer(RenDev);
			CommandBuffer.MapIndirectBuffer(RenDev->UsingPersistentBuffers, Capacity, DRAWCALL_BUFFER_USAGE_PATTERN, RenDev->UseBufferInvalidation);
			bUseIndirect = true;
		}

		void UnmapCommandBuffer()
		{
			CommandBuffer.DeleteBuffer();
			bUseIndirect = false;
		}

		void Draw(GLenum Mode, UXOpenGLRenderDevice* RenDev)
		{
			if (bUseIndirect)
			{
				CommandBuffer.Bind();
				CommandBuffer.BufferData(false);
				glMultiDrawArraysIndirect(Mode, (const void*)(uintptr_t)CommandBuffer.SubBufferOffsetBytes(),
										   TotalCommands, sizeof(DrawArraysIndirectCommand));
				// Unconditional: TotalCommands resets to 0 on every Draw() (see Reset(), called by our
				// owner right after this), so the next accumulation cycle always starts writing back at
				// element 0 of the currently-active sub-buffer -- we must rotate every single Draw() to
				// avoid overwriting a sub-buffer the GPU may still be reading from this very call.
				CommandBuffer.Lock();
				CommandBuffer.Rotate(true);
			}
			else if (glMultiDrawArrays)
			{
				glMultiDrawArrays(Mode, &FirstArray(0), &CountArray(0), TotalCommands);
			}
			else
			{
				// Fallback for ancient hardware
				for (INT i = 0; i < TotalCommands; ++i)
					glDrawArrays(Mode, FirstArray(i), CountArray(i));
			}
		}

		void Cleanup()
		{
			// No buffer cleanup needed!
		}

	public:
		// OpenGL strictly expects GLint and GLsizei for glMultiDrawArrays
		TArray<GLint>   FirstArray;
		TArray<GLsizei> CountArray;

		INT TotalVertices{};
		INT TotalCommands{};

		BufferObject<DrawArraysIndirectCommand> CommandBuffer;
		GLuint Capacity{};      // == FirstArray.Num(), remembered for MapCommandBuffer
		bool   bUseIndirect{};  // cached once, in MapCommandBuffer()
	};

	//
	// Shaders
	//
	enum ShaderProgType
	{
		No_Prog,
		Simple_Triangle_Prog,
        Simple_Line_Prog,
		Tile_Prog,
		PostProcess_Prog,
		Gouraud_Prog,
		Complex_Prog,
		Max_Prog,
	};

	class ShaderDrawFlags
	{
	public:
		enum
		{
			DF_None				= 0x0000,

			// Various types of textures we can use in a shader
			DF_DiffuseTexture	= 0x0001,
			DF_LightMap			= 0x0002,
			DF_FogMap			= 0x0004,
			DF_DetailTexture	= 0x0008,
			DF_MacroTexture		= 0x0010,
			DF_BumpMap			= 0x0020,
			DF_EnvironmentMap	= 0x0040,
			DF_HeightMap		= 0x0080,

			// PolyFlags the shader needs to know about
			DF_Masked			= 0x0100,
			DF_Unlit			= 0x0200,
			DF_Modulated		= 0x0400,
			DF_Translucent		= 0x0800,
			DF_Environment		= 0x1000,
			DF_RenderFog		= 0x2000,
			DF_AlphaBlended		= 0x4000,

			// Per-draw call editor state the shader needs to know about
			DF_Selected			= 0x8000
		};
	};
    
    class ShaderCompilationOptions
    {
		public:
        enum
        {
            OPT_None				 = 0x00000000,

			// Texture types enabled in the renderer config
			OPT_DetailTextures       = 0x00000001,
			OPT_MacroTextures        = 0x00000002,
			OPT_EnvironmentMaps		 = 0x00000004,
			OPT_BumpMaps			 = 0x00000008,
			OPT_HeightMaps			 = 0x00000010,

			// Features enabled in the renderer config
			OPT_DistanceFog			 = 0x00000020,
			OPT_SimulateMultiPass    = 0x00000040,
			OPT_HWLighting           = 0x00000080,

			// Hardware/driver capabilities we're using
			OPT_GLCore               = 0x00000100,
			OPT_GLES                 = 0x00000200,
			OPT_GeometryShaders      = 0x00000400,
			OPT_BindlessTextures     = 0x00000800,
			OPT_PersistentBuffers    = 0x00001000,
			OPT_ShaderDrawParameters = 0x00002000,
			OPT_ClipDistance         = 0x00004000,

			// Enabled editor-specific code
			OPT_Editor				 = 0x00008000,

			// Per-drawcall texture types we're actually going to use
			// in the shader. By specializing the shader for these,
			// we can avoid branching on non-dynamically uniform expressions.
			// This is a huge deal for performance on most drivers.
			OPT_HasLightMap			 = 0x00010000,
			OPT_HasFogMap			 = 0x00020000,
			OPT_HasDetailTexture	 = 0x00040000,
			OPT_HasMacroTexture		 = 0x00080000,
			OPT_HasBumpMap			 = 0x00100000,
			OPT_HasEnvironmentMap	 = 0x00200000,
			OPT_HasHeightMap		 = 0x00400000,

			// Per-drawcall poly-flag-derived render modes we're actually going to use in the shader.
			// Same idea as the OPT_HasXXX texture options above, just for the blend/masking logic
			// instead of texture layers. Only defined for flags we've verified are actually branched
			// on somewhere in the shader source.
			OPT_IsMasked			 = 0x00800000,
			OPT_IsAlphaBlended		 = 0x01000000,
			OPT_IsModulated			 = 0x02000000,
			OPT_IsTranslucent		 = 0x04000000,
			OPT_IsRenderFog			 = 0x08000000,
			OPT_IsUnlit				 = 0x10000000,

			// Set when OPT_HasXXX texture-layer bits above were force-set beyond what this
			// draw's own surface data implies (uber-shader specializations for translucent
			// draws, see DrawComplexSurface/PrepareGouraudCall). Tells the shader to guard
			// each texture-layer block with a runtime check of the per-draw DrawFlags instead
			// of trusting the compile-time OPT_HasXXX bit.
			OPT_RuntimeTextureLayers = 0x20000000
        };

		ShaderCompilationOptions(DWORD ShaderOptions)
		{
			OptionsMask = ShaderOptions;
		}
		ShaderCompilationOptions() {}
        
        FString GetShortString() const;
        FString GetPreprocessorString() const;

		void SetOptionsForRendererConfig(UXOpenGLRenderDevice* RenDev);
        void SetOption(DWORD Option);
        void UnsetOption(DWORD Option);
        bool HasOption(DWORD Option) const;
        DWORD GetMask() const { return OptionsMask; }

        friend DWORD GetTypeHash(const ShaderCompilationOptions& Options)
        {
            return Options.OptionsMask;
        }
		
		bool operator==(const DWORD Options)
		{
			return OptionsMask == Options;
		}

		bool operator==(const ShaderCompilationOptions& Options)
		{
			return OptionsMask == Options.OptionsMask;
		}

		ShaderCompilationOptions operator&(const ShaderCompilationOptions& Options)
        {
			return ShaderCompilationOptions(Options.OptionsMask & OptionsMask);
        }
        
    private:
        FString GetStringHelper(void (*AddOptionFunc)(FString&, const TCHAR*, bool)) const;
        DWORD OptionsMask{};
    };
    
    class CompiledShader
    {
    public:
		ShaderCompilationOptions Options{};
        GLuint VertexShaderObject{};
        GLuint GeoShaderObject{};
        GLuint FragmentShaderObject{};
        GLuint ShaderProgramObject{};
        FString ShaderName;
    };

	// Common interface for all shaders
	class ShaderProgram
	{
	public:

		struct DrawCallParameterInfo
		{
			const char* Type;
			const char* Name;
			const int ArrayCount;
		};

		typedef void (ShaderWriterFunc)(GLuint, class UXOpenGLRenderDevice*, FShaderWriterX&);

		// Allow recompiling shaders dynamically when renderer options change
        CompiledShader*								CurrentSpecialization{};

		// Which options can prompt a recompilation of this shader?
		ShaderCompilationOptions					RelevantSpecializationOptions;

		// Every specialization we've compiled or loaded for this shader so far.
		// We use the specialization options (see the ShaderCompilationOptions enum above)
		// as the key.
		TOpenGLMap<DWORD, CompiledShader*>			SpecializationCache;

		// Lets each shader's draw path (DrawComplexSurface, PrepareGouraudCall, DrawTile, ...) skip
		// rebuilding its RequiredOptions from scratch on every draw call. A cache hit requires BOTH:
		//   - LastPerDrawSignature: a cheap DWORD fingerprint of the per-draw inputs (which texture
		//     layers are present on this surface/mesh/tile, and which poly-flag-derived render modes
		//     apply) that determine the per-draw bits of the specialization.
		//   - LastRendererConfigOptions: the renderer-config-derived subset of the specialization's
		//     Options -- i.e. CurrentSpecialization->Options with the per-draw bits masked out. This
		//     only changes when RecompileShader runs (see SetOptionsForRendererConfig), never as a
		//     side effect of switching specializations for per-draw reasons.
		// On a hit, LastResolvedOptions (the full renderer-config + per-draw Options we computed last
		// time) is reused directly instead of being rebuilt.
		DWORD										LastPerDrawSignature{ 0xFFFFFFFFu };
		ShaderCompilationOptions					LastRendererConfigOptions{ 0xFFFFFFFFu };
		ShaderCompilationOptions					LastResolvedOptions;

		MultiDrawBuffer								DrawBuffer;
		const TCHAR*                                ShaderName{};
		UXOpenGLRenderDevice*                       RenDev{};
        
        // Parameters to be specified by the shader constructor
        INT                                         VertexBufferSize;
        INT                                         ParametersBufferSize;
        INT                                         ParametersBufferBindingIndex;
        INT                                         NumTextureSamplers;
        GLenum                                      DrawMode;
		BOOL										UseSSBOParametersBuffer;
		const DrawCallParameterInfo*				ParametersInfo;		
		ShaderWriterFunc*							VertexShaderFunc;
		ShaderWriterFunc*							GeoShaderFunc;
		ShaderWriterFunc*							FragmentShaderFunc;

		virtual ~ShaderProgram();

		//
		// State support
		//

		// Binds the uniform with the specified @Name to the binding point with index @BindingIndex in compiled shader program @ProgramObject
		void BindUniform(CompiledShader* Specialization, const GLuint BindingIndex, const char* Name) const;
		void GetUniformLocation(CompiledShader* Specialization, GLint& Uniform, const char* Name) const;

		// Binds shader-specific state such as uniforms
		virtual void BindShaderState(CompiledShader* Specialization);

		// Switches to the specified shader, possibly creating/compiling it on-the-fly
		virtual void UseShader();

		// Recompile/respecialize the shader after the renderer options change
		void RecompileShader(ShaderCompilationOptions Options);

		// Makes the specialization matching @Options current, compiling and caching it first if we
		// haven't built it before. Returns true if this actually changed the active specialization
		// (i.e., callers that need to flush pending batched draws on a program switch should check this).
		bool SelectSpecialization(ShaderCompilationOptions Options);

		// Looks up (or lazily compiles and caches) the specialization matching @Options, WITHOUT
		// making it current -- no CurrentSpecialization/glUseProgram side effect on a cache hit.
		// A fresh compile still needs a transient glUseProgram+BindShaderState (BindShaderState's
		// uniform/sampler calls aren't DSA and implicitly target whatever's currently bound), but
		// that only happens once per specialization per session, not on every lookup. Used by
		// MultiSpecializationShaderProgramImpl to resolve a specialization for buffering without
		// switching the GL program away from whatever's actually being drawn right now. Returns
		// nullptr if this shader has no vertex/fragment shader functions to build with.
		CompiledShader* GetOrBuildSpecialization(ShaderCompilationOptions Options);

		//
		// Compilation support
		//

		// Emit the shared header for a GLSL shader
		void EmitGlobals(ShaderCompilationOptions Options, GLuint ShaderType, UXOpenGLRenderDevice* RenDev, FShaderWriterX& Out, bool HaveGeoShader);

		// Compiles one shader function		
		bool CompileShaderFunction(GLuint ShaderFunctionObject, GLuint FunctionType, ShaderCompilationOptions Options, ShaderWriterFunc Func, bool HaveGeoShader=false);

		// Compiles and links an entire shader program
		// @GeoShaderFunc may be nullptr
		bool BuildShaderProgram(CompiledShader* Shader, ShaderWriterFunc VertexShaderFunc, ShaderWriterFunc GeoShaderFunc, ShaderWriterFunc FragmentShaderFunc);

		// Links the shader program after compiling all of its functions
		bool LinkShaderProgram(GLuint ShaderProgramObject) const;

		// Detaches and deletes the GL program object (and its attached shader stages) for @Shader.
		// Does not delete @Shader itself -- the caller (usually ClearSpecializationCache) owns that.
		void DeleteCompiledShader(CompiledShader* Shader);

		// Deletes every specialization we've compiled (including CurrentSpecialization) and empties
		// the cache. Called when we tear down the shader entirely, or when renderer
		// options change and invalidate every cached variant.
		void ClearSpecializationCache();

		// Used to describe the layout of the drawcall parameters		
		static void EmitDrawCallParametersHeader(const DrawCallParameterInfo* Info, FShaderWriterX& Out, ShaderProgram* Program, INT BufferBindingIndex, bool UseSSBO, bool EmitGetters);

		// Calculates the maximum size we could use for a uniform struct array whose layout is specified by @Info
		INT GetMaximumUniformBufferSize(const DrawCallParameterInfo* Info) const;

		//
		// Specialization-agnostic functionality
		//

		// Allocates and binds the vertex and/or drawcall parameter buffers this shader needs
		virtual void MapBuffers() = 0;

		// Unbinds and deallocates the vertex and/or drawcall parameter buffers this shader needs
		virtual void UnmapBuffers() = 0;		

		// Binds the input layout for our vertex and/or drawcall parameter buffers
		virtual void CreateInputLayout() = 0;

		// Switches to this shader and sets global GL state if necessary
		virtual void ActivateShader() = 0;

		// Switches away from this shader. This is where we should flush any leftover buffered data and restore global GL state if necessary
		virtual void DeactivateShader() = 0;

		// Dispatches buffered data. If @Rotate is true, we switch to a different (part of a) vertex and parameters buffer before returning
		virtual void Flush(bool Rotate = false) = 0;

		// Does this program have any buffered-but-undrawn data pending right now? Default is "my
		// single DrawBuffer has commands in it"; MultiSpecializationShaderProgramImpl overrides
		// this to check its whole pool of pending specialization batches instead.
		virtual bool HasPendingData() const { return DrawBuffer.TotalCommands > 0; }
	};

	// Base class for shader implementations
    template
    <
        typename VertexType,
        typename DrawCallParamsType
    >
		class ShaderProgramImpl : public ShaderProgram
	{
	public:
		ShaderProgramImpl(const TCHAR* Name, UXOpenGLRenderDevice* _RenDev)
		{
			ShaderName = Name;
			RenDev = _RenDev;
		}

		virtual ~ShaderProgramImpl()
		{
			ClearSpecializationCache();
			UnmapBuffers();
		}

		virtual void Flush(bool Rotate)
		{
			const auto HavePendingData = DrawBuffer.TotalCommands > 0;

			if (!HavePendingData && !Rotate)
				return;

            // stijn: since we always replace the entire buffer (with glBufferData), it is better to just rotate after every flush on these platforms
#if MACOSX || __LINUX_ARM__ || __LINUX_ARM64__
			Rotate = true;
#endif

			if (Rotate)
			{
				// Back up the parameters of the last draw call so we can write them into the first
				// slot of the parameters buffer after rotating
				auto In = ParametersBuffer.GetElementPtr(static_cast<GLuint>((ParametersBuffer.Size() > 0) ? (ParametersBuffer.Size() - 1) : 0));
				memcpy(&DrawCallParams, In, sizeof(DrawCallParamsType));
			}

			// We might have to rebind the parameters buffer here because things like PushClipPlane and
			// PopClipPlane can temporarily bind another UBO.
			ParametersBuffer.Bind();

			if (HavePendingData)
			{
				VertBuffer.BufferData(false);

				ParametersBuffer.BufferData(false);

				// Issue the draw call
				DrawBuffer.Draw(DrawMode, RenDev);
			}

			if (Rotate)
			{
				// One shared fence pair for both buffers -- they only ever rotate together, so a
				// single fence (which marks a point in the command stream, not a specific buffer)
				// covers both instead of each maintaining its own Sync[]/Lock()/Wait() cycle.
				LockRotation(ParametersBuffer.GetIndex());   // pre-rotation index, same for both buffers

				ParametersBuffer.Rotate();                   // position math only, no wait
				VertBuffer.Rotate();                         // position math only, no wait

				// MUST happen before any write into the freshly-rotated sub-buffers below.
				WaitRotation(VertBuffer.GetIndex());         // post-rotation index, same for both buffers

				// Make sure the new parameters buffer starts with the drawcall parameters of the
				// call latest drawcall
				auto Out = ParametersBuffer.GetCurrentElementPtr();
				memcpy(Out, &DrawCallParams, sizeof(DrawCallParamsType));
			}

			// Reset the multidraw buffer. Absolute buffer positions for the next draw calls are
			// computed fresh at write time (see MultiDrawBuffer::StartDrawCall), so there's no
			// baseline to recompute here even though Rotate() above might have switched to an
			// unused part of the same SSBO/VBO we were already using.
			DrawBuffer.Reset();
		}

		// Inserts one fence covering every buffer (VertBuffer/ParametersBuffer, and for the pooled
		// subclass CommandBuffer/NormalsBuffer) that rotates together in the same Flush(true) call --
		// a GL fence marks a point in the command stream, not a specific buffer, so one fence per
		// rotation event is equivalent to the N independent ones it replaces.
		void LockRotation(GLuint Index)
		{
			if (!RotationSync)
				return;
			glDeleteSync(RotationSync[Index]);
			RotationSync[Index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		// Blocks until the GPU has signaled sub-buffer slot @Index is free for the CPU to reuse.
		// MUST be called before writing anything new into that slot in any of the lockstep buffers.
		void WaitRotation(GLuint Index)
		{
			if (!RotationSync || !RotationSync[Index])
				return;
			while (1)
			{
				GLenum WaitReturn = glClientWaitSync(RotationSync[Index], GL_SYNC_FLUSH_COMMANDS_BIT, 1);
				if (WaitReturn == GL_ALREADY_SIGNALED || WaitReturn == GL_CONDITION_SATISFIED)
					return;
				if (WaitReturn == GL_WAIT_FAILED)
				{
					GWarn->Logf(TEXT("XOpenGL: glClientWaitSync[%i] GL_WAIT_FAILED"), Index);
					return;
				}
			}
		}

		virtual void ActivateShader()
		{
			WaitRotation(VertBuffer.GetIndex());
			VertBuffer.Bind();
			ParametersBuffer.Bind();
			UseShader();
		}

		virtual void DeactivateShader()
		{
			Flush(false);
		}

		virtual void MapBuffers()
		{
			if (!VertBuffer.Buffer)
			{
				VertBuffer.GenerateVertexBuffer(RenDev);
				VertBuffer.MapVertexBuffer(RenDev->UsingPersistentBuffers, VertexBufferSize, RenDev->UseBufferInvalidation);
				VertBuffer.Bind();
				CreateInputLayout();

				if (RenDev->UsingPersistentBuffers)
				{
					RotationSync = new GLsync[NUMBUFFERS];
					memset(RotationSync, 0, sizeof(GLsync) * NUMBUFFERS);
				}
			}

			if (!ParametersBuffer.Buffer)
			{
				if (UseSSBOParametersBuffer)
				{
					ParametersBufferSize = Min<INT>(ParametersBufferSize, (RenDev->MaxSSBOBlockSize / sizeof(DrawCallParams) / (RenDev->UsingPersistentBuffers ? NUMBUFFERS : 1)));
					ParametersBuffer.GenerateSSBOBuffer(RenDev, ParametersBufferBindingIndex);
					ParametersBuffer.MapSSBOBuffer(RenDev->UsingPersistentBuffers, ParametersBufferSize, DRAWCALL_BUFFER_USAGE_PATTERN, RenDev->UseBufferInvalidation);
				}
				else
				{
					ParametersBufferSize = Min<INT>(ParametersBufferSize, GetMaximumUniformBufferSize(ParametersInfo) / (RenDev->UsingPersistentBuffers ? NUMBUFFERS : 1));
					ParametersBuffer.GenerateUBOBuffer(RenDev, ParametersBufferBindingIndex);
					ParametersBuffer.MapUBOBuffer(RenDev->UsingPersistentBuffers, ParametersBufferSize, DRAWCALL_BUFFER_USAGE_PATTERN, RenDev->UseBufferInvalidation);
				}
			}

			if (RenDev->UsingIndirectDraw)
				DrawBuffer.MapCommandBuffer(RenDev);
		}

		virtual void UnmapBuffers()
		{
			if (RenDev->UsingIndirectDraw)
				DrawBuffer.UnmapCommandBuffer();
			VertBuffer.DeleteBuffer();
			ParametersBuffer.DeleteBuffer();

			delete[] RotationSync;
			RotationSync = nullptr;
		}

		// Templated member data
		DrawCallParamsType                          DrawCallParams;
		BufferObject<DrawCallParamsType>            ParametersBuffer;
		BufferObject<VertexType>                    VertBuffer;
		GLsync*                                     RotationSync{};   // shared fence array; one per NUMBUFFERS slot
	};

	// Extends ShaderProgramImpl to let ONE shader accumulate pending draws for MULTIPLE
	// specializations (different compiled GLSL variants of the same shader, resolved via
	// GetOrBuildSpecialization) at once, all writing into the same shared VertBuffer/
	// ParametersBuffer -- vertex format and DrawCallParameters layout are identical across every
	// specialization of a given shader (see EmitDrawCallParametersHeader), so this is safe. Used
	// by DrawComplexProgram/DrawGouraudProgram so a run of draws that keeps switching between a
	// handful of specializations (e.g. lightmapped vs. non-lightmapped BSP faces) doesn't force a
	// flush on every switch -- only when something (blend state, non-opaque draw order, capacity)
	// actually requires it. See DrawComplexSurface/PrepareGouraudCall for the call-site logic.
	template
	<
		typename VertexType,
		typename DrawCallParamsType
	>
		class MultiSpecializationShaderProgramImpl : public ShaderProgramImpl<VertexType, DrawCallParamsType>
	{
	public:
		using Base = ShaderProgramImpl<VertexType, DrawCallParamsType>;

		MultiSpecializationShaderProgramImpl(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
			: Base(Name, RenDev)
		{
		}

		// A specialization's pending, not-yet-drawn draw calls. Multiple of these can be alive at
		// once, all referencing the same shared VertBuffer/ParametersBuffer -- only the {First,
		// Count} command list is per-batch.
		struct SpecializationBatch
		{
			CompiledShader* Specialization{};
			// Deliberately much smaller than MultiDrawBuffer's 1024-entry default: this cap exists
			// so a single specialization being drawn a lot in a row still gets flushed sometimes
			// (freeing GPU-visible memory pressure and giving other pending batches a chance to
			// draw), not because we expect it to matter for typical scenes.
			MultiDrawBuffer DrawBuffer{ 1024 };
		};

		static constexpr INT PoolSize = 8;
		SpecializationBatch PendingBatches[PoolSize];

		// The batch most recently selected for writing. Mirrors how CurrentSpecialization already
		// works as a member rather than a threaded-through return value. IMPORTANT: any code path
		// that calls Flush() must call SelectBatch() again afterward before writing more data --
		// Flush() clears every batch's Specialization field, so ActiveBatch after a flush points
		// at valid but logically-empty memory, not the batch the caller thinks it's still writing.
		SpecializationBatch* ActiveBatch{};

		// True if some batch OTHER than @Except currently has pending (undrawn) draws. Used to
		// decide whether a non-opaque draw can safely append to its own batch without flushing
		// first -- see the design note on SelectBatch below.
		bool HasOtherPendingBatch(SpecializationBatch* Except) const
		{
			for (auto& B : PendingBatches)
				if (&B != Except && B.DrawBuffer.TotalCommands > 0)
					return true;
			return false;
		}

		// Finds (or creates) the batch for @Specialization and makes it ActiveBatch. If every pool
		// slot is already claimed by a different specialization, evicts whichever one has the
		// fewest pending draws (cheapest to give up) via a narrow single-batch drain, rather than
		// flushing every other batch's accumulated work too.
		//
		// Note this does NOT by itself guarantee draw-order safety for non-opaque draws: Flush()
		// draws each pending batch in pool-slot order, not submission order. That's invisible for
		// opaque/depth-tested geometry (whichever of two opaque draws is genuinely closer wins the
		// depth test regardless of which one the GPU rasterizes first), but two real hazards exist
		// for non-opaque (translucent) draws:
		//   1. Two non-opaque batches for different specializations both pending at once could have
		//      their relative blending order scrambled.
		//   2. A non-opaque draw's own depth test needs every OPAQUE draw submitted before it to
		//      already be depth-written -- translucent draws don't write depth, so opaque draws
		//      submitted after it are unaffected regardless of when they actually run, but opaque
		//      draws submitted before it are NOT safe to leave pending.
		// Both hazards reduce to the same rule: a non-opaque draw must flush everything (via
		// HasOtherPendingBatch) before it's safe to just append to its own batch. See
		// DrawComplexSurface/PrepareGouraudCall for where this is applied.
		SpecializationBatch* SelectBatch(CompiledShader* Specialization)
		{
			for (auto& B : PendingBatches)
				if (B.Specialization == Specialization)
					return ActiveBatch = &B;

			for (auto& B : PendingBatches)
				if (!B.Specialization)
				{
					B.Specialization = Specialization;
					return ActiveBatch = &B;
				}

			SpecializationBatch* Victim = &PendingBatches[0];
			for (auto& B : PendingBatches)
				if (B.DrawBuffer.TotalCommands < Victim->DrawBuffer.TotalCommands)
					Victim = &B;

			FlushOneBatch(Victim);
			Victim->Specialization = Specialization;
			return ActiveBatch = Victim;
		}

		virtual bool HasPendingData() const override
		{
			for (auto& B : PendingBatches)
				if (B.DrawBuffer.TotalCommands > 0)
					return true;
			return false;
		}

		// Deliberately does nothing. The base ActivateShader() (Wait/Bind/UseShader) exists so a
		// freshly-switched-to program is ready to draw immediately -- but for us, "ready to draw"
		// isn't decided at switch time at all: CurrentSpecialization is just a frozen renderer-
		// config marker (see GetOrBuildSpecialization/RecompileShader), never the specialization
		// we're actually about to use, so calling UseShader() here would always bind a program
		// object that's about to be superseded. Flush() (above) binds VertBuffer/ParametersBuffer
		// itself right before it needs them, and glUseProgram's each pending batch's own
		// specialization individually -- so there's nothing useful left for this to do. This is
		// what stops SetProgram switches and sibling flushes (FlushInactiveBatchedProgram) from
		// emitting a glUseProgram/rebind that's immediately thrown away without a draw ever
		// happening.
		virtual void ActivateShader() override
		{
		}

		// Shared indirect-draw command buffer for every pooled batch. Deliberately NOT one per
		// SpecializationBatch (unlike the base ShaderProgramImpl::DrawBuffer, which is fine owning its
		// own -- it's only ever Draw()n once per Flush()). Flush()'s loop can draw several batches per
		// call, and giving each its own persistently-mapped buffer meant each one separately paid a
		// glFenceSync/glClientWaitSync pair on every single Draw() -- confirmed via an NVIDIA Nsight
		// capture showing that triplet after nearly every glMultiDrawArraysIndirect call, instead of
		// once per Flush() like VertBuffer/ParametersBuffer. See DrawBatchIndirect() below for how a
		// shared buffer stays safe despite batches never writing in submission order.
		BufferObject<DrawArraysIndirectCommand> CommandBuffer;

		// Base ShaderProgramImpl::MapBuffers()/UnmapBuffers() know nothing about PendingBatches, so we
		// need our own override to map/unmap the shared indirect-draw command buffer too. Sized to
		// ParametersBufferSize (already finalized by Base::MapBuffers()): one indirect command
		// corresponds to exactly one draw call, exactly like one ParametersBuffer slot does, so reusing
		// that number gives CommandBuffer the same safe epoch ParametersBuffer already has, with no
		// extra overflow risk.
		virtual void MapBuffers() override
		{
			Base::MapBuffers();
			if (this->RenDev->UsingIndirectDraw && !CommandBuffer.Buffer)
			{
				CommandBuffer.GenerateIndirectBuffer(this->RenDev);
				CommandBuffer.MapIndirectBuffer(this->RenDev->UsingPersistentBuffers, static_cast<GLuint>(this->ParametersBufferSize),
												 DRAWCALL_BUFFER_USAGE_PATTERN, this->RenDev->UseBufferInvalidation);
			}
		}

		virtual void UnmapBuffers() override
		{
			if (this->RenDev->UsingIndirectDraw)
				CommandBuffer.DeleteBuffer();
			Base::UnmapBuffers();
		}

		// No-op hooks for a derived class that needs to keep a second vertex-attribute buffer (with
		// its own VAO) in lockstep with VertBuffer -- see DrawGouraudProgram's packed-normals setup.
		// Called at the exact points where VertBuffer itself is bound/uploaded/rotated, so a derived
		// override never has to duplicate this class's binding/rotation logic.
		virtual void OnVertBufferBound() {}
		virtual void OnVertBufferUploaded() {}
		virtual void OnVertBufferRotated() {}

		virtual void Flush(bool Rotate) override
		{
			const bool HavePendingData = HasPendingData();

			if (!HavePendingData && !Rotate)
				return;

			// stijn: since we always replace the entire buffer (with glBufferData), it is better to just rotate after every flush on these platforms
#if MACOSX || __LINUX_ARM__ || __LINUX_ARM64__
			Rotate = true;
#endif

			if (Rotate)
			{
				// Back up the parameters of the last draw call so we can write them into the first
				// slot of the parameters buffer after rotating
				auto In = this->ParametersBuffer.GetElementPtr(static_cast<GLuint>((this->ParametersBuffer.Size() > 0) ? (this->ParametersBuffer.Size() - 1) : 0));
				memcpy(&this->DrawCallParams, In, sizeof(DrawCallParamsType));
			}

			// Bind our own buffers unconditionally: ActivateShader() is a no-op for us (see below),
			// and a sibling program's own Flush() may have rebound its VAO/UBO/SSBO in between our
			// last write and this call, so we can't assume ours are still current.
			this->VertBuffer.Bind();
			this->ParametersBuffer.Bind();

			const bool UseIndirect = this->RenDev->UsingIndirectDraw;

			if (HavePendingData)
			{
				// MUST run before OnVertBufferBound(): for DrawGouraudProgram, that hook may call
				// NormalsBuffer.Bind(), which -- via the shared ArrayPoint binding-point protocol --
				// steals GL_ARRAY_BUFFER away from VertBuffer to select VAO B. BufferData()'s
				// non-persistent-buffer glBufferSubData/glBufferData calls trust whatever is
				// currently bound with no bBound check of their own, so uploading VertBuffer's own
				// data has to happen while VertBuffer itself is still the bound GL_ARRAY_BUFFER.
				this->VertBuffer.BufferData(false);
				this->ParametersBuffer.BufferData(false);
			}

			// Now safe to let a derived class switch to a sibling buffer/VAO (e.g. DrawGouraudProgram
			// selecting VAO B for real per-vertex normals) -- our own uploads above are already done.
			OnVertBufferBound();
			if (UseIndirect)
				CommandBuffer.Bind();

			if (HavePendingData)
			{
				OnVertBufferUploaded();

				// Issue one draw call per pending specialization batch. Nothing needs the GL
				// program restored to CurrentSpecialization afterward -- ActivateShader() is a
				// no-op for us, and GetOrBuildSpecialization/SelectBatch don't depend on any
				// particular program being current either -- so we just leave whatever the last
				// batch drawn with is bound.
				for (auto& B : PendingBatches)
				{
					if (B.DrawBuffer.TotalCommands == 0)
						continue;

					glUseProgram(B.Specialization->ShaderProgramObject);
					if (UseIndirect)
						DrawBatchIndirect(B);
					else
						B.DrawBuffer.Draw(this->DrawMode, this->RenDev);
				}

				if (UseIndirect)
					CommandBuffer.BufferData(false);
			}

			if (Rotate)
			{
				// Re-bind: OnVertBufferBound() above may have switched GL_ARRAY_BUFFER away from
				// VertBuffer (e.g. to NormalsBuffer) to select the drawing VAO -- Rotate()'s
				// non-persistent-buffer glBufferData orphan call needs VertBuffer/ParametersBuffer
				// actually bound again first, for the same reason the upload above does.
				this->VertBuffer.Bind();
				this->ParametersBuffer.Bind();

				// One shared fence pair for every buffer that rotates here -- they only ever
				// rotate together within a single Flush(true) call (VertBuffer/ParametersBuffer
				// always, CommandBuffer when UsingIndirectDraw, NormalsBuffer via the
				// OnVertBufferRotated() hook when DrawGouraudProgram needs it) -- so one fence
				// (which marks a point in the command stream, not a specific buffer) covers all
				// of them instead of each maintaining its own Sync[]/Lock()/Wait() cycle.
				this->LockRotation(this->ParametersBuffer.GetIndex());

				this->ParametersBuffer.Rotate();
				this->VertBuffer.Rotate();
				OnVertBufferRotated();
				if (UseIndirect)
					CommandBuffer.Rotate();

				// MUST happen before any write into the freshly-rotated sub-buffers below.
				this->WaitRotation(this->VertBuffer.GetIndex());

				// Make sure the new parameters buffer starts with the drawcall parameters of the
				// call latest drawcall
				auto Out = this->ParametersBuffer.GetCurrentElementPtr();
				memcpy(Out, &this->DrawCallParams, sizeof(DrawCallParamsType));
			}

			// Every batch's data has either just been drawn or belonged to an empty slot -- free
			// them all so the pool is ready for whatever specializations come next.
			for (auto& B : PendingBatches)
			{
				B.DrawBuffer.Reset();
				B.Specialization = nullptr;
			}
			ActiveBatch = nullptr;
		}

	private:
		// Writes @B's already-accumulated {First,Count} pairs into the shared CommandBuffer at its
		// current (contiguous) cursor position and issues one glMultiDrawArraysIndirect referencing
		// that range. Deferring the write to here (instead of writing incrementally at
		// StartDrawCall/EndDrawCall time, like the base ShaderProgramImpl::DrawBuffer does) is what
		// lets every batch drawn in one Flush()/FlushOneBatch() share ONE persistent buffer instead of
		// each owning its own -- nothing else writes to CommandBuffer between the moment we record
		// @Start and the moment we finish this batch's commands, so this range stays contiguous even
		// though different batches' StartDrawCall/EndDrawCall writes (into FirstArray/CountArray) are
		// themselves interleaved with each other over the course of accumulating this Flush.
		void DrawBatchIndirect(SpecializationBatch& B)
		{
			const GLuint Start = CommandBuffer.CurrentAbsolutePosition();
			for (INT i = 0; i < B.DrawBuffer.TotalCommands; ++i)
			{
				auto* Cmd = CommandBuffer.GetCurrentElementPtr();
				Cmd->First = B.DrawBuffer.FirstArray(i);
				Cmd->Count = B.DrawBuffer.CountArray(i);
				Cmd->InstanceCount = 1;
				Cmd->BaseInstance = 0;
				CommandBuffer.Advance(1);
			}
			glMultiDrawArraysIndirect(this->DrawMode, (const void*)(uintptr_t)(GLuint)(Start * sizeof(DrawArraysIndirectCommand)),
									   B.DrawBuffer.TotalCommands, sizeof(DrawArraysIndirectCommand));
		}

		// Drains one specific batch's own pending draws (its {First,Count} list only) without
		// touching the shared VertBuffer/ParametersBuffer's rotation -- used when the pool is full
		// and we need to free up exactly one slot, not when the shared buffers themselves are out
		// of room (that's handled by the normal Flush(true) path). Mirrors that: never calls
		// CommandBuffer.Lock()/Rotate() either, exactly like it never rotates VertBuffer/
		// ParametersBuffer -- Advance() always lands on a fresh slot in the current sub-buffer
		// regardless of whether it's reached via this path or the main Flush() loop, so only a real
		// Flush(true) needs to actually rotate anything.
		void FlushOneBatch(SpecializationBatch* Batch)
		{
			if (Batch->DrawBuffer.TotalCommands == 0)
			{
				Batch->Specialization = nullptr;
				return;
			}

			// Bind our own buffers unconditionally -- see the equivalent comment in Flush(). Upload
			// BEFORE OnVertBufferBound() -- see the equivalent comment in Flush() for why (that hook
			// may steal GL_ARRAY_BUFFER away from VertBuffer via NormalsBuffer.Bind(), and
			// BufferData()'s non-persistent-buffer path trusts whatever is currently bound).
			this->VertBuffer.Bind();
			this->ParametersBuffer.Bind();
			this->VertBuffer.BufferData(false);
			this->ParametersBuffer.BufferData(false);
			OnVertBufferBound();
			OnVertBufferUploaded();

			glUseProgram(Batch->Specialization->ShaderProgramObject);
			if (this->RenDev->UsingIndirectDraw)
			{
				CommandBuffer.Bind();
				DrawBatchIndirect(*Batch);
				CommandBuffer.BufferData(false);
			}
			else
			{
				Batch->DrawBuffer.Draw(this->DrawMode, this->RenDev);
			}

			Batch->DrawBuffer.Reset();
			Batch->Specialization = nullptr;
		}
	};

	ShaderProgram* Shaders[Max_Prog]{};
	void ResetShaders();
	void RecompileShaders();
	void InitShaders();

	// Saves all shaders we've compiled to a cache file so we can load them quickly next time
	UBOOL SaveShaderCache();

	// Loads all shaders from the cache file
	UBOOL LoadShaderCache();
	INT PrevProgram;
	INT ActiveProgram;

	//
	// Global Shader State
	//
	enum GlobalShaderBindingIndices
	{
		FrameStateIndex					= 1,
		LightInfoIndex					= 2,
		ClipPlaneIndex					= 3,
		EditorStateIndex				= 4,
		TileParametersIndex				= 5,
		ComplexParametersIndex			= 6,
		GouraudParametersIndex			= 7,
		SimpleLineParametersIndex		= 8,
		SimpleTriangleParametersIndex	= 9,
		DistanceFogInfoIndex			= 10
	};

	enum TextureIndices
	{
		DiffuseTextureIndex		= 0,
		LightMapIndex			= 1,
		FogMapIndex				= 2,
		DetailTextureIndex		= 3,
		MacroTextureIndex		= 4,
		BumpMapIndex			= 5,
		EnvironmentMapIndex		= 6,
		HeightMapIndex			= 7,
		UploadIndex				= 8
	};

	// Per-frame state
	struct FrameState
	{
		glm::mat4 projMat;
		glm::mat4 viewMat;
		glm::mat4 modelMat;
		glm::mat4 modelviewMat;
		glm::mat4 modelviewprojMat;
		glm::mat4 lightSpaceMat;
		glm::mat4 FrameCoords;
		glm::mat4 FrameUncoords;
		glm::float32 Gamma;
		glm::float32 LightMapIntensity;		// DrawComplex/OneXBlending
		glm::float32 LightColorIntensity;	// DrawGouraud/ActorXBlending
		glm::float32 YScale;			// -1.f when presenting through the DXGI interop swapchain (ReduceMouseLag), 1.f otherwise

	};
	BufferObject<FrameState> FrameStateBuffer;
		
	// Light info
	TArray<AActor*> LightList;
	INT NumLights{};
	struct LightInfo
	{
		glm::vec4 LightData1[MAX_LIGHTS];
		glm::vec4 LightData2[MAX_LIGHTS];
		glm::vec4 LightData3[MAX_LIGHTS];
		glm::vec4 LightData4[MAX_LIGHTS];
		glm::vec4 LightData5[MAX_LIGHTS];
		glm::vec4 LightPos[MAX_LIGHTS];
	};
	BufferObject<LightInfo> LightInfoBuffer;

	// Global ClipPlanes.
	struct ClipPlaneInfo
	{
		glm::vec4 ClipParams;
		glm::vec4 ClipPlane;
	};
	BufferObject<ClipPlaneInfo> GlobalClipPlaneBuffer;

	struct EditorState
	{
		glm::uint HitTesting;
		glm::uint RendMap;
	};
	BufferObject<EditorState> EditorStateBuffer;

	// Fog
	struct DistanceFogInfo
	{
		glm::vec4 FogColor;
		glm::float32 FogStart;
		glm::float32 FogEnd;
		glm::float32 FogDensity;
		glm::int32 FogMode;
	};
	BufferObject<DistanceFogInfo> DistanceFogBuffer;

	//
	// Shader Data Structures
	//

	// ============================== DRAWTILE ==============================
	struct DrawTileParameters
	{
		glm::vec4		DrawColor;
		glm::uint64     TexHandles[2]; // mirrored as a uvec4 in GLSL since uint64 is not universally supported
		glm::uint32     DrawFlags;
		glm::uint32     Dummy0;
		glm::uint32     Dummy1;
		glm::uint32     Dummy2;
	};
	static const ShaderProgram::DrawCallParameterInfo DrawTileParametersInfo[];
	static_assert(sizeof(DrawTileParameters) == 48, "Invalid tile draw parameters size");

	struct DrawTileVertexES
	{
		glm::vec3 Coords;
		glm::uint DrawID;
		glm::vec2 TexCoords;
	};
	static_assert(sizeof(DrawTileVertexES) == 24, "Invalid tile buffered vert size");

	struct DrawTileVertexCore
	{
		glm::vec3 Coords;
		glm::uint DrawID;
		glm::vec4 TexCoords0;
		glm::vec4 TexCoords1;
		glm::vec4 TexCoords2;
	};
	static_assert(sizeof(DrawTileVertexCore) == 64, "Invalid tile buffered vert size");

	// ============================== DRAWSIMPLE ==============================
	struct DrawSimpleParameters
	{
		glm::vec4 DrawColor;
	};
	static const ShaderProgram::DrawCallParameterInfo DrawSimpleParametersInfo[];

	struct DrawSimpleVertex
	{
		glm::vec3 Coords;
		glm::uint DrawID;
	};
	static_assert(sizeof(DrawSimpleVertex) == 16, "Invalid simple buffered vert size");

	// ============================== DRAWGOURAUD ==============================
	struct DrawGouraudParameters
	{
		glm::vec4 DiffuseInfo;			// UMult, VMult, Diffuse, Alpha
		glm::vec4 DetailMacroInfo;		// Detail UMult, Detail VMult, Macro UMult, Macro VMult
		glm::vec4 DrawColor;
		// Only indices 0 (Diffuse), 3 (Detail), 4 (Macro) are ever read on the GPU side for this
		// shader -- LightMap/FogMap/EnvironmentMap/HeightMap are structurally unused by Gouraud
		// meshes (they use per-vertex LightColor/FogColor instead), and BumpMapIndex(5)'s handle is
		// written (for the sibling game) but never read here. 6 slots (3 uvec4s) covers indices 0-5.
		glm::uint64 TexHandles[6];		// mirrored as 3 uvec4s
		glm::uint32 DrawFlags;
		glm::uint32 Dummy0;
		glm::uint32 Dummy1;
		glm::uint32 Dummy2;
	};
	static const ShaderProgram::DrawCallParameterInfo DrawGouraudParametersInfo[];
	static_assert(sizeof(DrawGouraudParameters) == 112, "Invalid complex drawcall parameters size");

	struct DrawGouraudVertex
	{
		glm::vec3 Coords;
		glm::uint DrawID;
		glm::vec2 TexCoords;
		glm::vec4 LightColor;
		glm::vec4 FogColor;
	};
	static_assert(sizeof(DrawGouraudVertex) == 56, "Invalid gouraud buffered vertex size");

	// A per-vertex normal, packed into GL_INT_2_10_10_10_REV (10/10/10/2 bits XYZW) instead of a
	// 16-byte vec4. Kept in its own VBO (DrawGouraudProgram::NormalsBuffer), separate from
	// DrawGouraudVertex, so draws that don't need real per-vertex normals (the common case -- no
	// bump maps active, not in the RendMap==REN_Normals editor debug view) never touch this data at
	// all. See DrawGouraudProgram::bNeedNormalsThisPass and CreateInputLayout.
	struct DrawGouraudNormal
	{
		glm::int32 PackedNormal;
	};
	static_assert(sizeof(DrawGouraudNormal) == 4, "Invalid gouraud packed-normal size");

	// ============================== DRAWCOMPLEX ==============================
	// EnviroMapUV/HeightMapInfo and their TexHandles slots are only ever written/read under
	// #if ENGINE_VERSION==227 (see DrawComplex.cpp/DrawComplex_GLSL.cpp) -- live for the sibling
	// game that shares this codebase at that engine version, dead in this build. BumpMapInfo is
	// different: its C++ writer is NOT version-gated (DrawComplex.cpp's SetTextureHelper call for
	// BumpMapIndex runs whenever a texture has a real bump map, in every engine version), but its
	// GLSL reader is only live under #if UNREAL_OLDUNREAL (ShaderProgram.cpp's
	// SetOptionsForRendererConfig), so it's gated on that macro instead. Version-gating instead of
	// deleting keeps this struct byte-identical to before for the sibling (304 bytes either way)
	// while shrinking it for this build (240 bytes).
	struct DrawComplexParameters
	{
		glm::vec4 DiffuseUV;
		glm::vec4 LightMapUV;
		glm::vec4 FogMapUV;
		glm::vec4 DetailUV;
		glm::vec4 MacroUV;
#if ENGINE_VERSION==227
		glm::vec4 EnviroMapUV;
#endif
		glm::vec4 DiffuseInfo;
		glm::vec4 MacroInfo;
#if UNREAL_OLDUNREAL
		glm::vec4 BumpMapInfo;
#endif
#if ENGINE_VERSION==227
		glm::vec4 HeightMapInfo;
#endif
		glm::vec4 XAxis;
		glm::vec4 YAxis;
		glm::vec4 ZAxis;
		glm::vec4 DrawColor;
#if ENGINE_VERSION==227
		glm::uint64 TexHandles[8]; // needs indices 0-7 (Environment/Height live for the sibling), mirrored as 4 uvec4s
#else
		glm::uint64 TexHandles[6]; // needs indices 0-5 (Diffuse..Macro + Bump handle), mirrored as 3 uvec4s
#endif
		glm::uint32 DrawFlags;
		glm::uint32 Dummy0;
		glm::uint32 Dummy1;
		glm::uint32 Dummy2;
	};
	static const ShaderProgram::DrawCallParameterInfo DrawComplexParametersInfo[];
#if ENGINE_VERSION==227
	static_assert(sizeof(DrawComplexParameters) == 304, "Invalid complex drawcall parameters size");
#else
	static_assert(sizeof(DrawComplexParameters) == 240, "Invalid complex drawcall parameters size");
#endif

	struct DrawComplexVertex
	{
		glm::vec3 Coords;
		glm::uint DrawID;
	};
	static_assert(sizeof(DrawComplexVertex) == 16, "Invalid complex buffered vertex size");

	// ============================== NOPROGRAM ==============================
	struct NoParameters
	{
		glm::vec4 Dummy;
	};

	struct NoVertex
	{
		glm::vec4 Dummy;
	};

	//
	// DrawTile Shaders
	//
	class DrawTileCoreProgram : public ShaderProgramImpl<DrawTileVertexCore, DrawTileParameters>
	{
	public:
		DrawTileCoreProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();	
		void ActivateShader();
		void DeactivateShader();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		BOOL DepthTesting;		
	};

	// Used for ES _and_ for Core (w/ geo shaders disabled)!
	class DrawTileESProgram : public ShaderProgramImpl<DrawTileVertexES, DrawTileParameters>
	{
	public:
		DrawTileESProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		BOOL DepthTesting;		
	};

	//
	// DrawSimple Shaders
	//
	class DrawSimpleLineProgram : public ShaderProgramImpl<DrawSimpleVertex, DrawSimpleParameters>
	{
	public:
		DrawSimpleLineProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		glm::uint OldLineFlags;
		glm::uint OldBlendMode;		
	};

	class DrawSimpleTriangleProgram : public ShaderProgramImpl<DrawSimpleVertex, DrawSimpleParameters>
	{
	public:
		DrawSimpleTriangleProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		glm::uint OldLineFlags;
		glm::uint OldBlendMode;
	};

	//
	// DrawGouraud Shader
	//
	class DrawGouraudProgram : public MultiSpecializationShaderProgramImpl<DrawGouraudVertex, DrawGouraudParameters>
	{
	public:
		DrawGouraudProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();
		void MapBuffers() override;
		void UnmapBuffers() override;

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		// Cached Texture Infos
		FTEXTURE_PTR DetailTextureInfo{};
		FTEXTURE_PTR MacroTextureInfo{};
		FTEXTURE_PTR BumpMapInfo{};

		// Packed per-vertex normals, kept in their own VBO/VAO so the common case (no bump maps,
		// not in the RendMap==REN_Normals editor debug view) never has to touch this data. See
		// CreateInputLayout for how the two VAOs are built, and XOpenGL.cpp's Lock() for where
		// bNeedNormalsThisPass gets decided (once per pass, matching how RendMap itself behaves).
		BufferObject<DrawGouraudNormal> NormalsBuffer;
		bool bNeedNormalsThisPass{};

	protected:
		void OnVertBufferBound() override;
		void OnVertBufferUploaded() override;
		void OnVertBufferRotated() override;
	};

	//
	// DrawComplex Shader
	//
	class DrawComplexProgram : public MultiSpecializationShaderProgramImpl<DrawComplexVertex, DrawComplexParameters>
	{
	public:
		DrawComplexProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		// Cached texture Info
		FTEXTURE_PTR BumpMapInfo{};
	};

	//
	// Dummy Shader
	//
	class NoProgram : public ShaderProgramImpl<NoVertex, NoParameters>
	{
	public:
		NoProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void Flush(bool Rotate);
		void CreateInputLayout();
		void MapBuffers();
		void UnmapBuffers();
		void ActivateShader();
		void DeactivateShader();
	};

	class PostProcessProgram : public ShaderProgramImpl<NoVertex, NoParameters>
	{
	public:
		PostProcessProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void Flush(bool Rotate);
		void CreateInputLayout();
		void MapBuffers();
		void UnmapBuffers();
		void ActivateShader();
		void DeactivateShader();
		void Draw(GLuint Texture, INT DstW, INT DstH);

		static void BuildVertexShader  (GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		GLuint BlitVAO{};
	};

	//
	// UObject interface
	//
	void StaticConstructor();
	void PostEditChange();
	void ShutdownAfterError();

	//
	// Common URenderDevice interface
	//
	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void  Exit();
	void  Flush(UBOOL AllowPrecache);
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void  Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize);
	void  Unlock(UBOOL Blit);
	void  DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet);
	void  DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void  DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);
	void  Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void  Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void  Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);
	void  ClearZ(FSceneNode* Frame);
	void  PushHit(const BYTE* Data, INT Count);
	void  PopHit(INT Count, UBOOL bForce);
	void  GetStats(TCHAR* Result);
#if UNREAL_OLDUNREAL
	void  ReadPixels(FColor* Pixels, UBOOL bGammaCorrectOutput);
#else
	void  ReadPixels(FColor* Pixels);
#endif
	void  EndFlash();
	void  DrawStats(FSceneNode* Frame);
	void  SetSceneNode(FSceneNode* Frame);
	void  PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags);

	//
	// Unreal 227 URenderDevice interface
	//
#if UNREAL_OLDUNREAL
	void  PreDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf);
	void  PostDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf);
	void  DrawPass(FSceneNode* Frame, INT Pass);
#endif
#if UNREAL_OLDUNREAL || UNREAL_TOURNAMENT_OLDUNREAL
	void  DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span = NULL);
#endif
	BYTE  PushClipPlane(const FPlane& Plane);
	BYTE  PopClipPlane();
	BYTE  SetZTestMode(BYTE Mode);

	//
	// Helper functions
	//
	DWORD PrepareGouraudCall(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags);
	void FinishGouraudCall(FTextureInfo& Info, DWORD DrawFlags);
	void PrepareSimpleCall(ShaderProgram* Shader, glm::uint& OldLineFlags, glm::uint LineFlags, glm::uint& OldBlendMode, glm::uint BlendMode);

	//
	// URenderDeviceOldUnreal469 interface
	//
#if UNREAL_TOURNAMENT_OLDUNREAL
	void  DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span);
	UBOOL SupportsTextureFormat(ETextureFormat Format);
	void  UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL);
#endif

	//
	// Extension Checking
	//
	UBOOL FindExt(const TCHAR* Name);
	void  FindProc(void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt);
	void  FindProcs(UBOOL AllowExt);
	UBOOL GLExtensionSupported(FString Extension_Name);
	void  CheckExtensions();

	//
	// Window/Context Creation
	//
	UBOOL CreateOpenGLContext(void* Window, INT NewColorBytes, UBOOL QueryOnly=FALSE);
	void  UnsetRes();
	void  SwapControl();

#if _WIN32 && !defined(_USING_V110_SDK71_)
	// DXGI low-latency swapchain (ReduceMouseLag).
	bool CreateDXGIFramebuffer(INT Width, INT Height, HANDLE hDev, struct ID3D11Device* pDevice, struct IDXGISwapChain* pSwapChain);
	void InitDXGISwapchain(INT Width, INT Height);
	void DestroyDXGISwapchain();
	void ResizeDXGISwapchain(INT Width, INT Height);
#endif

	void  UpdateRenderFBO(INT Width, INT Height);
	void  DestroyRenderFBO();

	UBOOL IsSupportedGLVersion(INT MajorVersion, INT MinorVersion);
	void SelectGLVersion();

#if !_WIN32
    UBOOL SetSDLAttributes() const;
	SDL_Window* CreateTemporaryWindow() const;
	void DestroyTemporaryWindow(SDL_Window* Window) const;
#else
	UBOOL SetWindowPixelFormat(HDC DC);
	void  PrintFormat(HDC hDC, INT nPixelFormat);
	HWND CreateTemporaryWindow(HDC& OutDC);
	void DestroyTemporaryWindow(HWND TmphWnd, HDC TmphDC) const;
#endif

	static QSORT_RETURN CDECL CompareRes(const FPlane* A, const FPlane* B) {
		return (QSORT_RETURN)(((A->X - B->X) != 0.0f) ? (A->X - B->X) : (A->Y - B->Y));
	}

	//
	// OpenGL Context State Management
	//
	void  MakeCurrent();
	void  UpdateCoords(FSceneNode* Frame);
	void  SetOrthoProjection(FSceneNode* Frame);
	void  SetProjection(FSceneNode* Frame, UBOOL bNearZ);
	void  SetPermanentState();
	void  SetProgram(INT CurrentProgram);

	// Complex_Prog and Gouraud_Prog independently batch opaque BSP surfaces and mesh triangles
	// into their own ring buffers and don't force a flush on every switch between them (see
	// SetProgram) -- draw order between two depth-tested opaque batches doesn't affect the final
	// image. Because of that, either one may be holding undrained data while the other is
	// active, so anything that mutates state undrained draws could depend on (shared UBOs,
	// texture bindings, blend state) -- or that needs to pin draw order against non-opaque draws
	// -- must drain the relevant program(s) first using these helpers instead of assuming
	// Shaders[ActiveProgram] is the only one that could have pending data.
	void  FlushInactiveBatchedProgram(INT ProgramIndex);
	void  FlushBatchedPrograms();
#if UNREAL_OLDUNREAL
	void  SetDistanceFog(FFogSurf& Surf);
#endif
	void  ResetDistanceFog();
	void  SetFrameStateUniforms();

	//
	// Textures/Sampler Management
	//
	static BOOL WillBlendStateChange(DWORD OldPolyFlags, DWORD NewPolyFlags);
	FCachedTexture* GetCachedTextureInfo(INT Multi, FTextureInfo& Info, DWORD PolyFlags, BOOL& IsResidentBindlessTexture, BOOL& IsBoundToTMU, BOOL& IsTextureDataStale, BOOL ShouldResetStaleState);
	void  SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias);
	void  SetNoTexture(INT Multi);
	DWORD GetPolyFlagsAndDrawFlags(DWORD PolyFlags, DWORD& DrawFlags, BOOL RemoveOccludeIfSolid);
	void  SetBlend(DWORD PolyFlags);
	DWORD SetDepth(DWORD LineFlags);
	void  SetSampler(GLuint Sampler, FTextureInfo& Info, UBOOL SkipMipmaps, UBOOL IsLightOrFogMap, UBOOL NoSmooth);
	BOOL  UploadTexture(FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags, BOOL IsFirstUpload, BOOL IsBindlessTexture, BOOL PartialUpload=FALSE, INT U=0, INT V=0, INT UL=0, INT VL=0, BYTE* TextureData=nullptr);
	void  GenerateTextureAndSampler(FCachedTexture* Bind);
	void  BindTextureAndSampler(INT Multi, FCachedTexture* Bind);

	//
	// Gamma Control
	//
	void  BuildGammaRamp(FLOAT GammaCorrection, FGammaRamp& Ramp);
	void  BuildGammaRamp(FLOAT GammaCorrection, FByteGammaRamp& Ramp);
	void  SetGamma(FLOAT GammaCorrection);
	FLOAT GetViewportGamma(UViewport* Viewport) const;

	//
	// Editor Hit Testing
	//
	void  LockHit(BYTE* InHitData, INT* InHitSize);
	void  UnlockHit(UBOOL Blit);
	void  SetSceneNodeHit(FSceneNode* Frame);
	bool  HitTesting() { return HitData != NULL; }

	//
	// Error logging
	//
#ifdef WIN32
	static void CALLBACK DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#else
    static void DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#endif
};

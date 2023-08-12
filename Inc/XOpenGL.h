/*=============================================================================
	XOpenGLDrv.h: Unreal OpenGL support header.

	Copyright 2014-2021 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Enables CHECK_GL_ERROR(). Deprecated, should use UseOpenGLDebug=True instead, but still may be handy to track something specific down.
// #define DEBUGGL 1

// Windows only.
// #define WINBUILD 1

// Linux/OSX mostly, needs to be set in Windows for SDL2Launch.
// #define SDL2BUILD 1

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

	#ifdef SDL2BUILD
		#include <SDL2/SDL.h>
	#endif
#else
    #define SDL2BUILD 1
	extern "C"
	{
		#include "glad.h"
	}
    #include <unistd.h>
    #include <SDL.h>
#endif

#include "XOpenGLTemplate.h" //thanks han!

#if ENGINE_VERSION==436 || ENGINE_VERSION==430
#define clockFast(Timer)   {Timer -= appCycles();}
#define unclockFast(Timer) {Timer += appCycles()-34;}
#elif ENGINE_VERSION==227
// stijn: Do we want to release resources (e.g., bound textures) if the game crashes?
// None of the original renderer or audio devices did this because you can easily trigger
// another crash during cleanup. This would change your crash message and hide the original
// cause of the crash.
#define XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP 1
#define XOPENGL_MODIFIED_LOCK 1
#elif UNREAL_TOURNAMENT_OLDUNREAL
// stijn: Just do what other devices do!
#define XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP 0
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

// maximum number of surfaces in one drawcomplex multi-draw
#define MAX_DRAWCOMPLEX_BATCH 4 * 256
// maximum number of polys in one drawgouraud multi-draw
#define MAX_DRAWGOURAUD_BATCH 64 * 256
// maximum number of tiles in one drawtile multi-draw
#define MAX_DRAWTILE_BATCH 64 * 256
// maximum number of lines/triangles in one drawsimple multi-draw
#define MAX_DRAWSIMPLE_BATCH 64 * 256

// stijn: this absolutely needs to be dynamic or stream draw on macOS
#define DRAWCALL_BUFFER_USAGE_PATTERN GL_STREAM_DRAW

#define DRAWSIMPLE_SIZE 64 * 256
#define DRAWTILE_SIZE 64 * 256
#define DRAWCOMPLEX_SIZE 256 * MAX_DRAWCOMPLEX_BATCH
#define DRAWGOURAUDPOLY_SIZE 256 * 1024
#define NUMBUFFERS 16

#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
# define MAX_LIGHTS 256
#else
# define MAX_LIGHTS 512
#endif

#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148

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
#if ENGINE_VERSION==227
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
// GL ERROR CHECK
inline int	CheckGLError(const char* file, int line)
{
	GLenum glErr = glGetError();

	if (glErr != GL_NO_ERROR)
	{
		const TCHAR* Msg = TEXT("Unknown");
		switch (glErr)
		{
		case GL_NO_ERROR:
			Msg = TEXT("GL_NO_ERROR");
			break;
		case GL_INVALID_ENUM:
			Msg = TEXT("GL_INVALID_ENUM");
			break;
		case GL_INVALID_VALUE:
			Msg = TEXT("GL_INVALID_VALUE");
			break;
		case GL_INVALID_OPERATION:
			Msg = TEXT("GL_INVALID_OPERATION");
			break;

		case GL_STACK_OVERFLOW:
			Msg = TEXT("GL_STACK_OVERFLOW");
			break;
		case GL_STACK_UNDERFLOW:
			Msg = TEXT("GL_STACK_UNDERFLOW");
			break;
		case GL_OUT_OF_MEMORY:
			Msg = TEXT("GL_OUT_OF_MEMORY");
			break;
		};
		GWarn->Logf(TEXT("XOpenGL Error: %ls (%i) file %ls at line %i"), Msg, glErr, appFromAnsi(file), line);
	}
	return 1;
}

inline glm::vec4 FPlaneToVec4(FPlane Plane)
{
	return glm::vec4(Plane.X, Plane.Y, Plane.Z, Plane.W);
}

enum DrawFlags : DWORD
{
	DF_None				= 0x00000000,
	DF_DiffuseTexture	= 0x00000001,
	DF_LightMap         = 0x00000002,
	DF_FogMap          	= 0x00000004,
	DF_DetailTexture	= 0x00000008,
	DF_MacroTexture	 	= 0x00000010,
	DF_BumpMap			= 0x00000020,
	DF_EnvironmentMap   = 0x00000040,
	DF_HeightMap	 	= 0x00000080,
	DF_NoNearZ			= 0x00000100,
};

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
			check(Len < 4096);
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

	FShaderWriterX& operator<<(DrawFlags Input)
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
#if ENGINE_VERSION==227
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
	BITFIELD NoBuffering;
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
	bool	UsingPersistentBuffersGouraud;
	bool	UsingPersistentBuffersComplex;
	bool	UsingPersistentBuffersTile;
	bool	UsingPersistentBuffersSimple;
	bool	UsingPersistentBuffersDrawcallParams;
	bool	UsingShaderDrawParameters;
	bool    UsingGeometryShaders;
	static INT LogLevel; // Verbosity level of the GL debug logging

	//
	// Window, OS, and global GL context state
	//
#ifdef _WIN32
	HGLRC hRC;
	HWND hWnd;
	HDC hDC;
    PIXELFORMATDESCRIPTOR pfd;
    static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
    static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
	TArray<FPlane> SupportedDisplayModes;
#else
	SDL_Window* Window;
#endif

	UBOOL WasFullscreen;

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

#ifdef SDL2BUILD
	SDL_GLContext glContext;
	static SDL_GLContext CurrentGLContext;
	static TArray<SDL_GLContext> AllContexts;
#elif _WIN32
	static TArray<HGLRC> AllContexts;
	static HGLRC   CurrentGLContext;
	static HMODULE hModuleGlMain;
	static HMODULE hModuleGlGdi;
#endif

	//
	// Hardware Capabilities and Constraints
	//
	FString AllExtensions;
	INT		MaxClippingPlanes;
	INT		NumberOfExtensions;
	INT		MaxUniformBlockSize;
	bool	SupportsAMDMemoryInfo;
	bool	SupportsNVIDIAMemoryInfo;
	bool	SupportsSwapControl;
	bool	SupportsSwapControlTear;
	bool	SupportsS3TC;
	bool	SupportsSSBO;
	bool	SupportsGLSLInt64;
	bool	SupportsClipDistance;

	//
	// Framerate Limiter
	//
#if ENGINE_VERSION==227
	FTime prevFrameTimestamp;
	INT FrameRateLimit;
#endif

	//
	// Current GL Context and Renderer State
	//
	bool bContextInitialized;
	bool bGlobalBuffersMapped;

	GLuint NumClipPlanes;
	BYTE LastZMode;

	// Lock variables.
	FPlane FlashScale, FlashFog;
	FLOAT RProjZ, Aspect;
	DWORD CurrentPolyFlags;
	DWORD CurrentAdditionalPolyFlags;
	DWORD CurrentLineFlags;
	FLOAT RFX2, RFY2;

	// SceneNode
	FLOAT StoredFovAngle;
	FLOAT StoredFX;
	FLOAT StoredFY;
	FLOAT StoredOrthoFovAngle;
	FLOAT StoredOrthoFX;
	FLOAT StoredOrthoFY;
	UBOOL StoredbNearZ;
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
	} TexInfo[8];
	
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

	BoundBuffer* UBOPoint{};    // aka GL_UNIFORM_BUFFER
	BoundBuffer* SSBOPoint{};   // aka GL_SHADER_STORAGE_BUFFER
	BoundBuffer* ArrayPoint{};  // aka GL_ARRAY_BUFFER

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
			return IndexOffset;
		}

		// Current size in bytes
		size_t SizeBytes()
		{
			return IndexOffset * sizeof(T);
		}

		// Offset of the first element of the active sub-buffer, in number of bytes
		GLuint BeginOffsetBytes()
		{
			return BeginOffset * sizeof(T);
		}

		// Moves the IndexOffset forward after buffering @ElementCount elements
		void Advance(GLuint ElementCount)
		{
			IndexOffset += ElementCount;
		}

		// Returns true if the currently active sub-buffer still has room for @ElementCount elements
		bool CanBuffer(GLuint ElementCount)
		{
			return SubBufferSize - IndexOffset >= ElementCount;
		}

		// Returns true if we have no buffered data in the currently active sub-buffer
		bool IsEmpty()
		{
			return IndexOffset == 0;
		}

		// Rotates @Index, @BeginOffset, and @IndexOffset so they point to the start of the next sub-buffer
		// if @Wait is true, this function will wait until the GPU has signaled the next sub-buffer
		// Returns true if Index points to a new sub-buffer after this call
		bool Rotate(bool Wait)
		{
			Index = (Index + 1) % SubBufferCount;
			IndexOffset = 0;
			BeginOffset = Index * SubBufferSize;
			if (Wait)
				this->Wait();

			if (SubBufferCount > 1)
				return true;
			return false;
		}

		T* GetElementPtr(GLuint Index)
		{
			checkSlow(Index < IndexOffset);
			return &Buffer[BeginOffset + Index];
		}

		// Returns a pointer to the element we're currently writing
		T* GetCurrentElementPtr()
		{
			return &Buffer[BeginOffset + IndexOffset];
		}

		T* GetLastElementPtr()
		{
			return &Buffer[BeginOffset + SubBufferSize - 1];
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

		// Creates a CPU-accessible mapping for this buffer
		void MapVertexBuffer(bool Persistent, GLuint BufferSize)
		{
			MapBuffer(GL_ARRAY_BUFFER, Persistent, BufferSize, GL_STREAM_DRAW);
		}

		void MapSSBOBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=GL_DYNAMIC_DRAW)
		{
			MapBuffer(GL_SHADER_STORAGE_BUFFER, Persistent, BufferSize, ExpectedUsage);
		}

		void MapUBOBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=GL_DYNAMIC_DRAW)
		{
			MapBuffer(GL_UNIFORM_BUFFER, Persistent, BufferSize, ExpectedUsage);
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

		void RebindBufferBase(const GLuint BindingIndex)
		{
			Bind();
			glBindBufferBase(BufferType, BindingIndex, BufferObjectName);
			Unbind();
		}

		//
		// Moves data over to the GPU by reinitializing or updating the backing buffer
		// If we set @Invalidate to true, XOGL will call glInvalidateBufferData on the backing buffer first
		// If we set @Reinitialize to true, we will use glBufferData to move the data. Otherwise, we use glBufferSubData
		// @ExpectedUsage is the expected usage pattern for the buffer data. We will ignore this value if @Reinitialize is false
		//
		// This function is a no-op if we're using persistent buffers!
		//
		void BufferData(bool Invalidate, bool Reinitialize, GLenum ExpectedUsage)
		{
			if (bPersistentBuffer)
			{
				glFlushMappedNamedBufferRange(BufferObjectName, BeginOffsetBytes(), SizeBytes());
				return;
			}

			if (Invalidate)
				glInvalidateBufferData(BufferObjectName);

			if (Reinitialize
#if defined(__LINUX_ARM__) || MACOSX
				|| 1 // These platforms seem to prefer forced reinitialization...
#endif
				)
				glBufferData(BufferType, SizeBytes(), Buffer, ExpectedUsage);
			else
				glBufferSubData(BufferType, 0, SizeBytes(), Buffer);
			CHECK_GL_ERROR();
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
			IndexOffset = Index = BeginOffset = 0;
			BufferObjectName = VaoObjectName = 0;
		}

		// Inserts a fence that makes the GPU signal the active sub-buffer when
		// it is done with the draw call that uses said buffer
		void Lock()
		{
			if (!bPersistentBuffer)
				return;

			Sync[Index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		// Blocks until the GPU has signaled the active sub-buffer
		void Wait()
		{
			if (!bPersistentBuffer)
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

	private:
		void MapBuffer(GLenum Target, bool Persistent, GLuint BufferSize, GLenum ExpectedUsage)
		{
			constexpr GLbitfield PersistentBufferFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;

			SubBufferSize = BufferSize;
			BufferType = Target;	
				
			// Allocate and pin buffers
			bPersistentBuffer = Persistent;
			if (bPersistentBuffer)
			{
				SubBufferCount = NUMBUFFERS;
				Sync = new GLsync [SubBufferCount];
				memset(Sync, 0, sizeof(GLsync) * SubBufferCount);

				glBindBuffer(Target, BufferObjectName);
				glBufferStorage(Target, SubBufferCount * BufferSize * sizeof(T), 0, PersistentBufferFlags);
				Buffer = static_cast<T*>(glMapNamedBufferRange(BufferObjectName, 0, SubBufferCount * BufferSize * sizeof(T), PersistentBufferFlags));
				glBindBuffer(Target, 0);
			}
			else
			{
				SubBufferCount = 1;
				Buffer = new T[BufferSize];
				glBindBuffer(Target, BufferObjectName);
				glBufferData(Target, BufferSize * sizeof(T), Buffer, ExpectedUsage);
				glBindBuffer(Target, 0);
			}
		}

		GLsync* Sync{};					// OpenGL sync object. One for each sub-buffer

		GLuint Index{};					// Index of the sub-buffer we're currently writing to. This will always be 0 if we're not using persistent buffers
		GLuint BeginOffset{};			// Global index of the first buffer element of the sub-buffer we're currently writing to (relative to the start of the _entire_ buffer)
		GLuint IndexOffset{};			// Index of the next buffer element we're going to write within the currently active sub-buffer (relative to the start of the sub-buffer)

		GLuint BufferObjectName{};		// OpenGL name of the buffer object
		GLuint VaoObjectName{};			// (Optional) OpenGL name of the VAO we associated with the buffer

		//
		// Buffer dimensions
		//
		GLuint SubBufferSize{};			// Size of each of the sub-buffers that comprise this buffer object (in number of T-sized elements)
		GLuint SubBufferCount{};		// Number of sub-buffers

		bool   bPersistentBuffer{};     //
		bool   bBound{};                // True if currently bound
		bool   bInputLayoutCreated{};   // 
		GLenum BufferType{};            // GL target
		BoundBuffer** BindingPoint{};   // The binding point that needs to be unbound before we can bind this buffer
		GLuint BindingIndex{};
	};

	//
	// Shaders
	//
	enum ShaderProgType
	{
		No_Prog,
		Simple_Prog,
		Tile_Prog,
		Gouraud_Prog,
		Complex_Prog,
		Max_Prog,
	};

	// Common interface for all shaders
	class ShaderProgram
	{
	protected:
		GLuint VertexShaderObject{};
		GLuint GeoShaderObject{};
		GLuint FragmentShaderObject{};
		GLuint ShaderProgramObject{};
		const TCHAR* ShaderName{};
		UXOpenGLRenderDevice* RenDev{};

	public:
		ShaderProgram(const TCHAR* Name, UXOpenGLRenderDevice* _RenDev)
			: ShaderName(Name)
			, RenDev(_RenDev)
		{}
		virtual ~ShaderProgram() = 0;

		//
		// Common functions
		//

		// Binds the uniform with the specified @Name to the binding point with index @BindingIndex
		void  BindUniform(const GLuint BindingIndex, const char* Name) const;

		// Gets the location of the uniform with the specified @Name
		// This location can later be used to write to the uniform
		void  GetUniformLocation(GLint& Uniform, const char* Name) const;

		// Compiles one shader
		typedef void (ShaderWriterFunc)(GLuint, class UXOpenGLRenderDevice*, FShaderWriterX&, ShaderProgram*);
		bool  CompileShader(GLuint ShaderType, GLuint& ShaderObject, ShaderWriterFunc Func, ShaderWriterFunc EmitHeaderFunc);

		// Links the entire shader program
		bool  LinkShaderProgram();

		// Builds the entire shader program.
		// @GeoShaderFunc may be nullptr
		// @EmitHeaderFunc is also optional. If it is not nullptr, BuildShaderProgram will call EmitHeaderFunc before each shader builder func
		bool  BuildShaderProgram(ShaderWriterFunc VertexShaderFunc, ShaderWriterFunc GeoShaderFunc, ShaderWriterFunc FragmentShaderFunc, ShaderWriterFunc EmitHeaderFunc = nullptr);

		void DeleteShader();

		void DumpShader(const char* Source, bool AddLineNumbers);

		// Used to describe the layout of the drawcall parameters
		struct DrawCallParameterInfo
		{
			const char* Type;
			const char* Name;
			const int ArrayCount;
		};
		static void  EmitDrawCallParametersHeader(GLuint ShaderType, class UXOpenGLRenderDevice* GL, const DrawCallParameterInfo* Info, FShaderWriterX& Out, ShaderProgram* Program, INT BufferBindingIndex, bool UseInstanceID=false);

		//
		// Program-specific functions
		//

		// Allocates and binds the vertex and/or drawcall parameter buffers this shader needs
		virtual void MapBuffers() = 0;

		// Unbinds and deallocates the vertex and/or drawcall parameter buffers this shader needs
		virtual void UnmapBuffers() = 0;

		// Generates, compiles, and links this shader
		virtual bool BuildShaderProgram() = 0;

		// Binds shader-specific state such as uniforms
		virtual void BindShaderState();

		// Binds the input layout for our vertex and/or drawcall parameter buffers
		virtual void CreateInputLayout() = 0;

		// Switches to this shader and sets global GL state if necessary
		virtual void ActivateShader() = 0;

		// Switches away from this shader. This is where we should flush any leftover buffered data and restore global GL state if necessary
		virtual void DeactivateShader() = 0;

		// Dispatches buffered data. If @Wait is true, we wait for the GPU to signal the next vertex (and, optionally, drawcall parameters) buffer before returning
		virtual void Flush(bool Wait = false) = 0;
	};

	ShaderProgram* Shaders[Max_Prog]{};
	void ResetShaders();
	void RecompileShaders();
	void InitShaders();
	INT PrevProgram;
	INT ActiveProgram;

	//
	// Global Shader State
	//
	enum GlobalShaderBindingIndices
	{
		MatricesIndex			= 0,
		TextureHandlesIndex		= 1,
		StaticLightInfoIndex	= 2,
		CoordsIndex				= 3,
		ClipPlaneIndex			= 4,
		TileParametersIndex		= 5,
		ComplexParametersIndex	= 6,
		GouraudParametersIndex	= 7,
		SimpleParametersIndex	= 8
	};

	// Matrices
	struct GlobalMatrices
	{
		glm::mat4 projMat;
		glm::mat4 viewMat;
		glm::mat4 modelMat;
		glm::mat4 modelviewMat;
		glm::mat4 modelviewprojMat;
		glm::mat4 lightSpaceMat;
	};
	BufferObject<GlobalMatrices> GlobalMatricesBuffer;
	
	// Static light info
	TArray<AActor*> StaticLightList;
	TArray<AActor*> LightList;
	struct LightInfo
	{
		glm::vec4 LightData1[MAX_LIGHTS];
		glm::vec4 LightData2[MAX_LIGHTS];
		glm::vec4 LightData3[MAX_LIGHTS];
		glm::vec4 LightData4[MAX_LIGHTS];
		glm::vec4 LightData5[MAX_LIGHTS];
		glm::vec4 LightPos[MAX_LIGHTS];
		LightInfo()
			:LightData1(),
			LightData2(),
			LightData3(),
			LightData4(),
			LightData5(),
			LightPos()
		{}
	}StaticLightData;
	LightInfo LightData;
	INT NumStaticLights = 0;
	INT NumLights = 0;
	BufferObject<LightInfo> StaticLightInfoBuffer;
	
	// Coords
	struct GlobalCoords
	{
		glm::mat4 FrameCoords;
		glm::mat4 FrameUncoords;
	};
	BufferObject<GlobalCoords> GlobalCoordsBuffer;

	// Global ClipPlanes.
	struct ClipPlaneInfo
	{
		glm::vec4 ClipParams;
		glm::vec4 ClipPlane;
	};
	BufferObject<ClipPlaneInfo> GlobalClipPlaneBuffer;

	// Fog
	glm::vec4 DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	glm::vec4 DistanceFogValues = glm::vec4(0.f, 0.f, 0.f, 0.f);
	glm::int32 DistanceFogMode = -1;
	bool bFogEnabled = false;

	//
	// SetTexture helper
	//
	static void StoreTexHandle(INT Index, glm::uvec4* Dst, const glm::uint64 TextureHandle)
	{
		if (Index % 2)
			*reinterpret_cast<glm::uint64*>(&Dst[Index / 2].z) = TextureHandle;
		else
			*reinterpret_cast<glm::uint64*>(&Dst[Index / 2].x) = TextureHandle;
	}

	//
	// Helper class for glMultiDrawArraysIndirect batching
	//
	class MultiDrawIndirectBuffer
	{
	public:
		MultiDrawIndirectBuffer(INT MaxMultiDraw)
		{
			CommandBuffer.AddZeroed(MaxMultiDraw);
		}

		void StartDrawCall()
		{
			CommandBuffer(TotalCommands).FirstVertex = TotalVertices + FirstVertexOffset;
			CommandBuffer(TotalCommands).BaseInstance = TotalCommands + BaseInstanceOffset;
			CommandBuffer(TotalCommands).InstanceCount = 1;
		}

		void EndDrawCall(INT Vertices)
		{
			TotalVertices += Vertices;
			CommandBuffer(TotalCommands++).Count = Vertices;
		}

		bool IsFull() const
		{
			return TotalCommands + 1 >= CommandBuffer.Num();
		}

		void Reset(INT NewFirstVertexOffset=0, INT NewBaseInstanceOffset=0)
		{
			TotalCommands = TotalVertices = 0;
			FirstVertexOffset = NewFirstVertexOffset;
			BaseInstanceOffset = NewBaseInstanceOffset;
		}

		void Draw(GLenum Mode, UXOpenGLRenderDevice* RenDev)
		{
			// Issue draw calls
			if (RenDev->OpenGLVersion == GL_Core && glDrawArraysInstancedBaseInstance)
			{
				for (INT i = 0; i < TotalCommands; ++i)
				{
					glDrawArraysInstancedBaseInstance(Mode,
						CommandBuffer(i).FirstVertex,
						CommandBuffer(i).Count,
						CommandBuffer(i).InstanceCount,
						CommandBuffer(i).BaseInstance
					);
				}

				// stijn: This does not work yet and I don't know why. Do we need to store the command buffer
				// in a GL_DRAW_INDIRECT_BUFFER even though the spec says that's optional?
				// glMultiDrawArraysIndirect(Mode, &DrawBuffer.CommandBuffer(0), DrawBuffer.TotalCommands, 0);
				CHECK_GL_ERROR();
			}
			else
			{
				for (INT i = 0; i < TotalCommands; ++i)
					glDrawArrays(Mode, CommandBuffer(i).FirstVertex, CommandBuffer(i).Count);
			}
		}

		struct MultiDrawIndirectCommand
		{
			glm::uint Count;
			glm::uint InstanceCount;
			glm::uint FirstVertex;
			glm::uint BaseInstance;
		};
		static_assert(sizeof(MultiDrawIndirectCommand) == 16, "Invalid indirect command size");

		TArray<MultiDrawIndirectCommand> CommandBuffer;
		INT TotalVertices{};
		INT TotalCommands{};

	private:
		INT BaseInstanceOffset{};
		INT FirstVertexOffset{};
	};

	//
	// DrawTile Shader
	//
	class DrawTileProgram : public ShaderProgram
	{
	public:
		DrawTileProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
			: ShaderProgram(Name, RenDev)
			, DrawBuffer(MAX_DRAWTILE_BATCH)
		{
		}

		~DrawTileProgram();

		//
		// ShaderProgram interface
		//
		void MapBuffers();
		void UnmapBuffers();
		bool BuildShaderProgram();
		void BindShaderState();
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();
		void Flush(bool Wait);

		//
		// RenDev functions
		//
		void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);

		//
		// GLSL Shaders
		//
		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		
	private:
		struct DrawCallParameters
		{
			glm::vec4		DrawColor;
			glm::vec4		HitColor;
			glm::uint		PolyFlags;
			glm::uint		BlendPolyFlags;
			glm::uint		HitTesting;
			glm::uint		DepthTested;
			glm::uint		Padding0;		// Manually inserted padding to ensure the size of DrawCallParameters is a multiple of GLSL vec4 alignment
			glm::uint		Padding1;
			glm::uint       Padding2;
			glm::float32	Gamma;
			glm::uvec4      TexHandles[1];  // Intentionally made this a uvec4 for alignment purposes 
		} DrawCallParams{};

		struct BufferedVertES
		{
			glm::vec3 Point;
			glm::vec2 UV;
		};
		static_assert(sizeof(BufferedVertES) == 20, "Invalid tile buffered vert size");

		struct BufferedVertCore
		{
			glm::vec3 Point;
			glm::vec4 TexCoords0;
			glm::vec4 TexCoords1;
			glm::vec4 TexCoords2;
		};
		static_assert(sizeof(BufferedVertCore) == 60, "Invalid tile buffered vert size");

		BufferObject<DrawCallParameters>	ParametersBuffer;
		BufferObject<BufferedVertES>		VertBufferES;
		BufferObject<BufferedVertCore>		VertBufferCore;
		MultiDrawIndirectBuffer DrawBuffer;
	};

	//
	// DrawSimple Shader
	//
	class DrawSimpleProgram : public ShaderProgram
	{
	public:
		DrawSimpleProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
			: ShaderProgram(Name, RenDev)
			, LineDrawBuffer(MAX_DRAWSIMPLE_BATCH)
			, TriangleDrawBuffer(MAX_DRAWSIMPLE_BATCH)
		{
		}

		~DrawSimpleProgram();

		//
		// ShaderProgram interface
		//
		void MapBuffers();
		void UnmapBuffers();
		bool BuildShaderProgram();
		void BindShaderState();
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();
		void Flush(bool Wait);

		//
		// RenDev functions
		//
		void Draw2DLine(const FSceneNode* Frame, FPlane& Color, DWORD LineFlags, const FVector& P1, const FVector& P2);
		void Draw3DLine(FSceneNode* Frame, FPlane& Color, DWORD LineFlags, FVector& P1, FVector& P2);
		void EndFlash();
		void Draw2DPoint(const FSceneNode* Frame, FPlane& Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);

		//
		// GLSL Shaders
		//
		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);

	private:
		struct DrawCallParameters
		{
			glm::vec4 DrawColor;
			glm::float32 Gamma;
			glm::uint HitTesting;
			glm::uint LineFlags;
			glm::uint DrawMode;
			glm::uint BlendMode;
			glm::uint Padding0;		// Manually inserted padding to ensure sizeof(DrawCallParameters) is a multiple of GLSL vec4 alignment
			glm::uint Padding1;
			glm::uint Padding2;
		} DrawCallParams{};

		struct BufferedVert
		{
			glm::vec3 Point;
		};
		static_assert(sizeof(BufferedVert) == 12, "Invalid simple buffered vert size");

		BufferObject<BufferedVert> LineVertBuffer;
		BufferObject<BufferedVert> TriangleVertBuffer;
		BufferObject<DrawCallParameters> ParametersBuffer;

		MultiDrawIndirectBuffer LineDrawBuffer;
		MultiDrawIndirectBuffer TriangleDrawBuffer;

		// Helpers
		void PrepareDrawCall(glm::uint LineFlags, const glm::vec4& DrawColor, glm::uint BlendMode, BufferObject<BufferedVert>& OutBuffer, INT VertexCount);
	};

	//
	// DrawGouraud Shader
	//
	class DrawGouraudProgram : public ShaderProgram
	{
	public:
		DrawGouraudProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
			: ShaderProgram(Name, RenDev)
			, DrawBuffer(MAX_DRAWGOURAUD_BATCH)
		{
		}

		~DrawGouraudProgram();

		//
		// ShaderProgram interface
        //
		void MapBuffers();
		void UnmapBuffers();
		bool BuildShaderProgram();
		void BindShaderState();
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();
		void Flush(bool Wait);

		//
		// RenDev functions
		//
		void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
		void DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
#endif
#if UNREAL_TOURNAMENT_OLDUNREAL
		void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span);
#endif
		//
		// GLSL Shaders
		//
		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);

	private:
		struct DrawCallParameters
		{
			glm::vec4 DiffuseInfo;			// UMult, VMult, Diffuse, Alpha
			glm::vec4 DetailMacroInfo;		// Detail UMult, Detail VMult, Macro UMult, Macro VMult
			glm::vec4 MiscInfo;				// BumpMap Specular, Gamma
			glm::vec4 DistanceFogColor;
			glm::vec4 DistanceFogInfo;
			glm::vec4 DrawColor;
			glm::uint DrawFlags;
			glm::uint PolyFlags;
			glm::uint TextureFormat;
			glm::uint HitTesting;
			glm::uint RendMap;
			glm::int32 DistanceFogMode;
			glm::uint Padding0;				// Intentionally inserted padding to make the struct layout consistent in C++, GLSL std140, and GLSL std430
			glm::uint Padding1;
			glm::uvec4 TexHandles[2];       // Holds up to 4 glm::uint64 texture handles
		} DrawCallParams{};

		struct BufferedVert
		{
			glm::vec3 Point;
			glm::vec3 Normal;
			glm::vec2 UV;
			glm::vec4 Light;
			glm::vec4 Fog;
		};
		static_assert(sizeof(BufferedVert) == 64, "Invalid gouraud buffered vertex size");

		// Helper funcs
		static void BufferVert(BufferedVert* Vert, FTransTexture* P);
		void PrepareDrawCall(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags);
		void FinishDrawCall(FTextureInfo& Info);
		void SetTexture(INT Multi, UTexture* Texture, DWORD DrawFlags, FSceneNode* Frame, FTEXTURE_PTR& CachedInfo);

		BufferObject<DrawCallParameters> ParametersBuffer;
		BufferObject<BufferedVert> VertBuffer;

		MultiDrawIndirectBuffer DrawBuffer;

		// Cached Texture Infos
		FTEXTURE_PTR DetailTextureInfo{};
		FTEXTURE_PTR MacroTextureInfo{};
		FTEXTURE_PTR BumpMapInfo{};
	};

	//
	// DrawComplex Shader
	//
	class DrawComplexProgram : public ShaderProgram
	{
	public:
		DrawComplexProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
			: ShaderProgram(Name, RenDev)
			, DrawBuffer(MAX_DRAWCOMPLEX_BATCH)
		{
		}

		~DrawComplexProgram();

		//
		// ShaderProgram interface
		//
		void MapBuffers();
		void UnmapBuffers();
		bool BuildShaderProgram();
		void BindShaderState();
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();
		void Flush(bool Wait);

		//
		// RenDev functions
		//
		void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, const FSurfaceFacet& Facet);

		//
		// GLSL Shaders
		//
		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);
		static void EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program);

	private:
		struct DrawCallParameters
		{
			glm::vec4 DiffuseUV;
			glm::vec4 LightMapUV;
			glm::vec4 FogMapUV;
			glm::vec4 DetailUV;
			glm::vec4 MacroUV;
			glm::vec4 EnviroMapUV;
			glm::vec4 DiffuseInfo;
			glm::vec4 MacroInfo;
			glm::vec4 BumpMapInfo;
			glm::vec4 HeightMapInfo;
			glm::vec4 XAxis;
			glm::vec4 YAxis;
			glm::vec4 ZAxis;
			glm::vec4 DrawColor;
			glm::vec4 DistanceFogColor;
			glm::vec4 DistanceFogInfo;
			glm::uint DrawFlags;
			glm::uint HitTesting;
			glm::uint TextureFormat;
			glm::uint PolyFlags;
			glm::uint RendMap;
			glm::int32 DistanceFogMode;
			glm::uint Padding0; // This manually inserted padding ensures this struct layout is identical in C++, GLSL std140, and GLSL std430
			glm::uint Padding1;
			glm::uvec4 TexHandles[4];  // Holds up to 8 glm::uint64 texture handles			
		} DrawCallParams{};

		// Sets texture and updates corresponding drawcall data
		void SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, DWORD DrawFlags, glm::vec4* TextureCoords = nullptr, glm::vec4* TextureInfo = nullptr);

		struct BufferedVert
		{
			glm::vec4 Coords;
			glm::vec4 Normal;
		};
		static_assert(sizeof(BufferedVert) == 32, "Invalid complex buffered vertex size");

		BufferObject<DrawCallParameters> ParametersBuffer;
		BufferObject<BufferedVert> VertBuffer;

		MultiDrawIndirectBuffer DrawBuffer;

		// Cached texture Info
		FTEXTURE_PTR BumpMapInfo{};
	};

	//
	// Dummy Shader
	//
	class NoProgram : public ShaderProgram
	{
	public:
		NoProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
			: ShaderProgram(Name, RenDev)
		{
		}

		//
		// ShaderProgram interface
		// 
		void MapBuffers();
		void UnmapBuffers();
		bool BuildShaderProgram();
		void BindShaderState();
		void CreateInputLayout();
		void ActivateShader();
		void DeactivateShader();
		void Flush(bool Wait);
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
	void  ReadPixels(FColor* Pixels);
	void  EndFlash();
	void  DrawStats(FSceneNode* Frame);
	void  SetSceneNode(FSceneNode* Frame);
	void  PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags);

	//
	// Unreal 227 URenderDevice interface
	//
#if ENGINE_VERSION==227
	void  PreDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf);
	void  PostDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf);
	void  DrawPass(FSceneNode* Frame, INT Pass);
#endif
#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
	void  DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span = NULL);
#endif
	BYTE  PushClipPlane(const FPlane& Plane);
	BYTE  PopClipPlane();
	BYTE  SetZTestMode(BYTE Mode);

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
	UBOOL CreateOpenGLContext(UViewport* Viewport, INT NewColorBytes);
	UBOOL SetWindowPixelFormat();
	void  UnsetRes();
	void  SwapControl();

#ifdef SDL2BUILD
    UBOOL SetSDLAttributes();
#elif _WIN32
	void  PrintFormat(HDC hDC, INT nPixelFormat);
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
	void  ResetFog();

	//
	// Textures/Sampler Management
	//
	static BOOL WillBlendStateChange(DWORD OldPolyFlags, DWORD NewPolyFlags);
	BOOL  WillTextureStateChange(INT Multi, FTextureInfo& Info, DWORD PolyFlags);
	FCachedTexture* GetCachedTextureInfo(INT Multi, FTextureInfo& Info, DWORD PolyFlags, BOOL& IsResidentBindlessTexture, BOOL& IsBoundToTMU, BOOL& IsTextureDataStale, BOOL ShouldResetStaleState);
	void  SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, DWORD DrawFlags);
	void  SetNoTexture(INT Multi);
	static DWORD SetPolyFlags(DWORD PolyFlags);
	void  SetBlend(DWORD PolyFlags, bool InverseOrder);
	DWORD SetDepth(DWORD PolyFlags);
	void  SetSampler(GLuint Multi, FTextureInfo& Info, DWORD PolyFlags, UBOOL SkipMipmaps, UBOOL IsLightOrFogMap, DWORD DrawFlags);
	BOOL  UploadTexture(FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags, BOOL IsFirstUpload, BOOL IsBindlessTexture, BOOL PartialUpload=FALSE, INT U=0, INT V=0, INT UL=0, INT VL=0, BYTE* TextureData=nullptr);
	void  GenerateTextureAndSampler(FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags, DWORD DrawFlags);
	void  BindTextureAndSampler(INT Multi, FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags);

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

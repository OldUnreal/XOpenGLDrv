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

//
// SetTexture helper
//
inline void StoreTexHandle(INT Index, glm::uvec4* Dst, const glm::uint64 TextureHandle)
{
	if (Index % 2)
		*reinterpret_cast<glm::uint64*>(&Dst[Index / 2].z) = TextureHandle;
	else
		*reinterpret_cast<glm::uint64*>(&Dst[Index / 2].x) = TextureHandle;
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
	INT		MaxSSBOBlockSize;
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

	GLuint NumClipPlanes;
	BYTE LastZMode;

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
	DWORD CachedPolyFlags;			// Last PolyFlags we used to select a shader specialization...
	DWORD CachedShaderOptions;		// ...and the resulting shader options

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

		// Creates a CPU-accessible mapping for this buffer
		void MapVertexBuffer(bool Persistent, GLuint BufferSize)
		{
			MapBuffer(GL_ARRAY_BUFFER, Persistent, BufferSize, VERTEX_BUFFER_USAGE_PATTERN);
		}

		void MapSSBOBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=DRAWCALL_BUFFER_USAGE_PATTERN)
		{
			MapBuffer(GL_SHADER_STORAGE_BUFFER, Persistent, BufferSize, ExpectedUsage);
		}

		void MapUBOBuffer(bool Persistent, GLuint BufferSize, GLenum ExpectedUsage=DRAWCALL_BUFFER_USAGE_PATTERN)
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
					glBufferSubData(BufferType, UnbufferedRegionOffset, Size, &Buffer[FirstUnbufferedElemIndex]);
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
		void MapBuffer(GLenum Target, bool Persistent, GLuint BufferSize, GLenum _ExpectedUsage)
		{
			// stijn: NOTE: nvidia persistent buffers seem to be coherent by default!
			constexpr GLbitfield PersistentBufferFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

			SubBufferSize = BufferSize;
			BufferType = Target;
			ExpectedUsage = _ExpectedUsage;
				
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
				Buffer = new T[BufferSize];
				memset(Buffer, 0, BufferSize * sizeof(T));
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
    
    //
    // Helper class for glMultiDrawArraysIndirect batching
    //
    class MultiDrawIndirectBuffer
    {
    public:
		MultiDrawIndirectBuffer()
		{
			CommandBuffer.AddZeroed(1024);
		}

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

		glm::uint GetDrawID() const
		{
			return TotalCommands + BaseInstanceOffset;
		}

        void Draw(GLenum Mode, UXOpenGLRenderDevice* RenDev)
        {
			// stijn: This does not work yet and I don't know why. Do we need to store the command buffer
			// in a GL_DRAW_INDIRECT_BUFFER even though the spec says that's optional?
			// glMultiDrawArraysIndirect(Mode, &CommandBuffer(0), TotalCommands, 0);
			for (INT i = 0; i < TotalCommands; ++i)
			{
				glDrawArrays(Mode, CommandBuffer(i).FirstVertex, CommandBuffer(i).Count);
				//glDrawArrays(Mode, 0, CommandBuffer(i).Count);
				//glDrawArraysInstancedBaseInstance(Mode, CommandBuffer(i).FirstVertex, CommandBuffer(i).Count, CommandBuffer(i).InstanceCount, CommandBuffer(i).BaseInstance);
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
	// Shaders
	//
	enum ShaderProgType
	{
		No_Prog,
		Simple_Triangle_Prog,
        Simple_Line_Prog,
		Tile_Prog,
		Gouraud_Prog,
		Complex_Prog,
		Max_Prog,
	};
    
    class ShaderOptions
    {
		public:
        enum
        {
            OPT_None				= 0x000000,
            
            // Types of textures we can use in a shader
            OPT_DiffuseTexture		= 0x000001,
            OPT_LightMap			= 0x000002,
            OPT_FogMap				= 0x000004,
            OPT_DetailTexture		= 0x000010,
            OPT_MacroTexture		= 0x000020,
            OPT_BumpMap				= 0x000040,
            OPT_EnvironmentMap		= 0x000080,
            OPT_HeightMap			= 0x000100,
            
            // Relevant PolyFlags
            OPT_Masked				= 0x000200,
            OPT_Unlit				= 0x000400,
            OPT_Modulated			= 0x000800,
            OPT_Translucent			= 0x001000,
            OPT_Environment			= 0x002000,
            OPT_RenderFog			= 0x004000,
            OPT_AlphaBlended		= 0x008000,
            
            // Miscellaneous
            OPT_DistanceFog			= 0x010000, // 227 distance fogging code. Seems rather slow
			OPT_NoNearZ				= 0x020000, // Not actually used in the shader code, but forces a flush when we're switching between NoNearZ and NearZ
			OPT_Selected			= 0x040000
        };

		ShaderOptions(DWORD ShaderOptions)
		{
			OptionsMask = ShaderOptions;
		}
		ShaderOptions() {}
        
        FString GetShortString() const;
        FString GetPreprocessorString() const;
        
        void SetOption(DWORD Option);
        void UnsetOption(DWORD Option);
        bool HasOption(DWORD Option) const;
        
        friend DWORD GetTypeHash(const ShaderOptions& Options)
        {
            return Options.OptionsMask;
        }
		
		bool operator==(const DWORD Options)
		{
			return OptionsMask == Options;
		}

		bool operator==(const ShaderOptions& Options)
		{
			return OptionsMask == Options.OptionsMask;
		}
        
    private:
        FString GetStringHelper(void (*AddOptionFunc)(FString&, const TCHAR*, bool)) const;
        DWORD OptionsMask{};
    };
    
    class ShaderSpecialization
    {
    public:
		ShaderOptions Options{};
        GLuint VertexShaderObject{};
        GLuint GeoShaderObject{};
        GLuint FragmentShaderObject{};
        GLuint ShaderProgramObject{};
        FString SpecializationName;
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

		//
		// We can compile different variants of the same shader functions and specialize them 
		// based on the invocation context (described by the shader options).
		// This specialization mechanism allows us to create (nearly) branchless shader code,
		// which massively boosts performance on some GPUs.
		//
		TMap<ShaderOptions, ShaderSpecialization*>   Specializations;

		//
		// Lookups in the Specializations map can be quite slow, so we cache the previous
		// lookup result here.
		//
        ShaderSpecialization*                       LastSpecialization{};		
        ShaderOptions                               LastOptions;

		MultiDrawIndirectBuffer						DrawBuffer;
		const TCHAR*                                ShaderName{};
		UXOpenGLRenderDevice*                       RenDev{};
        
        // Parameters to be specified by the shader constructor
        INT                                         VertexBufferSize;
        INT                                         ParametersBufferSize;
        INT                                         ParametersBufferBindingIndex;
        INT                                         NumTextureSamplers;
        GLenum                                      DrawMode;
		INT											NumVertexAttributes;
		BOOL										UseSSBOParametersBuffer;
		const DrawCallParameterInfo*				ParametersInfo;		
		ShaderWriterFunc*							VertexShaderFunc;
		ShaderWriterFunc*							GeoShaderFunc;
		ShaderWriterFunc*							FragmentShaderFunc;

		virtual ~ShaderProgram();

		//
		// Per-specialization functionality
		//

		// Binds the uniform with the specified @Name to the binding point with index @BindingIndex in compiled shader program @ProgramObject
		void  BindUniform(ShaderSpecialization* Specialization, const GLuint BindingIndex, const char* Name) const;

		void GetUniformLocation(ShaderSpecialization* Specialization, GLint& Uniform, const char* Name) const;

		// Binds shader-specific state such as uniforms
		virtual void BindShaderState(ShaderSpecialization* Specialization);

		// Switches to the specified shader specialization, possibly creating/compiling it on-the-fly
		virtual void SelectShaderSpecialization(ShaderOptions Options);

		//
		// Compilation support
		//

		// Compiles one shader function		
		bool CompileShaderFunction(GLuint ShaderFunctionObject, GLuint FunctionType, ShaderOptions Options, ShaderWriterFunc Func, bool HaveGeoShader=false);

		// Compiles and links an entire shader program
		// @GeoShaderFunc may be nullptr
		bool BuildShaderProgram(ShaderSpecialization* Specialization, ShaderWriterFunc VertexShaderFunc, ShaderWriterFunc GeoShaderFunc, ShaderWriterFunc FragmentShaderFunc);

		// Links the shader program after compiling all of its functions
		bool LinkShaderProgram(GLuint ShaderProgramObject) const;

		// Completely deletes/unmaps all compiled specializations of this shader
		void DeleteShaders();

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

		// Precompiles shaders we're definitely going to need
		virtual void BuildCommonSpecializations() = 0;
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
			DeleteShaders();
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
				auto In = ParametersBuffer.GetElementPtr((ParametersBuffer.Size() > 0) ? (ParametersBuffer.Size() - 1) : 0);
				memcpy(&DrawCallParams, In, sizeof(DrawCallParamsType));
			}

			if (HavePendingData)
			{
				VertBuffer.BufferData(false);

				// We might have to rebind the parameters buffer here because things like PushClipPlane and
				// PopClipPlane can temporarily bind another UBO.
				ParametersBuffer.Bind();
				ParametersBuffer.BufferData(false);

				// Issue the draw call
				DrawBuffer.Draw(DrawMode, RenDev);
			}

			if (Rotate)
			{
				// Now switch to a different parameters and vertex buffers so we don't stomp on 
				// the data the GPU is using
				ParametersBuffer.Lock();
				ParametersBuffer.Rotate(true);

				// Make sure the new parameters buffer starts with the drawcall parameters of the
				// call latest drawcall
				auto Out = ParametersBuffer.GetCurrentElementPtr();
				memcpy(Out, &DrawCallParams, sizeof(DrawCallParamsType));

				VertBuffer.Lock();
				VertBuffer.Rotate(true);
			}

			// Reset the multidraw buffer. Note that the Rotate() calls above might simply switch to an
			// unused part of the same SSBO/VBO we were already using, so we need to make sure the draw
			// buffer checks which offsets we're going to start from next
			DrawBuffer.Reset(
				VertBuffer.SubBufferOffset + VertBuffer.NextElemIndex,
				ParametersBuffer.SubBufferOffset + ParametersBuffer.NextElemIndex);
		}

		virtual void ActivateShader()
		{
			VertBuffer.Wait();
			VertBuffer.Bind();
			ParametersBuffer.Bind();
			for (INT i = 0; i < NumVertexAttributes; ++i)
				glEnableVertexAttribArray(i);
			// TODO: Check if we really need this
			// memset(&DrawCallParams, 0, sizeof(DrawCallParams));

			LastOptions = ShaderOptions::OPT_None;
			LastSpecialization = nullptr;
		}

		virtual void DeactivateShader()
		{
			Flush(false);
			for (INT i = 0; i < NumVertexAttributes; ++i)
				glDisableVertexAttribArray(i);
		}

		virtual void MapBuffers()
		{
			VertBuffer.GenerateVertexBuffer(RenDev);
			VertBuffer.MapVertexBuffer(RenDev->UsingPersistentBuffers, VertexBufferSize);
			VertBuffer.Bind();
			CreateInputLayout();

			if (UseSSBOParametersBuffer)
			{
				ParametersBufferSize = Min<INT>(ParametersBufferSize, (RenDev->MaxSSBOBlockSize / sizeof(DrawCallParams) / (RenDev->UsingPersistentBuffers ? NUMBUFFERS : 1)));
				ParametersBuffer.GenerateSSBOBuffer(RenDev, ParametersBufferBindingIndex);
				ParametersBuffer.MapSSBOBuffer(RenDev->UsingPersistentBuffers, ParametersBufferSize, DRAWCALL_BUFFER_USAGE_PATTERN);
			}
			else
			{
				ParametersBufferSize = Min<INT>(ParametersBufferSize, GetMaximumUniformBufferSize(ParametersInfo) / (RenDev->UsingPersistentBuffers ? NUMBUFFERS : 1));
				ParametersBuffer.GenerateUBOBuffer(RenDev, ParametersBufferBindingIndex);
				ParametersBuffer.MapUBOBuffer(RenDev->UsingPersistentBuffers, ParametersBufferSize, DRAWCALL_BUFFER_USAGE_PATTERN);
			}
		}

		virtual void UnmapBuffers()
		{
			VertBuffer.DeleteBuffer();
			ParametersBuffer.DeleteBuffer();
		}

		virtual void BuildCommonSpecializations() {}

		// Templated member data
		DrawCallParamsType                          DrawCallParams;
		BufferObject<DrawCallParamsType>            ParametersBuffer;
		BufferObject<VertexType>                    VertBuffer;
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
		FrameStateIndex					= 1,
		LightInfoIndex					= 2,
		ClipPlaneIndex					= 3,
		EditorStateIndex				= 4,
		TileParametersIndex				= 5,
		ComplexParametersIndex			= 6,
		GouraudParametersIndex			= 7,
		SimpleLineParametersIndex		= 8,
		SimpleTriangleParametersIndex	= 9
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
	glm::vec4 DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	glm::vec4 DistanceFogValues = glm::vec4(0.f, 0.f, 0.f, 0.f);
	glm::int32 DistanceFogMode = -1;
	bool bFogEnabled = false;

	//
	// Shader Data Structures
	//

	// ============================== DRAWTILE ==============================
	struct DrawTileParameters
	{
		glm::vec4		DrawColor;
		glm::uvec4      TexHandles[1];  // Intentionally made this a uvec4 for alignment purposes 
	};
	static const ShaderProgram::DrawCallParameterInfo DrawTileParametersInfo[];
	static_assert(sizeof(DrawTileParameters) == 0x20, "Invalid tile draw parameters size");

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
		glm::vec4 MiscInfo;				// BumpMap Specular, Gamma
		glm::vec4 DistanceFogColor;
		glm::vec4 DistanceFogInfo;
		glm::vec4 DrawColor;
		glm::int32 DistanceFogMode;
		glm::int32 Padding0;
		glm::int32 Padding1;
		glm::int32 Padding2;
		glm::uvec4 TexHandles[4];       // Holds up to 4 glm::uint64 texture handles
	};
	static const ShaderProgram::DrawCallParameterInfo DrawGouraudParametersInfo[];

	struct DrawGouraudVertex
	{
		glm::vec3 Coords;
		glm::uint DrawID;
		glm::vec4 Normals;
		glm::vec2 TexCoords;
		glm::vec4 LightColor;
		glm::vec4 FogColor;
	};
	static_assert(sizeof(DrawGouraudVertex) == 72, "Invalid gouraud buffered vertex size");

	// ============================== DRAWCOMPLEX ==============================
	struct DrawComplexParameters
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
		glm::int32 DistanceFogMode;
		glm::uint Padding0; // This manually inserted padding ensures this struct layout is identical in C++, GLSL std140, and GLSL std430
		glm::uint Padding1;
		glm::uint Padding2;
		glm::uvec4 TexHandles[4];  // Holds up to 8 glm::uint64 texture handles
	};
	static const ShaderProgram::DrawCallParameterInfo DrawComplexParametersInfo[];
	static_assert(sizeof(DrawComplexParameters) == 0x150, "Invalid complex drawcall parameters size");

	struct DrawComplexVertex
	{
		glm::vec3 Coords;
		glm::uint DrawID;
		glm::vec4 Normal;
	};
	static_assert(sizeof(DrawComplexVertex) == 32, "Invalid complex buffered vertex size");

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
		void BuildCommonSpecializations();

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
		void BuildCommonSpecializations();

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
		void BuildCommonSpecializations();

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
		void BuildCommonSpecializations();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		glm::uint OldLineFlags;
		glm::uint OldBlendMode;
	};

	//
	// DrawGouraud Shader
	//
	class DrawGouraudProgram : public ShaderProgramImpl<DrawGouraudVertex, DrawGouraudParameters>
	{
	public:
		DrawGouraudProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();
		void BuildCommonSpecializations();

		static void BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);
		static void BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out);

		// Cached Texture Infos
		FTEXTURE_PTR DetailTextureInfo{};
		FTEXTURE_PTR MacroTextureInfo{};
		FTEXTURE_PTR BumpMapInfo{};
	};

	//
	// DrawComplex Shader
	//
	class DrawComplexProgram : public ShaderProgramImpl<DrawComplexVertex, DrawComplexParameters>
	{
	public:
		DrawComplexProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev);
		void CreateInputLayout();
		void BuildCommonSpecializations();

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
		void BuildCommonSpecializations();
		void MapBuffers();
		void UnmapBuffers();
		void ActivateShader();
		void DeactivateShader();
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
#if ENGINE_VERSION==227
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
	// Helper functions
	//
	void PrepareGouraudCall(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags);
	void FinishGouraudCall(FTextureInfo& Info);
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
	void  SetFrameStateUniforms();

	//
	// Textures/Sampler Management
	//
	static BOOL WillBlendStateChange(DWORD OldPolyFlags, DWORD NewPolyFlags);
	BOOL  WillTextureStateChange(INT Multi, FTextureInfo& Info, DWORD PolyFlags);
	FCachedTexture* GetCachedTextureInfo(INT Multi, FTextureInfo& Info, DWORD PolyFlags, BOOL& IsResidentBindlessTexture, BOOL& IsBoundToTMU, BOOL& IsTextureDataStale, BOOL ShouldResetStaleState);
	void  SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias);
	void  SetNoTexture(INT Multi);
	DWORD GetPolyFlagsAndShaderOptions(DWORD PolyFlags, DWORD& Options, BOOL RemoveOccludeIfSolid);
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

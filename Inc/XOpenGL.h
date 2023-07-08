/*=============================================================================
	XOpenGLDrv.h: Unreal OpenGL support header.

	Copyright 2014-2021 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Enables CHECK_GL_ERROR(). Deprecated, should use UseOpenGLDebug=True instead, but still may be handy to track something specific down.
// #define DEBUGGL 1

// Maybe for future release. Not in use yet.
// #define QTBUILD 1

// Windows only.
// #define WINBUILD 1

// Linux/OSX mostly, needs to be set in Windows for SDL2Launch.
// #define SDL2BUILD 1


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
	#elif QTBUILD

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
#define MAX_DRAWCOMPLEX_BATCH 1024
// maximum number of polys in one drawgouraud multi-draw
#define MAX_DRAWGOURAUD_BATCH 16384
// size of the Bindless Texture Handles SSBO. The GL spec guarantees we can make this 128MiB, but 16MiB should be more than enough...
#define BINDLESS_SSBO_SIZE (16 * 1024 * 1024)

#define DRAWSIMPLE_SIZE 524288
#define DRAWTILE_SIZE 1048576
#define DRAWCOMPLEX_SIZE 8 * 32 * MAX_DRAWCOMPLEX_BATCH
#define DRAWGOURAUDPOLY_SIZE 2097152
#define NUMBUFFERS 6

#if ENGINE_VERSION>=430 && ENGINE_VERSION<1100
			#define MAX_LIGHTS 256
#else
			#define MAX_LIGHTS 512
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
    #define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

enum ShaderProgType
{
	No_Prog,
	Simple_Prog,
	Tile_Prog,
	GouraudPolyVert_Prog,
	GouraudMeshBufferPolyVert_Prog,
	ComplexSurfaceSinglePass_Prog,
	ShadowMap_Prog,
};

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
    Parallax_Disabled = 0,
    Parallax_Basic = 1,
    Parallax_Occlusion = 2,
    Parallax_Relief = 3,
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
	else
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
	else
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

	// Error checking
	// GL ERROR CHECK
	inline int CheckGLError(char *file, int line)
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

	// Information about a cached texture.
	struct FCachedTexture
	{
		GLuint Ids[2]; // 0:Unmasked, 1:Masked.
		INT BaseMip;
		INT MaxLevel;
		GLuint Sampler[2];			   // Sampler object
		GLuint64 BindlessTexHandle[2]; // Bindless handles
		GLuint TexNum[2];			   // TMU num
		INT RealtimeChangeCount{};
	};

	/*
	// Information about a Mesh
	struct UMeshBufferData
	{
		FLOAT*	Verts;
		FLOAT*	VertNormals;
		FLOAT*	Tex;
		FLOAT*	LightColor;
		GLuint	NumPts;
		DWORD	PolyFlags;
		bool	HasData;
	} MeshBufferData; // Buffer a mesh for the renderer.
	*/

	// Permanent variables.
#ifdef _WIN32
	HGLRC hRC;
	HWND hWnd;
	HDC hDC;
    PIXELFORMATDESCRIPTOR pfd;
    static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
    static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
#else
	SDL_Window* Window;
#endif

	UBOOL WasFullscreen;
	TOpenGLMap<QWORD,FCachedTexture> LocalBindMap, *BindMap;
	TArray<FPlane> Modes;
	ULevel* Level;

	TArray<AActor*> StaticLightList;
	TArray<AActor*> LightList;

	// Context specifics.
	INT DesiredColorBits;
	INT DesiredStencilBits;
	INT DesiredDepthBits;
	INT iPixelFormat;

	// Timing.
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

	// Hardware constraints.
	INT RefreshRate;
	FLOAT MaxAnisotropy;
	INT DebugLevel;
	INT NumAASamples;
    INT MaxClippingPlanes;
    INT NumberOfExtensions;
    INT DetailMax;
	INT MaxUniformBlockSize;
    FLOAT GammaMultiplier;
    FLOAT GammaMultiplierUED;

    // Pffffff. FrameRateLimiter UTGLR style.
#if ENGINE_VERSION==227
    FTime prevFrameTimestamp;
    INT FrameRateLimit;
#endif

    // Config
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

    //OpenGL 4 Config
	BITFIELD UseBindlessTextures;
	BITFIELD UseBindlessLightmaps;
	BITFIELD UsePersistentBuffers;
	BITFIELD UseBufferInvalidation;
	BITFIELD UseShaderDrawParameters;

	// Not really in use...(yet)
	BITFIELD UseMeshBuffering; //Buffer (Static)Meshes for drawing.
	BITFIELD UseSRGBTextures;
	BITFIELD EnvironmentMaps;

    // Internal stuff
	bool UsingPersistentBuffers;
	bool UsingPersistentBuffersGouraud;
	bool UsingPersistentBuffersComplex;
	bool UsingPersistentBuffersTile;
	bool UsingShaderDrawParameters;
	bool AMDMemoryInfo;
	bool NVIDIAMemoryInfo;
	bool SwapControlExt;
	bool SwapControlTearExt;
    bool Compression_s3tcExt;
	bool SupportsSSBO;
	bool SupportsGLSLInt64;

	// Bindless textures support
	bool UsingBindlessTextures;
	INT	 MaxBindlessTextures;

	enum EBindlessHandleStorage
	{
		//
		// Store handles in a uniform buffer object.
		//
		// * Pro: Supported by all GPUs with GL_ARB_bindless_texture capabilities
		//
		// * Con: UBOs are small. Even on modern GPUs, we often have a max UBO size
		// of only 64KiB. That only gives us enough space to store 4096 bindless
		// texture handles
		//
		STORE_UBO,

		//
		// Store handles in a shader storage buffer object.
		//
		// Requires: GL_ARB_shader_storage_buffer_object
		//
		// * Pro: Much bigger than UBOs. The spec guarantees that SSBOs can be
		// at least 128MiB. That gives us enough room for over 8 million bindless
		// texture handles
		//
		// * Con: requires SSBO support
		// * Con: SSBO access is usually slower than UBO access
		//
		STORE_SSBO,

		//
		// Passes bindless handles as integer parameters to the shaders
		//
		// Requires: GL_ARB_gpu_shader_int64
		//
		// * Pro: Unlimited bindless handles
		// * Pro: Faster than UBOs or SSBOs
		//
		// * Con: Not supported by Intel iGPUs or on macOS
		//
		STORE_INT
	} BindlessHandleStorage;

	bool NeedDynamicallyUniformBindlessHandles;

	BYTE OpenGLVersion;
	BYTE ParallaxVersion;
	BYTE UseVSync;
	bool NeedsInit;
	bool bMappedBuffers;
	BYTE SupportsClipDistance;

	FLOAT GammaOffsetScreenshots;
	FLOAT LODBias;

	// Hit testing.
	TArray<BYTE> HitStack;
	TArray<BYTE> HitMem;
	TArray<INT>  HitMemOffs;
	FPlane HitColor;
	BYTE*  HitData;
	INT*   HitSize;
	INT    HitCount;

	// Lock variables.
	FPlane FlashScale, FlashFog;
	FLOAT RProjZ, Aspect;
	DWORD CurrentPolyFlags;
	DWORD CurrentAdditionalPolyFlags;
	DWORD CurrentLineFlags;
	struct FTexInfo
	{
		QWORD CurrentCacheID;
		INT CurrentCacheSlot;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
		INT TexNum;					// TMU number or index in the bindless texture array
		INT RealTimeChangeCount{};
	FTexInfo()
		:CurrentCacheID(0),
		CurrentCacheSlot(0),
		UMult(0),
		VMult(0),
		UPan(0),
		VPan(0),
		TexNum(0)
		{}
	} TexInfo[8];
	FLOAT RFX2, RFY2;

	INT PrevProgram;
	INT ActiveProgram;
	static DWORD ComposeSize;
	static BYTE* Compose;

	enum DrawTileDrawDataIndices : DWORD
	{
		DTDD_HIT_COLOR,
		DTDD_DRAW_COLOR,
		DTDD_MISC_INFO		// Gamma
	};
	
	struct DrawTileShaderDrawParams
	{
		glm::vec4 DrawData[3];
		// 0: texture number, polyflags, blend polyflags hit testing (bool)
		// 1: depth tested (bool), unused, unused, unused
		glm::uvec4 _TexNum[2]; 

		UINT& TexNum()
		{
			return _TexNum[0].x;
		}

		UINT& PolyFlags()
		{
			return _TexNum[0].y;
		}

		UINT& BlendPolyFlags()
		{
			return _TexNum[0].z;
		}

		UINT& HitTesting()
		{
			return _TexNum[0].w;
		}

		UINT& DepthTested()
		{
			return _TexNum[1].x;
		}

		FLOAT& Gamma()
		{
			return DrawData[DTDD_MISC_INFO].x;
		}
		
	} DrawTileDrawParams;

	struct DrawTileBufferedVertES
	{
		glm::vec3 Point;
		glm::vec2 UV;
	};

	struct DrawTileBufferedVertCore
	{
		glm::vec3 Point;
		glm::vec4 TexCoords0;
		glm::vec4 TexCoords1;
		glm::vec4 TexCoords2;
	};

	struct DrawTileBuffer
	{
		GLuint Index;
		GLuint IndexOffset;
		GLuint BeginOffset;
		DrawTileBuffer()
			: Index(0),
			IndexOffset(0),
			BeginOffset(0)
		{}
	}DrawTileBufferData;

	struct DrawLinesBuffer
	{
		GLuint VertSize;
		FPlane DrawColor;
		DWORD LineFlags;
		DrawLinesBuffer()
			: VertSize(0),
			DrawColor(0.f, 0.f, 0.f, 0.f),
			LineFlags(0)
		{}
	}DrawLinesBufferData;

	//PMB's
	struct BufferRange
	{
		GLsync Sync[NUMBUFFERS];
		FLOAT* Buffer{};
		FLOAT* VertBuffer{};
		GLuint64* Int64Buffer{};
		BufferRange()
		: Sync()
		{}
	};

	BufferRange DrawGouraudBufferRange;
	INT PrevDrawGouraudBeginOffset;

	BufferRange DrawGouraudSSBORange;
	INT PrevDrawGouraudSSBOBeginOffset;

	GLint DrawGouraudMultiDrawPolyListArray[MAX_DRAWGOURAUD_BATCH];
	GLsizei DrawGouraudMultiDrawVertexCountArray[MAX_DRAWGOURAUD_BATCH];
	INT DrawGouraudMultiDrawCount;
	INT DrawGouraudMultiDrawVertices;

    BufferRange DrawComplexSinglePassRange;
	INT PrevDrawComplexBeginOffset;

	BufferRange DrawComplexSSBORange;
	INT PrevDrawComplexSSBOBeginOffset;

	GLint DrawComplexMultiDrawFacetArray[MAX_DRAWCOMPLEX_BATCH];
	GLsizei DrawComplexMultiDrawVertexCountArray[MAX_DRAWCOMPLEX_BATCH];
	INT DrawComplexMultiDrawCount;
	INT DrawComplexMultiDrawVertices;

    BufferRange DrawTileRange;
	INT PrevDrawTileBeginOffset;

	GLuint DrawSimpleVertObject, DrawSimpleGeoObject,DrawSimpleFragObject;
	GLuint DrawSimpleProg;

	GLuint DrawTileVertObject, DrawTileGeoObject, DrawTileFragObject;
	GLuint DrawTileProg;

	// Draw everything for ComplexSurface in one pass
	GLuint DrawComplexVertSinglePassObject, DrawComplexFragSinglePassObject, DrawComplexGeoSinglePassObject;
	GLuint DrawComplexProg;

	// DrawGouraud
	GLuint DrawGouraudVertObject, DrawGouraudGeoObject, DrawGouraudFragObject;
	GLuint DrawGouraudProg;

#if ENGINE_VERSION!=227

	// Definition for a surface that has fog
	struct FFogSurf
	{
		struct FSavedPoly* Polys;
		FLOAT		FogDistanceStart;	// Zone fog start (zero if none)
		FLOAT		FogDistanceEnd;		// Zone fog distance (zero if none)
		FLOAT		FogDensity;			// for exponential fog.
		FPlane		FogColor;			// Zone fog color
		INT			FogMode;			// 0 Linear, 1 Exponential, 2 Exponential2
		DWORD		PolyFlags;			// Surface flags.
		BYTE		bBSP;				// this flag is unfortunately needed for the old renderers to emulate the fog.
		FFogSurf()
			:Polys(NULL),
			FogDistanceStart(0.f),
			FogDistanceEnd(0.f),
			FogDensity(0.f),
			FogColor(FPlane(0.f, 0.f, 0.f, 0.f)),
			FogMode(0),
			PolyFlags(0),
			bBSP(0)
		{}
		inline BYTE IsValid()
		{
			return ((FogDistanceEnd > FogDistanceStart) && (FogDensity > 0.f || FogMode == 0));
		}
	};

#endif

	enum DrawGouraudDrawDataIndices : DWORD
	{
		DGDD_DIFFUSE_INFO,			// UMult, VMult, Diffuse, Alpha
		DGDD_DETAIL_MACRO_INFO,		// Detail UMult, Detail VMult, Macro UMult, Macro VMult
		DGDD_MISC_INFO,				// BumpMap Specular, Gamma, texture format, Unused
		DGDD_EDITOR_DRAWCOLOR,
		DGDD_DISTANCE_FOG_COLOR,
		DGDD_DISTANCE_FOG_INFO
	};

	struct DrawGouraudShaderDrawParams
	{
		glm::vec4 DrawData[6];
		glm::uvec4 TexNum;
		glm::uvec4 _DrawFlags;

		UINT& DrawFlags()
		{
			return _DrawFlags.x;
		}

		UINT& HitTesting()
		{
			return _DrawFlags.y;
		}

		UINT& PolyFlags()
		{
			return _DrawFlags.z;
		}

		UINT& RendMap()
		{
			return _DrawFlags.w;
		}

	} DrawGouraudDrawParams;

	struct DrawGouraudBufferedVert
	{
		glm::vec3 Point;
		glm::vec3 Normal;
		glm::vec2 UV;
		glm::vec4 Light;
		glm::vec4 Fog;
	};
	static_assert(sizeof(DrawGouraudBufferedVert) == 64, "Invalid gouraud buffered vertex size");

	struct DrawGouraudBuffer
	{
		GLuint Index;
		GLuint IndexOffset;
		GLuint BeginOffset;

		DrawGouraudBuffer()
			: Index(0),
			IndexOffset(0),
			BeginOffset(0)
		{}
	} DrawGouraudBufferData;

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

	enum DrawComplexDrawDataIndices : DWORD
	{
		DCDD_DIFFUSE_COORDS,
		DCDD_LIGHTMAP_COORDS,
		DCDD_FOGMAP_COORDS,
		DCDD_DETAIL_COORDS,
		DCDD_MACRO_COORDS,
		DCDD_ENVIROMAP_COORDS,
		DCDD_DIFFUSE_INFO,
		DCDD_MACRO_INFO,
		DCDD_BUMPMAP_INFO,
		DCDD_HEIGHTMAP_INFO,
		DCDD_X_AXIS,
		DCDD_Y_AXIS,
		DCDD_Z_AXIS,
		DCDD_EDITOR_DRAWCOLOR,
		DCDD_DISTANCE_FOG_COLOR,
		DCDD_DISTANCE_FOG_INFO
	};

	struct DrawComplexShaderDrawParams
	{
		glm::vec4 DrawData[16];
		glm::uvec4 TexNum[4];
		glm::uvec4 _DrawFlags;

		UINT& DrawFlags()
		{
			return _DrawFlags.x;
		}

		UINT& TextureFormat()
		{
			return _DrawFlags.y;
		}

		UINT& PolyFlags()
		{
			return _DrawFlags.z;
		}

		UINT& RendMap()
		{
			return _DrawFlags.w;
		}

		UINT& HitTesting()
		{
			return TexNum[3].x;
		}
	} DrawComplexDrawParams;

	struct DrawComplexBufferedVert
	{
		glm::vec4 Coords;
		glm::vec4 Normal;
	};
	static_assert(sizeof(DrawComplexBufferedVert) == 32, "Invalid complex buffered vertex size");

	struct DrawComplexBuffer
	{
		GLuint Index;
		GLuint IndexOffset;
		GLuint BeginOffset;
		GLuint Iteration;

		DrawComplexBuffer()
			: Index(0),
			IndexOffset(0),
			BeginOffset(0),
			Iteration(0)
		{}
	}DrawComplexBufferData;

	//DrawSimple
	FLOAT* DrawLinesVertsBuf;
	FLOAT* Draw2DLineVertsBuf;
	FLOAT* Draw2DPointVertsBuf;
	FLOAT* Draw3DLineVertsBuf;
	FLOAT* EndFlashVertsBuf;

	//DrawTile
	GLuint DrawTileTexCoords;
	GLuint DrawTileTextureHandle;

	//Matrices
	glm::mat4 projMat;
	glm::mat4 viewMat;
	glm::mat4 modelMat;
	glm::mat4 modelviewMat;
	glm::mat4 modelviewprojMat;
	glm::mat4 lightSpaceMat;

	// Coords.
	glm::mat4 FrameCoords;
	glm::mat4 FrameUncoords;

	FLOAT StoredFovAngle;
	FLOAT StoredFX;
	FLOAT StoredFY;
	FLOAT StoredOrthoFovAngle;
	FLOAT StoredOrthoFX;
	FLOAT StoredOrthoFY;
	UBOOL StoredbNearZ;
	bool bIsOrtho;

	// Shader globals
	GLuint GlobalUniformBlockIndex;
	GLuint GlobalMatricesUBO;
	static const GLuint GlobalMatricesBindingIndex = 0;

	// Global bindless textures.
    GLuint GlobalTextureHandlesBlockIndex;
    GLuint GlobalTextureHandlesBufferObject;
    BufferRange GlobalTextureHandlesRange;
	static const GLuint GlobalTextureHandlesBindingIndex = 1;

	// Hardware Lights
	GLuint GlobalUniformStaticLightInfoIndex;
	GLuint GlobalStaticLightInfoUBO;
	static const GLuint GlobalStaticLightInfoIndex = 3;

	// Global Coords
    GLuint GlobalUniformCoordsBlockIndex;
	GLuint GlobalCoordsUBO;
	static const GLuint GlobalCoordsBindingIndex = 4;

	// Global ClipPlanes.
	GLuint GlobalUniformClipPlaneIndex;
	GLuint GlobalClipPlaneUBO;
	static const GLuint GlobalClipPlaneBindingIndex = 5;

	//SSBOs
	GLuint DrawComplexSSBO;
	static const GLuint DrawComplexSSBOBindingIndex = 6;
	GLuint DrawGouraudSSBO;
	static const GLuint DrawGouraudSSBOBindingIndex = 7;

	glm::vec4 DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	glm::vec4 DistanceFogValues = glm::vec4(0.f, 0.f, 0.f, 0.f);
	bool bFogEnabled = false;

    GLuint TexNum;

	// Editor
	GLuint DrawSimplebHitTesting;
	GLuint DrawTilebHitTesting;

	// Bulk texture data
	GLuint DrawComplexSinglePassTexCoords;
	GLuint DrawGouraudDrawData;

	//Drawing colors
	GLuint DrawSimpleDrawColor;
	GLuint DrawTileHitDrawColor;
	GLuint DrawTileDrawColor;

	//Texture vars
	GLuint DrawTileTexture;
	GLuint DrawGouraudTexture[8];
	GLuint DrawComplexSinglePassTexture[8];

	// LineFlags for simple drawing.
	GLuint DrawSimpleLineFlags;

	// PolyFlags for shaders.
	GLuint DrawTilePolyFlags;
	GLuint DrawGouraudDrawFlags;
	GLuint DrawComplexSinglePassDrawFlags;

	// TexNum for bindless textures in shaders.
	GLuint DrawTileTexNum;
	GLuint DrawComplexSinglePassTexNum;
	GLuint DrawGouraudTexNum;

	// Gamma handling
	static FLOAT Gamma;
	GLuint DrawSimpleGamma;
	GLuint DrawTileGamma;

	//Vertices
	GLuint DrawSimpleVertBuffer;
	GLuint DrawTileVertBuffer;
	GLuint DrawComplexVertBuffer;
	GLuint DrawGouraudVertBuffer;

	//VAO's
	GLuint DrawSimpleGeometryVertsVao;
	GLuint DrawTileVertsVao;
	GLuint DrawGouraudPolyVertsVao;
	GLuint DrawComplexVertsSinglePassVao;
	GLuint SimpleDepthVao;

	// Cached Texture Infos
	FTEXTURE_PTR DrawGouraudDetailTextureInfo;
	FTEXTURE_PTR DrawGouraudMacroTextureInfo;
	FTEXTURE_PTR DrawGouraudBumpMapInfo;

	FTEXTURE_PTR DrawComplexBumpMapInfo;

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

	GLuint NumClipPlanes;
	BYTE LastZMode;

	// Static variables.
	static TOpenGLMap<QWORD,FCachedTexture> *SharedBindMap;
	static INT   LockCount;
	static INT LogLevel;

#ifdef SDL2BUILD
	SDL_GLContext glContext;
	static SDL_GLContext CurrentGLContext;
    static TArray<SDL_GLContext> AllContexts;
#elif QTBUILD

#elif _WIN32
	static TArray<HGLRC> AllContexts;
	static HGLRC   CurrentGLContext;
	static HMODULE hModuleGlMain;
	static HMODULE hModuleGlGdi;
#endif

    FString AllExtensions;

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
	void  PreDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf);
	void  PostDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf);
	void  DrawPass(FSceneNode* Frame, INT Pass);
	void  DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span = NULL);
	BYTE  PushClipPlane(const FPlane& Plane);
	BYTE  PopClipPlane();
	BYTE  SetZTestMode(BYTE Mode);

	//
	// URenderDeviceOldUnreal469 interface
	//
	void  DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span);
	UBOOL SupportsTextureFormat(ETextureFormat Format);

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
	void  LockBuffer(BufferRange& Buffer, GLuint Index);
	void  WaitBuffer(BufferRange& Buffer, GLuint Index);
	void  MapBuffers();
	void  UnMapBuffers();
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
	BOOL  WillTextureStateChange(INT Multi, FTextureInfo& Info, DWORD PolyFlags, DWORD BindlessTexNum=~0);
	FCachedTexture* GetCachedTextureInfo(INT Multi, FTextureInfo& Info, DWORD PolyFlags, BOOL& IsResidentBindlessTexture, BOOL& IsBoundToTMU, BOOL& IsTextureDataStale, BOOL ShouldResetStaleState);
	void  SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, DWORD DrawFlags);
	void  SetNoTexture(INT Multi);
	DWORD SetPolyFlags(DWORD PolyFlags);
	void  SetBlend(DWORD PolyFlags, bool InverseOrder);
	DWORD SetDepth(DWORD PolyFlags);
	void  SetSampler(GLuint Multi, FTextureInfo& Info, DWORD PolyFlags, UBOOL SkipMipmaps, UBOOL IsLightOrFogMap, DWORD DrawFlags);
	BOOL  UploadTexture(FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags, BOOL IsFirstUpload, BOOL IsBindlessTexture);
	void  GenerateTextureAndSampler(FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags, DWORD DrawFlags);
	void  BindTextureAndSampler(INT Multi, FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags);

	//
	// Gamma Control
	//
	void  BuildGammaRamp(FLOAT GammaCorrection, FGammaRamp& Ramp);
	void  BuildGammaRamp(FLOAT GammaCorrection, FByteGammaRamp& Ramp);
	void  SetGamma(FLOAT GammaCorrection);

	//
	// Shader Management
	//
	void  InitShaders();
	void  DeleteShaderBuffers();
	void  LinkShader(const TCHAR* ShaderProgName, GLuint& ShaderProg);
	void  GetUniformBlockIndex(GLuint& Program, GLuint BlockIndex, const GLuint BindingIndex, const char* Name, FString ProgramName);
	void  GetUniformLocation(GLuint& Uniform, GLuint& Program, const char* Name, FString ProgramName);

	//
	// Editor Hit Testing
	//
	void  LockHit(BYTE* InHitData, INT* InHitSize);
	void  UnlockHit(UBOOL Blit);
	void  SetSceneNodeHit(FSceneNode* Frame);
	bool  HitTesting() { return HitData != NULL; }

	//
	// DrawComplexSurface Support
	//
	DrawComplexShaderDrawParams* DrawComplexGetDrawParamsRef();
	void DrawComplexVertsSinglePass(DrawComplexBuffer& BufferData);
	void DrawComplexEnd(INT NextProgram);
	void DrawComplexStart();

	//
	// DrawGouraud Support
	//
	DrawGouraudShaderDrawParams* DrawGouraudGetDrawParamsRef();
	static void DrawGouraudBufferVert(DrawGouraudBufferedVert* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& Buffer);
	void  DrawGouraudSetState(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags);
	void  DrawGouraudReleaseState(FTextureInfo& Info);
	void  DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer& BufferData);
	void  DrawGouraudEnd(INT NextProgram);
	void  DrawGouraudStart();

	//
	// DrawTile Support
	//
	DrawTileShaderDrawParams* DrawTileGetDrawParamsRef();
	void  DrawTileVerts();
	void  DrawTileEnd(INT NextProgram);
	void  DrawTileStart();

	//
	// DrawSimple Support
	//
	void  DrawSimpleGeometryVerts(DrawSimpleMode DrawMode, GLuint size, INT Mode, DWORD LineFLags, FPlane DrawColor, bool BufferedDraw);
	void  DrawSimpleBufferLines(FLOAT* DrawLinesTemp, FLOAT* LineData);
	void  DrawSimpleEnd(INT NextProgram);
	void  DrawSimpleStart();

	//
	// Error logging
	//
#ifdef WIN32
	static void CALLBACK DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#else
    static void DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#endif
};

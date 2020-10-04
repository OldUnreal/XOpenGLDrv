/*=============================================================================
	XOpenGLDrv.h: Unreal OpenGL support header.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Enables CHECK_GL_ERROR(). Deprecated, should use UseOpenGLDebug=True instead, but still may be handy to track something specific down.
//#define DEBUGGL 1

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
	#define GLAD 1
	#include <windows.h>
	#include <mmsystem.h>

	#ifdef GLAD
		extern "C"
		{
		#include "glad.h"
		}
		#include "glext.h" // from https://khronos.org/registry/OpenGL/index_gl.php
		#include "wglext.h"
	#endif

	#ifdef SDL2BUILD
		#include <SDL2/SDL.h>
	#elif QTBUILD

	#endif
#else
    #define SDL2BUILD 1
    #ifdef __LINUX_ARM__
        //On init Glew uses OpenGL instead of GL ES, or fails entirely if SDL2 is built with ES support only, which forces ES only platforms like the ODROID-XU4 to fall back to Mesa.
        //Glew doesn't seem to work in an ES only environment and officially it does not support ES.
        //A solely ES based context can be made using glad, so gonna use glad here.
    #endif
		#define GLAD 1
	#ifdef GLAD
		extern "C"
		{
			#include "glad.h"
		}
	#endif

    #include <SDL2/SDL.h>
#endif

#include "XOpenGLTemplate.h" //thanks han!

#if ENGINE_VERSION==436 || ENGINE_VERSION==430
#define clockFast(Timer)   {Timer -= appCycles();}
#define unclockFast(Timer) {Timer += appCycles()-34;}
#elif ENGINE_VERSION==227
#define XOPENGL_REALLY_WANT_NONCRITICAL_CLEANUP 1
#define XOPENGL_BINDLESS_TEXTURE_SUPPORT 1
#elif UNREAL_TOURNAMENT_OLDUNREAL
#define XOPENGL_BINDLESS_TEXTURE_SUPPORT 1 // stijn: benchmarked on 16 JUN 2020. This has no statistically significant effect on performance in UT469
#endif

/*-----------------------------------------------------------------------------
Globals.
-----------------------------------------------------------------------------*/
#define MAX_FRAME_RECURSION 4

#define DRAWSIMPLE_SIZE 262144
#define DRAWTILE_SIZE 524288
#define DRAWCOMPLEX_SIZE 262144
#define DRAWGOURAUDPOLY_SIZE 1048576
#define DRAWGOURAUDPOLYLIST_SIZE 262144
#define NUMBUFFERS 6
#define NUMTEXTURES 4096
#define MAX_LIGHTS 256

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

enum TexType
{
	NORMALTEX,
	DETAILTEX,
	MACROTEX,
	LIGHTMAP,
	FOGMAP,
	ENVIRONMENTMAP,
	BUMPMAP,
	SHADOWMAP
};

//these values have to match the corresponding location in shader (such as "layout (location = 0) in vec3 v_coord;")
enum AttribType
{
	VERTEX_COORD_ATTRIB,
	NORMALS_ATTRIB,
	TEXTURE_COORD_ATTRIB,
	LIGHTMAP_COORD_ATTRIB,
	FOGMAP_COORD_ATTRIB,
	TEXTURE_ATTRIB,
	MACRO_COORD_ATTRIB,
	COLOR_ATTRIB,
	BINDLESS_TEXTURE_ATTRIB,
	TEXTURE_COORD_ATTRIB2,
	TEXTURE_COORD_ATTRIB3,
	MAP_COORDS_ATTRIB
};

enum ShaderProgType
{
	No_Prog,
	Simple_Prog,
	Tile_Prog,
	GouraudPolyVert_Prog,
	GouraudPolyVertList_Prog,
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

// stijn: missing defs in UT469 tree
#ifdef UNREAL_TOURNAMENT_OLDUNREAL
#define PF_AlphaBlend 0x2000
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
#if ENGINE_VERSION==227
	if (PolyFlags & PF_HeightMap)
        String+=TEXT("PF_HeightMap ");
#endif
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

enum DrawFlags
{
	DF_DiffuseTexture	= 0x00000001,
	DF_DetailTexture	= 0x00000002,
	DF_MacroTexture	 	= 0x00000004,
	DF_BumpMap			= 0x00000008,
	DF_LightMap         = 0x00000010,
	DF_FogMap          	= 0x00000020,
	DF_EnvironmentMap   = 0x00000040,
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
			debugf(TEXT("XOpenGL Error: %ls (%i) file %ls at line %i"), Msg, glErr, appFromAnsi(file), line);
		}
		return 1;
	}

	// Information about a cached texture.
	struct FCachedTexture
	{
		GLuint Ids[2]; // 0:Unmasked, 1:Masked.
		INT BaseMip;
		INT MaxLevel;
		GLuint Sampler[2];
		GLuint64 TexHandle[2];
		GLuint TexNum[2];
#if UNREAL_TOURNAMENT_OLDUNREAL
		INT RealtimeChangeCount;
#endif
		FCachedTexture()
			: Ids(),
			BaseMip(0),
			MaxLevel(0),
			Sampler(),
			TexHandle(),
			TexNum()
		{}
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

	#if ENGINE_VERSION==227 || (ENGINE_VERSION>436&&ENGINE_VERSION<1100)
	TArray<FLightInfo*> LightCache;
	#endif
	TArray<AActor*> StaticLightList;


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
		DWORD ProgramCycles;
		DWORD ComplexCycles;
		DWORD Draw2DLine;
		DWORD Draw3DLine;
		DWORD Draw2DPoint;
		DWORD GouraudPolyCycles;
		DWORD GouraudPolyListCycles;
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

    // Config
	BITFIELD NoFiltering;
    BITFIELD ShareLists;
    BITFIELD AlwaysMipmap;
    BITFIELD UsePrecache;
    BITFIELD UseTrilinear;
    BITFIELD UseAA;
    BITFIELD GammaCorrectScreenshots;
    BITFIELD MacroTextures;
    BITFIELD BumpMaps;
    BITFIELD NoAATiles;
    BITFIELD GenerateMipMaps;
    BITFIELD SyncToDraw;
    BITFIELD UseOpenGLDebug;
    BITFIELD NoBuffering;
    BITFIELD NoDrawComplexSurface;
    BITFIELD NoDrawGouraud;
    BITFIELD NoDrawGouraudList;
    BITFIELD NoDrawTile;
    BITFIELD NoDrawSimple;
    BITFIELD UseHWLighting;
    BITFIELD UseHWClipping;
	BITFIELD UseEnhandedLightmaps;

    //OpenGL 4 Config
	BITFIELD UseBindlessTextures;
	BITFIELD UsePersistentBuffers;
	BITFIELD UseBufferInvalidation;

	// Not really in use...(yet)
	BITFIELD UseMeshBuffering; //Buffer (Static)Meshes for drawing.
	BITFIELD UseSRGBTextures;
	BITFIELD EnvironmentMaps;

    // Internal stuff
    BITFIELD UsePersistentBuffersGouraud;
	BITFIELD UsePersistentBuffersComplex;
	BITFIELD UsePersistentBuffersTile;
	BITFIELD AMDMemoryInfo;
	BITFIELD NVIDIAMemoryInfo;
	BITFIELD SwapControlExt;
	BITFIELD SwapControlTearExt;

	INT MaxTextureSize;
	BYTE OpenGLVersion;
	BYTE UseVSync;
	bool NeedsInit;
	bool bMappedBuffers;
	bool bInitializedShaders;
	BYTE* ScaleByte;

	#if ENGINE_VERSION==227
	FLightInfo *FirstLight,*LastLight;
	#endif

	FLOAT GammaOffsetScreenshots;
	FLOAT LODBias;

	TMap<QWORD,INT> MeshMap;

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
	TArray<INT> GLHitData;
	struct FTexInfo
	{
		QWORD CurrentCacheID;
		INT CurrentCacheSlot;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
		FLOAT TexNum;
	FTexInfo()
		:CurrentCacheID(0),
		CurrentCacheSlot(0),
		UMult(0),
		VMult(0),
		UPan(0),
		VPan(0),
		TexNum(0.f)
		{}
	} TexInfo[8];
	FLOAT RFX2, RFY2;

	INT ActiveProgram;
	static DWORD ComposeSize;
	static BYTE* Compose;

	static const INT FloatsPerVertex = 3;

	static const GLuint FloatSize1 = (sizeof(float));
	static const GLuint FloatSize2 = (sizeof(float) * 2);// f.e. TexCoords2D;
	static const GLuint FloatSize3 = (sizeof(float) * 3);// f.e. FloatsPerVertex
	static const GLuint FloatSize4 = (sizeof(float) * 4);// f.e. ColorInfo

	// Don't laugh. Helps to keep overview, just needs to fit UXOpenGLRenderDevice::Buffer...
    GLuint FloatSize3_2				= FloatSize3 + FloatSize2;
    GLuint FloatSize3_2_4			= FloatSize3 + FloatSize2 + FloatSize4;
	GLuint FloatSize3_2_4_1			= FloatSize3 + FloatSize2 + FloatSize4 + FloatSize1;
	GLuint FloatSize3_2_4_4			= FloatSize3 + FloatSize2 + 2 * FloatSize4;
	GLuint FloatSize3_2_4_4_2		= FloatSize3 + FloatSize2 + 2 * FloatSize4 + FloatSize2;
	GLuint FloatSize3_2_4_4_2_2		= FloatSize3 + FloatSize2 + 2 * FloatSize4 + 2 * FloatSize2;
	GLuint FloatSize3_2_4_4_2_2_3	= FloatSize3 + FloatSize2 + (2 * FloatSize4) + (2 * FloatSize2) + FloatSize3;
	GLuint FloatSize3_2_4_4_3		= FloatSize3 + FloatSize2 + 2 * FloatSize4 + FloatSize3;
	GLuint FloatSize3_2_4_4_3_2		= FloatSize3 + FloatSize2 + 2 * FloatSize4 + FloatSize3 + FloatSize2;
	GLuint FloatSize3_2_4_4_4		= FloatSize3 + FloatSize2 + 3 * FloatSize4;
	GLuint FloatSize3_2_4_4_4_3		= FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3;
	GLuint FloatSize3_2_4_4_4_3_2	= FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3 + FloatSize2;
	GLuint FloatSize3_2_4_4_4_3_4	= FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3 + FloatSize4;
	GLuint FloatSize3_2_4_4_4_3_2_4 = FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3 + FloatSize2 + FloatSize4;
	GLuint FloatSize3_2_4_4_4_3_4_4 = FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3 + FloatSize4 + FloatSize4;
	GLuint FloatSize3_2_4_4_4_3_2_4_4 = FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3 + FloatSize2 + FloatSize4 + FloatSize4;
	GLuint FloatSize3_2_4_4_4_3_4_4_4 = FloatSize3 + FloatSize2 + 3 * FloatSize4 + FloatSize3 + FloatSize4 + FloatSize4 + FloatSize4;
	GLuint FloatSize3_3				= 2 * FloatSize3;
	GLuint FloatSize3_3_4			= 2 * FloatSize3 + FloatSize4;
	GLuint FloatSize3_3_4_4			= 2 * FloatSize3 + 2 * FloatSize4;
	GLuint FloatSize3_3_4_4_2		= 2 * FloatSize3 + 2 * FloatSize4 + FloatSize2;
	GLuint FloatSize3_3_4_4_2_16	= 2 * FloatSize3 + 2 * FloatSize4 + FloatSize2 + 4 * FloatSize4;
    GLuint FloatSize3_4				= FloatSize3 + FloatSize4;
    GLuint FloatSize3_4_4			= FloatSize3 + 2 * FloatSize4;
    GLuint FloatSize3_4_4_4			= FloatSize3 + 3 * FloatSize4;
    GLuint FloatSize3_4_4_4_4		= FloatSize3 + 4 * FloatSize4;
	GLuint FloatSize3_4_4_4_4_1		= FloatSize3 + 4 * FloatSize4 + FloatSize1;
	GLuint FloatSize4_4				= 2 * FloatSize4;

	GLuint DrawTileCoreStrideSize	= FloatSize3_4_4_4_4_1;
	GLuint DrawTileESStrideSize		= FloatSize3_2_4_1;
	GLuint DrawComplexStrideSize	= FloatSize4_4;
	GLuint DrawGouraudStrideSize	= FloatSize3_2_4_4_4_3_4_4_4;


	//DrawSimple
	struct DrawLinesBuffer
	{
		GLuint VertSize;
		FPlane DrawColor;
		DWORD LineFlags;
		DrawLinesBuffer()
			: VertSize(0),
			DrawColor(0.f,0.f,0.f,0.f),
			LineFlags(0)
		{}
	}DrawLinesBufferData;

	//PMB's
	struct BufferRange
	{
		GLsync Sync[NUMBUFFERS];
		FLOAT* Buffer;
		FLOAT* VertBuffer;
		GLuint64* UniformBuffer;
		BufferRange()
		: Sync()
		{}
	};

	BufferRange DrawGouraudBufferRange;
	BufferRange DrawGouraudListBufferRange;

    BufferRange DrawComplexSinglePassRange;
    BufferRange DrawTileRange;

	GLuint DrawSimpleVertObject, DrawSimpleGeoObject,DrawSimpleFragObject;
	GLuint DrawSimpleProg;

	GLuint DrawTileVertObject, DrawTileGeoObject, DrawTileFragObject;
	GLuint DrawTileProg;

	// Draw everything for ComplexSurface in one pass
	GLuint DrawComplexVertSinglePassObject, DrawComplexFragSinglePassObject, DrawComplexGeoSinglePassObject;
	GLuint DrawComplexProg;

	// DrawGouraud
	GLuint DrawGouraudVertObject, DrawGouraudGeoObject, DrawGouraudFragObject;
	GLuint DrawGouraudMeshBufferVertObject, DrawGouraudMeshBufferFragObject;
	GLuint DrawGouraudProg;
	GLuint DrawGouraudMeshBufferProg;

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

	//DrawTile
	struct DrawTileBuffer
	{
		glm::vec4 TexCoords[3];
		GLuint VertSize;
		GLuint TexSize;
		GLuint ColorSize;
		GLuint FloatSize1;
        GLuint Index;
		GLuint IndexOffset;
		GLuint BeginOffset;
		DWORD PolyFlags;
		DWORD PrevPolyFlags;
		DrawTileBuffer()
			: TexCoords(),
			VertSize(0),
			TexSize(0),
			ColorSize(0),
			FloatSize1(0),
            Index(0),
			IndexOffset(0),
			BeginOffset(0),
			PolyFlags(0),
            PrevPolyFlags(0)
		{}
	}DrawTileBufferData;

	struct DrawGouraudBuffer
	{
		GLuint VertSize;
		GLuint Index;
		GLuint IndexOffset;
		GLuint BeginOffset;
		DWORD PolyFlags;
		DWORD PrevPolyFlags;
		DWORD RendMap;
		FLOAT TexUMult;
		FLOAT TexVMult;
		FLOAT DetailTexUMult;
		FLOAT DetailTexVMult;
		FLOAT MacroTexUMult;
		FLOAT MacroTexVMult;
		FLOAT Alpha;
		GLuint DrawFlags;
		GLfloat TextureFormat;
		GLfloat TextureAlpha;
		GLuint TexNum[8];
		FPlane DrawColor;
		FLOAT TextureDiffuse;
		FLOAT BumpTextureSpecular;
		FLOAT MacroTextureDrawScale;
		FFogSurf FogSurf;
		DrawGouraudBuffer()
			: VertSize(0),
			Index(0),
			IndexOffset(0),
			BeginOffset(0),
			PolyFlags(0),
			PrevPolyFlags(0),
			RendMap(0),
			TexUMult(0.f),
			TexVMult(0.f),
			DetailTexUMult(0.f),
			DetailTexVMult(0.f),
			MacroTexUMult(0.f),
			MacroTexVMult(0.f),
			Alpha(0.f),
			DrawFlags(0),
			TextureFormat(0.f),
			TextureAlpha(0.f),
			TexNum(),
			DrawColor(0.f, 0.f, 0.f, 0.f),
			TextureDiffuse(0.f),
			BumpTextureSpecular(0.f),
			MacroTextureDrawScale(0.f),
			FogSurf()
		{}
	}DrawGouraudBufferData;
	DrawGouraudBuffer DrawGouraudListBufferData;

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

	struct DrawComplexTexMaps
	{
		FPlane SurfNormal;
		glm::vec4 TexCoords[16];
		FCoords MapCoords;
		GLuint DrawFlags;
		DrawComplexTexMaps()
			: SurfNormal(0.f, 0.f, 0.f, 0.f),
			TexCoords(),
			MapCoords(),
			DrawFlags(0)
		{}
	}TexMaps;

	struct DrawComplexBuffer
	{
		GLuint VertSize;
		GLuint TexSize;
		GLuint Index;
		GLuint IndexOffset;
		GLuint BeginOffset;
		GLuint Iteration;
		DWORD PolyFlags;
		DWORD PrevPolyFlags;
		DWORD RendMap;
		FPlane DrawColor;
		GLuint TexNum[8];
		DrawComplexBuffer()
			: VertSize(0),
			TexSize(0),
			Index(0),
			IndexOffset(0),
			BeginOffset(0),
			Iteration(0),
			PolyFlags(0),
			PrevPolyFlags(0),
			RendMap(0),
			DrawColor(0.f, 0.f, 0.f, 0.f),
			TexNum()
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

	//DrawGouard
	INT BufferCount;

	// Matrices calculation
	glm::vec3 cameraPosition;
	glm::vec3 cameraDirection;
	glm::vec3 cameraTarget;
	glm::vec3 cameraUp;

	//Matrices
	glm::mat4 projMat;
	glm::mat4 viewMat;
	glm::mat4 modelMat;
	glm::mat3 m_3x3_inv_transp;
	glm::mat4 modelviewMat;
	glm::mat4 viewMatinv;
	glm::mat4 gouraudmodelMat;
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
    GLuint GlobalUniformTextureHandlesIndex;
    GLuint GlobalTextureHandlesUBO;
    BufferRange GlobalUniformTextureHandles;
	static const GLuint GlobalTextureHandlesBindingIndex = 1;

	// Global DistanceFog.
	GLuint GlobalUniformDistanceFogIndex;
	GLuint GlobalDistanceFogUBO;
	static const GLuint GlobalDistanceFogBindingIndex = 2;

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

	glm::vec4 DistanceFogColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	glm::vec4 DistanceFogValues = glm::vec4(0.f, 0.f, 0.f, 0.f);
	bool bFogEnabled = false;

	FCachedTexture* MapTextureData[8];
    GLuint TexNum;

	// Editor
	GLuint DrawSimplebHitTesting;
	GLuint DrawTilebHitTesting;
	GLuint DrawGouraudbHitTesting;
	GLuint DrawGouraudRendMap;
	GLuint DrawComplexSinglePassbHitTesting;
	GLuint DrawComplexSinglePassRendMap;

	//DrawComplexSinglePass
	GLuint DrawComplexSinglePassFogMap;
	GLuint DrawComplexSinglePassLightPos;
	GLuint DrawComplexSinglePassTexCoords;

	//DrawGouraud Matrix
	GLuint DrawGouraudMeshBufferModelMat;
	GLuint DrawGouraudMeshBufferViewMat;
	GLuint DrawGouraudMeshBufferProjMat;
	GLuint DrawGouraudMeshBufferViewMatinv;
	GLuint DrawGouraudLightPos;

	//Drawing colors
	GLuint DrawSimpleDrawColor;
	GLuint DrawTileHitDrawColor;
	GLuint DrawComplexSinglePassDrawColor;
	GLuint DrawGouraudDrawColor;
	GLuint DrawGouraudMeshBufferDrawColor;

	//Texture vars
	GLuint DrawTileTexture;
	GLuint DrawGouraudTexture[8];
	GLuint DrawComplexSinglePassTexture[8];

	// LineFlags for simple drawing.
	GLuint DrawSimpleLineFlags;

	// PolyFlags for shaders.
	GLuint DrawTilePolyFlags;
	GLuint DrawComplexSinglePassPolyFlags;
	GLuint DrawGouraudPolyFlags;

	// TexNum for bindless textures in shaders.
	GLuint DrawTileTexNum;
	GLuint DrawComplexSinglePassTexNum;
	GLuint DrawGouraudTexNum;

	// Gamma handling
	static FLOAT Gamma;
	GLuint DrawSimpleGamma;
	GLuint DrawTileGamma;
	GLuint DrawComplexSinglePassGamma;
	GLuint DrawGouraudGamma;

	//Vertices
	GLuint DrawSimpleVertBuffer;
	GLuint DrawTileVertBuffer;
	GLuint DrawComplexVertBuffer;
	GLuint DrawGouraudVertBuffer;
	GLuint DrawGouraudVertListBuffer;
	GLuint DrawGouraudVertBufferInterleaved;

	//VAO's
	GLuint DrawSimpleGeometryVertsVao;
	GLuint DrawTileVertsVao;
	GLuint DrawGouraudPolyVertsVao;
	GLuint DrawGouraudPolyVertListVao;
	GLuint DrawGouraudPolyVertsSingleBufferVao;
	GLuint DrawGouraudPolyVertListSingleBufferVao;
	GLuint DrawComplexVertsSingleBufferVao;
	GLuint DrawComplexVertListSingleBufferVao;
	GLuint DrawComplexVertsSinglePassVao;
	GLuint SimpleDepthVao;

    GLuint DrawComplexSinglePassFogColor;
    GLuint DrawComplexSinglePassFogStart;
    GLuint DrawComplexSinglePassFogEnd;
    GLuint DrawComplexSinglePassFogDensity;
    GLuint DrawComplexSinglePassFogMode;

    GLuint DrawGouraudFogColor;
    GLuint DrawGouraudFogStart;
    GLuint DrawGouraudFogEnd;
    GLuint DrawGouraudFogDensity;
    GLuint DrawGouraudFogMode;

	struct GouraudBufferData
	{
		AActor* Owner;
		GLuint totalsize;
		GLuint vertssize;
		GLuint TexSize;
		GLuint colorsize;

		GLuint vertbuffer;
		GLuint texbuffer;
		GLuint normalbuffer;
		GLuint colorbuffer;
		GLuint lightmapbuffer;

		UBOOL bBuffered;
	};
	TArray<GouraudBufferData> GouraudBuffer;

	// Texture UV
	GLuint DrawGouraudTexUV;
	GLuint DrawGouraudDetailTexUV;

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
	UBOOL HaveOriginalRamp;
	FGammaRamp OriginalRamp; // to restore original value at exit or crash.

	GLfloat NumClipPlanes;
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
	FString AllExtensions;
#endif

	// UObject interface.
	void StaticConstructor();
	void PostEditChange();

	UBOOL CreateOpenGLContext(UViewport* Viewport, INT NewColorBytes);
	UBOOL SetWindowPixelFormat();
	UBOOL FindExt( const TCHAR* Name );
	void FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt );
	void FindProcs( UBOOL AllowExt );

    #ifdef _WIN32
	void PrintFormat( HDC hDC, INT nPixelFormat );
	#endif

    UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	UBOOL InitGLEW(HINSTANCE hInstance);

	static QSORT_RETURN CDECL CompareRes(const FPlane* A, const FPlane* B) {
		return (QSORT_RETURN) (((A->X - B->X) != 0.0f) ? (A->X - B->X) : (A->Y - B->Y));
	}

	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize);
	void UpdateCoords(FSceneNode* Frame);
	void SetSceneNode(FSceneNode* Frame);
	void SetOrthoProjection(FSceneNode* Frame);
	void SetProjection(FSceneNode* Frame, UBOOL bNearZ);
	void Unlock(UBOOL Blit);
	void Flush(UBOOL AllowPrecache);
	void SetPermanentState();

	void LockBuffer(BufferRange& Buffer, GLuint Index);
	void WaitBuffer(BufferRange& Buffer, GLuint Index);
	void MapBuffers();
	void UnMapBuffers();
	UBOOL GLExtensionSupported(FString Extension_Name);
	void CheckExtensions();

	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet);
	void BufferComplexSurfacePoint( FLOAT* DrawComplexTemp, FTransform* P, DrawComplexTexMaps TexMaps, DrawComplexBuffer& BufferData );
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span=NULL);
	void BufferGouraudPolygonPoint(FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& Buffer );
	void BufferGouraudPolygonVert(FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& Buffer);
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);
	void BufferTiles(FLOAT* DrawTilesTemp, FLOAT* TileData);
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);
	void BufferLines(FLOAT* DrawLinesTemp, FLOAT* LineData);
	void DrawPass(FSceneNode* Frame, INT Pass);
	void ClearZ(FSceneNode* Frame);
	void GetStats(TCHAR* Result);
	void ReadPixels(FColor* Pixels);
	void EndFlash();
	void SwapControl();
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags);

	BYTE PushClipPlane(const FPlane& Plane);
	BYTE PopClipPlane();

#if UNREAL_TOURNAMENT_OLDUNREAL
	void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span);
	UBOOL SupportsTextureFormat(ETextureFormat Format);
#endif      

	// Editor
	void PushHit(const BYTE* Data, INT Count);
	void PopHit(INT Count, UBOOL bForce);
	void LockHit(BYTE* InHitData, INT* InHitSize);
	void UnlockHit(UBOOL Blit);
	void SetSceneNodeHit(FSceneNode* Frame);
	bool HitTesting() { return HitData != NULL; }

	void SetProgram( INT CurrentProgram );
	void DrawProgram();
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();
	void MakeCurrent();

	void SetTexture( INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, INT ShaderProg, TexType TextureType ); //First parameter has to fit the uniform in the fragment shader
	void SetNoTexture( INT Multi );
	DWORD SetFlags(DWORD PolyFlags);
	void SetBlend(DWORD PolyFlags, bool InverseOrder);
	DWORD SetDepth(DWORD PolyFlags);
	void SetSampler(GLuint Multi, DWORD PolyFlags, UBOOL SkipMipmaps, BYTE UClampMode, BYTE VClampMode);

	void BuildGammaRamp(FLOAT GammaCorrection, FGammaRamp& Ramp);
	void BuildGammaRamp(FLOAT GammaCorrection, FByteGammaRamp& Ramp);
	void SetGamma(FLOAT GammaCorrection);
	BYTE SetZTestMode(BYTE Mode);

	// DistanceFog
	void PreDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf);
	void PostDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf);
	void ResetFog();

#if ENGINE_VERSION==227
	// Lighting
	void RenderLighting(FSceneNode* Frame, UTexture* SurfaceTexture, UTexture* DetailTexture, DWORD PolyFlags, UBOOL bDisableLights);
	INT	LightNum;
#endif

	// Shader stuff
	void InitShaders();
	void DeleteShaderBuffers();
	void LoadShader(const TCHAR* Filename, GLuint &ShaderObject);
	void LinkShader(const TCHAR* ShaderProgName, GLuint &ShaderProg);
	void GetUniformBlockIndex(GLuint &Program, GLuint BlockIndex, const GLuint BindingIndex, const char* Name, FString ProgramName);
	void GetUniformLocation(GLuint &Uniform, GLuint &Program, const char* Name, FString ProgramName);
	void DrawSimpleGeometryVerts(DrawSimpleMode DrawMode, GLuint size, INT Mode, DWORD LineFLags, FPlane DrawColor, bool BufferedDraw);
	void DrawTileVerts(DrawTileBuffer &BufferData);
	void DrawComplexVertsSinglePass(DrawComplexBuffer &BufferData, DrawComplexTexMaps TexMaps);
	void BufferGouraudPolyVerts( FLOAT* verts, GLuint VertSize, GLuint TexSize, GLuint colorsize, FPlane DrawColor);
	void DrawGouraudPolyBuffer();
	void DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer &BufferData);

	void Exit();
	void ShutdownAfterError();

	// OpenGLStats
	void DrawStats( FSceneNode* Frame );

	// Error logging
	public:
#ifdef WIN32
	static void CALLBACK DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#else
    static void DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#endif
};

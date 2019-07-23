/*=============================================================================
	XOpenGLDrv.h: Unreal OpenGL support header.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Enables CHECK_GL_ERROR(). Deprecated, should use UseOpenGLDebug=True instead
//#define DEBUGGL 1

// Maybe for future release. Not in use yet.
//#define QTBUILD 1

// Windows only.
//#define WINBUILD 1

// Linux/OSX mostly, needs to be set in Windows for SDL2Launch.
//#define SDL2BUILD 1


#ifdef WIN32
	#define WINBUILD 1
	#include <windows.h>
	#ifdef SDL2BUILD
		#include <SDL2/SDL.h>
	#endif

	#ifdef GLAD
		extern "C" {
			#include "glad.h"
		}
	#else		
		#include <GL/glew.h>
		#include <GL/wglew.h>		
		#include <GL/glcorearb.h>
		#include <GL/gl.h>
		#include <GL/glext.h>
		// fix for missing function type defs...
		#ifndef PFNGLBINDTEXTUREPROC
			typedef void (APIENTRYP PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
		#endif
		#ifndef PFNGLDELETETEXTURESPROC
			typedef void (APIENTRYP PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint* textures);
		#endif
		#ifndef PFNGLDRAWARRAYSPROC
			typedef void (APIENTRYP PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
		#endif
		#ifndef PFNGLGENTEXTURESPROC
			typedef void (APIENTRYP PFNGLGENTEXTURESPROC) (GLsizei n, GLuint* textures);
		#endif
		#ifndef PFNGLPOLYGONOFFSETPROC
			typedef void (APIENTRYP PFNGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat units);
		#endif
		#ifndef PFNGLTEXSUBIMAGE2DPROC
			typedef void (APIENTRYP PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
		#endif
	#endif
	#include <mmsystem.h>
#elif __GNUG__ || __clang__
	#define SDL2BUILD 1
	#define GL_GLES_PROTOTYPES 1
	#include "gles3.h"
	#include "SDL.h"
#endif

#include "XOpenGLTemplate.h" //thanks han!

/*-----------------------------------------------------------------------------
Globals.
-----------------------------------------------------------------------------*/
#define MAX_FRAME_RECURSION 4

#define DRAWSIMPLE_SIZE 65536
#define DRAWTILE_SIZE 65536
#define DRAWCOMPLEX_SIZE 65536
#define DRAWGOURAUDPOLYLIST_SIZE 262144
#define VBO_SIZE 65536

#define SHADOW_MAP_COEF 0.5
#define BLUR_COEF 0.25

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
#ifndef GL_COMPRESSED_RED_RGTC1
	#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#endif
#ifndef GL_COMPRESSED_SIGNED_RED_RGTC1
	#define GL_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#endif
#ifndef GL_COMPRESSED_RG_RGTC2
	#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#endif
#ifndef GL_COMPRESSED_SIGNED_RG_RGTC2
	#define GL_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
#endif
#ifndef GL_STACK_OVERFLOW
	#define GL_STACK_OVERFLOW 0x0503
#endif
#ifndef GL_STACK_UNDERFLOW
	#define GL_STACK_UNDERFLOW 0x0504
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
	#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
	#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
	#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
	#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
#ifndef GL_MAX_DUAL_SOURCE_DRAW_BUFFERS
	#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC
#endif
#ifndef GL_FRAMEBUFFER_SRGB
	#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif
#ifndef glInvalidateBufferData
	#define glInvalidateBufferData(x)
#endif
#ifndef GL_SRGB_EXT
	#define GL_SRGB_EXT 0x8C40
#endif
#ifndef GL_EXT_texture_sRGB
	#define GL_EXT_texture_sRGB 0
#endif
#ifndef GL_SRC1_COLOR
	#define GL_SRC1_COLOR 0x88F9
#endif
#ifndef GL_GEOMETRY_SHADER
	#define GL_GEOMETRY_SHADER 0x8DD9
#endif
#ifndef GL_ARB_buffer_storage
	#define GL_ARB_buffer_storage 0
#endif
#ifndef GL_ARB_invalidate_subdata
	#define GL_ARB_invalidate_subdata 0
#endif
//#ifndef gladLoadGLLoader
//    #define gladLoadGLLoader(x);
//#endif
#ifndef GL_MAP_WRITE_BIT
    #define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_PERSISTENT_BIT
    #define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
    #define GL_MAP_COHERENT_BIT 0x0080
#endif
#ifndef glBufferStorage
    #define glBufferStorage
#endif
#ifndef GL_BGRA
    #define GL_BGRA 0x80E1
#endif
// OpenGL specific extensions. ES usGL_EXT_texture_compression_dxt1, GL_ANGLE_texture_compression_dxt3,GL_ANGLE_texture_compression_dxt5
#ifndef GL_EXT_texture_compression_s3tc
    #define GL_EXT_texture_compression_s3tc 0
#endif

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
    #define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_TEXTURE_LOD_BIAS
    #define GL_TEXTURE_LOD_BIAS 0x8501
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_MAX_TEXTURE_LOD_BIAS_EXT
    #define GL_MAX_TEXTURE_LOD_BIAS_EXT 0x84FD
#endif
#ifndef GL_TEXTURE_FILTER_CONTROL_EXT
    #define GL_TEXTURE_FILTER_CONTROL_EXT 0x8500
#endif
#ifndef GL_TEXTURE_LOD_BIAS_EXT
    #define GL_TEXTURE_LOD_BIAS_EXT 0x8501
#endif

typedef void (GL_APIENTRYP PFNGLDEPTHRANGEPROC) (double n, double f);
typedef void (GL_APIENTRYP PFNGLCLEARDEPTHPROC) (double d);

enum TexType
{
	SURF_TEX,
	LIGHT_TEX,
	FOG_TEX,
	DETAIL_TEX,
	MACRO_TEX
};

//these values have to match the corresponding location in shader (such as "layout (location = 0) in vec3 v_coord;")
enum AttribType
{
	VERTEX_COORD_ATTRIB,
	NORMALS_ATTRIB,
	TEXTURE_COORD_ATTRIB,
	LIGHTMAP_COORD_ATTRIB,
	FOGMAP_COORD_ATTRIB,
	DETAIL_COORD_ATTRIB,
	MACRO_COORD_ATTRIB,
	BUMP_COORD_ATTRIB,
	COLOR_ATTRIB,
	DISTANCE_FOG_ATTRIB,
	VERTEX_ATTRIB2,
	VERTEX_ATTRIB3,
	VERTEX_ATTRIB4
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
	VS_Adaptive = 0,
	VS_Off      = 1,
	VS_On       = 2,
};
enum DrawSimpleMode
{
    DrawLineMode        = 0,
	Draw2DPointMode     = 1,
	Draw2DLineMode      = 2,
	Draw3DLineMode      = 3,
	DrawEndFlashMode    = 4,
};

#ifdef DEBUGGL
#define CHECK_GL_ERROR() CheckGLError(__FILE__, __LINE__)
#else
#define CHECK_GL_ERROR()
#endif

#if ENGINE_VERSION!=227
// Definition for a surface that has fog
struct FFogSurf
{
	FSavedPoly *Polys;
	FLOAT		FogDistanceStart;	// Zone fog start (zero if none)
	FLOAT		FogDistanceEnd;		// Zone fog distance (zero if none)
	FLOAT		FogDensity;			// for exponential fog.
	FPlane		FogColor;			// Zone fog color
	INT			FogMode;			// 0 Linear, 1 Exponential, 2 Exponential2
	DWORD		PolyFlags;			// Surface flags.
	UBOOL		bBSP;				// this flag is unfortunately needed for the old renderers to emulate the fog.
	FFogSurf()
		:Polys(NULL),
		FogDistanceStart(0.f),
		FogDistanceEnd(0.f),
		FogDensity(0.f),
		FogColor(FPlane(0.f,0.f,0.f,0.f)),
		FogMode(0),
		PolyFlags(0),
		bBSP(0)
	{}
};
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

/*-----------------------------------------------------------------------------
	XOpenGLDrv.
-----------------------------------------------------------------------------*/

class UXOpenGLRenderDevice : public URenderDevice
{
//#if ENGINE_VERSION==227
//	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config, XOpenGLDrv)
//#elif ENGINE_VERSION==430
	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config, XOpenGLDrv)
//#else
//	DECLARE_CLASS(UXOpenGLRenderDevice, URenderDevice, CLASS_Config)
//#endif

	// GL functions.
    public:
		UBOOL SUPPORTS_GL = 0;
		UBOOL SUPPORTS_GLCORE = 0;
		UBOOL SUPPORTS_GLES = 0;
		UBOOL AllowExt = 1;
	#define GL_EXT(name) UBOOL SUPPORTS_##name;
	#ifndef __EMSCRIPTEN__
	#define GL_PROC(ext,fntype,fnname) fntype fnname;
	#endif
	#include "XOpenGLFuncs.h"

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
			debugf(TEXT("OpenGL Error: %s (%i) file %s at line %i"), Msg, glErr, appFromAnsi(file), line);
		}
		return 1;
	}

	// Information about a cached texture.
	struct FCachedTexture
	{
		GLuint Ids[2]; // 0:Unmasked, 1:Masked.
		INT BaseMip;
		INT MaxLevel;
		GLuint Sampler;
		FCachedTexture()
			: Ids(),
			BaseMip(0),
			MaxLevel(0)
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
	INT PassNum;

	// Permanent variables.
	#ifdef _WIN32
	HGLRC hRC;
	HWND hWnd;
	HDC hDC;
	#endif
	UBOOL WasFullscreen;
	TOpenGLMap<QWORD,FCachedTexture> LocalBindMap, *BindMap;
	TArray<FPlane> Modes;
	UViewport* Viewport;

	#if ENGINE_VERSION==227
	TArray<FLightInfo*> LightCache;
	#endif

	// Timing.
	struct FGLStats
	{
		DWORD BindCycles;
		DWORD ImageCycles;
		DWORD BlendCycles;
		DWORD ProgramCycles;
		DWORD ComplexCycles;
		DWORD GouraudPolyCycles;
		DWORD GouraudPolyListCycles;
		DWORD TileCycles;
		DWORD TriangleCycles;
		DWORD Resample7777Cycles;
	} Stats;

	// Hardware constraints.
	INT RefreshRate;
	FLOAT MaxAnisotropy;
	INT DebugLevel;
	INT NumAASamples;
	UBOOL UseHWLighting;
	UBOOL UseHWClipping;
	UBOOL UseMeshBuffering; //Buffer (Static)Meshes for drawing.
	UBOOL UseZTrick;
	UBOOL UsePrecache;
	UBOOL UsePalette;
	UBOOL ShareLists;
	UBOOL AlwaysMipmap;
	UBOOL UseTrilinear;
	UBOOL UseAlphaPalette;
	UBOOL NoFiltering;
	UBOOL UseSRGBTextures;
	UBOOL UseOpenGLDebug;
	UBOOL UseAA;
	UBOOL GammaCorrectScreenshots;
	UBOOL MacroTextures;
	UBOOL BumpMaps;
	UBOOL EnvironmentMaps;
	UBOOL NoBuffering;
	UBOOL NoAATiles;
	UBOOL GenerateMipMaps;
	INT MaxTextureSize;
	BYTE OpenGLVersion;
	BYTE VSync;
	static bool NeedGlewInit;
	static INT NumContexts;

	//OpenGL 4
	UBOOL UsePersistentBuffers;
	UBOOL UseBufferInvalidation;

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
	UBOOL ZTrickToggle;
	INT ZTrickFunc;
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

	FTexInfo()
		:CurrentCacheID(0),
		CurrentCacheSlot(0),
		UMult(0),
		VMult(0),
		UPan(0),
		VPan(0)
		{}
	} TexInfo[8];
	FLOAT RFX2, RFY2;

	static INT ActiveProgram;
	static DWORD ComposeSize;
	static BYTE* Compose;

	static const INT ColorInfo = 4;
	static const INT FloatsPerVertex = 3;
	static const INT TexCoords2D = 2;
	static const GLuint TexFloatSize =  (sizeof(float) * TexCoords2D);
	static const GLuint VertFloatSize = (sizeof(float) * FloatsPerVertex);
	static const GLuint ColorFloatSize = (sizeof(float) * ColorInfo);

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
		GLsync Sync;
		bool bSync;
		FLOAT* DrawGouraudPMB;
		FLOAT* DrawGouraudListPMB;
		BufferRange()
		: Sync(0)
		, bSync(false)
		{}
	};

	BufferRange DrawGouraudBufferRange[3];
	GLuint DrawGouraudIndex;
	bool bDrawGouraudBuffers;
	size_t DrawGouraudIndexOffset;

	//DrawGouraud buffering
	GLsizei DrawGouraudCounter;
	GLsizei DrawGouraudArrayCounts[65536];
	GLint DrawGouraudArrayFirsts[65536];
	GLuint DrawGouraudOffset;
	FLOAT* DrawGouraudBufferMap;
	GLuint DrawGouraudBufferElements;
	UTexture* DrawGouraudBufferTexture;
	GLsizei DrawGouraudTotalSize;

	GLuint DrawSimpleVertObject, DrawSimpleGeoObject,DrawSimpleFragObject;
	GLuint DrawSimpleProg;

	GLuint DrawTileVertObject, DrawTileGeoObject, DrawTileFragObject;
	GLuint DrawTileProg;

	// Draw everything for ComplexSurface in one pass
	GLuint DrawComplexVertSinglePassObject, DrawComplexFragSinglePassObject, DrawComplexGeoSinglePassObject;
	GLuint DrawComplexSinglePassProg;

	// Separate shader for hw lighting.
	GLuint DrawComplexVertHWLightsObject, DrawComplexFragHWLightsObject;
	GLuint DrawComplexHWLightsProg;

	// DrawGouraud
	GLuint DrawGouraudVertObject, DrawGouraudFragObject;
	GLuint DrawGouraudMeshBufferVertObject, DrawGouraudMeshBufferFragObject;
	GLuint DrawGouraudProg;
	GLuint DrawGouraudMeshBufferProg;

	//DrawTile
	struct DrawTilesBuffer
	{
		glm::vec4 TexCoords[3];
		GLuint VertSize;
		GLuint TexSize;
		GLuint ColorSize;
		GLuint TotalSize;
		FPlane DrawColor;
		DWORD PolyFlags;
		DrawTilesBuffer()
			: TexCoords(),
			VertSize(0),
			TexSize(0),
			ColorSize(0),
			TotalSize(0),
			DrawColor(0.f, 0.f, 0.f, 0.f),
			PolyFlags(0)
		{}
	}DrawTilesBufferData;

	struct DrawGouraudBuffer
	{
		GLuint VertSize;
		GLuint TexSize;
		GLuint ColorSize;
		GLuint Iteration;
		GLuint IndexOffset;
		DWORD PolyFlags;
		UTexture* Texture;
		UTexture* DetailTexture;
		FLOAT TexUMult;
		FLOAT TexVMult;
		FLOAT DetailTexUMult;
		FLOAT DetailTexVMult;
		bool bDetailTex;
		DrawGouraudBuffer()
			: VertSize(0),
			TexSize(0),
			ColorSize(0),
			Iteration(0),
			IndexOffset(0),
			Texture(NULL),
			DetailTexture(NULL),
			PolyFlags(0),
			TexUMult(0.f),
			TexVMult(0.f),
			DetailTexUMult(0.f),
			DetailTexVMult(0.f),
			bDetailTex(false)
		{}
	}DrawGouraudBufferData;
	DrawGouraudBuffer DrawGouraudBufferListData;

	struct DrawComplexTexMaps
	{
		bool bLightMap;
		bool bDetailTex;
		bool bMacroTex;
		bool bBumpMap;
		bool bFogMap;
		bool bEnvironmentMap;
		FPlane SurfNormal;
		glm::vec4 LightPos[8];
		glm::vec4 TexCoords[10];
		DrawComplexTexMaps()
			: bLightMap(false),
			bDetailTex(false),
			bMacroTex(false),
			bBumpMap(false),
			bFogMap(false),
			SurfNormal(0.f, 0.f, 0.f, 0.f),
			LightPos(),
			TexCoords()
		{}
	};

	struct DrawComplexBuffer
	{
		GLuint VertSize;
		GLuint TexSize;
		GLuint IndexOffset;
		DWORD PolyFlags;
		DWORD RendMap;
		DrawComplexBuffer()
			: VertSize(0),
			TexSize(0),
			IndexOffset(0),
			PolyFlags(0),
			RendMap(0)
		{}
	}DrawComplexBufferData;

	//DrawSimple
	FLOAT* DrawLinesVertsBuf;
	FLOAT* Draw2DLineVertsBuf;
	FLOAT* Draw2DPointVertsBuf;
	FLOAT* Draw3DLineVertsBuf;
	FLOAT* EndFlashVertsBuf;

	//DrawTile
	FLOAT* DrawTileVertsBuf;
	GLuint DrawTileTexCoords;

	//DrawGouard
	FLOAT* DrawGouraudBuf;
	INT BufferCount;
#if ENGINE_VERSION==227
	FLOAT* DrawGouraudPolyListVertsBuf;
	FLOAT* DrawGouraudPolyListVertNormalsBuf;
	FLOAT* DrawGouraudPolyListTexBuf;
	FLOAT* DrawGouraudPolyListLightColorBuf;
	FLOAT* DrawGouraudPolyListSingleBuf;
#endif

	//DrawComplex
	FLOAT* DrawComplexVertsSingleBuf;

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
	FLOAT StoredFovAngle;
	FLOAT StoredFX;
	FLOAT StoredFY;
	FLOAT StoredOrthoFovAngle;
	FLOAT StoredOrthoFX;
	FLOAT StoredOrthoFY;
	bool bIsOrtho;

	GLuint GlobalUniformBlockIndex;
	GLuint GlobalMatricesUBO;
	static const GLuint GlobalMatricesBindingIndex = 0;

	// Editor
	GLuint DrawSimplebHitTesting;
	GLuint DrawTilebHitTesting;
	GLuint DrawGouraudbHitTesting;
	GLuint DrawComplexSinglePassbHitTesting;
	GLuint DrawComplexSinglePassRendMap;

	//DrawComplexSinglePass
	GLuint DrawComplexSinglePassComplexAdds;
	GLuint DrawComplexSinglePassFogMap;
	GLuint DrawComplexSinglePassLightPos;
	GLuint DrawComplexSinglePassTexCoords;

	//DrawGouraud Matrix
	GLuint DrawGouraudMeshBufferModelMat;
	GLuint DrawGouraudMeshBufferViewMat;
	GLuint DrawGouraudMeshBufferProjMat;
	GLuint DrawGouraudMeshBufferViewMatinv;

	//Drawing colors
	GLuint DrawSimpleDrawColor;
	GLuint DrawTileDrawColor;
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

	// Gamma handling
	static FLOAT Gamma;
	GLuint DrawTileGamma;
	GLuint DrawComplexSinglePassGamma;
	GLuint DrawGouraudGamma;

	//Vertices
	GLuint DrawSimpleVertBuffer;
	GLuint DrawTileVertBuffer;
	GLuint DrawComplexVertBufferSinglePass;
	GLuint DrawGouraudVertBufferSingle;
	GLuint DrawGouraudVertBufferListSingle;
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

	// DistanceFog
	GLuint DrawComplexFogColor;
	GLuint DrawComplexFogStart;
	GLuint DrawComplexFogEnd;
	GLuint DrawComplexFogDensity;
	GLuint DrawComplexFogMode;

	GLuint DrawComplexLMFogColor;
	GLuint DrawComplexLMFogStart;
	GLuint DrawComplexLMFogEnd;
	GLuint DrawComplexLMFogDensity;
	GLuint DrawComplexLMFogMode;

	GLuint DrawComplexDTFogColor;
	GLuint DrawComplexDTFogStart;
	GLuint DrawComplexDTFogEnd;
	GLuint DrawComplexDTFogDensity;
	GLuint DrawComplexDTFogMode;

	GLuint DrawComplexMTFogColor;
	GLuint DrawComplexMTFogStart;
	GLuint DrawComplexMTFogEnd;
	GLuint DrawComplexMTFogDensity;
	GLuint DrawComplexMTFogMode;

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

	FFogSurf DrawComplexFogSurface;
	FFogSurf DrawGouraudFogSurface;

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

	//Lighting
	#define MAX_LIGHTS 32 //Unreal's max light preset.
	GLuint	NumLights;
	struct ShaderLightInfo
	{
		//Light location
		GLuint LightLocation;
		GLuint WorldLightRadius;
		GLuint linearAttenuation;
		GLuint quadraticAttenuation;
		GLuint LightRadius;

		//Ambient Lighting
		GLuint AmbientLightColor;

		//Diffuse Lighting
		GLuint DiffuseLightColor;

		//Specular Lighting
		GLuint SpecularLightColor;

		//Spot light
		GLuint spotDirection;

		//LightInfo
		GLuint LightValues;
	};
	ShaderLightInfo ShaderLights[MAX_LIGHTS+1];

	// Distance fog.

	#define FOG_EQUATION_LINEAR		0
	#define FOG_EQUATION_EXP		1
	#define FOG_EQUATION_EXP2		2

	struct FogParameters
	{
		FLOAT fDensity;
		FLOAT fStart;
		FLOAT fEnd;
		glm::vec4 vFogColor;
		int iFogEquation; // 0 = linear, 1 = exp, 2 = exp2
	};

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

	// UObject interface.
	void StaticConstructor();
	void PostEditChange();

	void CreateOpenGLContext(UViewport* Viewport, INT NewColorBytes);

	UBOOL FindExt(const TCHAR* Name);
	void FindProc(void*& ProcAddress, const char* Name, const char* SupportName, UBOOL& Supports, UBOOL AllowExt);
	void FindProcs(UBOOL AllowExt);

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
	void SetSceneNode(FSceneNode* Frame);
	void Unlock(UBOOL Blit);
	void Flush(UBOOL AllowPrecache);
	void SetPermanentState();

	void LockBuffer(BufferRange& Buffer);
	void WaitBuffer(BufferRange& Buffer);
	void MapBuffers();

	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet);
	void BufferComplexSurfacePoint( FLOAT* DrawComplexTemp, FTransform* P, DrawComplexTexMaps TexMaps, FSurfaceFacet& Facet, DrawComplexBuffer& Buffer, INT InterleaveCounter );
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, AActor* Owner);
	void BufferGouraudPolygonPoint(FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& Buffer );
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
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags);

	// Editor
	void PushHit(const BYTE* Data, INT Count);
	void PopHit(INT Count, UBOOL bForce);
	void LockHit(BYTE* InHitData, INT* InHitSize);
	void UnlockHit(UBOOL Blit);
	void SetSceneNodeHit(FSceneNode* Frame);
	bool HitTesting() { return HitData != NULL; }

	void SetProgram( INT CurrentProgram );
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();
	void MakeCurrent(void);

	void SetTexture( INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, INT ShaderProg ); //First parameter has to fit the uniform in the fragment shader
	void SetNoTexture( INT Multi );
	DWORD SetBlend(DWORD PolyFlags, INT ShaderProg);
	DWORD SetDepth(DWORD PolyFlags);
	void SetSampler(GLuint Multi, DWORD PolyFlags, UBOOL SkipMipmaps, BYTE UClampMode, BYTE VClampMode);

	void BuildGammaRamp(FLOAT GammaCorrection, FGammaRamp& Ramp);
	void BuildGammaRamp(FLOAT GammaCorrection, FByteGammaRamp& Ramp);
	void SetGamma(FLOAT GammaCorrection);
	void SetFlatState(bool bEnable);

	// DistanceFog
	void DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf);
	void PreDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf);
	void PostDrawGouraud(FSceneNode* Frame, FFogSurf &FogSurf);
	void ResetFog();

#if ENGINE_VERSION==227
	// (Static) Mesh Rendering
	void BeginMeshDraw(FSceneNode* Frame, AActor* Owner);
	void EndMeshDraw(FSceneNode* Frame, AActor* Owner);

	// Lighting
	void RenderLighting(FSceneNode* Frame, UTexture* SurfaceTexture, UTexture* DetailTexture, DWORD PolyFlags, UBOOL bDisableLights);
	INT	LightNum;
#endif

	// Shader stuff
	void InitShaders();
	void DeleteShaderBuffers();
	void LoadShader(const TCHAR* Filename, GLuint &ShaderObject);
	void DrawSimpleGeometryVerts(DrawSimpleMode DrawMode, GLuint size, INT Mode, DWORD LineFLags, FPlane DrawColor, bool BufferedDraw);
	void DrawTileVerts();
	void DrawComplexVertsSinglePass(DrawComplexBuffer &Buffer, DrawComplexTexMaps TexMaps,FPlane DrawColor);
	void BufferGouraudPolyVerts( FLOAT* verts, GLuint VertSize, GLuint TexSize, GLuint colorsize, FPlane DrawColor);
	void DrawGouraudPolyBuffer();
	void DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer &Buffer, FPlane DrawColor);

#if ENGINE_VERSION==227
	void DrawGouraudPolyVertListMeshBuffer(FLOAT* verts, GLuint size,FLOAT* tex, GLuint TexSize, FLOAT* lightcolor, GLuint colorsize, FPlane DrawColor, AActor* Owner, INT Pass);
#endif

	void Exit();
	void DeleteBuffers();
	void ShutdownAfterError();

	// OpenGLStats
	void DrawStats( FSceneNode* Frame );

	void GetUniformBlockIndex(GLuint &Program, GLuint BlockIndex, const GLuint BindingIndex, const char* Name, FString ProgramName);
	void GetUniformLocation(GLuint &Uniform, GLuint &Program, const char* Name, FString ProgramName);

	// Error logging
	public:
#ifdef WIN32
	static void CALLBACK DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#else
    static void DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam);
#endif
};

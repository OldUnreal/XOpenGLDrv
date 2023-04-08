/*=============================================================================
	UnShader.cpp: Unreal XOpenGL shader support code.

	Copyright 2014-2017 Oldunreal

	Revision history:
	* Created by Smirftsch
=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

#define USE_BUILT_IN_SHADERS 1

#if USE_BUILT_IN_SHADERS
#define READABLE_OUTPUT 0 // For nicer debug output.

class FShaderOutputDevice : public FString, public FOutputDevice
{
#if READABLE_OUTPUT
	TCHAR Intendents[32];
	INT IntendIx;
	const TCHAR* BufferTag;
#endif
public:
	FShaderOutputDevice(const TCHAR* InTag)
#if READABLE_OUTPUT
		: BufferTag(InTag)
	{
		Intendents[0] = '\x0';
		IntendIx = 0;
	}
#else
	{}
#endif
#if READABLE_OUTPUT
	~FShaderOutputDevice()
	{
		verify(IntendIx==0);
		GLog->Logf(NAME_Event, TEXT("=================================================================== (%ls)"), BufferTag);
		GLog->Log(NAME_Event, *this);
		GLog->Log(NAME_Event, TEXT("==================================================================="));
	}
#endif
	void Serialize(const TCHAR* Data, EName Event)
	{
#if READABLE_OUTPUT
		if (Data[0] == '}')
		{
			verify(IntendIx > 0);
			--IntendIx;
			Intendents[IntendIx] = '\x0';
		}
		if (IntendIx)
			*this += Intendents;
		verify(appStrstr(Data, TEXT("PF_")) == NULL); // Make sure no PolyFlag constants slipped through...
		verify(appStrstr(Data, TEXT("DF_")) == NULL);
#endif
		*this += Data;
		
#if READABLE_OUTPUT
		* this += TEXT("\r\n");
		if (Data[0] == '{' && (!Data[1] || Data[1] != ';'))
		{
			Intendents[IntendIx] = '\t';
			++IntendIx;
			Intendents[IntendIx] = '\x0';
		}
#else
		* this += TEXT("\n");
#endif
	}
};
#include "XOpenGLShaders.h"
#endif

// Helpers
void UXOpenGLRenderDevice::GetUniformBlockIndex(GLuint& Program, GLuint BlockIndex, const GLuint BindingIndex, const char* Name, FString ProgramName)
{
	BlockIndex = glGetUniformBlockIndex(Program, Name);
	if (BlockIndex == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), *ProgramName);
		if (UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), *ProgramName);
	}
	glUniformBlockBinding(Program, BlockIndex, BindingIndex);
}

void UXOpenGLRenderDevice::GetUniformLocation(GLuint& Uniform, GLuint& Program, const char* Name, FString ProgramName)
{
	Uniform = glGetUniformLocation(Program, Name);
	if (Uniform == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), *ProgramName);
		if (UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), *ProgramName);
	}
}

// Shader handling
void UXOpenGLRenderDevice::LoadShader(const TCHAR* Filename, GLuint& ShaderObject)
{
	guard(UXOpenGLRenderDevice::LoadShader);
#if USE_BUILT_IN_SHADERS
	appErrorf(TEXT("Not in use in this build!"));
#else
	GLint blen = 0;
	GLsizei slen = 0;
	GLint IsCompiled = 0;
	GLint length = 0;

	FString Extensions, Definitions, Globals, Statics, Shaderdata;


	// VERSION
	FString GLVersionString = TEXT("#version 310 es\n");

	if (OpenGLVersion == GL_Core)
	{
		if (UsingShaderDrawParameters)
			GLVersionString = TEXT("#version 460 core\n");
		else if (UsingBindlessTextures || UsingPersistentBuffers)
			GLVersionString = TEXT("#version 450 core\n");
		else
			GLVersionString = TEXT("#version 330 core\n");
	}

	// ADD DEFINITIONS
	Definitions += *FString::Printf(TEXT("#define EDITOR %d\n"), GIsEditor ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define BINDLESSTEXTURES %d\n"), UsingBindlessTextures ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define BINDLESS_STORAGE_UBO %d\n"), (UsingBindlessTextures && BindlessHandleStorage == STORE_UBO) ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define BINDLESS_STORAGE_SSBO %d\n"), (UsingBindlessTextures && BindlessHandleStorage == STORE_SSBO) ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define BINDLESS_STORAGE_INT %d\n"), (UsingBindlessTextures && BindlessHandleStorage == STORE_INT) ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define NUMTEXTURES %i \n"), MaxBindlessTextures);
	Definitions += *FString::Printf(TEXT("#define HARDWARELIGHTS %d\n"), UseHWLighting ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define BUMPMAPS %d\n"), BumpMaps ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define DETAILTEXTURES %d\n"), DetailTextures ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define MACROTEXTURES %d\n"), MacroTextures ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define ENGINE_VERSION %d\n"), ENGINE_VERSION);
	Definitions += *FString::Printf(TEXT("#define MAX_LIGHTS %i \n"), MAX_LIGHTS);
	Definitions += *FString::Printf(TEXT("#define MAX_CLIPPINGPLANES %i \n"), MaxClippingPlanes);
	Definitions += *FString::Printf(TEXT("#define SHADERDRAWPARAMETERS %i \n"), UsingShaderDrawParameters);
	Definitions += *FString::Printf(TEXT("#define SIMULATEMULTIPASS %i \n"), SimulateMultiPass);
	Definitions += *FString::Printf(TEXT("#define BASIC_PARALLAX %d\n"), ParallaxVersion == Parallax_Basic ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define OCCLUSION_PARALLAX %d\n"), ParallaxVersion == Parallax_Occlusion ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define RELIEF_PARALLAX %i\n"), ParallaxVersion == Parallax_Relief ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define SUPPORTSCLIPDISTANCE %d\n"), SupportsClipDistance ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define SRGB %d\n"), UseSRGBTextures ? 1 : 0);
	Definitions += *FString::Printf(TEXT("#define ENHANCED_LIGHTMAPS %i \n"), UseEnhancedLightmaps ? 1 : 0);

	// LOAD EXTENSIONS
	if (!appLoadFileToString(Extensions, TEXT("xopengl/Extensions.incl"))) // Load extensions config.
		appErrorf(TEXT("XOpenGL: Failed loading global Extensions file xopengl/Extensions.incl"));

	// The following directive resets the line number to 1 to have the correct output logging for a possible error within the shader files.
	Definitions += *FString::Printf(TEXT("#line 1 \n"));

	// LOAD GLOBALS
	if (!appLoadFileToString(Globals, TEXT("xopengl/Globals.incl"))) // Load global config.
		appErrorf(TEXT("XOpenGL: Failed loading global shader file xopengl/Globals.incl"));

	// append static config vars
	Statics += *FString::Printf(TEXT("const int DetailMax=%i; \n"), DetailMax);
	Statics += *FString::Printf(TEXT("const float GammaMultiplier=%f; \n"), GammaMultiplier);
	Statics += *FString::Printf(TEXT("const float GammaMultiplierUED=%f; \n"), GammaMultiplierUED);
	Statics += *FString::Printf(TEXT("#line 1 \n"));

	// LOAD SHADER
	if (!appLoadFileToString(Shaderdata, Filename))
		appErrorf(TEXT("XOpenGL: Failed loading shader file %ls"), Filename);

	if (UseOpenGLDebug && LogLevel >= 3)
	{
		debugf(TEXT("GLVersion %ls: \n %ls \n\n"), Filename, *GLVersionString);
		debugf(TEXT("Definitions %ls: \n %ls \n\n"), Filename, *Definitions);
		debugf(TEXT("Extensions %ls: \n %ls \n\n"), Filename, *Extensions);
		debugf(TEXT("Globals %ls: \n %ls \n\n"), Filename, *Globals);
		debugf(TEXT("Statics %ls: \n %ls \n\n"), Filename, *Statics);
		debugf(TEXT("Shaderdata %ls: \n %ls \n\n"), Filename, *Shaderdata);
	}
	FString Text = (GLVersionString + Definitions + Extensions + Globals + Statics + Shaderdata);

	const GLchar* Shader = TCHAR_TO_ANSI(*Text);
	length = (GLint)appStrlen(*Text);
	glShaderSource(ShaderObject, 1, &Shader, &length);
	glCompileShader(ShaderObject);

	glGetShaderiv(ShaderObject, GL_COMPILE_STATUS, &IsCompiled);
	if (!IsCompiled)
		GWarn->Logf(TEXT("XOpenGL: Failed compiling %ls"), Filename);

	glGetShaderiv(ShaderObject, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* compiler_log = new GLchar[blen + 1];
		glGetShaderInfoLog(ShaderObject, blen, &slen, compiler_log);
		debugf(NAME_DevGraphics, TEXT("XOpenGL: Log compiling %ls %ls"), Filename, appFromAnsi(compiler_log));
		delete[] compiler_log;
	}
	else debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: No compiler messages for %ls"), Filename);

	CHECK_GL_ERROR();
#endif
	unguard;
}

#if USE_BUILT_IN_SHADERS
static void BuildShader(const TCHAR* ShaderProgName, GLuint& ShaderProg, UXOpenGLRenderDevice* GL, glShaderProg* Func)
{
	guard(BuildShader);
	FShaderOutputDevice ShOut(ShaderProgName);
	Implement_Extension(GL, ShOut);
	(*Func)(GL, ShOut);

	GLint blen = 0;
	GLsizei slen = 0;
	GLint IsCompiled = 0;
	const GLchar* Shader = TCHAR_TO_ANSI(*ShOut);
	GLint length = ShOut.Len();
	glShaderSource(ShaderProg, 1, &Shader, &length);
	glCompileShader(ShaderProg);

	glGetShaderiv(ShaderProg, GL_COMPILE_STATUS, &IsCompiled);
	if (!IsCompiled)
		GWarn->Logf(TEXT("XOpenGL: Failed compiling %ls"), ShaderProgName);

	glGetShaderiv(ShaderProg, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* compiler_log = new GLchar[blen + 1];
		glGetShaderInfoLog(ShaderProg, blen, &slen, compiler_log);
		debugf(NAME_DevGraphics, TEXT("XOpenGL: Log compiling %ls %ls"), ShaderProgName, appFromAnsi(compiler_log));
		delete[] compiler_log;
	}
	else debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: No compiler messages for %ls"), ShaderProgName);

	CHECK_GL_ERROR();
	unguard;
}
#endif

void UXOpenGLRenderDevice::LinkShader(const TCHAR* ShaderProgName, GLuint& ShaderProg)
{
	guard(UXOpenGLRenderDevice::LinkShader);
	GLint IsLinked = 0;
	GLint blen = 0;
	GLsizei slen = 0;
	glLinkProgram(ShaderProg);
	glGetProgramiv(ShaderProg, GL_LINK_STATUS, &IsLinked);

	if (!IsLinked)
		GWarn->Logf(TEXT("XOpenGL: Failed linking %ls"), ShaderProgName);

	glGetProgramiv(ShaderProg, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* linker_log = new GLchar[blen + 1];
		glGetProgramInfoLog(ShaderProg, blen, &slen, linker_log);
		debugf(TEXT("XOpenGL: Log linking %ls %ls"), ShaderProgName, appFromAnsi(linker_log));
		delete[] linker_log;
	}
	else debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: No linker messages for %ls"), ShaderProgName);

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::InitShaders()
{
	guard(UXOpenGLRenderDevice::InitShaders);

	DrawSimpleVertObject = glCreateShader(GL_VERTEX_SHADER);
	//if (OpenGLVersion == GL_Core)
	//	DrawSimpleGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawSimpleFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	DrawTileVertObject = glCreateShader(GL_VERTEX_SHADER);
	if (OpenGLVersion == GL_Core)
		DrawTileGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawTileFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	// DrawComplexSurface SinglePass
	DrawComplexVertSinglePassObject = glCreateShader(GL_VERTEX_SHADER);
	DrawComplexFragSinglePassObject = glCreateShader(GL_FRAGMENT_SHADER);
	// DrawComplexGeoSinglePassObject = glCreateShader(GL_GEOMETRY_SHADER);
	CHECK_GL_ERROR();

	// DrawGouraudPolygons.
	DrawGouraudVertObject = glCreateShader(GL_VERTEX_SHADER);
	if (OpenGLVersion == GL_Core)
		DrawGouraudGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawGouraudFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	/*
	// ShadowMaps
	SimpleDepthVertObject = glCreateShader(GL_VERTEX_SHADER);
	SimpleDepthFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();
	*/

#if USE_BUILT_IN_SHADERS
	{
		BuildShader(TEXT("DrawSimple.vert"), DrawSimpleVertObject, this, Create_DrawSimple_Vert);
		//if (OpenGLVersion == GL_Core)
		//	BuildShader(TEXT("DrawSimple.geo"), DrawSimpleGeoObject, this, Create_DrawSimple_Geo);
		BuildShader(TEXT("DrawSimple.frag"), DrawSimpleFragObject, this, Create_DrawSimple_Frag);

		BuildShader(TEXT("DrawTile.vert"), DrawTileVertObject, this, Create_DrawTile_Vert);
		if (OpenGLVersion == GL_Core)
			BuildShader(TEXT("DrawTile.geo"), DrawTileGeoObject, this, Create_DrawTile_Geo);
		BuildShader(TEXT("DrawTile.frag"), DrawTileFragObject, this, Create_DrawTile_Frag);

		BuildShader(TEXT("DrawGouraud.vert"), DrawGouraudVertObject, this, Create_DrawGouraud_Vert);
		if (OpenGLVersion == GL_Core)
			BuildShader(TEXT("DrawGouraud.geo"), DrawGouraudGeoObject, this, Create_DrawGouraud_Geo);
		BuildShader(TEXT("DrawGouraud.frag"), DrawGouraudFragObject, this, Create_DrawGouraud_Frag);

		BuildShader(TEXT("DrawComplexSinglePass.vert"), DrawComplexVertSinglePassObject, this, Create_DrawComplexSinglePass_Vert);
		//if (OpenGLVersion == GL_Core)
		//	BuildShader(TEXT("DrawComplexSinglePass.geo"), DrawComplexGeoSinglePassObject, this, Create_DrawComplexSinglePass_Geo);
		BuildShader(TEXT("DrawComplexSinglePass.frag"), DrawComplexFragSinglePassObject, this, Create_DrawComplexSinglePass_Frag);
	}
#else
	{
		// TODO!!! Add ini entries to handle shaders dynamically
		LoadShader(TEXT("xopengl/DrawSimple.vert"), DrawSimpleVertObject);
		//if (OpenGLVersion == GL_Core)
		//	LoadShader(TEXT("xopengl/DrawSimple.geo"), DrawSimpleGeoObject);
		LoadShader(TEXT("xopengl/DrawSimple.frag"), DrawSimpleFragObject);

		LoadShader(TEXT("xopengl/DrawTile.vert"), DrawTileVertObject);
		if (OpenGLVersion == GL_Core)
			LoadShader(TEXT("xopengl/DrawTile.geo"), DrawTileGeoObject);
		LoadShader(TEXT("xopengl/DrawTile.frag"), DrawTileFragObject);

		LoadShader(TEXT("xopengl/DrawGouraud.vert"), DrawGouraudVertObject);
		if (OpenGLVersion == GL_Core)
			LoadShader(TEXT("xopengl/DrawGouraud.geo"), DrawGouraudGeoObject);
		LoadShader(TEXT("xopengl/DrawGouraud.frag"), DrawGouraudFragObject);

		LoadShader(TEXT("xopengl/DrawComplexSinglePass.vert"), DrawComplexVertSinglePassObject);
		//if (OpenGLVersion == GL_Core)
		//	LoadShader(TEXT("xopengl/DrawComplexSinglePass.geo"), DrawComplexGeoSinglePassObject);
		LoadShader(TEXT("xopengl/DrawComplexSinglePass.frag"), DrawComplexFragSinglePassObject);
	}
#endif

	/*
	// ShadowMap
	LoadShader(TEXT("xopengl/SimpleDepth.vert"), SimpleDepthVertObject);
	LoadShader(TEXT("xopengl/SimpleDepth.frag"), SimpleDepthFragObject);
	*/

	DrawSimpleProg = glCreateProgram();
	DrawTileProg = glCreateProgram();
	DrawComplexProg = glCreateProgram();
	DrawGouraudProg = glCreateProgram();
	// SimpleDepthProg = glCreateProgram();
	CHECK_GL_ERROR();

	glAttachShader(DrawSimpleProg, DrawSimpleVertObject);
	//glAttachShader(DrawSimpleProg, DrawSimpleGeoObject);
	glAttachShader(DrawSimpleProg, DrawSimpleFragObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawTileProg, DrawTileVertObject);
	if (OpenGLVersion == GL_Core)
		glAttachShader(DrawTileProg, DrawTileGeoObject);
	glAttachShader(DrawTileProg, DrawTileFragObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawComplexProg, DrawComplexVertSinglePassObject);
	glAttachShader(DrawComplexProg, DrawComplexFragSinglePassObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawGouraudProg, DrawGouraudVertObject);
	if (OpenGLVersion == GL_Core)
		glAttachShader(DrawGouraudProg, DrawGouraudGeoObject);
	glAttachShader(DrawGouraudProg, DrawGouraudFragObject);
	CHECK_GL_ERROR();

	/*
	glAttachShader(SimpleDepthProg, SimpleDepthVertObject);
	glAttachShader(SimpleDepthProg, SimpleDepthFragObject);
	CHECK_GL_ERROR();
	*/

	// Link shaders.
	LinkShader(TEXT("DrawSimpleProg"), DrawSimpleProg);
	LinkShader(TEXT("DrawTileProg"), DrawTileProg);
	LinkShader(TEXT("DrawComplexProg"), DrawComplexProg);
	LinkShader(TEXT("DrawGouraudProg"), DrawGouraudProg);
	//LinkShader(TEXT("SimpleDepthProg"), SimpleDepthProg);

	//Global Matrix setup
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawGouraudProg"));
	//GetUniformBlockIndex(SimpleDepthProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("SimpleDepthProg"));
	CHECK_GL_ERROR();

	//Global ClipPlanes
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawGouraudProg"));
	CHECK_GL_ERROR();

	//Global texture handles for bindless.
	if (UsingBindlessTextures && BindlessHandleStorage == STORE_UBO)
	{
		GetUniformBlockIndex(DrawTileProg, GlobalTextureHandlesBlockIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawTileProg"));
		GetUniformBlockIndex(DrawComplexProg, GlobalTextureHandlesBlockIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawComplexProg"));
		GetUniformBlockIndex(DrawGouraudProg, GlobalTextureHandlesBlockIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawGouraudProg"));
		CHECK_GL_ERROR();
	}

	// Light information.
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawGouraudProg"));
	CHECK_GL_ERROR();

	//Global Matrix setup
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawGouraudProg"));
	//GetUniformBlockIndex(SimpleDepthProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("SimpleDepthProg"));
	CHECK_GL_ERROR();

	//DrawSimple vars.
	FString DrawSimple = TEXT("DrawSimple");
	GetUniformLocation(DrawSimpleDrawColor, DrawSimpleProg, "DrawColor", DrawSimple);
	GetUniformLocation(DrawSimplebHitTesting, DrawSimpleProg, "bHitTesting", DrawSimple);
	GetUniformLocation(DrawSimpleLineFlags, DrawSimpleProg, "LineFlags", DrawSimple);
	GetUniformLocation(DrawSimpleGamma, DrawSimpleProg, "Gamma", DrawSimple);
	CHECK_GL_ERROR();

	//DrawTile vars.
	FString DrawTile = TEXT("DrawTile");
	GetUniformLocation(DrawTileTexCoords, DrawTileProg, "TexCoords", DrawTile);
	GetUniformLocation(DrawTilebHitTesting, DrawTileProg, "bHitTesting", DrawTile);
	GetUniformLocation(DrawTileHitDrawColor, DrawTileProg, "HitDrawColor", DrawTile);
	GetUniformLocation(DrawTileTexture, DrawTileProg, "Texture0", DrawTile);
	GetUniformLocation(DrawTilePolyFlags, DrawTileProg, "PolyFlags", DrawTile);
	GetUniformLocation(DrawTileTexNum, DrawTileProg, "TexNum", DrawTile);
	GetUniformLocation(DrawTileGamma, DrawTileProg, "Gamma", DrawTile);
	CHECK_GL_ERROR();

	//DrawComplexSinglePass vars.
	FString DrawComplexSinglePass = TEXT("DrawComplexSinglePass");
	GetUniformLocation(DrawComplexSinglePassTexCoords, DrawComplexProg, "TexCoords", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassTexNum, DrawComplexProg, "TexNum", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassDrawFlags, DrawComplexProg, "DrawFlags", DrawComplexSinglePass);
	// Multitextures in DrawComplexProg
	for (INT i = 0; i < 8; i++)
		GetUniformLocation(DrawComplexSinglePassTexture[i], DrawComplexProg, (char*)(TCHAR_TO_ANSI(*FString::Printf(TEXT("Texture%i"), i))), DrawComplexSinglePass);
	CHECK_GL_ERROR();

	//DrawGouraud vars.
	FString DrawGouraud = TEXT("DrawGouraud");
	GetUniformLocation(DrawGouraudDrawData, DrawGouraudProg, "DrawData", DrawGouraud);
	GetUniformLocation(DrawGouraudDrawFlags, DrawGouraudProg, "DrawFlags", DrawGouraud);
	GetUniformLocation(DrawGouraudTexNum, DrawGouraudProg, "TexNum", DrawGouraud);
	// Multitextures DrawGouraudProg
	for (INT i = 0; i < 8; i++)
		GetUniformLocation(DrawGouraudTexture[i], DrawGouraudProg, (char*)(TCHAR_TO_ANSI(*FString::Printf(TEXT("Texture%i"), i))), DrawGouraud);
	// ShadowMap

	CHECK_GL_ERROR();

	// Global matrices.
	glGenBuffers(1, &GlobalMatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, (5 * sizeof(glm::mat4)), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalMatricesBindingIndex, GlobalMatricesUBO, 0, sizeof(glm::mat4) * 5);
	CHECK_GL_ERROR();

	// Global Coords.
	glGenBuffers(1, &GlobalCoordsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalCoordsUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2 * sizeof(glm::mat4)), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalCoordsBindingIndex, GlobalCoordsUBO, 0, sizeof(glm::mat4) * 2);
	CHECK_GL_ERROR();

	// Global bindless textures.
	if (UsingBindlessTextures && (BindlessHandleStorage == STORE_UBO || BindlessHandleStorage == STORE_SSBO))
	{
		GLenum Target = (BindlessHandleStorage == STORE_UBO) ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
		glGenBuffers(1, &GlobalTextureHandlesBufferObject);
		glBindBuffer(Target, GlobalTextureHandlesBufferObject);
		if (Target == GL_UNIFORM_BUFFER)
		{
			glBufferData(Target, sizeof(GLuint64) * MaxBindlessTextures * 2, NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(Target, 0);
			glBindBufferRange(Target, GlobalTextureHandlesBindingIndex, GlobalTextureHandlesBufferObject, 0, sizeof(GLuint64) * MaxBindlessTextures * 2);
		}
		else
		{
			glBindBufferBase(Target, GlobalTextureHandlesBindingIndex, GlobalTextureHandlesBufferObject);
			glBindBuffer(Target, 0);
		}

		CHECK_GL_ERROR();
	}

	// Static lights information.
	glGenBuffers(1, &GlobalStaticLightInfoUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalStaticLightInfoUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 6, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalStaticLightInfoIndex, GlobalStaticLightInfoUBO, 0, sizeof(glm::vec4) * MAX_LIGHTS * 6);
	CHECK_GL_ERROR();

	// Global ClipPlane.
	glGenBuffers(1, &GlobalClipPlaneUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalClipPlaneUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2 * sizeof(glm::vec4)), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalClipPlaneBindingIndex, GlobalClipPlaneUBO, 0, sizeof(glm::vec4) * 2);
	CHECK_GL_ERROR();

	//DrawSimple
	glGenBuffers(1, &DrawSimpleVertBuffer);
	glGenVertexArrays(1, &DrawSimpleGeometryVertsVao);
	CHECK_GL_ERROR();

	//DrawTile
	glGenBuffers(1, &DrawTileVertBuffer);
	glGenVertexArrays(1, &DrawTileVertsVao);
	CHECK_GL_ERROR();

	//DrawComplex
	glGenBuffers(1, &DrawComplexVertBuffer);
	glGenVertexArrays(1, &DrawComplexVertsSinglePassVao);
	CHECK_GL_ERROR();

	glGenBuffers(1, &DrawComplexSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBOBindingIndex, DrawComplexSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	//DrawGouraud
	glGenBuffers(1, &DrawGouraudVertBuffer);
	glGenVertexArrays(1, &DrawGouraudPolyVertsVao);

	glGenBuffers(1, &DrawGouraudSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBOBindingIndex, DrawGouraudSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	/*
	// ShadowMap
	glGenBuffers(1, &SimpleDepthBuffer);
	glGenVertexArrays(1, &SimpleDepthVao);
	*/
	CHECK_GL_ERROR();

	unguard;
}

void UXOpenGLRenderDevice::DeleteShaderBuffers()
{
	guard(UXOpenGLRenderDevice::DeleteShaderBuffers);

	if (GlobalMatricesUBO)
		glDeleteBuffers(1, &GlobalMatricesUBO);
	if (GlobalTextureHandlesBufferObject)
		glDeleteBuffers(1, &GlobalTextureHandlesBufferObject);
	if (GlobalStaticLightInfoUBO)
		glDeleteBuffers(1, &GlobalStaticLightInfoUBO);
	if (GlobalCoordsUBO)
		glDeleteBuffers(1, &GlobalCoordsUBO);
	if (DrawSimpleVertBuffer)
		glDeleteBuffers(1, &DrawSimpleVertBuffer);
	if (DrawTileVertBuffer)
		glDeleteBuffers(1, &DrawTileVertBuffer);
	if (DrawGouraudVertBuffer)
		glDeleteBuffers(1, &DrawGouraudVertBuffer);
	if (DrawComplexVertBuffer)
		glDeleteBuffers(1, &DrawComplexVertBuffer);
	if (DrawComplexSSBO)
		glDeleteBuffers(1, &DrawComplexSSBO);
	if (DrawGouraudSSBO)
		glDeleteBuffers(1, &DrawGouraudSSBO);
	/*
		if (SimpleDepthBuffer)
			glDeleteBuffers(1, &SimpleDepthBuffer);
	*/

	if (DrawSimpleGeometryVertsVao)
		glDeleteVertexArrays(1, &DrawSimpleGeometryVertsVao);
	if (DrawTileVertsVao)
		glDeleteVertexArrays(1, &DrawTileVertsVao);
	if (DrawGouraudPolyVertsVao)
		glDeleteVertexArrays(1, &DrawGouraudPolyVertsVao);

	if (DrawComplexVertsSinglePassVao)
		glDeleteVertexArrays(1, &DrawComplexVertsSinglePassVao);

	/*
	if (SimpleDepthVao)
		glDeleteVertexArrays(1, &SimpleDepthVao);
	*/
	unguard;
}

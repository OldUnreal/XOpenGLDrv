/*=============================================================================
	UnShader.h: Unreal XOpenGL shader support header.

	Copyright 2014-2017 Oldunreal

	Revision history:
	* Created by Smirftsch

=============================================================================*/

// Helpers
void UXOpenGLRenderDevice::GetUniformBlockIndex(GLuint &Program, GLuint BlockIndex, const GLuint BindingIndex, const char* Name, FString ProgramName)
{
	BlockIndex = glGetUniformBlockIndex(Program, Name);
	if (BlockIndex == GL_INVALID_INDEX)
    {
		debugf(NAME_Dev, TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), *ProgramName);
		if (UseOpenGLDebug && LogLevel >= 2)
            debugf(TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), *ProgramName);
    }
	glUniformBlockBinding(Program, BlockIndex, BindingIndex);
}

void UXOpenGLRenderDevice::GetUniformLocation(GLuint &Uniform, GLuint &Program, const char* Name, FString ProgramName)
{
	Uniform = glGetUniformLocation(Program, Name);
	if (Uniform == GL_INVALID_INDEX)
    {
		debugf(NAME_Dev, TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), *ProgramName);
		if (UseOpenGLDebug && LogLevel >= 2)
            debugf(TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), *ProgramName);
    }
}

// Shader handling
void UXOpenGLRenderDevice::LoadShader(const TCHAR* Filename, GLuint &ShaderObject)
{
	guard(UXOpenGLRenderDevice::LoadShader);
	GLint blen = 0;
	GLsizei slen = 0;
	GLint IsCompiled = 0;
	GLint length = 0;

	FString Globals;
	FString GLVersion = TEXT("#version 300 es\n");

	if (OpenGLVersion == GL_Core)
	{
		if (UseBindlessTextures || UsePersistentBuffers)
			GLVersion = TEXT("#version 450 core\n");
		else GLVersion = TEXT("#version 330 core\n");
	}


	if (GIsEditor)
		GLVersion += TEXT("#define EDITOR \n");

    if(UseBindlessTextures) //FIXME: Move to Globals.incl.
    {
        GLVersion += TEXT("#define BINDLESSTEXTURES \n");
        GLVersion += *FString::Printf(TEXT("#define NUMTEXTURES %i \n"), NUMTEXTURES);
    }
	if (UseHWLighting) //FIXME: Move to Globals.incl.
		GLVersion += TEXT("#define HARDWARELIGHTS \n");

	if (!appLoadFileToString(Globals, TEXT("xopengl/Globals.incl"))) // Load global config.
		appErrorf(TEXT("XOpenGL: Failed loading global shader file xopengl/Globals.incl"));

	Globals += *FString::Printf(TEXT("const int MAX_LIGHTS = %i; \n"), MAX_LIGHTS);

	FString Shaderdata =TEXT("");
	if (!appLoadFileToString(Shaderdata, Filename))
		appErrorf(TEXT("XOpenGL: Failed loading shader file %ls"), Filename);

	FString Text = (GLVersion + Globals + Shaderdata);

	if (UseOpenGLDebug && LogLevel >= 2)
	{
		debugf(TEXT("GLVersion %ls: %ls \n\n"), Filename, *GLVersion);
		debugf(TEXT("Globals %ls: %ls \n\n"), Filename, *Globals);
		debugf(TEXT("Shaderdata %ls: %ls \n\n"), Filename, *Shaderdata);
		debugf(TEXT("Shader %ls: %ls \n\n"), Filename, *Text);
	}

	const GLchar* Shader = appToAnsi(*Text);
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
		debugf(TEXT("XOpenGL: Log compiling %ls %ls"), Filename, appFromAnsi(compiler_log));
		delete[] compiler_log;
	}
	else debugf(TEXT("XOpenGL: No compiler messages for %ls"), Filename);

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::LinkShader(const TCHAR* ShaderProgName, GLuint &ShaderProg)
{
    guard(UXOpenGLRenderDevice::LinkShader);
    GLint IsLinked = 0;
    GLint blen = 0;
    GLsizei slen = 0;
    glLinkProgram(ShaderProg);
	glGetProgramiv(ShaderProg, GL_LINK_STATUS, &IsLinked);

    if (!IsLinked)
        GWarn->Logf(TEXT("XOpenGL: Failed linking %ls"),ShaderProgName);

    glGetProgramiv(ShaderProg, GL_INFO_LOG_LENGTH, &blen);
    if (blen > 1)
    {
        GLchar* linker_log = new GLchar[blen + 1];
        glGetProgramInfoLog(ShaderProg, blen, &slen, linker_log);
        debugf(TEXT("XOpenGL: Log linking %ls %ls"), ShaderProgName, appFromAnsi(linker_log));
        delete[] linker_log;
    }
    else debugf(TEXT("XOpenGL: No linker messages for %ls"), ShaderProgName);

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

	// TODO!!! Add ini entries to handle shaders dynamically
	LoadShader(TEXT("xopengl/DrawSimple.vert"), DrawSimpleVertObject);
	//LoadShader(TEXT("xopengl/DrawSimple.geo"), DrawSimpleGeoObject);
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
	LinkShader(TEXT("DrawSimpleProg"),DrawSimpleProg);
	LinkShader(TEXT("DrawTileProg"),DrawTileProg);
	LinkShader(TEXT("DrawComplexProg"),DrawComplexProg);
	LinkShader(TEXT("DrawGouraudProg"),DrawGouraudProg);
	//LinkShader(TEXT("SimpleDepthProg"), SimpleDepthProg);

	//Global Matrix setup
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawGouraudProg"));
	//GetUniformBlockIndex(SimpleDepthProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("SimpleDepthProg"));
	CHECK_GL_ERROR();

	//Global DistanceFog
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformDistanceFogIndex, GlobalDistanceFogBindingIndex, "DistanceFogParams", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformDistanceFogIndex, GlobalDistanceFogBindingIndex, "DistanceFogParams", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformDistanceFogIndex, GlobalDistanceFogBindingIndex, "DistanceFogParams", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformDistanceFogIndex, GlobalDistanceFogBindingIndex, "DistanceFogParams", TEXT("DrawGouraudProg"));
	CHECK_GL_ERROR();

    //Global texture handles for bindless.
    if (UseBindlessTextures)
    {
        GetUniformBlockIndex(DrawSimpleProg, GlobalUniformTextureHandlesIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawSimpleProg"));
        GetUniformBlockIndex(DrawTileProg, GlobalUniformTextureHandlesIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawTileProg"));
        GetUniformBlockIndex(DrawComplexProg, GlobalUniformTextureHandlesIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawComplexProg"));
        GetUniformBlockIndex(DrawGouraudProg, GlobalUniformTextureHandlesIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawGouraudProg"));
        CHECK_GL_ERROR();
    }

	// Light information.
	//GetUniformBlockIndex(DrawSimpleProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawSimpleProg"));
	//GetUniformBlockIndex(DrawTileProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawTileProg"));
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
	GetUniformLocation(DrawComplexSinglePassPolyFlags, DrawComplexProg, "PolyFlags", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassRendMap, DrawComplexProg, "RendMap", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassbHitTesting, DrawComplexProg, "bHitTesting", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassDrawColor, DrawComplexProg, "DrawColor", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogColor, DrawComplexProg, "FogParams.FogColor", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogStart, DrawComplexProg, "FogParams.FogStart", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogEnd, DrawComplexProg, "FogParams.FogEnd", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogDensity, DrawComplexProg, "FogParams.FogDensity", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogMode, DrawComplexProg, "FogParams.FogMode", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassGamma, DrawComplexProg, "Gamma", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassLightPos, DrawComplexProg, "LightPos", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassTexCoords, DrawComplexProg, "TexCoords", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogMode, DrawComplexProg, "FogMode", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassTexNum, DrawComplexProg, "TexNum", DrawComplexSinglePass);
	// Multitextures in DrawComplexProg
	for (INT i = 0; i < 8; i++)
        GetUniformLocation(DrawComplexSinglePassTexture[i], DrawComplexProg, (char*) (appToAnsi(*FString::Printf(TEXT("Texture%i"), i))), DrawComplexSinglePass);
	CHECK_GL_ERROR();

	//DrawGouraud vars.
	FString DrawGouraud = TEXT("DrawGouraud");
	GetUniformLocation(DrawGouraudPolyFlags, DrawGouraudProg, "PolyFlags", DrawGouraud);
	GetUniformLocation(DrawGouraudRendMap, DrawGouraudProg, "RendMap", DrawGouraud);
	GetUniformLocation(DrawGouraudbHitTesting, DrawGouraudProg, "bHitTesting", DrawGouraud);
	GetUniformLocation(DrawGouraudDrawColor, DrawGouraudProg, "DrawColor", DrawGouraud);
	GetUniformLocation(DrawGouraudGamma, DrawGouraudProg, "Gamma", DrawGouraud);
	GetUniformLocation(DrawGouraudPolyFlags, DrawGouraudProg, "PolyFlags", DrawGouraud);
	GetUniformLocation(DrawGouraudLightPos, DrawGouraudProg, "LightPos", DrawGouraud);
	// Multitextures DrawGouraudProg
	for (INT i = 0; i < 8; i++)
        GetUniformLocation(DrawGouraudTexture[i], DrawGouraudProg, (char *) (appToAnsi(*FString::Printf(TEXT("Texture%i"), i))), DrawGouraud);
	// ShadowMap

	CHECK_GL_ERROR();

	// Global matrices.
	glGenBuffers(1, &GlobalMatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, (4*sizeof(glm::mat4)), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalMatricesBindingIndex, GlobalMatricesUBO, 0, sizeof(glm::mat4) * 4);
	CHECK_GL_ERROR();

	// Global Coords.
	glGenBuffers(1, &GlobalCoordsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalCoordsUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2*sizeof(glm::mat4)), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalCoordsBindingIndex, GlobalCoordsUBO, 0, sizeof(glm::mat4) * 2);
	CHECK_GL_ERROR();

	// Global bindless textures.
	if (UseBindlessTextures)
    {
        glGenBuffers(1, &GlobalTextureHandlesUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, GlobalTextureHandlesUBO);
        glBufferData(GL_UNIFORM_BUFFER,sizeof(GLuint64) * NUMTEXTURES * 2, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferRange(GL_UNIFORM_BUFFER, GlobalTextureHandlesBindingIndex, GlobalTextureHandlesUBO, 0, sizeof(GLuint64) * NUMTEXTURES * 2);
        CHECK_GL_ERROR();
    }

	// Static lights information.
	glGenBuffers(1, &GlobalStaticLightInfoUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalStaticLightInfoUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 6, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalStaticLightInfoIndex, GlobalStaticLightInfoUBO, 0, sizeof(glm::vec4) * MAX_LIGHTS * 6);
	CHECK_GL_ERROR();

	// Global DistanceFog.
	glGenBuffers(1, &GlobalDistanceFogUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalDistanceFogUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2 * sizeof(glm::vec4)), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalDistanceFogBindingIndex, GlobalDistanceFogUBO, 0, sizeof(glm::vec4) * 2);
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
	glGenVertexArrays(1, &DrawComplexVertsSingleBufferVao);
	glGenVertexArrays(1, &DrawComplexVertListSingleBufferVao);
	glGenVertexArrays(1, &DrawComplexVertsSinglePassVao);
	CHECK_GL_ERROR();

	//DrawGouraud
	glGenBuffers(1, &DrawGouraudVertBuffer);
	glGenBuffers(1, &DrawGouraudVertBufferInterleaved);
	glGenBuffers(1, &DrawGouraudVertListBuffer);
	glGenVertexArrays(1, &DrawGouraudPolyVertsVao);
	glGenVertexArrays(1, &DrawGouraudPolyVertListVao);
	glGenVertexArrays(1, &DrawGouraudPolyVertsSingleBufferVao);
	glGenVertexArrays(1, &DrawGouraudPolyVertListSingleBufferVao);

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
#if ENGINE_VERSION==227
	if (Viewport && Viewport->Actor)
	{
		ULevel* Level = Viewport->Actor->GetLevel();
		if (Level)
		{
			for (INT i = 0; i < Level->Actors.Num(); i++)
			{
				if (Level->Actors(i) && Level->Actors(i)->MeshDataPtr)
				{
					FMeshVertCacheData* MemData = (FMeshVertCacheData*)Level->Actors(i)->MeshDataPtr;
					if (MemData)
					{
						for (INT Pass = 0; Pass < 64; Pass++) //TODO: Use some define
						{
							if (MemData->VAO[Pass])
								glDeleteVertexArrays(1, &MemData->VAO[Pass]);
							if (MemData->VertBuffer[Pass])
								glDeleteBuffers(1, &MemData->VertBuffer[Pass]);
							if (MemData->TexBuffer[Pass])
								glDeleteBuffers(1, &MemData->TexBuffer[Pass]);
							if (MemData->ColorBuffer[Pass])
								glDeleteBuffers(1, &MemData->ColorBuffer[Pass]);
							CHECK_GL_ERROR();
						}
						delete MemData;
						Level->Actors(i)->MeshDataPtr = NULL;
					}
				}
			}
		}
	}
#endif
	if (GlobalMatricesUBO)
		glDeleteBuffers(1, &GlobalMatricesUBO);
	if (GlobalTextureHandlesUBO)
		glDeleteBuffers(1, &GlobalTextureHandlesUBO);
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
	if (DrawGouraudVertBufferInterleaved)
		glDeleteBuffers(1, &DrawGouraudVertBufferInterleaved);
	if (DrawGouraudVertListBuffer)
		glDeleteBuffers(1, &DrawGouraudVertListBuffer);
	if (DrawComplexVertBuffer)
		glDeleteBuffers(1, &DrawComplexVertBuffer);
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
	if (DrawGouraudPolyVertListVao)
		glDeleteVertexArrays(1, &DrawGouraudPolyVertListVao);
	if (DrawGouraudPolyVertsSingleBufferVao)
		glDeleteVertexArrays(1, &DrawGouraudPolyVertsSingleBufferVao);
	if (DrawGouraudPolyVertListSingleBufferVao)
		glDeleteVertexArrays(1, &DrawGouraudPolyVertListSingleBufferVao);
	if (DrawComplexVertsSingleBufferVao)
		glDeleteVertexArrays(1, &DrawComplexVertsSingleBufferVao);
	if (DrawComplexVertListSingleBufferVao)
		glDeleteVertexArrays(1, &DrawComplexVertListSingleBufferVao);

	if (DrawComplexVertsSinglePassVao)
		glDeleteVertexArrays(1, &DrawComplexVertsSinglePassVao);

    /*
	if (SimpleDepthVao)
		glDeleteVertexArrays(1, &SimpleDepthVao);
    */
	unguard;
}

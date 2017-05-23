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
		debugf(NAME_Dev, TEXT("XOpenGL: invalid or unused shader var %s in %s"), appFromAnsi(Name), *ProgramName);
	glUniformBlockBinding(Program, BlockIndex, BindingIndex);
}

void UXOpenGLRenderDevice::GetUniformLocation(GLuint &Uniform, GLuint &Program, const char* Name, FString ProgramName)
{
	Uniform = glGetUniformLocation(Program, Name);
	if (Uniform == GL_INVALID_INDEX)
		debugf(NAME_Dev, TEXT("XOpenGL: invalid or unused shader var %s in %s"), appFromAnsi(Name), *ProgramName);
}

// Shader handling
void UXOpenGLRenderDevice::LoadShader(const TCHAR* Filename, GLuint &ShaderObject)
{
	guard(UXOpenGLRenderDevice::LoadShader);
	GLint blen = 0;
	GLsizei slen = 0;
	GLint compiled = 0;
	GLint length = 0;

	FString Globals;
	FString GLVersion = TEXT("#version 300 es\n");

	if (OpenGLVersion == GL_Core)
		GLVersion = TEXT("#version 330 core\n");

	if (GIsEditor)
		GLVersion += TEXT("#define EDITOR \n");

	if (!appLoadFileToString(Globals, TEXT("xopengl/Globals.incl"))) // Load global config.
		appErrorf(TEXT("XOpenGL: Failed loading global shader file xopengl/Globals.incl"));

	FString Shaderdata;
	if (!appLoadFileToString(Shaderdata, Filename))
		appErrorf(TEXT("XOpenGL: Failed loading shader file %s"), Filename);

	FString Text = GLVersion + Globals + Shaderdata;

	const GLchar* Shader = TCHAR_TO_ANSI(*Text);
	length = (GLint)appStrlen(*Text);
	glShaderSource(ShaderObject, 1, &Shader, &length);
	glCompileShader(ShaderObject);
	glGetShaderiv(ShaderObject, GL_COMPILE_STATUS, &compiled);
	glGetShaderiv(ShaderObject, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* compiler_log = new GLchar[blen + 1];
		glGetShaderInfoLog(ShaderObject, blen, &slen, compiler_log);
		debugf(TEXT("XOpenGL: Log compiling %s %s"), Filename, appFromAnsi(compiler_log));
		delete[] compiler_log;
	}
	else debugf(TEXT("XOpenGL: No compiler messages for %s"), Filename);
	if (!compiled)
		GWarn->Logf(TEXT("XOpenGL: Failed compiling %s"), Filename);
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::InitShaders()
{
	guard(UXOpenGLRenderDevice::InitShaders);

	DrawSimpleVertObject = glCreateShader(GL_VERTEX_SHADER);
	if (OpenGLVersion == GL_Core)
		DrawSimpleGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawSimpleFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	DrawTileVertObject = glCreateShader(GL_VERTEX_SHADER);
	if (OpenGLVersion == GL_Core)
		DrawTileGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawTileFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	//DrawComplexSurface SinglePass
	DrawComplexVertSinglePassObject = glCreateShader(GL_VERTEX_SHADER);
	DrawComplexFragSinglePassObject = glCreateShader(GL_FRAGMENT_SHADER);
	//DrawComplexGeoSinglePassObject = glCreateShader(GL_GEOMETRY_SHADER);
	CHECK_GL_ERROR();

	//DrawGouraudPolygons.
	DrawGouraudVertObject = glCreateShader(GL_VERTEX_SHADER);
	DrawGouraudFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	//DrawGouraudMeshBuffer.
	DrawGouraudMeshBufferVertObject = glCreateShader(GL_VERTEX_SHADER);
	DrawGouraudMeshBufferFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	// TODO!!! Add ini entries to handle shaders dynamically
	LoadShader(TEXT("xopengl/DrawSimple.vert"), DrawSimpleVertObject);
	//LoadShader(TEXT("xopengl/DrawSimple.geo"), DrawSimpleGeoObject);
	LoadShader(TEXT("xopengl/DrawSimple.frag"), DrawSimpleFragObject);

	LoadShader(TEXT("xopengl/DrawTile.vert"), DrawTileVertObject);
	if (OpenGLVersion == GL_Core)
		LoadShader(TEXT("xopengl/DrawTile.geo"), DrawTileGeoObject);
	LoadShader(TEXT("xopengl/DrawTile.frag"), DrawTileFragObject);

	LoadShader(TEXT("xopengl/DrawGouraud.vert"), DrawGouraudVertObject);
	LoadShader(TEXT("xopengl/DrawGouraud.frag"), DrawGouraudFragObject);

	LoadShader(TEXT("xopengl/DrawGouraudMeshBuffer.vert"), DrawGouraudMeshBufferVertObject);
	LoadShader(TEXT("xopengl/DrawGouraudMeshBuffer.frag"), DrawGouraudMeshBufferFragObject);

	LoadShader(TEXT("xopengl/DrawComplexSinglePass.vert"), DrawComplexVertSinglePassObject);
	//LoadShader(TEXT("xopengl/DrawComplexSinglePass.geo"), DrawComplexGeoSinglePassObject);
	LoadShader(TEXT("xopengl/DrawComplexSinglePass.frag"), DrawComplexFragSinglePassObject);

	DrawSimpleProg = glCreateProgram();
	DrawTileProg = glCreateProgram();
	DrawComplexSinglePassProg = glCreateProgram();
	//DrawComplexHWLightsProg = glCreateProgram();
	DrawGouraudProg = glCreateProgram();
	DrawGouraudMeshBufferProg = glCreateProgram();
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

	glAttachShader(DrawComplexSinglePassProg, DrawComplexVertSinglePassObject);
	glAttachShader(DrawComplexSinglePassProg, DrawComplexFragSinglePassObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawGouraudProg, DrawGouraudVertObject);
	glAttachShader(DrawGouraudProg, DrawGouraudFragObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawGouraudMeshBufferProg, DrawGouraudMeshBufferVertObject);
	glAttachShader(DrawGouraudMeshBufferProg, DrawGouraudMeshBufferFragObject);
	CHECK_GL_ERROR();

	glLinkProgram(DrawSimpleProg);
	glLinkProgram(DrawTileProg);
	glLinkProgram(DrawComplexSinglePassProg);
	//glLinkProgram(DrawComplexHWLightsProg);
	glLinkProgram(DrawGouraudProg);
	glLinkProgram(DrawGouraudMeshBufferProg);
	CHECK_GL_ERROR();

	//Global Matrix setup
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexSinglePassProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawComplexSinglePassProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawGouraudProg"));
	//GetUniformBlockIndex(GlobalUniformBlockIndex, DrawGouraudMeshBufferProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawGouraudMeshBufferProg")); // not in use (now) for BufferedMeshList.

	//DrawSimple vars.
	FString DrawSimple = TEXT("DrawSimple");
	GetUniformLocation(DrawSimplebHitTesting, DrawSimpleProg, "bHitTesting", DrawSimple);
	GetUniformLocation(DrawSimpleLineFlags, DrawSimpleProg, "LineFlags", DrawSimple);

	//DrawTile vars.
	FString DrawTile = TEXT("DrawTile");
	GetUniformLocation(DrawTileTexCoords, DrawTileProg, "TexCoords", DrawTile);
	GetUniformLocation(DrawTilebHitTesting, DrawTileProg, "bHitTesting", DrawTile);
	GetUniformLocation(DrawTileDrawColor, DrawTileProg, "DrawColor", DrawTile);
	GetUniformLocation(DrawTileTexture, DrawTileProg, "Texture0", DrawTile);
	GetUniformLocation(DrawTilePolyFlags, DrawTileProg, "PolyFlags", DrawTile);
	GetUniformLocation(DrawTileGamma, DrawTileProg, "Gamma", DrawTile);

	//DrawComplexSinglePass vars.
	FString DrawComplexSinglePass = TEXT("DrawComplexSinglePass");
	GetUniformLocation(DrawComplexSinglePassComplexAdds, DrawComplexSinglePassProg, "ComplexAdds", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassPolyFlags, DrawComplexSinglePassProg, "PolyFlags", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassRendMap, DrawComplexSinglePassProg, "RendMap", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassbHitTesting, DrawComplexSinglePassProg, "bHitTesting", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassDrawColor, DrawComplexSinglePassProg, "DrawColor", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogColor, DrawComplexSinglePassProg, "FogParams.FogColor", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogStart, DrawComplexSinglePassProg, "FogParams.FogStart", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogEnd, DrawComplexSinglePassProg, "FogParams.FogEnd", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogDensity, DrawComplexSinglePassProg, "FogParams.FogDensity", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogMode, DrawComplexSinglePassProg, "FogParams.FogMode", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassGamma, DrawComplexSinglePassProg, "Gamma", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassLightPos, DrawComplexSinglePassProg, "LightPos", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassTexCoords, DrawComplexSinglePassProg, "TexCoords", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassFogMode, DrawComplexSinglePassProg, "FogMode", DrawComplexSinglePass);
	// Multitextures in DrawComplexSinglePassProg and DrawGouraudProg
	for (INT i = 0; i < 8; i++)
	{
		GetUniformLocation(DrawComplexSinglePassTexture[i], DrawComplexSinglePassProg, (char*) (TCHAR_TO_ANSI(*FString::Printf(TEXT("Texture%i"), i))), DrawComplexSinglePass);
	}

	//DrawGouraud vars.
	FString DrawGouraud = TEXT("DrawGouraud");
	GetUniformLocation(DrawGouraudPolyFlags, DrawGouraudProg, "PolyFlags", DrawGouraud);
	GetUniformLocation(DrawGouraudbHitTesting, DrawGouraudProg, "bHitTesting", DrawGouraud);
	GetUniformLocation(DrawGouraudFogColor, DrawGouraudProg, "FogParams.FogColor", DrawGouraud);
	GetUniformLocation(DrawGouraudFogStart, DrawGouraudProg, "FogParams.FogStart", DrawGouraud);
	GetUniformLocation(DrawGouraudFogEnd, DrawGouraudProg, "FogParams.FogEnd", DrawGouraud);
	GetUniformLocation(DrawGouraudFogDensity, DrawGouraudProg, "FogParams.FogDensity", DrawGouraud);
	GetUniformLocation(DrawGouraudFogMode, DrawGouraudProg, "FogParams.FogMode", DrawGouraud);
	GetUniformLocation(DrawGouraudTexUV, DrawGouraudProg, "DrawGouraudTexUV", DrawGouraud);
	GetUniformLocation(DrawGouraudDetailTexUV, DrawGouraudProg, "DrawGouraudDetailTexUV", DrawGouraud);
	GetUniformLocation(DrawGouraudDrawColor, DrawGouraudProg, "DrawColor", DrawGouraud);
	GetUniformLocation(DrawGouraudGamma, DrawGouraudProg, "Gamma", DrawGouraud);
	GetUniformLocation(DrawGouraudPolyFlags, DrawGouraudProg, "PolyFlags", DrawGouraud);
	GetUniformLocation(DrawGouraudDetailTexUV, DrawGouraudProg, "DrawGouraudDetailTexUV", DrawGouraud);
	// Multitextures DrawGouraudProg
	for (INT i = 0; i < 8; i++)
	{
		GetUniformLocation(DrawGouraudTexture[i], DrawGouraudProg, (char *) (TCHAR_TO_ANSI(*FString::Printf(TEXT("Texture%i"), i))), DrawGouraud);
	}

	//DrawGouraudMeshBuffer vars (unused right now).
	FString DrawGouraudMeshBuffer = TEXT("DrawGouraudMeshBuffer");
	GetUniformLocation(DrawGouraudMeshBufferModelMat, DrawGouraudMeshBufferProg, "modelMat", DrawGouraudMeshBuffer);
	GetUniformLocation(DrawGouraudMeshBufferViewMat, DrawGouraudMeshBufferProg, "viewMat", DrawGouraudMeshBuffer);
	GetUniformLocation(DrawGouraudMeshBufferProjMat, DrawGouraudMeshBufferProg, "projMat", DrawGouraudMeshBuffer);
	GetUniformLocation(DrawGouraudMeshBufferDrawColor, DrawGouraudMeshBufferProg, "DrawColor", DrawGouraudMeshBuffer);

	glGenBuffers(1, &GlobalMatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2*sizeof(glm::mat4)), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalMatricesBindingIndex, GlobalMatricesUBO, 0, sizeof(glm::mat4)*2);
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
	glGenBuffers(1, &DrawComplexVertBufferSinglePass);
	glGenVertexArrays(1, &DrawComplexVertsSingleBufferVao);
	glGenVertexArrays(1, &DrawComplexVertListSingleBufferVao);
	glGenVertexArrays(1, &DrawComplexVertsSinglePassVao);
	CHECK_GL_ERROR();

	//DrawGouraud
	glGenBuffers(1, &DrawGouraudVertBufferSingle);
	glGenBuffers(1, &DrawGouraudVertBufferInterleaved);
	glGenBuffers(1, &DrawGouraudVertBufferListSingle);
	glGenVertexArrays(1, &DrawGouraudPolyVertsVao);
	glGenVertexArrays(1, &DrawGouraudPolyVertListVao);
	glGenVertexArrays(1, &DrawGouraudPolyVertsSingleBufferVao);
	glGenVertexArrays(1, &DrawGouraudPolyVertListSingleBufferVao);
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
	if (DrawSimpleVertBuffer)
		glDeleteBuffers(1, &DrawSimpleVertBuffer);
	if (DrawTileVertBuffer)
		glDeleteBuffers(1, &DrawTileVertBuffer);
	if (DrawGouraudVertBufferSingle)
		glDeleteBuffers(1, &DrawGouraudVertBufferSingle);
	if (DrawGouraudVertBufferInterleaved)
		glDeleteBuffers(1, &DrawGouraudVertBufferInterleaved);
	if (DrawGouraudVertBufferListSingle)
		glDeleteBuffers(1, &DrawGouraudVertBufferListSingle);
	if (DrawComplexVertBufferSinglePass)
		glDeleteBuffers(1, &DrawComplexVertBufferSinglePass);

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
	unguard;
}

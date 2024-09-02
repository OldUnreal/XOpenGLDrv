/*=============================================================================
	UnShader.cpp: Unreal XOpenGL shader support code.

	Copyright 2014-2017 OldUnreal

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

static void EmitGlobals(UXOpenGLRenderDevice::ShaderProgram* Program, UXOpenGLRenderDevice::ShaderOptions Options, GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, bool HaveGeoShader)
{
	// VERSION
	if (GL->OpenGLVersion == GL_Core)
	{
		if (GL->UsingShaderDrawParameters)
			Out << "#version 460 core" END_LINE;
		else if (GL->UsingBindlessTextures || GL->UsingPersistentBuffers)
			Out << "#version 450 core" END_LINE;
		else
			Out << "#version 330 core" END_LINE;
	}
	else
	{	  
	  Out << "#version 310 es" END_LINE;
	  Out << "#extension GL_OES_shader_io_blocks : require" END_LINE;
	}

	if (GL->UsingBindlessTextures)
		Out << "#extension GL_ARB_bindless_texture : require" END_LINE;

	if (GL->UsingShaderDrawParameters)
	{
		Out << R"(
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_shading_language_420pack : require
)";
	}

	if (GL->OpenGLVersion == GL_ES)
	{
		Out << R"(
// The following extension appears not to be available on RaspberryPi4 at the moment.
#extension GL_EXT_clip_cull_distance : enable

// This determines how much precision the GPU uses when calculating. 
// Performance critical (especially on low end)!! 
// Not every option is available on any platform. 
// TODO: separate option for vert and frag?
// options: lowp/mediump/highp, should be mediump for performance reasons, but appears to cause trouble determining DrawFlags then !?! (Currently on NVIDIA 470.103.01).
precision lowp float;
precision lowp int;
)";
	}

	// Emit specialization options
	Out << appToAnsi(*Options.GetPreprocessorString());

	// Emit hardcoded options
	Out << "#define OPT_GLCore " << (GL->OpenGLVersion == GL_Core ? 1 : 0) << END_LINE;
	Out << "#define OPT_GLES " << (GL->OpenGLVersion == GL_Core ? 0 : 1) << END_LINE;
	Out << "#define OPT_GeometryShaders " << (GL->UsingGeometryShaders ? 1 : 0) << END_LINE;
	Out << "#define OPT_DetailMax " << GL->DetailMax << END_LINE;
	Out << "#define OPT_SimulateMultiPass " << (GL->SimulateMultiPass ? 1 : 0) << END_LINE;
	Out << "#define OPT_HWLighting " << (GL->UseHWLighting ? 1 : 0) << END_LINE;
	Out << "#define OPT_SupportsClipDistance " << (GL->SupportsClipDistance ? 1 : 0) << END_LINE;
	Out << "#define OPT_MaxClippingPlanes " << GL->MaxClippingPlanes << END_LINE;
	Out << "#define OPT_Editor " << (GIsEditor ? 1 : 0) << END_LINE;

	// Emit engine constants
#if ENGINE_VERSION==227
	Out << "#define LE_Sunlight " << LE_Sunlight << END_LINE;
#else
	Out << "#define LE_Sunlight " << (LE_MAX + 1) << END_LINE;
#endif
	Out << "#define ENGINE_VERSION " << ENGINE_VERSION << END_LINE;	
	Out << "#define MAX_LIGHTS " << MAX_LIGHTS << END_LINE;	

	if (GIsEditor)
	{
		Out << "#define REN_None " << REN_None << "u" << END_LINE;
		Out << "#define REN_Wire " << REN_Wire << "u" << END_LINE;
		Out << "#define REN_Zones " << REN_Zones << "u" << END_LINE;
		Out << "#define REN_Polys " << REN_Polys << "u" << END_LINE;
		Out << "#define REN_PolyCuts " << REN_PolyCuts << "u" << END_LINE;
		Out << "#define REN_DynLight " << REN_DynLight << "u" << END_LINE;
		Out << "#define REN_PlainTex " << REN_PlainTex << "u" << END_LINE;
		Out << "#define REN_OrthXY " << REN_OrthXY << "u" << END_LINE;
		Out << "#define REN_OrthXZ " << REN_OrthXZ << "u" << END_LINE;
		Out << "#define REN_TexView " << REN_TexView << "u" << END_LINE;
		Out << "#define REN_TexBrowser " << REN_TexBrowser << "u" << END_LINE;
		Out << "#define REN_MeshView " << REN_MeshView << "u" << END_LINE;
#if ENGINE_VERSION==227
		Out << "#define REN_Normals " << REN_Normals << "u" << END_LINE;
#else
		Out << "#define REN_Normals " << (REN_MAX + 1) << "u" << END_LINE;
#endif
	}

	Out << R"(
layout(std140) uniform FrameState
{  
  mat4 projMat;
  mat4 viewMat;
  mat4 modelMat;
  mat4 modelviewMat;
  mat4 modelviewprojMat;
  mat4 lightSpaceMat;
  mat4 FrameCoords;
  mat4 FrameUncoords;
  float Gamma;
  float LightMapIntensity;
  float LightColorIntensity;  
};
)";

	for (INT i = 0; i < Program->NumTextureSamplers; ++i)
		Out << "uniform sampler2D Texture" << i << ";" << END_LINE;

	Out << R"(
// Light information.
layout(std140) uniform LightInfo
{  
  vec4 LightData1[MAX_LIGHTS]; // LightColor.R, LightColor.G, LightColor.B, LightCone
  vec4 LightData2[MAX_LIGHTS]; // LightEffect, LightPeriod, LightPhase, LightRadius
  vec4 LightData3[MAX_LIGHTS]; // LightType, VolumeBrightness, VolumeFog, VolumeRadius
  vec4 LightData4[MAX_LIGHTS]; // WorldLightRadius, NumLights, ZoneNumber, CameraRegion->ZoneNumber
  vec4 LightData5[MAX_LIGHTS]; // NormalLightRadius, bZoneNormalLight, LightBrightness, unused
  vec4 LightPos[MAX_LIGHTS];
};

layout(std140) uniform ClipPlaneParams
{
  vec4  ClipParams; // Clipping params, ClipIndex,0,0,0
  vec4  ClipPlane;  // Clipping planes. Plane.X, Plane.Y, Plane.Z, Plane.W
};

#if OPT_Editor
layout(std140) uniform EditorState
{
  uint HitTesting;
  uint RendMap;
};
#endif

#if OPT_DistanceFog
struct FogParameters
{
  vec4  FogColor;   // Fog color
  float FogStart;   // Only for linear fog
  float FogEnd;     // Only for linear fog
  float FogDensity; // For exp and exp2 equation
  float FogCoord;
  int FogMode;      // 0 = linear, 1 = exp, 2 = exp2
};

float getFogFactor(FogParameters DistanceFog)
{
  // DistanceFogValues.x = FogStart
  // DistanceFogValues.y = FogEnd
  // DistanceFogValues.z = FogDensity
  // DistanceFogValues.w = FogMode
  // FogResult = (Values.y-FogCoord)/(Values.y-Values.x);

  float FogResult = 1.0;
  if (DistanceFog.FogMode == 0)
    FogResult = ((DistanceFog.FogEnd - DistanceFog.FogCoord) / (DistanceFog.FogEnd - DistanceFog.FogStart));
  else if (DistanceFog.FogMode == 1)
    FogResult = exp(-DistanceFog.FogDensity * DistanceFog.FogCoord);
  else if (DistanceFog.FogMode == 2)
    FogResult = exp(-pow(DistanceFog.FogDensity * DistanceFog.FogCoord, 2.0));
  FogResult = 1.0 - clamp(FogResult, 0.0, 1.0);
  return FogResult;
}
#endif

float PlaneDot(vec4 Plane, vec3 Point)
{
  return dot(Plane.xyz, Point) - Plane.w;
}

vec4 GammaCorrect(float Gamma, vec4 Color)
{
    float InvGamma = 1.0 / Gamma;
    Color.r = pow(Color.r, InvGamma);
    Color.g = pow(Color.g, InvGamma);
    Color.b = pow(Color.b, InvGamma);
    return Color;
}

vec3 rgb2hsv(vec3 c)
{
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0); // some nice stuff from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
  vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
  vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);
  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
)";

	if (GL->UsingBindlessTextures)
	{
		Out << R"(
vec4 GetTexel(uvec2 BindlessTexHandle, sampler2D BoundSampler, vec2 TexCoords)
{
  return texture(sampler2D(BindlessTexHandle), TexCoords);
}
)";
	}
	else
	{
		// texture bound to TMU. BindlessTexBum is meaningless here
		Out << R"(
vec4 GetTexel(uvec2 BindlessTexHandle, sampler2D BoundSampler, vec2 TexCoords)
{
  return texture(BoundSampler, TexCoords);
}
)";
	}

	if (ShaderType == GL_FRAGMENT_SHADER)
	{
		Out << R"(
vec4 ApplyPolyFlags(vec4 Color)
{
#if OPT_Masked
    if (Color.a < 0.5)
      discard;
    else Color.rgb /= Color.a;
#elif OPT_AlphaBlended
    if (Color.a < 0.01)
      discard;
#endif
  return Color;
}
)";
	}

	if (ShaderType == GL_VERTEX_SHADER)
	{
		Out << "flat out uint vDrawID;" END_LINE;
	}
	else if (ShaderType == GL_GEOMETRY_SHADER)
	{
		Out << "flat in uint vDrawID[];" END_LINE;
		Out << "flat out uint gDrawID;" END_LINE;
	}
	else if (HaveGeoShader)
	{
		Out << "flat in uint gDrawID;" END_LINE;
	}
	else
	{
		Out << "flat in uint vDrawID;" END_LINE;
	}

    // The following directive resets the line number to 1 
    // to have the correct output logging for a possible 
    // error within the shader files.
	Out << "#line 1" END_LINE;
}

static void GetTypeInfo(const char* TypeName, INT& SizeBytes, INT& Components)
{
	if (!strcmp("vec4", TypeName) || !strcmp("uvec4", TypeName))
	{
		SizeBytes = 16;
		Components = 4;
	}
	else if (!strcmp("int", TypeName) || !strcmp("uint", TypeName) || !strcmp("float", TypeName))
	{
		SizeBytes = 4;
		Components = 1;
	}
	else
		appErrorf(TEXT("Unknown GLSL type: %ls"), appFromAnsi(TypeName));
}

INT UXOpenGLRenderDevice::ShaderProgram::GetMaximumUniformBufferSize(const DrawCallParameterInfo* Info) const
{
	INT TotalSizeBytes = 0;
	INT TotalComponents = 0;

	while (true)
	{
		if (!Info->Type)
			break;

		INT Size = 0, Components = 0;
		GetTypeInfo(Info->Type, Size, Components);
		TotalSizeBytes += Size * Max<INT>(Info->ArrayCount, 1);
		TotalComponents += Components * Max<INT>(Info->ArrayCount, 1);
		Info++;
	}

	// TODO: Check the various component limits

	if (TotalSizeBytes > 0)
		return (RenDev->MaxUniformBlockSize / TotalSizeBytes / 2);
	return 16;
}

void UXOpenGLRenderDevice::ShaderProgram::EmitDrawCallParametersHeader
(
	const DrawCallParameterInfo* Info, 
	FShaderWriterX& Out, 
	ShaderProgram* Program, 
	INT BufferBindingIndex, 
	bool UseSSBO, 
	bool EmitGetters
)
{
	// Emit draw call parameters struct
	Out << R"(struct DrawCallParameters
{
)";

	auto OrigInfo = Info;
	while (true)
	{
		if (!Info->Type)
			break;

		Out << "  " << Info->Type << " " << Info->Name;
		if (Info->ArrayCount > 0)
			Out << "[" << Info->ArrayCount << "]";
		Out << ";" END_LINE;
		Info++;
	}
	Info = OrigInfo;

	Out << "};" END_LINE;

	// Emit SSBO buffer (if necessary)
	if (UseSSBO)
	{
		Out << R"(
layout(std430, binding = )" << BufferBindingIndex << R"() buffer All)" << appToAnsi(Program->ShaderName) << R"(ShaderDrawParams
{
  DrawCallParameters Draw)" << appToAnsi(Program->ShaderName) << R"(Params[];
};
)";
	}
	else
	{
		//, binding = )" << BufferBindingIndex << R"(
		Out << R"(
layout(std140) uniform All)" << appToAnsi(Program->ShaderName) << R"(ShaderDrawParams
{
  DrawCallParameters Draw)"	<< appToAnsi(Program->ShaderName) << R"(Params[)" << Program->ParametersBufferSize << R"(];
};
)";
	}
	
	if (!EmitGetters)
		return;

	// Emit getters
	while (true)
	{
		if (!Info->Type)
			break;

		Out << Info->Type << " Get" << Info->Name;
		if (Info->ArrayCount > 0)
			Out << "(uint DrawID, int Index)" END_LINE;
		else
			Out << "(uint DrawID)" END_LINE;
		Out << "{" END_LINE;

		Out << "  return Draw"
			<< appToAnsi(Program->ShaderName)
			<< "Params[DrawID]."
			<< Info->Name;
		if (Info->ArrayCount > 0)
			Out << "[Index]";
		Out << ";" END_LINE;

		Out << "}" END_LINE;
		Info++;
	}
}

// Helpers
void UXOpenGLRenderDevice::ShaderProgram::BindUniform(ShaderSpecialization* Specialization, const GLuint BindingIndex, const char* Name) const
{
	const GLuint BlockIndex = glGetUniformBlockIndex(Specialization->ShaderProgramObject, Name);
	if (BlockIndex == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), ShaderName);
		if (RenDev->UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), ShaderName);
	}
	glUniformBlockBinding(Specialization->ShaderProgramObject, BlockIndex, BindingIndex);
}

void UXOpenGLRenderDevice::ShaderProgram::GetUniformLocation(ShaderSpecialization* Specialization, GLint& Uniform, const char* Name) const
{
	Uniform = glGetUniformLocation(Specialization->ShaderProgramObject, Name);
	if (Uniform == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), ShaderName);
		if (RenDev->UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), ShaderName);
	}
}

static const TCHAR* ShaderTypeString(GLuint ShaderType)
{
	switch (ShaderType)
	{
		case GL_VERTEX_SHADER: return TEXT("Vertex");
		case GL_GEOMETRY_SHADER: return TEXT("Geometry");
		case GL_FRAGMENT_SHADER: return TEXT("Fragment");
		default: return TEXT("Unknown");
	}
}

static void DumpShader(const char* Source, bool AddLineNumbers)
{
	FString ShaderSource(Source);
	TArray<FString> Lines;
	INT LineNum = 0;
	while (true)
	{
		const INT NewLine = ShaderSource.InStr(TEXT("\n"));
		FString Line = NewLine >= 0 ? ShaderSource.Left(NewLine) : ShaderSource;
		if (Line.Right(1) == TEXT("\r"))
			Line = Line.Left(Line.Len() - 1);
		if (Line.InStr(TEXT("#line 1")) == 0)
			LineNum = 0;
		if (AddLineNumbers)
			Lines.AddItem(FString::Printf(TEXT("%03d:\t%ls"), LineNum++, *Line));
		else
			Lines.AddItem(*Line);
		if (NewLine == -1)
			break;
		ShaderSource = ShaderSource.Mid(NewLine + 1);
	}
	debugf(TEXT("XOpenGL: Shader Source:"));
	for (INT i = 0; i < Lines.Num(); ++i)
		debugf(TEXT("%ls"), *Lines(i));
}

bool UXOpenGLRenderDevice::ShaderProgram::CompileShaderFunction(GLuint ShaderFunctionObject, GLuint FunctionType, ShaderOptions Options, ShaderWriterFunc Func, bool HaveGeoShader)
{
	guard(CompileShader);
	FShaderWriterX ShOut;
	EmitGlobals(this, Options, FunctionType, RenDev, ShOut, HaveGeoShader);

	if (ParametersInfo)
		EmitDrawCallParametersHeader(ParametersInfo, ShOut, this, ParametersBufferBindingIndex, UseSSBOParametersBuffer, true);

	(*Func)(FunctionType, RenDev, ShOut);

	GLint blen = 0;
	GLsizei slen = 0;
	GLint IsCompiled = 0;
	const GLchar* Shader = *ShOut;
	GLint length = ShOut.Length();
	bool Result = true;
	glShaderSource(ShaderFunctionObject, 1, &Shader, &length);
	glCompileShader(ShaderFunctionObject);

	glGetShaderiv(ShaderFunctionObject, GL_COMPILE_STATUS, &IsCompiled);
	if (!IsCompiled)
	{		
		GWarn->Logf(TEXT("XOpenGL: Failed compiling %ls %ls Shader (Options %ls)"), ShaderName, ShaderTypeString(FunctionType), *Options.GetShortString());
		glGetShaderiv(ShaderFunctionObject, GL_INFO_LOG_LENGTH, &blen);
		if (blen > 1)
		{
			GLchar* compiler_log = new GLchar[blen + 1];
			glGetShaderInfoLog(ShaderFunctionObject, blen, &slen, compiler_log);
			debugf(TEXT("XOpenGL: ErrorLog compiling %ls %ls"), ShaderName, appFromAnsi(compiler_log));
			delete[] compiler_log;
		}

		DumpShader(*ShOut, true);
		Result = false;
	}
	else 
	{
		glGetShaderiv(ShaderFunctionObject, GL_INFO_LOG_LENGTH, &blen);
		if (blen > 1)
		{
			GLchar* compiler_log = new GLchar[blen + 1];
			glGetShaderInfoLog(ShaderFunctionObject, blen, &slen, compiler_log);
			debugf(NAME_DevGraphics, TEXT("XOpenGL: Log compiling %ls %ls"), ShaderName, appFromAnsi(compiler_log));
			delete[] compiler_log;
		}
		else debugf(NAME_DevGraphics, TEXT("XOpenGL: No compiler messages for %ls %ls Shader (Options %ls)"), ShaderName, ShaderTypeString(FunctionType), *Options.GetShortString());
	}

	return Result;
	unguard;
}

bool UXOpenGLRenderDevice::ShaderProgram::LinkShaderProgram(GLuint ShaderProgramObject) const
{
	guard(UXOpenGLRenderDevice::LinkShader);
	GLint IsLinked = 0;
	GLint blen = 0;
	GLsizei slen = 0;
	bool Result = true;
	glLinkProgram(ShaderProgramObject);
	glGetProgramiv(ShaderProgramObject, GL_LINK_STATUS, &IsLinked);

	if (!IsLinked)
	{
		GWarn->Logf(TEXT("XOpenGL: Failed linking %ls"), ShaderName);
		Result = false;
	}

	glGetProgramiv(ShaderProgramObject, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* linker_log = new GLchar[blen + 1];
		glGetProgramInfoLog(ShaderProgramObject, blen, &slen, linker_log);
		debugf(TEXT("XOpenGL: Log linking %ls %ls"), ShaderName, appFromAnsi(linker_log));
		delete[] linker_log;
	}
	else debugf(NAME_DevGraphics, TEXT("XOpenGL: No linker messages for %ls"), ShaderName);

	CHECK_GL_ERROR();
	return Result;
	unguard;
}

void UXOpenGLRenderDevice::InitShaders()
{
	guard(UXOpenGLRenderDevice::InitShaders);

	memset(Shaders, 0, sizeof(Shaders));
	Shaders[No_Prog]				= new NoProgram(TEXT("No"), this);
	Shaders[Simple_Triangle_Prog]	= new DrawSimpleTriangleProgram(TEXT("DrawSimpleTriangle"), this);
	Shaders[Simple_Line_Prog]		= new DrawSimpleLineProgram(TEXT("DrawSimpleLine"), this);
	Shaders[Tile_Prog]				= (OpenGLVersion == GL_Core && UsingGeometryShaders) 
		? static_cast<ShaderProgram*>(new DrawTileCoreProgram(TEXT("DrawTile"), this)) 
		: static_cast<ShaderProgram*>(new DrawTileESProgram(TEXT("DrawTile"), this));
	Shaders[Gouraud_Prog]			= new DrawGouraudProgram(TEXT("DrawGouraud"), this);
	Shaders[Complex_Prog]			= new DrawComplexProgram(TEXT("DrawComplex"), this);

	// (Re)initialize UBOs
	if (!FrameStateBuffer.Buffer)
	{
		FrameStateBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::FrameStateIndex);
		FrameStateBuffer.MapUBOBuffer(false, 1);
		FrameStateBuffer.Advance(1);
	}
	else
	{
		FrameStateBuffer.RebindBufferBase(GlobalShaderBindingIndices::FrameStateIndex);
	}
	
	if (!LightInfoBuffer.Buffer)
	{
		LightInfoBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::LightInfoIndex);
		LightInfoBuffer.MapUBOBuffer(false, 1);
		LightInfoBuffer.Advance(1);
	}
	else
	{
		LightInfoBuffer.RebindBufferBase(GlobalShaderBindingIndices::LightInfoIndex);
	}

	if (!GlobalClipPlaneBuffer.Buffer)
	{
		GlobalClipPlaneBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::ClipPlaneIndex);
		GlobalClipPlaneBuffer.MapUBOBuffer(false, 1);
		GlobalClipPlaneBuffer.Advance(1);
	}
	else
	{
		GlobalClipPlaneBuffer.RebindBufferBase(GlobalShaderBindingIndices::ClipPlaneIndex);
	}

	if (GIsEditor)
	{
		if (!EditorStateBuffer.Buffer)
		{
			EditorStateBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::EditorStateIndex);
			EditorStateBuffer.MapUBOBuffer(false, 1);
			EditorStateBuffer.Advance(1);
		}
		else
		{
			EditorStateBuffer.RebindBufferBase(GlobalShaderBindingIndices::EditorStateIndex);
		}
	}
	
	for (const auto Shader : Shaders)
	{
		if (!Shader)
			continue;

		Shader->MapBuffers();
		Shader->BuildCommonSpecializations();
	}

	unguard;
}

void UXOpenGLRenderDevice::ResetShaders()
{
	guard(UXOpenGLRenderDevice::ResetShaders);

	if (UBOPoint)
		UBOPoint->Unbind();
	if (SSBOPoint)
		SSBOPoint->Unbind();
	if (ArrayPoint)
		ArrayPoint->Unbind();

	UBOPoint = SSBOPoint = ArrayPoint = nullptr;
	
	for (const auto Shader: Shaders)
		delete Shader;
	memset(Shaders, 0, sizeof(ShaderProgram*) * ARRAY_COUNT(Shaders));
		
	unguard;
}

void UXOpenGLRenderDevice::RecompileShaders()
{
	// This forces us to select new shader specalizations
	CachedPolyFlags = 0;
	CachedShaderOptions = 0;
}

void UXOpenGLRenderDevice::ShaderProgram::BindShaderState(ShaderSpecialization* Specialization)
{
	BindUniform(Specialization, FrameStateIndex, "FrameState");
	BindUniform(Specialization, LightInfoIndex, "LightInfo");
	BindUniform(Specialization, ClipPlaneIndex, "ClipPlaneParams");
	if (GIsEditor)
		BindUniform(Specialization, EditorStateIndex, "EditorState");

	if (!UseSSBOParametersBuffer)
		BindUniform(Specialization, ParametersBufferBindingIndex, appToAnsi(*FString::Printf(TEXT("All%lsShaderDrawParams"), ShaderName)));

	// Bind regular texture samplers to their respective TMUs
	check(NumTextureSamplers >= 0 && NumTextureSamplers < 9);
	for (INT i = 0; i < NumTextureSamplers; i++)
	{
		GLint MultiTextureUniform;
		GetUniformLocation(Specialization, MultiTextureUniform, appToAnsi(*FString::Printf(TEXT("Texture%i"), i)));
		if (MultiTextureUniform != -1)
			glUniform1i(MultiTextureUniform, i);
	}
}

void UXOpenGLRenderDevice::ShaderProgram::SelectShaderSpecialization(ShaderOptions Options)
{
	// Fast path to avoid an expensive pipeline state lookup
	if (Options == LastOptions && LastSpecialization)
		return;

	// We have to change to a different shader. Make sure we flush all buffered data
	Flush(false);

	// We have to switch to a different specialization. See if we've already compiled it
	auto Specialization = Specializations.FindRef(Options);
	if (Specialization)
	{
		LastOptions = Options;
		LastSpecialization = Specialization;
		glUseProgram(Specialization->ShaderProgramObject);
		return;
	}

	// Awww, we'll have to compile it
	Specialization = new ShaderSpecialization;
	Specializations.Set(Options, Specialization);

	Specialization->Options = Options;
	Specialization->SpecializationName = FString::Printf(TEXT("%ls%ls"), ShaderName, *Options.GetShortString());

	check(BuildShaderProgram(Specialization, VertexShaderFunc, GeoShaderFunc, FragmentShaderFunc));

	LastOptions = Options;
	LastSpecialization = Specialization;
	glUseProgram(Specialization->ShaderProgramObject);
	BindShaderState(Specialization);
}

bool UXOpenGLRenderDevice::ShaderProgram::BuildShaderProgram(ShaderSpecialization* Specialization, ShaderWriterFunc VertexShaderFunc, ShaderWriterFunc GeoShaderFunc, ShaderWriterFunc FragmentShaderFunc)
{
	Specialization->VertexShaderObject   = glCreateShader(GL_VERTEX_SHADER);
	if (GeoShaderFunc)
		Specialization->GeoShaderObject  = glCreateShader(GL_GEOMETRY_SHADER);
	Specialization->FragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);

	if (!CompileShaderFunction(Specialization->VertexShaderObject, GL_VERTEX_SHADER, Specialization->Options, VertexShaderFunc, GeoShaderFunc != nullptr) ||
		(GeoShaderFunc && !CompileShaderFunction(Specialization->GeoShaderObject, GL_GEOMETRY_SHADER, Specialization->Options, GeoShaderFunc, GeoShaderFunc != nullptr)) ||
		!CompileShaderFunction(Specialization->FragmentShaderObject, GL_FRAGMENT_SHADER, Specialization->Options, FragmentShaderFunc, GeoShaderFunc != nullptr))
		return false;

	Specialization->ShaderProgramObject = glCreateProgram();
	glAttachShader(Specialization->ShaderProgramObject, Specialization->VertexShaderObject);
	if (GeoShaderFunc)
		glAttachShader(Specialization->ShaderProgramObject, Specialization->GeoShaderObject);
	glAttachShader(Specialization->ShaderProgramObject, Specialization->FragmentShaderObject);

	if (!LinkShaderProgram(Specialization->ShaderProgramObject))
		return false;

	// stijn: We should detach and delete compiled shader objects immediately since they can consume a lot of RAM (especially with the AMD RDNA3 drivers)
	glDetachShader(Specialization->ShaderProgramObject, Specialization->VertexShaderObject);
	if (GeoShaderFunc)
		glDetachShader(Specialization->ShaderProgramObject, Specialization->GeoShaderObject);
	glDetachShader(Specialization->ShaderProgramObject, Specialization->FragmentShaderObject);
	glDeleteShader(Specialization->VertexShaderObject);
	if (GeoShaderFunc)
		glDeleteShader(Specialization->GeoShaderObject);
	glDeleteShader(Specialization->FragmentShaderObject);

	Specialization->VertexShaderObject = Specialization->GeoShaderObject = Specialization->FragmentShaderObject = 0;
	
	return true;
}

void UXOpenGLRenderDevice::ShaderProgram::DeleteShaders()
{
	for (TMap<ShaderOptions, ShaderSpecialization*>::TIterator It(Specializations); It; ++It)
	{
		const auto Specialization = It.Value();

		if (!Specialization->ShaderProgramObject)
			continue;

		if (Specialization->VertexShaderObject)
			glDetachShader(Specialization->ShaderProgramObject, Specialization->VertexShaderObject);

		if (Specialization->GeoShaderObject)
			glDetachShader(Specialization->ShaderProgramObject, Specialization->GeoShaderObject);

		if (Specialization->FragmentShaderObject)
			glDetachShader(Specialization->ShaderProgramObject, Specialization->FragmentShaderObject);

		glDeleteProgram(Specialization->ShaderProgramObject);

		delete Specialization;
	}

	Specializations.Empty();
}

UXOpenGLRenderDevice::ShaderProgram::~ShaderProgram()
= default;

/*-----------------------------------------------------------------------------
    ShaderOptions
-----------------------------------------------------------------------------*/
FString UXOpenGLRenderDevice::ShaderOptions::GetStringHelper(void (*AddOptionFunc)(FString&, const TCHAR*, bool)) const
{
    FString Result;
#define ADD_OPTION(x) \
AddOptionFunc(Result, L ## #x, (OptionsMask & x) ? true : false);
    
    ADD_OPTION(OPT_DiffuseTexture)
    ADD_OPTION(OPT_LightMap)
    ADD_OPTION(OPT_FogMap)
    ADD_OPTION(OPT_DetailTexture)
    ADD_OPTION(OPT_MacroTexture)
    ADD_OPTION(OPT_BumpMap)
    ADD_OPTION(OPT_EnvironmentMap)
    ADD_OPTION(OPT_HeightMap)
    ADD_OPTION(OPT_Masked)
    ADD_OPTION(OPT_Unlit)
    ADD_OPTION(OPT_Modulated)
    ADD_OPTION(OPT_Translucent)
    ADD_OPTION(OPT_Environment)
    ADD_OPTION(OPT_RenderFog)
    ADD_OPTION(OPT_AlphaBlended)
    ADD_OPTION(OPT_DistanceFog)
	ADD_OPTION(OPT_NoNearZ)
	ADD_OPTION(OPT_Selected)
    
    if (Result.Len() == 0)
        AddOptionFunc(Result, TEXT("OPT_None"), true);
    return Result;
}

FString UXOpenGLRenderDevice::ShaderOptions::GetShortString() const
{
    return GetStringHelper([](FString& Result, const TCHAR* OptionName, bool IsSet) {
		if (IsSet)
		{
			if (Result.Len() > 0)
				Result += TEXT("|");
			Result += OptionName;
		}
    });
}

FString UXOpenGLRenderDevice::ShaderOptions::GetPreprocessorString() const
{
    return GetStringHelper([](FString& Result, const TCHAR* OptionName, bool IsSet) {
        Result += FString::Printf(TEXT("#define %ls %d\n"), OptionName, IsSet ? 1 : 0);
    });
}

void UXOpenGLRenderDevice::ShaderOptions::SetOption(DWORD Option)
{
    OptionsMask |= Option;
}

void UXOpenGLRenderDevice::ShaderOptions::UnsetOption(DWORD Option)
{
    OptionsMask &= ~Option;
}

bool UXOpenGLRenderDevice::ShaderOptions::HasOption(DWORD Option) const
{
    return OptionsMask & Option;
}

/*-----------------------------------------------------------------------------
	Dummy Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::NoProgram::NoProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= 0;
	ParametersBufferSize			= 0;
	ParametersBufferBindingIndex	= 0;
	NumTextureSamplers				= 0;
	DrawMode						= GL_TRIANGLES;
	NumVertexAttributes				= 0;
	UseSSBOParametersBuffer			= false;
	ParametersInfo					= nullptr;
	VertexShaderFunc				= nullptr;
	GeoShaderFunc					= nullptr;
	FragmentShaderFunc				= nullptr;
}

void UXOpenGLRenderDevice::NoProgram::BuildCommonSpecializations()	{}
void UXOpenGLRenderDevice::NoProgram::CreateInputLayout()			{}
void UXOpenGLRenderDevice::NoProgram::Flush(bool Rotate)			{}
void UXOpenGLRenderDevice::NoProgram::MapBuffers()					{}
void UXOpenGLRenderDevice::NoProgram::UnmapBuffers()				{}
void UXOpenGLRenderDevice::NoProgram::ActivateShader()				{}
void UXOpenGLRenderDevice::NoProgram::DeactivateShader()			{}

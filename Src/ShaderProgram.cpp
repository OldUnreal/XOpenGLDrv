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

static void Emit_Globals(UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
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

	Out << R"(
layout(std140) uniform GlobalMatrices
{
  mat4 projMat;
  mat4 viewMat;
  mat4 modelMat;
  mat4 modelviewMat;
  mat4 modelviewprojMat;
  mat4 lightSpaceMat;
};

layout(std140) uniform ClipPlaneParams
{
  vec4  ClipParams; // Clipping params, ClipIndex,0,0,0
  vec4  ClipPlane;  // Clipping planes. Plane.X, Plane.Y, Plane.Z, Plane.W
};

// Light information.
layout(std140) uniform StaticLightInfo
{
  vec4 LightPos[)" << MAX_LIGHTS << R"(];
  vec4 LightData1[)" << MAX_LIGHTS << R"(]; // LightBrightness, LightHue, LightSaturation, LightCone
  vec4 LightData2[)" << MAX_LIGHTS << R"(]; // LightEffect, LightPeriod, LightPhase, LightRadius
  vec4 LightData3[)" << MAX_LIGHTS << R"(]; // LightType, VolumeBrightness, VolumeFog, VolumeRadius
  vec4 LightData4[)" << MAX_LIGHTS << R"(]; // WorldLightRadius, NumLights, ZoneNumber, CameraRegion->ZoneNumber
  vec4 LightData5[)" << MAX_LIGHTS << R"(]; // NormalLightRadius, bZoneNormalLight, unused, unused
};

layout(std140) uniform GlobalCoords
{
  mat4 FrameCoords;
  mat4 FrameUncoords;
};
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

	//DistanceFog
    Out << R"(
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

// The following directive resets the line number to 1 
// to have the correct output logging for a possible 
// error within the shader files.
#line 1
)";	

}

void UXOpenGLRenderDevice::ShaderProgram::EmitDrawCallParametersHeader(GLuint ShaderType, class UXOpenGLRenderDevice* GL, const DrawCallParameterInfo* Info, FShaderWriterX& Out, ShaderProgram* Program, INT BufferBindingIndex, bool UseInstanceID)
{
	// gl_DrawID is only accessible in vertex shaders
	if (GL->UsingShaderDrawParameters && ShaderType != GL_VERTEX_SHADER)
		return;

	// Emit draw call parameters struct
	if (GL->UsingShaderDrawParameters)
		Out << "struct ";
	else
		Out << "layout(std140) uniform ";

	Out << R"(DrawCallParameters
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
	if (GL->UsingShaderDrawParameters)
	{
		Out << R"(
layout(std430, binding = )" << BufferBindingIndex << R"() buffer All)" << appToAnsi(Program->ShaderName) << R"(ShaderDrawParams
{
	DrawCallParameters Draw)" << appToAnsi(Program->ShaderName) << R"(Params[];
};
)";
	}

	// Emit getters
	while (true)
	{
		if (!Info->Type)
			break;

		Out << Info->Type << " Get" << Info->Name;
		if (Info->ArrayCount > 0)
			Out << "(int Index)" END_LINE;
		else
			Out << "()" END_LINE;
		Out << "{" END_LINE;

		Out << "  return ";
		if (GL->UsingShaderDrawParameters)
		{
			Out << "Draw" << appToAnsi(Program->ShaderName);
			if (UseInstanceID)
				Out << "Params[gl_BaseInstance + gl_InstanceID].";
			else
				Out	<< "Params[gl_DrawID].";
		}
		Out << Info->Name;
		if (Info->ArrayCount > 0)
			Out << "[Index]";
		Out << ";" END_LINE;

		Out << "}" END_LINE;
		Info++;
	}
}

// Helpers
void UXOpenGLRenderDevice::ShaderProgram::BindUniform(const GLuint BindingIndex, const char* Name) const
{
	const GLuint BlockIndex = glGetUniformBlockIndex(ShaderProgramObject, Name);
	if (BlockIndex == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), ShaderName);
		if (RenDev->UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), ShaderName);
	}
	glUniformBlockBinding(ShaderProgramObject, BlockIndex, BindingIndex);
}

void UXOpenGLRenderDevice::ShaderProgram::GetUniformLocation(GLint& Uniform, const char* Name) const
{
	Uniform = glGetUniformLocation(ShaderProgramObject, Name);
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

void UXOpenGLRenderDevice::ShaderProgram::DumpShader(const char* Source, bool AddLineNumbers)
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
			Lines.AddItem(FString::Printf(TEXT("%d:\t%ls"), LineNum++, *Line));
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

bool UXOpenGLRenderDevice::ShaderProgram::CompileShader(GLuint ShaderType, GLuint& ShaderObject, ShaderWriterFunc Func, ShaderWriterFunc EmitHeaderFunc)
{
	guard(CompileShader);
	FShaderWriterX ShOut;
	Emit_Globals(RenDev, ShOut);
	if (EmitHeaderFunc)
		(*EmitHeaderFunc)(ShaderType, RenDev, ShOut, this);
	(*Func)(ShaderType, RenDev, ShOut, this);

	GLint blen = 0;
	GLsizei slen = 0;
	GLint IsCompiled = 0;
	const GLchar* Shader = *ShOut;
	GLint length = ShOut.Length();
	bool Result = true;
	glShaderSource(ShaderObject, 1, &Shader, &length);
	glCompileShader(ShaderObject);

	glGetShaderiv(ShaderObject, GL_COMPILE_STATUS, &IsCompiled);
	if (!IsCompiled)
	{		
		GWarn->Logf(TEXT("XOpenGL: Failed compiling %ls %ls Shader"), ShaderName, ShaderTypeString(ShaderType));
		glGetShaderiv(ShaderObject, GL_INFO_LOG_LENGTH, &blen);
		if (blen > 1)
		{
			GLchar* compiler_log = new GLchar[blen + 1];
			glGetShaderInfoLog(ShaderObject, blen, &slen, compiler_log);
			debugf(TEXT("XOpenGL: ErrorLog compiling %ls %ls"), ShaderName, appFromAnsi(compiler_log));
			delete[] compiler_log;
		}

		DumpShader(*ShOut, true);
		Result = false;
	}
	else 
	{
		glGetShaderiv(ShaderObject, GL_INFO_LOG_LENGTH, &blen);
		if (blen > 1)
		{
			GLchar* compiler_log = new GLchar[blen + 1];
			glGetShaderInfoLog(ShaderObject, blen, &slen, compiler_log);
			debugf(NAME_DevGraphics, TEXT("XOpenGL: Log compiling %ls %ls"), ShaderName, appFromAnsi(compiler_log));
			delete[] compiler_log;
		}
		else debugf(NAME_DevGraphics, TEXT("XOpenGL: No compiler messages for %ls %ls Shader"), ShaderName, ShaderTypeString(ShaderType));
	}

	//debugf(TEXT("XOpenGL: %ls %ls Shader Source"), ShaderName, ShaderTypeString(ShaderType));
	//DumpShader(*ShOut, false);

	return Result;
	unguard;
}

bool UXOpenGLRenderDevice::ShaderProgram::LinkShaderProgram()
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

	CHECK_GL_ERROR();

	Shaders[No_Prog]      = new NoProgram(TEXT("No"), this);
	Shaders[Simple_Prog]  = new DrawSimpleProgram(TEXT("DrawSimple"), this);
	Shaders[Tile_Prog]    = new DrawTileProgram(TEXT("DrawTile"), this);
	Shaders[Gouraud_Prog] = new DrawGouraudProgram(TEXT("DrawGouraud"), this);
	Shaders[Complex_Prog] = new DrawComplexProgram(TEXT("DrawComplex"), this);

	// (Re)initialize UBOs
	if (!GlobalMatricesBuffer.Buffer)
	{
		GlobalMatricesBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::MatricesIndex);
		GlobalMatricesBuffer.MapUBOBuffer(false, 1);
		GlobalMatricesBuffer.Advance(1);
	}
	else
	{
		GlobalMatricesBuffer.RebindBufferBase(GlobalShaderBindingIndices::MatricesIndex);
	}
	
	if (!StaticLightInfoBuffer.Buffer)
	{
		StaticLightInfoBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::StaticLightInfoIndex);
		StaticLightInfoBuffer.MapUBOBuffer(false, 1);
		StaticLightInfoBuffer.Advance(1);
	}
	else
	{
		StaticLightInfoBuffer.RebindBufferBase(GlobalShaderBindingIndices::StaticLightInfoIndex);
	}

	if (!GlobalCoordsBuffer.Buffer)
	{
		GlobalCoordsBuffer.GenerateUBOBuffer(this, GlobalShaderBindingIndices::CoordsIndex);
		GlobalCoordsBuffer.MapUBOBuffer(false, 1);
		GlobalCoordsBuffer.Advance(1);
	}
	else
	{
		GlobalCoordsBuffer.RebindBufferBase(GlobalShaderBindingIndices::CoordsIndex);
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
	
	for (const auto Shader : Shaders)
	{
		Shader->MapBuffers();
		Shader->BuildShaderProgram();
		Shader->ActivateShader();
		Shader->BindShaderState();			// We call this with the shader activated so we can bind uniform samplers immediately
		Shader->DeactivateShader();
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
	for (const auto Shader : Shaders)
	{
		Shader->DeleteShader();
		Shader->BuildShaderProgram();
		Shader->ActivateShader();
		Shader->BindShaderState();
		Shader->DeactivateShader();
	}
}

void UXOpenGLRenderDevice::ShaderProgram::BindShaderState()
{
	BindUniform(MatricesIndex, "GlobalMatrices");
	BindUniform(ClipPlaneIndex, "ClipPlaneParams");
	BindUniform(StaticLightInfoIndex, "StaticLightInfo");
	BindUniform(CoordsIndex, "GlobalCoords");
}

bool UXOpenGLRenderDevice::ShaderProgram::BuildShaderProgram(ShaderWriterFunc VertexShaderFunc, ShaderWriterFunc GeoShaderFunc, ShaderWriterFunc FragmentShaderFunc, ShaderWriterFunc EmitHeaderFunc)
{
	VertexShaderObject   = glCreateShader(GL_VERTEX_SHADER);
	if (GeoShaderFunc)
		GeoShaderObject  = glCreateShader(GL_GEOMETRY_SHADER);
	FragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);

	if (!CompileShader(GL_VERTEX_SHADER, VertexShaderObject, VertexShaderFunc, EmitHeaderFunc) ||
		(GeoShaderFunc && !CompileShader(GL_GEOMETRY_SHADER, GeoShaderObject, GeoShaderFunc, EmitHeaderFunc)) ||
		!CompileShader(GL_FRAGMENT_SHADER, FragmentShaderObject, FragmentShaderFunc, EmitHeaderFunc))
		return false;

	ShaderProgramObject = glCreateProgram();
	glAttachShader(ShaderProgramObject, VertexShaderObject);
	if (GeoShaderFunc)
		glAttachShader(ShaderProgramObject, GeoShaderObject);
	glAttachShader(ShaderProgramObject, FragmentShaderObject);

	if (!LinkShaderProgram())
		return false;

	return true;
}

void UXOpenGLRenderDevice::ShaderProgram::DeleteShader()
{
	if (!ShaderProgramObject)
		return;

	if (VertexShaderObject)
		glDetachShader(ShaderProgramObject, VertexShaderObject);

	if (GeoShaderObject)
		glDetachShader(ShaderProgramObject, GeoShaderObject);

	if (FragmentShaderObject)
		glDetachShader(ShaderProgramObject, FragmentShaderObject);

	glDeleteProgram(ShaderProgramObject);

	ShaderProgramObject = VertexShaderObject = GeoShaderObject = FragmentShaderObject = 0;
}

UXOpenGLRenderDevice::ShaderProgram::~ShaderProgram()
= default;

/*-----------------------------------------------------------------------------
	Dummy Shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::NoProgram::ActivateShader()
{
	/*
	if (RenDev->ArrayPoint)
	{
		RenDev->ArrayPoint->Unbind();
		RenDev->ArrayPoint = nullptr;
	}
	
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
	*/
}

void UXOpenGLRenderDevice::NoProgram::DeactivateShader()	{}
void UXOpenGLRenderDevice::NoProgram::BindShaderState()		{}
bool UXOpenGLRenderDevice::NoProgram::BuildShaderProgram()	{ return true; }
void UXOpenGLRenderDevice::NoProgram::CreateInputLayout()	{}
void UXOpenGLRenderDevice::NoProgram::Flush(bool Wait)		{}
void UXOpenGLRenderDevice::NoProgram::MapBuffers()			{}
void UXOpenGLRenderDevice::NoProgram::UnmapBuffers()		{}

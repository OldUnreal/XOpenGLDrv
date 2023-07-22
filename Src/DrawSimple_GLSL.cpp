/*=============================================================================
	DrawSimple_GLSL.cpp: UE1 Line and Simple Polygon Rendering Shaders

	Copyright 2014-2023 OldUnreal

	Revision history:
		* Created by Smirftsch
=============================================================================*/

#include <glm/glm.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	GLSL Code
------------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawSimpleProgram::EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	// DrawSimple isn't performance-critical so we don't use multi-drawing with SSBOs here
	Out << R"(
layout(std140) uniform DrawCallParameters
{
	vec4 DrawColor;
	float Gamma;
	uint HitTesting;
	uint LineFlags;
	uint DrawMode;
	uint BlendMode;
	uint Padding0;
	uint Padding1;
	uint Padding2;	
};
)";
}

void UXOpenGLRenderDevice::DrawSimpleProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	Out << "layout(location = 0) in vec3 Coords; // == gl_Vertex" END_LINE;

	if (GL->OpenGLVersion != GL_ES)
		Out << "out float gl_ClipDistance[" << GL->MaxClippingPlanes << "];" END_LINE;

	Out << R"(
void main(void)
{
  vec4 vEyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
)";

	if (GL->OpenGLVersion != GL_ES)
	{
		Out << R"(
  uint ClipIndex = uint(ClipParams.x);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Coords);
)";
	}

	Out << "}" END_LINE;
}

// stijn: I don't think we need this anymore + it breaks DrawSimple on macOS because
// the geoshader expects lines as input, but EndFlash and 2DPoint draw triangles

void UXOpenGLRenderDevice::DrawSimpleProgram::BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	Out << R"(
layout(lines) in;
layout(line_strip, max_vertices = 2) out;
noperspective out float texCoord;

void main()
{
  mat4 modelviewMat = modelMat * viewMat;
  vec2 winPos1 = vec2(512, 384) * gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
  vec2 winPos2 = vec2(512, 384) * gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

  // Line Start
  gl_Position = modelviewprojMat * (gl_in[0].gl_Position);
  texCoord = 0.0;
  EmitVertex();

  // Line End
  gl_Position = modelviewprojMat * (gl_in[1].gl_Position);
  texCoord = length(winPos2 - winPos1);
  EmitVertex();
  EndPrimitive();
}
)";
}

void UXOpenGLRenderDevice::DrawSimpleProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	if (GL->OpenGLVersion == GL_ES)
	{
		Out << "layout(location = 0) out vec4 FragColor;" END_LINE;
		if (GL->SimulateMultiPass)
			Out << "layout ( location = 1 ) out vec4 FragColor1;" END_LINE;
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out << "layout(location = 0, index = 1) out vec4 FragColor1;" END_LINE;
		Out << "layout(location = 0, index = 0) out vec4 FragColor;" END_LINE;
	}

	Out << R"(
void main(void)
{
  vec4 TotalColor = DrawColor;
)";

	// stijn: this is an attempt at stippled line drawing in GL4
#if 0
	Out << R"(
  if ((LineFlags & )" << LINE_Transparent << R"(u) == )" << LINE_Transparent << R"(u)
  {
    if (((uint(floor(gl_FragCoord.x)) & 1u) ^ (uint(floor(gl_FragCoord.y)) & 1u)) == 0u)
      discard;
  }
)";
#endif
	
	if (GL->SimulateMultiPass)
		Out << "  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;" END_LINE;
	Out << "  FragColor = TotalColor;" END_LINE;

	if (GIsEditor)
	{
		Out << "  if (!bool(HitTesting)) {" END_LINE;
	}

	Out << R"(
  FragColor = GammaCorrect(Gamma, FragColor);
)";

	if (GIsEditor)
		Out << "  }" END_LINE;

	Out << "}" END_LINE;
}

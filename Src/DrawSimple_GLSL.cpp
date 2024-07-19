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
	Globals
-----------------------------------------------------------------------------*/

const UXOpenGLRenderDevice::ShaderProgram::DrawCallParameterInfo UXOpenGLRenderDevice::DrawSimpleParametersInfo[]
=
{
	{"vec4", "DrawColor", 0},
	{ nullptr, nullptr, 0}
};

/*-----------------------------------------------------------------------------
	Shaders
-----------------------------------------------------------------------------*/

static const char* SimpleVertexShader = R"(
layout(location = 0) in vec3 Coords; // == gl_Vertex
layout(location = 1) in uint DrawID; // emulated gl_DrawID
#if OPT_SupportsClipDistance
out float gl_ClipDistance[OPT_MaxClippingPlanes];
#endif

void main(void)
{
  gl_Position = modelviewprojMat * vec4(Coords, 1.0);

#if OPT_SupportsClipDistance
  uint ClipIndex = uint(ClipParams.x);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Coords);
#endif

  vDrawID = DrawID;
}
)";

static const char* SimpleFragmentShader = R"(
#if OPT_GLES
layout(location = 0) out vec4 FragColor;
# if OPT_SimulateMultiPass
layout ( location = 1 ) out vec4 FragColor1;
# endif
#else
# if OPT_SimulateMultiPass
layout(location = 0, index = 1) out vec4 FragColor1;
# endif
layout(location = 0, index = 0) out vec4 FragColor;
#endif

void main(void)
{
  vec4 TotalColor = GetDrawColor(vDrawID);

  // stijn: this is an attempt at stippled line drawing in GL4
#if 0 && OPT_Transparent
  if (((uint(floor(gl_FragCoord.x)) & 1u) ^ (uint(floor(gl_FragCoord.y)) & 1u)) == 0u)
    discard;
#endif

#if OPT_SimulateMultiPass
  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;
#endif
FragColor = TotalColor;

#if OPT_Editor
  if (!bool(HitTesting)) {
#endif
    FragColor = GammaCorrect(Gamma, FragColor);
#if OPT_Editor
  }
#endif
}
)";

/*-----------------------------------------------------------------------------
	Simple Line Shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawSimpleLineProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << SimpleVertexShader;
}

void UXOpenGLRenderDevice::DrawSimpleLineProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << SimpleFragmentShader;
}

/*-----------------------------------------------------------------------------
	Simple Triangle Shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawSimpleTriangleProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << SimpleVertexShader;
}

void UXOpenGLRenderDevice::DrawSimpleTriangleProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << SimpleFragmentShader;
}
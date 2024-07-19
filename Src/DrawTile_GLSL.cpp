/*=============================================================================
	DrawTile_GLSL.cpp: UE1 Tile Rendering Shaders

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
const UXOpenGLRenderDevice::ShaderProgram::DrawCallParameterInfo UXOpenGLRenderDevice::DrawTileParametersInfo[]
=
{
	{"vec4", "DrawColor", 0},
	{"uvec4", "TexHandles", 0},
	{ nullptr, nullptr, 0}
};

static const char* InterfaceBlockData = R"(
  vec3 Coords;
  vec4 TexCoords;
  vec4 EyeSpacePos;

  // Core only
  vec4 TexCoords1;
  vec4 TexCoords2;
)";

/*-----------------------------------------------------------------------------
	OpenGL ES Tile Shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawTileESProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
out VertexData
{
)";
	Out << InterfaceBlockData;
	Out << R"(
} Out;

layout(location = 0) in vec3 Coords; // ==gl_Vertex
layout(location = 1) in uint DrawID; // emulated gl_DrawID
layout(location = 2) in vec2 TexCoords;

void main(void)
{
  Out.EyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  Out.TexCoords = vec4(TexCoords, 0.f, 0.f);
  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
  vDrawID = DrawID;
}
)";
}

void UXOpenGLRenderDevice::DrawTileESProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
layout(location = 0) out vec4 FragColor;
#if OPT_SimulateMultiPass
layout ( location = 1 ) out vec4 FragColor1;
#endif

in VertexData
{
)";
	Out << InterfaceBlockData;
	Out << R"(
} In;
void main(void)
{	
  vec4 TotalColor;
  vec4 Color = GetTexel(GetTexHandles(vDrawID).xy, Texture0, In.TexCoords.xy);

  TotalColor = ApplyPolyFlags(Color) * GetDrawColor(vDrawID);

#if !OPT_Modulated
  TotalColor = GammaCorrect(Gamma, TotalColor);
#endif

#if OPT_SimulateMultiPass
  FragColor = TotalColor;
  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;
#else
  FragColor = TotalColor;
#endif
}
)";
}

/*-----------------------------------------------------------------------------
	OpenGL Core Tile Shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawTileCoreProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
out VertexData
{
)";
	Out << InterfaceBlockData;
	Out << R"(
} Out;

layout(location = 0) in vec3 Coords; // ==gl_Vertex
layout(location = 1) in uint DrawID; // emulated gl_DrawID
layout(location = 2) in vec4 TexCoords0;
layout(location = 3) in vec4 TexCoords1;
layout(location = 4) in vec4 TexCoords2;

void main(void)
{
  Out.EyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  Out.Coords = Coords;
  Out.TexCoords = TexCoords0;
  Out.TexCoords1 = TexCoords1;
  Out.TexCoords2 = TexCoords2;
  gl_Position = vec4(Coords, 1.0);
  vDrawID = DrawID;
}
)";
}

void UXOpenGLRenderDevice::DrawTileCoreProgram::BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

in VertexData
{
)";
	Out << InterfaceBlockData;
	Out << R"(
} In[];

out GeometryData
{
)";
	Out << InterfaceBlockData;
	Out << R"(
} Out;

#if OPT_SupportsClipDistance
out float gl_ClipDistance[OPT_MaxClippingPlanes];
#endif

void main()
{
  uint ClipIndex = uint(ClipParams.x);

  float RFX2 = In[0].TexCoords.x;
  float RFY2 = In[0].TexCoords.y;
  float FX2 = In[0].TexCoords.z;
  float FY2 = In[0].TexCoords.w;

  float U = In[0].TexCoords1.x;
  float V = In[0].TexCoords1.y;
  float UL = In[0].TexCoords1.z;
  float VL = In[0].TexCoords1.w;

  float XL = In[0].TexCoords2.x;
  float YL = In[0].TexCoords2.y;
  float UMult = In[0].TexCoords2.z;
  float VMult = In[0].TexCoords2.w;

  float X = gl_in[0].gl_Position.x;
  float Y = gl_in[0].gl_Position.y;
  float Z = gl_in[0].gl_Position.z;

  vec3 Position;

  gDrawID = vDrawID[0];

  Out.TexCoords.zw = vec2(0.f, 0.f);

  // 0
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U)*UMult;
  Out.TexCoords.y = (V)*VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
#if OPT_SupportsClipDistance
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[0].Coords);
#endif
  EmitVertex();

  // 1
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U + UL) * UMult;
  Out.TexCoords.y = (V)*VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
#if OPT_SupportsClipDistance
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[1].Coords);
#endif
  EmitVertex();

  // 2
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U + UL) * UMult;
  Out.TexCoords.y = (V + VL) * VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
#if OPT_SupportsClipDistance
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[2].Coords);
#endif
  EmitVertex();
  EndPrimitive();

  // 0
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U)*UMult;
  Out.TexCoords.y = (V)*VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
#if OPT_SupportsClipDistance
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[0].Coords);
#endif
  EmitVertex();

  // 2
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U + UL) * UMult;
  Out.TexCoords.y = (V + VL) * VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
#if OPT_SupportsClipDistance
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[1].Coords);
#endif
  EmitVertex();

  // 3
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U)*UMult;
  Out.TexCoords.y = (V + VL) * VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
#if OPT_SupportsClipDistance
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[2].Coords);
#endif
  EmitVertex();  

  EndPrimitive();
}
)";
}

void UXOpenGLRenderDevice::DrawTileCoreProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
#if OPT_SimulateMultiPass
layout(location = 0, index = 1) out vec4 FragColor1;
#endif
layout(location = 0, index = 0) out vec4 FragColor;

in GeometryData
{
)";
	Out << InterfaceBlockData;
	Out << R"(
} In;

void main(void)
{
#if OPT_GeometryShaders
  uint DrawID = gDrawID;
#else
  uint DrawID = vDrawID;
#endif

  vec4 TotalColor;
  vec4 Color = GetTexel(GetTexHandles(DrawID).xy, Texture0, In.TexCoords.xy);

  TotalColor = ApplyPolyFlags(Color) * GetDrawColor(DrawID);

#if !OPT_Modulated
  TotalColor = GammaCorrect(Gamma, TotalColor);
#endif

#if OPT_Editor
# if OPT_Selected
  TotalColor.g = TotalColor.g - 0.04;
  TotalColor = clamp(TotalColor, 0.0, 1.0);
# endif

  if (bool(HitTesting))
    TotalColor = GetDrawColor(DrawID);
#endif

#if OPT_SimulateMultiPass
  FragColor = TotalColor;
  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;
#else
  FragColor = TotalColor;
#endif
}
)";
}
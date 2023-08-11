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
	GLSL Code
-----------------------------------------------------------------------------*/
const UXOpenGLRenderDevice::ShaderProgram::DrawCallParameterInfo Info[]
=
{
	{"vec4", "DrawColor", 0},
	{"vec4", "HitColor", 0},
	{"uint", "PolyFlags", 0},
	{"uint", "BlendPolyFlags", 0},
	{"uint", "HitTesting", 0},
	{"uint", "DepthTested", 0},
	{"uint", "Padding0", 0},
	{"uint", "Padding1", 0},
	{"uint", "Padding2", 0},
	{"float", "Gamma", 0},
	{"uvec4", "TexHandles", 0},
	{ nullptr, nullptr, 0}
};

void UXOpenGLRenderDevice::DrawTileProgram::EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	EmitDrawCallParametersHeader(ShaderType, GL, Info, Out, Program, GlobalShaderBindingIndices::TileParametersIndex, true);

	Out << "uniform sampler2D Texture0;" END_LINE;
}

static void EmitInterfaceBlockData(UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
  flat uvec2 TexHandle;
  flat uint PolyFlags;
  flat float Gamma;
  flat vec4 DrawColor;

  vec3 Coords;
  vec4 TexCoords;
  vec4 EyeSpacePos;

  // Core only
  vec4 TexCoords1;
  vec4 TexCoords2;

  // Editor only  
  flat vec4 HitColor;
  flat uint HitTesting;
)";
}

void UXOpenGLRenderDevice::DrawTileProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	Out << R"(
out VertexData
{
)";
	EmitInterfaceBlockData(GL, Out);
	Out << R"(
} Out;
)";

	if (!GL->UsingGeometryShaders)
	{
		Out << R"(
layout(location = 0) in vec3 Coords; // ==gl_Vertex
layout(location = 1) in vec2 TexCoords;

void main(void)
{
  Out.EyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  Out.TexCoords = vec4(TexCoords, 0.f, 0.f);
  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
)";
	}
	else
	{
		Out << R"(
layout(location = 0) in vec3 Coords; // ==gl_Vertex
layout(location = 1) in vec4 TexCoords0;
layout(location = 2) in vec4 TexCoords1;
layout(location = 3) in vec4 TexCoords2;

void main(void)
{
  Out.EyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  Out.Coords = Coords;
  Out.TexCoords = TexCoords0;
  Out.TexCoords1 = TexCoords1;
  Out.TexCoords2 = TexCoords2;
  gl_Position = vec4(Coords, 1.0);
)";
	}

	Out << R"(
  Out.TexHandle = GetTexHandles().xy;
  Out.PolyFlags = GetPolyFlags();
  Out.Gamma = GetGamma();
  Out.DrawColor = GetDrawColor();
)";

	if (GIsEditor)
	{
		Out << R"(  
  Out.HitColor = GetHitColor();
  Out.HitTesting = GetHitTesting();
)";
	}

	Out << "}" END_LINE;
}

void UXOpenGLRenderDevice::DrawTileProgram::BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	Out << R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

in VertexData
{
)";
	EmitInterfaceBlockData(GL, Out);
	Out << R"(
} In[];

out GeometryData
{
)";
	EmitInterfaceBlockData(GL, Out);
	Out << R"(
} Out;

out float gl_ClipDistance[)" << GL->MaxClippingPlanes << R"(];

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

  Out.PolyFlags = In[0].PolyFlags;
  Out.Gamma = In[0].Gamma;
  Out.TexHandle = In[0].TexHandle;
  Out.DrawColor = In[0].DrawColor;
  Out.TexCoords = In[0].TexCoords;
)";

	if (GIsEditor)
	{
		Out << R"(
  Out.HitTesting = In[0].HitTesting;
  Out.HitColor = In[0].HitColor;
)";
	}

	Out << R"(
  // 0
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U)*UMult;
  Out.TexCoords.y = (V)*VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[0].Coords);
  EmitVertex();

  // 1
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U + UL) * UMult;
  Out.TexCoords.y = (V)*VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[1].Coords);
  EmitVertex();

  // 2
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U + UL) * UMult;
  Out.TexCoords.y = (V + VL) * VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[2].Coords);
  EmitVertex();
  EndPrimitive();

  // 0
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U)*UMult;
  Out.TexCoords.y = (V)*VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[0].Coords);
  EmitVertex();

  // 2
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U + UL) * UMult;
  Out.TexCoords.y = (V + VL) * VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[1].Coords);
  EmitVertex();

  // 3
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  Out.TexCoords.x = (U)*UMult;
  Out.TexCoords.y = (V + VL) * VMult;
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[2].Coords);
  EmitVertex();

  EndPrimitive();
}
)";
}

void UXOpenGLRenderDevice::DrawTileProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	if (!GL->UsingGeometryShaders)
	{
		Out << "layout(location = 0) out vec4 FragColor;" END_LINE;
		if (GL->SimulateMultiPass)
			Out << "layout(location = 1) out vec4 FragColor1;" END_LINE;
		Out << "in VertexData" END_LINE;
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out << "layout(location = 0, index = 1) out vec4 FragColor1;" END_LINE;
		Out << "layout(location = 0, index = 0) out vec4 FragColor;" END_LINE;
		Out << "in GeometryData" END_LINE;
	}
	Out << "{" END_LINE;
	EmitInterfaceBlockData(GL, Out);
	Out << R"(
} In;

void main(void)
{	
  vec4 TotalColor;
  vec4 Color = GetTexel(In.TexHandle, Texture0, In.TexCoords.xy);

  // Handle PF_Masked.
  if ((In.PolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
  {
    if (Color.a < 0.5)
      discard;
    else Color.rgb /= Color.a;
  }
  else if (((In.PolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u) && Color.a < 0.01)
    discard;

  TotalColor = Color * In.DrawColor;

  if ((In.PolyFlags & )" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
    TotalColor = GammaCorrect(In.Gamma, TotalColor);
)";

	if (GIsEditor)
	{
		// Editor support.
		Out << R"(
  if ((In.PolyFlags & )" << PF_Selected << R"(u) == )" << PF_Selected << R"(u)
  {
    TotalColor.g = TotalColor.g - 0.04;
    TotalColor = clamp(TotalColor, 0.0, 1.0);
  }

  // HitSelection, Zoneview etc.
  if (bool(In.HitTesting))
    TotalColor = In.HitColor; // Use HitDrawColor.
)";
	}

	if (GL->SimulateMultiPass)
	{
		Out << R"(
  FragColor = TotalColor;
  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;
)";
	}
	else Out << "  FragColor = TotalColor;" END_LINE;
//	Out << "FragColor = vec4(1.0, 1.0, 1.0, 1.0);" END_LINE;	
	Out << "}" END_LINE;
}

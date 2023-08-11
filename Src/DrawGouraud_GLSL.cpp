/*=============================================================================
	DrawGouraud_GLSL.cpp: UE1 Mesh Rendering Shaders

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
    {"vec4", "DiffuseInfo", 0},
    {"vec4", "DetailMacroInfo", 0},
    {"vec4", "MiscInfo", 0},
    {"vec4", "DistanceFogColor", 0},
    {"vec4", "DistanceFogInfo", 0},
    {"vec4", "DrawColor", 0},
    {"uint", "DrawFlags", 0},
    {"uint", "PolyFlags", 0},
    {"uint", "TextureFormat", 0},
    {"uint", "HitTesting", 0},
    {"uint", "RendMap", 0},
    {"int", "DistanceFogMode", 0},
    {"uint", "Padding0", 0},
    {"uint", "Padding1", 0},
    {"uvec4", "TexHandles", 2},
    { nullptr, nullptr, 0}
};

void UXOpenGLRenderDevice::DrawGouraudProgram::EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
    EmitDrawCallParametersHeader(ShaderType, GL, Info, Out, Program, GlobalShaderBindingIndices::GouraudParametersIndex, true);

    Out << R"(
uniform sampler2D Texture0; // Base Texture
uniform sampler2D Texture1; // DetailTexture
uniform sampler2D Texture2; // BumpMap
uniform sampler2D Texture3; // MacroTex
)";
}

static void EmitInterfaceBlockData(UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
    Out << R"(
  flat uvec2 TexHandle;
  flat uvec2 DetailTexHandle;
  flat uvec2 BumpTexHandle;
  flat uvec2 MacroTexHandle;
  flat uint DrawFlags;
  flat uint TextureFormat;
  flat uint PolyFlags;
  flat float Gamma;
  flat vec4 TextureInfo; // diffuse, alpha, bumpmap specular
  flat vec4 DistanceFogColor;
  flat vec4 DistanceFogInfo;
  flat int DistanceFogMode;

  vec3 Coords;
  vec4 Normals;
  vec2 TexCoords;
  vec2 DetailTexCoords;
  vec2 MacroTexCoords;
  vec4 EyeSpacePos;
  vec4 LightColor;
  vec4 FogColor;

  // ES only
  mat3 TBNMat;
  vec3 TangentViewPos;
  vec3 TangentFragPos;

  // Editor only
  flat vec4 DrawColor;
  flat uint RendMap;
  flat uint HitTesting;
)";
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
    Out << R"(
layout(location = 0) in vec3 Coords; // == gl_Vertex
layout(location = 1) in vec3 Normals; // Normals
layout(location = 2) in vec2 TexCoords; // TexCoords
layout(location = 3) in vec4 LightColor;
layout(location = 4) in vec4 FogColor;

out VertexData
{
)";
    EmitInterfaceBlockData(GL, Out);
    Out << R"(
} Out;

void main(void)
{
  Out.TexHandle = GetTexHandles(0).xy;
  Out.DetailTexHandle = GetTexHandles(0).zw;
  Out.BumpTexHandle = GetTexHandles(1).xy;
  Out.MacroTexHandle = GetTexHandles(1).zw;
  Out.DrawFlags = GetDrawFlags();
  Out.TextureFormat = GetTextureFormat();
  Out.PolyFlags = GetPolyFlags();
  Out.Gamma = GetMiscInfo().y;
  Out.TextureInfo = vec4(GetDiffuseInfo().zw, GetMiscInfo().x, 0.f);
  Out.DistanceFogColor = GetDistanceFogColor();
  Out.DistanceFogInfo = GetDistanceFogInfo();
  Out.DistanceFogMode = GetDistanceFogMode();

  Out.Coords = Coords;
  Out.Normals = vec4(Normals.xyz, 0);
  Out.EyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  Out.TexCoords = TexCoords * GetDiffuseInfo().xy;
  Out.DetailTexCoords = TexCoords * GetDetailMacroInfo().xy;
  Out.MacroTexCoords = TexCoords * GetDetailMacroInfo().zw;
  Out.LightColor = LightColor * )" << (GL->ActorXBlending ? 1.5f : 1.f) << R"(;
  Out.FogColor = FogColor;
)";

    if (GIsEditor)
    {
        Out << R"(
  Out.HitTesting = GetHitTesting();
  Out.RendMap = GetRendMap();
  Out.DrawColor = GetDrawColor();
)";
    }

    if (!GL->UsingGeometryShaders)
    {
        Out << R"(
  vec3 T = vec3(1.0, 1.0, 1.0); // Arbitrary.
  vec3 B = vec3(1.0, 1.0, 1.0); // Replace with actual values extracted from mesh generation some day.
  vec3 N = normalize(Normals.xyz); // Normals.

  // TBN must have right handed coord system.
  //if (dot(cross(N, T), B) < 0.0)
  // T = T * -1.0;

  Out.TBNMat = transpose(mat3(T, B, N));
  Out.TangentViewPos = Out.TBNMat * normalize(FrameCoords[0].xyz);
  Out.TangentFragPos = Out.TBNMat * Coords.xyz;

  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
)";

        if (GL->SupportsClipDistance)
        {
            Out << R"(
  uint ClipIndex = uint(ClipParams.x);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Out.EyeSpacePos.xyz);
)";
        }
    }
    else
    {
        Out << "  gl_Position = vec4(Coords, 1.0);" END_LINE;
    }
    Out << "}" END_LINE;
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
    Out << R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

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

vec3 GetTangent(vec3 A, vec3 B, vec3 C, vec2 Auv, vec2 Buv, vec2 Cuv)
{
  float Bv_Cv = Buv.y - Cuv.y;
  if (Bv_Cv == 0.0)
    return (B - C) / (Buv.x - Cuv.x);

  float Quotient = (Auv.y - Cuv.y) / (Bv_Cv);
  vec3 D = C + (B - C) * Quotient;
  vec2 Duv = Cuv + (Buv - Cuv) * Quotient;
  return (D - A) / (Duv.x - Auv.x);
}

vec3 GetBitangent(vec3 A, vec3 B, vec3 C, vec2 Auv, vec2 Buv, vec2 Cuv)
{
  return GetTangent(A, C, B, Auv.yx, Cuv.yx, Buv.yx);
}

void main(void)
{
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);

  vec3 Tangent = GetTangent(In[0].Coords, In[1].Coords, In[2].Coords, In[0].TexCoords, In[1].TexCoords, In[2].TexCoords);
  vec3 Bitangent = GetBitangent(In[0].Coords, In[1].Coords, In[2].Coords, In[0].TexCoords, In[1].TexCoords, In[2].TexCoords);
  uint ClipIndex = uint(ClipParams.x);

  for (int i = 0; i < 3; ++i)
  {
    vec3 T = normalize(vec3(modelMat * vec4(Tangent, 0.0)));
    vec3 B = normalize(vec3(modelMat * vec4(Bitangent, 0.0)));
    vec3 N = normalize(vec3(modelMat * In[i].Normals));

    // TBN must have right handed coord system.
    // if (dot(cross(N, T), B) < 0.0)
    // T = T * -1.0;
    Out.TBNMat = mat3(T, B, N);

    Out.EyeSpacePos = In[i].EyeSpacePos;
    Out.LightColor = In[i].LightColor;
    Out.FogColor = In[i].FogColor;
    Out.Normals = In[i].Normals;
    Out.TexCoords = In[i].TexCoords;
    Out.DetailTexCoords = In[i].DetailTexCoords;
    Out.MacroTexCoords = In[i].MacroTexCoords;
    Out.Coords = In[i].Coords;
    Out.TexHandle = In[i].TexHandle;
    Out.DetailTexHandle = In[i].DetailTexHandle;
    Out.BumpTexHandle = In[i].BumpTexHandle;
    Out.MacroTexHandle = In[i].MacroTexHandle;
    Out.TextureInfo = In[i].TextureInfo;
    Out.DrawFlags = In[i].DrawFlags;
    Out.PolyFlags = In[i].PolyFlags;
    Out.Gamma = In[i].Gamma;
    Out.TextureFormat = In[i].TextureFormat;
    Out.DistanceFogColor = In[i].DistanceFogColor;
    Out.DistanceFogInfo = In[i].DistanceFogInfo;
    Out.DistanceFogMode = In[i].DistanceFogMode;

    Out.TangentViewPos = Out.TBNMat * normalize(FrameCoords[0].xyz);
    Out.TangentFragPos = Out.TBNMat * In[i].Coords.xyz;
)";

    if (GIsEditor)
    {
        Out << R"(
    Out.DrawColor = In[i].DrawColor;
    Out.RendMap = In[i].RendMap;
    Out.HitTesting = In[i].HitTesting;
)";
    }

    Out << R"(
    gl_Position = modelviewprojMat * gl_in[i].gl_Position;
    gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[i].Coords);
    EmitVertex();
  }
}
)";
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
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

    Out << R"(
void main(void)
{
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);
  vec4 TotalColor = vec4(0.0, 0.0, 0.0, 0.0);

  int NumLights = int(LightData4[0].y);
  vec4 Color = GetTexel(In.TexHandle, Texture0, In.TexCoords);

  if (In.TextureInfo.x > 0.0)
    Color *= In.TextureInfo.x; // Diffuse factor.
  if (In.TextureInfo.y > 0.0)
    Color.a *= In.TextureInfo.y; // Alpha.
  if (In.TextureFormat == )" << TEXF_BC5 << R"(u) // BC5 (GL_COMPRESSED_RG_RGTC2) compression
    Color.b = sqrt(1.0 - Color.r * Color.r + Color.g * Color.g);

  if ((In.PolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u)
  {
    Color.a *= In.LightColor.a; // Alpha.
    if (Color.a < 0.01)
      discard;
  }
  // Handle PF_Masked.
  else if ((In.PolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
  {
    if (Color.a < 0.5)
      discard;
    else Color.rgb /= Color.a;
  }

  vec4 LightColor;
)";

    if (GL->UseHWLighting)
    {
        constexpr FLOAT MinLight = 0.025f;
        Out << R"(
  float LightAdd = 0.0f;
  vec4 TotalAdd = vec4(0.0, 0.0, 0.0, 0.0);

  for (int i = 0; i < NumLights; i++)
  {
    float WorldLightRadius = LightData4[i].x;
    float LightRadius = LightData2[i].w;
    float RWorldLightRadius = WorldLightRadius * WorldLightRadius;

    vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
    float dist = distance(In.Coords, InLightPos);

    if (dist < RWorldLightRadius)
    {
      // Light color
      vec3 RGBColor = vec3(LightData1[i].x, LightData1[i].y, LightData1[i].z);
      float b = WorldLightRadius / (RWorldLightRadius * )" << MinLight << R"();
      float attenuation = WorldLightRadius / (dist + b * dist * dist);
      //float attenuation = 0.82*(1.0-smoothstep(LightRadius,24.0*LightRadius+0.50,dist));
      TotalAdd += (vec4(RGBColor, 1.0) * attenuation);
    }
  }
  LightColor = TotalAdd;
)";
    }
    else Out << "  LightColor = In.LightColor;" END_LINE;

    Out << R"(
  // Handle PF_RenderFog.
  if ((In.PolyFlags & )" << PF_RenderFog << R"(u) == )" << PF_RenderFog << R"(u)
  {
    // Handle PF_RenderFog|PF_Modulated.
    if ((In.PolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u)
    {
      // Compute delta to modulation identity.
      vec3 Delta = vec3(0.5) - Color.xyz;
      // Reduce delta by (squared) fog intensity.
      //Delta *= 1.0 - sqrt(In.FogColor.a);
      Delta *= 1.0 - In.FogColor.a;
      Delta *= vec3(1.0) - In.FogColor.rgb;

      TotalColor = vec4(vec3(0.5) - Delta, Color.a);
    }
    else // Normal.
    {
      Color *= LightColor;
      //TotalColor=mix(Color, vec4(In.FogColor.xyz,1.0), In.FogColor.w);
      TotalColor.rgb = Color.rgb * (1.0 - In.FogColor.a) + In.FogColor.rgb;
      TotalColor.a = Color.a;
	}
  }
  else if ((In.PolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u) // No Fog.
  {
    TotalColor = Color;
  }
  else
  {
    TotalColor = Color * vec4(LightColor.rgb, 1.0);
  }
)";

    if (GL->DetailTextures)
    {
        Out << R"(
  if ((In.DrawFlags & )" << DF_DetailTexture << R"(u) == )" << DF_DetailTexture << R"(u)
  {
    float NearZ = In.Coords.z / 512.0;
    float DetailScale = 1.0;
    float bNear;
    vec4 DetailTexColor;
    vec3 hsvDetailTex;

    for (int i = 0; i < )" << GL->DetailMax << R"(; ++i)
    {
      if (i > 0)
      {
        NearZ *= 4.223f;
        DetailScale *= 4.223f;
      }
      bNear = clamp(0.65 - NearZ, 0.0, 1.0);
      if (bNear > 0.0)
      {
        DetailTexColor = GetTexel(In.DetailTexHandle, Texture1, In.DetailTexCoords * DetailScale);

		vec3 hsvDetailTex = rgb2hsv(DetailTexColor.rgb); // cool idea Han :)
        hsvDetailTex.b += (DetailTexColor.r - 0.1);
        hsvDetailTex = hsv2rgb(hsvDetailTex);
        DetailTexColor = vec4(hsvDetailTex, 0.0);
        DetailTexColor = mix(vec4(1.0, 1.0, 1.0, 1.0), DetailTexColor, bNear); //fading out.
        TotalColor.rgb *= DetailTexColor.rgb;
      }
    }
  }     
)";
    }

    if (GL->MacroTextures)
    {
        Out << R"(
  if ((In.DrawFlags & )" << DF_MacroTexture << R"(u) == )" << DF_MacroTexture << R"(u && (In.DrawFlags & )" << DF_BumpMap << R"(u) != )" << DF_BumpMap << R"(u)
  {
    vec4 MacroTexColor = GetTexel(In.MacroTexHandle, Texture3, In.MacroTexCoords);
    vec3 hsvMacroTex = rgb2hsv(MacroTexColor.rgb);
    hsvMacroTex.b += (MacroTexColor.r - 0.1);
    hsvMacroTex = hsv2rgb(hsvMacroTex);
    MacroTexColor = vec4(hsvMacroTex, 1.0);
    TotalColor *= MacroTexColor;
  }
)";
    }

    if (GL->BumpMaps)
    {
        constexpr FLOAT MinLight = 0.05f;
#if ENGINE_VERSION == 227
        FString Sunlight = FString::Printf(TEXT("bool bSunlight = bool(uint(LightData2[i].x == %du));"), LE_Sunlight);
#else
        FString Sunlight = TEXT("bool bSunlight = false;");
#endif

        Out << R"(
  if ((In.DrawFlags & )" << DF_BumpMap << R"(u) == )" << DF_BumpMap << R"(u)
  {
    vec3 TangentViewDir = normalize(In.TangentViewPos - In.TangentFragPos);
    //normal from normal map
    vec3 TextureNormal = GetTexel(In.BumpTexHandle, Texture2, In.TexCoords).rgb * 2.0 - 1.0;
    vec3 BumpColor;
    vec3 TotalBumpColor = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < NumLights; ++i)
    {
      vec3 CurrentLightColor = vec3(LightData1[i].x, LightData1[i].y, LightData1[i].z);

      float NormalLightRadius = LightData5[i].x;
      bool bZoneNormalLight = bool(LightData5[i].y);
      float LightBrightness = LightData5[i].z / 255.0;
      if (NormalLightRadius == 0.0)
        NormalLightRadius = LightData2[i].w * 64.0;

      )" << appToAnsi(*Sunlight) << R"(
			
      vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
      float dist = distance(In.Coords, InLightPos);
      float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * )" << MinLight << R"();
      float attenuation = NormalLightRadius / (dist + b * dist * dist);
      
      if ((In.PolyFlags & )" << PF_Unlit << R"(u) == )" << PF_Unlit << R"(u)
        InLightPos = vec3(1.0, 1.0, 1.0); // no idea whats best here. Arbitrary value based on some tests.

      if ((NormalLightRadius == 0.0 || (dist > NormalLightRadius) || (bZoneNormalLight && (LightData4[i].z != LightData4[i].w))) && !bSunlight) // Do not consider if not in range or in a different zone.
        continue;

      vec3 TangentLightPos = In.TBNMat * InLightPos;
      vec3 TangentlightDir = normalize(TangentLightPos - In.TangentFragPos);
      // ambient
      vec3 ambient = 0.1 * TotalColor.xyz;
      // diffuse
      float diff = max(dot(TangentlightDir, TextureNormal), 0.0);
      vec3 diffuse = diff * TotalColor.xyz;
      // specular
      vec3 halfwayDir = normalize(TangentlightDir + TangentViewDir);
      float spec = pow(max(dot(TextureNormal, halfwayDir), 0.0), 8.0);
      vec3 specular = vec3(0.01) * spec * CurrentLightColor * LightBrightness;
      TotalBumpColor += (ambient + diffuse + specular) * attenuation;
    }
    TotalColor += vec4(clamp(TotalBumpColor, 0.0, 1.0), 1.0);
  }
)";
    }

#if ENGINE_VERSION==227
    // Add DistanceFog
    Out << R"(
  if (In.DistanceFogMode >= 0)
  {
    FogParameters DistanceFogParams;
    DistanceFogParams.FogStart = In.DistanceFogInfo.x;
    DistanceFogParams.FogEnd = In.DistanceFogInfo.y;
    DistanceFogParams.FogDensity = In.DistanceFogInfo.z;
    DistanceFogParams.FogMode = In.DistanceFogMode;

    if ((In.PolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u)
      DistanceFogParams.FogColor = vec4(0.5, 0.5, 0.5, 0.0);
	else if ((In.PolyFlags & )" << PF_Translucent << R"(u) == )" << PF_Translucent << R"(u && (In.PolyFlags & )" << PF_Environment << R"(u) != )" << PF_Environment << R"(u)
      DistanceFogParams.FogColor = vec4(0.0, 0.0, 0.0, 0.0);
    else DistanceFogParams.FogColor = In.DistanceFogColor;

    DistanceFogParams.FogCoord = abs(In.EyeSpacePos.z / In.EyeSpacePos.w);
    TotalColor = mix(TotalColor, DistanceFogParams.FogColor, getFogFactor(DistanceFogParams));
  }
)";
#endif

    if (GIsEditor)
    {
        // Editor support.
        Out << R"(
  if (In.RendMap == )" << REN_Zones << R"(u || In.RendMap == )" << REN_PolyCuts << R"(u || In.RendMap == )" << REN_Polys << R"(u)
  {
    TotalColor += 0.5;
    TotalColor *= In.DrawColor;
  }
)";
#if ENGINE_VERSION==227
        Out << R"(
  else if (In.RendMap == )" << REN_Normals << R"(u)
  {
    // Dot.
    float T = 0.5 * dot(normalize(In.Coords), In.Normals.xyz);
    // Selected.
    if ((In.PolyFlags & )" << PF_Selected << R"(u) == )" << PF_Selected << R"(u)
    {
      TotalColor = vec4(0.0, 0.0, abs(T), 1.0);
    }
    // Normal.
    else
    {
      TotalColor = vec4(max(0.0, T), max(0.0, -T), 0.0, 1.0);
    }
  }
)";
#endif
        Out << R"(
  else if (In.RendMap == )" << REN_PlainTex << R"(u)
  {
    TotalColor = Color;
  }
)";

#if ENGINE_VERSION==227
        Out << "  if ((In.RendMap != " << REN_Normals << "u) && ((In.PolyFlags & " << PF_Selected << "u) == " << PF_Selected << "u))" END_LINE;
#else
        Out << "  if ((In.PolyFlags & " << PF_Selected << "u) == " << PF_Selected << "u)" END_LINE;
#endif
        Out << R"(
  {
    TotalColor.r = (TotalColor.r * 0.75);
    TotalColor.g = (TotalColor.g * 0.75);
    TotalColor.b = (TotalColor.b * 0.75) + 0.1;
    TotalColor = clamp(TotalColor, 0.0, 1.0);
    if (TotalColor.a < 0.5)
      TotalColor.a = 0.51;
  }

  if ((In.PolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u && In.DrawColor.a > 0.0)
    TotalColor.a *= In.DrawColor.a;

  // HitSelection, Zoneview etc.
  if (bool(In.HitTesting))
    TotalColor = In.DrawColor; // Use DrawColor.
  else if ((In.PolyFlags&)" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
    TotalColor = GammaCorrect(In.Gamma, TotalColor);
)";
    }
    if (GL->SimulateMultiPass)
    {
        Out << R"(
  FragColor = TotalColor;
  FragColor1 = ((vec4(1.0) - TotalColor * LightColor)); //no, this is not entirely right, TotalColor has already LightColor applied. But will blow any fog/transparency otherwise. However should not make any (visual) difference here for this equation. Any better idea?
)";
    }
    else Out << "  FragColor = TotalColor;" END_LINE;

    if (!GIsEditor)
    {
        Out << R"(
  if ((In.PolyFlags&)" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
    FragColor = GammaCorrect(In.Gamma, FragColor);
 )";
    }

    Out << "}" END_LINE;

    // Blending translation table
    /*
    //PF_Modulated
    //glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
    //pixel_color * gl_FragColor

    GL_ONE 						vec4(1.0)
    GL_ZERO 					vec4(0.0)
    GL_SRC_COLOR 				gl_FragColor
    GL_SRC_ALPHA 				vec4(gl_FragColor.a)
    GL_DST_COLOR 				pixel_color
    GL_DST_ALPHA 				vec4(pixel_color.a)
    GL_ONE_MINUS_SRC_COLOR		vec4(1.0) - gl_FragColor
    GL_ONE_MINUS_SRC_ALPHA 		vec4(1.0  - gl_FragColor.a)
    GL_ONE_MINUS_DST_COLOR 		vec4(1.0) - pixel_color
    GL_ONE_MINUS_DST_ALPHA 		vec4(1.0  - pixel_color.a)

    if (gPolyFlags & PF_Invisible)
        glBlendFunc(GL_ZERO, GL_ONE);

    if (gPolyFlags & PF_Translucent)
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);

    if (gPolyFlags & PF_Modulated)
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

    if (gPolyFlags & PF_AlphaBlend)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (gPolyFlags & PF_Highlighted)
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    */
}

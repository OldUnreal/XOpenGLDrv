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

const UXOpenGLRenderDevice::ShaderProgram::DrawCallParameterInfo UXOpenGLRenderDevice::DrawGouraudParametersInfo[]
=
{
    {"vec4", "DiffuseInfo", 0},
    {"vec4", "DetailMacroInfo", 0},
    {"vec4", "MiscInfo", 0},
    {"vec4", "DrawColor", 0},
    {"uvec4", "TexHandles", 4},
    {"uint", "DrawFlags", 0},
    {"uint", "Dummy0", 0},
    {"uint", "Dummy1", 0},
    {"uint", "Dummy2", 0},
    { nullptr, nullptr, 0}
};

static const char* InterfaceBlockData = R"(  
  vec2 TexCoords;
  vec3 Coords;
  vec4 LightColor;
  vec4 FogColor;
  vec4 Normals;  

#if OPT_DetailTextures
  vec2 DetailTexCoords;
#endif

#if OPT_MacroTextures
  vec2 MacroTexCoords;
#endif

#if OPT_BumpMaps
  mat3 TBNMat;
  vec3 TangentViewPos;
  vec3 TangentFragPos;
#endif
  
#if OPT_DistanceFog || OPT_ClipDistance  
  vec4 EyeSpacePos;
#endif
)";

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
    Out << R"(
layout(location = 0) in vec3 Coords; // == gl_Vertex
layout(location = 1) in uint DrawID; // emulated gl_DrawID
layout(location = 2) in vec4 Normals; // Normals
layout(location = 3) in vec2 TexCoords; // TexCoords
layout(location = 4) in vec4 LightColor;
layout(location = 5) in vec4 FogColor;

out VertexData
{
)";
    Out << InterfaceBlockData;
    Out << R"(
} Out;

#if OPT_ClipDistance && !OPT_GeometryShaders
out float gl_ClipDistance[OPT_MaxClippingPlanes];
#endif

void main(void)
{
  uint DrawFlags = GetDrawFlags(DrawID);
  Out.TexCoords = TexCoords * GetDiffuseInfo(DrawID).xy;
  Out.Coords = Coords;
  Out.LightColor = LightColor * LightColorIntensity;
  Out.FogColor = FogColor;
  Out.Normals = vec4(Normals.xyz, 0);

#if OPT_DetailTextures
  if ((DrawFlags & DF_DetailTexture) == DF_DetailTexture)
    Out.DetailTexCoords = TexCoords * GetDetailMacroInfo(DrawID).xy;
#endif

#if OPT_MacroTextures
  if ((DrawFlags & DF_MacroTexture) == DF_MacroTexture)
    Out.MacroTexCoords = TexCoords * GetDetailMacroInfo(DrawID).zw;
#endif

#if OPT_DistanceFog || OPT_ClipDistance
  Out.EyeSpacePos = modelviewMat * vec4(Coords, 1.0);
#endif
  
#if OPT_GeometryShaders
  gl_Position = vec4(Coords, 1.0);
#else 
# if OPT_BumpMaps
  if ((DrawFlags & DF_BumpMap) == DF_BumpMap)
  {
    vec3 T = vec3(1.0, 1.0, 1.0); // Arbitrary.
    vec3 B = vec3(1.0, 1.0, 1.0); // Replace with actual values extracted from mesh generation some day.
    vec3 N = normalize(Normals.xyz); // Normals.

    // TBN must have right handed coord system.
    //if (dot(cross(N, T), B) < 0.0)
    // T = T * -1.0;

    Out.TBNMat = transpose(mat3(T, B, N));
    Out.TangentViewPos = Out.TBNMat * normalize(FrameCoords[0].xyz);
    Out.TangentFragPos = Out.TBNMat * Coords.xyz;
  }
# endif

  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
#endif

  vDrawID = DrawID;

#if OPT_ClipDistance && !OPT_GeometryShaders
  uint ClipIndex = uint(ClipParams.x);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Out.EyeSpacePos.xyz);
#endif
}
)";
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildGeometryShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
    Out << R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

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

#if OPT_ClipDistance
out float gl_ClipDistance[OPT_MaxClippingPlanes];
#endif

#if OPT_BumpMaps
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
#endif

void main(void)
{
  uint DrawFlags = GetDrawFlags(vDrawID[0]);
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);

#if OPT_BumpMaps
  vec3 Tangent;
  vec3 Bitangent;
  vec3 T;
  vec3 B;
  if ((DrawFlags & DF_BumpMap) == DF_BumpMap)
  {
    Tangent = GetTangent(In[0].Coords, In[1].Coords, In[2].Coords, In[0].TexCoords, In[1].TexCoords, In[2].TexCoords);
    Bitangent = GetBitangent(In[0].Coords, In[1].Coords, In[2].Coords, In[0].TexCoords, In[1].TexCoords, In[2].TexCoords);
    T = normalize(vec3(modelMat * vec4(Tangent, 0.0)));
    B = normalize(vec3(modelMat * vec4(Bitangent, 0.0)));
  }
#endif
	
#if OPT_ClipDistance
  uint ClipIndex = uint(ClipParams.x);
#endif

  gDrawID = vDrawID[0];

  for (int i = 0; i < 3; ++i)
  {
#if OPT_BumpMaps
    if ((DrawFlags & DF_BumpMap) == DF_BumpMap)
    {
      vec3 N = normalize(vec3(modelMat * In[i].Normals));

      // TBN must have right handed coord system.
      // if (dot(cross(N, T), B) < 0.0)
      // T = T * -1.0;
      Out.TBNMat = mat3(T, B, N);
      Out.TangentViewPos = Out.TBNMat * normalize(FrameCoords[0].xyz);
      Out.TangentFragPos = Out.TBNMat * In[i].Coords.xyz;
    }
#endif
#if OPT_DistanceFog || OPT_ClipDistance
    Out.EyeSpacePos = In[i].EyeSpacePos;
#endif
    Out.TexCoords = In[i].TexCoords;
    Out.Coords = In[i].Coords;
    Out.LightColor = In[i].LightColor;
    Out.FogColor = In[i].FogColor;
    Out.Normals = In[i].Normals;
    
#if OPT_DetailTextures
    Out.DetailTexCoords = In[i].DetailTexCoords;
#endif

#if OPT_MacroTextures
    Out.MacroTexCoords = In[i].MacroTexCoords;
#endif

    gl_Position = modelviewprojMat * gl_in[i].gl_Position;
#if OPT_ClipDistance
    gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, In[i].Coords);
#endif
    EmitVertex();
  }  
}
)";
}

void UXOpenGLRenderDevice::DrawGouraudProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
    Out << R"(
#if OPT_GLES
layout(location = 0) out vec4 FragColor;
# if OPT_SimulateMultiPass
layout(location = 1) out vec4 FragColor1;
# endif
#else
# if OPT_SimulateMultiPass
layout(location = 0, index = 1) out vec4 FragColor1;
# endif
layout(location = 0, index = 0) out vec4 FragColor;
#endif

#if OPT_GeometryShaders
in GeometryData
#else
in VertexData
#endif
{
)";
    Out << InterfaceBlockData;
    Out << R"(
} In;
)";

    Out << R"(
uvec2 GetTexHandleHelper(uint DrawID, uint Index)
{
	uvec4 Handles = GetTexHandles(DrawID, Index / 2u);
	return (Index % 2u == 0u) ? Handles.xy : Handles.zw;
}

void main(void)
{
#if OPT_GeometryShaders
  uint DrawID = gDrawID;
#else
  uint DrawID = vDrawID;
#endif

  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);
  vec4 TotalColor = vec4(0.0, 0.0, 0.0, 0.0);
  uint DrawFlags = GetDrawFlags(DrawID);

#if OPT_BumpMaps || OPT_HWLighting
  int NumLights = int(LightData4[0].y);
#endif

  vec4 Color = GetTexel(GetTexHandleHelper(DrawID, DiffuseTextureIndex), TMUDiffuse, In.TexCoords);
  Color *= GetDiffuseInfo(DrawID).z; // Diffuse factor.
  Color.a *= GetDiffuseInfo(DrawID).w; // Alpha.

  if ((DrawFlags & DF_AlphaBlended) == DF_AlphaBlended)
    Color.a *= In.LightColor.a;

  Color = ApplyPolyFlags(Color, DrawFlags);
  vec4 LightColor;

#if OPT_HWLighting
  float LightAdd = 0.0f;
  vec4 TotalAdd = vec4(0.0, 0.0, 0.0, 0.0);
  float MinHWLight = 0.025f;

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
      float b = WorldLightRadius / (RWorldLightRadius * MinHWLight);
      float attenuation = WorldLightRadius / (dist + b * dist * dist);
      //float attenuation = 0.82*(1.0-smoothstep(LightRadius,24.0*LightRadius+0.50,dist));
      TotalAdd += (vec4(RGBColor, 1.0) * attenuation);
    }
  }
  LightColor = TotalAdd;
#else
  LightColor = In.LightColor;
#endif

  if ((DrawFlags & DF_RenderFog) == DF_RenderFog)
  {
    if ((DrawFlags & DF_Modulated) == DF_Modulated)
    {
      // Compute delta to modulation identity.
      vec3 Delta = vec3(0.5) - Color.xyz;
      // Reduce delta by (squared) fog intensity.
      //Delta *= 1.0 - sqrt(In.FogColor.a);
      Delta *= 1.0 - In.FogColor.a;
      Delta *= vec3(1.0) - In.FogColor.rgb;
      TotalColor = vec4(vec3(0.5) - Delta, Color.a);
    }
    else
    {
      Color *= LightColor;
      //TotalColor=mix(Color, vec4(In.FogColor.xyz,1.0), In.FogColor.w);
      TotalColor.rgb = Color.rgb * (1.0 - In.FogColor.a) + In.FogColor.rgb;
      TotalColor.a = Color.a;
    }
  }
  else if ((DrawFlags & DF_Modulated) == DF_Modulated)
  {
    TotalColor = Color;
  }
  else
  {
    TotalColor = Color * vec4(LightColor.rgb, 1.0);
  }

#if OPT_DetailTextures
  if ((DrawFlags & DF_DetailTexture) == DF_DetailTexture)
  {
    float NearZ = In.Coords.z / 512.0;
    float DetailScale = 1.0;
    float bNear;
    vec4 DetailTexColor;
    vec3 hsvDetailTex;

    for (int i = 0; i < OPT_DetailMax; ++i)
    {
      if (i > 0)
      {
        NearZ *= 4.223f;
        DetailScale *= 4.223f;
      }
      bNear = clamp(0.65 - NearZ, 0.0, 1.0);
      if (bNear > 0.0)
      {
        DetailTexColor = GetTexel(GetTexHandleHelper(DrawID, DetailTextureIndex), TMUDetail, In.DetailTexCoords * DetailScale);

		vec3 hsvDetailTex = rgb2hsv(DetailTexColor.rgb); // cool idea Han :)
        hsvDetailTex.b += (DetailTexColor.r - 0.1);
        hsvDetailTex = hsv2rgb(hsvDetailTex);
        DetailTexColor = vec4(hsvDetailTex, 0.0);
        DetailTexColor = mix(vec4(1.0, 1.0, 1.0, 1.0), DetailTexColor, bNear); //fading out.
        TotalColor.rgb *= DetailTexColor.rgb;
      }
    }
  }     
#endif

#if OPT_MacroTextures && !OPT_BumpMaps
  if ((DrawFlags & DF_MacroTexture) == DF_MacroTexture)
  {
    vec4 MacroTexColor = GetTexel(GetTexHandleHelper(DrawID, MacroTextureIndex), TMUMacro, In.MacroTexCoords);
    vec3 hsvMacroTex = rgb2hsv(MacroTexColor.rgb);
    hsvMacroTex.b += (MacroTexColor.r - 0.1);
    hsvMacroTex = hsv2rgb(hsvMacroTex);
    MacroTexColor = vec4(hsvMacroTex, 1.0);
    TotalColor *= MacroTexColor;
  }
#endif

#if OPT_BumpMaps
  if ((DrawFlags & DF_BumpMap) == DF_BumpMap)
  {       
    float MinLight = 0.05f;
    vec3 TangentViewDir = normalize(In.TangentViewPos - In.TangentFragPos);
    //normal from normal map
    vec3 TextureNormal = GetTexel(GetTexHandleHelper(DrawID, BumpMapIndex), TMUBumpMap, In.TexCoords).rgb * 2.0 - 1.0;
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

      bool bSunlight = bool(uint(LightData2[i].x) == LE_Sunlight);
      vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
      float dist = distance(In.Coords, InLightPos);
      float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * MinLight);
      float attenuation = NormalLightRadius / (dist + b * dist * dist);

      if ((DrawFlags & DF_Unlit) == DF_Unlit)
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
#endif

#if OPT_DistanceFog
  // Add DistanceFog, needs to be added after Light has been applied. 
  if (DistanceFogMode >= 0)
  {
    vec4 MixColor;
    if ((DrawFlags & DF_Modulated) == DF_Modulated)
      MixColor = vec4(0.5, 0.5, 0.5, 0.0);
    else if ((DrawFlags & DF_Translucent) == DF_Translucent && (DrawFlags & DF_EnvironmentMap) != DF_EnvironmentMap)
      MixColor = vec4(0.0, 0.0, 0.0, 0.0);
    else
      MixColor = DistanceFogColor;

    float FogCoord = abs(In.EyeSpacePos.z / In.EyeSpacePos.w);
    TotalColor = mix(TotalColor, MixColor, getFogFactor(FogCoord));
  }
#endif

#if OPT_Editor
  vec4 DrawColor = GetDrawColor(DrawID);
  if (RendMap == REN_Zones || RendMap == REN_PolyCuts || RendMap == REN_Polys || RendMap == REN_WalkableSurfs)
  {
    TotalColor += 0.5;
    TotalColor *= DrawColor;
  }
  else if (RendMap == REN_Normals)
  {
    // Dot.
    float T = 0.5 * dot(normalize(In.Coords), In.Normals.xyz);
    // Selected.
    if ((DrawFlags & DF_Selected) == DF_Selected)
      TotalColor = vec4(0.0, 0.0, abs(T), 1.0);
    else
      TotalColor = vec4(max(0.0, T), max(0.0, -T), 0.0, 1.0);
  }
  else if (RendMap == REN_PlainTex)
  {
    TotalColor = Color;
  }

  if ((DrawFlags & DF_Selected) == DF_Selected && RendMap != REN_Normals)
  {
    TotalColor.r = (TotalColor.r * 0.75);
    TotalColor.g = (TotalColor.g * 0.75);
    TotalColor.b = (TotalColor.b * 0.75) + 0.1;
    TotalColor = clamp(TotalColor, 0.0, 1.0);
    if (TotalColor.a < 0.5)
      TotalColor.a = 0.51;
  }

  if ((DrawFlags & DF_AlphaBlended) == DF_AlphaBlended && DrawColor.a > 0.0)
    TotalColor.a *= DrawColor.a;

  // HitSelection, Zoneview etc.
  if (bool(HitTesting))
    TotalColor = DrawColor; // Use DrawColor.
  else if ((DrawFlags & DF_Modulated) != DF_Modulated)
    TotalColor = GammaCorrect(Gamma, TotalColor);

#endif // OPT_Editor

#if OPT_SimulateMultiPass
  FragColor = TotalColor;
  FragColor1 = ((vec4(1.0) - TotalColor * LightColor)); //no, this is not entirely right, TotalColor has already LightColor applied. But will blow any fog/transparency otherwise. However should not make any (visual) difference here for this equation. Any better idea?
#else
  FragColor = TotalColor;
#endif

#if !OPT_Editor
  if ((DrawFlags & DF_Modulated) != DF_Modulated)
    FragColor = GammaCorrect(Gamma, FragColor);
#endif
}
)";
}

/*=============================================================================
	DrawComplex_GLSL.cpp: UE1 BSP Rendering Shaders

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

const UXOpenGLRenderDevice::ShaderProgram::DrawCallParameterInfo UXOpenGLRenderDevice::DrawComplexParametersInfo[]
=
{
	{"vec4", "DiffuseUV", 0},
	{"vec4", "LightMapUV", 0},
	{"vec4", "FogMapUV", 0},
	{"vec4", "DetailUV", 0},
	{"vec4", "MacroUV", 0},
	{"vec4", "EnviroMapUV", 0},
	{"vec4", "DiffuseInfo", 0},
	{"vec4", "MacroInfo", 0},
	{"vec4", "BumpMapInfo", 0},
	{"vec4", "HeightMapInfo", 0},
	{"vec4", "XAxis", 0},
	{"vec4", "YAxis", 0},
	{"vec4", "ZAxis", 0},
	{"vec4", "DrawColor", 0},
	{"vec4", "DistanceFogColor", 0},
	{"vec4", "DistanceFogInfo", 0},
	{"int", "DistanceFogMode", 0},
	{"uint", "Padding0", 0},
	{"uint", "Padding1", 0},
    {"uint", "Padding2", 0},
    {"uvec4", "TexHandles", 4},
	{ nullptr, nullptr, 0}
};

// Vertexshader for DrawComplexSurface, single pass.
void UXOpenGLRenderDevice::DrawComplexProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice * GL, FShaderWriterX & Out)
{
    Out << R"(
layout(location = 0) in vec3 Coords; // == gl_Vertex
layout(location = 1) in uint DrawID; // emulated gl_DrawID
layout(location = 2) in vec4 Normal;

out vec3 vCoords;
out vec2 vTexCoords;

#if OPT_LightMap
out vec2 vLightMapCoords;
#endif

#if OPT_FogMap
out vec2 vFogMapCoords;
#endif

#if OPT_DetailTexture
out vec2 vDetailTexCoords;
#endif

#if OPT_MacroTexture
out vec2 vMacroTexCoords;
#endif

#if OPT_EnvironmentMap
out vec2 vEnvironmentTexCoords;
#endif

#if OPT_BumpMap
out vec2 vBumpTexCoords;
#endif

#if OPT_BumpMap || OPT_HWLighting
flat out mat3 vTBNMat;
out vec3 vTangentViewPos;
out vec3 vTangentFragPos;
#endif

#if OPT_BumpMap || OPT_DistanceFog || OPT_HWLighting
out vec4 vEyeSpacePos;
#endif

#if OPT_SupportsClipDistance
out float gl_ClipDistance[OPT_MaxClippingPlanes];
#endif

void main(void)
{
  // Point Coords
  vCoords = Coords.xyz;

  // UDot/VDot calculation.
  vec3 MapCoordsXAxis = GetXAxis(DrawID).xyz;
  vec3 MapCoordsYAxis = GetYAxis(DrawID).xyz;
#if OPT_Editor || OPT_BumpMap || OPT_HWLighting
  vec3 MapCoordsZAxis = GetZAxis(DrawID).xyz;
#endif

  float UDot = GetXAxis(DrawID).w;
  float VDot = GetYAxis(DrawID).w;
  
  float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
  float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
  vec2  MapDot = vec2(MapDotU, MapDotV);

  // Texture UV to fragment
  vec2 TexMapMult = GetDiffuseUV(DrawID).xy;
  vec2 TexMapPan = GetDiffuseUV(DrawID).zw;
  vTexCoords = (MapDot - TexMapPan) * TexMapMult;

  // Texture UV Lightmap to fragment
#if OPT_LightMap
  vec2 LightMapMult = GetLightMapUV(DrawID).xy;
  vec2 LightMapPan = GetLightMapUV(DrawID).zw;
  vLightMapCoords = (MapDot - LightMapPan) * LightMapMult;
#endif

  // Texture UV FogMap
#if OPT_FogMap
  vec2 FogMapMult = GetFogMapUV(DrawID).xy;
  vec2 FogMapPan = GetFogMapUV(DrawID).zw;
  vFogMapCoords = (MapDot - FogMapPan) * FogMapMult;
#endif

  // Texture UV DetailTexture
#if OPT_DetailTexture
  vec2 DetailMult = GetDetailUV(DrawID).xy;
  vec2 DetailPan = GetDetailUV(DrawID).zw;
  vDetailTexCoords = (MapDot - DetailPan) * DetailMult;
#endif

  // Texture UV Macrotexture
#if OPT_MacroTexture
  vec2 MacroMult = GetMacroUV(DrawID).xy;
  vec2 MacroPan = GetMacroUV(DrawID).zw;
  vMacroTexCoords = (MapDot - MacroPan) * MacroMult;
#endif

  // Texture UV EnvironmentMap
#if OPT_EnvironmentMap  
  vec2 EnvironmentMapMult = GetEnviroMapUV(DrawID).xy;
  vec2 EnvironmentMapPan = GetEnviroMapUV(DrawID).zw;
  vEnvironmentTexCoords = (MapDot - EnvironmentMapPan) * EnvironmentMapMult;
#endif

#if OPT_BumpMap || OPT_HWLighting || OPT_DistanceFog
  vEyeSpacePos = modelviewMat * vec4(Coords.xyz, 1.0);
#endif

#if OPT_BumpMap || OPT_HWLighting
  vec3 EyeSpacePos = normalize(FrameCoords[0].xyz); // despite pretty perfect results (so far) this still seems somewhat wrong to me.
  vec3 T = normalize(vec3(MapCoordsXAxis.x, MapCoordsXAxis.y, MapCoordsXAxis.z));
  vec3 B = normalize(vec3(MapCoordsYAxis.x, MapCoordsYAxis.y, MapCoordsYAxis.z));
  vec3 N = normalize(vec3(MapCoordsZAxis.x, MapCoordsZAxis.y, MapCoordsZAxis.z)); //SurfNormals.

  // TBN must have right handed coord system.
  //if (dot(cross(N, T), B) < 0.0)
  //   T = T * -1.0;
  vTBNMat = transpose(mat3(T, B, N));

  vTangentViewPos = vTBNMat * EyeSpacePos.xyz;
  vTangentFragPos = vTBNMat * Coords.xyz;
#endif

  gl_Position = modelviewprojMat * vec4(Coords.xyz, 1.0);
  vDrawID = DrawID;

#if OPT_SupportsClipDistance
  uint ClipIndex = uint(ClipParams.x);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Coords.xyz);
#endif
}
)";
}

static void EmitParallaxFunction(UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
    Out << R"(
#if OPT_HeightMap
vec2 ParallaxMapping(vec2 ptexCoords, vec3 viewDir, uvec2 TexHandle, out float parallaxHeight)
{
    float vParallaxScale = GetHeightMapInfo(vDrawID).z * 0.025;
    float vTimeSeconds = GetHeightMapInfo(vDrawID).w; // Surface.Level->TimeSeconds
        )";
        if (GL->ParallaxVersion == Parallax_Basic) // very basic implementation
        {
            Out << R"(
  float height = GetTexel(TexHandle, Texture7, ptexCoords).r;
  return ptexCoords - viewDir.xy * (height * 0.1);
}
#endif
)";
        }
        else if (GL->ParallaxVersion == Parallax_Occlusion) // parallax occlusion mapping
        {
            constexpr FLOAT minLayers = 8.f;
            constexpr FLOAT maxLayers = 32.f;

            Out << "  float numLayers = mix(" << maxLayers << ", " << minLayers << ", abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));" END_LINE;
            Out << R"(  
  //vParallaxScale += 8.0f * sin(vTimeSeconds) + 4.0 * cos(2.3f * vTimeSeconds);
  // number of depth layers		
  float layerDepth = 1.0 / numLayers; // calculate the size of each layer
  float currentLayerDepth = 0.0; // depth of current layer

  // the amount to shift the texture coordinates per layer (from vector P)
  vec2 P = viewDir.xy / viewDir.z * vParallaxScale;
  vec2 deltaTexCoords = P / numLayers;

  // get initial values
  vec2  currentTexCoords = ptexCoords;
  float currentDepthMapValue = 0.0;
  currentDepthMapValue = GetTexel(TexHandle, Texture7, currentTexCoords).r;

  while (currentLayerDepth < currentDepthMapValue)
  {
    currentTexCoords -= deltaTexCoords; // shift texture coordinates along direction of P
    currentDepthMapValue = GetTexel(TexHandle, Texture7, currentTexCoords).r; // get depthmap value at current texture coordinates
    currentLayerDepth += layerDepth; // get depth of next layer
  }

  vec2 prevTexCoords = currentTexCoords + deltaTexCoords; // get texture coordinates before collision (reverse operations)

  // get depth after and before collision for linear interpolation
  float afterDepth = currentDepthMapValue - currentLayerDepth;
  float beforeDepth = GetTexel(TexHandle, Texture7, currentTexCoords).r - currentLayerDepth + layerDepth;

  // interpolation of texture coordinates
  float weight = afterDepth / (afterDepth - beforeDepth);
  vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
  return finalTexCoords;
}
#endif
)";
        }
        else if (GL->ParallaxVersion == Parallax_Relief) // Relief Parallax Mapping
        {
            // determine required number of layers
            constexpr FLOAT minLayers = 10.f;
            constexpr FLOAT maxLayers = 15.f;
            constexpr INT numSearches = 5;
            Out << "  float numLayers = mix(" << maxLayers << ", " << minLayers << ", abs(dot(vec3(0, 0, 1), viewDir)));" END_LINE;
            Out << R"(
  float layerHeight = 1.0 / numLayers; // height of each layer
  float currentLayerHeight = 0.0; // depth of current layer
  vec2 dtex = vParallaxScale * viewDir.xy / viewDir.z / numLayers; // shift of texture coordinates for each iteration
  vec2 currentTexCoords = ptexCoords; // current texture coordinates
  float heightFromTexture = GetTexel(TexHandle, Texture7, currentTexCoords).r; // depth from heightmap

  // while point is above surface
  while (heightFromTexture > currentLayerHeight)
  {
    currentLayerHeight += layerHeight; // go to the next layer
    currentTexCoords -= dtex; // shift texture coordinates along V
    heightFromTexture = GetTexel(TexHandle, Texture7, currentTexCoords).r; // new depth from heightmap
  }

  ///////////////////////////////////////////////////////////
  // Start of Relief Parallax Mapping
  // decrease shift and height of layer by half
  vec2 deltaTexCoord = dtex / 2.0;
  float deltaHeight = layerHeight / 2.0;

  // return to the mid point of previous layer
  currentTexCoords += deltaTexCoord;
  currentLayerHeight -= deltaHeight;

  // binary search to increase precision of Steep Paralax Mapping
  for (int i = 0; i < )" << numSearches << R"(; i++)
  {
    // decrease shift and height of layer by half
    deltaTexCoord /= 2.0;
    deltaHeight /= 2.0;
 
    // new depth from heightmap
    heightFromTexture = GetTexel(TexHandle, Texture7, currentTexCoords).r;

    // shift along or agains vector V
    if (heightFromTexture > currentLayerHeight) // below the surface
    {
      currentTexCoords -= deltaTexCoord;
      currentLayerHeight += deltaHeight;
    }
    else // above the surface
    {
      currentTexCoords += deltaTexCoord;
      currentLayerHeight -= deltaHeight;
    }
  }

  // return results
  parallaxHeight = currentLayerHeight;
  return currentTexCoords;
}
#endif
)";

        }
        else
        {
            Out << R"(
  return ptexCoords;
}
#endif
)";
        }
}

void UXOpenGLRenderDevice::DrawComplexProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
    Out << R"(
in vec3 vCoords;
in vec2 vTexCoords;

#if OPT_LightMap
in vec2 vLightMapCoords;
#endif

#if OPT_FogMap
in vec2 vFogMapCoords;
#endif

#if OPT_DetailTexture
in vec2 vDetailTexCoords;
#endif

#if OPT_MacroTexture
in vec2 vMacroTexCoords;
#endif

#if OPT_EnvironmentMap
in vec2 vEnvironmentTexCoords;
#endif

#if OPT_BumpMap
flat in mat3 vTBNMat;
in vec2 vBumpTexCoords;
#endif

#if OPT_BumpMap || OPT_HWLighting
in vec3 vTangentViewPos;
in vec3 vTangentFragPos;
#endif

#if OPT_BumpMap || OPT_DistanceFog
in vec4 vEyeSpacePos;
#endif

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
)";

    EmitParallaxFunction(GL, Out);

    Out << R"(
void main(void)
{
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);

  vec4 TotalColor = vec4(1.0);
  vec2 texCoords = vTexCoords;

#if OPT_BumpMap || OPT_HWLighting
  vec3 TangentViewDir = normalize(vTangentViewPos - vTangentFragPos);
  int NumLights = int(LightData4[0].y);
#endif

#if OPT_HeightMap
  float parallaxHeight = 1.0;
  // get new texture coordinates from Parallax Mapping
  texCoords = ParallaxMapping(vTexCoords, TangentViewDir, GetTexHandles(vDrawID, 3).zw, parallaxHeight);
  //if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
  //discard; // texCoords = vTexCoords;
#endif

  vec4 Color = GetTexel(GetTexHandles(vDrawID, 0).xy, Texture0, texCoords.xy);
  Color *= GetDiffuseInfo(vDrawID).x; // Diffuse factor.
  Color.a *= GetDiffuseInfo(vDrawID).z; // Alpha.
	
  TotalColor = ApplyPolyFlags(Color);
  vec4 LightColor = vec4(1.0);

#if OPT_HWLighting
  float MinLight = 0.05f;
  float LightAdd = 0.0f;
  LightColor = vec4(0.0);

  for (int i = 0; i < NumLights; i++)
  {
    float WorldLightRadius = LightData4[i].x;
    float LightRadius = LightData2[i].w;
    float RWorldLightRadius = WorldLightRadius * WorldLightRadius;

    vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
    float dist = distance(vCoords, InLightPos);

    if (dist < RWorldLightRadius)
    {
      // Light color
      vec4 CurrentLightColor = vec4(LightData1[i].x, LightData1[i].y, LightData1[i].z, 1.0);
      float b = WorldLightRadius / (RWorldLightRadius * MinLight);
      float attenuation = WorldLightRadius / (dist + b * dist * dist);
      //float attenuation = 0.82*(1.0-smoothstep(LightRadius,24.0*LightRadius+0.50,dist));
      LightColor += CurrentLightColor * attenuation;
    }
  }
  
  TotalColor *= LightColor;

#elif OPT_LightMap

  LightColor = GetTexel(GetTexHandles(vDrawID, 0).zw, Texture1, vLightMapCoords);
  // Fetch lightmap texel. Data in LightMap is in 0..127/255 range, which needs to be scaled to 0..2 range.
  LightColor.rgb =
# if OPT_GLES
	LightColor.bgr
# else
	LightColor.rgb
# endif
	* (LightMapIntensity * 255.0 / 127.0);
  LightColor.a = 1.0;

#endif

#if OPT_DetailTexture
  {
    float NearZ = vCoords.z / 512.0;
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
        DetailTexColor = GetTexel(GetTexHandles(vDrawID, 1).zw, Texture3, vDetailTexCoords * DetailScale);
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

#if OPT_MacroTexture
  {    
    vec4 MacrotexColor = GetTexel(GetTexHandles(vDrawID, 2).xy, Texture4, vMacroTexCoords); 
# if OPT_Masked
    if (MacrotexColor.a < 0.5)
      discard;
    else MacrotexColor.rgb /= MacrotexColor.a;
# elif OPT_AlphaBlended    
    if (MacrotexColor.a < 0.01)
      discard;
# endif

    vec3 hsvMacroTex = rgb2hsv(MacrotexColor.rgb);
    hsvMacroTex.b += (MacrotexColor.r - 0.1);
    hsvMacroTex = hsv2rgb(hsvMacroTex);
    MacrotexColor = vec4(hsvMacroTex, 1.0);
    TotalColor *= MacrotexColor;
  }
#endif

	// BumpMap (Normal Map)
#if OPT_BumpMap
  {
    float MinLight = 0.05f;
    
    //normal from normal map
    vec3 TextureNormal = normalize(GetTexel(GetTexHandles(vDrawID, 2).zw, Texture5, texCoords).rgb * 2.0 - 1.0); // has to be texCoords instead of vBumpTexCoords, otherwise alignment won't work on bumps.
    vec3 BumpColor;
    vec3 TotalBumpColor = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < NumLights; ++i)
    {
      vec3 CurrentLightColor = vec3(LightData1[i].x, LightData1[i].y, LightData1[i].z);
      
      float NormalLightRadius = LightData5[i].x;
      bool bZoneNormalLight = bool(LightData5[i].y);
      float LightBrightness = LightData5[i].z / 255.0; // use LightBrightness to adjust specular reflection.

      if (NormalLightRadius == 0.0)
        NormalLightRadius = LightData2[i].w * 64.0;

      bool bSunlight = bool(uint(LightData2[i].x == LE_Sunlight));

      vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
      float dist = distance(vCoords, InLightPos);

      float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * MinLight);
      float attenuation = NormalLightRadius / (dist + b * dist * dist);

# if OPT_Unlit
      InLightPos = vec3(1.0, 1.0, 1.0); //no idea whats best here. Arbitrary value based on some tests.
# endif

      if ((NormalLightRadius == 0.0 || (dist > NormalLightRadius) || (bZoneNormalLight && (LightData4[i].z != LightData4[i].w))) && !bSunlight) // Do not consider if not in range or in a different zone.
        continue;

      vec3 TangentLightPos = vTBNMat * InLightPos;
      vec3 TangentlightDir = normalize(TangentLightPos - vTangentFragPos);

      // ambient
      vec3 ambient = 0.1 * TotalColor.xyz;

      // diffuse
      float diff = max(dot(TangentlightDir, TextureNormal), 0.0);
      vec3 diffuse = diff * TotalColor.xyz;

      // specular
      vec3 halfwayDir = normalize(TangentlightDir + TangentViewDir);
      float spec = pow(max(dot(TextureNormal, halfwayDir), 0.0), 8.0);
      vec3 specular = vec3(max(GetBumpMapInfo(vDrawID).y, 0.1)) * spec * CurrentLightColor * LightBrightness;

      TotalBumpColor += (ambient + diffuse + specular) * attenuation;
    }
    TotalColor += vec4(clamp(TotalBumpColor, 0.0, 1.0), 1.0);
  }
#endif

  vec4 FogColor = vec4(0.0);

#if OPT_FogMap
    FogColor = GetTexel(GetTexHandles(vDrawID, 1).xy, Texture2, vFogMapCoords) * 2.0;
#endif

#if OPT_EnvironmentMap
  {    
    vec4 EnvironmentColor = GetTexel(GetTexHandles(vDrawID, 3).xy, Texture6, vEnvironmentTexCoords);
# if OPT_Masked
    if (EnvironmentColor.a < 0.5)
      discard;
    else EnvironmentColor.rgb /= EnvironmentColor.a;
# elif OPT_AlphaBlended
    if (EnvironmentColor.a < 0.01)
      discard;
# endif

    TotalColor *= vec4(EnvironmentColor.rgb, 1.0);
  }
#endif

#if !OPT_Modulated
    TotalColor = TotalColor * LightColor;
#endif

  TotalColor += FogColor;

#if OPT_DistanceFog
  // Add DistanceFog, needs to be added after Light has been applied.  
  int vDistanceFogMode = GetDistanceFogMode(vDrawID);
  if (vDistanceFogMode >= 0)
  {
    vec4 vDistanceFogInfo = GetDistanceFogInfo(vDrawID);
    vec4 vDistanceFogColor = GetDistanceFogColor(vDrawID);

    FogParameters DistanceFogParams;
    DistanceFogParams.FogStart = vDistanceFogInfo.x;
    DistanceFogParams.FogEnd = vDistanceFogInfo.y;
    DistanceFogParams.FogDensity = vDistanceFogInfo.z;
    DistanceFogParams.FogMode = vDistanceFogMode;

# if OPT_Modulated
    DistanceFogParams.FogColor = vec4(0.5, 0.5, 0.5, 0.0);
# elif OPT_Translucent && !OPT_EnvironmentMap
    DistanceFogParams.FogColor = vec4(0.0, 0.0, 0.0, 0.0);
# else
    DistanceFogParams.FogColor = vDistanceFogColor;
# endif

    DistanceFogParams.FogCoord = abs(vEyeSpacePos.z / vEyeSpacePos.w);
    TotalColor = mix(TotalColor, DistanceFogParams.FogColor, getFogFactor(DistanceFogParams));
  }
#endif
	
#if OPT_Editor
  vec4 vDrawColor = GetDrawColor(vDrawID);
  if (RendMap == REN_Zones || RendMap == REN_PolyCuts || RendMap == REN_Polys)
  {
    TotalColor += 0.5;
    TotalColor *= vDrawColor;
  }
# if 0
  else if (RendMap == REN_Normals) //Thank you han!
  {
    // Dot.
    float T = 0.5 * dot(normalize(vCoords), GetZAxis(vDrawID).xyz);    
#  if OPT_Selected
    // Selected.
    TotalColor = vec4(0.0, 0.0, abs(T), 1.0);
#  else
    // Normal.
    TotalColor = vec4(max(0.0, T), max(0.0, -T), 0.0, 1.0);
#  endif
  }
# endif
  else if (RendMap == REN_PlainTex)
    TotalColor = Color;


# if OPT_Selected
  if (RendMap != REN_Normals)
  {
    TotalColor.r = (TotalColor.r * 0.75);
    TotalColor.g = (TotalColor.g * 0.75);
    TotalColor.b = (TotalColor.b * 0.75) + 0.1;
    TotalColor = clamp(TotalColor, 0.0, 1.0);
    if (TotalColor.a < 0.5)
      TotalColor.a = 0.51;
  }
# endif

  // HitSelection, Zoneview etc.
  if (bool(HitTesting))
    TotalColor = vDrawColor; // Use ONLY DrawColor.
# if !OPT_Modulated
  else 
    TotalColor = GammaCorrect(Gamma, TotalColor);    
# endif

#endif // OPT_Editor

#if OPT_SimulateMultiPass
  FragColor = TotalColor;
  FragColor1 = ((vec4(1.0) - TotalColor) * LightColor);
#else
  FragColor = TotalColor;
#endif

#if !OPT_Editor && !OPT_Modulated
  FragColor = GammaCorrect(Gamma, FragColor);
#endif
}
)";

	/*
	//
	// EnviroMap.
	//
	// Simple GLSL implementation of the C++ code. Should be obsoleted by some fancy
	// per pixel sphere mapping implementation. But for now, just use this approach
	// as PF_Environment handling is the last missing peace on obsoleting RenderSubsurface.
	//
	vec2 EnviroMap( vec3 Point, vec3 Normal )
	{
	vec3 R = reflect(normalize(Point),Normal);
	return vec2(0.5*dot(R,Uncoords_XAxis)+0.5,0.5*dot(R,Uncoords_YAxis)+0.5);
	}
	*/
}

// unused. Maybe later.
#if 0
Out << R"(
float parallaxSoftShadowMultiplier(in vec3 L, in vec2 initialTexCoord, in float initialHeight)
{
  float shadowMultiplier = 1;
)";

constexpr FLOAT minLayers = 15;
constexpr FLOAT maxLayers = 30;

// calculate lighting only for surface oriented to the light source
Out << R"(
if(dot(vec3(0, 0, 1), L) > 0)
{
  // calculate initial parameters
  "float numSamplesUnderSurface = 0;
  "shadowMultiplier = 0;"
)";

Out << "  float numLayers = mix(" << maxLayers << ", " << minLayers << ", abs(dot(vec3(0, 0, 1), L)));" END_LINE;
Out << R"(
  float layerHeight = initialHeight / numLayers;
  vec2 texStep = vParallaxScale * L.xy / L.z / numLayers;

  // current parameters
  float currentLayerHeight = initialHeight - layerHeight;
  vec2 currentTexCoords = initialTexCoord + texStep;
  float heightFromTexture = GetTexel(GetTexHandles(vDrawID, 3).zw, Texture7, currentTexCoords).r;

  int stepIndex = 1;

  // while point is below depth 0.0 )
  while(currentLayerHeight > 0)
  {
    if(heightFromTexture < currentLayerHeight) // if point is under the surface
    {
      // calculate partial shadowing factor
      numSamplesUnderSurface += 1;
      float newShadowMultiplier = (currentLayerHeight - heightFromTexture) * (1.0 - stepIndex / numLayers);
      shadowMultiplier = max(shadowMultiplier, newShadowMultiplier);
    }

    // offset to the next layer
    stepIndex += 1;
    currentLayerHeight -= layerHeight;
    currentTexCoords += texStep;
    heightFromTexture = GetTexel(GetTexHandles(vDrawID, 3).zw, Texture7, currentTexCoords).r;
  }

  // Shadowing factor should be 1 if there were no points under the surface
  if(numSamplesUnderSurface < 1)
    shadowMultiplier = 1;
  else
    shadowMultiplier = 1.0 - shadowMultiplier;

  return shadowMultiplier;
}
)";

#endif // 0

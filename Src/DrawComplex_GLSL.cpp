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

const UXOpenGLRenderDevice::ShaderProgram::DrawCallParameterInfo Info[]
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
	{"uint", "DrawFlags", 0},
	{"uint", "HitTesting", 0},
	{"uint", "TextureFormat", 0},
	{"uint", "PolyFlags", 0},
	{"uint", "RendMap", 0},
	{"int", "DistanceFogMode", 0},
	{"uint", "Padding0", 0},
	{"uint", "Padding1", 0},
	{"uvec4", "TexHandles", 4},
	{ nullptr, nullptr, 0}
};

void UXOpenGLRenderDevice::DrawComplexProgram::EmitHeader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
    EmitDrawCallParametersHeader(ShaderType, GL, Info, Out, Program, GlobalShaderBindingIndices::ComplexParametersIndex, true);

	Out << R"(
uniform sampler2D Texture0; //Base Texture
uniform sampler2D Texture1; //Lightmap
uniform sampler2D Texture2; //Fogmap
uniform sampler2D Texture3; //Detail Texture
uniform sampler2D Texture4; //Macro Texture
uniform sampler2D Texture5; //BumpMap
uniform sampler2D Texture6; //EnvironmentMap
uniform sampler2D Texture7; //HeightMap
)";
}

// Vertexshader for DrawComplexSurface, single pass.
void UXOpenGLRenderDevice::DrawComplexProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice * GL, FShaderWriterX & Out, ShaderProgram* Program)
{
    Out << R"(
layout(location = 0) in vec4 Coords; // == gl_Vertex
layout(location = 1) in vec4 Normal; // == gl_Vertex

flat out vec4 vDistanceFogColor;
flat out vec4 vDistanceFogInfo;
flat out uvec2 vTexHandle;
flat out uvec2 vLightMapTexHandle;
flat out uvec2 vFogMapTexHandle;
flat out uvec2 vDetailTexHandle;
flat out uvec2 vMacroTexHandle;
flat out uvec2 vBumpMapTexHandle;
flat out uvec2 vEnviroMapTexHandle;
flat out uvec2 vHeightMapTexHandle;
flat out uint vDrawFlags;
flat out uint vTextureFormat;
flat out uint vPolyFlags;
flat out int vDistanceFogMode;
flat out float vBaseDiffuse;
flat out float vBaseAlpha;
flat out float vParallaxScale;
flat out float vGamma;
flat out float vBumpMapSpecular;
flat out float vTimeSeconds;
)";

    if (GIsEditor)
        Out << "flat out vec3 vSurfaceNormal;" END_LINE; // f.e. Render normal view.
#if ENGINE_VERSION!=227
    if (GL->BumpMaps)
#endif
    {
        Out << "flat out mat3 vTBNMat;" END_LINE;
    }

    if (GL->SupportsClipDistance)
        Out << "out float gl_ClipDistance[" << GL->MaxClippingPlanes << "];" END_LINE;

    if (GIsEditor)
    {
        Out << R"(
flat out uint vHitTesting;
flat out uint vRendMap; 
flat out vec4 vDrawColor;
)";
    }

    Out << R"(
out vec4 vEyeSpacePos;
out vec3 vCoords;
out vec3 vTangentViewPos;
out vec3 vTangentFragPos;
out vec2 vTexCoords;
out vec2 vLightMapCoords;
out vec2 vFogMapCoords;
)";

	if (GL->DetailTextures)
		Out << "out vec2 vDetailTexCoords;" END_LINE;
	if (GL->MacroTextures)
		Out << "out vec2 vMacroTexCoords;" END_LINE;
	if (GL->BumpMaps)
		Out << "out vec2 vBumpTexCoords;" END_LINE;
#if ENGINE_VERSION==227
	Out << "out vec2 vEnvironmentTexCoords;" END_LINE;
#endif

#if 1
	Out << R"(
void main(void)
{
  // pass drawcall parameters 
  vPolyFlags = GetPolyFlags();
  vDrawFlags = GetDrawFlags();
  vTextureFormat = GetTextureFormat();
  vTexHandle = GetTexHandles(0).xy;
  vGamma = GetZAxis().w;
  vLightMapTexHandle = GetTexHandles(0).zw;
  vFogMapTexHandle = GetTexHandles(1).xy;
  vDetailTexHandle = GetTexHandles(1).zw;
  vMacroTexHandle = GetTexHandles(2).xy;
  vBumpMapTexHandle = GetTexHandles(2).zw;  
  vBaseDiffuse = GetDiffuseInfo().x;
  vBaseAlpha = GetDiffuseInfo().z;
  vBumpMapSpecular = GetBumpMapInfo().y;
)";

#if ENGINE_VERSION==227
    Out << R"(
  vParallaxScale = GetHeightMapInfo().z * 0.025; // arbitrary to get DrawScale into (for this purpose) sane regions.
  vTimeSeconds = GetHeightMapInfo().w; // Surface.Level->TimeSeconds
  vDistanceFogInfo = GetDistanceFogInfo();
  vDistanceFogColor = GetDistanceFogColor();
  vDistanceFogMode = GetDistanceFogMode();
  vEnviroMapTexHandle = GetTexHandles(3).xy;
  vHeightMapTexHandle = GetTexHandles(3).zw;
)";
#endif

    if (GIsEditor)
    {
        Out << R"(
  vRendMap = GetRendMap();
  vHitTesting = GetHitTesting();
  vDrawColor = GetDrawColor();
)";
    }

	Out << R"(
  // Point Coords
  vCoords = Coords.xyz;

  // UDot/VDot calculation.
  vec3 MapCoordsXAxis = GetXAxis().xyz;
  vec3 MapCoordsYAxis = GetYAxis().xyz;
)";

#if ENGINE_VERSION!=227
	if (GIsEditor || GL->BumpMaps)
#endif
	{
		Out << "  vec3 MapCoordsZAxis = GetZAxis().xyz;" END_LINE;
	}

	Out << R"(
  float UDot = GetXAxis().w;
  float VDot = GetYAxis().w;
  
  float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
  float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
  vec2  MapDot = vec2(MapDotU, MapDotV);

  // Texture UV to fragment
  vec2 TexMapMult = GetDiffuseUV().xy;
  vec2 TexMapPan = GetDiffuseUV().zw;
  vTexCoords = (MapDot - TexMapPan) * TexMapMult;

  // Texture UV Lightmap to fragment
  if ((vDrawFlags & )" << DF_LightMap << R"(u) == )" << DF_LightMap << R"(u)
  {
    vec2 LightMapMult = GetLightMapUV().xy;
    vec2 LightMapPan = GetLightMapUV().zw;
    vLightMapCoords = (MapDot - LightMapPan) * LightMapMult;
  }

  // Texture UV FogMap
  if ((vDrawFlags & )" << DF_FogMap << R"(u) == )" << DF_FogMap << R"(u)
  {
    vec2 FogMapMult = GetFogMapUV().xy;
    vec2 FogMapPan = GetFogMapUV().zw;
    vFogMapCoords = (MapDot - FogMapPan) * FogMapMult;
  }
)";

	// Texture UV DetailTexture
	if (GL->DetailTextures)
	{
		Out << R"(
  if ((vDrawFlags & )" << DF_DetailTexture << R"(u) == )" << DF_DetailTexture << R"(u)
  {
    vec2 DetailMult = GetDetailUV().xy;
    vec2 DetailPan = GetDetailUV().zw;
    vDetailTexCoords = (MapDot - DetailPan) * DetailMult;
  }
)";
	}

	// Texture UV Macrotexture
	if (GL->MacroTextures)
	{
		Out << R"(
  if ((vDrawFlags & )" << DF_MacroTexture << R"(u) == )" << DF_MacroTexture << R"(u)
  {
    vec2 MacroMult = GetMacroUV().xy;
    vec2 MacroPan = GetMacroUV().zw;
    vMacroTexCoords = (MapDot - MacroPan) * MacroMult;
  }
)";
	}

	// Texture UV EnvironmentMap
#if ENGINE_VERSION==227
	Out << R"(
  if ((vDrawFlags & )" << DF_EnvironmentMap << R"(u) == )" << DF_EnvironmentMap << R"(u)
  {
    vec2 EnvironmentMapMult = GetEnviroMapUV().xy;
    vec2 EnvironmentMapPan = GetEnviroMapUV().zw;
    vEnvironmentTexCoords = (MapDot - EnvironmentMapPan) * EnvironmentMapMult;
  }
)";
#endif

#if ENGINE_VERSION!=227
	if (GL->BumpMaps)
#endif
	{
		Out << R"(
  vEyeSpacePos = modelviewMat * vec4(Coords.xyz, 1.0);
  vec3 EyeSpacePos = normalize(FrameCoords[0].xyz); // despite pretty perfect results (so far) this still seems somewhat wrong to me.
  if ((vDrawFlags & )" << static_cast<INT>(DF_MacroTexture | DF_BumpMap) << R"(u) != 0u)
  {
    vec3 T = normalize(vec3(MapCoordsXAxis.x, MapCoordsXAxis.y, MapCoordsXAxis.z));
    vec3 B = normalize(vec3(MapCoordsYAxis.x, MapCoordsYAxis.y, MapCoordsYAxis.z));
    vec3 N = normalize(vec3(MapCoordsZAxis.x, MapCoordsZAxis.y, MapCoordsZAxis.z)); //SurfNormals.

    // TBN must have right handed coord system.
    //if (dot(cross(N, T), B) < 0.0)
    //   T = T * -1.0;
    vTBNMat = transpose(mat3(T, B, N));
    
    vTangentViewPos = vTBNMat * EyeSpacePos.xyz;
    vTangentFragPos = vTBNMat * Coords.xyz;
  }
)";

	}
	if (GIsEditor)
		Out << "  vSurfaceNormal = MapCoordsZAxis;" END_LINE;

	Out << "  gl_Position = modelviewprojMat * vec4(Coords.xyz, 1.0);" END_LINE;

	if (GL->SupportsClipDistance)
	{
		Out << "  uint ClipIndex = uint(ClipParams.x);" END_LINE;
		Out << "  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Coords.xyz);" END_LINE;
	}
	Out << "}" END_LINE;

#else

	Out << R"(
void main(void)
{
  vEyeSpacePos = modelviewMat * vec4(Coords.xyz, 1.0);

  // Point Coords
  vCoords = Coords.xyz;

  // UDot/VDot calculation.
  vec3 MapCoordsXAxis = GetXAxis().xyz;
  vec3 MapCoordsYAxis = GetYAxis().xyz;
  float UDot = GetXAxis().w;
  float VDot = GetYAxis().w;
  
  float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
  float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
  vec2  MapDot = vec2(MapDotU, MapDotV);

  // Texture UV to fragment
  vec2 TexMapMult = GetDiffuseUV().xy;
  vec2 TexMapPan = GetDiffuseUV().zw;
  vTexCoords = (MapDot - TexMapPan) * TexMapMult;

  gl_Position = modelviewprojMat * vec4(Coords.xyz, 1.0);
}
)";

#endif
}

void UXOpenGLRenderDevice::DrawComplexProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out, ShaderProgram* Program)
{
	Out << R"(
in vec3 vCoords;
in vec2 vTexCoords;
in vec2 vLightMapCoords;
in vec2 vFogMapCoords;
in vec3 vTangentViewPos;
in vec3 vTangentFragPos;

flat in uvec2 vTexHandle;
flat in uvec2 vLightMapTexHandle;
flat in uvec2 vFogMapTexHandle;
flat in uvec2 vDetailTexHandle;
flat in uvec2 vMacroTexHandle;
flat in uvec2 vBumpMapTexHandle;
flat in uvec2 vEnviroMapTexHandle;
flat in uvec2 vHeightMapTexHandle;
flat in uint vDrawFlags;
flat in uint vTextureFormat;
flat in uint vPolyFlags;
flat in float vBaseDiffuse;
flat in float vBaseAlpha;
flat in float vParallaxScale;
flat in float vGamma;
flat in float vBumpMapSpecular;
flat in vec4 vDistanceFogColor;
flat in vec4 vDistanceFogInfo;
flat in int vDistanceFogMode;

)";

    if (GIsEditor)
    {
        Out << R"(
flat in uint vHitTesting;
flat in uint vRendMap;
flat in vec4 vDrawColor;
)";
    }

	if (GIsEditor)
		Out << "flat in vec3 vSurfaceNormal;" END_LINE;
#if ENGINE_VERSION!=227
	if (GL->BumpMaps)
#endif
	{
		Out << "in vec4 vEyeSpacePos;" END_LINE;
		Out << "flat in mat3 vTBNMat;" END_LINE;
	}

	if (GL->DetailTextures)
		Out << "in vec2 vDetailTexCoords;" END_LINE;
	if (GL->MacroTextures)
		Out << "in vec2 vMacroTexCoords;" END_LINE;
	if (GL->BumpMaps)
		Out << "in vec2 vBumpTexCoords;" END_LINE;
#if ENGINE_VERSION==227
	Out << "in vec2 vEnvironmentTexCoords;" END_LINE;
#endif
	if (GL->OpenGLVersion == GL_ES)
	{
		Out << "layout(location = 0) out vec4 FragColor;" END_LINE;
		if (GL->SimulateMultiPass)
			Out << "layout(location = 1) out vec4 FragColor1;" END_LINE;
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out << "layout(location = 0, index = 1) out vec4 FragColor1;" END_LINE;
		Out << "layout(location = 0, index = 0) out vec4 FragColor;" END_LINE;
	}

	Out << R"(
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
vec2 ParallaxMapping(vec2 ptexCoords, vec3 viewDir, uvec2 TexHandle, out float parallaxHeight)
{
)";
	if (GL->ParallaxVersion == Parallax_Basic) // very basic implementation
	{
		Out << R"(
  float height = GetTexel(TexHandle, Texture7, ptexCoords).r;
  return ptexCoords - viewDir.xy * (height * 0.1);
}
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
)";

	}
	else
	{
		Out << R"(
  return ptexCoords;
}
)";
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
  float heightFromTexture = GetTexel(vMacroTexHandle, Texture7, currentTexCoords).r;

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
    heightFromTexture = GetTexel(vMacroTexHandle, Texture7, currentTexCoords).r;
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

#if 1
	Out << R"(
void main(void)
{
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);

  vec4 TotalColor = vec4(1.0);
  vec2 texCoords = vTexCoords;
)";

	if (GL->UseHWLighting || GL->BumpMaps)
	{
		Out << R"(
  vec3 TangentViewDir = normalize(vTangentViewPos - vTangentFragPos);
  int NumLights = int(LightData4[0].y);
)";

		if (GL->ParallaxVersion == Parallax_Basic ||
			GL->ParallaxVersion == Parallax_Occlusion ||
			GL->ParallaxVersion == Parallax_Relief)
		{
			// ParallaxMap
			Out << R"(
  float parallaxHeight = 1.0;
  if ((vDrawFlags & )" << DF_HeightMap << R"(u) == )" << DF_HeightMap << R"(u)
  {
    // get new texture coordinates from Parallax Mapping
    texCoords = ParallaxMapping(vTexCoords, TangentViewDir, vHeightMapTexHandle, parallaxHeight);
    //if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
    //discard; // texCoords = vTexCoords;
  }
)";
		}
	}

	Out << R"(
  vec4 Color = GetTexel(vTexHandle, Texture0, texCoords.xy);

  if (vBaseDiffuse > 0.0)
    Color *= vBaseDiffuse; // Diffuse factor.

  if (vBaseAlpha > 0.0)
    Color.a *= vBaseAlpha; // Alpha.

  if (vTextureFormat == )" << TEXF_BC5 << R"(u) //BC5 (GL_COMPRESSED_RG_RGTC2) compression
    Color.b = sqrt(1.0 - Color.r * Color.r + Color.g * Color.g);

  // Handle PF_Masked.
  if ((vPolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
  {
    if (Color.a < 0.5)
      discard;
    else Color.rgb /= Color.a;
  }
  else if ((vPolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u && Color.a < 0.01)
    discard;
  // if ((vPolyFlags&PF_Mirrored) == PF_Mirrored)
  // {
  //   add mirror code here.
  // }

  TotalColor = Color;
  vec4 LightColor = vec4(1.0);
)";

	if (GL->UseHWLighting)
	{
		constexpr FLOAT MinLight = 0.05f;
		Out << R"(
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
      float b = WorldLightRadius / (RWorldLightRadius * )" << MinLight << R"();
      float attenuation = WorldLightRadius / (dist + b * dist * dist);
      //float attenuation = 0.82*(1.0-smoothstep(LightRadius,24.0*LightRadius+0.50,dist));
      LightColor += CurrentLightColor * attenuation;
    }
  }
  
  TotalColor *= LightColor;
)";
	}
	else // Software Lighting
	{
		const char* LightColorOrder = GL->OpenGLVersion == GL_ES ? "bgr" : "rgb";

		// LightMap
		Out << R"(
  if ((vDrawFlags & )" << DF_LightMap << R"(u) == )" << DF_LightMap << R"(u)
  {
    LightColor = GetTexel(vLightMapTexHandle, Texture1, vLightMapCoords);
    // Fetch lightmap texel. Data in LightMap is in 0..127/255 range, which needs to be scaled to 0..2 range.
    LightColor.rgb = LightColor.)" << LightColorOrder << R"( * ()" << (GL->OneXBlending ? 1.f : 2.f) << R"(* 255.0 / 127.0);
    LightColor.a = 1.0;
  }
)";

	}

	// DetailTextures
	if (GL->DetailTextures)
	{
		Out << R"(
  if ((vDrawFlags & )" << DF_DetailTexture << R"(u) == )" << DF_DetailTexture << R"(u)
  {    
    float NearZ = vCoords.z / 512.0;
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
        DetailTexColor = GetTexel(vDetailTexHandle, Texture3, vDetailTexCoords * DetailScale);
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

	// MacroTextures
	if (GL->MacroTextures)
	{
		Out << R"(
  if ((vDrawFlags & )" << DF_MacroTexture << R"(u) == )" << DF_MacroTexture << R"(u)
  {    
    vec4 MacrotexColor = GetTexel(vMacroTexHandle, Texture4, vMacroTexCoords); 
    if ((vPolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
    {
      if (MacrotexColor.a < 0.5)
        discard;
      else MacrotexColor.rgb /= MacrotexColor.a;
    }
    else if ((vPolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u && MacrotexColor.a < 0.01)
      discard;

    vec3 hsvMacroTex = rgb2hsv(MacrotexColor.rgb);
    hsvMacroTex.b += (MacrotexColor.r - 0.1);
    hsvMacroTex = hsv2rgb(hsvMacroTex);
    MacrotexColor = vec4(hsvMacroTex, 1.0);
    TotalColor *= MacrotexColor;
  }
)";
	}

	// BumpMap (Normal Map)
	if (GL->BumpMaps)
	{
		constexpr float MinLight = 0.05f;
#if ENGINE_VERSION == 227
		FString Sunlight = FString::Printf(TEXT("bool bSunlight = bool(uint(LightData2[i].x == %du));"), LE_Sunlight);
#else
		FString Sunlight = TEXT("bool bSunlight = false;");
#endif

		Out << R"(
  if ((vDrawFlags & )" << DF_BumpMap << R"(u) == )" << DF_BumpMap << R"(u)
  {    
    //normal from normal map
    vec3 TextureNormal = normalize(GetTexel(vBumpMapTexHandle, Texture5, texCoords).rgb * 2.0 - 1.0); // has to be texCoords instead of vBumpTexCoords, otherwise alignment won't work on bumps.
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

      )" << appToAnsi(*Sunlight) << R"(

      vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
      float dist = distance(vCoords, InLightPos);

      float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * )" << MinLight << R"();
      float attenuation = NormalLightRadius / (dist + b * dist * dist);

      if ((vPolyFlags & )" << PF_Unlit << R"(u) == )" << PF_Unlit << R"(u)
        InLightPos = vec3(1.0, 1.0, 1.0); //no idea whats best here. Arbitrary value based on some tests.

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
      vec3 specular = vec3(max(vBumpMapSpecular, 0.1)) * spec * CurrentLightColor * LightBrightness;

      TotalBumpColor += (ambient + diffuse + specular) * attenuation;
    }
    TotalColor += vec4(clamp(TotalBumpColor, 0.0, 1.0), 1.0);
  }
)";
	}

	// FogMap
	Out << R"(
  vec4 FogColor = vec4(0.0);
  if ((vDrawFlags & )" << DF_FogMap << R"(u) == )" << DF_FogMap << R"(u)
    FogColor = GetTexel(vFogMapTexHandle, Texture2, vFogMapCoords) * 2.0;

  // EnvironmentMap
)";

#if ENGINE_VERSION==227
	Out << R"(
  if ((vDrawFlags & )" << DF_EnvironmentMap << R"(u) == )" << DF_EnvironmentMap << R"(u)
  {    
    vec4 EnvironmentColor = GetTexel(vEnviroMapTexHandle, Texture6, vEnvironmentTexCoords);
    if ((vPolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
    {
      if (EnvironmentColor.a < 0.5)
        discard;
      else EnvironmentColor.rgb /= EnvironmentColor.a;
    }
    else if ((vPolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u && EnvironmentColor.a < 0.01)
      discard;

    TotalColor *= vec4(EnvironmentColor.rgb, 1.0);
  }
)";
#endif

	Out << R"(
  //TotalColor=clamp(TotalColor,0.0,1.0); //saturate.
  if ((vPolyFlags & )" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
    TotalColor = TotalColor * LightColor;

  TotalColor += FogColor;

  // Add DistanceFog, needs to be added after Light has been applied.
)";

#if ENGINE_VERSION==227
	// stijn: Very slow! Went from 135 to 155FPS on CTF-BT-CallousV3 by just disabling this branch even tho 469 doesn't do distance fog
	Out << R"(    
  if (vDistanceFogMode >= 0)
  {    
    FogParameters DistanceFogParams;
    DistanceFogParams.FogStart = vDistanceFogInfo.x;
    DistanceFogParams.FogEnd = vDistanceFogInfo.y;
    DistanceFogParams.FogDensity = vDistanceFogInfo.z;
    DistanceFogParams.FogMode = vDistanceFogMode;

    if ((vPolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u)
      DistanceFogParams.FogColor = vec4(0.5, 0.5, 0.5, 0.0);
    else if ((vPolyFlags & )" << PF_Translucent << R"(u) == )" << PF_Translucent << R"(u && (vPolyFlags & )" << PF_Environment << R"(u) != )" << PF_Environment << R"(u)
      DistanceFogParams.FogColor = vec4(0.0, 0.0, 0.0, 0.0);
    else DistanceFogParams.FogColor = vDistanceFogColor;

    DistanceFogParams.FogCoord = abs(vEyeSpacePos.z / vEyeSpacePos.w);
    TotalColor = mix(TotalColor, DistanceFogParams.FogColor, getFogFactor(DistanceFogParams));
  }
)";
#endif
	if (GIsEditor)
	{
		// Editor support.
		Out << R"(
  if (vRendMap == )" << REN_Zones << R"(u || vRendMap == )" << REN_PolyCuts << R"(u || vRendMap == )" << REN_Polys << R"(u)
  {
    TotalColor += 0.5;
    TotalColor *= vDrawColor;
  }
)";

#if ENGINE_VERSION==227 && 0
		Out << R"(
  else if (vRendMap == )" << REN_Normals << R"(u) //Thank you han!
  {
    // Dot.
    float T = 0.5 * dot(normalize(vCoords), vSurfaceNormal);
    // Selected.
    if ((vPolyFlags & )" << PF_Selected << R"(u) == )" << PF_Selected << R"(u)
      TotalColor = vec4(0.0, 0.0, abs(T), 1.0);
    // Normal.
    else TotalColor = vec4(max(0.0, T), max(0.0, -T), 0.0, 1.0);
  }
)";
#endif
		Out << R"(
  else if (vRendMap == )" << REN_PlainTex << R"(u)
    TotalColor = Color;
)";

#if ENGINE_VERSION==227 && 0
		Out << "  if ((vRendMap != " << REN_Normals << "u) && ((vPolyFlags & " << PF_Selected << "u) == " << PF_Selected << "u))" END_LINE;
#else
		Out << "  if ((vPolyFlags & " << PF_Selected << "u) == " << PF_Selected << "u)" END_LINE;
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

  // HitSelection, Zoneview etc.
  if (bool(vHitTesting))
    TotalColor = vDrawColor; // Use ONLY DrawColor.
  else if ((vPolyFlags&)" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
    TotalColor = GammaCorrect(vGamma, TotalColor);    
)";
	}

	if (GL->SimulateMultiPass)
	{
		Out << R"(
  FragColor = TotalColor;
  FragColor1 = ((vec4(1.0) - TotalColor) * LightColor);
)";
	}
	else Out << "  FragColor = TotalColor;" END_LINE;

    if (!GIsEditor)
    {
        Out << R"(
  if ((vPolyFlags&)" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
    FragColor = GammaCorrect(vGamma, FragColor);
 )";
    }

	Out << "}" END_LINE;
#else
	Out << R"(
void main(void)
{
  vec4 Color = GetTexel(vTexHandle, Texture0, vTexCoords);

  if (vBaseDiffuse > 0.0)
    Color *= vBaseDiffuse; // Diffuse factor.

  if (vBaseAlpha > 0.0)
    Color.a *= vBaseAlpha; // Alpha.

  //FragColor = GetTexel(vTexHandle, Texture0, vTexCoords);
  FragColor = Color;
}
)";
#endif

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

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

#define END_LINE "\n"

// This is Higor's FCharWriter. We could (and should? move it elsewhere because
// OpenGLDrv also uses it)
class FCharWriter
{
public:
	TArray<ANSICHAR> Data;

	FCharWriter()
	{
		Data.Reserve(1000);
		Data.AddNoCheck();
		Data(0) = '\0';
	}

#if __cplusplus > 201103L || _MSVC_LANG > 201103L
	template < INT Size > FCharWriter& operator<<( const char(&Input)[Size])
	{
		if ( Size > 1 )
		{
			INT i = Data.Add(Size-1) - 1;
			appMemcpy( &Data(i), Input, Size);
		}
		check(Data.Last() == '\0');
		return *this;
	}
#endif

	FCharWriter& operator<<( const char* Input)
	{
		const char* InputEnd = Input;
		while ( *InputEnd != '\0' )
			InputEnd++;
		if ( InputEnd != Input )
		{
			INT Len = (INT)(InputEnd - Input);
			INT i = Data.Add(Len) - 1;
			check(Len > 0);
			check(Len < 4096);
			appMemcpy( &Data(i), Input, Len+1);
		}
		check(Data.Last() == '\0');
		return *this;
	}

	FCharWriter& operator<<(INT Input)
	{
		TCHAR Buffer[16];
		appSprintf( Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}

	FCharWriter& operator<<(UXOpenGLRenderDevice::DrawTileDrawDataIndices Input)
	{
		TCHAR Buffer[16];
		appSprintf( Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}

	FCharWriter& operator<<(UXOpenGLRenderDevice::DrawComplexDrawDataIndices Input)
	{
		TCHAR Buffer[16];
		appSprintf( Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}

	FCharWriter& operator<<(UXOpenGLRenderDevice::DrawGouraudDrawDataIndices Input)
	{
		TCHAR Buffer[16];
		appSprintf( Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}

	FCharWriter& operator<<(DrawFlags Input)
	{
		TCHAR Buffer[16];
		appSprintf( Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}

	FCharWriter& operator<<(EPolyFlags Input)
	{
		TCHAR Buffer[16];
		appSprintf( Buffer, TEXT("%i"), Input);
		return *this << appToAnsi(Buffer);
	}


	FCharWriter& operator<<(FLOAT Input)
	{
		TCHAR Buffer[32];
		appSprintf(Buffer, TEXT("%f"), Input);
		return *this << appToAnsi(Buffer);
	}

	const char* operator*()
	{
		return &Data(0);
	}

	GLsizei Length()
	{
		return (GLsizei)(Data.Num() - 1);
	}

	void Reset()
	{
		Data.EmptyNoRealloc();
		Data.AddNoCheck();
		Data(0) = '\0';
	}
};

class FShaderOutputDevice : public FCharWriter
{
public:
	const TCHAR* ProgName;
	
	FShaderOutputDevice(const TCHAR* _ProgName)
	{
		ProgName = _ProgName;
	}
};

typedef void (glShaderProg)(class UXOpenGLRenderDevice*, FShaderOutputDevice&);

static void Emit_Globals(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
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
	else Out << "#version 310 es" END_LINE;

	if (GL->UsingBindlessTextures) // defined for all variants of our bindlesstextures support
	{
		Out << "#extension GL_ARB_bindless_texture : require" END_LINE;

		if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_UBO) // defined if we're going to store handles in a UBO
		{
		}
		else if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_SSBO) // defined if we're going to store handles in a (much larger) SSBO
			Out << "#extension GL_ARB_shading_language_420pack : require" END_LINE;
		else if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_INT) // defined if we're going to pass handles directly to the shader using uniform parameters or the drawcall's parameters in the parameter SSBO
			Out << "#extension GL_ARB_gpu_shader_int64 : require" END_LINE;
	}

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
precision highp float;
precision highp int;
)";
	}

	Out << R"(
layout(std140) uniform GlobalMatrices
{
  mat4 modelMat;
  mat4 viewMat;
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

	if (GL->UsingBindlessTextures && GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_UBO)
	{
		Out << R"(
layout(std140) uniform TextureHandles
{
  layout(bindless_sampler) sampler2D Textures[)" << GL->MaxBindlessTextures << R"(];
};

vec4 GetTexel(uint BindlessTexNum, sampler2D BoundSampler, vec2 TexCoords)
{
  if (BindlessTexNum > 0u)
    return texture(Textures[BindlessTexNum], TexCoords);
  return texture(BoundSampler, TexCoords);
}
)";
	}
	else if (GL->UsingBindlessTextures && GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_SSBO)
	{
		Out << R"(
layout(std430, binding = 1) buffer TextureHandles
{
  uvec2 Textures[];
};

vec4 GetTexel(uint BindlessTexNum, sampler2D BoundSampler, vec2 TexCoords)
{
  if (BindlessTexNum > 0u)
    return texture(sampler2D(Textures[BindlessTexNum]), TexCoords);
  return texture(BoundSampler, TexCoords);
}
)";
	}
	else
	{
		// texture bound to TMU. BindlessTexBum is meaningless here
		Out << R"(
vec4 GetTexel(uint BindlessTexNum, sampler2D BoundSampler, vec2 TexCoords)
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

// The following directive resets the line number to 1 
// to have the correct output logging for a possible 
// error within the shader files.
#line 1
)";

}

// Fragmentshader for DrawSimple, line drawing.
static void Create_DrawSimple_Frag(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
uniform vec4 DrawColor;
uniform uint LineFlags;
uniform bool bHitTesting;
uniform float Gamma;
)";

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
	
	if (GIsEditor)
	{
		Out << "  if (!bHitTesting) {" END_LINE;
	}
	
	Out << R"(
  float InGamma = 1.0 / (Gamma * )" << (GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier) << R"(); // Gamma
  TotalColor.r = pow(TotalColor.r, InGamma);
  TotalColor.g = pow(TotalColor.g, InGamma);
  TotalColor.b = pow(TotalColor.b, InGamma);
)";
	
	if (GIsEditor)
		Out << "  }" END_LINE;

	if (GL->SimulateMultiPass)
		Out << "  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;" END_LINE;
	else 
        Out << "  FragColor = TotalColor;" END_LINE;
	Out << "}" END_LINE;
}

// Vertexshader for DrawSimple, Line drawing.
static void Create_DrawSimple_Vert(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
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

// Geoemtry shader for DrawSimple, line drawing.
static void Create_DrawSimple_Geo(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
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

// Fragmentshader for DrawComplexSurface, single pass.
static void Create_DrawComplexSinglePass_Frag(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
uniform sampler2D Texture0; //Base Texture
uniform sampler2D Texture1; //Lightmap
uniform sampler2D Texture2; //Fogmap
uniform sampler2D Texture3; //Detail Texture
uniform sampler2D Texture4; //Macro Texture
uniform sampler2D Texture5; //BumpMap
uniform sampler2D Texture6; //EnvironmentMap
uniform sampler2D Texture7; //HeightMap

in vec3 vCoords;
)";

	if (GIsEditor)
		Out << "flat in vec3 vSurfaceNormal;" END_LINE;
#if ENGINE_VERSION!=227
	if (GL->BumpMaps)
#endif
	{
		Out << "in vec4 vEyeSpacePos;" END_LINE;
		Out << "flat in mat3 vTBNMat;" END_LINE;
	}
	
	Out << R"(
in vec2 vTexCoords;
in vec2 vLightMapCoords;
in vec2 vFogMapCoords;
in vec3 vTangentViewPos;
in vec3 vTangentFragPos;
)";
	
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
	if (GL->UsingShaderDrawParameters)
	{
		Out << R"(
flat in uint vTexNum;
flat in uint vLightMapTexNum;
flat in uint vFogMapTexNum;
flat in uint vDetailTexNum;
flat in uint vMacroTexNum;
flat in uint vBumpMapTexNum;
flat in uint vEnviroMapTexNum;
flat in uint vHeightMapTexNum;
flat in uint vDrawFlags;
flat in uint vTextureFormat;
flat in uint vPolyFlags;
flat in float vBaseDiffuse;
flat in float vBaseAlpha;
flat in float vParallaxScale;
flat in float vGamma;
flat in float vBumpMapSpecular;
)";

		if (GIsEditor)
		{
			Out << R"(
flat in uint vHitTesting;
flat in uint vRendMap;
flat in vec4 vDrawColor;
)";
		}
		Out << R"(
flat in vec4 vDistanceFogColor;
flat in vec4 vDistanceFogInfo;
)";
	}
	else
	{
		Out << R"(
float vBumpMapSpecular;
uniform vec4 TexCoords[16];
uniform uint TexNum[16];
uniform uint DrawFlags[4];
)";
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

#if ENGINE_VERSION==227
	Out << R"(
vec2 ParallaxMapping(vec2 ptexCoords, vec3 viewDir, uint TexNum, out float parallaxHeight)
{
)";
	if (GL->ParallaxVersion == Parallax_Basic) // very basic implementation
	{
		Out << R"(
  float height = GetTexel(TexNum, Texture7, ptexCoords).r;
  return ptexCoords - viewDir.xy * (height * 0.1);
}
)";
	}
	else if (GL->ParallaxVersion == Parallax_Occlusion) // parallax occlusion mapping
	{
		if (!GL->UsingShaderDrawParameters)
		{
			Out << "  float vParallaxScale = TexCoords[" << UXOpenGLRenderDevice::DCDD_HEIGHTMAP_INFO << "].z * 0.025; // arbitrary to get DrawScale into (for this purpose) sane regions." END_LINE;
			Out << "  float vTimeSeconds = TexCoords[" << UXOpenGLRenderDevice::DCDD_HEIGHTMAP_INFO << "].w; // Surface.Level->TimeSeconds" END_LINE;
		}
		//vParallaxScale += 8.0f * sin(vTimeSeconds) + 4.0 * cos(2.3f * vTimeSeconds);
		// number of depth layers
		constexpr FLOAT minLayers = 8.f;
		constexpr FLOAT maxLayers = 32.f;
		Out << "  float numLayers = mix(" << maxLayers << ", " << minLayers << ", abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));" END_LINE;
		Out << R"(
  float layerDepth = 1.0 / numLayers; // calculate the size of each layer
  float currentLayerDepth = 0.0; // depth of current layer

  // the amount to shift the texture coordinates per layer (from vector P)
  vec2 P = viewDir.xy / viewDir.z * vParallaxScale;
  vec2 deltaTexCoords = P / numLayers;

  // get initial values
  vec2  currentTexCoords = ptexCoords;
  float currentDepthMapValue = 0.0;
  currentDepthMapValue = GetTexel(TexNum, Texture7, currentTexCoords).r;

  while (currentLayerDepth < currentDepthMapValue)
  {
    currentTexCoords -= deltaTexCoords; // shift texture coordinates along direction of P
    currentDepthMapValue = GetTexel(TexNum, Texture7, currentTexCoords).r; // get depthmap value at current texture coordinates
    currentLayerDepth += layerDepth; // get depth of next layer
  }

  vec2 prevTexCoords = currentTexCoords + deltaTexCoords; // get texture coordinates before collision (reverse operations)

  // get depth after and before collision for linear interpolation
  float afterDepth = currentDepthMapValue - currentLayerDepth;
  float beforeDepth = GetTexel(TexNum, Texture7, currentTexCoords).r - currentLayerDepth + layerDepth;

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

		if (!GL->UsingShaderDrawParameters)
		{
			Out << "  float vParallaxScale = TexCoords[" << UXOpenGLRenderDevice::DCDD_HEIGHTMAP_INFO << "].z * 0.025; // arbitrary to get DrawScale into (for this purpose) sane regions." END_LINE;
			Out << "  float vTimeSeconds = TexCoords[" << UXOpenGLRenderDevice::DCDD_HEIGHTMAP_INFO << "].w; // Surface.Level->TimeSeconds" END_LINE;
		}
		
		Out << R"(
  float layerHeight = 1.0 / numLayers; // height of each layer
  float currentLayerHeight = 0.0; // depth of current layer
  vec2 dtex = vParallaxScale * viewDir.xy / viewDir.z / numLayers; // shift of texture coordinates for each iteration
  vec2 currentTexCoords = ptexCoords; // current texture coordinates
  float heightFromTexture = GetTexel(TexNum, Texture7, currentTexCoords).r;" // depth from heightmap

  // while point is above surface
  while (heightFromTexture > currentLayerHeight)
  {
    currentLayerHeight += layerHeight; // go to the next layer
    currentTexCoords -= dtex; // shift texture coordinates along V
    heightFromTexture = GetTexel(TexNum, Texture7, currentTexCoords).r; // new depth from heightmap
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
    heightFromTexture = GetTexel(TexNum, Texture7, currentTexCoords).r;

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
# if 0
	Out << R"(
float parallaxSoftShadowMultiplier(in vec3 L, in vec2 initialTexCoord, in float initialHeight)
{
  float shadowMultiplier = 1;
)";

	constexpr FLOAT minLayers = 15;
	constexpr FLOAT maxLayers = 30;
	if (!GL->UsingShaderDrawParameters)
		Out << "float vParallaxScale = TexCoords[" << UXOpenGLRenderDevice::DCDD_MACRO_INFO << "].w * 0.025; // arbitrary to get DrawScale into (for this purpose) sane regions." END_LINE;

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
  float heightFromTexture = GetTexel(vMacroTexNum, Texture7, currentTexCoords).r;

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
    heightFromTexture = GetTexel(vMacroTexNum, Texture7, currentTexCoords).r;
  }

  // Shadowing factor should be 1 if there were no points under the surface
  if(numSamplesUnderSurface < 1)
    shadowMultiplier = 1;
  else
    shadowMultiplier = 1.0 - shadowMultiplier;

  return shadowMultiplier;
}
)";
	
# endif // 0
#endif // ENGINE_VERSION==227

#if 1
	Out << R"(
void main(void)
{
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);

  vec4 TotalColor = vec4(1.0);
  vec2 texCoords = vTexCoords;
)";

	if (!GL->UsingShaderDrawParameters)
	{
		Out << R"(
  uint vDrawFlags = DrawFlags[0];
  uint vTextureFormat = DrawFlags[1];
  uint vPolyFlags = DrawFlags[2];
  uint vRendMap = DrawFlags[3];
  bool bHitTesting = bool(TexNum[12]);
  float vBaseDiffuse = TexCoords[)" << UXOpenGLRenderDevice::DCDD_DIFFUSE_INFO << R"(].x;
  float vBaseAlpha = TexCoords[)" << UXOpenGLRenderDevice::DCDD_DIFFUSE_INFO << R"(].z;
  float vGamma = TexCoords[)" << UXOpenGLRenderDevice::DCDD_Z_AXIS << R"(].w;
  vec4 vDrawColor = TexCoords[)" << UXOpenGLRenderDevice::DCDD_EDITOR_DRAWCOLOR << R"(];
  vec4 vBumpMapInfo = TexCoords[)" << UXOpenGLRenderDevice::DCDD_BUMPMAP_INFO << R"(];
  vec4 vDistanceFogColor = TexCoords[)" << UXOpenGLRenderDevice::DCDD_DISTANCE_FOG_COLOR << R"(];
  vec4 vDistanceFogInfo = TexCoords[)" << UXOpenGLRenderDevice::DCDD_DISTANCE_FOG_INFO << R"(];
  vec4 vHeightMapInfo = TexCoords[)" << UXOpenGLRenderDevice::DCDD_HEIGHTMAP_INFO << R"(];
)";
		
		if (GL->UsingBindlessTextures)
		{
			Out << R"(
  uint vTexNum = TexNum[0];
  uint vLightMapTexNum = TexNum[1];
  uint vFogMapTexNum = TexNum[2];
  uint vDetailTexNum = TexNum[3];
  uint vMacroTexNum = TexNum[4];
  uint vBumpMapTexNum = TexNum[5];
  uint vEnviroMapTexNum = TexNum[6];
  uint vHeightMapTexNum = TexNum[7];
)";
		}
		else
		{
			Out << R"(
  uint vTexNum = 0u;
  uint vLightMapTexNum = 0u;
  uint vFogMapTexNum = 0u;
  uint vDetailTexNum = 0u;
  uint vMacroTexNum = 0u;
  uint vBumpMapTexNum = 0u;
  uint vEnviroMapTexNum = 0u;
  uint vHeightMapTexNum = 0u;
)";
		}
	}
	else if (GIsEditor)
		Out << "  bool bHitTesting = bool(vHitTesting);" END_LINE;

	if (GL->UseHWLighting || GL->BumpMaps)
	{
		Out << R"(
  vec3 TangentViewDir = normalize(vTangentViewPos - vTangentFragPos);
  int NumLights = int(LightData4[0].y);
)";

		if (!GL->UsingShaderDrawParameters)
			Out << "  vBumpMapSpecular = TexCoords[" << UXOpenGLRenderDevice::DCDD_BUMPMAP_INFO << "].y;" END_LINE;

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
    texCoords = ParallaxMapping(vTexCoords, TangentViewDir, vHeightMapTexNum, parallaxHeight);
    //if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
    //discard; // texCoords = vTexCoords;
  }
)";
		}
	}

	Out << R"(
  vec4 Color = GetTexel(vTexNum, Texture0, texCoords);

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
		constexpr FLOAT MinLight = 0.05;
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
    LightColor = GetTexel(vLightMapTexNum, Texture1, vLightMapCoords);
    // Fetch lightmap texel. Data in LightMap is in 0..127/255 range, which needs to be scaled to 0..2 range.
    LightColor.)" << LightColorOrder << R"( = LightColor.)" << LightColorOrder << R"( * (2.0 * 255.0 / 127.0);
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
        DetailTexColor = GetTexel(vDetailTexNum, Texture3, vDetailTexCoords * DetailScale);
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
    vec4 MacrotexColor = GetTexel(vMacroTexNum, Texture4, vMacroTexCoords); 
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
		constexpr float MinLight = 0.05;
#if ENGINE_VERSION == 227
		FString Sunlight = FString::Printf(TEXT("bool bSunlight = (uint(LightData2[i].x == %du));"), LE_Sunlight);
#else
		FString Sunlight = TEXT("bool bSunlight = false;");
#endif

		Out << R"(
  if ((vDrawFlags & )" << DF_BumpMap << R"(u) == )" << DF_BumpMap << R"(u)
  {
    //normal from normal map
    vec3 TextureNormal = normalize(GetTexel(vBumpMapTexNum, Texture5, texCoords).rgb * 2.0 - 1.0); // has to be texCoords instead of vBumpTexCoords, otherwise alignment won't work on bumps.
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
    FogColor = GetTexel(vFogMapTexNum, Texture2, vFogMapCoords) * 2.0;

  // EnvironmentMap
)";

#if ENGINE_VERSION==227
	Out << R"(
  if ((vDrawFlags & )" << DF_EnvironmentMap << R"(u) == )" << DF_EnvironmentMap << R"(u)
  {
    vec4 EnvironmentColor = GetTexel(vEnviroMapTexNum, Texture6, vEnvironmentTexCoords);
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
  {
    // Gamma
    float InGamma = 1.0 / (vGamma * )" << (GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier) << R"();
    TotalColor.r = pow(TotalColor.r, InGamma);
    TotalColor.g = pow(TotalColor.g, InGamma);
    TotalColor.b = pow(TotalColor.b, InGamma);

    TotalColor = TotalColor * LightColor;
  }

  TotalColor += FogColor;

  // Add DistanceFog, needs to be added after Light has been applied.
)";
	
#if ENGINE_VERSION==227
	// stijn: Very slow! Went from 135 to 155FPS on CTF-BT-CallousV3 by just disabling this branch even tho 469 doesn't do distance fog
	Out << R"(
  int FogMode = int(vDistanceFogInfo.w);
  if (FogMode >= 0)
  {
    FogParameters DistanceFogParams;
    DistanceFogParams.FogStart = vDistanceFogInfo.x;
    DistanceFogParams.FogEnd = vDistanceFogInfo.y;
    DistanceFogParams.FogDensity = vDistanceFogInfo.z;
    DistanceFogParams.FogMode = FogMode;

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
		
#if ENGINE_VERSION==227
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
		
#if ENGINE_VERSION==227
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
  if (bHitTesting)
    TotalColor = vDrawColor; // Use ONLY DrawColor.
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

	Out << "}" END_LINE;
#else
	Out << R"(
void main(void)
{
  //FragColor = GetTexel(TexNum[0], Texture0, vTexCoords);   
  FragColor = texture(sampler2D(Textures[TexNum[0]]), vTexCoords);
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

// Vertexshader for DrawComplexSurface, single pass.
static void Create_DrawComplexSinglePass_Vert(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
layout(location = 0) in vec4 Coords; // == gl_Vertex
layout(location = 1) in vec4 Normal; // == gl_Vertex

out vec3 vCoords;
out vec4 vEyeSpacePos;
out vec2 vTexCoords;
out vec2 vLightMapCoords;
out vec2 vFogMapCoords;
out vec3 vTangentViewPos;
out vec3 vTangentFragPos;
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
	if (GIsEditor)
		Out << "flat out vec3 vSurfaceNormal;" END_LINE; // f.e. Render normal view.
#if ENGINE_VERSION!=227
	if (GL->BumpMaps)
#endif
	{
		Out << "flat out mat3 vTBNMat;" END_LINE;
	}

	if (GL->UsingShaderDrawParameters)
	{
		Out << R"(
struct DrawComplexShaderDrawParams
{
  vec4 DiffuseUV;        // 0
  vec4 LightMapUV;       // 1
  vec4 FogMapUV;         // 2
  vec4 DetailUV;         // 3
  vec4 MacroUV;          // 4
  vec4 EnviroMapUV;      // 5
  vec4 DiffuseInfo;      // 6
  vec4 MacroInfo;        // 7
  vec4 BumpMapInfo;      // 8
  vec4 HeightMapInfo;    // 9
  vec4 XAxis;            // 10
  vec4 YAxis;            // 11
  vec4 ZAxis;            // 12
  vec4 DrawColor;        // 13
  vec4 DistanceFogColor; // 14
  vec4 DistanceFogInfo;  // 15
  uvec4 TexNum[4];
  uvec4 DrawFlags;
};

layout(std430, binding = 6) buffer AllDrawComplexShaderDrawParams
{
  DrawComplexShaderDrawParams DrawComplexParams[];
};

flat out uint vTexNum;
flat out uint vLightMapTexNum;
flat out uint vFogMapTexNum;
flat out uint vDetailTexNum;
flat out uint vMacroTexNum;
flat out uint vBumpMapTexNum;
flat out uint vEnviroMapTexNum;
flat out uint vHeightMapTexNum;
flat out uint vHeightMapNum;
flat out uint vDrawFlags;
flat out uint vTextureFormat;
flat out uint vPolyFlags;
flat out float vBaseDiffuse;
flat out float vBaseAlpha;
flat out float vParallaxScale;
flat out float vGamma;
flat out float vBumpMapSpecular;
flat out float vTimeSeconds;
)";

		if (GIsEditor)
		{
			Out << R"(
flat out uint vHitTesting;
flat out uint vRendMap;
flat out vec4 vDrawColor;
)";
		}
		Out << R"(
flat out vec4 vDistanceFogColor;
flat out vec4 vDistanceFogInfo;
)";
	}
	else
	{
		// No shader draw params support
		Out << R"(
uniform vec4 TexCoords[16];
uniform uint TexNum[16];
uniform uint DrawFlags[4];
)";
	}
	
	if (GL->SupportsClipDistance)
		Out << "out float gl_ClipDistance[" << GL->MaxClippingPlanes << "];" END_LINE;

#if 1
	Out << R"(
void main(void)
{
)";
	
	if (GL->UsingShaderDrawParameters)
	{
		Out << R"(
  vec4 XAxis = DrawComplexParams[gl_DrawID].XAxis;
  vec4 YAxis = DrawComplexParams[gl_DrawID].YAxis;
  vec4 ZAxis = DrawComplexParams[gl_DrawID].ZAxis;
  vec4 DiffuseUV = DrawComplexParams[gl_DrawID].DiffuseUV;
  vec4 LightMapUV = DrawComplexParams[gl_DrawID].LightMapUV;
  vec4 FogMapUV = DrawComplexParams[gl_DrawID].FogMapUV;
  vec4 DetailUV = DrawComplexParams[gl_DrawID].DetailUV;
  vec4 MacroUV = DrawComplexParams[gl_DrawID].MacroUV;
  vec4 EnviroMapUV = DrawComplexParams[gl_DrawID].EnviroMapUV;

  vDrawFlags = DrawComplexParams[gl_DrawID].DrawFlags[0];
  vTextureFormat = DrawComplexParams[gl_DrawID].DrawFlags[1];
  vPolyFlags = DrawComplexParams[gl_DrawID].DrawFlags[2];
)";

		if (GIsEditor)
		{
			Out << R"(
  vRendMap = DrawComplexParams[gl_DrawID].DrawFlags[3];
  vHitTesting = DrawComplexParams[gl_DrawID].TexNum[3].x;
  vDrawColor = DrawComplexParams[gl_DrawID].DrawColor;
)";
		}

		Out << R"(
  vTexNum = DrawComplexParams[gl_DrawID].TexNum[0].x;
  vLightMapTexNum = DrawComplexParams[gl_DrawID].TexNum[0].y;
  vFogMapTexNum = DrawComplexParams[gl_DrawID].TexNum[0].z;
  vDetailTexNum = DrawComplexParams[gl_DrawID].TexNum[0].w;
  vMacroTexNum = DrawComplexParams[gl_DrawID].TexNum[1].x;
  vBumpMapTexNum = DrawComplexParams[gl_DrawID].TexNum[1].y;
  vEnviroMapTexNum = DrawComplexParams[gl_DrawID].TexNum[1].z;
  vHeightMapTexNum = DrawComplexParams[gl_DrawID].TexNum[1].w;
  vBaseDiffuse = DrawComplexParams[gl_DrawID].DiffuseInfo.x;
  vBaseAlpha = DrawComplexParams[gl_DrawID].DiffuseInfo.z;
  vBumpMapSpecular = DrawComplexParams[gl_DrawID].BumpMapInfo.y;
  vParallaxScale = DrawComplexParams[gl_DrawID].HeightMapInfo.z * 0.025;
  vTimeSeconds = DrawComplexParams[gl_DrawID].HeightMapInfo.w;
  vGamma = ZAxis.w;
  vDistanceFogColor = DrawComplexParams[gl_DrawID].DistanceFogColor;
  vDistanceFogInfo = DrawComplexParams[gl_DrawID].DistanceFogInfo;
)";
	}
	else
	{
		Out << "  vec4 XAxis = TexCoords[" << UXOpenGLRenderDevice::DCDD_X_AXIS << "];" END_LINE;
		Out << "  vec4 YAxis = TexCoords[" << UXOpenGLRenderDevice::DCDD_Y_AXIS << "];" END_LINE;
		Out << "  vec4 ZAxis = TexCoords[" << UXOpenGLRenderDevice::DCDD_Z_AXIS << "];" END_LINE;
		Out << "  vec4 DiffuseUV = TexCoords[" << UXOpenGLRenderDevice::DCDD_DIFFUSE_COORDS << "];" END_LINE;
		Out << "  vec4 LightMapUV = TexCoords[" << UXOpenGLRenderDevice::DCDD_LIGHTMAP_COORDS << "];" END_LINE;
		Out << "  vec4 FogMapUV = TexCoords[" << UXOpenGLRenderDevice::DCDD_FOGMAP_COORDS << "];" END_LINE;
		Out << "  vec4 DetailUV = TexCoords[" << UXOpenGLRenderDevice::DCDD_DETAIL_COORDS << "];" END_LINE;
		Out << "  vec4 MacroUV = TexCoords[" << UXOpenGLRenderDevice::DCDD_MACRO_COORDS << "];" END_LINE;
		Out << "  vec4 EnviroMapUV = TexCoords[" << UXOpenGLRenderDevice::DCDD_ENVIROMAP_COORDS << "];" END_LINE;
		Out << "  uint vDrawFlags = DrawFlags[0];" END_LINE;
	}

	Out << R"(
  // Point Coords
  vCoords = Coords.xyz;

  // UDot/VDot calculation.
  vec3 MapCoordsXAxis = XAxis.xyz;
  vec3 MapCoordsYAxis = YAxis.xyz;
)";

#if ENGINE_VERSION!=227
	if (GIsEditor || GL->BumpMaps)
#endif
	{
		Out << "  vec3 MapCoordsZAxis = ZAxis.xyz;" END_LINE;
	}

	Out << R"(
  float UDot = XAxis.w;
  float VDot = YAxis.w;
  
  float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
  float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
  vec2  MapDot = vec2(MapDotU, MapDotV);

  // Texture UV to fragment
  vec2 TexMapMult = DiffuseUV.xy;
  vec2 TexMapPan = DiffuseUV.zw;
  vTexCoords = (MapDot - TexMapPan) * TexMapMult;

  // Texture UV Lightmap to fragment
  if ((vDrawFlags & )" << DF_LightMap << R"(u) == )" << DF_LightMap << R"(u)
  {
    vec2 LightMapMult = LightMapUV.xy;
    vec2 LightMapPan = LightMapUV.zw;
    vLightMapCoords = (MapDot - LightMapPan) * LightMapMult;
  }

  // Texture UV FogMap
  if ((vDrawFlags & )" << DF_FogMap << R"(u) == )" << DF_FogMap << R"(u)
  {
    vec2 FogMapMult = FogMapUV.xy;
    vec2 FogMapPan = FogMapUV.zw;
    vFogMapCoords = (MapDot - FogMapPan) * FogMapMult;
  }
)";
	
	// Texture UV DetailTexture
	if (GL->DetailTextures)
	{
		Out << R"(
  if ((vDrawFlags & )" << DF_DetailTexture << R"(u) == )" << DF_DetailTexture << R"(u)
  {
    vec2 DetailMult = DetailUV.xy;
    vec2 DetailPan = DetailUV.zw;
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
    vec2 MacroMult = MacroUV.xy;
    vec2 MacroPan = MacroUV.zw;
    vMacroTexCoords = (MapDot - MacroPan) * MacroMult;
  }
)";
	}

	// Texture UV EnvironmentMap
#if ENGINE_VERSION==227
	Out << R"(
  if ((vDrawFlags & )" << DF_EnvironmentMap << R"(u) == )" << DF_EnvironmentMap << R"(u)
  {
    vec2 EnvironmentMapMult = EnviroMapUV.xy;
    vec2 EnvironmentMapPan = EnviroMapUV.zw;
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
  vEyeSpacePos = modelviewMat * vec4(Coords.xyz, 1.0);"));

  // Point Coords
  vCoords = Coords.xyz;
  vTexCoords = vec2(0.0, 0.0);
#if BINDLESSTEXTURES
  // BaseTexNum = uint(TexNums0.x);
#endif
  gl_Position = modelviewprojMat * vec4(Coords.xyz, 1.0);
}
)";
	
#endif
}

// Fragmentshader for DrawGouraudPolygon, in 227 also DrawGouraudPolygonList.
static void Create_DrawGouraud_Frag(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
uniform sampler2D Texture0; // Base Texture
uniform sampler2D Texture1; // DetailTexture
uniform sampler2D Texture2; // BumpMap
uniform sampler2D Texture3; // MacroTex

in vec3 gCoords;
in vec2 gTexCoords; // TexCoords
in vec2 gDetailTexCoords;
in vec2 gMacroTexCoords;
in vec4 gNormals;
in vec4 gEyeSpacePos;
in vec4 gLightColor;
in vec4 gFogColor; //VertexFog

flat in vec3 gTextureInfo; // diffuse, alpha, bumpmap specular
flat in uint gTexNum;
flat in uint gDetailTexNum;
flat in uint gBumpTexNum;
flat in uint gMacroTexNum;
flat in uint gDrawFlags;
flat in uint gTextureFormat;
flat in uint gPolyFlags;
flat in float gGamma;
flat in vec4 gDistanceFogColor;
flat in vec4 gDistanceFogInfo;
in vec3 gTangentViewPos;
in vec3 gTangentFragPos;
in mat3 gTBNMat;
)";

	if (GIsEditor)
	{
		Out << R"(
flat in vec4 gDrawColor;
flat in uint gRendMap;
flat in uint gHitTesting;
)";
	}
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
void main(void)
{
  mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz); // TransformPointBy...
  mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);
  vec4 TotalColor = vec4(0.0, 0.0, 0.0, 0.0);

  int NumLights = int(LightData4[0].y);
  vec4 Color = GetTexel(uint(gTexNum), Texture0, gTexCoords);

  if (gTextureInfo.x > 0.0)
    Color *= gTextureInfo.x; // Diffuse factor.
  if (gTextureInfo.y > 0.0)
    Color.a *= gTextureInfo.y; // Alpha.
  if (gTextureFormat == )" << TEXF_BC5 << R"(u) // BC5 (GL_COMPRESSED_RG_RGTC2) compression
    Color.b = sqrt(1.0 - Color.r * Color.r + Color.g * Color.g);

  if ((gPolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u)
  {
    Color.a *= gLightColor.a; // Alpha.
    if (Color.a < 0.01)
      discard;
  }
  // Handle PF_Masked.
  else if ((gPolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
  {
    if (Color.a < 0.5)
      discard;
    else Color.rgb /= Color.a;
  }

  vec4 LightColor;
)";

	if (GL->UseHWLighting)
	{
		constexpr FLOAT MinLight = 0.025;
		Out << R"(
  float LightAdd = 0.0f;
  vec4 TotalAdd = vec4(0.0, 0.0, 0.0, 0.0);

  for (int i = 0; i < NumLights; i++)
  {
    float WorldLightRadius = LightData4[i].x;
    float LightRadius = LightData2[i].w;
    float RWorldLightRadius = WorldLightRadius * WorldLightRadius;

    vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords); // Frame->Coords.
    float dist = distance(gCoords, InLightPos);

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
	else Out << "  LightColor = gLightColor;" END_LINE;

	Out << R"(
  // Handle PF_RenderFog.
  if ((gPolyFlags & )" << PF_RenderFog << R"(u) == )" << PF_RenderFog << R"(u)
  {
    // Handle PF_RenderFog|PF_Modulated.
    if ((gPolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u)
    {
      // Compute delta to modulation identity.
      vec3 Delta = vec3(0.5) - Color.xyz;
      // Reduce delta by (squared) fog intensity.
      //Delta *= 1.0 - sqrt(gFogColor.a);
      Delta *= 1.0 - gFogColor.a;
      Delta *= vec3(1.0) - gFogColor.rgb;

      TotalColor = vec4(vec3(0.5) - Delta, Color.a);
    }
    else // Normal.
    {
      Color *= LightColor;
      //TotalColor=mix(Color, vec4(gFogColor.xyz,1.0), gFogColor.w);
      TotalColor.rgb = Color.rgb * (1.0 - gFogColor.a) + gFogColor.rgb;
      TotalColor.a = Color.a;
	}
  }
  else if ((gPolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u) // No Fog.
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
  if ((gDrawFlags & )" << DF_DetailTexture << R"(u) == )" << DF_DetailTexture << R"(u)
  {
    float NearZ = gCoords.z / 512.0;
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
        DetailTexColor = GetTexel(gDetailTexNum, Texture1, gDetailTexCoords * DetailScale);

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
  if ((gDrawFlags & )" << DF_MacroTexture << R"(u) == )" << DF_MacroTexture << R"(u && (gDrawFlags & )" << DF_BumpMap << R"(u) != )" << DF_BumpMap << R"(u)
  {
    vec4 MacroTexColor = GetTexel(gMacroTexNum, Texture3, gMacroTexCoords);
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
		constexpr FLOAT MinLight = 0.05;
#if ENGINE_VERSION == 227
		FString Sunlight = FString::Printf(TEXT("bool bSunlight = (uint(LightData2[i].x == %du));"), LE_Sunlight);
#else
		FString Sunlight = TEXT("bool bSunlight = false;");
#endif

		Out << R"(
  if ((gDrawFlags & )" << DF_BumpMap << R"(u) == )" << DF_BumpMap << R"(u)
  {
    vec3 TangentViewDir = normalize(gTangentViewPos - gTangentFragPos);
    //normal from normal map
    vec3 TextureNormal = GetTexel(gBumpTexNum, Texture2, gTexCoords).rgb * 2.0 - 1.0;
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
      float dist = distance(gCoords, InLightPos);
      float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * )" << MinLight << R"();
      float attenuation = NormalLightRadius / (dist + b * dist * dist);
      
      if ((gPolyFlags & )" << PF_Unlit << R"(u) == )" << PF_Unlit << R"(u)
        InLightPos = vec3(1.0, 1.0, 1.0); // no idea whats best here. Arbitrary value based on some tests.

      if ((NormalLightRadius == 0.0 || (dist > NormalLightRadius) || (bZoneNormalLight && (LightData4[i].z != LightData4[i].w))) && !bSunlight) // Do not consider if not in range or in a different zone.
        continue;

      vec3 TangentLightPos = gTBNMat * InLightPos;
      vec3 TangentlightDir = normalize(TangentLightPos - gTangentFragPos);
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
	
	Out << R"(
  if ((gPolyFlags & )" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
  {
    // Gamma
    float InGamma = 1.0 / (gGamma * )" << (GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier) << R"();
    TotalColor.r = pow(TotalColor.r, InGamma);
    TotalColor.g = pow(TotalColor.g, InGamma);
    TotalColor.b = pow(TotalColor.b, InGamma);
  }
)";

#if ENGINE_VERSION==227
	// Add DistanceFog
	Out << R"(
  if (gDistanceFogInfo.w >= 0.0)
  {
    FogParameters DistanceFogParams;
    DistanceFogParams.FogStart = gDistanceFogInfo.x;
    DistanceFogParams.FogEnd = gDistanceFogInfo.y;
    DistanceFogParams.FogDensity = gDistanceFogInfo.z;
    DistanceFogParams.FogMode = int(gDistanceFogInfo.w);

    if ((gPolyFlags & )" << PF_Modulated << R"(u) == )" << PF_Modulated << R"(u)
      DistanceFogParams.FogColor = vec4(0.5, 0.5, 0.5, 0.0);
	else if ((gPolyFlags & )" << PF_Translucent << R"(u) == )" << PF_Translucent << R"(u && (gPolyFlags & )" << PF_Environment << R"(u) != )" << PF_Environment << R"(u)
      DistanceFogParams.FogColor = vec4(0.0, 0.0, 0.0, 0.0);
    else DistanceFogParams.FogColor = gDistanceFogColor;

    DistanceFogParams.FogCoord = abs(gEyeSpacePos.z / gEyeSpacePos.w);
    TotalColor = mix(TotalColor, DistanceFogParams.FogColor, getFogFactor(DistanceFogParams));
  }
)";
#endif

	if (GIsEditor)
	{
		// Editor support.
		Out << R"(
  if (gRendMap == )" << REN_Zones << R"(u || gRendMap == )" << REN_PolyCuts << R"(u || gRendMap == )" << REN_Polys << R"(u)
  {
    TotalColor += 0.5;
    TotalColor *= gDrawColor;
  }
)";		
#if ENGINE_VERSION==227
		Out << R"(
  else if (gRendMap == )" << REN_Normals << R"(u)
  {
    // Dot.
    float T = 0.5 * dot(normalize(gCoords), gNormals.xyz);
    // Selected.
    if ((gPolyFlags & )" << PF_Selected << R"(u) == )" << PF_Selected << R"(u)
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
  else if (gRendMap == )" << REN_PlainTex << R"(u)
  {
    TotalColor = Color;
  }
)";

#if ENGINE_VERSION==227
		Out << "  if ((gRendMap != " << REN_Normals << "u) && ((gPolyFlags & " << PF_Selected << "u) == " << PF_Selected << "u))" END_LINE;
#else
		Out << "  if ((gPolyFlags & " << PF_Selected << "u) == " << PF_Selected << "u)" END_LINE;
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
  if (bool(gHitTesting))
    TotalColor = gDrawColor; // Use DrawColor.

  if ((gPolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u && gDrawColor.a > 0.0)
    TotalColor.a *= gDrawColor.a;
)";
	}
	if (GL->SimulateMultiPass)
	{
		Out << R"(
  FragColor = TotalColor;
  FragColor1 = ((vec4(1.0) - TotalColor * LightColor)); //no, this is not entirely right, TotalColor has already LightColor applied. But will blow any fog/transparency otherwise. However should not make any (visual) difference here for this equation. Any better idea?
)";
	}
	else Out << "FragColor = TotalColor;" END_LINE;
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

// Vertexshader for DrawGouraudPolygon, in 227 also DrawGouraudPolygonList.
static void Create_DrawGouraud_Vert(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
layout(location = 0) in vec3 Coords; // == gl_Vertex
layout(location = 1) in vec3 Normals; // Normals
layout(location = 2) in vec2 TexCoords; // TexCoords
layout(location = 3) in vec4 LightColor;
layout(location = 4) in vec4 FogColor;
)";

	if (GL->OpenGLVersion == GL_ES)
	{
		// No geometry shader in GL_ES.
		Out << R"(
flat out uint gTexNum;
flat out uint gDetailTexNum;
flat out uint gBumpTexNum;
flat out uint gMacroTexNum;
flat out uint gDrawFlags;
flat out uint gTextureFormat;
flat out uint gPolyFlags;
flat out float gGamma;
flat out vec3 gTextureInfo; // diffuse, alpha, bumpmap specular
flat out vec4 gDistanceFogColor;
flat out vec4 gDistanceFogInfo;

out vec3 gCoords;
out vec4 gNormals;
out vec2 gTexCoords;
out vec2 gDetailTexCoords;
out vec2 gMacroTexCoords;
out vec4 gEyeSpacePos;
out vec4 gLightColor;
out vec4 gFogColor;
out mat3 gTBNMat;
out vec3 gTangentViewPos;
out vec3 gTangentFragPos;
)";

		if (GIsEditor)
		{
			Out << R"(
flat out vec4 gDrawColor;
flat out uint gRendMap;
flat out uint gHitTesting;
)";
		}
		Out << R"(
uniform vec4 DrawData[6];
uniform uint TexNum[4];
uniform uint DrawFlags[4];
)";
	}
	else
	{
		if (GL->UsingShaderDrawParameters)
		{
			Out << R"(
struct DrawGouraudShaderDrawParams
{
  vec4 DiffuseInfo;      // 0
  vec4 DetailMacroInfo;  // 1
  vec4 MiscInfo;         // 2
  vec4 DrawColor;        // 3
  vec4 DistanceFogColor; // 4
  vec4 DistanceFogInfo;  // 5
  uvec4 TexNum;
  uvec4 DrawFlags;
};

layout(std430, binding = 7) buffer AllDrawGouraudShaderDrawParams
{
  DrawGouraudShaderDrawParams DrawGouraudParams[];
};
)";
		}
		else
		{
			Out << R"(
uniform vec4 DrawData[6];
uniform uint TexNum[4];
uniform uint DrawFlags[4];
)";
		}
		Out << R"(
flat out uint vTexNum;
flat out uint vDetailTexNum;
flat out uint vBumpTexNum;
flat out uint vMacroTexNum;
flat out uint vDrawFlags;
flat out uint vTextureFormat;
flat out uint vPolyFlags;
flat out float vGamma;

flat out vec3 vTextureInfo; // diffuse, alpha, bumpmap specular
flat out vec4 vDistanceFogColor;
flat out vec4 vDistanceFogInfo;

out vec3 vCoords;
out vec4 vNormals;
out vec2 vTexCoords;
out vec2 vDetailTexCoords;
out vec2 vMacroTexCoords;
out vec4 vEyeSpacePos;
out vec4 vLightColor;
out vec4 vFogColor;
)";

		if (GIsEditor)
		{
			Out << R"(
flat out vec4 vDrawColor;
flat out uint vRendMap;
flat out uint vHitTesting;
)";
		}
	}

	Out << R"(
void main(void)
{
)";
	if (GL->OpenGLVersion == GL_ES)
	{
		Out << R"(
  gEyeSpacePos = modelviewMat * vec4(Coords, 1.0);
  gCoords = Coords;
  gNormals = vec4(Normals.xyz, 0);
  gTexCoords = TexCoords * DrawData[)" << UXOpenGLRenderDevice::DGDD_DIFFUSE_INFO << R"(].xy; // TODO - REVIEW THESE INDICES!!!
  gDetailTexCoords = TexCoords * DrawData[)" << UXOpenGLRenderDevice::DGDD_DETAIL_MACRO_INFO << R"(%u].xy;
  gMacroTexCoords = TexCoords * DrawData[)" << UXOpenGLRenderDevice::DGDD_DETAIL_MACRO_INFO << R"(].zw;
  gLightColor = LightColor;
  gFogColor = FogColor;

  gTexNum = TexNum[0];
  gDetailTexNum = TexNum[1];
  gBumpTexNum = TexNum[2];
  gMacroTexNum = TexNum[3];

  gDrawFlags = DrawFlags[0];
  gTextureFormat = uint(DrawData[)" << UXOpenGLRenderDevice::DGDD_MISC_INFO << R"(].w);
  gPolyFlags = DrawFlags[2];
  gGamma = DrawData[)" << UXOpenGLRenderDevice::DGDD_MISC_INFO << R"(].y;

  gTextureInfo = vec3(DrawData[)" << UXOpenGLRenderDevice::DGDD_DIFFUSE_INFO << R"(].zw, DrawData[)" << UXOpenGLRenderDevice::DGDD_MISC_INFO << R"(].x);
  gDistanceFogColor = DrawData[)" << UXOpenGLRenderDevice::DGDD_DISTANCE_FOG_COLOR << R"(];
  gDistanceFogInfo = DrawData[)" << UXOpenGLRenderDevice::DGDD_DISTANCE_FOG_INFO << R"(];
)";
		
		if (GIsEditor)
		{
			Out << R"(
  gHitTesting = DrawFlags[1];
  gRendMap = DrawFlags[3];
  gDrawColor = DrawData[)" << UXOpenGLRenderDevice::DGDD_EDITOR_DRAWCOLOR << R"(];
)";
		}

		Out << R"(
  vec3 T = vec3(1.0, 1.0, 1.0); // Arbitrary.
  vec3 B = vec3(1.0, 1.0, 1.0); // Replace with actual values extracted from mesh generation some day.
  vec3 N = normalize(Normals.xyz); // Normals.

  // TBN must have right handed coord system.
  //if (dot(cross(N, T), B) < 0.0)
  // T = T * -1.0;

  gTBNMat = transpose(mat3(T, B, N));
  gTangentViewPos = gTBNMat * normalize(FrameCoords[0].xyz);
  gTangentFragPos = gTBNMat * gCoords.xyz;

  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
)";
		
		if (GL->SupportsClipDistance)
		{
			Out << R"(
  uint ClipIndex = uint(ClipParams.x);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, gEyeSpacePos.xyz);
)";
		}
	}
	else
	{
		Out << R"(
  vEyeSpacePos = modelviewMat * vec4(Coords, 1.0);

  vCoords = Coords;
  vNormals = vec4(Normals.xyz, 0);
  vLightColor = LightColor;
  vFogColor = FogColor;
)";

		if (GL->UsingShaderDrawParameters)
		{
			Out << R"(
  vTexCoords = TexCoords * DrawGouraudParams[gl_DrawID].DiffuseInfo.xy;
  vDetailTexCoords = TexCoords * DrawGouraudParams[gl_DrawID].DetailMacroInfo.xy;
  vMacroTexCoords = TexCoords * DrawGouraudParams[gl_DrawID].DetailMacroInfo.zw;

  vTexNum = DrawGouraudParams[gl_DrawID].TexNum[0];
  vDetailTexNum = DrawGouraudParams[gl_DrawID].TexNum[1];
  vBumpTexNum = DrawGouraudParams[gl_DrawID].TexNum[2];
  vMacroTexNum = DrawGouraudParams[gl_DrawID].TexNum[3];

  vDrawFlags = DrawGouraudParams[gl_DrawID].DrawFlags[0];
  vTextureFormat = uint(DrawGouraudParams[gl_DrawID].MiscInfo.z);
  vPolyFlags = DrawGouraudParams[gl_DrawID].DrawFlags[2];
  vGamma = DrawGouraudParams[gl_DrawID].MiscInfo.y;

  vTextureInfo = vec3(DrawGouraudParams[gl_DrawID].DiffuseInfo.zw, DrawGouraudParams[gl_DrawID].MiscInfo.x);
  vDistanceFogColor = DrawGouraudParams[gl_DrawID].DistanceFogColor;
  vDistanceFogInfo = DrawGouraudParams[gl_DrawID].DistanceFogInfo;
)";

			if (GIsEditor)
			{
				Out << R"(
  vHitTesting = DrawGouraudParams[gl_DrawID].DrawFlags[1];
  vRendMap = DrawGouraudParams[gl_DrawID].DrawFlags[3];
  vDrawColor = DrawGouraudParams[gl_DrawID].DrawColor;
)";
			}
		}
		else
		{
			Out << R"(
  vTexCoords = TexCoords * DrawData[)" << UXOpenGLRenderDevice::DGDD_DIFFUSE_INFO << R"(].xy;
  vDetailTexCoords = TexCoords * DrawData[)" << UXOpenGLRenderDevice::DGDD_DETAIL_MACRO_INFO << R"(].xy;
  vMacroTexCoords = TexCoords * DrawData[)" << UXOpenGLRenderDevice::DGDD_DETAIL_MACRO_INFO << R"(].zw;

  vTexNum = TexNum[0];
  vDetailTexNum = TexNum[1];
  vBumpTexNum = TexNum[2];
  vMacroTexNum = TexNum[3];

  vDrawFlags = DrawFlags[0];
  vTextureFormat = uint(DrawData[)" << UXOpenGLRenderDevice::DGDD_MISC_INFO << R"(].z);
  vPolyFlags = DrawFlags[2];
  vGamma = DrawData[)" << UXOpenGLRenderDevice::DGDD_MISC_INFO << R"(].y;

  vTextureInfo = vec3(DrawData[)" << UXOpenGLRenderDevice::DGDD_DIFFUSE_INFO << R"(].zw, DrawData[)" << UXOpenGLRenderDevice::DGDD_MISC_INFO << R"(].x);
  vDistanceFogColor = DrawData[)" << UXOpenGLRenderDevice::DGDD_DISTANCE_FOG_COLOR << R"(];
  vDistanceFogInfo = DrawData[)" << UXOpenGLRenderDevice::DGDD_DISTANCE_FOG_INFO << R"(];
)";
			
			if (GIsEditor)
			{
				Out << R"(
  vHitTesting = DrawFlags[1];
  vRendMap = DrawFlags[3];
  vDrawColor = DrawData[)" << UXOpenGLRenderDevice::DGDD_EDITOR_DRAWCOLOR << R"(];
)";
			}
		}
	}

	Out << R"(
  gl_Position = vec4(Coords, 1.0);
}
)";
}

// Geometryshader for DrawGouraud.
static void Create_DrawGouraud_Geo(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

flat in uint vTexNum[];
flat in uint vDetailTexNum[];
flat in uint vBumpTexNum[];
flat in uint vMacroTexNum[];
flat in uint vDrawFlags[];
flat in uint vTextureFormat[];
flat in uint vPolyFlags[];
flat in float vGamma[];

flat in vec3 vTextureInfo[]; // diffuse, alpha, bumpmap specular
flat in vec4 vDistanceFogColor[];
flat in vec4 vDistanceFogInfo[];
)";

	if (GIsEditor)
	{
		Out << R"(
flat in vec4 vDrawColor[];
flat in uint vRendMap[];
flat in uint vHitTesting[];
)";
	}

	Out << R"(
in vec3 vCoords[];
in vec4 vNormals[];
in vec2 vTexCoords[];
in vec2 vDetailTexCoords[];
in vec2 vMacroTexCoords[];
in vec4 vEyeSpacePos[];
in vec4 vLightColor[];
in vec4 vFogColor[];

flat out uint gTexNum;
flat out uint gDetailTexNum;
flat out uint gBumpTexNum;
flat out uint gMacroTexNum;
flat out uint gDrawFlags;
flat out uint gTextureFormat;
flat out uint gPolyFlags;
flat out float gGamma;

out vec3 gCoords;
out vec4 gNormals;
out vec2 gTexCoords;
out vec2 gDetailTexCoords;
out vec2 gMacroTexCoords;
out vec4 gEyeSpacePos;
out vec4 gLightColor;
out vec4 gFogColor;

out mat3 gTBNMat;
out vec3 gTangentViewPos;
out vec3 gTangentFragPos;

flat out vec3 gTextureInfo;
flat out vec4 gDistanceFogColor;
flat out vec4 gDistanceFogInfo;
)";
	
	if (GIsEditor)
	{
		Out << R"(
flat out vec4 gDrawColor;
flat out uint gRendMap;
flat out uint gHitTesting;
)";
	}
	
	Out << "out float gl_ClipDistance[" << GL->MaxClippingPlanes << "];" END_LINE;

	Out << R"(
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

  vec3 Tangent = GetTangent(vCoords[0], vCoords[1], vCoords[2], vTexCoords[0], vTexCoords[1], vTexCoords[2]);
  vec3 Bitangent = GetBitangent(vCoords[0], vCoords[1], vCoords[2], vTexCoords[0], vTexCoords[1], vTexCoords[2]);
  uint ClipIndex = uint(ClipParams.x);

  for (int i = 0; i < 3; ++i)
  {
    vec3 T = normalize(vec3(modelMat * vec4(Tangent, 0.0)));
    vec3 B = normalize(vec3(modelMat * vec4(Bitangent, 0.0)));
    vec3 N = normalize(vec3(modelMat * vNormals[i]));

    // TBN must have right handed coord system.
    // if (dot(cross(N, T), B) < 0.0)
    // T = T * -1.0;
    gTBNMat = mat3(T, B, N);

    gEyeSpacePos = vEyeSpacePos[i];
    gLightColor = vLightColor[i];
    gFogColor = vFogColor[i];
    gNormals = vNormals[i];
    gTexCoords = vTexCoords[i];
    gDetailTexCoords = vDetailTexCoords[i];
    gMacroTexCoords = vMacroTexCoords[i];
    gCoords = vCoords[i];
    gTexNum = vTexNum[i];
    gDetailTexNum = vDetailTexNum[i];
    gBumpTexNum = vBumpTexNum[i];
    gMacroTexNum = vMacroTexNum[i];
    gTextureInfo = vTextureInfo[i];
    gDrawFlags = vDrawFlags[i];
    gPolyFlags = vPolyFlags[i];
    gGamma = vGamma[i];
    gTextureFormat = vTextureFormat[i];
    gDistanceFogColor = vDistanceFogColor[i];
    gDistanceFogInfo = vDistanceFogInfo[i];

    gTangentViewPos = gTBNMat * normalize(FrameCoords[0].xyz);
    gTangentFragPos = gTBNMat * gCoords.xyz;
)";
	
	if (GIsEditor)
	{
		Out << R"(
    gDrawColor = vDrawColor[i];
    gRendMap = vRendMap[i];
    gHitTesting = vHitTesting[i];
)";
	}
	
	Out << R"(
    gl_Position = modelviewprojMat * gl_in[i].gl_Position;
    gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[i]);
    EmitVertex();
  }
}
)";
}

// Fragmentshader for DrawTile.
static void Create_DrawTile_Frag(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
uniform uint PolyFlags;
uniform bool bHitTesting;
uniform float Gamma;
uniform vec4 HitDrawColor;

in vec2 gTexCoords;
flat in vec4 gDrawColor;
flat in uint gTexNum;

uniform sampler2D Texture0;
uniform uint TexNum;
)";

	if (GL->OpenGLVersion == GL_ES)
	{
		Out << "layout(location = 0) out vec4 FragColor;" END_LINE;
		if (GL->SimulateMultiPass)
			Out << "layout(location = 1) out vec4 FragColor1;" END_LINE;
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out << "layout(location = 0, index = 1) out vec4 FragColor1;" END_LINE ;
		Out << "layout(location = 0, index = 0) out vec4 FragColor;" END_LINE;
	}
    Out << R"(
void main(void)
{
  vec4 TotalColor;
  vec4 Color = GetTexel(TexNum, Texture0, gTexCoords);

  // Handle PF_Masked.
  if ((PolyFlags & )" << PF_Masked << R"(u) == )" << PF_Masked << R"(u)
  {
    if (Color.a < 0.5)
      discard;
    else Color.rgb /= Color.a;
  }
  else if (((PolyFlags & )" << PF_AlphaBlend << R"(u) == )" << PF_AlphaBlend << R"(u) && Color.a < 0.01)
    discard;

  TotalColor = Color * gDrawColor; // Add DrawColor.

  if ((PolyFlags & )" << PF_Modulated << R"(u) != )" << PF_Modulated << R"(u)
  {
    float InGamma = 1.0 / (Gamma * )" << (GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier) << R"();
	
    TotalColor.r = pow(TotalColor.r, InGamma);
    TotalColor.g = pow(TotalColor.g, InGamma);
    TotalColor.b = pow(TotalColor.b, InGamma);
  }
)";

	if (GIsEditor)
	{
		// Editor support.
		Out << R"(
  if ((PolyFlags & )" << PF_Selected << R"(u) == )" << PF_Selected << R"(u)
  {
    TotalColor.g = TotalColor.g - 0.04;
    TotalColor = clamp(TotalColor, 0.0, 1.0);
  }

  // HitSelection, Zoneview etc.
  if (bHitTesting)
    TotalColor = HitDrawColor; // Use HitDrawColor.

)";		
	}

	if (GL->SimulateMultiPass)
	{
		Out << R"(
  FragColor = TotalColor;
  FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;
)";
	}
	else Out << "FragColor = TotalColor;" END_LINE;
    Out << "}" END_LINE;
}

// Geoshader for DrawTile.
static void Create_DrawTile_Geo(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

flat in uint vTexNum[];
in vec4 vTexCoords0[];
in vec4 vTexCoords1[];
in vec4 vTexCoords2[];
flat in vec4 vDrawColor[];
in vec4 vEyeSpacePos[];
in vec3 vCoords[];

out vec2 gTexCoords;
flat out vec4 gDrawColor;
flat out uint gTexNum;

out float gl_ClipDistance[)" << GL->MaxClippingPlanes << R"(];

void main()
{
  uint ClipIndex = uint(ClipParams.x);
  gTexNum = vTexNum[0];

  float RFX2 = vTexCoords0[0].x;
  float RFY2 = vTexCoords0[0].y;
  float FX2 = vTexCoords0[0].z;
  float FY2 = vTexCoords0[0].w;

  float U = vTexCoords1[0].x;
  float V = vTexCoords1[0].y;
  float UL = vTexCoords1[0].z;
  float VL = vTexCoords1[0].w;

  float XL = vTexCoords2[0].x;
  float YL = vTexCoords2[0].y;
  float UMult = vTexCoords2[0].z;
  float VMult = vTexCoords2[0].w;

  float X = gl_in[0].gl_Position.x;
  float Y = gl_in[0].gl_Position.y;
  float Z = gl_in[0].gl_Position.z;

  vec3 Position;

  // 0
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  gTexCoords.x = (U)*UMult;
  gTexCoords.y = (V)*VMult;
  gDrawColor = vDrawColor[0];
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[0]);
  EmitVertex();

  // 1
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  gTexCoords.x = (U + UL) * UMult;
  gTexCoords.y = (V)*VMult;
  gDrawColor = vDrawColor[0];
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[1]);
  EmitVertex();

  // 2
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  gTexCoords.x = (U + UL) * UMult;
  gTexCoords.y = (V + VL) * VMult;
  gDrawColor = vDrawColor[0];
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[2]);
  EmitVertex();
  EndPrimitive();

  // 0
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y - FY2);
  Position.z = Z;
  gTexCoords.x = (U)*UMult;
  gTexCoords.y = (V)*VMult;
  gDrawColor = vDrawColor[0];
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[0]);
  EmitVertex();

  // 2
  Position.x = RFX2 * Z * (X + XL - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  gTexCoords.x = (U + UL) * UMult;
  gTexCoords.y = (V + VL) * VMult;
  gDrawColor = vDrawColor[0];
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[1]);
  EmitVertex();

  // 3
  Position.x = RFX2 * Z * (X - FX2);
  Position.y = RFY2 * Z * (Y + YL - FY2);
  Position.z = Z;
  gTexCoords.x = (U)*UMult;
  gTexCoords.y = (V + VL) * VMult;
  gDrawColor = vDrawColor[0];
  gl_Position = modelviewprojMat * vec4(Position, 1.0);
  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[2]);
  EmitVertex();

  EndPrimitive();
}
)";
}

// Vertexshader for DrawTile.
static void Create_DrawTile_Vert(UXOpenGLRenderDevice* GL, FShaderOutputDevice& Out)
{
	Out << R"(
layout(location = 0) in vec3 Coords; // ==gl_Vertex
)";
	
	if (GL->OpenGLVersion == GL_ES)
	{
		//No geometry shader in GL_ES.
		Out << R"(
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in vec4 DrawColor;
out vec2 gTexCoords;
flat out vec4 gDrawColor;
flat out uint gTexNum;
)";
	}
	else
	{
		Out << R"(
layout(location = 1) in vec4 TexCoords0;
layout(location = 2) in vec4 TexCoords1;
layout(location = 3) in vec4 TexCoords2;
layout(location = 4) in vec4 DrawColor;

out vec3 vCoords;
out vec4 vTexCoords0;
out vec4 vTexCoords1;
out vec4 vTexCoords2;

flat out vec4 vDrawColor;
flat out uint vTexNum;
out vec4 vEyeSpacePos;
)";
	}

	Out << R"(
// we need to pass the bindless handle in a UBO because it 
// needs to be a dynamically uniform expression on GPUs 
// that do not support NV_gpu_shader5
uniform uint TexNum;
)";


	if (GL->OpenGLVersion == GL_ES)
	{
		Out << R"(
void main(void)
{
  vec4 gEyeSpacePos = modelviewMat * vec4(Coords, 1.0);

  gTexNum = TexNum;
  gTexCoords = TexCoords;
  gDrawColor = DrawColor;

  gl_Position = modelviewprojMat * vec4(Coords, 1.0);
}
)";
	}
	else
	{
		Out << R"(
void main(void)
{
  vEyeSpacePos = modelviewMat * vec4(Coords, 1.0);

  vCoords = Coords;
  vTexNum = TexNum;

  vTexCoords0 = TexCoords0;
  vTexCoords1 = TexCoords1;
  vTexCoords2 = TexCoords2;
  vDrawColor = DrawColor;

  gl_Position = vec4(Coords, 1.0);
}
)";
	}
}


// Helpers
void UXOpenGLRenderDevice::GetUniformBlockIndex(GLuint& Program, GLuint BlockIndex, const GLuint BindingIndex, const char* Name, FString ProgramName)
{
	BlockIndex = glGetUniformBlockIndex(Program, Name);
	if (BlockIndex == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), *ProgramName);
		if (UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformBlockIndex) %ls in %ls"), appFromAnsi(Name), *ProgramName);
	}
	glUniformBlockBinding(Program, BlockIndex, BindingIndex);
}

void UXOpenGLRenderDevice::GetUniformLocation(GLuint& Uniform, GLuint& Program, const char* Name, FString ProgramName)
{
	Uniform = glGetUniformLocation(Program, Name);
	if (Uniform == GL_INVALID_INDEX)
	{
		debugf(NAME_DevGraphics, TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), *ProgramName);
		if (UseOpenGLDebug && LogLevel >= 2)
			debugf(TEXT("XOpenGL: invalid or unused shader var (UniformLocation) %ls in %ls"), appFromAnsi(Name), *ProgramName);
	}
}

static void BuildShader(const TCHAR* ShaderProgName, GLuint& ShaderProg, UXOpenGLRenderDevice* GL, glShaderProg* Func)
{
	guard(BuildShader);
	FShaderOutputDevice ShOut(ShaderProgName);
	Emit_Globals(GL, ShOut);
	(*Func)(GL, ShOut);

	GLint blen = 0;
	GLsizei slen = 0;
	GLint IsCompiled = 0;
	const GLchar* Shader = *ShOut;
	GLint length = ShOut.Length();
	glShaderSource(ShaderProg, 1, &Shader, &length);
	glCompileShader(ShaderProg);

	glGetShaderiv(ShaderProg, GL_COMPILE_STATUS, &IsCompiled);
	if (!IsCompiled)
	{		
		GWarn->Logf(TEXT("XOpenGL: Failed compiling %ls"), ShaderProgName);
		FString ShaderSource(*ShOut);
		debugf(TEXT("XOpenGL: Shader Source: %ls"), *ShaderSource);
	}

	glGetShaderiv(ShaderProg, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* compiler_log = new GLchar[blen + 1];
		glGetShaderInfoLog(ShaderProg, blen, &slen, compiler_log);
		debugf(NAME_DevGraphics, TEXT("XOpenGL: Log compiling %ls %ls"), ShaderProgName, appFromAnsi(compiler_log));
		delete[] compiler_log;
	}
	else debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: No compiler messages for %ls"), ShaderProgName);

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::LinkShader(const TCHAR* ShaderProgName, GLuint& ShaderProg)
{
	guard(UXOpenGLRenderDevice::LinkShader);
	GLint IsLinked = 0;
	GLint blen = 0;
	GLsizei slen = 0;
	glLinkProgram(ShaderProg);
	glGetProgramiv(ShaderProg, GL_LINK_STATUS, &IsLinked);

	if (!IsLinked)
		GWarn->Logf(TEXT("XOpenGL: Failed linking %ls"), ShaderProgName);

	glGetProgramiv(ShaderProg, GL_INFO_LOG_LENGTH, &blen);
	if (blen > 1)
	{
		GLchar* linker_log = new GLchar[blen + 1];
		glGetProgramInfoLog(ShaderProg, blen, &slen, linker_log);
		debugf(TEXT("XOpenGL: Log linking %ls %ls"), ShaderProgName, appFromAnsi(linker_log));
		delete[] linker_log;
	}
	else debugfSlow(NAME_DevGraphics, TEXT("XOpenGL: No linker messages for %ls"), ShaderProgName);

	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::InitShaders()
{
	guard(UXOpenGLRenderDevice::InitShaders);

	DrawSimpleVertObject = glCreateShader(GL_VERTEX_SHADER);
	//if (OpenGLVersion == GL_Core)
	//	DrawSimpleGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawSimpleFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	DrawTileVertObject = glCreateShader(GL_VERTEX_SHADER);
	if (OpenGLVersion == GL_Core)
		DrawTileGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawTileFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	// DrawComplexSurface SinglePass
	DrawComplexVertSinglePassObject = glCreateShader(GL_VERTEX_SHADER);
	DrawComplexFragSinglePassObject = glCreateShader(GL_FRAGMENT_SHADER);
	// DrawComplexGeoSinglePassObject = glCreateShader(GL_GEOMETRY_SHADER);
	CHECK_GL_ERROR();

	// DrawGouraudPolygons.
	DrawGouraudVertObject = glCreateShader(GL_VERTEX_SHADER);
	if (OpenGLVersion == GL_Core)
		DrawGouraudGeoObject = glCreateShader(GL_GEOMETRY_SHADER);
	DrawGouraudFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();

	/*
	// ShadowMaps
	SimpleDepthVertObject = glCreateShader(GL_VERTEX_SHADER);
	SimpleDepthFragObject = glCreateShader(GL_FRAGMENT_SHADER);
	CHECK_GL_ERROR();
	*/

	BuildShader(TEXT("DrawSimple.vert"), DrawSimpleVertObject, this, Create_DrawSimple_Vert);
	//if (OpenGLVersion == GL_Core)
	//	BuildShader(TEXT("DrawSimple.geo"), DrawSimpleGeoObject, this, Create_DrawSimple_Geo);
	BuildShader(TEXT("DrawSimple.frag"), DrawSimpleFragObject, this, Create_DrawSimple_Frag);

	BuildShader(TEXT("DrawTile.vert"), DrawTileVertObject, this, Create_DrawTile_Vert);
	if (OpenGLVersion == GL_Core)
		BuildShader(TEXT("DrawTile.geo"), DrawTileGeoObject, this, Create_DrawTile_Geo);
	BuildShader(TEXT("DrawTile.frag"), DrawTileFragObject, this, Create_DrawTile_Frag);

	BuildShader(TEXT("DrawGouraud.vert"), DrawGouraudVertObject, this, Create_DrawGouraud_Vert);
	if (OpenGLVersion == GL_Core)
		BuildShader(TEXT("DrawGouraud.geo"), DrawGouraudGeoObject, this, Create_DrawGouraud_Geo);
	BuildShader(TEXT("DrawGouraud.frag"), DrawGouraudFragObject, this, Create_DrawGouraud_Frag);

	BuildShader(TEXT("DrawComplexSinglePass.vert"), DrawComplexVertSinglePassObject, this, Create_DrawComplexSinglePass_Vert);
	//if (OpenGLVersion == GL_Core)
	//	BuildShader(TEXT("DrawComplexSinglePass.geo"), DrawComplexGeoSinglePassObject, this, Create_DrawComplexSinglePass_Geo);
	BuildShader(TEXT("DrawComplexSinglePass.frag"), DrawComplexFragSinglePassObject, this, Create_DrawComplexSinglePass_Frag);

	/*
	// ShadowMap
	LoadShader(TEXT("xopengl/SimpleDepth.vert"), SimpleDepthVertObject);
	LoadShader(TEXT("xopengl/SimpleDepth.frag"), SimpleDepthFragObject);
	*/

	DrawSimpleProg = glCreateProgram();
	DrawTileProg = glCreateProgram();
	DrawComplexProg = glCreateProgram();
	DrawGouraudProg = glCreateProgram();
	// SimpleDepthProg = glCreateProgram();
	CHECK_GL_ERROR();

	glAttachShader(DrawSimpleProg, DrawSimpleVertObject);
	//glAttachShader(DrawSimpleProg, DrawSimpleGeoObject);
	glAttachShader(DrawSimpleProg, DrawSimpleFragObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawTileProg, DrawTileVertObject);
	if (OpenGLVersion == GL_Core)
		glAttachShader(DrawTileProg, DrawTileGeoObject);
	glAttachShader(DrawTileProg, DrawTileFragObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawComplexProg, DrawComplexVertSinglePassObject);
	glAttachShader(DrawComplexProg, DrawComplexFragSinglePassObject);
	CHECK_GL_ERROR();

	glAttachShader(DrawGouraudProg, DrawGouraudVertObject);
	if (OpenGLVersion == GL_Core)
		glAttachShader(DrawGouraudProg, DrawGouraudGeoObject);
	glAttachShader(DrawGouraudProg, DrawGouraudFragObject);
	CHECK_GL_ERROR();

	/*
	glAttachShader(SimpleDepthProg, SimpleDepthVertObject);
	glAttachShader(SimpleDepthProg, SimpleDepthFragObject);
	CHECK_GL_ERROR();
	*/

	// Link shaders.
	LinkShader(TEXT("DrawSimpleProg"), DrawSimpleProg);
	LinkShader(TEXT("DrawTileProg"), DrawTileProg);
	LinkShader(TEXT("DrawComplexProg"), DrawComplexProg);
	LinkShader(TEXT("DrawGouraudProg"), DrawGouraudProg);
	//LinkShader(TEXT("SimpleDepthProg"), SimpleDepthProg);

	//Global Matrix setup
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("DrawGouraudProg"));
	//GetUniformBlockIndex(SimpleDepthProg, GlobalUniformBlockIndex, GlobalMatricesBindingIndex, "GlobalMatrices", TEXT("SimpleDepthProg"));
	CHECK_GL_ERROR();

	//Global ClipPlanes
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformClipPlaneIndex, GlobalClipPlaneBindingIndex, "ClipPlaneParams", TEXT("DrawGouraudProg"));
	CHECK_GL_ERROR();

	//Global texture handles for bindless.
	if (UsingBindlessTextures && BindlessHandleStorage == STORE_UBO)
	{
		GetUniformBlockIndex(DrawTileProg, GlobalTextureHandlesBlockIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawTileProg"));
		GetUniformBlockIndex(DrawComplexProg, GlobalTextureHandlesBlockIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawComplexProg"));
		GetUniformBlockIndex(DrawGouraudProg, GlobalTextureHandlesBlockIndex, GlobalTextureHandlesBindingIndex, "TextureHandles", TEXT("DrawGouraudProg"));
		CHECK_GL_ERROR();
	}

	// Light information.
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformStaticLightInfoIndex, GlobalStaticLightInfoIndex, "StaticLightInfo", TEXT("DrawGouraudProg"));
	CHECK_GL_ERROR();

	//Global Matrix setup
	GetUniformBlockIndex(DrawSimpleProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawSimpleProg"));
	GetUniformBlockIndex(DrawTileProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawTileProg"));
	GetUniformBlockIndex(DrawComplexProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawComplexProg"));
	GetUniformBlockIndex(DrawGouraudProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("DrawGouraudProg"));
	//GetUniformBlockIndex(SimpleDepthProg, GlobalUniformCoordsBlockIndex, GlobalCoordsBindingIndex, "GlobalCoords", TEXT("SimpleDepthProg"));
	CHECK_GL_ERROR();

	//DrawSimple vars.
	FString DrawSimple = TEXT("DrawSimple");
	GetUniformLocation(DrawSimpleDrawColor, DrawSimpleProg, "DrawColor", DrawSimple);
	GetUniformLocation(DrawSimplebHitTesting, DrawSimpleProg, "bHitTesting", DrawSimple);
	GetUniformLocation(DrawSimpleLineFlags, DrawSimpleProg, "LineFlags", DrawSimple);
	GetUniformLocation(DrawSimpleGamma, DrawSimpleProg, "Gamma", DrawSimple);
	CHECK_GL_ERROR();

	//DrawTile vars.
	FString DrawTile = TEXT("DrawTile");
	GetUniformLocation(DrawTileTexCoords, DrawTileProg, "TexCoords", DrawTile);
	GetUniformLocation(DrawTilebHitTesting, DrawTileProg, "bHitTesting", DrawTile);
	GetUniformLocation(DrawTileHitDrawColor, DrawTileProg, "HitDrawColor", DrawTile);
	GetUniformLocation(DrawTileTexture, DrawTileProg, "Texture0", DrawTile);
	GetUniformLocation(DrawTilePolyFlags, DrawTileProg, "PolyFlags", DrawTile);
	GetUniformLocation(DrawTileTexNum, DrawTileProg, "TexNum", DrawTile);
	GetUniformLocation(DrawTileGamma, DrawTileProg, "Gamma", DrawTile);
	CHECK_GL_ERROR();

	//DrawComplexSinglePass vars.
	FString DrawComplexSinglePass = TEXT("DrawComplexSinglePass");
	GetUniformLocation(DrawComplexSinglePassTexCoords, DrawComplexProg, "TexCoords", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassTexNum, DrawComplexProg, "TexNum", DrawComplexSinglePass);
	GetUniformLocation(DrawComplexSinglePassDrawFlags, DrawComplexProg, "DrawFlags", DrawComplexSinglePass);
	// Multitextures in DrawComplexProg
	for (INT i = 0; i < 8; i++)
		GetUniformLocation(DrawComplexSinglePassTexture[i], DrawComplexProg, (char*)(TCHAR_TO_ANSI(*FString::Printf(TEXT("Texture%i"), i))), DrawComplexSinglePass);
	CHECK_GL_ERROR();

	//DrawGouraud vars.
	FString DrawGouraud = TEXT("DrawGouraud");
	GetUniformLocation(DrawGouraudDrawData, DrawGouraudProg, "DrawData", DrawGouraud);
	GetUniformLocation(DrawGouraudDrawFlags, DrawGouraudProg, "DrawFlags", DrawGouraud);
	GetUniformLocation(DrawGouraudTexNum, DrawGouraudProg, "TexNum", DrawGouraud);
	// Multitextures DrawGouraudProg
	for (INT i = 0; i < 8; i++)
		GetUniformLocation(DrawGouraudTexture[i], DrawGouraudProg, (char*)(TCHAR_TO_ANSI(*FString::Printf(TEXT("Texture%i"), i))), DrawGouraud);
	// ShadowMap

	CHECK_GL_ERROR();

	// Global matrices.
	glGenBuffers(1, &GlobalMatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, (5 * sizeof(glm::mat4)), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalMatricesBindingIndex, GlobalMatricesUBO, 0, sizeof(glm::mat4) * 5);
	CHECK_GL_ERROR();

	// Global Coords.
	glGenBuffers(1, &GlobalCoordsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalCoordsUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2 * sizeof(glm::mat4)), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalCoordsBindingIndex, GlobalCoordsUBO, 0, sizeof(glm::mat4) * 2);
	CHECK_GL_ERROR();

	// Global bindless textures.
	if (UsingBindlessTextures && (BindlessHandleStorage == STORE_UBO || BindlessHandleStorage == STORE_SSBO))
	{
		GLenum Target = (BindlessHandleStorage == STORE_UBO) ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
		glGenBuffers(1, &GlobalTextureHandlesBufferObject);
		glBindBuffer(Target, GlobalTextureHandlesBufferObject);
		if (Target == GL_UNIFORM_BUFFER)
		{
			glBufferData(Target, sizeof(GLuint64) * MaxBindlessTextures * 2, NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(Target, 0);
			glBindBufferRange(Target, GlobalTextureHandlesBindingIndex, GlobalTextureHandlesBufferObject, 0, sizeof(GLuint64) * MaxBindlessTextures * 2);
		}
		else
		{
			glBindBufferBase(Target, GlobalTextureHandlesBindingIndex, GlobalTextureHandlesBufferObject);
			glBindBuffer(Target, 0);
		}

		CHECK_GL_ERROR();
	}

	// Static lights information.
	glGenBuffers(1, &GlobalStaticLightInfoUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalStaticLightInfoUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * MAX_LIGHTS * 6, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalStaticLightInfoIndex, GlobalStaticLightInfoUBO, 0, sizeof(glm::vec4) * MAX_LIGHTS * 6);
	CHECK_GL_ERROR();

	// Global ClipPlane.
	glGenBuffers(1, &GlobalClipPlaneUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, GlobalClipPlaneUBO);
	glBufferData(GL_UNIFORM_BUFFER, (2 * sizeof(glm::vec4)), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, GlobalClipPlaneBindingIndex, GlobalClipPlaneUBO, 0, sizeof(glm::vec4) * 2);
	CHECK_GL_ERROR();

	//DrawSimple
	glGenBuffers(1, &DrawSimpleVertBuffer);
	glGenVertexArrays(1, &DrawSimpleGeometryVertsVao);
	CHECK_GL_ERROR();

	//DrawTile
	glGenBuffers(1, &DrawTileVertBuffer);
	glGenVertexArrays(1, &DrawTileVertsVao);
	CHECK_GL_ERROR();

	//DrawComplex
	glGenBuffers(1, &DrawComplexVertBuffer);
	glGenVertexArrays(1, &DrawComplexVertsSinglePassVao);
	CHECK_GL_ERROR();

	glGenBuffers(1, &DrawComplexSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBOBindingIndex, DrawComplexSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	//DrawGouraud
	glGenBuffers(1, &DrawGouraudVertBuffer);
	glGenVertexArrays(1, &DrawGouraudPolyVertsVao);

	glGenBuffers(1, &DrawGouraudSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DrawGouraudSSBOBindingIndex, DrawGouraudSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	/*
	// ShadowMap
	glGenBuffers(1, &SimpleDepthBuffer);
	glGenVertexArrays(1, &SimpleDepthVao);
	*/
	CHECK_GL_ERROR();

	unguard;
}

void UXOpenGLRenderDevice::DeleteShaderBuffers()
{
	guard(UXOpenGLRenderDevice::DeleteShaderBuffers);

	if (GlobalMatricesUBO)
		glDeleteBuffers(1, &GlobalMatricesUBO);
	if (GlobalTextureHandlesBufferObject)
		glDeleteBuffers(1, &GlobalTextureHandlesBufferObject);
	if (GlobalStaticLightInfoUBO)
		glDeleteBuffers(1, &GlobalStaticLightInfoUBO);
	if (GlobalCoordsUBO)
		glDeleteBuffers(1, &GlobalCoordsUBO);
	if (DrawSimpleVertBuffer)
		glDeleteBuffers(1, &DrawSimpleVertBuffer);
	if (DrawTileVertBuffer)
		glDeleteBuffers(1, &DrawTileVertBuffer);
	if (DrawGouraudVertBuffer)
		glDeleteBuffers(1, &DrawGouraudVertBuffer);
	if (DrawComplexVertBuffer)
		glDeleteBuffers(1, &DrawComplexVertBuffer);
	if (DrawComplexSSBO)
		glDeleteBuffers(1, &DrawComplexSSBO);
	if (DrawGouraudSSBO)
		glDeleteBuffers(1, &DrawGouraudSSBO);
	/*
		if (SimpleDepthBuffer)
			glDeleteBuffers(1, &SimpleDepthBuffer);
	*/

	if (DrawSimpleGeometryVertsVao)
		glDeleteVertexArrays(1, &DrawSimpleGeometryVertsVao);
	if (DrawTileVertsVao)
		glDeleteVertexArrays(1, &DrawTileVertsVao);
	if (DrawGouraudPolyVertsVao)
		glDeleteVertexArrays(1, &DrawGouraudPolyVertsVao);

	if (DrawComplexVertsSinglePassVao)
		glDeleteVertexArrays(1, &DrawComplexVertsSinglePassVao);

	/*
	if (SimpleDepthVao)
		glDeleteVertexArrays(1, &SimpleDepthVao);
	*/
	unguard;
}

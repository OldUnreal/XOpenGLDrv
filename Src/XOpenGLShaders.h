#pragma once

typedef void (glShaderProg)(class UXOpenGLRenderDevice*, FOutputDevice&);

static void Implement_Extension(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	// VERSION
	if (GL->OpenGLVersion == GL_Core)
	{
		if (GL->UsingShaderDrawParameters)
			Out.Log(TEXT("#version 460 core"));
		else if (GL->UsingBindlessTextures || GL->UsingPersistentBuffers)
			Out.Log(TEXT("#version 450 core"));
		else Out.Log(TEXT("#version 330 core"));
	}
	else Out.Log(TEXT("#version 310 es\n"));

	if (GL->UsingBindlessTextures) // defined for all variants of our bindlesstextures support
	{
		Out.Log(TEXT("#extension GL_ARB_bindless_texture : require"));

		if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_UBO) // defined if we're going to store handles in a UBO
		{}
		else if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_SSBO) // defined if we're going to store handles in a (much larger) SSBO
			Out.Log(TEXT("#extension GL_ARB_shading_language_420pack : require"));
		else if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_INT) // defined if we're going to pass handles directly to the shader using uniform parameters or the drawcall's parameters in the parameter SSBO
			Out.Log(TEXT("#extension GL_ARB_gpu_shader_int64 : require"));
	}

	if (GL->UsingShaderDrawParameters)
	{
		Out.Log(TEXT("#extension GL_ARB_shader_draw_parameters : require"));
		Out.Log(TEXT("#extension GL_ARB_shading_language_420pack : require"));
	}
	if (GL->OpenGLVersion == GL_ES)
	{
		Out.Log(TEXT("#extension GL_EXT_clip_cull_distance : enable")); // The following extension appears not to be available on RaspberryPi4 at the moment.

		// This determines how much precision the GPU uses when calculating. Performance critical (especially on low end)!! Not every option is available on any platform. Todo: separate option for vert and frag?
		Out.Log(TEXT("precision highp float;")); // options: lowp/mediump/highp, should be mediump for performance reasons, but appears to cause trouble determining DrawFlags then !?! (Currently on NVIDIA 470.103.01).
		Out.Log(TEXT("precision highp int;"));
	}

	Out.Log(TEXT("layout(std140) uniform GlobalMatrices"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("mat4 modelMat;"));
	Out.Log(TEXT("mat4 viewMat;"));
	Out.Log(TEXT("mat4 modelviewMat;"));
	Out.Log(TEXT("mat4 modelviewprojMat;"));
	Out.Log(TEXT("mat4 lightSpaceMat;"));
	Out.Log(TEXT("};"));

	Out.Log(TEXT("layout(std140) uniform ClipPlaneParams"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("vec4  ClipParams;")); // Clipping params, ClipIndex,0,0,0
	Out.Log(TEXT("vec4  ClipPlane;")); // Clipping planes. Plane.X, Plane.Y, Plane.Z, Plane.W
	Out.Log(TEXT("};"));

	// Light information.
	Out.Log(TEXT("layout(std140) uniform StaticLightInfo"));
	Out.Log(TEXT("{"));
	Out.Logf(TEXT("vec4 LightPos[%u];"), MAX_LIGHTS);
	Out.Logf(TEXT("vec4 LightData1[%u];"), MAX_LIGHTS); // LightBrightness, LightHue, LightSaturation, LightCone
	Out.Logf(TEXT("vec4 LightData2[%u];"), MAX_LIGHTS); // LightEffect, LightPeriod, LightPhase, LightRadius
	Out.Logf(TEXT("vec4 LightData3[%u];"), MAX_LIGHTS); // LightType, VolumeBrightness, VolumeFog, VolumeRadius
	Out.Logf(TEXT("vec4 LightData4[%u];"), MAX_LIGHTS); // WorldLightRadius, NumLights, ZoneNumber, CameraRegion->ZoneNumber
	Out.Logf(TEXT("vec4 LightData5[%u];"), MAX_LIGHTS); // NormalLightRadius, bZoneNormalLight, unused, unused
	Out.Log(TEXT("};"));

	Out.Log(TEXT("layout(std140) uniform GlobalCoords"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("mat4 FrameCoords;"));
	Out.Log(TEXT("mat4 FrameUncoords;"));
	Out.Log(TEXT("};"));

	if (GL->UsingBindlessTextures)
	{
		if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_UBO)
		{
			Out.Log(TEXT("layout(std140) uniform TextureHandles"));
			Out.Log(TEXT("{"));
			Out.Logf(TEXT("layout(bindless_sampler) sampler2D Textures[%u];"), GL->MaxBindlessTextures);
			Out.Log(TEXT("};"));

			Out.Log(TEXT("vec4 GetTexel(uint BindlessTexNum, sampler2D BoundSampler, vec2 TexCoords)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("if (BindlessTexNum > 0u)"));
			Out.Log(TEXT("return texture(Textures[BindlessTexNum], TexCoords);"));
			Out.Log(TEXT("return texture(BoundSampler, TexCoords);"));
			Out.Log(TEXT("}"));
		}
		else if (GL->BindlessHandleStorage == UXOpenGLRenderDevice::EBindlessHandleStorage::STORE_SSBO)
		{
			Out.Log(TEXT("layout(std430, binding = 1) buffer TextureHandles"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("uvec2 Textures[];"));
			Out.Log(TEXT("};"));

			Out.Log(TEXT("vec4 GetTexel(uint BindlessTexNum, sampler2D BoundSampler, vec2 TexCoords)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("if (BindlessTexNum > 0u)"));
			Out.Log(TEXT("return texture(sampler2D(Textures[BindlessTexNum]), TexCoords);"));
			Out.Log(TEXT("return texture(BoundSampler, TexCoords);"));
			Out.Log(TEXT("}"));
		}
		else
		{
			Out.Log(TEXT("vec4 GetTexel(uint BindlessTexNum, sampler2D BoundSampler, vec2 TexCoords)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("return texture(BoundSampler, TexCoords);"));
			Out.Log(TEXT("}"));
		}
	}
	//DistanceFog
	Out.Log(TEXT("struct FogParameters"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("vec4  FogColor;")); // Fog color
	Out.Log(TEXT("float FogStart;")); // Only for linear fog
	Out.Log(TEXT("float FogEnd;")); // Only for linear fog
	Out.Log(TEXT("float FogDensity;")); // For exp and exp2 equation
	Out.Log(TEXT("float FogCoord;"));
	Out.Log(TEXT("int FogMode;")); // 0 = linear, 1 = exp, 2 = exp2
	Out.Log(TEXT("};"));

	Out.Log(TEXT("float getFogFactor(FogParameters DistanceFog)"));
	Out.Log(TEXT("{"));
	// DistanceFogValues.x = FogStart
	// DistanceFogValues.y = FogEnd
	// DistanceFogValues.z = FogDensity
	// DistanceFogValues.w = FogMode
	//FogResult = (Values.y-FogCoord)/(Values.y-Values.x);
	Out.Log(TEXT("float FogResult = 1.0;"));
	Out.Log(TEXT("if (DistanceFog.FogMode == 0)"));
	Out.Log(TEXT("FogResult = ((DistanceFog.FogEnd - DistanceFog.FogCoord) / (DistanceFog.FogEnd - DistanceFog.FogStart));"));
	Out.Log(TEXT("else if (DistanceFog.FogMode == 1)"));
	Out.Log(TEXT("FogResult = exp(-DistanceFog.FogDensity * DistanceFog.FogCoord);"));
	Out.Log(TEXT("else if (DistanceFog.FogMode == 2)"));
	Out.Log(TEXT("FogResult = exp(-pow(DistanceFog.FogDensity * DistanceFog.FogCoord, 2.0));"));
	Out.Log(TEXT("FogResult = 1.0 - clamp(FogResult, 0.0, 1.0);"));
	Out.Log(TEXT("return FogResult;"));
	Out.Log(TEXT("}"));

	Out.Log(TEXT("float PlaneDot(vec4 Plane, vec3 Point)"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("return dot(Plane.xyz, Point) - Plane.w;"));
	Out.Log(TEXT("}"));

	// The following directive resets the line number to 1 to have the correct output logging for a possible error within the shader files.
	Out.Log(TEXT("#line 1"));
}

// Fragmentshader for DrawSimple, line drawing.
static void Create_DrawSimple_Frag(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("uniform vec4 DrawColor;"));
	Out.Log(TEXT("uniform uint LineFlags;"));
	Out.Log(TEXT("uniform bool bHitTesting;"));
	Out.Log(TEXT("uniform float Gamma;"));

	if (GL->OpenGLVersion == GL_ES)
	{
		Out.Log(TEXT("layout(location = 0) out vec4 FragColor;"));
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout ( location = 1 ) out vec4 FragColor1;"));
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 0, index = 1) out vec4 FragColor1;"));
		Out.Log(TEXT("layout(location = 0, index = 0) out vec4 FragColor;"));
	}

	Out.Log(TEXT("void main(void)"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("vec4 TotalColor = DrawColor;"));

	// stijn: this is an attempt at stippled line drawing in GL4
#if 0
	Out.Logf(TEXT("if ((LineFlags & %u) == %u)"), LINE_Transparent, LINE_Transparent);
	Out.Log(TEXT("{"));
	Out.Log(TEXT("if (((uint(floor(gl_FragCoord.x)) & 1u) ^ (uint(floor(gl_FragCoord.y)) & 1u)) == 0u)"));
	Out.Log(TEXT("discard;"));
	Out.Log(TEXT("}"));
#endif
	if (GIsEditor)
	{
		Out.Log(TEXT("if (!bHitTesting)"));
		Out.Log(TEXT("{"));
	}
	Out.Logf(TEXT("float InGamma = 1.0 / (Gamma * %f);"), GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier); // Gamma
	Out.Log(TEXT("TotalColor.r = pow(TotalColor.r, InGamma);"));
	Out.Log(TEXT("TotalColor.g = pow(TotalColor.g, InGamma);"));
	Out.Log(TEXT("TotalColor.b = pow(TotalColor.b, InGamma);"));
	if (GIsEditor)
		Out.Log(TEXT("}"));

	if (GL->SimulateMultiPass)
		Out.Log(TEXT("FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;"));
	else Out.Log(TEXT("FragColor = TotalColor;"));
	Out.Log(TEXT("}"));
}

// Vertexshader for DrawSimple, Line drawing.
static void Create_DrawSimple_Vert(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(location = 0) in vec3 Coords;")); // == gl_Vertex

	if (GL->OpenGLVersion != GL_ES)
		Out.Logf(TEXT("out float gl_ClipDistance[%i];"), GL->MaxClippingPlanes);

	Out.Log(TEXT("void main(void)"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("vec4 vEyeSpacePos = modelviewMat * vec4(Coords, 1.0);"));
	Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Coords, 1.0);"));
	if (GL->OpenGLVersion != GL_ES)
	{
		Out.Log(TEXT("uint ClipIndex = uint(ClipParams.x);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Coords);"));
	}
	Out.Log(TEXT("}"));
}

// Geoemtry shader for DrawSimple, line drawing.
static void Create_DrawSimple_Geo(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(lines) in;"));
	Out.Log(TEXT("layout(line_strip, max_vertices = 2) out;"));

	Out.Log(TEXT("noperspective out float texCoord;"));

	{
		Out.Log(TEXT("void main()"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("mat4 modelviewMat = modelMat * viewMat;"));
		Out.Log(TEXT("vec2 winPos1 = vec2(512, 384) * gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;"));
		Out.Log(TEXT("vec2 winPos2 = vec2(512, 384) * gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;"));

		// Line Start
		Out.Log(TEXT("gl_Position = modelviewprojMat * (gl_in[0].gl_Position);"));
		Out.Log(TEXT("texCoord = 0.0;"));
		Out.Log(TEXT("EmitVertex();"));

		// Line End
		Out.Log(TEXT("gl_Position = modelviewprojMat * (gl_in[1].gl_Position);"));
		Out.Log(TEXT("texCoord = length(winPos2 - winPos1);"));
		Out.Log(TEXT("EmitVertex();"));
		Out.Log(TEXT("EndPrimitive();"));
		Out.Log(TEXT("}"));
	}
}

// Fragmentshader for DrawComplexSurface, single pass.
static void Create_DrawComplexSinglePass_Frag(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("uniform sampler2D Texture0;")); //Base Texture
	Out.Log(TEXT("uniform sampler2D Texture1;")); //Lightmap
	Out.Log(TEXT("uniform sampler2D Texture2;")); //Fogmap
	Out.Log(TEXT("uniform sampler2D Texture3;")); //Detail Texture
	Out.Log(TEXT("uniform sampler2D Texture4;")); //Macro Texture
	Out.Log(TEXT("uniform sampler2D Texture5;")); //BumpMap
	Out.Log(TEXT("uniform sampler2D Texture6;")); //EnvironmentMap
	Out.Log(TEXT("uniform sampler2D Texture7;")); //HeightMap

	Out.Log(TEXT("in vec3 vCoords;"));
	if (GIsEditor)
		Out.Log(TEXT("flat in vec3 vSurfaceNormal;"));
#if ENGINE_VERSION!=227
	if (GL->BumpMaps)
#endif
	{
		Out.Log(TEXT("in vec4 vEyeSpacePos;"));
		Out.Log(TEXT("flat in mat3 vTBNMat;"));
	}
	Out.Log(TEXT("in vec2 vTexCoords;"));
	Out.Log(TEXT("in vec2 vLightMapCoords;"));
	Out.Log(TEXT("in vec2 vFogMapCoords;"));
	Out.Log(TEXT("in vec3 vTangentViewPos;"));
	Out.Log(TEXT("in vec3 vTangentFragPos;"));
	if (GL->DetailTextures)
		Out.Log(TEXT("in vec2 vDetailTexCoords;"));
	if (GL->MacroTextures)
		Out.Log(TEXT("in vec2 vMacroTexCoords;"));
	if (GL->BumpMaps)
		Out.Log(TEXT("in vec2 vBumpTexCoords;"));
#if ENGINE_VERSION==227
	Out.Log(TEXT("in vec2 vEnvironmentTexCoords;"));
#endif
	if (GL->OpenGLVersion == GL_ES)
	{
		Out.Log(TEXT("layout(location = 0) out vec4 FragColor;"));
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 1) out vec4 FragColor1;"));
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 0, index = 1) out vec4 FragColor1;"));
		Out.Log(TEXT("layout(location = 0, index = 0) out vec4 FragColor;"));
	}
	if (GL->UsingShaderDrawParameters)
	{
		Out.Log(TEXT("flat in uint vTexNum;"));
		Out.Log(TEXT("flat in uint vLightMapTexNum;"));
		Out.Log(TEXT("flat in uint vFogMapTexNum;"));
		Out.Log(TEXT("flat in uint vDetailTexNum;"));
		Out.Log(TEXT("flat in uint vMacroTexNum;"));
		Out.Log(TEXT("flat in uint vBumpMapTexNum;"));
		Out.Log(TEXT("flat in uint vEnviroMapTexNum;"));
		Out.Log(TEXT("flat in uint vHeightMapTexNum;"));
		Out.Log(TEXT("flat in uint vDrawFlags;"));
		Out.Log(TEXT("flat in uint vTextureFormat;"));
		Out.Log(TEXT("flat in uint vPolyFlags;"));
		Out.Log(TEXT("flat in float vBaseDiffuse;"));
		Out.Log(TEXT("flat in float vBaseAlpha;"));
		Out.Log(TEXT("flat in float vParallaxScale;"));
		Out.Log(TEXT("flat in float vGamma;"));
		Out.Log(TEXT("flat in float vBumpMapSpecular;"));
		if (GIsEditor)
		{
			Out.Log(TEXT("flat in uint vHitTesting;"));
			Out.Log(TEXT("flat in uint vRendMap;"));
			Out.Log(TEXT("flat in vec4 vDrawColor;"));
		}
		Out.Log(TEXT("flat in vec4 vDistanceFogColor;"));
		Out.Log(TEXT("flat in vec4 vDistanceFogInfo;"));
	}
	else
	{
		Out.Log(TEXT("float vBumpMapSpecular;"));
		Out.Log(TEXT("uniform vec4 TexCoords[16];"));
		Out.Log(TEXT("uniform uint TexNum[16];"));
		Out.Log(TEXT("uniform uint DrawFlags[4];"));
	}

	Out.Log(TEXT("vec3 rgb2hsv(vec3 c)"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);")); // some nice stuff from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
	Out.Log(TEXT("vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);"));
	Out.Log(TEXT("vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);"));
	Out.Log(TEXT("float d = q.x - min(q.w, q.y);"));
	Out.Log(TEXT("float e = 1.0e-10;"));
	Out.Log(TEXT("return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);"));
	Out.Log(TEXT("}"));

	Out.Log(TEXT("vec3 hsv2rgb(vec3 c)"));
	Out.Log(TEXT("{"));
	Out.Log(TEXT("vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);"));
	Out.Log(TEXT("vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);"));
	Out.Log(TEXT("return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);"));
	Out.Log(TEXT("}"));

#if ENGINE_VERSION==227
	{
		Out.Log(TEXT("vec2 ParallaxMapping(vec2 ptexCoords, vec3 viewDir, uint TexNum, out float parallaxHeight)"));
		Out.Log(TEXT("{"));
		if (GL->ParallaxVersion == Parallax_Basic) // very basic implementation
		{
			Out.Log(TEXT("float height = GetTexel(TexNum, Texture7, ptexCoords).r;"));
			Out.Log(TEXT("return ptexCoords - viewDir.xy * (height * 0.1);"));
		}
		else if (GL->ParallaxVersion == Parallax_Occlusion) // parallax occlusion mapping
		{
			if (!GL->UsingShaderDrawParameters)
			{
				Out.Logf(TEXT("float vParallaxScale = TexCoords[%u].z * 0.025;"), DCTI_HEIGHTMAP_INFO); // arbitrary to get DrawScale into (for this purpose) sane regions.
				Out.Logf(TEXT("float vTimeSeconds = TexCoords[%u].w;"), DCTI_HEIGHTMAP_INFO); // Surface.Level->TimeSeconds
			}
			//vParallaxScale += 8.0f * sin(vTimeSeconds) + 4.0 * cos(2.3f * vTimeSeconds);
			// number of depth layers
			constexpr FLOAT minLayers = 8.f;
			constexpr FLOAT maxLayers = 32.f;
			Out.Logf(TEXT("float numLayers = mix(%f, %f, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));"), maxLayers, minLayers);
			Out.Log(TEXT("float layerDepth = 1.0 / numLayers;")); // calculate the size of each layer
			Out.Log(TEXT("float currentLayerDepth = 0.0;")); // depth of current layer

			// the amount to shift the texture coordinates per layer (from vector P)
			Out.Log(TEXT("vec2 P = viewDir.xy / viewDir.z * vParallaxScale;"));
			Out.Log(TEXT("vec2 deltaTexCoords = P / numLayers;"));

			// get initial values
			Out.Log(TEXT("vec2  currentTexCoords = ptexCoords;"));
			Out.Log(TEXT("float currentDepthMapValue = 0.0;"));
			Out.Log(TEXT("currentDepthMapValue = GetTexel(TexNum, Texture7, currentTexCoords).r;"));

			Out.Log(TEXT("while (currentLayerDepth < currentDepthMapValue)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("currentTexCoords -= deltaTexCoords;")); // shift texture coordinates along direction of P
			Out.Log(TEXT("currentDepthMapValue = GetTexel(TexNum, Texture7, currentTexCoords).r;")); // get depthmap value at current texture coordinates
			Out.Log(TEXT("currentLayerDepth += layerDepth;")); // get depth of next layer
			Out.Log(TEXT("}"));
			Out.Log(TEXT("vec2 prevTexCoords = currentTexCoords + deltaTexCoords;")); // get texture coordinates before collision (reverse operations)

			// get depth after and before collision for linear interpolation
			Out.Log(TEXT("float afterDepth = currentDepthMapValue - currentLayerDepth;"));
			Out.Log(TEXT("float beforeDepth = GetTexel(TexNum, Texture7, currentTexCoords).r - currentLayerDepth + layerDepth;"));

			// interpolation of texture coordinates
			Out.Log(TEXT("float weight = afterDepth / (afterDepth - beforeDepth);"));
			Out.Log(TEXT("vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);"));
			Out.Log(TEXT("return finalTexCoords;"));
		}
		else if (GL->ParallaxVersion == Parallax_Relief) // Relief Parallax Mapping
		{
			// determine required number of layers
			constexpr FLOAT minLayers = 10.f;
			constexpr FLOAT maxLayers = 15.f;
			Out.Logf(TEXT("float numLayers = mix(%f, %f, abs(dot(vec3(0, 0, 1), viewDir)));"), maxLayers, minLayers);

			if (!GL->UsingShaderDrawParameters)
			{
				Out.Logf(TEXT("float vParallaxScale = TexCoords[%u].z * 0.025;"), DCTI_HEIGHTMAP_INFO); // arbitrary to get DrawScale into (for this purpose) sane regions.
				Out.Logf(TEXT("float vTimeSeconds = TexCoords[%u].w;"), DCTI_HEIGHTMAP_INFO); // Surface.Level->TimeSeconds
			}
			Out.Log(TEXT("float layerHeight = 1.0 / numLayers;")); // height of each layer
			Out.Log(TEXT("float currentLayerHeight = 0.0;")); // depth of current layer
			Out.Log(TEXT("vec2 dtex = vParallaxScale * viewDir.xy / viewDir.z / numLayers;")); // shift of texture coordinates for each iteration
			Out.Log(TEXT("vec2 currentTexCoords = ptexCoords;")); // current texture coordinates
			Out.Log(TEXT("float heightFromTexture = GetTexel(TexNum, Texture7, currentTexCoords).r;")); // depth from heightmap

			// while point is above surface
			Out.Log(TEXT("while (heightFromTexture > currentLayerHeight)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("currentLayerHeight += layerHeight;")); // go to the next layer
			Out.Log(TEXT("currentTexCoords -= dtex;")); // shift texture coordinates along V
			Out.Log(TEXT("heightFromTexture = GetTexel(TexNum, Texture7, currentTexCoords).r;")); // new depth from heightmap
			Out.Log(TEXT("}"));

			///////////////////////////////////////////////////////////
			// Start of Relief Parallax Mapping
			// decrease shift and height of layer by half
			Out.Log(TEXT("vec2 deltaTexCoord = dtex / 2.0;"));
			Out.Log(TEXT("float deltaHeight = layerHeight / 2.0;"));

			// return to the mid point of previous layer
			Out.Log(TEXT("currentTexCoords += deltaTexCoord;"));
			Out.Log(TEXT("currentLayerHeight -= deltaHeight;"));

			// binary search to increase precision of Steep Paralax Mapping
			constexpr INT numSearches = 5;
			Out.Logf(TEXT("for (int i = 0; i < %u; i++)"), numSearches);
			Out.Log(TEXT("{"));
			// decrease shift and height of layer by half
			Out.Log(TEXT("deltaTexCoord /= 2.0;"));
			Out.Log(TEXT("deltaHeight /= 2.0;"));

			// new depth from heightmap
			Out.Log(TEXT("heightFromTexture = GetTexel(TexNum, Texture7, currentTexCoords).r;"));

			// shift along or agains vector V
			Out.Log(TEXT("if (heightFromTexture > currentLayerHeight)")); // below the surface
			Out.Log(TEXT("{"));
			Out.Log(TEXT("currentTexCoords -= deltaTexCoord;"));
			Out.Log(TEXT("currentLayerHeight += deltaHeight;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("else")); // above the surface
			Out.Log(TEXT("{"));
			Out.Log(TEXT("currentTexCoords += deltaTexCoord;"));
			Out.Log(TEXT("currentLayerHeight -= deltaHeight;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));

			// return results
			Out.Log(TEXT("parallaxHeight = currentLayerHeight;"));
			Out.Log(TEXT("return currentTexCoords;"));
		}
		else
		{
			Out.Log(TEXT("return ptexCoords;"));
		}
		Out.Log(TEXT("}"));
	}

	// unused. Maybe later.
#if 0
	{
		Out.Log(TEXT("float parallaxSoftShadowMultiplier(in vec3 L, in vec2 initialTexCoord, in float initialHeight)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("float shadowMultiplier = 1;"));

		constexpr FLOAT minLayers = 15;
		constexpr FLOAT maxLayers = 30;
		if (!GL->UsingShaderDrawParameters)
			Out.Log(TEXT("float vParallaxScale = TexCoords[%u].w * 0.025;"), DCTI_MACRO_INFO); // arbitrary to get DrawScale into (for this purpose) sane regions.

		// calculate lighting only for surface oriented to the light source
		Out.Log(TEXT("if(dot(vec3(0, 0, 1), L) > 0)"));
		Out.Log(TEXT("{"));
		// calculate initial parameters
		Out.Log(TEXT("float numSamplesUnderSurface = 0;"));
		Out.Log(TEXT("shadowMultiplier = 0;"));
		Out.Logf(TEXT("float numLayers = mix(%f, %f, abs(dot(vec3(0, 0, 1), L)));"), maxLayers, minLayers);
		Out.Log(TEXT("float layerHeight = initialHeight / numLayers;"));
		Out.Log(TEXT("vec2 texStep = vParallaxScale * L.xy / L.z / numLayers;"));

		// current parameters
		Out.Log(TEXT("float currentLayerHeight = initialHeight - layerHeight;"));
		Out.Log(TEXT("vec2 currentTexCoords = initialTexCoord + texStep;"));
		Out.Log(TEXT("float heightFromTexture = GetTexel(vMacroTexNum, Texture7, currentTexCoords).r;"));

		Out.Log(TEXT("int stepIndex = 1;"));

		// while point is below depth 0.0 )
		Out.Log(TEXT("while(currentLayerHeight > 0)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("if(heightFromTexture < currentLayerHeight)")); // if point is under the surface
		Out.Log(TEXT("{"));
		// calculate partial shadowing factor
		Out.Log(TEXT("numSamplesUnderSurface += 1;"));
		Out.Log(TEXT("float newShadowMultiplier = (currentLayerHeight - heightFromTexture) * (1.0 - stepIndex / numLayers);"));
		Out.Log(TEXT("shadowMultiplier = max(shadowMultiplier, newShadowMultiplier);"));
		Out.Log(TEXT("}"));
		// offset to the next layer
		Out.Log(TEXT("stepIndex += 1;"));
		Out.Log(TEXT("currentLayerHeight -= layerHeight;"));
		Out.Log(TEXT("currentTexCoords += texStep;"));
		Out.Log(TEXT("heightFromTexture = GetTexel(vMacroTexNum, Texture7, currentTexCoords).r;"));
		Out.Log(TEXT("}"));

		// Shadowing factor should be 1 if there were no points under the surface
		Out.Log(TEXT("if(numSamplesUnderSurface < 1)"));
		Out.Log(TEXT("shadowMultiplier = 1;"));
		Out.Log(TEXT("else"));
		Out.Log(TEXT("shadowMultiplier = 1.0 - shadowMultiplier;"));
		Out.Log(TEXT("}"));
		Out.Log(TEXT("return shadowMultiplier;"));
		Out.Log(TEXT("}"));
	}
#endif
#endif

#if 1
	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz);")); // TransformPointBy...
		Out.Log(TEXT("mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);"));

		Out.Log(TEXT("vec4 TotalColor = vec4(1.0);"));
		Out.Log(TEXT("vec2 texCoords = vTexCoords;"));

		if (!GL->UsingShaderDrawParameters)
		{
			Out.Log(TEXT("uint vDrawFlags = DrawFlags[0];"));
			Out.Log(TEXT("uint vTextureFormat = DrawFlags[1];"));
			Out.Log(TEXT("uint vPolyFlags = DrawFlags[2];"));
			Out.Log(TEXT("uint vRendMap = DrawFlags[3];"));
			Out.Log(TEXT("bool bHitTesting = bool(TexNum[12]);"));
			Out.Logf(TEXT("float vBaseDiffuse = TexCoords[%u].x;"), DCTI_DIFFUSE_INFO);
			Out.Logf(TEXT("float vBaseAlpha = TexCoords[%u].z;"), DCTI_DIFFUSE_INFO);
			Out.Logf(TEXT("float vGamma = TexCoords[%u].w;"), DCTI_Z_AXIS);
			Out.Logf(TEXT("vec4 vDrawColor = TexCoords[%u];"), DCTI_EDITOR_DRAWCOLOR);
			Out.Logf(TEXT("vec4 vBumpMapInfo = TexCoords[%u];"), DCTI_BUMPMAP_INFO);
			Out.Logf(TEXT("vec4 vDistanceFogColor = TexCoords[%u];"), DCTI_DISTANCE_FOG_COLOR);
			Out.Logf(TEXT("vec4 vDistanceFogInfo = TexCoords[%u];"), DCTI_DISTANCE_FOG_INFO);
			Out.Logf(TEXT("vec4 vHeightMapInfo = TexCoords[%u];"), DCTI_HEIGHTMAP_INFO);

			if (GL->UsingBindlessTextures)
			{
				Out.Log(TEXT("uint vTexNum = TexNum[0];"));
				Out.Log(TEXT("uint vLightMapTexNum = TexNum[1];"));
				Out.Log(TEXT("uint vFogMapTexNum = TexNum[2];"));
				Out.Log(TEXT("uint vDetailTexNum = TexNum[3];"));
				Out.Log(TEXT("uint vMacroTexNum = TexNum[4];"));
				Out.Log(TEXT("uint vBumpMapTexNum = TexNum[5];"));
				Out.Log(TEXT("uint vEnviroMapTexNum = TexNum[6];"));
				Out.Log(TEXT("uint vHeightMapTexNum = TexNum[7];"));
			}
			else
			{
				Out.Log(TEXT("uint vTexNum = 0u;"));
				Out.Log(TEXT("uint vLightMapTexNum = 0u;"));
				Out.Log(TEXT("uint vFogMapTexNum = 0u;"));
				Out.Log(TEXT("uint vDetailTexNum = 0u;"));
				Out.Log(TEXT("uint vMacroTexNum = 0u;"));
				Out.Log(TEXT("uint vBumpMapTexNum = 0u;"));
				Out.Log(TEXT("uint vEnviroMapTexNum = 0u;"));
				Out.Log(TEXT("uint vHeightMapTexNum = 0u;"));
				Out.Log(TEXT(""));
			}
		}
		else if (GIsEditor)
			Out.Log(TEXT("bool bHitTesting = bool(vHitTesting);"));

		if (GL->UseHWLighting || GL->BumpMaps)
		{
			Out.Log(TEXT("vec3 TangentViewDir = normalize(vTangentViewPos - vTangentFragPos);"));
			Out.Log(TEXT("int NumLights = int(LightData4[0].y);"));

			if (!GL->UsingShaderDrawParameters)
				Out.Logf(TEXT("vBumpMapSpecular = TexCoords[%u].y;"), DCTI_BUMPMAP_INFO);

			if (GL->ParallaxVersion == Parallax_Basic || GL->ParallaxVersion == Parallax_Occlusion || GL->ParallaxVersion == Parallax_Relief)
			{
				// ParallaxMap
				Out.Log(TEXT("float parallaxHeight = 1.0;"));
				Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_HeightMap, DF_HeightMap);
				Out.Log(TEXT("{"));
				// get new texture coordinates from Parallax Mapping
				Out.Log(TEXT("texCoords = ParallaxMapping(vTexCoords, TangentViewDir, vHeightMapTexNum, parallaxHeight);"));
				//Out.Log(TEXT("if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)"));
				//Out.Log(TEXT("discard;")); // texCoords = vTexCoords;
				Out.Log(TEXT("}"));
			}
		}

		Out.Log(TEXT("vec4 Color = GetTexel(vTexNum, Texture0, texCoords);"));

		Out.Log(TEXT("if (vBaseDiffuse > 0.0)"));
		Out.Log(TEXT("Color *= vBaseDiffuse;")); // Diffuse factor.

		Out.Log(TEXT("if (vBaseAlpha > 0.0)"));
		Out.Log(TEXT("Color.a *= vBaseAlpha;")); // Alpha.

		Out.Logf(TEXT("if (vTextureFormat == %u)"), TEXF_BC5); //BC5 (GL_COMPRESSED_RG_RGTC2) compression
		Out.Log(TEXT("Color.b = sqrt(1.0 - Color.r * Color.r + Color.g * Color.g);"));

		// Handle PF_Masked.
		Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Masked, PF_Masked);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("if (Color.a < 0.5)"));
		Out.Log(TEXT("discard;"));
		Out.Log(TEXT("else Color.rgb /= Color.a;"));
		Out.Log(TEXT("}"));
		Out.Logf(TEXT("else if ((vPolyFlags & %u) == %u && Color.a < 0.01)"), PF_AlphaBlend, PF_AlphaBlend);
		Out.Log(TEXT("discard;"));
		/*	if ((vPolyFlags&PF_Mirrored) == PF_Mirrored)
		{
		//add mirror code here.
		}*/

		Out.Log(TEXT("TotalColor = Color;"));
		Out.Log(TEXT("vec4 LightColor = vec4(1.0);"));

		if (GL->UseHWLighting)
		{
			Out.Log(TEXT("float LightAdd = 0.0f;"));
			Out.Log(TEXT("LightColor = vec4(0.0);"));

			Out.Log(TEXT("for (int i = 0; i < NumLights; i++)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("float WorldLightRadius = LightData4[i].x;"));
			Out.Log(TEXT("float LightRadius = LightData2[i].w;"));
			Out.Log(TEXT("float RWorldLightRadius = WorldLightRadius * WorldLightRadius;"));

			Out.Log(TEXT("vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords);")); // Frame->Coords.
			Out.Log(TEXT("float dist = distance(vCoords, InLightPos);"));

			Out.Log(TEXT("if (dist < RWorldLightRadius)"));
			Out.Log(TEXT("{"));
			// Light color
			Out.Log(TEXT("vec4 CurrentLightColor = vec4(LightData1[i].x, LightData1[i].y, LightData1[i].z, 1.0);"));
			constexpr FLOAT MinLight = 0.05;
			Out.Logf(TEXT("float b = WorldLightRadius / (RWorldLightRadius * %f);"), MinLight);
			Out.Log(TEXT("float attenuation = WorldLightRadius / (dist + b * dist * dist);"));
			// Out.Log(TEXT("float attenuation = 0.82*(1.0-smoothstep(LightRadius,24.0*LightRadius+0.50,dist));"));
			Out.Log(TEXT("LightColor += CurrentLightColor * attenuation;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("TotalColor *= LightColor;"));
		}
		else
		{
			// LightMap
			Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_LightMap, DF_LightMap);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("LightColor = GetTexel(vLightMapTexNum, Texture1, vLightMapCoords);"));

			// Fetch lightmap texel. Data in LightMap is in 0..127/255 range, which needs to be scaled to 0..2 range.
			if (GL->OpenGLVersion == GL_ES)
				Out.Log(TEXT("LightColor.bgr = LightColor.bgr * (2.0 * 255.0 / 127.0);"));
			else Out.Log(TEXT("LightColor.rgb = LightColor.rgb * (2.0 * 255.0 / 127.0);"));
			Out.Log(TEXT("LightColor.a = 1.0;"));
			Out.Log(TEXT("}"));
		}

		// DetailTextures
		if (GL->DetailTextures)
		{
			Out.Logf(TEXT("if (((vDrawFlags & %u) == %u))"), DF_DetailTexture, DF_DetailTexture);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("float NearZ = vCoords.z / 512.0;"));
			Out.Log(TEXT("float DetailScale = 1.0;"));
			Out.Log(TEXT("float bNear;"));
			Out.Log(TEXT("vec4 DetailTexColor;"));
			Out.Log(TEXT("vec3 hsvDetailTex;"));

			Out.Logf(TEXT("for (int i = 0; i < %i; ++i)"), GL->DetailMax);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("if (i > 0)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("NearZ *= 4.223f;"));
			Out.Log(TEXT("DetailScale *= 4.223f;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("bNear = clamp(0.65 - NearZ, 0.0, 1.0);"));
			Out.Log(TEXT("if (bNear > 0.0)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("DetailTexColor = GetTexel(vDetailTexNum, Texture3, vDetailTexCoords * DetailScale);"));
			Out.Log(TEXT("vec3 hsvDetailTex = rgb2hsv(DetailTexColor.rgb);")); // cool idea Han :)
			Out.Log(TEXT("hsvDetailTex.b += (DetailTexColor.r - 0.1);"));
			Out.Log(TEXT("hsvDetailTex = hsv2rgb(hsvDetailTex);"));
			Out.Log(TEXT("DetailTexColor = vec4(hsvDetailTex, 0.0);"));
			Out.Log(TEXT("DetailTexColor = mix(vec4(1.0, 1.0, 1.0, 1.0), DetailTexColor, bNear);")); //fading out.
			Out.Log(TEXT("TotalColor.rgb *= DetailTexColor.rgb;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
		}

		// MacroTextures
		if (GL->MacroTextures)
		{
			Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_MacroTexture, DF_MacroTexture);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec4 MacrotexColor = GetTexel(vMacroTexNum, Texture4, vMacroTexCoords);"));
			Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Masked, PF_Masked);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("if (MacrotexColor.a < 0.5)"));
			Out.Log(TEXT("discard;"));
			Out.Log(TEXT("else MacrotexColor.rgb /= MacrotexColor.a;"));
			Out.Log(TEXT("}"));
			Out.Logf(TEXT("else if ((vPolyFlags & %u) == %u && MacrotexColor.a < 0.01)"), PF_AlphaBlend, PF_AlphaBlend);
			Out.Log(TEXT("discard;"));

			Out.Log(TEXT("vec3 hsvMacroTex = rgb2hsv(MacrotexColor.rgb);"));
			Out.Log(TEXT("hsvMacroTex.b += (MacrotexColor.r - 0.1);"));
			Out.Log(TEXT("hsvMacroTex = hsv2rgb(hsvMacroTex);"));
			Out.Log(TEXT("MacrotexColor = vec4(hsvMacroTex, 1.0);"));
			Out.Log(TEXT("TotalColor *= MacrotexColor;"));
			Out.Log(TEXT("}"));
		}

		// BumpMap (Normal Map)
		if (GL->BumpMaps)
		{
			Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_BumpMap, DF_BumpMap);
			Out.Log(TEXT("{"));
			//normal from normal map
			Out.Log(TEXT("vec3 TextureNormal = normalize(GetTexel(vBumpMapTexNum, Texture5, texCoords).rgb * 2.0 - 1.0);")); // has to be texCoords instead of vBumpTexCoords, otherwise alignment won't work on bumps.
			Out.Log(TEXT("vec3 BumpColor;"));
			Out.Log(TEXT("vec3 TotalBumpColor = vec3(0.0, 0.0, 0.0);"));

			Out.Log(TEXT("for (int i = 0; i < NumLights; ++i)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec3 CurrentLightColor = vec3(LightData1[i].x, LightData1[i].y, LightData1[i].z);"));

			Out.Log(TEXT("float NormalLightRadius = LightData5[i].x;"));
			Out.Log(TEXT("bool bZoneNormalLight = bool(LightData5[i].y);"));
			Out.Log(TEXT("float LightBrightness = LightData5[i].z / 255.0;")); // use LightBrightness to adjust specular reflection.

			Out.Log(TEXT("if (NormalLightRadius == 0.0)"));
			Out.Log(TEXT("NormalLightRadius = LightData2[i].w * 64.0;"));

			Out.Logf(TEXT("bool bSunlight = (uint(LightData2[i].x) == %u);"), LE_Sunlight);
			Out.Log(TEXT("vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords);")); // Frame->Coords.

			Out.Log(TEXT("float dist = distance(vCoords, InLightPos);"));

			constexpr float MinLight = 0.05;
			Out.Logf(TEXT("float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * %f);"), MinLight);
			Out.Log(TEXT("float attenuation = NormalLightRadius / (dist + b * dist * dist);"));

			Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Unlit, PF_Unlit);
			Out.Log(TEXT("InLightPos = vec3(1.0, 1.0, 1.0);")); //no idea whats best here. Arbitrary value based on some tests.

			Out.Log(TEXT("if ((NormalLightRadius == 0.0 || (dist > NormalLightRadius) || (bZoneNormalLight && (LightData4[i].z != LightData4[i].w))) && !bSunlight)")); // Do not consider if not in range or in a different zone.
			Out.Log(TEXT("continue;"));

			Out.Log(TEXT("vec3 TangentLightPos = vTBNMat * InLightPos;"));
			Out.Log(TEXT("vec3 TangentlightDir = normalize(TangentLightPos - vTangentFragPos);"));

			// ambient
			Out.Log(TEXT("vec3 ambient = 0.1 * TotalColor.xyz;"));

			// diffuse
			Out.Log(TEXT("float diff = max(dot(TangentlightDir, TextureNormal), 0.0);"));
			Out.Log(TEXT("vec3 diffuse = diff * TotalColor.xyz;"));

			// specular
			Out.Log(TEXT("vec3 halfwayDir = normalize(TangentlightDir + TangentViewDir);"));
			Out.Log(TEXT("float spec = pow(max(dot(TextureNormal, halfwayDir), 0.0), 8.0);"));
			Out.Log(TEXT("vec3 specular = vec3(max(vBumpMapSpecular, 0.1)) * spec * CurrentLightColor * LightBrightness;"));

			Out.Log(TEXT("TotalBumpColor += (ambient + diffuse + specular) * attenuation;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("TotalColor += vec4(clamp(TotalBumpColor, 0.0, 1.0), 1.0);"));
			Out.Log(TEXT("}"));
		}
		// FogMap
		Out.Log(TEXT("vec4 FogColor = vec4(0.0);"));
		Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_FogMap, DF_FogMap);
		Out.Log(TEXT("FogColor = GetTexel(vFogMapTexNum, Texture2, vFogMapCoords) * 2.0;"));

		// EnvironmentMap
#if ENGINE_VERSION==227
		Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_EnvironmentMap, DF_EnvironmentMap);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec4 EnvironmentColor = GetTexel(vEnviroMapTexNum, Texture6, vEnvironmentTexCoords);"));
		Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Masked, PF_Masked);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("if (EnvironmentColor.a < 0.5)"));
		Out.Log(TEXT("discard;"));
		Out.Log(TEXT("else EnvironmentColor.rgb /= EnvironmentColor.a;"));
		Out.Log(TEXT("}"));
		Out.Logf(TEXT("else if ((vPolyFlags & %u) == %u && EnvironmentColor.a < 0.01)"), PF_AlphaBlend, PF_AlphaBlend);
		Out.Log(TEXT("discard;"));

		Out.Log(TEXT("TotalColor *= vec4(EnvironmentColor.rgb, 1.0);"));
		Out.Log(TEXT("}"));
#endif

		//TotalColor=clamp(TotalColor,0.0,1.0); //saturate.
		Out.Logf(TEXT("if ((vPolyFlags & %u) != %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("{"));
		// Gamma
		Out.Logf(TEXT("float InGamma = 1.0 / (vGamma * %f);"), GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier);
		Out.Log(TEXT("TotalColor.r = pow(TotalColor.r, InGamma);"));
		Out.Log(TEXT("TotalColor.g = pow(TotalColor.g, InGamma);"));
		Out.Log(TEXT("TotalColor.b = pow(TotalColor.b, InGamma);"));
		Out.Log(TEXT("}"));

		Out.Logf(TEXT("if ((vPolyFlags & %u) != %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("TotalColor = TotalColor * LightColor;"));
		//Out.Log(TEXT("else TotalColor = TotalColor;"));
		Out.Log(TEXT("TotalColor += FogColor;"));

		// Add DistanceFog, needs to be added after Light has been applied.
#if ENGINE_VERSION==227
		// stijn: Very slow! Went from 135 to 155FPS on CTF-BT-CallousV3 by just disabling this branch even tho 469 doesn't do distance fog
		Out.Log(TEXT("int FogMode = int(vDistanceFogInfo.w);"));
		Out.Log(TEXT("if (FogMode >= 0)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("FogParameters DistanceFogParams;"));
		Out.Log(TEXT("DistanceFogParams.FogStart = vDistanceFogInfo.x;"));
		Out.Log(TEXT("DistanceFogParams.FogEnd = vDistanceFogInfo.y;"));
		Out.Log(TEXT("DistanceFogParams.FogDensity = vDistanceFogInfo.z;"));
		Out.Log(TEXT("DistanceFogParams.FogMode = FogMode;"));

		Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("DistanceFogParams.FogColor = vec4(0.5, 0.5, 0.5, 0.0);"));
		Out.Logf(TEXT("else if ((vPolyFlags & %u) == %u && (vPolyFlags & %u) != %u)"), PF_Translucent, PF_Translucent, PF_Environment, PF_Environment);
		Out.Log(TEXT("DistanceFogParams.FogColor = vec4(0.0, 0.0, 0.0, 0.0);"));
		Out.Log(TEXT("else DistanceFogParams.FogColor = vDistanceFogColor;"));

		Out.Log(TEXT("DistanceFogParams.FogCoord = abs(vEyeSpacePos.z / vEyeSpacePos.w);"));
		Out.Log(TEXT("TotalColor = mix(TotalColor, DistanceFogParams.FogColor, getFogFactor(DistanceFogParams));"));
		Out.Log(TEXT("}"));
#endif
		if (GIsEditor)
		{
			// Editor support.
			Out.Logf(TEXT("if (vRendMap == %u || vRendMap == %u || vRendMap == %u)"), REN_Zones, REN_PolyCuts, REN_Polys);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor += 0.5;"));
			Out.Log(TEXT("TotalColor *= vDrawColor;"));
			Out.Log(TEXT("}"));
#if ENGINE_VERSION==227
			Out.Logf(TEXT("else if (vRendMap == %u)"), REN_Normals); //Thank you han!
			Out.Log(TEXT("{"));
			// Dot.
			Out.Log(TEXT("float T = 0.5 * dot(normalize(vCoords), vSurfaceNormal);"));
			// Selected.
			Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Selected, PF_Selected);
			Out.Log(TEXT("TotalColor = vec4(0.0, 0.0, abs(T), 1.0);"));
			// Normal.
			Out.Log(TEXT("else TotalColor = vec4(max(0.0, T), max(0.0, -T), 0.0, 1.0);"));
			Out.Log(TEXT("}"));
#endif

			Out.Logf(TEXT("else if (vRendMap == %u)"), REN_PlainTex);
			Out.Log(TEXT("TotalColor = Color;"));

#if ENGINE_VERSION==227
			Out.Logf(TEXT("if ((vRendMap != %u) && ((vPolyFlags & %u) == %u))"), REN_Normals, PF_Selected, PF_Selected);
#else
			Out.Logf(TEXT("if ((vPolyFlags & %u) == %u)"), PF_Selected, PF_Selected);
#endif
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor.r = (TotalColor.r * 0.75);"));
			Out.Log(TEXT("TotalColor.g = (TotalColor.g * 0.75);"));
			Out.Log(TEXT("TotalColor.b = (TotalColor.b * 0.75) + 0.1;"));
			Out.Log(TEXT("TotalColor = clamp(TotalColor, 0.0, 1.0);"));
			Out.Log(TEXT("if (TotalColor.a < 0.5)"));
			Out.Log(TEXT("TotalColor.a = 0.51;"));
			Out.Log(TEXT("}"));

			// HitSelection, Zoneview etc.
			Out.Log(TEXT("if (bHitTesting)"));
			Out.Log(TEXT("TotalColor = vDrawColor;")); // Use ONLY DrawColor.
		}

		if (GL->SimulateMultiPass)
		{
			Out.Log(TEXT("FragColor = TotalColor;"));
			Out.Log(TEXT("FragColor1 = ((vec4(1.0) - TotalColor) * LightColor);"));
		}
		else Out.Log(TEXT("FragColor = TotalColor;"));

		Out.Log(TEXT("}"));
	}
#else
	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		//FragColor = GetTexel(TexNum[0], Texture0, vTexCoords);   
		Out.Log(TEXT("FragColor = texture(sampler2D(Textures[TexNum[0]]), vTexCoords);"));
		Out.Log(TEXT("}"));
	}
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
static void Create_DrawComplexSinglePass_Vert(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(location = 0) in vec4 Coords;")); // == gl_Vertex
	Out.Log(TEXT("layout(location = 1) in vec4 Normal;")); // == gl_Vertex

	Out.Log(TEXT("out vec3 vCoords;"));
	Out.Log(TEXT("out vec4 vEyeSpacePos;"));
	Out.Log(TEXT("out vec2 vTexCoords;"));
	Out.Log(TEXT("out vec2 vLightMapCoords;"));
	Out.Log(TEXT("out vec2 vFogMapCoords;"));
	Out.Log(TEXT("out vec3 vTangentViewPos;"));
	Out.Log(TEXT("out vec3 vTangentFragPos;"));
	if (GL->DetailTextures)
		Out.Log(TEXT("out vec2 vDetailTexCoords;"));
	if (GL->MacroTextures)
		Out.Log(TEXT("out vec2 vMacroTexCoords;"));
	if (GL->BumpMaps)
		Out.Log(TEXT("out vec2 vBumpTexCoords;"));
#if ENGINE_VERSION==227
	Out.Log(TEXT("out vec2 vEnvironmentTexCoords;"));
#endif
	if (GIsEditor)
		Out.Log(TEXT("flat out vec3 vSurfaceNormal;")); // f.e. Render normal view.
#if ENGINE_VERSION!=227
	if (GL->BumpMaps)
#endif
	{
		Out.Log(TEXT("flat out mat3 vTBNMat;"));
	}

	if (GL->UsingShaderDrawParameters)
	{
		Out.Log(TEXT("struct DrawComplexShaderDrawParams"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec4 DiffuseUV;"));	// 0
		Out.Log(TEXT("vec4 LightMapUV;"));	// 1
		Out.Log(TEXT("vec4 FogMapUV;"));	// 2
		Out.Log(TEXT("vec4 DetailUV;"));	// 3
		Out.Log(TEXT("vec4 MacroUV;"));		// 4
		Out.Log(TEXT("vec4 EnviroMapUV;"));	// 5
		Out.Log(TEXT("vec4 DiffuseInfo;"));	// 6
		Out.Log(TEXT("vec4 MacroInfo;"));	// 7
		Out.Log(TEXT("vec4 BumpMapInfo;"));	// 8
		Out.Log(TEXT("vec4 HeightMapInfo;"));// 9
		Out.Log(TEXT("vec4 XAxis;"));		// 10
		Out.Log(TEXT("vec4 YAxis;"));		// 11
		Out.Log(TEXT("vec4 ZAxis;"));		// 12
		Out.Log(TEXT("vec4 DrawColor;"));	// 13
		Out.Log(TEXT("vec4 DistanceFogColor;"));// 14
		Out.Log(TEXT("vec4 DistanceFogInfo;"));// 15
		Out.Log(TEXT("uvec4 TexNum[4];"));
		Out.Log(TEXT("uvec4 DrawFlags;"));
		Out.Log(TEXT("};"));

		Out.Log(TEXT("layout(std430, binding = 6) buffer AllDrawComplexShaderDrawParams"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("DrawComplexShaderDrawParams DrawComplexParams[];"));
		Out.Log(TEXT("};"));

		Out.Log(TEXT("flat out uint vTexNum;"));
		Out.Log(TEXT("flat out uint vLightMapTexNum;"));
		Out.Log(TEXT("flat out uint vFogMapTexNum;"));
		Out.Log(TEXT("flat out uint vDetailTexNum;"));
		Out.Log(TEXT("flat out uint vMacroTexNum;"));
		Out.Log(TEXT("flat out uint vBumpMapTexNum;"));
		Out.Log(TEXT("flat out uint vEnviroMapTexNum;"));
		Out.Log(TEXT("flat out uint vHeightMapTexNum;"));
		Out.Log(TEXT("flat out uint vHeightMapNum;"));
		Out.Log(TEXT("flat out uint vDrawFlags;"));
		Out.Log(TEXT("flat out uint vTextureFormat;"));
		Out.Log(TEXT("flat out uint vPolyFlags;"));
		Out.Log(TEXT("flat out float vBaseDiffuse;"));
		Out.Log(TEXT("flat out float vBaseAlpha;"));
		Out.Log(TEXT("flat out float vParallaxScale;"));
		Out.Log(TEXT("flat out float vGamma;"));
		Out.Log(TEXT("flat out float vBumpMapSpecular;"));
		Out.Log(TEXT("flat out float vTimeSeconds;"));

		if (GIsEditor)
		{
			Out.Log(TEXT("flat out uint vHitTesting;"));
			Out.Log(TEXT("flat out uint vRendMap;"));
			Out.Log(TEXT("flat out vec4 vDrawColor;"));
		}
		Out.Log(TEXT("flat out vec4 vDistanceFogColor;"));
		Out.Log(TEXT("flat out vec4 vDistanceFogInfo;"));
	}
	else
	{
		Out.Log(TEXT("uniform vec4 TexCoords[16];"));
		Out.Log(TEXT("uniform uint TexNum[16];"));
		Out.Log(TEXT("uniform uint DrawFlags[4];"));
	}
	if (GL->SupportsClipDistance)
		Out.Logf(TEXT("out float gl_ClipDistance[%i];"), GL->MaxClippingPlanes);

#if 1
	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		if (GL->UsingShaderDrawParameters)
		{
			Out.Log(TEXT("vec4 XAxis = DrawComplexParams[gl_DrawID].XAxis;"));
			Out.Log(TEXT("vec4 YAxis = DrawComplexParams[gl_DrawID].YAxis;"));
			Out.Log(TEXT("vec4 ZAxis = DrawComplexParams[gl_DrawID].ZAxis;"));
			Out.Log(TEXT("vec4 DiffuseUV = DrawComplexParams[gl_DrawID].DiffuseUV;"));
			Out.Log(TEXT("vec4 LightMapUV = DrawComplexParams[gl_DrawID].LightMapUV;"));
			Out.Log(TEXT("vec4 FogMapUV = DrawComplexParams[gl_DrawID].FogMapUV;"));
			Out.Log(TEXT("vec4 DetailUV = DrawComplexParams[gl_DrawID].DetailUV;"));
			Out.Log(TEXT("vec4 MacroUV = DrawComplexParams[gl_DrawID].MacroUV;"));
			Out.Log(TEXT("vec4 EnviroMapUV = DrawComplexParams[gl_DrawID].EnviroMapUV;"));

			Out.Log(TEXT("vDrawFlags = DrawComplexParams[gl_DrawID].DrawFlags[0];"));
			Out.Log(TEXT("vTextureFormat = DrawComplexParams[gl_DrawID].DrawFlags[1];"));
			Out.Log(TEXT("vPolyFlags = DrawComplexParams[gl_DrawID].DrawFlags[2];"));

			if (GIsEditor)
			{
				Out.Log(TEXT("vRendMap = DrawComplexParams[gl_DrawID].DrawFlags[3];"));
				Out.Log(TEXT("vHitTesting = DrawComplexParams[gl_DrawID].TexNum[3].x;"));
				Out.Log(TEXT("vDrawColor = DrawComplexParams[gl_DrawID].DrawColor;"));
			}

			Out.Log(TEXT("vTexNum = DrawComplexParams[gl_DrawID].TexNum[0].x;"));
			Out.Log(TEXT("vLightMapTexNum = DrawComplexParams[gl_DrawID].TexNum[0].y;"));
			Out.Log(TEXT("vFogMapTexNum = DrawComplexParams[gl_DrawID].TexNum[0].z;"));
			Out.Log(TEXT("vDetailTexNum = DrawComplexParams[gl_DrawID].TexNum[0].w;"));
			Out.Log(TEXT("vMacroTexNum = DrawComplexParams[gl_DrawID].TexNum[1].x;"));
			Out.Log(TEXT("vBumpMapTexNum = DrawComplexParams[gl_DrawID].TexNum[1].y;"));
			Out.Log(TEXT("vEnviroMapTexNum = DrawComplexParams[gl_DrawID].TexNum[1].z;"));
			Out.Log(TEXT("vHeightMapTexNum = DrawComplexParams[gl_DrawID].TexNum[1].w;"));
			Out.Log(TEXT("vBaseDiffuse = DrawComplexParams[gl_DrawID].DiffuseInfo.x;"));
			Out.Log(TEXT("vBaseAlpha = DrawComplexParams[gl_DrawID].DiffuseInfo.z;"));
			Out.Log(TEXT("vBumpMapSpecular = DrawComplexParams[gl_DrawID].BumpMapInfo.y;"));
			Out.Log(TEXT("vParallaxScale = DrawComplexParams[gl_DrawID].HeightMapInfo.z * 0.025;"));
			Out.Log(TEXT("vTimeSeconds = DrawComplexParams[gl_DrawID].HeightMapInfo.w;"));
			Out.Log(TEXT("vGamma = ZAxis.w;"));
			Out.Log(TEXT("vDistanceFogColor = DrawComplexParams[gl_DrawID].DistanceFogColor;"));
			Out.Log(TEXT("vDistanceFogInfo = DrawComplexParams[gl_DrawID].DistanceFogInfo;"));
		}
		else
		{
			Out.Logf(TEXT("vec4 XAxis = TexCoords[%u];"), DCTI_X_AXIS);
			Out.Logf(TEXT("vec4 YAxis = TexCoords[%u];"), DCTI_Y_AXIS);
			Out.Logf(TEXT("vec4 ZAxis = TexCoords[%u];"), DCTI_Z_AXIS);
			Out.Logf(TEXT("vec4 DiffuseUV = TexCoords[%u];"), DCTI_DIFFUSE_COORDS);
			Out.Logf(TEXT("vec4 LightMapUV = TexCoords[%u];"), DCTI_LIGHTMAP_COORDS);
			Out.Logf(TEXT("vec4 FogMapUV = TexCoords[%u];"), DCTI_FOGMAP_COORDS);
			Out.Logf(TEXT("vec4 DetailUV = TexCoords[%u];"), DCTI_DETAIL_COORDS);
			Out.Logf(TEXT("vec4 MacroUV = TexCoords[%u];"), DCTI_MACRO_COORDS);
			Out.Logf(TEXT("vec4 EnviroMapUV = TexCoords[%u];"), DCTI_ENVIROMAP_COORDS);

			Out.Log(TEXT("uint vDrawFlags = DrawFlags[0];"));
		}
		// Point Coords
		Out.Log(TEXT("vCoords = Coords.xyz;"));

		// UDot/VDot calculation.
		Out.Log(TEXT("vec3 MapCoordsXAxis = XAxis.xyz;"));
		Out.Log(TEXT("vec3 MapCoordsYAxis = YAxis.xyz;"));

#if ENGINE_VERSION!=227
		if (GIsEditor || GL->BumpMaps)
#endif
		{
			Out.Log(TEXT("vec3 MapCoordsZAxis = ZAxis.xyz;"));
		}
		Out.Log(TEXT("float UDot = XAxis.w;"));
		Out.Log(TEXT("float VDot = YAxis.w;"));

		Out.Log(TEXT("float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;"));
		Out.Log(TEXT("float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;"));
		Out.Log(TEXT("vec2  MapDot = vec2(MapDotU, MapDotV);"));

		//Texture UV to fragment
		Out.Log(TEXT("vec2 TexMapMult = DiffuseUV.xy;"));
		Out.Log(TEXT("vec2 TexMapPan = DiffuseUV.zw;"));
		Out.Log(TEXT("vTexCoords = (MapDot - TexMapPan) * TexMapMult;"));

		//Texture UV Lightmap to fragment
		Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_LightMap, DF_LightMap);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec2 LightMapMult = LightMapUV.xy;"));
		Out.Log(TEXT("vec2 LightMapPan = LightMapUV.zw;"));
		Out.Log(TEXT("vLightMapCoords = (MapDot - LightMapPan) * LightMapMult;"));
		Out.Log(TEXT("}"));

		// Texture UV FogMap
		Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_FogMap, DF_FogMap);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec2 FogMapMult = FogMapUV.xy;"));
		Out.Log(TEXT("vec2 FogMapPan = FogMapUV.zw;"));
		Out.Log(TEXT("vFogMapCoords = (MapDot - FogMapPan) * FogMapMult;"));
		Out.Log(TEXT("}"));

		// Texture UV DetailTexture
		if (GL->DetailTextures)
		{
			Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_DetailTexture, DF_DetailTexture);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec2 DetailMult = DetailUV.xy;"));
			Out.Log(TEXT("vec2 DetailPan = DetailUV.zw;"));
			Out.Log(TEXT("vDetailTexCoords = (MapDot - DetailPan) * DetailMult;"));
			Out.Log(TEXT("}"));
		}

		// Texture UV Macrotexture
		if (GL->MacroTextures)
		{
			Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_MacroTexture, DF_MacroTexture);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec2 MacroMult = MacroUV.xy;"));
			Out.Log(TEXT("vec2 MacroPan = MacroUV.zw;"));
			Out.Log(TEXT("vMacroTexCoords = (MapDot - MacroPan) * MacroMult;"));
			Out.Log(TEXT("}"));
		}

		// Texture UV EnvironmentMap
#if ENGINE_VERSION==227
		Out.Logf(TEXT("if ((vDrawFlags & %u) == %u)"), DF_EnvironmentMap, DF_EnvironmentMap);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec2 EnvironmentMapMult = EnviroMapUV.xy;"));
		Out.Log(TEXT("vec2 EnvironmentMapPan = EnviroMapUV.zw;"));
		Out.Log(TEXT("vEnvironmentTexCoords = (MapDot - EnvironmentMapPan) * EnvironmentMapMult;"));
		Out.Log(TEXT("}"));
#endif

#if ENGINE_VERSION!=227
		if (GL->BumpMaps)
#endif
		{
			Out.Log(TEXT("vEyeSpacePos = modelviewMat * vec4(Coords.xyz, 1.0);"));
			Out.Log(TEXT("vec3 EyeSpacePos = normalize(FrameCoords[0].xyz);")); // despite pretty perfect results (so far) this still seems somewhat wrong to me.
			Out.Logf(TEXT("if ((vDrawFlags & %u) != 0u)"), (DF_MacroTexture | DF_BumpMap));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec3 T = normalize(vec3(MapCoordsXAxis.x, MapCoordsXAxis.y, MapCoordsXAxis.z));"));
			Out.Log(TEXT("vec3 B = normalize(vec3(MapCoordsYAxis.x, MapCoordsYAxis.y, MapCoordsYAxis.z));"));
			Out.Log(TEXT("vec3 N = normalize(vec3(MapCoordsZAxis.x, MapCoordsZAxis.y, MapCoordsZAxis.z));")); //SurfNormals.

			// TBN must have right handed coord system.
			//if (dot(cross(N, T), B) < 0.0)
			//   T = T * -1.0;
			Out.Log(TEXT("vTBNMat = transpose(mat3(T, B, N));"));

			Out.Log(TEXT("vTangentViewPos = vTBNMat * EyeSpacePos.xyz;"));
			Out.Log(TEXT("vTangentFragPos = vTBNMat * Coords.xyz;"));
			Out.Log(TEXT("}"));
		}
		if (GIsEditor)
			Out.Log(TEXT("vSurfaceNormal = MapCoordsZAxis;"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Coords.xyz, 1.0);"));

		if (GL->SupportsClipDistance)
		{
			Out.Log(TEXT("uint ClipIndex = uint(ClipParams.x);"));
			Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, Coords.xyz);"));
		}
		Out.Log(TEXT("}"));
	}
#else
	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vEyeSpacePos = modelviewMat * vec4(Coords.xyz, 1.0);"));

		// Point Coords
		Out.Log(TEXT("vCoords = Coords.xyz;"));

		Out.Log(TEXT("vTexCoords = vec2(0.0, 0.0);"));
#if BINDLESSTEXTURES
		//	BaseTexNum = uint(TexNums0.x);
#endif
		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Coords.xyz, 1.0);"));
		Out.Log(TEXT("}"));
	}
#endif
}

// Fragmentshader for DrawGouraudPolygon, in 227 also DrawGouraudPolygonList.
static void Create_DrawGouraud_Frag(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("uniform sampler2D Texture0;")); // Base Texture
	Out.Log(TEXT("uniform sampler2D Texture1;")); // DetailTexture
	Out.Log(TEXT("uniform sampler2D Texture2;")); // BumpMap
	Out.Log(TEXT("uniform sampler2D Texture3;")); // MacroTex

	Out.Log(TEXT("in vec3 gCoords;"));
	Out.Log(TEXT("in vec2 gTexCoords;")); // TexCoords
	Out.Log(TEXT("in vec2 gDetailTexCoords;"));
	Out.Log(TEXT("in vec2 gMacroTexCoords;"));
	Out.Log(TEXT("in vec4 gNormals;"));
	Out.Log(TEXT("in vec4 gEyeSpacePos;"));
	Out.Log(TEXT("in vec4 gLightColor;"));
	Out.Log(TEXT("in vec4 gFogColor;")); //VertexFog

	Out.Log(TEXT("flat in vec3 gTextureInfo;")); // diffuse, alpha, bumpmap specular
	Out.Log(TEXT("flat in uint gTexNum;"));
	Out.Log(TEXT("flat in uint gDetailTexNum;"));
	Out.Log(TEXT("flat in uint gBumpTexNum;"));
	Out.Log(TEXT("flat in uint gMacroTexNum;"));
	Out.Log(TEXT("flat in uint gDrawFlags;"));
	Out.Log(TEXT("flat in uint gTextureFormat;"));
	Out.Log(TEXT("flat in uint gPolyFlags;"));
	Out.Log(TEXT("flat in float gGamma;"));
	Out.Log(TEXT("flat in vec4 gDistanceFogColor;"));
	Out.Log(TEXT("flat in vec4 gDistanceFogInfo;"));
	Out.Log(TEXT("in vec3 gTangentViewPos;"));
	Out.Log(TEXT("in vec3 gTangentFragPos;"));
	Out.Log(TEXT("in mat3 gTBNMat;"));

	if (GIsEditor)
	{
		Out.Log(TEXT("flat in vec4 gDrawColor;"));
		Out.Log(TEXT("flat in uint gRendMap;"));
		Out.Log(TEXT("flat in uint gHitTesting;"));
	}
	if (GL->OpenGLVersion == GL_ES)
	{
		Out.Log(TEXT("layout(location = 0) out vec4 FragColor;"));
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 1) out vec4 FragColor1;"));
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 0, index = 1) out vec4 FragColor1;"));
		Out.Log(TEXT("layout(location = 0, index = 0) out vec4 FragColor;"));
	}

	{
		Out.Log(TEXT("vec3 rgb2hsv(vec3 c)"));
		Out.Log(TEXT("{"));
		// some nice stuff from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
		Out.Log(TEXT("vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);"));
		Out.Log(TEXT("vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);"));
		Out.Log(TEXT("vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);"));

		Out.Log(TEXT("float d = q.x - min(q.w, q.y);"));
		Out.Log(TEXT("float e = 1.0e-10;"));
		Out.Log(TEXT("return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);"));
		Out.Log(TEXT("}"));
	}
	{
		Out.Log(TEXT("vec3 hsv2rgb(vec3 c)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);"));
		Out.Log(TEXT("vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);"));
		Out.Log(TEXT("return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);"));
		Out.Log(TEXT("}"));
	}
	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz);")); // TransformPointBy...
		Out.Log(TEXT("mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);"));
		Out.Log(TEXT("vec4 TotalColor = vec4(0.0, 0.0, 0.0, 0.0);"));

		Out.Log(TEXT("int NumLights = int(LightData4[0].y);"));
		Out.Log(TEXT("vec4 Color = GetTexel(uint(gTexNum), Texture0, gTexCoords);"));

		Out.Log(TEXT("if (gTextureInfo.x > 0.0)"));
		Out.Log(TEXT("Color *= gTextureInfo.x;")); // Diffuse factor.
		Out.Log(TEXT("if (gTextureInfo.y > 0.0)"));
		Out.Log(TEXT("Color.a *= gTextureInfo.y;")); // Alpha.
		Out.Logf(TEXT("if (gTextureFormat == %u)"), TEXF_BC5); // BC5 (GL_COMPRESSED_RG_RGTC2) compression
		Out.Log(TEXT("Color.b = sqrt(1.0 - Color.r * Color.r + Color.g * Color.g);"));

		Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_AlphaBlend, PF_AlphaBlend);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("Color.a *= gLightColor.a;")); // Alpha.
		Out.Log(TEXT("if (Color.a < 0.01)"));
		Out.Log(TEXT("discard;"));
		Out.Log(TEXT("}"));

		// Handle PF_Masked.
		Out.Logf(TEXT("else if ((gPolyFlags & %u) == %u)"), PF_Masked, PF_Masked);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("if (Color.a < 0.5)"));
		Out.Log(TEXT("discard;"));
		Out.Log(TEXT("else Color.rgb /= Color.a;"));
		Out.Log(TEXT("}"));

		Out.Log(TEXT("vec4 LightColor;"));

		if (GL->UseHWLighting)
		{
			Out.Log(TEXT("float LightAdd = 0.0f;"));
			Out.Log(TEXT("vec4 TotalAdd = vec4(0.0, 0.0, 0.0, 0.0);"));

			Out.Log(TEXT("for (int i = 0; i < NumLights; i++)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("float WorldLightRadius = LightData4[i].x;"));
			Out.Log(TEXT("float LightRadius = LightData2[i].w;"));
			Out.Log(TEXT("float RWorldLightRadius = WorldLightRadius * WorldLightRadius;"));

			Out.Log(TEXT("vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords);")); // Frame->Coords.
			Out.Log(TEXT("float dist = distance(gCoords, InLightPos);"));

			Out.Log(TEXT("if (dist < RWorldLightRadius)"));
			Out.Log(TEXT("{"));
			// Light color
			Out.Log(TEXT("vec3 RGBColor = vec3(LightData1[i].x, LightData1[i].y, LightData1[i].z);"));
			constexpr FLOAT MinLight = 0.025;
			Out.Logf(TEXT("float b = WorldLightRadius / (RWorldLightRadius * %f);"), MinLight);
			Out.Log(TEXT("float attenuation = WorldLightRadius / (dist + b * dist * dist);"));
			//float attenuation = 0.82*(1.0-smoothstep(LightRadius,24.0*LightRadius+0.50,dist));
			Out.Log(TEXT("TotalAdd += (vec4(RGBColor, 1.0) * attenuation);"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("LightColor = TotalAdd;"));
		}
		else Out.Log(TEXT("LightColor = gLightColor;"));

		// Handle PF_RenderFog.
		Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_RenderFog, PF_RenderFog);
		Out.Log(TEXT("{"));
		// Handle PF_RenderFog|PF_Modulated.
		Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("{"));
		// Compute delta to modulation identity.
		Out.Log(TEXT("vec3 Delta = vec3(0.5) - Color.xyz;"));
		// Reduce delta by (squared) fog intensity.
		//Delta *= 1.0 - sqrt(gFogColor.a);
		Out.Log(TEXT("Delta *= 1.0 - gFogColor.a;"));
		Out.Log(TEXT("Delta *= vec3(1.0) - gFogColor.rgb;"));

		Out.Log(TEXT("TotalColor = vec4(vec3(0.5) - Delta, Color.a);"));
		Out.Log(TEXT("}"));
		Out.Log(TEXT("else")); // Normal.
		Out.Log(TEXT("{"));
		Out.Log(TEXT("Color *= LightColor;"));
		//TotalColor=mix(Color, vec4(gFogColor.xyz,1.0), gFogColor.w);
		Out.Log(TEXT("TotalColor.rgb = Color.rgb * (1.0 - gFogColor.a) + gFogColor.rgb;"));
		Out.Log(TEXT("TotalColor.a = Color.a;"));
		Out.Log(TEXT("}"));
		Out.Log(TEXT("}"));
		Out.Logf(TEXT("else if ((gPolyFlags & %u) == %u)"), PF_Modulated, PF_Modulated); // No Fog.
		Out.Log(TEXT("{"));
		Out.Log(TEXT("TotalColor = Color;"));
		Out.Log(TEXT("}"));
		Out.Log(TEXT("else"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("TotalColor = Color * vec4(LightColor.rgb, 1.0);"));
		Out.Log(TEXT("}"));

		if (GL->DetailTextures)
		{
			Out.Logf(TEXT("if (((gDrawFlags & %u) == %u))"), DF_DetailTexture, DF_DetailTexture);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("float NearZ = gCoords.z / 512.0;"));
			Out.Log(TEXT("float DetailScale = 1.0;"));
			Out.Log(TEXT("float bNear;"));
			Out.Log(TEXT("vec4 DetailTexColor;"));
			Out.Log(TEXT("vec3 hsvDetailTex;"));

			Out.Logf(TEXT("for (int i = 0; i < %i; ++i)"), GL->DetailMax);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("if (i > 0)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("NearZ *= 4.223f;"));
			Out.Log(TEXT("DetailScale *= 4.223f;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("bNear = clamp(0.65 - NearZ, 0.0, 1.0);"));
			Out.Log(TEXT("if (bNear > 0.0)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("DetailTexColor = GetTexel(gDetailTexNum, Texture1, gDetailTexCoords * DetailScale);"));

			Out.Log(TEXT("vec3 hsvDetailTex = rgb2hsv(DetailTexColor.rgb);")); // cool idea Han :)
			Out.Log(TEXT("hsvDetailTex.b += (DetailTexColor.r - 0.1);"));
			Out.Log(TEXT("hsvDetailTex = hsv2rgb(hsvDetailTex);"));
			Out.Log(TEXT("DetailTexColor = vec4(hsvDetailTex, 0.0);"));
			Out.Log(TEXT("DetailTexColor = mix(vec4(1.0, 1.0, 1.0, 1.0), DetailTexColor, bNear);")); //fading out.
			Out.Log(TEXT("TotalColor.rgb *= DetailTexColor.rgb;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
		}
		if (GL->MacroTextures)
		{
			Out.Logf(TEXT("if ((gDrawFlags & %u) == %u && (gDrawFlags & %u) != %u)"), DF_MacroTexture, DF_MacroTexture, DF_BumpMap, DF_BumpMap);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec4 MacroTexColor = GetTexel(gMacroTexNum, Texture3, gMacroTexCoords);"));
			Out.Log(TEXT("vec3 hsvMacroTex = rgb2hsv(MacroTexColor.rgb);"));
			Out.Log(TEXT("hsvMacroTex.b += (MacroTexColor.r - 0.1);"));
			Out.Log(TEXT("hsvMacroTex = hsv2rgb(hsvMacroTex);"));
			Out.Log(TEXT("MacroTexColor = vec4(hsvMacroTex, 1.0);"));
			Out.Log(TEXT("TotalColor *= MacroTexColor;"));
			Out.Log(TEXT("}"));
		}
		if (GL->BumpMaps)
		{
			// BumpMap
			Out.Logf(TEXT("if ((gDrawFlags & %u) == %u)"), DF_BumpMap, DF_BumpMap);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec3 TangentViewDir = normalize(gTangentViewPos - gTangentFragPos);"));
			//normal from normal map
			Out.Log(TEXT("vec3 TextureNormal = GetTexel(gBumpTexNum, Texture2, gTexCoords).rgb * 2.0 - 1.0;"));
			Out.Log(TEXT("vec3 BumpColor;"));
			Out.Log(TEXT("vec3 TotalBumpColor = vec3(0.0, 0.0, 0.0);"));

			Out.Log(TEXT("for (int i = 0; i < NumLights; ++i)"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec3 CurrentLightColor = vec3(LightData1[i].x, LightData1[i].y, LightData1[i].z);"));

			Out.Log(TEXT("float NormalLightRadius = LightData5[i].x;"));
			Out.Log(TEXT("bool bZoneNormalLight = bool(LightData5[i].y);"));
			Out.Log(TEXT("float LightBrightness = LightData5[i].z / 255.0;"));
			Out.Log(TEXT("if (NormalLightRadius == 0.0)"));
			Out.Log(TEXT("NormalLightRadius = LightData2[i].w * 64.0;"));
			Out.Logf(TEXT("bool bSunlight = (uint(LightData2[i].x) == %u);"), LE_Sunlight);
			Out.Log(TEXT("vec3 InLightPos = ((LightPos[i].xyz - FrameCoords[0].xyz) * InFrameCoords);")); // Frame->Coords.
			Out.Log(TEXT("float dist = distance(gCoords, InLightPos);"));

			constexpr FLOAT MinLight = 0.05;
			Out.Logf(TEXT("float b = NormalLightRadius / (NormalLightRadius * NormalLightRadius * %f);"), MinLight);
			Out.Log(TEXT("float attenuation = NormalLightRadius / (dist + b * dist * dist);"));
			Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_Unlit, PF_Unlit);
			Out.Log(TEXT("InLightPos = vec3(1.0, 1.0, 1.0);")); // no idea whats best here. Arbitrary value based on some tests.

			Out.Log(TEXT("if ((NormalLightRadius == 0.0 || (dist > NormalLightRadius) || (bZoneNormalLight && (LightData4[i].z != LightData4[i].w))) && !bSunlight)")); // Do not consider if not in range or in a different zone.
			Out.Log(TEXT("continue;"));

			Out.Log(TEXT("vec3 TangentLightPos = gTBNMat * InLightPos;"));
			Out.Log(TEXT("vec3 TangentlightDir = normalize(TangentLightPos - gTangentFragPos);"));
			// ambient
			Out.Log(TEXT("vec3 ambient = 0.1 * TotalColor.xyz;"));
			// diffuse
			Out.Log(TEXT("float diff = max(dot(TangentlightDir, TextureNormal), 0.0);"));
			Out.Log(TEXT("vec3 diffuse = diff * TotalColor.xyz;"));
			// specular
			Out.Log(TEXT("vec3 halfwayDir = normalize(TangentlightDir + TangentViewDir);"));
			Out.Log(TEXT("float spec = pow(max(dot(TextureNormal, halfwayDir), 0.0), 8.0);"));
			Out.Log(TEXT("vec3 specular = vec3(0.01) * spec * CurrentLightColor * LightBrightness;"));
			Out.Log(TEXT("TotalBumpColor += (ambient + diffuse + specular) * attenuation;"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("TotalColor += vec4(clamp(TotalBumpColor, 0.0, 1.0), 1.0);"));
			Out.Log(TEXT("}"));
		}
		Out.Logf(TEXT("if ((gPolyFlags & %u) != %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("{"));
		// Gamma
		Out.Logf(TEXT("float InGamma = 1.0 / (gGamma * %f);"), GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier);
		Out.Log(TEXT("TotalColor.r = pow(TotalColor.r, InGamma);"));
		Out.Log(TEXT("TotalColor.g = pow(TotalColor.g, InGamma);"));
		Out.Log(TEXT("TotalColor.b = pow(TotalColor.b, InGamma);"));
		Out.Log(TEXT("}"));

#if ENGINE_VERSION==227
		// Add DistanceFog
		Out.Log(TEXT("if (gDistanceFogInfo.w >= 0.0)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("FogParameters DistanceFogParams;"));
		Out.Log(TEXT("DistanceFogParams.FogStart = gDistanceFogInfo.x;"));
		Out.Log(TEXT("DistanceFogParams.FogEnd = gDistanceFogInfo.y;"));
		Out.Log(TEXT("DistanceFogParams.FogDensity = gDistanceFogInfo.z;"));
		Out.Log(TEXT("DistanceFogParams.FogMode = int(gDistanceFogInfo.w);"));

		Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("DistanceFogParams.FogColor = vec4(0.5, 0.5, 0.5, 0.0);"));
		Out.Logf(TEXT("else if ((gPolyFlags & %u) == %u && (gPolyFlags & %u) != %u)"), PF_Translucent, PF_Translucent, PF_Environment, PF_Environment);
		Out.Log(TEXT("DistanceFogParams.FogColor = vec4(0.0, 0.0, 0.0, 0.0);"));
		Out.Log(TEXT("else DistanceFogParams.FogColor = gDistanceFogColor;"));

		Out.Log(TEXT("DistanceFogParams.FogCoord = abs(gEyeSpacePos.z / gEyeSpacePos.w);"));
		Out.Log(TEXT("TotalColor = mix(TotalColor, DistanceFogParams.FogColor, getFogFactor(DistanceFogParams));"));
		Out.Log(TEXT("}"));
#endif

		if (GIsEditor)
		{
			// Editor support.
			Out.Logf(TEXT("if (gRendMap == %u || gRendMap == %u || gRendMap == %u)"), REN_Zones, REN_PolyCuts, REN_Polys);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor += 0.5;"));
			Out.Log(TEXT("TotalColor *= gDrawColor;"));
			Out.Log(TEXT("}"));
#if ENGINE_VERSION==227
			Out.Logf(TEXT("else if (gRendMap == %u)"), REN_Normals);
			Out.Log(TEXT("{"));
			// Dot.
			Out.Log(TEXT("float T = 0.5 * dot(normalize(gCoords), gNormals.xyz);"));
			// Selected.
			Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_Selected, PF_Selected);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor = vec4(0.0, 0.0, abs(T), 1.0);"));
			Out.Log(TEXT("}"));
			// Normal.
			Out.Log(TEXT("else"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor = vec4(max(0.0, T), max(0.0, -T), 0.0, 1.0);"));
			Out.Log(TEXT("}"));
			Out.Log(TEXT("}"));
#endif
			Out.Logf(TEXT("else if (gRendMap == %u)"), REN_PlainTex);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor = Color;"));
			Out.Log(TEXT("}"));

#if ENGINE_VERSION==227
			Out.Logf(TEXT("if ((gRendMap != %u) && ((gPolyFlags & %u) == %u))"), REN_Normals, PF_Selected, PF_Selected);
#else
			Out.Logf(TEXT("if ((gPolyFlags & %u) == %u)"), PF_Selected, PF_Selected);
#endif
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor.r = (TotalColor.r * 0.75);"));
			Out.Log(TEXT("TotalColor.g = (TotalColor.g * 0.75);"));
			Out.Log(TEXT("TotalColor.b = (TotalColor.b * 0.75) + 0.1;"));
			Out.Log(TEXT("TotalColor = clamp(TotalColor, 0.0, 1.0);"));
			Out.Log(TEXT("if (TotalColor.a < 0.5)"));
			Out.Log(TEXT("TotalColor.a = 0.51;"));
			Out.Log(TEXT("}"));
			// HitSelection, Zoneview etc.
			Out.Log(TEXT("if (bool(gHitTesting))"));
			Out.Log(TEXT("TotalColor = gDrawColor;")); // Use DrawColor.

			Out.Logf(TEXT("if ((gPolyFlags & %u) == %u && gDrawColor.a > 0.0)"), PF_AlphaBlend, PF_AlphaBlend);
			Out.Log(TEXT("TotalColor.a *= gDrawColor.a;"));
		}
		if (GL->SimulateMultiPass)
		{
			Out.Log(TEXT("FragColor = TotalColor;"));
			Out.Log(TEXT("FragColor1 = ((vec4(1.0) - TotalColor * LightColor));")); //no, this is not entirely right, TotalColor has already LightColor applied. But will blow any fog/transparency otherwise. However should not make any (visual) difference here for this equation. Any better idea?
		}
		else Out.Log(TEXT("FragColor = TotalColor;"));
		Out.Log(TEXT("}"));
	}

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
static void Create_DrawGouraud_Vert(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(location = 0) in vec3 Coords;")); // == gl_Vertex
	Out.Log(TEXT("layout(location = 1) in vec3 Normals;")); // Normals
	Out.Log(TEXT("layout(location = 2) in vec2 TexCoords;")); // TexCoords
	Out.Log(TEXT("layout(location = 3) in vec4 LightColor;"));
	Out.Log(TEXT("layout(location = 4) in vec4 FogColor;"));

	if (GL->OpenGLVersion == GL_ES)
	{
		// No geometry shader in GL_ES.
		Out.Log(TEXT("flat out uint gTexNum;"));
		Out.Log(TEXT("flat out uint gDetailTexNum;"));
		Out.Log(TEXT("flat out uint gBumpTexNum;"));
		Out.Log(TEXT("flat out uint gMacroTexNum;"));
		Out.Log(TEXT("flat out uint gDrawFlags;"));
		Out.Log(TEXT("flat out uint gTextureFormat;"));
		Out.Log(TEXT("flat out uint gPolyFlags;"));
		Out.Log(TEXT("flat out float gGamma;"));

		Out.Log(TEXT("flat out vec3 gTextureInfo;")); // diffuse, alpha, bumpmap specular
		Out.Log(TEXT("flat out vec4 gDistanceFogColor;"));
		Out.Log(TEXT("flat out vec4 gDistanceFogInfo;"));

		Out.Log(TEXT("out vec3 gCoords;"));
		Out.Log(TEXT("out vec4 gNormals;"));
		Out.Log(TEXT("out vec2 gTexCoords;"));
		Out.Log(TEXT("out vec2 gDetailTexCoords;"));
		Out.Log(TEXT("out vec2 gMacroTexCoords;"));
		Out.Log(TEXT("out vec4 gEyeSpacePos;"));
		Out.Log(TEXT("out vec4 gLightColor;"));
		Out.Log(TEXT("out vec4 gFogColor;"));
		Out.Log(TEXT("out mat3 gTBNMat;"));
		Out.Log(TEXT("out vec3 gTangentViewPos;"));
		Out.Log(TEXT("out vec3 gTangentFragPos;"));

		if (GIsEditor)
		{
			Out.Log(TEXT("flat out vec4 gDrawColor;"));
			Out.Log(TEXT("flat out uint gRendMap;"));
			Out.Log(TEXT("flat out uint gHitTesting;"));
		}
		Out.Log(TEXT("uniform vec4 DrawData[6];"));
		Out.Log(TEXT("uniform uint TexNum[4];"));
		Out.Log(TEXT("uniform uint DrawFlags[4];"));
	}
	else
	{
		if (GL->UsingShaderDrawParameters)
		{
			Out.Log(TEXT("struct DrawGouraudShaderDrawParams"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("vec4 DiffuseInfo;"));		// 0
			Out.Log(TEXT("vec4 DetailMacroInfo;"));	// 1
			Out.Log(TEXT("vec4 MiscInfo;"));		// 2
			Out.Log(TEXT("vec4 DrawColor;"));		// 3
			Out.Log(TEXT("vec4 DistanceFogColor;"));// 4
			Out.Log(TEXT("vec4 DistanceFogInfo;"));	// 5
			Out.Log(TEXT("uvec4 TexNum;"));
			Out.Log(TEXT("uvec4 DrawFlags;"));
			Out.Log(TEXT("};"));

			Out.Log(TEXT("layout(std430, binding = 7) buffer AllDrawGouraudShaderDrawParams"));
			Out.Log(TEXT("{"));
			Out.Log(TEXT("DrawGouraudShaderDrawParams DrawGouraudParams[];"));
			Out.Log(TEXT("};"));
		}
		else
		{
			Out.Log(TEXT("uniform vec4 DrawData[6];"));
			Out.Log(TEXT("uniform uint TexNum[4];"));
			Out.Log(TEXT("uniform uint DrawFlags[4];"));
		}
		Out.Log(TEXT("flat out uint vTexNum;"));
		Out.Log(TEXT("flat out uint vDetailTexNum;"));
		Out.Log(TEXT("flat out uint vBumpTexNum;"));
		Out.Log(TEXT("flat out uint vMacroTexNum;"));
		Out.Log(TEXT("flat out uint vDrawFlags;"));
		Out.Log(TEXT("flat out uint vTextureFormat;"));
		Out.Log(TEXT("flat out uint vPolyFlags;"));
		Out.Log(TEXT("flat out float vGamma;"));

		Out.Log(TEXT("flat out vec3 vTextureInfo;")); // diffuse, alpha, bumpmap specular
		Out.Log(TEXT("flat out vec4 vDistanceFogColor;"));
		Out.Log(TEXT("flat out vec4 vDistanceFogInfo;"));

		Out.Log(TEXT("out vec3 vCoords;"));
		Out.Log(TEXT("out vec4 vNormals;"));
		Out.Log(TEXT("out vec2 vTexCoords;"));
		Out.Log(TEXT("out vec2 vDetailTexCoords;"));
		Out.Log(TEXT("out vec2 vMacroTexCoords;"));
		Out.Log(TEXT("out vec4 vEyeSpacePos;"));
		Out.Log(TEXT("out vec4 vLightColor;"));
		Out.Log(TEXT("out vec4 vFogColor;"));

		if (GIsEditor)
		{
			Out.Log(TEXT("flat out vec4 vDrawColor;"));
			Out.Log(TEXT("flat out uint vRendMap;"));
			Out.Log(TEXT("flat out uint vHitTesting;"));
		}
	}

	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		if (GL->OpenGLVersion == GL_ES)
		{
			Out.Log(TEXT("gEyeSpacePos = modelviewMat * vec4(Coords, 1.0);"));
			Out.Log(TEXT("gCoords = Coords;"));
			Out.Log(TEXT("gNormals = vec4(Normals.xyz, 0);"));
			Out.Logf(TEXT("gTexCoords = TexCoords * DrawData[%u].xy;"), DGTI_DIFFUSE_INFO); // TODO - REVIEW THESE INDICES!!!
			Out.Logf(TEXT("gDetailTexCoords = TexCoords * DrawData[%u].xy;"), DGTI_DETAIL_MACRO_INFO);
			Out.Logf(TEXT("gMacroTexCoords = TexCoords * DrawData[%u].zw;"), DGTI_DETAIL_MACRO_INFO);
			Out.Log(TEXT("gLightColor = LightColor;"));
			Out.Log(TEXT("gFogColor = FogColor;"));

			Out.Log(TEXT("gTexNum = TexNum[0];"));
			Out.Log(TEXT("gDetailTexNum = TexNum[1];"));
			Out.Log(TEXT("gBumpTexNum = TexNum[2];"));
			Out.Log(TEXT("gMacroTexNum = TexNum[3];"));

			Out.Log(TEXT("gDrawFlags = DrawFlags[0];"));
			Out.Logf(TEXT("gTextureFormat = uint(DrawData[%u].w);"), DGTI_MISC_INFO);
			Out.Log(TEXT("gPolyFlags = DrawFlags[2];"));
			Out.Logf(TEXT("gGamma = DrawData[%u].y;"), DGTI_MISC_INFO);

			Out.Logf(TEXT("gTextureInfo = vec3(DrawData[%u].zw, DrawData[%u].x);"), DGTI_DIFFUSE_INFO, DGTI_MISC_INFO);
			Out.Logf(TEXT("gDistanceFogColor = DrawData[%u];"), DGTI_DISTANCE_FOG_COLOR);
			Out.Logf(TEXT("gDistanceFogInfo = DrawData[%u];"), DGTI_DISTANCE_FOG_INFO);

			if (GIsEditor)
			{
				Out.Log(TEXT("gHitTesting = DrawFlags[1];"));
				Out.Log(TEXT("gRendMap = DrawFlags[3];"));
				Out.Logf(TEXT("gDrawColor = DrawData[%u];"), DGTI_EDITOR_DRAWCOLOR);
			}

			Out.Log(TEXT("vec3 T = vec3(1.0, 1.0, 1.0);")); // Arbitrary.
			Out.Log(TEXT("vec3 B = vec3(1.0, 1.0, 1.0);")); // Replace with actual values extracted from mesh generation some day.
			Out.Log(TEXT("vec3 N = normalize(Normals.xyz);")); // Normals.

			// TBN must have right handed coord system.
			//if (dot(cross(N, T), B) < 0.0)
			//	T = T * -1.0;

			Out.Log(TEXT("gTBNMat = transpose(mat3(T, B, N));"));
			Out.Log(TEXT("gTangentViewPos = gTBNMat * normalize(FrameCoords[0].xyz);"));
			Out.Log(TEXT("gTangentFragPos = gTBNMat * gCoords.xyz;"));

			Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Coords, 1.0);"));
			Out.Log(TEXT(""));

			if (GL->SupportsClipDistance)
			{
				Out.Log(TEXT("uint ClipIndex = uint(ClipParams.x);"));
				Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, gEyeSpacePos.xyz);"));
			}
		}
		else
		{
			Out.Log(TEXT("vEyeSpacePos = modelviewMat * vec4(Coords, 1.0);"));

			Out.Log(TEXT("vCoords = Coords;"));
			Out.Log(TEXT("vNormals = vec4(Normals.xyz, 0);"));
			Out.Log(TEXT("vLightColor = LightColor;"));
			Out.Log(TEXT("vFogColor = FogColor;"));

			if (GL->UsingShaderDrawParameters)
			{
				Out.Log(TEXT("vTexCoords = TexCoords * DrawGouraudParams[gl_DrawID].DiffuseInfo.xy;"));
				Out.Log(TEXT("vDetailTexCoords = TexCoords * DrawGouraudParams[gl_DrawID].DetailMacroInfo.xy;"));
				Out.Log(TEXT("vMacroTexCoords = TexCoords * DrawGouraudParams[gl_DrawID].DetailMacroInfo.zw;"));

				Out.Log(TEXT("vTexNum = DrawGouraudParams[gl_DrawID].TexNum[0];"));
				Out.Log(TEXT("vDetailTexNum = DrawGouraudParams[gl_DrawID].TexNum[1];"));
				Out.Log(TEXT("vBumpTexNum = DrawGouraudParams[gl_DrawID].TexNum[2];"));
				Out.Log(TEXT("vMacroTexNum = DrawGouraudParams[gl_DrawID].TexNum[3];"));

				Out.Log(TEXT("vDrawFlags = DrawGouraudParams[gl_DrawID].DrawFlags[0];"));
				Out.Log(TEXT("vTextureFormat = uint(DrawGouraudParams[gl_DrawID].MiscInfo.z);"));
				Out.Log(TEXT("vPolyFlags = DrawGouraudParams[gl_DrawID].DrawFlags[2];"));
				Out.Log(TEXT("vGamma = DrawGouraudParams[gl_DrawID].MiscInfo.y;"));

				Out.Log(TEXT("vTextureInfo = vec3(DrawGouraudParams[gl_DrawID].DiffuseInfo.zw, DrawGouraudParams[gl_DrawID].MiscInfo.x);"));
				Out.Log(TEXT("vDistanceFogColor = DrawGouraudParams[gl_DrawID].DistanceFogColor;"));
				Out.Log(TEXT("vDistanceFogInfo = DrawGouraudParams[gl_DrawID].DistanceFogInfo;"));

				if (GIsEditor)
				{
					Out.Log(TEXT("vHitTesting = DrawGouraudParams[gl_DrawID].DrawFlags[1];"));
					Out.Log(TEXT("vRendMap = DrawGouraudParams[gl_DrawID].DrawFlags[3];"));
					Out.Log(TEXT("vDrawColor = DrawGouraudParams[gl_DrawID].DrawColor;"));
				}
			}
			else
			{
				Out.Logf(TEXT("vTexCoords = TexCoords * DrawData[%u].xy;"), DGTI_DIFFUSE_INFO);
				Out.Logf(TEXT("vDetailTexCoords = TexCoords * DrawData[%u].xy;"), DGTI_DETAIL_MACRO_INFO);
				Out.Logf(TEXT("vMacroTexCoords = TexCoords * DrawData[%u].zw;"), DGTI_DETAIL_MACRO_INFO);

				Out.Log(TEXT("vTexNum = TexNum[0];"));
				Out.Log(TEXT("vDetailTexNum = TexNum[1];"));
				Out.Log(TEXT("vBumpTexNum = TexNum[2];"));
				Out.Log(TEXT("vMacroTexNum = TexNum[3];"));

				Out.Log(TEXT("vDrawFlags = DrawFlags[0];"));
				Out.Logf(TEXT("vTextureFormat = uint(DrawData[%u].z);"), DGTI_MISC_INFO);
				Out.Log(TEXT("vPolyFlags = DrawFlags[2];"));
				Out.Logf(TEXT("vGamma = DrawData[%u].y;"), DGTI_MISC_INFO);

				Out.Logf(TEXT("vTextureInfo = vec3(DrawData[%u].zw, DrawData[%u].x);"), DGTI_DIFFUSE_INFO, DGTI_MISC_INFO);
				Out.Logf(TEXT("vDistanceFogColor = DrawData[%u];"), DGTI_DISTANCE_FOG_COLOR);
				Out.Logf(TEXT("vDistanceFogInfo = DrawData[%u];"), DGTI_DISTANCE_FOG_INFO);

				if (GIsEditor)
				{
					Out.Log(TEXT("vHitTesting = DrawFlags[1];"));
					Out.Log(TEXT("vRendMap = DrawFlags[3];"));
					Out.Logf(TEXT("vDrawColor = DrawData[%u];"), DGTI_EDITOR_DRAWCOLOR);
				}
			}
		}
		Out.Log(TEXT("gl_Position = vec4(Coords, 1.0);"));
		Out.Log(TEXT("}"));
	}
}

// Geometryshader for DrawGouraud.
static void Create_DrawGouraud_Geo(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(triangles) in;"));
	Out.Log(TEXT("layout(triangle_strip, max_vertices = 3) out;"));

	Out.Log(TEXT("flat in uint vTexNum[];"));
	Out.Log(TEXT("flat in uint vDetailTexNum[];"));
	Out.Log(TEXT("flat in uint vBumpTexNum[];"));
	Out.Log(TEXT("flat in uint vMacroTexNum[];"));
	Out.Log(TEXT("flat in uint vDrawFlags[];"));
	Out.Log(TEXT("flat in uint vTextureFormat[];"));
	Out.Log(TEXT("flat in uint vPolyFlags[];"));
	Out.Log(TEXT("flat in float vGamma[];"));

	Out.Log(TEXT("flat in vec3 vTextureInfo[];")); // diffuse, alpha, bumpmap specular
	Out.Log(TEXT("flat in vec4 vDistanceFogColor[];"));
	Out.Log(TEXT("flat in vec4 vDistanceFogInfo[];"));

	if (GIsEditor)
	{
		Out.Log(TEXT("flat in vec4 vDrawColor[];"));
		Out.Log(TEXT("flat in uint vRendMap[];"));
		Out.Log(TEXT("flat in uint vHitTesting[];"));
	}
	Out.Log(TEXT("in vec3 vCoords[];"));
	Out.Log(TEXT("in vec4 vNormals[];"));
	Out.Log(TEXT("in vec2 vTexCoords[];"));
	Out.Log(TEXT("in vec2 vDetailTexCoords[];"));
	Out.Log(TEXT("in vec2 vMacroTexCoords[];"));
	Out.Log(TEXT("in vec4 vEyeSpacePos[];"));
	Out.Log(TEXT("in vec4 vLightColor[];"));
	Out.Log(TEXT("in vec4 vFogColor[];"));

	Out.Log(TEXT("flat out uint gTexNum;"));
	Out.Log(TEXT("flat out uint gDetailTexNum;"));
	Out.Log(TEXT("flat out uint gBumpTexNum;"));
	Out.Log(TEXT("flat out uint gMacroTexNum;"));
	Out.Log(TEXT("flat out uint gDrawFlags;"));
	Out.Log(TEXT("flat out uint gTextureFormat;"));
	Out.Log(TEXT("flat out uint gPolyFlags;"));
	Out.Log(TEXT("flat out float gGamma;"));

	Out.Log(TEXT("out vec3 gCoords;"));
	Out.Log(TEXT("out vec4 gNormals;"));
	Out.Log(TEXT("out vec2 gTexCoords;"));
	Out.Log(TEXT("out vec2 gDetailTexCoords;"));
	Out.Log(TEXT("out vec2 gMacroTexCoords;"));
	Out.Log(TEXT("out vec4 gEyeSpacePos;"));
	Out.Log(TEXT("out vec4 gLightColor;"));
	Out.Log(TEXT("out vec4 gFogColor;"));

	Out.Log(TEXT("out mat3 gTBNMat;"));
	Out.Log(TEXT("out vec3 gTangentViewPos;"));
	Out.Log(TEXT("out vec3 gTangentFragPos;"));

	Out.Log(TEXT("flat out vec3 gTextureInfo;"));
	Out.Log(TEXT("flat out vec4 gDistanceFogColor;"));
	Out.Log(TEXT("flat out vec4 gDistanceFogInfo;"));

	if (GIsEditor)
	{
		Out.Log(TEXT("flat out vec4 gDrawColor;"));
		Out.Log(TEXT("flat out uint gRendMap;"));
		Out.Log(TEXT("flat out uint gHitTesting;"));
	}
	Out.Logf(TEXT("out float gl_ClipDistance[%i];"), GL->MaxClippingPlanes);

	{
		Out.Log(TEXT("vec3 GetTangent(vec3 A, vec3 B, vec3 C, vec2 Auv, vec2 Buv, vec2 Cuv)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("float Bv_Cv = Buv.y - Cuv.y;"));
		Out.Log(TEXT("if (Bv_Cv == 0.0)"));
		Out.Log(TEXT("return (B - C) / (Buv.x - Cuv.x);"));

		Out.Log(TEXT("float Quotient = (Auv.y - Cuv.y) / (Bv_Cv);"));
		Out.Log(TEXT("vec3 D = C + (B - C) * Quotient;"));
		Out.Log(TEXT("vec2 Duv = Cuv + (Buv - Cuv) * Quotient;"));
		Out.Log(TEXT("return (D - A) / (Duv.x - Auv.x);"));
		Out.Log(TEXT("}"));
	}
	{
		Out.Log(TEXT("vec3 GetBitangent(vec3 A, vec3 B, vec3 C, vec2 Auv, vec2 Buv, vec2 Cuv)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("return GetTangent(A, C, B, Auv.yx, Cuv.yx, Buv.yx);"));
		Out.Log(TEXT("}"));
	}
	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("mat3 InFrameCoords = mat3(FrameCoords[1].xyz, FrameCoords[2].xyz, FrameCoords[3].xyz);")); // TransformPointBy...
		Out.Log(TEXT("mat3 InFrameUncoords = mat3(FrameUncoords[1].xyz, FrameUncoords[2].xyz, FrameUncoords[3].xyz);"));

		Out.Log(TEXT("vec3 Tangent = GetTangent(vCoords[0], vCoords[1], vCoords[2], vTexCoords[0], vTexCoords[1], vTexCoords[2]);"));
		Out.Log(TEXT("vec3 Bitangent = GetBitangent(vCoords[0], vCoords[1], vCoords[2], vTexCoords[0], vTexCoords[1], vTexCoords[2]);"));
		Out.Log(TEXT("uint ClipIndex = uint(ClipParams.x);"));

		Out.Log(TEXT("for (int i = 0; i < 3; ++i)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec3 T = normalize(vec3(modelMat * vec4(Tangent, 0.0)));"));
		Out.Log(TEXT("vec3 B = normalize(vec3(modelMat * vec4(Bitangent, 0.0)));"));
		Out.Log(TEXT("vec3 N = normalize(vec3(modelMat * vNormals[i]));"));

		// TBN must have right handed coord system.
		//if (dot(cross(N, T), B) < 0.0)
		//		T = T * -1.0;
		Out.Log(TEXT("gTBNMat = mat3(T, B, N);"));

		Out.Log(TEXT("gEyeSpacePos = vEyeSpacePos[i];"));
		Out.Log(TEXT("gLightColor = vLightColor[i];"));
		Out.Log(TEXT("gFogColor = vFogColor[i];"));
		Out.Log(TEXT("gNormals = vNormals[i];"));
		Out.Log(TEXT("gTexCoords = vTexCoords[i];"));
		Out.Log(TEXT("gDetailTexCoords = vDetailTexCoords[i];"));
		Out.Log(TEXT("gMacroTexCoords = vMacroTexCoords[i];"));
		Out.Log(TEXT("gCoords = vCoords[i];"));
		Out.Log(TEXT("gTexNum = vTexNum[i];"));
		Out.Log(TEXT("gDetailTexNum = vDetailTexNum[i];"));
		Out.Log(TEXT("gBumpTexNum = vBumpTexNum[i];"));
		Out.Log(TEXT("gMacroTexNum = vMacroTexNum[i];"));
		Out.Log(TEXT("gTextureInfo = vTextureInfo[i];"));
		Out.Log(TEXT("gDrawFlags = vDrawFlags[i];"));
		Out.Log(TEXT("gPolyFlags = vPolyFlags[i];"));
		Out.Log(TEXT("gGamma = vGamma[i];"));
		Out.Log(TEXT("gTextureFormat = vTextureFormat[i];"));
		Out.Log(TEXT("gDistanceFogColor = vDistanceFogColor[i];"));
		Out.Log(TEXT("gDistanceFogInfo = vDistanceFogInfo[i];"));

		Out.Log(TEXT("gTangentViewPos = gTBNMat * normalize(FrameCoords[0].xyz);"));
		Out.Log(TEXT("gTangentFragPos = gTBNMat * gCoords.xyz;"));

		if (GIsEditor)
		{
			Out.Log(TEXT("gDrawColor = vDrawColor[i];"));
			Out.Log(TEXT("gRendMap = vRendMap[i];"));
			Out.Log(TEXT("gHitTesting = vHitTesting[i];"));
		}
		Out.Log(TEXT("gl_Position = modelviewprojMat * gl_in[i].gl_Position;"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[i]);"));
		Out.Log(TEXT("EmitVertex();"));
		Out.Log(TEXT("}"));
		Out.Log(TEXT("}"));
	}
}

// Fragmentshader for DrawTile.
static void Create_DrawTile_Frag(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("uniform uint PolyFlags;"));
	Out.Log(TEXT("uniform bool bHitTesting;"));
	Out.Log(TEXT("uniform float Gamma;"));
	Out.Log(TEXT("uniform vec4 HitDrawColor;"));

	Out.Log(TEXT("in vec2 gTexCoords;"));
	Out.Log(TEXT("flat in vec4 gDrawColor;"));
	Out.Log(TEXT("flat in uint gTexNum;"));

	Out.Log(TEXT("uniform sampler2D Texture0;"));

	if (GL->OpenGLVersion == GL_ES)
	{
		Out.Log(TEXT("layout(location = 0) out vec4 FragColor;"));
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 1) out vec4 FragColor1;"));
	}
	else
	{
		if (GL->SimulateMultiPass)
			Out.Log(TEXT("layout(location = 0, index = 1) out vec4 FragColor1;"));
		Out.Log(TEXT("layout(location = 0, index = 0) out vec4 FragColor;"));
	}

	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("vec4 TotalColor;"));
		Out.Log(TEXT("vec4 Color = GetTexel(gTexNum, Texture0, gTexCoords);"));

		// Handle PF_Masked.
		Out.Logf(TEXT("if ((PolyFlags & %u) == %u)"), PF_Masked, PF_Masked);
		Out.Log(TEXT("{"));
		Out.Log(TEXT("if (Color.a < 0.5)"));
		Out.Log(TEXT("discard;"));
		Out.Log(TEXT("else Color.rgb /= Color.a;"));
		Out.Log(TEXT("}"));
		Out.Logf(TEXT("else if ((PolyFlags & %u) == %u && Color.a < 0.01)"), PF_AlphaBlend, PF_AlphaBlend);
		Out.Log(TEXT("discard;"));

		Out.Log(TEXT("TotalColor = Color * gDrawColor;")); // Add DrawColor.

		Out.Logf(TEXT("if ((PolyFlags & %u) != %u)"), PF_Modulated, PF_Modulated);
		Out.Log(TEXT("{"));
		// Gamma
		Out.Logf(TEXT("float InGamma = 1.0 / (Gamma * %f);"), GIsEditor ? GL->GammaMultiplierUED : GL->GammaMultiplier);
		Out.Log(TEXT("TotalColor.r = pow(TotalColor.r, InGamma);"));
		Out.Log(TEXT("TotalColor.g = pow(TotalColor.g, InGamma);"));
		Out.Log(TEXT("TotalColor.b = pow(TotalColor.b, InGamma);"));
		Out.Log(TEXT("}"));

		if (GIsEditor)
		{
			// Editor support.
			Out.Logf(TEXT("if ((PolyFlags & %u) == %u)"), PF_Selected, PF_Selected);
			Out.Log(TEXT("{"));
			Out.Log(TEXT("TotalColor.g = TotalColor.g - 0.04;"));
			Out.Log(TEXT("TotalColor = clamp(TotalColor, 0.0, 1.0);"));
			Out.Log(TEXT("}"));

			// HitSelection, Zoneview etc.
			Out.Log(TEXT("if (bHitTesting)"));
			Out.Log(TEXT("TotalColor = HitDrawColor;")); // Use HitDrawColor.
		}

		if (GL->SimulateMultiPass)
		{
			Out.Log(TEXT("FragColor = TotalColor;"));
			Out.Log(TEXT("FragColor1 = vec4(1.0, 1.0, 1.0, 1.0) - TotalColor;"));
		}
		else Out.Log(TEXT("FragColor = TotalColor;"));
		Out.Log(TEXT("}"));
	}
}

// Geoshader for DrawTile.
static void Create_DrawTile_Geo(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(triangles) in;"));
	Out.Log(TEXT("layout(triangle_strip, max_vertices = 6) out;"));

	Out.Log(TEXT("flat in uint vTexNum[];"));
	Out.Log(TEXT("in vec4 vTexCoords0[];"));
	Out.Log(TEXT("in vec4 vTexCoords1[];"));
	Out.Log(TEXT("in vec4 vTexCoords2[];"));
	Out.Log(TEXT("flat in vec4 vDrawColor[];"));
	Out.Log(TEXT("in vec4 vEyeSpacePos[];"));
	Out.Log(TEXT("in vec3 vCoords[];"));

	Out.Log(TEXT("out vec2 gTexCoords;"));
	Out.Log(TEXT("flat out vec4 gDrawColor;"));
	Out.Log(TEXT("flat out uint gTexNum;"));
	Out.Logf(TEXT("out float gl_ClipDistance[%i];"), GL->MaxClippingPlanes);

	{
		Out.Log(TEXT("void main()"));
		Out.Log(TEXT("{"));
		Out.Log(TEXT("uint ClipIndex = uint(ClipParams.x);"));
		Out.Log(TEXT("gTexNum = vTexNum[0];"));

		Out.Log(TEXT("float RFX2 = vTexCoords0[0].x;"));
		Out.Log(TEXT("float RFY2 = vTexCoords0[0].y;"));
		Out.Log(TEXT("float FX2 = vTexCoords0[0].z;"));
		Out.Log(TEXT("float FY2 = vTexCoords0[0].w;"));

		Out.Log(TEXT("float U = vTexCoords1[0].x;"));
		Out.Log(TEXT("float V = vTexCoords1[0].y;"));
		Out.Log(TEXT("float UL = vTexCoords1[0].z;"));
		Out.Log(TEXT("float VL = vTexCoords1[0].w;"));

		Out.Log(TEXT("float XL = vTexCoords2[0].x;"));
		Out.Log(TEXT("float YL = vTexCoords2[0].y;"));
		Out.Log(TEXT("float UMult = vTexCoords2[0].z;"));
		Out.Log(TEXT("float VMult = vTexCoords2[0].w;"));

		Out.Log(TEXT("float X = gl_in[0].gl_Position.x;"));
		Out.Log(TEXT("float Y = gl_in[0].gl_Position.y;"));
		Out.Log(TEXT("float Z = gl_in[0].gl_Position.z;"));

		Out.Log(TEXT("vec3 Position;"));

		// 0
		Out.Log(TEXT("Position.x = RFX2 * Z * (X - FX2);"));
		Out.Log(TEXT("Position.y = RFY2 * Z * (Y - FY2);"));
		Out.Log(TEXT("Position.z = Z;"));
		Out.Log(TEXT("gTexCoords.x = (U)*UMult;"));
		Out.Log(TEXT("gTexCoords.y = (V)*VMult;"));

		Out.Log(TEXT("gDrawColor = vDrawColor[0];"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Position, 1.0);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[0]);"));
		Out.Log(TEXT("EmitVertex();"));

		// 1
		Out.Log(TEXT("Position.x = RFX2 * Z * (X + XL - FX2);"));
		Out.Log(TEXT("Position.y = RFY2 * Z * (Y - FY2);"));
		Out.Log(TEXT("Position.z = Z;"));
		Out.Log(TEXT("gTexCoords.x = (U + UL) * UMult;"));
		Out.Log(TEXT("gTexCoords.y = (V)*VMult;"));
		Out.Log(TEXT("gDrawColor = vDrawColor[0];"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Position, 1.0);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[1]);"));
		Out.Log(TEXT("EmitVertex();"));

		// 2
		Out.Log(TEXT("Position.x = RFX2 * Z * (X + XL - FX2);"));
		Out.Log(TEXT("Position.y = RFY2 * Z * (Y + YL - FY2);"));
		Out.Log(TEXT("Position.z = Z;"));
		Out.Log(TEXT("gTexCoords.x = (U + UL) * UMult;"));
		Out.Log(TEXT("gTexCoords.y = (V + VL) * VMult;"));
		Out.Log(TEXT("gDrawColor = vDrawColor[0];"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Position, 1.0);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[2]);"));
		Out.Log(TEXT("EmitVertex();"));
		Out.Log(TEXT("EndPrimitive();"));

		// 0
		Out.Log(TEXT("Position.x = RFX2 * Z * (X - FX2);"));
		Out.Log(TEXT("Position.y = RFY2 * Z * (Y - FY2);"));
		Out.Log(TEXT("Position.z = Z;"));
		Out.Log(TEXT("gTexCoords.x = (U)*UMult;"));
		Out.Log(TEXT("gTexCoords.y = (V)*VMult;"));
		Out.Log(TEXT("gDrawColor = vDrawColor[0];"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Position, 1.0);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[0]);"));
		Out.Log(TEXT("EmitVertex();"));

		// 2
		Out.Log(TEXT("Position.x = RFX2 * Z * (X + XL - FX2);"));
		Out.Log(TEXT("Position.y = RFY2 * Z * (Y + YL - FY2);"));
		Out.Log(TEXT("Position.z = Z;"));
		Out.Log(TEXT("gTexCoords.x = (U + UL) * UMult;"));
		Out.Log(TEXT("gTexCoords.y = (V + VL) * VMult;"));
		Out.Log(TEXT("gDrawColor = vDrawColor[0];"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Position, 1.0);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[1]);"));
		Out.Log(TEXT("EmitVertex();"));

		// 3
		Out.Log(TEXT("Position.x = RFX2 * Z * (X - FX2);"));
		Out.Log(TEXT("Position.y = RFY2 * Z * (Y + YL - FY2);"));
		Out.Log(TEXT("Position.z = Z;"));
		Out.Log(TEXT("gTexCoords.x = (U)*UMult;"));
		Out.Log(TEXT("gTexCoords.y = (V + VL) * VMult;"));
		Out.Log(TEXT("gDrawColor = vDrawColor[0];"));

		Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Position, 1.0);"));
		Out.Log(TEXT("gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane, vCoords[2]);"));
		Out.Log(TEXT("EmitVertex();"));

		Out.Log(TEXT("EndPrimitive();"));
		Out.Log(TEXT("}"));
	}
}

// Vertexshader for DrawTile.
static void Create_DrawTile_Vert(UXOpenGLRenderDevice* GL, FOutputDevice& Out)
{
	Out.Log(TEXT("layout(location = 0) in vec3 Coords;")); // == gl_Vertex
	Out.Log(TEXT("layout(location = 7) in vec4 DrawColor;"));
	Out.Log(TEXT("layout(location = 8) in float TexNum;"));

	if (GL->OpenGLVersion == GL_ES)
	{
		//No geometry shader in GL_ES.
		Out.Log(TEXT("layout(location = 2) in vec2 TexCoords;"));

		Out.Log(TEXT("out vec2 gTexCoords;"));
		Out.Log(TEXT("flat out vec4 gDrawColor;"));
		Out.Log(TEXT("flat out uint gTexNum;"));
	}
	else
	{
		Out.Log(TEXT("layout(location = 2) in vec4 TexCoords0;"));
		Out.Log(TEXT("layout(location = 9) in vec4 TexCoords1;"));
		Out.Log(TEXT("layout(location = 10) in vec4 TexCoords2;"));

		Out.Log(TEXT("out vec3 vCoords;"));
		Out.Log(TEXT("out vec4 vTexCoords0;"));
		Out.Log(TEXT("out vec4 vTexCoords1;"));
		Out.Log(TEXT("out vec4 vTexCoords2;"));

		Out.Log(TEXT("flat out vec4 vDrawColor;"));
		Out.Log(TEXT("flat out uint vTexNum;"));
		Out.Log(TEXT("out vec4 vEyeSpacePos;"));
	}

	/*
	#if SHADERDRAWPARAMETERS
	struct DrawTileShaderDrawParams
	{
		vec4 FrameCoords;  // (RFX2, RFY2, FX2, FY2)
		vec4 TextureInfo;  // (UMult, VMult, TexNum, Gamma)
		vec4 DrawColor;    // Color for the tile
		vec4 HitDrawColor; // Selection color for UEd
		uvec4 DrawParams;  // (PolyFlags, bHitTesting, unused, unused)
	};

	layout(std430, binding = 6) buffer AllDrawTileShaderDrawParams
	{
		DrawTileShaderDrawParams DrawTileParams[];
	};

	flat out uint vTexNum;
	flat out uint vPolyFlags;
	flat out float vGamma;
	# if EDITOR
	flat out bool vHitTesting;
	flat out vec4 vHitDrawColor;
	# endif
	#endif
	*/

	{
		Out.Log(TEXT("void main(void)"));
		Out.Log(TEXT("{"));
		if (GL->OpenGLVersion == GL_ES)
		{
			Out.Log(TEXT("vec4 gEyeSpacePos = modelviewMat * vec4(Coords, 1.0);"));

			Out.Log(TEXT("gTexNum = uint(TexNum);"));
			Out.Log(TEXT("gTexCoords = TexCoords;"));
			Out.Log(TEXT("gDrawColor = DrawColor;"));

			Out.Log(TEXT("gl_Position = modelviewprojMat * vec4(Coords, 1.0);"));
		}
		else
		{
			Out.Log(TEXT("vEyeSpacePos = modelviewMat * vec4(Coords, 1.0);"));
			Out.Log(TEXT("vCoords = Coords;"));

			Out.Log(TEXT("vTexNum = uint(TexNum);"));
			Out.Log(TEXT("vTexCoords0 = TexCoords0;"));
			Out.Log(TEXT("vTexCoords1 = TexCoords1;"));
			Out.Log(TEXT("vTexCoords2 = TexCoords2;"));
			Out.Log(TEXT("vDrawColor = DrawColor;"));

			Out.Log(TEXT("gl_Position = vec4(Coords, 1.0);"));
		}
		Out.Log(TEXT("}"));
	}
}

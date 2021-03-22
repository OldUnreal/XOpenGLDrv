/*=============================================================================
	Vertexshader for DrawComplexSurface, single pass.

	Revision history:
		* Created by Smirftsch
=============================================================================*/

layout (location = 0) in vec3 Coords;		// == gl_Vertex
layout (location = 1) in vec4 Normals;		// Surface Normals

uniform vec4 TexCoords[16];
uniform uint DrawParams[3];

out vec3 vCoords;
out vec4 vEyeSpacePos;
out vec2 vTexCoords;
out vec2 vLightMapCoords;
out vec2 vFogMapCoords;
#if DETAILTEXTURES
out vec2 vDetailTexCoords;
#endif
#if MACROTEXTURES
out vec2 vMacroTexCoords;
#endif
#if BUMPMAPS
out vec2 vBumpTexCoords;
#endif
#if ENGINE_VERSION==227
out vec2 vEnvironmentTexCoords;
#endif
out vec3 vNormals;
#if EDITOR
out vec3 Surface_Normal; // f.e. Render normal view.
#endif
#if ENGINE_VERSION==227 || BUMPMAPS
out mat3 TBNMat;
#endif

out float gl_ClipDistance[MAX_CLIPPINGPLANES];

#if 1
void main(void)
{
	uint DrawFlags = DrawParams[0];

	// Point Coords
	vCoords = Coords.xyz;

	// UDot/VDot calculation.
	vec3 MapCoordsXAxis = TexCoords[IDX_X_AXIS].xyz;
	vec3 MapCoordsYAxis = TexCoords[IDX_Y_AXIS].xyz;
#if EDITOR || ENGINE_VERSION==227 || BUMPMAPS
	vec3 MapCoordsZAxis = TexCoords[IDX_Z_AXIS].xyz;
#endif

	float UDot = TexCoords[IDX_X_AXIS].w;
	float VDot = TexCoords[IDX_Y_AXIS].w;

	float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
	float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
	vec2  MapDot  = vec2(MapDotU, MapDotV);

	//Texture UV to fragment
	vec2 TexMapMult = TexCoords[IDX_DIFFUSE_COORDS].xy;
	vec2 TexMapPan  = TexCoords[IDX_DIFFUSE_COORDS].zw;
	vTexCoords      = (MapDot - TexMapPan) * TexMapMult;

	//Texture UV Lightmap to fragment
	if ((DrawFlags & DF_LightMap) == DF_LightMap)
	{
		vec2 LightMapMult = TexCoords[IDX_LIGHTMAP_COORDS].xy;
		vec2 LightMapPan  = TexCoords[IDX_LIGHTMAP_COORDS].zw;
		vLightMapCoords	  = (MapDot - LightMapPan) * LightMapMult;
	}

	// Texture UV FogMap
	if ((DrawFlags & DF_FogMap) == DF_FogMap)
	{
		vec2 FogMapMult = TexCoords[IDX_FOGMAP_COORDS].xy;
		vec2 FogMapPan  = TexCoords[IDX_FOGMAP_COORDS].zw;
		vFogMapCoords   = (MapDot - FogMapPan) * FogMapMult;
	}

	// Texture UV DetailTexture
#if DETAILTEXTURES
	if ((DrawFlags & DF_DetailTexture) == DF_DetailTexture)
	{
		vec2 DetailMult  = TexCoords[IDX_DETAIL_COORDS].xy;
		vec2 DetailPan   = TexCoords[IDX_DETAIL_COORDS].zw;
		vDetailTexCoords = (MapDot - DetailPan) * DetailMult;
	}
#endif

	// Texture UV Macrotexture
#if MACROTEXTURES
	if ((DrawFlags & DF_MacroTexture) == DF_MacroTexture)
	{
		vec2 MacroMult  = TexCoords[IDX_MACRO_COORDS].xy;
		vec2 MacroPan   = TexCoords[IDX_MACRO_COORDS].zw;
		vMacroTexCoords = (MapDot - MacroPan) * MacroMult;
	}
#endif

	// Texture UV BumpMap
#if BUMPMAPS
	if ((DrawFlags & DF_BumpMap) == DF_BumpMap)
	{
		vec2 BumpMapMult = TexCoords[IDX_BUMPMAP_COORDS].xy;
		vec2 BumpMapPan  = TexCoords[IDX_BUMPMAP_COORDS].zw;
		vBumpTexCoords   = (MapDot - BumpMapPan) * BumpMapMult;
	}
#endif

	// Texture UV EnvironmentMap
#if ENGINE_VERSION==227
	if ((DrawFlags & DF_EnvironmentMap) == DF_EnvironmentMap)
	{
		vec2 EnvironmentMapMult = TexCoords[IDX_ENVIROMAP_COORDS].xy;
		vec2 EnvironmentMapPan  = TexCoords[IDX_ENVIROMAP_COORDS].zw;
		vEnvironmentTexCoords   = (MapDot - EnvironmentMapPan) * EnvironmentMapMult;
	}
#endif

#if ENGINE_VERSION==227 || BUMPMAPS
	vEyeSpacePos = modelviewMat*vec4(Coords, 1.0);
	if ((DrawFlags & (DF_MacroTexture|DF_BumpMap)) != 0)
	{
		vec3 T = normalize(vec3(-MapCoordsXAxis.x,MapCoordsXAxis.y,MapCoordsXAxis.z));
		vec3 B = normalize(vec3(MapCoordsYAxis.x,-MapCoordsYAxis.y,-MapCoordsYAxis.z));
		vec3 N = normalize(vec3(-MapCoordsZAxis.x,MapCoordsZAxis.y,MapCoordsZAxis.z)); //SurfNormals.

		// TBN must have right handed coord system.
		if (dot(cross(N, T), B) < 0.0)
		   T = T * -1.0;

		TBNMat = transpose(mat3(T, B, N));
	}
#endif

#if EDITOR
	Surface_Normal = MapCoordsZAxis;
#endif

	// Normals
	vNormals = Normals.xyz;

	uint ClipIndex = uint(ClipParams.x);
	
	gl_Position = modelviewprojMat * vec4(Coords, 1.0);
    gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane,Coords);
}
#else
void main (void)
{
	vEyeSpacePos = modelviewMat*vec4(Coords, 1.0);

	// Point Coords
	vCoords = Coords.xyz;

//	vTexCoords = vec2(0.0, 0.0);

	vec3 MapCoordsXAxis = TexCoords[IDX_X_AXIS].xyz;
	vec3 MapCoordsYAxis = TexCoords[IDX_Y_AXIS].xyz;
#if EDITOR || ENGINE_VERSION==227 || BUMPMAPS
	vec3 MapCoordsZAxis = TexCoords[IDX_Z_AXIS].xyz;
#endif

	float UDot = TexCoords[IDX_X_AXIS].w;
	float VDot = TexCoords[IDX_Y_AXIS].w;

	float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
	float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
	vec2  MapDot  = vec2(MapDotU, MapDotV);

	//Texture UV to fragment
	vec2 TexMapMult = TexCoords[IDX_DIFFUSE_COORDS].xy;
	vec2 TexMapPan  = TexCoords[IDX_DIFFUSE_COORDS].zw;
	vTexCoords      = (MapDot - TexMapPan) * TexMapMult;


//	uint ClipIndex = uint(ClipParams.x);
	gl_Position = modelviewprojMat * vec4(Coords, 1.0);
  //  gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane,Coords);
}
#endif
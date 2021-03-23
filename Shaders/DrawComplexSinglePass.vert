/*=============================================================================
	Vertexshader for DrawComplexSurface, single pass.

	Revision history:
		* Created by Smirftsch
=============================================================================*/

layout (location = 0) in vec3 Coords;		// == gl_Vertex

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
#if EDITOR
out vec3 Surface_Normal; // f.e. Render normal view.
#endif
#if ENGINE_VERSION==227 || BUMPMAPS
out mat3 TBNMat;
#endif

#if BINDLESSTEXTURES
flat out uint BaseTexNum;
flat out uint LightMapTexNum;
flat out uint FogMapTexNum;
flat out uint DetailTexNum;
flat out uint MacroTexNum;
flat out uint BumpMapTexNum;
flat out uint EnviroMapTexNum;
flat out uint DrawFlags;
flat out uint TextureFormat;
flat out uint PolyFlags;
flat out float BaseDiffuse;
flat out float BaseAlpha;
flat out float parallaxScale;
flat out float Gamma;
flat out float BumpMapSpecular;
#else
uniform vec4 TexCoords[16];
uniform uint TexNum[8];
uniform uint DrawParams[3];
#endif

#ifndef GL_ES
out float gl_ClipDistance[MAX_CLIPPINGPLANES];
#endif

#if 1
void main(void)
{
#if BINDLESSTEXTURES
	vec4 XAxis       = DrawComplexParams[gl_DrawID].XAxis;
	vec4 YAxis       = DrawComplexParams[gl_DrawID].YAxis;
	vec4 ZAxis       = DrawComplexParams[gl_DrawID].ZAxis;
	vec4 DiffuseUV   = DrawComplexParams[gl_DrawID].DiffuseUV;
	vec4 LightMapUV  = DrawComplexParams[gl_DrawID].LightMapUV;
	vec4 FogMapUV    = DrawComplexParams[gl_DrawID].FogMapUV;
	vec4 DetailUV    = DrawComplexParams[gl_DrawID].DetailUV;
	vec4 MacroUV     = DrawComplexParams[gl_DrawID].MacroUV;
	vec4 BumpMapUV   = DrawComplexParams[gl_DrawID].BumpMapUV;
	vec4 EnviroMapUV = DrawComplexParams[gl_DrawID].EnviroMapUV;

	DrawFlags        = DrawComplexParams[gl_DrawID].DrawFlags[0];
	TextureFormat    = DrawComplexParams[gl_DrawID].DrawFlags[1];
	PolyFlags        = DrawComplexParams[gl_DrawID].DrawFlags[2];
	BaseTexNum       = uint(DrawComplexParams[gl_DrawID].TexNum[0].x);
	LightMapTexNum   = uint(DrawComplexParams[gl_DrawID].TexNum[0].y);
	FogMapTexNum     = uint(DrawComplexParams[gl_DrawID].TexNum[0].z);
	DetailTexNum     = uint(DrawComplexParams[gl_DrawID].TexNum[0].w);
	MacroTexNum      = uint(DrawComplexParams[gl_DrawID].TexNum[1].x);
	BumpMapTexNum    = uint(DrawComplexParams[gl_DrawID].TexNum[1].y);
	EnviroMapTexNum  = uint(DrawComplexParams[gl_DrawID].TexNum[1].z);
	BaseDiffuse      = DrawComplexParams[gl_DrawID].DiffuseInfo.x;
	BaseAlpha        = DrawComplexParams[gl_DrawID].DiffuseInfo.z;
	BumpMapSpecular  = DrawComplexParams[gl_DrawID].BumpMapInfo.y;
	parallaxScale    = DrawComplexParams[gl_DrawID].MacroInfo.w;
	Gamma            = ZAxis.w;
#else
	vec4 XAxis       = TexCoords[IDX_X_AXIS];
	vec4 YAxis       = TexCoords[IDX_Y_AXIS];
	vec4 ZAxis       = TexCoords[IDX_Z_AXIS];
	vec4 DiffuseUV   = TexCoords[IDX_DIFFUSE_COORDS];
	vec4 LightMapUV  = TexCoords[IDX_LIGHTMAP_COORDS];
	vec4 FogMapUV    = TexCoords[IDX_FOGMAP_COORDS];
	vec4 DetailUV    = TexCoords[IDX_DETAIL_COORDS];
	vec4 MacroUV     = TexCoords[IDX_MACRO_COORDS];
	vec4 BumpMapUV   = TexCoords[IDX_BUMPMAP_COORDS];
	vec4 EnviroMapUV = TexCoords[IDX_ENVIROMAP_COORDS];

	uint DrawFlags   = DrawParams[0];
#endif

	// Point Coords
	vCoords = Coords.xyz;

	// UDot/VDot calculation.
	vec3 MapCoordsXAxis = XAxis.xyz;
	vec3 MapCoordsYAxis = YAxis.xyz;
#if EDITOR || ENGINE_VERSION==227 || BUMPMAPS
	vec3 MapCoordsZAxis = ZAxis.xyz;
#endif

	float UDot = XAxis.w;
	float VDot = YAxis.w;

	float MapDotU = dot(MapCoordsXAxis, Coords.xyz) - UDot;
	float MapDotV = dot(MapCoordsYAxis, Coords.xyz) - VDot;
	vec2  MapDot  = vec2(MapDotU, MapDotV);

	//Texture UV to fragment
	vec2 TexMapMult = DiffuseUV.xy;
	vec2 TexMapPan  = DiffuseUV.zw;
	vTexCoords      = (MapDot - TexMapPan) * TexMapMult;

	//Texture UV Lightmap to fragment
	if ((DrawFlags & DF_LightMap) == DF_LightMap)
	{
		vec2 LightMapMult = LightMapUV.xy;
		vec2 LightMapPan  = LightMapUV.zw;
		vLightMapCoords	  = (MapDot - LightMapPan) * LightMapMult;
	}

	// Texture UV FogMap
	if ((DrawFlags & DF_FogMap) == DF_FogMap)
	{
		vec2 FogMapMult = FogMapUV.xy;
		vec2 FogMapPan  = FogMapUV.zw;
		vFogMapCoords   = (MapDot - FogMapPan) * FogMapMult;
	}

	// Texture UV DetailTexture
#if DETAILTEXTURES
	if ((DrawFlags & DF_DetailTexture) == DF_DetailTexture)
	{
		vec2 DetailMult  = DetailUV.xy;
		vec2 DetailPan   = DetailUV.zw;
		vDetailTexCoords = (MapDot - DetailPan) * DetailMult;
	}
#endif

	// Texture UV Macrotexture
#if MACROTEXTURES
	if ((DrawFlags & DF_MacroTexture) == DF_MacroTexture)
	{
		vec2 MacroMult  = MacroUV.xy;
		vec2 MacroPan   = MacroUV.zw;
		vMacroTexCoords = (MapDot - MacroPan) * MacroMult;
	}
#endif

	// Texture UV BumpMap
#if BUMPMAPS
	if ((DrawFlags & DF_BumpMap) == DF_BumpMap)
	{
		vec2 BumpMapMult = BumpMapUV.xy;
		vec2 BumpMapPan  = BumpMapUV.zw;
		vBumpTexCoords   = (MapDot - BumpMapPan) * BumpMapMult;
	}
#endif

	// Texture UV EnvironmentMap
#if ENGINE_VERSION==227
	if ((DrawFlags & DF_EnvironmentMap) == DF_EnvironmentMap)
	{
		vec2 EnvironmentMapMult = EnviroMapUV.xy;
		vec2 EnvironmentMapPan  = EnviroMapUV.zw;
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

	gl_Position = modelviewprojMat * vec4(Coords, 1.0);

#ifndef GL_ES
	uint ClipIndex = uint(ClipParams.x);
    gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane,Coords);
#endif
}
#else
void main (void)
{
/*
	vec4 XAxis       = DrawComplexParams[gl_DrawID].XAxis;
	vec4 YAxis       = DrawComplexParams[gl_DrawID].YAxis;
	vec4 ZAxis       = DrawComplexParams[gl_DrawID].ZAxis;
	vec4 DiffuseUV   = DrawComplexParams[gl_DrawID].DiffuseUV;
	vec4 LightMapUV  = DrawComplexParams[gl_DrawID].LightMapUV;
	vec4 FogMapUV    = DrawComplexParams[gl_DrawID].FogMapUV;
	vec4 DetailUV    = DrawComplexParams[gl_DrawID].DetailUV;
	vec4 MacroUV     = DrawComplexParams[gl_DrawID].MacroUV;
	vec4 BumpMapUV   = DrawComplexParams[gl_DrawID].BumpMapUV;
	vec4 EnviroMapUV = DrawComplexParams[gl_DrawID].EnviroMapUV;
*/
/*	DrawFlags        = DrawComplexParams[gl_DrawID].DrawFlags[0];
	TextureFormat    = DrawComplexParams[gl_DrawID].DrawFlags[1];
	PolyFlags        = DrawComplexParams[gl_DrawID].DrawFlags[2];
	*/
//	BaseTexNum       = uint(DrawComplexParams[gl_DrawID].TexNum[0].x);
/*	LightMapTexNum   = uint(DrawComplexParams[gl_DrawID].TexNum[0].y);
	FogMapTexNum     = uint(DrawComplexParams[gl_DrawID].TexNum[0].z);
	DetailTexNum     = uint(DrawComplexParams[gl_DrawID].TexNum[0].w);
	MacroTexNum      = uint(DrawComplexParams[gl_DrawID].TexNum[1].x);
	BumpMapTexNum    = uint(DrawComplexParams[gl_DrawID].TexNum[1].y);
	EnviroMapTexNum  = uint(DrawComplexParams[gl_DrawID].TexNum[1].z);
	BaseDiffuse      = DrawComplexParams[gl_DrawID].DiffuseInfo.x;
	BaseAlpha        = DrawComplexParams[gl_DrawID].DiffuseInfo.z;
	BumpMapSpecular  = DrawComplexParams[gl_DrawID].BumpMapInfo.y;
	parallaxScale    = DrawComplexParams[gl_DrawID].MacroInfo.w;
	Gamma            = ZAxis.w;
*/
	vEyeSpacePos = modelviewMat*vec4(Coords, 1.0);

	// Point Coords
	vCoords = Coords.xyz;

	vTexCoords = vec2(0.0, 0.0);
#if BINDLESSTEXTURES
//	BaseTexNum = uint(TexNums0.x);
#endif

	uint ClipIndex = uint(ClipParams.x);
	gl_Position = modelviewprojMat * vec4(Coords, 1.0);
    gl_ClipDistance[ClipIndex] = PlaneDot(ClipPlane,Coords);
}
#endif
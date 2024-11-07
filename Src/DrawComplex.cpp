/*=============================================================================
	DrawComplex.cpp: Unreal XOpenGL DrawComplexSurface routines.
	Used for BSP drawing.

	Copyright 2014-2021 Oldunreal

	Revision history:
		* Created by Smirftsch
=============================================================================*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	Helpers
-----------------------------------------------------------------------------*/

static void SetTextureHelper
(
	UXOpenGLRenderDevice* RenDev,
	INT Multi,
	FTextureInfo& Info,
	DWORD PolyFlags,
	FLOAT PanBias,
	glm::vec4* TextureCoords,
	glm::vec4* TextureInfo,
	glm::uvec4* TexHandles
)
{
	RenDev->SetTexture(Multi, Info, PolyFlags, PanBias);
	if (TextureCoords)
		*TextureCoords = glm::vec4(RenDev->TexInfo[Multi].UMult, RenDev->TexInfo[Multi].VMult, RenDev->TexInfo[Multi].UPan, RenDev->TexInfo[Multi].VPan);
	if (TextureInfo)
		*TextureInfo = glm::vec4(Info.Texture->Diffuse > 0.f ? Info.Texture->Diffuse : 1.f, Info.Texture->Specular, Info.Texture->Alpha > 0.f ? Info.Texture->Alpha : 1.f, Info.Texture->TEXTURE_SCALE_NAME);
	StoreTexHandle(Multi, TexHandles, RenDev->TexInfo[Multi].BindlessTexHandle);
}

/*-----------------------------------------------------------------------------
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);

	if (NoDrawComplexSurface)
		return;

	auto Shader = dynamic_cast<DrawComplexProgram*>(Shaders[Complex_Prog]);

    STAT(clockFast(Stats.ComplexCycles));
	SetProgram(Complex_Prog);

	check(Surface.Texture);

	// Gather options
	DWORD Options = ShaderOptions::OPT_None;
	const DWORD NextPolyFlags = GetPolyFlagsAndShaderOptions(Surface.PolyFlags, Options, FALSE);
	Options |= ShaderOptions::OPT_DiffuseTexture;

#if ENGINE_VERSION==227
	Options |= ShaderOptions::OPT_DistanceFog;
	
	if (Surface.EnvironmentMap && EnvironmentMaps)
		Options |= ShaderOptions::OPT_EnvironmentMap;

	if (Surface.HeightMap && ParallaxVersion != Parallax_Disabled)
		Options |= ShaderOptions::OPT_HeightMap;

	if (Surface.BumpMap && BumpMaps)
		Options |= ShaderOptions::OPT_BumpMap;
#else
	if (BumpMaps && Surface.Texture && Surface.Texture->Texture && Surface.Texture->Texture->BumpMap)
		Options |= ShaderOptions::OPT_BumpMap;
#endif

	if (Surface.LightMap)
		Options |= ShaderOptions::OPT_LightMap;

	if (Surface.FogMap && Surface.FogMap->Mips[0] && Surface.FogMap->Mips[0]->DataPtr)
		Options |= ShaderOptions::OPT_FogMap;

	if (Surface.DetailTexture && DetailTextures)
		Options |= ShaderOptions::OPT_DetailTexture;

	if (Surface.MacroTexture && MacroTextures)
		Options |= ShaderOptions::OPT_MacroTexture;

	if (GIsEditor && NextPolyFlags & PF_Selected)
		Options |= ShaderOptions::OPT_Selected;

	Shader->SelectShaderSpecialization(ShaderOptions(Options));

	const bool CanBuffer = !Shader->DrawBuffer.IsFull() && Shader->ParametersBuffer.CanBuffer(1);

	// Check if this draw call will change any global state. If so, we want to flush any pending draw calls before we make the changes
	if (WillBlendStateChange(CurrentBlendPolyFlags, NextPolyFlags) || // Check if the blending mode will change
		WillTextureStateChange(DiffuseTextureIndex, *Surface.Texture, NextPolyFlags) || // Check if the surface textures will change
		(Surface.LightMap && WillTextureStateChange(LightMapIndex, *Surface.LightMap, NextPolyFlags)) ||
		((Surface.FogMap && Surface.FogMap->Mips[0] && Surface.FogMap->Mips[0]->DataPtr) && WillTextureStateChange(FogMapIndex, *Surface.FogMap, NextPolyFlags)) ||
		(Surface.DetailTexture && DetailTextures && WillTextureStateChange(DetailTextureIndex, *Surface.DetailTexture, NextPolyFlags)) ||
		(Surface.MacroTexture && MacroTextures && WillTextureStateChange(MacroTextureIndex, *Surface.MacroTexture, NextPolyFlags)) ||
#if ENGINE_VERSION==227
		(Surface.BumpMap && BumpMaps && WillTextureStateChange(BumpMapIndex, *Surface.BumpMap, NextPolyFlags)) ||
		(Surface.EnvironmentMap && EnvironmentMaps && WillTextureStateChange(EnvironmentMapIndex, *Surface.EnvironmentMap, NextPolyFlags)) ||
		(Surface.HeightMap && WillTextureStateChange(HeightMapIndex, *Surface.HeightMap, NextPolyFlags)) ||
#endif
		!CanBuffer)
	{
		// Dispatch buffered data
		Shader->Flush(!CanBuffer);

		// Update global GL state
		SetBlend(NextPolyFlags);
	}

	DrawComplexParameters* DrawCallParams = Shader->ParametersBuffer.GetCurrentElementPtr();

	// Editor Support.
	if (GIsEditor)
		DrawCallParams->DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : FPlaneToVec4(Surface.FlatColor.Plane());

	// Set Textures
	SetTextureHelper(this, DiffuseTextureIndex, *Surface.Texture, NextPolyFlags, 0.0, &DrawCallParams->DiffuseUV, Surface.Texture->Texture ? &DrawCallParams->DiffuseInfo : nullptr, DrawCallParams->TexHandles);
	if (!Surface.Texture->Texture)
		DrawCallParams->DiffuseInfo = glm::vec4(1.f, 0.f, 0.f, 1.f);

	if (Surface.LightMap)
		SetTextureHelper(this, LightMapIndex, *Surface.LightMap, PF_None, -0.5, &DrawCallParams->LightMapUV, nullptr, DrawCallParams->TexHandles);

	if (Surface.FogMap && Surface.FogMap->Mips[0] && Surface.FogMap->Mips[0]->DataPtr)
		SetTextureHelper(this, FogMapIndex, *Surface.FogMap, PF_AlphaBlend, -0.5, &DrawCallParams->FogMapUV, nullptr, DrawCallParams->TexHandles);

	if (Surface.DetailTexture && DetailTextures)
		SetTextureHelper(this, DetailTextureIndex, *Surface.DetailTexture, PF_None, 0.0, &DrawCallParams->DetailUV, nullptr, DrawCallParams->TexHandles);

	if (Surface.MacroTexture && MacroTextures)
		SetTextureHelper(this, MacroTextureIndex, *Surface.MacroTexture, PF_None, 0.0, &DrawCallParams->MacroUV, &DrawCallParams->MacroInfo, DrawCallParams->TexHandles);

#if ENGINE_VERSION==227
	if (Surface.BumpMap && BumpMaps)
	{
		Shader->BumpMapInfo = Surface.BumpMap;
#else
	if (BumpMaps && Surface.Texture && Surface.Texture->Texture && Surface.Texture->Texture->BumpMap)
	{
# if ENGINE_VERSION==1100
		Surface.Texture->Texture->BumpMap->Lock(Shader->BumpMapInfo, Viewport->CurrentTime, 0, this);
# else
		Surface.Texture->Texture->BumpMap->Lock(Shader->BumpMapInfo, FTime(), 0, this);
# endif
#endif
		SetTextureHelper(this, BumpMapIndex, FTEXTURE_GET(Shader->BumpMapInfo), PF_None, 0.0, nullptr, &DrawCallParams->BumpMapInfo, DrawCallParams->TexHandles);
	}

#if ENGINE_VERSION==227
	if (Surface.EnvironmentMap && EnvironmentMaps)
		SetTextureHelper(this, EnvironmentMapIndex, *Surface.EnvironmentMap, PF_None, 0.0, &DrawCallParams->EnviroMapUV, nullptr, DrawCallParams->TexHandles);

	if (Surface.HeightMap && ParallaxVersion != Parallax_Disabled)
		SetTextureHelper(this, HeightMapIndex, *Surface.HeightMap, PF_None, 0.0, nullptr, &DrawCallParams->HeightMapInfo, DrawCallParams->TexHandles);
#endif

	// Other draw data
	DrawCallParams->XAxis = glm::vec4(Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, Facet.MapCoords.XAxis | Facet.MapCoords.Origin);
	DrawCallParams->YAxis = glm::vec4(Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, Facet.MapCoords.YAxis | Facet.MapCoords.Origin);
	DrawCallParams->ZAxis = glm::vec4(Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, 0.0);
	DrawCallParams->DistanceFogColor = DistanceFogColor;
	DrawCallParams->DistanceFogInfo = DistanceFogValues;
	DrawCallParams->DistanceFogMode = DistanceFogMode;	

	Shader->DrawBuffer.StartDrawCall();
	auto DrawID = Shader->DrawBuffer.GetDrawID();

	INT FacetVertexCount = 0;
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
	{
		const INT NumPts = Poly->NumPts;
		if (NumPts < 3) //Skip invalid polygons,if any?
			continue;

		if (!Shader->VertBuffer.CanBuffer((NumPts - 2) * 3))
		{
			Shader->DrawBuffer.EndDrawCall(FacetVertexCount);
			Shader->ParametersBuffer.Advance(1); // advance so Flush automatically restores the drawcall params of the _current_ drawcall
			Shader->Flush(true);
			Shader->DrawBuffer.StartDrawCall();
			DrawID = Shader->DrawBuffer.GetDrawID();

			// just in case...
			if ((NumPts - 2) * 3 >= Shader->VertexBufferSize)
			{
				debugf(TEXT("DrawComplexSurface facet too big (facet has %d vertices - need to buffer %d points - Vertex Buffer Size is %d)!"), NumPts, (NumPts-2) * 3, Shader->VertexBufferSize);
				continue;
			}

			FacetVertexCount = 0;
		}

		FTransform** In = &Poly->Pts[0];
		auto Out = Shader->VertBuffer.GetCurrentElementPtr();

		for (INT i = 0; i < NumPts - 2; i++)
		{
			// stijn: not using the normals currently, but we're keeping them in
			// because they make our vertex data aligned to a 32 byte boundary
			(Out  )->Coords = FPlaneToVec4(In[0    ]->Point);
			(Out++)->DrawID = DrawID;
			(Out  )->Coords = FPlaneToVec4(In[i + 1]->Point);
			(Out++)->DrawID = DrawID;
			(Out  )->Coords = FPlaneToVec4(In[i + 2]->Point);
			(Out++)->DrawID = DrawID;
		}

		FacetVertexCount += (NumPts - 2) * 3;
		Shader->VertBuffer.Advance((NumPts - 2) * 3);
	}

	Shader->DrawBuffer.EndDrawCall(FacetVertexCount);
	Shader->ParametersBuffer.Advance(1);

#if ENGINE_VERSION!=227
	if (Options & ShaderOptions::OPT_BumpMap)
		Surface.Texture->Texture->BumpMap->Unlock(Shader->BumpMapInfo);
#endif

    STAT(unclockFast(Stats.ComplexCycles));

	unguard;
}

#if ENGINE_VERSION==227
//Draw everything after one pass. This function is called after each internal rendering pass, everything has to be properly indexed before drawing. Used for DrawComplexSurface.
void UXOpenGLRenderDevice::DrawPass(FSceneNode* Frame, INT Pass) {}
#endif

/*-----------------------------------------------------------------------------
	Complex Surface Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawComplexProgram::DrawComplexProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWCOMPLEX_SIZE * 12;
	ParametersBufferSize			= DRAWCOMPLEX_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::ComplexParametersIndex;
	NumTextureSamplers				= 8;
	DrawMode						= GL_TRIANGLES;
	NumVertexAttributes				= 3;
	UseSSBOParametersBuffer			= RenDev->UsingShaderDrawParameters;
	ParametersInfo					= DrawComplexParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= nullptr;
	FragmentShaderFunc				= &BuildFragmentShader;
}

void UXOpenGLRenderDevice::DrawComplexProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DrawComplexVertex), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT,   sizeof(DrawComplexVertex), (GLvoid*)(offsetof(DrawComplexVertex, DrawID)));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(DrawComplexVertex), (GLvoid*)(offsetof(DrawComplexVertex, Normal)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawComplexProgram::BuildCommonSpecializations()
{
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_Masked));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_Translucent));
	SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_FogMap));
	if (RenDev->DetailTextures)
	{
		SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_DetailTexture));
		SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_DetailTexture));
		SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_DetailTexture | ShaderOptions::OPT_Translucent));
		SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_DetailTexture | ShaderOptions::OPT_FogMap));

		if (RenDev->MacroTextures)
		{
			SelectShaderSpecialization(ShaderOptions(ShaderOptions::OPT_DiffuseTexture | ShaderOptions::OPT_LightMap | ShaderOptions::OPT_DetailTexture | ShaderOptions::OPT_MacroTexture));
		}
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

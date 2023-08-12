/*=============================================================================
	DrawComplex.cpp: Unreal XOpenGL DrawComplexSurface routines.
	Used for BSP drawing.

	Copyright 2014-2021 Oldunreal

    Todo:
    * implement proper usage of persistent buffers.

	Revision history:
		* Created by Smirftsch
=============================================================================*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	if (NoDrawComplexSurface)
		return;

	clockFast(Stats.ComplexCycles);
	SetProgram(Complex_Prog);
	dynamic_cast<DrawComplexProgram*>(Shaders[Complex_Prog])->DrawComplexSurface(Frame, Surface, Facet);
	unclockFast(Stats.ComplexCycles);
}

#if ENGINE_VERSION==227
//Draw everything after one pass. This function is called after each internal rendering pass, everything has to be properly indexed before drawing. Used for DrawComplexSurface.
void UXOpenGLRenderDevice::DrawPass(FSceneNode* Frame, INT Pass)
{
	guard(UXOpenGLRenderDevice::DrawPass);
	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	ShaderProgram Implementation
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawComplexProgram::SetTexture
(
	INT Multi,
	FTextureInfo& Info,
	DWORD PolyFlags,
	FLOAT PanBias,
	DWORD DrawFlags,
	glm::vec4* TextureCoords, 
	glm::vec4* TextureInfo
)
{
	DrawCallParams.DrawFlags |= DrawFlags;
	RenDev->SetTexture(Multi, Info, PolyFlags, PanBias, DrawFlags);
	if (TextureCoords)
		*TextureCoords = glm::vec4(RenDev->TexInfo[Multi].UMult, RenDev->TexInfo[Multi].VMult, RenDev->TexInfo[Multi].UPan, RenDev->TexInfo[Multi].VPan);
	if (TextureInfo)
		*TextureInfo = glm::vec4(Info.Texture->Diffuse, Info.Texture->Specular, Info.Texture->Alpha, Info.Texture->TEXTURE_SCALE_NAME);
	StoreTexHandle(Multi, DrawCallParams.TexHandles, RenDev->TexInfo[Multi].BindlessTexHandle);
}

void UXOpenGLRenderDevice::DrawComplexProgram::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, const FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);
	check(Surface.Texture);

	//if(Frame->Recursion > MAX_FRAME_RECURSION)
	//return;		

	// stijn: this absolutely kills performance on mac. You're updating global state here for every gouraud mesh/complex surface!
#if ENGINE_VERSION==227 && !__APPLE__
	// Update FrameCoords
    if (RenDev->BumpMaps)
		RenDev->UpdateCoords(Frame);
#endif

	const DWORD NextPolyFlags = SetPolyFlags(Surface.PolyFlags);

	// Check if the uniforms will change
	if (!RenDev->UsingShaderDrawParameters ||
		DrawCallParams.HitTesting != RenDev->HitTesting() ||
		// Check if the blending mode will change
		WillBlendStateChange(DrawCallParams.PolyFlags, NextPolyFlags) ||
		// Check if the surface textures will change
		RenDev->WillTextureStateChange(0, *Surface.Texture, NextPolyFlags) ||
		(Surface.LightMap && RenDev->WillTextureStateChange(1, *Surface.LightMap, NextPolyFlags)) ||
		(Surface.FogMap && RenDev->WillTextureStateChange(2, *Surface.FogMap, NextPolyFlags)) ||
		(Surface.DetailTexture && RenDev->DetailTextures && RenDev->WillTextureStateChange(3, *Surface.DetailTexture, NextPolyFlags)) ||
		(Surface.MacroTexture && RenDev->MacroTextures && RenDev->WillTextureStateChange(4, *Surface.MacroTexture, NextPolyFlags)) ||
#if ENGINE_VERSION==227
		(Surface.BumpMap && RenDev->BumpMaps && RenDev->WillTextureStateChange(5, *Surface.BumpMap, NextPolyFlags)) ||
		(Surface.EnvironmentMap && RenDev->EnvironmentMaps && RenDev->WillTextureStateChange(6, *Surface.EnvironmentMap, NextPolyFlags)) ||
		(Surface.HeightMap && RenDev->WillTextureStateChange(7, *Surface.HeightMap, NextPolyFlags)) ||
#endif
		// Check if we have room left in the multi-draw array
		DrawBuffer.IsFull())
	{
		// Dispatch buffered data
		Flush(true);

		// Update global GL state
		RenDev->SetBlend(NextPolyFlags, false);
	}

	DrawCallParams.PolyFlags = NextPolyFlags;
	DrawCallParams.DrawFlags = 0;

	// Editor Support.
	DrawCallParams.HitTesting = (GIsEditor && RenDev->HitTesting()) ? 1 : 0;
	if (GIsEditor)
	{
		DrawCallParams.DrawColor = 
			RenDev->HitTesting() ? FPlaneToVec4(RenDev->HitColor) : FPlaneToVec4(Surface.FlatColor.Plane());
		
		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawCallParams.RendMap = Frame->Viewport->Actor->RendMap;
	}

	// Set Textures
	SetTexture(0, *Surface.Texture, DrawCallParams.PolyFlags, 0.0, DF_DiffuseTexture, &DrawCallParams.DiffuseUV, Surface.Texture->Texture ? &DrawCallParams.DiffuseInfo : nullptr);
	DrawCallParams.TextureFormat = Surface.Texture->Format;

	if (!Surface.Texture->Texture)
		DrawCallParams.DiffuseInfo = glm::vec4(1.f, 0.f, 0.f, 1.f);

	if (Surface.LightMap)
		SetTexture(1, *Surface.LightMap, DrawCallParams.PolyFlags, -0.5, DF_LightMap, &DrawCallParams.LightMapUV);

	if (Surface.FogMap)
		SetTexture(2, *Surface.FogMap, PF_AlphaBlend, -0.5, DF_FogMap, &DrawCallParams.FogMapUV);
	
	if (Surface.DetailTexture && RenDev->DetailTextures)
		SetTexture(3, *Surface.DetailTexture, DrawCallParams.PolyFlags, 0.0, DF_DetailTexture, &DrawCallParams.DetailUV);

	if (Surface.MacroTexture && RenDev->MacroTextures)
		SetTexture(4, *Surface.MacroTexture, DrawCallParams.PolyFlags, 0.0, DF_MacroTexture, &DrawCallParams.MacroUV, &DrawCallParams.MacroInfo);
	
#if ENGINE_VERSION==227
	if (Surface.BumpMap && RenDev->BumpMaps)
	{
		BumpMapInfo = Surface.BumpMap;
#else
	if (RenDev->BumpMaps && Surface.Texture && Surface.Texture->Texture && Surface.Texture->Texture->BumpMap)
	{
# if ENGINE_VERSION==1100
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, Viewport->CurrentTime, 0, RenDev);
# else
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, FTime(), 0, RenDev);
# endif
#endif
		SetTexture(5, FTEXTURE_GET(BumpMapInfo), DrawCallParams.PolyFlags, 0.0, DF_BumpMap, nullptr, &DrawCallParams.BumpMapInfo);
	}
		
#if ENGINE_VERSION==227
	if (Surface.EnvironmentMap && RenDev->EnvironmentMaps)
		SetTexture(6, *Surface.EnvironmentMap, DrawCallParams.PolyFlags, 0.0, DF_EnvironmentMap, &DrawCallParams.EnviroMapUV);

    if (Surface.HeightMap && RenDev->ParallaxVersion != Parallax_Disabled)
		SetTexture(7, *Surface.HeightMap, DrawCallParams.PolyFlags, 0.0, DF_HeightMap, nullptr, &DrawCallParams.HeightMapInfo);
#endif

	// Other draw data
	DrawCallParams.XAxis = glm::vec4(Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, Facet.MapCoords.XAxis | Facet.MapCoords.Origin);
	DrawCallParams.YAxis = glm::vec4(Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, Facet.MapCoords.YAxis | Facet.MapCoords.Origin);
	DrawCallParams.ZAxis = glm::vec4(Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, RenDev->GetViewportGamma(Frame->Viewport));
	DrawCallParams.DistanceFogColor = RenDev->DistanceFogColor;
	DrawCallParams.DistanceFogInfo = RenDev->DistanceFogValues;
	DrawCallParams.DistanceFogMode = RenDev->DistanceFogMode;

	if (RenDev->UsingShaderDrawParameters)
		memcpy(ParametersBuffer.GetCurrentElementPtr(), &DrawCallParams, sizeof(DrawCallParameters));

	DrawBuffer.StartDrawCall();

	INT FacetVertexCount = 0;
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
	{
		const INT NumPts = Poly->NumPts;
		if (NumPts < 3) //Skip invalid polygons,if any?
			continue;

		if (!VertBuffer.CanBuffer((NumPts - 2) * 3))
		{
			DrawBuffer.EndDrawCall(FacetVertexCount);
			if (RenDev->UsingShaderDrawParameters)
				ParametersBuffer.Advance(1);
			
			Flush(true);

			if (RenDev->UsingShaderDrawParameters)
				memcpy(ParametersBuffer.GetCurrentElementPtr(), &DrawCallParams, sizeof(DrawCallParameters));
			
			// just in case...
			if (sizeof(BufferedVert) * (NumPts - 2) * 3 >= DRAWCOMPLEX_SIZE)
			{
				debugf(NAME_DevGraphics, TEXT("DrawComplexSurface facet too big!"));
				continue;
			}

			FacetVertexCount = 0;
		}

		FTransform** In = &Poly->Pts[0];
		auto Out = VertBuffer.GetCurrentElementPtr();

		for (INT i = 0; i < NumPts-2; i++)
		{
			// stijn: not using the normals currently, but we're keeping them in
			// because they make our vertex data aligned to a 32 byte boundary
			(Out++)->Coords = FPlaneToVec4(In[0    ]->Point);
			(Out++)->Coords = FPlaneToVec4(In[i + 1]->Point);
			(Out++)->Coords = FPlaneToVec4(In[i + 2]->Point);
		}

		FacetVertexCount  += (NumPts - 2) * 3;
		VertBuffer.Advance((NumPts - 2) * 3);
	}

	DrawBuffer.EndDrawCall(FacetVertexCount);
	if (RenDev->UsingShaderDrawParameters)
		ParametersBuffer.Advance(1);

#if ENGINE_VERSION!=227
	if(DrawCallParams.DrawFlags & DF_BumpMap)
		Surface.Texture->Texture->BumpMap->Unlock(BumpMapInfo);
#endif

	unguard;
}

void UXOpenGLRenderDevice::DrawComplexProgram::Flush(bool Wait)
{
	if (VertBuffer.IsEmpty())
		return;

	VertBuffer.BufferData(RenDev->UseBufferInvalidation, true, GL_STREAM_DRAW);
	if (!RenDev->UsingShaderDrawParameters)
	{
		auto Out = ParametersBuffer.GetElementPtr(0);
		memcpy(Out, &DrawCallParams, sizeof(DrawCallParameters));
	}
	ParametersBuffer.Bind();
	ParametersBuffer.BufferData(RenDev->UseBufferInvalidation, false, DRAWCALL_BUFFER_USAGE_PATTERN);

	if (!VertBuffer.IsInputLayoutCreated())
		CreateInputLayout();

	DrawBuffer.Draw(GL_TRIANGLES, RenDev);

	if (RenDev->UsingShaderDrawParameters)
		ParametersBuffer.Rotate(false);

	VertBuffer.Lock();
	VertBuffer.Rotate(Wait);

	DrawBuffer.Reset(
		VertBuffer.BeginOffsetBytes() / sizeof(BufferedVert),
		ParametersBuffer.BeginOffsetBytes() / sizeof(DrawCallParameters));
}

void UXOpenGLRenderDevice::DrawComplexProgram::CreateInputLayout()
{
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BufferedVert), (GLvoid*)(0));
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BufferedVert), (GLvoid*)(offsetof(BufferedVert, Normal)));
	VertBuffer.SetInputLayoutCreated();
}

void UXOpenGLRenderDevice::DrawComplexProgram::ActivateShader()
{
	VertBuffer.Wait();

	glUseProgram(ShaderProgramObject);
	VertBuffer.Bind();
	ParametersBuffer.Bind();

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	DrawCallParams.PolyFlags = 0;// SetPolyFlags(CurrentAdditionalPolyFlags | CurrentPolyFlags);
}

void UXOpenGLRenderDevice::DrawComplexProgram::DeactivateShader()
{
	Flush(false);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}

void UXOpenGLRenderDevice::DrawComplexProgram::BindShaderState()
{
	ShaderProgram::BindShaderState();

	if (!RenDev->UsingShaderDrawParameters)
		BindUniform(ComplexParametersIndex, "DrawCallParameters");

	// Bind regular texture samplers to their respective TMUs
	for (INT i = 0; i < 8; i++)
	{
		GLint MultiTextureUniform;
		GetUniformLocation(MultiTextureUniform, appToAnsi(*FString::Printf(TEXT("Texture%i"), i)));
		if (MultiTextureUniform != -1)
			glUniform1i(MultiTextureUniform, i);
	}
}

void UXOpenGLRenderDevice::DrawComplexProgram::MapBuffers()
{
	VertBuffer.GenerateVertexBuffer(RenDev);
	VertBuffer.MapVertexBuffer(RenDev->UsingPersistentBuffersComplex, DRAWCOMPLEX_SIZE);

	if (RenDev->UsingShaderDrawParameters)
	{
		ParametersBuffer.GenerateSSBOBuffer(RenDev, ComplexParametersIndex);
		ParametersBuffer.MapSSBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, MAX_DRAWCOMPLEX_BATCH, DRAWCALL_BUFFER_USAGE_PATTERN);
	}
	else
	{
		ParametersBuffer.GenerateUBOBuffer(RenDev, ComplexParametersIndex);
		ParametersBuffer.MapUBOBuffer(RenDev->UsingPersistentBuffersDrawcallParams, 1, DRAWCALL_BUFFER_USAGE_PATTERN);
		ParametersBuffer.Advance(1);
	}
}

void UXOpenGLRenderDevice::DrawComplexProgram::UnmapBuffers()
{
	VertBuffer.DeleteBuffer();
	ParametersBuffer.DeleteBuffer();
}

bool UXOpenGLRenderDevice::DrawComplexProgram::BuildShaderProgram()
{
	return ShaderProgram::BuildShaderProgram(BuildVertexShader, nullptr, BuildFragmentShader, EmitHeader);
}

UXOpenGLRenderDevice::DrawComplexProgram::~DrawComplexProgram()
{
	DeleteShader();
	DrawComplexProgram::UnmapBuffers();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

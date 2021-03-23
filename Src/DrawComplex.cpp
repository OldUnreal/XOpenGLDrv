/*=============================================================================
	DrawComplex.cpp: Unreal XOpenGL DrawComplexSurface routines.
	Used for BSP drawing.

	Copyright 2014-2017 Oldunreal

    Todo:
    * implement proper usage of persistent buffers.

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

enum TexCoordsIndices
{
	DIFFUSE_COORDS,
	LIGHTMAP_COORDS,
	FOGMAP_COORDS,
	DETAIL_COORDS,
	MACRO_COORDS,	
	BUMPMAP_COORDS,
	ENVIROMAP_COORDS,
	DIFFUSE_INFO,
	MACRO_INFO,
	BUMPMAP_INFO,
	X_AXIS,
	Y_AXIS,
	Z_AXIS
};

struct BufferedPoint
{
	FPlane Point;
};

void UXOpenGLRenderDevice::BufferComplexShaderParams()
{
	auto& ShaderParams = reinterpret_cast<DrawComplexShaderParams*>(DrawComplexSSBORange.Buffer)[DrawComplexBufferData.Index * MAX_DRAWCOMPLEX_BATCH + DrawComplexMultiDrawCount];
	for (INT i = 0; i < Z_AXIS + 1; ++i)
		ShaderParams.TexCoords[i] = TexMaps.TexCoords[i];
	ShaderParams.TexNum[0] = glm::vec4(TexInfo[0].TexNum, TexInfo[1].TexNum, TexInfo[2].TexNum, TexInfo[3].TexNum);
	ShaderParams.TexNum[1] = glm::vec4(TexInfo[4].TexNum, TexInfo[5].TexNum, TexInfo[6].TexNum, TexInfo[7].TexNum);
	ShaderParams.DrawFlags = glm::uvec4(TexMaps.DrawFlags, TexMaps.TextureFormat, DrawComplexBufferData.PolyFlags, 0);
}


void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);
	check(Surface.Texture);

	if(Frame->Recursion > MAX_FRAME_RECURSION || NoDrawComplexSurface)
		return;

	clockFast(Stats.ComplexCycles);

	//Draw polygons
	SetProgram(ComplexSurfaceSinglePass_Prog);

	if (DrawComplexBufferData.PrevPolyFlags != Surface.PolyFlags &&
		// see if any of the changed polyflags can affect blending
		((DrawComplexBufferData.PrevPolyFlags^Surface.PolyFlags) & (PF_TwoSided | PF_RenderHint | PF_Translucent | PF_Modulated | PF_Invisible | PF_AlphaBlend | PF_Occlude | PF_Highlighted | PF_RenderFog)))
	{
		//debugf(TEXT("Polyswitch %08x => %08x"), DrawComplexBufferData.PrevPolyFlags, Surface.PolyFlags);
		if (DrawComplexBufferData.IndexOffset > 0)
		{
			DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);

			if (DrawComplexSinglePassRange.Sync[DrawComplexBufferData.Index])
				WaitBuffer(DrawComplexSinglePassRange, DrawComplexBufferData.Index);
		}
		
		DrawComplexBufferData.PrevPolyFlags = Surface.PolyFlags;
		DrawComplexBufferData.PolyFlags = SetFlags(Surface.PolyFlags);
		SetBlend(DrawComplexBufferData.PolyFlags, false);
	}

	TexMaps.DrawFlags = DF_DiffuseTexture;

#if ENGINE_VERSION==227
	TexMaps.SurfNormal = Surface.SurfNormal;
#endif

	// Editor Support.
	if (GIsEditor)
	{
		DrawComplexBufferData.DrawColor = Surface.FlatColor.Plane();
		if (HitTesting())
			DrawComplexBufferData.DrawColor = HitColor;
		else if (Surface.PolyFlags & PF_FlatShaded)
			DrawComplexBufferData.DrawColor = FOpenGLGammaDecompress_sRGB(DrawComplexBufferData.DrawColor);

		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawComplexBufferData.RendMap = Frame->Viewport->Actor->RendMap;
	}

	// UDot/VDot
	TexMaps.TexCoords[X_AXIS] = glm::vec4(Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, Facet.MapCoords.XAxis | Facet.MapCoords.Origin);
	TexMaps.TexCoords[Y_AXIS] = glm::vec4(Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, Facet.MapCoords.YAxis | Facet.MapCoords.Origin);
	TexMaps.TexCoords[Z_AXIS] = glm::vec4(Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, Gamma);

	SetTexture(0, *Surface.Texture, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, NORMALTEX);
	TexMaps.TexCoords[DIFFUSE_COORDS] = glm::vec4(TexInfo[0].UMult, TexInfo[0].VMult, TexInfo[0].UPan, TexInfo[0].VPan);
	TexMaps.TexCoords[DIFFUSE_INFO] = glm::vec4(Surface.Texture->Texture->Diffuse, Surface.Texture->Texture->Specular, Surface.Texture->Texture->Alpha, Surface.Texture->Texture->Scale);
	DrawComplexBufferData.TexNum[0] = TexInfo[0].TexNum;
	TexMaps.TextureFormat = Surface.Texture->Texture->Format;

	if (Surface.LightMap) //can not make use of bindless, to many single textures. Determined by Info->Texture.
	{
		TexMaps.DrawFlags |= DF_LightMap;
		SetTexture(1, *Surface.LightMap, DrawComplexBufferData.PolyFlags, -0.5, ComplexSurfaceSinglePass_Prog, LIGHTMAP); //First parameter has to fit the uniform in the fragment shader
		TexMaps.TexCoords[LIGHTMAP_COORDS] = glm::vec4(TexInfo[1].UMult, TexInfo[1].VMult, TexInfo[1].UPan, TexInfo[1].VPan);
		DrawComplexBufferData.TexNum[1] = TexInfo[1].TexNum;
	}
	if (Surface.FogMap)
	{
		TexMaps.DrawFlags |= DF_FogMap;
		SetTexture(2, *Surface.FogMap, PF_AlphaBlend, -0.5, ComplexSurfaceSinglePass_Prog, FOGMAP);
		TexMaps.TexCoords[FOGMAP_COORDS] = glm::vec4(TexInfo[2].UMult, TexInfo[2].VMult, TexInfo[2].UPan, TexInfo[2].VPan);
		DrawComplexBufferData.TexNum[2] = TexInfo[2].TexNum;
	}
	if (Surface.DetailTexture && DetailTextures)
	{
		TexMaps.DrawFlags |= DF_DetailTexture;
		SetTexture(3, *Surface.DetailTexture, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, DETAILTEX);
		TexMaps.TexCoords[DETAIL_COORDS] = glm::vec4(TexInfo[3].UMult, TexInfo[3].VMult, TexInfo[3].UPan, TexInfo[3].VPan);
		DrawComplexBufferData.TexNum[3] = TexInfo[3].TexNum;
	}
	if (Surface.MacroTexture && MacroTextures)
	{
		TexMaps.DrawFlags |= DF_MacroTexture;
		SetTexture(4, *Surface.MacroTexture, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, MACROTEX);
		TexMaps.TexCoords[MACRO_COORDS] = glm::vec4(TexInfo[4].UMult, TexInfo[4].VMult, TexInfo[4].UPan, TexInfo[4].VPan);
		TexMaps.TexCoords[MACRO_INFO] = glm::vec4(Surface.MacroTexture->Texture->Diffuse, Surface.MacroTexture->Texture->Specular, Surface.MacroTexture->Texture->Alpha, Surface.MacroTexture->Texture->Scale);
		DrawComplexBufferData.TexNum[4] = TexInfo[4].TexNum;
	}
#if ENGINE_VERSION==227
	if (Surface.BumpMap && BumpMaps)
	{
		TexMaps.DrawFlags |= DF_BumpMap;
		SetTexture(5, *Surface.BumpMap, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, BUMPMAP);
		TexMaps.TexCoords[BUMPMAP_COORDS] = glm::vec4(TexInfo[5].UMult, TexInfo[5].VMult, TexInfo[5].UPan, TexInfo[5].VPan);
		TexMaps.TexCoords[BUMPMAP_INFO] = glm::vec4(Surface.BumpMap->Texture->Diffuse, Surface.BumpMap->Texture->Specular, Surface.BumpMap->Texture->Alpha, Surface.BumpMap->Texture->Scale);
		DrawComplexBufferData.TexNum[5] = TexInfo[5].TexNum;
	}
#else
	FTextureInfo BumpMapInfo;
	if (BumpMaps && Surface.Texture && Surface.Texture->Texture && Surface.Texture->Texture->BumpMap)
	{
# if ENGINE_VERSION==1100
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, Viewport->CurrentTime, 0, this);
# else
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, FTime(), 0, this);
# endif
		TexMaps.DrawFlags |= DF_BumpMap;
		SetTexture(5, BumpMapInfo, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, BUMPMAP);
		TexMaps.TexCoords[BUMPMAP_COORDS] = glm::vec4(TexInfo[5].UMult, TexInfo[5].VMult, TexInfo[5].UPan, TexInfo[5].VPan);
		TexMaps.TexCoords[BUMPMAP_INFO] = glm::vec4(BumpMapInfo.Texture->Diffuse, BumpMapInfo.Texture->Specular, BumpMapInfo.Texture->Alpha, BumpMapInfo.Texture->Scale);
		DrawComplexBufferData.TexNum[5] = TexInfo[5].TexNum;
	}
#endif	
#if ENGINE_VERSION==227
	if (Surface.EnvironmentMap && EnvironmentMaps)
	{
		TexMaps.DrawFlags |= DF_EnvironmentMap;
		SetTexture(6, *Surface.EnvironmentMap, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, ENVIRONMENTMAP);
		TexMaps.TexCoords[ENVIROMAP_COORDS] = glm::vec4(TexInfo[6].UMult, TexInfo[6].VMult, TexInfo[6].UPan, TexInfo[6].VPan);
		DrawComplexBufferData.TexNum[6] = TexInfo[6].TexNum;
	}
#endif

	BufferComplexShaderParams();

	DrawComplexMultiDrawFacetArray[DrawComplexMultiDrawCount] = DrawComplexMultiDrawVertices;

	INT FacetVertexCount = 0;
    for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
    {
        INT NumPts = Poly->NumPts;
        if (NumPts < 3) //Skip invalid polygons,if any?
            continue;

		if (DrawComplexBufferData.IndexOffset >= DRAWCOMPLEX_SIZE - DrawComplexStrideSize * (NumPts - 2) * 3)
		{
			DrawComplexMultiDrawFacetArray[DrawComplexMultiDrawCount++] = FacetVertexCount;
			
			DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
			debugf(TEXT("DrawComplexSurface overflow!"));

			if (DrawComplexSinglePassRange.Sync[DrawComplexBufferData.Index])
				WaitBuffer(DrawComplexSinglePassRange, DrawComplexBufferData.Index);

			// just in case...
			if (DrawComplexStrideSize * (NumPts - 2) * 3 >= DRAWCOMPLEX_SIZE)
			{
				debugf(TEXT("DrawComplexSurface facet too big!"));
				continue;
			}

			BufferComplexShaderParams();
			FacetVertexCount = 0;
		}				
    			
		FTransform** In = &Poly->Pts[0];
		auto Out = reinterpret_cast<BufferedPoint*>(
			&DrawComplexSinglePassRange.Buffer[DrawComplexBufferData.IndexOffset]);
		
		for (INT i = 0; i < NumPts-2; i++)
		{
			(Out++)->Point = In[0  ]->Point;
			(Out++)->Point = In[i+1]->Point;
			(Out++)->Point = In[i+2]->Point;
		}

		FacetVertexCount += (NumPts - 2) * 3;
		DrawComplexMultiDrawVertices      += (NumPts-2) * 3;		
		DrawComplexBufferData.IndexOffset += (NumPts-2) * 3 * (sizeof(BufferedPoint) / sizeof(FLOAT));
    }
	
	DrawComplexMultiDrawVertexCountArray[DrawComplexMultiDrawCount] = FacetVertexCount;
	DrawComplexMultiDrawCount++;
    
	//if (!UseHWLighting)
	if ((!UsingBindlessTextures || BindlessFail) && DrawComplexBufferData.IndexOffset > 0)
		DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
	
	CHECK_GL_ERROR();
#if ENGINE_VERSION!=227
	if(TexMaps.DrawFlags & DF_BumpMap)
		Surface.Texture->Texture->BumpMap->Unlock(BumpMapInfo);
#endif
	unclockFast(Stats.ComplexCycles);

	unguard;
}

#if ENGINE_VERSION==227

//Draw everything after one pass. This function is called after each internal rendering pass, everything has to be properly indexed before drawing. Used for DrawComplexSurface.
void UXOpenGLRenderDevice::DrawPass(FSceneNode* Frame, INT Pass)
{
	guard(UXOpenGLRenderDevice::DrawPass);
	unguard;
}
#endif

void UXOpenGLRenderDevice::DrawComplexVertsSinglePass(DrawComplexBuffer &BufferData, DrawComplexTexMaps TexMaps)
{
	GLuint TotalSize = BufferData.IndexOffset;
    CHECK_GL_ERROR();

	checkSlow(ActiveProgram == ComplexSurfaceSinglePass_Prog);

	// Data
	if (!UsingPersistentBuffersComplex)
	{
		if (UseBufferInvalidation)
		{
			glInvalidateBufferData(DrawComplexVertBuffer);
			glInvalidateBufferData(DrawComplexSSBO);
		}
#if defined(__LINUX_ARM__)
		// stijn: we get a 10x perf increase on the pi if we just replace the entire buffer...
		glBufferData(GL_ARRAY_BUFFER, TotalSize * sizeof(FLOAT), DrawComplexSinglePassRange.Buffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_SHADER_STORAGE_BUFFER, DrawComplexMultiDrawCount * sizeof(DrawComplexShaderParams), DrawComplexSSBORange.Buffer, GL_DYNAMIC_DRAW);
#else		
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, DrawComplexMultiDrawCount * sizeof(DrawComplexShaderParams), DrawComplexSSBORange.Buffer);
		glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(FLOAT), DrawComplexSinglePassRange.Buffer);
#endif		
    }

	GLintptr BeginOffset = BufferData.BeginOffset * sizeof(float);
	if (BeginOffset != PrevDrawComplexBeginOffset)
	{
		glVertexAttribPointer  (VERTEX_COORD_ATTRIB, 4 , GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (GLvoid*)(BeginOffset                  ));		
		PrevDrawComplexBeginOffset = BeginOffset;
	}	

	//set DrawColor if any. UED only.
	if (GIsEditor)
	{
		if (HitTesting())
		{
			glUniform4f(DrawComplexSinglePassDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
			glUniform1i(DrawComplexSinglePassbHitTesting, true);
		}
		else
		{
			glUniform4f(DrawComplexSinglePassDrawColor, BufferData.DrawColor.X, BufferData.DrawColor.Y, BufferData.DrawColor.Z, BufferData.DrawColor.W);
			glUniform1i(DrawComplexSinglePassbHitTesting, false);
		}
		glUniform1ui(DrawComplexSinglePassRendMap, BufferData.RendMap);
		CHECK_GL_ERROR();
	}

	// Draw
	glMultiDrawArrays(GL_TRIANGLES, DrawComplexMultiDrawFacetArray, DrawComplexMultiDrawVertexCountArray, DrawComplexMultiDrawCount);
    CHECK_GL_ERROR();	

	// reset
	DrawComplexMultiDrawVertices = DrawComplexMultiDrawCount = 0;

	if (UsingPersistentBuffersComplex)
	{		
		LockBuffer(DrawComplexSinglePassRange, DrawComplexBufferData.Index);
		DrawComplexBufferData.Index = (DrawComplexBufferData.Index + 1) % NUMBUFFERS;
		CHECK_GL_ERROR();
	}

	DrawComplexBufferData.BeginOffset = DrawComplexBufferData.Index * DRAWCOMPLEX_SIZE;
	DrawComplexBufferData.IndexOffset = 0;
	for (INT i = 0; i < ARRAY_COUNT(DrawComplexBufferData.TexNum); i++)
		DrawComplexBufferData.TexNum[i] = 0;
	BindlessFail = false;

	CHECK_GL_ERROR();
}

//
// Program Switching
//
void UXOpenGLRenderDevice::DrawComplexEnd(INT NextProgram)
{
	if (DrawComplexBufferData.IndexOffset > 0)
		DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);

	CHECK_GL_ERROR();

	// Clean up
	glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
}

void UXOpenGLRenderDevice::DrawComplexStart()
{
	if (UsingPersistentBuffersComplex && DrawComplexSinglePassRange.Sync[DrawComplexBufferData.Index])
		WaitBuffer(DrawComplexSinglePassRange, DrawComplexBufferData.Index);
	
#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
	if (UseAA && PrevProgram != GouraudPolyVertList_Prog && PrevProgram != GouraudPolyVert_Prog)
		glEnable(GL_MULTISAMPLE);
#endif

	glUseProgram(DrawComplexProg);
	glBindVertexArray(DrawComplexVertsSinglePassVao);
	glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, DrawComplexSSBO);

	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	
	DrawComplexBufferData.PrevPolyFlags = CurrentAdditionalPolyFlags | CurrentPolyFlags;	
	PrevDrawComplexBeginOffset = -1;

	CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

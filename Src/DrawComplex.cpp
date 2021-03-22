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
	DIFFUSE_INFO,
	X_AXIS,
	Y_AXIS,
	Z_AXIS,
	LIGHTMAP_COORDS,
	FOGMAP_COORDS,
	DETAIL_COORDS,
	MACRO_COORDS,
	MACRO_INFO,
	BUMPMAP_COORDS,
	BUMPMAP_INFO,
	ENVIROMAP_COORDS
};

struct BufferedPoint
{
	FPlane Point, Normal;
};

void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);
	check(Surface.Texture);

	if(Frame->Recursion > MAX_FRAME_RECURSION || NoDrawComplexSurface)
		return;

	clockFast(Stats.ComplexCycles);

	//Draw polygons
	SetProgram(ComplexSurfaceSinglePass_Prog);

	if (DrawComplexBufferData.PrevPolyFlags != Surface.PolyFlags)
	{
		if (DrawComplexBufferData.IndexOffset > 0)
			DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
		
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
	// stijn: you get a decent perf boost by precalculating this on the CPU since the same UDot/VDot gets calculated for every pixel on the facet
	//TexMaps.TexCoords[10] = glm::vec4(Facet.MapCoords.Origin.X, Facet.MapCoords.Origin.Y, Facet.MapCoords.Origin.Z, 0.f);

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
		TexMaps.TexCoords[BUMPMAP_COORDS] = glm::vec4(BumpMapInfo.Texture->Diffuse, BumpMapInfo.Texture->Specular, BumpMapInfo.Texture->Alpha, BumpMapInfo.Texture->Scale);
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

    if (DrawComplexSinglePassRange.Sync[DrawComplexBufferData.Index])
		WaitBuffer(DrawComplexSinglePassRange, DrawComplexBufferData.Index);

	INT NumVertices = MultiDrawPolyCount = 0;
    for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
    {
        INT NumPts = Poly->NumPts;
        if (NumPts < 3) //Skip invalid polygons,if any?
            continue;

		MultiDrawPolyStartArray[MultiDrawPolyCount]  = NumVertices;
		MultiDrawPointCountArray[MultiDrawPolyCount] = NumPts;
		MultiDrawPolyCount++;
		NumVertices += NumPts;
    			
		FTransform** In = &Poly->Pts[0];
		auto Out = reinterpret_cast<BufferedPoint*>(
			&DrawComplexSinglePassRange.Buffer[DrawComplexBufferData.BeginOffset + DrawComplexBufferData.IndexOffset]);

    	for (INT i = 0; i < NumPts; i++)
		{
			Out->Point  = (*In++)->Point;
			Out->Normal = TexMaps.SurfNormal;
			Out++;
		}

		DrawComplexBufferData.IndexOffset += NumPts * (sizeof(BufferedPoint) / sizeof(FLOAT));
    	
        if ( DrawComplexBufferData.IndexOffset >= DRAWCOMPLEX_SIZE - DrawComplexStrideSize)
        {
            DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
            debugf(NAME_DevGraphics,TEXT("DrawComplexSurface overflow!"));

        	// reset
			MultiDrawPolyCount = NumVertices = 0;
        }
    }
    
	//if (!UseHWLighting)
	if (DrawComplexBufferData.IndexOffset > 0)
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
            glInvalidateBufferData(DrawComplexVertBuffer);
#if defined(__LINUX_ARM__) 
		// stijn: we get a 10x perf increase on the pi if we just replace the entire buffer...
		glBufferData(GL_ARRAY_BUFFER, TotalSize * sizeof(FLOAT), DrawComplexSinglePassRange.Buffer, GL_DYNAMIC_DRAW);
#else
		glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(FLOAT), DrawComplexSinglePassRange.Buffer);		
#endif
    }

	// Draw/Poly flags
	GLuint DrawFlags[] = { TexMaps.DrawFlags, TexMaps.TextureFormat, BufferData.PrevPolyFlags };
	glUniform1uiv(DrawComplexSinglePassDrawParams, 3, DrawFlags);
    
	// Tex UVs and more.
	glUniform4fv(DrawComplexSinglePassTexCoords, 16, (const GLfloat*)TexMaps.TexCoords);

	// TexNum and Textures to be applied.
	glUniform1uiv(DrawComplexSinglePassTexNum, 8, (const GLuint*)BufferData.TexNum);
	CHECK_GL_ERROR();

	GLintptr BeginOffset = BufferData.BeginOffset * sizeof(float);
	if (BeginOffset != PrevDrawComplexBeginOffset)
	{
		glVertexAttribPointer  (VERTEX_COORD_ATTRIB, 4 , GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (GLvoid*)(BeginOffset                 ));
		glVertexAttribPointer  (NORMALS_ATTRIB     , 4 , GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (GLvoid*)(BeginOffset + FloatSize4    ));
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
	glMultiDrawArrays(GL_TRIANGLE_FAN, MultiDrawPolyStartArray, MultiDrawPointCountArray, MultiDrawPolyCount);
    CHECK_GL_ERROR();

	if(UsingPersistentBuffersComplex)
	{
		LockBuffer(DrawComplexSinglePassRange, BufferData.Index);
		BufferData.Index = (BufferData.Index + 1) % NUMBUFFERS;
		CHECK_GL_ERROR();
	}

	BufferData.BeginOffset = BufferData.Index * DRAWCOMPLEX_SIZE;
	BufferData.IndexOffset = 0;
	for (INT i = 0; i < ARRAY_COUNT(BufferData.TexNum);i++)
		BufferData.TexNum[i] = 0;

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
#if XOPENGL_DRAWCOMPLEX_NORMALS
	glDisableVertexAttribArray(NORMALS_ATTRIB);
#endif
}

void UXOpenGLRenderDevice::DrawComplexStart()
{
#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
	if (UseAA && PrevProgram != GouraudPolyVertList_Prog && PrevProgram != GouraudPolyVert_Prog)
		glEnable(GL_MULTISAMPLE);
#endif

	glUseProgram(DrawComplexProg);
	glBindVertexArray(DrawComplexVertsSinglePassVao);
	glBindBuffer(GL_ARRAY_BUFFER, DrawComplexVertBuffer);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DRAWCOMPLEX_SIZE, 0, GL_STREAM_DRAW);

	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
#if XOPENGL_DRAWCOMPLEX_NORMALS
	glEnableVertexAttribArray(NORMALS_ATTRIB);
#endif
	PrevDrawComplexBeginOffset = -1;
	DrawComplexBufferData.PolyFlags = 0;

	CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

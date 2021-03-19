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

#if XOPENGL_DRAWCOMPLEX_NORMALS
struct BufferedPoint
{
	FPlane Point, Normal;
};
#else
struct BufferedPoint
{
	FPlane Point;
};
#endif

void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);
	check(Surface.Texture);

	if(Frame->Recursion > MAX_FRAME_RECURSION || NoDrawComplexSurface)
		return;

	clockFast(Stats.ComplexCycles);

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

	//Draw polygons
	SetProgram(ComplexSurfaceSinglePass_Prog);

	if (DrawComplexBufferData.IndexOffset > 0 && DrawComplexBufferData.PrevPolyFlags != Surface.PolyFlags)
		DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);

    DrawComplexBufferData.PrevPolyFlags = Surface.PolyFlags;

	DrawComplexBufferData.PolyFlags = SetFlags(Surface.PolyFlags);
	SetBlend(DrawComplexBufferData.PolyFlags , false);	

    SetTexture(0, *Surface.Texture, DrawComplexBufferData.PolyFlags , 0.0, ComplexSurfaceSinglePass_Prog,NORMALTEX);
    TexMaps.TexCoords[0] = glm::vec4(TexInfo[0].UMult, TexInfo[0].VMult, TexInfo[0].UPan, TexInfo[0].VPan);
	if (UsingBindlessTextures)
		DrawComplexBufferData.TexNum[0] = TexInfo[0].TexNum;

	if (Surface.LightMap) //can not make use of bindless, to many single textures. Determined by Info->Texture.
	{
		TexMaps.DrawFlags |= DF_LightMap;
		SetTexture(1, *Surface.LightMap, DrawComplexBufferData.PolyFlags, -0.5, ComplexSurfaceSinglePass_Prog, LIGHTMAP); //First parameter has to fit the uniform in the fragment shader
		TexMaps.TexCoords[1] = glm::vec4(TexInfo[1].UMult, TexInfo[1].VMult, TexInfo[1].UPan, TexInfo[1].VPan);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[1] = TexInfo[1].TexNum;
	}
	if (Surface.DetailTexture && DetailTextures)
	{
		TexMaps.DrawFlags |= DF_DetailTexture;
		SetTexture(2, *Surface.DetailTexture, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, DETAILTEX);
		TexMaps.TexCoords[2] = glm::vec4(TexInfo[2].UMult, TexInfo[2].VMult, TexInfo[2].UPan, TexInfo[2].VPan);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[2] = TexInfo[2].TexNum;
	}
	if (Surface.MacroTexture && MacroTextures)
	{
		TexMaps.DrawFlags |= DF_MacroTexture;
		SetTexture(3, *Surface.MacroTexture, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, MACROTEX);
		TexMaps.TexCoords[3] = glm::vec4(TexInfo[3].UMult, TexInfo[3].VMult, TexInfo[3].UPan, TexInfo[3].VPan);
		TexMaps.TexCoords[13] = glm::vec4(Surface.MacroTexture->Texture->Diffuse, Surface.MacroTexture->Texture->Specular, Surface.MacroTexture->Texture->Alpha, Surface.MacroTexture->Texture->Scale);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[3] = TexInfo[3].TexNum;
	}
#if ENGINE_VERSION==227
	if (Surface.BumpMap && BumpMaps)
	{
		TexMaps.DrawFlags |= DF_BumpMap;
		SetTexture(4, *Surface.BumpMap, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, BUMPMAP);
		TexMaps.TexCoords[4] = glm::vec4(TexInfo[4].UMult, TexInfo[4].VMult, TexInfo[4].UPan, TexInfo[4].VPan);
		TexMaps.TexCoords[12] = glm::vec4(Surface.BumpMap->Texture->Diffuse, Surface.BumpMap->Texture->Specular, Surface.BumpMap->Texture->Alpha, Surface.BumpMap->Texture->Scale);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[4] = TexInfo[4].TexNum;
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
		SetTexture(4, BumpMapInfo, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, BUMPMAP);
		TexMaps.TexCoords[12] = glm::vec4(BumpMapInfo.Texture->Diffuse, BumpMapInfo.Texture->Specular, BumpMapInfo.Texture->Alpha, BumpMapInfo.Texture->Scale);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[4] = TexInfo[4].TexNum;
	}	
#endif
	if (Surface.FogMap)
	{
		TexMaps.DrawFlags |= DF_FogMap;
		SetTexture(5, *Surface.FogMap, PF_AlphaBlend, -0.5, ComplexSurfaceSinglePass_Prog, FOGMAP);
		TexMaps.TexCoords[5] = glm::vec4(TexInfo[5].UMult, TexInfo[5].VMult, TexInfo[5].UPan, TexInfo[5].VPan);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[5] = TexInfo[5].TexNum;
	}
#if ENGINE_VERSION==227
	if (Surface.EnvironmentMap && EnvironmentMaps)
	{
		TexMaps.DrawFlags |= DF_EnvironmentMap;
		SetTexture(6, *Surface.EnvironmentMap, DrawComplexBufferData.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, ENVIRONMENTMAP);
		TexMaps.TexCoords[6] = glm::vec4(TexInfo[6].UMult, TexInfo[6].VMult, TexInfo[6].UPan, TexInfo[6].VPan);
		if (UsingBindlessTextures)
			DrawComplexBufferData.TexNum[6] = TexInfo[6].TexNum;
	}
#endif

	// UDot/VDot
	TexMaps.TexCoords[7] = glm::vec4(Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, 0.f);
	TexMaps.TexCoords[8] = glm::vec4(Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, 0.f);
	TexMaps.TexCoords[9] = glm::vec4(Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, 0.f);
	// stijn: you get a decent perf boost by precalculating this on the CPU since the same UDot/VDot gets calculated for every pixel on the facet
	TexMaps.TexCoords[10] = glm::vec4(Facet.MapCoords.XAxis | Facet.MapCoords.Origin, Facet.MapCoords.YAxis | Facet.MapCoords.Origin, 0.f, 0.f);
	//TexMaps.TexCoords[10] = glm::vec4(Facet.MapCoords.Origin.X, Facet.MapCoords.Origin.Y, Facet.MapCoords.Origin.Z, 0.f);
	TexMaps.TexCoords[11] = glm::vec4(Surface.Texture->Texture->Diffuse, Surface.Texture->Texture->Specular, Surface.Texture->Texture->Alpha, Surface.Texture->Texture->Scale);

	 //*(INT*)&TexMaps.TexCoords[11].w = (INT)Surface.Texture->Format;

    // Additional maps.
	//TexMaps.TexCoords[14] = glm::vec4(0.f, 0.f, 0.f, 0.f);
	TexMaps.TexCoords[15] = glm::vec4(0.f, 0.f, (GLfloat)Surface.Texture->Texture->Format, (GLfloat)TexMaps.DrawFlags);

	//debugf(TEXT("Facet.MapCoords.XAxis.X %f, Facet.MapCoords.XAxis.Y %f, Facet.MapCoords.XAxis.Z %f,Facet.MapCoords.YAxis.X %f , Facet.MapCoords.YAxis.Y %f, Facet.MapCoords.YAxis.Z %f,Facet.MapCoords.ZAxis.X %f, Facet.MapCoords.ZAxis.Y %f, Facet.MapCoords.ZAxis.Z %f Facet.MapCoords.Origin.X %f, Facet.MapCoords.Origin.Y %f, Facet.MapCoords.Origin.Z %f"), Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, Facet.MapCoords.Origin.X, Facet.MapCoords.Origin.Y, Facet.MapCoords.Origin.Z);

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
    	
		BufferedPoint* Out = reinterpret_cast<BufferedPoint*>(
			&DrawComplexSinglePassRange.Buffer[DrawComplexBufferData.BeginOffset + DrawComplexBufferData.IndexOffset]);
		FTransform** In = &Poly->Pts[0];
		
        for ( INT i=0; i<NumPts; i++ )
        {
			Out->Point = (*In++)->Point;
#if XOPENGL_DRAWCOMPLEX_NORMALS
			Out->Normal = TexMaps.SurfNormal;
#endif
			Out++;
        }
		DrawComplexBufferData.IndexOffset += (sizeof(BufferedPoint) / sizeof(FLOAT)) * NumPts;
        if ( DrawComplexBufferData.IndexOffset >= DRAWCOMPLEX_SIZE - DrawComplexStrideSize)
        {
            DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
            debugf(NAME_DevGraphics,TEXT("DrawComplexSurface overflow!"));

        	// reset
			MultiDrawPolyCount = NumVertices = 0;
        }
    }
    //debugf(TEXT("DrawComplexBufferData.IndexOffset %i DrawComplexBufferData.Index %i DrawComplexBufferData.BeginOffset %i"),DrawComplexBufferData.IndexOffset, DrawComplexBufferData.Index, DrawComplexBufferData.BeginOffset);

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

    // PolyFlags
	glUniform1ui(DrawComplexSinglePassPolyFlags, BufferData.PolyFlags);
	CHECK_GL_ERROR();

    // TexNum and Textures to be applied.
    glUniform1uiv(DrawComplexSinglePassTexNum, 8, (const GLuint*) BufferData.TexNum);
    CHECK_GL_ERROR();

	// Gamma
	glUniform1f(DrawComplexSinglePassGamma,Gamma);

	GLintptr BeginOffset = BufferData.BeginOffset * sizeof(float);
	if (BeginOffset != PrevDrawComplexBeginOffset)
	{
		glVertexAttribPointer(VERTEX_COORD_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (GLvoid*)BeginOffset);
#if XOPENGL_DRAWCOMPLEX_NORMALS
		glVertexAttribPointer(NORMALS_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (GLvoid*)(BeginOffset + FloatSize4));
#endif
		PrevDrawComplexBeginOffset = BeginOffset;
	}

	// Tex UVs and more.
	glUniform4fv(DrawComplexSinglePassTexCoords, 16, (const GLfloat*)TexMaps.TexCoords);

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

	CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

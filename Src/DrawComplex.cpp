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


inline void UXOpenGLRenderDevice::BufferComplexSurfacePoint( FLOAT* DrawComplexTemp, FTransform* P, DrawComplexTexMaps TexMaps, FSurfaceFacet& Facet, DrawComplexBuffer& Buffer, INT InterleaveCounter )
{
	guard(UXOpenGLRenderDevice::BufferGouraudPolygonPoint);

	// Points
	DrawComplexTemp[0] = P->Point.X;
	DrawComplexTemp[1] = P->Point.Y;
	DrawComplexTemp[2] = P->Point.Z;

	// Normals
	DrawComplexTemp[3] = TexMaps.SurfNormal.X;
	DrawComplexTemp[4] = TexMaps.SurfNormal.Y;
	DrawComplexTemp[5] = TexMaps.SurfNormal.Z;

	Buffer.VertSize += FloatsPerVertex;
	unguard;
}

void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);
	check(Surface.Texture);

	if(Frame->Recursion > MAX_FRAME_RECURSION)
		return;

	clock(Stats.ComplexCycles);

	DrawComplexTexMaps TexMaps;
	TexMaps.bLightMap = Surface.LightMap ? true : false;
	TexMaps.bFogMap = Surface.FogMap ? true : false;
	TexMaps.bDetailTex = (Surface.DetailTexture && DetailTextures) ? true : false;
	TexMaps.bMacroTex = (Surface.MacroTexture && MacroTextures) ? true : false;
#if ENGINE_VERSION==227
	TexMaps.SurfNormal = Surface.SurfNormal;
	TexMaps.bBumpMap = (Surface.BumpMap && BumpMaps) ? true : false;
	TexMaps.bEnvironmentMap = (Surface.EnvironmentMap && EnvironmentMaps) ? true : false; // not yet implemented.
	FLightInfo* LightStart;
	FLightInfo*	LightEnd;
	GLightManager->GetLightData(LightStart, LightEnd);

	INT j = 0;
	for (FLightInfo* Light = LightStart; Light<LightEnd; Light++)
	{
		if (!Light->Actor || !Light->Actor->bStatic)
			continue;
		TexMaps.LightPos[j] = glm::vec4(Light->Actor->Location.X, Light->Actor->Location.Y, Light->Actor->Location.Z,1.f);
		//debugf(TEXT("%i %f %f %f"), i, Light->Actor->Location.X, Light->Actor->Location.Y, Light->Actor->Location.Z);
		if (j == 7)
			break;
		j++;
	}
	LightNum = j;
	//debugf(TEXT("%f %f %f %f"), Surface.SurfNormal.W, Surface.SurfNormal.X, Surface.SurfNormal.Y, Surface.SurfNormal.Z);
#else
	bool bBumpMap = false;
	FTextureInfo BumpMapInfo;
	if(Surface.Texture && Surface.Texture->Texture && Surface.Texture->Texture->BumpMap)
	{
#if ENGINE_VERSION==1100
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, Viewport->CurrentTime, 0, this);
#else
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, FTime(), 0, this);
#endif
		bBumpMap=true;
	}
	TexMaps.LightPos[0] = glm::vec4(1.f, 1.f, 1.f, 1.f);
#endif

	// Editor Support.
	FPlane DrawColor = FPlane(0.f, 0.f, 0.f, 0.f);
	if (GIsEditor)
	{
	    DrawColor = Surface.FlatColor.Plane();
		if (HitTesting())
			DrawColor = HitColor;
		else if (Surface.PolyFlags & PF_FlatShaded)
			DrawColor = FOpenGLGammaDecompress_sRGB(DrawColor);

		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawComplexBufferData.RendMap = Frame->Viewport->Actor->RendMap;
	}

	//Draw polygons
	SetProgram(ComplexSurfaceSinglePass_Prog);
	DWORD PolyFlags=SetBlend(Surface.PolyFlags, ComplexSurfaceSinglePass_Prog);
	SetTexture(0, *Surface.Texture, Surface.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog);
	TexMaps.TexCoords[0] = glm::vec4(TexInfo[0].UMult, TexInfo[0].VMult, TexInfo[0].UPan, TexInfo[0].VPan);

	DrawComplexBufferData.PolyFlags = PolyFlags;

	INT InterleaveCounter = 6; // base count for Verts and SurfNormal.

	if (TexMaps.bLightMap)
	{
		SetTexture(1, *Surface.LightMap, PolyFlags, -0.5, ComplexSurfaceSinglePass_Prog); //First parameter has to fit the uniform in the fragment shader
		TexMaps.TexCoords[1] = glm::vec4(TexInfo[1].UMult, TexInfo[1].VMult, TexInfo[1].UPan, TexInfo[1].VPan);
	}
	if (TexMaps.bDetailTex)
	{
		SetTexture(2, *Surface.DetailTexture, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog);
		TexMaps.TexCoords[2] = glm::vec4(TexInfo[2].UMult, TexInfo[2].VMult, TexInfo[2].UPan, TexInfo[2].VPan);
	}
	if (TexMaps.bMacroTex)
	{
		SetTexture(3, *Surface.MacroTexture, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog);
		TexMaps.TexCoords[3] = glm::vec4(TexInfo[3].UMult, TexInfo[3].VMult, TexInfo[3].UPan, TexInfo[3].VPan);
	}
	if (TexMaps.bBumpMap)
	{
#if ENGINE_VERSION==227
		SetTexture(4, *Surface.BumpMap, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog);
		TexMaps.TexCoords[4] = glm::vec4(TexInfo[4].UMult, TexInfo[4].VMult, TexInfo[4].UPan, TexInfo[4].VPan);
#else
		SetTexture(4, BumpMapInfo, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog);
#endif
	}
	if (TexMaps.bFogMap)
	{
		SetTexture(5, *Surface.FogMap, PF_AlphaBlend, -0.5, ComplexSurfaceSinglePass_Prog);
		TexMaps.TexCoords[5] = glm::vec4(TexInfo[5].UMult, TexInfo[5].VMult, TexInfo[5].UPan, TexInfo[5].VPan);
	}
	if (TexMaps.bEnvironmentMap)
	{
//		SetTexture(6, *Surface.EnvironmentMap, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog);
		TexMaps.TexCoords[6] = glm::vec4(TexInfo[6].UMult, TexInfo[6].VMult, TexInfo[6].UPan, TexInfo[6].VPan);
	}

	// UDot/VDot
	TexMaps.TexCoords[7] = glm::vec4(Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, 0.f);
	TexMaps.TexCoords[8] = glm::vec4(Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, 0.f);
	TexMaps.TexCoords[9] = glm::vec4(Facet.MapCoords.Origin.X, Facet.MapCoords.Origin.Y, Facet.MapCoords.Origin.Z, 0.f);

	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
	{
		INT NumPts = Poly->NumPts;
		if (NumPts < 3) //Skip invalid polygons,if any?
			continue;

		for ( INT i=0; i<NumPts-2; i++ )
		{
			BufferComplexSurfacePoint( &DrawComplexVertsSingleBuf[DrawComplexBufferData.IndexOffset], Poly->Pts[0], TexMaps, Facet, DrawComplexBufferData, InterleaveCounter );
			DrawComplexBufferData.IndexOffset += InterleaveCounter;
			BufferComplexSurfacePoint( &DrawComplexVertsSingleBuf[DrawComplexBufferData.IndexOffset], Poly->Pts[i+1], TexMaps, Facet, DrawComplexBufferData, InterleaveCounter );
			DrawComplexBufferData.IndexOffset += InterleaveCounter;
			BufferComplexSurfacePoint( &DrawComplexVertsSingleBuf[DrawComplexBufferData.IndexOffset], Poly->Pts[i+2], TexMaps, Facet, DrawComplexBufferData, InterleaveCounter );
			DrawComplexBufferData.IndexOffset += InterleaveCounter;
		}

		if(NoBuffering)
			DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps, DrawColor);

		if ( DrawComplexBufferData.IndexOffset >= DRAWCOMPLEX_SIZE )
		{
			GWarn->Logf(TEXT("DrawComplexSurface overflow!"));
			break;
		}
	}
	DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps, DrawColor);
	CHECK_GL_ERROR();
#if ENGINE_VERSION!=227
	if(bBumpMap)
		Surface.Texture->Texture->BumpMap->Unlock(BumpMapInfo);
#endif
	unclock(Stats.ComplexCycles);

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

void UXOpenGLRenderDevice::DrawComplexVertsSinglePass(DrawComplexBuffer &Buffer, DrawComplexTexMaps TexMaps, FPlane DrawColor)
{
	GLuint TotalSize = 2 * Buffer.VertSize;
	GLuint StrideSize = 2 * VertFloatSize;

	if (UseBufferInvalidation && !UsePersistentBuffers)
		glInvalidateBufferData(DrawComplexVertBufferSinglePass);

	// PolyFlags
	glUniform1ui(DrawComplexSinglePassPolyFlags, Buffer.PolyFlags);

	// Gamma
	glUniform1f(DrawComplexSinglePassGamma,Gamma);

	// Data
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * TotalSize, DrawComplexVertsSingleBuf, GL_STREAM_DRAW);

	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glEnableVertexAttribArray(NORMALS_ATTRIB);// SurfNormals

	glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, StrideSize, 0);
	glVertexAttribPointer(NORMALS_ATTRIB, 3, GL_FLOAT, GL_FALSE, StrideSize, (void*)VertFloatSize);

	//DistanceFog
	glUniform4f(DrawComplexSinglePassFogColor,	DrawComplexFogSurface.FogColor.X, DrawComplexFogSurface.FogColor.Y, DrawComplexFogSurface.FogColor.Z, DrawComplexFogSurface.FogColor.W);
	glUniform1f(DrawComplexSinglePassFogStart,	DrawComplexFogSurface.FogDistanceStart);
	glUniform1f(DrawComplexSinglePassFogEnd,	DrawComplexFogSurface.FogDistanceEnd);
	glUniform1f(DrawComplexSinglePassFogDensity,DrawComplexFogSurface.FogDensity);
	glUniform1i(DrawComplexSinglePassFogMode,	DrawComplexFogSurface.FogMode);

	// Light position (right now for bumpmaps only).
	glUniform4fv(DrawComplexSinglePassLightPos, 8, (const GLfloat*)TexMaps.LightPos);

	// Tex UV's
	glUniform4fv(DrawComplexSinglePassTexCoords, 10, (const GLfloat*)TexMaps.TexCoords);

	// Textures to be applied.
	INT ComplexAdds[6];
	ComplexAdds[0] = TexMaps.bLightMap;
	ComplexAdds[1] = TexMaps.bDetailTex;
	ComplexAdds[2] = TexMaps.bMacroTex;
	ComplexAdds[3] = TexMaps.bBumpMap;
	ComplexAdds[4] = TexMaps.bFogMap;
	ComplexAdds[5] = TexMaps.bEnvironmentMap;

	glUniform1iv(DrawComplexSinglePassComplexAdds, 5, (const GLint*)ComplexAdds);

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
			glUniform4f(DrawComplexSinglePassDrawColor, DrawColor.X, DrawColor.Y, DrawColor.Z, DrawColor.W);
			glUniform1i(DrawComplexSinglePassbHitTesting, false);
		}
		glUniform1ui(DrawComplexSinglePassRendMap, Buffer.RendMap);
		CHECK_GL_ERROR();
	}

	// Draw
	glDrawArrays(GL_TRIANGLES, 0, Buffer.VertSize / FloatsPerVertex);

	DrawComplexBufferData.VertSize = 0;
	DrawComplexBufferData.TexSize = 0;
	DrawComplexBufferData.IndexOffset = 0;

	// Clean up
	glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glDisableVertexAttribArray(NORMALS_ATTRIB);

	CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

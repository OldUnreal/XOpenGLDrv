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


inline void UXOpenGLRenderDevice::BufferComplexSurfacePoint( FLOAT* DrawComplexTemp, FTransform* P, DrawComplexTexMaps TexMaps, FSurfaceFacet& Facet, DrawComplexBuffer& BufferData )
{
	// Points
	DrawComplexTemp[0] = P->Point.X;
	DrawComplexTemp[1] = P->Point.Y;
	DrawComplexTemp[2] = P->Point.Z;
	DrawComplexTemp[3] = 1.f;
	BufferData.VertSize += FloatsPerVertex;

	// Normals
	DrawComplexTemp[4] = TexMaps.SurfNormal.X;
	DrawComplexTemp[5] = TexMaps.SurfNormal.Y;
	DrawComplexTemp[6] = TexMaps.SurfNormal.Z;
	DrawComplexTemp[7] = TexMaps.SurfNormal.W;

	BufferData.IndexOffset += 8;
}

void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UXOpenGLRenderDevice::DrawComplexSurface);
	check(Surface.Texture);

	if(Frame->Recursion > MAX_FRAME_RECURSION || NoDrawComplexSurface)
		return;

	clock(Stats.ComplexCycles);

	TexMaps.DrawFlags = DF_DiffuseTexture;

	if (Surface.LightMap)
        TexMaps.DrawFlags |= DF_LightMap;

    if (Surface.FogMap)
        TexMaps.DrawFlags |= DF_FogMap;

    if (Surface.DetailTexture && DetailTextures)
        TexMaps.DrawFlags |= DF_DetailTexture;

    if (Surface.MacroTexture && MacroTextures)
        TexMaps.DrawFlags |= DF_MacroTexture;

#if ENGINE_VERSION==227
    if (Surface.BumpMap && BumpMaps)
        TexMaps.DrawFlags |= DF_BumpMap;

    if (Surface.EnvironmentMap && EnvironmentMaps)// not yet implemented.
        TexMaps.DrawFlags |= DF_EnvironmentMap;

	TexMaps.SurfNormal = Surface.SurfNormal;
#else
	FTextureInfo BumpMapInfo;
	if(Surface.Texture && Surface.Texture->Texture && Surface.Texture->Texture->BumpMap)
	{
#if ENGINE_VERSION==1100
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, Viewport->CurrentTime, 0, this);
#else
		Surface.Texture->Texture->BumpMap->Lock(BumpMapInfo, FTime(), 0, this);
#endif
		TexMaps.DrawFlags |= DF_BumpMap;
	}
	//TexMaps.LightPos[0] = glm::vec4(1.f, 1.f, 1.f,1.f);
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

	if (DrawComplexBufferData.VertSize > 0 && (DrawComplexBufferData.PolyFlags != Surface.PolyFlags))
		DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);

	DWORD PolyFlags=SetBlend(Surface.PolyFlags, ComplexSurfaceSinglePass_Prog, false);
	DrawComplexBufferData.PolyFlags = PolyFlags;

    SetTexture(0, *Surface.Texture, Surface.PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog,NORMALTEX);
    TexMaps.TexCoords[0] = glm::vec4(TexInfo[0].UMult, TexInfo[0].VMult, TexInfo[0].UPan, TexInfo[0].VPan);

    if (UseBindlessTextures)
        DrawComplexBufferData.TexNum[0] = TexInfo[0].TexNum;

	if (TexMaps.DrawFlags & DF_LightMap) //can not make use of bindless, to many single textures. Determined by Info->Texture.
	{
		SetTexture(1, *Surface.LightMap, PolyFlags, -0.5, ComplexSurfaceSinglePass_Prog, LIGHTMAP); //First parameter has to fit the uniform in the fragment shader
		TexMaps.TexCoords[1] = glm::vec4(TexInfo[1].UMult, TexInfo[1].VMult, TexInfo[1].UPan, TexInfo[1].VPan);
        if (UseBindlessTextures)
            DrawComplexBufferData.TexNum[1] = TexInfo[1].TexNum;
	}
	if (TexMaps.DrawFlags & DF_DetailTexture)
	{
		SetTexture(2, *Surface.DetailTexture, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, DETAILTEX);
		TexMaps.TexCoords[2] = glm::vec4(TexInfo[2].UMult, TexInfo[2].VMult, TexInfo[2].UPan, TexInfo[2].VPan);
        if (UseBindlessTextures)
            DrawComplexBufferData.TexNum[2] = TexInfo[2].TexNum;
	}
	if (TexMaps.DrawFlags & DF_MacroTexture)
	{
		SetTexture(3, *Surface.MacroTexture, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, MACROTEX);
		TexMaps.TexCoords[3] = glm::vec4(TexInfo[3].UMult, TexInfo[3].VMult, TexInfo[3].UPan, TexInfo[3].VPan);
        if (UseBindlessTextures)
            DrawComplexBufferData.TexNum[3] = TexInfo[3].TexNum;
		TexMaps.TexCoords[13] = glm::vec4(Surface.MacroTexture->Texture->Diffuse, Surface.MacroTexture->Texture->Specular, Surface.MacroTexture->Texture->Alpha, Surface.MacroTexture->Texture->Scale);
	}
	if (TexMaps.DrawFlags & DF_BumpMap)
	{
#if ENGINE_VERSION==227
		SetTexture(4, *Surface.BumpMap, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, BUMPMAP);
		TexMaps.TexCoords[4] = glm::vec4(TexInfo[4].UMult, TexInfo[4].VMult, TexInfo[4].UPan, TexInfo[4].VPan);
#else
		SetTexture(4, BumpMapInfo, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, BUMPMAP);
#endif
        if (UseBindlessTextures)
            DrawComplexBufferData.TexNum[4] = TexInfo[4].TexNum;
		TexMaps.TexCoords[12] = glm::vec4(Surface.Texture->Texture->BumpMap->Diffuse, Surface.Texture->Texture->BumpMap->Specular, Surface.Texture->Texture->BumpMap->Alpha, Surface.Texture->Texture->BumpMap->Scale);
	}
	if (TexMaps.DrawFlags & DF_FogMap)
	{
		SetTexture(5, *Surface.FogMap, PF_AlphaBlend, -0.5, ComplexSurfaceSinglePass_Prog, FOGMAP);
		TexMaps.TexCoords[5] = glm::vec4(TexInfo[5].UMult, TexInfo[5].VMult, TexInfo[5].UPan, TexInfo[5].VPan);
        if (UseBindlessTextures)
            DrawComplexBufferData.TexNum[5] = TexInfo[5].TexNum;
	}
#if ENGINE_VERSION==227
	if (TexMaps.DrawFlags & DF_EnvironmentMap)
	{
		SetTexture(6, *Surface.EnvironmentMap, PolyFlags, 0.0, ComplexSurfaceSinglePass_Prog, ENVIRONMENTMAP);
		TexMaps.TexCoords[6] = glm::vec4(TexInfo[6].UMult, TexInfo[6].VMult, TexInfo[6].UPan, TexInfo[6].VPan);
        if (UseBindlessTextures)
            DrawComplexBufferData.TexNum[6] = TexInfo[6].TexNum;
	}
#endif

	// UDot/VDot
	TexMaps.TexCoords[7] = glm::vec4(Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, 0.f);
	TexMaps.TexCoords[8] = glm::vec4(Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, 0.f);
	TexMaps.TexCoords[9] = glm::vec4(Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, 0.f);
	TexMaps.TexCoords[10] = glm::vec4(Facet.MapCoords.Origin.X, Facet.MapCoords.Origin.Y, Facet.MapCoords.Origin.Z, 0.f);
	TexMaps.TexCoords[11] = glm::vec4(Surface.Texture->Texture->Diffuse, Surface.Texture->Texture->Specular, Surface.Texture->Texture->Alpha, Surface.Texture->Texture->Scale);

	 //*(INT*)&TexMaps.TexCoords[11].w = (INT)Surface.Texture->Format;

    // Additional maps.
	TexMaps.TexCoords[14] = glm::vec4(0.f, 0.f, 0.f, 0.f);
	TexMaps.TexCoords[15] = glm::vec4(0.f, 0.f, (GLfloat)Surface.Texture->Texture->Format, (GLfloat)TexMaps.DrawFlags);

	//debugf(TEXT("Facet.MapCoords.XAxis.X %f, Facet.MapCoords.XAxis.Y %f, Facet.MapCoords.XAxis.Z %f,Facet.MapCoords.YAxis.X %f , Facet.MapCoords.YAxis.Y %f, Facet.MapCoords.YAxis.Z %f,Facet.MapCoords.ZAxis.X %f, Facet.MapCoords.ZAxis.Y %f, Facet.MapCoords.ZAxis.Z %f Facet.MapCoords.Origin.X %f, Facet.MapCoords.Origin.Y %f, Facet.MapCoords.Origin.Z %f"), Facet.MapCoords.XAxis.X, Facet.MapCoords.XAxis.Y, Facet.MapCoords.XAxis.Z, Facet.MapCoords.YAxis.X, Facet.MapCoords.YAxis.Y, Facet.MapCoords.YAxis.Z, Facet.MapCoords.ZAxis.X, Facet.MapCoords.ZAxis.Y, Facet.MapCoords.ZAxis.Z, Facet.MapCoords.Origin.X, Facet.MapCoords.Origin.Y, Facet.MapCoords.Origin.Z);

	if (DrawComplexSinglePassRange.Sync[DrawComplexBufferData.Index])
            WaitBuffer(DrawComplexSinglePassRange, DrawComplexBufferData.Index);

    for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
    {
        INT NumPts = Poly->NumPts;
        if (NumPts < 3) //Skip invalid polygons,if any?
            continue;

        for ( INT i=0; i<NumPts-2; i++ )
        {
            BufferComplexSurfacePoint( &DrawComplexSinglePassRange.Buffer[DrawComplexBufferData.BeginOffset + DrawComplexBufferData.IndexOffset], Poly->Pts[0], TexMaps, Facet, DrawComplexBufferData );
            BufferComplexSurfacePoint( &DrawComplexSinglePassRange.Buffer[DrawComplexBufferData.BeginOffset + DrawComplexBufferData.IndexOffset], Poly->Pts[i+1], TexMaps, Facet, DrawComplexBufferData );
            BufferComplexSurfacePoint( &DrawComplexSinglePassRange.Buffer[DrawComplexBufferData.BeginOffset + DrawComplexBufferData.IndexOffset], Poly->Pts[i+2], TexMaps, Facet, DrawComplexBufferData );
        }
        if ( DrawComplexBufferData.IndexOffset >= DRAWCOMPLEX_SIZE - DrawComplexStrideSize)
        {
            GWarn->Logf(TEXT("DrawComplexSurface overflow!"));
            break;
        }
    }
    //debugf(TEXT("DrawComplexBufferData.IndexOffset %i DrawComplexBufferData.Index %i DrawComplexBufferData.BeginOffset %i"),DrawComplexBufferData.IndexOffset, DrawComplexBufferData.Index, DrawComplexBufferData.BeginOffset);

	//if (!UseHWLighting)
		DrawComplexVertsSinglePass(DrawComplexBufferData, TexMaps);
	CHECK_GL_ERROR();
#if ENGINE_VERSION!=227
	if(TexMaps.DrawFlags & DF_BumpMap)
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

void UXOpenGLRenderDevice::DrawComplexVertsSinglePass(DrawComplexBuffer &BufferData, DrawComplexTexMaps TexMaps)
{
	GLuint TotalSize = BufferData.IndexOffset;
    CHECK_GL_ERROR();

	// Data
	if (!UsePersistentBuffersComplex)
    {
        if (UseBufferInvalidation)
            glInvalidateBufferData(DrawComplexVertBuffer);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * TotalSize, DrawComplexSinglePassRange.Buffer);
    }

    // PolyFlags
	glUniform1ui(DrawComplexSinglePassPolyFlags, BufferData.PolyFlags);
	CHECK_GL_ERROR();

    // TexNum and Textures to be applied.
    glUniform1uiv(DrawComplexSinglePassTexNum, 8, (const GLuint*) BufferData.TexNum);
    CHECK_GL_ERROR();

	// Gamma
	glUniform1f(DrawComplexSinglePassGamma,Gamma);

	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glEnableVertexAttribArray(NORMALS_ATTRIB);// SurfNormals

	GLuint BeginOffset = BufferData.BeginOffset * sizeof(float);
	glVertexAttribPointer(VERTEX_COORD_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (void*)BeginOffset);
	glVertexAttribPointer(NORMALS_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawComplexStrideSize, (void*)(	BeginOffset + FloatSize4));

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
	glDrawArrays(GL_TRIANGLES, 0, (BufferData.VertSize / FloatsPerVertex));
    CHECK_GL_ERROR();

	if(UsePersistentBuffersComplex)
	{
		LockBuffer(DrawComplexSinglePassRange, BufferData.Index);
		BufferData.Index = (BufferData.Index + 1) % NUMBUFFERS;
		CHECK_GL_ERROR();
	}

	BufferData.BeginOffset = BufferData.Index * DRAWCOMPLEX_SIZE;

	BufferData.VertSize = 0;
	BufferData.TexSize = 0;
	BufferData.IndexOffset = 0;
	for (INT i = 0; i < ARRAY_COUNT(BufferData.TexNum);i++)
		BufferData.TexNum[i] = 0;

	// Clean up
	glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glDisableVertexAttribArray(NORMALS_ATTRIB);

	CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

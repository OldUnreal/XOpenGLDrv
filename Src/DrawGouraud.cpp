/*=============================================================================
	DrawGouraud.cpp: Unreal XOpenGL DrawGouraud routines.
	Used for drawing meshes.

	VertLists are only supported by 227 so far, it pushes verts in a huge
	list instead of vertice by vertice. Currently this method improves
	performance 10x and more compared to unbuffered calls. Buffering
	catches up quite some.
	Copyright 2014-2017 Oldunreal

	Todo:
        * On a long run this should be replaced with a more mode
          modern mesh rendering method, but this requires also quite some
          rework in Render.dll and will be not compatible with other
          UEngine1 games.

	Revision history:
		* Created by Smirftsch
		* Added buffering to DrawGouraudPolygon
		* implemented proper usage of persistent buffers.
		* Added bindless texture support.

=============================================================================*/

// Include GLM
#ifdef _MSC_VER
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"
//#include "UnStaticLight.h"

inline void UXOpenGLRenderDevice::BufferGouraudPolygonPoint( FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& BufferData )
{
	// Verts
	DrawGouraudTemp[0] = P->Point.X;
	DrawGouraudTemp[1] = P->Point.Y;
	DrawGouraudTemp[2] = P->Point.Z;
	BufferData.VertSize += FloatsPerVertex; // Points

	// Textures
	DrawGouraudTemp[3] = P->U;
	DrawGouraudTemp[4] = P->V;

	DrawGouraudTemp[5] = P->Normal.X;
	DrawGouraudTemp[6] = P->Normal.Y;
	DrawGouraudTemp[7] = P->Normal.Z;
	DrawGouraudTemp[8] = P->Normal.W;

	// Light color
	DrawGouraudTemp[9] = P->Light.X;
	DrawGouraudTemp[10] = P->Light.Y;
	DrawGouraudTemp[11] = P->Light.Z;
	DrawGouraudTemp[12] = P->Light.W;

	// Vertex Fog color
	DrawGouraudTemp[13] = P->Fog.X;
	DrawGouraudTemp[14] = P->Fog.Y;
	DrawGouraudTemp[15] = P->Fog.Z;
	DrawGouraudTemp[16] = P->Fog.W;

	DrawGouraudTemp[17] = (GLfloat)BufferData.TexNum[0];
	DrawGouraudTemp[18] = (GLfloat)BufferData.TexNum[1];
	DrawGouraudTemp[19] = (GLfloat)BufferData.TexNum[2];

	DrawGouraudTemp[20] = BufferData.TexUMult;
	DrawGouraudTemp[21] = BufferData.TexVMult;
	DrawGouraudTemp[22] = BufferData.MacroTexUMult;
	DrawGouraudTemp[23] = BufferData.MacroTexVMult;

	DrawGouraudTemp[24] = BufferData.DetailTexUMult;
	DrawGouraudTemp[25] = BufferData.DetailTexVMult;
	DrawGouraudTemp[26] = BufferData.TextureFormat;
	DrawGouraudTemp[27] = (GLfloat)BufferData.DrawFlags;

	// Additional texture info. Added here to keep batched drawing.
	DrawGouraudTemp[28] = BufferData.TextureDiffuse;
	DrawGouraudTemp[29] = BufferData.BumpTextureSpecular;
	DrawGouraudTemp[30] = BufferData.TextureAlpha;
	DrawGouraudTemp[31] = (GLfloat)BufferData.TexNum[3];

    BufferData.IndexOffset += 32;
}
inline void UXOpenGLRenderDevice::BufferGouraudPolygonVert(FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& BufferData)
{
	// Verts
	DrawGouraudTemp[0] = P->Point.X;
	DrawGouraudTemp[1] = P->Point.Y;
	DrawGouraudTemp[2] = P->Point.Z;
	BufferData.VertSize += FloatsPerVertex; // Points
}

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3 || Frame->Recursion > MAX_FRAME_RECURSION || NoDrawGouraud) //reject invalid.
		return;

	SetProgram(GouraudPolyVert_Prog);

	if ( (DrawGouraudBufferData.VertSize > 0) && ((!UseBindlessTextures && (TexInfo[0].CurrentCacheID != Info.CacheID)) || (DrawGouraudBufferData.PolyFlags != PolyFlags)))
	{
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
	}

	//debugf(TEXT("VertSize %i TexSize %i, ColorSize %i"), VertSize, TexSize, ColorSize); //18/12/24
	DrawGouraudBufferData.PolyFlags = SetBlend(PolyFlags, GouraudPolyVert_Prog, false);
	SetTexture(0, Info, DrawGouraudBufferData.PolyFlags, 0, GouraudPolyVert_Prog, NORMALTEX);

    DrawGouraudBufferData.TexNum[0] = TexInfo[0].TexNum;
	DrawGouraudBufferData.Alpha = Info.Texture->Alpha;
	DrawGouraudBufferData.TextureDiffuse = Info.Texture->Diffuse;
	DrawGouraudBufferData.TexUMult = TexInfo[0].UMult;
	DrawGouraudBufferData.TexVMult = TexInfo[0].VMult;

	if (GIsEditor)
	{
		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawGouraudBufferData.RendMap = Frame->Viewport->Actor->RendMap;
	}

	DrawGouraudBufferData.DrawFlags = DF_DiffuseTexture;

	FTextureInfo DetailTextureInfo;
	FTextureInfo MacroTextureInfo;
	FTextureInfo BumpMapInfo;

	if (Info.Texture)
    {
        DrawGouraudBufferData.TextureFormat = (GLfloat)Info.Texture->Format;
        DrawGouraudBufferData.TextureAlpha = (GLfloat)Info.Texture->Alpha;

        if (Info.Texture->DetailTexture && DetailTextures)
        {

            Info.Texture->DetailTexture->Lock(DetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_DetailTexture;

            SetTexture(1, DetailTextureInfo, Info.Texture->DetailTexture->PolyFlags, 0.0, GouraudPolyVert_Prog, DETAILTEX);
            DrawGouraudBufferData.TexNum[1] = TexInfo[1].TexNum;

            DrawGouraudBufferData.DetailTexUMult = TexInfo[1].UMult;
            DrawGouraudBufferData.DetailTexVMult = TexInfo[1].VMult;
        }

    #if ENGINE_VERSION==227
        if (Info.Texture->BumpMap && BumpMaps)
        {
            Info.Texture->BumpMap->Lock(BumpMapInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_BumpMap;

            SetTexture(2, BumpMapInfo, Info.Texture->BumpMap->PolyFlags, 0.0, GouraudPolyVert_Prog, BUMPMAP);
            DrawGouraudBufferData.TexNum[2] = TexInfo[2].TexNum; //using Base Texture UV.

            DrawGouraudBufferData.BumpTextureSpecular = Info.Texture->BumpMap->Specular;
        }
    #endif // ENGINE_VERSION

        if (Info.Texture->MacroTexture && MacroTextures)
        {

            Info.Texture->MacroTexture->Lock(MacroTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_MacroTexture;

            SetTexture(3, MacroTextureInfo, Info.Texture->MacroTexture->PolyFlags, 0.0, GouraudPolyVert_Prog, MACROTEX);
            DrawGouraudBufferData.TexNum[3] = TexInfo[3].TexNum;

            DrawGouraudBufferData.MacroTexUMult = TexInfo[3].UMult;
            DrawGouraudBufferData.MacroTexVMult = TexInfo[3].VMult;
        }
    }

	clockFast(Stats.GouraudPolyCycles);
    CHECK_GL_ERROR();

    if (UsePersistentBuffersGouraud && DrawGouraudBufferRange.Sync[DrawGouraudBufferData.Index])
        WaitBuffer(DrawGouraudBufferRange, DrawGouraudBufferData.Index);
    CHECK_GL_ERROR();

    for ( INT i=0; i<NumPts-2; i++ )
    {
        BufferGouraudPolygonPoint(&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset], Pts[0], DrawGouraudBufferData);
        BufferGouraudPolygonPoint(&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset], Pts[i + 1], DrawGouraudBufferData);
        BufferGouraudPolygonPoint(&DrawGouraudBufferRange.Buffer[DrawGouraudBufferData.BeginOffset + DrawGouraudBufferData.IndexOffset], Pts[i + 2], DrawGouraudBufferData);
		if ( DrawGouraudBufferData.IndexOffset >= (DRAWGOURAUDPOLY_SIZE - DrawGouraudStrideSize))
        {
            DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
            debugf(NAME_Dev, TEXT("DrawGouraudPolygon overflow!"));
        }
    }
    CHECK_GL_ERROR();

    if (NoBuffering)
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);

	if (DrawGouraudBufferData.DrawFlags & DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(DetailTextureInfo);

	if (DrawGouraudBufferData.DrawFlags & DF_BumpMap)
		Info.Texture->BumpMap->Unlock(BumpMapInfo);

    if (DrawGouraudBufferData.DrawFlags & DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(MacroTextureInfo);


	CHECK_GL_ERROR();
	unclockFast(Stats.GouraudPolyCycles);
	unguard;
}

#if ENGINE_VERSION==227
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, AActor* Owner)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);
	if (NumPts < 3 || Frame->Recursion > MAX_FRAME_RECURSION || NoDrawGouraudList)		//reject invalid.
		return;

	SetProgram(GouraudPolyVertList_Prog);

	if (DrawGouraudListBufferData.VertSize > 0 && DrawGouraudListBufferData.PolyFlags != PolyFlags)
	{
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
	}

    bool bInverseOrder = false;
	/*
	FMeshVertCacheData* MemData = NULL;
	FVector Scale = FVector(1.f, 1.f, 1.f);
	if (Owner)// && Owner->MeshDataPtr)
	{
		//(FMeshVertCacheData*)Owner->MeshDataPtr;
		Scale = (Owner->DrawScale*Owner->DrawScale3D);
		bInverseOrder = ((Scale.X*Scale.Y*Scale.Z)<0.f) && !(PolyFlags & PF_TwoSided);
	}
	*/

    DrawGouraudListBufferData.PolyFlags=SetBlend(PolyFlags, GouraudPolyVertList_Prog, bInverseOrder);
    DrawGouraudListBufferData.DrawFlags = DF_DiffuseTexture;
	SetTexture(0, Info, DrawGouraudListBufferData.PolyFlags, 0, GouraudPolyVertList_Prog, NORMALTEX);

    DrawGouraudListBufferData.TexNum[0] = TexInfo[0].TexNum;
	DrawGouraudListBufferData.Alpha = Info.Texture->Alpha;
	DrawGouraudListBufferData.TextureDiffuse = Info.Texture->Diffuse;
	DrawGouraudListBufferData.TexUMult = TexInfo[0].UMult;
	DrawGouraudListBufferData.TexVMult = TexInfo[0].VMult;


	if (GIsEditor)
	{
		if (Frame->Viewport->Actor) // needed? better safe than sorry.
			DrawGouraudListBufferData.RendMap = Frame->Viewport->Actor->RendMap;
	}

	FTextureInfo DetailTextureInfo;
	FTextureInfo BumpMapInfo;
	FTextureInfo MacroTextureInfo;

	if (Info.Texture)
    {
        DrawGouraudListBufferData.TextureFormat = (GLfloat)Info.Texture->Format;
        DrawGouraudListBufferData.TextureAlpha = (GLfloat)Info.Texture->Alpha;

        if (Info.Texture->DetailTexture && DetailTextures)
        {
            Info.Texture->DetailTexture->Lock(DetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudListBufferData.DrawFlags |= DF_DetailTexture;

            SetTexture(1, DetailTextureInfo, Info.Texture->DetailTexture->PolyFlags, 0.0, GouraudPolyVertList_Prog, DETAILTEX);
            DrawGouraudListBufferData.TexNum[1] =TexInfo[1].TexNum;

            DrawGouraudListBufferData.DetailTexUMult = TexInfo[1].UMult;
            DrawGouraudListBufferData.DetailTexVMult = TexInfo[1].VMult;
        }

        if (Info.Texture->BumpMap && BumpMaps)
        {
            Info.Texture->BumpMap->Lock(BumpMapInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudListBufferData.DrawFlags |= DF_BumpMap;

            SetTexture(2, BumpMapInfo, Info.Texture->BumpMap->PolyFlags, 0.0, GouraudPolyVertList_Prog, BUMPMAP);
            DrawGouraudListBufferData.TexNum[2] = TexInfo[2].TexNum; //using Base Texture UV.

            DrawGouraudListBufferData.BumpTextureSpecular = Info.Texture->BumpMap->Specular;
        }

        if (Info.Texture->MacroTexture && MacroTextures)
        {
            Info.Texture->MacroTexture->Lock(MacroTextureInfo, Frame->Viewport->CurrentTime, -1, this);
            DrawGouraudBufferData.DrawFlags |= DF_MacroTexture;

            SetTexture(3, MacroTextureInfo, Info.Texture->MacroTexture->PolyFlags, 0.0, GouraudPolyVertList_Prog, MACROTEX);
            DrawGouraudBufferData.TexNum[3] = TexInfo[3].TexNum;

            DrawGouraudBufferData.MacroTexUMult = TexInfo[3].UMult;
            DrawGouraudBufferData.MacroTexVMult = TexInfo[3].VMult;
        }
    }

	CHECK_GL_ERROR();
	//debugf(TEXT("PolyList: VertSize %i TexSize %i, ColorSize %i"), VertSize, TexSize, ColorSize); // VertSize 12288 TexSize 8192, ColorSize 16384

	clockFast(Stats.GouraudPolyListCycles);
    if (UsePersistentBuffersGouraud && DrawGouraudListBufferRange.Sync[DrawGouraudListBufferData.Index])
        WaitBuffer(DrawGouraudListBufferRange, DrawGouraudListBufferData.Index);

    for (INT i = 0; i < NumPts; i++)
    {
        FTransTexture* P = &Pts[i];
        BufferGouraudPolygonPoint(&DrawGouraudListBufferRange.Buffer[DrawGouraudListBufferData.BeginOffset + DrawGouraudListBufferData.IndexOffset], P, DrawGouraudListBufferData);

        if ( DrawGouraudListBufferData.IndexOffset >= (DRAWGOURAUDPOLYLIST_SIZE - DrawGouraudStrideSize))
        {
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);
            debugf(NAME_Dev, TEXT("DrawGouraudPolyList overflow!"));
        }
    }

    if (NoBuffering)
        DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudListBufferData);

	if (DrawGouraudListBufferData.DrawFlags & DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(DetailTextureInfo);

	if (DrawGouraudListBufferData.DrawFlags & DF_BumpMap)
		Info.Texture->BumpMap->Unlock(BumpMapInfo);

    if (DrawGouraudListBufferData.DrawFlags & DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(MacroTextureInfo);

	unclockFast(Stats.GouraudPolyListCycles);
	CHECK_GL_ERROR();

	unguard;
}
#endif

void UXOpenGLRenderDevice::DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer& BufferData)
{
	// Using one huge buffer instead of 3, interleaved data.
	// This is to reduce overhead by reducing API calls.

	GLuint TotalSize = BufferData.IndexOffset;
	CHECK_GL_ERROR();

	if(!UsePersistentBuffersGouraud)
	{
	    if (UseBufferInvalidation)
        {
            if(ActiveProgram == GouraudPolyVertList_Prog)
                glInvalidateBufferData(DrawGouraudVertListBuffer);
            else if (ActiveProgram == GouraudPolyVert_Prog)
                glInvalidateBufferData(DrawGouraudVertBuffer);
            CHECK_GL_ERROR();
        }

        if (ActiveProgram == GouraudPolyVertList_Prog)
			glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(float), DrawGouraudListBufferRange.Buffer);
        else if (ActiveProgram == GouraudPolyVert_Prog)
			glBufferSubData(GL_ARRAY_BUFFER, 0, TotalSize * sizeof(float), DrawGouraudBufferRange.Buffer);

		// GL > 4.5 for later use maybe.
		/*
		if (ActiveProgram == GouraudPolyVertList_Prog)
			glNamedBufferData(DrawGouraudVertListBuffer, sizeof(float) * TotalSize, verts, GL_STREAM_DRAW);
		else if (ActiveProgram == GouraudPolyVert_Prog)
			glNamedBufferData(DrawGouraudVertBuffer, sizeof(float) * TotalSize, verts, GL_STREAM_DRAW);
		CHECK_GL_ERROR();
		*/
	}
/*
	else
	{
        if (ActiveProgram == GouraudPolyVertList_Prog)
            glFlushMappedNamedBufferRange(DrawGouraudVertListBuffer, sizeof(float) * BufferData.BeginOffset, TotalSize * sizeof(float));
        else if (ActiveProgram == GouraudPolyVert_Prog)
            glFlushMappedNamedBufferRange(DrawGouraudVertBuffer, sizeof(float) * BufferData.BeginOffset, TotalSize * sizeof(float));
	}
*/

	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
	glEnableVertexAttribArray(NORMALS_ATTRIB);
	glEnableVertexAttribArray(COLOR_ATTRIB);
	glEnableVertexAttribArray(FOGMAP_COORD_ATTRIB);//here for VertexFogColor
	glEnableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
	glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
	glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
	glEnableVertexAttribArray(TEXTURE_ATTRIB);

	PTRINT BeginOffset = BufferData.BeginOffset * sizeof(float);

	glVertexAttribPointer(VERTEX_COORD_ATTRIB,		3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)	BeginOffset);
	glVertexAttribPointer(TEXTURE_COORD_ATTRIB,		2, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3));
	glVertexAttribPointer(NORMALS_ATTRIB,			4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2));
	glVertexAttribPointer(COLOR_ATTRIB,				4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4));
	glVertexAttribPointer(FOGMAP_COORD_ATTRIB,		4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4));
	glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB,	3, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4_4));
	glVertexAttribPointer(TEXTURE_COORD_ATTRIB2,	4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4_4_3));// Tex U/V Mult, MacroTex U/V Mult
	glVertexAttribPointer(TEXTURE_COORD_ATTRIB3,	4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset	+ FloatSize3_2_4_4_4_3_4));// DetailTex U/V Mult, Texture Format, DrawFlags
	glVertexAttribPointer(TEXTURE_ATTRIB,			4, GL_FLOAT, GL_FALSE, DrawGouraudStrideSize, (void*)(	BeginOffset + FloatSize3_2_4_4_4_3_4_4));// Additional texture information
	CHECK_GL_ERROR();

    //PolyFlags
	glUniform1ui(DrawGouraudPolyFlags, BufferData.PolyFlags);

    // Gamma
    glUniform1f(DrawGouraudGamma, Gamma);
	CHECK_GL_ERROR();

	//set DrawColor if any
	if (GIsEditor)
	{
		if (HitTesting()) // UED only.
		{
			glUniform4f(DrawGouraudDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
			glUniform1i(DrawGouraudbHitTesting, true);
		}
		else
		{
			if (BufferData.Alpha > 0.f)
				glUniform4f(DrawGouraudDrawColor, 0.f, 0.f, 0.f, BufferData.Alpha);
			else glUniform4f(DrawGouraudDrawColor, BufferData.DrawColor.X, BufferData.DrawColor.Y, BufferData.DrawColor.Z, BufferData.DrawColor.W);
			glUniform1i(DrawGouraudbHitTesting, false);
		}
		glUniform1ui(DrawGouraudRendMap, BufferData.RendMap);
		CHECK_GL_ERROR();
	}

    CHECK_GL_ERROR();

	// Draw
	glDrawArrays(Mode, 0, (BufferData.VertSize / FloatsPerVertex));

	if(UsePersistentBuffersGouraud)
	{
		if (ActiveProgram == GouraudPolyVertList_Prog)
		{
			BufferData.BeginOffset = BufferData.Index * DRAWGOURAUDPOLYLIST_SIZE;
			LockBuffer(DrawGouraudListBufferRange, BufferData.Index);
		}
		else if (ActiveProgram == GouraudPolyVert_Prog)
		{
			BufferData.BeginOffset = BufferData.Index * DRAWGOURAUDPOLY_SIZE;
			LockBuffer(DrawGouraudBufferRange, BufferData.Index);
		}
		BufferData.Index = (BufferData.Index + 1) % NUMBUFFERS;
		CHECK_GL_ERROR();
	}

	// Clean up
	glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
	glDisableVertexAttribArray(NORMALS_ATTRIB);
	glDisableVertexAttribArray(COLOR_ATTRIB);
	glDisableVertexAttribArray(FOGMAP_COORD_ATTRIB);
	glDisableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
	glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
	glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
	glDisableVertexAttribArray(TEXTURE_ATTRIB);
	CHECK_GL_ERROR();

    BufferData.VertSize = 0;
    BufferData.IndexOffset = 0;
    BufferData.PolyFlags = 0;
	BufferData.Alpha = 0.f;
	for (INT i = 0; i < ARRAY_COUNT(BufferData.TexNum);i++)
		BufferData.TexNum[i] = 0;
}
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

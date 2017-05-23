/*=============================================================================
	DrawGouraud.cpp: Unreal XOpenGL DrawGouraud routines.
	Used for drawing meshes.

	VertLists are only supported by 227 so far, it pushes verts in a huge
	list instead of vertice by vertice. Currently this method improves
	performance 10x and more compared to unbuffered calls. Buffering
	catches up quite some.
	Copyright 2014-2017 Oldunreal

	Todo:
        * fix up and implement proper usage of persistent buffers.
        * On a long run this should be replaced with a more mode
          modern mesh rendering method, but this requires also quite some
          rework in Render.dll and will be not compatible with other
          UEngine1 games.

	Revision history:
		* Created by Smirftsch
		* Added buffering to DrawGouraudPolygon

=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

inline void UXOpenGLRenderDevice::BufferGouraudPolygonPoint( FLOAT* DrawGouraudTemp, FTransTexture* P, DrawGouraudBuffer& Buffer )
{
	guard(UXOpenGLRenderDevice::BufferGouraudPolygonPoint);

	DrawGouraudTemp[0] = P->Point.X;
	DrawGouraudTemp[1] = P->Point.Y;
	DrawGouraudTemp[2] = P->Point.Z;

	// Textures
	DrawGouraudTemp[3] = P->U;
	DrawGouraudTemp[4] = P->V;

	DrawGouraudTemp[5] = P->Light.X;
	DrawGouraudTemp[6] = P->Light.Y;
	DrawGouraudTemp[7] = P->Light.Z;
	DrawGouraudTemp[8] = P->Light.W;

	DrawGouraudTemp[9] = P->Fog.X;
	DrawGouraudTemp[10] = P->Fog.Y;
	DrawGouraudTemp[11] = P->Fog.Z;
	DrawGouraudTemp[12] = P->Fog.W;

	Buffer.VertSize += FloatsPerVertex; // Points
	Buffer.TexSize += TexCoords2D; // Textures
	Buffer.ColorSize += ColorInfo; // Light
	Buffer.ColorSize += ColorInfo; // Fog

	unguard;
}

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3 || Frame->Recursion > MAX_FRAME_RECURSION) //reject invalid.
		return;

	FPlane DrawColor = FPlane(0.f, 0.f, 0.f, 0.f);

	SetProgram(GouraudPolyVert_Prog);
	INT InterleaveCounter = 13; // base count for Verts, Texture, Color and FogColor.

	if(DrawGouraudBufferData.VertSize > 0 && ((TexInfo[0].CurrentCacheID!=Info.CacheID)))//|| DrawGouraudBufferData.PolyFlags != PolyFlags))
	{
		DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData, DrawColor);
	}
	//debugf(TEXT("VertSize %i TexSize %i, ColorSize %i"), VertSize, TexSize, ColorSize); //18/12/24
	PolyFlags = SetBlend(PolyFlags, GouraudPolyVert_Prog);

	SetTexture(0, Info, PolyFlags, 0, GouraudPolyVert_Prog);
	DrawGouraudBufferData.Texture = Info.Texture;
	DrawGouraudBufferData.PolyFlags = PolyFlags;
	DrawGouraudBufferData.TexUMult = TexInfo[0].UMult;
	DrawGouraudBufferData.TexVMult = TexInfo[0].VMult;

	DrawGouraudBufferData.bDetailTex = false;
	FTextureInfo DetailTextureInfo;
	if (Info.Texture && Info.Texture->DetailTexture)
	{
	    Info.Texture->DetailTexture->Lock(DetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
		DrawGouraudBufferData.DetailTexture = Info.Texture->DetailTexture;
		DrawGouraudBufferData.bDetailTex = true;
		SetTexture(1, DetailTextureInfo, DrawGouraudBufferData.DetailTexture->PolyFlags, 0.0, GouraudPolyVert_Prog);

		DrawGouraudBufferData.DetailTexUMult = TexInfo[1].UMult;
		DrawGouraudBufferData.DetailTexVMult = TexInfo[1].VMult;
	}

	clock(Stats.GouraudPolyCycles);

	if(!UsePersistentBuffers)
	{
		//debugf(TEXT("IndexOffset %i %i"),IndexOffset, DrawGouraudBufferData.Iteration);
		for ( INT i=0; i<NumPts-2; i++ )
		{
			BufferGouraudPolygonPoint( &DrawGouraudBuf[DrawGouraudBufferData.IndexOffset],Pts[0],DrawGouraudBufferData);
			DrawGouraudBufferData.IndexOffset += InterleaveCounter;
			BufferGouraudPolygonPoint( &DrawGouraudBuf[DrawGouraudBufferData.IndexOffset],Pts[i+1],DrawGouraudBufferData);
			DrawGouraudBufferData.IndexOffset += InterleaveCounter;
			BufferGouraudPolygonPoint( &DrawGouraudBuf[DrawGouraudBufferData.IndexOffset],Pts[i+2],DrawGouraudBufferData);
			DrawGouraudBufferData.IndexOffset += InterleaveCounter;
		}

		if(NoBuffering || Frame->Recursion)
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData, DrawColor);

		if (DrawGouraudBufferData.bDetailTex)
			DrawGouraudBufferData.DetailTexture->Unlock(DetailTextureInfo);

		if ( DrawGouraudBufferData.IndexOffset >= DRAWGOURAUDPOLYLIST_SIZE )
		{
			GWarn->Logf(TEXT("DrawGouraudPolygon overflow!"));
		}
	}
	else
	{
		if (DrawGouraudBufferRange[DrawGouraudIndex].Sync)
		{
			WaitBuffer(DrawGouraudBufferRange[DrawGouraudIndex]);
		}
		for ( INT i=0; i<NumPts-2; i++ )
		{
			BufferGouraudPolygonPoint(&DrawGouraudBufferRange[DrawGouraudIndex].DrawGouraudPMB[DrawGouraudBufferData.IndexOffset], Pts[0], DrawGouraudBufferData);
			DrawGouraudBufferData.IndexOffset += InterleaveCounter;
			BufferGouraudPolygonPoint(&DrawGouraudBufferRange[DrawGouraudIndex].DrawGouraudPMB[DrawGouraudBufferData.IndexOffset], Pts[i + 1], DrawGouraudBufferData);
			DrawGouraudBufferData.IndexOffset += InterleaveCounter;
			BufferGouraudPolygonPoint(&DrawGouraudBufferRange[DrawGouraudIndex].DrawGouraudPMB[DrawGouraudBufferData.IndexOffset], Pts[i + 2], DrawGouraudBufferData);
			DrawGouraudBufferData.IndexOffset += InterleaveCounter;
		}

		if(NoBuffering || Frame->Recursion)
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData, DrawColor);

		if ( DrawGouraudBufferData.IndexOffset >= DRAWGOURAUDPOLYLIST_SIZE )
		{
			GWarn->Logf(TEXT("DrawGouraudPolygon overflow!"));
		}
	}
	DrawGouraudBufferData.Iteration += 1;

	CHECK_GL_ERROR();
	unclock(Stats.GouraudPolyCycles);
	unguard;
}

#if ENGINE_VERSION==227
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, AActor* Owner)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);
	if (NumPts < 3 || Frame->Recursion > MAX_FRAME_RECURSION)		//reject invalid.
		return;

	SetProgram(GouraudPolyVertList_Prog);

	FMeshVertCacheData* MemData = (FMeshVertCacheData*)Owner->MeshDataPtr;
	FVector Scale = (Owner->DrawScale*Owner->DrawScale3D);
	UBOOL bInverseOrder = ((Scale.X*Scale.Y*Scale.Z)<0.f) && !(PolyFlags & PF_TwoSided);

	GLuint VertSize = FloatsPerVertex * NumPts;
	GLuint TexSize = TexCoords2D * NumPts;
	GLuint ColorSize = ColorInfo * NumPts;

	INT InterleaveCounter = 13; // base count for Verts, Texture, Color and FogColor.

	if (bInverseOrder)
		PolyFlags &= PF_RenderHint; //Using PF_RenderHint internally for CW/CCW switch.

	SetTexture(0, Info, PolyFlags, 0, GouraudPolyVertList_Prog);
	PolyFlags=SetBlend(PolyFlags, GouraudPolyVertList_Prog);

	DrawGouraudBufferListData.Texture = Info.Texture;
	DrawGouraudBufferListData.PolyFlags = PolyFlags;
	DrawGouraudBufferListData.TexUMult = TexInfo[0].UMult;
	DrawGouraudBufferListData.TexVMult = TexInfo[0].VMult;

	FTextureInfo DetailTextureInfo;
	DrawGouraudBufferListData.bDetailTex = false;
	if (Info.Texture && Info.Texture->DetailTexture)
	{
		Info.Texture->DetailTexture->Lock(DetailTextureInfo, Frame->Viewport->CurrentTime, -1, this);
		DrawGouraudBufferListData.DetailTexture = Info.Texture->DetailTexture;
		DrawGouraudBufferListData.bDetailTex = true;
		SetTexture(1, DetailTextureInfo, DrawGouraudBufferListData.DetailTexture->PolyFlags, 0.0, GouraudPolyVertList_Prog);

		DrawGouraudBufferListData.DetailTexUMult = TexInfo[1].UMult;
		DrawGouraudBufferListData.DetailTexVMult = TexInfo[1].VMult;
	}

	// Editor Support.
	FPlane DrawColor = FPlane(0.f, 0.f, 0.f, 0.f);

	CHECK_GL_ERROR();
	//debugf(TEXT("PolyList: VertSize %i TexSize %i, ColorSize %i"), VertSize, TexSize, ColorSize); // VertSize 12288 TexSize 8192, ColorSize 16384

	clock(Stats.GouraudPolyListCycles);
	INT i = 0;
	if (UseMeshBuffering) //unused. Testing only.
	{
		SetBlend(PolyFlags, GouraudPolyVert_Prog);
		SetTexture(0, Info, PolyFlags, 0, GouraudPolyVert_Prog);
		CHECK_GL_ERROR();
		for (i = 0; i < NumPts; i++)
		{
			//debugf(TEXT("WorldCoords %s %f %f %f"),Owner->GetName(),Pts[i].Point.TransformPointBy(Frame->Uncoords).X,Pts[i].Point.TransformPointBy(Frame->Uncoords).Y,Pts[i].Point.TransformPointBy(Frame->Uncoords).Z);
			//debugf(TEXT("Relative Coords %s %f %f %f"),Owner->GetName(),Pts[i].Point.X,Pts[i].Point.Y,Pts[i].Point.Z);

			FTransTexture* P = &Pts[i];

			if (!MemData->CacheID)
			{
				//Todo, optimize by using worldcoords without need for TransformPointBy.
				DrawGouraudPolyListVertsBuf[i * 3] = -P->Point.TransformPointBy(Frame->Uncoords).X;
				DrawGouraudPolyListVertsBuf[i * 3 + 1] = -P->Point.TransformPointBy(Frame->Uncoords).Y;
				DrawGouraudPolyListVertsBuf[i * 3 + 2] = -P->Point.TransformPointBy(Frame->Uncoords).Z;
			}

			// Textures
			DrawGouraudPolyListTexBuf[i * 2] = P->U*TexInfo[0].UMult;
			DrawGouraudPolyListTexBuf[i * 2 + 1] = P->V*TexInfo[0].VMult;

			if (PolyFlags & PF_Modulated)
			{
				DrawGouraudPolyListLightColorBuf[i * 4] = 1;
				DrawGouraudPolyListLightColorBuf[i * 4 + 1] = 1;
				DrawGouraudPolyListLightColorBuf[i * 4 + 2] = 1;
				DrawGouraudPolyListLightColorBuf[i * 4 + 3] = 1;
			}
			else
			{
				DrawGouraudPolyListLightColorBuf[i * 4] = P->Light.X;
				DrawGouraudPolyListLightColorBuf[i * 4 + 1] = P->Light.Y;
				DrawGouraudPolyListLightColorBuf[i * 4 + 2] = P->Light.Z;
				DrawGouraudPolyListLightColorBuf[i * 4 + 3] = 1;
			}
		}
		DrawGouraudPolyVertListMeshBuffer(DrawGouraudPolyListVertsBuf, VertSize, DrawGouraudPolyListTexBuf, TexSize, DrawGouraudPolyListLightColorBuf, ColorSize, FPlane(0.f, 0.f, 0.f, 0.f), Owner, PassNum);
		CHECK_GL_ERROR();
	}
	else
	{
		if(!UsePersistentBuffers)
		{
			for (i = 0; i < NumPts; i++)
			{
				FTransTexture* P = &Pts[i];
				BufferGouraudPolygonPoint( &DrawGouraudPolyListSingleBuf[DrawGouraudBufferListData.IndexOffset],P,DrawGouraudBufferListData);
				DrawGouraudBufferListData.IndexOffset += InterleaveCounter;

				if ( DrawGouraudBufferListData.IndexOffset >= DRAWGOURAUDPOLYLIST_SIZE ) //check me. To tired right now.
				{
					GWarn->Logf(TEXT("DrawGouraudPolyList overflow!"));
					break;
				}
			}
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferListData, DrawColor);
		}
		else
		{
			if (DrawGouraudBufferRange[DrawGouraudIndex].Sync)
			{
				WaitBuffer(DrawGouraudBufferRange[DrawGouraudIndex]);
			}
			for (i = 0; i < NumPts; i++)
			{
				FTransTexture* P = &Pts[i];
				BufferGouraudPolygonPoint(&DrawGouraudBufferRange[DrawGouraudIndex].DrawGouraudListPMB[DrawGouraudBufferListData.IndexOffset], P, DrawGouraudBufferListData);
				DrawGouraudBufferListData.IndexOffset += InterleaveCounter;

				if ( DrawGouraudBufferListData.IndexOffset >= DRAWGOURAUDPOLYLIST_SIZE ) //check me. To tired right now.
				{
					GWarn->Logf(TEXT("DrawGouraudPolyList overflow!"));
					break;
				}
			}
			DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferListData, DrawColor);
		}
		CHECK_GL_ERROR();
	}
	unclock(Stats.GouraudPolyListCycles);
	if (DrawGouraudBufferListData.bDetailTex)
		Info.Texture->DetailTexture->Unlock(DetailTextureInfo);
	PassNum++;

	CHECK_GL_ERROR();

	unguard;
}
#endif

void UXOpenGLRenderDevice::DrawGouraudPolyVerts(GLenum Mode, DrawGouraudBuffer& Buffer, FPlane DrawColor)
{
	// Using one huge buffer instead of 3, interleaved data.
	// This is to reduce overhead by reducing API calls.

	GLuint TotalSize = Buffer.VertSize + Buffer.TexSize + Buffer.ColorSize;
	GLuint StrideSize=VertFloatSize+TexFloatSize+ColorFloatSize+ColorFloatSize;
	CHECK_GL_ERROR();

	if (UseBufferInvalidation && !UsePersistentBuffers)
	{
		if(ActiveProgram == GouraudPolyVertList_Prog)
			glInvalidateBufferData(DrawGouraudVertBufferListSingle);
		else if (ActiveProgram == GouraudPolyVert_Prog)
			glInvalidateBufferData(DrawGouraudVertBufferSingle);
		CHECK_GL_ERROR();
	}

	if(!UsePersistentBuffers)
	{
#if ENGINE_VERSION==227
        if (ActiveProgram == GouraudPolyVertList_Prog)
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * TotalSize, DrawGouraudPolyListSingleBuf, GL_STREAM_DRAW);
        else
#endif
        if (ActiveProgram == GouraudPolyVert_Prog)
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * TotalSize, DrawGouraudBuf, GL_STREAM_DRAW);

		// GL > 4.5 for later use maybe.
		/*
		if (ActiveProgram == GouraudPolyVertList_Prog)
			glNamedBufferData(DrawGouraudVertBufferListSingle, sizeof(float) * TotalSize, verts, GL_STREAM_DRAW);
		else if (ActiveProgram == GouraudPolyVert_Prog)
			glNamedBufferData(DrawGouraudVertBufferSingle, sizeof(float) * TotalSize, verts, GL_STREAM_DRAW);
		CHECK_GL_ERROR();
		*/
	}
	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
	glEnableVertexAttribArray(COLOR_ATTRIB);
	glEnableVertexAttribArray(FOGMAP_COORD_ATTRIB);//here for VertexFogColor

	glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, StrideSize, 0);
	glVertexAttribPointer(TEXTURE_COORD_ATTRIB, 2, GL_FLOAT, GL_FALSE, StrideSize, (void*)VertFloatSize);
	glVertexAttribPointer(COLOR_ATTRIB, 4, GL_FLOAT, GL_FALSE, StrideSize, (void*)(VertFloatSize+TexFloatSize));
	glVertexAttribPointer(FOGMAP_COORD_ATTRIB, 4, GL_FLOAT, GL_FALSE, StrideSize, (void*)(VertFloatSize+TexFloatSize+ColorFloatSize));
	CHECK_GL_ERROR();

	// Tex UV's
	glUniform2f(DrawGouraudTexUV, Buffer.TexUMult, Buffer.TexVMult);
	glUniform3f(DrawGouraudDetailTexUV, Buffer.DetailTexUMult, Buffer.DetailTexVMult, (Buffer.bDetailTex ? 1.f : 0.f));

	glUniform1ui(DrawGouraudPolyFlags, Buffer.PolyFlags);
	glUniform1f(DrawGouraudGamma, Gamma);

	//DistanceFog
	glUniform4f(DrawGouraudFogColor, DrawGouraudFogSurface.FogColor.X, DrawGouraudFogSurface.FogColor.Y, DrawGouraudFogSurface.FogColor.Z, DrawGouraudFogSurface.FogColor.W);
	glUniform1f(DrawGouraudFogStart, DrawGouraudFogSurface.FogDistanceStart);
	glUniform1f(DrawGouraudFogEnd, DrawGouraudFogSurface.FogDistanceEnd);
	glUniform1f(DrawGouraudFogDensity, DrawGouraudFogSurface.FogDensity);
	glUniform1i(DrawGouraudFogMode, DrawGouraudFogSurface.FogMode);
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
			if (Buffer.Texture->Alpha > 0.f)
				glUniform4f(DrawGouraudDrawColor, 0.f, 0.f, 0.f, Buffer.Texture->Alpha);
			else glUniform4f(DrawGouraudDrawColor, DrawColor.X, DrawColor.Y, DrawColor.Z, DrawColor.W);
			glUniform1i(DrawGouraudbHitTesting, false);
		}
		CHECK_GL_ERROR();
	}
	// Texture.Alpha support.
	else if (Buffer.Texture->Alpha > 0.f)
		glUniform4f(DrawGouraudDrawColor, 0.f, 0.f, 0.f, Buffer.Texture->Alpha);

	// Draw
	glDrawArrays(Mode, 0, (Buffer.VertSize / FloatsPerVertex));

	if(UsePersistentBuffers)
	{
		LockBuffer(DrawGouraudBufferRange[DrawGouraudIndex]);
		DrawGouraudIndex = (DrawGouraudIndex + 1) % 3;
		CHECK_GL_ERROR();
	}

	// Clean up
	glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
	glDisableVertexAttribArray(COLOR_ATTRIB);
	glDisableVertexAttribArray(FOGMAP_COORD_ATTRIB);
	CHECK_GL_ERROR();

	if (ActiveProgram == GouraudPolyVert_Prog)
	{
		DrawGouraudBufferData.VertSize = 0;
		DrawGouraudBufferData.TexSize = 0;
		DrawGouraudBufferData.ColorSize = 0;
		DrawGouraudBufferData.Iteration = 0;
		DrawGouraudBufferData.IndexOffset = 0;
		DrawGouraudBufferData.Texture = NULL;
		DrawGouraudBufferData.PolyFlags = 0;
	}
	else if(ActiveProgram == GouraudPolyVertList_Prog)
	{
		DrawGouraudBufferListData.VertSize = 0;
		DrawGouraudBufferListData.TexSize = 0;
		DrawGouraudBufferListData.ColorSize = 0;
		DrawGouraudBufferListData.Iteration = 0;
		DrawGouraudBufferListData.IndexOffset = 0;
		DrawGouraudBufferListData.Texture = NULL;
		DrawGouraudBufferListData.PolyFlags = 0;
	}
}

#if ENGINE_VERSION==227
// Experimental code below, just for testing yet. Not in use.
void UXOpenGLRenderDevice::BeginMeshDraw(FSceneNode* Frame, AActor* Owner)
{
	guard(UXOpenGLRenderDevice::BeginMeshDraw);
	if (UseMeshBuffering)
	{
		if (ActiveProgram != GouraudMeshBufferPolyVert_Prog)
		{
			glBindVertexArray(0);
			glUseProgram(0);
			glUseProgram(DrawGouraudMeshBufferProg);
			ActiveProgram = GouraudMeshBufferPolyVert_Prog;
		}

		FMeshVertCacheData* MemData = (FMeshVertCacheData*)Owner->MeshDataPtr;
		if (MemData && MemData->Model != Owner->Mesh)
		{
			//debugf(TEXT("MemData && MemData->Model != Owner->Mesh"));
			delete MemData;
			MemData = NULL;
		}
		if (!MemData)
		{
			MemData = new FMeshVertCacheData(0, Owner->Mesh);
			Owner->MeshDataPtr = MemData;
		}

		FCoords Coords = Frame->Coords;
		cameraPosition = glm::vec3(-Coords.Origin.X, -Coords.Origin.Y, -Coords.Origin.Z);
		cameraDirection = glm::vec3(-Coords.ZAxis.X, -Coords.ZAxis.Y, -Coords.ZAxis.Z);
		cameraTarget = cameraPosition + cameraDirection;
		cameraUp = glm::vec3(Coords.YAxis.X, Coords.YAxis.Y, Coords.YAxis.Z); // upside down crap.

		glm::mat4 CameraMatrix = glm::lookAt(
			cameraPosition,
			cameraTarget,
			cameraUp
			);

		// needed for separate calculation of DrawGouraudPolyVertListMeshBuffer.
		glUniformMatrix4fv(DrawGouraudMeshBufferModelMat, 1, GL_FALSE, glm::value_ptr(modelMat));
		glUniformMatrix4fv(DrawGouraudMeshBufferProjMat, 1, GL_FALSE, glm::value_ptr(projMat));
		glUniformMatrix4fv(DrawGouraudMeshBufferViewMat, 1, GL_FALSE, glm::value_ptr(CameraMatrix));
		CHECK_GL_ERROR();
		PassNum = 0;
	}
	unguard;
}

void UXOpenGLRenderDevice::EndMeshDraw(FSceneNode* Frame, AActor* Owner)
{
	guard(UXOpenGLRenderDevice::EndMeshDraw);
	if (UseMeshBuffering)
	{
		//Set CacheID as buffer flag.
		if (Owner->MeshDataPtr)
		{
			FMeshVertCacheData* MemData = (FMeshVertCacheData*)Owner->MeshDataPtr;
			if (!MemData->CacheID)
				MemData->CacheID = MakeCacheID(CID_StaticMesh, Owner->Mesh);
		}
	}
	unguard;
}

void UXOpenGLRenderDevice::DrawGouraudPolyVertListMeshBuffer(FLOAT* verts, GLuint size, FLOAT* tex, GLuint TexSize, FLOAT* lightcolor, GLuint colorsize, FPlane DrawColor, AActor* Owner, INT Pass)
{
	// Used for DrawGouraudPolyList - instead of pushing poly by poly this is can reduce it to one call for the entire mesh.
	// Keep in mind that it is called for each texture set of one mesh.
	// compared with poly by poly method it seems newer graphics cards can handle this better the newer the card is, disregarding the overall performance of this card.

	FMeshVertCacheData* MemData = (FMeshVertCacheData*)Owner->MeshDataPtr;
	//debugf(TEXT("Mesh %s CacheID: %i"), Owner->GetFullName(),MemData->CacheID);

	if (MemData->CacheID)
	{
		//debugf(TEXT("Buffered Mesh: %s Pass %i"), Owner->GetFullName(),Pass);
		glBindVertexArray(MemData->VAO[Pass]);

		// Very bad for performance, but always need to update Lighting
		glBindBuffer(GL_ARRAY_BUFFER, MemData->ColorBuffer[Pass]);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * colorsize, lightcolor);

		//set DrawColor if any
		if (!DrawColor.IsZero())
			glUniform4f(DrawGouraudMeshBufferDrawColor, DrawColor.X, DrawColor.Y, DrawColor.Z, DrawColor.W);
		CHECK_GL_ERROR();

		glDrawArrays(GL_TRIANGLES, 0, size / FloatsPerVertex);
		glBindVertexArray(0);
		CHECK_GL_ERROR();
	}
	else// if (!MemData->CacheID)
	{
		glGenVertexArrays(1, &MemData->VAO[Pass]);
		glGenBuffers(1, &MemData->VertBuffer[Pass]);
		glGenBuffers(1, &MemData->TexBuffer[Pass]);
		glGenBuffers(1, &MemData->ColorBuffer[Pass]);
		//MeshMap.Set(MemData->CacheID, true);
		//debugf(TEXT("Unbuffered Mesh: %s Pass %i"), Owner->GetFullName(),Pass);
		// Allocate an OpenGL vertex array object.
		// Bind the vertex array object to store all the buffers and vertex attributes we create here.
		glBindVertexArray(MemData->VAO[Pass]);

		glBindBuffer(GL_ARRAY_BUFFER, MemData->VertBuffer[Pass]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, verts, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
		glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FloatsPerVertex, 0);

		// Textures
		glBindBuffer(GL_ARRAY_BUFFER, MemData->TexBuffer[Pass]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * TexSize, tex, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
		glVertexAttribPointer(TEXTURE_COORD_ATTRIB, 2, GL_FLOAT, GL_FALSE, sizeof(float) * TexCoords2D, 0);

		//add light color info
		glBindBuffer(GL_ARRAY_BUFFER, MemData->ColorBuffer[Pass]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * colorsize, lightcolor, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(COLOR_ATTRIB);
		glVertexAttribPointer(COLOR_ATTRIB, 4, GL_FLOAT, GL_FALSE, sizeof(float) * ColorInfo, 0);

		//set DrawColor if any
		if (!DrawColor.IsZero())
			glUniform4f(DrawGouraudDrawColor, DrawColor.X, DrawColor.Y, DrawColor.Z, DrawColor.W);
		CHECK_GL_ERROR();

		glUniform4f(DrawGouraudFogColor,	DrawGouraudFogSurface.FogColor.X, DrawGouraudFogSurface.FogColor.Y, DrawGouraudFogSurface.FogColor.Z, DrawGouraudFogSurface.FogColor.W);
		glUniform1f(DrawGouraudFogStart,	DrawGouraudFogSurface.FogDistanceStart);
		glUniform1f(DrawGouraudFogEnd,		DrawGouraudFogSurface.FogDistanceEnd);
		glUniform1f(DrawGouraudFogDensity,	DrawGouraudFogSurface.FogDensity);
		glUniform1i(DrawGouraudFogMode,		DrawGouraudFogSurface.FogMode);
		CHECK_GL_ERROR();

		glDrawArrays(GL_TRIANGLES, 0, size / FloatsPerVertex);
		CHECK_GL_ERROR();
		glBindVertexArray(0);
		CHECK_GL_ERROR();
	}
}
#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

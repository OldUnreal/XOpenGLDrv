/*=============================================================================
	DrawTile.cpp: Unreal XOpenGL for DrawTile routines.
	Used for f.e. for HUD drawing.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

    unlike DrawComplex and DrawGouraud there is no feasible way to
    properly buffer DrawTile here, reducing the amount of calls to increase
    performance. One problem is that PolyFlags are changed to often and
    although some of these could be maybe ignored (like PF_TwoSided) and set
    in general it still doesn't work out.
    Todo: check which AZDO techniques may apply here.

=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

void UXOpenGLRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UXOpenGLRenderDevice::DrawTile);
	clock(Stats.TileCycles);

	/*
	From SoftDrv DrawTile, no idea if ever of use here:
	- PF_Renderhint indicates nonscaled sprites, but a lot of calls without that flag set
	are also unscaled -> therefore currently we use explicit detection.
	// If PolyFlags & PF_RenderHint, this will be a non-scaled sprite (ie font character, hud icon).
	// 1:1 scaling detector
	if ( EqualPositiveFloat(XL,UL) && EqualPositiveFloat(YL,VL) ) PolyFlags |= PF_RenderHint;

	Set by WrappedPrint and execDrawTextClipped only.
	*/
	SetProgram(Tile_Prog);

	if (PolyFlags & PF_RenderHint)//Using PF_RenderHint internally for CW/CCW switch.
		PolyFlags &= ~PF_RenderHint;

	if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags&PF_Translucent))
		PolyFlags |= PF_Highlighted;

	SetBlend(PolyFlags, Tile_Prog);
	SetTexture(0, Info, PolyFlags, 0, Tile_Prog);

	if (GIsEditor)
	{
		if (Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
			Z = 1.0f; // Probably just needed because projection done below assumes non ortho projection.

		Color = FOpenGLGammaDecompress_sRGB(Color);
	}

	if (PolyFlags & PF_Modulated)
	{
		Color.X = 1.0f;
		Color.Y = 1.0f;
		Color.Z = 1.0f;
		Color.W = 1.0f;
	}
	if (Info.Texture->Alpha > 0.f)
		Color.W = Info.Texture->Alpha;
#if ENGINE_VERSION==227
	else if (!(PolyFlags & PF_AlphaBlend))
		Color.W = 1.0f;
#else
	else Color.W = 1.0f;
#endif

	GLuint VertSize = FloatsPerVertex * 4;
	GLuint TexSize = TexCoords2D * 4;
	GLuint ColorSize = ColorInfo * 4;
	GLuint Size = VertSize + TexSize + ColorSize;

	DrawTilesBufferData.VertSize = VertSize;
	DrawTilesBufferData.TexSize = TexSize;
	DrawTilesBufferData.ColorSize = ColorSize;
	DrawTilesBufferData.TotalSize = Size;
	DrawTilesBufferData.PolyFlags = PolyFlags;
	DrawTilesBufferData.DrawColor = Color;

	if (OpenGLVersion == GL_ES)
	{
		DrawTileVertsBuf[0] = RFX2*Z*(X - Frame->FX2);
		DrawTileVertsBuf[1] = RFY2*Z*(Y - Frame->FY2);
		DrawTileVertsBuf[2] = Z;
		DrawTileVertsBuf[3] = (U)*TexInfo[0].UMult;
		DrawTileVertsBuf[4] = (V)*TexInfo[0].VMult;
		DrawTileVertsBuf[5] = Color.X;
		DrawTileVertsBuf[6] = Color.Y;
		DrawTileVertsBuf[7] = Color.Z;
		DrawTileVertsBuf[8] = Color.W;

		// 1
		DrawTileVertsBuf[9] = RFX2*Z*(X + XL - Frame->FX2);
		DrawTileVertsBuf[10] = RFY2*Z*(Y - Frame->FY2);
		DrawTileVertsBuf[11] = Z;
		DrawTileVertsBuf[12] = (U + UL)*TexInfo[0].UMult;
		DrawTileVertsBuf[13] = (V)*TexInfo[0].VMult;
		DrawTileVertsBuf[14] = Color.X;
		DrawTileVertsBuf[15] = Color.Y;
		DrawTileVertsBuf[16] = Color.Z;
		DrawTileVertsBuf[17] = Color.W;

		// 2
		DrawTileVertsBuf[18] = RFX2*Z*(X + XL - Frame->FX2);
		DrawTileVertsBuf[19] = RFY2*Z*(Y + YL - Frame->FY2);
		DrawTileVertsBuf[20] = Z;
		DrawTileVertsBuf[21] = (U + UL)*TexInfo[0].UMult;
		DrawTileVertsBuf[22] = (V + VL)*TexInfo[0].VMult;
		DrawTileVertsBuf[23] = Color.X;
		DrawTileVertsBuf[24] = Color.Y;
		DrawTileVertsBuf[25] = Color.Z;
		DrawTileVertsBuf[26] = Color.W;

		// 3
		DrawTileVertsBuf[27] = RFX2*Z*(X - Frame->FX2);
		DrawTileVertsBuf[28] = RFY2*Z*(Y + YL - Frame->FY2);
		DrawTileVertsBuf[29] = Z;
		DrawTileVertsBuf[30] = (U)*TexInfo[0].UMult;
		DrawTileVertsBuf[31] = (V + VL)*TexInfo[0].VMult;
		DrawTileVertsBuf[32] = Color.X;
		DrawTileVertsBuf[33] = Color.Y;
		DrawTileVertsBuf[34] = Color.Z;
		DrawTileVertsBuf[35] = Color.W;
	}
	else
	{
		DrawTilesBufferData.TexCoords[0] = glm::vec4(RFX2, RFY2, Frame->FX2, Frame->FY2);
		DrawTilesBufferData.TexCoords[1] = glm::vec4(U, V, UL, VL);
		DrawTilesBufferData.TexCoords[2] = glm::vec4(XL, YL, TexInfo[0].UMult, TexInfo[0].VMult);

		// for more check DrawTile.geo
		DrawTileVertsBuf[0] = X;
		DrawTileVertsBuf[1] = Y;
		DrawTileVertsBuf[2] = Z;
	}

	DrawTileVerts();

	unclock(Stats.TileCycles);
	unguard;
}

void UXOpenGLRenderDevice::DrawTileVerts()
{
	if (OpenGLVersion == GL_ES)
	{
		//debugf(TEXT("%i %i"), DrawTilesBufferData.VertSize, DrawTilesBufferData.PolyFlags);
		// Using one buffer instead of 2, interleaved data to reduce api calls.
		GLuint StrideSize = VertFloatSize + TexFloatSize + ColorFloatSize;

		if (UseBufferInvalidation && !UsePersistentBuffers)
			glInvalidateBufferData(DrawTileVertBuffer);

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DrawTilesBufferData.TotalSize, DrawTileVertsBuf, GL_STREAM_DRAW);
		glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
		glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
		glEnableVertexAttribArray(COLOR_ATTRIB);
		glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, StrideSize, 0);
		glVertexAttribPointer(TEXTURE_COORD_ATTRIB, 2, GL_FLOAT, GL_FALSE, StrideSize, (void*)VertFloatSize);
		glVertexAttribPointer(COLOR_ATTRIB, 4, GL_FLOAT, GL_FALSE, StrideSize, (void*)(VertFloatSize + TexFloatSize));

		glUniform1ui(DrawTilePolyFlags, DrawTilesBufferData.PolyFlags);
		glUniform1f(DrawTileGamma, Gamma);

		if (HitTesting()) // UED selecting support.
			glUniform4f(DrawTileDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
		else glUniform4f(DrawTileDrawColor, 0.f, 0.f, 0.f, 0.f);

		CHECK_GL_ERROR();

		//Draw
		glDrawArrays(GL_TRIANGLE_FAN, 0, DrawTilesBufferData.VertSize / FloatsPerVertex);

		// Clean up
		glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
		glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
		glDisableVertexAttribArray(COLOR_ATTRIB);
		CHECK_GL_ERROR();
	}
	else
	{
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * DrawTilesBufferData.VertSize, DrawTileVertsBuf, GL_STREAM_DRAW);
		glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
		glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, DrawTilesBufferData.VertSize, 0);

		// Coords.
		glUniform4fv(DrawTileTexCoords, 3, (const GLfloat*)DrawTilesBufferData.TexCoords);

		// Color
		if (GIsEditor && HitTesting()) // UED selecting support.
		{
			glUniform4f(DrawTileDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
			glUniform1i(DrawTilebHitTesting, true);
		}
		else
		{
			glUniform4f(DrawTileDrawColor, DrawTilesBufferData.DrawColor.X, DrawTilesBufferData.DrawColor.Y, DrawTilesBufferData.DrawColor.Z, DrawTilesBufferData.DrawColor.W);
			if (GIsEditor)
				glUniform1i(DrawTilebHitTesting, false);
		}

		// Polyflags
		glUniform1ui(DrawTilePolyFlags, DrawTilesBufferData.PolyFlags);

		// Gamma
		glUniform1f(DrawTileGamma, Gamma);
		CHECK_GL_ERROR();

		//Draw
		glDrawArrays(GL_TRIANGLES, 0, DrawTilesBufferData.VertSize);

		// Clean up
		glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
		CHECK_GL_ERROR();
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

/*=============================================================================
	DrawTile.cpp: Unreal XOpenGL for DrawTile routines.
	Used f.e. for HUD drawing.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch
        * Added support for bindless textures
        * removed some blending changes (PF_TwoSided PF_Unlit)
        * Added batching

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

    if(NoDrawTile)
        return;

	CHECK_GL_ERROR();

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

    if (PolyFlags & PF_Unlit)//Of no relevance here.
		PolyFlags &= ~PF_Unlit;

    if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags&PF_Translucent))
		PolyFlags |= PF_Highlighted;

	// stijn: UT draws fonts with PF_Translucent AND PF_Masked set. All of the other rendevs will remove PF_Masked from PF_Translucent tiles, but XOpenGL did not
	//if (PolyFlags & (PF_Translucent|PF_AlphaBlend))
	//	PolyFlags &= ~PF_Masked;


    if (DrawTileBufferData.VertSize > 0 &&
        (DrawTileBufferData.PrevPolyFlags != PolyFlags ||
            (!UsingBindlessTextures && (Info.Texture != DrawTileBufferData.PrevTexture || Info.bRealtimeChanged)))) // stijn: with this check we can enable tile buffering even for GPUs that don't support bindless textures
    {
        // Non 227: to make this work, add missing ComputeRenderSize() in URender::DrawWorld and UGameEngine::Draw for canvas operations.
        DrawTileVerts(DrawTileBufferData);
    }	

	DrawTileBufferData.PrevPolyFlags = PolyFlags;
    DrawTileBufferData.PrevTexture = Info.Texture;

    DrawTileBufferData.PolyFlags = SetFlags(PolyFlags);
	SetBlend(PolyFlags, false);

    SetTexture(0, Info, PolyFlags, 0, Tile_Prog, NORMALTEX);
    //debugf(TEXT("%ls %ls"), GetTextureFormatString(Info.Format), Info.Texture->GetName());

    clockFast(Stats.TileBufferCycles);

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
	if (UsingPersistentBuffersTile && DrawTileRange.Sync[DrawTileBufferData.Index])
		WaitBuffer(DrawTileRange, DrawTileBufferData.Index);

	if (OpenGLVersion == GL_ES)
	{
        // 0
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+0] = RFX2*Z*(X - Frame->FX2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+1] = RFY2*Z*(Y - Frame->FY2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+2] = Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+3] = (U)*TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+4] = (V)*TexInfo[0].VMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+5] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+6] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+7] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+8] = Color.W;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+9] = TexInfo[0].TexNum;

        // 1
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+10] = RFX2*Z*(X + XL - Frame->FX2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+11] = RFY2*Z*(Y - Frame->FY2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+12] = Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+13] = (U + UL)*TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+14] = (V)*TexInfo[0].VMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+15] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+16] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+17] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+18] = Color.W;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+19] = TexInfo[0].TexNum;

        // 2
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+20] = RFX2*Z*(X + XL - Frame->FX2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+21] = RFY2*Z*(Y + YL - Frame->FY2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+22] = Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+23] = (U + UL)*TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+24] = (V + VL)*TexInfo[0].VMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+25] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+26] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+27] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+28] = Color.W;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+29] = TexInfo[0].TexNum;

        // 0
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+30] = RFX2*Z*(X - Frame->FX2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+31] = RFY2*Z*(Y - Frame->FY2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+32] = Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+33] = (U)*TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+34] = (V)*TexInfo[0].VMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+35] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+36] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+37] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+38] = Color.W;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+39] = TexInfo[0].TexNum;

        // 2
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+40] = RFX2*Z*(X + XL - Frame->FX2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+41] = RFY2*Z*(Y + YL - Frame->FY2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+42] = Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+43] = (U + UL)*TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+44] = (V + VL)*TexInfo[0].VMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+45] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+46] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+47] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+48] = Color.W;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+49] = TexInfo[0].TexNum;

        // 3
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+50] = RFX2*Z*(X - Frame->FX2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+51] = RFY2*Z*(Y + YL - Frame->FY2);
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+52] = Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+53] = (U)*TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+54] = (V + VL)*TexInfo[0].VMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+55] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+56] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+57] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+58] = Color.W;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+59] = TexInfo[0].TexNum;

#if ENGINE_VERSION==227
        if (Info.Modifier)
        {
            Info.Modifier->TransformPoint(  DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 3],
                                            DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 4]);

            Info.Modifier->TransformPoint(  DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 13],
                                            DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 14]);

            Info.Modifier->TransformPoint(  DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 23],
                                            DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 24]);

            Info.Modifier->TransformPoint(  DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 33],
                                            DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 34]);

            Info.Modifier->TransformPoint(  DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 43],
                                            DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 44]);

            Info.Modifier->TransformPoint(  DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 53],
                                            DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset + 54]);
        }
#endif

        DrawTileBufferData.VertSize += 18;
        DrawTileBufferData.IndexOffset += 60;
	}
	else
	{
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset] = X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+1] = Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+2] = Z;

        // for more check DrawTile.geo, math moved there.
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+3] = RFX2;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+4] = RFY2;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+5] = Frame->FX2;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+6] = Frame->FY2;

        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+7] = U;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+8] = V;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+9] = UL;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+10] = VL;

        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+11] = XL;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+12] = YL;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+13] = TexInfo[0].UMult;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+14] = TexInfo[0].VMult;

		// DrawColor
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+15] = Color.X;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+16] = Color.Y;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+17] = Color.Z;
        DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+18] = Color.W;

        // TexNum for Bindless
		DrawTileRange.Buffer[DrawTileBufferData.BeginOffset + DrawTileBufferData.IndexOffset+19] = TexInfo[0].TexNum;

        DrawTileBufferData.VertSize    += 9;
        DrawTileBufferData.IndexOffset += 60;
	}
	if ( DrawTileBufferData.IndexOffset >= DRAWTILE_SIZE - 60)
    {
        DrawTileVerts(DrawTileBufferData);
        debugf(NAME_DevGraphics, TEXT("DrawTile overflow!"));
    }
    unclockFast(Stats.TileBufferCycles);
	if (NoBuffering || /*!UsingBindlessTextures ||*/ GIsEditor) // No buffering at this time for Editor.
        DrawTileVerts(DrawTileBufferData);

	unguard;
}

void UXOpenGLRenderDevice::DrawTileVerts(DrawTileBuffer &BufferData)
{
    CHECK_GL_ERROR();
    clockFast(Stats.TileDrawCycles);
    INT DrawMode = GL_TRIANGLES;
	GLintptr BeginOffset = BufferData.BeginOffset * sizeof(float);

    checkSlow(ActiveProgram == Tile_Prog);

    if (OpenGLVersion == GL_ES)
	{
		// Using one buffer instead of 2, interleaved data to reduce api calls.
		if (!UsingPersistentBuffersTile)
        {
            if (UseBufferInvalidation)
                glInvalidateBufferData(DrawTileVertBuffer);
            glBufferSubData(GL_ARRAY_BUFFER, 0, BufferData.IndexOffset * sizeof(float), DrawTileRange.Buffer);
        }

        if (PrevDrawTileBeginOffset != BeginOffset)
        {
            glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (GLvoid*)(BeginOffset));
            glVertexAttribPointer(TEXTURE_COORD_ATTRIB, 2, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (GLvoid*)(BeginOffset + FloatSize3));
            glVertexAttribPointer(COLOR_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (GLvoid*)(BeginOffset + FloatSize3_2));
            glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB, 1, GL_FLOAT, GL_FALSE, DrawTileESStrideSize, (GLvoid*)(BeginOffset + FloatSize3_2_4));
            CHECK_GL_ERROR();
            PrevDrawTileBeginOffset = BeginOffset;
        }
	}
	else
	{
		if (!UsingPersistentBuffersTile)
        {
            if (UseBufferInvalidation)
                glInvalidateBufferData(DrawTileVertBuffer);
            glBufferData(GL_ARRAY_BUFFER, BufferData.IndexOffset * sizeof(float), DrawTileRange.Buffer, GL_STREAM_DRAW);
            CHECK_GL_ERROR();
        }		

        if (PrevDrawTileBeginOffset != BeginOffset)
        {
            glVertexAttribPointer(VERTEX_COORD_ATTRIB, 3, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (GLvoid*)BeginOffset);
            glVertexAttribPointer(TEXTURE_COORD_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (GLvoid*)(BeginOffset + FloatSize3));
            glVertexAttribPointer(TEXTURE_COORD_ATTRIB2, 4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (GLvoid*)(BeginOffset + FloatSize3_4));
            glVertexAttribPointer(TEXTURE_COORD_ATTRIB3, 4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (GLvoid*)(BeginOffset + FloatSize3_4_4));
            glVertexAttribPointer(COLOR_ATTRIB, 4, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (GLvoid*)(BeginOffset + FloatSize3_4_4_4));
            glVertexAttribPointer(BINDLESS_TEXTURE_ATTRIB, 1, GL_FLOAT, GL_FALSE, DrawTileCoreStrideSize, (GLvoid*)(BeginOffset + FloatSize3_4_4_4_4));
            CHECK_GL_ERROR();
            PrevDrawTileBeginOffset = BeginOffset;
        }
	}    

    // Color
    if (GIsEditor && HitTesting()) // UED selecting support.
    {
        glUniform4f(DrawTileHitDrawColor, HitColor.X, HitColor.Y, HitColor.Z, HitColor.W);
        glUniform1i(DrawTilebHitTesting, true);
    }
    else if (GIsEditor)
    {
        glUniform1i(DrawTilebHitTesting, false);
        CHECK_GL_ERROR();
    }

    // PolyFlags
    glUniform1ui(DrawTilePolyFlags, BufferData.PolyFlags);
    CHECK_GL_ERROR();

    // Gamma
    glUniform1f(DrawTileGamma, Gamma);
    CHECK_GL_ERROR();

    // Draw
	glDrawArrays(DrawMode, 0, (BufferData.VertSize / FloatsPerVertex));
    CHECK_GL_ERROR();

	if(UsingPersistentBuffersTile)
	{
		LockBuffer(DrawTileRange, BufferData.Index);
        BufferData.Index = (BufferData.Index + 1) % NUMBUFFERS;
        BufferData.BeginOffset = BufferData.Index * DRAWTILE_SIZE;
	}
	else BufferData.BeginOffset = 0;

	BufferData.VertSize = 0;
	BufferData.TexSize = 0;
	BufferData.ColorSize = 0;
	BufferData.IndexOffset = 0;
	BufferData.FloatSize1 = 0;

	unclockFast(Stats.TileDrawCycles);
    CHECK_GL_ERROR();
}

//
// Program Switching
//
void UXOpenGLRenderDevice::DrawTileEnd(INT NextProgram)
{
    if (DrawTileBufferData.VertSize > 0)
        DrawTileVerts(DrawTileBufferData);

    CHECK_GL_ERROR();

    // Clean up
    glDisableVertexAttribArray(VERTEX_COORD_ATTRIB);
    glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB);
    glDisableVertexAttribArray(COLOR_ATTRIB);
	
    if (OpenGLVersion == GL_Core)
    {
        glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
        glDisableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
    }

    if (UsingBindlessTextures)
        glDisableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);
}

void UXOpenGLRenderDevice::DrawTileStart()
{
#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
    if (UseAA && NoAATiles && PrevProgram != Simple_Prog)
        glDisable(GL_MULTISAMPLE);
#endif

    glUseProgram(DrawTileProg);
    glBindVertexArray(DrawTileVertsVao);
    glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);

	glEnableVertexAttribArray(VERTEX_COORD_ATTRIB);
	glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB);
	glEnableVertexAttribArray(COLOR_ATTRIB);
	
    if (OpenGLVersion == GL_Core)
    {
        glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB2);
        glEnableVertexAttribArray(TEXTURE_COORD_ATTRIB3);
    }
	
	if (UsingBindlessTextures)
		glEnableVertexAttribArray(BINDLESS_TEXTURE_ATTRIB);


    PrevDrawTileBeginOffset = -1;

    CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

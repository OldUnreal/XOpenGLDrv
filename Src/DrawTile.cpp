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

/*-----------------------------------------------------------------------------
    Functions
-----------------------------------------------------------------------------*/
UXOpenGLRenderDevice::DrawTileShaderDrawParams* UXOpenGLRenderDevice::DrawTileGetDrawParamsRef()
{
	// TODO: Implement shader draw params support
	return &DrawTileDrawParams;
}

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

	// Calculate new drawcall parameters
#if ENGINE_VERSION==227
    if (PolyFlags & (PF_AlphaBlend | PF_Translucent | PF_Modulated | PF_Highlighted)) // Make sure occlusion is correctly set.
#else
    if (PolyFlags & (PF_Translucent | PF_Modulated))
#endif
        PolyFlags &= ~PF_Occlude;
    else PolyFlags |= PF_Occlude;

	PolyFlags &= ~(PF_RenderHint | PF_Unlit); // Using PF_RenderHint internally for CW/CCW switch.

    DWORD NextPolyFlags = SetPolyFlags(PolyFlags);

	BOOL bHitTesting = GIsEditor && HitTesting();

	// Hack to render HUD on top of everything in 469
#if UNREAL_TOURNAMENT_OLDUNREAL
    UBOOL ShouldDepthTest = ((GUglyHackFlags & HACKFLAGS_PostRender) == 0) || Abs(1.f - Z) > SMALL_NUMBER;
#endif

	// Color
	if (PolyFlags & PF_Modulated)
	{
		Color.X = 1.0f;
		Color.Y = 1.0f;
		Color.Z = 1.0f;
		Color.W = 1.0f;
	}
#if ENGINE_VERSION==227
	else if (!(PolyFlags & PF_AlphaBlend))
		Color.W = 1.0f;
#else
    if (Info.Texture && Info.Texture->Alpha > 0.f)
        Color.W = Info.Texture->Alpha;
	else Color.W = 1.0f;
#endif

	glm::vec4 DrawColor = FPlaneToVec4(Color);

	// How much vertex data are we going to push to the GPU?
    GLuint DrawTileSize = (OpenGLVersion == GL_ES) ? 6 * sizeof(struct DrawTileBufferedVertES) : 3 * sizeof(struct DrawTileBufferedVertCore);

	// Check if GL state will change
	if (DrawTileDrawParams.DrawData[DTDD_DRAW_COLOR] != DrawColor ||
		DrawTileDrawParams.PolyFlags() != NextPolyFlags ||
		DrawTileDrawParams.HitTesting() != bHitTesting ||
        WillBlendStateChange(DrawTileDrawParams.BlendPolyFlags(), PolyFlags) || // orig polyflags here!
        WillTextureStateChange(0, Info, PolyFlags, DrawTileDrawParams.TexNum()) ||
        // Check if we have space to batch more data
        DrawTileBufferData.IndexOffset * sizeof(GLfloat) >= DRAWTILE_SIZE * sizeof(GLfloat) - DrawTileSize
#if UNREAL_TOURNAMENT_OLDUNREAL
        // Check if the depth testing mode will change
        || ShouldDepthTest != DrawTileDrawParams.DepthTested()
#endif
        )
	{
	    // Flush batched data
        if (DrawTileBufferData.IndexOffset > 0)
        {
            DrawTileVerts();
            WaitBuffer(DrawTileRange, DrawTileBufferData.Index);
        }

		// Set new GL state
        DrawTileDrawParams.BlendPolyFlags() = PolyFlags;
        SetBlend(PolyFlags, false); // yes, we use the original polyflags here!

#if UNREAL_TOURNAMENT_OLDUNREAL
		if (ShouldDepthTest)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	
		DrawTileDrawParams.DepthTested() = ShouldDepthTest;
#endif
    }

	// Bind texture or fetch its bindless handle
    SetTexture(0, Info, PolyFlags, 0, DF_DiffuseTexture);

    // Buffer new drawcall parameters
	DrawTileDrawParams.DrawData[DTDD_HIT_COLOR] = FPlaneToVec4(HitColor);
	DrawTileDrawParams.DrawData[DTDD_DRAW_COLOR] = DrawColor;
	DrawTileDrawParams.HitTesting() = bHitTesting;
	DrawTileDrawParams.PolyFlags() = NextPolyFlags;
	DrawTileDrawParams.Gamma() = Gamma;
	DrawTileDrawParams.TexNum() = TexInfo[0].TexNum;

	if (GIsEditor && 
		Frame->Viewport->Actor && 
		(Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.0f; // Probably just needed because projection done below assumes non ortho projection.
	}

    clockFast(Stats.TileBufferCycles);

	// Buffer the tile
	if (OpenGLVersion == GL_ES)
	{
		struct DrawTileBufferedVertES* Out =
			reinterpret_cast<struct DrawTileBufferedVertES*>(
				&DrawTileRange.Buffer[
					DrawTileBufferData.BeginOffset +
					DrawTileBufferData.IndexOffset]);

        // 0
		Out[0].Point = glm::vec3(RFX2*Z*(X - Frame->FX2), RFY2*Z*(Y - Frame->FY2), Z);
		Out[0].UV    = glm::vec2((U)*TexInfo[0].UMult, (V)*TexInfo[0].VMult);

        // 1
		Out[1].Point = glm::vec3(RFX2*Z*(X + XL - Frame->FX2), RFY2*Z*(Y - Frame->FY2), Z);
		Out[1].UV    = glm::vec2((U + UL)*TexInfo[0].UMult, (V)*TexInfo[0].VMult);

        // 2
		Out[2].Point = glm::vec3(RFX2*Z*(X + XL - Frame->FX2), RFY2*Z*(Y + YL - Frame->FY2), Z);
		Out[2].UV    = glm::vec2((U + UL)*TexInfo[0].UMult, (V + VL)*TexInfo[0].VMult);

        // 0
		Out[3].Point = glm::vec3(RFX2*Z*(X - Frame->FX2), RFY2*Z*(Y - Frame->FY2), Z);
		Out[3].UV    = glm::vec2((U)*TexInfo[0].UMult, (V)*TexInfo[0].VMult);

        // 2
		Out[4].Point = glm::vec3(RFX2*Z*(X + XL - Frame->FX2), RFY2*Z*(Y + YL - Frame->FY2), Z);
		Out[4].UV    = glm::vec2((U + UL)*TexInfo[0].UMult, (V + VL)*TexInfo[0].VMult);

        // 3
		Out[5].Point = glm::vec3(RFX2*Z*(X - Frame->FX2), RFY2*Z*(Y + YL - Frame->FY2), Z);
		Out[5].UV    = glm::vec2((U)*TexInfo[0].UMult, (V + VL)*TexInfo[0].VMult);

#if ENGINE_VERSION==227
        if (Info.Modifier)
        {
			for (INT i = 0; i < 6; ++i)
				Info.Modifier->TransformPoint(Out[i].UV.x, Out[i].UV.y);
		}
#endif
	}
	else
	{
		struct DrawTileBufferedVertCore* Out =
			reinterpret_cast<struct DrawTileBufferedVertCore*>(
				&DrawTileRange.Buffer[
					DrawTileBufferData.BeginOffset +
					DrawTileBufferData.IndexOffset]);

		Out->Point = glm::vec3(X, Y, Z);
		Out->TexCoords0 = glm::vec4(RFX2, RFY2, Frame->FX2, Frame->FY2);
		Out->TexCoords1 = glm::vec4(U, V, UL, VL);
		Out->TexCoords2 = glm::vec4(XL, YL, TexInfo[0].UMult, TexInfo[0].VMult);

#if ENGINE_VERSION==227 && 0 // TODO - Need to push 2x3 matrix transformation to UV mapping.
        if (Info.Modifier)
        {
        }
#endif
	}
	DrawTileBufferData.IndexOffset += DrawTileSize / sizeof(FLOAT);

    unclockFast(Stats.TileBufferCycles);

    if (NoBuffering) // No buffering at this time for Editor.
    {
        DrawTileVerts();
        WaitBuffer(DrawTileRange, DrawTileBufferData.Index);
    }

	unguard;
}

void UXOpenGLRenderDevice::DrawTileVerts()
{
    CHECK_GL_ERROR();
    clockFast(Stats.TileDrawCycles);
    INT DrawMode = GL_TRIANGLES;
	GLintptr BeginOffset = DrawTileBufferData.BeginOffset * sizeof(float);

    checkSlow(ActiveProgram == Tile_Prog);

    if (OpenGLVersion == GL_ES)
	{
		// Using one buffer instead of 2, interleaved data to reduce api calls.
		if (!UsingPersistentBuffersTile)
        {
            if (UseBufferInvalidation)
                glInvalidateBufferData(DrawTileVertBuffer);
            glBufferSubData(GL_ARRAY_BUFFER, 0, DrawTileBufferData.IndexOffset * sizeof(float), DrawTileRange.Buffer);
        }

        if (PrevDrawTileBeginOffset != BeginOffset)
        {
			using Vert = DrawTileBufferedVertES;
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(BeginOffset));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(BeginOffset + offsetof(Vert, UV)));
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
#if __LINUX_ARM__ || MACOSX
	        // stijn: we get a 10x perf increase on the pi if we just replace the entire buffer...
	        glBufferData(GL_ARRAY_BUFFER, DrawTileBufferData.IndexOffset * sizeof(float), DrawTileRange.Buffer, GL_DYNAMIC_DRAW);
#else
			glBufferSubData(GL_ARRAY_BUFFER, 0, DrawTileBufferData.IndexOffset * sizeof(float), DrawTileRange.Buffer);
#endif
            CHECK_GL_ERROR();
        }

        if (PrevDrawTileBeginOffset != BeginOffset)
        {
			using Vert = DrawTileBufferedVertCore;
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)BeginOffset);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(BeginOffset + offsetof(Vert, TexCoords0)));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(BeginOffset + offsetof(Vert, TexCoords1)));
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(BeginOffset + offsetof(Vert, TexCoords2)));

            CHECK_GL_ERROR();
            PrevDrawTileBeginOffset = BeginOffset;
        }
	}

	// Push drawcall parameters
	if (0 && UsingShaderDrawParameters)
	{
		// TODO: Implement
	}
	else
	{
		if (GIsEditor)
		{
			glUniform1i(DrawTilebHitTesting, DrawTileDrawParams.HitTesting());
			if (DrawTileDrawParams.HitTesting())
			{
				glm::vec4& Color = DrawTileDrawParams.DrawData[DTDD_HIT_COLOR];
				glUniform4f(DrawTileHitDrawColor, Color.x, Color.y, Color.z, Color.w);
			}
		}

		// PolyFlags
		glUniform1ui(DrawTilePolyFlags, DrawTileDrawParams.PolyFlags());
		CHECK_GL_ERROR();

		// Gamma
		glUniform1f(DrawTileGamma, DrawTileDrawParams.Gamma());
		CHECK_GL_ERROR();

		// Texture handle
		glUniform1ui(DrawTileTexNum, DrawTileDrawParams.TexNum());
		CHECK_GL_ERROR();

		// DrawColor
		glm::vec4& Color = DrawTileDrawParams.DrawData[DTDD_DRAW_COLOR];
		glUniform4f(DrawTileDrawColor, Color.x, Color.y, Color.z, Color.w);
	}

    // Draw
	GLuint VertexSizeInFloats = (OpenGLVersion == GL_ES) ? sizeof(struct DrawTileBufferedVertES) / sizeof(FLOAT) : sizeof(struct DrawTileBufferedVertCore) / sizeof(FLOAT);

	glDrawArrays(DrawMode, 0, DrawTileBufferData.IndexOffset / VertexSizeInFloats);
    CHECK_GL_ERROR();

	if(UsingPersistentBuffersTile)
	{
		LockBuffer(DrawTileRange, DrawTileBufferData.Index);
        DrawTileBufferData.Index = (DrawTileBufferData.Index + 1) % NUMBUFFERS;
        DrawTileBufferData.BeginOffset = DrawTileBufferData.Index * DRAWTILE_SIZE;
	}
	else DrawTileBufferData.BeginOffset = 0;

	DrawTileBufferData.IndexOffset = 0;

	unclockFast(Stats.TileDrawCycles);
    CHECK_GL_ERROR();
}

//
// Program Switching
//
void UXOpenGLRenderDevice::DrawTileEnd(INT NextProgram)
{
    if (DrawTileBufferData.IndexOffset > 0)
    {
        DrawTileVerts();
        WaitBuffer(DrawTileRange, DrawTileBufferData.Index);
    }

    CHECK_GL_ERROR();

	for (INT i = 0; i < (OpenGLVersion == GL_ES ? 2 : 4); ++i)
		glDisableVertexAttribArray(i);

#if UNREAL_TOURNAMENT_OLDUNREAL
    glEnable(GL_DEPTH_TEST);
#endif
}

void UXOpenGLRenderDevice::DrawTileStart()
{
    WaitBuffer(DrawTileRange, DrawTileBufferData.Index);

#if !defined(__EMSCRIPTEN__) && !__LINUX_ARM__
    if (UseAA && NoAATiles && PrevProgram != Simple_Prog)
        glDisable(GL_MULTISAMPLE);
#endif

#if UNREAL_TOURNAMENT_OLDUNREAL
    if (GUglyHackFlags & HACKFLAGS_PostRender) // ugly hack to make the HUD always render on top of weapons
        glDisable(GL_DEPTH_TEST);
#endif

    glUseProgram(DrawTileProg);
    glBindVertexArray(DrawTileVertsVao);
    glBindBuffer(GL_ARRAY_BUFFER, DrawTileVertBuffer);

	for (INT i = 0; i < (OpenGLVersion == GL_ES ? 2 : 4); ++i)
		glEnableVertexAttribArray(i);

    DrawTileDrawParams.BlendPolyFlags() = CurrentPolyFlags | CurrentAdditionalPolyFlags;
    DrawTileDrawParams.PolyFlags() = 0;
    PrevDrawTileBeginOffset = -1;

    CHECK_GL_ERROR();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

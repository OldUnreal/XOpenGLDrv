/*=============================================================================
	DrawGouraud.cpp: Unreal XOpenGL DrawGouraud routines.
	Used for drawing meshes.

	VertLists are only supported by 227 so far, it pushes verts in a huge
	list instead of vertice by vertice. Currently this method improves
	performance 10x and more compared to unbuffered calls. Buffering
	catches up quite some.
	Copyright 2014-2021 Oldunreal

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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	Helpers
-----------------------------------------------------------------------------*/

// Packs a normalized vector into GL_INT_2_10_10_10_REV (10 bits X, 10 bits Y, 10 bits Z, 2 bits W).
// Divides by 511 (2^(10-1)-1), matching the GL spec's signed-normalized decode formula for a
// 10-bit component. W bits are left 0 -- decodes to 0.0, unused by every consumer of this attribute.
static FORCEINLINE glm::int32 PackNormal_1010102(const FVector& N)
{
	auto Q = [](FLOAT V) -> glm::int32 { return static_cast<glm::int32>(Clamp(V, -1.f, 1.f) * 511.f) & 0x3FF; };
	return Q(N.X) | (Q(N.Y) << 10) | (Q(N.Z) << 20);
}

static void BufferVert(UXOpenGLRenderDevice::DrawGouraudVertex* Vert, UXOpenGLRenderDevice::DrawGouraudNormal* NormOut,
                        FTransTexture* P, glm::uint DrawID, bool bWriteNormal)
{
	Vert->Coords		= glm::vec3(P->Point.X, P->Point.Y, P->Point.Z);
	Vert->DrawID		= DrawID;
	Vert->TexCoords     = glm::vec2(P->U, P->V);
	Vert->LightColor	= glm::vec4(P->Light.X, P->Light.Y, P->Light.Z, P->Light.W);
	Vert->FogColor		= glm::vec4(P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W);
	if (bWriteNormal)
		NormOut->PackedNormal = PackNormal_1010102(P->Normal);
}

static void SetTextureHelper
(
	UXOpenGLRenderDevice* RenDev, 
	INT Multi, 
	UTexture* Texture, 
	FSceneNode* Frame, 
	FTEXTURE_PTR& CachedInfo,
	glm::uint64* TexHandles,
	DWORD& DrawFlags,
	DWORD AddDrawFlag
)
{
#if XOPENGL_MODIFIED_LOCK
	CachedInfo = Texture->GetTexture(INDEX_NONE, RenDev);
#else
	Texture->Lock(CachedInfo, Frame->Viewport->CurrentTime, -1, RenDev);
#endif

	RenDev->SetTexture(Multi, FTEXTURE_GET(CachedInfo), Texture->PolyFlags, 0.f);
	TexHandles[Multi] = RenDev->TexInfo[Multi].BindlessTexHandle;
	DrawFlags |= AddDrawFlag;
}

DWORD UXOpenGLRenderDevice::PrepareGouraudCall(FSceneNode* Frame, FTextureInfo& Info, DWORD PolyFlags)
{
	// Shaders[Gouraud_Prog] is always constructed as exactly DrawGouraudProgram (ShaderProgram.cpp,
	// InitShaders) -- static_cast is safe and avoids a per-draw RTTI lookup.
	auto Shader = static_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);

	// Gather options
	DWORD DrawFlags = ShaderDrawFlags::DF_None;
	DWORD NextPolyFlags = GetPolyFlagsAndDrawFlags(PolyFlags, DrawFlags, FALSE);
	UBOOL NoNearZ = (GUglyHackFlags & HACKFLAGS_NoNearZ) == HACKFLAGS_NoNearZ;
	if (GIsEditor && NextPolyFlags & PF_Selected)
		DrawFlags |= ShaderDrawFlags::DF_Selected;

	// Figure out which texture layers this mesh uses, so we can select (or lazily build) the shader
	// specialization that only contains straight-line code for those layers. See
	// ShaderProgram::GetOrBuildSpecialization.
#if ENGINE_VERSION==227
	const bool HasBumpMapPtr = Info.Texture && Info.Texture->BumpMap;
	const bool HasBumpMap = HasBumpMapPtr && BumpMaps;
#else
	const bool HasBumpMapPtr = false;
	const bool HasBumpMap = false;
#endif
	const DWORD PerDrawOptionsMask =
		ShaderCompilationOptions::OPT_HasDetailTexture |
		ShaderCompilationOptions::OPT_HasMacroTexture |
		ShaderCompilationOptions::OPT_HasBumpMap |
		ShaderCompilationOptions::OPT_IsMasked | ShaderCompilationOptions::OPT_IsAlphaBlended |
		ShaderCompilationOptions::OPT_IsModulated | ShaderCompilationOptions::OPT_IsRenderFog |
		ShaderCompilationOptions::OPT_IsTranslucent | ShaderCompilationOptions::OPT_IsUnlit;

	const DWORD PerDrawSignature =
		(DrawFlags & (ShaderDrawFlags::DF_Masked | ShaderDrawFlags::DF_AlphaBlended | ShaderDrawFlags::DF_Modulated | ShaderDrawFlags::DF_RenderFog | ShaderDrawFlags::DF_Translucent | ShaderDrawFlags::DF_Unlit)) |
		((Info.Texture && Info.Texture->DetailTexture) ? ShaderCompilationOptions::OPT_HasDetailTexture : 0) |
		((Info.Texture && Info.Texture->MacroTexture) ? ShaderCompilationOptions::OPT_HasMacroTexture : 0) |
		(HasBumpMapPtr ? ShaderCompilationOptions::OPT_HasBumpMap : 0);

	ShaderCompilationOptions RendererConfigOptions = Shader->CurrentSpecialization->Options;
	RendererConfigOptions.UnsetOption(PerDrawOptionsMask);

	ShaderCompilationOptions RequiredOptions;
	if (PerDrawSignature == Shader->LastPerDrawSignature && RendererConfigOptions == Shader->LastRendererConfigOptions)
	{
		// Nothing that matters has changed since the last draw call -- reuse what we computed then.
		RequiredOptions = Shader->LastResolvedOptions;
	}
	else
	{
		RequiredOptions = RendererConfigOptions; // already has the per-draw bits cleared
		if (Info.Texture && Info.Texture->DetailTexture && DetailTextures)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_HasDetailTexture);
		if (Info.Texture && Info.Texture->MacroTexture && MacroTextures)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_HasMacroTexture);
		if (HasBumpMap)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_HasBumpMap);
		if (DrawFlags & ShaderDrawFlags::DF_Masked)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_IsMasked);
		if (DrawFlags & ShaderDrawFlags::DF_AlphaBlended)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_IsAlphaBlended);
		if (DrawFlags & ShaderDrawFlags::DF_Modulated)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_IsModulated);
		if (DrawFlags & ShaderDrawFlags::DF_RenderFog)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_IsRenderFog);
		if (DrawFlags & ShaderDrawFlags::DF_Translucent)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_IsTranslucent);
		if (DrawFlags & ShaderDrawFlags::DF_Unlit)
			RequiredOptions.SetOption(ShaderCompilationOptions::OPT_IsUnlit);

		Shader->LastPerDrawSignature = PerDrawSignature;
		Shader->LastRendererConfigOptions = RendererConfigOptions;
		Shader->LastResolvedOptions = RequiredOptions;
	}

	const bool NonOpaque = !(NextPolyFlags & PF_Occlude);

	// For non-opaque draws, request a shared "uber" specialization instead of this mesh's exact
	// texture-layer combination -- see the OPT_RuntimeTextureLayers comment in XOpenGL.h and the
	// matching logic in DrawComplexSurface. Opaque draws are unaffected and keep requesting their
	// exact specialization as before.
	ShaderCompilationOptions SpecializationToRequest = RequiredOptions;
	if (NonOpaque && UsingBindlessTextures)
	{
		constexpr DWORD Tier2Mask = ShaderCompilationOptions::OPT_HasBumpMap;
		const bool NeedsTier2 = RequiredOptions.HasOption(Tier2Mask);
		SpecializationToRequest.SetOption(ShaderCompilationOptions::OPT_HasDetailTexture | ShaderCompilationOptions::OPT_HasMacroTexture);
		if (NeedsTier2)
			SpecializationToRequest.SetOption(Tier2Mask);
		else
			SpecializationToRequest.UnsetOption(Tier2Mask);
		// Masked vs alpha-blended discard is also collapsed to a runtime DrawFlags check (see
		// ApplyPolyFlags and the Color.a modulation sites in DrawGouraud_GLSL.cpp) -- clear both so
		// draws that only differ by these bits still land in the same specialization/batch.
		SpecializationToRequest.UnsetOption(ShaderCompilationOptions::OPT_IsMasked | ShaderCompilationOptions::OPT_IsAlphaBlended);
		SpecializationToRequest.SetOption(ShaderCompilationOptions::OPT_RuntimeTextureLayers);
	}

	// Resolve (lazily compiling if needed) the specialization this mesh needs, and select its
	// pending batch -- WITHOUT switching the GL program away from whatever's actually bound right
	// now. Gouraud_Prog no longer flushes just because a different specialization is needed: draws
	// for many specializations can be buffered at once (see MultiSpecializationShaderProgramImpl)
	// and only get drawn when something else requires it.
	CompiledShader* Specialization = Shader->GetOrBuildSpecialization(SpecializationToRequest);
	auto* Batch = Shader->SelectBatch(Specialization);

	const bool NeedRotate = !Shader->ParametersBuffer.CanBuffer(1); // shared buffer genuinely exhausted
	const bool CanBuffer = !Batch->DrawBuffer.IsFull() && !NeedRotate;
	const bool BlendWillChange = WillBlendStateChange(CurrentBlendPolyFlags, NextPolyFlags);
	const bool NeedsProjectionChange = NoNearZ &&
		(StoredFovAngle != Frame->Viewport->Actor->FovAngle ||
			StoredFX != Frame->FX ||
			StoredFY != Frame->FY ||
			!StoredbNearZ);

	// Some OTHER batch already has pending content: this mesh being non-opaque means it needs
	// everything submitted before it fully drawn first -- opaque draws don't care about being
	// reordered relative to each other, but a translucent draw's own depth test needs any earlier
	// opaque geometry already depth-written, and two pending non-opaque batches for different
	// specializations could have their relative blending order scrambled by Flush()'s pool-slot
	// iteration. Scoped to NonOpaque only: consecutive non-opaque meshes sharing a specialization,
	// with nothing else interleaved, still batch together normally, since they land in the same
	// batch in submission order either way.
	const bool NeedOwnFlushForOrdering = NonOpaque && Shader->HasOtherPendingBatch(Batch);
	const bool OwnFlushNeeded = BlendWillChange || StoredbNearZ != NoNearZ || NeedOwnFlushForOrdering || !CanBuffer;

	// Flush OUR OWN pending batches: blend change, near-Z mode toggling, a differently-specialized
	// non-opaque batch already pending, or we're out of room (this batch's own draw-call list is
	// full, or the shared buffer itself is). Specialization changes alone no longer trigger this.
	// The texture itself no longer needs to be pre-checked here -- BindTextureAndSampler flushes
	// lazily, exactly when it actually needs to rebind, instead of us predicting it upfront.
	if (OwnFlushNeeded)
	{
		Shader->Flush(NeedRotate); // only rotate if the shared buffer specifically needs it
		Batch = Shader->SelectBatch(Specialization); // re-acquire: Flush() cleared every batch
	}

	// Independently of whether we just flushed ourselves: Complex_Prog batches on its own schedule
	// and isn't drained just because we're drawing here (see SetProgram). If the blend state is
	// changing, the projection is about to change, or this mesh itself is non-opaque, any pending
	// Complex draws need to be on-screen before we touch shared blend/projection state or lose
	// track of draw order relative to them -- this must run before SetBlend/SetProjection below
	// (under the OLD state) and unconditionally, not nested inside the block above, or a
	// translucent mesh whose only difference from the previous draw is specialization would skip it.
	if (BlendWillChange || NeedsProjectionChange || NonOpaque)
		FlushInactiveBatchedProgram(Complex_Prog);

	// Update global GL state, now that anything that needed the OLD state is on-screen. Gated on
	// OwnFlushNeeded to match the original nesting (blend/near-Z toggle/capacity only -- a pure
	// specialization change was never a reason to touch projection either).
	if (OwnFlushNeeded && BlendWillChange)
		SetBlend(NextPolyFlags);

	if (OwnFlushNeeded && NeedsProjectionChange)
	{
		SetProjection(Frame, 1); // TODO/FIXME: Shouldn't this second argument be !NoNearZ ?
	}

	DrawGouraudParameters LocalParams{};
	DrawGouraudParameters* DrawCallParams = &LocalParams;

	const FLOAT TextureAlpha =
#if ENGINE_VERSION==227
		1.f;
#else
		(Info.Texture && Info.Texture->Alpha > 0.f) ? Info.Texture->Alpha : 1.f;
#endif
	
	DrawCallParams->DrawColor = HitTesting() ? FPlaneToVec4(HitColor) : glm::vec4(0.f, 0.f, 0.f, TextureAlpha);

	SetTexture(DiffuseTextureIndex, Info, NextPolyFlags, 0.0);
	DrawCallParams->DiffuseInfo = glm::vec4(TexInfo[DiffuseTextureIndex].UMult, TexInfo[DiffuseTextureIndex].VMult, Info.Texture ? Info.Texture->Diffuse : 1.f, TextureAlpha);
	DrawCallParams->TexHandles[DiffuseTextureIndex] = TexInfo[DiffuseTextureIndex].BindlessTexHandle;
	DrawFlags |= ShaderDrawFlags::DF_DiffuseTexture;

	DrawCallParams->DetailMacroInfo = glm::vec4(0.f, 0.f, 0.f, 0.f);
	if (Info.Texture && Info.Texture->DetailTexture && DetailTextures)
	{
		SetTextureHelper(this, DetailTextureIndex, Info.Texture->DetailTexture, Frame, Shader->DetailTextureInfo, DrawCallParams->TexHandles, DrawFlags, ShaderDrawFlags::DF_DetailTexture);
		DrawCallParams->DetailMacroInfo.x = TexInfo[DetailTextureIndex].UMult;
		DrawCallParams->DetailMacroInfo.y = TexInfo[DetailTextureIndex].VMult;
	}

#if ENGINE_VERSION==227
	if (Info.Texture && Info.Texture->BumpMap && BumpMaps)
	{
		SetTextureHelper(this, BumpMapIndex, Info.Texture->BumpMap, Frame, Shader->BumpMapInfo, DrawCallParams->TexHandles, DrawFlags, ShaderDrawFlags::DF_BumpMap);
	}
#endif

	if (Info.Texture && Info.Texture->MacroTexture && MacroTextures)
	{
		SetTextureHelper(this, MacroTextureIndex, Info.Texture->MacroTexture, Frame, Shader->MacroTextureInfo, DrawCallParams->TexHandles, DrawFlags, ShaderDrawFlags::DF_MacroTexture);
		DrawCallParams->DetailMacroInfo.z = TexInfo[MacroTextureIndex].UMult;
		DrawCallParams->DetailMacroInfo.w = TexInfo[MacroTextureIndex].VMult;
	}

	DrawCallParams->DrawFlags = DrawFlags;

	// Every texture layer is bound now, and any flush that setting them up could possibly have
	// triggered has already happened -- safe to fetch the real ring-buffer slot and commit our staged
	// parameters into it in one shot.
	*Shader->ParametersBuffer.GetCurrentElementPtr() = LocalParams;

	// Re-acquire our batch: a lazy flush from BindTextureAndSampler above (triggered by SetTexture/
	// SetTextureHelper needing to rebind or re-upload a texture) would have cleared every pending
	// batch, including the one we picked earlier -- this is a cheap cache-hit lookup when that
	// didn't happen, and correctly re-claims a fresh slot when it did. Our callers read
	// Shader->ActiveBatch (set by SelectBatch) rather than a return value from this function.
	Shader->SelectBatch(Specialization);

	return DrawFlags;
}

void UXOpenGLRenderDevice::FinishGouraudCall(FTextureInfo& Info, DWORD DrawFlags)
{
#if !XOPENGL_MODIFIED_LOCK
	// Shaders[Gouraud_Prog] is always constructed as exactly DrawGouraudProgram (ShaderProgram.cpp,
	// InitShaders) -- static_cast is safe and avoids a per-draw RTTI lookup.
	auto Shader = static_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);
	if (DrawFlags & ShaderDrawFlags::DF_DetailTexture)
		Info.Texture->DetailTexture->Unlock(Shader->DetailTextureInfo);

	if (DrawFlags & ShaderDrawFlags::DF_BumpMap)
		Info.Texture->BumpMap->Unlock(Shader->BumpMapInfo);

	if (DrawFlags & ShaderDrawFlags::DF_MacroTexture)
		Info.Texture->MacroTexture->Unlock(Shader->MacroTextureInfo);
#endif
}

/*-----------------------------------------------------------------------------
	RenDev Interface
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolygon);

	if (NoDrawGouraud)
		return;

	// Shaders[Gouraud_Prog] is always constructed as exactly DrawGouraudProgram (ShaderProgram.cpp,
	// InitShaders) -- static_cast is safe and avoids a per-draw RTTI lookup.
	auto Shader = static_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);

    STAT(clockFast(Stats.GouraudPolyCycles));
	SetProgram(Gouraud_Prog);

	if (NumPts < 3 /*|| Frame->Recursion > MAX_FRAME_RECURSION*/) //reject invalid.
		return;

	auto InVertexCount = NumPts - 2;
	auto OutVertexCount = InVertexCount * 3;

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i]->U, Pts[i]->V, UM, VM);
	}
#endif

	if (!Shader->VertBuffer.CanBuffer(OutVertexCount)) // we check the available capacity of the parameters and draw buffer elsewhere
	{
		Shader->Flush(true);

		// just in case...
		if (OutVertexCount >= Shader->VertexBufferSize)
		{
			GWarn->Logf(TEXT("DrawGouraudPolygon poly too big!"));
			return;
		}
	}

	DWORD DrawFlags = PrepareGouraudCall(Frame, Info, PolyFlags);

	// PrepareGouraudCall resolved and stashed the batch for this mesh's specialization in
	// Shader->ActiveBatch rather than returning it directly.
	auto* Batch = Shader->ActiveBatch;
	Batch->DrawBuffer.StartDrawCall(Shader->VertBuffer.CurrentAbsolutePosition());
	auto Out = Shader->VertBuffer.GetCurrentElementPtr();
	auto NormOut = Shader->NormalsBuffer.GetCurrentElementPtr();
	const auto DrawID = Shader->ParametersBuffer.CurrentAbsolutePosition();
	const bool bWriteNormal = Shader->bNeedNormalsThisPass;

	// Unfan and buffer
	for (INT i = 0; i < InVertexCount; i++)
	{
		BufferVert(Out++, NormOut++, Pts[0    ], DrawID, bWriteNormal);
		BufferVert(Out++, NormOut++, Pts[i + 1], DrawID, bWriteNormal);
		BufferVert(Out++, NormOut++, Pts[i + 2], DrawID, bWriteNormal);
	}

	Batch->DrawBuffer.EndDrawCall(OutVertexCount);
	Shader->VertBuffer.Advance(OutVertexCount);
	// Unconditional, regardless of bWriteNormal -- keeps NormalsBuffer's ring position numerically
	// identical to VertBuffer's at all times. See DrawGouraudProgram::OnVertBufferUploaded.
	Shader->NormalsBuffer.Advance(OutVertexCount);
	Shader->ParametersBuffer.Advance(1);

	FinishGouraudCall(Info, DrawFlags);
    STAT(unclockFast(Stats.GouraudPolyCycles));
	unguard;
}

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudPolyList(FSceneNode* Frame, FTextureInfo& Info, FTransTexture* Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudPolyList);

	if (NoDrawGouraudList)
		return;

	// Shaders[Gouraud_Prog] is always constructed as exactly DrawGouraudProgram (ShaderProgram.cpp,
	// InitShaders) -- static_cast is safe and avoids a per-draw RTTI lookup.
	auto Shader = static_cast<DrawGouraudProgram*>(Shaders[Gouraud_Prog]);

    STAT(clockFast(Stats.GouraudPolyCycles));
	SetProgram(Gouraud_Prog);

	if (NumPts < 3 /*|| Frame->Recursion > MAX_FRAME_RECURSION*/) //reject invalid.
		return;

#if ENGINE_VERSION==227
	if (Info.Modifier)
	{
		FLOAT UM = Info.USize, VM = Info.VSize;
		for (INT i = 0; i < NumPts; ++i)
			Info.Modifier->TransformPointUV(Pts[i].U, Pts[i].V, UM, VM);
	}
#endif

	DWORD DrawFlags = PrepareGouraudCall(Frame, Info, PolyFlags);

	// PrepareGouraudCall resolved and stashed the batch for this mesh's specialization in
	// Shader->ActiveBatch rather than returning it directly. Save the specialization itself too,
	// since the mid-loop overflow-split below needs to re-acquire a batch for it after a flush
	// clears Shader->ActiveBatch.
	auto* Batch = Shader->ActiveBatch;
	CompiledShader* Specialization = Batch->Specialization;

	Batch->DrawBuffer.StartDrawCall(Shader->VertBuffer.CurrentAbsolutePosition());
	auto Out = Shader->VertBuffer.GetCurrentElementPtr();
	auto End = Shader->VertBuffer.GetLastElementPtr();
	auto NormOut = Shader->NormalsBuffer.GetCurrentElementPtr();
	auto DrawID = Shader->ParametersBuffer.CurrentAbsolutePosition();
	const bool bWriteNormal = Shader->bNeedNormalsThisPass;

	INT PolyListSize = 0;
	for (INT i = 0; i < NumPts; i++)
	{
		// Polylists can be bigger than the vertex buffer so check here if we
		// need to split the mesh up into separate drawcalls
		if ((i % 3 == 0) && (Out + 2 > End))
		{
			Batch->DrawBuffer.EndDrawCall(PolyListSize);
			Shader->VertBuffer.Advance(PolyListSize);
			// Unconditional, regardless of bWriteNormal -- see DrawGouraudPolygon/OnVertBufferUploaded.
			Shader->NormalsBuffer.Advance(PolyListSize);
			Shader->ParametersBuffer.Advance(1); // advance so Flush automatically restores the drawcall params of the _current_ drawcall

			Shader->Flush(true);
			//debugf(NAME_DevGraphics, TEXT("DrawGouraudPolyList overflow!"));
			Batch = Shader->SelectBatch(Specialization); // re-acquire: Flush() cleared every batch

			Batch->DrawBuffer.StartDrawCall(Shader->VertBuffer.CurrentAbsolutePosition());
			Out = Shader->VertBuffer.GetCurrentElementPtr();
			End = Shader->VertBuffer.GetLastElementPtr();
			NormOut = Shader->NormalsBuffer.GetCurrentElementPtr();
			DrawID = Shader->ParametersBuffer.CurrentAbsolutePosition();

			PolyListSize = 0;
		}

		BufferVert(Out++, NormOut++, &Pts[i], DrawID, bWriteNormal);
		PolyListSize++;
	}

	Batch->DrawBuffer.EndDrawCall(PolyListSize);
	Shader->VertBuffer.Advance(PolyListSize);
	Shader->NormalsBuffer.Advance(PolyListSize);
	Shader->ParametersBuffer.Advance(1);

	FinishGouraudCall(Info, DrawFlags);
    STAT(unclockFast(Stats.GouraudPolyCycles));
	unguard;
}
#endif

// stijn: This is the UT extended renderer interface. This does not map directly onto DrawGouraudPolyList because DrawGouraudTriangles pushes info out earlier
#if UNREAL_TOURNAMENT_OLDUNREAL
void UXOpenGLRenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	guard(UXOpenGLRenderDevice::DrawGouraudTriangles);

	if (NoDrawGouraudList)
		return;

    STAT(clockFast(Stats.GouraudPolyCycles));

	INT StartOffset = 0;
	INT i = 0;

	if (Frame->NearClip.W != 0.0)
		PushClipPlane(Frame->NearClip);

	for (; i < NumPts; i += 3)
	{
		if (Frame->Mirror == -1.0)
			Exchange(Pts[i + 2], Pts[i]);

		// Environment mapping.
		if (PolyFlags & PF_Environment)
		{
			FLOAT UScale = Info.UScale * Info.USize / 256.0f;
			FLOAT VScale = Info.VScale * Info.VSize / 256.0f;

			for (INT j = 0; j < 3; j++)
			{
				FVector T = Pts[i + j].Point.UnsafeNormal().MirrorByVector(Pts[i + j].Normal).TransformVectorBy(Frame->Uncoords);
				Pts[i + j].U = (T.X + 1.0f) * 0.5f * 256.0f * UScale;
				Pts[i + j].V = (T.Y + 1.0f) * 0.5f * 256.0f * VScale;
			}
		}

		// If outcoded, skip it.
		if (Pts[i].Flags & Pts[i + 1].Flags & Pts[i + 2].Flags)
		{
			// stijn: push the triangles we've already processed (if any)
			if (i - StartOffset > 0)
			{
				DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);
				StartOffset = i + 3;
			}
			continue;
		}

		// Backface reject it.
		if ((PolyFlags & PF_TwoSided) && FTriple(Pts[i].Point, Pts[i + 1].Point, Pts[i + 2].Point) <= 0.0)
		{
			if (!(PolyFlags & PF_TwoSided))
			{
				// stijn: push the triangles we've already processed (if any)
				if (i - StartOffset > 0)
				{
					DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);
					StartOffset = i + 3;
				}
				continue;
			}
			Exchange(Pts[i + 2], Pts[i]);
		}
	}

	// stijn: push the remaining triangles
	if (i - StartOffset > 0)
		DrawGouraudPolyList(const_cast<FSceneNode*>(Frame), const_cast<FTextureInfo&>(Info), Pts + StartOffset, i - StartOffset, PolyFlags, nullptr);

	if (Frame->NearClip.W != 0.0)
		PopClipPlane();

    STAT(unclockFast(Stats.GouraudPolyCycles));
	unguard;
}
#endif

#if ENGINE_VERSION==227
void UXOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf)
{
	guard(UXOpenGLRenderDevice::PreDrawGouraud);

	if (FogSurf.IsValid())
		SetDistanceFog(FogSurf);
	else
		ResetDistanceFog();

	unguard;
}

void UXOpenGLRenderDevice::PostDrawGouraud(FSceneNode* Frame, FFogSurf& FogSurf)
{
	guard(UXOpenGLRenderDevice::PostDrawGouraud);
	ResetDistanceFog();
	unguard;
}
#endif // ENGINE_VERSION

/*-----------------------------------------------------------------------------
	Gouraud Mesh Shader
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::DrawGouraudProgram::DrawGouraudProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: MultiSpecializationShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize				= DRAWGOURAUDPOLY_SIZE * 12;
	ParametersBufferSize			= DRAWGOURAUDPOLY_SIZE;
	ParametersBufferBindingIndex	= GlobalShaderBindingIndices::GouraudParametersIndex;
	NumTextureSamplers				= 6;
	DrawMode						= GL_TRIANGLES;
	UseSSBOParametersBuffer			= RenDev->UsingShaderDrawParameters;
	ParametersInfo					= DrawGouraudParametersInfo;
	VertexShaderFunc				= &BuildVertexShader;
	GeoShaderFunc					= RenDev->UsingGeometryShaders ? &BuildGeometryShader : nullptr; // optional
	FragmentShaderFunc				= &BuildFragmentShader;
	RelevantSpecializationOptions =
		ShaderCompilationOptions::OPT_DetailTextures |
		ShaderCompilationOptions::OPT_MacroTextures |
		ShaderCompilationOptions::OPT_BumpMaps |
		ShaderCompilationOptions::OPT_HWLighting |
		ShaderCompilationOptions::OPT_DistanceFog |
		ShaderCompilationOptions::OPT_ClipDistance |
		ShaderCompilationOptions::OPT_Editor |
		ShaderCompilationOptions::OPT_SimulateMultiPass |
		ShaderCompilationOptions::OPT_GeometryShaders;
}

// Builds two VAOs against the same underlying VertBuffer VBO:
//   VAO A (VertBuffer's own, bound by the time this runs -- see MapBuffers) omits attribute 2
//   (Normal) entirely: the common case, used whenever bNeedNormalsThisPass is false.
//   VAO B (NormalsBuffer's own) additionally references NormalsBuffer's VBO for attribute 2, a
//   packed GL_INT_2_10_10_10_REV value that GL unpacks/normalizes into the same vec4 the vertex
//   shader already declares -- no shader-side changes needed for either VAO.
// See DrawGouraudProgram::OnVertBufferBound for where the two VAOs actually get selected.
void UXOpenGLRenderDevice::DrawGouraudProgram::CreateInputLayout()
{
	using Vert = DrawGouraudVertex;

	// VAO A: no-normals case
	for (INT i = 0; i < 6; ++i)
		if (i != 2)
			glEnableVertexAttribArray(i);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT,   sizeof(Vert), (GLvoid*)(offsetof(Vert, DrawID)));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, TexCoords)));
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, LightColor)));
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, FogColor)));
	VertBuffer.SetInputLayoutCreated();

	// VAO B: with-normals case, references both VBOs
	glBindVertexArray(NormalsBuffer.GetVaoObjectName());
	for (INT i = 0; i < 6; ++i)
		glEnableVertexAttribArray(i);
	glBindBuffer(GL_ARRAY_BUFFER, VertBuffer.GetBufferObjectName());
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(0));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT,   sizeof(Vert), (GLvoid*)(offsetof(Vert, DrawID)));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, TexCoords)));
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, LightColor)));
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)(offsetof(Vert, FogColor)));
	glBindBuffer(GL_ARRAY_BUFFER, NormalsBuffer.GetBufferObjectName());
	glVertexAttribPointer(2, 4, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(DrawGouraudNormal), (GLvoid*)(0));
	NormalsBuffer.SetInputLayoutCreated();

	// Leave VAO A actually bound: VertBuffer.bBound is still true from the Bind() call our caller
	// (MapBuffers) made before invoking us, and that flag must keep matching GL's real current VAO,
	// or the next Flush()'s "no-op if already bBound" VertBuffer.Bind() would wrongly skip restoring
	// VAO A and leave VAO B active regardless of bNeedNormalsThisPass. Also explicitly restore
	// GL_ARRAY_BUFFER to VertBuffer's own VBO -- the VAO-B setup above left it pointing at
	// NormalsBuffer's VBO, and unlike the VAO binding, GL_ARRAY_BUFFER isn't part of a VAO's captured
	// state, so it stays wrong until something corrects it. On the non-persistent-buffer fallback
	// path, VertBuffer.BufferData()'s glBufferSubData(GL_ARRAY_BUFFER, ...) call implicitly targets
	// whatever is CURRENTLY bound to GL_ARRAY_BUFFER -- since VertBuffer.bBound is (correctly) still
	// true, Bind()'s no-op-if-already-bound check means nothing ever re-issues this glBindBuffer
	// again, so every subsequent vertex upload for this shader would silently land in NormalsBuffer's
	// storage instead of VertBuffer's, leaving VertBuffer's actual GPU-visible vertex data stale --
	// invisible on the persistent-buffer path (BufferData() is a no-op there) but the exact cause of
	// visibly stale/corrupted Gouraud geometry when persistent buffers are off.
	glBindBuffer(GL_ARRAY_BUFFER, VertBuffer.GetBufferObjectName());
	glBindVertexArray(VertBuffer.GetVaoObjectName());
}

void UXOpenGLRenderDevice::DrawGouraudProgram::MapBuffers()
{
	if (!NormalsBuffer.Buffer)
	{
		NormalsBuffer.GenerateVertexBuffer(RenDev);
		NormalsBuffer.MapVertexBuffer(RenDev->UsingPersistentBuffers, VertexBufferSize, RenDev->UseBufferInvalidation);
	}
	// Base MapBuffers() maps VertBuffer/ParametersBuffer and, on first call, binds VertBuffer and
	// calls CreateInputLayout() -- which needs NormalsBuffer already mapped (above) so it can build
	// VAO B against it.
	MultiSpecializationShaderProgramImpl<DrawGouraudVertex, DrawGouraudParameters>::MapBuffers();
}

void UXOpenGLRenderDevice::DrawGouraudProgram::UnmapBuffers()
{
	NormalsBuffer.DeleteBuffer();
	MultiSpecializationShaderProgramImpl<DrawGouraudVertex, DrawGouraudParameters>::UnmapBuffers();
}

// this->VertBuffer.Bind() (called just before this) always rebinds VAO A -- if this pass needs
// real normals, override that with VAO B. NormalsBuffer.Bind() shares VertBuffer's ArrayPoint
// binding slot (both are GL_ARRAY_BUFFER BufferObjects), so it correctly invalidates VertBuffer's
// bBound cache too: the next flush's unconditional VertBuffer.Bind() will re-issue glBindVertexArray
// back to VAO A the next time bNeedNormalsThisPass is false, with no extra bookkeeping needed here.
void UXOpenGLRenderDevice::DrawGouraudProgram::OnVertBufferBound()
{
	if (bNeedNormalsThisPass)
		NormalsBuffer.Bind();
}

// Gated on bNeedNormalsThisPass, unlike position tracking (Advance(), unconditional in BufferVert's
// call sites -- that's what keeps NormalsBuffer's ring position numerically identical to
// VertBuffer's at all times). BufferData() itself must NOT run unconditionally: on the persistent-
// buffer path it's a no-op regardless, but on the non-persistent fallback it calls glBufferSubData
// against whatever buffer is currently bound to GL_ARRAY_BUFFER -- which is VertBuffer's, not
// NormalsBuffer's, whenever OnVertBufferBound() above didn't call NormalsBuffer.Bind() -- so calling
// it here unconditionally would silently corrupt VertBuffer's data in that fallback path. Skipping
// the upload when normals aren't needed is also simply correct: nothing written this pass into
// NormalsBuffer is meaningful (BufferVert only packs real data when bNeedNormalsThisPass is true),
// and nothing will read it until a future pass where this condition is true again re-uploads it.
void UXOpenGLRenderDevice::DrawGouraudProgram::OnVertBufferUploaded()
{
	if (bNeedNormalsThisPass)
		NormalsBuffer.BufferData(false);
}

void UXOpenGLRenderDevice::DrawGouraudProgram::OnVertBufferRotated()
{
	// Rotate()'s non-persistent-buffer branch does an implicit-target glBufferData(GL_ARRAY_BUFFER,
	// ...) orphan call -- it does NOT bind itself first, it trusts whatever is currently bound. The
	// caller (Flush()) just explicitly rebound VertBuffer right before calling VertBuffer.Rotate(),
	// so GL_ARRAY_BUFFER is now VertBuffer's, not NormalsBuffer's -- without this Bind(), the orphan
	// call below would silently reallocate VertBuffer's real GPU object down to NormalsBuffer's much
	// smaller size (this was the actual cause of the non-persistent-buffer flicker/GL_INVALID_VALUE
	// bug: VertBuffer's object stayed correctly *bound*, just wrongly *sized*, which is why a
	// bind-mismatch check didn't catch it).
	NormalsBuffer.Bind();
	// Position math only -- the caller (MultiSpecializationShaderProgramImpl::Flush) sequences a
	// shared WaitRotation() after this hook returns and before any new-slot write, covering
	// NormalsBuffer the same way it covers VertBuffer/ParametersBuffer/CommandBuffer.
	NormalsBuffer.Rotate();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

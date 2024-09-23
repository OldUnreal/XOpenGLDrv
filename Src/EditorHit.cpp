/*=============================================================================
	EditorHit.cpp: Unreal XOpenGL Editor routines.
	Copyright 2014-2016 Oldunreal

	Revision history:
		* Created by Sebastian Kaufel
		* added into XOpenGL by Smirftsch

=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

void UXOpenGLRenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UXOpenGLRenderDevice::PushHit);

	// Save the passed info on the working stack.
	INT Index = HitStack.Add(Count);
	appMemcpy(&HitStack(Index), Data, Count);

	// Save the full HitStack. This is needed for Hit->Parent code (e.g. HCoords).
	INT Offset = HitMem.Add(HitStack.Num());
	appMemcpy(&HitMem(Offset), &HitStack(0), HitStack.Num());
	HitMemOffs.AddItem(Offset);

	//
	// Use HitMemOffs.Num() to generate a new HitColor.
	//
	// One of the major problems is that the OpenGL implementation is fairly free how it handles it's rounding.
	// See: https://www.opengl.org/wiki/Normalized_Integer#Unsigned
	//
	// The basic idea to get around this issue is to add one (in bit representation) and not use the two least
	// significant bits on any color channel, which still leaves 18 bits and thus 262144 unique colors to use.
	// If these aren't enough or ignoring the two least significant bits is not enough, I can plain use a format
	// with 16 bits per color (which I intend to do anyway).
	//
	DWORD Seed = HitMemOffs.Num();
	//debugf( TEXT("(Seed=%i,(((Seed&0x0000003Fu)<< 2u)+1u)=0x%08X"), Seed, (((Seed&0x0000003Fu)<< 2u)+1u) );
	HitColor.X = (((Seed & 0x0000003F) << 2) | 1) / 255.f;
	HitColor.Y = (((Seed & 0x00000FC0) >> 4) | 1) / 255.f;
	HitColor.Z = (((Seed & 0x0003F000) >> 10) | 1) / 255.f;
	HitColor.W = 1.f;

	unguard;
}

void UXOpenGLRenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UXOpenGLRenderDevice::PopHit);

	check(Count <= HitStack.Num());

	// Force end buffing polygons. This makes sure we won't
	// draw the buffered polygones under the wrong name.
	SetProgram(No_Prog);
	//glFinish();

	// Handle Force hit.
	if (bForce)
	{
		if (HitStack.Num() <= *HitSize)
		{
			HitCount = HitStack.Num();
			appMemcpy(HitData, &HitStack(0), HitCount);
		}
		else
		{
			GWarn->Logf(TEXT("XOpenGL: PopHit HitData to small for HitStack."));
			HitCount = 0;
		}
	}

	// Remove the passed info from the working stack.
	HitStack.Remove(HitStack.Num() - Count, Count);

	unguard;
}
// Called at the end of UXOpenGLRenderDevice::Lock().
void UXOpenGLRenderDevice::LockHit(BYTE* InHitData, INT* InHitSize)
{
	guard(UXOpenGLRenderDevice::LockHit);

	HitCount = 0;
	HitData = InHitData;
	HitSize = InHitSize;

	if (HitTesting())
	{
		SetBlend(~0u);

		// Disabled dithering.
		glDisable(GL_DITHER);

        if (UseSRGBTextures && OpenGLVersion == GL_Core)
            glDisable(GL_FRAMEBUFFER_SRGB);

		// Set special clear color.
		FPlane ScreenClear = FPlane(0.f, 0.f, 0.f, 0.f);
		glClearColor(ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		// Clear HitProxy memory.
		HitMem.Empty();
		HitMemOffs.Empty();
	}

	unguard;
}

// Called at the end of UXOpenGLRenderDevice::Unlock().
void UXOpenGLRenderDevice::UnlockHit(UBOOL Blit)
{
	guard(UOpenGLRenderDevice::UnlockHit);

	check(HitStack.Num() == 0);

	if (HitTesting())
	{
		// Reuse Compose memory for ReadPixels.
		guard(AllocateCompose);
		DWORD MinComposeSize = Viewport->HitXL*Viewport->HitYL*sizeof(FColor);
		if (MinComposeSize>ComposeSize)
		{
			Compose = (BYTE*)appRealloc(Compose, MinComposeSize, TEXT("Compose"));
			if (!Compose)
	#if ENGINE_VERSION==227
				appErrorf(*LocalizeError(TEXT("ComposeAlloc")), MinComposeSize);
	#else
				appErrorf(LocalizeError(TEXT("ComposeAlloc")), MinComposeSize);
	#endif
			ComposeSize = MinComposeSize;
		}
		unguard;

		// Read Pixels.
		FColor* Pixels = (FColor*)Compose;
		glReadPixels(Viewport->HitX, Viewport->SizeY - Viewport->HitY - Viewport->HitYL, Viewport->HitXL, Viewport->HitYL, GL_RGBA, GL_UNSIGNED_BYTE, Pixels); // Actually we are upside down!

		// Evaluate hit. Prefer last drawn (e.g. highest value) to match SoftDrv behaviour/ease up line selection.
		DWORD Candidate = 0;
		for (INT i = 0; i<(Viewport->HitYL*Viewport->HitXL); i++)
			Candidate = Max(Candidate, (GET_COLOR_DWORD(Pixels[i]) & 0x00FCFCFC));

		// Handle hit.
		if (Candidate)
		{
			// Convert candidate into an Index.
			INT Index = ((Candidate & 0x000000FCu) >> 2) | ((Candidate & 0x0000FC00) >> 4) | ((Candidate & 0x00FC0000) >> 6);
			//debugf( TEXT("(Candidate=0x%08X,Index=0x%08X=%i)"), Candidate, Index, Index );

			// Sanity checks.
			if (Index && Index <= HitMemOffs.Num())
			{
				INT HitDataStart = HitMemOffs(Index - 1);
				INT HitDataEnd = Index<HitMemOffs.Num() ? HitMemOffs(Index) : HitMem.Num();
				INT HitDataCount = HitDataEnd - HitDataStart;

				// Sanity check.
				if (HitDataCount>0 && HitDataCount <= *HitSize)
				{
					HitCount = HitDataCount;
					appMemcpy(HitData, &HitMem(HitDataStart), HitDataCount);
				}
				// Overflow case.
				else if (HitDataCount>*HitSize)
				{
					GWarn->Logf(TEXT("XOpenGL: UnlockHit HitData to small for HitStack."));
					HitCount = 0;
				}
				// Warning.
				else if (HitDataCount <= 0)
					GWarn->Logf(TEXT("XOpenGL: UnlockHit HitDataCount<=0"));
			}
			// Warnings.
			else if (Index == 0)
				GWarn->Logf(TEXT("XOpenGL: UnlockHit Index == 0."));
			else if (Index>HitMemOffs.Num())
				GWarn->Logf(TEXT("XOpenGL: UnlockHit Index>HitMemOffs.Num() (Index=%i,HitMemOffs.Num()=%i,Candidate=0x%08X)"), Index, HitMemOffs.Num(), Candidate);
		}

		// Update HitSize.
		checkSlow(HitSize);
		*HitSize = HitCount;

		// Renable dithering. Should match whats done in SetRes().
		glEnable(GL_DITHER);

		// Clear HitProxy memory.
		HitMem.Empty();
		HitMemOffs.Empty();

        if (UseSRGBTextures && OpenGLVersion == GL_Core)
            glEnable(GL_FRAMEBUFFER_SRGB);
	}

	unguard;
}
// Called at the end of UOpenGLRenderDevice::SetSceneNode().
void UXOpenGLRenderDevice::SetSceneNodeHit(FSceneNode* Frame)
{
	guard(UOpenGLRenderDevice::SetSceneNodeHit);

	// Set clip planes.
	if (HitTesting())
	{
#if 0
		FVector N[4];
		N[0] = (FVector((Viewport->HitX - Frame->FX2)*Frame->RProj.Z, 0, 1) ^ FVector(0, -1, 0)).SafeNormal();
		N[1] = (FVector((Viewport->HitX + Viewport->HitXL - Frame->FX2)*Frame->RProj.Z, 0, 1) ^ FVector(0, +1, 0)).SafeNormal();
		N[2] = (FVector(0, (Viewport->HitY - Frame->FY2)*Frame->RProj.Z, 1) ^ FVector(+1, 0, 0)).SafeNormal();
		N[3] = (FVector(0, (Viewport->HitY + Viewport->HitYL - Frame->FY2)*Frame->RProj.Z, 1) ^ FVector(-1, 0, 0)).SafeNormal();
		for (INT i = 0; i<4; i++)
		{
			double D0[4] = { N[i].X, N[i].Y, N[i].Z, 0 };
			glClipPlane((GLenum)(GL_CLIP_PLANE0 + i), D0);
			glEnable((GLenum)(GL_CLIP_PLANE0 + i));
		}
#endif
	}

	unguard;
}
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

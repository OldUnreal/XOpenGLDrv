/*=============================================================================
	SetTexture.cpp: Unreal XOpenGL Texture handling.

	Copyright 2014-2021 Oldunreal

	Revision history:
		* Created by Smirftsch
=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

//
// stijn: Drawing a P8 texture as PF_Masked means rendering all pixels with palette index 0 as fully transparent.
// The only way to do this is to fix up these textures just before we upload them. Specifically, we can set
// alpha to 0 on all pixels that index slot 0 in the palette.
//
// The problem is that we might render the textures without PF_Masked later. Without PF_Masked, we need to actually
// pixels that index Palette[0], so we cannot set alpha to 0 on said pixels.
//
// This means we potentially need two copies of all P8 textures: one suitable for masked drawing, with all alpha values
// for Palette[0] pixels set to zero, and one suitable for non-masked drawing with all Palette[0] pixels converted
// to RGBA as-is.
//
// This function ensures the masked and non-masked copies of the texture map to different keys in the BindMap.
// We rely on the fact that the least significant bits of a CacheID are always zero. This means it is safe to reuse
// said bits as a tag.
//
#define MASKED_TEXTURE_TAG 4
static void FixCacheID(FTextureInfo& Info, DWORD PolyFlags)
{
	if ((PolyFlags & PF_Masked) && Info.Format == TEXF_P8)
	{
		Info.CacheID |= MASKED_TEXTURE_TAG;
	}
}

UXOpenGLRenderDevice::FCachedTexture*
UXOpenGLRenderDevice::GetCachedTextureInfo
(
	INT Multi,
	FTextureInfo& Info,
	DWORD PolyFlags,
	BOOL& IsResidentBindlessTexture,
	BOOL& IsBoundToTMU,
	BOOL& IsTextureDataStale,
	BOOL ShouldResetStaleState
)
{
	FixCacheID(Info, PolyFlags);
	FCachedTexture* Result = BindMap->Find(Info.CacheID);

	if (UsingBindlessTextures && Result && Result->BindlessTexHandle)
		IsResidentBindlessTexture = TRUE;

	// The texture is not bindless resident
	IsBoundToTMU = Result && TexInfo[Multi].CurrentCacheID == Info.CacheID;

	// already cached but the texture data may have changed
	if (Info.bRealtimeChanged)
	{
		if (!Info.Texture)
		{
			IsTextureDataStale = TRUE;
			return Result;
		}

		if (Result)
		{
#if ENGINE_VERSION==227
			IsTextureDataStale = Info.RenderTag != Result->RealtimeChangeCount;
#elif UNREAL_TOURNAMENT_OLDUNREAL
			IsTextureDataStale = Info.Texture->RealtimeChangeCount != Result->RealtimeChangeCount;
#else
			IsTextureDataStale = TRUE;
#endif

			if (ShouldResetStaleState)
			{
#if ENGINE_VERSION==227
				Result->RealtimeChangeCount = Info.RenderTag;
#elif UNREAL_TOURNAMENT_OLDUNREAL
				Result->RealtimeChangeCount = Info.Texture->RealtimeChangeCount;
#endif
			}
		}
		else
		{
			IsTextureDataStale = TRUE;
		}
	}

	return Result;
}

BOOL UXOpenGLRenderDevice::WillTextureStateChange(INT Multi, FTextureInfo& Info, DWORD PolyFlags)
{
	BOOL IsResidentBindlessTexture = FALSE, IsBoundToTMU = FALSE, IsTextureDataStale = FALSE;
	GetCachedTextureInfo(Multi, Info, PolyFlags, IsResidentBindlessTexture, IsBoundToTMU, IsTextureDataStale, FALSE);

	// We need to re-upload a texture we're currently using
	if (IsTextureDataStale)
		return TRUE;

	// We will have to evict a TMU => stop batching
	if (!UsingBindlessTextures && !IsBoundToTMU)
		return TRUE;

	return FALSE;
}

#if UNREAL_TOURNAMENT_OLDUNREAL
UBOOL UXOpenGLRenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	switch( Format )
	{
		case TEXF_P8:
		case TEXF_RGBA7:
		case TEXF_RGB8:
		case TEXF_BGRA8:
			return true;
		case TEXF_BC1:
		case TEXF_BC2:
		case TEXF_BC3:
		case TEXF_BC4:
		case TEXF_BC4_S:
		case TEXF_BC5:
		case TEXF_BC5_S:
# ifndef __LINUX_ARM__
		case TEXF_BC6H_S:
		case TEXF_BC6H:
		case TEXF_BC7:
# endif
			return SupportsTC;
		default:
			return false;
	}
}

// called when we need to re-upload a part of a texture
void UXOpenGLRenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guard(UOpenGLRenderDevice::UpdateTextureRect);

	if ((Info.NumMips <= 0) || !Info.Mips[0]->DataPtr)
		return;

	BOOL IsResidentBindlessTexture = FALSE, IsBoundToTMU = FALSE, IsTextureDataStale = FALSE;
	FCachedTexture* Bind = GetCachedTextureInfo(-1, Info, PF_None, IsResidentBindlessTexture, IsBoundToTMU, IsTextureDataStale, FALSE);

	// Just upload the full texture if we have never uploaded it before
	if (!Bind)
		return;

	if (WillTextureStateChange(-1, Info, 0))
	{
		// flush buffered data
		Shaders[ActiveProgram]->Flush(false);
	}

	// Use TMU 8 as a temporary storage location
	if (!IsResidentBindlessTexture)
	{
		BindTextureAndSampler(UploadIndex, Bind);
		TexInfo[UploadIndex].CurrentCacheID = Info.CacheID;
		TexInfo[UploadIndex].BindlessTexHandle = Bind->BindlessTexHandle;
	}

	Info.bRealtimeChanged = 0;

	FMemMark Mark(GMem);
	INT DataBlock = FTextureBlockBytes(Info.Format);
	INT DataSize = FTextureBytes(Info.Format, UL, VL);
	auto Data = new(GMem, DataSize) BYTE;

	INT USize = Info.Mips[0]->USize;
	BYTE* Input = Info.Mips[0]->DataPtr + (USize * V + U) * DataBlock;
	BYTE* Output = Data;
	BYTE* OutputEnd = Output + DataSize;
	while (Output < OutputEnd)
	{
		appMemcpy(Output, Input, UL * DataBlock);
		Input += USize * DataBlock;
		Output += UL * DataBlock;
	}

	UploadTexture(Info, Bind, PF_None, false, IsResidentBindlessTexture, TRUE, U, V, UL, VL, Data);

	Mark.Pop();

	unguard;
}
#endif

void UXOpenGLRenderDevice::SetNoTexture( INT Multi )
{
	guard(UXOpenGLRenderDevice::SetNoTexture);
	if( TexInfo[Multi].CurrentCacheID != 0 )
	{
		glBindTexture( GL_TEXTURE_2D, 0 );
		TexInfo[Multi].CurrentCacheID = 0;
	}
	unguard;
}

void UXOpenGLRenderDevice::SetSampler(GLuint Sampler, FTextureInfo& Info, UBOOL SkipMipmaps, UBOOL IsLightOrFogMap, UBOOL NoSmooth)
{
	guard(UOpenGLRenderDevice::SetSampler);

#if ENGINE_VERSION==227
	if (Info.UClampMode)
		glSamplerParameteri(Sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	if (Info.VClampMode)
		glSamplerParameteri(Sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif // ENGINE_VERSION

	// Also set for light and fogmaps.
    if ( IsLightOrFogMap )
    {
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    }

	// Set texture sampler state.
	if (NoSmooth)
	{
		// "PF_NoSmooth" implies that the worst filter method is used, so have to do this (even if NoFiltering is set) in order to get the expected results.
		glSamplerParameteri(Sampler, GL_TEXTURE_MIN_FILTER, SkipMipmaps ? GL_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
		glSamplerParameteri(Sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		if (NoFiltering)
			return;

		glSamplerParameteri(Sampler, GL_TEXTURE_MIN_FILTER, SkipMipmaps ? GL_LINEAR : (UseTrilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
		glSamplerParameteri(Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (MaxAnisotropy != 0.f)
			glSamplerParameterf(Sampler, GL_TEXTURE_MAX_ANISOTROPY, MaxAnisotropy);

		if (LODBias != 0.f)
			glSamplerParameteri(Sampler, GL_TEXTURE_LOD_BIAS, appRound(LODBias));

	}
	unguard;
}

#if ENGINE_VERSION==1100
static FName UserInterface = FName(TEXT("UserInterface"), FNAME_Intrinsic);
#endif

BOOL UXOpenGLRenderDevice::UploadTexture(FTextureInfo& Info, FCachedTexture* Bind, DWORD PolyFlags, BOOL IsFirstUpload, BOOL IsBindlessTexture, BOOL PartialUpload, INT U, INT V, INT UL, INT VL, BYTE* TextureData)
{
	bool UnsupportedTexture = false;

	if (Info.NumMips && !Info.Mips[0])
	{
		GWarn->Logf(TEXT("Encountered texture %ls with invalid MipMaps!"), Info.Texture->GetPathName());
		Info.NumMips = 0;
		UnsupportedTexture = true;
	}
	else
	{
		// Find lowest mip level support.
		while (Bind->BaseMip<Info.NumMips && Max(Info.Mips[Bind->BaseMip]->USize, Info.Mips[Bind->BaseMip]->VSize)>MaxTextureSize)
		{
			Bind->BaseMip++;
		}

		if (Bind->BaseMip >= Info.NumMips)
		{
			GWarn->Logf(TEXT("Encountered oversize texture %ls without sufficient mipmaps."), Info.Texture->GetPathName());
			UnsupportedTexture = true;
		}
	}

	if (SupportsLazyTextures)
		Info.Load();

	Info.bRealtimeChanged = 0;

	UBOOL UnpackSRGB = UseSRGBTextures && !(PolyFlags & PF_Modulated)
#if ENGINE_VERSION==1100 // Hack for DeusExUI.UserInterface.
		&& !(Info.Texture && Info.Texture->GetOuter() && Info.Texture->GetOuter()->GetFName() == UserInterface)
#endif
		;

	// Generate the palette.
	FColor  LocalPal[256];
	FColor* Palette = Info.Palette;// ? Info.Palette : LocalPal; // Save fallback for malformed P8.
	if (Info.Format == TEXF_P8)
	{
		if (!Info.Palette)
			appErrorf(TEXT("Encountered bogus P8 texture %ls"), Info.Texture->GetFullName());

		if (PolyFlags & PF_Masked)
		{
			// kaufel: could have kept the hack to modify and reset Info.Palette[0], but opted against.
			appMemcpy(LocalPal, Info.Palette, 256 * sizeof(FColor));
			LocalPal[0] = FColor(0, 0, 0, 0);
			Palette = LocalPal;
		}
	}

	// Download the texture.
	// If !0 allocates the requested amount of memory (and frees it afterwards).
	DWORD MinComposeSize = 0;

	GLuint InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	GLuint SourceFormat = GL_RGBA;
	GLuint SourceType   = GL_UNSIGNED_BYTE;

	if (!SupportsS3TC && FIsCompressedFormat(Info.Format))
		UnsupportedTexture = true;

	// Unsupported can already be set in case of only too large mip maps available.
	if (!UnsupportedTexture)
	{
		switch ((BYTE)Info.Format) //!!
		{
			// P8 -- Default palettized texture format.
		case TEXF_P8:
			MinComposeSize = Info.Mips[Bind->BaseMip]->USize * Info.Mips[Bind->BaseMip]->VSize * 4;
			InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			SourceFormat = GL_RGBA;
			break;

			// TEXF_BGRA8_LM used for Light and FogMaps.
		case TEXF_BGRA8_LM:
			MinComposeSize = Info.Mips[Bind->BaseMip]->USize * Info.Mips[Bind->BaseMip]->VSize * 4;
			InternalFormat = GL_RGBA8;
			if (OpenGLVersion == GL_Core)
			  SourceFormat = GL_BGRA; // Was GL_RGBA;
			else
			  SourceFormat = GL_RGBA; // ES prefers RGBA...
			break;
#if ENGINE_VERSION==227
			// RGB10A2_LM. Used for HDLightmaps.
		case TEXF_RGB10A2_LM:
			MinComposeSize = Info.Mips[Bind->BaseMip]->USize * Info.Mips[Bind->BaseMip]->VSize * 4;
			InternalFormat = GL_RGB10_A2;
			SourceFormat = GL_RGBA;
			SourceType = GL_UNSIGNED_INT_2_10_10_10_REV; // This seems to make alpha to be placed in the right spot.
			break;
#endif
			// TEXF_R5G6B5 (Just to make 0x02 supported).
		case TEXF_R5G6B5:
			InternalFormat = GL_RGB;
			SourceFormat = GL_RGB;
			SourceType = GL_UNSIGNED_SHORT_5_6_5_REV;

			// RGB8/RGBA8 -- (used f.e. for DefPreview), also used by Brother Bear.
		case TEXF_RGB8:
			InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			SourceFormat = GL_RGB; // stijn: was GL_RGB, then became GL_BGR, then turned back into GL_RGB after a discussion with Han on 25 OCT 2020. Brother bear definitely uses the GL_RGB pixel format for this one.
			break;
		case TEXF_RGBA8_:
			SourceFormat = GL_RGBA;
			break;

			// stijn: this was case TEXF_RGBA8 before, but TEXF_RGBA8 is a synonym for TEXF_BGRA8. The pixel format is actually BGRA8, though, so we might as well use that
		case TEXF_BGRA8:
			InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			SourceFormat = GL_BGRA; // Was GL_RGBA;
			break;

#if ENGINE_VERSION==227 && !defined(__LINUX_ARM__)
		case TEXF_RGBA16:
			InternalFormat = GL_RGBA16;
			SourceFormat = GL_RGBA;
			SourceType = GL_UNSIGNED_SHORT;
			break;
#endif
		case TEXF_RGB16_:
			InternalFormat = GL_RGBA16;
			SourceFormat = GL_RGBA;
			SourceType = GL_UNSIGNED_SHORT;
			break;
			// S3TC -- Ubiquitous Extension.
		case TEXF_BC1:
			if (OpenGLVersion == GL_Core)
			{
				/*
				Rather than expose a separate "sRGB_compression" extension, it makes more sense to specify a dependency between EXT_texture_compression_s3tc
				and EXT_texture_sRGB extension such that when BOTH extensions are exposed, the GL_COMPRESSED_SRGB*_S3TC_DXT*_EXT tokens are accepted.
				*/
				if (UnpackSRGB)
				{
					InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
					break;
				}
			}
			InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			break;

#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
		case TEXF_BC2:
			if (OpenGLVersion == GL_Core)
			{
				if (UnpackSRGB)
				{
					InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
					break;
				}
				InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				break;
			}
			else
			{
				InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE;
				break;
			}

		case TEXF_BC3:
			if (OpenGLVersion == GL_Core)
			{
				if (UnpackSRGB)
				{
					InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
					break;
				}
				InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				break;
			}
			else
			{
				InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE;
				break;
			}

			// RGTC -- Core since OpenGL 3.0. Also available on Direct3D 10. Not in GLES it seems.
		case TEXF_BC4:
			InternalFormat = GL_COMPRESSED_RED_RGTC1;
			break;
		case TEXF_BC4_S:
			InternalFormat = GL_COMPRESSED_SIGNED_RED_RGTC1;
			break;
		case TEXF_BC5:
			InternalFormat = GL_COMPRESSED_RG_RGTC2;
			// TODO: stijn: Fix up the blue channel here!
			// if (In.TextureFormat == )" << TEXF_BC5 << R"(u) // BC5 (GL_COMPRESSED_RG_RGTC2) compression
			//	Color.b = sqrt(1.0 - Color.r * Color.r + Color.g * Color.g);
			break;
		case TEXF_BC5_S:
			InternalFormat = GL_COMPRESSED_SIGNED_RG_RGTC2;
			break;

			// BPTC Core since 4.2. BC6H and BC7 in D3D11.
#ifndef __LINUX_ARM__
		case TEXF_BC6H:
			InternalFormat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT; //BC6H
			break;
		case TEXF_BC6H_S:
			InternalFormat = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT; //BC6H
			break;
		case TEXF_BC7:
			if (UnpackSRGB)
			{
				InternalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
				break;
			}
			InternalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM; //BC7
			break;
#endif

#endif
			// Default: Mark as unsupported.
		default:
			GWarn->Logf(TEXT("Unknown texture format %ls on texture %ls."), *FTextureFormatString(Info.Format), Info.Texture->GetPathName());
			UnsupportedTexture = true;
			break;
		}
	}

	// If not supported make sure we have enough compose mem for a fallback texture.
	if (UnsupportedTexture)
	{
		// 256x256 RGBA texture.
		MinComposeSize = 256 * 256 * 4;
	}

	// Allocate or enlarge compose memory if needed.
	guard(AllocateCompose);
	if (MinComposeSize > ComposeSize)
	{
		Compose = (BYTE*)appRealloc(Compose, MinComposeSize, TEXT("Compose"));
		if (!Compose)
			appErrorf(TEXT("Failed to allocate memory for texture compose."));
		ComposeSize = MinComposeSize;
	}
	unguard;

	// Unpack texture data.
	INT MaxLevel = -1;
	if (PartialUpload && TextureData)
	{
		if (!IsBindlessTexture)
			glTexSubImage2D(GL_TEXTURE_2D, ++MaxLevel, U, V, UL, VL, SourceFormat, SourceType, TextureData);
		else glTextureSubImage2D(Bind->Id, ++MaxLevel, U, V, UL, VL, SourceFormat, SourceType, TextureData);

		//debugf(TEXT("Partially reuploaded texture - U %d - V %d - UL %d - VL %d - Name %ls"), U, V, UL, VL, *FObjectName(Info.Texture));
	}
	else if (!UnsupportedTexture)
	{
		guard(Unpack texture data);
		for (INT MipIndex = Bind->BaseMip; MipIndex < Info.NumMips; MipIndex++)
		{
			// Convert the mipmap.
			FMipmapBase* Mip = Info.Mips[MipIndex];
			BYTE* ImgSrc = NULL;
			GLsizei      CompImageSize = 0; // !0 also enables use of glCompressedTex[Sub]Image2D.
			GLsizei      USize = Mip->USize;
			GLsizei      VSize = Mip->VSize;

			if (Mip && Mip->DataPtr)
			{
				switch ((BYTE)Info.Format) //!!
				{
					// P8 -- Default palettized texture format.
				case TEXF_P8:
					guard(ConvertP8_RGBA8888);
					ImgSrc = Compose;
					DWORD* Ptr = (DWORD*)Compose;
					INT Count = USize * VSize;
					for (INT i = 0; i < Count; i++)
						*Ptr++ = GET_COLOR_DWORD(Palette[Mip->DataPtr[i]]);
					unguard;
					break;

					// TEXF_BGRA8_LM used by light and fogmaps.
				case TEXF_BGRA8_LM:
					guard(ConvertBGRA7777_RGBA8888);
					if (OpenGLVersion == GL_Core)
					{
						ImgSrc = Mip->DataPtr;
					}
					else // GL ES can't do BGRA so we need to swap the colors here
					{
						ImgSrc = Compose;
						DWORD* Ptr = (DWORD*)Compose;
						INT Count = USize * VSize;
						for (INT i = 0; i < Count; i++)
						{
							FColor Color = ((FColor*)Mip->DataPtr)[i];
							Exchange(Color.R, Color.B);
							*Ptr++ = *(DWORD*)&Color;
						}
					}
					unguard;
					break;

#if ENGINE_VERSION==227
					// RGB9.
				case TEXF_RGB10A2_LM:
					ImgSrc = Mip->DataPtr;
					break;
#endif

					// RGB8/RGBA8 -- Actually used by Brother Bear.
				case TEXF_RGB8:
				case TEXF_RGBA8_:
				case TEXF_BGRA8:
#if ENGINE_VERSION==227
				case TEXF_RGBA16:
				case TEXF_RGB16_:
#endif
					ImgSrc = Mip->DataPtr;
					break;

					// S3TC -- Ubiquitous Extension. Was TEXF_DXTx before
				case TEXF_BC1:
#if ENGINE_VERSION==227 || UNREAL_TOURNAMENT_OLDUNREAL
				case TEXF_BC2:
				case TEXF_BC3:
					// RGTC -- Core since OpenGL 3.0. Also available on Direct3D 10.
				case TEXF_BC4:
				case TEXF_BC4_S:
				case TEXF_BC5:
				case TEXF_BC5_S:
				case TEXF_BC6H:
				case TEXF_BC6H_S:
				case TEXF_BC7:
					CompImageSize = FTextureBytes(Info.Format, USize, VSize);
					ImgSrc = Mip->DataPtr;
					break;
#endif

					// Should not happen (TM).
				default:
					appErrorf(TEXT("Unpacking unknown format %ls on %ls."), *FTextureFormatString(Info.Format), Info.Texture->GetFullName());
					break;
				}
			}
			else
			{
				if (GIsEditor)
					appMsgf(TEXT("Unpacking %ls on %ls failed due to invalid data."), *FTextureFormatString(Info.Format), Info.Texture->GetFullName());
				else
					GWarn->Logf(TEXT("Unpacking %ls on %ls failed due to invalid data."), *FTextureFormatString(Info.Format), Info.Texture->GetFullName());
				break;
			}

			// Upload texture.
			if (!IsFirstUpload)
			{
				if (CompImageSize)
				{
					if (!IsBindlessTexture)
						glCompressedTexSubImage2D(GL_TEXTURE_2D, ++MaxLevel, 0, 0, USize, VSize, InternalFormat, CompImageSize, ImgSrc);
					else glCompressedTextureSubImage2D(Bind->Id, ++MaxLevel, 0, 0, USize, VSize, SourceFormat, SourceType, ImgSrc);
				}
				else
				{
					if (!IsBindlessTexture)
						glTexSubImage2D(GL_TEXTURE_2D, ++MaxLevel, 0, 0, USize, VSize, SourceFormat, SourceType, ImgSrc);
					else glTextureSubImage2D(Bind->Id, ++MaxLevel, 0, 0, USize, VSize, SourceFormat, SourceType, ImgSrc);
				}
			}
			else
			{
				if (CompImageSize)
				{
					if (GenerateMipMaps)
					{
						glCompressedTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, USize, VSize, 0, CompImageSize, ImgSrc);
						glGenerateMipmap(GL_TEXTURE_2D);
						MaxLevel = Info.NumMips;
						break;
					}
					else
					{
						glCompressedTexImage2D(GL_TEXTURE_2D, ++MaxLevel, InternalFormat, USize, VSize, 0, CompImageSize, ImgSrc);
					}
				}
				else
				{
					if (GenerateMipMaps)
					{
						glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, USize, VSize, 0, SourceFormat, SourceType, ImgSrc);
						glGenerateMipmap(GL_TEXTURE_2D); // generate a complete set of mipmaps for a texture object
						MaxLevel = Info.NumMips;
						break;
					}
					else
					{
						glTexImage2D(GL_TEXTURE_2D, ++MaxLevel, InternalFormat, USize, VSize, 0, SourceFormat, SourceType, ImgSrc);
					}
				}
			}
			if (GenerateMipMaps)
				break;
		}
		unguardf((TEXT("Unpacking %ls on %ls crashed due to invalid data."), *FTextureFormatString(Info.Format), Info.Texture->GetFullName()));

		// This should not happen. If it happens, a sanity check is missing above.
		if (!GenerateMipMaps && MaxLevel == -1)
			GWarn->Logf(TEXT("No mip map unpacked for texture %ls."), Info.Texture->GetPathName());
	}

	// Create and unpack a chequerboard fallback texture texture for an unsupported format.
	guard(Unsupported);
	if (UnsupportedTexture)
	{
		check(Compose);
		check(ComposeSize >= 64 * 64 * 4);
		DWORD PaletteBM[16] =
		{
			0x00000000u, 0x000000FFu, 0x0000FF00u, 0x0000FFFFu,
			0x00FF0000u, 0x00FF00FFu, 0x00FFFF00u, 0x00FFFFFFu,
			0xFF000000u, 0xFF0000FFu, 0xFF00FF00u, 0xFF00FFFFu,
			0xFFFF0000u, 0xFFFF00FFu, 0xFFFFFF00u, 0xFFFFFFFFu,
		};
		MaxLevel = 0;
		DWORD* Ptr = (DWORD*)Compose;
		for (INT i = 0; i < (256 * 256); i++)
			*Ptr++ = PaletteBM[(i / 16 + i / (256 * 16)) % 16]; //

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, Compose);
	}
	unguard;

	// Set max level.
	if (IsFirstUpload || Bind->MaxLevel != MaxLevel)
	{
		Bind->MaxLevel = MaxLevel;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MaxLevel);
	}

	// Cleanup.
	if (SupportsLazyTextures)
		Info.Unload();

	return !UnsupportedTexture;
}

void UXOpenGLRenderDevice::GenerateTextureAndSampler(FCachedTexture* Bind)
{
	glGenTextures(1, &Bind->Id);

	if (!Bind->Sampler)
		glGenSamplers(1, &Bind->Sampler);
}

void UXOpenGLRenderDevice::BindTextureAndSampler(INT Multi, FCachedTexture* Bind)
{
	glActiveTexture(GL_TEXTURE0 + Multi);
	glBindTexture(GL_TEXTURE_2D, Bind->Id);
	glBindSampler(Multi, Bind->Sampler);
}

void UXOpenGLRenderDevice::SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias)
{
	guard(UXOpenGLRenderDevice::SetTexture);

	if (ActiveProgram <= No_Prog)
        return;

	// Set panning.
	FTexInfo& Tex = TexInfo[Multi];
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

    // Account for all the impact on scale normalization.
	Tex.UMult = 1.f / (Info.UScale * static_cast<FLOAT>(Info.USize));
	Tex.VMult = 1.f / (Info.VScale * static_cast<FLOAT>(Info.VSize));

	STAT(clockFast(Stats.BindCycles));

	// Check if the texture is already bound to the correct TMU
	BOOL IsResidentBindlessTexture = FALSE, IsBoundToTMU = FALSE, IsTextureDataStale = FALSE;
	FCachedTexture* Bind = GetCachedTextureInfo(Multi, Info, PolyFlags, IsResidentBindlessTexture, IsBoundToTMU, IsTextureDataStale, TRUE);

	// Bail out early if the texture is fully up-to-date
	if (Bind && (IsResidentBindlessTexture || IsBoundToTMU) && !IsTextureDataStale)
	{
		Tex.BindlessTexHandle = Bind->BindlessTexHandle;
		STAT(unclockFast(Stats.BindCycles));
		return;
	}

    // Make current.
	Tex.CurrentCacheID   = Info.CacheID;

	if (!Bind)
	{
		// Figure out OpenGL-related scaling for the texture.
		Bind = &BindMap->Set( Info.CacheID, FCachedTexture() );
		memset(Bind, 0, sizeof(FCachedTexture));
	}

	UBOOL IsNewBind = Bind->Id == 0;
	if (IsNewBind)
	{
		UBOOL SkipMipmaps = (!GenerateMipMaps && Info.NumMips == 1 && !AlwaysMipmap);
		UBOOL IsLightOrFogMap = Info.Format == TEXF_BGRA8_LM || Info.Format == TEXF_RGB10A2_LM;
		UBOOL NoSmooth = (PolyFlags & PF_NoSmooth) && (Multi == 0);
		GenerateTextureAndSampler(Bind);
		BindTextureAndSampler(Multi, Bind);
		SetSampler(Bind->Sampler, Info, SkipMipmaps, IsLightOrFogMap, NoSmooth);
	}
	else if (Bind->BindlessTexHandle == 0)
	{
		BindTextureAndSampler(Multi, Bind);
	}

	STAT(unclockFast(Stats.BindCycles));

	// Upload if needed.
	STAT(clockFast(Stats.ImageCycles));
	if( IsNewBind || Info.bRealtimeChanged || IsTextureDataStale )
		UploadTexture(Info, Bind, PolyFlags, IsNewBind, IsResidentBindlessTexture);

    if (UsingBindlessTextures && Bind->BindlessTexHandle == 0)
    {
        guard(MakeTextureHandleResident);
		Bind->BindlessTexHandle = glGetTextureSamplerHandleARB(Bind->Id, Bind->Sampler);

        if (!Bind->BindlessTexHandle)
        {
            GWarn->Logf(TEXT("Failed to get sampler for bindless texture: %ls!"), Info.Texture ? Info.Texture->GetFullName() : TEXT("LightMap/FogMap"));
            Bind->BindlessTexHandle = 0;
        }
        else
        {
            glMakeTextureHandleResidentARB(Bind->BindlessTexHandle);
        }

        unguard;
    }
    else if (IsNewBind)
    {
        Bind->BindlessTexHandle = 0;
    }

	Tex.BindlessTexHandle = Bind->BindlessTexHandle;

    CHECK_GL_ERROR();
	STAT(unclockFast(Stats.ImageCycles));
	unguard;
}

DWORD UXOpenGLRenderDevice::GetPolyFlagsAndShaderOptions(DWORD PolyFlags, DWORD& Options, BOOL RemoveOccludeIfSolid)
{
	if ((PolyFlags & (PF_RenderFog | PF_Translucent)) != PF_RenderFog)
		PolyFlags &= ~PF_RenderFog;

	if (!(PolyFlags & (PF_Translucent | PF_Modulated | PF_AlphaBlend | PF_Highlighted)))
		PolyFlags |= PF_Occlude;
	else if (RemoveOccludeIfSolid)
		PolyFlags &= ~PF_Occlude;
	// stijn: The skybox in zp01-umssakuracrashsite has PF_Translucent|PF_Masked. If we strip off PF_Masked here, the skybox renders incorrectly
	//else if (PolyFlags & (PF_Translucent | PF_AlphaBlend))
	//	PolyFlags &= ~PF_Masked;

	// fast path. If no relevant polyflags have changed since our previous query, then just return the same ShaderOptions as last time
	const DWORD RelevantPolyFlags = (PF_Modulated | PF_RenderFog | PF_Masked | PF_Straight_AlphaBlend | PF_Premultiplied_AlphaBlend | PF_Unlit | PF_Translucent | PF_Environment);
	if ((CachedPolyFlags & RelevantPolyFlags) ^ (PolyFlags & RelevantPolyFlags))
	{
		Options = ShaderOptions::OPT_None;

		if (PolyFlags & PF_Modulated)
			Options |= ShaderOptions::OPT_Modulated;

		if (PolyFlags & PF_RenderFog)
			Options |= ShaderOptions::OPT_RenderFog;

		if (PolyFlags & PF_Masked)
			Options |= ShaderOptions::OPT_Masked;

		if (PolyFlags & (PF_Straight_AlphaBlend | PF_Premultiplied_AlphaBlend))
			Options |= ShaderOptions::OPT_AlphaBlended;

		if (PolyFlags & PF_Translucent)
			Options |= ShaderOptions::OPT_Translucent;

		if (PolyFlags & PF_Environment)
			Options |= ShaderOptions::OPT_Environment;

		CachedPolyFlags = PolyFlags;
		CachedShaderOptions = Options;
	}
	else
	{
		// nothing changed
		Options = CachedShaderOptions;
	}

    return PolyFlags;
}

void UXOpenGLRenderDevice::SetBlend(DWORD PolyFlags)
{
	guard(UOpenGLRenderDevice::SetBlend);
	STAT(clockFast(Stats.BlendCycles));

	// For UED selection disable any blending.
	if (HitTesting())
	{
		glBlendFunc(GL_ONE, GL_ZERO);
		return;
	}

	// Check to disable culling or other frontface if needed (or more other affecting states yet). Perhaps should add own RenderFlags if so.
	DWORD Xor = CurrentBlendPolyFlags ^ PolyFlags;
	if (Xor & (PF_TwoSided | PF_RenderHint))
	{
#if ENGINE_VERSION==227
		if (PolyFlags & PF_TwoSided)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);
#endif
		CurrentBlendPolyFlags = PolyFlags;
	}

	// Detect changes in the blending modes.
	if (Xor & (PF_Translucent | PF_Modulated | PF_Invisible | PF_AlphaBlend | PF_Occlude | PF_Highlighted | PF_RenderFog))
	{
		if (Xor&(PF_Invisible | PF_Translucent | PF_Modulated | PF_AlphaBlend | PF_Highlighted))
		{
			if (!(PolyFlags & (PF_Invisible | PF_Translucent | PF_Modulated | PF_AlphaBlend | PF_Highlighted)))
			{
				//instead of forcing permanent state changes when disabling glBend use this as default state. Also fixes some flickering due to these changes.
				glBlendFunc(GL_ONE, GL_CONSTANT_COLOR);
				glBlendColor(0.0, 0.0, 0.0, 0.0);//actually add...nothing :)
			}
			else if (PolyFlags & PF_Invisible)
			{
				glBlendFunc(GL_ZERO, GL_ONE);
			}
			else if (PolyFlags & PF_Translucent)
			{
                if (SimulateMultiPass)//( !(PolyFlags & PF_Mirrored)
                {
                    //debugf(TEXT("PolyFlags %ls ActiveProgram %i"), *GetPolyFlagString(PolyFlags), ActiveProgram);
					glBlendFunc(GL_SRC_ALPHA, GL_SRC1_COLOR);
                }
                else glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
                /*
                else
                {
                    glBlendFunc( GL_ZERO, GL_SRC_COLOR ); //Mirrors!
                    //debugf(TEXT("Mirror"));
                }
                */
			}
			else if (PolyFlags & PF_Modulated)
			{
				glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			}
			else if (PolyFlags & PF_AlphaBlend)
			{
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			else if (PolyFlags & PF_Highlighted)
			{
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
		if (Xor & PF_Invisible)
		{
			if (PolyFlags & PF_Invisible)
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			else
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		if (Xor & PF_Occlude)
		{
			if (PolyFlags & PF_Occlude)
				glDepthMask(GL_TRUE);
			else
				glDepthMask(GL_FALSE);
		}
		CurrentBlendPolyFlags = PolyFlags;
	}
	STAT(unclockFast(Stats.BlendCycles));
	unguard;
}

BOOL UXOpenGLRenderDevice::WillBlendStateChange(DWORD OldPolyFlags, DWORD NewPolyFlags)
{
	// stijn: returns true if the polyflag switch will cause a change in the blending mode
	return ((OldPolyFlags ^ NewPolyFlags) & (PF_TwoSided | PF_RenderHint | PF_Translucent | PF_Modulated | PF_Invisible | PF_AlphaBlend | PF_Occlude | PF_Highlighted | PF_RenderFog | PF_Selected)) ? TRUE : FALSE;
}

constexpr GLenum ModeList[] = { GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_GEQUAL, GL_NOTEQUAL, GL_ALWAYS };
BYTE UXOpenGLRenderDevice::SetZTestMode(BYTE Mode)
{
	guard(UXOpenGLRenderDevice::SetZTestMode);
	if (LastZMode == Mode || Mode > 6)
		return Mode;

	// Flush any pending render.
	auto CurrentProgram = ActiveProgram;
	SetProgram(No_Prog);
	SetProgram(CurrentProgram);

	glDepthFunc(ModeList[Mode]);
	BYTE Prev = LastZMode;
	LastZMode = Mode;
	return Prev;
	unguard;
}

// SetBlend inspired approach to handle LineFlags.
DWORD UXOpenGLRenderDevice::SetDepth(DWORD LineFlags)
{
	guard(UXOpenGLRenderDevice::SetDepth);

	// Detect change in line flags.
	DWORD Xor = CurrentLineFlags^LineFlags;
	if (Xor & LINE_DepthCued)
	{
		if (LineFlags & LINE_DepthCued)
		{
			LastZMode = ZTEST_LessEqual;
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_TRUE);

			// Sync with SetBlend.
			CurrentBlendPolyFlags |= PF_Occlude;
		}
		else
		{
			LastZMode = ZTEST_Always;
			glDepthFunc(GL_ALWAYS);
			glDepthMask(GL_FALSE);

			// Sync with SetBlend.
			CurrentBlendPolyFlags &= ~PF_Occlude;
		}
		CurrentLineFlags = LineFlags;
	}
	return LineFlags;
	unguard;
}

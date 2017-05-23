/*=============================================================================
	SetTexture.cpp: Unreal XOpenGL Texture handling.

	Copyright 2014-2017 Oldunreal

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


void UXOpenGLRenderDevice::SetNoTexture( INT Multi )
{
	guard(UXOpenGLRenderDevice::SetNoTexture);
	if( TexInfo[Multi].CurrentCacheID != 0 )
	{
		glBindTexture( GL_TEXTURE_2D, 0 );
		TexInfo[Multi].CurrentCacheID = 0;
	}
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::SetSampler(GLuint Sampler, DWORD PolyFlags, UBOOL SkipMipmaps, BYTE UClampMode, BYTE VClampMode)
{
	guard(UOpenGLRenderDevice::SetSampler);
	CHECK_GL_ERROR();
	// Set texture sampler state.
	if (NoFiltering)
	{
		glSamplerParameteri(Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(Sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		CHECK_GL_ERROR();
	}
	else if (!(PolyFlags & PF_NoSmooth))
	{

		glSamplerParameteri(Sampler, GL_TEXTURE_MIN_FILTER, SkipMipmaps ? GL_LINEAR : (UseTrilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
		glSamplerParameteri(Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (MaxAnisotropy)
			glSamplerParameterf(Sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, MaxAnisotropy);

		if (LODBias)
			glSamplerParameteri(Sampler, GL_TEXTURE_LOD_BIAS_EXT, LODBias);
		CHECK_GL_ERROR();

	}
	else
	{
		glSamplerParameteri(Sampler, GL_TEXTURE_MIN_FILTER, SkipMipmaps ? GL_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
		glSamplerParameteri(Sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		CHECK_GL_ERROR();
	}

	// TODO: check, just stumbled across it.
	if (UClampMode)
	{
		glSamplerParameteri(Sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		CHECK_GL_ERROR();
	}
	if (VClampMode)
	{
		glSamplerParameteri(Sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		CHECK_GL_ERROR();
	}
	CHECK_GL_ERROR();
	unguard;
}

void UXOpenGLRenderDevice::SetTexture( INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, INT ShaderProg )
{
	guard(UXOpenGLRenderDevice::SetTexture);

	// Set panning.
	FTexInfo& Tex = TexInfo[Multi];
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

	// Determine slot to work around odd masked handling.
	//INT CacheSlot = ((PolyFlags & PF_Masked) && ((Info.Format==TEXF_P8 && P8Mode==P8_Special ) || (Info.Format==TEXF_DXT1 && DXT1Mode==DXT1_Special))) ? 1 : 0;
	INT CacheSlot = ((PolyFlags & PF_Masked) && (Info.Format==TEXF_P8)) ? 1 : 0;

	// Find in cache.
	if( !Info.bRealtimeChanged && Info.CacheID==Tex.CurrentCacheID && CacheSlot==Tex.CurrentCacheSlot )
		return;

	// Make current.
	STAT(clock(Stats.BindCycles));
	Tex.CurrentCacheSlot = CacheSlot;
	Tex.CurrentCacheID   = Info.CacheID;

	//debugf( TEXT("Tex.CurrentCacheID 0x%08x%08x, Multi %i" ), Tex.CurrentCacheID, Multi );
	FCachedTexture *Bind=BindMap->Find(Info.CacheID);

	// Whether we can handle this texture format/size.
	bool Unsupported = false;
	bool ExistingBind = false;

	bool SkipMipmaps = (!GenerateMipMaps && Info.NumMips == 1 && !AlwaysMipmap);

	if ( !Bind )
	{
		// Figure out OpenGL-related scaling for the texture.
		Bind           = &BindMap->Set( Info.CacheID, FCachedTexture() );
		Bind->Ids[0]   = 0;
		Bind->Ids[1]   = 0;
		Bind->BaseMip  = 0;
		Bind->MaxLevel = 0;
		Bind->Sampler  = 0;

		// Find lowest mip level support.
		while ( Bind->BaseMip<Info.NumMips && Max(Info.Mips[Bind->BaseMip]->USize,Info.Mips[Bind->BaseMip]->VSize)>MaxTextureSize )
		{
			Bind->BaseMip++;
		}
		if ( Bind->BaseMip>=Info.NumMips )
		{
			GWarn->Logf( TEXT("Encountered oversize texture %s without sufficient mipmaps."), Info.Texture->GetPathName() );
			Unsupported = 1;
		}
	}
	else  ExistingBind = true;

	if ( Bind->Ids[CacheSlot]==0 )
	{
		glGenTextures( 1, &Bind->Ids[CacheSlot] );

		if(!Bind->Sampler)
			glGenSamplers(1, &Bind->Sampler);

		CHECK_GL_ERROR();
		#if ENGINE_VERSION==227
			SetSampler(Bind->Sampler, PolyFlags, SkipMipmaps, Info.UClampMode, Info.VClampMode);
		#else
			SetSampler(Bind->Sampler, PolyFlags, SkipMipmaps, 0, 0);
		#endif

		// Spew warning if we uploaded this texture twice.
		if ( ExistingBind )
			debugf( NAME_Warning, TEXT("Unpacking texture %s a second time as %s."), Info.Texture->GetFullName(), CacheSlot ? TEXT("masked") : TEXT("unmasked") );

		ExistingBind = false;
	}

	switch (ShaderProg)
	{
		case Simple_Prog:
		{
			break;
		}
		case Tile_Prog:
		{
			glUniform1i(DrawTileTexture, Multi);
			CHECK_GL_ERROR();
			break;
		}
		case GouraudPolyVert_Prog:
		{
			glUniform1i(DrawGouraudTexture[Multi], Multi);
			CHECK_GL_ERROR();
			break;
		}
		case GouraudPolyVertList_Prog:
		{
			glUniform1i(DrawGouraudTexture[Multi], Multi);
			CHECK_GL_ERROR();
			break;
		}
		case ComplexSurfaceSinglePass_Prog:
		{
			glUniform1i(DrawComplexSinglePassTexture[Multi], Multi);
			CHECK_GL_ERROR();
			break;
		}
		default:
		{
			break;
		}
	}

	CHECK_GL_ERROR();
	glActiveTexture(GL_TEXTURE0 + Multi);
	CHECK_GL_ERROR();
	glBindTexture( GL_TEXTURE_2D, Bind->Ids[CacheSlot] );
	CHECK_GL_ERROR();
	glBindSampler(Multi, Bind->Sampler);
	CHECK_GL_ERROR();

	STAT(unclock(Stats.BindCycles));

	// Account for all the impact on scale normalization.
	Tex.UMult = 1.0/(Info.UScale*Info.USize);
	Tex.VMult = 1.0/(Info.VScale*Info.VSize);

	// Upload if needed.
	STAT(clock(Stats.ImageCycles));
	if( !ExistingBind || Info.bRealtimeChanged )
	{
		// Some debug output.
		//if( Info.Palette && Info.Palette[128].A!=255 && !(PolyFlags&PF_Translucent) )
		//	debugf( TEXT("Would set PF_Highlighted for %s."), Info.Texture->GetFullName() );

		// Cleanup texture flags.
		if ( SupportsLazyTextures )
			Info.Load();
		Info.bRealtimeChanged = 0;

#if ENGINE_VERSION==1100
		static FName UserInterface = FName( TEXT("UserInterface"), FNAME_Intrinsic );
#endif

		UBOOL UnpackSRGB = UseSRGBTextures && !(PolyFlags & PF_Modulated)
#if ENGINE_VERSION==1100 // Hack for DeusExUI.UserInterface.
			&& !(Info.Texture && Info.Texture->GetOuter() && Info.Texture->GetOuter()->GetFName()==UserInterface)
#endif
		;

		// Generate the palette.
		FColor  LocalPal[256];
		FColor* Palette = Info.Palette;// ? Info.Palette : LocalPal; // Save fallback for malformed P8.
		if( Info.Format==TEXF_P8 )
		{
			if ( !Info.Palette )
				appErrorf( TEXT("Encountered bogus P8 texture %s"), Info.Texture->GetFullName() );

			if ( PolyFlags & PF_Masked )
			{
				// kaufel: could have kept the hack to modify and reset Info.Palette[0], but opted against.
				appMemcpy( LocalPal, Info.Palette, 256*sizeof(FColor) );
				LocalPal[0] = FColor(0,0,0,0);
				Palette = LocalPal;
			}
		}

		// Download the texture.
		UBOOL NoAlpha = 0; // Temp.

		// If !0 allocates the requested amount of memory (and frees it afterwards).
		DWORD MinComposeSize = 0;

		GLuint InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
		GLuint SourceFormat   = GL_RGBA;

		// Unsupported can already be set in case of only too large mip maps available.
		if ( !Unsupported )
		{
			switch ( (BYTE)Info.Format ) //!!
			{
				// P8 -- Default palettized texture format.
				case TEXF_P8:
					MinComposeSize = Info.Mips[Bind->BaseMip]->USize*Info.Mips[Bind->BaseMip]->VSize*4;
					InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
					SourceFormat   = GL_RGBA;
					break;

				// RGBA7 -- Well it's actually BGRA and used by light and fogmaps.
				case TEXF_RGBA7:
					MinComposeSize = Info.Mips[Bind->BaseMip]->USize*Info.Mips[Bind->BaseMip]->VSize*4;
					InternalFormat = GL_RGBA8;
					if (OpenGLVersion == GL_Core)
                        SourceFormat   = GL_BGRA; // Was GL_RGBA;
					else
                        SourceFormat   = GL_RGBA; // ES prefers RGBA...

					break;

				// RGB8/RGBA8 -- Actually used by Brother Bear.
				case TEXF_RGB8:
					InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
					SourceFormat   = GL_RGB;
					break;

				case TEXF_RGBA8:
					InternalFormat = UnpackSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
					SourceFormat   = GL_RGBA;
					break;

				// S3TC -- Ubiquitous Extension.
				case TEXF_DXT1:
					if ( Info.Mips[Bind->BaseMip]->USize<4 || Info.Mips[Bind->BaseMip]->VSize<4 )
					{
						GWarn->Logf( TEXT("Undersized TEXF_DXT1 (USize=%i,VSize=%i)"), Info.Mips[Bind->BaseMip]->USize, Info.Mips[Bind->BaseMip]->VSize );
						Unsupported = 1;
						break;
					}
					NoAlpha = CacheSlot;
					if ( SUPPORTS_GL_EXT_texture_compression_dxt1 )
					{
						if ( UnpackSRGB )
						{
							if ( SUPPORTS_GL_EXT_texture_sRGB )
							{
								InternalFormat = NoAlpha ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
								break;
							}
							if ( NoAlpha )
								GWarn->Logf( TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGB_S3TC_DXT1_EXT as fallback for %s."), Info.Texture->GetPathName() );
							else
								GWarn->Logf( TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGBA_S3TC_DXT1_EXT as fallback for %s."), Info.Texture->GetPathName() );
						}
						InternalFormat = NoAlpha ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
						break;
					}
					GWarn->Logf( TEXT("GL_EXT_texture_compression_s3tc not supported on texture %s."), Info.Texture->GetPathName() );
					Unsupported = 1;
					break;

#if ENGINE_VERSION==227
				case TEXF_DXT3:
					if ( SUPPORTS_GL_EXT_texture_compression_dxt3 )
					{
						if (UnpackSRGB)
						{
							if (SUPPORTS_GL_EXT_texture_sRGB)
							{
								InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
								break;
							}
							GWarn->Logf(TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGBA_S3TC_DXT3_EXT as fallback for %s."), Info.Texture->GetPathName());
						}
						InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
						break;
					}
					GWarn->Logf( TEXT("GL_EXT_texture_compression_s3tc not supported on texture %s."), Info.Texture->GetPathName() );
					Unsupported = 1;
					break;

				case TEXF_DXT5:
					if ( SUPPORTS_GL_EXT_texture_compression_dxt5 )
					{
						if (UnpackSRGB)
						{
							if (SUPPORTS_GL_EXT_texture_sRGB)
							{
								InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
								break;
							}
							debugf(NAME_Warning, TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGBA_S3TC_DXT5_EXT as fallback for %s."), Info.Texture->GetPathName());
						}
						InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
						break;
					}
					GWarn->Logf( TEXT("GL_EXT_texture_compression_s3tc not supported on texture %s."), Info.Texture->GetPathName() );
					Unsupported = 1;
					break;

				// RGTC -- Core since OpenGL 3.0. Also available on Direct3D 10. Not in GLES it seems.
				case TEXF_RGTC_R:
					InternalFormat = GL_COMPRESSED_RED_RGTC1;
					break;
				case TEXF_RGTC_R_SIGNED:
					InternalFormat = GL_COMPRESSED_SIGNED_RED_RGTC1;
					break;
				case TEXF_RGTC_RG:
					InternalFormat = GL_COMPRESSED_RG_RGTC2;
					break;
				case TEXF_RGTC_RG_SIGNED:
					InternalFormat = GL_COMPRESSED_SIGNED_RG_RGTC2;
					break;

				// ETC2 and EAC -- Core since OpenGL 4.3.
				case TEXF_ETC2_RGB8:
					InternalFormat = GL_COMPRESSED_RGB8_ETC2;
					break;
				case TEXF_ETC2_SRGB8:
					InternalFormat = GL_COMPRESSED_SRGB8_ETC2;
					break;
				case TEXF_ETC2_RGB8_PA1:
					InternalFormat = GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
					break;
				case TEXF_ETC2_SRGB8_PA1:
					InternalFormat = GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
					break;
				case TEXF_ETC2_RGB8_EAC_A8:
					InternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
					break;
				case TEXF_ETC2_SRGB8_EAC_A8:
					InternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
					break;
				case TEXF_EAC_R11:
					InternalFormat = GL_COMPRESSED_R11_EAC;
					break;
				case TEXF_EAC_R11_SIGNED:
					InternalFormat = GL_COMPRESSED_SIGNED_R11_EAC;
					break;
				case TEXF_EAC_RG11:
					InternalFormat = GL_COMPRESSED_RG11_EAC;
					break;
				case TEXF_EAC_RG11_SIGNED:
					InternalFormat = GL_COMPRESSED_SIGNED_RG11_EAC;
					break;
#endif
				// Default: Mark as unsupported.
				default:
					GWarn->Logf( TEXT("Unknown texture format %i on texture %s."), Info.Format, Info.Texture->GetPathName() );
					Unsupported = 1;
					break;
			}
		}

		// If not supported make sure we have enough compose mem for a fallback texture.
		if ( Unsupported )
		{
			// 256x256 RGBA texture.
			MinComposeSize = 256*256*4;
		}

		// Allocate or enlarge compose memory if needed.
		guard(AllocateCompose);
		if ( MinComposeSize>ComposeSize )
		{
			Compose = (BYTE*)appRealloc( Compose, MinComposeSize, TEXT("Compose") );
			if ( !Compose )
				appErrorf( TEXT("Failed to allocate memory for texture compose.") );
			ComposeSize = MinComposeSize;
		}
		unguard;

		// Unpack texture data.
		INT MaxLevel = -1;
		if ( !Unsupported )
		{
			for ( INT MipIndex=Bind->BaseMip; MipIndex<Info.NumMips; MipIndex++ )
			{
				// Convert the mipmap.
				FMipmapBase* Mip            = Info.Mips[MipIndex];
				BYTE* 	     UnCompSrc      = Mip->DataPtr;
				BYTE*        CompSrc        = Mip->DataPtr;
				GLsizei      CompImageSize  = 0; // !0 also enables use of glCompressedTex[Sub]Image2D.
				GLsizei      USize          = Mip->USize;
				GLsizei      VSize          = Mip->VSize;

				if ( Mip->DataPtr )
				{
					switch ( (BYTE)Info.Format ) //!!
					{
						// P8 -- Default palettized texture format.
						case TEXF_P8:
							guard(ConvertP8_RGBA8888);
							UnCompSrc  = Compose;
							DWORD* Ptr = (DWORD*)Compose;
							INT Count  = USize*VSize;
							for ( INT i=0; i<Count; i++ )
								*Ptr++ = GET_COLOR_DWORD(Palette[Mip->DataPtr[i]]);
							unguard;
							break;

						// RGBA7 -- Well it's actually BGRA and used by light and fogmaps.
						case TEXF_RGBA7:
							guard(ConvertBGRA7777_RGBA8888);
							UnCompSrc  = Compose;
							DWORD* Ptr = (DWORD*)Compose;
							INT ULimit = Min(USize,Info.UClamp); // Implicit assumes NumMips==1.
							INT VLimit = Min(VSize,Info.VClamp);

							// Do resampling. The area outside of the clamp gets filled up with the last valid values
							// to emulate GL_CLAMP_TO_EDGE behaviour. This is done to avoid using NPOT textures.
							for ( INT v=0; v<VLimit; v++ )
							{
								if (OpenGLVersion == GL_Core)
								{
									// The AND 0x7F7F7F7F is used to eliminate the top bit as this was used internally by the LightManager.
									for ( INT u=0; u<ULimit; u++ )
										*Ptr++ = (GET_COLOR_DWORD(Mip->DataPtr[(u+v*USize)<<2])&0x7F7F7F7F)<<1; // We can skip this shift. Unless we want to unpack as sRGB.
								}
								else  // GLES needs to swizzle from BGRA to RGBA.
								{
									for ( INT u=0; u<ULimit; u++ )
									{
										union { DWORD dw; BYTE b[4]; } rgba;
										// The AND 0x7F7F7F7F is used to eliminate the top bit as this was used internally by the LightManager.
										rgba.dw = (GET_COLOR_DWORD(Mip->DataPtr[(u+v*USize)<<2])&0x7F7F7F7F)<<1; // We can skip this shift. Unless we want to unpack as sRGB.
										const BYTE tmp = rgba.b[0];
										rgba.b[0] = rgba.b[2];
										rgba.b[2] = tmp;
										*Ptr++ = rgba.dw;
									}
								}

								if ( ULimit>0 )
								{
									// Copy last valid value until end of line.
									DWORD LastValidValue = Ptr[-1];
									for ( INT ux=ULimit; ux<USize; ux++ )
										*Ptr++ = LastValidValue;
								}
								else
								{
									// If we had no valid value just memzero.
									appMemzero( Ptr, USize*sizeof(DWORD) );
									Ptr += USize;
								}
							}
							if ( VLimit>0 )
							{
								// Copy last valid line until reaching the bottom.
								BYTE* LastValidLine = Compose + (VLimit-1)*USize*sizeof(DWORD); // Compose is BYTE*.
								for ( INT vx=VLimit; vx<VSize; vx++ )
									appMemcpy( Compose + vx*USize*sizeof(DWORD), LastValidLine, USize*sizeof(DWORD) );
							}
							else // If we had no valid lines just memzero.
								appMemzero( Compose, USize*VSize*sizeof(DWORD) );

							// What about mipmaps, or a higher order reconstruction filter in shader? -- Quality improved significant in singlepass, so not needed.
							unguard;
							break;

						// RGB8/RGBA8 -- Actually used by Brother Bear.
						case TEXF_RGB8:
						case TEXF_RGBA8:
							UnCompSrc = (BYTE*)Mip->DataPtr;
							break;

						// S3TC -- Ubiquitous Extension.
						case TEXF_DXT1:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize = USize*VSize/2;
							break;
#if ENGINE_VERSION==227
						case TEXF_DXT3:
						case TEXF_DXT5:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize = USize*VSize;
							break;

						// RGTC -- Core since OpenGL 3.0. Also available on Direct3D 10.
						case TEXF_RGTC_R:
						case TEXF_RGTC_R_SIGNED:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize  = USize*VSize/2;
							break;
						case TEXF_RGTC_RG:
						case TEXF_RGTC_RG_SIGNED:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize = USize*VSize;
							break;

						// ETC2 and EAC -- Core since OpenGL 4.3.
						case TEXF_ETC2_RGB8:
						case TEXF_ETC2_SRGB8:
						case TEXF_ETC2_RGB8_PA1:
						case TEXF_ETC2_SRGB8_PA1:
						case TEXF_EAC_R11:
						case TEXF_EAC_R11_SIGNED:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize  = USize*VSize/2;
							break;
						case TEXF_ETC2_RGB8_EAC_A8:
						case TEXF_ETC2_SRGB8_EAC_A8:
						case TEXF_EAC_RG11:
						case TEXF_EAC_RG11_SIGNED:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize  = USize*VSize;
							break;
#endif
						// Should not happen (TM).
						default:
							appErrorf( TEXT("Unpacking unknown format %i on %s."), Info.Format, Info.Texture );
							break;
					}
				}

				// Upload texture.
				if ( ExistingBind )
				{
					if ( CompImageSize )
					{
						guard(glCompressedTexSubImage2D);
						glCompressedTexSubImage2D( GL_TEXTURE_2D, ++MaxLevel, 0, 0, USize, VSize, InternalFormat, CompImageSize, CompSrc );
						unguard;
					}
					else
					{
						guard(glTexSubImage2D);
						glTexSubImage2D( GL_TEXTURE_2D, ++MaxLevel, 0, 0, USize, VSize, SourceFormat, GL_UNSIGNED_BYTE, UnCompSrc );
						unguard;
					}
				}
				else
				{
					if ( CompImageSize )
					{
						guard(glCompressedTexImage2D);
						if (GenerateMipMaps)
						{
							glTexStorage2D(GL_TEXTURE_2D, MAX_MIPS, SourceFormat, USize, VSize);
							glCompressedTexImage2D( GL_TEXTURE_2D, 0, InternalFormat, USize, VSize, 0, CompImageSize, CompSrc );
							glGenerateMipmap(GL_TEXTURE_2D);
						}
						else
							glCompressedTexImage2D(GL_TEXTURE_2D, ++MaxLevel, InternalFormat, USize, VSize, 0, CompImageSize, CompSrc);
						unguard;
					}
					else
					{
						guard(glTexImage2D);
						if (GenerateMipMaps)
						{
							glTexStorage2D(GL_TEXTURE_2D, MAX_MIPS, SourceFormat, USize, VSize);
							glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, USize, VSize, 0, SourceFormat, GL_UNSIGNED_BYTE, UnCompSrc);
							glGenerateMipmap(GL_TEXTURE_2D);  //Generate num_mipmaps number of mipmaps here.
						}
						else glTexImage2D(GL_TEXTURE_2D, ++MaxLevel, InternalFormat, USize, VSize, 0, SourceFormat, GL_UNSIGNED_BYTE, UnCompSrc);
						unguard;
					}
				}
				CHECK_GL_ERROR();
				if (GenerateMipMaps)
					break;
			}

			// DXT1 textures contain mip maps until 1x1.
			FinishedUnpack:

			// This should not happen. If it happens, a sanity check is missing above.
			if (!GenerateMipMaps && MaxLevel == -1)
				GWarn->Logf( TEXT("No mip map unpacked for texture %s."), Info.Texture->GetPathName() );
		}

		// Create and unpack a chequerboard fallback texture texture for an unsupported format.
		guard(Unsupported);
		if ( Unsupported )
		{
			check(Compose);
			check(ComposeSize>=64*64*4);
			DWORD Palette[16] =
			{
				0x00000000u, 0x000000FFu, 0x0000FF00u, 0x0000FFFFu,
				0x00FF0000u, 0x00FF00FFu, 0x00FFFF00u, 0x00FFFFFFu,
				0xFF000000u, 0xFF0000FFu, 0xFF00FF00u, 0xFF00FFFFu,
				0xFFFF0000u, 0xFFFF00FFu, 0xFFFFFF00u, 0xFFFFFFFFu,
			};
			MaxLevel = 0;
			DWORD* Ptr = (DWORD*)Compose;
			for ( INT i=0;i <(256*256); i++ )
				*Ptr++ = Palette[(i/16+i/(256*16))%16]; //

			guard(glTexImage2D);
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, Compose );
			unguard;
		}
		unguard;

		// Set max level.
		if ( !ExistingBind || Bind->MaxLevel!=MaxLevel )
		{
			Bind->MaxLevel = MaxLevel;
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MaxLevel );
		}

		// Cleanup.
		if ( SupportsLazyTextures )
			Info.Unload();
	}
	STAT(unclock(Stats.ImageCycles));
	unguard;
}

DWORD UXOpenGLRenderDevice::SetBlend(DWORD PolyFlags, INT ShaderProg)
{
	guard(UOpenGLRenderDevice::SetBlend);
	STAT(clock(Stats.BlendCycles));

	if( (PolyFlags & (PF_RenderFog|PF_Translucent))!=PF_RenderFog )
		PolyFlags &= ~PF_RenderFog;

	if (!(PolyFlags & (PF_Translucent | PF_Modulated | PF_AlphaBlend)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & (PF_Translucent | PF_AlphaBlend))
		PolyFlags &= ~PF_Masked;

	// For UED selection disable any blending.
	if (HitTesting())
	{
		glBlendFunc(GL_ONE, GL_ZERO);
		return 0;
	}

	// Check to disable culling or other frontface if needed (or more other affecting states yet). Perhaps should add own RenderFlags if so.
	DWORD Xor = CurrentAdditionalPolyFlags^PolyFlags;
	if (Xor & (PF_TwoSided | PF_RenderHint))
	{
		#if ENGINE_VERSION==227
		if (PolyFlags & PF_TwoSided)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);

		if (PolyFlags & PF_RenderHint) // check for bInverseOrder order. Only in use in SoftDrv for DrawTile, so there should be no trouble.
			glFrontFace(GL_CCW); //rather expensive switch better try to avoid!
		else
			glFrontFace(GL_CW);
		#endif

		if (ActiveProgram==GouraudPolyVert_Prog)
		{
			FPlane DrawColor = FPlane(0.f, 0.f, 0.f, 0.f);
			if (DrawGouraudBufferData.VertSize > 0)
                DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData, DrawColor);
		}

		CurrentAdditionalPolyFlags=PolyFlags;
	}

	Xor = CurrentPolyFlags^PolyFlags;
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
				CHECK_GL_ERROR();
			}
			else if (PolyFlags & PF_Invisible)
			{
				glBlendFunc(GL_ZERO, GL_ONE);
				CHECK_GL_ERROR();
			}
			else if (PolyFlags & PF_Translucent)
			{
				if (0)//ShaderProg == ComplexSurfaceSinglePass_Prog)
				{
					glBlendFunc(GL_ONE, GL_SRC1_COLOR);
					glBlendEquation(GL_FUNC_ADD);
					//glBlendFunc(GL_DST_COLOR, GL_SRC1_COLOR);
				}
				else
				{
					glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
				}
				CHECK_GL_ERROR();
			}
			else if (PolyFlags & PF_Modulated)
			{
				glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				CHECK_GL_ERROR();
			}
			else if (PolyFlags & PF_AlphaBlend)
			{
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				CHECK_GL_ERROR();
			}
			else if (PolyFlags & PF_Highlighted)
			{
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				CHECK_GL_ERROR();
			}
		}
		if (Xor & PF_Invisible)
		{
			if (PolyFlags & PF_Invisible)
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			else
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			CHECK_GL_ERROR();
		}
		if (Xor & PF_Occlude)
		{
			if (PolyFlags & PF_Occlude)
				glDepthMask(GL_TRUE);
			else
				glDepthMask(GL_FALSE);
			CHECK_GL_ERROR();
		}
		CurrentPolyFlags = PolyFlags;
	}
	STAT(unclock(Stats.BlendCycles));

	return PolyFlags;

	CHECK_GL_ERROR();
	unguard;
}
void UXOpenGLRenderDevice::SetFlatState(bool bEnable)
{
	guard(UOpenGLRenderDevice::SetFlatState);
	if (bEnable)
		glDepthFunc(GL_EQUAL);
	else glDepthFunc(GL_LEQUAL);
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
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_TRUE);

			// Sync with SetBlend.
			CurrentPolyFlags |= PF_Occlude;
		}
		else
		{
			glDepthFunc(GL_ALWAYS);
			glDepthMask(GL_FALSE);

			// Sync with SetBlend.
			CurrentPolyFlags &= ~PF_Occlude;
		}
		CurrentLineFlags = LineFlags;
	}
	return LineFlags;
	unguard;
}

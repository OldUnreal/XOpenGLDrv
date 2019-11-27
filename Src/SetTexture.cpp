/*=============================================================================
	SetTexture.cpp: Unreal XOpenGL Texture handling.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

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
        return;

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

void UXOpenGLRenderDevice::SetTexture( INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias, INT ShaderProg, TexType TextureType )
{
	guard(UXOpenGLRenderDevice::SetTexture);

	if (ActiveProgram <= No_Prog)
        return;

	// Set panning.
	FTexInfo& Tex = TexInfo[Multi];
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

    // Account for all the impact on scale normalization.
	Tex.UMult = 1.0/(Info.UScale*Info.USize);
	Tex.VMult = 1.0/(Info.VScale*Info.VSize);

    bool bBindlessRealtimeChanged = false;

	// Determine slot to work around odd masked handling.
	INT CacheSlot = ((PolyFlags & PF_Masked) && (Info.Format==TEXF_P8)) ? 1 : 0;

	STAT(clockFast(Stats.BindCycles));

	FCachedTexture *Bind = NULL;
#if ENGINE_VERSION==227 //|| UNREAL_TOURNAMENT_UTPG
    // avoid lookups by storing information directly into texture. Add bindless information if available.
    // Should save quite some CPU.
    // To make use of this, add "void* TextureHandle" into class ENGINE_API UTexture : public UBitmap
	if (UseBindlessTextures && Info.Texture && Info.Texture->TextureHandle)
    {
        FCachedTexture* FCachedTextureInfo = (FCachedTexture*)Info.Texture->TextureHandle;
        if( FCachedTextureInfo && FCachedTextureInfo->TexNum[CacheSlot])
        {
            if (Info.bRealtimeChanged) //update bindless realtime textures.
            {
                //debugf(TEXT("bRealtimeChanged: %ls"),Info.Texture->GetFullName());
                bBindlessRealtimeChanged = true;
            }
            else
            {
                Tex.TexNum = FCachedTextureInfo->TexNum[CacheSlot];
                //debugf(TEXT("Unchanged: %ls %i 0x%08x%08x"),Info.Texture->GetFullName(),Tex.TexNum, FCachedTextureInfo->TexHandle[CacheSlot]);
                STAT(unclockFast(Stats.BindCycles));
                return;
            }
            Bind = FCachedTextureInfo;
        }
    }
#endif

	if( !Info.bRealtimeChanged && Info.CacheID==Tex.CurrentCacheID && CacheSlot==Tex.CurrentCacheSlot )
    {
        STAT(unclockFast(Stats.BindCycles));
        return;
    }

    // Make current.
	Tex.CurrentCacheSlot = CacheSlot;
	Tex.CurrentCacheID   = Info.CacheID;

	//debugf( TEXT("Tex.CurrentCacheID 0x%08x%08x, Multi %i" ), Tex.CurrentCacheID, Multi );

	// Find in cache.
	if (!Bind)
        Bind=BindMap->Find(Info.CacheID);

	// Whether we can handle this texture format/size.
	bool Unsupported = false;
	bool ExistingBind = false;

	bool SkipMipmaps = (!GenerateMipMaps && Info.NumMips == 1 && !AlwaysMipmap);

	if ( !Bind )
	{
		// Figure out OpenGL-related scaling for the texture.
		Bind                = &BindMap->Set( Info.CacheID, FCachedTexture() );
		Bind->Ids[0]        = 0;
		Bind->Ids[1]        = 0;
		Bind->BaseMip       = 0;
		Bind->MaxLevel      = 0;
		Bind->Sampler[0]    = 0;
		Bind->Sampler[1]	= 0;
		Bind->TexHandle[0]  = 0;
		Bind->TexHandle[1]  = 0;
		Bind->TexNum[0]     = 0;
		Bind->TexNum[1]     = 0;

		if (Info.NumMips && !Info.Mips[0])
		{
			GWarn->Logf(TEXT("Encountered texture %ls with invalid MipMaps!"), Info.Texture->GetPathName());
			Info.NumMips = 0;
			Unsupported = 1;
		}
		else
		{
			// Find lowest mip level support.
			while ( Bind->BaseMip<Info.NumMips && Max(Info.Mips[Bind->BaseMip]->USize,Info.Mips[Bind->BaseMip]->VSize)>MaxTextureSize )
			{
				Bind->BaseMip++;
			}

			if ( Bind->BaseMip>=Info.NumMips )
			{
				GWarn->Logf( TEXT("Encountered oversize texture %ls without sufficient mipmaps."), Info.Texture->GetPathName() );
				Unsupported = 1;
			}
		}
	}
	else ExistingBind = true;

	if ( Bind->Ids[CacheSlot]==0 )
	{
		glGenTextures( 1, &Bind->Ids[CacheSlot] );

		if (!Bind->Sampler[CacheSlot])
			glGenSamplers(1, &Bind->Sampler[CacheSlot]);

		CHECK_GL_ERROR();
		#if ENGINE_VERSION==227
		SetSampler(Bind->Sampler[CacheSlot], PolyFlags, SkipMipmaps, Info.UClampMode, Info.VClampMode);
		#else
		SetSampler(Bind->Sampler[CacheSlot], PolyFlags, SkipMipmaps, 0, 0);
		#endif

		// Spew warning if we uploaded this texture twice.
		if ( ExistingBind )
			debugf( NAME_Warning, TEXT("Unpacking texture %ls a second time as %ls."), Info.Texture->GetFullName(), CacheSlot ? TEXT("masked") : TEXT("unmasked") );

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
	if (!bBindlessRealtimeChanged)
    {
        CHECK_GL_ERROR();
        glActiveTexture(GL_TEXTURE0 + Multi);
        CHECK_GL_ERROR();
        glBindTexture( GL_TEXTURE_2D, Bind->Ids[CacheSlot] );
        CHECK_GL_ERROR();
		glBindSampler(Multi, Bind->Sampler[CacheSlot]);
        CHECK_GL_ERROR();
    }

	STAT(unclockFast(Stats.BindCycles));

	// Upload if needed.
	STAT(clockFast(Stats.ImageCycles));
	if( !ExistingBind || Info.bRealtimeChanged )
	{
		// Some debug output.
		//if( Info.Palette && Info.Palette[128].A!=255 && !(PolyFlags&PF_Translucent) )
		//	debugf( TEXT("Would set PF_Highlighted for %ls."), Info.Texture->GetFullName() );

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
				appErrorf( TEXT("Encountered bogus P8 texture %ls"), Info.Texture->GetFullName() );

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
		GLuint SourceType = GL_UNSIGNED_BYTE;

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
#if ENGINE_VERSION==227
    #ifndef __LINUX_ARM__
				case TEXF_RGBA16:
					InternalFormat = GL_RGBA16;
					SourceFormat = GL_RGBA;
					SourceType = GL_UNSIGNED_SHORT;
					break;
    #endif
#endif
				// S3TC -- Ubiquitous Extension.
				case TEXF_DXT1:
					if ( Info.Mips[Bind->BaseMip]->USize<4 || Info.Mips[Bind->BaseMip]->VSize<4 )
					{
						GWarn->Logf( TEXT("Undersized TEXF_DXT1 (USize=%i,VSize=%i)"), Info.Mips[Bind->BaseMip]->USize, Info.Mips[Bind->BaseMip]->VSize );
						Unsupported = 1;
						break;
					}
					NoAlpha = CacheSlot;
                    if (OpenGLVersion == GL_Core)
                    {
                        if ( GL_EXT_texture_compression_s3tc )
                        {
                            if ( UnpackSRGB )
                            {
                                if ( GL_EXT_texture_sRGB )
                                {
                                    InternalFormat = NoAlpha ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
                                    break;
                                }
                                if ( NoAlpha )
                                    GWarn->Logf( TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGB_S3TC_DXT1_EXT as fallback for %ls."), Info.Texture->GetPathName() );
                                else
                                    GWarn->Logf( TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGBA_S3TC_DXT1_EXT as fallback for %ls."), Info.Texture->GetPathName() );
                            }
                            InternalFormat = NoAlpha ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                            break;
                        }
                    }
                    else
                    {
                        if ( GL_EXT_texture_compression_dxt1 )
                        {
                            InternalFormat = NoAlpha ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                            break;
                        }
                    }
					GWarn->Logf( TEXT("GL_EXT_texture_compression_s3tc not supported on texture %ls."), Info.Texture->GetPathName() );
					Unsupported = 1;
					break;
#if ENGINE_VERSION==227
				case TEXF_DXT3:
                    if (OpenGLVersion == GL_Core)
                    {
                        if ( GL_EXT_texture_compression_s3tc )
                        {
                            if (UnpackSRGB)
                            {
                                if (GL_EXT_texture_sRGB)
                                {
                                    InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
                                    break;
                                }
                                GWarn->Logf(TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGBA_S3TC_DXT3_EXT as fallback for %ls."), Info.Texture->GetPathName());
                            }
                            InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                            break;
                        }
                    }
                    else
                    {
                        if(GL_ANGLE_texture_compression_dxt3)
                        {
                            InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE;
                            break;
                        }
                    }
					GWarn->Logf( TEXT("GL_EXT_texture_compression_s3tc not supported on texture %ls."), Info.Texture->GetPathName() );
					Unsupported = 1;
					break;
				case TEXF_DXT5:
                    if (OpenGLVersion == GL_Core)
                    {
                        if ( GL_EXT_texture_compression_s3tc )
                        {
                            if (UnpackSRGB)
                            {
                                if (GL_EXT_texture_sRGB)
                                {
                                    InternalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
                                    break;
                                }
                                debugf(NAME_Warning, TEXT("GL_EXT_texture_sRGB not supported, using GL_COMPRESSED_RGBA_S3TC_DXT5_EXT as fallback for %ls."), Info.Texture->GetPathName());
                            }
                            InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                            break;
                        }
                    }
                    else
                    {
                        if(GL_ANGLE_texture_compression_dxt5)
                        {
                            InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE;
                            break;
                        }
                    }
					GWarn->Logf( TEXT("GL_EXT_texture_compression_s3tc not supported on texture %ls."), Info.Texture->GetPathName() );
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
				// BPTC Core since 4.2. BC6H and BC7 in D3D11.
#ifndef __LINUX_ARM__
				case TEXF_BPTC_RGB_SF:
					InternalFormat = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT; //BC6H
					break;
				case TEXF_BPTC_RGB_UF:
					InternalFormat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT; //BC6H
					break;
				case TEXF_BPTC_RGBA:
					InternalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM; //BC7
						break;
#endif

#endif
				// Default: Mark as unsupported.
				default:
					GWarn->Logf( TEXT("Unknown texture format %i on texture %ls."), Info.Format, Info.Texture->GetPathName() );
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
				BYTE* 	     ImgSrc			= NULL;
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
							ImgSrc  = Compose;
							DWORD* Ptr = (DWORD*)Compose;
							INT Count  = USize*VSize;
							for ( INT i=0; i<Count; i++ )
								*Ptr++ = GET_COLOR_DWORD(Palette[Mip->DataPtr[i]]);
							unguard;
							break;

						// RGBA7 -- Well it's actually BGRA and used by light and fogmaps.
						case TEXF_RGBA7:
							guard(ConvertBGRA7777_RGBA8888);
							ImgSrc  = Compose;
							DWORD* Ptr = (DWORD*)Compose;
							INT ULimit = Min(USize,Info.UClamp); // Implicit assumes NumMips==1.
							INT VLimit = Min(VSize,Info.VClamp);

							// Do resampling. The area outside of the clamp gets filled up with the last valid values
							// to emulate GL_CLAMP_TO_EDGE behaviour. This is done to avoid using NPOT textures.
							for ( INT v=0; v<VLimit; v++ )
							{
								// The AND 0x7F7F7F7F is used to eliminate the top bit as this was used internally by the LightManager.
								for ( INT u=0; u<ULimit; u++ )
									*Ptr++ = (GET_COLOR_DWORD(Mip->DataPtr[(u+v*USize)<<2])&0x7F7F7F7F)<<1; // We can skip this shift. Unless we want to unpack as sRGB.

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
#if ENGINE_VERSION==227
						case TEXF_RGBA16:
#endif
							ImgSrc = Mip->DataPtr;
							break;

						// S3TC -- Ubiquitous Extension.
						case TEXF_DXT1:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize = USize*VSize/2;
							ImgSrc = Mip->DataPtr;
							break;
#if ENGINE_VERSION==227
						case TEXF_DXT3:
						case TEXF_DXT5:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize = USize*VSize;
							ImgSrc = Mip->DataPtr;
							break;

						// RGTC -- Core since OpenGL 3.0. Also available on Direct3D 10.
						case TEXF_RGTC_R:
						case TEXF_RGTC_R_SIGNED:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize  = USize*VSize/2;
							ImgSrc = Mip->DataPtr;
							break;
						case TEXF_RGTC_RG:
						case TEXF_RGTC_RG_SIGNED:
							if ( USize<4 || VSize<4 )
								goto FinishedUnpack;
							CompImageSize = USize*VSize;
							ImgSrc = Mip->DataPtr;
							break;
						case TEXF_BPTC_RGBA:
						case TEXF_BPTC_RGB_SF:
						case TEXF_BPTC_RGB_UF:
							if (USize<4 || VSize<4)
								goto FinishedUnpack;
							CompImageSize = USize*VSize;
							ImgSrc = Mip->DataPtr;
							break;

#endif
						// Should not happen (TM).
						default:
							appErrorf( TEXT("Unpacking unknown format %i on %ls."), Info.Format, Info.Texture->GetFullName() );
							break;
					}
				}
				else {
					appErrorf(TEXT("Unpacking unknown format %i on %ls."), Info.Format, Info.Texture->GetFullName() );
					break;
				}
                CHECK_GL_ERROR();

				// Upload texture.
				if ( ExistingBind )
				{
#ifndef __LINUX_ARM__
					if ( CompImageSize )
					{
					    if (!bBindlessRealtimeChanged)
							glCompressedTexSubImage2D(GL_TEXTURE_2D, ++MaxLevel, 0, 0, USize, VSize, InternalFormat, CompImageSize, ImgSrc);
						else glCompressedTextureSubImage2D(Bind->Ids[CacheSlot], ++MaxLevel, 0, 0, USize, VSize, SourceFormat, SourceType, ImgSrc);
						CHECK_GL_ERROR();
					}
					else
					{
					    CHECK_GL_ERROR();
					    if (!bBindlessRealtimeChanged)
							glTexSubImage2D(GL_TEXTURE_2D, ++MaxLevel, 0, 0, USize, VSize, SourceFormat, SourceType, ImgSrc);
						else glTextureSubImage2D(Bind->Ids[CacheSlot], ++MaxLevel, 0, 0, USize, VSize, SourceFormat, SourceType, ImgSrc);
						CHECK_GL_ERROR();
					}
#endif
                }
				else
				{
					if ( CompImageSize )
					{
						if (GenerateMipMaps)
						{
							glCompressedTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, USize, VSize, 0, CompImageSize, ImgSrc);
							glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST);
							glGenerateMipmap(GL_TEXTURE_2D);
                            MaxLevel = Info.NumMips;
                            CHECK_GL_ERROR();
							break;
						}
						else
                        {
							glCompressedTexImage2D(GL_TEXTURE_2D, ++MaxLevel, InternalFormat, USize, VSize, 0, CompImageSize, ImgSrc);
                            CHECK_GL_ERROR();
                        }
					}
					else
					{
						if (GenerateMipMaps)
                        {
                            /*
                            // OpenGL 4.2 needed. Since currently targeting 3.3 at first keep for later.
                            glTexStorage2D(GL_TEXTURE_2D, MAX_MIPS, SourceFormat, USize, VSize);
							CHECK_GL_ERROR();
	                        glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, USize, VSize, SourceFormat, SourceType, UnCompSrc ); //Generate num_mipmaps number of mipmaps here.
	                        CHECK_GL_ERROR();
	                        */
							glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, USize, VSize, 0, SourceFormat, SourceType, ImgSrc);
							glHint(GL_GENERATE_MIPMAP_HINT,GL_NICEST);
							glGenerateMipmap(GL_TEXTURE_2D); // generate a complete set of mipmaps for a texture object
							MaxLevel = Info.NumMips;
							CHECK_GL_ERROR();
							break;
						}
						else
                        {
							glTexImage2D(GL_TEXTURE_2D, ++MaxLevel, InternalFormat, USize, VSize, 0, SourceFormat, SourceType, ImgSrc);
                            CHECK_GL_ERROR();
                        }
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
				GWarn->Logf( TEXT("No mip map unpacked for texture %ls."), Info.Texture->GetPathName() );
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
			CHECK_GL_ERROR();
			unguard;
		}
		unguard;
		CHECK_GL_ERROR();

		// Set max level.
		if ( !ExistingBind || Bind->MaxLevel!=MaxLevel )
		{
			Bind->MaxLevel = MaxLevel;
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MaxLevel );
			CHECK_GL_ERROR();
		}
        // Cleanup.
        if ( SupportsLazyTextures )
            Info.Unload();
	}

    if (UseBindlessTextures && Info.Texture && !Unsupported)
    {
		if (!Bind->TexNum[CacheSlot] && GlobalUniformTextureHandles.UniformBuffer) //additional check required in case of runtime change UseBindlessTextures.
        {
            Bind->TexNum[CacheSlot]=TexNum;
            #ifdef __LINUX_ARM__
            Bind->TexHandle[CacheSlot] = glGetTextureSamplerHandleNV(Bind->Ids[CacheSlot], Bind->Sampler[CacheSlot]);
            #else
			Bind->TexHandle[CacheSlot] = glGetTextureSamplerHandleARB(Bind->Ids[CacheSlot], Bind->Sampler[CacheSlot]);
			#endif // __Linux_ARM
            CHECK_GL_ERROR();

            //debugf(TEXT("Making %ls with TexNum %i resident 0x%08x%08x"),Info.Texture->GetFullName(),FCachedTextureInfo->TexNum[CacheSlot], FCachedTextureInfo->TexHandle[CacheSlot]);
            #ifdef __LINUX_ARM__
            glMakeTextureHandleResidentNV(Bind->TexHandle[CacheSlot]);
            #else
            glMakeTextureHandleResidentARB(Bind->TexHandle[CacheSlot]);
            #endif
            CHECK_GL_ERROR();

            if (GlobalUniformTextureHandles.Sync[0])
                WaitBuffer(GlobalUniformTextureHandles, 0);

            GlobalUniformTextureHandles.UniformBuffer[TexNum*2] = Bind->TexHandle[CacheSlot];

            LockBuffer(GlobalUniformTextureHandles, 0);

#if ENGINE_VERSION==227 //|| defined(UNREAL_TOURNAMENT_UTPG)
            // avoid lookups by storing information directly into texture. Add bindless information if available.
            // Should save quite some CPU.
            // To make use of this, add "void* TextureHandle" into class ENGINE_API UTexture : public UBitmap
            FCachedTexture* FCachedTextureInfo = (FCachedTexture*)Info.Texture->TextureHandle;

            if (!FCachedTextureInfo)
                FCachedTextureInfo = new (FCachedTexture);

			FCachedTextureInfo->Ids[CacheSlot] = Bind->Ids[CacheSlot];
            FCachedTextureInfo->BaseMip = Bind->BaseMip;
            FCachedTextureInfo->MaxLevel = Bind->MaxLevel;
			FCachedTextureInfo->Sampler[CacheSlot] = Bind->Sampler[CacheSlot];
			FCachedTextureInfo->TexHandle[CacheSlot] = Bind->TexHandle[CacheSlot];
			FCachedTextureInfo->TexNum[CacheSlot] = Bind->TexNum[CacheSlot];

            Info.Texture->TextureHandle = FCachedTextureInfo;
#endif
            TexNum++;

            if (TexNum > NUMTEXTURES)
				debugf(TEXT("Bindless texture overflow! %i"),TexNum);

            if (!Bind->TexHandle[CacheSlot])
                appErrorf(TEXT("No bindless handle!"));
        }
    }
    else
    {
        Bind->TexHandle[CacheSlot] = 0;
        Bind->TexNum[CacheSlot] = 0;
    }
    Tex.TexNum = Bind->TexNum[CacheSlot];

    CHECK_GL_ERROR();
	STAT(unclockFast(Stats.ImageCycles));
	unguard;
}

DWORD UXOpenGLRenderDevice::SetBlend(DWORD PolyFlags, INT ShaderProg, bool InverseOrder)
{
	guard(UOpenGLRenderDevice::SetBlend);
	STAT(clockFast(Stats.BlendCycles));

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

		if (InverseOrder) // check for bInverseOrder order.
			glFrontFace(GL_CCW); //rather expensive switch better try to avoid!
		else
			glFrontFace(GL_CW);
		#endif

		if (ActiveProgram==GouraudPolyVert_Prog)
		{
			if (DrawGouraudBufferData.VertSize > 0)
				DrawGouraudPolyVerts(GL_TRIANGLES, DrawGouraudBufferData);
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
                if (1)//( !(PolyFlags & PF_Mirrored) )
                    glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
                else
                {
                    glBlendFunc( GL_ZERO, GL_SRC_COLOR ); //Mirrors!
                    //debugf(TEXT("Mirror"));
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
	STAT(unclockFast(Stats.BlendCycles));

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

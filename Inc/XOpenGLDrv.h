/*=============================================================================
	XOpenGlDrv.h: Unreal OpenGL support header.
	Copyright 2014-2016 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#ifndef _INCL_XOPENGLDRV_H_
#define _INCL_XOPENGLDRV_H_

#ifdef WIN32
#include <windows.h>
#endif
#include <cmath>
#ifndef WIN32
using std::isnan;
using std::isfinite;
#endif
#include "Engine.h"
#if ENGINE_VERSION==227
#include "Render.h"
#elif ENGINE_VERSION==430
#include "Render.h"
#elif ENGINE_VERSION>=436 && ENGINE_VERSION<1100
#include "Render.h"
#elif ENGINE_VERSION==1100
#include "RenderPrivate.h"
#endif
#include "UnRender.h"

#if !defined(SDL2BUILD) && !defined(SDL3BUILD)
# ifdef __LINUX__
#  define SDLBUILD  1
#  define SDL2BUILD 1
#  define SDL3BUILD 0
# elif MACOSX
#  define SDLBUILD  1
#  define SDL2BUILD 0
#  define SDL3BUILD 1
# elif defined(__EMSCRIPTEN__)
#  define SDLBUILD  1
#  define SDL2BUILD 0
#  define SDL3BUILD 1
# endif
#endif

//#define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV UXOpenGLRenderDevice::StaticClass();
extern "C" { void autoInitializeRegistrantsXOpenGLDrv(void); }
#define AUTO_INITIALIZE_REGISTRANTS_XOPENGLDRV autoInitializeRegistrantsXOpenGLDrv();

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Selects the sound mixer interface
//
//----------------------------------------------------------------------------
//
// Disclaimer
//
// THIS FILE IS NOT GENERATED OR SUPPORTED BY ACTIVISION.
//
// This material has been developed at apolyton.net by the Apolyton CtP2
// Source Code Project. Contact the authors at ctp2source@apolyton.net.
//
//----------------------------------------------------------------------------
//
// CTP2_USE_MIX_API is defined when the sound code talks to an SDL_mixer
// style interface (Mix_Chunk, Mix_PlayChannel, ...): SDL_mixer itself, or on
// the Amiga builds the AHI mixer in os/amiga/amiga_mixer.h, which provides
// the subset of that interface the game uses. Without it, the sound code uses
// the Miles Sound System (the original Windows build).
//
//----------------------------------------------------------------------------

#ifndef __SOUNDMIXER_H__
#define __SOUNDMIXER_H__ 1

#if defined(USE_SDL)
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_rwops.h>
#define CTP2_USE_MIX_API 1
#elif defined(CTP2_AMIGA_BUILD)
#include "amiga_mixer.h"
#define CTP2_USE_MIX_API 1
#else
#include "mss.h"
#endif

#endif

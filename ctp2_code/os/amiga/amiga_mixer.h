/*
 * The part of the SDL_mixer interface the game's sound code uses
 * (sound/civsound.cpp, sound/soundmanager.cpp), for the Amiga builds.
 *
 * Sound effects are WAV data the game loads into memory; Mix_LoadWAV_RW turns
 * each into 16-bit stereo at the output rate once, and amiga_mixer.cpp mixes up
 * to MIX_CHANNELS of them into the AHI ring (os/amiga/amiga_audio.c) from the
 * main loop. Music (Mix_Music) is not played: Mix_LoadMUS returns NULL, which
 * the sound manager already handles as "no music".
 */
#ifndef CTP2_AMIGA_MIXER_H
#define CTP2_AMIGA_MIXER_H 1

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;

#define SDL_INIT_AUDIO      0x00000010u
#define AUDIO_S16SYS        0x9010   /* the value is not used on the Amiga */
#define MIX_MAX_VOLUME      128
#define MIX_CHANNELS        8

typedef struct SDL_RWops
{
	const Uint8 *   mem;
	size_t          size;
} SDL_RWops;

typedef struct Mix_Chunk
{
	int     allocated;
	Uint8 * abuf;       /* interleaved stereo Sint16 at the output rate */
	Uint32  alen;       /* bytes */
	Uint8   volume;     /* 0 .. MIX_MAX_VOLUME */
} Mix_Chunk;

typedef struct Mix_Music Mix_Music;

#ifdef __cplusplus
extern "C" {
#endif

int          SDL_InitSubSystem(Uint32 flags);
void         SDL_QuitSubSystem(Uint32 flags);
const char * SDL_GetError(void);
SDL_RWops *  SDL_RWFromMem(void *mem, int size);

int          Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void         Mix_CloseAudio(void);

Mix_Chunk *  Mix_LoadWAV_RW(SDL_RWops *src, int freesrc);
Mix_Chunk *  Mix_QuickLoad_RAW(Uint8 *mem, Uint32 len);
void         Mix_FreeChunk(Mix_Chunk *chunk);
int          Mix_VolumeChunk(Mix_Chunk *chunk, int volume);

int          Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int          Mix_HaltChannel(int channel);
int          Mix_Playing(int channel);

Mix_Music *  Mix_LoadMUS(const char *file);
void         Mix_FreeMusic(Mix_Music *music);
int          Mix_PlayMusic(Mix_Music *music, int loops);
int          Mix_PlayingMusic(void);
int          Mix_VolumeMusic(int volume);

#ifdef __cplusplus
}
#endif

#endif

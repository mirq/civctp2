/*
 * SDL_mixer's sound effect channels on AHI, for the Amiga builds
 * (amiga_mixer.h says what is there and why).
 *
 * Plain C++ over the platform layer: no AmigaOS header here.
 */
#include <stdlib.h>
#include <string.h>

#include "amiga_mixer.h"
#include "amiga_platform.h"

namespace
{
	struct Channel
	{
		Mix_Chunk * chunk;
		Uint32      pos;     // frames played
		int         loops;   // -1: forever
	};

	Channel s_channels[MIX_CHANNELS];
	int     s_rate      = 0;
	bool    s_open      = false;
	char    s_error[96] = "";

	inline Uint16 le16(const Uint8 *p) { return static_cast<Uint16>(p[0] | (p[1] << 8)); }
	inline Uint32 le32(const Uint8 *p)
	{
		return static_cast<Uint32>(p[0]) | (static_cast<Uint32>(p[1]) << 8)
		     | (static_cast<Uint32>(p[2]) << 16) | (static_cast<Uint32>(p[3]) << 24);
	}

	void SetError(const char *msg)
	{
		strncpy(s_error, msg, sizeof(s_error) - 1);
		s_error[sizeof(s_error) - 1] = 0;
	}

	// Mixes every playing channel into out (stereo frames), called by the AHI
	// layer from the main loop.
	void Fill(int16_t *out, int frames, void *)
	{
		memset(out, 0, static_cast<size_t>(frames) * 4);

		int32_t acc[2 * 256];
		int done = 0;
		while (done < frames)
		{
			int const n = (frames - done) > 256 ? 256 : (frames - done);
			memset(acc, 0, sizeof(acc[0]) * 2 * n);
			bool any = false;

			for (int c = 0; c < MIX_CHANNELS; ++c)
			{
				Channel &ch = s_channels[c];
				if (!ch.chunk) continue;

				const int16_t * src     = reinterpret_cast<const int16_t *>(ch.chunk->abuf);
				Uint32 const    total   = ch.chunk->alen / 4;
				int const       vol     = ch.chunk->volume;
				int             i       = 0;

				while (i < n && ch.chunk)
				{
					if (ch.pos >= total)
					{
						if (ch.loops == 0) { ch.chunk = NULL; break; }
						if (ch.loops > 0) --ch.loops;
						ch.pos = 0;
						if (total == 0) { ch.chunk = NULL; break; }
					}
					Uint32 const avail = total - ch.pos;
					int const    take  = (static_cast<Uint32>(n - i) < avail) ? (n - i) : static_cast<int>(avail);
					const int16_t * s  = src + 2 * ch.pos;
					int32_t *       a  = acc + 2 * i;
					for (int k = 0; k < take; ++k)
					{
						a[2 * k]     += (s[2 * k]     * vol) >> 7;
						a[2 * k + 1] += (s[2 * k + 1] * vol) >> 7;
					}
					ch.pos += take;
					i      += take;
					any     = true;
				}
			}

			if (any)
			{
				int16_t * o = out + 2 * done;
				for (int k = 0; k < 2 * n; ++k)
				{
					int32_t v = acc[k];
					o[k] = static_cast<int16_t>(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
				}
			}
			done += n;
		}
	}
}

extern "C" {

int SDL_InitSubSystem(Uint32)
{
	return 0;
}

void SDL_QuitSubSystem(Uint32)
{
}

const char * SDL_GetError(void)
{
	return s_error;
}

SDL_RWops * SDL_RWFromMem(void *mem, int size)
{
	SDL_RWops * rw = static_cast<SDL_RWops *>(malloc(sizeof(SDL_RWops)));
	if (rw)
	{
		rw->mem  = static_cast<const Uint8 *>(mem);
		rw->size = size > 0 ? static_cast<size_t>(size) : 0;
	}
	return rw;
}

int Mix_OpenAudio(int frequency, Uint16, int, int)
{
	if (s_open) return 0;

	memset(s_channels, 0, sizeof(s_channels));

	// The rate the game asks for (22050) is a wish: AHI plays at what
	// amiga_audio_open chose, and the effects are resampled to it at load.
	(void) frequency;
	int const rate = amiga_audio_open();
	if (!rate)
	{
		SetError("ahi.device could not be opened");
		return -1;
	}

	s_rate = rate;
	s_open = true;
	amiga_audio_set_fill(Fill, NULL);
	amiga_log("Sound: AHI at %d Hz, %d channels\n", rate, MIX_CHANNELS);
	return 0;
}

void Mix_CloseAudio(void)
{
	if (!s_open) return;
	amiga_audio_set_fill(NULL, NULL);
	amiga_audio_close();
	memset(s_channels, 0, sizeof(s_channels));
	s_open = false;
}

// A RIFF WAVE file in memory: PCM 8 or 16 bits, mono or stereo, any rate,
// turned into stereo 16-bit at the output rate (linear interpolation).
Mix_Chunk * Mix_LoadWAV_RW(SDL_RWops *src, int freesrc)
{
	Mix_Chunk * chunk = NULL;

	if (!src || !src->mem || src->size < 12
	||  memcmp(src->mem, "RIFF", 4) != 0 || memcmp(src->mem + 8, "WAVE", 4) != 0)
	{
		SetError("not a RIFF WAVE file");
		if (freesrc) free(src);
		return NULL;
	}

	const Uint8 * p   = src->mem + 12;
	const Uint8 * end = src->mem + src->size;

	int           format = 0, channels = 0, bits = 0;
	Uint32        rate = 0;
	const Uint8 * data = NULL;
	Uint32        dataLen = 0;

	while (p + 8 <= end)
	{
		Uint32 const len = le32(p + 4);
		const Uint8 * body = p + 8;
		if (memcmp(p, "fmt ", 4) == 0 && len >= 16 && body + 16 <= end)
		{
			format   = le16(body);
			channels = le16(body + 2);
			rate     = le32(body + 4);
			bits     = le16(body + 14);
		}
		else if (memcmp(p, "data", 4) == 0)
		{
			data    = body;
			dataLen = (body + len <= end) ? len : static_cast<Uint32>(end - body);
		}
		p = body + len + (len & 1);
	}

	if (freesrc) free(src);

	if (format != 1 || (channels != 1 && channels != 2) || (bits != 8 && bits != 16)
	||  rate == 0 || !data)
	{
		SetError("unsupported WAVE format (PCM 8/16 bit, mono/stereo only)");
		return NULL;
	}

	int const    bytesPerFrame = channels * bits / 8;
	Uint32 const inFrames      = dataLen / bytesPerFrame;
	Uint32 const outRate       = s_rate ? s_rate : rate;
	Uint32 const outFrames     = static_cast<Uint32>((static_cast<uint64_t>(inFrames) * outRate) / rate);

	chunk = static_cast<Mix_Chunk *>(malloc(sizeof(Mix_Chunk)));
	if (!chunk) return NULL;

	int16_t * out = static_cast<int16_t *>(malloc(static_cast<size_t>(outFrames) * 4 + 4));
	if (!out)
	{
		free(chunk);
		return NULL;
	}

	// Source frame position in 16.16 fixed point.
	uint64_t const step = (static_cast<uint64_t>(rate) << 16) / outRate;
	uint64_t       pos  = 0;

	for (Uint32 i = 0; i < outFrames; ++i, pos += step)
	{
		Uint32 const idx  = static_cast<Uint32>(pos >> 16);
		int32_t const frac = static_cast<int32_t>(pos & 0xFFFF);
		Uint32 const nxt  = (idx + 1 < inFrames) ? idx + 1 : idx;

		int32_t l0, r0, l1, r1;
		const Uint8 * a = data + idx * bytesPerFrame;
		const Uint8 * b = data + nxt * bytesPerFrame;
		if (bits == 16)
		{
			l0 = static_cast<int16_t>(le16(a));
			l1 = static_cast<int16_t>(le16(b));
			r0 = channels == 2 ? static_cast<int16_t>(le16(a + 2)) : l0;
			r1 = channels == 2 ? static_cast<int16_t>(le16(b + 2)) : l1;
		}
		else
		{
			l0 = (static_cast<int32_t>(a[0]) - 128) << 8;
			l1 = (static_cast<int32_t>(b[0]) - 128) << 8;
			r0 = channels == 2 ? (static_cast<int32_t>(a[1]) - 128) << 8 : l0;
			r1 = channels == 2 ? (static_cast<int32_t>(b[1]) - 128) << 8 : l1;
		}
		out[2 * i]     = static_cast<int16_t>(l0 + (((l1 - l0) * frac) >> 16));
		out[2 * i + 1] = static_cast<int16_t>(r0 + (((r1 - r0) * frac) >> 16));
	}

	chunk->allocated = 1;
	chunk->abuf      = reinterpret_cast<Uint8 *>(out);
	chunk->alen      = outFrames * 4;
	chunk->volume    = MIX_MAX_VOLUME;
	return chunk;
}

Mix_Chunk * Mix_QuickLoad_RAW(Uint8 *mem, Uint32 len)
{
	Mix_Chunk * chunk = static_cast<Mix_Chunk *>(malloc(sizeof(Mix_Chunk)));
	if (!chunk) return NULL;
	chunk->allocated = 0;
	chunk->abuf      = mem;
	chunk->alen      = len & ~3u;
	chunk->volume    = MIX_MAX_VOLUME;
	return chunk;
}

void Mix_FreeChunk(Mix_Chunk *chunk)
{
	if (!chunk) return;

	for (int c = 0; c < MIX_CHANNELS; ++c)
	{
		if (s_channels[c].chunk == chunk) s_channels[c].chunk = NULL;
	}

	if (chunk->allocated) free(chunk->abuf);
	free(chunk);
}

int Mix_VolumeChunk(Mix_Chunk *chunk, int volume)
{
	if (!chunk) return -1;
	int const old = chunk->volume;
	if (volume >= 0)
	{
		chunk->volume = static_cast<Uint8>(volume > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : volume);
	}
	return old;
}

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
	if (!s_open || !chunk) return -1;

	if (channel < 0)
	{
		for (int c = 0; c < MIX_CHANNELS; ++c)
		{
			if (!s_channels[c].chunk) { channel = c; break; }
		}
		if (channel < 0)
		{
			SetError("no free channel");
			return -1;
		}
	}
	else if (channel >= MIX_CHANNELS)
	{
		return -1;
	}

	s_channels[channel].chunk = chunk;
	s_channels[channel].pos   = 0;
	s_channels[channel].loops = loops;
	return channel;
}

int Mix_HaltChannel(int channel)
{
	if (channel < 0)
	{
		for (int c = 0; c < MIX_CHANNELS; ++c) s_channels[c].chunk = NULL;
	}
	else if (channel < MIX_CHANNELS)
	{
		s_channels[channel].chunk = NULL;
	}
	return 0;
}

int Mix_Playing(int channel)
{
	if (channel < 0)
	{
		int n = 0;
		for (int c = 0; c < MIX_CHANNELS; ++c) n += (s_channels[c].chunk != NULL);
		return n;
	}
	return (channel < MIX_CHANNELS && s_channels[channel].chunk) ? 1 : 0;
}

Mix_Music * Mix_LoadMUS(const char *)
{
	SetError("no music on the Amiga build");
	return NULL;
}

void Mix_FreeMusic(Mix_Music *)
{
}

int Mix_PlayMusic(Mix_Music *, int)
{
	return -1;
}

int Mix_PlayingMusic(void)
{
	return 0;
}

int Mix_VolumeMusic(int)
{
	return MIX_MAX_VOLUME;
}

} // extern "C"

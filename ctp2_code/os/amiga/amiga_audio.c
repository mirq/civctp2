/*
 * The sound output (amiga_platform.h): a ring that AHI plays for ever and the
 * game's mixer (amiga_mixer.cpp) fills from the main loop.
 *
 * One dynamic sample (AHIST_DYNAMICSAMPLE), looped on one channel, as the Quake
 * ports on WarpOS and OpenSWOS (src/audio_wos.c) do it. Where AHI is playing
 * comes from an AHIET_CHANNELINFO effect hook, which AHI calls on every mix with
 * the play offset; the writer stays ahead of it. A frame makes no call to the
 * 68k side: on WarpOS the written lines are flushed (dcbf) instead, because the
 * 68k does not snoop the PPC's cache.
 */
#include <stdlib.h>
#include <string.h>

#include "amiga_os.h"
#include "amiga_platform.h"

/* The AHIET_CHANNELINFO hook, as 68k code (AHI calls it on its own side):
 *     hook->h_Data = info->ahieci_Offset[0]; return 0;
 * (movem.l a2,-(sp); movea.l a0,a2; move.l 12(a1),d0; move.l d0,16(a2);
 *  moveq #0,d0; movea.l (sp)+,a2; rts) */
static unsigned short s_hookCode[] =
{
	0x48e7, 0x0020, 0x2448, 0x2029, 0x000c, 0x2540, 0x0010, 0x7000, 0x245f, 0x4e75
};

#if defined(CTP2_AMIGA_MOS)
/* MorphOS: the same hook as PowerPC code behind a TRAP_LIB gate, so AHI calls
 * it without the 68k emulator. Hook in A0, the effect info in A1. */
static ULONG channel_info_hook(void)
{
	struct Hook *              const hook = (struct Hook *)REG_A0;
	struct AHIEffChannelInfo * const info = (struct AHIEffChannelInfo *)REG_A1;

	hook->h_Data = (APTR)info->ahieci_Offset[0];
	return 0;
}

static struct EmulLibEntry s_hookGate = { TRAP_LIB, 0, (void (*)(void))channel_info_hook };
#endif

#pragma pack(push, 2)
struct ChannelInfo
{
	struct AHIEffChannelInfo cinfo;
	ULONG                    x[1];      /* ahieci_Offset[] for the one channel */
};
#pragma pack(pop)

#define RING_FRAMES   16384                 /* a power of two: 1 s at 16 kHz */
#define FRAME_BYTES   4                     /* 16-bit stereo */
#define LEAD_FRAMES   1024                  /* how far ahead writing restarts */
#define TARGET_AHEAD  (RING_FRAMES / 4)     /* what one update fills up to */
#define CHUNK_FRAMES  1024

struct Library *AHIBase;

static struct MsgPort      *s_port;
static struct AHIRequest   *s_req;
static int                  s_devOpen;
static struct AHIAudioCtrl *s_ctrl;
static int                  s_soundLoaded;
static int                  s_effectOn;
static struct ChannelInfo   s_info;
static struct Hook          s_hook;
static int16_t             *s_ring;
static uint32_t             s_write;        /* frames written, ever */
static uint32_t             s_cleared;      /* ring frame up to which what
                                               AHI played is silence again */
static int                  s_rate;
static int                  s_open;
static AmigaAudioFill       s_fill;
static void                *s_user;
static int16_t              s_chunk[CHUNK_FRAMES * 2];

static uint32_t play_frame(void)
{
	amiga_cache_flush(&s_hook.h_Data, sizeof(s_hook.h_Data));
	return (uint32_t)(uintptr_t)s_hook.h_Data & (RING_FRAMES - 1);
}

static void restart_writer(uint32_t p)
{
	s_write = (s_write & ~(uint32_t)(RING_FRAMES - 1)) + ((p + LEAD_FRAMES) & (RING_FRAMES - 1));
}

/* Frames written and not yet played. A writer that fell behind (the game was
 * busy longer than half the ring, an AI turn) starts again just ahead. */
static uint32_t ahead(void)
{
	uint32_t const p = play_frame();
	uint32_t const w = s_write & (RING_FRAMES - 1);
	uint32_t       d = (w - p) & (RING_FRAMES - 1);

	if (d > RING_FRAMES / 2)
	{
		restart_writer(p);
		d = LEAD_FRAMES;
	}
	return d;
}

int amiga_audio_open(void)
{
	struct AHISampleInfo sample;
	char   env[16];

	if (s_open) return s_rate;

#if defined(CTP2_AMIGA_WOS) || defined(CTP2_AMIGA_MOS)
	s_rate = 22050;
#else
	s_rate = 16000;
#endif
	if (GetVar((CONST_STRPTR)"CTP2_AHI_RATE", (STRPTR)env, sizeof(env), GVF_GLOBAL_ONLY) > 0)
	{
		int const r = atoi(env);
		if (r >= 8000 && r <= 48000) s_rate = r;
	}

	if (!(s_port = CreateMsgPort())) goto fail;
	if (!(s_req = (struct AHIRequest *)CreateIORequest(s_port, sizeof(struct AHIRequest)))) goto fail;
	s_req->ahir_Version = 4;
	if (OpenDevice((CONST_STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)s_req, 0) != 0)
	{
		amiga_log("Sound: no ahi.device V4, the game plays silent\n");
		goto fail;
	}
	s_devOpen = 1;
	AHIBase = (struct Library *)s_req->ahir_Std.io_Device;

	{
		struct TagItem tags[] =
		{
			{ AHIA_AudioID,  AHI_DEFAULT_ID },
			{ AHIA_MixFreq,  (ULONG)s_rate },
			{ AHIA_Channels, 1 },
			{ AHIA_Sounds,   1 },
			{ TAG_DONE,      0 }
		};
		s_ctrl = AHI_AllocAudioA(tags);
	}
	if (!s_ctrl)
	{
		amiga_log("Sound: AHI_AllocAudio failed (is a default mode set in AHI prefs?)\n");
		goto fail;
	}

	s_ring = (int16_t *)AllocVec(RING_FRAMES * FRAME_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
	if (!s_ring) goto fail;
	amiga_cache_flush(s_ring, RING_FRAMES * FRAME_BYTES);

	sample.ahisi_Type    = AHIST_S16S;
	sample.ahisi_Address = s_ring;
	sample.ahisi_Length  = RING_FRAMES;
	if (AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, s_ctrl) != 0) goto fail;
	s_soundLoaded = 1;

	{
		struct TagItem tags[] = { { AHIC_Play, TRUE }, { TAG_DONE, 0 } };
		if (AHI_ControlAudioA(s_ctrl, tags) != 0) goto fail;
	}

#if defined(CTP2_AMIGA_MOS)
	s_hook.h_Entry = (HOOKFUNC)&s_hookGate;
#else
	s_hook.h_Entry = (HOOKFUNC)s_hookCode;
#endif
	s_hook.h_Data  = 0;
	s_info.cinfo.ahie_Effect     = AHIET_CHANNELINFO;
	s_info.cinfo.ahieci_Func     = &s_hook;
	s_info.cinfo.ahieci_Channels = 1;
	amiga_cache_flush(s_hookCode, sizeof(s_hookCode));
	amiga_cache_flush(&s_hook, sizeof(s_hook));
	amiga_cache_flush(&s_info, sizeof(s_info));

	{
		struct TagItem tags[] =
		{
			{ AHIP_BeginChannel, 0 },
			{ AHIP_Freq,         (ULONG)s_rate },
			{ AHIP_Vol,          0x10000 },
			{ AHIP_Pan,          0x8000 },
			{ AHIP_Sound,        0 },
			{ AHIP_EndChannel,   0 },
			{ TAG_DONE,          0 }
		};
		AHI_PlayA(s_ctrl, tags);
	}
	AHI_SetEffect(&s_info, s_ctrl);
	s_effectOn = 1;

	s_write   = LEAD_FRAMES;
	s_cleared = 0;
	s_open    = 1;

	amiga_log("Sound: AHI 16-bit stereo at %d Hz, a ring of %d frames\n", s_rate, RING_FRAMES);
	return s_rate;

fail:
	amiga_audio_close();
	return 0;
}

void amiga_audio_close(void)
{
	if (s_ctrl)
	{
		struct TagItem tags[] = { { AHIC_Play, FALSE }, { TAG_DONE, 0 } };

		if (s_effectOn)
		{
			s_info.cinfo.ahie_Effect = AHIET_CHANNELINFO | AHIET_CANCEL;
			AHI_SetEffect(&s_info, s_ctrl);
			s_effectOn = 0;
		}
		AHI_ControlAudioA(s_ctrl, tags);
		if (s_soundLoaded)
		{
			AHI_UnloadSound(0, s_ctrl);
			s_soundLoaded = 0;
		}
		AHI_FreeAudio(s_ctrl);
		s_ctrl = NULL;
	}
	if (s_ring)
	{
		FreeVec(s_ring);
		s_ring = NULL;
	}
	if (s_devOpen)
	{
		CloseDevice((struct IORequest *)s_req);
		s_devOpen = 0;
	}
	if (s_req)
	{
		DeleteIORequest((struct IORequest *)s_req);
		s_req = NULL;
	}
	if (s_port)
	{
		DeleteMsgPort(s_port);
		s_port = NULL;
	}
	AHIBase = NULL;
	s_open  = 0;
}

void amiga_audio_set_fill(AmigaAudioFill fill, void *user)
{
	s_fill = fill;
	s_user = user;
}

static void write_frames(const int16_t *pcm, uint32_t frames)
{
	uint32_t const w     = s_write & (RING_FRAMES - 1);
	uint32_t       first = RING_FRAMES - w;

	if (first > frames) first = frames;

	memcpy(s_ring + w * 2, pcm, first * FRAME_BYTES);
	amiga_cache_flush(s_ring + w * 2, first * FRAME_BYTES);

	if (frames > first)
	{
		memcpy(s_ring, pcm + first * 2, (frames - first) * FRAME_BYTES);
		amiga_cache_flush(s_ring, (frames - first) * FRAME_BYTES);
	}

	s_write += frames;
}

/* What AHI has played since the last update becomes silence. The ring loops
 * for ever: when the game stops filling it (a long AI turn blocks the main
 * loop), AHI goes on past what was written, and without this it would play
 * the second before over and over, every sound in it repeating once a second
 * (beeps). Now it runs into silence instead. A frame's worth, ~2 KB. */
static void silence_played(void)
{
	uint32_t const p = play_frame();
	uint32_t       c = s_cleared;

	if (c == p) return;
	if (p < c)
	{
		memset(s_ring + c * 2, 0, (RING_FRAMES - c) * FRAME_BYTES);
		amiga_cache_flush(s_ring + c * 2, (RING_FRAMES - c) * FRAME_BYTES);
		c = 0;
	}
	memset(s_ring + c * 2, 0, (p - c) * FRAME_BYTES);
	amiga_cache_flush(s_ring + c * 2, (p - c) * FRAME_BYTES);
	s_cleared = p;
}

void amiga_audio_update(void)
{
	uint32_t have;

	if (!s_open) return;

	silence_played();

	have = ahead();
	while (have < TARGET_AHEAD)
	{
		uint32_t n = TARGET_AHEAD - have;
		if (n > CHUNK_FRAMES) n = CHUNK_FRAMES;

		if (s_fill)
		{
			s_fill(s_chunk, (int)n, s_user);
		}
		else
		{
			memset(s_chunk, 0, n * FRAME_BYTES);
		}

		write_frames(s_chunk, n);
		have += n;
	}
}

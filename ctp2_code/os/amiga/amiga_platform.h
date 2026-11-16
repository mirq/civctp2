/*
 * The Amiga platform layer of Call to Power II: one plain C interface over the
 * AmigaOS calls, shared by the 68060 build (Makefile.amiga68k) and the WarpOS
 * build (Makefile.warpos).
 *
 * Only the files in ctp2_code/os/amiga include AmigaOS headers; the game and
 * the Amiga UI backend (ui/aui_amiga) see this header. That keeps the OS
 * structures' 68k layout (#pragma pack(2) on the PPC) and the OS headers'
 * macros (Remove, Wait, Lock, ...) away from the game code.
 */
#ifndef CTP2_AMIGA_PLATFORM_H
#define CTP2_AMIGA_PLATFORM_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Startup, libraries -------------------------------------------------- */

/* Opens the libraries the game needs (intuition, graphics, cybergraphics,
 * keymap, timer.device) and enters the program's own drawer (PROGDIR:) as the
 * current directory. Returns 0 and logs the reason when one is missing. */
int  amiga_platform_init(void);
void amiga_platform_exit(void);
#if defined(CTP2_AMIGA_WOS)
void amiga_exit_skip_destructors(void);
#endif

/* Exits through the game's normal cleanup after CTRL-C or a window close. */
int  amiga_break_requested(void);

/* CTRL-F (Break n F) since the last call: the UI writes the picture it
 * composes to RAM:ctp2shot.ppm (the screen is 16-bit RTG, which most
 * screen grabbers do not read). */
int  amiga_snapshot_requested(void);
/* The same without taking the request (the UI makes a copy for it then). */
int  amiga_snapshot_pending(void);
/* The game asks for one itself (a benchmark), written to path: only the
 * picture then, not the card and the shadow. amiga_snapshot_path is NULL for
 * a CTRL-F one; amiga_snapshot_done forgets the path. */
void amiga_snapshot_request(const char *path);
const char *amiga_snapshot_path(void);
void amiga_snapshot_done(void);

/* ENV:name into value (size bytes); 0 when it is not set. (The C library's
 * getenv does not read ENV: here.) */
int  amiga_env_value(const char *name, char *value, int size);

/* Whether ENV:name is set (and not "0"). */
int  amiga_env_flag(const char *name);

/* ENV:CTP2_PERF: microseconds per phase of the frame, logged and cleared
 * with the main loop's Perf line (civ3_main.cpp). */
enum
{
	AMIGA_PERF_MAP, AMIGA_PERF_UI, AMIGA_PERF_DIRECTOR, AMIGA_PERF_ROBOT,
	AMIGA_PERF_SOUND, AMIGA_PERF_EVENTS, AMIGA_PERF_WRITE,
	AMIGA_PERF_UI_IDLE, AMIGA_PERF_UI_MOUSE, AMIGA_PERF_UI_DRAW,
	AMIGA_PERF_MAP_MIX, AMIGA_PERF_MAP_WATER, AMIGA_PERF_MAP_SPRITES,
	AMIGA_PERF_MAP_RESTORE, AMIGA_PERF_MAP_HILITE,
	AMIGA_PERF_SCROLL_PIXELS, AMIGA_PERF_SCROLL_TILES, AMIGA_PERF_SCROLL_OVERLAYS,
	AMIGA_PERF_SCROLL_SPRITES, AMIGA_PERF_SCROLL_OTHER,
	AMIGA_PERF_PANEL_IDLE, AMIGA_PERF_SCROLL_LOOP, AMIGA_PERF_SCROLL_UI, AMIGA_PERF_SCROLL_NET,
	AMIGA_PERF_SPR_LAYER, AMIGA_PERF_SPR_DIRECTOR, AMIGA_PERF_SPR_OVERLAY, AMIGA_PERF_SPR_NAMES,
	AMIGA_PERF_FONT_LOAD, AMIGA_PERF_FONT_SIZE, AMIGA_PERF_FONT_LOADS, AMIGA_PERF_FONT_SIZES,
	AMIGA_PERF_UI_MINIMIZE, AMIGA_PERF_UI_MAXRECTS, AMIGA_PERF_UI_PRE,
	AMIGA_PERF_BUSY, AMIGA_PERF_SCROLL_COPY, AMIGA_PERF_CARD_SHIFT, AMIGA_PERF_CARD_LOCK,
	AMIGA_PERF_CARD_DIFF, AMIGA_PERF_SCROLL_STEPS,
	AMIGA_PERF_RES_HITS, AMIGA_PERF_RES_MISSES, AMIGA_PERF_RES_FIND, AMIGA_PERF_RES_LOAD,
	AMIGA_PERF_PATH_CALLS, AMIGA_PERF_PATH_MISSES, AMIGA_PERF_PATH_US,
	AMIGA_PERF_LDL_BUILDS, AMIGA_PERF_LDL_US,
	AMIGA_PERF_SLOTS
};
extern int      amiga_perf;                       /* ENV:CTP2_PERF, read at start */
extern uint32_t amiga_perf_us[AMIGA_PERF_SLOTS];
#define AMIGA_PERF_BEGIN(t)      uint64_t const t = amiga_perf ? amiga_ticks_us() : 0
#define AMIGA_PERF_END(t, slot)  do { if (amiga_perf) amiga_perf_us[slot] += (uint32_t)(amiga_ticks_us() - (t)); } while (0)

/* Free fast memory in bytes (all of it, not the largest block): the UI keeps
 * the images and fonts it no longer uses while plenty is left
 * (ui/aui_common/aui_resource.h). */
unsigned long amiga_avail_fast(void);

/* The n-th preferred language as an ISO 639-1 code ("en", "de", ...), or NULL
 * past the last: ENV:CTP2_LANGUAGE first, then the Locale preferences. */
const char *amiga_preferred_language(int n);

/* An Intuition requester; buttons separated by '|'. Returns the chosen
 * button counted from 1 for the first (Intuition's own numbering gives 0 to
 * the last button; this one does not). */
int amiga_message_box(const char *title, const char *text, const char *buttons);

/* ---- Files ---------------------------------------------------------------- */

/* The game's POSIX-style relative path as an AmigaDOS one, in one of a few
 * rotating static buffers: "." and empty parts dropped, "a/../b" resolved, a
 * leading ".." turned into "/". Paths with a volume ("Work:x") keep it. */
const char *amiga_fix_path(const char *path);

/* The absolute AmigaDOS path of path (NULL: the current directory). */
int amiga_fullpath(const char *path, char *out, size_t size);

/* ---- Time -------------------------------------------------------------- */

/* Milliseconds since amiga_platform_init, monotonic. */
uint32_t amiga_ticks_ms(void);
/* Microseconds since amiga_platform_init, monotonic, for profiling. */
uint64_t amiga_ticks_us(void);
/* Waits about ms milliseconds without busy-looping the 68k where possible. */
void amiga_delay_ms(uint32_t ms);

/* ---- Log ------------------------------------------------------------------ */

/* Buffered log, written to PROGDIR:ctp2.log every 2 s and at exit
 * (a line-per-write log costs tens of ms per frame on these machines). */
void amiga_log(const char *fmt, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 1, 2)))
#endif
	;
void amiga_log_flush(void);

/* ---- Video -------------------------------------------------------------- */

typedef struct AmigaRect
{
	int32_t left, top, right, bottom; /* right/bottom exclusive, as RECT */
} AmigaRect;

/* Opens a 16-bit RTG screen of at least width x height and a borderless
 * backdrop window on it for input. The mode comes from ENV:CTP2_MODE (a
 * display id, hex), else the closest CyberGraphX mode. The game's pixels are
 * native 16-bit R5G6B5 (big-endian words). Returns 0 on failure. */
int  amiga_video_open(int width, int height);
void amiga_video_close(void);
/* The card's 16-bit RTG screen sizes, at most max of them, sorted, no
 * duplicates. Returns how many. */
int  amiga_video_modes(int *widths, int *heights, int max);
int  amiga_video_width(void);
int  amiga_video_height(void);

/* Copies rectangles of a width x height R5G6B5 frame (pitch in bytes) to the
 * screen. Only the rows of the rectangles cross the bus. */
void amiga_video_present(const uint16_t *pixels, int pitch,
                         const AmigaRect *rects, int count);

/* The same from several frames in one copy (the map from its own surface and
 * the windows from the composed frame): all are compared first, then the
 * card's shifts and the writes follow back to back. Each frame's pixels
 * point at its screen pixel (0, 0). */
typedef struct AmigaSource
{
	const uint16_t *  pixels;
	int               pitch;
	const AmigaRect * rects;
	int               count;
} AmigaSource;
void amiga_video_present_multi(const AmigaSource *sources, int count);

/* Whether the copies compare with a shadow of the card (not once the lock
 * was refused). */
int amiga_video_has_shadow(void);
/* What the card shows at x, y, w x h pixels, into out (w pixels a row).
 * Returns 0 without a shadow or outside the picture. */
int amiga_video_read_shadow(int x, int y, int w, int h, uint16_t *out);

/* A rectangle of the next full copy that goes out whole, past the compare:
 * for what is known to have changed everywhere (a scroll's new strip). */
void amiga_video_force_rect(int left, int top, int right, int bottom);

/* What was touched while shifts wait (amiga_video_touch), where it is and
 * where the shifts take it, at most max rectangles: the caller puts them
 * among the rectangles of the full copy that does the shifts. */
int amiga_video_take_touched(AmigaRect *out, int max);

/* The picture's rectangle x, y, w, h was shifted by dx, dy pixels in the
 * game's own surface (positive: its content moved left or up). The card's
 * copy is shifted by its blitter and the shadow with it, so that the next
 * copy sends only what differs from the shifted picture.
 * Returns 0 when it will not shift (no shadow: the lock was refused): the
 * caller then leaves its frame alone and has the whole area sent. */
int  amiga_video_scroll(int x, int y, int w, int h, int dx, int dy);

/* Rows [top, bottom) of the next copy go out whole, past the diff: the UI
 * sends a few rows a frame this way, round the screen, so that the card
 * comes right again wherever a write did not reach it. */
void amiga_video_force_rows(int top, int bottom);

/* n 16-bit pixels from src to dst (one picture, as memmove), always copied
 * upwards in memory; shift: how far a row moves right (0: dst - src). */
void amiga_shift_row(uint16_t *dst, const uint16_t *src, int n, int shift);
void amiga_copy_up(uint16_t *dst, const uint16_t *src, int n);

/* While a shift waits (amiga_video_scroll): the rectangle's pixels may come
 * out wrong on the card once it is shifted (the cursor drawn there), so the
 * next full copy compares it where it is and where the shift takes it. */
void amiga_video_touch(int left, int top, int right, int bottom);
int  amiga_video_shift_pending(void);

/* 1 around a copy of the cursor alone, which leaves pending shifts for the
 * next full copy (the frame it copies from is the one before them). */
void amiga_video_cursor_only(int on);

/* ENV:CTP2_PROF (68060): a sampling profiler (amiga_prof.c). Started and
 * stopped with the platform; amiga_prof_poll, from the main loop, writes the
 * samples to RAM:ctp2prof.bin on CTRL-E. */
void amiga_prof_start(void);
void amiga_prof_poll(void);
void amiga_prof_stop(void);
/* What was sampled so far to path (NULL: dropped), and sampling starts
 * again (CTP2_BENCH=win profiles each window's opening this way). */
void amiga_prof_write(const char *path);
/* 0: not sampled now (ENV:CTP2_PROF_EVENT, set around game events). */
extern volatile int amiga_prof_gate;
/* The game event being processed (-1: none), kept with each sample. */
extern volatile int amiga_prof_tag;

/* What the copies cost since the last call (pixels, microseconds, calls), for
 * the ENV:CTP2_PERF log; the counters start again at zero. */
void amiga_video_stats(uint32_t *pixels, uint32_t *us, uint32_t *calls);
/* Pixels written to the card since the screen opened (never reset). */
uint32_t amiga_video_pixels_written(void);

/* The pointer: the game draws its own cursor, the system one is blanked.
 * (The system pointer is no option: Picasso96's sprite is 16 pixels wide
 * and 3 colours on AmigaOS 3, the game's cursors 48x48 in 16 bits.) */
void amiga_video_hide_pointer(void);

/* ENV:CTP2_CHECK: the card read back and compared with picture (the whole
 * picture as it should be, pitch in bytes), and the shadow with it too.
 * Returns 0 when it cannot (no lock, a 15-bit screen); else 1, with how many
 * pixels differ and the rectangle round those of the card. */
int amiga_video_check(const uint16_t *picture, int pitch,
                      uint32_t *cardDiffers, uint32_t *shadowDiffers, AmigaRect *where);

/* Writes what the card shows of the picture to a PPM (the CTRL-F snapshot's
 * check that the copies and shifts left nothing stale). Returns 1 if done. */
int amiga_video_dump_card(const char *path);

/* ---- Input ---------------------------------------------------------------- */

enum
{
	AMIGA_EV_NONE = 0,
	AMIGA_EV_KEY,         /* raw key: code, down, qualifier */
	AMIGA_EV_MOUSEMOVE,   /* x, y (absolute, screen pixels) */
	AMIGA_EV_MOUSEBUTTON, /* button 0=left 1=right 2=middle, down */
	AMIGA_EV_WHEEL,       /* delta +1 up, -1 down */
	AMIGA_EV_QUIT
};

/* Qualifier bits of AMIGA_EV_KEY, as IEQUALIFIER_*. */
#define AMIGA_QUAL_LSHIFT  0x0001
#define AMIGA_QUAL_RSHIFT  0x0002
#define AMIGA_QUAL_CAPS    0x0004
#define AMIGA_QUAL_CTRL    0x0008
#define AMIGA_QUAL_LALT    0x0010
#define AMIGA_QUAL_RALT    0x0020
#define AMIGA_QUAL_LAMIGA  0x0040
#define AMIGA_QUAL_RAMIGA  0x0080

typedef struct AmigaEvent
{
	int      type;
	int      code;      /* raw key code (0..127) or mouse button */
	int      down;
	int      x, y;
	int      qualifier;
	uint32_t time_ms;
	char     ch;        /* the key's character in the current keymap, or 0 */
} AmigaEvent;

/* Takes the next event from the window, or returns 0 when there is none.
 * Touches the OS only when the window's signal is set. */
int  amiga_input_poll(AmigaEvent *ev);
/* The mouse's latest position, taking the window's messages into the queue
 * (they stay there for amiga_input_poll). Returns whether it moved since the
 * last look. For the cursor while the game is busy (an AI turn). */
int  amiga_input_peek_mouse(int *x, int *y);
/* Waits until an event arrives or timeout_ms passes. */
void amiga_input_wait(uint32_t timeout_ms);
/* Milliseconds since the window's last message (key, mouse, button). */
uint32_t amiga_input_idle_ms(void);
/* Current mouse position (screen pixels). */
void amiga_input_mouse(int *x, int *y);

/* ---- Sound -------------------------------------------------------------- */

/* Opens ahi.device and starts a looping 16-bit stereo ring buffer. Returns the
 * rate it plays at (ENV:CTP2_AHI_RATE, else 16000 Hz on the 68060, which mixes
 * on the game's processor, and 22050 on WarpOS), or 0 when AHI is not there:
 * the game then runs silent. */
int  amiga_audio_open(void);
void amiga_audio_close(void);
/* The mixer fills the ring from the main loop: the callback is asked for
 * frames stereo 16-bit frames at a time. */
typedef void (*AmigaAudioFill)(int16_t *stereo, int frames, void *user);
void amiga_audio_set_fill(AmigaAudioFill fill, void *user);
/* Called once per frame from the main loop: writes what the player will need
 * next, ahead of its position. */
void amiga_audio_update(void);

#ifdef __cplusplus
}
#endif

#endif /* CTP2_AMIGA_PLATFORM_H */

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

/* Exits through the game's normal cleanup after CTRL-C or a window close. */
int  amiga_break_requested(void);

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

/* Copies a width x height patch (pitch in bytes) to the screen at x, y. */
void amiga_video_present_patch(const uint16_t *pixels, int pitch,
                               int x, int y, int width, int height);

/* The pointer: the game draws its own cursor, the system one is blanked.
 * (The system pointer is no option: Picasso96's sprite is 16 pixels wide
 * and 3 colours on AmigaOS 3, the game's cursors 48x48 in 16 bits.) */
void amiga_video_hide_pointer(void);

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

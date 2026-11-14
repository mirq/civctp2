/*
 * The RTG screen (amiga_platform.h): a 16-bit CyberGraphX/Picasso96 screen, a
 * borderless backdrop window on it for input, and the copy of the game's
 * frame to the card.
 *
 * The game draws R5G6B5 in big-endian words. Cards differ in how their 16-bit
 * modes lay a pixel out (PIXFMT_RGB16, or the PC byte order PIXFMT_RGB16PC
 * common on PCI and Zorro boards, or BGR, or 15 bits): the copy converts while
 * it writes, which costs the CPU little next to the bus (the 68060 writes a
 * Zorro III card at ~5 MB/s, OpenSWOS docs/RTG060.md).
 *
 * The bitmap is locked only around the copy, with no other OS call inside.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amiga_os.h"
#include "amiga_platform.h"

static struct Screen * s_screen;
static struct Window * s_window;
static UWORD *         s_blankPointer;     /* chip memory, for SetPointer */
static int             s_width;            /* the game's picture */
static int             s_height;
static int             s_ox;               /* where it sits on the screen */
static int             s_oy;
static ULONG           s_pixfmt;
#define LINEBUF_PIXELS 4096
static uint8_t *       s_lineBuf;          /* RGB24 rows for the fallback */
/* What the card shows of the picture, in the game's format: a row each, with
 * SHADOW_SLACK pixels spare on both sides, so that a shift of the whole width
 * moves where the rows start (across) or which row is which (up and down)
 * instead of copying them. The pixels a shift brings in from the slack are
 * whatever lay there: the strip the shift uncovers, which is compared anyway
 * (the next copy marks it). */
#define SHADOW_SLACK 1024
typedef struct { uint16_t *buf; int off; } ShadowRow;
static uint16_t *      s_shadow;           /* the rows' memory (NULL: none) */
static ShadowRow *     s_shadowRows;
static ShadowRow *     s_shadowKeep;       /* as many again, for a rotation of the rows */
#define SHADOW(y)      (s_shadowRows[y].buf + SHADOW_SLACK + s_shadowRows[y].off)
static int             s_shadowOff;        /* 1: send every pixel (the shadow no longer holds) */
static int             s_lockFailedLogged;


#if defined(CTP2_AMIGA_WOS)
/* Amiberry's RTG card (uaegfx.card) redraws only the parts of its memory it
 * saw written by the emulated 68k (or by its own drawing code): the emulated
 * PowerPC writes that memory directly, and the screen kept the grey it was
 * opened with. There the 68k writes the bytes the PPC copied over again
 * (CopyMem onto itself, one call per rectangle) so they are drawn. A real
 * board has no uaegfx.card and nothing of this. */
static int             s_touchFor68k;

static int uaegfx_card(void)
{
	struct Node * node;
	Forbid();
	node = FindName(&SysBase->LibList, (CONST_STRPTR)"uaegfx.card");
	Permit();
	return node != NULL;
}
#endif

/* amiga_input.c reads the window's port. */
struct Window *amiga_video_window(void) { return s_window; }
void amiga_video_offset(int *x, int *y) { *x = s_ox; *y = s_oy; }

static int pixfmt_ok(ULONG fmt)
{
	switch (fmt)
	{
	case PIXFMT_RGB16: case PIXFMT_RGB16PC:
	case PIXFMT_BGR16: case PIXFMT_BGR16PC:
	case PIXFMT_RGB15: case PIXFMT_RGB15PC:
		return 1;
	default:
		return 0;
	}
}

/* A mode is taken only when all of this holds: BestCModeIDTagList never
 * fails, it hands back a native chipset mode when nothing fits. */
static int mode_ok(ULONG id, int width, int height)
{
	if (id == INVALID_ID || !IsCyberModeID(id)) return 0;
	if (!pixfmt_ok(GetCyberIDAttr(CYBRIDATTR_PIXFMT, id))) return 0;
	if (GetCyberIDAttr(CYBRIDATTR_BPPIX, id) != 2) return 0;
	return (int)GetCyberIDAttr(CYBRIDATTR_WIDTH, id)  >= width
	    && (int)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id) >= height;
}

static ULONG choose_mode(int width, int height)
{
	char  env[32];
	ULONG id = INVALID_ID;

	/* 1. ENV:CTP2_MODE, a display id in hex (as the screen mode prefs show). */
	if (GetVar((CONST_STRPTR)"CTP2_MODE", (STRPTR)env, sizeof(env), GVF_GLOBAL_ONLY) > 0)
	{
		id = (ULONG)strtoul(env, NULL, 16);
		if (mode_ok(id, width, height)) return id;
		amiga_log("ENV:CTP2_MODE %s is no 16-bit RTG mode of at least %dx%d, choosing one\n",
		          env, width, height);
	}

	/* 2. CyberGraphX's own choice. */
	{
		struct TagItem tags[] =
		{
			{ CYBRBIDTG_Depth,         16 },
			{ CYBRBIDTG_NominalWidth,  (ULONG)width },
			{ CYBRBIDTG_NominalHeight, (ULONG)height },
			{ TAG_DONE,                0 }
		};
		id = BestCModeIDTagList(tags);
		if (mode_ok(id, width, height)) return id;
	}

	/* 3. The smallest 15/16-bit mode that holds the picture. */
	{
		struct TagItem tags[] =
		{
			{ CYBRMREQ_MinDepth,  15 },
			{ CYBRMREQ_MaxDepth,  16 },
			{ CYBRMREQ_MinWidth,  (ULONG)width },
			{ CYBRMREQ_MinHeight, (ULONG)height },
			{ TAG_DONE,           0 }
		};
		struct List * list = AllocCModeListTagList(tags);
		ULONG best = INVALID_ID;
		ULONG bestArea = 0xFFFFFFFFu;

		if (list)
		{
			struct CyberModeNode * node;
			for (node = (struct CyberModeNode *)list->lh_Head;
			     node->Node.ln_Succ;
			     node = (struct CyberModeNode *)node->Node.ln_Succ)
			{
				ULONG area = (ULONG)node->Width * node->Height;
				if (area < bestArea && mode_ok(node->DisplayID, width, height))
				{
					best     = node->DisplayID;
					bestArea = area;
				}
			}
			FreeCModeList(list);
		}
		return best;
	}
}

int amiga_video_modes(int *widths, int *heights, int max)
{
	struct TagItem tags[] =
	{
		{ CYBRMREQ_MinDepth, 15 },
		{ CYBRMREQ_MaxDepth, 16 },
		{ TAG_DONE,          0 }
	};
	struct List * list;
	int count = 0;

	if (!CyberGfxBase) return 0;

	list = AllocCModeListTagList(tags);
	if (list)
	{
		struct CyberModeNode * node;
		for (node = (struct CyberModeNode *)list->lh_Head;
		     node->Node.ln_Succ && count < max;
		     node = (struct CyberModeNode *)node->Node.ln_Succ)
		{
			int i, j, dup = 0;
			if (!pixfmt_ok(GetCyberIDAttr(CYBRIDATTR_PIXFMT, node->DisplayID))) continue;

			for (i = 0; i < count && !dup; ++i)
				dup = (widths[i] == node->Width && heights[i] == node->Height);
			if (dup) continue;

			/* Sorted by size, smallest first. */
			for (i = count; i > 0
			     && ((long)widths[i - 1] * heights[i - 1] > (long)node->Width * node->Height); --i)
			{
				widths[i]  = widths[i - 1];
				heights[i] = heights[i - 1];
			}
			widths[i]  = node->Width;
			heights[i] = node->Height;
			++count;
			(void)j;
		}
		FreeCModeList(list);
	}

	return count;
}

/* Zeros into the whole screen, through the lock: the shadow says the card is
 * black, and SetRast alone did not make it so (pen 0 of a new screen is grey
 * on a hi-colour screen too, and the diff then never sent the frame's black
 * segments: they came out grey). Without the lock the shadow goes off. */
static void clear_card(int sw, int sh)
{
	APTR  handle;
	ULONG base = 0, bpr = 0;
	struct TagItem tags[] =
	{
		{ LBMI_BASEADDRESS, (ULONG)&base },
		{ LBMI_BYTESPERROW, (ULONG)&bpr },
		{ TAG_DONE,         0 }
	};

	handle = LockBitMapTagList(s_screen->RastPort.BitMap, tags);
	if (handle && base)
	{
		int y;
		for (y = 0; y < sh; ++y)
			memset((uint8_t *)base + (size_t)y * bpr, 0, (size_t)sw * 2);
#if defined(CTP2_AMIGA_WOS)
		if (s_touchFor68k)
			CopyMem((APTR)base, (APTR)base, (ULONG)((size_t)(sh - 1) * bpr + (size_t)sw * 2));
#endif
		UnLockBitMap(handle);
		return;
	}
	if (handle) UnLockBitMap(handle);
	s_shadowOff = 1;
}

int amiga_video_open(int width, int height)
{
	ULONG id;
	int   sw, sh;

	if (s_screen) return 1;

	id = choose_mode(width, height);
	if (id == INVALID_ID)
	{
		amiga_log("No 16-bit RTG mode of at least %dx%d\n", width, height);
		return 0;
	}

	sw = (int)GetCyberIDAttr(CYBRIDATTR_WIDTH, id);
	sh = (int)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
	s_pixfmt = GetCyberIDAttr(CYBRIDATTR_PIXFMT, id);

	{
		/* Pen 0 is grey in a new screen's palette: black, for SetRast. */
		static const struct ColorSpec black[] = { { 0, 0, 0, 0 }, { -1, 0, 0, 0 } };
		struct TagItem tags[] =
		{
			{ SA_DisplayID, id },
			{ SA_Width,     (ULONG)sw },
			{ SA_Height,    (ULONG)sh },
			{ SA_Depth,     16 },
			{ SA_Quiet,     TRUE },
			{ SA_ShowTitle, FALSE },
			{ SA_Draggable, FALSE },
			{ SA_Type,      CUSTOMSCREEN },
			{ SA_Title,     (ULONG)"Call to Power II" },
			{ SA_Colors,    (ULONG)black },
			{ TAG_DONE,     0 }
		};
		s_screen = OpenScreenTagList(NULL, tags);
	}
	if (!s_screen)
	{
		amiga_log("OpenScreen of mode $%08lx failed\n", (unsigned long)id);
		return 0;
	}

	{
		struct TagItem tags[] =
		{
			{ WA_CustomScreen, (ULONG)s_screen },
			{ WA_Left,         0 },
			{ WA_Top,          0 },
			{ WA_Width,        (ULONG)sw },
			{ WA_Height,       (ULONG)sh },
			{ WA_Backdrop,     TRUE },
			{ WA_Borderless,   TRUE },
			{ WA_Activate,     TRUE },
			{ WA_RMBTrap,      TRUE },
			{ WA_ReportMouse,  TRUE },
			{ WA_SimpleRefresh,TRUE },
			{ WA_NoCareRefresh,TRUE },
			{ WA_IDCMP,        IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE },
			{ TAG_DONE,        0 }
		};
		s_window = OpenWindowTagList(NULL, tags);
	}
	if (!s_window)
	{
		amiga_log("OpenWindow failed\n");
		amiga_video_close();
		return 0;
	}

	s_width  = width;
	s_height = height;
	s_ox     = (sw - width) / 2;
	s_oy     = (sh - height) / 2;

	/* The border round a smaller picture is black. */
	SetRast(&s_screen->RastPort, 0);

	/* The shadow starts as zeros, black in every pixel format; clear_card
	 * (below) makes the card show just that. */
	free(s_shadow);
	free(s_shadowRows);
	free(s_shadowKeep);
	s_shadow     = (uint16_t *)calloc((size_t)(width + 2 * SHADOW_SLACK) * (size_t)height, sizeof(uint16_t));
	s_shadowRows = (ShadowRow *)malloc(sizeof(ShadowRow) * (size_t)height);
	s_shadowKeep = (ShadowRow *)malloc(sizeof(ShadowRow) * (size_t)height);
	if (!s_shadow || !s_shadowRows || !s_shadowKeep)
	{
		free(s_shadow);
		free(s_shadowRows);
		free(s_shadowKeep);
		s_shadow     = NULL;
		s_shadowRows = NULL;
		s_shadowKeep = NULL;
	}
	else
	{
		int y;
		for (y = 0; y < height; ++y)
		{
			s_shadowRows[y].buf = s_shadow + (size_t)y * (size_t)(width + 2 * SHADOW_SLACK);
			s_shadowRows[y].off = 0;
		}
	}
	s_shadowOff = (s_shadow == NULL);

#if defined(CTP2_AMIGA_WOS)
	s_touchFor68k = uaegfx_card();
	if (s_touchFor68k)
	{
		amiga_log("Screen: uaegfx.card (Amiberry): the 68k marks each copy for the display\n");
	}
#endif

	clear_card(sw, sh);

	ScreenToFront(s_screen);
	ActivateWindow(s_window);

	amiga_log("Screen: mode $%08lx, %dx%d, pixel format %lu, picture %dx%d at %d,%d\n",
	          (unsigned long)id, sw, sh, (unsigned long)s_pixfmt, width, height, s_ox, s_oy);
	return 1;
}

void amiga_video_close(void)
{
	if (s_window)
	{
		ClearPointer(s_window);
		CloseWindow(s_window);
		s_window = NULL;
	}
	if (s_screen)
	{
		CloseScreen(s_screen);
		s_screen = NULL;
	}
	if (s_blankPointer)
	{
		FreeVec(s_blankPointer);
		s_blankPointer = NULL;
	}
	free(s_lineBuf);
	s_lineBuf = NULL;
	free(s_shadow);
	free(s_shadowRows);
	free(s_shadowKeep);
	s_shadow     = NULL;
	s_shadowRows = NULL;
	s_shadowKeep = NULL;
}

int amiga_video_width(void)  { return s_width; }
int amiga_video_height(void) { return s_height; }

int amiga_video_dump_card(const char *path)
{
	APTR    handle;
	ULONG   base = 0, bpr = 0;
	FILE *  f;
	uint8_t * line;
	int     x, y, ok = 0;

	if (!s_screen) return 0;
	f = fopen(path, "wb");
	if (!f) return 0;
	line = (uint8_t *)malloc((size_t)s_width * 3);
	if (!line) { fclose(f); return 0; }
	fprintf(f, "P6\n%d %d\n255\n", s_width, s_height);

	{
		struct TagItem tags[] =
		{
			{ LBMI_BASEADDRESS, (ULONG)&base },
			{ LBMI_BYTESPERROW, (ULONG)&bpr },
			{ TAG_DONE,         0 }
		};
		handle = LockBitMapTagList(s_screen->RastPort.BitMap, tags);
	}
	if (handle && base)
	{
		for (y = 0; y < s_height; ++y)
		{
			const uint16_t * row = (const uint16_t *)((const uint8_t *)base + (size_t)(s_oy + y) * bpr) + s_ox;
			for (x = 0; x < s_width; ++x)
			{
				uint16_t p = row[x];
				/* Back to R5G6B5 from the card's layouts. */
				switch (s_pixfmt)
				{
				case PIXFMT_RGB16PC: p = (uint16_t)((p << 8) | (p >> 8)); break;
				case PIXFMT_BGR16:   p = (uint16_t)((p & 0x07E0) | (p << 11) | (p >> 11)); break;
				case PIXFMT_BGR16PC: p = (uint16_t)((p << 8) | (p >> 8));
				                     p = (uint16_t)((p & 0x07E0) | (p << 11) | (p >> 11)); break;
				default: break;
				}
				line[x * 3 + 0] = (uint8_t)(((p >> 11) & 0x1F) << 3);
				line[x * 3 + 1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
				line[x * 3 + 2] = (uint8_t)((p & 0x1F) << 3);
			}
			/* Out of the lock before the file system gets the row. */
			UnLockBitMap(handle);
			fwrite(line, 1, (size_t)s_width * 3, f);
			{
				struct TagItem tags[] =
				{
					{ LBMI_BASEADDRESS, (ULONG)&base },
					{ LBMI_BYTESPERROW, (ULONG)&bpr },
					{ TAG_DONE,         0 }
				};
				handle = LockBitMapTagList(s_screen->RastPort.BitMap, tags);
			}
			if (!handle || !base) break;
		}
		if (handle) UnLockBitMap(handle);
		ok = (y == s_height);
	}
	else if (handle)
	{
		UnLockBitMap(handle);
	}
	fclose(f);

	/* And the shadow (what the copies believe the card shows). */
	if (s_shadow && (f = fopen("RAM:ctp2shadow.ppm", "wb")) != NULL)
	{
		fprintf(f, "P6\n%d %d\n255\n", s_width, s_height);
		for (y = 0; y < s_height; ++y)
		{
			const uint16_t * row = SHADOW(y);
			for (x = 0; x < s_width; ++x)
			{
				uint16_t const p = row[x];
				line[x * 3 + 0] = (uint8_t)(((p >> 11) & 0x1F) << 3);
				line[x * 3 + 1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
				line[x * 3 + 2] = (uint8_t)((p & 0x1F) << 3);
			}
			fwrite(line, 1, (size_t)s_width * 3, f);
		}
		fclose(f);
	}
	free(line);
	return ok;
}

void amiga_video_hide_pointer(void)
{
	if (!s_window) return;

	/* An empty 1x1 sprite, in chip memory as SetPointer wants. */
	if (!s_blankPointer)
	{
		s_blankPointer = (UWORD *)AllocVec(6 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	}
	if (s_blankPointer)
	{
		SetPointer(s_window, s_blankPointer, 1, 16, 0, 0);
	}
}

/* ---- The copy -------------------------------------------------------------- */

/* One row of n R5G6B5 pixels into the card's format. dst need not be
 * aligned beyond words; pairs go out as longwords where they can. */
static void convert_row(uint8_t *dst, const uint16_t *src, int n)
{
	uint16_t * d = (uint16_t *)dst;
	int i;

	switch (s_pixfmt)
	{
	case PIXFMT_RGB16:
		memcpy(dst, src, (size_t)n * 2);
		return;

	case PIXFMT_RGB16PC:
		i = 0;
		if (((uintptr_t)d & 2) && n > 0)
		{
			uint16_t p = src[0];
			d[0] = (uint16_t)((p << 8) | (p >> 8));
			i = 1;
		}
		if ((((uintptr_t)(src + i)) & 2) == 0)
		{
			/* Both aligned (the usual case: even x on both sides): whole
			 * longwords in and out, two of them a turn. */
			const uint32_t * s32 = (const uint32_t *)(src + i);
			uint32_t *       d32 = (uint32_t *)(d + i);
			uint32_t const   mask = 0x00FF00FFu;
			int              pairs = (n - i) >> 1;

			i += pairs * 2;
			for (; pairs >= 2; pairs -= 2)
			{
				uint32_t a = s32[0], b = s32[1];
				s32 += 2;
				d32[0] = ((a & mask) << 8) | ((a >> 8) & mask);
				d32[1] = ((b & mask) << 8) | ((b >> 8) & mask);
				d32 += 2;
			}
			if (pairs)
			{
				uint32_t a = *s32;
				*d32 = ((a & mask) << 8) | ((a >> 8) & mask);
			}
		}
		else
		{
			for (; i + 1 < n; i += 2)
			{
				uint32_t v = ((uint32_t)src[i] << 16) | src[i + 1];
				*(uint32_t *)(d + i) = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
			}
		}
		if (i < n)
		{
			uint16_t p = src[i];
			d[i] = (uint16_t)((p << 8) | (p >> 8));
		}
		return;

	case PIXFMT_BGR16:
		for (i = 0; i < n; ++i)
		{
			uint16_t p = src[i];
			d[i] = (uint16_t)((p & 0x07E0) | (p << 11) | (p >> 11));
		}
		return;

	case PIXFMT_BGR16PC:
		for (i = 0; i < n; ++i)
		{
			uint16_t p = src[i];
			p = (uint16_t)((p & 0x07E0) | (p << 11) | (p >> 11));
			d[i] = (uint16_t)((p << 8) | (p >> 8));
		}
		return;

	case PIXFMT_RGB15:
		for (i = 0; i < n; ++i)
		{
			uint16_t p = src[i];
			d[i] = (uint16_t)(((p >> 1) & 0x7FE0) | (p & 0x1F));
		}
		return;

	case PIXFMT_RGB15PC:
		for (i = 0; i < n; ++i)
		{
			uint16_t p = src[i];
			p = (uint16_t)(((p >> 1) & 0x7FE0) | (p & 0x1F));
			d[i] = (uint16_t)((p << 8) | (p >> 8));
		}
		return;

	default:
		return;
	}
}

/* Without a lock (a driver that refuses it): WritePixelArray, which knows no
 * 16-bit source format in CyberGraphX V41, so the rows go as RGB24. Slow, but
 * only a fallback. */
static void write_rect_fallback(const uint16_t *pixels, int pitch, int x, int y, int w, int h)
{
	int row;

	/* Sized once for the widest RTG screen, not for the first caller (which
	 * may be a cursor-sized patch). */
	if (!s_lineBuf) s_lineBuf = (uint8_t *)malloc(LINEBUF_PIXELS * 3);
	if (!s_lineBuf) return;
	if (w > LINEBUF_PIXELS) w = LINEBUF_PIXELS;

	for (row = 0; row < h; ++row)
	{
		const uint16_t * s = (const uint16_t *)((const uint8_t *)pixels + (size_t)(y + row) * pitch) + x;
		uint8_t *        d = s_lineBuf;
		int i;
		for (i = 0; i < w; ++i)
		{
			uint16_t p = s[i];
			*d++ = (uint8_t)(((p >> 11) & 0x1F) << 3);
			*d++ = (uint8_t)(((p >> 5)  & 0x3F) << 2);
			*d++ = (uint8_t)((p & 0x1F) << 3);
		}
		WritePixelArray(s_lineBuf, 0, 0, (UWORD)(w * 3), &s_screen->RastPort,
		                (UWORD)(s_ox + x), (UWORD)(s_oy + y + row), (UWORD)w, 1, RECTFMT_RGB);
	}
}

static int clip(int *x, int *y, int *w, int *h)
{
	if (*x < 0) { *w += *x; *x = 0; }
	if (*y < 0) { *h += *y; *y = 0; }
	if (*x + *w > s_width)  *w = s_width  - *x;
	if (*y + *h > s_height) *h = s_height - *y;
	return *w > 0 && *h > 0;
}

/* The diff (as OpenSWOS's 68060 build): the 68060 reads fast RAM many times
 * faster than it writes a Zorro III card (~2.3 Mpixels/s on a Radeon), so a
 * row goes out only in the SEG-pixel segments that differ from the shadow,
 * which takes them. Segments sit on absolute multiples of SEG, so a run starts
 * on an even pixel and convert_row writes longwords. */
#define SEG 16

static uint32_t s_statPixels;

/* Whether n pixels differ. A whole aligned segment as eight longwords; the
 * ends of a row (and odd addresses) pixel by pixel. */
static inline int segment_differs(const uint16_t *a, const uint16_t *b, int n)
{
	if (n == SEG && (((uintptr_t)a | (uintptr_t)b) & 3) == 0)
	{
		/* XOR and OR without branches: 0.10 us a pixel on the A4000's
		 * 68060, where the early-out version took 0.17. */
		const uint32_t * a32 = (const uint32_t *)a;
		const uint32_t * b32 = (const uint32_t *)b;
		return ((a32[0] ^ b32[0]) | (a32[1] ^ b32[1]) | (a32[2] ^ b32[2]) | (a32[3] ^ b32[3])
		      | (a32[4] ^ b32[4]) | (a32[5] ^ b32[5]) | (a32[6] ^ b32[6]) | (a32[7] ^ b32[7])) != 0;
	}
	while (n-- > 0)
	{
		if (*a++ != *b++) return 1;
	}
	return 0;
}

/* A present goes in two steps: first the rows are compared with the shadow in
 * fast RAM and the differing runs noted (the shadow takes them at once), then
 * the card's pending shifts are blitted and the runs written, back to back.
 * So a scroll step reaches the screen within a few milliseconds of the
 * blitter's shift (the compare of the whole viewport, ~70 ms on an A4000,
 * comes before it) and is not seen being drawn. */
typedef struct { int32_t y, x, n, s; } Run;   /* s: which source (present_rects) */
static Run *  s_runs;
static size_t s_runCount, s_runMax;

static int s_runSrc;      /* the source being compared */

static void add_run(int y, int x, int n)
{
	if (s_runCount && s_runs[s_runCount - 1].y == y && s_runs[s_runCount - 1].s == s_runSrc
	 && s_runs[s_runCount - 1].x + s_runs[s_runCount - 1].n == x)
	{
		s_runs[s_runCount - 1].n += n;
		return;
	}
	if (s_runCount == s_runMax)
	{
		size_t const max = s_runMax ? s_runMax * 2 : 4096;
		Run * runs = (Run *)realloc(s_runs, max * sizeof(Run));
		if (!runs) return;   /* the run is lost; the scrub mends it */
		s_runs   = runs;
		s_runMax = max;
	}
	s_runs[s_runCount].y = y;
	s_runs[s_runCount].x = x;
	s_runs[s_runCount].n = n;
	s_runs[s_runCount].s = s_runSrc;
	++s_runCount;
}

/* One row of n pixels at picture column x, row y: src against the shadow;
 * the differing runs are noted and go into the shadow. Segment i of the row
 * covers [seg_start, seg_end): the first one ends at the next multiple of SEG,
 * the last one at n. */
static void compare_row(const uint16_t *src, uint16_t *shadow, int x, int y, int n)
{
	int i   = 0;
	int run = -1;       /* start of the differing run, -1: none */

	while (i < n)
	{
		int len = SEG - ((x + i) & (SEG - 1));
		if (len > n - i) len = n - i;

		if (segment_differs(src + i, shadow + i, len))
		{
			if (run < 0) run = i;
		}
		else if (run >= 0)
		{
			memcpy(shadow + run, src + run, (size_t)(i - run) * 2);
			add_run(y, x + run, i - run);
			run = -1;
		}
		i += len;
	}

	if (run >= 0)
	{
		memcpy(shadow + run, src + run, (size_t)(n - run) * 2);
		add_run(y, x + run, n - run);
	}
}

/* Shifts wait for the next full copy: until then the composed frame is the
 * one from before the shift, and a cursor-only copy (amiga_video_cursor_only)
 * must find the card showing that same frame. */
typedef struct { int x, y, w, h, dx, dy; } PendingScroll;
static PendingScroll s_pending[16];
static int           s_pendingCount;
static int           s_cursorOnly;
static int           s_forceTop, s_forceBottom;   /* rows sent whole, once */

void amiga_video_force_rows(int top, int bottom)
{
	s_forceTop    = top;
	s_forceBottom = bottom;
}

static void scroll_now(int x, int y, int w, int h, int dx, int dy, int card, int shadow);

/* Rectangles whose pixels may differ between the frame and the shadow once
 * the pending shifts are done (the cursor's, sent while they waited): the
 * next full copy compares them where they are and where the shift takes
 * them. With the frame shifted by the game too (amiga_ui_scroll_frame), the
 * copy compares only what the game marks, so these must not be missed. */
#define MAX_TOUCHED 32
static AmigaRect s_touched[MAX_TOUCHED];
static int       s_touchedCount;
static int       s_touchedOverflow;

void amiga_video_touch(int left, int top, int right, int bottom)
{
	if (!s_pendingCount || left >= right || top >= bottom) return;
	if (s_touchedCount == MAX_TOUCHED) { s_touchedOverflow = 1; return; }
	s_touched[s_touchedCount].left   = left;
	s_touched[s_touchedCount].top    = top;
	s_touched[s_touchedCount].right  = right;
	s_touched[s_touchedCount].bottom = bottom;
	++s_touchedCount;
}

int amiga_video_shift_pending(void)
{
	return s_pendingCount != 0;
}

int amiga_video_scroll(int x, int y, int w, int h, int dx, int dy)
{
	/* What scroll_now would refuse, refused here, where the caller hears. */
	{
		int cx = x, cy = y, cw = w, ch = h;
		int const adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
		if (!s_screen || !s_shadow || s_shadowOff || (dx == 0 && dy == 0)) return 0;
		if (!clip(&cx, &cy, &cw, &ch) || adx >= cw || ady >= ch) return 0;
	}
	if (s_pendingCount == (int)(sizeof(s_pending) / sizeof(s_pending[0])))
	{
		/* Too many before a copy: these go now, the rest is the diff's. */
		int i;
		for (i = 0; i < s_pendingCount; ++i)
		{
			PendingScroll const * p = &s_pending[i];
			scroll_now(p->x, p->y, p->w, p->h, p->dx, p->dy, 1, 1);
		}
		s_pendingCount = 0;
	}
	s_pending[s_pendingCount].x  = x;
	s_pending[s_pendingCount].y  = y;
	s_pending[s_pendingCount].w  = w;
	s_pending[s_pendingCount].h  = h;
	s_pending[s_pendingCount].dx = dx;
	s_pending[s_pendingCount].dy = dy;
	++s_pendingCount;
	return 1;
}

void amiga_video_cursor_only(int on)
{
	s_cursorOnly = on;
}

/* The card's copy (card) and the shadow (shadow) shifted. */
static void scroll_now(int x, int y, int w, int h, int dx, int dy, int card, int shadow)
{
	int adx, ady, sx, sy, tx, ty, cw, ch, row;

	if (!s_screen || !s_shadow || s_shadowOff || (dx == 0 && dy == 0)) return;
	if (!clip(&x, &y, &w, &h)) return;

	adx = dx < 0 ? -dx : dx;
	ady = dy < 0 ? -dy : dy;
	if (adx >= w || ady >= h) return;

	cw = w - adx;
	ch = h - ady;
	{
		static int s_logged = 0;
		if (amiga_perf && s_logged < 4)
		{
			++s_logged;
			amiga_log("Shift: %d,%d %dx%d by %d,%d (card %d, shadow %d)\n", x, y, w, h, dx, dy, card, shadow);
		}
	}
	sx = x + (dx > 0 ? dx : 0);     /* from */
	sy = y + (dy > 0 ? dy : 0);
	tx = x + (dx < 0 ? adx : 0);    /* to */
	ty = y + (dy < 0 ? ady : 0);

	/* The card: its blitter (Picasso96 copies overlapping areas right). The
	 * next LockBitMap waits for it. */
	if (card)
	{
		BltBitMap(s_screen->RastPort.BitMap, (LONG)(s_ox + sx), (LONG)(s_oy + sy),
		          s_screen->RastPort.BitMap, (LONG)(s_ox + tx), (LONG)(s_oy + ty),
		          (LONG)cw, (LONG)ch, 0xC0, 0xFF, NULL);
	}

	/* The shadow, row by row in the order that keeps the source intact. The
	 * strip left behind holds the old pixels on both sides. */
	if (!shadow) return;

	/* The whole width: the rows' starts move, or the rows trade places.
	 * The strip the shift uncovers then gets the pixels the card still
	 * shows there (its blitter leaves the old ones), which now lie a shift
	 * further on: otherwise a picture that happens to equal what the slack
	 * held there would never be sent. */
	if (x == 0 && w == s_width && 2 * adx <= w && 2 * ady <= h)
	{
		if (dx)
		{
			for (row = y; row < y + h; ++row)
			{
				ShadowRow * r = &s_shadowRows[row];
				int const   off = r->off + dx;
				if (off < -SHADOW_SLACK || off > SHADOW_SLACK)
				{
					/* Out of slack: the row back to the middle first. */
					memmove(r->buf + SHADOW_SLACK, r->buf + SHADOW_SLACK + r->off, (size_t)s_width * 2);
					r->off = 0;
				}
				r->off += dx;
				{
					uint16_t * p = r->buf + SHADOW_SLACK + r->off;
					if (dx > 0) amiga_copy_up(p + w - adx, p + w - 2 * adx, adx);
					else        amiga_copy_up(p, p + adx, adx);
				}
			}
		}
		else
		{
			/* Rows [y, y + h) rotated by dy: row t takes row t + dy, the
			 * ones uncovered take the rows that fell out. */
			/* (Nothing here may fail: the frame and the card go along.) */
			ShadowRow *keep = s_shadowKeep;
			if (dy > 0)
			{
				memcpy(keep, &s_shadowRows[y], sizeof(ShadowRow) * (size_t)ady);
				memmove(&s_shadowRows[y], &s_shadowRows[y + ady], sizeof(ShadowRow) * (size_t)(h - ady));
				memcpy(&s_shadowRows[y + h - ady], keep, sizeof(ShadowRow) * (size_t)ady);
			}
			else
			{
				memcpy(keep, &s_shadowRows[y + h - ady], sizeof(ShadowRow) * (size_t)ady);
				memmove(&s_shadowRows[y + ady], &s_shadowRows[y], sizeof(ShadowRow) * (size_t)(h - ady));
				memcpy(&s_shadowRows[y], keep, sizeof(ShadowRow) * (size_t)ady);
			}

			for (row = 0; row < ady; ++row)
			{
				if (dy > 0)
					amiga_copy_up(SHADOW(y + h - ady + row), SHADOW(y + h - 2 * ady + row), w);
				else
					amiga_copy_up(SHADOW(y + row), SHADOW(y + ady + row), w);
			}
		}
		return;
	}

	if (ty <= sy)
	{
		for (row = 0; row < ch; ++row)
			amiga_shift_row(SHADOW(ty + row) + tx, SHADOW(sy + row) + sx, cw, adx);
	}
	else
	{
		for (row = ch - 1; row >= 0; --row)
			amiga_shift_row(SHADOW(ty + row) + tx, SHADOW(sy + row) + sx, cw, adx);
	}
}

/* n pixels from src to dst within one picture, as memmove, but always going
 * upwards in memory, which the 68060 does in bursts (memmove copies down when
 * dst lies above src, at about half the speed). A row shifted right (dst =
 * src + shift on the same row) goes in chunks of shift pixels from its right
 * end: a chunk never overlaps its own source. Rows apart never overlap. */
void amiga_shift_row(uint16_t *dst, const uint16_t *src, int n, int shift)
{
	if (dst > src && dst < src + n)
	{
		int end;
		if (shift <= 0) shift = (int)(dst - src);
		for (end = n; end > 0; end -= shift)
		{
			int const start = end > shift ? end - shift : 0;
			amiga_copy_up(dst + start, src + start, end - start);
		}
		return;
	}
	amiga_copy_up(dst, src, n);
}

/* n pixels upwards; longwords when both are aligned alike. */
void amiga_copy_up(uint16_t *dst, const uint16_t *src, int n)
{
	if (n <= 0) return;
	if ((((uintptr_t)dst ^ (uintptr_t)src) & 3) == 0)
	{
		uint32_t *       d;
		const uint32_t * s;
		int              longs;
		if ((uintptr_t)dst & 2)
		{
			*dst++ = *src++;
			--n;
		}
		d = (uint32_t *)dst;
		s = (const uint32_t *)src;
		for (longs = n >> 1; longs > 0; --longs) *d++ = *s++;
		if (n & 1) *(uint16_t *)d = *(const uint16_t *)s;
		return;
	}
	while (n-- > 0) *dst++ = *src++;
}

static uint64_t s_statUs;
static uint32_t s_statCalls;

void amiga_video_stats(uint32_t *pixels, uint32_t *us, uint32_t *calls)
{
	*pixels = s_statPixels;
	*us     = (uint32_t)s_statUs;
	*calls  = s_statCalls;
	s_statPixels = 0;
	s_statUs     = 0;
	s_statCalls  = 0;
}

int amiga_video_has_shadow(void)
{
	return s_screen && s_shadow && !s_shadowOff;
}

int amiga_video_read_shadow(int x, int y, int w, int h, uint16_t *out)
{
	int row;
	if (!amiga_video_has_shadow()) return 0;
	if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > s_width || y + h > s_height) return 0;
	for (row = 0; row < h; ++row)
		memcpy(out + (size_t)row * w, SHADOW(y + row) + x, (size_t)w * 2);
	return 1;
}

/* Rectangles of the next full copy that go out whole, past the compare (a
 * scroll's new strip, ENV:CTP2_STRIPWRITE). */
#define MAX_FORCE 16
static AmigaRect s_force[MAX_FORCE];
static int       s_forceCount;

void amiga_video_force_rect(int left, int top, int right, int bottom)
{
	if (s_forceCount == MAX_FORCE || left >= right || top >= bottom) return;
	s_force[s_forceCount].left   = left;
	s_force[s_forceCount].top    = top;
	s_force[s_forceCount].right  = right;
	s_force[s_forceCount].bottom = bottom;
	++s_forceCount;
}

int amiga_video_take_touched(AmigaRect *out, int max)
{
	int n = 0, r, k;

	if (s_pendingCount && !s_touchedOverflow
	 && s_touchedCount * (1 + s_pendingCount) <= max)
	{
		/* Where it is, and where each shift in turn takes it: the shifts
		 * follow one another on the card. */
		for (r = 0; r < s_touchedCount; ++r)
		{
			AmigaRect a = s_touched[r];
			out[n++] = a;
			for (k = 0; k < s_pendingCount; ++k)
			{
				PendingScroll const * p = &s_pending[k];
				a.left -= p->dx; a.right  -= p->dx;
				a.top  -= p->dy; a.bottom -= p->dy;
				out[n++] = a;
			}
		}
	}
	else if (s_pendingCount && (s_touchedCount || s_touchedOverflow))
	{
		/* Too many: all of the shifted areas. */
		for (k = 0; k < s_pendingCount && n < max; ++k)
		{
			PendingScroll const * p = &s_pending[k];
			AmigaRect a = { p->x, p->y, p->x + p->w, p->y + p->h };
			out[n++] = a;
		}
	}
	s_touchedCount    = 0;
	s_touchedOverflow = 0;
	return n;
}

static void present_rects(const AmigaSource *srcs, int nsrc);

void amiga_video_present_multi(const AmigaSource *srcs, int nsrc)
{
	uint64_t const start = amiga_ticks_us();
	present_rects(srcs, nsrc);
	if (!s_cursorOnly)
	{
		s_forceTop = s_forceBottom = 0;
		s_forceCount = 0;
	}
	s_statUs += amiga_ticks_us() - start;
	++s_statCalls;
}

void amiga_video_present(const uint16_t *pixels, int pitch, const AmigaRect *rects, int count)
{
	AmigaSource src;
	src.pixels = pixels;
	src.pitch  = pitch;
	src.rects  = rects;
	src.count  = count;
	amiga_video_present_multi(&src, 1);
}

/* n pixels of a row: whole (the shadow takes them, a run is noted) or
 * compared. */
static void present_span(const uint16_t *src, int x, int y, int n, int whole)
{
	if (n <= 0) return;
	if (whole)
	{
		if (!s_shadowOff) memcpy(SHADOW(y) + x, src, (size_t)n * 2);
		add_run(y, x, n);
	}
	else
	{
		compare_row(src, SHADOW(y) + x, x, y, n);
	}
}

/* Step 1 of a copy: a source's rectangles compared with the shadow, the
 * differing runs noted (s_runs), the shadow updated. */
static void present_compare(const AmigaSource *source)
{
	int r, f;
	AMIGA_PERF_BEGIN(tDiff);
	for (r = 0; r < source->count; ++r)
	{
		int x = source->rects[r].left, y = source->rects[r].top;
		int w = source->rects[r].right - x, h = source->rects[r].bottom - y;
		int row;
		if (!clip(&x, &y, &w, &h)) continue;

		for (row = 0; row < h; ++row)
		{
			int const        yy  = y + row;
			const uint16_t * src = (const uint16_t *)((const uint8_t *)source->pixels + (size_t)yy * source->pitch) + x;

			if (s_shadowOff || (yy >= s_forceTop && yy < s_forceBottom))
			{
				present_span(src, x, yy, w, 1);
				continue;
			}

			/* The first rectangle to go out whole that this row meets. */
			for (f = 0; f < s_forceCount; ++f)
			{
				if (yy >= s_force[f].top && yy < s_force[f].bottom
				 && s_force[f].left < x + w && s_force[f].right > x)
				{
					break;
				}
			}
			if (f < s_forceCount)
			{
				int const a = s_force[f].left  > x     ? s_force[f].left  : x;
				int const b = s_force[f].right < x + w ? s_force[f].right : x + w;
				present_span(src,           x, yy, a - x,     0);
				present_span(src + (a - x), a, yy, b - a,     1);
				present_span(src + (b - x), b, yy, x + w - b, 0);
			}
			else
			{
				present_span(src, x, yy, w, 0);
			}
		}
	}
	AMIGA_PERF_END(tDiff, AMIGA_PERF_CARD_DIFF);
}

static void present_rects(const AmigaSource *srcs, int nsrc)
{
	APTR    handle;
	ULONG   base = 0, bpr = 0;
	int     r, k, pending = 0;
	size_t  i;

	if (!s_screen) return;

	if (s_cursorOnly && s_pendingCount)
	{
		for (k = 0; k < nsrc; ++k)
			for (r = 0; r < srcs[k].count; ++r)
				amiga_video_touch(srcs[k].rects[r].left, srcs[k].rects[r].top,
				                  srcs[k].rects[r].right, srcs[k].rects[r].bottom);
	}

	/* 1: the shadow shifted, the rows compared, the runs noted. (What was
	 * touched while the shifts waited is among the rectangles: the caller
	 * took it with amiga_video_take_touched.) */
	if (!s_cursorOnly && s_pendingCount)
	{
		AMIGA_PERF_BEGIN(t0);
		for (r = 0; r < s_pendingCount; ++r)
		{
			PendingScroll const * p = &s_pending[r];
			scroll_now(p->x, p->y, p->w, p->h, p->dx, p->dy, 0, 1);
		}
		pending = s_pendingCount;
		s_pendingCount = 0;
		s_touchedCount = 0;
		s_touchedOverflow = 0;
		AMIGA_PERF_END(t0, AMIGA_PERF_CARD_SHIFT);
	}

	s_runCount = 0;
	for (k = 0; k < nsrc; ++k)
	{
		s_runSrc = k;
		present_compare(&srcs[k]);
	}

	/* 2: the card's shifts and the runs, back to back. */
	for (r = 0; r < pending; ++r)
	{
		PendingScroll const * p = &s_pending[r];
		scroll_now(p->x, p->y, p->w, p->h, p->dx, p->dy, 1, 0);
	}
	if (!s_runCount) return;

	{
		struct TagItem tags[] =
		{
			{ LBMI_BASEADDRESS, (ULONG)&base },
			{ LBMI_BYTESPERROW, (ULONG)&bpr },
			{ TAG_DONE,         0 }
		};
		AMIGA_PERF_BEGIN(t0);
		handle = LockBitMapTagList(s_screen->RastPort.BitMap, tags);
		AMIGA_PERF_END(t0, AMIGA_PERF_CARD_LOCK);
	}

	if (handle && base)
	{
		/* Between lock and unlock: only the copy. */
		AMIGA_PERF_BEGIN(t0);
		for (i = 0; i < s_runCount; ++i)
		{
			Run const *         run    = &s_runs[i];
			AmigaSource const * source = &srcs[run->s];
			const uint16_t * src = (const uint16_t *)((const uint8_t *)source->pixels + (size_t)run->y * source->pitch) + run->x;
			uint8_t *        dst = (uint8_t *)base + (size_t)(s_oy + run->y) * bpr + (size_t)(s_ox + run->x) * 2;
			convert_row(dst, src, run->n);
			s_statPixels += (uint32_t)run->n;
		}
		AMIGA_PERF_END(t0, AMIGA_PERF_WRITE);
#if defined(CTP2_AMIGA_WOS)
		if (s_touchFor68k)
		{
			for (k = 0; k < nsrc; ++k)
			{
				for (r = 0; r < srcs[k].count; ++r)
				{
					int x = srcs[k].rects[r].left, y = srcs[k].rects[r].top;
					int w = srcs[k].rects[r].right - x, h = srcs[k].rects[r].bottom - y;
					uint8_t * first;
					if (!clip(&x, &y, &w, &h)) continue;
					first = (uint8_t *)base + (size_t)(s_oy + y) * bpr + (size_t)(s_ox + x) * 2;
					CopyMem(first, first, (ULONG)((size_t)(h - 1) * bpr + (size_t)w * 2));
				}
			}
		}
#endif
		UnLockBitMap(handle);
		return;
	}

	if (handle) UnLockBitMap(handle);

	if (!s_lockFailedLogged)
	{
		amiga_log("LockBitMap failed: copying with WritePixelArray (slow)\n");
		s_lockFailedLogged = 1;
	}
	s_shadowOff = 1;

	for (k = 0; k < nsrc; ++k)
	{
		for (r = 0; r < srcs[k].count; ++r)
		{
			int x = srcs[k].rects[r].left, y = srcs[k].rects[r].top;
			int w = srcs[k].rects[r].right - x, h = srcs[k].rects[r].bottom - y;
			if (clip(&x, &y, &w, &h)) write_rect_fallback(srcs[k].pixels, srcs[k].pitch, x, y, w, h);
		}
	}
}

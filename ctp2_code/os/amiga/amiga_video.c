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

#if defined(CTP2_AMIGA_WOS)
	s_touchFor68k = uaegfx_card();
	if (s_touchFor68k)
	{
		amiga_log("Screen: uaegfx.card (Amiberry): the 68k marks each copy for the display\n");
	}
#endif

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
}

int amiga_video_width(void)  { return s_width; }
int amiga_video_height(void) { return s_height; }

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

void amiga_video_present(const uint16_t *pixels, int pitch, const AmigaRect *rects, int count)
{
	APTR    handle;
	ULONG   base = 0, bpr = 0;
	int     r;

	if (!s_screen || count <= 0) return;

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
		/* Between lock and unlock: only the copy. */
		for (r = 0; r < count; ++r)
		{
			int x = rects[r].left, y = rects[r].top;
			int w = rects[r].right - x, h = rects[r].bottom - y;
			int row;
			if (!clip(&x, &y, &w, &h)) continue;

			for (row = 0; row < h; ++row)
			{
				const uint16_t * s = (const uint16_t *)((const uint8_t *)pixels + (size_t)(y + row) * pitch) + x;
				uint8_t *        d = (uint8_t *)base + (size_t)(s_oy + y + row) * bpr + (size_t)(s_ox + x) * 2;
				convert_row(d, s, w);
			}
#if defined(CTP2_AMIGA_WOS)
			if (s_touchFor68k)
			{
				uint8_t * first = (uint8_t *)base + (size_t)(s_oy + y) * bpr + (size_t)(s_ox + x) * 2;
				CopyMem(first, first, (ULONG)((size_t)(h - 1) * bpr + (size_t)w * 2));
			}
#endif
		}
		UnLockBitMap(handle);
		return;
	}

	if (handle) UnLockBitMap(handle);

	if (!s_lockFailedLogged)
	{
		amiga_log("LockBitMap failed: copying with WritePixelArray (slow)\n");
		s_lockFailedLogged = 1;
	}

	for (r = 0; r < count; ++r)
	{
		int x = rects[r].left, y = rects[r].top;
		int w = rects[r].right - x, h = rects[r].bottom - y;
		if (clip(&x, &y, &w, &h)) write_rect_fallback(pixels, pitch, x, y, w, h);
	}
}

void amiga_video_present_patch(const uint16_t *pixels, int pitch, int x, int y, int width, int height)
{
	AmigaRect rect;

	/* The same copy, with the patch seen as a frame whose (0,0) is (x,y). */
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = width;
	rect.bottom = height;

	{
		int const ox = s_ox, oy = s_oy, sw = s_width, sh = s_height;
		s_ox += x;
		s_oy += y;
		s_width  = width;
		s_height = height;
		amiga_video_present(pixels, pitch, &rect, 1);
		s_ox = ox;
		s_oy = oy;
		s_width  = sw;
		s_height = sh;
	}
}

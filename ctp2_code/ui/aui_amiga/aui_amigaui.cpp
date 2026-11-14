//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Amiga backend: the UI, its RTG screen and the frame copy
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

#include "ctp2_config.h"
#include "c3.h"

#ifdef __AUI_USE_AMIGA__

#include "aui_amigaui.h"

#include "aui_cursor.h"
#include "aui_mouse.h"
#include "aui_keyboard.h"
#include "aui_joystick.h"
#include "aui_amigasurface.h"
#include "aui_window.h"
#include "aui_amigamouse.h"
#include "aui_amigakeyboard.h"
#include "civ3_main.h"
#include "civapp.h"
#include "keypress.h"                   // ui_HandleKeypress
#include "initialplaywindow.h"          // initialplayscreen_QuickStart
#include "profileDB.h"                  // g_theProfileDB

#include "amiga_platform.h"
#include "amiga_keycodes.h"

extern CivApp *     g_civApp;
extern void         ui_HandleMouseWheel(sint16 delta);

namespace
{
	inline bool RectEmpty(const RECT &r)
	{
		return r.right <= r.left || r.bottom <= r.top;
	}

	inline bool RectIntersect(RECT &out, const RECT &a, const RECT &b)
	{
		out.left   = std::max(a.left,   b.left);
		out.top    = std::max(a.top,    b.top);
		out.right  = std::min(a.right,  b.right);
		out.bottom = std::min(a.bottom, b.bottom);
		return !RectEmpty(out);
	}
}

aui_AmigaUI::aui_AmigaUI
(
	AUI_ERRCODE *retval,
	HINSTANCE hinst,
	HWND hwnd,
	sint32 width,
	sint32 height,
	sint32 bpp,
	const MBCHAR *ldlFilename,
	BOOL useExclusiveMode
)
:
	aui_UI              (),
	aui_Amiga           (),
	m_present           (),
	m_presentAll        (false),
	m_cursorImage       (NULL),
	m_cursorImageSize   (0),
	m_cursorWidth       (0),
	m_cursorHeight      (0),
	m_cursorKey         (0),
	m_cursorOwner       (NULL),
	m_cursorHotX        (0),
	m_cursorHotY        (0),
	m_cursorX           (0),
	m_cursorY           (0),
	m_cursorShown       (),
	m_cursorVisible     (false),
	m_busyTick          (false),
	m_composite         (NULL),
	m_compositeMap      (NULL),
	m_under             (NULL),
	m_patchOld          (NULL),
	m_patchNew          (NULL),
	m_underNew          (NULL),
	m_compositeSize     (0),
	m_underValid        (false),
	m_cursorChanged     (false),
	m_directWindow      (NULL),
	m_directOn          (false),
	m_open              ()
{
	m_cursorShown.left = m_cursorShown.top = m_cursorShown.right = m_cursorShown.bottom = 0;

	*retval = aui_Region::InitCommon( 0, 0, 0, width, height );
	Assert( AUI_SUCCESS(*retval) );
	if ( !AUI_SUCCESS(*retval) ) return;

	Assert( aui_Base::GetBaseRefCount() == 2 );
	g_ui = aui_Base::GetBaseRefCount() == 2 ? this : NULL;

	*retval = aui_UI::InitCommon( hinst, hwnd, bpp, ldlFilename );
	Assert( AUI_SUCCESS(*retval) );
	if ( !AUI_SUCCESS(*retval) ) return;

	*retval = InitCommon();
	Assert( AUI_SUCCESS(*retval) );
	if ( !AUI_SUCCESS(*retval) ) return;

	*retval = CreateNativeScreen( useExclusiveMode );
	Assert( AUI_SUCCESS(*retval) );
	if ( !AUI_SUCCESS(*retval) ) return;
}

AUI_ERRCODE aui_AmigaUI::InitCommon()
{
	m_savedMouseAnimFirstIndex = 0;
	m_savedMouseAnimLastIndex = 0;
	m_savedMouseAnimCurIndex = 0;

	return AUI_ERRCODE_OK;
}

AUI_ERRCODE aui_AmigaUI::DestroyNativeScreen(void)
{
	if (m_primary)
	{
		delete m_primary;
		m_primary   = NULL;
	}

	return AUI_ERRCODE_OK;
}

AUI_ERRCODE aui_AmigaUI::CreateNativeScreen( BOOL useExclusiveMode )
{
	AUI_ERRCODE errcode = aui_Amiga::InitCommon( useExclusiveMode );
	Assert( AUI_SUCCESS(errcode) );
	if ( !AUI_SUCCESS(errcode) ) return errcode;

	m_pixelFormat = aui_Surface::TransformBppToSurfacePixelFormat(m_bpp);

	if (m_bpp != 16)
	{
		c3errors_FatalDialog("aui_AmigaUI", "The Amiga screen is 16-bit only, %d bits were asked for.\n", m_bpp);
	}

	if (!amiga_video_open(m_width, m_height))
	{
		c3errors_FatalDialog("aui_AmigaUI",
			"Could not open a 16-bit RTG screen of %dx%d.\n"
			"Set ENV:CTP2_MODE to a CyberGraphX mode id (hex) to choose one.\n",
			m_width, m_height);
	}

	amiga_video_hide_pointer();

	m_primary = new aui_AmigaSurface(&errcode, m_width, m_height, m_bpp, TRUE);
	Assert( AUI_NEWOK(m_primary,errcode) );
	if ( !AUI_NEWOK(m_primary,errcode) ) return AUI_ERRCODE_MEMALLOCFAILED;

	if(!m_secondary)
	{
		m_secondary = new aui_AmigaSurface(&errcode, m_width, m_height, m_bpp, FALSE);
		Assert( AUI_NEWOK(m_secondary,errcode) );
		if ( !AUI_NEWOK(m_secondary,errcode) ) return AUI_ERRCODE_MEMALLOCFAILED;
	}

	m_pixelFormat = m_primary->PixelFormat();

	int mx = 0, my = 0;
	amiga_input_mouse(&mx, &my);
	m_cursorX = mx;
	m_cursorY = my;

	AddPresentAll();

	return AUI_ERRCODE_OK;
}

aui_AmigaUI::~aui_AmigaUI( void )
{
	delete [] m_composite;
	delete [] m_compositeMap;
	delete [] m_under;
	delete [] m_patchOld;
	delete [] m_patchNew;
	delete [] m_underNew;
	m_composite = NULL;
	delete [] m_cursorImage;
	m_cursorImage = NULL;

	amiga_video_close();
}

AUI_ERRCODE aui_AmigaUI::TearDownMouse(void)
{
	if (m_mouse) {
		m_mouse->GetAnimIndexes(&m_savedMouseAnimFirstIndex, &m_savedMouseAnimLastIndex);
		m_savedMouseAnimCurIndex = m_mouse->GetCurrentCursorIndex();
		m_savedMouseAnimDelay = (sint32)m_mouse->GetAnimDelay();

		m_mouse->End();

		delete m_mouse;
		m_mouse = NULL;
	}

	return AUI_ERRCODE_OK;
}

AUI_ERRCODE aui_AmigaUI::RestoreMouse(void)
{
	AUI_ERRCODE		auiErr;

	aui_AmigaMouse *mouse = new aui_AmigaMouse( &auiErr, "CivMouse", TRUE );
	Assert(mouse != NULL);
	if ( !mouse ) return AUI_ERRCODE_MEMALLOCFAILED;

	delete m_mouse;
	m_mouse = mouse;

	m_mouse->SetAnimIndexes(m_savedMouseAnimFirstIndex, m_savedMouseAnimLastIndex);
	m_mouse->SetCurrentCursor(m_savedMouseAnimCurIndex);
	m_mouse->SetAnimDelay((uint32)m_savedMouseAnimDelay);

	auiErr = m_mouse->Start();
	Assert(auiErr == AUI_ERRCODE_OK);

	return auiErr;
}

// No task switching on a fullscreen Amiga screen: nothing to give up.
AUI_ERRCODE aui_AmigaUI::AltTabOut( void )
{
	return AUI_ERRCODE_OK;
}

AUI_ERRCODE aui_AmigaUI::AltTabIn( void )
{
	AddPresentAll();
	return FlushDirtyList();
}

aui_MovieManager* aui_AmigaUI::CreateMovieManager( void )
{
	// No movies on the Amiga (no FFmpeg): the game skips them.
	return NULL;
}

AUI_ERRCODE aui_AmigaUI::ChangeSize(sint32 width, sint32 height)
{
	// The screen cannot change size while the game runs.
	Invalidate(NULL);
	return AUI_ERRCODE_OK;
}

void aui_AmigaUI::AddPresentRect(const RECT &rect)
{
	if (m_presentAll) return;

	RECT screen = { 0, 0, m_width, m_height };
	RECT clipped;
	if (!RectIntersect(clipped, rect, screen)) return;

	// Merge with a rectangle it touches on the same rows, which is what most
	// windows' dirty lists produce; otherwise keep it as its own.
	for (size_t i = 0; i < m_present.size(); )
	{
		RECT & r = m_present[i];
		if (clipped.left >= r.left && clipped.right <= r.right
		&&  clipped.top  >= r.top  && clipped.bottom <= r.bottom)
		{
			return;
		}
		// One it covers goes: its pixels would be compared twice.
		if (r.left >= clipped.left && r.right <= clipped.right
		&&  r.top  >= clipped.top  && r.bottom <= clipped.bottom)
		{
			r = m_present.back();
			m_present.pop_back();
			continue;
		}
		++i;
	}

	m_present.push_back(clipped);

	// Too many small pieces cost more in calls than fewer bigger ones: those
	// that start in the same band of 64 rows become one. (One rectangle
	// round all of them was often most of the screen, ~60 ms to compare on
	// a 68060, for pieces far apart.)
	if (m_present.size() > 64)
	{
		std::vector<RECT> bands;
		std::vector<sint32> bandOf(static_cast<size_t>(m_height / 64 + 1), -1);
		for (size_t i = 0; i < m_present.size(); ++i)
		{
			RECT const & r = m_present[i];
			sint32 &     at = bandOf[static_cast<size_t>(r.top / 64)];
			if (at < 0)
			{
				at = static_cast<sint32>(bands.size());
				bands.push_back(r);
				continue;
			}
			RECT & u = bands[static_cast<size_t>(at)];
			u.left   = std::min(u.left,   r.left);
			u.top    = std::min(u.top,    r.top);
			u.right  = std::max(u.right,  r.right);
			u.bottom = std::max(u.bottom, r.bottom);
		}
		m_present.swap(bands);
	}
}

void aui_AmigaUI::AddPresentAll(void)
{
	m_presentAll = true;
	m_present.clear();
}

void aui_AmigaUI::SetCursorImage(aui_Surface *surface, sint32 hotX, sint32 hotY, const void *owner)
{
	m_cursorOwner = owner;
	m_cursorHotX  = hotX;
	m_cursorHotY  = hotY;

	// Through Lock: a cursor read from a picture archive (RIM) is a plain
	// aui_Surface over the archive's data, whose Buffer() is set only while
	// it is locked.
	uint8 * pixels = NULL;
	if (surface && surface->BitsPerPixel() == 16
	&&  surface->Width() > 0 && surface->Height() > 0)
	{
		LPVOID locked = NULL;
		if (surface->Lock(NULL, &locked, 0) == AUI_ERRCODE_OK)
		{
			pixels = static_cast<uint8 *>(locked);
			if (!pixels) surface->Unlock(locked);
		}
	}

	if (!pixels)
	{
		m_cursorVisible = false;
		m_cursorWidth   = 0;
		m_cursorHeight  = 0;
		return;
	}

	sint32 const w     = surface->Width();
	sint32 const h     = surface->Height();
	sint32 const pitch = surface->Pitch();
	size_t const need  = static_cast<size_t>(w) * h;

	if (need > m_cursorImageSize)
	{
		delete [] m_cursorImage;
		m_cursorImage     = new uint16[need];
		m_cursorImageSize = need;
	}
	if (need > m_compositeSize)
	{
		delete [] m_composite;
		delete [] m_compositeMap;
		delete [] m_under;
		delete [] m_patchOld;
		delete [] m_patchNew;
		delete [] m_underNew;
		m_composite     = new uint16[need];
		m_compositeMap  = new uint16[need];
		m_under         = new uint16[need];
		m_patchOld      = new uint16[need];
		m_patchNew      = new uint16[need];
		m_underNew      = new uint16[need];
		m_compositeSize = need;
		m_underValid    = false;
	}
	m_cursorChanged = true;

	for (sint32 y = 0; y < h; ++y)
	{
		memcpy(m_cursorImage + y * w, pixels + y * pitch, static_cast<size_t>(w) * 2);
	}
	surface->Unlock(pixels);

	m_cursorWidth   = w;
	m_cursorHeight  = h;
	m_cursorKey     = static_cast<uint16>(surface->GetChromaKey());
	m_cursorVisible = true;

	// Drawn again at once, also where the old image was bigger.
	if (!RectEmpty(m_cursorShown)) AddPresentRect(m_cursorShown);
	RECT now;
	CursorRect(now);
	AddPresentRect(now);
}

void aui_AmigaUI::ClearCursorImage(const void *owner)
{
	if (owner != m_cursorOwner) return;

	m_cursorOwner   = NULL;
	m_cursorVisible = false;
	m_cursorWidth   = 0;
	m_cursorHeight  = 0;
}

void aui_AmigaUI::SetCursorPosition(sint32 x, sint32 y)
{
	m_cursorX = x;
	m_cursorY = y;
}

void aui_AmigaUI::CursorRect(RECT &rect) const
{
	rect.left   = m_cursorX - m_cursorHotX;
	rect.top    = m_cursorY - m_cursorHotY;
	rect.right  = rect.left + m_cursorWidth;
	rect.bottom = rect.top  + m_cursorHeight;
}

static void CursorLook(void);   // ENV:CTP2_PERF: the cursor's waits (below)
static uintptr_t s_lookSite;    // (below)

// tiledmap.cpp: the parts of the background window no other window covers.
extern void amiga_map_open_rects(std::vector<RECT> &out);

namespace
{
	// list without what o covers.
	void SubtractFrom(std::vector<RECT> &list, const RECT &o)
	{
		std::vector<RECT> out;
		for (size_t i = 0; i < list.size(); ++i)
		{
			RECT const r = list[i];
			RECT       c;
			if (!RectIntersect(c, r, o)) { out.push_back(r); continue; }
			if (r.top < c.top)       { RECT t = { r.left,  r.top,    r.right, c.top    }; out.push_back(t); }
			if (c.bottom < r.bottom) { RECT t = { r.left,  c.bottom, r.right, r.bottom }; out.push_back(t); }
			if (r.left < c.left)     { RECT t = { r.left,  c.top,    c.left,  c.bottom }; out.push_back(t); }
			if (c.right < r.right)   { RECT t = { c.right, c.top,    r.right, c.bottom }; out.push_back(t); }
		}
		list.swap(out);
	}
}

void aui_AmigaUI::SetDirectWindow(aui_Window *window)
{
	if (window == m_directWindow) return;

	m_directWindow = window;
	if (m_directOn)
	{
		// The frame holds nothing of the map where it was open.
		m_directOn = false;
		m_open.clear();
		Invalidate(NULL);
		AddPresentAll();
	}
}

void aui_AmigaUI::ComposeBegin(void)
{
	static int s_wanted = -1;
	if (s_wanted < 0)
	{
		char value[16];
		s_wanted = 1;
		if (amiga_env_value("CTP2_DIRECTMAP", value, sizeof(value))) s_wanted = atoi(value) != 0;
	}

	bool const on = s_wanted && m_directWindow && !m_directWindow->IsHidden()
	             && m_directWindow->TheSurface()
	             && m_directWindow->TheSurface()->IsThisA(aui_AmigaSurface::m_AmigaSurfaceClassId)
	             && amiga_video_has_shadow();

	m_open.clear();
	if (on) amiga_map_open_rects(m_open);

	if (m_directOn && !on && m_directWindow)
	{
		// The frame holds nothing of the map where it was open: all of the
		// window again, into the frame this time.
		m_directWindow->AddDirtyRect(0, 0, m_directWindow->Width(), m_directWindow->Height());
		AddPresentAll();
	}
	m_directOn = on;
}

bool aui_AmigaUI::IsDirectRect(aui_Window *window, const RECT &rect) const
{
	if (!m_directOn || window != m_directWindow) return false;

	RECT const screen = { 0, 0, m_width, m_height };
	RECT       r;
	if (!RectIntersect(r, rect, screen)) return true;   // nothing of it shows

	// The open parts do not overlap: they cover it when their shares of it
	// add up to all of it.
	sint32 area = 0;
	for (size_t i = 0; i < m_open.size(); ++i)
	{
		RECT c;
		if (RectIntersect(c, r, m_open[i])) area += (c.right - c.left) * (c.bottom - c.top);
	}
	return area == (r.right - r.left) * (r.bottom - r.top);
}

bool aui_AmigaUI::DirectSource(uint16 *&base, sint32 &pitch, RECT &rect) const
{
	if (!m_directOn || !m_directWindow || m_open.empty()) return false;

	aui_Surface * const surface = m_directWindow->TheSurface();
	if (!surface || !surface->IsThisA(aui_AmigaSurface::m_AmigaSurfaceClassId)) return false;

	uint8 * const pixels = static_cast<aui_AmigaSurface *>(surface)->Pixels();
	if (!pixels) return false;

	pitch = surface->Pitch();
	base  = reinterpret_cast<uint16 *>(pixels - m_directWindow->Y() * pitch) - m_directWindow->X();

	RECT const screen = { 0, 0, m_width, m_height };
	RECT const window =
	{
		m_directWindow->X(), m_directWindow->Y(),
		m_directWindow->X() + surface->Width(), m_directWindow->Y() + surface->Height()
	};
	return RectIntersect(rect, window, screen);
}

void aui_AmigaUI::CursorInto(uint16 *base, sint32 pitch, const RECT &rect, uint16 *saved) const
{
	sint32 const w   = rect.right - rect.left;
	sint32 const ox  = m_cursorX - m_cursorHotX;     // the image's corner on the screen
	sint32 const oy  = m_cursorY - m_cursorHotY;
	uint16 const key = m_cursorKey;

	for (sint32 y = rect.top; y < rect.bottom; ++y)
	{
		uint16 * dst = base + (y * pitch >> 1) + rect.left;
		memcpy(saved + (y - rect.top) * w, dst, static_cast<size_t>(w) * 2);

		sint32 const iy = y - oy;
		if (iy < 0 || iy >= m_cursorHeight) continue;
		uint16 const * img = m_cursorImage + iy * m_cursorWidth;
		for (sint32 x = rect.left; x < rect.right; ++x)
		{
			sint32 const ix = x - ox;
			if (ix >= 0 && ix < m_cursorWidth && img[ix] != key) dst[x - rect.left] = img[ix];
		}
	}
}

void aui_AmigaUI::CursorOutOf(uint16 *base, sint32 pitch, const RECT &rect, const uint16 *saved) const
{
	sint32 const w = rect.right - rect.left;
	for (sint32 y = rect.top; y < rect.bottom; ++y)
	{
		memcpy(base + (y * pitch >> 1) + rect.left, saved + (y - rect.top) * w, static_cast<size_t>(w) * 2);
	}
}

AUI_ERRCODE aui_AmigaUI::AmigaDrawScreen( void )
{
	// The secondary surface is what the UI composes, and what goes to the
	// card; the primary one only takes full-screen copies (splash screen),
	// which are copies of the secondary one. The open parts of the map come
	// from the map window's own surface (DirectSource).
	aui_Surface * const source = m_secondary ? m_secondary : m_primary;
	if (!source || !source->Buffer()) return AUI_ERRCODE_OK;

	RECT const screen = { 0, 0, m_width, m_height };

	uint16 * mapBase  = NULL;
	sint32   mapPitch = 0;
	RECT     mapRect  = { 0, 0, 0, 0 };
	bool const direct = !m_busyTick && DirectSource(mapBase, mapPitch, mapRect);

	RECT cursor = { 0, 0, 0, 0 };
	bool const showCursor = m_cursorVisible && m_cursorImage && m_mouse && !m_mouse->IsHidden();
	if (showCursor)
	{
		RECT wanted;
		CursorRect(wanted);
		RectIntersect(cursor, wanted, screen);
	}

	bool const cursorMoved =
		   m_cursorChanged
		|| cursor.left   != m_cursorShown.left
		|| cursor.top    != m_cursorShown.top
		|| cursor.right  != m_cursorShown.right
		|| cursor.bottom != m_cursorShown.bottom;

	// A few rows of the screen go out whole every 80 ms, round the screen
	// (~12 s), past the compare: a safety net for a card that does not show
	// what the copies believe (ENV:CTP2_SCRUB, rows per 80 ms; 0: none). Not
	// from the cursor-only copies (BusyTick without a shadow), which copy
	// from a frame that may be half drawn.
	static uint32 s_lastScrub = 0;
	static sint32 s_scrubRow  = 0;
	static sint32 s_scrubRows = -1;
	if (s_scrubRows < 0)
	{
		char value[16];
		s_scrubRows = 4;
		if (amiga_env_value("CTP2_SCRUB", value, sizeof(value)))
		{
			s_scrubRows = atoi(value);
			if (s_scrubRows < 0)  s_scrubRows = 0;
			if (s_scrubRows > 64) s_scrubRows = 64;
		}
	}
	if (!m_busyTick && s_scrubRows > 0)
	{
		uint32 const now = amiga_ticks_ms();
		if (now - s_lastScrub >= 80)
		{
			s_lastScrub = now;
			sint32 const rows = s_scrubRows;
			if (s_scrubRow >= m_height) s_scrubRow = 0;
			sint32 const bottom = (s_scrubRow + rows < m_height) ? s_scrubRow + rows : m_height;
			RECT const strip = { 0, s_scrubRow, m_width, bottom };
			AddPresentRect(strip);
			amiga_video_force_rows(s_scrubRow, bottom);
			s_scrubRow = bottom;
		}
	}

	// A shift waits for this copy (a scroll step): the card shows the cursor
	// where it was drawn, and the shift carries it along; both places are
	// compared again (and whatever else was touched meanwhile), and the
	// cursor goes out where it is.
	bool const shifting = !m_busyTick && amiga_video_shift_pending();
	if (shifting)
	{
		if (!RectEmpty(m_cursorShown))
		{
			amiga_video_touch(m_cursorShown.left, m_cursorShown.top, m_cursorShown.right, m_cursorShown.bottom);
			AddPresentRect(m_cursorShown);
		}
		if (!RectEmpty(cursor)) AddPresentRect(cursor);

		static AmigaRect touched[600];
		int const        n = amiga_video_take_touched(touched, 600);
		for (int i = 0; i < n; ++i)
		{
			RECT const r = { touched[i].left, touched[i].top, touched[i].right, touched[i].bottom };
			AddPresentRect(r);
		}
	}

	if (!m_presentAll && m_present.empty() && !cursorMoved)
	{
		return AUI_ERRCODE_OK;
	}

	// Where the cursor was, the picture shows again; where it is now, the
	// cursor goes out with the picture.
	if (cursorMoved)
	{
		if (!RectEmpty(m_cursorShown)) AddPresentRect(m_cursorShown);
		if (!RectEmpty(cursor))        AddPresentRect(cursor);
	}

	// The cursor goes into the pictures themselves for the copy, and the
	// pixels under it come back after: the card gets each pixel once, so it
	// never shows the picture without the cursor (with no double buffer on
	// the card, a copy followed by a cursor patch made the cursor flicker).
	uint16 *     frame = reinterpret_cast<uint16 *>(source->Buffer());
	sint32 const pitch = source->Pitch();
	sint32 const cw    = cursor.right - cursor.left;
	sint32 const ch    = cursor.bottom - cursor.top;
	bool const   drawn = showCursor && !RectEmpty(cursor) && m_composite;

	RECT cursorOnMap = { 0, 0, 0, 0 };
	bool drawnOnMap  = false;

	if (drawn)
	{
		CursorInto(frame, pitch, cursor, m_composite);
		drawnOnMap = direct && RectIntersect(cursorOnMap, cursor, mapRect);
		if (drawnOnMap) CursorInto(mapBase, mapPitch, cursorOnMap, m_compositeMap);

		// What the card shows there without the cursor (BusyCursor): the
		// frame's pixels, and the map's own where it is open.
		memcpy(m_under, m_composite, static_cast<size_t>(cw) * ch * 2);
		if (drawnOnMap)
		{
			sint32 const mw = cursorOnMap.right - cursorOnMap.left;
			for (size_t i = 0; i < m_open.size(); ++i)
			{
				RECT c;
				if (!RectIntersect(c, cursorOnMap, m_open[i])) continue;
				for (sint32 y = c.top; y < c.bottom; ++y)
				{
					memcpy(m_under + (y - cursor.top) * cw + (c.left - cursor.left),
					       m_compositeMap + (y - cursorOnMap.top) * mw + (c.left - cursorOnMap.left),
					       static_cast<size_t>(c.right - c.left) * 2);
				}
			}
		}
		m_underValid = true;
	}
	else
	{
		m_underValid = false;
	}

	// What goes out, and from which picture.
	std::vector<RECT> fromFrame;
	std::vector<AmigaRect> mapRects;
	if (m_presentAll) fromFrame.push_back(screen);
	else              fromFrame = m_present;

	uint32 pixels = 0;
	for (size_t i = 0; i < fromFrame.size(); ++i)
	{
		pixels += static_cast<uint32>((fromFrame[i].right - fromFrame[i].left) * (fromFrame[i].bottom - fromFrame[i].top));
	}

	// ENV:CTP2_PERF: who sends big updates (caller addresses relative to
	// amiga_log, for addr2line on ctp2.debug), at most twice a second.
	{
		static int    s_perf = -1;
		static uint32 s_lastBig = 0;
		if (s_perf < 0) s_perf = amiga_env_flag("CTP2_PERF") ? 1 : 0;
		if (s_perf && pixels >= 50000 && !m_presentAll)
		{
			uint32 const now = amiga_ticks_ms();
			if (now - s_lastBig >= 500)
			{
				s_lastBig = now;
				char   buf[256] = "";
				size_t len = 0;
				for (size_t i = 0; i < fromFrame.size() && i < 8 && len < sizeof(buf) - 32; ++i)
				{
					RECT const & r = fromFrame[i];
					len += sprintf(buf + len, " %ld,%ld-%ld,%ld", (long)r.left, (long)r.top, (long)r.right, (long)r.bottom);
				}
				amiga_log("Big present: %lu kpixels in %lu rects:%s; caller +%lx\n",
				          (unsigned long)(pixels / 1000), (unsigned long)fromFrame.size(), buf,
				          (unsigned long)((uintptr_t)__builtin_return_address(0) - (uintptr_t)&amiga_log));
			}
		}
	}

	if (direct)
	{
		for (size_t o = 0; o < m_open.size(); ++o)
		{
			for (size_t i = 0; i < fromFrame.size(); ++i)
			{
				RECT c;
				if (RectIntersect(c, fromFrame[i], m_open[o]))
				{
					AmigaRect a = { c.left, c.top, c.right, c.bottom };
					mapRects.push_back(a);
				}
			}
			SubtractFrom(fromFrame, m_open[o]);
		}
	}

	std::vector<AmigaRect> frameRects;
	frameRects.reserve(fromFrame.size());
	for (size_t i = 0; i < fromFrame.size(); ++i)
	{
		AmigaRect a = { fromFrame[i].left, fromFrame[i].top, fromFrame[i].right, fromFrame[i].bottom };
		frameRects.push_back(a);
	}

	// One copy, also for a big one (a scroll step): amiga_video compares
	// everything first and then shifts the card and writes the differences
	// back to back, so the step appears at once.
	{
		AmigaSource sources[2];
		int         n = 0;
		if (!mapRects.empty())
		{
			sources[n].pixels = mapBase;
			sources[n].pitch  = mapPitch;
			sources[n].rects  = &mapRects[0];
			sources[n].count  = static_cast<int>(mapRects.size());
			++n;
		}
		if (!frameRects.empty())
		{
			sources[n].pixels = frame;
			sources[n].pitch  = pitch;
			sources[n].rects  = &frameRects[0];
			sources[n].count  = static_cast<int>(frameRects.size());
			++n;
		}
		if (n) amiga_video_present_multi(sources, n);
	}

	m_present.clear();
	m_presentAll    = false;
	m_cursorChanged = false;

	if (amiga_snapshot_requested())
	{
		// The picture as it should be on the card, cursor included, as a
		// PPM: the frame, and the map's own surface where it is open.
		FILE * f = fopen("RAM:ctp2shot.ppm", "wb");
		if (f)
		{
			fprintf(f, "P6\n%ld %ld\n255\n", (long)m_width, (long)m_height);
			std::vector<uint8>  line(static_cast<size_t>(m_width) * 3);
			std::vector<uint16> row(static_cast<size_t>(m_width));
			for (sint32 y = 0; y < m_height; ++y)
			{
				memcpy(&row[0], frame + (y * pitch >> 1), static_cast<size_t>(m_width) * 2);
				if (direct)
				{
					for (size_t o = 0; o < m_open.size(); ++o)
					{
						RECT const & r = m_open[o];
						if (y < r.top || y >= r.bottom) continue;
						memcpy(&row[r.left], mapBase + (y * mapPitch >> 1) + r.left,
						       static_cast<size_t>(r.right - r.left) * 2);
					}
				}
				for (sint32 x = 0; x < m_width; ++x)
				{
					uint16 const p = row[x];
					line[x * 3 + 0] = static_cast<uint8>(((p >> 11) & 0x1F) << 3);
					line[x * 3 + 1] = static_cast<uint8>(((p >> 5) & 0x3F) << 2);
					line[x * 3 + 2] = static_cast<uint8>((p & 0x1F) << 3);
				}
				fwrite(&line[0], 1, line.size(), f);
			}
			fclose(f);
			amiga_log("Snapshot: RAM:ctp2shot.ppm\n");

			// With it, how the screen is put together: the windows, and
			// which parts of the map come from its own surface.
			ListPos position = m_childList->GetHeadPosition();
			for (size_t n = m_childList->L(); n; --n)
			{
				aui_Region * window = m_childList->GetNext(position);
				if (!window) continue;
				amiga_log("Snapshot:   window %ld at %ld,%ld %ldx%ld%s%s\n", (long)window->Id(),
				          (long)window->X(), (long)window->Y(), (long)window->Width(), (long)window->Height(),
				          window->IsHidden() ? " hidden" : "",
				          window == static_cast<aui_Region *>(m_directWindow) ? " (the map)" : "");
			}
			for (size_t o = 0; direct && o < m_open.size(); ++o)
			{
				amiga_log("Snapshot:   map open %ld,%ld-%ld,%ld\n", (long)m_open[o].left, (long)m_open[o].top,
				          (long)m_open[o].right, (long)m_open[o].bottom);
			}
			if (amiga_video_dump_card("RAM:ctp2card.ppm"))
				amiga_log("Snapshot: the card in RAM:ctp2card.ppm\n");
		}
	}

	if (drawn)
	{
		if (drawnOnMap) CursorOutOf(mapBase, mapPitch, cursorOnMap, m_compositeMap);
		CursorOutOf(frame, pitch, cursor, m_composite);
	}

	m_cursorShown = cursor; // empty when no cursor is shown

	return AUI_ERRCODE_OK;
}

// The cursor moved (or changed) while the game is busy: what the card shows
// without the cursor comes from the shadow of the card and from m_under
// (where the cursor is), not from the frame or the map's surface, which may
// be half drawn now, or shifted already where the card is not yet. Two small
// patches go out: where the cursor was, and where it is.
void aui_AmigaUI::BusyCursor(void)
{
	RECT const screen = { 0, 0, m_width, m_height };
	RECT       next   = { 0, 0, 0, 0 };
	bool const show   = m_cursorVisible && m_cursorImage && m_mouse && !m_mouse->IsHidden();
	if (show)
	{
		RECT wanted;
		CursorRect(wanted);
		RectIntersect(next, wanted, screen);
	}

	RECT const old = m_cursorShown;
	if (!m_cursorChanged
	 && next.left == old.left && next.top == old.top && next.right == old.right && next.bottom == old.bottom)
	{
		return;
	}
	if (!m_under || (!RectEmpty(old) && !m_underValid)) return;   // the next frame's copy does it

	sint32 const ow = old.right - old.left,   oh = old.bottom - old.top;
	sint32 const nw = next.right - next.left, nh = next.bottom - next.top;
	if (static_cast<size_t>(ow) * oh > m_compositeSize || static_cast<size_t>(nw) * nh > m_compositeSize) return;

	// What the card shows where the cursor goes: the shadow, and under the
	// cursor as it stands (which the shadow shows too) what it covers.
	if (!RectEmpty(next))
	{
		if (!amiga_video_read_shadow(next.left, next.top, nw, nh, m_underNew)) return;

		RECT c;
		if (!RectEmpty(old) && RectIntersect(c, old, next))
		{
			for (sint32 y = c.top; y < c.bottom; ++y)
			{
				memcpy(m_underNew + (y - next.top) * nw + (c.left - next.left),
				       m_under + (y - old.top) * ow + (c.left - old.left),
				       static_cast<size_t>(c.right - c.left) * 2);
			}
		}
		memcpy(m_patchNew, m_underNew, static_cast<size_t>(nw) * nh * 2);
	}

	amiga_video_cursor_only(1);

	// Where it was: the picture again, with the cursor where it still lies.
	// (The patches are pictures whose screen pixel (0, 0) lies before them.)
	if (!RectEmpty(old))
	{
		memcpy(m_patchOld, m_under, static_cast<size_t>(ow) * oh * 2);
		uint16 * const base = m_patchOld - (old.top * ow + old.left);
		if (!RectEmpty(next))
		{
			RECT c;
			if (RectIntersect(c, old, next))
			{
				// Only the cursor's pixels: m_composite takes what CursorInto saves.
				CursorInto(base, ow * 2, c, m_composite);
			}
		}
		AmigaRect const a = { old.left, old.top, old.right, old.bottom };
		amiga_video_present(base, ow * 2, &a, 1);
	}

	if (!RectEmpty(next))
	{
		uint16 * const base = m_patchNew - (next.top * nw + next.left);
		CursorInto(base, nw * 2, next, m_composite);
		AmigaRect const a = { next.left, next.top, next.right, next.bottom };
		amiga_video_present(base, nw * 2, &a, 1);
	}

	amiga_video_cursor_only(0);

	std::swap(m_under, m_underNew);
	m_underValid    = !RectEmpty(next);
	m_cursorShown   = next;
	m_cursorChanged = false;
}

//----------------------------------------------------------------------------
//
// Events
//
// The same translation as the SDL build's SDLMessageHandler (civ3_main.cpp):
// special keys become the codes ui_HandleKeypress and aui_textfield expect,
// typed characters go in as they are, and the arrows, shifts and controls also
// go to aui_Keyboard (map scrolling, shift-clicks).
//
//----------------------------------------------------------------------------

namespace
{
	// Amiga raw key codes.
	enum
	{
		RAW_SPACE = 0x40, RAW_BACKSPACE = 0x41, RAW_TAB = 0x42,
		RAW_KP_ENTER = 0x43, RAW_RETURN = 0x44, RAW_ESCAPE = 0x45,
		RAW_DELETE = 0x46, RAW_KP_PERIOD = 0x3C,
		RAW_UP = 0x4C, RAW_DOWN = 0x4D, RAW_RIGHT = 0x4E, RAW_LEFT = 0x4F,
		RAW_F1 = 0x50, RAW_F10 = 0x59,
		RAW_LSHIFT = 0x60, RAW_RSHIFT = 0x61, RAW_CTRL = 0x63,
		RAW_WHEEL_UP = 0x7A, RAW_WHEEL_DOWN = 0x7B
	};

	WPARAM ArrowCode(sint32 sdlKey, sint32 qualifier)
	{
		bool const shift = (qualifier & (AMIGA_QUAL_LSHIFT | AMIGA_QUAL_RSHIFT)) != 0;
		bool const ctrl  = (qualifier & AMIGA_QUAL_CTRL) != 0;
		if (shift) return ctrl ? sdlKey + 1024 : sdlKey + 512;
		return ctrl ? sdlKey + 768 : sdlKey + 256;
	}

	// The key code ui_HandleKeypress gets for a raw key, or 0 when the key is
	// a character (sent as typed) or none of the game's keys.
	WPARAM SpecialKey(sint32 code, sint32 qualifier, char ch)
	{
		bool const shift = (qualifier & (AMIGA_QUAL_LSHIFT | AMIGA_QUAL_RSHIFT)) != 0;
		bool const ctrl  = (qualifier & AMIGA_QUAL_CTRL) != 0;

		switch (code)
		{
		case RAW_BACKSPACE: return VK_BACK;
		case RAW_TAB:       return '\t' + 128;
		case RAW_RETURN:
		case RAW_KP_ENTER:  return VK_RETURN + 128;
		case RAW_ESCAPE:    return VK_ESCAPE;
		case RAW_DELETE:
		case RAW_KP_PERIOD: return (code == RAW_KP_PERIOD && ch) ? 0 : SDLK_DELETE;
		case RAW_UP:        return ArrowCode(SDLK_UP,    qualifier);
		case RAW_DOWN:      return ArrowCode(SDLK_DOWN,  qualifier);
		case RAW_LEFT:      return ArrowCode(SDLK_LEFT,  qualifier);
		case RAW_RIGHT:     return ArrowCode(SDLK_RIGHT, qualifier);
		default:
			break;
		}

		if (code >= RAW_F1 && code <= RAW_F10)
		{
			if (shift) return 0;
			return (code == RAW_F10) ? '0' + 128 : '1' + (code - RAW_F1) + 128;
		}

		// Control + a letter: the control character, as SDLKCONVSHIFTCTRL.
		if (ctrl && ch)
		{
			char const lower = (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
			if (lower >= 'a' && lower <= 'z') return lower - 'a' + 1;
		}

		return 0;
	}
}

// ENV:CTP2_PERF: how long the cursor waits between two looks at the mouse
// (the cursor's lag), reported with the main loop's Perf line.
uint32 g_amigaCursorGapMax  = 0;
uint32 g_amigaCursorGapOver = 0;     // gaps of 50 ms and more

// Where the looks come from (return addresses, relative to amiga_log in the
// log), so that the longest wait can be found in the code (s_lookSite,
// declared above AmigaDrawScreen).
uintptr_t g_amigaCursorGapFrom = 0;
uintptr_t g_amigaCursorGapTo   = 0;

static void CursorLook(void)
{
	static uint32    s_last = 0;
	static uintptr_t s_lastSite = 0;
	if (!amiga_perf) return;
	uint32 const now = amiga_ticks_ms();
	if (s_last)
	{
		uint32 const gap = now - s_last;
		if (gap > g_amigaCursorGapMax)
		{
			g_amigaCursorGapMax  = gap;
			g_amigaCursorGapFrom = s_lastSite;
			g_amigaCursorGapTo   = s_lookSite;
		}
		if (gap >= 50) ++g_amigaCursorGapOver;
	}
	s_last     = now;
	s_lastSite = s_lookSite;
}

void aui_AmigaUI::BusyTick(void)
{
	static uint32 s_last = 0;
	uint32 const  now = amiga_ticks_ms();
	if (now - s_last < 20) return;
	s_last = now;
	AMIGA_PERF_BEGIN(perfBusy);

	CursorLook();

	int x = 0, y = 0;
	if (amiga_input_peek_mouse(&x, &y))
	{
		m_cursorX = x;
		m_cursorY = y;
	}

	aui_AmigaMouse * mouse = static_cast<aui_AmigaMouse *>(m_mouse);
	if (mouse) mouse->Tick(now);

	if (amiga_video_has_shadow())
	{
		BusyCursor();
	}
	else
	{
		// Without the shadow (the lock was refused): the cursor over the
		// frame as it is. Only the cursor goes to the card: what the UI has
		// drawn so far stays for the next frame, which may find it half
		// done now.
		std::vector<RECT> pending;
		pending.swap(m_present);
		bool const all = m_presentAll;
		m_presentAll = false;

		amiga_video_cursor_only(1);
		m_busyTick = true;
		AmigaDrawScreen();
		m_busyTick = false;
		amiga_video_cursor_only(0);

		m_present.swap(pending);
		m_presentAll = all;
	}
	AMIGA_PERF_END(perfBusy, AMIGA_PERF_BUSY);
}

// aui_UI::Draw, before the windows' changes go to the frame.
void amiga_ui_compose_begin(void)
{
	if (g_ui) static_cast<aui_AmigaUI *>(g_ui)->ComposeBegin();
}

// The map's window (backgroundwin.cpp), NULL when it goes.
void amiga_ui_direct_window(aui_Window *window)
{
	if (g_ui) static_cast<aui_AmigaUI *>(g_ui)->SetDirectWindow(window);
}

// TiledMap::ScrollPixels shifted the map surface by dx, dy (its content
// moved left/up for positive values): the same rectangle of the composed
// frame, a band no other window covers, is shifted in memory too, so that the
// map's step needs only the new strip composed and compared
// (TiledMap::ScrollMap marks only that), not the whole view.
void amiga_ui_scroll_frame(sint32 x, sint32 y, sint32 w, sint32 h, sint32 dx, sint32 dy)
{
	if (g_ui) static_cast<aui_AmigaUI *>(g_ui)->ScrollFrame(x, y, w, h, dx, dy);
}

void aui_AmigaUI::ScrollFrame(sint32 x, sint32 y, sint32 w, sint32 h, sint32 dx, sint32 dy)
{
	// Where the map goes to the card from its own surface, the frame holds
	// nothing of it.
	if (m_directOn) return;

	aui_Surface * const source = m_secondary ? m_secondary : m_primary;
	if (!source || !source->Buffer()) return;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > m_width)  w = m_width  - x;
	if (y + h > m_height) h = m_height - y;
	sint32 const adx = dx < 0 ? -dx : dx;
	sint32 const ady = dy < 0 ? -dy : dy;
	if (w <= adx || h <= ady) return;

	uint8 *      frame = reinterpret_cast<uint8 *>(source->Buffer());
	sint32 const pitch = source->Pitch();
	sint32 const cw = w - adx, ch = h - ady;
	sint32 const sx = x + (dx > 0 ? dx : 0), sy = y + (dy > 0 ? dy : 0);
	sint32 const tx = x + (dx < 0 ? adx : 0), ty = y + (dy < 0 ? ady : 0);

	if (ty <= sy)
	{
		for (sint32 row = 0; row < ch; ++row)
			amiga_shift_row(reinterpret_cast<uint16_t *>(frame + (ty + row) * pitch + tx * 2),
			                reinterpret_cast<uint16_t *>(frame + (sy + row) * pitch + sx * 2), cw, adx);
	}
	else
	{
		for (sint32 row = ch - 1; row >= 0; --row)
			amiga_shift_row(reinterpret_cast<uint16_t *>(frame + (ty + row) * pitch + tx * 2),
			                reinterpret_cast<uint16_t *>(frame + (sy + row) * pitch + sx * 2), cw, adx);
	}
}

// Called by the game event manager between events (GameEventManager.cpp).
void amiga_ui_busy_tick(void)
{
	s_lookSite = (uintptr_t)__builtin_return_address(0);
	if (g_ui) static_cast<aui_AmigaUI *>(g_ui)->BusyTick();
}

BOOL aui_AmigaUI::PumpEvents(bool *pressed)
{
	s_lookSite = (uintptr_t)__builtin_return_address(0);
	CursorLook();

	if (pressed) *pressed = false;

	aui_AmigaMouse * mouse = static_cast<aui_AmigaMouse *>(m_mouse);

	AmigaEvent ev;
	while (amiga_input_poll(&ev))
	{
		switch (ev.type)
		{
		case AMIGA_EV_QUIT:
			return FALSE;

		case AMIGA_EV_MOUSEMOVE:
			m_cursorX = ev.x;
			m_cursorY = ev.y;
			if (mouse) mouse->Moved(ev.x, ev.y, ev.time_ms);
			break;

		case AMIGA_EV_MOUSEBUTTON:
			if (pressed) *pressed = true;
			if (mouse) mouse->Button(ev.code, ev.down, ev.time_ms);
			break;

		case AMIGA_EV_WHEEL:
			if (pressed) *pressed = true;
			ui_HandleMouseWheel(static_cast<sint16>(ev.code));
			break;

		case AMIGA_EV_KEY:
		{
			if (pressed) *pressed = true;
			if (ev.code == RAW_WHEEL_UP || ev.code == RAW_WHEEL_DOWN)
			{
				if (ev.down) ui_HandleMouseWheel(ev.code == RAW_WHEEL_UP ? 1 : -1);
				break;
			}

			// What aui_Keyboard polls: arrows, shifts, control.
			switch (ev.code)
			{
			case RAW_UP: case RAW_DOWN: case RAW_LEFT: case RAW_RIGHT:
			case RAW_LSHIFT: case RAW_RSHIFT: case RAW_CTRL:
			case RAW_RETURN:
				aui_AmigaKeyboard::QueueRawKey(ev.code, ev.down, ev.time_ms);
				break;
			default:
				break;
			}

			if (!ev.down) break;

			// ENV:CTP2_TEST: F9 explores the whole map (display checks).
			if (ev.code == RAW_F1 + 8 && amiga_env_flag("CTP2_TEST"))
			{
				extern void amiga_test_reveal(void);
				amiga_test_reveal();
				break;
			}

			// F4 on the main menu starts a game with the last settings.
			if (ev.code == RAW_F1 + 3 && initialplayscreen_QuickStart()) break;

			WPARAM const wp = SpecialKey(ev.code, ev.qualifier, ev.ch);
			if (wp)
			{
				ui_HandleKeypress(wp, 0);
			}
			else if (ev.ch && static_cast<unsigned char>(ev.ch) >= ' '
			     && !(ev.qualifier & AMIGA_QUAL_CTRL))
			{
				// A typed character, as SDL_TEXTINPUT.
				ui_HandleKeypress(static_cast<unsigned char>(ev.ch), 0);
			}
			break;
		}

		default:
			break;
		}
	}

	if (mouse)
	{
		mouse->Tick(amiga_ticks_ms());
	}

	return !amiga_break_requested();
}

#endif  // __AUI_USE_AMIGA__

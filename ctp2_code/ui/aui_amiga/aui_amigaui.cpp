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
	m_composite         (NULL),
	m_compositeSize     (0)
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
	for (size_t i = 0; i < m_present.size(); ++i)
	{
		RECT & r = m_present[i];
		if (clipped.left >= r.left && clipped.right <= r.right
		&&  clipped.top  >= r.top  && clipped.bottom <= r.bottom)
		{
			return;
		}
	}

	m_present.push_back(clipped);

	// Too many small pieces cost more in calls than one copy of the union.
	if (m_present.size() > 64)
	{
		RECT u = m_present[0];
		for (size_t i = 1; i < m_present.size(); ++i)
		{
			u.left   = std::min(u.left,   m_present[i].left);
			u.top    = std::min(u.top,    m_present[i].top);
			u.right  = std::max(u.right,  m_present[i].right);
			u.bottom = std::max(u.bottom, m_present[i].bottom);
		}
		m_present.clear();
		m_present.push_back(u);
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
		m_composite     = new uint16[need];
		m_compositeSize = need;
	}

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

AUI_ERRCODE aui_AmigaUI::AmigaDrawScreen( void )
{
	// The secondary surface is what the UI composes, and what goes to the
	// card; the primary one only takes full-screen copies (splash screen),
	// which are copies of the secondary one.
	aui_Surface * const source = m_secondary ? m_secondary : m_primary;
	if (!source || !source->Buffer()) return AUI_ERRCODE_OK;

	RECT const screen = { 0, 0, m_width, m_height };

	RECT cursor = { 0, 0, 0, 0 };
	bool const showCursor = m_cursorVisible && m_cursorImage && m_mouse && !m_mouse->IsHidden();
	if (showCursor)
	{
		RECT wanted;
		CursorRect(wanted);
		RectIntersect(cursor, wanted, screen);
	}

	bool const cursorMoved =
		   cursor.left   != m_cursorShown.left
		|| cursor.top    != m_cursorShown.top
		|| cursor.right  != m_cursorShown.right
		|| cursor.bottom != m_cursorShown.bottom;

	if (!m_presentAll && m_present.empty() && !cursorMoved)
	{
		return AUI_ERRCODE_OK;
	}

	// Where the cursor was, the frame shows again; where it is now, the
	// cursor goes out with the frame.
	if (cursorMoved)
	{
		if (!RectEmpty(m_cursorShown)) AddPresentRect(m_cursorShown);
		if (!RectEmpty(cursor))        AddPresentRect(cursor);
	}

	// The cursor goes into the frame itself for the copy, and the pixels
	// under it come back after: the card gets each pixel once, so it never
	// shows the frame without the cursor (with no double buffer on the card,
	// a frame copy followed by a cursor patch made the cursor flicker).
	uint16 *     frame = reinterpret_cast<uint16 *>(source->Buffer());
	sint32 const pitch = source->Pitch();
	sint32 const cw    = cursor.right - cursor.left;
	sint32 const ch    = cursor.bottom - cursor.top;
	bool const   drawn = showCursor && !RectEmpty(cursor) && m_composite;

	if (drawn)
	{
		sint32 const cx  = cursor.left - (m_cursorX - m_cursorHotX); // offset into the image
		sint32 const cy  = cursor.top  - (m_cursorY - m_cursorHotY);
		uint16 const key = m_cursorKey;

		for (sint32 y = 0; y < ch; ++y)
		{
			uint16 *       dst   = frame + ((cursor.top + y) * pitch >> 1) + cursor.left;
			uint16 *       saved = m_composite + y * cw;
			uint16 const * img   = m_cursorImage + (cy + y) * m_cursorWidth + cx;

			memcpy(saved, dst, static_cast<size_t>(cw) * 2);
			for (sint32 x = 0; x < cw; ++x)
			{
				if (img[x] != key) dst[x] = img[x];
			}
		}
	}

	if (m_presentAll)
	{
		AmigaRect all = { 0, 0, m_width, m_height };
		amiga_video_present(frame, pitch, &all, 1);
	}
	else if (!m_present.empty())
	{
		std::vector<AmigaRect> rects;
		rects.reserve(m_present.size());
		for (size_t i = 0; i < m_present.size(); ++i)
		{
			RECT const & r = m_present[i];
			AmigaRect a = { r.left, r.top, r.right, r.bottom };
			rects.push_back(a);
		}
		amiga_video_present(frame, pitch, &rects[0], static_cast<int>(rects.size()));
	}

	m_present.clear();
	m_presentAll = false;

	if (drawn)
	{
		for (sint32 y = 0; y < ch; ++y)
		{
			memcpy(frame + ((cursor.top + y) * pitch >> 1) + cursor.left,
			       m_composite + y * cw, static_cast<size_t>(cw) * 2);
		}
	}

	m_cursorShown = cursor; // empty when no cursor is shown

	return AUI_ERRCODE_OK;
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

void aui_AmigaUI::BusyTick(void)
{
	static uint32 s_last = 0;
	uint32 const  now = amiga_ticks_ms();
	if (now - s_last < 20) return;
	s_last = now;

	int x = 0, y = 0;
	if (amiga_input_peek_mouse(&x, &y))
	{
		m_cursorX = x;
		m_cursorY = y;
	}

	aui_AmigaMouse * mouse = static_cast<aui_AmigaMouse *>(m_mouse);
	if (mouse) mouse->Tick(now);

	// Only the cursor goes to the card: what the UI has drawn so far stays
	// for the next frame, which may find it half done now.
	std::vector<RECT> pending;
	pending.swap(m_present);
	bool const all = m_presentAll;
	m_presentAll = false;

	AmigaDrawScreen();

	m_present.swap(pending);
	m_presentAll = all;
}

// Called by the game event manager between events (GameEventManager.cpp).
void amiga_ui_busy_tick(void)
{
	if (g_ui) static_cast<aui_AmigaUI *>(g_ui)->BusyTick();
}

BOOL aui_AmigaUI::PumpEvents(bool *pressed)
{
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

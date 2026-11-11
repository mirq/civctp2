//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Amiga backend: the keyboard
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
// Follows aui_SDLKeyboard::GetInput (ui/aui_sdl/aui_sdlkeyboard.cpp): shifts
// and controls set the mouse's modifier flags, the arrows start and stop map
// scrolling, and the keys aui_Keyboard knows are passed on as its key codes.
//
//----------------------------------------------------------------------------

#include "ctp2_config.h"
#include "c3.h"

#if defined(__AUI_USE_AMIGA__)

#include <deque>

#include "aui_amigakeyboard.h"
#include "aui_mouse.h"
#include "c3ui.h"
#include "civapp.h"

extern C3UI *   g_c3ui;
extern CivApp * g_civApp;

namespace
{
	struct RawKey
	{
		sint32  code;
		BOOL    down;
		uint32  time;
	};

	std::deque<RawKey> s_keys;

	// Amiga raw key codes, as in aui_amigaui.cpp.
	enum
	{
		RAW_RETURN = 0x44, RAW_UP = 0x4C, RAW_DOWN = 0x4D, RAW_RIGHT = 0x4E,
		RAW_LEFT = 0x4F, RAW_LSHIFT = 0x60, RAW_RSHIFT = 0x61, RAW_CTRL = 0x63
	};

	uint32 AuiKey(sint32 code)
	{
		switch (code)
		{
		case RAW_RETURN: return AUI_KEYBOARD_KEY_RETURN;
		case RAW_UP:     return AUI_KEYBOARD_KEY_UPARROW;
		case RAW_DOWN:   return AUI_KEYBOARD_KEY_DOWNARROW;
		case RAW_LEFT:   return AUI_KEYBOARD_KEY_LEFTARROW;
		case RAW_RIGHT:  return AUI_KEYBOARD_KEY_RIGHTARROW;
		default:         return AUI_KEYBOARD_KEY_INVALID;
		}
	}
}

aui_AmigaKeyboard::aui_AmigaKeyboard(AUI_ERRCODE *retval)
:
	aui_Input       (),
	aui_Keyboard    (),
	aui_AmigaInput  (retval, FALSE)
{
}

void aui_AmigaKeyboard::QueueRawKey(sint32 code, BOOL down, uint32 time)
{
	// The game reads one key per frame; a few frames of keys are plenty.
	if (s_keys.size() >= 64) s_keys.pop_front();

	RawKey key = { code, down, time };
	s_keys.push_back(key);
}

AUI_ERRCODE aui_AmigaKeyboard::GetInput(void)
{
	if (s_keys.empty())
	{
		return AUI_ERRCODE_NOINPUT;
	}

	RawKey const key = s_keys.front();
	s_keys.pop_front();

	m_data.time = key.time;

	aui_Mouse * mouse = g_c3ui ? g_c3ui->TheMouse() : NULL;

	switch (key.code)
	{
	case RAW_LSHIFT:
		if (mouse) { if (key.down) mouse->SetLeftShift(); else mouse->UnsetLeftShift(); }
		return AUI_ERRCODE_OK;
	case RAW_RSHIFT:
		if (mouse) { if (key.down) mouse->SetRightShift(); else mouse->UnsetRightShift(); }
		return AUI_ERRCODE_OK;
	case RAW_CTRL:
		// One control key on the Amiga keyboard: it counts as the left one.
		if (mouse) { if (key.down) mouse->SetLeftControl(); else mouse->UnsetLeftControl(); }
		return AUI_ERRCODE_OK;
	case RAW_UP:
	case RAW_DOWN:
	case RAW_LEFT:
	case RAW_RIGHT:
		if (g_civApp)
		{
			if (key.down) g_civApp->BeginKeyboardScrolling(AuiKey(key.code));
			else          g_civApp->StopKeyboardScrolling(AuiKey(key.code));
		}
		break;
	default:
		break;
	}

	m_data.down = key.down;
	m_data.key  = AuiKey(key.code);

	return AUI_ERRCODE_OK;
}

#endif // defined(__AUI_USE_AMIGA__)

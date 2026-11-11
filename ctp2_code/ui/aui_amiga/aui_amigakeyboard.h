//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
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
// aui_Keyboard sees the keys the game polls (arrows, shift, control) through
// a queue filled by aui_AmigaUI::PumpEvents; typed characters go to
// ui_HandleKeypress there, as the SDL build's SDLMessageHandler does.
//
//----------------------------------------------------------------------------

#ifndef __aui_amiga__aui_amigakeyboard_h__
#define __aui_amiga__aui_amigakeyboard_h__ 1

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#include "ctp2_config.h"

#if defined(__AUI_USE_AMIGA__)

#include "aui_keyboard.h"
#include "aui_amigainput.h"

class aui_AmigaKeyboard : public aui_Keyboard, public aui_AmigaInput
{
public:
	aui_AmigaKeyboard(AUI_ERRCODE *retval);
	virtual ~aui_AmigaKeyboard() {}

	// A raw key from the window: code 0..127, down or up.
	static void QueueRawKey(sint32 code, BOOL down, uint32 time);

protected:
	aui_AmigaKeyboard() {}

public:
	virtual AUI_ERRCODE Acquire() { return AUI_ERRCODE_OK; }
	virtual AUI_ERRCODE Unacquire() { return AUI_ERRCODE_OK; }
	virtual AUI_ERRCODE GetInput();
};

typedef aui_AmigaKeyboard aui_NativeKeyboard;

#endif // defined(__AUI_USE_AMIGA__)

#endif

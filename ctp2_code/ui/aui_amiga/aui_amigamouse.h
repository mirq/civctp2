//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga backend: the mouse
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
// Mouse events come from the screen's backdrop window (IDCMP), queued by
// aui_AmigaUI::PumpEvents through ManipulateInputs. The cursor is drawn by
// aui_AmigaUI in software; its animation is stepped from the main loop.
//
//----------------------------------------------------------------------------

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#ifndef __aui_amiga__aui_amigamouse_h__
#define __aui_amiga__aui_amigamouse_h__ 1

#include "ctp2_config.h"

#if defined(__AUI_USE_AMIGA__)

#include "aui_mouse.h"
#include "aui_amigainput.h"

class aui_AmigaMouse : public aui_Mouse, public aui_AmigaInput
{
public:
	aui_AmigaMouse(AUI_ERRCODE *retval, const MBCHAR *ldlBlock, BOOL useExclusiveMode = FALSE);
	virtual ~aui_AmigaMouse();

	virtual AUI_ERRCODE HandleAnim();
	virtual void SetAnimIndexes(sint32 firstIndex, sint32 lastIndex);

	virtual AUI_ERRCODE BltWindowToPrimary(aui_Window *window);
	virtual AUI_ERRCODE BltDirtyRectInfoToPrimary();
	virtual AUI_ERRCODE BltBackgroundColorToPrimary(COLORREF color, aui_DirtyList *colorAreas);
	virtual AUI_ERRCODE BltBackgroundImageToPrimary(aui_Image *image, RECT *imageRect, aui_DirtyList *imageAreas);

	virtual sint32 ManipulateInputs(aui_MouseEvent *data, BOOL add);

	// From aui_AmigaUI::PumpEvents.
	void Moved(sint32 x, sint32 y, uint32 time);
	void Button(sint32 button, BOOL down, uint32 time);
	// Steps the cursor animation when its time has come (main loop).
	void Tick(uint32 now);

protected:
	aui_AmigaMouse() {}

	virtual void ActivateCursor(aui_Cursor *cursor);

	virtual AUI_ERRCODE Acquire(void) { return AUI_ERRCODE_OK; }
	virtual AUI_ERRCODE Unacquire(void) { return AUI_ERRCODE_OK; }
	virtual AUI_ERRCODE GetInput() { return AUI_ERRCODE_OK; };
	virtual AUI_ERRCODE ReactToInput() { return AUI_ERRCODE_OK; }

private:
	void Queue(uint32 time);
	// Copies the rectangles of the secondary surface just drawn to the
	// primary one and records them for the frame copy.
	void Show(const RECT &screenRect);

	aui_Cursor *m_currentCursor;
	uint32      m_lastFrameTick;
	uint32      m_nextAnimTick;
	sint32      m_queued;
};

typedef aui_AmigaMouse aui_NativeMouse;

#endif // defined(__AUI_USE_AMIGA__)

#endif

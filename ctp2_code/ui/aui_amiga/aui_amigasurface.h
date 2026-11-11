//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga backend: surfaces in fast RAM
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
// Every surface, the primary one included, is a 16-bit R5G6B5 buffer in main
// memory, as with SDL: Buffer() is always valid, so the generic blitters of
// aui_Blitter/C3Blitter draw everything. Only aui_AmigaUI::AmigaDrawScreen
// takes pixels to the card.
//
//----------------------------------------------------------------------------

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#ifndef __aui_amiga__aui_amigasurface_h__
#define __aui_amiga__aui_amigasurface_h__ 1

#include "ctp2_config.h"

#if defined(__AUI_USE_AMIGA__)

#include "aui_surface.h"
#include "aui_amiga.h"

class aui_AmigaSurface : public aui_Surface, public aui_Amiga
{
public:
	aui_AmigaSurface(
		AUI_ERRCODE *retval,
		sint32 width,
		sint32 height,
		sint32 bpp,
		BOOL isPrimary = FALSE);
	virtual ~aui_AmigaSurface();

protected:
	aui_AmigaSurface() : aui_Surface(), m_pixels(NULL) {}

public:
	virtual BOOL IsThisA(uint32 classId)
	{
		return (classId == m_AmigaSurfaceClassId)
		    || aui_Surface::IsThisA(classId)
		    || aui_Amiga::IsThisA(classId);
	}

	virtual AUI_ERRCODE Lock(RECT *rect, LPVOID *buffer, DWORD flags);
	virtual AUI_ERRCODE Unlock(LPVOID buffer);

	virtual BOOL IsOK(void) const { return m_pixels != NULL; }

	virtual AUI_ERRCODE Blank(const uint32 &color);

	// The primary surface shows itself whole (splash screens, the movie
	// player); the game loop shows only what changed (aui_AmigaMouse).
	virtual void Flip();

	static uint32 m_AmigaSurfaceClassId;

protected:
	uint8 * m_pixels;
};

typedef aui_AmigaSurface aui_NativeSurface;

#endif // defined(__AUI_USE_AMIGA__)

#endif

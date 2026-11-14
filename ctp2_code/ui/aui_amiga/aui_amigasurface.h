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
	aui_AmigaSurface() : aui_Surface(), m_pixels(NULL), m_block(NULL),
	                     m_slackX(0), m_slackY(0), m_originX(0), m_originY(0) {}

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

	// Spare room round the picture (the map's background window): the rows
	// get slackX pixels more on both sides and slackY rows above and below,
	// so that a scroll moves where the picture starts, not its pixels
	// (ScrollOrigin). The picture and Width()/Height() stay; Pitch() grows.
	// FALSE when locked or out of memory (nothing changes then).
	BOOL EnsureSlack(sint32 slackX, sint32 slackY);
	BOOL HasSlack(void) const { return m_slackX > 0 || m_slackY > 0; }

	// The picture moved by dx, dy pixels (positive: its content goes left or
	// up, as TiledMap::ScrollPixels), by moving its start within the spare
	// room. The strip it uncovers holds whatever lay there. When the room
	// runs out that way the picture is copied once to the far side of it.
	// clearUncovered: that strip is set to black (for where nothing will be
	// painted into it).
	// FALSE (nothing done) without spare room, when locked, or for a step
	// as big as the picture.
	BOOL ScrollOrigin(sint32 dx, sint32 dy, BOOL clearUncovered = FALSE);

	// The picture's first pixel, locked or not (Buffer() is NULL while no
	// lock is held, after the first one).
	uint8 * Pixels(void) const { return m_pixels; }

	static uint32 m_AmigaSurfaceClassId;

protected:
	uint8 * m_pixels;   // the picture's first pixel
	uint8 * m_block;    // the memory it lies in (the same without spare room)
	sint32  m_slackX;
	sint32  m_slackY;
	sint32  m_originX;  // where the picture starts in the block, 0 .. 2 * slack
	sint32  m_originY;
};

typedef aui_AmigaSurface aui_NativeSurface;

#endif // defined(__AUI_USE_AMIGA__)

#endif

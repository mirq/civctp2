//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga (68060 / WarpOS) backend of the AUI: common base
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
// The Amiga counterpart of ui/aui_sdl: an RTG screen (cybergraphics), IDCMP
// input and AHI sound, all through os/amiga/amiga_platform.h. Selected by
// __AUI_USE_AMIGA__ (ui/aui_common/auicfg.h).
//
//----------------------------------------------------------------------------

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#ifndef __aui_amiga__aui_amiga__h__
#define __aui_amiga__aui_amiga__h__ 1

#include "ctp2_config.h"

#if defined(__AUI_USE_AMIGA__)

class aui_Amiga
{
public:
	aui_Amiga() : m_exclusiveMode(FALSE) { m_AmigaRefCount++; }
	virtual ~aui_Amiga();

protected:
	AUI_ERRCODE InitCommon(BOOL useExclusiveMode);

public:
	virtual BOOL IsThisA(uint32 classId)
	{
		return classId == m_AmigaClassId;
	}

	BOOL GetExclusiveMode(void) { return m_exclusiveMode; }

	// The DirectDraw object of the DirectX backend, the screen surface of the
	// SDL one. Nothing here: the surfaces are plain memory.
	static void * DD() { return NULL; }

protected:
	BOOL            m_exclusiveMode;

private:
	static sint32   m_AmigaRefCount;

protected:
	static uint32   m_AmigaClassId;
};

typedef aui_Amiga aui_Native;

#endif // defined(__AUI_USE_AMIGA__)

#endif

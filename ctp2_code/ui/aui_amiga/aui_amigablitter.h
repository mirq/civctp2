//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga backend: blitter
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
// The surfaces are memory buffers, so aui_Blitter's software blits do it all
// (the SDL blitter's SDL paths only ran for surfaces without a buffer, which
// never exist there either).
//
//----------------------------------------------------------------------------

#ifndef __AUI_AMIGABLITTER_H__
#define __AUI_AMIGABLITTER_H__

#ifdef __AUI_USE_AMIGA__

#include "aui_blitter.h"

class aui_AmigaBlitter : public aui_Blitter
{
public:
	aui_AmigaBlitter() {}
	virtual ~aui_AmigaBlitter() {}
};

typedef aui_AmigaBlitter aui_NativeBlitter;

#endif

#endif

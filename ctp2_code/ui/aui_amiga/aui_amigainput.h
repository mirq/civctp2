//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga backend: input base
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

#ifndef __aui_amiga__aui_amigainput__h__
#define __aui_amiga__aui_amigainput__h__ 1

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#include "ctp2_config.h"

#if defined(__AUI_USE_AMIGA__)

#include "aui_input.h"
#include "aui_amiga.h"

class aui_AmigaInput : public virtual aui_Input, public aui_Amiga
{
public:
	aui_AmigaInput(AUI_ERRCODE *retval, BOOL useExclusiveMode = FALSE) {}
	virtual ~aui_AmigaInput() {}

protected:
	aui_AmigaInput() {}

public:
	virtual AUI_ERRCODE Acquire( void ) { return AUI_ERRCODE_OK; }
	virtual AUI_ERRCODE Unacquire( void ) { return AUI_ERRCODE_OK; }
};

#endif // defined(__AUI_USE_AMIGA__)

#endif

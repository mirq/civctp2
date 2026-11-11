//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Amiga backend: common base
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

#include "aui_ui.h"
#include "aui_uniqueid.h"
#include "aui_amiga.h"

uint32 aui_Amiga::m_AmigaClassId    = aui_UniqueId();
sint32 aui_Amiga::m_AmigaRefCount   = 0;

AUI_ERRCODE aui_Amiga::InitCommon(BOOL useExclusiveMode)
{
	m_exclusiveMode = useExclusiveMode;
	return AUI_ERRCODE_OK;
}

aui_Amiga::~aui_Amiga()
{
	--m_AmigaRefCount;
}

#endif // __AUI_USE_AMIGA__

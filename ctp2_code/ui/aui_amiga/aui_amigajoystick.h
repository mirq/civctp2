//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga backend: joystick (not used by the game)
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

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#ifndef __aui_amiga__aui_amigajoystick_h__
#define __aui_amiga__aui_amigajoystick_h__ 1

#include "ctp2_config.h"

#if defined(__AUI_USE_AMIGA__)

#include "aui_joystick.h"
#include "aui_amigainput.h"

class aui_AmigaJoystick : public aui_Joystick, public aui_AmigaInput
{
public:
	aui_AmigaJoystick(AUI_ERRCODE *retval, uint32 lMin = -1000, uint32 lMax = 1000)
	:   m_lMax(lMax), m_lMin(lMin) { m_deviceName[0] = 0; }
	virtual ~aui_AmigaJoystick() {}

	void SetDeviceName( MBCHAR *name ) { strncpy( m_deviceName, name, sizeof(m_deviceName) - 1 ); }
	MBCHAR *DeviceName( void ) { return m_deviceName; }
	uint32 GetLowerMin ( void ) { return m_lMin; }
	uint32 GetUpperMax ( void ) { return m_lMax; }

protected:
	aui_AmigaJoystick() {}

public:
	virtual AUI_ERRCODE Acquire( void ) { return aui_AmigaInput::Acquire(); }
	virtual AUI_ERRCODE Unacquire( void ) { return aui_AmigaInput::Unacquire(); }

protected:
	MBCHAR	m_deviceName[40];
	uint32	m_lMax;
	uint32	m_lMin;
};

#endif // defined(__AUI_USE_AMIGA__)

#endif

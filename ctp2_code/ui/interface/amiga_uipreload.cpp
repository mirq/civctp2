//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Amiga: building the game's main windows ahead of their use
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
// Modifications from the original Activision code:
//
// - New file (Amiga port): ENV:CTP2_UIPRELOAD.
//
//----------------------------------------------------------------------------
//
// A window of the game is built from its layout the first time it is opened
// and kept from then on: on a 68060 that first build takes ~0.5-3 s (the
// build manager after the first city, ~1.5 s with its fonts). ENV:CTP2_UIPRELOAD
// chooses when:
//
//   lazy:   at the first open, as on the other platforms;
//   start:  all of them while the game or saved game loads (the load takes
//           those seconds longer);
//   idle (or unset): one at a time on the map while the player has not
//           touched the mouse or keyboard for a while (a short pause each).
//
// The windows are only built here, never shown: their content is filled in
// when they are opened, as before.
//
//----------------------------------------------------------------------------

#include "c3.h"

#if defined(CTP2_AMIGA_BUILD)

#include <stdlib.h>
#include <string.h>

#include "amiga_platform.h"

#include "EditQueue.h"
#include "citywindow.h"
#include "NationalManagementDialog.h"
#include "DomesticManagementDialog.h"
#include "ScienceManagementDialog.h"
#include "trademanager.h"
#include "unitmanager.h"
#include "diplomacywindow.h"
#include "intelligencewindow.h"
#include "armymanagerwindow.h"

extern sint32 g_modalWindow;

namespace
{
	enum { PRELOAD_LAZY = 0, PRELOAD_START = 1, PRELOAD_IDLE = 2 };

	int Mode()
	{
		static int s_mode = -1;
		if (s_mode < 0)
		{
			char buffer[32];
			char const * value = amiga_env_value("CTP2_UIPRELOAD", buffer, sizeof(buffer)) ? buffer : NULL;
			// Unset: while idle (the first opening of a window cost 0.5-3 s).
			s_mode = (value && strncmp(value, "lazy", 4) == 0) ? PRELOAD_LAZY : PRELOAD_IDLE;
			// (Also by the names "preload" and "lazypreload".)
			if (value && (strncmp(value, "start", 5) == 0 || strcmp(value, "preload") == 0)) s_mode = PRELOAD_START;
			if (value && (strncmp(value, "idle",  4) == 0 || strncmp(value, "lazypreload", 11) == 0
			           || strncmp(value, "lazy-preload", 12) == 0)) s_mode = PRELOAD_IDLE;
			amiga_log("UI preload: %s\n", s_mode == PRELOAD_START ? "at the start" :
			                              s_mode == PRELOAD_IDLE  ? "while idle" : "lazy (at first use)");
		}
		return s_mode;
	}

	void BuildEditQueue()    { EditQueue::Preload(); }
	void BuildCityWindow()   { CityWindow::Initialize(); }
	void BuildNational()     { NationalManagementDialog::Preload(); }
	void BuildDomestic()     { DomesticManagementDialog::Preload(); }
	void BuildScience()      { ScienceManagementDialog::Preload(); }
	void BuildTrade()        { TradeManager::Initialize(); }
	void BuildUnits()        { UnitManager::Initialize(); }
	void BuildDiplomacy()    { DiplomacyWindow::Initialize(); }
	void BuildIntelligence() { IntelligenceWindow::Initialize(); }
	void BuildArmies()       { ArmyManagerWindow::Initialize(); }

	// Most used first: the build manager opens after every new city.
	struct Step { char const * name; void (*build)(); };
	Step const s_steps[] =
	{
		{ "build manager",      BuildEditQueue    },
		{ "city window",        BuildCityWindow   },
		{ "national manager",   BuildNational     },
		{ "science manager",    BuildScience      },
		{ "domestic manager",   BuildDomestic     },
		{ "unit manager",       BuildUnits        },
		{ "trade manager",      BuildTrade        },
		{ "diplomacy",          BuildDiplomacy    },
		{ "intelligence",       BuildIntelligence },
		{ "army manager",       BuildArmies       },
	};
	size_t const s_stepCount = sizeof(s_steps) / sizeof(s_steps[0]);
	size_t       s_next      = 0;

	void RunStep()
	{
		Step const & step  = s_steps[s_next++];
		uint32 const start = amiga_ticks_ms();
		step.build();   // a window built before (opened already) is kept as it is
		amiga_log("UI preload: %s in %lu ms\n", step.name, (unsigned long)(amiga_ticks_ms() - start));
	}
}

void amiga_uipreload_start(void)
{
	if (Mode() != PRELOAD_START) return;

	uint32 const start = amiga_ticks_ms();
	while (s_next < s_stepCount)
	{
		RunStep();
	}
	amiga_log("UI preload: all windows in %lu ms\n", (unsigned long)(amiga_ticks_ms() - start));
}

void amiga_uipreload_idle(void)
{
	if (Mode() != PRELOAD_IDLE || s_next >= s_stepCount) return;

	// Hands off the mouse and keyboard for 1.5 s, no dialog up; the next
	// window only a while after the last (the frame it costs is the pause).
	static uint32 s_lastStep = 0;
	uint32 const  now = amiga_ticks_ms();
	if (amiga_input_idle_ms() < 1500 || g_modalWindow > 0) return;
	if (s_lastStep && now - s_lastStep < 1000) return;

	RunStep();
	s_lastStep = amiga_ticks_ms();
}

#endif // CTP2_AMIGA_BUILD

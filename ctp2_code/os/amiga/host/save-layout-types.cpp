// The classes whose raw saved bytes differ between the PowerPC builds before
// SaveLayout.h (natural alignment) and the 68k layout every Amiga build uses
// now. save-layout-maps.py compiles this twice with the WarpOS compiler (-g,
// with and without -DCTP2_SAVE_LAYOUT_NATURAL) and writes their field maps.
#include "c3.h"
#include "Cell.h"
#include "EndGameRecord.h"
#include "Happy.h"
#include "player.h"
#include "thronedb.h"
#include "diplomattypes.h"
#include "Foreigner.h"
#include "Diffcly.h"
#include "PlayHap.h"
#include "BldQue.h"
#include "GameEventArgument.h"
#include "SlicSymbol.h"

//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header file
// Description  : One saved-game layout for every Amiga build
//
//----------------------------------------------------------------------------
//
// The game saves many objects as raw memory (CivArchive::StoreChunk, or
// Store/Load of sizeof(T)), so a save file holds the compiler's layout of
// those classes. The 68k Amiga ABI aligns int, pointers and doubles to 2
// bytes; the PowerPC builds (WarpOS, MorphOS) align them naturally. Saves
// made on one did not load on the other: the WarpOS build read every map
// cell of a 68060 save 2 bytes off and wrote outside an array.
//
// SAVE_LAYOUT_BEGIN / SAVE_LAYOUT_END around such a class give the PowerPC
// builds the 68k layout (2-byte packing), so one save format serves them all.
// The 68k build, AROS and the other platforms are unchanged. Which classes:
// the ones whose saved bytes differ (build/layout/chunks.py compares both
// compilers' offsets of every member).
//
// CTP2_SAVE_LAYOUT_NATURAL turns the packing off: the layout of the PowerPC
// builds before this header (only for save-layout-maps.py and tests).
//
// Saves of those older PowerPC builds load through SAVE_LAYOUT_LOAD when
// g_saveLayoutOldPpc is set (the "oldsave" argument, civ3_main.cpp): the
// old bytes of each such range are read and every field is copied to its
// place in the 68k layout (SaveLayout.cpp, maps in SaveLayoutMaps.h).
//
//----------------------------------------------------------------------------

#ifndef SAVE_LAYOUT_H
#define SAVE_LAYOUT_H

#if (defined(CTP2_AMIGA_WOS) || defined(CTP2_AMIGA_MOS)) && !defined(CTP2_SAVE_LAYOUT_NATURAL)
#define SAVE_LAYOUT_BEGIN _Pragma("pack(push, 2)")
#define SAVE_LAYOUT_END   _Pragma("pack(pop)")
#else
#define SAVE_LAYOUT_BEGIN
#define SAVE_LAYOUT_END
#endif

#if defined(CTP2_AMIGA_BUILD)
#include <stddef.h>     // size_t

class CivArchive;

struct SaveLayoutCopy
{
	unsigned short  to;         // in the object (68k layout)
	unsigned short  from;       // in the old PowerPC bytes
	unsigned short  length;
};

struct SaveLayoutMap
{
	unsigned short          newSize;    // bytes of the range in memory
	unsigned short          oldSize;    // bytes of the range in an old save
	unsigned short          count;
	SaveLayoutCopy const *  copies;
};

extern bool g_saveLayoutOldPpc;

// count objects of the range, newSize bytes apart in memory.
void saveLayout_Load(CivArchive & archive, void * object, SaveLayoutMap const & map, size_t count);

extern SaveLayoutMap const g_saveLayoutCell;
extern SaveLayoutMap const g_saveLayoutHappy;
extern SaveLayoutMap const g_saveLayoutPlayer;
extern SaveLayoutMap const g_saveLayoutThreat;
extern SaveLayoutMap const g_saveLayoutRegardEvent;
extern SaveLayoutMap const g_saveLayoutNegotiationEvent;
extern SaveLayoutMap const g_saveLayoutOldNegotiationEvent;
extern SaveLayoutMap const g_saveLayoutDifficulty;
extern SaveLayoutMap const g_saveLayoutPlayerHappiness;
extern SaveLayoutMap const g_saveLayoutBuildNode;
extern SaveLayoutMap const g_saveLayoutAgreement;
extern SaveLayoutMap const g_saveLayoutGameEventArgumentData;
extern SaveLayoutMap const g_saveLayoutSlicSymbolValue;

// archive.Load(object, size) of a range saved as raw memory, or, for a save
// of an older PowerPC build, the conversion of map's range.
#define SAVE_LAYOUT_LOAD(archive, object, size, map) \
	do { \
		if (g_saveLayoutOldPpc) saveLayout_Load((archive), (object), g_saveLayout##map, 1); \
		else                    (archive).Load((uint8 *)(object), (size)); \
	} while (0)
#endif // CTP2_AMIGA_BUILD

#endif // SAVE_LAYOUT_H

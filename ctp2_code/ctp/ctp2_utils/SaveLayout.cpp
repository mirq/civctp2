//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Loading saves of the older PowerPC Amiga builds
//
//----------------------------------------------------------------------------
//
// The WarpOS and MorphOS builds before SaveLayout.h saved some classes in
// the PowerPC compiler's natural layout. With the "oldsave" argument such a
// save loads: each of those ranges is read in its old size and its fields
// are copied to where the 68k layout (every Amiga build now) keeps them.
// Saved again, the game is in the one Amiga format.
//
//----------------------------------------------------------------------------

#include "c3.h"
#include "SaveLayout.h"

#if defined(CTP2_AMIGA_BUILD)

#include <string.h>
#include "civarchive.h"

bool g_saveLayoutOldPpc = false;

#include "SaveLayoutMaps.h"

void saveLayout_Load(CivArchive & archive, void * object, SaveLayoutMap const & map, size_t count)
{
	uint8   buffer[640];    // the largest range but the Player's
	uint8 * old = (map.oldSize <= sizeof(buffer)) ? buffer : new uint8[map.oldSize];

	for (size_t i = 0; i < count; ++i)
	{
		uint8 * const to = static_cast<uint8 *>(object) + i * map.newSize;
		archive.Load(old, map.oldSize);
		for (unsigned c = 0; c < map.count; ++c)
		{
			memcpy(to + map.copies[c].to, old + map.copies[c].from, map.copies[c].length);
		}
	}

	if (old != buffer) delete [] old;
}

#endif // CTP2_AMIGA_BUILD

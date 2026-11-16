//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Sprite Group List
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
// Compiler flags
//
//
//----------------------------------------------------------------------------
//
// Modifications from the original Activision code:
//
// - Increased sprite limit from 200 to 255.
// - Marked Microsoft extensions.
// - Removed useless - and memory wasting - SpriteList dependency.
// - Amiga builds: groups stay fully loaded after their animations (a cache).
//
//----------------------------------------------------------------------------

#if defined(HAVE_PRAGMA_ONCE)
#pragma once
#endif

#ifndef SPRITEGROUPLIST_H__
#define SPRITEGROUPLIST_H__

//----------------------------------------------------------------------------
// Library dependencies
//----------------------------------------------------------------------------

#if defined(CTP2_AMIGA_BUILD)
#include <vector>
#endif

//----------------------------------------------------------------------------
// Export overview
//----------------------------------------------------------------------------

class	SpriteGroupList;

enum	SPRITELISTERR
{
		SPRITELISTERR_OK,
		SPRITELISTERR_NOTFOUND,
		SPRITELISTERR_BADTYPE,
		SPRITELISTERR_MAX
};

//----------------------------------------------------------------------------
// Project dependencies
//----------------------------------------------------------------------------

#include "Action.h"			// GAME_ACTION
#include "c3types.h"		// uint8, uint32
#include "SpriteGroup.h"	// GROUPTYPE, LOADTYPE, SpriteGroup

//----------------------------------------------------------------------------
// Class declarations
//----------------------------------------------------------------------------

#if USE_FORMAT_67
uint32 const    k_MAX_SPRITES   = 511;
#else
uint8  const    k_MAX_SPRITES   = 255;
// uint8 restriction: see UnitActor::Serialize. Going beyond 255 will require
// a file format update.
#endif

class SpriteGroupList
{
public:
	SpriteGroupList();
	virtual ~SpriteGroupList();

	SPRITELISTERR	LoadSprite(uint32 index, GROUPTYPE type, LOADTYPE loadType,GAME_ACTION action);

	SpriteGroup *   GetSprite(uint32 index, GROUPTYPE type, LOADTYPE loadType,GAME_ACTION action);
	bool            ReleaseSprite(uint32 index, LOADTYPE loadType);

	void            RefreshBasicLoads(GROUPTYPE groupType);

private:
	SpriteGroup *   m_spriteList[k_MAX_SPRITES];

#if defined(CTP2_AMIGA_BUILD)
	// ENV:CTP2_SPRITECACHE (units, default 8) and ENV:CTP2_EFFECTCACHE
	// (effects, default 4; 0: off): so many groups stay fully loaded once no
	// actor uses their full load any more. A unit's move read its whole
	// sprite file before the animation and its basic part again after it
	// (~640 ms a move on an A4000), and every combat flash or special effect
	// read its file again; now only the first use reads it. Least recently
	// used first out.
	bool            AmigaKeepFull(uint32 index);
	void            AmigaForget(uint32 index);
	void            AmigaFlushKept();

	std::vector<uint32> m_amigaKept;   // oldest first
#endif
};

//----------------------------------------------------------------------------
// Function declarations
//----------------------------------------------------------------------------

void spritegrouplist_Initialize(void);
void spritegrouplist_Cleanup(void);

#endif	// Multiple include guard

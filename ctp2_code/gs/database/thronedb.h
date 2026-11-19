#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif
#ifndef _BMH_THRONE_DB_H_
#define _BMH_THRONE_DB_H_

#include "dbtypes.h"
#include "SaveLayout.h"     // SAVE_LAYOUT_BEGIN, SAVE_LAYOUT_END

class CivArchive;
class Token;

SAVE_LAYOUT_BEGIN   // saved as raw memory: one layout on every Amiga (SaveLayout.h)
struct ThroneInfo {

	StringId		m_name;

	StringId		m_text;

	sint32			m_upgradeSoundID;


	MBCHAR			m_zoomedImageFilename[_MAX_PATH];

	bool			m_canZoom;


	bool			m_isCeiling;
};
SAVE_LAYOUT_END

class ThroneDB {
public:

	sint32		m_nThroneTypes;
	sint32		m_nThroneLevels;
	ThroneInfo	*m_throneInfo;

	ThroneDB();

	ThroneDB(CivArchive &archive);

	~ThroneDB();

	void Initialize(void);

	sint32 Init(MBCHAR *filename);

	ThroneInfo *GetThroneInfo( sint32 type, sint32 level ) const;

	void Serialize(CivArchive &archive);

protected:

	sint32 ParseNumber(Token *token, sint32 *number);

	sint32 ParseThroneDatabase(const MBCHAR *filename);

	sint32 CheckToken(Token *token, sint32 type, const MBCHAR *error);

	sint32 ParseAThrone(Token *throneToken, ThroneInfo *throneInfo);
};


extern ThroneDB *g_theThroneDB;

#endif

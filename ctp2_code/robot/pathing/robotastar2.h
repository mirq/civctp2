//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : A-star AI pathing algorithm
// Id           : $Id$
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
// - None
//
//----------------------------------------------------------------------------
//
// Modifications from the original Activision code:
//
// - None
//
//----------------------------------------------------------------------------

#pragma once
#ifndef __ROBOT_ASTAR2_H__
#define __ROBOT_ASTAR2_H__ 1

#include "UnitAstar.h"
#include "Cell.h"
#if defined(CTP2_AMIGA_BUILD)
#include <vector>

// ENV:CTP2_ASTATS: counts of the AI's path searches (a measurement).
// robotastar2_AmigaStatsPass starts a match pass (repeats are counted
// within one), robotastar2_AmigaStatsLog writes the totals to ctp2.log.
bool robotastar2_AmigaStatsOn(void);
void robotastar2_AmigaRequest(sint32 type, uint32 army, uint32 moveType,
                              const MapPoint & start, const MapPoint & dest, bool checkDest);
void robotastar2_AmigaCount(sint32 nodes, uint32 us);
void robotastar2_AmigaStatsPass(void);
void robotastar2_AmigaStatsLog(void);
#endif

class RobotAstar2 : public UnitAstar
{
public:

	enum PathType
	{
		PATH_TYPE_DEFAULT,
		PATH_TYPE_TRANSPORT,
		PATH_TYPE_AIRLIFT,
		PATH_TYPE_DEFENSIVE
	};

	static RobotAstar2 s_aiPathing;

	RobotAstar2();

	bool FindPath(const PathType & pathType,
	              const Army & army,
	              const uint32 & army_move_type,
	              const MapPoint & start,
	              const MapPoint & dest,
	              const bool & check_dest,
	              const float & trans_max_r,
	              Path & new_path,
	              float & total_cost,
	              sint32 additionalUnits = 0);
private:

	bool TransportPathCallback (const bool & can_enter,
	                            const MapPoint & prev,
	                            const MapPoint & pos,
	                            const bool & is_zoc,
	                            float & cost,
	                            ASTAR_ENTRY_TYPE & entry);

	bool AirliftPathCallback   (const bool & can_enter,
	                            const MapPoint & prev,
	                            const MapPoint & pos,
	                            const bool & is_zoc,
	                            float & cost,
	                            ASTAR_ENTRY_TYPE & entry);

	bool DefensivePathCallback (const bool & can_enter,
	                            const MapPoint & prev,
	                            const MapPoint & pos,
	                            const bool & is_zoc,
	                            float & cost,
	                            ASTAR_ENTRY_TYPE & entry);

	bool EntryCost(const MapPoint & prev, const MapPoint & pos,
	               float &cost, bool & is_zoc, ASTAR_ENTRY_TYPE & entry);

	// A robot can't actually know what's exerting ZOC on a tile it
	// currently can't see, so don't let UnitAstar::IsMoveZOC's always-
	// omniscient robot shortcut block AI pathfinding toward it.
	bool IsMoveZOC(const MapPoint & start, const MapPoint & dest) const;

	void RecalcEntryCost(AstarPoint *parent, AstarPoint *node,
	                     float &new_entry_cost, bool &new_is_zoc,
	                     ASTAR_ENTRY_TYPE &new_entry);

	PathType m_pathType;
	ContinentIDs m_transDestCont;
	float m_transMaxR;
	uint32 m_incursionPermission;

#if defined(CTP2_AMIGA_BUILD)
	// Amiga builds: CheckIsDangerForPos for a cell is the same throughout
	// one search (the army, its start and goal, and every unit on the map
	// stay as they are), but EntryCost asks it for every way into the cell
	// (up to eight), and it looks at eight neighbours each time. Kept per
	// cell for the search: m_dangerStamp[cell] == m_dangerGen means
	// m_dangerValue[cell] holds the answer.
	bool CheckIsDangerForPosKept(const MapPoint & pos);

	// EntryCost raises an allowed entry cost to at least 1.0, DecayOrtho
	// takes 5 % off: never below 0.95. 0.9 leaves room. ENV:CTP2_AICACHE=0:
	// no bound, every cell asked as before.
	float MinEntryCost() const;

	// The same for the transport and airlift costs: whether a cell is next
	// to a foreign army that can attack (World::IsNextToForeigner, eight
	// neighbours) and whether the army's cargo can unload there
	// (Army::CanAtLeastOneCargoUnloadAt(pos, false, !m_is_robot)).
	bool IsNextToForeignerKept(const MapPoint & pos);
	bool CargoCanUnloadAtKept(const MapPoint & pos);

	// The cell's index into the kept answers, or -1 (not kept: off the map,
	// or ENV:CTP2_AICACHE=0). Starts a new set when what they read changed.
	sint32 KeptIndex(const MapPoint & pos);

	enum { KEPT_DANGER, KEPT_FOREIGN, KEPT_UNLOAD, KEPT_FACTS };
	std::vector<uint32> m_keptStamp[KEPT_FACTS];
	std::vector<uint8>  m_keptValue[KEPT_FACTS];
	uint32              m_dangerGen    = 0;
	uint32              m_dangerArmy   = 0;
	sint32              m_dangerOwner  = -1;
	bool                m_dangerRobot  = false;
	MapPoint            m_dangerStart;
	MapPoint            m_dangerDest;
#endif
};

#endif // __ROBOT_ASTAR2_H__

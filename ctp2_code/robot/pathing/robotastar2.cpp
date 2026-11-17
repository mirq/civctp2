//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
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
// - For an AI transporter it is no more checked, whether the transporter cannot
//   unload its cargo, because the cargo has not enough move points.
// - A transporter's path does not not leave the target continent, once it
//   is on the target continent. (13-Jul-2009 Martin Gühmann)
// - Amiga builds: the danger test of a cell, whether it is next to a foreign
//   army and whether the cargo can unload there are worked out once per
//   search (CheckIsDangerForPosKept, IsNextToForeignerKept,
//   CargoCanUnloadAtKept).
//
//----------------------------------------------------------------------------

#include "c3.h"
#include "robotastar2.h"
#if defined(CTP2_AMIGA_BUILD)
#include <algorithm>
#include <set>
#include <vector>
#include "agent.h"          // Agent::AI_Cache_On
#include "amiga_platform.h" // amiga_env_flag, amiga_ticks_us, amiga_log
#endif

#include "Path.h"
#include "World.h"          // g_theWorld
#include "ArmyData.h"
#include "Diplomat.h"
#include "player.h"
#include "ctpaidebug.h"

uint32 const    INCURSION_PERMISSION_ALL    = 0xffffffffu;

RobotAstar2 RobotAstar2::s_aiPathing;

RobotAstar2::RobotAstar2()
{
	m_pathType = PATH_TYPE_DEFAULT;
}

bool RobotAstar2::IsMoveZOC(const MapPoint & start, const MapPoint & dest) const
{
	if (g_player[m_owner]->IsRobot() && !g_player[m_owner]->IsVisible(dest))
	{
		return false;
	}

	return UnitAstar::IsMoveZOC(start, dest);
}

bool RobotAstar2::TransportPathCallback (const bool & can_enter,
                                         const MapPoint & prev,
                                         const MapPoint & pos,
                                         const bool & is_zoc,
                                         float & cost,
                                         ASTAR_ENTRY_TYPE & entry )
{
	if (can_enter)
	{
		ContinentIDs cont = g_theWorld->GetContinent(pos);
		bool is_land    = ( g_theWorld->IsLand(pos) || g_theWorld->IsMountain(pos) );
		bool wrong_cont = ( cont != m_transDestCont ) && (!g_theWorld->IsCity(pos));

#if defined(CTP2_AMIGA_BUILD)
		bool occupied = (m_army->HasCargo() && (!CargoCanUnloadAtKept(pos)));
#else
		bool occupied = (m_army->HasCargo() && (!m_army.CanAtLeastOneCargoUnloadAt(pos, false, !m_is_robot)));
#endif

		// World::IsTunnel(pos) requires IsWater(pos) first, so a tunnel
		// tile also satisfies IsWater/IsShallowWater below - without this,
		// cargo standing on a tunnel (already disembarked onto its
		// walkable surface) would be treated as still aboard the ship,
		// same as if prev were open water.
		bool const prevIsTunnel = g_theWorld->IsTunnel(prev);

		// On an X-wrap map, x=0 should be seamlessly adjacent to x=width-1,
		// not a dead end. Log continent IDs on both sides whenever the
		// search actually looks at a tile near that seam, to check whether
		// GrowWater/GrowLand assign matching IDs across it. Tightly gated
		// so this doesn't flood the log for ordinary, non-seam moves.
		sint32 const mapWidth = g_theWorld->GetWidth();
		if(pos.x <= 2 || pos.x >= mapWidth - 2 || prev.x <= 2 || prev.x >= mapWidth - 2)
		{
			AI_DPRINTF(k_DBG_ASTAR, m_owner, -1, m_army.m_id,
			    ("\tWRAP_DIAG: prev(%d,%d) pos(%d,%d) start(%d,%d) dest(%d,%d) mapWidth=%d contPrev=%d/%d contPos=%d/%d transDestCont=%d/%d wrong_cont=%d occupied=%d\n",
			     prev.x, prev.y, pos.x, pos.y, m_start.x, m_start.y, m_dest.x, m_dest.y,
			     mapWidth,
			     g_theWorld->GetContinent(prev).GetLandContinent(), g_theWorld->GetContinent(prev).GetWaterContinent(),
			     cont.GetLandContinent(), cont.GetWaterContinent(),
			     m_transDestCont.GetLandContinent(), m_transDestCont.GetWaterContinent(),
			     wrong_cont, occupied));
		}

		if(occupied || wrong_cont)
		{
			if(  is_land
			   &&
			     (    (g_theWorld->IsWater(prev) && !prevIsTunnel)
			       || (g_theWorld->IsShallowWater(prev) && !prevIsTunnel)
			       || g_theWorld->IsCity(prev)
			     )
			  )
			{
				AI_DPRINTF(k_DBG_ASTAR, m_owner, -1, m_army.m_id,
				    ("\tBLOCK_DIAG: prev(%d,%d) pos(%d,%d) start(%d,%d) dest(%d,%d) occupied=%d wrong_cont=%d checkDest=%d IsCity(pos)=%d IsCity(prev)=%d contPos=%d/%d transDestCont=%d/%d\n",
				     prev.x, prev.y, pos.x, pos.y, m_start.x, m_start.y, m_dest.x, m_dest.y,
				     occupied, wrong_cont, GetCheckDest(),
				     g_theWorld->IsCity(pos), g_theWorld->IsCity(prev),
				     cont.GetLandContinent(), cont.GetWaterContinent(),
				     m_transDestCont.GetLandContinent(), m_transDestCont.GetWaterContinent()));

				cost = k_ASTAR_BIG;
				entry = ASTAR_RETRY_DIRECTION;
				return false;
			}
		}

		if(g_theWorld->IsWater(pos) || g_theWorld->IsShallowWater(pos))
		{
			// Block stepping from land back onto water once already on the
			// target continent, on the assumption that having arrived
			// there, the rest of the way should be on foot - except right
			// at the search's own start: a transport already sitting on
			// the target continent (e.g. in a coastal city) may still need
			// to leave by water for a continent shaped like a near-
			// complete ring around the world with a narrow gap, where the
			// water crossing is dramatically shorter than walking almost
			// the whole way around. Once under way, though, do not allow
			// repeatedly hopping on and off the coast elsewhere along the
			// route - only the very first step gets this exemption.
			bool const prevIsTargetContLand =
			    (   g_theWorld->IsLand(prev)
			     || g_theWorld->IsMountain(prev)
			    )
			 && !g_theWorld->IsWater(prev) // Ocean city and tunnel tiles are both land and sea, just make sure the previous one was not one of those
			 && !g_theWorld->IsShallowWater(prev)
			 &&  g_theWorld->GetContinent(prev) == m_transDestCont;

			// A tunnel tile is water for a ship, but for cargo that's
			// already disembarked it's the land bridge across a strait -
			// crossing one (or a chain of them, since each tile is
			// checked independently as pos advances) while already
			// marching on the target continent isn't "leaving" it, and
			// should cost like a land step below, not get scaled by the
			// ship's speed ratio.
			bool const posIsTargetContTunnel =
			    g_theWorld->IsTunnel(pos)
			 && g_theWorld->GetContinent(pos).GetLandContinent() == m_transDestCont.GetLandContinent();

			AI_DPRINTF(k_DBG_ASTAR, m_owner, -1, m_army.m_id,
			    ("\tLEAVE_DIAG: prev(%d,%d) pos(%d,%d) start(%d,%d) dest(%d,%d) transDestCont=%d/%d contPrev=%d/%d contPos=%d/%d prevIsTargetContLand=%d posIsTargetContTunnel=%d prev!=start=%d IsCity(prev)=%d IsCity(pos)=%d\n",
			     prev.x, prev.y, pos.x, pos.y, m_start.x, m_start.y, m_dest.x, m_dest.y,
			     m_transDestCont.GetLandContinent(), m_transDestCont.GetWaterContinent(),
			     g_theWorld->GetContinent(prev).GetLandContinent(), g_theWorld->GetContinent(prev).GetWaterContinent(),
			     g_theWorld->GetContinent(pos).GetLandContinent(), g_theWorld->GetContinent(pos).GetWaterContinent(),
			     prevIsTargetContLand, posIsTargetContTunnel, (prev != m_start), g_theWorld->IsCity(prev), g_theWorld->IsCity(pos)));

			if(prevIsTargetContLand && prev != m_start && !posIsTargetContTunnel)
			{
				// Return invalid if we leave the target continent
				cost = k_ASTAR_BIG;
				entry = ASTAR_RETRY_DIRECTION;
				return false;
			}

			if (!posIsTargetContTunnel)
			{
				cost *= m_transMaxR;
			}
		}

		if(g_theWorld->IsWater(prev) || g_theWorld->IsShallowWater(prev))
		{
			// A tunnel tile satisfies IsWater() too, so unlike a true coastal
			// landing - where this gate fires exactly once, right as the ship
			// puts cargo ashore - walking a tunnel chain re-triggers it at
			// every single hop, since prev is a tunnel (= water) the whole
			// way. Left unrestricted, that piles up danger cost for foreign
			// presence anywhere along a long corridor, far from the actual
			// target, instead of just discouraging landing too close to it.
			// Only apply it once actually next to the destination.
			bool posNextToDest = true;

			if(prevIsTunnel)
			{
				posNextToDest = false;
				MapPoint neighbor;
				for(sint32 dir = 0; dir <= SOUTH; ++dir)
				{
					if(pos.GetNeighborPosition(WORLD_DIRECTION(dir), neighbor) && neighbor == m_dest)
					{
						posNextToDest = true;
						break;
					}
				}
			}

			if(posNextToDest)
			{
				if
				  (
				       (   // If we land
				           g_theWorld->IsLand(pos)
				        || g_theWorld->IsMountain(pos)
				       )
				    && (
				          ( g_theWorld->IsOccupiedByForeigner  (pos, m_owner) // If the target is a city
				        && !g_theWorld->IsSurroundedByWater    (pos))
#if defined(CTP2_AMIGA_BUILD)
				        ||  IsNextToForeignerKept(pos)
#else
				        ||  g_theWorld->IsNextToForeigner(pos, m_owner)
#endif
				       )
				  )
				{
					cost += k_MOVE_ISDANGER_COST;
				}
				// If we do not land, just avoid some units next, for instance bombardment units
#if defined(CTP2_AMIGA_BUILD)
				else if(IsNextToForeignerKept(pos))
#else
				else if(g_theWorld->IsNextToForeigner(pos, m_owner))
#endif
				{
					cost += k_MOVE_ISDANGER_COST;
				}
			}
		}

		return true;
	}
	else
	{
		cost = k_ASTAR_BIG;
		entry = ASTAR_BLOCKED;
		return false;
	}
}

bool RobotAstar2::AirliftPathCallback (const bool & can_enter,
                                       const MapPoint & prev,
                                       const MapPoint & pos,
                                       const bool & is_zoc,
                                       float & cost,
                                       ASTAR_ENTRY_TYPE & entry )
{
	if (can_enter)
	{
		if
		  (
		        pos == m_dest
		    &&
		   (
		        g_theWorld->GetCell(pos)->GetNumUnits() > 0
		    &&  g_theWorld->GetArmyPtr(pos)->GetOwner() != m_army->GetOwner()
		    && !m_army->CanFight(*g_theWorld->GetArmyPtr(pos))
		   )
		    ||
		   (
		        g_theWorld->HasCity(pos)
		    &&  g_theWorld->GetCity(pos).GetOwner() != m_army->GetOwner()
		    && !m_army->CanAtLeastOneCaptureCity()
		   )
		  )
		{
			bool occupied = (m_army->HasCargo() && !m_army.CanAtLeastOneCargoUnloadAt(pos, false, !m_is_robot));

			if(occupied)
			{
				if(!m_army.CanAtLeastOneCargoUnloadAt(prev, false, !m_is_robot))
				{
					cost = k_ASTAR_BIG;
					entry = ASTAR_RETRY_DIRECTION;
					return false;
				}
			}
		}

		// Protects the transporter itself, not its cargo - unlike a landing
		// preference (m_dest is a single fixed tile, so cost added there
		// can't steer anything), this applies to every other tile still in
		// flight, where genuine alternate routes exist. Mirrors
		// TransportPathCallback's "if we do not land, just avoid some units
		// next" branch, which does the same for a ship still at sea.
#if defined(CTP2_AMIGA_BUILD)
		if(pos != m_dest && IsNextToForeignerKept(pos))
#else
		if(pos != m_dest && g_theWorld->IsNextToForeigner(pos, m_owner))
#endif
		{
			cost += k_MOVE_ISDANGER_COST;
		}

		return true;
	}
	else
	{
		cost = k_ASTAR_BIG;
		entry = ASTAR_BLOCKED;
		return false;
	}
}

bool RobotAstar2::DefensivePathCallback (const bool & can_enter,
                                         const MapPoint & prev,
                                         const MapPoint & pos,
                                         const bool & is_zoc,
                                         float & cost,
                                         ASTAR_ENTRY_TYPE & entry)
{
	PLAYER_INDEX pos_owner;
	PLAYER_INDEX prev_owner;

	pos_owner = g_theWorld->GetCell(pos)->GetOwner();
	if (!can_enter)
	{
		cost = k_ASTAR_BIG;
		entry = ASTAR_ENTRY_TYPE(0);
		return false;
	}

	if ((pos_owner < 0) || (m_incursionPermission & (0x1 << pos_owner)))
		return true;

	prev_owner = g_theWorld->GetCell(prev)->GetOwner();
	if((prev_owner == pos_owner) &&
	   !(m_incursionPermission & (0x1 << prev_owner)))
	{
		cost += k_MOVE_TREASPASSING_COST;
	}

	return true;
}

bool RobotAstar2::FindPath( const PathType & pathType,
                            const Army & army,
                            const uint32 & army_move_type,
                            const MapPoint & start,
                            const MapPoint & dest,
                            const bool & check_dest,
                            const float & trans_max_r,
                            Path & new_path,
                            float & total_cost,
                            sint32 additionalUnits)
{
#if defined(CTP2_AMIGA_BUILD)
	// A new search: nothing kept from the last one (CheckIsDangerForPosKept).
	if (++m_dangerGen == 0)
	{
		for (int f = 0; f < KEPT_FACTS; ++f)
			std::fill(m_keptStamp[f].begin(), m_keptStamp[f].end(), 0);
		m_dangerGen = 1;
	}
	m_dangerArmy  = army.m_id;
	m_dangerOwner = -1;   // set by KeptIndex once the search is set up
	m_dangerStart = start;
	m_dangerDest  = dest;
#endif

	sint32 cutoff = Astar_MaxSearchNodes();

	sint32 nodes_opened = 0;
	bool is_broken_path = false;
	Path bad_path;

#if defined(CTP2_AMIGA_BUILD)
	// ENV:CTP2_ASTATS: counts for robotastar2_AmigaStats (a measurement).
	struct AstarStat
	{
		sint32 const * nodes;
		uint64_t       t0;
		bool           on;
		~AstarStat() { if (on) robotastar2_AmigaCount(*nodes, (uint32)(amiga_ticks_us() - t0)); }
	} stat = { &nodes_opened, 0, robotastar2_AmigaStatsOn() };
	if (stat.on)
	{
		stat.t0 = amiga_ticks_us();
		robotastar2_AmigaRequest(pathType, army.m_id, army_move_type, start, dest, check_dest);
	}
#endif

	m_pathType = pathType;
	m_transDestCont = g_theWorld->GetContinent(dest);
	m_transMaxR = trans_max_r;

	sint32 nUnits;
	uint32 move_intersection;
	uint32 move_union;
	m_is_robot = true;

	bool isspecial, cancapture, haszoc, canbombard;
	bool isstealth, canthrowparty, canestablishembassy;
	sint32 maxattack, maxdefense;
	army->CharacterizeArmy( isspecial,
	    isstealth,
	    maxattack,
	    maxdefense,
	    cancapture,
	    haszoc,
	    canbombard,
	    canthrowparty,
	    canestablishembassy);

	if(isspecial && maxattack == 0 && !haszoc)
	{
		m_incursionPermission = INCURSION_PERMISSION_ALL;
	}
	else
	{
		m_incursionPermission =
			Diplomat::GetDiplomat(army.GetOwner()).GetIncursionPermission();
	}

	if (army_move_type != 0x0)
	{
		nUnits = 1;
		move_intersection = army_move_type;
		move_union = 0;
		m_army_minmax_move = 300.0;
		m_army_can_expel_stealth = false;
	}
	else
	{
		UnitAstar::InitArmy (army, nUnits, move_intersection, move_union,
			m_army_minmax_move);
	}

	AI_DPRINTF(k_DBG_ASTAR, army.GetOwner(), -1, army.m_id,("\n"));
	if(!UnitAstar::FindPath(army,
	                        nUnits + additionalUnits,
	                        move_intersection,
	                        move_union,
	                        start,
	                        army.GetOwner(),
	                        dest, new_path,
	                        is_broken_path,
	                        bad_path,
	                        total_cost,
	                        true,
	                        cutoff,
	                        nodes_opened,
	                        check_dest)
	){
		return false;
	}

	return !is_broken_path;
}

bool RobotAstar2::EntryCost( const MapPoint &prev,
                               const MapPoint &pos,
                               float & cost,
                               bool &is_zoc,
                               ASTAR_ENTRY_TYPE &entry )
{
	if(m_pathType == PATH_TYPE_TRANSPORT || m_pathType == PATH_TYPE_AIRLIFT)
	{
		m_isTransporter = true;
	}
	else
	{
		m_isTransporter = false;
	}

	bool r = UnitAstar::EntryCost(prev, pos, cost, is_zoc, entry);

	if (r)
	{
		switch (m_pathType)
		{
		case PATH_TYPE_TRANSPORT:
			r = TransportPathCallback(r, prev, pos, is_zoc, cost, entry);
			break;
		case PATH_TYPE_AIRLIFT:
			r = AirliftPathCallback  (r, prev, pos, is_zoc, cost, entry);
			break;
		case PATH_TYPE_DEFENSIVE:
			r = DefensivePathCallback(r, prev, pos, is_zoc, cost, entry);
			break;
		default:
			break;
		}

		if(m_is_robot && pos != m_army->RetPos() && pos != m_dest && cost < k_ASTAR_BIG)
		{
#if defined(CTP2_AMIGA_BUILD)
			if(CheckIsDangerForPosKept(pos))
#else
			if(CheckIsDangerForPos(pos))
#endif
			{
				cost      += k_MOVE_ISDANGER_COST;
			}
		}

		AI_DPRINTF(k_DBG_ASTAR, m_owner, -1, m_army.m_id,
		    ("\tCheckEnter, StartPos (%d, %d), DestPos (%d, %d), ThisPos (%d, %d), NextPos (%d, %d), IsZoc: %d, EntryType: %d, Cost: %f\n", m_start.x, m_start.y, m_dest.x, m_dest.y, prev.x, prev.y, pos.x, pos.y, is_zoc, entry, cost));

		if (cost < 1.0)
		{
			cost = 1.0;
		}
		else if ((k_ASTAR_BIG <= cost) && (entry != ASTAR_RETRY_DIRECTION))
		{
			return false;
		}
	}

	return r;
}

#if defined(CTP2_AMIGA_BUILD)
// ENV:CTP2_ASTATS: what the AI's path searches cost, and how many asked the
// same as one before in the same match pass (a measurement only).
namespace
{
	sint32   s_astatsOn = -1;
	uint32   s_astatCalls, s_astatRepeats, s_astatBig, s_astatPasses;
	uint64_t s_astatNodes, s_astatUs;
	std::set<std::vector<sint32> > s_astatSeen;
}

bool robotastar2_AmigaStatsOn(void)
{
	if (s_astatsOn < 0) s_astatsOn = amiga_env_flag("CTP2_ASTATS") ? 1 : 0;
	return s_astatsOn != 0;
}

void robotastar2_AmigaRequest(sint32 type, uint32 army, uint32 moveType,
                              const MapPoint & start, const MapPoint & dest, bool checkDest)
{
	std::vector<sint32> key(8);
	key[0] = type; key[1] = (sint32)army; key[2] = (sint32)moveType;
	key[3] = start.x; key[4] = start.y; key[5] = dest.x; key[6] = dest.y; key[7] = checkDest;
	if (!s_astatSeen.insert(key).second) ++s_astatRepeats;
}

void robotastar2_AmigaCount(sint32 nodes, uint32 us)
{
	++s_astatCalls;
	s_astatNodes += (nodes > 0) ? nodes : 0;
	s_astatUs    += us;
	if (nodes >= Astar_MaxSearchNodes() / 2) ++s_astatBig;
}

void robotastar2_AmigaStatsPass(void)
{
	if (!robotastar2_AmigaStatsOn()) return;
	++s_astatPasses;
	s_astatSeen.clear();
}

void robotastar2_AmigaStatsLog(void)
{
	if (!robotastar2_AmigaStatsOn()) return;
	amiga_log("AStats: %lu searches in %lu passes, %lu ms, %lu nodes opened, %lu over half the cutoff, %lu repeats in a pass\n",
	          (unsigned long)s_astatCalls, (unsigned long)s_astatPasses, (unsigned long)(s_astatUs / 1000),
	          (unsigned long)s_astatNodes, (unsigned long)s_astatBig, (unsigned long)s_astatRepeats);
}

//----------------------------------------------------------------------------
//
// Name       : RobotAstar2::CheckIsDangerForPosKept
//
// Description: CheckIsDangerForPos, worked out once per cell and search.
//
// Remark(s)  : What CheckIsDangerForPos reads that could differ within a
//              FindPath (the army, where it is, the goal) is compared each
//              time; a difference starts a new set. ENV:CTP2_AICACHE=0:
//              asked every time, as before.
//
//----------------------------------------------------------------------------
sint32 RobotAstar2::KeptIndex(const MapPoint & pos)
{
	if (!Agent::AI_Cache_On())
		return -1;

	sint32 const width = g_theWorld->GetXWidth();
	sint32 const cells = width * g_theWorld->GetYHeight();
	if (pos.x < 0 || pos.y < 0 || pos.x >= width || pos.y * width + pos.x >= cells)
		return -1;

	if (m_keptStamp[0].size() != static_cast<size_t>(cells))
	{
		for (int f = 0; f < KEPT_FACTS; ++f)
		{
			m_keptStamp[f].assign(cells, 0);
			m_keptValue[f].assign(cells, 0);
		}
		m_dangerGen = 1;
	}

	MapPoint const start = m_army->RetPos();
	if (   m_army.m_id != m_dangerArmy || start != m_dangerStart || m_dest != m_dangerDest
	    || m_owner != m_dangerOwner || m_is_robot != m_dangerRobot)
	{
		if (++m_dangerGen == 0)
		{
			for (int f = 0; f < KEPT_FACTS; ++f)
				std::fill(m_keptStamp[f].begin(), m_keptStamp[f].end(), 0);
			m_dangerGen = 1;
		}
		m_dangerArmy  = m_army.m_id;
		m_dangerStart = start;
		m_dangerDest  = m_dest;
		m_dangerOwner = m_owner;
		m_dangerRobot = m_is_robot;
	}

	return pos.y * width + pos.x;
}

bool RobotAstar2::CheckIsDangerForPosKept(const MapPoint & pos)
{
	sint32 const index = KeptIndex(pos);
	if (index < 0)
		return CheckIsDangerForPos(pos);

	if (m_keptStamp[KEPT_DANGER][index] != m_dangerGen)
	{
		m_keptValue[KEPT_DANGER][index] = CheckIsDangerForPos(pos) ? 1 : 0;
		m_keptStamp[KEPT_DANGER][index] = m_dangerGen;
	}

	return m_keptValue[KEPT_DANGER][index] != 0;
}

bool RobotAstar2::IsNextToForeignerKept(const MapPoint & pos)
{
	sint32 const index = KeptIndex(pos);
	if (index < 0)
		return g_theWorld->IsNextToForeigner(pos, m_owner);

	if (m_keptStamp[KEPT_FOREIGN][index] != m_dangerGen)
	{
		m_keptValue[KEPT_FOREIGN][index] = g_theWorld->IsNextToForeigner(pos, m_owner) ? 1 : 0;
		m_keptStamp[KEPT_FOREIGN][index] = m_dangerGen;
	}

	return m_keptValue[KEPT_FOREIGN][index] != 0;
}

bool RobotAstar2::CargoCanUnloadAtKept(const MapPoint & pos)
{
	sint32 const index = KeptIndex(pos);
	if (index < 0)
		return m_army.CanAtLeastOneCargoUnloadAt(pos, false, !m_is_robot);

	if (m_keptStamp[KEPT_UNLOAD][index] != m_dangerGen)
	{
		m_keptValue[KEPT_UNLOAD][index] = m_army.CanAtLeastOneCargoUnloadAt(pos, false, !m_is_robot) ? 1 : 0;
		m_keptStamp[KEPT_UNLOAD][index] = m_dangerGen;
	}

	return m_keptValue[KEPT_UNLOAD][index] != 0;
}
#endif

void RobotAstar2::RecalcEntryCost(AstarPoint *parent,
    AstarPoint *node, float &new_entry_cost,
    bool &new_is_zoc, ASTAR_ENTRY_TYPE &new_entry)
{
	new_entry = ASTAR_CAN_ENTER;
	UnitAstar::RecalcEntryCost(parent,
							   node,
							   new_entry_cost,
							   new_is_zoc,
							   new_entry);

	if ((new_entry_cost < k_ASTAR_BIG) && (new_entry == ASTAR_CAN_ENTER))
	{
		switch (m_pathType)
		{
			case PATH_TYPE_TRANSPORT:
				TransportPathCallback(true, parent->m_pos, node->m_pos, new_is_zoc,
									  new_entry_cost, new_entry);
				break;
			case PATH_TYPE_AIRLIFT:
				AirliftPathCallback  (true, parent->m_pos, node->m_pos, new_is_zoc,
									  new_entry_cost, new_entry);
				break;
			case PATH_TYPE_DEFENSIVE:
				DefensivePathCallback(true, parent->m_pos, node->m_pos, new_is_zoc,
									  new_entry_cost, new_entry);
				break;
			default:
				break;
		}
	}
}

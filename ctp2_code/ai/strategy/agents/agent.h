//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Goal handling
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
// - Improved design
// - Made FindPath and GetRounds methods more flexible. (25-Jan-2008 Martin Gühmann)
// - Redesigned AI, so that the matching algorithm is now a greedy algorithm. (13-Aug-2008 Martin Gühmann)
// - Fixed unit garrison assignment. (23-Jan-2009 Martin Gühmann)
// - Merged in CTPAgent, removed virtual functions, for design and speed
//   improvement. (29-Jan-2009 Martin Gühmann)
// - Merged in Squad, no need for an additional class, just wastes space. (29-Jan-2009 Martin Gühmann)
// - Removed last Squad remainings, no need to allocate all that memory. (14-Feb-2009 Martin Gühmann)
//
//----------------------------------------------------------------------------

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif

#ifndef __AGENT_H__
#define __AGENT_H__ 1

class Agent;

#include "c3.h"

#include "Goal.h"               // Needed here to instantaite std::greater<Goal_ptr> correctly
#include "scheduler_types.h"
#include "squad_Strength.h"
#include "Army.h"               // Army
#include "MapPoint.h"           // MapPoint
#include "Path.h"
#include "player.h"             // PLAYER_INDEX
#include "scheduler_types.h"    // SQUAD_CLASS, Squad_Strength, etc.
#include "GameEventTypes.h"
#include "c3debugstl.h"
#include "Army.h"
#include "ArmyData.h"
#include "World.h"          // g_theWorld
#if defined(CTP2_AMIGA_BUILD)
#include <vector>
class OrderRecord;
#endif

class Agent
{
public:
#if defined(_DEBUG)
	typedef std::list<Goal_ptr, dbgallocator<Goal_ptr> > Goal_Ref_List;
#else
	typedef std::list<Goal_ptr> Goal_Ref_List;
#endif

private:
	Agent();
public:
	explicit Agent(const Army & army);
	Agent(const Agent & an_Original);
	~Agent();

	Agent& operator= (const Agent & an_Original);

	sint16 Get_Type() const { return m_agent_type; };

	void Set_Type(const sint16 & type) { m_agent_type = type; };

	SQUAD_CLASS Get_Squad_Class() const { return m_squad_class; };

	void     Set_Goal(Goal_ptr goal)                 { m_goal = goal; };
	Goal_ptr Get_Goal()                        const { return m_goal; };
	bool     Has_Any_Goal()                    const { return m_goal != NULL; };
	bool     Has_Goal(const Goal * const goal) const { return m_goal == goal; };
	sint32   Get_Goal_Type()                   const { return Has_Any_Goal() ? m_goal->Get_Goal_Type() : -1; }

	bool Get_Is_Dead() const { return !m_army.IsValid() || m_army->GetOwner() != m_playerId; }
;

	void Set_Squad_Class(const SQUAD_CLASS & squad_class) { m_squad_class = squad_class; };

	SQUAD_CLASS Compute_Squad_Class();

	const Squad_Strength & Get_Squad_Strength() const { return m_squad_strength; };

	const Squad_Strength & Compute_Squad_Strength();

	void Set_Can_Be_Executed(const bool &can_be_executed) { m_can_be_executed = can_be_executed; };

	bool Get_Can_Be_Executed() const { return m_can_be_executed && !m_detached; };

	void Set_Detached(const bool detached) { m_detached = detached; };

	bool Get_Detached() const { return m_detached; };

	void Log_Debug_Info(const int & log, const Goal * const goal) const;

	const Army & Get_Army() const { return m_army; };

	PLAYER_INDEX Get_Player_Number() const { return m_playerId; };

	MapPoint Get_Pos() const { return m_army.IsValid() ? m_army->RetPos() : MapPoint(); };

	sint32 GetUnitsAtPos() const { return g_theWorld->GetCell(Get_Pos())->GetNumUnits(); };
	bool IsArmyPosFilled() const { return g_theWorld->GetCell(Get_Pos())->GetNumUnits() >= k_MAX_ARMY_SIZE; };
	bool IsOneArmyAtPos () const { return g_theWorld->GetCell(Get_Pos())->GetNumUnits() == Get_Army()->Num(); };

	bool CanMove() const { return m_army.IsValid() && m_army->CanMove(); };

	// Shared A* solver (PATH_TYPE_TRANSPORT) behind all transport-related
	// task pathfinding below - it doesn't itself know or care whether the
	// mover is empty, being picked up, or already loaded.
	bool FindPathForTransportTasks( const uint32 & move_intersection, const MapPoint & dest_pos, const bool & check_dest, Path & found_path, sint32 additionalUnits = 0);

	// The three wrappers below exist purely so each call site gets a name
	// that actually describes what it's doing there, since the three
	// scenarios that need this pathing are conceptually different even
	// though they share the same underlying solver.

	// An empty transport moving to rendezvous with cargo waiting to be
	// picked up (SUB_TASK_TRANSPORT_TO_BOARD).
	bool FindPathToPickUpCargo( const uint32 & move_intersection, const MapPoint & dest_pos, const bool & check_dest, Path & found_path, sint32 additionalUnits = 0)
	{
		return FindPathForTransportTasks(move_intersection, dest_pos, check_dest, found_path, additionalUnits);
	}

	// A land army moving to rendezvous with a transport in order to board
	// it (SUB_TASK_CARGO_TO_BOARD).
	bool FindPathToBoard( const uint32 & move_intersection, const MapPoint & dest_pos, const bool & check_dest, Path & found_path)
	{
		return FindPathForTransportTasks(move_intersection, dest_pos, check_dest, found_path);
	}

	// An already-loaded transport moving toward its actual destination,
	// cargo staying aboard the whole way (SUB_TASK_TRANSPORT_TO_GOAL).
	bool FindPathToGoalWhileLoaded( const uint32 & move_intersection, const MapPoint & dest_pos, const bool & check_dest, Path & found_path)
	{
		return FindPathForTransportTasks(move_intersection, dest_pos, check_dest, found_path);
	}

	static bool FindPath(const Army & army, const MapPoint & target_pos, const bool & check_dest, Path & found_path );
	static bool FindPath(const Army & army, const MapPoint & target_pos, const bool & check_dest, Path & found_path, float & total_cost);

	sint32 GetRounds(const MapPoint & pos, sint32 & cells) const;
	double GetRoundsPrecise(const MapPoint & pos, sint32 & cells) const;

	bool CanReachTargetContinent(const MapPoint & pos) const;
	bool EstimateTransportUtility(const Agent_ptr transport, Utility & utility) const;

	void Set_Target_Order(const sint32   &target_order) { m_targetOrder = target_order; };
	void Set_Target_Pos  (const MapPoint &target_pos)   { m_targetPos   = target_pos;   };

	sint32           Get_Target_Order() const { return m_targetOrder; };
	const MapPoint & Get_Target_Pos  () const { return m_targetPos;   };

#if 0
	void Follow_Path(const Path & found_path, const sint32 & order_type);
#endif

	void Execute_Order(const sint32 & order_type, const MapPoint & order_pos);

	void Group_Order();

	void Group_With( Agent_ptr second_army );

	void Ungroup_Order();

	sint32 DisbandObsoleteArmies();
	inline sint32 DisbandObsoleteUnits();

	void MoveIntoTransport();
	void UnloadCargo();
	bool CargoCanEnter() const { return CargoCanEnter(Get_Pos()); };
	bool CargoCanEnter(const MapPoint &pos) const { return m_army->CargoCanEnter(pos); };
	void PerformOrderHere(const OrderRecord * order_rec, const Path * path, GAME_EVENT_INSERT priority = GEV_INSERT_AfterCurrent);
	void PerformOrder(const OrderRecord * order_rec);
	void WaitHere(const MapPoint & goal_pos);
	void ClearOrders();
	bool HasMovePoints() const;

	bool IsNeededForGarrison() const { return m_neededForGarrison; };
	void SetIsNeededForGarrison(bool neededForGarrison) { m_neededForGarrison = neededForGarrison; };

#if defined(CTP2_AMIGA_BUILD)
	// Amiga builds: what the scheduler asks the army again for every goal
	// it scores the agent for (Goal::Compute_Agent_Matching_Value,
	// Goal::Pretest_Bid). While a match pass is open the answers are worked
	// out once per agent and pass. A pass is open only where no army changes
	// (Scheduler::Process_Goal_Changes); outside one, nothing is kept.
	// ENV:CTP2_AICACHE=0 turns it off.
	struct MatchFacts
	{
		Utility      tieBreaker;          // The sort tie breaker of Compute_Agent_Matching_Value
		double       minMove;             // ArmyData::MinMovementPoints
		sint32       minFuel;             // ArmyData::GetMinFuel
		sint32       transports;          // ArmyData::GetCargo
		sint32       maxCargo;
		sint32       emptyCargo;
		sint32       num;                 // ArmyData::Num
		sint32       maxAttack;           // CharacterizeArmy or CharacterizeCargo
		uint32       movementType;        // Own movement type, the cargo's added if any
		PLAYER_INDEX posOwner;            // g_theWorld->GetOwner at the army
		PLAYER_INDEX cellOwner;           // its cell's GetOwner
		bool         getCargo;            // ArmyData::GetCargo's result
		bool         hasCargo;
		bool         isAir;
		bool         canSettle;
		bool         canBeachAssault;
		bool         canSomeCargoBeachAssault;
		bool         canNukeCity;
		bool         woundedNotObsolete;  // The wounded bonus test
		bool         obsolete;            // The obsolete bonus test
		bool         isSpecial;           // CharacterizeArmy or CharacterizeCargo
		bool         hasZoc;
	};

	// ENV:CTP2_AICACHE=0 turns off this and Scheduler's goal index.
	static bool AI_Cache_On();

	static bool Match_Pass_Open() { return s_matchPass != 0; }
	static void Open_Match_Pass();
	static void Close_Match_Pass();

	// Only while a pass is open, and only for a live agent.
	const MatchFacts & Get_Match_Facts() const;

	// Whether both TestOrder and CargoTestOrder are illegal for the
	// order of a goal type (Pretest_Bid); while a pass is open, once per
	// agent, goal type and pass.
	bool Order_Is_Illegal(const OrderRecord * order_rec, sint32 goal_type) const;
#endif

#ifdef _DEBUG_SCHEDULER
	bool ContainsArmyIn(const Agent_ptr agent) const { return Get_Army() == agent->Get_Army(); };
#endif

private:

	Squad_Strength m_squad_strength;
	Goal_ptr       m_goal;
	Army           m_army;
	sint32         m_playerId;
	sint32         m_targetOrder;
	MapPoint       m_targetPos;
	SQUAD_CLASS    m_squad_class;
	sint16         m_agent_type;
	bool           m_can_be_executed;
	bool           m_detached;
	bool           m_neededForGarrison;

#if defined(CTP2_AMIGA_BUILD)
	mutable MatchFacts          m_facts;
	mutable uint32              m_factsPass   = 0;  // The pass m_facts belong to, 0: none
	mutable uint32              m_ordersPass  = 0;  // The pass m_orderIllegal belongs to
	mutable std::vector<sint8>  m_orderIllegal;     // Per goal type: -1 not known yet, 0, 1

	static uint32 s_matchPass;      // The open pass, 0: none
	static uint32 s_passCounter;    // Numbers the passes
	static sint32 s_matchCacheOn;   // -1: ENV not read yet
#endif
};

#endif // __AGENT_H__

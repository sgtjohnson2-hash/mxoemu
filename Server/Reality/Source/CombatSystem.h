// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// ***************************************************************************

#ifndef MXOEMU_COMBATSYSTEM_H
#define MXOEMU_COMBATSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "MessageTypes.h"
#include <mutex>
#include <unordered_map>
#include <string>

class PlayerObject;

// Damage types used by the MxO combat system (client.dll StatModifier/DamageType)
typedef enum
{
	DAMAGE_NONE			= 0,
	DAMAGE_MELEE		= 1,
	DAMAGE_RANGED		= 2,
	DAMAGE_VIRAL		= 3,
	DAMAGE_BALLISTIC	= 4,
	DAMAGE_HACKING		= 5,
} mxoDamageType;

// Combat tactics (client.dll string table, ILDB Abilities.ilmb MFAC values)
// Interlock button mapping: Block->Defense(3) Grab->Retaliate(0) Power(4) Speed(5)
typedef enum
{
	TACTIC_RETALIATE	= 0, //"grab" in interlock UI
	TACTIC_AIMEDSHOT	= 1,
	TACTIC_BURST		= 2,
	TACTIC_DEFENSE		= 3, //"block" in interlock UI
	TACTIC_POWER		= 4,
	TACTIC_SPEED		= 5,
	TACTIC_PRECISE		= 6,
	TACTIC_ENERGIZED	= 7,
	TACTIC_NORMAL		= 8, //default, no tactic selected
} mxoTacticType;

// Oracle premonition helper (the client->server tactic remap that lived here was invented
// and has been removed: the client sends mxoTacticType values directly)
class TacticAdapter
{
public:
    // Calculates tactical premonition chance: P = min(0.85, 0.35 + Perception/200 + (InnerStrength/MaxIS)*0.25)
    static float CalculatePremonitionChance(float perception, float innerStrength, float maxInnerStrength)
    {
        float ratio = maxInnerStrength > 0.0f ? (innerStrength / maxInnerStrength) : 0.0f;
        float p = 0.35f + (perception / 200.0f) + (ratio * 0.25f);
        return std::min(0.85f, std::max(0.35f, p));
    }
};

// Special logic flags for combat abilities (stuns, hacks, etc.)
enum mxoAbilityFlag {
	ABILITY_FLAG_NONE = 0,
	ABILITY_FLAG_STUN = 1 << 0,
	ABILITY_FLAG_MASK = 1 << 1,
	ABILITY_FLAG_TRACE = 1 << 2,
	ABILITY_FLAG_BOMB = 1 << 3
};

// Server-side combat move. The real game loads ability stats from
// gameobjects.gob - this table stands in for damage/range/cost data while the
// DataLoader ability templates provide cast times and FX for real ability ids.
struct CombatMove
{
	uint16 id;
	std::string name;
	mxoDamageType dmgType;
	float minDmg;			//damage roll floor at level 1
	float maxDmg;			//damage roll ceiling at level 1
	float minDmgPerLvl;		//MinValueScaleLevel
	float maxDmgPerLvl;		//MaxValueScaleLevel
	uint16 isCost;			//AbilityInnerStrengthCost (flat, no level scaling)
	float range;			//selection range in world units (100 units = 1m)
	uint32 hitFxId;			//FX played on the victim when the hit lands
	bool interlockOnly;		//melee specials usable only in interlock
	bool freefireOnly;		//guns/viruses usable only outside interlock
	float castTime;			//cast bar duration in seconds (0 = instant)
	uint32 specialFlags;	//mxoAbilityFlag bitmask
};

// One paired interlock encounter between two combatants
struct InterlockSession
{
	uint32 goIdA;
	uint32 goIdB;
	uint8 tacticA;
	uint8 tacticB;
	uint16 queuedMoveA;		//0 = plain attack this round
	uint16 queuedMoveB;
	uint32 nextRoundTime;	//ms timestamp (getMSTime)
	uint32 roundNumber;
	uint16 ilViewIdA;		//ILCombatHandler view spawned on A's client
	uint16 ilViewIdB;		//ILCombatHandler view spawned on B's client
	LocationVector ilPos;	//IL handler position; exchange positions are relative to it
	uint16 exchangeNum;		//last interlock exchange number sent (client plays only newer ones)
};

// Interlock move references (interlock database rez 0x24000B8B), from captured live exchanges
static const uint32 IL_MOVE_DATABASE = 0x24000B8B;
static const uint32 IL_MOVE_PRE = 0x5214;			//lead-in used on both sides in 4 of 5 captures
static const uint32 IL_MOVE_OPEN_ATTACKER_PRE = 0x523C;
static const uint32 IL_MOVE_OPEN_MAIN = 0x2026;	//main move of the captured interlock start

// One free-fire engagement (attacker keeps firing at target until it ends)
struct FreeFireState
{
	uint32 attackerGoId;
	uint32 targetGoId;
	uint16 moveId;
	uint32 nextShotTime;	//ms timestamp (getMSTime)
};

class CombatSystem : public Singleton<CombatSystem>
{
public:
	CombatSystem();
	~CombatSystem();
	void Init();

	void LoadAbilities();

	//called from the main server loop
	void Update();

	//combat requests (from client RPCs, chat commands and bot AI)
	bool RequestInterlock(uint32 attackerGoId, uint32 targetGoId, uint32 clientTargetRef = 0);
	bool RequestRangedCombat(uint32 attackerGoId, uint32 targetGoId, uint16 moveId=0);
	void SetTactic(uint32 goId, uint8 tactic);
	void QueueAbility(uint32 goId, uint16 moveId);
	void TriggerForesightPremonition(PlayerObject* player, PlayerObject* opponent, uint8 enemyTactic);
	void LeaveInterlock(uint32 goId) { EndInterlock(goId,true); }
	void EndInterlock(uint32 goId, bool byWithdraw);
	void StopFreeFire(uint32 goId);
	void RemoveCombatant(uint32 goId); //drops a combatant out of everything (death, disconnect)

	bool IsInterlocked(uint32 goId) const;
	bool IsFreeFiring(uint32 goId) const;
	// Raw pointer session access (callers must hold m_combatMutex if mutating). Prefer GetInterlockSessionCopy or WithInterlockSession.
	InterlockSession* GetInterlockSession(uint32 goId);
	bool GetInterlockSessionCopy(uint32 goId, InterlockSession& outSession) const;

	template<typename Fn>
	bool WithInterlockSession(uint32 goId, Fn&& fn)
	{
		std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
		for (auto& session : m_interlocks) {
			if (session.goIdA == goId || session.goIdB == goId) {
				fn(session);
				return true;
			}
		}
		return false;
	}

	//client ability activation (0x80b9) - resolves DataLoader templates for
	//cast time / FX / IS cost, then routes into interlock or free-fire
	bool UseAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId);

	//one attack resolution; usable directly from test commands
	bool ResolveSingleAttack(uint32 attackerGoId, uint32 targetGoId, uint16 moveId, bool inInterlock);

	static const CombatMove* GetMove(uint16 moveId);
	static const CombatMove* GetMoveByName(const string &name);
	static const CombatMove* DefaultMelee();
	static const CombatMove* DefaultRanged();
	static const CombatMove* GetDefaultStyleMove(PlayerObject* player);

	//pays XP / $Info / loot-advance for a kill; invoked from PlayerObject::die()
	void AwardKill(PlayerObject* killer, PlayerObject* victim);

	struct AttackResult
	{
		bool hit;
		uint16 damageTaken;
		bool isCrit;
		bool isBlocked;
		bool isGlancing;
	};

	AttackResult StrikeInterlock(InterlockSession &session, PlayerObject* attacker, PlayerObject* target,
		const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock, bool bypassBlock);
	ILExchange BuildExchange(const InterlockSession &session, PlayerObject* viewer, PlayerObject* attacker,
		PlayerObject* defender, uint16 number, uint32 attackerPreMove, uint32 defenderPreMove, uint32 mainMove);
	void SendInterlockExchange(InterlockSession &session, PlayerObject* attacker, PlayerObject* defender);
	uint32 SelectInterlockMove(const InterlockSession &session, PlayerObject* attacker, PlayerObject* defender);
	bool m_ilExchangeActive = false;

	AttackResult ResolveAttack(PlayerObject* attacker, PlayerObject* target,
		const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock = false, bool bypassBlock = false);

	static const float INTERLOCK_ROUND_SECONDS;
	static const float FREEFIRE_SHOT_SECONDS;
private:
	bool RunInterlockRound(InterlockSession &session); //false = session over, reap it
	bool RunFreeFireShot(FreeFireState &state); //false = engagement over, reap it
	float TacticModifier(uint8 attackerTactic, uint8 targetTactic);

	std::unordered_map<uint16, CombatMove> m_moveTable;
	std::unordered_map<std::string, const CombatMove*> m_movesByName;
	mutable std::recursive_mutex m_combatMutex;
	list<InterlockSession> m_interlocks;
	list<FreeFireState> m_freefires;

	//M3: die() -> RemoveCombatant -> EndInterlock can be reached from inside Update() while it
	//iterates m_interlocks. Those requests are queued here and replayed after the loop.
	bool m_updatingInterlocks;
	std::vector< std::pair<uint32,bool> > m_deferredInterlockEnds;
	std::vector<uint32> m_deferredFreefireStops;
};

#define sCombatSys CombatSystem::getSingleton()

#endif

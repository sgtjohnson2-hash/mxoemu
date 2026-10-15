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
#include "CombatAnimationMatrix.h"
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

// Combat states (client.dll string table ST_* values; StatusCombat_*StateIcon). CR2: a Speed hit
// leaves the target Dazed (less damage out), a Power hit Staggered (less damage resistance),
// a Grab hit Off-Balance (specials cost more Inner Strength).
enum mxoCombatState
{
	COMBAT_STATE_DAZED		= 0x02,
	COMBAT_STATE_OFFBALANCE	= 0x04,
	COMBAT_STATE_STAGGERED	= 0x08,
	COMBAT_STATE_STUNNED	= 0x20,
};

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
	uint8 committedTacticA = TACTIC_NORMAL; // stance the opponent showed last round
	uint8 committedTacticB = TACTIC_NORMAL;
	uint16 queuedMoveA;		//0 = plain attack this round
	uint16 queuedMoveB;
	uint32 nextRoundTime;	//ms timestamp (getMSTime)
	uint32 roundNumber;
	uint16 ilViewIdA;		//ILCombatHandler view spawned on A's client
	uint16 ilViewIdB;		//ILCombatHandler view spawned on B's client
	LocationVector ilPos;	//IL handler position; exchange positions are relative to it
	uint16 exchangeNum;		//last interlock exchange number sent (client plays only newer ones)
	uint32 otherRefA = 0;	//opponent slot handle (view | spawn<<16) as A's client knows it
	uint32 otherRefB = 0;	//opponent slot handle as B's client knows it
	uint8 comboStepA = 0;	//1..3 Martial Arts style combo step for combatant A
	uint8 comboStepB = 0;	//1..3 Martial Arts style combo step for combatant B
	FightingStyle lastStyleA = FightingStyle::None;
	FightingStyle lastStyleB = FightingStyle::None;
	uint16 roundMoveA = 0;	//ability/move each side swung in the last round (HUD round status)
	uint16 roundMoveB = 0;
	uint32 lastExchangeAttackerGoId = 0; //goId of attacker in the most recently emitted ILExchange
	uint16 combatStateA = 0;	//CR2 combat states on A (client.dll ST_DAZED 0x2 / ST_OFFBALANCE 0x4 / ST_STAGGERED 0x8)
	uint16 combatStateB = 0;
	uint32 stateRoundA = 0;	//round the state on A was applied (lasts through the next round)
	uint32 stateRoundB = 0;
};

// Interlock move references (interlock database rez 0x24000B8B), from captured live exchanges
static const uint32 IL_MOVE_DATABASE = 0x24000B8B;
static const uint32 IL_MOVE_PRE = 0x5214;			//lead-in used on both sides in 4 of 5 captures
static const uint32 IL_MOVE_OPEN_ATTACKER_PRE = 0x523C;
static const uint32 IL_MOVE_OPEN_MAIN = 0x2026;	//main move of the captured interlock start

// Interlock slot layout for one client (see CombatSystem.cpp; order from Reality.conf Interlock.SlotOrder)
std::vector<uint32> BuildILSlots(uint32 otherRef);
uint8 ILSlotFor(bool isViewer);

// One free-fire engagement (attacker keeps firing at target until it ends)
struct FreeFireState
{
	uint32 attackerGoId;
	uint32 targetGoId;
	uint16 moveId;
	uint32 nextShotTime;	//ms timestamp (getMSTime)
	uint32 shotCount;
	uint32 outOfRangeShots = 0; //consecutive shots skipped because the target was out of range
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
	// Who a human may attack (Combat.FactionRules, default on). Bots are not restricted here.
	// why gets a short player-facing reason when the answer is no.
	bool CanAttack(PlayerObject* attacker, PlayerObject* target, std::string* why = nullptr) const;
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
	static float ComputeBallisticDistanceMultiplier(double dist, double maxRange, uint16 abilityId = 0);

	//pays XP / $Info / loot-advance for a kill; invoked from PlayerObject::die()
	void AwardKill(PlayerObject* killer, PlayerObject* victim);

	struct AttackResult
	{
		bool hit = false;
		uint16 damageTaken = 0;
		bool isCrit = false;
		bool isBlocked = false;
		bool isGlancing = false;
	};

	AttackResult StrikeInterlock(InterlockSession &session, PlayerObject* attacker, PlayerObject* target,
		const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock, bool bypassBlock, bool emitExchange = true);
	ILExchange BuildExchange(const InterlockSession &session, PlayerObject* viewer, PlayerObject* attacker,
		PlayerObject* defender, uint16 number, uint32 attackerPreMove, uint32 defenderPreMove, uint32 mainMove,
		const InterlockAnimPair* animPair = nullptr);
	void SendInterlockExchange(InterlockSession &session, PlayerObject* attacker, PlayerObject* defender,
		const AttackResult& res, const CombatMove& move, uint8 attackerTactic, uint8 targetTactic);
	uint32 SelectInterlockMove(const InterlockSession &session, PlayerObject* attacker, PlayerObject* defender,
		InterlockExchangeOutcome outcome = InterlockExchangeOutcome::NormalHit);
	bool m_ilExchangeActive = false;
	// CR2 interlock: damage multiplier for the swing being resolved (stance + Dazed/Staggered). 0 = legacy tactic matrix.
	float m_cr2DamageMul = 0.0f;
	static bool UseCR2Interlock();
	uint16 GetCombatState(uint32 goId);
	static float CR2HitChance(PlayerObject* attacker, PlayerObject* defender, uint8 attTac, uint8 defTac);

	AttackResult ResolveAttack(PlayerObject* attacker, PlayerObject* target,
		const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock = false, bool bypassBlock = false);
	bool RunFreeFireShot(FreeFireState &state); //false = engagement over, reap it

	static const float INTERLOCK_ROUND_SECONDS;
	static const float FREEFIRE_SHOT_SECONDS;
private:
	bool RunInterlockRound(InterlockSession &session); //false = session over, reap it
	void RunCR2Exchange(InterlockSession &session, PlayerObject* pA, PlayerObject* pB,
		const CombatMove* moveA, const CombatMove* moveB, uint8 tacA, uint8 tacB);
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

	//goId -> getMSTime() until which nobody else may lock that fighter back into an interlock.
	//Set on withdraw. Live 2026-10-13: the dojo bot re-locked Slacker 1 s after &withdraw.
	std::unordered_map<uint32, uint32> m_withdrawGraceUntil;
};

#define sCombatSys CombatSystem::getSingleton()

#endif

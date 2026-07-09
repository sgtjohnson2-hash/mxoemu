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

#include "CombatSystem.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "GameClient.h"
#include "GOAttributes.h"
#include "MessageTypes.h"
#include "DataLoader.h"
#include "MissionSystem.h"
#include "BotManager.h"
#include "Timer.h"
#include "Log.h"
#include <boost/algorithm/string.hpp>

using boost::iequals;

createFileSingleton(CombatSystem);

const float CombatSystem::INTERLOCK_ROUND_SECONDS = 4.0f; //real MxO interlock rounds are 4 seconds
const float CombatSystem::FREEFIRE_SHOT_SECONDS = 2.0f;

// Known-good hit FX id captured from live combat traffic (HDS combat.log)
static const uint32 DEFAULT_HIT_FX = 0x280001C1;

// Built-in combat move table exercising every damage form. Damage model
// follows the client.dll combat resolution structure:
//   raw = random(scaledMin,scaledMax), scaled per attacker level
static const CombatMove s_moveTable[] =
{
	//id name              dmgType           min   max  min/L max/L  IS   range    fx              ILonly FFonly cast
	{ 1, "punch",          DAMAGE_MELEE,      6.0f, 12.0f, 0.8f, 1.2f,  0,   300.0f, DEFAULT_HIT_FX, false, false, 0.0f },
	{ 2, "kick",           DAMAGE_MELEE,      8.0f, 15.0f, 1.0f, 1.4f,  3,   300.0f, DEFAULT_HIT_FX, false, false, 0.0f },
	{ 3, "handgun",        DAMAGE_BALLISTIC, 10.0f, 18.0f, 1.2f, 1.6f,  0,  6000.0f, DEFAULT_HIT_FX, false, true,  0.0f },
	{ 4, "rifle",          DAMAGE_BALLISTIC, 14.0f, 24.0f, 1.4f, 2.0f,  4,  9000.0f, DEFAULT_HIT_FX, false, true,  0.5f },
	{ 5, "virus",          DAMAGE_VIRAL,     12.0f, 20.0f, 1.3f, 1.8f, 10,  5000.0f, DEFAULT_HIT_FX, false, true,  2.0f },
	{ 6, "logicbarrage",   DAMAGE_HACKING,   18.0f, 28.0f, 1.6f, 2.2f, 18,  5000.0f, DEFAULT_HIT_FX, false, true,  2.5f },
	{ 7, "hyperstrike",    DAMAGE_MELEE,     20.0f, 34.0f, 1.8f, 2.6f, 22,   300.0f, DEFAULT_HIT_FX, true,  false, 0.0f },
	{ 8, "hyperkick",      DAMAGE_MELEE,     24.0f, 40.0f, 2.0f, 3.0f, 30,   300.0f, DEFAULT_HIT_FX, true,  false, 0.0f },
	{ 9, "throwingknife",  DAMAGE_RANGED,     9.0f, 16.0f, 1.1f, 1.5f,  2,  4000.0f, DEFAULT_HIT_FX, false, true,  0.0f },
};

static const size_t s_moveTableSize = sizeof(s_moveTable)/sizeof(s_moveTable[0]);

const CombatMove* CombatSystem::GetMove( uint16 moveId )
{
	for (size_t i=0;i<s_moveTableSize;i++)
	{
		if (s_moveTable[i].id == moveId)
			return &s_moveTable[i];
	}
	return NULL;
}

const CombatMove* CombatSystem::GetMoveByName( const string &name )
{
	for (size_t i=0;i<s_moveTableSize;i++)
	{
		if (iequals(name,s_moveTable[i].name))
			return &s_moveTable[i];
	}
	return NULL;
}

const CombatMove* CombatSystem::DefaultMelee()
{
	return GetMove(1); //punch
}

const CombatMove* CombatSystem::DefaultRanged()
{
	return GetMove(3); //handgun
}

CombatSystem::CombatSystem()
{
}

CombatSystem::~CombatSystem()
{
}

static PlayerObject* getPlayerSafe(uint32 goId)
{
	if (goId == 0)
		return NULL;
	try
	{
		return sObjMgr.getGOPtr(goId);
	}
	catch (ObjectMgr::ObjectNotAvailable)
	{
		return NULL;
	}
}

void CombatSystem::Update()
{
	float currTime = getFloatTime();

	//tick interlock rounds
	for (list<InterlockSession>::iterator it=m_interlocks.begin();it!=m_interlocks.end();)
	{
		PlayerObject *pA = getPlayerSafe(it->goIdA);
		PlayerObject *pB = getPlayerSafe(it->goIdB);
		if (pA == NULL || pB == NULL || pA->isDead() || pB->isDead())
		{
			if (pA)
			{
				if (it->ilViewIdA)
				{
					pA->getClient().QueueState(shared_ptr<DeleteViewMsg>(new DeleteViewMsg(it->ilViewIdA)));
					sObjMgr.releaseDynamicView(&pA->getClient(),it->ilViewIdA);
				}
				pA->leaveInterlock();
			}
			if (pB)
			{
				if (it->ilViewIdB)
				{
					pB->getClient().QueueState(shared_ptr<DeleteViewMsg>(new DeleteViewMsg(it->ilViewIdB)));
					sObjMgr.releaseDynamicView(&pB->getClient(),it->ilViewIdB);
				}
				pB->leaveInterlock();
			}
			it = m_interlocks.erase(it);
			continue;
		}

		if (currTime >= it->nextRoundTime)
		{
			RunInterlockRound(*it);
			it->nextRoundTime = currTime + INTERLOCK_ROUND_SECONDS;
			it->roundNumber++;
		}
		++it;
	}

	//tick free-fire engagements
	for (list<FreeFireState>::iterator it=m_freefires.begin();it!=m_freefires.end();)
	{
		PlayerObject *attacker = getPlayerSafe(it->attackerGoId);
		PlayerObject *target = getPlayerSafe(it->targetGoId);

		//engagement over: someone left/died, or either side got interlocked
		if (attacker == NULL || target == NULL || attacker->isDead() || target->isDead()
			|| IsInterlocked(it->attackerGoId) || IsInterlocked(it->targetGoId))
		{
			if (attacker && !IsInterlocked(it->attackerGoId))
			{
				attacker->setInCombat(false);
				attacker->setCombatStance(false);
			}
			it = m_freefires.erase(it);
			continue;
		}

		if (currTime >= it->nextShotTime)
		{
			RunFreeFireShot(*it);
			it->nextShotTime = currTime + FREEFIRE_SHOT_SECONDS;
		}
		++it;
	}
}

bool CombatSystem::RequestInterlock( uint32 attackerGoId, uint32 targetGoId )
{
	if (attackerGoId == targetGoId)
		return false;
	if (IsInterlocked(attackerGoId) || IsInterlocked(targetGoId))
		return false;

	PlayerObject *pA = getPlayerSafe(attackerGoId);
	PlayerObject *pB = getPlayerSafe(targetGoId);
	if (pA == NULL || pB == NULL || pA->isDead() || pB->isDead())
		return false;

	//interlock cancels any running free-fire from either side
	StopFreeFire(attackerGoId);
	StopFreeFire(targetGoId);

	InterlockSession session;
	session.goIdA = attackerGoId;
	session.goIdB = targetGoId;
	session.tacticA = TACTIC_SPEED;
	session.tacticB = TACTIC_SPEED;
	session.queuedMoveA = 0;
	session.queuedMoveB = 0;
	session.nextRoundTime = getFloatTime() + INTERLOCK_ROUND_SECONDS;
	session.roundNumber = 1;
	session.ilViewIdA = 0;
	session.ilViewIdB = 0;

	//spawn the ILCombatHandler view + pairing packet on both clients - this is
	//what drives the client-side interlock camera and round UI
	float simTime = sGame.GetSimTime();
	LocationVector ilPos = pA->getPosition();

	struct SideSetup { PlayerObject* self; PlayerObject* other; uint16* viewSlot; };
	SideSetup sides[2] =
	{
		{ pA, pB, &session.ilViewIdA },
		{ pB, pA, &session.ilViewIdB },
	};

	for (int i=0;i<2;i++)
	{
		PlayerObject *self = sides[i].self;
		PlayerObject *other = sides[i].other;

		try
		{
			uint16 ilViewId = sObjMgr.allocateDynamicView(&self->getClient(),uint32(GOID_ILCOMBATHANDLER)<<16);
			*(sides[i].viewSlot) = ilViewId;

			self->getClient().QueueState(shared_ptr<SpawnILCombatHandlerMsg>(
				new SpawnILCombatHandlerMsg(ilViewId,self->nextSpawnCounter(),ilPos,simTime)));

			//the pairing references the opponent's view as this client sees it
			uint16 otherViewId = sObjMgr.getViewForGO(&self->getClient(),other->getGoId());
			uint32 otherViewWithSpawnId = uint32(otherViewId) | (uint32(2) << 16);

			self->getClient().QueueState(shared_ptr<InterlockInitMsg>(
				new InterlockInitMsg(ilViewId,ilPos,otherViewWithSpawnId,self->nextSpawnCounter())));
		}
		catch (ObjectMgr::NoMoreFreeViews)
		{
			WARNING_LOG("No free views for interlock handler spawn");
		}
		catch (ObjectMgr::ObjectNotAvailable) {}
		catch (ObjectMgr::ClientNotAvailable) {}
	}

	m_interlocks.push_back(session);

	pA->enterInterlock(targetGoId);
	pB->enterInterlock(attackerGoId);

	sBotMgr.LogCombat((format("%1% entered interlock with %2%") % pA->getHandle() % pB->getHandle()).str());
	INFO_LOG(format("Interlock started between %1% and %2%") % pA->getHandle() % pB->getHandle());
	return true;
}

bool CombatSystem::RequestRangedCombat( uint32 attackerGoId, uint32 targetGoId, uint16 moveId )
{
	PlayerObject *attacker = getPlayerSafe(attackerGoId);
	PlayerObject *target = getPlayerSafe(targetGoId);
	if (attacker == NULL || target == NULL)
		return false;
	if (attacker->isDead() || target->isDead())
		return false;
	if (IsInterlocked(attackerGoId) || IsInterlocked(targetGoId))
		return false;

	if (moveId == 0)
		moveId = DefaultRanged()->id;

	const CombatMove *move = GetMove(moveId);
	if (move == NULL || move->interlockOnly)
		return false;

	//refresh existing engagement instead of stacking a second one
	foreach(FreeFireState &state, m_freefires)
	{
		if (state.attackerGoId == attackerGoId)
		{
			state.targetGoId = targetGoId;
			state.moveId = moveId;
			return true;
		}
	}

	FreeFireState newState;
	newState.attackerGoId = attackerGoId;
	newState.targetGoId = targetGoId;
	newState.moveId = moveId;
	newState.nextShotTime = getFloatTime(); //first shot resolves on next tick
	m_freefires.push_back(newState);

	attacker->setInCombat(true);
	attacker->setCombatStance(true);

	sBotMgr.LogCombat((format("%1% opened fire on %2%") % attacker->getHandle() % target->getHandle()).str());
	return true;
}

void CombatSystem::StopFreeFire( uint32 goId )
{
	for (list<FreeFireState>::iterator it=m_freefires.begin();it!=m_freefires.end();)
	{
		if (it->attackerGoId == goId)
			it = m_freefires.erase(it);
		else
			++it;
	}

	PlayerObject *attacker = getPlayerSafe(goId);
	if (attacker && !IsInterlocked(goId))
	{
		attacker->setInCombat(false);
		attacker->setCombatStance(false);
	}
}

bool CombatSystem::IsFreeFiring( uint32 goId ) const
{
	foreach(const FreeFireState &state, m_freefires)
	{
		if (state.attackerGoId == goId)
			return true;
	}
	return false;
}

void CombatSystem::EndInterlock( uint32 goId, bool byWithdraw )
{
	for (list<InterlockSession>::iterator it=m_interlocks.begin();it!=m_interlocks.end();)
	{
		if (it->goIdA == goId || it->goIdB == goId)
		{
			PlayerObject *pA = getPlayerSafe(it->goIdA);
			PlayerObject *pB = getPlayerSafe(it->goIdB);
			if (pA)
			{
				if (it->ilViewIdA)
				{
					pA->getClient().QueueState(shared_ptr<DeleteViewMsg>(new DeleteViewMsg(it->ilViewIdA)));
					sObjMgr.releaseDynamicView(&pA->getClient(),it->ilViewIdA);
				}
				pA->leaveInterlock();
				if (byWithdraw)
					pA->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FFFF00}Interlock has ended.{/c}")));
			}
			if (pB)
			{
				if (it->ilViewIdB)
				{
					pB->getClient().QueueState(shared_ptr<DeleteViewMsg>(new DeleteViewMsg(it->ilViewIdB)));
					sObjMgr.releaseDynamicView(&pB->getClient(),it->ilViewIdB);
				}
				pB->leaveInterlock();
				if (byWithdraw)
					pB->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FFFF00}Interlock has ended.{/c}")));
			}
			it = m_interlocks.erase(it);
		}
		else
			++it;
	}
}

bool CombatSystem::IsInterlocked( uint32 goId ) const
{
	foreach(const InterlockSession &session, m_interlocks)
	{
		if (session.goIdA == goId || session.goIdB == goId)
			return true;
	}
	return false;
}

InterlockSession* CombatSystem::GetInterlockSession( uint32 goId )
{
	foreach(InterlockSession &session, m_interlocks)
	{
		if (session.goIdA == goId || session.goIdB == goId)
			return &session;
	}
	return NULL;
}

void CombatSystem::SetTactic( uint32 goId, uint8 tactic )
{
	InterlockSession *session = GetInterlockSession(goId);
	if (session == NULL)
		return;

	if (session->goIdA == goId)
		session->tacticA = tactic;
	else
		session->tacticB = tactic;
}

void CombatSystem::QueueAbility( uint32 goId, uint16 moveId )
{
	InterlockSession *session = GetInterlockSession(goId);
	if (session == NULL)
		return;

	const CombatMove *move = GetMove(moveId);
	if (move == NULL)
	{
		// If it's a data-driven ability that isn't mapped to a specific hardcoded combat move,
		// allow it to queue anyway (it will resolve as a generic attack carrying the FX).
		const AbilityTemplate *dataTemplate = sDataLoader.GetAbilityTemplate(moveId);
		if (dataTemplate == NULL) return;
	}
	else if (move->freefireOnly)
	{
		return;
	}

	if (session->goIdA == goId)
		session->queuedMoveA = moveId;
	else
		session->queuedMoveB = moveId;
}

void CombatSystem::RemoveCombatant( uint32 goId )
{
	StopFreeFire(goId);
	EndInterlock(goId,false);

	//also remove free-fire engagements targeting this combatant
	for (list<FreeFireState>::iterator it=m_freefires.begin();it!=m_freefires.end();)
	{
		if (it->targetGoId == goId)
		{
			PlayerObject *attacker = getPlayerSafe(it->attackerGoId);
			if (attacker)
			{
				attacker->setInCombat(false);
				attacker->setCombatStance(false);
				attacker->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("Your target is gone.")));
			}
			it = m_freefires.erase(it);
		}
		else
			++it;
	}
}

// Per-tactic vulnerability modifier, from the client.dll combat resolution
// structure (DefenseVulnerability/PowerVulnerability/SpeedVulnerability).
// Until per-character vulnerability stats exist we derive them from the
// classic tactic triangle: Power beats Grab, Grab beats Speed, Speed beats Power.
float CombatSystem::TacticModifier( uint8 attackerTactic, uint8 targetTactic )
{
	float vulnerability = 0.0f;
	if (attackerTactic == TACTIC_POWER && targetTactic == TACTIC_RETALIATE)
		vulnerability = 0.30f;
	else if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_SPEED)
		vulnerability = 0.30f;
	else if (attackerTactic == TACTIC_SPEED && targetTactic == TACTIC_POWER)
		vulnerability = 0.30f;
	else if (attackerTactic == targetTactic)
		vulnerability = -0.10f; //mirrored tactics glance off

	return 1.0f + vulnerability;
}

CombatSystem::AttackResult CombatSystem::ResolveAttack( PlayerObject* attacker, PlayerObject* target,
	const CombatMove& move, uint8 attackerTactic, uint8 targetTactic )
{
	AttackResult result;
	result.hit = false;
	result.damageTaken = 0;

	//attack roll vs defense roll, accuracy/defense scale with level
	int attackRoll = (rand() % 100) + int(attacker->getLevel()) * 2;
	int defenseRoll = (rand() % 100) + int(target->getLevel()) * 2;

	//block tactic makes the defender much harder to hit cleanly
	if (targetTactic == TACTIC_DEFENSE)
		defenseRoll += 25;

	result.hit = (attackRoll >= defenseRoll);
	if (!result.hit)
		return result;

	//rawDamage = random(scaledMin, scaledMax)
	float scaledMin = move.minDmg + move.minDmgPerLvl * float(attacker->getLevel());
	float scaledMax = move.maxDmg + move.maxDmgPerLvl * float(attacker->getLevel());
	float rawDamage = scaledMin + (float(rand())/float(RAND_MAX)) * (scaledMax - scaledMin);

	//Modifier = tacticModifier * (1 + DamageModifier)
	float modifier = TacticModifier(attackerTactic,targetTactic);

	//DamageAbsorbed = toughness, derived from target level for now
	float damageAbsorbed = float(target->getLevel()) * 0.5f;

	//blocking halves what gets through
	if (targetTactic == TACTIC_DEFENSE)
		modifier *= 0.5f;

	float damageTaken = rawDamage * modifier - damageAbsorbed;
	if (damageTaken < 0.0f)
		damageTaken = 0.0f;

	result.damageTaken = uint16(damageTaken);
	return result;
}

void CombatSystem::AwardKill( PlayerObject* killer, PlayerObject* victim )
{
	if (killer == NULL || victim == NULL)
		return;

	//experience reward scales with the victim's level
	uint32 expGained = uint32(victim->getLevel()) * 150 + (rand() % 100);
	killer->awardCombatExperience(expGained);

	//mission kill objectives
	sMissionSys.AdvanceObjective(killer,ObjectiveCommand::DEFEAT,victim->getGoId());

	sBotMgr.LogCombat((format("%1% defeated %2% (+%3% exp)") % killer->getHandle() % victim->getHandle() % expGained).str());
}

bool CombatSystem::ResolveSingleAttack( uint32 attackerGoId, uint32 targetGoId, uint16 moveId, bool inInterlock )
{
	PlayerObject *attacker = getPlayerSafe(attackerGoId);
	PlayerObject *target = getPlayerSafe(targetGoId);
	if (attacker == NULL || target == NULL || attacker->isDead() || target->isDead())
		return false;

	const CombatMove *movePtr = GetMove(moveId);
	const AbilityTemplate *dataTemplate = NULL;
	CombatMove tempMove;

	if (movePtr == NULL)
	{
		dataTemplate = sDataLoader.GetAbilityTemplate(moveId);
		if (dataTemplate == NULL)
			return false; // Actually unknown
		
		tempMove = *(DefaultMelee());
		tempMove.id = moveId;
		tempMove.name = dataTemplate->name.c_str();
		tempMove.hitFxId = dataTemplate->executionFX ? dataTemplate->executionFX : tempMove.hitFxId;
		tempMove.isCost = dataTemplate->innerStrengthCost;
		
		movePtr = &tempMove;
	}
	else
	{
		tempMove = *movePtr;
		movePtr = &tempMove;
	}

	//range gate
	float distance = float(attacker->getPosition().Distance(target->getPosition()));
	if (movePtr->range > 0 && distance > movePtr->range)
	{
		attacker->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}Target is out of range.{/c}")));
		return false;
	}

	//inner strength gate
	if (movePtr->isCost > 0 && !attacker->spendIS(movePtr->isCost))
	{
		attacker->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}Not enough Inner Strength.{/c}")));
		return false;
	}

	AttackResult result = ResolveAttack(attacker,target,*movePtr,
		attacker->getTactic(),target->getTactic());

	if (result.hit && result.damageTaken > 0)
	{
		bool wasAlive = !target->isDead();
		target->takeDamage(attackerGoId,result.damageTaken,movePtr->hitFxId);

		attacker->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:00FF00}Your %1% hits %2% for %3% damage.{/c}") % movePtr->name % target->getHandle() % result.damageTaken).str() )));
		target->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:FF0000}%1%'s %2% hits you for %3% damage.{/c}") % attacker->getHandle() % movePtr->name % result.damageTaken).str() )));

		if (sBotMgr.IsCombatLoggingEnabled())
			sBotMgr.LogCombat((format("%1% hit %2% with %3% for %4%") % attacker->getHandle() % target->getHandle() % movePtr->name % result.damageTaken).str());

		if (wasAlive && target->isDead())
			AwardKill(attacker,target);
	}
	else
	{
		attacker->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:FFFF00}Your %1% misses %2%.{/c}") % movePtr->name % target->getHandle()).str() )));
		target->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:FFFF00}%1%'s %2% misses you.{/c}") % attacker->getHandle() % movePtr->name).str() )));
	}

	//defender in block regains inner strength (documented block behavior)
	if (target->getTactic() == TACTIC_DEFENSE && !target->isDead())
		target->restoreIS(5);

	return true;
}

bool CombatSystem::UseAbility( PlayerObject* caster, uint16 abilityId, uint32 targetGoId )
{
	if (caster == NULL)
		return false;

	PlayerObject *target = getPlayerSafe(targetGoId);

	//real client ability ids resolve through the DataLoader templates for
	//cast time and FX; damage stats come from a matching combat move or a
	//generic level-scaled fallback
	const AbilityTemplate *dataTemplate = sDataLoader.GetAbilityTemplate(abilityId);
	const CombatMove *move = GetMove(abilityId);

	float castTime = 0.0f;
	if (dataTemplate)
		castTime = float(dataTemplate->castTime) / 1000.0f;
	else if (move)
		castTime = move->castTime;

	//cast bar for anything with a cast time
	if (castTime > 0.05f)
		caster->getClient().QueueCommand(shared_ptr<CastBarMsg>(new CastBarMsg(abilityId,castTime)));

	if (move == NULL)
	{
		//unmapped ability: play its FX so the client still animates, apply a
		//generic strike so combat progresses, and log the id for mapping work
		INFO_LOG(format("%1% used data-driven ability %2% (%3%)")
			% caster->getHandle() % abilityId % (dataTemplate ? dataTemplate->name : string("unknown")));

		if (target && !target->isDead() && target != caster)
		{
			const CombatMove *generic = CombatSystem::DefaultMelee();
			uint32 fx = (dataTemplate && dataTemplate->executionFX) ? dataTemplate->executionFX : generic->hitFxId;

			//resolve as a generic hit carrying the ability's own FX
			CombatMove tempMove = *generic;
			tempMove.hitFxId = fx;
			tempMove.range = 6000.0f;

			AttackResult result = ResolveAttack(caster,target,tempMove,caster->getTactic(),target->getTactic());
			if (result.hit && result.damageTaken > 0)
			{
				bool wasAlive = !target->isDead();
				target->takeDamage(caster->getGoId(),result.damageTaken,fx);
				if (wasAlive && target->isDead())
					AwardKill(caster,target);
			}
		}
		return true;
	}

	if (IsInterlocked(caster->getGoId()))
	{
		QueueAbility(caster->getGoId(),move->id);
		caster->getClient().QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("%1% queued for the next interlock round.") % move->name).str() )));
	}
	else if (targetGoId != 0)
	{
		ResolveSingleAttack(caster->getGoId(),targetGoId,move->id,false);
		if (move->freefireOnly)
			RequestRangedCombat(caster->getGoId(),targetGoId,move->id);
	}
	return true;
}

void CombatSystem::RunInterlockRound( InterlockSession &session )
{
	PlayerObject *pA = getPlayerSafe(session.goIdA);
	PlayerObject *pB = getPlayerSafe(session.goIdB);
	if (pA == NULL || pB == NULL)
		return;

	pA->setTactic(session.tacticA);
	pB->setTactic(session.tacticB);

	//round resolution: A attacks B and B attacks A. When a special ability is
	//queued there is no hit-hit round - the special preempts the exchange.
	uint16 moveA = session.queuedMoveA ? session.queuedMoveA : DefaultMelee()->id;
	uint16 moveB = session.queuedMoveB ? session.queuedMoveB : DefaultMelee()->id;
	bool specialFromA = (session.queuedMoveA != 0);

	ResolveSingleAttack(session.goIdA,session.goIdB,moveA,true);
	if (!specialFromA && !pB->isDead())
		ResolveSingleAttack(session.goIdB,session.goIdA,moveB,true);

	session.queuedMoveA = 0;
	session.queuedMoveB = 0;

	//deaths end the session on the next Update() pass
}

void CombatSystem::RunFreeFireShot( FreeFireState &state )
{
	PlayerObject *attacker = getPlayerSafe(state.attackerGoId);
	PlayerObject *target = getPlayerSafe(state.targetGoId);
	if (attacker == NULL || target == NULL)
		return;

	//drifting out of range pauses the engagement rather than ending it
	const CombatMove *move = GetMove(state.moveId);
	if (move == NULL)
		return;

	float distance = float(attacker->getPosition().Distance(target->getPosition()));
	if (move->range > 0 && distance > move->range)
		return;

	ResolveSingleAttack(state.attackerGoId,state.targetGoId,state.moveId,false);
}

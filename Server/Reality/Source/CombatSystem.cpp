#include "Common.h"
#include "CombatSystem.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "BotManager.h"
#include "MissionSystem.h"
#include "DataLoader.h"
#include "MessageTypes.h"
#include "GOAttributes.h"
#include "GameClient.h"
#include "Timer.h"
#include "SpatialGrid.h"
#include "BotClient.h"
#include "AI/MatrixThreatHeatmap.h"
#include "WorldDirector.h"
#include "LogisticsManager.h"
#include "StatusEffectManager.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

const float CombatSystem::INTERLOCK_ROUND_SECONDS = 4.0f; //authentic MxO interlock round
const float CombatSystem::FREEFIRE_SHOT_SECONDS = 2.0f;

createFileSingleton(CombatSystem);

// sObjMgr.getGOPtr throws on missing objects - combat wants a null instead so
// a mid-fight disconnect can never unwind the server loop
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

CombatSystem::CombatSystem() {}
CombatSystem::~CombatSystem() {}

void CombatSystem::Init()
{
	LoadAbilities();
}

void CombatSystem::LoadAbilities()
{
	m_moveTable.clear();
	boost::property_tree::ptree pt;
	try {
		boost::property_tree::read_json("Data/abilities.json", pt);
		for (auto& item : pt.get_child("abilities")) {
			CombatMove move;
			move.id = item.second.get<uint16>("id");
			move.name = item.second.get<std::string>("name");
			move.dmgType = (mxoDamageType)item.second.get<int>("dmgType");
			move.minDmg = item.second.get<float>("minDmg");
			move.maxDmg = item.second.get<float>("maxDmg");
			move.minDmgPerLvl = item.second.get<float>("minDmgPerLvl");
			move.maxDmgPerLvl = item.second.get<float>("maxDmgPerLvl");
			move.isCost = item.second.get<uint16>("isCost");
			// abilities.json authors range in METERS; the world uses centi-units
			// (100 units = 1m), so convert or every attack is ~100x out of range.
			move.range = item.second.get<float>("range") * 100.0f;
			move.hitFxId = item.second.get<uint32>("hitFxId");
			move.interlockOnly = item.second.get<bool>("interlockOnly");
			move.freefireOnly = item.second.get<bool>("freefireOnly");
			move.castTime = item.second.get<float>("castTime");
			move.specialFlags = item.second.get<uint32>("specialFlags", 0);
			m_moveTable[move.id] = move;
		}
	} catch (...) {
        CombatMove defaultMelee;
        defaultMelee.id = 1;
        defaultMelee.name = "Melee Attack";
        defaultMelee.dmgType = DAMAGE_MELEE;
        defaultMelee.minDmg = 10.0f;
        defaultMelee.maxDmg = 15.0f;
        defaultMelee.minDmgPerLvl = 1.0f;
        defaultMelee.maxDmgPerLvl = 1.5f;
        defaultMelee.isCost = 0;
        defaultMelee.range = 300.0f; //world units: 3m
        defaultMelee.hitFxId = 1234;
        defaultMelee.interlockOnly = true;
        defaultMelee.freefireOnly = false;
        defaultMelee.castTime = 0.0f;
        defaultMelee.specialFlags = 0;
        m_moveTable[defaultMelee.id] = defaultMelee;
    }
}

const CombatMove* CombatSystem::GetMove(uint16 moveId)
{
    std::lock_guard<std::recursive_mutex> lock(sCombatSys.m_combatMutex);
    auto it = sCombatSys.m_moveTable.find(moveId);
    if (it != sCombatSys.m_moveTable.end()) return &it->second;
    return nullptr;
}

const CombatMove* CombatSystem::GetMoveByName(const std::string &name)
{
    std::lock_guard<std::recursive_mutex> lock(sCombatSys.m_combatMutex);
    for (auto& pair : sCombatSys.m_moveTable) {
        if (pair.second.name == name) return &pair.second;
    }
    return nullptr;
}

const CombatMove* CombatSystem::DefaultMelee()
{
    return GetMove(1);
}

const CombatMove* CombatSystem::DefaultRanged()
{
    return GetMove(2);
}

void CombatSystem::Update()
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	uint32 currTime = getMSTime();

	for (auto it = m_interlocks.begin(); it != m_interlocks.end(); ) {
		PlayerObject* pA = getPlayerSafe(it->goIdA);
		PlayerObject* pB = getPlayerSafe(it->goIdB);

		//reap sessions whose participants vanished or died
		bool keepAlive = (pA && pB && !pA->isDead() && !pB->isDead());

		if (keepAlive && currTime >= it->nextRoundTime) {
			keepAlive = RunInterlockRound(*it);

			// [Item 13] Bullet Time / Dilation Zones
			float avgDilation = (pA->GetTimeDilation() + pB->GetTimeDilation()) / 2.0f;
			if (avgDilation <= 0.1f) avgDilation = 0.1f; // Prevent infinite division

			it->nextRoundTime = currTime + uint32((INTERLOCK_ROUND_SECONDS * 1000.0f) / avgDilation);
			it->roundNumber++;
		}

		if (!keepAlive) {
			//tear the interlock UI down on both clients
			if (pA && it->ilViewIdA) {
				pA->getClient().QueueState(std::make_shared<DeleteViewMsg>(it->ilViewIdA));
				sObjMgr.releaseDynamicView(&pA->getClient(), it->ilViewIdA);
			}
			if (pB && it->ilViewIdB) {
				pB->getClient().QueueState(std::make_shared<DeleteViewMsg>(it->ilViewIdB));
				sObjMgr.releaseDynamicView(&pB->getClient(), it->ilViewIdB);
			}
			if (pA) pA->leaveInterlock();
			if (pB) pB->leaveInterlock();
			it = m_interlocks.erase(it);
			continue;
		}
		++it;
	}

	for (auto it = m_freefires.begin(); it != m_freefires.end(); ) {
		bool keepAlive = true;
		if (currTime >= it->nextShotTime) {
			keepAlive = RunFreeFireShot(*it);
            
            PlayerObject* pA = getPlayerSafe(it->attackerGoId);
            float dilation = pA ? pA->GetTimeDilation() : 1.0f;
            if (dilation <= 0.1f) dilation = 0.1f;
			it->nextShotTime = currTime + uint32((FREEFIRE_SHOT_SECONDS * 1000.0f) / dilation);
		}
		if (!keepAlive) {
			PlayerObject* pA = getPlayerSafe(it->attackerGoId);
			if (pA) pA->setCombatStance(false);
			it = m_freefires.erase(it);
			continue;
		}
		++it;
	}

	// Phase 3: Matrix Threat Heatmap Diffusion & Escalation Tick
	static uint32 lastHeatmapTickMs = 0;
	if (currTime - lastHeatmapTickMs >= 1000) {
		float dtSeconds = (lastHeatmapTickMs == 0) ? 1.0f : (float)(currTime - lastHeatmapTickMs) / 1000.0f;
		sMatrixThreatHeatmap.Update(dtSeconds, currTime);
		lastHeatmapTickMs = currTime;
	}
}

bool CombatSystem::IsInterlocked(uint32 goId) const
{
	std::lock_guard<std::recursive_mutex> lock(const_cast<CombatSystem*>(this)->m_combatMutex);
	for (const auto& session : m_interlocks) {
		if (session.goIdA == goId || session.goIdB == goId) return true;
	}
	return false;
}

bool CombatSystem::IsFreeFiring(uint32 goId) const
{
	std::lock_guard<std::recursive_mutex> lock(const_cast<CombatSystem*>(this)->m_combatMutex);
	for (const auto& state : m_freefires) {
		if (state.attackerGoId == goId) return true;
	}
	return false;
}

InterlockSession* CombatSystem::GetInterlockSession(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	for (auto& session : m_interlocks) {
		if (session.goIdA == goId || session.goIdB == goId) return &session;
	}
	return nullptr;
}

bool CombatSystem::GetInterlockSessionCopy(uint32 goId, InterlockSession& outSession) const
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	for (const auto& session : m_interlocks) {
		if (session.goIdA == goId || session.goIdB == goId) {
			outSession = session;
			return true;
		}
	}
	return false;
}

bool CombatSystem::RequestInterlock(uint32 attackerGoId, uint32 targetGoId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	if (IsInterlocked(attackerGoId) || IsInterlocked(targetGoId)) return false;

	PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead()) return false;
	if (pA->getPosition().Distance(pB->getPosition()) > 1500.0f) return false;

    // Bystander Panic (15-20m radius = 1500-2000 units, SpatialGrid accelerated)
    const float panicRadius = 2000.0f;
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(pA->getPosition().x, pA->getPosition().z, panicRadius);
    for (auto* gc : nearbyClients) {
        if (!gc) continue;
        PlayerObject* p = getPlayerSafe(gc->GetPlayerGoId());
        if (p && gc->isBot() && p->getPosition().DistanceSq(pA->getPosition()) <= (panicRadius * panicRadius) && (p->getFactionName() == "Civilian" || p->getHandle().find("Civilian") != std::string::npos)) {
            BotClient* bot = dynamic_cast<BotClient*>(gc);
            if (bot) {
                bot->triggerPanic(attackerGoId);
            }
        }
    }

	InterlockSession session;
	session.goIdA = attackerGoId;
	session.goIdB = targetGoId;
	session.tacticA = TACTIC_NORMAL;
	session.tacticB = TACTIC_NORMAL;
	session.queuedMoveA = 0;
	session.queuedMoveB = 0;
	session.nextRoundTime = getMSTime() + uint32(INTERLOCK_ROUND_SECONDS * 1000.0f);
	session.roundNumber = 0;
	session.ilViewIdA = 0;
	session.ilViewIdB = 0;

	//spawn the ILCombatHandler view + pairing packet on both clients - this
	//drives the client-side interlock camera and round UI
	{
		float simTime = sGame.GetSimTime();
		LocationVector ilPos = pA->getPosition();

		struct SideSetup { PlayerObject* self; PlayerObject* other; uint16* viewSlot; };
		SideSetup sides[2] =
		{
			{ pA, pB, &session.ilViewIdA },
			{ pB, pA, &session.ilViewIdB },
		};

		for (int i = 0; i < 2; i++)
		{
			try
			{
				uint16 ilViewId = sObjMgr.allocateDynamicView(&sides[i].self->getClient(), uint32(GOID_ILCOMBATHANDLER) << 16);
				*(sides[i].viewSlot) = ilViewId;

				sides[i].self->getClient().QueueState(std::make_shared<SpawnILCombatHandlerMsg>(
					ilViewId, uint8(0x41 + i), ilPos, simTime));

				//the pairing references the opponent's view as this client sees it
				uint16 otherViewId = sObjMgr.getViewForGO(&sides[i].self->getClient(), sides[i].other->getGoId());
				uint32 otherViewWithSpawnId = uint32(otherViewId) | (uint32(2) << 16);

				sides[i].self->getClient().QueueState(std::make_shared<InterlockInitMsg>(
					ilViewId, ilPos, otherViewWithSpawnId, uint16(2)));
			}
			catch (ObjectMgr::NoMoreFreeViews) { WARNING_LOG("No free views for interlock handler spawn"); }
			catch (ObjectMgr::ObjectNotAvailable) {}
			catch (ObjectMgr::ClientNotAvailable) {}
		}
	}

	m_interlocks.push_back(session);

	// Turn combatants to face each other squarely upon interlock initiation
	LocationVector posA = pA->getPosition();
	LocationVector posB = pB->getPosition();
	posA.rot = posA.CalcAngTo(posB);
	pA->setPosition(posA);
	sGame.AnnounceStateUpdate(NULL, std::make_shared<PositionStateMsg>(pA->getGoId()));

	posB.rot = posB.CalcAngTo(posA);
	pB->setPosition(posB);
	sGame.AnnounceStateUpdate(NULL, std::make_shared<PositionStateMsg>(pB->getGoId()));

	pA->enterInterlock(targetGoId);
	pB->enterInterlock(attackerGoId);
	return true;
}

bool CombatSystem::RequestRangedCombat(uint32 attackerGoId, uint32 targetGoId, uint16 moveId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	if (IsFreeFiring(attackerGoId)) return false;

	PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead()) return false;

    // Bystander Panic (15-20m radius = 1500-2000 units, SpatialGrid accelerated)
    const float panicRadius = 2000.0f;
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(pA->getPosition().x, pA->getPosition().z, panicRadius);
    for (auto* gc : nearbyClients) {
        if (!gc) continue;
        PlayerObject* p = getPlayerSafe(gc->GetPlayerGoId());
        if (p && gc->isBot() && p->getPosition().DistanceSq(pA->getPosition()) <= (panicRadius * panicRadius) && (p->getFactionName() == "Civilian" || p->getHandle().find("Civilian") != std::string::npos)) {
            BotClient* bot = dynamic_cast<BotClient*>(gc);
            if (bot) {
                bot->triggerPanic(attackerGoId);
            }
        }
    }

	FreeFireState state;
	state.attackerGoId = attackerGoId;
	state.targetGoId = targetGoId;
	state.moveId = moveId;
	state.nextShotTime = getMSTime() + uint32(FREEFIRE_SHOT_SECONDS * 1000.0f);

	m_freefires.push_back(state);
	pA->setCombatStance(true);
	return true;
}

void CombatSystem::TriggerForesightPremonition(PlayerObject* player, PlayerObject* opponent, uint8 enemyTactic)
{
	if (!player || player->getClient().isBot() || !opponent) return;
	if (sStatusEffectManager.HasEffect(player->getGoId(), EFFECT_ORACLE_INTUITION) ||
		sStatusEffectManager.HasEffect(player->getCharId(), EFFECT_ORACLE_INTUITION) ||
		sStatusEffectManager.HasEffect(player->getGoId(), EFFECT_ORACLE_PREMONITION_BOOST) ||
		sStatusEffectManager.HasEffect(player->getCharId(), EFFECT_ORACLE_PREMONITION_BOOST)) {
		float pPerception = static_cast<float>(player->getPerception());
		float pIS = static_cast<float>(player->getInnerStrength());
		float maxIS = static_cast<float>(player->getMaximumInnerStrength());
		float chance = TacticAdapter::CalculatePremonitionChance(pPerception, pIS, maxIS);
		if (sStatusEffectManager.HasEffect(player->getGoId(), EFFECT_ORACLE_PREMONITION_BOOST) ||
			sStatusEffectManager.HasEffect(player->getCharId(), EFFECT_ORACLE_PREMONITION_BOOST)) {
			chance = 0.95f; // Premonition Snickerdoodle boost
		}
		if (((rand() % 100) / 100.0f) <= chance) {
			std::string enemyTacticName = "Unknown";
			std::string counterTactic = "Power";
			switch (enemyTactic) {
				case TACTIC_POWER:     enemyTacticName = "POWER"; counterTactic = "SPEED"; break;
				case TACTIC_SPEED:     enemyTacticName = "SPEED"; counterTactic = "GRAB";  break;
				case TACTIC_RETALIATE: enemyTacticName = "GRAB";  counterTactic = "POWER"; break;
				case TACTIC_DEFENSE:   enemyTacticName = "BLOCK"; counterTactic = "GRAB";  break;
				default:               enemyTacticName = "NORMAL"; counterTactic = "POWER"; break;
			}
			player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				(format("{c:FFB300}[ORACLE FORESIGHT] Premonition: %1% prepares %2%! Recommended counter: %3%{/c}")
				 % opponent->getHandle() % enemyTacticName % counterTactic).str()
			));
		}
	}
}

void CombatSystem::SetTactic(uint32 goId, uint8 tactic)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	InterlockSession* session = GetInterlockSession(goId);
	if (session) {
		if (session->goIdA == goId) {
			session->tacticA = tactic;
			PlayerObject* pA = sObjMgr.getGOPtrSafe(session->goIdA);
			PlayerObject* pB = sObjMgr.getGOPtrSafe(session->goIdB);
			if (pA && pB) TriggerForesightPremonition(pB, pA, tactic);
		} else {
			session->tacticB = tactic;
			PlayerObject* pA = sObjMgr.getGOPtrSafe(session->goIdA);
			PlayerObject* pB = sObjMgr.getGOPtrSafe(session->goIdB);
			if (pA && pB) TriggerForesightPremonition(pA, pB, tactic);
		}
	}
}

void CombatSystem::QueueAbility(uint32 goId, uint16 moveId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	InterlockSession* session = GetInterlockSession(goId);
	if (session) {
		if (session->goIdA == goId) session->queuedMoveA = moveId;
		else session->queuedMoveB = moveId;
	}
}

void CombatSystem::StopFreeFire(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	for (auto it = m_freefires.begin(); it != m_freefires.end(); ) {
		if (it->attackerGoId == goId) {
			PlayerObject* pA = getPlayerSafe(it->attackerGoId);
			if (pA) pA->setCombatStance(false);
			it = m_freefires.erase(it);
		} else {
			++it;
		}
	}
}

void CombatSystem::EndInterlock(uint32 goId, bool byWithdraw)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	for (auto it = m_interlocks.begin(); it != m_interlocks.end(); ) {
		if (it->goIdA == goId || it->goIdB == goId) {
			PlayerObject* pA = getPlayerSafe(it->goIdA);
			PlayerObject* pB = getPlayerSafe(it->goIdB);
			
            if (byWithdraw && pA && pB && !pA->isDead() && !pB->isDead()) {
                // Parting Shot
                uint32 attackerId = (it->goIdA == goId) ? it->goIdB : it->goIdA;
                PlayerObject* pAttacker = getPlayerSafe(attackerId);
                PlayerObject* pTarget = getPlayerSafe(goId);
                const CombatMove* move = DefaultMelee();
                if (pAttacker && pTarget && move) {
                    float dmg = move->minDmg + (move->maxDmg - move->minDmg) * ((rand() % 100) / 100.0f);
                    dmg += move->minDmgPerLvl * pAttacker->getLevel();
                    dmg *= 1.5f; // Parting shot does 50% extra damage
                    
                    if (pTarget->getCurrentHealth() <= dmg) {
                        sGame.AnnounceStateUpdateNear(pAttacker->getPosition().x, pAttacker->getPosition().z, 20000.0f, std::make_shared<EmoteMsg>(pAttacker->getGoId(), 43, 1));
                    }
                    pTarget->takeDamage(attackerId, (uint16)dmg, move->hitFxId);
                    
                    if (!pTarget->getClient().isBot()) {
                        pTarget->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[COMBAT] Withdraw Penalty! You suffer a parting shot!{/c}"));
                    }
                }
            }

			if (pA && it->ilViewIdA) {
				pA->getClient().QueueState(std::make_shared<DeleteViewMsg>(it->ilViewIdA));
				sObjMgr.releaseDynamicView(&pA->getClient(), it->ilViewIdA);
			}
			if (pB && it->ilViewIdB) {
				pB->getClient().QueueState(std::make_shared<DeleteViewMsg>(it->ilViewIdB));
				sObjMgr.releaseDynamicView(&pB->getClient(), it->ilViewIdB);
			}
			if (pA) pA->leaveInterlock();
			if (pB) pB->leaveInterlock();
			it = m_interlocks.erase(it);
		} else {
			++it;
		}
	}
}

void CombatSystem::RemoveCombatant(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	StopFreeFire(goId);
	EndInterlock(goId, false);
}

float CombatSystem::TacticModifier(uint8 attackerTactic, uint8 targetTactic)
{
	// Authentic Matrix Online martial arts counter matrix:
	// Power beats Grab (1.30x)
	// Grab beats Defense/Block (1.40x throw/block break)
	// Grab beats Speed (1.25x intercept)
	// Speed beats Power (1.30x fast interrupt)
	// Defense absorbs Power/Speed (handled in ResolveAttack)
	if (attackerTactic == TACTIC_POWER && targetTactic == TACTIC_RETALIATE) return 1.30f;
	if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_DEFENSE) return 1.40f;
	if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_SPEED) return 1.25f;
	if (attackerTactic == TACTIC_SPEED && targetTactic == TACTIC_POWER) return 1.30f;
	if (attackerTactic == targetTactic && attackerTactic != TACTIC_NORMAL) return 0.90f; // Mirrored tactics glance off
	return 1.0f;
}

CombatSystem::AttackResult CombatSystem::ResolveAttack(PlayerObject* attacker, PlayerObject* target, const CombatMove& move, uint8 attackerTactic, uint8 targetTactic)
{
	AttackResult res;
	res.hit = true;
	res.isCrit = false;
	res.isBlocked = false;
	res.isGlancing = false;

    // Item 33: Hacker System Integration (Stun)
    if (attacker->isStunned()) {
        res.hit = false;
        if (!attacker->getClient().isBot()) {
            attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[SYSTEM] You are Stunned. Attack aborted.{/c}"));
        }
        return res;
    }

    // Phase 3 Hacker Domain Abilities
    if (move.specialFlags & ABILITY_FLAG_MASK) {
        // attacker->setMaskFaction(target->getFaction(), 300000); // 5 minutes (STUBBED: Faction masking not implemented)
        if (!attacker->getClient().isBot()) {
            attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[HACK] Simulacra Mask Active. Spoofing target faction.{/c}"));
        }
    }
    else if (move.specialFlags & ABILITY_FLAG_TRACE) {
        if (!attacker->getClient().isBot()) {
            std::string msg = (format("{c:00FF00}[TRACE SUCCESS] %1% is at X: %2%, Y: %3%, Z: %4%{/c}") % target->getHandle() % target->getPosition().x % target->getPosition().y % target->getPosition().z).str();
            attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(msg));
        }
    }
    else if (move.specialFlags & ABILITY_FLAG_STUN) {
        target->applyStun(5000); // 5 seconds
        if (!attacker->getClient().isBot()) {
            attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[HACK] Target Stunned for 5 seconds.{/c}"));
        }
    }
    else if (move.specialFlags & ABILITY_FLAG_BOMB) {
        const float bombRadius = 500.0f;
        auto nearbyClients = sSpatialGrid.GetClientsInRadius(target->getPosition().x, target->getPosition().z, bombRadius);
        for (auto* gc : nearbyClients) {
            if (!gc) continue;
            PlayerObject* p = getPlayerSafe(gc->GetPlayerGoId());
            if (p && p != target && p->getFaction() == target->getFaction()) {
                if (p->getPosition().DistanceSq(target->getPosition()) < 250000.0f) { // 500 range
                    p->takeDamage(attacker->getGoId(), move.minDmg, 201);
                }
            }
        }
    }

    if (target->getClient().isBot() && target->getHandle().find("Agent") != std::string::npos && (rand() % 100 < 30)) {
        res.hit = false;
        sMatrixThreatHeatmap.RecordDisruption(target->getPosition().x, target->getPosition().z, 15.0f, "Agent Wire-Fu Dodge");
        return res; // Agent Dodge
    }
    
    // [Item 5] Adaptive Combat Learning
    if (target->getClient().isBot()) {
        uint8 newTactic = TACTIC_NORMAL;
        if (attackerTactic == TACTIC_NORMAL) newTactic = TACTIC_RETALIATE;
        else if (attackerTactic == TACTIC_RETALIATE) newTactic = TACTIC_DEFENSE;
        SetTactic(target->getGoId(), newTactic);
    }

	//authentic hit resolution: attack roll vs defense roll (d100 + level accuracy)
	{
		int attackRoll = (rand() % 100) + int(attacker->getLevel()) * 2;
		int defenseRoll = (rand() % 100) + int(target->getLevel()) * 2;
		if (targetTactic == TACTIC_DEFENSE)
			defenseRoll += 25; //a blocking defender is much harder to hit cleanly
		if (attackRoll < defenseRoll)
		{
			res.hit = false;
			if (!attacker->getClient().isBot()) {
				attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
					(format("{c:FFFF00}[COMBAT] You missed %1%!{/c}") % target->getHandle()).str()
				));
			}
			if (!target->getClient().isBot()) {
				target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
					(format("{c:00FFFF}[COMBAT] You evaded %1%'s attack!{/c}") % attacker->getHandle()).str()
				));
			}
			return res;
		}
	}

	float dmg = move.minDmg + (move.maxDmg - move.minDmg) * ((rand() % 100) / 100.0f);
	dmg += move.minDmgPerLvl * attacker->getLevel();
	dmg *= TacticModifier(attackerTactic, targetTactic);

    // Apply adaptive learning mitigation
    dmg *= target->m_combatMemory.getMitigationModifier(move.id);

    // [Item 19] Dual Wielding Firepower
    if (move.dmgType == DAMAGE_RANGED && attacker->isDualWielding()) {
        dmg *= 1.5f; // 50% more damage for off-hand
    }

    // Item 20: Deflection / Bullet Blocking
    uint16 evasion = target->getEvasion();
    if (targetTactic == TACTIC_DEFENSE && move.dmgType == DAMAGE_RANGED && (rand() % 100 < 15 + (evasion / 5))) {
        res.isBlocked = true; // Deflection
        dmg = 0;
        sMatrixThreatHeatmap.RecordDisruption(target->getPosition().x, target->getPosition().z, 15.0f, "Bullet Deflection");
        
        sGame.AnnounceStateUpdateNear(target->getPosition().x, target->getPosition().z, 20000.0f, std::make_shared<EmoteMsg>(target->getGoId(), 41, 1));
        
        if (!target->getClient().isBot())
            target->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:00FFFF}You deflect the incoming fire!{/c}"));
        if (!attacker->getClient().isBot())
            attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}Your shot is deflected!{/c}"));
    } else {
        if (move.dmgType == DAMAGE_RANGED) {
            sMatrixThreatHeatmap.RecordDisruption(attacker->getPosition().x, attacker->getPosition().z, 8.0f, "Ballistic Fire");
        } else {
            sMatrixThreatHeatmap.RecordDisruption(attacker->getPosition().x, attacker->getPosition().z, 12.0f, "Melee Interlock");
        }
    }

    if (target->getClient().isBot() && target->getHandle().find("Agent") != std::string::npos) {
        dmg *= 0.5f; // Agent Resilience
    }

    // Phase 4: Seraphic Kinetic Deflection (Absorbs 50% damage, deflects 20% kinetic force back to attacker)
    if (targetTactic == TACTIC_DEFENSE && (sStatusEffectManager.HasEffect(target->getGoId(), EFFECT_ORACLE_INTUITION) ||
                                           sStatusEffectManager.HasEffect(target->getCharId(), EFFECT_ORACLE_INTUITION) ||
                                           sStatusEffectManager.HasEffect(target->getGoId(), EFFECT_ORACLE_SERAPHIC_AEGIS) ||
                                           sStatusEffectManager.HasEffect(target->getCharId(), EFFECT_ORACLE_SERAPHIC_AEGIS))) {
        bool hasAegis = sStatusEffectManager.HasEffect(target->getGoId(), EFFECT_ORACLE_SERAPHIC_AEGIS) ||
                        sStatusEffectManager.HasEffect(target->getCharId(), EFFECT_ORACLE_SERAPHIC_AEGIS);
        float absorbRatio = hasAegis ? 0.40f : 0.50f; // 60% absorbed with Aegis, 50% absorbed with base Intuition
        float reflectRatio = hasAegis ? 0.35f : 0.20f; // 35% reflected with Aegis, 20% reflected with base Intuition
        float deflectedDmg = dmg * reflectRatio;
        dmg *= absorbRatio; // Absorbed
        if (deflectedDmg > 0.0f && !attacker->isDead()) {
            attacker->takeDamage(target->getGoId(), static_cast<uint16>(deflectedDmg), 0x280001C1);
            if (!target->getClient().isBot()) {
                target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    (format("{c:FFB300}[Seraphic Deflection] You absorbed %1%%% damage and deflected %2% kinetic force back to %3%!{/c}")
                     % static_cast<uint16>((1.0f - absorbRatio) * 100.0f) % static_cast<uint16>(deflectedDmg) % attacker->getHandle()).str()
                ));
            }
        }
    }

    // Civilian Panic: bystanders flee in terror from active combat
    if (res.hit && !res.isBlocked) {
        auto nearbyClients = sSpatialGrid.GetClientsInRadius(attacker->getPosition().x, attacker->getPosition().z, 2500.0f);
        for (GameClient* gc : nearbyClients) {
            if (gc && gc->isBot()) {
                BotClient* bc = dynamic_cast<BotClient*>(gc);
                if (bc && !bc->IsPanicking()) {
                    PlayerObject* botPo = BotGetPlayer(bc->GetPlayerGoId());
                    if (botPo && (botPo->getFactionName() == "Civilian" || botPo->getHandle().find("Civilian") != std::string::npos)) {
                        float distSq = attacker->getPosition().DistanceSq(botPo->getPosition().x, botPo->getPosition().y, botPo->getPosition().z);
                        if (distSq < 2500.0f * 2500.0f) { // 25m radius
                            bc->triggerPanic(attacker->getGoId());
                        }
                    }
                }
            }
        }
    }

	//toughness absorbs part of the blow; blocking halves what gets through
	//and recovers Inner Strength (documented block behavior)
	{
		float absorbed = float(target->getLevel()) * 0.5f;
		if (targetTactic == TACTIC_DEFENSE)
		{
			if (attackerTactic == TACTIC_RETALIATE)
			{
				if (!target->getClient().isBot())
					target->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[COMBAT] Your Block was broken by a Grab!{/c}"));
			}
			else
			{
				dmg *= 0.5f;
				target->restoreIS(5);
			}
		}
		dmg = (dmg > absorbed) ? (dmg - absorbed) : 0.0f;
	}

	res.damageTaken = (uint16)dmg;
    
    // [Item 15] Melee Weapon Durability
    if (res.hit && move.dmgType == DAMAGE_MELEE) {
        attacker->degradeEquippedWeapon(1);
    }
    
    // Item 32: Takedown Moves
    if (res.hit && target->getCurrentHealth() <= res.damageTaken) {
        // Play cinematic takedown emote for attacker based on weapon type
        uint32 emoteId = (move.dmgType == DAMAGE_MELEE) ? 43 : 42;
        sGame.AnnounceStateUpdateNear(attacker->getPosition().x, attacker->getPosition().z, 20000.0f, std::make_shared<EmoteMsg>(attacker->getGoId(), emoteId, 1)); 
        sGame.AnnounceStateUpdateNear(target->getPosition().x, target->getPosition().z, 20000.0f, std::make_shared<EmoteMsg>(target->getGoId(), emoteId, 1)); // Victim plays matched emote
        target->m_deathDelayMS = getMSTime() + 3000; // 3 seconds takedown animation
        target->m_deathDelayKillerId = attacker->getGoId();
    }

	if (res.hit)
	{
		bool wasAlive = !target->isDead();
		target->takeDamage(attacker->getGoId(), res.damageTaken, move.hitFxId);
        target->recordIncomingAttack(move.id);

		if (!attacker->getClient().isBot()) {
			attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				(format("{c:00FF00}[COMBAT] You hit %1% for %2% damage with %3%!{/c}")
				 % target->getHandle() % res.damageTaken % move.name).str()
			));
		}
		if (!target->getClient().isBot()) {
			target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				(format("{c:FF4444}[COMBAT] %1% hits you for %2% damage with %3%! (%4%/%5% HP){/c}")
				 % attacker->getHandle() % res.damageTaken % move.name % target->getCurrentHealth() % target->getMaximumHealth()).str()
			));
		}

		if (wasAlive && target->isDead())
			AwardKill(attacker, target);
	}
	return res;
}

bool CombatSystem::RunInterlockRound(InterlockSession &session)
{
	PlayerObject* pA = getPlayerSafe(session.goIdA);
	PlayerObject* pB = getPlayerSafe(session.goIdB);
	if (!pA || !pB) return false;

	// Interlock broken by movement without explicit withdraw
	float dist = float(pA->getPosition().Distance(pB->getPosition()));
	if (dist > 1500.0f) {
		if (!pA->getClient().isBot()) pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[SYSTEM] Interlock broken due to distance.{/c}"));
		if (!pB->getClient().isBot()) pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[SYSTEM] Interlock broken due to distance.{/c}"));
		return false; //Update() reaps the session and tears down the IL views
	}

	const CombatMove* moveA = session.queuedMoveA ? GetMove(session.queuedMoveA) : DefaultMelee();
	const CombatMove* moveB = session.queuedMoveB ? GetMove(session.queuedMoveB) : DefaultMelee();

	//a queued special preempts the counter-exchange (no hit-hit round)
	bool specialFromA = (session.queuedMoveA != 0);

	if (moveA) ResolveAttack(pA, pB, *moveA, session.tacticA, session.tacticB);
	if (moveB && !specialFromA && !pB->isDead()) ResolveAttack(pB, pA, *moveB, session.tacticB, session.tacticA);

	session.queuedMoveA = 0;
	session.queuedMoveB = 0;

	return !pA->isDead() && !pB->isDead();
}

bool CombatSystem::RunFreeFireShot(FreeFireState &state)
{
	PlayerObject* pA = getPlayerSafe(state.attackerGoId);
	PlayerObject* pB = getPlayerSafe(state.targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead())
		return false; //Update() reaps the engagement (never erase mid-iteration)

	const CombatMove* moveA = state.moveId ? GetMove(state.moveId) : DefaultRanged();
	if (!moveA)
		return false;

	//out of range pauses rather than ends the engagement
	float dist = float(pA->getPosition().Distance(pB->getPosition()));
	if (moveA->range > 0 && dist > moveA->range)
		return true;

	ResolveAttack(pA, pB, *moveA, pA->getTactic(), pB->getTactic());
	return !pB->isDead();
}

bool CombatSystem::ResolveSingleAttack(uint32 attackerGoId, uint32 targetGoId, uint16 moveId, bool inInterlock)
{
    PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
    if (!pA || !pB) return false;
    const CombatMove* move = moveId ? GetMove(moveId) : DefaultRanged();
    if (move) ResolveAttack(pA, pB, *move, TACTIC_NORMAL, TACTIC_NORMAL);
    return true;
}

bool CombatSystem::UseAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
    const CombatMove* move = GetMove(abilityId);
    if (!move) return false;

    //cast bar for anything with a cast time
    if (move->castTime > 0.05f && !caster->getClient().isBot())
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, move->castTime));
    
    // Disruption from special ability execution
    sMatrixThreatHeatmap.RecordDisruption(caster->getPosition().x, caster->getPosition().z, 12.0f, move->name);

    if (move->interlockOnly && !IsInterlocked(caster->getGoId())) return false;
    if (move->freefireOnly && IsInterlocked(caster->getGoId())) return false;
    
    if (IsInterlocked(caster->getGoId())) {
        QueueAbility(caster->getGoId(), abilityId);
        return true;
    } else {
        return RequestRangedCombat(caster->getGoId(), targetGoId, abilityId);
    }
}

void CombatSystem::AwardKill(PlayerObject* killer, PlayerObject* victim)
{
    if (!killer || !victim) return;
    
    // Record elimination on Matrix Threat Heatmap
    sMatrixThreatHeatmap.RecordDisruption(victim->getPosition().x, victim->getPosition().z, 
                                         killer->getClient().isBot() ? 30.0f : 50.0f, "Combat Elimination");

    // Original experience logic
    uint32 exp = victim->getLevel() * 100;
    killer->awardCombatExperience(exp);

    // Economy: $Info drop logic
    // Base amount based on victim level
    uint32 baseInfo = victim->getLevel() * 10;
    
    // Faction multipliers
    float multiplier = 1.0f;
    std::string victimHandle = victim->getHandle();
    if (victimHandle.find("Machine") != std::string::npos || victimHandle.find("Agent") != std::string::npos) {
        multiplier = 1.5f; // Machine +50%
    } else if (victimHandle.find("Merovingian") != std::string::npos || victimHandle.find("Exile") != std::string::npos) {
        multiplier = 1.2f; // Merovingian +20%
    }
    
    uint32 infoAmount = (uint32)(baseInfo * multiplier);
    
    // Living History Notoriety & Reputation Update
    if (victimHandle.find("Agent") != std::string::npos && !killer->getClient().isBot()) {
        sWorldDirector.RecordAgentDefeated(killer->getClient().GetCharacterId(), killer->getHandle());
        sWorldDirector.PropagatePlayerDeedGossip(
            (format("%1% terminated an Agent in combat!") % killer->getHandle()).str(),
            victim->getPosition().x, victim->getPosition().z
        );
    }

    // Logistics courier ambush check
    sLogisticsMgr.OnCourierDestroyed(victim->getGoId(), killer->getGoId());

    killer->addInformation(infoAmount);
    if (!killer->getClient().isBot()) {
        killer->getClient().QueueCommand(std::make_shared<SetInformationCmd>(killer->getInformation()));
        killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:00FF00}[COMBAT] Defeated %1%! Looted %2% $Info.{/c}") % victim->getHandle() % infoAmount).str()
        ));
        killer->saveCashToDB();
    }
}




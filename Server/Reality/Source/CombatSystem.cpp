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
#include "StaticObjectManager.h"
#include "AbilitySystem.h"
#include "CombatAnimationMatrix.h"
#include "HackerSystem.h"
#include "Config.h"
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

CombatSystem::CombatSystem() : m_updatingInterlocks(false) {}
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

	// The default loadout (AbilitySystem::GetDefaultLoadout) uses REAL retail ability ids from
	// abilityIDs.csv, which abilities.json does not know. Until per-ability damage data exists,
	// alias each of them to the basic strike (move id 1) under its real name so a hotbar press
	// resolves to an actual attack instead of being dropped.
	{
		auto baseIt = m_moveTable.find(1);
		if (baseIt != m_moveTable.end()) {
			const CombatMove baseMove = baseIt->second;
			for (const auto& e : AbilitySystem::GetDefaultLoadout()) {
				if (m_moveTable.find(e.abilityId) != m_moveTable.end())
					continue;
				CombatMove alias = baseMove;
				alias.id = e.abilityId;
				alias.name = e.name;
				m_moveTable[alias.id] = alias;
			}
		}

		// Ensure all authentic retail launch martial arts abilities are registered in move table
		struct RetailMoveDef { uint16 id; const char* name; uint16 isCost; float minDmg; float maxDmg; };
		static const RetailMoveDef s_retailMoves[] = {
			{ 17,  "SelfDefenseAbility",            0, 10.0f, 15.0f },
			{ 101, "AikidoAbility",                15, 12.0f, 18.0f },
			{ 132, "KarateAbility",                15, 15.0f, 20.0f },
			{ 133, "KungFuAbility",                15, 14.0f, 19.0f },
			{ 135, "MartialArtsAbility",           15, 14.0f, 18.0f },
			{ 137, "MartialArtsInitiateAbility",   10, 10.0f, 14.0f },
			{ 197, "Head Butt",                    10, 15.0f, 22.0f },
			{ 198, "Cheap Shot",                   10, 12.0f, 16.0f },
			{ 296, "AikidoSpinClayPigeonAbility",  25, 25.0f, 35.0f },
			{ 531, "KarateFocusAbility",           25, 20.0f, 30.0f },
			{ 569, "KarateCombatTacticsAbility",   15, 15.0f, 22.0f },
			{ 570, "KungFuCombatTacticsAbility",   15, 15.0f, 22.0f },
			{ 571, "AikidoCombatTacticsAbility",   15, 15.0f, 22.0f },
			{ 572, "AikidoDamageAbility",          20, 20.0f, 28.0f },
			{ 573, "KarateDamageAbility",          20, 22.0f, 30.0f },
			{ 574, "KungFuDamageAbility",          20, 20.0f, 28.0f },
			{ 600, "CloseCombatTrainingAbility",    0, 10.0f, 14.0f },
			{ 8449, "KungFuMasterAbility",         25, 22.0f, 32.0f },
			{ 8450, "KungfuMasteryAbility",        25, 24.0f, 34.0f },
			{ 8455, "KarateMasterAbility",         25, 24.0f, 34.0f },
			{ 8456, "KarateExpertiseAbility",      20, 20.0f, 28.0f },
			{ 8461, "AikidoMasteryAbility",        25, 22.0f, 32.0f },
			{ 8462, "AikidoMasterAbility",         25, 24.0f, 34.0f },
			{ 8597, "AikidoRedirectionAbility",    25, 20.0f, 30.0f },
			{ 8602, "AikidoGrandmasterAbility",    30, 28.0f, 40.0f },
			{ 8618, "KungFuGrandmasterAbility",    30, 28.0f, 40.0f },
			{ 8623, "KungFuPerfectionAbility",     35, 32.0f, 45.0f },
			{ 8725, "KarateGrandmasterAbility",    30, 28.0f, 40.0f },
			// Soldier Moves
			{ 14,  "PowerShotAbility",             15, 25.0f, 35.0f },
			{ 126, "PistolDisarmingShotAbility",   20, 15.0f, 22.0f },
			{ 129, "HandgunsAbility",              10, 12.0f, 18.0f },
			{ 147, "RiflesAbility",                20, 20.0f, 30.0f },
			{ 453, "RifleButtSmashAbility",        15, 18.0f, 24.0f },
			{ 499, "PistolPointBlankAbility",      15, 22.0f, 30.0f },
			{ 501, "PistolWhipAbility",            10, 14.0f, 20.0f },
			{ 505, "SniperShotAbility",            35, 45.0f, 65.0f },
			// Spy Moves
			{ 146, "PoisonKnifeAbility",           20, 18.0f, 26.0f },
			{ 209, "StealthAbility",               25, 0.0f,   0.0f },
			{ 283, "KnifeThrowerAbility",          15, 16.0f, 24.0f },
		};
		for (const auto& rm : s_retailMoves) {
			if (m_moveTable.find(rm.id) == m_moveTable.end()) {
				CombatMove m;
				m.id = rm.id;
				m.name = rm.name;
				m.dmgType = DAMAGE_MELEE;
				m.minDmg = rm.minDmg;
				m.maxDmg = rm.maxDmg;
				m.minDmgPerLvl = 1.0f;
				m.maxDmgPerLvl = 1.5f;
				m.isCost = rm.isCost;
				m.range = 300.0f;
				m.hitFxId = 0x280006DF;
				m.interlockOnly = true;
				m.freefireOnly = false;
				m.castTime = 0.0f;
				m.specialFlags = 0;
				m_moveTable[m.id] = m;
			}
		}

		// Dynamic Move Table Synthesis: Synthesize CombatMove definitions for all abilities in DataLoader
		for (const auto& kv : sDataLoader.GetAllAbilities())
		{
			const AbilityTemplate& templ = kv.second;
			if (m_moveTable.find(templ.abilityId) != m_moveTable.end())
				continue;

			CombatMove m;
			m.id = templ.abilityId;
			m.name = templ.name;
			m.isCost = templ.innerStrengthCost;
			m.castTime = float(templ.castTime) / 1000.0f;
			m.hitFxId = (templ.executionFX != 0) ? templ.executionFX : 0x280006DF;
			m.specialFlags = 0;

			// Base damage calculation from ValueFrom/ValueTo or level scaling
			float baseMin = (templ.valueFrom > 0) ? float(templ.valueFrom) : (12.0f + float(templ.abilityId % 20));
			float baseMax = (templ.valueTo > templ.valueFrom) ? float(templ.valueTo) : (baseMin * 1.4f);
			m.minDmg = baseMin;
			m.maxDmg = baseMax;
			m.minDmgPerLvl = 1.0f + float(templ.abilityId % 5) * 0.2f;
			m.maxDmgPerLvl = 1.5f + float(templ.abilityId % 5) * 0.3f;

			switch (templ.discipline)
			{
				case DisciplineType::MARTIAL_ARTIST:
					m.dmgType = DAMAGE_MELEE;
					m.range = 300.0f; // 3m
					m.interlockOnly = true;
					m.freefireOnly = false;
					break;
				case DisciplineType::GUNNER:
					m.dmgType = DAMAGE_RANGED;
					if (templ.name.find("Sniper") != std::string::npos)
						m.range = 8000.0f; // 80m
					else if (templ.name.find("Rifle") != std::string::npos)
						m.range = 4500.0f; // 45m
					else if (templ.name.find("PointBlank") != std::string::npos || templ.name.find("Whip") != std::string::npos)
						m.range = 400.0f; // 4m close quarters
					else
						m.range = 3500.0f; // 35m standard firearm
					m.interlockOnly = false;
					m.freefireOnly = true;
					break;
				case DisciplineType::HACKER:
					m.dmgType = DAMAGE_VIRAL;
					m.range = 3500.0f; // 35m
					m.interlockOnly = false;
					m.freefireOnly = true;
					break;
				case DisciplineType::CODER:
					m.dmgType = DAMAGE_HACKING;
					m.range = 2500.0f; // 25m
					m.interlockOnly = false;
					m.freefireOnly = true;
					break;
				case DisciplineType::SPY:
					m.dmgType = DAMAGE_MELEE;
					if (templ.name.find("Throw") != std::string::npos || templ.name.find("Shuriken") != std::string::npos)
						m.range = 3000.0f; // 30m thrown
					else
						m.range = 400.0f; // 4m melee blade
					m.interlockOnly = false;
					m.freefireOnly = false;
					break;
				case DisciplineType::OPERATIVE:
				default:
					m.dmgType = DAMAGE_MELEE;
					m.range = 300.0f;
					m.interlockOnly = false;
					m.freefireOnly = false;
					break;
			}
			m_moveTable[m.id] = m;
		}
		for (const auto& pair : m_moveTable) {
			m_movesByName[pair.second.name] = &pair.second;
		}
		INFO_LOG(format("CombatSystem: Loaded and synthesized %1% combat moves from retail definitions.") % m_moveTable.size());
	}
}

const CombatMove* CombatSystem::GetMove(uint16 moveId)
{
    //UseAbility inserts synthesized moves at runtime from other threads - keep the lock
    std::lock_guard<std::recursive_mutex> lock(sCombatSys.m_combatMutex);
    auto it = sCombatSys.m_moveTable.find(moveId);
    if (it != sCombatSys.m_moveTable.end()) return &it->second;
    return nullptr;
}

const CombatMove* CombatSystem::GetMoveByName(const std::string &name)
{
    std::lock_guard<std::recursive_mutex> lock(sCombatSys.m_combatMutex);
    auto it = sCombatSys.m_movesByName.find(name);
    if (it != sCombatSys.m_movesByName.end()) return it->second;
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

const CombatMove* CombatSystem::GetDefaultStyleMove(PlayerObject* player)
{
    if (!player) return DefaultMelee();

    FightingStyle style = player->getFightingStyle();
    const CombatMove* move = nullptr;

    switch (style)
    {
        case FightingStyle::KungFu:
            move = GetMove(133); // KungFuAbility
            if (!move) move = GetMoveByName("KungFuAbility");
            break;
        case FightingStyle::Karate:
            move = GetMove(132); // KarateAbility
            if (!move) move = GetMoveByName("KarateAbility");
            break;
        case FightingStyle::Aikido:
            move = GetMove(101); // AikidoAbility
            if (!move) move = GetMoveByName("AikidoAbility");
            break;
        case FightingStyle::None:
        default:
            move = GetMove(17);  // SelfDefenseAbility
            if (!move) move = GetMove(600); // CloseCombatTrainingAbility
            if (!move) move = GetMoveByName("SelfDefenseAbility");
            break;
    }

    if (move) return move;
    return DefaultMelee();
}

void CombatSystem::Update()
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	uint32 currTime = getMSTime();

	// M3: from here until the loops finish, EndInterlock()/StopFreeFire() only queue their
	// work (see m_deferredInterlockEnds) - die() inside a round must not erase the node we iterate.
	m_updatingInterlocks = true;

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
			INFO_LOG(format("Interlock %1% vs %2% over after %3% rounds (A %4%, B %5%)")
				% it->goIdA % it->goIdB % it->roundNumber
				% (pA ? (pA->isDead() ? "dead" : "alive") : "gone")
				% (pB ? (pB->isDead() ? "dead" : "alive") : "gone"));
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

	m_updatingInterlocks = false;

	// replay requests that arrived while the loops were running (no iterator is alive now)
	if (!m_deferredFreefireStops.empty()) {
		std::vector<uint32> stops;
		stops.swap(m_deferredFreefireStops);
		for (uint32 goId : stops)
			StopFreeFire(goId);
	}
	if (!m_deferredInterlockEnds.empty()) {
		std::vector< std::pair<uint32,bool> > ends;
		ends.swap(m_deferredInterlockEnds);
		for (const auto& e : ends)
			EndInterlock(e.first, e.second);
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

bool CombatSystem::RequestInterlock(uint32 attackerGoId, uint32 targetGoId, uint32 clientTargetRef)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	if (IsInterlocked(attackerGoId) || IsInterlocked(targetGoId)) {
		INFO_LOG(format("RequestInterlock %1% -> %2% refused: one side already interlocked") % attackerGoId % targetGoId);
		return false;
	}

	PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead()) {
		INFO_LOG(format("RequestInterlock %1% -> %2% refused: %3%") % attackerGoId % targetGoId
			% ((!pA || !pB) ? "attacker or target not found" : "attacker or target is dead"));
		return false;
	}
	{
		float interlockDist = pA->getPosition().Distance(pB->getPosition());
		if (interlockDist > 1500.0f) {
			INFO_LOG(format("RequestInterlock %1% -> %2% refused: distance %3% > 1500 units") % attackerGoId % targetGoId % interlockDist);
			return false;
		}
	}

	// Guarantee both combatants know each other and have views spawned on their clients
	pA->ensureEntityKnown(pB);
	pB->ensureEntityKnown(pA);

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
	session.exchangeNum = 1; //exchange 1 is the opening sent with the IL state below

	// Step interlock participants into melee range (~1.5m = 150.0 units) and face each other squarely
	// before issuing InterlockInitMsg, ensuring accurate animation alignment and camera framing.
	LocationVector posA = pA->getPosition();
	LocationVector posB = pB->getPosition();
	const double targetMeleeDist = 150.0; // 1.5m in world centi-units

	double dx = posB.x - posA.x;
	double dz = posB.z - posA.z;
	double curDist = std::sqrt(dx * dx + dz * dz);
	double midX = (posA.x + posB.x) * 0.5;
	double midZ = (posA.z + posB.z) * 0.5;

	if (curDist > 0.001)
	{
		double dirX = dx / curDist;
		double dirZ = dz / curDist;
		posA.x = midX - dirX * (targetMeleeDist * 0.5);
		posA.z = midZ - dirZ * (targetMeleeDist * 0.5);
		posB.x = midX + dirX * (targetMeleeDist * 0.5);
		posB.z = midZ + dirZ * (targetMeleeDist * 0.5);
	}
	else
	{
		posA.x = midX - (targetMeleeDist * 0.5);
		posB.x = midX + (targetMeleeDist * 0.5);
	}

	posA.rot = std::atan2(-(posB.x - posA.x), -(posB.z - posA.z));
	posB.rot = std::atan2(-(posA.x - posB.x), -(posA.z - posB.z));
	pA->setPosition(posA);
	pB->setPosition(posB);

	sSpatialGrid.UpdateClientPosition(&pA->getClient(), (float)posA.x, (float)posA.z);
	sSpatialGrid.UpdateClientPosition(&pB->getClient(), (float)posB.x, (float)posB.z);

	sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<PositionStateMsg>(pA->getGoId()));
	sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<RotationStateMsg>(pA->getGoId(), posA.getMxoRot()));
	sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<LocomotionStateMsg>(pA->getGoId(), 0, posA.getMxoRot()));
	sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<PositionStateMsg>(pB->getGoId()));
	sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<RotationStateMsg>(pB->getGoId(), posB.getMxoRot()));
	sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<LocomotionStateMsg>(pB->getGoId(), 0, posB.getMxoRot()));
	pA->getClient().QueueState(std::make_shared<LocomotionStateMsg>(pB->getGoId(), 0, posB.getMxoRot()));
	pB->getClient().QueueState(std::make_shared<LocomotionStateMsg>(pA->getGoId(), 0, posA.getMxoRot()));

	//spawn the ILCombatHandler view + pairing packet on both clients - this
	//drives the client-side interlock camera and round UI
	{
		float simTime = sGame.GetSimTime();
		LocationVector ilPos(midX, (posA.y + posB.y) * 0.5, midZ);
		session.ilPos = ilPos;
		session.nextRoundTime = getMSTime() + 2500; //first resolved round right after the opening

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
				if (sides[i].self->getClient().isBot())
					continue; //bots have no client to drive

				//combat mode on - exact HDS capture (CombatHandler.ProcessRequestCloseCombat)
				sides[i].self->getClient().QueueState(std::make_shared<SelfCombatModeOnMsg>());

				uint16 ilViewId = sObjMgr.allocateDynamicView(&sides[i].self->getClient(), uint32(GOID_ILCOMBATHANDLER) << 16);
				*(sides[i].viewSlot) = ilViewId;

				static std::atomic<uint8> s_ilSpawnCounter(0x40);
				uint8 spawnCounter = ++s_ilSpawnCounter;
				if (spawnCounter == 0) spawnCounter = ++s_ilSpawnCounter;
				sides[i].self->getClient().QueueState(std::make_shared<SpawnILCombatHandlerMsg>(
					ilViewId, spawnCounter, ilPos, simTime));

				//the pairing references the opponent's view as this client sees it
				uint16 otherViewId = sObjMgr.getViewForGO(&sides[i].self->getClient(), sides[i].other->getGoId());
				if (otherViewId != 0)
				{
					//HDS echoes the u32 the client sent in its close-combat request. For the
					//requesting player we have it; for the defending side we rebuild it from the
					//opponent's view id and the spawn counter byte our PlayerSpawnMsg uses.
					uint32 otherViewWithSpawnId = uint32(otherViewId) | (uint32(PLAYER_SPAWN_COUNTER) << 16);
					if (i == 0 && clientTargetRef != 0)
						otherViewWithSpawnId = clientTargetRef;
					//slot 1 = opponent, slot 2 = self (view 2, spawn counter 0 as in HDS).
					//The opening exchange mirrors the captured interlock start: the initiator
					//steps in with move 0x2026 after the 0x523C/0x5214 lead-ins.
					std::vector<uint32> slots;
					slots.push_back(otherViewWithSpawnId);
					slots.push_back(uint32(VIEWID_SELF));
					std::vector<ILExchange> opening;
					opening.push_back(BuildExchange(session, sides[i].self, pA, pB, 1,
						IL_MOVE_OPEN_ATTACKER_PRE, IL_MOVE_PRE, IL_MOVE_OPEN_MAIN));
					sides[i].self->getClient().QueueState(std::make_shared<ILCombatStateMsg>(
						ilViewId, ilPos, 1, slots, opening));
				}
				else
				{
					WARNING_LOG(format("Opponent GO %1% has no view on client %2%, skipping interlock pairing packet")
						% sides[i].other->getGoId() % sides[i].self->getGoId());
				}
			}
			catch (ObjectMgr::NoMoreFreeViews) { WARNING_LOG("No free views for interlock handler spawn"); }
			catch (ObjectMgr::ObjectNotAvailable) {}
			catch (ObjectMgr::ClientNotAvailable) {}
		}
	}

	m_interlocks.push_back(session);

	pA->enterInterlock(targetGoId);
	pB->enterInterlock(attackerGoId);
	INFO_LOG(format("Interlock started: %1% (%2%) vs %3% (%4%)") % attackerGoId % pA->getHandle() % targetGoId % pB->getHandle());
	return true;
}

bool CombatSystem::RequestRangedCombat(uint32 attackerGoId, uint32 targetGoId, uint16 moveId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	if (IsFreeFiring(attackerGoId)) return false;
	if (IsInterlocked(attackerGoId)) return false; //no shooting out of an interlock

	PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead()) return false;

	pA->ensureEntityKnown(pB);
	pB->ensureEntityKnown(pA);

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
	if (pB->getClient().isBot()) {
		BotClient* botB = dynamic_cast<BotClient*>(&pB->getClient());
		if (botB && botB->GetTargetGoId() == 0) {
			botB->SetTargetGoId(attackerGoId);
		}
	}
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
	if (m_updatingInterlocks) {
		// Update() is iterating m_freefires - queue it, Update() replays after the loops
		m_deferredFreefireStops.push_back(goId);
		return;
	}
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

	// Update() is walking m_interlocks right now (this call came from die() inside a round):
	// erasing here would invalidate its iterator, so queue the request for after the loop.
	if (m_updatingInterlocks)
	{
		m_deferredInterlockEnds.push_back(std::make_pair(goId, byWithdraw));
		return;
	}

	// Detach every matching session FIRST. Everything below can re-enter this function
	// (a parting shot can kill -> die() -> RemoveCombatant -> EndInterlock), so no list
	// iterator may be alive while callbacks run.
	std::vector<InterlockSession> ended;
	for (auto it = m_interlocks.begin(); it != m_interlocks.end(); ) {
		if (it->goIdA == goId || it->goIdB == goId) {
			ended.push_back(*it);
			it = m_interlocks.erase(it);
		} else {
			++it;
		}
	}

	for (const InterlockSession& s : ended) {
		PlayerObject* pA = getPlayerSafe(s.goIdA);
		PlayerObject* pB = getPlayerSafe(s.goIdB);

		INFO_LOG(format("EndInterlock: %1% vs %2% ended (requested by go %3%, %4%)")
			% s.goIdA % s.goIdB % goId % (byWithdraw ? "withdraw" : "reaped"));

		if (byWithdraw && pA && pB && !pA->isDead() && !pB->isDead()) {
			// Parting Shot
			uint32 attackerId = (s.goIdA == goId) ? s.goIdB : s.goIdA;
			PlayerObject* pAttacker = getPlayerSafe(attackerId);
			PlayerObject* pTarget = getPlayerSafe(goId);
			const CombatMove* move = DefaultMelee();
			if (pAttacker && pTarget && move) {
				float dmg = move->minDmg + (move->maxDmg - move->minDmg) * ((rand() % 100) / 100.0f);
				dmg += move->minDmgPerLvl * pAttacker->getLevel();
				dmg *= pAttacker->getDamageScale();
				dmg *= 1.5f; // Parting shot does 50% extra damage
				if (dmg > 65535.0f) dmg = 65535.0f;

				pTarget->takeDamage(attackerId, (uint16)dmg, move->hitFxId);

				if (!pTarget->getClient().isBot()) {
					pTarget->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[COMBAT] Withdraw Penalty! You suffer a parting shot!{/c}"));
				}
			}
		}

		// the participants may have died / vanished during the parting shot - refetch
		pA = getPlayerSafe(s.goIdA);
		pB = getPlayerSafe(s.goIdB);
		if (pA) {
			if (s.ilViewIdA) {
				pA->getClient().QueueState(std::make_shared<DeleteViewMsg>(s.ilViewIdA));
				sObjMgr.releaseDynamicView(&pA->getClient(), s.ilViewIdA);
			}
			pA->leaveInterlock();
		}
		if (pB) {
			if (s.ilViewIdB) {
				pB->getClient().QueueState(std::make_shared<DeleteViewMsg>(s.ilViewIdB));
				sObjMgr.releaseDynamicView(&pB->getClient(), s.ilViewIdB);
			}
			pB->leaveInterlock();
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
	// Power crushes Speed (+35% damage bonus, frame advantage; Speed deals 0.70x against Power)
	// Speed interrupts Grab (+35% damage bonus, fast interrupt; Grab deals 0.65x if not cancelled)
	// Grab breaks Guard (+40% damage bonus, unblockable throw / block break)
	// Grab breaks Power (+35% damage bonus, counters heavy windup)
	// Guard vs Speed (Guard blocks/deflects light hits: Speed deals 0.60x)
	// Matched tactics clash (0.90x damage, glancing blow)
	if (attackerTactic == TACTIC_POWER && targetTactic == TACTIC_SPEED) return 1.35f;
	if (attackerTactic == TACTIC_SPEED && targetTactic == TACTIC_POWER) return 0.70f;
	if (attackerTactic == TACTIC_SPEED && targetTactic == TACTIC_RETALIATE) return 1.35f;
	if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_SPEED) return 0.65f;
	if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_DEFENSE) return 1.40f;
	if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_POWER) return 1.35f;
	if (attackerTactic == TACTIC_SPEED && targetTactic == TACTIC_DEFENSE) return 0.60f;
	if (attackerTactic == targetTactic && attackerTactic != TACTIC_NORMAL) return 0.90f; // Mirrored tactics glance off
	return 1.0f;
}

CombatSystem::AttackResult CombatSystem::ResolveAttack(PlayerObject* attacker, PlayerObject* target, const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock, bool bypassBlock)
{
	AttackResult res;
	res.hit = true;
	res.isCrit = false;
	res.isBlocked = false;
	res.isGlancing = false;

    // Firearms & Ballistics line-of-sight validation (prevent shooting through buildings)
    if (move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC) {
        LocationVector aPos = attacker->getPosition();
        LocationVector tPos = target->getPosition();
        // Check line of sight through static building geometry at chest height (+100.0f)
        bool hasLOS = sStaticObjMgr.CheckLineOfSight(aPos.x, aPos.y + 100.0f, aPos.z, tPos.x, tPos.y + 100.0f, tPos.z);
        if (!hasLOS) {
            res.hit = false;
            sMatrixThreatHeatmap.RecordDisruption(aPos.x, aPos.z, 5.0f, "Ballistic Impact on Building Cover");
            if (!attacker->getClient().isBot()) {
                attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FF5555}[BALLISTICS] Line of sight obstructed! Bullet intercepted by building geometry.{/c}"
                ));
            }
            if (!target->getClient().isBot()) {
                target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:00FFFF}[BALLISTICS] Incoming gunfire intercepted by building cover!{/c}"
                ));
            }
            return res;
        }
    }

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
    
	// Authentic hit resolution: attack roll vs defense roll (d100 + level accuracy)
	// Blocking in interlock actively absorbs the strike rather than evading it.
	bool isMeleeBlock = (targetTactic == TACTIC_DEFENSE && !bypassBlock && attackerTactic != TACTIC_RETALIATE);
	if (isMeleeBlock)
	{
		res.hit = true;
		res.isBlocked = true;
	}
	else
	{
		int attackRoll = (rand() % 100) + int(attacker->getLevel()) * 2;
		int defenseRoll = (rand() % 100) + int(target->getLevel()) * 2;
		if (attackRoll < defenseRoll)
		{
			res.hit = false;
			if (inInterlock && !m_ilExchangeActive) {
				InterlockAnimPair pair = CombatAnimationMatrix::GetAnimationPair(
					attacker->getFightingStyle(), attackerTactic,
					target->getFightingStyle(), targetTactic,
					InterlockExchangeOutcome::Dodged, move.id
				);
				sGame.AnnounceStateUpdateNear(attacker->getPosition().x, attacker->getPosition().z, 20000.0f,
					std::make_shared<ExtendedAnimationMsg>(attacker->getGoId(), pair.attackerAnimId, 1));
				sGame.AnnounceStateUpdateNear(target->getPosition().x, target->getPosition().z, 20000.0f,
					std::make_shared<ExtendedAnimationMsg>(target->getGoId(), pair.defenderAnimId, 1));
			}
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
	dmg *= attacker->getDamageScale(); //scaled dojo bots hit harder; everyone else is 1.0
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
            attacker->takeDamage(target->getGoId(), static_cast<uint16>(deflectedDmg), 0x280006DF);
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
		if (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_POWER)
		{
			// Grab breaks Power: throws heavy power stance and bypasses toughness absorption
			absorbed = 0.0f;
		}

		if (targetTactic == TACTIC_DEFENSE)
		{
			if (bypassBlock || attackerTactic == TACTIC_RETALIATE)
			{
				// Grab breaks Guard: unblockable throw bypasses defense completely
				if (!target->getClient().isBot())
					target->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[COMBAT] Your Block was broken by a Grab! Unblockable throw!{/c}"));
			}
			else
			{
				dmg *= 0.5f;
				target->restoreIS(5);
			}
		}
		dmg = (dmg > absorbed) ? (dmg - absorbed) : 0.0f;
	}

	if (dmg > 65535.0f) dmg = 65535.0f; //uint16 cast below
	res.damageTaken = (uint16)dmg;
    
    // [Item 15] Melee Weapon Durability
    if (res.hit && move.dmgType == DAMAGE_MELEE) {
        attacker->degradeEquippedWeapon(1);
    }
    
    // Item 32: Takedown Moves & Interlock Animation Dispatch
    if (res.hit && target->getCurrentHealth() <= res.damageTaken) {
        if (inInterlock && !m_ilExchangeActive) {
            InterlockAnimPair pair = CombatAnimationMatrix::GetAnimationPair(
                attacker->getFightingStyle(), attackerTactic,
                target->getFightingStyle(), targetTactic,
                InterlockExchangeOutcome::GuardBreak, move.id
            );
            auto animAtk = std::make_shared<ExtendedAnimationMsg>(attacker->getGoId(), pair.attackerAnimId, 1);
            auto animDef = std::make_shared<ExtendedAnimationMsg>(target->getGoId(), pair.defenderAnimId, 1);
            //AnnounceStateUpdateNear already reaches both combatants - queueing the same
            //shared message again serialized it twice and raced setReceiver/toBuf
            sGame.AnnounceStateUpdateNear(attacker->getPosition().x, attacker->getPosition().z, 20000.0f, animAtk);
            sGame.AnnounceStateUpdateNear(target->getPosition().x, target->getPosition().z, 20000.0f, animDef);
        }
        // arm the 3 s takedown timer only once; a second lethal hit must not push death out
        if (target->m_deathDelayMS == 0)
        {
            target->m_deathDelayMS = getMSTime() + 3000; // 3 seconds takedown animation
            target->m_deathDelayKillerId = attacker->getGoId();
        }
    }
    else if (res.hit && inInterlock && !m_ilExchangeActive && !(attackerTactic == targetTactic && attackerTactic != TACTIC_NORMAL)) {
        InterlockExchangeOutcome hitOutcome = res.isBlocked ? InterlockExchangeOutcome::Blocked :
            ((attackerTactic == TACTIC_POWER && targetTactic == TACTIC_SPEED) ? InterlockExchangeOutcome::StanceCrush :
            ((attackerTactic == TACTIC_SPEED && targetTactic == TACTIC_RETALIATE) ? InterlockExchangeOutcome::FastInterrupt :
            ((attackerTactic == TACTIC_RETALIATE && (targetTactic == TACTIC_DEFENSE || targetTactic == TACTIC_POWER)) ? InterlockExchangeOutcome::GuardBreak :
            ((move.id == 197 || move.id == 198 || move.id == 296 || move.id == 531) ? InterlockExchangeOutcome::SpecialHit :
            InterlockExchangeOutcome::NormalHit))));
        InterlockAnimPair pair = CombatAnimationMatrix::GetAnimationPair(
            attacker->getFightingStyle(), attackerTactic,
            target->getFightingStyle(), targetTactic,
            hitOutcome, move.id
        );
        auto animAtk = std::make_shared<ExtendedAnimationMsg>(attacker->getGoId(), pair.attackerAnimId, 1);
        auto animDef = std::make_shared<ExtendedAnimationMsg>(target->getGoId(), pair.defenderAnimId, 1);
        sGame.AnnounceStateUpdateNear(attacker->getPosition().x, attacker->getPosition().z, 20000.0f, animAtk);
        sGame.AnnounceStateUpdateNear(target->getPosition().x, target->getPosition().z, 20000.0f, animDef);
    }

	if (res.hit)
	{
		// Synchronized combat animation subpacket trigger
		static const uint32 s_cfgHitFx = (uint32)sConfig.GetIntDefault("Combat.HitFx", 0);
		uint32 hitFx = s_cfgHitFx;
		if (res.isBlocked) {
			hitFx = (s_cfgHitFx != 0) ? 0x28000794 : 0; // FX_CHARACTER_BLOCK_INTERLOCK (authentic block spark)
		} else if (inInterlock) {
			if (bypassBlock || attackerTactic == TACTIC_RETALIATE) {
				hitFx = (s_cfgHitFx != 0) ? ((attacker->getFightingStyle() == FightingStyle::Karate) ? 0x2800045A : 0x28000432) : 0;
			} else if (move.id == 531) {
				hitFx = (s_cfgHitFx != 0) ? 0x2800045A : 0; // Ki aura impact
			} else if (move.id == 296) {
				hitFx = (s_cfgHitFx != 0) ? 0x28000432 : 0; // Throw impact
			} else {
				hitFx = s_cfgHitFx;
			}
		} else if (move.hitFxId != 0 && move.hitFxId != 1234) {
			hitFx = (s_cfgHitFx != 0) ? move.hitFxId : 0;
		}
		target->takeDamage(attacker->getGoId(), res.damageTaken, hitFx);
        target->recordIncomingAttack(move.id);

		if (res.isBlocked) {
			if (!attacker->getClient().isBot()) {
				attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
					(format("{c:FFFF00}[COMBAT] %1% blocked your %2%! (%3% damage, -50%%){/c}")
					 % target->getHandle() % move.name % res.damageTaken).str()
				));
			}
			if (!target->getClient().isBot()) {
				target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
					(format("{c:00FFFF}[COMBAT] You blocked %1%'s %2%! (%3% damage taken, +5 IS){/c}")
					 % attacker->getHandle() % move.name % res.damageTaken).str()
				));
			}
		} else {
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
		}

		// H1: the kill reward is paid by PlayerObject::die(), which runs for every death
		// (zero-delay deaths inside takeDamage and the delayed takedown deaths alike).
	}
	return res;
}

ILExchange CombatSystem::BuildExchange(const InterlockSession &session, PlayerObject* viewer,
	PlayerObject* attacker, PlayerObject* defender, uint16 number,
	uint32 attackerPreMove, uint32 defenderPreMove, uint32 mainMove)
{
	ILExchange e;
	//on every client: slot 2 = that client's own character, slot 1 = the opponent
	e.attackerSlot = (attacker == viewer) ? 2 : 1;
	e.defenderSlot = (defender == viewer) ? 2 : 1;
	LocationVector a = attacker->getPosition();
	LocationVector d = defender->getPosition();
	e.attackerPos[0] = float(a.x - session.ilPos.x);
	e.attackerPos[1] = float(a.y - session.ilPos.y);
	e.attackerPos[2] = float(a.z - session.ilPos.z);
	e.defenderPos[0] = float(d.x - session.ilPos.x);
	e.defenderPos[1] = float(d.y - session.ilPos.y);
	e.defenderPos[2] = float(d.z - session.ilPos.z);
	e.number = number;
	//far-future start + flag bit0: the client clamps it to "now + 5 ms" on its own clock,
	//so server/client clock skew can't make it skip the exchange
	e.startMs = 1000000;
	e.defenderOffsetMs = 666;		//values from the captured exchanges
	e.attackerExtraMs = 1332;
	e.defenderExtraMs = 1333;
	e.flags = 0x03;
	if (attackerPreMove) { e.moves[0][0] = attackerPreMove; e.moves[0][1] = IL_MOVE_DATABASE; }
	if (defenderPreMove) { e.moves[1][0] = defenderPreMove; e.moves[1][1] = IL_MOVE_DATABASE; }
	e.moves[2][0] = mainMove; e.moves[2][1] = IL_MOVE_DATABASE;
	e.attackerHealth = attacker->getCurrentHealth();
	e.defenderHealth = defender->getCurrentHealth();
	return e;
}

void CombatSystem::SendInterlockExchange(InterlockSession &session, PlayerObject* attacker, PlayerObject* defender)
{
	if (session.ilViewIdA == 0 && session.ilViewIdB == 0)
		return;
	//main moves seen in captured live exchanges (interlock database 0x24000B8B)
	static const uint32 mainMoves[] = { 0x2026, 0x2388, 0x236D, 0x2367, 0x4EE5 };
	session.exchangeNum++;
	uint32 mainMove = mainMoves[session.exchangeNum % (sizeof(mainMoves)/sizeof(mainMoves[0]))];

	PlayerObject* pA = getPlayerSafe(session.goIdA);
	PlayerObject* pB = getPlayerSafe(session.goIdB);
	struct { PlayerObject* p; uint16 view; } sides[2] = { { pA, session.ilViewIdA }, { pB, session.ilViewIdB } };
	for (int i = 0; i < 2; i++)
	{
		if (!sides[i].p || sides[i].view == 0 || sides[i].p->getClient().isBot())
			continue;
		std::vector<ILExchange> ex;
		ex.push_back(BuildExchange(session, sides[i].p, attacker, defender, session.exchangeNum,
			IL_MOVE_PRE, IL_MOVE_PRE, mainMove));
		sides[i].p->getClient().QueueState(std::make_shared<ILCombatStateMsg>(
			sides[i].view, session.ilPos, session.exchangeNum, std::vector<uint32>(), ex));
	}
	INFO_LOG(format("Interlock exchange %1%: %2% -> %3% move 0x%4$04X (HP %5% / %6%)")
		% session.exchangeNum % attacker->getHandle() % defender->getHandle() % mainMove
		% attacker->getCurrentHealth() % defender->getCurrentHealth());
}

CombatSystem::AttackResult CombatSystem::StrikeInterlock(InterlockSession &session, PlayerObject* attacker,
	PlayerObject* target, const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock, bool bypassBlock)
{
	const bool exchangeDriven = (session.ilViewIdA != 0 || session.ilViewIdB != 0);
	m_ilExchangeActive = exchangeDriven;
	AttackResult res = ResolveAttack(attacker, target, move, attackerTactic, targetTactic, inInterlock, bypassBlock);
	m_ilExchangeActive = false;
	if (exchangeDriven)
		SendInterlockExchange(session, attacker, target);
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

	const CombatMove* moveA = (session.queuedMoveA != 0 && session.queuedMoveA != 1) ? GetMove(session.queuedMoveA) : GetDefaultStyleMove(pA);
	const CombatMove* moveB = (session.queuedMoveB != 0 && session.queuedMoveB != 1) ? GetMove(session.queuedMoveB) : GetDefaultStyleMove(pB);

	// Bot combat AI: update round tactics and discipline moves
	if (pA->getClient().isBot()) {
		uint8 newTac = TACTIC_POWER;
		bool isElite = (pA->getHandle().find("Agent") != std::string::npos || pA->getLevel() >= 30);
		if (isElite) {
			switch (session.tacticB) {
				case TACTIC_POWER:     newTac = TACTIC_RETALIATE; break;
				case TACTIC_SPEED:     newTac = TACTIC_POWER;     break;
				case TACTIC_RETALIATE: newTac = TACTIC_SPEED;     break;
				case TACTIC_DEFENSE:   newTac = TACTIC_RETALIATE; break;
				default:               newTac = (rand() % 2 == 0) ? TACTIC_POWER : TACTIC_SPEED; break;
			}
		} else {
			const uint8 thugTactics[] = { TACTIC_POWER, TACTIC_SPEED, TACTIC_RETALIATE, TACTIC_DEFENSE };
			newTac = thugTactics[rand() % 4];
		}
		session.tacticA = newTac;

		if (session.queuedMoveA == 0 || session.queuedMoveA == 1) {
			if (pA->getFightingStyle() == FightingStyle::KungFu) {
				const uint16 kfMoves[] = { 133, 570, 574, 197, 198 };
				uint16 selected = kfMoves[rand() % 5];
				const CombatMove* m = GetMove(selected);
				if (m) moveA = m;
			} else if (pA->getFightingStyle() == FightingStyle::Karate) {
				const uint16 karateMoves[] = { 132, 569, 573, 531, 197 };
				uint16 selected = karateMoves[rand() % 5];
				const CombatMove* m = GetMove(selected);
				if (m) moveA = m;
			} else if (pA->getFightingStyle() == FightingStyle::Aikido) {
				const uint16 aikidoMoves[] = { 101, 296, 571, 572, 198 };
				uint16 selected = aikidoMoves[rand() % 5];
				const CombatMove* m = GetMove(selected);
				if (m) moveA = m;
			} else {
				moveA = GetDefaultStyleMove(pA);
			}
		}
	}
	if (pB->getClient().isBot()) {
		uint8 newTac = TACTIC_POWER;
		bool isElite = (pB->getHandle().find("Agent") != std::string::npos || pB->getLevel() >= 30);
		if (isElite) {
			switch (session.tacticA) {
				case TACTIC_POWER:     newTac = TACTIC_RETALIATE; break;
				case TACTIC_SPEED:     newTac = TACTIC_POWER;     break;
				case TACTIC_RETALIATE: newTac = TACTIC_SPEED;     break;
				case TACTIC_DEFENSE:   newTac = TACTIC_RETALIATE; break;
				default:               newTac = (rand() % 2 == 0) ? TACTIC_POWER : TACTIC_SPEED; break;
			}
		} else {
			const uint8 thugTactics[] = { TACTIC_POWER, TACTIC_SPEED, TACTIC_RETALIATE, TACTIC_DEFENSE };
			newTac = thugTactics[rand() % 4];
		}
		session.tacticB = newTac;

		if (session.queuedMoveB == 0 || session.queuedMoveB == 1) {
			if (pB->getFightingStyle() == FightingStyle::KungFu) {
				const uint16 kfMoves[] = { 133, 570, 574, 197, 198 };
				uint16 selected = kfMoves[rand() % 5];
				const CombatMove* m = GetMove(selected);
				if (m) moveB = m;
			} else if (pB->getFightingStyle() == FightingStyle::Karate) {
				const uint16 karateMoves[] = { 132, 569, 573, 531, 197 };
				uint16 selected = karateMoves[rand() % 5];
				const CombatMove* m = GetMove(selected);
				if (m) moveB = m;
			} else if (pB->getFightingStyle() == FightingStyle::Aikido) {
				const uint16 aikidoMoves[] = { 101, 296, 571, 572, 198 };
				uint16 selected = aikidoMoves[rand() % 5];
				const CombatMove* m = GetMove(selected);
				if (m) moveB = m;
			} else {
				moveB = GetDefaultStyleMove(pB);
			}
		}
	}

	uint8 tacA = session.tacticA;
	uint8 tacB = session.tacticB;

	const bool humanRound = !pA->getClient().isBot() || !pB->getClient().isBot();
	if (humanRound)
	{
		INFO_LOG(format("Interlock round %1% start: %2%:%3% (tactic %4%, move %5%, HP %6%/%7%) vs %8%:%9% (tactic %10%, move %11%, HP %12%/%13%)")
			% session.roundNumber
			% pA->getHandle() % pA->getGoId() % uint32(tacA) % (moveA ? moveA->name : std::string("<none>")) % pA->getCurrentHealth() % pA->getMaximumHealth()
			% pB->getHandle() % pB->getGoId() % uint32(tacB) % (moveB ? moveB->name : std::string("<none>")) % pB->getCurrentHealth() % pB->getMaximumHealth());
	}

	// Deterministic Rock-Paper-Scissors martial arts interlock evaluations:
	// 1. Power crushes Speed (+35% damage bonus, frame advantage)
	// 2. Speed interrupts Grab (+35% damage bonus, fast interrupt cancels grab)
	// 3. Grab breaks Guard (+40% damage bonus, unblockable throw bypasses block)
	//    Grab breaks Power (+35% damage bonus, unblockable throw counters heavy windup)
	bool aCrushesB = (tacA == TACTIC_POWER && tacB == TACTIC_SPEED);
	bool bCrushesA = (tacB == TACTIC_POWER && tacA == TACTIC_SPEED);

	bool aInterruptsB = (tacA == TACTIC_SPEED && tacB == TACTIC_RETALIATE);
	bool bInterruptsA = (tacB == TACTIC_SPEED && tacA == TACTIC_RETALIATE);

	bool aBreaksGuardB = (tacA == TACTIC_RETALIATE && tacB == TACTIC_DEFENSE);
	bool bBreaksGuardA = (tacB == TACTIC_RETALIATE && tacA == TACTIC_DEFENSE);

	bool aBreaksPowerB = (tacA == TACTIC_RETALIATE && tacB == TACTIC_POWER);
	bool bBreaksPowerA = (tacB == TACTIC_RETALIATE && tacA == TACTIC_POWER);

	bool isClash = (tacA == tacB && tacA != TACTIC_NORMAL);

	FightingStyle styleA = pA->getFightingStyle();
	FightingStyle styleB = pB->getFightingStyle();

	// Tactical banner announcements
	if (aCrushesB) {
		if (!pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Power crushes Speed! Frame advantage secured against %1%! (+35%% Damage){/c}") % pB->getHandle()).str()));
		if (!pB->getClient().isBot())
			pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Power stance crushed your Speed attack!{/c}") % pA->getHandle()).str()));
	} else if (bCrushesA) {
		if (!pB->getClient().isBot())
			pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Power crushes Speed! Frame advantage secured against %1%! (+35%% Damage){/c}") % pA->getHandle()).str()));
		if (!pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Power stance crushed your Speed attack!{/c}") % pB->getHandle()).str()));
	} else if (aInterruptsB) {
		if (!pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Speed interrupts Grab! Fast jab interrupted %1%'s grab maneuver! (+35%% Damage){/c}") % pB->getHandle()).str()));
		if (!pB->getClient().isBot())
			pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Speed strike interrupted your Grab maneuver!{/c}") % pA->getHandle()).str()));
	} else if (bInterruptsA) {
		if (!pB->getClient().isBot())
			pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Speed interrupts Grab! Fast jab interrupted %1%'s grab maneuver! (+35%% Damage){/c}") % pA->getHandle()).str()));
		if (!pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Speed strike interrupted your Grab maneuver!{/c}") % pB->getHandle()).str()));
	} else if (aBreaksGuardB || aBreaksPowerB) {
		const char* targetStance = aBreaksGuardB ? "Guard" : "Power";
		if (!pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Grab breaks %1%! Unblockable throw executed on %2%! (+%3%%% Damage){/c}") % targetStance % pB->getHandle() % (aBreaksGuardB ? 40 : 35)).str()));
		if (!pB->getClient().isBot())
			pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF0000}[MARTIAL ARTS] Your %1% was broken by %2%'s Grab! Unblockable throw!{/c}") % targetStance % pA->getHandle()).str()));
	} else if (bBreaksGuardA || bBreaksPowerA) {
		const char* targetStance = bBreaksGuardA ? "Guard" : "Power";
		if (!pB->getClient().isBot())
			pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Grab breaks %1%! Unblockable throw executed on %2%! (+%3%%% Damage){/c}") % targetStance % pA->getHandle() % (bBreaksGuardA ? 40 : 35)).str()));
		if (!pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF0000}[MARTIAL ARTS] Your %1% was broken by %2%'s Grab! Unblockable throw!{/c}") % targetStance % pB->getHandle()).str()));
	} else if (isClash) {
		// Both combatants selected identical stance - dispatch clash rebound animations
		if (session.ilViewIdA == 0 && session.ilViewIdB == 0) //human interlocks animate through IL exchanges
		{
			InterlockAnimPair pair = CombatAnimationMatrix::GetAnimationPair(styleA, tacA, styleB, tacB, InterlockExchangeOutcome::Clash, moveA ? moveA->id : 0);
			sGame.AnnounceStateUpdateNear(pA->getPosition().x, pA->getPosition().z, 20000.0f, std::make_shared<ExtendedAnimationMsg>(pA->getGoId(), pair.attackerAnimId, 1));
			sGame.AnnounceStateUpdateNear(pB->getPosition().x, pB->getPosition().z, 20000.0f, std::make_shared<ExtendedAnimationMsg>(pB->getGoId(), pair.defenderAnimId, 1));
		}
		std::string clashName = (tacA == TACTIC_POWER) ? "Power" : (tacA == TACTIC_SPEED ? "Speed" : (tacA == TACTIC_RETALIATE ? "Grab" : "Guard"));
		if (!pA->getClient().isBot()) pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FFFF00}[MARTIAL ARTS] Stance Clash! Both combatants chose %1%. Glancing exchange.{/c}") % clashName).str()));
		if (!pB->getClient().isBot()) pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FFFF00}[MARTIAL ARTS] Stance Clash! Both combatants chose %1%. Glancing exchange.{/c}") % clashName).str()));

		// Both combatants exchange glancing blows at 0.90x damage (TacticModifier handles 0.90x)
		if (moveA) StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, false);
		if (moveB && !pB->isDead()) StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, false);
	}

	// Execution & Interrupt Resolution:
	if (aInterruptsB) {
		// A's Speed interrupts B's Grab. A strikes, B's grab is cancelled!
		if (moveA) StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, false);
	} else if (bInterruptsA) {
		// B's Speed interrupts A's Grab. B strikes, A's grab is cancelled!
		if (moveB) StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, false);
	} else if (aCrushesB) {
		// A's Power crushes B's Speed with frame advantage
		if (moveA) StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, false);
	} else if (bCrushesA) {
		// B's Power crushes A's Speed with frame advantage
		if (moveB) StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, false);
	} else if (aBreaksPowerB || aBreaksGuardB) {
		// A's Grab counters B's heavy Power attack or breaks Guard with an unblockable throw.
		if (moveA) StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, true);
	} else if (bBreaksPowerA || bBreaksGuardA) {
		// B's Grab counters A's heavy Power attack or breaks Guard with an unblockable throw.
		if (moveB) StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, true);
	} else if (!isClash) {
		// Standard exchange: defenders only strike if an active special is queued
		bool specialFromA = (session.queuedMoveA != 0);
		if (tacA != TACTIC_DEFENSE || specialFromA) {
			if (moveA) {
				StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, false);
			}
		}
		if (!pB->isDead()) {
			bool specialFromB = (session.queuedMoveB != 0);
			if (tacB != TACTIC_DEFENSE || specialFromB) {
				if (moveB) {
					StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, false);
				}
			}
		}
	}

	session.queuedMoveA = 0;
	session.queuedMoveB = 0;

	if (humanRound)
	{
		INFO_LOG(format("Interlock round %1% resolved: %2%:%3% HP %4%/%5%%6% | %7%:%8% HP %9%/%10%%11%")
			% session.roundNumber
			% pA->getHandle() % pA->getGoId() % pA->getCurrentHealth() % pA->getMaximumHealth() % (pA->isDead() ? " (dead)" : "")
			% pB->getHandle() % pB->getGoId() % pB->getCurrentHealth() % pB->getMaximumHealth() % (pB->isDead() ? " (dead)" : ""));
	}

	// Re-align and clamp spacing to 150.0 +/- 5.0 units facing each other to eliminate root-motion drift
	if (!pA->isDead() && !pB->isDead())
	{
		LocationVector posA = pA->getPosition();
		LocationVector posB = pB->getPosition();
		double dx = posB.x - posA.x;
		double dz = posB.z - posA.z;
		double dist = std::sqrt(dx * dx + dz * dz);
		if (dist > 1.0)
		{
			double normX = dx / dist;
			double normZ = dz / dist;
			const double targetSpacing = 150.0; // 1.5m retail interlock engagement distance
			LocationVector clampedBPos = posB;
			clampedBPos.x = posA.x + normX * targetSpacing;
			clampedBPos.y = posA.y; // maintain pavement elevation flush with pavement
			clampedBPos.z = posA.z + normZ * targetSpacing;
			pB->setPosition(clampedBPos);
			sGame.AnnounceStateUpdate(NULL, std::make_shared<PositionStateMsg>(pB->getGoId()));
		}
	}

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
    if (!caster) return false;

    // Resolve target if not explicitly provided
    if (targetGoId == 0)
    {
        if (IsInterlocked(caster->getGoId()))
            targetGoId = caster->getInterlockPartner();
        else if (caster->getTargetGoId() != 0)
            targetGoId = caster->getTargetGoId();
    }

    const bool humanCaster = !caster->getClient().isBot();
    const uint16 requestedAbilityId = abilityId;

    // Check authentic discipline routing (Hacker viral logic / Coder RSI support)
    const AbilityTemplate* abilTempl = sDataLoader.GetAbilityTemplate(requestedAbilityId);
    if (abilTempl)
    {
        if (abilTempl->discipline == DisciplineType::HACKER)
        {
            return sHackerSystem.ExecuteHackerAbility(caster, requestedAbilityId, targetGoId, abilTempl);
        }
        else if (abilTempl->discipline == DisciplineType::CODER)
        {
            return sHackerSystem.ExecuteCoderAbility(caster, requestedAbilityId, targetGoId, abilTempl);
        }
        else if (abilTempl->discipline == DisciplineType::GUNNER)
        {
            return sHackerSystem.ExecuteSoldierAbility(caster, requestedAbilityId, targetGoId, abilTempl);
        }
        else if (abilTempl->discipline == DisciplineType::SPY)
        {
            return sHackerSystem.ExecuteSpyAbility(caster, requestedAbilityId, targetGoId, abilTempl);
        }
        else if (abilTempl->discipline == DisciplineType::MARTIAL_ARTIST)
        {
            if (abilTempl->name.find("KungFu") != std::string::npos)
                caster->setFightingStyle(FightingStyle::KungFu);
            else if (abilTempl->name.find("Karate") != std::string::npos)
                caster->setFightingStyle(FightingStyle::Karate);
            else if (abilTempl->name.find("Aikido") != std::string::npos)
                caster->setFightingStyle(FightingStyle::Aikido);
            else if (abilTempl->name.find("SelfDefense") != std::string::npos ||
                     abilTempl->name.find("CloseCombat") != std::string::npos ||
                     abilTempl->name.find("MartialArts") != std::string::npos)
                caster->setFightingStyle(FightingStyle::None);
        }
    }

    const CombatMove* move = GetMove(abilityId);
    if (!move)
    {
        const AbilityTemplate* dynTempl = sDataLoader.GetAbilityTemplate(requestedAbilityId);
        if (dynTempl)
        {
            CombatMove dynMove;
            dynMove.id = dynTempl->abilityId;
            dynMove.name = dynTempl->name;
            dynMove.dmgType = (dynTempl->discipline == DisciplineType::GUNNER) ? DAMAGE_RANGED :
                              (dynTempl->discipline == DisciplineType::HACKER) ? DAMAGE_VIRAL :
                              (dynTempl->discipline == DisciplineType::CODER) ? DAMAGE_HACKING : DAMAGE_MELEE;
            dynMove.minDmg = (dynTempl->valueFrom > 0) ? float(dynTempl->valueFrom) : 15.0f;
            dynMove.maxDmg = (dynTempl->valueTo > dynTempl->valueFrom) ? float(dynTempl->valueTo) : (dynMove.minDmg * 1.4f);
            dynMove.minDmgPerLvl = 1.0f;
            dynMove.maxDmgPerLvl = 1.5f;
            dynMove.isCost = dynTempl->innerStrengthCost;
            dynMove.range = (dynTempl->discipline == DisciplineType::GUNNER || dynTempl->discipline == DisciplineType::HACKER) ? 3500.0f : 300.0f;
            dynMove.hitFxId = dynTempl->executionFX != 0 ? dynTempl->executionFX : 0x280006DF;
            dynMove.interlockOnly = (dynTempl->discipline == DisciplineType::MARTIAL_ARTIST);
            dynMove.freefireOnly = (dynTempl->discipline == DisciplineType::GUNNER || dynTempl->discipline == DisciplineType::HACKER || dynTempl->discipline == DisciplineType::CODER);
            dynMove.castTime = float(dynTempl->castTime) / 1000.0f;
            dynMove.specialFlags = 0;
            {
                std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
                m_moveTable[dynMove.id] = dynMove;
            }
            move = GetMove(abilityId);
        }
    }
    if (!move)
    {
        // Unknown ability ids fallback to basic strike for humans
        if (!humanCaster) return false;

        const CombatMove* fallback = DefaultMelee();
        INFO_LOG(format("UseAbility: %1%:%2% ability %3% has no server move definition, falling back to %4%")
            % caster->getHandle() % caster->getGoId() % requestedAbilityId % (fallback ? fallback->name : std::string("<none>")));
        if (!fallback) return false;
        caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:FFFF00}[COMBAT] Ability %1% is not implemented on this server yet - using basic strike.{/c}") % requestedAbilityId).str()));
        move = fallback;
        abilityId = fallback->id;
    }

    if (move && !abilTempl)
    {
        if (move->name.find("KungFu") != std::string::npos)
            caster->setFightingStyle(FightingStyle::KungFu);
        else if (move->name.find("Karate") != std::string::npos)
            caster->setFightingStyle(FightingStyle::Karate);
        else if (move->name.find("Aikido") != std::string::npos)
            caster->setFightingStyle(FightingStyle::Aikido);
        else if (move->name.find("SelfDefense") != std::string::npos ||
                 move->name.find("CloseCombat") != std::string::npos ||
                 move->name.find("MartialArts") != std::string::npos)
            caster->setFightingStyle(FightingStyle::None);
    }

    INFO_LOG(format("UseAbility: %1%:%2% uses %3% (requested id %4%, move id %5%) on target go %6% [interlocked=%7% castTime=%8%]")
        % caster->getHandle() % caster->getGoId() % move->name % requestedAbilityId % abilityId % targetGoId
        % IsInterlocked(caster->getGoId()) % move->castTime);

    //cast bar for anything with a cast time
    if (move->castTime > 0.05f && humanCaster)
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, move->castTime));
    
    // Disruption from special ability execution
    sMatrixThreatHeatmap.RecordDisruption(caster->getPosition().x, caster->getPosition().z, 12.0f, move->name);

    // melee abilities pressed outside an interlock open one against the selected target
    if (move->interlockOnly && !IsInterlocked(caster->getGoId()))
    {
        if (targetGoId == 0 || !RequestInterlock(caster->getGoId(), targetGoId))
        {
            INFO_LOG(format("UseAbility: %1%:%2% %3% needs an interlock but none could be opened (target go %4%)")
                % caster->getHandle() % caster->getGoId() % move->name % targetGoId);
            if (humanCaster)
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    (format("{c:FF0000}[COMBAT] %1% needs a valid target within melee range.{/c}") % move->name).str()));
            return false;
        }
    }
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
    INFO_LOG(format("AwardKill: %1%:%2% defeated %3%:%4% -> +%5% XP, +%6% $Info (killer total %7% XP, %8% $Info)")
        % killer->getHandle() % killer->getGoId() % victim->getHandle() % victim->getGoId()
        % exp % infoAmount % killer->getExperience() % killer->getInformation());
    if (!killer->getClient().isBot()) {
        killer->getClient().QueueCommand(std::make_shared<SetInformationCmd>(killer->getInformation()));
        killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:00FF00}[COMBAT] Defeated %1%! Looted %2% $Info.{/c}") % victim->getHandle() % infoAmount).str()
        ));
        killer->saveCashToDB();
    }
}




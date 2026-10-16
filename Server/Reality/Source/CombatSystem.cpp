#include <set>
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
#include "StatusEffectManager.h"
#include "InventorySystem.h"
#include "StaticObjectManager.h"
#include "AbilitySystem.h"
#include "CombatAnimationMatrix.h"
#include "HackerSystem.h"
#include "Config.h"
#include "AgentPossessionManager.h"
#include "FactionWarManager.h"
#include "AI/PedestrianEcology.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
static bool IsServerDrivenBot(PlayerObject* p);

const float CombatSystem::INTERLOCK_ROUND_SECONDS = 4.0f; //authentic MxO interlock round
const float CombatSystem::FREEFIRE_SHOT_SECONDS = 2.0f;

createFileSingleton(CombatSystem);

// Interlock participant slots as one client sees them. Shared by the opening and every round
// so both always agree.
// "hds" order (default), from HDS CombatHandler.cs L63-67:
//   mask 3, slot 1 = the opponent handle the client sent in its close-combat request (L64),
//   slot 2 = u16 2 (own avatar view) + u16 selfSpawnIdCounter (L65-66).
// "self" order (Interlock.SlotOrder = self) keeps the earlier layout: slot 1 self, slot 2 opponent.
// That layout came from a reading of client.dll 0x624fd270 that was never confirmed in game.
std::vector<uint32> BuildILSlots(uint32 otherRef)
{
	// Interlock.SelfSpawnCounter: spawn counter byte of our own avatar handle. HDS sent
	// playerData.selfSpawnIdCounter, which it never assigned (0). Our self spawn uses 0x2F.
	const int selfCounter = sConfig.GetIntDefault("Interlock.SelfSpawnCounter", PLAYER_SPAWN_COUNTER);
	const uint32 selfRef = uint32(VIEWID_SELF) | (uint32(selfCounter & 0xFF) << 16);
	std::vector<uint32> slots;
	if (ILSlotsOpponentFirst())
	{
		slots.push_back(otherRef);
		slots.push_back(selfRef);
	}
	else
	{
		slots.push_back(selfRef);
		slots.push_back(otherRef);
	}
	return slots;
}

uint8 ILSlotFor(bool isViewer)
{
	const bool oppFirst = ILSlotsOpponentFirst();
	if (isViewer)
		return oppFirst ? 2 : 1;
	return oppFirst ? 1 : 2;
}

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
		std::set<uint16> aliasedToBase; // placeholders the retail table below must replace
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
				aliasedToBase.insert(alias.id);
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
			// the loadout aliases ran first, so 17/137/600 (the default hotbar) stayed basic-strike
			// placeholders and their retail rows were skipped: testers fought with the weakest move
			if (m_moveTable.find(rm.id) == m_moveTable.end() || aliasedToBase.count(rm.id)) {
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

		// The hardcoded martial list above also registered firearms and spy tools as
		// 3m interlock punches, and synthesis skips ids already in the table.
		auto tuneMove = [this](uint16 id, mxoDamageType dmg, float range, bool interlockOnly, bool freefireOnly)
		{
			auto it = m_moveTable.find(id);
			if (it == m_moveTable.end()) return;
			it->second.dmgType = dmg;
			it->second.range = range;
			it->second.interlockOnly = interlockOnly;
			it->second.freefireOnly = freefireOnly;
		};
		tuneMove(14,  DAMAGE_RANGED, 3500.0f, false, true);  // PowerShot
		tuneMove(126, DAMAGE_RANGED, 3500.0f, false, true);  // PistolDisarmingShot
		tuneMove(129, DAMAGE_RANGED, 3500.0f, false, true);  // Handguns
		tuneMove(147, DAMAGE_RANGED, 4500.0f, false, true);  // Rifles
		tuneMove(453, DAMAGE_MELEE,  300.0f,  true,  false); // Rifle butt is a melee smash, not a volley
		tuneMove(499, DAMAGE_RANGED, 400.0f,  false, true);  // Point blank
		tuneMove(501, DAMAGE_MELEE,  300.0f,  true,  false); // Pistol whip
		tuneMove(505, DAMAGE_RANGED, 8000.0f, false, true);  // Sniper
		tuneMove(146, DAMAGE_MELEE,  400.0f,  false, false); // Poison knife
		tuneMove(283, DAMAGE_RANGED, 3000.0f, false, true);  // Knife throw
		tuneMove(209, DAMAGE_MELEE,  0.0f,    false, false); // Stealth is not a strike
		if (m_moveTable.count(209))
		{
			m_moveTable[209].minDmg = 0.0f;
			m_moveTable[209].maxDmg = 0.0f;
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
    const CombatMove* m = GetMove(129); // HandgunsAbility
    if (!m) m = GetMoveByName("HandgunsAbility");
    if (!m) m = GetMove(14); // PowerShotAbility
    return m;
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

// RPC 0x45 to each human in the interlock (field map in MessageTypes.h): both sides' cards, who hit,
// the damage each took and when the next round locks. It drives the retail interlock HUD (tactic
// icons, numbers, VS / lock timer) and the client's own combat-log lines ("X hit you with their ...").
static ILCard BuildILCard(uint8 committedTactic, uint16 moveId, int dealt)
{
	ILCard c;
	const uint8 setting = ClientSettingFromTactic(committedTactic);
	if (setting <= CLIENT_TACTIC_BLOCK)
	{
		c.type = 0;		// basic tactic attack: the log names the tactic
		c.setting = setting;
	}
	else
	{
		c.type = 1;		// no tactic chosen: the log names the ability that was swung
		c.setting = 0;
		c.additional = moveId;
	}
	c.combatValue = int16(std::max(0, std::min(dealt, 32767)));
	return c;
}

static void SendILRoundStatus(const InterlockSession &session, PlayerObject* pA, PlayerObject* pB,
	int dmgByA, int dmgByB, uint32 msUntilLock)
{
	if (!pA || !pB)
		return;
	// Interlock.HudRpcs = 0 turns the 0x40/0x45 HUD feed off without a rebuild (config reload)
	if (!sConfig.GetBoolDefault("Interlock.HudRpcs", true))
		return;
	const uint32 lockMs = uint32(double(sGame.GetSimTime()) * 1000.0 + 0.5) + msUntilLock;
	// Interlock.HudIsAttacker: -1 (default) = viewer is the attacker when it is the attacker of the
	// current ILExchange (client.dll 0x620bd3fa checks st.isAttacker against [ebx+0x34] attackerSlot
	// vs [ebx+0x38] defenderSlot and maps outcomeFlag bit 0x1 to attackerSlot, bit 0x2 to defenderSlot).
	const int isAttackerCfg = sConfig.GetIntDefault("Interlock.HudIsAttacker", -1);
	// round 0 (interlock start) carries no swing: 0x100 keeps the client from logging a fake exchange
	const bool opening = session.roundNumber == 0;

	DEBUG_LOG(format("IL round status round %1% (ex %2%): A %3% (tactic %4%, move %5%, dealt %6%) B %7% (tactic %8%, move %9%, dealt %10%) lock +%11%ms")
		% session.roundNumber % session.exchangeNum % pA->getHandle() % uint32(session.committedTacticA) % session.roundMoveA % dmgByA
		% pB->getHandle() % uint32(session.committedTacticB) % session.roundMoveB % dmgByB % msUntilLock);

	for (int side = 0; side < 2; side++)
	{
		PlayerObject* viewer = side == 0 ? pA : pB;
		const uint32 otherRef = side == 0 ? session.otherRefA : session.otherRefB;
		if (viewer->getClient().isBot() || otherRef == 0)
			continue;
		const int dealt = side == 0 ? dmgByA : dmgByB;
		const int taken = side == 0 ? dmgByB : dmgByA;
		const uint32 exAttackerGoId = session.lastExchangeAttackerGoId != 0 ? session.lastExchangeAttackerGoId : session.goIdA;
		const bool viewerIsAttacker = isAttackerCfg < 0 ? (viewer->getGoId() == exAttackerGoId) : (isAttackerCfg != 0);
		ILRoundStatus st;
		st.turn = uint16(session.roundNumber);
		// client.dll 0x621a199f compares st.round (@3 u16 CombatRound) against [ebx+0x18] (ILExchange::number)
		st.round = uint16(session.exchangeNum > 0 ? session.exchangeNum : (session.roundNumber + 1));
		st.isAttacker = viewerIsAttacker;
		st.player = BuildILCard(side == 0 ? session.committedTacticA : session.committedTacticB,
			side == 0 ? session.roundMoveA : session.roundMoveB, dealt);
		st.opponent = BuildILCard(side == 0 ? session.committedTacticB : session.committedTacticA,
			side == 0 ? session.roundMoveB : session.roundMoveA, taken);
		st.opponentHandle = otherRef;
		if (opening)
			st.outcomeFlag = 0x100;
		else if (viewerIsAttacker)
			st.outcomeFlag = (dealt > 0 ? 0x1u : 0u) | (taken > 0 ? 0x2u : 0u);
		else
			st.outcomeFlag = (taken > 0 ? 0x1u : 0u) | (dealt > 0 ? 0x2u : 0u);
		st.damageTaken = int16(std::max(0, std::min(taken, 32767)));
		st.opponentDamageTaken = int16(std::max(0, std::min(dealt, 32767)));
		st.lockTimeMs = lockMs;
		viewer->getClient().QueueCommand(std::make_shared<ILRoundStatusMsg>(st));
	}
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
			const int hpA0 = int(pA->getCurrentHealth());
			const int hpB0 = int(pB->getCurrentHealth());
			keepAlive = RunInterlockRound(*it);
			if (it->withdrewGoId != 0)
			{
				// CR2 withdraw succeeded: same grace as an instant withdraw
				const int graceMs = sConfig.GetIntDefault("Interlock.WithdrawGraceMs", 8000);
				if (graceMs > 0)
					m_withdrawGraceUntil[it->withdrewGoId] = getMSTime() + uint32(graceMs);
				keepAlive = false;
			}

			// [Item 13] Bullet Time / Dilation Zones
			float avgDilation = (pA->GetTimeDilation() + pB->GetTimeDilation()) / 2.0f;
			if (avgDilation <= 0.1f) avgDilation = 0.1f; // Prevent infinite division

			it->nextRoundTime = currTime + uint32((INTERLOCK_ROUND_SECONDS * 1000.0f) / avgDilation);
			if (hpA0 > 0 && hpB0 > 0) {
				it->roundNumber++;
				SendILRoundStatus(*it, pA, pB, hpB0 - int(pB->getCurrentHealth()),
					hpA0 - int(pA->getCurrentHealth()), it->nextRoundTime - currTime);
			}
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
            float shotInterval = FREEFIRE_SHOT_SECONDS;
            if (pA) {
                auto weapons = pA->getEquippedWeapons();
                if (!weapons.empty()) {
                    const ItemTemplate* wpnTpl = sDataLoader.GetItemTemplate(weapons[0]->getTemplateId());
                    if (wpnTpl && wpnTpl->type == ITEM_TYPE_WEAPON && wpnTpl->attackSpeed > 0.1f) {
                        shotInterval = wpnTpl->attackSpeed;
                    }
                }
            }
            float dilation = pA ? pA->GetTimeDilation() : 1.0f;
            if (dilation <= 0.1f) dilation = 0.1f;
			it->nextShotTime = currTime + uint32((shotInterval * 1000.0f) / dilation);
		}
		if (!keepAlive) {
			PlayerObject* pA = getPlayerSafe(it->attackerGoId);
			if (pA) {
				pA->setCombatStance(false);
				if (pA->getInventory()) pA->getInventory()->saveToDB();
				pA->saveDataToDB();
			}
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
	// Threat heatmap escalation. Not from the original game: every hit adds heat, and about two
	// hits into any fight it spawned Transit Police, then SWAT, then Agent teams with server-wide
	// alerts (bot-vs-bot fights fed it too). Off unless World.ThreatEscalation = 1.
	static uint32 lastHeatmapTickMs = 0;
	static const bool s_threatEscalation = sConfig.GetBoolDefault("World.ThreatEscalation", false);
	if (s_threatEscalation && currTime - lastHeatmapTickMs >= 1000) {
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

bool CombatSystem::CanAttack(PlayerObject* attacker, PlayerObject* target, std::string* why) const
{
	auto no = [why](const char* r) { if (why) *why = r; return false; };
	if (!attacker || !target) return no("No valid target.");
	if (attacker == target) return no("You can't attack yourself.");
	// Only human attackers are checked: bot AI picks its own enemies, and tester bots fight
	// whatever their missions send them at.
	if (attacker->getClient().isBot()) return true;
	if (!sConfig.GetBoolDefault("Combat.FactionRules", true)) return true;

	const std::string& tf = target->getFactionName();
	if (target->getClient().isBot())
	{
		if (tf == "HOSTILE") return true;              // mission targets
		if (attacker->m_lastDojoBotGoId == target->getGoId()) return true;
		if (tf == "FRIENDLY") return no("That is a friendly contact.");
		if (tf == "Civilian" || target->getHandle().find("Civilian") != std::string::npos)
			return no("You can't attack civilians.");
		if (target->getFaction() != FACTION_NONE && target->getFaction() == attacker->getFaction())
			return no("That is one of your own faction.");
		return true;
	}
	// player vs player: duel partners, or opposing factions
	if (attacker->isDueling() && attacker->getDuelPartner() == target->getGoId()) return true;
	if (target->getTrueFaction() == attacker->getTrueFaction())
		return no("You can only fight players of your own faction in a duel.");
	return true;
}

bool CombatSystem::RequestInterlock(uint32 attackerGoId, uint32 targetGoId, uint32 clientTargetRef)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	if (IsInterlocked(attackerGoId) || IsInterlocked(targetGoId)) {
		INFO_LOG(format("RequestInterlock %1% -> %2% refused: one side already interlocked") % attackerGoId % targetGoId);
		return false;
	}
	// A fighter who just withdrew can walk away: nobody else may lock them again during the
	// grace window. They may still start a new fight themselves (they are the attacker then).
	{
		auto g = m_withdrawGraceUntil.find(targetGoId);
		if (g != m_withdrawGraceUntil.end()) {
			const uint32 now = getMSTime();
			if (int32(g->second - now) > 0) {
				INFO_LOG(format("RequestInterlock %1% -> %2% refused: target withdrew, grace %3% ms left")
					% attackerGoId % targetGoId % (g->second - now));
				return false;
			}
			m_withdrawGraceUntil.erase(g);
		}
	}

	PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead()) {
		INFO_LOG(format("RequestInterlock %1% -> %2% refused: %3%") % attackerGoId % targetGoId
			% ((!pA || !pB) ? "attacker or target not found" : "attacker or target is dead"));
		return false;
	}
	{
		std::string why;
		if (!CanAttack(pA, pB, &why)) {
			INFO_LOG(format("RequestInterlock %1% -> %2% refused: %3%") % attackerGoId % targetGoId % why);
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}" + why + "{/c}"));
			return false;
		}
	}
	{
		float interlockDist = pA->getPosition().Distance(pB->getPosition());
		if (interlockDist > 1500.0f) {
			INFO_LOG(format("RequestInterlock %1% -> %2% refused: distance %3% > 1500 units") % attackerGoId % targetGoId % interlockDist);
			return false;
		}
	}

	// Retail Evade Combat: an evading target's shield soaks interlock attempts until it breaks
	if (EvadeBlocksInterlock(pA, pB)) {
		INFO_LOG(format("RequestInterlock %1% -> %2% refused: target evaded (shield %3%)") % attackerGoId % targetGoId % uint32(pB->getEvadeShield()));
		return false;
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
	session.tacticA = pA->getTactic();
	session.tacticB = pB->getTactic();
	// Bots predict from last round. Seeding this with the live stance would let them counter it immediately.
	session.committedTacticA = TACTIC_NORMAL;
	session.committedTacticB = TACTIC_NORMAL;
	session.queuedMoveA = 0;
	session.queuedMoveB = 0;
	session.nextRoundTime = getMSTime() + uint32(INTERLOCK_ROUND_SECONDS * 1000.0f);
	session.roundNumber = 0;
	session.ilViewIdA = 0;
	session.ilViewIdB = 0;
	session.exchangeNum = 1; //exchange 1 is the opening sent with the IL state below
	session.lastExchangeAttackerGoId = attackerGoId;

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

	posA.rot = std::atan2(posB.x - posA.x, posB.z - posA.z);
	posB.rot = std::atan2(posA.x - posB.x, posA.z - posB.z);
	// A bot spawned on a different floor made the exchange play in the air.
	// The attacker is standing on the floor the fight was started from.
	posB.y = posA.y;
	pA->setPosition(posA);
	pB->setPosition(posB);

	// PositionStateMsg does not move the local avatar. The same materialize packet a
	// hardline uses does, so the client starts the fight at the snapped melee range
	// instead of sending its old position back and pulling the pair apart.
	if (!pA->getClient().isBot())
		pA->getClient().QueueState(std::make_shared<JackoutEffectMsg>(pA->getGoId(), false));
	if (!pB->getClient().isBot())
		pB->getClient().QueueState(std::make_shared<JackoutEffectMsg>(pB->getGoId(), false));

	sSpatialGrid.UpdateClientPosition(&pA->getClient(), (float)posA.x, (float)posA.z);
	sSpatialGrid.UpdateClientPosition(&pB->getClient(), (float)posB.x, (float)posB.z);

	sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<PositionStateMsg>(pA->getGoId()));
	sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<RotationStateMsg>(pA->getGoId(), posA.getMxoRot()));
	sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<LocomotionStateMsg>(pA->getGoId(), 0, posA.getMxoRot()));
	sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<PositionStateMsg>(pB->getGoId()));
	sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<RotationStateMsg>(pB->getGoId(), posB.getMxoRot()));
	sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<LocomotionStateMsg>(pB->getGoId(), 0, posB.getMxoRot()));
	pA->getClient().QueueState(std::make_shared<PositionStateMsg>(pA->getGoId()));
	pA->getClient().QueueState(std::make_shared<RotationStateMsg>(pA->getGoId(), posA.getMxoRot()));
	pA->getClient().QueueState(std::make_shared<LocomotionStateMsg>(pA->getGoId(), 0, posA.getMxoRot()));
	pA->getClient().QueueState(std::make_shared<PositionStateMsg>(pB->getGoId()));
	pA->getClient().QueueState(std::make_shared<RotationStateMsg>(pB->getGoId(), posB.getMxoRot()));
	pA->getClient().QueueState(std::make_shared<LocomotionStateMsg>(pB->getGoId(), 0, posB.getMxoRot()));
	if (!pB->getClient().isBot())
	{
		pB->getClient().QueueState(std::make_shared<PositionStateMsg>(pA->getGoId()));
		pB->getClient().QueueState(std::make_shared<RotationStateMsg>(pA->getGoId(), posA.getMxoRot()));
		pB->getClient().QueueState(std::make_shared<LocomotionStateMsg>(pA->getGoId(), 0, posA.getMxoRot()));
		pB->getClient().QueueState(std::make_shared<PositionStateMsg>(pB->getGoId()));
		pB->getClient().QueueState(std::make_shared<RotationStateMsg>(pB->getGoId(), posB.getMxoRot()));
		pB->getClient().QueueState(std::make_shared<LocomotionStateMsg>(pB->getGoId(), 0, posB.getMxoRot()));
	}

	//spawn the ILCombatHandler view + pairing packet on both clients - this
	//drives the client-side interlock camera and round UI
	{
		float simTime = sGame.GetSimTime();
		LocationVector ilPos(midX, (posA.y + posB.y) * 0.5, midZ);
		session.ilPos = ilPos;
		session.nextRoundTime = getMSTime() + 2500; //first resolved round right after the opening

		// CombatantMode (Component 0x11) must be active on both combatants BEFORE
		// ILCombatStateMsg is queued, satisfying the client-side exchange player gating check.
		pA->enterInterlock(targetGoId);
		pB->enterInterlock(attackerGoId);
		if (!pA->getClient().isBot())
		{
			pA->getClient().QueueState(std::make_shared<CombatantModeMsg>(pB->getGoId(), 1));
		}
		if (!pB->getClient().isBot())
		{
			pB->getClient().QueueState(std::make_shared<CombatantModeMsg>(pA->getGoId(), 1));
		}
		sGame.AnnounceStateUpdateNear(posA.x, posA.z, 20000.0f, std::make_shared<CombatantModeMsg>(pA->getGoId(), 1));
		sGame.AnnounceStateUpdateNear(posB.x, posB.z, 20000.0f, std::make_shared<CombatantModeMsg>(pB->getGoId(), 1));
		INFO_LOG(format("Interlock started: %1% (%2%) vs %3% (%4%)") % attackerGoId % pA->getHandle() % targetGoId % pB->getHandle());
		if (PedestrianEcology::getSingletonPtr()) {
			sPedestrianEcology.SpreadRumorFearAura((float)posA.x, (float)posA.z, 0.40f, 3500.0f);
		}

		struct SideSetup { PlayerObject* self; PlayerObject* other; uint16* viewSlot; };
		SideSetup sides[2] =
		{
			{ pA, pB, &session.ilViewIdA },
			{ pB, pA, &session.ilViewIdB },
		};

		static std::atomic<uint32> s_openingExchangeSeed(0);
		uint32 openVariant = (++s_openingExchangeSeed * 17) + (rand() % 13);
		uint32 openMove = 0;
		InterlockAnimPair openPair;
		if (sConfig.GetBoolDefault("Interlock.UseDynamicILDB", true))
		{
			const ILDBMoveRecord* openRec = CombatAnimationMatrix::FindMoveVariant(
				pA->getFightingStyle(), session.tacticA,
				pB->getFightingStyle(), session.tacticB,
				InterlockExchangeOutcome::NormalHit, false, openVariant);
			if (openRec && openRec->moveId != 0)
			{
				openMove = openRec->moveId;
				openPair.attackerAnimId = openRec->aggrAnim;
				openPair.defenderAnimId = openRec->defeAnim;
				openPair.contactDelaySeconds = (openRec->aggrDur > 0) ? ((float)openRec->aggrDur / 1000.0f) : 0.50f;
			}
		}
		if (openMove == 0)
		{
			openMove = SelectInterlockMove(session, pA, pB, InterlockExchangeOutcome::NormalHit);
			openPair = CombatAnimationMatrix::GetDynamicAnimationPair(
				pA->getFightingStyle(), session.tacticA,
				pB->getFightingStyle(), session.tacticB,
				InterlockExchangeOutcome::NormalHit, false
			);
		}
		INFO_LOG(format("Interlock exchange 1: %1% (style %2%) -> %3% (style %4%) move 0x%5$04X (contact %6%ms, outcome 1, HP %7% / %8%)")
			% pA->getHandle() % (uint32)pA->getFightingStyle()
			% pB->getHandle() % (uint32)pB->getFightingStyle() % openMove
			% int32(openPair.contactDelaySeconds * 1000.0f)
			% pA->getCurrentHealth() % pB->getCurrentHealth());

		for (int i = 0; i < 2; i++)
		{
			try
			{
				if (sides[i].self->getClient().isBot())
					continue; //bots have no client to drive

				//combat mode on - exact HDS capture (CombatHandler.ProcessRequestCloseCombat)
				sides[i].self->getClient().QueueState(std::make_shared<SelfCombatantModeMsg>(1));
				sides[i].self->getClient().QueueState(std::make_shared<SelfCombatModeOnMsg>());
				sides[i].self->getClient().QueueState(std::make_shared<CombatantModeMsg>(sides[i].other->getGoId(), 1));

				uint16 ilViewId = sObjMgr.allocateDynamicView(&sides[i].self->getClient(), uint32(GetILCombatHandlerGoId()) << 16);
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
					{
						if ((clientTargetRef >> 16) == 0)
							clientTargetRef |= (uint32(PLAYER_SPAWN_COUNTER) << 16);
						otherViewWithSpawnId = clientTargetRef;
					}
					if (i == 0) session.otherRefA = otherViewWithSpawnId;
					else session.otherRefB = otherViewWithSpawnId;
					// point the client's combat target / target window at the opponent
					if (sConfig.GetBoolDefault("Interlock.HudRpcs", true))
						sides[i].self->getClient().QueueCommand(std::make_shared<CombatTargetMsg>(otherViewWithSpawnId));
					std::vector<uint32> slots = BuildILSlots(otherViewWithSpawnId);
					std::vector<ILExchange> opening;
					opening.push_back(BuildExchange(session, sides[i].self, pA, pB, 1,
						0, 0, openMove, &openPair));
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
	// round 0 status: no damage yet, tactics open until the first round resolves
	{
		uint32 now = getMSTime();
		SendILRoundStatus(session, pA, pB, 0, 0, session.nextRoundTime > now ? session.nextRoundTime - now : 0);
	}
	return true;
}

bool CombatSystem::RequestRangedCombat(uint32 attackerGoId, uint32 targetGoId, uint16 moveId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	if (IsInterlocked(attackerGoId)) return false; //no shooting out of an interlock
	// Already firing: switch target and/or fire the requested special next. This used to fail
	// ("Can't engage that target"), so a player had to withdraw to change target, and every
	// ranged ability used while firing was silently dropped (audit 2026-10-13).
	if (IsFreeFiring(attackerGoId))
	{
		PlayerObject* tgt = getPlayerSafe(targetGoId);
		if (!tgt || tgt->isDead() || tgt->getCurrentHealth() == 0) return false;
		if (!CanAttack(getPlayerSafe(attackerGoId), tgt)) return false;
		for (auto& ff : m_freefires)
		{
			if (ff.attackerGoId != attackerGoId) continue;
			ff.targetGoId = targetGoId;
			ff.moveId = moveId;
			ff.outOfRangeShots = 0;
		}
		return true;
	}

	PlayerObject* pA = getPlayerSafe(attackerGoId);
	PlayerObject* pB = getPlayerSafe(targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead()) return false;
	{
		std::string why;
		if (!CanAttack(pA, pB, &why)) {
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}" + why + "{/c}"));
			return false;
		}
	}

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
	state.nextShotTime = getMSTime();
	state.shotCount = 0;

	m_freefires.push_back(state);
	pA->setCombatStance(true);
	if (!pA->getClient().isBot())
	{
		pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
			(format("{c:FFFF00}[COMBAT] Firing on %1%.{/c}") % pB->getHandle()).str()));
	}
	if (pB->getClient().isBot()) {
		BotClient* botB = dynamic_cast<BotClient*>(&pB->getClient());
		if (botB && botB->GetTargetGoId() == 0) {
			botB->SetTargetGoId(attackerGoId);
		}
		// A second bot of that faction within about 25 m joins
		auto nearbyAllies = sSpatialGrid.GetClientsInRadius(pB->getPosition().x, pB->getPosition().z, 2500.0f);
		for (GameClient* gc : nearbyAllies) {
			if (!gc || gc == &pB->getClient() || !gc->isBot()) continue;
			BotClient* allyBot = dynamic_cast<BotClient*>(gc);
			PlayerObject* allyPo = BotGetPlayer(gc->GetPlayerGoId());
			if (allyBot && allyPo && !allyPo->isDead() && allyPo->getFaction() == pB->getFaction()) {
				if (allyBot->GetTargetGoId() == 0 && allyPo->getInterlockPartner() == 0) {
					allyBot->SetTargetGoId(attackerGoId);
					break;
				}
			}
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
			if (pA) {
				pA->setCombatStance(false);
				if (pA->getInventory()) pA->getInventory()->saveToDB();
				pA->saveDataToDB();
			}
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
		if (byWithdraw) {
			// Interlock.WithdrawGraceMs (default 8000, 0 = off): see m_withdrawGraceUntil
			const int graceMs = sConfig.GetIntDefault("Interlock.WithdrawGraceMs", 8000);
			if (graceMs > 0)
				m_withdrawGraceUntil[goId] = getMSTime() + uint32(graceMs);
		}

		// The 1.5x "parting shot" on withdraw is an emulator invention (Combat.EmulatorExtras).
		if (byWithdraw && pA && pB && !pA->isDead() && !pB->isDead() && sConfig.GetBoolDefault("Combat.EmulatorExtras", false)) {
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

    const bool isDojoBout = (attacker->m_lastDojoBotGoId != 0 || target->m_lastDojoBotGoId != 0 ||
                             attacker->m_dojoOwnerGoId != 0 || target->m_dojoOwnerGoId != 0 ||
                             attacker->getHandle().find("Dojo") != std::string::npos || target->getHandle().find("Dojo") != std::string::npos ||
                             attacker->getHandle().find("Sparring") != std::string::npos || target->getHandle().find("Sparring") != std::string::npos);
    const bool recordHeat = (GameServer::getSingletonPtr() == nullptr) ||
                            (!attacker->getClient().isBot() && !isDojoBout && sConfig.GetBoolDefault("World.ThreatEscalation", false));

    // Firearms & Ballistics line-of-sight validation (prevent shooting through buildings)
    if (move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC) {
        LocationVector aPos = attacker->getPosition();
        LocationVector tPos = target->getPosition();
        bool isDojo = isDojoBout;
        // Check line of sight through static building geometry at chest height (+100.0f)
        bool hasLOS = isDojo || sStaticObjMgr.CheckLineOfSight(aPos.x, aPos.y + 100.0f, aPos.z, tPos.x, tPos.y + 100.0f, tPos.z);
        if (!hasLOS) {
            res.hit = false;
            if (recordHeat) {
                sMatrixThreatHeatmap.RecordDisruption((float)aPos.x, (float)aPos.y, (float)aPos.z, 5.0f, "Ballistic Impact on Building Cover");
            }
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
        attacker->setMaskFaction(target->getFaction(), 300000); // 5 minutes
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

    // Emulator extras with no retail source (Agent 30% auto-dodge and 50% damage cut, dual-wield 1.5x)
    // stay off unless Combat.EmulatorExtras = 1. Retail Agents were tough through their level and stats.
    const bool emuExtras = sConfig.GetBoolDefault("Combat.EmulatorExtras", false);
    if (emuExtras && target->getClient().isBot() && target->getHandle().find("Agent") != std::string::npos && target->getHandle().find("Dojo") == std::string::npos && (rand() % 100 < 30)) {
        res.hit = false;
        if (recordHeat) {
            sMatrixThreatHeatmap.RecordDisruption((float)target->getPosition().x, (float)target->getPosition().y, (float)target->getPosition().z, 15.0f, "Agent Wire-Fu Dodge");
        }
        return res; // Agent Dodge
    }
    
	// Authentic hit resolution: attack roll vs defense roll (d100 + level accuracy)
	bool isBallistic = (move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC);
	if (isBallistic)
	{
		int attackRoll = (rand() % 100) + int(attacker->getLevel()) * 2 + int(attacker->getPerception() / 2);
		int defenseRoll = (rand() % 100) + int(target->getLevel()) * 2 + int(target->getEvasion() / 2);
		if (targetTactic == TACTIC_DEFENSE)
			defenseRoll += 25; // Bullet deflection stance bonus

		if (defenseRoll >= attackRoll)
		{
			if (target->getEvadeShield() > 0)
			{
				// BULLET DODGE SUCCESSFUL!
				res.hit = false;
				res.damageTaken = 0;
				res.isBlocked = true;

				uint8 shieldDrain = (uint8)(15 + (rand() % 11)); // 15-25 points
				target->consumeEvadeShield(shieldDrain);

				if (target->getCurrentIS() >= 10)
				{
					target->setCurrentIS(target->getCurrentIS() - 10);
				}

				// Authentic deflection FX 0x2800059c (FX_CHARACTER_DEFLECTION)
				static const uint32 s_cfgDeflectHitFx = (uint32)sConfig.GetIntDefault("Combat.HitFx", 0);
				const uint32 deflectFx = (s_cfgDeflectHitFx != 0) ? 0x2800059c : 0;
				// Authentic dodge animation (0x03EC = G_D_XR_DodgeFromHF_GLb)
				target->setCurrentAnimation(0xEC);
				target->setCurrentMood(0x03);
				sGame.AnnounceStateUpdate(&target->getClient(), std::make_shared<AnimationStateMsg>(target->getGoId()));

				if (!target->getClient().isBot())
				{
					target->getClient().QueueState(std::make_shared<SelfHitFxMsg>(target, deflectFx, target->getHitCounter()));
				}
				sGame.AnnounceStateUpdate(&target->getClient(), std::make_shared<CombatHitFxMsg>(target->getGoId(), deflectFx, target->nextHitCounter()));

				if (recordHeat) {
					sMatrixThreatHeatmap.RecordDisruption((float)target->getPosition().x, (float)target->getPosition().y, (float)target->getPosition().z, 15.0f, "Bullet Dodge");
				}

				if (!target->getClient().isBot())
				{
					target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
						(format("{c:00FFFF}[BULLET DODGE] You dodged incoming gunfire! Evade Shield: %1%%%, -10 IS.{/c}") % uint32(target->getEvadeShield())).str()
					));
				}
				if (!attacker->getClient().isBot())
				{
					attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
						(format("{c:FFFF00}[COMBAT] %1% dodged your gunfire!{/c}") % target->getHandle()).str()
					));
				}
				return res;
			}
			else
			{
				// Evade Shield is 0: unable to dodge!
				if (!target->getClient().isBot())
				{
					target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
						"{c:FF5555}[EVADE SHIELD] Evade Shield depleted! Unable to dodge gunfire!{/c}"
					));
				}
			}
		}
	}
	else
	{
		// Melee. A block is a hit that is reduced, not a miss.
		// Inside an interlock the stance exchange already decided this swing. A second
		// attack-vs-defense roll missed about half of those hits after the banner.
		bool isMeleeBlock = (targetTactic == TACTIC_DEFENSE && !bypassBlock && attackerTactic != TACTIC_RETALIATE);
		if (isMeleeBlock)
		{
			res.hit = true;
			res.isBlocked = true;
		}
		else if (inInterlock)
		{
			res.hit = true;
		}
		else
		{
			int attackRoll = (rand() % 100) + int(attacker->getLevel()) * 2;
			int defenseRoll = (rand() % 100) + int(target->getLevel()) * 2;
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
	}

	float dmg = move.minDmg + (move.maxDmg - move.minDmg) * ((rand() % 100) / 100.0f);
	dmg += move.minDmgPerLvl * attacker->getLevel();
	dmg *= attacker->getDamageScale(); //scaled dojo bots hit harder; everyone else is 1.0
	// Street mobs (server-driven, not Agents, below L30) hitting a player or tester hit lighter than a
	// player character of their level: L1 "Hostile" mission targets killed L1 testers twice per mission.
	if (IsServerDrivenBot(attacker) && !IsServerDrivenBot(target) && attacker->getLevel() < 30 &&
		attacker->getHandle().find("Agent") == std::string::npos && attacker->getHandle().find("Dojo") == std::string::npos)
		dmg *= sConfig.GetFloatDefault("Combat.StreetMobDamageScale", 0.8f);
	// CR2 interlock passes its stance/state multiplier in; everything else keeps the tactic matrix.
	dmg *= (m_cr2DamageMul > 0.0f) ? m_cr2DamageMul : TacticModifier(attackerTactic, targetTactic);
	// Same-move "adaptive" mitigation used to drop a repeated strike to 15% after four hits.
	// Interlock rounds reuse one style move, so every fight collapsed mid-way. Retail does not do this.

    // [Item 19] Dual Wielding Firepower
    if (emuExtras && move.dmgType == DAMAGE_RANGED && attacker->isDualWielding()) {
        dmg *= 1.5f; // 50% more damage for off-hand
    }

    // Phase 2: Gunner Ballistic Distance Drop-Off
    if (!inInterlock && (move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC) && move.range > 0.0f) {
        double dist = attacker->getPosition().Distance(target->getPosition());
        dmg *= ComputeBallisticDistanceMultiplier(dist, double(move.range), move.id);
    }

    if (recordHeat) {
        if (isBallistic) {
            sMatrixThreatHeatmap.RecordDisruption((float)attacker->getPosition().x, (float)attacker->getPosition().y, (float)attacker->getPosition().z, 8.0f, "Ballistic Fire");
        } else {
            sMatrixThreatHeatmap.RecordDisruption((float)attacker->getPosition().x, (float)attacker->getPosition().y, (float)attacker->getPosition().z, 12.0f, "Melee Interlock");
        }
    }

    if (emuExtras && target->getClient().isBot() && target->getHandle().find("Agent") != std::string::npos && target->getHandle().find("Dojo") == std::string::npos) {
        dmg *= 0.5f; // Agent Resilience (wild Agents only; Dojo sparring dummies take full damage)
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

	// Blocking halves what gets through and recovers Inner Strength.
	// Level mitigation is applied once, in takeDamage. Subtracting it here as well
	// made the hit line bigger than the health-bar drop.
	const bool grabBreaksPower = (attackerTactic == TACTIC_RETALIATE && targetTactic == TACTIC_POWER);
	if (targetTactic == TACTIC_DEFENSE)
	{
		if (bypassBlock || attackerTactic == TACTIC_RETALIATE)
		{
			if (!target->getClient().isBot())
				target->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[COMBAT] Your Block was broken by a Grab! Unblockable throw!{/c}"));
		}
		else
		{
			dmg *= 0.5f;
			target->restoreIS(5);
		}
	}

	if (dmg > 65535.0f) dmg = 65535.0f; //uint16 cast below
	const uint16 rawDamage = (uint16)dmg;
	const uint16 levelMit = (!grabBreaksPower && target->getHandle().find("Dojo") == std::string::npos)
		? uint16(target->getLevel() / 2) : 0;
	const uint16 expected = (rawDamage == 0) ? 0 : ((rawDamage > levelMit) ? uint16(rawDamage - levelMit) : 1);
	res.damageTaken = expected;
    
    // [Item 15] Melee Weapon Durability
    // retail: equipped weapons (guns too) wear down with use
    if (res.hit && (move.dmgType == DAMAGE_MELEE || move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC)) {
        attacker->degradeEquippedWeapon(1);
    }
    
    // Item 32: Takedown Moves & Interlock Animation Dispatch
    // Sparring duels resolve non-lethally inside takeDamage -> die() without a 3s death timer.
    const bool isSparringDuel = (target->isDueling() && target->getDuelPartner() == attacker->getGoId());
    if (res.hit && !isSparringDuel && target->getCurrentHealth() <= res.damageTaken) {
        // arm the 3 s takedown timer only once; a second lethal hit must not push death out
        if (target->m_deathDelayMS == 0)
        {
            target->m_deathDelayMS = getMSTime() + 3000; // 3 seconds takedown animation
            target->m_deathDelayKillerId = attacker->getGoId();
        }
    }

	if (res.hit)
	{
		// Synchronized combat animation subpacket trigger
		static const uint32 s_cfgHitFx = (uint32)sConfig.GetIntDefault("Combat.HitFx", 0);
		uint32 hitFx = s_cfgHitFx;
		if (res.isBlocked) {
			hitFx = (s_cfgHitFx != 0) ? ((move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC) ? 0x2800059c : 0x28000794) : 0; // FX_CHARACTER_DEFLECTION (0x2800059c) for ballistic / FX_CHARACTER_BLOCK_INTERLOCK (0x28000794)
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
		} else if (move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC) {
			hitFx = (s_cfgHitFx != 0) ? 0x280006ea : 0; // FX_BULLET_BODY_HITS_BULLETHIT_HF (authentic retail bullet impact)
		}
		uint16 hpBefore = target->getCurrentHealth();
		target->takeDamage(attacker->getGoId(), rawDamage, hitFx, !grabBreaksPower);
        target->recordIncomingAttack(move.id);
		uint16 hpAfter = target->getCurrentHealth();
		res.damageTaken = (hpBefore > hpAfter) ? uint16(hpBefore - hpAfter) : 0;

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
		// Threat heat / Agent intervention is not retail behaviour; it follows the same switch as the
		// heatmap update (World.ThreatEscalation, default off).
		if (sConfig.GetBoolDefault("World.ThreatEscalation", false) &&
			GameServer::getSingletonPtr() != nullptr && MatrixThreatHeatmap::getSingletonPtr() != nullptr &&
			!attacker->getClient().isBot() && target->getHandle().find("Dojo") == std::string::npos)
		{
			LocationVector aPos = attacker->getPosition();
			float heatAmt = (move.dmgType == DAMAGE_RANGED || move.dmgType == DAMAGE_BALLISTIC) ? 8.0f : 5.0f;
			sMatrixThreatHeatmap.RecordDisruption((float)aPos.x, (float)aPos.y, (float)aPos.z, heatAmt, move.name);
			if (target->getHandle().find("Agent") == std::string::npos)
			{
				sMatrixThreatHeatmap.CheckAndTriggerIntervention((float)aPos.x, (float)aPos.y, (float)aPos.z, attacker->getGoId());
			}
		}
	}
	return res;
}

float CombatSystem::ComputeBallisticDistanceMultiplier(double dist, double maxRange, uint16 abilityId)
{
	if (maxRange <= 0.0)
		maxRange = 3500.0;
	// SniperShotAbility (505): Long-range precision scaling (+25% beyond 20m, -20% inside 5m)
	if (abilityId == 505)
	{
		if (dist >= 2000.0)
			return 1.25f;
		if (dist < 500.0)
			return 0.80f;
		return 1.00f;
	}
	// PistolPointBlankAbility (499): Close-quarters +15% bonus within 5m
	if (abilityId == 499 && dist <= 500.0)
	{
		return 1.15f;
	}
	// Optimal effective range up to 65% of maxRange: 100% damage
	double optimalCutoff = maxRange * 0.65;
	if (dist <= optimalCutoff)
		return 1.00f;
	if (dist >= maxRange)
		return 0.60f;
	// Smooth linear drop-off from 1.00x at 65% maxRange down to 0.60x at 100% maxRange
	double t = (dist - optimalCutoff) / (maxRange - optimalCutoff);
	return float(1.00 - 0.40 * t);
}

ILExchange CombatSystem::BuildExchange(const InterlockSession &session, PlayerObject* viewer,
	PlayerObject* attacker, PlayerObject* defender, uint16 number,
	uint32 attackerPreMove, uint32 defenderPreMove, uint32 mainMove,
	const InterlockAnimPair* animPair)
{
	ILExchange e;
	e.attackerSlot = ILSlotFor(attacker == viewer);
	e.defenderSlot = ILSlotFor(defender == viewer);
	LocationVector a = attacker->getPosition();
	LocationVector d = defender->getPosition();
	e.attackerPos[0] = float(a.x - session.ilPos.x);
	e.attackerPos[1] = float(a.y - session.ilPos.y);
	e.attackerPos[2] = float(a.z - session.ilPos.z);
	e.defenderPos[0] = float(d.x - session.ilPos.x);
	e.defenderPos[1] = float(d.y - session.ilPos.y);
	e.defenderPos[2] = float(d.z - session.ilPos.z);
	e.number = number;
	// HDS CombatHandler.cs L76 capture: startMs 0x113, timing pair 0x534 / 0x29A, style byte 0.
	// (L71/L73 carry 0x104-0x113, 0x534 / 0x535 and style 1 / 7.) The previous startMs of
	// 1000000 had no source.
	static const int s_startMs = sConfig.GetIntDefault("Interlock.ExchangeStartMs", 0x113);
	static const int s_durMs = sConfig.GetIntDefault("Interlock.ExchangeDurMs", 0x534);
	static const int s_contactMs = sConfig.GetIntDefault("Interlock.ExchangeContactMs", 0x29A);
	static const int s_styleByte = sConfig.GetIntDefault("Interlock.ExchangeStyleByte", 0);
	e.startMs = uint32(s_startMs);
	e.unk28 = uint16(s_durMs);
	e.unk2A = uint16(s_contactMs);

	// Dynamic contact frame timing synchronization (aligns blocks/dodges with strike impact)
	if (animPair && animPair->contactDelaySeconds > 0.05f)
	{
		e.defenderOffsetMs = int16(animPair->contactDelaySeconds * 1000.0f);
	}
	else
	{
		e.defenderOffsetMs = 666; // authentic default opening timing from captured exchanges
	}

	e.flags = 0x23;

	// -1 = old behaviour (attacker's FightingStyle enum); otherwise the fixed captured byte
	e.style = (s_styleByte < 0) ? (uint8)attacker->getFightingStyle() : uint8(s_styleByte);

	uint32 moveResolved = mainMove ? mainMove : IL_MOVE_OPEN_MAIN;
	// Interlock.MoveSet = capture: main move limited to ids seen in HDS captures
	// (0x2026 L76, 0x236D L73, 0x2367 L71) plus the retail finisher 0x4EE5.
	static const bool s_captureMoves = sConfig.GetStringDefault("Interlock.MoveSet", "ildb") == "capture";
	if (s_captureMoves && moveResolved != 0x2026 && moveResolved != 0x236D
		&& moveResolved != 0x2367 && moveResolved != 0x4EE5)
	{
		static const uint32 capMoves[] = { 0x2026, 0x236D, 0x2367 };
		moveResolved = capMoves[number % 3];
	}
	uint32 attMove = attackerPreMove;
	uint32 defMove = defenderPreMove;
	e.moves[0][0] = attMove; e.moves[0][1] = attMove ? IL_MOVE_DATABASE : 0;
	e.moves[1][0] = defMove; e.moves[1][1] = defMove ? IL_MOVE_DATABASE : 0;
	e.moves[2][0] = moveResolved; e.moves[2][1] = moveResolved ? IL_MOVE_DATABASE : 0;
	e.moves[3][0] = 0; e.moves[3][1] = 0;
	e.moves[4][0] = 0; e.moves[4][1] = 0;

	e.attackerHealth = attacker->getCurrentHealth();
	e.defenderHealth = defender->getCurrentHealth();
	return e;
}

uint32 CombatSystem::SelectInterlockMove(const InterlockSession &session, PlayerObject* attacker, PlayerObject* defender, InterlockExchangeOutcome outcome)
{
	if (!attacker || !defender)
		return 0x2026; // Authentic unarmed fallback (Kung Fu strike)

	if (outcome != InterlockExchangeOutcome::Blocked &&
	    outcome != InterlockExchangeOutcome::Dodged &&
	    (defender->isDead() || defender->getCurrentHealth() == 0))
		return 0x4EE5; // Double Overhead Smash / Ground Slam Finisher (retail capture)

	// Tactics belong to THIS swing's attacker, not "either fighter queued Grab".
	// A Speed interrupt used to animate as a throw because the defender's Grab was enough.
	uint8 attTac = TACTIC_NORMAL;
	uint8 defTac = TACTIC_NORMAL;
	uint8 attackerComboStep = 0;
	if (attacker->getGoId() == session.goIdA)
	{
		attTac = session.tacticA;
		defTac = session.tacticB;
		attackerComboStep = session.comboStepA;
	}
	else if (attacker->getGoId() == session.goIdB)
	{
		attTac = session.tacticB;
		defTac = session.tacticA;
		attackerComboStep = session.comboStepB;
	}

	FightingStyle style = attacker->getFightingStyle();
	if (attackerComboStep > 0 && style != FightingStyle::None && CombatAnimationMatrix::GetTotalMovesLoaded() > 0)
	{
		const ILDBMoveRecord* comboRec = CombatAnimationMatrix::FindMoveVariant(
			style, attTac, defender->getFightingStyle(), defTac,
			outcome, (attackerComboStep == 3), uint32(attackerComboStep) * 7 + session.exchangeNum);
		if (comboRec && comboRec->moveId != 0)
			return comboRec->moveId;
	}

	const bool fastInterrupt = outcome == InterlockExchangeOutcome::FastInterrupt
		|| (attTac == TACTIC_SPEED && defTac == TACTIC_RETALIATE);
	const bool guardBreak = outcome == InterlockExchangeOutcome::GuardBreak
		|| (attTac == TACTIC_RETALIATE && (defTac == TACTIC_DEFENSE || defTac == TACTIC_POWER));
	const bool stanceCrush = outcome == InterlockExchangeOutcome::StanceCrush
		|| (attTac == TACTIC_POWER && defTac == TACTIC_SPEED);

	if (fastInterrupt)
	{
		if (attacker->getFightingStyle() == FightingStyle::Karate)
			return 0x2388;
		return 0x2026;
	}
	if (guardBreak)
		return 0x236D; // Aikido Throw / Momentum Reversal (retail capture)
	if (stanceCrush)
		return 0x2367; // Hyperstrike / Stance Crush (retail capture)

	switch (style)
	{
		case FightingStyle::Karate:
			return 0x2388; // Karate High Kick / Thrust (retail capture)
		case FightingStyle::Aikido:
			return 0x236D; // Aikido Redirection (retail capture)
		case FightingStyle::KungFu:
			return (session.exchangeNum % 2 == 0) ? 0x2026 : 0x2367; // Kung Fu Palm / Dragon Strike
		case FightingStyle::None:
		default:
		{
			static const uint32 authenticMoves[] = { 0x2026, 0x2388, 0x236D, 0x2367 };
			return authenticMoves[session.exchangeNum % 4];
		}
	}
}

void CombatSystem::SendInterlockExchange(InterlockSession &session, PlayerObject* attacker, PlayerObject* defender,
	const AttackResult& res, const CombatMove& move, uint8 attackerTactic, uint8 defenderTactic)
{
	if (session.ilViewIdA == 0 && session.ilViewIdB == 0)
		return;

	session.exchangeNum++;
	session.lastExchangeAttackerGoId = attacker ? attacker->getGoId() : 0;

	// Determine exchange outcome for fluid contact synchronization
	InterlockExchangeOutcome outcome = InterlockExchangeOutcome::NormalHit;
	if (session.cr2Outcome >= 0)
	{
		outcome = InterlockExchangeOutcome(session.cr2Outcome);
	}
	else if (res.isBlocked)
	{
		outcome = InterlockExchangeOutcome::Blocked;
	}
	else if (!res.hit)
	{
		outcome = InterlockExchangeOutcome::Dodged;
	}
	else if (attackerTactic == defenderTactic && attackerTactic != TACTIC_NORMAL)
	{
		outcome = InterlockExchangeOutcome::Clash;
	}
	else if (attackerTactic == TACTIC_POWER && defenderTactic == TACTIC_SPEED)
	{
		outcome = InterlockExchangeOutcome::StanceCrush;
	}
	else if (attackerTactic == TACTIC_SPEED && defenderTactic == TACTIC_RETALIATE)
	{
		outcome = InterlockExchangeOutcome::FastInterrupt;
	}
	else if (attackerTactic == TACTIC_RETALIATE && (defenderTactic == TACTIC_DEFENSE || defenderTactic == TACTIC_POWER))
	{
		outcome = InterlockExchangeOutcome::GuardBreak;
	}
	else if (move.id != 0 && move.id != 1)
	{
		outcome = InterlockExchangeOutcome::SpecialHit;
	}

	uint8 attackerComboStep = (attacker->getGoId() == session.goIdA) ? session.comboStepA
		: ((attacker->getGoId() == session.goIdB) ? session.comboStepB : 0);
	bool isFinisher = outcome != InterlockExchangeOutcome::Blocked
		&& outcome != InterlockExchangeOutcome::Dodged
		&& (defender->isDead() || defender->getCurrentHealth() == 0 || attackerComboStep == 3);

	// The client plays whatever ILDB move id is in slot 2. The old hardcoded ids
	// (0x2026 and three others) are one generic clip, so a block looked like a punch.
	static const bool useDynamicILDB = sConfig.GetBoolDefault("Interlock.UseDynamicILDB", true);
	InterlockAnimPair pair;
	uint32 mainMove = 0;
	if (useDynamicILDB)
	{
		const ILDBMoveRecord* rec = CombatAnimationMatrix::FindMoveVariant(
			attacker->getFightingStyle(), attackerTactic,
			defender->getFightingStyle(), defenderTactic,
			outcome, isFinisher, session.exchangeNum + uint32(attackerComboStep) * 7);
		if (rec && rec->moveId != 0)
		{
			mainMove = rec->moveId;
			pair.attackerAnimId = rec->aggrAnim;
			pair.defenderAnimId = rec->defeAnim;
			pair.contactDelaySeconds = (rec->aggrDur > 0) ? ((float)rec->aggrDur / 1000.0f) : 0.50f;
			if (outcome == InterlockExchangeOutcome::Clash || outcome == InterlockExchangeOutcome::Blocked)
				pair.hitFxId = 0x28000794;
			else if (outcome == InterlockExchangeOutcome::Dodged)
				pair.hitFxId = 0;
			else if (outcome == InterlockExchangeOutcome::GuardBreak)
				pair.hitFxId = 0x28000432;
			else
				pair.hitFxId = 0x280006DF;
		}
	}
	if (mainMove == 0)
	{
		mainMove = SelectInterlockMove(session, attacker, defender, outcome);
		pair = CombatAnimationMatrix::GetAnimationPair(
			attacker->getFightingStyle(), attackerTactic,
			defender->getFightingStyle(), defenderTactic,
			outcome, move.id);
	}

	PlayerObject* pA = getPlayerSafe(session.goIdA);
	PlayerObject* pB = getPlayerSafe(session.goIdB);
	struct { PlayerObject* p; uint16 view; } sides[2] = { { pA, session.ilViewIdA }, { pB, session.ilViewIdB } };
	for (int i = 0; i < 2; i++)
	{
		if (!sides[i].p || sides[i].view == 0 || sides[i].p->getClient().isBot())
			continue;

		PlayerObject* opponent = (sides[i].p == pA) ? pB : pA;
		uint32 otherRef = (sides[i].p == pA) ? session.otherRefA : session.otherRefB;
		if (opponent && otherRef == 0)
		{
			uint16 otherViewId = sObjMgr.getViewForGO(&sides[i].p->getClient(), opponent->getGoId());
			otherRef = uint32(otherViewId) | (uint32(PLAYER_SPAWN_COUNTER) << 16);
		}
		std::vector<uint32> slots = BuildILSlots(otherRef);
		if (opponent)
			sides[i].p->getClient().QueueState(std::make_shared<CombatantModeMsg>(opponent->getGoId(), 1));

		std::vector<ILExchange> ex;
		ex.push_back(BuildExchange(session, sides[i].p, attacker, defender, session.exchangeNum,
			0, 0, mainMove, &pair));
		sides[i].p->getClient().QueueState(std::make_shared<ILCombatStateMsg>(
			sides[i].view, session.ilPos, session.exchangeNum, slots, ex));
	}
	INFO_LOG(format("Interlock exchange %1%: %2% (style %3%) -> %4% (style %5%) move 0x%6$04X (contact %7%ms, outcome %8%, HP %9% / %10%)")
		% session.exchangeNum % attacker->getHandle() % (uint32)attacker->getFightingStyle()
		% defender->getHandle() % (uint32)defender->getFightingStyle() % mainMove
		% int32(pair.contactDelaySeconds * 1000.0f) % (uint32)outcome
		% attacker->getCurrentHealth() % defender->getCurrentHealth());
}

CombatSystem::AttackResult CombatSystem::StrikeInterlock(InterlockSession &session, PlayerObject* attacker,
	PlayerObject* target, const CombatMove& move, uint8 attackerTactic, uint8 targetTactic, bool inInterlock, bool bypassBlock, bool emitExchange)
{
	const bool exchangeDriven = (session.ilViewIdA != 0 || session.ilViewIdB != 0);
	m_ilExchangeActive = exchangeDriven;
	AttackResult res = ResolveAttack(attacker, target, move, attackerTactic, targetTactic, inInterlock, bypassBlock);
	m_ilExchangeActive = false;

	if (attacker && target)
	{
		uint8* comboStep = nullptr;
		FightingStyle* lastStyle = nullptr;
		if (attacker->getGoId() == session.goIdA)
		{
			comboStep = &session.comboStepA;
			lastStyle = &session.lastStyleA;
		}
		else if (attacker->getGoId() == session.goIdB)
		{
			comboStep = &session.comboStepB;
			lastStyle = &session.lastStyleB;
		}

		// The +20% "3-step combo finisher" is not a retail mechanic; the CR2 model leaves it out.
		if (comboStep && lastStyle && !UseCR2Interlock())
		{
			FightingStyle curStyle = attacker->getFightingStyle();
			if (res.hit && !res.isBlocked && curStyle != FightingStyle::None)
			{
				if (*lastStyle != curStyle || *comboStep >= 3)
					*comboStep = 1;
				else
					(*comboStep)++;
				*lastStyle = curStyle;

				if (*comboStep == 3 && res.damageTaken > 0)
				{
					uint16 bonusDmg = std::max<uint16>(1, uint16(res.damageTaken / 5)); // +20% 3-step combo finisher bonus
					if (!target->isDead() && target->getCurrentHealth() > 0)
					{
						uint16 hpBeforeBonus = target->getCurrentHealth();
						target->takeDamage(attacker->getGoId(), bonusDmg, 0, false);
						uint16 hpAfterBonus = target->getCurrentHealth();
						res.damageTaken += (hpBeforeBonus > hpAfterBonus) ? uint16(hpBeforeBonus - hpAfterBonus) : bonusDmg;
					}
					else
					{
						res.damageTaken += bonusDmg;
					}
					if (!attacker->getClient().isBot())
					{
						attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
							"{c:00FF88}[MARTIAL ARTS] 3-Step Style Combo Finisher! (+20% damage){/c}"));
					}
				}
			}
			else
			{
				*comboStep = 0;
				*lastStyle = curStyle;
			}
		}
	}

	if (exchangeDriven && emitExchange)
		SendInterlockExchange(session, attacker, target, res, move, attackerTactic, targetTactic);
	return res;
}

// Tester bots (BotTester.cpp) choose their own tactics/moves through the client RPCs like a
// human does; the server-side bot AI below must not overwrite them.

static bool IsServerDrivenBot(PlayerObject* p)
{
	if (!p || !p->getClient().isBot()) return false;
	BotClient* b = dynamic_cast<BotClient*>(&p->getClient());
	return !(b && b->IsTester());
}

bool CombatSystem::RunInterlockRound(InterlockSession &session)
{
	PlayerObject* pA = getPlayerSafe(session.goIdA);
	PlayerObject* pB = getPlayerSafe(session.goIdB);
	if (!pA || !pB) return false;

	// The blow that emptied the bar already played. Further rounds were hitting a
	// body at 0 HP until the 3 s takedown timer called die(). Hold the interlock.
	if (pA->getCurrentHealth() == 0 || pB->getCurrentHealth() == 0)
		return true;

	// Interlock broken by movement without explicit withdraw
	float dist = float(pA->getPosition().Distance(pB->getPosition()));
	if (dist > 1500.0f) {
		if (!pA->getClient().isBot()) pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[SYSTEM] Interlock broken due to distance.{/c}"));
		if (!pB->getClient().isBot()) pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[SYSTEM] Interlock broken due to distance.{/c}"));
		return false; //Update() reaps the session and tears down the IL views
	}

	const CombatMove* moveA = (session.queuedMoveA != 0 && session.queuedMoveA != 1) ? GetMove(session.queuedMoveA) : nullptr;
	const CombatMove* moveB = (session.queuedMoveB != 0 && session.queuedMoveB != 1) ? GetMove(session.queuedMoveB) : nullptr;
	// A hotbar id that is not a combat move used to skip the swing entirely.
	if (!moveA) moveA = GetDefaultStyleMove(pA);
	if (!moveB) moveB = GetDefaultStyleMove(pB);

	// Humans and tester bots drive their own stance. Copy it every round so a &tactic
	// that landed on the player object cannot be left behind on Normal.
	if (!IsServerDrivenBot(pA))
		session.tacticA = (pA->getTactic() <= TACTIC_NORMAL) ? pA->getTactic() : uint8(TACTIC_NORMAL);
	if (!IsServerDrivenBot(pB))
		session.tacticB = (pB->getTactic() <= TACTIC_NORMAL) ? pB->getTactic() : uint8(TACTIC_NORMAL);

	auto selectBotRoundMove = [&](PlayerObject* bot, PlayerObject* opponent, const CombatMove*& chosenMove)
	{
		// Street mobs throw their faction special about one round in three and a plain style strike
		// otherwise (live 16:10-16:14: L1 "Hostile" mobs swung KungFu every round, 930 casts in 25 min,
		// and still killed L1 testers twice per mission). Agents and L30+ elites keep the full kit.
		const bool eliteBot = bot->getHandle().find("Agent") != std::string::npos || bot->getLevel() >= 30;
		if (!eliteBot && (rand() % 3) != 0)
		{
			chosenMove = GetDefaultStyleMove(bot);
			return;
		}
		uint32 faction = bot->getFaction();
		if (faction == FACTION_ZION)
		{
			const CombatMove* m = GetMove(133);
			// signature move only when it is a close-combat move and the Inner Strength is there;
			// Machines used to fire PowerShot (25-35, a gun) every interlock round for free and
			// L1 mission mobs out-damaged L1 operatives (BOTTEST "died twice to mission target")
			const uint16 cost = (m && m->isCost > 0) ? m->isCost : 10;
			if (m && !m->freefireOnly && m->dmgType != DAMAGE_RANGED && m->dmgType != DAMAGE_BALLISTIC && bot->spendIS(cost))
			{
				chosenMove = m;
				INFO_LOG(format("UseAbility: %1%:%2% uses %3% (requested id 133, move id 133) on target go %4% [interlocked=1 castTime=0]")
					% bot->getHandle() % bot->getGoId() % m->name % opponent->getGoId());
				return;
			}
		}
		else if (faction == FACTION_MACHINES)
		{
			const CombatMove* m = GetMove(14);
			// signature move only when it is a close-combat move and the Inner Strength is there;
			// Machines used to fire PowerShot (25-35, a gun) every interlock round for free and
			// L1 mission mobs out-damaged L1 operatives (BOTTEST "died twice to mission target")
			const uint16 cost = (m && m->isCost > 0) ? m->isCost : 15;
			if (m && !m->freefireOnly && m->dmgType != DAMAGE_RANGED && m->dmgType != DAMAGE_BALLISTIC && bot->spendIS(cost))
			{
				chosenMove = m;
				INFO_LOG(format("UseAbility: %1%:%2% uses %3% (requested id 14, move id 14) on target go %4% [interlocked=1 castTime=0]")
					% bot->getHandle() % bot->getGoId() % m->name % opponent->getGoId());
				return;
			}
		}
		else if (faction == FACTION_MEROVINGIAN)
		{
			bool hurt = (bot->getMaximumHealth() > 0) && (bot->getCurrentHealth() < (bot->getMaximumHealth() * 3 / 4));
			if (hurt)
			{
				const CombatMove* m = GetMove(5043);
				if (!m) m = GetMoveByName("Patch Health");
				uint16 cost = (m && m->isCost > 0) ? m->isCost : 25;
				// the heal needs the Inner Strength and restores at most 15% of the bar (a flat 50 on a
				// 72 HP L1 mob undid two rounds of damage: L1 operatives won 2 fights in 10 vs Merovingians)
				if (bot->spendIS(cost))
				{
					uint16 healAmt = (m && m->minDmg > 0.0f) ? (uint16)m->minDmg : 50;
					healAmt = std::min<uint16>(healAmt, std::max<uint16>(1, uint16(bot->getMaximumHealth() * 15 / 100)));
					bot->applyHeal(bot->getGoId(), healAmt);
					// the heal is the round: it used to be cast AND followed by a style strike, so a
					// Merovingian mob out-sustained L1 testers (BOTTEST deaths after 31b4457f)
					(bot == pA ? session.tacticA : session.tacticB) = uint8(TACTIC_DEFENSE);
					chosenMove = GetDefaultStyleMove(bot);
					INFO_LOG(format("UseAbility: %1%:%2% uses %3% (requested id 5043, move id 5043) on target go %4% [interlocked=1 castTime=0]")
						% bot->getHandle() % bot->getGoId() % (m ? m->name : "Restore") % opponent->getGoId());
					return;
				}
				INFO_LOG(format("UseAbility: %1%:%2% uses %3% (requested id 5043, move id 5043) on target go %4% [interlocked=1 castTime=0]")
					% bot->getHandle() % bot->getGoId() % (m ? m->name : "Restore") % opponent->getGoId());
			}
			else
			{
				const CombatMove* m = GetMove(146);
				if (!m) m = GetMoveByName("Poison knife");
				if (m && bot->spendIS((m->isCost > 0) ? m->isCost : 10))
				{
					chosenMove = m;
					INFO_LOG(format("UseAbility: %1%:%2% uses %3% (requested id 146, move id 146) on target go %4% [interlocked=1 castTime=0]")
						% bot->getHandle() % bot->getGoId() % m->name % opponent->getGoId());
					return;
				}
			}
		}

		if (bot->getFightingStyle() == FightingStyle::KungFu) {
			const uint16 kfMoves[] = { 133, 570, 574, 197, 198 };
			uint16 selected = kfMoves[rand() % 5];
			const CombatMove* m = GetMove(selected);
			if (m) chosenMove = m;
		} else if (bot->getFightingStyle() == FightingStyle::Karate) {
			const uint16 karateMoves[] = { 132, 569, 573, 531, 197 };
			uint16 selected = karateMoves[rand() % 5];
			const CombatMove* m = GetMove(selected);
			if (m) chosenMove = m;
		} else if (bot->getFightingStyle() == FightingStyle::Aikido) {
			const uint16 aikidoMoves[] = { 101, 296, 571, 572, 198 };
			uint16 selected = aikidoMoves[rand() % 5];
			const CombatMove* m = GetMove(selected);
			if (m) chosenMove = m;
		} else {
			chosenMove = GetDefaultStyleMove(bot);
		}
	};

	// Bot combat AI: update round tactics and discipline moves
	if (IsServerDrivenBot(pA)) {
		uint8 newTac = TACTIC_POWER;
		bool isElite = (pA->getHandle().find("Agent") != std::string::npos || (pA->getLevel() >= 30 && pA->getHandle().find("Dojo") == std::string::npos));
		if (UseCR2Interlock()) {
			const float hpFrac = pA->getMaximumHealth() ? float(pA->getCurrentHealth()) / float(pA->getMaximumHealth()) : 1.0f;
			newTac = CR2BotTactic(session.committedTacticB, isElite, session.combatStateA, hpFrac, uint32(rand()));
		} else if (isElite) {
			switch (session.committedTacticB) {
				case TACTIC_POWER:     newTac = TACTIC_RETALIATE; break;
				case TACTIC_SPEED:     newTac = TACTIC_POWER;     break;
				case TACTIC_RETALIATE: newTac = TACTIC_SPEED;     break;
				case TACTIC_DEFENSE:   newTac = TACTIC_RETALIATE; break;
				default:               newTac = (rand() % 2 == 0) ? TACTIC_POWER : TACTIC_SPEED; break;
			}
		} else {
			// Street NPCs press the attack; Block is the exception (a blocking NPC that never
			// swings made fights drag on). Weights: Speed 35, Power 35, Grab 20, Block 10.
			const uint8 thugTactics[] = { TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED,
				TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER,
				TACTIC_RETALIATE, TACTIC_RETALIATE, TACTIC_RETALIATE, TACTIC_RETALIATE, TACTIC_DEFENSE, TACTIC_DEFENSE };
			newTac = thugTactics[rand() % 20];
		}
		session.tacticA = newTac;

		if (session.queuedMoveA == 0 || session.queuedMoveA == 1) {
			selectBotRoundMove(pA, pB, moveA);
		}
	}
	if (IsServerDrivenBot(pB)) {
		uint8 newTac = TACTIC_POWER;
		bool isElite = (pB->getHandle().find("Agent") != std::string::npos || (pB->getLevel() >= 30 && pB->getHandle().find("Dojo") == std::string::npos));
		if (UseCR2Interlock()) {
			const float hpFrac = pB->getMaximumHealth() ? float(pB->getCurrentHealth()) / float(pB->getMaximumHealth()) : 1.0f;
			newTac = CR2BotTactic(session.committedTacticA, isElite, session.combatStateB, hpFrac, uint32(rand()));
		} else if (isElite) {
			switch (session.committedTacticA) {
				case TACTIC_POWER:     newTac = TACTIC_RETALIATE; break;
				case TACTIC_SPEED:     newTac = TACTIC_POWER;     break;
				case TACTIC_RETALIATE: newTac = TACTIC_SPEED;     break;
				case TACTIC_DEFENSE:   newTac = TACTIC_RETALIATE; break;
				default:               newTac = (rand() % 2 == 0) ? TACTIC_POWER : TACTIC_SPEED; break;
			}
		} else {
			// Street NPCs press the attack; Block is the exception (a blocking NPC that never
			// swings made fights drag on). Weights: Speed 35, Power 35, Grab 20, Block 10.
			const uint8 thugTactics[] = { TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED, TACTIC_SPEED,
				TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER, TACTIC_POWER,
				TACTIC_RETALIATE, TACTIC_RETALIATE, TACTIC_RETALIATE, TACTIC_RETALIATE, TACTIC_DEFENSE, TACTIC_DEFENSE };
			newTac = thugTactics[rand() % 20];
		}
		session.tacticB = newTac;

		if (session.queuedMoveB == 0 || session.queuedMoveB == 1) {
			selectBotRoundMove(pB, pA, moveB);
		}
	}

	uint8 tacA = session.tacticA;
	uint8 tacB = session.tacticB;
	session.roundMoveA = moveA ? uint16(moveA->id) : 0;
	session.roundMoveB = moveB ? uint16(moveB->id) : 0;

	const bool humanRound = !IsServerDrivenBot(pA) || !IsServerDrivenBot(pB);
	if (humanRound)
	{
		INFO_LOG(format("Interlock round %1% start: %2%:%3% (tactic %4%, move %5%, HP %6%/%7%) vs %8%:%9% (tactic %10%, move %11%, HP %12%/%13%)")
			% session.roundNumber
			% pA->getHandle() % pA->getGoId() % uint32(tacA) % (moveA ? moveA->name : std::string("<none>")) % pA->getCurrentHealth() % pA->getMaximumHealth()
			% pB->getHandle() % pB->getGoId() % uint32(tacB) % (moveB ? moveB->name : std::string("<none>")) % pB->getCurrentHealth() % pB->getMaximumHealth());
	}

	if (UseCR2Interlock())
	{
		RunCR2Exchange(session, pA, pB, moveA, moveB, tacA, tacB);
	}
	else
	{
	// Combat.InterlockModel = legacy: the emulator's earlier rock-paper-scissors rules (not retail).
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

		// Tactical banner announcements. Retail showed round outcomes in the interlock HUD and its combat
		// log (RPC 0x45, SendILRoundStatus), not as chat lines; Interlock.ChatBanners = 1 brings these back.
		const bool chatBanners = sConfig.GetBoolDefault("Interlock.ChatBanners", false);
		if (aCrushesB) {
			if (chatBanners && !pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Power crushes Speed! Frame advantage secured against %1%! (+35%% Damage){/c}") % pB->getHandle()).str()));
			if (chatBanners && !pB->getClient().isBot())
				pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Power stance crushed your Speed attack!{/c}") % pA->getHandle()).str()));
		} else if (bCrushesA) {
			if (chatBanners && !pB->getClient().isBot())
				pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Power crushes Speed! Frame advantage secured against %1%! (+35%% Damage){/c}") % pA->getHandle()).str()));
			if (chatBanners && !pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Power stance crushed your Speed attack!{/c}") % pB->getHandle()).str()));
		} else if (aInterruptsB) {
			if (chatBanners && !pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Speed interrupts Grab! Fast jab interrupted %1%'s grab maneuver! (+35%% Damage){/c}") % pB->getHandle()).str()));
			if (chatBanners && !pB->getClient().isBot())
				pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Speed strike interrupted your Grab maneuver!{/c}") % pA->getHandle()).str()));
		} else if (bInterruptsA) {
			if (chatBanners && !pB->getClient().isBot())
				pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Speed interrupts Grab! Fast jab interrupted %1%'s grab maneuver! (+35%% Damage){/c}") % pA->getHandle()).str()));
			if (chatBanners && !pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF4444}[MARTIAL ARTS] %1%'s Speed strike interrupted your Grab maneuver!{/c}") % pB->getHandle()).str()));
		} else if (aBreaksGuardB || aBreaksPowerB) {
			const char* targetStance = aBreaksGuardB ? "Guard" : "Power";
			if (chatBanners && !pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Grab breaks %1%! Unblockable throw executed on %2%! (+%3%%% Damage){/c}") % targetStance % pB->getHandle() % (aBreaksGuardB ? 40 : 35)).str()));
			if (chatBanners && !pB->getClient().isBot())
				pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF0000}[MARTIAL ARTS] Your %1% was broken by %2%'s Grab! Unblockable throw!{/c}") % targetStance % pA->getHandle()).str()));
		} else if (bBreaksGuardA || bBreaksPowerA) {
			const char* targetStance = bBreaksGuardA ? "Guard" : "Power";
			if (chatBanners && !pB->getClient().isBot())
				pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:00FF00}[MARTIAL ARTS] Grab breaks %1%! Unblockable throw executed on %2%! (+%3%%% Damage){/c}") % targetStance % pA->getHandle() % (bBreaksGuardA ? 40 : 35)).str()));
			if (chatBanners && !pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FF0000}[MARTIAL ARTS] Your %1% was broken by %2%'s Grab! Unblockable throw!{/c}") % targetStance % pB->getHandle()).str()));
		} else if (isClash) {
			// Both combatants selected identical stance - glancing exchange.
			// Interlocks animate exclusively through ILCombatStateMsg / ILExchange structures.
			std::string clashName = (tacA == TACTIC_POWER) ? "Power" : (tacA == TACTIC_SPEED ? "Speed" : (tacA == TACTIC_RETALIATE ? "Grab" : "Guard"));
			if (chatBanners && !pA->getClient().isBot()) pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FFFF00}[MARTIAL ARTS] Stance Clash! Both combatants chose %1%. Glancing exchange.{/c}") % clashName).str()));
			if (chatBanners && !pB->getClient().isBot()) pB->getClient().QueueCommand(std::make_shared<SystemChatMsg>((format("{c:FFFF00}[MARTIAL ARTS] Stance Clash! Both combatants chose %1%. Glancing exchange.{/c}") % clashName).str()));

			// Both combatants exchange glancing blows at 0.90x damage (TacticModifier handles 0.90x).
			// Emit one paired ILExchange per round so session.exchangeNum and ILRoundStatusMsg stay 1:1.
			AttackResult resA, resB;
			bool aSwung = false, bSwung = false;
			if (moveA) {
				resA = StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, false, false);
				aSwung = true;
			}
			// 0 HP is down even before the reaper sets the dead flag: no swing back from the knocked-out side
			if (moveB && !pB->isDead() && pB->getCurrentHealth() > 0) {
				resB = StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, false, false);
				bSwung = true;
			}
			if (session.ilViewIdA != 0 || session.ilViewIdB != 0) {
				if (bSwung && (pA->isDead() || pA->getCurrentHealth() == 0))
					SendInterlockExchange(session, pB, pA, resB, *moveB, tacB, tacA);
				else if (aSwung)
					SendInterlockExchange(session, pA, pB, resA, *moveA, tacA, tacB);
				else if (bSwung)
					SendInterlockExchange(session, pB, pA, resB, *moveB, tacB, tacA);
			}
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
			// Standard exchange: defenders only strike if an active special is queued.
			// Emit one paired ILExchange per round so session.exchangeNum and ILRoundStatusMsg stay 1:1.
			AttackResult resA, resB;
			bool aSwung = false, bSwung = false;
			bool specialFromA = (session.queuedMoveA != 0);
			if (tacA != TACTIC_DEFENSE || specialFromA) {
				if (moveA) {
					resA = StrikeInterlock(session, pA, pB, *moveA, tacA, tacB, true, false, false);
					aSwung = true;
				}
			}
			// B at 0 HP is knocked out even before the reaper sets the dead flag. It used to swing back
			// after the killing blow (live 10:58:09, exchange 148 from a 0 HP Dojo Street Brawler).
			if (!pB->isDead() && pB->getCurrentHealth() > 0) {
				bool specialFromB = (session.queuedMoveB != 0);
				if (tacB != TACTIC_DEFENSE || specialFromB) {
					if (moveB) {
						resB = StrikeInterlock(session, pB, pA, *moveB, tacB, tacA, true, false, false);
						bSwung = true;
					}
				}
			}
			if (session.ilViewIdA != 0 || session.ilViewIdB != 0) {
				if (bSwung && (pA->isDead() || pA->getCurrentHealth() == 0))
					SendInterlockExchange(session, pB, pA, resB, *moveB, tacB, tacA);
				else if (aSwung)
					SendInterlockExchange(session, pA, pB, resA, *moveA, tacA, tacB);
				else if (bSwung)
					SendInterlockExchange(session, pB, pA, resB, *moveB, tacB, tacA);
			}
		}

	}

	// CR2: a special queued while blocking is kept for the round the fighter stops blocking
	const bool keepQueued = UseCR2Interlock() && !sConfig.GetBoolDefault("Combat.CR2.BlockSpecials", false);
	if (!(keepQueued && tacA == TACTIC_DEFENSE)) session.queuedMoveA = 0;
	if (!(keepQueued && tacB == TACTIC_DEFENSE)) session.queuedMoveB = 0;
	session.committedTacticA = tacA;
	session.committedTacticB = tacB;

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

// ---------------------------------------------------------------------------------------------
// CR2 interlock (Combat Revamp 2, the model retail shipped from 2006 to shutdown).
// Sources: CR2 overview (files.mxoemu.info/CR2.pdf), the developer Q&As, the MxO archive combat
// page. Each round both fighters roll offense against the other's defense independently, so
// both, one or neither can land ("zero-sum outcomes were removed"). Stances shift the odds:
// Speed +accuracy -defense; Power +damage -accuracy; Grab +defense -damage; Block large defense,
// no basic attack, restores Inner Strength and shakes off states. A landed Speed hit leaves the
// target Dazed, Power Staggered, Grab Off-Balance. Level difference moves the odds.
// The exact retail numbers were server-side and never published: the values below are a
// reconstruction and every one can be tuned in Reality.conf (Combat.CR2.*).
// ---------------------------------------------------------------------------------------------
float CombatSystem::CR2WithdrawChance(PlayerObject* who, PlayerObject* opponent, uint8 opponentTactic)
{
	if (!who || !opponent) return 1.0f;
	float p = sConfig.GetFloatDefault("Combat.CR2.WithdrawBase", 0.50f);
	const float lvl = (float(who->getLevel()) - float(opponent->getLevel())) * sConfig.GetFloatDefault("Combat.CR2.LevelHit", 0.02f);
	p += std::max(-0.25f, std::min(0.25f, lvl));
	// a grabbing opponent holds on; a blocking one is not pressing the attack
	if (opponentTactic == TACTIC_RETALIATE) p -= sConfig.GetFloatDefault("Combat.CR2.WithdrawVsGrab", 0.15f);
	else if (opponentTactic == TACTIC_DEFENSE) p += sConfig.GetFloatDefault("Combat.CR2.WithdrawVsBlock", 0.20f);
	return std::max(0.10f, std::min(0.95f, p));
}

void CombatSystem::RequestWithdraw(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	InterlockSession* s = GetInterlockSession(goId);
	PlayerObject* other = s ? getPlayerSafe(s->goIdA == goId ? s->goIdB : s->goIdA) : nullptr;
	// legacy rules, contested withdraw switched off, or nobody left to hold on: leave at once
	if (!s || !UseCR2Interlock() || !sConfig.GetBoolDefault("Interlock.ContestedWithdraw", true) ||
		!other || other->isDead() || other->getCurrentHealth() == 0)
	{
		LeaveInterlock(goId);
		return;
	}
	if (s->goIdA == goId) s->withdrawA = true; else s->withdrawB = true;
	PlayerObject* me = getPlayerSafe(goId);
	if (me && !me->getClient().isBot())
		me->getClient().QueueCommand(std::make_shared<SystemChatMsg>("You attempt to withdraw..."));
}

// ---------------------------------------------------------------------------------------------
// Evade Combat (retail Awakened ability 217, EvadeCombatAbility). Sources: fandom wiki Evade_Combat
// and Combat_Basics, CR2.pdf. "A temporary shield to prevent entry into close combat"; ablative:
// every attempt to draw you in consumes part of it (and some of your Inner Strength) until it is
// gone; using the ability again drops it; a waiting period follows before it can be raised again.
// Combat Evasion (shield strength) vs the attacker's Force Combat and the level gap set the drain.
// Stance modifiers (Combat_Basics): Grab +100% Force / -65% Evasion, Block -400% Force / +60% Evasion.
// ---------------------------------------------------------------------------------------------
bool CombatSystem::ToggleEvadeCombat(PlayerObject* p)
{
	if (!p) return false;
	const bool human = !p->getClient().isBot();
	const uint32 now = getMSTime();
	const uint32 cooldown = uint32(sConfig.GetIntDefault("Combat.EvadeCooldownMs", 30000));
	if (p->isEvading())
	{
		p->setEvading(false);
		p->setEvadeCooldownUntil(now + cooldown);
		if (human) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>("You stop evading."));
		return true;
	}
	if (IsInterlocked(p->getGoId()))
	{
		if (human) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>("You cannot raise your evade shield in close combat."));
		return false;
	}
	if (int32(p->getEvadeCooldownUntil() - now) > 0)
	{
		if (human) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
			(format("Your evade shield can be raised again in %1% seconds.") % ((p->getEvadeCooldownUntil() - now + 999) / 1000)).str()));
		return false;
	}
	p->setEvading(true);
	p->setEvadeShield(100);
	if (human) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>("You are evading close combat."));
	return true;
}

bool CombatSystem::EvadeBlocksInterlock(PlayerObject* attacker, PlayerObject* target)
{
	if (!attacker || !target || !target->isEvading() || target->getEvadeShield() == 0)
		return false;
	// Force Combat of the attacker (Grab +100%, Block -400% = cannot force at all)
	float force = 1.0f;
	if (attacker->getTactic() == TACTIC_RETALIATE) force = 2.0f;
	else if (attacker->getTactic() == TACTIC_DEFENSE) force = 0.0f;
	// Combat Evasion of the target (Grab -65%, Block +60%)
	float evasion = 1.0f;
	if (target->getTactic() == TACTIC_RETALIATE) evasion = 0.35f;
	else if (target->getTactic() == TACTIC_DEFENSE) evasion = 1.6f;
	float lvl = 1.0f + (float(attacker->getLevel()) - float(target->getLevel())) * 0.05f;
	lvl = std::max(0.5f, std::min(2.0f, lvl));
	const float base = sConfig.GetFloatDefault("Combat.EvadeDrain", 34.0f);
	const int drain = int(base * force * lvl / evasion + 0.5f);
	const uint8 before = target->getEvadeShield();
	target->consumeEvadeShield(uint8(std::min(255, std::max(0, drain))));
	const uint16 isCost = uint16(sConfig.GetIntDefault("Combat.EvadeISCost", 5));
	if (target->getCurrentIS() >= isCost) target->setCurrentIS(target->getCurrentIS() - isCost);
	const bool human = !target->getClient().isBot();
	if (target->getEvadeShield() > 0)
	{
		if (!attacker->getClient().isBot())
			attacker->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				(format("%1% evades your attempt to engage.") % target->getHandle()).str()));
		if (human)
			target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				(format("You evade %1%. Evade shield %2%%%.") % attacker->getHandle() % uint32(target->getEvadeShield())).str()));
		return true;
	}
	// shield broken: the attempt goes through, evading ends and the waiting period starts
	target->setEvading(false);
	target->setEvadeCooldownUntil(getMSTime() + uint32(sConfig.GetIntDefault("Combat.EvadeCooldownMs", 30000)));
	if (human)
		target->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
			(format("%1% breaks through your evade shield.") % attacker->getHandle()).str()));
	INFO_LOG(format("Evade shield of %1% broken by %2% (%3% -> 0)") % target->getHandle() % attacker->getHandle() % uint32(before));
	return false;
}

bool CombatSystem::UseCR2Interlock()
{
	return sConfig.GetStringDefault("Combat.InterlockModel", "cr2") != "legacy";
}

uint16 CombatSystem::GetCombatState(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> lock(m_combatMutex);
	InterlockSession* s = GetInterlockSession(goId);
	if (!s) return 0;
	return (goId == s->goIdA) ? s->combatStateA : s->combatStateB;
}

static float CR2StanceAccuracy(uint8 t)
{
	switch (t)
	{
		case TACTIC_SPEED:   return sConfig.GetFloatDefault("Combat.CR2.SpeedAccuracy", 0.10f);
		case TACTIC_POWER:   return -sConfig.GetFloatDefault("Combat.CR2.PowerAccuracyPenalty", 0.05f);
		case TACTIC_DEFENSE: return -sConfig.GetFloatDefault("Combat.CR2.BlockAccuracyPenalty", 0.15f);
		default:             return 0.0f;
	}
}

static float CR2StanceDefense(uint8 t)
{
	switch (t)
	{
		case TACTIC_SPEED:     return -sConfig.GetFloatDefault("Combat.CR2.SpeedDefensePenalty", 0.10f);
		case TACTIC_POWER:     return -sConfig.GetFloatDefault("Combat.CR2.PowerDefensePenalty", 0.05f);
		case TACTIC_RETALIATE: return sConfig.GetFloatDefault("Combat.CR2.GrabDefense", 0.10f);
		case TACTIC_DEFENSE:   return sConfig.GetFloatDefault("Combat.CR2.BlockDefense", 0.25f);
		default:               return 0.0f;
	}
}

static float CR2StanceDamage(uint8 t)
{
	switch (t)
	{
		case TACTIC_POWER:     return sConfig.GetFloatDefault("Combat.CR2.PowerDamage", 1.20f);
		case TACTIC_RETALIATE: return sConfig.GetFloatDefault("Combat.CR2.GrabDamage", 0.85f);
		case TACTIC_DEFENSE:   return sConfig.GetFloatDefault("Combat.CR2.BlockDamage", 0.50f);
		default:               return 1.0f;
	}
}

float CombatSystem::CR2HitChance(PlayerObject* attacker, PlayerObject* defender, uint8 attTac, uint8 defTac)
{
	if (!attacker || !defender) return 0.0f;
	float p = sConfig.GetFloatDefault("Combat.CR2.BaseHit", 0.55f);
	const float perLevel = sConfig.GetFloatDefault("Combat.CR2.LevelHit", 0.02f);
	const float lvl = (float(attacker->getLevel()) - float(defender->getLevel())) * perLevel;
	p += std::max(-0.25f, std::min(0.25f, lvl));
	p += CR2StanceAccuracy(attTac);
	// Block's bonus is Evade Combat and it gives up Force Combat, which is what resists a Grab
	if (attTac == TACTIC_RETALIATE && defTac == TACTIC_DEFENSE)
		p += sConfig.GetFloatDefault("Combat.CR2.GrabVsBlock", 0.10f);
	else
		p -= CR2StanceDefense(defTac);
	return std::max(0.05f, std::min(0.95f, p));
}

void CombatSystem::RunCR2Exchange(InterlockSession &session, PlayerObject* pA, PlayerObject* pB,
	const CombatMove* moveA, const CombatMove* moveB, uint8 tacA, uint8 tacB)
{
	const uint32 round = session.roundNumber;
	// a state lasts through the round after the hit that caused it
	if (session.combatStateA != 0 && round > session.stateRoundA + 1) session.combatStateA = 0;
	if (session.combatStateB != 0 && round > session.stateRoundB + 1) session.combatStateB = 0;

	// Block restores Inner Strength and shakes off states
	const int blockIS = sConfig.GetIntDefault("Combat.CR2.BlockIS", 5);
	if (tacA == TACTIC_DEFENSE) { if (blockIS > 0) pA->restoreIS(uint16(blockIS)); session.combatStateA = 0; }
	if (tacB == TACTIC_DEFENSE) { if (blockIS > 0) pB->restoreIS(uint16(blockIS)); session.combatStateB = 0; }

	const uint16 stA = session.combatStateA;
	const uint16 stB = session.combatStateB;

	// Withdraw is contested (CR2): the attempt takes the round, it can fail, and the fighter
	// trying to leave throws nothing that round.
	const bool tryA = session.withdrawA, tryB = session.withdrawB;
	session.withdrawA = session.withdrawB = false;
	auto attemptWithdraw = [&](PlayerObject* who, PlayerObject* opp, uint8 oppTac) -> bool
	{
		const bool ok = (float(rand() % 10000) / 10000.0f) < CR2WithdrawChance(who, opp, oppTac);
		INFO_LOG(format("Interlock CR2 withdraw attempt: %1%:%2% vs %3% (p=%4$.2f) -> %5%") % who->getHandle() % who->getGoId() % opp->getHandle() % CR2WithdrawChance(who, opp, oppTac) % (ok ? "escaped" : "held"));
		if (!who->getClient().isBot())
			who->getClient().QueueCommand(std::make_shared<SystemChatMsg>(ok
				? "You withdraw from combat."
				: (format("You fail to withdraw. %1% keeps you engaged.") % opp->getHandle()).str()));
		if (ok && !opp->getClient().isBot())
			opp->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				(format("%1% withdraws from combat.") % who->getHandle()).str()));
		return ok;
	};

	// A blocking fighter throws no basic attack, only a queued special
	// Block: -100% damage and accuracy (Combat_Basics) - a blocking fighter throws nothing; a queued
	// special waits until they change stance (see RunInterlockRound). Combat.CR2.BlockSpecials = 1
	// restores the older 'specials from Block' behaviour.
	const bool blockSpecials = sConfig.GetBoolDefault("Combat.CR2.BlockSpecials", false);
	const bool aSwings = !tryA && moveA && (tacA != TACTIC_DEFENSE || (blockSpecials && session.queuedMoveA != 0));
	const bool bSwings = !tryB && moveB && (tacB != TACTIC_DEFENSE || (blockSpecials && session.queuedMoveB != 0));
	// both offense rolls are made before either blow lands
	const bool aHits = aSwings && (float(rand() % 10000) / 10000.0f) < CR2HitChance(pA, pB, tacA, tacB);
	const bool bHits = bSwings && (float(rand() % 10000) / 10000.0f) < CR2HitChance(pB, pA, tacB, tacA);

	auto swing = [&](PlayerObject* att, PlayerObject* def, const CombatMove& mv, uint8 at, uint8 dt,
		uint16 attState, uint16 defState) -> AttackResult
	{
		float mul = CR2StanceDamage(at);
		if (attState & COMBAT_STATE_DAZED) mul *= sConfig.GetFloatDefault("Combat.CR2.DazedDamage", 0.90f);
		if (defState & COMBAT_STATE_STAGGERED) mul *= sConfig.GetFloatDefault("Combat.CR2.StaggeredDamageTaken", 1.05f);
		m_cr2DamageMul = std::max(0.01f, mul);
		AttackResult r = StrikeInterlock(session, att, def, mv, at, dt, true, false, false);
		m_cr2DamageMul = 0.0f;
		return r;
	};

	AttackResult resA, resB;
	resA.hit = false; resA.isBlocked = (aSwings && tacB == TACTIC_DEFENSE);
	resB.hit = false; resB.isBlocked = (bSwings && tacA == TACTIC_DEFENSE);
	// The blows are simultaneous. Resolve in random order so neither side always lands first
	// (a fighter dropped to 0 HP by the first blow does not get the second).
	const bool aFirst = (rand() % 2) == 0;
	for (int k = 0; k < 2; k++)
	{
		const bool doA = (k == 0) == aFirst;
		if (doA && aHits && pA->getCurrentHealth() > 0 && !pA->isDead())
			resA = swing(pA, pB, *moveA, tacA, tacB, stA, stB);
		else if (!doA && bHits && pB->getCurrentHealth() > 0 && !pB->isDead())
			resB = swing(pB, pA, *moveB, tacB, tacA, stB, stA);
	}

	// withdraw attempts resolve after the blows (a fighter dropped this round does not leave)
	if (tryA && pA->getCurrentHealth() > 0 && pB->getCurrentHealth() > 0 && attemptWithdraw(pA, pB, tacB))
		session.withdrewGoId = pA->getGoId();
	else if (tryB && pA->getCurrentHealth() > 0 && pB->getCurrentHealth() > 0 && attemptWithdraw(pB, pA, tacA))
		session.withdrewGoId = pB->getGoId();

	auto stateFor = [](uint8 t) -> uint16
	{
		switch (t)
		{
			case TACTIC_SPEED:     return COMBAT_STATE_DAZED;
			case TACTIC_POWER:     return COMBAT_STATE_STAGGERED;
			case TACTIC_RETALIATE: return COMBAT_STATE_OFFBALANCE;
			default:               return 0;
		}
	};
	if (resA.hit && !resA.isBlocked && tacB != TACTIC_DEFENSE && stateFor(tacA))
	{
		session.combatStateB |= stateFor(tacA);
		session.stateRoundB = round;
	}
	if (resB.hit && !resB.isBlocked && tacA != TACTIC_DEFENSE && stateFor(tacB))
	{
		session.combatStateA |= stateFor(tacB);
		session.stateRoundA = round;
	}

	if (pA->getClient().isBot() == false || pB->getClient().isBot() == false)
	{
		INFO_LOG(format("Interlock CR2 round %1%: %2% %3% (p=%4$.2f) %5% | %6% %7% (p=%8$.2f) %9% | states A=0x%10$x B=0x%11$x")
			% round
			% pA->getHandle() % (aSwings ? (aHits ? "hit" : "miss") : "no swing") % CR2HitChance(pA, pB, tacA, tacB) % resA.damageTaken
			% pB->getHandle() % (bSwings ? (bHits ? "hit" : "miss") : "no swing") % CR2HitChance(pB, pA, tacB, tacA) % resB.damageTaken
			% session.combatStateA % session.combatStateB);
	}

	// One ILExchange per round (keeps exchangeNum 1:1 with the 0x45 round status). Show the blow
	// that landed, the bigger one when both did, otherwise the attempt that was thrown.
	if (session.ilViewIdA == 0 && session.ilViewIdB == 0)
		return;
	bool showA;
	if (resA.hit && resB.hit) showA = resA.damageTaken >= resB.damageTaken;
	else if (resA.hit) showA = true;
	else if (resB.hit) showA = false;
	else showA = aSwings || !bSwings;
	{
		const AttackResult& shown = showA ? resA : resB;
		const AttackResult& other = showA ? resB : resA;
		const uint8 targetTac = showA ? tacB : tacA;
		const bool special = showA ? (session.queuedMoveA > 1) : (session.queuedMoveB > 1);
		session.cr2Outcome = int8(CR2Outcome(shown.hit, other.hit, targetTac == TACTIC_DEFENSE, special));
	}
	if (showA && moveA)
		SendInterlockExchange(session, pA, pB, resA, *moveA, tacA, tacB);
	else if (!showA && moveB)
		SendInterlockExchange(session, pB, pA, resB, *moveB, tacB, tacA);
	session.cr2Outcome = -1;
}

InterlockExchangeOutcome CombatSystem::CR2Outcome(bool shownHit, bool otherHit, bool targetBlocking, bool special)
{
	if (shownHit && otherHit) return InterlockExchangeOutcome::Clash;
	if (shownHit) return special ? InterlockExchangeOutcome::SpecialHit : InterlockExchangeOutcome::NormalHit;
	return targetBlocking ? InterlockExchangeOutcome::Blocked : InterlockExchangeOutcome::Dodged;
}

uint8 CombatSystem::CR2BotTactic(uint8 opp, bool elite, uint16 ownStates, float hpFrac, uint32 rnd)
{
	const uint32 r = rnd % 100;
	// Block shakes off states and restores Inner Strength: hurt or dazed fighters use it
	const uint32 blockPct = (ownStates != 0 ? (elite ? 45u : 25u) : 0u) + (hpFrac < 0.30f ? (elite ? 20u : 10u) : 0u);
	if (r < blockPct) return TACTIC_DEFENSE;
	if (elite)
	{
		// read last round's stance: Grab through a Block (Block gives up Force Combat), Power into a
		// Speed fighter (Speed lowers its own defense), Speed against Power/Grab (accuracy vs defense)
		switch (opp)
		{
			case TACTIC_DEFENSE:   return TACTIC_RETALIATE;
			case TACTIC_SPEED:     return TACTIC_POWER;
			case TACTIC_POWER:
			case TACTIC_RETALIATE: return TACTIC_SPEED;
			default:               return (r & 1) ? TACTIC_POWER : TACTIC_SPEED;
		}
	}
	// street fighters press the attack: Speed 35, Power 35, Grab 20, Block 10
	const uint32 t = (rnd / 100) % 100;
	if (t < 35) return TACTIC_SPEED;
	if (t < 70) return TACTIC_POWER;
	if (t < 90) return TACTIC_RETALIATE;
	return TACTIC_DEFENSE;
}

bool CombatSystem::RunFreeFireShot(FreeFireState &state)
{
	PlayerObject* pA = getPlayerSafe(state.attackerGoId);
	PlayerObject* pB = getPlayerSafe(state.targetGoId);
	if (!pA || !pB || pA->isDead() || pB->isDead())
		return false; //Update() reaps the engagement (never erase mid-iteration)
	// The shot that emptied the bar already landed. Do not keep firing for the takedown window.
	if (pA->getCurrentHealth() == 0 || pB->getCurrentHealth() == 0)
		return false;
	// Pulled into close combat: the gun loop ends (it kept firing during the interlock).
	if (IsInterlocked(state.attackerGoId))
		return false;

	const CombatMove* moveA = state.moveId ? GetMove(state.moveId) : nullptr;
	CombatMove synthesizedGunMove;
	auto weapons = pA->getEquippedWeapons();
	if (!moveA && !weapons.empty())
	{
		const ItemTemplate* wpnTpl = sDataLoader.GetItemTemplate(weapons[0]->getTemplateId());
		if (wpnTpl && wpnTpl->type == ITEM_TYPE_WEAPON)
		{
			synthesizedGunMove.id = 129; // HandgunsAbility
			synthesizedGunMove.name = wpnTpl->name;
			synthesizedGunMove.dmgType = DAMAGE_RANGED;
			synthesizedGunMove.minDmg = (float)wpnTpl->minDamage;
			synthesizedGunMove.maxDmg = (float)wpnTpl->maxDamage;
			synthesizedGunMove.minDmgPerLvl = 1.0f;
			synthesizedGunMove.maxDmgPerLvl = 1.5f;
			synthesizedGunMove.range = (wpnTpl->range > 0.0f) ? wpnTpl->range : 2500.0f;
			synthesizedGunMove.hitFxId = 0x280006ea;
			synthesizedGunMove.interlockOnly = false;
			synthesizedGunMove.freefireOnly = true;
			synthesizedGunMove.isCost = 0;
			moveA = &synthesizedGunMove;
		}
	}
	if (!moveA)
		moveA = DefaultRanged();
	if (!moveA)
		return false;

	// Resolve equipped firearm properties
	float shotRange = moveA->range;
	uint16 firingAnim = 0x0EDF; // default pistol attack anim
	bool isDualWield = false;
	if (!weapons.empty())
	{
		const ItemTemplate* wpnTpl = sDataLoader.GetItemTemplate(weapons[0]->getTemplateId());
		if (wpnTpl && wpnTpl->type == ITEM_TYPE_WEAPON)
		{
			if (wpnTpl->range > 0)
				shotRange = wpnTpl->range;
			isDualWield = (wpnTpl->isDualWield || pA->isDualWielding());
			if (wpnTpl->animAttack != 0)
				firingAnim = wpnTpl->animAttack;
			else if (isDualWield)
				firingAnim = 0x0EDD;

			// Ammunition tracking & clip reload
			if (!pA->consumeFirearmAmmo())
				return false; // Stop free fire
		}
	}

	// Check distance against firearm ballistic range
	float dist = float(pA->getPosition().Distance(pB->getPosition()));
	if (shotRange > 0 && dist > shotRange)
	{
		// It used to loop forever, repeating this line. Give the player a few shots to close
		// in, then stop firing.
		state.outOfRangeShots++;
		state.shotCount++;
		if (state.outOfRangeShots >= 4)
		{
			if (!pA->getClient().isBot())
				pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
					"{c:FFFF00}[COMBAT] Target is out of range. You stop firing.{/c}"));
			return false;
		}
		if (state.outOfRangeShots == 1 && !pA->getClient().isBot())
			pA->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
				"{c:FFFF00}[COMBAT] Target is out of firearm range.{/c}"));
		return true;
	}
	state.outOfRangeShots = 0;

	state.shotCount++;

	// Broadcast weapon firing animation (operatives only, bots never send animation state)
	if (!pA->getClient().isBot())
	{
		pA->setCurrentAnimation(uint8(firingAnim & 0xFF));
		sGame.AnnounceStateUpdate(&pA->getClient(), std::make_shared<AnimationStateMsg>(pA->getGoId()));
	}

	ResolveAttack(pA, pB, *moveA, pA->getTactic(), pB->getTactic());
	if (isDualWield && !pB->isDead() && sConfig.GetBoolDefault("Combat.EmulatorExtras", false))
	{
		// Second volley for dual-wielding handguns
		ResolveAttack(pA, pB, *moveA, pA->getTactic(), pB->getTactic());
	}
	// A ranged ability is one special shot. It used to become the move for every following
	// shot (Inner Strength paid once, the special repeated forever).
	if (state.moveId != 0)
		state.moveId = 0;
	return !pB->isDead() && pB->getCurrentHealth() > 0;
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

    // Retail Awakened ability: Evade Combat (EvadeCombatAbility, id 217)
    if (requestedAbilityId == 217 && sConfig.GetBoolDefault("Combat.EvadeCombat", true))
        return ToggleEvadeCombat(caster);

    // Check authentic discipline routing (Hacker viral logic / Coder RSI support)
    const AbilityTemplate* abilTempl = sDataLoader.GetAbilityTemplate(requestedAbilityId);
    // Retail: programs that need a few uninterrupted seconds cannot be run in close combat;
    // quick ones work in or out of interlock (Combat.InterlockCastLimitMs, default 1000).
    if (abilTempl && abilTempl->castTime > 0 && IsInterlocked(caster->getGoId()) &&
        abilTempl->discipline != DisciplineType::MARTIAL_ARTIST &&
        int(abilTempl->castTime) > sConfig.GetIntDefault("Combat.InterlockCastLimitMs", 1000))
    {
        if (humanCaster)
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "That program needs a few uninterrupted seconds. Withdraw from close combat first."));
        return false;
    }
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

    // Hacker, coder, soldier, and spy already spend Inner Strength on their own path.
    // Martial and generic moves did not, so every special was free.
    // CR2: an Off-Balance fighter (hit by a Grab last round) pays more Inner Strength for specials
    uint16 isCost = move->isCost;
    if (isCost > 0 && UseCR2Interlock() && (GetCombatState(caster->getGoId()) & COMBAT_STATE_OFFBALANCE))
        isCost = uint16(isCost + std::max<uint16>(1, isCost / 10)); // StaggeredAbility/OffBalanceAbility data: -5 / -10
    const bool spentIS = isCost > 0;
    if (spentIS && !caster->spendIS(isCost))
    {
        if (humanCaster)
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:FF0000}[COMBAT] Not enough Inner Strength for %1% (%2% required).{/c}")
                    % move->name % isCost).str()));
        return false;
    }

    //cast bar for anything with a cast time
    if (move->castTime > 0.05f && humanCaster)
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, move->castTime));
    
    // Disruption from special ability execution
    if (GameServer::getSingletonPtr() == nullptr || (humanCaster && sConfig.GetBoolDefault("World.ThreatEscalation", false))) {
        sMatrixThreatHeatmap.RecordDisruption((float)caster->getPosition().x, (float)caster->getPosition().y, (float)caster->getPosition().z, 12.0f, move->name);
    }

    // melee abilities pressed outside an interlock open one against the selected target
    if (move->interlockOnly && !IsInterlocked(caster->getGoId()))
    {
        if (targetGoId == 0 || !RequestInterlock(caster->getGoId(), targetGoId))
        {
            if (spentIS) caster->restoreIS(isCost);
            INFO_LOG(format("UseAbility: %1%:%2% %3% needs an interlock but none could be opened (target go %4%)")
                % caster->getHandle() % caster->getGoId() % move->name % targetGoId);
            if (humanCaster)
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    (format("{c:FF0000}[COMBAT] %1% needs a valid target within melee range.{/c}") % move->name).str()));
            return false;
        }
    }
    if (move->freefireOnly && IsInterlocked(caster->getGoId()))
    {
        if (spentIS) caster->restoreIS(isCost);
        return false;
    }
    
    if (IsInterlocked(caster->getGoId())) {
        QueueAbility(caster->getGoId(), abilityId);
        return true;
    }
    if (!RequestRangedCombat(caster->getGoId(), targetGoId, abilityId))
    {
        if (spentIS) caster->restoreIS(isCost);
        return false;
    }
    return true;
}

void CombatSystem::AwardKill(PlayerObject* killer, PlayerObject* victim)
{
    if (!killer || !victim) return;
    
    // Record elimination on Matrix Threat Heatmap
    if (GameServer::getSingletonPtr() == nullptr || (!killer->getClient().isBot() && sConfig.GetBoolDefault("World.ThreatEscalation", false))) {
        sMatrixThreatHeatmap.RecordDisruption((float)victim->getPosition().x, (float)victim->getPosition().y, (float)victim->getPosition().z, 
                                             killer->getClient().isBot() ? 30.0f : 50.0f, "Combat Elimination");
    }

    // Retail: a street mob that cons grey gives no experience and no information
    if (victim->isWorldMob() && WorldMobConsGrey((int)victim->getLevel(), (int)killer->getLevel()))
    {
        if (AgentPossessionManager::getSingletonPtr() && sAgentPossessionMgr.IsEntityPossessed(victim->getGoId()))
            sAgentPossessionMgr.HandleAgentDefeat(victim->getGoId(), killer->getGoId());
        INFO_LOG(format("AwardKill: %1% defeated grey %2% (L%3%) -> no XP/info") % killer->getHandle() % victim->getHandle() % (int)victim->getLevel());
        if (!killer->getClient().isBot())
            killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:AAAAAA}[COMBAT] Defeated %1%. It cons grey: no experience or information.{/c}") % victim->getHandle()).str()));
        return;
    }
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

    // Phase 14: +5% Information ($I) drop bonus when fighting inside a friendly-controlled district
    if (FactionWarManager::getSingletonPtr() != nullptr) {
        float districtBonus = sFactionWarMgr.GetDistrictInfoGainBonus(killer->getDistrict(), killer->getFaction());
        if (districtBonus > 0.0f) {
            infoAmount += std::max<uint32>(1, uint32(std::lround(float(infoAmount) * districtBonus)));
        }
    }
    
    // Dynamic Agent Defeat Demorphing
    if (AgentPossessionManager::getSingletonPtr() && sAgentPossessionMgr.IsEntityPossessed(victim->getGoId())) {
        sAgentPossessionMgr.HandleAgentDefeat(victim->getGoId(), killer->getGoId());
    }

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




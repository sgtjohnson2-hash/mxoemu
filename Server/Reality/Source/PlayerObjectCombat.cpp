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

#include "Common.h"
#include "PlayerObject.h"
#include "GameServer.h"
#include "CombatSystem.h"
#include "BotManager.h"
#include "BotClient.h"
#include "FactionWarManager.h"
#include "AdaptiveMusicSystem.h"
#include "Database/AsyncDatabase.h"
#include <mutex>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include "Log.h"
#include "Database/Database.h"
#include "GameServer.h"
#include "GameClient.h"
#include "GameSocket.h"
#include "AbilitySystem.h"
#include "MessageTypes.h"
#include "Database/PreparedStatement.h"
#include "DataLoader.h"
#include "MissionSystem.h"
#include "Config.h"
#include "InventorySystem.h"
#include "SpatialGrid.h"
#include "LootManager.h"
#include <fstream>
#include <sstream>

std::map<uint32, std::vector<LocationVector>> PlayerObject::s_hardlineCache;
std::map<std::pair<uint32, uint32>, HardlineNode> PlayerObject::s_hardlineDirectory;

void PlayerObject::LoadHardlinesFromCSV(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
    {
        WARNING_LOG(format("PlayerObject: Could not open hardlines CSV: %1%") % filePath);
        return;
    }

    s_hardlineDirectory.clear();
    s_hardlineCache.clear();

    std::string line;
    bool isFirstLine = true;
    size_t count = 0;

    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        if (isFirstLine)
        {
            isFirstLine = false; // skip header: id;districtId;hardlineId;name;x;y;z;rot;factionTag
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ';'))
        {
            tokens.push_back(token);
        }

        if (tokens.size() >= 8)
        {
            try
            {
                HardlineNode node;
                node.id = static_cast<uint32>(std::stoul(tokens[0]));
                node.districtId = static_cast<uint32>(std::stoul(tokens[1]));
                node.hardlineId = static_cast<uint32>(std::stoul(tokens[2]));
                node.name = tokens[3];

                std::string xStr = tokens[4];
                std::replace(xStr.begin(), xStr.end(), ',', '.');
                node.x = std::stod(xStr);

                std::string yStr = tokens[5];
                std::replace(yStr.begin(), yStr.end(), ',', '.');
                node.y = std::stod(yStr);

                std::string zStr = tokens[6];
                std::replace(zStr.begin(), zStr.end(), ',', '.');
                node.z = std::stod(zStr);

                std::string rotStr = tokens[7];
                std::replace(rotStr.begin(), rotStr.end(), ',', '.');
                node.rot = std::stod(rotStr);

                if (tokens.size() >= 9)
                {
                    node.factionTag = std::stoi(tokens[8]);
                }

                s_hardlineDirectory[std::make_pair(node.districtId, node.hardlineId)] = node;

                LocationVector hlPos(node.x, node.y, node.z);
                hlPos.rot = node.rot;
                s_hardlineCache[node.districtId].push_back(hlPos);
                count++;
            }
            catch (...) {}
        }
    }
    INFO_LOG(format("PlayerObject: Successfully loaded %1% authentic hardlines across %2% districts from %3%.")
        % count % s_hardlineCache.size() % filePath);
}

void PlayerObject::LoadHardlines()
{
    INFO_LOG("Loading Hardlines into cache...");
    s_hardlineCache.clear();
    s_hardlineDirectory.clear();

    if (Database_Main != nullptr)
    {
        try
        {
            PreparedStatement stmt("SELECT `DistrictId`,`X`,`Y`,`Z`,`ROT`,`HardLineId`,`HardlineName`,`FactionTag` FROM `hardlines`");
            scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
            if (result)
            {
            do
            {
                Field *field = result->Fetch();
                HardlineNode node;
                node.districtId = field[0].GetUInt32();
                node.x = field[1].GetDouble();
                node.y = field[2].GetDouble();
                node.z = field[3].GetDouble();
                node.rot = field[4].GetDouble();
                node.hardlineId = field[5].GetUInt32();
                node.name = field[6].GetString();
                node.factionTag = field[7].GetInt32();

                s_hardlineDirectory[std::make_pair(node.districtId, node.hardlineId)] = node;

                LocationVector hlPos(node.x, node.y, node.z);
                hlPos.rot = node.rot;
                s_hardlineCache[node.districtId].push_back(hlPos);
            } while (result->NextRow());
        }
    }
    catch (...) {}
}

    // Fallback if DB was unavailable or yielded 0 records (e.g. headless tests)
    if (s_hardlineDirectory.empty())
    {
        LoadHardlinesFromCSV("Data/hd_dump/hardlines.csv");
    }
    else
    {
        INFO_LOG(format("Loaded %1% hardlines across %2% districts from DB.") % s_hardlineDirectory.size() % s_hardlineCache.size());
    }
}

const HardlineNode* PlayerObject::GetHardline(uint32 districtId, uint32 hardlineId)
{
    auto it = s_hardlineDirectory.find(std::make_pair(districtId, hardlineId));
    if (it != s_hardlineDirectory.end()) return &(it->second);

    // Fallback: check across all districts if districtId was omitted or unknown
    for (const auto& pair : s_hardlineDirectory)
    {
        if (pair.second.hardlineId == hardlineId) return &(pair.second);
    }
    return nullptr;
}

const HardlineNode* PlayerObject::GetNearestHardline(uint32 districtId, double x, double z)
{
    const HardlineNode* nearest = nullptr;
    double bestDistSq = 1e18;

    for (const auto& pair : s_hardlineDirectory)
    {
        if (districtId != 0 && pair.first.first != districtId) continue;
        double dx = pair.second.x - x;
        double dz = pair.second.z - z;
        double dSq = dx * dx + dz * dz;
        if (dSq < bestDistSq)
        {
            bestDistSq = dSq;
            nearest = &(pair.second);
        }
    }
    return nearest;
}

// ---------------------------------------------------------------------------
// combat state transitions
// ---------------------------------------------------------------------------

void PlayerObject::enterInterlock( uint32 partnerGoId )
{
	if (cancelEvents(EVENT_JACKOUT) > 0)
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			"{c:FF0000}[JACKOUT] Jackout sequence interrupted by close combat interlock!{/c}")));
	}
	m_ilPartner = partnerGoId;
	m_inCombat = true;
	setCombatStance(true);
	if (m_parent.isBot())
	{
		BotClient* bot = dynamic_cast<BotClient*>(&m_parent);
		if (bot)
		{
			bot->SetTargetGoId(partnerGoId);
		}
	}
	else
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			"{c:FF8800}You are now in Interlock. Tactics: &tactic speed|power|grab|block - withdraw with &withdraw{/c}")));
	}
}

void PlayerObject::leaveInterlock()
{
	m_ilPartner = 0;
	m_inCombat = false;
	m_tactic = TACTIC_NORMAL;
	setCombatStance(false);
}

void PlayerObject::setCombatStance( bool inCombatStance )
{
	uint8 mode = inCombatStance ? 1 : 0;
	//own HUD/stance
	m_parent.QueueState(shared_ptr<SelfCombatantModeMsg>(new SelfCombatantModeMsg(mode)));
	//everyone else sees the stance change on our view
	sGame.AnnounceStateUpdate(&m_parent,shared_ptr<CombatantModeMsg>(new CombatantModeMsg(m_goId,mode)));
}

void PlayerObject::takeDamage( uint32 attackerGoId, uint16 damage, uint32 fxId )
{
	if (m_isDead)
		return;
	m_lastDamageTakenMs = getMSTime();

	if (cancelEvents(EVENT_JACKOUT) > 0)
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			"{c:FF0000}[JACKOUT] Jackout sequence interrupted by incoming damage!{/c}")));
	}

	// Configurable retail hit FX (Task 3): 0x280006DF was a misidentified weapon skeleton model
	// (resource/GameObjects/weapons/program_launcher2/skeleton/skeleton.ska) that crashed retail 7.6005
	// in client.dll+0x59608C (resolving _GaussianBlur).
	static const uint32 configuredHitFx = (uint32)sConfig.GetIntDefault("Combat.HitFx", 0);
	if (configuredHitFx == 0 || fxId == 0x280006DF || fxId == 0x28000794 || fxId == 0x28000432 || fxId == 0x2800045A)
		fxId = configuredHitFx;

	if (attackerGoId != 0 && attackerGoId != m_goId)
	{
		PlayerObject* attackerObj = sObjMgr.getGOPtrSafe(attackerGoId);
		if (attackerObj)
		{
			ensureEntityKnown(attackerObj);
			if (m_targetGoId == 0)
			{
				m_targetGoId = attackerGoId;
				if (!m_parent.isBot())
				{
					m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
						(format("{c:FF3333}[COMBAT] Under attack by %1%! Target locked.{/c}") % attackerObj->getHandle()).str())));
				}
			}
		}
		if (m_parent.isBot())
		{
			BotClient* bot = dynamic_cast<BotClient*>(&m_parent);
			if (bot && bot->GetTargetGoId() == 0)
			{
				bot->SetTargetGoId(attackerGoId);
			}
		}
	}

	// Apply basic mitigation based on player level
	uint16 mitigation = m_lvl / 2;
	uint16 actualDamage = (damage > mitigation) ? (damage - mitigation) : 1;

	// Apply firewall absorption
	if (m_firewallPoints > 0 && actualDamage > 0)
	{
		uint16 absorbed = std::min<uint16>(m_firewallPoints, actualDamage);
		m_firewallPoints -= absorbed;
		actualDamage -= absorbed;
		if (!m_parent.isBot())
		{
			m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
				(format("{c:00FFFF}[FIREWALL] Absorbed %1% damage (%2% shield remaining).{/c}") % absorbed % m_firewallPoints).str())));
		}
	}

	if (m_isStealthed && actualDamage > 0)
	{
		m_isStealthed = false;
		if (!m_parent.isBot())
		{
			m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
				"{c:FF5555}[SPY] Concealment broken by incoming damage!{/c}")));
		}
	}

	uint16 healthBefore = m_healthC;
	if (actualDamage >= m_healthC)
		m_healthC = 0;
	else
		m_healthC -= actualDamage;

	// Phase A observability: log every hit that involves a human player (bot-vs-bot
	// background fights would flood the log at server scale).
	{
		PlayerObject* attackerObj = sObjMgr.getGOPtrSafe(attackerGoId);
		if (!m_parent.isBot() || (attackerObj && !attackerObj->getClient().isBot()))
		{
			INFO_LOG(format("Damage applied: %1%:%2% -> %3%:%4% raw %5% actual %6% fx 0x%7$X HP %8% -> %9%/%10%")
				% (attackerObj ? attackerObj->getHandle() : std::string("<none>")) % attackerGoId
				% m_handle % m_goId % damage % actualDamage % fxId
				% healthBefore % m_healthC % m_healthM);
		}
	}

	m_hitCounter++;

	// Notify Adaptive Music System of combat intensity (threat level)
	sAdaptiveMusicSystem.registerThreat(m_goId, actualDamage * 2, getMSTime());

	//health change + hit FX: other clients get the other-view layout,
	//the victim's own client gets the self-view layout (HUD bar + FX)
	sGame.AnnounceStateUpdate(&m_parent,shared_ptr<CombatHitFxMsg>(new CombatHitFxMsg(m_goId,fxId,m_hitCounter)));
	m_parent.QueueState(shared_ptr<SelfHitFxMsg>(new SelfHitFxMsg(this,fxId,m_hitCounter)));

	if (m_healthC == 0) {
        if (m_deathDelayMS == 0) {
		    die(attackerGoId);
        }
    }
}

void PlayerObject::applyHeal( uint32 healerGoId, uint16 amount, uint32 fxId )
{
	if (isDead()) return;

	static const uint32 configuredHitFx = (uint32)sConfig.GetIntDefault("Combat.HitFx", 0);
	if (configuredHitFx == 0 || fxId == 0x280006DF || fxId == 0x28000794 || fxId == 0x28000432 || fxId == 0x2800045A)
		fxId = configuredHitFx;

	uint16 healthBefore = m_healthC;
	uint32 newHealth = uint32(m_healthC) + uint32(amount);
	if (newHealth > m_healthM)
		newHealth = m_healthM;
	m_healthC = (uint16)newHealth;
	uint16 actualHeal = m_healthC - healthBefore;

	m_hitCounter++;

	PlayerObject* healerObj = sObjMgr.getGOPtrSafe(healerGoId);
	if (!m_parent.isBot() || (healerObj && !healerObj->getClient().isBot()))
	{
		INFO_LOG(format("Heal applied: %1%:%2% -> %3%:%4% raw %5% actual %6% fx 0x%7$X HP %8% -> %9%/%10%")
			% (healerObj ? healerObj->getHandle() : std::string("<none>")) % healerGoId
			% m_handle % m_goId % amount % actualHeal % fxId
			% healthBefore % m_healthC % m_healthM);
	}

	sGame.AnnounceStateUpdate(&m_parent, shared_ptr<CombatHitFxMsg>(new CombatHitFxMsg(m_goId, fxId, m_hitCounter)));
	m_parent.QueueState(shared_ptr<SelfHitFxMsg>(new SelfHitFxMsg(this, fxId, m_hitCounter)));
	sendVitals();
}

void PlayerObject::revive( uint32 reviverGoId, float healthPct )
{
	if (!isDead()) return;

	m_isDead = false;
	m_deathDelayMS = 0;
	m_healthC = std::max<uint16>(1, (uint16)(m_healthM * healthPct));
	m_innerStrC = m_innerStrM / 2;

	PlayerObject* reviver = sObjMgr.getGOPtrSafe(reviverGoId);
	INFO_LOG(format("Character %1% revived by %2% with %3% HP")
		% m_handle % (reviver ? reviver->getHandle() : "<system>") % m_healthC);

	if (!m_parent.isBot())
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:00FF00}[RSI RECONSTRUCTION] You have been resuscitated by %1%!{/c}")
				% (reviver ? reviver->getHandle() : "Support Operative")).str())));
	}

	sGame.AnnounceStateUpdate(&m_parent, shared_ptr<HealthUpdateMsg>(new HealthUpdateMsg(m_goId, true, true)));
	sendVitals(true, true);
}

bool PlayerObject::spendIS( uint16 amount )
{
	if (m_innerStrC < amount)
		return false;

	m_innerStrC -= amount;
	sendVitals();
	return true;
}

void PlayerObject::restoreIS( uint16 amount )
{
	uint32 newIS = uint32(m_innerStrC) + uint32(amount);
	if (newIS > m_innerStrM)
		newIS = m_innerStrM;
	m_innerStrC = uint16(newIS);
	sendVitals();
}

void PlayerObject::sendVitals( bool includeMax, bool includeDead )
{
	m_parent.QueueState(shared_ptr<SelfVitalsMsg>(new SelfVitalsMsg(this,includeMax,includeDead)));
}

void PlayerObject::sendHealthUpdate()
{
	sGame.AnnounceStateUpdate(&m_parent,shared_ptr<HealthUpdateMsg>(new HealthUpdateMsg(m_goId)));
	sendVitals();
}

void PlayerObject::awardCombatExperience( uint32 amount )
{
	m_exp += amount;
	
	// Check for Level Up (Bots and Players)
    // The authentic Matrix Online XP curve (approximated polynomial: level * (level + 1) * 500)
    auto getRequiredExpForLevel = [](uint8 level) -> uint32 {
        if (level >= 50) return 0xFFFFFFFF;
        // Example curve: L1->2=1000, L2->3=3000, L10->11=55000
        return level * (level + 1) * 500;
    };
    
	uint32 requiredExp = getRequiredExpForLevel(m_lvl);
	bool leveledUp = false;
	while (m_exp >= requiredExp && m_lvl < 50) {
		m_lvl++;
		m_healthM += 50;
		m_healthC = m_healthM;
		m_innerStrM += 25;
		m_innerStrC = m_innerStrM;
		requiredExp = getRequiredExpForLevel(m_lvl);
		leveledUp = true;
	}

	m_parent.QueueCommand(shared_ptr<SetExperienceCmd>(new SetExperienceCmd(m_exp)));
	
	if (leveledUp) {
		INFO_LOG(format("Character %1% leveled up to %2%!") % m_handle % (uint32)m_lvl);
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:00FF00}Congratulations! You are now level %1%.{/c}") % (uint32)m_lvl).str() )));
		sendVitals(true, true);
		sGame.AnnounceStateUpdate(&m_parent, shared_ptr<HealthUpdateMsg>(new HealthUpdateMsg(m_goId)));
	} else {
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:00FFFF}You gained %1% experience.{/c}") % amount).str() )));
	}

	if (m_characterUID < 9000000) { // Only save real players to DB
		PreparedStatement stmt("UPDATE `characters` SET `exp` = ?0, `level` = ?1, `healthM` = ?2, `innerStrM` = ?3 WHERE `charId` = ?4");
		stmt.SetUInt32(0, m_exp);
		stmt.SetUInt32(1, (uint32)m_lvl);
		stmt.SetUInt32(2, m_healthM);
		stmt.SetUInt32(3, m_innerStrM);
		stmt.SetUInt64(4, m_characterUID);
		sDatabase.ExecutePrepared(&stmt);
	}
}

void PlayerObject::die( uint32 killerGoId )
{
	if (m_isDead)
		return;

	m_isDead = true;
	m_inCombat = false;
	m_healthC = 0;
	sCombatSys.RemoveCombatant(m_goId);

	string killerName = "the Matrix";
	PlayerObject *killer = sObjMgr.getGOPtrSafe(killerGoId);
	try
	{
		if (killer) {
			killerName = killer->getHandle();
			uint32 killerFaction = killer->getFaction();
			uint32 victimFaction = getFaction();
			sFactionWarMgr.registerPvPKill(killerFaction, victimFaction);
		}
	}
	catch (...) {}

	INFO_LOG(format("Player %1%:%2% was defeated by %3%") % m_handle % m_goId % killerName);

	if (!m_parent.isBot())
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:FF0000}You have been defeated by %1%. Emergency jack-out in progress...{/c}") % killerName).str() )));
		sGame.AnnounceCommand(&m_parent,shared_ptr<SystemChatMsg>(new SystemChatMsg(
			(format("{c:FF4444}%1% has been defeated by %2%.{/c}") % m_handle % killerName).str() )));
	}
	else
	{
		DEBUG_LOG(format("Bot %1% defeated by %2%") % m_handle % killerName);
	}

	//IsDead attribute so clients render the death state on our views
	sGame.AnnounceStateUpdate(&m_parent,shared_ptr<HealthUpdateMsg>(new HealthUpdateMsg(m_goId,false,true)), true);
	sendVitals(false,true);
	setCombatStance(false);

	// H1: every death pays out here (it used to live in ResolveAttack and only ran for
	// zero-delay deaths, which never happen because takedowns always delay death 3 s)
	if (killer && killerGoId != m_goId)
	{
		sCombatSys.AwardKill(killer, this);
		// DEFEAT objectives were never advanced by anything (only LOOT was, below)
		try { sMissionSys.AdvanceObjective(killer, ObjectiveCommand::DEFEAT, m_goId); } catch (...) {}
	}
    
    // Authentic Loot Engine (Only bots drop loot)
    if (m_parent.isBot())
    {
        try {
            PlayerObject *killer = sObjMgr.getGOPtrSafe(killerGoId);
            if (killer && !killer->isDead())
            {
                sLootMgr.GenerateLoot(killer, this);
            }
        } catch (...) {}
    }
    
	sAdaptiveMusicSystem.clearThreat(m_goId, getMSTime());

	//(removed: setOnlineStatus(false) flagged dead players offline in the DB/friends list, and a
	//jackout beam was used as a "downed" visual; the IsDead attribute above is the death state)

	this->addEvent(EVENT_RESPAWN,boost::bind(&PlayerObject::respawn,this),8.0f);
}

void PlayerObject::respawn()
{
	if (!m_isDead)
		return;

	m_isDead = false;
	m_healthC = m_healthM / 2;	//come back at half health
	m_innerStrC = m_innerStrM;
	m_tactic = TACTIC_NORMAL;
	m_hitCounter = 0;


	//respawn at the nearest hardline in this district, if we know any
	{
        // V17: Use cached hardlines instead of synchronous DB query
		if (s_hardlineCache.count((uint32)m_district) > 0)
		{
			double bestDistSq = -1;
			LocationVector bestPos = m_pos;
            const auto& hardlines = s_hardlineCache[(uint32)m_district];
            for (const auto& hlPos : hardlines)
			{
				double distSq = m_pos.DistanceSq(hlPos);
				if (bestDistSq < 0 || distSq < bestDistSq)
				{
					bestDistSq = distSq;
					bestPos = hlPos;
				}
			}
			
			m_pos = bestPos;
			sGame.AnnounceStateUpdate(NULL,shared_ptr<PositionStateMsg>(new PositionStateMsg(m_goId)));
			// m_parent.QueueCommand(shared_ptr<TeleportSelfCmd>(new TeleportSelfCmd(m_goId,bestPos,m_district)));
		}
	}

	//clear IsDead and refresh all bars including maxima
	sGame.AnnounceStateUpdate(&m_parent,shared_ptr<HealthUpdateMsg>(new HealthUpdateMsg(m_goId,true,true)));
	sendVitals(true,true);
	m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
		"{c:00FF00}You have been re-inserted at the nearest hardline.{/c}")));
}

// ---------------------------------------------------------------------------
// combat RPC handlers
// ---------------------------------------------------------------------------

//0x40 - client requests close combat (interlock) against a view
void PlayerObject::RPC_HandleCloseCombatRequest( ByteBuffer &srcCmd )
{
	uint16 targetViewId = srcCmd.read<uint16>();
	uint16 spawnCounter = 0;
	if (srcCmd.remaining() >= sizeof(spawnCounter))
		spawnCounter = srcCmd.read<uint16>();

	uint32 targetGoId = sObjMgr.getGOForView(&m_parent,targetViewId);
	if (targetGoId == 0)
		targetGoId = m_targetGoId;

	INFO_LOG(format("(%1%) %2%:%3% close combat request view %4% spawn %5% -> target go %6%")
		% m_parent.Address() % m_handle % m_goId % targetViewId % spawnCounter % targetGoId);

	if (targetGoId == 0)
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}No valid interlock target.{/c}")));
		return;
	}

	if (spawnCounter == 0)
		spawnCounter = PLAYER_SPAWN_COUNTER;

	//echo the client's own u32 (view | spawn<<16) back in the interlock pairing, as HDS does
	uint32 clientTargetRef = uint32(targetViewId) | (uint32(spawnCounter) << 16);
	if (sCombatSys.RequestInterlock(m_goId,targetGoId,clientTargetRef) == false)
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}Interlock request failed.{/c}")));
}

//0x41 - client requests ranged/free-fire combat
void PlayerObject::RPC_HandleRangeCombatRequest( ByteBuffer &srcCmd )
{
	uint16 targetViewId = 0;
	if (srcCmd.remaining() >= sizeof(targetViewId))
		targetViewId = srcCmd.read<uint16>();

	//log leftover payload - layout of this RPC is still being researched
	if (srcCmd.remaining() > 0)
	{
		ByteBuffer extraData = ByteBuffer(&srcCmd.contents()[srcCmd.rpos()],srcCmd.remaining());
		DEBUG_LOG(format("(%1%) %2%:%3% range combat extra data: %4%")
			% m_parent.Address() % m_handle % m_goId % Bin2Hex(extraData));
	}

	uint32 targetGoId = sObjMgr.getGOForView(&m_parent,targetViewId);
	if (targetGoId == 0)
		targetGoId = m_targetGoId;

	if (targetGoId == 0)
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}No valid target to fire at.{/c}")));
		return;
	}

	if (sCombatSys.RequestRangedCombat(m_goId,targetGoId) == false)
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}Can't engage that target.{/c}")));
}

//0x42 - combat tactic change
void PlayerObject::RPC_HandleChangeTactic( ByteBuffer &srcCmd )
{
	//The client sends the ILDB tactic value directly (Block=3 Grab=0 Power=4 Speed=5, see
	//mxoTacticType). The 0..3 "TacticAdapter" remap had no source and has been removed.
	//Logged at INFO until the value set is confirmed from a real session.
	uint8 rawClientTactic = srcCmd.read<uint8>();
	uint8 serverTactic = (rawClientTactic <= TACTIC_NORMAL) ? rawClientTactic : uint8(TACTIC_NORMAL);

	INFO_LOG(format("(%1%) %2%:%3% tactic change raw=%4%")
		% m_parent.Address() % m_handle % m_goId % uint32(rawClientTactic));

	m_tactic = serverTactic;
	sCombatSys.SetTactic(m_goId, serverTactic);
}

//0x44 - leave combat / withdraw from interlock
void PlayerObject::RPC_HandleLeaveCombat( ByteBuffer &srcCmd )
{
	DEBUG_LOG(format("(%1%) %2%:%3% leaving combat")
		% m_parent.Address() % m_handle % m_goId);

	sCombatSys.StopFreeFire(m_goId);
	sCombatSys.LeaveInterlock(m_goId);
}

//0x50 - duel request
void PlayerObject::RPC_HandleDuelRequest( ByteBuffer &srcCmd )
{
	ByteBuffer extraData = ByteBuffer(&srcCmd.contents()[srcCmd.rpos()],srcCmd.remaining());
	DEBUG_LOG(format("(%1%) %2%:%3% duel request data: %4%")
		% m_parent.Address() % m_handle % m_goId % Bin2Hex(extraData));

	m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
		"{c:FFFF00}Dueling is not implemented yet - PvP is always on for now.{/c}")));
}

//0x80b9 - use an ability on a target
void PlayerObject::RPC_HandleAbilityUse( ByteBuffer &srcCmd )
{
	uint16 abilityId = srcCmd.read<uint16>();
	uint16 targetViewId = 0;
	if (srcCmd.remaining() >= sizeof(targetViewId))
		targetViewId = srcCmd.read<uint16>();

	if (srcCmd.remaining() > 0)
	{
		ByteBuffer extraData = ByteBuffer(&srcCmd.contents()[srcCmd.rpos()],srcCmd.remaining());
		DEBUG_LOG(format("(%1%) %2%:%3% ability %4% on view %5% extra: %6%")
			% m_parent.Address() % m_handle % m_goId % abilityId % targetViewId % Bin2Hex(extraData));
	}

	uint32 targetGoId = sObjMgr.getGOForView(&m_parent,targetViewId);
	if (targetGoId == 0)
		targetGoId = m_targetGoId;
	if (targetGoId == 0 && m_ilPartner != 0)
		targetGoId = m_ilPartner;

	// Auto-acquire nearest hostile entity if no target is currently locked
	if (targetGoId == 0)
	{
		auto nearby = sSpatialGrid.GetClientsInRadius(m_pos.x, m_pos.z, 1500.0f);
		double closestDistSq = 1500.0 * 1500.0;
		for (GameClient* gc : nearby)
		{
			if (!gc) continue;
			uint32 otherGoId = gc->GetPlayerGoId();
			if (otherGoId == 0 || otherGoId == m_goId) continue;
			PlayerObject* po = sObjMgr.getGOPtrSafe(otherGoId);
			if (po && !po->isDead() && (po->getFaction() == FACTION_MACHINES || po->getFactionName() == "Machines" || m_lastDojoBotGoId == otherGoId))
			{
				double dSq = m_pos.DistanceSq(po->getPosition());
				if (dSq < closestDistSq)
				{
					closestDistSq = dSq;
					targetGoId = otherGoId;
				}
			}
		}
		if (targetGoId != 0)
		{
			m_targetGoId = targetGoId;
		}
	}

	if (targetGoId != 0)
	{
		PlayerObject* targetObj = sObjMgr.getGOPtrSafe(targetGoId);
		if (targetObj)
		{
			ensureEntityKnown(targetObj);
		}
	}

	INFO_LOG(format("(%1%) %2%:%3% UseAbility request ability %4% view %5% -> target go %6%")
		% m_parent.Address() % m_handle % m_goId % abilityId % targetViewId % targetGoId);

	//cooldown bookkeeping when the ability is part of the player's loadout
	if (m_abilitySystem && m_abilitySystem->getAbility(abilityId))
		m_abilitySystem->onAbilityCast(abilityId);

	// Update active martial arts discipline style when an ability of that style is executed
	const AbilityTemplate* abilTempl = sDataLoader.GetAbilityTemplate(abilityId);
	if (abilTempl)
	{
		if (abilTempl->name.find("KungFu") != std::string::npos)
		{
			if (m_fightingStyle != FightingStyle::KungFu)
			{
				m_fightingStyle = FightingStyle::KungFu;
				if (!m_parent.isBot())
					m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style engaged: Kung Fu (Wushu){/c}"));
			}
		}
		else if (abilTempl->name.find("Karate") != std::string::npos)
		{
			if (m_fightingStyle != FightingStyle::Karate)
			{
				m_fightingStyle = FightingStyle::Karate;
				if (!m_parent.isBot())
					m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style engaged: Karate{/c}"));
			}
		}
		else if (abilTempl->name.find("Aikido") != std::string::npos)
		{
			if (m_fightingStyle != FightingStyle::Aikido)
			{
				m_fightingStyle = FightingStyle::Aikido;
				if (!m_parent.isBot())
					m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style engaged: Aikido{/c}"));
			}
		}
		else if (abilTempl->name.find("SelfDefense") != std::string::npos ||
		         abilTempl->name.find("CloseCombat") != std::string::npos ||
		         abilTempl->name.find("MartialArts") != std::string::npos)
		{
			if (m_fightingStyle != FightingStyle::None)
			{
				m_fightingStyle = FightingStyle::None;
				if (!m_parent.isBot())
					m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style engaged: Self-Defense / Close Combat{/c}"));
			}
		}
	}

	sCombatSys.UseAbility(this,abilityId,targetGoId);
}

//0x80ae - client loads/unloads abilities into memory (hotbar loadout)
//request: [staticObjId:4] [unloadFlag:2] [loadFlag:2] [count:2] then per
//ability: [slot:2] [abilityId:2] [pad:2] [level:2] [pad:1]
void PlayerObject::RPC_HandleAbilityLoad( ByteBuffer &srcCmd )
{
	ByteBuffer rawCopy = ByteBuffer(&srcCmd.contents()[srcCmd.rpos()],srcCmd.remaining());

	uint32 staticObjId = srcCmd.read<uint32>();
	uint16 unloadFlag = srcCmd.read<uint16>();
	uint16 loadFlag = srcCmd.read<uint16>();
	uint16 countAbilities = srcCmd.read<uint16>();

	INFO_LOG(format("(%1%) %2%:%3% ability loader: obj %4% unload %5% load %6% count %7% raw %8%")
		% m_parent.Address() % m_handle % m_goId
		% staticObjId % unloadFlag % loadFlag % countAbilities % Bin2Hex(rawCopy));

	if (countAbilities > 20) //sanity
		return;

	//HDS PlayerHandler.ProcessLoadAbility starts the entries at index 11: one unread byte
	//follows the 10-byte header
	if (srcCmd.remaining() > 0)
		srcCmd.read<uint8>();

	for (uint16 i=0;i<countAbilities;i++)
	{
		if (srcCmd.remaining() < 9)
			break;

		uint16 slotId = srcCmd.read<uint16>();
		uint16 abilityId = srcCmd.read<uint16>();
		srcCmd.read<uint16>(); //pad
		uint16 abilityLevel = srcCmd.read<uint16>();
		srcCmd.read<uint8>(); //pad

		if (unloadFlag > 0)
		{
			if (m_abilitySystem)
				m_abilitySystem->unloadAbility(abilityId);
			m_parent.QueueCommand(shared_ptr<AbilityUnloadRspMsg>(new AbilityUnloadRspMsg(abilityId)));
		}
		else
		{
			if (m_abilitySystem)
			{
				if (m_abilitySystem->loadAbility(abilityId, abilityLevel, slotId))
				{
					m_parent.QueueCommand(shared_ptr<AbilityLoadRspMsg>(new AbilityLoadRspMsg(abilityId, abilityLevel, slotId)));
				}
				else
				{
					if (!m_parent.isBot())
						m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}[MEMORY] Insufficient memory capacity to load ability.{/c}"));
				}
			}
		}

		DEBUG_LOG(format("%1%:%2% %3% ability %4% level %5% slot %6%")
			% m_handle % m_goId % (unloadFlag > 0 ? "unloaded" : "loaded")
			% abilityId % abilityLevel % slotId);
	}

	if (m_abilitySystem)
		m_abilitySystem->saveToDB();
}




// crowd-control state: hacker System Stun etc. CC resistance shortens stuns.
bool PlayerObject::isStunned() const
{
	return getMSTime() < m_stunExpiresMS;
}

void PlayerObject::applyStun(uint32 durationMs)
{
	uint32 until = getMSTime() + durationMs;
	if (until > m_stunExpiresMS)
		m_stunExpiresMS = until;
}

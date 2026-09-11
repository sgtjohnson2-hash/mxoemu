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
#include "InventorySystem.h"

std::map<uint32, std::vector<LocationVector>> PlayerObject::s_hardlineCache;

void PlayerObject::LoadHardlines()
{
    INFO_LOG("Loading Hardlines into cache...");
    PreparedStatement stmt("SELECT `DistrictId`,`X`,`Y`,`Z`,`ROT` FROM `hardlines`");
    scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
    if (result)
    {
        do
        {
            Field *field = result->Fetch();
            uint32 districtId = field[0].GetUInt32();
            LocationVector hlPos(field[1].GetDouble(), field[2].GetDouble(), field[3].GetDouble());
            hlPos.rot = field[4].GetDouble();
            s_hardlineCache[districtId].push_back(hlPos);
        } while (result->NextRow());
    }
    INFO_LOG(format("Loaded %1% hardline districts.") % s_hardlineCache.size());
}

// ---------------------------------------------------------------------------
// combat state transitions
// ---------------------------------------------------------------------------

void PlayerObject::enterInterlock( uint32 partnerGoId )
{
	m_ilPartner = partnerGoId;
	m_inCombat = true;
	setCombatStance(true);
	m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
		"{c:FF8800}You are now in Interlock. Tactics: &tactic speed|power|grab|block - withdraw with &withdraw{/c}")));
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

	// Apply basic mitigation based on player level
	uint16 mitigation = m_lvl / 2;
	uint16 actualDamage = (damage > mitigation) ? (damage - mitigation) : 1;

	if (actualDamage >= m_healthC)
		m_healthC = 0;
	else
		m_healthC -= actualDamage;


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
	try
	{
		PlayerObject *killer = sObjMgr.getGOPtr(killerGoId);
		if (killer) {
			killerName = killer->getHandle();
			uint32 killerFaction = killer->getFaction();
			uint32 victimFaction = getFaction();
			sFactionWarMgr.registerPvPKill(killerFaction, victimFaction);
		}
	}
	catch (...) {}

	INFO_LOG(format("Player %1%:%2% was defeated by %3%") % m_handle % m_goId % killerName);

	m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg(
		(format("{c:FF0000}You have been defeated by %1%. Emergency jack-out in progress...{/c}") % killerName).str() )));
	sGame.AnnounceCommand(&m_parent,shared_ptr<SystemChatMsg>(new SystemChatMsg(
		(format("{c:FF4444}%1% has been defeated by %2%.{/c}") % m_handle % killerName).str() )));

	//IsDead attribute so clients render the death state on our views
	sGame.AnnounceStateUpdate(&m_parent,shared_ptr<HealthUpdateMsg>(new HealthUpdateMsg(m_goId,false,true)), true);
	sendVitals(false,true);
	setCombatStance(false);
    
    // V17: The Loot Engine (Only bots drop loot)
    if (m_parent.isBot())
    {
        try {
            PlayerObject *killer = sObjMgr.getGOPtr(killerGoId);
            if (killer && !killer->isDead())
            {
                // Advance mission objective
                sMissionSys.AdvanceObjective(killer, ObjectiveCommand::LOOT, m_goId);
                
                // Random item drop
                const auto& allItems = sDataLoader.GetAllItems();
                if (!allItems.empty() && killer->getInventory()) {
                    auto it = allItems.begin();
                    std::advance(it, rand() % allItems.size());
                    killer->getInventory()->addItem(make_shared<Item>(sObjMgr.getNewObjectId(), it->second.templateId), 1);
                    if (!killer->getClient().isBot()) {
                        killer->getClient().QueueCommand(make_shared<SystemChatMsg>(
                            (format("{c:00FF00}Loot Received: %1%{/c}") % it->second.name).str()));
                    }
                }
            }
        } catch (...) {}
    }
    
	sAdaptiveMusicSystem.clearThreat(m_goId, getMSTime());

	setOnlineStatus(false);

	//downed visual - the jackout beam doubles as the emergency-jackout effect
	sGame.AnnounceStateUpdate(NULL,shared_ptr<JackoutEffectMsg>(new JackoutEffectMsg(m_goId,true)));

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

	//clear the downed visual
	sGame.AnnounceStateUpdate(NULL,shared_ptr<JackoutEffectMsg>(new JackoutEffectMsg(m_goId,false)));

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

	DEBUG_LOG(format("(%1%) %2%:%3% close combat request view %4% spawn %5% -> target go %6%")
		% m_parent.Address() % m_handle % m_goId % targetViewId % spawnCounter % targetGoId);

	if (targetGoId == 0)
	{
		m_parent.QueueCommand(shared_ptr<SystemChatMsg>(new SystemChatMsg("{c:FF0000}No valid interlock target.{/c}")));
		return;
	}

	if (sCombatSys.RequestInterlock(m_goId,targetGoId) == false)
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
	uint8 newTactic = srcCmd.read<uint8>();

	DEBUG_LOG(format("(%1%) %2%:%3% changing combat tactic to %4%")
		% m_parent.Address() % m_handle % m_goId % uint32(newTactic));

	m_tactic = newTactic;
	sCombatSys.SetTactic(m_goId,newTactic);
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

	//cooldown bookkeeping when the ability is part of the player's loadout
	if (m_abilitySystem && m_abilitySystem->getAbility(abilityId))
		m_abilitySystem->onAbilityCast(abilityId);

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
				m_abilitySystem->loadAbility(abilityId,abilityLevel,slotId);
			m_parent.QueueCommand(shared_ptr<AbilityLoadRspMsg>(new AbilityLoadRspMsg(abilityId,abilityLevel,slotId)));
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

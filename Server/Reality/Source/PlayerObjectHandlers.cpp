// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
// Copyright (C) 2006-2010 Rajko Stojadinovic
// http://mxoemu.info
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ---------------------------------------------------------------------------
//
// ***************************************************************************

#include "Common.h"
#include "MissionSystem.h"
#include "FactionWarManager.h"
#include "OrganizationManager.h"
#include "WeatherSystem.h"
#include "BotManager.h"
#include "SocketSystem.h"
#include "CraftingSystem.h"
#include "EconomySystem.h"
#include "PlayerObject.h"
#include "Log.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"
#include "Timer.h"
#include "ObjectMgr.h"
#include "SpatialGrid.h"
#include "CombatSystem.h"
#include "DojoSpawn.h"
#include <iomanip>
#include "GameServer.h"
#include "GameClient.h"
#include "Config.h"
#include "BotManager.h"
#include "DataLoader.h"
#include "InventorySystem.h"
#include "Item.h"
#include "ItemSerializer.h"
#include "WorldDirector.h"
#include "LogisticsManager.h"
#include "AI/MatrixThreatHeatmap.h"
#include "HovercraftFlightSystem.h"
#include "FrankCastleManager.h"
#include "LoadingConstruct.h"
#include "BackdoorNetwork.h"
#include "OracleDialogueTree.h"
#include "OracleSanctuarySystem.h"
#include "OracleCookieSystem.h"
#include "OracleVisionSimulacra.h"
#include "StatusEffectManager.h"
#include "NeuralVoiceSystem.h"
#include "RadioDispatchSystem.h"
#include "APUCombatSystem.h"
#include "RedpillAwakeningSystem.h"
#include "MachineCitySystem.h"
#include "FreewayCombatSystem.h"
#include "MobilAveRailSystem.h"
#include "CyberdeckHackingSystem.h"
#include "ClubHelRaidSystem.h"
#include "PodHarvestSystem.h"
#include "OrbitalSatelliteSystem.h"
#include "SourceCodeCompilerSystem.h"
#include "UnderworldManager.h"
#include "CityLifeManager.h"
#include "EmergentPoliceManager.h"
#include "MafiaEcosystemManager.h"
#include "ExileChateauManager.h"
#include "AgentPossessionManager.h"
#include "AI/PedestrianEcology.h"

#include <boost/algorithm/string.hpp>
using boost::iequals;

void PlayerObject::RPC_NullHandle( ByteBuffer &srcCmd )
{
	return;
}

void PlayerObject::RPC_HandleReadyForSpawn( ByteBuffer &srcCmd )
{
	if (!m_spawnedInWorld)
	{
		this->SpawnSelf();
	}
}

void PlayerObject::ParseAdminCommand( string theCmd )
{
	stringstream cmdStream;
	cmdStream.str(theCmd);

	string command;
	cmdStream >> command;

	if (cmdStream.fail())
		return;

	if (iequals(command, "teleportPlayer") || iequals(command, "bringPlayer"))
	{
		string playerName;
		cmdStream >> playerName;

		if (cmdStream.fail() || playerName.length() < 1)
			return;

		if (iequals(command, "teleportPlayer") && cmdStream.eof())
			return;

		PlayerObject* theTargetPlayer = NULL;
		{
			vector<uint32> allObjects = sObjMgr.getAllGOIds();
			foreach(uint32 objId, allObjects)
			{
				PlayerObject* playerObj = NULL;
				try
				{
					playerObj = sObjMgr.getGOPtr(objId);
				}
				catch (...)
				{
					continue;
				}

				if (playerObj && iequals(playerName,playerObj->getHandle()))
				{
					theTargetPlayer = playerObj;
					break;
				}
			}
		}

		if (theTargetPlayer == NULL)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Player %1% is not online")%playerName).str()));
			return;
		}

		LocationVector derp;

		if (iequals(command, "teleportPlayer"))
		{
			double x,y,z;
			cmdStream >> x;
			if (cmdStream.eof() || cmdStream.fail())
				return;
			cmdStream >> y;
			if (cmdStream.eof() || cmdStream.fail())
				return;
			cmdStream >> z;
			if (cmdStream.fail())
				return;

			x*=100;
			y*=100;
			z*=100;

			derp.ChangeCoords(x,y,z);
		}
		else if (iequals(command, "bringPlayer"))
		{
			LocationVector newPos = this->getPosition();
			derp.ChangeCoords(newPos.x,newPos.y,newPos.z,newPos.getMxoRot());
		}

		theTargetPlayer->setPosition(derp);
		sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(sObjMgr.getGOId(theTargetPlayer)));
		return;
	}
	else if (iequals(command, "mutateBot"))
	{
		string playerName;
		cmdStream >> playerName;
		if (cmdStream.fail() || playerName.length() < 1) return;

		PlayerObject* target = NULL;
		for (uint32 objId : sObjMgr.getAllGOIds()) {
			PlayerObject* po = sObjMgr.getGOPtr(objId);
			if (po && iequals(playerName, po->getHandle())) {
				target = po;
				break;
			}
		}

        if (target && target->getClient().isBot()) {
            auto bot = sBotMgr.GetBotByGOID(target->getGoId());
            if (bot) {
                uint32 newProfile = rand() % 256;
                const BotPersonality* newP = sDataLoader.GetPersonalityProfile(newProfile);
                bot->SetPersonality(newP);
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Mutated bot %1% to %2%") % playerName % newP->vibe).str()));
            }
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Bot %1% not found") % playerName).str()));
        }
		return;
	}
    else if (iequals(command, "setBotFaction"))
    {
        string playerName;
        string newFaction;
        cmdStream >> playerName;
        cmdStream >> newFaction;
        if (cmdStream.fail() || playerName.length() < 1 || newFaction.length() < 1) return;

        PlayerObject* target = NULL;
		for (uint32 objId : sObjMgr.getAllGOIds()) {
			PlayerObject* po = sObjMgr.getGOPtr(objId);
			if (po && iequals(playerName, po->getHandle())) {
				target = po;
				break;
			}
		}

        if (target && target->getClient().isBot()) {
            target->setFactionName(newFaction);
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Set bot %1% faction to %2%") % playerName % newFaction).str()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Bot %1% not found") % playerName).str()));
        }
        return;
    }
    else if (iequals(command, "wipeBotInventory"))
    {
        string playerName;
        cmdStream >> playerName;
        if (cmdStream.fail() || playerName.length() < 1) return;

        PlayerObject* target = NULL;
		for (uint32 objId : sObjMgr.getAllGOIds()) {
			PlayerObject* po = sObjMgr.getGOPtr(objId);
			if (po && iequals(playerName, po->getHandle())) {
				target = po;
				break;
			}
		}

        if (target && target->getClient().isBot()) {
            if (target->getInventory()) {
                target->getInventory()->clear();
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Wiped inventory of bot %1%") % playerName).str()));
            }
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Bot %1% not found") % playerName).str()));
        }
        return;
    }
    else if (iequals(command, "mission") || iequals(command, "procmission"))
    {
        uint32 mId = sMissionSys.GenerateFactionTensionMission(this);
        if (mId > 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}[OPERATOR] Procedural contract synthesized: ID #%1%{/c}") % mId).str()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[OPERATOR] You already have an active mission contract or could not synthesize at this time.{/c}"));
        }
        return;
    }
    else if (iequals(command, "dojo"))
    {
        ParsePlayerCommand(theCmd);
        return;
    }
    else if (iequals(command, "frank") || iequals(command, "frankStatus") || iequals(command, "punisher"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sFrankCastleMgr.GenerateStatusReport()));
        return;
    }
    else if (iequals(command, "frankHuntAgent"))
    {
        sFrankCastleMgr.ScanForAgentsAndThreats();
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[Frank Castle] Tactical radar scan initiated. Hunting nearby Agents.{/c}"));
        return;
    }
    else if (iequals(command, "frankTakeSafehouse"))
    {
        uint32 shId = 1;
        cmdStream >> shId;
        bool res = sFrankCastleMgr.CaptureSafehouse(shId);
        if (res) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}[Frank Castle] Safehouse %1% captured.{/c}") % shId).str()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[Frank Castle] Safehouse ID not found.{/c}"));
        }
        return;
    }
	else if (iequals(command, "teleportAll") || iequals(command, "bringAll"))
	{
		LocationVector derp;

		if (iequals(command, "teleportAll"))
		{
			double x,y,z;
			cmdStream >> x;
			if (cmdStream.eof() || cmdStream.fail())
				return;
			cmdStream >> y;
			if (cmdStream.eof() || cmdStream.fail())
				return;
			cmdStream >> z;
			if (cmdStream.fail())
				return;

			x*=100;
			y*=100;
			z*=100;

			derp.ChangeCoords(x,y,z);
		}
		else if (iequals(command, "bringAll"))
		{
			LocationVector newPos = this->getPosition();
			derp.ChangeCoords(newPos.x,newPos.y,newPos.z,newPos.getMxoRot());
		}

		vector<uint32> allObjects = sObjMgr.getAllGOIds();
		foreach(uint32 objId, allObjects)
		{
			PlayerObject* playerObj = NULL;
			try
			{
				playerObj = sObjMgr.getGOPtr(objId);
			}
			catch (std::exception)
			{
				continue;
			}

			if (playerObj == NULL)
				continue;

			playerObj->setPosition(derp);
			sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(objId));
			playerObj->PopulateWorld();

		}
		return;
	}
	else if (iequals(command, "set"))
	{
		string area;
		cmdStream >> area;

		if (cmdStream.fail()) 
			return;


		std::string s;
		std::stringstream out;
		out << int(m_district);
		s = out.str();

		double X,Y,Z;
		X = this->getPosition().x;
		Y = this->getPosition().y;
		Z = this->getPosition().z;

		string sql1 = (format("DELETE FROM `locations` Where `District` = '%1%' And `Command` = '%2%'") % s % area ).str();
		string sql2 = (format("INSERT INTO `locations` SET `District` = '%1%', `Command` = '%2%', X = '%3%', Y = '%4%', Z = '%5%'") % s % area % X % Y % Z ).str();
		sDatabase.Execute(sql1);
		sDatabase.Execute(sql2);
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}New location set (async), test it out.{/c}"));
		return;


	}
	else if (iequals(command, "simTimeSet") || iequals(command, "simTimeInc"))
	{
		float newSimTime;
		cmdStream >> newSimTime;

		if (iequals(command,"simTimeSet"))
			sGame.SetSimTime(newSimTime);
		else
			sGame.IncreaseSimTime(newSimTime);

		sGame.AnnounceCommand(NULL,make_shared<BroadcastMsg>((format("Simtime set to %1%")%sGame.GetSimTime()).str()));
		return;
	}
	else if (iequals(command, "setHL"))
	{
		string hardlineId;
		cmdStream >> hardlineId;

		if (cmdStream.fail()) 
			return;

		string hardlineName;
		cmdStream >> hardlineName;

		if (cmdStream.fail()) 
			return;

		std::string s;
		std::stringstream out;
		out << int(m_district);
		s = out.str();

		double X,Y,Z,O;
		X = this->getPosition().x;
		Y = this->getPosition().y;
		Z = this->getPosition().z;
		O = this->getPosition().rot;

		string sql1 = (format("DELETE FROM `hardlines` WHERE `DistrictId` = '%1%' AND `HardlineId` = '%2%'") % s % hardlineId ).str();
		string sql2 = (format("INSERT INTO `hardlines` SET `DistrictId` = '%1%', `HardlineId`='%2%',`X`='%3%',`Y`='%4%',`Z`= '%5%',`HardlineName`='%6%',`ROT`='%7%'") % s % hardlineId % X % Y % Z % hardlineName % O).str();
		sDatabase.Execute(sql1);
		sDatabase.Execute(sql2);
		string msg1 = (format("{c:FFFF00}HardlineId:%1% Set to %2% at X:%3% Y:%4% Z:%5% O:%6% (async){/c}") % hardlineId % hardlineName % X % Y % Z % O ).str();
		m_parent.QueueCommand(make_shared<SystemChatMsg>(msg1));
		return;


	}
	else if (iequals(command, "giveitem"))
	{
		uint32 templateId;
		cmdStream >> templateId;
		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !giveitem <templateId>{/c}"));
			return;
		}
		
		if (getInventory()) {
			uint32 newGoId = sObjMgr.getNewItemId();
			auto item = std::make_shared<Item>(newGoId, templateId);
			std::string meta = ItemSerializer::Serialize(item);
			item->setMetadata(meta);
			getInventory()->addItemAuto(item);
			if (!getClient().isBot()) {
				getInventory()->saveToDB();
			}
			const ItemTemplate* tpl = sDataLoader.GetItemTemplate(templateId);
			std::string itemName = tpl ? tpl->name : "Item";
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Added [%1%] (Tpl %2%, GOID %3%) to inventory.{/c}") % itemName % templateId % newGoId).str()));
		}
		return;
	}
	else if (iequals(command, "setlevel"))
	{
		uint32 newLevel;
		cmdStream >> newLevel;
		if (cmdStream.fail() || newLevel > 50) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !setlevel <level> (max 50){/c}"));
			return;
		}
		
		m_lvl = newLevel;
		m_exp = newLevel * 1000;
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Level set to %1%.{/c}") % newLevel).str()));
		return;
	}
	else if (iequals(command, "setrep"))
	{
		uint32 newRep;
		cmdStream >> newRep;
		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !setrep <amount>{/c}"));
			return;
		}
		
		setFactionReputation(newRep);
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Faction reputation set to %1%.{/c}") % newRep).str()));
		return;
	}
	else if (iequals(command, "spawnmob"))
	{
		uint32 templateId;
		cmdStream >> templateId;
		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !spawnmob <templateId>{/c}"));
			return;
		}
		
		uint32 goId = sObjMgr.constructPlayer(&m_parent, 9000000 + rand()%100000, true);
		PlayerObject* bot = sObjMgr.getGOPtr(goId);
		if (bot) {
			LocationVector spawnLoc = this->getPosition();
			spawnLoc.x += 100.0f;
			bot->setPosition(spawnLoc);
			bot->PopulateWorld();
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Spawned mob template %1% at ID %2%.{/c}") % templateId % goId).str()));
		}
		return;
	}
    else if (iequals(command, "sysbroadcast"))
    {
        string message;
        std::getline(cmdStream, message);
        if (message.empty()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !sysbroadcast <message>{/c}"));
            return;
        }
        // Send a yellow alert to all players
        sGame.AnnounceCommand(NULL, make_shared<SystemChatMsg>((format("{c:FFFF00}[SYS]%1%{/c}") % message).str()));
        return;
    }
    else if (iequals(command, "botstress"))
    {
        int count = 10;
        cmdStream >> count;
        sBotMgr.BotStressTest(count);
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Spawning %1% bots for stress test...{/c}") % count).str()));
        return;
    }
    else if (iequals(command, "botpossess"))
    {
        uint32 targetGoId;
        cmdStream >> targetGoId;
        if (cmdStream.fail()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !botpossess <goId>{/c}"));
            return;
        }
        
        auto bot = sBotMgr.GetBotByGOID(targetGoId);
        if (!bot) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Bot not found!{/c}"));
            return;
        }
        
        // If we were already possessing one, release it
        if (m_possessingBotId != 0) {
            auto oldBot = sBotMgr.GetBotByGOID(m_possessingBotId);
            if (oldBot) oldBot->SetPossessedBy(0);
        }
        
        m_possessingBotId = targetGoId;
        bot->SetPossessedBy(m_goId);
        
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Possessing Bot %1%. Your chat and movements are now overridden.{/c}") % targetGoId).str()));
        return;
    }
    else if (iequals(command, "botrelease"))
    {
        if (m_possessingBotId == 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not possessing any bot.{/c}"));
            return;
        }
        auto bot = sBotMgr.GetBotByGOID(m_possessingBotId);
        if (bot) bot->SetPossessedBy(0);
        
        m_possessingBotId = 0;
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Released possession of bot.{/c}"));
        return;
    }
    // Item 116: Live Event Orchestrator
    else if (iequals(command, "event"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "weather")) {
            float intensity = 0.5f;
            if (!cmdStream.eof()) cmdStream >> intensity;
            sWeatherSys.TriggerGlitchAnomaly(intensity, 600000); // 10 mins
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Triggered Weather Glitch Anomaly.{/c}"));
        } else if (iequals(subCmd, "spawn")) {
            string factionStr = "Machines";
            if (!cmdStream.eof()) cmdStream >> factionStr;
            int faction = FACTION_MACHINES;
            if (iequals(factionStr, "Zion")) faction = FACTION_ZION;
            else if (iequals(factionStr, "Merovingian")) faction = FACTION_MEROVINGIAN;
            
            auto pos = getPosition();
            sBotMgr.SpawnBot(1, pos.x, pos.y, pos.z, faction);
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Spawned %1% bot.{/c}") % factionStr).str()));
        } else if (iequals(subCmd, "bounty")) {
            m_hasBounty = true;
            m_pvpflag = true;
            string bountyMsg = (format("{c:FF0000}[Bounty] %1% is now marked as a rogue by Admin command!{/c}") % getHandle()).str();
            auto players = sObjMgr.getAllGOIds();
            for (auto id : players) {
                PlayerObject* p = sObjMgr.getGOPtr(id);
                if (p) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(bountyMsg));
            }
        }
        return;
    }
    else if (iequals(command, "frank") || iequals(command, "punisher") || iequals(command, "frankstatus"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "status") || subCmd.empty()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sFrankCastleMgr.GenerateStatusReport()));
        } else if (iequals(subCmd, "hunt")) {
            sFrankCastleMgr.ScanForAgentsAndThreats();
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Admin: Triggered Agent scan for Frank Castle.{/c}"));
        } else if (iequals(subCmd, "safehouse")) {
            uint32 shId = 0;
            cmdStream >> shId;
            if (shId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !frank safehouse <id>{/c}"));
            } else {
                bool res = sFrankCastleMgr.CaptureSafehouse(shId);
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Admin: Frank Castle safehouse capture result: %1%{/c}") % (res ? "Success" : "Failed")).str()));
            }
        } else if (iequals(subCmd, "respawn")) {
            sFrankCastleMgr.TriggerFieldSurgeryAndRespawn();
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Admin: Frank Castle field surgery & respawn triggered.{/c}"));
        } else if (iequals(subCmd, "xp")) {
            uint64 xp = 10000;
            cmdStream >> xp;
            sFrankCastleMgr.AwardExperience(xp);
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Admin: Awarded %1% XP to Frank Castle. Current Level: %2%{/c}") % xp % (uint32)sFrankCastleMgr.GetLevel()).str()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !frank <status | hunt | safehouse <id> | respawn | xp <amount>>{/c}"));
        }
        return;
    }
    else if (iequals(command, "inspect") || iequals(command, "bio") ||
             iequals(command, "social") || iequals(command, "love") || iequals(command, "friendships") ||
             iequals(command, "memories") || iequals(command, "memory") || iequals(command, "socialstats") ||
             iequals(command, "family") || iequals(command, "household") ||
             iequals(command, "dreams") || iequals(command, "dream") || iequals(command, "aspiration") ||
             iequals(command, "career") || iequals(command, "job") || iequals(command, "vocation") ||
             iequals(command, "life") || iequals(command, "lifedossier") || iequals(command, "dossier") ||
             iequals(command, "lifestats") || iequals(command, "familystats") ||
             iequals(command, "emergentlife") || iequals(command, "emergentlifestyle") ||
             iequals(command, "awakening") || iequals(command, "epiphany") ||
             iequals(command, "gossip") || iequals(command, "rumors") ||
             iequals(command, "socialcircles") || iequals(command, "circles"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[System] Subsystem offline.{/c}"));
        return;
    }
    else if (iequals(command, "underworld") || iequals(command, "syndicate"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "bosses")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sUnderworldMgr.GenerateBossHierarchyReport()));
        } else if (iequals(subCmd, "turf")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sUnderworldMgr.GenerateTurfGridReport()));
        } else if (iequals(subCmd, "crimes")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sUnderworldMgr.GenerateEmergentCrimesReport()));
        } else if (iequals(subCmd, "precincts")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sUnderworldMgr.GeneratePolicePrecinctsReport()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sUnderworldMgr.GenerateUnderworldStatusReport()));
        }
        return;
    }
    else if (iequals(command, "police") || iequals(command, "swat"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "squads")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentPoliceMgr.GenerateTacticalSquadsReport()));
        } else if (iequals(subCmd, "overwatch") || iequals(subCmd, "snipers")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentPoliceMgr.GenerateOverwatchReport()));
        } else if (iequals(subCmd, "roadblocks")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentPoliceMgr.GenerateRoadblocksReport()));
        } else if (iequals(subCmd, "ia") || iequals(subCmd, "stings")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentPoliceMgr.GenerateIAStingsReport()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentPoliceMgr.GeneratePoliceSWATReport()));
        }
        return;
    }
    else if (iequals(command, "citylife") || iequals(command, "simulation"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "demo") || iequals(subCmd, "demographics")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sCityLifeMgr.GenerateDemographicsReport()));
        } else if (iequals(subCmd, "transit") || iequals(subCmd, "subway") || iequals(subCmd, "traffic")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sCityLifeMgr.GenerateTransitReport()));
        } else if (iequals(subCmd, "commerce") || iequals(subCmd, "shops")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sCityLifeMgr.GenerateCommerceReport()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sCityLifeMgr.GenerateCityLifeStatusReport()));
        }
        return;
    }
    else if (iequals(command, "emergent") || iequals(command, "affordances"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[System] Subsystem offline.{/c}"));
        return;
    }
    else if (iequals(command, "mafia") || iequals(command, "commission") || iequals(command, "pizzo"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "commission")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateCommissionReport()));
        } else if (iequals(subCmd, "pizzo")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GeneratePizzoExtortionReport()));
        } else if (iequals(subCmd, "marcone")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateFamilyDossier(MafiaFamilyId::MarconeFamily)));
        } else if (iequals(subCmd, "valenti")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateFamilyDossier(MafiaFamilyId::ValentiFamily)));
        } else if (iequals(subCmd, "scarlotti")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateFamilyDossier(MafiaFamilyId::ScarlottiSyndicate)));
        } else if (iequals(subCmd, "triad") || iequals(subCmd, "chenwu")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateFamilyDossier(MafiaFamilyId::ChenWuTriad)));
        } else if (iequals(subCmd, "bratva") || iequals(subCmd, "petrov")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateFamilyDossier(MafiaFamilyId::PetrovBratva)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sMafiaMgr.GenerateMafiaWorldReport()));
        }
        return;
    }
    else if (iequals(command, "exile") || iequals(command, "chateau") || iequals(command, "clubhel") || iequals(command, "backdoor"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "clubhel") || iequals(subCmd, "hel")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sExileMgr.GenerateClubHelStatusReport()));
        } else if (iequals(subCmd, "backdoor") || iequals(subCmd, "doors") || iequals(subCmd, "portals")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sExileMgr.GenerateBackdoorCorridorsReport()));
        } else if (iequals(subCmd, "mobil") || iequals(subCmd, "train") || iequals(subCmd, "limbo")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sExileMgr.GenerateMobilAveLimboReport()));
        } else if (iequals(subCmd, "bestiary") || iequals(subCmd, "vampires") || iequals(subCmd, "monsters")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sExileMgr.GenerateSupernaturalBestiaryReport()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sExileMgr.GenerateExileCourtReport()));
        }
        return;
    }
	else
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Unrecognized server command %1%")%command).str()));
		return;
	}
}


void PlayerObject::ParsePlayerCommand( string theCmd )
{
	stringstream cmdStream;
	cmdStream.str(theCmd);

	string command;
	cmdStream >> command;

	if (cmdStream.fail())
		return;

	//Developer/cheat commands: teleporting, raw packet injection, free abilities/disciplines,
	//dojo spawns and simulation toggles. These were reachable by any player. They now need
	//adminFlags set on the character (characters.adminFlags).
	{
		static const char* devCommands[] = {
			"discipline", "class", "loadability", "learn", "dojo", "loadout", "botdebug", "giveitem",
			"claimhl", "send", "sendCmd", "socket", "bullettime", "gotoPos", "incX", "incY", "incZ",
			"goThru", "random", "update", "gotoPlayer", "go", "frank", "punisher", "underworld",
			"syndicate", "police", "swat", "citylife", "simulation", "emergent", "bottest", "overwrite", "agentstrike", "agent", NULL };
		for (int i = 0; devCommands[i] != NULL; i++)
		{
			if (iequals(command, devCommands[i]) && !m_isAdmin)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}That command is restricted to admins.{/c}"));
				return;
			}
		}
	}

	using boost::erase_all;
	if (iequals(command, "bottest"))
	{
		// &bottest - status of the tester bots (BotTester.cpp) and refresh bot_test_report.json
		std::string summary = sBotMgr.GetTesterSummary();
		size_t start = 0;
		while (start < summary.size())
		{
			size_t end = summary.find('\n', start);
			if (end == std::string::npos) end = summary.size();
			if (end > start)
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}[BOTTEST] " + summary.substr(start, end - start) + "{/c}"));
			start = end + 1;
		}
		sBotMgr.WriteTesterReport();
		return;
	}
	if (iequals(command, "attack") || iequals(command, "interlock"))
	{
		if (m_targetGoId == 0)
		{
			float bestDistSq = 1500.0f * 1500.0f;
			for (uint32 gid : sObjMgr.getAllGOIds())
			{
				PlayerObject* po = sObjMgr.getGOPtrSafe(gid);
				if (po && po != this && !po->isDead() && (po->getFaction() == FACTION_MACHINES || po->getFactionName() == "Machines"))
				{
					float d = (float)m_pos.DistanceSq(po->getPosition());
					if (d <= bestDistSq)
					{
						bestDistSq = d;
						m_targetGoId = gid;
					}
				}
			}
		}
		if (m_targetGoId == 0)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}No valid target selected.{/c}"));
			return;
		}
		if (m_tactic == TACTIC_NORMAL)
		{
			m_tactic = TACTIC_POWER;
			sCombatSys.SetTactic(m_goId, TACTIC_POWER);
		}
		uint16 targetViewId = sObjMgr.getViewForGO(&m_parent, m_targetGoId);
		uint32 clientTargetRef = uint32(targetViewId) | (uint32(PLAYER_SPAWN_COUNTER) << 16);
		INFO_LOG(format("(%1%) %2%:%3% close combat request view %4% spawn %5% -> target go %6%")
			% m_parent.Address() % m_handle % m_goId % targetViewId % uint32(PLAYER_SPAWN_COUNTER) % m_targetGoId);
		if (!sCombatSys.RequestInterlock(m_goId, m_targetGoId, clientTargetRef))
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Interlock request failed (out of range or already in combat).{/c}"));
		}
		return;
	}
	else if (iequals(command, "tactic"))
	{
		string tacStr;
		cmdStream >> tacStr;
		uint8 tac = TACTIC_NORMAL;
		if (iequals(tacStr, "power")) tac = TACTIC_POWER;
		else if (iequals(tacStr, "speed")) tac = TACTIC_SPEED;
		else if (iequals(tacStr, "grab") || iequals(tacStr, "retaliate")) tac = TACTIC_RETALIATE;
		else if (iequals(tacStr, "block") || iequals(tacStr, "defense")) tac = TACTIC_DEFENSE;
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &tactic <power|speed|grab|block>{/c}"));
			return;
		}
		m_tactic = tac;
		sCombatSys.SetTactic(m_goId, tac);
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Combat tactic set to %1%.{/c}") % tacStr).str()));
		return;
	}
	else if (iequals(command, "style") || iequals(command, "discipline"))
	{
		string styleStr;
		cmdStream >> styleStr;
		if (iequals(styleStr, "kungfu") || iequals(styleStr, "wushu"))
		{
			setFightingStyle(FightingStyle::KungFu);
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Kung Fu (Wushu).{/c}"));
		}
		else if (iequals(styleStr, "karate"))
		{
			setFightingStyle(FightingStyle::Karate);
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Karate.{/c}"));
		}
		else if (iequals(styleStr, "aikido"))
		{
			setFightingStyle(FightingStyle::Aikido);
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Aikido.{/c}"));
		}
		else if (iequals(styleStr, "street") || iequals(styleStr, "selfdefense") || iequals(styleStr, "brawl"))
		{
			setFightingStyle(FightingStyle::None);
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Self-Defense / Close Combat.{/c}"));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &style <kungfu|karate|aikido|street>{/c}"));
		}
		return;
	}
	else if (iequals(command, "discipline") || iequals(command, "class"))
	{
		string discStr;
		cmdStream >> discStr;
		if (m_abilitySystem)
		{
			if (iequals(discStr, "hacker"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(57, 1, 0); // LogicBlast1Ability
				m_abilitySystem->loadAbility(53, 1, 1); // HarmfulCodeAbility
				m_abilitySystem->loadAbility(68, 1, 2); // PersonalFirewall1Ability
				m_abilitySystem->loadAbility(60, 1, 3); // LogicBomb1Ability
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:00FFCC}[DISCIPLINE] Hacker kit loaded into memory: 1:LogicBlast 2:HarmfulCode 3:Firewall 4:LogicBomb{/c}"));
			}
			else if (iequals(discStr, "coder") || iequals(discStr, "support"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(77, 1, 0); // RestoreHealth1Ability
				m_abilitySystem->loadAbility(46, 1, 1); // FastHealing1Ability
				m_abilitySystem->loadAbility(39, 1, 2); // BolsterHealth1Ability
				m_abilitySystem->loadAbility(56, 1, 3); // GroupRepairs1Ability
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:00FF00}[DISCIPLINE] Coder/Support kit loaded into memory: 1:RestoreHealth 2:FastHealing 3:BolsterHealth 4:GroupRepairs{/c}"));
			}
			else if (iequals(discStr, "kungfu") || iequals(discStr, "wushu"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(133, 1, 0);  // KungFuAbility
				m_abilitySystem->loadAbility(570, 1, 1);  // KungFuCombatTacticsAbility
				m_abilitySystem->loadAbility(574, 1, 2);  // KungFuDamageAbility
				m_abilitySystem->loadAbility(8449, 1, 3); // KungFuMasterAbility
				setFightingStyle(FightingStyle::KungFu);
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:00FF00}[DISCIPLINE] Kung Fu (Wushu) kit loaded into memory: 1:KungFu 2:Tactics 3:Damage 4:Master{/c}"));
			}
			else if (iequals(discStr, "karate"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(132, 1, 0);  // KarateAbility
				m_abilitySystem->loadAbility(531, 1, 1);  // KarateFocusAbility
				m_abilitySystem->loadAbility(569, 1, 2);  // KarateCombatTacticsAbility
				m_abilitySystem->loadAbility(573, 1, 3);  // KarateDamageAbility
				setFightingStyle(FightingStyle::Karate);
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:00FF00}[DISCIPLINE] Karate kit loaded into memory: 1:Karate 2:Focus 3:Tactics 4:Damage{/c}"));
			}
			else if (iequals(discStr, "aikido"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(101, 1, 0);  // AikidoAbility
				m_abilitySystem->loadAbility(296, 1, 1);  // AikidoSpinClayPigeonAbility
				m_abilitySystem->loadAbility(571, 1, 2);  // AikidoCombatTacticsAbility
				m_abilitySystem->loadAbility(572, 1, 3);  // AikidoDamageAbility
				setFightingStyle(FightingStyle::Aikido);
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:00FF00}[DISCIPLINE] Aikido kit loaded into memory: 1:Aikido 2:SpinClayPigeon 3:Tactics 4:Damage{/c}"));
			}
			else if (iequals(discStr, "street") || iequals(discStr, "selfdefense") || iequals(discStr, "brawl"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(17, 1, 0);   // SelfDefenseAbility
				m_abilitySystem->loadAbility(197, 1, 1);  // Head Butt
				m_abilitySystem->loadAbility(198, 1, 2);  // Cheap Shot
				m_abilitySystem->loadAbility(600, 1, 3);  // CloseCombatTrainingAbility
				setFightingStyle(FightingStyle::None);
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:00FF00}[DISCIPLINE] Self-Defense / Street Brawl kit loaded: 1:SelfDefense 2:HeadButt 3:CheapShot 4:CloseCombat{/c}"));
			}
			else if (iequals(discStr, "martialartist") || iequals(discStr, "operative"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(600, 1, 0);
				m_abilitySystem->loadAbility(137, 1, 1);
				m_abilitySystem->loadAbility(17, 1, 2);
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:FF8800}[DISCIPLINE] Martial Artist kit loaded into memory: 1:CloseCombat 2:MartialArts 3:SelfDefense{/c}"));
			}
			else if (iequals(discStr, "soldier") || iequals(discStr, "gunner"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(14, 1, 0);  // PowerShotAbility
				m_abilitySystem->loadAbility(129, 1, 1); // HandgunsAbility
				m_abilitySystem->loadAbility(147, 1, 2); // RiflesAbility
				m_abilitySystem->loadAbility(453, 1, 3); // RifleButtSmashAbility
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:FF4444}[DISCIPLINE] Soldier kit loaded into memory: 1:PowerShot 2:Handguns 3:Rifles 4:RifleButtSmash{/c}"));
			}
			else if (iequals(discStr, "spy"))
			{
				m_abilitySystem->clearLoadout();
				m_abilitySystem->loadAbility(209, 1, 0); // StealthAbility
				m_abilitySystem->loadAbility(146, 1, 1); // PoisonKnifeAbility
				m_abilitySystem->loadAbility(283, 1, 2); // KnifeThrowerAbility
				m_abilitySystem->loadAbility(293, 1, 3); // StealthCountermeasuresAbility
				m_abilitySystem->sendFullLoadout();
				m_abilitySystem->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:AA00FF}[DISCIPLINE] Spy kit loaded into memory: 1:Stealth 2:PoisonKnife 3:KnifeThrower 4:Countermeasures{/c}"));
			}
			else
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					"{c:FF0000}Usage: &discipline <kungfu|karate|aikido|street|martialartist|soldier|spy|hacker|coder>{/c}"));
			}
		}
		return;
	}
	else if (iequals(command, "loadability") || iequals(command, "learn"))
	{
		uint16 abId = 0;
		uint16 slot = 0;
		cmdStream >> abId;
		if (cmdStream >> slot) {} else { slot = 0; }
		if (abId > 0 && m_abilitySystem)
		{
			const AbilityTemplate* t = sDataLoader.GetAbilityTemplate(abId);
			m_abilitySystem->loadAbility(abId, 1, slot);
			m_parent.QueueCommand(make_shared<AbilityLoadRspMsg>(abId, 1, slot));
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[ABILITY] Loaded %1% (ID %2%) into slot %3%.{/c}")
					% (t ? t->name : "Ability") % abId % slot).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &loadability <abilityId> [slot]{/c}"));
		}
		return;
	}
	else if (iequals(command, "withdraw") || iequals(command, "escape"))
	{
		sCombatSys.StopFreeFire(m_goId);
		sCombatSys.LeaveInterlock(m_goId);
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Withdrew from combat.{/c}"));
		return;
	}
	else if (iequals(command, "inventory") || iequals(command, "inv"))
	{
		if (m_inventorySystem)
		{
			m_inventorySystem->sendFullInventory();
		}
		return;
	}
	else if (iequals(command, "giveitem"))
	{
		uint32 templateId = 0;
		cmdStream >> templateId;
		if (templateId > 0 && m_inventorySystem)
		{
			uint32 newGoId = sObjMgr.getNewItemId();
			auto item = std::make_shared<Item>(newGoId, templateId);
			std::string meta = ItemSerializer::Serialize(item);
			item->setMetadata(meta);
			m_inventorySystem->addItemAuto(item);
			if (!getClient().isBot())
			{
				m_inventorySystem->saveToDB();
			}
			const ItemTemplate* tpl = sDataLoader.GetItemTemplate(templateId);
			std::string itemName = tpl ? tpl->name : "Item";
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[INVENTORY] Added [%1%] (Tpl %2%, GOID %3%) to inventory.{/c}")
					% itemName % templateId % newGoId).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &giveitem <templateId>{/c}"));
		}
		return;
	}
	else if (iequals(command, "locker") || iequals(command, "vault"))
	{
		string subCmd;
		cmdStream >> subCmd;

		const HardlineNode* hl = PlayerObject::GetNearestHardline(m_district, m_pos.x, m_pos.z);
		if (!hl) hl = PlayerObject::GetNearestHardline(0, m_pos.x, m_pos.z);
		uint32 hlId = hl ? hl->hardlineId : 152;
		double d2 = hl ? ((m_pos.x - hl->x)*(m_pos.x - hl->x) + (m_pos.z - hl->z)*(m_pos.z - hl->z)) : 1e12;
		bool nearHl = (d2 <= (2500.0 * 2500.0)); // 25 meters
		bool canAccess = nearHl || m_isAdmin;

		INFO_LOG(format("HardlineLocker: %1% executed locker command '%2%' (nearHl=%3%, hlId=%4%, admin=%5%)") % m_handle % subCmd % nearHl % hlId % m_isAdmin);

		if (subCmd.empty() || boost::iequals(subCmd, "help") || boost::iequals(subCmd, "list"))
		{
			if (!canAccess)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Access Denied. You must be within 25m of an active Hardline.{/c}"));
				return;
			}
			auto items = sEconomySys.GetLockerItems(getCharacterUID(), hlId);
			if (items.empty())
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FFFF00}[LOCKER] Hardline #%1% vault is empty.{/c}") % hlId).str()));
				return;
			}
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}--- HARDLINE #%1% VAULT (%2% ITEMS) ---{/c}") % hlId % items.size()).str()));
			for (const auto& it : items)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("  [ID: %1%] Slot: %2% | Template: %3% | Qty: %4%")
						% it.entryId % (int)it.slot % it.templateId % it.quantity).str()));
			}
			return;
		}

		if (boost::iequals(subCmd, "store"))
		{
			if (!canAccess)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Access Denied. You must be within 25m of an active Hardline.{/c}"));
				return;
			}
			uint32 invSlot = 0;
			cmdStream >> invSlot;
			if (invSlot == 0 || !m_inventorySystem)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &locker store <invSlot>{/c}"));
				return;
			}
			shared_ptr<Item> item = m_inventorySystem->getItemBySlot(static_cast<uint8>(invSlot));
			if (!item)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}[LOCKER] No item in inventory slot %1%.{/c}") % invSlot).str()));
				return;
			}
			uint32 tId = item->getTemplateId();
			if (sEconomySys.DepositToLocker(this, hlId, tId, static_cast<uint8>(invSlot)))
			{
				INFO_LOG(format("HardlineLocker: %1% deposited item %2% into vault %3%") % m_handle % tId % hlId);
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}[LOCKER] Deposited item (Template #%1%) into Hardline #%2% vault.{/c}") % tId % hlId).str()));
			}
			else
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Failed to deposit item into vault.{/c}"));
			}
			return;
		}

		if (boost::iequals(subCmd, "withdraw"))
		{
			if (!canAccess)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Access Denied. You must be within 25m of an active Hardline.{/c}"));
				return;
			}
			uint64 entryId = 0;
			cmdStream >> entryId;
			if (entryId == 0)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &locker withdraw <entryId>{/c}"));
				return;
			}
			if (sEconomySys.WithdrawFromLocker(this, hlId, entryId))
			{
				INFO_LOG(format("HardlineLocker: %1% withdrew item %2% from vault %3%") % m_handle % entryId % hlId);
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}[LOCKER] Withdrew item (Locker ID #%1%) from Hardline #%2% vault into inventory.{/c}") % entryId % hlId).str()));
			}
			else
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Failed to withdraw item (check entry ID or inventory space).{/c}"));
			}
			return;
		}
		return;
	}
	else if (iequals(command, "dye"))
	{
		string slotStr;
		uint32 colorVal = 0;
		cmdStream >> slotStr >> colorVal;

		if (slotStr.empty())
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}Usage: &dye <coat|shirt|pants|shoes|glasses|hair|skin> <0-31>{/c}"));
			return;
		}

		string colName = "";
		uint32 maxColor = 31;
		if (boost::iequals(slotStr, "coat"))        { colName = "coatcolor"; maxColor = 31; }
		else if (boost::iequals(slotStr, "shirt"))   { colName = "shirtcolor"; maxColor = 63; }
		else if (boost::iequals(slotStr, "pants"))   { colName = "pantscolor"; maxColor = 31; }
		else if (boost::iequals(slotStr, "shoes"))   { colName = "shoecolor"; maxColor = 15; }
		else if (boost::iequals(slotStr, "glasses")) { colName = "glassescolor"; maxColor = 15; }
		else if (boost::iequals(slotStr, "hair"))    { colName = "haircolor"; maxColor = 31; }
		else if (boost::iequals(slotStr, "skin"))    { colName = "skintone"; maxColor = 31; }
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[TAILOR] Unknown wardrobe slot. Valid: coat, shirt, pants, shoes, glasses, hair, skin.{/c}"));
			return;
		}

		if (colorVal > maxColor) colorVal = maxColor;

		if (Database_Main != nullptr)
		{
			try
			{
				PreparedStatement stmt((format("UPDATE `rsivalues` SET `%1%` = ?0 WHERE `charId` = ?1") % colName).str());
				stmt.SetUInt32(0, colorVal);
				stmt.SetUInt64(1, getCharacterUID());
				sDatabase.ExecutePrepared(&stmt);
			} catch (...) {}
		}

		UpdateAppearance();
		INFO_LOG(format("Tailor: %1% dyed %2% to color %3%") % m_handle % slotStr % colorVal);
		m_parent.QueueCommand(make_shared<SystemChatMsg>(
			(format("{c:00FF00}[TAILOR] Your %1% has been dyed to palette color #%2%! Appearance matrix synchronized.{/c}")
				% slotStr % colorVal).str()
		));
		return;
	}
	else if (iequals(command, "capture") || iequals(command, "hacknode"))
	{
		uint32 nodeId = 0;
		cmdStream >> nodeId;
		if (nodeId == 0)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &capture <nodeId>{/c}"));
			return;
		}

		const auto& nodes = sFactionWarMgr.GetControlNodes();
		auto it = nodes.find(nodeId);
		if (it == nodes.end())
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}[FACTION WAR] Control Node #%1% not found in directory.{/c}") % nodeId).str()));
			return;
		}

		const ControlNode& node = it->second;
		double dx = m_pos.x - node.x;
		double dz = m_pos.z - node.z;
		double dist = sqrt(dx * dx + dz * dz);

		INFO_LOG(format("FactionWar: %1% viral capture request for node %2% (dist=%3%m, admin=%4%)") % m_handle % nodeId % (dist / 100.0) % m_isAdmin);

		if (!m_isAdmin && (dx * dx + dz * dz) > (2500.0 * 2500.0)) // 25 meters
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[FACTION WAR] You must be within 25m of the Hardline node to upload viral control code.{/c}"));
			return;
		}

		uint32 myFaction = getFaction();
		if (myFaction == 0 || node.controllingFaction == myFaction)
		{
			myFaction = (node.controllingFaction == FACTION_ZION) ? FACTION_MACHINES : FACTION_ZION;
		}

		float deltaProgress = 0.35f;
		if (m_isAdmin)
		{
			float optProgress = 0.0f;
			if (cmdStream >> optProgress && optProgress > 0.0f)
			{
				deltaProgress = optProgress;
			}
			else
			{
				deltaProgress = 1.0f; // admin capture immediately completes capture!
			}
		}

		INFO_LOG(format("FactionWar: %1% executed viral capture on node %2% (faction %3%, delta=%4%)") % m_handle % nodeId % myFaction % deltaProgress);
		sFactionWarMgr.AdvanceNodeCapture(nodeId, myFaction, deltaProgress, this);
		return;
	}
	else if (iequals(command, "trade"))
	{
		string targetHandle;
		uint32 templateId = 0;
		cmdStream >> targetHandle >> templateId;
		if (targetHandle.empty() || templateId == 0)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &trade <targetName> <templateId>{/c}"));
			return;
		}

		PlayerObject* recipient = nullptr;
		auto allIds = sObjMgr.getAllGOIds();
		for (auto id : allIds)
		{
			PlayerObject* p = sObjMgr.getGOPtrSafe(id);
			if (p && iequals(p->getHandle(), targetHandle))
			{
				recipient = p;
				break;
			}
		}

		if (!recipient)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}Operative '%1%' not found.{/c}") % targetHandle).str()));
			return;
		}

		LocationVector pA = getPosition();
		LocationVector pB = recipient->getPosition();
		double dx = pB.x - pA.x;
		double dz = pB.z - pA.z;
		if ((dx * dx + dz * dz) > (1000.0 * 1000.0))
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Trade partner is out of range (max 10m).{/c}"));
			return;
		}

		if (m_inventorySystem && recipient->getInventory())
		{
			if (m_inventorySystem->consumeItemByTemplate(templateId))
			{
				recipient->giveItem(templateId);
				m_inventorySystem->saveToDB();
				recipient->getInventory()->saveToDB();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}[TRADE] Transferred item %1% to %2%.{/c}") % templateId % targetHandle).str()));
				recipient->getClient().QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}[TRADE] Received item %1% from %2%.{/c}") % templateId % getHandle()).str()));
			}
			else
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[TRADE] You do not possess that item.{/c}"));
			}
		}
		return;
	}
	else if (iequals(command, "dojo"))
	{
		string subCommand;
		cmdStream >> subCommand;
		LocationVector pos = this->getPosition();

		// Spawns one passive, scaled training bot distM metres from the player along
		// (facing + angleOffsetRad), turned to face the player. World units are 100/m and
		// "forward" is (+sin(rot), +cos(rot)) in the authentic client world space (+Z forward).
		auto spawnDojoBot = [&](float distM, float angleOffsetRad, FightingStyle botStyle = FightingStyle::None, const std::string& botName = "", float customHp = 0.0f) -> bool
		{
			const LocationVector botPos = DojoPlaceInFront(pos, distM, angleOffsetRad);
			const double bx = botPos.x;
			const double bz = botPos.z;

			auto bot = sBotMgr.SpawnSingleBot((float)bx, (float)pos.y, (float)bz, FACTION_MACHINES);
			if (!bot)
				return false;
			uint32 botGoId = bot->GetPlayerGoId();
			PlayerObject* botPo = sObjMgr.getGOPtrSafe(botGoId);
			if (!botPo)
				return false;

			// Put dojo bot into the same instance as the summoning player
			bot->m_instanceId = m_parent.m_instanceId;

			// SpawnSingleBot swaps the requested faction for the controlling one at that
			// location - the dojo dummy must always be hostile, so force Machines.
			bot->SetFaction(FACTION_MACHINES);
			botPo->setFactionName("Machines");
			bot->SetPassive(true);
			botPo->setFightingStyle(botStyle);
			if (!botName.empty()) {
				botPo->setHandle(botName);
			}

			// place + face the player
			botPo->setPosition(botPos);
			sSpatialGrid.UpdateClientPosition(bot.get(), (float)bx, (float)bz);

			// Scale the DUMMY (never the player): same level as the player. HP follows a bounded
			// level curve (the player's own damage is not scaled, so it must stay killable in a
			// reasonable number of 4 s interlock rounds); damage scales with the player's HP pool
			// so the dummy stays a threat for characters that have levelled up (+50 HP/level).
			const uint8 botLvl = std::max<uint8>(1, getLevel());
			const float hpF = (customHp > 0.0f) ? customHp : std::max(70.0f, std::min(800.0f, 60.0f + 10.0f * botLvl));
			const float dmgScale = 0.05f; // Sparring dummy deals non-lethal chip damage so operatives can spar safely across many rounds
			botPo->setLevel(botLvl);
			botPo->setMaximumHealth((uint16)hpF);
			botPo->setCurrentHealth((uint16)hpF);
			botPo->setDamageScale(dmgScale);

			noteEntitySpawned(botGoId);
			setTargetGoId(botGoId);
			m_lastDojoBotGoId = botGoId;
			INFO_LOG(format("(%1%) %2%:%3% selected dynamic object view id 0001 objType 0001 (targetGoId=%4%)")
				% m_parent.Address() % m_handle % m_goId % botGoId);
			auto pkts = botPo->getCurrentStatePackets();
			for (const auto& pkt : pkts) {
				m_parent.QueueState(pkt);
			}
			m_parent.QueueState(std::make_shared<LocomotionStateMsg>(botGoId, 0, botPos.getMxoRot()));
			sGame.AnnounceStateUpdateNear((float)bx, (float)bz, 20000.0f, std::make_shared<LocomotionStateMsg>(botGoId, 0, botPos.getMxoRot()));
			INFO_LOG(format("Dojo bot %1% spawned for %2%: dist %3% units, pos (%4%, %5%, %6%) player (%7%, %8%, %9%) bot rot %10% lvl %11% hp %12% dmgScale %13%")
				% botGoId % m_handle % (sqrt((pos.x - bx) * (pos.x - bx) + (pos.z - bz) * (pos.z - bz)))
				% bx % pos.y % bz % pos.x % pos.y % pos.z % botPos.rot % int(botLvl) % hpF % dmgScale);
			return true;
		};

		FightingStyle targetStyle = FightingStyle::None;
		std::string styleLabel = "Street Brawler";
		bool is1v1 = false;
		float optHp = 0.0f;

		if (iequals(subCommand, "kungfu") || iequals(subCommand, "wushu")) {
			targetStyle = FightingStyle::KungFu;
			styleLabel = "Kung Fu Master";
			is1v1 = true;
			cmdStream >> optHp;
		} else if (iequals(subCommand, "karate")) {
			targetStyle = FightingStyle::Karate;
			styleLabel = "Karate Master";
			is1v1 = true;
			cmdStream >> optHp;
		} else if (iequals(subCommand, "aikido")) {
			targetStyle = FightingStyle::Aikido;
			styleLabel = "Aikido Sensei";
			is1v1 = true;
			cmdStream >> optHp;
		} else if (iequals(subCommand, "brawl") || iequals(subCommand, "street")) {
			targetStyle = FightingStyle::None;
			styleLabel = "Street Brawler";
			is1v1 = true;
			cmdStream >> optHp;
		} else if (iequals(subCommand, "1v1")) {
			is1v1 = true;
			string optStyle;
			if (cmdStream >> optStyle) {
				if (iequals(optStyle, "kungfu") || iequals(optStyle, "wushu")) {
					targetStyle = FightingStyle::KungFu;
					styleLabel = "Kung Fu Master";
				} else if (iequals(optStyle, "karate")) {
					targetStyle = FightingStyle::Karate;
					styleLabel = "Karate Master";
				} else if (iequals(optStyle, "aikido")) {
					targetStyle = FightingStyle::Aikido;
					styleLabel = "Aikido Sensei";
				} else if (iequals(optStyle, "brawl") || iequals(optStyle, "street")) {
					targetStyle = FightingStyle::None;
					styleLabel = "Street Brawler";
				} else {
					try { optHp = (float)std::stof(optStyle); } catch (...) {}
				}
				if (optHp <= 0.0f) {
					cmdStream >> optHp;
				}
			} else {
				static int s_cycleStyle = 0;
				int s = (s_cycleStyle++) % 4;
				if (s == 0) { targetStyle = FightingStyle::KungFu; styleLabel = "Kung Fu Master"; }
				else if (s == 1) { targetStyle = FightingStyle::Karate; styleLabel = "Karate Master"; }
				else if (s == 2) { targetStyle = FightingStyle::Aikido; styleLabel = "Aikido Sensei"; }
				else { targetStyle = FightingStyle::None; styleLabel = "Street Brawler"; }
			}
		}

		if (is1v1)
		{
			bool ok = spawnDojoBot(4.0f, 0.0f, targetStyle, "Dojo " + styleLabel, optHp);
			if (ok && m_lastDojoBotGoId) {
				uint16 targetViewId = sObjMgr.getViewForGO(&m_parent, m_lastDojoBotGoId);
				uint32 clientTargetRef = uint32(targetViewId) | (uint32(PLAYER_SPAWN_COUNTER) << 16);
				sCombatSys.RequestInterlock(m_goId, m_lastDojoBotGoId, clientTargetRef);
			}
			m_parent.QueueCommand(make_shared<SystemChatMsg>(ok
				? (format("{c:00FF00}Dojo: 1v1 %1% spawned and engaged in close combat interlock!{/c}") % styleLabel).str()
				: "{c:FF0000}Dojo: could not spawn an enemy.{/c}"));
		}
		else if (iequals(subCommand, "group"))
		{
			// four dummies in a 3-5 m arc in front of the player, each with a different martial arts style!
			static const float distM[4]   = { 4.5f, 3.5f, 3.5f, 4.5f };
			static const float offRad[4]  = { -0.50f, -0.17f, 0.17f, 0.50f };
			static const FightingStyle styles[4] = { FightingStyle::KungFu, FightingStyle::Karate, FightingStyle::Aikido, FightingStyle::None };
			static const char* labels[4] = { "Dojo Kung Fu Master", "Dojo Karate Master", "Dojo Aikido Sensei", "Dojo Street Brawler" };
			int spawned = 0;
			for (int i = 0; i < 4; i++)
				if (spawnDojoBot(distM[i], offRad[i], styles[i], labels[i]))
					spawned++;
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Dojo: %1% martial arts masters (Kung Fu, Karate, Aikido, Brawl) spawned ahead of you!{/c}") % spawned).str()));
		}
		else if (iequals(subCommand, "clear"))
		{
			int killed = 0;
			auto goIds = sObjMgr.getAllGOIds();
			for (uint32 id : goIds) {
				PlayerObject* p = sObjMgr.getGOPtrSafe(id);
				if (p && p != this && p->getClient().isBot() && !p->isDead()) {
					if (p->getPosition().DistanceSq(pos) < 5000.0f * 5000.0f) {
						p->die(0); // killerGoId 0 = no reward; announces death + cleans combat state
						killed++;
					}
				}
			}
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Dojo: Cleared %1% bots.{/c}") % killed).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &dojo <1v1 | group | clear>{/c}"));
		}
		return;
	}
	else if (iequals(command, "overwrite") || iequals(command, "agentstrike") || iequals(command, "agent"))
	{
		uint32 targetCivId = 0;
		uint32 customHp = 0;
		string agentName = "Agent Johnson";
		bool isSmith = false;

		string token1, token2;
		if (cmdStream >> token1) {
			if (!token1.empty() && isdigit((unsigned char)token1[0])) {
				uint32 val = (uint32)atoi(token1.c_str());
				if (val > 1000) {
					targetCivId = val;
				} else {
					customHp = val;
				}
			} else if (iequals(token1, "smith")) {
				isSmith = true;
				agentName = "Agent Smith";
			} else if (!token1.empty()) {
				agentName = token1;
			}
		}

		if (cmdStream >> token2) {
			if (!token2.empty() && isdigit((unsigned char)token2[0])) {
				customHp = (uint32)atoi(token2.c_str());
			} else if (iequals(token2, "smith")) {
				isSmith = true;
				agentName = "Agent Smith";
			}
		}

		if (targetCivId == 0) {
			targetCivId = m_targetGoId;
		}

		LocationVector pos = this->getPosition();
		if (targetCivId == 0 || !sObjMgr.getGOPtrSafe(targetCivId)) {
			float bestDistSq = 3000.0f * 3000.0f;
			for (uint32 gid : sObjMgr.getAllGOIds()) {
				PlayerObject* po = sObjMgr.getGOPtrSafe(gid);
				if (po && po != this && !po->isDead() && (po->getClient().isBot() || po->getFactionName() == "Civilian")) {
					float d = (float)pos.DistanceSq(po->getPosition());
					if (d < bestDistSq) {
						bestDistSq = d;
						targetCivId = gid;
					}
				}
			}
		}

		if (targetCivId == 0 || !sObjMgr.getGOPtrSafe(targetCivId)) {
			// Spawn a civilian bot 4m in front of player at authentic pavement level
			const LocationVector botPos = DojoPlaceInFront(pos, 4.0f, 0.0f);
			auto bot = sBotMgr.SpawnSingleBot((float)botPos.x, (float)pos.y, (float)botPos.z, FACTION_NONE);
			if (bot) {
				targetCivId = bot->GetPlayerGoId();
				if (auto po = sObjMgr.getGOPtrSafe(targetCivId)) {
					po->setHandle("Thomas Anderson");
					po->setFactionName("Civilian");
					po->setPosition(botPos);
					noteEntitySpawned(targetCivId);
				}
			}
		}

		if (targetCivId != 0) {
			uint32 agentId = sAgentPossessionMgr.PossessCivilian(targetCivId, agentName, m_goId, isSmith);
			if (agentId != 0) {
				setTargetGoId(targetCivId);
				m_lastDojoBotGoId = targetCivId;

				string alertMsg = (format("{c:00FF00}[AGENT OVERWRITE] Anomaly detected: %1%. Commencing system overwrite of civilian host into %2%!{/c}") 
					% m_handle % agentName).str();
				m_parent.QueueCommand(make_shared<SystemChatMsg>(alertMsg));
				sGame.AnnounceStateUpdateNear((float)pos.x, (float)pos.z, 20000.0f, make_shared<SystemChatMsg>(alertMsg));
				INFO_LOG(format("AgentPossession: Overwrote civilian GoID %1% with %2% [AgentID: %3%]") % targetCivId % agentName % agentId);

				if (PedestrianEcology::getSingletonPtr()) {
					sPedestrianEcology.SpreadRumorFearAura((float)pos.x, (float)pos.z, 0.75f, 3500.0f);
				}

				if (auto agentPo = sObjMgr.getGOPtrSafe(targetCivId)) {
					if (customHp > 0) {
						agentPo->setMaximumHealth(customHp);
						agentPo->setCurrentHealth(customHp);
					}
					auto pkts = agentPo->getCurrentStatePackets();
					for (const auto& pkt : pkts) {
						m_parent.QueueState(pkt);
					}
					m_parent.QueueState(make_shared<CombatantModeMsg>(targetCivId, 1));
				}

				uint16 targetViewId = sObjMgr.getViewForGO(&m_parent, targetCivId);
				uint32 clientTargetRef = uint32(targetViewId) | (uint32(PLAYER_SPAWN_COUNTER) << 16);
				sCombatSys.RequestInterlock(m_goId, targetCivId, clientTargetRef);
			} else {
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[AGENT OVERWRITE] Failed to possess candidate.{/c}"));
			}
		} else {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[AGENT OVERWRITE] No civilian host available to possess.{/c}"));
		}
		return;
	}
	else if (iequals(command, "target"))
	{
		uint32 bestGoId = 0;
		if (m_lastDojoBotGoId != 0 && sObjMgr.getGOPtrSafe(m_lastDojoBotGoId) && !sObjMgr.getGOPtrSafe(m_lastDojoBotGoId)->isDead())
		{
			bestGoId = m_lastDojoBotGoId;
		}
		else
		{
			float bestDistSq = 2000.0f * 2000.0f;
			for (uint32 gid : sObjMgr.getAllGOIds())
			{
				PlayerObject* po = sObjMgr.getGOPtrSafe(gid);
				if (po && po != this && !po->isDead() && (po->getFaction() == FACTION_MACHINES || po->getFactionName() == "Machines"))
				{
					float d = (float)m_pos.DistanceSq(po->getPosition());
					if (d < bestDistSq)
					{
						bestDistSq = d;
						bestGoId = gid;
					}
				}
			}
		}
		if (bestGoId != 0)
		{
			setTargetGoId(bestGoId);
			INFO_LOG(format("(%1%) %2%:%3% selected dynamic object view id 0001 objType 0001 (targetGoId=%4%)")
				% m_parent.Address() % m_handle % m_goId % bestGoId);
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[TARGET] Acquired target: %1% (GOID %2%){/c}") % sObjMgr.getGOPtrSafe(bestGoId)->getHandle() % bestGoId).str()
			));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[TARGET] No hostile or dojo bot found nearby.{/c}"));
		}
		return;
	}
	else if (iequals(command, "loadout"))
	{
		string disc;
		cmdStream >> disc;
		if (cmdStream.fail() || disc.empty())
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				"{c:FF8800}Usage: &loadout <martial|kungfu|karate|aikido|hacker|coder|soldier|spy|default>{/c}"));
			return;
		}

		if (!m_abilitySystem)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOADOUT] Ability system unavailable.{/c}"));
			return;
		}

		m_abilitySystem->clearLoadout();

		struct LoadoutItem { uint16 id; uint16 level; uint16 slot; };
		std::vector<LoadoutItem> items;
		std::string discName = "Default";

		if (iequals(disc, "kungfu") || iequals(disc, "martial"))
		{
			discName = "Kung Fu / Martial Arts";
			items = {
				{ 600, 1, 0 },  // CloseCombatTrainingAbility
				{ 137, 1, 1 },  // MartialArtsInitiateAbility
				{  17, 1, 2 },  // SelfDefenseAbility
				{ 133, 1, 3 },  // KungFuAbility
				{ 197, 1, 4 },  // Head Butt
				{ 198, 1, 5 },  // Cheap Shot
				{ 574, 1, 6 },  // KungFuDamageAbility
			};
			setFightingStyle(FightingStyle::KungFu);
		}
		else if (iequals(disc, "karate"))
		{
			discName = "Karate";
			items = {
				{ 600, 1, 0 },  // CloseCombatTrainingAbility
				{ 132, 1, 1 },  // KarateAbility
				{ 531, 1, 2 },  // KarateFocusAbility
				{ 569, 1, 3 },  // KarateCombatTacticsAbility
				{ 573, 1, 4 },  // KarateDamageAbility
				{ 197, 1, 5 },  // Head Butt
			};
			setFightingStyle(FightingStyle::Karate);
		}
		else if (iequals(disc, "aikido"))
		{
			discName = "Aikido";
			items = {
				{ 600, 1, 0 },  // CloseCombatTrainingAbility
				{ 101, 1, 1 },  // AikidoAbility
				{ 296, 1, 2 },  // AikidoSpinClayPigeonAbility
				{ 571, 1, 3 },  // AikidoCombatTacticsAbility
				{ 572, 1, 4 },  // AikidoDamageAbility
				{ 8597, 1, 5 }, // AikidoRedirectionAbility
			};
			setFightingStyle(FightingStyle::Aikido);
		}
		else if (iequals(disc, "hacker"))
		{
			discName = "Hacker (Viral Logic)";
			items = {
				{  57, 1, 0 },  // LogicBlast1Ability
				{  58, 1, 1 },  // LogicBlast2Ability
				{  60, 1, 2 },  // LogicBomb1Ability
				{ 359, 1, 3 },  // CodeNukeAbility
				{  68, 1, 4 },  // PersonalFirewall1Ability
				{  43, 1, 5 },  // DisruptInputs1Ability
				{  40, 1, 6 },  // CodeFreeze1Ability
			};
		}
		else if (iequals(disc, "coder"))
		{
			discName = "Coder (RSI Repair & Support)";
			items = {
				{  77, 1, 0 },  // RestoreHealth1Ability
				{  80, 1, 1 },  // RestoreHealth2Ability
				{ 169, 1, 2 },  // EmergencyRepairs1Ability
				{  56, 1, 3 },  // GroupRepairs1Ability
				{  39, 1, 4 },  // BolsterHealth1Ability
				{ 375, 1, 5 },  // ReviveRSIAbility
			};
		}
		else if (iequals(disc, "soldier") || iequals(disc, "gunner"))
		{
			discName = "Soldier (Tactical Firearms)";
			items = {
				{ 129, 1, 0 },  // HandgunsAbility
				{  14, 1, 1 },  // PowerShotAbility
				{ 126, 1, 2 },  // PistolDisarmingShotAbility
				{ 147, 1, 3 },  // RiflesAbility
				{ 499, 1, 4 },  // PistolPointBlankAbility
				{ 505, 1, 5 },  // SniperShotAbility
			};
		}
		else if (iequals(disc, "spy"))
		{
			discName = "Spy (Stealth & Blades)";
			items = {
				{ 209, 1, 0 },  // StealthAbility
				{ 146, 1, 1 },  // PoisonKnifeAbility
				{ 283, 1, 2 },  // KnifeThrowerAbility
				{ 293, 1, 3 },  // StealthCountermeasuresAbility
			};
		}
		else
		{
			discName = "Default Operative";
			items = {
				{ 600, 1, 0 },
				{ 137, 1, 1 },
				{  17, 1, 2 },
			};
		}

		for (const auto& itm : items)
		{
			m_abilitySystem->loadAbility(itm.id, itm.level, itm.slot);
		}

		m_abilitySystem->saveToDB();
		m_abilitySystem->sendFullLoadout();

		m_parent.QueueCommand(make_shared<SystemChatMsg>(
			(format("{c:00FF00}[LOADOUT] Equipped %1% loadout (%2% abilities) on hotbar.{/c}")
				% discName % items.size()).str()
		));
		return;
	}
	else if (iequals(command, "botdebug"))
	{
		string targetName;
		cmdStream >> targetName;
		
		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &botdebug <bot_name>{/c}"));
			return;
		}

		std::shared_ptr<BotClient> bot = nullptr;
		uint32 targetGoid = 0;
		auto goIds = sObjMgr.getAllGOIds();
		for (uint32 id : goIds) {
			PlayerObject* p = sObjMgr.getGOPtr(id);
			if (p && iequals(p->getHandle(), targetName)) {
				bot = sBotMgr.GetBotByGOID(id);
				targetGoid = id;
				break;
			}
		}

		if (!bot) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target is not a BotClient!{/c}"));
			return;
		}

		PlayerObject* botPlayer = sObjMgr.getGOPtr(targetGoid);
		auto personality = bot->GetPersonality();
		auto somatic = bot->GetSomaticMarkers();

		std::stringstream ss;
		ss << "{c:00FF00}--- BOT COGNITIVE DUMP ---{/c}\n";
		if (botPlayer) ss << "Level: " << (int)botPlayer->getLevel() << " | XP: " << botPlayer->getExperience() << "\n";
		ss << "Vibe: " << personality.vibe << "\n";
		ss << "Personality: Aggro:" << personality.aggressiveness << " Fearless:" << personality.fearlessness << " Curious:" << personality.curiosity << " Talkative:" << personality.talkativeness << "\n";
		ss << "Somatic Markers: Heart Rate:" << somatic.metrics.heartRate << " Adrenaline:" << somatic.metrics.adrenaline << " Fatigue:" << somatic.metrics.physicalFatigue << "\n";
		
		m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
		return;
	}
    else if (iequals(command, "bank"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Bank system disabled (stubs removed).{/c}"));
        return;
    }
    else if (iequals(command, "choosepill"))
    {
        string pillChoice;
        cmdStream >> pillChoice;
        if (iequals(pillChoice, "red")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[Morpheus]: You take the red pill... you stay in Wonderland, and I show you how deep the rabbit hole goes.{/c}"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[System] You have been awakened. Faction: Zion. Initiating hardline upload...{/c}"));
            setPosition(LocationVector(56700.0f, 100.0f, 28000.0f)); // Slums hardline
            setFactionName("Zion");
        } else if (iequals(pillChoice, "blue")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:0000FF}[Morpheus]: You take the blue pill... the story ends, you wake up in your bed and believe whatever you want to believe.{/c}"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[System] Warning: Connection terminated. You have chosen the illusion.{/c}"));
            // Set faction to civilian and kick them to nowhere, simulating jack out or civilian lock
            setFactionName("Civilian");
            // Optionally kick the client entirely.
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &choosepill [red|blue]{/c}"));
        }
        return;
    }
    else if (iequals(command, "subway"))
    {
        string districtName;
        cmdStream >> districtName;
        if (districtName.empty()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &subway [Slums|Richland|Downtown]{/c}"));
            return;
        }

        if (getInfo() < 50) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Not enough Info. A subway ticket costs 50 Info.{/c}"));
            return;
        }

        LocationVector dest;
        bool valid = false;
        if (iequals(districtName, "Slums")) { dest = LocationVector(56700.0f, 0.0f, 28000.0f); valid = true; }
        else if (iequals(districtName, "Richland")) { dest = LocationVector(40000.0f, 0.0f, 28000.0f); valid = true; }
        else if (iequals(districtName, "Downtown")) { dest = LocationVector(58000.0f, 0.0f, 10000.0f); valid = true; }
        else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Invalid district. Choose: Slums, Richland, Downtown.{/c}"));
            return;
        }

        if (valid) {
            addInfo(-50);
            setPosition(dest);
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Trainman: Welcome to the %1%. Watch your back.{/c}") % districtName).str()));
        }
        return;
    }
    else if (iequals(command, "claimhl"))
    {
        LocationVector loc = this->getPosition();
        uint8 district = this->getDistrict();
        
        format sql = format("SELECT `HardlineId`, `X`, `Y`, `Z` FROM `hardlines` WHERE `DistrictId`='%1%'") % (int)district;
        scoped_ptr<QueryResult> result(sDatabase.Query(sql));
        if (result) {
            uint32 closestId = 0;
            float closestDist = 999999.0f;
            LocationVector bestLoc;
            do {
                Field* fields = result->Fetch();
                uint32 hlId = fields[0].GetUInt32();
                float hx = fields[1].GetFloat();
                float hy = fields[2].GetFloat();
                float hz = fields[3].GetFloat();
                
                LocationVector hlLoc(hx, hy, hz);
                float dist = loc.Distance(hlLoc);
                if (dist < closestDist) {
                    closestDist = dist;
                    closestId = hlId;
                    bestLoc = hlLoc;
                }
            } while (result->NextRow());
            
            if (closestDist < 1000.0f) {
                format updateSql = format("UPDATE `hardlines` SET `FactionTag`='%1%', `HardlineName`='Tagged By %2%' WHERE `DistrictId`='%3%' AND `HardlineId`='%4%'")
                    % (int)getFaction() % getHandle() % (int)district % closestId;
                sDatabase.Execute(updateSql);
                
                sFactionWarMgr.captureNode(closestId, getFaction());
                
                // Living History: Record hardline captured & propagate gossip
                sWorldDirector.RecordHardlineCaptured(m_parent.GetCharacterId(), getHandle(), getFaction());
                sWorldDirector.PropagatePlayerDeedGossip(
                    (format("%1% captured strategic hardline %2% for their faction!") % getHandle() % closestId).str(),
                    bestLoc.x, bestLoc.z
                );
                
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Hardline %1% claimed for your faction!{/c}") % closestId).str()));
                
                // Item 30: Trigger Faction Defenders
                sBotMgr.SpawnFactionDefenders(district, closestId, getFaction(), bestLoc);
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not close enough to a Hardline.{/c}"));
            }
        }
        return;
    }
	else if (iequals(command, "send") || iequals(command, "sendCmd"))
	{
		stringstream restOfStream;
		restOfStream << cmdStream.rdbuf();
		string hexStream = restOfStream.str();
		erase_all(hexStream," ");

		msgBaseClassPtr thePacket = make_shared<HexGenericMsg>(hexStream);
		const ByteBuffer& dataBuf = thePacket->toBuf();
		if (!dataBuf.count())
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("No bytes to send!"));
			return;
		}

		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Sending %1% bytes to you")%dataBuf.count()).str()));

		if (iequals(command,"send"))
			m_parent.QueueState(thePacket,true);
		else
			m_parent.QueueCommand(thePacket);
	}
	else if (iequals(command, "socket"))
	{
		uint64 gearUid = 0;
		string type;
		int value = 0;
		cmdStream >> gearUid >> type >> value;

		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &socket <gearUid> <type> <value>{/c}"));
			return;
		}

		SocketFragment frag = { type, value };
		if (sSocketSystem.ApplySocket(this, gearUid, frag)) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Successfully socketed %1% (+%2%)!{/c}") % type % value).str()));
		} else {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Socketing failed.{/c}"));
		}
	}
    else if (iequals(command, "bullettime"))
    {
        float radius;
        float multiplier;
        cmdStream >> radius >> multiplier;
        if (cmdStream.fail()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &bullettime <radius> <multiplier>{/c}"));
            return;
        }

        // Broadcast Chrono-Sphere to spatial grid
        auto localClients = sSpatialGrid.GetClientsNearClient(&getClient());
        int affected = 0;
        for(GameClient* gc : localClients) {
            if(gc && gc->GetPlayerGoId() != 0) {
                PlayerObject* target = sObjMgr.getGOPtr(gc->GetPlayerGoId());
                if (target && target->getGoId() != this->getGoId()) {
                    float distSq = target->getPosition().DistanceSq(this->getPosition());
                    if (distSq <= (radius * radius)) {
                        target->ApplyTimeDilation(multiplier, 15000); // 15 seconds
                        affected++;
                    }
                }
            }
        }
        
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF00FF}Bullet Time deployed! %1% entities caught in Time Dilation.{/c}") % affected).str()));
    }
	else if (iequals(command,"netstats"))
	{
		string theNetStats = m_parent.GetNetStats();
		m_parent.QueueCommand(make_shared<SystemChatMsg>(theNetStats));
		boost::replace_all(theNetStats,"\n"," ");
		INFO_LOG(format("(%1%) %2%:%3% netstats: %4%")
			% m_parent.Address()
			% m_handle
			% m_goId
			% theNetStats );
	}
	else if (iequals(command, "gotoPos"))
	{
		double x,y,z;
		cmdStream >> x;
		if (cmdStream.eof() || cmdStream.fail())
			return;
		cmdStream >> y;
		if (cmdStream.eof() || cmdStream.fail())
			return;
		cmdStream >> z;
		if (cmdStream.fail())
			return;

		x*=100;
		y*=100;
		z*=100;

		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Teleported...{/c}"));
		LocationVector derp(x,y,z);
		this->setPosition(derp);

		sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId));
		return;
	}
	else if (iequals(command, "incX") || iequals(command, "incY") || iequals(command, "incZ"))
	{
		double incrementAmount=0;

		if (cmdStream.eof())
			incrementAmount = 1;

		cmdStream >> incrementAmount;

		if (cmdStream.fail())
			incrementAmount = 1;

		if (incrementAmount==0)
			return;

		incrementAmount*=100;

		double newX,newY,newZ;
		newX = this->getPosition().x;
		newY = this->getPosition().y;
		newZ = this->getPosition().z;

		if (iequals(command, "incX"))
			newX+=incrementAmount;
		else if (iequals(command, "incY"))
			newY+=incrementAmount;
		else if (iequals(command,"incZ"))
			newZ+=incrementAmount;

		LocationVector newPos(newX,newY,newZ);
		this->setPosition(newPos);
		sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId),true);
		return;
	}
	else if (iequals(command, "goThru"))
	{
		double incrementAmount=0;

		if (cmdStream.eof())
			incrementAmount = 2;

		cmdStream >> incrementAmount;

		this->GoAhead(incrementAmount);
		return;
	}
	else if (iequals(command, "random"))
	{
		//Random Object Id
		uint32 randObjId = rand() % 0xFFFFFFFF;
		uint16 randViewId = rand() % 0xFFFF;

		//randObjId = 1310720002;   //mara church middle door

		sObjMgr.RandomObject(randObjId, &m_parent,this->getPosition().x, this->getPosition().y, this->getPosition().z, this->getPosition().rot );
		m_parent.QueueState(make_shared<DoorAnimationMsg>(randObjId, randViewId, this->getPosition().x, this->getPosition().y, this->getPosition().z, this->getPosition().rot, 1));
		return;
	}
	else if (iequals(command, "update"))
	{
		this->UpdateAppearance();
		m_parent.QueueCommand(make_shared<SystemChatMsg>("Your appearance has been refreshed."));
		return;
	}
	else if (iequals(command, "gotoPlayer"))
	{
		string playerName;
		cmdStream >> playerName;

		if (cmdStream.fail())
			return;

		PlayerObject* theTargetPlayer = NULL;
		{
			vector<uint32> allObjects = sObjMgr.getAllGOIds();
			foreach(uint32 objId, allObjects)
			{
				PlayerObject* playerObj = NULL;
				try
				{
					playerObj = sObjMgr.getGOPtr(objId);
				}
				catch (...)
				{
					continue;
				}

				if (playerObj && iequals(playerName,playerObj->getHandle()))
				{
					theTargetPlayer = playerObj;
					break;
				}
			}
		}

		if (theTargetPlayer == NULL)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Player %1% is not online")%playerName).str()));
			return;
		}

		this->setPosition(theTargetPlayer->getPosition());		
		sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId));
		return;
	}
	else if (iequals(command, "go"))
	{
		string area;
		cmdStream >> area;

		if (cmdStream.fail()) //Get list and whisper it but for now we just fail
			return;

		PreparedStatement stmt("SELECT `X`,`Y`,`Z` FROM `locations` WHERE `District` = ?0 AND `Command` = ?1 LIMIT 1");
		stmt.SetUInt32(0, (uint32)getDistrict());
		stmt.SetString(1, area);
		scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
		if (!result)
			return;
		else
		{
			Field *field = result->Fetch();
			double newX = field[0].GetDouble();
			double newY = field[1].GetDouble();
			double newZ = field[2].GetDouble();

			string message1 = (format("{c:00FF00}Welcome to %1%.{/c}") % area ).str();
			m_parent.QueueCommand(make_shared<SystemChatMsg>(message1));
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}You may have to wait a min or two for the client and server to sync if the area is complex...{/c}"));
			LocationVector derp(newX,newY,newZ);
			this->setPosition(derp);

			sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId));
			return;
		}
	}
	else if (iequals(command, "buy"))
	{
		uint32 templateId;
		cmdStream >> templateId;
		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &buy <templateId>{/c}"));
			return;
		}

		const ItemTemplate* templ = sDataLoader.GetItemTemplate(templateId);
		if (!templ) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Invalid item template.{/c}"));
			return;
		}

		if (consumeInformation(templ->value)) {
			if (getInventory()) {
				auto item = std::make_shared<Item>(rand(), templateId);
				getInventory()->addItemAuto(item);
				m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Bought item %1% for %2% Info.{/c}") % templ->name % templ->value).str()));
			}
		} else {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Not enough Info.{/c}"));
		}
		return;
	}
	else if (iequals(command, "sell"))
	{
		uint32 itemGoId;
		cmdStream >> itemGoId;
		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &sell <itemGoId>{/c}"));
			return;
		}

		if (getInventory()) {
			auto item = getInventory()->getItemByGoId(itemGoId);
			if (item) {
				const ItemTemplate* templ = sDataLoader.GetItemTemplate(item->getTemplateId());
				if (templ) {
					uint32 sellValue = templ->value / 2;
					addInformation(sellValue);
					getInventory()->removeItem(itemGoId);
					m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Sold item for %1% Info.{/c}") % sellValue).str()));
				}
			} else {
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Item not found in inventory.{/c}"));
			}
		}
		return;
	}	
    else if (iequals(command, "frank") || iequals(command, "punisher") || iequals(command, "frankstatus"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "hunt")) {
            sFrankCastleMgr.ScanForAgentsAndThreats();
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Frank Castle notified. Tactical agent sweep initiated.{/c}"));
        } else if (iequals(subCmd, "safehouse")) {
            uint32 shId = 0;
            cmdStream >> shId;
            if (shId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &frank safehouse <1-5>{/c}"));
            } else {
                sFrankCastleMgr.CommandDeployToSafehouse(shId);
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Dispatched tactical deployment order to Safehouse #%1%.{/c}") % shId).str()));
            }
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sFrankCastleMgr.GenerateStatusReport()));
        }
        return;
    }
    else if (iequals(command, "inspect") || iequals(command, "bio") ||
             iequals(command, "social") || iequals(command, "love") || iequals(command, "friendships") ||
             iequals(command, "memories") || iequals(command, "memory") || iequals(command, "socialstats") ||
             iequals(command, "family") || iequals(command, "household") ||
             iequals(command, "dreams") || iequals(command, "dream") || iequals(command, "aspiration") ||
             iequals(command, "career") || iequals(command, "job") || iequals(command, "vocation") ||
             iequals(command, "life") || iequals(command, "lifedossier") || iequals(command, "dossier") ||
             iequals(command, "lifestats") || iequals(command, "familystats") ||
             iequals(command, "emergentlife") || iequals(command, "emergentlifestyle") ||
             iequals(command, "awakening") || iequals(command, "epiphany") ||
             iequals(command, "gossip") || iequals(command, "rumors") ||
             iequals(command, "socialcircles") || iequals(command, "circles"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[System] Subsystem offline.{/c}"));
        return;
    }
    else if (iequals(command, "underworld") || iequals(command, "syndicate"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sUnderworldMgr.GenerateUnderworldStatusReport()));
        return;
    }
    else if (iequals(command, "police") || iequals(command, "swat"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentPoliceMgr.GeneratePoliceSWATReport()));
        return;
    }
    else if (iequals(command, "citylife") || iequals(command, "simulation"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sCityLifeMgr.GenerateCityLifeStatusReport()));
        return;
    }
    else if (iequals(command, "emergent"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[System] Subsystem offline.{/c}"));
        return;
    }
    else if (iequals(command, "market") || iequals(command, "marketplace"))
    {
        string sub;
        cmdStream >> sub;
        if (sub.empty() || iequals(sub, "list"))
        {
            ByteBuffer dummy;
            RPC_HandleMarketListItems(dummy);
            return;
        }
        else if (iequals(sub, "sell"))
        {
            uint32 tplId = 0, price = 0;
            cmdStream >> tplId >> price;
            if (tplId > 0 && price > 0)
            {
                uint64 listingId = sEconomySys.ListVendorItem(this, tplId, price);
                if (listingId > 0)
                {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>(
                        (format("{c:00FFCC}[MARKETPLACE] Listed item %1% on Exchange for %2% $Info (Listing ID: %3%){/c}")
                         % tplId % price % listingId).str()
                    ));
                    INFO_LOG(format("Marketplace: Player %1% listed item %2% for %3% $Info (Listing %4%)")
                             % m_handle % tplId % price % listingId);
                }
                else
                {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /market sell <templateId> <price>{/c}"));
                }
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /market sell <templateId> <price>{/c}"));
            }
            return;
        }
        else if (iequals(sub, "buy"))
        {
            uint64 listingId = 0;
            cmdStream >> listingId;
            if (listingId > 0)
            {
                bool ok = sEconomySys.PurchaseVendorItem(this, listingId);
                if (ok)
                {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>(
                        (format("{c:00FF00}[MARKETPLACE] Successfully purchased listing ID %1%!{/c}") % listingId).str()
                    ));
                }
                else
                {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>(
                        (format("{c:FF4444}[MARKETPLACE] Purchase failed for listing ID %1% (insufficient funds, full inventory, or invalid listing).{/c}") % listingId).str()
                    ));
                }
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /market buy <listingId>{/c}"));
            }
            return;
        }
    }
    else if (iequals(command, "craft") || iequals(command, "compile"))
    {
        string sub;
        cmdStream >> sub;
        if (sub.empty() || iequals(sub, "list"))
        {
            auto bps = sCraftSys.GetAllBlueprints();
            if (bps.empty()) {
                sCraftSys.LoadBlueprints();
                bps = sCraftSys.GetAllBlueprints();
            }
            std::stringstream ss;
            ss << "{c:00FFCC}[CRAFTING] MegaCity Coder Blueprints (" << bps.size() << " available):{/c}\n";
            for (const auto& pair : bps) {
                const auto& bp = pair.second;
                ss << "{c:FFFF88}* [" << bp.blueprintId << "] " << bp.resultingName << "{/c} - Cost: "
                   << bp.infoCost << " $Info (Time: " << bp.craftTimeMs << "ms)\n";
            }
            ss << "{c:00FF88}Synthesize via: /craft <blueprintId>{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            INFO_LOG(format("Crafting: Dispatched %1% blueprint listings to %2%") % bps.size() % m_handle);
            return;
        }
        else
        {
            uint32 bpId = 0;
            try {
                bpId = static_cast<uint32>(std::stoul(sub));
            } catch (...) { bpId = 0; }
            if (bpId > 0)
            {
                bool ok = sCraftSys.HandleCraftRequest(this, bpId);
                if (ok) {
                    INFO_LOG(format("Crafting: Player %1% successfully synthesized blueprint %2%") % m_handle % bpId);
                }
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /craft list | /craft <blueprintId>{/c}"));
            }
            return;
        }
    }
    else if (iequals(command, "hovercraft") || iequals(command, "ship"))
    {
        if (sHovercraftFlightSys.GetShipCount() == 0) {
            sHovercraftFlightSys.SpawnHovercraft(HOVERCRAFT_NEBUCHADNEZZAR, FlightVector3(0.0f, 400.0f, 0.0f), "Nebuchadnezzar");
        }
        auto ship = sHovercraftFlightSys.GetHovercraft(1);
        auto emp = sHovercraftFlightSys.GetEMPSystem();
        auto uplink = sHovercraftFlightSys.CalculateUplinkTelemetry(1);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[Hovercraft Flight] Ship: %1% | Pos: (%2$.0f, %3$.0f, %4$.0f) | Hull: %5$.0f%% | EMP: %6$.1f%% (%7%) | Carrier: %8% (Strength: %9$.2f){/c}")
             % (ship ? ship->name : "Nebuchadnezzar") % (ship ? ship->position.x : 0.0f) % (ship ? ship->position.y : 0.0f) % (ship ? ship->position.z : 0.0f)
             % (ship ? ship->hullIntegrity : 100.0f) % emp.chargePercent % (emp.isReady ? "READY" : "CHARGING") % uplink.statusText % uplink.signalStrength).str()
        ));
        INFO_LOG(format("Hovercraft: Dispatched flight telemetry for ship %1% to %2%") % (ship ? ship->name : "Nebuchadnezzar") % m_handle);
        return;
    }
    else if (iequals(command, "construct") || iequals(command, "whitevoid"))
    {
        size_t weapons = sLoadingConstruct.GetTotalAvailableWeapons();
        size_t dummies = sLoadingConstruct.GetDummyCount();
        float dilation = sLoadingConstruct.GetTimeDilation();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFFFFF}[Loading Construct] Mode: White Void | Available Weapons: %1% | Sparring Dummies: %2% | Time Dilation: %3$.2fx{/c}")
             % weapons % dummies % dilation).str()
        ));
        INFO_LOG(format("Construct: Dispatched Loading Construct telemetry to %1%") % m_handle);
        return;
    }
    else if (iequals(command, "org") || iequals(command, "crew"))
    {
        string sub;
        cmdStream >> sub;
        if (iequals(sub, "create"))
        {
            string orgName;
            std::getline(cmdStream >> std::ws, orgName);
            if (orgName.empty()) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /org create <Organization Name>{/c}"));
                return;
            }
            if (m_orgId != 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are already in an Organization.{/c}"));
                return;
            }
            uint32 newOrgId = sOrgMgr.createOrganization(orgName, m_goId);
            m_orgId = newOrgId;
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}[ORGANIZATION] '%1%' created successfully (Org ID: %2%).{/c}") % orgName % newOrgId).str()));
            INFO_LOG(format("Organization: Player %1% created organization '%2%' (ID %3%)") % m_handle % orgName % newOrgId);
            return;
        }
        else if (iequals(sub, "leave"))
        {
            if (m_orgId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
                return;
            }
            if (sOrgMgr.leaveOrganization(m_orgId, m_goId)) {
                m_orgId = 0;
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[ORGANIZATION] You have left the Organization.{/c}"));
                INFO_LOG(format("Organization: Player %1% left organization") % m_handle);
            }
            return;
        }
        else
        {
            if (m_orgId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}[ORGANIZATION] Status: Independent Operative (Unaffiliated). Create one via: /org create <Name>{/c}"));
            } else {
                Organization* org = sOrgMgr.getOrganization(m_orgId);
                if (org) {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}[ORGANIZATION] Crew: %1% (ID %2%) | Leader: %3% | Members: %4%{/c}")
                        % org->name % org->id % org->leaderId % org->members.size()).str()));
                }
            }
            return;
        }
    }
	else
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Unrecognized server command %1%")%command).str()));
		return;
	}
}

void PlayerObject::RPC_HandleChat( ByteBuffer &srcCmd )
{
	uint16 stringLenPos = srcCmd.read<uint16>();
	stringLenPos = swap16(stringLenPos);

	if (stringLenPos != 8)
		WARNING_LOG(format("(%1%) Chat packet stringLenPos not 8 but %2%, packet %3%") % m_parent.Address() % stringLenPos % Bin2Hex(srcCmd));

	srcCmd.rpos(stringLenPos);
	string theMessage = srcCmd.readString();

	if (!theMessage.length())
		return;

	if (m_isAdmin && theMessage[0] == '!')
	{
		ParseAdminCommand(theMessage.substr(1));
		return;
	}
	else if (theMessage[0] == '&' ||
			 (theMessage[0] == '/' && (boost::istarts_with(theMessage, "/attack") ||
									  boost::istarts_with(theMessage, "/interlock") ||
									  boost::istarts_with(theMessage, "/tactic") ||
									  boost::istarts_with(theMessage, "/withdraw") ||
									  boost::istarts_with(theMessage, "/escape") ||
									  boost::istarts_with(theMessage, "/target") ||
									  boost::istarts_with(theMessage, "/loadout") ||
									  boost::istarts_with(theMessage, "/locker") ||
									  boost::istarts_with(theMessage, "/vault") ||
									  boost::istarts_with(theMessage, "/dye") ||
									  boost::istarts_with(theMessage, "/capture") ||
									  boost::istarts_with(theMessage, "/hacknode") ||
									  boost::istarts_with(theMessage, "/socket") ||
									  boost::istarts_with(theMessage, "/overwrite") ||
									  boost::istarts_with(theMessage, "/agentstrike") ||
									  boost::istarts_with(theMessage, "/agent") ||
									  boost::istarts_with(theMessage, "/market") ||
									  boost::istarts_with(theMessage, "/craft") ||
									  boost::istarts_with(theMessage, "/hovercraft") ||
									  boost::istarts_with(theMessage, "/construct") ||
									  boost::istarts_with(theMessage, "/org") ||
									  boost::istarts_with(theMessage, "/crew") ||
									  boost::istarts_with(theMessage, "/dojo"))))
	{
		INFO_LOG(format("(%1%) %2%:%3% chat command: %4%") % m_parent.Address() % m_handle % m_goId % theMessage);
		ParsePlayerCommand(theMessage.substr(1));
		return;
	}
	else if (m_isAdmin && theMessage[0] == '/' && boost::istarts_with(theMessage, "/mission"))
	{
		ParseAdminCommand("mission"); //admin path - players no longer reach ParseAdminCommand
		return;
	}

    // Item 118: Organization Chat
    if (theMessage.length() >= 5 && boost::iequals(theMessage.substr(0, 5), "/org ")) {
        if (m_orgId == 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
            return;
        }
        string orgMsg = theMessage.substr(5);
        string formattedMsg = (format("{c:00FFFF}[Org] %1%: %2%{/c}") % m_handle % orgMsg).str();
        auto players = sObjMgr.getAllGOIds();
        for (auto id : players) {
            PlayerObject* p = sObjMgr.getGOPtrSafe(id);
            if (p && p->getOrgId() == m_orgId) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(formattedMsg));
            }
        }
        return;
    }

    if (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/trade ")) {
        std::stringstream ss(theMessage.substr(7));
        string targetHandle;
        uint32 templateId = 0;
        ss >> targetHandle >> templateId;
        if (targetHandle.empty() || templateId == 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: /trade <targetName> <templateId>{/c}"));
            return;
        }

        PlayerObject* recipient = nullptr;
        auto allIds = sObjMgr.getAllGOIds();
        for (auto id : allIds) {
            PlayerObject* p = sObjMgr.getGOPtrSafe(id);
            if (p && boost::iequals(p->getHandle(), targetHandle)) {
                recipient = p;
                break;
            }
        }

        if (!recipient) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}Operative '%1%' not found.{/c}") % targetHandle).str()));
            return;
        }

        LocationVector pA = getPosition();
        LocationVector pB = recipient->getPosition();
        double dx = pB.x - pA.x;
        double dz = pB.z - pA.z;
        if ((dx * dx + dz * dz) > (1000.0 * 1000.0)) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Trade partner is out of range (max 10m).{/c}"));
            return;
        }

        if (m_inventorySystem && recipient->getInventory()) {
            if (m_inventorySystem->consumeItemByTemplate(templateId)) {
                recipient->giveItem(templateId);
                m_inventorySystem->saveToDB();
                recipient->getInventory()->saveToDB();
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[TRADE] Transferred item %1% to %2%.{/c}") % templateId % targetHandle).str()));
                recipient->getClient().QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[TRADE] Received item %1% from %2%.{/c}") % templateId % getHandle()).str()));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[TRADE] You do not possess that item.{/c}"));
            }
        }
        return;
    }

    // Phase 4 Part 2: Crew & Faction Management
    if (theMessage.length() >= 10 && boost::iequals(theMessage.substr(0, 11), "/orgcreate ")) {
        string orgName = theMessage.substr(11);
        if (m_orgId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are already in an Organization.{/c}"));
            return;
        }
        uint32 newOrgId = sOrgMgr.createOrganization(orgName, m_goId);
        m_orgId = newOrgId;
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Organization '%1%' created successfully.{/c}") % orgName).str()));
        // Note: Broadcast visual tag update here when packet mapped
        return;
    }

    if (theMessage.length() >= 10 && boost::iequals(theMessage.substr(0, 11), "/orginvite ")) {
        string targetName = theMessage.substr(11);
        if (m_orgId == 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
            return;
        }
        Organization* org = sOrgMgr.getOrganization(m_orgId);
        if (!org || org->leaderId != m_goId) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Only the Organization Leader can invite.{/c}"));
            return;
        }
        auto players = sObjMgr.getAllGOIds();
        for (auto id : players) {
            PlayerObject* p = sObjMgr.getGOPtrSafe(id);
            if (p && boost::iequals(p->getHandle(), targetName)) {
                if (p->getOrgId() != 0) {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target is already in an Organization.{/c}"));
                    return;
                }
                if (sOrgMgr.joinOrganization(m_orgId, p->getGoId())) {
                    p->setOrgId(m_orgId);
                    p->getClient().QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}You have been recruited into Organization '%1%'.{/c}") % org->name).str()));
                    m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}%1% has joined the Organization.{/c}") % p->getHandle()).str()));
                }
                return;
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target player not found.{/c}"));
        return;
    }

    if (boost::iequals(theMessage, "/orgleave")) {
        if (m_orgId == 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
            return;
        }
        if (sOrgMgr.leaveOrganization(m_orgId, m_goId)) {
            m_orgId = 0;
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}You have left the Organization.{/c}"));
            // Note: Broadcast visual tag update here when packet mapped
        }
        return;
    }

    // Phase 4: Faction Chat
    if (theMessage.length() >= 3 && (boost::iequals(theMessage.substr(0, 3), "/f ") || boost::iequals(theMessage.substr(0, 9), "/faction "))) {
        string factionMsg = boost::iequals(theMessage.substr(0, 3), "/f ") ? theMessage.substr(3) : theMessage.substr(9);
        
        string color = "FFFFFF";
        if (m_factionName == "Zion") color = "00FF00";
        else if (m_factionName == "Machines") color = "0000FF";
        else if (m_factionName == "Merovingian") color = "FF00FF";
        
        string formattedMsg = (format("{c:%1%}[Faction] %2%: %3%{/c}") % color % m_handle % factionMsg).str();
        auto players = sObjMgr.getAllGOIds();
        for (auto id : players) {
            PlayerObject* p = sObjMgr.getGOPtrSafe(id);
            if (p && p->getFactionName() == m_factionName) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(formattedMsg));
            }
        }
        return;
    }

    // Hardline Locker / Vault Storage System
    if (boost::iequals(theMessage, "/locker") || boost::iequals(theMessage, "/vault") ||
        boost::iequals(theMessage, "/locker help") || boost::iequals(theMessage, "/vault help") ||
        (theMessage.length() >= 8 && boost::iequals(theMessage.substr(0, 8), "/locker ")) ||
        (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/vault "))) {

        string subCmd;
        std::stringstream ss;
        if (boost::istarts_with(theMessage, "/locker")) {
            if (theMessage.length() > 7) ss.str(theMessage.substr(8));
        } else {
            if (theMessage.length() > 6) ss.str(theMessage.substr(7));
        }
        ss >> subCmd;

        const HardlineNode* hl = PlayerObject::GetNearestHardline(m_district, m_pos.x, m_pos.z);
        uint32 hlId = hl ? hl->hardlineId : 0;
        double d2 = hl ? ((m_pos.x - hl->x)*(m_pos.x - hl->x) + (m_pos.z - hl->z)*(m_pos.z - hl->z)) : 1e12;
        bool nearHl = (d2 <= (2500.0 * 2500.0)); // 25 meters

        if (subCmd.empty() || boost::iequals(subCmd, "help")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}--- HARDLINE LOCKER VAULT SYSTEM ---{/c}"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("  /locker list - List stored items in this Hardline vault"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("  /locker store <invSlot> - Store item from inventory (1-24) into vault"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("  /locker withdraw <entryId> - Retrieve item from vault into inventory"));
            return;
        }

        if (!nearHl) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Access Denied. You must be within 25m of an active Hardline.{/c}"));
            return;
        }

        if (boost::iequals(subCmd, "list")) {
            auto items = sEconomySys.GetLockerItems(getCharacterUID(), hlId);
            if (items.empty()) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FFFF00}[LOCKER] Hardline #%1% vault is empty.{/c}") % hlId).str()));
                return;
            }
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}--- HARDLINE #%1% VAULT (%2% ITEMS) ---{/c}") % hlId % items.size()).str()));
            for (const auto& it : items) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("  [ID: %1%] Slot: %2% | Template: %3% | Qty: %4%")
                        % it.entryId % (int)it.slot % it.templateId % it.quantity).str()));
            }
            return;
        }

        if (boost::iequals(subCmd, "store")) {
            uint32 invSlot = 0;
            ss >> invSlot;
            if (invSlot == 0 || !m_inventorySystem) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: /locker store <invSlot>{/c}"));
                return;
            }
            shared_ptr<Item> item = m_inventorySystem->getItemBySlot(static_cast<uint8>(invSlot));
            if (!item) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}[LOCKER] No item in inventory slot %1%.{/c}") % invSlot).str()));
                return;
            }
            uint32 tId = item->getTemplateId();
            if (sEconomySys.DepositToLocker(this, hlId, tId, static_cast<uint8>(invSlot))) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[LOCKER] Deposited item (Template #%1%) into Hardline #%2% vault.{/c}") % tId % hlId).str()));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Failed to deposit item into vault.{/c}"));
            }
            return;
        }

        if (boost::iequals(subCmd, "withdraw")) {
            uint64 entryId = 0;
            ss >> entryId;
            if (entryId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: /locker withdraw <entryId>{/c}"));
                return;
            }
            if (sEconomySys.WithdrawFromLocker(this, hlId, entryId)) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[LOCKER] Withdrew item (Locker ID #%1%) from Hardline #%2% vault into inventory.{/c}") % entryId % hlId).str()));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[LOCKER] Failed to withdraw item (check entry ID or inventory space).{/c}"));
            }
            return;
        }
    }

    // Tailor Wardrobe & RSI Color Dyeing
    if (theMessage.length() >= 5 && boost::iequals(theMessage.substr(0, 5), "/dye ")) {
        std::stringstream ss(theMessage.substr(5));
        string slotStr;
        uint32 colorVal = 0;
        ss >> slotStr >> colorVal;

        if (slotStr.empty()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}Usage: /dye <coat|shirt|pants|shoes|glasses|hair|skin> <0-31>{/c}"));
            return;
        }

        string colName = "";
        uint32 maxColor = 31;
        if (boost::iequals(slotStr, "coat"))        { colName = "coatcolor"; maxColor = 31; }
        else if (boost::iequals(slotStr, "shirt"))   { colName = "shirtcolor"; maxColor = 63; }
        else if (boost::iequals(slotStr, "pants"))   { colName = "pantscolor"; maxColor = 31; }
        else if (boost::iequals(slotStr, "shoes"))   { colName = "shoecolor"; maxColor = 15; }
        else if (boost::iequals(slotStr, "glasses")) { colName = "glassescolor"; maxColor = 15; }
        else if (boost::iequals(slotStr, "hair"))    { colName = "haircolor"; maxColor = 31; }
        else if (boost::iequals(slotStr, "skin"))    { colName = "skintone"; maxColor = 31; }
        else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[TAILOR] Unknown wardrobe slot. Valid: coat, shirt, pants, shoes, glasses, hair, skin.{/c}"));
            return;
        }

        if (colorVal > maxColor) colorVal = maxColor;

        if (Database_Main != nullptr) {
            try {
                PreparedStatement stmt((format("UPDATE `rsivalues` SET `%1%` = ?0 WHERE `charId` = ?1") % colName).str());
                stmt.SetUInt32(0, colorVal);
                stmt.SetUInt64(1, getCharacterUID());
                sDatabase.ExecutePrepared(&stmt);
            } catch (...) {}
        }

        UpdateAppearance();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FF00}[TAILOR] Your %1% has been dyed to palette color #%2%! Appearance matrix synchronized.{/c}")
                % slotStr % colorVal).str()
        ));
        return;
    }

    // Faction Warfare Hardline / Broadcast Node Viral Capture
    if ((theMessage.length() >= 9 && boost::iequals(theMessage.substr(0, 9), "/capture ")) ||
        (theMessage.length() >= 10 && boost::iequals(theMessage.substr(0, 10), "/hacknode "))) {
        std::stringstream ss;
        if (boost::istarts_with(theMessage, "/capture")) ss.str(theMessage.substr(9));
        else ss.str(theMessage.substr(10));

        uint32 nodeId = 0;
        ss >> nodeId;
        if (nodeId == 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: /capture <nodeId>{/c}"));
            return;
        }

        const auto& nodes = sFactionWarMgr.GetControlNodes();
        auto it = nodes.find(nodeId);
        if (it == nodes.end()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}[FACTION WAR] Control Node #%1% not found in directory.{/c}") % nodeId).str()));
            return;
        }

        const ControlNode& node = it->second;
        double dx = m_pos.x - node.x;
        double dz = m_pos.z - node.z;
        if ((dx * dx + dz * dz) > (2500.0 * 2500.0)) { // 25 meters
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[FACTION WAR] You must be within 25m of the Hardline node to upload viral control code.{/c}"));
            return;
        }

        uint32 myFaction = getFaction();
        if (myFaction == 0) myFaction = FACTION_ZION;

        if (node.controllingFaction == myFaction) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FFFF00}[FACTION WAR] Hardline Node #%1% is already controlled by your faction.{/c}") % nodeId).str()));
            return;
        }

        sFactionWarMgr.AdvanceNodeCapture(nodeId, myFaction, 0.35f, this);
        return;
    }

    // Matrix Emergence Commands
    if (boost::iequals(theMessage, "/clock")) {
        std::string timeStr = sWeatherSys.GetTimeString();
        WeatherSystem::CircadianPeriod period = sWeatherSys.GetCircadianPeriod();
        std::string desc;
        switch (period) {
            case WeatherSystem::PERIOD_COMMUTE_MORNING: desc = "Morning Commute"; break;
            case WeatherSystem::PERIOD_WORK: desc = "Work Hours"; break;
            case WeatherSystem::PERIOD_COMMUTE_EVENING: desc = "Evening Commute"; break;
            case WeatherSystem::PERIOD_LEISURE: desc = "Evening Leisure"; break;
            case WeatherSystem::PERIOD_REST_NIGHT: desc = "Night Rest"; break;
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:55FF55}[Matrix Clock] Time: %1% | Cycle: %2%{/c}") % timeStr % desc).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/heat")) {
        float hx = getPosition().x;
        float hz = getPosition().z;
        float heat = sMatrixThreatHeatmap.GetHeat(hx, hz);
        EscalationTier tier = sMatrixThreatHeatmap.GetTier(hx, hz);
        uint32 dId = sMatrixThreatHeatmap.GetDistrictAt(hx, hz);
        std::string dName = (dId == 1) ? "Richland" : ((dId == 2) ? "Downtown" : ((dId == 3) ? "International" : "The Slums"));
        bool sabotaged = sMatrixThreatHeatmap.IsDistrictSabotaged(dId);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFFF55}[Heatmap] District: %1% (ID %2%) | Heat: %3$.1f | Escalation Tier: %4% | Sabotaged: %5%{/c}") 
             % dName % dId % heat % (int)tier % (sabotaged ? "YES" : "NO")).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/rep")) {
        CharacterReputation rep = sWorldDirector.GetReputation(m_parent.GetCharacterId(), getHandle());
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFFF}[Living History] Notoriety: %1% | Zion: %2$.1f | Machine: %3$.1f | Mero: %4$.1f | Agents Defeated: %5% | Hardlines: %6%{/c}")
             % rep.notoriety % rep.zionStanding % rep.machineStanding % rep.meroStanding % rep.agentsDefeated % rep.hardlinesCaptured).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/crisis")) {
        const ActiveCrisis* crisis = sWorldDirector.GetCurrentCrisis();
        if (crisis) {
            uint32 elapsedSec = (getMSTime() - crisis->startTimeMs) / 1000;
            uint32 totalSec = crisis->durationMs / 1000;
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:FF5555}[Crisis Active] '%1%': %2% (Elapsed: %3%s / %4%s){/c}") 
                 % crisis->title % crisis->description % elapsedSec % totalSec).str()
            ));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:55FF55}[The Oracle] The Matrix is currently in equilibrium. No active crisis.{/c}"));
        }
        return;
    }

    if (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/crisis")) {
        std::stringstream ss(theMessage.substr(7));
        std::string subCmd;
        ss >> subCmd;
        if (boost::iequals(subCmd, "trigger")) {
            int cType = 1;
            ss >> cType;
            sWorldDirector.TriggerCrisis((WorldCrisisType)std::clamp(cType, 1, 4));
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF8800}Triggered crisis type %1%{/c}") % cType).str()));
            return;
        }
    }

    if (boost::iequals(theMessage, "/substations")) {
        const auto& subs = sLogisticsMgr.GetAllSubstations();
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}--- DISTRICT RELAY SUBSTATIONS ---{/c}"));
        for (const auto& pair : subs) {
            const auto& s = pair.second;
            std::string status = s.isSabotaged ? "{c:FF0000}SABOTAGED{/c}" : "{c:00FF00}ONLINE{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("Substation %1% [%2%]: HP %.0f/%.0f | Status: %3%") % s.substationId % s.name % s.health % s.maxHealth % status).str()
            ));
        }
        return;
    }

    if (theMessage.length() >= 9 && boost::iequals(theMessage.substr(0, 9), "/sabotage")) {
        std::stringstream ss(theMessage.substr(9));
        uint32 subId = 1;
        ss >> subId;
        bool ok = sLogisticsMgr.DamageSubstation(subId, 1500.0f, m_goId);
        if (ok) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}Substation %1% damaged/sabotaged!{/c}") % subId).str()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF0000}Failed to sabotage substation %1%. Check ID.{/c}") % subId).str()));
        }
        return;
    }

    if (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/repair")) {
        std::stringstream ss(theMessage.substr(7));
        uint32 subId = 1;
        ss >> subId;
        sLogisticsMgr.RepairSubstation(subId, 1000.0f);
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Substation %1% repaired.{/c}") % subId).str()));
        return;
    }

    if (boost::iequals(theMessage, "/help") || boost::iequals(theMessage, "/?")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}=== THE MATRIX ONLINE: REMASTER COMMANDS ==={/c}"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/attack | &attack{/c} - Engage selected target in melee interlock"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/tactic <power|speed|grab|block>{/c} - Set combat martial arts tactic"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/withdraw | &withdraw{/c} - Disengage from interlock"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/clock{/c} - Current Matrix time & circadian cycle"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/heat{/c} - Threat heatmap & law enforcement tier"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/rep{/c} - Player notoriety & faction standing"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/crisis{/c} - Active world crisis events"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/substations{/c} - District relay substation statuses"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/sabotage <id> | /repair <id>{/c} - Substation warfare"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/district <slums|dt|it|richland>{/c} - Fast district transit"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/who{/c} - Online players & active construct census"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/stats{/c} - Operator combat, HP & location stats"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/hardlines{/c} - Nearest Hardline phone booth beacon"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/stuck{/c} - Recover operator to nearest safe Hardline"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/hovercraft{/c} - Zion hovercraft flight, EMP & broadcast link"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/construct{/c} - Infinite Loading Construct sandbox"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/dojo{/c} - Oriental sparring dojo destruction status"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/backdoors{/c} - Non-Euclidean Hallway of Doors network"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/source{/c} - The Architect's Chamber & Two Doors"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/tts <persona> <text>{/c} - 24kHz Neural TTS voice synthesis"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/dispatch{/c} - Police radio scanner dispatch"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/vr{/c} - OpenXR VR 6-DOF & bullet-dodge evasion"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/apu{/c} - Zion APU walker mechanics & Dock defense status"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/gunship{/c} - Bell 212 gunship & Megacity destruction"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/redpill{/c} - Civilian awakening & Matrix glitch tracking"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/webrtc{/c} - WebGPU thin-client & AR operator audio link"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/prophecy{/c} - On-device neural LLM & daily newspapers"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/zerone{/c} - Machine City 01 & Deus Ex Machina collective"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/freeway{/c} - 101 Freeway loop & rooftop wire-fu combat"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/mobilave{/c} - Mobil Ave purgatory loop & smuggling rails"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/codevision{/c} - Green digital rain shader & cyberdecks"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/federation{/c} - Shard federation mesh & peace protocol"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/clubhel{/c} - Chateau estate & Club Hel anti-gravity shootout"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/podfields{/c} - Real-world battery towers & Harvester machine AI"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/orbital{/c} - Orbital satellite mesh & kinetic lance strikes"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/codecompile{/c} - In-engine JIT script compiler & custom katas"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/reboot{/c} - Matrix 7.0 Prime Program reboot & Singularity"));
        return;
    }

    if (boost::iequals(theMessage, "/district")) {
        // ... handled below
    }

    if (boost::iequals(theMessage, "/hovercraft")) {
        if (sHovercraftFlightSys.GetShipCount() == 0) {
            sHovercraftFlightSys.SpawnHovercraft(HOVERCRAFT_NEBUCHADNEZZAR, FlightVector3(0.0f, 400.0f, 0.0f), "Nebuchadnezzar");
        }
        auto ship = sHovercraftFlightSys.GetHovercraft(1);
        auto emp = sHovercraftFlightSys.GetEMPSystem();
        auto uplink = sHovercraftFlightSys.CalculateUplinkTelemetry(1);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[Hovercraft Flight] Ship: %1% | Pos: (%2$.0f, %3$.0f, %4$.0f) | Hull: %5$.0f%% | EMP: %6$.1f%% (%7%) | Carrier: %8% (Strength: %9$.2f){/c}")
             % (ship ? ship->name : "Nebuchadnezzar") % (ship ? ship->position.x : 0.0f) % (ship ? ship->position.y : 0.0f) % (ship ? ship->position.z : 0.0f)
             % (ship ? ship->hullIntegrity : 100.0f) % emp.chargePercent % (emp.isReady ? "READY" : "CHARGING") % uplink.statusText % uplink.signalStrength).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/construct")) {
        size_t weapons = sLoadingConstruct.GetTotalAvailableWeapons();
        size_t dummies = sLoadingConstruct.GetDummyCount();
        float dilation = sLoadingConstruct.GetTimeDilation();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFFFFF}[Loading Construct] Mode: White Void | Available Weapons: %1% | Sparring Dummies: %2% | Time Dilation: %.2fx{/c}")
             % weapons % dummies % dilation).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/dojo")) {
        size_t pillars = sLoadingConstruct.GetPillarCount();
        size_t shattered = sLoadingConstruct.GetShatteredPillarCount();
        size_t destroyedShoji = sLoadingConstruct.GetDestroyedShojiCount();
        float integrity = sLoadingConstruct.GetDojoStructuralIntegrityPercent();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFCC00}[Oriental Dojo] Pillars: %1% (%2% shattered) | Shoji Screens: 12 (%3% destroyed) | Structural Integrity: %.1f%%{/c}")
             % pillars % shattered % destroyedShoji % integrity).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/backdoors")) {
        size_t portals = sBackdoorNetwork.GetPortalCount();
        uint32 transits = sBackdoorNetwork.GetTotalTransits();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FF00}[Hallway of Doors] Active Portals: %1% | Transits: %2% | Access: Keymaker Cryptographic Keys{/c}")
             % portals % transits).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/source")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:00FFFF}[The Architect's Chamber] Subsystem offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/oracle") || (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/oracle"))) {
        uint32 charId = static_cast<uint32>(this->getCharId());
        std::string sub = (theMessage.length() > 7) ? theMessage.substr(7) : "";
        boost::trim(sub);

        if (!sub.empty() && isdigit(sub[0])) {
            // Player selected an option number: /oracle <optionId>
            uint32 optId = 0;
            try {
                optId = static_cast<uint32>(std::stoul(sub));
            } catch (const std::exception&) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}[The Oracle] That is not a valid choice right now. Type /oracle to see available options.{/c}"));
                return;
            }
            std::string reply;
            uint32 audioFx = 0;
            bool ok = sOracleDialogue.SelectDialogueOption(charId, optId, reply, audioFx);
            if (ok) {
                float valence = sOracleDialogue.GetFaithValence(charId);
                const auto* nextNode = sOracleDialogue.GetCurrentNode(charId);
                std::stringstream replySs;
                replySs << "{c:FFB300}[The Oracle] \"" << reply << "\"{/c}\n"
                        << "{c:00FFCC}(Faith Valence: " << std::fixed << std::setprecision(2) << valence << "){/c}";
                if (audioFx > 0) {
                    replySs << "\n{c:AAAAAA}[Audio FX 0x" << std::hex << std::uppercase << audioFx << " triggered]{/c}";
                }
                if (nextNode && !nextNode->isTerminal && !nextNode->options.empty()) {
                    replySs << "\n{c:FFFF55}Available Choices:{/c}";
                    for (const auto& opt : nextNode->options) {
                        replySs << "\n{c:FFFF88}  [" << opt.optionId << "] " << opt.playerChoiceText << "{/c}";
                    }
                    replySs << "\n{c:00FF88}Respond with: /oracle <optionId>{/c}";
                } else {
                    replySs << "\n{c:00FF88}[Dialogue Encounter Concluded]{/c}";
                }
                m_parent.QueueCommand(make_shared<SystemChatMsg>(replySs.str()));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}[The Oracle] That is not a valid choice right now. Type /oracle to see available options.{/c}"));
            }
            return;
        }

        // No option given: show or start encounter
        if (!sOracleDialogue.HasActiveEncounter(charId)) {
            sOracleDialogue.StartEncounter(charId);
        }

        const auto* node = sOracleDialogue.GetCurrentNode(charId);
        float valence = sOracleDialogue.GetFaithValence(charId);
        size_t totalNodes = sOracleDialogue.GetTotalDialogueNodes();
        std::stringstream ss;
        ss << "{c:FFB300}[The Oracle's Kitchen] Dialogue Nodes: " << totalNodes
           << " | Faith Valence: " << std::fixed << std::setprecision(2) << valence
           << " | Tone: " << (node ? node->emotionalTone : "Maternal") << "{/c}\n"
           << "{c:FFFF88}\"" << (node ? node->oracleSpeech : "Have another cookie, darling.") << "\"{/c}";

        if (node && !node->options.empty()) {
            ss << "\n{c:FFFF55}Available Choices:{/c}";
            for (const auto& opt : node->options) {
                ss << "\n{c:FFFF88}  [" << opt.optionId << "] " << opt.playerChoiceText << "{/c}";
            }
            ss << "\n{c:00FF88}Select by typing: /oracle <optionId>{/c}";
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
        return;
    }

    if (boost::iequals(theMessage, "/bake") || (theMessage.length() >= 5 && boost::iequals(theMessage.substr(0, 5), "/bake"))) {
        uint32 charId = static_cast<uint32>(this->getCharId());
        std::string sub = (theMessage.length() > 5) ? theMessage.substr(5) : "";
        boost::trim(sub);

        if (sub.empty() || boost::iequals(sub, "list") || boost::iequals(sub, "recipes")) {
            std::stringstream ss;
            ss << "{c:FFB300}[Oracle's Memory Bakery - Recipes]{/c}\n";
            for (const auto& pair : sOracleCookie.GetAllRecipes()) {
                const auto& r = pair.second;
                ss << "{c:FFFF88}* [" << r.cookieId << "] " << r.name << "{/c} - " << r.description << "\n"
                   << "  {c:00FFCC}Requires: ";
                for (const auto& req : r.requiredFragments) {
                    const auto* def = sOracleCookie.GetFragmentDefinition(req.first);
                    ss << (def ? def->name : "Fragment") << " x" << req.second << " ";
                }
                ss << "{/c} | {c:FFAA00}Effect: " << r.metaphysicalEffect << "{/c}\n";
            }
            ss << "{c:00FF88}Commands: /bake craft <id|name> | /cookie eat <id|name> | /bake inventory{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            return;
        }

        if (boost::istarts_with(sub, "craft ") || boost::istarts_with(sub, "bake ")) {
            std::string targetName = sub.substr(sub.find(' ') + 1);
            boost::trim(targetName);
            const CookieRecipe* r = nullptr;
            if (!targetName.empty() && isdigit(targetName[0])) {
                try {
                    r = sOracleCookie.GetRecipe(static_cast<OracleCookieId>(std::stoul(targetName)));
                } catch (const std::exception&) {
                    r = nullptr;
                }
            } else {
                r = sOracleCookie.FindRecipeByName(targetName);
            }

            if (!r) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}[Memory Bakery] Unknown cookie recipe. Type /bake list to view available recipes.{/c}"));
                return;
            }

            std::string bakeResult;
            bool ok = sOracleCookie.Bake(charId, r->cookieId, bakeResult);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:%1%}[Memory Bakery] %2%{/c}") % (ok ? "FFB300" : "FF5555") % bakeResult).str()
            ));
            return;
        }

        if (boost::iequals(sub, "inventory") || boost::iequals(sub, "pouch")) {
            const auto* pouch = sOracleCookie.GetPouch(charId);
            std::stringstream ss;
            ss << "{c:FFB300}[Memory Bakery Pouch - Operative " << charId << "]{/c}\n"
               << "{c:FFFF88}--- Data Fragments ---{/c}\n";
            for (const auto& fPair : sOracleCookie.GetAllFragmentDefinitions()) {
                uint32 count = sOracleCookie.GetFragmentCount(charId, fPair.first);
                ss << "  " << fPair.second.name << ": " << count << "\n";
            }
            ss << "{c:FFFF88}--- Baked Cookies ---{/c}\n";
            for (const auto& rPair : sOracleCookie.GetAllRecipes()) {
                uint32 count = sOracleCookie.GetCookieCount(charId, rPair.first);
                ss << "  " << rPair.second.name << ": " << count << "\n";
            }
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            return;
        }

        if (boost::iequals(sub, "grant") || boost::iequals(sub, "starter")) {
            sOracleCookie.GrantStarterKit(charId);
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFB300}[Memory Bakery] Starter data fragment pouch granted (Sugar, Flour, Spices, Yeast, Cocoa, Salt).{/c}"));
            return;
        }

        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF88}Usage: /bake list | /bake craft <name|id> | /bake inventory | /bake grant{/c}"));
        return;
    }

    if (boost::iequals(theMessage, "/cookie") || (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/cookie"))) {
        uint32 charId = static_cast<uint32>(this->getCharId());
        std::string sub = (theMessage.length() > 7) ? theMessage.substr(7) : "";
        boost::trim(sub);

        if (sub.empty() || boost::iequals(sub, "status")) {
            const auto* enc = sOracleDialogue.GetEncounterState(charId);
            float valence = sOracleDialogue.GetFaithValence(charId);
            bool hasIntuition = sStatusEffectManager.HasEffect(getGoId(), EFFECT_ORACLE_INTUITION) ||
                                sStatusEffectManager.HasEffect(charId, EFFECT_ORACLE_INTUITION);
            std::string decisionStr = (enc && enc->cookieDecision == COOKIE_DECISION_ACCEPTED) ? "ACCEPTED" :
                                      ((enc && enc->cookieDecision == COOKIE_DECISION_REFUSED) ? "REFUSED" : "PENDING");
            std::stringstream cookieSs;
            cookieSs << "{c:FFB300}[Oracle Cookie Status] Decision: " << decisionStr
                     << " | Faith Valence: " << std::fixed << std::setprecision(2) << valence
                     << " | Seraphic Intuition: " << (hasIntuition ? "ACTIVE" : "INACTIVE") << "{/c}\n"
                     << "{c:00FF88}Usage: /cookie accept | /cookie refuse | /cookie eat <name|id>{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(cookieSs.str()));
            return;
        }

        if (boost::istarts_with(sub, "eat ")) {
            std::string cookieName = sub.substr(4);
            boost::trim(cookieName);
            const CookieRecipe* r = nullptr;
            if (!cookieName.empty() && isdigit(cookieName[0])) {
                try {
                    r = sOracleCookie.GetRecipe(static_cast<OracleCookieId>(std::stoul(cookieName)));
                } catch (const std::exception&) {
                    r = nullptr;
                }
            } else {
                r = sOracleCookie.FindRecipeByName(cookieName);
            }
            if (!r) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}[Oracle Cookie] Unknown cookie. Type /bake list to see available baked cookies.{/c}"));
                return;
            }
            std::string consumeMsg;
            bool ok = sOracleCookie.ConsumeCookie(charId, r->cookieId, this, consumeMsg);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:%1%}[Oracle Cookie] %2%{/c}") % (ok ? "FFB300" : "FF5555") % consumeMsg).str()
            ));
            return;
        }

        OracleCookieChoice choice = (boost::iequals(sub, "refuse") || boost::iequals(sub, "no") || boost::iequals(sub, "decline")) 
                                     ? COOKIE_DECISION_REFUSED : COOKIE_DECISION_ACCEPTED;
        std::string consequence;
        sOracleDialogue.ExecuteCookieChoice(charId, choice, consequence, true, getGoId());
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFB300}[Oracle Cookie Choice] %1%{/c}") % consequence).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/seraph") || (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/seraph")) ||
        boost::iequals(theMessage, "/trial") || (theMessage.length() >= 6 && boost::iequals(theMessage.substr(0, 6), "/trial"))) {
        uint32 charId = static_cast<uint32>(this->getCharId());
        std::string sub = "";
        if (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/seraph")) {
            sub = (theMessage.length() > 7) ? theMessage.substr(7) : "";
        } else if (theMessage.length() >= 6 && boost::iequals(theMessage.substr(0, 6), "/trial")) {
            sub = (theMessage.length() > 6) ? theMessage.substr(6) : "";
        }
        boost::trim(sub);

        if (sub.empty() || boost::iequals(sub, "status")) {
            const auto* duel = sOracleSanctuary.GetSeraphDuelState(charId);
            bool passed = sOracleSanctuary.HasPassedSeraphTrial(charId);
            std::stringstream ss;
            ss << "{c:FFB300}[Seraph Trial Status - NPC 9101]{/c}\n"
               << "Clearance: " << (passed ? "{c:00FF88}PASSED (Inner Sanctum Open){/c}" : "{c:FF5555}PENDING (Threshold Blocked){/c}") << "\n";
            if (duel) {
                ss << "Seraph HP: " << std::fixed << std::setprecision(0) << duel->currentHealth << " / " << duel->maxHealth
                   << " | Successful Interlocks: " << duel->interlocksCompleted << "/3\n";
            }
            ss << "{c:00FF88}Usage: /seraph trial (Commence duel) | /seraph strike <damage> [counter]{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            return;
        }

        if (boost::iequals(sub, "trial") || boost::iequals(sub, "start") || boost::iequals(sub, "challenge")) {
            std::string trialMsg;
            sOracleSanctuary.StartSeraphTrial(charId, this, trialMsg);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:FFB300}[Seraph Trial] %1%{/c}") % trialMsg).str()
            ));
            return;
        }

        if (boost::istarts_with(sub, "strike ")) {
            std::stringstream ss(sub.substr(7));
            float dmg = 1000.0f;
            bool isCounter = false;
            if (!(ss >> dmg) || std::isnan(dmg) || std::isinf(dmg) || dmg < 0.0f) {
                dmg = 1000.0f;
            }
            dmg = std::clamp(dmg, 0.0f, 50000.0f);
            std::string flag;
            if (ss >> flag && (boost::iequals(flag, "counter") || boost::iequals(flag, "interlock") || flag == "1")) {
                isCounter = true;
            }
            std::string dialogue;
            bool yielded = false;
            sOracleSanctuary.ProcessSeraphDuelHit(charId, dmg, isCounter, dialogue, yielded);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:%1%}[Seraph Trial Duel] %2%{/c}") % (yielded ? "00FF88" : "FFB300") % dialogue).str()
            ));
            return;
        }

        if (boost::iequals(sub, "reset")) {
            sOracleSanctuary.ResetSeraphTrial(charId);
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF88}[Seraph Trial] Duel progress reset for character.{/c}"));
            return;
        }
    }

    if (boost::iequals(theMessage, "/vision") || (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/vision"))) {
        uint32 charId = static_cast<uint32>(this->getCharId());
        std::string sub = (theMessage.length() > 7) ? theMessage.substr(7) : "";
        boost::trim(sub);

        if (sub.empty() || boost::iequals(sub, "list")) {
            std::stringstream ss;
            ss << "{c:FFB300}[Interactive Prophetic Vision Flashbacks - Visions of the Unmade]{/c}\n";
            for (const auto& pair : sOracleVision.GetAllDefinitions()) {
                const auto& v = pair.second;
                bool completed = sOracleVision.HasCompletedSimulacrum(charId, v.id);
                ss << "{c:FFFF88}* [" << v.id << "] " << v.title << "{/c} (" << v.historicalEra << ")\n"
                   << "  Location: " << v.location << " | Completed: " << (completed ? "{c:00FF88}YES{/c}" : "{c:AAAAAA}NO{/c}") << "\n"
                   << "  {c:00FFCC}Rewards: Fragment " << v.rewardFragmentId << " x" << v.rewardFragmentCount
                   << " | Faith Shift: " << std::showpos << v.faithValenceShift << std::noshowpos << "{/c}\n";
            }
            ss << "{c:00FF88}Usage: /vision start <1-4|name> | /vision status | /vision end{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            return;
        }

        if (boost::istarts_with(sub, "start ") || boost::istarts_with(sub, "trigger ")) {
            std::string query = sub.substr(sub.find(' ') + 1);
            boost::trim(query);
            const VisionSimulacrumDef* def = nullptr;
            if (!query.empty() && isdigit(query[0])) {
                try {
                    def = sOracleVision.GetSimulacrumDef(static_cast<SimulacrumId>(std::stoul(query)));
                } catch (const std::exception&) {
                    def = nullptr;
                }
            } else {
                def = sOracleVision.FindSimulacrumByName(query);
            }

            if (!def) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}[Prophetic Vision] Unknown simulacrum. Type /vision list to see available visions.{/c}"));
                return;
            }

            std::string narrative;
            sOracleVision.StartTrance(charId, def->id, this, narrative);
            return;
        }

        if (boost::iequals(sub, "end") || boost::iequals(sub, "stop")) {
            std::string endMsg;
            sOracleVision.EndTrance(charId, this, endMsg);
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FFB300}[Prophetic Vision] %1%{/c}") % endMsg).str()));
            return;
        }

        if (boost::iequals(sub, "status")) {
            const auto* state = sOracleVision.GetTranceState(charId);
            bool inTrance = sOracleVision.IsInTrance(charId);
            std::stringstream ss;
            ss << "{c:FFB300}[Prophetic Vision Status - Operative " << charId << "]{/c}\n"
               << "Active Trance: " << (inTrance ? "IN PROGRESS" : "IDLE") << "\n"
               << "Completed Simulacra: " << (state ? state->completedSimulacra.size() : 0) << " / 4\n";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            return;
        }
    }

    if (boost::iequals(theMessage, "/skybox") || (theMessage.length() >= 7 && boost::iequals(theMessage.substr(0, 7), "/skybox"))) {
        std::string sub = (theMessage.length() > 7) ? theMessage.substr(7) : "";
        boost::trim(sub);

        if (sub.empty() || boost::iequals(sub, "status")) {
            auto curState = sOracleSanctuary.GetCurrentSkyboxState();
            auto pal = sOracleSanctuary.GetSkyboxPalette(curState);
            std::stringstream ss;
            ss << "{c:FFB300}[Sati's Living Environmental Canvas]{/c}\n"
               << "Active State: " << pal.name << " (State ID: " << static_cast<int>(curState) << ")\n"
               << "Primary Tone: " << pal.primaryHex << " | Accent: " << pal.accentHex << " | Ambient: " << pal.ambientHex << "\n"
               << "{c:FFFF88}\"" << pal.description << "\"{/c}\n"
               << "{c:00FF88}Usage: /skybox set <0=Zion|1=Machine|2=Merovingian|3=Sunrise>{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
            return;
        }

        if (boost::istarts_with(sub, "set ")) {
            std::string stateStr = sub.substr(4);
            boost::trim(stateStr);
            int sid = -1;
            try {
                sid = std::stoi(stateStr);
            } catch (const std::exception&) {
                sid = -1;
            }
            if (sid >= 0 && sid <= 3) {
                sOracleSanctuary.SetSkyboxState(static_cast<SatiSkyboxState>(sid));
                auto pal = sOracleSanctuary.GetSkyboxPalette(static_cast<SatiSkyboxState>(sid));
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:FFB300}[Sati's Skybox] Horizon shifted to %1% (%2%).{/c}") % pal.name % pal.primaryHex).str()
                ));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}[Sati's Skybox] Invalid state ID. Use 0=Zion, 1=Machine, 2=Merovingian, 3=Sunrise.{/c}"));
            }
            return;
        }
    }

    if (boost::iequals(theMessage, "/sanctuary") || (theMessage.length() >= 10 && boost::iequals(theMessage.substr(0, 10), "/sanctuary"))) {
        std::string sub = (theMessage.length() > 10) ? theMessage.substr(10) : "";
        boost::trim(sub);

        auto pos = this->getPosition();
        std::string curSanctuaryName;
        bool inSanctuary = sOracleSanctuary.IsInSanctuary(pos.x, pos.y, pos.z, curSanctuaryName);

        if (sub.empty() && inSanctuary) {
            uint32 charId = static_cast<uint32>(this->getCharId());
            if (curSanctuaryName == "Park East Bench") {
                sOracleSanctuary.TriggerSatiSunrise(charId, this);
            }
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FF88}[Sanctuary Active] You are within the serene threshold of %1%.{/c}") % curSanctuaryName).str()
            ));
            return;
        }

        std::string targetEnclave = sub.empty() ? "Tenement Kitchen" : sub;
        sOracleSanctuary.TeleportToSanctuary(this, targetEnclave);
        return;
    }

    if (theMessage.length() >= 4 && boost::iequals(theMessage.substr(0, 4), "/tts")) {
        std::stringstream ss(theMessage.substr(4));
        std::string personaStr;
        ss >> personaStr;
        std::string text;
        std::getline(ss, text);
        if (text.empty()) text = "Welcome to the Desert of the Real.";
        while (!text.empty() && text.front() == ' ') text.erase(0, 1);

        VoicePersona p = VOICE_PERSONA_MORPHEUS;
        if (boost::iequals(personaStr, "oracle")) p = VOICE_PERSONA_ORACLE;
        else if (boost::iequals(personaStr, "smith")) p = VOICE_PERSONA_AGENT_SMITH;
        else if (boost::iequals(personaStr, "merovingian") || boost::iequals(personaStr, "mero")) p = VOICE_PERSONA_MEROVINGIAN;

        SynthesizedAudioClip clip;
        bool ok = sNeuralVoiceSystem.SynthesizeContactVoice(p, text, clip);
        if (ok) {
            auto prof = sNeuralVoiceSystem.GetVoiceProfile(p);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:55FF55}[Neural TTS] Synthesized %1%ms 24kHz audio for %2%: '%3%'{/c}")
                 % clip.durationMs % (prof ? prof->displayName : "Contact") % text).str()
            ));
        }
        return;
    }

    if (boost::iequals(theMessage, "/dispatch")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:AAAAAA}[Police Radio Dispatch] Scanner frequency offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/vr")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:00FFCC}[OpenXR VR Pipeline] Subsystem offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/apu")) {
        auto apu = sAPUCombatSystem.GetAPU(1);
        auto log = sAPUCombatSystem.GetLogistics();
        size_t apuCount = sAPUCombatSystem.GetActiveAPUCount();
        size_t breaches = sAPUCombatSystem.GetActiveBreachCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFCC00}[Zion APU Walker] Unit: %1% (%2%) | Ammo: L:%3% R:%4% | Barrel Temp: L:%5$.1fC R:%6$.1fC | Pressure: %7$.0f PSI | Breaches: %8% | Defense Grid: %9$.0f%%{/c}")
             % (apu ? apu->pilotHandle : "Unassigned") % (apu && apu->isMounted ? "MOUNTED" : "UNMANNED")
             % (apu ? apu->leftArm.ammoRoundsRemaining : 0) % (apu ? apu->rightArm.ammoRoundsRemaining : 0)
             % (apu ? apu->leftArm.barrelTempCelsius : 25.0f) % (apu ? apu->rightArm.barrelTempCelsius : 25.0f)
             % (apu ? apu->hydraulicPressurePsi : 3000.0f) % breaches % log.defenseGridAllocationPercent).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/gunship")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:00FFCC}[Megacity Gunship] Subsystem offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/redpill")) {
        size_t total = sRedpillAwakeningSystem.GetPotentialCount();
        size_t awakened = sRedpillAwakeningSystem.GetAwakenedCount();
        size_t extracted = sRedpillAwakeningSystem.GetTotalExtracted();
        auto pot = sRedpillAwakeningSystem.GetPotential(1);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:55FF55}[Redpill Awakening] Tracked Potentials: %1% | Awakened: %2% | Extracted: %3% | Target: %4% (Disbelief: %5$.1f%%, Deja Vu: %6%){/c}")
             % total % awakened % extracted % (pot ? pot->civilianName : "None")
             % (pot ? pot->systemDisbeliefPercent : 0.0f) % (pot ? pot->dejaVuCatOccurrences : 0)).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/webrtc")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:00FF99}[WebGPU Bridge] Subsystem offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/prophecy")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:FF55FF}[Neural Prophecy Engine] Subsystem offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/zerone")) {
        auto deus = sMachineCitySystem.GetDeusState();
        size_t severed = sMachineCitySystem.GetSeveredTetherCount();
        size_t bps = sMachineCitySystem.GetTotalBlueprintCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FF9900}[Zero-One Machine City] Deus Ex Machina: Phase %1% | Emotional State: %2% | Drones: %3% | Shield: %4$.0f%% | Tethers Severed: %5%/4 | Blueprints: %6% | Truce: %7%{/c}")
             % (int)deus.currentPhase % (int)deus.emotionalState % deus.swarmDroneCount
             % deus.vortexShieldIntegrity % severed % bps % (deus.peaceTreatyRatified ? "RATIFIED" : "PENDING")).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/freeway")) {
        size_t vCount = sFreewayCombatSystem.GetVehicleCount();
        size_t duels = sFreewayCombatSystem.GetActiveRoofDuelCount();
        auto escort = sFreewayCombatSystem.GetEscortState();
        bool twin1Phased = sFreewayCombatSystem.IsTwinPhased(1);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00CCFF}[101 Freeway Chase] Vehicles: %1% | Rooftop Duels: %2% | Keymaker Escort: %3$.1f%% (HP: %4$.0f) | Twins Phase: %5%{/c}")
             % vCount % duels % escort.progressPercent % escort.escortHealth % (twin1Phased ? "ETHEREAL" : "SOLID")).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/mobilave")) {
        auto train = sMobilAveRailSystem.GetTrain();
        auto boss = sMobilAveRailSystem.GetTrainmanState();
        size_t smuggleRuns = sMobilAveRailSystem.GetActiveSmuggleRunCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFFF55}[Mobil Ave Station] Train #%1% [%2%mph]: %3% | Destination: %4% | Smuggling Runs: %5% | Trainman HP: %6$.0f (Time Dilation: %7$.2fx){/c}")
             % train.trainId % train.speedMph % (sMobilAveRailSystem.IsTrainDocked() ? "DOCKED" : (train.isHazardActive ? "EXPRESS HAZARD" : "IN TRANSIT"))
             % train.destinationShard % smuggleRuns % boss.health % boss.timeDilationFactor).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/codevision")) {
        auto cv = sCyberdeckHackingSystem.GetCodeVisionConfig();
        size_t termCount = sCyberdeckHackingSystem.GetTerminalCount();
        size_t compCount = sCyberdeckHackingSystem.GetCompromisedTerminalCount();
        CyberdeckHardware deck;
        bool hasDeck = sCyberdeckHackingSystem.GetDeck(1, deck);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FF00}[Code Vision & Cyberdeck] Green Rain: %1% (Density: %2$.1f, Speed: %3$.1f) | Terminals: %4% (%5% compromised) | Rig: %6% (%7%MB, %8$.1fGHz){/c}")
             % (cv.isEnabled ? "ACTIVE" : "OFF") % cv.glyphDensity % cv.rainVelocity
             % termCount % compCount % (hasDeck ? deck.deckModel : "None") % (hasDeck ? deck.installedRamMb : 0) % (hasDeck ? deck.busSpeedGhz : 0.0f)).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/federation")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:CC99FF}[Shard Federation] Subsystem offline.{/c}"
        ));
        return;
    }

    if (boost::iequals(theMessage, "/clubhel")) {
        auto chateau = sClubHelRaidSystem.GetChateauState();
        size_t exiles = sClubHelRaidSystem.GetActiveExileCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FF3366}[Club Hel & Chateau] Exiles Active: %1% | Grand Staircase HP: %2$.0f%% | Statues Shattered: %3%/6 | Secret Cellar: %4% | Persephone Vault: %5%{/c}")
             % exiles % chateau.grandStaircaseIntegrity % chateau.destroyedStatues
             % (chateau.secretCellarUnlocked ? "UNLOCKED" : "LOCKED")
             % (chateau.persephoneVaultOpen ? "OPEN" : "SEALED")).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/podfields")) {
        auto tower = sPodHarvestSystem.GetTowerState();
        size_t activePods = sPodHarvestSystem.GetActivePodCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FF6600}[Power Plant Battery Tower] Active Pods: %1% (Total Grid: %2%) | Output: %3$.1f MW | Copper-Tops Rescued: %4% | Purged: %5%{/c}")
             % activePods % tower.totalPodsCount % tower.totalEnergyOutputMw % tower.rescuedHumanCount % tower.purgedHumanCount).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/orbital")) {
        size_t sats = sOrbitalSatelliteSystem.GetSatelliteCount();
        size_t pirate = sOrbitalSatelliteSystem.GetPirateTransponderCount();
        bool beamActive = sOrbitalSatelliteSystem.IsSolarBeamActive();
        auto strike = sOrbitalSatelliteSystem.GetLatestStrike();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:33CCFF}[Orbital Satellite Mesh] Satellites: %1% (%2% pirate relays) | Solar Recharge Beam: %3% | Kinetic Lance: %4% (Last Energy: %5$.1f GJ){/c}")
             % sats % pirate % (beamActive ? "LOCKED (150 MW)" : "STANDBY")
             % (strike.isDischarged ? "DISCHARGED" : "READY") % strike.impactKineticEnergyGj).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/codecompile")) {
        auto tele = sSourceCodeCompilerSystem.GetTelemetry();
        size_t blocks = sSourceCodeCompilerSystem.GetCompiledBlockCount();
        size_t katas = sSourceCodeCompilerSystem.GetCustomKataCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FF66}[JIT Code Compiler] Compiled Blocks: %1% | Custom Katas: %2% | Avg Latency: %3$.2fms | Sanitized Execs: %4% | Rejected Exploits: %5%{/c}")
             % blocks % katas % tele.averageCompilationTimeMs % tele.sanitizedExecutions % tele.rejectedExploits).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/reboot")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            "{c:FFD700}[Matrix Reboot Engine] Subsystem offline.{/c}"
        ));
        return;
    }

    if (theMessage.length() >= 9 && boost::iequals(theMessage.substr(0, 9), "/district")) {
        std::stringstream ss(theMessage.substr(9));
        std::string targetDistrict;
        ss >> targetDistrict;
        boost::to_lower(targetDistrict);
        LocationVector targetPos(99640.0f, 500.0f, 8350.0f);
        std::string distName = "The Slums";

        if (targetDistrict == "dt" || targetDistrict == "downtown") {
            targetPos = LocationVector(39216.0f, 500.0f, -21475.0f);
            distName = "Downtown";
        } else if (targetDistrict == "it" || targetDistrict == "international") {
            targetPos = LocationVector(-37444.0f, 500.0f, 23659.0f);
            distName = "International";
        } else if (targetDistrict == "richland") {
            targetPos = LocationVector(-15000.0f, 500.0f, -15000.0f);
            distName = "Richland";
        } else if (targetDistrict == "slums") {
            targetPos = LocationVector(99640.0f, 500.0f, 8350.0f);
            distName = "The Slums";
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}Unknown district. Valid: slums, dt, it, richland{/c}"));
            return;
        }

        this->setPosition(targetPos);
        sGame.AnnounceStateUpdate(NULL, make_shared<PositionStateMsg>(m_goId));
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}[Transit] Transferred to %1% hardline network.{/c}") % distName).str()));
        return;
    }

    if (boost::iequals(theMessage, "/who")) {
        auto allIds = sObjMgr.getAllGOIds();
        uint32 humanCount = 0;
        for (auto id : allIds) {
            PlayerObject* p = sObjMgr.getGOPtrSafe(id);
            if (p && !p->getClient().isBot()) humanCount++;
        }
        size_t botCount = sBotMgr.GetBotCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FF00}[Zion Census] Operators Jacked In: %1% | Active Construct Bots: %2% | Hardlines: 135{/c}") 
             % humanCount % botCount).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/stats")) {
        LocationVector pos = getPosition();
        uint32 dId = sMatrixThreatHeatmap.GetDistrictAt(pos.x, pos.z);
        std::string dName = (dId == 1) ? "Richland" : ((dId == 2) ? "Downtown" : ((dId == 3) ? "International" : "The Slums"));
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0);
        ss << "{c:00FFFF}[Operator Stats] Handle: " << m_handle 
           << " | Level: " << (int)getLevel() 
           << " | HP: " << (int)getCurrentHealth() << "/" << (int)getMaximumHealth() 
           << " | IS: " << (int)getCurrentIS() << "/" << (int)getMaximumIS() 
           << " | District: " << dName 
           << " | Pos: (" << pos.x << ", " << pos.y << ", " << pos.z << "){/c}";
        m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
        return;
    }

    if (boost::iequals(theMessage, "/hardlines")) {
        LocationVector nearestHl = sBotMgr.GetNearestHardline(getPosition().x, getPosition().z);
        float dist = sqrt(nearestHl.DistanceSq(getPosition()));
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[Hardline Relay] Total Hardlines: 135 | Nearest Hardline: (%.0f, %.0f, %.0f) - Range: %.1fm{/c}")
             % nearestHl.x % nearestHl.y % nearestHl.z % (dist / 100.0f)).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/stuck")) {
        LocationVector nearestHl = sBotMgr.GetNearestHardline(getPosition().x, getPosition().z);
        nearestHl.y += 20.0f;
        this->setPosition(nearestHl);
        sGame.AnnounceStateUpdate(NULL, make_shared<PositionStateMsg>(m_goId));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}[Emergency Unstuck] Repositioned operator to nearest safe Hardline phone booth.{/c}"));
        return;
    }

    // Authentic Vendor & Economy Chat Commands
    if (boost::iequals(theMessage, "/vendor") || boost::iequals(theMessage, "/shop") || boost::iequals(theMessage, "/vendor list")) {
        auto vendors = sEconomySys.GetVendorsForDistrict(m_district);
        if (vendors.empty()) vendors = sEconomySys.GetVendorsForDistrict(1);
        if (!vendors.empty()) {
            const HardlineVendor* nearest = &vendors[0];
            double bestDist = 1e9;
            for (const auto& v : vendors) {
                double d = (m_pos.x - v.x)*(m_pos.x - v.x) + (m_pos.z - v.z)*(m_pos.z - v.z);
                if (d < bestDist) {
                    bestDist = d;
                    nearest = &v;
                }
            }
            m_parent.QueueCommand(std::make_shared<VendorOpenMsg>(
                500.0f, nearest->x, nearest->y, nearest->z, nearest->inventoryTemplates
            ));
            std::stringstream ss;
            ss << "{c:00FFCC}[VENDOR] " << nearest->name << " (ID: " << nearest->staticId << ") Catalog:\n";
            size_t count = 0;
            for (uint32 tplId : nearest->inventoryTemplates) {
                if (++count > 8) { ss << "... and " << (nearest->inventoryTemplates.size() - 8) << " more items."; break; }
                const ItemTemplate* tpl = sDataLoader.GetItemTemplate(tplId);
                std::string name = tpl ? tpl->name : (format("Item %1%") % tplId).str();
                uint32 price = tpl && tpl->value > 0 ? tpl->value : sEconomySys.GetItemPrice(tplId);
                ss << " - " << name << " (ID " << tplId << "): " << price << " $Info\n";
            }
            ss << "{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] No active district vendors found.{/c}"));
        }
        return;
    }

    if (boost::istarts_with(theMessage, "/vendor buy ") || boost::istarts_with(theMessage, "/shop buy ")) {
        std::string arg = theMessage.substr(theMessage.find("buy ") + 4);
        boost::trim(arg);
        try {
            uint32 tplId = std::stoul(arg);
            ByteBuffer buyCmd;
            buyCmd << uint32(tplId) << uint32(0);
            RPC_HandleVendorBuy(buyCmd);
        } catch (...) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /vendor buy <itemTemplateId>{/c}"));
        }
        return;
    }

    if (boost::istarts_with(theMessage, "/vendor sell ") || boost::istarts_with(theMessage, "/shop sell ")) {
        std::string arg = theMessage.substr(theMessage.find("sell ") + 5);
        boost::trim(arg);
        try {
            uint32 itemId = std::stoul(arg);
            ByteBuffer sellCmd;
            sellCmd << uint32(itemId) << uint32(0);
            RPC_HandleVendorSell(sellCmd);
        } catch (...) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /vendor sell <itemId>{/c}"));
        }
        return;
    }

    // Authentic Marketplace Chat Commands (/market, &market, /market list, /market sell, /market buy)
    if (boost::iequals(theMessage, "/market") || boost::iequals(theMessage, "&market") ||
        boost::iequals(theMessage, "/market list") || boost::iequals(theMessage, "&market list") ||
        boost::iequals(theMessage, "/marketplace") || boost::iequals(theMessage, "&marketplace")) {
        ByteBuffer dummy;
        RPC_HandleMarketListItems(dummy);
        return;
    }

    if (boost::istarts_with(theMessage, "/market sell ") || boost::istarts_with(theMessage, "&market sell ") ||
        boost::istarts_with(theMessage, "/market list ") || boost::istarts_with(theMessage, "&market list ")) {
        std::string args = theMessage.substr(theMessage.find(" ") + 1);
        if (args.find("sell ") == 0) args = args.substr(5);
        else if (args.find("list ") == 0) args = args.substr(5);
        boost::trim(args);
        std::vector<std::string> parts;
        boost::split(parts, args, boost::is_any_of(" "), boost::token_compress_on);
        if (parts.size() >= 2) {
            try {
                uint32 tplId = std::stoul(parts[0]);
                uint32 price = std::stoul(parts[1]);
                uint64 listingId = sEconomySys.ListVendorItem(this, tplId, price);
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFCC}[MARKETPLACE] Listed item %1% on Exchange for %2% $Info (Listing ID: %3%){/c}")
                        % tplId % price % listingId).str()
                ));
            } catch (...) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /market sell <templateId> <price>{/c}"));
            }
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /market sell <templateId> <price>{/c}"));
        }
        return;
    }

    if (boost::istarts_with(theMessage, "/market buy ") || boost::istarts_with(theMessage, "&market buy ")) {
        std::string arg = theMessage.substr(theMessage.find("buy ") + 4);
        boost::trim(arg);
        try {
            uint64 listingId = std::stoull(arg);
            bool ok = sEconomySys.PurchaseVendorItem(this, listingId);
            if (ok) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[MARKETPLACE] Successfully purchased listing ID %1%!{/c}") % listingId).str()
                ));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:FF4444}[MARKETPLACE] Purchase failed for listing ID %1% (insufficient funds, full inventory, or invalid listing).{/c}") % listingId).str()
                ));
            }
        } catch (...) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /market buy <listingId>{/c}"));
        }
        return;
    }

    if (boost::iequals(theMessage, "/jackout") || boost::iequals(theMessage, "/exit")) {
        ByteBuffer dummy;
        RPC_HandleJackoutRequest(dummy);
        return;
    }

    if (boost::iequals(theMessage, "/hardline") || boost::iequals(theMessage, "/hardlines")) {
        const HardlineNode* nearest = GetNearestHardline(m_district, getPosition().x, getPosition().z);
        size_t total = GetTotalHardlines();
        if (nearest) {
            double dist = sqrt((nearest->x - getPosition().x)*(nearest->x - getPosition().x) + (nearest->z - getPosition().z)*(nearest->z - getPosition().z));
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << "{c:00FFCC}[HARDLINE] Nearest: " << nearest->name 
               << " (District " << (int)nearest->districtId << ", #" << (int)nearest->hardlineId << ")"
               << " | Distance: " << (dist / 100.0) << "m"
               << " | Total Matrix Hardlines: " << total << "{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
        } else {
            std::ostringstream ss;
            ss << "{c:00FFCC}[HARDLINE] Total Matrix Hardlines: " << total << "{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
        }
        return;
    }

    if (boost::iequals(theMessage, "/hardline list")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}--- HARDLINES IN DISTRICT %1% ---{/c}") % (int)m_district).str()));
        int count = 0;
        for (const auto& kv : GetHardlineDirectory()) {
            if (kv.first.first == m_district && count < 8) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(0);
                ss << "  Node #" << (int)kv.second.hardlineId << ": " << kv.second.name
                   << " at (" << kv.second.x << ", " << kv.second.y << ", " << kv.second.z << ")";
                m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
                count++;
            }
        }
        return;
    }

    if (boost::istarts_with(theMessage, "/teleport ") || boost::istarts_with(theMessage, "/tp ")) {
        std::stringstream ss(theMessage.substr(theMessage.find(' ') + 1));
        uint32 destDistrict = 0;
        uint32 destHL = 0;
        if (ss >> destDistrict >> destHL) {
            ByteBuffer tpCmd;
            tpCmd << (uint8)1; // source HL
            while (tpCmd.size() < 6) tpCmd << (uint8)0;
            tpCmd << (uint8)m_district;
            while (tpCmd.size() < 10) tpCmd << (uint8)0;
            tpCmd << (uint8)destHL;
            while (tpCmd.size() < 14) tpCmd << (uint8)0;
            tpCmd << (uint8)destDistrict;
            RPC_HandleHardlineTeleport(tpCmd);
            return;
        }
    }

    if (boost::iequals(theMessage, "/slmstats")) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}[SLM Dialogue Engine] Subsystem offline.{/c}"));
        return;
    }

    if (theMessage.length() >= 10 && (boost::iequals(theMessage.substr(0, 10), "/epistemic") || boost::iequals(theMessage.substr(0, 10), "!epistemic"))) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}[Epistemic Horizon] Subsystem offline.{/c}"));
        return;
    }

    if (theMessage.length() >= 9 && (boost::iequals(theMessage.substr(0, 9), "/dialogue") || boost::iequals(theMessage.substr(0, 9), "!dialogue"))) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}[SLM Dialogue Engine] Subsystem offline.{/c}"));
        return;
    }

    // Item 43: Director Mode Possession Chat Reroute
    if (m_possessingBotId != 0) {
        auto bot = sBotMgr.GetBotByGOID(m_possessingBotId);
        if (bot) {
            bot->Say(theMessage);
            return;
        }
    }

	INFO_LOG(format("%1% says %2%") % m_handle % theMessage);
	m_parent.QueueCommand(make_shared<SystemChatMsg>((format("You said %1%") % theMessage).str()));
	sGame.AnnounceCommand(&m_parent,make_shared<PlayerChatMsg>(m_handle,theMessage));

    // Dynamic Contextual Local NPC Chat Response (Phase 14)
    std::string npcReply, npcName;
    if (sNeuralVoiceSystem.ProcessLocalChatSay(m_goId, theMessage, getPosition().x, getPosition().z, npcReply, npcName)) {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:AAAAAA}[Local] %1% murmurs: \"%2%\"{/c}") % npcName % npcReply).str()
        ));
    }
}

void PlayerObject::RPC_HandleWhisper( ByteBuffer &srcCmd )
{
	uint8 thirdByte = srcCmd.read<uint8>();

	if (thirdByte != 0)
		WARNING_LOG(format("(%1%) Whisper packet third byte not 0 but %2%, packet %3%") % m_parent.Address() % uint32(thirdByte) % Bin2Hex(srcCmd));

	uint16 messageLenPos = srcCmd.read<uint16>();
	uint16 whisperCount = srcCmd.read<uint16>();
	whisperCount = swap16(whisperCount); //big endian in packet

	string theRecipient = srcCmd.readString();
	string serverPrefix = sGame.GetChatPrefix() + string("+");
	string::size_type prefixPos = theRecipient.find_first_of(serverPrefix);
	if (prefixPos != string::npos)
		theRecipient = theRecipient.substr(prefixPos+serverPrefix.length());

	if (srcCmd.rpos() != messageLenPos)
		WARNING_LOG(format("(%1%) Whisper packet size byte mismatch, packet %2%") % m_parent.Address() % Bin2Hex(srcCmd));

	string theMessage = srcCmd.readString();

	bool sentProperly=false;
	vector<uint32> objectLists = sObjMgr.getAllGOIds();
	foreach (int currObj, objectLists)
	{
		PlayerObject* targetPlayer = sObjMgr.getGOPtrSafe(currObj);

		if (targetPlayer == NULL)
			continue;

		if (iequals(targetPlayer->getHandle(),theRecipient))
		{
			targetPlayer->getClient().QueueCommand(make_shared<WhisperMsg>(m_handle,theMessage));
			sentProperly=true;
		}
	}
	if (sentProperly)
	{
		INFO_LOG(format("%1% whispered to %2%: %3%") % m_handle % theRecipient % theMessage);
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("You whispered %1% to %2%")%theMessage%theRecipient).str()));
	}
	else
	{
		INFO_LOG(format("%1% sent whisper to disconnected player %2%: %3%") % m_handle % theRecipient % theMessage);
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("%1% is not online")%theRecipient).str()));
	}
}

void PlayerObject::RPC_HandleStopAnimation( ByteBuffer &srcCmd )
{
	m_currAnimation = 0;
	sGame.AnnounceStateUpdate(NULL,make_shared<AnimationStateMsg>(m_goId));
}

void PlayerObject::RPC_HandleStartAnimtion( ByteBuffer &srcCmd )
{
	uint8 newAnimation = srcCmd.read<uint8>();
	m_currAnimation = newAnimation;
	sGame.AnnounceStateUpdate(NULL,make_shared<AnimationStateMsg>(m_goId));
}

void PlayerObject::RPC_HandleChangeMood( ByteBuffer &srcCmd )
{
	uint8 newMood = srcCmd.read<uint8>();
	m_currMood = newMood;	
	sGame.AnnounceStateUpdate(NULL,make_shared<AnimationStateMsg>(m_goId));
	return;
}

void PlayerObject::RPC_HandlePerformEmote( ByteBuffer &srcCmd )
{
	uint32 emoteId = srcCmd.read<uint32>();
	uint32 emoteTarget = srcCmd.read<uint32>();

	m_emoteCounter++;
	sGame.AnnounceStateUpdate(NULL,make_shared<EmoteMsg>(m_goId,emoteId,m_emoteCounter));

	DEBUG_LOG(format("(%1%) %2%:%3% doing emote %4% on target %5% at coords %6%,%7%,%8%")
		% m_parent.Address()
		% m_handle
		% m_goId
		% Bin2Hex((const byte*)&emoteId,sizeof(emoteId),0)
		% Bin2Hex((const byte*)&emoteTarget,sizeof(emoteTarget),0)
		% m_pos.x % m_pos.y % m_pos.z );
}

void PlayerObject::RPC_HandleDynamicObjInteraction( ByteBuffer &srcCmd )
{
	uint16 viewId = srcCmd.read<uint16>();
	uint16 objType = srcCmd.read<uint16>();
	uint16 interaction = srcCmd.read<uint16>();

	format debugStr = 
		format("(%s) %s:%d interacting with dynamic view id 0x%04x object type 0x%04x interaction %d")
		% m_parent.Address()
		% m_handle
		% m_goId
		% viewId
		% objType
		% int(interaction);

	INFO_LOG( debugStr );
	
    // V17: The Mission Interaction (NPC TALK / GIVE)
    uint32 targetGoId = sObjMgr.getGOForView(&m_parent, viewId);
    if (targetGoId > 0)
    {
        // Must actually be next to the NPC (this used to complete from anywhere in the world)
        PlayerObject* npc = sObjMgr.getGOPtrSafe(targetGoId);
        if (npc && npc != this && m_pos.Distance(npc->getPosition()) > 2000.0)
        {
            INFO_LOG(format("%1%: interaction with %2% refused, %3% units away") % m_handle % targetGoId % m_pos.Distance(npc->getPosition()));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}You are too far away.{/c}"));
            return;
        }

        // Check if target is a Vendor NPC
        const HardlineVendor* vendor = sEconomySys.GetHardlineVendor(targetGoId);
        if (!vendor && npc && (npc->getHandle().find("Merchant") != std::string::npos ||
                               npc->getHandle().find("Vendor") != std::string::npos ||
                               npc->getHandle().find("Trader") != std::string::npos))
        {
            auto districtVendors = sEconomySys.GetVendorsForDistrict(m_district);
            if (districtVendors.empty()) districtVendors = sEconomySys.GetVendorsForDistrict(1);
            if (!districtVendors.empty()) vendor = &districtVendors[0];
        }
        if (vendor && !vendor->inventoryTemplates.empty())
        {
            m_parent.QueueCommand(std::make_shared<VendorOpenMsg>(
                500.0f,
                vendor->x,
                vendor->y,
                vendor->z,
                vendor->inventoryTemplates
            ));
            m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:00FFCC}[VENDOR] Opened %1% (Vendor ID: %2%, Items: %3%){/c}")
                    % vendor->name % vendor->staticId % vendor->inventoryTemplates.size()).str()
            ));
            return;
        }

        // Handing a mission item over is the same client interaction as talking (no other RPC
        // reaches GIVE objectives), so dispatch on what the current objective expects.
        ActiveObjectiveInfo info;
        ObjectiveCommand cmd = ObjectiveCommand::TALK;
        if (sMissionSys.GetActiveObjectiveInfo(m_goId, info))
        {
            if (info.command == ObjectiveCommand::GIVE) cmd = ObjectiveCommand::GIVE;
            else if (info.command == ObjectiveCommand::HACK) cmd = ObjectiveCommand::HACK;
            else if (info.command == ObjectiveCommand::USE_ITEM) cmd = ObjectiveCommand::USE_ITEM;
            else if (info.command == ObjectiveCommand::ESCORT) cmd = ObjectiveCommand::ESCORT;
        }
        sMissionSys.AdvanceObjective(this, cmd, targetGoId);
    }
    else
    {
        // Item 120: Archives & Data Fragments
        if (objType == 0x1234) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}You collected an Archive Data Fragment! Gained 500 Info.{/c}"));
            addInformation(500);
            return;
        }

        // Item 47: Crafting Nodes
        // If targetGoId is not a character (or no specific target found), treat as a node extraction
        if (interaction == 2 || interaction == 3) // Example interaction ids for use
        {
            if (getInventory()) {
                const auto& allItems = sDataLoader.GetAllItems();
                if (!allItems.empty()) {
                    auto it = allItems.begin();
                    std::advance(it, rand() % allItems.size());
                    
                    auto dropItem = std::make_shared<Item>(rand(), it->second.templateId);
                    if (getInventory()->addItemAuto(dropItem)) {
                        m_parent.QueueCommand(make_shared<SystemChatMsg>(
                            (format("{c:00FF00}You extracted data component: %1%.{/c}") % it->second.name).str()
                        ));
                        addInformation(50);
                    }
                }
            }
        }
    }
}

void PlayerObject::RPC_HandleStaticObjInteraction( ByteBuffer &srcCmd )
{
	uint32 staticObjId = srcCmd.read<uint32>();
	uint16 interaction = srcCmd.read<uint16>();

	format debugStr = 
		format("(%s) %s:%d interacting with object id 0x%08x interaction %d")
		% m_parent.Address()
		% m_handle
		% m_goId
		% staticObjId
		% int(interaction);

	INFO_LOG( debugStr );

	m_parent.QueueCommand(make_shared<SystemChatMsg>(debugStr.str()));

	// Check if this static object is an authentic vendor (or NPC vendor interaction)
	const HardlineVendor* vendor = sEconomySys.GetHardlineVendor(staticObjId);
	std::vector<HardlineVendor> districtVendors;
	if (!vendor && interaction == 0x02) // 0x02 = HUMAN_NPC / Vendor in HDS ObjectInteractionHandler
	{
		districtVendors = sEconomySys.GetVendorsForDistrict(m_district);
		if (districtVendors.empty()) districtVendors = sEconomySys.GetVendorsForDistrict(1);
		if (!districtVendors.empty())
		{
			vendor = &districtVendors[0];
			double bestDist = 1e9;
			for (const auto& v : districtVendors)
			{
				double d = (m_pos.x - v.x)*(m_pos.x - v.x) + (m_pos.z - v.z)*(m_pos.z - v.z);
				if (d < bestDist)
				{
					bestDist = d;
					vendor = &v;
				}
			}
		}
	}
	if (vendor && !vendor->inventoryTemplates.empty())
	{
		m_parent.QueueCommand(std::make_shared<VendorOpenMsg>(
			500.0f,
			vendor->x,
			vendor->y,
			vendor->z,
			vendor->inventoryTemplates
		));
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
			(format("{c:00FFCC}[VENDOR] Opened %1% (Vendor ID: %2%, Items: %3%){/c}")
				% vendor->name % vendor->staticId % vendor->inventoryTemplates.size()).str()
		));
		if (vendor->name.find("Tailor") != std::string::npos || vendor->name.find("Clothing") != std::string::npos)
		{
			m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
				"{c:00FF00}[TAILOR] Custom dye available! Use /dye <coat|shirt|pants|shoes|glasses|hair> <0-31> to alter your appearance palette.{/c}"
			));
		}
		return;
	}

	if (interaction == 0x03) //open door
	{
		LocationVector loc = this->getPosition();

		scoped_ptr<QueryResult> resultDoorExists(sDatabase.Query(format("SELECT * FROM `doors` WHERE `DistrictId`='%1%' And `DoorId`='%2%' Limit 1") % (int)getDistrict() % staticObjId));
		if (resultDoorExists == NULL)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}You are using a door not in the database yet, lets add it{/c}"));	
			format sqlDoorInsert = 
				format("INSERT INTO `doors` SET  `DistrictId` = '%1%', `DoorId` = '%2%', X = '%3%', Y = '%4%', Z = '%5%', ROT = '%6%', FirstUser = '%7%'")
				% (int)m_district
				% staticObjId
				% loc.x	% loc.y	% loc.z	% loc.rot
				% this->getHandle();

			if (sDatabase.Execute(sqlDoorInsert))
			{
				format msg1 = 
					format("{c:00FF00}Door:0x%08x in District %d Set to Location X:%f Y:%f Z:%f O:%f{/c}")
					% staticObjId 
					% (int)m_district 
					% loc.x	% loc.y	% loc.z	% loc.rot;

				m_parent.QueueCommand(make_shared<SystemChatMsg>(msg1.str()));
			}
		}			
		sObjMgr.OpenDoor(staticObjId, &m_parent);		
		//sGame.AnnounceStateUpdate(NULL,make_shared<DeleteDoorMsg>(staticObjId));
		//sGame.AnnounceStateUpdate(NULL,make_shared<DeleteDoorMsg>(staticObjId));
		//uint16 viewId = m_goId;// sObjMgr.getViewForGO(&m_parent,staticObjId);
		//sGame.AnnounceStateUpdate(&m_parent,make_shared<DoorAnimationMsg>(staticObjId, viewId));
		this->GoAhead(1);
		return;
	}
}

void PlayerObject::RPC_HandleJump( ByteBuffer &srcCmd )
{
	LocationVector endPos;
	if (endPos.fromDoubleBuf(srcCmd) == false)
	{
		WARNING_LOG(format("(%1%) %2%:%3% jump packet doesn't have endPos: %4%")
			% m_parent.Address()
			% m_handle
			% m_goId
			% Bin2Hex(srcCmd) );
		return;
	}

	vector<byte> extraData(0x0B);
	srcCmd.read(extraData);

	uint32 theTimeStamp = srcCmd.read<uint32>();

/*	DEBUG_LOG(format("(%1%) %2%:%3% jumping to %4%,%5%,%6% extra data %7% timestamp %8%")
		% m_parent.Address()
		% m_handle
		% m_goId
		% endPos.x % endPos.y % endPos.z
		% Bin2Hex(&extraData[0],extraData.size())
		% theTimeStamp );*/

	this->setPosition(endPos);
	sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId));
}

void PlayerObject::RPC_HandleRegionLoadedNotification( ByteBuffer &srcCmd )
{
	vector<byte> fourBytes(4);
	srcCmd.read(fourBytes);
	LocationVector loc;
	if (!loc.fromFloatBuf(srcCmd))
		throw ByteBuffer::out_of_range();

/*	DEBUG_LOG(format("(%1%) %2%:%3% loaded region X:%4% Y:%5% Z:%6% extra: %7%")
		% m_parent.Address()
		% m_handle
		% m_goId
		% loc.x % loc.y % loc.z
		% Bin2Hex(fourBytes));
*/
}

void PlayerObject::RPC_HandleReadyForWorldChange( ByteBuffer &srcCmd )
{
	uint32 shouldBeZero = srcCmd.read<uint32>();

	if (shouldBeZero != 0)
		DEBUG_LOG(format("ReadyForWorldChange uint32 is %1%")%shouldBeZero);

	m_district = 0x03;
	InitializeWorld();
	SpawnSelf();
}

void PlayerObject::RPC_HandleWho( ByteBuffer &srcCmd )
{
	stringstream playerList;
	vector<uint32> objects = sObjMgr.getAllGOIds();
	playerList << "Players(" << objects.size() << ")";
	playerList << " [ ";
	foreach (uint32 objId, objects)
	{
		PlayerObject* pObj = NULL;
		try
		{
			pObj = sObjMgr.getGOPtr(objId);
		}
		catch (...)
		{
			continue;
		}

		if (pObj && pObj->getDistrict() == this->getDistrict())
		{
			if (pObj->getClient().isBot() == false)
			{
				playerList << pObj->getHandle() << " ";
			}
		}
	}
	playerList << "]";

	m_parent.QueueCommand(make_shared<WhisperMsg>("PlayersInYourDistrict",playerList.str()));
}

void PlayerObject::RPC_HandleWhereAmI( ByteBuffer &srcCmd )
{
	LocationVector clientSidePos;
	if (clientSidePos.fromFloatBuf(srcCmd) == false)
		throw ByteBuffer::out_of_range();

	bool byte1Valid = false;
	bool byte2Valid = false;
	bool byte3Valid = false;
	uint8 byte1,byte2,byte3;

	if (srcCmd.remaining() >= sizeof(byte1))
	{
		byte1Valid=true;
		srcCmd >> byte1;
	}
	if (srcCmd.remaining() >= sizeof(byte2))
	{
		byte2Valid=true;
		srcCmd >> byte2;
	}
	if (srcCmd.remaining() >= sizeof(byte3))
	{
		byte3Valid=true;
		srcCmd >> byte3;
	}

	INFO_LOG(format("(%1%) %2%:%3% requesting whereami clientPos %4%,%5%,%6% %7%:%8% %9%:%10% %11%:%12%")
		% m_parent.Address()
		% m_handle
		% m_goId
		% clientSidePos.x % clientSidePos.y % clientSidePos.z
		% byte1Valid % uint32(byte1)
		% byte2Valid % uint32(byte2)
		% byte3Valid % uint32(byte3) );

	m_parent.QueueCommand(make_shared<WhereAmIResponse>(m_pos));
	//m_parent.QueueCommand(make_shared<HexGenericMsg>("8107"));
}

void PlayerObject::RPC_HandleGetPlayerDetails( ByteBuffer &srcCmd )
{
	uint32 zeroInt = srcCmd.read<uint32>();

	if (zeroInt != 0)
		WARNING_LOG(format("Get player details zero int is %1%") % zeroInt );

	uint16 playerNameStrLenPos = srcCmd.read<uint16>();

	if (playerNameStrLenPos != srcCmd.rpos())
	{
		WARNING_LOG(format("Get player details strlenpos not %1% but %2%") % (int)srcCmd.rpos() % playerNameStrLenPos );
		return;
	}

	srcCmd.rpos(playerNameStrLenPos);
	string thePlayerName = srcCmd.readString();

	vector<uint32> objectList = sObjMgr.getAllGOIds();
	foreach(uint32 objId, objectList)
	{
		PlayerObject* targetPlayer = NULL;
		try
		{
			targetPlayer = sObjMgr.getGOPtr(objId);
		}
		catch (std::exception)
		{
			continue;
		}

		if (targetPlayer == NULL)
			continue;

		if (targetPlayer->getHandle() == thePlayerName)
		{
			m_parent.QueueCommand(make_shared<PlayerDetailsMsg>(targetPlayer));
			m_parent.QueueCommand(make_shared<PlayerBackgroundMsg>(targetPlayer->getBackground()));
			break;
		}
	}
}

void PlayerObject::RPC_HandleGetBackground( ByteBuffer &srcCmd )
{
	m_parent.QueueCommand(make_shared<BackgroundResponseMsg>(this->getBackground()));
}

void PlayerObject::RPC_HandleSetBackground( ByteBuffer &srcCmd )
{
	uint16 backgroundStrLenPos = srcCmd.read<uint16>();

	srcCmd.rpos(backgroundStrLenPos);
	string theNewBackground = srcCmd.readString();

	bool success = this->setBackground(theNewBackground);
	if (success)
	{
		INFO_LOG(format("(%1%) %2%:%3% changed background to |%4%|")
			% m_parent.Address()
			% m_handle
			% m_goId
			% this->getBackground() );
	}
	else
	{
		WARNING_LOG(format("(%1%) %2%:%3% background sql query update failed")
			% m_parent.Address()
			% m_handle
			% m_goId );
	}
}

void PlayerObject::RPC_HandleHardlineTeleport( ByteBuffer &srcCmd )
{
	uint8 hardlineYouAreUsing;
	uint8 districtYouAreIn;
	uint8 hardlineLocation;
	uint8 hardlineDistrict;

	//Get hardlineYouAreUsing
	srcCmd >> hardlineYouAreUsing;

	srcCmd.rpos(6);
	//Get District you are currently in
	srcCmd >> districtYouAreIn;

	srcCmd.rpos(10);
	//Get Hardline location
	srcCmd >> hardlineLocation;

	srcCmd.rpos(14);
	//Get Hardline District
	srcCmd >> hardlineDistrict;

	format debugMsg = 
		format("You want to go to Hardline:%1% District:%2% From District:%3% Hardline:%4%")
		% (int)hardlineLocation 
		% (int)hardlineDistrict 
		% (int)districtYouAreIn 
		% (int)hardlineYouAreUsing;

	m_parent.QueueCommand(make_shared<SystemChatMsg>(debugMsg.str()));

	double newX = 0.0, newY = 0.0, newZ = 0.0, newRot = 0.0;
	string newlocationName = "";
	int factionTag = 0;
	bool found = false;

	if (Database_Main != nullptr)
	{
		format sql = 
			format("SELECT `X`,`Y`,`Z`, `ROT`, `HardlineName`, `FactionTag` FROM `hardlines` Where `DistrictId` = '%1%' And `HardlineId` = '%2%' LIMIT 1")
			% (int)hardlineDistrict 
			% (int)hardlineLocation;

		try
		{
			scoped_ptr<QueryResult> result(sDatabase.Query(sql));
			if (result != NULL)
			{
				Field *field = result->Fetch();
				newX = field[0].GetDouble();
				newY = field[1].GetDouble();
				newZ = field[2].GetDouble();
				newRot = field[3].GetDouble();
				newlocationName = field[4].GetString();
				factionTag = field[5].GetInt32();
				found = true;
			}
		}
		catch (...) {}
	}

	// Fallback to static hardline directory (e.g. headless tests or offline DB)
	if (!found)
	{
		const HardlineNode* node = GetHardline(hardlineDistrict, hardlineLocation);
		if (node)
		{
			newX = node->x;
			newY = node->y;
			newZ = node->z;
			newRot = node->rot;
			newlocationName = node->name;
			factionTag = node->factionTag;
			found = true;
		}
	}

	if (!found)
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>(
			(format("{c:FF0000}[HARDLINE] Selected Hardline #%1% in District %2% not found in directory.{/c}")
				% (int)hardlineLocation % (int)hardlineDistrict).str()));
		return;
	}

	// Faction Warfare - Hardline Access restriction
	if (factionTag != 0 && factionTag != getFaction())
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Access Denied. This Hardline is controlled by a hostile Faction.{/c}"));
		return;
	}

	format message1 = format("{c:00FFFF}[HARDLINE] Transferred to %1% (District %2%, Node #%3%).{/c}") 
		% newlocationName % (int)hardlineDistrict % (int)hardlineLocation;
	m_parent.QueueCommand(make_shared<SystemChatMsg>(message1.str()));

	LocationVector newLoc(newX, newY, newZ);
	newLoc.rot = newRot;
	this->setPosition(newLoc);
	this->setDistrict(hardlineDistrict);
	sGame.AnnounceStateUpdate(NULL, make_shared<PositionStateMsg>(m_goId));
	
	// Link Hardlines properly to spatial network
	sSpatialGrid.UpdateClientPosition(&m_parent, newLoc.x, newLoc.z);

	// Authentic retail hardline ring & transmission audio FX via JackoutEffectMsg materialization
	m_parent.QueueState(make_shared<JackoutEffectMsg>(m_goId, false));
	m_parent.QueueCommand(make_shared<SystemChatMsg>(
		"{c:00FFCC}[HARDLINE] Secure Vault Link Established. Access your locker with /locker list, /locker store <slot>, /locker withdraw <id>.{/c}"
	));
}

void PlayerObject::RPC_HandleObjectSelected( ByteBuffer &srcCmd )
{
	uint16 viewId = srcCmd.read<uint16>();
	uint16 objType = srcCmd.read<uint16>();
	if (viewId || objType)
	{
		uint32 targetGoId = sObjMgr.getGOForView(&m_parent, viewId);
		setTargetGoId(targetGoId);

		format msg = 
			format("(%s) %s:%d selected dynamic object view id %04x objType %04x (targetGoId=%d)")
			% m_parent.Address() % m_handle	% m_goId
			% viewId % objType % targetGoId;

		INFO_LOG(msg);
		if (targetGoId == 0)
			INFO_LOG(format("%1%:%2% selected view %3% which maps to no known object (targetGoId=0)") % m_handle % m_goId % viewId);
	}
	else
	{
		setTargetGoId(0);
	}
}

void PlayerObject::RPC_HandleJackoutRequest( ByteBuffer &srcCmd )
{
	DEBUG_LOG(format("(%s) %s:%d requested Jackout escape sequence") % m_parent.Address() % m_handle % m_goId);
	
	// Cancel existing Jackout if already ticking
	cancelEvents(EVENT_JACKOUT);

	m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[JACKOUT] Jackout sequence confirmed. Escaping the Matrix in 10 seconds...{/c}"));

	// Authentic retail telephone booth / mirror dissolve effect
	m_parent.QueueState(make_shared<JackoutEffectMsg>(m_goId));
	// Authentic retail carrier packet
	m_parent.QueueCommand(make_shared<HexGenericMsg>("2E0700000000000000000000002300002E00000000000000000000000000000000000000"));
	this->addEvent(EVENT_JACKOUT, boost::bind(&PlayerObject::jackoutEvent, this), 10.0f); // Schedule jackout in 10 seconds
}

void PlayerObject::jackoutEvent()
{
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80fd000000000000"));
	m_parent.FlushQueue();
	saveDataToDB();
	INFO_LOG(format("Player %1% successfully jacked out of the Matrix.") % m_handle);
	m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[JACKOUT] Carrier signal terminated. Jackout complete.{/c}"));
}

void PlayerObject::RPC_HandleJackoutFinished( ByteBuffer &srcCmd )
{
	//this doesn't get sent by client, even though it does on real server

	ByteBuffer extraData = ByteBuffer(&srcCmd.contents()[srcCmd.rpos()],srcCmd.remaining());
	format msg = 
		format("(%s) %s:%d jackout complete extra data %s")
		% m_parent.Address()
		% m_handle
		% m_goId
		% Bin2Hex(extraData,0);

	DEBUG_LOG(msg);
	m_parent.Invalidate();
}
void PlayerObject::RPC_HandleMarketListItems(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMarketListItems") % m_parent.Address() % m_handle % m_goId);
	auto listings = sEconomySys.GetActiveListings();
	if (listings.empty())
	{
		// Authentic retail seed items so the marketplace is always populated:
		// 903: Foot Wear (8,000 $Info) - authentic retail HDS capture
		// 10101: Dual Berettas (15,000 $Info)
		// 10102: Onyx Trenchcoat (25,000 $Info)
		// 10103: Dark Shades (5,000 $Info)
		// 10105: Polished Berettas (18,000 $Info)
		static const std::vector<std::pair<uint32, uint32>> seedCatalog = {
			{ 903, 8000 },
			{ 10101, 15000 },
			{ 10102, 25000 },
			{ 10103, 5000 },
			{ 10105, 18000 }
		};
		for (const auto& seed : seedCatalog)
		{
			VendorListing l;
			l.listingId = static_cast<uint64>(seed.first);
			l.sellerId = 0;
			l.templateId = seed.first;
			l.infoPrice = seed.second;
			l.isActive = true;
			listings.push_back(l);
		}
	}

	std::vector<MarketItemEntry> entries;
	entries.reserve(listings.size());
	uint32 now = static_cast<uint32>(time(nullptr));
	for (const auto& l : listings)
	{
		MarketItemEntry entry;
		entry.templateId = l.templateId;
		entry.instanceData = 0;
		entry.marketplaceId = static_cast<uint32>(l.listingId);
		entry.sellingPrice = l.infoPrice;
		entry.organizationId = 3;
		entry.timePostedGMT = now;
		entry.playerIsSelling = (m_goId != 0 && l.sellerId == m_goId) ? 1 : 0;
		entries.push_back(entry);
	}

	m_parent.QueueCommand(std::make_shared<MarketplaceListReplyMsg>(entries));
	m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
		(format("{c:00FFCC}[MARKETPLACE] %1% active item listing(s) retrieved from MegaCity Exchange.{/c}")
			% entries.size()).str()
	));
	INFO_LOG(format("Marketplace: Dispatched %1% active listings (opcode 0x8125) to %2% (GoID %3%)")
		% entries.size() % m_handle % m_goId);
}

void PlayerObject::RPC_HandleMarketOpen(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMarketOpen") % m_parent.Address() % m_handle % m_goId);
	auto vendors = sEconomySys.GetVendorsForDistrict(m_district);
	if (vendors.empty()) vendors = sEconomySys.GetVendorsForDistrict(1);
	if (!vendors.empty())
	{
		const HardlineVendor* nearest = &vendors[0];
		double bestDist = 1e9;
		for (const auto& v : vendors)
		{
			double d = (m_pos.x - v.x)*(m_pos.x - v.x) + (m_pos.z - v.z)*(m_pos.z - v.z);
			if (d < bestDist)
			{
				bestDist = d;
				nearest = &v;
			}
		}
		m_parent.QueueCommand(std::make_shared<VendorOpenMsg>(
			500.0f,
			nearest->x,
			nearest->y,
			nearest->z,
			nearest->inventoryTemplates
		));
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
			(format("{c:00FFCC}[VENDOR] Connected to %1% (Vendor ID: %2%, Items: %3%){/c}")
				% nearest->name % nearest->staticId % nearest->inventoryTemplates.size()).str()
		));
	}
}
void PlayerObject::RPC_HandleVendorBuy(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() < 4) return;
	uint32 itemTemplateId = srcCmd.read<uint32>();
	uint32 vendorGoId = 0;
	if (srcCmd.remaining() >= 4)
		vendorGoId = srcCmd.read<uint32>();

	const ItemTemplate* tpl = sDataLoader.GetItemTemplate(itemTemplateId);
	uint32 cost = tpl && tpl->value > 0 ? tpl->value : sEconomySys.GetItemPrice(itemTemplateId);
	if (cost == 0) cost = 100;
	std::string itemName = tpl ? tpl->name : (format("Catalog Item %1%") % itemTemplateId).str();

	if (m_cash < cost)
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
			(format("{c:FF4444}[VENDOR] Insufficient Information Bits. Cost: %1% bits, Available: %2% bits.{/c}") % cost % m_cash).str()
		));
		return;
	}

	if (!m_inventorySystem) return;

	if (m_inventorySystem->getFirstFreeSlot() == 0 || m_inventorySystem->getFirstFreeSlot() == 0xFF)
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] Inventory is full. Clear space before purchasing.{/c}"));
		return;
	}

	if (!giveItem(itemTemplateId))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] Failed to add item to inventory.{/c}"));
		return;
	}

	removeInfo(cost);
	saveCashToDB();
	m_parent.QueueCommand(std::make_shared<SetInformationCmd>(m_cash));

	m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
		(format("{c:00FF00}[VENDOR] Purchased %1% for %2% Information Bits.{/c}") % itemName % cost).str()
	));
}
void PlayerObject::RPC_HandleVendorSell(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() < 4) return;
	uint32 itemId = srcCmd.read<uint32>();
	uint32 vendorGoId = 0;
	if (srcCmd.remaining() >= 4)
		vendorGoId = srcCmd.read<uint32>();

	if (!m_inventorySystem) return;

	// Locate item by GoId first, then by TemplateId
	auto item = m_inventorySystem->getItemByGoId(itemId);
	if (!item) item = m_inventorySystem->getItemByTemplate(itemId);

	if (!item)
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] Item not found in your inventory.{/c}"));
		return;
	}

	// Verify not selling an equipped weapon
	if (m_equippedWeaponId != 0 && (item->getGoId() == m_equippedWeaponId || item->getTemplateId() == m_equippedWeaponId))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] Cannot sell an equipped weapon! Unequip first.{/c}"));
		return;
	}

	const ItemTemplate* tpl = sDataLoader.GetItemTemplate(item->getTemplateId());
	uint32 baseValue = tpl && tpl->value > 0 ? tpl->value : sEconomySys.GetItemPrice(item->getTemplateId());
	if (baseValue == 0) baseValue = 100;
	uint32 sellValue = std::max<uint32>(1, baseValue / 2);

	// Remove from inventory
	if (!m_inventorySystem->consumeItemByTemplate(item->getTemplateId()))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] Failed to remove item from inventory.{/c}"));
		return;
	}

	if (!getClient().isBot())
	{
		m_inventorySystem->saveToDB();
	}

	addInfo(sellValue);
	saveCashToDB();
	m_parent.QueueCommand(std::make_shared<SetInformationCmd>(m_cash));

	std::string itemName = tpl ? tpl->name : "Item";
	m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
		(format("{c:00FF00}[VENDOR] Sold %1% for %2% Information Bits.{/c}") % itemName % sellValue).str()
	));
}
void PlayerObject::RPC_HandleCraftRequest(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() < 4) return;
	uint32 blueprintId = srcCmd.read<uint32>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleCraftRequest: blueprintId=%4%")
		% m_parent.Address() % m_handle % m_goId % blueprintId);
	sCraftSys.HandleCraftRequest(this, blueprintId);
}

void PlayerObject::RPC_HandleFactionInfo(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleFactionInfo %4%") % m_parent.Address() % m_handle % m_goId % Bin2Hex(srcCmd));
}

void PlayerObject::RPC_HandleMissionInvite(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMissionInvite") % m_parent.Address() % m_handle % m_goId);
}

void PlayerObject::RPC_HandlePartyLeave(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandlePartyLeave") % m_parent.Address() % m_handle % m_goId);
}

void PlayerObject::RPC_HandleMemoryChangeTactic(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMemoryChangeTactic") % m_parent.Address() % m_handle % m_goId);
}
void PlayerObject::RPC_HandleUpgradeAbility(ByteBuffer& srcCmd)
{
	//layout per HDS AbilityHandler.ProcessUpgradeAbility: ability u16, unknown u16, level u8
	if (srcCmd.remaining() < 5)
		return;
	uint16 abilityId = srcCmd.read<uint16>();
	uint16 unknown = srcCmd.read<uint16>();
	uint8 requestedLevel = srcCmd.read<uint8>();

	INFO_LOG(format("(%1%) %2%:%3% upgrade ability %4% unk %5% -> level %6%")
		% m_parent.Address() % m_handle % m_goId % abilityId % unknown % uint32(requestedLevel));

	if (abilityId == 0 || !m_abilitySystem)
		return;

	//only abilities the character already owns can be upgraded, and never past the
	//character's own level (the old handler loaded any ability at any level for free)
	auto ab = m_abilitySystem->getAbility(abilityId);
	if (!ab)
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}You don't have that ability.{/c}"));
		return;
	}
	uint16 newLevel = requestedLevel;
	if (newLevel > getLevel())
		newLevel = getLevel();
	if (newLevel == 0)
		newLevel = 1;

	m_abilitySystem->loadAbility(abilityId, newLevel, ab->getMemorySlot());
	m_abilitySystem->saveToDB();
	m_parent.QueueCommand(std::make_shared<AbilityUpgradeRspMsg>(abilityId, newLevel));
}
void PlayerObject::RPC_HandleMissionAbort(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMissionAbort") % m_parent.Address() % m_handle % m_goId);
	if (sMissionSys.AbortMission(this, "player abort (0x80a6)"))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[OPERATOR] Contract aborted. Uplink severed.{/c}"));
	}
	else
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[OPERATOR] No active contract to abort.{/c}"));
	}
}
void PlayerObject::RPC_HandleMissionAccept(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMissionAccept") % m_parent.Address() % m_handle % m_goId);
	if (sMissionSys.HasActiveMission(m_goId))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:00FF00}[OPERATOR] Contract confirmed. Uplink established. Execute mission parameters.{/c}"));
		sMissionSys.SendMissionObjectiveDialog(this);
	}
	else
	{
		RPC_HandleMissionRequest(srcCmd);
	}
}
void PlayerObject::RPC_HandleMissionInfo(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMissionInfo") % m_parent.Address() % m_handle % m_goId);
	if (sMissionSys.HasActiveMission(m_goId))
	{
		sMissionSys.SendMissionObjectiveDialog(this);
	}
	else
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[OPERATOR] No active contract data on file. Contact operator for assignment.{/c}"));
	}
}
void PlayerObject::RPC_HandleMissionRequest(ByteBuffer& srcCmd)
{
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleMissionRequest") % m_parent.Address() % m_handle % m_goId);

	if (sMissionSys.HasActiveMission(m_goId))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FFFF00}[OPERATOR] Active contract in progress. Complete or abort current parameters before requesting new orders.{/c}"));
		sMissionSys.SendMissionObjectiveDialog(this);
		return;
	}

	uint32 missionId = sMissionSys.GetAvailableStoryMission(this);
	if (missionId == 0)
	{
		missionId = sMissionSys.GenerateFactionTensionMission(this);
	}

	if (missionId != 0)
	{
		sMissionSys.AssignMission(this, missionId);
		const auto& templates = sMissionSys.GetMissionTemplates();
		auto it = templates.find(missionId);
		if (it != templates.end())
		{
			const MissionTemplate& templ = it->second;
			m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
				(format("{c:00FF00}[OPERATOR DISPATCH] New Contract Assigned: %1%{/c}") % templ.title).str()
			));
			m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
				(format("{c:00FFCC}[BRIEFING] %1%{/c}") % templ.description).str()
			));
			m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
				(format("{c:FFD700}[REWARDS] %1% Info Bits | %2% XP{/c}") % templ.infoReward % templ.expReward).str()
			));

			if (!templ.objectives.empty())
			{
				const auto& firstObj = templ.objectives[0];
				m_parent.QueueCommand(std::make_shared<SystemChatMsg>(
					(format("{c:00FF00}[RADAR BREADCRUMB] Objective Target: %1%{/c}") % firstObj.description).str()
				));
			}
		}
	}
	else
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[OPERATOR] No operational contacts currently requesting tactical assets in this sector.{/c}"));
	}
}
void PlayerObject::RPC_HandleItemMoveSlot(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() < 2) return;
	uint8 fromSlot = srcCmd.read<uint8>();
	uint8 toSlot = srcCmd.read<uint8>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleItemMoveSlot: fromSlot=%4% toSlot=%5%")
		% m_parent.Address() % m_handle % m_goId % (int)fromSlot % (int)toSlot);
	if (m_inventorySystem)
	{
		if (m_inventorySystem->moveItem(fromSlot, toSlot))
		{
			if (!getClient().isBot())
			{
				m_inventorySystem->saveToDB();
			}
		}
	}
}

void PlayerObject::RPC_HandleItemUnmountRSI(ByteBuffer& srcCmd)
{
	uint8 slot = 0;
	if (srcCmd.remaining() >= 1)
		slot = srcCmd.read<uint8>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleItemUnmountRSI: slot=%4%")
		% m_parent.Address() % m_handle % m_goId % (int)slot);
	UpdateAppearance();
	if (m_inventorySystem && !getClient().isBot())
	{
		m_inventorySystem->saveToDB();
	}
}

void PlayerObject::RPC_HandleItemMountRSI(ByteBuffer& srcCmd)
{
	uint32 itemGoId = 0;
	uint8 slot = 0;
	if (srcCmd.remaining() >= 4)
		itemGoId = srcCmd.read<uint32>();
	if (srcCmd.remaining() >= 1)
		slot = srcCmd.read<uint8>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleItemMountRSI: itemGoId=%4% slot=%5%")
		% m_parent.Address() % m_handle % m_goId % itemGoId % (int)slot);
	UpdateAppearance();
	if (m_inventorySystem && !getClient().isBot())
	{
		m_inventorySystem->saveToDB();
	}
}

void PlayerObject::RPC_HandleCallContact( ByteBuffer &srcCmd )
{
	uint32 contactId = 0;
	if (srcCmd.remaining() >= 4)
		contactId = srcCmd.read<uint32>();
	else if (srcCmd.remaining() >= 2)
		contactId = srcCmd.read<uint16>();
	else if (srcCmd.remaining() >= 1)
		contactId = srcCmd.read<uint8>();

	DEBUG_LOG(format("(%1%) RPC_HandleCallContact: contactId=%2%") % m_parent.Address() % contactId);

	uint32 faction = getFaction();
	const SponsorContact* sponsor = sMissionSys.GetSponsor(contactId);

	// If no specific sponsor requested or not found, pick authentic faction sponsor
	if (!sponsor)
	{
		uint32 defaultSponsorId = (faction == 1) ? 2000 : // Tyndall (Zion)
		                          (faction == 2) ? 2001 : // Agent Gray (Machines)
		                          (faction == 3) ? 2002 : // Flood (Merovingian)
		                          2007;                   // Operator (General)
		sponsor = sMissionSys.GetSponsor(defaultSponsorId);
	}

	std::string contactName = sponsor ? sponsor->name : "Operator";

	// Check if player has an active mission or needs one
	if (sMissionSys.HasActiveMission(m_goId))
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>(
			(format("{c:00FFCC}[%1%] Uplink verified. Proceed with your current mission objectives.{/c}") % contactName).str()
		));
		sMissionSys.SendMissionObjectiveDialog(this);
	}
	else
	{
		uint32 missionId = sMissionSys.GetAvailableStoryMission(this, sponsor ? sponsor->id : 0);
		if (missionId != 0)
		{
			sMissionSys.AssignMission(this, missionId);
			const auto& templates = sMissionSys.GetMissionTemplates();
			auto it = templates.find(missionId);
			if (it != templates.end())
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}[%1%] New Mission Available: %2%{/c}") % contactName % it->second.title).str()
				));
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FFFF}[BRIEFING] %1%{/c}") % it->second.description).str()
				));
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:FFD700}[REWARDS] %1% Info Bits | %2% XP{/c}") % it->second.infoReward % it->second.expReward).str()
				));
			}
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[%1%] Transmission received, %2%. No outstanding contracts in this sector.{/c}") % contactName % m_handle).str()
			));
		}
	}
}

void PlayerObject::RPC_HandleAbilityHotbarSync(ByteBuffer& srcCmd)
{
	//0x80BE meaning is a guess - log the payload, don't answer with guessed packets
	DEBUG_LOG(format("(%1%) %2%:%3% RPC 0x80BE %4%") % m_parent.Address() % m_handle % m_goId % Bin2Hex(srcCmd));
}

void PlayerObject::RPC_HandleStatusQuery(ByteBuffer& srcCmd)
{
	//0x8148 meaning is a guess - log the payload, don't answer with guessed packets
	DEBUG_LOG(format("(%1%) %2%:%3% RPC 0x8148 %4%") % m_parent.Address() % m_handle % m_goId % Bin2Hex(srcCmd));
}

void PlayerObject::RPC_HandleInteractionTrigger(ByteBuffer& srcCmd)
{
	uint32 triggerId = 0;
	if (srcCmd.remaining() >= 4)
		triggerId = srcCmd.read<uint32>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleInteractionTrigger: 0x%4$08X") % m_parent.Address() % m_handle % m_goId % triggerId);
}

void PlayerObject::RPC_HandleClientAck(ByteBuffer& srcCmd)
{
	// Client UI frame notification / state ACK
}

void PlayerObject::RPC_HandleCameraPitch(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() >= sizeof(float))
	{
		m_cameraPitch = srcCmd.read<float>();
	}
}

void PlayerObject::RPC_HandleCameraYaw(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() >= sizeof(float))
	{
		m_cameraYaw = srcCmd.read<float>();
	}
}


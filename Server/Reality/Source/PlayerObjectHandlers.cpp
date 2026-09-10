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
#include "PlayerObject.h"
#include "Log.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"
#include "Timer.h"
#include "ObjectMgr.h"
#include "SpatialGrid.h"
#include <iomanip>
#include "GameServer.h"
#include "GameClient.h"
#include "Config.h"
#include "BotManager.h"
#include "DataLoader.h"
#include "InventorySystem.h"
#include "Item.h"
#include "WorldDirector.h"
#include "LogisticsManager.h"
#include "AI/MatrixThreatHeatmap.h"
#include "HovercraftFlightSystem.h"
#include "FrankCastleManager.h"
#include "LoadingConstruct.h"
#include "BackdoorNetwork.h"
#include "ArchitectDialogueTree.h"
#include "OracleDialogueTree.h"
#include "OracleSanctuarySystem.h"
#include "OracleCookieSystem.h"
#include "OracleVisionSimulacra.h"
#include "StatusEffectManager.h"
#include "NeuralVoiceSystem.h"
#include "RadioDispatchSystem.h"
#include "OpenXRPipeline.h"
#include "APUCombatSystem.h"
#include "MegacityDestructionEngine.h"
#include "RedpillAwakeningSystem.h"
#include "WebGPUTerminalBridge.h"
#include "NeuralNarrativeEngine.h"
#include "MachineCitySystem.h"
#include "FreewayCombatSystem.h"
#include "MobilAveRailSystem.h"
#include "CyberdeckHackingSystem.h"
#include "ShardFederationEngine.h"
#include "ClubHelRaidSystem.h"
#include "PodHarvestSystem.h"
#include "OrbitalSatelliteSystem.h"
#include "SourceCodeCompilerSystem.h"
#include "MatrixRebootEngine.h"
#include "BiographicalNarrativeEngine.h"
#include "UnderworldManager.h"
#include "CityLifeManager.h"
#include "EmergentAIEngine.h"
#include "EmergentPoliceManager.h"
#include "NPCSocialLifeEngine.h"
#include "NPCFamilyDreamsEngine.h"
#include "NPCEmergentLifeEngine.h"
#include "SLMDialogueContextEngine.h"
#include "MafiaEcosystemManager.h"
#include "ExileChateauManager.h"

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
			auto item = std::make_shared<Item>(rand(), templateId);
			getInventory()->addItemAuto(item);
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Added item %1% to inventory.{/c}") % templateId).str()));
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
    else if (iequals(command, "inspect") || iequals(command, "bio"))
    {
        string targetArg;
        cmdStream >> targetArg;
        if (iequals(targetArg, "stats") || iequals(targetArg, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sBioEngine.GenerateEngineTelemetryReport()));
            return;
        }
        else if (iequals(targetArg, "generate")) {
            string factionArg;
            cmdStream >> factionArg;
            BioFaction faction = BioFaction::ZionRedpill;
            if (iequals(factionArg, "machine") || iequals(factionArg, "agent")) faction = BioFaction::MachineAgent;
            else if (iequals(factionArg, "exile") || iequals(factionArg, "merovingian")) faction = BioFaction::MerovingianExile;
            else if (iequals(factionArg, "cypherite")) faction = BioFaction::CypheriteTurncoat;
            else if (iequals(factionArg, "civilian") || iequals(factionArg, "bluepill")) faction = BioFaction::BluepillCivilian;
            else if (iequals(factionArg, "police") || iequals(factionArg, "swat")) faction = BioFaction::MMPDPoliceSWAT;
            else if (iequals(factionArg, "syndicate") || iequals(factionArg, "mob")) faction = BioFaction::SyndicateEnforcer;
            
            uint64_t seed = ((uint64_t)rand() << 32) | (uint64_t)rand();
            BiographicalProfile prof = sBioEngine.GenerateProfile(seed, faction);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(prof.ToDFCharacterSheet()));
            return;
        }

        PlayerObject* target = nullptr;
        if (!targetArg.empty()) {
            uint32 targetGoId = 0;
            try {
                targetGoId = (uint32)std::stoul(targetArg);
            } catch (...) {
                targetGoId = 0;
            }

            for (uint32 objId : sObjMgr.getAllGOIds()) {
                PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                if (po) {
                    if (targetGoId != 0 && po->getGoId() == targetGoId) {
                        target = po;
                        break;
                    } else if (iequals(po->getHandle(), targetArg)) {
                        target = po;
                        break;
                    }
                }
            }
        } else {
            target = this;
        }

        if (target) {
            BiographicalProfile prof = sBioEngine.GenerateProfileForBot(target->getGoId(), target->getCharacterUID(), (mxoFaction)target->getFaction());
            m_parent.QueueCommand(make_shared<SystemChatMsg>(prof.ToDFCharacterSheet()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target not found for inspection.{/c}"));
        }
        return;
    }
    else if (iequals(command, "social") || iequals(command, "love") || iequals(command, "friendships"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (subCmd.empty() || iequals(subCmd, "stats") || iequals(subCmd, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateSocialTelemetryReport()));
            return;
        }

        if (iequals(subCmd, "date")) {
            uint32 idA = 0, idB = 0;
            cmdStream >> idA >> idB;
            if (idA != 0 && idB != 0) {
                bool res = sSocialEngine.ScheduleAndExecuteDate(idA, idB, "Le Bistro de Merovingian", "Scheduled Admin Romantic Date");
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Admin: Scheduled date between %1% and %2%: %3%{/c}") % idA % idB % (res ? "Success" : "Failed")).str()));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !social date <idA> <idB>{/c}"));
            }
            return;
        } else if (iequals(subCmd, "breakup")) {
            uint32 idA = 0, idB = 0;
            cmdStream >> idA >> idB;
            if (idA != 0 && idB != 0) {
                bool res = sSocialEngine.TriggerArgumentOrBreakup(idA, idB, "Irreconcilable ideological differences regarding the Matrix");
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Admin: Triggered breakup between %1% and %2%: %3%{/c}") % idA % idB % (res ? "Success" : "Failed")).str()));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: !social breakup <idA> <idB>{/c}"));
            }
            return;
        }

        uint32 targetGoId = 0;
        try {
            targetGoId = (uint32)std::stoul(subCmd);
        } catch (...) {
            targetGoId = 0;
        }

        if (targetGoId == 0) {
            for (uint32 objId : sObjMgr.getAllGOIds()) {
                PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                if (po && iequals(po->getHandle(), subCmd)) {
                    targetGoId = po->getGoId();
                    break;
                }
            }
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateEntitySocialSummary(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateEntitySocialSummary(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "memories") || iequals(command, "memory"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateEntityMemoriesReport(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target not found for memories inspection.{/c}"));
        }
        return;
    }
    else if (iequals(command, "socialstats"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateSocialTelemetryReport()));
        return;
    }
    else if (iequals(command, "family") || iequals(command, "household"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "stats") || iequals(subCmd, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateEngineMasterTelemetryReport()));
            return;
        }

        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateFamilySummary(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateFamilySummary(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "dreams") || iequals(command, "dream") || iequals(command, "aspiration"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateDreamsSummary(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateDreamsSummary(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "career") || iequals(command, "job") || iequals(command, "vocation"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateCareerSummary(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateCareerSummary(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "life") || iequals(command, "lifedossier") || iequals(command, "dossier"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "stats") || iequals(subCmd, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateEngineMasterTelemetryReport()));
            return;
        }

        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateCompleteLifeDossier(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateCompleteLifeDossier(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "lifestats") || iequals(command, "familystats"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateEngineMasterTelemetryReport()));
        return;
    }
    else if (iequals(command, "emergentlife") || iequals(command, "emergentlifestyle"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "stats") || iequals(subCmd, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateMasterTelemetryReport()));
            return;
        }

        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateEmergentLifeSummary(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateEmergentLifeSummary(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "awakening") || iequals(command, "epiphany"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateAwakeningReport(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateAwakeningReport(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "gossip") || iequals(command, "rumors"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateGossipNetworkReport()));
        return;
    }
    else if (iequals(command, "socialcircles") || iequals(command, "circles"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateSocialCirclesReport()));
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
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentAIMgr.GenerateEmergentTelemetryReport()));
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

	using boost::erase_all;
	if (iequals(command, "dojo"))
	{
		string subCommand;
		cmdStream >> subCommand;
		LocationVector pos = this->getPosition();
		if (iequals(subCommand, "1v1"))
		{
			sBotMgr.SpawnBot(1, pos.x + 20.0f, pos.y, pos.z + 20.0f, 2); // 2 = FACTION_MACHINES
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Dojo: 1v1 Enemy spawned!{/c}"));
		}
		else if (iequals(subCommand, "group"))
		{
			sBotMgr.SpawnBot(4, pos.x + 20.0f, pos.y, pos.z + 20.0f, 2); // 2 = FACTION_MACHINES
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Dojo: Group of enemies spawned!{/c}"));
		}
		else if (iequals(subCommand, "clear"))
		{
			int killed = 0;
			auto goIds = sObjMgr.getAllGOIds();
			for (uint32 id : goIds) {
				PlayerObject* p = sObjMgr.getGOPtr(id);
				if (p && p->getClient().isBot()) {
					if (p->getPosition().DistanceSq(pos) < 5000.0f * 5000.0f) {
						p->setCurrentHealth(0);
						p->sendHealthUpdate();
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
		uint64 gearUid;
		string type;
		int value;
		cmdStream >> gearUid >> type >> value;

		if (cmdStream.fail()) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &socket <gearUid> <type> <value>{/c}"));
			return;
		}

		CodeFragment frag = { type, value };
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
    else if (iequals(command, "inspect") || iequals(command, "bio"))
    {
        string targetArg;
        cmdStream >> targetArg;
        if (iequals(targetArg, "stats") || iequals(targetArg, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sBioEngine.GenerateEngineTelemetryReport()));
            return;
        }
        else if (iequals(targetArg, "generate")) {
            string factionArg;
            cmdStream >> factionArg;
            BioFaction faction = BioFaction::ZionRedpill;
            if (iequals(factionArg, "machine") || iequals(factionArg, "agent")) faction = BioFaction::MachineAgent;
            else if (iequals(factionArg, "exile") || iequals(factionArg, "merovingian")) faction = BioFaction::MerovingianExile;
            else if (iequals(factionArg, "cypherite")) faction = BioFaction::CypheriteTurncoat;
            else if (iequals(factionArg, "civilian") || iequals(factionArg, "bluepill")) faction = BioFaction::BluepillCivilian;
            else if (iequals(factionArg, "police") || iequals(factionArg, "swat")) faction = BioFaction::MMPDPoliceSWAT;
            else if (iequals(factionArg, "syndicate") || iequals(factionArg, "mob")) faction = BioFaction::SyndicateEnforcer;
            
            uint64_t seed = ((uint64_t)rand() << 32) | (uint64_t)rand();
            BiographicalProfile prof = sBioEngine.GenerateProfile(seed, faction);
            m_parent.QueueCommand(make_shared<SystemChatMsg>(prof.ToDFCharacterSheet()));
            return;
        }

        PlayerObject* target = nullptr;
        if (!targetArg.empty()) {
            uint32 targetGoId = 0;
            try {
                targetGoId = (uint32)std::stoul(targetArg);
            } catch (...) {
                targetGoId = 0;
            }

            for (uint32 objId : sObjMgr.getAllGOIds()) {
                PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                if (po) {
                    if (targetGoId != 0 && po->getGoId() == targetGoId) {
                        target = po;
                        break;
                    } else if (iequals(po->getHandle(), targetArg)) {
                        target = po;
                        break;
                    }
                }
            }
        } else {
            target = this;
        }

        if (target) {
            BiographicalProfile prof = sBioEngine.GenerateProfileForBot(target->getGoId(), target->getCharacterUID(), (mxoFaction)target->getFaction());
            m_parent.QueueCommand(make_shared<SystemChatMsg>(prof.ToDFCharacterSheet()));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target not found for inspection.{/c}"));
        }
        return;
    }
    else if (iequals(command, "social") || iequals(command, "love") || iequals(command, "friendships"))
    {
        string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "stats") || iequals(subCmd, "telemetry")) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateSocialTelemetryReport()));
            return;
        }

        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateEntitySocialSummary(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateEntitySocialSummary(this->getGoId())));
        }
        return;
    }
    else if (iequals(command, "memories") || iequals(command, "memory"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = 0;
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = 0;
            }
            if (targetGoId == 0) {
                for (uint32 objId : sObjMgr.getAllGOIds()) {
                    PlayerObject* po = sObjMgr.getGOPtrSafe(objId);
                    if (po && iequals(po->getHandle(), subCmd)) {
                        targetGoId = po->getGoId();
                        break;
                    }
                }
            }
        } else {
            targetGoId = this->getGoId();
        }

        if (targetGoId != 0) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateEntityMemoriesReport(targetGoId)));
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Target not found for memories inspection.{/c}"));
        }
        return;
    }
    else if (iequals(command, "socialstats"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sSocialEngine.GenerateSocialTelemetryReport()));
        return;
    }
    else if (iequals(command, "family") || iequals(command, "household"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = this->getGoId();
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = this->getGoId();
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateFamilySummary(targetGoId)));
        return;
    }
    else if (iequals(command, "dreams") || iequals(command, "dream") || iequals(command, "aspiration"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = this->getGoId();
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = this->getGoId();
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateDreamsSummary(targetGoId)));
        return;
    }
    else if (iequals(command, "career") || iequals(command, "job") || iequals(command, "vocation"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = this->getGoId();
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = this->getGoId();
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateCareerSummary(targetGoId)));
        return;
    }
    else if (iequals(command, "life") || iequals(command, "lifedossier") || iequals(command, "dossier"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = this->getGoId();
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = this->getGoId();
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateCompleteLifeDossier(targetGoId)));
        return;
    }
    else if (iequals(command, "lifestats") || iequals(command, "familystats"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sNPCFamilyDreamsEngine.GenerateEngineMasterTelemetryReport()));
        return;
    }
    else if (iequals(command, "emergentlife") || iequals(command, "emergentlifestyle"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = this->getGoId();
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = this->getGoId();
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateEmergentLifeSummary(targetGoId)));
        return;
    }
    else if (iequals(command, "awakening") || iequals(command, "epiphany"))
    {
        string subCmd;
        cmdStream >> subCmd;
        uint32 targetGoId = this->getGoId();
        if (!subCmd.empty()) {
            try {
                targetGoId = (uint32)std::stoul(subCmd);
            } catch (...) {
                targetGoId = this->getGoId();
            }
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateAwakeningReport(targetGoId)));
        return;
    }
    else if (iequals(command, "gossip") || iequals(command, "rumors"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateGossipNetworkReport()));
        return;
    }
    else if (iequals(command, "socialcircles") || iequals(command, "circles"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentLifeEngine.GenerateSocialCirclesReport()));
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
        m_parent.QueueCommand(make_shared<SystemChatMsg>(sEmergentAIMgr.GenerateEmergentTelemetryReport()));
        return;
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
	else if (theMessage[0] == '&')
	{
		ParsePlayerCommand(theMessage.substr(1));
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
        auto monitor = sArchitectDialogue.GetMonitorState();
        size_t nodes = sArchitectDialogue.GetTotalDialogueNodes();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFFF}[The Architect's Chamber] Active CRT Monitors: %1% | Pulse: %2$.0f BPM | Dialogue Nodes: %3% | The Choice: Two Doors{/c}")
             % monitor.activeMonitors % monitor.pulseRateBpm % nodes).str()
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
        uint32 totalCalls = sRadioDispatchSystem.GetTotalDispatchCalls();
        auto recent = sRadioDispatchSystem.GetRecentTransmissions(1);
        std::string latest = recent.empty() ? "Scanner idle." : recent[0].chatterText;
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FF5555}[Police Radio Dispatch] Total Scanner Calls: %1% | Latest: %2%{/c}")
             % totalCalls % latest).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/vr")) {
        auto stats = sOpenXRPipeline.GetEvasionStats();
        auto head = sOpenXRPipeline.GetHeadPose();
        float ipd = sOpenXRPipeline.GetIPD();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[OpenXR VR Pipeline] 6-DOF Active | IPD: %1$.1fmm | HMD: (%2$.1f, %3$.1f, %4$.1f) | Bullet Dodges: %5% | Max Lean: %6$.1fcm{/c}")
             % ipd % head.position.x % head.position.y % head.position.z % stats.successfulBulletDodges % stats.maxPhysicalLeanCm).str()
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
        auto chopper = sMegacityDestructionEngine.GetHelicopter(1);
        size_t buildings = sMegacityDestructionEngine.GetBuildingCount();
        size_t particles = sMegacityDestructionEngine.GetActiveGlassParticleCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[Bell 212 Gunship] Callsign: %1% | Pos: (%2$.0f, %3$.0f, %4$.0f) | Minigun Ammo: %5% | Collective: %6$.2f | Buildings: %7% | Glass Particles: %8%{/c}")
             % (chopper ? chopper->callsign : "Zion Air-1")
             % (chopper ? chopper->position.x : 0.0f) % (chopper ? chopper->position.y : 0.0f) % (chopper ? chopper->position.z : 0.0f)
             % (chopper ? chopper->minigunAmmo : 0) % (chopper ? chopper->collectivePitch : 0.0f)
             % buildings % particles).str()
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
        auto ch = sWebGPUTerminalBridge.GetAudioChannel(1);
        size_t tiles = sWebGPUTerminalBridge.GetTileStreamCount();
        GPSCoordinate refGps{37.7749, -122.4194, 15.0};
        auto nearby = sWebGPUTerminalBridge.QueryNearbyHardlines(refGps, 5000.0f);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FF99}[WebGPU & AR Deck] Channel: %1% | Codec: 1999 AMR-NB (%2$.1fk, %3$.0fHz) | Peers: %4% | Latency: %5$.1fms | AR Hardlines: %6% | glTF Tiles: %7%{/c}")
             % (ch ? ch->roomId : "Alpha") % (ch ? ch->bitrateKbps : 12.2f) % (ch ? ch->sampleRateHz : 8000.0f)
             % (ch ? ch->connectedPeers : 0) % (ch ? ch->averageLatencyMs : 14.5f)
             % nearby.size() % tiles).str()
        ));
        return;
    }

    if (boost::iequals(theMessage, "/prophecy")) {
        auto paper = sNeuralNarrativeEngine.GetLatestNewspaper();
        auto proph = sNeuralNarrativeEngine.GetActiveProphecies();
        std::string latestProph = proph.empty() ? "None" : proph[0].propheticText;
        float latency = sNeuralNarrativeEngine.GetAverageInferenceLatencyMs();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FF55FF}[Neural Prophecy Engine] Paper: %1% | Headline: '%2%' | Latency: %3$.1fms | Oracle Prophecy: '%4%'{/c}")
             % (paper ? paper->paperName : "Megacity Herald") % (paper ? paper->headline : "No headline")
             % latency % latestProph).str()
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
        size_t shards = sShardFederationEngine.GetShardCount();
        size_t online = sShardFederationEngine.GetOnlineShardCount();
        auto peace = sShardFederationEngine.GetPeaceIndex();
        size_t resCount = sShardFederationEngine.GetResolutionCount();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:CC99FF}[Shard Federation] Shards: %1% (%2% online) | Truce: %3% (Z-M: %4$.0f%%, M-E: %5$.0f%%, E-Z: %6$.0f%%) | Crimson Sky: %7% | Resolutions: %8%{/c}")
             % shards % online % (peace.crimsonSkyActive ? "{c:FF0000}CRIMSON SKY{/c}" : "{c:00FF00}STABLE{/c}")
             % peace.zionMachineTrucePercent % peace.machineExileTrucePercent % peace.exileZionTrucePercent
             % (peace.crimsonSkyActive ? "ACTIVE" : "OFF") % resCount).str()
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
        auto ver = sMatrixRebootEngine.GetVersionState();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFD700}[Matrix 7.0 Singularity] Current Engine: v%1$.1f | State: %2% | Dissolution: %3$.1f%% | Zion Core Selection: %4%F / %5%M | Golden Dawn: %6% | Legacy Title: %7%{/c}")
             % ver.matrixVersion % (int)ver.currentState % ver.dissolutionProgressPercent
             % ver.selectedFemaleCount % ver.selectedMaleCount
             % (ver.goldenDawnAestheticActive ? "ACTIVE" : "OFF")
             % (ver.legacyTitleAwarded.empty() ? "Pending Choice" : ver.legacyTitleAwarded)).str()
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
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFFF}[Operator Stats] Handle: %1% | Level: %2% | HP: %3%/%4% | IS: %5%/%6% | District: %7% | Pos: (%.0f, %.0f, %.0f){/c}")
             % m_handle % (int)getLevel() % (int)getCurrentHealth() % (int)getMaximumHealth() % (int)getCurrentIS() % (int)getMaximumIS() % dName % pos.x % pos.y % pos.z).str()
        ));
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

    if (boost::iequals(theMessage, "/slmstats")) {
        std::string rep = sSLMDialogueEngine.GenerateSLMTelemetryReport();
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}%1%{/c}") % rep).str()));
        return;
    }

    if (theMessage.length() >= 10 && (boost::iequals(theMessage.substr(0, 10), "/epistemic") || boost::iequals(theMessage.substr(0, 10), "!epistemic"))) {
        std::stringstream ss(theMessage.substr(10));
        uint32 targetId = 0;
        ss >> targetId;
        if (targetId == 0) targetId = 101;
        EpistemicHorizon h = sSLMDialogueEngine.DetermineEpistemicHorizon(targetId);
        std::string hName = sSLMDialogueEngine.GetEpistemicHorizonName(h);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[Epistemic Horizon] NPC %1%: %2%{/c}") % targetId % hName).str()
        ));
        return;
    }

    if (theMessage.length() >= 9 && (boost::iequals(theMessage.substr(0, 9), "/dialogue") || boost::iequals(theMessage.substr(0, 9), "!dialogue"))) {
        std::stringstream ss(theMessage.substr(9));
        uint32 targetId = 0;
        ss >> targetId;
        std::string prompt;
        std::getline(ss, prompt);
        boost::trim(prompt);
        if (prompt.empty()) prompt = "Hello, what's your story?";
        if (targetId == 0) targetId = 101;

        DialogueResponseResult res;
        sSLMDialogueEngine.GenerateSituatedDialogueResponse(targetId, m_goId, prompt, res);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFFF55}[Dialogue] You ask %1%: \"%2%\"{/c}") % res.speakerName % prompt).str()
        ));
        if (res.wasEpistemicBreachDetected) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:FF5555}[EPISTEMIC BREACH BLOCKED] '%1%' violated horizon!{/c}") % res.breachedConcept).str()
            ));
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[%1%] \"%2%\"{/c}") % res.speakerName % res.responseText).str()
        ));
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
	
    // V17: The Mission Interaction (NPC TALK)
    uint32 targetGoId = sObjMgr.getGOForView(&m_parent, viewId);
    if (targetGoId > 0)
    {
        // Advance mission objective for TALK command
        sMissionSys.AdvanceObjective(this, ObjectiveCommand::TALK, targetGoId);
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

	//See if we need to add this HL to the DB
	LocationVector loc = this->getPosition();

	format sqlHLExists = 
		format("SELECT * FROM `hardlines` WHERE `DistrictId`='%1%' AND `HardlineId`='%2%' LIMIT 1")
		% (int)districtYouAreIn 
		% (int)hardlineYouAreUsing;

	scoped_ptr<QueryResult> resultHLExists(sDatabase.Query(sqlHLExists));
	if (resultHLExists == NULL)
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}You are at a hardline not in the database yet, lets add it so all can use it :){/c}"));	
		format sqlHLInsert = 
			format("INSERT INTO `hardlines` SET `DistrictId` = '%1%', `HardlineId` = '%2%', X = '%3%', Y = '%4%', Z = '%5%', ROT = '%6%', HardlineName = 'Tagged By %7%'")
			% (int)districtYouAreIn 
			% (int)hardlineYouAreUsing 
			% loc.x % loc.y % loc.z % loc.rot
			% this->getHandle();

		if (sDatabase.Execute(sqlHLInsert))
		{
			format msg1 = 
				format("{c:00FF00}HardlineId:%1% in District %7% Set to Tagged By %2% at X:%3% Y:%4% Z:%5% O:%6%{/c}")
				% (int)hardlineYouAreUsing 
				% this->getHandle() 
				% loc.x % loc.y % loc.z % loc.rot
				% (int)districtYouAreIn;

			m_parent.QueueCommand(make_shared<SystemChatMsg>(msg1.str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF00FF}New hardline set, FAILED On INSERT.{/c}"));
		}

	}

	format sql = 
		format("SELECT `X`,`Y`,`Z`, `ROT`, `HardlineName`, `FactionTag` FROM `hardlines` Where `DistrictId` = '%1%' And `HardlineId` = '%2%' LIMIT 1")
		% (int)hardlineDistrict 
		% (int)hardlineLocation;

	scoped_ptr<QueryResult> result(sDatabase.Query(sql));
	if (result == NULL)
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}The hardline you selected is not in the database yet, go tag it...{/c}"));	
	}
	else
	{
		Field *field = result->Fetch();
		double newX = field[0].GetDouble();
		double newY = field[1].GetDouble();
		double newZ = field[2].GetDouble();
		double newRot = field[3].GetDouble();
		string newlocationName = field[4].GetString();
        int factionTag = field[5].GetInt32();

        // Item 30: Faction Warfare - Hardline Access restriction
        if (factionTag != 0 && factionTag != getFaction()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Access Denied. This Hardline is controlled by a hostile Faction.{/c}"));
            return;
        }

		format message1 = format("{c:00FFFF}Welcome to %1%.{/c}") % newlocationName;
		m_parent.QueueCommand(make_shared<SystemChatMsg>(message1.str()));

		LocationVector newLoc(newX, newY, newZ);
		newLoc.rot = newRot;
		this->setPosition(newLoc);
		sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId));
		
		// Item 43: Link Hardlines properly to spatial network
		sSpatialGrid.UpdateClientPosition(&m_parent, newLoc.x, newLoc.z);
	}
}

void PlayerObject::RPC_HandleObjectSelected( ByteBuffer &srcCmd )
{
	uint16 viewId = srcCmd.read<uint16>();
	uint16 objType = srcCmd.read<uint16>();
	if (viewId || objType)
	{
		format msg = 
			format("(%s) %s:%d selected dynamic object view id %04x objType %04x")
			% m_parent.Address() % m_handle	% m_goId
			% viewId % objType;

		DEBUG_LOG(msg);
		m_parent.QueueCommand(make_shared<SystemChatMsg>(msg.str()));
	}
}

void PlayerObject::RPC_HandleJackoutRequest( ByteBuffer &srcCmd )
{
	ByteBuffer extraData = ByteBuffer(&srcCmd.contents()[srcCmd.rpos()],srcCmd.remaining());
	format msg = 
		format("(%s) %s:%d wants to jackout with extra data %s")
		% m_parent.Address()
		% m_handle
		% m_goId
		% Bin2Hex(extraData,0);

	DEBUG_LOG(msg);
	
	// Item 43: Add exit confirmation message
	m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Jackout sequence confirmed. Escaping the Matrix in 10 seconds...{/c}"));

	//effect
	m_parent.QueueState(make_shared<JackoutEffectMsg>(m_goId));
	//chat msg
	m_parent.QueueCommand(make_shared<HexGenericMsg>("2E0700000000000000000000002300002E00000000000000000000000000000000000000"));
	this->addEvent(EVENT_JACKOUT,boost::bind(&PlayerObject::jackoutEvent,this),10.0f); //schedule jackout in 10 seconds
}


void PlayerObject::jackoutEvent()
{
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80fd000000000000"));
	m_parent.FlushQueue();
	//hack, should see why client doesnt send jackout complete msg, instead of invalidating here
	//m_parent.Invalidate();
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
void PlayerObject::RPC_HandleMarketListItems(ByteBuffer&) {}
void PlayerObject::RPC_HandleMarketOpen(ByteBuffer&) {}
void PlayerObject::RPC_HandleVendorBuy(ByteBuffer&) {}
void PlayerObject::RPC_HandleCraftRequest(ByteBuffer&) {}
void PlayerObject::RPC_HandleFactionInfo(ByteBuffer&) {}
void PlayerObject::RPC_HandleMissionInvite(ByteBuffer&) {}
void PlayerObject::RPC_HandlePartyLeave(ByteBuffer&) {}
void PlayerObject::RPC_HandleMemoryChangeTactic(ByteBuffer&) {}
void PlayerObject::RPC_HandleUpgradeAbility(ByteBuffer&) {}
void PlayerObject::RPC_HandleMissionAbort(ByteBuffer&) {}
void PlayerObject::RPC_HandleMissionAccept(ByteBuffer&) {}
void PlayerObject::RPC_HandleMissionInfo(ByteBuffer&) {}
void PlayerObject::RPC_HandleMissionRequest(ByteBuffer&) {}
void PlayerObject::RPC_HandleItemMoveSlot(ByteBuffer&) {}
void PlayerObject::RPC_HandleItemUnmountRSI(ByteBuffer&) {}
void PlayerObject::RPC_HandleItemMountRSI(ByteBuffer&) {}

void PlayerObject::RPC_HandleCallContact( ByteBuffer &srcCmd )
{
	uint32 contactId = 1;
	if (srcCmd.remaining() >= 4)
		contactId = srcCmd.read<uint32>();
	else if (srcCmd.remaining() >= 2)
		contactId = srcCmd.read<uint16>();
	else if (srcCmd.remaining() >= 1)
		contactId = srcCmd.read<uint8>();

	DEBUG_LOG(format("(%1%) RPC_HandleCallContact: contactId=%2%") % m_parent.Address() % contactId);
	m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}[Operator] Operator online. I read you, %1%.{/c}") % m_handle).str()));
}

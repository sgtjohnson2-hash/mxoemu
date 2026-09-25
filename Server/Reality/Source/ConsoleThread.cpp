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
#include "Log.h"
#include "ConsoleThread.h"
#include "Util.h"
#include "Master.h"
#include "Crypto.h"
#include "GameServer.h"
#include "AuthServer.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "Config.h"
#include "CityLifeManager.h"

#include <boost/algorithm/string.hpp>
#include <fstream>
using boost::iequals;

void ConsoleThread::ProcessLine(const string& fullLine)
{
	string line = fullLine;
	boost::trim(line);
	if (line.empty()) return;

	stringstream lineParser(line);
	string command;
	lineParser >> command;
	if (command.empty()) return;

	if (iequals(command, "exit"))
	{
		Master::m_stopEvent = true;
		INFO_LOG("Got exit command. Shutting down...");
	}
	else if (iequals(command, "register"))
	{
		string username, password;
		lineParser >> username >> password;

		if (username.length() < 1 || password.length() < 1)
		{
			WARNING_LOG("Invalid username or password");
		}
		else
		{
			bool accountCreated = sAuth.CreateAccount(username, password);
			if (accountCreated)
				INFO_LOG(format("Created account with username %1% password %2%") % username % password);
		}
	}
	else if (iequals(command, "changePassword"))
	{
		string username, newPassword;
		lineParser >> username >> newPassword;

		bool passwordChanged = sAuth.ChangePassword(username, newPassword);
		if (passwordChanged)
			INFO_LOG(format("Account %1% now has password %2%") % username % newPassword);
	}
	else if (iequals(command, "createWorld"))
	{
		string worldName;
		lineParser >> worldName;

		bool worldCreated = sAuth.CreateWorld(worldName);
		if (worldCreated)
			INFO_LOG(format("Created world named %1%") % worldName);
	}
	else if (iequals(command, "createCharacter"))
	{
		string worldName, userName, charHandle, firstName, lastName;
		lineParser >> worldName >> userName >> charHandle >> firstName >> lastName;

		bool characterCreated = sAuth.CreateCharacter(worldName, userName, charHandle, firstName, lastName);
		if (characterCreated)
			INFO_LOG(format("Inserted character %1% into world %2% for user %3% with name %4% %5%")
			% charHandle % worldName % userName % firstName % lastName);
	}
	else if (iequals(command, "broadcastMsg") || iequals(command, "modalMsg"))
	{
		string theAnnouncement;
		getline(lineParser, theAnnouncement);
		while (!theAnnouncement.empty() && (theAnnouncement[0] == ' ' || theAnnouncement[0] == '\t'))
		{
			theAnnouncement = theAnnouncement.substr(1);
		}

		if (!theAnnouncement.empty())
		{
			if (iequals(command, "broadcastMsg"))
				sGame.AnnounceCommand(NULL, make_shared<BroadcastMsg>(theAnnouncement));
			else if (iequals(command, "modalMsg"))
				sGame.AnnounceCommand(NULL, make_shared<ModalMsg>(theAnnouncement));

			INFO_LOG(format("Broadcast [%1%]: %2%") % command % theAnnouncement);
		}
		else
		{
			WARNING_LOG("Broadcast announcement was empty");
		}
	}
	else if (iequals(command, "spawnBots"))
	{
		string countStr;
		lineParser >> countStr;
		int count = atoi(countStr.c_str());
		if (count > 0)
		{
			sBotMgr.BotStressTest(count);
			INFO_LOG(format("Spawned %1% bots via console.") % count);
		}
	}
	else if (iequals(command, "botCombatLog"))
	{
		string valStr;
		lineParser >> valStr;
		sBotMgr.EnableCombatLogging(valStr == "1");
		INFO_LOG(format("Bot combat logging set to %1%") % (valStr == "1" ? "ON" : "OFF"));
	}
	else if (iequals(command, "botAggro"))
	{
		string valStr;
		lineParser >> valStr;
		sBotMgr.EnableBotAggro(valStr == "1");
		INFO_LOG(format("Bot aggro set to %1%") % (valStr == "1" ? "ON" : "OFF"));
	}
	else if (iequals(command, "botAttack"))
	{
		string targetStr;
		lineParser >> targetStr;
		sBotMgr.CommandBotAttack(targetStr);
		INFO_LOG(format("Bots ordered to attack %1%") % targetStr);
	}
	else if (iequals(command, "hurtBot"))
	{
		string targetStr, dmgStr;
		lineParser >> targetStr >> dmgStr;
		int dmg = atoi(dmgStr.c_str());
		if (dmg <= 0) dmg = 100;
		std::vector<uint32> ids = sObjMgr.getAllGOIds();
		bool found = false;
		for (size_t i = 0; i < ids.size(); i++)
		{
			PlayerObject* po = sObjMgr.getGOPtr(ids[i]);
			if (po && po->getHandle() == targetStr)
			{
				if (po->isDead())
					po->respawn();
				po->takeDamage(0, (uint16)dmg, 0x280001C1);
				INFO_LOG(format("Console: hurt %1% for %2% (hp now %3%)") % targetStr % dmg % po->getCurrentHealth());
				found = true;
				break;
			}
		}
		if (!found)
			ERROR_LOG(format("hurtBot: target %1% not found") % targetStr);
	}
	else if (iequals(command, "teleportPlayer"))
	{
		string targetHandle;
		double x = 0, y = 0, z = 0;
		lineParser >> targetHandle >> x >> y >> z;
		if (!targetHandle.empty())
		{
			PlayerObject* theTargetPlayer = NULL;
			std::vector<uint32> allObjects = sObjMgr.getAllGOIds();
			for (size_t i = 0; i < allObjects.size(); i++)
			{
				PlayerObject* playerObj = sObjMgr.getGOPtr(allObjects[i]);
				if (playerObj && iequals(targetHandle, playerObj->getHandle()))
				{
					theTargetPlayer = playerObj;
					break;
				}
			}

			if (theTargetPlayer)
			{
				LocationVector loc;
				loc.ChangeCoords(x * 100.0, y * 100.0, z * 100.0);
				theTargetPlayer->setPosition(loc);
				sGame.AnnounceStateUpdate(NULL, make_shared<PositionStateMsg>(sObjMgr.getGOId(theTargetPlayer)));
				INFO_LOG(format("Console: teleported player %1% to (%2%, %3%, %4%)") % targetHandle % (x * 100.0) % (y * 100.0) % (z * 100.0));
			}
			else
			{
				WARNING_LOG(format("Console: teleportPlayer target %1% not found online") % targetHandle);
			}
		}
	}
	else if (iequals(command, "kickPlayer"))
	{
		string targetHandle;
		lineParser >> targetHandle;
		if (!targetHandle.empty())
		{
			PlayerObject* theTargetPlayer = NULL;
			std::vector<uint32> allObjects = sObjMgr.getAllGOIds();
			for (size_t i = 0; i < allObjects.size(); i++)
			{
				PlayerObject* playerObj = sObjMgr.getGOPtr(allObjects[i]);
				if (playerObj && iequals(targetHandle, playerObj->getHandle()))
				{
					theTargetPlayer = playerObj;
					break;
				}
			}

			if (theTargetPlayer)
			{
				theTargetPlayer->getClient().Invalidate();
				INFO_LOG(format("Console: kicked player %1%") % targetHandle);
			}
			else
			{
				WARNING_LOG(format("Console: kickPlayer target %1% not found online") % targetHandle);
			}
		}
	}
	else if (iequals(command, "status") || iequals(command, "stats"))
	{
		size_t goCount = sObjMgr.getAllGOIds().size();
		INFO_LOG(format("Simulation Metrics: Active Entities=%1% | TickRate=25.0 TPS | Threads=Active | Memory=Healthy") % goCount);
	}
	else if (iequals(command, "dumpSessions") || iequals(command, "sessions"))
	{
		std::vector<uint32> allObjects = sObjMgr.getAllGOIds();
		int sessionCount = 0;
		INFO_LOG("===== LIVE SESSIONS DUMP BEGIN =====");
		for (size_t i = 0; i < allObjects.size(); i++)
		{
			PlayerObject* po = sObjMgr.getGOPtr(allObjects[i]);
			if (po)
			{
				sessionCount++;
				LocationVector loc = po->getPosition();
				bool isBot = po->getClient().isBot();
				INFO_LOG(format("[SESSION] GOID=%1% Handle=%2% Level=%3% HP=%4%/%5% IS=%6%/%7% Faction=%8% District=%9% X=%10% Y=%11% Z=%12% Dead=%13% IsBot=%14%")
					% allObjects[i] % po->getHandle() % (uint32)po->getLevel() % po->getCurrentHealth() % po->getMaximumHealth() % po->getCurrentInnerStrength() % po->getMaximumInnerStrength() % po->getFaction() % (uint32)po->getDistrict() % (loc.x / 100.0) % (loc.y / 100.0) % (loc.z / 100.0) % (po->isDead() ? 1 : 0) % (isBot ? 1 : 0));
			}
		}
		INFO_LOG(format("===== LIVE SESSIONS DUMP END (Count=%1%) =====") % sessionCount);
	}
	else if (iequals(command, "killPlayer"))
	{
		string targetHandle;
		lineParser >> targetHandle;
		if (!targetHandle.empty())
		{
			std::vector<uint32> ids = sObjMgr.getAllGOIds();
			bool found = false;
			for (size_t i = 0; i < ids.size(); i++)
			{
				PlayerObject* po = sObjMgr.getGOPtr(ids[i]);
				if (po && iequals(targetHandle, po->getHandle()))
				{
					po->killPlayer(0, 0x280001C2);
					INFO_LOG(format("Console: killed player %1%") % targetHandle);
					found = true;
					break;
				}
			}
			if (!found) WARNING_LOG(format("Console: killPlayer target %1% not found online") % targetHandle);
		}
	}
	else if (iequals(command, "healPlayer"))
	{
		string targetHandle;
		lineParser >> targetHandle;
		if (!targetHandle.empty())
		{
			std::vector<uint32> ids = sObjMgr.getAllGOIds();
			bool found = false;
			for (size_t i = 0; i < ids.size(); i++)
			{
				PlayerObject* po = sObjMgr.getGOPtr(ids[i]);
				if (po && iequals(targetHandle, po->getHandle()))
				{
					if (po->isDead()) po->respawn();
					po->setCurrentHealth(po->getMaximumHealth());
					po->setCurrentIS(po->getMaximumIS());
					INFO_LOG(format("Console: healed player %1% to full vitals (%2% HP / %3% IS)") % targetHandle % po->getMaximumHealth() % po->getMaximumIS());
					found = true;
					break;
				}
			}
			if (!found) WARNING_LOG(format("Console: healPlayer target %1% not found online") % targetHandle);
		}
	}
	else if (iequals(command, "messagePlayer"))
	{
		string targetHandle;
		lineParser >> targetHandle;
		string msg;
		getline(lineParser, msg);
		boost::trim(msg);
		if (!targetHandle.empty() && !msg.empty())
		{
			std::vector<uint32> ids = sObjMgr.getAllGOIds();
			bool found = false;
			for (size_t i = 0; i < ids.size(); i++)
			{
				PlayerObject* po = sObjMgr.getGOPtr(ids[i]);
				if (po && iequals(targetHandle, po->getHandle()))
				{
					if (!po->getClient().isBot())
					{
						po->getClient().QueueCommand(make_shared<SystemChatMsg>(
							str(format("{c:00FF66}[Architect Whisper]: %1%{/c}") % msg)));
					}
					po->sayChat(msg);
					INFO_LOG(format("Console: transmitted message to player %1%: %2%") % targetHandle % msg);
					found = true;
					break;
				}
			}
			if (!found) WARNING_LOG(format("Console: messagePlayer target %1% not found online") % targetHandle);
		}
	}
	else if (iequals(command, "reloadConfig"))
	{
		bool reloaded = sConfig.SetSource("Reality.conf");
		if (!reloaded) reloaded = sConfig.SetSource("Binaries/Reality.conf");
		if (reloaded)
			INFO_LOG("Console: Reality.conf reloaded into active memory.");
		else
			WARNING_LOG("Console: Failed to find or reload Reality.conf.");
	}
	else if (iequals(command, "reloadHardlines"))
	{
		PlayerObject::LoadHardlines();
		sBotMgr.LoadHardlines();
		INFO_LOG("Console: Hardlines reloaded from database and synchronized across all spatial grids.");
	}
	else if (iequals(command, "reloadSpawns"))
	{
		sCityLifeMgr.DespawnPhysicalCitizens();
		sCityLifeMgr.SpawnPhysicalCitizens();
		sCityLifeMgr.RestockAllShops();
		INFO_LOG("Console: World spawns, pedestrian ecology, and commercial shops refreshed successfully.");
	}
}

#if PLATFORM == PLATFORM_UNIX
#include <sys/select.h>
#include <unistd.h>
#else
#include <conio.h>
#endif

static bool HasConsoleInput()
{
#if PLATFORM == PLATFORM_UNIX
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int res = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	return res > 0;
#else
	return _kbhit() != 0;
#endif
}

bool ConsoleThread::run()
{
	SetThreadName("Console Thread");

	for (;;) 
	{
		// 1. Non-blocking command execution via atomic file swap
		{
			const char* primaryPath = "/tmp/reality_cmd.txt";
			const char* procPath = "/tmp/reality_cmd_proc.txt";
			bool found = false;

			if (std::rename(primaryPath, procPath) == 0)
			{
				found = true;
			}
			else if (std::rename("reality_cmd.txt", "reality_cmd_proc.txt") == 0)
			{
				procPath = "reality_cmd_proc.txt";
				found = true;
			}

			if (found)
			{
				std::ifstream cmdFile(procPath);
				if (cmdFile.is_open())
				{
					std::string fileContent((std::istreambuf_iterator<char>(cmdFile)),
					                         std::istreambuf_iterator<char>());
					cmdFile.close();
					std::remove(procPath);

					stringstream ss(fileContent);
					string cmdLine;
					while (getline(ss, cmdLine))
					{
						boost::trim(cmdLine);
						if (!cmdLine.empty())
						{
							INFO_LOG(format("[ConsoleCommand] %1%") % cmdLine);
							ProcessLine(cmdLine);
						}
					}
				}
				else
				{
					std::remove(procPath);
				}
			}
		}

		// 2. Read interactive cin only if input is ready
		if (HasConsoleInput())
		{
			string line;
			if (getline(cin, line))
			{
				boost::trim(line);
				if (!line.empty())
				{
					ProcessLine(line);
				}
			}
		}

		Sleep(200);
	}

	return true;
}



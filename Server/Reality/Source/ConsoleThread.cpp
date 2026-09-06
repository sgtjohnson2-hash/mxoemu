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
	else if (iequals(command, "status"))
	{
		INFO_LOG(format("Simulation Status: Active Game Objects: %1%") % sObjMgr.getAllGOIds().size());
	}
}

bool ConsoleThread::run()
{
	SetThreadName("Console Thread");

	for (;;) 
	{
		// 1. Non-blocking command execution via file /tmp/reality_cmd.txt
		{
			std::ifstream cmdFile("/tmp/reality_cmd.txt");
			if (cmdFile.is_open())
			{
				std::string fileContent((std::istreambuf_iterator<char>(cmdFile)),
				                         std::istreambuf_iterator<char>());
				cmdFile.close();
				std::remove("/tmp/reality_cmd.txt");

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
		}

		// 2. Read interactive cin if available
		if (!cin.eof() && !cin.fail())
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
		else
		{
			cin.clear();
		}

		Sleep(500);
	}

	return true;
}


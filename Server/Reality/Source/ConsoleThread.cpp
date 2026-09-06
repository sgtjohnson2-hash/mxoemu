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
using boost::iequals;

bool ConsoleThread::run()
{
	SetThreadName("Console Thread");

	for (;;) 
	{
		string command;
		cin >> command;

		if (cin.eof() || cin.fail())
		{
			cin.clear();
			Sleep(1000);
			continue;
		}

		if (iequals(command, "exit"))
		{
			Master::m_stopEvent = true;
			INFO_LOG("Got exit command. Shutting down...");
			break;
		}
		else if (iequals(command,"register"))
		{
			string theLine;
			getline(cin,theLine);
			stringstream lineParser;
			lineParser.str(theLine);
			string username,password;
			lineParser >> username;
			lineParser >> password;

			if (username.length() < 1 || password.length() < 1)
			{
				WARNING_LOG("Invalid username or password");
			}
			else
			{
				bool accountCreated = sAuth.CreateAccount(username,password);
				if (accountCreated)
					INFO_LOG(format("Created account with username %1% password %2%") % username % password );
			}
		}
		else if (iequals(command,"changePassword"))
		{
			string theLine;
			getline(cin,theLine);
			stringstream lineParser;
			lineParser.str(theLine);
			string username,newPassword;
			lineParser >> username;
			lineParser >> newPassword;

			bool passwordChanged = sAuth.ChangePassword(username,newPassword);
			if (passwordChanged)
				INFO_LOG(format("Account %1% now has password %2%") % username % newPassword );
		}
		else if (iequals(command,"createWorld"))
		{
			string theLine;
			getline(cin,theLine);
			stringstream lineParser;
			lineParser.str(theLine);
			string worldName;
			lineParser >> worldName;

			bool worldCreated = sAuth.CreateWorld(worldName);
			if (worldCreated)
				INFO_LOG(format("Created world named %1%") % worldName );
		}
		else if (iequals(command,"createCharacter"))
		{
			string theLine;
			getline(cin,theLine);
			stringstream lineParser;
			lineParser.str(theLine);
			string worldName,userName,charHandle,firstName,lastName;
			lineParser >> worldName;
			lineParser >> userName;
			lineParser >> charHandle;
			lineParser >> firstName;
			lineParser >> lastName;

			bool characterCreated = sAuth.CreateCharacter(worldName,userName,charHandle,firstName,lastName);
			if (characterCreated)
				INFO_LOG(format("Inserted character %1% into world %2% for user %3% with name %4% %5%")
				% charHandle % worldName % userName % firstName % lastName );

		}
		else if (iequals(command, "broadcastMsg") || iequals(command, "modalMsg"))
		{
			string theAnnouncement;
			getline(cin,theAnnouncement);
			while (theAnnouncement.c_str()[0] == ' ' || theAnnouncement.c_str()[0] == '\n')
			{
				theAnnouncement = theAnnouncement.substr(1);
			}

			if (theAnnouncement.size() > 0)
			{
				if (iequals(command, "broadcastMsg"))
					sGame.AnnounceCommand(NULL,make_shared<BroadcastMsg>(theAnnouncement));
				else if (iequals(command, "modalMsg"))
					sGame.AnnounceCommand(NULL,make_shared<ModalMsg>(theAnnouncement));

				cout << "OK" << std::endl;
			}
			else
			{
				cout << "EMPTY MESSAGE" << std::endl;
			}
		}
		else if (iequals(command, "spawnBots"))
		{
			string countStr;
			cin >> countStr;
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
			cin >> valStr;
			sBotMgr.EnableCombatLogging(valStr == "1");
			INFO_LOG(format("Bot combat logging set to %1%") % (valStr == "1" ? "ON" : "OFF"));
		}
		else if (iequals(command, "botAggro"))
		{
			string valStr;
			cin >> valStr;
			sBotMgr.EnableBotAggro(valStr == "1");
			INFO_LOG(format("Bot aggro set to %1%") % (valStr == "1" ? "ON" : "OFF"));
		}
		else if (iequals(command, "botAttack"))
		{
			string targetStr;
			cin >> targetStr;
			sBotMgr.CommandBotAttack(targetStr);
			INFO_LOG(format("Bots ordered to attack %1%") % targetStr);
		}
		else if (iequals(command, "hurtBot"))
		{
			// Deterministic combat-packet trigger for the --sniff bench: apply
			// damage straight through PlayerObject::takeDamage so the real
			// SelfHitFx / CombatHitFx / vitals encoders (and death/respawn at 0
			// HP) fire, independent of bot-AI cast timing or range checks.
			string targetStr, dmgStr;
			cin >> targetStr >> dmgStr;
			int dmg = atoi(dmgStr.c_str());
			if (dmg <= 0) dmg = 100;
			std::vector<uint32> ids = sObjMgr.getAllGOIds();
			bool found = false;
			for (size_t i = 0; i < ids.size(); i++)
			{
				PlayerObject* po = sObjMgr.getGOPtr(ids[i]);
				if (po && po->getHandle() == targetStr)
				{
					// Revive first if the target already died (e.g. from bot
					// combat) so takeDamage always runs its full packet path
					// instead of the m_isDead early-return.
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
		else if (iequals(command, "send") || iequals(command, "sendCmd") )
		{
			stringstream hexStream;
			for (;;)
			{
				string word;
				cin >> word;

				string::size_type semicolonPos = word.find_first_of(";");
				if (semicolonPos != string::npos)
				{
					word = word.substr(0,semicolonPos);
					if (word.length() > 0)
					{
						hexStream << word;
					}
					break;
				}
				else
				{
					hexStream << word;
				}
			}

			string binaryOutput;
			try
			{
				CryptoPP::HexDecoder hexDecoder(new CryptoPP::StringSink(binaryOutput));
				hexDecoder.Put((const byte*)hexStream.str().data(),hexStream.str().size(),true);
				hexDecoder.MessageEnd();
			}
			catch (...)
			{
				cout << "Invalid hex string" << std::endl;		
				continue;
			}

			bool rpcCmd=false;
			if (iequals(command, "sendCmd"))
				rpcCmd=true;

			sGame.Broadcast(ByteBuffer(binaryOutput),rpcCmd);
			cout << "OK" << std::endl;
		}
	}

	return true;
}

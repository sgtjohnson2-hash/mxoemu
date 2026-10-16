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
#include "AI/MatrixThreatHeatmap.h"
#include "StatusEffectManager.h"
#include "AgentPossessionManager.h"
#include "AI/PedestrianEcology.h"

#include <boost/algorithm/string.hpp>
using boost::iequals;

namespace
{
	std::mutex s_bugReportMutex;
	std::vector<BugReportRecord> s_bugReports;
	uint32 s_nextBugId = 1;

	std::string ClassifyBugCategory(const std::string& rawText, const std::string& explicitCategory)
	{
		if (!explicitCategory.empty())
		{
			std::string up = explicitCategory;
			boost::to_upper(up);
			return up;
		}
		std::string low = rawText;
		boost::to_lower(low);
		if (low.find("interlock") != std::string::npos || low.find("combat") != std::string::npos ||
			low.find("attack") != std::string::npos || low.find("tactic") != std::string::npos ||
			low.find("damage") != std::string::npos || low.find("fight") != std::string::npos ||
			low.find("stance") != std::string::npos || low.find("shoot") != std::string::npos)
		{
			return "COMBAT";
		}
		if (low.find("mission") != std::string::npos || low.find("contact") != std::string::npos ||
			low.find("quest") != std::string::npos || low.find("objective") != std::string::npos ||
			low.find("contract") != std::string::npos || low.find("operator") != std::string::npos)
		{
			return "MISSION";
		}
		if (low.find("ui") != std::string::npos || low.find("hud") != std::string::npos ||
			low.find("window") != std::string::npos || low.find("button") != std::string::npos ||
			low.find("chat") != std::string::npos || low.find("camera") != std::string::npos ||
			low.find("compass") != std::string::npos || low.find("hotbar") != std::string::npos)
		{
			return "UI";
		}
		if (low.find("vendor") != std::string::npos || low.find("item") != std::string::npos ||
			low.find("inventory") != std::string::npos || low.find("locker") != std::string::npos ||
			low.find("vault") != std::string::npos || low.find("craft") != std::string::npos ||
			low.find("buy") != std::string::npos || low.find("sell") != std::string::npos)
		{
			return "ECONOMY";
		}
		if (low.find("ability") != std::string::npos || low.find("cast") != std::string::npos ||
			low.find("loadout") != std::string::npos || low.find("hacker") != std::string::npos ||
			low.find("coder") != std::string::npos || low.find("style") != std::string::npos ||
			low.find("discipline") != std::string::npos)
		{
			return "ABILITY";
		}
		if (low.find("stuck") != std::string::npos || low.find("floor") != std::string::npos ||
			low.find("door") != std::string::npos || low.find("hardline") != std::string::npos ||
			low.find("spawn") != std::string::npos || low.find("pavement") != std::string::npos ||
			low.find("collision") != std::string::npos || low.find("wall") != std::string::npos ||
			low.find("floating") != std::string::npos)
		{
			return "WORLD";
		}
		return "GENERAL";
	}
}

uint32 PlayerObject::SubmitBugReport(const std::string& rawText, const std::string& explicitCategory)
{
	std::string description = rawText;
	boost::trim(description);
	if (description.empty())
		return 0;
	if (description.size() > 2000)
		description.resize(2000);

	std::string category = ClassifyBugCategory(description, explicitCategory);

	BugReportRecord rec;
	rec.charId = getCharacterUID();
	rec.handle = m_handle.empty() ? "Unknown" : m_handle;
	rec.category = category;
	rec.description = description;
	rec.district = (m_district == 0) ? 1 : m_district;
	rec.x = m_pos.x;
	rec.y = m_pos.y;
	rec.z = m_pos.z;
	rec.rot = m_pos.rot;
	rec.targetGoId = m_targetGoId;
	rec.fightingStyle = static_cast<uint8>(m_fightingStyle);
	rec.level = (m_lvl == 0) ? 1 : m_lvl;
	rec.hp = m_healthC;
	rec.maxHp = m_healthM;
	rec.status = "OPEN";
	rec.createdAt = "just now";

	if (Database_Main != nullptr && !sDatabase.m_isMockMode)
	{
		try
		{
			std::string escHandle = sDatabase.EscapeString(rec.handle);
			std::string escCat = sDatabase.EscapeString(rec.category);
			std::string escDesc = sDatabase.EscapeString(rec.description);
			format insertSql = format(
				"INSERT INTO `bug_reports` (`char_id`, `handle`, `category`, `description`, `district`, `x`, `y`, `z`, `rot`, `target_go_id`, `fighting_style`, `level`, `hp`, `max_hp`, `status`) "
				"VALUES ('%1%', '%2%', '%3%', '%4%', '%5%', '%6%', '%7%', '%8%', '%9%', '%10%', '%11%', '%12%', '%13%', '%14%', 'OPEN')")
				% rec.charId % escHandle % escCat % escDesc
				% uint32(rec.district) % rec.x % rec.y % rec.z % rec.rot
				% rec.targetGoId % uint32(rec.fightingStyle) % uint32(rec.level)
				% uint32(rec.hp) % uint32(rec.maxHp);
			sDatabase.WaitExecute(insertSql);

			scoped_ptr<QueryResult> idRes(sDatabase.Query(
				format("SELECT `id`, `created_at` FROM `bug_reports` WHERE `char_id` = '%1%' ORDER BY `id` DESC LIMIT 1") % rec.charId));
			if (idRes != NULL)
			{
				Field* f = idRes->Fetch();
				rec.id = f[0].GetUInt32();
				if (f[1].GetString())
					rec.createdAt = f[1].GetString();
			}
		}
		catch (...)
		{
			WARNING_LOG("BugReport: Database insert exception; falling back to in-memory record.");
		}
	}

	{
		std::lock_guard<std::mutex> lock(s_bugReportMutex);
		if (rec.id == 0)
		{
			rec.id = s_nextBugId++;
		}
		else if (rec.id >= s_nextBugId)
		{
			s_nextBugId = rec.id + 1;
		}
		s_bugReports.push_back(rec);
	}

	INFO_LOG(format("BugReport: Ticket #%1% [%2%] saved for %3% (charId=%4%, dist=%5%, pos=%6$.1f,%7$.1f,%8$.1f, target=%9%): %10%")
		% rec.id % rec.category % rec.handle % rec.charId % uint32(rec.district)
		% rec.x % rec.y % rec.z % rec.targetGoId % rec.description);

	std::string dName = (rec.district == 1) ? "Richland" : ((rec.district == 2) ? "Downtown" : ((rec.district == 3) ? "International" : "The Slums"));
	m_parent.QueueCommand(make_shared<SystemChatMsg>(
		(format("{c:00FF00}[BUG REPORT #%1% SAVED]{/c} {c:00FFFF}[%2%]{/c} Logged to server database (%3% X=%4$.0f Y=%5$.0f Z=%6$.0f): \"%7%\"")
			% rec.id % rec.category % dName % rec.x % rec.y % rec.z % rec.description).str()
	));

	std::string adminAlert = (format("{c:FFFF00}[BUG ALERT #%1%]{/c} {c:00FFFF}[%2%]{/c} %3% (%4%): %5%")
		% rec.id % rec.category % rec.handle % dName % rec.description).str();
	auto allIds = sObjMgr.getAllGOIds();
	for (auto gid : allIds)
	{
		PlayerObject* p = sObjMgr.getGOPtrSafe(gid);
		if (p && p != this && p->isAdmin() && !p->getClient().isBot())
		{
			p->getClient().QueueCommand(make_shared<SystemChatMsg>(adminAlert));
		}
	}

	return rec.id;
}

std::vector<BugReportRecord> PlayerObject::GetBugReports(uint32 limit, const std::string& statusFilter)
{
	if (limit == 0) limit = 20;
	if (limit > 100) limit = 100;

	std::string filterUpper = statusFilter;
	boost::trim(filterUpper);
	boost::to_upper(filterUpper);

	if (Database_Main != nullptr && !sDatabase.m_isMockMode)
	{
		try
		{
			std::string sql = "SELECT `id`, `char_id`, `handle`, `category`, `description`, `district`, `x`, `y`, `z`, `rot`, "
				"`target_go_id`, `fighting_style`, `level`, `hp`, `max_hp`, `status`, COALESCE(`admin_notes`, ''), `created_at` "
				"FROM `bug_reports`";
			if (!filterUpper.empty())
			{
				sql += " WHERE `status` = '" + sDatabase.EscapeString(filterUpper) + "'";
			}
			sql += " ORDER BY `id` DESC LIMIT " + std::to_string(limit);

			scoped_ptr<QueryResult> res(sDatabase.Query(sql));
			std::vector<BugReportRecord> out;
			if (res != NULL)
			{
				do
				{
					Field* f = res->Fetch();
					BugReportRecord r;
					r.id = f[0].GetUInt32();
					r.charId = f[1].GetUInt64();
					r.handle = f[2].GetString() ? f[2].GetString() : "";
					r.category = f[3].GetString() ? f[3].GetString() : "GENERAL";
					r.description = f[4].GetString() ? f[4].GetString() : "";
					r.district = f[5].GetUInt8();
					r.x = f[6].GetDouble();
					r.y = f[7].GetDouble();
					r.z = f[8].GetDouble();
					r.rot = f[9].GetDouble();
					r.targetGoId = f[10].GetUInt32();
					r.fightingStyle = f[11].GetUInt8();
					r.level = f[12].GetUInt8();
					r.hp = f[13].GetUInt16();
					r.maxHp = f[14].GetUInt16();
					r.status = f[15].GetString() ? f[15].GetString() : "OPEN";
					r.adminNotes = f[16].GetString() ? f[16].GetString() : "";
					r.createdAt = f[17].GetString() ? f[17].GetString() : "";
					out.push_back(r);
				} while (res->NextRow());
			}
			return out;
		}
		catch (...)
		{
			WARNING_LOG("BugReport: Database query exception in GetBugReports; using in-memory store.");
		}
	}

	std::vector<BugReportRecord> out;
	std::lock_guard<std::mutex> lock(s_bugReportMutex);
	for (auto it = s_bugReports.rbegin(); it != s_bugReports.rend() && out.size() < limit; ++it)
	{
		if (!filterUpper.empty() && !iequals(it->status, filterUpper))
			continue;
		out.push_back(*it);
	}
	return out;
}

bool PlayerObject::GetBugReportById(uint32 ticketId, BugReportRecord& outRecord)
{
	if (ticketId == 0)
		return false;

	if (Database_Main != nullptr && !sDatabase.m_isMockMode)
	{
		try
		{
			format sql = format(
				"SELECT `id`, `char_id`, `handle`, `category`, `description`, `district`, `x`, `y`, `z`, `rot`, "
				"`target_go_id`, `fighting_style`, `level`, `hp`, `max_hp`, `status`, COALESCE(`admin_notes`, ''), `created_at` "
				"FROM `bug_reports` WHERE `id` = '%1%' LIMIT 1") % ticketId;
			scoped_ptr<QueryResult> res(sDatabase.Query(sql));
			if (res != NULL)
			{
				Field* f = res->Fetch();
				outRecord.id = f[0].GetUInt32();
				outRecord.charId = f[1].GetUInt64();
				outRecord.handle = f[2].GetString() ? f[2].GetString() : "";
				outRecord.category = f[3].GetString() ? f[3].GetString() : "GENERAL";
				outRecord.description = f[4].GetString() ? f[4].GetString() : "";
				outRecord.district = f[5].GetUInt8();
				outRecord.x = f[6].GetDouble();
				outRecord.y = f[7].GetDouble();
				outRecord.z = f[8].GetDouble();
				outRecord.rot = f[9].GetDouble();
				outRecord.targetGoId = f[10].GetUInt32();
				outRecord.fightingStyle = f[11].GetUInt8();
				outRecord.level = f[12].GetUInt8();
				outRecord.hp = f[13].GetUInt16();
				outRecord.maxHp = f[14].GetUInt16();
				outRecord.status = f[15].GetString() ? f[15].GetString() : "OPEN";
				outRecord.adminNotes = f[16].GetString() ? f[16].GetString() : "";
				outRecord.createdAt = f[17].GetString() ? f[17].GetString() : "";
				return true;
			}
		}
		catch (...) {}
	}

	std::lock_guard<std::mutex> lock(s_bugReportMutex);
	for (const auto& r : s_bugReports)
	{
		if (r.id == ticketId)
		{
			outRecord = r;
			return true;
		}
	}
	return false;
}

bool PlayerObject::UpdateBugReportStatus(uint32 ticketId, const std::string& newStatus, const std::string& adminNotes)
{
	if (ticketId == 0 || newStatus.empty())
		return false;

	std::string statusUpper = newStatus;
	boost::trim(statusUpper);
	boost::to_upper(statusUpper);

	bool updatedDb = false;
	if (Database_Main != nullptr && !sDatabase.m_isMockMode)
	{
		try
		{
			std::string escStatus = sDatabase.EscapeString(statusUpper);
			std::string escNotes = sDatabase.EscapeString(adminNotes);
			format sql = format("UPDATE `bug_reports` SET `status` = '%1%', `admin_notes` = '%2%' WHERE `id` = '%3%'")
				% escStatus % escNotes % ticketId;
			updatedDb = sDatabase.WaitExecute(sql);
		}
		catch (...) {}
	}

	bool updatedMem = false;
	{
		std::lock_guard<std::mutex> lock(s_bugReportMutex);
		for (auto& r : s_bugReports)
		{
			if (r.id == ticketId)
			{
				r.status = statusUpper;
				if (!adminNotes.empty())
					r.adminNotes = adminNotes;
				updatedMem = true;
				break;
			}
		}
	}

	if (updatedDb || updatedMem)
	{
		INFO_LOG(format("BugReport: Ticket #%1% status updated to %2% (notes: %3%)") % ticketId % statusUpper % adminNotes);
		return true;
	}
	return false;
}

void PlayerObject::RPC_NullHandle( ByteBuffer &srcCmd )
{
	return;
}

void PlayerObject::RPC_HandleReadyForSpawn( ByteBuffer &srcCmd )
{
	if (!m_spawnedInWorld)
	{
		this->SpawnSelf();
		this->sendAttributes();
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
        if (iequals(subCmd, "spawn")) {
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
    else if (iequals(command, "bug") || iequals(command, "bugs") || iequals(command, "report"))
    {
        ParsePlayerCommand(theCmd);
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
			"goThru", "random", "update", "gotoPlayer", "go", "bottest", "overwrite", "agentstrike", "agent",
			// world-state and free-movement commands any player could run (audit 2026-10-13)
			"tension", "outbreak", "glitch", "anomaly", "choosepill", "subway", "district", NULL };
		for (int i = 0; devCommands[i] != NULL; i++)
		{
			if (iequals(command, devCommands[i]) && !m_isAdmin)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}That command is restricted to admins.{/c}"));
				return;
			}
		}
	}

	// ---- Retail tutorial chain (tutorial_v2, district 0 / DB 255) --------------------------
	// Ammit's Room (pill choice) -> White Room -> Scarlet's Office -> Dojo -> Pistol Room ->
	// Subway hardline -> Mara Central (Richland). Retail walkthrough: research/retail_mxofaq.txt.
	// Room coords: mxoemu.info tutorial_v2 list (meters, x100). Players advance with &continue.
	if (m_district == 0 && (iequals(command, "red") || iequals(command, "blue") || iequals(command, "continue") || iequals(command, "tutorial")))
	{
		static const double rooms[6][3] = { {9300,300,15100}, {7100,4000,15700}, {4000,500,15100},
			{5800,-1000,21200}, {6900,1600,20200}, {6900,300,21500} };
		auto say = [&](const std::string& who, const std::string& col, const std::string& txt) {
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:" + col + "}" + who + ": " + txt + "{/c}"));
		};
		auto moveTo = [&](int room) {
			LocationVector p(rooms[room][0], rooms[room][1], rooms[room][2]);
			this->setPosition(p);
			auto posMsg = make_shared<PositionStateMsg>(m_goId);
			m_parent.QueueState(posMsg);
			sGame.AnnounceStateUpdate(&m_parent, posMsg);
			m_tutStep = (uint8)room;
		};
		if (iequals(command, "tutorial"))
		{
			TutorialIntro();
			return;
		}
		if (m_tutStep == 0)
		{
			if (iequals(command, "blue"))
			{
				say("Niobe", "6699FF", "Then the story ends. You wake up in your bed and believe whatever you want to believe.");
				say("Niobe", "6699FF", "...But you are still here. Some part of you knows the choice is real. Choose again: &red or &blue.");
				return;
			}
			if (!iequals(command, "red"))
			{
				say("Niobe", "6699FF", "First you have to choose. Type &red to take the red pill, &blue to take the blue pill.");
				return;
			}
			say("Niobe", "6699FF", "Remember: all I am offering is the truth. Nothing more.");
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[Extraction] Your body is flushed from the power plant... You wake in the Construct.{/c}"));
			moveTo(1);
			say("Link", "00FF66", "Welcome to the real world, redpill. I'm Link, your operator. This is the Construct, our loading program.");
			say("Link", "00FF66", "First, get used to your RSI. Walk around a bit (W A S D, mouse to look). Type &continue when you're ready.");
			return;
		}
		if (!iequals(command, "continue"))
			return;
		switch (m_tutStep)
		{
		case 1:
		{
			double dx = m_pos.x - rooms[1][0], dz = m_pos.z - rooms[1][2];
			if (dx*dx + dz*dz < 200.0*200.0)
			{
				say("Link", "00FF66", "Move around a little first. Walk a few steps, then type &continue.");
				return;
			}
			moveTo(2);
			say("Link", "00FF66", "Good. This is Scarlet's office. In the Matrix you'll talk to contacts like her to get missions.");
			say("Link", "00FF66", "Select a person by clicking on them, then right-click to interact. Type &continue for combat training.");
			return;
		}
		case 2:
			moveTo(3);
			say("Link", "00FF66", "The Dojo. Close combat in the Matrix is Interlock: target an enemy and attack to lock into a fight.");
			say("Link", "00FF66", "Your fighting style and abilities decide each round. Blocks, throws and special moves beat each other. Type &continue.");
			return;
		case 3:
			moveTo(4);
			say("Link", "00FF66", "The Pistol Room. Free Fire is ranged combat: equip a gun, target, and fire from a distance.");
			say("Link", "00FF66", "Free Fire skips Interlock, but anyone you shoot can pull you into a close fight. Type &continue.");
			return;
		case 4:
			moveTo(5);
			say("Link", "00FF66", "This subway station has a hardline. Hardlines are how you jack in and out of the Matrix safely.");
			say("Link", "00FF66", "Level up your abilities from the Loading Area with your Upgrade points. Type &continue to use the hardline and enter the Matrix.");
			return;
		case 5:
		default:
		{
			say("Link", "00FF66", "You're ready. I'm sending you to Mara Central in Richland. Find Morpheus's people, and stay out of the Agents' way.");
			m_tutStep = 6;
			m_district = 1; // tutorial complete: saves write 1 instead of 255
			LocationVector mara(17043.1, 495.0, 2398.8); // Mara Central hardline area, Richland
			this->setPosition(mara);
			m_savedPos = mara;
			this->saveDataToDB();
			INFO_LOG(format("Character %1% completed the tutorial: moved to Mara Central") % m_handle);
			// Switching worlds inside a session desyncs object views (client rebuilds the world,
			// server keeps the old views). Retail returned you to the Loading Area after the
			// tutorial, so jack out through the hardline; the next jack-in loads Richland.
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[Hardline] Exiting the Construct. Jack in from the Loading Area to enter the Matrix.{/c}"));
			m_parent.QueueState(make_shared<JackoutEffectMsg>(m_goId));
			cancelEvents(EVENT_JACKOUT);
			this->addEvent(EVENT_JACKOUT, boost::bind(&PlayerObject::jackoutEvent, this), 4.0f);
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
		bool isFirearm = false;
		auto weapons = getEquippedWeapons();
		if (!weapons.empty())
		{
			const ItemTemplate* wpnTpl = sDataLoader.GetItemTemplate(weapons[0]->getTemplateId());
			if (wpnTpl && wpnTpl->type == ITEM_TYPE_WEAPON && (wpnTpl->range > 500.0f || wpnTpl->maxAmmo > 0))
				isFirearm = true;
		}

		if (isFirearm && !iequals(command, "interlock"))
		{
			if (!sCombatSys.RequestRangedCombat(m_goId, m_targetGoId, 0))
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Ranged combat request failed.{/c}"));
			}
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
	else if (iequals(command, "shoot") || iequals(command, "fire"))
	{
		if (m_targetGoId == 0)
		{
			float bestDistSq = 3500.0f * 3500.0f;
			for (uint32 gid : sObjMgr.getAllGOIds())
			{
				PlayerObject* po = sObjMgr.getGOPtrSafe(gid);
				if (po && po != this && !po->isDead())
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
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}No valid target to shoot.{/c}"));
			return;
		}

		if (!sCombatSys.RequestRangedCombat(m_goId, m_targetGoId, 0))
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Ranged combat request failed.{/c}"));
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
	else if (iequals(command, "style"))
	{
		string styleStr;
		cmdStream >> styleStr;
		if (iequals(styleStr, "kungfu") || iequals(styleStr, "wushu"))
		{
			setFightingStyle(FightingStyle::KungFu);
			saveDataToDB();
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Kung Fu (Wushu).{/c}"));
		}
		else if (iequals(styleStr, "karate"))
		{
			setFightingStyle(FightingStyle::Karate);
			saveDataToDB();
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Karate.{/c}"));
		}
		else if (iequals(styleStr, "aikido"))
		{
			setFightingStyle(FightingStyle::Aikido);
			saveDataToDB();
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[MARTIAL ARTS] Fighting style set to Aikido.{/c}"));
		}
		else if (iequals(styleStr, "street") || iequals(styleStr, "selfdefense") || iequals(styleStr, "brawl"))
		{
			setFightingStyle(FightingStyle::None);
			saveDataToDB();
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
			std::string abName = t ? t->name : "Ability";
			if (abId == 133) abName = "Kung Fu";
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[ABILITY] %1% loaded into memory (ID %2%, slot %3%).{/c}")
					% abName % abId % slot).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &loadability <abilityId> [slot]{/c}"));
		}
		return;
	}
	else if (iequals(command, "cast") || iequals(command, "useability"))
	{
		uint16 abId = 0;
		cmdStream >> abId;
		if (abId == 0 && m_abilitySystem)
		{
			for (const auto& pair : m_abilitySystem->getLoadedAbilities())
			{
				abId = pair.first;
				break;
			}
		}
		if (abId == 0)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &cast <abilityId>{/c}"));
			return;
		}

		uint32 targetGoId = m_targetGoId;
		if (targetGoId == 0 && m_ilPartner != 0) targetGoId = m_ilPartner;
		if (targetGoId == 0)
		{
			auto nearby = sSpatialGrid.GetClientsInRadius(m_pos.x, m_pos.z, 2500.0f);
			for (GameClient* gc : nearby)
			{
				if (gc && gc->GetPlayerGoId() != 0 && gc->GetPlayerGoId() != m_goId)
				{
					targetGoId = gc->GetPlayerGoId();
					break;
				}
			}
		}

		ByteBuffer dummy;
		dummy << uint16(abId);
		uint16 targetViewId = (targetGoId != 0) ? sObjMgr.getViewForGO(&m_parent, targetGoId) : uint16(0);
		dummy << targetViewId;
		dummy.rpos(0);
		RPC_HandleAbilityUse(dummy);
		return;
	}
	else if (iequals(command, "equip") || iequals(command, "wield"))
	{
		string arg;
		cmdStream >> arg;
		if (arg.empty())
		{
			if (m_equippedWeaponId != 0)
			{
				const Item* it = m_inventorySystem ? m_inventorySystem->getItemByGoId(m_equippedWeaponId).get() : nullptr;
				const ItemTemplate* tpl = it ? sDataLoader.GetItemTemplate(it->getTemplateId()) : nullptr;
				std::string wName = tpl ? tpl->name : "Weapon";
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}Currently equipped: [%1%] (GOID %2%, Ammo: %3%){/c}")
						% wName % m_equippedWeaponId % (it ? it->getAmmoCount() : 0)).str()));
			}
			else
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}No weapon currently equipped. Use &equip <slot|pistol|name>{/c}"));
			}
			return;
		}

		shared_ptr<Item> targetItem = nullptr;
		char* endp = nullptr;
		long slotNum = std::strtol(arg.c_str(), &endp, 10);
		if (endp != arg.c_str() && *endp == '\0' && slotNum >= 1 && slotNum <= 255)
		{
			targetItem = m_inventorySystem ? m_inventorySystem->getItemBySlot((uint8)slotNum) : nullptr;
		}

		if (!targetItem && m_inventorySystem)
		{
			for (auto item : m_inventorySystem->getAllItems())
			{
				if (!item) continue;
				const ItemTemplate* tpl = sDataLoader.GetItemTemplate(item->getTemplateId());
				if (!tpl) continue;
				if (iequals(arg, "pistol") || iequals(arg, "gun") || iequals(arg, "beretta"))
				{
					if (tpl->type == ITEM_TYPE_WEAPON && (tpl->range > 500.0f || tpl->maxAmmo > 0))
					{
						targetItem = item;
						break;
					}
				}
				else if (tpl->name.find(arg) != std::string::npos || std::to_string(tpl->templateId) == arg)
				{
					targetItem = item;
					break;
				}
			}
		}

		if (targetItem)
		{
			setEquippedWeaponId(targetItem->getGoId());
			saveDataToDB();
			if (m_inventorySystem) m_inventorySystem->saveToDB();
			const ItemTemplate* tpl = sDataLoader.GetItemTemplate(targetItem->getTemplateId());
			std::string wName = tpl ? tpl->name : "Weapon";
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[EQUIP] Equipped [%1%] in primary weapon slot (Ammo: %2%).{/c}")
					% wName % targetItem->getAmmoCount()).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:FF0000}[EQUIP] Item '%1%' not found in inventory.{/c}") % arg).str()));
		}
		return;
	}
	else if (iequals(command, "withdraw") || iequals(command, "escape"))
	{
		sCombatSys.StopFreeFire(m_goId);
		// same path as the client Withdraw button (RPC 0x44): contested under CR2, instant otherwise
		if (sCombatSys.IsInterlocked(m_goId)) { sCombatSys.RequestWithdraw(m_goId); return; }
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

		if (!SetRsiDyeColor(slotStr, colorVal))
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[TAILOR] Unknown wardrobe slot. Valid: coat, shirt, pants, shoes, glasses, hair, skin.{/c}"));
			return;
		}

		INFO_LOG(format("Tailor: %1% dyed %2% to color %3%") % m_handle % slotStr % colorVal);
		m_parent.QueueCommand(make_shared<SystemChatMsg>(
			(format("{c:00FF00}[TAILOR] Your %1% has been dyed to palette color #%2%! Appearance matrix synchronized.{/c}")
				% slotStr % colorVal).str()
		));
		return;
	}
	else if (iequals(command, "wear") || iequals(command, "wardrobe"))
	{
		string firstArg;
		cmdStream >> firstArg;
		if (firstArg.empty())
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				"{c:00FFCC}Usage: &wear <coat|shirt|pants|shoes|glasses|gloves|hat> <styleId> [colorId] | &wear <itemIdOrSlot>{/c}"));
			return;
		}

		if (isdigit((unsigned char)firstArg[0]))
		{
			uint32 idOrSlot = static_cast<uint32>(std::stoul(firstArg));
			uint8 slotHint = (idOrSlot <= 64) ? static_cast<uint8>(idOrSlot) : 0;
			if (MountClothingItem(idOrSlot, slotHint))
			{
				INFO_LOG(format("Tailor: %1% mounted clothing item %2% (Evasion=%3%, Hacking=+%4%)")
					% m_handle % idOrSlot % getEvasion() % getBonusHacking());
				m_parent.QueueCommand(make_shared<SystemChatMsg>(
					(format("{c:00FF00}[WARDROBE] Equipped clothing item #%1%! (Evasion: %2%, Hacking: +%3%){/c}")
						% idOrSlot % getEvasion() % getBonusHacking()).str()));
			}
			else
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[WARDROBE] Could not mount specified item.{/c}"));
			}
			return;
		}

		uint32 styleVal = 1;
		int colorVal = -1;
		cmdStream >> styleVal;
		if (!(cmdStream >> colorVal)) colorVal = -1;

		if (SetRsiWardrobe(firstArg, static_cast<uint8>(styleVal), colorVal))
		{
			INFO_LOG(format("Tailor: %1% equipped wardrobe %2% style %3% color %4%") % m_handle % firstArg % styleVal % colorVal);
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FF00}[WARDROBE] Equipped %1% style #%2%! Appearance matrix synchronized.{/c}")
					% firstArg % styleVal).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				"{c:FF0000}[WARDROBE] Unknown slot. Valid: coat, shirt, pants, shoes, glasses, gloves, hat, hair.{/c}"));
		}
		return;
	}
	else if (iequals(command, "capture") || iequals(command, "hacknode"))
	{
		if (!sConfig.GetBoolDefault("World.FactionWar", false))
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("Hardline capture is not part of the retail game (World.FactionWar is off)."));
			return;
		}
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

			auto bot = sBotMgr.SpawnSingleBot((float)bx, (float)pos.y, (float)bz, FACTION_MACHINES, false);
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
			botPo->m_despawnOnDeath = true;
			botPo->m_dojoOwnerGoId = m_goId;

			if (botName.find("Agent") != std::string::npos) {
				botPo->setAgentAppearance();
			} else {
				botPo->setMartialArtistAppearance(botStyle);
			}
			botPo->SpawnSelf();

			noteEntitySpawned(botGoId);
			setTargetGoId(botGoId);
			m_lastDojoBotGoId = botGoId;
			INFO_LOG(format("(%1%) %2%:%3% selected dynamic object view id 0001 objType 0001 (targetGoId=%4%)")
				% m_parent.Address() % m_handle % m_goId % botGoId);
			auto pkts = botPo->getCurrentStatePackets();
			for (const auto& pkt : pkts) {
				m_parent.QueueState(pkt);
			}
			m_parent.QueueState(std::make_shared<PlayerAppearanceMsg>(botGoId));
			m_parent.QueueState(std::make_shared<PositionStateMsg>(botGoId));
			m_parent.QueueState(std::make_shared<RotationStateMsg>(botGoId, botPos.getMxoRot()));
			m_parent.QueueState(std::make_shared<LocomotionStateMsg>(botGoId, 0, botPos.getMxoRot()));
			m_parent.QueueState(std::make_shared<CombatantModeMsg>(botGoId, 1));
			sGame.AnnounceStateUpdateNear((float)bx, (float)bz, 20000.0f, std::make_shared<PlayerAppearanceMsg>(botGoId));
			sGame.AnnounceStateUpdateNear((float)bx, (float)bz, 20000.0f, std::make_shared<PositionStateMsg>(botGoId));
			sGame.AnnounceStateUpdateNear((float)bx, (float)bz, 20000.0f, std::make_shared<RotationStateMsg>(botGoId, botPos.getMxoRot()));
			sGame.AnnounceStateUpdateNear((float)bx, (float)bz, 20000.0f, std::make_shared<LocomotionStateMsg>(botGoId, 0, botPos.getMxoRot()));
			sGame.AnnounceStateUpdateNear((float)bx, (float)bz, 20000.0f, std::make_shared<CombatantModeMsg>(botGoId, 1));
			INFO_LOG(format("Dojo bot %1% spawned for %2%: dist %3% units, pos (%4%, %5%, %6%) player (%7%, %8%, %9%) bot rot %10% lvl %11% hp %12% dmgScale %13%")
				% botGoId % m_handle % (sqrt((pos.x - bx) * (pos.x - bx) + (pos.z - bz) * (pos.z - bz)))
				% bx % pos.y % bz % pos.x % pos.y % pos.z % botPos.rot % int(botLvl) % hpF % dmgScale);
			return true;
		};

		FightingStyle targetStyle = FightingStyle::None;
		std::string styleLabel = "Street Brawler";
		bool is1v1 = false;
		float optHp = 0.0f;

		if (iequals(subCommand, "agent") || iequals(subCommand, "smith")) {
			targetStyle = FightingStyle::KungFu;
			styleLabel = iequals(subCommand, "smith") ? "Agent Smith" : "Agent Johnson";
			is1v1 = true;
			cmdStream >> optHp;
		} else if (iequals(subCommand, "kungfu") || iequals(subCommand, "wushu")) {
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
				if (iequals(optStyle, "agent") || iequals(optStyle, "smith")) {
					targetStyle = FightingStyle::KungFu;
					styleLabel = iequals(optStyle, "smith") ? "Agent Smith" : "Agent Johnson";
				} else if (iequals(optStyle, "kungfu") || iequals(optStyle, "wushu")) {
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
				int s = (s_cycleStyle++) % 5;
				if (s == 0) { targetStyle = FightingStyle::KungFu; styleLabel = "Agent Johnson"; }
				else if (s == 1) { targetStyle = FightingStyle::KungFu; styleLabel = "Kung Fu Master"; }
				else if (s == 2) { targetStyle = FightingStyle::Karate; styleLabel = "Karate Master"; }
				else if (s == 3) { targetStyle = FightingStyle::Aikido; styleLabel = "Aikido Sensei"; }
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
		else if (iequals(subCommand, "ranged") || iequals(subCommand, "range") || iequals(subCommand, "freefire"))
		{
			float rangedHp = 100.0f;
			cmdStream >> rangedHp;
			if (rangedHp <= 0.0f) rangedHp = 100.0f;
			bool ok = spawnDojoBot(6.0f, 0.0f, FightingStyle::KungFu, "Dojo Range Target", rangedHp);
			if (ok && m_lastDojoBotGoId) {
				sCombatSys.RequestRangedCombat(m_goId, m_lastDojoBotGoId);
			}
			m_parent.QueueCommand(make_shared<SystemChatMsg>(ok
				? "{c:00FF00}Dojo: Ranged target spawned at 6m and engaged in Free-Fire combat!{/c}"
				: "{c:FF0000}Dojo: could not spawn a ranged target.{/c}"));
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
			std::vector<uint32> toDestroy;
			auto goIds = sObjMgr.getAllGOIds();
			for (uint32 id : goIds) {
				PlayerObject* p = sObjMgr.getGOPtrSafe(id);
				if (p && p != this && p->getClient().isBot()) {
					if (p->getPosition().DistanceSq(pos) < 5000.0f * 5000.0f) {
						if (!p->isDead()) {
							p->die(0); // killerGoId 0 = no reward; announces death + cleans combat state
						}
						toDestroy.push_back(id);
						killed++;
					}
				}
			}
			for (uint32 id : toDestroy) {
				sCombatSys.RemoveCombatant(id);
				sObjMgr.destroyObject(id);
			}
			if (!toDestroy.empty() && BotManager::getSingletonPtr()) {
				sBotMgr.PruneDeadBots();
			}
			m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}Dojo: Cleared and despawned %1% bots.{/c}") % killed).str()));
		}
		else if (iequals(subCommand, "leave") || iequals(subCommand, "dismiss"))
		{
			// Dismiss only the sparring partners this player summoned (dead or alive).
			std::vector<uint32> mine;
			for (uint32 id : sObjMgr.getAllGOIds()) {
				PlayerObject* p = sObjMgr.getGOPtrSafe(id);
				if (p && p != this && p->getClient().isBot() && p->m_dojoOwnerGoId == m_goId)
					mine.push_back(id);
			}
			for (uint32 id : mine) {
				sCombatSys.RemoveCombatant(id);
				sBotMgr.DespawnBot(id);
			}
			if (std::find(mine.begin(), mine.end(), m_lastDojoBotGoId) != mine.end())
				m_lastDojoBotGoId = 0;
			INFO_LOG(format("Dojo leave: %1% dismissed %2% sparring partner(s)") % m_handle % mine.size());
			m_parent.QueueCommand(make_shared<SystemChatMsg>(mine.empty()
				? std::string("{c:FFFF00}Dojo: you have no sparring partners to dismiss.{/c}")
				: (format("{c:00FF00}Dojo: %1% sparring partner(s) left.{/c}") % mine.size()).str()));
		}
		else
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}Usage: &dojo <1v1 [style] [hp] | group | leave | clear>{/c}"));
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

		LocationVector pos = this->getPosition();
		if (targetCivId == 0 && BotManager::getSingletonPtr() == nullptr) {
			float bestDistSq = 1200.0f * 1200.0f;
			for (uint32 gid : sObjMgr.getAllGOIds()) {
				PlayerObject* po = sObjMgr.getGOPtrSafe(gid);
				if (po && po != this && !po->isDead() && po->m_dojoOwnerGoId == 0 &&
				    (po->getClient().isBot() || po->getFactionName() == "Civilian")) {
					float d = (float)pos.DistanceSq(po->getPosition());
					if (d < bestDistSq) {
						bestDistSq = d;
						targetCivId = gid;
					}
				}
			}
		}

		bool spawnedDedicatedHost = false;
		if ((targetCivId == 0 || !sObjMgr.getGOPtrSafe(targetCivId)) && BotManager::getSingletonPtr() != nullptr) {
			// Spawn a dedicated civilian host 4m in front of player at authentic pavement level (autoSpawnSelf = false
			// so ObjectCreateMsg 0x02 is sent after PossessCivilian sets the Agent handle and appearance)
			const LocationVector botPos = DojoPlaceInFront(pos, 4.0f, 0.0f);
			auto bot = sBotMgr.SpawnSingleBot((float)botPos.x, (float)pos.y, (float)botPos.z, FACTION_NONE, false);
			if (bot) {
				targetCivId = bot->GetPlayerGoId();
				if (auto po = sObjMgr.getGOPtrSafe(targetCivId)) {
					po->setHandle("Thomas Anderson");
					po->setFactionName("Civilian");
					po->setPosition(botPos);
					po->m_despawnOnDeath = true;
					po->m_dojoOwnerGoId = m_goId;
					spawnedDedicatedHost = true;
				}
			}
		}

		if (targetCivId != 0) {
			uint32 agentId = sAgentPossessionMgr.PossessCivilian(targetCivId, agentName, m_goId, isSmith);
			if (agentId != 0) {
				if (spawnedDedicatedHost) {
					if (auto agentPo = sObjMgr.getGOPtrSafe(targetCivId)) {
						agentPo->SpawnSelf();
					}
					noteEntitySpawned(targetCivId);
				}
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
					m_parent.QueueState(make_shared<PlayerAppearanceMsg>(targetCivId));
					m_parent.QueueState(make_shared<PositionStateMsg>(targetCivId));
					m_parent.QueueState(make_shared<RotationStateMsg>(targetCivId, agentPo->getPosition().getMxoRot()));
					m_parent.QueueState(make_shared<LocomotionStateMsg>(targetCivId, 0, agentPo->getPosition().getMxoRot()));
					m_parent.QueueState(make_shared<CombatantModeMsg>(targetCivId, 1));
					sGame.AnnounceStateUpdate(&agentPo->getClient(), make_shared<PlayerAppearanceMsg>(targetCivId));
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
        if (!sConfig.GetBoolDefault("World.FactionWar", false))
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("Hardline claiming is not part of the retail game (World.FactionWar is off)."));
            return;
        }
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
			if (m_inventorySystem) m_inventorySystem->saveToDB();
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

		auto posMsg = make_shared<PositionStateMsg>(m_goId);
		m_parent.QueueState(posMsg);
		sGame.AnnounceStateUpdate(&m_parent, posMsg);
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
		auto posMsg = make_shared<PositionStateMsg>(m_goId);
		m_parent.QueueState(posMsg, true);
		sGame.AnnounceStateUpdate(&m_parent, posMsg, true);
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
		if (Database_Main != nullptr && m_characterUID < 9000000)
			loadFromDB(false);
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
		auto posMsg = make_shared<PositionStateMsg>(m_goId);
		m_parent.QueueState(posMsg);
		sGame.AnnounceStateUpdate(&m_parent, posMsg);
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

			auto posMsg = make_shared<PositionStateMsg>(m_goId);
			m_parent.QueueState(posMsg);
			sGame.AnnounceStateUpdate(&m_parent, posMsg);
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
    else if (iequals(command, "org") || iequals(command, "crew") ||
             iequals(command, "orgcreate") || iequals(command, "orginvite") || iequals(command, "orgleave"))
    {
        string sub;
        if (iequals(command, "orgcreate")) sub = "create";
        else if (iequals(command, "orginvite")) sub = "invite";
        else if (iequals(command, "orgleave")) sub = "leave";
        else cmdStream >> sub;

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
            uint32 newOrgId = sOrgMgr.createOrganization(orgName, getCharId());
            m_orgId = newOrgId;
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}[ORGANIZATION] '%1%' created successfully (Org ID: %2%).{/c}") % orgName % newOrgId).str()));
            INFO_LOG(format("Organization: Player %1% created organization '%2%' (ID %3%)") % m_handle % orgName % newOrgId);
            return;
        }
        else if (iequals(sub, "invite"))
        {
            string targetName;
            cmdStream >> targetName;
            if (targetName.empty()) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /org invite <targetName>{/c}"));
                return;
            }
            if (m_orgId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
                return;
            }
            Organization* org = sOrgMgr.getOrganization(m_orgId);
            if (!org || org->leaderId != getCharId()) {
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
                    if (sOrgMgr.joinOrganization(m_orgId, p->getCharId())) {
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
        else if (iequals(sub, "leave"))
        {
            if (m_orgId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
                return;
            }
            if (sOrgMgr.leaveOrganization(m_orgId, getCharId())) {
                m_orgId = 0;
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[ORGANIZATION] You have left the Organization.{/c}"));
                INFO_LOG(format("Organization: Player %1% left organization") % m_handle);
            }
            return;
        }
        else if (sub.empty() || iequals(sub, "info") || iequals(sub, "status"))
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
        else
        {
            if (m_orgId == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You are not in an Organization.{/c}"));
                return;
            }
            string restMsg;
            std::getline(cmdStream, restMsg);
            string orgMsg = sub + restMsg;
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
    }
    else if (iequals(command, "mission") || iequals(command, "contract"))
    {
        string sub;
        cmdStream >> sub;
        if (iequals(sub, "abort"))
        {
            if (sMissionSys.AbortMission(this, "player slash abort"))
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}[OPERATOR] Mission aborted by operator request.{/c}"));
                INFO_LOG(format("MissionSystem: Player %1% aborted active mission contract") % m_handle);
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[OPERATOR] No active mission to abort.{/c}"));
            }
            return;
        }
        else if (iequals(sub, "info") || iequals(sub, "status"))
        {
            if (sMissionSys.HasActiveMission(m_goId))
            {
                sMissionSys.SendMissionObjectiveDialog(this);
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[OPERATOR] No active mission contract.{/c}"));
            }
            return;
        }
        else
        {
            ByteBuffer dummy;
            RPC_HandleMissionRequest(dummy);
            return;
        }
    }
    else if (iequals(command, "duel") || iequals(command, "spar"))
    {
        string sub;
        cmdStream >> sub;
        if (iequals(sub, "accept"))
        {
            uint32 challengerId = getDuelChallenger();
            PlayerObject* challenger = sObjMgr.getGOPtrSafe(challengerId);
            if (!challenger || challengerId == 0)
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[DUEL] You have no pending duel challenges.{/c}"));
                return;
            }
            if (isDueling() || challenger->isDueling())
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[DUEL] A participant is already in an active duel.{/c}"));
                return;
            }
            setDuelPartner(challengerId);
            challenger->setDuelPartner(m_goId);
            setDuelChallenger(0);
            challenger->setDuelChallenger(0);

            sGame.AnnounceCommand(&m_parent, make_shared<SystemChatMsg>(
                (format("{c:00FF00}[DUEL] Sparring duel initiated between %1% and %2%! Combat engagement rules active.{/c}") % challenger->getHandle() % m_handle).str()
            ));
            INFO_LOG(format("Duel: Sparring duel started between %1% and %2%") % challenger->getHandle() % m_handle);
            return;
        }
        else if (iequals(sub, "decline"))
        {
            uint32 challengerId = getDuelChallenger();
            PlayerObject* challenger = sObjMgr.getGOPtrSafe(challengerId);
            setDuelChallenger(0);
            if (challenger)
            {
                challenger->m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:FFFF00}[DUEL] %1% declined your duel challenge.{/c}") % m_handle).str()
                ));
            }
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[DUEL] Duel challenge declined.{/c}"));
            return;
        }
        else
        {
            PlayerObject* target = nullptr;
            if (!sub.empty())
            {
                for (uint32 gId : sObjMgr.getAllGOIds())
                {
                    PlayerObject* p = sObjMgr.getGOPtrSafe(gId);
                    if (p && iequals(p->getHandle(), sub))
                    {
                        target = p;
                        break;
                    }
                }
            }
            if (!target && m_targetGoId != 0)
            {
                target = sObjMgr.getGOPtrSafe(m_targetGoId);
            }
            if (!target || target == this)
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: /duel <targetName> | /duel accept | /duel decline{/c}"));
                return;
            }
            ByteBuffer dummy;
            dummy << (uint16)0;
            m_targetGoId = target->getGoId();
            RPC_HandleDuelRequest(dummy);
            return;
        }
    }
    else if (iequals(command, "hardline") || iequals(command, "hardlines"))
    {
        string sub;
        cmdStream >> sub;
        if (iequals(sub, "list"))
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}--- HARDLINES IN DISTRICT %1% ---{/c}") % (int)m_district).str()));
            for (const auto& kv : GetHardlineDirectory()) {
                if (kv.second.districtId == m_district) {
                    std::stringstream ss;
                    ss << "  Node #" << (int)kv.second.hardlineId << ": " << kv.second.name
                       << " (" << (int)kv.second.x << ", " << (int)kv.second.y << ", " << (int)kv.second.z << ")";
                    m_parent.QueueCommand(make_shared<SystemChatMsg>(ss.str()));
                }
            }
            return;
        }
        else if (iequals(sub, "tp") || iequals(sub, "jump"))
        {
            uint32 destDistrict = (uint32)m_district;
            uint32 destHL = 1;
            cmdStream >> destDistrict >> destHL;
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
        else
        {
            const HardlineNode* nearest = GetNearestHardline(m_district, getPosition().x, getPosition().z);
            size_t total = GetTotalHardlines();
            if (nearest) {
                double dx = getPosition().x - nearest->x;
                double dz = getPosition().z - nearest->z;
                double range = sqrt(dx*dx + dz*dz) / 100.0;
                INFO_LOG(format("HardlineDirectory: Player %1% queried nearest hardline %2%") % m_handle % nearest->name);
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFCC}[HARDLINE] Nearest: %1% (District %2%, #%3%) - Range: %4$.1fm | Total Matrix Hardlines: %5%{/c}")
                     % nearest->name % (int)nearest->districtId % (int)nearest->hardlineId % range % total).str()
                ));
            } else {
                INFO_LOG(format("HardlineDirectory: Player %1% queried total hardlines (%2%)") % m_handle % total);
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}[HARDLINE] Total Matrix Hardlines: %1%{/c}") % total).str()));
            }
            return;
        }
    }
    else if (iequals(command, "tension") || iequals(command, "outbreak"))
    {
        string sub;
        cmdStream >> sub;
        uint32 hDistrict = sFactionWarMgr.GetHighestTensionDistrict();
        float hTension = sFactionWarMgr.GetDistrictTension(hDistrict);
        std::string hName = sFactionWarMgr.GetDistrictName(hDistrict);

        if (sub.empty() || iequals(sub, "status") || iequals(sub, "list"))
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}=== DISTRICT FACTION TENSION & CONFLICT HOTSPOTS ==={/c}"));
            for (uint32 dId = 1; dId <= 4; ++dId)
            {
                float t = sFactionWarMgr.GetDistrictTension(dId);
                std::string name = sFactionWarMgr.GetDistrictName(dId);
                std::string status = (t >= 75.0f) ? "{c:FF0000}CRITICAL OUTBREAK{/c}" : (t >= 50.0f ? "{c:FFAA00}ELEVATED CONFLICT{/c}" : "{c:00FF00}STABLE{/c}");
                std::ostringstream line;
                line << "  District " << dId << " [" << name << "]: Tension " << static_cast<int>(t) << "% | Status: " << status;
                m_parent.QueueCommand(make_shared<SystemChatMsg>(line.str()));
            }
            int hPct = static_cast<int>(hTension);
            std::ostringstream hotspot;
            hotspot << "{c:FFD700}[HOTSPOT] Highest Tension: " << hName << " (District " << hDistrict << ") at " << hPct << "% contested.{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(hotspot.str()));
            INFO_LOG((format("DistrictTension: Player %1% queried district tension metrics (hotspot: %2% %3% percent)") % m_handle % hName % hPct).str());
            return;
        }
        else if (iequals(sub, "surge"))
        {
            uint32 targetDist = hDistrict;
            float surgeAmt = 35.0f;
            cmdStream >> targetDist >> surgeAmt;
            sMatrixThreatHeatmap.RecordDisruption((float)getPosition().x, (float)getPosition().y, (float)getPosition().z, surgeAmt, "Faction Tension Surge");
            std::ostringstream surgeMsg;
            surgeMsg << "{c:FF0000}[TENSION SURGE] Injected +" << static_cast<int>(surgeAmt) << " threat heat disruption into " << sFactionWarMgr.GetDistrictName(targetDist) << ". Escalation imminent!{/c}";
            m_parent.QueueCommand(make_shared<SystemChatMsg>(surgeMsg.str()));
            INFO_LOG((format("DistrictTension: Player %1% triggered tension surge of %2% in district %3%") % m_handle % static_cast<int>(surgeAmt) % targetDist).str());
            return;
        }
        else
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: /tension [status|surge <districtId> <amount>]{/c}"));
            return;
        }
    }
    else if (iequals(command, "contact") || iequals(command, "contacts"))
    {
        string sub;
        cmdStream >> sub;
        if (sub.empty() || iequals(sub, "list"))
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}=== ICONIC CONTACT STORYLINE QUESTS ==={/c}"));
            const auto& quests = sMissionSys.GetAllContactQuests();
            for (const auto& q : quests)
            {
                uint32 prog = sMissionSys.GetPlayerContactProgress(getCharacterUID(), q.questId);
                std::string status = (prog >= q.objectives.size()) ? "{c:00FF00}[COMPLETED]{/c}" : 
                                     (prog > 0 ? (format("{c:FFFF00}[STEP %1%/%2%]{/c}") % prog % q.objectives.size()).str() : "{c:AAAAAA}[AVAILABLE]{/c}");
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("  Quest #%1%: %2% ({c:00FFFF}%3%{/c}) - %4%") % q.questId % q.title % q.contactName % status).str()
                ));
            }
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFD700}Commands: /contact call <name>, /contact accept <id>, /contact complete <id>{/c}"));
            INFO_LOG(format("ContactQuest: Player %1% listed contact storyline quests (total: %2%)") % m_handle % quests.size());
            return;
        }
        else if (iequals(sub, "call"))
        {
            string contactTarget;
            cmdStream >> contactTarget;
            uint32 cId = 0;
            if (iequals(contactTarget, "morpheus")) cId = CONTACT_MORPHEUS;
            else if (iequals(contactTarget, "ghost")) cId = CONTACT_GHOST;
            else if (iequals(contactTarget, "trinity")) cId = CONTACT_TRINITY;
            else if (iequals(contactTarget, "niobe")) cId = CONTACT_NIOBE;
            else if (iequals(contactTarget, "merovingian") || iequals(contactTarget, "mero")) cId = CONTACT_MEROVINGIAN;
            else if (iequals(contactTarget, "oracle")) cId = CONTACT_ORACLE;
            else if (iequals(contactTarget, "gray") || iequals(contactTarget, "agentgray")) cId = CONTACT_AGENT_GRAY;
            else if (iequals(contactTarget, "architect")) cId = CONTACT_ARCHITECT;
            else if (iequals(contactTarget, "seraph")) cId = CONTACT_SERAPH;
            else {
                try { cId = std::stoul(contactTarget); } catch (...) { cId = 0; }
            }

            ByteBuffer callCmd;
            callCmd << (uint32)cId;
            RPC_HandleCallContact(callCmd);
            INFO_LOG(format("ContactQuest: Player %1% called contact %2% (ID: %3%)") % m_handle % contactTarget % cId);
            return;
        }
        else if (iequals(sub, "accept"))
        {
            uint32 qId = 1001;
            cmdStream >> qId;
            const ContactQuest* q = sMissionSys.GetContactQuest(qId);
            if (q)
            {
                sMissionSys.AcceptContactQuest(this, qId);
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[CONTACT QUEST ACCEPTED] '%1%' from %2%{/c}") % q->title % q->contactName).str()
                ));
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFCC}[GREETING] \"%1%\"{/c}") % q->dialogGreeting).str()
                ));
                if (!q->objectives.empty())
                {
                    m_parent.QueueCommand(make_shared<SystemChatMsg>(
                        (format("{c:FFD700}[OBJECTIVE 1] %1%{/c}") % q->objectives[0]).str()
                    ));
                }
                INFO_LOG(format("ContactQuest: Player %1% accepted contact quest %2% (%3%)") % m_handle % qId % q->title);
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[CONTACT] Quest ID not found. Use /contact list.{/c}"));
            }
            return;
        }
        else if (iequals(sub, "progress") || iequals(sub, "step"))
        {
            uint32 qId = 1001;
            uint32 step = 1;
            cmdStream >> qId >> step;
            if (sMissionSys.ProgressContactQuest(this, qId, step))
            {
                const ContactQuest* q = sMissionSys.GetContactQuest(qId);
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[CONTACT QUEST] Advanced '%1%' to step %2%.{/c}") % (q ? q->title : "Quest") % step).str()
                ));
                INFO_LOG(format("ContactQuest: Player %1% progressed contact quest %2% to step %3%") % m_handle % qId % step);
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[CONTACT] Failed to progress quest.{/c}"));
            }
            return;
        }
        else if (iequals(sub, "complete"))
        {
            uint32 qId = 1001;
            cmdStream >> qId;
            const ContactQuest* q = sMissionSys.GetContactQuest(qId);
            if (q && sMissionSys.CompleteContactQuest(this, qId))
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[CONTACT QUEST COMPLETED] '%1%'! Rewards: %2% $Info | %3% XP{/c}")
                        % q->title % q->rewardInfo % q->rewardExp).str()
                ));
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFCC}[COMPLETION] \"%1%\"{/c}") % q->dialogCompletion).str()
                ));
                INFO_LOG(format("ContactQuest: Player %1% completed contact quest %2% (%3%)") % m_handle % qId % q->title);
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}[CONTACT] Failed to complete quest. Check ID or status.{/c}"));
            }
            return;
        }
        else
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: /contact [list|call <name>|accept <id>|progress <id> <step>|complete <id>]{/c}"));
            return;
        }
    }
    else if (iequals(command, "bugs"))
    {
        string filterArg;
        cmdStream >> filterArg;
        uint32 limit = 10;
        string statusFilter = "";
        if (!filterArg.empty())
        {
            if (iequals(filterArg, "open") || iequals(filterArg, "resolved") || iequals(filterArg, "closed") || iequals(filterArg, "all"))
            {
                statusFilter = filterArg;
            }
            else
            {
                try { limit = static_cast<uint32>(std::stoul(filterArg)); } catch (...) { limit = 10; }
            }
        }
        auto reports = PlayerObject::GetBugReports(limit, statusFilter);
        if (reports.empty())
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[BUG TRACKER] No matching bug reports in database. Submit one with &bug <description> or /bug <description>.{/c}"));
            return;
        }
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}=== SERVER BUG REPORTS (%1% shown) ==={/c}") % reports.size()).str()));
        for (const auto& r : reports)
        {
            std::string dName = (r.district == 1) ? "Richland" : ((r.district == 2) ? "Downtown" : ((r.district == 3) ? "International" : "The Slums"));
            std::string noteSuffix = r.adminNotes.empty() ? "" : (format(" {c:AAAAAA}[Note: %1%]{/c}") % r.adminNotes).str();
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FFFF}[#%1%]{/c} {c:FFFF55}[%2%|%3%]{/c} {c:00FF00}%4%{/c} (%5%): %6%%7%")
                    % r.id % r.status % r.category % r.handle % dName % r.description % noteSuffix).str()
            ));
        }
        return;
    }
    else if (iequals(command, "bug") || iequals(command, "report"))
    {
        string rest;
        std::getline(cmdStream, rest);
        boost::trim(rest);

        if (rest.empty() || iequals(rest, "help"))
        {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}=== IN-GAME BUG REPORT SYSTEM ==={/c}"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}&bug <description> | /bug <description>{/c} - Save a bug report with your live telemetry to the server database"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}&bugs [open|resolved|all] | /bugs{/c} - Check recent bug reports saved on the server"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}&bug view <id> | /bug view <id>{/c} - View full details & coordinates of bug report #<id>"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}&bug resolve <id> [notes]{/c} - Mark bug #<id> as RESOLVED in database"));
            return;
        }

        std::stringstream subStream(rest);
        string subCmd;
        subStream >> subCmd;

        if (iequals(subCmd, "list"))
        {
            string filterArg;
            subStream >> filterArg;
            uint32 limit = 10;
            string statusFilter = "";
            if (!filterArg.empty())
            {
                if (iequals(filterArg, "open") || iequals(filterArg, "resolved") || iequals(filterArg, "closed") || iequals(filterArg, "all"))
                    statusFilter = filterArg;
                else
                {
                    try { limit = static_cast<uint32>(std::stoul(filterArg)); } catch (...) { limit = 10; }
                }
            }
            auto reports = PlayerObject::GetBugReports(limit, statusFilter);
            if (reports.empty())
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}[BUG TRACKER] No matching bug reports in database.{/c}"));
                return;
            }
            m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FF00}=== SERVER BUG REPORTS (%1% shown) ==={/c}") % reports.size()).str()));
            for (const auto& r : reports)
            {
                std::string dName = (r.district == 1) ? "Richland" : ((r.district == 2) ? "Downtown" : ((r.district == 3) ? "International" : "The Slums"));
                std::string noteSuffix = r.adminNotes.empty() ? "" : (format(" {c:AAAAAA}[Note: %1%]{/c}") % r.adminNotes).str();
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFFF}[#%1%]{/c} {c:FFFF55}[%2%|%3%]{/c} {c:00FF00}%4%{/c} (%5%): %6%%7%")
                        % r.id % r.status % r.category % r.handle % dName % r.description % noteSuffix).str()
                ));
            }
            return;
        }
        else if (iequals(subCmd, "view") || iequals(subCmd, "check") || iequals(subCmd, "info") || iequals(subCmd, "show"))
        {
            uint32 reportId = 0;
            subStream >> reportId;
            if (reportId == 0)
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: &bug view <id> (or /bug view <id>){/c}"));
                return;
            }
            BugReportRecord rec;
            if (!PlayerObject::GetBugReportById(reportId, rec))
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:FF5555}[BUG TRACKER] Bug report #%1% not found in database.{/c}") % reportId).str()));
                return;
            }
            std::string dName = (rec.district == 1) ? "Richland" : ((rec.district == 2) ? "Downtown" : ((rec.district == 3) ? "International" : "The Slums"));
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FF00}=== BUG REPORT #%1% [%2%] ==={/c}") % rec.id % rec.status).str()
            ));
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FFFF}Reporter:{/c} %1% (CharID %2%, Lv.%3%, Style 0x%4$X, HP %5%/%6%)")
                    % rec.handle % rec.charId % (int)rec.level % rec.fightingStyle % rec.hp % rec.maxHp).str()
            ));
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FFFF}Location:{/c} %1% [District %2%] (X=%3$.1f, Y=%4$.1f, Z=%5$.1f, Rot=%6%) | TargetGoID=%7%")
                    % dName % rec.district % rec.x % rec.y % rec.z % (int)rec.rot % rec.targetGoId).str()
            ));
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FFFF}Category:{/c} %1% | {c:00FFFF}Logged:{/c} %2%") % rec.category % rec.createdAt).str()
            ));
            m_parent.QueueCommand(make_shared<SystemChatMsg>(
                (format("{c:00FFFF}Description:{/c} %1%") % rec.description).str()
            ));
            if (!rec.adminNotes.empty())
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:FFFF55}Notes:{/c} %1%") % rec.adminNotes).str()
                ));
            }
            return;
        }
        else if (iequals(subCmd, "resolve") || iequals(subCmd, "close") || iequals(subCmd, "reopen") || iequals(subCmd, "status"))
        {
            uint32 reportId = 0;
            subStream >> reportId;
            if (reportId == 0)
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: &bug resolve <id> [notes] | &bug status <id> <OPEN|RESOLVED|CLOSED> [notes]{/c}"));
                return;
            }
            string newStatus = "RESOLVED";
            if (iequals(subCmd, "close")) newStatus = "CLOSED";
            else if (iequals(subCmd, "reopen")) newStatus = "OPEN";
            else if (iequals(subCmd, "status"))
            {
                subStream >> newStatus;
                if (newStatus.empty()) newStatus = "RESOLVED";
            }
            string notes;
            std::getline(subStream, notes);
            boost::trim(notes);

            if (PlayerObject::UpdateBugReportStatus(reportId, newStatus, notes))
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FF00}[BUG TRACKER] Bug report #%1% updated to status [%2%] in server database.{/c}")
                        % reportId % boost::to_upper_copy(newStatus)).str()
                ));
            }
            else
            {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:FF5555}[BUG TRACKER] Failed to update bug report #%1% (check ID).{/c}") % reportId).str()
                ));
            }
            return;
        }

        SubmitBugReport(rest);
        return;
    }
    else if (iequals(command, "clock"))
    {
        float h = sPedestrianEcology.GetMatrixHour();
        int hour = (int)h;
        int minute = (int)((h - hour) * 60.0f);
        CircadianPeriod period = sPedestrianEcology.GetCircadianPeriod();
        std::string desc;
        switch (period) {
            case PERIOD_COMMUTE_MORNING: desc = "Morning Commute"; break;
            case PERIOD_WORK: desc = "Work Hours"; break;
            case PERIOD_COMMUTE_EVENING: desc = "Evening Commute"; break;
            case PERIOD_LEISURE: desc = "Evening Leisure"; break;
            case PERIOD_REST_NIGHT: desc = "Night Rest"; break;
        }
        std::string timeStr = (format("%02d:%02d") % hour % minute).str();
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:55FF55}[Matrix Clock] Time: %1% | Cycle: %2%{/c}") % timeStr % desc).str()
        ));
        return;
    }
    else if (iequals(command, "f") || iequals(command, "faction"))
    {
        string factionMsg;
        std::getline(cmdStream, factionMsg);
        boost::trim(factionMsg);
        if (factionMsg.empty()) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: /f <message> | /faction <message>{/c}"));
            return;
        }
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
    else if (iequals(command, "teleport") || iequals(command, "tp"))
    {
        uint32 destDistrict = 0;
        uint32 destHL = 0;
        if (cmdStream >> destDistrict >> destHL) {
            ByteBuffer tpCmd;
            tpCmd << (uint8)1; // source HL
            while (tpCmd.size() < 6) tpCmd << (uint8)0;
            tpCmd << (uint8)m_district;
            while (tpCmd.size() < 10) tpCmd << (uint8)0;
            tpCmd << (uint8)destHL;
            while (tpCmd.size() < 14) tpCmd << (uint8)0;
            tpCmd << (uint8)destDistrict;
            RPC_HandleHardlineTeleport(tpCmd);
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}Usage: /tp <districtId> <hardlineId>{/c}"));
        }
        return;
    }
    else if (iequals(command, "heat"))
    {
        float hx = getPosition().x;
        float hz = getPosition().z;
        float heat = sMatrixThreatHeatmap.GetHeat(hx, hz);
        HeatTier tier = sMatrixThreatHeatmap.GetTier(hx, hz);
        uint32 dId = sMatrixThreatHeatmap.GetDistrictAt(hx, hz);
        std::string dName = (dId == 1) ? "Richland" : ((dId == 2) ? "Downtown" : ((dId == 3) ? "International" : "The Slums"));
        bool sabotaged = sMatrixThreatHeatmap.IsDistrictSabotaged(dId);
        INFO_LOG(format("ThreatHeatmap: Player %1% queried heat in %2% (heat=%3$.1f, tier=%4%)") % m_handle % dName % heat % (int)tier);
        m_parent.QueueCommand(make_shared<SystemChatMsg>(
            (format("{c:FFFF55}[Heatmap] District: %1% (ID %2%) | Heat: %3$.1f | Heat Tier: %4% | Sabotaged: %5%{/c}")
             % dName % dId % heat % (int)tier % (sabotaged ? "YES" : "NO")).str()
        ));
        return;
    }
    else if (iequals(command, "district"))
    {
        std::string targetDistrict;
        cmdStream >> targetDistrict;
        boost::to_lower(targetDistrict);
        LocationVector targetPos(99640.0f, 572.0f, 8350.0f);
        std::string distName = "The Slums";
        uint32 newDistrictId = 1;

        if (targetDistrict == "dt" || targetDistrict == "downtown") {
            targetPos = LocationVector(39216.0f, 572.0f, -21475.0f);
            distName = "Downtown";
            newDistrictId = 2;
        } else if (targetDistrict == "it" || targetDistrict == "international") {
            targetPos = LocationVector(-37444.0f, 572.0f, 23659.0f);
            distName = "International";
            newDistrictId = 3;
        } else if (targetDistrict == "richland") {
            targetPos = LocationVector(-15000.0f, 572.0f, -15000.0f);
            distName = "Richland";
            newDistrictId = 4;
        } else if (targetDistrict == "slums") {
            targetPos = LocationVector(99640.0f, 572.0f, 8350.0f);
            distName = "The Slums";
            newDistrictId = 1;
        } else {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF5555}Unknown district. Valid: slums, dt, it, richland{/c}"));
            return;
        }

        this->setPosition(targetPos);
        this->setDistrict(newDistrictId);
        this->saveDataToDB();
        auto posMsg = make_shared<PositionStateMsg>(m_goId);
        m_parent.QueueState(posMsg);
        sGame.AnnounceStateUpdate(&m_parent, posMsg);
        this->FlushAndRefreshAoI();
        m_parent.QueueState(make_shared<JackoutEffectMsg>(m_goId, false));
        m_parent.QueueCommand(make_shared<SystemChatMsg>((format("{c:00FFCC}[Transit] Transferred to %1% hardline network (District %2%).{/c}") % distName % newDistrictId).str()));
        return;
    }
    else if (iequals(command, "vendor") || iequals(command, "shop"))
    {
        std::string sub;
        cmdStream >> sub;
        if (iequals(sub, "buy")) {
            uint32 tplId = 0;
            cmdStream >> tplId;
            if (tplId > 0) {
                ByteBuffer buyCmd;
                buyCmd << uint32(tplId) << uint32(0);
                RPC_HandleVendorBuy(buyCmd);
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /vendor buy <itemTemplateId>{/c}"));
            }
            return;
        }
        if (iequals(sub, "sell")) {
            uint32 itemId = 0;
            cmdStream >> itemId;
            if (itemId > 0) {
                ByteBuffer sellCmd;
                sellCmd << uint32(itemId) << uint32(0);
                RPC_HandleVendorSell(sellCmd);
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Usage: /vendor sell <itemId>{/c}"));
            }
            return;
        }

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
            INFO_LOG(format("Vendor: Player %1% opened vendor %2% (ID %3%)") % m_handle % nearest->name % nearest->staticId);
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
    else if (iequals(command, "who"))
    {
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
    else if (iequals(command, "stats"))
    {
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
    else if (iequals(command, "stuck"))
    {
        LocationVector nearestHl = sBotMgr.GetNearestHardline(getPosition().x, getPosition().z);
        nearestHl.y += 20.0f;
        this->setPosition(nearestHl);
        auto posMsg = make_shared<PositionStateMsg>(m_goId);
        m_parent.QueueState(posMsg);
        sGame.AnnounceStateUpdate(&m_parent, posMsg);
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}[Emergency Unstuck] Repositioned operator to nearest safe Hardline phone booth.{/c}"));
        return;
    }
    else if (iequals(command, "jackout") || iequals(command, "exit"))
    {
        ByteBuffer dummy;
        RPC_HandleJackoutRequest(dummy);
        return;
    }
    else if (iequals(command, "help") || iequals(command, "?"))
    {
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}=== THE MATRIX ONLINE COMMANDS ==={/c}"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}&bug <description> | /bug <description>{/c} - Save a bug report to the server database"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}&bugs | /bugs | &bug view <id>{/c} - Check saved bug reports in the database"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/attack | &attack{/c} - Engage selected target in melee interlock"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/tactic <power|speed|grab|block>{/c} - Set combat martial arts tactic"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/withdraw | &withdraw{/c} - Disengage from interlock"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/style <kungfu|karate|aikido|street>{/c} - Switch martial arts discipline style"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/loadability <id>{/c} - Load ability onto memory track"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}/target{/c} - Target nearest hostile operative"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/wear <slot> <id> | /dye <slot> <color>{/c} - Tailor RSI wardrobe & dye"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/clock{/c} - Current Matrix time & circadian cycle"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/heat{/c} - Local disruption heat & security tier"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/district <slums|dt|it|richland>{/c} - Fast district transit"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/who{/c} - Online operatives census"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/stats{/c} - Operator combat, HP & location telemetry"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/hardlines{/c} - Nearest Hardline phone booth beacon"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/stuck{/c} - Recover operative to nearest safe Hardline"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/jackout | /exit{/c} - Cleanly disconnect from the Matrix"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/vendor | /shop{/c} - Connect to nearest Hardline vendor"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/craft <id> | /craft list{/c} - Coder ability blueprint compilation"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/org <create|leave>{/c} - Syndicate organization management"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/dojo <1v1|clear|leave>{/c} - Sparring partner construct"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/contact <list|call <name>>{/c} - Uplink with canonical mission contacts"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/worldstatus | &worldstatus{/c} - District faction control, circadian hour & active world events"));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF55}/lair <enter|exit>{/c} - Enter or exit active instanced mission interior"));
        return;
    }
    else if (iequals(command, "worldstatus"))
    {
        sFactionWarMgr.WriteWorldStatusJsonFile("world_status.json");
        std::string jsonPayload = sFactionWarMgr.BuildWorldStatusJson();
        float circadianHour = sPedestrianEcology.GetMatrixHour();
        std::ostringstream summary;
        summary << std::fixed << std::setprecision(2);
        summary << "{c:00FFCC}[WORLD STATUS] Circadian Hour: " << circadianHour << ":00";
        for (const auto& pair : sFactionWarMgr.GetDistricts()) {
            uint32 dId = pair.first;
            uint32 ctrl = sFactionWarMgr.GetDistrictControllingFaction(dId);
            summary << " | D" << dId << "(" << sFactionWarMgr.GetDistrictName(dId) << "): " << sFactionWarMgr.GetFactionControlName(ctrl);
        }
        size_t activeEvents = 0;
        for (const auto& ev : sFactionWarMgr.GetActiveWorldEvents()) {
            if (ev.active) activeEvents++;
        }
        summary << " | Active World Events: " << activeEvents << "{/c}";
        m_parent.QueueCommand(make_shared<SystemChatMsg>(summary.str()));
        m_parent.QueueCommand(make_shared<SystemChatMsg>("[WORLD_STATUS_JSON] " + jsonPayload));
        return;
    }
    else if (iequals(command, "lair"))
    {
        std::string subCmd;
        cmdStream >> subCmd;
        if (iequals(subCmd, "exit") || iequals(subCmd, "leave")) {
            sMissionSys.ExitMissionInterior(this);
        } else if (iequals(subCmd, "status")) {
            uint32 instId = m_parent.m_instanceId;
            const MissionInteriorInstance* inst = (instId != 0) ? sMissionSys.GetInteriorInstance(instId) : nullptr;
            if (!inst) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    "{c:00FFCC}[CONSTRUCT INTERIOR] Status: Street Matrix (Dimension 0, no active interior instance).{/c}"));
            } else {
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFCC}[CONSTRUCT INTERIOR] Instance #%1% | Stage: %2% | Guards Remaining: %3% | Boss Door (0x%4$08X): %5% | Lieutenant: %6%{/c}")
                        % inst->instanceId % inst->stage % inst->guardGoIds.size() % inst->bossDoorId
                        % (inst->bossDoorUnlocked ? "UNLOCKED" : "LOCKED")
                        % (inst->bossDefeated ? "DEFEATED" : (inst->bossSpawned ? "SPAWNED" : "PENDING"))).str()));
            }
        } else {
            uint32 missionId = 0;
            uint32 doorId = 9101;
            if (!subCmd.empty() && !iequals(subCmd, "enter")) {
                try { missionId = (uint32)std::stoul(subCmd, nullptr, 0); } catch (...) {}
            } else if (!cmdStream.eof()) {
                cmdStream >> missionId;
            }
            if (!cmdStream.eof()) cmdStream >> doorId;
            sMissionSys.EnterMissionInterior(this, missionId, doorId);
        }
        return;
    }
    else if (iequals(command, "worldevent"))
    {
        std::string evTypeStr;
        cmdStream >> evTypeStr;
        if (iequals(evTypeStr, "status")) {
            size_t activeCount = 0;
            for (const auto& ev : sFactionWarMgr.GetActiveWorldEvents()) {
                if (!ev.active) continue;
                activeCount++;
                m_parent.QueueCommand(make_shared<SystemChatMsg>(
                    (format("{c:00FFCC}[WORLD EVENT STATUS] Event #%1%: %2% | Boss: %3% (GoID %4%) | District %5% (%6%){/c}")
                        % ev.eventId % ev.eventName % ev.bossName % ev.bossGoId % ev.districtId % sFactionWarMgr.GetDistrictName(ev.districtId)).str()));
            }
            if (activeCount == 0) {
                m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFCC}[WORLD EVENT STATUS] No active world boss events.{/c}"));
            }
            return;
        }
        if (!m_isAdmin) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}That command is restricted to admins.{/c}"));
            return;
        }
        uint32 distId = getDistrict() != 0 ? getDistrict() : 1;
        if (!cmdStream.eof()) cmdStream >> distId;
        bool isMero = (iequals(evTypeStr, "mero") || iequals(evTypeStr, "exile") ||
                       iequals(evTypeStr, "exileraid") || iequals(evTypeStr, "merovingian") ||
                       iequals(evTypeStr, "assassin"));
        WorldEventType evType = isMero
            ? WorldEventType::MEROVINGIAN_EXILE_RAID
            : WorldEventType::MACHINE_SENTINEL_INCURSION;
        sFactionWarMgr.TriggerWorldEvent(distId, evType, getPosition().x, getPosition().y, getPosition().z);
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

	if ((theMessage[0] == '?' || theMessage[0] == '!') &&
		(boost::istarts_with(theMessage.substr(1), "bug") || boost::istarts_with(theMessage.substr(1), "report")))
	{
		INFO_LOG(format("(%1%) %2%:%3% bug command: %4%") % m_parent.Address() % m_handle % m_goId % theMessage);
		ParsePlayerCommand(theMessage.substr(1));
		return;
	}
	if (boost::istarts_with(theMessage, "bug:") || boost::istarts_with(theMessage, "report:"))
	{
		size_t colonPos = theMessage.find(':');
		string desc = (colonPos != string::npos) ? theMessage.substr(colonPos + 1) : "";
		ParsePlayerCommand("bug " + desc);
		return;
	}

	if (m_isAdmin && theMessage[0] == '!')
	{
		ParseAdminCommand(theMessage.substr(1));
		return;
	}

	// Slash commands that start world crises, sabotage substations, or move/teleport the player
	// anywhere were open to every player (audit 2026-10-13). Admins only.
	if (!m_isAdmin && theMessage[0] == '/')
	{
		static const char* adminSlash[] = { "/crisis", "/sabotage", "/repair", "/district", "/teleport", "/tp ",
			"/tension", "/outbreak", "/glitch", "/anomaly", NULL };
		for (int i = 0; adminSlash[i] != NULL; i++)
		{
			if (boost::istarts_with(theMessage, adminSlash[i]))
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}That command is restricted to admins.{/c}"));
				return;
			}
		}
	}

	if (theMessage[0] == '&' ||
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
									  boost::istarts_with(theMessage, "/wear") ||
									  boost::istarts_with(theMessage, "/wardrobe") ||
									  boost::istarts_with(theMessage, "/capture") ||
									  boost::istarts_with(theMessage, "/hacknode") ||
									  boost::istarts_with(theMessage, "/socket") ||
									  boost::istarts_with(theMessage, "/overwrite") ||
									  boost::istarts_with(theMessage, "/agentstrike") ||
									  boost::istarts_with(theMessage, "/agent") ||
									  boost::istarts_with(theMessage, "/craft") ||
									  boost::istarts_with(theMessage, "/compile") ||
									  boost::istarts_with(theMessage, "/org") ||
									  boost::istarts_with(theMessage, "/crew") ||
									  boost::istarts_with(theMessage, "/mission") ||
									  boost::istarts_with(theMessage, "/contract") ||
									  boost::istarts_with(theMessage, "/duel") ||
									  boost::istarts_with(theMessage, "/spar") ||
									  boost::istarts_with(theMessage, "/hardline") ||
									  boost::istarts_with(theMessage, "/hardlines") ||
									  boost::istarts_with(theMessage, "/tension") ||
									  boost::istarts_with(theMessage, "/outbreak") ||
									  boost::istarts_with(theMessage, "/contact") ||
									  boost::istarts_with(theMessage, "/contacts") ||
									  boost::istarts_with(theMessage, "/dojo") ||
									  boost::istarts_with(theMessage, "/bug") ||
									  boost::istarts_with(theMessage, "/bugs") ||
									  boost::istarts_with(theMessage, "/report") ||
									  boost::istarts_with(theMessage, "/trade") ||
									  boost::istarts_with(theMessage, "/clock") ||
									  boost::istarts_with(theMessage, "/heat") ||
									  boost::istarts_with(theMessage, "/district") ||
									  boost::istarts_with(theMessage, "/who") ||
									  boost::istarts_with(theMessage, "/stats") ||
									  boost::istarts_with(theMessage, "/stuck") ||
									  boost::istarts_with(theMessage, "/vendor") ||
									  boost::istarts_with(theMessage, "/shop") ||
									  boost::istarts_with(theMessage, "/jackout") ||
									  boost::istarts_with(theMessage, "/exit") ||
									  boost::istarts_with(theMessage, "/help") ||
									  boost::iequals(theMessage, "/?") ||
									  boost::istarts_with(theMessage, "/f ") ||
									  boost::istarts_with(theMessage, "/faction") ||
									  boost::istarts_with(theMessage, "/teleport") ||
									  boost::istarts_with(theMessage, "/tp ") ||
									  boost::iequals(theMessage, "/tp") ||
									  boost::istarts_with(theMessage, "/worldstatus") ||
									  boost::istarts_with(theMessage, "/lair") ||
									  boost::istarts_with(theMessage, "/worldevent"))))
	{
		INFO_LOG(format("(%1%) %2%:%3% chat command: %4%") % m_parent.Address() % m_handle % m_goId % theMessage);
		ParsePlayerCommand(theMessage.substr(1));
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
	auto animMsg = make_shared<AnimationStateMsg>(m_goId);
	m_parent.QueueState(animMsg);
	sGame.AnnounceStateUpdate(&m_parent, animMsg);
}

void PlayerObject::RPC_HandleStartAnimtion( ByteBuffer &srcCmd )
{
	uint8 newAnimation = srcCmd.read<uint8>();
	m_currAnimation = newAnimation;
	auto animMsg = make_shared<AnimationStateMsg>(m_goId);
	m_parent.QueueState(animMsg);
	sGame.AnnounceStateUpdate(&m_parent, animMsg);
}

void PlayerObject::RPC_HandleChangeMood( ByteBuffer &srcCmd )
{
	uint8 newMood = srcCmd.read<uint8>();
	m_currMood = newMood;
	auto animMsg = make_shared<AnimationStateMsg>(m_goId);
	m_parent.QueueState(animMsg);
	sGame.AnnounceStateUpdate(&m_parent, animMsg);
	return;
}

void PlayerObject::RPC_HandlePerformEmote( ByteBuffer &srcCmd )
{
	uint32 emoteId = srcCmd.read<uint32>();
	uint32 emoteTarget = srcCmd.read<uint32>();

	m_emoteCounter++;
	auto emoteMsg = make_shared<EmoteMsg>(m_goId,emoteId,m_emoteCounter);
	m_parent.QueueState(emoteMsg);
	sGame.AnnounceStateUpdate(&m_parent, emoteMsg);

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
        // (removed, audit 2026-10-13) object type 0x1234 paid 500 info per click, and any
        // unmapped dynamic object with interaction 2/3 gave a random item from the whole item
        // database plus 50 info, repeatable forever. Neither came from the game. Kept behind
        // World.LegacyFreeLoot = 1 for anyone who relied on it while testing.
        static const bool s_legacyFreeLoot = sConfig.GetBoolDefault("World.LegacyFreeLoot", false);
        if (!s_legacyFreeLoot) return;
        if (objType == 0x1234) {
            m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}You collected an Archive Data Fragment! Gained 500 Info.{/c}"));
            addInformation(500);
            return;
        }
        if (interaction == 2 || interaction == 3)
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
	if (srcCmd.remaining() < 6)
		return;
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
	if (m_isAdmin)
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

	// HDS ObjectInteractionHandler.cs:9-18:
	// 0x01 = COLLECTOR, 0x03 = DOOR, 0x04 = HARDLINE_UPLOAD, 0x05 = HARDLINE_LAEXIT, 0x08 = HARDLINE_SYNC
	if (interaction == 0x01) // COLLECTOR (Archive / Data Node)
	{
		sMissionSys.AdvanceObjective(this, ObjectiveCommand::HACK, staticObjId);
		addInfo(25);
		saveCashToDB();
		m_parent.QueueCommand(std::make_shared<SetInformationCmd>(m_cash));
		m_parent.QueueCommand(make_shared<SystemChatMsg>(
			(format("{c:00FFCC}[ARCHIVE COLLECTOR] Extracted 25 Information Bits from static node 0x%1$08X.{/c}") % staticObjId).str()));
		return;
	}

	if (interaction == 0x04 || interaction == 0x08) // HARDLINE_UPLOAD (0x04) / HARDLINE_SYNC (0x08)
	{
		const HardlineNode* nearest = GetNearestHardline(m_district, m_pos.x, m_pos.z);
		if (!nearest)
			nearest = GetNearestHardline(1, m_pos.x, m_pos.z);
		m_parent.QueueState(std::make_shared<JackoutEffectMsg>(m_goId, false));
		if (nearest)
		{
			INFO_LOG(format("HardlineSync: %1% synchronized with Hardline #%2% (%3%) in District %4% via 0x80c8 interaction %5%")
				% m_handle % (int)nearest->hardlineId % nearest->name % (int)nearest->districtId % (int)interaction);
			m_parent.QueueCommand(make_shared<SystemChatMsg>(
				(format("{c:00FFCC}[HARDLINE] Synchronized with %1% (District %2%, Node #%3%).{/c}")
					% nearest->name % (int)nearest->districtId % (int)nearest->hardlineId).str()));
		}
		return;
	}

	if (interaction == 0x05) // HARDLINE_LAEXIT (HDS EnviromentPackets.cs:48-59 SendHardlineExit 0x80fb: 95 00 00 00 01)
	{
		INFO_LOG(format("HardlineExit: %1% triggered HARDLINE_LAEXIT (0x05) on static object 0x%2$08X") % m_handle % staticObjId);
		m_parent.QueueCommand(std::make_shared<HexGenericMsg>("80fb9500000001"));
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[HARDLINE] Hardline exit carrier confirmed.{/c}"));
		return;
	}

	if (interaction == 0x03) //open door
	{
		LocationVector loc = this->getPosition();

		// Only admins record new doors; check in-memory door cache instead of blocking on SQL SELECT.
		if (!sObjMgr.HasDoor(getDistrict(), staticObjId) && m_isAdmin)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FFFF00}You are using a door not in the database yet, lets add it{/c}"));	
			sObjMgr.RegisterDoor(m_district, staticObjId, loc.x, loc.y, loc.z, loc.rot, 1);
			format sqlDoorInsert = 
				format("INSERT INTO `doors` SET  `DistrictId` = '%1%', `DoorId` = '%2%', X = '%3%', Y = '%4%', Z = '%5%', ROT = '%6%', FirstUser = '%7%'")
				% (int)m_district
				% staticObjId
				% loc.x	% loc.y	% loc.z	% loc.rot
				% this->getHandle();

			if (Database_Main == nullptr || sDatabase.Execute(sqlDoorInsert))
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
	auto posMsg = make_shared<PositionStateMsg>(m_goId);
	m_parent.QueueState(posMsg);
	sGame.AnnounceStateUpdate(&m_parent, posMsg);
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

	// The saved district is already on the character. Forcing 3 put Slums
	// coordinates into the International load. 0 is not a district; the schema default is 1.
	if (m_district == 0)
		m_district = 1;

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

	DEBUG_LOG(format("(%1%) %2%: %3%") % m_parent.Address() % m_handle % debugMsg.str());

	// The hardline UI only opens at a hardline. Without these checks anyone could jump to any
	// hardline from anywhere, mid-fight or while dead (0x818e and &hardline tp both land here).
	if (!m_isAdmin)
	{
		if (isDead() || getCurrentHealth() == 0)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You can't use a hardline while down.{/c}"));
			return;
		}
		if (sCombatSys.IsInterlocked(m_goId) || sCombatSys.IsFreeFiring(m_goId))
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You can't jack out of a fight.{/c}"));
			return;
		}
		const double useRange = sConfig.GetFloatDefault("Hardline.UseRange", 800.0f);
		bool atHardline = false;
		for (const auto& kv : GetHardlineDirectory())
		{
			if (kv.second.districtId != m_district) continue;
			const double dx = kv.second.x - m_pos.x, dz = kv.second.z - m_pos.z;
			if (dx * dx + dz * dz <= useRange * useRange) { atHardline = true; break; }
		}
		if (!atHardline)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}You need to be at a hardline to use it.{/c}"));
			return;
		}
	}

	double newX = 0.0, newY = 0.0, newZ = 0.0, newRot = 0.0;
	string newlocationName = "";
	int factionTag = 0;
	bool found = false;

	// Fast-path: check in-memory static hardline directory first (zero main-thread SQL blocking)
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

	// Fallback to database only if custom hardline entry is not in the in-memory directory
	if (!found && Database_Main != nullptr)
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

	if (newY >= -10.0 && newY <= 10.0)
	{
		newY = 572.0;
	}
	LocationVector newLoc(newX, newY, newZ);
	newLoc.rot = newRot;
	this->setPosition(newLoc);
	this->setDistrict(hardlineDistrict);
	this->saveDataToDB();
	auto posMsg = make_shared<PositionStateMsg>(m_goId);
	m_parent.QueueState(posMsg);
	sGame.AnnounceStateUpdate(&m_parent, posMsg);
	this->FlushAndRefreshAoI();

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
	if (m_parent.isBot())
		return;

	// Drop the fight first so the other person is not left in a dead interlock.
	sCombatSys.RemoveCombatant(m_goId);
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80fd000000000000"));
	m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}[JACKOUT] Carrier signal terminated. Jackout complete.{/c}"));
	m_parent.FlushQueue();
	saveDataToDB();
	INFO_LOG(format("Player %1% successfully jacked out of the Matrix.") % m_handle);
	// The client never sends JackoutFinished. Without this the socket stays open and you are still in the city.
	m_parent.Invalidate();
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
// true when a vendor of the player's district stands within Vendor.UseRange; with an item id,
// that vendor must also stock the item
bool PlayerObject::IsNearVendor(uint32 itemTemplateId) const
{
	const double range = sConfig.GetFloatDefault("Vendor.UseRange", 1500.0f);
	for (const auto& v : sEconomySys.GetVendorsForDistrict(m_district))
	{
		const double dx = v.x - m_pos.x, dz = v.z - m_pos.z;
		if (dx * dx + dz * dz > range * range) continue;
		if (itemTemplateId == 0) return true;
		for (uint32 t : v.inventoryTemplates)
			if (t == itemTemplateId) return true;
	}
	return false;
}

void PlayerObject::RPC_HandleVendorBuy(ByteBuffer& srcCmd)
{
	if (srcCmd.remaining() < 4) return;
	uint32 itemTemplateId = srcCmd.read<uint32>();
	uint32 vendorGoId = 0;
	if (srcCmd.remaining() >= 4)
		vendorGoId = srcCmd.read<uint32>();

	// Any item template could be bought from anywhere (audit 2026-10-13). A purchase now needs
	// a vendor in this district within Vendor.UseRange that stocks the item. Admins bypass.
	if (!m_isAdmin && !IsNearVendor(itemTemplateId))
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] No vendor nearby sells that item.{/c}"));
		return;
	}

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

	if (!m_isAdmin && !IsNearVendor())
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[VENDOR] You need to be at a vendor to sell.{/c}"));
		return;
	}

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

// 0x8110 - retail client vendor close notification
// Primary source: Client\client.dll+0x10A170 (0x6210A170) and 0x6210A200 (0x6210A200),
// registered in the "Trading" RPC table at .rdata 0x6287D570..0x6287D5AC (0x010e, 0x0110, 0x0111).
// 0x6210A170 allocates a 7-byte packet: [0x81, 0x10][uint32 0x00000000][uint8 0x01].
void PlayerObject::RPC_HandleVendorClose(ByteBuffer& srcCmd)
{
	uint32 vendorCtx = 0;
	uint8 closeFlag = 0;
	if (srcCmd.remaining() >= 4)
		vendorCtx = srcCmd.read<uint32>();
	if (srcCmd.remaining() >= 1)
		closeFlag = srcCmd.read<uint8>();
	INFO_LOG(format("(%1%) %2%:%3% RPC_HandleVendorClose: ctx=%4% flag=%5%")
		% m_parent.Address() % m_handle % m_goId % vendorCtx % uint32(closeFlag));
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

void PlayerObject::RPC_HandleApplyAttributes(ByteBuffer& srcCmd)
{
	// Primary source citations:
	// 1. Client/client.dll 0x620c1330 (LevelUp_ApplyAttributes) -> 0x621d4680 (RPC 0x80b5 marshaler):
	//    Payload after 0x80b5: uint16 arrayOffset (0x0002), uint16 count, followed by count * 6-byte entries:
	//      [uint8 unk0, uint32 attrId, uint8 delta]
	//    Attribute IDs (client.dll 0x6287727c & hd_reference/hds/world/Client/RpcHandlers/PlayerHandler.cs:89-100):
	//      0x4E (78) = Focus, 0x4F (79) = Perception, 0x51 (81) = Reason, 0x52 (82) = Belief, 0x54 (84) = Vitality
	// 2. Client/client.dll 0x6217a174 (RPC 0x80b6 dispatcher) -> parser 0x6213b450 -> handler 0x621e8c30 -> UI callback 0x625c52b0
	if (srcCmd.remaining() < 4)
		return;

	uint16 arrayOffset = srcCmd.read<uint16>();
	if (arrayOffset > 2 && srcCmd.remaining() >= static_cast<size_t>(arrayOffset - 2 + 2))
	{
		for (uint16 skip = 2; skip < arrayOffset; ++skip)
			srcCmd.read<uint8>();
	}
	if (srcCmd.remaining() < 2)
		return;

	uint16 count = srcCmd.read<uint16>();
	if (count > 16)
		count = 16;

	std::vector<std::pair<uint32, uint8>> updatedAttrs;
	for (uint16 i = 0; i < count && srcCmd.remaining() >= 6; ++i)
	{
		uint8 unk0 = srcCmd.read<uint8>();
		(void)unk0;
		uint32 attrId = srcCmd.read<uint32>();
		uint8 delta = srcCmd.read<uint8>();

		uint16 newVal = 0;
		switch (attrId)
		{
		case 0x4E: // Focus (78)
			if (delta > 0) m_focus = std::min<uint16>(60, m_focus + delta);
			newVal = m_focus;
			break;
		case 0x4F: // Perception (79)
			if (delta > 0) m_perception = std::min<uint16>(60, m_perception + delta);
			newVal = m_perception;
			break;
		case 0x51: // Reason (81)
			if (delta > 0) m_reason = std::min<uint16>(60, m_reason + delta);
			newVal = m_reason;
			break;
		case 0x52: // Belief (82)
			if (delta > 0) m_belief = std::min<uint16>(60, m_belief + delta);
			newVal = m_belief;
			break;
		case 0x54: // Vitality (84)
			if (delta > 0) m_vitality = std::min<uint16>(60, m_vitality + delta);
			newVal = m_vitality;
			break;
		default:
			continue;
		}
		updatedAttrs.push_back(std::make_pair(attrId, static_cast<uint8>(newVal)));
		m_parent.QueueCommand(std::make_shared<PlayerAttributeMsg>(static_cast<uint16>(attrId), newVal));
	}

	saveDataToDB();
	m_parent.QueueCommand(std::make_shared<AttributeUpgradeRspMsg>(0, updatedAttrs));
	INFO_LOG(format("(%1%) %2%:%3% RPC_HandleApplyAttributes saved %4% attribute(s): Focus=%5% Perception=%6% Reason=%7% Belief=%8% Vitality=%9%")
		% m_parent.Address() % m_handle % m_goId % updatedAttrs.size()
		% m_focus % m_perception % m_reason % m_belief % m_vitality);
}

void PlayerObject::RPC_HandleUpgradeAbility(ByteBuffer& srcCmd)
{
	// Primary source citations:
	// 1. hd_reference/hds/world/Client/RpcHandlers/AbilityHandler.cs:22-35 (ProcessUpgradeAbility: ability u16, unknown u16, levelToIncrease u8)
	// 2. Client/client.dll 0x620c1380 (LevelUp_ApplyUpgrade) -> 0x621e1290 (RPC 0x80b7 marshaler):
	//    [esi + 0xb8] = abilityId (u16), [esi + 0xc4] = levelToIncrease (u8 delta added in UI)
	if (srcCmd.remaining() < 5)
		return;
	uint16 abilityId = srcCmd.read<uint16>();
	uint16 unknown = srcCmd.read<uint16>();
	uint8 levelDelta = srcCmd.read<uint8>();

	INFO_LOG(format("(%1%) %2%:%3% upgrade ability %4% unk %5% delta +%6%")
		% m_parent.Address() % m_handle % m_goId % abilityId % unknown % uint32(levelDelta));

	if (abilityId == 0 || !m_abilitySystem)
		return;

	auto ab = m_abilitySystem->getAbility(abilityId);
	if (!ab)
	{
		m_parent.QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}You don't have that ability.{/c}"));
		return;
	}
	uint16 inc = (levelDelta > 0) ? uint16(levelDelta) : 1;
	uint16 newLevel = ab->getLevel() + inc;
	uint16 maxAllowed = std::max<uint16>(10, uint16(getLevel()));
	if (newLevel > maxAllowed)
		newLevel = maxAllowed;
	if (newLevel == 0)
		newLevel = 1;

	m_abilitySystem->loadAbility(abilityId, newLevel, ab->getMemorySlot());
	m_abilitySystem->saveToDB();
	m_parent.QueueCommand(std::make_shared<AbilityUpgradeRspMsg>(abilityId, newLevel));
	m_parent.QueueCommand(std::make_shared<AbilityLoadRspMsg>(abilityId, newLevel, ab->getMemorySlot()));
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
		INFO_LOG(format("MissionSystem: Player %1% active contract in progress") % m_handle);
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
	// 0x8065: [u16 fromSlot][u16 toSlot] (HDS InventoryHandler.processItemMove). The handler used to
	// read two bytes, so the destination was always the high byte of the source (0).
	uint16 fromSlot = 0, toSlot = 0;
	if (srcCmd.remaining() >= 4)
	{
		fromSlot = srcCmd.read<uint16>();
		toSlot = srcCmd.read<uint16>();
	}
	else if (srcCmd.remaining() >= 2)
	{
		fromSlot = srcCmd.read<uint8>();
		toSlot = srcCmd.read<uint8>();
	}
	else
		return;
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleItemMoveSlot: fromSlot=%4% toSlot=%5%")
		% m_parent.Address() % m_handle % m_goId % fromSlot % toSlot);
	if (!m_inventorySystem || fromSlot > 0xFF || toSlot > 0xFF)
		return;
	if (m_inventorySystem->moveItem(uint8(fromSlot), uint8(toSlot)))
	{
		// the client waits for the server's 0x65 before it moves the icon (HDS sends it back)
		m_inventorySystem->notifyMove(uint8(fromSlot), uint8(toSlot));
		if (!getClient().isBot())
			m_inventorySystem->saveToDB();
	}
}

// Retail wear slots (HDS InventoryHandler.processMountItem): 0x61 hat, 0x62 glasses, 0x63 shirt,
// 0x64 gloves, 0x65 coat, 0x66 pants, 0x67 leggings, 0x68 shoes. Part names as in
// ItemTemplate::clothesSlot / SetRsiWardrobe.
static uint8 WearSlotForPart(const std::string &part)
{
	if (boost::iequals(part, "Hat")) return 0x61;
	if (boost::iequals(part, "Glasses")) return 0x62;
	if (boost::iequals(part, "Shirt")) return 0x63;
	if (boost::iequals(part, "Gloves")) return 0x64;
	if (boost::iequals(part, "Coat")) return 0x65;
	if (boost::iequals(part, "Pants")) return 0x66;
	if (boost::iequals(part, "Leggings")) return 0x67;
	if (boost::iequals(part, "Shoes")) return 0x68;
	return 0;
}

void PlayerObject::RPC_HandleItemUnmountRSI(ByteBuffer& srcCmd)
{
	// 0x8064: [u8 wearSlot][u8 destSlot] (HDS reads rpcData[0])
	if (srcCmd.remaining() < 1)
		return;
	const uint8 sourceSlot = srcCmd.read<uint8>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleItemUnmountRSI: slot=%4%")
		% m_parent.Address() % m_handle % m_goId % (int)sourceSlot);
	shared_ptr<Item> item = (m_inventorySystem && sourceSlot >= 0x61 && sourceSlot <= 0x68)
		? m_inventorySystem->getItemBySlot(sourceSlot) : shared_ptr<Item>();
	if (!item)
	{
		// not a worn item: the server's own callers pass a part index (1 coat .. 7 hat)
		UnmountClothingItem(sourceSlot);
		if (m_inventorySystem && !getClient().isBot())
			m_inventorySystem->saveToDB();
		return;
	}
	const ItemTemplate* tpl = sDataLoader.GetItemTemplate(item->getTemplateId());
	const uint8 newSlot = m_inventorySystem->getFirstFreeSlot();
	if (newSlot == 0)
	{
		m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Your inventory is full.{/c}"));
		return;
	}
	if (tpl && !tpl->clothesSlot.empty())
		SetRsiWardrobe(tpl->clothesSlot, 0, 0); // bare part, as HDS writes 0 for model and colour
	if (m_inventorySystem->moveItem(sourceSlot, newSlot))
		m_inventorySystem->notifyMove(sourceSlot, newSlot);
	if (!getClient().isBot())
		m_inventorySystem->saveToDB();
}

void PlayerObject::RPC_HandleItemMountRSI(ByteBuffer& srcCmd)
{
	// 0x8063: [u8 sourceSlot][u8 destSlot] (HDS reads rpcData[0], rpcData[1]); the wear slot comes
	// from the item's clothes type, not from the request
	if (srcCmd.remaining() < 1 || !m_inventorySystem)
		return;
	if (srcCmd.remaining() >= 5)
	{
		// server-side form used by the bot/test harness: [u32 item goid or template][u8 part index]
		const uint32 itemGoId = srcCmd.read<uint32>();
		const uint8 partHint = srcCmd.read<uint8>();
		MountClothingItem(itemGoId, partHint);
		if (!getClient().isBot())
			m_inventorySystem->saveToDB();
		return;
	}
	const uint8 sourceSlot = srcCmd.read<uint8>();
	DEBUG_LOG(format("(%1%) %2%:%3% RPC_HandleItemMountRSI: slot=%4%")
		% m_parent.Address() % m_handle % m_goId % (int)sourceSlot);
	shared_ptr<Item> item = m_inventorySystem->getItemBySlot(sourceSlot);
	if (!item)
		return;
	const ItemTemplate* tpl = sDataLoader.GetItemTemplate(item->getTemplateId());
	const uint8 wearSlot = tpl ? WearSlotForPart(tpl->clothesSlot) : 0;
	if (!tpl || wearSlot == 0)
	{
		// weapons and anything else keep the existing equip path
		MountClothingItem(item->getGoId(), sourceSlot);
		if (!getClient().isBot())
			m_inventorySystem->saveToDB();
		return;
	}
	// whatever is in the wear slot goes back to the first free bag slot
	if (m_inventorySystem->getItemBySlot(wearSlot))
	{
		const uint8 freeSlot = m_inventorySystem->getFirstFreeSlot();
		if (freeSlot == 0)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF4444}Your inventory is full.{/c}"));
			return;
		}
		if (m_inventorySystem->moveItem(wearSlot, freeSlot))
			m_inventorySystem->notifyMove(wearSlot, freeSlot);
	}
	SetRsiWardrobe(tpl->clothesSlot, uint8(tpl->rsiDataId), int(tpl->rsiColorId));
	if (m_inventorySystem->moveItem(sourceSlot, wearSlot))
		m_inventorySystem->notifyMove(sourceSlot, wearSlot);
	if (!getClient().isBot())
		m_inventorySystem->saveToDB();
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


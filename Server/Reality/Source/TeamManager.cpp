#include "TeamManager.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "Log.h"
#include <boost/algorithm/string.hpp>
#include <cstring>
#include <algorithm>

// String fields: the fixed header holds a u16 offset (from the opcode byte) to [u16 len incl NUL][bytes][NUL];
// offset 0 = empty string (client parser FUN_6211e670).
static void putStringAt(ByteBuffer& b, size_t offsetFieldPos, const std::string& s)
{
	if (s.empty()) { b.put<uint16>(offsetFieldPos, 0); return; }
	b.put<uint16>(offsetFieldPos, uint16(b.wpos()));
	b.writeString(s);
}

static std::string readStringAt(ByteBuffer& pkt, uint16 off)
{
	if (off == 0 || size_t(off) + 2 > pkt.size()) return std::string();
	uint16 len = *reinterpret_cast<const uint16*>(&pkt.contents()[off]);
	if (len == 0 || size_t(off) + 2 + len > pkt.size()) return std::string();
	std::string s(reinterpret_cast<const char*>(&pkt.contents()[off + 2]), len);
	while (!s.empty() && s.back() == '\0') s.pop_back();
	return s;
}

template <typename T> static bool readAt(ByteBuffer& pkt, size_t pos, T& out)
{
	if (pos + sizeof(T) > pkt.size()) return false;
	memcpy(&out, &pkt.contents()[pos], sizeof(T));
	return true;
}

uint32 TeamManager::WireId(PlayerObject* p)
{
	return p ? uint32(p->getCharacterUID() & 0xFFFFFFFF) : 0;
}

ByteBuffer TeamManager::BuildTeamCreated(uint32 teamId, const std::string& name, uint8 loot, uint32 status)
{
	ByteBuffer b;
	b << uint8(0x80) << uint8(0x8d) << uint32(teamId);
	size_t nameOff = b.wpos(); b << uint16(0);
	b << uint8(loot) << uint8(0) << uint32(status);   // 14 fixed bytes
	putStringAt(b, nameOff, name);
	return b;
}

ByteBuffer TeamManager::BuildRoster(uint32 selfId, const MissionTeam& t, const std::vector<std::pair<uint32, std::string>>& members, uint32 leaderId)
{
	ByteBuffer b;
	b << uint8(0x80) << uint8(0x8e) << uint32(selfId) << uint32(t.id);
	size_t nameOff = b.wpos(); b << uint16(0);
	b << uint32(leaderId) << uint8(t.lootSharing);
	size_t membersOff = b.wpos(); b << uint16(0);        // 19 fixed bytes
	putStringAt(b, nameOff, t.name);
	b.put<uint16>(membersOff, uint16(b.wpos()));
	b << uint16(members.size());
	for (const auto& m : members)
	{
		// 38-byte record: u8 unused, u32 MemberID, char[32] name, u8 UIRanking
		b << uint8(0) << uint32(m.first);
		char name[32]; memset(name, 0, sizeof(name));
		strncpy(name, m.second.c_str(), sizeof(name) - 1);
		b.append(reinterpret_cast<const uint8*>(name), sizeof(name));
		b << uint8(0);
	}
	return b;
}

ByteBuffer TeamManager::BuildInvite(const std::string& inviterName, const std::string& teamName)
{
	ByteBuffer b;
	b << uint8(0x80) << uint8(0x8f);
	size_t inviterOff = b.wpos(); b << uint16(0);
	size_t groupOff = b.wpos(); b << uint16(0);
	b << uint8(0) << uint8(0);                          // MissionSponsorOrg, MissionTargetOrg
	size_t typeOff = b.wpos(); b << uint16(0);
	size_t rewardOff = b.wpos(); b << uint16(0);        // 12 fixed bytes
	putStringAt(b, inviterOff, inviterName);
	putStringAt(b, groupOff, teamName);
	putStringAt(b, typeOff, "");
	putStringAt(b, rewardOff, "");
	return b;
}

ByteBuffer TeamManager::BuildInviteResult(const std::string& name, uint32 status)
{
	ByteBuffer b;
	b << uint8(0x6f) << uint8(GROUP_MISSION_TEAM);
	size_t nameOff = b.wpos(); b << uint16(0);
	b << uint32(status);                                // 8 fixed bytes
	putStringAt(b, nameOff, name);
	return b;
}

ByteBuffer TeamManager::BuildMemberJoined(uint32 playerId, const std::string& name)
{
	ByteBuffer b;
	b << uint8(0x70) << uint8(GROUP_MISSION_TEAM) << uint32(playerId);
	size_t nameOff = b.wpos(); b << uint16(0);
	b << uint8(0);                                      // PlayerData; 9 fixed bytes
	putStringAt(b, nameOff, name);
	return b;
}

ByteBuffer TeamManager::BuildMemberRemoved(uint32 playerId)
{
	ByteBuffer b; b << uint8(0x72) << uint8(GROUP_MISSION_TEAM) << uint32(playerId); return b;
}

ByteBuffer TeamManager::BuildNewLeader(uint32 playerId)
{
	ByteBuffer b; b << uint8(0x75) << uint8(GROUP_MISSION_TEAM) << uint32(playerId); return b;
}

ByteBuffer TeamManager::BuildDisbanded()
{
	ByteBuffer b; b << uint8(0x77) << uint8(GROUP_MISSION_TEAM); return b;
}

ByteBuffer TeamManager::BuildLootMode(uint8 mode)
{
	ByteBuffer b; b << uint8(0x80) << uint8(0x90) << uint8(mode); return b;
}

// Same layout as PlayerChatMsg (HDS SendChatMessage) with scope 21 = mission team
ByteBuffer TeamManager::BuildTeamChat(uint32 charId, const std::string& handle, const std::string& text)
{
	ByteBuffer b;
	b << uint8(0x2E) << uint16(21) << uint32(charId);
	b << uint32(36) << uint32(uint32(handle.length()) + 36 + 3);
	for (int i = 0; i < 21; ++i) b << uint8(0);
	b.writeString(handle);
	b.writeString(text);
	return b;
}

void TeamManager::sendTo(uint32 goId, const ByteBuffer& b)
{
	PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
	if (!p || p->getClient().isBot()) return;
	p->getClient().QueueCommand(std::make_shared<StaticMsg>(b));
}

void TeamManager::sendToTeam(const MissionTeam& t, const ByteBuffer& b, uint32 exceptGoId)
{
	for (uint32 m : t.memberGoIds)
		if (m != exceptGoId) sendTo(m, b);
}

void TeamManager::sendRoster(const MissionTeam& t, uint32 toGoId)
{
	PlayerObject* to = sObjMgr.getGOPtrSafe(toGoId);
	if (!to) return;
	std::vector<std::pair<uint32, std::string>> members;
	for (uint32 m : t.memberGoIds)
		if (PlayerObject* p = sObjMgr.getGOPtrSafe(m))
			members.push_back(std::make_pair(WireId(p), p->getHandle()));
	sendTo(toGoId, BuildRoster(WireId(to), t, members, WireId(sObjMgr.getGOPtrSafe(t.leaderGoId))));
}

PlayerObject* TeamManager::findByHandle(const std::string& handle)
{
	if (handle.empty()) return nullptr;
	for (uint32 id : sObjMgr.getAllGOIds())
	{
		PlayerObject* p = sObjMgr.getGOPtrSafe(id);
		if (p && boost::iequals(p->getHandle(), handle)) return p;
	}
	return nullptr;
}

uint32 TeamManager::GetTeamId(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	auto it = m_memberTeam.find(goId);
	return it == m_memberTeam.end() ? 0 : it->second;
}

bool TeamManager::GetTeamCopy(uint32 teamId, MissionTeam& out)
{
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	auto it = m_teams.find(teamId);
	if (it == m_teams.end()) return false;
	out = it->second;
	return true;
}

std::vector<uint32> TeamManager::GetMembers(uint32 goId)
{
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	auto it = m_memberTeam.find(goId);
	if (it == m_memberTeam.end()) return {};
	return m_teams[it->second].memberGoIds;
}

bool TeamManager::SameTeam(uint32 a, uint32 b)
{
	uint32 ta = GetTeamId(a);
	return ta != 0 && ta == GetTeamId(b);
}

uint32 TeamManager::PendingInviteFrom(uint32 inviteeGoId)
{
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	auto it = m_pending.find(inviteeGoId);
	return it == m_pending.end() ? 0 : it->second.first;
}

bool TeamManager::Invite(PlayerObject* inviter, PlayerObject* invitee, const std::string& teamName)
{
	if (!inviter) return false;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	if (!invitee || invitee == inviter)
	{
		sendTo(inviter->getGoId(), BuildInviteResult(invitee ? invitee->getHandle() : "", INVITE_FAILED));
		return false;
	}
	uint32 teamId = GetTeamId(inviter->getGoId());
	if (teamId != 0)
	{
		MissionTeam& t = m_teams[teamId];
		if (t.leaderGoId != inviter->getGoId() || t.memberGoIds.size() >= MAX_TEAM_SIZE)
		{
			sendTo(inviter->getGoId(), BuildInviteResult(invitee->getHandle(), INVITE_FAILED));
			return false;
		}
	}
	if (GetTeamId(invitee->getGoId()) != 0 || m_pending.count(invitee->getGoId()))
	{
		sendTo(inviter->getGoId(), BuildInviteResult(invitee->getHandle(), INVITE_FAILED));
		return false;
	}
	if (teamId == 0)
	{
		// the inviter becomes leader of a new team (client expects 80 8d before anyone joins)
		MissionTeam t;
		t.id = m_nextTeamId++;
		t.name = teamName.empty() ? (inviter->getHandle() + "'s Team") : teamName;
		t.leaderGoId = inviter->getGoId();
		t.memberGoIds.push_back(inviter->getGoId());
		m_teams[t.id] = t;
		m_memberTeam[inviter->getGoId()] = t.id;
		teamId = t.id;
		sendTo(inviter->getGoId(), BuildTeamCreated(t.id, t.name, t.lootSharing, 0));
		INFO_LOG(format("Teams: %1% formed team %2% '%3%'") % inviter->getHandle() % t.id % t.name);
	}
	m_pending[invitee->getGoId()] = std::make_pair(inviter->getGoId(), m_teams[teamId].name);
	sendTo(invitee->getGoId(), BuildInvite(inviter->getHandle(), m_teams[teamId].name));
	INFO_LOG(format("Teams: %1% invited %2% to team %3%") % inviter->getHandle() % invitee->getHandle() % teamId);
	return true;
}

bool TeamManager::Accept(PlayerObject* invitee)
{
	if (!invitee) return false;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	auto pit = m_pending.find(invitee->getGoId());
	if (pit == m_pending.end()) return false;
	uint32 inviterGoId = pit->second.first;
	m_pending.erase(pit);
	uint32 teamId = GetTeamId(inviterGoId);
	PlayerObject* inviter = sObjMgr.getGOPtrSafe(inviterGoId);
	if (teamId == 0 || !inviter || m_teams[teamId].memberGoIds.size() >= MAX_TEAM_SIZE || GetTeamId(invitee->getGoId()) != 0)
	{
		if (inviter) sendTo(inviterGoId, BuildInviteResult(invitee->getHandle(), INVITE_FAILED));
		return false;
	}
	MissionTeam& t = m_teams[teamId];
	sendTo(inviterGoId, BuildInviteResult(invitee->getHandle(), 0));
	sendToTeam(t, BuildMemberJoined(WireId(invitee), invitee->getHandle()));
	t.memberGoIds.push_back(invitee->getGoId());
	m_memberTeam[invitee->getGoId()] = teamId;
	sendRoster(t, invitee->getGoId());
	INFO_LOG(format("Teams: %1% joined team %2% (%3% members)") % invitee->getHandle() % teamId % t.memberGoIds.size());
	return true;
}

void TeamManager::Decline(PlayerObject* invitee)
{
	if (!invitee) return;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	auto pit = m_pending.find(invitee->getGoId());
	if (pit == m_pending.end()) return;
	uint32 inviterGoId = pit->second.first;
	m_pending.erase(pit);
	sendTo(inviterGoId, BuildInviteResult(invitee->getHandle(), INVITE_DECLINED));
	// a team made only for this invite stays as a one-member team until the leader leaves
}

void TeamManager::removeMemberLocked(uint32 goId, bool booted, PlayerObject* known, bool quiet)
{
	auto it = m_memberTeam.find(goId);
	if (it == m_memberTeam.end()) return;
	uint32 teamId = it->second;
	MissionTeam& t = m_teams[teamId];
	PlayerObject* p = known ? known : sObjMgr.getGOPtrSafe(goId);
	const uint32 wire = p ? WireId(p) : 0;
	// everyone, including the member leaving, gets 72 (the client resets its team when the id is its own)
	sendToTeam(t, BuildMemberRemoved(wire), quiet ? goId : 0);
	t.memberGoIds.erase(std::remove(t.memberGoIds.begin(), t.memberGoIds.end(), goId), t.memberGoIds.end());
	m_memberTeam.erase(it);
	if (p && !quiet && !p->getClient().isBot())
		p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(booted ? "You were removed from the team." : "You left the team."));
	if (t.memberGoIds.size() <= 1)
	{
		// a team of one is gone
		for (uint32 m : t.memberGoIds) { sendTo(m, BuildDisbanded()); m_memberTeam.erase(m); }
		for (auto pit = m_pending.begin(); pit != m_pending.end(); )
			pit = (GetTeamId(pit->second.first) == 0 || pit->second.first == goId) ? m_pending.erase(pit) : std::next(pit);
		m_teams.erase(teamId);
		return;
	}
	if (t.leaderGoId == goId)
	{
		t.leaderGoId = t.memberGoIds.front();
		sendToTeam(t, BuildNewLeader(WireId(sObjMgr.getGOPtrSafe(t.leaderGoId))));
	}
}

void TeamManager::Leave(PlayerObject* p, bool booted)
{
	if (!p) return;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	m_pending.erase(p->getGoId());
	removeMemberLocked(p->getGoId(), booted);
}

void TeamManager::OnLogout(PlayerObject* p)
{
	if (!p) return;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	m_pending.erase(p->getGoId());
	for (auto pit = m_pending.begin(); pit != m_pending.end(); )
		pit = (pit->second.first == p->getGoId()) ? m_pending.erase(pit) : std::next(pit);
	removeMemberLocked(p->getGoId(), false, p, true);
}

void TeamManager::HandleInvite(PlayerObject* inviter, ByteBuffer& pkt)
{
	uint16 inviteeOff = 0, teamOff = 0; uint8 loot = 4;
	readAt(pkt, 2, inviteeOff); readAt(pkt, 4, teamOff); readAt(pkt, 6, loot);
	const std::string inviteeName = readStringAt(pkt, inviteeOff);
	const std::string teamName = readStringAt(pkt, teamOff);
	PlayerObject* invitee = findByHandle(inviteeName);
	if (!invitee || invitee->getClient().isBot())
	{
		sendTo(inviter->getGoId(), BuildInviteResult(inviteeName, INVITE_FAILED));
		return;
	}
	if (Invite(inviter, invitee, teamName) && loot >= 1 && loot <= 4)
	{
		std::lock_guard<std::recursive_mutex> l(m_mutex);
		uint32 tid = GetTeamId(inviter->getGoId());
		if (tid) m_teams[tid].lootSharing = loot;
	}
}

void TeamManager::HandleInviteReply(PlayerObject* invitee, ByteBuffer& pkt)
{
	uint8 groupType = 0; uint32 status = 0;
	readAt(pkt, 1, groupType); readAt(pkt, 4, status);
	if (groupType != GROUP_MISSION_TEAM) return; // crews/factions are not implemented here
	if (status == 0) Accept(invitee); else Decline(invitee);
}

void TeamManager::HandleLeave(PlayerObject* p) { Leave(p, false); }

void TeamManager::HandleBoot(PlayerObject* p, ByteBuffer& pkt)
{
	uint32 wire = 0; readAt(pkt, 2, wire);
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	uint32 tid = GetTeamId(p->getGoId());
	if (!tid || m_teams[tid].leaderGoId != p->getGoId()) return;
	for (uint32 m : m_teams[tid].memberGoIds)
	{
		PlayerObject* mp = sObjMgr.getGOPtrSafe(m);
		if (mp && WireId(mp) == wire && m != p->getGoId()) { removeMemberLocked(m, true); return; }
	}
}

void TeamManager::HandleMakeLeader(PlayerObject* p, ByteBuffer& pkt)
{
	uint32 wire = 0; readAt(pkt, 2, wire);
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	uint32 tid = GetTeamId(p->getGoId());
	if (!tid || m_teams[tid].leaderGoId != p->getGoId()) return;
	MissionTeam& t = m_teams[tid];
	for (uint32 m : t.memberGoIds)
	{
		PlayerObject* mp = sObjMgr.getGOPtrSafe(m);
		if (mp && WireId(mp) == wire) { t.leaderGoId = m; sendToTeam(t, BuildNewLeader(wire)); return; }
	}
}

void TeamManager::HandleDisband(PlayerObject* p)
{
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	uint32 tid = GetTeamId(p->getGoId());
	if (!tid || m_teams[tid].leaderGoId != p->getGoId()) return;
	MissionTeam t = m_teams[tid];
	sendToTeam(t, BuildDisbanded());
	for (uint32 m : t.memberGoIds) m_memberTeam.erase(m);
	m_teams.erase(tid);
	INFO_LOG(format("Teams: %1% disbanded team %2%") % p->getHandle() % tid);
}

void TeamManager::HandleLootMode(PlayerObject* p, uint8 mode)
{
	if (mode < 1 || mode > 4) return;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	uint32 tid = GetTeamId(p->getGoId());
	if (!tid || m_teams[tid].leaderGoId != p->getGoId()) return;
	m_teams[tid].lootSharing = mode;
	sendToTeam(m_teams[tid], BuildLootMode(mode));
}

void TeamManager::HandleTeamChat(PlayerObject* p, const std::string& text)
{
	if (text.empty()) return;
	std::lock_guard<std::recursive_mutex> l(m_mutex);
	uint32 tid = GetTeamId(p->getGoId());
	if (!tid)
	{
		if (!p->getClient().isBot())
			p->getClient().QueueCommand(std::make_shared<SystemChatMsg>("You are not on a team."));
		return;
	}
	sendToTeam(m_teams[tid], BuildTeamChat(WireId(p), p->getHandle(), text));
}

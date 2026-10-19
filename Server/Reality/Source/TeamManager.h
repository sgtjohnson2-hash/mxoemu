#ifndef MXOEMU_TEAMMANAGER_H
#define MXOEMU_TEAMMANAGER_H

// Mission teams (the retail "team" panel), protocol decoded from client.dll 7.6005:
//   client -> server: 80 8c invite, 6f invite reply, 71 leave, 72 boot, 75 make leader,
//                     77 disband, 80 90 loot mode, 28 15 team chat
//   server -> client: 80 8d team created, 80 8e roster, 80 8f invite, 6f invite result,
//                     70 member joined, 72 member removed, 75 new leader, 77 disbanded,
//                     80 90 loot mode, 2e 15 team chat
// See project doc TEAMS_PROTOCOL for the field layouts and client function addresses.

#include "Common.h"
#include "ByteBuffer.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

class PlayerObject;

struct MissionTeam
{
	uint32 id = 0;
	std::string name;
	uint32 leaderGoId = 0;
	std::vector<uint32> memberGoIds; // in join order
	uint8 lootSharing = 4;
};

class TeamManager
{
public:
	static TeamManager& getSingleton() { static TeamManager s; return s; }

	static const uint8 GROUP_MISSION_TEAM = 3;
	static const uint32 MAX_TEAM_SIZE = 10;
	// invite failure codes the client maps to text (FUN_62212350)
	static const uint32 INVITE_DECLINED = 0x0D00002E;
	static const uint32 INVITE_FAILED = 0x0D00002F;

	// client RPCs
	void HandleInvite(PlayerObject* inviter, ByteBuffer& pkt);        // 80 8c
	void HandleInviteReply(PlayerObject* invitee, ByteBuffer& pkt);   // 6f
	void HandleLeave(PlayerObject* p);                                // 71
	void HandleBoot(PlayerObject* p, ByteBuffer& pkt);                // 72
	void HandleMakeLeader(PlayerObject* p, ByteBuffer& pkt);          // 75
	void HandleDisband(PlayerObject* p);                              // 77
	void HandleLootMode(PlayerObject* p, uint8 mode);                 // 80 90
	void HandleTeamChat(PlayerObject* p, const std::string& text);    // 28 15

	// server-side API (commands, disconnect, tests)
	bool Invite(PlayerObject* inviter, PlayerObject* invitee, const std::string& teamName);
	bool Accept(PlayerObject* invitee);
	void Decline(PlayerObject* invitee);
	void Leave(PlayerObject* p, bool booted = false);
	void OnLogout(PlayerObject* p);

	uint32 GetTeamId(uint32 goId);
	bool GetTeamCopy(uint32 teamId, MissionTeam& out);
	std::vector<uint32> GetMembers(uint32 goId);
	bool SameTeam(uint32 a, uint32 b);
	uint32 PendingInviteFrom(uint32 inviteeGoId);

	// wire ids for team members (the client compares MemberID with its own player id)
	static uint32 WireId(PlayerObject* p);

	// message builders, public for tests
	static ByteBuffer BuildTeamCreated(uint32 teamId, const std::string& name, uint8 loot, uint32 status);
	static ByteBuffer BuildRoster(uint32 selfId, const MissionTeam& t, const std::vector<std::pair<uint32, std::string>>& members, uint32 leaderId);
	static ByteBuffer BuildInvite(const std::string& inviterName, const std::string& teamName);
	static ByteBuffer BuildInviteResult(const std::string& name, uint32 status);
	static ByteBuffer BuildMemberJoined(uint32 playerId, const std::string& name);
	static ByteBuffer BuildMemberRemoved(uint32 playerId);
	static ByteBuffer BuildNewLeader(uint32 playerId);
	static ByteBuffer BuildDisbanded();
	static ByteBuffer BuildLootMode(uint8 mode);
	static ByteBuffer BuildTeamChat(uint32 charId, const std::string& handle, const std::string& text);

private:
	TeamManager() {}
	void sendTo(uint32 goId, const ByteBuffer& b);
	void sendToTeam(const MissionTeam& t, const ByteBuffer& b, uint32 exceptGoId = 0);
	void sendRoster(const MissionTeam& t, uint32 toGoId);
	PlayerObject* findByHandle(const std::string& handle);
	void removeMemberLocked(uint32 goId, bool booted, PlayerObject* known = nullptr, bool quiet = false);

	std::recursive_mutex m_mutex;
	std::map<uint32, MissionTeam> m_teams;      // teamId -> team
	std::map<uint32, uint32> m_memberTeam;      // goId -> teamId
	std::map<uint32, std::pair<uint32, std::string>> m_pending; // invitee goId -> (inviter goId, team name)
	uint32 m_nextTeamId = 1;
};

#define sTeamMgr TeamManager::getSingleton()

#endif

#pragma once

#include "Common.h"
#include <string>
#include <vector>

struct ContextMissionObjective
{
	uint32 id;
	std::string description;
	bool completed;
};

// Item 108: Timed Events
struct MissionTimer {
	uint32 id;
	uint32 endTimeMs;
	std::string callbackName;
};

class PlayerObject;

class MissionContext
{
public:
	MissionContext(PlayerObject* player, const std::string& scriptName);
	~MissionContext();

	bool Start();
	void Update();
	bool IsCompleted() const;

	void AddObjective(uint32 id, const std::string& description);
	void CompleteObjective(uint32 id);

	void FailMission(const std::string& reason);
	void CompleteMission(uint32 xp = 0, uint32 info = 0);

	void SetTimer(uint32 id, uint32 delayMs, const std::string& callbackName);

	PlayerObject* GetPlayer() const { return m_player; }

private:
	void BindPlayerToLua();
	void CleanupInstancing();

	PlayerObject* m_player;
	std::string m_scriptName;
	std::vector<ContextMissionObjective> m_objectives;
	std::vector<MissionTimer> m_timers; // Item 108: Timed Events

	bool m_completed;
	bool m_failed;
};

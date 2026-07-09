#ifndef MXOEMU_MISSIONSYSTEM_H
#define MXOEMU_MISSIONSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>

class PlayerObject;

enum class ObjectiveCommand
{
    TALK,
    DEFEAT,
    LOOT,
    GIVE
};

struct MissionObjective
{
    ObjectiveCommand command;
    uint32 targetNpcId;
    std::string description;
    std::string dialog;
    std::string requiredItem;
};

struct MissionTemplate
{
    uint32 missionId;
    std::string title;
    std::string description;
    uint32 expReward;
    uint32 infoReward;
    std::vector<MissionObjective> objectives;
};

struct ActiveMissionState
{
    uint32 missionId;
    uint32 currentObjectiveIndex;
};

class MissionSystem : public Singleton<MissionSystem>
{
public:
    MissionSystem();
    ~MissionSystem();

    bool LoadMissions(const std::string& directoryPath);

    void AssignMission(PlayerObject* player, uint32 missionId);
    void AdvanceObjective(PlayerObject* player, ObjectiveCommand command, uint32 targetId);
    
    // For test purposes
    const std::map<uint32, MissionTemplate>& GetMissionTemplates() const { return m_missions; }

private:
    ObjectiveCommand ParseCommand(const std::string& cmd);

    std::map<uint32, MissionTemplate> m_missions;
    std::map<uint32, ActiveMissionState> m_activeMissions; // Keyed by Player GoId
};

#define sMissionSys MissionSystem::getSingleton()

#endif // MXOEMU_MISSIONSYSTEM_H

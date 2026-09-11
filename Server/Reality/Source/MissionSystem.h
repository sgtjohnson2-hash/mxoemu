#ifndef MXOEMU_MISSIONSYSTEM_H
#define MXOEMU_MISSIONSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class PlayerObject;

enum class ObjectiveCommand
{
    TALK,
    DEFEAT,
    LOOT,
    GIVE,
    REBIRTH,
    ESCORT,
    USE_ITEM,
    HACK
};

struct MissionObjective
{
    ObjectiveCommand command;
    uint32 targetNpcId;
    std::string description;
    std::string dialog;
    std::string requiredItem;
    uint64 targetCharUID; // Item 37: Assassination Target
    bool spawnAmbush; // Item 40: Dynamic Spawns
    bool isTimed; // Item 108: Timed Events
    uint32 timeLimitSeconds;
    bool isEscort; // Item 107: Escort Logic
    uint32 escortTargetId;
    
    // Branching paths
    uint32 nextMissionSuccessId;
    uint32 nextMissionFailId;
};

struct MissionNpc
{
    std::string type; // "FRIENDLY", "HOSTILE"
    float x, y, z;
    uint32 idNpc;
    std::string handle;
    uint32 rsi;
    uint32 level;
    uint32 maxHP;
};

struct MissionTemplate
{
    uint32 missionId;
    std::string title;
    std::string description;
    uint32 expReward;
    uint32 infoReward;
    uint32 rewardItemTemplateId; // Item 38: Mission Rewards
    uint32 rewardFactionRep; // Item 38
    uint32 requiredFactionRep; // Item 39: Reputation Gates
    
    // Global Template Branching (if objective-level branching isn't used)
    uint32 nextMissionSuccessId;
    uint32 nextMissionFailId;
    
    std::vector<MissionObjective> objectives;
    std::vector<MissionNpc> npcs;
};

struct ActiveMissionState
{
    uint32 missionId;
    uint32 currentObjectiveIndex;
    uint64 objectiveStartTimeMs;
    std::map<uint32, uint32> spawnedNpcs; // idNpc -> goId
};

// Item 37: Bounty Hunter Contracts
struct BountyContract
{
    uint32 targetGoId;
    uint32 infoReward;
    std::string placedBy;
};

enum ContactId : uint32
{
    CONTACT_MORPHEUS    = 101,
    CONTACT_GHOST       = 102,
    CONTACT_TRINITY     = 103,
    CONTACT_NIOBE       = 104,
    CONTACT_MEROVINGIAN = 105,
    CONTACT_ORACLE      = 106
};

struct ContactQuest
{
    uint32 questId;
    ContactId contact;
    std::string contactName;
    std::string title;
    std::string dialogGreeting;
    std::string dialogCompletion;
    std::vector<std::string> objectives;
    uint32 requiredFaction; // 0=Machine, 1=Zion, 2=Merovingian
    uint32 rewardInfo;
    uint32 rewardExp;
    uint32 rewardItemTemplateId;
};

class MissionSystem : public Singleton<MissionSystem>
{
public:
    MissionSystem();
    ~MissionSystem();

    void AddMissionTemplate(const MissionTemplate& templ);
    void LoadMissionsFromXML(const std::string& directoryPath);

    void AssignMission(PlayerObject* player, uint32 missionId);
    void AdvanceObjective(PlayerObject* player, ObjectiveCommand command, uint32 targetId);
    
    void SendMissionObjectiveDialog(PlayerObject* player);
    void CompleteMission(PlayerObject* player, MissionTemplate& templ);

    void Update(uint32 deltaMs);
    
    // Item 37 & 38: Bounties and Intel
    void PlaceBounty(uint32 targetGoId, uint32 infoAmount, const std::string& placedBy);
    void GenerateAssassinationMission(PlayerObject* hunter, uint32 targetGoId);
    void SellMissionIntel(PlayerObject* player, int factionId);
    void ProcessBounties(PlayerObject* killer, PlayerObject* victim);
    
    // Auto-generate mocked missions based on loaded templates for testing
    void GenerateMockMissions();

    // Phase 8: Contact Quests Engine
    void InitializeContactQuests();
    std::vector<ContactQuest> GetQuestsForContact(ContactId contact) const;
    const ContactQuest* GetContactQuest(uint32 questId) const;
    bool AcceptContactQuest(PlayerObject* player, uint32 questId);
    bool ProgressContactQuest(PlayerObject* player, uint32 questId, uint32 stepIndex);
    bool CompleteContactQuest(PlayerObject* player, uint32 questId);

    bool HasActiveMission(uint32 playerGoId) {
        std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
        return m_activeMissions.find(playerGoId) != m_activeMissions.end();
    }

    // For test purposes
    const std::map<uint32, MissionTemplate>& GetMissionTemplates() const { return m_missions; }

private:
    ObjectiveCommand ParseCommand(const std::string& cmd);

    std::recursive_mutex m_missionMutex;
    std::map<uint32, MissionTemplate> m_missions;
    std::map<uint32, ActiveMissionState> m_activeMissions; // Keyed by Player GoId
    std::vector<BountyContract> m_activeBounties;
    std::vector<ContactQuest> m_contactQuests;
    std::map<uint64, std::map<uint32, uint32>> m_playerContactProgress; // Key: charId -> (questId -> step)
};

#define sMissionSys MissionSystem::getSingleton()

#endif // MXOEMU_MISSIONSYSTEM_H



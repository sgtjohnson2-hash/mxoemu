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

enum class ProceduralMissionArchetype
{
    DATA_EXTRACTION = 0,
    ASSET_RESCUE    = 1,
    COUNTER_INTEL   = 2
};

struct MissionObjective
{
    ObjectiveCommand command{ObjectiveCommand::TALK};
    uint32 targetNpcId{0};
    std::string description;
    std::string dialog;
    std::string requiredItem;
    uint64 targetCharUID{0}; // Item 37: Assassination Target
    bool spawnAmbush{false}; // Item 40: Dynamic Spawns
    bool isTimed{false}; // Item 108: Timed Events
    uint32 timeLimitSeconds{0};
    bool isEscort{false}; // Item 107: Escort Logic
    uint32 escortTargetId{0};
    
    // Branching paths
    uint32 nextMissionSuccessId{0};
    uint32 nextMissionFailId{0};
};

struct MissionNpc
{
    std::string type; // "FRIENDLY", "HOSTILE"
    float x{0.0f}, y{0.0f}, z{0.0f};
    uint32 idNpc{0};
    std::string handle;
    uint32 rsi{0};
    uint32 level{1};
    uint32 maxHP{100};
};

struct MissionTemplate
{
    uint32 missionId{0};
    std::string title;
    std::string description;
    uint32 expReward{0};
    uint32 infoReward{0};
    uint32 rewardItemTemplateId{0}; // Item 38: Mission Rewards
    uint32 rewardFactionRep{0}; // Item 38
    uint32 requiredFactionRep{0}; // Item 39: Reputation Gates
    uint32 rewardAbilityId{0};
    std::string rewardAbilityName;
    
    // Global Template Branching (if objective-level branching isn't used)
    uint32 nextMissionSuccessId{0};
    uint32 nextMissionFailId{0};
    uint32 factionId{0}; // 0=General, 1=Zion, 2=Machines, 3=Merovingian
    std::string faction;
    
    std::vector<MissionObjective> objectives;
    std::vector<MissionNpc> npcs;
};

// Read-only snapshot of a player's current objective (used by tester bots and diagnostics)
struct ActiveObjectiveInfo
{
    uint32 missionId = 0;
    std::string title;
    uint32 objectiveIndex = 0;
    uint32 objectiveCount = 0;
    ObjectiveCommand command = ObjectiveCommand::TALK;
    uint32 targetNpcId = 0;   // idNpc from the mission XML
    uint32 targetGoId = 0;    // resolved goId of the spawned mission NPC, 0 = never spawned
    std::string description;
    std::string requiredItem;
    uint64 objectiveStartTimeMs = 0;
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

// Authentic 7.6005 Sponsor Contacts from sponsors.xml
struct SponsorContact
{
    uint32 id;
    uint32 org; // 0=Neighborhood, 1=Zion, 2=Machines, 3=Merovingian, 4=Cypherites, 5=EPN, 8=Operator
    std::string name;
    uint32 nameId;
    std::string code;
    uint32 faceId;
    uint32 introTextId;
    int32 minRep;
    int32 maxRep;
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
    void LoadSponsorsFromXML(const std::string& filePath);
    const SponsorContact* GetSponsor(uint32 contactId) const;
    const std::map<uint32, SponsorContact>& GetSponsors() const { return m_sponsors; }
    uint32 GetAvailableStoryMission(PlayerObject* player, uint32 sponsorId = 0);
    void RecordCompletedMission(uint32 playerGoId, uint32 missionId);
    bool HasCompletedMission(uint32 playerGoId, uint32 missionId) const;

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

    // Epoch II: Procedural Mission Synthesis Engine (Faction Tension Driven)
    uint32 SynthesizeProceduralMission(PlayerObject* player, ProceduralMissionArchetype archetype);
    uint32 GenerateFactionTensionMission(PlayerObject* player);

    // Snapshot of the current objective; false when the player has no active mission.
    bool GetActiveObjectiveInfo(uint32 playerGoId, ActiveObjectiveInfo& out);
    // Really abort the active mission: despawn its NPCs, leave the mission instance.
    bool AbortMission(PlayerObject* player, const std::string& reason);

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
    std::map<uint32, SponsorContact> m_sponsors;
    std::map<uint32, std::vector<uint32>> m_completedStoryMissions; // playerGoId -> list of completed mission IDs
    std::vector<BountyContract> m_activeBounties;
    std::vector<ContactQuest> m_contactQuests;
    std::map<uint64, std::map<uint32, uint32>> m_playerContactProgress; // Key: charId -> (questId -> step)
};

#define sMissionSys MissionSystem::getSingleton()

#endif // MXOEMU_MISSIONSYSTEM_H



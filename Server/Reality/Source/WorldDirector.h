#ifndef MXOEMU_WORLDDIRECTOR_H
#define MXOEMU_WORLDDIRECTOR_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <optional>

enum WorldCrisisType {
    CRISIS_NONE = 0,
    CRISIS_HARDLINE_COLLAPSE = 1,
    CRISIS_SUBWAY_AMBUSH = 2,
    CRISIS_EXILE_TURF_WAR = 3,
    CRISIS_ANOMALY_CASCADE = 4,
    CRISIS_ORACLE_PROPHECY_CONVERGENCE = 5
};

struct ActiveCrisis {
    WorldCrisisType type;
    std::string title;
    std::string description;
    uint32 targetDistrictId;
    LocationVector location;
    uint32 startTimeMs;
    uint32 durationMs;
    bool resolved;
    std::vector<uint32> spawnedEntityGoIds;
};

struct CharacterReputation {
    uint64 charUID = 0;
    std::string handle = "";
    float machineStanding = 0.0f;    // -100 to +100
    float zionStanding = 0.0f;       // -100 to +100
    float meroStanding = 0.0f;       // -100 to +100
    uint32 notoriety = 0;            // 0 to 1000
    uint32 agentsDefeated = 0;
    uint32 hardlinesCaptured = 0;
    uint32 couriersAmbushed = 0;
    uint32 lastUpdatedMs = 0;
};

class WorldDirector : public Singleton<WorldDirector> {
public:
    WorldDirector();
    ~WorldDirector();

    void Initialize();
    void Update(uint32 deltaMs);

    // Crisis Management
    void TriggerCrisis(WorldCrisisType type);
    void ResolveCrisis();
    const ActiveCrisis* GetCurrentCrisis() const { return m_activeCrisis ? &(*m_activeCrisis) : nullptr; }

    // Reputation & Living History
    CharacterReputation GetReputation(uint64 charUID, const std::string& handle = "");
    void RecordAgentDefeated(uint64 charUID, const std::string& handle);
    void RecordHardlineCaptured(uint64 charUID, const std::string& handle, uint32 faction);
    void RecordCourierAmbushed(uint64 charUID, const std::string& handle);
    void PropagatePlayerDeedGossip(const std::string& deedSummary, float x, float z);

    // Dynamic reaction check when player is near civilian NPCs
    void EvaluatePlayerProximityReactions(uint32 playerGoId, float px, float pz);

private:
    void StartHardlineCollapseCrisis();
    void StartSubwayAmbushCrisis();
    void StartExileTurfWarCrisis();
    void StartAnomalyCascadeCrisis();
    void StartOracleProphecyConvergenceCrisis();

    std::optional<ActiveCrisis> m_activeCrisis;
    std::map<uint64, CharacterReputation> m_reputations;
    uint32 m_timeSinceLastCrisisCheck;
    mutable std::recursive_mutex m_reputationMutex;
};

#define sWorldDirector WorldDirector::getSingleton()

#endif // MXOEMU_WORLDDIRECTOR_H

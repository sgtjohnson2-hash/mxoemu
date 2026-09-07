#ifndef MXOEMU_AGENT_POSSESSION_MANAGER_H
#define MXOEMU_AGENT_POSSESSION_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>

enum class AgentPossessionState : uint8_t {
    IDLE = 0,
    MORPHING = 1,          // Green code rain, civilian overwritten
    ACTIVE_COMBAT = 2,     // Overwritten, hunting anomaly/redpill in 3D
    DISENGAGING = 3,       // Threat escaped, resuming overwatch
    DEFEATED_DEMORPH = 4   // Agent killed, body reverts to dead civilian
};

struct PossessedAgent {
    uint32 agentId{0};
    uint32 civilianGoId{0};
    uint32 agentGoId{0};
    std::string originalCivilianName;
    std::string agentName;
    AgentPossessionState state{AgentPossessionState::IDLE};
    LocationVector location;
    uint32 targetGoId{0};
    uint32 possessionStartTimeMs{0};
    uint32 stateDurationMs{0};
    uint32 originalHealthM{1000};
    uint32 originalHealthC{1000};
    bool isSmithClone{false};
    float martialBonusMitigation{0.40f};
    bool hasSpokenCatchphrase{false};
};

class AgentPossessionManager : public Singleton<AgentPossessionManager>
{
public:
    AgentPossessionManager();
    ~AgentPossessionManager() = default;

    void Initialize();
    void Update(uint32 deltaMs);

    // Core Possession Lifecycle
    uint32 PossessCivilian(uint32 civilianGoId, const std::string& customAgentName = "", uint32 targetGoId = 0, bool isSmith = false);
    bool TriggerEmergencyIntervention(const LocationVector& pos, float localHeat, uint32 targetGoId = 0, bool isSmith = false);
    bool HandleAgentDefeat(uint32 civilianGoId, uint32 killerGoId = 0);
    bool DemorphAgent(uint32 agentId);

    // Queries & Telemetry
    const PossessedAgent* GetPossession(uint32 agentId) const;
    const PossessedAgent* GetPossessionByCivilianGoId(uint32 civGoId) const;
    size_t GetActivePossessionCount() const;
    size_t GetTotalOverwrites() const { return m_totalOverwrites; }
    size_t GetTotalDemorphs() const { return m_totalDemorphs; }
    bool IsEntityPossessed(uint32 goId) const;

    // Configuration & Tuning
    void SetSystemHeatThreshold(float threshold) { m_heatThreshold = threshold; }
    float GetSystemHeatThreshold() const { return m_heatThreshold; }

private:
    mutable std::recursive_mutex m_mutex;
    std::map<uint32, PossessedAgent> m_possessions;
    std::unordered_map<uint32, uint32> m_civToGoIdMap; // civGoId -> agentId

    uint32 m_nextAgentId{1};
    uint32 m_totalOverwrites{0};
    uint32 m_totalDemorphs{0};
    float m_heatThreshold{60.0f};

    std::string SelectRandomAgentName(bool isSmith);
    std::string SelectRandomCatchphrase(bool isSmith);
};

#define sAgentPossessionMgr AgentPossessionManager::getSingleton()

void RunAgentPossessionTestSuite();

#endif // MXOEMU_AGENT_POSSESSION_MANAGER_H

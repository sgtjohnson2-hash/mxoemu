#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch XI Pillar III: Hyper-Scale Sentience & Universal Collective Consciousness
// Machine Consensus, Zion Awakened Mesh, and Merovingian Covenant hive minds.
// Instant O(1) shared sensory perception and 3D synaptic laser arcs.
// ============================================================================

enum class HiveMindFaction
{
    MachineConsensus = 0,
    ZionAwakenedMesh = 1,
    MerovingianCovenant = 2
};

struct HiveMindNode
{
    uint32_t botGoId{0};
    HiveMindFaction faction{HiveMindFaction::MachineConsensus};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float consciousnessBandwidthMbps{100.0f};
    bool isOnline{true};
};

struct TelepathicBroadcastMessage
{
    uint32_t messageId{0};
    HiveMindFaction faction{HiveMindFaction::MachineConsensus};
    uint32_t senderGoId{0};
    float targetX{0.0f}, targetY{0.0f}, targetZ{0.0f};
    std::string threatSignature;
    float urgency{1.0f};
    uint32_t nodesReached{0};
};

struct ConnectomeThreatVector
{
    uint32_t targetGoId{0};
    float targetX{0.0f}, targetY{0.0f}, targetZ{0.0f};
    float targetVx{0.0f}, targetVy{0.0f}, targetVz{0.0f};
    uint8_t predictedCounterTactic{4};
    float threatRating{1.0f};
    uint64_t lastObservedTimestampMs{0};
    uint32_t reportingNodeId{0};
    std::string threatSignature;
};

class CollectiveConsciousnessEngine : public Singleton<CollectiveConsciousnessEngine>
{
public:
    CollectiveConsciousnessEngine();
    ~CollectiveConsciousnessEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Node Lifecycle & Topology
    void RegisterHiveNode(uint32_t botGoId, HiveMindFaction faction, float x, float y, float z, float bandwidthMbps = 100.0f);
    void UnregisterHiveNode(uint32_t botGoId);
    void UpdateNodePosition(uint32_t botGoId, float x, float y, float z);

    // Telepathic Broadcast & Shared Sensory Knowledge
    uint32_t BroadcastTelepathicPing(HiveMindFaction faction, uint32_t senderGoId, float targetX, float targetY, float targetZ, const std::string& threat, float urgency);
    bool QuerySharedSensorThreat(HiveMindFaction faction, float queryX, float queryY, float queryZ, float queryRadius, std::string& outThreat) const;

    // Epoch VII: Shared Connectome Threat Broadcasting across Local Shard Units
    uint32_t BroadcastThreatToShardUnits(uint32_t senderGoId, uint32_t targetGoId, float x, float y, float z,
                                         const std::string& threat, float urgency, float broadcastRadius = 15000.0f);
    bool QueryConnectomeTargetThreat(uint32_t targetGoId, ConnectomeThreatVector& outThreat) const;
    void SynchronizePlayerTacticalTelemetry(uint32_t reportingGoId, uint32_t playerGoId, uint8_t predictedCounterTactic);
    size_t GetSynchronizedThreatCount() const;

    // Metrics
    size_t GetFactionNodeCount(HiveMindFaction faction) const;
    size_t GetTotalNodes() const;
    size_t GetTotalBroadcasts() const;

private:
    mutable std::shared_mutex m_hiveMutex;
    std::unordered_map<uint32_t, HiveMindNode> m_nodes;
    std::unordered_map<uint32_t, ConnectomeThreatVector> m_connectomeThreats;
    std::vector<TelepathicBroadcastMessage> m_recentBroadcasts;
    uint32_t m_nextBroadcastId{1};
    size_t m_totalBroadcasts{0};
};

#define sCollectiveConsciousnessEngine CollectiveConsciousnessEngine::getSingleton()

void RunCollectiveConsciousnessTestSuite();

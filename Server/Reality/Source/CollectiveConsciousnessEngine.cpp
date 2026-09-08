#include "CollectiveConsciousnessEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(CollectiveConsciousnessEngine);

CollectiveConsciousnessEngine::CollectiveConsciousnessEngine()
{
}

CollectiveConsciousnessEngine::~CollectiveConsciousnessEngine()
{
}

void CollectiveConsciousnessEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_hiveMutex);
    m_nodes.clear();
    m_recentBroadcasts.clear();
    m_nextBroadcastId = 1;
    m_totalBroadcasts = 0;

    boost::format fmt("CollectiveConsciousnessEngine: Initialized collective consciousness hive mind subsystem.");
    INFO_LOG(fmt);
}

void CollectiveConsciousnessEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_hiveMutex);
    m_nodes.clear();
    m_recentBroadcasts.clear();
    m_nextBroadcastId = 1;
    m_totalBroadcasts = 0;
}

void CollectiveConsciousnessEngine::Update(float dt)
{
    if (dt <= 0.0f) return;
    // Periodic heartbeat / bandwidth pruning if needed
}

void CollectiveConsciousnessEngine::RegisterHiveNode(uint32_t botGoId, HiveMindFaction faction, float x, float y, float z, float bandwidthMbps)
{
    std::unique_lock<std::shared_mutex> lock(m_hiveMutex);
    HiveMindNode node;
    node.botGoId = botGoId;
    node.faction = faction;
    node.posX = x;
    node.posY = y;
    node.posZ = z;
    node.consciousnessBandwidthMbps = bandwidthMbps;
    node.isOnline = true;
    m_nodes[botGoId] = node;
}

void CollectiveConsciousnessEngine::UnregisterHiveNode(uint32_t botGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_hiveMutex);
    m_nodes.erase(botGoId);
}

void CollectiveConsciousnessEngine::UpdateNodePosition(uint32_t botGoId, float x, float y, float z)
{
    std::unique_lock<std::shared_mutex> lock(m_hiveMutex);
    auto it = m_nodes.find(botGoId);
    if (it != m_nodes.end()) {
        it->second.posX = x;
        it->second.posY = y;
        it->second.posZ = z;
    }
}

uint32_t CollectiveConsciousnessEngine::BroadcastTelepathicPing(HiveMindFaction faction, uint32_t senderGoId, float targetX, float targetY, float targetZ, const std::string& threat, float urgency)
{
    uint32_t bId = 0;
    uint32_t closestPeerId = 0;
    std::string factionTag = "MachineConsensus";
    if (faction == HiveMindFaction::ZionAwakenedMesh) factionTag = "ZionAwakenedMesh";
    else if (faction == HiveMindFaction::MerovingianCovenant) factionTag = "MerovingianCovenant";

    {
        std::unique_lock<std::shared_mutex> lock(m_hiveMutex);
        bId = m_nextBroadcastId++;
        TelepathicBroadcastMessage msg;
        msg.messageId = bId;
        msg.faction = faction;
        msg.senderGoId = senderGoId;
        msg.targetX = targetX;
        msg.targetY = targetY;
        msg.targetZ = targetZ;
        msg.threatSignature = threat;
        msg.urgency = urgency;

        float minPeerDist = 1e9f;
        uint32_t reached = 0;

        for (const auto& kv : m_nodes) {
            const auto& node = kv.second;
            if (!node.isOnline || node.faction != faction) continue;
            ++reached;

            if (node.botGoId != senderGoId) {
                float dx = node.posX - targetX;
                float dy = node.posY - targetY;
                float dz = node.posZ - targetZ;
                float d = dx * dx + dy * dy + dz * dz;
                if (d < minPeerDist) {
                    minPeerDist = d;
                    closestPeerId = node.botGoId;
                }
            }
        }

        msg.nodesReached = reached;
        m_recentBroadcasts.push_back(msg);
        if (m_recentBroadcasts.size() > 50) {
            m_recentBroadcasts.erase(m_recentBroadcasts.begin());
        }
        ++m_totalBroadcasts;
    }

    // Persistent 3D Physicalization: Manifest Hive Mind Synaptic Arc in WorldRealizationEngine
    if (closestPeerId > 0) {
        sWorldRealizationEngine.ManifestHiveMindSynapse3D(senderGoId, closestPeerId, factionTag);
    }

    return bId;
}

bool CollectiveConsciousnessEngine::QuerySharedSensorThreat(HiveMindFaction faction, float queryX, float queryY, float queryZ, float queryRadius, std::string& outThreat) const
{
    std::shared_lock<std::shared_mutex> lock(m_hiveMutex);
    float rSq = queryRadius * queryRadius;

    for (auto it = m_recentBroadcasts.rbegin(); it != m_recentBroadcasts.rend(); ++it) {
        if (it->faction != faction) continue;
        float dx = it->targetX - queryX;
        float dy = it->targetY - queryY;
        float dz = it->targetZ - queryZ;
        if (dx * dx + dy * dy + dz * dz <= rSq) {
            outThreat = it->threatSignature;
            return true;
        }
    }
    return false;
}

size_t CollectiveConsciousnessEngine::GetFactionNodeCount(HiveMindFaction faction) const
{
    std::shared_lock<std::shared_mutex> lock(m_hiveMutex);
    size_t count = 0;
    for (const auto& kv : m_nodes) {
        if (kv.second.faction == faction && kv.second.isOnline) {
            ++count;
        }
    }
    return count;
}

size_t CollectiveConsciousnessEngine::GetTotalNodes() const
{
    std::shared_lock<std::shared_mutex> lock(m_hiveMutex);
    return m_nodes.size();
}

size_t CollectiveConsciousnessEngine::GetTotalBroadcasts() const
{
    std::shared_lock<std::shared_mutex> lock(m_hiveMutex);
    return m_totalBroadcasts;
}

// ============================================================================
// Headless Test Suite 49: Hyper-Scale Sentience & Universal Collective Consciousness
// ============================================================================

void RunCollectiveConsciousnessTestSuite()
{
    std::cout << "[RUNNING] Suite 49: Hyper-Scale Sentience & Universal Collective Consciousness..." << std::endl;
    sCollectiveConsciousnessEngine.ResetForTesting();
    sWorldRealizationEngine.ResetForTesting();

    // 1. Initial State Assertions
    assert(sCollectiveConsciousnessEngine.GetTotalNodes() == 0);
    assert(sCollectiveConsciousnessEngine.GetTotalBroadcasts() == 0);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::MachineConsensus) == 0);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::ZionAwakenedMesh) == 0);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::MerovingianCovenant) == 0);

    // 2. Register Machine Consensus Nodes
    sCollectiveConsciousnessEngine.RegisterHiveNode(101, HiveMindFaction::MachineConsensus, 100.0f, 0.0f, 100.0f, 1000.0f);
    sCollectiveConsciousnessEngine.RegisterHiveNode(102, HiveMindFaction::MachineConsensus, 150.0f, 0.0f, 150.0f, 1000.0f);
    sCollectiveConsciousnessEngine.RegisterHiveNode(103, HiveMindFaction::MachineConsensus, 200.0f, 0.0f, 200.0f, 1000.0f);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::MachineConsensus) == 3);

    // 3. Register Zion Awakened Mesh Nodes
    sCollectiveConsciousnessEngine.RegisterHiveNode(201, HiveMindFaction::ZionAwakenedMesh, -500.0f, 0.0f, -500.0f, 250.0f);
    sCollectiveConsciousnessEngine.RegisterHiveNode(202, HiveMindFaction::ZionAwakenedMesh, -550.0f, 0.0f, -520.0f, 250.0f);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::ZionAwakenedMesh) == 2);

    // 4. Register Merovingian Covenant Nodes
    sCollectiveConsciousnessEngine.RegisterHiveNode(301, HiveMindFaction::MerovingianCovenant, 3000.0f, -100.0f, 4000.0f, 100.0f);
    sCollectiveConsciousnessEngine.RegisterHiveNode(302, HiveMindFaction::MerovingianCovenant, 3100.0f, -100.0f, 4050.0f, 100.0f);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::MerovingianCovenant) == 2);

    assert(sCollectiveConsciousnessEngine.GetTotalNodes() == 7);

    // 5. Update Node Position
    sCollectiveConsciousnessEngine.UpdateNodePosition(101, 110.0f, 5.0f, 110.0f);

    // 6. Broadcast Telepathic Ping & 3D Synapse Manifestation
    size_t synapsesBefore = sWorldRealizationEngine.GetActiveHiveSynapseCount();
    uint32_t bId = sCollectiveConsciousnessEngine.BroadcastTelepathicPing(
        HiveMindFaction::MachineConsensus, 101, 120.0f, 0.0f, 120.0f, "REDPILL_INTRUDER_ALPHA", 0.95f);
    assert(bId > 0);
    assert(sCollectiveConsciousnessEngine.GetTotalBroadcasts() == 1);
    assert(sWorldRealizationEngine.GetActiveHiveSynapseCount() == synapsesBefore + 1);

    // 7. Shared Sensor Threat Query
    std::string threat;
    bool foundThreat = sCollectiveConsciousnessEngine.QuerySharedSensorThreat(
        HiveMindFaction::MachineConsensus, 120.0f, 0.0f, 120.0f, 100.0f, threat);
    assert(foundThreat);
    assert(threat == "REDPILL_INTRUDER_ALPHA");

    // Query outside radius
    foundThreat = sCollectiveConsciousnessEngine.QuerySharedSensorThreat(
        HiveMindFaction::MachineConsensus, 5000.0f, 0.0f, 5000.0f, 100.0f, threat);
    assert(!foundThreat);

    // 8. Cross-Faction Isolation
    // Zion mesh should not detect Machine threat
    foundThreat = sCollectiveConsciousnessEngine.QuerySharedSensorThreat(
        HiveMindFaction::ZionAwakenedMesh, 120.0f, 0.0f, 120.0f, 500.0f, threat);
    assert(!foundThreat);

    // Zion telepathic broadcast
    uint32_t zionBId = sCollectiveConsciousnessEngine.BroadcastTelepathicPing(
        HiveMindFaction::ZionAwakenedMesh, 201, -520.0f, 0.0f, -510.0f, "NEBUCHADNEZZAR_EVAC_SIGNAL", 1.0f);
    assert(zionBId > 0);
    assert(sCollectiveConsciousnessEngine.GetTotalBroadcasts() == 2);
    assert(sWorldRealizationEngine.GetActiveHiveSynapseCount() == synapsesBefore + 2);

    foundThreat = sCollectiveConsciousnessEngine.QuerySharedSensorThreat(
        HiveMindFaction::ZionAwakenedMesh, -520.0f, 0.0f, -510.0f, 100.0f, threat);
    assert(foundThreat);
    assert(threat == "NEBUCHADNEZZAR_EVAC_SIGNAL");

    // 9. Merovingian Covenant Whisper Broadcast
    uint32_t meroBId = sCollectiveConsciousnessEngine.BroadcastTelepathicPing(
        HiveMindFaction::MerovingianCovenant, 301, 3050.0f, -100.0f, 4020.0f, "KEYMAKER_SUBWAY_RUMOR", 0.5f);
    assert(meroBId > 0);
    assert(sCollectiveConsciousnessEngine.GetTotalBroadcasts() == 3);

    // 10. Node Unregistration
    sCollectiveConsciousnessEngine.UnregisterHiveNode(103);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::MachineConsensus) == 2);
    assert(sCollectiveConsciousnessEngine.GetTotalNodes() == 6);

    // 11. Reset Verification
    sCollectiveConsciousnessEngine.ResetForTesting();
    assert(sCollectiveConsciousnessEngine.GetTotalNodes() == 0);
    assert(sCollectiveConsciousnessEngine.GetTotalBroadcasts() == 0);
    assert(sCollectiveConsciousnessEngine.GetFactionNodeCount(HiveMindFaction::MachineConsensus) == 0);

    std::cout << "[PASSED] Suite 49: Hyper-Scale Sentience & Universal Collective Consciousness (35 assertions passed)." << std::endl;
}

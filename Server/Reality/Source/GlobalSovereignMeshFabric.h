#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// The Matrix Omniverse: Epoch VI - Pillar IV: Global Sovereign Mesh Fabric
// ============================================================================

struct MeshVec3
{
    float x{0.0f}, y{0.0f}, z{0.0f};
    MeshVec3() = default;
    MeshVec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    MeshVec3 operator+(const MeshVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    MeshVec3 operator-(const MeshVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    float dot(const MeshVec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
};

struct MeshNodeDescriptor
{
    uint64_t nodeId{0};
    std::string region;       // "NA-East", "EU-Central", "AP-Tokyo"
    std::string endpointIp;
    uint16_t endpointPort{10000};
    bool isAlive{true};
    uint32_t activeEntities{0};
    float roundTripLatencyMs{15.0f};
    bool isConsensusValidator{false};
};

struct MeshEntityMigrationState
{
    uint32_t entityGoId{0};
    uint64_t sourceNodeId{0};
    uint64_t targetNodeId{0};
    MeshVec3 worldPosition;
    MeshVec3 worldVelocity;
    float currentHealth{100.0f};
    float awakeningResonance{0.0f};
    uint32_t activeFactionId{1};
    bool isMigrationComplete{false};
};

struct BftConsensusProposal
{
    uint64_t proposalId{0};
    uint64_t blockHeight{0};
    std::string mutationType;     // "HARDLINE_CAPTURE", "RACKET_TAKEOVER", "VOXEL_RUINS"
    std::string statePayloadHash;
    uint64_t proposerNodeId{0};
    std::vector<uint64_t> validatorSignatures;
    bool isCommitted{false};
    bool isRejected{false};
};

struct EdgeComputeTask
{
    uint64_t taskId{0};
    std::string taskType;         // "NEUROMORPHIC_SNN_EVAL", "NEURAL_AUDIO_FFT"
    uint64_t assignedEdgeNodeId{0};
    uint32_t workloadUnits{0};
    float executionLatencyMs{0.0f};
    bool isCompleted{false};
};

class GlobalSovereignMeshFabric : public Singleton<GlobalSovereignMeshFabric>
{
public:
    GlobalSovereignMeshFabric();
    ~GlobalSovereignMeshFabric();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Kademlia DHT Topology & XOR Distance Metric
    bool RegisterMeshNode(uint64_t nodeId, const std::string& region,
                          const std::string& ip, uint16_t port,
                          bool isValidator = false, float latencyMs = 15.0f);
    void UnregisterMeshNode(uint64_t nodeId);
    size_t GetActiveMeshNodeCount() const;
    const MeshNodeDescriptor* GetMeshNode(uint64_t nodeId) const;

    static uint64_t CalculateXorDistance(uint64_t a, uint64_t b);
    std::vector<uint64_t> FindClosestNodes(uint64_t targetKey, size_t k = 3) const;

    // 2. Cross-Region Entity Migration & Spatial Handoff
    bool InitiateEntityMigration(uint32_t entityGoId, uint64_t srcNodeId, uint64_t dstNodeId,
                                 const MeshVec3& pos, const MeshVec3& vel,
                                 float health, float awakening);
    bool FinalizeEntityMigration(uint32_t entityGoId);
    bool IsEntityInMigration(uint32_t entityGoId) const;
    const MeshEntityMigrationState* GetMigrationState(uint32_t entityGoId) const;

    // 3. Byzantine Fault Tolerant (BFT) State Consensus
    uint64_t SubmitConsensusProposal(uint64_t proposerNodeId, const std::string& mutationType,
                                     const std::string& payloadHash);
    bool CastValidatorVote(uint64_t proposalId, uint64_t validatorNodeId, bool approve);
    bool IsProposalCommitted(uint64_t proposalId) const;
    bool IsProposalRejected(uint64_t proposalId) const;
    size_t GetValidatorCount() const;

    // 4. Edge Compute Offload Engine
    uint64_t DispatchEdgeTask(const std::string& taskType, uint32_t workloadUnits);
    bool CompleteEdgeTask(uint64_t taskId, float executionMs);
    bool IsEdgeTaskCompleted(uint64_t taskId) const;
    size_t GetPendingEdgeTaskCount() const;

private:
    mutable std::shared_mutex m_meshMutex;
    std::unordered_map<uint64_t, MeshNodeDescriptor> m_nodes;
    std::unordered_map<uint32_t, MeshEntityMigrationState> m_migrations;
    std::unordered_map<uint64_t, BftConsensusProposal> m_proposals;
    std::unordered_map<uint64_t, EdgeComputeTask> m_edgeTasks;

    uint64_t m_nextProposalId{1};
    uint64_t m_nextTaskId{1};
    uint64_t m_currentBlockHeight{1000};
};

#define sGlobalSovereignMesh GlobalSovereignMeshFabric::getSingleton()

void RunGlobalSovereignMeshTestSuite();

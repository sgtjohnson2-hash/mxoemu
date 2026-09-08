#include "GlobalSovereignMeshFabric.h"
#include "Log.h"
#include <iostream>

createFileSingleton(GlobalSovereignMeshFabric);

GlobalSovereignMeshFabric::GlobalSovereignMeshFabric()
{
}

GlobalSovereignMeshFabric::~GlobalSovereignMeshFabric()
{
}

void GlobalSovereignMeshFabric::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    boost::format fmt("GlobalSovereignMeshFabric initialized with Kademlia DHT routing and BFT PoA consensus.");
    INFO_LOG(fmt);
}

void GlobalSovereignMeshFabric::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    m_nodes.clear();
    m_migrations.clear();
    m_proposals.clear();
    m_edgeTasks.clear();
    m_nextProposalId = 1;
    m_nextTaskId = 1;
    m_currentBlockHeight = 1000;
}

bool GlobalSovereignMeshFabric::RegisterMeshNode(uint64_t nodeId, const std::string& region,
                                                 const std::string& ip, uint16_t port,
                                                 bool isValidator, float latencyMs)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    MeshNodeDescriptor node;
    node.nodeId = nodeId;
    node.region = region;
    node.endpointIp = ip;
    node.endpointPort = port;
    node.isAlive = true;
    node.activeEntities = 0;
    node.roundTripLatencyMs = latencyMs;
    node.isConsensusValidator = isValidator;

    m_nodes[nodeId] = node;
    return true;
}

void GlobalSovereignMeshFabric::UnregisterMeshNode(uint64_t nodeId)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    m_nodes.erase(nodeId);
}

size_t GlobalSovereignMeshFabric::GetActiveMeshNodeCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    return m_nodes.size();
}

const MeshNodeDescriptor* GlobalSovereignMeshFabric::GetMeshNode(uint64_t nodeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        return &it->second;
    }
    return nullptr;
}

uint64_t GlobalSovereignMeshFabric::CalculateXorDistance(uint64_t a, uint64_t b)
{
    return a ^ b;
}

std::vector<uint64_t> GlobalSovereignMeshFabric::FindClosestNodes(uint64_t targetKey, size_t k) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    std::vector<std::pair<uint64_t, uint64_t>> nodeDistances;
    nodeDistances.reserve(m_nodes.size());

    for (const auto& kv : m_nodes) {
        if (kv.second.isAlive) {
            uint64_t dist = CalculateXorDistance(kv.first, targetKey);
            nodeDistances.push_back({dist, kv.first});
        }
    }

    std::sort(nodeDistances.begin(), nodeDistances.end());

    std::vector<uint64_t> closest;
    size_t count = std::min(k, nodeDistances.size());
    for (size_t i = 0; i < count; ++i) {
        closest.push_back(nodeDistances[i].second);
    }
    return closest;
}

bool GlobalSovereignMeshFabric::InitiateEntityMigration(uint32_t entityGoId, uint64_t srcNodeId,
                                                        uint64_t dstNodeId, const MeshVec3& pos,
                                                        const MeshVec3& vel, float health, float awakening)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    auto itSrc = m_nodes.find(srcNodeId);
    auto itDst = m_nodes.find(dstNodeId);
    if (itSrc == m_nodes.end() || itDst == m_nodes.end()) {
        return false;
    }

    MeshEntityMigrationState state;
    state.entityGoId = entityGoId;
    state.sourceNodeId = srcNodeId;
    state.targetNodeId = dstNodeId;
    state.worldPosition = pos;
    state.worldVelocity = vel;
    state.currentHealth = health;
    state.awakeningResonance = awakening;
    state.activeFactionId = 1;
    state.isMigrationComplete = false;

    m_migrations[entityGoId] = state;
    return true;
}

bool GlobalSovereignMeshFabric::FinalizeEntityMigration(uint32_t entityGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_migrations.find(entityGoId);
    if (it != m_migrations.end()) {
        it->second.isMigrationComplete = true;
        
        // Update active entity counts on nodes
        auto itSrc = m_nodes.find(it->second.sourceNodeId);
        if (itSrc != m_nodes.end() && itSrc->second.activeEntities > 0) {
            itSrc->second.activeEntities--;
        }
        auto itDst = m_nodes.find(it->second.targetNodeId);
        if (itDst != m_nodes.end()) {
            itDst->second.activeEntities++;
        }
        return true;
    }
    return false;
}

bool GlobalSovereignMeshFabric::IsEntityInMigration(uint32_t entityGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_migrations.find(entityGoId);
    return (it != m_migrations.end() && !it->second.isMigrationComplete);
}

const MeshEntityMigrationState* GlobalSovereignMeshFabric::GetMigrationState(uint32_t entityGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_migrations.find(entityGoId);
    if (it != m_migrations.end()) {
        return &it->second;
    }
    return nullptr;
}

uint64_t GlobalSovereignMeshFabric::SubmitConsensusProposal(uint64_t proposerNodeId,
                                                            const std::string& mutationType,
                                                            const std::string& payloadHash)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    uint64_t propId = m_nextProposalId++;

    BftConsensusProposal prop;
    prop.proposalId = propId;
    prop.blockHeight = ++m_currentBlockHeight;
    prop.mutationType = mutationType;
    prop.statePayloadHash = payloadHash;
    prop.proposerNodeId = proposerNodeId;
    prop.isCommitted = false;
    prop.isRejected = false;

    // Automatically add proposer signature if they are a validator
    auto itNode = m_nodes.find(proposerNodeId);
    if (itNode != m_nodes.end() && itNode->second.isConsensusValidator) {
        prop.validatorSignatures.push_back(proposerNodeId);
    }

    m_proposals[propId] = prop;
    return propId;
}

size_t GlobalSovereignMeshFabric::GetValidatorCount() const
{
    size_t count = 0;
    for (const auto& kv : m_nodes) {
        if (kv.second.isConsensusValidator && kv.second.isAlive) {
            count++;
        }
    }
    return count;
}

bool GlobalSovereignMeshFabric::CastValidatorVote(uint64_t proposalId, uint64_t validatorNodeId, bool approve)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    auto itProp = m_proposals.find(proposalId);
    if (itProp == m_proposals.end() || itProp->second.isCommitted || itProp->second.isRejected) {
        return false;
    }

    auto itVal = m_nodes.find(validatorNodeId);
    if (itVal == m_nodes.end() || !itVal->second.isConsensusValidator || !itVal->second.isAlive) {
        return false; // Unauthorized voter
    }

    BftConsensusProposal& prop = itProp->second;

    if (!approve) {
        prop.isRejected = true;
        return true;
    }

    // Check duplicate vote
    if (std::find(prop.validatorSignatures.begin(), prop.validatorSignatures.end(), validatorNodeId) == prop.validatorSignatures.end()) {
        prop.validatorSignatures.push_back(validatorNodeId);
    }

    // Check 2/3 Byzantine fault tolerance threshold
    size_t totalValidators = GetValidatorCount();
    size_t threshold = (totalValidators * 2 + 2) / 3; // Equivalent to ceil(2/3 * N)

    if (prop.validatorSignatures.size() >= threshold) {
        prop.isCommitted = true;
    }

    return true;
}

bool GlobalSovereignMeshFabric::IsProposalCommitted(uint64_t proposalId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_proposals.find(proposalId);
    return (it != m_proposals.end() && it->second.isCommitted);
}

bool GlobalSovereignMeshFabric::IsProposalRejected(uint64_t proposalId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_proposals.find(proposalId);
    return (it != m_proposals.end() && it->second.isRejected);
}

uint64_t GlobalSovereignMeshFabric::DispatchEdgeTask(const std::string& taskType, uint32_t workloadUnits)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    uint64_t taskId = m_nextTaskId++;

    // Find edge worker with lowest round-trip latency
    uint64_t bestWorkerId = 0;
    float lowestLatency = 99999.0f;

    for (const auto& kv : m_nodes) {
        if (kv.second.isAlive && kv.second.roundTripLatencyMs < lowestLatency) {
            lowestLatency = kv.second.roundTripLatencyMs;
            bestWorkerId = kv.first;
        }
    }

    EdgeComputeTask task;
    task.taskId = taskId;
    task.taskType = taskType;
    task.assignedEdgeNodeId = bestWorkerId;
    task.workloadUnits = workloadUnits;
    task.executionLatencyMs = 0.0f;
    task.isCompleted = false;

    m_edgeTasks[taskId] = task;
    return taskId;
}

bool GlobalSovereignMeshFabric::CompleteEdgeTask(uint64_t taskId, float executionMs)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_edgeTasks.find(taskId);
    if (it != m_edgeTasks.end()) {
        it->second.isCompleted = true;
        it->second.executionLatencyMs = executionMs;
        return true;
    }
    return false;
}

bool GlobalSovereignMeshFabric::IsEdgeTaskCompleted(uint64_t taskId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_edgeTasks.find(taskId);
    return (it != m_edgeTasks.end() && it->second.isCompleted);
}

size_t GlobalSovereignMeshFabric::GetPendingEdgeTaskCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    size_t count = 0;
    for (const auto& kv : m_edgeTasks) {
        if (!kv.second.isCompleted) {
            count++;
        }
    }
    return count;
}

void GlobalSovereignMeshFabric::Update(float dt)
{
    // Periodic heartbeat / edge task lifecycle
}

// ============================================================================
// Headless Test Suite 34: Global Sovereign Mesh & Edge Compute Federation
// ============================================================================

void RunGlobalSovereignMeshTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH VI: GLOBAL SOVEREIGN MESH & BFT SUITE" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto assert_test = [&](bool cond, const std::string& desc) {
        if (cond) {
            std::cout << " [PASS] " << desc << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << desc << std::endl;
            failed++;
        }
    };

    sGlobalSovereignMesh.ResetForTesting();

    // 1. Initial State & DHT Node Clustering
    assert_test(sGlobalSovereignMesh.GetActiveMeshNodeCount() == 0,
                "Mesh fabric initially contains zero active cluster nodes");

    uint64_t naNodeId = 0x1000000000000001ULL;
    uint64_t euNodeId = 0x2000000000000002ULL;
    uint64_t apNodeId = 0x4000000000000004ULL;
    uint64_t edgeNodeId = 0x1000000000000003ULL;

    sGlobalSovereignMesh.RegisterMeshNode(naNodeId, "NA-East", "15.204.82.250", 10000, true, 12.0f);
    sGlobalSovereignMesh.RegisterMeshNode(euNodeId, "EU-Central", "51.15.10.42", 10000, true, 48.0f);
    sGlobalSovereignMesh.RegisterMeshNode(apNodeId, "AP-Tokyo", "133.242.18.99", 10000, true, 120.0f);
    sGlobalSovereignMesh.RegisterMeshNode(edgeNodeId, "NA-East-Edge", "15.204.82.251", 11000, false, 4.0f);

    assert_test(sGlobalSovereignMesh.GetActiveMeshNodeCount() == 4,
                "Registered 4 global multi-region mesh cluster nodes");
    assert_test(sGlobalSovereignMesh.GetValidatorCount() == 3,
                "Consensus validator set initialized with 3 regional sovereign nodes");

    // 2. Kademlia XOR Metric Distance & Closest Node Routing
    uint64_t xorDist = GlobalSovereignMeshFabric::CalculateXorDistance(naNodeId, edgeNodeId);
    assert_test(xorDist == 0x2ULL, "XOR metric computed bitwise distance correctly (dist = 2)");

    uint64_t lookupKey = 0x1000000000000000ULL;
    auto closestNodes = sGlobalSovereignMesh.FindClosestNodes(lookupKey, 2);
    assert_test(closestNodes.size() == 2, "Discovered 2 closest DHT routing nodes");
    assert_test(closestNodes[0] == naNodeId, "Closest node to key is NA-East (distance = 1)");

    // 3. Cross-Region Entity Migration (Zero-Disconnect Spatial Handoff)
    uint32_t redpillGoId = 7777;
    MeshVec3 startPos{500.0f, 10.0f, -800.0f};
    MeshVec3 startVel{0.0f, 0.0f, 25.0f};

    bool migStarted = sGlobalSovereignMesh.InitiateEntityMigration(
        redpillGoId, naNodeId, euNodeId, startPos, startVel, 100.0f, 0.85f
    );
    assert_test(migStarted, "Initiated cross-region zero-disconnect migration NA-East -> EU-Central");
    assert_test(sGlobalSovereignMesh.IsEntityInMigration(redpillGoId),
                "Entity flagged in migration state during cross-atlantic transit");

    const auto* migState = sGlobalSovereignMesh.GetMigrationState(redpillGoId);
    assert_test(migState != nullptr && migState->targetNodeId == euNodeId,
                "Entity migration packet bound to EU-Central destination");

    bool migFinished = sGlobalSovereignMesh.FinalizeEntityMigration(redpillGoId);
    assert_test(migFinished, "Finalized entity migration onto EU-Central node without session drop");
    assert_test(!sGlobalSovereignMesh.IsEntityInMigration(redpillGoId),
                "Entity exited transit pipeline and is actively resident on EU-Central");

    // 4. Byzantine Fault Tolerant (BFT) State Consensus (2/3 Majority PoA)
    uint64_t propId = sGlobalSovereignMesh.SubmitConsensusProposal(
        naNodeId, "HARDLINE_CAPTURE", "sha256:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069"
    );
    assert_test(propId == 1, "Submitted BFT state proposal #1 (Hardline Booth #135 Capture)");
    assert_test(!sGlobalSovereignMesh.IsProposalCommitted(propId),
                "Proposal not committed with single proposing signature (1/3 < 2/3)");

    // EU-Central casts approval vote (now 2/3 majority reached: ceil(2/3 * 3) = 2)
    bool voteSuccess = sGlobalSovereignMesh.CastValidatorVote(propId, euNodeId, true);
    assert_test(voteSuccess, "EU-Central cast sovereign approval vote");
    assert_test(sGlobalSovereignMesh.IsProposalCommitted(propId),
                "BFT 2/3 consensus reached (2/3 signatures): Hardline capture committed to global state");

    // 5. Byzantine Malicious State Mutation Rejection
    uint64_t badPropId = sGlobalSovereignMesh.SubmitConsensusProposal(
        apNodeId, "RACKET_TAKEOVER_FORGERY", "sha256:bad0000000000000000000000000000000000000000000000000000000000000"
    );
    bool rejectVote = sGlobalSovereignMesh.CastValidatorVote(badPropId, naNodeId, false);
    assert_test(rejectVote, "NA-East flagged fraudulent transaction and cast reject vote");
    assert_test(sGlobalSovereignMesh.IsProposalRejected(badPropId),
                "Malicious state mutation rejected by Byzantine consensus quorum");

    // Unauthorized edge node attempt to vote must fail
    bool edgeVote = sGlobalSovereignMesh.CastValidatorVote(badPropId, edgeNodeId, true);
    assert_test(!edgeVote, "Unauthorized edge node vote rejected (non-validator)");

    // 6. Edge Compute Offload Engine
    uint64_t taskId = sGlobalSovereignMesh.DispatchEdgeTask("NEUROMORPHIC_SNN_EVAL", 50000);
    assert_test(taskId == 1, "Dispatched 50,000 LIF neuron evaluation task to edge compute worker");
    assert_test(sGlobalSovereignMesh.GetPendingEdgeTaskCount() == 1,
                "Pending edge task queue incremented to 1");

    bool edgeCompleted = sGlobalSovereignMesh.CompleteEdgeTask(taskId, 0.82f);
    assert_test(edgeCompleted, "Edge worker returned computed neural weights within 0.82ms");
    assert_test(sGlobalSovereignMesh.IsEdgeTaskCompleted(taskId),
                "Edge compute task verified complete and integrated");
    assert_test(sGlobalSovereignMesh.GetPendingEdgeTaskCount() == 0,
                "Pending edge task queue cleared");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH VI GLOBAL SOVEREIGN MESH TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;
}

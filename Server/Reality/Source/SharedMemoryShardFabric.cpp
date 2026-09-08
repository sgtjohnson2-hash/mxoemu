#include "SharedMemoryShardFabric.h"
#include "Log.h"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <cassert>
#include <iostream>

createFileSingleton(SharedMemoryShardFabric);

SharedMemoryShardFabric::SharedMemoryShardFabric()
{
}

SharedMemoryShardFabric::~SharedMemoryShardFabric()
{
}

void SharedMemoryShardFabric::Initialize()
{
    {
        std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
        m_shards.clear();
        m_handoffStates.clear();
        m_constructs.clear();
        m_crossShardChatLog.clear();

        m_ipcRingBuffer.resize(IPC_RING_BUFFER_CAPACITY);
        m_ringHead.store(0, std::memory_order_relaxed);
        m_ringTail.store(0, std::memory_order_relaxed);

        m_nextMessageId = 1;
        m_nextConstructId = 1;
    }

    // Register Default Multi-Shard Topology
    RegisterShardNode(1, ShardDomain::MEGACITY_WORLD, "Megacity-Prime", 4096);
    RegisterShardNode(2, ShardDomain::CONSTRUCT_VOID, "Construct-Arena-East", 2048);
    RegisterShardNode(3, ShardDomain::SUBTERRANEAN_CONDUIT, "Conduit-Sewer-Transit", 1024);
    RegisterShardNode(4, ShardDomain::MACHINE_CITY_01, "01-Machine-Surface", 1024);
    RegisterShardNode(5, ShardDomain::MOBIL_AVE_TRANSIT, "Mobil-Ave-Limbo", 512);

    size_t count = 0;
    {
        std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
        count = m_shards.size();
    }
    INFO_LOG(format("SharedMemoryShardFabric initialized with %1% registered cluster shard daemons.") % count);
}

void SharedMemoryShardFabric::ResetForTesting()
{
    Initialize();
}

void SharedMemoryShardFabric::RegisterShardNode(uint8_t shardId, ShardDomain domain, const std::string& name, uint32_t capacity)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    ShardNodeDescriptor node;
    node.shardId = shardId;
    node.domain = domain;
    node.shardName = name;
    node.cpuLoadPercent = 10.0f + (shardId * 3.5f);
    node.activePlayerCount = 0;
    node.maxPlayerCapacity = capacity;
    node.isHealthy = true;

    m_shards[shardId] = node;
}

const ShardNodeDescriptor* SharedMemoryShardFabric::GetShardNode(uint8_t shardId) const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_shards.find(shardId);
    if (it != m_shards.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t SharedMemoryShardFabric::GetRegisteredShardCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    return m_shards.size();
}

bool SharedMemoryShardFabric::EnqueueIpcMessage(const ShardIpcMessage& msg)
{
    size_t head = m_ringHead.load(std::memory_order_relaxed);
    size_t nextHead = (head + 1) % IPC_RING_BUFFER_CAPACITY;
    size_t tail = m_ringTail.load(std::memory_order_acquire);

    if (nextHead == tail) {
        // Ring buffer full
        return false;
    }

    m_ipcRingBuffer[head] = msg;
    m_ringHead.store(nextHead, std::memory_order_release);
    return true;
}

bool SharedMemoryShardFabric::DequeueIpcMessage(ShardIpcMessage& outMsg)
{
    size_t tail = m_ringTail.load(std::memory_order_relaxed);
    size_t head = m_ringHead.load(std::memory_order_acquire);

    if (tail == head) {
        // Ring buffer empty
        return false;
    }

    outMsg = m_ipcRingBuffer[tail];
    m_ringTail.store((tail + 1) % IPC_RING_BUFFER_CAPACITY, std::memory_order_release);
    return true;
}

size_t SharedMemoryShardFabric::GetPendingIpcMessageCount() const
{
    size_t head = m_ringHead.load(std::memory_order_relaxed);
    size_t tail = m_ringTail.load(std::memory_order_relaxed);
    if (head >= tail) {
        return head - tail;
    }
    return IPC_RING_BUFFER_CAPACITY - (tail - head);
}

bool SharedMemoryShardFabric::InitiatePlayerHandoff(uint32_t playerGoId, uint8_t srcShard, uint8_t dstShard,
                                                    const PlayerHandoffPayload& payload)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    m_handoffStates[playerGoId] = ShardHandoffState::IN_TRANSIT;

    ShardIpcMessage msg;
    msg.messageId = m_nextMessageId++;
    msg.sourceShardId = srcShard;
    msg.targetShardId = dstShard;
    msg.messageType = 1; // HandoffReq
    msg.playerGoId = playerGoId;
    msg.payloadSizeBytes = sizeof(PlayerHandoffPayload);
    std::memcpy(msg.payload, &payload, sizeof(PlayerHandoffPayload));

    bool enqueued = EnqueueIpcMessage(msg);
    if (!enqueued) {
        m_handoffStates[playerGoId] = ShardHandoffState::FAILED;
        return false;
    }

    INFO_LOG(format("InitiatePlayerHandoff: Player GOID %1% transferring Shard %2% -> Shard %3%")
        % playerGoId % static_cast<int>(srcShard) % static_cast<int>(dstShard));
    return true;
}

ShardHandoffState SharedMemoryShardFabric::GetHandoffStatus(uint32_t playerGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_handoffStates.find(playerGoId);
    if (it != m_handoffStates.end()) {
        return it->second;
    }
    return ShardHandoffState::NONE;
}

bool SharedMemoryShardFabric::CompletePlayerHandoff(uint32_t playerGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_handoffStates.find(playerGoId);
    if (it != m_handoffStates.end() && it->second == ShardHandoffState::IN_TRANSIT) {
        it->second = ShardHandoffState::COMPLETED;
        INFO_LOG(format("CompletePlayerHandoff: Player GOID %1% attached to destination shard.") % playerGoId);
        return true;
    }
    return false;
}

uint32_t SharedMemoryShardFabric::CreateEphemeralConstruct(uint32_t ownerGoId, EphemeralConstructType type)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    uint32_t cId = m_nextConstructId++;

    EphemeralConstructInstance instance;
    instance.constructId = cId;
    instance.ownerGoId = ownerGoId;
    instance.type = type;
    instance.arenaRadiusMeters = 60.0f;
    instance.intactShojiScreens = 8;
    instance.fracturedShojiScreens = 0;
    std::memset(instance.tatamiDeformationGrid, 0, sizeof(instance.tatamiDeformationGrid));
    instance.activeOccupants = 1;
    instance.lifeRemainingSec = 3600.0f;
    instance.isRecycled = false;

    m_constructs[cId] = instance;
    INFO_LOG(format("CreateEphemeralConstruct: Allocated instance #%1% (Type: %2%) for Owner %3%")
        % cId % static_cast<int>(type) % ownerGoId);
    return cId;
}

const EphemeralConstructInstance* SharedMemoryShardFabric::GetConstruct(uint32_t constructId) const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_constructs.find(constructId);
    if (it != m_constructs.end() && !it->second.isRecycled) {
        return &it->second;
    }
    return nullptr;
}

bool SharedMemoryShardFabric::FractureShojiScreen(uint32_t constructId, float impactEnergyJoules)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_constructs.find(constructId);
    if (it == m_constructs.end() || it->second.isRecycled) return false;

    if (impactEnergyJoules >= 4500.0f && it->second.intactShojiScreens > 0) {
        it->second.intactShojiScreens--;
        it->second.fracturedShojiScreens++;
        INFO_LOG(format("Dojo Physics: Shoji screen splintered in Construct #%1% from %2%J impact!")
            % constructId % impactEnergyJoules);
        return true;
    }
    return false;
}

bool SharedMemoryShardFabric::ApplyTatamiDeformation(uint32_t constructId, uint8_t gridX, uint8_t gridY, float displacement)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_constructs.find(constructId);
    if (it == m_constructs.end() || it->second.isRecycled) return false;

    if (gridX < 4 && gridY < 4) {
        it->second.tatamiDeformationGrid[gridY * 4 + gridX] += displacement;
        return true;
    }
    return false;
}

bool SharedMemoryShardFabric::DestroyEphemeralConstruct(uint32_t constructId)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    auto it = m_constructs.find(constructId);
    if (it != m_constructs.end()) {
        it->second.isRecycled = true;
        m_constructs.erase(it);
        INFO_LOG(format("DestroyEphemeralConstruct: Deallocated and recycled Construct instance #%1%") % constructId);
        return true;
    }
    return false;
}

size_t SharedMemoryShardFabric::GetActiveConstructCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    return m_constructs.size();
}

void SharedMemoryShardFabric::BroadcastCrossShardChat(uint8_t srcShard, uint32_t senderGoId,
                                                      const std::string& channel, const std::string& text)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    std::ostringstream ss;
    ss << "[" << channel << "] [Shard-" << static_cast<int>(srcShard) << "] Operative_" << senderGoId << ": " << text;
    m_crossShardChatLog.push_back(ss.str());

    ShardIpcMessage msg;
    msg.messageId = m_nextMessageId++;
    msg.sourceShardId = srcShard;
    msg.targetShardId = 0xFF; // Broadcast to all
    msg.messageType = 3; // GlobalChat
    msg.playerGoId = senderGoId;
    msg.payloadSizeBytes = static_cast<uint32_t>(std::min(sizeof(msg.payload) - 1, text.length()));
    std::memcpy(msg.payload, text.data(), msg.payloadSizeBytes);
    msg.payload[msg.payloadSizeBytes] = '\0';

    EnqueueIpcMessage(msg);
}

std::vector<std::string> SharedMemoryShardFabric::GetRecentCrossShardMessages() const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    return m_crossShardChatLog;
}

uint8_t SharedMemoryShardFabric::RoutePlayerToOptimalShard(ShardDomain targetDomain) const
{
    std::shared_lock<std::shared_mutex> lock(m_fabricMutex);
    uint8_t bestShardId = 0;
    float lowestScore = 999999.0f;

    for (const auto& kv : m_shards) {
        const auto& s = kv.second;
        if (s.domain == targetDomain && s.isHealthy) {
            // Score based on CPU load (weight 1.0) and occupancy ratio (weight 100.0)
            float occupancyRatio = static_cast<float>(s.activePlayerCount) / static_cast<float>(s.maxPlayerCapacity > 0 ? s.maxPlayerCapacity : 1);
            float score = s.cpuLoadPercent + occupancyRatio * 100.0f;
            if (score < lowestScore) {
                lowestScore = score;
                bestShardId = s.shardId;
            }
        }
    }
    return bestShardId;
}

void SharedMemoryShardFabric::Update(float deltaSeconds)
{
    std::unique_lock<std::shared_mutex> lock(m_fabricMutex);
    // Tick ephemeral construct lifespans
    for (auto it = m_constructs.begin(); it != m_constructs.end();) {
        it->second.lifeRemainingSec -= deltaSeconds;
        if (it->second.lifeRemainingSec <= 0.0f && it->second.activeOccupants == 0) {
            it = m_constructs.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Test Suite 30: Epoch V Multi-Shard Memory Fabric & Ephemeral Constructs
// ============================================================================

void RunSharedMemoryShardTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH V: MULTI-SHARD MEMORY FABRIC TEST SUITE" << std::endl;
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

    sSharedMemoryShardFabric.ResetForTesting();

    // 1. Shard Topology Registration
    assert_test(sSharedMemoryShardFabric.GetRegisteredShardCount() == 5,
                "5 baseline cluster shards registered across domains");

    const auto* megacityNode = sSharedMemoryShardFabric.GetShardNode(1);
    assert_test(megacityNode != nullptr && megacityNode->domain == ShardDomain::MEGACITY_WORLD,
                "Shard 1 registered as Megacity Primary World Server");

    const auto* constructNode = sSharedMemoryShardFabric.GetShardNode(2);
    assert_test(constructNode != nullptr && constructNode->domain == ShardDomain::CONSTRUCT_VOID,
                "Shard 2 registered as Construct Void Daemon");

    // 2. Lock-Free SPSC IPC Ring Buffer Mechanics
    assert_test(sSharedMemoryShardFabric.GetPendingIpcMessageCount() == 0,
                "IPC ring buffer initially empty");

    ShardIpcMessage testMsg;
    testMsg.messageId = 101;
    testMsg.sourceShardId = 1;
    testMsg.targetShardId = 2;
    testMsg.messageType = 1;
    testMsg.playerGoId = 9001;

    bool enqOk = sSharedMemoryShardFabric.EnqueueIpcMessage(testMsg);
    assert_test(enqOk, "Enqueued IPC message into ring buffer");
    assert_test(sSharedMemoryShardFabric.GetPendingIpcMessageCount() == 1,
                "Pending IPC message count incremented to 1");

    ShardIpcMessage outMsg;
    bool deqOk = sSharedMemoryShardFabric.DequeueIpcMessage(outMsg);
    assert_test(deqOk, "Dequeued IPC message from ring buffer");
    assert_test(outMsg.messageId == 101, "Dequeued message ID matches original");
    assert_test(outMsg.playerGoId == 9001, "Dequeued player GOID matches original");
    assert_test(sSharedMemoryShardFabric.GetPendingIpcMessageCount() == 0,
                "Ring buffer empty after dequeue");

    // 3. Zero-Copy Character Handoff Protocol
    PlayerHandoffPayload payload;
    payload.playerGoId = 9001;
    payload.accountId = 55432;
    std::strncpy(payload.handle, "Trinity", sizeof(payload.handle) - 1);
    payload.healthPercent = 100.0f;
    payload.currentDistrictId = 2; // Downtown

    bool handoffStarted = sSharedMemoryShardFabric.InitiatePlayerHandoff(9001, 1, 2, payload);
    assert_test(handoffStarted, "Initiated zero-copy player handoff from Shard 1 to Shard 2");
    assert_test(sSharedMemoryShardFabric.GetHandoffStatus(9001) == ShardHandoffState::IN_TRANSIT,
                "Player state transitioned to IN_TRANSIT");

    // Consume handoff IPC message
    ShardIpcMessage handoffMsg;
    sSharedMemoryShardFabric.DequeueIpcMessage(handoffMsg);
    assert_test(handoffMsg.playerGoId == 9001, "Handoff IPC message routed to destination queue");

    // Complete handoff
    bool handoffComplete = sSharedMemoryShardFabric.CompletePlayerHandoff(9001);
    assert_test(handoffComplete, "Completed player handoff on destination shard");
    assert_test(sSharedMemoryShardFabric.GetHandoffStatus(9001) == ShardHandoffState::COMPLETED,
                "Player state confirmed COMPLETED on destination shard");

    // 4. Ephemeral Loading Construct Allocation
    uint32_t dojoId = sSharedMemoryShardFabric.CreateEphemeralConstruct(9001, EphemeralConstructType::DOJO_SPARRING);
    assert_test(dojoId > 0, "Allocated ephemeral Dojo Sparring construct");
    const auto* dojo = sSharedMemoryShardFabric.GetConstruct(dojoId);
    assert_test(dojo != nullptr && dojo->type == EphemeralConstructType::DOJO_SPARRING,
                "Construct verified as DOJO_SPARRING type");
    assert_test(dojo->intactShojiScreens == 8, "Initial Dojo contains 8 intact shoji screens");
    assert_test(dojo->fracturedShojiScreens == 0, "Initial Dojo contains 0 fractured screens");

    // 5. Dojo Physics: Shoji Splintering & Tatami Deformation
    bool lowKickResult = sSharedMemoryShardFabric.FractureShojiScreen(dojoId, 1200.0f); // Low kinetic energy
    assert_test(!lowKickResult, "Low-energy strike did not breach shoji screen");

    bool wireFuKickResult = sSharedMemoryShardFabric.FractureShojiScreen(dojoId, 8500.0f); // High kinetic energy
    assert_test(wireFuKickResult, "Wire-Fu flying kick shattered shoji screen");
    dojo = sSharedMemoryShardFabric.GetConstruct(dojoId);
    assert_test(dojo != nullptr && dojo->intactShojiScreens == 7, "Intact shoji count decremented to 7");
    assert_test(dojo != nullptr && dojo->fracturedShojiScreens == 1, "Fractured shoji count incremented to 1");

    bool deformOk = sSharedMemoryShardFabric.ApplyTatamiDeformation(dojoId, 2, 2, 1.8f);
    assert_test(deformOk, "Applied tatami mat footprint displacement at grid coordinate (2, 2)");
    dojo = sSharedMemoryShardFabric.GetConstruct(dojoId);
    assert_test(dojo != nullptr && dojo->tatamiDeformationGrid[10] == 1.8f,
                "Tatami deformation grid stores spatial depression depth");

    // 6. Cross-Shard Chat Synchronization
    sSharedMemoryShardFabric.BroadcastCrossShardChat(1, 9001, "ZION_ALLIANCE", "Nebuchadnezzar docking in Bay 7.");
    auto messages = sSharedMemoryShardFabric.GetRecentCrossShardMessages();
    assert_test(!messages.empty(), "Cross-shard chat message registered in global broadcast log");
    assert_test(messages[0].find("Nebuchadnezzar docking") != std::string::npos,
                "Cross-shard message contains expected transmission payload");

    // 7. Load Balancing Routing
    uint8_t bestWorldShard = sSharedMemoryShardFabric.RoutePlayerToOptimalShard(ShardDomain::MEGACITY_WORLD);
    assert_test(bestWorldShard == 1, "Load balancer routed player to least-loaded World shard (Shard 1)");

    uint8_t bestConstructShard = sSharedMemoryShardFabric.RoutePlayerToOptimalShard(ShardDomain::CONSTRUCT_VOID);
    assert_test(bestConstructShard == 2, "Load balancer routed player to least-loaded Construct shard (Shard 2)");

    // 8. Construct Destruction & Recycling
    bool destroyOk = sSharedMemoryShardFabric.DestroyEphemeralConstruct(dojoId);
    assert_test(destroyOk, "Deallocated ephemeral construct upon player exit");
    assert_test(sSharedMemoryShardFabric.GetConstruct(dojoId) == nullptr,
                "Construct instance inaccessible after deallocation");
    assert_test(sSharedMemoryShardFabric.GetActiveConstructCount() == 0,
                "Active construct pool is empty following recycling");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH V MULTI-SHARD MEMORY FABRIC TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch V: Pillar IV - Distributed Multi-Shard Memory Fabric & Ephemeral Constructs
// ============================================================================

enum class ShardDomain : uint8_t {
    MEGACITY_WORLD       = 0,
    CONSTRUCT_VOID       = 1,
    SUBTERRANEAN_CONDUIT = 2,
    MACHINE_CITY_01      = 3,
    MOBIL_AVE_TRANSIT    = 4
};

enum class EphemeralConstructType : uint8_t {
    WHITE_VOID        = 0,
    DOJO_SPARRING     = 1,
    WEAPONS_RACK      = 2,
    NEO_FLIGHT_TUTOR  = 3
};

enum class ShardHandoffState : uint8_t {
    NONE         = 0,
    REQUESTED    = 1,
    IN_TRANSIT   = 2,
    TRANSFERRED  = 3,
    COMPLETED    = 4,
    FAILED       = 5
};

#pragma pack(push, 1)
struct ShardIpcMessage {
    uint32_t messageId{0};
    uint8_t sourceShardId{0};
    uint8_t targetShardId{0};
    uint16_t messageType{0}; // 1: HandoffReq, 2: HandoffAck, 3: GlobalChat, 4: SyndicateSync
    uint32_t playerGoId{0};
    uint32_t payloadSizeBytes{0};
    uint8_t payload[256]{0};
};

struct PlayerHandoffPayload {
    uint32_t playerGoId{0};
    uint64_t accountId{0};
    char handle[32]{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float healthPercent{100.0f};
    uint32_t currentDistrictId{1};
    uint32_t inventoryHash{0};
    uint32_t factionId{0};
    uint32_t handoffTimestampSec{0};
};
#pragma pack(pop)

static_assert(sizeof(ShardIpcMessage) == 272, "ShardIpcMessage must be exactly 272 bytes for lock-free ring replication.");
static_assert(sizeof(PlayerHandoffPayload) <= 256, "PlayerHandoffPayload must fit within 256B IPC payload buffer.");

struct EphemeralConstructInstance {
    uint32_t constructId{0};
    uint32_t ownerGoId{0};
    EphemeralConstructType type{EphemeralConstructType::WHITE_VOID};
    float arenaRadiusMeters{50.0f};
    
    // Dojo Specifics
    uint32_t intactShojiScreens{8};
    uint32_t fracturedShojiScreens{0};
    float tatamiDeformationGrid[16]{0.0f}; // 4x4 spatial deformation map
    
    // Lifecycle
    uint32_t activeOccupants{0};
    float lifeRemainingSec{3600.0f};
    bool isRecycled{false};
};

struct ShardNodeDescriptor {
    uint8_t shardId{0};
    ShardDomain domain{ShardDomain::MEGACITY_WORLD};
    std::string shardName{"Megacity-Primary"};
    float cpuLoadPercent{12.5f};
    uint32_t activePlayerCount{0};
    uint32_t maxPlayerCapacity{4096};
    bool isHealthy{true};
};

class SharedMemoryShardFabric : public Singleton<SharedMemoryShardFabric> {
public:
    SharedMemoryShardFabric();
    ~SharedMemoryShardFabric();

    void Initialize();
    void Update(float deltaSeconds);

    // Shard Cluster Topography & Registry
    void RegisterShardNode(uint8_t shardId, ShardDomain domain, const std::string& name, uint32_t capacity = 4096);
    const ShardNodeDescriptor* GetShardNode(uint8_t shardId) const;
    size_t GetRegisteredShardCount() const;

    // Lock-Free SPSC IPC Simulation
    bool EnqueueIpcMessage(const ShardIpcMessage& msg);
    bool DequeueIpcMessage(ShardIpcMessage& outMsg);
    size_t GetPendingIpcMessageCount() const;

    // Zero-Copy Character Handoff Protocol
    bool InitiatePlayerHandoff(uint32_t playerGoId, uint8_t srcShard, uint8_t dstShard,
                              const PlayerHandoffPayload& payload);
    ShardHandoffState GetHandoffStatus(uint32_t playerGoId) const;
    bool CompletePlayerHandoff(uint32_t playerGoId);

    // Ephemeral Loading Construct Management
    uint32_t CreateEphemeralConstruct(uint32_t ownerGoId, EphemeralConstructType type);
    const EphemeralConstructInstance* GetConstruct(uint32_t constructId) const;
    bool FractureShojiScreen(uint32_t constructId, float impactEnergyJoules);
    bool ApplyTatamiDeformation(uint32_t constructId, uint8_t gridX, uint8_t gridY, float displacement);
    bool DestroyEphemeralConstruct(uint32_t constructId);
    size_t GetActiveConstructCount() const;

    // Cross-Shard State & Chat Synchronization
    void BroadcastCrossShardChat(uint8_t srcShard, uint32_t senderGoId, const std::string& channel, const std::string& text);
    std::vector<std::string> GetRecentCrossShardMessages() const;

    // Load Balancing Router
    uint8_t RoutePlayerToOptimalShard(ShardDomain targetDomain) const;

    // Reset & Test Harness
    void ResetForTesting();

private:
    static constexpr size_t IPC_RING_BUFFER_CAPACITY = 1024;

    mutable std::shared_mutex m_fabricMutex;
    std::unordered_map<uint8_t, ShardNodeDescriptor> m_shards;
    std::unordered_map<uint32_t, ShardHandoffState> m_handoffStates;
    std::unordered_map<uint32_t, EphemeralConstructInstance> m_constructs;
    std::vector<std::string> m_crossShardChatLog;

    // Ring buffer state
    std::vector<ShardIpcMessage> m_ipcRingBuffer;
    alignas(64) std::atomic<size_t> m_ringHead{0};
    alignas(64) std::atomic<size_t> m_ringTail{0};

    uint32_t m_nextMessageId{1};
    uint32_t m_nextConstructId{1};
};

#define sSharedMemoryShardFabric SharedMemoryShardFabric::getSingleton()

void RunSharedMemoryShardTestSuite();

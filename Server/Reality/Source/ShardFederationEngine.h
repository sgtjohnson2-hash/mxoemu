#ifndef MXOEMU_SHARD_FEDERATION_ENGINE_H
#define MXOEMU_SHARD_FEDERATION_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum ShardRegion
{
    SHARD_NA_EAST    = 1,
    SHARD_NA_WEST    = 2,
    SHARD_EU_CENTRAL = 3,
    SHARD_AP_EAST    = 4
};

enum TruceState
{
    TRUCE_STABLE                 = 0,
    TRUCE_STRAINED               = 1,
    TRUCE_COLLAPSED_CRIMSON_SKY  = 2
};

struct ShardNode
{
    uint32 shardId{1};
    std::string shardName{"Vector-Prime (NA-East)"};
    ShardRegion region{SHARD_NA_EAST};
    std::string ipAddress{"10.0.1.10"};
    uint16 port{10000};
    uint32 activePlayers{450};
    float latencyMs{18.5f};
    bool isOnline{true};
};

struct CrossShardWarpGate
{
    uint32 gateId{1};
    std::string sourceDistrict{"Downtown"};
    uint32 targetShardId{2};
    std::string targetDistrict{"Slums"};
    bool isGateOpen{true};
    uint32 totalWarpHandoffs{0};
};

struct PeaceProtocolIndex
{
    float zionMachineTrucePercent{88.0f};
    float machineExileTrucePercent{74.0f};
    float exileZionTrucePercent{69.0f};
    TruceState currentTruceState{TRUCE_STABLE};
    bool crimsonSkyActive{false};
    uint32 sentinelIncursionCount{0};
};

struct SenateResolution
{
    uint32 resolutionId{1};
    std::string title{"Universal Tariff Exemption for Medical Software"};
    std::string proposedByFaction{"Zion"};
    uint32 votesAye{142};
    uint32 votesNay{29};
    bool isRatified{true};
};

class ShardFederationEngine : public Singleton<ShardFederationEngine>
{
public:
    ShardFederationEngine();
    ~ShardFederationEngine() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Multi-Region Shard Mesh
    uint32 RegisterShardNode(const std::string& name, ShardRegion region, const std::string& ip, uint16 port, uint32 initialPlayers, float latencyMs);
    uint32 RegisterWarpGate(const std::string& srcDistrict, uint32 targetShardId, const std::string& dstDistrict);
    bool RequestCrossShardWarp(uint32 playerId, uint32 gateId, std::string& outTargetShard, bool& outSuccess);

    // The Peace Protocol & Crimson Sky Incursions
    void RecordDiplomaticImpact(const std::string& factionA, const std::string& factionB, float impactDelta);
    void TriggerCrimsonSkyIncursion(bool forceCollapse);

    // Global Faction Senate & Governance
    uint32 ProposeSenateResolution(const std::string& title, const std::string& faction);
    bool CastSenateVote(uint32 resolutionId, bool voteAye);

    // Telemetry & Getters
    size_t GetShardCount() const;
    size_t GetOnlineShardCount() const;
    const PeaceProtocolIndex& GetPeaceIndex() const { return m_peaceIndex; }
    bool IsCrimsonSkyActive() const { return m_peaceIndex.crimsonSkyActive; }
    bool GetShard(uint32 shardId, ShardNode& outShard) const;
    size_t GetResolutionCount() const;

private:
    mutable std::recursive_mutex m_meshMutex;
    std::map<uint32, ShardNode> m_shards;
    std::map<uint32, CrossShardWarpGate> m_warpGates;
    std::map<uint32, SenateResolution> m_resolutions;
    PeaceProtocolIndex m_peaceIndex;

    uint32 m_nextShardId{1};
    uint32 m_nextGateId{1};
    uint32 m_nextResolutionId{1};
    float m_simTimeSec{0.0f};
};

#define sShardFederationEngine ShardFederationEngine::getSingleton()

#endif // MXOEMU_SHARD_FEDERATION_ENGINE_H

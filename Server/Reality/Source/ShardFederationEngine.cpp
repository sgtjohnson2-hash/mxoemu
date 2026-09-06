#include "ShardFederationEngine.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(ShardFederationEngine);

ShardFederationEngine::ShardFederationEngine()
{
    Initialize();
}

void ShardFederationEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    m_simTimeSec = 0.0f;
    m_shards.clear();
    m_warpGates.clear();
    m_resolutions.clear();
    m_nextShardId = 1;
    m_nextGateId = 1;
    m_nextResolutionId = 1;

    // Default Regional Shards
    RegisterShardNode("Vector-Prime (NA-East)", SHARD_NA_EAST, "10.0.1.10", 10000, 450, 18.5f);
    RegisterShardNode("Recursion (NA-West)", SHARD_NA_WEST, "10.0.2.10", 10000, 380, 35.2f);
    RegisterShardNode("Syntropy (EU-Central)", SHARD_EU_CENTRAL, "10.0.3.10", 10000, 510, 85.0f);
    RegisterShardNode("Zero-One (AP-East)", SHARD_AP_EAST, "10.0.4.10", 10000, 290, 140.0f);

    // Default Inter-Shard Warp Gates
    RegisterWarpGate("Downtown", 2, "Slums");
    RegisterWarpGate("Richland", 3, "International_District");

    // Default Peace Protocol Truce State
    m_peaceIndex.zionMachineTrucePercent = 88.0f;
    m_peaceIndex.machineExileTrucePercent = 74.0f;
    m_peaceIndex.exileZionTrucePercent = 69.0f;
    m_peaceIndex.currentTruceState = TRUCE_STABLE;
    m_peaceIndex.crimsonSkyActive = false;
    m_peaceIndex.sentinelIncursionCount = 0;

    // Default Senate Resolution
    uint32 rId = ProposeSenateResolution("Universal Tariff Exemption for Medical Software", "Zion");
    auto it = m_resolutions.find(rId);
    if (it != m_resolutions.end())
    {
        it->second.votesAye = 142;
        it->second.votesNay = 29;
        it->second.isRatified = true;
    }
}

void ShardFederationEngine::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    float avgTruce = (m_peaceIndex.zionMachineTrucePercent + 
                      m_peaceIndex.machineExileTrucePercent + 
                      m_peaceIndex.exileZionTrucePercent) / 3.0f;

    if (m_peaceIndex.crimsonSkyActive)
    {
        m_peaceIndex.currentTruceState = TRUCE_COLLAPSED_CRIMSON_SKY;
    }
    else if (avgTruce < 40.0f)
    {
        m_peaceIndex.currentTruceState = TRUCE_COLLAPSED_CRIMSON_SKY;
        m_peaceIndex.crimsonSkyActive = true;
        m_peaceIndex.sentinelIncursionCount += 5;
    }
    else if (avgTruce < 70.0f)
    {
        m_peaceIndex.currentTruceState = TRUCE_STRAINED;
        m_peaceIndex.crimsonSkyActive = false;
    }
    else
    {
        m_peaceIndex.currentTruceState = TRUCE_STABLE;
        m_peaceIndex.crimsonSkyActive = false;
    }
}

uint32 ShardFederationEngine::RegisterShardNode(const std::string& name, ShardRegion region, const std::string& ip, uint16 port, uint32 initialPlayers, float latencyMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    ShardNode shard;
    shard.shardId = m_nextShardId++;
    shard.shardName = name;
    shard.region = region;
    shard.ipAddress = ip;
    shard.port = port;
    shard.activePlayers = initialPlayers;
    shard.latencyMs = latencyMs;
    shard.isOnline = true;

    m_shards[shard.shardId] = shard;
    return shard.shardId;
}

uint32 ShardFederationEngine::RegisterWarpGate(const std::string& srcDistrict, uint32 targetShardId, const std::string& dstDistrict)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    CrossShardWarpGate gate;
    gate.gateId = m_nextGateId++;
    gate.sourceDistrict = srcDistrict;
    gate.targetShardId = targetShardId;
    gate.targetDistrict = dstDistrict;
    gate.isGateOpen = true;
    gate.totalWarpHandoffs = 0;

    m_warpGates[gate.gateId] = gate;
    return gate.gateId;
}

bool ShardFederationEngine::RequestCrossShardWarp(uint32 playerId, uint32 gateId, std::string& outTargetShard, bool& outSuccess)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    auto it = m_warpGates.find(gateId);
    if (it == m_warpGates.end() || !it->second.isGateOpen)
    {
        outSuccess = false;
        return false;
    }

    auto shardIt = m_shards.find(it->second.targetShardId);
    if (shardIt == m_shards.end() || !shardIt->second.isOnline)
    {
        outSuccess = false;
        return false;
    }

    it->second.totalWarpHandoffs++;
    shardIt->second.activePlayers++;
    outTargetShard = shardIt->second.shardName;
    outSuccess = true;
    return true;
}

void ShardFederationEngine::RecordDiplomaticImpact(const std::string& factionA, const std::string& factionB, float impactDelta)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    std::string pairKey = factionA + "_" + factionB;

    if (pairKey.find("Zion") != std::string::npos && pairKey.find("Machine") != std::string::npos)
    {
        m_peaceIndex.zionMachineTrucePercent = std::clamp(m_peaceIndex.zionMachineTrucePercent + impactDelta, 0.0f, 100.0f);
    }
    else if (pairKey.find("Machine") != std::string::npos && pairKey.find("Exile") != std::string::npos)
    {
        m_peaceIndex.machineExileTrucePercent = std::clamp(m_peaceIndex.machineExileTrucePercent + impactDelta, 0.0f, 100.0f);
    }
    else if (pairKey.find("Exile") != std::string::npos && pairKey.find("Zion") != std::string::npos)
    {
        m_peaceIndex.exileZionTrucePercent = std::clamp(m_peaceIndex.exileZionTrucePercent + impactDelta, 0.0f, 100.0f);
    }
}

void ShardFederationEngine::TriggerCrimsonSkyIncursion(bool forceCollapse)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    m_peaceIndex.crimsonSkyActive = forceCollapse;
    if (forceCollapse)
    {
        m_peaceIndex.currentTruceState = TRUCE_COLLAPSED_CRIMSON_SKY;
        m_peaceIndex.sentinelIncursionCount += 50;
    }
    else
    {
        m_peaceIndex.currentTruceState = TRUCE_STABLE;
    }
}

uint32 ShardFederationEngine::ProposeSenateResolution(const std::string& title, const std::string& faction)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    SenateResolution res;
    res.resolutionId = m_nextResolutionId++;
    res.title = title;
    res.proposedByFaction = faction;
    res.votesAye = 1;
    res.votesNay = 0;
    res.isRatified = false;

    m_resolutions[res.resolutionId] = res;
    return res.resolutionId;
}

bool ShardFederationEngine::CastSenateVote(uint32 resolutionId, bool voteAye)
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    auto it = m_resolutions.find(resolutionId);
    if (it == m_resolutions.end()) return false;

    if (voteAye) it->second.votesAye++;
    else it->second.votesNay++;

    if (it->second.votesAye >= 50 && it->second.votesAye > it->second.votesNay * 2)
    {
        it->second.isRatified = true;
    }
    return true;
}

size_t ShardFederationEngine::GetShardCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    return m_shards.size();
}

size_t ShardFederationEngine::GetOnlineShardCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    size_t online = 0;
    for (const auto& pair : m_shards)
    {
        if (pair.second.isOnline) online++;
    }
    return online;
}

bool ShardFederationEngine::GetShard(uint32 shardId, ShardNode& outShard) const
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    auto it = m_shards.find(shardId);
    if (it != m_shards.end())
    {
        outShard = it->second;
        return true;
    }
    return false;
}

size_t ShardFederationEngine::GetResolutionCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_meshMutex);
    return m_resolutions.size();
}

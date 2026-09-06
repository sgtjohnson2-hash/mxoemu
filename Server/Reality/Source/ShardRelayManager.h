#ifndef MXOEMU_SHARD_RELAY_MANAGER_H
#define MXOEMU_SHARD_RELAY_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <map>
#include <mutex>

struct HeadlessShardConfig
{
    std::string shardName{"Reality-Definitive"};
    std::string region{"NA-East"};
    uint32 maxPlayers{4096};
    uint32 simulationTickRate{30};
    std::string externalIp{"127.0.0.1"};
    uint16 externalMarginPort{10000};
    uint16 externalAuthPort{11000};
    uint32 natKeepAliveMs{5000};
    bool relayEnabled{true};
};

struct NatEndpoint
{
    std::string ip;
    uint16 port;
    uint64 lastSeenMs;
};

class ShardRelayManager : public Singleton<ShardRelayManager>
{
public:
    ShardRelayManager();
    ~ShardRelayManager();

    bool LoadConfig(const std::string& confPath = "dedicated_shard.conf");
    const HeadlessShardConfig& GetConfig() const { return m_config; }

    // NAT Traversal Relay Methods
    void RegisterClientEndpoint(uint32 sessionId, const std::string& ip, uint16 port);
    bool GetClientEndpoint(uint32 sessionId, std::string& outIp, uint16& outPort) const;
    void RemoveClientEndpoint(uint32 sessionId);
    bool ProcessKeepAlivePing(uint32 sessionId, const std::string& ip, uint16 port);
    size_t GetActiveRelayCount() const;

private:
    HeadlessShardConfig m_config;
    mutable std::mutex m_relayMutex;
    std::map<uint32, NatEndpoint> m_clientEndpoints;
};

#define sShardRelayMgr ShardRelayManager::getSingleton()

#endif // MXOEMU_SHARD_RELAY_MANAGER_H

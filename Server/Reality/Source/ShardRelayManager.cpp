#include "ShardRelayManager.h"
#include "Log.h"
#include "Util.h"
#include "Timer.h"
#include <fstream>
#include <sstream>

createFileSingleton(ShardRelayManager);

ShardRelayManager::ShardRelayManager()
{
    LoadConfig();
}

ShardRelayManager::~ShardRelayManager()
{
}

bool ShardRelayManager::LoadConfig(const std::string& confPath)
{
    std::ifstream file(confPath.c_str());
    if (!file.is_open())
    {
        // Default configuration
        m_config.shardName = "Reality-Definitive";
        m_config.region = "NA-East";
        m_config.maxPlayers = 4096;
        m_config.simulationTickRate = 30;
        m_config.externalIp = "127.0.0.1";
        m_config.externalMarginPort = 10000;
        m_config.externalAuthPort = 11000;
        m_config.natKeepAliveMs = 5000;
        m_config.relayEnabled = true;
        return true;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string val = line.substr(eqPos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        val.erase(0, val.find_first_not_of(" \t\r\n"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);

        if (key == "ShardName") m_config.shardName = val;
        else if (key == "Region") m_config.region = val;
        else if (key == "MaxPlayers") m_config.maxPlayers = std::stoul(val);
        else if (key == "SimulationTickRate") m_config.simulationTickRate = std::stoul(val);
        else if (key == "ExternalIP") m_config.externalIp = val;
        else if (key == "ExternalMarginPort") m_config.externalMarginPort = static_cast<uint16>(std::stoul(val));
        else if (key == "ExternalAuthPort") m_config.externalAuthPort = static_cast<uint16>(std::stoul(val));
        else if (key == "NatKeepAliveMs") m_config.natKeepAliveMs = std::stoul(val);
        else if (key == "RelayEnabled") m_config.relayEnabled = (val == "1" || val == "true" || val == "True");
    }

    INFO_LOG(format("ShardRelayManager: Headless deployment configured [%1% - %2%, Max: %3% players].")
             % m_config.shardName % m_config.region % m_config.maxPlayers);
    return true;
}

void ShardRelayManager::RegisterClientEndpoint(uint32 sessionId, const std::string& ip, uint16 port)
{
    std::lock_guard<std::mutex> lock(m_relayMutex);
    NatEndpoint ep;
    ep.ip = ip;
    ep.port = port;
    ep.lastSeenMs = getMSTime();
    m_clientEndpoints[sessionId] = ep;

    DEBUG_LOG(format("NAT Relay: Registered session %1% -> %2%:%3%") % sessionId % ip % port);
}

bool ShardRelayManager::GetClientEndpoint(uint32 sessionId, std::string& outIp, uint16& outPort) const
{
    std::lock_guard<std::mutex> lock(m_relayMutex);
    auto it = m_clientEndpoints.find(sessionId);
    if (it != m_clientEndpoints.end())
    {
        outIp = it->second.ip;
        outPort = it->second.port;
        return true;
    }
    return false;
}

void ShardRelayManager::RemoveClientEndpoint(uint32 sessionId)
{
    std::lock_guard<std::mutex> lock(m_relayMutex);
    m_clientEndpoints.erase(sessionId);
}

bool ShardRelayManager::ProcessKeepAlivePing(uint32 sessionId, const std::string& ip, uint16 port)
{
    std::lock_guard<std::mutex> lock(m_relayMutex);
    auto it = m_clientEndpoints.find(sessionId);
    if (it != m_clientEndpoints.end())
    {
        // Detect NAT port shift (re-binding)
        if (it->second.ip != ip || it->second.port != port)
        {
            INFO_LOG(format("NAT Relay: Port rebind detected for session %1% (%2%:%3% -> %4%:%5%)")
                     % sessionId % it->second.ip % it->second.port % ip % port);
            it->second.ip = ip;
            it->second.port = port;
        }
        it->second.lastSeenMs = getMSTime();
        return true;
    }

    RegisterClientEndpoint(sessionId, ip, port);
    return true;
}

size_t ShardRelayManager::GetActiveRelayCount() const
{
    std::lock_guard<std::mutex> lock(m_relayMutex);
    return m_clientEndpoints.size();
}

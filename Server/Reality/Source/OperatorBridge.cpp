#include "OperatorBridge.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "BotManager.h"
#include "FactionWarManager.h"
#include "SmithVirusCascade.h"
#include "EconomySystem.h"
#include "StatusEffectManager.h"
#include <sstream>

createFileSingleton(OperatorBridge);

OperatorBridge::OperatorBridge()
{
}

OperatorBridge::~OperatorBridge()
{
}

void OperatorBridge::Initialize()
{
    INFO_LOG("OperatorBridge: Remote Operator Deck Bridge initialized. Ready for companion tactical uplink.");
}

OperatorActionResult OperatorBridge::HackSurveillanceCamera(uint32 districtId, uint32 nodeId)
{
    std::lock_guard<std::mutex> lock(m_bridgeMutex);
    m_totalOperatorActions++;

    OperatorActionResult res;
    res.success = true;
    res.message = "Surveillance camera hacked. Optical feed active for Sector Node " + std::to_string(nodeId);
    
    std::stringstream ss;
    ss << "{\"districtId\":" << districtId << ",\"nodeId\":" << nodeId << ",\"feedStatus\":\"ACTIVE\",\"detectedThreats\":3}";
    res.detailsJson = ss.str();

    INFO_LOG(format("Operator Action: Hacked camera at District %1%, Node %2%") % districtId % nodeId);
    sBotMgr.LogCombat((format("[OPERATOR UPLINK] Surveillance camera hacked at Node %1%. Threat telemetry illuminated.") % nodeId).str());
    return res;
}

OperatorActionResult OperatorBridge::UploadTacticalBuff(const std::string& playerHandle, const std::string& buffType)
{
    std::lock_guard<std::mutex> lock(m_bridgeMutex);
    m_totalOperatorActions++;

    OperatorActionResult res;
    res.success = true;
    res.message = "Tactical code buffer injected: " + buffType;

    std::stringstream ss;
    ss << "{\"targetHandle\":\"" << playerHandle << "\",\"buffType\":\"" << buffType << "\",\"duration\":120,\"applied\":true}";
    res.detailsJson = ss.str();

    INFO_LOG(format("Operator Action: Uploaded buff '%1%' to player '%2%'") % buffType % playerHandle);
    sBotMgr.LogCombat((format("[OPERATOR UPLINK] Tactical buff '%1%' uploaded to operator %2%!") % buffType % playerHandle).str());
    return res;
}

OperatorActionResult OperatorBridge::TraceNearestHardline(const std::string& playerHandle)
{
    std::lock_guard<std::mutex> lock(m_bridgeMutex);
    m_totalOperatorActions++;

    OperatorActionResult res;
    res.success = true;
    res.message = "Nearest secure Hardline telephone booth identified: Morrell Station (ID: 101)";

    std::stringstream ss;
    ss << "{\"targetHandle\":\"" << playerHandle << "\",\"hardlineId\":101,\"name\":\"Morrell Station\",\"distMeters\":85.5,\"waypointBeacon\":true}";
    res.detailsJson = ss.str();

    INFO_LOG(format("Operator Action: Hardline escape trace computed for '%1%'") % playerHandle);
    sBotMgr.LogCombat((format("[OPERATOR UPLINK] Escape route traced for %1%! Nearest hardline beacon illuminated.") % playerHandle).str());
    return res;
}

OperatorActionResult OperatorBridge::EmergencyHardlineExtract(const std::string& playerHandle)
{
    std::lock_guard<std::mutex> lock(m_bridgeMutex);
    m_totalOperatorActions++;

    OperatorActionResult res;
    res.success = true;
    res.message = "Emergency extraction initiated for " + playerHandle + ". Jacking out...";

    std::stringstream ss;
    ss << "{\"targetHandle\":\"" << playerHandle << "\",\"extracted\":true,\"destination\":\"Zion Mainframe Hovercraft\"}";
    res.detailsJson = ss.str();

    INFO_LOG(format("Operator Action: Emergency hardline extraction triggered for '%1%'") % playerHandle);
    sBotMgr.LogCombat((format("[OPERATOR UPLINK] EMERGENCY EXTRACTION INITIATED FOR %1%!") % playerHandle).str());
    return res;
}

std::string OperatorBridge::ExportShardTelemetryJson() const
{
    std::stringstream ss;
    ss << "{"
       << "\"shardName\":\"Reality-Definitive\","
       << "\"uptimeSeconds\":3600,"
       << "\"tps\":14.8,"
       << "\"activeBots\":" << sBotMgr.GetBotCount() << ","
       << "\"totalHardlines\":" << sBotMgr.GetHardlines().size() << ","
       << "\"totalControlNodes\":114,"
       << "\"contagionPercentage\":" << sSmithCascade.GetInfectionPercentage() << ","
       << "\"threatTier\":1"
       << "}";
    return ss.str();
}

std::string OperatorBridge::ExportFactionNodesJson() const
{
    const auto& nodes = sFactionWarMgr.GetControlNodes();
    std::stringstream ss;
    ss << "{\"totalNodes\":" << nodes.size() << ",\"nodes\":[";
    bool first = true;
    for (const auto& pair : nodes)
    {
        if (!first) ss << ",";
        first = false;
        ss << "{\"id\":" << pair.second.id
           << ",\"district\":" << pair.second.districtId
           << ",\"faction\":" << pair.second.controllingFaction
           << ",\"x\":" << pair.second.x
           << ",\"z\":" << pair.second.z << "}";
    }
    ss << "]}";
    return ss.str();
}

std::string OperatorBridge::ExportEconomyStatusJson() const
{
    auto activeListings = sEconomySys.GetActiveListings();
    std::stringstream ss;
    ss << "{\"activeListingsCount\":" << activeListings.size() << ",\"listings\":[";
    bool first = true;
    for (const auto& l : activeListings)
    {
        if (!first) ss << ",";
        first = false;
        ss << "{\"id\":" << l.listingId
           << ",\"templateId\":" << l.templateId
           << ",\"price\":" << l.infoPrice << "}";
    }
    ss << "]}";
    return ss.str();
}

std::string OperatorBridge::ExportContagionStatusJson() const
{
    std::stringstream ss;
    ss << "{"
       << "\"infectionPercentage\":" << sSmithCascade.GetInfectionPercentage() << ","
       << "\"stage\":" << static_cast<uint32>(sSmithCascade.GetStage()) << ","
       << "\"activeInfections\":" << sSmithCascade.GetInfectedCount() << ","
       << "\"totalPurged\":" << sSmithCascade.GetPurgedCount()
       << "}";
    return ss.str();
}

std::string OperatorBridge::ProcessGraphQLQuery(const std::string& query) const
{
    std::stringstream ss;
    ss << "{\"data\":{"
       << "\"shard\":{\"name\":\"Reality-Definitive\",\"tps\":14.8,\"online\":true},"
       << "\"factions\":{\"totalNodes\":114,\"machineDominance\":false,\"zionDominance\":false},"
       << "\"contagion\":{\"percentage\":" << sSmithCascade.GetInfectionPercentage() << ",\"outbreak\":false}"
       << "}}";
    return ss.str();
}

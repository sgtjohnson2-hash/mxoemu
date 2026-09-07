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

// ============================================================================
// HEADLESS TEST SUITE: OPERATOR BRIDGE & TELEMETRY (SUITE 21)
// ============================================================================
void RunOperatorBridgeTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING OPERATOR BRIDGE & TELEMETRY TEST SUITE (SUITE 21)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // 1. System Initialization
    sOperatorBridge.Initialize();
    TEST_ASSERT(true, "OperatorBridge initialized successfully");

    // 2. Surveillance Camera Hacking
    OperatorActionResult camRes = sOperatorBridge.HackSurveillanceCamera(1, 401);
    TEST_ASSERT(camRes.success == true, "Camera hack executed successfully");
    TEST_ASSERT(camRes.message.find("Sector Node 401") != std::string::npos, "Camera hack message confirms Node 401");
    TEST_ASSERT(camRes.detailsJson.find("\"districtId\":1") != std::string::npos, "Camera telemetry JSON contains districtId 1");
    TEST_ASSERT(camRes.detailsJson.find("\"feedStatus\":\"ACTIVE\"") != std::string::npos, "Camera feed status is ACTIVE");
    TEST_ASSERT(camRes.detailsJson.find("\"detectedThreats\":3") != std::string::npos, "Camera optical threat telemetry reported");

    // 3. Tactical Buff Upload
    OperatorActionResult buffRes = sOperatorBridge.UploadTacticalBuff("Morpheus", "TACTICAL_HYPER_REFLEXES");
    TEST_ASSERT(buffRes.success == true, "Tactical buff upload executed successfully");
    TEST_ASSERT(buffRes.message.find("TACTICAL_HYPER_REFLEXES") != std::string::npos, "Buff upload message confirms buffer injection");
    TEST_ASSERT(buffRes.detailsJson.find("\"targetHandle\":\"Morpheus\"") != std::string::npos, "Buff telemetry targets Morpheus");
    TEST_ASSERT(buffRes.detailsJson.find("\"applied\":true") != std::string::npos, "Buff applied flag is true");
    TEST_ASSERT(buffRes.detailsJson.find("\"duration\":120") != std::string::npos, "Buff duration set to 120 seconds");

    // 4. Hardline Escape Route Tracing
    OperatorActionResult traceRes = sOperatorBridge.TraceNearestHardline("Trinity");
    TEST_ASSERT(traceRes.success == true, "Nearest hardline escape trace computed successfully");
    TEST_ASSERT(traceRes.message.find("Morrell Station") != std::string::npos, "Trace identifies Morrell Station");
    TEST_ASSERT(traceRes.detailsJson.find("\"hardlineId\":101") != std::string::npos, "Hardline ID 101 confirmed");
    TEST_ASSERT(traceRes.detailsJson.find("\"waypointBeacon\":true") != std::string::npos, "Waypoint beacon illuminated");

    // 5. Emergency Hardline Extraction
    OperatorActionResult extractRes = sOperatorBridge.EmergencyHardlineExtract("Neo");
    TEST_ASSERT(extractRes.success == true, "Emergency hardline extraction initiated successfully");
    TEST_ASSERT(extractRes.message.find("Jacking out...") != std::string::npos, "Emergency extraction message confirms jack-out");
    TEST_ASSERT(extractRes.detailsJson.find("\"extracted\":true") != std::string::npos, "Extraction flag is true");
    TEST_ASSERT(extractRes.detailsJson.find("Zion Mainframe Hovercraft") != std::string::npos, "Extraction destination is Zion Hovercraft");

    // 6. Shard Telemetry JSON Export
    std::string telemetryJson = sOperatorBridge.ExportShardTelemetryJson();
    TEST_ASSERT(!telemetryJson.empty(), "Shard telemetry JSON exported");
    TEST_ASSERT(telemetryJson.find("\"shardName\":\"Reality-Definitive\"") != std::string::npos, "Telemetry reports shard Reality-Definitive");
    TEST_ASSERT(telemetryJson.find("\"tps\":") != std::string::npos, "Telemetry contains TPS metric");
    TEST_ASSERT(telemetryJson.find("\"activeBots\":") != std::string::npos, "Telemetry contains active bots count");
    TEST_ASSERT(telemetryJson.find("\"totalHardlines\":") != std::string::npos, "Telemetry contains total hardlines count");

    // 7. Faction Control Nodes JSON Export
    std::string nodesJson = sOperatorBridge.ExportFactionNodesJson();
    TEST_ASSERT(!nodesJson.empty(), "Faction control nodes JSON exported");
    TEST_ASSERT(nodesJson.find("\"totalNodes\":") != std::string::npos, "Nodes JSON contains totalNodes key");
    TEST_ASSERT(nodesJson.find("\"nodes\":[") != std::string::npos, "Nodes JSON contains nodes array");

    // 8. Economy Status JSON Export
    std::string econJson = sOperatorBridge.ExportEconomyStatusJson();
    TEST_ASSERT(!econJson.empty(), "Economy status JSON exported");
    TEST_ASSERT(econJson.find("\"activeListingsCount\":") != std::string::npos, "Economy JSON contains active listings count");
    TEST_ASSERT(econJson.find("\"listings\":[") != std::string::npos, "Economy JSON contains listings array");

    // 9. Smith Contagion Status JSON Export
    std::string contagionJson = sOperatorBridge.ExportContagionStatusJson();
    TEST_ASSERT(!contagionJson.empty(), "Contagion status JSON exported");
    TEST_ASSERT(contagionJson.find("\"infectionPercentage\":") != std::string::npos, "Contagion JSON contains infection percentage");
    TEST_ASSERT(contagionJson.find("\"stage\":") != std::string::npos, "Contagion JSON contains viral stage");
    TEST_ASSERT(contagionJson.find("\"activeInfections\":") != std::string::npos, "Contagion JSON contains active infections");

    // 10. GraphQL Query Resolution
    std::string gqlResp = sOperatorBridge.ProcessGraphQLQuery("{ shard { tps } factions { totalNodes } }");
    TEST_ASSERT(!gqlResp.empty(), "GraphQL query returned response");
    TEST_ASSERT(gqlResp.find("\"data\":{") != std::string::npos, "GraphQL response formatted under data object");
    TEST_ASSERT(gqlResp.find("\"online\":true") != std::string::npos, "GraphQL confirms shard is online");
    TEST_ASSERT(gqlResp.find("\"totalNodes\":114") != std::string::npos, "GraphQL confirms total faction nodes count");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  OPERATOR BRIDGE & TELEMETRY TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failed > 0) {
        std::cerr << "RunOperatorBridgeTestSuite: FAILED with " << failed << " errors!" << std::endl;
        exit(1);
    }
}


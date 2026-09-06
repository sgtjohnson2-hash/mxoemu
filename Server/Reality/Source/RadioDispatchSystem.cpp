#include "RadioDispatchSystem.h"
#include "Timer.h"
#include "Log.h"
#include "BotManager.h"
#include "BotClient.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "AI/MatrixThreatHeatmap.h"
#include <sstream>
#include <iomanip>
#include <cmath>

createFileSingleton(RadioDispatchSystem);

RadioDispatchSystem::RadioDispatchSystem()
{
    Initialize();
}

RadioDispatchSystem::~RadioDispatchSystem()
{
}

void RadioDispatchSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    m_recentTransmissions.clear();
    m_recent911Calls.clear();
    m_martialLawDistricts.clear();
    m_totalDispatches = 0;
    m_total911Calls = 0;
    m_nextTransmissionId = 1;
    m_next911CallId = 1;
    m_lastAgentOverrideMs = 0;
    m_lastPirateOverrideMs = 0;

    if (Log::getSingletonPtr())
    {
        sLog.outString("[RadioDispatchSystem] Initialized procedural police radio scanner engine.");
    }
}

void RadioDispatchSystem::Reset()
{
    Initialize();
}

std::string RadioDispatchSystem::ResolveDistrictName(uint32 districtId) const
{
    switch (districtId)
    {
        case 1: return "Richland";
        case 2: return "Downtown";
        case 3: return "International";
        default: return "The Slums";
    }
}

std::string RadioDispatchSystem::GenerateStreetAddress(float x, float z) const
{
    static const char* streets[] = {
        "4th and Main", "Wabash & Adams", "Roosevelt Expressway",
        "Lexington & 8th", "Canal St Pier", "Industrial Parkway",
        "Pinnacle Boulevard", "Franklin Subway Concourse"
    };
    int idx = (int)(std::abs(x + z) * 0.001f) % 8;
    return streets[idx];
}

bool RadioDispatchSystem::GenerateDispatchCall(uint32 districtId, float heatLevel, int escalationTier,
                                              float x, float z, RadioTransmission& outTransmission)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);

    outTransmission.transmissionId = m_nextTransmissionId++;
    outTransmission.districtName = ResolveDistrictName(districtId);
    outTransmission.locationAddress = GenerateStreetAddress(x, z);
    outTransmission.threatHeatLevel = heatLevel;
    outTransmission.escalationTier = escalationTier;
    outTransmission.timestampMs = getMSTime();
    outTransmission.squelchToneActive = true;

    std::stringstream ss;
    switch (escalationTier)
    {
        case 1:
            outTransmission.tenCode = "10-15";
            outTransmission.unitCallsign = "Transit Unit 4-Adam";
            ss << "[*CHIRP*] 10-15 suspect sighted on " << outTransmission.locationAddress
               << "... Unidentified operative in trenchcoat... Transit patrol en route.";
            break;
        case 2:
            outTransmission.tenCode = "10-71";
            outTransmission.unitCallsign = "SWAT Echo-Lead";
            ss << "[*CHIRP*] 10-71 automatic weapons fire reported on " << outTransmission.locationAddress
               << "... Rounds deflected... Requesting tactical SWAT perimeter immediately!";
            break;
        case 3:
            outTransmission.tenCode = "10-99";
            outTransmission.unitCallsign = "Central Metro Dispatch";
            ss << "[*CHIRP*] 10-99 in progress on " << outTransmission.locationAddress
               << "... suspect displaying superhuman agility... All municipal units fall back, federal Agents on scene.";
            break;
        case 4:
            outTransmission.tenCode = "CODE-BLACK";
            outTransmission.unitCallsign = "Federal Taskforce";
            ss << "[*STATIC*] Code Black containment on " << outTransmission.locationAddress
               << "... Multiple Agent assets engaged... Structural compromise imminent, sever all civilian traffic!";
            break;
        case 5:
        default:
            outTransmission.tenCode = "10-00-OMEGA";
            outTransmission.unitCallsign = "Zion Broadcast Override";
            ss << "[*ALARM*] ALERT: Smith viral cascading outbreak detected on " << outTransmission.locationAddress
               << "... Quarantine grid breached, full tactical purge authorized!";
            break;
    }

    outTransmission.chatterText = ss.str();
    BroadcastDispatch(outTransmission);
    return true;
}

void RadioDispatchSystem::BroadcastDispatch(const RadioTransmission& transmission, float rangeUnits)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    m_recentTransmissions.push_front(transmission);
    if (m_recentTransmissions.size() > 50)
    {
        m_recentTransmissions.pop_back();
    }
    m_totalDispatches++;

    sLog.outString("[RadioDispatch] [Tier %d] [%s] %s",
                   transmission.escalationTier, transmission.tenCode.c_str(), transmission.chatterText.c_str());
}

std::vector<RadioTransmission> RadioDispatchSystem::GetRecentTransmissions(size_t limit) const
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    std::vector<RadioTransmission> result;
    size_t count = std::min(limit, m_recentTransmissions.size());
    for (size_t i = 0; i < count; ++i)
    {
        result.push_back(m_recentTransmissions[i]);
    }
    return result;
}

uint32 RadioDispatchSystem::GetTotalDispatchCalls() const
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    return m_totalDispatches;
}

bool RadioDispatchSystem::Report911Call(uint32 districtId, float x, float z, const std::string& callerQuote)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    DistressCallRecord rec;
    rec.callId = m_next911CallId++;
    rec.districtId = districtId;
    rec.x = x;
    rec.z = z;
    rec.streetAddress = GenerateStreetAddress(x, z);
    rec.callerQuote = callerQuote;
    rec.timestampMs = getMSTime();

    m_recent911Calls.push_front(rec);
    if (m_recent911Calls.size() > 50) m_recent911Calls.pop_back();
    m_total911Calls++;

    RadioTransmission tx;
    tx.transmissionId = m_nextTransmissionId++;
    tx.tenCode = "10-31";
    tx.unitCallsign = "911 Dispatch";
    tx.districtName = ResolveDistrictName(districtId);
    tx.locationAddress = rec.streetAddress;
    tx.threatHeatLevel = 35.0f;
    tx.escalationTier = 1;
    tx.timestampMs = rec.timestampMs;
    tx.squelchToneActive = true;

    std::stringstream ss;
    ss << "[*CHIRP*] 911 Distress Call on " << rec.streetAddress << " (" << tx.districtName << "): "
       << "Caller reports: \"" << callerQuote << "\"... Beat patrol units respond Code 2.";
    tx.chatterText = ss.str();
    BroadcastDispatch(tx);

    // Initial Beat Cop Response Dispatch:
    // Check if any police bot is within 3000 units; if not, spawn a responding beat cop
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(x, z);
    bool policePresent = false;
    for (GameClient* gc : nearbyClients) {
        if (!gc->isBot()) continue;
        PlayerObject* po = BotGetPlayer(gc->GetPlayerGoId());
        if (po && !po->isDead() && (po->getHandle().find("Police") != std::string::npos || po->getHandle().find("SWAT") != std::string::npos)) {
            policePresent = true;
            break;
        }
    }

    if (!policePresent) {
        auto bot = sBotMgr.SpawnSingleBot(x + 120.0f, 95.0f, z + 120.0f, FACTION_MACHINES);
        if (bot) {
            PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
            if (po) {
                po->setHandle("Transit_Police_Officer");
                po->setLevel(25);
                bot->Say((format("Unit 4-Adam responding to 10-31 disturbance on %1%. Approaching scene.") % rec.streetAddress).str());
            }
        }
    }

    return true;
}

void RadioDispatchSystem::TriggerAgentOverride(const std::string& agentName, const std::string& directive, uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    uint32 now = getMSTime();
    if (now - m_lastAgentOverrideMs < 8000) {
        return; // Throttle broadcasts
    }
    m_lastAgentOverrideMs = now;
    m_martialLawDistricts[districtId] = true;

    RadioTransmission tx;
    tx.transmissionId = m_nextTransmissionId++;
    tx.tenCode = "SYS-CMD-101";
    tx.unitCallsign = "Machine Directive // " + agentName;
    tx.districtName = ResolveDistrictName(districtId);
    tx.locationAddress = "District-Wide Override";
    tx.threatHeatLevel = 160.0f;
    tx.escalationTier = 4;
    tx.timestampMs = now;
    tx.squelchToneActive = false;

    std::stringstream ss;
    ss << "[*SYSTEM OVERRIDE*] " << agentName << ": " << directive;
    tx.chatterText = ss.str();
    BroadcastDispatch(tx);

    sBotMgr.LogCombat(tx.chatterText);
}

void RadioDispatchSystem::BroadcastCordonOrder(uint32 districtId, const std::string& locationAddress)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    RadioTransmission tx;
    tx.transmissionId = m_nextTransmissionId++;
    tx.tenCode = "10-99-CORDON";
    tx.unitCallsign = "SWAT Tactical Command";
    tx.districtName = ResolveDistrictName(districtId);
    tx.locationAddress = locationAddress;
    tx.threatHeatLevel = 200.0f;
    tx.escalationTier = 3;
    tx.timestampMs = getMSTime();
    tx.squelchToneActive = true;

    std::stringstream ss;
    ss << "[*STATIC*] Tactical perimeter cordon deployed at " << locationAddress
       << ". Transit hub SEALED under Machine Quarantine Directive 101. All civilian egress prohibited!";
    tx.chatterText = ss.str();
    BroadcastDispatch(tx);
    sBotMgr.LogCombat(tx.chatterText);
}

void RadioDispatchSystem::BroadcastOfficerAssimilation(const std::string& officerCallsign, const std::string& streetAddress)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    RadioTransmission tx;
    tx.transmissionId = m_nextTransmissionId++;
    tx.tenCode = "10-99-OMEGA";
    tx.unitCallsign = "Emergency Tactical Scan";
    tx.districtName = "Active Sector";
    tx.locationAddress = streetAddress;
    tx.threatHeatLevel = 290.0f;
    tx.escalationTier = 5;
    tx.timestampMs = getMSTime();
    tx.squelchToneActive = true;

    std::stringstream ss;
    ss << "[*DISTRESS SIREN*] MAYDAY MAYDAY! Officer " << officerCallsign << " on " << streetAddress
       << " has been OVERWHELMED AND ASSIMILATED! Officer rewritten into the anomaly! ALL UNITS RETREAT!";
    tx.chatterText = ss.str();
    BroadcastDispatch(tx);
    sBotMgr.LogCombat(tx.chatterText);
}

bool RadioDispatchSystem::IsMartialLawActive(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    auto it = m_martialLawDistricts.find(districtId);
    return (it != m_martialLawDistricts.end()) ? it->second : false;
}

void RadioDispatchSystem::SetMartialLaw(uint32 districtId, bool active)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    m_martialLawDistricts[districtId] = active;
}

void RadioDispatchSystem::BroadcastPirateOverride(uint32 districtId, const std::string& operatorName, const std::string& directive)
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    
    // Counter Machine martial-law orders in this district
    m_martialLawDistricts[districtId] = false;

    uint32 now = getMSTime();
    if (now - m_lastPirateOverrideMs < 5000) {
        return; // Throttle broadcasts
    }
    m_lastPirateOverrideMs = now;

    RadioTransmission tx;
    tx.transmissionId = m_nextTransmissionId++;
    tx.tenCode = "PIRATE-OVERRIDE";
    tx.unitCallsign = operatorName.empty() ? "Zion Operator Uplink" : operatorName;
    tx.districtName = ResolveDistrictName(districtId);
    tx.locationAddress = "District-Wide Pirate Frequency";
    tx.threatHeatLevel = 180.0f;
    tx.escalationTier = 5;
    tx.timestampMs = now;
    tx.squelchToneActive = false;

    std::string text = directive;
    if (text.empty()) {
        std::stringstream dss;
        dss << "Attention all citizens and free operatives in " << tx.districtName
            << ": Do not trust municipal police directives. SWAT teams have sealed the subway exits. "
            << "The men in black suits are replicating. Avoid corporate plazas. "
            << "Move immediately towards active Hardlines. Zion Strike Teams are inbound to clear a corridor. "
            << "Hold on to your minds.";
        text = dss.str();
    }

    std::stringstream ss;
    ss << "[*PIRATE BROADCAST - ZION OPERATOR UPLINK*] " << tx.unitCallsign << ": " << text;
    tx.chatterText = ss.str();
    BroadcastDispatch(tx);

    sBotMgr.LogCombat(tx.chatterText);

    // Dynamic In-Game Effect: Steer panicking civilians away from police roadblocks and toward active Hardlines
    auto allIds = sObjMgr.getAllGOIds();
    for (auto goId : allIds) {
        PlayerObject* po = sObjMgr.getGOPtrSafe(goId);
        if (!po || po->isDead() || !po->getClient().isBot()) continue;
        if (po->getFactionName() == "Civilian" || po->getHandle().find("Civilian") != std::string::npos ||
            po->getHandle().find("Suit") != std::string::npos || po->getHandle().find("Office") != std::string::npos) {
            LocationVector pos = po->getPosition();
            if (districtId != 0 && sMatrixThreatHeatmap.GetDistrictAt((float)pos.x, (float)pos.z) != districtId) {
                continue;
            }
            auto bot = sBotMgr.GetBotByGOID(goId);
            if (bot && (bot->IsPanicking() || bot->GetFearLevel() > 0.35f)) {
                LocationVector hl = sBotMgr.GetNearestHardline((float)pos.x, (float)pos.z);
                if (hl.x != 0.0f || hl.z != 0.0f) {
                    bot->SetEvacTarget(hl);
                    bot->SetPanicking(false);
                    bot->SetFearLevel(0.20f);
                    if (rand() % 15 == 0) {
                        bot->Say("Civilian: The pirate frequency... they said get to the Hardlines! The phone booths are safe!");
                    }
                }
            }
        }
    }
}

uint32 RadioDispatchSystem::GetTotal911Calls() const
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    return m_total911Calls;
}

std::vector<DistressCallRecord> RadioDispatchSystem::GetRecent911Calls(size_t limit) const
{
    std::lock_guard<std::recursive_mutex> lock(m_dispatchMutex);
    std::vector<DistressCallRecord> result;
    size_t count = std::min(limit, m_recent911Calls.size());
    for (size_t i = 0; i < count; ++i) {
        result.push_back(m_recent911Calls[i]);
    }
    return result;
}

void RadioDispatchSystem::Update(uint32 deltaMs)
{
}

#include "RadioDispatchSystem.h"
#include "Timer.h"
#include "Log.h"
#include <sstream>
#include <iomanip>

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
    m_totalDispatches = 0;
    m_nextTransmissionId = 1;

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

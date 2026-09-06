#ifndef MXOEMU_RADIO_DISPATCH_SYSTEM_H
#define MXOEMU_RADIO_DISPATCH_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <deque>
#include <mutex>

struct RadioTransmission
{
    uint32 transmissionId{0};
    std::string tenCode{"10-99"};
    std::string unitCallsign{"Dispatch"};
    std::string districtName{"Downtown"};
    std::string locationAddress{"4th and Main St"};
    std::string chatterText;
    float threatHeatLevel{0.0f};
    int escalationTier{1};
    uint32 timestampMs{0};
    bool squelchToneActive{true};
};

struct DistressCallRecord
{
    uint32 callId{0};
    uint32 districtId{1};
    float x{0.0f};
    float z{0.0f};
    std::string streetAddress;
    std::string callerQuote;
    uint32 timestampMs{0};
};

class RadioDispatchSystem : public Singleton<RadioDispatchSystem>
{
public:
    RadioDispatchSystem();
    ~RadioDispatchSystem();

    void Initialize();
    void Reset();
    void Update(uint32 deltaMs);

    // Procedural Scanner Dispatch
    bool GenerateDispatchCall(uint32 districtId, float heatLevel, int escalationTier,
                              float x, float z, RadioTransmission& outTransmission);

    // 911 Distress Reporting & Beat Cop Dispatch
    bool Report911Call(uint32 districtId, float x, float z, const std::string& callerQuote);

    // Machine System Agents Commandeering & Martial Law
    void TriggerAgentOverride(const std::string& agentName, const std::string& directive, uint32 districtId);

    // Tactical Perimeter Cordon & Barricades Broadcast
    void BroadcastCordonOrder(uint32 districtId, const std::string& locationAddress);

    // Officer Overwhelmed & Assimilation Alert
    void BroadcastOfficerAssimilation(const std::string& officerCallsign, const std::string& streetAddress);

    // Martial Law / Quarantine Query & Control
    bool IsMartialLawActive(uint32 districtId) const;
    void SetMartialLaw(uint32 districtId, bool active);

    void BroadcastDispatch(const RadioTransmission& transmission, float rangeUnits = 30000.0f);
    std::vector<RadioTransmission> GetRecentTransmissions(size_t limit = 10) const;
    uint32 GetTotalDispatchCalls() const;
    uint32 GetTotal911Calls() const;
    std::vector<DistressCallRecord> GetRecent911Calls(size_t limit = 10) const;

    std::string ResolveDistrictName(uint32 districtId) const;
    std::string GenerateStreetAddress(float x, float z) const;

private:
    mutable std::recursive_mutex m_dispatchMutex;
    std::deque<RadioTransmission> m_recentTransmissions;
    std::deque<DistressCallRecord> m_recent911Calls;
    std::map<uint32, bool> m_martialLawDistricts;
    uint32 m_nextTransmissionId{1};
    uint32 m_next911CallId{1};
    uint32 m_totalDispatches{0};
    uint32 m_total911Calls{0};
    uint32 m_lastAgentOverrideMs{0};
};

#define sRadioDispatchSystem RadioDispatchSystem::getSingleton()

#endif // MXOEMU_RADIO_DISPATCH_SYSTEM_H

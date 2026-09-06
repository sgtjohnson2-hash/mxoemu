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

class RadioDispatchSystem : public Singleton<RadioDispatchSystem>
{
public:
    RadioDispatchSystem();
    ~RadioDispatchSystem();

    void Initialize();
    void Reset();

    // Procedural Scanner Dispatch
    bool GenerateDispatchCall(uint32 districtId, float heatLevel, int escalationTier,
                              float x, float z, RadioTransmission& outTransmission);

    void BroadcastDispatch(const RadioTransmission& transmission, float rangeUnits = 30000.0f);
    std::vector<RadioTransmission> GetRecentTransmissions(size_t limit = 10) const;
    uint32 GetTotalDispatchCalls() const;

private:
    std::string ResolveDistrictName(uint32 districtId) const;
    std::string GenerateStreetAddress(float x, float z) const;

    mutable std::recursive_mutex m_dispatchMutex;
    std::deque<RadioTransmission> m_recentTransmissions;
    uint32 m_nextTransmissionId{1};
    uint32 m_totalDispatches{0};
};

#define sRadioDispatchSystem RadioDispatchSystem::getSingleton()

#endif // MXOEMU_RADIO_DISPATCH_SYSTEM_H

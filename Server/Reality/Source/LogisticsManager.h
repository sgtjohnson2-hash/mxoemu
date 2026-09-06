#ifndef MXOEMU_LOGISTICSMANAGER_H
#define MXOEMU_LOGISTICSMANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <vector>
#include <string>
#include <map>
#include <memory>

enum CourierType {
    COURIER_ZION_RUNNER = 0,
    COURIER_MEROVINGIAN_SMUGGLER = 1,
    COURIER_MACHINE_PROBE = 2
};

struct CourierRoute {
    std::string name;
    uint32 fromDistrict;
    uint32 toDistrict;
    std::vector<LocationVector> waypoints;
};

struct ActiveCourier {
    uint32 courierGoId;
    CourierType type;
    uint32 faction;
    std::string cargoDescription;
    uint32 cargoValue;
    uint32 routeIndex;
    size_t currentWaypoint;
    std::vector<uint32> escortGoIds;
    uint32 lastCalloutTime;
    bool reachedDestination;
    bool destroyed;
};

struct RelaySubstation {
    uint32 substationId;
    std::string name;
    uint32 districtId;
    LocationVector position;
    uint32 controllingFaction;
    float health;
    float maxHealth;
    bool isSabotaged;
    uint32 sabotageEndTime;
};

class LogisticsManager : public Singleton<LogisticsManager> {
public:
    LogisticsManager();
    ~LogisticsManager();

    void Initialize();
    void Update(uint32 deltaMs);

    // Courier Operations
    bool DispatchCourier(CourierType type, uint32 routeIndex);
    void OnCourierAttacked(uint32 courierGoId, uint32 attackerGoId);
    void OnCourierDestroyed(uint32 courierGoId, uint32 killerGoId);

    // Substation Sabotage & Defense
    bool DamageSubstation(uint32 substationId, float damage, uint32 attackerGoId);
    void RepairSubstation(uint32 substationId, float repairAmount);
    const RelaySubstation* GetSubstation(uint32 substationId) const;
    const std::map<uint32, RelaySubstation>& GetAllSubstations() const { return m_substations; }
    const std::vector<ActiveCourier>& GetActiveCouriers() const { return m_couriers; }

private:
    void SetupRoutes();
    void SetupSubstations();
    void UpdateCouriers(uint32 deltaMs);
    void UpdateSubstations(uint32 deltaMs);

    std::vector<CourierRoute> m_routes;
    std::vector<ActiveCourier> m_couriers;
    std::map<uint32, RelaySubstation> m_substations;
    uint32 m_timeSinceLastCourierDispatch;
};

#define sLogisticsMgr LogisticsManager::getSingleton()

#endif // MXOEMU_LOGISTICSMANAGER_H

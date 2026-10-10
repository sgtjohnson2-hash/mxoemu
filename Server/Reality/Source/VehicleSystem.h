#ifndef MXOEMU_VEHICLESYSTEM_H
#define MXOEMU_VEHICLESYSTEM_H

#include "Common.h"
#include "Singleton.h"

#include <vector>

struct SimulatedVehicle {
    uint32 vehicleId;
    uint32 districtId;
    uint8 vehicleType; // 0: Taxi, 1: Bus, 2: Delivery Van, 3: Civilian Sedan, 4: Police Cruiser
    float x;
    float y;
    float z;
    float heading;
    float speed;
};

class VehicleSystem : public Singleton<VehicleSystem>
{
public:
    VehicleSystem();
    ~VehicleSystem();

    void Initialize();
    void Tick(uint32 currentMs);

    size_t GetActiveVehicleCount() const { return m_vehicles.size(); }
    const std::vector<SimulatedVehicle>& GetVehicles() const { return m_vehicles; }

private:
    uint32 m_lastSpawnMs;
    uint32 m_lastMoveMs;
    uint32 m_nextVehicleId;
    std::vector<SimulatedVehicle> m_vehicles;
};

#define sVehicleSys VehicleSystem::getSingleton()

#endif

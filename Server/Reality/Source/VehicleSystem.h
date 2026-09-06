#ifndef MXOEMU_VEHICLESYSTEM_H
#define MXOEMU_VEHICLESYSTEM_H

#include "Common.h"
#include "Singleton.h"

class VehicleSystem : public Singleton<VehicleSystem>
{
public:
    VehicleSystem();
    ~VehicleSystem();

    void Initialize();
    void Tick(uint32 currentMs);

private:
    uint32 m_lastSpawnMs;
    uint32 m_vehicleCount;
};

#define sVehicleSys VehicleSystem::getSingleton()

#endif

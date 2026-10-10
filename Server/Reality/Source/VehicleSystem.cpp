#include "VehicleSystem.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include <cmath>

createFileSingleton(VehicleSystem);

VehicleSystem::VehicleSystem()
    : m_lastSpawnMs(0), m_lastMoveMs(0), m_nextVehicleId(1)
{
}

VehicleSystem::~VehicleSystem()
{
    m_vehicles.clear();
}

void VehicleSystem::Initialize()
{
    INFO_LOG("VehicleSystem Initialized: Spawning ambient city vehicular traffic.");
    m_vehicles.clear();

    // Seed authentic vehicular fleet across the 4 districts along major avenues
    // Authentic street pavement elevation is 572.0f
    struct InitialSpawn {
        uint32 district;
        uint8 type;
        float x, z;
        float heading;
        float speed;
    };

    static const InitialSpawn seeds[] = {
        {1, 0, 16500.0f,  2000.0f, 0.0f, 18.0f}, // Slums Yellow Taxi
        {1, 3, 17200.0f,  2500.0f, 1.57f, 15.0f}, // Slums Sedan
        {1, 2, 16800.0f,  1800.0f, 3.14f, 12.0f}, // Slums Delivery Van
        {1, 1, 17500.0f,  3000.0f, 4.71f, 10.0f}, // Slums Transit Bus
        {2, 0, -12000.0f, 5000.0f, 0.0f, 20.0f}, // Downtown Taxi
        {2, 3, -11500.0f, 5500.0f, 1.57f, 16.0f}, // Downtown Sedan
        {2, 4, -12500.0f, 4800.0f, 3.14f, 22.0f}, // Downtown Municipal Cruiser
        {2, 1, -11000.0f, 6000.0f, 4.71f, 10.0f}, // Downtown Transit Bus
        {3, 0,  5000.0f, -8000.0f, 0.0f, 20.0f}, // Intl Taxi
        {3, 3,  5500.0f, -7500.0f, 1.57f, 15.0f}, // Intl Sedan
        {3, 2,  4800.0f, -8200.0f, 3.14f, 14.0f}, // Intl Freight Van
        {4, 0, -4000.0f, -4000.0f, 0.0f, 22.0f}, // Richland Luxury Sedan
        {4, 3, -3500.0f, -3800.0f, 1.57f, 18.0f}, // Richland Sedan
        {4, 4, -4200.0f, -4500.0f, 3.14f, 22.0f}  // Richland Police Patrol
    };

    for (const auto& s : seeds) {
        SimulatedVehicle v;
        v.vehicleId = m_nextVehicleId++;
        v.districtId = s.district;
        v.vehicleType = s.type;
        v.x = s.x;
        v.y = 572.0f; // Clamped flush to street pavement
        v.z = s.z;
        v.heading = s.heading;
        v.speed = s.speed;
        m_vehicles.push_back(v);
    }

    INFO_LOG(format("VehicleSystem: %1% ambient transit vehicles deployed across MegaCity road grid.") % m_vehicles.size());
}

void VehicleSystem::Tick(uint32 currentMs)
{
    if (m_lastMoveMs == 0) m_lastMoveMs = currentMs;
    uint32 deltaMs = currentMs - m_lastMoveMs;
    if (deltaMs < 500) return; // Process movement every 500ms
    m_lastMoveMs = currentMs;

    float dt = deltaMs / 1000.0f;

    for (auto& v : m_vehicles) {
        // Move along heading
        v.x += std::cos(v.heading) * v.speed * dt * 100.0f;
        v.z += std::sin(v.heading) * v.speed * dt * 100.0f;
        v.y = 572.0f; // Clamped to pavement

        // Turn at district boundary grid extents
        if (v.districtId == 1) { // Slums
            if (v.x > 19000.0f) { v.x = 19000.0f; v.heading += 1.5708f; }
            if (v.x < 15000.0f) { v.x = 15000.0f; v.heading += 1.5708f; }
            if (v.z > 4000.0f)  { v.z = 4000.0f;  v.heading += 1.5708f; }
            if (v.z < 1000.0f)  { v.z = 1000.0f;  v.heading += 1.5708f; }
        } else if (v.districtId == 2) { // Downtown
            if (v.x > -10000.0f) { v.x = -10000.0f; v.heading += 1.5708f; }
            if (v.x < -14000.0f) { v.x = -14000.0f; v.heading += 1.5708f; }
            if (v.z > 7000.0f)   { v.z = 7000.0f;   v.heading += 1.5708f; }
            if (v.z < 4000.0f)   { v.z = 4000.0f;   v.heading += 1.5708f; }
        }
    }
}

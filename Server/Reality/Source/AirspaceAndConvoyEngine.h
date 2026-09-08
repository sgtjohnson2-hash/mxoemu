#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Epoch IX: Pillar IV & V - Autonomous Airspace Drones & Highway Smuggling Convoys
// Extends Megacity verticality to 120m UAV patrols & physical highway logistics
// ============================================================================

struct Vector3D
{
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vector3D() = default;
    Vector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float DistanceTo(const Vector3D& o) const {
        float dx = x - o.x;
        float dy = y - o.y;
        float dz = z - o.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    Vector3D Normalized() const {
        float len = Length();
        if (len > 0.0001f) return Vector3D(x / len, y / len, z / len);
        return Vector3D(0.0f, 0.0f, 0.0f);
    }
};

struct SurveillanceDrone
{
    uint32_t droneId{0};
    std::string modelName;
    Vector3D currentPos;
    float altitudeMeters{80.0f}; // 60m - 120m
    float headingDeg{0.0f};
    float speedUnitsPerSec{400.0f}; // ~4 m/s in world scale
    Vector3D patrolCenter;
    float patrolRadius{5000.0f};
    float patrolAngleRad{0.0f};
    Vector3D spotlightDirection;
    float spotlightConeAngleDeg{35.0f};
    float spotlightRangeUnits{15000.0f}; // 150m
    uint32_t targetLockGoId{0};
    bool isTargetLocked{false};
    float health{500.0f};
    bool isDestroyed{false};
    bool wreckageSpawned{false};
};

struct SentinelAirspaceSwarm
{
    uint32_t swarmId{0};
    std::string originType; // "SewerGrate", "MetroVent", "SkyboxBreach"
    Vector3D originPos;
    uint32_t sentinelCount{12};
    float heatThreshold{70.0f};
    bool isActive{true};
    uint32_t targetEntityGoId{0};
};

struct HighwaySmugglingConvoy
{
    uint32_t convoyId{0};
    std::string syndicateName; // "The Judas Cabal", "The Byte Cartel", etc.
    std::string cargoManifest; // "Blackmail_Code_Shards", "AP_Sabot_Crates", etc.
    uint32_t cargoUnits{4};
    Vector3D currentPos;
    float speedUnitsPerSec{2500.0f}; // 25 m/s highway cruising speed
    float headingDeg{0.0f};
    std::vector<Vector3D> routeWaypoints;
    size_t currentWaypointIndex{0};
    float leadArmorHP{2500.0f};
    uint32_t escortCruiserCount{2};
    float escortArmorHP{1200.0f};
    bool isHalted{false};
    bool isAmbushed{false};
    bool isCargoSpilled{false};
    std::vector<uint32_t> spilledCrateIds;
};

class AirspaceAndConvoyEngine : public Singleton<AirspaceAndConvoyEngine>
{
public:
    AirspaceAndConvoyEngine();
    ~AirspaceAndConvoyEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Airspace Surveillance Drones (Pillar IV.1)
    uint32_t SpawnDrone(const std::string& model, float x, float y, float z,
                        float altitudeMeters = 80.0f, float patrolRadius = 5000.0f);
    const SurveillanceDrone* GetDrone(uint32_t droneId) const;
    size_t GetActiveDroneCount() const;
    bool ScanTargetWithSpotlight(uint32_t droneId, float targetX, float targetY, float targetZ,
                                 float& outDist, float& outAngleDeg);
    bool LockGroundTarget(uint32_t droneId, uint32_t targetGoId, float targetX, float targetY, float targetZ);
    bool DamageDrone(uint32_t droneId, float damage, bool& outCrashedAndManifested);

    // 2. Subterranean & Sewer Sentinel Swarms (Pillar IV.2)
    uint32_t TriggerSentinelIncursion(const std::string& originType, float x, float y, float z, uint32_t count = 12);
    size_t GetActiveSentinelIncursionCount() const;

    // 3. Autonomous Armored Smuggling Convoys (Pillar V.1)
    uint32_t SpawnConvoy(const std::string& syndicate, const std::string& cargo, uint32_t cargoCount,
                        const std::vector<float>& waypointsXYZ, float speed = 2500.0f);
    const HighwaySmugglingConvoy* GetConvoy(uint32_t convoyId) const;
    size_t GetActiveConvoyCount() const;
    bool AdvanceConvoyAlongRoute(uint32_t convoyId, float dt);
    bool CheckAndTriggerRoadblockInterception(uint32_t convoyId, float& outKineticDamage);
    bool AttackConvoy(uint32_t convoyId, float damage, uint32_t& outCratesSpilled);

    // 4. Dynamic Sierra SWAT Highway Roadblock Interception (Pillar V.2)
    bool DeploySWATInterceptionRoadblock(uint32_t convoyId, float distanceAheadUnits = 3000.0f, uint32_t roadblockId = 9999);

private:
    mutable std::shared_mutex m_engineMutex;
    std::unordered_map<uint32_t, SurveillanceDrone> m_drones;
    std::unordered_map<uint32_t, SentinelAirspaceSwarm> m_swarms;
    std::unordered_map<uint32_t, HighwaySmugglingConvoy> m_convoys;

    uint32_t m_nextDroneId{1};
    uint32_t m_nextSwarmId{1};
    uint32_t m_nextConvoyId{1};
};

#define sAirspaceAndConvoyEngine AirspaceAndConvoyEngine::getSingleton()

void RunAirspaceAndConvoyTestSuite();

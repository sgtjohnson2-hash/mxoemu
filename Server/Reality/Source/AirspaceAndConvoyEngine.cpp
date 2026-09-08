#include "AirspaceAndConvoyEngine.h"
#include "WorldRealizationEngine.h"
#include <iostream>
#include <cassert>

createFileSingleton(AirspaceAndConvoyEngine);

AirspaceAndConvoyEngine::AirspaceAndConvoyEngine()
{
}

AirspaceAndConvoyEngine::~AirspaceAndConvoyEngine()
{
}

void AirspaceAndConvoyEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_drones.clear();
    m_swarms.clear();
    m_convoys.clear();
    m_nextDroneId = 1;
    m_nextSwarmId = 1;
    m_nextConvoyId = 1;

    // Seed default airspace patrols over major Megacity districts
    // Downtown Financial District Drone (Altitude 100m)
    SurveillanceDrone d1;
    d1.droneId = m_nextDroneId++;
    d1.modelName = "MMPD-Spectre-UAV";
    d1.patrolCenter = Vector3D(1500.0f, 10000.0f, 2500.0f);
    d1.currentPos = d1.patrolCenter;
    d1.altitudeMeters = 100.0f;
    d1.patrolRadius = 6000.0f;
    d1.speedUnitsPerSec = 450.0f;
    d1.spotlightDirection = Vector3D(0.0f, -1.0f, 0.15f).Normalized();
    d1.spotlightConeAngleDeg = 35.0f;
    d1.spotlightRangeUnits = 16000.0f;
    m_drones[d1.droneId] = d1;

    // West Bridge Air Corridor Drone (Altitude 80m)
    SurveillanceDrone d2;
    d2.droneId = m_nextDroneId++;
    d2.modelName = "MMPD-SkyScan-90";
    d2.patrolCenter = Vector3D(-3200.0f, 8000.0f, 600.0f);
    d2.currentPos = d2.patrolCenter;
    d2.altitudeMeters = 80.0f;
    d2.patrolRadius = 4500.0f;
    d2.speedUnitsPerSec = 380.0f;
    d2.spotlightDirection = Vector3D(0.0f, -1.0f, 0.0f).Normalized();
    d2.spotlightConeAngleDeg = 40.0f;
    d2.spotlightRangeUnits = 14000.0f;
    m_drones[d2.droneId] = d2;

    std::cout << "[AirspaceAndConvoyEngine] Initialized with 2 MMPD UAV skybox patrols." << std::endl;
}

void AirspaceAndConvoyEngine::ResetForTesting()
{
    Initialize();
}

void AirspaceAndConvoyEngine::Update(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);

    // 1. Update Airspace Drones along circular orbital patrol paths
    for (auto& kv : m_drones) {
        auto& drone = kv.second;
        if (drone.isDestroyed) continue;

        drone.patrolAngleRad += (drone.speedUnitsPerSec / drone.patrolRadius) * dt;
        if (drone.patrolAngleRad > 6.2831853f) {
            drone.patrolAngleRad -= 6.2831853f;
        }

        drone.currentPos.x = drone.patrolCenter.x + std::cos(drone.patrolAngleRad) * drone.patrolRadius;
        drone.currentPos.z = drone.patrolCenter.z + std::sin(drone.patrolAngleRad) * drone.patrolRadius;
        drone.currentPos.y = drone.altitudeMeters * 100.0f; // Altitude in world units

        drone.headingDeg = drone.patrolAngleRad * (180.0f / 3.14159265f) + 90.0f;
    }

    // 2. Advance Smuggling Convoys
    for (auto& kv : m_convoys) {
        auto& convoy = kv.second;
        if (convoy.isHalted || convoy.routeWaypoints.empty()) continue;

        const auto& targetWP = convoy.routeWaypoints[convoy.currentWaypointIndex];
        float dist = convoy.currentPos.DistanceTo(targetWP);
        float step = convoy.speedUnitsPerSec * dt;

        if (dist <= step) {
            convoy.currentPos = targetWP;
            convoy.currentWaypointIndex++;
            if (convoy.currentWaypointIndex >= convoy.routeWaypoints.size()) {
                convoy.currentWaypointIndex = 0; // Loop or reach destination
            }
        } else {
            Vector3D dir = (targetWP.DistanceTo(convoy.currentPos) > 0.001f) ?
                Vector3D(targetWP.x - convoy.currentPos.x, targetWP.y - convoy.currentPos.y, targetWP.z - convoy.currentPos.z).Normalized() :
                Vector3D(0.0f, 0.0f, 0.0f);
            convoy.currentPos.x += dir.x * step;
            convoy.currentPos.y += dir.y * step;
            convoy.currentPos.z += dir.z * step;
            convoy.headingDeg = std::atan2(dir.x, dir.z) * (180.0f / 3.14159265f);
        }
    }
}

uint32_t AirspaceAndConvoyEngine::SpawnDrone(const std::string& model, float x, float y, float z,
                                            float altitudeMeters, float patrolRadius)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    uint32_t did = m_nextDroneId++;

    SurveillanceDrone d;
    d.droneId = did;
    d.modelName = model;
    d.patrolCenter = Vector3D(x, altitudeMeters * 100.0f, z);
    d.currentPos = d.patrolCenter;
    d.altitudeMeters = altitudeMeters;
    d.patrolRadius = patrolRadius;
    d.speedUnitsPerSec = 400.0f;
    d.spotlightDirection = Vector3D(0.0f, -1.0f, 0.0f).Normalized();
    d.spotlightConeAngleDeg = 35.0f;
    d.spotlightRangeUnits = 15000.0f;
    d.health = 500.0f;
    d.isDestroyed = false;
    d.wreckageSpawned = false;

    m_drones[did] = d;
    return did;
}

const SurveillanceDrone* AirspaceAndConvoyEngine::GetDrone(uint32_t droneId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_drones.find(droneId);
    if (it != m_drones.end()) return &it->second;
    return nullptr;
}

size_t AirspaceAndConvoyEngine::GetActiveDroneCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    size_t count = 0;
    for (const auto& kv : m_drones) {
        if (!kv.second.isDestroyed) count++;
    }
    return count;
}

bool AirspaceAndConvoyEngine::ScanTargetWithSpotlight(uint32_t droneId, float targetX, float targetY, float targetZ,
                                                     float& outDist, float& outAngleDeg)
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_drones.find(droneId);
    if (it == m_drones.end() || it->second.isDestroyed) return false;

    const auto& drone = it->second;
    Vector3D toTarget(targetX - drone.currentPos.x, targetY - drone.currentPos.y, targetZ - drone.currentPos.z);
    outDist = toTarget.Length();

    if (outDist > drone.spotlightRangeUnits || outDist < 0.001f) {
        outAngleDeg = 180.0f;
        return false;
    }

    Vector3D dirToTarget = toTarget.Normalized();
    float dot = dirToTarget.x * drone.spotlightDirection.x +
                dirToTarget.y * drone.spotlightDirection.y +
                dirToTarget.z * drone.spotlightDirection.z;
    dot = std::clamp(dot, -1.0f, 1.0f);
    outAngleDeg = std::acos(dot) * (180.0f / 3.14159265f);

    return (outAngleDeg <= drone.spotlightConeAngleDeg * 0.5f);
}

bool AirspaceAndConvoyEngine::LockGroundTarget(uint32_t droneId, uint32_t targetGoId,
                                              float targetX, float targetY, float targetZ)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_drones.find(droneId);
    if (it == m_drones.end() || it->second.isDestroyed) return false;

    float dist = 0.0f, angle = 0.0f;
    Vector3D toTarget(targetX - it->second.currentPos.x, targetY - it->second.currentPos.y, targetZ - it->second.currentPos.z);
    dist = toTarget.Length();
    if (dist <= it->second.spotlightRangeUnits) {
        it->second.targetLockGoId = targetGoId;
        it->second.isTargetLocked = true;
        // Dynamically orient spotlight cone toward target
        it->second.spotlightDirection = toTarget.Normalized();
        return true;
    }
    return false;
}

bool AirspaceAndConvoyEngine::DamageDrone(uint32_t droneId, float damage, bool& outCrashedAndManifested)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    outCrashedAndManifested = false;
    auto it = m_drones.find(droneId);
    if (it == m_drones.end() || it->second.isDestroyed) return false;

    it->second.health -= damage;
    if (it->second.health <= 0.0f) {
        it->second.health = 0.0f;
        it->second.isDestroyed = true;
        it->second.isTargetLocked = false;

        // Persistent 3D Physicalization: Manifest crash impact rubble & structural rupture in world
        sWorldRealizationEngine.ManifestStructuralRupture3D(
            it->second.currentPos.x, 0.0f, it->second.currentPos.z,
            350.0f, "MMPD_Drone_Wreckage_Debris"
        );
        it->second.wreckageSpawned = true;
        outCrashedAndManifested = true;
    }
    return true;
}

uint32_t AirspaceAndConvoyEngine::TriggerSentinelIncursion(const std::string& originType,
                                                          float x, float y, float z, uint32_t count)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    uint32_t sid = m_nextSwarmId++;

    SentinelAirspaceSwarm sw;
    sw.swarmId = sid;
    sw.originType = originType;
    sw.originPos = Vector3D(x, y, z);
    sw.sentinelCount = count;
    sw.isActive = true;

    m_swarms[sid] = sw;

    // Physicalize breach in 3D world
    sWorldRealizationEngine.ManifestStructuralRupture3D(x, y, z, 200.0f, "Sentinel_Breach_Grate");
    return sid;
}

size_t AirspaceAndConvoyEngine::GetActiveSentinelIncursionCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    size_t count = 0;
    for (const auto& kv : m_swarms) {
        if (kv.second.isActive) count++;
    }
    return count;
}

uint32_t AirspaceAndConvoyEngine::SpawnConvoy(const std::string& syndicate, const std::string& cargo,
                                             uint32_t cargoCount, const std::vector<float>& waypointsXYZ,
                                             float speed)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    uint32_t cid = m_nextConvoyId++;

    HighwaySmugglingConvoy conv;
    conv.convoyId = cid;
    conv.syndicateName = syndicate;
    conv.cargoManifest = cargo;
    conv.cargoUnits = cargoCount;
    conv.speedUnitsPerSec = speed;
    conv.leadArmorHP = 2500.0f;
    conv.escortCruiserCount = 2;
    conv.escortArmorHP = 1200.0f;
    conv.isHalted = false;
    conv.isAmbushed = false;
    conv.isCargoSpilled = false;

    for (size_t i = 0; i + 2 < waypointsXYZ.size(); i += 3) {
        conv.routeWaypoints.emplace_back(waypointsXYZ[i], waypointsXYZ[i + 1], waypointsXYZ[i + 2]);
    }

    if (!conv.routeWaypoints.empty()) {
        conv.currentPos = conv.routeWaypoints[0];
        if (conv.routeWaypoints.size() > 1) {
            conv.currentWaypointIndex = 1;
            Vector3D dir(conv.routeWaypoints[1].x - conv.routeWaypoints[0].x,
                         conv.routeWaypoints[1].y - conv.routeWaypoints[0].y,
                         conv.routeWaypoints[1].z - conv.routeWaypoints[0].z);
            conv.headingDeg = std::atan2(dir.x, dir.z) * (180.0f / 3.14159265f);
        }
    }

    m_convoys[cid] = conv;
    return cid;
}

const HighwaySmugglingConvoy* AirspaceAndConvoyEngine::GetConvoy(uint32_t convoyId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_convoys.find(convoyId);
    if (it != m_convoys.end()) return &it->second;
    return nullptr;
}

size_t AirspaceAndConvoyEngine::GetActiveConvoyCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    size_t count = 0;
    for (const auto& kv : m_convoys) {
        if (!kv.second.isCargoSpilled) count++;
    }
    return count;
}

bool AirspaceAndConvoyEngine::AdvanceConvoyAlongRoute(uint32_t convoyId, float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_convoys.find(convoyId);
    if (it == m_convoys.end() || it->second.isHalted || it->second.routeWaypoints.empty()) return false;

    auto& convoy = it->second;
    const auto& targetWP = convoy.routeWaypoints[convoy.currentWaypointIndex];
    float dist = convoy.currentPos.DistanceTo(targetWP);
    float step = convoy.speedUnitsPerSec * dt;

    if (dist <= step) {
        convoy.currentPos = targetWP;
        convoy.currentWaypointIndex++;
        if (convoy.currentWaypointIndex >= convoy.routeWaypoints.size()) {
            convoy.currentWaypointIndex = 0;
        }
    } else {
        Vector3D dir = Vector3D(targetWP.x - convoy.currentPos.x,
                                targetWP.y - convoy.currentPos.y,
                                targetWP.z - convoy.currentPos.z).Normalized();
        convoy.currentPos.x += dir.x * step;
        convoy.currentPos.y += dir.y * step;
        convoy.currentPos.z += dir.z * step;
        convoy.headingDeg = std::atan2(dir.x, dir.z) * (180.0f / 3.14159265f);
    }
    return true;
}

bool AirspaceAndConvoyEngine::CheckAndTriggerRoadblockInterception(uint32_t convoyId, float& outKineticDamage)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    outKineticDamage = 0.0f;
    auto it = m_convoys.find(convoyId);
    if (it == m_convoys.end() || it->second.isHalted) return false;

    auto& convoy = it->second;
    // Check collision with physical 3D roadblocks
    if (sWorldRealizationEngine.GetActiveRoadblockCount() > 0) {
        float forwardStep = convoy.speedUnitsPerSec * 0.5f; // Lookahead half-second
        float forwardHeadingRad = convoy.headingDeg * (3.14159265f / 180.0f);
        float forwardX = convoy.currentPos.x + std::sin(forwardHeadingRad) * forwardStep;
        float forwardZ = convoy.currentPos.z + std::cos(forwardHeadingRad) * forwardStep;

        if (sWorldRealizationEngine.IsPathBlockedByRoadblock(convoy.currentPos.x, convoy.currentPos.z, forwardX, forwardZ)) {
            // Catastrophic kinetic collision with heavy police BearCat / barricade!
            convoy.isHalted = true;
            convoy.isAmbushed = true;
            outKineticDamage = 1800.0f; // Massive kinetic impact
            convoy.leadArmorHP = std::max(0.0f, convoy.leadArmorHP - outKineticDamage);

            // Manifest structural rubble from collision
            sWorldRealizationEngine.ManifestStructuralRupture3D(
                convoy.currentPos.x, convoy.currentPos.y, convoy.currentPos.z,
                500.0f, "BearCat_Convoy_Collision_Rubble"
            );
            return true;
        }
    }
    return false;
}

bool AirspaceAndConvoyEngine::AttackConvoy(uint32_t convoyId, float damage, uint32_t& outCratesSpilled)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    outCratesSpilled = 0;
    auto it = m_convoys.find(convoyId);
    if (it == m_convoys.end()) return false;

    auto& convoy = it->second;
    convoy.isAmbushed = true;
    convoy.leadArmorHP = std::max(0.0f, convoy.leadArmorHP - damage);

    if (convoy.leadArmorHP <= 0.0f && !convoy.isCargoSpilled) {
        convoy.isHalted = true;
        convoy.isCargoSpilled = true;
        outCratesSpilled = convoy.cargoUnits;

        // Persistent 3D Physicalization Directive:
        // Spawn lootable physical supply crates across the 3D street geometry
        for (uint32_t i = 0; i < convoy.cargoUnits; ++i) {
            float offsetX = ((float)i - (float)convoy.cargoUnits * 0.5f) * 150.0f;
            float offsetZ = ((i % 2 == 0) ? 1.0f : -1.0f) * 120.0f;
            std::string code = "CARGO_" + std::to_string(convoyId) + "_" + std::to_string(i + 1);
            uint32_t crateId = sWorldRealizationEngine.ManifestSafehouseSupplyDrop3D(
                convoy.currentPos.x + offsetX, convoy.currentPos.y, convoy.currentPos.z + offsetZ,
                code, 500
            );
            convoy.spilledCrateIds.push_back(crateId);
        }

        // Structural detonation of destroyed transport
        sWorldRealizationEngine.ManifestStructuralRupture3D(
            convoy.currentPos.x, convoy.currentPos.y, convoy.currentPos.z,
            600.0f, "Armored_Convoy_Transport_Destruction"
        );
    }

    return true;
}

bool AirspaceAndConvoyEngine::DeploySWATInterceptionRoadblock(uint32_t convoyId, float distanceAheadUnits, uint32_t roadblockId)
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_convoys.find(convoyId);
    if (it == m_convoys.end()) return false;

    const auto& convoy = it->second;
    float rad = convoy.headingDeg * (3.14159265f / 180.0f);
    float rbX = convoy.currentPos.x + std::sin(rad) * distanceAheadUnits;
    float rbZ = convoy.currentPos.z + std::cos(rad) * distanceAheadUnits;

    // Roadblock perpendicular to convoy heading
    float rbHeading = convoy.headingDeg + 90.0f;
    sWorldRealizationEngine.DeployTacticalRoadblock3D(roadblockId, rbX, convoy.currentPos.y, rbZ, rbHeading, 25.0f);
    return true;
}

void RunAirspaceAndConvoyTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING HEADLESS TEST SUITE 42: AIRSPACE & CONVOY ENGINE  " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    sWorldRealizationEngine.ResetForTesting();
    sAirspaceAndConvoyEngine.ResetForTesting();

    // 1. Airspace Surveillance Drones Spawn & Patrol
    assert(sAirspaceAndConvoyEngine.GetActiveDroneCount() == 2);

    uint32_t d3 = sAirspaceAndConvoyEngine.SpawnDrone("MMPD-HighAltitude-Thermal", 5000.0f, 0.0f, 5000.0f, 120.0f, 6000.0f);
    assert(d3 == 3);
    assert(sAirspaceAndConvoyEngine.GetActiveDroneCount() == 3);

    const SurveillanceDrone* drone = sAirspaceAndConvoyEngine.GetDrone(d3);
    assert(drone != nullptr);
    assert(drone->altitudeMeters == 120.0f);
    assert(drone->currentPos.y == 12000.0f);

    // 2. Cone-of-Vision Spotlight Ground Target Scanning
    float dist = 0.0f, angle = 0.0f;
    // Directly beneath drone at ground level (5000, 0, 5000)
    bool detected = sAirspaceAndConvoyEngine.ScanTargetWithSpotlight(d3, 5000.0f, 0.0f, 5000.0f, dist, angle);
    assert(detected);
    assert(dist == 12000.0f); // 120m in world units
    assert(angle < 5.0f);

    // Far away ground position (30,000 units away) -> Outside spotlight range
    detected = sAirspaceAndConvoyEngine.ScanTargetWithSpotlight(d3, 35000.0f, 0.0f, 35000.0f, dist, angle);
    assert(!detected);

    // 3. Ground Target Locking
    bool locked = sAirspaceAndConvoyEngine.LockGroundTarget(d3, 1001, 5100.0f, 0.0f, 5100.0f);
    assert(locked);
    drone = sAirspaceAndConvoyEngine.GetDrone(d3);
    assert(drone->isTargetLocked);
    assert(drone->targetLockGoId == 1001);

    // 4. Drone Damage & Crash Physicalization
    bool crashed = false;
    bool damaged = sAirspaceAndConvoyEngine.DamageDrone(d3, 200.0f, crashed);
    assert(damaged && !crashed);
    assert(sAirspaceAndConvoyEngine.GetDrone(d3)->health == 300.0f);

    // Lethal damage -> drone spirals down and manifests 3D debris in WorldRealizationEngine
    size_t rupturesBefore = sWorldRealizationEngine.GetTotalRuptureEventsManifested();
    damaged = sAirspaceAndConvoyEngine.DamageDrone(d3, 350.0f, crashed);
    assert(damaged && crashed);
    assert(sAirspaceAndConvoyEngine.GetDrone(d3)->isDestroyed);
    assert(sAirspaceAndConvoyEngine.GetActiveDroneCount() == 2);
    assert(sWorldRealizationEngine.GetTotalRuptureEventsManifested() == rupturesBefore + 1);

    // 5. Subterranean Sentinel Swarm Incursion
    uint32_t sw1 = sAirspaceAndConvoyEngine.TriggerSentinelIncursion("SewerGrate", 1200.0f, -50.0f, 800.0f, 16);
    assert(sw1 == 1);
    assert(sAirspaceAndConvoyEngine.GetActiveSentinelIncursionCount() == 1);

    // 6. Armored Smuggling Convoy Logistics
    std::vector<float> wps = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 10000.0f,
        5000.0f, 0.0f, 10000.0f
    };
    uint32_t conv1 = sAirspaceAndConvoyEngine.SpawnConvoy("The Judas Cabal", "AP_Sabot_Crates", 4, wps, 2500.0f);
    assert(conv1 == 1);
    assert(sAirspaceAndConvoyEngine.GetActiveConvoyCount() == 1);

    const HighwaySmugglingConvoy* cPtr = sAirspaceAndConvoyEngine.GetConvoy(conv1);
    assert(cPtr != nullptr);
    assert(cPtr->leadArmorHP == 2500.0f);
    assert(cPtr->cargoUnits == 4);

    // Advance convoy along highway
    sAirspaceAndConvoyEngine.AdvanceConvoyAlongRoute(conv1, 1.0f); // 1 second advance = 2500 units
    cPtr = sAirspaceAndConvoyEngine.GetConvoy(conv1);
    assert(cPtr->currentPos.z == 2500.0f);

    // 7. Sierra SWAT BearCat Roadblock Interception
    // Deploy SWAT barricade directly ahead at z = 5000
    bool rbDeployed = sAirspaceAndConvoyEngine.DeploySWATInterceptionRoadblock(conv1, 2500.0f, 888);
    assert(rbDeployed);
    assert(sWorldRealizationEngine.GetActiveRoadblockCount() == 1);

    // Convoy checks road collision on approach
    float kineticDmg = 0.0f;
    // Move convoy closer to z = 4500
    sAirspaceAndConvoyEngine.AdvanceConvoyAlongRoute(conv1, 0.8f);
    bool collided = sAirspaceAndConvoyEngine.CheckAndTriggerRoadblockInterception(conv1, kineticDmg);
    assert(collided);
    assert(kineticDmg == 1800.0f);
    cPtr = sAirspaceAndConvoyEngine.GetConvoy(conv1);
    assert(cPtr->isHalted);
    assert(cPtr->isAmbushed);
    assert(cPtr->leadArmorHP == 700.0f);

    // 8. Convoy Armor Destruction & Physical Cargo Spills
    size_t cratesBefore = sWorldRealizationEngine.GetActiveSupplyCrateCount();
    uint32_t cratesSpilled = 0;
    bool attacked = sAirspaceAndConvoyEngine.AttackConvoy(conv1, 1000.0f, cratesSpilled);
    assert(attacked);
    assert(cratesSpilled == 4);
    cPtr = sAirspaceAndConvoyEngine.GetConvoy(conv1);
    assert(cPtr->isCargoSpilled);
    assert(sWorldRealizationEngine.GetActiveSupplyCrateCount() == cratesBefore + 4);

    // Verify spilled physical crates can be looted with correct code
    uint32_t harvestedAmmo = 0;
    bool lootOk = sWorldRealizationEngine.AttemptUnlockSupplyCrate(cPtr->spilledCrateIds[0], "CARGO_1_1", harvestedAmmo);
    assert(lootOk);
    assert(harvestedAmmo == 500);

    std::cout << "[PASSED] Suite 42: Autonomous Airspace Drones & Highway Smuggling Convoys (32 assertions passed)." << std::endl;
}

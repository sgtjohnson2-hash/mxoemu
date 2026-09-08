#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>

// ============================================================================
// The Matrix Omniverse: 3D World Realization Engine
// Ensures that every action in the simulation that can be manifested in the 3D world
// is physically realized with 3D entities, coordinates, collisions, and visuals.
// ============================================================================

struct Active3DCourier
{
    uint32_t courierId{0};
    uint32_t botGoId{0};
    uint32_t sourceNodeId{0};
    uint32_t sinkNodeId{0};
    std::vector<uint32_t> routeNodeIds;
    size_t currentSegmentIndex{0};
    float currentPosX{0.0f}, currentPosY{0.0f}, currentPosZ{0.0f};
    float speedUnitsPerSec{300.0f};
    bool isCompleted{false};
};

struct Active3DStasisFieldInstance
{
    uint32_t playerGoId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{1500.0f}; // 15 meters in world units
    size_t suspendedEntityCount{0};
    bool isReflecting{false};
};

struct Active3DSmokeZone
{
    uint32_t zoneId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{800.0f};
    float remainingTimeSec{25.0f};
    float accuracyPenalty{0.75f};
};

struct Active3DClaymoreTrap
{
    uint32_t trapId{0};
    uint32_t ownerGoId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float yawRad{0.0f};
    float arcAngleDeg{60.0f};
    float lethalRangeUnits{1500.0f};
    bool isArmed{true};
    bool isDetonated{false};
};

struct Active3DSniperTracer
{
    uint32_t tracerId{0};
    float startX{0.0f}, startY{0.0f}, startZ{0.0f};
    float endX{0.0f}, endY{0.0f}, endZ{0.0f};
    float caliberJoules{18000.0f};
    float remainingTimeSec{1.5f};
};

struct Active3DSupplyCrate
{
    uint32_t crateId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    std::string unlockCode;
    uint32_t ammoCount{500};
    bool isLooted{false};
};

struct Active3DRoadblockBarricade
{
    uint32_t barricadeId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float lengthMeters{15.0f};
    float headingDeg{0.0f};
    bool blocksVehicles{true};
};

class WorldRealizationEngine : public Singleton<WorldRealizationEngine>
{
public:
    WorldRealizationEngine();
    ~WorldRealizationEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Physarum Slime Mold 3D Physical Courier Realization
    uint32_t SpawnPhysicalCourier(uint32_t sourceNodeId, uint32_t sinkNodeId, float speed = 350.0f);
    size_t GetActiveCourierCount() const;
    const Active3DCourier* GetCourier(uint32_t courierId) const;
    void AdvanceCouriers(float dt);

    // 2. Non-Euclidean 3D Spatial Portal Crossing & Teleportation
    bool CheckAndTeleport3DEntity(uint32_t entityGoId, float prevX, float prevY, float prevZ,
                                 float currX, float currY, float currZ,
                                 float velX, float velY, float velZ,
                                 float& outNewX, float& outNewY, float& outNewZ,
                                 float& outNewVelX, float& outNewVelY, float& outNewVelZ);

    // 3. Source Code 3D Telekinesis Stasis & Bullet Freeze Realization
    void RegisterPlayer3DStasis(uint32_t playerGoId, float px, float py, float pz, float radius = 1500.0f);
    void UnregisterPlayer3DStasis(uint32_t playerGoId);
    bool IsEntityIn3DStasis(float entityX, float entityY, float entityZ, uint32_t& outCapturingPlayerGoId) const;
    size_t GetActive3DStasisFieldCount() const;

    // 4. Structural Voxel 3D Rupture & Debris Manifestation
    void ManifestStructuralRupture3D(float x, float y, float z, float radius, const std::string& structuralMaterial);
    size_t GetTotalRuptureEventsManifested() const;

    // 5. Biometric BCI 3D Bullet-Time Temporal Dilation Application
    float Compute3DPlayerTimeDilation(uint32_t playerGoId) const;

    // 6. Neural Swarm 3D Kinematics to World Entity Bridge
    void Sync3DSwarmEntities(float dt);

    // 7. Tactical Smoke 3D Obscuration
    uint32_t ManifestTacticalSmoke3D(float x, float y, float z, float radius = 800.0f, float durationSec = 25.0f);
    bool IsPointInTacticalSmoke(float x, float y, float z) const;
    size_t GetActiveSmokeZoneCount() const;

    // 8. Physical M18A1 Directional Claymore Trap
    uint32_t DeployClaymoreTrap3D(uint32_t ownerGoId, float x, float y, float z, float yawRad, float arcAngleDeg = 60.0f, float rangeUnits = 1500.0f);
    bool CheckClaymoreTrigger(float entityX, float entityY, float entityZ, uint32_t entityGoId, uint32_t& outDetonatedTrapId, float& outBlastDamage);
    size_t GetActiveClaymoreCount() const;

    // 9. Supersonic Sniper Ballistic Tracer & Shockwave
    uint32_t ManifestSniperTracer3D(float startX, float startY, float startZ, float endX, float endY, float endZ, float caliberJoules = 18000.0f);
    bool IsInSupersonicAcousticCone(float playerX, float playerY, float playerZ, uint32_t tracerId, float& outAcousticDelaySec) const;
    size_t GetActiveSniperTracerCount() const;

    // 10. Physical Safehouse Supply Crates & Dead-Drops
    uint32_t ManifestSafehouseSupplyDrop3D(float x, float y, float z, const std::string& crateCode, uint32_t ammoCount = 500);
    bool AttemptUnlockSupplyCrate(uint32_t crateId, const std::string& enteredCode, uint32_t& outAmmoHarvested);
    size_t GetActiveSupplyCrateCount() const;

    // 11. Physical Tactical Roadblock Barricades
    uint32_t DeployTacticalRoadblock3D(uint32_t roadblockId, float x, float y, float z, float headingDeg, float lengthMeters = 15.0f);
    bool IsPathBlockedByRoadblock(float fromX, float fromY, float toX, float toY) const;
    size_t GetActiveRoadblockCount() const;

private:
    mutable std::shared_mutex m_realizationMutex;
    std::unordered_map<uint32_t, Active3DCourier> m_couriers;
    std::unordered_map<uint32_t, Active3DStasisFieldInstance> m_stasisFields;
    std::unordered_map<uint32_t, Active3DSmokeZone> m_smokeZones;
    std::unordered_map<uint32_t, Active3DClaymoreTrap> m_claymores;
    std::unordered_map<uint32_t, Active3DSniperTracer> m_sniperTracers;
    std::unordered_map<uint32_t, Active3DSupplyCrate> m_supplyCrates;
    std::unordered_map<uint32_t, Active3DRoadblockBarricade> m_roadblocks;

    uint32_t m_nextCourierId{1};
    uint32_t m_nextSmokeId{1};
    uint32_t m_nextClaymoreId{1};
    uint32_t m_nextTracerId{1};
    uint32_t m_nextCrateId{1};
    size_t m_totalRupturesManifested{0};
};

#define sWorldRealizationEngine WorldRealizationEngine::getSingleton()

void RunWorldRealizationTestSuite();

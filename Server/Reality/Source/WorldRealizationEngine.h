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

private:
    mutable std::shared_mutex m_realizationMutex;
    std::unordered_map<uint32_t, Active3DCourier> m_couriers;
    std::unordered_map<uint32_t, Active3DStasisFieldInstance> m_stasisFields;
    uint32_t m_nextCourierId{1};
    size_t m_totalRupturesManifested{0};
};

#define sWorldRealizationEngine WorldRealizationEngine::getSingleton()

void RunWorldRealizationTestSuite();

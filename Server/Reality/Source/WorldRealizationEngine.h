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

struct Active3DMachineEnergyArc
{
    uint32_t arcId{0};
    float startX{0.0f}, startY{0.0f}, startZ{0.0f};
    float targetX{0.0f}, targetY{0.0f}, targetZ{0.0f};
    float voltageMV{50.0f};
    float remainingTimeSec{1.2f};
};

struct Active3DQuantumDistortion
{
    uint32_t distortionId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{500.0f};
    float remainingTimeSec{2.0f};
};

struct Active3DPhysicsBubble
{
    uint32_t bubbleId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{1200.0f};
    float customGravity{0.0f};
    float timeDilation{0.2f};
};

struct Active3DCodeDissolution
{
    uint32_t dissolutionId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{10.0f};
    float density{1.0f};
    float remainingTimeSec{3.0f};
};

struct Active3DSeismicTremor
{
    uint32_t tremorId{0};
    float posX{0.0f}, posZ{0.0f};
    float magnitude{5.5f};
    float depthKm{15.0f};
    float remainingTimeSec{5.0f};
};

struct Active3DHiveSynapse
{
    uint32_t synapseId{0};
    uint32_t srcId{0};
    uint32_t dstId{0};
    std::string type{"MachineConsensus"};
    float remainingTimeSec{2.0f};
};

struct Active3DPanicCrowd
{
    uint32_t crowdId{0};
    float posX{0.0f}, posZ{0.0f};
    float severity{1.0f};
    uint32_t civilianCount{50};
    float remainingTimeSec{15.0f};
};

struct Active3DTemporalEcho
{
    uint32_t echoId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float durationSec{3.0f};
    float remainingTimeSec{3.0f};
};

struct Active3DCyclePortal
{
    uint32_t portalId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    int cycle{1};
    float remainingTimeSec{10.0f};
};

struct Active3DThreatWave
{
    uint32_t waveId{0};
    float posX{0.0f}, posZ{0.0f};
    float currentRadius{0.0f};
    float maxRadius{500.0f};
    float remainingTimeSec{3.0f};
};

struct Active3DVoxelCover
{
    uint32_t coverId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float width{2.0f};
    float height{1.5f};
    float health{500.0f};
    float remainingTimeSec{30.0f};
};

struct Active3DSentinelEclipse
{
    uint32_t eclipseId{0};
    float coveragePercent{0.9f};
    float remainingTimeSec{20.0f};
};

struct Active3DArchitectBeam
{
    uint32_t beamId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float remainingTimeSec{4.0f};
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

    // 12. 01 Machine City Energy Arcs & Geothermal Discharges
    uint32_t ManifestMachineEnergyArc3D(float startX, float startY, float startZ,
                                        float targetX, float targetY, float targetZ, float voltageMV = 50.0f);
    size_t GetActiveMachineEnergyArcCount() const;

    // 13. Quantum Superposition Wavefunction Collapse Distortions
    uint32_t ManifestQuantumCollapseDistortion3D(float x, float y, float z, float radius = 500.0f);
    size_t GetActiveQuantumDistortionCount() const;

    // 14. Subterranean Utility Ruptures (High-pressure steam / High-voltage sparks)
    void ManifestSubterraneanUtilityRupture3D(float x, float y, float z, const std::string& utilityType = "High_Pressure_Steam");

    // 15. Local Reality Reshaping AST Physics Bubbles
    void RegisterPhysicsConstantBubble3D(uint32_t bubbleId, float x, float y, float z,
                                         float radius = 1200.0f, float customGravity = 0.0f, float timeDilation = 0.2f);
    void UnregisterPhysicsConstantBubble3D(uint32_t bubbleId);
    bool IsPointInPhysicsBubble(float x, float y, float z, float& outGravity, float& outDilation) const;
    size_t GetActivePhysicsBubbleCount() const;

    // 16. Sub-Atomic Code Lattice 3D Dissolution
    uint32_t ManifestCodeDissolution3D(float x, float y, float z, float radius = 10.0f, float density = 1.0f);
    size_t GetActiveCodeDissolutionCount() const;

    // 17. Cosmic Planetary 3D Seismic Tremor & Mantle Realization
    uint32_t TriggerMegacitySeismicTremor3D(float x, float z, float magnitude = 5.5f, float depth = 15.0f);
    size_t GetActiveSeismicTremorCount() const;

    // 18. Hyper-Scale Sentience 3D Hive Mind Synaptic Arcs
    uint32_t ManifestHiveMindSynapse3D(uint32_t srcId, uint32_t dstId, const std::string& type = "MachineConsensus");
    size_t GetActiveHiveSynapseCount() const;

    // 19. Autonomous Economy 3D Financial Panic Crowd Realization
    uint32_t ManifestFinancialPanicCrowd3D(float x, float z, float severity = 1.0f);
    size_t GetActivePanicCrowdCount() const;

    // 20. Trans-Dimensional Chronos 3D Temporal Phantom Echoes
    uint32_t ManifestTemporalEcho3D(uint32_t echoId, float x, float y, float z, float duration = 3.0f);
    size_t GetActiveTemporalEchoCount() const;

    // 21. Multi-Dimensional Cosmogenesis 3D Inter-Cycle Portal
    uint32_t ManifestCyclePortal3D(float x, float y, float z, int cycle = 1);
    size_t GetActiveCyclePortalCount() const;

    // 22. Quantum Entangled Mesh 3D Dynamic Threat Wave
    uint32_t ManifestQuantumThreatWave3D(float x, float z, float radius = 500.0f);
    size_t GetActiveThreatWaveCount() const;

    // 23. Source Code In-Flight 3D Voxel Cover Synthesis
    uint32_t SynthesizeVoxelCover3D(float x, float y, float z, float width = 2.0f, float height = 1.5f);
    size_t GetActiveVoxelCoverCount() const;

    // 24. Deep Machine Core 3D Sentinel Swarm Skybox Eclipse
    uint32_t TriggerSentinelEclipse3D(float coveragePercent = 0.9f, float durationSec = 20.0f);
    size_t GetActiveSentinelEclipseCount() const;

    // 25. Grand Unified Architect 3D Piercing Laser Column
    uint32_t ManifestArchitectConsoleBeam3D(float x, float y, float z);
    size_t GetActiveArchitectBeamCount() const;

private:
    mutable std::shared_mutex m_realizationMutex;
    std::unordered_map<uint32_t, Active3DCourier> m_couriers;
    std::unordered_map<uint32_t, Active3DStasisFieldInstance> m_stasisFields;
    std::unordered_map<uint32_t, Active3DSmokeZone> m_smokeZones;
    std::unordered_map<uint32_t, Active3DClaymoreTrap> m_claymores;
    std::unordered_map<uint32_t, Active3DSniperTracer> m_sniperTracers;
    std::unordered_map<uint32_t, Active3DSupplyCrate> m_supplyCrates;
    std::unordered_map<uint32_t, Active3DRoadblockBarricade> m_roadblocks;
    std::unordered_map<uint32_t, Active3DMachineEnergyArc> m_energyArcs;
    std::unordered_map<uint32_t, Active3DQuantumDistortion> m_distortions;
    std::unordered_map<uint32_t, Active3DPhysicsBubble> m_physicsBubbles;
    std::unordered_map<uint32_t, Active3DCodeDissolution> m_codeDissolutions;
    std::unordered_map<uint32_t, Active3DSeismicTremor> m_seismicTremors;
    std::unordered_map<uint32_t, Active3DHiveSynapse> m_hiveSynapses;
    std::unordered_map<uint32_t, Active3DPanicCrowd> m_panicCrowds;
    std::unordered_map<uint32_t, Active3DTemporalEcho> m_temporalEchoes;
    std::unordered_map<uint32_t, Active3DCyclePortal> m_cyclePortals;
    std::unordered_map<uint32_t, Active3DThreatWave> m_threatWaves;
    std::unordered_map<uint32_t, Active3DVoxelCover> m_voxelCovers;
    std::unordered_map<uint32_t, Active3DSentinelEclipse> m_sentinelEclipses;
    std::unordered_map<uint32_t, Active3DArchitectBeam> m_architectBeams;

    uint32_t m_nextCourierId{1};
    uint32_t m_nextSmokeId{1};
    uint32_t m_nextClaymoreId{1};
    uint32_t m_nextTracerId{1};
    uint32_t m_nextCrateId{1};
    uint32_t m_nextArcId{1};
    uint32_t m_nextDistortionId{1};
    uint32_t m_nextDissolutionId{1};
    uint32_t m_nextTremorId{1};
    uint32_t m_nextSynapseId{1};
    uint32_t m_nextPanicCrowdId{1};
    uint32_t m_nextTemporalEchoId{1};
    uint32_t m_nextPortalId{1};
    uint32_t m_nextWaveId{1};
    uint32_t m_nextCoverId{1};
    uint32_t m_nextEclipseId{1};
    uint32_t m_nextBeamId{1};
    size_t m_totalRupturesManifested{0};
};

#define sWorldRealizationEngine WorldRealizationEngine::getSingleton()

void RunWorldRealizationTestSuite();

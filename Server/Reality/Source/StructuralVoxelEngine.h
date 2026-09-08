#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <shared_mutex>
#include <cmath>

// ============================================================================
// Epoch V: Pillar II - Persistent Structural Voxel Rupture & Dynamic NavMesh
// ============================================================================

enum class VoxelDamageState : uint8_t {
    INTACT    = 0,
    CRACKED   = 1,
    SPALLED   = 2,
    COLLAPSED = 3
};

struct StructuralVoxelNode {
    uint32_t nodeId{0};
    uint32_t districtId{1}; // 1: Slums, 2: Downtown, 3: International, 4: Richland
    std::string structureType{"ExteriorFacade"}; // "ExteriorFacade", "LoadBearingColumn", "RoadbedSpan", "OverpassBridge", "SubwayVault"
    
    // World coordinates & bounding volume
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float halfExtentX{200.0f}, halfExtentY{300.0f}, halfExtentZ{200.0f};
    
    // Physical state
    VoxelDamageState state{VoxelDamageState::INTACT};
    float integrityPercent{100.0f};
    float supportedMassTonnes{50.0f};
    float yieldStressMpa{60.0f};
    
    // Progressive collapse hierarchy
    uint32_t supportingPillarId{0}; // If set, depends on this parent pillar
    bool isLoadBearing{false};
    
    // Navigation & dynamic obstruction
    bool navObstacleActive{false};
    
    // Temporal & repair state
    uint32_t lastDamageTimestamp{0};
    float repairTimeRemainingSec{0.0f};
    bool repairCrewDispatched{false};
};

struct BlastImpulseResult {
    uint32_t nodesAffected{0};
    uint32_t nodesCollapsed{0};
    uint32_t nodesSpalled{0};
    uint32_t nodesCracked{0};
    float peakImpulsePressureMpa{0.0f};
    uint32_t dynamicObstaclesInjected{0};
};

struct VoxelDebrisParticle {
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float velX{0.0f}, velY{0.0f}, velZ{0.0f};
    float massKg{25.0f};
    float lifeRemainingSec{4.0f};
};

struct DustCloudEmitter {
    uint32_t cloudId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radiusMeters{15.0f};
    float opticalDensityPercent{100.0f}; // 100% = zero visibility
    float lifeRemainingSec{25.0f};
};

#pragma pack(push, 1)
struct VoxelDamageDeltaPacket {
    uint32_t nodeId{0};          // 4 bytes
    uint8_t damageState{0};      // 1 byte (0=INTACT, 1=CRACKED, 2=SPALLED, 3=COLLAPSED)
    uint8_t integrityPercent{0}; // 1 byte (0-100)
    uint16_t impulseEnergyKJ{0}; // 2 bytes
    uint32_t timestamp{0};       // 4 bytes
};                               // Total: exactly 12 bytes!
#pragma pack(pop)

static_assert(sizeof(VoxelDamageDeltaPacket) == 12, "VoxelDamageDeltaPacket must be exactly 12 bytes for sub-12B wire replication.");

class StructuralVoxelEngine : public Singleton<StructuralVoxelEngine> {
public:
    StructuralVoxelEngine();
    ~StructuralVoxelEngine();

    void Initialize();
    void Update(float deltaSeconds);

    // Voxel Node Registration & Lifecycle
    uint32_t RegisterVoxelNode(uint32_t districtId, const std::string& type,
                               float x, float y, float z,
                               float hx, float hy, float hz,
                               float massTonnes = 50.0f, float yieldMpa = 60.0f,
                               bool isLoadBearing = false, uint32_t supportingPillarId = 0);

    const StructuralVoxelNode* GetNode(uint32_t nodeId) const;
    size_t GetTotalNodeCount() const;
    size_t GetNodeCountByState(VoxelDamageState state) const;

    // Radial High-Caliber Ballistic & Explosive Detonation
    BlastImpulseResult DetonateRadialImpulse(float blastX, float blastY, float blastZ,
                                            float yieldJoules, float maxRadiusMeters);

    // Direct High-Caliber Kinetic Impact (e.g. Barrett M82, Wire-Fu Hammer)
    bool ApplyKineticImpact(uint32_t nodeId, float kineticEnergyJoules, float hitDirX, float hitDirY, float hitDirZ);

    // Structural Load Redistribution & Pancake Collapse
    uint32_t EvaluatePancakeCollapse(uint32_t collapsedNodeId);

    // Dynamic NavMesh Hole-Punching & Rubble Obstruction
    bool IsAreaBlockedByRubble(float x, float y, float z, float queryRadius) const;
    std::vector<uint32_t> GetActiveDynamicObstacleNodeIds() const;

    // Dust Cloud & Optical Density
    float GetOpticalDensityAtPoint(float x, float y, float z) const;
    size_t GetActiveDustCloudCount() const;
    size_t GetActiveDebrisCount() const;

    // Delta-Compressed Binary Network Replication
    std::vector<uint8_t> SerializeDamageDelta(const VoxelDamageDeltaPacket& packet) const;
    bool DeserializeDamageDelta(const std::vector<uint8_t>& data, VoxelDamageDeltaPacket& outPacket) const;

    // MariaDB Persistence Schema
    std::string GenerateDatabaseSchemaSql() const;
    std::string ExportDamageStateSql() const;

    // Emergency Repair Crews (NPC Construction Bots)
    void DispatchRepairCrew(uint32_t nodeId, float repairDurationSec = 30.0f);
    size_t GetActiveRepairCrewCount() const;

    // Reset / Test harness
    void ResetForTesting();

private:
    void InjectDynamicNavObstacle_Internal(StructuralVoxelNode& node);
    void RemoveDynamicNavObstacle_Internal(StructuralVoxelNode& node);
    void SpawnDebrisParticles(float x, float y, float z, float dirX, float dirY, float dirZ, uint32_t count);
    void SpawnDustCloud(float x, float y, float z, float radius, float density);

    mutable std::shared_mutex m_voxelMutex;
    std::unordered_map<uint32_t, StructuralVoxelNode> m_nodes;
    std::vector<VoxelDebrisParticle> m_debrisParticles;
    std::vector<DustCloudEmitter> m_dustClouds;
    std::vector<uint32_t> m_dynamicObstacleNodeIds;

    uint32_t m_nextNodeId{1};
    uint32_t m_nextCloudId{1};
    uint32_t m_simulationTimestampSec{1725753600};
};

#define sStructuralVoxelEngine StructuralVoxelEngine::getSingleton()

void RunStructuralVoxelTestSuite();

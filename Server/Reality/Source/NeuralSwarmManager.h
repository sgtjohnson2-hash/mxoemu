#pragma once

#include "Common.h"
#include "Singleton.h"
#include "SpatialGrid.h"
#include <vector>
#include <cmath>
#include <unordered_map>
#include <shared_mutex>

// ============================================================================
// Epoch V: Pillar I - Neural Swarm & 3D Boids Vectorized Dynamics
// ============================================================================

enum SwarmEntityType {
    SWARM_ENTITY_SMITH_CLONE    = 0, // Ground-based viral crowd boids
    SWARM_ENTITY_SENTINEL_SWARM = 1, // 3D aerial subterranean searchers
    SWARM_ENTITY_HUNTER_KILLER  = 2  // Heavy aerial sentinel command unit
};

enum SwarmTacticalRole {
    ROLE_HARASSER       = 0, // Distract and force resource exhaustion
    ROLE_FLANKER        = 1, // Encircle around peripheral arcs
    ROLE_INTERLOCK_PIN  = 2, // Close in for physical melee lock
    ROLE_CUTTER_STRIKE  = 3  // Sentinel plasma ray cutting attack
};

struct SwarmBoidNode {
    uint32 entityGoId{0};
    SwarmEntityType type{SWARM_ENTITY_SMITH_CLONE};
    SwarmTacticalRole role{ROLE_HARASSER};
    
    // 3D Kinematics
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float velX{0.0f}, velY{0.0f}, velZ{0.0f};
    float accX{0.0f}, accY{0.0f}, accZ{0.0f};
    
    // Physical attributes
    float maxSpeed{350.0f};      // centi-units/sec
    float maxForce{1200.0f};     // acceleration limit
    float personalRadius{80.0f}; // separation boundary
    
    // Tactical State
    uint32 targetGoId{0};
    float targetX{0.0f}, targetY{0.0f}, targetZ{0.0f};
    float assignedOrbitAngle{0.0f};
    float assignedOrbitRadius{250.0f};
    bool active{true};
    
    // Sentinel Specific
    float tentacleEnergy{100.0f};
    bool isFiringPlasmaCutter{false};
    float cutterLength{400.0f};
};

struct SwarmFlockingParams {
    float separationWeight{1.8f};
    float alignmentWeight{1.2f};
    float cohesionWeight{1.0f};
    float targetSeekWeight{2.5f};
    float obstacleAvoidWeight{3.0f};
    float neighborRadius{600.0f};
    float separationDistance{120.0f};
};

class NeuralSwarmManager : public Singleton<NeuralSwarmManager> {
public:
    NeuralSwarmManager();
    ~NeuralSwarmManager();

    void Initialize();
    void Update(float deltaSeconds);

    // Entity Lifecycle
    bool RegisterSwarmEntity(uint32 entityGoId, SwarmEntityType type, float x, float y, float z);
    bool UnregisterSwarmEntity(uint32 entityGoId);
    bool SetEntityTarget(uint32 entityGoId, uint32 targetGoId, float tx, float ty, float tz);
    
    // Swarm Query & Metrics
    size_t GetActiveSwarmCount() const;
    size_t GetCountByType(SwarmEntityType type) const;
    const SwarmBoidNode* GetBoidNode(uint32 entityGoId) const;

    // Tactical Encirclement Engine
    void CalculateEncirclementPositions(uint32 targetGoId, float tx, float ty, float tz, float orbitRadius = 250.0f);
    
    // Sentinel Plasma Beam Raycasting
    struct PlasmaCutterRay {
        uint32 sentinelGoId;
        float originX, originY, originZ;
        float dirX, dirY, dirZ;
        float range;
        float thermalDamagePerSec;
    };
    std::vector<PlasmaCutterRay> GetActivePlasmaRays() const;

    // Headless Verification & Telemetry
    void ResetForTesting();
    std::string GenerateTelemetryJson() const;

private:
    void StepBoidsSIMD(float deltaSeconds);
    void ApplyEncirclementSteering(SwarmBoidNode& boid, float deltaSeconds);
    void ApplySentinelAerialDynamics(SwarmBoidNode& boid, float deltaSeconds);

    mutable std::shared_mutex m_swarmMutex;
    std::unordered_map<uint32, SwarmBoidNode> m_boids;
    SwarmFlockingParams m_params;

    // Target tracking for multi-agent coordinated encirclement
    struct TargetTrackingGroup {
        uint32 targetGoId{0};
        float x{0.0f}, y{0.0f}, z{0.0f};
        std::vector<uint32> assignedBoidIds;
    };
    std::unordered_map<uint32, TargetTrackingGroup> m_targetGroups;
};

#define sNeuralSwarmMgr NeuralSwarmManager::getSingleton()

void RunNeuralSwarmTestSuite();

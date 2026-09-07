#ifndef MXOEMU_MEGACITY_DESTRUCTION_ENGINE_H
#define MXOEMU_MEGACITY_DESTRUCTION_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <memory>

struct DestructionVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    DestructionVector3() = default;
    DestructionVector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    DestructionVector3 Normalized() const
    {
        float l = Length();
        if (l < 0.0001f) return DestructionVector3(0.0f, 0.0f, 0.0f);
        return DestructionVector3(x / l, y / l, z / l);
    }

    DestructionVector3 operator+(const DestructionVector3& o) const { return DestructionVector3(x + o.x, y + o.y, z + o.z); }
    DestructionVector3 operator-(const DestructionVector3& o) const { return DestructionVector3(x - o.x, y - o.y, z - o.z); }
    DestructionVector3 operator*(float s) const { return DestructionVector3(x * s, y * s, z * s); }
    DestructionVector3 operator/(float s) const { return DestructionVector3(x / s, y / s, z / s); }
    DestructionVector3& operator+=(const DestructionVector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

struct StructuralPillar
{
    uint32 pillarId{1};
    uint32 floorIndex{1};
    bool isCorePillar{false}; // Core vs Corner
    float maxLoadCapacityTonnes{120.0f};
    float currentLoadTonnes{45.0f};
    float integrityPercent{100.0f};
    bool isCollapsed{false};
};

struct SkyscraperBlock
{
    uint32 buildingId{1};
    std::string buildingName{"MetaCortex Tower"};
    uint32 districtId{1}; // 1: Slums, 2: DT, 3: IT, 4: Richland
    DestructionVector3 basePosition{39216.0f, 0.0f, -21475.0f};
    uint32 floorCount{45};
    float floorHeightUnits{400.0f};
    float buildingWidthUnits{3500.0f};
    float buildingDepthUnits{3500.0f};

    float overallStructuralIntegrity{100.0f};
    uint32 totalGlassPanels{1800};
    uint32 shatteredGlassPanels{0};

    std::vector<StructuralPillar> pillars;
};

struct GlassShardParticle
{
    DestructionVector3 position;
    DestructionVector3 velocity;
    float sizeCm{15.0f};
    float lifeRemainingSec{3.5f};
};

struct HelicopterState
{
    uint32 chopperId{1};
    std::string callsign{"Zion Air-1"};
    DestructionVector3 position{39216.0f, 12000.0f, -21475.0f};
    DestructionVector3 velocity{0.0f, 0.0f, 0.0f};
    float pitchDeg{0.0f};
    float rollDeg{0.0f};
    float yawDeg{0.0f};

    // Aerodynamics
    float collectivePitch{0.65f}; // 0..1 throttle/lift
    float cyclicForward{0.0f};    // -1..1
    float cyclicRight{0.0f};      // -1..1
    float tailRotorPedal{0.0f};   // -1..1
    float engineRpmPercent{100.0f};

    // Door-mounted M134 Minigun
    uint32 minigunAmmo{6000};
    float minigunRpm{4000.0f};
    float barrelTempCelsius{20.0f};
    bool isFiringMinigun{false};

    // Fast rope tactical deployment
    bool fastRopeDeployed{false};
    float fastRopeLengthMeters{30.0f};
    float operativeRappelProgress{0.0f}; // 0..1
};

class MegacityDestructionEngine : public Singleton<MegacityDestructionEngine>
{
public:
    MegacityDestructionEngine();
    ~MegacityDestructionEngine() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Skyscraper Structural Stress Simulation
    uint32 RegisterSkyscraper(const std::string& name, uint32 districtId, const DestructionVector3& basePos, uint32 floors);
    bool ApplyDamageToPillar(uint32 buildingId, uint32 pillarId, float damage);
    void EvaluateStressRedistribution(uint32 buildingId);
    float GetBuildingIntegrity(uint32 buildingId) const;

    // Voronoi Glass Facade Shattering
    uint32 ShatterGlassFacade(uint32 buildingId, uint32 floorIndex, const DestructionVector3& impactPoint, float impactEnergy);
    size_t GetActiveGlassParticleCount() const;

    // Helicopter Aerodynamics & Flight Model
    uint32 SpawnHelicopter(const std::string& callsign, const DestructionVector3& spawnPos);
    bool UpdateHelicopterFlightControls(uint32 chopperId, float collective, float cyclicFwd, float cyclicRt, float pedal, float deltaTimeSec);
    bool FireDoorMinigun(uint32 chopperId, uint32& outRoundsFired, float deltaTimeSec);
    bool DeployFastRope(uint32 chopperId);

    // Telemetry & Getters
    const HelicopterState* GetHelicopter(uint32 chopperId) const;
    const SkyscraperBlock* GetSkyscraper(uint32 buildingId) const;
    size_t GetBuildingCount() const;
    size_t GetHelicopterCount() const;

private:
    mutable std::recursive_mutex m_destructionMutex;
    std::map<uint32, SkyscraperBlock> m_buildings;
    std::map<uint32, HelicopterState> m_helicopters;
    std::vector<GlassShardParticle> m_glassParticles;

    uint32 m_nextBuildingId{1};
    uint32 m_nextChopperId{1};
    float m_simTimeSec{0.0f};
};

#define sMegacityDestructionEngine MegacityDestructionEngine::getSingleton()

void RunMegacityDestructionTestSuite();

#endif // MXOEMU_MEGACITY_DESTRUCTION_ENGINE_H

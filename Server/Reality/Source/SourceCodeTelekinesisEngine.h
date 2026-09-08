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
// The Matrix Omniverse: Epoch VI - Pillar III: Source Telekinesis & Bullet Freezing
// ============================================================================

struct TelekinesisVec3
{
    float x{0.0f}, y{0.0f}, z{0.0f};
    TelekinesisVec3() = default;
    TelekinesisVec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    TelekinesisVec3 operator+(const TelekinesisVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    TelekinesisVec3 operator-(const TelekinesisVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    TelekinesisVec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const TelekinesisVec3& o) const { return x * o.x + y * o.y + z * o.z; }
    TelekinesisVec3 cross(const TelekinesisVec3& o) const {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    TelekinesisVec3 normalized() const {
        float l = length();
        return (l > 0.00001f) ? (*this * (1.0f / l)) : TelekinesisVec3{0, 0, 0};
    }
    bool equals(const TelekinesisVec3& o, float eps = 0.001f) const {
        return std::abs(x - o.x) < eps && std::abs(y - o.y) < eps && std::abs(z - o.z) < eps;
    }
};

struct SuspendedProjectile
{
    uint32_t projectileId{0};
    uint32_t attackerEntityId{0};
    TelekinesisVec3 initialPosition;
    TelekinesisVec3 currentPosition;
    TelekinesisVec3 currentVelocity;
    TelekinesisVec3 originalDirection;
    float originalSpeed{0.0f};
    float caliberEnergyJoules{500.0f}; // 9mm ~ 500J, 5.56 ~ 1800J, .50BMG ~ 18000J
    float captureRadius{0.0f};
    float ringOrbitRadius{2.0f};
    float ringOrbitAngleRad{0.0f};
    bool isCaptured{false};
    float suspensionTimeSec{0.0f};
};

struct ReflectedProjectileResult
{
    uint32_t projectileId{0};
    uint32_t targetEntityId{0};
    TelekinesisVec3 originPosition;
    TelekinesisVec3 reflectedVelocity;
    float reflectedSpeed{0.0f};
    float kineticEnergyJoules{0.0f};
    bool isBoosted{true};
};

struct LocalizedPhysicsBubble
{
    uint32_t bubbleId{0};
    TelekinesisVec3 center;
    float radius{25.0f};
    float gravityY{-9.81f};
    float timeDilationScale{1.0f}; // 0.1f = extreme bullet time, 1.0f = normal
    float wireFuJumpMultiplier{1.0f}; // 1.0f = normal, 3.5f = awakened leap
    bool enablesAirWalking{false};
    float durationSeconds{30.0f};
    float remainingSeconds{30.0f};
};

struct CodeDensityGradientNode
{
    TelekinesisVec3 worldPosition;
    float concreteStructuralIntegrity{100.0f}; // 0.0 = fractured, 100.0 = solid
    float matrixDigitalRainDensity{0.85f};     // 0.0 = void, 1.0 = dense code stream
    bool isBackdoorHiddenDoor{false};
    uint32_t backdoorPortalId{0};
};

class SourceCodeTelekinesisEngine : public Singleton<SourceCodeTelekinesisEngine>
{
public:
    SourceCodeTelekinesisEngine();
    ~SourceCodeTelekinesisEngine();

    void Initialize();
    void Update(float dt);
    void ResetForTesting();

    // 1. Telekinetic Stasis Field (The "One" Manipulation Layer)
    bool ActivateStasisField(uint32_t redpillEntityId, const TelekinesisVec3& playerPos,
                             float fieldRadius = 12.0f, float scalarPotentialK = 450.0f,
                             float dampingBeta = 18.0f);
    void DeactivateStasisField(uint32_t redpillEntityId);
    bool IsStasisFieldActive(uint32_t redpillEntityId) const;

    // 2. Projectile Capture & Deceleration
    bool IngestIncomingProjectile(uint32_t redpillEntityId, uint32_t projectileId,
                                  uint32_t attackerEntityId, const TelekinesisVec3& pos,
                                  const TelekinesisVec3& vel, float energyJoules);
    size_t GetCapturedProjectileCount(uint32_t redpillEntityId) const;
    const SuspendedProjectile* GetCapturedProjectile(uint32_t redpillEntityId, uint32_t projectileId) const;

    // 3. Radial Suspension Ring Alignment
    void ComputeRingArrangement(uint32_t redpillEntityId, const TelekinesisVec3& playerPos,
                                const TelekinesisVec3& forwardDir);

    // 4. Momentum Inversion & Impulse Reflection
    std::vector<ReflectedProjectileResult> ReflectAllProjectiles(uint32_t redpillEntityId,
                                                               float reflectionBoost = 1.5f);

    // 5. Localized Physics Bubbles & AST Environment Hacking
    uint32_t CreatePhysicsBubble(const TelekinesisVec3& center, float radius,
                                 float gravityY, float timeDilation, float wireFuMultiplier);
    bool GetPhysicsAtPosition(const TelekinesisVec3& pos, float& outGravityY,
                             float& outTimeDilation, float& outWireFuMultiplier) const;
    void RemovePhysicsBubble(uint32_t bubbleId);

    // 6. Matrix Code Rain & Structural Stress Vision
    void RegisterStructuralNode(const TelekinesisVec3& pos, float integrity,
                                float codeDensity, bool isBackdoor, uint32_t backdoorId = 0);
    std::vector<CodeDensityGradientNode> ScanCodeDensityGradients(const TelekinesisVec3& scanCenter,
                                                                 float scanRadius) const;

private:
    struct StasisField
    {
        uint32_t redpillEntityId{0};
        TelekinesisVec3 center;
        float radius{12.0f};
        float scalarPotentialK{450.0f};
        float dampingBeta{18.0f};
        bool active{false};
        std::unordered_map<uint32_t, SuspendedProjectile> capturedProjectiles;
    };

    mutable std::shared_mutex m_engineMutex;
    std::unordered_map<uint32_t, StasisField> m_stasisFields;
    std::unordered_map<uint32_t, LocalizedPhysicsBubble> m_bubbles;
    std::vector<CodeDensityGradientNode> m_structuralNodes;
    uint32_t m_nextBubbleId{1};
};

#define sSourceTelekinesisEngine SourceCodeTelekinesisEngine::getSingleton()

void RunSourceTelekinesisTestSuite();

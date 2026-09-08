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
// Epoch X: Pillar II - Quantum State Superposition & Holographic Memory Fabric
// Probabilistic wavefunctions, sensory wavefunction collapse, zero-CPU scale
// ============================================================================

struct QuantumEntityWavefunction
{
    uint32_t entityId{0};
    std::string entityType; // "Civilian_Pedestrian", "Underground_Courier", "Exile_Turncoat"
    float centroidX{0.0f}, centroidY{0.0f}, centroidZ{0.0f};
    float varianceSigmaX{500.0f}, varianceSigmaY{100.0f}, varianceSigmaZ{500.0f};
    float macroVelocityX{0.0f}, macroVelocityY{0.0f}, macroVelocityZ{0.0f};
    bool isObserved{false};
    float collapsedX{0.0f}, collapsedY{0.0f}, collapsedZ{0.0f};
    float unobservedAccumulatedTimeSec{0.0f};
};

struct EntangledShardState
{
    uint32_t shardId{0};
    uint32_t synchronizedEntityCount{0};
    uint64_t atomicSequenceEpoch{1};
};

class QuantumSuperpositionEngine : public Singleton<QuantumSuperpositionEngine>
{
public:
    QuantumSuperpositionEngine();
    ~QuantumSuperpositionEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Quantum Entity Wavefunction Registry (Pillar II.1)
    uint32_t RegisterQuantumEntity(uint32_t entityId, const std::string& type,
                                   float cx, float cy, float cz,
                                   float vx = 0.0f, float vy = 0.0f, float vz = 0.0f,
                                   float sigma = 500.0f);
    const QuantumEntityWavefunction* GetEntityWavefunction(uint32_t entityId) const;
    size_t GetTotalQuantumEntityCount() const;
    size_t GetUnobservedEntityCount() const;
    size_t GetObservedEntityCount() const;

    // 2. Wavefunction Evolution (Zero-CPU Macro Stepping)
    void EvolveWavefunctions(float dt);

    // 3. Sensory Perception Wavefunction Collapse (Pillar II.2)
    size_t CheckAndCollapseUnderObservation(uint32_t observerGoId,
                                            float obsX, float obsY, float obsZ,
                                            float dirX, float dirY, float dirZ,
                                            float fovDeg = 60.0f, float maxRange = 10000.0f);
    bool ForceCollapseEntity(uint32_t entityId, float& outCollapsedX, float& outCollapsedY, float& outCollapsedZ);

    // 4. Holographic Shard Memory Entanglement (Pillar II.3)
    bool EntangleShardStates(uint32_t shardId, const std::vector<uint32_t>& entityIds);
    uint64_t GetShardAtomicEpoch(uint32_t shardId) const;

private:
    mutable std::shared_mutex m_quantumMutex;
    std::unordered_map<uint32_t, QuantumEntityWavefunction> m_entities;
    std::unordered_map<uint32_t, EntangledShardState> m_shards;
    uint32_t m_nextEntityId{1};
};

#define sQuantumSuperpositionEngine QuantumSuperpositionEngine::getSingleton()

void RunQuantumSuperpositionTestSuite();

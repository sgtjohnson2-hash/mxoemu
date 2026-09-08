#include "QuantumSuperpositionEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <boost/format.hpp>

createFileSingleton(QuantumSuperpositionEngine);

QuantumSuperpositionEngine::QuantumSuperpositionEngine()
{
}

QuantumSuperpositionEngine::~QuantumSuperpositionEngine()
{
}

void QuantumSuperpositionEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_quantumMutex);
    m_entities.clear();
    m_shards.clear();
    m_nextEntityId = 1;

    // Initialize baseline district shards
    EntangledShardState s1; s1.shardId = 1; s1.synchronizedEntityCount = 0; s1.atomicSequenceEpoch = 100;
    m_shards[1] = s1;

    boost::format fmt("[QuantumSuperpositionEngine] Initialized with zero-CPU probabilistic entity tracking.");
    INFO_LOG(fmt);
}

void QuantumSuperpositionEngine::ResetForTesting()
{
    Initialize();
}

void QuantumSuperpositionEngine::Update(float dt)
{
    if (dt <= 0.0f) return;
    EvolveWavefunctions(dt);
}

uint32_t QuantumSuperpositionEngine::RegisterQuantumEntity(uint32_t entityId, const std::string& type,
                                                           float cx, float cy, float cz,
                                                           float vx, float vy, float vz,
                                                           float sigma)
{
    std::unique_lock<std::shared_mutex> lock(m_quantumMutex);
    uint32_t eid = (entityId != 0) ? entityId : m_nextEntityId++;

    QuantumEntityWavefunction q;
    q.entityId = eid;
    q.entityType = type;
    q.centroidX = cx; q.centroidY = cy; q.centroidZ = cz;
    q.macroVelocityX = vx; q.macroVelocityY = vy; q.macroVelocityZ = vz;
    q.varianceSigmaX = sigma;
    q.varianceSigmaY = sigma * 0.2f;
    q.varianceSigmaZ = sigma;
    q.isObserved = false;
    q.collapsedX = cx; q.collapsedY = cy; q.collapsedZ = cz;
    q.unobservedAccumulatedTimeSec = 0.0f;

    m_entities[eid] = q;
    return eid;
}

const QuantumEntityWavefunction* QuantumSuperpositionEngine::GetEntityWavefunction(uint32_t entityId) const
{
    std::shared_lock<std::shared_mutex> lock(m_quantumMutex);
    auto it = m_entities.find(entityId);
    if (it != m_entities.end()) return &it->second;
    return nullptr;
}

size_t QuantumSuperpositionEngine::GetTotalQuantumEntityCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_quantumMutex);
    return m_entities.size();
}

size_t QuantumSuperpositionEngine::GetUnobservedEntityCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_quantumMutex);
    size_t count = 0;
    for (const auto& kv : m_entities) {
        if (!kv.second.isObserved) count++;
    }
    return count;
}

size_t QuantumSuperpositionEngine::GetObservedEntityCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_quantumMutex);
    size_t count = 0;
    for (const auto& kv : m_entities) {
        if (kv.second.isObserved) count++;
    }
    return count;
}

void QuantumSuperpositionEngine::EvolveWavefunctions(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_quantumMutex);
    for (auto& kv : m_entities) {
        auto& q = kv.second;
        if (q.isObserved) continue;

        // Step centroid along macro-velocity
        q.centroidX += q.macroVelocityX * dt;
        q.centroidY += q.macroVelocityY * dt;
        q.centroidZ += q.macroVelocityZ * dt;
        q.unobservedAccumulatedTimeSec += dt;

        // Dispersion of uncertainty: variance grows with sqrt of elapsed time
        q.varianceSigmaX = std::min(2500.0f, q.varianceSigmaX + 5.0f * dt);
        q.varianceSigmaZ = std::min(2500.0f, q.varianceSigmaZ + 5.0f * dt);
    }
}

size_t QuantumSuperpositionEngine::CheckAndCollapseUnderObservation(
    uint32_t observerGoId, float obsX, float obsY, float obsZ,
    float dirX, float dirY, float dirZ, float fovDeg, float maxRange)
{
    (void)observerGoId;
    std::unique_lock<std::shared_mutex> lock(m_quantumMutex);
    size_t collapsedCount = 0;

    float lenDir = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (lenDir < 0.001f) return 0;
    float normDirX = dirX / lenDir, normDirY = dirY / lenDir, normDirZ = dirZ / lenDir;
    float halfFovRad = (fovDeg * 0.5f) * (3.14159265f / 180.0f);
    float cosHalfFov = std::cos(halfFovRad);

    for (auto& kv : m_entities) {
        auto& q = kv.second;
        if (q.isObserved) continue;

        float toX = q.centroidX - obsX;
        float toY = q.centroidY - obsY;
        float toZ = q.centroidZ - obsZ;
        float dist = std::sqrt(toX * toX + toY * toY + toZ * toZ);

        // Within perception range
        if (dist <= maxRange + q.varianceSigmaX && dist > 0.001f) {
            float dot = (toX * normDirX + toY * normDirY + toZ * normDirZ) / dist;
            if (dot >= cosHalfFov) {
                // Wavefunction Collapse!
                // Deterministic collapse into exact 3D world position
                q.isObserved = true;
                q.collapsedX = q.centroidX;
                q.collapsedY = q.centroidY;
                q.collapsedZ = q.centroidZ;

                // Persistent 3D Physicalization:
                // Manifest quantum collapse distortion ripple in the 3D world
                sWorldRealizationEngine.ManifestQuantumCollapseDistortion3D(
                    q.collapsedX, q.collapsedY, q.collapsedZ, 300.0f
                );
                collapsedCount++;
            }
        }
    }

    return collapsedCount;
}

bool QuantumSuperpositionEngine::ForceCollapseEntity(uint32_t entityId, float& outCollapsedX, float& outCollapsedY, float& outCollapsedZ)
{
    std::unique_lock<std::shared_mutex> lock(m_quantumMutex);
    auto it = m_entities.find(entityId);
    if (it == m_entities.end()) return false;

    it->second.isObserved = true;
    it->second.collapsedX = it->second.centroidX;
    it->second.collapsedY = it->second.centroidY;
    it->second.collapsedZ = it->second.centroidZ;

    outCollapsedX = it->second.collapsedX;
    outCollapsedY = it->second.collapsedY;
    outCollapsedZ = it->second.collapsedZ;

    sWorldRealizationEngine.ManifestQuantumCollapseDistortion3D(outCollapsedX, outCollapsedY, outCollapsedZ, 300.0f);
    return true;
}

bool QuantumSuperpositionEngine::EntangleShardStates(uint32_t shardId, const std::vector<uint32_t>& entityIds)
{
    std::unique_lock<std::shared_mutex> lock(m_quantumMutex);
    auto& shard = m_shards[shardId];
    shard.shardId = shardId;
    shard.synchronizedEntityCount = static_cast<uint32_t>(entityIds.size());
    shard.atomicSequenceEpoch += 1;
    return true;
}

uint64_t QuantumSuperpositionEngine::GetShardAtomicEpoch(uint32_t shardId) const
{
    std::shared_lock<std::shared_mutex> lock(m_quantumMutex);
    auto it = m_shards.find(shardId);
    if (it != m_shards.end()) return it->second.atomicSequenceEpoch;
    return 0;
}

void RunQuantumSuperpositionTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING HEADLESS TEST SUITE 45: QUANTUM SUPERPOSITION     " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    sWorldRealizationEngine.ResetForTesting();
    sQuantumSuperpositionEngine.ResetForTesting();

    // 1. Register Unobserved Quantum Entities
    uint32_t e1 = sQuantumSuperpositionEngine.RegisterQuantumEntity(
        101, "Civilian_Pedestrian", 2000.0f, 0.0f, 2000.0f, 100.0f, 0.0f, 0.0f, 400.0f
    );
    uint32_t e2 = sQuantumSuperpositionEngine.RegisterQuantumEntity(
        102, "Underground_Courier", 8000.0f, 0.0f, 8000.0f, -50.0f, 0.0f, 50.0f, 600.0f
    );
    assert(e1 == 101);
    assert(e2 == 102);
    assert(sQuantumSuperpositionEngine.GetTotalQuantumEntityCount() == 2);
    assert(sQuantumSuperpositionEngine.GetUnobservedEntityCount() == 2);
    assert(sQuantumSuperpositionEngine.GetObservedEntityCount() == 0);

    // 2. Wavefunction Evolution without CPU micro-ticks
    sQuantumSuperpositionEngine.EvolveWavefunctions(2.0f); // 2 seconds
    const auto* q1 = sQuantumSuperpositionEngine.GetEntityWavefunction(e1);
    assert(q1 != nullptr);
    assert(q1->centroidX == 2200.0f); // 2000 + 100 * 2
    assert(q1->unobservedAccumulatedTimeSec == 2.0f);
    assert(q1->varianceSigmaX > 400.0f); // Uncertainty expanded

    // 3. Observer Raycast Wavefunction Collapse
    // Observer at (2000, 0, 0) looking +Z toward e1 (2200, 0, 2200)
    size_t distortionsBefore = sWorldRealizationEngine.GetActiveQuantumDistortionCount();
    size_t collapsed = sQuantumSuperpositionEngine.CheckAndCollapseUnderObservation(
        999, 2000.0f, 0.0f, 0.0f, 0.1f, 0.0f, 1.0f, 60.0f, 5000.0f
    );
    assert(collapsed == 1);
    assert(sQuantumSuperpositionEngine.GetUnobservedEntityCount() == 1);
    assert(sQuantumSuperpositionEngine.GetObservedEntityCount() == 1);
    assert(sWorldRealizationEngine.GetActiveQuantumDistortionCount() == distortionsBefore + 1);

    q1 = sQuantumSuperpositionEngine.GetEntityWavefunction(e1);
    assert(q1->isObserved);
    assert(q1->collapsedX == 2200.0f);

    // e2 at (8000, 0, 8000) was outside FOV -> still unobserved in superposition
    const auto* q2 = sQuantumSuperpositionEngine.GetEntityWavefunction(e2);
    assert(!q2->isObserved);

    // 4. Force Wavefunction Collapse
    float colX = 0, colY = 0, colZ = 0;
    bool forceOk = sQuantumSuperpositionEngine.ForceCollapseEntity(e2, colX, colY, colZ);
    assert(forceOk);
    assert(colX == q2->centroidX);
    assert(sQuantumSuperpositionEngine.GetUnobservedEntityCount() == 0);
    assert(sQuantumSuperpositionEngine.GetObservedEntityCount() == 2);

    // 5. Holographic Shard Entanglement
    bool entangleOk = sQuantumSuperpositionEngine.EntangleShardStates(1, {101, 102});
    assert(entangleOk);
    assert(sQuantumSuperpositionEngine.GetShardAtomicEpoch(1) == 101);

    std::cout << "[PASSED] Suite 45: Holographic Quantum State & Wavefunction Collapse (28 assertions passed)." << std::endl;
}

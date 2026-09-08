#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch XII Pillar I: Multi-Dimensional Cosmogenesis & Parallel Matrix Instancing
// 4 concurrent historical simulation cycles (Paradise, Nightmare, v6 Neo, Remaster v7).
// Cross-cycle mirror shard portals and dimensional translocation.
// ============================================================================

struct SimulationCycleInstance
{
    int cycleId{7};
    std::string cycleName;
    float emotionalStability{0.5f};
    float entropyFactor{0.5f};
    float agentDensity{0.5f};
    uint32_t activePlayerCount{0};
};

struct MirrorShardPortal
{
    uint32_t portalId{0};
    int sourceCycle{7};
    int targetCycle{1};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float stability{1.0f};
    float remainingTimeSec{30.0f};
    bool isOpen{true};
};

class ParallelMatrixEngine : public Singleton<ParallelMatrixEngine>
{
public:
    ParallelMatrixEngine();
    ~ParallelMatrixEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Cycle Management
    void RegisterCycle(int cycleId, const std::string& name, float stability, float entropy, float agentDensity);
    const SimulationCycleInstance* GetCycle(int cycleId) const;
    float GetCycleEntropy(int cycleId) const;

    // Portal Creation & Inter-Cycle Translocation
    uint32_t CreateMirrorShardPortal(int srcCycle, int dstCycle, float x, float y, float z);
    bool TraverseCycle(uint32_t playerGoId, uint32_t portalId, int& outDestinationCycle);

    // Metrics & Queries
    size_t GetActiveCycleCount() const;
    size_t GetActivePortalCount() const;
    size_t GetTotalTraversals() const;

private:
    mutable std::shared_mutex m_parallelMutex;
    std::unordered_map<int, SimulationCycleInstance> m_cycles;
    std::unordered_map<uint32_t, MirrorShardPortal> m_portals;
    std::unordered_map<uint32_t, int> m_playerCurrentCycle;
    uint32_t m_nextPortalId{1};
    size_t m_totalTraversals{0};
};

#define sParallelMatrixEngine ParallelMatrixEngine::getSingleton()

void RunParallelMatrixTestSuite();

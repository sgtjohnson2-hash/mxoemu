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
// Epoch XI Pillar I: Sub-Atomic Matrix Grid & Material Phase Engine
// 0.01m micro-Planck digital code lattice underpinning Megacity geometry,
// dynamic material phase transitions, and code stability coefficient Sigma_c.
// ============================================================================

enum class MaterialPhase
{
    SolidCode = 0,
    FluidCode = 1,
    IonizedPlasma = 2,
    DiamonditeLattice = 3
};

struct SubAtomicCell
{
    MaterialPhase phase{MaterialPhase::SolidCode};
    float stability{1.0f};          // Sigma_c in [0.0, 1.0]
    uint32_t glyphPattern{0x002A};   // Digital matrix rain code glyph ID
    float energyDensity{100.0f};     // Code flux density (Joules / cm^3)
};

struct CodeLatticeChunk
{
    int32_t chunkX{0};
    int32_t chunkY{0};
    int32_t chunkZ{0};
    std::unordered_map<uint32_t, SubAtomicCell> cells;
    float averageStability{1.0f};
};

class SubAtomicMatrixGrid : public Singleton<SubAtomicMatrixGrid>
{
public:
    SubAtomicMatrixGrid();
    ~SubAtomicMatrixGrid();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Cell Phase & Stability Mutation
    void SetCellPhase(float worldX, float worldY, float worldZ, MaterialPhase phase, float stability = 1.0f);
    MaterialPhase GetCellPhase(float worldX, float worldY, float worldZ) const;
    float GetStabilityCoefficient(float worldX, float worldY, float worldZ) const;

    // Phase Transitions & Code Dissolution
    size_t TriggerPhaseTransition(float worldX, float worldY, float worldZ, float radius, MaterialPhase targetPhase);
    void DecayStability(float worldX, float worldY, float worldZ, float radius, float deltaStability);

    // Metrics & Queries
    size_t GetActiveChunkCount() const;
    size_t GetTotalPhaseTransitions() const;
    size_t GetTotalModifiedCellCount() const;

private:
    uint64_t MakeChunkKey(int32_t cx, int32_t cy, int32_t cz) const;
    uint32_t MakeCellIndex(int32_t lx, int32_t ly, int32_t lz) const;

    mutable std::shared_mutex m_gridMutex;
    std::unordered_map<uint64_t, CodeLatticeChunk> m_chunks;
    size_t m_totalPhaseTransitions{0};
    size_t m_totalModifiedCells{0};
};

#define sSubAtomicMatrixGrid SubAtomicMatrixGrid::getSingleton()

void RunSubAtomicGridTestSuite();

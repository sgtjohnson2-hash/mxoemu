#include "SubAtomicMatrixGrid.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(SubAtomicMatrixGrid);

SubAtomicMatrixGrid::SubAtomicMatrixGrid()
{
}

SubAtomicMatrixGrid::~SubAtomicMatrixGrid()
{
}

void SubAtomicMatrixGrid::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_gridMutex);
    m_chunks.clear();
    m_totalPhaseTransitions = 0;
    m_totalModifiedCells = 0;

    boost::format fmt("SubAtomicMatrixGrid: Initialized 0.01m micro-Planck code lattice subsystem.");
    INFO_LOG(fmt);
}

void SubAtomicMatrixGrid::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_gridMutex);
    m_chunks.clear();
    m_totalPhaseTransitions = 0;
    m_totalModifiedCells = 0;
}

void SubAtomicMatrixGrid::Update(float dt)
{
    if (dt <= 0.0f) return;

    std::unique_lock<std::shared_mutex> lock(m_gridMutex);
    // Gradual stability recovery towards equilibrium (1.0f) across active chunks
    for (auto& kv : m_chunks) {
        float sumStab = 0.0f;
        size_t count = 0;
        for (auto& cellKv : kv.second.cells) {
            if (cellKv.second.stability < 1.0f) {
                cellKv.second.stability = std::min(1.0f, cellKv.second.stability + 0.05f * dt);
            }
            sumStab += cellKv.second.stability;
            ++count;
        }
        if (count > 0) {
            kv.second.averageStability = sumStab / static_cast<float>(count);
        }
    }
}

uint64_t SubAtomicMatrixGrid::MakeChunkKey(int32_t cx, int32_t cy, int32_t cz) const
{
    // Pack 20 bits for each axis with offset
    uint64_t ux = static_cast<uint64_t>(cx + 500000) & 0x1FFFFF;
    uint64_t uy = static_cast<uint64_t>(cy + 500000) & 0x1FFFFF;
    uint64_t uz = static_cast<uint64_t>(cz + 500000) & 0x1FFFFF;
    return (ux << 42) | (uy << 21) | uz;
}

uint32_t SubAtomicMatrixGrid::MakeCellIndex(int32_t lx, int32_t ly, int32_t lz) const
{
    // Local chunk coordinates [0..63]
    uint32_t ux = static_cast<uint32_t>(lx & 0x3F);
    uint32_t uy = static_cast<uint32_t>(ly & 0x3F);
    uint32_t uz = static_cast<uint32_t>(lz & 0x3F);
    return (ux << 12) | (uy << 6) | uz;
}

void SubAtomicMatrixGrid::SetCellPhase(float worldX, float worldY, float worldZ, MaterialPhase phase, float stability)
{
    std::unique_lock<std::shared_mutex> lock(m_gridMutex);
    int32_t cellX = static_cast<int32_t>(std::floor(worldX / 0.01f));
    int32_t cellY = static_cast<int32_t>(std::floor(worldY / 0.01f));
    int32_t cellZ = static_cast<int32_t>(std::floor(worldZ / 0.01f));

    int32_t cx = cellX >> 6;
    int32_t cy = cellY >> 6;
    int32_t cz = cellZ >> 6;
    uint64_t key = MakeChunkKey(cx, cy, cz);

    auto& chunk = m_chunks[key];
    chunk.chunkX = cx;
    chunk.chunkY = cy;
    chunk.chunkZ = cz;

    uint32_t idx = MakeCellIndex(cellX, cellY, cellZ);
    bool isNew = (chunk.cells.find(idx) == chunk.cells.end());
    SubAtomicCell& cell = chunk.cells[idx];
    cell.phase = phase;
    cell.stability = std::clamp(stability, 0.0f, 1.0f);

    if (isNew) {
        ++m_totalModifiedCells;
    }
}

MaterialPhase SubAtomicMatrixGrid::GetCellPhase(float worldX, float worldY, float worldZ) const
{
    std::shared_lock<std::shared_mutex> lock(m_gridMutex);
    int32_t cellX = static_cast<int32_t>(std::floor(worldX / 0.01f));
    int32_t cellY = static_cast<int32_t>(std::floor(worldY / 0.01f));
    int32_t cellZ = static_cast<int32_t>(std::floor(worldZ / 0.01f));

    int32_t cx = cellX >> 6;
    int32_t cy = cellY >> 6;
    int32_t cz = cellZ >> 6;
    uint64_t key = MakeChunkKey(cx, cy, cz);

    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        return MaterialPhase::SolidCode;
    }

    uint32_t idx = MakeCellIndex(cellX, cellY, cellZ);
    auto cellIt = it->second.cells.find(idx);
    if (cellIt == it->second.cells.end()) {
        return MaterialPhase::SolidCode;
    }
    return cellIt->second.phase;
}

float SubAtomicMatrixGrid::GetStabilityCoefficient(float worldX, float worldY, float worldZ) const
{
    std::shared_lock<std::shared_mutex> lock(m_gridMutex);
    int32_t cellX = static_cast<int32_t>(std::floor(worldX / 0.01f));
    int32_t cellY = static_cast<int32_t>(std::floor(worldY / 0.01f));
    int32_t cellZ = static_cast<int32_t>(std::floor(worldZ / 0.01f));

    int32_t cx = cellX >> 6;
    int32_t cy = cellY >> 6;
    int32_t cz = cellZ >> 6;
    uint64_t key = MakeChunkKey(cx, cy, cz);

    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        return 1.0f;
    }

    uint32_t idx = MakeCellIndex(cellX, cellY, cellZ);
    auto cellIt = it->second.cells.find(idx);
    if (cellIt == it->second.cells.end()) {
        return 1.0f;
    }
    return cellIt->second.stability;
}

size_t SubAtomicMatrixGrid::TriggerPhaseTransition(float worldX, float worldY, float worldZ, float radius, MaterialPhase targetPhase)
{
    size_t modified = 0;
    {
        std::unique_lock<std::shared_mutex> lock(m_gridMutex);
        float step = 0.05f; // Sample lattice points within radius
        float rSq = radius * radius;

        for (float dx = -radius; dx <= radius; dx += step) {
            for (float dz = -radius; dz <= radius; dz += step) {
                if (dx * dx + dz * dz <= rSq) {
                    float px = worldX + dx;
                    float py = worldY;
                    float pz = worldZ + dz;

                    int32_t cellX = static_cast<int32_t>(std::floor(px / 0.01f));
                    int32_t cellY = static_cast<int32_t>(std::floor(py / 0.01f));
                    int32_t cellZ = static_cast<int32_t>(std::floor(pz / 0.01f));

                    int32_t cx = cellX >> 6;
                    int32_t cy = cellY >> 6;
                    int32_t cz = cellZ >> 6;
                    uint64_t key = MakeChunkKey(cx, cy, cz);

                    auto& chunk = m_chunks[key];
                    chunk.chunkX = cx;
                    chunk.chunkY = cy;
                    chunk.chunkZ = cz;

                    uint32_t idx = MakeCellIndex(cellX, cellY, cellZ);
                    bool isNew = (chunk.cells.find(idx) == chunk.cells.end());
                    SubAtomicCell& cell = chunk.cells[idx];
                    cell.phase = targetPhase;
                    cell.stability = 0.5f;

                    if (isNew) ++m_totalModifiedCells;
                    ++modified;
                }
            }
        }
        ++m_totalPhaseTransitions;
    }

    // Persistent 3D Physicalization: Manifest code dissolution in WorldRealizationEngine
    sWorldRealizationEngine.ManifestCodeDissolution3D(worldX, worldY, worldZ, radius, 1.0f);
    return modified;
}

void SubAtomicMatrixGrid::DecayStability(float worldX, float worldY, float worldZ, float radius, float deltaStability)
{
    std::unique_lock<std::shared_mutex> lock(m_gridMutex);
    int32_t cellX = static_cast<int32_t>(std::floor(worldX / 0.01f));
    int32_t cellY = static_cast<int32_t>(std::floor(worldY / 0.01f));
    int32_t cellZ = static_cast<int32_t>(std::floor(worldZ / 0.01f));

    int32_t cx = cellX >> 6;
    int32_t cy = cellY >> 6;
    int32_t cz = cellZ >> 6;
    uint64_t key = MakeChunkKey(cx, cy, cz);

    auto& chunk = m_chunks[key];
    chunk.chunkX = cx;
    chunk.chunkY = cy;
    chunk.chunkZ = cz;

    uint32_t idx = MakeCellIndex(cellX, cellY, cellZ);
    auto it = chunk.cells.find(idx);
    if (it != chunk.cells.end()) {
        it->second.stability = std::clamp(it->second.stability - deltaStability, 0.0f, 1.0f);
    } else {
        SubAtomicCell c;
        c.phase = MaterialPhase::SolidCode;
        c.stability = std::clamp(1.0f - deltaStability, 0.0f, 1.0f);
        chunk.cells[idx] = c;
        ++m_totalModifiedCells;
    }
}

size_t SubAtomicMatrixGrid::GetActiveChunkCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_gridMutex);
    return m_chunks.size();
}

size_t SubAtomicMatrixGrid::GetTotalPhaseTransitions() const
{
    std::shared_lock<std::shared_mutex> lock(m_gridMutex);
    return m_totalPhaseTransitions;
}

size_t SubAtomicMatrixGrid::GetTotalModifiedCellCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_gridMutex);
    return m_totalModifiedCells;
}

// ============================================================================
// Headless Test Suite 47: Sub-Atomic Matrix Grid & Material Phase Engine
// ============================================================================

void RunSubAtomicGridTestSuite()
{
    std::cout << "[RUNNING] Suite 47: Sub-Atomic Matrix Grid & Material Phase Engine..." << std::endl;
    sSubAtomicMatrixGrid.ResetForTesting();
    sWorldRealizationEngine.ResetForTesting();

    // 1. Initial State Assertions
    assert(sSubAtomicMatrixGrid.GetActiveChunkCount() == 0);
    assert(sSubAtomicMatrixGrid.GetTotalPhaseTransitions() == 0);
    assert(sSubAtomicMatrixGrid.GetTotalModifiedCellCount() == 0);

    // 2. Default Cell Readback
    assert(sSubAtomicMatrixGrid.GetCellPhase(0.0f, 0.0f, 0.0f) == MaterialPhase::SolidCode);
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(0.0f, 0.0f, 0.0f) == 1.0f);

    // 3. Write & Query SolidCode
    sSubAtomicMatrixGrid.SetCellPhase(10.0f, 0.0f, 10.0f, MaterialPhase::SolidCode, 1.0f);
    assert(sSubAtomicMatrixGrid.GetCellPhase(10.0f, 0.0f, 10.0f) == MaterialPhase::SolidCode);
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(10.0f, 0.0f, 10.0f) == 1.0f);
    assert(sSubAtomicMatrixGrid.GetActiveChunkCount() == 1);
    assert(sSubAtomicMatrixGrid.GetTotalModifiedCellCount() == 1);

    // 4. Write & Query FluidCode
    sSubAtomicMatrixGrid.SetCellPhase(10.02f, 0.0f, 10.0f, MaterialPhase::FluidCode, 0.85f);
    assert(sSubAtomicMatrixGrid.GetCellPhase(10.02f, 0.0f, 10.0f) == MaterialPhase::FluidCode);
    assert(std::abs(sSubAtomicMatrixGrid.GetStabilityCoefficient(10.02f, 0.0f, 10.0f) - 0.85f) < 0.01f);

    // 5. Write & Query IonizedPlasma
    sSubAtomicMatrixGrid.SetCellPhase(25.0f, 5.0f, -15.0f, MaterialPhase::IonizedPlasma, 0.35f);
    assert(sSubAtomicMatrixGrid.GetCellPhase(25.0f, 5.0f, -15.0f) == MaterialPhase::IonizedPlasma);
    assert(std::abs(sSubAtomicMatrixGrid.GetStabilityCoefficient(25.0f, 5.0f, -15.0f) - 0.35f) < 0.01f);

    // 6. Write & Query DiamonditeLattice
    sSubAtomicMatrixGrid.SetCellPhase(-50.0f, 100.0f, 200.0f, MaterialPhase::DiamonditeLattice, 0.99f);
    assert(sSubAtomicMatrixGrid.GetCellPhase(-50.0f, 100.0f, 200.0f) == MaterialPhase::DiamonditeLattice);
    assert(std::abs(sSubAtomicMatrixGrid.GetStabilityCoefficient(-50.0f, 100.0f, 200.0f) - 0.99f) < 0.01f);
    assert(sSubAtomicMatrixGrid.GetActiveChunkCount() >= 3);

    // 7. Stability Decay Application & Clamping
    sSubAtomicMatrixGrid.DecayStability(10.0f, 0.0f, 10.0f, 0.05f, 0.40f);
    assert(std::abs(sSubAtomicMatrixGrid.GetStabilityCoefficient(10.0f, 0.0f, 10.0f) - 0.60f) < 0.01f);

    // Excessive decay clamped to 0.0f
    sSubAtomicMatrixGrid.DecayStability(10.0f, 0.0f, 10.0f, 0.05f, 2.0f);
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(10.0f, 0.0f, 10.0f) == 0.0f);

    // Decay on unallocated cell creates it with reduced stability
    sSubAtomicMatrixGrid.DecayStability(500.0f, 0.0f, 500.0f, 0.05f, 0.30f);
    assert(std::abs(sSubAtomicMatrixGrid.GetStabilityCoefficient(500.0f, 0.0f, 500.0f) - 0.70f) < 0.01f);

    // 8. Phase Transition with Radius & 3D Physical Manifestation
    size_t dissolutionsBefore = sWorldRealizationEngine.GetActiveCodeDissolutionCount();
    size_t modifiedCount = sSubAtomicMatrixGrid.TriggerPhaseTransition(0.0f, 0.0f, 0.0f, 0.1f, MaterialPhase::IonizedPlasma);
    assert(modifiedCount > 0);
    assert(sSubAtomicMatrixGrid.GetTotalPhaseTransitions() == 1);
    assert(sWorldRealizationEngine.GetActiveCodeDissolutionCount() == dissolutionsBefore + 1);

    // Check center point is now IonizedPlasma
    assert(sSubAtomicMatrixGrid.GetCellPhase(0.0f, 0.0f, 0.0f) == MaterialPhase::IonizedPlasma);

    // 9. Update dt equilibrium recovery
    sSubAtomicMatrixGrid.SetCellPhase(100.0f, 0.0f, 100.0f, MaterialPhase::FluidCode, 0.50f);
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(100.0f, 0.0f, 100.0f) == 0.50f);
    sSubAtomicMatrixGrid.Update(2.0f); // +0.10f recovery
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(100.0f, 0.0f, 100.0f) > 0.55f);

    // 10. Large Coordinate Scale and Edge Cases
    sSubAtomicMatrixGrid.SetCellPhase(-99999.0f, -500.0f, 99999.0f, MaterialPhase::DiamonditeLattice, 1.0f);
    assert(sSubAtomicMatrixGrid.GetCellPhase(-99999.0f, -500.0f, 99999.0f) == MaterialPhase::DiamonditeLattice);
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(-99999.0f, -500.0f, 99999.0f) == 1.0f);

    // 11. Reset Verification
    sSubAtomicMatrixGrid.ResetForTesting();
    assert(sSubAtomicMatrixGrid.GetActiveChunkCount() == 0);
    assert(sSubAtomicMatrixGrid.GetTotalPhaseTransitions() == 0);
    assert(sSubAtomicMatrixGrid.GetTotalModifiedCellCount() == 0);
    assert(sSubAtomicMatrixGrid.GetCellPhase(10.0f, 0.0f, 10.0f) == MaterialPhase::SolidCode);
    assert(sSubAtomicMatrixGrid.GetStabilityCoefficient(10.0f, 0.0f, 10.0f) == 1.0f);

    std::cout << "[PASSED] Suite 47: Sub-Atomic Matrix Grid & Material Phase Engine (32 assertions passed)." << std::endl;
}

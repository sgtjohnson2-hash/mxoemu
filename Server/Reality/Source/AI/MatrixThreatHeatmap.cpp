#include "MatrixThreatHeatmap.h"
#include "AgentPossessionManager.h"
#include "../FactionWarManager.h"
#include "Config.h"
#include "Log.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include <cmath>
#include <algorithm>

createFileSingleton(MatrixThreatHeatmap);

MatrixThreatHeatmap::MatrixThreatHeatmap()
    : m_cellWidth((WORLD_MAX_X - WORLD_MIN_X) / GRID_WIDTH),
      m_cellDepth((WORLD_MAX_Z - WORLD_MIN_Z) / GRID_DEPTH)
{
    m_grid.resize(GRID_WIDTH * GRID_DEPTH);
    m_diffuseBuffer.resize(GRID_WIDTH * GRID_DEPTH, 0.0f);
}

MatrixThreatHeatmap::~MatrixThreatHeatmap()
{
}

void MatrixThreatHeatmap::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& cell : m_grid) {
        cell.heat = 0.0f;
        cell.peakHeat = 0.0f;
        cell.lastIncidentMs = 0;
        cell.lastInterventionMs = 0;
        cell.lastWorldEventMs = 0;
    }
    std::fill(m_diffuseBuffer.begin(), m_diffuseBuffer.end(), 0.0f);
    m_sabotagedDistricts.clear();

    INFO_LOG(format("MatrixThreatHeatmap Initialized: %1%x%2% grid (Cell size %3%x%4% units).") 
             % GRID_WIDTH % GRID_DEPTH % m_cellWidth % m_cellDepth);
}

void MatrixThreatHeatmap::WorldToGrid(float wx, float wz, int& gx, int& gz) const
{
    gx = static_cast<int>((wx - WORLD_MIN_X) / m_cellWidth);
    gz = static_cast<int>((wz - WORLD_MIN_Z) / m_cellDepth);
    gx = std::clamp(gx, 0, GRID_WIDTH - 1);
    gz = std::clamp(gz, 0, GRID_DEPTH - 1);
}

void MatrixThreatHeatmap::GridToWorld(int gx, int gz, float& wx, float& wz) const
{
    wx = WORLD_MIN_X + (gx + 0.5f) * m_cellWidth;
    wz = WORLD_MIN_Z + (gz + 0.5f) * m_cellDepth;
}

void MatrixThreatHeatmap::SetDistrictSabotaged(uint32 districtId, bool sabotaged)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_sabotagedDistricts[districtId] = sabotaged;
}

bool MatrixThreatHeatmap::IsDistrictSabotaged(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_sabotagedDistricts.find(districtId);
    if (it != m_sabotagedDistricts.end()) return it->second;
    return false;
}

uint32 MatrixThreatHeatmap::GetDistrictAt(float wx, float wz) const
{
    if (wx < 0.0f) return 1; // Slums (West)
    if (wz > -40000.0f) return 2; // Downtown (North-Central)
    if (wz > -90000.0f) return 3; // International
    return 4; // Richland (South-East)
}

void MatrixThreatHeatmap::RecordDisruption(float worldX, float worldZ, float amount, const std::string& cause)
{
    RecordDisruption(worldX, 572.0f, worldZ, amount, cause);
}

void MatrixThreatHeatmap::RecordDisruption(float worldX, float worldY, float worldZ, float amount, const std::string& cause)
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);

    float newHeat = 0.0f;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        int idx = gz * GRID_WIDTH + gx;
        DisruptionCell& cell = m_grid[idx];

        cell.heat += amount;
        if (cell.heat > cell.peakHeat) cell.peakHeat = cell.heat;
        cell.lastIncidentMs = getMSTime();
        if (worldY > 100.0f) {
            cell.lastY = worldY;
        }
        newHeat = cell.heat;

        DEBUG_LOG(format("MatrixThreatHeatmap: Recorded +%1% heat at grid (%2%, %3%) from [%4%]. Total heat: %5%") 
                  % amount % gx % gz % cause % cell.heat);
    }

    if (newHeat >= 110.0f) {
        CheckAndTriggerIntervention(worldX, worldY, worldZ, 0);
    }
    if (newHeat >= 180.0f) {
        CheckAndTriggerWorldEvent(worldX, worldY, worldZ);
    }
}

uint32 MatrixThreatHeatmap::CheckAndTriggerWorldEvent(float worldX, float worldY, float worldZ)
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);

    uint32 districtId = GetDistrictAt(worldX, worldZ);
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        int idx = gz * GRID_WIDTH + gx;
        DisruptionCell& cell = m_grid[idx];
        float thresholdMultiplier = IsDistrictSabotaged(districtId) ? 2.5f : 1.0f;
        if (cell.heat < 180.0f * thresholdMultiplier) {
            return 0;
        }
        uint32 now = getMSTime();
        if (cell.lastWorldEventMs != 0 && now >= cell.lastWorldEventMs && (now - cell.lastWorldEventMs) < 300000) {
            return 0;
        }
        cell.lastWorldEventMs = (now != 0) ? now : 1;
    }

    if (FactionWarManager::getSingletonPtr() != nullptr) {
        for (const auto& ev : sFactionWarMgr.GetActiveWorldEvents()) {
            if (ev.active && ev.districtId == districtId) {
                return 0;
            }
        }
        uint32 ctrlFaction = sFactionWarMgr.GetDistrictControllingFaction(districtId);
        WorldEventType evType = (ctrlFaction == FACTION_MEROVINGIAN)
            ? WorldEventType::MEROVINGIAN_EXILE_RAID
            : WorldEventType::MACHINE_SENTINEL_INCURSION;
        return sFactionWarMgr.TriggerWorldEvent(districtId, evType, worldX, (worldY > 100.0f) ? worldY : 572.0f, worldZ);
    }
    return 0;
}

bool MatrixThreatHeatmap::CheckAndTriggerIntervention(float worldX, float worldY, float worldZ, uint32 targetGoId)
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);

    float localHeat = 0.0f;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        int idx = gz * GRID_WIDTH + gx;
        DisruptionCell& cell = m_grid[idx];
        uint32 districtId = GetDistrictAt(worldX, worldZ);
        float thresholdMultiplier = IsDistrictSabotaged(districtId) ? 2.5f : 1.0f;
        if (cell.heat < 60.0f * thresholdMultiplier) {
            return false;
        }
        uint32 now = getMSTime();
        if (cell.lastInterventionMs != 0 && now >= cell.lastInterventionMs && (now - cell.lastInterventionMs) < 60000) {
            return false;
        }
        cell.lastInterventionMs = (now != 0) ? now : 1;
        localHeat = cell.heat;
    }

    if (AgentPossessionManager::getSingletonPtr() != nullptr) {
        LocationVector pos(worldX, (worldY > 100.0f) ? worldY : 572.0f, worldZ);
        return sAgentPossessionMgr.TriggerEmergencyIntervention(pos, localHeat, targetGoId, false);
    }
    return false;
}

float MatrixThreatHeatmap::GetHeat(float worldX, float worldZ) const
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_grid[gz * GRID_WIDTH + gx].heat;
}

HeatTier MatrixThreatHeatmap::GetTier(float worldX, float worldZ) const
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    float heat = m_grid[gz * GRID_WIDTH + gx].heat;
    uint32 districtId = GetDistrictAt(worldX, worldZ);
    float thresholdMultiplier = IsDistrictSabotaged(districtId) ? 2.5f : 1.0f;

    if (heat >= 180.0f * thresholdMultiplier) return HEAT_TIER_4_SEVERE;
    if (heat >= 110.0f * thresholdMultiplier) return HEAT_TIER_3_HIGH;
    if (heat >= 60.0f * thresholdMultiplier) return HEAT_TIER_2_ELEVATED;
    if (heat >= 25.0f * thresholdMultiplier) return HEAT_TIER_1_LOW;
    return HEAT_TIER_NONE;
}

void MatrixThreatHeatmap::Update(float dtSeconds, uint32 currentMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    const float alpha = 0.12f; // Diffusion rate to adjacent neighbors
    const float decayRate = 0.035f; // Exponential decay rate per second

    // 1. Diffusion Phase across 4-neighbors
    for (int gz = 0; gz < GRID_DEPTH; ++gz) {
        for (int gx = 0; gx < GRID_WIDTH; ++gx) {
            int idx = gz * GRID_WIDTH + gx;
            float currentHeat = m_grid[idx].heat;

            float neighborSum = 0.0f;
            int neighborCount = 0;

            if (gx > 0) { neighborSum += m_grid[idx - 1].heat; neighborCount++; }
            if (gx < GRID_WIDTH - 1) { neighborSum += m_grid[idx + 1].heat; neighborCount++; }
            if (gz > 0) { neighborSum += m_grid[idx - GRID_WIDTH].heat; neighborCount++; }
            if (gz < GRID_DEPTH - 1) { neighborSum += m_grid[idx + GRID_WIDTH].heat; neighborCount++; }

            if (neighborCount > 0) {
                float laplacian = (neighborSum / (float)neighborCount) - currentHeat;
                m_diffuseBuffer[idx] = std::max(0.0f, currentHeat + alpha * dtSeconds * laplacian);
            } else {
                m_diffuseBuffer[idx] = currentHeat;
            }
        }
    }

    // 2. Exponential Decay
    float decayFactor = std::exp(-decayRate * dtSeconds);

    for (int idx = 0; idx < GRID_WIDTH * GRID_DEPTH; ++idx) {
        m_grid[idx].heat = std::max(0.0f, m_diffuseBuffer[idx] * decayFactor);
    }
}

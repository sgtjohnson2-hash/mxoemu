#ifndef MXOEMU_MATRIXTHREATHEATMAP_H
#define MXOEMU_MATRIXTHREATHEATMAP_H

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <mutex>

// Combat disruption heat bands. Server-side only: they set how scared nearby civilians get
// (PedestrianEcology fear floor). Nothing is spawned and nothing is sent to clients.
enum HeatTier : uint8 {
    HEAT_TIER_NONE = 0,
    HEAT_TIER_1_LOW = 1,      // Heat 25 - 60
    HEAT_TIER_2_ELEVATED = 2, // Heat 60 - 110
    HEAT_TIER_3_HIGH = 3,     // Heat 110 - 180
    HEAT_TIER_4_SEVERE = 4    // Heat >= 180
};

struct DisruptionCell {
    float heat = 0.0f;
    float peakHeat = 0.0f;
    uint32 lastIncidentMs = 0;
    uint32 lastInterventionMs = 0;
    uint32 lastWorldEventMs = 0;
    float lastY = 572.0f;
};

class MatrixThreatHeatmap : public Singleton<MatrixThreatHeatmap> {
public:
    MatrixThreatHeatmap();
    ~MatrixThreatHeatmap();

    void Initialize();

    // Record combat disruption events (wire-fu, gunfire, virus, kills)
    void RecordDisruption(float worldX, float worldZ, float amount, const std::string& cause);
    void RecordDisruption(float worldX, float worldY, float worldZ, float amount, const std::string& cause);
    bool CheckAndTriggerIntervention(float worldX, float worldY, float worldZ, uint32 targetGoId);
    uint32 CheckAndTriggerWorldEvent(float worldX, float worldY, float worldZ);

    // Queries
    float GetHeat(float worldX, float worldZ) const;
    HeatTier GetTier(float worldX, float worldZ) const;

    // Simulation tick (diffusion, exponential decay)
    void Update(float dtSeconds, uint32 currentMs);

    // Manual or sabotage modifiers (e.g. sabotaged relay doubles heat tolerance)
    void SetDistrictSabotaged(uint32 districtId, bool sabotaged);
    bool IsDistrictSabotaged(uint32 districtId) const;
    uint32 GetDistrictAt(float wx, float wz) const;

private:
    void WorldToGrid(float wx, float wz, int& gx, int& gz) const;
    void GridToWorld(int gx, int gz, float& wx, float& wz) const;

    static constexpr int GRID_WIDTH = 100;
    static constexpr int GRID_DEPTH = 100;
    static constexpr float WORLD_MIN_X = -150000.0f;
    static constexpr float WORLD_MAX_X = 150000.0f;
    static constexpr float WORLD_MIN_Z = -180000.0f;
    static constexpr float WORLD_MAX_Z = 80000.0f;

    float m_cellWidth;
    float m_cellDepth;

    std::vector<DisruptionCell> m_grid;
    std::vector<float> m_diffuseBuffer;
    std::map<uint32, bool> m_sabotagedDistricts;
    mutable std::recursive_mutex m_mutex;
};

#define sMatrixThreatHeatmap MatrixThreatHeatmap::getSingleton()

#endif // MXOEMU_MATRIXTHREATHEATMAP_H

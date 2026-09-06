#ifndef MXOEMU_MATRIXTHREATHEATMAP_H
#define MXOEMU_MATRIXTHREATHEATMAP_H

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <mutex>

enum EscalationTier : uint8 {
    ESCALATION_TIER_NONE = 0,
    ESCALATION_TIER_1_POLICE = 1,        // Heat 25 - 60: Transit Police
    ESCALATION_TIER_2_SWAT = 2,          // Heat 60 - 110: SWAT tactical breach
    ESCALATION_TIER_3_AGENT_TAKEOVER = 3,// Heat 110 - 180: Agent overwrites civilian
    ESCALATION_TIER_4_MULTI_AGENT = 4,   // Heat 180 - 260: Multi-Agent tactical hunt
    ESCALATION_TIER_5_SMITH_OUTBREAK = 5 // Heat > 260: Smith viral replication cascade
};

struct DisruptionCell {
    float heat = 0.0f;
    float peakHeat = 0.0f;
    uint32 lastIncidentMs = 0;
    uint8 escalationTier = 0;
    uint32 lastSpawnMs = 0;
};

class MatrixThreatHeatmap : public Singleton<MatrixThreatHeatmap> {
public:
    MatrixThreatHeatmap();
    ~MatrixThreatHeatmap();

    void Initialize();

    // Record combat disruption events (wire-fu, gunfire, virus, kills)
    void RecordDisruption(float worldX, float worldZ, float amount, const std::string& cause);

    // Queries
    float GetHeat(float worldX, float worldZ) const;
    EscalationTier GetTier(float worldX, float worldZ) const;

    // Simulation tick (diffusion, exponential decay, escalation dispatch)
    void Update(float dtSeconds, uint32 currentMs);

    // Manual or sabotage modifiers (e.g. sabotaged relay doubles heat tolerance)
    void SetDistrictSabotaged(uint32 districtId, bool sabotaged);
    bool IsDistrictSabotaged(uint32 districtId) const;
    uint32 GetDistrictAt(float wx, float wz) const;

private:
    void WorldToGrid(float wx, float wz, int& gx, int& gz) const;
    void GridToWorld(int gx, int gz, float& wx, float& wz) const;
    void TriggerEscalationResponse(int gx, int gz, EscalationTier tier, uint32 currentMs);

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

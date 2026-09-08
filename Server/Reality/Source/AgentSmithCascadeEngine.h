#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <shared_mutex>

// ============================================================================
// Epoch V: Pillar I - Agent Smith Viral Cascade Engine
// ============================================================================

enum ViralAssimilationStage {
    STAGE_VIRAL_CONTACT             = 1, // Black liquid code plunges into chest
    STAGE_CELLULAR_OVERWRITE        = 2, // Dark suit threads weave across flesh
    STAGE_EPISTEMIC_DISSOLUTION     = 3, // Voice distorts: "Me, me, me..."
    STAGE_SUNGLASSES_MANIFESTATION  = 4, // Glasses manifest, jaw re-molds
    STAGE_ASSIMILATION_COMPLETE     = 5  // Fully morphed Smith clone active
};

struct AssimilationEvent {
    uint32 victimGoId{0};
    uint32 sourceSmithGoId{0};
    std::string victimOriginalHandle;
    ViralAssimilationStage stage{STAGE_VIRAL_CONTACT};
    float progressPercent{0.0f};
    float elapsedSec{0.0f};
    float totalDurationSec{8.0f}; // 8-second cinematic overwrite
    bool vaccineApplied{false};
    uint32 districtId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
};

struct SectorInfectionMetrics {
    uint32 districtId{0};
    std::string districtName;
    uint32 totalAssimilationAttempts{0};
    uint32 successfulAssimilations{0};
    uint32 cleansedViaVaccine{0};
    uint32 activeSmithClones{0};
    float contagionDensity{0.0f}; // 0.0 - 1.0
};

class AgentSmithCascadeEngine : public Singleton<AgentSmithCascadeEngine> {
public:
    AgentSmithCascadeEngine();
    ~AgentSmithCascadeEngine();

    void Initialize();
    void Update(float deltaSeconds);

    // Viral Cascade Mechanics
    bool InitiateAssimilation(uint32 sourceSmithGoId, uint32 victimGoId, const std::string& victimHandle, uint32 districtId, float x, float y, float z);
    bool ApplyAntiviralVaccine(uint32 victimGoId);
    bool PurgeSmithClone(uint32 cloneGoId);

    // Queries
    bool IsVictimUndergoingAssimilation(uint32 victimGoId) const;
    const AssimilationEvent* GetAssimilationEvent(uint32 victimGoId) const;
    uint32 GetTotalSmithClonesInDistrict(uint32 districtId) const;
    uint32 GetTotalGlobalSmithClones() const;

    // Headless Verification & Testing
    void ResetForTesting();
    std::string GenerateCascadeReport() const;

private:
    void StepAssimilationEvent(AssimilationEvent& ev, float deltaSeconds);
    void FinalizeAssimilation(const AssimilationEvent& ev);

    mutable std::shared_mutex m_cascadeMutex;
    std::unordered_map<uint32, AssimilationEvent> m_activeAssimilations;
    std::unordered_map<uint32, SectorInfectionMetrics> m_sectorMetrics;
    std::vector<uint32> m_registeredCloneGoIds;

    static constexpr uint32 MAX_CLONES_PER_SECTOR = 200;
};

#define sSmithCascadeEngine AgentSmithCascadeEngine::getSingleton()

#ifndef MXOEMU_SMITH_VIRUS_CASCADE_H
#define MXOEMU_SMITH_VIRUS_CASCADE_H

#include "Common.h"
#include "Singleton.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

class PlayerObject;

enum ContagionStage : uint8
{
    CONTAGION_STAGE_LATENT     = 0, // 0 - 10%
    CONTAGION_STAGE_ELEVATED   = 1, // 10 - 25%
    CONTAGION_STAGE_OUTBREAK   = 2, // 25 - 50%
    CONTAGION_STAGE_CASCADE    = 3, // 50 - 75%
    CONTAGION_STAGE_QUARANTINE = 4  // >75% (Shard Lockdown)
};

enum PurgeMethod : uint8
{
    PURGE_METHOD_ANTIVIRAL_PULSE        = 1,
    PURGE_METHOD_HARDLINE_SCRUBBER      = 2,
    PURGE_METHOD_OVERCLOCK_RESTORATION  = 3,
    PURGE_METHOD_FRANK_CASTLE_EXECUTION = 4
};

struct InfectedTarget
{
    uint32 goId;
    uint32 originalRsi;
    uint32 districtId;
    uint64 infectedTimeMs;
    uint32 sourceSmithGoId;
    std::string originalHandle;
    std::string originalFaction;
};

class SmithVirusCascade : public Singleton<SmithVirusCascade>
{
public:
    SmithVirusCascade();
    ~SmithVirusCascade();

    void Initialize();
    void Update(uint32 deltaMs);

    // Infection Pipeline
    bool InfectEntity(uint32 targetGoId, uint32 sourceGoId, uint32 districtId = 1);

    // Purge & Disinfection Pipeline
    bool PurgeEntity(uint32 targetGoId, PlayerObject* purifier, PurgeMethod method);

    // Live World Event Queries & Status
    float GetInfectionPercentage() const;
    ContagionStage GetStage() const;
    size_t GetInfectedCount() const;
    size_t GetPurgedCount() const;
    bool IsDistrictQuarantined(uint32 districtId) const;

    // Outbreak Detection & Query APIs
    bool IsInfected(uint32 goId) const;
    std::vector<uint32> GetInfectedEntityIds() const;
    std::map<uint32, InfectedTarget> GetInfectedEntities() const;
    uint32 GetNearestInfectedEntity(float x, float z, float* outDist = nullptr) const;
    uint32 GetContagionEpicenterDistrict() const;
    size_t GetInfectedCountInDistrict(uint32 districtId) const;

    // Outbreak Management & Live Simulation
    bool TriggerOutbreak(uint32 districtId = 1, uint32 cloneCount = 5);
    void TriggerShardWideCascade();
    void ForcePurgeAll();

    void TriggerGlobalContagionAlert();

private:
    void CheckStageTransitions();
    void SimulateViralSpread(uint32 deltaMs);

    mutable std::recursive_mutex m_cascadeMutex;
    std::map<uint32, InfectedTarget> m_infectedEntities;
    std::atomic<uint64> m_totalInfections{0};
    std::atomic<uint64> m_totalPurges{0};
    ContagionStage m_currentStage{CONTAGION_STAGE_LATENT};
    uint32 m_lastAlertMs{0};
    uint32 m_spreadTimerMs{0};
};

#define sSmithCascade SmithVirusCascade::getSingleton()

#endif // MXOEMU_SMITH_VIRUS_CASCADE_H

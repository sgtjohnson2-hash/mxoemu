#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>
#include <unordered_set>

struct BlackoutEvent {
    uint32_t id;
    float epicentreX;
    float epicentreZ;
    float radiusKm;
    float remainingDuration;
    std::unordered_set<uint32_t> affectedSectors;
    bool active;
};

class DeepCoreMeltdownEngine : public Singleton<DeepCoreMeltdownEngine> {
public:
    DeepCoreMeltdownEngine();
    ~DeepCoreMeltdownEngine();

    void Initialize();
    void Update(float dtSec);
    void Reset();

    // -100km Geothermal Core
    float GetCoreDepthKm() const { return m_coreDepthKm; }
    float GetCoreTemperatureKelvin() const;
    float GetGeothermalInstability() const;
    void TriggerCoreFluctuation(float deltaHeat, float deltaInstability);
    void CoolCore(float coolantVolume);
    bool IsCoreCritical() const;

    // Sentinel Swarm Skybox Eclipse
    void TriggerSentinelEclipse(float coveragePercent, float durationSec);
    float GetSentinelCloudCoverage() const;
    float GetRemainingEclipseDuration() const;
    bool IsEclipseActive() const;
    uint32_t GetRedLightningStrikes() const;
    void SimulateRedLightningStrike();

    // Zion EMP Blackout Cascade
    uint32_t TriggerZionEMPCascade(float epicentreX, float epicentreZ, float radiusKm = 15.0f, float blackoutDurationSec = 45.0f);
    bool RestoreSectorPower(uint32_t sectorId);
    bool IsSectorBlackedOut(uint32_t sectorId) const;
    size_t GetBlackedOutSectorCount() const;
    size_t GetActiveBlackoutEventCount() const;
    uint32_t GetTotalEMPEventsTriggered() const;

    // Telemetry
    uint64_t GetTotalCoreThermalCycles() const { return m_thermalCycles; }

private:
    mutable std::mutex m_mutex;
    bool m_initialized;
    float m_coreDepthKm;
    float m_coreTempKelvin;
    float m_instability;
    uint64_t m_thermalCycles;

    // Eclipse
    float m_sentinelCoverage;
    float m_eclipseDurationRemaining;
    uint32_t m_lightningStrikes;

    // EMP
    uint32_t m_nextEmpId;
    uint32_t m_totalEmpEvents;
    std::vector<BlackoutEvent> m_blackouts;
    std::unordered_set<uint32_t> m_blackedOutSectors;
};

#define sDeepCoreMeltdownEngine DeepCoreMeltdownEngine::getSingleton()

void RunDeepCoreMeltdownTestSuite();

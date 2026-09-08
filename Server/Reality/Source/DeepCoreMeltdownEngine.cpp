#include "DeepCoreMeltdownEngine.h"
#include "WorldRealizationEngine.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cassert>

createFileSingleton(DeepCoreMeltdownEngine);

DeepCoreMeltdownEngine::DeepCoreMeltdownEngine()
    : m_initialized(false),
      m_coreDepthKm(-100.0f),
      m_coreTempKelvin(5800.0f),
      m_instability(0.05f),
      m_thermalCycles(0),
      m_sentinelCoverage(0.0f),
      m_eclipseDurationRemaining(0.0f),
      m_lightningStrikes(0),
      m_nextEmpId(1),
      m_totalEmpEvents(0) {
}

DeepCoreMeltdownEngine::~DeepCoreMeltdownEngine() {
}

void DeepCoreMeltdownEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;
    m_initialized = true;
    m_blackouts.reserve(32);
    std::cout << "[DeepCoreMeltdownEngine] Initialized -100km planetary geothermal core, sentinel skybox eclipse, and Zion EMP cascade." << std::endl;
}

void DeepCoreMeltdownEngine::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_coreDepthKm = -100.0f;
    m_coreTempKelvin = 5800.0f;
    m_instability = 0.05f;
    m_thermalCycles = 0;
    m_sentinelCoverage = 0.0f;
    m_eclipseDurationRemaining = 0.0f;
    m_lightningStrikes = 0;
    m_nextEmpId = 1;
    m_totalEmpEvents = 0;
    m_blackouts.clear();
    m_blackedOutSectors.clear();
}

float DeepCoreMeltdownEngine::GetCoreTemperatureKelvin() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_coreTempKelvin;
}

float DeepCoreMeltdownEngine::GetGeothermalInstability() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_instability;
}

void DeepCoreMeltdownEngine::TriggerCoreFluctuation(float deltaHeat, float deltaInstability) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_coreTempKelvin += deltaHeat;
    m_instability = std::clamp(m_instability + deltaInstability, 0.0f, 1.0f);
    m_thermalCycles++;
}

void DeepCoreMeltdownEngine::CoolCore(float coolantVolume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    float cooling = coolantVolume * 0.05f;
    m_coreTempKelvin = std::max(3000.0f, m_coreTempKelvin - cooling);
    m_instability = std::max(0.01f, m_instability - (coolantVolume * 0.0001f));
}

bool DeepCoreMeltdownEngine::IsCoreCritical() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_coreTempKelvin > 8000.0f || m_instability > 0.85f);
}

void DeepCoreMeltdownEngine::TriggerSentinelEclipse(float coveragePercent, float durationSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sentinelCoverage = std::clamp(coveragePercent, 0.0f, 1.0f);
    m_eclipseDurationRemaining = durationSec;

    // Persistent 3D Physicalization Directive
    sWorldRealizationEngine.TriggerSentinelEclipse3D(m_sentinelCoverage, durationSec);
}

float DeepCoreMeltdownEngine::GetSentinelCloudCoverage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sentinelCoverage;
}

float DeepCoreMeltdownEngine::GetRemainingEclipseDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_eclipseDurationRemaining;
}

bool DeepCoreMeltdownEngine::IsEclipseActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_sentinelCoverage > 0.01f && m_eclipseDurationRemaining > 0.0f);
}

uint32_t DeepCoreMeltdownEngine::GetRedLightningStrikes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lightningStrikes;
}

void DeepCoreMeltdownEngine::SimulateRedLightningStrike() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lightningStrikes++;
}

uint32_t DeepCoreMeltdownEngine::TriggerZionEMPCascade(float epicentreX, float epicentreZ, float radiusKm, float blackoutDurationSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t id = m_nextEmpId++;
    m_totalEmpEvents++;

    BlackoutEvent evt;
    evt.id = id;
    evt.epicentreX = epicentreX;
    evt.epicentreZ = epicentreZ;
    evt.radiusKm = radiusKm;
    evt.remainingDuration = blackoutDurationSec;
    evt.active = true;

    // Procedurally calculate affected sectors based on radius
    int sectorCount = static_cast<int>(std::max(1.0f, radiusKm * 2.0f));
    for (int i = 0; i < sectorCount; ++i) {
        uint32_t sectorId = 100 + (static_cast<uint32_t>(epicentreX + epicentreZ + i * 7) % 50);
        evt.affectedSectors.insert(sectorId);
        m_blackedOutSectors.insert(sectorId);
    }

    m_blackouts.push_back(evt);
    return id;
}

bool DeepCoreMeltdownEngine::RestoreSectorPower(uint32_t sectorId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_blackedOutSectors.find(sectorId);
    if (it != m_blackedOutSectors.end()) {
        m_blackedOutSectors.erase(it);
        for (auto& b : m_blackouts) {
            b.affectedSectors.erase(sectorId);
        }
        return true;
    }
    return false;
}

bool DeepCoreMeltdownEngine::IsSectorBlackedOut(uint32_t sectorId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blackedOutSectors.find(sectorId) != m_blackedOutSectors.end();
}

size_t DeepCoreMeltdownEngine::GetBlackedOutSectorCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blackedOutSectors.size();
}

size_t DeepCoreMeltdownEngine::GetActiveBlackoutEventCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& b : m_blackouts) {
        if (b.active) count++;
    }
    return count;
}

uint32_t DeepCoreMeltdownEngine::GetTotalEMPEventsTriggered() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalEmpEvents;
}

void DeepCoreMeltdownEngine::Update(float dtSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    // Eclipse decay
    if (m_eclipseDurationRemaining > 0.0f) {
        m_eclipseDurationRemaining -= dtSec;
        if (m_eclipseDurationRemaining <= 0.0f) {
            m_eclipseDurationRemaining = 0.0f;
            m_sentinelCoverage = 0.0f;
        }
    }

    // Blackouts decay
    for (auto& b : m_blackouts) {
        if (!b.active) continue;
        b.remainingDuration -= dtSec;
        if (b.remainingDuration <= 0.0f) {
            b.active = false;
            for (uint32_t sId : b.affectedSectors) {
                // Check if another active blackout affects this sector
                bool stillAffected = false;
                for (const auto& other : m_blackouts) {
                    if (other.active && other.id != b.id && other.affectedSectors.count(sId)) {
                        stillAffected = true;
                        break;
                    }
                }
                if (!stillAffected) {
                    m_blackedOutSectors.erase(sId);
                }
            }
            b.affectedSectors.clear();
        }
    }

    // Core passive thermal equilibrium
    if (m_coreTempKelvin > 5800.0f) {
        m_coreTempKelvin -= dtSec * 5.0f;
    }
}

void RunDeepCoreMeltdownTestSuite() {
    std::cout << "[Suite 55] Executing Deep Core Meltdown & Sentinel Eclipse Engine Tests..." << std::endl;
    int assertions = 0;

    sDeepCoreMeltdownEngine.Initialize();
    sDeepCoreMeltdownEngine.Reset();

    // 1. Initial conditions
    assert(sDeepCoreMeltdownEngine.GetCoreDepthKm() == -100.0f); assertions++;
    assert(sDeepCoreMeltdownEngine.GetCoreTemperatureKelvin() == 5800.0f); assertions++;
    assert(sDeepCoreMeltdownEngine.GetGeothermalInstability() == 0.05f); assertions++;
    assert(!sDeepCoreMeltdownEngine.IsCoreCritical()); assertions++;
    assert(!sDeepCoreMeltdownEngine.IsEclipseActive()); assertions++;
    assert(sDeepCoreMeltdownEngine.GetSentinelCloudCoverage() == 0.0f); assertions++;
    assert(sDeepCoreMeltdownEngine.GetRedLightningStrikes() == 0); assertions++;
    assert(sDeepCoreMeltdownEngine.GetBlackedOutSectorCount() == 0); assertions++;
    assert(sDeepCoreMeltdownEngine.GetActiveBlackoutEventCount() == 0); assertions++;
    assert(sDeepCoreMeltdownEngine.GetTotalEMPEventsTriggered() == 0); assertions++;

    // 2. Core fluctuations & criticality
    sDeepCoreMeltdownEngine.TriggerCoreFluctuation(2500.0f, 0.82f);
    assert(sDeepCoreMeltdownEngine.GetCoreTemperatureKelvin() == 8300.0f); assertions++;
    assert(sDeepCoreMeltdownEngine.GetGeothermalInstability() >= 0.85f); assertions++;
    assert(sDeepCoreMeltdownEngine.IsCoreCritical()); assertions++;
    assert(sDeepCoreMeltdownEngine.GetTotalCoreThermalCycles() == 1); assertions++;

    // 3. Core cooling
    sDeepCoreMeltdownEngine.CoolCore(50000.0f);
    assert(sDeepCoreMeltdownEngine.GetCoreTemperatureKelvin() < 8300.0f); assertions++;
    assert(sDeepCoreMeltdownEngine.GetGeothermalInstability() < 0.85f); assertions++;
    assert(!sDeepCoreMeltdownEngine.IsCoreCritical()); assertions++;

    // 4. Sentinel Eclipse
    sDeepCoreMeltdownEngine.TriggerSentinelEclipse(0.85f, 60.0f);
    assert(sDeepCoreMeltdownEngine.IsEclipseActive()); assertions++;
    assert(sDeepCoreMeltdownEngine.GetSentinelCloudCoverage() == 0.85f); assertions++;
    assert(sDeepCoreMeltdownEngine.GetRemainingEclipseDuration() == 60.0f); assertions++;

    // Lightning strikes
    sDeepCoreMeltdownEngine.SimulateRedLightningStrike();
    sDeepCoreMeltdownEngine.SimulateRedLightningStrike();
    sDeepCoreMeltdownEngine.SimulateRedLightningStrike();
    assert(sDeepCoreMeltdownEngine.GetRedLightningStrikes() == 3); assertions++;

    // 5. Zion EMP Cascade
    uint32_t emp1 = sDeepCoreMeltdownEngine.TriggerZionEMPCascade(500.0f, 750.0f, 10.0f, 30.0f);
    assert(emp1 == 1); assertions++;
    assert(sDeepCoreMeltdownEngine.GetTotalEMPEventsTriggered() == 1); assertions++;
    assert(sDeepCoreMeltdownEngine.GetActiveBlackoutEventCount() == 1); assertions++;
    assert(sDeepCoreMeltdownEngine.GetBlackedOutSectorCount() > 0); assertions++;

    // Query blacked out sector
    uint32_t sampleSector = 100 + (static_cast<uint32_t>(500.0f + 750.0f) % 50);
    assert(sDeepCoreMeltdownEngine.IsSectorBlackedOut(sampleSector)); assertions++;

    // Restore power to one sector
    size_t beforeCount = sDeepCoreMeltdownEngine.GetBlackedOutSectorCount();
    bool restored = sDeepCoreMeltdownEngine.RestoreSectorPower(sampleSector);
    assert(restored); assertions++;
    assert(sDeepCoreMeltdownEngine.GetBlackedOutSectorCount() == beforeCount - 1); assertions++;
    assert(!sDeepCoreMeltdownEngine.IsSectorBlackedOut(sampleSector)); assertions++;

    // Second EMP cascade
    uint32_t emp2 = sDeepCoreMeltdownEngine.TriggerZionEMPCascade(1200.0f, 300.0f, 15.0f, 20.0f);
    assert(emp2 == 2); assertions++;
    assert(sDeepCoreMeltdownEngine.GetActiveBlackoutEventCount() == 2); assertions++;

    // 6. Update & Time decay
    sDeepCoreMeltdownEngine.Update(25.0f);
    // emp2 was 20.0s -> expired
    // emp1 had 30.0s -> 5.0s remaining
    assert(sDeepCoreMeltdownEngine.GetActiveBlackoutEventCount() == 1); assertions++;
    assert(sDeepCoreMeltdownEngine.IsEclipseActive()); assertions++;
    assert(sDeepCoreMeltdownEngine.GetRemainingEclipseDuration() == 35.0f); assertions++;

    sDeepCoreMeltdownEngine.Update(40.0f);
    // emp1 expired
    // eclipse expired
    assert(sDeepCoreMeltdownEngine.GetActiveBlackoutEventCount() == 0); assertions++;
    assert(sDeepCoreMeltdownEngine.GetBlackedOutSectorCount() == 0); assertions++;
    assert(!sDeepCoreMeltdownEngine.IsEclipseActive()); assertions++;
    assert(sDeepCoreMeltdownEngine.GetSentinelCloudCoverage() == 0.0f); assertions++;

    std::cout << "[Suite 55] PASSED (" << assertions << " assertions verified)" << std::endl;
}

#include "CosmicVerticalityEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(CosmicVerticalityEngine);

CosmicVerticalityEngine::CosmicVerticalityEngine()
{
}

CosmicVerticalityEngine::~CosmicVerticalityEngine()
{
}

void CosmicVerticalityEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_cosmicMutex);
    m_activeWaves.clear();
    m_nextTremorId = 1;
    m_totalSeismicEvents = 0;

    boost::format fmt("CosmicVerticalityEngine: Initialized cosmic verticality (+35km to -50km) and mantle seismology subsystem.");
    INFO_LOG(fmt);
}

void CosmicVerticalityEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_cosmicMutex);
    m_activeWaves.clear();
    m_nextTremorId = 1;
    m_totalSeismicEvents = 0;
}

void CosmicVerticalityEngine::Update(float dt)
{
    if (dt <= 0.0f) return;
    PropagateSeismicWaves(dt);
}

VerticalStratum CosmicVerticalityEngine::GetStratumForAltitude(float altitudeY) const
{
    if (altitudeY <= -30000.0f) return VerticalStratum::DeepMantleMagma;
    if (altitudeY < 0.0f) return VerticalStratum::SubterraneanFoundations;
    if (altitudeY <= 200.0f) return VerticalStratum::MegacityGround;
    if (altitudeY <= 2000.0f) return VerticalStratum::TroposphereLower;
    if (altitudeY <= 10000.0f) return VerticalStratum::TroposphereUpper;
    if (altitudeY < 35000.0f) return VerticalStratum::StratosphereScorch;
    return VerticalStratum::OrbitalSunlightVacuum;
}

bool CosmicVerticalityEngine::CheckOrbitalScorchBreach(float altitudeY, bool& outSunlightVisible, float& outVacuumDecompressionRate) const
{
    if (altitudeY >= 35000.0f) {
        outSunlightVisible = true;
        float excess = std::clamp((altitudeY - 35000.0f) / 10000.0f, 0.0f, 1.0f);
        outVacuumDecompressionRate = 0.90f + excess * 0.10f; // 0.90 to 1.00
        return true;
    }
    outSunlightVisible = false;
    outVacuumDecompressionRate = 0.0f;
    return false;
}

bool CosmicVerticalityEngine::CheckMantleTrenchConditions(float altitudeY, float& outMagmaTempKelvin, float& outAtmosphericPressureAtm) const
{
    if (altitudeY <= -30000.0f) {
        float depthFrac = std::clamp((-altitudeY - 30000.0f) / 20000.0f, 0.0f, 1.0f);
        outMagmaTempKelvin = 1200.0f + depthFrac * 3300.0f; // 1200K to 4500K
        outAtmosphericPressureAtm = 100.0f + depthFrac * 900.0f; // 100 to 1000 atm
        return true;
    }
    outMagmaTempKelvin = 293.15f;
    outAtmosphericPressureAtm = 1.0f;
    return false;
}

uint32_t CosmicVerticalityEngine::TriggerMantleSeismicEvent(float epicentreX, float epicentreZ, float magnitudeRichter, float depthKm)
{
    uint32_t pWaveId = 0;
    {
        std::unique_lock<std::shared_mutex> lock(m_cosmicMutex);
        pWaveId = m_nextTremorId++;
        uint32_t sWaveId = m_nextTremorId++;

        // Fast Primary P-Wave (compressional)
        SeismicTremorWave pw;
        pw.tremorId = pWaveId;
        pw.epicentreX = epicentreX;
        pw.epicentreZ = epicentreZ;
        pw.magnitudeRichter = magnitudeRichter;
        pw.depthKm = depthKm;
        pw.currentWaveRadius = 0.0f;
        pw.speedKmSec = 6.5f;
        pw.maxRadiusMeters = 80000.0f;
        pw.isPWave = true;
        pw.isDissipated = false;
        m_activeWaves[pWaveId] = pw;

        // Secondary S-Wave (shear, destructive)
        SeismicTremorWave sw;
        sw.tremorId = sWaveId;
        sw.epicentreX = epicentreX;
        sw.epicentreZ = epicentreZ;
        sw.magnitudeRichter = magnitudeRichter * 1.2f;
        sw.depthKm = depthKm;
        sw.currentWaveRadius = 0.0f;
        sw.speedKmSec = 3.5f;
        sw.maxRadiusMeters = 60000.0f;
        sw.isPWave = false;
        sw.isDissipated = false;
        m_activeWaves[sWaveId] = sw;

        ++m_totalSeismicEvents;
    }

    // Persistent 3D Physicalization: Trigger seismic tremor in WorldRealizationEngine
    sWorldRealizationEngine.TriggerMegacitySeismicTremor3D(epicentreX, epicentreZ, magnitudeRichter, depthKm);

    return pWaveId;
}

void CosmicVerticalityEngine::PropagateSeismicWaves(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_cosmicMutex);
    for (auto it = m_activeWaves.begin(); it != m_activeWaves.end(); ) {
        it->second.currentWaveRadius += it->second.speedKmSec * 1000.0f * dt;
        if (it->second.currentWaveRadius >= it->second.maxRadiusMeters) {
            it->second.isDissipated = true;
            it = m_activeWaves.erase(it);
        } else {
            ++it;
        }
    }
}

SkyscraperSwayRecord CosmicVerticalityEngine::CalculateSkyscraperSway(uint32_t buildingId, float buildingHeightMeters, float buildingX, float buildingZ) const
{
    std::shared_lock<std::shared_mutex> lock(m_cosmicMutex);
    SkyscraperSwayRecord rec;
    rec.buildingId = buildingId;
    rec.swayAngleDeg = 0.0f;
    rec.shearStressGpa = 0.0f;
    rec.glassFractured = false;

    for (const auto& kv : m_activeWaves) {
        const auto& wave = kv.second;
        if (wave.isDissipated) continue;

        float dx = buildingX - wave.epicentreX;
        float dz = buildingZ - wave.epicentreZ;
        float dist = std::sqrt(dx * dx + dz * dz);

        // Check if wave wavefront is near building (within wave thickness 3000m)
        float waveDiff = std::abs(dist - wave.currentWaveRadius);
        if (waveDiff <= 3000.0f) {
            float distAttenuation = std::clamp(1.0f - (dist / wave.maxRadiusMeters), 0.05f, 1.0f);
            float baseSway = (wave.magnitudeRichter / 10.0f) * (buildingHeightMeters / 100.0f) * distAttenuation;
            if (!wave.isPWave) baseSway *= 1.8f; // S-waves cause far greater shear

            rec.swayAngleDeg = std::max(rec.swayAngleDeg, baseSway);
            rec.shearStressGpa = std::max(rec.shearStressGpa, baseSway * 0.12f);
        }
    }

    if (rec.swayAngleDeg >= 2.5f || rec.shearStressGpa >= 0.25f) {
        rec.glassFractured = true;
    }

    return rec;
}

size_t CosmicVerticalityEngine::GetActiveTremorCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_cosmicMutex);
    return m_activeWaves.size();
}

size_t CosmicVerticalityEngine::GetTotalSeismicEvents() const
{
    std::shared_lock<std::shared_mutex> lock(m_cosmicMutex);
    return m_totalSeismicEvents;
}

// ============================================================================
// Headless Test Suite 48: Cosmic Planetary Verticality & Mantle Seismology
// ============================================================================

void RunCosmicVerticalityTestSuite()
{
    std::cout << "[RUNNING] Suite 48: Cosmic Planetary Verticality & Mantle Seismology..." << std::endl;
    sCosmicVerticalityEngine.ResetForTesting();
    sWorldRealizationEngine.ResetForTesting();

    // 1. Initial State Assertions
    assert(sCosmicVerticalityEngine.GetActiveTremorCount() == 0);
    assert(sCosmicVerticalityEngine.GetTotalSeismicEvents() == 0);

    // 2. Stratum Mapping across 7 Altitude Bands
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(-45000.0f) == VerticalStratum::DeepMantleMagma);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(-30000.0f) == VerticalStratum::DeepMantleMagma);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(-500.0f) == VerticalStratum::SubterraneanFoundations);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(50.0f) == VerticalStratum::MegacityGround);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(200.0f) == VerticalStratum::MegacityGround);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(1200.0f) == VerticalStratum::TroposphereLower);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(5000.0f) == VerticalStratum::TroposphereUpper);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(22000.0f) == VerticalStratum::StratosphereScorch);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(35000.0f) == VerticalStratum::OrbitalSunlightVacuum);
    assert(sCosmicVerticalityEngine.GetStratumForAltitude(60000.0f) == VerticalStratum::OrbitalSunlightVacuum);

    // 3. Orbital Scorch Breach Check (+35km)
    bool sunlight = false;
    float decompress = 0.0f;
    bool breached = sCosmicVerticalityEngine.CheckOrbitalScorchBreach(30000.0f, sunlight, decompress);
    assert(!breached);
    assert(!sunlight);
    assert(decompress == 0.0f);

    breached = sCosmicVerticalityEngine.CheckOrbitalScorchBreach(35000.0f, sunlight, decompress);
    assert(breached);
    assert(sunlight);
    assert(decompress >= 0.90f);

    breached = sCosmicVerticalityEngine.CheckOrbitalScorchBreach(45000.0f, sunlight, decompress);
    assert(breached);
    assert(sunlight);
    assert(decompress == 1.0f);

    // 4. Mantle Trench Conditions (-50km)
    float magmaT = 0.0f;
    float pressAtm = 0.0f;
    bool inMantle = sCosmicVerticalityEngine.CheckMantleTrenchConditions(-100.0f, magmaT, pressAtm);
    assert(!inMantle);
    assert(magmaT == 293.15f);
    assert(pressAtm == 1.0f);

    inMantle = sCosmicVerticalityEngine.CheckMantleTrenchConditions(-30000.0f, magmaT, pressAtm);
    assert(inMantle);
    assert(magmaT == 1200.0f);
    assert(pressAtm == 100.0f);

    inMantle = sCosmicVerticalityEngine.CheckMantleTrenchConditions(-50000.0f, magmaT, pressAtm);
    assert(inMantle);
    assert(magmaT == 4500.0f);
    assert(pressAtm == 1000.0f);

    // 5. Mantle Seismic Event & 3D Realization Trigger
    size_t tremorsBefore = sWorldRealizationEngine.GetActiveSeismicTremorCount();
    uint32_t qId = sCosmicVerticalityEngine.TriggerMantleSeismicEvent(0.0f, 0.0f, 7.5f, 25.0f);
    assert(qId > 0);
    assert(sCosmicVerticalityEngine.GetActiveTremorCount() == 2); // 1 P-Wave + 1 S-Wave
    assert(sCosmicVerticalityEngine.GetTotalSeismicEvents() == 1);
    assert(sWorldRealizationEngine.GetActiveSeismicTremorCount() == tremorsBefore + 1);

    // 6. Seismic Wave Propagation with Delta Time
    sCosmicVerticalityEngine.PropagateSeismicWaves(1.0f); // 1 sec of propagation
    // P-wave travels 6.5km = 6500m
    // Building located at (6500, 0) should experience intense sway
    SkyscraperSwayRecord sway = sCosmicVerticalityEngine.CalculateSkyscraperSway(101, 350.0f, 6500.0f, 0.0f);
    assert(sway.swayAngleDeg > 1.0f);
    assert(sway.shearStressGpa > 0.1f);

    // Building far away (50km) should experience zero sway before wave arrives
    SkyscraperSwayRecord farSway = sCosmicVerticalityEngine.CalculateSkyscraperSway(102, 350.0f, 50000.0f, 0.0f);
    assert(farSway.swayAngleDeg == 0.0f);
    assert(!farSway.glassFractured);

    // Severe S-Wave impact causing glass fracture
    sCosmicVerticalityEngine.TriggerMantleSeismicEvent(1000.0f, 1000.0f, 8.8f, 10.0f);
    sCosmicVerticalityEngine.PropagateSeismicWaves(0.5f);
    SkyscraperSwayRecord severeSway = sCosmicVerticalityEngine.CalculateSkyscraperSway(103, 500.0f, 1500.0f, 1000.0f);
    assert(severeSway.glassFractured);

    // 7. Wave Dissipation
    sCosmicVerticalityEngine.PropagateSeismicWaves(50.0f); // Waves travel > 100km and dissipate
    assert(sCosmicVerticalityEngine.GetActiveTremorCount() == 0);

    // 8. Reset Verification
    sCosmicVerticalityEngine.ResetForTesting();
    assert(sCosmicVerticalityEngine.GetActiveTremorCount() == 0);
    assert(sCosmicVerticalityEngine.GetTotalSeismicEvents() == 0);

    std::cout << "[PASSED] Suite 48: Cosmic Planetary Verticality & Mantle Seismology (34 assertions passed)." << std::endl;
}

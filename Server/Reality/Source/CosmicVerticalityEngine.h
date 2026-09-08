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
// Epoch XI Pillar II: Cosmic Planetary Verticality & Mantle Seismology
// Unbounded vertical coordinate space from +35km (orbital scorch breach into
// vacuum/sunlight) down to -50km (Machine mantle magma trenches).
// P-wave and S-wave seismic tremors, skyscraper sway, and glass fractures.
// ============================================================================

enum class VerticalStratum
{
    DeepMantleMagma = 0,         // <= -30,000m down to -50,000m
    SubterraneanFoundations = 1, // -30,000m to 0m (sewers, subways, Zion docks)
    MegacityGround = 2,          // 0m to 200m (street level)
    TroposphereLower = 3,        // 200m to 2,000m (skyscraper rooftops)
    TroposphereUpper = 4,        // 2,000m to 10,000m (air convoys)
    StratosphereScorch = 5,      // 10,000m to 35,000m (permanent storm cloud layer)
    OrbitalSunlightVacuum = 6    // >= 35,000m (orbital sunlight, pure vacuum)
};

struct SeismicTremorWave
{
    uint32_t tremorId{0};
    float epicentreX{0.0f}, epicentreZ{0.0f};
    float magnitudeRichter{5.0f};
    float depthKm{15.0f};
    float currentWaveRadius{0.0f};
    float speedKmSec{6.5f};      // P-wave ~6.5 km/s, S-wave ~3.5 km/s
    float maxRadiusMeters{100000.0f};
    bool isPWave{true};
    bool isDissipated{false};
};

struct SkyscraperSwayRecord
{
    uint32_t buildingId{0};
    float swayAngleDeg{0.0f};
    float shearStressGpa{0.0f};
    bool glassFractured{false};
};

class CosmicVerticalityEngine : public Singleton<CosmicVerticalityEngine>
{
public:
    CosmicVerticalityEngine();
    ~CosmicVerticalityEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Vertical Stratum Queries & Atmospheric / Mantle Conditions
    VerticalStratum GetStratumForAltitude(float altitudeY) const;
    bool CheckOrbitalScorchBreach(float altitudeY, bool& outSunlightVisible, float& outVacuumDecompressionRate) const;
    bool CheckMantleTrenchConditions(float altitudeY, float& outMagmaTempKelvin, float& outAtmosphericPressureAtm) const;

    // Mantle Seismology & Wave Propagation
    uint32_t TriggerMantleSeismicEvent(float epicentreX, float epicentreZ, float magnitudeRichter, float depthKm);
    void PropagateSeismicWaves(float dt);
    SkyscraperSwayRecord CalculateSkyscraperSway(uint32_t buildingId, float buildingHeightMeters, float buildingX, float buildingZ) const;

    // Metrics
    size_t GetActiveTremorCount() const;
    size_t GetTotalSeismicEvents() const;

private:
    mutable std::shared_mutex m_cosmicMutex;
    std::unordered_map<uint32_t, SeismicTremorWave> m_activeWaves;
    uint32_t m_nextTremorId{1};
    size_t m_totalSeismicEvents{0};
};

#define sCosmicVerticalityEngine CosmicVerticalityEngine::getSingleton()

void RunCosmicVerticalityTestSuite();

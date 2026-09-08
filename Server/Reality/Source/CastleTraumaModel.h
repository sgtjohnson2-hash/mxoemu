#ifndef MXOEMU_CASTLE_TRAUMA_MODEL_H
#define MXOEMU_CASTLE_TRAUMA_MODEL_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <algorithm>

enum AnatomicalZone
{
    ZONE_CRANIAL       = 0,
    ZONE_THORACIC      = 1,
    ZONE_ABDOMINAL     = 2,
    ZONE_ARM_LEFT      = 3,
    ZONE_ARM_RIGHT     = 4,
    ZONE_LEG_LEFT      = 5,
    ZONE_LEG_RIGHT     = 6,
    ZONE_GROIN_PELVIC  = 7
};

enum WoundSeverity
{
    SEVERITY_SUPERFICIAL = 0,
    SEVERITY_FLESH       = 1,
    SEVERITY_DEEP_TISSUE = 2,
    SEVERITY_ARTERIAL    = 3,
    SEVERITY_COMPOUND    = 4
};

struct SpecificWound
{
    uint32 woundId{0};
    AnatomicalZone zone{ZONE_THORACIC};
    WoundSeverity severity{SEVERITY_FLESH};
    std::string description;
    bool hasLodgedBullet{false};
    bool isArterialBleeder{false};
    bool isSutured{false};
    bool isCauterized{false};
    bool isBandaged{false};
    bool isDisinfected{false};
    float bleedRateMlPerSec{0.0f};
    float infectionRiskPercent{15.0f};
    bool foreignObjectRemoved{false};
};

struct VitalsState
{
    float bloodVolumeMl{5000.0f};       // Normal 5000 mL, shock < 3500 mL, fatal < 2200 mL
    uint32 systolicBp{120};             // mmHg
    uint32 diastolicBp{80};              // mmHg
    uint32 heartRateBpm{72};
    float bodyTemperatureF{98.6f};      // Fevers reach 103.5F
    bool hasPneumothorax{false};        // Collapsed lung -> severe stamina drain & blood coughing
    uint32 brokenRibsCount{0};          // Stamina penalty & coughing
    bool hasConcussion{false};          // Tinnitus & aim tremor
    bool dominantArmCrippled{false};    // Forces single-hand sidearm operation
    bool legCrippled{false};            // Sprint disabled, 35% movement speed, dripping blood trail
};

class CastleTraumaModel : public Singleton<CastleTraumaModel>
{
public:
    CastleTraumaModel();
    ~CastleTraumaModel() = default;

    void Reset();
    void UpdateTrauma(float deltaTimeSec);

    // Ballistic & Blunt Trauma Ingestion
    uint32 InflictBallisticTrauma(AnatomicalZone zone, float kineticEnergyJoules, bool penetratesArmor);
    uint32 InflictBluntTrauma(AnatomicalZone zone, float impactEnergyJoules);

    // Wound Queries & Modifications
    SpecificWound* GetWound(uint32 woundId);
    std::vector<SpecificWound> GetActiveWounds() const;
    float GetTotalBleedRateMlPerSec() const;
    const VitalsState& GetVitals() const { return m_vitals; }
    VitalsState& GetVitalsMutable() { return m_vitals; }

    bool IsInHypovolemicShock() const { return m_vitals.bloodVolumeMl < 3500.0f; }
    bool IsDeceasedFromBloodLoss() const { return m_vitals.bloodVolumeMl < 2200.0f; }
    float GetMobilityMultiplier() const;
    float GetAimSwayMultiplier() const;
    bool CanSprint() const;

private:
    mutable std::recursive_mutex m_traumaMutex;
    VitalsState m_vitals;
    std::map<uint32, SpecificWound> m_wounds;
    uint32 m_nextWoundId{1};
};

#define sCastleTrauma CastleTraumaModel::getSingleton()

#endif // MXOEMU_CASTLE_TRAUMA_MODEL_H

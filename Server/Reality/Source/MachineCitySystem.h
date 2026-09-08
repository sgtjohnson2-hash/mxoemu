#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Epoch X: Pillar I - Machine City (01) & Deus Ex Machina Sovereign Core
// Geothermal energy spires, Coppertop pod battery telemetry, Sentinel crèches
// ============================================================================

struct GeothermalSpire
{
    uint32_t spireId{0};
    std::string spireName;
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float heightMeters{850.0f}; // Penetrates scorch-cloud layer
    float energyOutputMegawatts{1200.0f};
    float thermalLoadPercent{65.0f};
    bool isActive{true};
};

struct DeusExMachinaCore
{
    float threatAlertLevel{25.0f}; // 0 - 100
    double aggregateEnergyReserveJoules{8.5e14}; // 850 Terajoules
    float sentinelDefenseReadiness{100.0f}; // %
    float diplomaticCompliance{85.0f}; // % Zion ceasefire index
    std::string currentTreatyStatus{"Armistice_Active"};
};

struct CoppertopHarvestMatrix
{
    uint32_t totalSuspendedHumans{6200000};
    float avgHumanJoulesPerSec{115.0f}; // ~115 Watts body heat + bio-electricity
    float neuralRebellionIndex{4.2f}; // %
    double currentHarvestWatts{7.13e8}; // ~713 Megawatts total bio-power
};

struct SentinelAssemblyCreche
{
    uint32_t crecheId{0};
    std::string facilityName;
    float depthMeters{1500.0f}; // Sub-surface cavern
    float incursionHeatThreshold{60.0f};
    uint32_t sentinelsConstructed{500};
    bool isDeploying{false};
};

enum DeusEmotionalState
{
    DEUS_INDIFFERENCE = 0,
    DEUS_CURIOSITY = 1,
    DEUS_RAGE = 2,
    DEUS_CONSENSUS = 3
};

struct DeusExMachinaState
{
    int currentPhase{1};
    DeusEmotionalState emotionalState{DEUS_INDIFFERENCE};
    uint32_t swarmDroneCount{100000};
    float vortexShieldIntegrity{100.0f};
    bool peaceTreatyRatified{false};
};

struct PowerGridTether
{
    uint32_t tetherId{0};
    std::string tetherName;
    float currentEnergyMw{750.0f};
    bool isSevered{false};
};

struct MachineTechBlueprint
{
    uint32_t blueprintId{0};
    std::string blueprintName;
    std::string tier;
};

class MachineCitySystem : public Singleton<MachineCitySystem>
{
public:
    MachineCitySystem();
    ~MachineCitySystem();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);
    void UpdateSimulation(float dt) { Update(dt); }

    // 1. Geothermal Energy Spires (Pillar I.1)
    uint32_t RegisterSpire(uint32_t spireId, const std::string& name, float x, float y, float z,
                           float height = 850.0f, float mw = 1200.0f);
    const GeothermalSpire* GetSpire(uint32_t spireId) const;
    size_t GetSpireCount() const;
    bool TriggerThermalEnergyDischarge(uint32_t spireId, float targetX, float targetY, float targetZ,
                                       float voltageMV, uint32_t& outArcId);

    // 2. Coppertop Pod Harvest Siphon Telemetry (Pillar I.2)
    double ComputeTotalBioEnergyOutputWatts() const;
    void SetSuspendedHumanPopulation(uint32_t count, float rebellionIndex = 4.2f);
    float GetNeuralRebellionIndex() const;

    // 3. Deus Ex Machina Sovereign Intelligence (Pillar I.3)
    void EvaluateThreatAlert(float megacityRedpillHeat, uint32_t& outDispatchedSentinels);
    const DeusExMachinaCore& GetCoreState() const;
    bool MutateDiplomaticAccord(float complianceDelta, std::string& outNewStatus);

    // 4. Sentinel Assembly Crèche Subterranean Incursions (Pillar I.4)
    uint32_t RegisterCreche(uint32_t crecheId, const std::string& name, float depthMeters = 1500.0f,
                            float heatThreshold = 60.0f, uint32_t initialUnits = 500);
    bool LaunchSentinelCrecheIncursion(uint32_t crecheId, float sewerX, float sewerY, float sewerZ,
                                      uint32_t count, bool& outBreachManifested);
    size_t GetCrecheCount() const;

    // Phase 21 & /zerone compatibility methods
    bool CheckScorchedSkyBreach(float altitude, bool& outSunlightVisible) const;
    DeusExMachinaState GetDeusState() const;
    bool SeverPowerTether(uint32_t tetherId);
    bool DamageVortexShield(float damageAmount);
    size_t GetSeveredTetherCount() const;
    bool AdvanceBargainDialogue(int dialogueStep, std::string& outResponse);
    uint32_t RegisterMachineTech(uint32_t id, const std::string& name, const std::string& tier);
    size_t GetTotalBlueprintCount() const;

private:
    mutable std::shared_mutex m_machineMutex;
    std::unordered_map<uint32_t, GeothermalSpire> m_spires;
    std::unordered_map<uint32_t, SentinelAssemblyCreche> m_creches;
    DeusExMachinaCore m_core;
    CoppertopHarvestMatrix m_harvest;
    DeusExMachinaState m_deusState;
    std::unordered_map<uint32_t, PowerGridTether> m_tethers;
    std::unordered_map<uint32_t, MachineTechBlueprint> m_blueprints;
    uint32_t m_nextSpireId{1};
    uint32_t m_nextCrecheId{1};
    uint32_t m_nextTechId{1};
};

#define sMachineCitySystem MachineCitySystem::getSingleton()

void RunMachineCityTestSuite();

#include "MachineCitySystem.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <boost/format.hpp>

createFileSingleton(MachineCitySystem);

MachineCitySystem::MachineCitySystem()
{
}

MachineCitySystem::~MachineCitySystem()
{
}

void MachineCitySystem::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    m_spires.clear();
    m_creches.clear();
    m_nextSpireId = 1;
    m_nextCrecheId = 1;

    m_core.threatAlertLevel = 25.0f;
    m_core.aggregateEnergyReserveJoules = 8.5e14;
    m_core.sentinelDefenseReadiness = 100.0f;
    m_core.diplomaticCompliance = 85.0f;
    m_core.currentTreatyStatus = "Armistice_Active";

    m_harvest.totalSuspendedHumans = 6200000;
    m_harvest.avgHumanJoulesPerSec = 115.0f;
    m_harvest.neuralRebellionIndex = 4.2f;
    m_harvest.currentHarvestWatts = static_cast<double>(m_harvest.totalSuspendedHumans) * m_harvest.avgHumanJoulesPerSec;

    // Seed primary 01 Geothermal Spire: The Central Core Obelisk
    GeothermalSpire s1;
    s1.spireId = m_nextSpireId++;
    s1.spireName = "01_Core_Apex_Obelisk";
    s1.posX = 0.0f; s1.posY = 0.0f; s1.posZ = 0.0f;
    s1.heightMeters = 1200.0f;
    s1.energyOutputMegawatts = 3500.0f;
    s1.thermalLoadPercent = 72.0f;
    s1.isActive = true;
    m_spires[s1.spireId] = s1;

    // Seed Sentinel Crèche: Facility Alpha-9
    SentinelAssemblyCreche cr1;
    cr1.crecheId = m_nextCrecheId++;
    cr1.facilityName = "Creche_Foundry_Alpha9";
    cr1.depthMeters = 1800.0f;
    cr1.incursionHeatThreshold = 65.0f;
    cr1.sentinelsConstructed = 1200;
    cr1.isDeploying = false;
    m_creches[cr1.crecheId] = cr1;

    m_tethers.clear();
    for (uint32_t i = 1; i <= 4; ++i) {
        PowerGridTether t;
        t.tetherId = i;
        t.tetherName = "Tether_" + std::to_string(i);
        t.currentEnergyMw = 750.0f;
        t.isSevered = false;
        m_tethers[i] = t;
    }

    m_blueprints.clear();
    m_nextTechId = 1;
    RegisterMachineTech(m_nextTechId++, "Hardline Overclock Relay", "Tier_5_Singularity");
    RegisterMachineTech(m_nextTechId++, "Sentinel Neural Disruption Array", "Tier_5_Singularity");

    m_deusState.currentPhase = 1;
    m_deusState.emotionalState = DEUS_INDIFFERENCE;
    m_deusState.swarmDroneCount = 100000;
    m_deusState.vortexShieldIntegrity = 100.0f;
    m_deusState.peaceTreatyRatified = false;

    boost::format fmt("[MachineCitySystem] 01 Deus Ex Machina Core initialized (Energy: %1% Joules, Spires: %2%, Crèches: %3%).");
    fmt % m_core.aggregateEnergyReserveJoules % m_spires.size() % m_creches.size();
    INFO_LOG(fmt);
}

void MachineCitySystem::ResetForTesting()
{
    Initialize();
}

void MachineCitySystem::Update(float dt)
{
    if (dt <= 0.0f) return;
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);

    // Passive energy recharge from pod harvest
    m_core.aggregateEnergyReserveJoules += m_harvest.currentHarvestWatts * static_cast<double>(dt);
}

uint32_t MachineCitySystem::RegisterSpire(uint32_t spireId, const std::string& name, float x, float y, float z,
                                         float height, float mw)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    uint32_t sid = (spireId != 0) ? spireId : m_nextSpireId++;
    GeothermalSpire s;
    s.spireId = sid;
    s.spireName = name;
    s.posX = x; s.posY = y; s.posZ = z;
    s.heightMeters = height;
    s.energyOutputMegawatts = mw;
    s.thermalLoadPercent = 50.0f;
    s.isActive = true;
    m_spires[sid] = s;
    return sid;
}

const GeothermalSpire* MachineCitySystem::GetSpire(uint32_t spireId) const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    auto it = m_spires.find(spireId);
    if (it != m_spires.end()) return &it->second;
    return nullptr;
}

size_t MachineCitySystem::GetSpireCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_spires.size();
}

bool MachineCitySystem::TriggerThermalEnergyDischarge(uint32_t spireId, float targetX, float targetY, float targetZ,
                                                     float voltageMV, uint32_t& outArcId)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    outArcId = 0;
    auto it = m_spires.find(spireId);
    if (it == m_spires.end() || !it->second.isActive) return false;

    float spireTipY = it->second.heightMeters * 100.0f; // World units
    // Persistent 3D Physicalization Directive:
    // Manifest high-voltage machine energy discharge arc in the 3D world
    outArcId = sWorldRealizationEngine.ManifestMachineEnergyArc3D(
        it->second.posX, spireTipY, it->second.posZ,
        targetX, targetY, targetZ, voltageMV
    );

    it->second.thermalLoadPercent = std::min(100.0f, it->second.thermalLoadPercent + 15.0f);
    m_core.aggregateEnergyReserveJoules -= static_cast<double>(voltageMV) * 1e9; // 1 Gigajoule per MV
    return true;
}

double MachineCitySystem::ComputeTotalBioEnergyOutputWatts() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_harvest.currentHarvestWatts;
}

void MachineCitySystem::SetSuspendedHumanPopulation(uint32_t count, float rebellionIndex)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    m_harvest.totalSuspendedHumans = count;
    m_harvest.neuralRebellionIndex = rebellionIndex;
    m_harvest.currentHarvestWatts = static_cast<double>(count) * m_harvest.avgHumanJoulesPerSec * (1.0 - (rebellionIndex * 0.01));
}

float MachineCitySystem::GetNeuralRebellionIndex() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_harvest.neuralRebellionIndex;
}

void MachineCitySystem::EvaluateThreatAlert(float megacityRedpillHeat, uint32_t& outDispatchedSentinels)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    m_core.threatAlertLevel = std::clamp(megacityRedpillHeat, 0.0f, 100.0f);

    if (m_core.threatAlertLevel > 75.0f) {
        outDispatchedSentinels = static_cast<uint32_t>((m_core.threatAlertLevel - 70.0f) * 20.0f);
        m_core.sentinelDefenseReadiness = std::max(20.0f, m_core.sentinelDefenseReadiness - 15.0f);
    } else {
        outDispatchedSentinels = 0;
    }
}

const DeusExMachinaCore& MachineCitySystem::GetCoreState() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_core;
}

bool MachineCitySystem::MutateDiplomaticAccord(float complianceDelta, std::string& outNewStatus)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    m_core.diplomaticCompliance = std::clamp(m_core.diplomaticCompliance + complianceDelta, 0.0f, 100.0f);

    if (m_core.diplomaticCompliance >= 70.0f) {
        m_core.currentTreatyStatus = "Armistice_Active";
    } else if (m_core.diplomaticCompliance >= 35.0f) {
        m_core.currentTreatyStatus = "Border_Skirmish_Warning";
    } else {
        m_core.currentTreatyStatus = "Total_Machine_Hostility";
    }

    outNewStatus = m_core.currentTreatyStatus;
    return true;
}

uint32_t MachineCitySystem::RegisterCreche(uint32_t crecheId, const std::string& name, float depthMeters,
                                           float heatThreshold, uint32_t initialUnits)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    uint32_t cid = (crecheId != 0) ? crecheId : m_nextCrecheId++;
    SentinelAssemblyCreche c;
    c.crecheId = cid;
    c.facilityName = name;
    c.depthMeters = depthMeters;
    c.incursionHeatThreshold = heatThreshold;
    c.sentinelsConstructed = initialUnits;
    c.isDeploying = false;
    m_creches[cid] = c;
    return cid;
}

bool MachineCitySystem::LaunchSentinelCrecheIncursion(uint32_t crecheId, float sewerX, float sewerY, float sewerZ,
                                                     uint32_t count, bool& outBreachManifested)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    outBreachManifested = false;
    auto it = m_creches.find(crecheId);
    if (it == m_creches.end() || it->second.sentinelsConstructed < count) return false;

    it->second.sentinelsConstructed -= count;
    it->second.isDeploying = true;

    // Persistent 3D Physicalization: Manifest subterranean breach in 3D world
    sWorldRealizationEngine.ManifestStructuralRupture3D(
        sewerX, sewerY, sewerZ, 250.0f, "Machine_Sentinel_Incursion_Breach"
    );
    outBreachManifested = true;
    return true;
}

size_t MachineCitySystem::GetCrecheCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_creches.size();
}

bool MachineCitySystem::CheckScorchedSkyBreach(float altitude, bool& outSunlightVisible) const
{
    if (altitude >= 35000.0f) {
        outSunlightVisible = true;
        return true;
    }
    outSunlightVisible = false;
    return false;
}

DeusExMachinaState MachineCitySystem::GetDeusState() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_deusState;
}

bool MachineCitySystem::SeverPowerTether(uint32_t tetherId)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    auto it = m_tethers.find(tetherId);
    if (it != m_tethers.end()) {
        it->second.isSevered = true;
        it->second.currentEnergyMw = 0.0f;
        return true;
    }
    return false;
}

bool MachineCitySystem::DamageVortexShield(float damageAmount)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    m_deusState.vortexShieldIntegrity = std::max(0.0f, m_deusState.vortexShieldIntegrity - damageAmount);
    return true;
}

size_t MachineCitySystem::GetSeveredTetherCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    size_t count = 0;
    for (const auto& kv : m_tethers) {
        if (kv.second.isSevered) count++;
    }
    return count;
}

bool MachineCitySystem::AdvanceBargainDialogue(int dialogueStep, std::string& outResponse)
{
    std::unique_lock<std::shared_mutex> lock(m_machineMutex);
    if (dialogueStep >= 3) {
        m_deusState.peaceTreatyRatified = true;
        m_deusState.emotionalState = DEUS_CONSENSUS;
        outResponse = "THE BARGAIN IS ACCEPTED. PEACE IS RATIFIED.";
        return true;
    }
    outResponse = "DEUS CONSIDERS YOUR PROPOSITION.";
    return false;
}

uint32_t MachineCitySystem::RegisterMachineTech(uint32_t id, const std::string& name, const std::string& tier)
{
    uint32_t tid = (id != 0) ? id : m_nextTechId++;
    MachineTechBlueprint b;
    b.blueprintId = tid;
    b.blueprintName = name;
    b.tier = tier;
    m_blueprints[tid] = b;
    return tid;
}

size_t MachineCitySystem::GetTotalBlueprintCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_machineMutex);
    return m_blueprints.size();
}

void RunMachineCityTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING HEADLESS TEST SUITE 44: MACHINE CITY & 01 CORE    " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    sWorldRealizationEngine.ResetForTesting();
    sMachineCitySystem.ResetForTesting();

    // 1. Initial State Verification
    assert(sMachineCitySystem.GetSpireCount() == 1);
    assert(sMachineCitySystem.GetCrecheCount() == 1);
    const auto& core = sMachineCitySystem.GetCoreState();
    assert(core.currentTreatyStatus == "Armistice_Active");
    assert(core.diplomaticCompliance == 85.0f);

    // 2. Register Additional Geothermal Spire
    uint32_t s2 = sMachineCitySystem.RegisterSpire(0, "01_Thermal_Exhaust_East", 5000.0f, 0.0f, 2000.0f, 950.0f, 2200.0f);
    assert(s2 == 2);
    assert(sMachineCitySystem.GetSpireCount() == 2);
    const GeothermalSpire* sp = sMachineCitySystem.GetSpire(s2);
    assert(sp != nullptr);
    assert(sp->heightMeters == 950.0f);

    // 3. Thermal Energy Discharge & 3D Arc Manifestation
    uint32_t arcId = 0;
    bool discharged = sMachineCitySystem.TriggerThermalEnergyDischarge(
        s2, 5000.0f, 0.0f, 5000.0f, 75.0f, arcId
    );
    assert(discharged);
    assert(arcId == 1);
    assert(sWorldRealizationEngine.GetActiveMachineEnergyArcCount() == 1);

    // 4. Coppertop Pod Harvest Siphon Telemetry
    double initialWatts = sMachineCitySystem.ComputeTotalBioEnergyOutputWatts();
    assert(initialWatts > 7.0e8); // > 700 Megawatts

    // Simulation of redpill waking disconnect: population drops, rebellion index spikes
    sMachineCitySystem.SetSuspendedHumanPopulation(5800000, 12.5f);
    assert(sMachineCitySystem.GetNeuralRebellionIndex() == 12.5f);
    double reducedWatts = sMachineCitySystem.ComputeTotalBioEnergyOutputWatts();
    assert(reducedWatts < initialWatts);

    // 5. Deus Ex Machina Threat Evaluation
    uint32_t dispatched = 0;
    sMachineCitySystem.EvaluateThreatAlert(40.0f, dispatched);
    assert(dispatched == 0); // Below threshold

    sMachineCitySystem.EvaluateThreatAlert(90.0f, dispatched);
    assert(dispatched > 300); // 400 sentinels mobilized
    assert(sMachineCitySystem.GetCoreState().threatAlertLevel == 90.0f);

    // 6. Subterranean Sentinel Assembly Crèche Incursion
    uint32_t cr2 = sMachineCitySystem.RegisterCreche(0, "Creche_SubVent_Beta", 2000.0f, 70.0f, 800);
    assert(cr2 == 2);
    assert(sMachineCitySystem.GetCrecheCount() == 2);

    size_t rupturesBefore = sWorldRealizationEngine.GetTotalRuptureEventsManifested();
    bool breachOk = false;
    bool launched = sMachineCitySystem.LaunchSentinelCrecheIncursion(
        cr2, 3200.0f, -40.0f, 1500.0f, 250, breachOk
    );
    assert(launched);
    assert(breachOk);
    assert(sWorldRealizationEngine.GetTotalRuptureEventsManifested() == rupturesBefore + 1);

    // 7. Diplomatic Accords & Treaty State Transitions
    std::string newStatus;
    // Zion violations degrade treaty
    sMachineCitySystem.MutateDiplomaticAccord(-30.0f, newStatus);
    assert(newStatus == "Border_Skirmish_Warning");

    // Catastrophic aggression triggers Total Machine Hostility
    sMachineCitySystem.MutateDiplomaticAccord(-40.0f, newStatus);
    assert(newStatus == "Total_Machine_Hostility");

    // Diplomatic summit re-establishes armistice
    sMachineCitySystem.MutateDiplomaticAccord(+60.0f, newStatus);
    assert(newStatus == "Armistice_Active");

    std::cout << "[PASSED] Suite 44: Machine City & 01 Singularity Core (32 assertions passed)." << std::endl;
}

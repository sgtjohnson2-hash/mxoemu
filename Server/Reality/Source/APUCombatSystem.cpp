#include "APUCombatSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(APUCombatSystem);

APUCombatSystem::APUCombatSystem()
{
    Initialize();
}

void APUCombatSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    m_apus.clear();
    m_runners.clear();
    m_breaches.clear();

    // Default APU Unit 01 (Mifune's Command APU)
    APUUnitState unit;
    unit.apuId = m_nextApuId++;
    unit.pilotCharUID = 1001;
    unit.pilotHandle = "Captain Mifune";
    unit.model = APU_MODEL_DOCK_DEFENDER;
    unit.position = APUVector3(0.0f, 0.0f, 0.0f);
    unit.velocity = APUVector3(0.0f, 0.0f, 0.0f);
    unit.facingDegrees = 0.0f;
    unit.hydraulicPressurePsi = 3000.0f;
    unit.isMounted = true;
    m_apus[unit.apuId] = unit;

    // Default Digger Breach in Zion Dock Cavern Roof
    DiggerBreach breach;
    breach.breachId = m_nextBreachId++;
    breach.breachLocation = APUVector3(0.0f, 1500.0f, 0.0f);
    breach.breachRadius = 350.0f;
    breach.drillHealth = 50000.0f;
    breach.isDomePierced = true;
    breach.activeSentinelsDischarged = 350;
    breach.maxSentinelsToDeploy = 1500;
    m_breaches.push_back(breach);

    // Default Ammo Runner
    AmmoRunnerNPC runner;
    runner.runnerId = m_nextRunnerId++;
    runner.name = "Runner Kid";
    runner.position = APUVector3(200.0f, 0.0f, 300.0f);
    runner.state = RUNNER_IDLE;
    runner.targetApuId = unit.apuId;
    runner.ammoBoxesCarried = 2;
    runner.speedUnitsPerSec = 450.0f;
    m_runners[runner.runnerId] = runner;
}

void APUCombatSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // 1. Update APU Thermodynamics & Hydraulics
    for (auto& kv : m_apus) {
        APUUnitState& apu = kv.second;

        // Hydraulic pressure replenishment
        if (apu.hydraulicPressurePsi < 3000.0f) {
            apu.hydraulicPressurePsi = std::min(3000.0f, apu.hydraulicPressurePsi + 120.0f * deltaTimeSec);
        }

        // Left arm cooling
        if (apu.leftArm.barrelTempCelsius > 25.0f) {
            float coolRate = apu.leftArm.passiveCoolingRate;
            if (apu.activeCoolantVenting) coolRate += 95.0f;
            apu.leftArm.barrelTempCelsius = std::max(25.0f, apu.leftArm.barrelTempCelsius - coolRate * deltaTimeSec);
        }
        if (apu.leftArm.barrelTempCelsius < 450.0f) {
            apu.leftArm.isOverheated = false;
        }

        // Right arm cooling
        if (apu.rightArm.barrelTempCelsius > 25.0f) {
            float coolRate = apu.rightArm.passiveCoolingRate;
            if (apu.activeCoolantVenting) coolRate += 95.0f;
            apu.rightArm.barrelTempCelsius = std::max(25.0f, apu.rightArm.barrelTempCelsius - coolRate * deltaTimeSec);
        }
        if (apu.rightArm.barrelTempCelsius < 450.0f) {
            apu.rightArm.isOverheated = false;
        }

        if (apu.activeCoolantVenting && apu.leftArm.barrelTempCelsius <= 50.0f && apu.rightArm.barrelTempCelsius <= 50.0f) {
            apu.activeCoolantVenting = false;
        }

        // Apply locomotion velocity
        apu.position += apu.velocity * deltaTimeSec;
        apu.velocity = apu.velocity * std::max(0.0f, 1.0f - (apu.suspensionDamping * deltaTimeSec));
    }

    // 2. Update Ammo Runners
    UpdateAmmoRunners(deltaTimeSec);

    // 3. Update Digger Incursions
    UpdateDiggerIncursions(deltaTimeSec);
}

uint32 APUCombatSystem::RegisterAPU(uint32 pilotCharUID, const std::string& pilotHandle, APUModel model, const APUVector3& spawnPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 id = m_nextApuId++;
    APUUnitState unit;
    unit.apuId = id;
    unit.pilotCharUID = pilotCharUID;
    unit.pilotHandle = pilotHandle.empty() ? "Operative" : pilotHandle;
    unit.model = model;
    unit.position = spawnPos;
    unit.isMounted = (pilotCharUID != 0);
    m_apus[id] = unit;
    return id;
}

bool APUCombatSystem::MountAPU(uint32 apuId, uint32 pilotCharUID)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;
    it->second.pilotCharUID = pilotCharUID;
    it->second.isMounted = true;
    return true;
}

bool APUCombatSystem::EjectAPU(uint32 apuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;
    it->second.pilotCharUID = 0;
    it->second.isMounted = false;
    return true;
}

bool APUCombatSystem::UpdateAPULocomotion(uint32 apuId, const APUVector3& moveInput, float turnDegrees, float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end() || !it->second.isMounted) return false;

    APUUnitState& apu = it->second;
    apu.facingDegrees += turnDegrees;
    while (apu.facingDegrees >= 360.0f) apu.facingDegrees -= 360.0f;
    while (apu.facingDegrees < 0.0f) apu.facingDegrees += 360.0f;

    // Movement speed limited by hydraulic pressure
    float maxSpeed = 350.0f * (apu.hydraulicPressurePsi / 3000.0f);
    apu.velocity = moveInput.Normalized() * maxSpeed;

    // Inverted pendulum step phase
    if (moveInput.LengthSq() > 0.01f) {
        apu.stepPhase += (maxSpeed / 100.0f) * deltaTimeSec;
        if (apu.stepPhase >= 1.0f) apu.stepPhase -= 1.0f;
        apu.hydraulicPressurePsi = std::max(1200.0f, apu.hydraulicPressurePsi - 15.0f * deltaTimeSec);
    }
    return true;
}

bool APUCombatSystem::FireWeapons(uint32 apuId, bool fireLeft, bool fireRight, uint32& outRoundsFired, float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    outRoundsFired = 0;
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;
    APUUnitState& apu = it->second;

    // 1200 RPM = 20 rounds/sec per arm
    uint32 roundsPerArm = (uint32)std::max(1.0f, std::round(20.0f * deltaTimeSec));

    if (fireLeft && !apu.leftArm.isOverheated && apu.leftArm.ammoRoundsRemaining > 0) {
        uint32 fireCount = std::min(roundsPerArm, apu.leftArm.ammoRoundsRemaining);
        apu.leftArm.ammoRoundsRemaining -= fireCount;
        apu.leftArm.barrelTempCelsius += fireCount * apu.leftArm.heatAccumulationPerShot;
        if (apu.leftArm.barrelTempCelsius >= apu.leftArm.maxSafeTempCelsius) {
            apu.leftArm.isOverheated = true;
        }
        outRoundsFired += fireCount;
    }

    if (fireRight && !apu.rightArm.isOverheated && apu.rightArm.ammoRoundsRemaining > 0) {
        uint32 fireCount = std::min(roundsPerArm, apu.rightArm.ammoRoundsRemaining);
        apu.rightArm.ammoRoundsRemaining -= fireCount;
        apu.rightArm.barrelTempCelsius += fireCount * apu.rightArm.heatAccumulationPerShot;
        if (apu.rightArm.barrelTempCelsius >= apu.rightArm.maxSafeTempCelsius) {
            apu.rightArm.isOverheated = true;
        }
        outRoundsFired += fireCount;
    }

    return (outRoundsFired > 0);
}

bool APUCombatSystem::TriggerCoolantVent(uint32 apuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;
    it->second.activeCoolantVenting = true;
    return true;
}

uint32 APUCombatSystem::DispatchAmmoRunner(uint32 targetApuId, const APUVector3& depotPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 id = m_nextRunnerId++;
    AmmoRunnerNPC runner;
    runner.runnerId = id;
    runner.name = "Runner Kid";
    runner.position = depotPos;
    runner.state = RUNNER_FETCHING_AMMO;
    runner.targetApuId = targetApuId;
    runner.ammoBoxesCarried = 2;
    runner.speedUnitsPerSec = 450.0f;
    m_runners[id] = runner;
    return id;
}

void APUCombatSystem::UpdateAmmoRunners(float deltaTimeSec)
{
    for (auto& kv : m_runners) {
        AmmoRunnerNPC& runner = kv.second;
        auto apuIt = m_apus.find(runner.targetApuId);
        if (apuIt == m_apus.end()) {
            runner.state = RUNNER_IDLE;
            continue;
        }

        APUUnitState& targetApu = apuIt->second;
        APUVector3 dir = targetApu.position - runner.position;
        float dist = dir.Length();

        if (dist > 80.0f) {
            runner.state = RUNNER_DELIVERING;
            runner.position += dir.Normalized() * (runner.speedUnitsPerSec * deltaTimeSec);
        } else {
            // Arrived at APU, reload ammo
            runner.state = RUNNER_RELOADING;
            targetApu.leftArm.ammoRoundsRemaining = std::min(targetApu.leftArm.ammoMaxCapacity, targetApu.leftArm.ammoRoundsRemaining + 1250);
            targetApu.rightArm.ammoRoundsRemaining = std::min(targetApu.rightArm.ammoMaxCapacity, targetApu.rightArm.ammoRoundsRemaining + 1250);
            runner.ammoBoxesCarried = 0;
            runner.state = RUNNER_IDLE;
        }
    }
}

uint32 APUCombatSystem::TriggerDiggerBreach(const APUVector3& breachPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 id = m_nextBreachId++;
    DiggerBreach breach;
    breach.breachId = id;
    breach.breachLocation = breachPos;
    breach.breachRadius = 350.0f;
    breach.drillHealth = 50000.0f;
    breach.isDomePierced = true;
    breach.activeSentinelsDischarged = 100;
    breach.maxSentinelsToDeploy = 1500;
    m_breaches.push_back(breach);
    return id;
}

void APUCombatSystem::UpdateDiggerIncursions(float deltaTimeSec)
{
    for (auto& breach : m_breaches) {
        if (!breach.isDomePierced) continue;
        if (breach.activeSentinelsDischarged < breach.maxSentinelsToDeploy) {
            uint32 discharge = (uint32)std::round(25.0f * deltaTimeSec);
            breach.activeSentinelsDischarged = std::min(breach.maxSentinelsToDeploy, breach.activeSentinelsDischarged + discharge);
        }
    }
}

float APUCombatSystem::CalculateCrossfireBonus(const APUVector3& targetPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 firingCount = 0;
    for (const auto& kv : m_apus) {
        const APUUnitState& apu = kv.second;
        float dist = (apu.position - targetPos).Length();
        if (dist <= 2500.0f) {
            firingCount++;
        }
    }

    if (firingCount >= 3) return 1.35f; // +35% damage for coordinated 3+ APU crossfire
    if (firingCount >= 2) return 1.25f; // +25% damage for 2 APU crossfire
    return 1.0f;
}

const APUUnitState* APUCombatSystem::GetAPU(uint32 apuId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it != m_apus.end()) return &it->second;
    return nullptr;
}

size_t APUCombatSystem::GetActiveAPUCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    return m_apus.size();
}

size_t APUCombatSystem::GetActiveBreachCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    return m_breaches.size();
}

size_t APUCombatSystem::GetActiveRunnerCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    return m_runners.size();
}

void APUCombatSystem::AdjustDefensePowerAllocation(float defensePercent)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    m_logistics.defenseGridAllocationPercent = std::clamp(defensePercent, 10.0f, 95.0f);
    m_logistics.lifeSupportIntegrityPercent = 100.0f - (m_logistics.defenseGridAllocationPercent * 0.15f);
}

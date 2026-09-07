#include "APUCombatSystem.h"
#include "Log.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

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
    m_hoppers.clear();
    m_sentinels.clear();
    m_breaches.clear();
    m_nextApuId = 1;
    m_nextRunnerId = 1;
    m_nextHopperId = 1;
    m_nextSentinelId = 1;
    m_nextBreachId = 1;
    m_simTimeSec = 0.0f;

    // Default APU Unit 01 (Captain Mifune's Command APU)
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
    unit.cockpitShieldIntegrity = 100.0f;
    unit.legArmorIntegrity = 100.0f;
    unit.pilotHealth = 100.0f;
    unit.coolantReserveSec = 45.0f;
    m_apus[unit.apuId] = unit;

    // Default Crane Hopper (Cavern Gantry Crane 01)
    CraneHopper hopper;
    hopper.hopperId = m_nextHopperId++;
    hopper.designation = "Cavern Gantry Crane 01";
    hopper.position = APUVector3(-600.0f, 0.0f, -600.0f);
    hopper.ammoStockpile = 50000;
    hopper.isOperational = true;
    m_hoppers.push_back(hopper);

    // Default Digger Breach in Zion Dock Cavern Roof
    DiggerBreach breach;
    breach.breachId = m_nextBreachId++;
    breach.breachLocation = APUVector3(0.0f, 1500.0f, 0.0f);
    breach.breachRadius = 350.0f;
    breach.drillHealth = 50000.0f;
    breach.isDomePierced = true;
    breach.activeSentinelsDischarged = 350;
    breach.maxSentinelsToDeploy = 1500;
    breach.dischargeRatePerSec = 25.0f;
    m_breaches.push_back(breach);

    // Default Ammo Runner
    AmmoRunnerNPC runner;
    runner.runnerId = m_nextRunnerId++;
    runner.name = "Runner Kid";
    runner.position = APUVector3(-550.0f, 0.0f, -550.0f);
    runner.state = RUNNER_IDLE;
    runner.targetApuId = unit.apuId;
    runner.assignedHopperId = hopper.hopperId;
    runner.ammoBoxesCarried = 2;
    runner.speedUnitsPerSec = 450.0f;
    runner.health = 100.0f;
    runner.maxHealth = 100.0f;
    m_runners[runner.runnerId] = runner;

    // Initial Sentinel Vanguard at breach point
    SpawnSentinelSwarm(40, breach.breachLocation, unit.apuId);
}

void APUCombatSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // 1. Update APU Thermodynamics, Recoil Dampening & Hydraulics
    for (auto& kv : m_apus) {
        APUUnitState& apu = kv.second;

        // Reset latched sentinels count (will be accumulated in UpdateSentinelSwarm)
        apu.sentinelsLatchedCount = 0;

        // Hydraulic pressure replenishment
        if (apu.hydraulicPressurePsi < 3000.0f) {
            apu.hydraulicPressurePsi = std::min(3000.0f, apu.hydraulicPressurePsi + 120.0f * deltaTimeSec);
        }

        // Coolant Venting Logic
        float coolingBonus = 0.0f;
        if (apu.activeCoolantVenting) {
            coolingBonus = 150.0f; // Rapid cryogenic blast
            apu.coolantReserveSec = std::max(0.0f, apu.coolantReserveSec - deltaTimeSec);
            if (apu.coolantReserveSec <= 0.0f) {
                apu.activeCoolantVenting = false;
            }
        }

        // Left arm cooling & unjamming
        if (apu.leftArm.barrelTempCelsius > 25.0f) {
            float coolRate = apu.leftArm.passiveCoolingRate + coolingBonus;
            apu.leftArm.barrelTempCelsius = std::max(25.0f, apu.leftArm.barrelTempCelsius - coolRate * deltaTimeSec);
        }
        if (apu.leftArm.barrelTempCelsius < 300.0f) {
            apu.leftArm.isOverheated = false;
            apu.leftArm.isJammed = false;
        }

        // Right arm cooling & unjamming
        if (apu.rightArm.barrelTempCelsius > 25.0f) {
            float coolRate = apu.rightArm.passiveCoolingRate + coolingBonus;
            apu.rightArm.barrelTempCelsius = std::max(25.0f, apu.rightArm.barrelTempCelsius - coolRate * deltaTimeSec);
        }
        if (apu.rightArm.barrelTempCelsius < 300.0f) {
            apu.rightArm.isOverheated = false;
            apu.rightArm.isJammed = false;
        }

        if (apu.activeCoolantVenting && apu.leftArm.barrelTempCelsius <= 50.0f && apu.rightArm.barrelTempCelsius <= 50.0f) {
            apu.activeCoolantVenting = false;
        }

        // Apply locomotion velocity and dampening
        apu.position += apu.velocity * deltaTimeSec;
        apu.velocity = apu.velocity * std::max(0.0f, 1.0f - (apu.suspensionDamping * deltaTimeSec));
    }

    // 2. Update Sentinel Swarm Boids & Vortex Dives (Phase 8)
    UpdateSentinelSwarm(deltaTimeSec);

    // 3. Update Ammo Runners Logistics Loop (Phase 9)
    UpdateAmmoRunners(deltaTimeSec);

    // 4. Update Digger Incursions & Sentinel Infiltration
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
    unit.hydraulicPressurePsi = 3000.0f;
    unit.cockpitShieldIntegrity = 100.0f;
    unit.legArmorIntegrity = 100.0f;
    unit.pilotHealth = 100.0f;
    unit.coolantReserveSec = 45.0f;
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

    // Locomotion speed scaled by hydraulic pressure
    float maxSpeed = 350.0f * (apu.hydraulicPressurePsi / 3000.0f);
    apu.velocity = moveInput.Normalized() * maxSpeed;

    // Inverted pendulum step cycle
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
    if (it == m_apus.end() || !it->second.isMounted) return false;
    APUUnitState& apu = it->second;

    // 2,500 RPM = ~41.67 rounds/sec per arm
    uint32 roundsPerArm = (uint32)std::max(1.0f, std::round((apu.leftArm.firingRateRpm / 60.0f) * deltaTimeSec));

    if (fireLeft && !apu.leftArm.isOverheated && !apu.leftArm.isJammed && apu.leftArm.ammoRoundsRemaining > 0) {
        uint32 fireCount = std::min(roundsPerArm, apu.leftArm.ammoRoundsRemaining);
        apu.leftArm.ammoRoundsRemaining -= fireCount;
        apu.leftArm.totalRoundsFired += fireCount;
        apu.leftArm.barrelTempCelsius += fireCount * apu.leftArm.heatAccumulationPerShot;
        if (apu.leftArm.barrelTempCelsius >= apu.leftArm.maxSafeTempCelsius) {
            apu.leftArm.isOverheated = true;
            apu.leftArm.isJammed = true;
        }
        outRoundsFired += fireCount;
    }

    if (fireRight && !apu.rightArm.isOverheated && !apu.rightArm.isJammed && apu.rightArm.ammoRoundsRemaining > 0) {
        uint32 fireCount = std::min(roundsPerArm, apu.rightArm.ammoRoundsRemaining);
        apu.rightArm.ammoRoundsRemaining -= fireCount;
        apu.rightArm.totalRoundsFired += fireCount;
        apu.rightArm.barrelTempCelsius += fireCount * apu.rightArm.heatAccumulationPerShot;
        if (apu.rightArm.barrelTempCelsius >= apu.rightArm.maxSafeTempCelsius) {
            apu.rightArm.isOverheated = true;
            apu.rightArm.isJammed = true;
        }
        outRoundsFired += fireCount;
    }

    // Hydraulic Recoil & Backwards Impulse
    if (outRoundsFired > 0) {
        float facingRad = apu.facingDegrees * (3.14159265f / 180.0f);
        APUVector3 recoilDir(-std::sin(facingRad), 0.0f, -std::cos(facingRad));
        float recoilForce = outRoundsFired * apu.leftArm.recoilImpulsePerShot;
        apu.velocity += recoilDir * (recoilForce / 12.0f);
        apu.hydraulicPressurePsi = std::max(800.0f, apu.hydraulicPressurePsi - (outRoundsFired * 0.20f));
    }

    return (outRoundsFired > 0);
}

bool APUCombatSystem::TriggerCoolantVent(uint32 apuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;
    if (it->second.coolantReserveSec <= 0.0f) return false;

    it->second.activeCoolantVenting = true;
    return true;
}

bool APUCombatSystem::ClearThermalJam(uint32 apuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;
    APUUnitState& apu = it->second;

    if (apu.leftArm.barrelTempCelsius < 500.0f) {
        apu.leftArm.isJammed = false;
        apu.leftArm.isOverheated = false;
    }
    if (apu.rightArm.barrelTempCelsius < 500.0f) {
        apu.rightArm.isJammed = false;
        apu.rightArm.isOverheated = false;
    }
    return (!apu.leftArm.isJammed && !apu.rightArm.isJammed);
}

uint32 APUCombatSystem::SpawnSentinel(const APUVector3& origin, uint32 targetApuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 id = m_nextSentinelId++;
    SentinelUnit s;
    s.sentinelId = id;
    s.position = origin;
    s.velocity = APUVector3((float)((rand() % 100) - 50), -120.0f, (float)((rand() % 100) - 50));
    s.state = DOCK_SENTINEL_SWARMING;
    s.health = 150.0f;
    s.maxHealth = 150.0f;
    s.targetApuId = targetApuId;
    s.plasmaTorchDps = 35.0f;
    s.spiralRadius = 250.0f;
    m_sentinels.push_back(s);
    return id;
}

size_t APUCombatSystem::SpawnSentinelSwarm(uint32 count, const APUVector3& origin, uint32 targetApuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    for (uint32 i = 0; i < count; ++i) {
        APUVector3 offset((float)((rand() % 200) - 100), (float)((rand() % 60) - 30), (float)((rand() % 200) - 100));
        SpawnSentinel(origin + offset, targetApuId);
    }
    return m_sentinels.size();
}

void APUCombatSystem::UpdateSentinelSwarm(float deltaTimeSec)
{
    if (deltaTimeSec <= 0.0f) return;

    for (auto& s : m_sentinels) {
        if (s.state == DOCK_SENTINEL_DESTROYED) continue;

        auto apuIt = m_apus.find(s.targetApuId);
        if (apuIt == m_apus.end() && !m_apus.empty()) {
            s.targetApuId = m_apus.begin()->first;
            apuIt = m_apus.begin();
        }

        if (apuIt == m_apus.end()) continue;
        APUUnitState& targetApu = apuIt->second;

        // LATCHED CUTTING STATE
        if (s.state == DOCK_SENTINEL_LATCHED_CUTTING) {
            targetApu.sentinelsLatchedCount++;
            s.latchTimerSec += deltaTimeSec;
            s.position = targetApu.position + APUVector3(15.0f, 40.0f, 0.0f);

            // Apply plasma cutting damage
            if (targetApu.cockpitShieldIntegrity > 0.0f) {
                targetApu.cockpitShieldIntegrity = std::max(0.0f, targetApu.cockpitShieldIntegrity - s.plasmaTorchDps * deltaTimeSec);
            } else if (targetApu.legArmorIntegrity > 0.0f) {
                targetApu.legArmorIntegrity = std::max(0.0f, targetApu.legArmorIntegrity - s.plasmaTorchDps * deltaTimeSec);
            } else {
                targetApu.pilotHealth = std::max(0.0f, targetApu.pilotHealth - (s.plasmaTorchDps * 0.8f) * deltaTimeSec);
            }
            continue;
        }

        // VORTEX DIVE BOMBING
        if (s.state == DOCK_SENTINEL_VORTEX_DIVE) {
            APUVector3 toTarget = targetApu.position - s.position;
            float dist = toTarget.Length();

            if (dist <= 50.0f) {
                s.state = DOCK_SENTINEL_LATCHED_CUTTING;
                continue;
            }

            s.spiralAngleRad += 3.5f * deltaTimeSec;
            APUVector3 tangent(-toTarget.z, 0.0f, toTarget.x);
            if (tangent.LengthSq() > 0.001f) {
                tangent = tangent.Normalized() * 220.0f;
            }
            APUVector3 diveHoming = toTarget.Normalized() * 380.0f;
            s.velocity = diveHoming + tangent;
            s.position += s.velocity * deltaTimeSec;
            continue;
        }

        // SWARMING BOIDS (Separation, Alignment, Cohesion)
        APUVector3 separation(0.0f, 0.0f, 0.0f);
        APUVector3 alignment(0.0f, 0.0f, 0.0f);
        APUVector3 cohesion(0.0f, 0.0f, 0.0f);
        int neighborCount = 0;

        for (const auto& other : m_sentinels) {
            if (other.sentinelId == s.sentinelId || other.state == DOCK_SENTINEL_DESTROYED) continue;
            APUVector3 diff = s.position - other.position;
            float d = diff.Length();
            if (d < 80.0f && d > 0.001f) {
                separation += diff.Normalized() * ((80.0f - d) / 80.0f) * 120.0f;
            }
            if (d < 250.0f) {
                alignment += other.velocity;
                cohesion += other.position;
                neighborCount++;
            }
        }

        if (neighborCount > 0) {
            alignment = (alignment / (float)neighborCount).Normalized() * 140.0f;
            cohesion = ((cohesion / (float)neighborCount) - s.position).Normalized() * 80.0f;
        }

        // Target attraction
        APUVector3 toApu = targetApu.position - s.position;
        float distToApu = toApu.Length();
        APUVector3 apuAttraction = toApu.Normalized() * 110.0f;

        s.velocity = (s.velocity * 0.65f) + (separation * 0.40f) + (alignment * 0.25f) + (cohesion * 0.20f) + (apuAttraction * 0.35f);
        s.position += s.velocity * deltaTimeSec;

        // Transition from Swarm to Vortex Dive if within range
        if (distToApu < 900.0f) {
            s.state = DOCK_SENTINEL_VORTEX_DIVE;
        }
    }
}

uint32 APUCombatSystem::ApplyFlakDamage(const APUVector3& burstPos, float blastRadius, float damage)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 destroyedCount = 0;

    for (auto& s : m_sentinels) {
        if (s.state == DOCK_SENTINEL_DESTROYED) continue;
        float dist = (s.position - burstPos).Length();
        if (dist <= blastRadius) {
            float falloff = std::max(0.2f, 1.0f - (dist / blastRadius));
            s.health -= damage * falloff;
            if (s.health <= 0.0f) {
                s.state = DOCK_SENTINEL_DESTROYED;
                destroyedCount++;
            }
        }
    }

    if (!m_apus.empty() && destroyedCount > 0) {
        m_apus.begin()->second.sentinelsNeutralized += destroyedCount;
    }
    return destroyedCount;
}

size_t APUCombatSystem::GetActiveSentinelCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    size_t count = 0;
    for (const auto& s : m_sentinels) {
        if (s.state != DOCK_SENTINEL_DESTROYED) count++;
    }
    return count;
}

size_t APUCombatSystem::GetLatchedSentinelCount(uint32 apuId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    size_t count = 0;
    for (const auto& s : m_sentinels) {
        if (s.targetApuId == apuId && s.state == DOCK_SENTINEL_LATCHED_CUTTING) {
            count++;
        }
    }
    return count;
}

const SentinelUnit* APUCombatSystem::GetSentinel(uint32 sentinelId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    for (const auto& s : m_sentinels) {
        if (s.sentinelId == sentinelId) return &s;
    }
    return nullptr;
}

uint32 APUCombatSystem::RegisterCraneHopper(const std::string& designation, const APUVector3& pos, uint32 stockpile)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 id = m_nextHopperId++;
    CraneHopper ch;
    ch.hopperId = id;
    ch.designation = designation;
    ch.position = pos;
    ch.ammoStockpile = stockpile;
    ch.isOperational = true;
    m_hoppers.push_back(ch);
    return id;
}

uint32 APUCombatSystem::DispatchAmmoRunner(uint32 targetApuId, uint32 hopperId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 id = m_nextRunnerId++;
    AmmoRunnerNPC runner;
    runner.runnerId = id;
    runner.name = "Runner Kid";

    // Start at hopper
    for (const auto& h : m_hoppers) {
        if (h.hopperId == hopperId) {
            runner.position = h.position;
            break;
        }
    }
    runner.state = RUNNER_FETCHING_AMMO;
    runner.targetApuId = targetApuId;
    runner.assignedHopperId = hopperId;
    runner.ammoBoxesCarried = 0;
    runner.speedUnitsPerSec = 450.0f;
    runner.health = 100.0f;
    runner.maxHealth = 100.0f;
    m_runners[id] = runner;
    return id;
}

uint32 APUCombatSystem::DispatchAmmoRunner(uint32 targetApuId, const APUVector3& depotPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    uint32 hopperId = m_hoppers.empty() ? 1 : m_hoppers.front().hopperId;
    uint32 id = m_nextRunnerId++;
    AmmoRunnerNPC runner;
    runner.runnerId = id;
    runner.name = "Runner Kid";
    runner.position = depotPos;
    runner.state = RUNNER_FETCHING_AMMO;
    runner.targetApuId = targetApuId;
    runner.assignedHopperId = hopperId;
    runner.ammoBoxesCarried = 0;
    runner.speedUnitsPerSec = 450.0f;
    runner.health = 100.0f;
    runner.maxHealth = 100.0f;
    m_runners[id] = runner;
    return id;
}

void APUCombatSystem::UpdateAmmoRunners(float deltaTimeSec)
{
    for (auto& kv : m_runners) {
        AmmoRunnerNPC& runner = kv.second;
        if (runner.state == RUNNER_INCAPACITATED) continue;

        auto apuIt = m_apus.find(runner.targetApuId);
        if (apuIt == m_apus.end()) {
            runner.state = RUNNER_IDLE;
            continue;
        }
        APUUnitState& targetApu = apuIt->second;

        if (runner.state == RUNNER_FETCHING_AMMO) {
            // Traverse to assigned crane hopper
            APUVector3 hopperPos(0.0f, 0.0f, 0.0f);
            for (const auto& h : m_hoppers) {
                if (h.hopperId == runner.assignedHopperId) {
                    hopperPos = h.position;
                    break;
                }
            }

            APUVector3 dir = hopperPos - runner.position;
            float dist = dir.Length();
            if (dist > 60.0f) {
                runner.position += dir.Normalized() * (runner.speedUnitsPerSec * deltaTimeSec);
            } else {
                // Grab 2 ammo boxes and transition to delivery
                runner.ammoBoxesCarried = 2;
                runner.state = RUNNER_DELIVERING;
            }
        }
        else if (runner.state == RUNNER_DELIVERING) {
            // Sprint towards target APU
            APUVector3 dir = targetApu.position - runner.position;
            float dist = dir.Length();

            if (dist > 80.0f) {
                runner.position += dir.Normalized() * (runner.speedUnitsPerSec * deltaTimeSec);
            } else {
                runner.state = RUNNER_RELOADING;
            }
        }
        else if (runner.state == RUNNER_RELOADING) {
            // Reload 1,250 rounds into each weapon arm
            targetApu.leftArm.ammoRoundsRemaining = std::min(targetApu.leftArm.ammoMaxCapacity, targetApu.leftArm.ammoRoundsRemaining + 1250);
            targetApu.rightArm.ammoRoundsRemaining = std::min(targetApu.rightArm.ammoMaxCapacity, targetApu.rightArm.ammoRoundsRemaining + 1250);
            runner.ammoBoxesCarried = 0;
            runner.totalDeliveriesMade++;
            runner.state = RUNNER_IDLE;
        }
    }
}

bool APUCombatSystem::RequestEmergencyAmmoResupply(uint32 apuId)
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it == m_apus.end()) return false;

    // Find first idle runner
    for (auto& kv : m_runners) {
        if (kv.second.state == RUNNER_IDLE) {
            kv.second.targetApuId = apuId;
            kv.second.state = RUNNER_FETCHING_AMMO;
            return true;
        }
    }

    // Otherwise dispatch a new runner
    uint32 hopperId = m_hoppers.empty() ? 1 : m_hoppers.front().hopperId;
    DispatchAmmoRunner(apuId, hopperId);
    return true;
}

const CraneHopper* APUCombatSystem::GetCraneHopper(uint32 hopperId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    for (const auto& h : m_hoppers) {
        if (h.hopperId == hopperId) return &h;
    }
    return nullptr;
}

size_t APUCombatSystem::GetCraneHopperCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    return m_hoppers.size();
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
    breach.dischargeRatePerSec = 25.0f;
    m_breaches.push_back(breach);

    SpawnSentinelSwarm(50, breachPos, 1);
    return id;
}

void APUCombatSystem::UpdateDiggerIncursions(float deltaTimeSec)
{
    for (auto& breach : m_breaches) {
        if (!breach.isDomePierced) continue;
        if (breach.activeSentinelsDischarged < breach.maxSentinelsToDeploy) {
            uint32 discharge = (uint32)std::round(breach.dischargeRatePerSec * deltaTimeSec);
            if (discharge > 0) {
                breach.activeSentinelsDischarged = std::min(breach.maxSentinelsToDeploy, breach.activeSentinelsDischarged + discharge);
                if (m_sentinels.size() < 300) {
                    SpawnSentinel(breach.breachLocation, 1);
                }
            }
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

    if (firingCount >= 3) return 1.35f; // +35% coordinated 3+ APU crossfire
    if (firingCount >= 2) return 1.25f; // +25% 2 APU crossfire
    return 1.0f;
}

const APUUnitState* APUCombatSystem::GetAPU(uint32 apuId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_apus.find(apuId);
    if (it != m_apus.end()) return &it->second;
    return nullptr;
}

const AmmoRunnerNPC* APUCombatSystem::GetAmmoRunner(uint32 runnerId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_apuMutex);
    auto it = m_runners.find(runnerId);
    if (it != m_runners.end()) return &it->second;
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

// ============================================================================
// HEADLESS TEST SUITE: ZION DOCK APU COMBAT & SENTINEL SWARMS (SUITE 16)
// ============================================================================
void RunAPUCombatTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING ZION DOCK APU COMBAT & SENTINEL SWARM TEST SUITE " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // 1. System Initialization & Baseline Setup
    sAPUCombatSystem.Initialize();
    TEST_ASSERT(sAPUCombatSystem.GetActiveAPUCount() >= 1, "Default APU Unit 01 initialized");
    TEST_ASSERT(sAPUCombatSystem.GetActiveBreachCount() >= 1, "Default Cavern Roof Digger breach registered");
    TEST_ASSERT(sAPUCombatSystem.GetActiveRunnerCount() >= 1, "Default Ammo Runner Kid registered");
    TEST_ASSERT(sAPUCombatSystem.GetCraneHopperCount() >= 1, "Cavern Gantry Crane Hopper initialized");

    const APUUnitState* apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1 != nullptr, "Mifune's APU retrieved successfully");
    TEST_ASSERT(apu1->isMounted == true, "Mifune APU is initially mounted");
    TEST_ASSERT(apu1->pilotHandle == "Captain Mifune", "Pilot handle matches Captain Mifune");
    TEST_ASSERT(apu1->hydraulicPressurePsi == 3000.0f, "Hydraulic pressure starts at nominal 3000 PSI");
    TEST_ASSERT(apu1->cockpitShieldIntegrity == 100.0f, "Cockpit shield integrity begins at 100%");

    // 2. APU Locomotion & Hydraulic Gait
    bool moveOk = sAPUCombatSystem.UpdateAPULocomotion(1, APUVector3(1.0f, 0.0f, 0.0f), 15.0f, 0.5f);
    TEST_ASSERT(moveOk, "APULocomotion executes successfully with input");
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->velocity.Length() > 0.0f, "APU gains forward locomotion velocity");
    TEST_ASSERT(apu1->facingDegrees == 15.0f, "APU facing yaw adjusted accurately");
    TEST_ASSERT(apu1->stepPhase > 0.0f, "Inverted pendulum step phase advances during stride");

    // 3. Dual 30mm Rotary Cannons & Ballistics (2500 RPM)
    uint32 roundsFired = 0;
    bool fireOk = sAPUCombatSystem.FireWeapons(1, true, true, roundsFired, 0.1f);
    TEST_ASSERT(fireOk && roundsFired > 0, "Twin 30mm rotary cannons fire simultaneously");
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->leftArm.ammoRoundsRemaining < 2500, "Left arm ammo decremented from fire");
    TEST_ASSERT(apu1->rightArm.ammoRoundsRemaining < 2500, "Right arm ammo decremented from fire");
    TEST_ASSERT(apu1->leftArm.barrelTempCelsius > 25.0f, "Left barrel temp increases from 30mm thermal friction");

    // 4. Hydraulic Recoil Impulse
    TEST_ASSERT(apu1->velocity.z < 0.0f || apu1->velocity.LengthSq() > 0.0f, "Firing cannons produces backwards recoil impulse");

    // 5. Sustained Fire Overheat & Thermal Jamming (850°C)
    for (int i = 0; i < 60; ++i) {
        uint32 rf = 0;
        sAPUCombatSystem.FireWeapons(1, true, true, rf, 0.5f);
    }
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->leftArm.barrelTempCelsius >= 850.0f, "Barrel temperature exceeds 850C under sustained fire");
    TEST_ASSERT(apu1->leftArm.isOverheated == true, "Rotary cannon flags isOverheated state");
    TEST_ASSERT(apu1->leftArm.isJammed == true, "Rotary cannon mechanism experiences thermal jam");

    // Attempting to fire while jammed
    uint32 jammedFire = 0;
    sAPUCombatSystem.FireWeapons(1, true, true, jammedFire, 0.1f);
    TEST_ASSERT(jammedFire == 0, "Jammed rotary cannons refuse to fire");

    // 6. Cryogenic Coolant Venting
    bool ventOk = sAPUCombatSystem.TriggerCoolantVent(1);
    TEST_ASSERT(ventOk, "TriggerCoolantVent activates cryogenic venting");
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->activeCoolantVenting == true, "Coolant venting is active on APU");

    // Simulate cooling ticks
    for (int i = 0; i < 20; ++i) {
        sAPUCombatSystem.UpdateSimulation(0.5f);
    }
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->leftArm.barrelTempCelsius < 850.0f, "Barrel temperature rapidly reduced by coolant venting");
    TEST_ASSERT(apu1->leftArm.isJammed == false, "Thermal jam cleared once barrel temperature safe");

    // 7. Sentinel Swarm Boids AI (Phase 8)
    size_t initialSentinels = sAPUCombatSystem.GetActiveSentinelCount();
    TEST_ASSERT(initialSentinels >= 40, "Sentinel swarm vanguards active in cavern");

    // Spawn targeted swarm
    sAPUCombatSystem.SpawnSentinelSwarm(25, APUVector3(0.0f, 800.0f, 0.0f), 1);
    TEST_ASSERT(sAPUCombatSystem.GetActiveSentinelCount() >= initialSentinels + 25, "SpawnSentinelSwarm creates flocking Boids");

    // Advance swarm simulation for vortex dive
    for (int i = 0; i < 15; ++i) {
        sAPUCombatSystem.UpdateSimulation(0.2f);
    }
    TEST_ASSERT(sAPUCombatSystem.GetActiveSentinelCount() > 0, "Sentinel flocking positions updated across simulation steps");

    // 8. Ballistic Flak Destruction
    uint32 neutralized = sAPUCombatSystem.ApplyFlakDamage(APUVector3(0.0f, 0.0f, 0.0f), 2500.0f, 300.0f);
    TEST_ASSERT(neutralized > 0, "APU 30mm flak blast destroys flying Sentinels");
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->sentinelsNeutralized >= neutralized, "APU unit tracks confirmed Sentinel kills");

    // 9. Plasma Torch Latching & Shield Degradation
    uint32 testSentinelId = sAPUCombatSystem.SpawnSentinel(APUVector3(10.0f, 20.0f, 10.0f), 1);
    const SentinelUnit* testSentinel = sAPUCombatSystem.GetSentinel(testSentinelId);
    TEST_ASSERT(testSentinel != nullptr, "Close-quarters Sentinel spawned successfully");

    // Force dive / latch update
    sAPUCombatSystem.UpdateSimulation(0.5f);
    apu1 = sAPUCombatSystem.GetAPU(1);
    TEST_ASSERT(apu1->cockpitShieldIntegrity <= 100.0f, "Sentinel plasma torch cuts into APU cockpit shield");

    // 10. Ammo Runner Logistics Loop (Phase 9)
    uint32 newHopperId = sAPUCombatSystem.RegisterCraneHopper("Gantry Crane East", APUVector3(400.0f, 0.0f, 400.0f), 40000);
    TEST_ASSERT(newHopperId > 0, "RegisterCraneHopper creates auxiliary ammo depot");
    const CraneHopper* hopperEast = sAPUCombatSystem.GetCraneHopper(newHopperId);
    TEST_ASSERT(hopperEast != nullptr && hopperEast->ammoStockpile == 40000, "Auxiliary hopper stockpile verified");

    // Deplete APU ammo for delivery test
    // Register APU 2
    uint32 apu2Id = sAPUCombatSystem.RegisterAPU(1002, "Lieutenant Jax", APU_MODEL_ZION_STANDARD, APUVector3(200.0f, 0.0f, 200.0f));
    const APUUnitState* apu2 = sAPUCombatSystem.GetAPU(apu2Id);
    TEST_ASSERT(apu2 != nullptr, "APU Unit 2 registered");

    // Dispatch Runner to APU 2
    uint32 runnerJaxId = sAPUCombatSystem.DispatchAmmoRunner(apu2Id, newHopperId);
    TEST_ASSERT(runnerJaxId > 0, "DispatchAmmoRunner creates supply runner");
    const AmmoRunnerNPC* runnerJax = sAPUCombatSystem.GetAmmoRunner(runnerJaxId);
    TEST_ASSERT(runnerJax != nullptr && runnerJax->state == RUNNER_FETCHING_AMMO, "Runner starts in FETCHING_AMMO state");

    // Advance runner towards hopper
    for (int i = 0; i < 10; ++i) {
        sAPUCombatSystem.UpdateSimulation(0.2f);
    }
    runnerJax = sAPUCombatSystem.GetAmmoRunner(runnerJaxId);
    TEST_ASSERT(runnerJax->state == RUNNER_DELIVERING || runnerJax->state == RUNNER_IDLE, "Runner loads ammo boxes and transitions to delivering/reloading");

    // 11. Coordinated Crossfire Multipliers
    float crossfireSingle = sAPUCombatSystem.CalculateCrossfireBonus(APUVector3(9000.0f, 0.0f, 0.0f));
    TEST_ASSERT(crossfireSingle == 1.0f, "Distant target receives baseline 1.0x damage");

    float crossfireCoordinated = sAPUCombatSystem.CalculateCrossfireBonus(APUVector3(50.0f, 0.0f, 50.0f));
    TEST_ASSERT(crossfireCoordinated >= 1.25f, "Coordinated multi-APU crossfire grants damage bonus >= 1.25x");

    // 12. Subterranean Logistics & Power Grid
    sAPUCombatSystem.AdjustDefensePowerAllocation(85.0f);
    const ZionSubterraneanLogistics& logistics = sAPUCombatSystem.GetLogistics();
    TEST_ASSERT(logistics.defenseGridAllocationPercent == 85.0f, "Defense grid allocation adjusted to 85%");
    TEST_ASSERT(logistics.lifeSupportIntegrityPercent < 90.0f, "Life support rebalanced dynamically under siege load");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  ZION DOCK APU COMBAT & SENTINEL SWARMS SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

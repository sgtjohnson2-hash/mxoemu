#include "FreewayCombatSystem.h"
#include "GameServer.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "AgentPossessionManager.h"
#include "MessageTypes.h"
#include "Log.h"
#include <algorithm>
#include <iostream>

static inline bool Has3DWorldSupport() {
    return GameServer::getSingletonPtr() != nullptr && BotManager::getSingletonPtr() != nullptr;
}

createFileSingleton(FreewayCombatSystem);

FreewayCombatSystem::FreewayCombatSystem()
{
    Initialize();
}

void FreewayCombatSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    m_simTimeSec = 0.0f;
    m_vehicles.clear();
    m_duels.clear();
    m_twins.clear();
    m_nextVehicleId = 1;
    m_nextDuelId = 1;

    // Default 101 Freeway Loop Traffic
    SpawnVehicle(VEHICLE_CADILLAC_CTS, LANE_MIDDLE, 80.0f);     // Escort sedan
    SpawnVehicle(VEHICLE_DUCATI_996, LANE_HOV, 95.0f);         // Trinity bike
    SpawnVehicle(VEHICLE_SEMI_TRAILER, LANE_OUTER, 65.0f);     // Freight truck
    SpawnVehicle(VEHICLE_POLICE_CRUISER, LANE_INNER, 85.0f);   // Chaser

    // Initialize Agent Twins
    AgentTwinAI twin1;
    twin1.twinId = 1;
    twin1.phaseState = TWIN_SOLID;
    twin1.currentTargetVehicleId = 1;
    twin1.isInvulnerable = false;
    m_twins[1] = twin1;

    AgentTwinAI twin2;
    twin2.twinId = 2;
    twin2.phaseState = TWIN_SOLID;
    twin2.currentTargetVehicleId = 1;
    twin2.isInvulnerable = false;
    m_twins[2] = twin2;

    // Reset Escort State
    m_escort.escortVehicleId = 1;
    m_escort.escortHealth = 5000.0f;
    m_escort.progressPercent = 0.0f;
    m_escort.isAmbushed = false;
    m_escort.isEscortSuccessful = false;
}

void FreewayCombatSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Simulate Freeway Vehicles along the 50,000 unit circular loop
    for (auto& pair : m_vehicles)
    {
        auto& v = pair.second;
        if (v.isWrecked) continue;

        if (v.isTireBlown)
        {
            v.speedMph = std::max(10.0f, v.speedMph - 15.0f * deltaTimeSec);
        }

        // Convert mph to units/sec (~1.46667 feet/sec scale)
        float speedUnitsPerSec = v.speedMph * 22.0f;
        v.position.z += speedUnitsPerSec * deltaTimeSec;

        // Loop continuous 101 highway
        if (v.position.z > 50000.0f)
        {
            v.position.z -= 50000.0f;
        }
    }

    // Simulate Rooftop Wire-Fu Duels
    for (auto& pair : m_duels)
    {
        auto& d = pair.second;
        // Natural wind turbulence penalty
        float windDecay = (d.windResistanceForce / 50.0f) * deltaTimeSec;
        d.p1BalanceMeter = std::max(0.0f, d.p1BalanceMeter - windDecay);
        d.p2BalanceMeter = std::max(0.0f, d.p2BalanceMeter - windDecay);
    }

    // Keymaker Escort Progress
    if (!m_escort.isEscortSuccessful && m_escort.escortHealth > 0.0f)
    {
        m_escort.progressPercent += 0.8f * deltaTimeSec;
        if (m_escort.progressPercent >= 100.0f)
        {
            m_escort.progressPercent = 100.0f;
            m_escort.isEscortSuccessful = true;
        }
    }
}

uint32 FreewayCombatSystem::SpawnVehicle(VehicleType type, VehicleLane lane, float initialSpeedMph)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    FreewayVehicle v;
    v.vehicleId = m_nextVehicleId++;
    v.type = type;
    v.lane = lane;
    v.speedMph = initialSpeedMph;

    switch (lane)
    {
        case LANE_HOV:    v.position.x = -300.0f; break;
        case LANE_INNER:  v.position.x = -100.0f; break;
        case LANE_MIDDLE: v.position.x =  100.0f; break;
        case LANE_OUTER:  v.position.x =  300.0f; break;
    }

    if (type == VEHICLE_SEMI_TRAILER)
    {
        v.health = 10000.0f;
        v.maxHealth = 10000.0f;
    }
    else if (type == VEHICLE_DUCATI_996)
    {
        v.health = 800.0f;
        v.maxHealth = 800.0f;
    }
    else
    {
        v.health = 2500.0f;
        v.maxHealth = 2500.0f;
    }

    m_vehicles[v.vehicleId] = v;
    return v.vehicleId;
}

bool FreewayCombatSystem::TriggerTireBlowout(uint32 vehicleId)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_vehicles.find(vehicleId);
    if (it == m_vehicles.end() || it->second.isWrecked) return false;

    it->second.isTireBlown = true;
    return true;
}

bool FreewayCombatSystem::DamageVehicle(uint32 vehicleId, float damage)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_vehicles.find(vehicleId);
    if (it == m_vehicles.end()) return false;

    it->second.health = std::max(0.0f, it->second.health - damage);
    if (it->second.health <= 0.0f)
    {
        it->second.isWrecked = true;
        it->second.speedMph = 0.0f;
    }
    return true;
}

uint32 FreewayCombatSystem::BoardVehicleRoof(uint32 vehicleId, uint32 playerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_vehicles.find(vehicleId);
    if (it == m_vehicles.end() || it->second.isWrecked) return 0;

    it->second.hasRoofCombatant = true;

    if (it->second.roofDuelId != 0 && m_duels.find(it->second.roofDuelId) != m_duels.end())
    {
        auto& duel = m_duels[it->second.roofDuelId];
        duel.participant2Id = playerId;
        duel.p2BalanceMeter = 100.0f;
        return duel.duelId;
    }

    RoofWireFuDuel duel;
    duel.duelId = m_nextDuelId++;
    duel.vehicleId = vehicleId;
    duel.participant1Id = playerId;
    duel.p1BalanceMeter = 100.0f;
    duel.p2BalanceMeter = 100.0f;
    duel.windResistanceForce = (it->second.speedMph / 75.0f) * 120.0f;

    m_duels[duel.duelId] = duel;
    it->second.roofDuelId = duel.duelId;
    return duel.duelId;
}

bool FreewayCombatSystem::UpdateRoofDuel(uint32 duelId, uint32 playerId, float balanceDelta, bool airborneJump)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_duels.find(duelId);
    if (it == m_duels.end()) return false;

    auto& duel = it->second;
    if (duel.participant1Id == playerId)
    {
        duel.p1BalanceMeter = std::clamp(duel.p1BalanceMeter + balanceDelta, 0.0f, 100.0f);
        duel.isP1Airborne = airborneJump;
        return true;
    }
    else if (duel.participant2Id == playerId)
    {
        duel.p2BalanceMeter = std::clamp(duel.p2BalanceMeter + balanceDelta, 0.0f, 100.0f);
        duel.isP2Airborne = airborneJump;
        return true;
    }
    return false;
}

bool FreewayCombatSystem::TriggerTwinPhaseShift(uint32 twinId, bool phaseOn)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_twins.find(twinId);
    if (it == m_twins.end()) return false;

    it->second.phaseState = phaseOn ? TWIN_PHASING : TWIN_SOLID;
    it->second.isInvulnerable = phaseOn;
    return true;
}

void FreewayCombatSystem::StartKeymakerEscort()
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    m_escort.escortVehicleId = 1;
    m_escort.escortHealth = 5000.0f;
    m_escort.progressPercent = 0.0f;
    m_escort.isAmbushed = true;
    m_escort.isEscortSuccessful = false;
}

bool FreewayCombatSystem::DamageEscortSedan(float damage)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    m_escort.escortHealth = std::max(0.0f, m_escort.escortHealth - damage);
    if (m_escort.escortHealth <= 0.0f)
    {
        m_escort.isEscortSuccessful = false;
        return true; // Destroyed
    }
    return false;
}

size_t FreewayCombatSystem::GetVehicleCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    return m_vehicles.size();
}

size_t FreewayCombatSystem::GetActiveRoofDuelCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    return m_duels.size();
}

bool FreewayCombatSystem::IsTwinPhased(uint32 twinId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_twins.find(twinId);
    if (it != m_twins.end())
    {
        return it->second.phaseState == TWIN_PHASING;
    }
    return false;
}

bool FreewayCombatSystem::GetVehicle(uint32 vehicleId, FreewayVehicle& outVehicle) const
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_vehicles.find(vehicleId);
    if (it != m_vehicles.end())
    {
        outVehicle = it->second;
        return true;
    }
    return false;
}

bool FreewayCombatSystem::ExecuteVehicleRam(uint32 attackerVehicleId, uint32 targetVehicleId)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto itAttacker = m_vehicles.find(attackerVehicleId);
    auto itTarget = m_vehicles.find(targetVehicleId);
    if (itAttacker == m_vehicles.end() || itTarget == m_vehicles.end()) return false;
    if (itAttacker->second.isWrecked || itTarget->second.isWrecked) return false;

    FreewayVehicle& attacker = itAttacker->second;
    FreewayVehicle& target = itTarget->second;

    float relSpeed = std::abs(attacker.speedMph - target.speedMph);
    float impactDamage = std::max(250.0f, relSpeed * 15.0f);

    if (attacker.type == VEHICLE_SEMI_TRAILER) {
        impactDamage *= 2.5f;
    }

    target.health = std::max(0.0f, target.health - impactDamage);
    if (target.health <= 0.0f) {
        target.isWrecked = true;
        target.speedMph = 0.0f;
    } else if (relSpeed > 20.0f) {
        target.isTireBlown = true;
    }

    attacker.health = std::max(0.0f, attacker.health - (impactDamage * 0.35f));
    if (attacker.health <= 0.0f) {
        attacker.isWrecked = true;
        attacker.speedMph = 0.0f;
    }

    if (Has3DWorldSupport()) {
        DEBUG_LOG(format("FreewayCombatSystem: Vehicle %1% rammed %2% for %3% damage!")
                  % attackerVehicleId % targetVehicleId % impactDamage);
    }

    return true;
}

bool FreewayCombatSystem::HandleRooftopFalloff(uint32 duelId, uint32 participantId)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_duels.find(duelId);
    if (it == m_duels.end()) return false;

    RoofWireFuDuel& duel = it->second;
    if (duel.participant1Id != participantId && duel.participant2Id != participantId) return false;

    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(participantId)) {
            po->takeDamage(0, 1500, 0x280001C2);
            po->sayChat("Lost footing! Falling onto the 101 tarmac!");
            po->Emote(50);
        }
    }

    if (duel.participant1Id == participantId) {
        duel.p1BalanceMeter = 0.0f;
    } else {
        duel.p2BalanceMeter = 0.0f;
    }

    if (duel.p1BalanceMeter <= 0.0f && duel.p2BalanceMeter <= 0.0f) {
        auto vIt = m_vehicles.find(duel.vehicleId);
        if (vIt != m_vehicles.end()) {
            vIt->second.hasRoofCombatant = false;
            vIt->second.roofDuelId = 0;
        }
        m_duels.erase(it);
    }

    return true;
}

bool FreewayCombatSystem::AgentJumpOntoVehicle(uint32 vehicleId, const std::string& agentName)
{
    std::lock_guard<std::recursive_mutex> lock(m_freewayMutex);
    auto it = m_vehicles.find(vehicleId);
    if (it == m_vehicles.end() || it->second.isWrecked) return false;

    FreewayVehicle& v = it->second;
    v.hasRoofCombatant = true;
    v.speedMph += 20.0f;

    if (Has3DWorldSupport()) {
        DEBUG_LOG(format("FreewayCombatSystem: %1% landed on hood of vehicle %2%!")
                  % agentName % vehicleId);
    }

    return true;
}

void RunFreewayCombatTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING MATRIX RELOADED 101 FREEWAY PURSUIT TEST SUITE   " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    FreewayCombatSystem& freeway = sFreewayCombatSystem;
    freeway.Initialize();

    int passedCount = 0;
    int failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passedCount++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failedCount++;
        }
    };

    // 1. Highway Loop Traffic Spawning & Initial State
    {
        size_t initialCount = freeway.GetVehicleCount();
        TEST_ASSERT(initialCount >= 4, "Default 101 Freeway loop spawns Cadillac, Ducati, Semi, and Cruiser");

        FreewayVehicle semi;
        bool foundSemi = freeway.GetVehicle(3, semi);
        TEST_ASSERT(foundSemi && semi.type == VEHICLE_SEMI_TRAILER, "Semi-Trailer freight truck registered");
        TEST_ASSERT(semi.health == 10000.0f, "Semi-Trailer has 10,000 HP heavy armor");
        TEST_ASSERT(semi.lane == LANE_OUTER, "Semi-Trailer drives in outer freight lane");

        FreewayVehicle ducati;
        bool foundDucati = freeway.GetVehicle(2, ducati);
        TEST_ASSERT(foundDucati && ducati.type == VEHICLE_DUCATI_996, "Trinity's Ducati 996 motorcycle registered");
        TEST_ASSERT(ducati.lane == LANE_HOV, "Ducati utilizes high-speed HOV lane");
    }

    // 2. Tire Blowout & Top Speed Penalty
    {
        FreewayVehicle caddy;
        freeway.GetVehicle(1, caddy);
        float initSpeed = caddy.speedMph;

        bool blowout = freeway.TriggerTireBlowout(1);
        TEST_ASSERT(blowout, "TriggerTireBlowout punctures Cadillac tire");

        freeway.UpdateSimulation(2.0f);
        freeway.GetVehicle(1, caddy);
        TEST_ASSERT(caddy.isTireBlown, "Vehicle tracks blown tire status");
        TEST_ASSERT(caddy.speedMph < initSpeed, "Blown tire causes decelerative friction drag");
    }

    // 3. High-Speed Vehicle Ramming Mechanics
    {
        uint32 chaserId = freeway.SpawnVehicle(VEHICLE_POLICE_CRUISER, LANE_INNER, 90.0f);
        uint32 targetId = freeway.SpawnVehicle(VEHICLE_CADILLAC_CTS, LANE_INNER, 65.0f);

        FreewayVehicle targetBefore;
        freeway.GetVehicle(targetId, targetBefore);

        bool ramResult = freeway.ExecuteVehicleRam(chaserId, targetId);
        TEST_ASSERT(ramResult, "ExecuteVehicleRam executes high-speed highway collision");

        FreewayVehicle targetAfter;
        freeway.GetVehicle(targetId, targetAfter);
        TEST_ASSERT(targetAfter.health < targetBefore.health, "Ramming collision inflicts damage scaled to relative momentum");
        TEST_ASSERT(targetAfter.isTireBlown, "High relative-speed collision triggers tire blowout");

        // Semi-trailer heavy ramming bonus
        bool semiRam = freeway.ExecuteVehicleRam(3, targetId);
        TEST_ASSERT(semiRam, "Semi-Trailer crushes lighter vehicle");
    }

    // 4. Rooftop Wire-Fu Duels on Speeding Freight
    {
        uint32 morpheusGoId = 9101;
        uint32 agentJohnsonGoId = 9102;

        uint32 duelId = freeway.BoardVehicleRoof(3, morpheusGoId);
        TEST_ASSERT(duelId != 0, "BoardVehicleRoof initiates rooftop duel on Semi-Trailer");

        uint32 joinedDuelId = freeway.BoardVehicleRoof(3, agentJohnsonGoId);
        TEST_ASSERT(joinedDuelId == duelId, "Second combatant boards same roof duel");
        TEST_ASSERT(freeway.GetActiveRoofDuelCount() >= 1, "Active roof duel count tracked");

        bool balanceUpdate = freeway.UpdateRoofDuel(duelId, morpheusGoId, -10.0f, true);
        TEST_ASSERT(balanceUpdate, "UpdateRoofDuel adjusts balance and airborne wire-fu jump state");
    }

    // 5. Rooftop Falloff Trauma Mechanics
    {
        bool falloffResult = freeway.HandleRooftopFalloff(1, 9102);
        TEST_ASSERT(falloffResult, "HandleRooftopFalloff triggers fall damage and balance collapse");
    }

    // 6. Agent Twins Phasing Mechanics
    {
        TEST_ASSERT(!freeway.IsTwinPhased(1), "Twin starts in solid physical state");

        bool phased = freeway.TriggerTwinPhaseShift(1, true);
        TEST_ASSERT(phased && freeway.IsTwinPhased(1), "Twin phase shift transitions to ethereal TWIN_PHASING state");

        bool solidified = freeway.TriggerTwinPhaseShift(1, false);
        TEST_ASSERT(solidified && !freeway.IsTwinPhased(1), "Twin solidifies to take physical damage");
    }

    // 7. Keymaker Escort Mission & Ambush Escalation
    {
        freeway.StartKeymakerEscort();
        const KeymakerEscortState& escort = freeway.GetEscortState();
        TEST_ASSERT(escort.isAmbushed, "StartKeymakerEscort flags mission as ambushed");
        TEST_ASSERT(escort.escortHealth == 5000.0f, "Escort sedan has 5,000 baseline durability");

        bool damaged = freeway.DamageEscortSedan(500.0f);
        TEST_ASSERT(!damaged, "Escort sedan survives partial ambush damage");
        TEST_ASSERT(freeway.GetEscortState().escortHealth == 4500.0f, "Escort sedan health accurately decremented");

        freeway.UpdateSimulation(5.0f);
        TEST_ASSERT(freeway.GetEscortState().progressPercent > 0.0f, "Escort vehicle advances along 101 Freeway loop");
    }

    // 8. Agent Hood Jump & Overwrite
    {
        bool agentJump = freeway.AgentJumpOntoVehicle(4, "Agent Jackson");
        TEST_ASSERT(agentJump, "AgentJumpOntoVehicle executes hood latch and ramming speed boost");
        FreewayVehicle cruiser;
        freeway.GetVehicle(4, cruiser);
        TEST_ASSERT(cruiser.hasRoofCombatant, "Vehicle tracks roof combatant presence");
        TEST_ASSERT(cruiser.speedMph >= 100.0f, "Vehicle gains ramming speed acceleration");
    }

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  101 FREEWAY PURSUIT & ROOFTOP DUELS TEST SUITE COMPLETE   " << std::endl;
    std::cout << "  PASSED: " << passedCount << " | FAILED: " << failedCount << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failedCount > 0) {
        std::cerr << "Freeway Combat test suite encountered failures!" << std::endl;
        exit(1);
    }
}


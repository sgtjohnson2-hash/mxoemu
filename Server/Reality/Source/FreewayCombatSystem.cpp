#include "FreewayCombatSystem.h"
#include "Log.h"
#include <algorithm>

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

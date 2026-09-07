#include "HovercraftFlightSystem.h"
#include "Log.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cassert>

createFileSingleton(HovercraftFlightSystem);

HovercraftFlightSystem::HovercraftFlightSystem()
{
    Initialize();
}

HovercraftFlightSystem::~HovercraftFlightSystem()
{
}

void HovercraftFlightSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    m_ships.clear();
    m_sentinels.clear();
    m_corridors.clear();
    m_nextShipId = 1;
    m_nextDroneId = 1;
    m_simulationTimeSec = 0.0f;

    // Reset EMP System
    m_emp = EMPShockwaveSystem();

    // Register Default Broadcast Corridors
    RegisterBroadcastCorridor(1, "Sewer Main Line 04 (Zion Pipeline)", FlightVector3(0.0f, 400.0f, -40000.0f), FlightVector3(0.0f, 400.0f, 40000.0f), 104.7f);
    RegisterBroadcastCorridor(2, "Sub-Level 12 Emergency Relay", FlightVector3(-10000.0f, 250.0f, -20000.0f), FlightVector3(10000.0f, 250.0f, 20000.0f), 88.5f);
    RegisterBroadcastCorridor(3, "Machine Boundary Outpost Uplink", FlightVector3(15000.0f, 600.0f, -10000.0f), FlightVector3(15000.0f, 600.0f, 30000.0f), 142.1f);

    if (Log::getSingletonPtr())
    {
        sLog.outString("[HovercraftFlightSystem] Initialized 6-DOF flight engine with %zu broadcast corridors.", m_corridors.size());
    }
}

void HovercraftFlightSystem::Reset()
{
    Initialize();
}

uint32 HovercraftFlightSystem::SpawnHovercraft(HovercraftType type, const FlightVector3& spawnPos, const std::string& customName)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    uint32 id = m_nextShipId++;

    HovercraftPhysicsState state;
    state.shipId = id;
    state.type = type;
    state.position = spawnPos;
    state.velocity = FlightVector3(0.0f, 0.0f, 0.0f);
    state.acceleration = FlightVector3(0.0f, 0.0f, 0.0f);

    switch (type)
    {
        case HOVERCRAFT_NEBUCHADNEZZAR:
            state.name = customName.empty() ? "Nebuchadnezzar" : customName;
            state.mass = 125000.0f;
            state.maxSpeed = 2800.0f;
            state.maxHull = 1500.0f;
            state.hullIntegrity = 1500.0f;
            state.acousticSignature = 0.50f;
            state.thermalSignature = 0.55f;
            break;
        case HOVERCRAFT_LOGOS:
            state.name = customName.empty() ? "Logos" : customName;
            state.mass = 85000.0f;
            state.maxSpeed = 3800.0f;
            state.maxHull = 900.0f;
            state.hullIntegrity = 900.0f;
            state.acousticSignature = 0.35f;
            state.thermalSignature = 0.40f;
            break;
        case HOVERCRAFT_MJOLNIR:
            state.name = customName.empty() ? "Mjolnir" : customName;
            state.mass = 180000.0f;
            state.maxSpeed = 2400.0f;
            state.maxHull = 2200.0f;
            state.hullIntegrity = 2200.0f;
            state.acousticSignature = 0.70f;
            state.thermalSignature = 0.75f;
            break;
        case HOVERCRAFT_VIGILANT:
            state.name = customName.empty() ? "Vigilant" : customName;
            state.mass = 95000.0f;
            state.maxSpeed = 3200.0f;
            state.maxHull = 1100.0f;
            state.hullIntegrity = 1100.0f;
            state.acousticSignature = 0.25f;
            state.thermalSignature = 0.30f;
            break;
    }

    m_ships[id] = state;
    sLog.outString("[HovercraftFlightSystem] Spawned hovercraft '%s' (ID %u) at (%.1f, %.1f, %.1f)",
                   state.name.c_str(), id, spawnPos.x, spawnPos.y, spawnPos.z);
    return id;
}

HovercraftPhysicsState* HovercraftFlightSystem::GetHovercraft(uint32 shipId)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    auto it = m_ships.find(shipId);
    if (it != m_ships.end())
        return &it->second;
    return nullptr;
}

bool HovercraftFlightSystem::ApplyThrustInput(uint32 shipId, float forwardThrust, float strafeThrust, float verticalThrust,
                                              float pitchInput, float rollInput, float yawInput)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    auto it = m_ships.find(shipId);
    if (it == m_ships.end() || !it->second.enginesActive)
        return false;

    auto& ship = it->second;

    // Convert orientation angles from degrees to radians
    float yawRad = ship.orientation.yaw * (3.14159265f / 180.0f);
    float pitchRad = ship.orientation.pitch * (3.14159265f / 180.0f);

    // Forward direction vector
    FlightVector3 forwardDir(
        std::sin(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::cos(yawRad) * std::cos(pitchRad)
    );

    // Strafe right direction vector
    FlightVector3 rightDir(
        std::cos(yawRad),
        0.0f,
        -std::sin(yawRad)
    );

    // Up vector
    FlightVector3 upDir(0.0f, 1.0f, 0.0f);

    // Compute net thrust force
    FlightVector3 thrustForce = (forwardDir * forwardThrust) + (rightDir * strafeThrust) + (upDir * verticalThrust);

    // Linear acceleration (F = m * a)
    ship.acceleration = thrustForce / (ship.mass * 0.001f);

    // Angular acceleration
    ship.angularVelocity.pitch += pitchInput * 15.0f;
    ship.angularVelocity.roll += rollInput * 20.0f;
    ship.angularVelocity.yaw += yawInput * 25.0f;

    return true;
}

void HovercraftFlightSystem::SetCavernBounds(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    m_cavernMinX = minX;
    m_cavernMaxX = maxX;
    m_cavernMinY = minY;
    m_cavernMaxY = maxY;
    m_cavernMinZ = minZ;
    m_cavernMaxZ = maxZ;
}

bool HovercraftFlightSystem::CheckCavernCollision(uint32 shipId, float& outDamageDealt)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    outDamageDealt = 0.0f;
    auto it = m_ships.find(shipId);
    if (it == m_ships.end()) return false;

    auto& ship = it->second;
    bool collided = false;

    if (ship.position.x < m_cavernMinX)
    {
        ship.position.x = m_cavernMinX;
        ship.velocity.x = -ship.velocity.x * 0.5f;
        outDamageDealt += std::abs(ship.velocity.x) * 0.08f;
        collided = true;
    }
    else if (ship.position.x > m_cavernMaxX)
    {
        ship.position.x = m_cavernMaxX;
        ship.velocity.x = -ship.velocity.x * 0.5f;
        outDamageDealt += std::abs(ship.velocity.x) * 0.08f;
        collided = true;
    }

    if (ship.position.y < m_cavernMinY + 50.0f)
    {
        ship.position.y = m_cavernMinY + 50.0f;
        ship.velocity.y = std::max(0.0f, -ship.velocity.y * 0.4f);
        outDamageDealt += std::abs(ship.velocity.y) * 0.10f;
        collided = true;
    }
    else if (ship.position.y > m_cavernMaxY)
    {
        ship.position.y = m_cavernMaxY;
        ship.velocity.y = -ship.velocity.y * 0.5f;
        outDamageDealt += std::abs(ship.velocity.y) * 0.08f;
        collided = true;
    }

    if (ship.position.z < m_cavernMinZ)
    {
        ship.position.z = m_cavernMinZ;
        ship.velocity.z = -ship.velocity.z * 0.5f;
        outDamageDealt += std::abs(ship.velocity.z) * 0.08f;
        collided = true;
    }
    else if (ship.position.z > m_cavernMaxZ)
    {
        ship.position.z = m_cavernMaxZ;
        ship.velocity.z = -ship.velocity.z * 0.5f;
        outDamageDealt += std::abs(ship.velocity.z) * 0.08f;
        collided = true;
    }

    if (outDamageDealt > 0.0f)
    {
        ship.hullIntegrity = std::max(0.0f, ship.hullIntegrity - outDamageDealt);
    }

    return collided;
}

FlightVector3 HovercraftFlightSystem::ComputeDraftTurbulence(const FlightVector3& pos, float timeSec)
{
    float tx = std::sin(pos.z * 0.0002f + timeSec * 1.5f) * 45.0f;
    float ty = std::cos(pos.x * 0.0003f + timeSec * 2.0f) * 30.0f;
    float tz = std::sin((pos.x + pos.y) * 0.0001f + timeSec) * 25.0f;
    return FlightVector3(tx, ty, tz);
}

void HovercraftFlightSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    if (deltaTimeSec <= 0.0f) return;

    m_simulationTimeSec += deltaTimeSec;

    // 1. Update Hovercraft 6-DOF Physics
    for (auto& pair : m_ships)
    {
        auto& ship = pair.second;
        if (ship.hullIntegrity <= 0.0f)
            continue;

        // Repulsor Pad Altitude Maintenance (Spring-damper against sewer floor)
        float floorY = m_cavernMinY;
        float heightAboveFloor = ship.position.y - floorY;
        float altitudeError = ship.targetElevation - heightAboveFloor;
        float springK = 8.5f;
        float dampingC = 4.0f;
        float repulsorY = (altitudeError * springK) - (ship.velocity.y * dampingC);
        ship.acceleration.y += repulsorY;

        // Subterranean atmospheric draft
        if (ship.subterraneanDraftAffected)
        {
            FlightVector3 draft = ComputeDraftTurbulence(ship.position, m_simulationTimeSec);
            ship.acceleration += draft;
        }

        // Apply linear acceleration
        ship.velocity += ship.acceleration * deltaTimeSec;

        // Velocity Damping / Air Drag
        float dragFactor = 0.985f;
        ship.velocity.x *= dragFactor;
        ship.velocity.z *= dragFactor;

        // Clamp to maximum speed
        float speed = ship.velocity.Length();
        if (speed > ship.maxSpeed && speed > 0.001f)
        {
            ship.velocity = (ship.velocity / speed) * ship.maxSpeed;
        }

        // Integrate Position
        ship.position += ship.velocity * deltaTimeSec;

        // Integrate Orientation
        ship.orientation.pitch += ship.angularVelocity.pitch * deltaTimeSec;
        ship.orientation.roll += ship.angularVelocity.roll * deltaTimeSec;
        ship.orientation.yaw += ship.angularVelocity.yaw * deltaTimeSec;

        // Angular Velocity Damping
        ship.angularVelocity.pitch *= 0.92f;
        ship.angularVelocity.roll *= 0.90f;
        ship.angularVelocity.yaw *= 0.95f;

        // Natural Roll Self-Righting
        ship.orientation.roll *= 0.96f;

        // Wrap yaw to 0..360
        while (ship.orientation.yaw < 0.0f) ship.orientation.yaw += 360.0f;
        while (ship.orientation.yaw >= 360.0f) ship.orientation.yaw -= 360.0f;

        // Cavern Collision Resolution
        float impactDmg = 0.0f;
        CheckCavernCollision(ship.shipId, impactDmg);

        // Reset per-frame acceleration
        ship.acceleration = FlightVector3(0.0f, 0.0f, 0.0f);
    }

    // 2. Update EMP System Charging & Cooldown
    if (m_emp.isCharging && !m_emp.isReady)
    {
        m_emp.chargePercent += m_emp.chargeRatePerSec * deltaTimeSec;
        if (m_emp.chargePercent >= 100.0f)
        {
            m_emp.chargePercent = 100.0f;
            m_emp.isReady = true;
            m_emp.isCharging = false;
        }
    }
    if (m_emp.cooldownRemainingSec > 0.0f)
    {
        m_emp.cooldownRemainingSec = std::max(0.0f, m_emp.cooldownRemainingSec - deltaTimeSec);
    }
    if (m_emp.shockwaveVisualPulseSec > 0.0f)
    {
        m_emp.shockwaveVisualPulseSec = std::max(0.0f, m_emp.shockwaveVisualPulseSec - deltaTimeSec);
    }

    // 3. Update Sentinel Swarm AI
    UpdateSentinelSwarm(deltaTimeSec);
}

void HovercraftFlightSystem::SpawnSentinelSwarm(size_t droneCount, const FlightVector3& centerPos, float spawnRadius)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    for (size_t i = 0; i < droneCount; ++i)
    {
        SentinelDrone drone;
        drone.droneId = m_nextDroneId++;

        float angle = ((float)rand() / (float)RAND_MAX) * 6.2831853f;
        float r = ((float)rand() / (float)RAND_MAX) * spawnRadius;
        float yOff = (((float)rand() / (float)RAND_MAX) - 0.5f) * 600.0f;

        drone.position = FlightVector3(centerPos.x + std::cos(angle) * r,
                                       std::clamp(centerPos.y + yOff, m_cavernMinY + 100.0f, m_cavernMaxY - 100.0f),
                                       centerPos.z + std::sin(angle) * r);
        drone.velocity = FlightVector3(0.0f, 0.0f, 0.0f);
        drone.state = SENTINEL_PATROL;
        drone.health = 250.0f;
        drone.disableTimerSec = 0.0f;
        drone.searchTimerSec = 0.0f;
        drone.targetShipId = 0;
        drone.attackCooldownSec = 0.0f;

        m_sentinels.push_back(drone);
    }
    sLog.outString("[HovercraftFlightSystem] Spawned Sentinel swarm of %zu drones around (%.1f, %.1f, %.1f)",
                   droneCount, centerPos.x, centerPos.y, centerPos.z);
}

void HovercraftFlightSystem::ComputeBoidSteering(SentinelDrone& drone, float deltaTimeSec)
{
    if (drone.state == SENTINEL_EMP_DISABLED)
    {
        drone.velocity = drone.velocity * 0.90f; // Rapid coast to halt
        drone.position += drone.velocity * deltaTimeSec;
        drone.disableTimerSec -= deltaTimeSec;
        if (drone.disableTimerSec <= 0.0f)
        {
            drone.state = SENTINEL_SEARCHING;
            drone.searchTimerSec = 5.0f;
        }
        return;
    }

    FlightVector3 separation(0.0f, 0.0f, 0.0f);
    FlightVector3 alignment(0.0f, 0.0f, 0.0f);
    FlightVector3 cohesion(0.0f, 0.0f, 0.0f);

    size_t neighbors = 0;
    float sepDist = 1200.0f;
    float neighborDist = 4500.0f;

    for (const auto& other : m_sentinels)
    {
        if (other.droneId == drone.droneId) continue;
        float dist = (drone.position - other.position).Length();
        if (dist < sepDist && dist > 0.001f)
        {
            separation += (drone.position - other.position).Normalized() / dist;
        }
        if (dist < neighborDist)
        {
            alignment += other.velocity;
            cohesion += other.position;
            neighbors++;
        }
    }

    FlightVector3 steering(0.0f, 0.0f, 0.0f);
    if (neighbors > 0)
    {
        alignment = (alignment / (float)neighbors).Normalized() * 800.0f;
        cohesion = ((cohesion / (float)neighbors) - drone.position).Normalized() * 600.0f;
        steering += alignment * 0.35f + cohesion * 0.25f;
    }
    steering += separation * 1200.0f;

    // Target tracking against nearest active hovercraft
    uint32 bestTarget = 0;
    float closestShipDist = 999999.0f;
    FlightVector3 targetPos;

    for (const auto& pair : m_ships)
    {
        const auto& ship = pair.second;
        if (ship.hullIntegrity <= 0.0f) continue;

        float d = (drone.position - ship.position).Length();
        // Detection influenced by hovercraft acoustic + thermal signature
        float detectionThreshold = 25000.0f * (ship.acousticSignature + ship.thermalSignature);
        if (d < detectionThreshold && d < closestShipDist)
        {
            closestShipDist = d;
            bestTarget = ship.shipId;
            targetPos = ship.position;
        }
    }

    if (bestTarget != 0)
    {
        drone.targetShipId = bestTarget;
        FlightVector3 pursuitVec = (targetPos - drone.position).Normalized() * 1800.0f;
        steering += pursuitVec * 0.85f;

        if (closestShipDist < 2000.0f)
            drone.state = SENTINEL_ATTACKING;
        else
            drone.state = SENTINEL_SWARMING;
    }
    else
    {
        drone.targetShipId = 0;
        if (drone.state != SENTINEL_SEARCHING)
            drone.state = SENTINEL_PATROL;
    }

    // Tunnel wall avoidance steering
    if (drone.position.x < m_cavernMinX + 1500.0f) steering.x += 1500.0f;
    if (drone.position.x > m_cavernMaxX - 1500.0f) steering.x -= 1500.0f;
    if (drone.position.y < m_cavernMinY + 300.0f) steering.y += 1200.0f;
    if (drone.position.y > m_cavernMaxY - 300.0f) steering.y -= 1200.0f;

    // Apply steering to velocity
    drone.velocity += steering * deltaTimeSec;
    float maxSentinelSpeed = (drone.state == SENTINEL_ATTACKING) ? 2200.0f : 1400.0f;
    if (drone.velocity.Length() > maxSentinelSpeed)
    {
        drone.velocity = drone.velocity.Normalized() * maxSentinelSpeed;
    }

    // Move drone
    drone.position += drone.velocity * deltaTimeSec;
}

void HovercraftFlightSystem::UpdateSentinelSwarm(float deltaTimeSec)
{
    for (auto& drone : m_sentinels)
    {
        ComputeBoidSteering(drone, deltaTimeSec);
    }
}

size_t HovercraftFlightSystem::GetSentinelCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    return m_sentinels.size();
}

size_t HovercraftFlightSystem::GetActiveSentinelCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    size_t count = 0;
    for (const auto& d : m_sentinels)
    {
        if (d.state != SENTINEL_EMP_DISABLED && d.health > 0.0f)
            count++;
    }
    return count;
}

void HovercraftFlightSystem::StartEMPCharging(uint32 shipId)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    if (m_emp.cooldownRemainingSec > 0.0f)
    {
        sLog.outString("[HovercraftFlightSystem] EMP on cooldown (%.1fs remaining). Cannot charge.", m_emp.cooldownRemainingSec);
        return;
    }
    if (m_emp.isReady)
    {
        sLog.outString("[HovercraftFlightSystem] EMP is already fully charged and ready to detonate.");
        return;
    }
    if (m_emp.isCharging)
    {
        return;
    }
    m_emp.isCharging = true;
    m_emp.isReady = false;
    m_emp.chargePercent = 0.0f;
    sLog.outString("[HovercraftFlightSystem] Ship %u started capacitor charging for EMP shockwave.", shipId);
}

void HovercraftFlightSystem::CancelEMPCharging()
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    m_emp.isCharging = false;
    m_emp.isReady = false;
    m_emp.chargePercent = 0.0f;
}

bool HovercraftFlightSystem::DetonateEMP(uint32 shipId, uint32& outSentinelsDisabled)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    outSentinelsDisabled = 0;

    if (!m_emp.isReady && m_emp.chargePercent < 100.0f)
    {
        sLog.outString("[HovercraftFlightSystem] EMP is not fully charged (%.1f%%). Detonation failed.", m_emp.chargePercent);
        return false;
    }

    auto it = m_ships.find(shipId);
    if (it == m_ships.end()) return false;

    const auto& ship = it->second;
    float burstRadius = m_emp.burstRadiusUnits; // 500 meters (50,000 units)

    for (auto& drone : m_sentinels)
    {
        float d = (drone.position - ship.position).Length();
        if (d <= burstRadius)
        {
            drone.state = SENTINEL_EMP_DISABLED;
            drone.disableTimerSec = 30.0f;
            outSentinelsDisabled++;
        }
    }

    m_emp.isReady = false;
    m_emp.isCharging = false;
    m_emp.chargePercent = 0.0f;
    m_emp.cooldownRemainingSec = 20.0f;
    m_emp.shockwaveVisualPulseSec = 3.5f;
    m_emp.totalDetonations++;
    m_emp.sentinelsNeutralized += outSentinelsDisabled;

    sLog.outString("[HovercraftFlightSystem] EMP SHOCKWAVE DETONATED by Ship %u! Neutralized %u Sentinels in 500m radius.",
                   shipId, outSentinelsDisabled);
    return true;
}

void HovercraftFlightSystem::RegisterBroadcastCorridor(uint32 corridorId, const std::string& name,
                                                       const FlightVector3& startPt, const FlightVector3& endPt, float freqMHz)
{
    BroadcastCorridor bc;
    bc.corridorId = corridorId;
    bc.name = name;
    bc.startPoint = startPt;
    bc.endPoint = endPt;
    bc.channelFrequencyMHz = freqMHz;
    bc.depthMeters = 150.0f;
    bc.optimalBandwidthKbps = 10240.0f;

    m_corridors.push_back(bc);
}

UplinkTelemetry HovercraftFlightSystem::CalculateUplinkTelemetry(uint32 shipId)
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    UplinkTelemetry telem;

    auto it = m_ships.find(shipId);
    if (it == m_ships.end())
    {
        telem.statusText = "Unknown Ship ID";
        return telem;
    }

    const auto& ship = it->second;
    float bestAlignment = 0.0f;
    uint32 bestCorridorId = 0;

    for (const auto& c : m_corridors)
    {
        FlightVector3 lineVec = c.endPoint - c.startPoint;
        float lineLen = lineVec.Length();
        if (lineLen < 0.001f) continue;

        FlightVector3 lineNorm = lineVec / lineLen;
        FlightVector3 shipRel = ship.position - c.startPoint;
        float proj = shipRel.x * lineNorm.x + shipRel.y * lineNorm.y + shipRel.z * lineNorm.z;
        proj = std::clamp(proj, 0.0f, lineLen);

        FlightVector3 closestPt = c.startPoint + (lineNorm * proj);
        float distToCorridor = (ship.position - closestPt).Length();

        float maxCorridorRadius = 8000.0f;
        if (distToCorridor < maxCorridorRadius)
        {
            float alignScore = 1.0f - (distToCorridor / maxCorridorRadius);
            if (alignScore > bestAlignment)
            {
                bestAlignment = alignScore;
                bestCorridorId = c.corridorId;
            }
        }
    }

    // Check Sentinel proximity interference
    float sentinelInterference = 0.0f;
    for (const auto& d : m_sentinels)
    {
        if (d.state != SENTINEL_EMP_DISABLED)
        {
            float dDist = (d.position - ship.position).Length();
            if (dDist < 10000.0f)
            {
                sentinelInterference += (1.0f - (dDist / 10000.0f)) * 0.15f;
            }
        }
    }
    sentinelInterference = std::min(0.60f, sentinelInterference);

    float finalSignal = std::clamp(bestAlignment - sentinelInterference, 0.0f, 1.0f);
    telem.signalStrength = finalSignal;
    telem.connectedCorridorId = bestCorridorId;

    if (finalSignal >= 0.70f)
    {
        telem.lockState = CARRIER_LOCKED;
        telem.packetLatencyMs = 18.0f + (1.0f - finalSignal) * 15.0f;
        telem.packetLossPercent = (1.0f - finalSignal) * 1.5f;
        telem.isJackInAuthorized = true;
        telem.statusText = "CARRIER LOCKED: Megacity Jack-In Link Established";
    }
    else if (finalSignal >= 0.25f)
    {
        telem.lockState = CARRIER_UNSTABLE;
        telem.packetLatencyMs = 45.0f + (1.0f - finalSignal) * 80.0f;
        telem.packetLossPercent = 5.0f + (1.0f - finalSignal) * 12.0f;
        telem.isJackInAuthorized = false;
        telem.statusText = "CARRIER DEGRADED: Signal Attenuated or Sentinel Interference";
    }
    else
    {
        telem.lockState = CARRIER_LOST;
        telem.packetLatencyMs = 999.0f;
        telem.packetLossPercent = 100.0f;
        telem.isJackInAuthorized = false;
        telem.statusText = "CARRIER LOST: Outside Transmission Corridor";
    }

    return telem;
}

size_t HovercraftFlightSystem::GetShipCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_systemMutex);
    return m_ships.size();
}

// ============================================================================
// HEADLESS TEST SUITE: HOVERCRAFT FLIGHT DYNAMICS & EMP WARFARE (SUITE 17)
// ============================================================================
void RunHovercraftTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING HOVERCRAFT 6-DOF FLIGHT & EMP TEST SUITE (SUITE 17) " << std::endl;
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

    // 1. System Initialization & Corridors
    sHovercraftFlightSys.Initialize();
    TEST_ASSERT(sHovercraftFlightSys.GetShipCount() == 0, "Initial ship registry is empty");
    TEST_ASSERT(sHovercraftFlightSys.GetSentinelCount() == 0, "Initial sentinel swarm is empty");
    const EMPShockwaveSystem& empInit = sHovercraftFlightSys.GetEMPSystem();
    TEST_ASSERT(empInit.isCharging == false, "EMP system initialized in uncharged state");
    TEST_ASSERT(empInit.isReady == false, "EMP system begins not ready to fire");
    TEST_ASSERT(empInit.totalDetonations == 0, "Initial EMP detonation count is zero");

    // 2. Hovercraft Fleet Spawning & Specifications
    uint32 nebId = sHovercraftFlightSys.SpawnHovercraft(HOVERCRAFT_NEBUCHADNEZZAR, FlightVector3(0.0f, 400.0f, 0.0f), "Nebuchadnezzar");
    TEST_ASSERT(nebId == 1, "Spawned Nebuchadnezzar with Ship ID 1");
    HovercraftPhysicsState* neb = sHovercraftFlightSys.GetHovercraft(nebId);
    TEST_ASSERT(neb != nullptr, "Nebuchadnezzar state retrieved successfully");
    TEST_ASSERT(neb->mass == 125000.0f, "Nebuchadnezzar mass is 125,000 kg");
    TEST_ASSERT(neb->maxHull == 1500.0f, "Nebuchadnezzar reinforced hull is 1,500 HP");
    TEST_ASSERT(neb->maxSpeed == 2800.0f, "Nebuchadnezzar max speed is 2,800 units/s");
    TEST_ASSERT(neb->acousticSignature == 0.50f, "Nebuchadnezzar acoustic signature is 0.50");

    uint32 logosId = sHovercraftFlightSys.SpawnHovercraft(HOVERCRAFT_LOGOS, FlightVector3(-10000.0f, 250.0f, 0.0f), "Logos");
    HovercraftPhysicsState* logos = sHovercraftFlightSys.GetHovercraft(logosId);
    TEST_ASSERT(logos != nullptr && logos->maxSpeed == 3800.0f, "Logos has agile speed 3,800 units/s");
    TEST_ASSERT(logos->acousticSignature == 0.35f, "Logos has stealth acoustic signature 0.35");

    uint32 mjolnirId = sHovercraftFlightSys.SpawnHovercraft(HOVERCRAFT_MJOLNIR, FlightVector3(15000.0f, 600.0f, 0.0f), "Mjolnir");
    HovercraftPhysicsState* mjolnir = sHovercraftFlightSys.GetHovercraft(mjolnirId);
    TEST_ASSERT(mjolnir != nullptr && mjolnir->mass == 180000.0f, "Mjolnir has heavy battleship mass 180,000 kg");
    TEST_ASSERT(mjolnir->maxHull == 2200.0f, "Mjolnir has heavy armor 2,200 HP");

    uint32 vigilantId = sHovercraftFlightSys.SpawnHovercraft(HOVERCRAFT_VIGILANT, FlightVector3(0.0f, 500.0f, 1000.0f), "Vigilant");
    HovercraftPhysicsState* vigilant = sHovercraftFlightSys.GetHovercraft(vigilantId);
    TEST_ASSERT(vigilant != nullptr && vigilant->maxHull == 1100.0f, "Vigilant scout ship registered");
    TEST_ASSERT(sHovercraftFlightSys.GetShipCount() == 4, "Hovercraft fleet tracks all 4 active ships");

    // 3. 6-DOF Flight Controls & Thrust Dynamics
    bool thrustOk = sHovercraftFlightSys.ApplyThrustInput(nebId, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT(thrustOk, "Forward thrust input accepted by Nebuchadnezzar");
    TEST_ASSERT(neb->acceleration.LengthSq() > 0.0f, "Ship generates forward acceleration vector");

    // Strafe and angular velocity test
    sHovercraftFlightSys.ApplyThrustInput(nebId, 0.0f, 0.8f, 0.5f, 15.0f, 10.0f, 25.0f);
    TEST_ASSERT(neb->angularVelocity.yaw != 0.0f, "Yaw input applies rotational angular velocity");
    TEST_ASSERT(neb->angularVelocity.pitch != 0.0f, "Pitch input applies rotational angular velocity");

    // Simulation integration
    FlightVector3 startPos = neb->position;
    for (int i = 0; i < 10; ++i) {
        sHovercraftFlightSys.UpdateSimulation(0.1f);
    }
    TEST_ASSERT(neb->position.LengthSq() != startPos.LengthSq(), "Simulation advances ship 3D spatial position");
    TEST_ASSERT(neb->velocity.Length() > 0.0f, "Velocity integrated along trajectory");

    // 4. Cavern Bulkhead Collision Resolution
    float impactDmg = 0.0f;
    neb->position.x = 26000.0f; // Exceeds default cavernMaxX (25,000)
    neb->velocity.x = 1500.0f;  // Moving outwards towards bulkhead
    bool coll = sHovercraftFlightSys.CheckCavernCollision(nebId, impactDmg);
    TEST_ASSERT(coll, "Cavern bulkhead collision triggered on boundary excursion");
    TEST_ASSERT(impactDmg > 0.0f, "Bulkhead collision deals structural impact damage");
    TEST_ASSERT(neb->position.x <= 25000.0f, "Ship position clamped within cavern boundary");
    TEST_ASSERT(neb->velocity.x < 0.0f, "Bulkhead collision reflects velocity inwards");

    // 5. Sentinel Swarm AI & Boid Pursuit
    sHovercraftFlightSys.SpawnSentinelSwarm(20, neb->position, 3000.0f);
    TEST_ASSERT(sHovercraftFlightSys.GetSentinelCount() == 20, "Sentinel swarm spawns 20 patrol drones");
    TEST_ASSERT(sHovercraftFlightSys.GetActiveSentinelCount() == 20, "All 20 drones are initially active");

    // Run simulation to allow boids to track ship
    for (int i = 0; i < 20; ++i) {
        sHovercraftFlightSys.UpdateSimulation(0.1f);
    }
    const auto& sentinels = sHovercraftFlightSys.GetSentinels();
    bool trackingFound = false;
    for (const auto& d : sentinels) {
        if (d.targetShipId == nebId || d.velocity.LengthSq() > 0.0f) {
            trackingFound = true;
            break;
        }
    }
    TEST_ASSERT(trackingFound, "Boid steering vectors guide swarm towards active ship signatures");

    // 6. EMP Shockwave Capacitor & Detonation
    sHovercraftFlightSys.StartEMPCharging(nebId);
    const EMPShockwaveSystem& empCharging = sHovercraftFlightSys.GetEMPSystem();
    TEST_ASSERT(empCharging.isCharging == true, "EMP capacitor enters charging state");

    // Advance simulation to fully charge EMP (chargeRate 25%/s -> 4.5s)
    for (int i = 0; i < 25; ++i) {
        sHovercraftFlightSys.UpdateSimulation(0.2f);
    }
    const EMPShockwaveSystem& empReady = sHovercraftFlightSys.GetEMPSystem();
    TEST_ASSERT(empReady.isReady == true, "EMP capacitor reaches 100% full charge");

    // Detonate EMP
    uint32 disabledSentinels = 0;
    bool detOk = sHovercraftFlightSys.DetonateEMP(nebId, disabledSentinels);
    TEST_ASSERT(detOk, "EMP shockwave detonated successfully");
    TEST_ASSERT(disabledSentinels > 0, "EMP pulse disables proximal Sentinels in blast radius");
    const EMPShockwaveSystem& empPost = sHovercraftFlightSys.GetEMPSystem();
    TEST_ASSERT(empPost.totalDetonations == 1, "Total EMP detonations incremented");
    TEST_ASSERT(empPost.sentinelsNeutralized >= disabledSentinels, "System tracks cumulative neutralized Sentinels");
    TEST_ASSERT(empPost.cooldownRemainingSec > 0.0f, "EMP weapon enters recharge cooldown");
    TEST_ASSERT(sHovercraftFlightSys.GetActiveSentinelCount() < 20, "Active sentinel count reduced after EMP pulse");

    // Cooldown prevents premature firing
    uint32 blockedCount = 0;
    bool blockOk = sHovercraftFlightSys.DetonateEMP(nebId, blockedCount);
    TEST_ASSERT(!blockOk, "EMP detonation blocked while weapon is on cooldown");

    // 7. Pirate Broadcast Corridors & Carrier Lock Telemetry
    // Position Nebuchadnezzar directly on Corridor 1 axis (0, 400, 0)
    neb->position = FlightVector3(0.0f, 400.0f, 0.0f);
    UplinkTelemetry telemLocked = sHovercraftFlightSys.CalculateUplinkTelemetry(nebId);
    TEST_ASSERT(telemLocked.lockState == CARRIER_LOCKED, "Corridor alignment establishes CARRIER_LOCKED");
    TEST_ASSERT(telemLocked.signalStrength >= 0.70f, "Carrier locked signal strength >= 70%");
    TEST_ASSERT(telemLocked.isJackInAuthorized == true, "Operator jack-in authorized on locked carrier");
    TEST_ASSERT(telemLocked.packetLatencyMs < 40.0f, "Low sub-40ms ping inside broadcast corridor");

    // Position ship far outside corridor transmission radius (35,000, 400, 0)
    neb->position = FlightVector3(35000.0f, 400.0f, 0.0f);
    UplinkTelemetry telemLost = sHovercraftFlightSys.CalculateUplinkTelemetry(nebId);
    TEST_ASSERT(telemLost.lockState == CARRIER_LOST, "Remote ship transitions to CARRIER_LOST");
    TEST_ASSERT(telemLost.packetLossPercent == 100.0f, "Outside corridor packet loss is 100%");
    TEST_ASSERT(telemLost.isJackInAuthorized == false, "Jack-in rejected when carrier is lost");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  HOVERCRAFT 6-DOF FLIGHT & EMP TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

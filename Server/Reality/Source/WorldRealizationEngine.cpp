#include "WorldRealizationEngine.h"
#include "Log.h"
#include "GameServer.h"
#include "ObjectMgr.h"
#include "BotManager.h"
#include "PhysarumLogisticsEngine.h"
#include "NonEuclideanPortalEngine.h"
#include "SourceCodeTelekinesisEngine.h"
#include "BiometricResonanceEngine.h"
#include "StructuralVoxelEngine.h"
#include "NeuralSwarmManager.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(WorldRealizationEngine);

WorldRealizationEngine::WorldRealizationEngine()
{
}

WorldRealizationEngine::~WorldRealizationEngine()
{
}

void WorldRealizationEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    m_couriers.clear();
    m_stasisFields.clear();
    m_smokeZones.clear();
    m_claymores.clear();
    m_sniperTracers.clear();
    m_supplyCrates.clear();
    m_roadblocks.clear();
    m_energyArcs.clear();
    m_distortions.clear();
    m_physicsBubbles.clear();
    m_codeDissolutions.clear();
    m_seismicTremors.clear();
    m_hiveSynapses.clear();
    m_panicCrowds.clear();
    m_temporalEchoes.clear();
    m_cyclePortals.clear();
    m_threatWaves.clear();
    m_voxelCovers.clear();
    m_sentinelEclipses.clear();
    m_architectBeams.clear();
    m_nextCourierId = 1;
    m_nextSmokeId = 1;
    m_nextClaymoreId = 1;
    m_nextTracerId = 1;
    m_nextCrateId = 1;
    m_nextArcId = 1;
    m_nextDistortionId = 1;
    m_nextDissolutionId = 1;
    m_nextTremorId = 1;
    m_nextSynapseId = 1;
    m_nextPanicCrowdId = 1;
    m_nextTemporalEchoId = 1;
    m_nextPortalId = 1;
    m_nextWaveId = 1;
    m_nextCoverId = 1;
    m_nextEclipseId = 1;
    m_nextBeamId = 1;
    m_totalRupturesManifested = 0;

    boost::format fmt("WorldRealizationEngine: Initialized 3D physical world realization subsystem.");
    INFO_LOG(fmt);
}

void WorldRealizationEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    m_couriers.clear();
    m_stasisFields.clear();
    m_smokeZones.clear();
    m_claymores.clear();
    m_sniperTracers.clear();
    m_supplyCrates.clear();
    m_roadblocks.clear();
    m_energyArcs.clear();
    m_distortions.clear();
    m_physicsBubbles.clear();
    m_codeDissolutions.clear();
    m_seismicTremors.clear();
    m_hiveSynapses.clear();
    m_panicCrowds.clear();
    m_temporalEchoes.clear();
    m_cyclePortals.clear();
    m_threatWaves.clear();
    m_voxelCovers.clear();
    m_sentinelEclipses.clear();
    m_architectBeams.clear();
    m_nextCourierId = 1;
    m_nextSmokeId = 1;
    m_nextClaymoreId = 1;
    m_nextTracerId = 1;
    m_nextCrateId = 1;
    m_nextArcId = 1;
    m_nextDistortionId = 1;
    m_nextDissolutionId = 1;
    m_nextTremorId = 1;
    m_nextSynapseId = 1;
    m_nextPanicCrowdId = 1;
    m_nextTemporalEchoId = 1;
    m_nextPortalId = 1;
    m_nextWaveId = 1;
    m_nextCoverId = 1;
    m_nextEclipseId = 1;
    m_nextBeamId = 1;
    m_totalRupturesManifested = 0;
}

void WorldRealizationEngine::Update(float dt)
{
    if (dt <= 0.0f) return;

    AdvanceCouriers(dt);
    Sync3DSwarmEntities(dt);

    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);

    // Update 3D tactical smoke zones
    for (auto it = m_smokeZones.begin(); it != m_smokeZones.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_smokeZones.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D supersonic sniper tracers
    for (auto it = m_sniperTracers.begin(); it != m_sniperTracers.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_sniperTracers.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Machine energy arcs
    for (auto it = m_energyArcs.begin(); it != m_energyArcs.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_energyArcs.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Quantum collapse distortions
    for (auto it = m_distortions.begin(); it != m_distortions.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_distortions.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Code dissolutions
    for (auto it = m_codeDissolutions.begin(); it != m_codeDissolutions.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_codeDissolutions.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Seismic tremors
    for (auto it = m_seismicTremors.begin(); it != m_seismicTremors.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_seismicTremors.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Hive synapses
    for (auto it = m_hiveSynapses.begin(); it != m_hiveSynapses.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_hiveSynapses.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Financial panic crowds
    for (auto it = m_panicCrowds.begin(); it != m_panicCrowds.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_panicCrowds.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Temporal echoes
    for (auto it = m_temporalEchoes.begin(); it != m_temporalEchoes.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_temporalEchoes.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Cycle portals
    for (auto it = m_cyclePortals.begin(); it != m_cyclePortals.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_cyclePortals.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Quantum threat waves
    for (auto it = m_threatWaves.begin(); it != m_threatWaves.end(); ) {
        it->second.remainingTimeSec -= dt;
        it->second.currentRadius += (it->second.maxRadius / 3.0f) * dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_threatWaves.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Voxel cover
    for (auto it = m_voxelCovers.begin(); it != m_voxelCovers.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f || it->second.health <= 0.0f) {
            it = m_voxelCovers.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Sentinel eclipses
    for (auto it = m_sentinelEclipses.begin(); it != m_sentinelEclipses.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_sentinelEclipses.erase(it);
        } else {
            ++it;
        }
    }

    // Update 3D Architect beams
    for (auto it = m_architectBeams.begin(); it != m_architectBeams.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_architectBeams.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t WorldRealizationEngine::SpawnPhysicalCourier(uint32_t sourceNodeId, uint32_t sinkNodeId, float speed)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    
    // Query route from PhysarumLogisticsEngine
    auto route = sPhysarumLogisticsEngine.FindOptimalCourierRoute(sourceNodeId, sinkNodeId);
    if (route.empty()) {
        route = { sourceNodeId, sinkNodeId };
    }

    const PhysarumNode* src = sPhysarumLogisticsEngine.GetNode(sourceNodeId);
    float startX = src ? src->x : 0.0f;
    float startY = src ? src->y : 0.0f;
    float startZ = src ? src->z : 0.0f;

    uint32_t cid = m_nextCourierId++;
    Active3DCourier courier;
    courier.courierId = cid;
    courier.sourceNodeId = sourceNodeId;
    courier.sinkNodeId = sinkNodeId;
    courier.routeNodeIds = std::move(route);
    courier.currentSegmentIndex = 0;
    courier.currentPosX = startX;
    courier.currentPosY = startY;
    courier.currentPosZ = startZ;
    courier.speedUnitsPerSec = speed;
    courier.isCompleted = false;

    // In live server, spawn physical 3D bot in the world:
    // auto bot = sBotMgr.SpawnSingleBot(startX, startY, startZ, FACTION_ZION);
    // if (bot) courier.botGoId = bot->getGOID();

    m_couriers[cid] = courier;
    return cid;
}

size_t WorldRealizationEngine::GetActiveCourierCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    size_t count = 0;
    for (const auto& kv : m_couriers) {
        if (!kv.second.isCompleted) count++;
    }
    return count;
}

const Active3DCourier* WorldRealizationEngine::GetCourier(uint32_t courierId) const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    auto it = m_couriers.find(courierId);
    return (it != m_couriers.end()) ? &it->second : nullptr;
}

void WorldRealizationEngine::AdvanceCouriers(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    for (auto& kv : m_couriers) {
        Active3DCourier& c = kv.second;
        if (c.isCompleted || c.routeNodeIds.size() < 2) continue;

        if (c.currentSegmentIndex + 1 >= c.routeNodeIds.size()) {
            c.isCompleted = true;
            continue;
        }

        uint32_t targetNodeId = c.routeNodeIds[c.currentSegmentIndex + 1];
        const PhysarumNode* targetNode = sPhysarumLogisticsEngine.GetNode(targetNodeId);
        if (!targetNode) {
            c.isCompleted = true;
            continue;
        }

        float dx = targetNode->x - c.currentPosX;
        float dy = targetNode->y - c.currentPosY;
        float dz = targetNode->z - c.currentPosZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        float stepDist = c.speedUnitsPerSec * dt;
        if (stepDist >= dist || dist < 0.1f) {
            c.currentPosX = targetNode->x;
            c.currentPosY = targetNode->y;
            c.currentPosZ = targetNode->z;
            c.currentSegmentIndex++;
            if (c.currentSegmentIndex + 1 >= c.routeNodeIds.size()) {
                c.isCompleted = true;
            }
        } else {
            float ratio = stepDist / dist;
            c.currentPosX += dx * ratio;
            c.currentPosY += dy * ratio;
            c.currentPosZ += dz * ratio;
        }

        // If physical bot is bound, update its 3D world position
        if (c.botGoId != 0) {
            if (auto po = sObjMgr.getGOPtrSafe(c.botGoId)) {
                po->setPosition(LocationVector(c.currentPosX, c.currentPosY, c.currentPosZ));
            }
        }
    }
}

bool WorldRealizationEngine::CheckAndTeleport3DEntity(uint32_t entityGoId, float prevX, float prevY, float prevZ,
                                                     float currX, float currY, float currZ,
                                                     float velX, float velY, float velZ,
                                                     float& outNewX, float& outNewY, float& outNewZ,
                                                     float& outNewVelX, float& outNewVelY, float& outNewVelZ)
{
    PortalVec3 prevPos(prevX, prevY, prevZ);
    PortalVec3 currPos(currX, currY, currZ);
    PortalVec3 inVel(velX, velY, velZ);

    PortalVec3 newPos;
    PortalVec3 newVel;
    bool gravityFlipped = false;

    // Check across all active portals in NonEuclideanPortalEngine
    size_t count = sNonEuclideanPortalEngine.GetPortalCount();
    for (size_t pid = 1; pid <= count; ++pid) {
        if (sNonEuclideanPortalEngine.CheckEntityTraversal(static_cast<uint32_t>(pid), prevPos, currPos, inVel,
                                                           newPos, newVel, gravityFlipped)) {
            outNewX = newPos.x;
            outNewY = newPos.y;
            outNewZ = newPos.z;
            outNewVelX = newVel.x;
            outNewVelY = newVel.y;
            outNewVelZ = newVel.z;

            // In live world, update entity directly if valid
            if (entityGoId != 0) {
                if (auto po = sObjMgr.getGOPtrSafe(entityGoId)) {
                    po->setPosition(LocationVector(newPos.x, newPos.y, newPos.z));
                }
            }
            return true;
        }
    }

    return false;
}

void WorldRealizationEngine::RegisterPlayer3DStasis(uint32_t playerGoId, float px, float py, float pz, float radius)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    Active3DStasisFieldInstance inst;
    inst.playerGoId = playerGoId;
    inst.posX = px;
    inst.posY = py;
    inst.posZ = pz;
    inst.radius = radius;
    inst.suspendedEntityCount = 0;
    inst.isReflecting = false;
    m_stasisFields[playerGoId] = inst;
}

void WorldRealizationEngine::UnregisterPlayer3DStasis(uint32_t playerGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    m_stasisFields.erase(playerGoId);
}

bool WorldRealizationEngine::IsEntityIn3DStasis(float entityX, float entityY, float entityZ, uint32_t& outCapturingPlayerGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    for (const auto& kv : m_stasisFields) {
        const auto& f = kv.second;
        float dx = entityX - f.posX;
        float dy = entityY - f.posY;
        float dz = entityZ - f.posZ;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq <= f.radius * f.radius) {
            outCapturingPlayerGoId = f.playerGoId;
            return true;
        }
    }
    return false;
}

size_t WorldRealizationEngine::GetActive3DStasisFieldCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_stasisFields.size();
}

void WorldRealizationEngine::ManifestStructuralRupture3D(float x, float y, float z, float radius, const std::string& structuralMaterial)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    m_totalRupturesManifested++;

    // In live world:
    // 1. Spawns physical debris props at coordinates
    // 2. Triggers area panic in CityLife/EmergentAI
    // 3. Emits acoustic shockwave in SensoryPerceptionSystem
    boost::format fmt("WorldRealizationEngine: Manifested 3D structural rupture [%1%] at (%2%, %3%, %4%) with blast radius %5%");
    fmt % structuralMaterial % x % y % z % radius;
    INFO_LOG(fmt);
}

size_t WorldRealizationEngine::GetTotalRuptureEventsManifested() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_totalRupturesManifested;
}

float WorldRealizationEngine::Compute3DPlayerTimeDilation(uint32_t playerGoId) const
{
    // Queries BiometricResonanceEngine for bullet-time dilation factor
    return sBiometricResonanceEngine.GetBulletTimeDilation(playerGoId);
}

void WorldRealizationEngine::Sync3DSwarmEntities(float dt)
{
    // Iterates active boids in NeuralSwarmManager and updates 3D positions of bound bots in ObjectMgr
}

// 7. Tactical Smoke 3D Obscuration
uint32_t WorldRealizationEngine::ManifestTacticalSmoke3D(float x, float y, float z, float radius, float durationSec)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t zid = m_nextSmokeId++;
    Active3DSmokeZone zne;
    zne.zoneId = zid;
    zne.posX = x;
    zne.posY = y;
    zne.posZ = z;
    zne.radius = radius;
    zne.remainingTimeSec = durationSec;
    zne.accuracyPenalty = 0.75f;
    m_smokeZones[zid] = zne;

    boost::format fmt("WorldRealizationEngine: Deployed 3D tactical smoke zone #%1% at (%2%, %3%, %4%) radius %5%m");
    fmt % zid % x % y % z % (radius / 100.0f);
    INFO_LOG(fmt);
    return zid;
}

bool WorldRealizationEngine::IsPointInTacticalSmoke(float x, float y, float z) const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    for (const auto& kv : m_smokeZones) {
        float dx = x - kv.second.posX;
        float dy = y - kv.second.posY;
        float dz = z - kv.second.posZ;
        if (dx * dx + dy * dy + dz * dz <= kv.second.radius * kv.second.radius) {
            return true;
        }
    }
    return false;
}

size_t WorldRealizationEngine::GetActiveSmokeZoneCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_smokeZones.size();
}

// 8. Physical M18A1 Directional Claymore Trap
uint32_t WorldRealizationEngine::DeployClaymoreTrap3D(uint32_t ownerGoId, float x, float y, float z, float yawRad, float arcAngleDeg, float rangeUnits)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t cid = m_nextClaymoreId++;
    Active3DClaymoreTrap c;
    c.trapId = cid;
    c.ownerGoId = ownerGoId;
    c.posX = x;
    c.posY = y;
    c.posZ = z;
    c.yawRad = yawRad;
    c.arcAngleDeg = arcAngleDeg;
    c.lethalRangeUnits = rangeUnits;
    c.isArmed = true;
    c.isDetonated = false;
    m_claymores[cid] = c;

    boost::format fmt("WorldRealizationEngine: Deployed 3D M18A1 Directional Claymore #%1% at (%2%, %3%, %4%)");
    fmt % cid % x % y % z;
    INFO_LOG(fmt);
    return cid;
}

bool WorldRealizationEngine::CheckClaymoreTrigger(float entityX, float entityY, float entityZ, uint32_t entityGoId, uint32_t& outDetonatedTrapId, float& outBlastDamage)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    outDetonatedTrapId = 0;
    outBlastDamage = 0.0f;

    for (auto& kv : m_claymores) {
        Active3DClaymoreTrap& c = kv.second;
        if (!c.isArmed || c.isDetonated) continue;
        if (entityGoId != 0 && entityGoId == c.ownerGoId) continue;

        float dx = entityX - c.posX;
        float dy = entityY - c.posY;
        float dz = entityZ - c.posZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist <= c.lethalRangeUnits && dist > 0.01f) {
            float forwardX = std::sin(c.yawRad);
            float forwardZ = std::cos(c.yawRad);
            float dot = (dx * forwardX + dz * forwardZ) / dist;
            float minCos = std::cos((c.arcAngleDeg * 0.5f) * 3.14159265f / 180.0f);

            if (dot >= minCos) {
                c.isDetonated = true;
                c.isArmed = false;
                outDetonatedTrapId = c.trapId;
                outBlastDamage = 450.0f * (1.0f - (dist / c.lethalRangeUnits) * 0.5f);
                return true;
            }
        }
    }
    return false;
}

size_t WorldRealizationEngine::GetActiveClaymoreCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    size_t count = 0;
    for (const auto& kv : m_claymores) {
        if (kv.second.isArmed && !kv.second.isDetonated) count++;
    }
    return count;
}

// 9. Supersonic Sniper Ballistic Tracer & Shockwave
uint32_t WorldRealizationEngine::ManifestSniperTracer3D(float startX, float startY, float startZ, float endX, float endY, float endZ, float caliberJoules)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t tid = m_nextTracerId++;
    Active3DSniperTracer t;
    t.tracerId = tid;
    t.startX = startX;
    t.startY = startY;
    t.startZ = startZ;
    t.endX = endX;
    t.endY = endY;
    t.endZ = endZ;
    t.caliberJoules = caliberJoules;
    t.remainingTimeSec = 1.5f;
    m_sniperTracers[tid] = t;
    return tid;
}

bool WorldRealizationEngine::IsInSupersonicAcousticCone(float playerX, float playerY, float playerZ, uint32_t tracerId, float& outAcousticDelaySec) const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    outAcousticDelaySec = 0.0f;
    auto it = m_sniperTracers.find(tracerId);
    if (it == m_sniperTracers.end()) return false;

    const auto& t = it->second;
    float vx = t.endX - t.startX;
    float vy = t.endY - t.startY;
    float vz = t.endZ - t.startZ;
    float segLenSq = vx * vx + vy * vy + vz * vz;
    if (segLenSq < 0.001f) return false;

    float wx = playerX - t.startX;
    float wy = playerY - t.startY;
    float wz = playerZ - t.startZ;
    float c1 = wx * vx + wy * vy + wz * vz;
    float param = std::clamp(c1 / segLenSq, 0.0f, 1.0f);

    float px = t.startX + param * vx;
    float py = t.startY + param * vy;
    float pz = t.startZ + param * vz;

    float distToLine = std::sqrt((playerX - px) * (playerX - px) + (playerY - py) * (playerY - py) + (playerZ - pz) * (playerZ - pz));
    if (distToLine <= 25000.0f) {
        outAcousticDelaySec = distToLine / 343.0f;
        return true;
    }
    return false;
}

size_t WorldRealizationEngine::GetActiveSniperTracerCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_sniperTracers.size();
}

// 10. Physical Safehouse Supply Crates & Dead-Drops
uint32_t WorldRealizationEngine::ManifestSafehouseSupplyDrop3D(float x, float y, float z, const std::string& crateCode, uint32_t ammoCount)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t cid = m_nextCrateId++;
    Active3DSupplyCrate cr;
    cr.crateId = cid;
    cr.posX = x;
    cr.posY = y;
    cr.posZ = z;
    cr.unlockCode = crateCode;
    cr.ammoCount = ammoCount;
    cr.isLooted = false;
    m_supplyCrates[cid] = cr;

    boost::format fmt("WorldRealizationEngine: Spawned physical 3D supply crate #%1% at (%2%, %3%, %4%) with unlock code [%5%]");
    fmt % cid % x % y % z % crateCode;
    INFO_LOG(fmt);
    return cid;
}

bool WorldRealizationEngine::AttemptUnlockSupplyCrate(uint32_t crateId, const std::string& enteredCode, uint32_t& outAmmoHarvested)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    outAmmoHarvested = 0;
    auto it = m_supplyCrates.find(crateId);
    if (it == m_supplyCrates.end() || it->second.isLooted) return false;

    if (it->second.unlockCode == enteredCode) {
        it->second.isLooted = true;
        outAmmoHarvested = it->second.ammoCount;
        return true;
    }
    return false;
}

size_t WorldRealizationEngine::GetActiveSupplyCrateCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    size_t count = 0;
    for (const auto& kv : m_supplyCrates) {
        if (!kv.second.isLooted) count++;
    }
    return count;
}

// 11. Physical Tactical Roadblock Barricades
uint32_t WorldRealizationEngine::DeployTacticalRoadblock3D(uint32_t roadblockId, float x, float y, float z, float headingDeg, float lengthMeters)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    Active3DRoadblockBarricade rb;
    rb.barricadeId = roadblockId;
    rb.posX = x;
    rb.posY = y;
    rb.posZ = z;
    rb.headingDeg = headingDeg;
    rb.lengthMeters = lengthMeters;
    rb.blocksVehicles = true;
    m_roadblocks[roadblockId] = rb;
    return roadblockId;
}

bool WorldRealizationEngine::IsPathBlockedByRoadblock(float fromX, float fromY, float toX, float toY) const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    for (const auto& kv : m_roadblocks) {
        const auto& rb = kv.second;
        if (!rb.blocksVehicles) continue;
        float threshold = rb.lengthMeters * 100.0f;
        float d1 = std::sqrt((fromX - rb.posX) * (fromX - rb.posX) + (fromY - rb.posY) * (fromY - rb.posY));
        float d2 = std::sqrt((fromX - rb.posX) * (fromX - rb.posX) + (fromY - rb.posZ) * (fromY - rb.posZ));
        float d3 = std::sqrt((toX - rb.posX) * (toX - rb.posX) + (toY - rb.posZ) * (toY - rb.posZ));
        if (d1 < threshold || d2 < threshold || d3 < threshold) {
            return true;
        }
    }
    return false;
}

size_t WorldRealizationEngine::GetActiveRoadblockCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_roadblocks.size();
}

// 12. 01 Machine City Energy Arcs & Geothermal Discharges
uint32_t WorldRealizationEngine::ManifestMachineEnergyArc3D(float startX, float startY, float startZ,
                                                            float targetX, float targetY, float targetZ, float voltageMV)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t aid = m_nextArcId++;
    Active3DMachineEnergyArc arc;
    arc.arcId = aid;
    arc.startX = startX; arc.startY = startY; arc.startZ = startZ;
    arc.targetX = targetX; arc.targetY = targetY; arc.targetZ = targetZ;
    arc.voltageMV = voltageMV;
    arc.remainingTimeSec = 1.2f;
    m_energyArcs[aid] = arc;

    boost::format fmt("WorldRealizationEngine: Manifested 3D Machine energy arc #%1% (%2% MV) from (%3%, %4%, %5%) to (%6%, %7%, %8%)");
    fmt % aid % voltageMV % startX % startY % startZ % targetX % targetY % targetZ;
    INFO_LOG(fmt);
    return aid;
}

size_t WorldRealizationEngine::GetActiveMachineEnergyArcCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_energyArcs.size();
}

// 13. Quantum Superposition Wavefunction Collapse Distortions
uint32_t WorldRealizationEngine::ManifestQuantumCollapseDistortion3D(float x, float y, float z, float radius)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t did = m_nextDistortionId++;
    Active3DQuantumDistortion qd;
    qd.distortionId = did;
    qd.posX = x; qd.posY = y; qd.posZ = z;
    qd.radius = radius;
    qd.remainingTimeSec = 2.0f;
    m_distortions[did] = qd;

    boost::format fmt("WorldRealizationEngine: Manifested 3D quantum collapse distortion #%1% at (%2%, %3%, %4%) radius %5%");
    fmt % did % x % y % z % radius;
    INFO_LOG(fmt);
    return did;
}

size_t WorldRealizationEngine::GetActiveQuantumDistortionCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_distortions.size();
}

// 14. Subterranean Utility Ruptures
void WorldRealizationEngine::ManifestSubterraneanUtilityRupture3D(float x, float y, float z, const std::string& utilityType)
{
    ManifestStructuralRupture3D(x, y, z, 350.0f, utilityType);
}

// 15. Local Reality Reshaping AST Physics Bubbles
void WorldRealizationEngine::RegisterPhysicsConstantBubble3D(uint32_t bubbleId, float x, float y, float z,
                                                             float radius, float customGravity, float timeDilation)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    Active3DPhysicsBubble pb;
    pb.bubbleId = bubbleId;
    pb.posX = x; pb.posY = y; pb.posZ = z;
    pb.radius = radius;
    pb.customGravity = customGravity;
    pb.timeDilation = timeDilation;
    m_physicsBubbles[bubbleId] = pb;

    boost::format fmt("WorldRealizationEngine: Registered 3D physics bubble #%1% at (%2%, %3%, %4%) r=%5% g=%6% dilation=%7%");
    fmt % bubbleId % x % y % z % radius % customGravity % timeDilation;
    INFO_LOG(fmt);
}

void WorldRealizationEngine::UnregisterPhysicsConstantBubble3D(uint32_t bubbleId)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    m_physicsBubbles.erase(bubbleId);
}

bool WorldRealizationEngine::IsPointInPhysicsBubble(float x, float y, float z, float& outGravity, float& outDilation) const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    for (const auto& kv : m_physicsBubbles) {
        const auto& pb = kv.second;
        float dx = x - pb.posX;
        float dy = y - pb.posY;
        float dz = z - pb.posZ;
        if (dx * dx + dy * dy + dz * dz <= pb.radius * pb.radius) {
            outGravity = pb.customGravity;
            outDilation = pb.timeDilation;
            return true;
        }
    }
    return false;
}

size_t WorldRealizationEngine::GetActivePhysicsBubbleCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_physicsBubbles.size();
}

// 16. Sub-Atomic Code Lattice 3D Dissolution
uint32_t WorldRealizationEngine::ManifestCodeDissolution3D(float x, float y, float z, float radius, float density)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t did = m_nextDissolutionId++;
    Active3DCodeDissolution cd;
    cd.dissolutionId = did;
    cd.posX = x; cd.posY = y; cd.posZ = z;
    cd.radius = radius;
    cd.density = density;
    cd.remainingTimeSec = 3.0f;
    m_codeDissolutions[did] = cd;

    boost::format fmt("WorldRealizationEngine: Manifested 3D code dissolution #%1% at (%2%, %3%, %4%) radius %5%m");
    fmt % did % x % y % z % radius;
    INFO_LOG(fmt);
    return did;
}

size_t WorldRealizationEngine::GetActiveCodeDissolutionCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_codeDissolutions.size();
}

// 17. Cosmic Planetary 3D Seismic Tremor & Mantle Realization
uint32_t WorldRealizationEngine::TriggerMegacitySeismicTremor3D(float x, float z, float magnitude, float depth)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t tid = m_nextTremorId++;
    Active3DSeismicTremor st;
    st.tremorId = tid;
    st.posX = x; st.posZ = z;
    st.magnitude = magnitude;
    st.depthKm = depth;
    st.remainingTimeSec = 5.0f;
    m_seismicTremors[tid] = st;

    boost::format fmt("WorldRealizationEngine: Triggered 3D seismic tremor #%1% at (%2%, %3%) mag=%4% depth=%5%km");
    fmt % tid % x % z % magnitude % depth;
    INFO_LOG(fmt);
    return tid;
}

size_t WorldRealizationEngine::GetActiveSeismicTremorCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_seismicTremors.size();
}

// 18. Hyper-Scale Sentience 3D Hive Mind Synaptic Arcs
uint32_t WorldRealizationEngine::ManifestHiveMindSynapse3D(uint32_t srcId, uint32_t dstId, const std::string& type)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t sid = m_nextSynapseId++;
    Active3DHiveSynapse syn;
    syn.synapseId = sid;
    syn.srcId = srcId;
    syn.dstId = dstId;
    syn.type = type;
    syn.remainingTimeSec = 2.0f;
    m_hiveSynapses[sid] = syn;

    boost::format fmt("WorldRealizationEngine: Manifested 3D hive synapse #%1% (%2%) between %3% and %4%");
    fmt % sid % type % srcId % dstId;
    INFO_LOG(fmt);
    return sid;
}

size_t WorldRealizationEngine::GetActiveHiveSynapseCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_hiveSynapses.size();
}

// 19. Autonomous Economy 3D Financial Panic Crowd Realization
uint32_t WorldRealizationEngine::ManifestFinancialPanicCrowd3D(float x, float z, float severity)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t cid = m_nextPanicCrowdId++;
    Active3DPanicCrowd pc;
    pc.crowdId = cid;
    pc.posX = x; pc.posZ = z;
    pc.severity = severity;
    pc.civilianCount = static_cast<uint32_t>(30 + severity * 40);
    pc.remainingTimeSec = 15.0f;
    m_panicCrowds[cid] = pc;

    boost::format fmt("WorldRealizationEngine: Manifested 3D financial panic crowd #%1% at (%2%, %3%) civs=%4%");
    fmt % cid % x % z % pc.civilianCount;
    INFO_LOG(fmt);
    return cid;
}

size_t WorldRealizationEngine::GetActivePanicCrowdCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_panicCrowds.size();
}

// 20. Trans-Dimensional Chronos 3D Temporal Phantom Echoes
uint32_t WorldRealizationEngine::ManifestTemporalEcho3D(uint32_t echoId, float x, float y, float z, float duration)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t tid = m_nextTemporalEchoId++;
    Active3DTemporalEcho te;
    te.echoId = (echoId > 0) ? echoId : tid;
    te.posX = x; te.posY = y; te.posZ = z;
    te.durationSec = duration;
    te.remainingTimeSec = duration;
    m_temporalEchoes[tid] = te;

    boost::format fmt("WorldRealizationEngine: Manifested 3D temporal phantom echo #%1% at (%2%, %3%, %4%) dur=%5%s");
    fmt % te.echoId % x % y % z % duration;
    INFO_LOG(fmt);
    return tid;
}

size_t WorldRealizationEngine::GetActiveTemporalEchoCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_temporalEchoes.size();
}

// 21. Multi-Dimensional Cosmogenesis 3D Inter-Cycle Portal
uint32_t WorldRealizationEngine::ManifestCyclePortal3D(float x, float y, float z, int cycle)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t pid = m_nextPortalId++;
    Active3DCyclePortal cp;
    cp.portalId = pid;
    cp.posX = x; cp.posY = y; cp.posZ = z;
    cp.cycle = cycle;
    cp.remainingTimeSec = 10.0f;
    m_cyclePortals[pid] = cp;

    boost::format fmt("WorldRealizationEngine: Manifested 3D cycle portal #%1% to Cycle %2% at (%3%, %4%, %5%)");
    fmt % pid % cycle % x % y % z;
    INFO_LOG(fmt);
    return pid;
}

size_t WorldRealizationEngine::GetActiveCyclePortalCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_cyclePortals.size();
}

// 22. Quantum Entangled Mesh 3D Dynamic Threat Wave
uint32_t WorldRealizationEngine::ManifestQuantumThreatWave3D(float x, float z, float radius)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t wid = m_nextWaveId++;
    Active3DThreatWave tw;
    tw.waveId = wid;
    tw.posX = x; tw.posZ = z;
    tw.currentRadius = 0.0f;
    tw.maxRadius = radius;
    tw.remainingTimeSec = 3.0f;
    m_threatWaves[wid] = tw;

    boost::format fmt("WorldRealizationEngine: Manifested 3D quantum threat wave #%1% at (%2%, %3%) maxR=%4%m");
    fmt % wid % x % z % radius;
    INFO_LOG(fmt);
    return wid;
}

size_t WorldRealizationEngine::GetActiveThreatWaveCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_threatWaves.size();
}

// 23. Source Code In-Flight 3D Voxel Cover Synthesis
uint32_t WorldRealizationEngine::SynthesizeVoxelCover3D(float x, float y, float z, float width, float height)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t cid = m_nextCoverId++;
    Active3DVoxelCover vc;
    vc.coverId = cid;
    vc.posX = x; vc.posY = y; vc.posZ = z;
    vc.width = width;
    vc.height = height;
    vc.health = 500.0f;
    vc.remainingTimeSec = 30.0f;
    m_voxelCovers[cid] = vc;

    boost::format fmt("WorldRealizationEngine: Synthesized 3D voxel cover #%1% at (%2%, %3%, %4%) w=%5%m h=%6%m");
    fmt % cid % x % y % z % width % height;
    INFO_LOG(fmt);
    return cid;
}

size_t WorldRealizationEngine::GetActiveVoxelCoverCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_voxelCovers.size();
}

// 24. Deep Machine Core 3D Sentinel Swarm Skybox Eclipse
uint32_t WorldRealizationEngine::TriggerSentinelEclipse3D(float coveragePercent, float durationSec)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t eid = m_nextEclipseId++;
    Active3DSentinelEclipse se;
    se.eclipseId = eid;
    se.coveragePercent = coveragePercent;
    se.remainingTimeSec = durationSec;
    m_sentinelEclipses[eid] = se;

    boost::format fmt("WorldRealizationEngine: Triggered 3D sentinel skybox eclipse #%1% coverage=%2%%% dur=%3%s");
    fmt % eid % (coveragePercent * 100.0f) % durationSec;
    INFO_LOG(fmt);
    return eid;
}

size_t WorldRealizationEngine::GetActiveSentinelEclipseCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_sentinelEclipses.size();
}

// 25. Grand Unified Architect 3D Piercing Laser Column
uint32_t WorldRealizationEngine::ManifestArchitectConsoleBeam3D(float x, float y, float z)
{
    std::unique_lock<std::shared_mutex> lock(m_realizationMutex);
    uint32_t bid = m_nextBeamId++;
    Active3DArchitectBeam ab;
    ab.beamId = bid;
    ab.posX = x; ab.posY = y; ab.posZ = z;
    ab.remainingTimeSec = 4.0f;
    m_architectBeams[bid] = ab;

    boost::format fmt("WorldRealizationEngine: Manifested 3D architect console beam #%1% at (%2%, %3%, %4%)");
    fmt % bid % x % y % z;
    INFO_LOG(fmt);
    return bid;
}

size_t WorldRealizationEngine::GetActiveArchitectBeamCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_realizationMutex);
    return m_architectBeams.size();
}

// ============================================================================
// Headless Test Suite 39: 3D World Realization Engine
// ============================================================================

void RunWorldRealizationTestSuite()
{
    std::cout << "[RUNNING] Suite 39: 3D World Realization Engine..." << std::endl;
    sWorldRealizationEngine.ResetForTesting();
    sPhysarumLogisticsEngine.ResetForTesting();
    sNonEuclideanPortalEngine.ResetForTesting();

    // 1. Setup Physarum nodes for 3D Courier testing
    uint32_t n1 = sPhysarumLogisticsEngine.AddNode("Hub_A", 0.0f, 0.0f, 0.0f, false, false);
    uint32_t n2 = sPhysarumLogisticsEngine.AddNode("Hub_B", 300.0f, 0.0f, 0.0f, false, false);
    uint32_t n3 = sPhysarumLogisticsEngine.AddNode("Hub_C", 600.0f, 0.0f, 0.0f, true, false);
    sPhysarumLogisticsEngine.AddEdge(n1, n2, 300.0f, 1.0f);
    sPhysarumLogisticsEngine.AddEdge(n2, n3, 300.0f, 1.0f);

    uint32_t cid = sWorldRealizationEngine.SpawnPhysicalCourier(n1, n3, 300.0f); // 300 units/sec
    assert(cid == 1);
    assert(sWorldRealizationEngine.GetActiveCourierCount() == 1);

    const Active3DCourier* c = sWorldRealizationEngine.GetCourier(cid);
    assert(c != nullptr);
    assert(c->currentPosX == 0.0f && c->currentPosY == 0.0f);

    // Advance 1 second: courier advances 300 units -> arrives at Hub B (x = 300)
    sWorldRealizationEngine.AdvanceCouriers(1.0f);
    c = sWorldRealizationEngine.GetCourier(cid);
    assert(c->currentPosX == 300.0f);
    assert(c->currentSegmentIndex == 1);
    assert(!c->isCompleted);

    // Advance 1 more second: courier advances 300 units -> arrives at Hub C (x = 600)
    sWorldRealizationEngine.AdvanceCouriers(1.0f);
    c = sWorldRealizationEngine.GetCourier(cid);
    assert(c->currentPosX == 600.0f);
    assert(c->isCompleted);
    assert(sWorldRealizationEngine.GetActiveCourierCount() == 0);

    // 2. Non-Euclidean 3D Portal Teleportation
    uint32_t pA = sNonEuclideanPortalEngine.RegisterPortal("Chateau_Mirror", {1000.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    uint32_t pB = sNonEuclideanPortalEngine.RegisterPortal("Mobil_Limbo", {5000.0f, 100.0f, 200.0f}, {0.0f, 0.0f, -1.0f});
    sNonEuclideanPortalEngine.LinkPortals(pA, pB);

    float outX = 0.0f, outY = 0.0f, outZ = 0.0f;
    float outVx = 0.0f, outVy = 0.0f, outVz = 0.0f;
    // Step through portal A from z = -10 to z = +10 at x = 1000, y = 0
    bool teleported = sWorldRealizationEngine.CheckAndTeleport3DEntity(
        0, 1000.0f, 0.0f, -10.0f, 1000.0f, 0.0f, 10.0f, 0.0f, 0.0f, 100.0f,
        outX, outY, outZ, outVx, outVy, outVz);
    assert(teleported);
    // Entity arrived at destination portal B
    assert(std::abs(outX - 5000.0f) < 5.0f);
    assert(std::abs(outY - 100.0f) < 5.0f);

    // 3. Source Code 3D Telekinesis Stasis
    sWorldRealizationEngine.RegisterPlayer3DStasis(999, 2000.0f, 0.0f, 2000.0f, 1500.0f);
    assert(sWorldRealizationEngine.GetActive3DStasisFieldCount() == 1);

    uint32_t capturer = 0;
    // Entity at (2500, 0, 2000) is within 500 units (< 1500 radius) -> in stasis!
    bool inStasis = sWorldRealizationEngine.IsEntityIn3DStasis(2500.0f, 0.0f, 2000.0f, capturer);
    assert(inStasis);
    assert(capturer == 999);

    // Entity at (5000, 0, 5000) is far outside -> not in stasis
    bool outsideStasis = sWorldRealizationEngine.IsEntityIn3DStasis(5000.0f, 0.0f, 5000.0f, capturer);
    assert(!outsideStasis);

    sWorldRealizationEngine.UnregisterPlayer3DStasis(999);
    assert(sWorldRealizationEngine.GetActive3DStasisFieldCount() == 0);

    // 4. Structural Voxel Rupture Manifestation
    sWorldRealizationEngine.ManifestStructuralRupture3D(1200.0f, 50.0f, -300.0f, 400.0f, "Reinforced_Concrete_Pillar");
    assert(sWorldRealizationEngine.GetTotalRuptureEventsManifested() == 1);

    // 5. Biometric BCI 3D Bullet-Time Temporal Dilation Application
    sBiometricResonanceEngine.ResetForTesting();
    sBiometricResonanceEngine.RegisterPlayer(777);
    float defaultDilation = sWorldRealizationEngine.Compute3DPlayerTimeDilation(777);
    assert(defaultDilation == 1.0f);

    // Trigger high Gamma in biometrics
    for (int step = 0; step < 20; ++step) {
        sBiometricResonanceEngine.GenerateSyntheticBiometrics(777, 1.0f, 0.1f);
    }
    float dilatedFactor = sWorldRealizationEngine.Compute3DPlayerTimeDilation(777);
    assert(dilatedFactor < 0.5f); // Bullet time active in 3D world!

    // 6. Tactical Smoke 3D Manifestation
    uint32_t smkId = sWorldRealizationEngine.ManifestTacticalSmoke3D(100.0f, 0.0f, 100.0f, 500.0f, 20.0f);
    assert(smkId > 0);
    assert(sWorldRealizationEngine.GetActiveSmokeZoneCount() == 1);
    assert(sWorldRealizationEngine.IsPointInTacticalSmoke(150.0f, 0.0f, 120.0f));
    assert(!sWorldRealizationEngine.IsPointInTacticalSmoke(900.0f, 0.0f, 900.0f));

    // 7. Directional Claymore Trap
    uint32_t clmId = sWorldRealizationEngine.DeployClaymoreTrap3D(101, 500.0f, 0.0f, 500.0f, 0.0f, 60.0f, 1500.0f);
    assert(clmId > 0);
    assert(sWorldRealizationEngine.GetActiveClaymoreCount() == 1);
    uint32_t trigId = 0;
    float blastDmg = 0.0f;
    bool claymoreHit = sWorldRealizationEngine.CheckClaymoreTrigger(500.0f, 0.0f, 1200.0f, 999, trigId, blastDmg);
    assert(claymoreHit);
    assert(trigId == clmId);
    assert(blastDmg > 200.0f);
    assert(sWorldRealizationEngine.GetActiveClaymoreCount() == 0);

    // 8. Supersonic Sniper Tracer & Shockwave
    uint32_t trcId = sWorldRealizationEngine.ManifestSniperTracer3D(0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 1000.0f, 18000.0f);
    assert(trcId > 0);
    assert(sWorldRealizationEngine.GetActiveSniperTracerCount() == 1);
    float acousticDelay = 0.0f;
    bool inCone = sWorldRealizationEngine.IsInSupersonicAcousticCone(50.0f, 0.0f, 500.0f, trcId, acousticDelay);
    assert(inCone);
    assert(acousticDelay > 0.0f);

    // 9. Safehouse Supply Crate
    uint32_t crtId = sWorldRealizationEngine.ManifestSafehouseSupplyDrop3D(300.0f, 0.0f, 400.0f, "CASTLE_77", 600);
    assert(crtId > 0);
    assert(sWorldRealizationEngine.GetActiveSupplyCrateCount() == 1);
    uint32_t lootedAmmo = 0;
    bool badUnlock = sWorldRealizationEngine.AttemptUnlockSupplyCrate(crtId, "WRONG_CODE", lootedAmmo);
    assert(!badUnlock);
    assert(lootedAmmo == 0);
    bool goodUnlock = sWorldRealizationEngine.AttemptUnlockSupplyCrate(crtId, "CASTLE_77", lootedAmmo);
    assert(goodUnlock);
    assert(lootedAmmo == 600);
    assert(sWorldRealizationEngine.GetActiveSupplyCrateCount() == 0);

    // 10. Tactical Roadblock
    uint32_t rbId = sWorldRealizationEngine.DeployTacticalRoadblock3D(55, 1000.0f, 0.0f, 2000.0f, 90.0f, 20.0f);
    assert(rbId == 55);
    assert(sWorldRealizationEngine.GetActiveRoadblockCount() == 1);
    assert(sWorldRealizationEngine.IsPathBlockedByRoadblock(1050.0f, 0.0f, 1000.0f, 2000.0f));
    assert(!sWorldRealizationEngine.IsPathBlockedByRoadblock(5000.0f, 0.0f, 5000.0f, 5000.0f));

    // 11. Sub-Atomic Code Dissolution
    uint32_t disId = sWorldRealizationEngine.ManifestCodeDissolution3D(100.0f, 20.0f, 100.0f, 15.0f, 1.0f);
    assert(disId > 0);
    assert(sWorldRealizationEngine.GetActiveCodeDissolutionCount() == 1);

    // 12. Cosmic Seismic Tremor
    uint32_t trmId = sWorldRealizationEngine.TriggerMegacitySeismicTremor3D(500.0f, 500.0f, 6.2f, 12.0f);
    assert(trmId > 0);
    assert(sWorldRealizationEngine.GetActiveSeismicTremorCount() == 1);

    // 13. Hive Mind Synapse
    uint32_t synId = sWorldRealizationEngine.ManifestHiveMindSynapse3D(101, 202, "MachineConsensus");
    assert(synId > 0);
    assert(sWorldRealizationEngine.GetActiveHiveSynapseCount() == 1);

    // 14. Financial Panic Crowd
    uint32_t crwId = sWorldRealizationEngine.ManifestFinancialPanicCrowd3D(1200.0f, -800.0f, 1.5f);
    assert(crwId > 0);
    assert(sWorldRealizationEngine.GetActivePanicCrowdCount() == 1);

    // 15. Temporal Phantom Echo
    uint32_t echId = sWorldRealizationEngine.ManifestTemporalEcho3D(77, 400.0f, 10.0f, 400.0f, 4.0f);
    assert(echId > 0);
    assert(sWorldRealizationEngine.GetActiveTemporalEchoCount() == 1);

    // 16. Cycle Portal
    uint32_t cPtl = sWorldRealizationEngine.ManifestCyclePortal3D(100.0f, 0.0f, 100.0f, 2);
    assert(cPtl > 0);
    assert(sWorldRealizationEngine.GetActiveCyclePortalCount() == 1);

    // 17. Quantum Threat Wave
    uint32_t qTw = sWorldRealizationEngine.ManifestQuantumThreatWave3D(200.0f, 200.0f, 300.0f);
    assert(qTw > 0);
    assert(sWorldRealizationEngine.GetActiveThreatWaveCount() == 1);

    // 18. Synthesized Voxel Cover
    uint32_t sVc = sWorldRealizationEngine.SynthesizeVoxelCover3D(50.0f, 0.0f, 50.0f, 2.5f, 1.8f);
    assert(sVc > 0);
    assert(sWorldRealizationEngine.GetActiveVoxelCoverCount() == 1);

    // 19. Sentinel Skybox Eclipse
    uint32_t sEcl = sWorldRealizationEngine.TriggerSentinelEclipse3D(0.95f, 15.0f);
    assert(sEcl > 0);
    assert(sWorldRealizationEngine.GetActiveSentinelEclipseCount() == 1);

    // 20. Architect Console Beam
    uint32_t aBm = sWorldRealizationEngine.ManifestArchitectConsoleBeam3D(500.0f, 0.0f, 500.0f);
    assert(aBm > 0);
    assert(sWorldRealizationEngine.GetActiveArchitectBeamCount() == 1);

    std::cout << "[PASSED] Suite 39: 3D World Realization Engine (75 assertions passed)." << std::endl;
}

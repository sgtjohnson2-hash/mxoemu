#include "StructuralVoxelEngine.h"
#include "Log.h"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <iostream>

createFileSingleton(StructuralVoxelEngine);

StructuralVoxelEngine::StructuralVoxelEngine()
{
}

StructuralVoxelEngine::~StructuralVoxelEngine()
{
}

void StructuralVoxelEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_voxelMutex);
    m_nodes.clear();
    m_debrisParticles.clear();
    m_dustClouds.clear();
    m_dynamicObstacleNodeIds.clear();
    m_nextNodeId = 1;
    m_nextCloudId = 1;

    // Seed Landmark Structural Nodes across Megacity Districts
    // 1. Slums (District 1)
    uint32_t slumsPillar = m_nextNodeId++;
    m_nodes[slumsPillar] = {
        slumsPillar, 1, "LoadBearingColumn",
        -30434.0f, 150.0f, 20325.0f,
        150.0f, 400.0f, 150.0f,
        VoxelDamageState::INTACT, 100.0f, 85.0f, 55.0f,
        0, true, false, 0, 0.0f, false
    };

    uint32_t slumsFacade = m_nextNodeId++;
    m_nodes[slumsFacade] = {
        slumsFacade, 1, "ExteriorFacade",
        -30434.0f, 550.0f, 20325.0f,
        400.0f, 300.0f, 50.0f,
        VoxelDamageState::INTACT, 100.0f, 30.0f, 40.0f,
        slumsPillar, false, false, 0, 0.0f, false
    };

    // 2. Downtown (District 2 - MetaCortex Financial Center)
    uint32_t dtCoreCol1 = m_nextNodeId++;
    m_nodes[dtCoreCol1] = {
        dtCoreCol1, 2, "LoadBearingColumn",
        17043.0f, 200.0f, 2398.0f,
        250.0f, 600.0f, 250.0f,
        VoxelDamageState::INTACT, 100.0f, 150.0f, 85.0f,
        0, true, false, 0, 0.0f, false
    };

    uint32_t dtUpperFloor = m_nextNodeId++;
    m_nodes[dtUpperFloor] = {
        dtUpperFloor, 2, "FloorSlab",
        17043.0f, 800.0f, 2398.0f,
        600.0f, 50.0f, 600.0f,
        VoxelDamageState::INTACT, 100.0f, 90.0f, 50.0f,
        dtCoreCol1, false, false, 0, 0.0f, false
    };

    uint32_t dtGlassCurtain = m_nextNodeId++;
    m_nodes[dtGlassCurtain] = {
        dtGlassCurtain, 2, "GlassCurtainWall",
        17043.0f, 500.0f, 2998.0f,
        500.0f, 500.0f, 20.0f,
        VoxelDamageState::INTACT, 100.0f, 15.0f, 25.0f,
        0, false, false, 0, 0.0f, false
    };

    // 3. International (District 3 - Diplomatic Overpass & Monorail Bridge)
    uint32_t intlBridgeSpan = m_nextNodeId++;
    m_nodes[intlBridgeSpan] = {
        intlBridgeSpan, 3, "OverpassBridge",
        111180.0f, 800.0f, -40913.0f,
        1200.0f, 120.0f, 350.0f,
        VoxelDamageState::INTACT, 100.0f, 220.0f, 95.0f,
        0, true, false, 0, 0.0f, false
    };

    // 4. Richland (District 4 - Cathedral Vault & Subterranean Conduit)
    uint32_t richlandVault = m_nextNodeId++;
    m_nodes[richlandVault] = {
        richlandVault, 4, "SubwayVault",
        107619.0f, -450.0f, -149092.0f,
        800.0f, 300.0f, 800.0f,
        VoxelDamageState::INTACT, 100.0f, 350.0f, 110.0f,
        0, true, false, 0, 0.0f, false
    };

    INFO_LOG(format("StructuralVoxelEngine initialized with %1% baseline Megacity architectural nodes.") % m_nodes.size());
}

void StructuralVoxelEngine::ResetForTesting()
{
    Initialize();
}

uint32_t StructuralVoxelEngine::RegisterVoxelNode(uint32_t districtId, const std::string& type,
                                                  float x, float y, float z,
                                                  float hx, float hy, float hz,
                                                  float massTonnes, float yieldMpa,
                                                  bool isLoadBearing, uint32_t supportingPillarId)
{
    std::unique_lock<std::shared_mutex> lock(m_voxelMutex);
    uint32_t id = m_nextNodeId++;
    StructuralVoxelNode node;
    node.nodeId = id;
    node.districtId = districtId;
    node.structureType = type;
    node.posX = x;
    node.posY = y;
    node.posZ = z;
    node.halfExtentX = hx;
    node.halfExtentY = hy;
    node.halfExtentZ = hz;
    node.state = VoxelDamageState::INTACT;
    node.integrityPercent = 100.0f;
    node.supportedMassTonnes = massTonnes;
    node.yieldStressMpa = yieldMpa;
    node.isLoadBearing = isLoadBearing;
    node.supportingPillarId = supportingPillarId;
    node.navObstacleActive = false;
    node.lastDamageTimestamp = m_simulationTimestampSec;
    node.repairTimeRemainingSec = 0.0f;
    node.repairCrewDispatched = false;

    m_nodes[id] = node;
    return id;
}

const StructuralVoxelNode* StructuralVoxelEngine::GetNode(uint32_t nodeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t StructuralVoxelEngine::GetTotalNodeCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    return m_nodes.size();
}

size_t StructuralVoxelEngine::GetNodeCountByState(VoxelDamageState state) const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    size_t count = 0;
    for (const auto& kv : m_nodes) {
        if (kv.second.state == state) {
            ++count;
        }
    }
    return count;
}

BlastImpulseResult StructuralVoxelEngine::DetonateRadialImpulse(float blastX, float blastY, float blastZ,
                                                                float yieldJoules, float maxRadiusMeters)
{
    std::unique_lock<std::shared_mutex> lock(m_voxelMutex);
    BlastImpulseResult res;
    float maxRadiusUnits = maxRadiusMeters * 100.0f; // converts meters to world units
    float maxRadiusSq = maxRadiusUnits * maxRadiusUnits;

    std::vector<uint32_t> newlyCollapsed;

    for (auto& kv : m_nodes) {
        auto& node = kv.second;
        if (node.state == VoxelDamageState::COLLAPSED) continue;

        float dx = node.posX - blastX;
        float dy = node.posY - blastY;
        float dz = node.posZ - blastZ;
        float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq <= maxRadiusSq) {
            float distMeters = std::sqrt(distSq) / 100.0f;
            // Radial shockwave pressure falloff: P(r) = Yield / (1 + 0.05 * r^2) * 1e-5 MPa
            float pressureMpa = (yieldJoules / (1.0f + 0.05f * distMeters * distMeters)) * 1e-5f;
            if (pressureMpa > res.peakImpulsePressureMpa) {
                res.peakImpulsePressureMpa = pressureMpa;
            }

            res.nodesAffected++;
            node.lastDamageTimestamp = m_simulationTimestampSec;

            // Damage state transitions based on yield ratio
            float ratio = pressureMpa / (node.yieldStressMpa > 0.001f ? node.yieldStressMpa : 1.0f);
            if (ratio >= 0.85f) {
                node.state = VoxelDamageState::COLLAPSED;
                node.integrityPercent = 0.0f;
                res.nodesCollapsed++;
                newlyCollapsed.push_back(node.nodeId);
                InjectDynamicNavObstacle_Internal(node);
                res.dynamicObstaclesInjected++;

                // Spawn debris & aerosolized dust cloud
                float invDist = (distMeters > 0.1f) ? (1.0f / (distMeters * 100.0f)) : 1.0f;
                SpawnDebrisParticles(node.posX, node.posY, node.posZ, dx * invDist, dy * invDist + 0.5f, dz * invDist, 12);
                SpawnDustCloud(node.posX, node.posY, node.posZ, 25.0f, 95.0f);
            } else if (ratio >= 0.45f) {
                if (node.state < VoxelDamageState::SPALLED) {
                    node.state = VoxelDamageState::SPALLED;
                    res.nodesSpalled++;
                    node.integrityPercent = std::max(10.0f, node.integrityPercent - 55.0f);
                    SpawnDebrisParticles(node.posX, node.posY, node.posZ, 0.0f, 0.3f, 0.0f, 6);
                }
            } else if (ratio >= 0.15f) {
                if (node.state < VoxelDamageState::CRACKED) {
                    node.state = VoxelDamageState::CRACKED;
                    res.nodesCracked++;
                    node.integrityPercent = std::max(50.0f, node.integrityPercent - 25.0f);
                }
            }
        }
    }

    // Evaluate pancake cascades for newly collapsed load-bearing nodes
    for (uint32_t colId : newlyCollapsed) {
        EvaluatePancakeCollapse(colId);
    }

    INFO_LOG(format("DetonateRadialImpulse: Yield=%1%J at (%2%, %3%, %4%) -> Affected=%5%, Collapsed=%6%, Spalled=%7%, Cracked=%8%")
        % yieldJoules % blastX % blastY % blastZ % res.nodesAffected % res.nodesCollapsed % res.nodesSpalled % res.nodesCracked);

    return res;
}

bool StructuralVoxelEngine::ApplyKineticImpact(uint32_t nodeId, float kineticEnergyJoules,
                                               float hitDirX, float hitDirY, float hitDirZ)
{
    std::unique_lock<std::shared_mutex> lock(m_voxelMutex);
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) return false;

    auto& node = it->second;
    if (node.state == VoxelDamageState::COLLAPSED) return true;

    node.lastDamageTimestamp = m_simulationTimestampSec;
    // Kinetic energy in Joules converted to localized stress
    float stressMpa = (kineticEnergyJoules * 0.0002f);
    float ratio = stressMpa / (node.yieldStressMpa > 0.001f ? node.yieldStressMpa : 1.0f);

    if (ratio >= 0.80f) {
        node.state = VoxelDamageState::COLLAPSED;
        node.integrityPercent = 0.0f;
        InjectDynamicNavObstacle_Internal(node);
        SpawnDebrisParticles(node.posX, node.posY, node.posZ, hitDirX, hitDirY, hitDirZ, 10);
        SpawnDustCloud(node.posX, node.posY, node.posZ, 18.0f, 85.0f);
        EvaluatePancakeCollapse(node.nodeId);
    } else if (ratio >= 0.40f) {
        if (node.state < VoxelDamageState::SPALLED) {
            node.state = VoxelDamageState::SPALLED;
            node.integrityPercent = std::max(20.0f, node.integrityPercent - 45.0f);
            SpawnDebrisParticles(node.posX, node.posY, node.posZ, hitDirX, hitDirY, hitDirZ, 4);
        }
    } else if (ratio >= 0.05f) {
        if (node.state < VoxelDamageState::CRACKED) {
            node.state = VoxelDamageState::CRACKED;
            node.integrityPercent = std::max(60.0f, node.integrityPercent - 20.0f);
        }
    }

    return true;
}

uint32_t StructuralVoxelEngine::EvaluatePancakeCollapse(uint32_t collapsedNodeId)
{
    // If a load-bearing column collapses, unsupported upper structures lose equilibrium
    auto it = m_nodes.find(collapsedNodeId);
    if (it == m_nodes.end()) return 0;
    if (!it->second.isLoadBearing && it->second.state != VoxelDamageState::COLLAPSED) return 0;

    uint32_t cascadeCount = 0;
    std::vector<uint32_t> downstreamNodes;

    for (const auto& kv : m_nodes) {
        if (kv.second.supportingPillarId == collapsedNodeId && kv.second.state != VoxelDamageState::COLLAPSED) {
            downstreamNodes.push_back(kv.first);
        }
    }

    for (uint32_t childId : downstreamNodes) {
        auto& child = m_nodes[childId];
        child.state = VoxelDamageState::COLLAPSED;
        child.integrityPercent = 0.0f;
        child.lastDamageTimestamp = m_simulationTimestampSec;
        InjectDynamicNavObstacle_Internal(child);
        SpawnDebrisParticles(child.posX, child.posY, child.posZ, 0.0f, -1.0f, 0.0f, 15);
        SpawnDustCloud(child.posX, child.posY, child.posZ, 30.0f, 100.0f);
        cascadeCount++;
        // Recurse pancake propagation
        cascadeCount += EvaluatePancakeCollapse(childId);
    }

    if (cascadeCount > 0) {
        INFO_LOG(format("StructuralVoxelEngine: Pancake collapse cascade triggered by pillar #%1% -> %2% downstream structures collapsed.")
            % collapsedNodeId % cascadeCount);
    }
    return cascadeCount;
}

void StructuralVoxelEngine::InjectDynamicNavObstacle_Internal(StructuralVoxelNode& node)
{
    if (node.navObstacleActive) return;
    node.navObstacleActive = true;
    m_dynamicObstacleNodeIds.push_back(node.nodeId);
    INFO_LOG(format("Dynamic NavMesh: Injected rubble obstacle for collapsed node #%1% at (%2%, %3%, %4%)")
        % node.nodeId % node.posX % node.posY % node.posZ);
}

void StructuralVoxelEngine::RemoveDynamicNavObstacle_Internal(StructuralVoxelNode& node)
{
    if (!node.navObstacleActive) return;
    node.navObstacleActive = false;
    auto it = std::remove(m_dynamicObstacleNodeIds.begin(), m_dynamicObstacleNodeIds.end(), node.nodeId);
    if (it != m_dynamicObstacleNodeIds.end()) {
        m_dynamicObstacleNodeIds.erase(it, m_dynamicObstacleNodeIds.end());
    }
    INFO_LOG(format("Dynamic NavMesh: Removed rubble obstacle for repaired node #%1% at (%2%, %3%, %4%)")
        % node.nodeId % node.posX % node.posY % node.posZ);
}

bool StructuralVoxelEngine::IsAreaBlockedByRubble(float x, float y, float z, float queryRadius) const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    for (uint32_t id : m_dynamicObstacleNodeIds) {
        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            const auto& node = it->second;
            float minX = node.posX - node.halfExtentX;
            float maxX = node.posX + node.halfExtentX;
            float minY = node.posY - node.halfExtentY;
            float maxY = node.posY + node.halfExtentY;
            float minZ = node.posZ - node.halfExtentZ;
            float maxZ = node.posZ + node.halfExtentZ;

            // Clamped AABB-sphere test
            float closestX = std::max(minX, std::min(x, maxX));
            float closestY = std::max(minY, std::min(y, maxY));
            float closestZ = std::max(minZ, std::min(z, maxZ));

            float dx = x - closestX;
            float dy = y - closestY;
            float dz = z - closestZ;
            if ((dx * dx + dy * dy + dz * dz) <= (queryRadius * queryRadius)) {
                return true; // Blocked by rubble!
            }
        }
    }
    return false;
}

std::vector<uint32_t> StructuralVoxelEngine::GetActiveDynamicObstacleNodeIds() const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    return m_dynamicObstacleNodeIds;
}

void StructuralVoxelEngine::SpawnDebrisParticles(float x, float y, float z, float dirX, float dirY, float dirZ, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        VoxelDebrisParticle p;
        p.posX = x;
        p.posY = y;
        p.posZ = z;
        float speed = 250.0f + (i * 30.0f);
        p.velX = dirX * speed + ((i % 3) - 1.0f) * 40.0f;
        p.velY = dirY * speed + 60.0f;
        p.velZ = dirZ * speed + ((i % 5) - 2.0f) * 35.0f;
        p.massKg = 15.0f + (i * 5.0f);
        p.lifeRemainingSec = 3.5f + (i * 0.2f);
        m_debrisParticles.push_back(p);
    }
}

void StructuralVoxelEngine::SpawnDustCloud(float x, float y, float z, float radius, float density)
{
    DustCloudEmitter cloud;
    cloud.cloudId = m_nextCloudId++;
    cloud.posX = x;
    cloud.posY = y;
    cloud.posZ = z;
    cloud.radiusMeters = radius;
    cloud.opticalDensityPercent = density;
    cloud.lifeRemainingSec = 20.0f;
    m_dustClouds.push_back(cloud);
}

float StructuralVoxelEngine::GetOpticalDensityAtPoint(float x, float y, float z) const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    float totalDensity = 0.0f;
    for (const auto& cloud : m_dustClouds) {
        float dx = x - cloud.posX;
        float dy = y - cloud.posY;
        float dz = z - cloud.posZ;
        float distSq = dx * dx + dy * dy + dz * dz;
        float radUnits = cloud.radiusMeters * 100.0f;
        if (distSq <= (radUnits * radUnits)) {
            float dist = std::sqrt(distSq);
            float falloff = 1.0f - (dist / radUnits);
            totalDensity += cloud.opticalDensityPercent * falloff;
        }
    }
    return totalDensity;
}

size_t StructuralVoxelEngine::GetActiveDustCloudCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    return m_dustClouds.size();
}

size_t StructuralVoxelEngine::GetActiveDebrisCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    return m_debrisParticles.size();
}

std::vector<uint8_t> StructuralVoxelEngine::SerializeDamageDelta(const VoxelDamageDeltaPacket& packet) const
{
    std::vector<uint8_t> buffer(sizeof(VoxelDamageDeltaPacket));
    std::memcpy(buffer.data(), &packet, sizeof(VoxelDamageDeltaPacket));
    return buffer;
}

bool StructuralVoxelEngine::DeserializeDamageDelta(const std::vector<uint8_t>& data, VoxelDamageDeltaPacket& outPacket) const
{
    if (data.size() < sizeof(VoxelDamageDeltaPacket)) return false;
    std::memcpy(&outPacket, data.data(), sizeof(VoxelDamageDeltaPacket));
    return true;
}

std::string StructuralVoxelEngine::GenerateDatabaseSchemaSql() const
{
    return "CREATE TABLE IF NOT EXISTS `megacity_structural_damage` (\n"
           "  `node_id` INT UNSIGNED NOT NULL,\n"
           "  `district_id` INT UNSIGNED NOT NULL DEFAULT 1,\n"
           "  `damage_state` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0:INTACT, 1:CRACKED, 2:SPALLED, 3:COLLAPSED',\n"
           "  `integrity_percent` FLOAT NOT NULL DEFAULT 100.0,\n"
           "  `last_damage_timestamp` INT UNSIGNED NOT NULL DEFAULT 0,\n"
           "  `repair_time_remaining` FLOAT NOT NULL DEFAULT 0.0,\n"
           "  `nav_obstacle_active` TINYINT UNSIGNED NOT NULL DEFAULT 0,\n"
           "  PRIMARY KEY (`node_id`),\n"
           "  KEY `idx_district_state` (`district_id`, `damage_state`)\n"
           ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;\n";
}

std::string StructuralVoxelEngine::ExportDamageStateSql() const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    std::ostringstream ss;
    ss << "INSERT INTO `megacity_structural_damage` (node_id, district_id, damage_state, integrity_percent, last_damage_timestamp, repair_time_remaining, nav_obstacle_active) VALUES \n";
    bool first = true;
    for (const auto& kv : m_nodes) {
        const auto& n = kv.second;
        if (!first) ss << ",\n";
        first = false;
        ss << "(" << n.nodeId << ", " << n.districtId << ", " << static_cast<int>(n.state)
           << ", " << n.integrityPercent << ", " << n.lastDamageTimestamp << ", " << n.repairTimeRemainingSec
           << ", " << (n.navObstacleActive ? 1 : 0) << ")";
    }
    ss << "\nON DUPLICATE KEY UPDATE damage_state=VALUES(damage_state), integrity_percent=VALUES(integrity_percent), "
       << "last_damage_timestamp=VALUES(last_damage_timestamp), repair_time_remaining=VALUES(repair_time_remaining), "
       << "nav_obstacle_active=VALUES(nav_obstacle_active);\n";
    return ss.str();
}

void StructuralVoxelEngine::DispatchRepairCrew(uint32_t nodeId, float repairDurationSec)
{
    std::unique_lock<std::shared_mutex> lock(m_voxelMutex);
    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        it->second.repairCrewDispatched = true;
        it->second.repairTimeRemainingSec = repairDurationSec;
        INFO_LOG(format("StructuralVoxelEngine: NPC construction repair crew dispatched to node #%1% (%2%s est).")
            % nodeId % repairDurationSec);
    }
}

size_t StructuralVoxelEngine::GetActiveRepairCrewCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_voxelMutex);
    size_t count = 0;
    for (const auto& kv : m_nodes) {
        if (kv.second.repairCrewDispatched) {
            count++;
        }
    }
    return count;
}

void StructuralVoxelEngine::Update(float deltaSeconds)
{
    std::unique_lock<std::shared_mutex> lock(m_voxelMutex);
    m_simulationTimestampSec += static_cast<uint32_t>(deltaSeconds);

    // 1. Update Debris Particles
    for (auto it = m_debrisParticles.begin(); it != m_debrisParticles.end();) {
        it->lifeRemainingSec -= deltaSeconds;
        if (it->lifeRemainingSec <= 0.0f) {
            it = m_debrisParticles.erase(it);
        } else {
            it->posX += it->velX * deltaSeconds;
            it->posY += it->velY * deltaSeconds;
            it->posZ += it->velZ * deltaSeconds;
            it->velY -= 980.0f * deltaSeconds; // Gravity
            ++it;
        }
    }

    // 2. Update Dust Clouds & Dissipation
    for (auto it = m_dustClouds.begin(); it != m_dustClouds.end();) {
        it->lifeRemainingSec -= deltaSeconds;
        if (it->lifeRemainingSec <= 0.0f) {
            it = m_dustClouds.erase(it);
        } else {
            it->opticalDensityPercent *= (1.0f - std::min(0.9f, 0.10f * deltaSeconds)); // Gradual dissipation
            ++it;
        }
    }

    // 3. Update NPC Construction Repair Crews
    for (auto& kv : m_nodes) {
        auto& node = kv.second;
        if (node.repairCrewDispatched && node.repairTimeRemainingSec > 0.0f) {
            node.repairTimeRemainingSec -= deltaSeconds;
            // Incremental structural knitting
            node.integrityPercent = std::min(100.0f, node.integrityPercent + (25.0f * deltaSeconds));

            // State promotion
            if (node.integrityPercent >= 90.0f) {
                if (node.state != VoxelDamageState::INTACT) {
                    node.state = VoxelDamageState::INTACT;
                    RemoveDynamicNavObstacle_Internal(node);
                }
            } else if (node.integrityPercent >= 50.0f) {
                if (node.state > VoxelDamageState::CRACKED) {
                    node.state = VoxelDamageState::CRACKED;
                    RemoveDynamicNavObstacle_Internal(node);
                }
            } else if (node.integrityPercent >= 20.0f) {
                if (node.state > VoxelDamageState::SPALLED) {
                    node.state = VoxelDamageState::SPALLED;
                }
            }

            if (node.repairTimeRemainingSec <= 0.0f) {
                node.repairCrewDispatched = false;
                node.integrityPercent = 100.0f;
                node.state = VoxelDamageState::INTACT;
                RemoveDynamicNavObstacle_Internal(node);
                INFO_LOG(format("StructuralVoxelEngine: Node #%1% fully restored to INTACT by repair crew.") % node.nodeId);
            }
        }
    }
}

// ============================================================================
// Test Suite 28: Epoch V Persistent Structural Voxel Rupture & Dynamic NavMesh
// ============================================================================

void RunStructuralVoxelTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH V: STRUCTURAL VOXEL & DYNAMIC NAVMESH SUITE" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto assert_test = [&](bool cond, const std::string& desc) {
        if (cond) {
            std::cout << " [PASS] " << desc << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << desc << std::endl;
            failed++;
        }
    };

    sStructuralVoxelEngine.ResetForTesting();

    // 1. Initial State & Baseline Landmarks
    size_t totalNodes = sStructuralVoxelEngine.GetTotalNodeCount();
    assert_test(totalNodes >= 7, "Baseline Megacity structural nodes initialized (minimum 7 landmarks)");
    assert_test(sStructuralVoxelEngine.GetNodeCountByState(VoxelDamageState::INTACT) == totalNodes,
                "All baseline landmarks start in INTACT state");
    assert_test(sStructuralVoxelEngine.GetActiveDynamicObstacleNodeIds().empty(),
                "Initial dynamic NavMesh obstacle list is empty");

    // 2. Custom Structural Registration
    uint32_t towerPillar = sStructuralVoxelEngine.RegisterVoxelNode(
        2, "TowerFoundationPillar",
        1000.0f, 100.0f, 1000.0f,
        200.0f, 500.0f, 200.0f,
        180.0f, 75.0f, true, 0
    );
    assert_test(towerPillar > 0, "Registered TowerFoundationPillar in Downtown");

    uint32_t upperSpire = sStructuralVoxelEngine.RegisterVoxelNode(
        2, "UpperSpireFacade",
        1000.0f, 700.0f, 1000.0f,
        400.0f, 200.0f, 400.0f,
        60.0f, 40.0f, false, towerPillar
    );
    assert_test(upperSpire > 0, "Registered UpperSpireFacade with dependency on foundation pillar");

    // 3. High-Caliber Direct Kinetic Impact
    // Barrett M82 .50 BMG round (~18,000 J) against upper spire
    bool hitOk = sStructuralVoxelEngine.ApplyKineticImpact(upperSpire, 18000.0f, 1.0f, 0.0f, 0.0f);
    assert_test(hitOk, "Applied high-caliber kinetic impact to UpperSpireFacade");
    const auto* spireNode = sStructuralVoxelEngine.GetNode(upperSpire);
    assert_test(spireNode != nullptr && spireNode->state == VoxelDamageState::CRACKED,
                "UpperSpireFacade transitioned to CRACKED under M82 anti-materiel strike");
    assert_test(spireNode->integrityPercent < 100.0f, "Structural integrity reduced following kinetic crack");

    // 4. Heavy Kinetic Sledge Slam -> SPALLED
    sStructuralVoxelEngine.ApplyKineticImpact(upperSpire, 120000.0f, 0.0f, 0.0f, 1.0f);
    spireNode = sStructuralVoxelEngine.GetNode(upperSpire);
    assert_test(spireNode != nullptr && spireNode->state == VoxelDamageState::SPALLED,
                "UpperSpireFacade transitioned to SPALLED under heavy kinetic overload");
    assert_test(sStructuralVoxelEngine.GetActiveDebrisCount() > 0,
                "Debris particles spawned following spalling fracture");

    // 5. High-Yield Radial Explosive Detonation (C4 Demolition Blast: 25,000,000 Joules)
    BlastImpulseResult blast = sStructuralVoxelEngine.DetonateRadialImpulse(
        1000.0f, 100.0f, 1000.0f, 25000000.0f, 30.0f
    );
    assert_test(blast.nodesAffected >= 1, "Radial blast affected structural nodes in proximity");
    assert_test(blast.nodesCollapsed >= 1, "Direct foundation pillar collapsed from radial blast pressure");
    assert_test(blast.dynamicObstaclesInjected >= 1, "Collapsed pillar injected dynamic NavMesh obstacle");

    const auto* pillarNode = sStructuralVoxelEngine.GetNode(towerPillar);
    assert_test(pillarNode != nullptr && pillarNode->state == VoxelDamageState::COLLAPSED,
                "TowerFoundationPillar confirmed in COLLAPSED state");
    assert_test(pillarNode->navObstacleActive == true, "NavMesh obstacle active flag set on collapsed pillar");

    // 6. Progressive Pancake Collapse Cascade
    spireNode = sStructuralVoxelEngine.GetNode(upperSpire);
    assert_test(spireNode != nullptr && spireNode->state == VoxelDamageState::COLLAPSED,
                "UpperSpireFacade experienced progressive pancake collapse upon parent pillar destruction");

    // 7. Dynamic NavMesh Query & Rubble Obstruction
    bool isBlocked = sStructuralVoxelEngine.IsAreaBlockedByRubble(1000.0f, 100.0f, 1000.0f, 50.0f);
    assert_test(isBlocked, "AI path query reports area blocked by collapsed rubble pile");

    bool clearAreaBlocked = sStructuralVoxelEngine.IsAreaBlockedByRubble(50000.0f, 100.0f, 50000.0f, 50.0f);
    assert_test(!clearAreaBlocked, "Clear street area unaffected by distant rubble");

    // 8. Dust Cloud Optical Density & Dissipation
    size_t dustCount = sStructuralVoxelEngine.GetActiveDustCloudCount();
    assert_test(dustCount >= 1, "Explosive collapse spawned aerosolized dust clouds");
    float densityAtGroundZero = sStructuralVoxelEngine.GetOpticalDensityAtPoint(1000.0f, 100.0f, 1000.0f);
    assert_test(densityAtGroundZero > 50.0f, "High optical density (vision obstruction) detected at ground zero");

    // Update simulation by 5 seconds
    sStructuralVoxelEngine.Update(5.0f);
    float densityAfter5s = sStructuralVoxelEngine.GetOpticalDensityAtPoint(1000.0f, 100.0f, 1000.0f);
    assert_test(densityAfter5s < densityAtGroundZero, "Dust cloud optical density dissipated over simulation tick");

    // 9. Delta-Compressed Binary Network Replication (<= 12 Bytes)
    VoxelDamageDeltaPacket txPacket;
    txPacket.nodeId = towerPillar;
    txPacket.damageState = static_cast<uint8_t>(VoxelDamageState::COLLAPSED);
    txPacket.integrityPercent = 0;
    txPacket.impulseEnergyKJ = 25000;
    txPacket.timestamp = 1725753605;

    std::vector<uint8_t> wireData = sStructuralVoxelEngine.SerializeDamageDelta(txPacket);
    assert_test(wireData.size() == 12, "Serialized delta packet is exactly 12 bytes");

    VoxelDamageDeltaPacket rxPacket;
    bool rxOk = sStructuralVoxelEngine.DeserializeDamageDelta(wireData, rxPacket);
    assert_test(rxOk, "Deserialized delta packet successfully");
    assert_test(rxPacket.nodeId == towerPillar, "Deserialized nodeId matches original");
    assert_test(rxPacket.damageState == static_cast<uint8_t>(VoxelDamageState::COLLAPSED), "Deserialized damage state matches");
    assert_test(rxPacket.impulseEnergyKJ == 25000, "Deserialized impulse energy matches");

    // 10. MariaDB Persistence Generation
    std::string schemaSql = sStructuralVoxelEngine.GenerateDatabaseSchemaSql();
    assert_test(schemaSql.find("CREATE TABLE IF NOT EXISTS `megacity_structural_damage`") != std::string::npos,
                "Generated valid MariaDB structural damage schema SQL");

    std::string exportSql = sStructuralVoxelEngine.ExportDamageStateSql();
    assert_test(exportSql.find("INSERT INTO `megacity_structural_damage`") != std::string::npos,
                "Generated valid damage state SQL export query");

    // 11. NPC Construction Repair Crew Dispatch
    sStructuralVoxelEngine.DispatchRepairCrew(towerPillar, 4.0f);
    assert_test(sStructuralVoxelEngine.GetActiveRepairCrewCount() == 1,
                "Repair crew dispatched and actively knitting tower foundation");

    // Advance simulation to complete repair
    sStructuralVoxelEngine.Update(5.0f);
    pillarNode = sStructuralVoxelEngine.GetNode(towerPillar);
    assert_test(pillarNode != nullptr && pillarNode->state == VoxelDamageState::INTACT,
                "Repair crew fully restored TowerFoundationPillar to INTACT state");
    assert_test(pillarNode->integrityPercent >= 100.0f, "Integrity percent restored to 100%");
    assert_test(pillarNode->navObstacleActive == false, "Dynamic NavMesh rubble obstacle automatically cleared");

    bool stillBlocked = sStructuralVoxelEngine.IsAreaBlockedByRubble(1000.0f, 100.0f, 1000.0f, 50.0f);
    assert_test(!stillBlocked, "NavMesh traversability restored for AI agents");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH V STRUCTURAL VOXEL TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

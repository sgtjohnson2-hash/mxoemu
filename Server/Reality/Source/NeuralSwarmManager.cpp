#include "NeuralSwarmManager.h"
#include "Log.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

createFileSingleton(NeuralSwarmManager);

NeuralSwarmManager::NeuralSwarmManager()
{
}

NeuralSwarmManager::~NeuralSwarmManager()
{
}

void NeuralSwarmManager::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);
    m_boids.clear();
    m_targetGroups.clear();
    m_params = SwarmFlockingParams();
    INFO_LOG("NeuralSwarmManager: Initialized Epoch V 3D Boids & Swarm AI Engine.");
}

void NeuralSwarmManager::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);
    m_boids.clear();
    m_targetGroups.clear();
}

bool NeuralSwarmManager::RegisterSwarmEntity(uint32 entityGoId, SwarmEntityType type, float x, float y, float z)
{
    if (entityGoId == 0) return false;
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);

    SwarmBoidNode boid;
    boid.entityGoId = entityGoId;
    boid.type = type;
    boid.posX = x;
    boid.posY = y;
    boid.posZ = z;
    boid.velX = 0.0f;
    boid.velY = 0.0f;
    boid.velZ = 0.0f;
    boid.active = true;

    if (type == SWARM_ENTITY_SENTINEL_SWARM) {
        boid.maxSpeed = 550.0f;     // Faster in 3D subterranean pipes
        boid.personalRadius = 140.0f;
        boid.role = ROLE_CUTTER_STRIKE;
    } else if (type == SWARM_ENTITY_HUNTER_KILLER) {
        boid.maxSpeed = 400.0f;
        boid.personalRadius = 250.0f;
        boid.role = ROLE_HARASSER;
    } else {
        boid.maxSpeed = 360.0f;     // Smith crowd runner
        boid.personalRadius = 75.0f;
        boid.role = ROLE_INTERLOCK_PIN;
    }

    m_boids[entityGoId] = boid;
    return true;
}

bool NeuralSwarmManager::UnregisterSwarmEntity(uint32 entityGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);
    auto it = m_boids.find(entityGoId);
    if (it == m_boids.end()) return false;

    uint32 tgtId = it->second.targetGoId;
    m_boids.erase(it);

    if (tgtId != 0) {
        auto gIt = m_targetGroups.find(tgtId);
        if (gIt != m_targetGroups.end()) {
            auto& vec = gIt->second.assignedBoidIds;
            vec.erase(std::remove(vec.begin(), vec.end(), entityGoId), vec.end());
            if (vec.empty()) {
                m_targetGroups.erase(gIt);
            }
        }
    }
    return true;
}

bool NeuralSwarmManager::SetEntityTarget(uint32 entityGoId, uint32 targetGoId, float tx, float ty, float tz)
{
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);
    auto it = m_boids.find(entityGoId);
    if (it == m_boids.end()) return false;

    uint32 oldTgt = it->second.targetGoId;
    if (oldTgt != targetGoId && oldTgt != 0) {
        auto gIt = m_targetGroups.find(oldTgt);
        if (gIt != m_targetGroups.end()) {
            auto& vec = gIt->second.assignedBoidIds;
            vec.erase(std::remove(vec.begin(), vec.end(), entityGoId), vec.end());
            if (vec.empty()) m_targetGroups.erase(gIt);
        }
    }

    it->second.targetGoId = targetGoId;
    it->second.targetX = tx;
    it->second.targetY = ty;
    it->second.targetZ = tz;

    if (targetGoId != 0) {
        auto& group = m_targetGroups[targetGoId];
        group.targetGoId = targetGoId;
        group.x = tx;
        group.y = ty;
        group.z = tz;
        if (std::find(group.assignedBoidIds.begin(), group.assignedBoidIds.end(), entityGoId) == group.assignedBoidIds.end()) {
            group.assignedBoidIds.push_back(entityGoId);
        }
    }
    return true;
}

size_t NeuralSwarmManager::GetActiveSwarmCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_swarmMutex);
    return m_boids.size();
}

size_t NeuralSwarmManager::GetCountByType(SwarmEntityType type) const
{
    std::shared_lock<std::shared_mutex> lock(m_swarmMutex);
    size_t c = 0;
    for (const auto& pair : m_boids) {
        if (pair.second.type == type) c++;
    }
    return c;
}

const SwarmBoidNode* NeuralSwarmManager::GetBoidNode(uint32 entityGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_swarmMutex);
    auto it = m_boids.find(entityGoId);
    if (it != m_boids.end()) return &it->second;
    return nullptr;
}

void NeuralSwarmManager::CalculateEncirclementPositions(uint32 targetGoId, float tx, float ty, float tz, float orbitRadius)
{
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);
    auto gIt = m_targetGroups.find(targetGoId);
    if (gIt == m_targetGroups.end() || gIt->second.assignedBoidIds.empty()) return;

    gIt->second.x = tx;
    gIt->second.y = ty;
    gIt->second.z = tz;

    const auto& ids = gIt->second.assignedBoidIds;
    size_t count = ids.size();
    float angleStep = (2.0f * 3.14159265f) / static_cast<float>(count);

    for (size_t i = 0; i < count; ++i) {
        auto bIt = m_boids.find(ids[i]);
        if (bIt != m_boids.end()) {
            bIt->second.assignedOrbitAngle = static_cast<float>(i) * angleStep;
            bIt->second.assignedOrbitRadius = orbitRadius;
            bIt->second.targetX = tx;
            bIt->second.targetY = ty;
            bIt->second.targetZ = tz;
        }
    }
}

void NeuralSwarmManager::Update(float deltaSeconds)
{
    if (deltaSeconds <= 0.0f) return;
    std::unique_lock<std::shared_mutex> lock(m_swarmMutex);

    if (m_boids.empty()) return;

    // 1. Coordinated Encirclement Positioning
    for (auto& gPair : m_targetGroups) {
        const auto& ids = gPair.second.assignedBoidIds;
        size_t count = ids.size();
        if (count == 0) continue;
        float angleStep = (2.0f * 3.14159265f) / static_cast<float>(count);
        for (size_t i = 0; i < count; ++i) {
            auto bIt = m_boids.find(ids[i]);
            if (bIt != m_boids.end()) {
                bIt->second.assignedOrbitAngle = static_cast<float>(i) * angleStep;
                bIt->second.targetX = gPair.second.x;
                bIt->second.targetY = gPair.second.y;
                bIt->second.targetZ = gPair.second.z;
            }
        }
    }

    // 2. Compute 3D Boids Kinematics
    StepBoidsSIMD(deltaSeconds);
}

void NeuralSwarmManager::StepBoidsSIMD(float deltaSeconds)
{
    // Accumulate forces per boid
    for (auto& pair : m_boids) {
        SwarmBoidNode& b = pair.second;
        if (!b.active) continue;

        float forceX = 0.0f, forceY = 0.0f, forceZ = 0.0f;

        // A. Separation & Cohesion & Alignment across neighbors
        float sepX = 0.0f, sepY = 0.0f, sepZ = 0.0f;
        float alignX = 0.0f, alignY = 0.0f, alignZ = 0.0f;
        float cohX = 0.0f, cohY = 0.0f, cohZ = 0.0f;
        int neighborCount = 0;

        for (const auto& otherPair : m_boids) {
            if (otherPair.first == b.entityGoId) continue;
            const SwarmBoidNode& other = otherPair.second;
            if (other.type != b.type) continue; // Only flock with same swarm type

            float dx = b.posX - other.posX;
            float dy = b.posY - other.posY;
            float dz = b.posZ - other.posZ;
            float distSq = dx * dx + dy * dy + dz * dz;

            if (distSq < m_params.neighborRadius * m_params.neighborRadius && distSq > 0.001f) {
                float dist = std::sqrt(distSq);
                
                // Separation: Inversely proportional to distance
                if (dist < b.personalRadius) {
                    float repulse = (b.personalRadius - dist) / b.personalRadius;
                    sepX += (dx / dist) * repulse;
                    sepY += (dy / dist) * repulse;
                    sepZ += (dz / dist) * repulse;
                }

                // Alignment
                alignX += other.velX;
                alignY += other.velY;
                alignZ += other.velZ;

                // Cohesion
                cohX += other.posX;
                cohY += other.posY;
                cohZ += other.posZ;

                neighborCount++;
            }
        }

        if (neighborCount > 0) {
            // Normalize & Scale Alignment
            alignX /= static_cast<float>(neighborCount);
            alignY /= static_cast<float>(neighborCount);
            alignZ /= static_cast<float>(neighborCount);

            // Cohesion: Vector toward center of mass
            cohX = (cohX / static_cast<float>(neighborCount)) - b.posX;
            cohY = (cohY / static_cast<float>(neighborCount)) - b.posY;
            cohZ = (cohZ / static_cast<float>(neighborCount)) - b.posZ;
        }

        forceX += sepX * m_params.separationWeight * b.maxForce;
        forceY += sepY * m_params.separationWeight * b.maxForce;
        forceZ += sepZ * m_params.separationWeight * b.maxForce;

        forceX += alignX * m_params.alignmentWeight;
        forceY += alignY * m_params.alignmentWeight;
        forceZ += alignZ * m_params.alignmentWeight;

        forceX += cohX * m_params.cohesionWeight;
        forceY += cohY * m_params.cohesionWeight;
        forceZ += cohZ * m_params.cohesionWeight;

        // B. Target Seeking / Encirclement Steering
        if (b.targetGoId != 0) {
            float desiredPosX = b.targetX + std::cos(b.assignedOrbitAngle) * b.assignedOrbitRadius;
            float desiredPosZ = b.targetZ + std::sin(b.assignedOrbitAngle) * b.assignedOrbitRadius;
            float desiredPosY = b.targetY;
            if (b.type == SWARM_ENTITY_SENTINEL_SWARM) {
                desiredPosY += 150.0f; // Maintain high ground in 3D aerial space
            }

            float toTgtX = desiredPosX - b.posX;
            float toTgtY = desiredPosY - b.posY;
            float toTgtZ = desiredPosZ - b.posZ;
            float distToTarget = std::sqrt(toTgtX * toTgtX + toTgtY * toTgtY + toTgtZ * toTgtZ);

            if (distToTarget > 10.0f) {
                float seekVelX = (toTgtX / distToTarget) * b.maxSpeed;
                float seekVelY = (toTgtY / distToTarget) * b.maxSpeed;
                float seekVelZ = (toTgtZ / distToTarget) * b.maxSpeed;

                forceX += (seekVelX - b.velX) * m_params.targetSeekWeight;
                forceY += (seekVelY - b.velY) * m_params.targetSeekWeight;
                forceZ += (seekVelZ - b.velZ) * m_params.targetSeekWeight;
            }
        }

        // C. Sentinel Specific Cutting Lasers
        if (b.type == SWARM_ENTITY_SENTINEL_SWARM) {
            ApplySentinelAerialDynamics(b, deltaSeconds);
        }

        // Integrate Acceleration
        b.accX = forceX;
        b.accY = forceY;
        b.accZ = forceZ;

        // Clamp Force
        float forceMag = std::sqrt(b.accX * b.accX + b.accY * b.accY + b.accZ * b.accZ);
        if (forceMag > b.maxForce) {
            b.accX = (b.accX / forceMag) * b.maxForce;
            b.accY = (b.accY / forceMag) * b.maxForce;
            b.accZ = (b.accZ / forceMag) * b.maxForce;
        }

        // Integrate Velocity
        b.velX += b.accX * deltaSeconds;
        b.velY += b.accY * deltaSeconds;
        b.velZ += b.accZ * deltaSeconds;

        // Clamp Speed
        float speed = std::sqrt(b.velX * b.velX + b.velY * b.velY + b.velZ * b.velZ);
        if (speed > b.maxSpeed) {
            b.velX = (b.velX / speed) * b.maxSpeed;
            b.velY = (b.velY / speed) * b.maxSpeed;
            b.velZ = (b.velZ / speed) * b.maxSpeed;
        }

        // Integrate Position
        b.posX += b.velX * deltaSeconds;
        b.posY += b.velY * deltaSeconds;
        b.posZ += b.velZ * deltaSeconds;
    }
}

void NeuralSwarmManager::ApplySentinelAerialDynamics(SwarmBoidNode& boid, float deltaSeconds)
{
    if (boid.targetGoId == 0) {
        boid.isFiringPlasmaCutter = false;
        return;
    }

    float dx = boid.targetX - boid.posX;
    float dy = boid.targetY - boid.posY;
    float dz = boid.targetZ - boid.posZ;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist <= boid.cutterLength && boid.tentacleEnergy > 15.0f) {
        boid.isFiringPlasmaCutter = true;
        boid.tentacleEnergy -= 8.0f * deltaSeconds;
    } else {
        boid.isFiringPlasmaCutter = false;
        boid.tentacleEnergy = std::min(100.0f, boid.tentacleEnergy + 12.0f * deltaSeconds);
    }
}

std::vector<NeuralSwarmManager::PlasmaCutterRay> NeuralSwarmManager::GetActivePlasmaRays() const
{
    std::shared_lock<std::shared_mutex> lock(m_swarmMutex);
    std::vector<PlasmaCutterRay> rays;

    for (const auto& pair : m_boids) {
        const SwarmBoidNode& b = pair.second;
        if (b.type == SWARM_ENTITY_SENTINEL_SWARM && b.isFiringPlasmaCutter) {
            float dx = b.targetX - b.posX;
            float dy = b.targetY - b.posY;
            float dz = b.targetZ - b.posZ;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > 0.001f) {
                PlasmaCutterRay ray;
                ray.sentinelGoId = b.entityGoId;
                ray.originX = b.posX;
                ray.originY = b.posY;
                ray.originZ = b.posZ;
                ray.dirX = dx / dist;
                ray.dirY = dy / dist;
                ray.dirZ = dz / dist;
                ray.range = dist;
                ray.thermalDamagePerSec = 75.0f;
                rays.push_back(ray);
            }
        }
    }
    return rays;
}

std::string NeuralSwarmManager::GenerateTelemetryJson() const
{
    std::shared_lock<std::shared_mutex> lock(m_swarmMutex);
    std::ostringstream ss;
    ss << "{\"totalBoids\": " << m_boids.size()
       << ", \"targetGroups\": " << m_targetGroups.size()
       << ", \"smithClones\": " << GetCountByType(SWARM_ENTITY_SMITH_CLONE)
       << ", \"sentinels\": " << GetCountByType(SWARM_ENTITY_SENTINEL_SWARM)
       << ", \"activeCutters\": " << GetActivePlasmaRays().size()
       << "}";
    return ss.str();
}

#include "AgentSmithCascadeEngine.h"
#include <iostream>

void RunNeuralSwarmTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH V: NEURAL SWARM & SMITH CASCADE TEST SUITE (SUITE 27)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;
    auto assertTest = [&](const std::string& name, bool condition) {
        if (condition) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cerr << " [FAIL] " << name << std::endl;
            failed++;
        }
    };

    sNeuralSwarmMgr.ResetForTesting();
    sSmithCascadeEngine.ResetForTesting();

    // 1. Initial State
    assertTest("Initial swarm boid count is 0", sNeuralSwarmMgr.GetActiveSwarmCount() == 0);
    assertTest("Initial global Smith clones is 0", sSmithCascadeEngine.GetTotalGlobalSmithClones() == 0);

    // 2. Swarm Registration
    bool reg1 = sNeuralSwarmMgr.RegisterSwarmEntity(7001, SWARM_ENTITY_SMITH_CLONE, 1000.0f, 0.0f, 2000.0f);
    bool reg2 = sNeuralSwarmMgr.RegisterSwarmEntity(7002, SWARM_ENTITY_SMITH_CLONE, 1050.0f, 0.0f, 2020.0f);
    bool reg3 = sNeuralSwarmMgr.RegisterSwarmEntity(7003, SWARM_ENTITY_SENTINEL_SWARM, 1500.0f, 300.0f, 2500.0f);
    assertTest("Registered Smith clone 1", reg1);
    assertTest("Registered Smith clone 2", reg2);
    assertTest("Registered Sentinel boid", reg3);
    assertTest("Total swarm count is 3", sNeuralSwarmMgr.GetActiveSwarmCount() == 3);
    assertTest("Smith clone count is 2", sNeuralSwarmMgr.GetCountByType(SWARM_ENTITY_SMITH_CLONE) == 2);
    assertTest("Sentinel boid count is 1", sNeuralSwarmMgr.GetCountByType(SWARM_ENTITY_SENTINEL_SWARM) == 1);

    // 3. Target Acquisition & Coordinated Encirclement
    sNeuralSwarmMgr.SetEntityTarget(7001, 9999, 1000.0f, 0.0f, 2000.0f);
    sNeuralSwarmMgr.SetEntityTarget(7002, 9999, 1000.0f, 0.0f, 2000.0f);
    sNeuralSwarmMgr.CalculateEncirclementPositions(9999, 1000.0f, 0.0f, 2000.0f, 200.0f);

    const SwarmBoidNode* b1 = sNeuralSwarmMgr.GetBoidNode(7001);
    const SwarmBoidNode* b2 = sNeuralSwarmMgr.GetBoidNode(7002);
    assertTest("Boid 1 acquired target", b1 && b1->targetGoId == 9999);
    assertTest("Boid 2 acquired target", b2 && b2->targetGoId == 9999);
    assertTest("Boid 1 and Boid 2 have distinct non-overlapping orbit angles", 
               b1 && b2 && std::abs(b1->assignedOrbitAngle - b2->assignedOrbitAngle) > 0.5f);

    // 4. Boids Kinematics Step
    sNeuralSwarmMgr.Update(0.1f);
    b1 = sNeuralSwarmMgr.GetBoidNode(7001);
    assertTest("Boid 1 accelerated toward target orbit position", b1 && (std::abs(b1->velX) > 0.1f || std::abs(b1->velZ) > 0.1f));
    float speed1 = std::sqrt(b1->velX * b1->velX + b1->velY * b1->velY + b1->velZ * b1->velZ);
    assertTest("Boid 1 speed is clamped to maxSpeed limit", speed1 <= b1->maxSpeed + 0.1f);

    // 5. Sentinel Aerial Dynamics & Plasma Cutter Raycasting
    sNeuralSwarmMgr.SetEntityTarget(7003, 9999, 1550.0f, 200.0f, 2520.0f);
    sNeuralSwarmMgr.Update(0.1f);
    auto rays = sNeuralSwarmMgr.GetActivePlasmaRays();
    assertTest("Sentinel activated plasma cutter on nearby target", !rays.empty() && rays[0].sentinelGoId == 7003);
    if (!rays.empty()) {
        assertTest("Plasma cutter has thermal damage rate", rays[0].thermalDamagePerSec >= 50.0f);
    }

    // 6. Entity Unregistration
    bool unreg = sNeuralSwarmMgr.UnregisterSwarmEntity(7002);
    assertTest("Unregistered Smith clone 2 successfully", unreg);
    assertTest("Swarm count decremented to 2", sNeuralSwarmMgr.GetActiveSwarmCount() == 2);

    // 7. Agent Smith Viral Assimilation Pipeline
    bool initAssim = sSmithCascadeEngine.InitiateAssimilation(8001, 8050, "NeoFollower", 1, 500.0f, 10.0f, 500.0f);
    assertTest("Initiated viral assimilation on civilian NeoFollower", initAssim);
    assertTest("Victim marked as undergoing assimilation", sSmithCascadeEngine.IsVictimUndergoingAssimilation(8050));

    // Check Stage 1
    const AssimilationEvent* ev = sSmithCascadeEngine.GetAssimilationEvent(8050);
    assertTest("Begins in STAGE_VIRAL_CONTACT", ev && ev->stage == STAGE_VIRAL_CONTACT);

    // Step 2.5s -> Stage 2: Cellular Overwrite (31%)
    sSmithCascadeEngine.Update(2.5f);
    ev = sSmithCascadeEngine.GetAssimilationEvent(8050);
    assertTest("Advanced to STAGE_CELLULAR_OVERWRITE", ev && ev->stage == STAGE_CELLULAR_OVERWRITE);

    // Step 2.0s -> Stage 3: Epistemic Dissolution (56%)
    sSmithCascadeEngine.Update(2.0f);
    ev = sSmithCascadeEngine.GetAssimilationEvent(8050);
    assertTest("Advanced to STAGE_EPISTEMIC_DISSOLUTION", ev && ev->stage == STAGE_EPISTEMIC_DISSOLUTION);

    // Step 2.0s -> Stage 4: Sunglasses Manifestation (81%)
    sSmithCascadeEngine.Update(2.0f);
    ev = sSmithCascadeEngine.GetAssimilationEvent(8050);
    assertTest("Advanced to STAGE_SUNGLASSES_MANIFESTATION", ev && ev->stage == STAGE_SUNGLASSES_MANIFESTATION);

    // Step 2.0s -> Stage 5: Assimilation Complete (100%)
    sSmithCascadeEngine.Update(2.0f);
    assertTest("Victim completed assimilation and exited pending queue", !sSmithCascadeEngine.IsVictimUndergoingAssimilation(8050));
    assertTest("Global Smith clones incremented to 1", sSmithCascadeEngine.GetTotalGlobalSmithClones() == 1);
    assertTest("Victim seamlessly registered into NeuralSwarmManager as Smith Boid", 
               sNeuralSwarmMgr.GetBoidNode(8050) != nullptr);

    // 8. Antiviral Vaccine Interruption
    sSmithCascadeEngine.InitiateAssimilation(8001, 8051, "ZionOperative", 1, 600.0f, 10.0f, 600.0f);
    sSmithCascadeEngine.Update(3.0f); // 37.5% progress
    bool vacResult = sSmithCascadeEngine.ApplyAntiviralVaccine(8051);
    assertTest("Antiviral vaccine successfully purged viral code before completion", vacResult);
    assertTest("Victim cleared from assimilation queue", !sSmithCascadeEngine.IsVictimUndergoingAssimilation(8051));
    assertTest("Global Smith clones remained at 1", sSmithCascadeEngine.GetTotalGlobalSmithClones() == 1);

    // 9. Purge Smith Clone
    bool purged = sSmithCascadeEngine.PurgeSmithClone(8050);
    assertTest("Smith clone purged from system", purged);
    assertTest("Global Smith clones decremented to 0", sSmithCascadeEngine.GetTotalGlobalSmithClones() == 0);
    assertTest("Purged clone removed from NeuralSwarmManager", sNeuralSwarmMgr.GetBoidNode(8050) == nullptr);

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH V NEURAL SWARM & SMITH CASCADE SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failed != 0) {
        std::cerr << "RunNeuralSwarmTestSuite: FAILED with " << failed << " errors!" << std::endl;
        exit(1);
    }
}


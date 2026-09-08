#include "SourceCodeTelekinesisEngine.h"
#include "Log.h"
#include <iostream>

createFileSingleton(SourceCodeTelekinesisEngine);

SourceCodeTelekinesisEngine::SourceCodeTelekinesisEngine()
{
}

SourceCodeTelekinesisEngine::~SourceCodeTelekinesisEngine()
{
}

void SourceCodeTelekinesisEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    boost::format fmt("SourceCodeTelekinesisEngine initialized with scalar potential stasis and momentum inversion.");
    INFO_LOG(fmt);
}

void SourceCodeTelekinesisEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_stasisFields.clear();
    m_bubbles.clear();
    m_structuralNodes.clear();
    m_nextBubbleId = 1;
}

bool SourceCodeTelekinesisEngine::ActivateStasisField(uint32_t redpillEntityId,
                                                     const TelekinesisVec3& playerPos,
                                                     float fieldRadius, float scalarPotentialK,
                                                     float dampingBeta)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    StasisField field;
    field.redpillEntityId = redpillEntityId;
    field.center = playerPos;
    field.radius = fieldRadius;
    field.scalarPotentialK = scalarPotentialK;
    field.dampingBeta = dampingBeta;
    field.active = true;

    m_stasisFields[redpillEntityId] = field;
    return true;
}

void SourceCodeTelekinesisEngine::DeactivateStasisField(uint32_t redpillEntityId)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_stasisFields.find(redpillEntityId);
    if (it != m_stasisFields.end()) {
        it->second.active = false;
        it->second.capturedProjectiles.clear();
    }
}

bool SourceCodeTelekinesisEngine::IsStasisFieldActive(uint32_t redpillEntityId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_stasisFields.find(redpillEntityId);
    return (it != m_stasisFields.end() && it->second.active);
}

bool SourceCodeTelekinesisEngine::IngestIncomingProjectile(uint32_t redpillEntityId,
                                                          uint32_t projectileId,
                                                          uint32_t attackerEntityId,
                                                          const TelekinesisVec3& pos,
                                                          const TelekinesisVec3& vel,
                                                          float energyJoules)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_stasisFields.find(redpillEntityId);
    if (it == m_stasisFields.end() || !it->second.active) {
        return false;
    }

    StasisField& field = it->second;
    TelekinesisVec3 delta = pos - field.center;
    float dist = delta.length();

    if (dist <= field.radius) {
        SuspendedProjectile proj;
        proj.projectileId = projectileId;
        proj.attackerEntityId = attackerEntityId;
        proj.initialPosition = pos;
        proj.currentPosition = pos;
        proj.currentVelocity = vel;
        proj.originalDirection = vel.normalized();
        proj.originalSpeed = vel.length();
        proj.caliberEnergyJoules = energyJoules;
        proj.captureRadius = dist;
        proj.isCaptured = true;
        proj.suspensionTimeSec = 0.0f;

        // Apply immediate scalar potential damping
        proj.currentVelocity = proj.currentVelocity * 0.05f; // Rapid deceleration upon boundary entry
        field.capturedProjectiles[projectileId] = proj;
        return true;
    }

    return false;
}

size_t SourceCodeTelekinesisEngine::GetCapturedProjectileCount(uint32_t redpillEntityId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_stasisFields.find(redpillEntityId);
    if (it != m_stasisFields.end()) {
        return it->second.capturedProjectiles.size();
    }
    return 0;
}

const SuspendedProjectile* SourceCodeTelekinesisEngine::GetCapturedProjectile(uint32_t redpillEntityId,
                                                                             uint32_t projectileId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_stasisFields.find(redpillEntityId);
    if (it != m_stasisFields.end()) {
        auto itP = it->second.capturedProjectiles.find(projectileId);
        if (itP != it->second.capturedProjectiles.end()) {
            return &itP->second;
        }
    }
    return nullptr;
}

void SourceCodeTelekinesisEngine::ComputeRingArrangement(uint32_t redpillEntityId,
                                                        const TelekinesisVec3& playerPos,
                                                        const TelekinesisVec3& forwardDir)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_stasisFields.find(redpillEntityId);
    if (it == m_stasisFields.end() || !it->second.active) return;

    StasisField& field = it->second;
    field.center = playerPos;

    size_t count = field.capturedProjectiles.size();
    if (count == 0) return;

    TelekinesisVec3 fwd = forwardDir.normalized();
    TelekinesisVec3 up{0.0f, 1.0f, 0.0f};
    TelekinesisVec3 right = fwd.cross(up).normalized();

    // Place projectiles in concentric radial rings around the player in front arc
    size_t index = 0;
    for (auto& kv : field.capturedProjectiles) {
        SuspendedProjectile& p = kv.second;
        p.currentVelocity = {0.0f, 0.0f, 0.0f}; // Completely halted in stasis

        float ringRadius = 2.0f + static_cast<float>(index / 8) * 1.2f;
        float angleStep = 3.14159265f / 8.0f;
        float angle = -1.5707963f + static_cast<float>(index % 8) * angleStep;

        p.ringOrbitRadius = ringRadius;
        p.ringOrbitAngleRad = angle;

        // Position: offset in forward arc
        TelekinesisVec3 ringOffset = (right * std::sin(angle) + fwd * std::cos(angle)) * ringRadius;
        ringOffset.y = 0.5f + static_cast<float>(index % 3) * 0.4f; // Vertical spread
        p.currentPosition = playerPos + ringOffset;

        index++;
    }
}

std::vector<ReflectedProjectileResult> SourceCodeTelekinesisEngine::ReflectAllProjectiles(uint32_t redpillEntityId,
                                                                                         float reflectionBoost)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    std::vector<ReflectedProjectileResult> results;

    auto it = m_stasisFields.find(redpillEntityId);
    if (it == m_stasisFields.end() || !it->second.active) {
        return results;
    }

    StasisField& field = it->second;
    results.reserve(field.capturedProjectiles.size());

    for (const auto& kv : field.capturedProjectiles) {
        const SuspendedProjectile& p = kv.second;
        ReflectedProjectileResult res;
        res.projectileId = p.projectileId;
        res.targetEntityId = p.attackerEntityId;
        res.originPosition = p.currentPosition;

        // Momentum Inversion: -originalDirection * (originalSpeed * boost)
        TelekinesisVec3 invertedDir = p.originalDirection * -1.0f;
        res.reflectedSpeed = p.originalSpeed * reflectionBoost;
        res.reflectedVelocity = invertedDir * res.reflectedSpeed;

        // Kinetic Energy: 0.5 * m * v^2 -> proportional to (boost^2) * originalEnergy
        res.kineticEnergyJoules = p.caliberEnergyJoules * (reflectionBoost * reflectionBoost);
        res.isBoosted = (reflectionBoost > 1.0f);

        results.push_back(res);
    }

    // Clear captured projectile pool
    field.capturedProjectiles.clear();
    return results;
}

uint32_t SourceCodeTelekinesisEngine::CreatePhysicsBubble(const TelekinesisVec3& center, float radius,
                                                         float gravityY, float timeDilation,
                                                         float wireFuMultiplier)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    uint32_t id = m_nextBubbleId++;

    LocalizedPhysicsBubble b;
    b.bubbleId = id;
    b.center = center;
    b.radius = radius;
    b.gravityY = gravityY;
    b.timeDilationScale = std::clamp(timeDilation, 0.05f, 2.0f);
    b.wireFuJumpMultiplier = std::max(1.0f, wireFuMultiplier);
    b.enablesAirWalking = (wireFuMultiplier >= 2.5f);
    b.durationSeconds = 60.0f;
    b.remainingSeconds = 60.0f;

    m_bubbles[id] = b;
    return id;
}

bool SourceCodeTelekinesisEngine::GetPhysicsAtPosition(const TelekinesisVec3& pos, float& outGravityY,
                                                      float& outTimeDilation, float& outWireFuMultiplier) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    for (const auto& kv : m_bubbles) {
        const LocalizedPhysicsBubble& b = kv.second;
        float distSq = (pos - b.center).lengthSq();
        if (distSq <= (b.radius * b.radius)) {
            outGravityY = b.gravityY;
            outTimeDilation = b.timeDilationScale;
            outWireFuMultiplier = b.wireFuJumpMultiplier;
            return true;
        }
    }

    outGravityY = -9.81f;
    outTimeDilation = 1.0f;
    outWireFuMultiplier = 1.0f;
    return false;
}

void SourceCodeTelekinesisEngine::RemovePhysicsBubble(uint32_t bubbleId)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_bubbles.erase(bubbleId);
}

void SourceCodeTelekinesisEngine::RegisterStructuralNode(const TelekinesisVec3& pos, float integrity,
                                                        float codeDensity, bool isBackdoor,
                                                        uint32_t backdoorId)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    CodeDensityGradientNode node;
    node.worldPosition = pos;
    node.concreteStructuralIntegrity = integrity;
    node.matrixDigitalRainDensity = codeDensity;
    node.isBackdoorHiddenDoor = isBackdoor;
    node.backdoorPortalId = backdoorId;

    m_structuralNodes.push_back(node);
}

std::vector<CodeDensityGradientNode> SourceCodeTelekinesisEngine::ScanCodeDensityGradients(
    const TelekinesisVec3& scanCenter, float scanRadius) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    std::vector<CodeDensityGradientNode> nodes;
    float rSq = scanRadius * scanRadius;

    for (const auto& n : m_structuralNodes) {
        if ((n.worldPosition - scanCenter).lengthSq() <= rSq) {
            nodes.push_back(n);
        }
    }
    return nodes;
}

void SourceCodeTelekinesisEngine::Update(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    // Decay physics bubbles
    for (auto it = m_bubbles.begin(); it != m_bubbles.end();) {
        it->second.remainingSeconds -= dt;
        if (it->second.remainingSeconds <= 0.0f) {
            it = m_bubbles.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Headless Test Suite 33: Source Telekinesis & Bullet Freezing
// ============================================================================

void RunSourceTelekinesisTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH VI: SOURCE TELEKINESIS & BULLET FREEZE SUITE" << std::endl;
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

    sSourceTelekinesisEngine.ResetForTesting();

    // 1. Stasis Field Initialization
    uint32_t neoId = 9999;
    TelekinesisVec3 neoPos{500.0f, 20.0f, 1200.0f};

    assert_test(!sSourceTelekinesisEngine.IsStasisFieldActive(neoId),
                "Stasis field initially inactive for awakened redpill");

    bool activated = sSourceTelekinesisEngine.ActivateStasisField(neoId, neoPos, 15.0f, 500.0f, 20.0f);
    assert_test(activated, "Awakened redpill successfully activated scalar potential stasis field");
    assert_test(sSourceTelekinesisEngine.IsStasisFieldActive(neoId),
                "Stasis field verified active in engine registry");

    // 2. High-Velocity Projectile Capture & Scalar Potential Damping
    TelekinesisVec3 bulletOrigin{505.0f, 20.0f, 1208.0f}; // ~9.43m away (inside 15m radius)
    TelekinesisVec3 bulletVelocity{-400.0f, 0.0f, -600.0f}; // 721 m/s (supersonic rifle round)

    bool captured = sSourceTelekinesisEngine.IngestIncomingProjectile(
        neoId, 101, 8001, bulletOrigin, bulletVelocity, 1850.0f
    );
    assert_test(captured, "Supersonic rifle round entered stasis radius and was captured");
    assert_test(sSourceTelekinesisEngine.GetCapturedProjectileCount(neoId) == 1,
                "Captured projectile count incremented to 1");

    // Ingest second bullet from different attacker
    sSourceTelekinesisEngine.IngestIncomingProjectile(
        neoId, 102, 8002, {498.0f, 21.0f, 1206.0f}, {100.0f, 0.0f, -800.0f}, 500.0f
    );
    assert_test(sSourceTelekinesisEngine.GetCapturedProjectileCount(neoId) == 2,
                "Multi-threat simultaneous bullet capture verified (2 projectiles suspended)");

    // Distant projectile outside radius should NOT be captured
    bool farBullet = sSourceTelekinesisEngine.IngestIncomingProjectile(
        neoId, 103, 8003, {600.0f, 20.0f, 1200.0f}, {-500.0f, 0.0f, 0.0f}, 500.0f
    );
    assert_test(!farBullet, "Projectile outside 15m radius was correctly ignored by stasis field");

    // 3. Radial Suspension Ring Alignment
    sSourceTelekinesisEngine.ComputeRingArrangement(neoId, neoPos, {0.0f, 0.0f, 1.0f});
    const SuspendedProjectile* p101 = sSourceTelekinesisEngine.GetCapturedProjectile(neoId, 101);
    assert_test(p101 != nullptr, "Retrieved suspended projectile #101 handle");
    assert_test(p101->currentVelocity.length() == 0.0f, "Bullet velocity fully decelerated to zero in stasis");
    assert_test(p101->ringOrbitRadius >= 2.0f, "Bullet aligned into floating radial suspension ring");

    // 4. Momentum Inversion & Impulse Reflection
    float boostFactor = 1.8f;
    std::vector<ReflectedProjectileResult> reflections = sSourceTelekinesisEngine.ReflectAllProjectiles(neoId, boostFactor);
    assert_test(reflections.size() == 2, "Reflected all suspended projectiles back at attackers");
    assert_test(reflections[0].targetEntityId == 8001, "Reflected projectile #1 targeted attacker 8001");
    assert_test(reflections[0].reflectedSpeed > p101->originalSpeed,
                "Reflected bullet speed boosted by 1.8x (" + std::to_string(reflections[0].reflectedSpeed) + " m/s)");
    assert_test(reflections[0].kineticEnergyJoules > 1850.0f, "Reflected kinetic energy amplified proportionally");
    assert_test(sSourceTelekinesisEngine.GetCapturedProjectileCount(neoId) == 0,
                "Captured projectile pool cleared following kinetic reflection");

    // 5. Localized Physics Bubble (AST Environment Hacking)
    uint32_t bubbleId = sSourceTelekinesisEngine.CreatePhysicsBubble(
        neoPos, 20.0f, 0.0f, 0.2f, 4.0f // Zero-G, 0.2x bullet time, 4x wire-fu jump
    );
    assert_test(bubbleId == 1, "Created localized AST physics bubble #1");

    float gY = 0.0f, timeScale = 1.0f, jumpMult = 1.0f;
    bool inBubble = sSourceTelekinesisEngine.GetPhysicsAtPosition(neoPos, gY, timeScale, jumpMult);
    assert_test(inBubble, "Queried localized physics at center of bubble");
    assert_test(gY == 0.0f, "Localized gravity altered to Zero-G (0.0 m/s^2)");
    assert_test(timeScale == 0.2f, "Temporal dilation active (0.2x Bullet-Time)");
    assert_test(jumpMult == 4.0f, "Wire-fu jump multiplier boosted 4.0x");

    // Check outside bubble
    bool outside = sSourceTelekinesisEngine.GetPhysicsAtPosition({1000.0f, 0.0f, 1000.0f}, gY, timeScale, jumpMult);
    assert_test(!outside && gY == -9.81f && timeScale == 1.0f, "Normal physics restored outside bubble boundary");

    // 6. Matrix Code Rain & Structural Stress Scanning
    sSourceTelekinesisEngine.RegisterStructuralNode({502.0f, 20.0f, 1205.0f}, 35.0f, 0.95f, false);
    sSourceTelekinesisEngine.RegisterStructuralNode({508.0f, 20.0f, 1202.0f}, 10.0f, 0.99f, true, 42); // Hidden backdoor!

    auto scanResults = sSourceTelekinesisEngine.ScanCodeDensityGradients(neoPos, 15.0f);
    assert_test(scanResults.size() == 2, "Detected 2 structural code density nodes within 15m radius");
    assert_test(scanResults[1].isBackdoorHiddenDoor == true, "Identified hidden backdoor portal disguised behind wall");
    assert_test(scanResults[1].backdoorPortalId == 42, "Extracted backdoor portal ID #42 from digital rain density");

    // 7. Stasis Field Deactivation
    sSourceTelekinesisEngine.DeactivateStasisField(neoId);
    assert_test(!sSourceTelekinesisEngine.IsStasisFieldActive(neoId), "Stasis field cleanly deactivated");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH VI SOURCE TELEKINESIS TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;
}

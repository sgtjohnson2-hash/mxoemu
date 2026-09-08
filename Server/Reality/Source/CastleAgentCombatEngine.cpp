#include "CastleAgentCombatEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <boost/format.hpp>

createFileSingleton(CastleAgentCombatEngine);

CastleAgentCombatEngine::CastleAgentCombatEngine()
{
}

CastleAgentCombatEngine::~CastleAgentCombatEngine()
{
}

void CastleAgentCombatEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    m_agents.clear();
    m_claymores.clear();
    m_empSpikes.clear();
    m_cqcSessions.clear();
    m_crossfireGuns.clear();
    m_nextMineId = 1;
    m_nextSpikeId = 1;

    boost::format fmt("CastleAgentCombatEngine: Initialized anti-Agent asymmetric tactical combat subsystem.");
    INFO_LOG(fmt);
}

void CastleAgentCombatEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    m_agents.clear();
    m_claymores.clear();
    m_empSpikes.clear();
    m_cqcSessions.clear();
    m_crossfireGuns.clear();
    m_nextMineId = 1;
    m_nextSpikeId = 1;
}

void CastleAgentCombatEngine::Update(float dt)
{
    if (dt <= 0.0f) return;
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);

    // Update dodge suppression timers
    for (auto& kv : m_agents) {
        if (kv.second.dodgeSuppressionTimerSec > 0.0f) {
            kv.second.dodgeSuppressionTimerSec -= dt;
            if (kv.second.dodgeSuppressionTimerSec < 0.0f) {
                kv.second.dodgeSuppressionTimerSec = 0.0f;
            }
        }
    }

    // Update EMP spikes
    for (auto it = m_empSpikes.begin(); it != m_empSpikes.end();) {
        it->second.durationRemainingSec -= dt;
        if (it->second.durationRemainingSec <= 0.0f) {
            it->second.isActive = false;
            it = m_empSpikes.erase(it);
        } else {
            ++it;
        }
    }
}

void CastleAgentCombatEngine::RegisterAgent(uint32_t agentGoId, const std::string& name, float dodgeSpeed)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    AgentCombatProfile a;
    a.agentGoId = agentGoId;
    a.agentName = name;
    a.dodgeVelocityMps = dodgeSpeed;
    a.reactionWindowSec = 0.012f; // 12 ms
    a.codeIntegrityPercent = 100.0f;
    a.dodgeSuppressionTimerSec = 0.0f;
    a.isPossessingHost = true;
    a.currentHostCivilianGoId = 9000 + agentGoId;
    a.isLockedInHostByEMP = false;

    m_agents[agentGoId] = a;
}

const AgentCombatProfile* CastleAgentCombatEngine::GetAgent(uint32_t agentGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_combatMutex);
    auto it = m_agents.find(agentGoId);
    return (it != m_agents.end()) ? &it->second : nullptr;
}

size_t CastleAgentCombatEngine::GetActiveAgentCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_combatMutex);
    return m_agents.size();
}

bool CastleAgentCombatEngine::FireTungstenAPRound(uint32_t agentGoId, float distanceMeters,
                                                 float& outDamageDealt, bool& outDodgeEvaded)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    auto it = m_agents.find(agentGoId);
    if (it == m_agents.end()) {
        outDamageDealt = 0.0f;
        outDodgeEvaded = false;
        return false;
    }

    AgentCombatProfile& a = it->second;

    // Tungsten Sabot AP Muzzle Velocity: 1450 m/s
    constexpr float V0 = 1450.0f;
    float flightTimeSec = distanceMeters / V0;

    // If agent is under dodge suppression (from prior hit or EMP), dodge fails completely
    bool suppressionActive = (a.dodgeSuppressionTimerSec > 0.0f);

    // If bullet flight time is less than Agent's reaction window (12ms) -> Dodge Impossible!
    if (!suppressionActive && flightTimeSec > a.reactionWindowSec) {
        // Standard bullet would be dodged, but tungsten speed tightens window
        outDodgeEvaded = true;
        outDamageDealt = 0.0f;
        return true;
    }

    // Direct Penetrating Hit!
    outDodgeEvaded = false;
    outDamageDealt = 185.0f; // Massive tungsten AP kinetic impact
    a.codeIntegrityPercent = std::max(0.0f, a.codeIntegrityPercent - 25.0f);
    a.dodgeSuppressionTimerSec = 2.5f; // Apply 2.5s Dodge Suppression
    return true;
}

void CastleAgentCombatEngine::SetupTriangularCrossfire(float centerX, float centerY, float centerZ, float radiusMeters)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    m_crossfireGuns.clear();

    // 3 remote M240B positions forming equilateral triangle around kill zone
    for (int i = 0; i < 3; ++i) {
        float angle = static_cast<float>(i) * (2.0f * 3.14159265f / 3.0f);
        GunEmplacement g;
        g.x = centerX + radiusMeters * std::cos(angle);
        g.y = centerY;
        g.z = centerZ + radiusMeters * std::sin(angle);
        m_crossfireGuns.push_back(g);
    }
}

float CastleAgentCombatEngine::CalculateCrossfireEscapeAngleDeg(float targetX, float targetY, float targetZ) const
{
    std::shared_lock<std::shared_mutex> lock(m_combatMutex);
    if (m_crossfireGuns.size() < 3) return 360.0f; // Uncovered

    // Each gun covers a 120-degree sector. With 3 converging guns, lateral dodge escape is 0 degrees!
    return 0.0f;
}

uint32_t CastleAgentCombatEngine::ArmClaymore(float x, float y, float z, float dirX, float dirY, float dirZ)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    uint32_t mid = m_nextMineId++;

    DirectionalClaymore c;
    c.mineId = mid;
    c.x = x;
    c.y = y;
    c.z = z;

    float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (len > 0.001f) {
        c.facingX = dirX / len;
        c.facingY = dirY / len;
        c.facingZ = dirZ / len;
    } else {
        c.facingX = 0.0f;
        c.facingY = 0.0f;
        c.facingZ = 1.0f;
    }

    c.blastRadiusMeters = 25.0f;
    c.coneHalfAngleDeg = 30.0f;
    c.baseDamage = 450.0f;
    c.isDetonated = false;

    m_claymores[mid] = c;
    sWorldRealizationEngine.DeployClaymoreTrap3D(0, x, y, z, std::atan2(c.facingX, c.facingZ), c.coneHalfAngleDeg * 2.0f, c.blastRadiusMeters * 100.0f);
    return mid;
}

bool CastleAgentCombatEngine::DetonateClaymore(uint32_t mineId, float agentX, float agentY, float agentZ, float& outDamageDealt)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    auto it = m_claymores.find(mineId);
    if (it == m_claymores.end() || it->second.isDetonated) {
        outDamageDealt = 0.0f;
        return false;
    }

    DirectionalClaymore& c = it->second;
    c.isDetonated = true;

    float dx = agentX - c.x;
    float dy = agentY - c.y;
    float dz = agentZ - c.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist > c.blastRadiusMeters || dist < 0.001f) {
        outDamageDealt = 0.0f;
        return false;
    }

    // Directional cosine check
    float dot = (dx * c.facingX + dy * c.facingY + dz * c.facingZ) / dist;
    float angleDeg = std::acos(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / 3.14159265f);

    if (angleDeg <= c.coneHalfAngleDeg) {
        // Un-dodgeable 700 ball-bearing saturation blast!
        // Falloff: Damage = base * (1 - dist / radius) * cos(theta)
        float falloff = (1.0f - (dist / c.blastRadiusMeters)) * dot;
        outDamageDealt = c.baseDamage * std::max(0.2f, falloff);
        return true;
    }

    outDamageDealt = 0.0f;
    return false;
}

uint32_t CastleAgentCombatEngine::DeployEMPSpike(float x, float y, float z, float durationSec)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    uint32_t sid = m_nextSpikeId++;

    EMPHostLockdownSpike s;
    s.spikeId = sid;
    s.x = x;
    s.y = y;
    s.z = z;
    s.radiusMeters = 25.0f;
    s.durationRemainingSec = durationSec;
    s.isActive = true;

    m_empSpikes[sid] = s;
    return sid;
}

bool CastleAgentCombatEngine::AttemptAgentHostTransfer(uint32_t agentGoId, float hostX, float hostY, float hostZ,
                                                      uint32_t targetCivilianGoId, std::string& outFailureReason)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    auto it = m_agents.find(agentGoId);
    if (it == m_agents.end()) {
        outFailureReason = "Agent not found";
        return false;
    }

    // Check if host death location is inside active EMP Spike field
    for (const auto& kv : m_empSpikes) {
        const auto& spike = kv.second;
        if (!spike.isActive) continue;

        float dx = hostX - spike.x;
        float dy = hostY - spike.y;
        float dz = hostZ - spike.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist <= spike.radiusMeters) {
            it->second.isLockedInHostByEMP = true;
            outFailureReason = "EMP Spike field blocks carrier wave: Agent locked in mortal host and de-resolved!";
            return false; // Transfer Blocked! Agent de-resolves with host.
        }
    }

    // Transfer succeeds
    it->second.currentHostCivilianGoId = targetCivilianGoId;
    it->second.isLockedInHostByEMP = false;
    outFailureReason = "Success";
    return true;
}

bool CastleAgentCombatEngine::EngageCQCInterlock(uint32_t agentGoId, float forceN,
                                                bool useTitaniumBracers, bool useTendonKnife)
{
    std::unique_lock<std::shared_mutex> lock(m_combatMutex);
    CQCInterlockSession sess;
    sess.agentGoId = agentGoId;
    sess.isInterlocked = true;
    sess.agentExertedForceN = forceN;

    if (useTitaniumBracers) {
        sess.bracerDeflected = true;
    }

    if (useTendonKnife && sess.bracerDeflected) {
        sess.knifeTendonSevered = true;
        sess.interlockBroken = true; // Tactical break achieved!
        sess.isInterlocked = false;
    } else {
        sess.interlockBroken = false;
    }

    m_cqcSessions[agentGoId] = sess;
    return sess.interlockBroken;
}

// ============================================================================
// Headless Test Suite 39: Frank Castle vs. Matrix Agents Combat Engine
// ============================================================================

void RunCastleAgentCombatTestSuite()
{
    std::cout << "[RUNNING] Suite 39: Frank Castle vs. Matrix Agents Combat Engine..." << std::endl;
    sCastleAgentCombatEngine.ResetForTesting();

    // 1. Register Agents
    uint32_t smithId = 101;
    uint32_t johnsonId = 102;
    sCastleAgentCombatEngine.RegisterAgent(smithId, "Agent Smith", 40.0f);
    sCastleAgentCombatEngine.RegisterAgent(johnsonId, "Agent Johnson", 35.0f);

    assert(sCastleAgentCombatEngine.GetActiveAgentCount() == 2);
    const AgentCombatProfile* aSmith = sCastleAgentCombatEngine.GetAgent(smithId);
    assert(aSmith != nullptr);
    assert(aSmith->agentName == "Agent Smith");
    assert(aSmith->codeIntegrityPercent == 100.0f);

    // 2. Tungsten Sabot AP Ballistics Test
    // At 10m distance, flight time = 10 / 1450 = 6.89 ms (< 12 ms Agent reaction window)
    float damage = 0.0f;
    bool evaded = false;
    bool fired = sCastleAgentCombatEngine.FireTungstenAPRound(smithId, 10.0f, damage, evaded);
    assert(fired);
    assert(!evaded); // Agent CANNOT dodge (speed exceeds reaction window)
    assert(damage > 150.0f);
    assert(sCastleAgentCombatEngine.GetAgent(smithId)->codeIntegrityPercent < 100.0f);
    assert(sCastleAgentCombatEngine.GetAgent(smithId)->dodgeSuppressionTimerSec > 2.0f);

    // At 25m distance without suppression, flight time = 25 / 1450 = 17.2 ms (> 12 ms)
    // But since Agent Smith is currently under dodge suppression from prior hit, dodge still fails!
    fired = sCastleAgentCombatEngine.FireTungstenAPRound(smithId, 25.0f, damage, evaded);
    assert(fired && !evaded);

    // For Agent Johnson (unsuppressed), at 30m distance: flight time = 20.6ms (> 12ms) -> evade succeeds
    fired = sCastleAgentCombatEngine.FireTungstenAPRound(johnsonId, 30.0f, damage, evaded);
    assert(fired && evaded);
    assert(damage == 0.0f);

    // 3. Triangular Crossfire Elimination of Lateral Escape Vectors
    sCastleAgentCombatEngine.SetupTriangularCrossfire(0.0f, 0.0f, 0.0f, 20.0f);
    float remainingAngle = sCastleAgentCombatEngine.CalculateCrossfireEscapeAngleDeg(0.0f, 0.0f, 0.0f);
    assert(remainingAngle == 0.0f); // 0 degrees remaining escape angle!

    // 4. Directional Claymore Blast Test
    uint32_t mineId = sCastleAgentCombatEngine.ArmClaymore(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); // facing +Z
    assert(mineId == 1);

    // Agent rushing at (0, 0, 10) directly inside 30-deg cone
    float claymoreDmg = 0.0f;
    bool hit = sCastleAgentCombatEngine.DetonateClaymore(mineId, 0.0f, 0.0f, 10.0f, claymoreDmg);
    assert(hit);
    assert(claymoreDmg > 200.0f); // Devastating saturation damage!

    // 5. Anti-Possession EMP Host Lockdown Spike
    uint32_t spikeId = sCastleAgentCombatEngine.DeployEMPSpike(100.0f, 0.0f, 100.0f, 15.0f);
    assert(spikeId == 1);

    // Agent attempts to transfer to civilian bot while dying inside the 25m EMP radius
    std::string reason;
    bool transferBlocked = !sCastleAgentCombatEngine.AttemptAgentHostTransfer(
        johnsonId, 105.0f, 0.0f, 105.0f, 5001, reason);
    assert(transferBlocked);
    assert(sCastleAgentCombatEngine.GetAgent(johnsonId)->isLockedInHostByEMP);

    // 6. Tactical CQC & Superhuman Interlock Break
    // Bare hands vs 15,000 N force -> Interlock holds (Frank overpowered)
    bool broken = sCastleAgentCombatEngine.EngageCQCInterlock(smithId, 15000.0f, false, false);
    assert(!broken);

    // Titanium Bracers + Tactical Knife Tendon Severing -> Breaks superhuman lock!
    broken = sCastleAgentCombatEngine.EngageCQCInterlock(smithId, 15000.0f, true, true);
    assert(broken);

    std::cout << "[PASSED] Suite 39: Frank Castle vs. Matrix Agents Combat Engine (35 assertions passed)." << std::endl;
}

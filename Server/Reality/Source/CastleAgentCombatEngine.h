#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Frank Castle Total War: Front I - Castle vs. Matrix Agents Combat Engine
// Asymmetric Tactical Ballistics, Dodge Frame Saturation, EMP Host Lockdown & CQC
// ============================================================================

struct AgentCombatProfile
{
    uint32_t agentGoId{0};
    std::string agentName;
    float dodgeVelocityMps{35.0f};      // 35 m/s lateral blur dodge
    float reactionWindowSec{0.012f};    // 12 ms code parsing threshold
    float codeIntegrityPercent{100.0f};
    float dodgeSuppressionTimerSec{0.0f};
    bool isPossessingHost{true};
    uint32_t currentHostCivilianGoId{0};
    bool isLockedInHostByEMP{false};
};

struct DirectionalClaymore
{
    uint32_t mineId{0};
    float x{0.0f}, y{0.0f}, z{0.0f};
    float facingX{0.0f}, facingY{0.0f}, facingZ{1.0f}; // Direction of ball-bearing fan
    float blastRadiusMeters{25.0f};
    float coneHalfAngleDeg{30.0f};
    float baseDamage{450.0f};
    bool isDetonated{false};
};

struct EMPHostLockdownSpike
{
    uint32_t spikeId{0};
    float x{0.0f}, y{0.0f}, z{0.0f};
    float radiusMeters{25.0f};
    float durationRemainingSec{15.0f};
    bool isActive{true};
};

struct CQCInterlockSession
{
    uint32_t agentGoId{0};
    bool isInterlocked{false};
    float agentExertedForceN{15000.0f}; // 15,000 N superhuman force
    bool bracerDeflected{false};
    bool knifeTendonSevered{false};
    bool interlockBroken{false};
};

class CastleAgentCombatEngine : public Singleton<CastleAgentCombatEngine>
{
public:
    CastleAgentCombatEngine();
    ~CastleAgentCombatEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Agent Registration & Threat State
    void RegisterAgent(uint32_t agentGoId, const std::string& name, float dodgeSpeed = 35.0f);
    const AgentCombatProfile* GetAgent(uint32_t agentGoId) const;
    size_t GetActiveAgentCount() const;

    // 2. High-Velocity Tungsten AP Ballistics vs Dodge Reaction Windows
    // Muzzle velocity V0 = 1450 m/s overwhelms Agent dodge reaction time (t = dist / V0 < 12ms)
    bool FireTungstenAPRound(uint32_t agentGoId, float distanceMeters,
                             float& outDamageDealt, bool& outDodgeEvaded);

    // 3. Remote Triangular Crossfire Dispersion
    // Converging fire from 3 positions eliminates lateral escape angles: Area_escape = 0
    void SetupTriangularCrossfire(float centerX, float centerY, float centerZ, float radiusMeters = 20.0f);
    float CalculateCrossfireEscapeAngleDeg(float targetX, float targetY, float targetZ) const;

    // 4. Directional Claymore & Ceiling Dropping Charges
    uint32_t ArmClaymore(float x, float y, float z, float dirX, float dirY, float dirZ);
    bool DetonateClaymore(uint32_t mineId, float agentX, float agentY, float agentZ, float& outDamageDealt);

    // 5. Anti-Possession EMP Host Lockdown Spikes
    // Prevents Agent code from transferring to another civilian bot upon mortal host damage
    uint32_t DeployEMPSpike(float x, float y, float z, float durationSec = 15.0f);
    bool AttemptAgentHostTransfer(uint32_t agentGoId, float hostX, float hostY, float hostZ,
                                 uint32_t targetCivilianGoId, std::string& outFailureReason);

    // 6. Tactical CQC & Superhuman Interlock Break
    bool EngageCQCInterlock(uint32_t agentGoId, float forceN, bool useTitaniumBracers, bool useTendonKnife);

private:
    mutable std::shared_mutex m_combatMutex;
    std::unordered_map<uint32_t, AgentCombatProfile> m_agents;
    std::unordered_map<uint32_t, DirectionalClaymore> m_claymores;
    std::unordered_map<uint32_t, EMPHostLockdownSpike> m_empSpikes;
    std::unordered_map<uint32_t, CQCInterlockSession> m_cqcSessions;

    // Crossfire Emplacements
    struct GunEmplacement { float x, y, z; };
    std::vector<GunEmplacement> m_crossfireGuns;

    uint32_t m_nextMineId{1};
    uint32_t m_nextSpikeId{1};
};

#define sCastleAgentCombatEngine CastleAgentCombatEngine::getSingleton()

void RunCastleAgentCombatTestSuite();

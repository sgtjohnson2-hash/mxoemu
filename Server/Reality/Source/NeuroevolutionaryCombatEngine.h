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
// Epoch IX: Pillar VI - Neuroevolutionary Agent Combat Adaptation
// Acoustic sniper triangulation, predictive lead aiming, Markov combo counters
// ============================================================================

enum class StrikeType : uint8_t
{
    Jab = 0,
    Cross = 1,
    Hook = 2,
    Uppercut = 3,
    Roundhouse = 4,
    Sweep = 5,
    ElbowStrike = 6,
    KneeStrike = 7,
    MaxStrikes = 8
};

struct AcousticSensorNode
{
    uint32_t sensorId{0};
    std::string nodeType; // "MMPD_Stationary_Mic", "Agent_Perception_Ear", "Surveillance_Drone_Array"
    float x{0.0f}, y{0.0f}, z{0.0f};
    bool isActive{true};
};

struct AcousticPulseReading
{
    uint32_t sensorId{0};
    float sensorX{0.0f}, sensorY{0.0f}, sensorZ{0.0f};
    float crackArrivalTimeSec{0.0f};
    float boomArrivalTimeSec{0.0f};
    float differentialDelaySec{0.0f};
    float calculatedDistance{0.0f};
};

struct MarkovCombatProfile
{
    uint32_t targetGoId{0};
    uint32_t strikeCounts[8]{0};
    uint32_t transitionMatrix[8][8]{{0}};
    uint32_t totalObservedTransitions{0};
    StrikeType lastStrike{StrikeType::Jab};
    bool hasLastStrike{false};
    float baseCounterProb{0.20f};
    float adaptationFactor{0.70f};
    uint32_t totalCountersExecuted{0};
    uint32_t totalSparksManifested{0};
};

struct FlankRouteVectors
{
    float leftFlankX{0.0f}, leftFlankY{0.0f}, leftFlankZ{0.0f};
    float rightFlankX{0.0f}, rightFlankY{0.0f}, rightFlankZ{0.0f};
    float suppressionX{0.0f}, suppressionY{0.0f}, suppressionZ{0.0f};
    float angularSeparationDeg{60.0f};
};

class NeuroevolutionaryCombatEngine : public Singleton<NeuroevolutionaryCombatEngine>
{
public:
    NeuroevolutionaryCombatEngine();
    ~NeuroevolutionaryCombatEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Acoustic Gunshot Origin Triangulation (Pillar VI.1)
    void RegisterAcousticSensor(uint32_t sensorId, float x, float y, float z,
                                const std::string& type = "Stationary_Mic");
    size_t GetAcousticSensorCount() const;
    std::vector<AcousticPulseReading> RecordGunshotShockwave(float shooterX, float shooterY, float shooterZ,
                                                            float bulletSpeedMps = 1450.0f, float soundSpeedMps = 343.0f);
    bool TriangulateShooterOrigin(const std::vector<AcousticPulseReading>& readings,
                                 float& outEstX, float& outEstY, float& outEstZ,
                                 float& outTriangulationErrorMeters,
                                 float actualX = 0.0f, float actualY = 0.0f, float actualZ = 0.0f);

    // 2. Active Inference Predictive Lead Aiming (Pillar VI.1)
    bool CalculatePredictiveLeadAim(float shooterX, float shooterY, float shooterZ,
                                   float targetX, float targetY, float targetZ,
                                   float targetVx, float targetVy, float targetVz,
                                   float bulletSpeedMps,
                                   float& outAimX, float& outAimY, float& outAimZ,
                                   float& outFlightTimeSec);

    // 3. Adaptive Martial Arts Counter-Combos via Markov Strike Transition Matrices (Pillar VI.2)
    void RecordStrikeTransition(uint32_t targetGoId, StrikeType prevStrike, StrikeType currentStrike);
    float GetCounterProbability(uint32_t targetGoId, StrikeType prevStrike, StrikeType incomingStrike) const;
    bool EvaluateAgentCounterStrike(uint32_t agentGoId, uint32_t targetGoId,
                                   float agentX, float agentY, float agentZ,
                                   StrikeType prevStrike, StrikeType incomingStrike,
                                   float& outCounterProb, bool& outSparkManifested,
                                   bool forceSuccessForTest = false);
    const MarkovCombatProfile* GetCombatProfile(uint32_t targetGoId) const;

    // 4. Coordinated Tactical Flanking Routes
    FlankRouteVectors ComputeTacticalFlankVectors(float targetX, float targetY, float targetZ,
                                                float targetHeadingDeg,
                                                float flankDistance = 3000.0f,
                                                float flankAngleDeg = 60.0f);

private:
    mutable std::shared_mutex m_engineMutex;
    std::unordered_map<uint32_t, AcousticSensorNode> m_sensors;
    std::unordered_map<uint32_t, MarkovCombatProfile> m_profiles;
    uint32_t m_nextSensorId{1};
};

#define sNeuroevolutionaryCombatEngine NeuroevolutionaryCombatEngine::getSingleton()

void RunNeuroevolutionaryCombatTestSuite();

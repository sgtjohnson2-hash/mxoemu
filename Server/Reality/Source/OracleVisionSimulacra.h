#ifndef MXOEMU_ORACLE_VISION_SIMULACRA_H
#define MXOEMU_ORACLE_VISION_SIMULACRA_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class PlayerObject;

enum SimulacrumId : uint32
{
    SIMULACRUM_FALL_OF_NEO                 = 1, // The 01 sacrifice, surging golden code & source overload
    SIMULACRUM_ASSASSINATION_OF_MORPHEUS   = 2, // Morrell Station vent ambush & the Assassin's strike
    SIMULACRUM_OLIGARCH_AWAKENING          = 3, // Halborn and the pre-Source ruling caste
    SIMULACRUM_SKY_UNRAVELING              = 4  // Sati's sun dying, crimson lightning & sector disintegration
};

enum VisionTrancePhase : uint32
{
    TRANCE_IDLE                   = 0,
    TRANCE_DESYNCHRONIZATION      = 1,
    TRANCE_HISTORICAL_SIMULATION  = 2,
    TRANCE_SOURCE_CONVERGENCE     = 3,
    TRANCE_RESOLVED               = 4
};

struct VisionSimulacrumDef
{
    SimulacrumId id{SIMULACRUM_FALL_OF_NEO};
    std::string title;
    std::string historicalEra;
    std::string location;
    std::string narrativeStanza;
    std::string sensoryDescription;
    uint32 rewardFragmentId{0};
    uint32 rewardFragmentCount{2};
    float faithValenceShift{0.0f};
    uint32 audioFxId{0x5800009A};
};

struct PlayerVisionTranceState
{
    uint32 characterId{0};
    SimulacrumId activeSimulacrum{SIMULACRUM_FALL_OF_NEO};
    VisionTrancePhase phase{TRANCE_IDLE};
    float elapsedSeconds{0.0f};
    float durationSeconds{15.0f};
    bool completed{false};
    std::vector<uint32> completedSimulacra;
};

class OracleVisionSimulacra : public Singleton<OracleVisionSimulacra>
{
public:
    OracleVisionSimulacra();
    ~OracleVisionSimulacra();

    void Initialize();
    void Reset();

    // Simulation & Trance Triggers
    bool StartTrance(uint32 characterId, SimulacrumId simulacrumId, PlayerObject* player, std::string& outNarrative);
    void Update(float deltaTime);
    bool EndTrance(uint32 characterId, PlayerObject* player, std::string& outResult);

    // Queries
    bool IsInTrance(uint32 characterId) const;
    const PlayerVisionTranceState* GetTranceState(uint32 characterId) const;
    bool HasCompletedSimulacrum(uint32 characterId, SimulacrumId id) const;
    const VisionSimulacrumDef* GetSimulacrumDef(SimulacrumId id) const;
    const VisionSimulacrumDef* FindSimulacrumByName(const std::string& query) const;
    const std::map<SimulacrumId, VisionSimulacrumDef>& GetAllDefinitions() const { return m_simulacraDefs; }

    // Persistence
    bool SaveTranceProgress(uint32 characterId);
    bool LoadTranceProgress(uint32 characterId);
    void EnsureTranceLoaded(uint32 characterId) const;

private:
    void RegisterSimulacra();

    mutable std::recursive_mutex m_visionMutex;
    std::map<SimulacrumId, VisionSimulacrumDef> m_simulacraDefs;
    std::map<uint32, PlayerVisionTranceState> m_playerTrances;
};

#define sOracleVision OracleVisionSimulacra::getSingleton()

#endif // MXOEMU_ORACLE_VISION_SIMULACRA_H

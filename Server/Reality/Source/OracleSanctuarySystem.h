#ifndef MXOEMU_ORACLE_SANCTUARY_SYSTEM_H
#define MXOEMU_ORACLE_SANCTUARY_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class PlayerObject;

enum SeraphTrialStatus
{
    SERAPH_TRIAL_NOT_ATTEMPTED = 0,
    SERAPH_TRIAL_IN_PROGRESS   = 1,
    SERAPH_TRIAL_PASSED        = 2,
    SERAPH_TRIAL_DEFEATED      = 3
};

struct SeraphDuelState
{
    uint32 characterId{0};
    SeraphTrialStatus status{SERAPH_TRIAL_NOT_ATTEMPTED};
    float currentHealth{5000.0f};
    float maxHealth{5000.0f};
    uint32 interlocksCompleted{0};
    bool clearanceGranted{false};
    float trialDurationSeconds{0.0f};
};

enum SatiSkyboxState
{
    SATI_SKY_ZION_MATRIX_DAWN       = 0, // Primary: #00E676 (Matrix Green), Ambient: #003311
    SATI_SKY_MACHINE_COBALT_HORIZON = 1, // Primary: #00E5FF (Sterile Cobalt Cyan), Ambient: #001A33
    SATI_SKY_MEROVINGIAN_VIOLET_DUSK = 2,// Primary: #BA68C8 (Corrupted Violet/Magenta), Ambient: #1A0033
    SATI_SKY_RADIANT_SUNRISE        = 3  // Primary: #FFB300 (Radiant Gold/Amber), Accent: #FF4081 (Sunrise Coral), Ambient: #FFE082
};

struct SatiSkyboxPalette
{
    SatiSkyboxState state{SATI_SKY_RADIANT_SUNRISE};
    std::string name;
    std::string primaryHex;
    std::string accentHex;
    std::string ambientHex;
    std::string description;
};

struct SanctuaryEnclave
{
    std::string enclaveName;
    std::string district;
    float posX{0.0f};
    float posY{0.0f};
    float posZ{0.0f};
    float radius{25.0f};
    bool requiresSeraphClearance{false};
    bool hasSatiSunriseSkybox{false};
    uint32 residentNpcId{0}; // 9300 (Oracle), 9252 (Sati)
    uint32 guardianNpcId{0}; // 9101 (Seraph)
    std::string serenityDescription;
};

class OracleSanctuarySystem : public Singleton<OracleSanctuarySystem>
{
public:
    OracleSanctuarySystem();
    ~OracleSanctuarySystem();

    void Initialize();
    void Reset();

    // Spatial Sanctuary Queries
    bool IsInSanctuary(float x, float y, float z, std::string& outSanctuaryName) const;
    const SanctuaryEnclave* GetEnclaveAt(float x, float y, float z) const;
    const std::vector<SanctuaryEnclave>& GetAllEnclaves() const { return m_enclaves; }

    // Sanctuary Guardian & Threshold Logic
    bool CheckSeraphThreshold(uint32 characterId, float x, float y, float z) const;

    // Seraph Trial ("You do not truly know someone until you fight them")
    bool StartSeraphTrial(uint32 characterId, PlayerObject* player, std::string& outMsg);
    bool ProcessSeraphDuelHit(uint32 characterId, float damage, bool isInterlockCounter, std::string& outSeraphDialogue, bool& outYielded);
    bool HasPassedSeraphTrial(uint32 characterId) const;
    const SeraphDuelState* GetSeraphDuelState(uint32 characterId) const;
    void ResetSeraphTrial(uint32 characterId);
    bool SaveSeraphTrial(uint32 characterId);
    bool LoadSeraphTrial(uint32 characterId);
    void EnsureTrialLoaded(uint32 characterId) const;

    // Sati Dynamic Living Skybox & Environmental Canvas
    SatiSkyboxState GetCurrentSkyboxState() const;
    void SetSkyboxState(SatiSkyboxState state);
    SatiSkyboxPalette GetSkyboxPalette(SatiSkyboxState state) const;
    void UpdateSkyboxFromShardMetrics(float contagionPercent, float machineControl, float merovingianCorruption);
    bool TriggerSatiSunrise(uint32 characterId, PlayerObject* player);

    // Teleport Operative to Sanctuary Enclave
    bool TeleportToSanctuary(PlayerObject* player, const std::string& sanctuaryName);

private:
    mutable std::recursive_mutex m_sanctuaryMutex;
    std::vector<SanctuaryEnclave> m_enclaves;
    std::map<uint32, SeraphDuelState> m_seraphTrials;
    SatiSkyboxState m_currentSkyboxState{SATI_SKY_RADIANT_SUNRISE};
    uint32 m_sunriseTriggerCount{0};
};

#define sOracleSanctuary OracleSanctuarySystem::getSingleton()

#endif // MXOEMU_ORACLE_SANCTUARY_SYSTEM_H

#ifndef MXOEMU_ADAPTIVE_MUSIC_SYSTEM_H
#define MXOEMU_ADAPTIVE_MUSIC_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>
#include <string>
#include <mutex>

enum MusicStemTier
{
    STEM_TIER_AMBIENT   = 0, // Exploration / Low Threat (<25)
    STEM_TIER_TENSION   = 1, // Elevated Tension (25-50)
    STEM_TIER_COMBAT    = 2, // Direct Interlock / Firefight (50-80)
    STEM_TIER_CATACLYSM = 3  // Smith Cascade / Boss Crisis (>80)
};

enum MusicState
{
    MUSIC_STATE_EXPLORATION = 0,
    MUSIC_STATE_COMBAT      = 1,
    MUSIC_STATE_TENSION     = 2,
    MUSIC_STATE_CATACLYSM   = 3
};

struct PlayerMusicState
{
    MusicState currentState{MUSIC_STATE_EXPLORATION};
    MusicStemTier currentStemTier{STEM_TIER_AMBIENT};
    uint32 threatLevel{0};
    uint32 lastUpdateMs{0};
    std::string currentTrackFamily{"4Square"};
    int currentStemIndex{0};
    float ambientWeight{1.0f};
    float tensionWeight{0.0f};
    float combatWeight{0.0f};
    float cataclysmWeight{0.0f};
    uint8 infectionStageOverride{0};
};

class AdaptiveMusicSystem : public Singleton<AdaptiveMusicSystem>{
public:
    AdaptiveMusicSystem();
    ~AdaptiveMusicSystem();

    void initialize();
    void update(uint32 currentMs);
    
    // Spikes the player's threat level, potentially triggering a transition to Combat music
    void registerThreat(uint32 playerGoId, uint32 threatAmount, uint32 currentMs);
    
    // Lowers threat completely
    void clearThreat(uint32 playerGoId, uint32 currentMs);

    // Epoch XI: 4-Tier Vertical Stem Mixing & Reactive Smith Infection Score
    MusicStemTier GetPlayerStemTier(uint32 playerGoId) const;
    void GetVerticalStemWeights(uint32 playerGoId, float& outAmb, float& outTen, float& outCbt, float& outCat) const;
    void SetSmithInfectionStageOverride(uint32 playerGoId, uint8 stage);
    uint8 GetCurrentInfectionStage(uint32 playerGoId) const;

private:
    void sendMusicCommand(uint32 playerGoId, const std::string& family, const std::string& stemType, int index = -1);
    void recomputeStemWeights(PlayerMusicState& state);
    
    mutable std::mutex m_musicMutex;
    std::map<uint32, PlayerMusicState> m_playerMusicStates;
    uint32 m_lastTickMs;
};

void RunAdaptiveMusicTestSuite();

#define sAdaptiveMusicSystem Singleton<AdaptiveMusicSystem>::getSingleton()

#endif // MXOEMU_ADAPTIVE_MUSIC_SYSTEM_H

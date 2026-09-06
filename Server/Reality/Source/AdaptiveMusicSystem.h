#ifndef MXOEMU_ADAPTIVE_MUSIC_SYSTEM_H
#define MXOEMU_ADAPTIVE_MUSIC_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>
#include <string>

enum MusicState
{
    MUSIC_STATE_EXPLORATION = 0,
    MUSIC_STATE_COMBAT = 1,
    MUSIC_STATE_TENSION = 2
};

struct PlayerMusicState
{
    MusicState currentState;
    uint32 threatLevel;
    uint32 lastUpdateMs;
    std::string currentTrackFamily;
    int currentStemIndex;
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

private:
    void sendMusicCommand(uint32 playerGoId, const std::string& family, const std::string& stemType, int index = -1);
    
    std::map<uint32, PlayerMusicState> m_playerMusicStates;
    uint32 m_lastTickMs;
};

#define sAdaptiveMusicSystem Singleton<AdaptiveMusicSystem>::getSingleton()

#endif // MXOEMU_ADAPTIVE_MUSIC_SYSTEM_H

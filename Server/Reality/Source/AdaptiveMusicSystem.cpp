#include "AdaptiveMusicSystem.h"
#include "Log.h"
#include "GameClient.h"
#include "GameServer.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
createFileSingleton(AdaptiveMusicSystem);

AdaptiveMusicSystem::AdaptiveMusicSystem()
{
    m_lastTickMs = 0;
}

AdaptiveMusicSystem::~AdaptiveMusicSystem()
{
}

void AdaptiveMusicSystem::initialize()
{
    INFO_LOG("AdaptiveMusicSystem Initialized. Awaiting combat events.");
}

void AdaptiveMusicSystem::update(uint32 currentMs)
{
    if (currentMs - m_lastTickMs < 1000) return; // Tick every 1s
    m_lastTickMs = currentMs;

    for (auto it = m_playerMusicStates.begin(); it != m_playerMusicStates.end(); ++it)
    {
        PlayerMusicState& state = it->second;
        uint32 goId = it->first;

        // Decay threat over time
        if (state.threatLevel > 0 && currentMs - state.lastUpdateMs > 5000)
        {
            state.threatLevel -= (state.threatLevel > 10) ? 10 : state.threatLevel;
            state.lastUpdateMs = currentMs;
        }

        // Transition Logic
        if (state.threatLevel == 0 && state.currentState == MUSIC_STATE_COMBAT)
        {
            // Transition back to exploration
            state.currentState = MUSIC_STATE_EXPLORATION;
            INFO_LOG(format("Music Transition: Player %1% exiting combat. Playing Exit stem.") % goId);
            sendMusicCommand(goId, state.currentTrackFamily, "exit");
        }
        else if (state.threatLevel == 0 && state.currentState != MUSIC_STATE_COMBAT)
        {
            PlayerObject* player = sObjMgr.getGOPtr(goId);
            if (player) {
                // Check if in Club Hel zone (e.g. coordinates around X=58000, Z=10000 downtown)
                LocationVector pos = player->getPosition();
                if (pos.x > 57900 && pos.x < 58100 && pos.z > 9900 && pos.z < 10100) {
                    if (state.currentTrackFamily != "ClubHel_Juno") {
                        state.currentTrackFamily = "ClubHel_Juno";
                        INFO_LOG(format("Music Transition: Player %1% entered Club Hel. Playing Juno Reactor ambient.") % goId);
                        sendMusicCommand(goId, state.currentTrackFamily, "ambient");
                    }
                } else if (state.currentTrackFamily == "ClubHel_Juno") {
                    state.currentTrackFamily = "4Square"; // Back to default
                    sendMusicCommand(goId, state.currentTrackFamily, "ambient");
                }
            }
        }
    }
}

void AdaptiveMusicSystem::registerThreat(uint32 playerGoId, uint32 threatAmount, uint32 currentMs)
{
    PlayerMusicState& state = m_playerMusicStates[playerGoId];
    state.threatLevel += threatAmount;
    state.lastUpdateMs = currentMs;
    
    if (state.threatLevel > 50 && state.currentState != MUSIC_STATE_COMBAT)
    {
        state.currentState = MUSIC_STATE_COMBAT;
        state.currentTrackFamily = "4Square"; // Stubbed default
        state.currentStemIndex = 1;
        INFO_LOG(format("Music Transition: Player %1% entered combat! Playing Intro stem.") % playerGoId);
        sendMusicCommand(playerGoId, state.currentTrackFamily, "intro");
    }
}

void AdaptiveMusicSystem::clearThreat(uint32 playerGoId, uint32 currentMs)
{
    if (m_playerMusicStates.find(playerGoId) != m_playerMusicStates.end())
    {
        m_playerMusicStates[playerGoId].threatLevel = 0;
        m_playerMusicStates[playerGoId].lastUpdateMs = currentMs;
    }
}

void AdaptiveMusicSystem::sendMusicCommand(uint32 playerGoId, const std::string& family, const std::string& stemType, int index)
{
    PlayerObject* po = sObjMgr.getGOPtr(playerGoId);
    if (po)
    {
        // V16: Wwise / DSP CEF Integration Payload
        float dilation = po->GetTimeDilation();
        std::string payload = (format("[CEF_PAYLOAD] {\"system\":\"audio\", \"action\":\"play\", \"stem\":\"%1%_%2%_%3%\", \"dilation\": %4%}") 
            % family % stemType % index % dilation).str();
        po->getClient().QueueCommand(make_shared<SystemChatMsg>(payload));
    }
}

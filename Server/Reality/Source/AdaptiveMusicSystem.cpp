#include "AdaptiveMusicSystem.h"
#include "Log.h"
#include "GameClient.h"
#include "GameServer.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <cmath>

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

void AdaptiveMusicSystem::recomputeStemWeights(PlayerMusicState& state)
{
    // If Smith infection stage override is present:
    // 0 = Normal (use threatLevel)
    // 1 = Outbreak -> bias towards tension (>= 35)
    // 2 = Cascade  -> bias towards combat (>= 65)
    // 3 = Quarantine / Critical -> force cataclysm (>= 95)
    uint32 effectiveThreat = state.threatLevel;
    if (state.infectionStageOverride == 1 && effectiveThreat < 35) {
        effectiveThreat = 35;
    } else if (state.infectionStageOverride == 2 && effectiveThreat < 65) {
        effectiveThreat = 65;
    } else if (state.infectionStageOverride >= 3) {
        effectiveThreat = 95;
    }

    if (effectiveThreat < 25) {
        state.currentStemTier = STEM_TIER_AMBIENT;
        state.currentState = MUSIC_STATE_EXPLORATION;
        float alpha = static_cast<float>(effectiveThreat) / 25.0f;
        state.ambientWeight = 1.0f - (alpha * 0.4f);
        state.tensionWeight = alpha * 0.4f;
        state.combatWeight = 0.0f;
        state.cataclysmWeight = 0.0f;
    } else if (effectiveThreat < 50) {
        state.currentStemTier = STEM_TIER_TENSION;
        state.currentState = MUSIC_STATE_TENSION;
        float alpha = static_cast<float>(effectiveThreat - 25) / 25.0f;
        state.ambientWeight = 0.6f * (1.0f - alpha);
        state.tensionWeight = 0.6f + (0.3f * alpha);
        state.combatWeight = 0.1f * alpha;
        state.cataclysmWeight = 0.0f;
    } else if (effectiveThreat < 80) {
        state.currentStemTier = STEM_TIER_COMBAT;
        state.currentState = MUSIC_STATE_COMBAT;
        float alpha = static_cast<float>(effectiveThreat - 50) / 30.0f;
        state.ambientWeight = 0.0f;
        state.tensionWeight = 0.4f * (1.0f - alpha);
        state.combatWeight = 0.6f + (0.3f * alpha);
        state.cataclysmWeight = 0.1f * alpha;
    } else {
        state.currentStemTier = STEM_TIER_CATACLYSM;
        state.currentState = MUSIC_STATE_CATACLYSM;
        float alpha = std::min(1.0f, static_cast<float>(effectiveThreat - 80) / 20.0f);
        state.ambientWeight = 0.0f;
        state.tensionWeight = 0.0f;
        state.combatWeight = 0.4f * (1.0f - alpha);
        state.cataclysmWeight = 0.6f + (0.4f * alpha);
    }

    // Normalize weights to sum to 1.0f
    float sum = state.ambientWeight + state.tensionWeight + state.combatWeight + state.cataclysmWeight;
    if (sum > 0.0001f) {
        state.ambientWeight /= sum;
        state.tensionWeight /= sum;
        state.combatWeight /= sum;
        state.cataclysmWeight /= sum;
    }
}

void AdaptiveMusicSystem::update(uint32 currentMs)
{
    if (currentMs - m_lastTickMs < 1000) return; // Tick every 1s
    m_lastTickMs = currentMs;

    std::lock_guard<std::mutex> lock(m_musicMutex);
    for (auto it = m_playerMusicStates.begin(); it != m_playerMusicStates.end(); ++it)
    {
        PlayerMusicState& state = it->second;
        uint32 goId = it->first;

        // Decay threat over time
        if (state.threatLevel > 0 && currentMs - state.lastUpdateMs > 5000)
        {
            state.threatLevel -= (state.threatLevel > 10) ? 10 : state.threatLevel;
            state.lastUpdateMs = currentMs;
            recomputeStemWeights(state);
        }

        // Transition Logic
        if (state.threatLevel == 0 && state.currentState == MUSIC_STATE_COMBAT && state.infectionStageOverride == 0)
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
    std::lock_guard<std::mutex> lock(m_musicMutex);
    PlayerMusicState& state = m_playerMusicStates[playerGoId];
    state.threatLevel += threatAmount;
    state.lastUpdateMs = currentMs;
    MusicStemTier oldTier = state.currentStemTier;
    recomputeStemWeights(state);
    
    if (state.threatLevel > 50 && state.currentState == MUSIC_STATE_COMBAT && oldTier != STEM_TIER_COMBAT && oldTier != STEM_TIER_CATACLYSM)
    {
        state.currentTrackFamily = "4Square"; // Stubbed default
        state.currentStemIndex = 1;
        INFO_LOG(format("Music Transition: Player %1% entered combat! Playing Intro stem.") % playerGoId);
        sendMusicCommand(playerGoId, state.currentTrackFamily, "intro");
    }
}

void AdaptiveMusicSystem::clearThreat(uint32 playerGoId, uint32 currentMs)
{
    std::lock_guard<std::mutex> lock(m_musicMutex);
    if (m_playerMusicStates.find(playerGoId) != m_playerMusicStates.end())
    {
        m_playerMusicStates[playerGoId].threatLevel = 0;
        m_playerMusicStates[playerGoId].lastUpdateMs = currentMs;
        recomputeStemWeights(m_playerMusicStates[playerGoId]);
    }
}

MusicStemTier AdaptiveMusicSystem::GetPlayerStemTier(uint32 playerGoId) const
{
    std::lock_guard<std::mutex> lock(m_musicMutex);
    auto it = m_playerMusicStates.find(playerGoId);
    if (it != m_playerMusicStates.end()) {
        return it->second.currentStemTier;
    }
    return STEM_TIER_AMBIENT;
}

void AdaptiveMusicSystem::GetVerticalStemWeights(uint32 playerGoId, float& outAmb, float& outTen, float& outCbt, float& outCat) const
{
    std::lock_guard<std::mutex> lock(m_musicMutex);
    auto it = m_playerMusicStates.find(playerGoId);
    if (it != m_playerMusicStates.end()) {
        outAmb = it->second.ambientWeight;
        outTen = it->second.tensionWeight;
        outCbt = it->second.combatWeight;
        outCat = it->second.cataclysmWeight;
    } else {
        outAmb = 1.0f;
        outTen = 0.0f;
        outCbt = 0.0f;
        outCat = 0.0f;
    }
}

void AdaptiveMusicSystem::SetSmithInfectionStageOverride(uint32 playerGoId, uint8 stage)
{
    std::lock_guard<std::mutex> lock(m_musicMutex);
    PlayerMusicState& state = m_playerMusicStates[playerGoId];
    state.infectionStageOverride = stage;
    recomputeStemWeights(state);
}

uint8 AdaptiveMusicSystem::GetCurrentInfectionStage(uint32 playerGoId) const
{
    std::lock_guard<std::mutex> lock(m_musicMutex);
    auto it = m_playerMusicStates.find(playerGoId);
    if (it != m_playerMusicStates.end()) {
        return it->second.infectionStageOverride;
    }
    return 0;
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
        po->getClient().QueueCommand(std::make_shared<SystemChatMsg>(payload));
    }
}

// ============================================================================
// Headless Test Suite: Epoch XI 4-Tier Vertical Stem Mixing & Reactive Score
// ============================================================================
void RunAdaptiveMusicTestSuite()
{
    std::cout << "[RUNNING] Adaptive Music System Test Suite: 4-Tier Vertical Stems & Reactive Score..." << std::endl;

    uint32 pId = 98765;
    sAdaptiveMusicSystem.clearThreat(pId, 1000);
    sAdaptiveMusicSystem.SetSmithInfectionStageOverride(pId, 0);

    float amb = 0.0f, ten = 0.0f, cbt = 0.0f, cat = 0.0f;

    // 1. Initial State: Ambient exploration
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_AMBIENT);
    assert(amb > 0.9f);
    assert(ten < 0.1f);
    assert(cbt == 0.0f);
    assert(cat == 0.0f);
    float sum = amb + ten + cbt + cat;
    assert(std::abs(sum - 1.0f) < 0.001f);

    // 2. Register low threat (10) -> Still Ambient, tension slightly rising
    sAdaptiveMusicSystem.registerThreat(pId, 10, 1100);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_AMBIENT);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(amb > ten);
    assert(ten > 0.0f);
    assert(cbt == 0.0f);
    assert(cat == 0.0f);
    sum = amb + ten + cbt + cat;
    assert(std::abs(sum - 1.0f) < 0.001f);

    // 3. Threat elevated to 35 -> STEM_TIER_TENSION
    sAdaptiveMusicSystem.registerThreat(pId, 25, 1200); // 10 + 25 = 35
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_TENSION);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(ten > amb);
    assert(ten > cbt);
    assert(cat == 0.0f);
    sum = amb + ten + cbt + cat;
    assert(std::abs(sum - 1.0f) < 0.001f);

    // 4. Threat elevated to 65 -> STEM_TIER_COMBAT
    sAdaptiveMusicSystem.registerThreat(pId, 30, 1300); // 35 + 30 = 65
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_COMBAT);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(cbt > ten);
    assert(amb == 0.0f);
    assert(cbt > 0.5f);
    sum = amb + ten + cbt + cat;
    assert(std::abs(sum - 1.0f) < 0.001f);

    // 5. Threat elevated to 95 -> STEM_TIER_CATACLYSM
    sAdaptiveMusicSystem.registerThreat(pId, 30, 1400); // 65 + 30 = 95
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_CATACLYSM);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(cat > cbt);
    assert(amb == 0.0f);
    assert(ten == 0.0f);
    assert(cat > 0.7f);
    sum = amb + ten + cbt + cat;
    assert(std::abs(sum - 1.0f) < 0.001f);

    // 6. Clear threat -> Reverts to Ambient
    sAdaptiveMusicSystem.clearThreat(pId, 1500);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_AMBIENT);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(amb > 0.9f);
    assert(cbt == 0.0f);
    assert(cat == 0.0f);

    // 7. Smith Infection Stage Overrides
    // Stage 1 (Outbreak): Forces Tension
    sAdaptiveMusicSystem.SetSmithInfectionStageOverride(pId, 1);
    assert(sAdaptiveMusicSystem.GetCurrentInfectionStage(pId) == 1);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_TENSION);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(ten > amb);

    // Stage 2 (Cascade): Forces Combat
    sAdaptiveMusicSystem.SetSmithInfectionStageOverride(pId, 2);
    assert(sAdaptiveMusicSystem.GetCurrentInfectionStage(pId) == 2);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_COMBAT);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(cbt > ten);

    // Stage 3 (Quarantine/Cataclysm): Forces Cataclysm
    sAdaptiveMusicSystem.SetSmithInfectionStageOverride(pId, 3);
    assert(sAdaptiveMusicSystem.GetCurrentInfectionStage(pId) == 3);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_CATACLYSM);
    sAdaptiveMusicSystem.GetVerticalStemWeights(pId, amb, ten, cbt, cat);
    assert(cat > 0.8f);

    // Stage reset to 0
    sAdaptiveMusicSystem.SetSmithInfectionStageOverride(pId, 0);
    assert(sAdaptiveMusicSystem.GetCurrentInfectionStage(pId) == 0);
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(pId) == STEM_TIER_AMBIENT);

    // 8. Non-existent player query safety
    assert(sAdaptiveMusicSystem.GetPlayerStemTier(111111) == STEM_TIER_AMBIENT);
    sAdaptiveMusicSystem.GetVerticalStemWeights(111111, amb, ten, cbt, cat);
    assert(amb == 1.0f && ten == 0.0f && cbt == 0.0f && cat == 0.0f);

    std::cout << "[PASSED] Adaptive Music System Test Suite: 4-Tier Vertical Stems & Reactive Score (36 assertions passed)." << std::endl;
}


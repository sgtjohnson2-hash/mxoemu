#ifndef MXOEMU_SENTIENT_MAJOR_CHARACTERS_H
#define MXOEMU_SENTIENT_MAJOR_CHARACTERS_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class PlayerObject;
class BotClient;

struct HostHijackRecord
{
    uint32 entityGoId;
    std::string originalHandle;
    std::string originalFaction;
    std::string originalRsi;
    uint32 hijackTimeMs;
};

class SentientMajorCharacters : public Singleton<SentientMajorCharacters>
{
public:
    SentientMajorCharacters();
    ~SentientMajorCharacters();

    void Initialize();
    void Update(float deltaSec);

    // Agent Smith: Autonomous Host Body Hijacking
    bool HijackHost(BotClient* bot, PlayerObject* po, uint32 smithGoId = 0);
    bool HijackNearbyHost(float wx, float wz, uint32 smithGoId = 0);
    bool RevertHijackedHost(uint32 entityGoId);
    bool IsHijackedHost(uint32 entityGoId) const;

    // Morpheus: Inspirational Tactician Aura & Dual-Stance Switching
    void ProcessMorpheusAura(PlayerObject* morpheusPo);
    void ProcessMorpheusStance(BotClient* morpheusBot, PlayerObject* morpheusPo);

    // The Merovingian: Causality AI & Henchmen Wave Orchestration
    void ProcessMerovingianCombat(BotClient* meroBot, PlayerObject* meroPo);
    bool TriggerMerovingianBackdoorEscape(BotClient* meroBot, PlayerObject* meroPo);

    // Trinity: High-Acrobatic Airborne Execution
    void ProcessTrinityCombat(BotClient* trinityBot, PlayerObject* trinityPo);

private:
    mutable std::recursive_mutex m_majorMutex;
    std::map<uint32, HostHijackRecord> m_hijackedHosts;
    uint32 m_lastHenchmenWaveMs{0};
    uint32 m_lastAuraPulseMs{0};
};

#define sSentientCharacters SentientMajorCharacters::getSingleton()

#endif // MXOEMU_SENTIENT_MAJOR_CHARACTERS_H

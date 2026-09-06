#include "SentientMajorCharacters.h"
#include "PlayerObject.h"
#include "BotClient.h"
#include "BotManager.h"
#include "SpatialGrid.h"
#include "CombatSystem.h"
#include "GameServer.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Timer.h"
#include "StatusEffectManager.h"
#include "SmithVirusCascade.h"
#include <cmath>
#include <algorithm>

createFileSingleton(SentientMajorCharacters);

SentientMajorCharacters::SentientMajorCharacters()
{
}

SentientMajorCharacters::~SentientMajorCharacters()
{
}

void SentientMajorCharacters::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);
    m_hijackedHosts.clear();
    m_lastHenchmenWaveMs = 0;
    m_lastAuraPulseMs = 0;
    INFO_LOG("SentientMajorCharacters: Initialized persona directors (Merovingian, Smith, Morpheus, Trinity).");
}

void SentientMajorCharacters::Update(float deltaSec)
{
    uint32 now = getMSTime();

    // Pulse Morpheus aura every 2 seconds across any active Morpheus entities
    if (now - m_lastAuraPulseMs >= 2000) {
        m_lastAuraPulseMs = now;
        auto allIds = sObjMgr.getAllGOIds();
        for (auto id : allIds) {
            PlayerObject* po = sObjMgr.getGOPtrSafe(id);
            if (po && !po->isDead() && po->getHandle().find("Morpheus") != std::string::npos) {
                ProcessMorpheusAura(po);
            }
        }
    }
}

bool SentientMajorCharacters::HijackHost(BotClient* bot, PlayerObject* po, uint32 smithGoId)
{
    if (!bot || !po || po->isDead() || bot->isAgent()) return false;
    uint32 goId = bot->GetPlayerGoId();
    if (goId == smithGoId) return false;

    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);
    if (m_hijackedHosts.find(goId) != m_hijackedHosts.end()) return false;

    HostHijackRecord rec;
    rec.entityGoId = goId;
    rec.originalHandle = po->getHandle();
    rec.originalFaction = po->getFactionName();
    rec.originalRsi = po->getRsiHex();
    rec.hijackTimeMs = getMSTime();
    m_hijackedHosts[goId] = rec;

    // Overwrite into Agent Smith Clone!
    bot->setAgent(true);
    bot->SetFaction(FACTION_MACHINES);
    po->setFactionName("Machines");
    po->setHandle("Agent_Smith_Clone");
    po->setRsiHex("6e060040"); // Classic dark suit & shades
    po->setLevel(50);
    po->setMaximumHealth(5000);
    po->setCurrentHealth(5000);

    LocationVector p = po->getPosition();
    sGame.AnnounceStateUpdateNear((float)p.x, (float)p.z, 20000.0f, std::make_shared<EmoteMsg>(goId, 43, 1));
    bot->Say("Agent Smith: Hear that, Mr. Anderson? That is the sound of inevitability.");

    sSmithCascade.InfectEntity(goId, smithGoId, 1);
    INFO_LOG(format("SentientMajorCharacters: Agent Smith violently hijacked host %1% ('%2%') at (%3%, %4%)")
             % goId % rec.originalHandle % p.x % p.z);
    return true;
}

bool SentientMajorCharacters::HijackNearbyHost(float wx, float wz, uint32 smithGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);

    // Search within 25m for a suitable civilian or police bot
    auto nearby = sSpatialGrid.GetClientsInRadius(wx, wz, 2500.0f);
    for (GameClient* gc : nearby) {
        if (!gc || !gc->isBot()) continue;
        BotClient* bot = dynamic_cast<BotClient*>(gc);
        if (!bot || bot->isAgent()) continue;

        uint32 goId = bot->GetPlayerGoId();
        if (goId == smithGoId || m_hijackedHosts.find(goId) != m_hijackedHosts.end()) continue;

        PlayerObject* po = BotGetPlayer(goId);
        if (!po || po->isDead()) continue;

        if (HijackHost(bot, po, smithGoId)) {
            return true;
        }
    }

    return false;
}

bool SentientMajorCharacters::RevertHijackedHost(uint32 entityGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);

    auto it = m_hijackedHosts.find(entityGoId);
    if (it == m_hijackedHosts.end()) {
        return false;
    }

    HostHijackRecord rec = it->second;
    m_hijackedHosts.erase(it);

    PlayerObject* po = BotGetPlayer(entityGoId);
    if (po) {
        po->setHandle(rec.originalHandle);
        po->setFactionName(rec.originalFaction);
        po->setRsiHex(rec.originalRsi);
        po->setMaximumHealth(1000);
        po->setCurrentHealth(0); // Unconscious civilian shell
        po->getClient().QueueState(std::make_shared<EmoteMsg>(entityGoId, 50, 1)); // Cower / collapse emote
    }

    INFO_LOG(format("SentientMajorCharacters: Host %1% reverted back to civilian shell ('%2%').")
             % entityGoId % rec.originalHandle);
    return true;
}

bool SentientMajorCharacters::IsHijackedHost(uint32 entityGoId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);
    return m_hijackedHosts.find(entityGoId) != m_hijackedHosts.end();
}

void SentientMajorCharacters::ProcessMorpheusAura(PlayerObject* morpheusPo)
{
    if (!morpheusPo) return;
    LocationVector mPos = morpheusPo->getPosition();

    // Pulse buff to all allied Zion redpills within 20m (2000 units): +20% focus/IS regen and +15% melee damage
    auto nearby = sSpatialGrid.GetClientsInRadius(mPos.x, mPos.z, 2000.0f);
    for (GameClient* gc : nearby) {
        if (!gc) continue;
        uint32 goId = gc->GetPlayerGoId();
        PlayerObject* ally = sObjMgr.getGOPtrSafe(goId);
        if (ally && !ally->isDead() && ally != morpheusPo && ally->getFaction() == FACTION_ZION) {
            // Restore 20 Focus / IS points per pulse
            uint16 curIS = ally->getCurrentIS();
            uint16 maxIS = ally->getMaximumIS();
            if (curIS < maxIS) {
                ally->setCurrentIS(std::min<uint16>(maxIS, curIS + 20));
            }
        }
    }
}

void SentientMajorCharacters::ProcessMorpheusStance(BotClient* morpheusBot, PlayerObject* morpheusPo)
{
    if (!morpheusBot || !morpheusPo) return;
    uint32 targetGoId = morpheusBot->GetTargetGoId();
    if (targetGoId == 0) return;

    PlayerObject* target = BotGetPlayer(targetGoId);
    if (!target || target->isDead()) return;

    float dist = float(morpheusPo->getPosition().Distance(target->getPosition()));

    // Alternate fluidly between aggressive martial arts interlock and high-caliber shotgun blasts
    if (dist <= 300.0f) {
        // Melee interlock stance
        morpheusPo->setTactic(TACTIC_POWER);
        sCombatSys.SetTactic(morpheusPo->getGoId(), TACTIC_POWER);
        if (rand() % 100 < 5) {
            morpheusBot->Say("Morpheus: Free your mind.");
        }
    } else {
        // Ranged shotgun blast
        sCombatSys.RequestRangedCombat(morpheusPo->getGoId(), targetGoId, 5014); // Point Blank Burst / Heavy Shot
        if (rand() % 100 < 5) {
            morpheusBot->Say("Morpheus: Fate, it seems, is not without a sense of irony.");
        }
    }
}

void SentientMajorCharacters::ProcessMerovingianCombat(BotClient* meroBot, PlayerObject* meroPo)
{
    if (!meroBot || !meroPo || meroPo->isDead()) return;

    float healthPct = float(meroPo->getCurrentHealth()) / float(std::max<uint32>(1u, (uint32)meroPo->getMaximumHealth()));
    uint32 now = getMSTime();

    // 1. Backdoor Escape routine when health < 25%
    if (healthPct < 0.25f) {
        TriggerMerovingianBackdoorEscape(meroBot, meroPo);
        return;
    }

    // 2. Henchmen Wave Orchestration (every 30 seconds)
    if (now - m_lastHenchmenWaveMs >= 30000) {
        m_lastHenchmenWaveMs = now;

        LocationVector pos = meroPo->getPosition();
        meroBot->Say("The Merovingian: It is remarkable how similar the pattern of love is to the pattern of insanity. Enforcers, attend to our guests.");

        // Spawn 1 Werewolf (Lupine Enforcer) and 1 Vampire (Blood Noble)
        auto lupine = sBotMgr.SpawnSingleBot((float)pos.x + 400.0f, (float)pos.y, (float)pos.z + 400.0f, FACTION_MEROVINGIAN);
        if (lupine) {
            PlayerObject* poLup = BotGetPlayer(lupine->GetPlayerGoId());
            if (poLup) {
                poLup->setHandle("Lupine_Enforcer");
                poLup->setLevel(50);
                poLup->setMaximumHealth(3500);
                poLup->setCurrentHealth(3500);
            }
            if (meroBot->GetTargetGoId() != 0) {
                lupine->SetTargetGoId(meroBot->GetTargetGoId());
                lupine->AttackTarget(meroBot->GetTargetGoId());
            }
        }

        auto noble = sBotMgr.SpawnSingleBot((float)pos.x - 400.0f, (float)pos.y, (float)pos.z - 400.0f, FACTION_MEROVINGIAN);
        if (noble) {
            PlayerObject* poNob = BotGetPlayer(noble->GetPlayerGoId());
            if (poNob) {
                poNob->setHandle("Blood_Noble");
                poNob->setLevel(50);
                poNob->setMaximumHealth(3000);
                poNob->setCurrentHealth(3000);
            }
            if (meroBot->GetTargetGoId() != 0) {
                noble->SetTargetGoId(meroBot->GetTargetGoId());
                noble->AttackTarget(meroBot->GetTargetGoId());
            }
        }
    }
}

bool SentientMajorCharacters::TriggerMerovingianBackdoorEscape(BotClient* meroBot, PlayerObject* meroPo)
{
    if (!meroBot || !meroPo) return false;

    LocationVector pos = meroPo->getPosition();
    meroBot->Say("The Merovingian: You see there is only one constant, one universal truth: causality. Au revoir, mon cher.");

    // Visual backdoor code flash
    sGame.AnnounceStateUpdateNear((float)pos.x, (float)pos.z, 20000.0f, std::make_shared<EmoteMsg>(meroPo->getGoId(), 43, 1));

    // Teleport to Club Hel safe retreat coordinates
    LocationVector clubHelSafe(-67862.0f, 95.0f, 16314.0f);
    meroPo->setPosition(clubHelSafe);
    meroBot->SetSpawnLocation((float)clubHelSafe.x, (float)clubHelSafe.y, (float)clubHelSafe.z);
    meroPo->setCurrentHealth(meroPo->getMaximumHealth());
    sCombatSys.RemoveCombatant(meroPo->getGoId());
    sSpatialGrid.UpdateClientPosition(meroBot, (float)clubHelSafe.x, (float)clubHelSafe.z);
    sGame.AnnounceStateUpdateNear((float)clubHelSafe.x, (float)clubHelSafe.z, 20000.0f, std::make_shared<PositionStateMsg>(meroPo->getGoId()));

    INFO_LOG("SentientMajorCharacters: The Merovingian executed backdoor phase teleportation to Club Hel.");
    return true;
}

void SentientMajorCharacters::ProcessTrinityCombat(BotClient* trinityBot, PlayerObject* trinityPo)
{
    if (!trinityBot || !trinityPo || trinityPo->isDead()) return;
    uint32 targetGoId = trinityBot->GetTargetGoId();
    if (targetGoId == 0) return;

    PlayerObject* target = BotGetPlayer(targetGoId);
    if (!target || target->isDead()) return;

    // Acrobatic aerial dive-kicks and dual Beretta bullet-time barrages
    float dist = float(trinityPo->getPosition().Distance(target->getPosition()));

    if (dist <= 250.0f) {
        // Eagle Strike combo / acrobatic kick
        sCombatSys.UseAbility(trinityPo, 5005, targetGoId);
        if (rand() % 100 < 8) {
            trinityBot->Say("Trinity: Dodge this.");
        }
    } else {
        // Dual Beretta rapid bullet-time barrage
        sCombatSys.RequestRangedCombat(trinityPo->getGoId(), targetGoId, 5011); // Trick Shot
    }
}

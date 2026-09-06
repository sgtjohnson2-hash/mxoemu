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
    m_viralImmunity.clear();
    m_lastHenchmenWaveMs = 0;
    m_lastAuraPulseMs = 0;
    INFO_LOG("SentientMajorCharacters: Initialized persona directors (Merovingian, Smith, Morpheus, Trinity).");
}

bool SentientMajorCharacters::HasViralImmunity(uint32 entityGoId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);
    auto it = m_viralImmunity.find(entityGoId);
    if (it == m_viralImmunity.end()) return false;
    return getMSTime() < it->second;
}

void SentientMajorCharacters::SetViralImmunity(uint32 entityGoId, uint32 durationMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_majorMutex);
    m_viralImmunity[entityGoId] = getMSTime() + durationMs;
}

bool SentientMajorCharacters::IsInMorpheusAura(float wx, float wz, float radius) const
{
    float radiusSq = radius * radius;
    auto allIds = sObjMgr.getAllGOIds();
    for (auto id : allIds) {
        PlayerObject* po = sObjMgr.getGOPtrSafe(id);
        if (po && !po->isDead() && po->getHandle().find("Morpheus") != std::string::npos) {
            LocationVector mPos = po->getPosition();
            float dx = float(mPos.x) - wx;
            float dz = float(mPos.z) - wz;
            if ((dx * dx + dz * dz) <= radiusSq) {
                return true;
            }
        }
    }
    return false;
}

bool SentientMajorCharacters::CheckMorpheusAuraDeflection(uint32 targetGoId, uint32 smithGoId)
{
    PlayerObject* targetPo = sObjMgr.getGOPtrSafe(targetGoId);
    if (!targetPo) return false;
    LocationVector tPos = targetPo->getPosition();
    if (!IsInMorpheusAura((float)tPos.x, (float)tPos.z, 2500.0f)) return false;

    // 95% deflection check under Morpheus's Aura of Free Will
    if ((rand() % 100) < 95) {
        PlayerObject* smithPo = sObjMgr.getGOPtrSafe(smithGoId);
        if (smithPo) {
            LocationVector sPos = smithPo->getPosition();
            float dx = float(sPos.x - tPos.x);
            float dz = float(sPos.z - tPos.z);
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > 0.01f) {
                dx /= dist;
                dz /= dist;
            } else {
                dx = 1.0f;
                dz = 0.0f;
            }
            float kbX = float(sPos.x) + dx * 500.0f;
            float kbZ = float(sPos.z) + dz * 500.0f;
            smithPo->setPosition(LocationVector(kbX, sPos.y, kbZ));
            auto smithBot = sBotMgr.GetBotByGOID(smithGoId);
            if (smithBot) {
                smithBot->StopInfecting();
                smithBot->MoveTo(kbX, (float)sPos.y, kbZ);
                smithBot->Emote(51); // Knockdown stagger
            }
            sGame.AnnounceStateUpdateNear((float)kbX, (float)kbZ, 20000.0f, std::make_shared<EmoteMsg>(smithGoId, 43, 1));
        }
        sBotMgr.LogCombat("[AURA OF FREE WILL] Morpheus's Aura deflected Agent Smith viral infection! Kinetic shockwave staggered clone.");
        return true; // Deflected!
    }
    return false;
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

    // Check 30s viral immunity window post-cleansing
    if (HasViralImmunity(goId)) {
        return false;
    }

    // Check Morpheus Aura of Free Will (25m radius, 95% deflection check)
    if (CheckMorpheusAuraDeflection(goId, smithGoId)) {
        return false;
    }

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

    PlayerObject* po = BotGetPlayer(entityGoId);
    if (!po) return false;

    auto it = m_hijackedHosts.find(entityGoId);
    HostHijackRecord rec;
    if (it != m_hijackedHosts.end()) {
        rec = it->second;
        m_hijackedHosts.erase(it);
    } else {
        // Fallback for naturally or command-spawned Smith clones
        rec.entityGoId = entityGoId;
        rec.originalHandle = "Civilian_" + std::to_string(entityGoId % 10000);
        rec.originalFaction = "Civilian";
        rec.originalRsi = "00000000";
        rec.hijackTimeMs = 0;
    }

    po->setHandle(rec.originalHandle);
    po->setFactionName(rec.originalFaction);
    po->setRsiHex(rec.originalRsi);
    po->setMaximumHealth(1000);
    po->setCurrentHealth(150); // Baseline conscious civilian state (1000 max, 150 current)
    
    // Apply 30s Viral Immunity status buff
    SetViralImmunity(entityGoId, 30000);

    LocationVector pos = po->getPosition();
    // Green Matrix waterfall cleansing FX
    sGame.AnnounceStateUpdateNear((float)pos.x, (float)pos.z, 20000.0f, std::make_shared<EmoteMsg>(entityGoId, 45, 1));

    auto bot = sBotMgr.GetBotByGOID(entityGoId);
    if (bot) {
        bot->setAgent(false);
        bot->StopInfecting();
        if (rec.originalFaction == "Machines") bot->SetFaction(FACTION_MACHINES);
        else if (rec.originalFaction == "Zion") bot->SetFaction(FACTION_ZION);
        else if (rec.originalFaction == "Merovingian") bot->SetFaction(FACTION_MEROVINGIAN);
        else bot->SetFaction(FACTION_NONE);
    }

    // Trigger memory awakening / Redpill recruit check on BotManager
    sBotMgr.HandleCleanseAwakening(entityGoId);

    INFO_LOG(format("SentientMajorCharacters: Host %1% reverted back to civilian shell ('%2%') with 30s viral immunity.")
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

    // Pulse buff to all allied Zion redpills and civilians within 25m (2500 units)
    auto nearby = sSpatialGrid.GetClientsInRadius((float)mPos.x, (float)mPos.z, 2500.0f);
    for (GameClient* gc : nearby) {
        if (!gc) continue;
        uint32 goId = gc->GetPlayerGoId();
        PlayerObject* target = sObjMgr.getGOPtrSafe(goId);
        if (!target || target->isDead() || target == morpheusPo) continue;

        if (target->getFaction() == FACTION_ZION) {
            // Restore +35 Focus / IS points per pulse to allied Zion redpills
            uint16 curIS = target->getCurrentIS();
            uint16 maxIS = target->getMaximumIS();
            if (curIS < maxIS) {
                target->setCurrentIS(std::min<uint16>(maxIS, curIS + 35));
            }
            // Defense tactic for melee parry and ranged deflection boost (+25%)
            target->setTactic(TACTIC_DEFENSE);
        } else if (gc->isBot() || target->getFactionName() == "Civilian" || target->getHandle().find("Civilian") != std::string::npos) {
            // Psychological Trauma & Panic Cleansing
            auto bot = sBotMgr.GetBotByGOID(goId);
            if (bot) {
                bot->SetPanicking(false);
                bot->SetFearLevel(0.15f); // Calm alertness
                target->getClient().QueueState(std::make_shared<EmoteMsg>(goId, 0, 1)); // Clear cower emote
                LocationVector hl = sBotMgr.GetNearestHardline((float)target->getPosition().x, (float)target->getPosition().z);
                if (hl.x != 0.0f || hl.z != 0.0f) {
                    bot->SetEvacTarget(hl);
                }
                if (rand() % 12 == 0) {
                    bot->Say("Civilian: Morpheus is with us! Don't look back, get to the Hardlines!");
                }
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

    LocationVector pos = trinityPo->getPosition();
    uint32 targetGoId = trinityBot->GetTargetGoId();

    // Target Prioritization: Prioritize active Smith infectors channeling on victims
    if (targetGoId == 0) {
        auto nearby = sSpatialGrid.GetClientsInRadius((float)pos.x, (float)pos.z, 2500.0f);
        for (GameClient* gc : nearby) {
            if (!gc || !gc->isBot()) continue;
            auto enemyBot = sBotMgr.GetBotByGOID(gc->GetPlayerGoId());
            if (enemyBot && enemyBot->IsInfecting()) {
                targetGoId = enemyBot->GetPlayerGoId();
                trinityBot->SetTargetGoId(targetGoId);
                break;
            }
        }
        // Fallback: any Smith clone or hijacked host
        if (targetGoId == 0) {
            for (GameClient* gc : nearby) {
                if (!gc || !gc->isBot()) continue;
                PlayerObject* enemyPo = BotGetPlayer(gc->GetPlayerGoId());
                if (enemyPo && !enemyPo->isDead() && (enemyPo->getHandle().find("Smith") != std::string::npos || IsHijackedHost(enemyPo->getGoId()))) {
                    targetGoId = enemyPo->getGoId();
                    trinityBot->SetTargetGoId(targetGoId);
                    break;
                }
            }
        }
    }

    if (targetGoId == 0) return;
    PlayerObject* target = BotGetPlayer(targetGoId);
    if (!target || target->isDead()) {
        trinityBot->SetTargetGoId(0);
        return;
    }

    // Acrobatic aerial dive-kicks and dual Beretta bullet-time barrages
    float dist = float(trinityPo->getPosition().Distance(target->getPosition()));

    if (dist <= 250.0f) {
        // High-angle dive kick (Ability 5005 - Eagle Strike) interrupting active infectors
        sCombatSys.UseAbility(trinityPo, 5005, targetGoId);
        auto enemyBot = sBotMgr.GetBotByGOID(targetGoId);
        if (enemyBot && enemyBot->IsInfecting()) {
            enemyBot->StopInfecting();
            enemyBot->SetTargetGoId(0);
            sBotMgr.LogCombat((format("[TRINITY AIR SUPPORT] Trinity executed Eagle Strike dive-kick on %1%! Infection broken.") % target->getHandle()).str());
        }
        if (rand() % 100 < 15) {
            trinityBot->Say("Trinity: Dodge this.");
        }
    } else {
        // Dual Beretta rapid bullet-time suppression fire (Ability 5011 - Trick Shot)
        sCombatSys.RequestRangedCombat(trinityPo->getGoId(), targetGoId, 5011);
    }
}

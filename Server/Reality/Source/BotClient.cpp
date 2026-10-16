#include <memory>
#include "BotClient.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "GameServer.h"
#include "DataLoader.h"
#include "BehaviorTree.h"
#include "MessageTypes.h"
#include "GameServer.h"
#include "MissionSystem.h"
#include "CombatSystem.h"
#include "HackerSystem.h"
#include "Timer.h"
#include "NavMeshMgr.h"
#include "SpatialGrid.h"
#include "FactionWarManager.h"
#include "StatusEffectManager.h"
#include "HackerSystem.h"
#include "Database/DatabaseEnv.h"
#include "SniffDump.h"
#include "AI/PedestrianEcology.h"
#include "AI/RVO2Steering.h"
#include "AI/CoverSystem.h"
#include "AI/SensoryPerceptionSystem.h"
#include "BotTester.h"
#include <cmath>
#include <fstream>
#include <typeinfo>
#include "Config.h"

QTable BotClient::s_qTable;

// sObjMgr.getGOPtr no longer throws on missing objects, it returns NULL
PlayerObject* BotGetPlayer(uint32 goId)
{
    if (goId == 0)
        return NULL;
    return sObjMgr.getGOPtr(goId);
}

class GOAPFindTargetAction : public GOAPAction {
public:
    GOAPFindTargetAction() : GOAPAction("FindTarget", 2.0f) {
        effects["TargetAcquired"] = true;
    }
    bool Execute(BotClient* bot) override {
        ActionFindTarget find;
        find.Tick(bot);
        return true;
    }
};

class GOAPAttackAction : public GOAPAction {
public:
    GOAPAttackAction() : GOAPAction("Attack", 1.0f) {
        preconditions["TargetAcquired"] = true;
        effects["EnemyKilled"] = true;
    }
    bool Execute(BotClient* bot) override {
        ActionEngageTarget engage;
        engage.Tick(bot);
        ActionCombatCycle cycle;
        cycle.Tick(bot);
        return true;
    }
};

BotClient::BotClient(uint64 charUID) 
    : GameClient(sockaddr_in(), nullptr), m_targetGoId(0), m_stateTimer(0), 
      m_nextActionTime(0), m_spawnX(0.0f), m_spawnY(0.0f), m_spawnZ(0.0f), m_faction(FACTION_ZION), m_crewLeaderId(0), m_crewId(0), m_possessedBy(0)
{
    m_characterUID = charUID;
    m_personality = sDataLoader.GetPersonalityProfile(charUID);

    // Create the player object
    m_playerGoId = sObjMgr.constructPlayer(this, m_characterUID, true);
    if (m_playerGoId != 0)
    {
        sObjMgr.getGOPtr(m_playerGoId)->InitializeWorld();
        // Bots don't need to populate their network queue with the entire world state
        
        // Dummy encryption to avoid errors
        m_encryptionInitialized = true;
    }
    else
    {
        ERROR_LOG(format("Failed to construct player object for BotClient charUID %1%") % m_characterUID);
    }
}

BotClient::~BotClient()
{
    if (m_playerGoId != 0)
    {
        if (sObjMgr.getGOPtrSafe(m_playerGoId) != nullptr)
        {
            sObjMgr.destroyObject(m_playerGoId);
        }
        m_playerGoId = 0;
    }
}

void BotClient::FlushQueue(bool alsoResend)
{
    // Bots have no real socket, so normally we just drop queued messages. But
    // when packet sniffing is on (--sniff), serialize each queued combat/state
    // message to protocol_dump.json first: this captures the REAL wire bytes
    // (real view ids, health values, FX ids) that bot-vs-bot combat generates,
    // giving the Python verification bench something to assert against without
    // needing a full network handshake or crypto.
    if (g_sniffPackets)
    {
        std::ofstream dumpFile("protocol_dump.json", std::ios::app);
        if (dumpFile.is_open())
        {
            // SniffTrySerialize whitelists combat/interlock/ability messages and
            // skips non-combat traffic whose toBuf() would AV on a bot receiver.
            for (auto &qs : m_queuedStates)
                SniffTrySerialize(qs.stateData, "state", dumpFile);
            for (auto &qc : m_queuedCommands)
                SniffTrySerialize(qc.data, "command", dumpFile);
        }
    }

    m_queuedCommands.clear();
    m_queuedStates.clear();
}

void BotClient::CheckAndResend()
{
    // Do nothing. Overrides GameClient to prevent packet resends.
}

void BotClient::StartInfecting(uint32 targetGoId, uint32 durationMs)
{
    m_infectingTargetGoId = targetGoId;
    m_infectChannelStartMs = getMSTime();
    m_infectChannelDurationMs = durationMs;
}

void BotClient::StopInfecting()
{
    m_infectingTargetGoId = 0;
    m_infectChannelStartMs = 0;
}

// Someone this bot should treat as an enemy. Other bots are ignored unless they
// are already swinging at us, or Bots.NpcVsNpc is on. Civilians and quest givers never qualify.
static bool HostileTo(BotClient* bot, PlayerObject* me, PlayerObject* other)
{
    if (!bot || !me || !other || other == me || other->isDead()) return false;
    std::string f = other->getFactionName();
    if (f == "FRIENDLY") return false;
    if (f == "Civilian" || other->getHandle().find("Civilian") != std::string::npos)
        return false;

    bool onUs = other->getTargetGoId() == me->getGoId() || other->getInterlockPartner() == me->getGoId();
    if (!onUs && std::fabs(other->getPosition().y - me->getPosition().y) > 400.0)
        return false;
    if (onUs)
        return true;

    int mine = me->getFaction();
    int theirs = other->getFaction();
    if (mine == FACTION_NONE || theirs == FACTION_NONE || mine == theirs)
        return false;

    // A player of another faction is a fight. Other bots are not, unless NpcVsNpc is on.
    if (other->getClient().isBot())
    {
        static const bool npc = sConfig.GetBoolDefault("Bots.NpcVsNpc", false);
        if (!npc) return false;
    }
    return true;
}

// Closest real threat within 25 m, preferring whoever is already fighting us.
// If an ally next to us is in a fight with a player, that player is the threat.
static uint32 NoticeThreat(BotClient* bot, PlayerObject* me)
{
    std::vector<GameClient*> nearby = sSpatialGrid.GetClientsInRadius(
        (float)me->getPosition().x, (float)me->getPosition().z, 2500.0f, me->getClient().m_instanceId);
    uint32 best = 0;
    float bestScore = 1.0e9f;
    for (GameClient* c : nearby)
    {
        if (!c || c == bot) continue;
        PlayerObject* o = c->getPlayer();
        if (!o) o = BotGetPlayer(c->GetPlayerGoId());
        if (!o || o == me || o->isDead()) continue;

        float dx = (float)(o->getPosition().x - me->getPosition().x);
        float dz = (float)(o->getPosition().z - me->getPosition().z);
        float d = std::sqrt(dx * dx + dz * dz);

        if (!HostileTo(bot, me, o))
        {
            uint32 partner = o->getInterlockPartner();
            if (partner == 0) continue;
            PlayerObject* foe = BotGetPlayer(partner);
            if (!foe || foe->isDead() || foe->getClient().isBot()) continue;
            if (!HostileTo(bot, me, foe)) continue;
            o = foe;
            d += 200.0f; // a direct threat sorts ahead of an assist
        }

        bool onUs = o->getTargetGoId() == me->getGoId() || o->getInterlockPartner() == me->getGoId();
        if (!onUs && WorldMobWontAggro(me, o, d)) continue;
        float score = onUs ? d - 5000.0f : d;
        if (score < bestScore)
        {
            bestScore = score;
            best = o->getGoId();
        }
    }
    return best;
}

void BotClient::UpdateBotAI(float deltaSeconds)
{

    if (m_playerGoId == 0) return;
    m_deltaSeconds = deltaSeconds;

    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me)
        return;

    LocationVector loc = me->getPosition();


    //drive event processing (scheduled respawn, regen, status effects) and
    //drop packets that combat queued at us - no real socket behind a bot
    me->Update();
    

    ClearQueues();

    // Tester bots run their own player-like brain (BotTester.cpp), including while dead.
    if (m_tester)
    {
        m_tester->Tick(deltaSeconds);
        return;
    }

    if (me->isDead())
    {
        m_targetGoId = 0; //respawn fires through the normal EVENT_RESPAWN path
        SetLocomotionAnimation(0);
        return;
    }
    
    // Director Mode Possession: Skip AI logic if controlled by a GM
    if (m_possessedBy != 0) {
        return;
    }
    

    // Item 33: Stun/Root Effects
    if (sStatusEffectManager.HasEffect(m_playerGoId, EFFECT_STUN)) {
        return; // Skip AI evaluation while stunned
    }

    // 1. Civilian Behavior Branch - Pedestrian Ecology & Circadian Routines
    if (me->getFactionName() == "Civilian")
    {
        sPedestrianEcology.UpdateCivilian(this, deltaSeconds);
        return;
    }

    // Street-life mode (default): fight back when engaged, otherwise stroll around home.
    // The legacy ecology/viral/sentient/active-inference pipeline below mostly generated
    // chat text and short-lived spawns; it only runs with World.LegacySimulation = 1.
    static const bool s_legacySim = sConfig.GetBoolDefault("World.LegacySimulation", false);
    if (!s_legacySim)
    {
        auto stepToward = [&](float tx, float tz, float speed, uint8 anim)
        {
            LocationVector loc = me->getPosition();
            float dx = tx - (float)loc.x;
            float dz = tz - (float)loc.z;
            float len = std::sqrt(dx * dx + dz * dz);
            if (len < 40.0f)
            {
                SetLocomotionAnimation(0);
                return;
            }
            dx /= len; dz /= len;
            float dt = (deltaSeconds > 0.0001f) ? deltaSeconds : 0.1f;
            float nx = (float)loc.x + dx * speed * dt;
            float nz = (float)loc.z + dz * speed * dt;
            if (sSpatialGrid.CheckCollision(nx, nz, 1.0f, m_playerGoId))
            {
                nx = (float)loc.x + (-dz) * 80.0f;
                nz = (float)loc.z + dx * 80.0f;
                if (sSpatialGrid.CheckCollision(nx, nz, 1.0f, m_playerGoId))
                    return;
            }
            loc.x = nx;
            loc.z = nz;
            loc.rot = std::atan2(dx, dz);
            me->setPosition(loc);
            sSpatialGrid.UpdateClientPosition(this, nx, nz);
            SetLocomotionAnimation(anim);
            uint32 nowMs = getMSTime();
            if (nowMs - m_lastRoamBroadcastMs >= 150)
            {
                m_lastRoamBroadcastMs = nowMs;
                sGame.AnnounceStateUpdateNear(loc.x, loc.z, 20000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
                sGame.AnnounceStateUpdateNear(loc.x, loc.z, 20000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, anim, loc.getMxoRot()));
            }
        };

        float homeX = m_hasHome ? m_homeX : m_spawnX;
        float homeZ = m_hasHome ? m_homeZ : m_spawnZ;
        float homeDx = (float)me->getPosition().x - homeX;
        float homeDz = (float)me->getPosition().z - homeZ;
        bool farFromHome = (homeDx * homeDx + homeDz * homeDz) > (8000.0f * 8000.0f);

        if (!m_passive && me->getInterlockPartner() == 0 && farFromHome)
        {
            m_targetGoId = 0;
            m_pathWaypoints.clear();
            stepToward(homeX, homeZ, 280.0f, 3);
            return;
        }

        if (m_targetGoId == 0 && !m_passive && BotManager::getSingletonPtr()->IsAggroEnabled()
            && me->getFactionName() != "Civilian")
        {
            uint32 noticed = NoticeThreat(this, me);
            if (noticed != 0)
                m_targetGoId = noticed;
        }

        if (m_targetGoId != 0)
        {
            PlayerObject* target = BotGetPlayer(m_targetGoId);
            float hpPct = (me->getMaximumHealth() > 0)
                ? float(me->getCurrentHealth()) / float(me->getMaximumHealth()) : 1.0f;
            if (target && !target->isDead() && hpPct < 0.25f && me->getInterlockPartner() == 0)
            {
                m_targetGoId = 0;
                m_pathWaypoints.clear();
                sCombatSys.RemoveCombatant(m_playerGoId);
                stepToward(homeX, homeZ, 520.0f, 30);
                return;
            }
            if (target && !target->isDead() && float(me->getPosition().Distance(target->getPosition())) < 6000.0f)
            {
                m_pathWaypoints.clear();
                ActionEngageTarget engage;
                if (engage.Tick(this) == NodeStatus::SUCCESS)
                {
                    ActionCombatCycle cycle;
                    cycle.Tick(this);
                }
                return;
            }
            m_targetGoId = 0;
            SetLocomotionAnimation(0);
        }
        else if (!m_passive && BotManager::getSingletonPtr()->IsAggroEnabled() && me->getFactionName() != "Civilian")
        {
            ActionFindTarget find;
            if (find.Tick(this) == NodeStatus::SUCCESS)
            {
                m_pathWaypoints.clear();
                ActionEngageTarget engage;
                if (engage.Tick(this) == NodeStatus::SUCCESS)
                {
                    ActionCombatCycle cycle;
                    cycle.Tick(this);
                }
                return;
            }
        }
        if (m_passive)
            return; // dojo dummy and mission contacts stand still until attacked

        // Strike squad bots (m_crewId != 0) take or hold a hardline on this block.
        // Regular ambient street NPCs patrol their sidewalk circuits via RoamAndSwarm.
        if (m_crewId != 0 && me->getFaction() >= FACTION_ZION && me->getFaction() <= FACTION_MEROVINGIAN)
        {
            const ControlNode* goal = nullptr;
            float best = 8000.0f;
            for (const auto& kv : sFactionWarMgr.GetControlNodes())
            {
                const ControlNode& node = kv.second;
                float dx = node.x - (float)me->getPosition().x;
                float dz = node.z - (float)me->getPosition().z;
                float d = std::sqrt(dx * dx + dz * dz);
                if (d > 8000.0f) continue;
                bool enemy = node.controllingFaction != (uint32)me->getFaction();
                bool defend = !enemy && (node.isContested || node.captureProgress > 0.01f);
                if (!enemy && !defend) continue;
                if (d < best)
                {
                    best = d;
                    goal = &node;
                }
            }
            if (goal)
            {
                if (best > 1200.0f)
                {
                    m_pathWaypoints.clear();
                    stepToward(goal->x, goal->z, 180.0f, 3);
                }
                else
                    SetLocomotionAnimation(0); // on the uplink: hold it
                return;
            }
        }

        RoamAndSwarm(deltaSeconds);
        return;
    }

    // 1. Civilian Behavior Branch - Pedestrian Ecology & Circadian Routines
    if (me->getFactionName() == "Civilian") {
        sPedestrianEcology.UpdateCivilian(this, deltaSeconds);
        return;
    }

    // 1a. Awakened Redpill / Rescued Combatant Hardline Evacuation
    if (m_isEvacuating) {
        LocationVector myPos = me->getPosition();
        float dx = m_evacTarget.x - (float)myPos.x;
        float dz = m_evacTarget.z - (float)myPos.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist <= 300.0f) {
            sPedestrianEcology.ExecuteCivilianJackout(me->getGoId());
            return;
        }
        float evacSpeed = 8.5f;
        dx /= dist;
        dz /= dist;
        MoveTo((float)myPos.x + dx * evacSpeed * deltaSeconds * 100.0f, (float)myPos.y, (float)myPos.z + dz * evacSpeed * deltaSeconds * 100.0f);
        return;
    }

    float healthPct = float(me->getCurrentHealth()) / float(me->getMaximumHealth());

    // 2. Agent Viral Assimilation Trigger
    if (m_isAgent || (m_faction == FACTION_MACHINES && healthPct < 0.5f)) {
        ActionAgentInfect infect;
        NodeStatus status = infect.Tick(this);
        if (status == NodeStatus::SUCCESS || status == NodeStatus::RUNNING) {
            return;
        }
    }

    // 2b. Zion Viral Disruption Countermeasure
    if (m_faction == FACTION_ZION) {
        ActionDisruptInfection disrupt;
        if (disrupt.Tick(this) == NodeStatus::SUCCESS) {
            return;
        }
    }

    // 3. Active Inference Intention Selection (Unified Pipeline)
    std::vector<ActionPolicy> policies;

    // Somatic State Update
    float danger = 0.0f;
    if (healthPct < 0.3f) danger += 0.5f;
    if (m_targetGoId != 0) danger += 0.5f;
    danger *= (1.0f - m_personality->fearlessness * 0.8f);
    m_somatic.UpdateSomaticStates(danger, 0.1f);

    // Roam & Mission Utility
    policies.push_back({"ROAM", 0.2f + (m_personality->curiosity * 0.5f), 0.5f, 0.0f});
    policies.push_back({"MISSION", 0.6f + (m_personality->curiosity * 0.3f), 0.2f, 0.0f});

    if (BotManager::getSingletonPtr()->IsAggroEnabled()) {
        policies.push_back({"ATTACK", 0.0f, 0.0f, 0.0f});
        policies.push_back({"USE_ABILITY", 0.0f, 0.0f, 0.0f});
        
        if (m_targetGoId == 0) {
            policies.push_back({"FIND_TARGET", 0.6f + (m_personality->aggressiveness * 0.4f), 0.2f, 0.0f});
        } else {
            PlayerObject* target = BotGetPlayer(m_targetGoId);
            if (target && target->isDead()) {
                policies.push_back({"LOOT_TARGET", 0.6f + (m_personality->curiosity * 0.4f), 0.1f, 0.0f});
            } else if (target && !target->isDead()) {
                float dist = float(me->getPosition().Distance(target->getPosition()));
                if (dist > 250.0f) {
                    policies[2].expectedUtility = 0.8f + (m_personality->aggressiveness * 0.2f); // ATTACK
                    float hackerTendency = (m_personality->curiosity * 0.7f) + ((1.0f - m_personality->aggressiveness) * 0.3f);
                    policies.push_back({"USE_HACKER_ABILITY", 0.85f + (hackerTendency * 0.3f), 0.1f, 0.0f}); 
                } else {
                    policies[2].expectedUtility = 0.9f + (m_personality->aggressiveness * 0.2f);
                    policies[3].expectedUtility = (me->getCurrentIS() > 50) ? 1.0f : 0.0f; 
                }
                
                if (healthPct < (0.3f - m_personality->fearlessness * 0.2f)) {
                    policies.push_back({"FLEE", 1.5f, 0.0f, 0.0f}); 
                }
            } else {
                m_targetGoId = 0;
            }
        }
    }

    // Select Policy via Active Inference
    ActionPolicy optimal = m_activeInference.SelectOptimalPolicy(policies, 0.0f, 1.0f, m_somatic);
    uint32 currentTime = getMSTime();

    // 4. Tactical Execution of Selected Policy
    if (optimal.name == "ROAM") {
        RoamAndSwarm(deltaSeconds);
    } else if (optimal.name == "FLEE") {
        ActionFlee flee;
        flee.Tick(this);
    } else if (optimal.name == "FIND_TARGET") {
        ActionFindTarget find;
        find.Tick(this);
    } else if (optimal.name == "ATTACK") {
        if (m_targetGoId != 0) {
            PlayerObject* target = BotGetPlayer(m_targetGoId);
            if (target && !target->isDead()) {
                // Tactical Approach / Interlock
                ActionEngageTarget engage;
                if (engage.Tick(this) == NodeStatus::SUCCESS) {
                    ActionCombatCycle cycle;
                    cycle.Tick(this);
                }
                
                // Update Theory of Mind
                m_tomSolver.UpdateState(std::to_string(m_targetGoId), -0.05f, 0.1f);
            } else {
                m_targetGoId = 0;
            }
        }
    } else if (optimal.name == "USE_ABILITY") {
        if (currentTime >= m_nextActionTime && m_targetGoId != 0) {
            uint16 chosenAbilityId = 0;
            if (auto abSys = me->getAbilitySystem()) {
                const auto& loaded = abSys->getLoadedAbilities();
                std::vector<uint16> candidateAbilities;
                for (const auto& [id, ab] : loaded) {
                    const AbilityTemplate* tmpl = sDataLoader.GetAbilityTemplate(id);
                    if (tmpl && tmpl->isCastable && me->getCurrentIS() >= tmpl->innerStrengthCost) {
                        candidateAbilities.push_back(id);
                    }
                }
                if (!candidateAbilities.empty()) {
                    chosenAbilityId = candidateAbilities[rand() % candidateAbilities.size()];
                }
            }
            if (chosenAbilityId == 0) {
                // Fallback: match discipline templates from DataLoader
                DisciplineType botDisc = DisciplineType::NONE;
                if (m_faction == FACTION_MEROVINGIAN) botDisc = DisciplineType::CODER;
                else if (m_faction == FACTION_ZION) botDisc = DisciplineType::HACKER;
                else if (m_faction == FACTION_MACHINES) botDisc = DisciplineType::OPERATIVE;

                std::vector<uint16> discAbilities;
                for (const auto& [id, tmpl] : sDataLoader.GetAllAbilities()) {
                    if (tmpl.isCastable && (botDisc == DisciplineType::NONE || tmpl.discipline == botDisc) && me->getCurrentIS() >= tmpl.innerStrengthCost) {
                        discAbilities.push_back(id);
                    }
                }
                if (!discAbilities.empty()) {
                    chosenAbilityId = discAbilities[rand() % discAbilities.size()];
                }
            }

            if (chosenAbilityId != 0) {
                sCombatSys.UseAbility(me, chosenAbilityId, m_targetGoId);
                m_nextActionTime = currentTime + 3000;
            }
        }
    } else if (optimal.name == "USE_HACKER_ABILITY") {
        if (currentTime >= m_nextActionTime && m_targetGoId != 0) {
            uint32 virusId = 1;
            const AbilityTemplate* t = sDataLoader.GetAbilityTemplate((uint16)virusId);
            sHackerSystem.ExecuteHackerAbility(me, (uint16)virusId, m_targetGoId, t);
            m_nextActionTime = currentTime + 3000;
        }
    } else if (optimal.name == "LOOT_TARGET") {
        if (currentTime >= m_nextActionTime && m_targetGoId != 0) {
            me->restoreIS(25);
            me->awardCombatExperience(500);
            m_targetGoId = 0;
            m_nextActionTime = currentTime + 1000;
        }
    } else if (optimal.name == "MISSION") {
        // Bots "on a mission" still walk the streets so players can see them
        // NPCs do not run player missions; they go about their street life.
        RoamAndSwarm(deltaSeconds);
    }

    // Periodic Theory of Mind cache eviction for inactive/stale targets
    if (rand() % 50 == 0) {
        m_tomSolver.EvictInactiveTargets(60);
    }

    // Talkativeness trigger
    if (m_personality->talkativeness > 0.5f && (rand() % 1000) > 995) {
        if (optimal.name == "ATTACK") {
            if (m_personality->aggressiveness > 0.7f) DEBUG_LOG((format("%1%: You're mine!") % me->getHandle()).str());
            else if (m_personality->vibe == "Heroic") DEBUG_LOG((format("%1%: For Zion!") % me->getHandle()).str());
        } else if (optimal.name == "ROAM") {
            if (m_personality->curiosity > 0.7f) DEBUG_LOG((format("%1%: What's over there...?") % me->getHandle()).str());
            else if (m_personality->vibe == "Paranoid") DEBUG_LOG((format("%1%: Did you hear that?") % me->getHandle()).str());
        }
    }
}

void BotClient::SetLocomotionAnimation(uint8 animId)
{
    if (m_currentAnimState == animId)
        return;

    m_currentAnimState = animId;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me)
        return;

    me->setCurrentAnimation(animId);
    LocationVector loc = me->getPosition();
    uint8 rotByte = loc.getMxoRot();
    // 0x06: Locomotion state update (animation ID + heading rotation) drives client skeleton walk/run blend trees
    sGame.AnnounceStateUpdateNear(loc.x, loc.z, 20000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, animId, rotByte));
}

void BotClient::InitializePatrolCircuit()
{
    if (m_patrolInitialized) return;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me) return;

    LocationVector h = me->getPosition();
    m_homeX = (float)h.x;
    m_homeY = (float)h.y;
    m_homeZ = (float)h.z;
    m_hasHome = true;

    m_patrolCircuit.clear();

    // 1. Check nearby hardlines for authentic city transit patrols (within 15m - 120m)
    const auto& hardlines = BotManager::getSingleton().GetHardlines();
    for (const auto& hl : hardlines)
    {
        float dx = (float)hl.x - m_homeX;
        float dz = (float)hl.z - m_homeZ;
        float dSq = dx * dx + dz * dz;
        if (dSq >= 2250000.0f && dSq <= 144000000.0f) // 15m to 120m
        {
            if (!sSpatialGrid.CheckCollision((float)hl.x, (float)hl.z, 1.0f, m_playerGoId))
            {
                m_patrolCircuit.push_back(BotVector2D((float)hl.x, (float)hl.z));
                if (m_patrolCircuit.size() >= 3)
                    break;
            }
        }
    }

    // 2. Generate street-scale waypoints across intersections and avenues (15m - 25m) along street axes
    // In the Mega-City, streets align with coordinate axes (X and Z). Patrolling along cardinal axes
    // keeps bots on sidewalks and street corridors rather than casting into corner buildings.
    const float cardinalOffsets[][2] = {
        { 2500.0f, 0.0f },
        { 0.0f, 2500.0f },
        { -2500.0f, 0.0f },
        { 0.0f, -2500.0f },
        { 1500.0f, 0.0f },
        { 0.0f, -1500.0f }
    };
    for (const auto& off : cardinalOffsets)
    {
        float wx = m_homeX + off[0];
        float wz = m_homeZ + off[1];
        if (!sSpatialGrid.CheckCollision(wx, wz, 1.0f, m_playerGoId))
        {
            m_patrolCircuit.push_back(BotVector2D(wx, wz));
        }
        if (m_patrolCircuit.size() >= 5)
            break;
    }

    // Always loop back through home origin
    m_patrolCircuit.push_back(BotVector2D(m_homeX, m_homeZ));
    m_currentPatrolIndex = 0;
    m_patrolInitialized = true;
}

void BotClient::RoamAndSwarm(float deltaSeconds)
{
    if (deltaSeconds < 0.0f) deltaSeconds = m_deltaSeconds;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me || me->isDead()) return;

    uint32 nowMs = getMSTime();
    if (!m_patrolInitialized)
    {
        InitializePatrolCircuit();
    }
    if (m_pathWaypoints.empty() || m_currentWaypointIndex >= (int)m_pathWaypoints.size())
    {
        SetLocomotionAnimation(0); // Idle when waiting between waypoints
        if (m_idleUntilMs == 0)
        {
            m_idleUntilMs = nowMs + 2000 + (rand() % 3000);
            return;
        }
        if (nowMs < m_idleUntilMs)
            return;
        m_idleUntilMs = 0;

        if (!m_patrolCircuit.empty())
        {
            m_currentPatrolIndex = (m_currentPatrolIndex + 1) % m_patrolCircuit.size();
            BotVector2D nextWp = m_patrolCircuit[m_currentPatrolIndex];
            MoveTo(nextWp.x, m_homeY, nextWp.z);
        }
        if (m_pathWaypoints.empty())
            return;
    }

    if (m_currentWaypointIndex < (int)m_pathWaypoints.size())
    {
        auto wp = m_pathWaypoints[m_currentWaypointIndex];
        LocationVector loc = me->getPosition();
        
        float dx = wp.first - (float)loc.x;
        float dz = wp.second - (float)loc.z;
        float dist = std::sqrt(dx*dx + dz*dz);
        
        if (dist < 50.0f) {
            m_currentWaypointIndex++;
            if (m_currentWaypointIndex >= (int)m_pathWaypoints.size()) {
                m_pathWaypoints.clear();
                SetLocomotionAnimation(0);
                sGame.AnnounceStateUpdateNear(loc.x, loc.z, 20000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
                sGame.AnnounceStateUpdateNear(loc.x, loc.z, 20000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, 0, loc.getMxoRot()));
            }
        } else {
            dist = std::max(0.01f, dist); // Prevent division by zero

            // Authentic walking stride velocity: 280.0f units/s = 2.8 m/s
            float speed = 280.0f;
            BotVector2D pathVel((dx / dist) * speed, (dz / dist) * speed);

            // Add Boids velocity
            BotVector2D swarmVel = CalculateBoidsVelocity(me);
            
            float dilation = me->GetTimeDilation();
            float moveX = (pathVel.x + swarmVel.x) * dilation * deltaSeconds;
            float moveZ = (pathVel.z + swarmVel.z) * dilation * deltaSeconds;
            float newX = (float)loc.x + moveX;
            float newZ = (float)loc.z + moveZ;
            
            if (!sSpatialGrid.CheckCollision(newX, newZ, 1.0f, m_playerGoId)) {
                loc.x = newX;
                loc.z = newZ;

                // Compute orientation/heading angle facing direction of movement (+sin(rot), +cos(rot))
                float heading = (moveX * moveX + moveZ * moveZ > 1e-4f)
                    ? std::atan2(moveX, moveZ)
                    : std::atan2(dx, dz);
                loc.rot = heading;

                me->setPosition(loc);
                sSpatialGrid.UpdateClientPosition(this, newX, newZ);
                SetLocomotionAnimation(3); // 3 = WalkF (after updating loc.rot)
                m_stuckTicks = 0;

                uint32 nowMs = getMSTime();
                if (nowMs - m_lastRoamBroadcastMs >= 150) {
                    m_lastRoamBroadcastMs = nowMs;
                    // PositionStateMsg + LocomotionStateMsg keep client position, walk cycle, and heading synced
                    sGame.AnnounceStateUpdateNear(loc.x, loc.z, 15000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
                    sGame.AnnounceStateUpdateNear(loc.x, loc.z, 15000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, 3, loc.getMxoRot()));
                }
            } else {
                // Someone is standing on the next step. Sidestep instead of abandoning the walk.
                float sx = -dz / dist;
                float sz = dx / dist;
                bool moved = false;
                for (float sign : { 1.0f, -1.0f })
                {
                    float ax = (float)loc.x + sx * 80.0f * sign;
                    float az = (float)loc.z + sz * 80.0f * sign;
                    if (sSpatialGrid.CheckCollision(ax, az, 1.0f, m_playerGoId))
                        continue;
                    loc.x = ax;
                    loc.z = az;
                    loc.rot = std::atan2(dx, dz);
                    me->setPosition(loc);
                    sSpatialGrid.UpdateClientPosition(this, ax, az);
                    SetLocomotionAnimation(3);
                    m_stuckTicks = 0;

                    uint32 nowMs = getMSTime();
                    if (nowMs - m_lastRoamBroadcastMs >= 150) {
                        m_lastRoamBroadcastMs = nowMs;
                        sGame.AnnounceStateUpdateNear(loc.x, loc.z, 15000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
                        sGame.AnnounceStateUpdateNear(loc.x, loc.z, 15000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, 3, loc.getMxoRot()));
                    }
                    moved = true;
                    break;
                }
                if (!moved)
                {
                    m_stuckTicks++;
                    if (m_stuckTicks >= 3)
                    {
                        // Wall or obstacle collision turnaround: turn 180 degrees back to home origin or previous point
                        m_stuckTicks = 0;
                        m_pathWaypoints.clear();
                        SetLocomotionAnimation(0);
                        uint32 nowMs = getMSTime();
                        m_idleUntilMs = nowMs + 1000 + (rand() % 1500); // pause 1-2.5s before redirecting
                        if (!m_patrolCircuit.empty())
                        {
                            m_currentPatrolIndex = (m_currentPatrolIndex + 1) % m_patrolCircuit.size();
                        }
                    }
                    else
                    {
                        m_currentWaypointIndex++;
                    }
                }
            }
        }
    }
}

void BotClient::AttackTarget(uint32 targetGoId)
{
    m_targetGoId = targetGoId;
    if (m_playerGoId == 0)
        return;

    PlayerObject* me = BotGetPlayer(m_playerGoId);
    PlayerObject* target = BotGetPlayer(targetGoId);
    if (!me || !target || target->isDead())
    {
        SetLocomotionAnimation(0);
        return;
    }

    //engage through the combat system so a real session exists:
    //interlock in melee range, otherwise open fire
    float dist = float(me->getPosition().Distance(target->getPosition()));
    const CombatMove* defaultMelee = CombatSystem::DefaultMelee();
    float meleeRange = defaultMelee ? defaultMelee->range : 250.0f;
    
    // The dojo range target is a gunfight drill: it stands its ground and returns fire, it never
    // closes in or pulls the shooter into an interlock (live 15:43:58, it interlocked after 3 s).
    if (me->getHandle().find("Range Target") != std::string::npos) {
        SetLocomotionAnimation(0);
        if (!sCombatSys.IsFreeFiring(m_playerGoId))
            sCombatSys.RequestRangedCombat(m_playerGoId, targetGoId, 0);
        return;
    }

    if (dist <= meleeRange) {
        SetLocomotionAnimation(0);
        if (!sCombatSys.RequestInterlock(m_playerGoId, targetGoId))
        {
            // Refused (usually: the target is already interlocked with someone else). Standing
            // next to it retrying every 2 s is what made street bots look frozen - give up.
            m_targetGoId = 0;
            m_nextActionTime = getMSTime() + 4000 + (rand() % 4000);
        }
    }
    else
    {
        // Try ranged combat. If it fails (e.g. out of ammo or LOS blocked), move closer
        sCombatSys.RequestRangedCombat(m_playerGoId, targetGoId, 0);
        
        float dirX = target->getPosition().x - me->getPosition().x;
        float dirZ = target->getPosition().z - me->getPosition().z;
        float lenSq = dirX*dirX + dirZ*dirZ;
        if (lenSq > 0.0001f) {
            float len = sqrt(lenSq);
            dirX /= len;
            dirZ /= len;
        } else {
            dirX = 0; dirZ = 0;
        }
        
        // Move towards target scaled by deltaSeconds for uniform movement across LOD tiers
        float speed = 450.0f; // 4.5 m/s combat approach speed
        float dt = (m_deltaSeconds > 0.0001f) ? m_deltaSeconds : 0.033f;
        float newX = me->getPosition().x + dirX * speed * dt;
        float newZ = me->getPosition().z + dirZ * speed * dt;
        if (!sSpatialGrid.CheckCollision(newX, newZ, 1.0f, m_playerGoId)) {
            LocationVector newPos(newX, me->getPosition().y, newZ);
            newPos.rot = std::atan2(dirX, dirZ);
            uint8 newRot = newPos.getMxoRot();
            me->setPosition(newPos);
            sSpatialGrid.UpdateClientPosition(this, newX, newZ);
            // Approach target with combat sprint animation (after updating rotation)
            SetLocomotionAnimation(4); // 4 = DetectDiff_RunF

            uint32 nowMs = getMSTime();
            if (nowMs - m_lastRoamBroadcastMs >= 150) {
                m_lastRoamBroadcastMs = nowMs;
                sGame.AnnounceStateUpdateNear(newX, newZ, 20000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
                // Broadcast 0x06 locomotion animation (4 = DetectDiff_RunF combat sprint) + heading rotation
                sGame.AnnounceStateUpdateNear(newX, newZ, 20000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, 4, newRot));
            }
        }
    }
}

void BotClient::MakeTester(int index)
{
    m_tester = std::make_shared<TesterBrain>(this, index);
}

bool BotClient::StepAlongPath(float deltaSeconds, float speed, uint8 locomotionAnim)
{
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me || me->isDead()) return false;
    if (!HasPath()) return true;

    auto wp = m_pathWaypoints[m_currentWaypointIndex];
    LocationVector loc = me->getPosition();
    float dx = wp.first - (float)loc.x;
    float dz = wp.second - (float)loc.z;
    float dist = std::sqrt(dx*dx + dz*dz);
    float step = speed * std::max(0.01f, deltaSeconds);

    if (dist > 0.01f)
    {
        loc.rot = std::atan2(dx, dz);
    }

    if (dist <= std::max(50.0f, step))
    {
        loc.x = wp.first;
        loc.z = wp.second;
        m_currentWaypointIndex++;
    }
    else
    {
        loc.x += (dx / dist) * step;
        loc.z += (dz / dist) * step;
    }
    me->setPosition(loc);
    sSpatialGrid.UpdateClientPosition(this, (float)loc.x, (float)loc.z);

    bool arrived = !HasPath();
    uint8 anim = arrived ? 0 : locomotionAnim;
    SetLocomotionAnimation(anim);
    uint32 nowMs = getMSTime();
    if (arrived || nowMs - m_lastRoamBroadcastMs >= 150)
    {
        m_lastRoamBroadcastMs = nowMs;
        sGame.AnnounceStateUpdateNear(loc.x, loc.z, 15000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
        sGame.AnnounceStateUpdateNear(loc.x, loc.z, 15000.0f, std::make_shared<LocomotionStateMsg>(m_playerGoId, anim, loc.getMxoRot()));
    }
    if (arrived)
    {
        m_pathWaypoints.clear();
        m_currentWaypointIndex = 0;
    }
    return arrived;
}

void BotClient::MoveTo(float x, float y, float z)
{
    PlayerObject* po = BotGetPlayer(m_playerGoId);
    if (!po) return;

    // Item 4: Collision Physics - Prevent overlapping coordinates
    if (sSpatialGrid.CheckCollision(x, z, 1.0f, m_playerGoId))
    {
        // Try to offset slightly (Boids separation handles ongoing adjustments)
        x += (rand() % 100 / 100.0f) - 0.5f;
        z += (rand() % 100 / 100.0f) - 0.5f;
        
        // Check again, if still colliding, just reject the move to prevent stacking
        if (sSpatialGrid.CheckCollision(x, z, 0.5f, m_playerGoId)) {
            return;
        }
    }
    // Spawn anchor (m_spawnX/Y/Z) is fixed and must not be mutated by intermediate pathfinding.

    if (m_playerGoId != 0)
    {
        PlayerObject* me = BotGetPlayer(m_playerGoId);
        if (me)
        {
            LocationVector loc = me->getPosition();
            
            // Item 10: Ghost AI / The Twins bypass NavMesh collision
            bool ignoreCollision = (me->getHandle() == "The_Twins");
            
            // Check 3D vertical elevation (stairs, fire escapes, rooftop ladders)
            if (std::abs(loc.y - y) > 150.0f) {
                auto pts3D = NavMeshMgr::getSingletonPtr()->FindPath3D(loc.x, loc.y, loc.z, x, y, z, ignoreCollision);
                m_pathWaypoints.clear();
                for (const auto& p : pts3D) {
                    m_pathWaypoints.push_back({p.x, p.z});
                }
            } else {
                m_pathWaypoints = NavMeshMgr::getSingletonPtr()->FindPath(loc.x, loc.z, x, z, ignoreCollision);
            }
            m_currentWaypointIndex = 0;
            
            if (m_pathWaypoints.empty())
            {
                // Direct stride waypoint to prevent instantaneous teleport jumps
                m_pathWaypoints.push_back({x, z});
                m_currentWaypointIndex = 0;
            }
        }
    }
}

void BotClient::Say(const std::string& msg)
{
    if (m_playerGoId == 0) return;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me) return;

    DEBUG_LOG(format("%1% (Bot) says %2%") % me->getHandle() % msg);
}


void BotClient::Emote(uint32 emoteId)
{
    // Bots never emote on the wire (GEMINI.md Rule 6.1)
    (void)emoteId;
    return;
}

BotVector2D BotClient::CalculateBoidsVelocity(PlayerObject* me)
{
    BotVector2D separation(0, 0);
    BotVector2D alignment(0, 0);
    BotVector2D cohesion(0, 0);
    int flockCount = 0;

    LocationVector myLoc = me->getPosition();
    
    // O(1) neighboring cast
    auto neighbors = sSpatialGrid.GetClientsNearClient(this);
    std::vector<RVOAgentState> rvoNeighbors;
    rvoNeighbors.reserve(neighbors.size());
    
    for (GameClient* client : neighbors)
    {
        if (client == this) continue;
        
        // We only flock with other bots for now
        BotClient* otherBot = dynamic_cast<BotClient*>(client);
        if (!otherBot) continue;

        PlayerObject* otherPo = BotGetPlayer(otherBot->m_playerGoId);
        if (!otherPo || otherPo->isDead()) continue;

        LocationVector otherLoc = otherPo->getPosition();
        float dx = myLoc.x - otherLoc.x;
        float dz = myLoc.z - otherLoc.z;
        float distSq = dx*dx + dz*dz;

        // Build RVO neighbor state
        RVOAgentState rvoOther;
        rvoOther.goId = otherBot->m_playerGoId;
        rvoOther.position = RVOVector2D(otherLoc.x, otherLoc.z);
        rvoOther.velocity = RVOVector2D(-dx * 0.05f, -dz * 0.05f);
        rvoOther.prefVelocity = rvoOther.velocity;
        rvoOther.radius = 50.0f; // 50 cm
        rvoOther.maxSpeed = 500.0f;
        rvoNeighbors.push_back(rvoOther);

        if (otherBot->GetFaction() != this->GetFaction()) continue; // Only flock with allies

        // Separation (< 2.0m = 200 units, or scaled)
        if (distSq > 0.0001f && distSq < 40000.0f)
        {
            float dist = std::max(std::sqrt(distSq), 10.0f);
            separation.x += (dx / dist) / dist; // scale by inverse distance
            separation.z += (dz / dist) / dist;
        }

        // Alignment & Cohesion (< 10.0m = 1000 units)
        if (distSq < 1000000.0f)
        {
            // Cohesion (Add positions, average later)
            cohesion.x += otherLoc.x;
            cohesion.z += otherLoc.z;
            flockCount++;
        }
    }

    if (flockCount > 0)
    {
        cohesion.x = (cohesion.x / flockCount) - myLoc.x;
        cohesion.z = (cohesion.z / flockCount) - myLoc.z;
        
        // Normalize cohesion
        float cLen = std::sqrt(cohesion.x*cohesion.x + cohesion.z*cohesion.z);
        if (cLen > 0)
        {
            cohesion.x /= cLen;
            cohesion.z /= cLen;
        }
    }

    // Weights: Prioritize separation so they don't overlap
    BotVector2D totalVel;
    totalVel.x = (separation.x * 2.5f) + (cohesion.x * 0.5f);
    totalVel.z = (separation.z * 2.5f) + (cohesion.z * 0.5f);

    // RVO2 Reciprocal Velocity Obstacle collision avoidance refinement
    if (!rvoNeighbors.empty()) {
        RVOAgentState meState;
        meState.goId = m_playerGoId;
        meState.position = RVOVector2D(myLoc.x, myLoc.z);
        meState.velocity = RVOVector2D(totalVel.x, totalVel.z);
        meState.prefVelocity = RVOVector2D(totalVel.x, totalVel.z);
        meState.radius = 50.0f;
        meState.maxSpeed = 500.0f;

        RVOVector2D rvoVel = RVO2Solver::ComputeRVOVelocity(meState, rvoNeighbors);
        totalVel.x = rvoVel.x;
        totalVel.z = rvoVel.z;
    }

    // Clamp max swarm velocity
    float tLenSq = totalVel.x*totalVel.x + totalVel.z*totalVel.z;
    if (tLenSq > 25.0f) {
        float tLen = std::sqrt(tLenSq);
        totalVel.x = (totalVel.x / tLen) * 5.0f;
        totalVel.z = (totalVel.z / tLen) * 5.0f;
    }

    return totalVel;
}

bool BotClient::IsPanicking() const { return m_isPanicking || m_fearLevel >= 0.80f; }
void BotClient::SetPanicking(bool panic) {
    m_isPanicking = panic;
    if (panic && m_fearLevel < 0.80f) m_fearLevel = 0.85f;
    else if (!panic && m_fearLevel >= 0.80f) m_fearLevel = 0.50f;
}
float BotClient::GetFearLevel() const { return m_fearLevel; }
void BotClient::SetFearLevel(float f) {
    m_fearLevel = std::clamp(f, 0.0f, 1.0f);
    m_isPanicking = (m_fearLevel >= 0.80f);
}
void BotClient::AddFear(float amount) { SetFearLevel(m_fearLevel + amount); }
uint8 BotClient::GetCivilianTier() const {
    if (m_fearLevel >= 0.80f) return 3;
    if (m_fearLevel >= 0.55f) return 2;
    if (m_fearLevel >= 0.25f) return 1;
    return 0;
}
uint32 BotClient::GetLastDistressCallTime() const { return m_lastDistressCallTime; }
void BotClient::SetLastDistressCallTime(uint32 t) { m_lastDistressCallTime = t; }
uint32 BotClient::GetLastLookAroundTime() const { return m_lastLookAroundTime; }
void BotClient::SetLastLookAroundTime(uint32 t) { m_lastLookAroundTime = t; }
uint32 BotClient::GetLastWhisperTime() const { return m_lastWhisperTime; }
void BotClient::SetLastWhisperTime(uint32 t) { m_lastWhisperTime = t; }

void BotClient::triggerPanic(uint32 sourceGoId)
{
    if (m_isPanicking) return;

    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me) return;

    // OCEAN Neuroticism threshold: Low N bots are much more resistant to panic
    float panicResistChance = (1.0f - m_personality->neuroticism) * 0.45f;
    if ((rand() % 100) / 100.0f < panicResistChance) {
        // High stoicism / low neuroticism bot resists panic!
        return;
    }

    m_fearLevel = 1.0f;
    m_isPanicking = true;
    m_targetGoId = 0; // Drop targets
    
    if (m_personality->agreeableness > 0.6f) {
        Say("Look out! Trouble! Everyone get to the subway!");
    } else if (m_personality->neuroticism > 0.6f) {
        Say("No, please! Someone help! They're killing people!");
    } else {
        Say("Help! Someone call the police!");
    }

    // Randomly cower before running (scaled by Neuroticism)
    if ((rand() % 100) < static_cast<int>(m_personality->neuroticism * 80.0f + 20.0f)) {
        Emote(50); // Cower emote ID
    }
}

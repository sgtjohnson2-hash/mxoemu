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
#include <cmath>
#include <fstream>
#include <typeinfo>

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
        // sObjMgr.getGOPtr(m_playerGoId)->PopulateWorld();
        sObjMgr.getGOPtr(m_playerGoId)->SpawnSelf();
        
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

    if (me->isDead())
    {
        m_targetGoId = 0; //respawn fires through the normal EVENT_RESPAWN path
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
    if (me->getFactionName() == "Civilian") {
        sPedestrianEcology.UpdateCivilian(this, deltaSeconds);
        return;
    }

    float healthPct = float(me->getCurrentHealth()) / float(me->getMaximumHealth());

    // 2. Agent Viral Assimilation Trigger
    if (m_isAgent || (m_faction == FACTION_MACHINES && healthPct < 0.5f)) {
        ActionAgentInfect infect;
        if (infect.Tick(this) == NodeStatus::SUCCESS) {
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
                
                // Update Theory of Mind & Combat Memory
                m_tomSolver.UpdateState(std::to_string(m_targetGoId), -0.05f, 0.1f);
                if (rand() % 10 == 0) {
                    MemoryNode mem;
                    mem.text = (format("In combat with %1%") % target->getHandle()).str();
                    mem.importance = 0.7f;
                    mem.timestamp = double(currentTime);
                    m_memoryStream.AddMemory(mem);
                }
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
            sHackerSystem.CompileProgram(me, virusId, m_targetGoId);
            sHackerSystem.ExecuteProgram(me, virusId, m_targetGoId);
            m_nextActionTime = currentTime + 3000;
        }
    } else if (optimal.name == "LOOT_TARGET") {
        if (currentTime >= m_nextActionTime && m_targetGoId != 0) {
            sHackerSystem.ExtractSourceCode(me, m_targetGoId);
            me->awardCombatExperience(500);
            m_targetGoId = 0;
            m_nextActionTime = currentTime + 1000;
        }
    } else if (optimal.name == "MISSION") {
        if (currentTime >= m_nextActionTime) {
            uint32 randomMissionId = 1 + (rand() % 3);
            sMissionSys.AssignMission(me, randomMissionId);
            sMissionSys.AdvanceObjective(me, ObjectiveCommand::TALK, 0);
            m_nextActionTime = currentTime + 10000;
        }
    }

    // Talkativeness trigger
    if (m_personality->talkativeness > 0.5f && (rand() % 1000) > 995) {
        if (optimal.name == "ATTACK") {
            if (m_personality->aggressiveness > 0.7f) sGame.AnnounceCommand(this, std::make_shared<PlayerChatMsg>(me->getHandle(), "You're mine!"));
            else if (m_personality->vibe == "Heroic") sGame.AnnounceCommand(this, std::make_shared<PlayerChatMsg>(me->getHandle(), "For Zion!"));
        } else if (optimal.name == "ROAM") {
            if (m_personality->curiosity > 0.7f) sGame.AnnounceCommand(this, std::make_shared<PlayerChatMsg>(me->getHandle(), "What's over there...?"));
            else if (m_personality->vibe == "Paranoid") sGame.AnnounceCommand(this, std::make_shared<PlayerChatMsg>(me->getHandle(), "Did you hear that?"));
        }
    }
}

void BotClient::RoamAndSwarm(float deltaSeconds)
{
    if (deltaSeconds < 0.0f) deltaSeconds = m_deltaSeconds;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me || me->isDead()) return;

    if (m_pathWaypoints.empty() || m_currentWaypointIndex >= m_pathWaypoints.size())
    {
        // Pick a random location within 100m
        LocationVector loc = me->getPosition();
        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float distance = 10.0f + static_cast<float>(rand() % 90);
        
        MoveTo(loc.x + std::cos(angle) * distance, loc.y, loc.z + std::sin(angle) * distance);
    }

    if (m_currentWaypointIndex < m_pathWaypoints.size())
    {
        auto wp = m_pathWaypoints[m_currentWaypointIndex];
        LocationVector loc = me->getPosition();
        
        float dx = wp.first - loc.x;
        float dz = wp.second - loc.z;
        float dist = std::sqrt(dx*dx + dz*dz);
        
        if (dist < 1.0f) {
            m_currentWaypointIndex++;
        } else {
            dist = std::max(0.01f, dist); // Prevent division by zero

            // Base Pathfinding velocity
            float speed = 5.0f; // 5m/s base speed
            BotVector2D pathVel((dx / dist) * speed, (dz / dist) * speed);

            // Add Boids velocity
            BotVector2D swarmVel = CalculateBoidsVelocity(me);
            
            // Blend them (weighting can be tuned)
            float dilation = me->GetTimeDilation();
            float newX = loc.x + (pathVel.x + swarmVel.x) * dilation * deltaSeconds;
            float newZ = loc.z + (pathVel.z + swarmVel.z) * dilation * deltaSeconds;
            
            if (!sSpatialGrid.CheckCollision(newX, newZ, 1.0f, m_playerGoId)) {
                loc.x = newX;
                loc.z = newZ;
                me->setPosition(loc);
                sGame.AnnounceStateUpdateNear(loc.x, loc.z, 20000.0f, std::make_shared<PositionStateMsg>(m_playerGoId));
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
        return;

    //engage through the combat system so a real session exists:
    //interlock in melee range, otherwise open fire
    float dist = float(me->getPosition().Distance(target->getPosition()));
    const CombatMove* defaultMelee = CombatSystem::DefaultMelee();
    float meleeRange = defaultMelee ? defaultMelee->range : 250.0f;
    
    if (dist <= meleeRange) {
        sCombatSys.RequestInterlock(m_playerGoId, targetGoId);
    }
    else
    {
        // Try ranged combat. If it fails (e.g. out of ammo or LOS blocked), move closer
        sCombatSys.RequestRangedCombat(m_playerGoId, targetGoId, 0);
        
        // Always try to move towards target during ranged combat anyway to prevent getting stuck
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
        float speed = 60.0f; 
        float dt = (m_deltaSeconds > 0.0001f) ? m_deltaSeconds : 0.033f;
        float newX = me->getPosition().x + dirX * speed * dt;
        float newZ = me->getPosition().z + dirZ * speed * dt;
        if (!sSpatialGrid.CheckCollision(newX, newZ, 1.0f, m_playerGoId)) {
            me->setPosition(LocationVector(newX, me->getPosition().y, newZ));
        }
    }
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
    m_spawnX = x;
    m_spawnY = y;
    m_spawnZ = z;

    if (m_playerGoId != 0)
    {
        PlayerObject* me = BotGetPlayer(m_playerGoId);
        if (me)
        {
            LocationVector loc = me->getPosition();
            
            // Item 10: Ghost AI / The Twins bypass NavMesh collision
            bool ignoreCollision = (me->getHandle() == "The_Twins");
            
            m_pathWaypoints = NavMeshMgr::getSingletonPtr()->FindPath(loc.x, loc.z, x, z, ignoreCollision);
            m_currentWaypointIndex = 0;
            
            if (m_pathWaypoints.empty())
            {
                // Fallback to direct teleport if no path found
                loc.x = x;
                loc.y = y;
                loc.z = z;
                me->setPosition(loc);
                sGame.AnnounceStateUpdate(this, std::make_shared<PositionStateMsg>(m_playerGoId));
            }
        }
    }
}

void BotClient::Say(const std::string& msg)
{
    if (m_playerGoId == 0) return;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me) return;

    INFO_LOG(format("%1% (Bot) says %2%") % me->getHandle() % msg);
    sGame.AnnounceCommand(this, shared_ptr<PlayerChatMsg>(new PlayerChatMsg(me->getHandle(), msg)));
}


void BotClient::Emote(uint32 emoteId)
{
    if (m_playerGoId == 0) return;
    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me) return;

    // Emote counter doesn't matter too much for bots, just pass 1
    sGame.AnnounceStateUpdateNear(me->getPosition().x, me->getPosition().z, 20000.0f, shared_ptr<EmoteMsg>(new EmoteMsg(m_playerGoId, emoteId, 1)));
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
    
    for (GameClient* client : neighbors)
    {
        if (client == this) continue;
        
        // We only flock with other bots for now
        BotClient* otherBot = dynamic_cast<BotClient*>(client);
        if (!otherBot) continue;
        
        if (otherBot->GetFaction() != this->GetFaction()) continue; // Only flock with allies

        PlayerObject* otherPo = BotGetPlayer(otherBot->m_playerGoId);
        if (!otherPo || otherPo->isDead()) continue;

        LocationVector otherLoc = otherPo->getPosition();
        float dx = myLoc.x - otherLoc.x;
        float dz = myLoc.z - otherLoc.z;
        float distSq = dx*dx + dz*dz;

        // Separation (< 2.0m)
        if (distSq > 0.0001f && distSq < 4.0f)
        {
            float dist = std::sqrt(distSq);
            separation.x += (dx / dist) / dist; // scale by inverse distance
            separation.z += (dz / dist) / dist;
        }

        // Alignment & Cohesion (< 10.0m)
        if (distSq < 100.0f)
        {
            // Cohesion (Add positions, average later)
            cohesion.x += otherLoc.x;
            cohesion.z += otherLoc.z;
            flockCount++;
            
            // Note: Alignment would average velocities, but Bots in MXO don't have a rigid velocity vector, 
            // they teleport incrementally. We skip strict alignment to save CPU and focus on Cohesion/Separation.
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

    // Clamp max swarm velocity
    float tLenSq = totalVel.x*totalVel.x + totalVel.z*totalVel.z;
    if (tLenSq > 25.0f) {
        float tLen = std::sqrt(tLenSq);
        totalVel.x = (totalVel.x / tLen) * 5.0f;
        totalVel.z = (totalVel.z / tLen) * 5.0f;
    }

    return totalVel;
}

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

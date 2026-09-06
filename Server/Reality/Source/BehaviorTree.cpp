#include "Common.h"
#include "BehaviorTree.h"
#include "BotClient.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "Timer.h"
#include "Log.h"
#include "GameServer.h"
#include "CombatSystem.h"
#include "DataLoader.h"
#include "SpatialGrid.h"
#include "StatusEffectManager.h"
#include "MessageTypes.h"
#include "AI/SensoryPerceptionSystem.h"
#include "AI/SentientMajorCharacters.h"
#include <cmath>

NodeStatus SequenceNode::Tick(BotClient* bot)
{
    for (auto& child : m_children)
    {
        NodeStatus status = child->Tick(bot);
        if (status != NodeStatus::SUCCESS)
        {
            return status;
        }
    }
    return NodeStatus::SUCCESS;
}

NodeStatus SelectorNode::Tick(BotClient* bot)
{
    for (auto& child : m_children)
    {
        NodeStatus status = child->Tick(bot);
        if (status != NodeStatus::FAILURE)
        {
            return status;
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionRoam::Tick(BotClient* bot)
{
    // The Swarm Logic (V13 Boids + NavMeshMgr)
    uint32 leaderId = bot->GetCrewLeaderId();
    if (leaderId != 0) {
        PlayerObject* leader = BotGetPlayer(leaderId);
        if (leader && !leader->isDead()) {
            LocationVector lPos = leader->getPosition();
            bot->MoveTo(lPos.x, lPos.y, lPos.z);
            return NodeStatus::SUCCESS;
        } else {
            bot->SetCrewLeaderId(0); // Leader died or despawned
        }
    }

    bot->RoamAndSwarm();
    return NodeStatus::SUCCESS;
}

NodeStatus ActionSmuggle::Tick(BotClient* bot)
{
    if (bot->GetFaction() != FACTION_MEROVINGIAN) return NodeStatus::FAILURE;

    // Smuggling logic: Find a random hardline, but explicitly try to avoid nodes with active players
    if ((rand() % 100) < 5) {
        LocationVector targetHl = BotManager::getSingletonPtr()->GetRandomHardline();
        bot->MoveTo(targetHl.x, targetHl.y, targetHl.z);
        bot->Say("Moving the package. Keep eyes peeled.");
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionGossip::Tick(BotClient* bot)
{
    // 1% chance per tick to say something while idle
    if (bot->GetTargetGoId() == 0 && (rand() % 1000) < 10) {
        const char* rumors[] = {
            "Did you hear what happened in Morrell?",
            "Machines are amassing near the hardlines.",
            "I saw an Exile trading data fragments.",
            "Agents are looking for someone.",
            "The truce won't last forever."
        };
        bot->Say(rumors[rand() % 5]);
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionSniperRoam::Tick(BotClient* bot)
{
    // Look for high Y-coordinate NavMesh nodes if ranged
    // For now, simulated by moving to a random offset with high Y
    if ((rand() % 100) < 5) {
        PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
        if (me) {
            LocationVector p = me->getPosition();
            bot->MoveTo(p.x + (rand()%2000 - 1000), p.y + 1500.0f, p.z + (rand()%2000 - 1000));
        }
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionFindTarget::Tick(BotClient* bot)
{
    if (!BotManager::getSingletonPtr()->IsAggroEnabled()) return NodeStatus::FAILURE;

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;
    if (me->getFactionName() == "Civilian") return NodeStatus::FAILURE;

    std::vector<GameClient*> localClients = sSpatialGrid.GetClientsInRadius(me->getPosition().x, me->getPosition().z);
    for (GameClient* client : localClients)
    {
        if (client->GetPlayerGoId() == bot->GetPlayerGoId()) continue;
        
        PlayerObject* potentialTarget = BotGetPlayer(client->GetPlayerGoId());
        if (potentialTarget && !potentialTarget->isDead())
        {
            if (sStatusEffectManager.HasEffect(potentialTarget->getGoId(), EFFECT_FACTION_MASK)) continue; // Item 25: Simulacra Masking
            
            std::string fName = potentialTarget->getFactionName();
            if (fName == "Civilian") continue; // Never ambiently target neutral civilians!

            // Enforce mxoFaction checks so we don't attack our own
            mxoFaction targetFaction = FACTION_ZION;
            if (fName == "Machines") targetFaction = FACTION_MACHINES;
            else if (fName == "Merovingian") targetFaction = FACTION_MEROVINGIAN;

            if (targetFaction == bot->GetFaction()) continue; // Skip same faction

            // Sensory perception: dual-cone vision + acoustic awareness check
            LocationVector myPos = me->getPosition();
            LocationVector targetPos = potentialTarget->getPosition();
            float dist = sqrt(pow(myPos.x - targetPos.x, 2) + pow(myPos.y - targetPos.y, 2) + pow(myPos.z - targetPos.z, 2));
            
            float visionConfidence = 0.0f;
            bool canSee = sSensoryPerception.CheckVision(me, potentialTarget, false, visionConfidence);
            BotAwarenessState awareness = sSensoryPerception.GetAwareness(bot->GetPlayerGoId());

            if (canSee || (dist < 1500.0f) || (awareness.stage >= AWARENESS_ALERTED && awareness.alertSourceGoId == client->GetPlayerGoId()))
            {
                sSensoryPerception.SetAwareness(bot->GetPlayerGoId(), AWARENESS_IN_COMBAT, client->GetPlayerGoId());
                bot->SetTargetGoId(client->GetPlayerGoId());
                
                // Map BotPersonality to Local Chat Output
                if ((rand() % 100) / 100.0f < bot->GetPersonality().talkativeness) {
                    std::string shout = "Target locked!";
                    mxoFaction myFaction = bot->GetFaction();
                    if (myFaction == FACTION_ZION) {
                        const char* zionShouts[] = {"Watch your six, redpills!", "Taking them down!", "For Zion!"};
                        shout = zionShouts[rand() % 3];
                    } else if (myFaction == FACTION_MACHINES) {
                        const char* machineShouts[] = {"Anomaly detected.", "Executing deletion protocol.", "Surrender your source code."};
                        shout = machineShouts[rand() % 3];
                    } else if (myFaction == FACTION_MEROVINGIAN) {
                        const char* meroShouts[] = {"Ah, another pawn to play with.", "The Frenchman sends his regards.", "You cannot fight causality."};
                        shout = meroShouts[rand() % 3];
                    }
                    bot->Say(shout);
                }
                bot->Emote(0); // Emote before combat
                return NodeStatus::SUCCESS;
            }
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionHealAlly::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    bool hasHeal = false;
    uint16 healAbilityId = 0;
    if (auto abSys = me->getAbilitySystem()) {
        for (auto const& [id, ab] : abSys->getLoadedAbilities()) {
            auto templ = sDataLoader.GetAbilityTemplate(id);
            if (templ && templ->isBuff && (templ->name.find("Heal") != std::string::npos || templ->name.find("Restore") != std::string::npos)) {
                hasHeal = true;
                healAbilityId = id;
                break;
            }
        }
    }

    if (!hasHeal) return NodeStatus::FAILURE;

    std::vector<GameClient*> localClients = sSpatialGrid.GetClientsInRadius(me->getPosition().x, me->getPosition().z);
    for (GameClient* client : localClients)
    {
        if (client->GetPlayerGoId() == bot->GetPlayerGoId()) continue;
        
        PlayerObject* potentialTarget = BotGetPlayer(client->GetPlayerGoId());
        if (potentialTarget && !potentialTarget->isDead())
        {
            mxoFaction targetFaction = FACTION_ZION;
            std::string fName = potentialTarget->getFactionName();
            if (fName == "Machines") targetFaction = FACTION_MACHINES;
            else if (fName == "Merovingian") targetFaction = FACTION_MEROVINGIAN;

            if (targetFaction != bot->GetFaction()) continue;

            float hpPct = float(potentialTarget->getCurrentHealth()) / float(potentialTarget->getMaximumHealth());
            if (hpPct < 0.5f) {
                bot->SetTargetGoId(potentialTarget->getGoId());
                if ((rand() % 100) / 100.0f < bot->GetPersonality().talkativeness) {
                    bot->Say("Hold on, I'm patching you up!");
                }
                
                uint32 healAmt = 500;
                potentialTarget->setCurrentHealth(std::min<uint32>(potentialTarget->getMaximumHealth(), potentialTarget->getCurrentHealth() + healAmt));
                INFO_LOG(format("%1% healed %2% for %3%!") % me->getHandle() % potentialTarget->getHandle() % healAmt);
                bot->Emote(50);
                return NodeStatus::SUCCESS;
            }
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionFormCrew::Tick(BotClient* bot)
{
    if (bot->GetCrewId() != 0) return NodeStatus::FAILURE; // Already in a crew

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    std::vector<GameClient*> localClients = sSpatialGrid.GetClientsInRadius(me->getPosition().x, me->getPosition().z);
    for (GameClient* client : localClients)
    {
        if (client->GetPlayerGoId() == bot->GetPlayerGoId()) continue;

        std::shared_ptr<BotClient> otherBot = BotManager::getSingletonPtr()->GetBotByGOID(client->GetPlayerGoId());
        if (!otherBot) continue; // Not a bot

        if (otherBot->GetFaction() != bot->GetFaction()) continue;

        PlayerObject* otherPo = BotGetPlayer(otherBot->GetPlayerGoId());
        if (!otherPo || otherPo->isDead()) continue;

        LocationVector myPos = me->getPosition();
        LocationVector targetPos = otherPo->getPosition();
        float dist = sqrt(pow(myPos.x - targetPos.x, 2) + pow(myPos.y - targetPos.y, 2) + pow(myPos.z - targetPos.z, 2));
        if (dist < 1500.0f) // within 15m
        {
            if (otherBot->GetCrewId() != 0)
            {
                bot->SetCrewId(otherBot->GetCrewId());
                bot->SetCrewLeaderId(otherBot->GetCrewLeaderId()); // Follow the established leader
                bot->Say((format("Teaming up with %1%.") % otherPo->getFirstName()).str());
                return NodeStatus::SUCCESS;
            }
            else
            {
                uint32 newCrewId = BotManager::getSingletonPtr()->GetNextCrewId();
                otherBot->SetCrewId(newCrewId);
                otherBot->SetCrewLeaderId(otherBot->GetPlayerGoId()); // They become the leader
                bot->SetCrewId(newCrewId);
                bot->SetCrewLeaderId(otherBot->GetPlayerGoId()); // I follow them
                bot->Say((format("Teaming up with %1%.") % otherPo->getFirstName()).str());
                return NodeStatus::SUCCESS;
            }
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionAgentInfect::Tick(BotClient* bot)
{
    if (!bot->isAgent()) return NodeStatus::FAILURE;
    
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me || me->isDead()) return NodeStatus::FAILURE;

    // High chance if wounded (< 50% HP), moderate chance ambiently
    float hpPct = float(me->getCurrentHealth()) / float(me->getMaximumHealth());
    int infectChance = (hpPct < 0.5f) ? 50 : 15;
    if ((rand() % 100) >= infectChance) return NodeStatus::FAILURE;

    // Find nearby host (non-agent civilian or resistance bot)
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(me->getPosition().x, me->getPosition().z);
    for (GameClient* client : nearbyClients)
    {
        if (client->GetPlayerGoId() == bot->GetPlayerGoId()) continue;
        if (!client->isBot()) continue;
        
        PlayerObject* target = BotGetPlayer(client->GetPlayerGoId());
        if (!target || target->isDead()) continue;

        std::shared_ptr<BotClient> targetBot = BotManager::getSingletonPtr()->GetBotByGOID(target->getGoId());
        if (targetBot && !targetBot->isAgent())
        {
            LocationVector myPos = me->getPosition();
            LocationVector tPos = target->getPosition();
            float distSq = pow(myPos.x - tPos.x, 2) + pow(myPos.z - tPos.z, 2);
            if (distSq <= 2250000.0f) // 1500 units (15 meters)
            {
                // Cascade into SentientMajorCharacters host hijacking
                sSentientCharacters.HijackNearbyHost(myPos.x, myPos.z, me->getGoId());

                // Viral Assimilation!
                target->setFactionName("Machines");
                targetBot->SetFaction(FACTION_MACHINES);
                targetBot->setAgent(true);
                target->setHandle("Agent_Smith_Clone");
                target->setRsiHex("6e060040"); // Authentic Agent suit & sunglasses
                
                // Agents assimilate host to restore their integrity
                me->setCurrentHealth(me->getMaximumHealth());

                // Record in Memory Stream
                MemoryNode mem;
                mem.text = (format("Assimilated host %1% into Agent clone") % target->getHandle()).str();
                mem.importance = 0.9f;
                mem.timestamp = double(getMSTime());
                bot->GetMemoryStreamCuller().AddMemory(mem);

                BotManager::getSingletonPtr()->LogCombat((format("[VIRAL INFECTION] %1% possessed %2%!") % me->getHandle() % target->getHandle()).str());
                
                sGame.AnnounceStateUpdateNear(tPos.x, tPos.z, 20000.0f, std::make_shared<EmoteMsg>(target->getGoId(), 43, 1));

                if (bot->GetTargetGoId() == target->getGoId()) {
                    bot->SetTargetGoId(0);
                }
                return NodeStatus::SUCCESS;
            }
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionEngageTarget::Tick(BotClient* bot)
{
    if (bot->GetTargetGoId() == 0) return NodeStatus::FAILURE;

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    PlayerObject* target = BotGetPlayer(bot->GetTargetGoId());

    if (!me || !target || target->isDead())
    {
        bot->SetTargetGoId(0);
        return NodeStatus::FAILURE;
    }

    LocationVector myPos = me->getPosition();
    LocationVector targetPos = target->getPosition();
    float dist = sqrt(pow(myPos.x - targetPos.x, 2) + pow(myPos.y - targetPos.y, 2) + pow(myPos.z - targetPos.z, 2));

    if (dist > 250.0f) //melee engage distance
    {
        // Move towards target scaled by deltaSeconds for uniform movement across LOD tiers
        float dx = targetPos.x - myPos.x;
        float dz = targetPos.z - myPos.z;
        float length = std::max(0.01f, static_cast<float>(sqrt(dx*dx + dz*dz)));
        dx /= length; dz /= length;
        
        float dt = (bot->GetDeltaSeconds() > 0.0001f) ? bot->GetDeltaSeconds() : 0.033f;
        float approachSpeed = 4500.0f; // world units per second (matches 150.0f / 0.0333s)
        myPos.x += dx * approachSpeed * dt;
        myPos.z += dz * approachSpeed * dt;
        
        me->setPosition(myPos);
        sGame.AnnounceStateUpdate(&me->getClient(), make_shared<PositionStateMsg>(bot->GetPlayerGoId()));
        return NodeStatus::RUNNING; // Still moving
    }
    
    // Within range
    return NodeStatus::SUCCESS; 
}

NodeStatus ActionCombatCycle::Tick(BotClient* bot)
{
    if (bot->GetTargetGoId() == 0) {
        return NodeStatus::FAILURE;
    }

    uint32 currentTime = getMSTime();
    if (currentTime < bot->GetNextActionTime()) {
        return NodeStatus::RUNNING;
    }

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    PlayerObject* target = BotGetPlayer(bot->GetTargetGoId());

    if (!me || !target) {
        return NodeStatus::FAILURE;
    }

    //make sure we are actually engaged through the combat system
    if (!me->isInCombat())
    {
        bot->AttackTarget(bot->GetTargetGoId());
    }

    // Q-Learning Tactic Selection
    float hpPct = float(me->getCurrentHealth()) / float(me->getMaximumHealth());
    float dist = float(me->getPosition().Distance(target->getPosition()));
    bool hasIS = me->getCurrentIS() > 50;

    std::string currentState = BotClient::s_qTable.GetStateHash(hpPct, dist, hasIS);
    std::vector<std::string> actions = {"DEFENSE", "RETALIATE", "POWER", "SPEED", "GRAB"};
    
    // Inject faction-based combat flavor
    float r = static_cast<float>(rand()) / RAND_MAX;
    std::string chosenStr;
    
    if (r > 0.8f) {
        // 20% chance to override Q-Learning with Faction-specific flavor
        if (me->getFaction() == FACTION_MACHINES) {
            chosenStr = "SPEED"; // Agents are overwhelmingly fast
        } else if (me->getFaction() == FACTION_MEROVINGIAN) {
            chosenStr = "GRAB"; // Exiles fight dirty
        } else {
            chosenStr = "POWER"; // Zion relies on brute force
        }
    } else {
        chosenStr = BotClient::s_qTable.SelectBestAction(currentState, actions);
    }
    
    uint8 chosen;
    if (chosenStr == "DEFENSE") chosen = TACTIC_DEFENSE;
    else if (chosenStr == "RETALIATE") chosen = TACTIC_RETALIATE;
    else if (chosenStr == "POWER") chosen = TACTIC_POWER;
    else if (chosenStr == "GRAB") chosen = TACTIC_RETALIATE;
    else chosen = TACTIC_SPEED;

    me->setTactic(chosen);
    sCombatSys.SetTactic(bot->GetPlayerGoId(), chosen);
    
    // Evaluate reward for previous state (mock simulation)
    // Normally, the damage taken/dealt events from CombatSystem would trigger UpdateQValue.
    // For this rewrite, we will just use a basic heuristic: if we used power and the target's HP is low, it's good.
    float targetHpPct = float(target->getCurrentHealth()) / float(target->getMaximumHealth());
    float reward = (1.0f - targetHpPct) - (1.0f - hpPct); 
    BotClient::s_qTable.UpdateQValue(currentState, chosenStr, reward, currentState, actions);

    uint32 delay = (uint32)(2000.0f / me->GetTimeDilation());
    bot->SetNextActionTime(currentTime + delay); // Scaled GCD

    return NodeStatus::SUCCESS;
}

NodeStatus ActionLeash::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    LocationVector myPos = me->getPosition();
    float distToSpawn = sqrt(pow(myPos.x - bot->GetSpawnX(), 2) + pow(myPos.z - bot->GetSpawnZ(), 2));

    if (distToSpawn > 5000.0f) //50m leash
    {
        bot->SetTargetGoId(0); // Drop aggro
        sCombatSys.RemoveCombatant(bot->GetPlayerGoId());
        bot->MoveTo(bot->GetSpawnX(), bot->GetSpawnY(), bot->GetSpawnZ());
        DEBUG_LOG("BotClient: Leashing back to spawn.");
        return NodeStatus::SUCCESS; // Handled leash
    }

    return NodeStatus::FAILURE; // No need to leash
}

NodeStatus ActionFlee::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    // Health check or Panic state (Item 7)
    if (me->getCurrentHealth() < 0.2f * me->getMaximumHealth() || bot->IsPanicking())
    {
        // Do not break interlock artificially. If in interlock, we must fight to the death or use valid tactics.
        if (sCombatSys.IsInterlocked(bot->GetPlayerGoId()))
        {
            bot->SetPanicking(false); // Can't flee if interlocked
            return NodeStatus::FAILURE; 
        }

        DEBUG_LOG("BotClient: Fleeing!");
        
        bot->SetTargetGoId(0); // Break combat targeting
        
        // Item 11: Pathfind to nearest Hardline to Jack-Out
        LocationVector myPos = me->getPosition();
        LocationVector nearestHardline = BotManager::getSingletonPtr()->GetNearestHardline(myPos.x, myPos.z);
        
        float distToHardline = sqrt(pow(myPos.x - nearestHardline.x, 2) + pow(myPos.z - nearestHardline.z, 2));
        
        if (distToHardline < 50.0f) // Close enough to the hardline to jack-out
        {
            DEBUG_LOG(format("Hacker %1% jacked out successfully.") % me->getHandle());
            sGame.BroadcastNear(me->getPosition().x, me->getPosition().z, 200.0f, make_shared<JackoutEffectMsg>(me->getGoId(), true)->toBuf(), false);
            bot->Invalidate(); // Despawn
            me->saveDataToDB();
            me->die(0); // Trigger death state to despawn safely
            return NodeStatus::SUCCESS;
        }
        else
        {
            bot->MoveTo(nearestHardline.x, nearestHardline.y, nearestHardline.z);
            return NodeStatus::SUCCESS;
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionCastAbility::Tick(BotClient* bot)
{
    if (!sCombatSys.IsInterlocked(bot->GetPlayerGoId()))
        return NodeStatus::FAILURE;

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    // Build a dynamic pool of all valid abilities the bot can afford
    std::vector<uint16> validAbilities;
    uint32 currentIS = me->getCurrentIS();
    const auto& allAbilities = sDataLoader.GetAllAbilities();

    for (const auto& pair : allAbilities)
    {
        const AbilityTemplate& tpl = pair.second;
        if (tpl.innerStrengthCost <= currentIS && tpl.isCastable)
        {
            validAbilities.push_back(pair.first);
        }
    }

    if (validAbilities.empty())
    {
        // If they can't afford anything, fall back to basic kick (2) or just fail
        sCombatSys.QueueAbility(bot->GetPlayerGoId(), 2);
        return NodeStatus::SUCCESS;
    }

    // Pick a random ability from the valid pool
    int randomIndex = rand() % validAbilities.size();
    uint16 queuedAbility = validAbilities[randomIndex];

    sCombatSys.QueueAbility(bot->GetPlayerGoId(), queuedAbility);
    return NodeStatus::SUCCESS;
}

NodeStatus ActionHyperjump::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    float healthPct = (float)me->getCurrentHealth() / (me->getMaximumHealth() == 0 ? 1 : me->getMaximumHealth());
    
    // Item 9: Corrupted Redpills
    if (me->getHandle() == "Corrupted_Redpill") {
        if ((rand() % 100) < 5) { // 5% chance to glitch jump per tick
            if (sCombatSys.IsInterlocked(bot->GetPlayerGoId())) {
                LocationVector p = me->getPosition();
                bot->MoveTo(p.x + (rand()%1000 - 500), p.y, p.z + (rand()%1000 - 500));
                bot->Say("G-g-g-glitch detected!");
                return NodeStatus::SUCCESS;
            }
        }
    }
    
    // Hyperjump if health < 15%
    if (healthPct < 0.15f) 
    {
        bot->Say("I'm jacking out!");
        
        // Item 10 & 11: Find nearest authentic Hardline
        LocationVector nearestHardline = BotManager::getSingletonPtr()->GetNearestHardline(me->getPosition().x, me->getPosition().z);

        LocationVector myPos = me->getPosition();
        float dist = sqrt(pow(myPos.x - nearestHardline.x, 2) + pow(myPos.z - nearestHardline.z, 2));

        if (dist < 50.0f) // Close enough to the hardline
        {
            bot->Say("Operator, get me out of here!");
            DEBUG_LOG(format("Agent %1% jacked out successfully.") % me->getHandle());
            sGame.BroadcastNear(me->getPosition().x, me->getPosition().z, 200.0f, make_shared<JackoutEffectMsg>(me->getGoId(), true)->toBuf(), false);
            bot->Invalidate(); // Despawn and Serialize
            me->saveDataToDB();
            me->die(0); // Trigger death state to despawn
            return NodeStatus::SUCCESS;
        }
        else
        {
            bot->MoveTo(nearestHardline.x, nearestHardline.y, nearestHardline.z);
            return NodeStatus::SUCCESS;
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionPartyInvite::Tick(BotClient* bot)
{
    if (bot->GetCrewLeaderId() != 0) return NodeStatus::FAILURE;

    if ((rand() % 1000) < 5) // 0.5% chance per tick
    {
        PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
        if (!me) return NodeStatus::FAILURE;

        auto nearbyClients = sSpatialGrid.GetClientsInRadius(me->getPosition().x, me->getPosition().z);
        for (GameClient* client : nearbyClients)
        {
            if (client == bot) continue;
            if (client->isBot())
            {
                BotClient* otherBot = static_cast<BotClient*>(client);
                if (otherBot->GetFaction() == bot->GetFaction())
                {
                    bot->SetCrewLeaderId(otherBot->GetPlayerGoId());
                    bot->Say("Let's group up!");
                    return NodeStatus::SUCCESS;
                }
            }
        }
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionIdle::Tick(BotClient* bot)
{
    // Item 18: Idle Animations
    if ((rand() % 1000) < 10) { // 1% chance per tick
        int animId = rand() % 3;
        if (animId == 0) bot->Say("*smokes a cigarette*");
        else if (animId == 1) bot->Say("*stretches*");
        else bot->Say("*reviews source code*");
        
        // Notify nearby clients of idle action
        PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
        if (me) {
            INFO_LOG(format("%1% is performing an idle animation") % me->getHandle());
            // Broadcast actual animation packet (using Emote as proxy for now since it broadcasts)
            // Emote ID 100+ usually reserved for ambient indles
            bot->Emote(100 + animId); 
        }
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionEvade::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me || me->isDead() || bot->GetTargetGoId() == 0) return NodeStatus::FAILURE;

    // Item 31: Evade logic if health is low or under attack
    float healthPct = float(me->getCurrentHealth()) / float(me->getMaximumHealth());
    if (healthPct < 0.4f && (rand() % 100) < 20) { // 20% chance to evade when low health
        me->setTactic(TACTIC_DEFENSE);
        
        // Emote 10 as dodge/evade
        bot->Emote(10);
        INFO_LOG(format("%1% is evading!") % me->getHandle());
        return NodeStatus::SUCCESS;
    }

    return NodeStatus::FAILURE;
}

// Item 107: Escort AI Logic
NodeStatus ActionEscort::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    PlayerObject* target = BotGetPlayer(m_targetGoId);
    if (!target || target->isDead()) return NodeStatus::FAILURE;

    float dx = target->getPosition().x - me->getPosition().x;
    float dz = target->getPosition().z - me->getPosition().z;
    float dist = std::sqrt(dx*dx + dz*dz);

    if (dist > m_followDistance) {
        float moveSpeed = 5.0f; // 5m/s
        if (dist > m_followDistance * 3.0f) moveSpeed = 10.0f; // sprint to catch up

        dx /= dist;
        dz /= dist;
        float newX = me->getPosition().x + dx * bot->m_deltaSeconds * moveSpeed;
        float newZ = me->getPosition().z + dz * bot->m_deltaSeconds * moveSpeed;
        bot->MoveTo(newX, me->getPosition().y, newZ);
        return NodeStatus::RUNNING;
    }
    return NodeStatus::SUCCESS;
}

NodeStatus ActionRebirth::Tick(BotClient* bot)
{
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    // Check if we meet the conditions to Rebirth (e.g. High Tier Agent/Exile, defeated or critically low health, level 50)
    // We'll simulate that high tier bots are level 50. If they hit 0 health, they might die. 
    // To rebirth before actual deletion, we trigger this when health is 0 but we intercept it.
    if (me->getCurrentHealth() == 0 && me->getLevel() >= 50) {
        // Only Agents and Exiles do this, check name/faction (simulated via name prefix)
        if (me->getFirstName().find("Agent") != std::string::npos || me->getFirstName().find("Exile") != std::string::npos) {
            me->PerformRebirth();
            // Reset death state basically handled by PerformRebirth healing them to full
            return NodeStatus::SUCCESS;
        }
    }
    return NodeStatus::FAILURE;
}


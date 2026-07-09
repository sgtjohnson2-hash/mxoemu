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
    uint32 currentTime = getMSTime();
    if (currentTime < bot->GetNextActionTime()) 
        return NodeStatus::RUNNING;

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    // Pick a random point within 5 units of spawn
    float rx = bot->GetSpawnX() + ((rand() % 1000) / 100.0f - 5.0f);
    float rz = bot->GetSpawnZ() + ((rand() % 1000) / 100.0f - 5.0f);
    LocationVector newPos = me->getPosition();
    newPos.x = rx;
    newPos.z = rz;
    me->setPosition(newPos);
    
    bot->SetNextActionTime(currentTime + 3000 + (rand() % 2000));
    return NodeStatus::SUCCESS;
}

NodeStatus ActionFindTarget::Tick(BotClient* bot)
{
    if (!BotManager::getSingletonPtr()->IsAggroEnabled()) return NodeStatus::FAILURE;

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me) return NodeStatus::FAILURE;

    std::vector<uint32> allIds = sObjMgr.getAllGOIds();
    for (size_t i = 0; i < allIds.size(); i++)
    {
        if (allIds[i] == bot->GetPlayerGoId()) continue;
        
        PlayerObject* potentialTarget = BotGetPlayer(allIds[i]);
        if (potentialTarget && !potentialTarget->isDead() && !potentialTarget->isStealthed())
        {
            // Simple aggro radius
            LocationVector myPos = me->getPosition();
            LocationVector targetPos = potentialTarget->getPosition();
            float dist = sqrt(pow(myPos.x - targetPos.x, 2) + pow(myPos.y - targetPos.y, 2) + pow(myPos.z - targetPos.z, 2));
            if (dist < 1500.0f) //15m aggro radius
            {
                bot->SetTargetGoId(allIds[i]);
                bot->Say("Target locked!");
                bot->Emote(0); // Emote before combat
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
        // Move towards target
        float dx = targetPos.x - myPos.x;
        float dz = targetPos.z - myPos.z;
        float length = sqrt(dx*dx + dz*dz);
        dx /= length; dz /= length;
        
        myPos.x += dx * 150.0f; //1.5m per AI tick
        myPos.z += dz * 150.0f;
        me->setPosition(myPos);
        sGame.AnnounceStateUpdate(&me->getClient(), make_shared<PositionStateMsg>(bot->GetPlayerGoId()));
        return NodeStatus::RUNNING; // Still moving
    }
    
    // Within range
    return NodeStatus::SUCCESS; 
}

NodeStatus ActionCombatCycle::Tick(BotClient* bot)
{
    if (bot->GetTargetGoId() == 0) return NodeStatus::FAILURE;

    uint32 currentTime = getMSTime();
    if (currentTime < bot->GetNextActionTime()) 
        return NodeStatus::RUNNING;

    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    PlayerObject* target = BotGetPlayer(bot->GetTargetGoId());

    if (!me || !target) return NodeStatus::FAILURE;

    //make sure we are actually engaged through the combat system
    if (!me->isInCombat())
    {
        bot->AttackTarget(bot->GetTargetGoId());
    }

    //rotate tactics like a live opponent: block when hurt, otherwise mix it up
    uint8 chosen;
    float hpPct = float(me->getCurrentHealth()) / float(me->getMaximumHealth());
    if (hpPct < 0.3f)
    {
        chosen = TACTIC_DEFENSE; //block absorbs hits and regains IS
    }
    else
    {
        uint8 tactics[] = {TACTIC_RETALIATE, TACTIC_POWER, TACTIC_SPEED};
        chosen = tactics[rand() % 3];
    }
    me->setTactic(chosen);
    sCombatSys.SetTactic(bot->GetPlayerGoId(), chosen);

    bot->SetNextActionTime(currentTime + 2000); // 2 second GCD

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

    // Health check
    if (me->getCurrentHealth() < 0.2f * me->getMaximumHealth())
    {
        // Do not break interlock artificially. If in interlock, we must fight to the death or use valid tactics.
        if (sCombatSys.IsInterlocked(bot->GetPlayerGoId()))
        {
            // Fail the flee action, letting the CombatCycle take over (which already handles low HP by switching to TACTIC_DEFENSE)
            return NodeStatus::FAILURE; 
        }

        DEBUG_LOG("BotClient: Health critical! Fleeing!");
        
        PlayerObject* target = BotGetPlayer(bot->GetTargetGoId());
        if (target)
        {
            LocationVector myPos = me->getPosition();
            LocationVector targetPos = target->getPosition();
            
            float dx = myPos.x - targetPos.x;
            float dz = myPos.z - targetPos.z;
            
            bot->MoveTo(myPos.x + dx * 2, myPos.y, myPos.z + dz * 2);
            bot->SetTargetGoId(0); // Break combat targeting
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
        // Don't use passive abilities (usually cast time 0 or 0 cost, depending on DB)
        // Let's assume an attack has an executionFX or valid cast time and we can afford it.
        // And we skip massive IS cost ones just to be safe if they're broken? No, user says use all.
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

#include "BotClient.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Timer.h"
#include "BehaviorTree.h"
#include "MessageTypes.h"
#include "GameServer.h"
#include "CombatSystem.h"
#include <cmath>

// sObjMgr.getGOPtr throws on missing objects - the bot AI wants a null instead
PlayerObject* BotGetPlayer(uint32 goId)
{
    if (goId == 0)
        return NULL;
    try
    {
        return sObjMgr.getGOPtr(goId);
    }
    catch (ObjectMgr::ObjectNotAvailable)
    {
        return NULL;
    }
}

BotClient::BotClient(uint64 charUID) 
    : GameClient(sockaddr_in(), nullptr), m_targetGoId(0), m_stateTimer(0), 
      m_nextActionTime(0), m_spawnX(0.0f), m_spawnY(0.0f), m_spawnZ(0.0f), m_faction(FACTION_ZION)
{
    m_characterUID = charUID;
    
    // Create the player object
    try
    {
        m_playerGoId = sObjMgr.constructPlayer(this, m_characterUID, true);
        sObjMgr.getGOPtr(m_playerGoId)->InitializeWorld();
        sObjMgr.getGOPtr(m_playerGoId)->PopulateWorld();
        sObjMgr.getGOPtr(m_playerGoId)->SpawnSelf();
        
        // Dummy encryption to avoid errors
        m_encryptionInitialized = true;
        DEBUG_LOG(format("BotClient constructed for charUID %1%") % m_characterUID);
    }
    catch (ObjectMgr::ObjectNotAvailable)
    {
        ERROR_LOG(format("Failed to construct player object for BotClient: ObjectNotAvailable charUID %1%") % m_characterUID);
    }
    catch (const std::exception& e)
    {
        ERROR_LOG(format("Failed to construct player object for BotClient: %1%") % e.what());
    }
    catch (...)
    {
        ERROR_LOG(format("Failed to construct player object for BotClient: UNKNOWN EXCEPTION charUID %1%") % m_characterUID);
    }
}

BotClient::~BotClient()
{
}

void BotClient::FlushQueue(bool alsoResend)
{
    // Do nothing. Overrides GameClient to prevent network transmission.
    m_queuedCommands.clear();
    m_queuedStates.clear();
}

void BotClient::CheckAndResend()
{
    // Do nothing. Overrides GameClient to prevent packet resends.
}

void BotClient::UpdateBotAI()
{
    if (m_playerGoId == 0) return;

    PlayerObject* me = BotGetPlayer(m_playerGoId);
    if (!me)
        return;

    //drive event processing (scheduled respawn, regen, status effects) and
    //drop packets that combat queued at us - no real socket behind a bot
    me->Update();
    FlushQueue();

    if (me->isDead())
    {
        m_targetGoId = 0; //respawn fires through the normal EVENT_RESPAWN path
        return;
    }

    if (m_behaviorTree)
    {
        m_behaviorTree->Tick(this);
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
    if (dist <= CombatSystem::DefaultMelee()->range)
        sCombatSys.RequestInterlock(m_playerGoId, targetGoId);
    else
        sCombatSys.RequestRangedCombat(m_playerGoId, targetGoId, 0);
}

void BotClient::MoveTo(float x, float y, float z)
{
    m_spawnX = x;
    m_spawnY = y;
    m_spawnZ = z;

    if (m_playerGoId != 0)
    {
        PlayerObject* me = BotGetPlayer(m_playerGoId);
        if (me)
        {
            LocationVector loc = me->getPosition();
            loc.x = x;
            loc.y = y;
            loc.z = z;
            me->setPosition(loc);
            sGame.AnnounceStateUpdate(this, make_shared<PositionStateMsg>(m_playerGoId));
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
    sGame.AnnounceStateUpdate(NULL, shared_ptr<EmoteMsg>(new EmoteMsg(m_playerGoId, emoteId, 1)));
}

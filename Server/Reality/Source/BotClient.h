#ifndef MXOSIM_BOTCLIENT_H
#define MXOSIM_BOTCLIENT_H

#include "GameClient.h"

class BehaviorTree;

enum mxoFaction
{
    FACTION_ZION,
    FACTION_MACHINES,
    FACTION_MEROVINGIAN
};

enum class ExecutionLOD {
    ACTIVE_VIEWPORT = 0,   // < 50m: 4Hz Tick (250ms)
    APPROACH_AREA = 1,     // 50m - 200m: 1Hz Tick (1000ms)
    BACKGROUND_AREA = 2    // > 200m: 0Hz Tick (skip logic)
};

//exception-safe object lookup shared by the bot AI (defined in BotClient.cpp)
class PlayerObject* BotGetPlayer(uint32 goId);

class BotClient : public GameClient
{
public:
    BotClient(uint64 charUID);
    virtual ~BotClient();

    virtual bool isBot() const override { return true; }

    virtual void FlushQueue(bool alsoResend = false);
    virtual void CheckAndResend();

    void UpdateBotAI();
    void AttackTarget(uint32 targetGoId);
    void MoveTo(float x, float y, float z);

    void Say(const std::string& msg);
    void Emote(uint32 emoteId);

    void SetBehaviorTree(std::shared_ptr<class BehaviorNode> tree) { m_behaviorTree = tree; }
    void SetFaction(mxoFaction faction) { m_faction = faction; }
    mxoFaction GetFaction() const { return m_faction; }

    uint32 GetTargetGoId() const { return m_targetGoId; }
    void SetTargetGoId(uint32 id) { m_targetGoId = id; }
    uint32 GetNextActionTime() const { return m_nextActionTime; }
    void SetNextActionTime(uint32 time) { m_nextActionTime = time; }
    float GetSpawnX() const { return m_spawnX; }
    float GetSpawnY() const { return m_spawnY; }
    float GetSpawnZ() const { return m_spawnZ; }

    ExecutionLOD GetLOD() const { return m_currentLOD; }
    void SetLOD(ExecutionLOD lod) { m_currentLOD = lod; }
    uint32 GetLastLodTick() const { return m_lastLodTickMS; }
    void SetLastLodTick(uint32 tick) { m_lastLodTickMS = tick; }


private:
    uint32 m_targetGoId;
    uint32 m_stateTimer;
    uint32 m_nextActionTime;
    float m_spawnX;
    float m_spawnY;
    float m_spawnZ;
    mxoFaction m_faction;
    std::shared_ptr<class BehaviorNode> m_behaviorTree;
    
    ExecutionLOD m_currentLOD;
    uint32 m_lastLodTickMS;
};

#endif

#ifndef MXOSIM_BOTMANAGER_H
#define MXOSIM_BOTMANAGER_H

#include "BotClient.h"
#include <vector>
#include <memory>
#include "Singleton.h"

class BotManager : public Singleton<BotManager>
{
public:
    BotManager();
    ~BotManager();

    void SpawnBot(int count, float x, float y, float z, int faction = 0);
    void CommandBotAttack(const std::string& targetName);
    void BotStressTest(int count);
    
    void EnableBotAggro(bool enabled) { m_aggroEnabled = enabled; }
    bool IsAggroEnabled() const { return m_aggroEnabled; }

    void EnableCombatLogging(bool enabled) { m_combatLogging = enabled; }
    bool IsCombatLoggingEnabled() const { return m_combatLogging; }
    void LogCombat(const std::string& msg);

    void Update();

    void PopulateWorld();

private:
    uint64 findOrCreateBotCharacter(int botNumber, float x, float y, float z, int faction);

    std::vector<std::shared_ptr<BotClient>> m_bots;
    uint64 m_nextBotId;
    uint32 m_lastAiTickMS;
    uint32 m_lastTrafficTickMS;
    bool m_aggroEnabled;
    bool m_combatLogging;
    std::shared_ptr<class BehaviorNode> m_sharedBehaviorTree;
};

#define sBotMgr BotManager::getSingleton()

#endif

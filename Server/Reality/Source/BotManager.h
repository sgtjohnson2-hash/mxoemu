#ifndef MXOSIM_BOTMANAGER_H
#define MXOSIM_BOTMANAGER_H

#include "BotClient.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include "Singleton.h"


class BotManager : public Singleton<BotManager>
{
public:
    BotManager();
    ~BotManager();

    void SpawnBot(int count, float x, float y, float z, int faction = 0);
    std::shared_ptr<BotClient> SpawnSingleBot(float x, float y, float z, int faction = 0);
    uint32 SpawnMissionBot(const struct MissionNpc& npcInfo, uint32 instanceId);
    void CommandBotAttack(const std::string& targetName);
    void BotStressTest(int count);
    
    // V16: Oracle's Prophecy
    void SpawnHighValueTarget(int district);
    
    // Item 30: Faction Warfare
    void SpawnFactionDefenders(uint8 district, uint32 hlId, uint8 faction, LocationVector loc);
    
    // Item 10: Hardlines
    void LoadHardlines();
    LocationVector GetRandomHardline();
    LocationVector GetNearestHardline(float x, float z);
    
    std::shared_ptr<BotClient> GetBotByGOID(uint32 goid);

    void EnableBotAggro(bool enabled) { m_aggroEnabled = enabled; }
    bool IsAggroEnabled() const { return m_aggroEnabled; }

    void EnableCombatLogging(bool enabled) { m_combatLogging = enabled; }
    bool IsCombatLoggingEnabled() const { return m_combatLogging; }
    void LogCombat(const std::string& msg);

    // Host Decontamination & Awakening
    void HandleCleanseAwakening(uint32 entityGoId);

    void Update();

    void PopulateWorld();
    size_t GetBotCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        return m_bots.size();
    }
    const std::vector<LocationVector>& GetHardlines() const { return m_hardlines; }
    uint32 GetNextCrewId() { return ++m_nextCrewId; }

private:
    uint64 findOrCreateBotCharacter(int botNumber, float x, float y, float z, int faction);

    std::vector<std::shared_ptr<BotClient>> m_bots;
    std::vector<LocationVector> m_hardlines;
    std::atomic<uint64> m_nextBotId;
    std::atomic<uint32> m_nextCrewId;
    uint32 m_lastAiTickMS;
    uint32 m_lastTrafficTickMS;
    uint32 m_lastPlayerCacheTickMS;
    std::vector<uint32> m_activePlayerIds;
    bool m_aggroEnabled;
    bool m_combatLogging;
    mutable std::recursive_mutex m_botMutex;
};

#define sBotMgr BotManager::getSingleton()

#endif

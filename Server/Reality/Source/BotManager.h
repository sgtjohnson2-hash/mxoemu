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
    std::shared_ptr<BotClient> GetBotByPlayerGoId(uint32 goid) { return GetBotByGOID(goid); }

    void EnableBotAggro(bool enabled) { m_aggroEnabled = enabled; }
    bool IsAggroEnabled() const { return m_aggroEnabled; }

    void EnableCombatLogging(bool enabled) { m_combatLogging = enabled; }
    bool IsCombatLoggingEnabled() const { return m_combatLogging; }
    void LogCombat(const std::string& msg);

    // Host Decontamination & Awakening
    void HandleCleanseAwakening(uint32 entityGoId);

    void Update();

    void PopulateWorld();
    void PruneDeadBots();

    // Removes one bot (mission NPC cleanup). Deferred to the next BotManager::Update because
    // it is called from inside die()/AdvanceObjective of the very object being removed.
    void DespawnBot(uint32 goId);

    // Tester bots ("PlayerSim", BotTester.cpp): spawn Bots.Testers players that level,
    // run missions, fight and shop through the real client RPC handlers, and report.
    void SpawnTesters();
    std::shared_ptr<BotClient> SpawnTester(int index, const LocationVector& start);
    std::string GetTesterSummary();
    void WriteTesterReport();
    static constexpr size_t MAX_BOT_POPULATION_CEILING = 12000;
    size_t GetBotCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        return m_bots.size();
    }
    const std::vector<LocationVector>& GetHardlines() const { return m_hardlines; }
    uint32 GetNextCrewId() { return ++m_nextCrewId; }

    std::shared_ptr<const std::vector<std::shared_ptr<BotClient>>> GetBotsSnapshot() {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        if (m_botsDirty || !m_botsSnapshot) {
            m_botsSnapshot = std::make_shared<const std::vector<std::shared_ptr<BotClient>>>(m_bots);
            m_botsDirty = false;
        }
        return m_botsSnapshot;
    }

private:
    uint64 findOrCreateBotCharacter(int botNumber, float x, float y, float z, int faction);

    std::vector<std::shared_ptr<BotClient>> m_bots;
    std::shared_ptr<const std::vector<std::shared_ptr<BotClient>>> m_botsSnapshot;
    bool m_botsDirty{true};
    size_t m_recycleBotIndex{0};
    std::vector<LocationVector> m_hardlines;
    std::atomic<uint64> m_nextBotId;
    std::atomic<uint32> m_nextCrewId;
    uint32 m_lastAiTickMS;
    uint32 m_lastTrafficTickMS;
    uint32 m_lastPlayerCacheTickMS;
    std::vector<uint32> m_activePlayerIds;
    bool m_aggroEnabled;
    bool m_combatLogging;
    std::vector<std::shared_ptr<BotClient>> m_testers;
    std::vector<uint32> m_pendingDespawn;
    void ProcessPendingDespawns();
    uint32 m_lastTesterReportMs{0};
    mutable std::recursive_mutex m_botMutex;
};

#define sBotMgr BotManager::getSingleton()

#endif

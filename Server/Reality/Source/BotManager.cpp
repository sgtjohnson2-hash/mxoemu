#include "BotManager.h"
#include "MessageTypes.h"
#include "DataLoader.h"
#include "GameServer.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "GameClient.h"
#include "SpatialGrid.h"
#include "BehaviorTree.h"
#include "Database/Database.h"
#include "NavGrid.h"
#include <execution>
#include <algorithm>
#include <cstdlib>

#include "PlayerObject.h"
#include "FactionWarManager.h"
#include "MissionSystem.h"
#include "Threading/TaskScheduler.h"
#include "Config.h"
#include "BotTester.h"
#include "CombatSystem.h"
#include <fstream>
#include <memory>

createFileSingleton(BotManager);

BotManager::BotManager()
{
    m_nextBotId = 9000000;
    m_nextCrewId = 0;
    m_lastAiTickMS = 0;
    m_lastTrafficTickMS = 0;
    m_lastPlayerCacheTickMS = 0;
    m_aggroEnabled = true;
    m_combatLogging = true;
    m_botsDirty = true;
}

BotManager::~BotManager()
{
    std::lock_guard<std::recursive_mutex> lock(m_botMutex);
    m_bots.clear();
    m_botsSnapshot.reset();
    m_botsDirty = true;
}

void BotManager::LoadHardlines()
{
    INFO_LOG("BotManager: Loading Hardlines from database...");
    m_hardlines.clear();
    scoped_ptr<QueryResult> result(sDatabase.Query("SELECT `X`, `Y`, `Z` FROM `hardlines`"));
    if (result)
    {
        do 
        {
            Field* fields = result->Fetch();
            LocationVector loc;
            loc.x = fields[0].GetFloat();
            loc.y = fields[1].GetFloat();
            loc.z = fields[2].GetFloat();
            m_hardlines.push_back(loc);
        } while (result->NextRow());
    }
    INFO_LOG(format("BotManager: Loaded %1% Hardlines.") % m_hardlines.size());
}

LocationVector BotManager::GetRandomHardline()
{
    if (m_hardlines.empty()) return LocationVector(0.0f, 0.0f, 0.0f);
    return m_hardlines[rand() % m_hardlines.size()];
}

LocationVector BotManager::GetNearestHardline(float x, float z)
{
    if (m_hardlines.empty()) return LocationVector(100.0f, 0.0f, 100.0f); // Fallback
    
    LocationVector nearest = m_hardlines[0];
    float min_dist = -1.0f;
    for (size_t i = 0; i < m_hardlines.size(); i++)
    {
        float dx = m_hardlines[i].x - x;
        float dz = m_hardlines[i].z - z;
        float distSq = (dx * dx) + (dz * dz);
        if (min_dist < 0.0f || distSq < min_dist)
        {
            min_dist = distSq;
            nearest = m_hardlines[i];
        }
    }
    return nearest;
}

// Bots are backed by real character rows so the normal PlayerObject DB load
// works for them. Rows are created on demand and reused between runs.
uint64 BotManager::findOrCreateBotCharacter(int botNumber, float x, float y, float z, int faction)
{
    // Bypassed: Bots are now completely virtual and live in memory.
    // Return a dummy UID starting from 9000000.
    return 9000000 + botNumber;
}

std::shared_ptr<BotClient> BotManager::SpawnSingleBot(float x, float y, float z, int faction)
{
    // Item 113: Dynamic Spawns based on Control
    if (faction == FACTION_MACHINES || faction == FACTION_ZION || faction == FACTION_MEROVINGIAN) {
        faction = FactionWarManager::getSingleton().getControllingFactionByLocation(x, y, z);
    }

    // Phase 2: Bot Population Ceiling & Spatial Recycling
    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        size_t activeBotLimit = (size_t)sConfig.GetIntDefault("BotManager.ActiveBotLimit", 500);
        if (activeBotLimit > MAX_BOT_POPULATION_CEILING) activeBotLimit = MAX_BOT_POPULATION_CEILING;
        if (m_bots.size() >= activeBotLimit && !m_bots.empty()) {
            size_t idx = (m_recycleBotIndex++) % m_bots.size();
            auto recycledBot = m_bots[idx];
            if (recycledBot && recycledBot->IsTester()) recycledBot.reset();
            if (recycledBot) {
                recycledBot->SetFaction((mxoFaction)faction);
                recycledBot->MoveTo(x, y, z);
                PlayerObject* po = sObjMgr.getGOPtrSafe(recycledBot->GetPlayerGoId());
                if (po) {
                    po->setPosition(LocationVector(x, y, z));
                    std::string factionName = "Civilian";
                    if (faction == FACTION_ZION) factionName = "Zion";
                    else if (faction == FACTION_MACHINES) factionName = "Machines";
                    else if (faction == FACTION_MEROVINGIAN) factionName = "Merovingian";
                    po->setFactionName(factionName);
                }
                return recycledBot;
            }
        }
    }

    uint64 uid = findOrCreateBotCharacter(int(++m_nextBotId), x, y, z, faction);
    if (uid == 0)
    {
        ERROR_LOG("BotManager: could not create bot character");
        return nullptr;
    }
    std::shared_ptr<BotClient> bot = std::make_shared<BotClient>(uid);
    bot->SetFaction((mxoFaction)faction);

    // A fresh BotClient's PlayerObject sits at world origin (0,0,0). MoveTo() only plans a
    // path / updates the spatial grid, so without this the bot is really AT the origin and
    // walks from there (and is AoI-culled for any player who is not near the origin).
    PlayerObject* po = sObjMgr.getGOPtr(bot->GetPlayerGoId());
    if (po) {
        po->setPosition(LocationVector(x, y, z));
    }
    bot->MoveTo(x, y, z);
    
    // Give bot a mock ranged weapon (e.g., Template ID 500 = SMG)
    if (po) {
        po->giveItem(500); // 500 is just a mock template ID for now
        
        std::string factionName = "Civilian";
        if (faction == FACTION_ZION) factionName = "Zion";
        else if (faction == FACTION_MACHINES) {
            factionName = "Machines";
            if ((rand() % 100) < 5) { // 5% chance
                bot->setAgent(true);
                po->setHandle("Agent Simulacra");
                po->setAgentAppearance();
            }
        }
        else if (faction == FACTION_MEROVINGIAN) factionName = "Merovingian";
        po->setFactionName(factionName);
        po->SpawnSelf();
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        m_bots.push_back(bot);
        m_botsDirty = true;
    }
    DEBUG_LOG(format("BotManager: Spawned bot UID %1% (Faction %5%) at %2%, %3%, %4%") % uid % x % y % z % faction);
    return bot;
}

void BotManager::SpawnBot(int count, float x, float y, float z, int faction)
{
    for (int i = 0; i < count; i++)
    {
        // Add a small random offset so they don't stack exactly and can trigger Boids separation
        float randX = ((rand() % 200) / 100.0f) - 1.0f; // -1.0 to 1.0
        float randZ = ((rand() % 200) / 100.0f) - 1.0f;
        
        SpawnSingleBot(x + randX, y, z + randZ, faction);
    }
}

uint32 BotManager::SpawnMissionBot(const MissionNpc& npcInfo, uint32 instanceId)
{
    uint64 uid = findOrCreateBotCharacter(int(++m_nextBotId), npcInfo.x, npcInfo.y, npcInfo.z, 0);
    if (uid == 0)
    {
        ERROR_LOG("BotManager: could not create mission bot character");
        return 0;
    }
    std::shared_ptr<BotClient> bot = std::make_shared<BotClient>(uid);
    int faction = (npcInfo.type == "FRIENDLY") ? FACTION_ZION : FACTION_MACHINES;
    bot->SetFaction((mxoFaction)faction);
    bot->MoveTo(npcInfo.x, npcInfo.y, npcInfo.z);
    
    bot->SetSpawnLocation(npcInfo.x, npcInfo.y, npcInfo.z);
    if (npcInfo.type == "FRIENDLY")
        bot->SetPassive(true); // quest givers stand still and never pick fights

    PlayerObject* po = sObjMgr.getGOPtr(bot->GetPlayerGoId());
    if (po) {
        po->setHandle(npcInfo.handle);
        po->getClient().m_instanceId = instanceId;
        po->setFactionName(npcInfo.type);
        // a fresh bot sits at the origin until positioned (MoveTo only plans a path)
        po->setPosition(LocationVector(npcInfo.x, npcInfo.y, npcInfo.z));
        sSpatialGrid.UpdateClientPosition(bot.get(), npcInfo.x, npcInfo.z);
        if (npcInfo.level > 0 && npcInfo.level <= 255) po->setLevel((uint8)npcInfo.level);
        if (npcInfo.maxHP > 0) {
            uint16 hp = (uint16)std::min<uint32>(npcInfo.maxHP, 65000);
            po->setMaximumHealth(hp);
            po->setCurrentHealth(hp);
        }
        // npcInfo.idNpc could be stored if we added an idNpc field to PlayerObject, but for now we rely on the handle or instanceId
        po->SpawnSelf();
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        m_bots.push_back(bot);
        m_botsDirty = true;
    }
    INFO_LOG(format("BotManager: Spawned Mission NPC %1% (Instance %2%) at %3%, %4%, %5%") % npcInfo.handle % instanceId % npcInfo.x % npcInfo.y % npcInfo.z);
    return bot->GetPlayerGoId();
}

void BotManager::CommandBotAttack(const std::string& targetName)
{
    uint32 targetGoId = 0;
    
    // Find target by name
    std::vector<uint32> allIds = sObjMgr.getAllGOIds();
    for (size_t i = 0; i < allIds.size(); i++)
    {
        PlayerObject* po = BotGetPlayer(allIds[i]);
        if (po && po->getHandle() == targetName)
        {
            targetGoId = allIds[i];
            break;
        }
    }

    if (targetGoId == 0)
    {
        ERROR_LOG(format("BotManager: Target %1% not found for attack.") % targetName);
        return;
    }

    // Command all bots to attack
    std::shared_ptr<const std::vector<std::shared_ptr<BotClient>>> botsSnapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        if (m_botsDirty || !m_botsSnapshot) {
            m_botsSnapshot = std::make_shared<const std::vector<std::shared_ptr<BotClient>>>(m_bots);
            m_botsDirty = false;
        }
        botsSnapshot = m_botsSnapshot;
    }
    
    if (botsSnapshot) {
        for (size_t i = 0; i < botsSnapshot->size(); i++)
        {
            (*botsSnapshot)[i]->AttackTarget(targetGoId);
        }
        DEBUG_LOG(format("BotManager: Commanded %1% bots to attack %2%") % botsSnapshot->size() % targetName);
    }
}

void BotManager::BotStressTest(int count)
{
    if (m_hardlines.empty()) 
    {
        SpawnBot(count, 0.0f, 0.0f, 0.0f);
    }
    else 
    {
        // Item 10: Distribute bots evenly across all available hardlines
        int botsPerHardline = count / m_hardlines.size();
        if (botsPerHardline == 0) botsPerHardline = 1;
        
        for (const auto& hl : m_hardlines)
        {
            SpawnBot(botsPerHardline, hl.x, hl.y, hl.z, FACTION_ZION);
        }
    }
    DEBUG_LOG(format("BotManager: Stress test started with %1% bots across hardlines") % count);
}

void BotManager::Update()
{
    //INFO_LOG("DEBUG_TRACER: BotManager::Update started");
    uint32 now = getMSTime();
    float deltaSeconds = (now - m_lastAiTickMS) / 1000.0f;
    if (m_lastAiTickMS == 0) deltaSeconds = 0.0f;
    m_lastAiTickMS = now;

    ProcessPendingDespawns();



    // Spawn ambient pedestrian traffic every 30 seconds (capped at 100 bots)
    if (now - m_lastTrafficTickMS > 30000)
    {
        m_lastTrafficTickMS = now;
        
        // Find active human players and spawn a neutral pedestrian near them if they are alone
        sObjMgr.ForEachHumanPlayer([this](PlayerObject* p) {
            {
                std::lock_guard<std::recursive_mutex> lock(m_botMutex);
                if (m_bots.size() >= 100) return;
            }
            // Spawn a random pedestrian nearby (radius 20)
            float rx = p->getPosition().x + ((rand() % 40) - 20);
            float ry = p->getPosition().y;
            float rz = p->getPosition().z + ((rand() % 40) - 20);
            
            SpawnBot(1, rx, ry, rz, 1); // 1 = FACTION_MACHINES/Neutral Pedestrian
        });
    }

    // Periodic ambient bot pruning (every 60 seconds)
    static uint32 lastBotPruneMs = 0;
    if (now - lastBotPruneMs > 60000)
    {
        lastBotPruneMs = now;
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        if (m_activePlayerIds.empty() && m_bots.size() > 32)
        {
            size_t toRemove = m_bots.size() - 32;
            size_t removed = 0;
            for (auto it = m_bots.begin(); it != m_bots.end() && removed < toRemove;)
            {
                if (*it && !(*it)->IsInCombat() && (*it)->GetFaction() != FACTION_ZION && !(*it)->IsTester())
                {
                    uint32 goId = (*it)->GetPlayerGoId();
                    if (goId != 0) {
                        sObjMgr.destroyObject(goId);
                    }
                    it = m_bots.erase(it);
                    removed++;
                }
                else
                {
                    ++it;
                }
            }
            if (removed > 0) m_botsDirty = true;
        }
    }

    // Collect all active human players (cache updated every 2 seconds)
    if (now - m_lastPlayerCacheTickMS > 2000)
    {
        m_activePlayerIds = sObjMgr.getHumanPlayerGOIds();
        m_lastPlayerCacheTickMS = now;
    }

    std::shared_ptr<const std::vector<std::shared_ptr<BotClient>>> botsSnapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        if (m_botsDirty || !m_botsSnapshot) {
            m_botsSnapshot = std::make_shared<const std::vector<std::shared_ptr<BotClient>>>(m_bots);
            m_botsDirty = false;
        }
        botsSnapshot = m_botsSnapshot;
    }

    if (!botsSnapshot || botsSnapshot->empty()) {
        return;
    }

    // Tester bots always play, even with nobody online. Ticked serially here (simulation
    // thread) because they drive the same RPC handlers human packets do.
    {
        std::vector<std::shared_ptr<BotClient>> testers;
        {
            std::lock_guard<std::recursive_mutex> lock(m_botMutex);
            testers = m_testers;
        }
        for (auto& t : testers)
        {
            if (!t || t->GetPlayerGoId() == 0) continue;
            float dt = 0.2f;
            if (t->GetLastLodTick() != 0) dt = std::min(3.5f, (now - t->GetLastLodTick()) / 1000.0f);
            t->SetLastLodTick(now);
            try { t->UpdateBotAI(dt); }
            catch (const std::exception& e) { ERROR_LOG(format("BOTTEST tick exception: %1%") % e.what()); }
            catch (...) { ERROR_LOG("BOTTEST tick exception (unknown)"); }
        }
        if (!testers.empty())
        {
            static const uint32 reportMs = (uint32)sConfig.GetIntDefault("Bots.TesterReportSeconds", 300) * 1000;
            if (m_lastTesterReportMs == 0) m_lastTesterReportMs = now;
            if (now - m_lastTesterReportMs >= reportMs)
            {
                m_lastTesterReportMs = now;
                WriteTesterReport();
            }
        }
    }

    // Fast-path: When no human players are connected, assign background LOD without thread pool overhead
    if (m_activePlayerIds.empty())
    {
        bool anyCombat = false;
        for (const auto& bot : *botsSnapshot)
        {
            if (bot && (bot->IsInCombat() || bot->IsPanicking())) {
                bot->SetLOD(ExecutionLOD::APPROACH_AREA);
                anyCombat = true;
            } else if (bot) {
                bot->SetLOD(ExecutionLOD::BACKGROUND_AREA);
            }
        }
        if (!anyCombat) {
            return; // 0% CPU when server is idle with no connected players and no active combat!
        }
    }
    else
    {
        // Cache human player positions to eliminate per-bot heap allocations and lock contention
        struct HumanPos { double x, y, z; };
        std::vector<HumanPos> humanPositions;
        humanPositions.reserve(m_activePlayerIds.size());
        for (uint32 id : m_activePlayerIds)
        {
            PlayerObject* p = sObjMgr.getGOPtrSafe(id);
            if (p) {
                LocationVector pos = p->getPosition();
                humanPositions.push_back({pos.x, pos.y, pos.z});
            }
        }

        if (humanPositions.empty())
        {
            bool anyCombat = false;
            for (const auto& bot : *botsSnapshot)
            {
                if (bot && (bot->IsInCombat() || bot->IsPanicking())) {
                    bot->SetLOD(ExecutionLOD::APPROACH_AREA);
                    anyCombat = true;
                } else if (bot) {
                    bot->SetLOD(ExecutionLOD::BACKGROUND_AREA);
                }
            }
            if (!anyCombat) return;
        }
        else
        {
            const double activeRadiusSq = 10000.0 * 10000.0;   // 100m (Active Viewport)
            const double approachRadiusSq = 25000.0 * 25000.0; // 250m (Approach Area)

            std::for_each(std::execution::par, botsSnapshot->begin(), botsSnapshot->end(), [&](const std::shared_ptr<BotClient>& bot)
            {
                try {
                    if (bot->IsInCombat() || bot->IsPanicking())
                    {
                        bot->SetLOD(ExecutionLOD::ACTIVE_VIEWPORT);
                        return;
                    }

                    double botX = (double)bot->GetSpawnX();
                    double botY = (double)bot->GetSpawnY();
                    double botZ = (double)bot->GetSpawnZ();
                    PlayerObject* botPo = BotGetPlayer(bot->GetPlayerGoId());
                    if (botPo) {
                        botX = botPo->getPosition().x;
                        botY = botPo->getPosition().y;
                        botZ = botPo->getPosition().z;
                    }

                    double minDistSq = 999999999999.0;
                    for (const auto& hp : humanPositions)
                    {
                        double dx = hp.x - botX;
                        double dy = hp.y - botY;
                        double dz = hp.z - botZ;
                        double distSq = dx * dx + dy * dy + dz * dz;
                        if (distSq < minDistSq)
                        {
                            minDistSq = distSq;
                            if (minDistSq <= activeRadiusSq)
                                break;
                        }
                    }

                    ExecutionLOD targetLOD = ExecutionLOD::BACKGROUND_AREA;
                    if (minDistSq <= activeRadiusSq)
                        targetLOD = ExecutionLOD::ACTIVE_VIEWPORT;
                    else if (minDistSq <= approachRadiusSq)
                        targetLOD = ExecutionLOD::APPROACH_AREA;

                    bot->SetLOD(targetLOD);
                } catch (...) {}
            });
        }
    }

    // Filter only active bots requiring execution to eliminate 12,000-bot scheduler churn
    std::vector<std::shared_ptr<BotClient>> activeBots;
    activeBots.reserve(64);
    for (const auto& bot : *botsSnapshot)
    {
        if (bot && !bot->IsTester() && bot->GetLOD() != ExecutionLOD::BACKGROUND_AREA)
        {
            activeBots.push_back(bot);
        }
    }

    if (activeBots.empty())
    {
        return;
    }

    // Execute active bot updates directly if small, or parallelize across workers
    if (activeBots.size() <= 32)
    {
        for (auto& bot : activeBots)
        {
            ExecutionLOD targetLOD = bot->GetLOD();
            uint32 tickRate = (targetLOD == ExecutionLOD::ACTIVE_VIEWPORT) ? 100 : 500;
            if (tickRate > 0 && (now - bot->GetLastLodTick() >= tickRate))
            {
                float botDeltaSeconds = 0.033f;
                if (bot->GetLastLodTick() != 0) {
                    botDeltaSeconds = (now - bot->GetLastLodTick()) / 1000.0f;
                    if (botDeltaSeconds > 3.5f) botDeltaSeconds = 3.5f;
                }
                if (bot->GetPlayerGoId() != 0)
                {
                    bot->SetLastLodTick(now);
                    try {
                        bot->UpdateBotAI(botDeltaSeconds);
                    } catch (...) {}
                }
            }
        }
    }
    else
    {
        sTaskScheduler.ParallelFor(0, activeBots.size(), [&](size_t i)
        {
            auto bot = activeBots[i];
            if (!bot) return;

            ExecutionLOD targetLOD = bot->GetLOD();
            uint32 tickRate = (targetLOD == ExecutionLOD::ACTIVE_VIEWPORT) ? 100 : 500;
            if (tickRate > 0 && (now - bot->GetLastLodTick() >= tickRate))
            {
                float botDeltaSeconds = 0.033f;
                if (bot->GetLastLodTick() != 0) {
                    botDeltaSeconds = (now - bot->GetLastLodTick()) / 1000.0f;
                    if (botDeltaSeconds > 3.5f) botDeltaSeconds = 3.5f;
                }
                if (bot->GetPlayerGoId() != 0)
                {
                    bot->SetLastLodTick(now);
                    try {
                        bot->UpdateBotAI(botDeltaSeconds);
                    } catch (...) {}
                }
            }
        }, 16);
    }
}

void BotManager::PopulateWorld()
{
    const auto& npcs = sDataLoader.GetAllNPCs();
    size_t targetPop = (size_t)sConfig.GetIntDefault("BotManager.InitialPopulation", 350);
    targetPop = std::min(targetPop, (size_t)MAX_BOT_POPULATION_CEILING);
    INFO_LOG(format("BotManager: Populating world with authentic NPC spawn points (Target: %1%, Ceiling: %2%, Available: %3%)...")
        % targetPop % MAX_BOT_POPULATION_CEILING % npcs.size());

    std::vector<std::shared_ptr<BotClient>> newBots;
    newBots.reserve(std::min(targetPop, npcs.size()));

    int spawnCount = 0;
    for (auto it = npcs.begin(); it != npcs.end(); ++it)
    {
        if (newBots.size() >= targetPop) {
            break;
        }
        const NPCTemplate& templ = it->second;
        
        int factionId = FACTION_ZION;
        if (templ.faction == "Machines") {
            factionId = FACTION_MACHINES;
        } else if (templ.faction == "Merovingian") {
            factionId = FACTION_MEROVINGIAN;
        } else if (templ.faction == "Zion") {
            factionId = FACTION_ZION;
        } else {
            factionId = FACTION_ZION; // Civilians use Zion mechanics (neutral to player by default)
        }

        uint64 charId = findOrCreateBotCharacter(templ.npcId, templ.x, templ.y, templ.z, factionId);
        
        std::shared_ptr<BotClient> bot = std::make_shared<BotClient>(charId);
        bot->SetFaction((mxoFaction)factionId);
        bot->SetSpawnLocation(templ.x, templ.y, templ.z);
        bot->MoveTo(templ.x, templ.y, templ.z);

        std::string nameLower = templ.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find("agent") != std::string::npos) {
            bot->setAgent(true);
        }

        PlayerObject* po = sObjMgr.getGOPtr(bot->GetPlayerGoId());
        if (po) {
            po->setHandle(templ.name);
            po->setFactionName(templ.faction);
            po->setLevel(templ.level);
            po->setCurrentHealth(templ.health);
            po->setMaximumHealth(templ.health);
            po->setInnerStrength(templ.innerStrength, templ.innerStrength);

            LocationVector loc;
            loc.x = templ.x; loc.y = templ.y; loc.z = templ.z; loc.rot = templ.rot;
            po->setPosition(loc);

            // Apply authentic RSI appearance
            po->setRsiHex(templ.rsiHex);
            
            // Give weapon if template specified or fallback
            if (!templ.weaponHex.empty()) {
                po->giveItem(500);
            }
            
            // Phase 3: Enforce Discipline and Ability Loadout on spawn
            if (po->getAbilitySystem()) {
                DisciplineType botDisc = DisciplineType::NONE;
                if (templ.faction == "Merovingian") botDisc = DisciplineType::CODER;
                else if (templ.faction == "Zion") botDisc = DisciplineType::HACKER;
                else if (templ.faction == "Machines") botDisc = DisciplineType::OPERATIVE;
                
                if (botDisc != DisciplineType::NONE) {
                    const auto& allAbs = sDataLoader.GetAllAbilities();
                    uint16 slot = 1;
                    for (const auto& pair : allAbs) {
                        if (pair.second.discipline == botDisc && pair.second.maxLevel <= po->getLevel()) {
                            po->getAbilitySystem()->loadAbility(pair.first, pair.second.maxLevel, slot);
                            slot++;
                            if (slot > 6) break; // Memory limit for bots
                        }
                    }
                }
            }
            if (bot->isAgent()) {
                po->setAgentAppearance();
            }
            po->SpawnSelf();
        }

        newBots.push_back(bot);
        spawnCount++;
        if (spawnCount % 3000 == 0) {
            INFO_LOG(format("BotManager: Populated %1% / %2% NPCs...") % spawnCount % npcs.size());
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        m_bots.reserve(m_bots.size() + newBots.size());
        m_bots.insert(m_bots.end(), newBots.begin(), newBots.end());
        m_botsDirty = true;
    }

    INFO_LOG(format("BotManager: Successfully populated world with %1% authentic bots.") % spawnCount);
}

void BotManager::LogCombat(const std::string& msg)
{
    if (!m_combatLogging) return;
    std::ofstream logFile("combat_log.csv", std::ios_base::app);
    if (logFile.is_open())
    {
        logFile << getMSTime() << "," << msg << "\n";
    }
}

void BotManager::SpawnHighValueTarget(int district)
{
    // Find a random location near a known point for this district
    float baseX = 0.0f;
    float baseZ = 0.0f;
    
    if (district == 1) { baseX = 1000.0f; baseZ = 1000.0f; }
    else if (district == 2) { baseX = -1000.0f; baseZ = 500.0f; }

    // Spawn 1 Boss and 4 bodyguards
    SpawnBot(1, baseX, 0.0f, baseZ, 2); // Faction 2 = Agent/Machine
    SpawnBot(4, baseX + 10.0f, 0.0f, baseZ + 10.0f, 2); 

    // Faction broadcast (Machine/Agent faction)
    std::string msg = (format("{c:0000FF}[Faction] System: High-Value Target spawned in District %1%. Eliminate all Exile interference.{/c}") % district).str();
    DEBUG_LOG(msg);
}

void BotManager::SpawnFactionDefenders(uint8 district, uint32 hlId, uint8 faction, LocationVector loc)
{
    // Item 30: Spawn 3 defenders around the hardline
    // We add an offset so they don't spawn exactly on the hardline model
    SpawnBot(3, loc.x + 200.0f, loc.y, loc.z + 200.0f, faction);

    std::string factionName = "Unknown Faction";
    if (faction == FACTION_ZION || faction == 1) factionName = "Zion";
    else if (faction == FACTION_MACHINES || faction == 2) factionName = "Machines";
    else if (faction == FACTION_MEROVINGIAN || faction == 3) factionName = "Merovingian";
    else if (faction == 0) factionName = "Machines";

    std::string msg = (format("{c:00FF00}[Faction Warfare] %1% has deployed defenders to Hardline %2% in District %3%.{/c}") % factionName % hlId % (int)district).str();
    DEBUG_LOG(msg);
}

std::shared_ptr<BotClient> BotManager::GetBotByGOID(uint32 goid)
{
    std::lock_guard<std::recursive_mutex> lock(m_botMutex);
    for (auto& bot : m_bots)
    {
        if (bot->GetPlayerGoId() == goid)
            return bot;
    }
    return nullptr;
}

void BotManager::HandleCleanseAwakening(uint32 entityGoId)
{
    PlayerObject* po = BotGetPlayer(entityGoId);
    if (!po) return;

    auto bot = GetBotByGOID(entityGoId);
    if (!bot) return;

    LocationVector pos = po->getPosition();
    bool awakened = ((rand() % 100) < 15);

    if (awakened) {
        po->setHandle("Awakened_Redpill_Novice");
        po->setFactionName("Zion");
        bot->SetFaction(FACTION_ZION);
        po->setLevel(20);
        po->setMaximumHealth(2000);
        po->setCurrentHealth(1000);

        bot->Say("Awakened Redpill: I saw it... the green code behind the walls. Get me out of here.");
        LocationVector hl = GetNearestHardline((float)pos.x, (float)pos.z);
        if (hl.x != 0.0f || hl.z != 0.0f) {
            bot->SetEvacTarget(hl);
        }
        bot->SetPanicking(false);
        bot->SetFearLevel(0.05f);

        LogCombat((format("[AWAKENING] Rescued civilian entity %1% awakened into Redpill Novice aligned with Zion!") % entityGoId).str());
    } else {
        bot->Say("Civilian: What happened to me? Who was that man in the suit?");
        bot->SetPanicking(false);
        bot->SetFearLevel(0.20f);
        LocationVector hl = GetNearestHardline((float)pos.x, (float)pos.z);
        if (hl.x != 0.0f || hl.z != 0.0f) {
            bot->SetEvacTarget(hl);
        }
    }
}

void BotManager::DespawnBot(uint32 goId)
{
    if (goId == 0) return;
    std::lock_guard<std::recursive_mutex> lock(m_botMutex);
    m_pendingDespawn.push_back(goId);
}

void BotManager::ProcessPendingDespawns()
{
    std::vector<uint32> pending;
    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        pending.swap(m_pendingDespawn);
    }
    for (uint32 goId : pending)
    {
        std::shared_ptr<BotClient> victim;
        {
            std::lock_guard<std::recursive_mutex> lock(m_botMutex);
            for (auto it = m_bots.begin(); it != m_bots.end(); ++it)
            {
                if (*it && (*it)->GetPlayerGoId() == goId && !(*it)->IsTester())
                {
                    victim = *it;
                    m_bots.erase(it);
                    m_botsDirty = true;
                    break;
                }
            }
        }
        if (victim)
        {
            sCombatSys.RemoveCombatant(goId);
            sObjMgr.destroyObject(goId);
        }
    }
}

void BotManager::SpawnTesters()
{
    int count = sConfig.GetIntDefault("Bots.Testers", 6);
    if (count <= 0)
    {
        INFO_LOG("BOTTEST: tester bots disabled (Bots.Testers = 0)");
        return;
    }
    count = std::min(count, 50);
    for (int i = 0; i < count; i++)
    {
        LocationVector start = m_hardlines.empty() ? LocationVector(0.0f, 0.0f, 0.0f)
                                                   : m_hardlines[(i * 7) % m_hardlines.size()];
        SpawnTester(i + 1, start);
    }
}

std::shared_ptr<BotClient> BotManager::SpawnTester(int index, const LocationVector& start)
{
    static const char* factions[] = { "Zion", "Machines", "Merovingian" };
    static const mxoFaction factionIds[] = { FACTION_ZION, FACTION_MACHINES, FACTION_MEROVINGIAN };
    uint32 startCash = (uint32)sConfig.GetIntDefault("Bots.TesterStartInfo", 500);
    int i = index - 1;
    {
        uint64 uid = findOrCreateBotCharacter(int(++m_nextBotId), start.x, start.y, start.z, factionIds[i % 3]);
        std::shared_ptr<BotClient> bot = std::make_shared<BotClient>(uid);
        PlayerObject* po = sObjMgr.getGOPtr(bot->GetPlayerGoId());
        if (!po)
        {
            ERROR_LOG(format("BOTTEST: could not construct tester %1%") % (i + 1));
            return nullptr;
        }
        char handle[32];
        snprintf(handle, sizeof(handle), "Tester_%02d", i + 1);
        bot->SetFaction(factionIds[i % 3]);
        bot->SetSpawnLocation(start.x, start.y, start.z);
        po->setHandle(handle);
        po->setFactionName(factions[i % 3]);
        po->setLevel(1);
        po->setPosition(start);
        po->addInformation(startCash);
        po->ensureAbilitySystem();
        sSpatialGrid.UpdateClientPosition(bot.get(), start.x, start.z);
        po->SpawnSelf();
        bot->MakeTester(i + 1);
        {
            std::lock_guard<std::recursive_mutex> lock(m_botMutex);
            m_bots.push_back(bot);
            m_testers.push_back(bot);
            m_botsDirty = true;
        }
        INFO_LOG(format("BOTTEST: spawned %1% (%2%) goId %3% at %4%,%5%,%6%")
            % handle % factions[i % 3] % bot->GetPlayerGoId() % start.x % start.y % start.z);
        return bot;
    }
}

std::string BotManager::GetTesterSummary()
{
    std::vector<std::shared_ptr<BotClient>> testers;
    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        testers = m_testers;
    }
    std::string out;
    for (auto& t : testers)
        if (t && t->GetTester())
            out += t->GetTester()->ReportLine() + "\n";
    if (out.empty()) out = "no tester bots running (Bots.Testers = 0?)\n";
    return out;
}

void BotManager::WriteTesterReport()
{
    std::vector<std::shared_ptr<BotClient>> testers;
    {
        std::lock_guard<std::recursive_mutex> lock(m_botMutex);
        testers = m_testers;
    }
    std::string json = "{\n  \"generatedMs\": " + std::to_string(getMSTime()) + ",\n  \"testers\": [\n";
    bool first = true;
    for (auto& t : testers)
    {
        if (!t || !t->GetTester()) continue;
        INFO_LOG("BOTTEST " + t->GetTester()->ReportLine());
        if (!first) json += ",\n";
        json += t->GetTester()->ReportJson();
        first = false;
    }
    json += "\n  ],\n  \"findings\": " + TesterBrain::FindingsJson() + "\n}\n";
    std::ofstream f("bot_test_report.json", std::ios::trunc);
    if (f.is_open()) f << json;
}

void BotManager::PruneDeadBots()
{
    std::lock_guard<std::recursive_mutex> lock(m_botMutex);
    size_t before = m_bots.size();
    m_bots.erase(
        std::remove_if(m_bots.begin(), m_bots.end(), [](const std::shared_ptr<BotClient>& b) {
            if (!b) return true;
            uint32 goId = b->GetPlayerGoId();
            if (goId == 0) return true;
            PlayerObject* po = sObjMgr.getGOPtrSafe(goId);
            return (po == nullptr);
        }),
        m_bots.end()
    );
    if (m_bots.size() != before) {
        m_botsDirty = true;
    }
}

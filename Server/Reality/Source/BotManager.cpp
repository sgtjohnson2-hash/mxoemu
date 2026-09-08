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
        if (m_bots.size() >= MAX_BOT_POPULATION_CEILING && !m_bots.empty()) {
            size_t idx = (m_recycleBotIndex++) % m_bots.size();
            auto recycledBot = m_bots[idx];
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
    bot->MoveTo(x, y, z);
    
    // Give bot a mock ranged weapon (e.g., Template ID 500 = SMG)
    PlayerObject* po = sObjMgr.getGOPtr(bot->GetPlayerGoId());
    if (po) {
        po->giveItem(500); // 500 is just a mock template ID for now
        
        std::string factionName = "Civilian";
        if (faction == FACTION_ZION) factionName = "Zion";
        else if (faction == FACTION_MACHINES) {
            factionName = "Machines";
            if ((rand() % 100) < 5) { // 5% chance
                bot->setAgent(true);
                po->setHandle("Agent Simulacra");
            }
        }
        else if (faction == FACTION_MEROVINGIAN) factionName = "Merovingian";
        po->setFactionName(factionName);
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
    
    PlayerObject* po = sObjMgr.getGOPtr(bot->GetPlayerGoId());
    if (po) {
        po->setHandle(npcInfo.handle);
        po->getClient().m_instanceId = instanceId;
        po->setFactionName(npcInfo.type);
        // npcInfo.idNpc could be stored if we added an idNpc field to PlayerObject, but for now we rely on the handle or instanceId
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



    // Spawn ambient pedestrian traffic every 30 seconds
    if (now - m_lastTrafficTickMS > 30000)
    {
        m_lastTrafficTickMS = now;
        
        // Find active players and spawn a neutral pedestrian near them if they are alone
        std::vector<PlayerObject*> activePlayers;
        auto goIds = sObjMgr.getAllGOIds();
        for (uint32 id : goIds)
        {
            PlayerObject* p = sObjMgr.getGOPtr(id);
            if (p && p->getCharacterUID() < 9000000) // 9000000+ are bot UIDs
            {
                activePlayers.push_back(p);
            }
        }

        for (PlayerObject* p : activePlayers)
        {
            // Spawn a random pedestrian nearby (radius 20)
            float rx = p->getPosition().x + ((rand() % 40) - 20);
            float ry = p->getPosition().y;
            float rz = p->getPosition().z + ((rand() % 40) - 20);
            
            SpawnBot(1, rx, ry, rz, 1); // 1 = FACTION_MACHINES/Neutral Pedestrian
        }
    }

    // Collect all active players (cache updated every 2 seconds)
    if (now - m_lastPlayerCacheTickMS > 2000)
    {
        m_activePlayerIds.clear();
        auto goIds = sObjMgr.getAllGOIds();
        for (uint32 id : goIds)
        {
            PlayerObject* p = sObjMgr.getGOPtr(id);
            if (p && p->getCharacterUID() < 9000000)
            {
                m_activePlayerIds.push_back(id);
            }
        }
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

    // Fast-path: When no human players are connected, assign background LOD without thread pool overhead
    if (m_activePlayerIds.empty())
    {
        for (const auto& bot : *botsSnapshot)
        {
            if (bot->IsInCombat() || bot->IsPanicking())
                bot->SetLOD(ExecutionLOD::APPROACH_AREA);
            else
                bot->SetLOD(ExecutionLOD::BACKGROUND_AREA);
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
            for (const auto& bot : *botsSnapshot)
            {
                if (bot->IsInCombat() || bot->IsPanicking())
                    bot->SetLOD(ExecutionLOD::APPROACH_AREA);
                else
                    bot->SetLOD(ExecutionLOD::BACKGROUND_AREA);
            }
        }
        else
        {
            const double activeRadiusSq = 5000.0 * 5000.0;     // 50m
            const double approachRadiusSq = 20000.0 * 20000.0; // 200m

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

    // Sequential bot update execution
    for (size_t i = 0; i < botsSnapshot->size(); i++)
    {
        auto bot = (*botsSnapshot)[i];
        ExecutionLOD targetLOD = bot->GetLOD();
        uint32 tickRate = 0;
        if (targetLOD == ExecutionLOD::ACTIVE_VIEWPORT)
            tickRate = 250;
        else if (targetLOD == ExecutionLOD::APPROACH_AREA)
            tickRate = 1000;
        // BACKGROUND_AREA: 0Hz (skip logic per ExecutionLOD specification)

        if (tickRate > 0 && (now - bot->GetLastLodTick() >= tickRate))
        {
            float botDeltaSeconds = 0.033f;
            if (bot->GetLastLodTick() != 0) {
                botDeltaSeconds = (now - bot->GetLastLodTick()) / 1000.0f;
                if (botDeltaSeconds > 3.5f) botDeltaSeconds = 3.5f;
            }

            if (bot && bot->GetPlayerGoId() != 0)
            {
                bot->SetLastLodTick(now);
                try {
                    bot->UpdateBotAI(botDeltaSeconds);
                } catch(const std::exception& e) {
                    std::cout << "CRASH inside UpdateBotAI for bot index " << i << ": " << e.what() << std::endl;
                } catch(...) {
                    std::cout << "UNKNOWN CRASH inside UpdateBotAI for bot index " << i << std::endl;
                }
            }
        }
    }
}

void BotManager::PopulateWorld()
{
    const auto& npcs = sDataLoader.GetAllNPCs();
    INFO_LOG(format("BotManager: Populating world with %1% authentic NPC spawn points...") % npcs.size());

    std::vector<std::shared_ptr<BotClient>> newBots;
    newBots.reserve(npcs.size());

    int spawnCount = 0;
    for (auto it = npcs.begin(); it != npcs.end(); ++it)
    {
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
    
    auto players = sObjMgr.getAllGOIds();
    for (auto id : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(id);
        if (p && p->getFactionName() == "Machines") {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(msg));
        }
    }
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
    sGame.Broadcast(std::make_shared<SystemChatMsg>(msg)->toBuf(), false);
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




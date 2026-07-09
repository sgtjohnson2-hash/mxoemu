#include "BotManager.h"
#include "DataLoader.h"
#include "GameServer.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "BehaviorTree.h"
#include "Database/Database.h"
#include "Timer.h"
#include "PlayerObject.h"
#include <fstream>

createFileSingleton(BotManager);

BotManager::BotManager()
{
    m_nextBotId = 0; //bots are backed by real character rows (TestBotN)
    m_lastAiTickMS = 0;
    m_lastTrafficTickMS = 0;
    m_aggroEnabled = false;
    m_combatLogging = false;

    // Build shared behavior tree
    std::shared_ptr<SelectorNode> root = std::make_shared<SelectorNode>();
    
    std::shared_ptr<SequenceNode> combatSeq = std::make_shared<SequenceNode>();
    combatSeq->AddChild(std::make_shared<ActionEngageTarget>());
    
    // Create a selector for combat actions (Flee vs Fight)
    std::shared_ptr<SelectorNode> combatActions = std::make_shared<SelectorNode>();
    combatActions->AddChild(std::make_shared<ActionFlee>());
    combatActions->AddChild(std::make_shared<ActionCastAbility>());
    combatActions->AddChild(std::make_shared<ActionCombatCycle>());
    combatSeq->AddChild(combatActions);

    std::shared_ptr<SequenceNode> idleSeq = std::make_shared<SequenceNode>();
    idleSeq->AddChild(std::make_shared<ActionFindTarget>());
    
    root->AddChild(std::make_shared<ActionLeash>());
    root->AddChild(combatSeq);
    root->AddChild(idleSeq);
    root->AddChild(std::make_shared<ActionRoam>());
    
    m_sharedBehaviorTree = root;
}

BotManager::~BotManager()
{
    m_bots.clear();
}

// Bots are backed by real character rows so the normal PlayerObject DB load
// works for them. Rows are created on demand and reused between runs.
uint64 BotManager::findOrCreateBotCharacter(int botNumber, float x, float y, float z, int faction)
{
    // Bypassed: Bots are now completely virtual and live in memory.
    // Return a dummy UID starting from 9000000.
    return 9000000 + botNumber;
}

void BotManager::SpawnBot(int count, float x, float y, float z, int faction)
{
    for (int i = 0; i < count; i++)
    {
        uint64 uid = findOrCreateBotCharacter(int(++m_nextBotId), x, y, z, faction);
        if (uid == 0)
        {
            ERROR_LOG("BotManager: could not create bot character");
            continue;
        }
        std::shared_ptr<BotClient> bot = std::make_shared<BotClient>(uid);
        bot->SetFaction((mxoFaction)faction);
        bot->SetBehaviorTree(m_sharedBehaviorTree);
        bot->MoveTo(x, y, z);
        m_bots.push_back(bot);
        DEBUG_LOG(format("BotManager: Spawned bot UID %1% (Faction %5%) at %2%, %3%, %4%") % uid % x % y % z % faction);
    }
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
    for (size_t i = 0; i < m_bots.size(); i++)
    {
        m_bots[i]->AttackTarget(targetGoId);
    }
    DEBUG_LOG(format("BotManager: Commanded %1% bots to attack %2%") % m_bots.size() % targetName);
}

void BotManager::BotStressTest(int count)
{
    SpawnBot(count, 0.0f, 0.0f, 0.0f);
    DEBUG_LOG(format("BotManager: Stress test started with %1% bots") % count);
}

void BotManager::Update()
{
    uint32 now = getMSTime();

    // Spawn ambient pedestrian traffic every 30 seconds
    if (now - m_lastTrafficTickMS > 30000)
    {
        m_lastTrafficTickMS = now;
        // Pedestrian spawner logic can be implemented here later
    }

    // Collect all active players
    std::vector<PlayerObject*> activePlayers;
    auto goIds = sObjMgr.getAllGOIds();
    for (uint32 id : goIds)
    {
        PlayerObject* p = sObjMgr.getGOPtr(id);
        if (p && p->getHandle().find("Bot_") != 0)
        {
            activePlayers.push_back(p);
        }
    }

    for (size_t i = 0; i < m_bots.size(); i++)
    {
        auto bot = m_bots[i];

        double minDistSq = 999999999.0;
        for (PlayerObject* p : activePlayers)
        {
            double distSq = p->getPosition().DistanceSq(bot->GetSpawnX(), bot->GetSpawnY(), bot->GetSpawnZ());
            if (distSq < minDistSq)
                minDistSq = distSq;
        }

        // Apply Spatial LOD
        ExecutionLOD targetLOD = ExecutionLOD::BACKGROUND_AREA;
        
        // Treat as ACTIVE if no players are logged in (for testing) or if players are nearby
        if (activePlayers.empty() || minDistSq <= 50.0 * 50.0)
        {
            targetLOD = ExecutionLOD::ACTIVE_VIEWPORT;
        }
        else if (minDistSq <= 200.0 * 200.0)
        {
            targetLOD = ExecutionLOD::APPROACH_AREA;
        }

        bot->SetLOD(targetLOD);

        uint32 tickRate = 0;
        if (targetLOD == ExecutionLOD::ACTIVE_VIEWPORT)
            tickRate = 250;
        else if (targetLOD == ExecutionLOD::APPROACH_AREA)
            tickRate = 1000;

        if (tickRate > 0 && (now - bot->GetLastLodTick() >= tickRate))
        {
            bot->SetLastLodTick(now);
            bot->UpdateBotAI();
        }
    }
}

void BotManager::PopulateWorld()
{
    const auto& npcs = sDataLoader.GetAllNPCs();
    INFO_LOG(format("BotManager: Populating world with %1% authentic NPC spawn points...") % npcs.size());

    int spawnCount = 0;
    for (auto it = npcs.begin(); it != npcs.end(); ++it)
    {
        const NPCTemplate& templ = it->second;
        // Map faction correctly based on authentic XML later, assume Zion for now
        uint64 charId = findOrCreateBotCharacter(templ.npcId, templ.x, templ.y, templ.z, FACTION_ZION);
        
        std::shared_ptr<BotClient> bot = std::make_shared<BotClient>(charId);
        bot->SetBehaviorTree(m_sharedBehaviorTree);

        // Bots are already added to ObjMgr by findOrCreateBotCharacter, just track them here
        m_bots.push_back(bot);
        spawnCount++;
    }
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

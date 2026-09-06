#include "WorldDirector.h"
#include "Log.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include "WeatherSystem.h"
#include "GameServer.h"
#include "FactionWarManager.h"
#include "AI/PedestrianEcology.h"
#include "AI/MatrixThreatHeatmap.h"
#include "HovercraftFlightSystem.h"
#include "LoadingConstruct.h"
#include "RadioDispatchSystem.h"
#include "APUCombatSystem.h"
#include "MegacityDestructionEngine.h"
#include "RedpillAwakeningSystem.h"
#include "NeuralNarrativeEngine.h"
#include "MachineCitySystem.h"
#include "FreewayCombatSystem.h"
#include "MobilAveRailSystem.h"
#include "CyberdeckHackingSystem.h"
#include "ShardFederationEngine.h"
#include "ClubHelRaidSystem.h"
#include "PodHarvestSystem.h"
#include "OrbitalSatelliteSystem.h"
#include "SourceCodeCompilerSystem.h"
#include "MatrixRebootEngine.h"
#include <cmath>
#include <algorithm>

createFileSingleton(WorldDirector);

WorldDirector::WorldDirector()
    : m_timeSinceLastCrisisCheck(0)
{
}

WorldDirector::~WorldDirector()
{
}

void WorldDirector::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_reputationMutex);
    m_reputations.clear();
    m_activeCrisis.reset();
    m_timeSinceLastCrisisCheck = 0;
    INFO_LOG("The Oracle World Director Initialized.");
}

CharacterReputation WorldDirector::GetReputation(uint64 charUID, const std::string& handle)
{
    std::lock_guard<std::recursive_mutex> lock(m_reputationMutex);
    auto it = m_reputations.find(charUID);
    if (it == m_reputations.end()) {
        CharacterReputation rep;
        rep.charUID = charUID;
        rep.handle = handle;
        rep.machineStanding = 0.0f;
        rep.zionStanding = 10.0f; // Default rebel sympathy
        rep.meroStanding = 0.0f;
        rep.notoriety = 0;
        rep.lastUpdatedMs = getMSTime();
        m_reputations[charUID] = rep;
        return rep;
    }
    if (!handle.empty()) it->second.handle = handle;
    return it->second;
}

void WorldDirector::RecordAgentDefeated(uint64 charUID, const std::string& handle)
{
    std::lock_guard<std::recursive_mutex> lock(m_reputationMutex);
    CharacterReputation& rep = m_reputations[charUID];
    rep.charUID = charUID;
    if (!handle.empty()) rep.handle = handle;
    rep.agentsDefeated++;
    rep.zionStanding = std::min(100.0f, rep.zionStanding + 25.0f);
    rep.meroStanding = std::min(100.0f, rep.meroStanding + 15.0f);
    rep.machineStanding = std::max(-100.0f, rep.machineStanding - 35.0f);
    rep.notoriety = std::min<uint32>(1000, rep.notoriety + 40);
    rep.lastUpdatedMs = getMSTime();

    INFO_LOG(format("WorldDirector: [Living History] %1% defeated an Agent! Notoriety: %2%, Zion: %3%") 
             % handle % rep.notoriety % rep.zionStanding);
}

void WorldDirector::RecordHardlineCaptured(uint64 charUID, const std::string& handle, uint32 faction)
{
    std::lock_guard<std::recursive_mutex> lock(m_reputationMutex);
    CharacterReputation& rep = m_reputations[charUID];
    rep.charUID = charUID;
    if (!handle.empty()) rep.handle = handle;
    rep.hardlinesCaptured++;
    if (faction == FACTION_ZION) {
        rep.zionStanding = std::min(100.0f, rep.zionStanding + 20.0f);
        rep.machineStanding = std::max(-100.0f, rep.machineStanding - 20.0f);
    } else if (faction == FACTION_MACHINES) {
        rep.machineStanding = std::min(100.0f, rep.machineStanding + 20.0f);
        rep.zionStanding = std::max(-100.0f, rep.zionStanding - 20.0f);
    } else {
        rep.meroStanding = std::min(100.0f, rep.meroStanding + 20.0f);
    }
    rep.notoriety = std::min<uint32>(1000, rep.notoriety + 25);
    rep.lastUpdatedMs = getMSTime();

    INFO_LOG(format("WorldDirector: [Living History] %1% captured a hardline! Notoriety: %2%") 
             % handle % rep.notoriety);
}

void WorldDirector::RecordCourierAmbushed(uint64 charUID, const std::string& handle)
{
    std::lock_guard<std::recursive_mutex> lock(m_reputationMutex);
    CharacterReputation& rep = m_reputations[charUID];
    rep.charUID = charUID;
    if (!handle.empty()) rep.handle = handle;
    rep.couriersAmbushed++;
    rep.meroStanding = std::min(100.0f, rep.meroStanding + 15.0f);
    rep.notoriety = std::min<uint32>(1000, rep.notoriety + 20);
    rep.lastUpdatedMs = getMSTime();
}

void WorldDirector::PropagatePlayerDeedGossip(const std::string& deedSummary, float x, float z)
{
    uint32 now = getMSTime();
    auto clients = sSpatialGrid.GetClientsInRadius(x, z);

    for (GameClient* gc : clients) {
        if (!gc->isBot()) continue;
        BotClient* bot = dynamic_cast<BotClient*>(gc);
        if (bot) {
            MemoryNode mem;
            mem.text = deedSummary;
            mem.importance = 0.8f;
            mem.timestamp = double(now);
            bot->GetMemoryStreamCuller().AddMemory(mem);
        }
    }
}

void WorldDirector::EvaluatePlayerProximityReactions(uint32 playerGoId, float px, float pz)
{
    PlayerObject* p = sObjMgr.getGOPtrSafe(playerGoId);
    if (!p || p->getClient().isBot()) return;

    uint64 charUID = p->getClient().GetCharacterId();
    const CharacterReputation& rep = GetReputation(charUID, p->getHandle());

    if (rep.notoriety < 80) return; // Not famous enough yet

    auto nearby = sSpatialGrid.GetClientsInRadius(px, pz);
    for (GameClient* gc : nearby) {
        if (!gc->isBot()) continue;
        BotClient* bot = dynamic_cast<BotClient*>(gc);
        if (!bot) continue;

        PlayerObject* botPo = BotGetPlayer(bot->GetPlayerGoId());
        if (!botPo || botPo->getFactionName() != "Civilian") continue;

        float distSq = std::pow(botPo->getPosition().x - px, 2) + std::pow(botPo->getPosition().z - pz, 2);
        if (distSq <= 1000000.0f) { // Within 10m
            if (rand() % 100 < 8) { // 8% chance to react
                if (rep.zionStanding > 40.0f) {
                    bot->Say((format("Look, that's %1%... the redpill who fought the Agents!") % p->getHandle()).str());
                } else if (rep.machineStanding > 40.0f) {
                    bot->Say((format("Careful... %1% has Machine security clearance.") % p->getHandle()).str());
                } else if (rep.notoriety > 300) {
                    bot->Say((format("Don't make eye contact with %1%... they're dangerous.") % p->getHandle()).str());
                }
                break;
            }
        }
    }
}

void WorldDirector::Update(uint32 deltaMs)
{
    uint32 now = getMSTime();
    float deltaSec = (float)deltaMs * 0.001f;
    sHovercraftFlightSys.UpdateSimulation(deltaSec);
    sLoadingConstruct.UpdateTimeDilation(deltaSec);
    sAPUCombatSystem.UpdateSimulation(deltaSec);
    sMegacityDestructionEngine.UpdateSimulation(deltaSec);
    sRedpillAwakeningSystem.UpdateSimulation(deltaSec);
    sNeuralNarrativeEngine.UpdateSimulation(deltaSec);
    sMachineCitySystem.UpdateSimulation(deltaSec);
    sFreewayCombatSystem.UpdateSimulation(deltaSec);
    sMobilAveRailSystem.UpdateSimulation(deltaSec);
    sCyberdeckHackingSystem.UpdateSimulation(deltaSec);
    sShardFederationEngine.UpdateSimulation(deltaSec);
    sClubHelRaidSystem.UpdateSimulation(deltaSec);
    sPodHarvestSystem.UpdateSimulation(deltaSec);
    sOrbitalSatelliteSystem.UpdateSimulation(deltaSec);
    sSourceCodeCompilerSystem.UpdateSimulation(deltaSec);
    sMatrixRebootEngine.UpdateSimulation(deltaSec);

    // 1. Check Active Crisis Expiration
    if (m_activeCrisis) {
        if (now - m_activeCrisis->startTimeMs >= m_activeCrisis->durationMs) {
            ResolveCrisis();
        }
    } else {
        // 2. Schedule Dynamic World Crises (Every 10 minutes)
        m_timeSinceLastCrisisCheck += deltaMs;
        if (m_timeSinceLastCrisisCheck >= 600000) {
            m_timeSinceLastCrisisCheck = 0;
            // Roll for dynamic crisis
            int roll = 1 + (rand() % 4);
            TriggerCrisis((WorldCrisisType)roll);
        }
    }

    // 3. Evaluate Living History Proximity Reactions for Online Players
    static uint32 lastPlayerCheckMs = 0;
    if (now - lastPlayerCheckMs >= 5000) {
        lastPlayerCheckMs = now;
        auto players = sObjMgr.getAllGOIds();
        for (auto goId : players) {
            PlayerObject* po = sObjMgr.getGOPtrSafe(goId);
            if (po && !po->getClient().isBot()) {
                EvaluatePlayerProximityReactions(goId, po->getPosition().x, po->getPosition().z);
            }
        }
    }
}

void WorldDirector::TriggerCrisis(WorldCrisisType type)
{
    if (m_activeCrisis) return; // One crisis at a time

    switch (type) {
        case CRISIS_HARDLINE_COLLAPSE:
            StartHardlineCollapseCrisis();
            break;
        case CRISIS_SUBWAY_AMBUSH:
            StartSubwayAmbushCrisis();
            break;
        case CRISIS_EXILE_TURF_WAR:
            StartExileTurfWarCrisis();
            break;
        case CRISIS_ANOMALY_CASCADE:
            StartAnomalyCascadeCrisis();
            break;
        default:
            break;
    }
}

void WorldDirector::StartHardlineCollapseCrisis()
{
    LocationVector hl = sBotMgr.GetRandomHardline();

    ActiveCrisis crisis;
    crisis.type = CRISIS_HARDLINE_COLLAPSE;
    crisis.title = "Hardline Code Rupture";
    crisis.description = "A strategic hardline has suffered code integrity decay and is destabilizing!";
    crisis.targetDistrictId = 1;
    crisis.location = hl;
    crisis.startTimeMs = getMSTime();
    crisis.durationMs = 300000; // 5 minutes
    crisis.resolved = false;

    // Spawn 3 Glitched Corrupted Redpills around the hardline
    for (int i = 0; i < 3; ++i) {
        float offX = (i == 0) ? 500.0f : (i == 1 ? -500.0f : 0.0f);
        float offZ = (i == 2) ? 500.0f : -300.0f;
        auto bot = sBotMgr.SpawnSingleBot(hl.x + offX, hl.y, hl.z + offZ, FACTION_MACHINES);
        if (bot) {
            PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
            if (po) {
                po->setHandle("Corrupted_Redpill");
                po->setLevel(48);
            }
            crisis.spawnedEntityGoIds.push_back(bot->GetPlayerGoId());
        }
    }

    m_activeCrisis = crisis;

    // The Oracle speaks
    std::string broadcastMsg = (format("{c:00FF00}[The Oracle] The thread unweaves at coordinates (%1%, %2%). Stabilize the hardline before the doorway collapses!{/c}") 
                                % (int)hl.x % (int)hl.z).str();
    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
        }
    }

    INFO_LOG(format("WorldDirector: Started Crisis [Hardline Collapse] at (%1%, %2%)") % hl.x % hl.z);
}

void WorldDirector::StartSubwayAmbushCrisis()
{
    PointOfInterest subway = sPedestrianEcology.GetNearestSubway(7737.0f, 13801.0f); // Downtown Metro

    ActiveCrisis crisis;
    crisis.type = CRISIS_SUBWAY_AMBUSH;
    crisis.title = "Subway Terminal Lockdown";
    crisis.description = "Agents have established a security perimeter at the subway station!";
    crisis.targetDistrictId = 2;
    crisis.location = LocationVector(subway.x, subway.y, subway.z);
    crisis.startTimeMs = getMSTime();
    crisis.durationMs = 300000;
    crisis.resolved = false;

    // Spawn 2 elite Agents
    for (int i = 0; i < 2; ++i) {
        float off = (i == 0) ? 600.0f : -600.0f;
        auto bot = sBotMgr.SpawnSingleBot(subway.x + off, subway.y, subway.z, FACTION_MACHINES);
        if (bot) {
            bot->setAgent(true);
            PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
            if (po) {
                po->setHandle(i == 0 ? "Agent_Smith" : "Agent_Brown");
                po->setRsiHex("6e060040");
                po->setLevel(50);
                po->setMaximumHealth(4500);
                po->setCurrentHealth(4500);
            }
            crisis.spawnedEntityGoIds.push_back(bot->GetPlayerGoId());
        }
    }

    m_activeCrisis = crisis;

    std::string broadcastMsg = (format("{c:FF0000}[System Directive] Security protocol initiated: %1% is under federal lockdown. Subroutine scan in progress.{/c}") 
                                % subway.name).str();
    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
        }
    }

    INFO_LOG(format("WorldDirector: Started Crisis [Subway Ambush] at %1%") % subway.name);
}

void WorldDirector::StartExileTurfWarCrisis()
{
    LocationVector loc(82745.0f, 695.0f, -66760.0f); // International District

    ActiveCrisis crisis;
    crisis.type = CRISIS_EXILE_TURF_WAR;
    crisis.title = "Exile Syndicate Offensive";
    crisis.description = "The Merovingian has unleashed rogue Exiles to claim street influence!";
    crisis.targetDistrictId = 3;
    crisis.location = loc;
    crisis.startTimeMs = getMSTime();
    crisis.durationMs = 300000;
    crisis.resolved = false;

    // Spawn 3 Merovingian Enforcers
    for (int i = 0; i < 3; ++i) {
        float offX = (i - 1) * 700.0f;
        auto bot = sBotMgr.SpawnSingleBot(loc.x + offX, loc.y, loc.z, FACTION_MEROVINGIAN);
        if (bot) {
            PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
            if (po) {
                po->setHandle("Exile_Syndicate_Vampire");
                po->setLevel(50);
                po->setMaximumHealth(3200);
                po->setCurrentHealth(3200);
            }
            crisis.spawnedEntityGoIds.push_back(bot->GetPlayerGoId());
        }
    }

    m_activeCrisis = crisis;

    std::string broadcastMsg = "{c:FF8800}[The Merovingian] Ah, causality in motion. The streets belong to those with the courage to seize them.{/c}";
    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
        }
    }

    INFO_LOG("WorldDirector: Started Crisis [Exile Turf War]");
}

void WorldDirector::StartAnomalyCascadeCrisis()
{
    LocationVector loc(13211.0f, 95.0f, -37821.0f); // Downtown Park

    ActiveCrisis crisis;
    crisis.type = CRISIS_ANOMALY_CASCADE;
    crisis.title = "Source Code Fragment Descent";
    crisis.description = "A golden encrypted code fragment has materialized in the Megacity!";
    crisis.targetDistrictId = 2;
    crisis.location = loc;
    crisis.startTimeMs = getMSTime();
    crisis.durationMs = 240000; // 4 minutes
    crisis.resolved = false;

    // Disruption spike on heatmap
    sMatrixThreatHeatmap.RecordDisruption(loc.x, loc.z, 75.0f, "Source Code Anomaly");

    m_activeCrisis = crisis;

    std::string broadcastMsg = "{c:FFFF00}[Radio Free Zion] SENSORS DETECT AN UNBOUND CODE FRAGMENT! 3-way faction race underway in Downtown!{/c}";
    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
        }
    }

    INFO_LOG("WorldDirector: Started Crisis [Source Code Anomaly]");
}

void WorldDirector::ResolveCrisis()
{
    if (!m_activeCrisis) return;

    std::string title = m_activeCrisis->title;
    std::string resolveMsg = (format("{c:00FF00}[The Oracle] The crisis '%1%' has concluded. The equation balances once again.{/c}") 
                              % title).str();

    // Clean up surviving spawned entities from the crisis
    for (uint32 botGoId : m_activeCrisis->spawnedEntityGoIds) {
        PlayerObject* po = sObjMgr.getGOPtrSafe(botGoId);
        if (po) {
            sObjMgr.QueueDeletion(botGoId);
        }
    }

    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(resolveMsg));
        }
    }

    INFO_LOG(format("WorldDirector: Crisis [%1%] resolved.") % title);
    m_activeCrisis.reset();
}

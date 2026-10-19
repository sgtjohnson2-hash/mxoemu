#include "Item.h"
#include "FactionWarManager.h"
#include "Log.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"
#include "MessageTypes.h"
#include "BotClient.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameServer.h"
#include "SpatialGrid.h"
#include "Config.h"

// Hardline capture, district territory buffs, faction strike squads, "Radio Free Zion" broadcasts and
// the 15-minute world-event director are emulator inventions; retail MxO had none of them (hardlines
// were travel and jack-out points). World.FactionWar = 1 turns the system back on.
static bool FactionWarEnabled() { return sConfig.GetBoolDefault("World.FactionWar", false); }
#include "BotManager.h"
#include "CombatSystem.h"
#include "HackerSystem.h"
#include "DataLoader.h"
#include "BehaviorTree.h"
#include "EconomySystem.h"
#include "AI/PedestrianEcology.h"
#include "AI/MatrixThreatHeatmap.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

createFileSingleton(FactionWarManager);

// ---------------------------------------------------------------------------
// Strategic Faction Commander Implementation
// ---------------------------------------------------------------------------

StrategicFactionCommander::StrategicFactionCommander()
    : m_faction(FACTION_MACHINES), m_name("Machine Mainframe"), m_warSupply(100), m_preferredTargetNodeId(0), m_lastEvalMs(0)
{
}

StrategicFactionCommander::StrategicFactionCommander(uint32 faction, const std::string& name)
    : m_faction(faction), m_name(name), m_warSupply(100), m_preferredTargetNodeId(0), m_lastEvalMs(0)
{
}

void StrategicFactionCommander::EvaluateFrontlines(const std::map<uint32, DistrictStatus>& districts, 
                                                   const std::map<uint32, ControlNode>& nodes, 
                                                   uint32 currentMs)
{
    m_lastEvalMs = currentMs;
    m_preferredTargetNodeId = 0;

    float bestScore = -1.0f;

    for (const auto& pair : nodes) {
        const ControlNode& node = pair.second;
        if (node.controllingFaction == m_faction && !node.isContested) {
            continue; // Already firmly controlled
        }

        float score = 10.0f;

        // Give priority to contested nodes
        if (node.isContested) score += 50.0f;
        if (node.captureProgress > 0.0f) score += (node.captureProgress * 40.0f);

        // Faction-specific strategic doctrine
        if (m_faction == FACTION_MACHINES) {
            // Machines prioritize Downtown (District 2) and Slums (District 1)
            if (node.districtId == 2) score += 30.0f;
            else if (node.districtId == 1) score += 20.0f;
            // High priority on taking Zion nodes
            if (node.controllingFaction == FACTION_ZION) score += 25.0f;
        } else if (m_faction == FACTION_ZION) {
            // Zion prioritizes Slums (District 1) and International (District 3)
            if (node.districtId == 1) score += 35.0f;
            else if (node.districtId == 3) score += 25.0f;
            // Break Machine control
            if (node.controllingFaction == FACTION_MACHINES) score += 30.0f;
        } else { // Merovingian
            // Merovingians target contested or isolated hardlines
            if (node.districtId == 3 || node.districtId >= 4) score += 30.0f;
            if (node.controllingFaction != FACTION_MEROVINGIAN) score += 20.0f;
        }

        if (score > bestScore) {
            bestScore = score;
            m_preferredTargetNodeId = node.id;
        }
    }
}

// ---------------------------------------------------------------------------
// FactionWarManager Implementation
// ---------------------------------------------------------------------------

FactionWarManager::FactionWarManager()
    : m_nextSquadId(100), m_timeSinceLastSync(0), m_lastStrategicTickMs(0)
{
}

FactionWarManager::~FactionWarManager()
{
    m_strikeSquads.clear();
}

void FactionWarManager::initialize()
{
    m_factionScores[FACTION_ZION] = 0;
    m_factionScores[FACTION_MACHINES] = 0;
    m_factionScores[FACTION_MEROVINGIAN] = 0;

    // Initialize the Three Strategic Faction Commanders
    m_commanders.clear();
    m_commanders.emplace_back(FACTION_MACHINES, "Agent Gray Subsystem Directive");
    m_commanders.emplace_back(FACTION_ZION, "Commander Lock Defense Taskforce");
    m_commanders.emplace_back(FACTION_MEROVINGIAN, "The Frenchman's Syndicate Syndicate");

    // Initialize District Metadata
    m_districtStatus[1] = {1, "Slums", 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, FACTION_MACHINES, {}};
    m_districtStatus[2] = {2, "Downtown", 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, FACTION_MACHINES, {}};
    m_districtStatus[3] = {3, "International", 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, FACTION_MACHINES, {}};
    m_districtStatus[4] = {4, "Richland", 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, FACTION_MACHINES, {}};
    
    // Attempt to load current standings from DB
    if (Database_Main != nullptr)
    {
        scoped_ptr<QueryResult> result(sDatabase.Query("SELECT `faction`, SUM(`control_points`) FROM `territory_map` GROUP BY `faction`"));
        if (result)
        {
            do
            {
                Field *field = result->Fetch();
                uint32 faction = field[0].GetUInt32();
                uint32 score = field[1].GetUInt32();
                m_factionScores[faction] = score;
            } while (result->NextRow());
        }
        
        // Load Hardlines as Control Nodes with DistrictId
        scoped_ptr<QueryResult> hlResult(sDatabase.Query("SELECT `HardlineId`, `DistrictId`, `X`, `Y`, `Z`, `FactionTag` FROM `hardlines`"));
        if (hlResult)
        {
            do
            {
                Field *field = hlResult->Fetch();
                uint32 id = field[0].GetUInt32();
                uint32 distId = field[1].GetUInt32();
                float x = field[2].GetFloat();
                float y = field[3].GetFloat();
                float z = field[4].GetFloat();
                uint32 faction = field[5].GetUInt32();
                
                registerControlNode(id, distId, x, y, z);
                if (faction != 0) {
                    m_controlNodes[id].controllingFaction = faction;
                }
            } while (hlResult->NextRow());
        }
    }
    
    // Phase 14: Ensure district_control table exists and load persistent district ownership
    EnsureDistrictControlTable();
    LoadDistrictControlFromDB();
    updateDistrictFrontlines(0);

    INFO_LOG(format("FactionWarManager Initialized with %1% Control Nodes across %2% Strategic Districts.") 
             % m_controlNodes.size() % m_districtStatus.size());
}

void FactionWarManager::registerControlNode(uint32 id, uint32 districtId, float x, float y, float z)
{
    ControlNode node;
    node.id = id;
    node.districtId = (districtId == 0) ? 1 : districtId;
    node.x = x;
    node.y = y;
    node.z = z;
    node.controllingFaction = FACTION_MACHINES; // Default to Machines
    node.captureProgress = 0.0f;
    node.capturingFaction = 0;
    node.isContested = false;
    node.lastContestedMs = 0;
    m_controlNodes[id] = node;
}

void FactionWarManager::update(uint32 deltaMs)
{
    if (!FactionWarEnabled()) return;
    m_timeSinceLastSync += deltaMs;
    m_lastStrategicTickMs += deltaMs;
    m_worldEventTimerMs += deltaMs;
    
    updateControlNodes(deltaMs);
    updateDistrictFrontlines(deltaMs);

    // Strategic Commander AI tick every 30 seconds
    if (m_lastStrategicTickMs >= 30000) {
        updateFactionCommanders(m_lastStrategicTickMs);
        m_lastStrategicTickMs = 0;
    }

    // Phase 15: Dynamic Live Event Director periodic check (every 15 minutes = 900,000 ms)
    if (m_worldEventTimerMs >= 900000) {
        m_worldEventTimerMs = 0;
        bool hasActiveEvent = false;
        for (const auto& ev : m_activeWorldEvents) {
            if (ev.active) {
                hasActiveEvent = true;
                break;
            }
        }
        if (!hasActiveEvent) {
            uint32 targetDistrict = GetHighestTensionDistrict();
            WorldEventType eventType = ((m_nextEventId % 2) == 0)
                ? WorldEventType::MEROVINGIAN_EXILE_RAID
                : WorldEventType::MACHINE_SENTINEL_INCURSION;
            TriggerWorldEvent(targetDistrict, eventType);
        }
    }

    updateStrikeSquads(deltaMs);

    // Sync to DB every hour (3600000 ms)
    if (m_timeSinceLastSync >= 3600000)
    {
        syncToDatabase();
        updateBroadcasts(deltaMs);
        m_timeSinceLastSync = 0;
    }
}

void FactionWarManager::updateDistrictFrontlines(uint32 deltaMs)
{
    // Reset district counters
    for (auto& pair : m_districtStatus) {
        DistrictStatus& d = pair.second;
        d.totalNodes = 0;
        d.machineNodes = 0;
        d.zionNodes = 0;
        d.meroNodes = 0;
        d.contestedNodeIds.clear();
    }

    // Tally nodes
    for (auto& pair : m_controlNodes) {
        ControlNode& node = pair.second;
        uint32 dId = node.districtId;
        if (m_districtStatus.find(dId) == m_districtStatus.end()) {
            m_districtStatus[dId] = {dId, "Megacity District " + std::to_string(dId), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, FACTION_MACHINES, {}};
        }
        DistrictStatus& d = m_districtStatus[dId];
        d.totalNodes++;
        if (node.controllingFaction == FACTION_MACHINES) d.machineNodes++;
        else if (node.controllingFaction == FACTION_ZION) d.zionNodes++;
        else if (node.controllingFaction == FACTION_MEROVINGIAN) d.meroNodes++;

        // Detect contested status: capture progress > 0 or within 300m of enemy node
        bool enemyAdjacent = false;
        for (const auto& otherPair : m_controlNodes) {
            if (otherPair.first == node.id) continue;
            const ControlNode& otherNode = otherPair.second;
            if (otherNode.controllingFaction != node.controllingFaction) {
                float dx = node.x - otherNode.x;
                float dz = node.z - otherNode.z;
                if ((dx * dx + dz * dz) <= 900000000.0f) { // 30,000 units (300m)
                    enemyAdjacent = true;
                    break;
                }
            }
        }

        node.isContested = (node.captureProgress > 0.05f) || enemyAdjacent;
        if (node.isContested) {
            d.contestedNodeIds.push_back(node.id);
        }
    }

    // Compute influence percentages
    for (auto& pair : m_districtStatus) {
        DistrictStatus& d = pair.second;
        if (d.totalNodes > 0) {
            d.machineInfluence = float(d.machineNodes) / float(d.totalNodes);
            d.zionInfluence    = float(d.zionNodes) / float(d.totalNodes);
            d.meroInfluence    = float(d.meroNodes) / float(d.totalNodes);

            if (d.machineInfluence >= d.zionInfluence && d.machineInfluence >= d.meroInfluence)
                d.dominantFaction = FACTION_MACHINES;
            else if (d.zionInfluence >= d.machineInfluence && d.zionInfluence >= d.meroInfluence)
                d.dominantFaction = FACTION_ZION;
            else
                d.dominantFaction = FACTION_MEROVINGIAN;
        }
        auto ovIt = m_districtControlOverride.find(pair.first);
        if (ovIt != m_districtControlOverride.end() && ovIt->second != 0) {
            d.dominantFaction = ovIt->second;
            if (d.totalNodes == 0) {
                d.machineInfluence = (ovIt->second == FACTION_MACHINES) ? 1.0f : 0.0f;
                d.zionInfluence    = (ovIt->second == FACTION_ZION) ? 1.0f : 0.0f;
                d.meroInfluence    = (ovIt->second == FACTION_MEROVINGIAN) ? 1.0f : 0.0f;
            }
        }
    }
}

void FactionWarManager::updateFactionCommanders(uint32 deltaMs)
{
    uint32 now = getMSTime();

    for (auto& commander : m_commanders) {
        commander.AddWarSupply(25); // Passive supply tick
        commander.EvaluateFrontlines(m_districtStatus, m_controlNodes, now);

        uint32 targetNode = commander.GetPreferredTargetNode();
        if (targetNode != 0 && commander.GetWarSupply() >= 50) {
            // Count active squads for this faction
            size_t activeCount = 0;
            for (const auto& sq : m_strikeSquads) {
                if (sq.active && sq.faction == commander.GetFaction()) activeCount++;
            }

            if (activeCount < 2) {
                commander.SpendWarSupply(50);
                DeployStrikeSquad(commander.GetFaction(), targetNode);
            }
        }
    }
}

bool FactionWarManager::DeployStrikeSquad(uint32 faction, uint32 targetNodeId)
{
    if (m_controlNodes.find(targetNodeId) == m_controlNodes.end()) return false;
    const ControlNode& node = m_controlNodes[targetNodeId];

    // Find nearest friendly hardline or spawn offset 100m away
    float spawnX = node.x - 5000.0f;
    float spawnY = node.y;
    float spawnZ = node.z - 5000.0f;

    uint32 squadId = ++m_nextSquadId;

    StrikeSquad squad;
    squad.squadId = squadId;
    squad.faction = faction;
    squad.targetNodeId = targetNodeId;
    squad.targetPos = LocationVector(node.x, node.y, node.z);
    squad.state = SQUAD_STATE_ADVANCING;
    squad.lastCalloutTime = getMSTime();
    squad.spawnTime = getMSTime();
    squad.active = true;

    // Role naming and spawn handles
    std::string opName, hackName, maName;
    std::string opRsi, hackRsi, maRsi;

    if (faction == FACTION_MACHINES) {
        opName = "Machine_Enforcer_Tank_" + std::to_string(squadId);
        hackName = "Machine_Logic_Probe_" + std::to_string(squadId);
        maName = "Machine_Subroutine_Striker_" + std::to_string(squadId);
        opRsi = "6e060040"; hackRsi = "6e060040"; maRsi = "6e060040";
    } else if (faction == FACTION_ZION) {
        opName = "Zion_Operative_Vanguard_" + std::to_string(squadId);
        hackName = "Zion_Code_Specialist_" + std::to_string(squadId);
        maName = "Zion_Strikemaster_" + std::to_string(squadId);
        opRsi = "2a010040"; hackRsi = "2a020040"; maRsi = "2a030040";
    } else {
        opName = "Mero_Myrmidon_" + std::to_string(squadId);
        hackName = "Mero_Exile_Cipher_" + std::to_string(squadId);
        maName = "Mero_Nightblade_" + std::to_string(squadId);
        opRsi = "4b010040"; hackRsi = "4b020040"; maRsi = "4b030040";
    }

    // 1. Spawn Operative (Point / Tank)
    auto opBot = sBotMgr.SpawnSingleBot(spawnX, spawnY, spawnZ, faction);
    if (!opBot) return false;
    uint32 opGoId = opBot->GetPlayerGoId();
    opBot->SetCrewId(squadId);
    PlayerObject* poOp = BotGetPlayer(opGoId);
    if (poOp) {
        poOp->setHandle(opName);
        poOp->setMaximumHealth(3500);
        poOp->setCurrentHealth(3500);
        poOp->setLevel(50);
        poOp->setRsiHex(opRsi);
    }
    squad.operative = {opGoId, ROLE_OPERATIVE_TANK, true};

    // 2. Spawn Hacker (Support / Debuff)
    auto hackBot = sBotMgr.SpawnSingleBot(spawnX - 1000.0f, spawnY, spawnZ - 1000.0f, faction);
    if (!hackBot) return false;
    uint32 hackGoId = hackBot->GetPlayerGoId();
    hackBot->SetCrewId(squadId);
    hackBot->SetCrewLeaderId(opGoId);
    PlayerObject* poHack = BotGetPlayer(hackGoId);
    if (poHack) {
        poHack->setHandle(hackName);
        poHack->setMaximumHealth(2200);
        poHack->setCurrentHealth(2200);
        poHack->setLevel(50);
        poHack->setRsiHex(hackRsi);
    }
    squad.hacker = {hackGoId, ROLE_HACKER_SUPPORT, true};

    // 3. Spawn Martial Artist (Flanker)
    auto maBot = sBotMgr.SpawnSingleBot(spawnX + 1000.0f, spawnY, spawnZ - 500.0f, faction);
    if (!maBot) return false;
    uint32 maGoId = maBot->GetPlayerGoId();
    maBot->SetCrewId(squadId);
    maBot->SetCrewLeaderId(opGoId);
    PlayerObject* poMa = BotGetPlayer(maGoId);
    if (poMa) {
        poMa->setHandle(maName);
        poMa->setMaximumHealth(2600);
        poMa->setCurrentHealth(2600);
        poMa->setLevel(50);
        poMa->setRsiHex(maRsi);
    }
    squad.martialArtist = {maGoId, ROLE_MARTIAL_ARTIST_FLANK, true};

    m_strikeSquads.push_back(squad);

    std::string facStr = (faction == FACTION_ZION) ? "Zion" : (faction == FACTION_MACHINES ? "Machines" : "Merovingian");
    INFO_LOG(format("FactionWarManager: Deployed Strike Squad #%1% (%2%) towards Node %3% at (%4%, %5%)")
             % squadId % facStr % targetNodeId % node.x % node.z);

    // Initial deployment callout
    BroadcastSquadCallout(squad, "Operative", (format("Strike Squad %1% mobilizing! Target locked on Hardline %2%. Advance!") % squadId % targetNodeId).str());

    return true;
}

void FactionWarManager::BroadcastSquadCallout(const StrikeSquad& squad, const std::string& roleStr, const std::string& msg)
{
    PlayerObject* speaker = BotGetPlayer(squad.operative.goId);
    if (!speaker) speaker = BotGetPlayer(squad.hacker.goId);
    if (!speaker) speaker = BotGetPlayer(squad.martialArtist.goId);
    if (!speaker) return;

    // Only broadcast if there are actual human players within 20m (2000 units) proximity,
    // preventing fake background chatter spam while operative is alone.
    auto clients = sSpatialGrid.GetClientsInRadius(speaker->getPosition().x, speaker->getPosition().z, 2000.0f);
    bool hasHumanNearby = false;
    for (GameClient* gc : clients) {
        if (!gc->isBot()) {
            hasHumanNearby = true;
            break;
        }
    }
    if (!hasHumanNearby) return;

    std::string fullMsg = (format("[%1% - %2%] : %3%") % speaker->getHandle() % roleStr % msg).str();
    for (GameClient* gc : clients) {
        if (!gc->isBot()) {
            gc->QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:FFAA00}%1%{/c}") % fullMsg).str()
            ));
        }
    }
}

void FactionWarManager::updateStrikeSquads(uint32 deltaMs)
{
    uint32 now = getMSTime();

    for (auto it = m_strikeSquads.begin(); it != m_strikeSquads.end();) {
        StrikeSquad& squad = *it;
        if (!squad.active) {
            it = m_strikeSquads.erase(it);
            continue;
        }

        PlayerObject* op = BotGetPlayer(squad.operative.goId);
        PlayerObject* hack = BotGetPlayer(squad.hacker.goId);
        PlayerObject* ma = BotGetPlayer(squad.martialArtist.goId);

        squad.operative.isAlive = (op && !op->isDead());
        squad.hacker.isAlive = (hack && !hack->isDead());
        squad.martialArtist.isAlive = (ma && !ma->isDead());

        // Disband squad if all members are wiped out
        if (!squad.operative.isAlive && !squad.hacker.isAlive && !squad.martialArtist.isAlive) {
            INFO_LOG(format("FactionWarManager: Strike Squad %1% has been eliminated.") % squad.squadId);
            it = m_strikeSquads.erase(it);
            continue;
        }

        // Squad Tactical Coordination Loop
        float tx = squad.targetPos.x;
        float ty = squad.targetPos.y;
        float tz = squad.targetPos.z;

        PlayerObject* squadLead = squad.operative.isAlive ? op : (squad.martialArtist.isAlive ? ma : hack);
        if (squadLead) {
            LocationVector leadPos = squadLead->getPosition();
            float distToTarget = std::sqrt(std::pow(leadPos.x - tx, 2) + std::pow(leadPos.z - tz, 2));

            if (distToTarget > 1500.0f) { // Advancing to node
                squad.state = SQUAD_STATE_ADVANCING;
                auto leadBot = sBotMgr.GetBotByGOID(squadLead->getGoId());
                if (leadBot) leadBot->MoveTo(tx, ty, tz);

                // Hacker trails behind lead
                if (squad.hacker.isAlive && squadLead != hack) {
                    auto hackBot = sBotMgr.GetBotByGOID(squad.hacker.goId);
                    if (hackBot) hackBot->MoveTo(leadPos.x - 1200.0f, leadPos.y, leadPos.z - 1200.0f);
                }

                // Martial Artist on the flank
                if (squad.martialArtist.isAlive && squadLead != ma) {
                    auto maBot = sBotMgr.GetBotByGOID(squad.martialArtist.goId);
                    if (maBot) maBot->MoveTo(leadPos.x + 1000.0f, leadPos.y, leadPos.z - 500.0f);
                }

                if (now - squad.lastCalloutTime > 20000) {
                    squad.lastCalloutTime = now;
                    BroadcastSquadCallout(squad, "Tactical", "Moving on the contested perimeter. Maintain formation.");
                }
            } else {
                // At the target node!
                auto nodeIt = m_controlNodes.find(squad.targetNodeId);
                if (nodeIt != m_controlNodes.end() && nodeIt->second.controllingFaction == squad.faction) {
                    // Node captured! Hold defensive position
                    if (squad.state != SQUAD_STATE_HOLDING) {
                        squad.state = SQUAD_STATE_HOLDING;
                        BroadcastSquadCallout(squad, "Operative", "Hardline perimeter secured! Holding defensive line!");
                    }


                } else {
                    // Engaging / capturing node
                    if (squad.state != SQUAD_STATE_ENGAGING) {
                        squad.state = SQUAD_STATE_ENGAGING;
                        BroadcastSquadCallout(squad, "Operative", "Breaching hardline! Hacker, drop their firewall! Striker, clear the flanks!");
                    }



                    // Synchronized triad action:
                    // 1. Point combatant engages closest enemy defender & acts as heavy aggro magnet
                    uint32 targetGoId = 0;
                    auto pointBot = sBotMgr.GetBotByGOID(squadLead->getGoId());
                    if (pointBot) {
                        if (pointBot->GetTargetGoId() == 0) {
                            auto localClients = sSpatialGrid.GetClientsInRadius(tx, tz, 2500.0f);
                            for (GameClient* gc : localClients) {
                                uint32 gid = gc->GetPlayerGoId();
                                if (gid == 0 || gid == squad.operative.goId || gid == squad.hacker.goId || gid == squad.martialArtist.goId) continue;
                                PlayerObject* p = BotGetPlayer(gid);
                                if (p && !p->isDead() && (p->getFaction() != squad.faction || p->getHandle().find("Smith") != std::string::npos) && p->getFactionName() != "Civilian") {
                                    pointBot->SetTargetGoId(gid);
                                    break;
                                }
                            }
                        }
                        targetGoId = pointBot->GetTargetGoId();
                    }

                    if (targetGoId != 0) {
                        uint32 enemyGoId = targetGoId;
                        PlayerObject* enemyPo = BotGetPlayer(enemyGoId);

                        // Zion Operative Vanguard Tank Synergies: High-threat aggro redirect / body-blocking
                        if (squad.faction == FACTION_ZION && enemyPo && !enemyPo->isDead() && squad.operative.isAlive) {
                            auto enemyBot = sBotMgr.GetBotByGOID(enemyGoId);
                            if (enemyBot && (enemyBot->GetTargetGoId() == squad.hacker.goId || enemyBot->GetTargetGoId() == 0)) {
                                enemyBot->SetTargetGoId(squad.operative.goId); // Force aggro onto Vanguard tank
                                if (rand() % 15 == 0) {
                                    BroadcastSquadCallout(squad, "Operative", "Eyes on me, suit! Leave the civilians and support alone!");
                                }
                            }
                        }
                        
                        // 2. Hacker supports by debuffing, healing, replenishing IS, and antiviral code purging
                        if (squad.hacker.isAlive) {
                            auto hackBot = sBotMgr.GetBotByGOID(squad.hacker.goId);
                            if (hackBot) {
                                // Maintain 8-10m standoff behind the tank/lead
                                LocationVector leadCurPos = squadLead->getPosition();
                                hackBot->MoveTo((float)leadCurPos.x - 900.0f, (float)leadCurPos.y, (float)leadCurPos.z - 900.0f);
                            }

                            const AbilityTemplate* t = sDataLoader.GetAbilityTemplate(1);
                            sHackerSystem.ExecuteHackerAbility(hack, 1, enemyGoId, t);
                            
                            // Heal operative if wounded
                            if (op && op->getCurrentHealth() < op->getMaximumHealth() * 0.6f) {
                                op->setCurrentHealth(std::min<uint32>(op->getMaximumHealth(), op->getCurrentHealth() + 600));
                                BroadcastSquadCallout(squad, "Hacker", "Patching operative health matrix!");
                            }

                            // Replenish Inner Strength to strike team (+35 IS)
                            if (op && op->getCurrentIS() < op->getMaximumIS()) {
                                op->setCurrentIS(std::min<uint16>(op->getMaximumIS(), op->getCurrentIS() + 35));
                            }
                            if (ma && ma->getCurrentIS() < ma->getMaximumIS()) {
                                ma->setCurrentIS(std::min<uint16>(ma->getMaximumIS(), ma->getCurrentIS() + 35));
                            }


                        }

                        // 3. Martial Artist flanks, executes power combat, and 85% interlock infection interrupt
                        if (squad.martialArtist.isAlive) {
                            auto maBot = sBotMgr.GetBotByGOID(squad.martialArtist.goId);
                            if (maBot) {
                                maBot->SetTargetGoId(enemyGoId);
                                ma->setTactic(TACTIC_POWER);
                                sCombatSys.SetTactic(squad.martialArtist.goId, TACTIC_POWER);

                                // 85% interrupt on clones actively attempting infection
                                if (squad.faction == FACTION_ZION && enemyPo && !enemyPo->isDead()) {
                                    bool isSmith = (enemyPo->getHandle().find("Smith") != std::string::npos);
                                    auto enemyBot = sBotMgr.GetBotByGOID(enemyGoId);
                                    if (isSmith && (enemyBot && enemyBot->IsInfecting()) && (rand() % 100) < 85) {
                                        enemyBot->StopInfecting();
                                        enemyBot->SetTargetGoId(0);
                                        enemyPo->takeDamage(ma->getGoId(), 250, 0);
                                        BroadcastSquadCallout(squad, "Striker", "Wire-fu sweep! Knocked the clone down, breaking the channel!");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ++it;
    }
}

const DistrictStatus* FactionWarManager::GetDistrict(uint32 districtId) const
{
    auto it = m_districtStatus.find(districtId);
    if (it != m_districtStatus.end()) return &it->second;
    return nullptr;
}

void FactionWarManager::registerPvPKill(uint32 killerFaction, uint32 victimFaction)
{
    if (killerFaction == victimFaction) return; // No points for teamkilling
    
    m_factionScores[killerFaction] += 10; // 10 control points per kill
    INFO_LOG(format("FactionWar: Faction %1% gained 10 points for killing Faction %2%") % killerFaction % victimFaction);
}

void FactionWarManager::syncToDatabase()
{
    INFO_LOG("FactionWarManager: Syncing territory map to database...");
    if (Database_Main != nullptr)
    {
        for (auto it = m_factionScores.begin(); it != m_factionScores.end(); ++it)
        {
            PreparedStatement stmt("REPLACE INTO `territory_map` (`territory_id`, `faction`, `control_points`) VALUES (1, ?0, ?1)");
            stmt.SetUInt32(0, it->first);
            stmt.SetUInt32(1, it->second);
            sDatabase.ExecutePrepared(&stmt);
        }
    }
}

void FactionWarManager::updateBroadcasts(uint32 deltaMs)
{
    uint32 zionScore = m_factionScores[FACTION_ZION];
    uint32 machScore = m_factionScores[FACTION_MACHINES];
    uint32 meroScore = m_factionScores[FACTION_MEROVINGIAN];

    string leader = "Contested";
    if (zionScore > machScore && zionScore > meroScore) leader = "Zion";
    else if (machScore > zionScore && machScore > meroScore) leader = "Machines";
    else if (meroScore > zionScore && meroScore > machScore) leader = "Merovingian";

    string broadcastMsg = (format("{c:FFFF00}[Radio Free Zion] : Faction Control Update! Zion: %1% | Machines: %2% | Merovingian: %3%. Current Leader: %4%{/c}") 
                            % zionScore % machScore % meroScore % leader).str();

    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players)
    {
        PlayerObject* p = sObjMgr.getGOPtr(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:00FFFF}[Environment] District billboards updated to %1% propaganda.{/c}") % leader).str()
            ));
        }
    }
    INFO_LOG("FactionWarManager: Broadcasted Radio Free Zion update.");
}

void FactionWarManager::captureNode(uint32 id, uint32 newFaction)
{
    if (m_controlNodes.find(id) != m_controlNodes.end())
    {
        uint32 oldFaction = m_controlNodes[id].controllingFaction;
        if (oldFaction == newFaction) return;

        m_controlNodes[id].controllingFaction = newFaction;
        m_controlNodes[id].captureProgress = 0.0f;

        if (Database_Main != nullptr)
        {
            // Persist Hardline FactionTag into MariaDB hardlines table
            PreparedStatement hlStmt("UPDATE `hardlines` SET `FactionTag` = ?0 WHERE `HardlineId` = ?1");
            hlStmt.SetUInt32(0, newFaction);
            hlStmt.SetUInt32(1, id);
            sDatabase.ExecutePrepared(&hlStmt);

            // Update district frontlines and score
            PreparedStatement scoreStmt("REPLACE INTO `territory_map` (`territory_id`, `faction`, `control_points`) VALUES (?0, ?1, ?2)");
            scoreStmt.SetUInt32(0, m_controlNodes[id].districtId);
            scoreStmt.SetUInt32(1, newFaction);
            scoreStmt.SetUInt32(2, m_factionScores[newFaction] + 25);
            sDatabase.ExecutePrepared(&scoreStmt);
        }

        uint32 distId = m_controlNodes[id].districtId;
        m_districtControlOverride[distId] = newFaction;
        m_factionScores[newFaction] += 25;
        updateDistrictFrontlines(0);
        SaveDistrictControlToDB(distId);
        
        string factionStr = GetFactionControlName(newFaction);

        string districtBuff = GetActiveDistrictBuffName(distId);
        string broadcastMsg = (format("{c:FFFF00}[Radio Free Zion] : Hardline Uplink %1% has been CAPTURED by %2%! Dynamic District Buff: %3%{/c}") % id % factionStr % districtBuff).str();
        auto players = sObjMgr.getAllGOIds();
        for (auto goId : players)
        {
            PlayerObject* p = sObjMgr.getGOPtr(goId);
            if (p && !p->getClient().isBot()) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
            }
        }
        INFO_LOG(format("FactionWarManager: Node %1% captured by %2% (District buff: %3%)") % id % factionStr % districtBuff);
    }
}

bool FactionWarManager::AdvanceNodeCapture(uint32 nodeId, uint32 faction, float deltaProgress, PlayerObject* player)
{
    if (!FactionWarEnabled() && !(player && player->isAdmin())) return false;
    auto it = m_controlNodes.find(nodeId);
    if (it == m_controlNodes.end()) return false;

    ControlNode& node = it->second;
    if (node.controllingFaction == faction) return false;

    if (node.capturingFaction != faction) {
        node.capturingFaction = faction;
        node.captureProgress = 0.0f;
    }

    node.captureProgress += deltaProgress;
    node.isContested = true;

    uint32 now = getMSTime();
    if (node.lastContestedMs == 0 || (now - node.lastContestedMs > 15000)) {
        node.lastContestedMs = now;
        string attackerStr = "Machines";
        if (faction == FACTION_ZION) attackerStr = "Zion";
        else if (faction == FACTION_MEROVINGIAN) attackerStr = "Merovingian";

        string alertMsg = (format("{c:FF3300}[FACTION ALERT] Hardline Broadcast Node #%1% (District %2%) is UNDER VIRAL ASSAULT by %3%! Defense requested immediately!{/c}")
            % node.id % node.districtId % attackerStr).str();
        INFO_LOG(format("[FACTION ALERT] Hardline Broadcast Node #%1% (District %2%) is UNDER VIRAL ASSAULT by %3%!") % node.id % node.districtId % attackerStr);

        auto players = sObjMgr.getAllGOIds();
        for (auto goId : players) {
            PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
            if (p && !p->getClient().isBot() && p->getDistrict() == node.districtId) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(alertMsg));
            }
        }
    }

    if (player) {
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[VIRUS UPLOAD] Hardline #%1% capture progress: %2%%%{/c}")
                % nodeId % int(node.captureProgress * 100.0f)).str()
        ));
    }

    if (node.captureProgress >= 1.0f) {
        captureNode(nodeId, faction);
        sBotMgr.SpawnFactionDefenders(1, nodeId, (uint8)faction, LocationVector(node.x, node.y, node.z));
        if (player) {
            player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:00FF00}[FACTION WAR] You successfully captured Hardline Node #%1%!{/c}") % nodeId).str()
            ));
        }
    }

    return true;
}

uint32 FactionWarManager::getControllingFaction(uint32 nodeId)
{
    if (m_controlNodes.find(nodeId) != m_controlNodes.end())
        return m_controlNodes[nodeId].controllingFaction;
    return FACTION_MACHINES;
}

uint32 FactionWarManager::getControllingFactionByLocation(float x, float y, float z)
{
    float minDistSq = -1.0f;
    uint32 bestFaction = FACTION_MACHINES;

    for (auto& pair : m_controlNodes) {
        float dx = pair.second.x - x;
        float dy = pair.second.y - y;
        float dz = pair.second.z - z;
        float distSq = dx*dx + dy*dy + dz*dz;
        if (minDistSq < 0.0f || distSq < minDistSq) {
            minDistSq = distSq;
            bestFaction = pair.second.controllingFaction;
        }
    }
    return bestFaction;
}

void FactionWarManager::updateControlNodes(uint32 deltaMs)
{
    static uint32 tickAcc = 0;
    tickAcc += deltaMs;
    if (tickAcc < 1000) return; // Process every second
    float dtSeconds = tickAcc / 1000.0f;
    tickAcc = 0;

    for (auto& pair : m_controlNodes) {
        ControlNode& node = pair.second;
        
        int factionPresence[5] = {0, 0, 0, 0, 0}; // Indexed by mxoFaction: 1: Zion, 2: Machines, 3: Merovingian
        auto nearbyClients = sSpatialGrid.GetClientsInRadius(node.x, node.z, 1500.0f);
        
        for (GameClient* gc : nearbyClients) {
            uint32 goId = gc->GetPlayerGoId();
            if (goId == 0) continue;
            try {
                PlayerObject* p = sObjMgr.getGOPtr(goId);
                if (p && !p->isDead()) {
                    LocationVector pos = p->getPosition();
                    float dx = pos.x - node.x;
                    float dz = pos.z - node.z;
                    if ((dx * dx + dz * dz) <= 2250000.0f) { // 1500 units (15m radius)
                        int fac = p->getFaction();
                        if (fac >= 1 && fac <= 3) {
                            factionPresence[fac]++;
                        }
                    }
                }
            } catch (...) {}
        }

        // Determine dominant contesting faction
        int maxPresence = 0;
        int dominantFaction = -1;
        for (int i = 1; i <= 3; ++i) {
            if (i != (int)node.controllingFaction && factionPresence[i] > maxPresence) {
                maxPresence = factionPresence[i];
                dominantFaction = i;
            }
        }

        if (dominantFaction >= 1 && (node.controllingFaction > 4 || maxPresence > factionPresence[node.controllingFaction])) {
            if (node.capturingFaction != (uint32)dominantFaction) {
                node.capturingFaction = dominantFaction;
                node.captureProgress = 0.0f;
            }
            node.captureProgress += 0.05f * dtSeconds * float(maxPresence);

            uint32 now = getMSTime();
            if (node.captureProgress > 0.10f && (now - node.lastContestedMs > 30000)) {
                node.lastContestedMs = now;
                string attackerStr = "Machines";
                if (node.capturingFaction == FACTION_ZION) attackerStr = "Zion";
                else if (node.capturingFaction == FACTION_MEROVINGIAN) attackerStr = "Merovingian";

                string alertMsg = (format("{c:FF3300}[FACTION ALERT] Hardline Node #%1% (District %2%) is UNDER VIRAL ASSAULT by %3%! Defense requested immediately!{/c}")
                    % node.id % node.districtId % attackerStr).str();
                INFO_LOG(format("[FACTION ALERT] Hardline Node #%1% (District %2%) is UNDER VIRAL ASSAULT by %3%!") % node.id % node.districtId % attackerStr);

                auto players = sObjMgr.getAllGOIds();
                for (auto goId : players) {
                    PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
                    if (p && !p->getClient().isBot() && p->getDistrict() == node.districtId) {
                        p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(alertMsg));
                    }
                }
            }

            if (node.captureProgress >= 1.0f) {
                captureNode(node.id, node.capturingFaction);
                sBotMgr.SpawnFactionDefenders(1, node.id, (uint8)node.capturingFaction, LocationVector(node.x, node.y, node.z));
            }
        } else if (node.captureProgress > 0.0f) {
            node.captureProgress = std::max(0.0f, node.captureProgress - 0.05f * dtSeconds);
        }
    }
}

bool FactionWarManager::getClosestEnemyNode(uint32 myFaction, float x, float y, float z, float& outX, float& outY, float& outZ)
{
    float minDistSq = -1.0f;
    bool found = false;

    for (auto& pair : m_controlNodes) {
        if (pair.second.controllingFaction != myFaction) {
            float dx = pair.second.x - x;
            float dy = pair.second.y - y;
            float dz = pair.second.z - z;
            float distSq = dx*dx + dy*dy + dz*dz;
            if (minDistSq < 0.0f || distSq < minDistSq) {
                minDistSq = distSq;
                outX = pair.second.x;
                outY = pair.second.y;
                outZ = pair.second.z;
                found = true;
            }
        }
    }
    return found;
}

bool FactionWarManager::HasDistrictDominance(uint32 districtId, uint32 faction) const
{
    if (!FactionWarEnabled()) return false; // no territory buffs in retail
    if (faction == 0) return false;
    auto ovIt = m_districtControlOverride.find(districtId);
    if (ovIt != m_districtControlOverride.end() && ovIt->second != 0) {
        return ovIt->second == faction;
    }

    auto it = m_districtStatus.find(districtId);
    if (it == m_districtStatus.end()) return false;
    if (it->second.totalNodes == 0) {
        return it->second.dominantFaction == faction;
    }

    const DistrictStatus& d = it->second;
    uint32 factionNodes = 0;
    if (faction == FACTION_MACHINES) factionNodes = d.machineNodes;
    else if (faction == FACTION_ZION) factionNodes = d.zionNodes;
    else if (faction == FACTION_MEROVINGIAN) factionNodes = d.meroNodes;

    return (static_cast<float>(factionNodes) / static_cast<float>(d.totalNodes)) >= 0.50f;
}

std::string FactionWarManager::GetActiveDistrictBuffName(uint32 districtId) const
{
    if (HasDistrictDominance(districtId, FACTION_MACHINES)) return "Machine Territory Control (+5% IS Regen, +5% $Info Drops, +15% Health Regen)";
    if (HasDistrictDominance(districtId, FACTION_ZION)) return "Zion Territory Control (+5% IS Regen, +5% $Info Drops, +20% Wire-Fu Leap)";
    if (HasDistrictDominance(districtId, FACTION_MEROVINGIAN)) return "Merovingian Territory Control (+5% IS Regen, +5% $Info Drops)";
    return "None (Contested Territory)";
}

float FactionWarManager::GetDistrictHealthRegenBonus(uint32 districtId, uint32 faction) const
{
    if (faction == FACTION_MACHINES && HasDistrictDominance(districtId, FACTION_MACHINES)) return 0.15f;
    return 0.0f;
}

float FactionWarManager::GetDistrictWireFuBonus(uint32 districtId, uint32 faction) const
{
    if (faction == FACTION_ZION && HasDistrictDominance(districtId, FACTION_ZION)) return 0.20f;
    return 0.0f;
}

float FactionWarManager::GetDistrictISRegenBonus(uint32 districtId, uint32 faction) const
{
    if (faction != 0 && HasDistrictDominance(districtId, faction)) return 0.05f;
    return 0.0f;
}

float FactionWarManager::GetDistrictInfoGainBonus(uint32 districtId, uint32 faction) const
{
    if (faction != 0 && HasDistrictDominance(districtId, faction)) return 0.05f;
    return 0.0f;
}

float FactionWarManager::GetDistrictTension(uint32 districtId) const
{
    auto it = m_districtStatus.find(districtId);
    if (it != m_districtStatus.end()) {
        const auto& d = it->second;
        float contestedScore = static_cast<float>(d.contestedNodeIds.size()) * 25.0f;
        float spread = std::abs(d.machineInfluence - d.zionInfluence) + std::abs(d.zionInfluence - d.meroInfluence);
        float balanceScore = std::max(0.0f, (2.0f - spread) * 20.0f);
        float tension = contestedScore + balanceScore + 25.0f;
        return std::clamp(tension, 15.0f, 98.0f);
    }
    return 50.0f;
}

uint32 FactionWarManager::GetHighestTensionDistrict() const
{
    uint32 bestDistrict = 1;
    float maxTension = 0.0f;
    for (const auto& pair : m_districtStatus) {
        float tension = GetDistrictTension(pair.first);
        if (tension > maxTension) {
            maxTension = tension;
            bestDistrict = pair.first;
        }
    }
    return bestDistrict;
}

uint32 FactionWarManager::GetLeadingEnemyFaction(uint32 myFaction, uint32 districtId) const
{
    auto it = m_districtStatus.find(districtId);
    if (it != m_districtStatus.end()) {
        const auto& d = it->second;
        if (myFaction == FACTION_ZION) {
            return (d.machineInfluence >= d.meroInfluence) ? FACTION_MACHINES : FACTION_MEROVINGIAN;
        } else if (myFaction == FACTION_MACHINES) {
            return (d.zionInfluence >= d.meroInfluence) ? FACTION_ZION : FACTION_MEROVINGIAN;
        } else {
            return (d.zionInfluence >= d.machineInfluence) ? FACTION_ZION : FACTION_MACHINES;
        }
    }
    return (myFaction == FACTION_ZION) ? FACTION_MACHINES : FACTION_ZION;
}

std::string FactionWarManager::GetDistrictName(uint32 districtId) const
{
    auto it = m_districtStatus.find(districtId);
    if (it != m_districtStatus.end() && !it->second.name.empty()) {
        return it->second.name;
    }
    switch (districtId) {
        case 1: return "Slums / Barrens";
        case 2: return "Richland Corporate Park";
        case 3: return "Downtown Financial Construct";
        case 4: return "International Cultural Hub";
        case 5: return "Westview Residential Corridor";
        default: return "MegaCity Sector " + std::to_string(districtId);
    }
}

std::string FactionWarManager::GetFactionControlName(uint32 faction) const
{
    if (faction == FACTION_ZION) return "Zion";
    if (faction == FACTION_MACHINES) return "Machine";
    if (faction == FACTION_MEROVINGIAN) return "Merovingian";
    return "Neutral";
}

void FactionWarManager::EnsureDistrictControlTable()
{
    if (Database_Main == nullptr) return;
    sDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS `district_control` ("
        "`district_id` INT UNSIGNED NOT NULL PRIMARY KEY, "
        "`district_name` VARCHAR(64) NOT NULL DEFAULT '', "
        "`controlling_faction` INT UNSIGNED NOT NULL DEFAULT 2, "
        "`faction_name` VARCHAR(32) NOT NULL DEFAULT 'Machine', "
        "`zion_nodes` INT UNSIGNED NOT NULL DEFAULT 0, "
        "`machine_nodes` INT UNSIGNED NOT NULL DEFAULT 0, "
        "`mero_nodes` INT UNSIGNED NOT NULL DEFAULT 0, "
        "`updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    );
}

void FactionWarManager::SaveDistrictControlToDB(uint32 districtId)
{
    uint32 controllingFaction = GetDistrictControllingFaction(districtId);
    std::string factionName = GetFactionControlName(controllingFaction);
    std::string districtName = GetDistrictName(districtId);
    uint32 zionNodes = 0, machineNodes = 0, meroNodes = 0;
    auto it = m_districtStatus.find(districtId);
    if (it != m_districtStatus.end()) {
        zionNodes = it->second.zionNodes;
        machineNodes = it->second.machineNodes;
        meroNodes = it->second.meroNodes;
    }

    m_lastDistrictControlSql = (format(
        "REPLACE INTO `district_control` (`district_id`, `district_name`, `controlling_faction`, `faction_name`, `zion_nodes`, `machine_nodes`, `mero_nodes`) "
        "VALUES (%1%, '%2%', %3%, '%4%', %5%, %6%, %7%)"
    ) % districtId % districtName % controllingFaction % factionName % zionNodes % machineNodes % meroNodes).str();

    if (Database_Main != nullptr)
    {
        PreparedStatement stmt(
            "REPLACE INTO `district_control` (`district_id`, `district_name`, `controlling_faction`, `faction_name`, `zion_nodes`, `machine_nodes`, `mero_nodes`) "
            "VALUES (?0, ?1, ?2, ?3, ?4, ?5, ?6)"
        );
        stmt.SetUInt32(0, districtId);
        stmt.SetString(1, districtName.c_str());
        stmt.SetUInt32(2, controllingFaction);
        stmt.SetString(3, factionName.c_str());
        stmt.SetUInt32(4, zionNodes);
        stmt.SetUInt32(5, machineNodes);
        stmt.SetUInt32(6, meroNodes);
        sDatabase.ExecutePrepared(&stmt);
    }
    WriteWorldStatusJsonFile("world_status.json");
}

void FactionWarManager::LoadDistrictControlFromDB()
{
    if (Database_Main != nullptr)
    {
        scoped_ptr<QueryResult> result(sDatabase.Query(
            "SELECT `district_id`, `district_name`, `controlling_faction`, `zion_nodes`, `machine_nodes`, `mero_nodes` FROM `district_control`"
        ));
        if (result)
        {
            do
            {
                Field* field = result->Fetch();
                uint32 distId = field[0].GetUInt32();
                std::string distName = field[1].GetString();
                uint32 faction = field[2].GetUInt32();
                uint32 zionNodes = field[3].GetUInt32();
                uint32 machineNodes = field[4].GetUInt32();
                uint32 meroNodes = field[5].GetUInt32();

                m_districtControlOverride[distId] = faction;
                if (m_districtStatus.find(distId) == m_districtStatus.end()) {
                    m_districtStatus[distId] = {distId, distName, zionNodes + machineNodes + meroNodes, machineNodes, zionNodes, meroNodes, 0.0f, 0.0f, 0.0f, faction, {}};
                } else {
                    m_districtStatus[distId].dominantFaction = faction;
                }
            } while (result->NextRow());
        }
        else
        {
            // Seed initial district_control rows if empty
            updateDistrictFrontlines(0);
            for (const auto& pair : m_districtStatus) {
                SaveDistrictControlToDB(pair.first);
            }
        }
    }
}

void FactionWarManager::SetDistrictControl(uint32 districtId, uint32 faction, bool persistToDb)
{
    m_districtControlOverride[districtId] = faction;
    if (m_districtStatus.find(districtId) == m_districtStatus.end()) {
        m_districtStatus[districtId] = {districtId, GetDistrictName(districtId), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, faction, {}};
    }
    for (auto& pair : m_controlNodes) {
        if (pair.second.districtId == districtId) {
            pair.second.controllingFaction = faction;
        }
    }
    updateDistrictFrontlines(0);
    m_districtStatus[districtId].dominantFaction = faction;
    if (persistToDb) {
        SaveDistrictControlToDB(districtId);
    }
}

uint32 FactionWarManager::GetDistrictControllingFaction(uint32 districtId) const
{
    auto ovIt = m_districtControlOverride.find(districtId);
    if (ovIt != m_districtControlOverride.end() && ovIt->second != 0) {
        return ovIt->second;
    }
    auto it = m_districtStatus.find(districtId);
    if (it != m_districtStatus.end()) {
        return it->second.dominantFaction;
    }
    return FACTION_MACHINES;
}

uint32 FactionWarManager::TriggerWorldEvent(uint32 districtId, WorldEventType type, float spawnX, float spawnY, float spawnZ)
{
    if (!FactionWarEnabled()) return 0;
    if (districtId == 0) districtId = 1;
    if (type == WorldEventType::NONE) {
        type = WorldEventType::MACHINE_SENTINEL_INCURSION;
    }

    // Default district coordinates if not specified
    if (spawnX == 0.0f && spawnZ == 0.0f) {
        for (const auto& pair : m_controlNodes) {
            if (pair.second.districtId == districtId) {
                spawnX = pair.second.x;
                spawnY = pair.second.y;
                spawnZ = pair.second.z;
                break;
            }
        }
        if (spawnX == 0.0f && spawnZ == 0.0f) {
            spawnX = -15360.0f + float(districtId * 1200);
            spawnY = 572.0f;
            spawnZ = -21125.0f + float(districtId * 1200);
        }
    }

    uint32 eventId = ++m_nextEventId;
    ActiveWorldEvent ev;
    ev.eventId = eventId;
    ev.type = type;
    ev.districtId = districtId;
    ev.spawnPos = LocationVector(spawnX, spawnY, spawnZ);
    ev.startTimeMs = getMSTime();
    ev.active = true;

    std::string bossRsi = "6e060040";
    if (type == WorldEventType::MACHINE_SENTINEL_INCURSION) {
        ev.faction = FACTION_MACHINES;
        ev.eventName = "Machine Sentinel Incursion";
        ev.bossName = "The General's Sentinel Command Unit";
        bossRsi = "6e060040";
    } else {
        ev.faction = FACTION_MEROVINGIAN;
        ev.eventName = "Merovingian Exile Raid";
        ev.bossName = "The Assassin";
        bossRsi = "4b010040";
    }

    auto bossBot = sBotMgr.SpawnSingleBot(spawnX, spawnY, spawnZ, ev.faction);
    if (bossBot) {
        bossBot->SetFaction((mxoFaction)ev.faction);
        ev.bossGoId = bossBot->GetPlayerGoId();
        PlayerObject* poBoss = sObjMgr.getGOPtrSafe(ev.bossGoId);
        if (poBoss) {
            poBoss->setHandle(ev.bossName);
            poBoss->setFaction(ev.faction);
            poBoss->setFactionName(GetFactionControlName(ev.faction));
            poBoss->setLevel(60);
            poBoss->setMaximumHealth(8000);
            poBoss->setCurrentHealth(8000);
            poBoss->setRsiHex(bossRsi);
            poBoss->setDistrict(districtId);
        }
    }

    // Spawn 2 elite escorts
    for (int i = 0; i < 2; ++i) {
        float offsetX = (i == 0) ? 350.0f : -350.0f;
        auto escortBot = sBotMgr.SpawnSingleBot(spawnX + offsetX, spawnY, spawnZ + 250.0f, ev.faction);
        if (escortBot) {
            escortBot->SetFaction((mxoFaction)ev.faction);
            uint32 escortGoId = escortBot->GetPlayerGoId();
            ev.escortGoIds.push_back(escortGoId);
            PlayerObject* poEscort = sObjMgr.getGOPtrSafe(escortGoId);
            if (poEscort) {
                std::string escortName = (type == WorldEventType::MACHINE_SENTINEL_INCURSION)
                    ? ("Sentinel_Enforcer_" + std::to_string(eventId) + "_" + std::to_string(i + 1))
                    : ("Exile_Bloodhound_" + std::to_string(eventId) + "_" + std::to_string(i + 1));
                poEscort->setHandle(escortName);
                poEscort->setFaction(ev.faction);
                poEscort->setFactionName(GetFactionControlName(ev.faction));
                poEscort->setLevel(52);
                poEscort->setMaximumHealth(3000);
                poEscort->setCurrentHealth(3000);
                poEscort->setRsiHex(bossRsi);
                poEscort->setDistrict(districtId);
            }
        }
    }

    m_activeWorldEvents.push_back(ev);
    WriteWorldStatusJsonFile("world_status.json");

    std::string distName = GetDistrictName(districtId);
    std::string warnMsg = (format("{c:FF2200}[WORLD EVENT ALERT] %1% detected in %2% (District %3%)! World Boss '%4%' [Lv60] has materialized near (%5%, %6%)! All Operatives converge!{/c}")
        % ev.eventName % distName % districtId % ev.bossName % int(spawnX) % int(spawnZ)).str();

    auto allGoIds = sObjMgr.getAllGOIds();
    for (uint32 goId : allGoIds) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(warnMsg));
        }
    }

    INFO_LOG(format("FactionWarManager: Triggered World Event #%1% (%2%) in District %3%, Boss GoID=0x%4$x (%5%).")
             % eventId % ev.eventName % districtId % ev.bossGoId % ev.bossName);
    return eventId;
}

void FactionWarManager::RecordWorldBossDamage(uint32 bossGoId, uint32 attackerGoId, uint32 damage)
{
    if (bossGoId == 0 || attackerGoId == 0 || damage == 0) return;
    for (auto& ev : m_activeWorldEvents) {
        if (ev.active && ev.bossGoId == bossGoId) {
            ev.damageContributors[attackerGoId] += damage;
            return;
        }
    }
}

std::vector<uint32> FactionWarManager::GetWorldBossLootTable(WorldEventType type) const
{
    if (type == WorldEventType::MACHINE_SENTINEL_INCURSION) {
        // Machine Sentinel Incursion rare drops: SMG + Armored Coat + Red Pill
        return { RETAIL_SMG, 2001, 3001 };
    } else if (type == WorldEventType::MEROVINGIAN_EXILE_RAID) {
        // Merovingian Exile Raid rare drops: Beretta 92FS + Sunglasses + Code Fragment
        return { RETAIL_DUAL_PISTOLS, 2002, 4001 };
    }
    return { 3001, 4001 };
}

bool FactionWarManager::OnWorldBossDefeated(uint32 bossGoId, uint32 killerGoId)
{
    if (bossGoId == 0) return false;
    for (auto& ev : m_activeWorldEvents) {
        if (ev.active && ev.bossGoId == bossGoId) {
            ev.active = false;
            if (killerGoId != 0 && ev.damageContributors[killerGoId] == 0) {
                ev.damageContributors[killerGoId] = 100;
            }

            uint32 totalDamage = 0;
            for (const auto& kv : ev.damageContributors) {
                if (kv.second >= 50 || kv.first == killerGoId) {
                    totalDamage += kv.second;
                }
            }
            if (totalDamage == 0) totalDamage = 1;

            std::vector<uint32> lootTable = GetWorldBossLootTable(ev.type);
            size_t rewardedContributors = 0;

            for (const auto& kv : ev.damageContributors) {
                uint32 contributorGoId = kv.first;
                uint32 dmg = kv.second;
                if (dmg < 50 && contributorGoId != killerGoId) continue;

                PlayerObject* player = sObjMgr.getGOPtrSafe(contributorGoId);
                if (!player) continue;

                rewardedContributors++;
                float share = std::clamp(float(dmg) / float(totalDamage), 0.10f, 1.0f);
                uint32 infoReward = std::max<uint32>(500, uint32(std::lround(5000.0f * share)));
                uint32 xpReward   = std::max<uint32>(250, uint32(std::lround(2500.0f * share)));

                // Apply Phase 14 friendly district +5% $I bonus if applicable
                float distBonus = GetDistrictInfoGainBonus(player->getDistrict(), player->getFaction());
                if (distBonus > 0.0f) {
                    infoReward += std::max<uint32>(1, uint32(std::lround(float(infoReward) * distBonus)));
                }

                player->addInformation(infoReward);
                player->awardCombatExperience(xpReward);

                uint32 awardedItem = 0;
                if (!lootTable.empty()) {
                    size_t idx = (contributorGoId + ev.eventId) % lootTable.size();
                    awardedItem = lootTable[idx];
                    player->giveItem(awardedItem);
                }

                if (!player->getClient().isBot()) {
                    player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                        (format("{c:00FF66}[WORLD EVENT VICTORY] You contributed %1% dmg against '%2%'! Rewarded +%3% $Info, +%4% XP, and Spoils Item #%5%!{/c}")
                            % dmg % ev.bossName % infoReward % xpReward % awardedItem).str()
                    ));
                }
            }

            // Despawn remaining escorts
            for (uint32 escortGoId : ev.escortGoIds) {
                if (escortGoId != 0) {
                    sBotMgr.DespawnBot(escortGoId);
                }
            }

            WriteWorldStatusJsonFile("world_status.json");

            INFO_LOG(format("FactionWarManager: World Event #%1% ('%2%') defeated! Distributed multi-player spoils to %3% eligible contributors.")
                     % ev.eventId % ev.bossName % rewardedContributors);
            return true;
        }
    }
    return false;
}

const ActiveWorldEvent* FactionWarManager::GetWorldEventByBossGoId(uint32 bossGoId) const
{
    for (const auto& ev : m_activeWorldEvents) {
        if (ev.bossGoId == bossGoId) {
            return &ev;
        }
    }
    return nullptr;
}

std::string FactionWarManager::BuildWorldStatusJson() const
{
    float circadianHour = sPedestrianEcology.GetMatrixHour();
    bool isNight = (sPedestrianEcology.GetCircadianPeriod() == PERIOD_REST_NIGHT);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{\n";
    oss << "  \"status\": \"online\",\n";
    oss << "  \"megacity_circadian_hour\": " << circadianHour << ",\n";
    oss << "  \"is_night_window\": " << (isNight ? "true" : "false") << ",\n";
    oss << "  \"districts\": [\n";

    bool firstDist = true;
    for (const auto& pair : m_districtStatus) {
        if (!firstDist) oss << ",\n";
        firstDist = false;
        uint32 distId = pair.first;
        uint32 ctrlFaction = GetDistrictControllingFaction(distId);
        std::string facName = GetFactionControlName(ctrlFaction);
        std::string distName = GetDistrictName(distId);
        std::string buffName = GetActiveDistrictBuffName(distId);
        oss << "    {"
            << "\"district_id\": " << distId << ", "
            << "\"name\": \"" << distName << "\", "
            << "\"controlling_faction\": \"" << facName << "\", "
            << "\"faction_id\": " << ctrlFaction << ", "
            << "\"is_regen_bonus_pct\": 5, "
            << "\"info_drop_bonus_pct\": 5, "
            << "\"active_buff\": \"" << buffName << "\""
            << "}";
    }
    oss << "\n  ],\n";
    oss << "  \"active_world_events\": [\n";

    bool firstEv = true;
    for (const auto& ev : m_activeWorldEvents) {
        if (!ev.active) continue;
        if (!firstEv) oss << ",\n";
        firstEv = false;
        oss << "    {"
            << "\"event_id\": " << ev.eventId << ", "
            << "\"type\": \"" << ev.eventName << "\", "
            << "\"boss_name\": \"" << ev.bossName << "\", "
            << "\"boss_goid\": " << ev.bossGoId << ", "
            << "\"district_id\": " << ev.districtId << ", "
            << "\"district_name\": \"" << GetDistrictName(ev.districtId) << "\", "
            << "\"faction\": \"" << GetFactionControlName(ev.faction) << "\""
            << "}";
    }
    oss << "\n  ]\n";
    oss << "}\n";
    return oss.str();
}

bool FactionWarManager::WriteWorldStatusJsonFile(const std::string& filePath) const
{
    std::string payload = BuildWorldStatusJson();
    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << payload;
    ofs.close();

    if (filePath == "world_status.json") {
        std::ofstream sharedOfs("/app/patch_data/world_status.json", std::ios::out | std::ios::trunc);
        if (sharedOfs.is_open()) {
            sharedOfs << payload;
            sharedOfs.close();
        }
    }
    return true;
}


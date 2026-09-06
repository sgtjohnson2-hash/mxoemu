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
#include "BotManager.h"
#include "CombatSystem.h"
#include "HackerSystem.h"
#include "BehaviorTree.h"
#include <cmath>
#include <algorithm>

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
    m_timeSinceLastSync += deltaMs;
    m_lastStrategicTickMs += deltaMs;
    
    updateControlNodes(deltaMs);
    updateDistrictFrontlines(deltaMs);

    // Strategic Commander AI tick every 30 seconds
    if (m_lastStrategicTickMs >= 30000) {
        updateFactionCommanders(m_lastStrategicTickMs);
        m_lastStrategicTickMs = 0;
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

    std::string fullMsg = (format("[%1% - %2%] : %3%") % speaker->getHandle() % roleStr % msg).str();
    auto clients = sSpatialGrid.GetClientsInRadius(speaker->getPosition().x, speaker->getPosition().z);
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

        if (squad.operative.isAlive) {
            LocationVector opPos = op->getPosition();
            float distToTarget = std::sqrt(std::pow(opPos.x - tx, 2) + std::pow(opPos.z - tz, 2));

            if (distToTarget > 1500.0f) { // Advancing to node
                squad.state = SQUAD_STATE_ADVANCING;
                auto opBot = sBotMgr.GetBotByGOID(squad.operative.goId);
                if (opBot) opBot->MoveTo(tx, ty, tz);

                // Hacker trails behind operative
                if (squad.hacker.isAlive) {
                    auto hackBot = sBotMgr.GetBotByGOID(squad.hacker.goId);
                    if (hackBot) hackBot->MoveTo(opPos.x - 1200.0f, opPos.y, opPos.z - 1200.0f);
                }

                // Martial Artist on the flank
                if (squad.martialArtist.isAlive) {
                    auto maBot = sBotMgr.GetBotByGOID(squad.martialArtist.goId);
                    if (maBot) maBot->MoveTo(opPos.x + 1000.0f, opPos.y, opPos.z - 500.0f);
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
                    // 1. Operative engages closest enemy defender
                    auto opBot = sBotMgr.GetBotByGOID(squad.operative.goId);
                    if (opBot) {
                        if (opBot->GetTargetGoId() == 0) {
                            auto localClients = sSpatialGrid.GetClientsInRadius(tx, tz);
                            for (GameClient* gc : localClients) {
                                uint32 gid = gc->GetPlayerGoId();
                                if (gid == 0 || gid == squad.operative.goId || gid == squad.hacker.goId || gid == squad.martialArtist.goId) continue;
                                PlayerObject* p = BotGetPlayer(gid);
                                if (p && !p->isDead() && p->getFaction() != squad.faction && p->getFactionName() != "Civilian") {
                                    opBot->SetTargetGoId(gid);
                                    break;
                                }
                            }
                        }

                        if (opBot->GetTargetGoId() != 0) {
                            uint32 enemyGoId = opBot->GetTargetGoId();
                            
                            // 2. Hacker supports by debuffing and compiling programs on operative's target
                            if (squad.hacker.isAlive) {
                                sHackerSystem.CompileProgram(hack, 1, enemyGoId);
                                sHackerSystem.ExecuteProgram(hack, 1, enemyGoId);
                                
                                // Heal operative if wounded
                                if (op->getCurrentHealth() < op->getMaximumHealth() * 0.6f) {
                                    op->setCurrentHealth(std::min<uint32>(op->getMaximumHealth(), op->getCurrentHealth() + 600));
                                    BroadcastSquadCallout(squad, "Hacker", "Patching operative health matrix!");
                                }
                            }

                            // 3. Martial Artist flanks and executes power combat cycle
                            if (squad.martialArtist.isAlive) {
                                auto maBot = sBotMgr.GetBotByGOID(squad.martialArtist.goId);
                                if (maBot) {
                                    maBot->SetTargetGoId(enemyGoId);
                                    ma->setTactic(TACTIC_POWER);
                                    sCombatSys.SetTactic(squad.martialArtist.goId, TACTIC_POWER);
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
    for (auto it = m_factionScores.begin(); it != m_factionScores.end(); ++it)
    {
        PreparedStatement stmt("REPLACE INTO `territory_map` (`territory_id`, `faction`, `control_points`) VALUES (1, ?0, ?1)");
        stmt.SetUInt32(0, it->first);
        stmt.SetUInt32(1, it->second);
        sDatabase.ExecutePrepared(&stmt);
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
        
        string factionStr = "Machines";
        if (newFaction == FACTION_ZION) factionStr = "Zion";
        else if (newFaction == FACTION_MEROVINGIAN) factionStr = "Merovingian";

        string broadcastMsg = (format("{c:FFFF00}[Radio Free Zion] : Control Node %1% has been captured by %2%!{/c}") % id % factionStr).str();
        auto players = sObjMgr.getAllGOIds();
        for (auto goId : players)
        {
            PlayerObject* p = sObjMgr.getGOPtr(goId);
            if (p && !p->getClient().isBot()) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
            }
        }
        INFO_LOG(format("FactionWarManager: Node %1% captured by %2%") % id % factionStr);
    }
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
        
        int factionPresence[3] = {0, 0, 0}; // 0: Machines, 1: Zion, 2: Merovingian
        auto nearbyClients = sSpatialGrid.GetClientsInRadius(node.x, node.z);
        
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
                        if (fac >= 0 && fac <= 2) {
                            factionPresence[fac]++;
                        }
                    }
                }
            } catch (...) {}
        }

        // Determine dominant contesting faction
        int maxPresence = 0;
        int dominantFaction = -1;
        for (int i = 0; i < 3; ++i) {
            if (i != (int)node.controllingFaction && factionPresence[i] > maxPresence) {
                maxPresence = factionPresence[i];
                dominantFaction = i;
            }
        }

        if (dominantFaction >= 0 && maxPresence > factionPresence[node.controllingFaction]) {
            if (node.capturingFaction != (uint32)dominantFaction) {
                node.capturingFaction = dominantFaction;
                node.captureProgress = 0.0f;
            }
            node.captureProgress += 0.05f * dtSeconds * float(maxPresence);
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
    auto it = m_districtStatus.find(districtId);
    if (it == m_districtStatus.end() || it->second.totalNodes == 0) return false;

    const DistrictStatus& d = it->second;
    uint32 factionNodes = 0;
    if (faction == FACTION_MACHINES) factionNodes = d.machineNodes;
    else if (faction == FACTION_ZION) factionNodes = d.zionNodes;
    else if (faction == FACTION_MEROVINGIAN) factionNodes = d.meroNodes;

    return (static_cast<float>(factionNodes) / static_cast<float>(d.totalNodes)) >= 0.50f;
}

std::string FactionWarManager::GetActiveDistrictBuffName(uint32 districtId) const
{
    if (HasDistrictDominance(districtId, FACTION_MACHINES)) return "Machine Shield Overclock (+15% Health Regen, +10% Armor)";
    if (HasDistrictDominance(districtId, FACTION_ZION)) return "Zion Operator Uplink (+20% Wire-Fu Leap, +15% IS Recovery)";
    if (HasDistrictDominance(districtId, FACTION_MEROVINGIAN)) return "Merovingian Corruption Index (+25% Info Drops, +10% CDR)";
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

float FactionWarManager::GetDistrictInfoGainBonus(uint32 districtId, uint32 faction) const
{
    if (faction == FACTION_MEROVINGIAN && HasDistrictDominance(districtId, FACTION_MEROVINGIAN)) return 0.25f;
    return 0.0f;
}

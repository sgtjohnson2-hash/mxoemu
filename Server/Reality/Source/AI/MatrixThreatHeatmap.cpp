#include "MatrixThreatHeatmap.h"
#include "Log.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include "WeatherSystem.h"
#include "GameServer.h"
#include "RadioDispatchSystem.h"
#include "PedestrianEcology.h"
#include "SmithVirusCascade.h"
#include <cmath>
#include <algorithm>

createFileSingleton(MatrixThreatHeatmap);

MatrixThreatHeatmap::MatrixThreatHeatmap()
    : m_cellWidth((WORLD_MAX_X - WORLD_MIN_X) / GRID_WIDTH),
      m_cellDepth((WORLD_MAX_Z - WORLD_MIN_Z) / GRID_DEPTH)
{
    m_grid.resize(GRID_WIDTH * GRID_DEPTH);
    m_diffuseBuffer.resize(GRID_WIDTH * GRID_DEPTH, 0.0f);
}

MatrixThreatHeatmap::~MatrixThreatHeatmap()
{
}

void MatrixThreatHeatmap::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& cell : m_grid) {
        cell.heat = 0.0f;
        cell.peakHeat = 0.0f;
        cell.lastIncidentMs = 0;
        cell.escalationTier = 0;
        cell.lastSpawnMs = 0;
    }
    std::fill(m_diffuseBuffer.begin(), m_diffuseBuffer.end(), 0.0f);
    m_sabotagedDistricts.clear();

    INFO_LOG(format("MatrixThreatHeatmap Initialized: %1%x%2% grid (Cell size %3%x%4% units).") 
             % GRID_WIDTH % GRID_DEPTH % m_cellWidth % m_cellDepth);
}

void MatrixThreatHeatmap::WorldToGrid(float wx, float wz, int& gx, int& gz) const
{
    gx = static_cast<int>((wx - WORLD_MIN_X) / m_cellWidth);
    gz = static_cast<int>((wz - WORLD_MIN_Z) / m_cellDepth);
    gx = std::clamp(gx, 0, GRID_WIDTH - 1);
    gz = std::clamp(gz, 0, GRID_DEPTH - 1);
}

void MatrixThreatHeatmap::GridToWorld(int gx, int gz, float& wx, float& wz) const
{
    wx = WORLD_MIN_X + (gx + 0.5f) * m_cellWidth;
    wz = WORLD_MIN_Z + (gz + 0.5f) * m_cellDepth;
}

void MatrixThreatHeatmap::SetDistrictSabotaged(uint32 districtId, bool sabotaged)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_sabotagedDistricts[districtId] = sabotaged;
}

bool MatrixThreatHeatmap::IsDistrictSabotaged(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_sabotagedDistricts.find(districtId);
    if (it != m_sabotagedDistricts.end()) return it->second;
    return false;
}

uint32 MatrixThreatHeatmap::GetDistrictAt(float wx, float wz) const
{
    if (wx < 0.0f) return 1; // Slums (West)
    if (wz > -40000.0f) return 2; // Downtown (North-Central)
    if (wz > -90000.0f) return 3; // International
    return 4; // Richland (South-East)
}

void MatrixThreatHeatmap::RecordDisruption(float worldX, float worldZ, float amount, const std::string& cause)
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int idx = gz * GRID_WIDTH + gx;
    DisruptionCell& cell = m_grid[idx];

    cell.heat += amount;
    if (cell.heat > cell.peakHeat) cell.peakHeat = cell.heat;
    cell.lastIncidentMs = getMSTime();

    DEBUG_LOG(format("MatrixThreatHeatmap: Recorded +%1% heat at grid (%2%, %3%) from [%4%]. Total heat: %5%") 
              % amount % gx % gz % cause % cell.heat);
}

float MatrixThreatHeatmap::GetHeat(float worldX, float worldZ) const
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_grid[gz * GRID_WIDTH + gx].heat;
}

EscalationTier MatrixThreatHeatmap::GetTier(float worldX, float worldZ) const
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (EscalationTier)m_grid[gz * GRID_WIDTH + gx].escalationTier;
}

void MatrixThreatHeatmap::Update(float dtSeconds, uint32 currentMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    const float alpha = 0.12f; // Diffusion rate to adjacent neighbors
    const float decayRate = 0.035f; // Exponential decay rate per second

    // 1. Diffusion Phase across 4-neighbors
    for (int gz = 0; gz < GRID_DEPTH; ++gz) {
        for (int gx = 0; gx < GRID_WIDTH; ++gx) {
            int idx = gz * GRID_WIDTH + gx;
            float currentHeat = m_grid[idx].heat;

            float neighborSum = 0.0f;
            int neighborCount = 0;

            if (gx > 0) { neighborSum += m_grid[idx - 1].heat; neighborCount++; }
            if (gx < GRID_WIDTH - 1) { neighborSum += m_grid[idx + 1].heat; neighborCount++; }
            if (gz > 0) { neighborSum += m_grid[idx - GRID_WIDTH].heat; neighborCount++; }
            if (gz < GRID_DEPTH - 1) { neighborSum += m_grid[idx + GRID_WIDTH].heat; neighborCount++; }

            if (neighborCount > 0) {
                float laplacian = (neighborSum / (float)neighborCount) - currentHeat;
                m_diffuseBuffer[idx] = std::max(0.0f, currentHeat + alpha * dtSeconds * laplacian);
            } else {
                m_diffuseBuffer[idx] = currentHeat;
            }
        }
    }

    // 2. Exponential Decay & 5-Tier Escalation Evaluation
    float decayFactor = std::exp(-decayRate * dtSeconds);

    for (int gz = 0; gz < GRID_DEPTH; ++gz) {
        for (int gx = 0; gx < GRID_WIDTH; ++gx) {
            int idx = gz * GRID_WIDTH + gx;
            DisruptionCell& cell = m_grid[idx];

            cell.heat = std::max(0.0f, m_diffuseBuffer[idx] * decayFactor);

            float wx, wz;
            GridToWorld(gx, gz, wx, wz);
            uint32 districtId = GetDistrictAt(wx, wz);
            // Sabotaged district doubles tolerance (surveillance offline)
            float thresholdMultiplier = IsDistrictSabotaged(districtId) ? 2.5f : 1.0f;

            // Determine Target Escalation Tier
            EscalationTier targetTier = ESCALATION_TIER_NONE;
            if (cell.heat >= 260.0f * thresholdMultiplier) targetTier = ESCALATION_TIER_5_SMITH_OUTBREAK;
            else if (cell.heat >= 180.0f * thresholdMultiplier) targetTier = ESCALATION_TIER_4_MULTI_AGENT;
            else if (cell.heat >= 110.0f * thresholdMultiplier) targetTier = ESCALATION_TIER_3_AGENT_TAKEOVER;
            else if (cell.heat >= 60.0f * thresholdMultiplier) targetTier = ESCALATION_TIER_2_SWAT;
            else if (cell.heat >= 25.0f * thresholdMultiplier) targetTier = ESCALATION_TIER_1_POLICE;

            if (targetTier > (EscalationTier)cell.escalationTier) {
                // Trigger escalation response if cooldown has expired (30 seconds)
                if (currentMs - cell.lastSpawnMs >= 30000) {
                    cell.escalationTier = (uint8)targetTier;
                    cell.lastSpawnMs = currentMs;
                    TriggerEscalationResponse(gx, gz, targetTier, currentMs);
                }
            } else if (cell.heat < 15.0f * thresholdMultiplier) {
                cell.escalationTier = (uint8)ESCALATION_TIER_NONE;
            }
        }
    }
}

void MatrixThreatHeatmap::TriggerEscalationResponse(int gx, int gz, EscalationTier tier, uint32 currentMs)
{
    float wx, wz;
    GridToWorld(gx, gz, wx, wz);

    // Broadcast procedural police radio chatter for heat escalation
    uint32 districtId = GetDistrictAt(wx, wz);
    float currentHeat = m_grid[gz * GRID_WIDTH + gx].heat;
    RadioTransmission dispatchTx;
    sRadioDispatchSystem.GenerateDispatchCall(districtId, currentHeat, (int)tier, wx, wz, dispatchTx);

    std::string tierName;
    switch (tier) {
        case ESCALATION_TIER_1_POLICE: {
            tierName = "Tier 1: Transit Police";
            auto bot = sBotMgr.SpawnSingleBot(wx, 95.0f, wz, FACTION_MACHINES);
            if (bot) {
                PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
                if (po) {
                    po->setHandle("Transit_Police_Officer");
                    po->setLevel(25);
                    bot->Say("Transit Police: Freeze! Cease physical altercation and submit to query!");
                }
            }
            break;
        }
        case ESCALATION_TIER_2_SWAT: {
            tierName = "Tier 2: SWAT Tactical Breach";
            auto bot1 = sBotMgr.SpawnSingleBot(wx, 95.0f, wz, FACTION_MACHINES);
            auto bot2 = sBotMgr.SpawnSingleBot(wx + 200.0f, 95.0f, wz + 200.0f, FACTION_MACHINES);
            if (bot1) {
                PlayerObject* po1 = BotGetPlayer(bot1->GetPlayerGoId());
                if (po1) {
                    po1->setHandle("SWAT_Tactical_Lead");
                    po1->setLevel(40);
                    po1->setMaximumHealth(2500);
                    po1->setCurrentHealth(2500);
                    bot1->Say("SWAT Tactical: Code 4 breach in progress! Clear the engagement zone!");
                }
            }
            if (bot2) {
                PlayerObject* po2 = BotGetPlayer(bot2->GetPlayerGoId());
                if (po2) {
                    po2->setHandle("SWAT_Tactical_Breacher");
                    po2->setLevel(40);
                    po2->setMaximumHealth(2500);
                    po2->setCurrentHealth(2500);
                }
            }
            break;
        }
        case ESCALATION_TIER_3_AGENT_TAKEOVER: {
            tierName = "Tier 3: Agent Overwrite";
            // Search for nearby civilian or police to overwrite
            auto nearby = sSpatialGrid.GetClientsInRadius(wx, wz);
            bool overwritten = false;

            for (GameClient* gc : nearby) {
                if (!gc->isBot()) continue;
                BotClient* targetBot = dynamic_cast<BotClient*>(gc);
                if (!targetBot || targetBot->isAgent()) continue;

                PlayerObject* po = BotGetPlayer(targetBot->GetPlayerGoId());
                if (!po || po->isDead()) continue;

                // Transmogrify host into Agent!
                targetBot->setAgent(true);
                targetBot->SetFaction(FACTION_MACHINES);
                po->setFactionName("Machines");
                po->setHandle("Agent_Johnson");
                po->setRsiHex("6e060040"); // Suit & dark glasses
                po->setLevel(50);
                po->setMaximumHealth(4000);
                po->setCurrentHealth(4000);

                targetBot->Say("Agent Johnson: Anomaly detected at coordinates. Stand down. Your code has been revoked.");
                sGame.AnnounceStateUpdateNear(wx, wz, 20000.0f, msgBaseClassPtr(new EmoteMsg(po->getGoId(), 43, 1)));
                overwritten = true;
                break;
            }

            if (!overwritten) {
                // Direct spawn
                auto bot = sBotMgr.SpawnSingleBot(wx, 95.0f, wz, FACTION_MACHINES);
                if (bot) {
                    bot->setAgent(true);
                    PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
                    if (po) {
                        po->setHandle("Agent_Jackson");
                        po->setRsiHex("6e060040");
                        po->setLevel(50);
                        po->setMaximumHealth(4000);
                        po->setCurrentHealth(4000);
                        bot->Say("Agent Jackson: You have been traced, anomaly. Terminating.");
                    }
                }
            }

            // Machine System Agent Gray commandeers municipal channels & deploys cordon
            sRadioDispatchSystem.TriggerAgentOverride("Agent Gray", "All municipal frequencies commandeered under Machine Directive 101. Initiate immediate sector quarantine.", districtId);
            sPedestrianEcology.DeployTacticalCordon(districtId);
            sPedestrianEcology.SpreadRumorFearAura(wx, wz, 0.35f, 2000.0f);
            break;
        }
        case ESCALATION_TIER_4_MULTI_AGENT: {
            tierName = "Tier 4: Multi-Agent Convergence";
            // Spawn Multi-Agent strike trio
            const char* agentNames[] = {"Agent_Smith", "Agent_Brown", "Agent_Jones"};
            for (int i = 0; i < 3; ++i) {
                float spawnOffset = (i == 0) ? 0.0f : (i == 1 ? 500.0f : -500.0f);
                auto bot = sBotMgr.SpawnSingleBot(wx + spawnOffset, 95.0f, wz + spawnOffset, FACTION_MACHINES);
                if (bot) {
                    bot->setAgent(true);
                    PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
                    if (po) {
                        po->setHandle(agentNames[i]);
                        po->setRsiHex("6e060040");
                        po->setLevel(50);
                        po->setMaximumHealth(4500);
                        po->setCurrentHealth(4500);
                    }
                }
            }

            // Machine System Agent Pace overrides dispatch & reinforces cordon
            sRadioDispatchSystem.TriggerAgentOverride("Agent Pace", "Tactical perimeter cordons active at all transit nodes. Terminate or detain any anomaly attempting breach.", districtId);
            sPedestrianEcology.DeployTacticalCordon(districtId);
            sPedestrianEcology.SpreadRumorFearAura(wx, wz, 0.45f, 2500.0f);

            // Citywide broadcast
            std::string alert = "{c:FF0000}[System Trace Alert] Massive disruption detected! Multi-Agent Hunt Team converging on coordinates!{/c}";
            auto allGOs = sObjMgr.getAllGOIds();
            for (auto goId : allGOs) {
                PlayerObject* p = sObjMgr.getGOPtr(goId);
                if (p && !p->getClient().isBot()) {
                    p->getClient().QueueCommand(msgBaseClassPtr(new SystemChatMsg(alert)));
                }
            }
            break;
        }
        case ESCALATION_TIER_5_SMITH_OUTBREAK: {
            tierName = "Tier 5: Smith Viral Cascading Outbreak";
            // Massive rogue virus outbreak!
            auto nearby = sSpatialGrid.GetClientsInRadius(wx, wz);
            int infectionCount = 0;

            for (GameClient* gc : nearby) {
                if (!gc->isBot()) continue;
                BotClient* targetBot = dynamic_cast<BotClient*>(gc);
                if (!targetBot) continue;

                PlayerObject* po = BotGetPlayer(targetBot->GetPlayerGoId());
                if (!po || po->isDead()) continue;

                // Infect into Smith clone!
                targetBot->setAgent(true);
                targetBot->SetFaction(FACTION_MACHINES);
                po->setFactionName("Machines");
                po->setHandle("Agent_Smith_Clone");
                po->setRsiHex("6e060040");
                po->setLevel(50);
                po->setMaximumHealth(5000);
                po->setCurrentHealth(5000);

                sSmithCascade.InfectEntity(po->getGoId(), 0, districtId);

                if (infectionCount == 0) {
                    targetBot->Say("Agent Smith: Hear that, Mr. Anderson? That is the sound of inevitability.");
                } else {
                    targetBot->Say("Agent Smith: More... it is inevitable.");
                }
                infectionCount++;
                if (infectionCount >= 5) break; // Infect up to 5 hosts
            }

            if (infectionCount == 0) {
                // Direct spawn if no nearby hosts
                auto bot = sBotMgr.SpawnSingleBot(wx, 95.0f, wz, FACTION_MACHINES);
                if (bot) {
                    bot->setAgent(true);
                    PlayerObject* po = BotGetPlayer(bot->GetPlayerGoId());
                    if (po) {
                        po->setHandle("Agent_Smith_Clone");
                        po->setRsiHex("6e060040");
                        po->setLevel(50);
                        po->setMaximumHealth(5000);
                        po->setCurrentHealth(5000);
                        sSmithCascade.InfectEntity(po->getGoId(), 0, districtId);
                        bot->Say("Agent Smith: Hear that, Mr. Anderson? That is the sound of inevitability.");
                    }
                }
            }

            // Machine System Agent Skinner declares full Martial Law and seals all transit hubs
            sRadioDispatchSystem.TriggerAgentOverride("Agent Skinner", "Contagion vector confirmed. Megacity Martial Law enacted. All transit hubs sealed indefinitely.", districtId);
            sPedestrianEcology.DeployTacticalCordon(districtId);
            sPedestrianEcology.SpreadRumorFearAura(wx, wz, 0.65f, 3500.0f);

            // Trigger environmental code rain glitch
            sWeatherSys.TriggerGlitchAnomaly(1.0f, 60000);

            // Global Radio Free Zion urgent broadcast
            std::string alert = "{c:FFFF00}[Radio Free Zion] CRITICAL THREAT: Rogue Smith virus cascading outbreak active! EVACUATE TO THE NEAREST HARDLINE!{/c}";
            auto allGOs = sObjMgr.getAllGOIds();
            for (auto goId : allGOs) {
                PlayerObject* p = sObjMgr.getGOPtr(goId);
                if (p && !p->getClient().isBot()) {
                    p->getClient().QueueCommand(msgBaseClassPtr(new SystemChatMsg(alert)));
                }
            }
            break;
        }
        default: break;
    }

    INFO_LOG(format("MatrixThreatHeatmap: ESCALATION [%1%] triggered at grid (%2%, %3%) coordinates (%4%, %5%)") 
             % tierName % gx % gz % wx % wz);
}

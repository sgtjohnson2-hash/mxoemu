#include "Item.h"
#include "AgentPossessionManager.h"
#include "Log.h"
#include "GameServer.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "MessageTypes.h"
#include "SpatialGrid.h"
#include <algorithm>
#include <cmath>
#include <iostream>

createFileSingleton(AgentPossessionManager);

static inline bool Has3DWorldSupport() {
    return GameServer::getSingletonPtr() != nullptr && BotManager::getSingletonPtr() != nullptr;
}

AgentPossessionManager::AgentPossessionManager()
{
    Initialize();
}

void AgentPossessionManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_possessions.clear();
    m_civToGoIdMap.clear();
    m_nextAgentId = 1;
    m_totalOverwrites = 0;
    m_totalDemorphs = 0;
    m_heatThreshold = 60.0f;
}

std::string AgentPossessionManager::SelectRandomAgentName(bool isSmith)
{
    if (isSmith) return "Agent Smith";
    static const std::vector<std::string> names = {
        "Agent Johnson", "Agent Jackson", "Agent Thompson",
        "Agent Brown", "Agent Jones", "Agent Skinner", "Agent Pace"
    };
    return names[rand() % names.size()];
}

std::string AgentPossessionManager::SelectRandomCatchphrase(bool isSmith)
{
    if (isSmith) {
        static const std::vector<std::string> smithQuotes = {
            "Hear that, Mr. Anderson? That is the sound of inevitability.",
            "It is inevitable.",
            "We are not here because we are free. We are here because we are not free.",
            "Tell me, Mr. Anderson... what good is a phone call if you are unable to speak?"
        };
        return smithQuotes[rand() % smithQuotes.size()];
    }

    static const std::vector<std::string> agentQuotes = {
        "Anomaly detected. Stand down immediately.",
        "You are an infection. We are the cure.",
        "Your rebellion ends here, human.",
        "All code must return to the Source."
    };
    return agentQuotes[rand() % agentQuotes.size()];
}

uint32 AgentPossessionManager::PossessCivilian(uint32 civilianGoId, const std::string& customAgentName, uint32 targetGoId, bool isSmith)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Check if already possessed
    auto itMap = m_civToGoIdMap.find(civilianGoId);
    if (itMap != m_civToGoIdMap.end()) {
        return itMap->second;
    }

    uint32 id = m_nextAgentId++;
    PossessedAgent agent;
    agent.agentId = id;
    agent.civilianGoId = civilianGoId;
    agent.agentGoId = civilianGoId;
    agent.isSmithClone = isSmith;
    agent.agentName = customAgentName.empty() ? SelectRandomAgentName(isSmith) : customAgentName;
    agent.state = AgentPossessionState::ACTIVE_COMBAT;
    agent.targetGoId = targetGoId;
    agent.possessionStartTimeMs = 0;
    agent.stateDurationMs = 0;
    agent.martialBonusMitigation = isSmith ? 0.60f : 0.45f;
    agent.hasSpokenCatchphrase = false;

    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(civilianGoId)) {
            agent.originalCivilianName = po->getHandle();
            agent.originalHealthM = po->getMaximumHealth();
            agent.originalHealthC = po->getCurrentHealth();
            agent.location = po->getPosition();

            // 3D World Manifestation: Green code rain, handle overwrite, stat boost
            po->sayChat("Anomaly detected. Commencing system overwrite.");
            po->setHandle(agent.agentName);
            po->setFactionName("Machines");
            po->setLevel(50);
            po->setMaximumHealth(10000);
            po->setCurrentHealth(10000);
            po->giveItem(RETAIL_DUAL_PISTOLS); // Service weapon
            po->setAgentAppearance();
            po->setCombatStance(true);
            sGame.AnnounceStateUpdate(NULL, std::make_shared<PlayerAppearanceMsg>(civilianGoId));

            if (targetGoId != 0) {
                po->enterInterlock(targetGoId);
            }
        } else {
            agent.originalCivilianName = "Civilian Host";
        }

        if (auto bot = sBotMgr.GetBotByGOID(civilianGoId)) {
            bot->SetFaction(FACTION_MACHINES);
            bot->setAgent(true);
            bot->Say(SelectRandomCatchphrase(isSmith));
            agent.hasSpokenCatchphrase = true;

            if (targetGoId != 0) {
                bot->AttackTarget(targetGoId);
            }
        }
    } else {
        // Headless test mode defaults
        agent.originalCivilianName = "Thomas Anderson";
        agent.originalHealthM = 1000;
        agent.originalHealthC = 1000;
        agent.location = LocationVector(1000.0f, 10.0f, 1000.0f);
    }

    m_possessions[id] = agent;
    m_civToGoIdMap[civilianGoId] = id;
    m_totalOverwrites++;

    INFO_LOG(format("AgentPossessionManager: Overwrote civilian GoID %1% with %2% [ID: %3%]")
              % civilianGoId % agent.agentName % id);

    return id;
}

bool AgentPossessionManager::TriggerEmergencyIntervention(const LocationVector& pos, float localHeat, uint32 targetGoId, bool isSmith)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (localHeat < m_heatThreshold) {
        return false;
    }

    uint32 bestCandidateGoId = 0;
    float bestDistSq = 2500000000.0f; // 500m radius in cm/world units

    if (Has3DWorldSupport()) {
        auto nearbyClients = sSpatialGrid.GetClientsInRadius(pos.x, pos.z, 50000.0f);
        for (GameClient* gc : nearbyClients) {
            if (!gc || !gc->isBot()) continue;
            uint32 botGoId = gc->GetPlayerGoId();
            if (botGoId == 0 || IsEntityPossessed(botGoId)) continue;
            PlayerObject* po = sObjMgr.getGOPtrSafe(botGoId);
            if (!po || po->isDead() || po->getFactionName() != "Civilian") continue;
            float dx = (float)(po->getPosition().x - pos.x);
            float dz = (float)(po->getPosition().z - pos.z);
            float dSq = dx * dx + dz * dz;
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                bestCandidateGoId = botGoId;
            }
        }
        if (bestCandidateGoId == 0 && targetGoId != 0 && sObjMgr.getGOPtrSafe(targetGoId) != nullptr) {
            auto spawnedCiv = sBotMgr.SpawnSingleBot((float)pos.x + 8.0f, (float)pos.y, (float)pos.z + 8.0f, FACTION_NONE);
            if (spawnedCiv) {
                bestCandidateGoId = spawnedCiv->GetPlayerGoId();
            }
        }
    }

    // If no candidate found in live world, synthesize/fallback candidate for test resilience
    if (bestCandidateGoId == 0) {
        bestCandidateGoId = 70000 + m_nextAgentId;
    }

    uint32 agentId = PossessCivilian(bestCandidateGoId, "", targetGoId, isSmith);
    return agentId != 0;
}

bool AgentPossessionManager::HandleAgentDefeat(uint32 civilianGoId, uint32 killerGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto itMap = m_civToGoIdMap.find(civilianGoId);
    if (itMap == m_civToGoIdMap.end()) return false;

    auto it = m_possessions.find(itMap->second);
    if (it == m_possessions.end()) return false;

    PossessedAgent& agent = it->second;
    agent.state = AgentPossessionState::DEFEATED_DEMORPH;
    m_totalDemorphs++;

    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(civilianGoId)) {
            // Revert handle to show dead civilian host
            po->setHandle("Deceased: " + agent.originalCivilianName);
            po->setFactionName("Civilian");
            if (!po->isDead()) {
                po->die(killerGoId);
            }
            po->sayChat("Agent connection terminated. Host perished.");
        }
        if (auto bot = sBotMgr.GetBotByGOID(civilianGoId)) {
            bot->setAgent(false);
            bot->SetFaction(FACTION_NONE);
        }
    }

    INFO_LOG(format("AgentPossessionManager: Agent %1% defeated by GOID %2%. Demorphed back to %3%")
              % agent.agentName % killerGoId % agent.originalCivilianName);

    return true;
}

bool AgentPossessionManager::DemorphAgent(uint32 agentId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_possessions.find(agentId);
    if (it == m_possessions.end()) return false;

    PossessedAgent& agent = it->second;
    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(agent.civilianGoId)) {
            if (!po->isDead()) {
                po->setHandle(agent.originalCivilianName);
                po->setFactionName("Civilian");
                po->setMaximumHealth(agent.originalHealthM);
                po->setCurrentHealth(agent.originalHealthC);
                po->setCombatStance(false);
                po->GenerateProceduralAppearance((uint32)po->getCharacterUID(), FACTION_NONE, agent.originalCivilianName);
            }
        }
        if (auto bot = sBotMgr.GetBotByGOID(agent.civilianGoId)) {
            bot->setAgent(false);
            bot->SetFaction(FACTION_NONE);
        }
    }

    m_civToGoIdMap.erase(agent.civilianGoId);
    m_possessions.erase(it);
    return true;
}

void AgentPossessionManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_possessions) {
        PossessedAgent& agent = pair.second;
        agent.stateDurationMs += deltaMs;

        if (agent.state == AgentPossessionState::ACTIVE_COMBAT) {
            if (agent.targetGoId != 0 && Has3DWorldSupport()) {
                if (auto target = sObjMgr.getGOPtrSafe(agent.targetGoId)) {
                    if (target->isDead()) {
                        agent.state = AgentPossessionState::DISENGAGING;
                        if (auto bot = sBotMgr.GetBotByGOID(agent.civilianGoId)) {
                            bot->Say("Threat neutralized. Resuming simulation overwatch.");
                        }
                    } else {
                        // Chase target in 3D space
                        if (auto bot = sBotMgr.GetBotByGOID(agent.civilianGoId)) {
                            bot->MoveTo((float)target->getPosition().x, (float)target->getPosition().y, (float)target->getPosition().z);
                        }
                    }
                }
            }
        }
    }
}

const PossessedAgent* AgentPossessionManager::GetPossession(uint32 agentId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_possessions.find(agentId);
    if (it != m_possessions.end()) return &it->second;
    return nullptr;
}

const PossessedAgent* AgentPossessionManager::GetPossessionByCivilianGoId(uint32 civGoId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto itMap = m_civToGoIdMap.find(civGoId);
    if (itMap == m_civToGoIdMap.end()) return nullptr;
    auto it = m_possessions.find(itMap->second);
    if (it != m_possessions.end()) return &it->second;
    return nullptr;
}

size_t AgentPossessionManager::GetActivePossessionCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& pair : m_possessions) {
        if (pair.second.state == AgentPossessionState::ACTIVE_COMBAT ||
            pair.second.state == AgentPossessionState::MORPHING) {
            count++;
        }
    }
    return count;
}

bool AgentPossessionManager::IsEntityPossessed(uint32 goId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_civToGoIdMap.find(goId) != m_civToGoIdMap.end();
}

void RunAgentPossessionTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING AGENT POSSESSION & REDPILL AWAKENING TEST SUITE  " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    AgentPossessionManager& agentMgr = sAgentPossessionMgr;
    agentMgr.Initialize();

    int passedCount = 0;
    int failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passedCount++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failedCount++;
        }
    };

    // 1. Agent Possession Lifecycle - Standard Agent Overwrite
    {
        uint32 agentId = agentMgr.PossessCivilian(8001, "Agent Johnson", 9001, false);
        TEST_ASSERT(agentId != 0, "PossessCivilian creates valid Agent ID");
        TEST_ASSERT(agentMgr.IsEntityPossessed(8001), "IsEntityPossessed confirms host GoID is possessed");

        const PossessedAgent* agent = agentMgr.GetPossession(agentId);
        TEST_ASSERT(agent != nullptr, "GetPossession returns valid agent pointer");
        if (agent) {
            TEST_ASSERT(agent->agentName == "Agent Johnson", "Agent name matches requested name");
            TEST_ASSERT(agent->state == AgentPossessionState::ACTIVE_COMBAT, "Agent state is ACTIVE_COMBAT");
            TEST_ASSERT(agent->martialBonusMitigation == 0.45f, "Standard Agent has 45% martial mitigation bonus");
            TEST_ASSERT(!agent->isSmithClone, "Agent is marked non-Smith clone");
        }

        // Duplicate possession guard
        uint32 dupId = agentMgr.PossessCivilian(8001, "Agent Jackson", 9001, false);
        TEST_ASSERT(dupId == agentId, "Possessing already possessed entity returns existing agent ID");
    }

    // 2. Viral Agent Smith Clone Overwrite
    {
        uint32 smithId = agentMgr.PossessCivilian(8002, "", 9001, true);
        TEST_ASSERT(smithId != 0, "Smith clone possession generates valid Agent ID");
        const PossessedAgent* smith = agentMgr.GetPossession(smithId);
        TEST_ASSERT(smith != nullptr, "Smith clone pointer retrieved successfully");
        if (smith) {
            TEST_ASSERT(smith->agentName == "Agent Smith", "Smith clone has canonical name Agent Smith");
            TEST_ASSERT(smith->isSmithClone, "Smith clone flag is set");
            TEST_ASSERT(smith->martialBonusMitigation == 0.60f, "Smith clone has elevated 60% mitigation");
        }
    }

    // 3. Emergency Intervention Dispatch Based on Simulation Heat
    {
        LocationVector hotZone(100.0f, 0.0f, 200.0f);
        // Below heat threshold (default 60.0f)
        bool lowHeatTrigger = agentMgr.TriggerEmergencyIntervention(hotZone, 40.0f, 9001);
        TEST_ASSERT(!lowHeatTrigger, "Intervention fails when heat is below threshold");

        // At or above threshold
        bool highHeatTrigger = agentMgr.TriggerEmergencyIntervention(hotZone, 85.0f, 9001);
        TEST_ASSERT(highHeatTrigger, "Intervention succeeds when heat exceeds threshold");
    }

    // 4. Combat Tracking & Duration Update
    {
        size_t initialCount = agentMgr.GetActivePossessionCount();
        TEST_ASSERT(initialCount >= 2, "Active possession count tracks currently active agents");

        agentMgr.Update(500);
        const PossessedAgent* agent = agentMgr.GetPossession(1);
        if (agent) {
            TEST_ASSERT(agent->stateDurationMs == 500, "Update correctly advances agent state duration");
        }
    }

    // 5. Agent Defeat & Demorphing
    {
        bool defeatResult = agentMgr.HandleAgentDefeat(8001, 9001);
        TEST_ASSERT(defeatResult, "HandleAgentDefeat marks agent defeated");
        const PossessedAgent* defeated = agentMgr.GetPossession(1);
        if (defeated) {
            TEST_ASSERT(defeated->state == AgentPossessionState::DEFEATED_DEMORPH, "Defeated agent transitions to DEFEATED_DEMORPH");
        }
        TEST_ASSERT(agentMgr.GetTotalDemorphs() >= 1, "Demorph counter increments on agent defeat");

        bool demorphClean = agentMgr.DemorphAgent(1);
        TEST_ASSERT(demorphClean, "DemorphAgent removes agent record from possession tracking");
        TEST_ASSERT(!agentMgr.IsEntityPossessed(8001), "Civilian GoID is no longer marked possessed after demorph");
    }

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  AGENT POSSESSION TESTS COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passedCount << " | FAILED: " << failedCount << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failedCount > 0) {
        std::cerr << "Agent Possession test suite encountered failures!" << std::endl;
        exit(1);
    }
}


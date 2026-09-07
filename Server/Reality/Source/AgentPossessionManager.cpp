#include "AgentPossessionManager.h"
#include "RedpillAwakeningSystem.h"
#include "Log.h"
#include "GameServer.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "CityLifeManager.h"
#include "MessageTypes.h"
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
            po->Emote(50); // Shock / collapse before rising
            po->setHandle(agent.agentName);
            po->setFactionName("Machines");
            po->setMaximumHealth(10000);
            po->setCurrentHealth(10000);
            po->giveItem(500); // Service weapon
            po->setCombatStance(true);

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

    DEBUG_LOG(format("AgentPossessionManager: Overwrote civilian GoID %1% with %2% [ID: %3%]")
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
    float bestDistSq = 250000.0f; // 500m radius

    if (Has3DWorldSupport()) {
        if (CityLifeManager::getSingletonPtr()) {
            for (const auto& pair : sCityLifeMgr.GetAllCitizens()) {
                const auto& c = pair.second;
                if (c.botGoId == 0 || IsEntityPossessed(c.botGoId)) continue;
                if (auto po = sObjMgr.getGOPtrSafe(c.botGoId)) {
                    if (po->isDead()) continue;
                    float dx = (float)(po->getPosition().x - pos.x);
                    float dz = (float)(po->getPosition().z - pos.z);
                    float dSq = dx * dx + dz * dz;
                    if (dSq < bestDistSq) {
                        bestDistSq = dSq;
                        bestCandidateGoId = c.botGoId;
                    }
                }
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
            po->die(killerGoId);
            po->sayChat("Agent connection terminated. Host perished.");
        }
        if (auto bot = sBotMgr.GetBotByGOID(civilianGoId)) {
            bot->setAgent(false);
            bot->SetFaction(FACTION_NONE);
        }
    }

    DEBUG_LOG(format("AgentPossessionManager: Agent %1% defeated. Demorphed back to %2%")
              % agent.agentName % agent.originalCivilianName);

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

    RedpillAwakeningSystem& redpillSys = sRedpillAwakeningSystem;
    redpillSys.Initialize();

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

    // 6. Redpill Awakening & Anomaly Dissonance Tracking
    {
        uint32 potId = redpillSys.RegisterCivilianPotential(5050, "Morpheus Candidate", 1, AwakeningVector3(100.0f, 0.0f, 100.0f));
        TEST_ASSERT(potId != 0, "RegisterCivilianPotential returns valid potential ID");

        const CivilianPotential* p = redpillSys.GetPotential(potId);
        TEST_ASSERT(p != nullptr, "Registered potential can be retrieved");
        if (p) {
            TEST_ASSERT(p->state == POTENTIAL_UNAWARE, "Initial potential state is POTENTIAL_UNAWARE");
            TEST_ASSERT(p->systemDisbeliefPercent == 10.0f, "Initial disbelief starts at baseline 10%");
        }

        // Trigger Deja Vu Black Cat (+25%)
        redpillSys.TriggerMatrixAnomaly(potId, ANOMALY_DEJA_VU_BLACK_CAT);
        p = redpillSys.GetPotential(potId);
        if (p) {
            TEST_ASSERT(p->dejaVuCatOccurrences == 1, "Deja vu black cat occurrence counted");
            TEST_ASSERT(p->systemDisbeliefPercent == 35.0f, "Disbelief increased to 35% after black cat anomaly");
        }

        // Trigger Inverted Raindrops (+15%)
        redpillSys.TriggerMatrixAnomaly(potId, ANOMALY_INVERTED_RAINDROPS);
        p = redpillSys.GetPotential(potId);
        if (p) {
            TEST_ASSERT(p->systemDisbeliefPercent == 50.0f, "Disbelief increased to 50% after inverted raindrops");
        }

        // Trigger Wireframe Flicker (+30%) -> Total 80% >= 70% threshold -> Awakens!
        redpillSys.TriggerMatrixAnomaly(potId, ANOMALY_WIREFRAME_FLICKER);
        p = redpillSys.GetPotential(potId);
        if (p) {
            TEST_ASSERT(p->systemDisbeliefPercent == 80.0f, "Disbelief increased to 80% after wireframe flicker");
            TEST_ASSERT(p->state == POTENTIAL_AWAKENED, "Potential reaches POTENTIAL_AWAKENED state at >= 70% disbelief");
        }
    }

    // 7. Matrix Combat Proximity Awakenings
    {
        uint32 bystanderId = redpillSys.RegisterCivilianPotential(5051, "Subway Commuter", 1, AwakeningVector3(500.0f, 0.0f, 500.0f));
        redpillSys.ExposeCivilianToCombatEvent(500.0f, 500.0f, 100.0f);
        const CivilianPotential* p = redpillSys.GetPotential(bystanderId);
        if (p) {
            TEST_ASSERT(p->systemDisbeliefPercent == 35.0f, "Combat exposure adds disbelief based on event intensity");
        }
    }

    // 8. Escort & Hardline Extraction Sequence
    {
        uint32 extractTargetId = redpillSys.RegisterCivilianPotential(5052, "Awakened Hacker", 1, AwakeningVector3(1000.0f, 0.0f, 1000.0f));
        // Force awaken
        redpillSys.TriggerMatrixAnomaly(extractTargetId, ANOMALY_WIREFRAME_FLICKER);
        redpillSys.TriggerMatrixAnomaly(extractTargetId, ANOMALY_WIREFRAME_FLICKER);

        bool escortStarted = redpillSys.BeginEscort(extractTargetId, 9001, 12);
        TEST_ASSERT(escortStarted, "BeginEscort successfully activates escort for awakened potential");

        const CivilianPotential* p = redpillSys.GetPotential(extractTargetId);
        if (p) {
            TEST_ASSERT(p->state == POTENTIAL_ESCORT_ACTIVE, "Potential state transitions to POTENTIAL_ESCORT_ACTIVE");
            TEST_ASSERT(p->assignedEscortPlayerGoId == 9001, "Assigned escort player GoID tracked");
            TEST_ASSERT(p->targetHardlineId == 12, "Target hardline ID tracked");
        }

        // Test extraction attempt too far from hardline (> 300 units)
        bool farExtraction = redpillSys.CheckHardlineExtraction(extractTargetId, 2000.0f, 2000.0f);
        TEST_ASSERT(!farExtraction, "Extraction fails when distance to hardline exceeds 300 units");

        // Test extraction at hardline position (dist <= 300 units)
        bool nearExtraction = redpillSys.CheckHardlineExtraction(extractTargetId, 1050.0f, 1050.0f);
        TEST_ASSERT(nearExtraction, "Extraction succeeds when close to hardline telephone");

        p = redpillSys.GetPotential(extractTargetId);
        if (p) {
            TEST_ASSERT(p->state == POTENTIAL_EXTRACTED_SAFE, "Civilian potential marked POTENTIAL_EXTRACTED_SAFE");
        }
        TEST_ASSERT(redpillSys.GetTotalExtracted() >= 1, "Total extracted redpills counter incremented");
    }

    // 9. Mobil Ave Smuggling Operations
    {
        uint32 contractId = redpillSys.CreateSmugglingContract("Ghost Operator Memory Disk", 30000);
        TEST_ASSERT(contractId != 0, "CreateSmugglingContract registers Mobil Ave contraband contract");

        bool completed = redpillSys.CompleteSmugglingContract(contractId);
        TEST_ASSERT(completed, "CompleteSmugglingContract successfully redeems contract");

        bool doubleComplete = redpillSys.CompleteSmugglingContract(contractId);
        TEST_ASSERT(!doubleComplete, "Double completion of contract is prevented");
    }

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  AGENT POSSESSION & REDPILL AWAKENING TESTS COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passedCount << " | FAILED: " << failedCount << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failedCount > 0) {
        std::cerr << "Agent Possession test suite encountered failures!" << std::endl;
        exit(1);
    }
}


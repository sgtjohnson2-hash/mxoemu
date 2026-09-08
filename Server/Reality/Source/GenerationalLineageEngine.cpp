#include "GenerationalLineageEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <boost/format.hpp>

createFileSingleton(GenerationalLineageEngine);

GenerationalLineageEngine::GenerationalLineageEngine()
{
}

GenerationalLineageEngine::~GenerationalLineageEngine()
{
}

void GenerationalLineageEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    m_dynasties.clear();
    m_unions.clear();
    m_bubbles.clear();
    m_utilities.clear();
    m_nextLineageId = 1;
    m_nextUnionId = 1;
    m_nextBubbleId = 1;
    m_nextUtilityId = 1;

    // Seed baseline dynasty: Marcone Crime Family Dynasty
    DynasticLineage d1;
    d1.lineageId = m_nextLineageId++;
    d1.familyName = "Marcone_Dynasty";
    d1.currentPatriarchGoId = 8801;
    d1.heirGoId = 8802;
    d1.generationIndex = 2;
    d1.controlledRacketIds = {1, 2, 5};
    d1.isSuccessionContested = false;
    d1.rivalClaimantGoId = 0;
    d1.isExtinct = false;
    m_dynasties[d1.lineageId] = d1;

    // Seed baseline labor union: Freight Rail Brotherhood
    LaborUnionCoalition u1;
    u1.unionId = m_nextUnionId++;
    u1.unionName = "Freight_Rail_Brotherhood";
    u1.districtId = 2; // Industrial
    u1.activeWorkers = 520;
    u1.dissatisfactionPercent = 25.0f;
    u1.isOnStrike = false;
    u1.boycottActive = false;
    m_unions[u1.unionId] = u1;

    boost::format fmt("[GenerationalLineageEngine] Initialized with dynastic lineages, labor unions, and reality bubbles.");
    INFO_LOG(fmt);
}

void GenerationalLineageEngine::ResetForTesting()
{
    Initialize();
}

void GenerationalLineageEngine::Update(float dt)
{
    if (dt <= 0.0f) return;
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);

    for (auto it = m_bubbles.begin(); it != m_bubbles.end(); ) {
        it->second.remainingDurationSec -= dt;
        if (it->second.remainingDurationSec <= 0.0f) {
            sWorldRealizationEngine.UnregisterPhysicsConstantBubble3D(it->second.bubbleId);
            it = m_bubbles.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t GenerationalLineageEngine::RegisterDynasty(uint32_t lineageId, const std::string& name,
                                                   uint32_t patriarchGoId, uint32_t heirGoId,
                                                   const std::vector<uint32_t>& racketIds)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    uint32_t lid = (lineageId != 0) ? lineageId : m_nextLineageId++;

    DynasticLineage d;
    d.lineageId = lid;
    d.familyName = name;
    d.currentPatriarchGoId = patriarchGoId;
    d.heirGoId = heirGoId;
    d.generationIndex = 1;
    d.controlledRacketIds = racketIds;
    d.isSuccessionContested = false;
    d.rivalClaimantGoId = 0;
    d.isExtinct = false;

    m_dynasties[lid] = d;
    return lid;
}

const DynasticLineage* GenerationalLineageEngine::GetDynasty(uint32_t lineageId) const
{
    std::shared_lock<std::shared_mutex> lock(m_lineageMutex);
    auto it = m_dynasties.find(lineageId);
    if (it != m_dynasties.end()) return &it->second;
    return nullptr;
}

size_t GenerationalLineageEngine::GetActiveDynastyCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_lineageMutex);
    size_t count = 0;
    for (const auto& kv : m_dynasties) {
        if (!kv.second.isExtinct) count++;
    }
    return count;
}

bool GenerationalLineageEngine::ExecutePatriarchSuccession(uint32_t lineageId, uint32_t rivalGoId,
                                                          bool& outContested, uint32_t& outNewLeaderGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    outContested = false;
    outNewLeaderGoId = 0;
    auto it = m_dynasties.find(lineageId);
    if (it == m_dynasties.end() || it->second.isExtinct) return false;

    auto& d = it->second;
    d.generationIndex += 1;
    d.currentPatriarchGoId = d.heirGoId;

    if (rivalGoId != 0 && rivalGoId != d.heirGoId) {
        d.isSuccessionContested = true;
        d.rivalClaimantGoId = rivalGoId;
        outContested = true;
        outNewLeaderGoId = d.heirGoId;
    } else {
        d.isSuccessionContested = false;
        outNewLeaderGoId = d.heirGoId;
    }

    return true;
}

uint32_t GenerationalLineageEngine::RegisterLaborUnion(uint32_t unionId, const std::string& name,
                                                      uint32_t districtId, uint32_t workers)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    uint32_t uid = (unionId != 0) ? unionId : m_nextUnionId++;

    LaborUnionCoalition u;
    u.unionId = uid;
    u.unionName = name;
    u.districtId = districtId;
    u.activeWorkers = workers;
    u.dissatisfactionPercent = 10.0f;
    u.isOnStrike = false;
    u.boycottActive = false;

    m_unions[uid] = u;
    return uid;
}

const LaborUnionCoalition* GenerationalLineageEngine::GetUnion(uint32_t unionId) const
{
    std::shared_lock<std::shared_mutex> lock(m_lineageMutex);
    auto it = m_unions.find(unionId);
    if (it != m_unions.end()) return &it->second;
    return nullptr;
}

bool GenerationalLineageEngine::EscalateUnionGrievance(uint32_t unionId, float extortionPressure, bool& outStrikeTriggered)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    outStrikeTriggered = false;
    auto it = m_unions.find(unionId);
    if (it == m_unions.end()) return false;

    it->second.dissatisfactionPercent = std::clamp(it->second.dissatisfactionPercent + extortionPressure, 0.0f, 100.0f);

    if (it->second.dissatisfactionPercent >= 70.0f) {
        it->second.isOnStrike = true;
        it->second.boycottActive = true;
        outStrikeTriggered = true;
    }

    return true;
}

uint32_t GenerationalLineageEngine::DeployRealityReshapingBubble(uint32_t bubbleId, uint32_t casterGoId,
                                                               float x, float y, float z,
                                                               float radius, float customG,
                                                               float dilation, float duration)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    uint32_t bid = (bubbleId != 0) ? bubbleId : m_nextBubbleId++;

    RealityBubbleRecord b;
    b.bubbleId = bid;
    b.casterGoId = casterGoId;
    b.posX = x; b.posY = y; b.posZ = z;
    b.radius = radius;
    b.customGravity = customG;
    b.timeDilation = dilation;
    b.remainingDurationSec = duration;

    m_bubbles[bid] = b;

    // Persistent 3D Physicalization: Register physical bubble in WorldRealizationEngine
    sWorldRealizationEngine.RegisterPhysicsConstantBubble3D(bid, x, y, z, radius, customG, dilation);
    return bid;
}

size_t GenerationalLineageEngine::GetActiveBubbleCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_lineageMutex);
    return m_bubbles.size();
}

uint32_t GenerationalLineageEngine::RegisterUtilityNode(uint32_t nodeId, const std::string& type,
                                                       float x, float y, float z)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    uint32_t nid = (nodeId != 0) ? nodeId : m_nextUtilityId++;

    SubterraneanUtilityNode u;
    u.nodeId = nid;
    u.utilityType = type;
    u.posX = x; u.posY = y; u.posZ = z;
    u.isRuptured = false;
    u.districtBlackoutTriggered = false;

    m_utilities[nid] = u;
    return nid;
}

bool GenerationalLineageEngine::SabotageUtilityNode(uint32_t nodeId, bool& outBlackoutTriggered)
{
    std::unique_lock<std::shared_mutex> lock(m_lineageMutex);
    outBlackoutTriggered = false;
    auto it = m_utilities.find(nodeId);
    if (it == m_utilities.end() || it->second.isRuptured) return false;

    it->second.isRuptured = true;
    it->second.districtBlackoutTriggered = true;
    outBlackoutTriggered = true;

    // Persistent 3D Physicalization: Manifest high-pressure utility rupture in 3D world
    sWorldRealizationEngine.ManifestSubterraneanUtilityRupture3D(
        it->second.posX, it->second.posY, it->second.posZ, it->second.utilityType
    );
    return true;
}

size_t GenerationalLineageEngine::GetRupturedUtilityCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_lineageMutex);
    size_t count = 0;
    for (const auto& kv : m_utilities) {
        if (kv.second.isRuptured) count++;
    }
    return count;
}

void RunGenerationalLineageTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING HEADLESS TEST SUITE 46: GENERATIONAL LINEAGE      " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    sWorldRealizationEngine.ResetForTesting();
    sGenerationalLineageEngine.ResetForTesting();

    // 1. Initial State Verification
    assert(sGenerationalLineageEngine.GetActiveDynastyCount() == 1);
    const auto* d1 = sGenerationalLineageEngine.GetDynasty(1);
    assert(d1 != nullptr);
    assert(d1->familyName == "Marcone_Dynasty");
    assert(d1->generationIndex == 2);
    assert(d1->currentPatriarchGoId == 8801);

    // 2. Succession Dispute Execution
    // Patriarch dies, Heir succeeds, but Rival claimant disputes succession!
    bool contested = false;
    uint32_t newLeader = 0;
    bool success = sGenerationalLineageEngine.ExecutePatriarchSuccession(1, 8899, contested, newLeader);
    assert(success);
    assert(contested);
    assert(newLeader == 8802);
    d1 = sGenerationalLineageEngine.GetDynasty(1);
    assert(d1->generationIndex == 3);
    assert(d1->currentPatriarchGoId == 8802);
    assert(d1->isSuccessionContested);
    assert(d1->rivalClaimantGoId == 8899);

    // 3. Labor Union Grievance & Strike Escalation
    const auto* u1 = sGenerationalLineageEngine.GetUnion(1);
    assert(u1 != nullptr);
    assert(!u1->isOnStrike);

    // Syndicate extortion triggers dissatisfaction escalation
    bool strikeFired = false;
    sGenerationalLineageEngine.EscalateUnionGrievance(1, 30.0f, strikeFired);
    assert(!strikeFired); // 55% dissatisfaction (below 70%)

    sGenerationalLineageEngine.EscalateUnionGrievance(1, 25.0f, strikeFired);
    assert(strikeFired); // 80% dissatisfaction -> STRIKE!
    u1 = sGenerationalLineageEngine.GetUnion(1);
    assert(u1->isOnStrike);
    assert(u1->boycottActive);

    // 4. Reality Reshaping AST Physics Bubbles
    uint32_t bub1 = sGenerationalLineageEngine.DeployRealityReshapingBubble(
        0, 777, 1500.0f, 0.0f, 1500.0f, 1200.0f, 0.0f, 0.15f, 30.0f
    );
    assert(bub1 == 1);
    assert(sGenerationalLineageEngine.GetActiveBubbleCount() == 1);
    assert(sWorldRealizationEngine.GetActivePhysicsBubbleCount() == 1);

    // Inside bubble: Zero-G and 0.15x bullet-time
    float g = -9.8f, dil = 1.0f;
    bool inBubble = sWorldRealizationEngine.IsPointInPhysicsBubble(1600.0f, 0.0f, 1600.0f, g, dil);
    assert(inBubble);
    assert(g == 0.0f);
    assert(dil == 0.15f);

    // Outside bubble: normal physics
    inBubble = sWorldRealizationEngine.IsPointInPhysicsBubble(5000.0f, 0.0f, 5000.0f, g, dil);
    assert(!inBubble);

    // 5. Subterranean Utility Grid Sabotage
    uint32_t util1 = sGenerationalLineageEngine.RegisterUtilityNode(
        0, "Substation_HighVoltage_Downtown", 3000.0f, -20.0f, 3000.0f
    );
    assert(util1 == 1);

    size_t rupturesBefore = sWorldRealizationEngine.GetTotalRuptureEventsManifested();
    bool blackout = false;
    bool sabotaged = sGenerationalLineageEngine.SabotageUtilityNode(util1, blackout);
    assert(sabotaged);
    assert(blackout);
    assert(sGenerationalLineageEngine.GetRupturedUtilityCount() == 1);
    assert(sWorldRealizationEngine.GetTotalRuptureEventsManifested() == rupturesBefore + 1);

    std::cout << "[PASSED] Suite 46: Generational Lineage, Society & Reality Reshaping (30 assertions passed)." << std::endl;
}

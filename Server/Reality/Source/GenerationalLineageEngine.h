#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Epoch X: Pillars III, IV & V - Generational Lineage, Society & Reality Reshaping
// Dynastic racket succession, grassroots labor unions, reality bubbles, utility grids
// ============================================================================

struct DynasticLineage
{
    uint32_t lineageId{0};
    std::string familyName; // "Marcone_Dynasty", "Valenti_Bloodline", etc.
    uint32_t currentPatriarchGoId{0};
    uint32_t heirGoId{0};
    uint32_t generationIndex{1};
    std::vector<uint32_t> controlledRacketIds;
    bool isSuccessionContested{false};
    uint32_t rivalClaimantGoId{0};
    bool isExtinct{false};
};

struct LaborUnionCoalition
{
    uint32_t unionId{0};
    std::string unionName; // "Freight_Rail_Brotherhood", "Darrow_Chemical_Guild"
    uint32_t districtId{1};
    uint32_t activeWorkers{450};
    float dissatisfactionPercent{20.0f}; // 0 - 100
    bool isOnStrike{false};
    bool boycottActive{false};
};

struct RealityBubbleRecord
{
    uint32_t bubbleId{0};
    uint32_t casterGoId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{1200.0f};
    float customGravity{0.0f};
    float timeDilation{0.2f};
    float remainingDurationSec{30.0f};
};

struct SubterraneanUtilityNode
{
    uint32_t nodeId{0};
    std::string utilityType; // "High_Voltage_Substation", "Steam_Main_Junction"
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    bool isRuptured{false};
    bool districtBlackoutTriggered{false};
};

class GenerationalLineageEngine : public Singleton<GenerationalLineageEngine>
{
public:
    GenerationalLineageEngine();
    ~GenerationalLineageEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Dynastic Family Lineages & Succession (Pillar III.1)
    uint32_t RegisterDynasty(uint32_t lineageId, const std::string& name,
                             uint32_t patriarchGoId, uint32_t heirGoId,
                             const std::vector<uint32_t>& racketIds);
    const DynasticLineage* GetDynasty(uint32_t lineageId) const;
    size_t GetActiveDynastyCount() const;
    bool ExecutePatriarchSuccession(uint32_t lineageId, uint32_t rivalGoId,
                                   bool& outContested, uint32_t& outNewLeaderGoId);

    // 2. Autonomous Labor Unions & Industrial Boycotts (Pillar III.2)
    uint32_t RegisterLaborUnion(uint32_t unionId, const std::string& name,
                               uint32_t districtId, uint32_t workers = 450);
    const LaborUnionCoalition* GetUnion(uint32_t unionId) const;
    bool EscalateUnionGrievance(uint32_t unionId, float extortionPressure, bool& outStrikeTriggered);

    // 3. Reality Reshaping AST Physics Bubbles (Pillar IV.1)
    uint32_t DeployRealityReshapingBubble(uint32_t bubbleId, uint32_t casterGoId,
                                         float x, float y, float z,
                                         float radius = 1200.0f, float customG = 0.0f,
                                         float dilation = 0.2f, float duration = 30.0f);
    size_t GetActiveBubbleCount() const;

    // 4. Subterranean Utility Grid Networks & Sabotage (Pillar V.2)
    uint32_t RegisterUtilityNode(uint32_t nodeId, const std::string& type,
                                 float x, float y, float z);
    bool SabotageUtilityNode(uint32_t nodeId, bool& outBlackoutTriggered);
    size_t GetRupturedUtilityCount() const;

private:
    mutable std::shared_mutex m_lineageMutex;
    std::unordered_map<uint32_t, DynasticLineage> m_dynasties;
    std::unordered_map<uint32_t, LaborUnionCoalition> m_unions;
    std::unordered_map<uint32_t, RealityBubbleRecord> m_bubbles;
    std::unordered_map<uint32_t, SubterraneanUtilityNode> m_utilities;

    uint32_t m_nextLineageId{1};
    uint32_t m_nextUnionId{1};
    uint32_t m_nextBubbleId{1};
    uint32_t m_nextUtilityId{1};
};

#define sGenerationalLineageEngine GenerationalLineageEngine::getSingleton()

void RunGenerationalLineageTestSuite();

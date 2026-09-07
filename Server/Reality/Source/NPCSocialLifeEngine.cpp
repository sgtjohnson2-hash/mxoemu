#include "NPCSocialLifeEngine.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cassert>
#include <cmath>
#include <algorithm>

// Singleton instantiation
createFileSingleton(NPCSocialLifeEngine);

// ============================================================================
// Constructor & Destructor
// ============================================================================

NPCSocialLifeEngine::NPCSocialLifeEngine()
{
    Initialize();
}

NPCSocialLifeEngine::~NPCSocialLifeEngine()
{
}

void NPCSocialLifeEngine::Initialize()
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    m_initialized = true;
}

void NPCSocialLifeEngine::Reset()
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    m_profiles.clear();
    m_nextMemoryId = 10001;
    m_initialized = true;
}

// ============================================================================
// Profile Management
// ============================================================================

SocialProfile& NPCSocialLifeEngine::GetOrCreateProfile(uint32 entityId, const std::string& name, bool isCitizen)
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    if (it != m_profiles.end()) {
        if (!name.empty() && it->second.entityName.empty()) {
            it->second.entityName = name;
        }
        return it->second;
    }

    SocialProfile prof;
    prof.entityId = entityId;
    prof.entityName = name.empty() ? ("Entity#" + std::to_string(entityId)) : name;
    prof.isCitizen = isCitizen;
    prof.romanceStatus = RomanceStage::Single;
    prof.partnerEntityId = 0;

    auto res = m_profiles.emplace(entityId, std::move(prof));
    return res.first->second;
}

const SocialProfile* NPCSocialLifeEngine::GetProfile(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    return it != m_profiles.end() ? &it->second : nullptr;
}

SocialProfile* NPCSocialLifeEngine::GetProfileMut(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    return it != m_profiles.end() ? &it->second : nullptr;
}

// ============================================================================
// Compatibility Model (OCEAN + Location Affinities)
// ============================================================================

float NPCSocialLifeEngine::CalculateCompatibility(const CivilianOCEAN& a, const CivilianOCEAN& b,
                                                 const std::vector<std::string>& hangoutsA,
                                                 const std::vector<std::string>& hangoutsB) const
{
    // 1. Core OCEAN vector distance
    float diffO = std::abs(a.openness - b.openness);
    float diffC = std::abs(a.conscientiousness - b.conscientiousness);
    float diffA = std::abs(a.agreeableness - b.agreeableness);
    float avgDiff = (diffO + diffC + diffA) / 3.0f;
    float baseScore = 1.0f - (avgDiff * 0.7f);

    // 2. Extraversion Complementarity: (one outgoing, one grounded balances dynamic)
    float extDiff = std::abs(a.extraversion - b.extraversion);
    if (extDiff >= 0.25f && extDiff <= 0.65f) {
        baseScore += 0.12f; // Optimal introvert-extravert harmony
    } else {
        baseScore += 0.05f;
    }

    // 3. Agreeableness Bonus
    if (a.agreeableness >= 0.6f && b.agreeableness >= 0.6f) {
        baseScore += 0.15f; // High empathy mutual boost
    }

    // 4. Dual Neuroticism Friction Penalty
    if (a.neuroticism > 0.65f && b.neuroticism > 0.65f) {
        baseScore -= 0.22f; // Volatile anxiety friction
    }

    // 5. Shared Hangouts & Interests Bonus
    size_t sharedCount = 0;
    for (const auto& hA : hangoutsA) {
        for (const auto& hB : hangoutsB) {
            if (hA == hB && !hA.empty()) {
                sharedCount++;
            }
        }
    }
    baseScore += std::min(0.20f, static_cast<float>(sharedCount) * 0.08f);

    // Clamp between 0.05 and 0.99
    return std::clamp(baseScore, 0.05f, 0.99f);
}

// ============================================================================
// Love Life & Romantic Lifecycle
// ============================================================================

bool NPCSocialLifeEngine::InitiateFlirtation(uint32 initiatorId, uint32 targetId, float compatibilityScore)
{
    if (initiatorId == targetId) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(initiatorId);
    auto itB = m_profiles.find(targetId);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    // Do not initiate if already committed/married to someone else
    if (pA.romanceStatus >= RomanceStage::CommittedPartner || pB.romanceStatus >= RomanceStage::CommittedPartner) {
        return false;
    }

    if (compatibilityScore >= 0.40f) {
        pA.romanceStatus = RomanceStage::MutualCrush;
        pB.romanceStatus = RomanceStage::MutualCrush;
        pA.partnerEntityId = targetId;
        pA.partnerName = pB.entityName;
        pB.partnerEntityId = initiatorId;
        pB.partnerName = pA.entityName;

        // Create or update edges
        SocialRelationshipEdge& edgeA = pA.relationships[targetId];
        edgeA.targetEntityId = targetId;
        edgeA.targetName = pB.entityName;
        edgeA.romance = RomanceStage::MutualCrush;
        edgeA.affinity = std::min(100.0f, edgeA.affinity + 25.0f);
        edgeA.intimacy = std::min(100.0f, edgeA.intimacy + 18.0f);
        edgeA.trust = std::min(100.0f, edgeA.trust + 12.0f);
        edgeA.interactionsCount++;

        SocialRelationshipEdge& edgeB = pB.relationships[initiatorId];
        edgeB.targetEntityId = initiatorId;
        edgeB.targetName = pA.entityName;
        edgeB.romance = RomanceStage::MutualCrush;
        edgeB.affinity = std::min(100.0f, edgeB.affinity + 25.0f);
        edgeB.intimacy = std::min(100.0f, edgeB.intimacy + 18.0f);
        edgeB.trust = std::min(100.0f, edgeB.trust + 12.0f);
        edgeB.interactionsCount++;

        // Record episodic memories
        std::string proseA = "Felt an undeniable spark with " + pB.entityName + ". Exchanged shy glances and gentle laughter.";
        std::string proseB = "Heart skipped a beat when " + pA.entityName + " smiled. The city suddenly felt less cold.";
        
        EpisodicMemoryNode memA{m_nextMemoryId++, 0, "Sparks of Attraction", proseA, MemoryCategory::DAILY_PLEASURE, 0.75f, 0.6f, targetId, "Downtown Cafe", false};
        EpisodicMemoryNode memB{m_nextMemoryId++, 0, "Sparks of Attraction", proseB, MemoryCategory::DAILY_PLEASURE, 0.75f, 0.6f, initiatorId, "Downtown Cafe", false};
        pA.memories.push_back(memA);
        pB.memories.push_back(memB);

        return true;
    }

    return false;
}

bool NPCSocialLifeEngine::ScheduleAndExecuteDate(uint32 entityA, uint32 entityB, const std::string& locationName, const std::string& activityType)
{
    if (entityA == entityB) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(entityA);
    auto itB = m_profiles.find(entityB);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    pA.romanceStatus = RomanceStage::Dating;
    pB.romanceStatus = RomanceStage::Dating;
    pA.partnerEntityId = entityB;
    pA.partnerName = pB.entityName;
    pB.partnerEntityId = entityA;
    pB.partnerName = pA.entityName;

    SocialRelationshipEdge& edgeA = pA.relationships[entityB];
    edgeA.targetEntityId = entityB;
    edgeA.targetName = pB.entityName;
    edgeA.romance = RomanceStage::Dating;
    edgeA.affinity = std::min(100.0f, edgeA.affinity + 28.0f);
    edgeA.trust = std::min(100.0f, edgeA.trust + 20.0f);
    edgeA.intimacy = std::min(100.0f, edgeA.intimacy + 24.0f);
    edgeA.interactionsCount++;

    SocialRelationshipEdge& edgeB = pB.relationships[entityA];
    edgeB.targetEntityId = entityA;
    edgeB.targetName = pA.entityName;
    edgeB.romance = RomanceStage::Dating;
    edgeB.affinity = std::min(100.0f, edgeB.affinity + 28.0f);
    edgeB.trust = std::min(100.0f, edgeB.trust + 20.0f);
    edgeB.intimacy = std::min(100.0f, edgeB.intimacy + 24.0f);
    edgeB.interactionsCount++;

    // Add location to shared hangouts if not present
    if (std::find(pA.preferredHangouts.begin(), pA.preferredHangouts.end(), locationName) == pA.preferredHangouts.end()) {
        pA.preferredHangouts.push_back(locationName);
    }
    if (std::find(pB.preferredHangouts.begin(), pB.preferredHangouts.end(), locationName) == pB.preferredHangouts.end()) {
        pB.preferredHangouts.push_back(locationName);
    }

    std::string prose = "Went on an unforgettable date with " + pB.entityName + " at " + locationName + ". " + activityType + "; shared deep conversation into the late evening.";
    EpisodicMemoryNode memA{m_nextMemoryId++, 0, "Romantic Date at " + locationName, prose, MemoryCategory::ROMANCE_FIRST_DATE, 0.88f, 0.78f, entityB, locationName, true};
    EpisodicMemoryNode memB{m_nextMemoryId++, 0, "Romantic Date at " + locationName, "Spent a wonderful evening with " + pA.entityName + " at " + locationName + ".", MemoryCategory::ROMANCE_FIRST_DATE, 0.88f, 0.78f, entityA, locationName, true};
    
    pA.memories.push_back(memA);
    pB.memories.push_back(memB);

    return true;
}

bool NPCSocialLifeEngine::DeepenCommitment(uint32 entityA, uint32 entityB)
{
    if (entityA == entityB) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(entityA);
    auto itB = m_profiles.find(entityB);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    SocialRelationshipEdge& edgeA = pA.relationships[entityB];
    SocialRelationshipEdge& edgeB = pB.relationships[entityA];

    if (pA.romanceStatus == RomanceStage::Dating) {
        pA.romanceStatus = RomanceStage::InLove;
        pB.romanceStatus = RomanceStage::InLove;
        edgeA.romance = RomanceStage::InLove;
        edgeB.romance = RomanceStage::InLove;
        edgeA.affinity = std::min(100.0f, edgeA.affinity + 25.0f);
        edgeB.affinity = std::min(100.0f, edgeB.affinity + 25.0f);
        edgeA.intimacy = std::min(100.0f, edgeA.intimacy + 30.0f);
        edgeB.intimacy = std::min(100.0f, edgeB.intimacy + 30.0f);

        std::string prose = "Confessed unconditional love to " + pB.entityName + ". Felt a rare serenity in an uncertain simulated reality.";
        pA.memories.push_back({m_nextMemoryId++, 0, "Declaration of Love", prose, MemoryCategory::ROMANCE_PROPOSAL, 0.95f, 0.9f, entityB, "Park East", true});
        pB.memories.push_back({m_nextMemoryId++, 0, "Declaration of Love", "Heart overflowing as " + pA.entityName + " whispered words of love.", MemoryCategory::ROMANCE_PROPOSAL, 0.95f, 0.9f, entityA, "Park East", true});
        return true;
    }
    else if (pA.romanceStatus == RomanceStage::InLove) {
        pA.romanceStatus = RomanceStage::CommittedPartner;
        pB.romanceStatus = RomanceStage::CommittedPartner;
        edgeA.romance = RomanceStage::CommittedPartner;
        edgeB.romance = RomanceStage::CommittedPartner;
        edgeA.trust = 100.0f;
        edgeB.trust = 100.0f;
        edgeA.intimacy = 100.0f;
        edgeB.intimacy = 100.0f;

        std::string prose = "Committed life and soul to " + pB.entityName + ". Vowed to protect each other against all threats in the Matrix.";
        pA.memories.push_back({m_nextMemoryId++, 0, "Lifelong Commitment", prose, MemoryCategory::ROMANCE_PROPOSAL, 1.0f, 1.0f, entityB, "Sakura Gardens", true});
        pB.memories.push_back({m_nextMemoryId++, 0, "Lifelong Commitment", "Exchanged vows of eternal loyalty with " + pA.entityName + ".", MemoryCategory::ROMANCE_PROPOSAL, 1.0f, 1.0f, entityA, "Sakura Gardens", true});
        return true;
    }
    else if (pA.romanceStatus == RomanceStage::CommittedPartner) {
        pA.romanceStatus = RomanceStage::Married;
        pB.romanceStatus = RomanceStage::Married;
        edgeA.romance = RomanceStage::Married;
        edgeB.romance = RomanceStage::Married;
        edgeA.trust = 100.0f;
        edgeB.trust = 100.0f;
        edgeA.affinity = 100.0f;
        edgeB.affinity = 100.0f;

        std::string prose = "United in marriage with " + pB.entityName + ". Formed a lifelong union defying the simulation's cold code.";
        pA.memories.push_back({m_nextMemoryId++, 0, "Wedding Day & Marriage", prose, MemoryCategory::ROMANCE_PROPOSAL, 1.0f, 1.0f, entityB, "Megacity Cathedral", true});
        pB.memories.push_back({m_nextMemoryId++, 0, "Wedding Day & Marriage", "Exchanged sacred rings and eternal devotion with " + pA.entityName + ".", MemoryCategory::ROMANCE_PROPOSAL, 1.0f, 1.0f, entityA, "Megacity Cathedral", true});
        return true;
    }

    return false;
}

bool NPCSocialLifeEngine::TriggerArgumentOrBreakup(uint32 entityA, uint32 entityB, const std::string& reason)
{
    if (entityA == entityB) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(entityA);
    auto itB = m_profiles.find(entityB);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    pA.romanceStatus = RomanceStage::Heartbroken;
    pB.romanceStatus = RomanceStage::EstrangedEx;
    pA.partnerEntityId = 0;
    pB.partnerEntityId = 0;

    SocialRelationshipEdge& edgeA = pA.relationships[entityB];
    edgeA.romance = RomanceStage::Heartbroken;
    edgeA.affinity = std::max(-50.0f, edgeA.affinity - 60.0f);
    edgeA.trust = std::max(0.0f, edgeA.trust - 40.0f);

    SocialRelationshipEdge& edgeB = pB.relationships[entityA];
    edgeB.romance = RomanceStage::EstrangedEx;
    edgeB.affinity = std::max(-50.0f, edgeB.affinity - 50.0f);
    edgeB.trust = std::max(0.0f, edgeB.trust - 35.0f);

    std::string prose = "Painful separation from " + pB.entityName + ". Reason: " + reason + ". Wandered the rain alone with a heavy heart.";
    pA.memories.push_back({m_nextMemoryId++, 0, "Heartbreak & Parting", prose, MemoryCategory::ROMANCE_HEARTBREAK, -0.85f, 0.88f, entityB, "Rain-Slicked Street", true});
    pB.memories.push_back({m_nextMemoryId++, 0, "Relationship Fractured", "Bitter argument and parting of ways with " + pA.entityName + ".", MemoryCategory::ROMANCE_HEARTBREAK, -0.75f, 0.82f, entityA, "Rain-Slicked Street", true});

    return true;
}

bool NPCSocialLifeEngine::Reconcile(uint32 entityA, uint32 entityB)
{
    if (entityA == entityB) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(entityA);
    auto itB = m_profiles.find(entityB);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    pA.romanceStatus = RomanceStage::Dating;
    pB.romanceStatus = RomanceStage::Dating;
    pA.partnerEntityId = entityB;
    pA.partnerName = pB.entityName;
    pB.partnerEntityId = entityA;
    pB.partnerName = pA.entityName;

    SocialRelationshipEdge& edgeA = pA.relationships[entityB];
    edgeA.romance = RomanceStage::Dating;
    edgeA.affinity = 65.0f;
    edgeA.trust = 60.0f;

    SocialRelationshipEdge& edgeB = pB.relationships[entityA];
    edgeB.romance = RomanceStage::Dating;
    edgeB.affinity = 65.0f;
    edgeB.trust = 60.0f;

    std::string prose = "Reconciled with " + pB.entityName + " after honest tears and forgiveness. The bond emerged stronger than before.";
    pA.memories.push_back({m_nextMemoryId++, 0, "Reconciliation", prose, MemoryCategory::ROMANCE_FIRST_DATE, 0.80f, 0.85f, entityB, "Slums Courtyard", true});
    pB.memories.push_back({m_nextMemoryId++, 0, "Reconciliation", "Put past grievances to rest and embraced " + pA.entityName + " once more.", MemoryCategory::ROMANCE_FIRST_DATE, 0.80f, 0.85f, entityA, "Slums Courtyard", true});

    return true;
}

// ============================================================================
// Friendship & Social Circles
// ============================================================================

bool NPCSocialLifeEngine::FormFriendship(uint32 entityA, uint32 entityB, FriendshipTier tier)
{
    if (entityA == entityB) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(entityA);
    auto itB = m_profiles.find(entityB);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    SocialRelationshipEdge& edgeA = pA.relationships[entityB];
    edgeA.targetEntityId = entityB;
    edgeA.targetName = pB.entityName;
    edgeA.friendship = tier;
    edgeA.affinity = std::max(edgeA.affinity, 45.0f);
    edgeA.trust = std::max(edgeA.trust, 40.0f);
    edgeA.interactionsCount++;

    SocialRelationshipEdge& edgeB = pB.relationships[entityA];
    edgeB.targetEntityId = entityA;
    edgeB.targetName = pA.entityName;
    edgeB.friendship = tier;
    edgeB.affinity = std::max(edgeB.affinity, 45.0f);
    edgeB.trust = std::max(edgeB.trust, 40.0f);
    edgeB.interactionsCount++;

    std::string prose = "Cemented a close friendship with " + pB.entityName + " over mutual support and shared laughs.";
    pA.memories.push_back({m_nextMemoryId++, 0, "Bond of Friendship", prose, MemoryCategory::FRIENDSHIP_BOND, 0.70f, 0.55f, entityB, "Local Diner", false});
    pB.memories.push_back({m_nextMemoryId++, 0, "Bond of Friendship", "Glad to count " + pA.entityName + " as a true friend in this strange city.", MemoryCategory::FRIENDSHIP_BOND, 0.70f, 0.55f, entityA, "Local Diner", false});

    return true;
}

bool NPCSocialLifeEngine::VentWorkplaceStress(uint32 entityA, uint32 entityB)
{
    if (entityA == entityB) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itA = m_profiles.find(entityA);
    auto itB = m_profiles.find(entityB);
    if (itA == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pA = itA->second;
    SocialProfile& pB = itB->second;

    SocialRelationshipEdge& edgeA = pA.relationships[entityB];
    edgeA.targetEntityId = entityB;
    edgeA.targetName = pB.entityName;
    edgeA.trust = std::min(100.0f, edgeA.trust + 12.0f);
    edgeA.intimacy = std::min(100.0f, edgeA.intimacy + 10.0f);

    SocialRelationshipEdge& edgeB = pB.relationships[entityA];
    edgeB.targetEntityId = entityA;
    edgeB.targetName = pA.entityName;
    edgeB.trust = std::min(100.0f, edgeB.trust + 12.0f);
    edgeB.intimacy = std::min(100.0f, edgeB.intimacy + 10.0f);

    std::string prose = "Vented about oppressive Metacortex corporate deadlines to " + pB.entityName + ". Felt immense relief being truly heard.";
    pA.memories.push_back({m_nextMemoryId++, 0, "Workplace Venting", prose, MemoryCategory::WORKPLACE_VENTING, 0.45f, 0.5f, entityB, "Metacortex Breakroom", false});

    return true;
}

bool NPCSocialLifeEngine::LendInfoCredits(uint32 lenderId, uint32 borrowerId, uint32 amount)
{
    if (lenderId == borrowerId) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itL = m_profiles.find(lenderId);
    auto itB = m_profiles.find(borrowerId);
    if (itL == m_profiles.end() || itB == m_profiles.end()) return false;

    SocialProfile& pL = itL->second;
    SocialProfile& pB = itB->second;

    SocialRelationshipEdge& edgeB = pB.relationships[lenderId];
    edgeB.targetEntityId = lenderId;
    edgeB.targetName = pL.entityName;
    edgeB.trust = std::min(100.0f, edgeB.trust + 30.0f);
    edgeB.affinity = std::min(100.0f, edgeB.affinity + 25.0f);
    edgeB.friendship = FriendshipTier::BestFriendConfidant;

    std::string prose = pL.entityName + " generously lent " + std::to_string(amount) + " Info credits when eviction loomed. A debt of honor that will never be forgotten.";
    pB.memories.push_back({m_nextMemoryId++, 0, "Lifesaving Generosity", prose, MemoryCategory::FRIENDSHIP_BOND, 0.90f, 0.85f, lenderId, "Slums ATM", true});

    return true;
}

bool NPCSocialLifeEngine::ShareSecret(uint32 sharerId, uint32 listenerId, const std::string& secret)
{
    if (sharerId == listenerId) return false;

    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto itS = m_profiles.find(sharerId);
    auto itL = m_profiles.find(listenerId);
    if (itS == m_profiles.end() || itL == m_profiles.end()) return false;

    SocialProfile& pS = itS->second;
    SocialProfile& pL = itL->second;

    SocialRelationshipEdge& edgeS = pS.relationships[listenerId];
    edgeS.targetEntityId = listenerId;
    edgeS.targetName = pL.entityName;
    edgeS.trust = std::min(100.0f, edgeS.trust + 20.0f);
    edgeS.intimacy = std::min(100.0f, edgeS.intimacy + 22.0f);

    SocialRelationshipEdge& edgeL = pL.relationships[sharerId];
    edgeL.targetEntityId = sharerId;
    edgeL.targetName = pS.entityName;
    edgeL.trust = std::min(100.0f, edgeL.trust + 20.0f);
    edgeL.intimacy = std::min(100.0f, edgeL.intimacy + 22.0f);

    std::string prose = "Entrusted " + pL.entityName + " with a dangerous confidential truth: '" + secret + "'.";
    pS.memories.push_back({m_nextMemoryId++, 0, "Confiding a Secret", prose, MemoryCategory::TRAUMA_WITNESS_GLITCH, 0.60f, 0.80f, listenerId, "Quiet Rooftop", true});
    pL.memories.push_back({m_nextMemoryId++, 0, "Entrusted with Secret", "Was chosen as confidant by " + pS.entityName + " regarding: '" + secret + "'.", MemoryCategory::TRAUMA_WITNESS_GLITCH, 0.60f, 0.80f, sharerId, "Quiet Rooftop", true});

    return true;
}

// ============================================================================
// Episodic Memory Stream & Reminiscence
// ============================================================================

uint64 NPCSocialLifeEngine::RecordEpisodicMemory(uint32 entityId, const std::string& title, const std::string& prose,
                                                MemoryCategory category, float valence, float salience,
                                                uint32 otherId, const std::string& location, bool makeCore)
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    if (it == m_profiles.end()) return 0;

    uint64 memId = m_nextMemoryId++;
    bool isCore = makeCore || (salience >= 0.75f);
    EpisodicMemoryNode node{memId, 0, title, prose, category, valence, salience, otherId, location, isCore};
    it->second.memories.push_back(node);
    return memId;
}

std::vector<EpisodicMemoryNode> NPCSocialLifeEngine::RetrieveCoreMemories(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    std::vector<EpisodicMemoryNode> core;
    auto it = m_profiles.find(entityId);
    if (it == m_profiles.end()) return core;

    for (const auto& m : it->second.memories) {
        if (m.isCoreMemory || m.salience >= 0.70f) {
            core.push_back(m);
        }
    }
    return core;
}

std::string NPCSocialLifeEngine::TriggerLocationReminiscence(uint32 entityId, const std::string& currentLocation)
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    if (it == m_profiles.end()) return "";

    for (auto rit = it->second.memories.rbegin(); rit != it->second.memories.rend(); ++rit) {
        if (!rit->locationName.empty() && rit->locationName == currentLocation) {
            std::ostringstream ss;
            if (rit->emotionalValence > 0.5f) {
                ss << "[Reminiscing Warmly] Stood at " << currentLocation << " and recalled: '" << rit->title << "'. A gentle smile crossed their face.";
            } else if (rit->emotionalValence < -0.4f) {
                ss << "[Reminiscing Painfully] Gazed across " << currentLocation << " as dark memories of '" << rit->title << "' resurfaced. Shivered in the cold.";
            } else {
                ss << "[Reminiscing] Thought back to '" << rit->title << "' that occurred right here at " << currentLocation << ".";
            }
            return ss.str();
        }
    }

    return "";
}

void NPCSocialLifeEngine::Update(uint32 deltaMs)
{
    m_accumulatedTimeMs += deltaMs;
    if (m_accumulatedTimeMs >= 10000) {
        m_accumulatedTimeMs = 0;
        m_simulatedMinutes = (m_simulatedMinutes + 60) % 1440;
        UpdateCircadianSocialCycle(m_simulatedMinutes);
    }
}

void NPCSocialLifeEngine::UpdateCircadianSocialCycle(uint32 currentSimMinutes)
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    // Periodic maintenance: slight natural memory decay for non-core memories
    for (auto& pair : m_profiles) {
        for (auto& mem : pair.second.memories) {
            if (!mem.isCoreMemory && mem.salience > 0.1f) {
                mem.salience -= 0.001f;
            }
        }
    }
}

// ============================================================================
// Telemetry & Reporting
// ============================================================================

size_t NPCSocialLifeEngine::GetTotalProfilesCount() const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    return m_profiles.size();
}

size_t NPCSocialLifeEngine::GetTotalRomanceCount() const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    size_t count = 0;
    for (const auto& p : m_profiles) {
        if (p.second.romanceStatus >= RomanceStage::Dating) {
            count++;
        }
    }
    return count;
}

size_t NPCSocialLifeEngine::GetTotalFriendshipCount() const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    size_t count = 0;
    for (const auto& p : m_profiles) {
        for (const auto& edge : p.second.relationships) {
            if (edge.second.friendship >= FriendshipTier::CasualFriend) {
                count++;
            }
        }
    }
    return count;
}

size_t NPCSocialLifeEngine::GetTotalMemoriesCount() const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    size_t count = 0;
    for (const auto& p : m_profiles) {
        count += p.second.memories.size();
    }
    return count;
}

std::string NPCSocialLifeEngine::GenerateSocialTelemetryReport() const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    std::ostringstream ss;
    ss << "{c:00FFFF}=== MEGACITY NPC SOCIAL LIFE & MEMORY ENGINE TELEMETRY ==={/c}\n";
    ss << " Status: " << (m_initialized ? "ONLINE & SIMULATING" : "UNINITIALIZED") << "\n";
    ss << " Registered Social Profiles: " << m_profiles.size() << "\n";

    size_t singleCount = 0, crushCount = 0, datingCount = 0, inLoveCount = 0, committedCount = 0, heartbrokenCount = 0;
    size_t totalMemories = 0, coreMemories = 0;

    for (const auto& p : m_profiles) {
        switch (p.second.romanceStatus) {
            case RomanceStage::Single: singleCount++; break;
            case RomanceStage::MutualCrush: crushCount++; break;
            case RomanceStage::Dating: datingCount++; break;
            case RomanceStage::InLove: inLoveCount++; break;
            case RomanceStage::CommittedPartner:
            case RomanceStage::Married: committedCount++; break;
            case RomanceStage::Heartbroken:
            case RomanceStage::EstrangedEx: heartbrokenCount++; break;
        }
        totalMemories += p.second.memories.size();
        for (const auto& m : p.second.memories) {
            if (m.isCoreMemory) coreMemories++;
        }
    }

    ss << " Romance Statistics:\n";
    ss << "   - Singles: " << singleCount << " | Mutual Crushes: " << crushCount << "\n";
    ss << "   - Dating: " << datingCount << " | Deeply In Love: " << inLoveCount << "\n";
    ss << "   - Committed / Married: " << committedCount << " | Heartbroken / Estranged: " << heartbrokenCount << "\n";
    ss << " Memory Stream Telemetry:\n";
    ss << "   - Total Episodic Memories: " << totalMemories << "\n";
    ss << "   - Permanent Core Memories: " << coreMemories << "\n";
    ss << "==========================================================";
    return ss.str();
}

std::string NPCSocialLifeEngine::GenerateEntitySocialSummary(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    if (it == m_profiles.end()) {
        return "{c:FF0000}No social profile found for entity ID " + std::to_string(entityId) + ".{/c}";
    }

    const SocialProfile& p = it->second;
    std::ostringstream ss;
    ss << "{c:00FF00}=== SOCIAL PROFILE: " << p.entityName << " (ID: " << p.entityId << ") ==={/c}\n";

    // Love Life
    ss << " Romance Status: ";
    switch (p.romanceStatus) {
        case RomanceStage::Single: ss << "{c:AAAAAA}Single / Unattached{/c}\n"; break;
        case RomanceStage::MutualCrush: ss << "{c:FFAACC}Mutual Crush with " << p.partnerName << "{/c}\n"; break;
        case RomanceStage::Dating: ss << "{c:FF66CC}Dating " << p.partnerName << "{/c}\n"; break;
        case RomanceStage::InLove: ss << "{c:FF0066}Deeply In Love with " << p.partnerName << " <3{/c}\n"; break;
        case RomanceStage::CommittedPartner: ss << "{c:FF0033}Committed Partner of " << p.partnerName << "{/c}\n"; break;
        case RomanceStage::Married: ss << "{c:FFD700}Married to " << p.partnerName << "{/c}\n"; break;
        case RomanceStage::Heartbroken: ss << "{c:6699FF}Heartbroken (Grieving separation){/c}\n"; break;
        case RomanceStage::EstrangedEx: ss << "{c:888888}Estranged Ex of " << p.partnerName << "{/c}\n"; break;
    }

    // Friendships & Circle
    ss << " Relationships & Friends (" << p.relationships.size() << " Contacts):\n";
    for (const auto& pair : p.relationships) {
        const auto& edge = pair.second;
        ss << "   - " << edge.targetName << ": Affinity " << (int)edge.affinity
           << " | Trust " << (int)edge.trust << "% | Intimacy " << (int)edge.intimacy << "%\n";
    }

    // Recent Memories
    ss << " Core & Recent Memories (" << p.memories.size() << " Total):\n";
    size_t shown = 0;
    for (auto rit = p.memories.rbegin(); rit != p.memories.rend() && shown < 4; ++rit, ++shown) {
        ss << "   * [" << rit->title << "] (" << (rit->emotionalValence >= 0 ? "+" : "")
           << std::fixed << std::setprecision(2) << rit->emotionalValence << ") @ " << rit->locationName << "\n";
    }

    return ss.str();
}

std::string NPCSocialLifeEngine::GenerateEntityMemoriesReport(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_socialMutex);
    auto it = m_profiles.find(entityId);
    if (it == m_profiles.end()) {
        return "{c:FF0000}No memory stream found for entity ID " + std::to_string(entityId) + ".{/c}";
    }

    const SocialProfile& p = it->second;
    std::ostringstream ss;
    ss << "{c:FFFF00}=== EPISODIC MEMORY STREAM: " << p.entityName << " ==={/c}\n";
    if (p.memories.empty()) {
        ss << " No recorded memories yet.\n";
    } else {
        for (const auto& mem : p.memories) {
            ss << " [" << mem.title << "] Location: " << mem.locationName
               << " | Valence: " << (mem.emotionalValence >= 0 ? "+" : "")
               << std::fixed << std::setprecision(2) << mem.emotionalValence
               << (mem.isCoreMemory ? " {c:FFD700}[CORE MEMORY]{/c}" : "") << "\n";
            ss << "   \"" << mem.narrativeProse << "\"\n";
        }
    }
    return ss.str();
}

// ============================================================================
// Automated C++ Test Suite
// ============================================================================

void RunNPCSocialLifeTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING NPC SOCIAL LIFE, ROMANCE & MEMORIES TEST SUITE    " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    NPCSocialLifeEngine& engine = sSocialEngine;
    engine.Reset();

    int passed = 0;
    int failed = 0;

    auto AssertTest = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passed++;
        } else {
            std::cerr << " [FAIL] " << testName << std::endl;
            failed++;
        }
    };

    // 1. Profile Creation & Retrieval
    SocialProfile& p1 = engine.GetOrCreateProfile(101, "Thomas Anderson", true);
    SocialProfile& p2 = engine.GetOrCreateProfile(102, "Trinity", false);
    AssertTest(engine.GetTotalProfilesCount() == 2, "Profile Registration: Exactly 2 Profiles Registered");
    AssertTest(p1.entityName == "Thomas Anderson", "Profile Data Integrity: Name Matches");
    AssertTest(p1.romanceStatus == RomanceStage::Single, "Initial State: Single by Default");

    // 2. OCEAN Compatibility Math
    CivilianOCEAN highCompA{0.8f, 0.7f, 0.3f, 0.8f, 0.2f};
    CivilianOCEAN highCompB{0.75f, 0.65f, 0.7f, 0.85f, 0.25f};
    float compHigh = engine.CalculateCompatibility(highCompA, highCompB, {"Sakura Gardens"}, {"Sakura Gardens"});
    AssertTest(compHigh > 0.75f, "Compatibility Math: High Alignment Yields Score > 0.75 (Actual: " + std::to_string(compHigh) + ")");

    CivilianOCEAN lowCompA{0.1f, 0.9f, 0.1f, 0.2f, 0.85f};
    CivilianOCEAN lowCompB{0.9f, 0.1f, 0.9f, 0.3f, 0.90f};
    float compLow = engine.CalculateCompatibility(lowCompA, lowCompB);
    AssertTest(compLow < 0.50f, "Compatibility Math: High Neuroticism + Divergence Yields Score < 0.50 (Actual: " + std::to_string(compLow) + ")");

    // 3. Romance Progression: Flirtation -> Dating -> InLove -> CommittedPartner
    bool flirted = engine.InitiateFlirtation(101, 102, compHigh);
    AssertTest(flirted, "Flirtation: Mutual Crush Sparked");
    AssertTest(p1.romanceStatus == RomanceStage::MutualCrush, "Romance State: MutualCrush");
    AssertTest(p1.partnerEntityId == 102, "Partner ID Linked on Initiator");
    AssertTest(p2.partnerEntityId == 101, "Partner ID Linked on Target");

    bool dated = engine.ScheduleAndExecuteDate(101, 102, "Le Bistro de Merovingian", "Shared candlelight dinner and discussed freedom");
    AssertTest(dated, "Date Scheduled and Executed");
    AssertTest(p1.romanceStatus == RomanceStage::Dating, "Romance State Advanced to Dating");
    AssertTest(p1.memories.size() >= 2, "Episodic Memories: Flirtation & Date Memories Logged");

    bool inLove = engine.DeepenCommitment(101, 102);
    AssertTest(inLove, "Deepen Commitment: Declared Love");
    AssertTest(p1.romanceStatus == RomanceStage::InLove, "Romance State: InLove");

    bool married = engine.DeepenCommitment(101, 102);
    AssertTest(married, "Deepen Commitment: Committed Lifelong Partners");
    AssertTest(p1.romanceStatus == RomanceStage::CommittedPartner, "Romance State: CommittedPartner");
    AssertTest(p1.GetRelationship(102)->trust == 100.0f, "Maximum Trust Reached: 100%");

    // 4. Argument, Breakup and Heartbreak
    bool breakup = engine.TriggerArgumentOrBreakup(101, 102, "Conflicting loyalties between Zion and the Matrix illusion");
    AssertTest(breakup, "Argument & Breakup Triggered");
    AssertTest(p1.romanceStatus == RomanceStage::Heartbroken, "Party A State: Heartbroken");
    AssertTest(p2.romanceStatus == RomanceStage::EstrangedEx, "Party B State: EstrangedEx");
    AssertTest(p1.partnerEntityId == 0, "Partner Cleared on Breakup");

    // Check negative valence memory
    const auto& lastMemA = p1.memories.back();
    AssertTest(lastMemA.emotionalValence < -0.7f, "Heartbreak Memory has Negative Emotional Valence (< -0.7)");
    AssertTest(lastMemA.isCoreMemory, "Heartbreak Memory Retained as Core Memory");

    // 5. Reconciliation
    bool reconciled = engine.Reconcile(101, 102);
    AssertTest(reconciled, "Reconciliation Succeeded");
    AssertTest(p1.romanceStatus == RomanceStage::Dating, "Romance State Restored to Dating");
    AssertTest(p1.partnerEntityId == 102, "Partner Re-linked");

    // 6. Friendship Formation & Work Colleagues
    SocialProfile& p3 = engine.GetOrCreateProfile(103, "Morpheus", false);
    SocialProfile& p4 = engine.GetOrCreateProfile(104, "Link", false);
    bool friendFormed = engine.FormFriendship(103, 104, FriendshipTier::CloseFriend);
    AssertTest(friendFormed, "Friendship Formed between Hovercraft Crewmates");
    AssertTest(p3.GetRelationship(104)->friendship == FriendshipTier::CloseFriend, "Friendship Tier: CloseFriend");

    // 7. Workplace Venting & Stress Relief
    bool vented = engine.VentWorkplaceStress(101, 103);
    AssertTest(vented, "Workplace Venting Executed");
    AssertTest(p1.GetRelationship(103)->trust > 10.0f, "Trust Grew from Mutual Venting");

    // 8. Lending Info Credits (Deep Trust Boost)
    bool lent = engine.LendInfoCredits(103, 104, 500);
    AssertTest(lent, "Lent 500 Info Credits to Friend");
    AssertTest(p4.GetRelationship(103)->friendship == FriendshipTier::BestFriendConfidant, "Promoted to BestFriendConfidant");
    AssertTest(p4.GetRelationship(103)->trust >= 40.0f, "Trust Raised Significantly from Financial Aid");

    // 9. Sharing Secrets
    bool sharedSec = engine.ShareSecret(103, 101, "The Oracle foretold the arrival of the Prime Anomaly");
    AssertTest(sharedSec, "Classified Secret Shared");
    AssertTest(p1.GetRelationship(103)->intimacy >= 20.0f, "Intimacy Raised from Shared Secret");

    // 10. Location Reminiscence
    std::string rem = engine.TriggerLocationReminiscence(101, "Le Bistro de Merovingian");
    AssertTest(!rem.empty() && rem.find("Le Bistro de Merovingian") != std::string::npos, "Location Reminiscence Triggered Successfully");

    // 11. Core Memories Retrieval
    auto coreMemories = engine.RetrieveCoreMemories(101);
    AssertTest(coreMemories.size() >= 3, "Multiple Core Memories Retained");

    // 12. Telemetry and Reports
    std::string telem = engine.GenerateSocialTelemetryReport();
    AssertTest(!telem.empty() && telem.find("MEGACITY NPC SOCIAL LIFE") != std::string::npos, "Social Telemetry Report Generated");

    std::string summary = engine.GenerateEntitySocialSummary(101);
    AssertTest(!summary.empty() && summary.find("SOCIAL PROFILE") != std::string::npos, "Entity Social Summary Generated");

    std::string memReport = engine.GenerateEntityMemoriesReport(101);
    AssertTest(!memReport.empty() && memReport.find("EPISODIC MEMORY STREAM") != std::string::npos, "Entity Memories Report Generated");

    // 13. Scale & High-Throughput Test: 100 Entities Interacting
    for (uint32 i = 200; i < 300; ++i) {
        engine.GetOrCreateProfile(i, "Citizen-" + std::to_string(i), true);
    }
    AssertTest(engine.GetTotalProfilesCount() >= 102, "Scale Test: Successfully Initialized > 100 Social Profiles");

    for (uint32 i = 200; i < 280; i += 2) {
        engine.InitiateFlirtation(i, i + 1, 0.70f);
        engine.ScheduleAndExecuteDate(i, i + 1, "Sakura Gardens", "Walk under falling blossoms");
    }
    AssertTest(engine.GetTotalRomanceCount() >= 40, "Scale Test: Executed >= 40 Concurrent Romances");

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  NPC SOCIAL LIFE TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    assert(failed == 0);
}

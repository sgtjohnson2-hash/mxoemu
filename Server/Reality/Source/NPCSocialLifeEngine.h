#ifndef MXOEMU_NPC_SOCIAL_LIFE_ENGINE_H
#define MXOEMU_NPC_SOCIAL_LIFE_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "CityLifeManager.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <deque>
#include <memory>
#include <mutex>
#include <cstdint>
#include <sstream>

// ============================================================================
// Romance & Friendship Simulation Enums
// ============================================================================

enum class RomanceStage : uint8_t {
    Single                  = 0, // Not in an active romance
    MutualCrush             = 1, // Romantic sparks, eye contact, flirtation
    Dating                  = 2, // Regularly going on dates (diners, parks, bistros)
    InLove                  = 3, // Deep mutual emotional commitment, shared secrets
    CommittedPartner        = 4, // Long-term domestic partner / engaged
    Married                 = 5, // Formal marital bond inside or outside the Matrix
    Heartbroken             = 6, // Recently rejected, dumped, or grieving loss of partner
    EstrangedEx             = 7  // Former partner with lingering tension or fond memories
};

enum class FriendshipTier : uint8_t {
    Stranger                = 0, // No social familiarity
    Acquaintance            = 1, // Familiar face on subway or street
    Coworker                = 2, // Colleague at Metacortex, Omnicorp, etc.
    CasualFriend            = 3, // Grabs lunch or coffee occasionally
    CloseFriend             = 4, // Shares personal worries, visits apartments
    BestFriendConfidant     = 5, // Unshakable trust, lends money, defends with life
    Rival                   = 6, // Competitive friction or professional jealousy
    Nemesis                 = 7  // Bitter hatred or betrayal
};

enum class MemoryCategory : uint8_t {
    ROMANCE_FIRST_DATE              = 0, // First dinner or park stroll together
    ROMANCE_PROPOSAL                = 1, // Romantic pledge or wedding commitment
    ROMANCE_HEARTBREAK              = 2, // Painful breakup, argument, or abandonment
    FRIENDSHIP_BOND                 = 3, // Heartfelt bonding moment or mutual support
    WORKPLACE_VENTING               = 4, // Blowing off steam after exhausting corporate shift
    WORKPLACE_ACHIEVEMENT           = 5, // Promotion, bonus, or team victory
    TRAUMA_WITNESS_GLITCH           = 6, // Witnessing impossible Matrix phenomena (green code, teleports)
    TRAUMA_VIOLENCE                 = 7, // Surviving SWAT breach, cartel shootout, or Agent pursuit
    DAILY_PLEASURE                  = 8  // Warm coffee, soothing jazz, delicious noodles
};

// ============================================================================
// Social Relationship Edge (Between Two Entities)
// ============================================================================

struct SocialRelationshipEdge {
    uint32 targetEntityId{0};
    std::string targetName;
    RomanceStage romance{RomanceStage::Single};
    FriendshipTier friendship{FriendshipTier::Stranger};
    
    float affinity{0.0f};          // -100.0 (hostile) to +100.0 (adoring love)
    float trust{10.0f};            // 0.0 (paranoid) to 100.0 (complete faith)
    float intimacy{0.0f};          // 0.0 (formal distance) to 100.0 (soulmates)
    
    uint32 interactionsCount{0};
    uint32 lastInteractionTimestamp{0}; // Simulation minute
    std::string relationshipNotes;      // e.g. "Met at Morrell Noodle Bar during rainstorm"
};

// ============================================================================
// Episodic Memory Node (Human-like Subjective Experience)
// ============================================================================

struct EpisodicMemoryNode {
    uint64 memoryId{0};
    uint32 timestampMinutes{0};   // Simulation clock in minutes
    std::string title;            // e.g. "First Date with Clara at Sakura Gardens"
    std::string narrativeProse;   // Vivid subjective reflection
    MemoryCategory category{MemoryCategory::DAILY_PLEASURE};
    
    float emotionalValence{0.0f}; // -1.0 (devastating trauma) to +1.0 (pure joy)
    float salience{0.5f};         // 0.0 (fading trivia) to 1.0 (indelible core memory)
    uint32 associatedEntityId{0}; // Partner, friend, or offender ID
    std::string locationName;     // Specific shop, park, or workplace
    bool isCoreMemory{false};     // Permanent, immune to cognitive fading
};

// ============================================================================
// Social Profile of an NPC / Citizen / Bot
// ============================================================================

struct SocialProfile {
    uint32 entityId{0};
    std::string entityName;
    bool isCitizen{true}; // true = BluepillCitizen, false = BotClient
    
    RomanceStage romanceStatus{RomanceStage::Single};
    uint32 partnerEntityId{0};
    std::string partnerName;
    
    // Social Graph Edges: otherEntityId -> Edge
    std::unordered_map<uint32, SocialRelationshipEdge> relationships;
    
    // Episodic Memory Stream
    std::vector<EpisodicMemoryNode> memories;
    
    // Favorite hangouts and places of affection
    std::vector<std::string> preferredHangouts;
    
    // Helper accessors
    const SocialRelationshipEdge* GetRelationship(uint32 targetId) const {
        auto it = relationships.find(targetId);
        return it != relationships.end() ? &it->second : nullptr;
    }
    
    SocialRelationshipEdge* GetRelationshipMut(uint32 targetId) {
        auto it = relationships.find(targetId);
        return it != relationships.end() ? &it->second : nullptr;
    }
};

// ============================================================================
// NPC Social Life Engine (Master Singleton)
// ============================================================================

class NPCSocialLifeEngine : public Singleton<NPCSocialLifeEngine> {
public:
    NPCSocialLifeEngine();
    ~NPCSocialLifeEngine();

    void Initialize();
    bool IsInitialized() const { return m_initialized; }
    void Reset(); // For tests and re-initialization

    // Profile Management
    SocialProfile& GetOrCreateProfile(uint32 entityId, const std::string& name, bool isCitizen = true);
    const SocialProfile* GetProfile(uint32 entityId) const;
    SocialProfile* GetProfileMut(uint32 entityId);

    // Compatibility Calculation (OCEAN Traits + Shared Hangouts + Alignment)
    float CalculateCompatibility(const CivilianOCEAN& a, const CivilianOCEAN& b,
                                 const std::vector<std::string>& hangoutsA = {},
                                 const std::vector<std::string>& hangoutsB = {}) const;

    // Love Life & Romantic Lifecycle
    bool InitiateFlirtation(uint32 initiatorId, uint32 targetId, float compatibilityScore = 0.6f);
    bool ScheduleAndExecuteDate(uint32 entityA, uint32 entityB, const std::string& locationName, const std::string& activityType);
    bool DeepenCommitment(uint32 entityA, uint32 entityB); // Dating -> InLove -> CommittedPartner/Married
    bool TriggerArgumentOrBreakup(uint32 entityA, uint32 entityB, const std::string& reason);
    bool Reconcile(uint32 entityA, uint32 entityB);

    // Friendship & Social Circle Dynamics
    bool FormFriendship(uint32 entityA, uint32 entityB, FriendshipTier tier = FriendshipTier::CasualFriend);
    bool VentWorkplaceStress(uint32 entityA, uint32 entityB);
    bool LendInfoCredits(uint32 lenderId, uint32 borrowerId, uint32 amount);
    bool ShareSecret(uint32 sharerId, uint32 listenerId, const std::string& secret);

    // Episodic Memory Stream
    uint64 RecordEpisodicMemory(uint32 entityId, const std::string& title, const std::string& prose,
                                MemoryCategory category, float valence, float salience,
                                uint32 otherId, const std::string& location, bool makeCore = false);
    std::vector<EpisodicMemoryNode> RetrieveCoreMemories(uint32 entityId) const;
    std::string TriggerLocationReminiscence(uint32 entityId, const std::string& currentLocation);

    // Periodic Update (Circadian tick integration)
    void Update(uint32 deltaMs);
    void UpdateCircadianSocialCycle(uint32 currentSimMinutes);

    // Telemetry & Reporting
    std::string GenerateSocialTelemetryReport() const;
    std::string GenerateEntitySocialSummary(uint32 entityId) const;
    std::string GenerateEntityMemoriesReport(uint32 entityId) const;

    size_t GetTotalProfilesCount() const;
    size_t GetTotalRomanceCount() const;
    size_t GetTotalFriendshipCount() const;
    size_t GetTotalMemoriesCount() const;

private:
    bool m_initialized{false};
    mutable std::mutex m_socialMutex;
    uint64 m_nextMemoryId{10001};
    uint32 m_accumulatedTimeMs{0};
    uint32 m_simulatedMinutes{0};

    // Profiles indexed by entity ID (GoId or Citizen ID)
    std::unordered_map<uint32, SocialProfile> m_profiles;

    // Fast random helper
    float GetRandomFloat() const {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }
};

#define sSocialEngine NPCSocialLifeEngine::getSingleton()

// Standalone C++ Automated Test Suite
void RunNPCSocialLifeTestSuite();

#endif // MXOEMU_NPC_SOCIAL_LIFE_ENGINE_H

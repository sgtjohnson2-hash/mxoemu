#ifndef MXOEMU_NPC_EMERGENT_LIFE_ENGINE_H
#define MXOEMU_NPC_EMERGENT_LIFE_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <sstream>

// ============================================================================
// Emergent Activities & Hobbies Enums
// ============================================================================

enum class EmergentActivityType : uint8_t {
    VISIT_ESPRESSO_BAR          = 0, // Park East Cafe: coffee, news, small talk
    ROOFTOP_MEDITATION_GAZING   = 1, // High-rise roof: contemplation, stargazing
    UNDERGROUND_FIGHT_CLUB      = 2, // Slums basement: martial arts, wagers
    NEON_NIGHTCLUB_DANCING      = 3, // Club Hel / Matrix disco: music, sensory rush
    GYM_MARTIAL_ARTS_TRAINING   = 4, // Dojo: Kung Fu practice, physical stamina
    LOCAL_MARKET_SHOPPING       = 5, // Street market: fresh food, artisan items
    LIBRARY_ANOMALY_RESEARCH    = 6, // Historic archives: researching glitch histories
    ZION_SYMPATHIZER_RALLY      = 7, // Abandoned terminal: dissident speech, whispers
    CHURCH_OF_THE_MACHINE       = 8, // Cathedral of Logic: reverence of systemic purity
    COMMUNITY_VOLUNTEERING      = 9  // Slums aid kitchen: distributing food & clothes
};

// ============================================================================
// Life Milestones & Volatility Enums
// ============================================================================

enum class LifeMilestoneType : uint8_t {
    WEDDING_CEREMONY            = 0, // Public marital bond & celebration
    CHILD_BIRTH_OR_ADOPTION     = 1, // Welcoming new child into family household
    WEDDING_ANNIVERSARY         = 2, // Reaffirming domestic love and shared trials
    MEMORIAL_SERVICE            = 3, // Mourning fallen kin at Morpheus Memorial
    ECONOMIC_WINDFALL           = 4, // Wealth windfall (+3000 bits), penthouse upgrade
    EVICTION_DOWNSIZING         = 5, // Debt distress, forced move to Slums tenement
    COGNITIVE_DISSONANCE_GLITCH = 6  // Witnessing Matrix anomaly, shattering illusion
};

// ============================================================================
// Awakening Trajectory & Ideological Drift Enums
// ============================================================================

enum class AwakeningStage : uint8_t {
    STAGE_0_BLUEPILL_SLEEPER    = 0, // Blissfully unaware, rationalizes glitches
    STAGE_1_MATRIX_SKEPTIC      = 1, // Notices green tint, deja vu, unexplainable events
    STAGE_2_AWAKENING_SEARCHER  = 2, // Seeks underground forums, phone booths, rumors
    STAGE_3_REDPILL_SYMPATHIZER = 3, // Harbors Zion operatives, acts as Megacity courier
    STAGE_4_FREED_MIND_OPERATIVE= 4, // Fully unplugged / recruited redpill combatant
    STAGE_CYPHERITE_RENEGADE    = 5  // Traumatized or bribed, wants illusion restored
};

// ============================================================================
// Word-of-Mouth Gossip & Rumor Enums
// ============================================================================

enum class RumorTopicType : uint8_t {
    FRANK_CASTLE_SIGHTING       = 0, // The Punisher hunting syndicate enforcers
    AGENT_SMITH_INFECTION       = 1, // Clones spreading viral corruption in sector
    POLICE_SWAT_CHECKPOINT      = 2, // MMPD barricades and tactical cordons
    CORPORATE_SCANDAL           = 3, // Metacortex embezzlement & layoff rumors
    ROMANCE_SCANDAL             = 4, // Breakups, secret proposals, illicit affairs
    MATRIX_CODE_GLITCH          = 5  // Flickering buildings, black cats crossing twice
};

// ============================================================================
// Data Structures
// ============================================================================

struct EmergentActivityRecord {
    EmergentActivityType type{EmergentActivityType::VISIT_ESPRESSO_BAR};
    std::string activityName;
    std::string venueLocationName;
    uint32 durationMinutes{60};
    float stressReliefScore{15.0f};
    float socialAffinityBonus{10.0f};
    float cognitiveImpact{0.0f}; // Positive raises skepticism/awakening
};

struct LifeMilestoneEvent {
    uint32 milestoneId{0};
    LifeMilestoneType type{LifeMilestoneType::WEDDING_CEREMONY};
    uint32 primaryEntityId{0};
    uint32 secondaryEntityId{0};
    uint32 timestampMinutes{0};
    std::string narrativeProse;
    double financialImpact{0.0};
    float happinessImpact{0.0f};
    std::string venueName;
};

struct AwakeningProfile {
    uint32 entityId{0};
    std::string citizenName;
    AwakeningStage stage{AwakeningStage::STAGE_0_BLUEPILL_SLEEPER};
    float cognitiveDissonance{0.0f};    // 0.0 to 1.0
    uint32 anomaliesWitnessedCount{0};
    float redpillTrust{0.0f};           // 0.0 to 100.0
    float machinePurityTrust{100.0f};   // 0.0 to 100.0
    bool hasHardlineContact{false};
    std::string latestAwakeningEpiphany;
};

struct SocialCircle {
    uint32 circleId{0};
    std::string circleName;
    std::string circleType; // "TenantsUnion", "PhilosophySalon", "FightClubCrew", "CoffeeRegulars"
    std::string meetingVenue;
    std::vector<uint32> memberIds;
    double pooledTreasuryBits{0.0};
    uint32 lastMeetingTimestamp{0};
    std::string currentObjective;
};

struct GossipRumor {
    uint32 rumorId{0};
    RumorTopicType topic{RumorTopicType::FRANK_CASTLE_SIGHTING};
    std::string headline;
    std::string narrativeDetails;
    uint32 originEntityId{0};
    float credibilityScore{0.8f}; // 0.0 to 1.0
    uint32 timesSpreadCount{0};
    uint32 creationTimestamp{0};
};

// ============================================================================
// NPCEmergentLifeEngine Singleton Class
// ============================================================================

class NPCEmergentLifeEngine : public Singleton<NPCEmergentLifeEngine>
{
public:
    NPCEmergentLifeEngine();
    ~NPCEmergentLifeEngine() = default;

    void Initialize();
    void Reset();

    // Activities & Hobbies
    EmergentActivityRecord GetActivityDefinition(EmergentActivityType type) const;
    EmergentActivityType RecommendActivityForPersonality(float openness, float conscientiousness,
                                                         float extraversion, float agreeableness, float neuroticism) const;
    bool ScheduleCitizenActivity(uint32 entityId, EmergentActivityType activity);
    bool ExecuteCurrentActivity(uint32 entityId, float& outStressRelief, float& outHappinessDelta);
    EmergentActivityType GetCitizenCurrentActivity(uint32 entityId) const;

    // Life Milestones
    LifeMilestoneEvent TriggerWeddingCeremony(uint32 spouseA, uint32 spouseB, const std::string& venueName = "Park East Chapel");
    LifeMilestoneEvent TriggerChildBirthOrAdoption(uint32 headEntityId, const std::string& childName, bool isAdoption = false);
    LifeMilestoneEvent TriggerWeddingAnniversary(uint32 spouseA, uint32 spouseB, uint32 yearsMarried);
    LifeMilestoneEvent TriggerMemorialService(uint32 mourningEntityId, const std::string& fallenKinName, const std::string& causeOfDeath);
    LifeMilestoneEvent TriggerEconomicWindfall(uint32 entityId, double windfallBits, const std::string& sourceDescription);
    LifeMilestoneEvent TriggerEvictionDownsizing(uint32 entityId, const std::string& landlordReason);
    LifeMilestoneEvent TriggerCognitiveDissonanceGlitch(uint32 entityId, const std::string& glitchDescription, float severity = 0.5f);

    // Awakening & Ideological Drift
    AwakeningProfile& GetOrCreateAwakeningProfile(uint32 entityId, const std::string& name = "");
    bool ProcessWitnessedAnomaly(uint32 entityId, const std::string& anomalyType, float anomalyIntensity);
    bool AdvanceAwakeningStage(uint32 entityId, AwakeningStage newStage, const std::string& epiphanyProse);
    bool TriggerCypheriteDisillusionment(uint32 entityId, const std::string& grievanceReason);
    AwakeningStage GetAwakeningStage(uint32 entityId);

    // Social Circles & Communities
    uint32 CreateSocialCircle(const std::string& name, const std::string& type, const std::string& venue, uint32 founderId);
    bool AddCitizenToCircle(uint32 circleId, uint32 entityId);
    bool ConductCircleMeeting(uint32 circleId, const std::string& agendaDiscussion);
    std::vector<SocialCircle> GetCitizenSocialCircles(uint32 entityId) const;
    const SocialCircle* GetSocialCircle(uint32 circleId) const;

    // Word-of-Mouth Gossip Diffusion
    uint32 SeedRumor(RumorTopicType topic, const std::string& headline, const std::string& details, uint32 originEntityId, float credibility = 0.85f);
    bool SpreadRumorBetweenEntities(uint32 tellerId, uint32 listenerId);
    std::vector<GossipRumor> GetEntityKnownRumors(uint32 entityId) const;
    std::vector<GossipRumor> GetTrendingRumors(size_t limit = 5) const;

    // Reporting & Telemetry
    std::string GenerateEmergentLifeSummary(uint32 entityId) const;
    std::string GenerateAwakeningReport(uint32 entityId) const;
    std::string GenerateGossipNetworkReport() const;
    std::string GenerateSocialCirclesReport() const;
    std::string GenerateMasterTelemetryReport() const;

    // Metrics
    size_t GetTotalAwakenedCount() const;
    size_t GetTotalMilestonesCount() const;
    size_t GetTotalSocialCirclesCount() const;
    size_t GetTotalRumorsCount() const;

    // Static Utility Helpers
    static std::string GetActivityTypeName(EmergentActivityType type);
    static std::string GetMilestoneTypeName(LifeMilestoneType type);
    static std::string GetAwakeningStageName(AwakeningStage stage);
    static std::string GetRumorTopicName(RumorTopicType topic);

private:
    bool m_initialized{false};
    mutable std::mutex m_engineMutex;

    uint32 m_nextMilestoneId{1001};
    uint32 m_nextCircleId{201};
    uint32 m_nextRumorId{3001};

    // Entity Current Activity: entityId -> EmergentActivityType
    std::unordered_map<uint32, EmergentActivityType> m_currentActivities;

    // Milestones history: milestoneId -> LifeMilestoneEvent
    std::unordered_map<uint32, LifeMilestoneEvent> m_milestones;
    // Fast lookup: entityId -> vector of milestoneIds
    std::unordered_map<uint32, std::vector<uint32>> m_entityMilestones;

    // Awakening profiles: entityId -> AwakeningProfile
    std::unordered_map<uint32, AwakeningProfile> m_awakeningProfiles;

    // Social Circles: circleId -> SocialCircle
    std::unordered_map<uint32, SocialCircle> m_socialCircles;

    // Gossip Rumors: rumorId -> GossipRumor
    std::unordered_map<uint32, GossipRumor> m_rumors;
    // Entity known rumor IDs: entityId -> unordered_set of rumorIds
    std::unordered_map<uint32, std::unordered_set<uint32>> m_entityKnownRumors;
};

#define sEmergentLifeEngine NPCEmergentLifeEngine::getSingleton()
#define sNPCEmergentLifeEngine NPCEmergentLifeEngine::getSingleton()

// Standalone C++ Automated Test Suite
void RunNPCEmergentLifeTestSuite();

#endif // MXOEMU_NPC_EMERGENT_LIFE_ENGINE_H

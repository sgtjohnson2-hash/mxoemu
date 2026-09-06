#ifndef MXOEMU_SLM_DIALOGUE_CONTEXT_ENGINE_H
#define MXOEMU_SLM_DIALOGUE_CONTEXT_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "BiographicalNarrativeEngine.h"
#include "NPCSocialLifeEngine.h"
#include "NPCFamilyDreamsEngine.h"
#include "NPCEmergentLifeEngine.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

// ============================================================================
// Epistemic Horizons & Meta-Knowledge Boundaries
// ============================================================================

enum class EpistemicHorizon : uint8
{
    HORIZON_BLUEPILL_SLEEPER      = 0, // Zero awareness of simulation, Zion, agents, pod harvesting
    HORIZON_MATRIX_SKEPTIC        = 1, // Notices code glitches, deja vu, paranoia; no full Zion lore
    HORIZON_REDPILL_OPERATIVE     = 2, // Full simulation truth, Zion hovercrafts, hardlines, agent evasion
    HORIZON_CYPHERITE_RENEGADE    = 3, // Disillusioned with Zion misery, seeks bluepill memory wipe
    HORIZON_MMPD_POLICE           = 4, // 10-codes, APBs for Castle, views redpills as cyber-terrorists
    HORIZON_SYNDICATE_UNDERWORLD  = 5, // Rackets, turf wars, mob families, terror of Frank Castle
    HORIZON_MACHINE_AGENT         = 6, // Simulation integrity, RSI anomalies; blind to emotional sacrifice
    HORIZON_EXILE_PROGRAM         = 7  // Causality, Matrix history v1/v2, contraband code algorithms
};

enum class EpistemicBreachAction : uint8
{
    NONE                          = 0,
    DISMISS_AS_ABSURD             = 1, // "Are you off your meds? Reality is right here."
    SUSPICIOUS_PARANOIA           = 2, // "Keep quiet... The walls might have ears."
    ALARM_CALL_POLICE             = 3, // "Security! We have a crazy person talking about terrorists!"
    AFFIRM_SKEPTICISM             = 4, // "You noticed the flickering streetlights too, didn't you?"
    CONFIRM_OPERATIVE_IDENTITY    = 5, // "Hardline frequency verified. What's your dispatch, operator?"
    CYPHERITE_CONTEMPT            = 6, // "Spare me the Zion prophecy garbage. Zion is cold sludge and death."
    ENFORCE_SYSTEM_PROTOCOL       = 7  // "Anomalous RSI detected. Commencing standard erasure."
};

// ============================================================================
// Situated Context & Dialogue Structures
// ============================================================================

struct SituatedDialogueContext
{
    uint32 speakerEntityId{0};
    std::string speakerName;
    EpistemicHorizon horizon{EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER};
    
    // Biographical & Personality
    std::string walkOfLife;
    std::string birthplace;
    std::string physicalQuirks;
    float openness{0.5f};
    float conscientiousness{0.5f};
    float extraversion{0.5f};
    float agreeableness{0.5f};
    float neuroticism{0.5f};

    // Domestic & Family
    uint32 householdId{0};
    std::string householdName;
    std::string kinshipRole;
    std::string spouseName;
    uint32 childrenCount{0};
    double familySavingsBits{0.0};

    // Career & Vocation
    std::string careerTrackName;
    std::string jobTitle;
    uint32 jobLevel{0};
    std::string employerName;
    float hourlyWage{25.0f};
    float burnoutLevel{0.0f};

    // Life Dreams & Purpose
    std::string activeDreamTitle;
    float dreamProgress{0.0f};
    bool isInCrisis{false};

    // Social & Interpersonal
    std::string interlocutorName;
    uint32 interlocutorId{0};
    std::string relationshipTier; // "Stranger", "Coworker", "CloseFriend", "Spouse", "Enemy"
    float interpersonalTrust{50.0f};

    // Episodic Memories (Top 3 relevant)
    std::vector<std::string> relevantMemories;

    // Gossip & Rumors (Strictly rumors personally heard)
    std::vector<std::string> knownRumors;

    // Routine & Leisure
    std::string currentActivityName;
    std::string currentVenue;

    // Awakening State
    AwakeningStage awakeningStage{AwakeningStage::STAGE_0_BLUEPILL_SLEEPER};
    float cognitiveDissonance{0.05f};
    bool hasHardlineContact{false};

    // Environment
    std::string districtName; // "Richland", "Downtown", "International", "The Slums"
    std::string timeOfDay;    // "Morning", "Afternoon", "Evening", "Night"
};

struct DialoguePromptPayload
{
    std::string systemPrompt;
    std::string epistemicProhibitions;
    std::string situatedDossier;
    std::string dialogueTurn;
    std::string jsonPayload; // Complete serialized JSON payload for LLM/SLM inference
};

struct DialogueResponseResult
{
    uint32 speakerEntityId{0};
    std::string speakerName;
    std::string interlocutorName;
    bool wasEpistemicBreachDetected{false};
    std::string breachedConcept;
    EpistemicBreachAction breachActionTaken{EpistemicBreachAction::NONE};
    std::string responseText;
    float emotionalImpactDelta{0.0f};
    bool episodicMemoryLogged{false};
};

// ============================================================================
// SLM Dialogue Context Engine Singleton
// ============================================================================

class SLMDialogueContextEngine : public Singleton<SLMDialogueContextEngine>
{
public:
    SLMDialogueContextEngine();
    ~SLMDialogueContextEngine() = default;

    void Initialize();
    void Reset();

    // Epistemic Classification
    EpistemicHorizon DetermineEpistemicHorizon(uint32 entityId, mxoFaction faction = FACTION_NONE);
    std::string GetEpistemicHorizonName(EpistemicHorizon horizon) const;
    const std::vector<std::string>& GetForbiddenTopics(EpistemicHorizon horizon) const;

    // Epistemic Guardrail Evaluation
    bool EvaluateEpistemicBreach(EpistemicHorizon horizon, const std::string& userUtterance,
                                 std::string& outBreachedKeyword, EpistemicBreachAction& outAction) const;

    // Situated Context Assembly
    SituatedDialogueContext AssembleSituatedContext(uint32 speakerEntityId, uint32 interlocutorId = 0,
                                                    const std::string& district = "Downtown",
                                                    const std::string& venue = "Sidewalk");

    // SLM Prompt Payload Composition
    DialoguePromptPayload ComposeSLMPromptPayload(const SituatedDialogueContext& ctx, const std::string& userUtterance);

    // Situated Dialogue Generation (Heuristic Guardrail + Local Simulation)
    bool GenerateSituatedDialogueResponse(uint32 npcEntityId, uint32 interlocutorId,
                                          const std::string& userUtterance,
                                          DialogueResponseResult& outResult,
                                          const std::string& district = "Downtown",
                                          const std::string& venue = "Sidewalk");

    // Telemetry & Metrics
    size_t GetTotalDialogueInferencesRun() const;
    size_t GetTotalEpistemicBreachesBlocked() const;
    std::string GenerateSLMTelemetryReport() const;

private:
    bool m_initialized{false};
    mutable std::mutex m_dialogueMutex;

    size_t m_totalInferences{0};
    size_t m_breachesBlocked{0};

    // Prohibited keyword dictionaries per horizon
    std::unordered_map<EpistemicHorizon, std::vector<std::string>> m_forbiddenKeywords;

    void InitializeForbiddenDictionaries();
    std::string GenerateHeuristicBreachReply(const SituatedDialogueContext& ctx, const std::string& breachedKeyword,
                                             EpistemicBreachAction action) const;
    std::string GenerateHeuristicInContextReply(const SituatedDialogueContext& ctx, const std::string& userUtterance) const;
};

#define sSLMDialogueEngine SLMDialogueContextEngine::getSingleton()

// Standalone C++ Automated Test Suite
void RunSLMDialogueTestSuite();

#endif // MXOEMU_SLM_DIALOGUE_CONTEXT_ENGINE_H

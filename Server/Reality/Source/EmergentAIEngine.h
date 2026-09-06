#ifndef MXOEMU_EMERGENT_AI_ENGINE_H
#define MXOEMU_EMERGENT_AI_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "CityLifeManager.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <random>

// ============================================================================
// Phase 1: Smart Object & Affordance Enums & Structures
// ============================================================================

enum class AffordanceType : uint8_t {
    REST_SIT            = 0,  // Park bench, subway concourse seat, diner booth
    EAT_FOOD            = 1,  // Noodle bar, food cart, deli counter
    CALL_PHONE          = 2,  // Payphone booth, public landline
    REPORT_CRIME        = 3,  // Police emergency callbox, precinct intercom
    ATM_TRANSACT        = 4,  // Bank ATM, cyber-teller machine
    TRASH_SCAVENGE      = 5,  // Alley dumpster, waste receptacle, refuse heap
    POWER_TAP           = 6,  // High-voltage transformer, circuit box, breaker
    VENDING_DRINK       = 7,  // Soda automat, coffee machine
    TAKE_COVER          = 8,  // Ballistic planter, concrete pillar, dumpster barrier
    NEWS_STAND          = 9,  // Electronic news terminal, paper rack
    DEAD_DROP           = 10, // Hidden loose brick, hollow vent, safehouse cache
    OPERATOR_HARDLINE   = 11  // Hardline telephone booth for Matrix extraction
};

enum class SmartObjectStatus : uint8_t {
    OPERATIONAL         = 0,
    OCCUPIED            = 1,
    OUT_OF_SERVICE      = 2,
    DAMAGED_GLITCHING   = 3,
    LOCKED_DOWN         = 4
};

struct AffordanceDefinition {
    AffordanceType type{AffordanceType::REST_SIT};
    std::string actionName{"Rest and Sit"};
    uint32 durationMs{8000};
    float baseUtility{0.6f};

    // Biometric & economic deltas upon completion
    float deltaHunger{0.0f};
    float deltaFatigue{0.0f};
    float deltaStress{0.0f};
    int32 deltaWalletInfo{0};

    // Environmental & social constraints
    uint32 maxConcurrentUsers{1};
    float minLocalDanger{0.0f};
    float maxLocalDanger{1.0f};
    bool allowsPanickedUsers{false};
    bool requiresCleanRecord{false};
};

struct SmartObject {
    uint32 objectId{0};
    std::string name;
    std::string typeTag; // "PhoneBooth", "ParkBench", "NoodleCounter", "ATM", "PoliceCallbox", "Dumpster", "Transformer", "VendingMachine", "NewsStand", "DeadDrop"
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector position;
    float interactionRadius{4.5f};
    SmartObjectStatus status{SmartObjectStatus::OPERATIONAL};

    std::vector<AffordanceDefinition> affordances;

    uint32 maxCapacity{1};
    std::vector<uint32> activeUsers;
    std::map<uint32, uint32> userTimersMs;
    std::map<uint32, AffordanceType> userAffordanceType;
    std::map<uint32, uint32> reservationTimersMs; // EntityId -> Remaining reservation timeout (ms)

    uint32 totalInteractionsCompleted{0};
    bool isGlitching{false};
    uint32 glitchTimerMs{0};

    void SetGlitching(bool glitch) {
        isGlitching = glitch;
        status = glitch ? SmartObjectStatus::DAMAGED_GLITCHING : SmartObjectStatus::OPERATIONAL;
    }

    bool HasAffordance(AffordanceType type) const {
        for (const auto& aff : affordances) {
            if (aff.type == type) return true;
        }
        return false;
    }

    const AffordanceDefinition* GetAffordance(AffordanceType type) const {
        for (const auto& aff : affordances) {
            if (aff.type == type) return &aff;
        }
        return nullptr;
    }

    bool IsAvailable() const {
        return (status == SmartObjectStatus::OPERATIONAL || status == SmartObjectStatus::OCCUPIED) &&
               activeUsers.size() < maxCapacity;
    }
};

class SmartObjectAffordanceGrid {
public:
    static constexpr float GRID_CELL_SIZE = 500.0f;

    void RegisterObject(const SmartObject& obj);
    bool UnregisterObject(uint32 objectId);
    SmartObject* GetObject(uint32 objectId);
    const std::map<uint32, SmartObject>& GetAllObjects() const { return m_objects; }

    std::vector<SmartObject*> FindObjectsInRadius(const LocationVector& pos, float radius, AffordanceType affordanceType);
    SmartObject* FindNearestAvailableObject(const LocationVector& pos, AffordanceType affordanceType, float maxRadius = 1000.0f, uint32 entityId = 0);

    bool TryReserve(uint32 objectId, AffordanceType affordanceType, uint32 entityId, uint32 reservationTimeoutMs = 15000);
    bool BeginInteraction(uint32 objectId, AffordanceType affordanceType, uint32 entityId);
    void UpdateInteractions(uint32 deltaMs, std::function<void(uint32 entityId, const AffordanceDefinition& affordance)> onCompleteCallback);
    bool Release(uint32 objectId, uint32 entityId);
    size_t GetActiveReservationCount(uint32 objectId) const;

    size_t GetTotalObjectCount() const { return m_objects.size(); }
    void Clear();

private:
    int64_t GetCellKey(float x, float z) const;

    std::map<uint32, SmartObject> m_objects;
    std::unordered_map<int64_t, std::vector<uint32>> m_spatialGrid;
    mutable std::recursive_mutex m_gridMutex;
};

// ============================================================================
// Phase 2: Social Contagion & Rumor Mutation Enums & Structures
// ============================================================================

enum class RumorMutationType : uint8_t {
    NONE                = 0,
    HYSTERIA_ESCALATION = 1, // Exaggerates danger, body counts, threats (high Neuroticism)
    FACTUAL_DISTORTION  = 2, // Garbles factions, locations, names (low Conscientiousness)
    MATRIX_ANOMALY      = 3, // Injects sci-fi, green code, supernatural glitches (high Openness)
    VIRAL_AMPLIFICATION = 4  // Rapid proliferation, dramatic rumor spin (high Extraversion)
};

struct RumorGene {
    uint32 rumorId{0};
    uint32 topicId{0}; // Maps to RumorTopic
    std::string rootHeadline;
    std::string rootNarrative;
    std::string currentHeadline;
    std::string currentNarrative;

    uint32 originDistrictId{1};
    uint32 currentDistrictId{1};
    uint64 originTimestampMs{0};

    uint32 generationCount{0}; // Number of transmission hops
    float truthValue{1.0f};    // 1.0 = objective ground truth, degrades per hop
    float virulence{0.75f};    // Transmission likelihood [0.0, 1.0]
    float panicWeight{0.30f};  // Emotional alarm score [0.0, 1.0]

    std::vector<std::string> mutationHistory;
    std::set<uint32> carrierEntityIds;
    RumorMutationType lastMutationType{RumorMutationType::NONE};
};

struct PanicContagionWave {
    uint32 eventId{0};
    LocationVector epicenter;
    float radius{65.0f};
    float initialIntensity{0.85f};
    float peakIntensity{0.85f};
    std::string threatDescription;
    uint32 durationMs{30000};
    uint32 elapsedMs{0};
    bool isActive{true};
};

class SocialContagionEngine {
public:
    SocialContagionEngine();
    ~SocialContagionEngine();

    void Initialize();
    void Update(uint32 deltaMs);

    // Rumor Lifecycle & Genetic Mutation
    uint32 SeedRumor(uint32 topicId, const std::string& headline, const std::string& narrative, uint32 districtId, float initialPanic = 0.2f);
    bool AttemptRumorTransmission(uint32 rumorId, uint32 speakerId, uint32 listenerId,
                                  const CivilianOCEAN& speakerTraits,
                                  const CivilianOCEAN& listenerTraits,
                                  float distanceMeters);
    bool DebunkRumor(uint32 rumorId, uint32 debunkerId, const CivilianOCEAN& debunkerTraits);
    RumorGene* GetRumor(uint32 rumorId);
    const std::map<uint32, RumorGene>& GetAllRumors() const { return m_rumors; }
    size_t GetTotalRumorCount() const { return m_rumors.size(); }
    size_t GetCarrierCount(uint32 rumorId) const;

    // Panic Wave Dynamics
    uint32 EmitPanicWave(const LocationVector& epicenter, float radius, float intensity, const std::string& threat, uint32 durationMs = 30000);
    bool EvaluatePanicInfection(uint32 citizenId, const LocationVector& citizenPos, const CivilianOCEAN& traits, float localDanger, float& outPanicIntensity);
    size_t GetActivePanicEventsCount() const;
    void ClearPanic();

    // Cross-system statistics
    uint32 GetTotalRumorMutations() const { return m_totalMutations; }
    uint32 GetTotalPanicWavesEmitted() const { return m_totalPanicWaves; }

private:
    void MutateRumorNarrative(RumorGene& rumor, const CivilianOCEAN& speakerTraits);

    std::map<uint32, RumorGene> m_rumors;
    std::vector<PanicContagionWave> m_panicWaves;

    uint32 m_nextRumorId{1};
    uint32 m_nextWaveId{1};
    uint32 m_totalMutations{0};
    uint32 m_totalPanicWaves{0};

    mutable std::recursive_mutex m_contagionMutex;
};

// ============================================================================
// Phase 3: Anti-Flicker Hysteresis Utility AI Curves
// ============================================================================

enum class UtilityCurveType : uint8_t {
    LINEAR              = 0,
    POLYNOMIAL          = 1, // x^k
    LOGISTIC_SIGMOID    = 2, // 1 / (1 + exp(-k * (x - x0)))
    EXPONENTIAL         = 3, // (exp(k * x) - 1) / (exp(k) - 1)
    INVERSE_EXPONENTIAL = 4, // 1 - exp(-k * x)
    STEP_THRESHOLD      = 5  // x >= x0 ? 1 : 0
};

struct UtilityCurve {
    UtilityCurveType type{UtilityCurveType::LINEAR};
    float slope{1.0f};     // k or m
    float midpoint{0.5f};  // x0
    float exponent{2.0f};  // power k
    float offset{0.0f};    // base offset

    float Evaluate(float input) const;
};

enum class EmergentActionId : uint8_t {
    // Civilian Actions
    CIV_IDLE_STROLL         = 0,
    CIV_SEEK_FOOD           = 1,
    CIV_SEEK_REST           = 2,
    CIV_WORK_SHIFT          = 3,
    CIV_COMMUTE             = 4,
    CIV_RETAIL_SHOPPING     = 5,
    CIV_SOCIAL_GOSSIP       = 6,
    CIV_REPORT_CRIME_CALLBOX= 7,
    CIV_PANIC_FLEE          = 8,
    CIV_TAKE_SHELTER        = 9,
    CIV_OPERATOR_EXTRACTION = 10,

    // Gang Enforcer Actions
    GANG_IDLE_TURF_WATCH    = 11,
    GANG_EXTORT_COMMERCE    = 12,
    GANG_DEAL_CONTRABAND    = 13,
    GANG_AMBUSH_RIVALS      = 14,
    GANG_DEFEND_RACKET      = 15,
    GANG_FLEE_CASTLE        = 16,
    GANG_SURRENDER_SWAT     = 17,

    // Police Officer Actions
    COP_ROUTINE_PATROL      = 18,
    COP_INVESTIGATE_10CODE  = 19,
    COP_INTERVENE_CRIME     = 20,
    COP_CALL_SWAT_BACKUP    = 21,
    COP_TACTICAL_BREACH     = 22,
    COP_TAKE_SYNDICATE_BRIBE= 23
};

struct ActionUtilityConfig {
    EmergentActionId actionId;
    std::string name;
    UtilityCurve primaryCurve;
    float activationThreshold{0.60f};   // theta_enter: Utility needed to enter action
    float deactivationThreshold{0.35f}; // theta_exit: Utility below which action is abandoned
    float inertiaBonus{0.18f};          // M: Stickiness bonus for current action
    uint32 minCommitmentMs{5000};       // Minimum duration before switching
    bool isEmergencyOverride{false};    // Bypasses commitment timer if acute
};

struct AgentUtilityState {
    uint32 entityId{0};
    EmergentActionId currentAction{EmergentActionId::CIV_IDLE_STROLL};
    uint32 currentActionDurationMs{0};
    float currentActionUtility{0.5f};
    uint32 totalSwitchesPreventedByHysteresis{0};
    uint32 totalSwitchesExecuted{0};
};

class UtilityAICurveEngine {
public:
    UtilityAICurveEngine();
    ~UtilityAICurveEngine();

    void Initialize();

    EmergentActionId EvaluateCivilianAction(AgentUtilityState& state,
                                           const BluepillCitizen& citizen,
                                           float localDanger,
                                           bool hasNearbyCrime,
                                           bool hasAvailableCallbox,
                                           bool hasAvailableFood,
                                           bool hasAvailableBench,
                                           uint32 deltaMs);

    EmergentActionId EvaluateGangAction(AgentUtilityState& state,
                                       float localGangPower,
                                       float rivalGangPower,
                                       float localHeat,
                                       bool isCastleSpotted,
                                       bool isSWATSurrounding,
                                       bool hasRacketUnderAttack,
                                       uint32 deltaMs);

    EmergentActionId EvaluatePoliceAction(AgentUtilityState& state,
                                         float precinctCorruption,
                                         bool hasActive10Code,
                                         bool hasCrimeInProgress,
                                         bool isOutnumbered,
                                         bool isTacticalCall,
                                         uint32 deltaMs);

    const ActionUtilityConfig* GetActionConfig(EmergentActionId action) const;
    static std::string GetActionName(EmergentActionId action);

private:
    std::map<EmergentActionId, ActionUtilityConfig> m_actionConfigs;
    mutable std::recursive_mutex m_utilityMutex;
};

// ============================================================================
// Phase 4: Tiered LOD Simulation Culling Enums & Structures
// ============================================================================

enum class LODTier : uint8_t {
    LOD0_ACTIVE_VIEWPORT    = 0, // < 50m: 100% tick frequency, full physics/steering/affordances
    LOD1_VICINITY           = 1, // 50m - 200m: 10 Hz (every 100ms), coarse spatial hash
    LOD2_BACKGROUND         = 2, // 200m - 1000m: 1 Hz (every 1000ms), macro waypoint interpolation
    LOD3_CULLED_VIRTUAL     = 3  // > 1000m: 0 Hz (dormant), purely analytical catch-up on awake
};

struct ObserverFocusPoint {
    uint32 observerId{0};
    std::string name;
    LocationVector position;
    float activeRadius{50.0f};
};

struct SimulatedEntityLODState {
    uint32 entityId{0};
    LODTier currentTier{LODTier::LOD3_CULLED_VIRTUAL};
    float distanceToNearestObserver{99999.0f};
    uint32 timeSinceLastTickMs{0};
    uint32 totalAccumulatedVirtualMs{0};
    bool needsStateCatchup{false};
};

class TieredLODSimulationManager {
public:
    TieredLODSimulationManager();
    ~TieredLODSimulationManager();

    void RegisterObserver(uint32 observerId, const std::string& name, const LocationVector& pos);
    void UpdateObserverPosition(uint32 observerId, const LocationVector& pos);
    void RemoveObserver(uint32 observerId);
    const std::map<uint32, ObserverFocusPoint>& GetAllObservers() const { return m_observers; }

    LODTier ComputeEntityLOD(const LocationVector& entityPos, float& outNearestDist) const;
    void UpdateEntityLOD(SimulatedEntityLODState& lodState, const LocationVector& entityPos);

    bool ShouldEntityTick(SimulatedEntityLODState& lodState, uint32 deltaMs);
    void ApplyAnalyticalCatchUp(BluepillCitizen& citizen, uint32 elapsedVirtualMs);

    size_t GetEntityCountInLOD(LODTier tier) const;
    void RegisterManagedEntity(uint32 entityId, LODTier initialTier = LODTier::LOD3_CULLED_VIRTUAL);
    void SetEntityLOD(uint32 entityId, LODTier tier);

    void SetLODThresholds(float lod0, float lod1, float lod2);
    float GetLOD0Radius() const { return m_lod0Radius; }
    float GetLOD1Radius() const { return m_lod1Radius; }
    float GetLOD2Radius() const { return m_lod2Radius; }

private:
    std::map<uint32, ObserverFocusPoint> m_observers;
    std::map<uint32, LODTier> m_entityTierRegistry;
    float m_lod0Radius{50.0f};
    float m_lod1Radius{200.0f};
    float m_lod2Radius{1000.0f};
    mutable std::recursive_mutex m_lodMutex;
};

// ============================================================================
// Phase 5: EmergentAIEngine Orchestrator Singleton
// ============================================================================

class EmergentAIEngine : public Singleton<EmergentAIEngine> {
public:
    friend void RunEmergentAITestSuite();

    EmergentAIEngine();
    ~EmergentAIEngine();

    void Initialize();
    void Update(uint32 deltaMs);
    void UpdateManagedCitizens(uint32 deltaMs);

    SmartObjectAffordanceGrid& GetAffordanceGrid() { return m_affordanceGrid; }
    SocialContagionEngine& GetContagionEngine() { return m_contagionEngine; }
    UtilityAICurveEngine& GetUtilityEngine() { return m_utilityEngine; }
    TieredLODSimulationManager& GetLODManager() { return m_lodManager; }

    // Multi-Agent Cross-System Collision Triggers
    void OnEmergentCrimeDetected(uint32 crimeId, uint32 districtId, const LocationVector& crimePos, const std::string& crimeDesc);
    void OnPolice10CodeDispatched(uint32 precinctId, const std::string& tenCode, const LocationVector& targetPos);
    void OnFrankCastleAmbushExecuted(const LocationVector& ambushPos, const std::string& targetDesc, bool skullLeft);
    void OnGunfireEcho(const LocationVector& originPos, float loudnessRadius, const std::string& sourceDesc);

    // Agent State Accessors
    AgentUtilityState* GetOrCreateUtilityState(uint32 entityId);
    SimulatedEntityLODState* GetOrCreateLODState(uint32 entityId);

    // Diagnostics, Persistence & Reporting
    std::string GenerateEmergentTelemetryReport() const;
    bool SaveEmergentStateToFile(const std::string& path = "EmergentAISimulation.json");
    bool LoadEmergentStateFromFile(const std::string& path = "EmergentAISimulation.json");

    uint32 GetCrossSystemEventsTriggered() const { return m_crossSystemEventsTriggered; }
    uint32 GetCrimesReportedViaCallboxes() const { return m_crimesReportedViaCallboxes; }
    uint32 GetHysteresisSwitchesPrevented() const { return m_hysteresisSwitchesPrevented; }

private:
    void PopulateDefaultSmartObjects();
    void InitializeDefaultObservers();

    SmartObjectAffordanceGrid m_affordanceGrid;
    SocialContagionEngine m_contagionEngine;
    UtilityAICurveEngine m_utilityEngine;
    TieredLODSimulationManager m_lodManager;

    std::map<uint32, AgentUtilityState> m_civilianUtilityStates;
    std::map<uint32, SimulatedEntityLODState> m_citizenLODStates;

    uint32 m_crossSystemEventsTriggered{0};
    uint32 m_crimesReportedViaCallboxes{0};
    uint32 m_hysteresisSwitchesPrevented{0};

    mutable std::recursive_mutex m_mutex;
};

#define sEmergentAIMgr EmergentAIEngine::getSingleton()

// Test Suite Declaration
void RunEmergentAITestSuite();

#endif // MXOEMU_EMERGENT_AI_ENGINE_H

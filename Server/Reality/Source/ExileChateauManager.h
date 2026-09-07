#ifndef MXOEMU_EXILE_CHATEAU_MANAGER_H
#define MXOEMU_EXILE_CHATEAU_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <sstream>
#include <cstdint>

// ============================================================================
// Megacity Exile Syndicate & Club Hel Enums & Constants
// ============================================================================

enum class ExileFactionType : uint8_t {
    MerovingianHighCourt    = 0, // Le Mérovingien, Chateau alpine estate, causality brokers
    ClubHelEnforcers        = 1, // Subterranean BDSM techno club, inverted gravity bouncers
    TrainmanTransitGuild    = 2, // Mobil Ave limbo station, firewall smugglers
    NightmareVampires       = 3, // Version 2.0 blood-code siphons, silver & stake vulnerabilities
    NightmareLupines        = 4, // Version 2.0 werewolf kinetic berserkers, silver vulnerability
    PhantomTwins            = 5, // Version 2.0 phase-shifting specters, collision-layer bypass
    RenegadeKeymakers       = 6  // Rogue locksmiths, golden key compilers
};

enum class SupernaturalCreatureType : uint8_t {
    None                    = 0,
    VampireAristocrat       = 1, // High evasion, rapier melee, blood-code vitality drain
    LupineBerserker         = 2, // Heavy kinetic charge, frenzy knockdown, bullet stagger immune
    SpectralPhantom         = 3, // Collision-layer 0 phase-shift, 100% kinetic immunity in motion
    StoneGargoyle           = 4  // Rooftop camouflage, 80% kinetic armor hardening
};

enum class BackdoorDoorState : uint8_t {
    Locked                  = 0,
    KeyInserted             = 1,
    DimensionalRiftActive   = 2,
    AgentSuppressed         = 3
};

enum class MobilAveTransitStatus : uint8_t {
    IdleAtPlatform          = 0,
    BoardingGhostTrain      = 1,
    InLimboTransit          = 2,
    FirewallSmuggled        = 3,
    ArrivedDestination      = 4
};

enum class SensoryPayloadType : uint8_t {
    OrgasmFondant           = 0, // Overloads dopamine/serotonin telemetry; hyper-suggestibility
    AdrenalineBordeaux      = 1, // +30% evasion, +25% melee attack speed, RSI heat cost
    MemoryBrandy            = 2, // Infuses deleted human memories; grants temporary skill mastery
    CodeVermouth            = 3  // Calms neural telemetry, dampens fear, blocks panic debuffs
};

// ============================================================================
// Core Exile Data Structures
// ============================================================================

struct SupernaturalProgram {
    uint32 programId{0};
    std::string codeName;
    std::string title;           // e.g., "Vlad the Impaler", "Cujo the Hound", "Cain", "Abel"
    SupernaturalCreatureType creatureType{SupernaturalCreatureType::None};
    ExileFactionType faction{ExileFactionType::NightmareVampires};
    uint32 originMatrixVersion{2}; // 1 = Paradise, 2 = Nightmare, 3 = Modern Megacity
    
    float maxRsiVitality{1000.0f};
    float currentRsiVitality{1000.0f};
    float rsiBloodDrainRate{45.0f};      // Bytes/sec siphoned from target
    float silverDamageMultiplier{3.5f};  // Damage multiplier taken from silver-coated munitions
    float kineticArmorMitigation{0.0f};  // 0.80f for Gargoyles
    
    bool isPhaseShifted{false};          // Phase-shift active (Twins/Phantoms)
    float phaseShiftDurationSec{8.0f};
    float phaseShiftCooldownSec{15.0f};
    uint64 lastPhaseShiftTimeMs{0};
    
    bool isBerserk{false};               // Lupine rage mode
    float knockdownChance{0.65f};
    
    LocationVector currentLocation;
    std::string assignedSanctuary;       // "Chateau Dining Hall", "Club Hel Pit", "Mobil Ave Limbo"
    bool isAlive{true};
    uint32 botGoId{0};
};

struct ExileBackdoorPortal {
    uint32 portalId{0};
    std::string portalName;
    uint32 sourceDistrictId{1};
    LocationVector sourceLocation;
    uint32 targetDistrictId{2};
    LocationVector targetLocation;
    
    BackdoorDoorState state{BackdoorDoorState::Locked};
    uint32 requiredKeyTemplateId{9001};
    uint32 remainingKeyCharges{3};
    float anomalyTelemetryRipple{0.0f}; // 0.0 to 1.0 (triggers Agent breach at > 0.85)
    bool isBreachedByAgents{false};
};

struct GoldenKeyItem {
    uint32 keyInstanceId{0};
    std::string keyName;
    uint32 targetPortalId{0};
    uint32 maxCharges{5};
    uint32 remainingCharges{5};
    bool isMasterKey{false};            // The Keymaker's personal master key
};

struct CausalityRecord {
    uint32 recordId{0};
    uint32 playerId{0};
    std::string playerAction;
    float causalityDebtScore{0.0f};     // Positive = Merovingian owes favor; Negative = player owes French Court
    std::string scheduledReaction;
    bool isResolved{false};
    uint64 timestampMs{0};
};

struct MobilTrainTicket {
    uint32 ticketId{0};
    uint32 passengerEntityId{0};
    std::string passengerName;
    bool isContrabandCode{false};
    std::string smuggledPayloadDescription; // e.g. "Decommissioned Sentinel Core", "Deleted Oracle Subroutine"
    MobilAveTransitStatus status{MobilAveTransitStatus::IdleAtPlatform};
    uint64 departureTimeMs{0};
};

struct ClubHelEnforcer {
    uint32 enforcerId{0};
    std::string name;
    std::string roleTitle;               // "Coat Check Bouncer", "VIP Gatekeeper", "Ceiling Stalker"
    SupernaturalCreatureType creatureType{SupernaturalCreatureType::VampireAristocrat};
    bool isInvertedGravityActive{false}; // True if walking on ceilings/walls
    float combatBpmSyncRating{140.0f};   // Beats per minute combat cadence
    LocationVector position;
    bool isHostile{false};
    uint32 botGoId{0};
};

struct PersephoneFavorContract {
    uint32 contractId{0};
    uint32 redpillPlayerId{0};
    std::string requestedReward;        // "Golden Key to Source Hallway", "Silver Bullet Blueprint", "Vampire Code Cipher"
    bool isKissDelivered{false};
    float emotionalAuthenticityScore{0.0f}; // 0.0 to 1.0 (requires > 0.70 for genuine passion)
    bool isBetrayalAgainstMerovingian{true};
    bool isCompleted{false};
};

// ============================================================================
// Megacity Exile Chateau & Club Hel Manager (Master Singleton)
// ============================================================================

class ExileChateauManager : public Singleton<ExileChateauManager> {
public:
    ExileChateauManager();
    ~ExileChateauManager();

    void Initialize();
    bool IsInitialized() const { return m_initialized; }
    void Reset();
    void Update(uint32 deltaMs);

    // High Court & Causality Engine
    float RecordCausalityAction(uint32 playerId, const std::string& action, float impactDelta, const std::string& reaction);
    float GetPlayerCausalityDebt(uint32 playerId) const;
    bool ResolveCausalityReaction(uint32 playerId);
    
    bool BrewSensoryPayload(SensoryPayloadType type, uint32 targetEntityId, float& outDopamineBoost, float& outEvasionBoost);

    // Supernatural Bestiary Management
    bool RegisterSupernaturalProgram(const SupernaturalProgram& prog);
    const SupernaturalProgram* GetProgram(uint32 programId) const;
    SupernaturalProgram* GetProgramMut(uint32 programId);
    
    // Combat mechanics
    float ApplyDamageToSupernatural(uint32 programId, float rawDamage, bool isSilver, bool isWoodenStake, bool& outDeRezzed);
    float ProcessBloodCodeSiphon(uint32 vampireProgramId, uint32 targetVictimId, float durationSec);
    bool TriggerLupineBerserkFrenzy(uint32 lupineProgramId);
    bool TogglePhantomPhaseShift(uint32 phantomProgramId, bool enablePhase);

    // The Keymaker's Quantum Backdoor Hallways
    bool RegisterBackdoorPortal(const ExileBackdoorPortal& portal);
    const ExileBackdoorPortal* GetBackdoorPortal(uint32 portalId) const;
    ExileBackdoorPortal* GetBackdoorPortalMut(uint32 portalId);
    bool UnlockBackdoorPortal(uint32 portalId, uint32 keyInstanceId, LocationVector& outExitCoordinate);
    bool GenerateAnomalyRipple(uint32 portalId, float rippleDelta);
    bool SuppressPortalByAgents(uint32 portalId);

    // Golden Key Management
    bool RegisterGoldenKey(const GoldenKeyItem& key);
    const GoldenKeyItem* GetGoldenKey(uint32 keyInstanceId) const;
    GoldenKeyItem* GetGoldenKeyMut(uint32 keyInstanceId);

    // Mobil Ave Limbo & Trainman Transit
    uint32 IssueTrainTicket(uint32 passengerEntityId, const std::string& passengerName, bool isContraband, const std::string& payload);
    bool BoardGhostTrain(uint32 ticketId);
    bool ProcessMobilTransitLoop(uint32 ticketId);
    const MobilTrainTicket* GetTrainTicket(uint32 ticketId) const;
    bool IsTrainmanInvulnerableInMobilAve() const { return m_trainmanInLimboInvulnerable; }

    // Club Hel & Inverted Gravity Arena
    bool RegisterClubHelEnforcer(const ClubHelEnforcer& enforcer);
    const ClubHelEnforcer* GetClubHelEnforcer(uint32 enforcerId) const;
    ClubHelEnforcer* GetClubHelEnforcerMut(uint32 enforcerId);
    bool InvertGravityForCombatant(uint32 enforcerId, bool inverted);
    size_t GetActiveInvertedEnforcerCount() const;

    // Persephone's Kiss & Emotional Extraction
    uint32 OfferPersephoneContract(uint32 playerId, const std::string& desiredReward);
    bool DeliverPersephoneKiss(uint32 contractId, float emotionalAuthenticity, std::string& outResponseMessage);
    const PersephoneFavorContract* GetPersephoneContract(uint32 contractId) const;

    // Cross-Engine & Underworld Hooks
    bool ProcessMafiaTributeToExiles(uint8_t mafiaFamilyId, double tributeBits, std::string& outGrantedPerk);
    bool ExecuteFrankCastleVampireRaid(uint32 vampireDenDistrictId, uint32& outVampiresPurged);

    // Diagnostic & Telemetry Reports
    std::string GenerateExileCourtReport() const;
    std::string GenerateClubHelStatusReport() const;
    std::string GenerateBackdoorCorridorsReport() const;
    std::string GenerateMobilAveLimboReport() const;
    std::string GenerateSupernaturalBestiaryReport() const;

private:
    void InitializeExileHierarchy();
    void InitializeNightmareBestiary();
    void InitializeBackdoorNetwork();
    void InitializeClubHelArena();

    mutable std::mutex m_exileMutex;
    bool m_initialized{false};

    // State collections
    std::map<uint32, SupernaturalProgram> m_supernaturals;
    std::map<uint32, ExileBackdoorPortal> m_backdoorPortals;
    std::map<uint32, GoldenKeyItem> m_goldenKeys;
    std::map<uint32, CausalityRecord> m_causalityRecords;
    std::map<uint32, MobilTrainTicket> m_trainTickets;
    std::map<uint32, ClubHelEnforcer> m_clubHelEnforcers;
    std::map<uint32, PersephoneFavorContract> m_persephoneContracts;

    // ID Sequences
    uint32 m_nextProgramId{3001};
    uint32 m_nextPortalId{8001};
    uint32 m_nextKeyId{9001};
    uint32 m_nextCausalityId{4001};
    uint32 m_nextTicketId{6001};
    uint32 m_nextEnforcerId{5001};
    uint32 m_nextContractId{2001};

    bool m_trainmanInLimboInvulnerable{true};
    double m_totalContrabandBitsSmuggled{0.0};
    uint32 m_totalBackdoorTransits{0};
    uint32 m_totalVampiresDeRezzedBySilver{0};
};

#define sExileMgr ExileChateauManager::getSingleton()

void RunExileChateauTestSuite();

#endif // MXOEMU_EXILE_CHATEAU_MANAGER_H

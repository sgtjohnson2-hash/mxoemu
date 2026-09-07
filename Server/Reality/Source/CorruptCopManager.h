#ifndef MXOEMU_CORRUPT_COP_MANAGER_H
#define MXOEMU_CORRUPT_COP_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <algorithm>

// ============================================================================
// Corrupt Cops & Internal Affairs Enums
// ============================================================================

enum class CorruptionTier : uint8_t {
    CLEAN_IDEALIST           = 0, // Refuses all bribes, potential whistleblower
    COMPROMISED_ROOKIE       = 1, // Takes minor kickbacks, ignores low-level vice
    ACTIVE_BAGMAN            = 2, // Collects and routes scheduled syndicate envelopes
    CREATIVE_INVESTIGATOR    = 3, // Skims evidence, tampers with logs, plants drop guns
    CABAL_COMMANDER          = 4, // Precinct Captain / Commander directing mob protection
    UNTOUCHABLE_KINGPIN      = 5  // High-ranking brass on direct mob retainers
};

enum class WhistleblowerStatus : uint8_t {
    NONE                     = 0,
    COLLECTING_EVIDENCE      = 1, // Secretly logging corruption into evidence binder
    SUSPECTED_BY_CABAL       = 2, // Cabal commanders suspicious of leaks
    UNDER_RETALIATION        = 3, // Assigned hazardous solo calls without backup
    IAB_PROTECTED_WITNESS    = 4, // Secured by Internal Affairs in safehouse
    SILENCED_OR_MISSING      = 5  // Murdered or intimidated into silence
};

enum class BribeDropState : uint8_t {
    SCHEDULED                = 0,
    EN_ROUTE                 = 1,
    DELIVERED                = 2,
    AMBUSHED_BY_CASTLE       = 3,
    STUNG_BY_IAB             = 4,
    FAILED                   = 5
};

enum class ContrabandCategory : uint8_t {
    SEIZED_FIREARMS          = 0,
    MILITARY_EXPLOSIVES      = 1,
    STOLEN_BEARER_BONDS      = 2,
    SYNTHETIC_NARCOTICS      = 3,
    CORRUPTED_CODE_RELICS    = 4
};

enum class StagedRaidObjective : uint8_t {
    HARASS_RIVAL_RACKET      = 0, // Clear rival gang territory for paying family
    DIVERT_POLICE_RESPONSE   = 1, // False 10-7 look-away distraction
    THEATRICAL_ARREST_FALLGUY= 2  // Arrest designated low-level scapegoat with rapid bail
};

enum class IABStingStage : uint8_t {
    SURVEILLANCE_ACTIVE      = 0, // Unmarked surveillance van wiretaps & photography
    MARKED_CURRENCY_DELIVERED= 1, // Undercover agent hands over serialized bills
    AUDIO_TAPE_INTERCEPTED   = 2, // Crooked officer caught on wire pocketing bribe
    WARRANT_ISSUED           = 3, // Grand jury indictment signed
    PRE_DAWN_PRECINCT_RAID   = 4, // Federal/tactical raid arresting corrupt brass
    REFORM_COMPLETED         = 5  // Department cleaned, clean captain appointed
};

// ============================================================================
// Data Structures
// ============================================================================

struct CorruptOfficerProfile {
    uint32 officerId{0};
    uint32 entityGoId{0};
    std::string badgeNumber;
    std::string name;
    uint32 precinctId{13};
    CorruptionTier tier{CorruptionTier::COMPROMISED_ROOKIE};
    float corruptionScore{65.0f}; // 0.0 to 100.0
    float suspicionMeter{10.0f};  // 0.0 to 100.0 (Attention from IAB & Castle)
    uint32 patronSyndicateId{0}; // 1=Yakuza, 2=Bratva, 3=Triads, 4=Cartel, 5=Irish
    uint32 patronFamilyId{0};    // 1=Valetti, 2=Marcone, 3=Moretti, 4=Leone, 5=Falcone
    uint32 totalBribesPocketed{0};
    uint32 evidenceTamperedCount{0};
    bool carriesDropGun{false};
    std::string dropGunModel{"Scrubbed Revolver .38"};
    WhistleblowerStatus whistleblowerState{WhistleblowerStatus::NONE};
    bool isMarkedByCastle{false};
    bool isAgentPossessed{false};
    bool isAlive{true};
    bool isArrestedByIAB{false};
};

struct BribeDropSchedule {
    uint32 dropId{0};
    uint32 precinctId{13};
    uint32 patronId{1};
    bool isMafia{true};
    std::string dropLocationName{"Slums Meatpacking Backlot"};
    LocationVector location;
    uint32 cashAmount{35000};
    uint32 bagmanOfficerId{0};
    BribeDropState state{BribeDropState::SCHEDULED};
    uint32 captainShare{0};
    uint32 detectiveShare{0};
    uint32 dispatchShare{0};
    uint32 patrolPoolShare{0};
    std::string notes;
};

struct PrecinctCorruptionState {
    uint32 precinctId{13};
    std::string precinctName{"Precinct 13 - Slums"};
    uint32 districtId{1};
    float corruptionScore{82.5f}; // 0.0 to 100.0
    uint32 totalOfficers{48};
    uint32 corruptOfficers{38};
    uint32 whistleblowerCount{3};
    uint32 weeklyGraftTotal{0};
    uint32 activeProtectionContracts{0};
    bool isUnderLookAwayProtocol{false};
    uint32 lookAwayExpireMs{0};
    std::string lookAwayTargetDesc;
    bool isUnderActiveIABSting{false};
    uint32 activeStingId{0};
    bool isTargetedByCastle{false};
};

struct EvidenceSkimRecord {
    uint32 recordId{0};
    uint32 precinctId{13};
    uint32 officerId{0};
    ContrabandCategory category{ContrabandCategory::SEIZED_FIREARMS};
    std::string description{"12x Seized Calico 9mm Machine Pistols"};
    uint32 totalSeized{12};
    uint32 loggedQuantity{6};
    uint32 skimmedQuantity{6};
    float estimatedValue{18000.0f};
    bool isFenced{false};
    uint32 fencedToSyndicateId{0};
    uint32 fencedProfit{0};
};

struct StagedRaidRecord {
    uint32 raidId{0};
    uint32 payingFamilyId{1};
    uint32 targetSyndicateId{2};
    uint32 targetRacketId{5};
    StagedRaidObjective objective{StagedRaidObjective::HARASS_RIVAL_RACKET};
    LocationVector raidLocation;
    std::string squadCallsign{"Sierra-2"};
    bool isExecuted{false};
    uint32 fallGuyGoId{0};
    bool fallGuyBailedOut{false};
};

struct WhistleblowerRecord {
    uint32 officerId{0};
    uint32 evidencePagesCollected{0};
    float suspicionAgainstOfficer{15.0f}; // 0.0 to 100.0
    WhistleblowerStatus status{WhistleblowerStatus::COLLECTING_EVIDENCE};
    bool soloHotCallAssigned{false};
    LocationVector soloCallLocation;
    bool survivedSoloCall{false};
    bool iabProtectionActive{false};
};

struct IABStingOperation {
    uint32 stingId{0};
    uint32 precinctId{13};
    uint32 targetOfficerId{0};
    IABStingStage stage{IABStingStage::SURVEILLANCE_ACTIVE};
    uint32 markedBribeCash{50000};
    bool serialNumbersLogged{true};
    bool wiretapAudioSecured{false};
    bool indictmentSigned{false};
    bool raidSuccessful{false};
    std::string leadInvestigator{"Special Agent Marcus Kelly"};
};

struct CastleTaintedShieldEntry {
    uint32 officerId{0};
    std::string badgeNumber;
    std::string name;
    std::string crimeCommitted;
    bool isInterrogated{false};
    bool isConfessionTaped{false};
    bool isExecuted{false};
    bool tapeDeliveredToIAB{false};
};

// ============================================================================
// Corrupt Cop Manager Singleton
// ============================================================================

class CorruptCopManager : public Singleton<CorruptCopManager> {
public:
    CorruptCopManager();
    ~CorruptCopManager();

    void Initialize();
    void Reset();
    void Update(uint32 deltaMs);

    // Pillar I: Precinct Ledgers & Corruption Dynamics
    void RegisterPrecinct(uint32 precinctId, const std::string& name, uint32 districtId,
                          float initialScore, uint32 totalOfficers);
    PrecinctCorruptionState* GetPrecinct(uint32 precinctId);
    const std::map<uint32, PrecinctCorruptionState>& GetAllPrecincts() const { return m_precincts; }
    void UpdatePrecinctCorruption(uint32 precinctId, float deltaScore);

    void RegisterOfficer(uint32 officerId, const std::string& badge, const std::string& name,
                         uint32 precinctId, CorruptionTier tier, float corruptionScore);
    CorruptOfficerProfile* GetOfficer(uint32 officerId);
    std::vector<CorruptOfficerProfile*> GetOfficersInPrecinct(uint32 precinctId);
    size_t GetTotalOfficersCount() const;

    // Pillar II: Bagman Ledger & Bribe Drops
    uint32 ScheduleBribeDrop(uint32 precinctId, uint32 patronId, bool isMafia,
                             const std::string& locName, const LocationVector& loc,
                             uint32 cashAmount, uint32 bagmanOfficerId);
    BribeDropSchedule* GetBribeDrop(uint32 dropId);
    bool ExecuteBribeDelivery(uint32 dropId, uint32& outCaptainShare, uint32& outDetectiveShare,
                             uint32& outDispatchShare, uint32& outPatrolShare);
    uint32 GetTotalBribesLaundered() const { return m_totalBribesLaundered; }
    size_t GetBribeDropCount() const;

    // Pillar III: Evidence Locker Skimming & Drop Guns
    uint32 RecordEvidenceSeizure(uint32 precinctId, uint32 officerId, ContrabandCategory cat,
                                 const std::string& desc, uint32 seizedQty, float estimatedValue);
    EvidenceSkimRecord* GetEvidenceRecord(uint32 recordId);
    bool FenceContrabandToUnderworld(uint32 recordId, uint32 buyerSyndicateId, uint32& outCashGained);
    bool EquipThrowawayDropGun(uint32 officerId, const std::string& serialScrubbedGun = "Scrubbed Colt .38 Special");
    bool PlantDropGunOnVictim(uint32 officerId, uint32 victimGoId, const LocationVector& scenePos);
    size_t GetTotalEvidenceRecords() const { return m_evidenceRecords.size(); }

    // Pillar IV: Look-Away Protocols & Staged Raids
    bool IssueCode10_7LookAway(uint32 precinctId, uint32 requestedByFamilyId,
                               const std::string& targetCrimeScene, uint32 diversionDurationSec = 480);
    bool IsAreaUnderLookAwayProtocol(uint32 precinctId, const LocationVector& loc) const;
    uint32 StageRivalRacketRaid(uint32 payingFamilyId, uint32 targetSyndicateId,
                                uint32 targetRacketId, const LocationVector& racketPos, uint32 fallGuyGoId);
    StagedRaidRecord* GetStagedRaid(uint32 raidId);
    bool ProcessCatchAndRelease(uint32 arrestedMobsterGoId, uint32 payingFamilyId, uint32 bailAmountInfo);

    // Pillar V: Whistleblower Dynamics & Internal Affairs Stings
    bool PromoteWhistleblower(uint32 officerId);
    WhistleblowerRecord* GetWhistleblower(uint32 officerId);
    bool GatherWhistleblowerEvidence(uint32 officerId, const std::string& evidenceType, float suspicionGain = 12.0f);
    bool OrderRetaliationSoloCall(uint32 whistleblowerOfficerId, const LocationVector& dangerousLoc);
    bool ResolveSoloCallOutcome(uint32 whistleblowerOfficerId, bool castleOrPlayerIntervened);

    uint32 LaunchIABMarkedCurrencySting(uint32 targetOfficerId, uint32 stingCashAmount = 50000);
    IABStingOperation* GetIABSting(uint32 stingId);
    bool AdvanceIABStingStage(uint32 stingId, bool& outConvictionSecured);

    // Pillar VI: Frank Castle Retribution & Agent Host Purges
    bool LogCastleTaintedShieldTarget(uint32 officerId, const std::string& reason);
    CastleTaintedShieldEntry* GetCastleTarget(uint32 officerId);
    bool ExecuteCastleAmbushOnBribeDrop(uint32 dropId, bool executeOfficer);
    bool DeliverCastleConfessionTapeToIAB(uint32 officerId);

    float EvaluateAgentPossessionSusceptibility(uint32 officerId) const;
    bool TriggerAgentPossessionOfCorruptCop(uint32 officerId, uint32 agentType = 1);

    // Reporting & Diagnostics
    std::string GenerateCorruptionSummaryReport() const;

private:
    void InitializeDefaultPrecincts();
    void InitializeDefaultOfficers();

    mutable std::recursive_mutex m_mutex;
    std::map<uint32, PrecinctCorruptionState> m_precincts;
    std::map<uint32, CorruptOfficerProfile> m_officers;
    std::map<uint32, BribeDropSchedule> m_bribeDrops;
    std::map<uint32, EvidenceSkimRecord> m_evidenceRecords;
    std::map<uint32, StagedRaidRecord> m_stagedRaids;
    std::map<uint32, WhistleblowerRecord> m_whistleblowers;
    std::map<uint32, IABStingOperation> m_iabStings;
    std::map<uint32, CastleTaintedShieldEntry> m_castleTargets;

    uint32 m_nextDropId{1};
    uint32 m_nextRecordId{1};
    uint32 m_nextRaidId{1};
    uint32 m_nextStingId{1};
    uint32 m_totalBribesLaundered{0};
    uint32 m_totalStagedRaidsExecuted{0};
    uint32 m_totalWhistleblowersSilenced{0};
    uint32 m_totalWhistleblowersRescued{0};
    uint32 m_totalIABConvictions{0};
    uint32 m_totalCastleDirtyCopKills{0};
};

#define sCorruptCopMgr CorruptCopManager::getSingleton()

void RunCorruptCopTestSuite();

#endif // MXOEMU_CORRUPT_COP_MANAGER_H

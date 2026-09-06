#ifndef MXOEMU_NPC_FAMILY_DREAMS_ENGINE_H
#define MXOEMU_NPC_FAMILY_DREAMS_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <sstream>

// ============================================================================
// Kinship & Household Enums
// ============================================================================

enum class KinshipRole : uint8_t {
    HeadOfHousehold         = 0, // Patriarch, Matriarch, or Household Lead
    Spouse                  = 1, // Married or committed domestic partner
    Child                   = 2, // Offspring / dependent
    Parent                  = 3, // Elderly father or mother in residence
    Sibling                 = 4, // Brother or sister sharing apartment
    FoundFamilyCrewmate     = 5, // Zion hovercraft crew living under Captain
    InLaw                   = 6  // Extended relation
};

// ============================================================================
// Life Dreams & Aspirations Enums
// ============================================================================

enum class LifeDreamCategory : uint8_t {
    CIVILIAN_PROSPERITY     = 0, // Wealth, luxury home, comfort
    PROFESSIONAL_MASTERY    = 1, // Executive rank, master craft, acclaim
    DOMESTIC_DEVOTION       = 2, // Raising family, marital harmony
    REDPILL_LIBERATION      = 3, // Waking minds, defeating machines, crew loyalty
    SYSTEMIC_EQUILIBRIUM    = 4, // Machine code purity, zero anomaly
    EXILE_LUXURY            = 5, // French cuisine, Merovingian favor
    UNDERWORLD_DOMINANCE    = 6, // Rackets, turf boss, surviving vigilantes
    JUSTICE_AND_DUTY        = 7  // Law enforcement, clearing crime, pension
};

enum class LifeDreamType : uint8_t {
    // Civilian Dreams
    BuyRichlandHighRise     = 0, // Accumulate 5,000 credits to purchase penthouse
    BecomeMetacortexVP      = 1, // Climb corporate ladder to Executive Suite
    RaiseFlourishingFamily  = 2, // Build loving home with thriving children
    OpenArtisanBakery       = 3, // Launch independent cafe & boutique
    ClearHouseholdDebt      = 4, // Escape predatory corporate loans
    // Redpill Dreams
    LiberatePodSibling      = 5, // Awaken biological sibling from Power Plants
    CaptainHovercraft       = 6, // Command a Nebuchadnezzar-class vessel
    MasterHyperJump         = 7, // Unlock impossible RSI acrobatics
    // Machine & Agent Dreams
    SystemicZeroDefect      = 8, // Cleanse sector of all anomalous intrusions
    StudyHumanLove          = 9, // Understand sacrificial affection without crashing
    // Exile Dreams
    GainMerovingianFavor    = 10, // Deliver source code fragment to Club Hel
    // Syndicate Dreams
    RiseToUnderboss         = 11, // Seize control of Slums rackets
    SurviveFrankCastle      = 12, // Evade Punisher's crusade and live in luxury
    // Police Dreams
    MakeDetectiveLieutenant = 13, // Solve 10 major felony cases in precinct
    BustHarborCartel        = 14  // Expose corrupt IA captains and seize contraband
};

// ============================================================================
// Career Tracks & Job Levels
// ============================================================================

enum class CareerTrack : uint8_t {
    CorporateTech           = 0, // Metacortex, Omnicorp programming & finance
    IndustrialManufacturing = 1, // Docks, foundry, assembly lines
    MedicalHealthcare       = 2, // St. Jude Hospital, triage, EMT
    MunicipalGovernment     = 3, // City Hall, urban planning, records
    RetailCulinaryCommerce  = 4, // Noodle bars, artisan cafes, pharmacies
    NightlifeEntertainment  = 5, // Club Hel, Club Chateau host / performer
    HovercraftZionMilitary  = 6, // Zion defense force, APU pilot, operator
    SyndicateRacket         = 7, // Extortion, smuggling, loan sharking
    MMPDLawEnforcement      = 8  // Street beat patrol, detective, SWAT
};

enum class JobPositionLevel : uint8_t {
    Level0_EntryIntern      = 0, // Intern, apprentice, recruit, dock hand
    Level1_JuniorAssociate  = 1, // Junior analyst, machinist, beat officer
    Level2_MidLevelSpecialist=2, // Software engineer, technician, detective
    Level3_SeniorLead       = 3, // Senior architect, shop foreman, tactical sergeant
    Level4_DirectorMaster   = 4, // Department director, chief surgeon, lieutenant
    Level5_ExecutiveBoss    = 5  // VP, Don, Precinct Commander, Ship Captain
};

// ============================================================================
// Kinship & Household Data Structures
// ============================================================================

struct KinshipMember {
    uint32 entityId{0};
    std::string name;
    KinshipRole role{KinshipRole::Child};
    uint32 age{25};
    float affectionToHead{80.0f};      // 0.0 to 100.0
    uint32 dailyWageContribution{0};
};

struct FamilyHousehold {
    uint32 householdId{0};
    std::string householdName;
    std::string homeApartmentName;
    LocationVector homeLocation;
    uint32 headEntityId{0};
    std::vector<KinshipMember> members;
    
    uint32 householdSavingsInfoCredits{1200};
    uint32 pantryStock{40};            // Meals available for household
    float familyHappiness{0.8f};       // 0.0 (miserable/grieving) to 1.0 (ecstatic)
    
    bool isUnderGrief{false};
    std::string griefVictimName;
    std::string swornVengeanceTarget;

    const KinshipMember* FindMember(uint32 id) const {
        for (const auto& m : members) {
            if (m.entityId == id) return &m;
        }
        return nullptr;
    }

    KinshipMember* FindMemberMut(uint32 id) {
        for (auto& m : members) {
            if (m.entityId == id) return &m;
        }
        return nullptr;
    }
};

// ============================================================================
// Life Dreams & Aspiration Data Structures
// ============================================================================

struct ActiveAspiration {
    uint32 aspirationId{0};
    LifeDreamType dreamType{LifeDreamType::RaiseFlourishingFamily};
    std::string title;
    std::string description;
    float progress{0.0f};              // 0.0 to 1.0
    
    std::string currentMilestone;
    std::vector<std::string> milestonesCompleted;
    
    bool isFulfilled{false};
    bool isInCrisis{false};
    std::string fulfillmentNarrative;
};

// ============================================================================
// Career & Vocation Data Structures
// ============================================================================

struct CareerRecord {
    CareerTrack track{CareerTrack::CorporateTech};
    JobPositionLevel level{JobPositionLevel::Level0_EntryIntern};
    std::string jobTitle{"Junior Analyst"};
    std::string workplaceName{"Metacortex Core"};
    uint32 workplaceId{1};
    
    uint32 hourlyWage{45};
    float performanceScore{0.5f};      // 0.0 (fired) to 1.0 (promotion ready)
    float burnoutIndex{0.1f};          // 0.0 (energized) to 1.0 (exhausted)
    uint32 dailyTasksCompleted{0};
    uint32 totalPromotions{0};
    
    bool isOvertime{false};
    bool isUnemployed{false};
};

// ============================================================================
// NPC Family & Dreams Engine (Master Singleton)
// ============================================================================

class NPCFamilyDreamsEngine : public Singleton<NPCFamilyDreamsEngine> {
public:
    NPCFamilyDreamsEngine();
    ~NPCFamilyDreamsEngine();

    void Initialize();
    bool IsInitialized() const { return m_initialized; }
    void Reset(); // For tests and re-initialization

    // Household Management
    uint32 CreateHousehold(uint32 headId, const std::string& headName, const std::string& householdName,
                           const std::string& homeApartment, LocationVector loc);
    bool AddKinshipMember(uint32 householdId, uint32 memberId, const std::string& name, KinshipRole role, uint32 age);
    bool FormHouseholdFromRomance(uint32 partnerA, uint32 partnerB, const std::string& nameA, const std::string& nameB,
                                  const std::string& homeAddress, LocationVector loc);
    
    FamilyHousehold* GetHousehold(uint32 householdId);
    const FamilyHousehold* GetHousehold(uint32 householdId) const;
    FamilyHousehold* GetHouseholdByMember(uint32 entityId);
    const FamilyHousehold* GetHouseholdByMember(uint32 entityId) const;

    // Household Circadian Dynamics
    bool ProcessHouseholdMorningBreakfast(uint32 householdId);
    bool ProcessHouseholdEveningDinner(uint32 householdId);
    bool DepositHouseholdSavings(uint32 householdId, uint32 memberId, uint32 amount);
    bool TriggerFamilyGriefOrVengeance(uint32 victimId, const std::string& assailantName, const std::string& incidentDetails);

    // Life Dreams & Aspirations
    void AssignDream(uint32 entityId, LifeDreamType dreamType, const std::string& customTitle = "");
    ActiveAspiration* GetAspiration(uint32 entityId);
    const ActiveAspiration* GetAspiration(uint32 entityId) const;
    bool AdvanceAspirationMilestone(uint32 entityId, float progressDelta, const std::string& milestoneName);
    bool FulfillAspiration(uint32 entityId);
    bool TriggerAspirationCrisis(uint32 entityId, const std::string& crisisReason);

    // Career Ladders & Workplace Performance
    void AssignCareer(uint32 entityId, CareerTrack track, JobPositionLevel level, const std::string& title,
                      const std::string& workplace, uint32 wpId, uint32 wage);
    CareerRecord* GetCareer(uint32 entityId);
    const CareerRecord* GetCareer(uint32 entityId) const;
    bool AdvanceWorkShiftPerformance(uint32 entityId, float performanceDelta, bool overtime = false);
    bool PromoteCitizenCareer(uint32 entityId);
    bool DemoteOrTerminateCitizen(uint32 entityId, const std::string& reason);
    void MitigateBurnout(uint32 entityId, float amount);

    // Formatting & Summaries
    std::string GenerateFamilySummary(uint32 entityId) const;
    std::string GenerateDreamsSummary(uint32 entityId) const;
    std::string GenerateCareerSummary(uint32 entityId) const;
    std::string GenerateCompleteLifeDossier(uint32 entityId) const;
    std::string GenerateEngineMasterTelemetryReport() const;

    // Metrics
    size_t GetTotalHouseholdsCount() const;
    size_t GetTotalAspirationsCount() const;
    size_t GetTotalCareersCount() const;
    size_t GetFulfilledDreamsCount() const;

    // Static Utility Helpers
    static std::string GetKinshipRoleName(KinshipRole role);
    static std::string GetDreamCategoryName(LifeDreamCategory cat);
    static std::string GetCareerTrackName(CareerTrack track);
    static std::string GetJobLevelName(JobPositionLevel lvl);

private:
    bool m_initialized{false};
    mutable std::mutex m_familyMutex;
    uint32 m_nextHouseholdId{101};
    uint32 m_nextAspirationId{5001};

    // Households: householdId -> FamilyHousehold
    std::unordered_map<uint32, FamilyHousehold> m_households;
    // Fast lookup: entityId -> householdId
    std::unordered_map<uint32, uint32> m_entityToHousehold;
    // Aspirations: entityId -> ActiveAspiration
    std::unordered_map<uint32, ActiveAspiration> m_aspirations;
    // Careers: entityId -> CareerRecord
    std::unordered_map<uint32, CareerRecord> m_careers;
};

#define sFamilyDreamsEngine NPCFamilyDreamsEngine::getSingleton()
#define sNPCFamilyDreamsEngine NPCFamilyDreamsEngine::getSingleton()

// Standalone C++ Automated Test Suite
void RunNPCFamilyDreamsTestSuite();

#endif // MXOEMU_NPC_FAMILY_DREAMS_ENGINE_H

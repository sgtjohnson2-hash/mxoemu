#ifndef MXOEMU_MAFIA_ECOSYSTEM_MANAGER_H
#define MXOEMU_MAFIA_ECOSYSTEM_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "UnderworldManager.h"
#include "EmergentPoliceManager.h"
#include "CityLifeManager.h"

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
// Megacity Mafia Ecosystem Enums & Constants
// ============================================================================

enum class MafiaFamilyId : uint8_t {
    MarconeFamily       = 0, // The Concrete Kings: Docks, Sanitation, Old Guard
    ValentiFamily       = 1, // The Velvet Siphon: Downtown Nightlife, Casinos, Loans
    ScarlottiSyndicate  = 2, // The Iron Arsenal: Midtown Rail Yards, Arms Smuggling
    ChenWuTriad         = 3, // The Golden Dragon Tong: Chinatown, Code Opium
    PetrovBratva        = 4  // The Northern Vor: Slums Sump, Cyber-Extortion
};

enum class MobRank : uint8_t {
    Associate           = 0, // Street earner, runner, lookout, uninducted
    SoldierMadeMan      = 1, // Inducted via blood oath; protected by Omerta
    Caporegime          = 2, // Crew captain, manages district rackets & soldiers
    Consigliere         = 3, // Legal/political counselor and mediator
    Underboss           = 4, // Street operational director and logistics boss
    DonGodfather        = 5  // Supreme family ruler; Commission seat
};

enum class PizzoState : uint8_t {
    Unmarked            = 0, // Independent commercial business
    SoftShakedown       = 1, // Mob associates offered "protection insurance"
    ActivePizzo         = 2, // Regularly paying weekly protection envelope
    DefaultArson        = 3, // Failed payment; store vandalized/torched
    BustOutTakeover     = 4  // Mob seized business equity for bankruptcy fraud
};

enum class FrontBusinessType : uint8_t {
    WasteManagement     = 0, // Marcone Sanitation & Drywall
    NightclubLounge     = 1, // Valenti Gilded Cage & Le Vrai
    RailLogistics       = 2, // Scarlotti Freight & Scrap Salvage
    HerbalApothecary    = 3, // Chen-Wu Golden Lotus & Tea Wholesalers
    CommercialLaundry   = 4  // Petrov Tsarina 24-Hour Laundromat
};

enum class CommissionVoteStatus : uint8_t {
    Pending             = 0,
    Approved            = 1,
    Vetoed              = 2,
    EmergencyTruce      = 3
};

enum class WhackingStatus : uint8_t {
    None                = 0,
    Sanctioned          = 1,
    HitContractIssued   = 2,
    AmbushUnderway      = 3,
    ExecutedCleaned     = 4,
    FailedEscaped       = 5
};

// ============================================================================
// Core Mafia Data Structures
// ============================================================================

struct MadeSoldier {
    uint32 entityId{0};
    std::string name;
    std::string moniker; // e.g., "Jimmy Two-Times", "Sal the Rat-Trap"
    MobRank rank{MobRank::SoldierMadeMan};
    MafiaFamilyId familyId{MafiaFamilyId::MarconeFamily};
    uint32 crewId{0};
    
    float omertaLoyalty{90.0f};  // 0.0 (flipped rat) to 100.0 (unshakable)
    float paranoia{10.0f};       // 0.0 to 100.0
    bool isFlippedInformant{false};
    bool isAlive{true};
    uint32 hitsCarriedOut{0};
    LocationVector currentLocation;
    uint32 botGoId{0};
};

struct CapoCrew {
    uint32 crewId{0};
    uint32 capoEntityId{0};
    std::string crewName;
    MafiaFamilyId familyId{MafiaFamilyId::MarconeFamily};
    uint32 districtId{1};
    std::string districtName{"Slums"};
    
    std::vector<uint32> memberEntityIds;
    std::vector<uint32> extortedShopIds;
    
    double weeklyTributeOwed{2500.0};
    double weeklyTributeDelivered{0.0};
    float crewHeat{10.0f}; // 0.0 to 100.0
    LocationVector safehouseLocation;
};

struct FrontBusiness {
    uint32 businessId{0};
    std::string businessName;
    FrontBusinessType type{FrontBusinessType::WasteManagement};
    MafiaFamilyId familyId{MafiaFamilyId::MarconeFamily};
    uint32 districtId{1};
    LocationVector location;
    
    double dirtyBitsInQueue{0.0};
    double cleanBitsLaunderedTotal{0.0};
    float launderingEfficiency{0.75f}; // 75% clean conversion rate
    uint32 totalWashCycles{0};
};

struct PizzoExtortionTarget {
    uint32 shopId{0};
    std::string shopName;
    uint32 districtId{1};
    uint32 crewId{0};
    MafiaFamilyId controllingFamily{MafiaFamilyId::MarconeFamily};
    PizzoState state{PizzoState::Unmarked};
    
    double weeklyFeeBits{400.0};
    double totalBitsExtorted{0.0};
    uint32 missedPaymentsCount{0};
    float merchantTerrorIndex{0.0f}; // 0.0 to 1.0
    bool isArsonBurned{false};
};

struct CommissionSummit {
    uint32 summitId{0};
    std::string topic;
    CommissionVoteStatus status{CommissionVoteStatus::Pending};
    MafiaFamilyId initiatingFamily{MafiaFamilyId::MarconeFamily};
    std::unordered_map<uint8_t, bool> familyVotes; // MafiaFamilyId -> bool (aye/nay)
    std::string outcomeResolution;
    uint32 targetEntityId{0};
};

struct CorruptOfficial {
    uint32 officialId{0};
    std::string name;
    std::string roleTitle; // "Beat Officer", "Detective Lieutenant", "Precinct Captain", "Municipal Judge"
    MafiaFamilyId owningFamily{MafiaFamilyId::MarconeFamily};
    double weeklyBribeCost{500.0};
    float corruptionLoyalty{80.0f};
    bool isExposedByInternalAffairs{false};
};

struct HitContract {
    uint32 hitId{0};
    uint32 targetEntityId{0};
    std::string targetName;
    MafiaFamilyId orderingFamily{MafiaFamilyId::MarconeFamily};
    WhackingStatus status{WhackingStatus::None};
    std::string justification;
    bool isSanctionedByCommission{false};
    uint32 assignedHitmanId{0};
    bool wasBodyCleanedByExiles{false};
};

struct MafiaFamily {
    MafiaFamilyId familyId{MafiaFamilyId::MarconeFamily};
    std::string familyName;
    std::string epithetTitle; // "The Concrete Kings", "The Velvet Siphon"
    std::string territoryDescription;
    
    uint32 donEntityId{0};
    std::string donName;
    uint32 underbossEntityId{0};
    std::string underbossName;
    uint32 consigliereEntityId{0};
    std::string consigliereName;
    
    std::vector<uint32> crewIds;
    std::vector<uint32> frontBusinessIds;
    
    double treasuryCleanBits{25000.0};
    double treasuryDirtyBits{10000.0};
    
    float familyHeat{20.0f}; // 0.0 to 100.0
    float ricoIndictmentMeter{0.05f}; // 0.0 to 1.0
    bool isUnderRICOIndictment{false};
    uint32 totalMadeMenCount{0};
    uint32 totalWhackingsExecuted{0};
};

// ============================================================================
// Megacity Mafia Ecosystem Manager (Master Singleton)
// ============================================================================

class MafiaEcosystemManager : public Singleton<MafiaEcosystemManager> {
public:
    MafiaEcosystemManager();
    ~MafiaEcosystemManager();

    void Initialize();
    bool IsInitialized() const { return m_initialized; }
    void Reset();
    void Update(uint32 deltaMs);

    // Family & Hierarchy Management
    const MafiaFamily* GetFamily(MafiaFamilyId id) const;
    MafiaFamily* GetFamilyMut(MafiaFamilyId id);
    const CapoCrew* GetCrew(uint32 crewId) const;
    CapoCrew* GetCrewMut(uint32 crewId);
    const MadeSoldier* GetSoldier(uint32 entityId) const;
    MadeSoldier* GetSoldierMut(uint32 entityId);

    bool InductMadeMan(uint32 entityId, const std::string& name, const std::string& moniker,
                       MafiaFamilyId familyId, uint32 crewId, MobRank rank = MobRank::SoldierMadeMan);
    bool PromoteMember(uint32 entityId, MobRank newRank);

    // Pizzo Extortion & Front Business Laundering
    bool RegisterShopExtortion(uint32 shopId, const std::string& shopName, uint32 districtId,
                               uint32 crewId, double weeklyFee = 400.0);
    const PizzoExtortionTarget* GetPizzoTarget(uint32 shopId) const;
    PizzoExtortionTarget* GetPizzoTargetMut(uint32 shopId);
    void ProcessWeeklyPizzoCollections();
    bool DefaultOnPizzo(uint32 shopId);
    bool ExecuteArsonShakedown(uint32 shopId);
    bool BustOutTakeover(uint32 shopId);

    bool RegisterFrontBusiness(uint32 id, const std::string& name, FrontBusinessType type,
                               MafiaFamilyId familyId, uint32 districtId, LocationVector loc, float efficiency = 0.75f);
    const FrontBusiness* GetFrontBusiness(uint32 id) const;
    double LaunderDirtyBits(MafiaFamilyId familyId, double dirtyAmount);

    // Omertà, Informants & Whacking Execution
    bool DecayOmertaLoyalty(uint32 entityId, float delta);
    bool FlipInformantToRICO(uint32 entityId);
    bool AuditFamilyForRats(MafiaFamilyId familyId, uint32& outSuspectId);
    uint32 IssueHitContract(MafiaFamilyId orderingFamily, uint32 targetEntityId,
                            const std::string& reason, bool requestCommissionSanction = true);
    bool ExecuteWhacking(uint32 hitId, uint32 hitmanId);
    bool DeployExileCleaners(uint32 hitId);
    const HitContract* GetHitContract(uint32 hitId) const;

    // La Commissione High Council Governance
    uint32 ConveneCommissionSummit(const std::string& topic, MafiaFamilyId initiatingFamily, uint32 targetEntityId = 0);
    bool CastCommissionVote(uint32 summitId, MafiaFamilyId votingFamily, bool approve);
    const CommissionSummit* GetCommissionSummit(uint32 summitId) const;
    void DeclareInterFamilyTruce();
    bool ContributeToBribePool(MafiaFamilyId familyId, double amount);
    double GetBribePoolBalance() const { return m_commonBribePool; }

    // Political & Judicial Corruption
    bool RegisterCorruptOfficial(uint32 id, const std::string& name, const std::string& role,
                                 MafiaFamilyId family, double weeklyCost);
    void AdvanceRICOInvestigation(MafiaFamilyId family, float heatDelta);
    bool MitigateRICOWithBribe(MafiaFamilyId family, double cost);
    bool TriggerRICOSwatRaid(MafiaFamilyId family);

    // Adversarial Interlocks
    bool TriggerCastleSafehouseIncursion(uint32 crewId, const std::string& assaultDetails);
    bool TriggerAgentAnomalyIntervention(uint32 districtId);

    // Telemetry & Reporting
    std::string GenerateMafiaWorldReport() const;
    std::string GenerateFamilyDossier(MafiaFamilyId id) const;
    std::string GenerateCommissionReport() const;
    std::string GeneratePizzoExtortionReport() const;

    // Metrics
    size_t GetTotalMadeMenCount() const;
    size_t GetTotalCrewsCount() const { return m_crews.size(); }
    size_t GetTotalExtortedShopsCount() const { return m_pizzoTargets.size(); }
    size_t GetTotalFrontBusinessesCount() const { return m_frontBusinesses.size(); }
    size_t GetTotalHitContractsCount() const { return m_hitContracts.size(); }

    // Static String Utility Helpers
    static std::string GetFamilyName(MafiaFamilyId id);
    static std::string GetRankName(MobRank rank);
    static std::string GetPizzoStateName(PizzoState state);
    static std::string GetFrontTypeName(FrontBusinessType type);

private:
    bool m_initialized{false};
    mutable std::mutex m_mafiaMutex;

    uint32 m_nextCrewId{101};
    uint32 m_nextBusinessId{501};
    uint32 m_nextSummitId{1001};
    uint32 m_nextHitId{7001};
    double m_commonBribePool{50000.0};

    // The Five Families
    std::unordered_map<uint8_t, MafiaFamily> m_families;
    // Crews: crewId -> CapoCrew
    std::unordered_map<uint32, CapoCrew> m_crews;
    // Made Soldiers & Associates: entityId -> MadeSoldier
    std::unordered_map<uint32, MadeSoldier> m_soldiers;
    // Pizzo Extortion: shopId -> PizzoExtortionTarget
    std::unordered_map<uint32, PizzoExtortionTarget> m_pizzoTargets;
    // Front Businesses: businessId -> FrontBusiness
    std::unordered_map<uint32, FrontBusiness> m_frontBusinesses;
    // Commission Summits: summitId -> CommissionSummit
    std::unordered_map<uint32, CommissionSummit> m_summits;
    // Corrupt Officials: officialId -> CorruptOfficial
    std::unordered_map<uint32, CorruptOfficial> m_officials;
    // Hit Contracts: hitId -> HitContract
    std::unordered_map<uint32, HitContract> m_hitContracts;

    void InitializeTheFiveFamilies();
};

#define sMafiaMgr MafiaEcosystemManager::getSingleton()

// Standalone C++ Automated Test Suite Declaration
void RunMafiaEcosystemTestSuite();

#endif // MXOEMU_MAFIA_ECOSYSTEM_MANAGER_H

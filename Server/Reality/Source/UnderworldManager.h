#ifndef MXOEMU_UNDERWORLD_MANAGER_H
#define MXOEMU_UNDERWORLD_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <memory>

// ============================================================================
// Megacity Criminal Underworld Enums & Constants
// ============================================================================

enum class SyndicateFaction : uint8_t {
    WolfpackLupines     = 0, // Merovingian Lupine Outcasts (Westview Slums & Waterfront)
    ChateauSanguine     = 1, // Vampire Aristocracy & Le Vrai (Richland & Downtown Penthouses)
    JudasCabal          = 2, // Cypherite Turncoat Front (Financial Towers & Data Hubs)
    MarconeFamily       = 3, // Corrupt Human Mobs & Red Dragon (Docks, Chop Shops & Warehouses)
    ByteCartel          = 4  // Rogue Exile Glitch Brokers (Abandoned Subways & Sewers)
};

enum class RacketType : uint8_t {
    CodeStill           = 0, // Synthetic narcotics ("Static Dust", "Blue Bliss")
    WeaponsDepot        = 1, // Illicit AP munitions, explosives, heavy weapons
    NightclubFront      = 2, // VIP blackmail parlors, memory theft, forged RSIs
    ChopShop            = 3, // Stolen convoy modifications, armored sedans
    ExtortionHub        = 4, // Protection racket, captive redpills, code pits
    BlackClinic         = 5  // Unregistered cyber-stims and illegal code modifications
};

enum class RacketState : uint8_t {
    Incubating          = 0, // Low-profile hideout establishing operations
    Active              = 1, // Pumping contraband, generating illicit revenue & heat
    Fortified           = 2, // Reinforced blast doors, automated defenses, elite guards
    UnderSiegeByCastle  = 3, // Frank Castle currently breaching the facility
    DecapitatedCooldown = 4  // Boss dead, cache destroyed, skull signature left, in cooldown
};

enum class ConvoyStatus : uint8_t {
    CONVOY_PREPARING    = 0,
    CONVOY_IN_TRANSIT   = 1,
    CONVOY_AMBUSHED     = 2,
    CONVOY_DESTROYED    = 3,
    CONVOY_DELIVERED    = 4
};

enum class EmergentCrimeType : uint8_t {
    StreetMugging        = 0, // Quick violent shakedown of civilians / redpills
    BankHeist            = 1, // High-stakes financial core breach with hostage negotiation
    HostageKidnapping    = 2, // Captive Zion operative held in fortified hideout
    DriveByShooting      = 3, // Rival gang retaliatory drive-by hit
    ContrabandDrop       = 4, // Illicit black market courier drop-off
    CyberdeckDataSiphon  = 5, // Tap into Megacity power / data grid
    IllegalArmsDeal      = 6  // Military AP ammunition exchange in back-alley
};

enum class EmergentCrimeState : uint8_t {
    Emerging             = 0, // Crime being planned / initial lookout spotted
    InProgress           = 1, // Crime underway, 911 dispatch alerted
    Escalated            = 2, // Shots fired, hostages threatened, heat rising rapidly
    NeutralizedByCastle  = 3, // Frank Castle intervened and wiped out perps
    NeutralizedByPolice  = 4, // MMPD officers / SWAT secured the scene
    CompletedEscaped     = 5  // Perpetrators fled with loot; district heat escalated
};

enum class PoliceUnitType : uint8_t {
    BeatPatrol           = 1, // 1 Star: 2 patrol cops, Taurus 9mm
    SquadCarCruiser      = 2, // 2 Stars: 2 cruiser cars, shotguns, roadblocks
    SWATBreachTeam       = 3, // 3 Stars: Tactical BearCat, assault rifles, flashbangs
    HeavyTacticalUnit    = 4, // 4 Stars: Heavy armor, sniper overwatch, air helicopter
    AgentIntervention    = 5  // 5 Stars: System Agents override law enforcement
};

// ============================================================================
// Core Underworld Data Structures
// ============================================================================

struct CartelLieutenant {
    uint32 id{0};
    std::string name;
    SyndicateFaction faction{SyndicateFaction::MarconeFamily};
    std::string factionName;
    std::string rankTitle; // e.g., "Capo", "Alpha Enforcer", "Blood Sire"
    std::vector<std::string> personalityTraits; // "Ironclad", "Pyromaniac", "Phase Glitcher", "Hostage Taker", "Cowardly Tactician", "Blood Frenzy", "Sniper Overwatch"
    uint32 bountyOnCastle{0};
    int32 grudgeScoreAgainstCastle{0};
    bool isAlive{true};
    LocationVector lastKnownLocation;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    uint32 combatPower{500};

    bool HasTrait(const std::string& trait) const {
        for (const auto& t : personalityTraits) {
            if (t == trait) return true;
        }
        return false;
    }
};

struct UnderworldRacketNode {
    uint32 id{0};
    std::string name;
    SyndicateFaction faction{SyndicateFaction::MarconeFamily};
    std::string factionName;
    RacketType type{RacketType::WeaponsDepot};
    std::string typeName;
    RacketState state{RacketState::Active};
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector coordinates;
    uint32 bossLieutenantId{0};
    std::string bossName;
    float defenseHealth{100.0f};
    float maxDefenseHealth{100.0f};
    float illicitRevenueRate{250.0f}; // Credits / contraband units per minute
    uint32 activeGoonsCount{4};
    int64_t lastAttackedTimestamp{0};
    uint32 cooldownTimerMs{0};
    uint32 cooldownDurationMs{180000}; // 3 minutes standard cooldown
    bool skullDecalMarked{false};
    uint32 totalDecapitations{0};
    uint32 securityTier{2}; // 1 = Low, 2 = Guarded, 3 = Fortified, 4 = High-Tech EMP
    bool tripwireTrapArmed{true};
    std::vector<uint32> guardBotGoIds;
    uint32 bossBotGoId{0};
};

struct SmugglingConvoy {
    uint32 convoyId{0};
    SyndicateFaction faction{SyndicateFaction::MarconeFamily};
    std::string factionName;
    std::string cargoDescription;
    uint32 startDistrictId{3};
    uint32 destDistrictId{2};
    LocationVector startLocation;
    LocationVector destinationLocation;
    LocationVector currentLocation;
    float routeProgress{0.0f}; // 0.0f to 1.0f
    uint32 escortVehicleCount{2};
    uint32 escortGuardsCount{6};
    uint32 cargoValuation{5000};
    ConvoyStatus status{ConvoyStatus::CONVOY_PREPARING};
    bool isInterceptedByCastle{false};
    uint32 transitTimerMs{0};
    uint32 transportBotGoId{0};
    std::vector<uint32> escortBotGoIds;
};

struct TurfSector {
    uint32 sectorId{0};
    std::string name;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector centerCoordinates;
    float radiusUnits{1500.0f};
    SyndicateFaction controllingFaction{SyndicateFaction::WolfpackLupines};
    float factionInfluence[5]{0.0f}; // 0: Wolfpack, 1: Chateau, 2: Judas, 3: Marcone, 4: ByteCartel
    bool isContested{false};
    SyndicateFaction attackingFaction{SyndicateFaction::MarconeFamily};
    uint32 turfWarTimerMs{0};
    uint32 totalSkirmishes{0};

    SyndicateFaction GetDominantFaction() const {
        int bestIdx = 0;
        float bestInf = -1.0f;
        for (int i = 0; i < 5; ++i) {
            if (factionInfluence[i] > bestInf) {
                bestInf = factionInfluence[i];
                bestIdx = i;
            }
        }
        return (SyndicateFaction)bestIdx;
    }
};

struct EmergentCrimeEvent {
    uint32 crimeId{0};
    EmergentCrimeType type{EmergentCrimeType::StreetMugging};
    std::string typeName{"Street Mugging"};
    EmergentCrimeState state{EmergentCrimeState::Emerging};
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector location;
    SyndicateFaction perpFaction{SyndicateFaction::WolfpackLupines};
    std::string perpFactionName;
    std::string perpDescription;
    uint32 perpCount{3};
    uint32 hostageCount{0};
    float lootValue{1500.0f};
    uint32 durationMs{0};
    uint32 timeLimitMs{45000}; // 45s window
    bool policeDispatched{false};
    bool castleIntervening{false};
    uint32 linkedDispatchId{0};
    std::vector<uint32> perpBotGoIds;
    uint32 victimBotGoId{0};
};

struct PolicePrecinct {
    uint32 precinctId{0};
    std::string name;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector precinctLocation;
    float corruptionIndex{20.0f}; // 0.0 (Clean) to 100.0 (Fully Mob Owned)
    SyndicateFaction bribingFaction{SyndicateFaction::MarconeFamily};
    std::string precinctCaptainName{"Captain Vance"};
    bool isExposedByCastle{false};
    uint32 activePatrolUnits{8};
    uint32 maxPatrolUnits{8};
    uint32 holdingCellInmates{8};
    float confiscatedContrabandValue{35000.0f};
    uint32 cleanLeadershipTimerMs{0}; // Vigilance window after corruption purge
};

struct PoliceDispatchCall {
    uint32 dispatchId{0};
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector sceneLocation;
    uint32 wantedLevelStars{1}; // 1 to 5
    PoliceUnitType respondingUnitType{PoliceUnitType::BeatPatrol};
    std::string callsign{"Unit 1-Adam-12"};
    std::string tenCode{"10-31"};
    std::string dispatchChatter;
    bool isArrived{false};
    bool isCompromisedByCorruption{false};
    uint32 responseTimerMs{0};
    uint32 linkedCrimeId{0};
    uint32 respondingPrecinctId{0};
    bool isMutualAid{false};
};

// ============================================================================
// Underworld Manager Singleton
// ============================================================================

class UnderworldManager : public Singleton<UnderworldManager> {
public:
    friend void RunUnderworldTestSuite();

    UnderworldManager();
    ~UnderworldManager();

    void Initialize();
    void Update(uint32 deltaMs);

    // Racket Operations
    const std::map<uint32, UnderworldRacketNode>& GetAllRackets() const { return m_rackets; }
    UnderworldRacketNode* GetRacket(uint32 racketId);
    std::vector<UnderworldRacketNode*> GetRacketsInDistrict(uint32 districtId);
    std::vector<UnderworldRacketNode*> GetActiveRackets();
    UnderworldRacketNode* GetNearestActiveRacket(float x, float z, uint32 districtId = 0);
    bool RaidRacket(uint32 racketId, bool byCastle = true);
    bool DamageRacketDefenses(uint32 racketId, float damage, bool byCastle = true);
    bool DecapitateRacket(uint32 racketId, bool byCastle = true);
    void ClearAllCooldowns();

    // Smuggling Convoys
    uint32 SpawnConvoy(SyndicateFaction faction, uint32 startDistrictId, uint32 destDistrictId);
    bool InterceptConvoy(uint32 convoyId, bool byCastle = true);
    const std::map<uint32, SmugglingConvoy>& GetActiveConvoys() const { return m_convoys; }
    SmugglingConvoy* GetConvoy(uint32 convoyId);

    // Cartel Lieutenants & Nemesis AI
    const std::map<uint32, CartelLieutenant>& GetAllLieutenants() const { return m_lieutenants; }
    CartelLieutenant* GetLieutenant(uint32 lieutenantId);
    std::vector<CartelLieutenant*> GetLivingLieutenants();
    void RecordLieutenantDefeated(uint32 lieutenantId, bool byCastle = true);
    void EscalateRetaliationBounty(SyndicateFaction faction, uint32 amount);

    // Gang Turf & Territory Grid
    const std::map<uint32, TurfSector>& GetAllTurfSectors() const { return m_turfSectors; }
    TurfSector* GetTurfSector(uint32 sectorId);
    std::vector<TurfSector*> GetTurfSectorsInDistrict(uint32 districtId);
    bool TriggerTurfWar(uint32 sectorId, SyndicateFaction attackerFaction);
    bool ResolveTurfWar(uint32 sectorId, SyndicateFaction victorFaction);
    SyndicateFaction GetDominantFactionInDistrict(uint32 districtId) const;

    // Emergent Crimes Engine
    uint32 SpawnEmergentCrime(EmergentCrimeType type, uint32 districtId, LocationVector loc);
    const std::map<uint32, EmergentCrimeEvent>& GetAllActiveCrimes() const { return m_crimes; }
    EmergentCrimeEvent* GetEmergentCrime(uint32 crimeId);
    bool EscalateCrime(uint32 crimeId);
    bool NeutralizeCrime(uint32 crimeId, bool byCastle, bool byPolice = false);
    size_t GetActiveCrimeCount() const;

    // Police & Law Enforcement (MMPD)
    const std::map<uint32, PolicePrecinct>& GetAllPrecincts() const { return m_precincts; }
    PolicePrecinct* GetPrecinct(uint32 precinctId);
    PolicePrecinct* GetPrecinctInDistrict(uint32 districtId);
    uint32 DispatchPoliceResponse(uint32 districtId, uint32 wantedStars, LocationVector sceneLoc, const std::string& incidentDesc = "", uint32 crimeId = 0);
    const std::vector<PoliceDispatchCall>& GetRecentDispatches() const { return m_recentDispatches; }
    bool InvestigatePrecinctCorruption(uint32 precinctId, bool byCastle = true);
    bool ExposePrecinctCorruption(uint32 precinctId, bool byCastle = true);
    bool RestockPrecinctPatrols(uint32 precinctId);
    uint32 GetDistrictWantedLevel(uint32 districtId) const;
    void SetDistrictWantedLevel(uint32 districtId, uint32 stars);

    // Shard Heat & Megacity Tension (0.0 to 100.0)
    float GetDistrictHeat(uint32 districtId) const;
    void SetDistrictHeat(uint32 districtId, float heat);
    void AddDistrictHeat(uint32 districtId, float delta);
    std::string GetDistrictTensionName(uint32 districtId) const;
    bool IsThreeWayWarActive(uint32 districtId) const;
    bool IsFourWayWarActive(uint32 districtId) const;

    // Reporting & Persistence
    std::string GenerateUnderworldStatusReport() const;
    std::string GenerateBossHierarchyReport() const;
    std::string GenerateTurfGridReport() const;
    std::string GenerateEmergentCrimesReport() const;
    std::string GeneratePolicePrecinctsReport() const;
    bool SaveUnderworldStateToFile(const std::string& path = "UnderworldEcosystem.json");
    bool LoadUnderworldStateFromFile(const std::string& path = "UnderworldEcosystem.json");

    // Utilities
    static std::string GetFactionName(SyndicateFaction f);
    static std::string GetRacketTypeName(RacketType t);
    static std::string GetDistrictName(uint32 districtId);
    static std::string GetCrimeTypeName(EmergentCrimeType t);
    static std::string GetPoliceUnitName(PoliceUnitType t);

    // Testing & Simulation Ticks
    void UpdatePoliceDispatches(uint32 deltaMs);
    void UpdatePrecincts(uint32 deltaMs);

private:
    void InitializeDefaultRackets();
    void InitializeDefaultLieutenants();
    void InitializeDefaultTurfSectors();
    void InitializeDefaultPrecincts();

    void UpdateRackets(uint32 deltaMs);
    void UpdateConvoys(uint32 deltaMs);
    void UpdateShardHeat(uint32 deltaMs);
    void UpdateTurfWars(uint32 deltaMs);
    void UpdateEmergentCrimes(uint32 deltaMs);
    void SyncWithFrankCastleHitList();

    std::map<uint32, UnderworldRacketNode> m_rackets;
    std::map<uint32, CartelLieutenant> m_lieutenants;
    std::map<uint32, SmugglingConvoy> m_convoys;
    std::map<uint32, TurfSector> m_turfSectors;
    std::map<uint32, EmergentCrimeEvent> m_crimes;
    std::map<uint32, PolicePrecinct> m_precincts;
    std::vector<PoliceDispatchCall> m_recentDispatches;

    float m_districtHeat[6]; // Indices 1 to 5 for districts
    uint32 m_districtWantedStars[6]; // Wanted level stars 0-5
    bool m_threeWayWarTriggered[6];
    bool m_fourWayWarTriggered[6];

    uint32 m_nextConvoyId{1};
    uint32 m_nextLieutenantId{1};
    uint32 m_nextCrimeId{1};
    uint32 m_nextDispatchId{1};

    uint32 m_hitListSyncTimerMs{0};
    uint32 m_convoySpawnTimerMs{0};
    uint32 m_crimeSpawnTimerMs{0};
    uint32 m_turfSkirmishTimerMs{0};
    uint32 m_precinctReplenishTimerMs{0};
    uint32 m_inmateProcessingTimerMs{0};

    uint32 m_totalRacketsDecapitated{0};
    uint32 m_totalConvoysIntercepted{0};
    uint32 m_totalCrimesNeutralized{0};
    uint32 m_totalPrecinctsExposed{0};

    mutable std::recursive_mutex m_mutex;
};

#define sUnderworldMgr UnderworldManager::getSingleton()

// Test Suite Declaration
void RunUnderworldTestSuite();

#endif // MXOEMU_UNDERWORLD_MANAGER_H

#ifndef MXOEMU_EMERGENT_POLICE_MANAGER_H
#define MXOEMU_EMERGENT_POLICE_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "UnderworldManager.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <memory>
#include <functional>

// ============================================================================
// Emergent Police & Tactical SWAT Enums & Constants
// ============================================================================

enum class SWATRole : uint8_t {
    POINTMAN_BALLISTIC_SHIELD = 0, // Carries Level IV ballistic shield, leads entry, absorbs fire
    BREACHER_HEAVY_RAM        = 1, // Carries hydraulic ram / C4 breaching strips / shotgun
    ASSAULTER_ALPHA           = 2, // Primary CQB assaulter, pies left corner
    ASSAULTER_BRAVO           = 3, // Secondary CQB assaulter, pies right corner, covers dead angles
    TEAM_LEADER               = 4, // Tactical squad leader, gives detonation/entry orders, 10-code comms
    SNIPER_OVERWATCH          = 5, // High-ground marksman providing target locks and precision fire
    HOSTAGE_TRIAGE_MEDIC      = 6, // Performs field triage and patient stabilization
    REAR_SECURITY             = 7  // Covers flank and rear approaches during threshold hold
};

enum class BreachManeuverPhase : uint8_t {
    STAGING_AT_PERIMETER     = 0, // Squad assembled at perimeter vehicle, awaiting breach directive
    STACK_AT_THRESHOLD       = 1, // Line stack on wall beside doorway/threshold, weapons high-ready
    PREPARING_BREACH_DEVICE  = 2, // Affixing C4 linear charge / aligning hydraulic door ram
    DETONATION_RAM_STRIKE    = 3, // Breaching charge blown or door kicked, threshold concussed
    NON_LETHAL_SUPPRESSION   = 4, // Deploying M84 flashbang or CS tear gas into entry zone
    TACTICAL_ROOM_CLEAR      = 5, // Dynamic entry, pieing corners, neutralizing armed combatants
    CONTAINMENT_AND_RESTRAINT= 6, // Flex-cuffing suspects, kicking weapons away, clearing closets
    HOSTAGE_EXTRACTION_ESCORT= 7, // Evacuating hostages under ballistic shield corridor to CCP
    CODE_4_COMPLETED         = 8  // Structure cleared, scene secure, radio callout "Code 4"
};

enum class BallisticShieldFormation : uint8_t {
    STACK_LINE          = 0, // Single-file queue trailing pointman (narrow doors/hallways)
    TACTICAL_WEDGE      = 1, // V-shaped wedge for wide open floors (banks, warehouses)
    EXTRACTION_BOX      = 2, // 360-degree protective ring shielding hostages/VIPs
    STATIONARY_BARRICADE= 3  // Shield locked to ground, officers firing around edges
};

enum class OrdnanceType : uint8_t {
    ORDNANCE_FLASHBANG_M84 = 0, // Stuns & disorients in 14m radius (170dB / 1.5M candela)
    ORDNANCE_TEAR_GAS_CS   = 1, // Lingering chemical cloud (22m radius, 30s duration)
    ORDNANCE_BREACHING_C4  = 2, // High explosive charge for reinforced blast doors
    ORDNANCE_SMOKE_SCREEN  = 3, // Dense aerosol obscuring hostile line of sight
    ORDNANCE_STINGER_RUBBER= 4  // Less-lethal rubber pellet grenade for crowd suppression
};

enum class SniperOverwatchState : uint8_t {
    SEARCHING_VANTAGE         = 0, // Searching target sector through high-magnification optic
    TARGET_ACQUIRED_TRACKING  = 1, // Crosshairs locked onto high-value target / hostage taker
    LASER_DESIGNATED          = 2, // IR / visible laser designator painted on target's center mass
    WAITING_GREEN_LIGHT       = 3, // Holding trigger, awaiting tactical breach detonation or green light
    AUTHORIZED_LETHAL_FIRE    = 4, // Clearance granted, sniper executing precision shot
    TARGET_NEUTRALIZED        = 5, // Confirmed kill / target incapacitated, reporting overwatch clear
    RELOCATING                = 6  // Shifting to alternate perch
};

enum class HostageMedicalTriage : uint8_t {
    GREEN_WALKING_WOUNDED = 0, // Minor cuts, shock, able to walk under own power
    YELLOW_DELAYED_CARE   = 1, // Moderate contusions, requires escort support
    RED_IMMEDIATE_TRAUMA  = 2, // Gunshot trauma, requires emergency field dressing / stretcher
    BLACK_DECEASED        = 3  // Lethal casualty
};

enum class HostageRescueState : uint8_t {
    HELD_HOSTAGE             = 0, // Held at gunpoint / barricaded inside
    UNDER_LETHAL_THREAT      = 1, // Hostage taker aiming firearm directly at hostage
    DISORIENTED_BY_ORDNANCE  = 2, // Temporarily dazed by flashbang / tear gas
    SECURED_BY_OPERATORS     = 3, // Zip-tied / verified non-combatant by SWAT
    UNDER_EXTRACTION_ESCORT  = 4, // Moving in protective shield formation to CCP
    SAFELY_EVACUATED_TO_CCP  = 5, // Safely delivered to Casualty Collection Point / ambulance
    KILLED_BY_HOSTILE        = 6  // Executed by hostile prior to intervention
};

enum class RoadblockType : uint8_t {
    VEHICULAR_CRUISER_V_BLOCK = 0, // Two MMPD cruisers angled in V-formation with spotlight
    BEARCAT_HEAVY_BARRICADE   = 1, // Lenco BearCat armored vehicle with roof gunner cover
    ARTERIAL_SPIKE_CHOKEPOINT = 2, // Stinger spike strips deployed across 2-4 roadway lanes
    FULL_DISTRICT_LOCKDOWN    = 3  // Total bridge / freeway seal with cruisers and spike strips
};

enum class StingPhase : uint8_t {
    INACTIVE                       = 0,
    COVERT_SURVEILLANCE            = 1, // IA surveillance van deployed, wiretaps on radio & mob lines
    MARKED_CURRENCY_BRIBE          = 2, // Undercover IA agent delivers $50,000 in marked serialized bills
    AUDIO_INTERCEPT_CONFIRMATION   = 3, // Audio recording confirms crooked captain pocketing kickback
    PRECINCT_HQ_RAID               = 4, // IA tactical team storms precinct HQ, arrests corrupt command
    DEPARTMENTAL_PURGE_REFORM      = 5  // Clean interim commissioner installed, precinct reformed
};

enum class MMPD10Code : uint8_t {
    CODE_10_4   = 4,   // Acknowledged / Message received
    CODE_10_7   = 7,   // Out of service / End of shift
    CODE_10_8   = 8,   // In service / Unit available for call
    CODE_10_15  = 15,  // Suspect in custody / Inmate transport
    CODE_10_20  = 20,  // Location check / Status update
    CODE_10_23  = 23,  // Arrived on scene
    CODE_10_31  = 31,  // Crime in progress / In-progress alert
    CODE_10_33  = 33,  // Officer in emergency distress / Panic alarm
    CODE_10_50  = 50,  // Motor vehicle accident / Pursuit vehicle disabled by spikes
    CODE_10_70  = 70,  // Fire hazard / Chemical tear gas deployed
    CODE_10_71  = 71,  // Shots fired / Active gun battle
    CODE_10_78  = 78,  // Request Emergency Medical Services (EMS)
    CODE_10_80  = 80,  // High-speed pursuit in progress
    CODE_10_89  = 89,  // Explosive breach charge armed / Bomb squad
    CODE_10_90  = 90,  // Bank silent alarm triggered
    CODE_10_99  = 99,  // SWAT Code Red / Barricaded suspects with hostages
    CODE_10_100 = 100  // Internal Affairs corruption alert / Clandestine directive
};

// ============================================================================
// Tactical Data Structures
// ============================================================================

struct SWATOfficer {
    uint32 officerId{0};
    std::string badgeNumber;
    std::string officerName;
    SWATRole role{SWATRole::ASSAULTER_ALPHA};
    std::string roleName{"Assaulter"};
    float health{150.0f};
    float maxHealth{150.0f};
    float armorPoints{100.0f};
    float maxArmorPoints{100.0f};
    std::string primaryWeapon{"HK MP5A3 Tactical"};
    std::string sidearm{"Glock 22 .40 S&W"};
    bool hasGasMask{true};
    bool isAlive{true};
    bool isStunned{false};
    uint32 stunRemainingMs{0};
    LocationVector position;

    void TakeDamage(float rawDamage, bool isArmorPiercing) {
        if (!isAlive) return;
        if (armorPoints > 0.0f) {
            float armorMitigation = isArmorPiercing ? 0.40f : 0.75f;
            float dmgToArmor = rawDamage * armorMitigation;
            float dmgToHealth = rawDamage * (1.0f - armorMitigation);
            armorPoints = std::max(0.0f, armorPoints - dmgToArmor);
            health = std::max(0.0f, health - dmgToHealth);
        } else {
            health = std::max(0.0f, health - rawDamage);
        }
        if (health <= 0.0f) {
            health = 0.0f;
            isAlive = false;
        }
    }
};

struct BallisticShield {
    uint32 shieldId{0};
    float durability{600.0f};
    float maxDurability{600.0f};
    float coverArcDegrees{120.0f}; // 120-degree frontal protection
    float damageAbsorptionPercent{0.90f}; // 90% frontal damage absorbed by shield
    float stackedAlliesProtectionPercent{0.85f}; // 85% damage mitigation to trailing stack
    BallisticShieldFormation formation{BallisticShieldFormation::STACK_LINE};
    bool isDeployed{true};
    bool isBreached{false};
    uint32 totalRoundsDeflected{0};

    float AbsorbHit(float incomingDamage, bool isAP) {
        if (!isDeployed || isBreached) return incomingDamage;
        totalRoundsDeflected++;
        float absorbed = incomingDamage * damageAbsorptionPercent;
        if (isAP) absorbed *= 0.70f; // AP rounds chew through shields and penetrate more

        // AP rounds degrade shield durability significantly faster
        float durabilityLoss = isAP ? (incomingDamage * 1.35f) : absorbed;
        durability -= durabilityLoss;
        if (durability <= 0.0f) {
            durability = 0.0f;
            isBreached = true;
            isDeployed = false;
        }
        return incomingDamage - absorbed;
    }
};

struct SWATSquad {
    uint32 squadId{0};
    std::string callsign{"Sierra-1"};
    uint32 districtId{1};
    std::string districtName{"Slums"};
    std::vector<SWATOfficer> officers;
    BallisticShield shield;
    BreachManeuverPhase phase{BreachManeuverPhase::STAGING_AT_PERIMETER};
    LocationVector stagingLocation;
    LocationVector stackLocation;
    LocationVector breachTargetLocation;
    uint32 targetCrimeId{0};
    uint32 targetRacketId{0};
    uint32 phaseTimerMs{0};
    uint32 totalHostagesRescued{0};
    uint32 totalSuspectsDetained{0};
    uint32 totalSuspectsNeutralized{0};
    bool hasClearLOS{false};
    bool greenLightGranted{false};
    bool flashbangDeployed{false};
    bool tearGasDeployed{false};
    std::vector<std::string> operationalLog;

    SWATOfficer* GetOfficerByRole(SWATRole role) {
        for (auto& o : officers) {
            if (o.role == role) return &o;
        }
        return nullptr;
    }

    size_t GetActiveOfficerCount() const {
        size_t count = 0;
        for (const auto& o : officers) {
            if (o.isAlive) count++;
        }
        return count;
    }
};

struct SniperOverwatchPerch {
    uint32 perchId{0};
    std::string name;
    uint32 districtId{1};
    std::string districtName{"Downtown"};
    LocationVector vantageCoordinates; // Elevation Y: 45m - 110m
    LocationVector primaryTargetSector;
    float coverageRadius{800.0f};
    SniperOverwatchState state{SniperOverwatchState::SEARCHING_VANTAGE};
    SWATOfficer assignedSniper;
    uint32 designatedTargetEntityId{0};
    std::string targetDescription;
    bool laserDesignationActive{false};
    float shotAccuracy{0.98f};
    uint32 totalConfirmedTakedowns{0};
    uint32 stateTimerMs{0};
};

struct ActiveOrdnanceEffect {
    uint32 effectId{0};
    OrdnanceType type{OrdnanceType::ORDNANCE_FLASHBANG_M84};
    std::string name{"M84 Stun Flashbang"};
    LocationVector epicenter;
    float radiusMeters{14.0f};
    uint32 durationRemainingMs{7000};
    float acousticDecibels{170.0f};
    float candelaFlash{1500000.0f};
    bool isActive{true};
};

struct HostageRecord {
    uint32 hostageId{0};
    std::string name;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector location;
    HostageRescueState state{HostageRescueState::HELD_HOSTAGE};
    HostageMedicalTriage triage{HostageMedicalTriage::GREEN_WALKING_WOUNDED};
    float panicLevel{0.80f};
    uint32 assignedEscortSquadId{0};
    uint32 linkedCrimeId{0};
    bool isExtractedSafely{false};
};

struct VehicularRoadblock {
    uint32 roadblockId{0};
    std::string name;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector location;
    RoadblockType type{RoadblockType::ARTERIAL_SPIKE_CHOKEPOINT};
    uint32 cruisersDeployed{2};
    bool bearCatDeployed{true};
    bool spikeStripsDeployed{true};
    uint32 lanesCovered{3};
    float interceptionRadius{45.0f};
    uint32 totalVehiclesIntercepted{0};
    uint32 totalContrabandSeizedValue{0};
    bool isActive{true};
};

struct InternalAffairsSting {
    uint32 stingId{0};
    uint32 targetPrecinctId{3};
    std::string precinctName{"Harbor Division Precinct"};
    StingPhase phase{StingPhase::INACTIVE};
    std::string leadInvestigatorName{"Special Agent Marcus Kelly"};
    std::string targetOfficerName{"Captain Frank 'Bull' O'Malley"};
    float initialCorruptionScore{75.0f};
    uint32 markedBribeAmount{50000};
    std::string wiretapRecordingTranscript;
    uint32 phaseTimerMs{0};
    bool evidenceValidated{false};
    bool stingSuccessful{false};
    bool assistedByCastle{false};
    std::vector<std::string> investigationLog;
};

struct MMPDDispatchLog {
    uint32 logId{0};
    MMPD10Code code{MMPD10Code::CODE_10_31};
    std::string codeString{"10-31"};
    std::string unitCallsign{"Unit 1-Adam-12"};
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector location;
    std::string situationBrief;
    uint32 timestampMs{0};
    bool isTacticalSWAT{false};
    bool isMutualAid{false};
};

// ============================================================================
// Emergent Police & SWAT Manager Singleton
// ============================================================================

class EmergentPoliceManager : public Singleton<EmergentPoliceManager> {
public:
    friend void RunEmergentPoliceTestSuite();

    EmergentPoliceManager();
    ~EmergentPoliceManager();

    void Initialize();
    void Update(uint32 deltaMs);

    // SWAT Stack-and-Breach Maneuvers
    uint32 DeploySWATSquad(uint32 districtId, const std::string& callsign, const LocationVector& stagingPos);
    bool OrderStackAndBreach(uint32 squadId, const LocationVector& stackPos, const LocationVector& breachTargetPos,
                             uint32 linkedCrimeId = 0, uint32 linkedRacketId = 0);
    SWATSquad* GetSWATSquad(uint32 squadId);
    std::vector<SWATSquad*> GetSquadsInDistrict(uint32 districtId);
    bool AdvanceBreachPhase(uint32 squadId);
    bool DetonateBreachCharge(uint32 squadId);
    bool DeployFlashbang(uint32 squadId, const LocationVector& targetPos);
    bool DeployTearGas(uint32 squadId, const LocationVector& targetPos);
    bool ExecuteRoomEntry(uint32 squadId, std::vector<std::string>& outCallouts);
    bool SecureRoomAndArrest(uint32 squadId);
    bool DeclareCode4(uint32 squadId);

    // Ballistic Shield Formations & Kinetic Defense
    bool SetShieldFormation(uint32 squadId, BallisticShieldFormation formation);
    float ApplyIncomingDamageToSquad(uint32 squadId, float rawDamage, float attackAngleDegrees, bool isArmorPiercing);
    bool RepairOrReplaceShield(uint32 squadId);

    // Sniper Overwatch Perches
    const std::map<uint32, SniperOverwatchPerch>& GetAllSniperPerches() const { return m_sniperPerches; }
    SniperOverwatchPerch* GetSniperPerch(uint32 perchId);
    SniperOverwatchPerch* FindNearestPerchWithLOS(const LocationVector& targetPos, uint32 districtId);
    bool DesignateSniperTarget(uint32 perchId, uint32 targetId, const std::string& targetDesc);
    bool GrantGreenLight(uint32 perchId);
    bool ExecuteSniperTakedown(uint32 perchId, float& outDamageDealt);

    // Active Ordnance Systems
    const std::map<uint32, ActiveOrdnanceEffect>& GetActiveOrdnance() const { return m_activeOrdnance; }
    bool IsPointUnderTearGas(const LocationVector& pos) const;
    bool IsPointUnderFlashbangStun(const LocationVector& pos) const;

    // Hostage Rescue & Extraction Protocols
    uint32 RegisterHostage(const std::string& name, uint32 districtId, const LocationVector& pos, uint32 linkedCrimeId = 0);
    HostageRecord* GetHostage(uint32 hostageId);
    std::vector<HostageRecord*> GetHostagesInCrime(uint32 crimeId);
    bool AssignHostageEscort(uint32 hostageId, uint32 squadId);
    bool ExtractHostageToCCP(uint32 hostageId, const LocationVector& ccpPos);
    void TriageHostage(uint32 hostageId, HostageMedicalTriage triage);
    size_t GetTotalHostagesRescued() const { return m_totalHostagesExtracted; }

    // Vehicular Roadblocks & Spike Strip Interception
    uint32 DeployRoadblock(uint32 districtId, const std::string& name, const LocationVector& pos, RoadblockType type);
    VehicularRoadblock* GetRoadblock(uint32 roadblockId);
    std::vector<VehicularRoadblock*> GetRoadblocksInDistrict(uint32 districtId);
    bool InterceptVehicularTarget(uint32 roadblockId, const LocationVector& vehiclePos, bool isSmugglingConvoy, uint32 convoyId = 0);
    bool RemoveRoadblock(uint32 roadblockId);
    size_t GetTotalVehiclesSpikeStripped() const { return m_totalVehiclesSpikeStripped; }

    // MMPD Radio 10-Code Coordination Network
    uint32 Transmit10Code(MMPD10Code code, const std::string& callsign, uint32 districtId,
                          const LocationVector& loc, const std::string& brief,
                          bool isSWAT = false, bool isMutualAid = false);
    const std::deque<MMPDDispatchLog>& GetDispatchLogs() const { return m_dispatchLogs; }
    static std::string Get10CodeDescription(MMPD10Code code);
    static std::string Get10CodeString(MMPD10Code code);

    // Internal Affairs (IA) Corruption Sting Mechanics
    uint32 LaunchIASting(uint32 precinctId, const std::string& leadInvestigator = "Special Agent Marcus Kelly");
    InternalAffairsSting* GetIASting(uint32 stingId);
    InternalAffairsSting* GetActiveStingForPrecinct(uint32 precinctId);
    bool AdvanceIASting(uint32 stingId);
    bool IntegrateCastleEvidenceIntoSting(uint32 stingId);
    size_t GetTotalStingsExecuted() const { return m_totalStingsCompleted; }

    // Multi-Agent & Ecosystem Integration Hooks
    void OnEmergentCrimeReported(uint32 crimeId, uint32 districtId, const LocationVector& loc,
                                 EmergentCrimeType crimeType, uint32 hostageCount);
    void OnConvoyDetectedOnRoadway(uint32 convoyId, uint32 districtId, const LocationVector& currentPos);
    void OnFrankCastleAssistance(uint32 squadId, const std::string& tacticalAction);

    // Diagnostics, Reporting & JSON Persistence
    std::string GeneratePoliceSWATReport() const;
    std::string GenerateTacticalSquadsReport() const;
    std::string GenerateOverwatchReport() const;
    std::string GenerateRoadblocksReport() const;
    std::string GenerateIAStingsReport() const;
    bool SavePoliceStateToFile(const std::string& path = "PoliceSWATSimulation.json");
    bool LoadPoliceStateFromFile(const std::string& path = "PoliceSWATSimulation.json");

private:
    void InitializeDefaultSniperPerches();
    void InitializeDefaultRoadblocks();
    void InitializeDefaultSWATSquads();

    void UpdateBreachOperations(uint32 deltaMs);
    void UpdateOrdnanceEffects(uint32 deltaMs);
    void UpdateSniperOverwatch(uint32 deltaMs);
    void UpdateRoadblocks(uint32 deltaMs);
    void UpdateIAStings(uint32 deltaMs);

    std::map<uint32, SWATSquad> m_squads;
    std::map<uint32, SniperOverwatchPerch> m_sniperPerches;
    std::map<uint32, ActiveOrdnanceEffect> m_activeOrdnance;
    std::map<uint32, HostageRecord> m_hostages;
    std::map<uint32, VehicularRoadblock> m_roadblocks;
    std::map<uint32, InternalAffairsSting> m_iaStings;
    std::deque<MMPDDispatchLog> m_dispatchLogs;

    uint32 m_nextSquadId{1};
    uint32 m_nextPerchId{1};
    uint32 m_nextOrdnanceId{1};
    uint32 m_nextHostageId{1};
    uint32 m_nextRoadblockId{1};
    uint32 m_nextStingId{1};
    uint32 m_nextLogId{1};

    uint32 m_totalBreachesCompleted{0};
    uint32 m_totalHostagesExtracted{0};
    uint32 m_totalVehiclesSpikeStripped{0};
    uint32 m_totalStingsCompleted{0};
    uint32 m_totalSniperNeutralizations{0};

    mutable std::recursive_mutex m_mutex;
};

#define sEmergentPoliceMgr EmergentPoliceManager::getSingleton()

// Test Suite Declaration
void RunEmergentPoliceTestSuite();

#endif // MXOEMU_EMERGENT_POLICE_MANAGER_H

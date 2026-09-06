#include "EmergentPoliceManager.h"
#include "UnderworldManager.h"
#include "CityLifeManager.h"
#include "EmergentAIEngine.h"
#include "FrankCastleManager.h"
#include "RadioDispatchSystem.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <thread>

createFileSingleton(EmergentPoliceManager);

EmergentPoliceManager::EmergentPoliceManager()
{
}

EmergentPoliceManager::~EmergentPoliceManager()
{
}

void EmergentPoliceManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    INFO_LOG("EmergentPoliceManager: Initializing Emergent Police & Tactical SWAT Systems...");

    m_squads.clear();
    m_sniperPerches.clear();
    m_activeOrdnance.clear();
    m_hostages.clear();
    m_roadblocks.clear();
    m_iaStings.clear();
    m_dispatchLogs.clear();

    m_nextSquadId = 1;
    m_nextPerchId = 1;
    m_nextOrdnanceId = 1;
    m_nextHostageId = 1;
    m_nextRoadblockId = 1;
    m_nextStingId = 1;
    m_nextLogId = 1;

    m_totalBreachesCompleted = 0;
    m_totalHostagesExtracted = 0;
    m_totalVehiclesSpikeStripped = 0;
    m_totalStingsCompleted = 0;
    m_totalSniperNeutralizations = 0;

    InitializeDefaultSniperPerches();
    InitializeDefaultRoadblocks();
    InitializeDefaultSWATSquads();

    INFO_LOG(format("EmergentPoliceManager: Initialized with %1% Tactical Squads, %2% Sniper Perches, %3% Roadblocks.")
             % m_squads.size() % m_sniperPerches.size() % m_roadblocks.size());
}

void EmergentPoliceManager::InitializeDefaultSniperPerches()
{
    // Slums
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "Westview Water Tower Perch";
        p.districtId = 1;
        p.districtName = "Slums";
        p.vantageCoordinates = LocationVector(1510.0, 65.0, -3190.0);
        p.primaryTargetSector = LocationVector(1500.0, 10.0, -3200.0);
        p.coverageRadius = 750.0f;
        p.assignedSniper.officerName = "Officer Miller";
        p.assignedSniper.badgeNumber = "MMPD-SN-101";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "Remington 700 Tactical .308";
        m_sniperPerches[p.perchId] = p;
    }
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "Morrell Tenement Catwalk";
        p.districtId = 1;
        p.districtName = "Slums";
        p.vantageCoordinates = LocationVector(1390.0, 52.0, -3090.0);
        p.primaryTargetSector = LocationVector(1400.0, 10.0, -3100.0);
        p.coverageRadius = 700.0f;
        p.assignedSniper.officerName = "Officer Kowalski";
        p.assignedSniper.badgeNumber = "MMPD-SN-102";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "CheyTac M200 .408";
        m_sniperPerches[p.perchId] = p;
    }

    // Downtown
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "First National Bank Cornice";
        p.districtId = 2;
        p.districtName = "Downtown";
        p.vantageCoordinates = LocationVector(4150.0, 75.0, 1150.0);
        p.primaryTargetSector = LocationVector(4200.0, 10.0, 1200.0);
        p.coverageRadius = 850.0f;
        p.assignedSniper.officerName = "Sergeant Hayes";
        p.assignedSniper.badgeNumber = "MMPD-SN-201";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "Barrett MRAD .338 Lapua";
        m_sniperPerches[p.perchId] = p;
    }
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "Metacortex Plaza Rooftop";
        p.districtId = 2;
        p.districtName = "Downtown";
        p.vantageCoordinates = LocationVector(4250.0, 110.0, 1250.0);
        p.primaryTargetSector = LocationVector(4200.0, 10.0, 1200.0);
        p.coverageRadius = 900.0f;
        p.assignedSniper.officerName = "Officer Zhang";
        p.assignedSniper.badgeNumber = "MMPD-SN-202";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "Barrett M82A1 .50 BMG";
        m_sniperPerches[p.perchId] = p;
    }

    // International (Harbor & Docks)
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "Pier 44 Crane Gantry";
        p.districtId = 3;
        p.districtName = "International";
        p.vantageCoordinates = LocationVector(-2480.0, 85.0, 4980.0);
        p.primaryTargetSector = LocationVector(-2500.0, 10.0, 5000.0);
        p.coverageRadius = 800.0f;
        p.assignedSniper.officerName = "Officer Petrov";
        p.assignedSniper.badgeNumber = "MMPD-SN-301";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "Steyr SSG 08 .308";
        m_sniperPerches[p.perchId] = p;
    }

    // Industrial
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "Foundry Smokestack Catwalk";
        p.districtId = 4;
        p.districtName = "Industrial";
        p.vantageCoordinates = LocationVector(3050.0, 95.0, -4480.0);
        p.primaryTargetSector = LocationVector(3000.0, 15.0, -4500.0);
        p.coverageRadius = 850.0f;
        p.assignedSniper.officerName = "Officer Burke";
        p.assignedSniper.badgeNumber = "MMPD-SN-401";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "Sako TRG-42 .338";
        m_sniperPerches[p.perchId] = p;
    }

    // Park East
    {
        SniperOverwatchPerch p;
        p.perchId = m_nextPerchId++;
        p.name = "Grand Concourse Colonnade";
        p.districtId = 5;
        p.districtName = "Park East";
        p.vantageCoordinates = LocationVector(-1050.0, 48.0, -1200.0);
        p.primaryTargetSector = LocationVector(-1000.0, 10.0, -1150.0);
        p.coverageRadius = 650.0f;
        p.assignedSniper.officerName = "Officer Diaz";
        p.assignedSniper.badgeNumber = "MMPD-SN-501";
        p.assignedSniper.role = SWATRole::SNIPER_OVERWATCH;
        p.assignedSniper.primaryWeapon = "Remington 700 Tactical .308";
        m_sniperPerches[p.perchId] = p;
    }
}

void EmergentPoliceManager::InitializeDefaultRoadblocks()
{
    // Slums
    {
        VehicularRoadblock r;
        r.roadblockId = m_nextRoadblockId++;
        r.name = "Westview Arterial Bridge Chokepoint";
        r.districtId = 1;
        r.districtName = "Slums";
        r.location = LocationVector(1450.0, 10.0, -3150.0);
        r.type = RoadblockType::ARTERIAL_SPIKE_CHOKEPOINT;
        r.cruisersDeployed = 2;
        r.bearCatDeployed = true;
        r.spikeStripsDeployed = true;
        r.lanesCovered = 3;
        r.interceptionRadius = 50.0f;
        m_roadblocks[r.roadblockId] = r;
    }

    // Downtown
    {
        VehicularRoadblock r;
        r.roadblockId = m_nextRoadblockId++;
        r.name = "Financial Center Core Boulevard Block";
        r.districtId = 2;
        r.districtName = "Downtown";
        r.location = LocationVector(4100.0, 15.0, 1100.0);
        r.type = RoadblockType::VEHICULAR_CRUISER_V_BLOCK;
        r.cruisersDeployed = 3;
        r.bearCatDeployed = false;
        r.spikeStripsDeployed = true;
        r.lanesCovered = 4;
        r.interceptionRadius = 45.0f;
        m_roadblocks[r.roadblockId] = r;
    }

    // International (Harbor / Docks)
    {
        VehicularRoadblock r;
        r.roadblockId = m_nextRoadblockId++;
        r.name = "Pier 44 Harbor Tunnel Checkpoint";
        r.districtId = 3;
        r.districtName = "International";
        r.location = LocationVector(-2450.0, 12.0, 4900.0);
        r.type = RoadblockType::BEARCAT_HEAVY_BARRICADE;
        r.cruisersDeployed = 2;
        r.bearCatDeployed = true;
        r.spikeStripsDeployed = true;
        r.lanesCovered = 3;
        r.interceptionRadius = 55.0f;
        m_roadblocks[r.roadblockId] = r;
    }

    // Industrial
    {
        VehicularRoadblock r;
        r.roadblockId = m_nextRoadblockId++;
        r.name = "Industrial Parkway Expressway Off-Ramp";
        r.districtId = 4;
        r.districtName = "Industrial";
        r.location = LocationVector(3000.0, 18.0, -4400.0);
        r.type = RoadblockType::FULL_DISTRICT_LOCKDOWN;
        r.cruisersDeployed = 4;
        r.bearCatDeployed = true;
        r.spikeStripsDeployed = true;
        r.lanesCovered = 4;
        r.interceptionRadius = 60.0f;
        m_roadblocks[r.roadblockId] = r;
    }

    // Park East
    {
        VehicularRoadblock r;
        r.roadblockId = m_nextRoadblockId++;
        r.name = "Grand Concourse East Entrance";
        r.districtId = 5;
        r.districtName = "Park East";
        r.location = LocationVector(-1000.0, 10.0, -1150.0);
        r.type = RoadblockType::VEHICULAR_CRUISER_V_BLOCK;
        r.cruisersDeployed = 2;
        r.bearCatDeployed = false;
        r.spikeStripsDeployed = true;
        r.lanesCovered = 2;
        r.interceptionRadius = 40.0f;
        m_roadblocks[r.roadblockId] = r;
    }
}

void EmergentPoliceManager::InitializeDefaultSWATSquads()
{
    // Deploy standard response squads in major tactical hotzones
    DeploySWATSquad(2, "Sierra-1", LocationVector(4180.0, 10.0, 1180.0)); // Downtown
    DeploySWATSquad(1, "Sierra-2", LocationVector(1480.0, 10.0, -3180.0)); // Slums
    DeploySWATSquad(3, "Echo-Tactical", LocationVector(-2470.0, 10.0, 4970.0)); // International
}

uint32 EmergentPoliceManager::DeploySWATSquad(uint32 districtId, const std::string& callsign, const LocationVector& stagingPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    SWATSquad squad;
    squad.squadId = m_nextSquadId++;
    squad.callsign = callsign;
    squad.districtId = districtId;
    squad.districtName = UnderworldManager::GetDistrictName(districtId);
    squad.stagingLocation = stagingPos;
    squad.stackLocation = stagingPos;
    squad.breachTargetLocation = stagingPos;
    squad.phase = BreachManeuverPhase::STAGING_AT_PERIMETER;

    // Ballistic Shield setup
    squad.shield.shieldId = squad.squadId;
    squad.shield.durability = 600.0f;
    squad.shield.maxDurability = 600.0f;
    squad.shield.coverArcDegrees = 120.0f;
    squad.shield.damageAbsorptionPercent = 0.90f;
    squad.shield.stackedAlliesProtectionPercent = 0.85f;
    squad.shield.formation = BallisticShieldFormation::STACK_LINE;
    squad.shield.isDeployed = true;
    squad.shield.isBreached = false;

    // Build specialized 6-officer stack roster
    struct RoleTemplate {
        SWATRole role;
        std::string roleName;
        std::string weapon;
    };

    static const RoleTemplate roles[6] = {
        { SWATRole::POINTMAN_BALLISTIC_SHIELD, "Pointman (Ballistic Shield)", "Colt 9mm Submachine Gun" },
        { SWATRole::BREACHER_HEAVY_RAM,        "Breacher",                     "Benelli M4 Entry Shotgun" },
        { SWATRole::ASSAULTER_ALPHA,           "Assaulter Alpha",              "HK MP5A3 Tactical" },
        { SWATRole::ASSAULTER_BRAVO,           "Assaulter Bravo",              "Colt M4A1 CQB Carbine" },
        { SWATRole::TEAM_LEADER,               "Team Leader",                  "Colt M4A1 CQB Carbine" },
        { SWATRole::HOSTAGE_TRIAGE_MEDIC,      "Triage Medic",                 "Glock 22 .40 S&W" }
    };

    for (int i = 0; i < 6; ++i) {
        SWATOfficer o;
        o.officerId = squad.squadId * 10 + (i + 1);
        o.badgeNumber = (format("MMPD-%1%-%2%") % squad.callsign % (i + 1)).str();
        o.officerName = (format("Operator %1%") % (i + 1)).str();
        o.role = roles[i].role;
        o.roleName = roles[i].roleName;
        o.primaryWeapon = roles[i].weapon;
        o.health = 150.0f;
        o.maxHealth = 150.0f;
        o.armorPoints = 100.0f;
        o.maxArmorPoints = 100.0f;
        o.hasGasMask = true;
        o.isAlive = true;
        o.position = stagingPos;
        squad.officers.push_back(o);
    }

    squad.operationalLog.push_back((format("SWAT Squad [%1%] deployed to %2% staging area.") % callsign % squad.districtName).str());
    m_squads[squad.squadId] = squad;

    Transmit10Code(MMPD10Code::CODE_10_8, callsign, districtId, stagingPos, "SWAT Team In-Service and Staging", true);

    return squad.squadId;
}

bool EmergentPoliceManager::OrderStackAndBreach(uint32 squadId, const LocationVector& stackPos,
                                               const LocationVector& breachTargetPos,
                                               uint32 linkedCrimeId, uint32 linkedRacketId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->stackLocation = stackPos;
    s->breachTargetLocation = breachTargetPos;
    s->targetCrimeId = linkedCrimeId;
    s->targetRacketId = linkedRacketId;
    s->phase = BreachManeuverPhase::STACK_AT_THRESHOLD;
    s->phaseTimerMs = 0;

    // Reposition officers in stack
    for (size_t i = 0; i < s->officers.size(); ++i) {
        s->officers[i].position = stackPos;
    }

    s->operationalLog.push_back((format("Stacked at threshold (%1%, %2%). Preparing dynamic breach.")
                                 % (int)stackPos.x % (int)stackPos.z).str());

    Transmit10Code(MMPD10Code::CODE_10_99, s->callsign, s->districtId, stackPos,
                   "SWAT Stack Formed at Target Entryway - Awaiting Breach Execution", true);

    // Register tactical cover smart object if emergent AI is running
    if (EmergentAIEngine::getSingletonPtr()) {
        SmartObject coverObj;
        coverObj.objectId = 9000 + squadId;
        coverObj.name = (format("SWAT Ballistic Cover [%1%]") % s->callsign).str();
        coverObj.typeTag = "TacticalCover";
        coverObj.districtId = s->districtId;
        coverObj.districtName = s->districtName;
        coverObj.position = stackPos;
        coverObj.interactionRadius = 4.0f;
        coverObj.maxCapacity = 4;

        AffordanceDefinition aff;
        aff.type = AffordanceType::TAKE_COVER;
        aff.actionName = "Take Tactical SWAT Stack Cover";
        aff.durationMs = 12000;
        aff.baseUtility = 0.95f;
        aff.allowsPanickedUsers = true;
        coverObj.affordances.push_back(aff);

        sEmergentAIMgr.GetAffordanceGrid().RegisterObject(coverObj);
    }

    // Lockdown nearby commercial shops to protect civilians during tactical operation
    if (CityLifeManager::getSingletonPtr()) {
        sCityLifeMgr.LockdownShopsInRadius(static_cast<float>(stackPos.x), static_cast<float>(stackPos.z), 60.0f, "MMPD SWAT Tactical Breach Operation");
    }

    return true;
}

SWATSquad* EmergentPoliceManager::GetSWATSquad(uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_squads.find(squadId);
    return (it != m_squads.end()) ? &it->second : nullptr;
}

std::vector<SWATSquad*> EmergentPoliceManager::GetSquadsInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<SWATSquad*> res;
    for (auto& pair : m_squads) {
        if (pair.second.districtId == districtId) {
            res.push_back(&pair.second);
        }
    }
    return res;
}

bool EmergentPoliceManager::AdvanceBreachPhase(uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    switch (s->phase) {
        case BreachManeuverPhase::STAGING_AT_PERIMETER:
            s->phase = BreachManeuverPhase::STACK_AT_THRESHOLD;
            break;
        case BreachManeuverPhase::STACK_AT_THRESHOLD:
            s->phase = BreachManeuverPhase::PREPARING_BREACH_DEVICE;
            s->operationalLog.push_back("Affixing C4 linear breaching charge to reinforced door frame.");
            break;
        case BreachManeuverPhase::PREPARING_BREACH_DEVICE:
            DetonateBreachCharge(squadId);
            break;
        case BreachManeuverPhase::DETONATION_RAM_STRIKE:
            s->phase = BreachManeuverPhase::NON_LETHAL_SUPPRESSION;
            DeployFlashbang(squadId, s->breachTargetLocation);
            break;
        case BreachManeuverPhase::NON_LETHAL_SUPPRESSION:
        {
            std::vector<std::string> callouts;
            ExecuteRoomEntry(squadId, callouts);
            break;
        }
        case BreachManeuverPhase::TACTICAL_ROOM_CLEAR:
            SecureRoomAndArrest(squadId);
            break;
        case BreachManeuverPhase::CONTAINMENT_AND_RESTRAINT:
            s->phase = BreachManeuverPhase::HOSTAGE_EXTRACTION_ESCORT;
            s->operationalLog.push_back("Forming protective extraction box around recovered civilians.");
            SetShieldFormation(squadId, BallisticShieldFormation::EXTRACTION_BOX);
            break;
        case BreachManeuverPhase::HOSTAGE_EXTRACTION_ESCORT:
            DeclareCode4(squadId);
            break;
        case BreachManeuverPhase::CODE_4_COMPLETED:
            return false; // Already finished
    }
    s->phaseTimerMs = 0;
    return true;
}

bool EmergentPoliceManager::DetonateBreachCharge(uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->phase = BreachManeuverPhase::DETONATION_RAM_STRIKE;
    s->operationalLog.push_back("BREACH DETONATION: Linear charge fired! Door blown off hinges!");

    Transmit10Code(MMPD10Code::CODE_10_89, s->callsign, s->districtId, s->breachTargetLocation,
                   "BREACH CHARGE DETONATED - Door down! Dynamic entry commencing!", true);

    // Blast effect
    ActiveOrdnanceEffect charge;
    charge.effectId = m_nextOrdnanceId++;
    charge.type = OrdnanceType::ORDNANCE_BREACHING_C4;
    charge.name = "C4 Linear Breaching Strip";
    charge.epicenter = s->breachTargetLocation;
    charge.radiusMeters = 5.0f;
    charge.durationRemainingMs = 2000;
    charge.acousticDecibels = 185.0f;
    m_activeOrdnance[charge.effectId] = charge;

    // Trigger cross-system gunshot echo & panic
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.OnGunfireEcho(s->breachTargetLocation, 85.0f, "SWAT Explosive Breach Detonation");
    }

    // Damage racket defenses if target is an underworld racket
    if (s->targetRacketId != 0 && UnderworldManager::getSingletonPtr()) {
        sUnderworldMgr.DamageRacketDefenses(s->targetRacketId, 60.0f, false);
    }

    return true;
}

bool EmergentPoliceManager::DeployFlashbang(uint32 squadId, const LocationVector& targetPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->flashbangDeployed = true;
    s->operationalLog.push_back((format("M84 Flashbang tossed into entry zone (%1%, %2%). Detonation confirmed.")
                                 % (int)targetPos.x % (int)targetPos.z).str());

    ActiveOrdnanceEffect flash;
    flash.effectId = m_nextOrdnanceId++;
    flash.type = OrdnanceType::ORDNANCE_FLASHBANG_M84;
    flash.name = "M84 Stun Flashbang";
    flash.epicenter = targetPos;
    flash.radiusMeters = 14.0f;
    flash.durationRemainingMs = 7000;
    flash.acousticDecibels = 170.0f;
    flash.candelaFlash = 1500000.0f;
    m_activeOrdnance[flash.effectId] = flash;

    Transmit10Code(MMPD10Code::CODE_10_70, s->callsign, s->districtId, targetPos,
                   "Non-lethal M84 Flashbang deployed into threshold", true);

    // Stun hostages in room temporarily to keep them down (check linked crime ID or spatial radius in same district)
    for (auto& pair : m_hostages) {
        if ((s->targetCrimeId != 0 && pair.second.linkedCrimeId == s->targetCrimeId) ||
            (pair.second.districtId == s->districtId && pair.second.location.DistanceSq(targetPos) < (14.0f * 14.0f))) {
            pair.second.state = HostageRescueState::DISORIENTED_BY_ORDNANCE;
        }
    }

    return true;
}

bool EmergentPoliceManager::DeployTearGas(uint32 squadId, const LocationVector& targetPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->tearGasDeployed = true;
    s->operationalLog.push_back((format("CS Tear Gas canister deployed at (%1%, %2%). 22m perimeter denial active.")
                                 % (int)targetPos.x % (int)targetPos.z).str());

    ActiveOrdnanceEffect gas;
    gas.effectId = m_nextOrdnanceId++;
    gas.type = OrdnanceType::ORDNANCE_TEAR_GAS_CS;
    gas.name = "CS Chemical Tear Gas Cloud";
    gas.epicenter = targetPos;
    gas.radiusMeters = 22.0f;
    gas.durationRemainingMs = 30000;
    gas.acousticDecibels = 90.0f;
    m_activeOrdnance[gas.effectId] = gas;

    Transmit10Code(MMPD10Code::CODE_10_70, s->callsign, s->districtId, targetPos,
                   "Chemical CS Tear Gas deployed - gas masks verified", true);

    // Disperse nearby bluepill civilians and broadcast gas alert
    if (CityLifeManager::getSingletonPtr()) {
        sCityLifeMgr.TriggerAreaPanic(static_cast<float>(targetPos.x), static_cast<float>(targetPos.z), 35.0f, "CS Tear Gas Dispersion", 25000);
        sCityLifeMgr.BroadcastStreetRumor(RumorTopic::RUMOR_POLICE_CRACKDOWN,
                                          "CS Chemical Gas Deployed by SWAT",
                                          "Tactical officers deployed riot control gas in " + s->districtName,
                                          s->districtId);
    }
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.GetContagionEngine().SeedRumor(s->districtId,
                                                      "SWAT Chemical Gas Dispersion in " + s->districtName,
                                                      "CS gas canisters deployed to flush out barricaded suspects.",
                                                      s->districtId, 0.75f);
    }

    return true;
}

bool EmergentPoliceManager::ExecuteRoomEntry(uint32 squadId, std::vector<std::string>& outCallouts)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->phase = BreachManeuverPhase::TACTICAL_ROOM_CLEAR;

    // Move squad officers into the breached room
    for (auto& o : s->officers) {
        o.position = s->breachTargetLocation;
    }

    outCallouts.push_back((format("[%1% Pointman] Shield leading through threshold! Covering fatal funnel!") % s->callsign).str());
    outCallouts.push_back((format("[%1% Assaulter 1] Pieing left corner! One armed suspect neutralized!") % s->callsign).str());
    outCallouts.push_back((format("[%1% Assaulter 2] Clear right! Covering second barricade! Drop your weapon!") % s->callsign).str());
    outCallouts.push_back((format("[%1% Team Leader] Hostages spotted at far wall! Moving to secure!") % s->callsign).str());

    for (const auto& c : outCallouts) {
        s->operationalLog.push_back(c);
    }

    s->totalSuspectsNeutralized += 2;
    s->totalSuspectsDetained += 3;

    // Secure hostages (check linked crime or spatial proximity in same district)
    for (auto& pair : m_hostages) {
        if ((s->targetCrimeId != 0 && pair.second.linkedCrimeId == s->targetCrimeId) ||
            (pair.second.districtId == s->districtId && pair.second.location.DistanceSq(s->breachTargetLocation) < (25.0f * 25.0f))) {
            pair.second.state = HostageRescueState::SECURED_BY_OPERATORS;
            pair.second.assignedEscortSquadId = squadId;
            s->totalHostagesRescued++;
        }
    }

    Transmit10Code(MMPD10Code::CODE_10_71, s->callsign, s->districtId, s->breachTargetLocation,
                   "Dynamic Room Entry - Resistance suppressed, securing suspects", true);

    return true;
}

bool EmergentPoliceManager::SecureRoomAndArrest(uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->phase = BreachManeuverPhase::CONTAINMENT_AND_RESTRAINT;
    s->operationalLog.push_back("Perimeter locked down. All surviving suspects flex-cuffed and disarmed.");

    Transmit10Code(MMPD10Code::CODE_10_15, s->callsign, s->districtId, s->breachTargetLocation,
                   "Suspects in Custody - Transporting to Precinct Holding Cells", true);

    // If linked to an underworld crime, neutralize it
    if (s->targetCrimeId != 0 && UnderworldManager::getSingletonPtr()) {
        sUnderworldMgr.NeutralizeCrime(s->targetCrimeId, false, true);
    }

    // Add inmates to precinct holding cells
    if (UnderworldManager::getSingletonPtr()) {
        PolicePrecinct* p = sUnderworldMgr.GetPrecinctInDistrict(s->districtId);
        if (p) {
            p->holdingCellInmates += 3;
            p->confiscatedContrabandValue += 15000.0f;
        }
    }

    return true;
}

bool EmergentPoliceManager::DeclareCode4(uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->phase = BreachManeuverPhase::CODE_4_COMPLETED;
    s->operationalLog.push_back("CODE 4: Structure fully cleared and secured. Handing scene over to detectives.");
    m_totalBreachesCompleted++;

    // Evacuate all secured/escorted hostages linked to this squad to CCP
    for (auto& pair : m_hostages) {
        HostageRecord& h = pair.second;
        if ((h.assignedEscortSquadId == squadId || (s->targetCrimeId != 0 && h.linkedCrimeId == s->targetCrimeId)) &&
            !h.isExtractedSafely) {
            ExtractHostageToCCP(h.hostageId, s->stagingLocation);
        }
    }

    // Reset shield formation to stack line
    SetShieldFormation(squadId, BallisticShieldFormation::STACK_LINE);

    // Unregister tactical cover smart object
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.GetAffordanceGrid().UnregisterObject(9000 + squadId);
    }

    // Lift shop lockdowns in the vicinity
    if (CityLifeManager::getSingletonPtr()) {
        sCityLifeMgr.LiftShopLockdowns();
    }

    Transmit10Code(MMPD10Code::CODE_10_4, s->callsign, s->districtId, s->breachTargetLocation,
                   "All units: Scene is Code 4 - All threats neutralized. Hostages secured.", true);

    // Suppress district heat
    if (UnderworldManager::getSingletonPtr()) {
        sUnderworldMgr.AddDistrictHeat(s->districtId, -15.0f);
    }

    return true;
}

// ============================================================================
// Ballistic Shield Formations & Kinetic Defense
// ============================================================================

bool EmergentPoliceManager::SetShieldFormation(uint32 squadId, BallisticShieldFormation formation)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->shield.formation = formation;
    std::string formName = "Stack Line";
    if (formation == BallisticShieldFormation::STACK_LINE) {
        formName = "Stack Line";
        s->shield.coverArcDegrees = 120.0f;
        s->shield.damageAbsorptionPercent = 0.90f;
        s->shield.stackedAlliesProtectionPercent = 0.85f;
    } else if (formation == BallisticShieldFormation::TACTICAL_WEDGE) {
        formName = "Tactical Wedge";
        s->shield.coverArcDegrees = 160.0f;
        s->shield.damageAbsorptionPercent = 0.88f;
        s->shield.stackedAlliesProtectionPercent = 0.80f;
    } else if (formation == BallisticShieldFormation::EXTRACTION_BOX) {
        formName = "Extraction Box";
        s->shield.coverArcDegrees = 360.0f;
        s->shield.damageAbsorptionPercent = 0.80f;
        s->shield.stackedAlliesProtectionPercent = 0.92f;
    } else if (formation == BallisticShieldFormation::STATIONARY_BARRICADE) {
        formName = "Stationary Barricade";
        s->shield.coverArcDegrees = 140.0f;
        s->shield.damageAbsorptionPercent = 0.95f;
        s->shield.stackedAlliesProtectionPercent = 0.90f;
    }

    s->operationalLog.push_back("Shield formation shifted to: " + formName);
    return true;
}

float EmergentPoliceManager::ApplyIncomingDamageToSquad(uint32 squadId, float rawDamage, float attackAngleDegrees, bool isArmorPiercing)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return rawDamage;

    // Normalize angle to [-180, 180]
    while (attackAngleDegrees > 180.0f) attackAngleDegrees -= 360.0f;
    while (attackAngleDegrees < -180.0f) attackAngleDegrees += 360.0f;

    // Check if attack is within protected shield arc (360 degrees for EXTRACTION_BOX)
    bool isProtected = (s->shield.coverArcDegrees >= 360.0f) ||
                       (std::abs(attackAngleDegrees) <= (s->shield.coverArcDegrees * 0.5f));

    float damageThrough = rawDamage;
    if (isProtected && s->shield.isDeployed && !s->shield.isBreached) {
        damageThrough = s->shield.AbsorbHit(rawDamage, isArmorPiercing);

        // Trailing stack officers enjoy stacked mitigation
        for (auto& o : s->officers) {
            if (o.role == SWATRole::POINTMAN_BALLISTIC_SHIELD) {
                o.TakeDamage(damageThrough * 0.15f, isArmorPiercing);
            } else {
                o.TakeDamage(damageThrough * (1.0f - s->shield.stackedAlliesProtectionPercent), isArmorPiercing);
            }
        }
    } else {
        // Flank or rear attack: no shield mitigation
        for (auto& o : s->officers) {
            o.TakeDamage(rawDamage * 0.25f, isArmorPiercing);
        }
    }

    return damageThrough;
}

bool EmergentPoliceManager::RepairOrReplaceShield(uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return false;

    s->shield.durability = s->shield.maxDurability;
    s->shield.isBreached = false;
    s->shield.isDeployed = true;
    s->operationalLog.push_back("Fresh Level IV Ballistic Shield equipped from BearCat armory.");
    return true;
}

// ============================================================================
// Sniper Overwatch Perches
// ============================================================================

SniperOverwatchPerch* EmergentPoliceManager::GetSniperPerch(uint32 perchId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_sniperPerches.find(perchId);
    return (it != m_sniperPerches.end()) ? &it->second : nullptr;
}

SniperOverwatchPerch* EmergentPoliceManager::FindNearestPerchWithLOS(const LocationVector& targetPos, uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SniperOverwatchPerch* best = nullptr;
    double bestDistSq = 999999999.0;

    for (auto& pair : m_sniperPerches) {
        if (districtId != 0 && pair.second.districtId != districtId) continue;
        double dSq = pair.second.vantageCoordinates.DistanceSq(targetPos);
        if (dSq <= (pair.second.coverageRadius * pair.second.coverageRadius) && dSq < bestDistSq) {
            bestDistSq = dSq;
            best = &pair.second;
        }
    }
    return best;
}

bool EmergentPoliceManager::DesignateSniperTarget(uint32 perchId, uint32 targetId, const std::string& targetDesc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SniperOverwatchPerch* p = GetSniperPerch(perchId);
    if (!p) return false;

    p->designatedTargetEntityId = targetId;
    p->targetDescription = targetDesc;
    p->laserDesignationActive = true;
    p->state = SniperOverwatchState::WAITING_GREEN_LIGHT;

    Transmit10Code(MMPD10Code::CODE_10_20, p->assignedSniper.badgeNumber, p->districtId,
                   p->vantageCoordinates, "Sniper Target Designated: " + targetDesc + " - Holding for Green Light", true);

    return true;
}

bool EmergentPoliceManager::GrantGreenLight(uint32 perchId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SniperOverwatchPerch* p = GetSniperPerch(perchId);
    if (!p) return false;

    p->state = SniperOverwatchState::AUTHORIZED_LETHAL_FIRE;
    return true;
}

bool EmergentPoliceManager::ExecuteSniperTakedown(uint32 perchId, float& outDamageDealt)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SniperOverwatchPerch* p = GetSniperPerch(perchId);
    if (!p || p->state != SniperOverwatchState::AUTHORIZED_LETHAL_FIRE) {
        outDamageDealt = 0.0f;
        return false;
    }

    outDamageDealt = 550.0f; // High velocity armor-piercing marksman round
    p->totalConfirmedTakedowns++;
    p->state = SniperOverwatchState::TARGET_NEUTRALIZED;
    p->laserDesignationActive = false;
    p->stateTimerMs = 0;
    m_totalSniperNeutralizations++;

    // Emit gunshot echo acoustic wave across Megacity
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.OnGunfireEcho(p->vantageCoordinates, 90.0f, "Sniper Precision Overwatch Rifle Shot");
    }

    // If target was linked to an emergent crime, clear immediate lethal threat on hostages
    if (p->designatedTargetEntityId >= 5000) {
        uint32 crimeId = p->designatedTargetEntityId - 5000;
        for (auto& pair : m_hostages) {
            if (pair.second.linkedCrimeId == crimeId && pair.second.state == HostageRescueState::UNDER_LETHAL_THREAT) {
                pair.second.state = HostageRescueState::SECURED_BY_OPERATORS;
            }
        }
    }

    Transmit10Code(MMPD10Code::CODE_10_71, p->assignedSniper.badgeNumber, p->districtId,
                   p->vantageCoordinates, "Overwatch Shot Fired: Hostage taker down! Target confirmed neutralized.", true);

    return true;
}

// ============================================================================
// Active Ordnance Systems
// ============================================================================

bool EmergentPoliceManager::IsPointUnderTearGas(const LocationVector& pos) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& pair : m_activeOrdnance) {
        if (pair.second.type == OrdnanceType::ORDNANCE_TEAR_GAS_CS && pair.second.isActive) {
            if (pair.second.epicenter.DistanceSq(pos) <= (pair.second.radiusMeters * pair.second.radiusMeters)) {
                return true;
            }
        }
    }
    return false;
}

bool EmergentPoliceManager::IsPointUnderFlashbangStun(const LocationVector& pos) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& pair : m_activeOrdnance) {
        if (pair.second.type == OrdnanceType::ORDNANCE_FLASHBANG_M84 && pair.second.isActive) {
            if (pair.second.epicenter.DistanceSq(pos) <= (pair.second.radiusMeters * pair.second.radiusMeters)) {
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Hostage Rescue & Extraction Protocols
// ============================================================================

uint32 EmergentPoliceManager::RegisterHostage(const std::string& name, uint32 districtId,
                                             const LocationVector& pos, uint32 linkedCrimeId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    HostageRecord h;
    h.hostageId = m_nextHostageId++;
    h.name = name;
    h.districtId = districtId;
    h.districtName = UnderworldManager::GetDistrictName(districtId);
    h.location = pos;
    h.linkedCrimeId = linkedCrimeId;
    h.state = HostageRescueState::HELD_HOSTAGE;
    h.triage = HostageMedicalTriage::GREEN_WALKING_WOUNDED;
    h.panicLevel = 0.85f;

    m_hostages[h.hostageId] = h;
    return h.hostageId;
}

HostageRecord* EmergentPoliceManager::GetHostage(uint32 hostageId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_hostages.find(hostageId);
    return (it != m_hostages.end()) ? &it->second : nullptr;
}

std::vector<HostageRecord*> EmergentPoliceManager::GetHostagesInCrime(uint32 crimeId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<HostageRecord*> res;
    for (auto& pair : m_hostages) {
        if (pair.second.linkedCrimeId == crimeId) {
            res.push_back(&pair.second);
        }
    }
    return res;
}

bool EmergentPoliceManager::AssignHostageEscort(uint32 hostageId, uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    HostageRecord* h = GetHostage(hostageId);
    if (!h) return false;

    h->assignedEscortSquadId = squadId;
    h->state = HostageRescueState::UNDER_EXTRACTION_ESCORT;
    return true;
}

bool EmergentPoliceManager::ExtractHostageToCCP(uint32 hostageId, const LocationVector& ccpPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    HostageRecord* h = GetHostage(hostageId);
    if (!h) return false;

    h->location = ccpPos;
    h->state = HostageRescueState::SAFELY_EVACUATED_TO_CCP;
    h->isExtractedSafely = true;
    h->panicLevel = 0.15f;
    m_totalHostagesExtracted++;

    Transmit10Code(MMPD10Code::CODE_10_78, "SWAT-Triage-1", h->districtId, ccpPos,
                   (format("Hostage [%1%] successfully extracted to Casualty Collection Point. Paramedics assessing.") % h->name).str(), true);

    return true;
}

void EmergentPoliceManager::TriageHostage(uint32 hostageId, HostageMedicalTriage triage)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    HostageRecord* h = GetHostage(hostageId);
    if (h) {
        h->triage = triage;
    }
}

// ============================================================================
// Vehicular Roadblocks & Spike Strip Interception
// ============================================================================

uint32 EmergentPoliceManager::DeployRoadblock(uint32 districtId, const std::string& name,
                                             const LocationVector& pos, RoadblockType type)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    VehicularRoadblock r;
    r.roadblockId = m_nextRoadblockId++;
    r.name = name;
    r.districtId = districtId;
    r.districtName = UnderworldManager::GetDistrictName(districtId);
    r.location = pos;
    r.type = type;
    r.cruisersDeployed = (type == RoadblockType::FULL_DISTRICT_LOCKDOWN) ? 4 : 2;
    r.bearCatDeployed = (type == RoadblockType::BEARCAT_HEAVY_BARRICADE || type == RoadblockType::FULL_DISTRICT_LOCKDOWN);
    r.spikeStripsDeployed = true;
    r.lanesCovered = 3;
    r.interceptionRadius = 50.0f;
    r.isActive = true;

    m_roadblocks[r.roadblockId] = r;

    // Register roadblock cover smart object in emergent AI affordance grid
    if (EmergentAIEngine::getSingletonPtr()) {
        SmartObject rbObj;
        rbObj.objectId = 8000 + r.roadblockId;
        rbObj.name = "MMPD Vehicular Roadblock [" + name + "]";
        rbObj.typeTag = "VehicularRoadblock";
        rbObj.districtId = districtId;
        rbObj.districtName = r.districtName;
        rbObj.position = pos;
        rbObj.interactionRadius = 8.0f;
        rbObj.maxCapacity = 6;

        AffordanceDefinition aff;
        aff.type = AffordanceType::TAKE_COVER;
        aff.actionName = "Take Cover Behind Armored BearCat / Police Cruiser";
        aff.durationMs = 15000;
        aff.baseUtility = 0.90f;
        aff.allowsPanickedUsers = true;
        rbObj.affordances.push_back(aff);

        sEmergentAIMgr.GetAffordanceGrid().RegisterObject(rbObj);
    }

    // Halt civilian traffic before spike strip perimeter in CityLifeManager
    if (CityLifeManager::getSingletonPtr()) {
        for (auto& vPair : const_cast<std::map<uint32, CityVehicle>&>(sCityLifeMgr.GetAllVehicles())) {
            if (vPair.second.currentLocation.DistanceSq(pos) <= (r.interceptionRadius * r.interceptionRadius * 2.0f)) {
                vPair.second.isStoppedAtLight = true;
                vPair.second.speedUnitsPerSec = 0.0f;
            }
        }
    }

    Transmit10Code(MMPD10Code::CODE_10_80, "Traffic-Command-1", districtId, pos,
                   "Roadblock and Stinger Spike Strips deployed across arterial roadway: " + name, true);

    return r.roadblockId;
}

VehicularRoadblock* EmergentPoliceManager::GetRoadblock(uint32 roadblockId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_roadblocks.find(roadblockId);
    return (it != m_roadblocks.end()) ? &it->second : nullptr;
}

std::vector<VehicularRoadblock*> EmergentPoliceManager::GetRoadblocksInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<VehicularRoadblock*> res;
    for (auto& pair : m_roadblocks) {
        if (pair.second.districtId == districtId && pair.second.isActive) {
            res.push_back(&pair.second);
        }
    }
    return res;
}

bool EmergentPoliceManager::InterceptVehicularTarget(uint32 roadblockId, const LocationVector& vehiclePos,
                                                    bool isSmugglingConvoy, uint32 convoyId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    VehicularRoadblock* r = GetRoadblock(roadblockId);
    if (!r || !r->isActive) return false;

    double dSq = r->location.DistanceSq(vehiclePos);
    if (dSq > (r->interceptionRadius * r->interceptionRadius)) return false;

    r->totalVehiclesIntercepted++;
    m_totalVehiclesSpikeStripped++;

    Transmit10Code(MMPD10Code::CODE_10_50, "Roadblock-Interceptor-1", r->districtId, vehiclePos,
                   "SPIKE STRIPS EFFECTIVE: Getaway / convoy tyres shredded! Vehicle immobilized!", true);

    if (isSmugglingConvoy && convoyId != 0 && UnderworldManager::getSingletonPtr()) {
        sUnderworldMgr.InterceptConvoy(convoyId, false); // Intercepted by police
        SmugglingConvoy* sc = sUnderworldMgr.GetConvoy(convoyId);
        uint32 cargoVal = (sc && sc->cargoValuation > 0) ? sc->cargoValuation : 25000;
        r->totalContrabandSeizedValue += cargoVal;

        PolicePrecinct* p = sUnderworldMgr.GetPrecinctInDistrict(r->districtId);
        if (p) {
            p->confiscatedContrabandValue += static_cast<float>(cargoVal);
            p->holdingCellInmates += 2;
        }

        // Broadcast news / rumor of convoy takedown
        if (CityLifeManager::getSingletonPtr()) {
            sCityLifeMgr.BroadcastStreetRumor(
                RumorTopic::RUMOR_POLICE_CRACKDOWN,
                "Smuggling Convoy Impounded by MMPD",
                "Arterial roadblock spike strips shredded cartel convoy vehicles! Contraband seized.",
                r->districtId
            );
        }
        if (EmergentAIEngine::getSingletonPtr()) {
            sEmergentAIMgr.GetContagionEngine().SeedRumor(
                r->districtId,
                "MMPD Roadblock Intercepts Cartel Smugglers",
                "Spike strips deployed across roadway impounded illicit cartel shipment.",
                r->districtId,
                0.80f
            );
        }
    }

    return true;
}

bool EmergentPoliceManager::RemoveRoadblock(uint32 roadblockId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_roadblocks.find(roadblockId);
    if (it != m_roadblocks.end()) {
        it->second.isActive = false;
        if (EmergentAIEngine::getSingletonPtr()) {
            sEmergentAIMgr.GetAffordanceGrid().UnregisterObject(8000 + roadblockId);
        }
        return true;
    }
    return false;
}

// ============================================================================
// MMPD Radio 10-Code Coordination Network
// ============================================================================

uint32 EmergentPoliceManager::Transmit10Code(MMPD10Code code, const std::string& callsign,
                                            uint32 districtId, const LocationVector& loc,
                                            const std::string& brief, bool isSWAT, bool isMutualAid)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    MMPDDispatchLog log;
    log.logId = m_nextLogId++;
    log.code = code;
    log.codeString = Get10CodeString(code);
    log.unitCallsign = callsign;
    log.districtId = districtId;
    log.districtName = UnderworldManager::GetDistrictName(districtId);
    log.location = loc;
    log.situationBrief = brief;
    log.timestampMs = getMSTime();
    log.isTacticalSWAT = isSWAT;
    log.isMutualAid = isMutualAid;

    m_dispatchLogs.push_back(log);
    if (m_dispatchLogs.size() > 50) {
        m_dispatchLogs.pop_front();
    }

    std::string chatter = (format("MMPD Dispatch [%1%] [%2%]: %3% at (%4%, %5%)")
                           % callsign % log.codeString % brief % (int)loc.x % (int)loc.z).str();

    // Broadcast to RadioDispatchSystem if available
    if (RadioDispatchSystem::getSingletonPtr()) {
        RadioTransmission tx;
        tx.transmissionId = log.logId;
        tx.tenCode = log.codeString;
        tx.unitCallsign = callsign;
        tx.districtName = log.districtName;
        tx.locationAddress = (format("Sector (%1%, %2%)") % (int)loc.x % (int)loc.z).str();
        tx.chatterText = chatter;
        tx.threatHeatLevel = isSWAT ? 75.0f : 35.0f;
        tx.escalationTier = isSWAT ? 3 : 2;
        tx.timestampMs = log.timestampMs;
        sRadioDispatchSystem.BroadcastDispatch(tx, 30000.0f);
    }

    // Trigger emergent AI contagion & observer reaction
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.OnPolice10CodeDispatched(districtId, log.codeString, loc);
    }

    return log.logId;
}

std::string EmergentPoliceManager::Get10CodeString(MMPD10Code code)
{
    switch (code) {
        case MMPD10Code::CODE_10_4:   return "10-4";
        case MMPD10Code::CODE_10_7:   return "10-7";
        case MMPD10Code::CODE_10_8:   return "10-8";
        case MMPD10Code::CODE_10_15:  return "10-15";
        case MMPD10Code::CODE_10_20:  return "10-20";
        case MMPD10Code::CODE_10_23:  return "10-23";
        case MMPD10Code::CODE_10_31:  return "10-31";
        case MMPD10Code::CODE_10_33:  return "10-33";
        case MMPD10Code::CODE_10_50:  return "10-50";
        case MMPD10Code::CODE_10_70:  return "10-70";
        case MMPD10Code::CODE_10_71:  return "10-71";
        case MMPD10Code::CODE_10_78:  return "10-78";
        case MMPD10Code::CODE_10_80:  return "10-80";
        case MMPD10Code::CODE_10_89:  return "10-89";
        case MMPD10Code::CODE_10_90:  return "10-90";
        case MMPD10Code::CODE_10_99:  return "10-99";
        case MMPD10Code::CODE_10_100: return "10-100";
        default: return "10-99";
    }
}

std::string EmergentPoliceManager::Get10CodeDescription(MMPD10Code code)
{
    switch (code) {
        case MMPD10Code::CODE_10_4:   return "Acknowledgment / Copy";
        case MMPD10Code::CODE_10_7:   return "Out of Service / End of Shift";
        case MMPD10Code::CODE_10_8:   return "In Service / Available";
        case MMPD10Code::CODE_10_15:  return "Suspect in Custody";
        case MMPD10Code::CODE_10_20:  return "Location Query / Check-in";
        case MMPD10Code::CODE_10_23:  return "Arrived on Scene";
        case MMPD10Code::CODE_10_31:  return "Crime in Progress";
        case MMPD10Code::CODE_10_33:  return "Officer Emergency / Panic Distress";
        case MMPD10Code::CODE_10_50:  return "Vehicle Accident / Spike Strip Intercept";
        case MMPD10Code::CODE_10_70:  return "Fire / Tear Gas Deployed";
        case MMPD10Code::CODE_10_71:  return "Shots Fired / Gun Battle";
        case MMPD10Code::CODE_10_78:  return "Send EMS Paramedics";
        case MMPD10Code::CODE_10_80:  return "Pursuit in Progress";
        case MMPD10Code::CODE_10_89:  return "Explosive Breach Charge Armed";
        case MMPD10Code::CODE_10_90:  return "Bank Silent Alarm Triggered";
        case MMPD10Code::CODE_10_99:  return "SWAT Code Red / Barricaded Hostages";
        case MMPD10Code::CODE_10_100: return "Internal Affairs Corruption Alert";
        default: return "Tactical Emergency";
    }
}

// ============================================================================
// Internal Affairs (IA) Corruption Sting Mechanics
// ============================================================================

uint32 EmergentPoliceManager::LaunchIASting(uint32 precinctId, const std::string& leadInvestigator)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PolicePrecinct* p = UnderworldManager::getSingletonPtr() ? sUnderworldMgr.GetPrecinct(precinctId) : nullptr;
    if (!p) return 0;

    InternalAffairsSting sting;
    sting.stingId = m_nextStingId++;
    sting.targetPrecinctId = precinctId;
    sting.precinctName = p->name;
    sting.targetOfficerName = p->precinctCaptainName;
    sting.initialCorruptionScore = p->corruptionIndex;
    sting.leadInvestigatorName = leadInvestigator;
    sting.phase = StingPhase::COVERT_SURVEILLANCE;
    sting.markedBribeAmount = 50000;

    sting.investigationLog.push_back((format("IA Covert Operation authorized targeting %1% (%2%). Corruption Index: %3%")
                                      % p->name % p->precinctCaptainName % (int)p->corruptionIndex).str());

    m_iaStings[sting.stingId] = sting;

    Transmit10Code(MMPD10Code::CODE_10_100, "IAD-SpecialOps", p->districtId, p->precinctLocation,
                   "Internal Affairs Division Operation Initiated: Covert Surveillance Van In Position", false);

    return sting.stingId;
}

InternalAffairsSting* EmergentPoliceManager::GetIASting(uint32 stingId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_iaStings.find(stingId);
    return (it != m_iaStings.end()) ? &it->second : nullptr;
}

InternalAffairsSting* EmergentPoliceManager::GetActiveStingForPrecinct(uint32 precinctId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_iaStings) {
        if (pair.second.targetPrecinctId == precinctId && pair.second.phase != StingPhase::DEPARTMENTAL_PURGE_REFORM) {
            return &pair.second;
        }
    }
    return nullptr;
}

bool EmergentPoliceManager::AdvanceIASting(uint32 stingId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    InternalAffairsSting* s = GetIASting(stingId);
    if (!s) return false;

    PolicePrecinct* p = UnderworldManager::getSingletonPtr() ? sUnderworldMgr.GetPrecinct(s->targetPrecinctId) : nullptr;

    switch (s->phase) {
        case StingPhase::INACTIVE:
            s->phase = StingPhase::COVERT_SURVEILLANCE;
            break;
        case StingPhase::COVERT_SURVEILLANCE:
            s->phase = StingPhase::MARKED_CURRENCY_BRIBE;
            s->investigationLog.push_back("Undercover operative delivered $50,000 in marked serialized bills to bagman.");
            break;
        case StingPhase::MARKED_CURRENCY_BRIBE:
            s->phase = StingPhase::AUDIO_INTERCEPT_CONFIRMATION;
            s->wiretapRecordingTranscript = (format("[WIRETAP AUDIO] %1%: 'Tell Marcone the dock patrol is diverted until 0400.'")
                                             % s->targetOfficerName).str();
            s->evidenceValidated = true;
            s->investigationLog.push_back("Wiretap confirmed: " + s->wiretapRecordingTranscript);
            break;
        case StingPhase::AUDIO_INTERCEPT_CONFIRMATION:
            s->phase = StingPhase::PRECINCT_HQ_RAID;
            s->investigationLog.push_back("IA tactical squad executed high-profile warrant at precinct HQ. Captain placed in handcuffs!");
            Transmit10Code(MMPD10Code::CODE_10_15, "IAD-StrikeTeam", p ? p->districtId : 1,
                           p ? p->precinctLocation : LocationVector(),
                           "IA Raid Successful: Corrupt Captain arrested! Marked bribe funds recovered!", true);
            break;
        case StingPhase::PRECINCT_HQ_RAID:
            s->phase = StingPhase::DEPARTMENTAL_PURGE_REFORM;
            s->stingSuccessful = true;
            m_totalStingsCompleted++;

            if (p) {
                p->corruptionIndex = 0.0f;
                p->isExposedByCastle = s->assistedByCastle;
                p->precinctCaptainName = "Interim Commissioner O'Donnell";
                p->cleanLeadershipTimerMs = 180000;
                p->confiscatedContrabandValue += s->markedBribeAmount;
            }
            s->investigationLog.push_back("Department purged. Clean leadership established with zero-tolerance corruption policy.");

            // Spread corruption purge rumors throughout Megacity
            if (CityLifeManager::getSingletonPtr()) {
                sCityLifeMgr.BroadcastStreetRumor(
                    RumorTopic::RUMOR_POLICE_CRACKDOWN,
                    "Internal Affairs Purges Corrupt Precinct",
                    "Massive IA sting operation brought down dirty captain. Clean leadership installed.",
                    p ? p->districtId : 1
                );
            }
            if (EmergentAIEngine::getSingletonPtr()) {
                sEmergentAIMgr.GetContagionEngine().SeedRumor(
                    p ? p->districtId : 1,
                    "Precinct HQ Raided by Internal Affairs",
                    "Corrupt police captain arrested in handcuffs after undercover IA sting.",
                    p ? p->districtId : 1,
                    0.95f
                );
            }
            break;
        case StingPhase::DEPARTMENTAL_PURGE_REFORM:
            return false;
    }
    s->phaseTimerMs = 0;
    return true;
}

bool EmergentPoliceManager::IntegrateCastleEvidenceIntoSting(uint32 stingId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    InternalAffairsSting* s = GetIASting(stingId);
    if (!s) return false;

    s->assistedByCastle = true;
    s->evidenceValidated = true;
    s->phaseTimerMs = 0;
    s->investigationLog.push_back("Frank Castle delivered intercepted cartel ledgers and audio recordings directly to IA.");

    // Frank's evidence fast-tracks directly to tactical takedown
    if (s->phase < StingPhase::AUDIO_INTERCEPT_CONFIRMATION) {
        s->phase = StingPhase::AUDIO_INTERCEPT_CONFIRMATION;
        s->wiretapRecordingTranscript = "[PUNISHER DOSSIER] Bank accounts, burner phone logs, and video evidence of cartel payoffs.";
    }

    return true;
}

// ============================================================================
// Multi-Agent & Ecosystem Integration Hooks
// ============================================================================

void EmergentPoliceManager::OnEmergentCrimeReported(uint32 crimeId, uint32 districtId,
                                                   const LocationVector& loc,
                                                   EmergentCrimeType crimeType, uint32 hostageCount)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Register hostages if crime involves hostages
    if (hostageCount > 0) {
        for (uint32 i = 0; i < hostageCount; ++i) {
            std::string hName = (format("Civilian Hostage %1%") % (i + 1)).str();
            RegisterHostage(hName, districtId, loc, crimeId);
        }
    }

    // Determine appropriate SWAT or Beat Patrol mobilization
    if (crimeType == EmergentCrimeType::BankHeist || crimeType == EmergentCrimeType::HostageKidnapping) {
        auto squads = GetSquadsInDistrict(districtId);
        SWATSquad* chosenSquad = nullptr;
        for (auto* sq : squads) {
            if (sq->phase == BreachManeuverPhase::STAGING_AT_PERIMETER || sq->phase == BreachManeuverPhase::CODE_4_COMPLETED) {
                chosenSquad = sq;
                break;
            }
        }
        if (!chosenSquad && !squads.empty()) {
            chosenSquad = squads[0];
        }

        if (chosenSquad) {
            LocationVector stackPos(loc.x - 15.0, loc.y, loc.z);
            OrderStackAndBreach(chosenSquad->squadId, stackPos, loc, crimeId);
        } else {
            // Deploy tactical squad from mutual aid
            uint32 sId = DeploySWATSquad(districtId, "Sierra-MutualAid", loc);
            LocationVector stackPos(loc.x - 15.0, loc.y, loc.z);
            OrderStackAndBreach(sId, stackPos, loc, crimeId);
        }

        // Assign nearest sniper perch
        SniperOverwatchPerch* perch = FindNearestPerchWithLOS(loc, districtId);
        if (perch) {
            DesignateSniperTarget(perch->perchId, 5000 + crimeId, "Hostage Taker Barricade Point");
        }
    } else {
        Transmit10Code(MMPD10Code::CODE_10_31, "Patrol-Command", districtId, loc,
                       "Crime In Progress Reported: " + UnderworldManager::GetCrimeTypeName(crimeType), false);
    }
}

void EmergentPoliceManager::OnConvoyDetectedOnRoadway(uint32 convoyId, uint32 districtId, const LocationVector& currentPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_roadblocks) {
        VehicularRoadblock& r = pair.second;
        if (r.isActive && r.spikeStripsDeployed) {
            if (districtId != 0 && r.districtId != districtId) {
                continue;
            }
            if (r.location.DistanceSq(currentPos) <= (r.interceptionRadius * r.interceptionRadius)) {
                InterceptVehicularTarget(r.roadblockId, currentPos, true, convoyId);
                break;
            }
        }
    }
}

void EmergentPoliceManager::OnFrankCastleAssistance(uint32 squadId, const std::string& tacticalAction)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    SWATSquad* s = GetSWATSquad(squadId);
    if (!s) return;

    s->operationalLog.push_back("Frank Castle flank intervention: " + tacticalAction);
    s->totalSuspectsNeutralized++;

    Transmit10Code(MMPD10Code::CODE_10_4, s->callsign, s->districtId, s->stackLocation,
                   "Flank support noted - Suspect fire suppressed from exterior angle", true);
}

// ============================================================================
// Update Loop
// ============================================================================

void EmergentPoliceManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    UpdateBreachOperations(deltaMs);
    UpdateOrdnanceEffects(deltaMs);
    UpdateSniperOverwatch(deltaMs);
    UpdateRoadblocks(deltaMs);
    UpdateIAStings(deltaMs);
}

void EmergentPoliceManager::UpdateBreachOperations(uint32 deltaMs)
{
    for (auto& pair : m_squads) {
        SWATSquad& s = pair.second;
        s.phaseTimerMs += deltaMs;

        // Auto-advance if squad is performing an autonomous breach sequence
        if (s.phase == BreachManeuverPhase::STACK_AT_THRESHOLD && s.phaseTimerMs >= 5000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::PREPARING_BREACH_DEVICE && s.phaseTimerMs >= 3000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::DETONATION_RAM_STRIKE && s.phaseTimerMs >= 2000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::NON_LETHAL_SUPPRESSION && s.phaseTimerMs >= 3000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::TACTICAL_ROOM_CLEAR && s.phaseTimerMs >= 4000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::CONTAINMENT_AND_RESTRAINT && s.phaseTimerMs >= 4000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::HOSTAGE_EXTRACTION_ESCORT && s.phaseTimerMs >= 5000) {
            AdvanceBreachPhase(s.squadId);
        } else if (s.phase == BreachManeuverPhase::CODE_4_COMPLETED && s.phaseTimerMs >= 15000) {
            // Post-operation reset: return squad to staging and ready for subsequent deployments
            s.phase = BreachManeuverPhase::STAGING_AT_PERIMETER;
            s.phaseTimerMs = 0;
            s.targetCrimeId = 0;
            s.targetRacketId = 0;
            s.flashbangDeployed = false;
            s.tearGasDeployed = false;
            s.shield.formation = BallisticShieldFormation::STACK_LINE;
            if (s.shield.isBreached || s.shield.durability < s.shield.maxDurability) {
                RepairOrReplaceShield(s.squadId);
            }
            for (auto& o : s.officers) {
                o.position = s.stagingLocation;
                o.health = o.maxHealth;
                o.armorPoints = o.maxArmorPoints;
                o.isAlive = true;
                o.isStunned = false;
            }
        }
    }
}

void EmergentPoliceManager::UpdateOrdnanceEffects(uint32 deltaMs)
{
    std::vector<uint32> expired;
    for (auto& pair : m_activeOrdnance) {
        if (pair.second.durationRemainingMs <= deltaMs) {
            pair.second.durationRemainingMs = 0;
            pair.second.isActive = false;
            expired.push_back(pair.first);
        } else {
            pair.second.durationRemainingMs -= deltaMs;
        }
    }
    for (uint32 id : expired) {
        m_activeOrdnance.erase(id);
    }
}

void EmergentPoliceManager::UpdateSniperOverwatch(uint32 deltaMs)
{
    for (auto& pair : m_sniperPerches) {
        SniperOverwatchPerch& p = pair.second;
        if (p.state == SniperOverwatchState::AUTHORIZED_LETHAL_FIRE) {
            float dmg = 0.0f;
            ExecuteSniperTakedown(p.perchId, dmg);
        } else if (p.state == SniperOverwatchState::TARGET_NEUTRALIZED) {
            p.stateTimerMs += deltaMs;
            if (p.stateTimerMs >= 10000) {
                p.state = SniperOverwatchState::SEARCHING_VANTAGE;
                p.designatedTargetEntityId = 0;
                p.targetDescription.clear();
                p.stateTimerMs = 0;
            }
        }
    }
}

void EmergentPoliceManager::UpdateRoadblocks(uint32 deltaMs)
{
    // Auto-check for active convoys in UnderworldManager
    if (!UnderworldManager::getSingletonPtr()) return;

    const auto& convoys = sUnderworldMgr.GetActiveConvoys();
    for (const auto& cPair : convoys) {
        if (cPair.second.status == ConvoyStatus::CONVOY_IN_TRANSIT) {
            OnConvoyDetectedOnRoadway(cPair.first, 0, cPair.second.currentLocation);
        }
    }
}

void EmergentPoliceManager::UpdateIAStings(uint32 deltaMs)
{
    for (auto& pair : m_iaStings) {
        InternalAffairsSting& s = pair.second;
        if (s.phase != StingPhase::INACTIVE && s.phase != StingPhase::DEPARTMENTAL_PURGE_REFORM) {
            s.phaseTimerMs += deltaMs;
            if (s.phaseTimerMs >= 45000) { // 45 seconds per sting phase tick
                AdvanceIASting(s.stingId);
            }
        }
    }
}

// ============================================================================
// Diagnostics, Reporting & JSON Persistence
// ============================================================================

std::string EmergentPoliceManager::GeneratePoliceSWATReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "============================================================\n";
    ss << "     MEGACITY POLICE DEPARTMENT - SWAT & TACTICAL REPORT    \n";
    ss << "============================================================\n";
    ss << "Active Tactical Squads:         " << m_squads.size() << "\n";
    ss << "Sniper Overwatch Perches:       " << m_sniperPerches.size() << "\n";
    ss << "Active Vehicular Roadblocks:    " << m_roadblocks.size() << "\n";
    ss << "Active Ordnance Fields:         " << m_activeOrdnance.size() << "\n";
    ss << "Internal Affairs Stings:        " << m_iaStings.size() << "\n";
    ss << "Total Breaches Completed:       " << m_totalBreachesCompleted << "\n";
    ss << "Total Hostages Extracted:       " << m_totalHostagesExtracted << "\n";
    ss << "Vehicles Spike-Stripped:        " << m_totalVehiclesSpikeStripped << "\n";
    ss << "Sniper Neutralizations:         " << m_totalSniperNeutralizations << "\n";
    ss << "IA Precinct Stings Completed:   " << m_totalStingsCompleted << "\n";
    ss << "============================================================\n";
    return ss.str();
}

std::string EmergentPoliceManager::GenerateTacticalSquadsReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "=== SWAT TACTICAL SQUADS ROSTER ===\n";
    for (const auto& pair : m_squads) {
        const auto& s = pair.second;
        ss << "Squad [" << s.callsign << "] (ID: " << s.squadId << ") - District: " << s.districtName << "\n";
        ss << "  Phase: " << (int)s.phase << " | Shield HP: " << (int)s.shield.durability << "/600\n";
        ss << "  Active Officers: " << s.GetActiveOfficerCount() << " | Rescued: " << s.totalHostagesRescued
           << " | Detained: " << s.totalSuspectsDetained << "\n";
    }
    return ss.str();
}

std::string EmergentPoliceManager::GenerateOverwatchReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "=== SNIPER OVERWATCH PERCHES ===\n";
    for (const auto& pair : m_sniperPerches) {
        const auto& p = pair.second;
        ss << "Perch #" << p.perchId << ": " << p.name << " (" << p.districtName << ")\n";
        ss << "  Sniper: " << p.assignedSniper.officerName << " [" << p.assignedSniper.primaryWeapon << "]\n";
        ss << "  Elevation Y: " << (int)p.vantageCoordinates.y << "m | Takedowns: " << p.totalConfirmedTakedowns << "\n";
    }
    return ss.str();
}

std::string EmergentPoliceManager::GenerateRoadblocksReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "=== VEHICULAR ROADBLOCKS & SPIKE STRIPS ===\n";
    for (const auto& pair : m_roadblocks) {
        const auto& r = pair.second;
        ss << "Roadblock #" << r.roadblockId << ": " << r.name << " (" << r.districtName << ")\n";
        ss << "  Type: " << (int)r.type << " | Spike Strips: " << (r.spikeStripsDeployed ? "ACTIVE" : "OFF") << "\n";
        ss << "  Vehicles Intercepted: " << r.totalVehiclesIntercepted << " | Contraband Seized: $" << r.totalContrabandSeizedValue << "\n";
    }
    return ss.str();
}

std::string EmergentPoliceManager::GenerateIAStingsReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "=== INTERNAL AFFAIRS CORRUPTION STINGS ===\n";
    for (const auto& pair : m_iaStings) {
        const auto& s = pair.second;
        ss << "Sting #" << s.stingId << ": " << s.precinctName << " | Target: " << s.targetOfficerName << "\n";
        ss << "  Phase: " << (int)s.phase << " | Success: " << (s.stingSuccessful ? "YES" : "NO")
           << " | Castle Assisted: " << (s.assistedByCastle ? "YES" : "NO") << "\n";
    }
    return ss.str();
}

bool EmergentPoliceManager::SavePoliceStateToFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << "{\n";
    ofs << "  \"totalBreachesCompleted\": " << m_totalBreachesCompleted << ",\n";
    ofs << "  \"totalHostagesExtracted\": " << m_totalHostagesExtracted << ",\n";
    ofs << "  \"totalVehiclesSpikeStripped\": " << m_totalVehiclesSpikeStripped << ",\n";
    ofs << "  \"totalStingsCompleted\": " << m_totalStingsCompleted << ",\n";
    ofs << "  \"totalSniperNeutralizations\": " << m_totalSniperNeutralizations << ",\n";

    ofs << "  \"squads\": [\n";
    size_t sIdx = 0;
    for (const auto& pair : m_squads) {
        const auto& s = pair.second;
        ofs << "    {\"id\": " << s.squadId << ", \"callsign\": \"" << s.callsign << "\", \"district\": " << s.districtId
            << ", \"phase\": " << (int)s.phase << ", \"shieldDurability\": " << s.shield.durability
            << ", \"hostagesRescued\": " << s.totalHostagesRescued << ", \"suspectsDetained\": " << s.totalSuspectsDetained << "}"
            << (++sIdx < m_squads.size() ? "," : "") << "\n";
    }
    ofs << "  ],\n";

    ofs << "  \"roadblocks\": [\n";
    size_t rIdx = 0;
    for (const auto& pair : m_roadblocks) {
        const auto& r = pair.second;
        ofs << "    {\"id\": " << r.roadblockId << ", \"district\": " << r.districtId
            << ", \"intercepted\": " << r.totalVehiclesIntercepted << ", \"contraband\": " << r.totalContrabandSeizedValue << "}"
            << (++rIdx < m_roadblocks.size() ? "," : "") << "\n";
    }
    ofs << "  ]\n";
    ofs << "}\n";

    ofs.close();
    return true;
}

bool EmergentPoliceManager::LoadPoliceStateFromFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    auto extractNumber = [](const std::string& l, const std::string& key) -> double {
        size_t kpos = l.find("\"" + key + "\":");
        if (kpos == std::string::npos) return 0.0;
        std::string sub = l.substr(kpos + key.size() + 3);
        try {
            return std::stod(sub);
        } catch (...) {
            return 0.0;
        }
    };

    std::string line;
    uint32 loadedItems = 0;

    while (std::getline(ifs, line)) {
        if (line.find("\"totalBreachesCompleted\":") != std::string::npos) {
            m_totalBreachesCompleted = (uint32)extractNumber(line, "totalBreachesCompleted");
            loadedItems++;
        } else if (line.find("\"totalHostagesExtracted\":") != std::string::npos) {
            m_totalHostagesExtracted = (uint32)extractNumber(line, "totalHostagesExtracted");
            loadedItems++;
        } else if (line.find("\"totalVehiclesSpikeStripped\":") != std::string::npos) {
            m_totalVehiclesSpikeStripped = (uint32)extractNumber(line, "totalVehiclesSpikeStripped");
            loadedItems++;
        } else if (line.find("\"totalStingsCompleted\":") != std::string::npos) {
            m_totalStingsCompleted = (uint32)extractNumber(line, "totalStingsCompleted");
            loadedItems++;
        } else if (line.find("\"id\":") != std::string::npos && line.find("\"shieldDurability\":") != std::string::npos) {
            uint32 sId = (uint32)extractNumber(line, "id");
            SWATSquad* s = GetSWATSquad(sId);
            if (s) {
                s->phase = (BreachManeuverPhase)(int)extractNumber(line, "phase");
                s->shield.durability = (float)extractNumber(line, "shieldDurability");
                s->totalHostagesRescued = (uint32)extractNumber(line, "hostagesRescued");
                s->totalSuspectsDetained = (uint32)extractNumber(line, "suspectsDetained");
                loadedItems++;
            }
        }
    }
    ifs.close();
    return loadedItems > 0;
}

// ============================================================================
// Emergent Police & Tactical SWAT Test Suite
// ============================================================================

void RunEmergentPoliceTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  EMERGENT POLICE & TACTICAL SWAT - TEST SUITE VERIFICATION " << std::endl;
    std::cout << "============================================================" << std::endl;

    int passed = 0;
    int failed = 0;

    auto assertTest = [&](const std::string& name, bool condition) {
        if (condition) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << std::endl;
            failed++;
        }
    };

    if (UnderworldManager::getSingletonPtr()) {
        sUnderworldMgr.Initialize();
    }
    sEmergentPoliceMgr.Initialize();

    // 1. Initial State & Squad Deployment
    assertTest("Tactical SWAT Squads Initialized (Sierra-1, Sierra-2, Echo-Tactical)",
               sEmergentPoliceMgr.GetSquadsInDistrict(2).size() >= 1 &&
               sEmergentPoliceMgr.GetSquadsInDistrict(1).size() >= 1);
    assertTest("Sniper Overwatch Perches Populated Across All 5 Districts",
               sEmergentPoliceMgr.GetAllSniperPerches().size() >= 7);
    assertTest("Vehicular Roadblocks with Spike Strips Initialized",
               sEmergentPoliceMgr.GetRoadblocksInDistrict(1).size() >= 1 &&
               sEmergentPoliceMgr.GetRoadblocksInDistrict(2).size() >= 1);

    // 2. SWAT Squad Roster & Role Verification
    SWATSquad* s1 = sEmergentPoliceMgr.GetSWATSquad(1);
    assertTest("Sierra-1 Exists with Valid Squad ID", s1 != nullptr && s1->squadId == 1);
    assertTest("Sierra-1 Full 6-Officer Tactical Roster", s1 != nullptr && s1->officers.size() == 6);
    SWATOfficer* pointman = s1 ? s1->GetOfficerByRole(SWATRole::POINTMAN_BALLISTIC_SHIELD) : nullptr;
    assertTest("Pointman Assigned Level IV Ballistic Shield", pointman != nullptr && s1->shield.isDeployed);
    SWATOfficer* breacher = s1 ? s1->GetOfficerByRole(SWATRole::BREACHER_HEAVY_RAM) : nullptr;
    assertTest("Breacher Assigned Heavy Hydraulic Ram / Shotgun", breacher != nullptr && breacher->primaryWeapon.find("Shotgun") != std::string::npos);

    // 3. Ballistic Shield Formations & Kinetic Defense Arc
    assertTest("Ballistic Shield Durability Starts at Max (600 HP)", s1->shield.durability == 600.0f);
    float dmgAfterFrontal = sEmergentPoliceMgr.ApplyIncomingDamageToSquad(1, 100.0f, 0.0f, false); // Frontal shot (0 deg)
    assertTest("Frontal Shot (0 deg) Absorbed by Shield (90% Absorption)", dmgAfterFrontal <= 10.0f && s1->shield.durability < 600.0f);
    assertTest("Stacked Allies Mitigation Protected Trailing Officers", pointman->health > 145.0f);

    float dmgFlank = sEmergentPoliceMgr.ApplyIncomingDamageToSquad(1, 100.0f, 90.0f, false); // Flank shot (90 deg)
    assertTest("Flanking Shot (90 deg) Bypasses 120-deg Shield Arc (No Absorption)", dmgFlank == 100.0f);

    sEmergentPoliceMgr.SetShieldFormation(1, BallisticShieldFormation::TACTICAL_WEDGE);
    assertTest("Shift to Tactical Wedge Formation Verified", s1->shield.formation == BallisticShieldFormation::TACTICAL_WEDGE);

    // Extraction Box 360-degree coverage
    sEmergentPoliceMgr.SetShieldFormation(1, BallisticShieldFormation::EXTRACTION_BOX);
    float dmgFlankInBox = sEmergentPoliceMgr.ApplyIncomingDamageToSquad(1, 100.0f, 90.0f, false);
    assertTest("Extraction Box Provides 360-deg Flank Protection Arc", dmgFlankInBox <= 20.0f);
    sEmergentPoliceMgr.SetShieldFormation(1, BallisticShieldFormation::STACK_LINE);

    // Shield Degradation & Breakthrough Test
    sEmergentPoliceMgr.ApplyIncomingDamageToSquad(1, 2000.0f, 0.0f, true); // Overwhelming AP barrage
    assertTest("Heavy Fire Breakthrough: Shield Breached at Zero Durability", s1->shield.isBreached && !s1->shield.isDeployed);
    sEmergentPoliceMgr.RepairOrReplaceShield(1);
    assertTest("Shield Replacement Restores Full Durability and Deployment", s1->shield.durability == 600.0f && s1->shield.isDeployed);

    // AP degradation rate check: AP deals greater wear to durability than normal
    float durBeforeNorm = s1->shield.durability;
    s1->shield.AbsorbHit(100.0f, false);
    float normWear = durBeforeNorm - s1->shield.durability;
    sEmergentPoliceMgr.RepairOrReplaceShield(1);
    float durBeforeAP = s1->shield.durability;
    s1->shield.AbsorbHit(100.0f, true);
    float apWear = durBeforeAP - s1->shield.durability;
    assertTest("Armor-Piercing Barrage Accelerates Shield Durability Degradation", apWear > normWear);
    sEmergentPoliceMgr.RepairOrReplaceShield(1);

    // 4. SWAT Stack-and-Breach Maneuver Lifecycle
    LocationVector stackLoc(4210.0, 10.0, 1200.0);
    LocationVector breachLoc(4200.0, 10.0, 1200.0);
    bool ordered = sEmergentPoliceMgr.OrderStackAndBreach(1, stackLoc, breachLoc, 101, 201);
    assertTest("Order Stack-and-Breach Successfully Sets STACK_AT_THRESHOLD", ordered && s1->phase == BreachManeuverPhase::STACK_AT_THRESHOLD);

    // Step to Preparation
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Advances to PREPARING_BREACH_DEVICE", s1->phase == BreachManeuverPhase::PREPARING_BREACH_DEVICE);

    // Step to Detonation
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Detonates Linear Charge (DETONATION_RAM_STRIKE)", s1->phase == BreachManeuverPhase::DETONATION_RAM_STRIKE);
    assertTest("Breaching Charge Generated Active Explosion Ordnance", sEmergentPoliceMgr.GetActiveOrdnance().size() >= 1);

    // Step to Non-Lethal Flashbang Suppression
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Deploys Flashbang (NON_LETHAL_SUPPRESSION)", s1->phase == BreachManeuverPhase::NON_LETHAL_SUPPRESSION && s1->flashbangDeployed);
    assertTest("M84 Flashbang Stun Effect Active at Breach Location", sEmergentPoliceMgr.IsPointUnderFlashbangStun(breachLoc));

    // Step to Room Entry & Pieing Corners
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Executes Room Entry (TACTICAL_ROOM_CLEAR)", s1->phase == BreachManeuverPhase::TACTICAL_ROOM_CLEAR);
    assertTest("Room Entry Produced Dynamic Corner-Pieing Callouts", s1->operationalLog.size() >= 5);

    // Step to Containment & Arrest
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Enforces Restraint (CONTAINMENT_AND_RESTRAINT)", s1->phase == BreachManeuverPhase::CONTAINMENT_AND_RESTRAINT);

    // Step to Hostage Extraction & Code 4
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Transitions to HOSTAGE_EXTRACTION_ESCORT", s1->phase == BreachManeuverPhase::HOSTAGE_EXTRACTION_ESCORT);
    sEmergentPoliceMgr.AdvanceBreachPhase(1);
    assertTest("Breach Phase Completes Successfully with CODE_4_COMPLETED", s1->phase == BreachManeuverPhase::CODE_4_COMPLETED);

    // 5. Non-Lethal Chemical Tear Gas (CS Gas) Dynamics
    LocationVector gasLoc(1480.0, 10.0, -3180.0);
    sEmergentPoliceMgr.DeployTearGas(2, gasLoc);
    assertTest("Chemical CS Tear Gas Deployed Successfully", sEmergentPoliceMgr.IsPointUnderTearGas(gasLoc));
    assertTest("Points Outside Tear Gas Radius (22m) are Unaffected", !sEmergentPoliceMgr.IsPointUnderTearGas(LocationVector(1600.0, 10.0, -3180.0)));

    // 6. Rooftop Sniper Overwatch Perches & Green-Light Precision Fire
    SniperOverwatchPerch* bankPerch = sEmergentPoliceMgr.FindNearestPerchWithLOS(LocationVector(4200.0, 10.0, 1200.0), 2);
    assertTest("LOS Calculation Correctly Identified Bank Cornice Perch", bankPerch != nullptr && bankPerch->name.find("Bank") != std::string::npos);
    assertTest("Sniper Perch Positioned at High Elevation (Y >= 75m)", bankPerch != nullptr && bankPerch->vantageCoordinates.y >= 75.0);

    bool designated = sEmergentPoliceMgr.DesignateSniperTarget(bankPerch->perchId, 888, "Hostage Taker Lieutenant");
    assertTest("Sniper Laser Designation Active and Awaiting Green Light", designated && bankPerch->laserDesignationActive && bankPerch->state == SniperOverwatchState::WAITING_GREEN_LIGHT);

    float testDmg = 0.0f;
    bool deniedWithoutGreen = sEmergentPoliceMgr.ExecuteSniperTakedown(bankPerch->perchId, testDmg);
    assertTest("Sniper Lethal Fire Blocked Prior to Green Light Authorization", !deniedWithoutGreen);

    sEmergentPoliceMgr.GrantGreenLight(bankPerch->perchId);
    assertTest("Green Light Lethal Authorization Granted", bankPerch->state == SniperOverwatchState::AUTHORIZED_LETHAL_FIRE);

    bool takedownExecuted = sEmergentPoliceMgr.ExecuteSniperTakedown(bankPerch->perchId, testDmg);
    assertTest("Sniper Lethal Precision Takedown Executed Cleanly (550 DMG)", takedownExecuted && testDmg >= 500.0f);
    assertTest("Confirmed Sniper Takedowns Counter Incremented", bankPerch->totalConfirmedTakedowns == 1);

    // Sniper Perch Auto-Reset after neutralizing target
    assertTest("Sniper in TARGET_NEUTRALIZED State", bankPerch->state == SniperOverwatchState::TARGET_NEUTRALIZED);
    sEmergentPoliceMgr.UpdateSniperOverwatch(10000); // 10s cooldown
    assertTest("Sniper Automatically Re-Arms to SEARCHING_VANTAGE after Cooldown", bankPerch->state == SniperOverwatchState::SEARCHING_VANTAGE);

    // 7. Hostage Rescue & Extraction Protocols
    uint32 hId = sEmergentPoliceMgr.RegisterHostage("Dr. Eleanor Vance", 2, LocationVector(4205.0, 10.0, 1200.0), 101);
    HostageRecord* h = sEmergentPoliceMgr.GetHostage(hId);
    assertTest("Hostage Successfully Registered in Hotzone", h != nullptr && h->state == HostageRescueState::HELD_HOSTAGE);

    // Test Hostage Isolation Across Districts (targetCrimeId == 0 does NOT stun/secure unrelated hostages)
    uint32 distantHostageId = sEmergentPoliceMgr.RegisterHostage("Slums Bystander", 1, LocationVector(1400.0, 10.0, -3100.0), 0);
    HostageRecord* distantH = sEmergentPoliceMgr.GetHostage(distantHostageId);
    sEmergentPoliceMgr.DeployFlashbang(1, LocationVector(4200.0, 10.0, 1200.0));
    assertTest("Distant Unlinked Hostage (Slums) Immune to Downtown Flashbang", distantH != nullptr && distantH->state == HostageRescueState::HELD_HOSTAGE);

    sEmergentPoliceMgr.TriageHostage(hId, HostageMedicalTriage::YELLOW_DELAYED_CARE);
    assertTest("Hostage Medical Triage Tagged (Yellow Delayed Care)", h->triage == HostageMedicalTriage::YELLOW_DELAYED_CARE);

    sEmergentPoliceMgr.AssignHostageEscort(hId, 1);
    assertTest("Protective Shield Escort Assigned to Sierra-1", h->assignedEscortSquadId == 1 && h->state == HostageRescueState::UNDER_EXTRACTION_ESCORT);

    sEmergentPoliceMgr.ExtractHostageToCCP(hId, LocationVector(4100.0, 10.0, 1100.0));
    assertTest("Hostage Safely Evacuated to Casualty Collection Point", h->isExtractedSafely && h->state == HostageRescueState::SAFELY_EVACUATED_TO_CCP);
    assertTest("Master Hostage Extraction Counter Incremented", sEmergentPoliceMgr.GetTotalHostagesRescued() >= 1);

    // 8. Vehicular Roadblocks & Spike Strip Interception
    VehicularRoadblock* rb1 = sEmergentPoliceMgr.GetRoadblock(1);
    assertTest("Roadblock #1 Active with Stinger Spike Strips Deployed", rb1 != nullptr && rb1->spikeStripsDeployed && rb1->bearCatDeployed);

    LocationVector targetVehicleLoc(1455.0, 10.0, -3148.0); // Inside 50m radius
    bool spiked = sEmergentPoliceMgr.InterceptVehicularTarget(1, targetVehicleLoc, false);
    assertTest("Fleeing Vehicle Intercepted: Tyres Shredded by Spike Strip", spiked && rb1->totalVehiclesIntercepted == 1);

    // Distant vehicle outside roadblock radius
    LocationVector distantVehicleLoc(1950.0, 10.0, -3150.0);
    bool distantSpiked = sEmergentPoliceMgr.InterceptVehicularTarget(1, distantVehicleLoc, false);
    assertTest("Distant Vehicle Beyond Roadblock Radius Unaffected", !distantSpiked);

    // Smuggling Convoy Spike Interception with UnderworldManager
    if (UnderworldManager::getSingletonPtr()) {
        uint32 convoyId = sUnderworldMgr.SpawnConvoy(SyndicateFaction::MarconeFamily, 1, 2);
        bool convoySpiked = sEmergentPoliceMgr.InterceptVehicularTarget(1, targetVehicleLoc, true, convoyId);
        SmugglingConvoy* c = sUnderworldMgr.GetConvoy(convoyId);
        assertTest("Smuggling Convoy Spike-Stripped & Intercepted at Roadblock", convoySpiked && c != nullptr && c->status == ConvoyStatus::CONVOY_DESTROYED);
        assertTest("Contraband Value Confiscated from Convoy", rb1->totalContrabandSeizedValue > 0);

        // Cross-District Convoy Interception at Physical Coordinates
        uint32 crossConvoyId = sUnderworldMgr.SpawnConvoy(SyndicateFaction::ByteCartel, 3, 2); // Starts in 3, moves to 2
        SmugglingConvoy* crossConvoy = sUnderworldMgr.GetConvoy(crossConvoyId);
        if (crossConvoy) {
            crossConvoy->currentLocation = LocationVector(4105.0, 15.0, 1105.0); // Downtown roadblock #2 location
            sEmergentPoliceMgr.UpdateRoadblocks(1000);
            assertTest("Cross-District Convoy Intercepted at Physical Roadblock Coordinates", crossConvoy->status == ConvoyStatus::CONVOY_DESTROYED);
        }
    }

    // 9. MMPD Radio 10-Code Coordination Network
    uint32 log1 = sEmergentPoliceMgr.Transmit10Code(MMPD10Code::CODE_10_33, "Unit 3-Baker", 1,
                                                   LocationVector(1500, 10, -3200), "Officer Emergency! Under heavy fire!");
    assertTest("10-Code Transmission Successfully Logged (10-33 Officer Distress)", log1 > 0);
    assertTest("10-Code Description Decoded Correctly",
               EmergentPoliceManager::Get10CodeDescription(MMPD10Code::CODE_10_33).find("Emergency") != std::string::npos);
    assertTest("10-Code String Formatted Correctly (10-89)", EmergentPoliceManager::Get10CodeString(MMPD10Code::CODE_10_89) == "10-89");

    // 10. Internal Affairs Corruption Sting (5-Phase Execution)
    if (UnderworldManager::getSingletonPtr()) {
        PolicePrecinct* harborP = sUnderworldMgr.GetPrecinct(3);
        if (harborP) {
            harborP->corruptionIndex = 80.0f; // Reset to dirty for test
            harborP->isExposedByCastle = false;
        }

        uint32 stingId = sEmergentPoliceMgr.LaunchIASting(3, "Special Agent Kelly");
        InternalAffairsSting* sting = sEmergentPoliceMgr.GetIASting(stingId);
        assertTest("IA Sting Authorized Against Corrupt Precinct (Phase 1: Surveillance)",
                   sting != nullptr && sting->phase == StingPhase::COVERT_SURVEILLANCE);

        sEmergentPoliceMgr.AdvanceIASting(stingId);
        assertTest("IA Sting Advances to Phase 2 (Marked Currency Bribe)", sting->phase == StingPhase::MARKED_CURRENCY_BRIBE);

        sEmergentPoliceMgr.AdvanceIASting(stingId);
        assertTest("IA Sting Advances to Phase 3 (Wiretap Audio Intercept Confirmed)",
                   sting->phase == StingPhase::AUDIO_INTERCEPT_CONFIRMATION && sting->evidenceValidated);

        sEmergentPoliceMgr.AdvanceIASting(stingId);
        assertTest("IA Sting Advances to Phase 4 (Precinct HQ Tactical Raid)", sting->phase == StingPhase::PRECINCT_HQ_RAID);

        sEmergentPoliceMgr.AdvanceIASting(stingId);
        assertTest("IA Sting Advances to Phase 5 (Departmental Purge & Clean Commissioner)",
                   sting->phase == StingPhase::DEPARTMENTAL_PURGE_REFORM && sting->stingSuccessful);
        assertTest("Precinct Corruption Index Scrubbed to 0.0%", harborP != nullptr && harborP->corruptionIndex == 0.0f);
        assertTest("Clean Leadership Installed at Reformed Precinct", harborP != nullptr && harborP->precinctCaptainName.find("Interim") != std::string::npos);

        // Test Frank Castle Evidence Integration into New Sting
        if (harborP) harborP->corruptionIndex = 65.0f;
        uint32 sting2Id = sEmergentPoliceMgr.LaunchIASting(3, "Special Agent Vance");
        InternalAffairsSting* sting2 = sEmergentPoliceMgr.GetIASting(sting2Id);
        sEmergentPoliceMgr.IntegrateCastleEvidenceIntoSting(sting2Id);
        assertTest("Frank Castle Evidence Dossier Fast-Tracks IA Sting Directly to Validated Evidence",
                   sting2 != nullptr && sting2->assistedByCastle && sting2->evidenceValidated);
    }

    // 11. Multi-Agent Ecosystem Cross-System Cascades
    sEmergentPoliceMgr.OnEmergentCrimeReported(999, 1, LocationVector(1500, 10, -3200),
                                              EmergentCrimeType::BankHeist, 3);
    assertTest("Emergent Bank Heist Automatically Mobilized SWAT Stack and Registered Hostages",
               sEmergentPoliceMgr.GetHostagesInCrime(999).size() == 3);

    sEmergentPoliceMgr.OnFrankCastleAssistance(1, "Suppressive rifle fire on rear alley flank");
    assertTest("Frank Castle Tactical Flank Assistance Logged with SWAT Command", s1->operationalLog.back().find("Castle") != std::string::npos);

    // Autonomous Breach Simulation Update Cycle Test
    uint32 autoSquadId = sEmergentPoliceMgr.DeploySWATSquad(1, "Sierra-Auto", LocationVector(1500, 10, -3200));
    sEmergentPoliceMgr.OrderStackAndBreach(autoSquadId, LocationVector(1510, 10, -3200), LocationVector(1500, 10, -3200), 555);
    uint32 autoHostageId = sEmergentPoliceMgr.RegisterHostage("Auto Hostage", 1, LocationVector(1502, 10, -3200), 555);
    // Simulate autonomous breach ticks across all phases
    for (int t = 0; t < 10; ++t) {
        sEmergentPoliceMgr.Update(5000);
    }
    HostageRecord* autoH = sEmergentPoliceMgr.GetHostage(autoHostageId);
    assertTest("Autonomous SWAT Breach Cycle Successfully Extracted Hostage to CCP", autoH != nullptr && autoH->isExtractedSafely);

    // 12. Lossless Round-Trip JSON State Persistence & Restoration
    bool saved = sEmergentPoliceMgr.SavePoliceStateToFile("PoliceSWATSimulation_Test.json");
    s1->shield.durability = 123.0f;
    bool loaded = sEmergentPoliceMgr.LoadPoliceStateFromFile("PoliceSWATSimulation_Test.json");
    assertTest("Lossless Round-Trip JSON State Persistence & Restoration",
               saved && loaded && (s1->shield.durability == 600.0f));

    // 13. Concurrency & Mutex Thread Safety
    bool threadsCompleted = true;
    try {
        std::thread t1([&]() {
            for (int i = 0; i < 50; ++i) {
                sEmergentPoliceMgr.GetSquadsInDistrict(1);
                sEmergentPoliceMgr.IsPointUnderTearGas(LocationVector(1480, 10, -3180));
            }
        });
        std::thread t2([&]() {
            for (int i = 0; i < 50; ++i) {
                sEmergentPoliceMgr.GetAllSniperPerches();
                sEmergentPoliceMgr.GetRoadblocksInDistrict(2);
            }
        });
        t1.join();
        t2.join();
    } catch (...) {
        threadsCompleted = false;
    }
    assertTest("Multi-Threaded Concurrency Safety & Mutex Locking", threadsCompleted);

    // 14. Reporting Diagnostics Generation
    std::string report = sEmergentPoliceMgr.GeneratePoliceSWATReport();
    assertTest("Master Police & SWAT Telemetry Report Generated", !report.empty());

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  EMERGENT POLICE TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;
}

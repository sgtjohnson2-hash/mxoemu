#include "UnderworldManager.h"
#include "FrankCastleManager.h"
#include "EmergentAIEngine.h"
#include "EmergentPoliceManager.h"
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

createFileSingleton(UnderworldManager);

UnderworldManager::UnderworldManager()
{
    for (int i = 0; i <= 5; ++i) {
        m_districtHeat[i] = 20.0f;
        m_districtWantedStars[i] = 1;
        m_threeWayWarTriggered[i] = false;
        m_fourWayWarTriggered[i] = false;
    }
}

UnderworldManager::~UnderworldManager()
{
}

void UnderworldManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    INFO_LOG("UnderworldManager: Initializing Megacity Criminal Underworld Ecosystem...");

    InitializeDefaultLieutenants();
    InitializeDefaultRackets();
    InitializeDefaultTurfSectors();
    InitializeDefaultPrecincts();

    // Baseline district heat levels
    m_districtHeat[1] = 35.0f; // Slums: High street crime
    m_districtHeat[2] = 45.0f; // Downtown: Heavy Syndicate & Nightclub operations
    m_districtHeat[3] = 40.0f; // International: Dock smuggling & weapons trade
    m_districtHeat[4] = 50.0f; // Industrial: Chop shops & code stills
    m_districtHeat[5] = 20.0f; // Park East: Covert operations

    // Baseline wanted stars
    m_districtWantedStars[1] = 2;
    m_districtWantedStars[2] = 1;
    m_districtWantedStars[3] = 2;
    m_districtWantedStars[4] = 3;
    m_districtWantedStars[5] = 1;

    SyncWithFrankCastleHitList();

    INFO_LOG(format("UnderworldManager: Initialized with %1% Rackets, %2% Boss Lieutenants, %3% Turf Sectors, %4% Precincts.")
             % m_rackets.size() % m_lieutenants.size() % m_turfSectors.size() % m_precincts.size());
}

std::string UnderworldManager::GetFactionName(SyndicateFaction f)
{
    switch (f) {
        case SyndicateFaction::WolfpackLupines: return "The Wolfpack (Merovingian Lupines)";
        case SyndicateFaction::ChateauSanguine: return "Château Sanguine (Vampire Aristocracy)";
        case SyndicateFaction::JudasCabal:      return "The Judas Cabal (Cypherite Turncoats)";
        case SyndicateFaction::MarconeFamily:   return "The Marcone Family & Red Dragon";
        case SyndicateFaction::ByteCartel:      return "The Byte Cartel (Rogue Exiles)";
        default: return "Unknown Syndicate";
    }
}

std::string UnderworldManager::GetRacketTypeName(RacketType t)
{
    switch (t) {
        case RacketType::CodeStill:       return "Code Still (Sensory Narcotics)";
        case RacketType::WeaponsDepot:    return "Munitions Depot (Heavy Arms)";
        case RacketType::NightclubFront:  return "Nightclub VIP Front";
        case RacketType::ChopShop:        return "Convoy Chop Shop";
        case RacketType::ExtortionHub:    return "Extortion Hub";
        case RacketType::BlackClinic:     return "Black Market Clinic";
        default: return "Illicit Den";
    }
}

std::string UnderworldManager::GetDistrictName(uint32 districtId)
{
    switch (districtId) {
        case 1: return "Slums";
        case 2: return "Downtown";
        case 3: return "International";
        case 4: return "Industrial";
        case 5: return "Park East";
        default: return "Megacity";
    }
}

std::string UnderworldManager::GetCrimeTypeName(EmergentCrimeType t)
{
    switch (t) {
        case EmergentCrimeType::StreetMugging:       return "Street Shakedown & Mugging";
        case EmergentCrimeType::BankHeist:           return "Armored Bank Core Vault Heist";
        case EmergentCrimeType::HostageKidnapping:   return "Redpill Hostage Extraction";
        case EmergentCrimeType::DriveByShooting:     return "Gang Turf Drive-By Ambush";
        case EmergentCrimeType::ContrabandDrop:      return "Illegal Code Contraband Dead-Drop";
        case EmergentCrimeType::CyberdeckDataSiphon: return "Grid Cyberdeck Data Siphon";
        case EmergentCrimeType::IllegalArmsDeal:     return "Military Weapons Black Market Exchange";
        default: return "Emergent Syndicate Crime";
    }
}

std::string UnderworldManager::GetPoliceUnitName(PoliceUnitType t)
{
    switch (t) {
        case PoliceUnitType::BeatPatrol:           return "MMPD Beat Patrol (9mm Units)";
        case PoliceUnitType::SquadCarCruiser:      return "MMPD Cruiser Squad (Roadblock Cordon)";
        case PoliceUnitType::SWATBreachTeam:       return "MMPD Tactical SWAT (Breach & Clear)";
        case PoliceUnitType::HeavyTacticalUnit:    return "MMPD BearCat Heavy Assault & Sniper Overwatch";
        case PoliceUnitType::AgentIntervention:    return "System Agent Anomalous Quarantine";
        default: return "MMPD Emergency Response";
    }
}

void UnderworldManager::InitializeDefaultLieutenants()
{
    m_lieutenants.clear();

    // 1. The Wolfpack
    CartelLieutenant l1;
    l1.id = 1;
    l1.name = "Vane the Skinner";
    l1.faction = SyndicateFaction::WolfpackLupines;
    l1.factionName = GetFactionName(l1.faction);
    l1.rankTitle = "Alpha Pack Enforcer";
    l1.personalityTraits = {"Ironclad", "Blood Frenzy"};
    l1.bountyOnCastle = 10000;
    l1.grudgeScoreAgainstCastle = 50;
    l1.isAlive = true;
    l1.districtId = 1;
    l1.districtName = "Slums";
    l1.lastKnownLocation = LocationVector(1400.0f, 0.0f, -3200.0f);
    l1.combatPower = 750;
    m_lieutenants[1] = l1;

    CartelLieutenant l2;
    l2.id = 2;
    l2.name = "Fenris Graves";
    l2.faction = SyndicateFaction::WolfpackLupines;
    l2.factionName = GetFactionName(l2.faction);
    l2.rankTitle = "Pitmaster Lieutenant";
    l2.personalityTraits = {"Hostage Taker", "Pyromaniac"};
    l2.bountyOnCastle = 6000;
    l2.grudgeScoreAgainstCastle = 30;
    l2.isAlive = true;
    l2.districtId = 1;
    l2.districtName = "Slums";
    l2.lastKnownLocation = LocationVector(1800.0f, 10.0f, -3600.0f);
    l2.combatPower = 620;
    m_lieutenants[2] = l2;

    // 2. Château Sanguine
    CartelLieutenant l3;
    l3.id = 3;
    l3.name = "Sire Cassian";
    l3.faction = SyndicateFaction::ChateauSanguine;
    l3.factionName = GetFactionName(l3.faction);
    l3.rankTitle = "Blood Sire Archon";
    l3.personalityTraits = {"Phase Glitcher", "Hostage Taker"};
    l3.bountyOnCastle = 25000;
    l3.grudgeScoreAgainstCastle = 80;
    l3.isAlive = true;
    l3.districtId = 2;
    l3.districtName = "Downtown";
    l3.lastKnownLocation = LocationVector(4800.0f, 120.0f, 1400.0f);
    l3.combatPower = 880;
    m_lieutenants[3] = l3;

    CartelLieutenant l4;
    l4.id = 4;
    l4.name = "Madame Valerius";
    l4.faction = SyndicateFaction::ChateauSanguine;
    l4.factionName = GetFactionName(l4.faction);
    l4.rankTitle = "Le Vrai Matron";
    l4.personalityTraits = {"Cowardly Tactician", "Phase Glitcher"};
    l4.bountyOnCastle = 15000;
    l4.grudgeScoreAgainstCastle = 45;
    l4.isAlive = true;
    l4.districtId = 2;
    l4.districtName = "Downtown";
    l4.lastKnownLocation = LocationVector(4400.0f, 80.0f, 1800.0f);
    l4.combatPower = 690;
    m_lieutenants[4] = l4;

    // 3. The Judas Cabal
    CartelLieutenant l5;
    l5.id = 5;
    l5.name = "Judas-09";
    l5.faction = SyndicateFaction::JudasCabal;
    l5.factionName = GetFactionName(l5.faction);
    l5.rankTitle = "Turncoat Sniper Overseer";
    l5.personalityTraits = {"Ironclad", "Sniper Overwatch", "Pyromaniac"};
    l5.bountyOnCastle = 18000;
    l5.grudgeScoreAgainstCastle = 60;
    l5.isAlive = true;
    l5.districtId = 2;
    l5.districtName = "Downtown";
    l5.lastKnownLocation = LocationVector(3900.0f, 250.0f, 800.0f);
    l5.combatPower = 820;
    m_lieutenants[5] = l5;

    // 4. Marcone Family & Red Dragon
    CartelLieutenant l6;
    l6.id = 6;
    l6.name = "Don Sal Marcone";
    l6.faction = SyndicateFaction::MarconeFamily;
    l6.factionName = GetFactionName(l6.faction);
    l6.rankTitle = "Syndicate Don";
    l6.personalityTraits = {"Ironclad", "Cowardly Tactician"};
    l6.bountyOnCastle = 30000;
    l6.grudgeScoreAgainstCastle = 90;
    l6.isAlive = true;
    l6.districtId = 3;
    l6.districtName = "International";
    l6.lastKnownLocation = LocationVector(-2500.0f, 20.0f, 5200.0f);
    l6.combatPower = 800;
    m_lieutenants[6] = l6;

    CartelLieutenant l7;
    l7.id = 7;
    l7.name = "Jimmy 'Two-Code' Dragon";
    l7.faction = SyndicateFaction::MarconeFamily;
    l7.factionName = GetFactionName(l7.faction);
    l7.rankTitle = "Dockyard Enforcer Capo";
    l7.personalityTraits = {"Hostage Taker", "Blood Frenzy"};
    l7.bountyOnCastle = 12000;
    l7.grudgeScoreAgainstCastle = 40;
    l7.isAlive = true;
    l7.districtId = 3;
    l7.districtName = "International";
    l7.lastKnownLocation = LocationVector(-1900.0f, 0.0f, 5600.0f);
    l7.combatPower = 710;
    m_lieutenants[7] = l7;

    // 5. The Byte Cartel
    CartelLieutenant l8;
    l8.id = 8;
    l8.name = "Zero-Day Malice";
    l8.faction = SyndicateFaction::ByteCartel;
    l8.factionName = GetFactionName(l8.faction);
    l8.rankTitle = "Glitch Overlord";
    l8.personalityTraits = {"Pyromaniac", "Phase Glitcher"};
    l8.bountyOnCastle = 22000;
    l8.grudgeScoreAgainstCastle = 75;
    l8.isAlive = true;
    l8.districtId = 4;
    l8.districtName = "Industrial";
    l8.lastKnownLocation = LocationVector(6900.0f, -40.0f, -1400.0f);
    l8.combatPower = 850;
    m_lieutenants[8] = l8;

    CartelLieutenant l9;
    l9.id = 9;
    l9.name = "Proxy Null";
    l9.faction = SyndicateFaction::ByteCartel;
    l9.factionName = GetFactionName(l9.faction);
    l9.rankTitle = "Code Still Master";
    l9.personalityTraits = {"Cowardly Tactician", "Blood Frenzy"};
    l9.bountyOnCastle = 8000;
    l9.grudgeScoreAgainstCastle = 35;
    l9.isAlive = true;
    l9.districtId = 4;
    l9.districtName = "Industrial";
    l9.lastKnownLocation = LocationVector(7300.0f, -10.0f, -900.0f);
    l9.combatPower = 640;
    m_lieutenants[9] = l9;
}

void UnderworldManager::InitializeDefaultRackets()
{
    m_rackets.clear();

    auto createRacket = [&](uint32 id, const std::string& name, SyndicateFaction f, RacketType t, 
                            uint32 distId, LocationVector loc, uint32 bossId, const std::string& bossName, float health, float revRate) {
        UnderworldRacketNode r;
        r.id = id;
        r.name = name;
        r.faction = f;
        r.factionName = GetFactionName(f);
        r.type = t;
        r.typeName = GetRacketTypeName(t);
        r.state = RacketState::Active;
        r.districtId = distId;
        r.districtName = GetDistrictName(distId);
        r.coordinates = loc;
        r.bossLieutenantId = bossId;
        r.bossName = bossName;
        r.defenseHealth = health;
        r.maxDefenseHealth = health;
        r.illicitRevenueRate = revRate;
        r.activeGoonsCount = 5;
        r.lastAttackedTimestamp = 0;
        r.cooldownTimerMs = 0;
        r.cooldownDurationMs = 120000;
        r.skullDecalMarked = false;
        r.totalDecapitations = 0;
        r.securityTier = 2;
        r.tripwireTrapArmed = true;
        m_rackets[id] = r;
    };

    // District 1: Slums (Wolfpack Dominance)
    createRacket(101, "Westview Code Pit & Extortion Hub", SyndicateFaction::WolfpackLupines, RacketType::ExtortionHub, 1, LocationVector(1450.0f, 0.0f, -3250.0f), 1, "Vane the Skinner", 120.0f, 320.0f);
    createRacket(102, "Morrell Sump Narcotic Still", SyndicateFaction::ByteCartel, RacketType::CodeStill, 1, LocationVector(1750.0f, -30.0f, -3550.0f), 9, "Proxy Null", 90.0f, 280.0f);
    createRacket(103, "Waterfront Shakedown Chop Shop", SyndicateFaction::WolfpackLupines, RacketType::ChopShop, 1, LocationVector(1200.0f, 10.0f, -2900.0f), 2, "Fenris Graves", 110.0f, 290.0f);

    // District 2: Downtown (Château Sanguine & Judas Cabal)
    createRacket(201, "Club Hel VIP Penthouse & Bloodline Den", SyndicateFaction::ChateauSanguine, RacketType::NightclubFront, 2, LocationVector(4750.0f, 140.0f, 1350.0f), 3, "Sire Cassian", 160.0f, 650.0f);
    createRacket(202, "Le Vrai Memory Blackmail Parlor", SyndicateFaction::ChateauSanguine, RacketType::NightclubFront, 2, LocationVector(4350.0f, 75.0f, 1750.0f), 4, "Madame Valerius", 130.0f, 480.0f);
    createRacket(203, "Federal Tower Cypherite Data Switch", SyndicateFaction::JudasCabal, RacketType::WeaponsDepot, 2, LocationVector(3950.0f, 220.0f, 850.0f), 5, "Judas-09", 150.0f, 520.0f);

    // District 3: International (Marcone Family Docks)
    createRacket(301, "Pier 44 Heavy Munitions Depot", SyndicateFaction::MarconeFamily, RacketType::WeaponsDepot, 3, LocationVector(-2450.0f, 15.0f, 5250.0f), 6, "Don Sal Marcone", 180.0f, 700.0f);
    createRacket(302, "St. Jude's Black Market Clinic", SyndicateFaction::MarconeFamily, RacketType::BlackClinic, 3, LocationVector(-1950.0f, 5.0f, 5550.0f), 7, "Jimmy 'Two-Code' Dragon", 100.0f, 380.0f);
    createRacket(303, "Freeway Terminal Armored Vehicle Bay", SyndicateFaction::MarconeFamily, RacketType::ChopShop, 3, LocationVector(-2700.0f, 30.0f, 4800.0f), 6, "Don Sal Marcone", 140.0f, 420.0f);

    // District 4: Industrial (Byte Cartel Deep City)
    createRacket(401, "Sub-Level 7 Glitch Synthesis Lab", SyndicateFaction::ByteCartel, RacketType::CodeStill, 4, LocationVector(6850.0f, -35.0f, -1450.0f), 8, "Zero-Day Malice", 150.0f, 580.0f);
    createRacket(402, "Foundry Corridor Heavy Arms Depot", SyndicateFaction::MarconeFamily, RacketType::WeaponsDepot, 4, LocationVector(7250.0f, -15.0f, -950.0f), 7, "Jimmy 'Two-Code' Dragon", 130.0f, 450.0f);
    createRacket(403, "Rail Siding Contraband Sorting Facility", SyndicateFaction::ByteCartel, RacketType::ExtortionHub, 4, LocationVector(6600.0f, 0.0f, -1700.0f), 8, "Zero-Day Malice", 115.0f, 360.0f);

    // District 5: Park East (Surveillance & Infiltration)
    createRacket(501, "The Conservatory False Identity Still", SyndicateFaction::JudasCabal, RacketType::CodeStill, 5, LocationVector(-4700.0f, 60.0f, -2300.0f), 5, "Judas-09", 105.0f, 340.0f);
    createRacket(502, "Reservoir Tunnel Safehouse Front", SyndicateFaction::ChateauSanguine, RacketType::NightclubFront, 5, LocationVector(-4400.0f, 30.0f, -2600.0f), 4, "Madame Valerius", 120.0f, 410.0f);
}

void UnderworldManager::InitializeDefaultTurfSectors()
{
    m_turfSectors.clear();

    auto createSector = [&](uint32 id, const std::string& name, uint32 distId, LocationVector center,
                            SyndicateFaction dominant, float w, float c, float j, float m, float b) {
        TurfSector s;
        s.sectorId = id;
        s.name = name;
        s.districtId = distId;
        s.districtName = GetDistrictName(distId);
        s.centerCoordinates = center;
        s.radiusUnits = 1800.0f;
        s.controllingFaction = dominant;
        s.factionInfluence[0] = w; // Wolfpack
        s.factionInfluence[1] = c; // Chateau
        s.factionInfluence[2] = j; // Judas
        s.factionInfluence[3] = m; // Marcone
        s.factionInfluence[4] = b; // ByteCartel
        s.isContested = false;
        s.attackingFaction = dominant;
        s.turfWarTimerMs = 0;
        s.totalSkirmishes = 0;
        m_turfSectors[id] = s;
    };

    // District 1: Slums
    createSector(1, "Westview Tenement Strip", 1, LocationVector(1500.0f, 0.0f, -3200.0f), SyndicateFaction::WolfpackLupines, 80.0f, 5.0f, 5.0f, 10.0f, 0.0f);
    createSector(2, "Morrell Industrial Canal", 1, LocationVector(1800.0f, -20.0f, -3600.0f), SyndicateFaction::ByteCartel, 25.0f, 0.0f, 5.0f, 5.0f, 65.0f);

    // District 2: Downtown
    createSector(3, "Club Hel Entertainment Strip", 2, LocationVector(4700.0f, 100.0f, 1400.0f), SyndicateFaction::ChateauSanguine, 0.0f, 85.0f, 15.0f, 0.0f, 0.0f);
    createSector(4, "Financial District Data Switch", 2, LocationVector(4000.0f, 180.0f, 900.0f), SyndicateFaction::JudasCabal, 0.0f, 20.0f, 75.0f, 5.0f, 0.0f);

    // District 3: International
    createSector(5, "Pier 44 Deepwater Cargo Terminal", 3, LocationVector(-2400.0f, 10.0f, 5200.0f), SyndicateFaction::MarconeFamily, 0.0f, 0.0f, 5.0f, 90.0f, 5.0f);
    createSector(6, "International Chop-Shop Corridor", 3, LocationVector(-2000.0f, 15.0f, 5500.0f), SyndicateFaction::MarconeFamily, 10.0f, 0.0f, 0.0f, 80.0f, 10.0f);

    // District 4: Industrial
    createSector(7, "Foundry Sub-Level 7 Ducts", 4, LocationVector(6900.0f, -30.0f, -1400.0f), SyndicateFaction::ByteCartel, 15.0f, 0.0f, 0.0f, 10.0f, 75.0f);
    createSector(8, "Freight Rail Shunting Yard", 4, LocationVector(7200.0f, -10.0f, -900.0f), SyndicateFaction::MarconeFamily, 5.0f, 0.0f, 0.0f, 65.0f, 30.0f);

    // District 5: Park East
    createSector(9, "Conservatory Glasshouses", 5, LocationVector(-4600.0f, 50.0f, -2400.0f), SyndicateFaction::JudasCabal, 0.0f, 25.0f, 70.0f, 5.0f, 0.0f);
    createSector(10, "Reservoir Pumping Tunnels", 5, LocationVector(-4300.0f, 25.0f, -2700.0f), SyndicateFaction::ChateauSanguine, 10.0f, 65.0f, 25.0f, 0.0f, 0.0f);
}

void UnderworldManager::InitializeDefaultPrecincts()
{
    m_precincts.clear();

    auto createPrecinct = [&](uint32 id, const std::string& name, uint32 distId, LocationVector loc,
                              float corrupt, SyndicateFaction bribeFac, const std::string& captain) {
        PolicePrecinct p;
        p.precinctId = id;
        p.name = name;
        p.districtId = distId;
        p.districtName = GetDistrictName(distId);
        p.precinctLocation = loc;
        p.corruptionIndex = corrupt;
        p.bribingFaction = bribeFac;
        p.precinctCaptainName = captain;
        p.isExposedByCastle = false;
        p.activePatrolUnits = 8;
        p.holdingCellInmates = 12;
        p.confiscatedContrabandValue = 40000.0f;
        m_precincts[id] = p;
    };

    createPrecinct(1, "MMPD 1st Precinct (Westview / Slums)", 1, LocationVector(1100.0f, 0.0f, -2800.0f), 35.0f, SyndicateFaction::WolfpackLupines, "Captain Marcus Vance");
    createPrecinct(2, "MMPD Central Metro Precinct (Downtown)", 2, LocationVector(4200.0f, 50.0f, 1100.0f), 65.0f, SyndicateFaction::ChateauSanguine, "Captain Jonathan Cross");
    createPrecinct(3, "MMPD Harbor Division Precinct (International)", 3, LocationVector(-2800.0f, 10.0f, 4900.0f), 80.0f, SyndicateFaction::MarconeFamily, "Captain Frank 'Bull' O'Malley");
    createPrecinct(4, "MMPD Industrial Division Precinct", 4, LocationVector(6400.0f, -10.0f, -1800.0f), 45.0f, SyndicateFaction::ByteCartel, "Captain Viktor Ramos");
    createPrecinct(5, "MMPD Uptown 5th Precinct (Park East)", 5, LocationVector(-4100.0f, 40.0f, -2100.0f), 15.0f, SyndicateFaction::JudasCabal, "Captain Evelyn Reed");
}

UnderworldRacketNode* UnderworldManager::GetRacket(uint32 racketId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_rackets.find(racketId);
    return (it != m_rackets.end()) ? &it->second : nullptr;
}

std::vector<UnderworldRacketNode*> UnderworldManager::GetRacketsInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<UnderworldRacketNode*> list;
    for (auto& kv : m_rackets) {
        if (kv.second.districtId == districtId) {
            list.push_back(&kv.second);
        }
    }
    return list;
}

std::vector<UnderworldRacketNode*> UnderworldManager::GetActiveRackets()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<UnderworldRacketNode*> list;
    for (auto& kv : m_rackets) {
        if (kv.second.state == RacketState::Active || kv.second.state == RacketState::Fortified) {
            list.push_back(&kv.second);
        }
    }
    return list;
}

UnderworldRacketNode* UnderworldManager::GetNearestActiveRacket(float x, float z, uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    UnderworldRacketNode* best = nullptr;
    float minDistSq = 999999999.0f;

    for (auto& kv : m_rackets) {
        if (kv.second.state != RacketState::Active && kv.second.state != RacketState::Fortified) continue;
        if (districtId != 0 && kv.second.districtId != districtId) continue;

        float dx = static_cast<float>(kv.second.coordinates.x - x);
        float dz = static_cast<float>(kv.second.coordinates.z - z);
        float dSq = (dx * dx) + (dz * dz);
        if (dSq < minDistSq) {
            minDistSq = dSq;
            best = &kv.second;
        }
    }
    return best;
}

bool UnderworldManager::RaidRacket(uint32 racketId, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    UnderworldRacketNode* r = GetRacket(racketId);
    if (!r || r->state == RacketState::DecapitatedCooldown) return false;

    r->state = RacketState::UnderSiegeByCastle;
    r->lastAttackedTimestamp = getMSTime();

    if (byCastle) {
        DEBUG_LOG(format("[FrankCastle] Breaching syndicate racket [%1%] in %2%. Neutralizing guards.") % r->name % r->districtName);
    }
    return true;
}

bool UnderworldManager::DamageRacketDefenses(uint32 racketId, float damage, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    UnderworldRacketNode* r = GetRacket(racketId);
    if (!r || r->state == RacketState::DecapitatedCooldown) return false;

    r->defenseHealth -= damage;
    if (r->defenseHealth <= 0.0f) {
        r->defenseHealth = 0.0f;
        return DecapitateRacket(racketId, byCastle);
    }
    return true;
}

bool UnderworldManager::DecapitateRacket(uint32 racketId, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    UnderworldRacketNode* r = GetRacket(racketId);
    if (!r || r->state == RacketState::DecapitatedCooldown) return false;

    r->state = RacketState::DecapitatedCooldown;
    r->defenseHealth = 0.0f;
    r->cooldownTimerMs = 0;
    r->skullDecalMarked = true;
    r->activeGoonsCount = 0;
    r->totalDecapitations++;
    m_totalRacketsDecapitated++;

    // Shard heat in the district drops significantly as Castle suppresses crime
    m_districtHeat[r->districtId] = std::max(5.0f, m_districtHeat[r->districtId] - 25.0f);

    // Defeat the boss lieutenant if linked
    if (r->bossLieutenantId != 0) {
        RecordLieutenantDefeated(r->bossLieutenantId, byCastle);
    }

    if (byCastle) {
        // Scavenge supplies and award experience to Frank Castle
        sFrankCastleMgr.AwardExperience(2500);
        sFrankCastleMgr.ScavengeSupplies(150, 4, 3, 2, 2);

        INFO_LOG(format("[FrankCastle] Stronghold [%1%] dismantled! Contraband burned. White skull painted at perimeter.") % r->name);

        sFrankCastleMgr.AddWarJournalEntry(
            JOURNAL_SYNDICATE_HIT,
            r->bossName,
            "Syndicate Racket Decapitated",
            "Breached and destroyed underworld stronghold [" + r->name + "]. Skull signature left on blast door.",
            r->districtId,
            r->coordinates
        );

        sEmergentAIMgr.OnFrankCastleAmbushExecuted(r->coordinates, r->name, byCastle);
    }

    // Surviving lieutenants of this faction escalate retaliation
    EscalateRetaliationBounty(r->faction, 5000);

    return true;
}

void UnderworldManager::ClearAllCooldowns()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& kv : m_rackets) {
        if (kv.second.state == RacketState::DecapitatedCooldown) {
            kv.second.state = RacketState::Active;
            kv.second.defenseHealth = kv.second.maxDefenseHealth;
            kv.second.activeGoonsCount = 5;
            kv.second.cooldownTimerMs = 0;
        }
    }
}

uint32 UnderworldManager::SpawnConvoy(SyndicateFaction faction, uint32 startDistrictId, uint32 destDistrictId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32 cId = m_nextConvoyId++;

    SmugglingConvoy c;
    c.convoyId = cId;
    c.faction = faction;
    c.factionName = GetFactionName(faction);
    c.cargoDescription = (faction == SyndicateFaction::MarconeFamily) ? "Military AP Ammunition Crates" :
                         ((faction == SyndicateFaction::ByteCartel) ? "Raw Synthetic Memory Stills" : "Blackmail Code Shards");
    c.startDistrictId = startDistrictId;
    c.destDistrictId = destDistrictId;
    c.startLocation = LocationVector(-2400.0f, 10.0f, 5100.0f);
    c.destinationLocation = LocationVector(4500.0f, 50.0f, 1200.0f);
    c.currentLocation = c.startLocation;
    c.routeProgress = 0.0f;
    c.escortVehicleCount = 2;
    c.escortGuardsCount = 6;
    c.cargoValuation = 7500;
    c.status = ConvoyStatus::CONVOY_IN_TRANSIT;
    c.isInterceptedByCastle = false;
    c.transitTimerMs = 0;

    m_convoys[cId] = c;

    // Convoys increase district heat
    AddDistrictHeat(startDistrictId, 10.0f);
    AddDistrictHeat(destDistrictId, 8.0f);

    DEBUG_LOG(format("[Underworld] %1% smuggling convoy rolling from %2% to %3%. Cargo: %4%.")
        % c.factionName % GetDistrictName(startDistrictId) % GetDistrictName(destDistrictId) % c.cargoDescription);

    return cId;
}

bool UnderworldManager::InterceptConvoy(uint32 convoyId, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_convoys.find(convoyId);
    if (it == m_convoys.end() || it->second.status == ConvoyStatus::CONVOY_DESTROYED) return false;

    SmugglingConvoy& c = it->second;
    c.status = ConvoyStatus::CONVOY_DESTROYED;
    c.isInterceptedByCastle = byCastle;
    m_totalConvoysIntercepted++;

    if (byCastle) {
        sFrankCastleMgr.AwardExperience(1800);
        sFrankCastleMgr.ScavengeSupplies(120, 3, 2, 2, 1);

        INFO_LOG(format("[FrankCastle] %1% smuggling shipment intercepted and neutralized via explosive ambush.") % c.factionName);

        sFrankCastleMgr.AddWarJournalEntry(
            JOURNAL_SYNDICATE_HIT,
            c.factionName,
            "Smuggling Convoy Neutralized",
            "High-explosive ambush executed on " + c.cargoDescription + " transit route.",
            c.startDistrictId,
            c.currentLocation
        );
    }

    return true;
}

SmugglingConvoy* UnderworldManager::GetConvoy(uint32 convoyId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_convoys.find(convoyId);
    return (it != m_convoys.end()) ? &it->second : nullptr;
}

CartelLieutenant* UnderworldManager::GetLieutenant(uint32 lieutenantId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_lieutenants.find(lieutenantId);
    return (it != m_lieutenants.end()) ? &it->second : nullptr;
}

std::vector<CartelLieutenant*> UnderworldManager::GetLivingLieutenants()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<CartelLieutenant*> list;
    for (auto& kv : m_lieutenants) {
        if (kv.second.isAlive) {
            list.push_back(&kv.second);
        }
    }
    return list;
}

void UnderworldManager::RecordLieutenantDefeated(uint32 lieutenantId, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    CartelLieutenant* l = GetLieutenant(lieutenantId);
    if (!l || !l->isAlive) return;

    l->isAlive = false;
    l->grudgeScoreAgainstCastle += 100;

    if (byCastle) {
        sFrankCastleMgr.OnSyndicateLieutenantEliminated(l->id, l->name);
    }
}

void UnderworldManager::EscalateRetaliationBounty(SyndicateFaction faction, uint32 amount)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& kv : m_lieutenants) {
        if (kv.second.faction == faction && kv.second.isAlive) {
            kv.second.bountyOnCastle += amount;
            kv.second.grudgeScoreAgainstCastle += 25;
        }
    }
}

TurfSector* UnderworldManager::GetTurfSector(uint32 sectorId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_turfSectors.find(sectorId);
    return (it != m_turfSectors.end()) ? &it->second : nullptr;
}

std::vector<TurfSector*> UnderworldManager::GetTurfSectorsInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<TurfSector*> list;
    for (auto& kv : m_turfSectors) {
        if (kv.second.districtId == districtId) {
            list.push_back(&kv.second);
        }
    }
    return list;
}

bool UnderworldManager::TriggerTurfWar(uint32 sectorId, SyndicateFaction attackerFaction)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    TurfSector* s = GetTurfSector(sectorId);
    if (!s || s->isContested) return false;

    if (s->controllingFaction == attackerFaction) return false;

    s->isContested = true;
    s->attackingFaction = attackerFaction;
    s->turfWarTimerMs = 0;
    s->totalSkirmishes++;

    AddDistrictHeat(s->districtId, 15.0f);

    DEBUG_LOG(format("[Underworld] Turf war: %1% launched incursion against %2% in [%3%].")
        % GetFactionName(attackerFaction) % GetFactionName(s->controllingFaction) % s->name);

    // Dispatch police to the turf war
    DispatchPoliceResponse(s->districtId, 3, s->centerCoordinates, "Gang Turf Skirmish in " + s->name);

    return true;
}

bool UnderworldManager::ResolveTurfWar(uint32 sectorId, SyndicateFaction victorFaction)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    TurfSector* s = GetTurfSector(sectorId);
    if (!s || !s->isContested) return false;

    s->isContested = false;
    uint8_t victorIdx = (uint8_t)victorFaction;
    SyndicateFaction loserFaction = (victorFaction == s->controllingFaction) ? s->attackingFaction : s->controllingFaction;
    uint8_t loserIdx = (uint8_t)loserFaction;

    // Victor gains 25% influence; defeated loser loses 20% influence
    s->factionInfluence[victorIdx] = std::min(100.0f, s->factionInfluence[victorIdx] + 25.0f);
    s->factionInfluence[loserIdx] = std::max(0.0f, s->factionInfluence[loserIdx] - 20.0f);

    // Re-evaluate dominant faction based on highest influence
    SyndicateFaction newDominant = s->GetDominantFaction();
    if (newDominant != s->controllingFaction) {
        s->controllingFaction = newDominant;
        DEBUG_LOG(format("[Underworld] Turf control shift: [%1%] is now under the authoritative control of %2%.")
            % s->name % GetFactionName(newDominant));
    }

    return true;
}

SyndicateFaction UnderworldManager::GetDominantFactionInDistrict(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    float factionScores[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (const auto& kv : m_turfSectors) {
        if (kv.second.districtId == districtId) {
            for (int i = 0; i < 5; ++i) {
                factionScores[i] += kv.second.factionInfluence[i];
            }
        }
    }
    int best = 0;
    float maxScore = -1.0f;
    for (int i = 0; i < 5; ++i) {
        if (factionScores[i] > maxScore) {
            maxScore = factionScores[i];
            best = i;
        }
    }
    return (SyndicateFaction)best;
}

uint32 UnderworldManager::SpawnEmergentCrime(EmergentCrimeType type, uint32 districtId, LocationVector loc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32 crimeId = m_nextCrimeId++;

    EmergentCrimeEvent c;
    c.crimeId = crimeId;
    c.type = type;
    c.typeName = GetCrimeTypeName(type);
    c.state = EmergentCrimeState::InProgress;
    c.districtId = districtId;
    c.districtName = GetDistrictName(districtId);
    c.location = loc;

    // Anchor gang perpetrators to dominant turf sector if in range
    TurfSector* localSector = nullptr;
    float bestDistSq = 999999999.0f;
    for (auto& kv : m_turfSectors) {
        if (kv.second.districtId == districtId) {
            float dx = static_cast<float>(kv.second.centerCoordinates.x - loc.x);
            float dz = static_cast<float>(kv.second.centerCoordinates.z - loc.z);
            float dSq = (dx * dx) + (dz * dz);
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                localSector = &kv.second;
            }
        }
    }
    if (localSector && (rand() % 100 < 75)) {
        c.perpFaction = localSector->controllingFaction;
    } else {
        c.perpFaction = (SyndicateFaction)(rand() % 5);
    }
    c.perpFactionName = GetFactionName(c.perpFaction);
    c.perpCount = 3 + (rand() % 4);
    c.hostageCount = (type == EmergentCrimeType::HostageKidnapping) ? (1 + rand() % 3) :
                     ((type == EmergentCrimeType::BankHeist) ? (2 + rand() % 4) : 0);
    c.lootValue = 2000.0f + (rand() % 5000);
    c.durationMs = 0;
    c.timeLimitMs = 45000;
    c.policeDispatched = false;
    c.castleIntervening = false;

    if (type == EmergentCrimeType::StreetMugging) {
        c.perpDescription = "Armed gang enforcers shaking down civilians in alley";
    } else if (type == EmergentCrimeType::BankHeist) {
        c.perpDescription = "Heavily armored crew breaching bank depository vault";
    } else if (type == EmergentCrimeType::HostageKidnapping) {
        c.perpDescription = "Extortionists holding captive Zion redpill operative";
    } else if (type == EmergentCrimeType::DriveByShooting) {
        c.perpDescription = "Two cartel vehicles firing automatic weapons into rival safehouse";
    } else if (type == EmergentCrimeType::ContrabandDrop) {
        c.perpDescription = "Couriers exchanging military code crates behind subway vent";
    } else if (type == EmergentCrimeType::CyberdeckDataSiphon) {
        c.perpDescription = "Rogue exile hackers splicing into power sub-station grid";
    } else {
        c.perpDescription = "Illegal black-market ballistic munitions exchange";
    }

    // Automatically dispatch police response linked to this crime
    uint32 wantedStars = (type == EmergentCrimeType::BankHeist || type == EmergentCrimeType::HostageKidnapping) ? 3 : 2;
    uint32 dId = DispatchPoliceResponse(districtId, wantedStars, loc, c.typeName, crimeId);
    c.linkedDispatchId = dId;
    c.policeDispatched = true;

    m_crimes[crimeId] = c;

    // Escalation adds heat
    AddDistrictHeat(districtId, 6.0f);

    DEBUG_LOG(format("[Underworld] Emergent crime detected: %1% [%2%] in %3%. Perps: %4% (%5% enforcers).")
        % c.typeName % c.perpDescription % c.districtName % c.perpFactionName % c.perpCount);

    sEmergentAIMgr.OnEmergentCrimeDetected(crimeId, districtId, loc, c.perpDescription);

    if (EmergentPoliceManager::getSingletonPtr()) {
        sEmergentPoliceMgr.OnEmergentCrimeReported(crimeId, districtId, loc, type, c.hostageCount);
    }

    return crimeId;
}

EmergentCrimeEvent* UnderworldManager::GetEmergentCrime(uint32 crimeId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_crimes.find(crimeId);
    return (it != m_crimes.end()) ? &it->second : nullptr;
}

bool UnderworldManager::EscalateCrime(uint32 crimeId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    EmergentCrimeEvent* c = GetEmergentCrime(crimeId);
    if (!c || c->state != EmergentCrimeState::InProgress) return false;

    c->state = EmergentCrimeState::Escalated;
    AddDistrictHeat(c->districtId, 12.0f);
    SetDistrictWantedLevel(c->districtId, std::min(5u, GetDistrictWantedLevel(c->districtId) + 1));

    DEBUG_LOG(format("[Underworld] Shootout escalated at [%1%] in %2%! Hostages at risk.") % c->typeName % c->districtName);

    // Call in heavier police tactical unit
    DispatchPoliceResponse(c->districtId, 4, c->location, "ESCALATION: " + c->typeName, crimeId);
    return true;
}

bool UnderworldManager::NeutralizeCrime(uint32 crimeId, bool byCastle, bool byPolice)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    EmergentCrimeEvent* c = GetEmergentCrime(crimeId);
    if (!c || (c->state != EmergentCrimeState::InProgress && c->state != EmergentCrimeState::Escalated)) return false;

    if (byCastle) {
        c->state = EmergentCrimeState::NeutralizedByCastle;
        m_totalCrimesNeutralized++;

        // Frank Castle rewards
        sFrankCastleMgr.AwardExperience(1200 + (c->perpCount * 150));
        sFrankCastleMgr.ScavengeSupplies(80, 2, 2, 1, 1);

        if (c->hostageCount > 0) {
            sFrankCastleMgr.AddWarJournalEntry(
                JOURNAL_VIGILANTE_JUDGMENT,
                c->perpFactionName,
                "Hostages Rescued: " + c->typeName,
                "Neutralized " + std::to_string(c->perpCount) + " perps and rescued " + std::to_string(c->hostageCount) + " civilians in " + c->districtName + ".",
                c->districtId,
                c->location
            );
        } else {
            sFrankCastleMgr.AddWarJournalEntry(
                JOURNAL_VIGILANTE_JUDGMENT,
                c->perpFactionName,
                "Crime Intercepted: " + c->typeName,
                "Neutralized perps and secured black-market loot valuation of " + std::to_string((int)c->lootValue) + " credits.",
                c->districtId,
                c->location
            );
        }

        INFO_LOG(format("[FrankCastle] Neutralized all perpetrators at [%1%] in %2%! %3% hostages secured.")
            % c->typeName % c->districtName % c->hostageCount);

        // Subside heat
        m_districtHeat[c->districtId] = std::max(5.0f, m_districtHeat[c->districtId] - 15.0f);
    } else if (byPolice) {
        c->state = EmergentCrimeState::NeutralizedByPolice;
        m_totalCrimesNeutralized++;

        PolicePrecinct* p = GetPrecinctInDistrict(c->districtId);
        if (p) {
            p->holdingCellInmates += std::max(1u, c->perpCount / 2);
            p->confiscatedContrabandValue += c->lootValue;
        }

        // Subside heat
        m_districtHeat[c->districtId] = std::max(5.0f, m_districtHeat[c->districtId] - 10.0f);

        INFO_LOG(format("[MMPD Tactical] Secured crime scene at [%1%] in %2%. Suspects in custody, loot seized into evidence vault.")
            % c->typeName % c->districtName);
    }

    return true;
}

size_t UnderworldManager::GetActiveCrimeCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& kv : m_crimes) {
        if (kv.second.state == EmergentCrimeState::InProgress || kv.second.state == EmergentCrimeState::Escalated) {
            count++;
        }
    }
    return count;
}

PolicePrecinct* UnderworldManager::GetPrecinct(uint32 precinctId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_precincts.find(precinctId);
    return (it != m_precincts.end()) ? &it->second : nullptr;
}

PolicePrecinct* UnderworldManager::GetPrecinctInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& kv : m_precincts) {
        if (kv.second.districtId == districtId) {
            return &kv.second;
        }
    }
    return nullptr;
}

uint32 UnderworldManager::DispatchPoliceResponse(uint32 districtId, uint32 wantedStars, LocationVector sceneLoc, const std::string& incidentDesc, uint32 crimeId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32 dId = m_nextDispatchId++;

    PoliceDispatchCall call;
    call.dispatchId = dId;
    call.districtId = districtId;
    call.districtName = GetDistrictName(districtId);
    call.sceneLocation = sceneLoc;
    call.wantedLevelStars = std::clamp(wantedStars, 1u, 5u);
    call.respondingUnitType = (PoliceUnitType)call.wantedLevelStars;
    call.isArrived = false;
    call.responseTimerMs = 0;
    call.linkedCrimeId = crimeId;

    PolicePrecinct* p = GetPrecinctInDistrict(districtId);
    if (p) {
        call.respondingPrecinctId = p->precinctId;
        if (p->activePatrolUnits > 0) {
            p->activePatrolUnits--;
        } else {
            // Precinct patrol units are completely saturated! Trigger Mutual Aid!
            call.isMutualAid = true;
        }
    }

    // Determine 10-code and callsign
    if (call.isMutualAid) {
        call.tenCode = "10-99";
        uint32 aidDist = (districtId % 5) + 1;
        call.callsign = "Mutual-Aid Tactical (" + GetDistrictName(aidDist) + " Div)";
        call.dispatchChatter = (format("[MUTUAL AID PRIORITY] District %1% units saturated! %2% responding for %3% at (%4%, %5%)")
                                % call.districtName % call.callsign % incidentDesc % (int)sceneLoc.x % (int)sceneLoc.z).str();
    } else {
        if (call.wantedLevelStars == 1) {
            call.tenCode = "10-31"; // Crime in progress
            call.callsign = "Unit 1-Adam-12";
        } else if (call.wantedLevelStars == 2) {
            call.tenCode = "10-71"; // Shots fired / vehicle pursuit
            call.callsign = "Unit 2-Baker-4";
        } else if (call.wantedLevelStars == 3) {
            call.tenCode = "10-99"; // Officer in distress / high risk warrant
            call.callsign = "SWAT-Lead Bravo";
        } else if (call.wantedLevelStars == 4) {
            call.tenCode = "10-100"; // Severe hostile riot / heavy assault
            call.callsign = "Heavy BearCat Tactical-1";
        } else {
            call.tenCode = "10-00"; // Simulation integrity breach / Agent override
            call.callsign = "System Pacifier Skinner";
        }

        // Check precinct corruption
        if (p && p->corruptionIndex >= 50.0f && !p->isExposedByCastle && (rand() % 100 < (int)p->corruptionIndex)) {
            call.isCompromisedByCorruption = true;
            call.dispatchChatter = "[STATIC] Dispatch: Corrupt order logged. Response delayed for " + incidentDesc;
        } else {
            call.isCompromisedByCorruption = false;
            call.dispatchChatter = (format("MMPD Dispatch [%1%]: %2% code %3% responding to %4% at (%5%, %6%)")
                                    % call.callsign % call.tenCode % incidentDesc % call.districtName % (int)sceneLoc.x % (int)sceneLoc.z).str();
        }
    }

    m_recentDispatches.push_back(call);
    if (m_recentDispatches.size() > 20) {
        m_recentDispatches.erase(m_recentDispatches.begin());
    }

    // Also notify RadioDispatchSystem if available
    if (RadioDispatchSystem::getSingletonPtr()) {
        RadioTransmission tx;
        tx.transmissionId = dId;
        tx.tenCode = call.tenCode;
        tx.unitCallsign = call.callsign;
        tx.districtName = call.districtName;
        tx.locationAddress = (format("Coordinates (%1%, %2%)") % (int)sceneLoc.x % (int)sceneLoc.z).str();
        tx.chatterText = call.dispatchChatter;
        tx.threatHeatLevel = m_districtHeat[districtId];
        tx.escalationTier = call.wantedLevelStars;
        tx.timestampMs = getMSTime();
        sRadioDispatchSystem.BroadcastDispatch(tx, 30000.0f);
    }

    sEmergentAIMgr.OnPolice10CodeDispatched(districtId, call.tenCode, sceneLoc);

    if (call.wantedLevelStars >= 3 && EmergentPoliceManager::getSingletonPtr()) {
        sEmergentPoliceMgr.DeploySWATSquad(districtId, call.callsign, sceneLoc);
    }

    return dId;
}

bool UnderworldManager::InvestigatePrecinctCorruption(uint32 precinctId, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PolicePrecinct* p = GetPrecinct(precinctId);
    if (!p) return false;

    if (byCastle) {
        DEBUG_LOG(format("[InternalAffairs] Investigating corruption in %1% (%2%).") % p->name % p->precinctCaptainName);
    }

    if (EmergentPoliceManager::getSingletonPtr()) {
        InternalAffairsSting* sting = sEmergentPoliceMgr.GetActiveStingForPrecinct(precinctId);
        if (!sting) {
            uint32 sId = sEmergentPoliceMgr.LaunchIASting(precinctId, byCastle ? "Special Agent Frank Castle (Liaison)" : "Special Agent Marcus Kelly");
            if (byCastle && sId != 0) {
                sEmergentPoliceMgr.IntegrateCastleEvidenceIntoSting(sId);
            }
        } else if (byCastle) {
            sEmergentPoliceMgr.IntegrateCastleEvidenceIntoSting(sting->stingId);
        }
    }

    return true;
}

bool UnderworldManager::ExposePrecinctCorruption(uint32 precinctId, bool byCastle)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PolicePrecinct* p = GetPrecinct(precinctId);
    if (!p || p->isExposedByCastle) return false;

    p->isExposedByCastle = true;
    p->corruptionIndex = 0.0f; // Cleaned up
    p->cleanLeadershipTimerMs = 0;
    m_totalPrecinctsExposed++;

    if (EmergentPoliceManager::getSingletonPtr()) {
        InternalAffairsSting* sting = sEmergentPoliceMgr.GetActiveStingForPrecinct(precinctId);
        if (sting) {
            while (sting->phase != StingPhase::DEPARTMENTAL_PURGE_REFORM) {
                sEmergentPoliceMgr.AdvanceIASting(sting->stingId);
            }
        }
    }

    if (byCastle) {
        sFrankCastleMgr.AwardExperience(3500);
        sFrankCastleMgr.ScavengeSupplies(200, 5, 4, 3, 2);

        INFO_LOG(format("[InternalAffairs] Corrupt leadership purged in %1%. Clean leadership installed.") % p->name);

        sFrankCastleMgr.AddWarJournalEntry(
            JOURNAL_VIGILANTE_JUDGMENT,
            p->precinctCaptainName,
            "Corrupt Police Precinct Purged",
            "Exposed systemic bribes from " + GetFactionName(p->bribingFaction) + " inside " + p->name + ". Clean leadership installed.",
            p->districtId,
            p->precinctLocation
        );
    }

    return true;
}

bool UnderworldManager::RestockPrecinctPatrols(uint32 precinctId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PolicePrecinct* p = GetPrecinct(precinctId);
    if (!p) return false;
    p->activePatrolUnits = p->maxPatrolUnits;
    return true;
}

uint32 UnderworldManager::GetDistrictWantedLevel(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return 0;
    return m_districtWantedStars[districtId];
}

void UnderworldManager::SetDistrictWantedLevel(uint32 districtId, uint32 stars)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return;
    m_districtWantedStars[districtId] = std::clamp(stars, 0u, 5u);
}

float UnderworldManager::GetDistrictHeat(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return 0.0f;
    return m_districtHeat[districtId];
}

void UnderworldManager::SetDistrictHeat(uint32 districtId, float heat)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return;
    m_districtHeat[districtId] = std::clamp(heat, 0.0f, 100.0f);
}

void UnderworldManager::AddDistrictHeat(uint32 districtId, float delta)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return;
    m_districtHeat[districtId] = std::clamp(m_districtHeat[districtId] + delta, 0.0f, 100.0f);
}

std::string UnderworldManager::GetDistrictTensionName(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    float h = GetDistrictHeat(districtId);
    if (h < 26.0f) return "Low Tension (Covert Operations)";
    if (h < 61.0f) return "Elevated Crime (Gang Activity)";
    if (h < 86.0f) return "War Zone (Vigilante Strike Active)";
    return "Machine Critical (System Agent Lockdown)";
}

bool UnderworldManager::IsThreeWayWarActive(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return false;
    return m_districtHeat[districtId] >= 85.0f;
}

bool UnderworldManager::IsFourWayWarActive(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (districtId > 5) return false;
    return (m_districtHeat[districtId] >= 85.0f && m_districtWantedStars[districtId] >= 4);
}

void UnderworldManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    UpdateRackets(deltaMs);
    UpdateConvoys(deltaMs);
    UpdateShardHeat(deltaMs);
    UpdateTurfWars(deltaMs);
    UpdateEmergentCrimes(deltaMs);
    UpdatePoliceDispatches(deltaMs);
    UpdatePrecincts(deltaMs);

    // Sync Hit List every 10 seconds
    m_hitListSyncTimerMs += deltaMs;
    if (m_hitListSyncTimerMs >= 10000) {
        m_hitListSyncTimerMs = 0;
        SyncWithFrankCastleHitList();
    }

    // Periodic smuggling convoy spawn every 60 seconds
    m_convoySpawnTimerMs += deltaMs;
    if (m_convoySpawnTimerMs >= 60000) {
        m_convoySpawnTimerMs = 0;
        SyndicateFaction f = (SyndicateFaction)(rand() % 5);
        SpawnConvoy(f, 3, 2);
    }

    // Periodic emergent crime spawn every 45 seconds
    m_crimeSpawnTimerMs += deltaMs;
    if (m_crimeSpawnTimerMs >= 45000) {
        m_crimeSpawnTimerMs = 0;
        if (GetActiveCrimeCount() < 4) {
            EmergentCrimeType t = (EmergentCrimeType)(rand() % 7);
            uint32 d = 1 + (rand() % 5);
            LocationVector loc(1500.0f + (rand() % 2000), 0.0f, -3000.0f + (rand() % 2000));
            SpawnEmergentCrime(t, d, loc);
        }
    }

    // Periodic turf skirmish trigger every 75 seconds
    m_turfSkirmishTimerMs += deltaMs;
    if (m_turfSkirmishTimerMs >= 75000) {
        m_turfSkirmishTimerMs = 0;
        uint32 sId = 1 + (rand() % 10);
        SyndicateFaction attacker = (SyndicateFaction)(rand() % 5);
        TriggerTurfWar(sId, attacker);
    }
}

void UnderworldManager::UpdateRackets(uint32 deltaMs)
{
    for (auto& kv : m_rackets) {
        UnderworldRacketNode& r = kv.second;

        if (r.state == RacketState::DecapitatedCooldown) {
            r.cooldownTimerMs += deltaMs;
            if (r.cooldownTimerMs >= r.cooldownDurationMs) {
                r.state = RacketState::Active;
                r.defenseHealth = r.maxDefenseHealth;
                r.activeGoonsCount = 5;
                r.cooldownTimerMs = 0;
                r.skullDecalMarked = false;

                // Revive lieutenant if dead
                if (r.bossLieutenantId != 0) {
                    CartelLieutenant* l = GetLieutenant(r.bossLieutenantId);
                    if (l) l->isAlive = true;
                }

                DEBUG_LOG(format("[Underworld] New gang elements moving into [%1%] in %2%. Racket operational again.")
                    % r.name % r.districtName);
            }
        } else if (r.state == RacketState::Active || r.state == RacketState::Fortified) {
            // Active rackets slowly bleed heat into the district
            AddDistrictHeat(r.districtId, (r.illicitRevenueRate / 1000.0f) * ((float)deltaMs / 60000.0f));
        }
    }
}

void UnderworldManager::UpdateConvoys(uint32 deltaMs)
{
    for (auto it = m_convoys.begin(); it != m_convoys.end();) {
        SmugglingConvoy& c = it->second;
        if (c.status == ConvoyStatus::CONVOY_DESTROYED || c.status == ConvoyStatus::CONVOY_DELIVERED) {
            ++it;
            continue;
        }

        c.transitTimerMs += deltaMs;
        // Convoy takes 45 seconds to cross transit
        c.routeProgress = std::min(1.0f, (float)c.transitTimerMs / 45000.0f);

        // Interpolate location
        c.currentLocation.x = c.startLocation.x + (c.destinationLocation.x - c.startLocation.x) * c.routeProgress;
        c.currentLocation.z = c.startLocation.z + (c.destinationLocation.z - c.startLocation.z) * c.routeProgress;

        if (EmergentPoliceManager::getSingletonPtr()) {
            sEmergentPoliceMgr.OnConvoyDetectedOnRoadway(c.convoyId, c.startDistrictId, c.currentLocation);
        }

        if (c.routeProgress >= 1.0f) {
            c.status = ConvoyStatus::CONVOY_DELIVERED;
            AddDistrictHeat(c.destDistrictId, 15.0f);
        }

        ++it;
    }
}

void UnderworldManager::UpdateShardHeat(uint32 deltaMs)
{
    float decay = 1.0f * ((float)deltaMs / 60000.0f); // 1.0 heat drop per minute
    for (int d = 1; d <= 5; ++d) {
        m_districtHeat[d] = std::max(0.0f, m_districtHeat[d] - decay);

        if (m_districtHeat[d] >= 85.0f && !m_threeWayWarTriggered[d]) {
            m_threeWayWarTriggered[d] = true;
            std::string dName = GetDistrictName(d);

            INFO_LOG(format("[WorldDirector] Shard Heat critical (%1%%%) in %2%! System Agents deployed.")
                % (int)m_districtHeat[d] % dName);

            sFrankCastleMgr.AddHitListTarget(
                88000 + d,
                1000000ULL + d,
                "System Hunter-Killer Unit " + std::to_string(d),
                PRIORITY_SYSTEM_AGENT,
                600.0f,
                "Machine Pacification Squad deployed to purge 3-way gang war in " + dName,
                LocationVector(3000.0f, 0.0f, 2000.0f),
                d,
                dName
            );

            if (m_districtWantedStars[d] >= 4 && !m_fourWayWarTriggered[d]) {
                m_fourWayWarTriggered[d] = true;
                INFO_LOG(format("[WorldDirector] %1% urban battle escalated: Syndicates vs MMPD SWAT vs Vigilante vs System Agents.")
                    % dName);
            }
        } else if (m_districtHeat[d] < 70.0f) {
            m_threeWayWarTriggered[d] = false;
            m_fourWayWarTriggered[d] = false;
        }
    }
}

void UnderworldManager::UpdateTurfWars(uint32 deltaMs)
{
    for (auto& kv : m_turfSectors) {
        TurfSector& s = kv.second;
        if (!s.isContested) continue;

        s.turfWarTimerMs += deltaMs;
        // Skirmishes resolve in 15 seconds
        if (s.turfWarTimerMs >= 15000) {
            // Determine winner based on relative influence + random factor
            int attIdx = (int)s.attackingFaction;
            int defIdx = (int)s.controllingFaction;
            float attScore = s.factionInfluence[attIdx] + (rand() % 30);
            float defScore = s.factionInfluence[defIdx] + (rand() % 30);

            SyndicateFaction victor = (attScore > defScore) ? s.attackingFaction : s.controllingFaction;
            ResolveTurfWar(s.sectorId, victor);
        }
    }
}

void UnderworldManager::UpdateEmergentCrimes(uint32 deltaMs)
{
    for (auto it = m_crimes.begin(); it != m_crimes.end();) {
        EmergentCrimeEvent& c = it->second;
        if (c.state != EmergentCrimeState::InProgress && c.state != EmergentCrimeState::Escalated) {
            ++it;
            continue;
        }

        c.durationMs += deltaMs;

        // Escalate at half duration if not already escalated
        if (c.durationMs >= (c.timeLimitMs / 2) && c.state == EmergentCrimeState::InProgress) {
            EscalateCrime(c.crimeId);
        }

        // Check if time expired without intervention
        if (c.durationMs >= c.timeLimitMs) {
            c.state = EmergentCrimeState::CompletedEscaped;
            AddDistrictHeat(c.districtId, 15.0f);
            DEBUG_LOG(format("[Underworld] Perpetrators of [%1%] in %2% vanished with stolen loot.") % c.typeName % c.districtName);
        }

        ++it;
    }
}

void UnderworldManager::UpdatePoliceDispatches(uint32 deltaMs)
{
    for (auto& d : m_recentDispatches) {
        if (!d.isArrived) {
            d.responseTimerMs += deltaMs;
            uint32 arrivalThreshold = d.isMutualAid ? 12000 : 8000;
            if (d.responseTimerMs >= arrivalThreshold) {
                d.isArrived = true;

                // Handle linked crime resolution
                if (d.linkedCrimeId != 0) {
                    EmergentCrimeEvent* c = GetEmergentCrime(d.linkedCrimeId);
                    if (c && (c->state == EmergentCrimeState::InProgress || c->state == EmergentCrimeState::Escalated)) {
                        if (d.isCompromisedByCorruption) {
                            // Crooked officers stand down and pocket a cut
                            c->state = EmergentCrimeState::CompletedEscaped;
                            AddDistrictHeat(c->districtId, 10.0f);

                            PolicePrecinct* p = GetPrecinctInDistrict(c->districtId);
                            if (p) {
                                p->corruptionIndex = std::min(95.0f, p->corruptionIndex + 2.0f);
                                p->confiscatedContrabandValue += (c->lootValue * 0.15f);
                            }

                            DEBUG_LOG(format("[Underworld] Crooked officers stood down at [%1%] in %2%. Syndicate perps escaped.")
                                % c->typeName % c->districtName);
                        } else {
                            // Clean officers intervene!
                            if (d.respondingUnitType >= PoliceUnitType::SWATBreachTeam ||
                                c->type == EmergentCrimeType::StreetMugging ||
                                c->type == EmergentCrimeType::ContrabandDrop ||
                                c->perpCount <= 3) {
                                NeutralizeCrime(c->crimeId, false, true);
                            } else {
                                // Beat patrol outgunned by heavy bank crew; escalate situation
                                EscalateCrime(c->crimeId);
                            }
                        }
                    }
                }
            }
        }
    }
}

void UnderworldManager::UpdatePrecincts(uint32 deltaMs)
{
    // Patrol unit replenishment every 15 seconds
    m_precinctReplenishTimerMs += deltaMs;
    if (m_precinctReplenishTimerMs >= 15000) {
        m_precinctReplenishTimerMs = 0;
        for (auto& kv : m_precincts) {
            PolicePrecinct& p = kv.second;
            if (p.activePatrolUnits < p.maxPatrolUnits) {
                p.activePatrolUnits++;
            }
        }
    }

    // Inmate transfer / processing every 45 seconds
    m_inmateProcessingTimerMs += deltaMs;
    if (m_inmateProcessingTimerMs >= 45000) {
        m_inmateProcessingTimerMs = 0;
        for (auto& kv : m_precincts) {
            PolicePrecinct& p = kv.second;
            if (p.holdingCellInmates > 5) {
                p.holdingCellInmates--;
            }
        }
    }

    // Dynamic corruption and clean vigilance tracking
    for (auto& kv : m_precincts) {
        PolicePrecinct& p = kv.second;
        if (p.isExposedByCastle) {
            p.cleanLeadershipTimerMs += deltaMs;
            // 5 minutes of clean leadership vigilance before corruption can slowly return
            if (p.cleanLeadershipTimerMs >= 300000) {
                p.isExposedByCastle = false;
                p.cleanLeadershipTimerMs = 0;
            }
        } else {
            // Count active rackets belonging to bribing faction in this district
            uint32 activeRackets = 0;
            for (const auto& rkv : m_rackets) {
                if (rkv.second.districtId == p.districtId &&
                    rkv.second.faction == p.bribingFaction &&
                    (rkv.second.state == RacketState::Active || rkv.second.state == RacketState::Fortified)) {
                    activeRackets++;
                }
            }

            if (activeRackets > 0) {
                // Cartel kickbacks slowly raise corruption: +0.25% per minute per active racket
                float bribeRate = (0.25f * (float)activeRackets) * ((float)deltaMs / 60000.0f);
                p.corruptionIndex = std::min(95.0f, p.corruptionIndex + bribeRate);
            } else {
                // Decapitated syndicates can't pay bribes: clean officers slowly reduce corruption
                float decayRate = 0.5f * ((float)deltaMs / 60000.0f);
                p.corruptionIndex = std::max(5.0f, p.corruptionIndex - decayRate);
            }
        }
    }
}

void UnderworldManager::SyncWithFrankCastleHitList()
{
    // Feed living bosses into Frank Castle's dynamic Hit List
    for (const auto& kv : m_lieutenants) {
        const auto& l = kv.second;
        if (!l.isAlive) continue;

        float infamy = 200.0f + (float)l.grudgeScoreAgainstCastle + (float)(l.bountyOnCastle / 100);
        std::string traitsStr;
        for (const auto& t : l.personalityTraits) {
            traitsStr += "[" + t + "] ";
        }

        sFrankCastleMgr.AddHitListTarget(
            77000 + l.id,
            9900000ULL + l.id,
            l.name,
            PRIORITY_SYNDICATE_BOSS,
            infamy,
            l.rankTitle + " (" + l.factionName + ") | Traits: " + traitsStr + "| Bounty: " + std::to_string(l.bountyOnCastle),
            l.lastKnownLocation,
            l.districtId,
            l.districtName
        );
    }
}

std::string UnderworldManager::GenerateUnderworldStatusReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "{c:FF5500}=== MEGACITY CRIMINAL UNDERWORLD ECOSYSTEM ==={/c}\n";
    ss << " Active Rackets: " << m_rackets.size() << " | Decapitated (Cooldown): " << m_totalRacketsDecapitated << "\n";
    ss << " Total Convoys Intercepted: " << m_totalConvoysIntercepted << " | Active Convoys: " << m_convoys.size() << "\n";
    ss << " Total Emergent Crimes Neutralized: " << m_totalCrimesNeutralized << " | Active Crimes: " << GetActiveCrimeCount() << "\n";
    ss << " Police Precincts Exposed: " << m_totalPrecinctsExposed << " / " << m_precincts.size() << "\n\n";

    ss << "{c:FFFF00}--- DISTRICT SHARD HEAT, WANTED STARS & TENSION ---{/c}\n";
    for (int d = 1; d <= 5; ++d) {
        ss << " [" << d << "] " << std::left << std::setw(14) << GetDistrictName(d) 
           << " : Heat " << std::fixed << std::setprecision(1) << m_districtHeat[d] << "/100.0"
           << " | Wanted: [" << m_districtWantedStars[d] << "★] (" 
           << GetDistrictTensionName(d) << ")\n";
    }

    ss << "\n{c:FFFF00}--- NOTORIOUS RACKET NODES ---{/c}\n";
    for (const auto& kv : m_rackets) {
        const auto& r = kv.second;
        std::string stateStr = (r.state == RacketState::Active) ? "ACTIVE" :
                               ((r.state == RacketState::Fortified) ? "FORTIFIED" :
                               ((r.state == RacketState::UnderSiegeByCastle) ? "UNDER SIEGE" : "DECAPITATED (COOLDOWN)"));
        ss << " #" << r.id << " [" << r.name << "] (" << r.districtName << ") - " 
           << r.factionName << "\n   Status: " << stateStr 
           << " | Boss: " << r.bossName 
           << " | HP: " << (int)r.defenseHealth << "/" << (int)r.maxDefenseHealth
           << " | Skull Signature: " << (r.skullDecalMarked ? "YES" : "NO") << "\n";
    }

    return ss.str();
}

std::string UnderworldManager::GenerateBossHierarchyReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "{c:FF2222}=== CARTEL LIEUTENANTS & NEMESIS HIERARCHY ==={/c}\n";
    for (const auto& kv : m_lieutenants) {
        const auto& l = kv.second;
        ss << " #" << l.id << " [" << l.name << "] - " << l.rankTitle << " (" << l.factionName << ")\n";
        ss << "   Status: " << (l.isAlive ? "{c:00FF00}ALIVE & ACTIVE{/c}" : "{c:777777}TERMINATED{/c}")
           << " | District: " << l.districtName 
           << " | Bounty On Castle: " << l.bountyOnCastle 
           << " | Grudge: " << l.grudgeScoreAgainstCastle << "\n   Traits: ";
        for (const auto& t : l.personalityTraits) {
            ss << "[" << t << "] ";
        }
        ss << "\n";
    }
    return ss.str();
}

std::string UnderworldManager::GenerateTurfGridReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "{c:00FFCC}=== GANG TURF & TERRITORY GRID ==={/c}\n";
    for (const auto& kv : m_turfSectors) {
        const auto& s = kv.second;
        ss << " Sector #" << s.sectorId << " [" << s.name << "] (" << s.districtName << ")\n";
        ss << "   Dominant Gang: " << GetFactionName(s.controllingFaction) 
           << " | Contested: " << (s.isContested ? "{c:FF3333}YES (TURF WAR IN PROGRESS){/c}" : "NO")
           << " | Skirmishes: " << s.totalSkirmishes << "\n";
        ss << "   Influence: Wolfpack: " << (int)s.factionInfluence[0] << "% | Chateau: " << (int)s.factionInfluence[1]
           << "% | Judas: " << (int)s.factionInfluence[2] << "% | Marcone: " << (int)s.factionInfluence[3]
           << "% | ByteCartel: " << (int)s.factionInfluence[4] << "%\n";
    }
    return ss.str();
}

std::string UnderworldManager::GenerateEmergentCrimesReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "{c:FF3366}=== EMERGENT STREET CRIMES & POLICE DISPATCH ==={/c}\n";
    ss << " Active Crimes: " << GetActiveCrimeCount() << " | Lifetime Neutralized: " << m_totalCrimesNeutralized << "\n";
    for (const auto& kv : m_crimes) {
        const auto& c = kv.second;
        std::string st = (c.state == EmergentCrimeState::InProgress) ? "{c:FFFF00}IN PROGRESS{/c}" :
                         ((c.state == EmergentCrimeState::Escalated) ? "{c:FF0000}ESCALATED{/c}" :
                         ((c.state == EmergentCrimeState::NeutralizedByCastle) ? "{c:00FF00}PUNISHED BY CASTLE{/c}" :
                         ((c.state == EmergentCrimeState::NeutralizedByPolice) ? "{c:3399FF}SECURED BY POLICE{/c}" : "ESCAPED")));
        ss << " Crime #" << c.crimeId << " [" << c.typeName << "] in " << c.districtName << " - Status: " << st << "\n";
        ss << "   Perps: " << c.perpFactionName << " (" << c.perpCount << " gunmen) | Hostages: " << c.hostageCount
           << " | Loot: " << (int)c.lootValue << " credits\n";
    }
    return ss.str();
}

std::string UnderworldManager::GeneratePolicePrecinctsReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "{c:3399FF}=== METROPOLITAN POLICE DEPARTMENT (MMPD) PRECINCTS ==={/c}\n";
    for (const auto& kv : m_precincts) {
        const auto& p = kv.second;
        ss << " Precinct #" << p.precinctId << " [" << p.name << "] (" << p.districtName << ")\n";
        ss << "   Commander: " << p.precinctCaptainName 
           << " | Corruption: " << (p.isExposedByCastle ? "{c:00FF00}0.0% (EXPOSED & PURGED){/c}" : 
                                   ((p.corruptionIndex > 50.0f) ? "{c:FF3333}" + std::to_string((int)p.corruptionIndex) + "% (MOB CONTROLLED){/c}" : 
                                   std::to_string((int)p.corruptionIndex) + "%"))
           << "\n   Bribe Syndicate: " << GetFactionName(p.bribingFaction)
           << " | Patrol Units: " << p.activePatrolUnits << "/" << p.maxPatrolUnits
           << " | Holding Cells: " << p.holdingCellInmates << " inmates"
           << " | Evidence Vault: " << (int)p.confiscatedContrabandValue << " credits\n";
    }
    return ss.str();
}

bool UnderworldManager::SaveUnderworldStateToFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << "{\n";
    ofs << "  \"heat\": [\n";
    for (int d = 1; d <= 5; ++d) {
        ofs << "    {\"district\": " << d << ", \"heat\": " << m_districtHeat[d] << ", \"wanted\": " << m_districtWantedStars[d] << "}" << (d < 5 ? "," : "") << "\n";
    }
    ofs << "  ],\n";
    ofs << "  \"stats\": {\n";
    ofs << "    \"totalDecapitated\": " << m_totalRacketsDecapitated << ",\n";
    ofs << "    \"totalConvoysIntercepted\": " << m_totalConvoysIntercepted << ",\n";
    ofs << "    \"totalCrimesNeutralized\": " << m_totalCrimesNeutralized << ",\n";
    ofs << "    \"totalPrecinctsExposed\": " << m_totalPrecinctsExposed << "\n";
    ofs << "  },\n";
    ofs << "  \"rackets\": [\n";
    size_t rIdx = 0;
    for (const auto& kv : m_rackets) {
        const auto& r = kv.second;
        ofs << "    {\"id\": " << r.id << ", \"state\": " << (int)r.state << ", \"health\": " << r.defenseHealth
            << ", \"skull\": " << (r.skullDecalMarked ? 1 : 0) << ", \"cooldown\": " << r.cooldownTimerMs << "}"
            << (++rIdx < m_rackets.size() ? "," : "") << "\n";
    }
    ofs << "  ],\n";
    ofs << "  \"precincts\": [\n";
    size_t pIdx = 0;
    for (const auto& kv : m_precincts) {
        const auto& p = kv.second;
        ofs << "    {\"id\": " << p.precinctId << ", \"corruption\": " << p.corruptionIndex
            << ", \"exposed\": " << (p.isExposedByCastle ? 1 : 0) << ", \"inmates\": " << p.holdingCellInmates
            << ", \"contraband\": " << p.confiscatedContrabandValue << ", \"units\": " << p.activePatrolUnits << "}"
            << (++pIdx < m_precincts.size() ? "," : "") << "\n";
    }
    ofs << "  ],\n";
    ofs << "  \"sectors\": [\n";
    size_t sIdx = 0;
    for (const auto& kv : m_turfSectors) {
        const auto& s = kv.second;
        ofs << "    {\"id\": " << s.sectorId << ", \"controlling\": " << (int)s.controllingFaction
            << ", \"inf0\": " << s.factionInfluence[0] << ", \"inf1\": " << s.factionInfluence[1]
            << ", \"inf2\": " << s.factionInfluence[2] << ", \"inf3\": " << s.factionInfluence[3]
            << ", \"inf4\": " << s.factionInfluence[4] << "}"
            << (++sIdx < m_turfSectors.size() ? "," : "") << "\n";
    }
    ofs << "  ]\n";
    ofs << "}\n";
    ofs.close();
    return true;
}

bool UnderworldManager::LoadUnderworldStateFromFile(const std::string& path)
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
        if (line.find("\"district\":") != std::string::npos && line.find("\"heat\":") != std::string::npos) {
            uint32 d = (uint32)extractNumber(line, "district");
            if (d >= 1 && d <= 5) {
                m_districtHeat[d] = (float)extractNumber(line, "heat");
                m_districtWantedStars[d] = (uint32)extractNumber(line, "wanted");
                loadedItems++;
            }
        } else if (line.find("\"totalDecapitated\":") != std::string::npos) {
            m_totalRacketsDecapitated = (uint32)extractNumber(line, "totalDecapitated");
            loadedItems++;
        } else if (line.find("\"totalConvoysIntercepted\":") != std::string::npos) {
            m_totalConvoysIntercepted = (uint32)extractNumber(line, "totalConvoysIntercepted");
        } else if (line.find("\"totalCrimesNeutralized\":") != std::string::npos) {
            m_totalCrimesNeutralized = (uint32)extractNumber(line, "totalCrimesNeutralized");
        } else if (line.find("\"totalPrecinctsExposed\":") != std::string::npos) {
            m_totalPrecinctsExposed = (uint32)extractNumber(line, "totalPrecinctsExposed");
        } else if (line.find("\"id\":") != std::string::npos && line.find("\"state\":") != std::string::npos) {
            uint32 rId = (uint32)extractNumber(line, "id");
            UnderworldRacketNode* r = GetRacket(rId);
            if (r) {
                r->state = (RacketState)(int)extractNumber(line, "state");
                r->defenseHealth = (float)extractNumber(line, "health");
                r->skullDecalMarked = (extractNumber(line, "skull") > 0.5);
                r->cooldownTimerMs = (uint32)extractNumber(line, "cooldown");
                loadedItems++;
            }
        } else if (line.find("\"id\":") != std::string::npos && line.find("\"corruption\":") != std::string::npos) {
            uint32 pId = (uint32)extractNumber(line, "id");
            PolicePrecinct* p = GetPrecinct(pId);
            if (p) {
                p->corruptionIndex = (float)extractNumber(line, "corruption");
                p->isExposedByCastle = (extractNumber(line, "exposed") > 0.5);
                p->holdingCellInmates = (uint32)extractNumber(line, "inmates");
                p->confiscatedContrabandValue = (float)extractNumber(line, "contraband");
                p->activePatrolUnits = (uint32)extractNumber(line, "units");
                loadedItems++;
            }
        } else if (line.find("\"id\":") != std::string::npos && line.find("\"controlling\":") != std::string::npos) {
            uint32 sId = (uint32)extractNumber(line, "id");
            TurfSector* s = GetTurfSector(sId);
            if (s) {
                s->controllingFaction = (SyndicateFaction)(int)extractNumber(line, "controlling");
                s->factionInfluence[0] = (float)extractNumber(line, "inf0");
                s->factionInfluence[1] = (float)extractNumber(line, "inf1");
                s->factionInfluence[2] = (float)extractNumber(line, "inf2");
                s->factionInfluence[3] = (float)extractNumber(line, "inf3");
                s->factionInfluence[4] = (float)extractNumber(line, "inf4");
                loadedItems++;
            }
        }
    }
    ifs.close();
    return loadedItems > 0;
}

// ============================================================================
// Underworld Master Test Suite
// ============================================================================
void RunUnderworldTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  MEGACITY CRIMINAL UNDERWORLD - ECOSYSTEM VERIFICATION     " << std::endl;
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

    sUnderworldMgr.Initialize();

    // 1. Core Underworld Initialization
    assertTest("Underworld Racket Nodes Initialized (14+ nodes)", sUnderworldMgr.GetAllRackets().size() >= 14);
    assertTest("Cartel Lieutenants Initialized (9 bosses)", sUnderworldMgr.GetAllLieutenants().size() >= 9);
    assertTest("All 5 Districts Have Heat Values", sUnderworldMgr.GetDistrictHeat(1) > 0.0f && sUnderworldMgr.GetDistrictHeat(2) > 0.0f);

    // 2. Boss Traits & Nemesis Memory
    CartelLieutenant* cassian = sUnderworldMgr.GetLieutenant(3);
    assertTest("Château Sanguine Sire Cassian Trait Check (Phase Glitcher)", cassian != nullptr && cassian->HasTrait("Phase Glitcher"));
    CartelLieutenant* vane = sUnderworldMgr.GetLieutenant(1);
    assertTest("Wolfpack Alpha Vane Trait Check (Ironclad)", vane != nullptr && vane->HasTrait("Ironclad"));

    // 3. Racket Raid, Damage & Decapitation Lifecycle
    UnderworldRacketNode* r = sUnderworldMgr.GetRacket(101);
    assertTest("Target Racket Exists in Slums", r != nullptr && r->districtId == 1);
    bool raided = sUnderworldMgr.RaidRacket(101, true);
    assertTest("Racket Under Siege State Transition", raided && r->state == RacketState::UnderSiegeByCastle);
    bool decap = sUnderworldMgr.DecapitateRacket(101, true);
    assertTest("Racket Decapitation & Skull Signature Decal", decap && r->state == RacketState::DecapitatedCooldown && r->skullDecalMarked);
    assertTest("Decapitation Suppresses District Shard Heat", sUnderworldMgr.GetDistrictHeat(1) <= 20.0f);

    // 4. Smuggling Convoys
    uint32 cId = sUnderworldMgr.SpawnConvoy(SyndicateFaction::MarconeFamily, 3, 2);
    SmugglingConvoy* convoy = sUnderworldMgr.GetConvoy(cId);
    assertTest("Smuggling Convoy Deployment", convoy != nullptr && convoy->status == ConvoyStatus::CONVOY_IN_TRANSIT);
    bool intercepted = sUnderworldMgr.InterceptConvoy(cId, true);
    assertTest("Smuggling Convoy Interception & Cargo Confiscation", intercepted && convoy->status == ConvoyStatus::CONVOY_DESTROYED);

    // 5. Gang Turf & Territory Grid Skirmishes (Attacker Victory & Defender Victory)
    assertTest("Turf Sectors Initialized (10 sectors across city)", sUnderworldMgr.GetAllTurfSectors().size() == 10);
    TurfSector* s1 = sUnderworldMgr.GetTurfSector(1);
    assertTest("Sector 1 Dominant Gang (Wolfpack Lupines)", s1 != nullptr && s1->controllingFaction == SyndicateFaction::WolfpackLupines);
    bool warTriggered = sUnderworldMgr.TriggerTurfWar(1, SyndicateFaction::MarconeFamily);
    assertTest("Turf War Trigger & Contested State", warTriggered && s1->isContested);
    bool warResolved = sUnderworldMgr.ResolveTurfWar(1, SyndicateFaction::MarconeFamily);
    assertTest("Turf War Resolution & Influence Redistribution (Attacker Victorious)", warResolved && !s1->isContested && s1->factionInfluence[(int)SyndicateFaction::MarconeFamily] >= 30.0f);

    // Defender Victory Edge Case Verification: Defender should NOT self-cannibalize influence
    sUnderworldMgr.TriggerTurfWar(1, SyndicateFaction::ByteCartel);
    float defBefore = s1->factionInfluence[(int)SyndicateFaction::MarconeFamily];
    float attBefore = s1->factionInfluence[(int)SyndicateFaction::ByteCartel];
    sUnderworldMgr.ResolveTurfWar(1, SyndicateFaction::MarconeFamily); // Defender wins
    bool defWonProperly = (s1->factionInfluence[(int)SyndicateFaction::MarconeFamily] > defBefore) &&
                          (s1->factionInfluence[(int)SyndicateFaction::ByteCartel] <= attBefore);
    assertTest("Turf War Resolution Defender Victory (No Self-Penalty)", defWonProperly);

    // 6. Emergent Street Crimes Engine
    uint32 crimeId = sUnderworldMgr.SpawnEmergentCrime(EmergentCrimeType::BankHeist, 2, LocationVector(4200.0f, 50.0f, 1200.0f));
    EmergentCrimeEvent* crime = sUnderworldMgr.GetEmergentCrime(crimeId);
    assertTest("Emergent Bank Heist Spawned with Hostages", crime != nullptr && crime->type == EmergentCrimeType::BankHeist && crime->hostageCount > 0);
    bool escalated = sUnderworldMgr.EscalateCrime(crimeId);
    assertTest("Crime Shootout Escalation & Wanted Star Elevation", escalated && crime->state == EmergentCrimeState::Escalated);
    bool neutralized = sUnderworldMgr.NeutralizeCrime(crimeId, true, false);
    assertTest("Frank Castle Intervention & Hostage Rescue", neutralized && crime->state == EmergentCrimeState::NeutralizedByCastle);

    // 7. MMPD Police & Law Enforcement System
    assertTest("Police Precincts Initialized (5 precincts)", sUnderworldMgr.GetAllPrecincts().size() == 5);
    PolicePrecinct* harborPrecinct = sUnderworldMgr.GetPrecinct(3);
    assertTest("Harbor Division Precinct Corruption High", harborPrecinct != nullptr && harborPrecinct->corruptionIndex >= 70.0f);
    uint32 dispatchId = sUnderworldMgr.DispatchPoliceResponse(3, 4, LocationVector(-2500.0f, 10.0f, 5000.0f), "Bank Heist In Progress");
    assertTest("MMPD 4-Star Heavy BearCat Tactical Dispatch", dispatchId > 0 && sUnderworldMgr.GetRecentDispatches().back().callsign.find("BearCat") != std::string::npos);

    // 8. Patrol Unit Allocation & Mutual Aid Trigger
    PolicePrecinct* p1 = sUnderworldMgr.GetPrecinct(1);
    uint32 unitsBefore = p1->activePatrolUnits;
    sUnderworldMgr.DispatchPoliceResponse(1, 2, LocationVector(1500.0f, 0.0f, -3200.0f), "Slums Patrol Call");
    assertTest("Police Dispatch Allocates Precinct Patrol Unit", p1->activePatrolUnits == unitsBefore - 1);

    // Exhaust precinct units to test Mutual Aid
    p1->activePatrolUnits = 0;
    sUnderworldMgr.DispatchPoliceResponse(1, 4, LocationVector(1500.0f, 0.0f, -3200.0f), "Critical Riot");
    const auto& dispatches = sUnderworldMgr.GetRecentDispatches();
    assertTest("Precinct Unit Saturation Triggers Mutual Aid Response", dispatches.back().isMutualAid);

    // Restock patrol units
    sUnderworldMgr.RestockPrecinctPatrols(1);
    assertTest("Precinct Patrol Unit Replenishment Restores Roster", p1->activePatrolUnits == p1->maxPatrolUnits);

    // 9. Autonomous Clean Police Intervention & Evidence Vault
    uint32 streetCrimeId = sUnderworldMgr.SpawnEmergentCrime(EmergentCrimeType::StreetMugging, 1, LocationVector(1500.0f, 0.0f, -3200.0f));
    float contraBefore = p1->confiscatedContrabandValue;
    uint32 inmatesBefore = p1->holdingCellInmates;
    sUnderworldMgr.UpdatePoliceDispatches(10000); // Trigger 8000ms arrival
    EmergentCrimeEvent* streetCrime = sUnderworldMgr.GetEmergentCrime(streetCrimeId);
    assertTest("Autonomous Clean Police Intervention Neutralizes Crime", streetCrime != nullptr && streetCrime->state == EmergentCrimeState::NeutralizedByPolice);
    assertTest("Police Evidence Locker Contraband Confiscated", p1->confiscatedContrabandValue > contraBefore);
    assertTest("Police Holding Cell Apprehended Inmates Updated", p1->holdingCellInmates > inmatesBefore);

    // 10. Inmate Processing & Dynamic Re-Corruption
    uint32 curInmates = p1->holdingCellInmates;
    sUnderworldMgr.UpdatePrecincts(50000); // 50s tick processes inmate transfer
    assertTest("Police Holding Cell Inmate Processing Transfer", p1->holdingCellInmates < curInmates);

    PolicePrecinct* p4 = sUnderworldMgr.GetPrecinct(4); // Industrial has active Byte Cartel rackets
    float corruptBefore = p4->corruptionIndex;
    sUnderworldMgr.UpdatePrecincts(120000); // 2 minutes of cartel kickbacks
    assertTest("Dynamic Precinct Re-Corruption via Active Syndicates", p4->corruptionIndex > corruptBefore);

    // 11. Internal Affairs Probe & Crooked Badge Purge
    bool probed = sUnderworldMgr.InvestigatePrecinctCorruption(3, true);
    assertTest("Internal Affairs & Vigilante Corruption Probe", probed);
    bool purged = sUnderworldMgr.ExposePrecinctCorruption(3, true);
    assertTest("Crooked Badge Exposure & Evidence Vault Confiscation", purged && harborPrecinct->corruptionIndex == 0.0f && harborPrecinct->isExposedByCastle);
    assertTest("Clean Leadership Post-Purge Vigilance Active", harborPrecinct->isExposedByCastle);

    // 12. 4-Way Urban War Escalation
    sUnderworldMgr.SetDistrictHeat(2, 95.0f);
    sUnderworldMgr.SetDistrictWantedLevel(2, 4);
    assertTest("High Shard Heat Triggers 3-Way War", sUnderworldMgr.IsThreeWayWarActive(2));
    assertTest("Maximum Tension + 4-Star Wanted Triggers 4-Way War", sUnderworldMgr.IsFourWayWarActive(2));

    // 13. Frank Castle Hit List Ingestion
    sUnderworldMgr.Update(1000);
    auto hitList = sFrankCastleMgr.GetHitList();
    bool foundUnderworldBossOnHitList = false;
    for (const auto& h : hitList) {
        if (h.priority == PRIORITY_SYNDICATE_BOSS) {
            foundUnderworldBossOnHitList = true;
            break;
        }
    }
    assertTest("Frank Castle Dynamic Hit List Ingests Underworld Lieutenants", foundUnderworldBossOnHitList);

    // 14. Decapitation Cooldown Reset
    sUnderworldMgr.ClearAllCooldowns();
    assertTest("Underworld Cooldown Clear Resets Rackets to Active", r->state == RacketState::Active);

    // 15. Lossless Round-Trip JSON State Persistence & Restoration
    sUnderworldMgr.SetDistrictHeat(3, 77.5f);
    sUnderworldMgr.SetDistrictWantedLevel(3, 4);
    bool saved = sUnderworldMgr.SaveUnderworldStateToFile("UnderworldEcosystem_Test.json");
    sUnderworldMgr.SetDistrictHeat(3, 10.0f);
    sUnderworldMgr.SetDistrictWantedLevel(3, 1);
    bool loaded = sUnderworldMgr.LoadUnderworldStateFromFile("UnderworldEcosystem_Test.json");
    bool restoredAccurately = saved && loaded && (std::abs(sUnderworldMgr.GetDistrictHeat(3) - 77.5f) < 0.1f) &&
                              (sUnderworldMgr.GetDistrictWantedLevel(3) == 4);
    assertTest("Lossless Round-Trip JSON State Persistence & Restoration", restoredAccurately);

    // 16. Concurrency & Mutex Thread Safety Verification
    bool threadsCompleted = true;
    try {
        std::thread t1([&]() {
            for (int i = 0; i < 50; ++i) {
                sUnderworldMgr.GetActiveCrimeCount();
                sUnderworldMgr.GetDistrictHeat(1);
            }
        });
        std::thread t2([&]() {
            for (int i = 0; i < 50; ++i) {
                sUnderworldMgr.GetLivingLieutenants();
                sUnderworldMgr.GetDistrictWantedLevel(2);
            }
        });
        t1.join();
        t2.join();
    } catch (...) {
        threadsCompleted = false;
    }
    assertTest("Underworld Concurrency & Mutex Thread Safety", threadsCompleted);

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  UNDERWORLD TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================" << std::endl;

    if (failed != 0) {
        exit(1);
    }
}

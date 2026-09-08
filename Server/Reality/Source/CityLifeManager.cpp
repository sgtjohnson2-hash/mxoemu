#include "CityLifeManager.h"
#include "EmergentAIEngine.h"
#include "WeatherSystem.h"
#include "UnderworldManager.h"
#include "BiographicalNarrativeEngine.h"
#include "NPCSocialLifeEngine.h"
#include "NPCFamilyDreamsEngine.h"
#include "NPCEmergentLifeEngine.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameServer.h"
#include "Log.h"
#include "Timer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cassert>

static inline bool Has3DWorldSupport() {
    return GameServer::getSingletonPtr() != nullptr && BotManager::getSingletonPtr() != nullptr;
}

createFileSingleton(CityLifeManager);

CityLifeManager::CityLifeManager()
{
}

CityLifeManager::~CityLifeManager()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_citizens.clear();
    m_workplaces.clear();
    m_shops.clear();
    m_stations.clear();
    m_trains.clear();
    m_intersections.clear();
    m_vehicles.clear();
    m_rumors.clear();
}

// ============================================================================
// String Conversion Utilities
// ============================================================================

std::string CityLifeManager::GetArchetypeName(CivilianArchetype a)
{
    switch (a) {
        case CivilianArchetype::CorporateSuit:        return "Corporate Suit";
        case CivilianArchetype::IndustrialBlueCollar: return "Industrial Blue-Collar";
        case CivilianArchetype::ServiceRetailWorker:  return "Service & Retail Worker";
        case CivilianArchetype::AcademicStudent:      return "Academic & Student";
        case CivilianArchetype::NightlifeClubber:     return "Nightlife Clubber";
        case CivilianArchetype::UrbanDrifter:         return "Urban Drifter";
        case CivilianArchetype::MunicipalCivilServant:return "Municipal Civil Servant";
        case CivilianArchetype::MedicalStaff:         return "Medical Staff";
    }
    return "Unknown Archetype";
}

std::string CityLifeManager::GetRoutineStateName(RoutineScheduleState r)
{
    switch (r) {
        case RoutineScheduleState::Sleeping:       return "Sleeping";
        case RoutineScheduleState::Breakfast:      return "Breakfast";
        case RoutineScheduleState::CommuteToWork:  return "Commute To Work";
        case RoutineScheduleState::Working:        return "Working";
        case RoutineScheduleState::LunchBreak:     return "Lunch Break";
        case RoutineScheduleState::AfternoonWork:  return "Afternoon Work";
        case RoutineScheduleState::CommuteHome:    return "Commute Home";
        case RoutineScheduleState::Shopping:       return "Shopping";
        case RoutineScheduleState::Dining:         return "Dining";
        case RoutineScheduleState::Leisure:        return "Leisure";
        case RoutineScheduleState::Nightclubbing:  return "Nightclubbing";
        case RoutineScheduleState::Panicking:      return "Panicking";
        case RoutineScheduleState::Sheltering:     return "Sheltering";
    }
    return "Unknown State";
}

std::string CityLifeManager::GetShiftName(WorkShiftType s)
{
    switch (s) {
        case WorkShiftType::ShiftDay:       return "Day Shift (08:00 - 17:00)";
        case WorkShiftType::ShiftSwing:     return "Swing Shift (16:00 - 00:00)";
        case WorkShiftType::ShiftGraveyard: return "Graveyard Shift (00:00 - 08:00)";
        case WorkShiftType::ShiftFlexible:  return "Flexible / Freelance";
    }
    return "Unknown Shift";
}

std::string CityLifeManager::GetSubwayLineName(SubwayLineId l)
{
    switch (l) {
        case SubwayLineId::Line1_RedLine:   return "Line 1 (Red Line - Main Trunk)";
        case SubwayLineId::Line2_BlueLine:  return "Line 2 (Blue Line - East Crosstown)";
        case SubwayLineId::Line3_GreenLine: return "Line 3 (Green Line - Waterfront Loop)";
    }
    return "Unknown Subway Line";
}

std::string CityLifeManager::GetVehicleTypeName(VehicleClassification v)
{
    switch (v) {
        case VehicleClassification::TAXI_CAB:          return "Yellow Cab Taxi";
        case VehicleClassification::TRANSIT_BUS:       return "Municipal Transit Bus";
        case VehicleClassification::DELIVERY_VAN:      return "Commercial Delivery Van";
        case VehicleClassification::CIVILIAN_SEDAN:    return "Civilian Sedan";
        case VehicleClassification::MUNICIPAL_CRUISER: return "Municipal Police Cruiser";
    }
    return "Unknown Vehicle";
}

std::string CityLifeManager::GetDistrictName(uint32 districtId)
{
    switch (districtId) {
        case 1: return "Slums (Westview & Morrell)";
        case 2: return "Downtown Metacortex Core";
        case 3: return "International Concourse";
        case 4: return "Richland Financial High-Rises";
        case 5: return "Industrial Harbor & Canals";
    }
    return "Megacity Outskirts";
}

// ============================================================================
// Initialization Subroutines
// ============================================================================

void CityLifeManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    INFO_LOG("CityLifeManager: Initializing Megacity Simulation, Daily Life & Bluepill Ecology...");

    m_simulatedHour = 8.0f; // 08:00 AM Morning Shift Start
    m_totalTransitRidership = 0;
    m_totalCommerceTransactions = 0;
    m_totalPanicEvents = 0;

    InitializeDefaultWorkplaces();
    InitializeDefaultShops();
    InitializeDefaultTransitSubways();
    InitializeDefaultTrafficGrid();
    PopulateDefaultCitizens(120);
    InitializeDefaultRumors();
    sEmergentAIMgr.Initialize();

    if (Has3DWorldSupport()) {
        SpawnPhysicalCitizens();
    }

    INFO_LOG(format("CityLifeManager: Initialized with %1% Citizens, %2% Workplaces, %3% Shops, %4% Subway Stations, %5% Trains, %6% Vehicles.")
        % m_citizens.size() % m_workplaces.size() % m_shops.size() % m_stations.size() % m_trains.size() % m_vehicles.size());
}

void CityLifeManager::SpawnPhysicalCitizens()
{
    if (!Has3DWorldSupport()) return;

    for (auto& pair : m_citizens) {
        BluepillCitizen& c = pair.second;
        if (c.botGoId == 0) {
            auto bot = sBotMgr.SpawnSingleBot((float)c.homeLocation.x, (float)c.homeLocation.y, (float)c.homeLocation.z, FACTION_NONE);
            if (bot) {
                c.botGoId = bot->GetPlayerGoId();
                c.currentLocation = c.homeLocation;
                if (auto po = sObjMgr.getGOPtrSafe(c.botGoId)) {
                    po->setHandle(c.name);
                    po->setFactionName("Civilian");
                    po->giveItem(1001 + (c.id % 20));
                }
            }
        }
    }
    m_physicalSpawnsActive = true;
    DEBUG_LOG(format("CityLifeManager: Spawned %1% physical civilian bots in 3D world.") % m_citizens.size());
}

void CityLifeManager::DespawnPhysicalCitizens()
{
    if (!Has3DWorldSupport()) return;

    for (auto& pair : m_citizens) {
        BluepillCitizen& c = pair.second;
        if (c.botGoId != 0) {
            sObjMgr.QueueDeletion(c.botGoId);
            c.botGoId = 0;
        }
    }
    m_physicalSpawnsActive = false;
}

void CityLifeManager::InitializeDefaultWorkplaces()
{
    m_workplaces.clear();

    auto AddWorkplace = [this](uint32 id, const std::string& name, const std::string& type, uint32 dist, LocationVector loc, uint32 cap, uint32 wage) {
        WorkplaceEstablishment w;
        w.id = id;
        w.name = name;
        w.typeDesc = type;
        w.districtId = dist;
        w.districtName = GetDistrictName(dist);
        w.location = loc;
        w.capacity = cap;
        w.hourlyWageInfo = wage;
        w.isLockedDown = false;
        w.economicOutputCredits = 0.0f;
        m_workplaces[id] = w;
    };

    // 8 Major Megacity Workplaces across all districts
    AddWorkplace(1, "Metacortex Cybernetics & Software HQ", "Software & Cyber-Architecture", 2, LocationVector(17043.0, 495.0, 2398.0), 50, 65);
    AddWorkplace(2, "Corrington Financial Exchange", "Financial Trading & Banking", 2, LocationVector(12500.0, 300.0, -15000.0), 40, 75);
    AddWorkplace(3, "Pier 44 International Docks & Logistics", "Maritime Freight Logistics", 1, LocationVector(-45000.0, 95.0, -95000.0), 45, 45);
    AddWorkplace(4, "Creston Heavy Industrial Foundry", "Metalwork & Heavy Fabrication", 1, LocationVector(-28000.0, 95.0, 15000.0), 35, 40);
    AddWorkplace(5, "St. Jude Municipal Hospital & Trauma Center", "Healthcare & Emergency Trauma", 3, LocationVector(85000.0, 250.0, -45000.0), 30, 60);
    AddWorkplace(6, "Richland Civic Administration Center", "Municipal Government & Planning", 4, LocationVector(75000.0, 200.0, -135000.0), 35, 55);
    AddWorkplace(7, "Morrell Textile & Assembly Works", "Synthetic Garments & Assembly", 1, LocationVector(-12000.0, 95.0, -18000.0), 30, 35);
    AddWorkplace(8, "OmniGlobal Data Routing Center", "Telecommunications Grid Core", 2, LocationVector(32000.0, 450.0, -25000.0), 25, 50);
}

void CityLifeManager::InitializeDefaultShops()
{
    m_shops.clear();

    auto AddShop = [this](uint32 id, const std::string& name, const std::string& category, uint32 dist, LocationVector loc, float openH, float closeH, std::vector<ShopItem> items) {
        CommercialShop s;
        s.id = id;
        s.name = name;
        s.categoryName = category;
        s.districtId = dist;
        s.districtName = GetDistrictName(dist);
        s.location = loc;
        s.openingHour = openH;
        s.closingHour = closeH;
        s.isOpen = true;
        s.inventory = items;
        s.registerCash = 3000;
        s.activeShoppersCount = 0;
        s.maxShoppers = 30;
        s.isLockedDown = false;
        s.totalTransactions = 0;
        m_shops[id] = s;
    };

    // Shop 1: Morrell Noodle & Dim Sum Bar (Slums)
    AddShop(1, "Morrell Noodle & Dim Sum Bar", "Restaurant / Food", 1, LocationVector(-6415.0, 95.0, -7121.0), 5.0f, 26.0f, {
        {1001, "Steaming Pork Dumplings", "Food", 12, 60, 60, 0},
        {1002, "Braised Beef Noodle Bowl", "Food", 18, 50, 50, 0},
        {1003, "Hot Jasmine Tea", "Beverage", 5, 80, 80, 0}
    });

    // Shop 2: G-Bistro Fine Continental (Downtown)
    AddShop(2, "G-Bistro Fine Continental", "Fine Dining", 2, LocationVector(39216.0, 95.0, -21475.0), 11.0f, 23.5f, {
        {1011, "Pan-Seared Ribeye Steak", "Food", 65, 30, 30, 0},
        {1012, "Vintage Cabernet Sauvignon", "Beverage", 45, 40, 40, 0},
        {1013, "Single Origin Espresso", "Beverage", 8, 70, 70, 0}
    });

    // Shop 3: Le Vrai Haute Cuisine (International)
    AddShop(3, "Le Vrai Haute Cuisine", "Luxury Dining", 3, LocationVector(82745.0, 695.0, -66760.0), 17.5f, 26.0f, {
        {1021, "Caspian Caviar Blinis", "Food", 120, 20, 20, 0},
        {1022, "Truffle Tagliolini", "Food", 85, 25, 25, 0},
        {1023, "Dom Perignon Champagne", "Beverage", 150, 20, 20, 0}
    });

    // Shop 4: Tabor Corner Bodega & Newsstand (Slums)
    AddShop(4, "Tabor Corner Bodega & Newsstand", "Convenience Store", 1, LocationVector(-36326.0, 95.0, -23953.0), 0.0f, 24.0f, {
        {1031, "Daily Sentinel Newspaper", "Periodical", 3, 100, 100, 0},
        {1032, "Sparkling Cola Can", "Beverage", 4, 120, 120, 0},
        {1033, "Pre-Packaged Ham Sandwich", "Food", 10, 40, 40, 0},
        {1034, "Silver Cloud Cigarettes", "Tobacco", 15, 60, 60, 0}
    });

    // Shop 5: Metacortex Electronics & Code Importers (Downtown)
    AddShop(5, "Metacortex Electronics & Code Importers", "Technology & Cyber", 2, LocationVector(22500.0, 350.0, 5500.0), 8.5f, 20.0f, {
        {1041, "High-Density Data Disk (500TB)", "Hardware", 85, 35, 35, 0},
        {1042, "Sub-Ether Signal Booster", "Electronics", 130, 20, 20, 0},
        {1043, "Multi-Spectrum Optical Scanner", "Tools", 210, 15, 15, 0}
    });

    // Shop 6: Richland Haute Couture Boutique (Richland)
    AddShop(6, "Richland Haute Couture Boutique", "Luxury Apparel", 4, LocationVector(92000.0, 150.0, -138000.0), 10.0f, 21.0f, {
        {1051, "Tailored Onyx Wool Trenchcoat", "Apparel", 350, 15, 15, 0},
        {1052, "Polarized Mirrored Sunglasses", "Apparel", 125, 30, 30, 0},
        {1053, "Nanoweave Evening Suit", "Apparel", 420, 10, 10, 0}
    });

    // Shop 7: St. Jude Pharmacy & Dispensary (International)
    AddShop(7, "St. Jude Pharmacy & Dispensary", "Medical & Pharmaceuticals", 3, LocationVector(86500.0, 200.0, -42000.0), 0.0f, 24.0f, {
        {1061, "Tactical First-Aid Dressing", "Medical", 35, 50, 50, 0},
        {1062, "Sterile Adrenaline Ampoule", "Medical", 75, 30, 30, 0},
        {1063, "Broad-Spectrum Anti-Toxin", "Medical", 90, 25, 25, 0}
    });

    // Shop 8: Westview Surplus & Tool Supply (Slums)
    AddShop(8, "Westview Surplus & Tool Supply", "Hardware & Surplus", 1, LocationVector(-48000.0, 95.0, -152000.0), 8.0f, 18.0f, {
        {1071, "High-Tensile Wire Spool", "Hardware", 25, 40, 40, 0},
        {1072, "Heavy Bypass Lockpick Set", "Tools", 95, 20, 20, 0},
        {1073, "Pneumatic Rivet Gun", "Tools", 160, 15, 15, 0}
    });

    // Shop 9: Richland Artisan Bakery & Cafe (Richland)
    AddShop(9, "Richland Artisan Bakery & Cafe", "Bakery / Cafe", 4, LocationVector(87635.0, 85.0, -117864.0), 6.0f, 19.0f, {
        {1081, "Freshly Baked Brioche", "Food", 8, 70, 70, 0},
        {1082, "Almond Cream Croissant", "Food", 9, 65, 65, 0},
        {1083, "Caramel Macchiato", "Beverage", 7, 90, 90, 0}
    });

    // Shop 10: Downtown Terminal Pawn & Exchange (Downtown)
    AddShop(10, "Downtown Terminal Pawn & Exchange", "Pawn & Antiques", 2, LocationVector(11500.0, 95.0, 8500.0), 10.0f, 19.5f, {
        {1091, "Antique Mechanical Wristwatch", "Valuable", 180, 10, 10, 0},
        {1092, "Uncompiled Fragment Decoder", "Curio", 260, 8, 8, 0},
        {1093, "Solid Sterling Silver Ring", "Valuable", 95, 20, 20, 0}
    });
}

void CityLifeManager::InitializeDefaultTransitSubways()
{
    m_stations.clear();
    m_trains.clear();

    auto AddStation = [this](uint32 id, const std::string& name, SubwayLineId line, uint32 dist, LocationVector loc) {
        SubwayStation s;
        s.id = id;
        s.name = name;
        s.lineId = line;
        s.lineName = GetSubwayLineName(line);
        s.districtId = dist;
        s.districtName = GetDistrictName(dist);
        s.platformLocation = loc;
        s.totalPassengerBoardings = 0;
        m_stations[id] = s;
    };

    // Line 1 Stations (Red Line - Main Trunk)
    AddStation(1, "Westview Slums Terminal", SubwayLineId::Line1_RedLine, 1, LocationVector(-49790.0, 95.0, -159434.0));
    AddStation(2, "Downtown Central Station", SubwayLineId::Line1_RedLine, 2, LocationVector(7737.0, 95.0, 13801.0));
    AddStation(3, "Richland Concourse", SubwayLineId::Line1_RedLine, 4, LocationVector(40267.0, 95.0, -116994.0));

    // Line 2 Stations (Blue Line - East Crosstown)
    AddStation(4, "Morrell Sump Station", SubwayLineId::Line2_BlueLine, 1, LocationVector(-6415.0, 95.0, -7121.0));
    AddStation(5, "International Concourse", SubwayLineId::Line2_BlueLine, 3, LocationVector(77349.0, 695.0, -43966.0));
    AddStation(6, "Corrington Financial Center", SubwayLineId::Line2_BlueLine, 2, LocationVector(14200.0, 250.0, -12500.0));

    // Line 3 Stations (Green Line - Waterfront Loop)
    AddStation(7, "Pier 44 Docks Terminal", SubwayLineId::Line3_GreenLine, 1, LocationVector(-43500.0, 95.0, -92000.0));
    AddStation(8, "Downtown Park Promenade", SubwayLineId::Line3_GreenLine, 2, LocationVector(13211.0, 95.0, -37821.0));
    AddStation(9, "Sakura Gardens Station", SubwayLineId::Line3_GreenLine, 3, LocationVector(51820.0, 95.0, -42922.0));
    AddStation(10, "Creston Tenements Station", SubwayLineId::Line3_GreenLine, 1, LocationVector(-30434.0, 95.0, 20325.0));

    auto AddTrain = [this](uint32 id, SubwayLineId line, const std::vector<uint32>& route) {
        SubwayTrain t;
        t.trainId = id;
        t.lineId = line;
        t.lineName = GetSubwayLineName(line);
        t.routeStationIds = route;
        t.currentStationIndex = 0;
        t.targetStationIndex = 1;
        t.state = SubwayTrainState::TRAIN_STOPPED_AT_STATION;
        t.dwellTimerMs = 0;
        t.dwellDurationMs = 15000;
        t.transitProgress = 0.0f;
        t.transitDurationMs = 40000.0f;
        t.currentPosition = m_stations[route[0]].platformLocation;
        t.maxPassengerCapacity = 100;
        t.isEmergencyHold = false;
        t.totalPassengersMoved = 0;
        m_trains[id] = t;
    };

    // 3 Active Scheduled Trains
    AddTrain(1, SubwayLineId::Line1_RedLine, {1, 2, 3});
    AddTrain(2, SubwayLineId::Line2_BlueLine, {4, 5, 6});
    AddTrain(3, SubwayLineId::Line3_GreenLine, {7, 8, 9, 10});
}

void CityLifeManager::InitializeDefaultTrafficGrid()
{
    m_intersections.clear();
    m_vehicles.clear();

    auto AddIntersection = [this](uint32 id, const std::string& name, uint32 dist, LocationVector loc) {
        TrafficIntersection inter;
        inter.id = id;
        inter.name = name;
        inter.districtId = dist;
        inter.centerLocation = loc;
        inter.lightState = TrafficLightState::LIGHT_GREEN;
        inter.lightTimerMs = 0;
        inter.greenDurationMs = 22000;
        inter.yellowDurationMs = 4000;
        inter.redDurationMs = 24000;
        inter.queuedVehiclesCount = 0;
        m_intersections[id] = inter;
    };

    // 6 Strategic Intersections
    AddIntersection(1, "Metacortex Boulevard & 4th Avenue", 2, LocationVector(15000.0, 200.0, 0.0));
    AddIntersection(2, "Corrington Financial Way & Broad St", 2, LocationVector(10000.0, 150.0, -18000.0));
    AddIntersection(3, "Westview Slums Commercial Arterial", 1, LocationVector(-32000.0, 95.0, -50000.0));
    AddIntersection(4, "Pier 44 Harbor Highway Intersection", 1, LocationVector(-42000.0, 95.0, -88000.0));
    AddIntersection(5, "International Embassy Concourse Circle", 3, LocationVector(80000.0, 300.0, -50000.0));
    AddIntersection(6, "Richland Promenade & High-Rise Boulevard", 4, LocationVector(85000.0, 100.0, -125000.0));

    // Spawn 12 Ambient Road Vehicles
    auto SpawnCar = [this](uint32 id, VehicleClassification type, const std::string& model, const std::string& plate, uint32 dist, LocationVector start, LocationVector dest) {
        CityVehicle v;
        v.vehicleId = id;
        v.type = type;
        v.typeName = GetVehicleTypeName(type);
        v.modelName = model;
        v.licensePlate = plate;
        v.districtId = dist;
        v.currentLocation = start;
        v.destinationLocation = dest;
        v.waypoints = {start, LocationVector((start.x + dest.x) * 0.5, start.y, (start.z + dest.z) * 0.5), dest};
        v.currentWaypointIndex = 0;
        v.speedUnitsPerSec = (type == VehicleClassification::TAXI_CAB ? 550.0f : 420.0f);
        v.isStoppedAtLight = false;
        v.stoppedTimerMs = 0;
        v.passengerCapacity = (type == VehicleClassification::TRANSIT_BUS ? 24 : 4);
        v.isEmergencyResponding = false;
        m_vehicles[id] = v;
    };

    SpawnCar(1, VehicleClassification::TAXI_CAB, "Crown Victoria Yellow Cab", "MX-4401", 2, LocationVector(8000.0, 95.0, 5000.0), LocationVector(25000.0, 95.0, -20000.0));
    SpawnCar(2, VehicleClassification::TAXI_CAB, "Crown Victoria Yellow Cab", "MX-4402", 2, LocationVector(22000.0, 95.0, -15000.0), LocationVector(9000.0, 95.0, 8000.0));
    SpawnCar(3, VehicleClassification::TRANSIT_BUS, "Megacity Transit Bus Line 10", "CT-1088", 2, LocationVector(12000.0, 95.0, 10000.0), LocationVector(35000.0, 95.0, -30000.0));
    SpawnCar(4, VehicleClassification::DELIVERY_VAN, "Freightliner Cargo Express", "DL-9012", 1, LocationVector(-40000.0, 95.0, -85000.0), LocationVector(-15000.0, 95.0, -20000.0));
    SpawnCar(5, VehicleClassification::CIVILIAN_SEDAN, "Chevy Lumina Sedan", "PL-3129", 1, LocationVector(-35000.0, 95.0, -30000.0), LocationVector(-25000.0, 95.0, 10000.0));
    SpawnCar(6, VehicleClassification::CIVILIAN_SEDAN, "Ford Taurus GL", "PL-8874", 3, LocationVector(75000.0, 200.0, -40000.0), LocationVector(95000.0, 200.0, -65000.0));
    SpawnCar(7, VehicleClassification::TAXI_CAB, "Crown Victoria Yellow Cab", "MX-4409", 3, LocationVector(88000.0, 200.0, -60000.0), LocationVector(72000.0, 200.0, -38000.0));
    SpawnCar(8, VehicleClassification::TRANSIT_BUS, "Megacity Transit Bus Line 22", "CT-2204", 4, LocationVector(70000.0, 100.0, -115000.0), LocationVector(98000.0, 100.0, -145000.0));
    SpawnCar(9, VehicleClassification::CIVILIAN_SEDAN, "Lincoln Town Car Executive", "EX-7711", 4, LocationVector(95000.0, 100.0, -140000.0), LocationVector(72000.0, 100.0, -120000.0));
    SpawnCar(10, VehicleClassification::MUNICIPAL_CRUISER, "MMPD Interceptor Cruiser", "MMPD-12", 2, LocationVector(18000.0, 95.0, 2000.0), LocationVector(30000.0, 95.0, -18000.0));
    SpawnCar(11, VehicleClassification::MUNICIPAL_CRUISER, "MMPD Harbor Beat Cruiser", "MMPD-44", 1, LocationVector(-42000.0, 95.0, -90000.0), LocationVector(-30000.0, 95.0, -45000.0));
    SpawnCar(12, VehicleClassification::DELIVERY_VAN, "St. Jude Medical Courier", "MD-5520", 3, LocationVector(82000.0, 200.0, -48000.0), LocationVector(88000.0, 200.0, -41000.0));

    m_nextVehicleId = 13;
}

void CityLifeManager::PopulateDefaultCitizens(size_t count)
{
    m_citizens.clear();

    // Archetype distribution profiles
    const std::vector<std::string> firstNamesMale = {"Thomas", "Michael", "David", "James", "Robert", "John", "Richard", "Charles", "Daniel", "Matthew", "Lucas", "Aaron", "Simon", "Walter", "Ethan"};
    const std::vector<std::string> firstNamesFemale = {"Sarah", "Jennifer", "Emily", "Rachel", "Jessica", "Laura", "Claire", "Amanda", "Megan", "Hannah", "Elena", "Sophia", "Maya", "Natalie", "Rebecca"};
    const std::vector<std::string> lastNames = {"Anderson", "Miller", "Taylor", "Wong", "Vance", "Kowalski", "Sterling", "Cross", "Chen", "Reynolds", "Mercer", "Blackwood", "Frost", "Castillo", "Novak", "Sinclair", "Hayes", "Drake"};

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> traitDist(0.15f, 0.95f);
    std::uniform_int_distribution<int> genderDist(0, 1);

    for (size_t i = 1; i <= count; ++i) {
        BluepillCitizen c;
        c.id = static_cast<uint32>(i);
        bool isMale = genderDist(rng) == 0;
        c.gender = isMale ? "Male" : "Female";
        const auto& fnPool = isMale ? firstNamesMale : firstNamesFemale;
        c.name = fnPool[rng() % fnPool.size()] + " " + lastNames[rng() % lastNames.size()];

        // Round-robin archetype assignment
        c.archetype = static_cast<CivilianArchetype>(i % 8);
        c.archetypeName = GetArchetypeName(c.archetype);

        // Biometric & OCEAN Personality tailoring
        c.traits.openness = traitDist(rng);
        c.traits.conscientiousness = traitDist(rng);
        c.traits.extraversion = traitDist(rng);
        c.traits.agreeableness = traitDist(rng);
        c.traits.neuroticism = traitDist(rng);

        // Archetype OCEAN adjustments
        switch (c.archetype) {
            case CivilianArchetype::CorporateSuit:
                c.districtId = 2;
                c.traits.conscientiousness = std::clamp(c.traits.conscientiousness + 0.3f, 0.6f, 0.95f);
                c.workplaceId = (i % 2 == 0) ? 1 : 2; // Metacortex or Financial
                c.preferredShopId = (i % 2 == 0) ? 2 : 5; // G-Bistro or Electronics
                c.shift = WorkShiftType::ShiftDay;
                c.homeLocation = LocationVector(9844.0, 1295.0, 1314.0); // Downtown High-Rise Lofts
                c.homeApartmentName = "Metacortex Executive Suites #404";
                c.drives.walletInfo = 850 + (rng() % 500);
                break;
            case CivilianArchetype::IndustrialBlueCollar:
                c.districtId = 1;
                c.traits.conscientiousness = std::clamp(c.traits.conscientiousness + 0.1f, 0.4f, 0.85f);
                c.workplaceId = (i % 2 == 0) ? 3 : 4; // Pier 44 or Foundry
                c.preferredShopId = 1; // Noodle Bar
                c.shift = (i % 3 == 0) ? WorkShiftType::ShiftSwing : WorkShiftType::ShiftDay;
                c.homeLocation = LocationVector(-30434.0, 95.0, 20325.0); // Creston Tenements
                c.homeApartmentName = "Creston Tenement Block B-12";
                c.drives.walletInfo = 350 + (rng() % 300);
                break;
            case CivilianArchetype::ServiceRetailWorker:
                c.districtId = (i % 4) + 1;
                c.traits.agreeableness = std::clamp(c.traits.agreeableness + 0.25f, 0.6f, 0.95f);
                c.workplaceId = 7; // Morrell Textile or retail
                c.preferredShopId = 4; // Bodega
                c.shift = WorkShiftType::ShiftDay;
                c.homeLocation = LocationVector(-32000.0, 95.0, -25000.0);
                c.homeApartmentName = "Westview Walk-Up Flats #2";
                c.drives.walletInfo = 280 + (rng() % 250);
                break;
            case CivilianArchetype::AcademicStudent:
                c.districtId = 3;
                c.traits.openness = std::clamp(c.traits.openness + 0.35f, 0.7f, 0.98f);
                c.workplaceId = 8; // Data routing / library
                c.preferredShopId = 9; // Bakery
                c.shift = WorkShiftType::ShiftFlexible;
                c.homeLocation = LocationVector(96190.0, 95.0, -85320.0);
                c.homeApartmentName = "International Dormitory Annex #7";
                c.drives.walletInfo = 180 + (rng() % 150);
                break;
            case CivilianArchetype::NightlifeClubber:
                c.districtId = 3;
                c.traits.extraversion = std::clamp(c.traits.extraversion + 0.4f, 0.75f, 0.99f);
                c.traits.conscientiousness = std::clamp(c.traits.conscientiousness - 0.3f, 0.15f, 0.5f);
                c.workplaceId = 0; // Unemployed / socialite
                c.preferredShopId = 3; // Le Vrai
                c.shift = WorkShiftType::ShiftGraveyard;
                c.homeLocation = LocationVector(85000.0, 200.0, -70000.0);
                c.homeApartmentName = "Club Hel Penthouse Loft";
                c.drives.walletInfo = 950 + (rng() % 1200);
                break;
            case CivilianArchetype::UrbanDrifter:
                c.districtId = 1;
                c.traits.conscientiousness = 0.25f;
                c.workplaceId = 0;
                c.preferredShopId = 4; // Bodega
                c.shift = WorkShiftType::ShiftFlexible;
                c.homeLocation = LocationVector(-36326.0, 95.0, -23953.0); // Tabor Park
                c.homeApartmentName = "Tabor Park Shelter";
                c.drives.walletInfo = 45 + (rng() % 60);
                break;
            case CivilianArchetype::MunicipalCivilServant:
                c.districtId = 4;
                c.traits.conscientiousness = std::clamp(c.traits.conscientiousness + 0.3f, 0.8f, 0.98f);
                c.workplaceId = 6; // Richland Administration
                c.preferredShopId = 9; // Bakery
                c.shift = WorkShiftType::ShiftDay;
                c.homeLocation = LocationVector(107619.0, 95.0, -149092.0);
                c.homeApartmentName = "Richland Civic Residences #101";
                c.drives.walletInfo = 650 + (rng() % 400);
                break;
            case CivilianArchetype::MedicalStaff:
                c.districtId = 3;
                c.traits.agreeableness = std::clamp(c.traits.agreeableness + 0.3f, 0.7f, 0.95f);
                c.workplaceId = 5; // St. Jude Hospital
                c.preferredShopId = 7; // Pharmacy
                c.shift = (i % 2 == 0) ? WorkShiftType::ShiftDay : WorkShiftType::ShiftSwing;
                c.homeLocation = LocationVector(90000.0, 150.0, -55000.0);
                c.homeApartmentName = "St. Jude Staff Apartments #3B";
                c.drives.walletInfo = 720 + (rng() % 350);
                break;
        }

        c.districtName = GetDistrictName(c.districtId);
        c.currentLocation = c.homeLocation;
        c.destinationLocation = c.homeLocation;
        c.currentRoutine = RoutineScheduleState::Sleeping;
        c.currentStreetActivity = StreetActivityType::Strolling;
        c.drives.hunger = 0.2f + (rng() % 30) / 100.0f;
        c.drives.fatigue = 0.1f + (rng() % 20) / 100.0f;
        c.drives.socialNeed = 0.3f + (rng() % 40) / 100.0f;
        c.drives.stress = 0.05f;

        if (c.workplaceId != 0 && m_workplaces.find(c.workplaceId) != m_workplaces.end()) {
            c.workplaceName = m_workplaces[c.workplaceId].name;
            m_workplaces[c.workplaceId].assignedCitizenIds.push_back(c.id);
        }

        m_citizens[c.id] = c;
    }

    m_nextCitizenId = static_cast<uint32>(count + 1);

    // ------------------------------------------------------------------------
    // Seed NPC Social Life Engine Profiles & Emergent Social Network
    // ------------------------------------------------------------------------
    sSocialEngine.Initialize();
    for (auto& pair : m_citizens) {
        BluepillCitizen& c = pair.second;
        auto& profile = sSocialEngine.GetOrCreateProfile(c.id, c.name, true);
        if (c.preferredShopId != 0 && m_shops.find(c.preferredShopId) != m_shops.end()) {
            profile.preferredHangouts.push_back(m_shops[c.preferredShopId].name);
        }
        if (!c.homeApartmentName.empty()) {
            profile.preferredHangouts.push_back(c.homeApartmentName);
        }
        
        // Initial Episodic Memories: Residence & Workplace
        sSocialEngine.RecordEpisodicMemory(c.id, "Moving into " + c.homeApartmentName,
            "Signed tenancy agreement and unboxed belongings under flickering fluorescents.",
            MemoryCategory::DAILY_PLEASURE, 0.4f, 0.5f, 0, c.homeApartmentName, false);

        if (!c.workplaceName.empty()) {
            sSocialEngine.RecordEpisodicMemory(c.id, "First Day at " + c.workplaceName,
                "Began employment shift and oriented to corporate workflow directives.",
                MemoryCategory::WORKPLACE_ACHIEVEMENT, 0.5f, 0.6f, 0, c.workplaceName, false);
        }
    }

    // Seed Coworker Edges for citizens assigned to the same workplace
    for (const auto& wpPair : m_workplaces) {
        const auto& workerIds = wpPair.second.assignedCitizenIds;
        for (size_t i = 0; i < workerIds.size(); ++i) {
            for (size_t j = i + 1; j < workerIds.size(); ++j) {
                uint32 idA = workerIds[i];
                uint32 idB = workerIds[j];
                sSocialEngine.FormFriendship(idA, idB, FriendshipTier::Coworker);
            }
        }
    }

    // Seed Romance and Friendships across compatible citizens
    for (size_t i = 1; i <= count; i += 4) {
        uint32 idA = static_cast<uint32>(i);
        uint32 idB = static_cast<uint32>(i + 1 <= count ? i + 1 : 1);
        if (idA != idB && m_citizens.find(idA) != m_citizens.end() && m_citizens.find(idB) != m_citizens.end()) {
            float compat = sSocialEngine.CalculateCompatibility(m_citizens[idA].traits, m_citizens[idB].traits);
            if (compat >= 0.55f) {
                sSocialEngine.InitiateFlirtation(idA, idB, compat);
                std::string venue = (m_citizens[idA].preferredShopId != 0 && m_shops.find(m_citizens[idA].preferredShopId) != m_shops.end())
                    ? m_shops[m_citizens[idA].preferredShopId].name : "Morrell Noodle Bar";
                sSocialEngine.ScheduleAndExecuteDate(idA, idB, venue, "Romantic dinner date");
                if (compat >= 0.70f) {
                    sSocialEngine.DeepenCommitment(idA, idB);
                }
            } else {
                sSocialEngine.FormFriendship(idA, idB, FriendshipTier::CasualFriend);
            }
        }
    }

    // ------------------------------------------------------------------------
    // Seed NPC Families, Life Dreams & Career Ladders
    // ------------------------------------------------------------------------
    sFamilyDreamsEngine.Initialize();

    // 1. Assign Career Tracks and Initial Positions based on Archetype
    for (auto& pair : m_citizens) {
        BluepillCitizen& c = pair.second;
        CareerTrack track = CareerTrack::CorporateTech;
        std::string jobTitle = "Office Associate";
        JobPositionLevel level = JobPositionLevel::Level1_JuniorAssociate;
        uint32 wage = 45;

        switch (c.archetype) {
            case CivilianArchetype::CorporateSuit:
                track = CareerTrack::CorporateTech;
                level = (c.id % 5 == 0) ? JobPositionLevel::Level3_SeniorLead : JobPositionLevel::Level1_JuniorAssociate;
                jobTitle = (level == JobPositionLevel::Level3_SeniorLead) ? "Senior Systems Architect" : "Systems Analyst";
                wage = 65;
                break;
            case CivilianArchetype::IndustrialBlueCollar:
                track = CareerTrack::IndustrialManufacturing;
                level = (c.id % 6 == 0) ? JobPositionLevel::Level3_SeniorLead : JobPositionLevel::Level1_JuniorAssociate;
                jobTitle = (level == JobPositionLevel::Level3_SeniorLead) ? "Shop Floor Supervisor" : "Heavy Machine Operator";
                wage = 40;
                break;
            case CivilianArchetype::ServiceRetailWorker:
                track = CareerTrack::RetailCulinaryCommerce;
                jobTitle = "Store Manager & Barista";
                wage = 35;
                break;
            case CivilianArchetype::AcademicStudent:
                track = CareerTrack::CorporateTech;
                level = JobPositionLevel::Level0_EntryIntern;
                jobTitle = "Graduate Research Intern";
                wage = 25;
                break;
            case CivilianArchetype::NightlifeClubber:
                track = CareerTrack::NightlifeEntertainment;
                jobTitle = "Club Host & Promoter";
                wage = 50;
                break;
            case CivilianArchetype::UrbanDrifter:
                track = CareerTrack::RetailCulinaryCommerce;
                level = JobPositionLevel::Level0_EntryIntern;
                jobTitle = "Freelance Courier";
                wage = 20;
                break;
            case CivilianArchetype::MunicipalCivilServant:
                track = CareerTrack::MunicipalGovernment;
                level = JobPositionLevel::Level2_MidLevelSpecialist;
                jobTitle = "Municipal Records Administrator";
                wage = 55;
                break;
            case CivilianArchetype::MedicalStaff:
                track = CareerTrack::MedicalHealthcare;
                level = (c.id % 3 == 0) ? JobPositionLevel::Level3_SeniorLead : JobPositionLevel::Level2_MidLevelSpecialist;
                jobTitle = (level == JobPositionLevel::Level3_SeniorLead) ? "Attending Physician" : "Critical Care Specialist";
                wage = 85;
                break;
        }

        std::string wpName = c.workplaceName.empty() ? "Megacity Municipal Services" : c.workplaceName;
        sFamilyDreamsEngine.AssignCareer(c.id, track, level, jobTitle, wpName, c.workplaceId, wage);

        // 2. Assign Lifelong Dreams matching citizen persona
        LifeDreamType dream = LifeDreamType::RaiseFlourishingFamily;
        switch (c.archetype) {
            case CivilianArchetype::CorporateSuit:
                dream = (c.traits.conscientiousness > 0.7f) ? LifeDreamType::BecomeMetacortexVP : LifeDreamType::BuyRichlandHighRise;
                break;
            case CivilianArchetype::IndustrialBlueCollar:
                dream = (c.traits.agreeableness > 0.6f) ? LifeDreamType::RaiseFlourishingFamily : LifeDreamType::ClearHouseholdDebt;
                break;
            case CivilianArchetype::ServiceRetailWorker:
                dream = LifeDreamType::OpenArtisanBakery;
                break;
            case CivilianArchetype::MedicalStaff:
                dream = LifeDreamType::RaiseFlourishingFamily;
                break;
            default:
                dream = static_cast<LifeDreamType>(c.id % 5);
                break;
        }
        sFamilyDreamsEngine.AssignDream(c.id, dream);
    }

    // 3. Form Family Households among married/dating couples and assign dependents
    for (size_t i = 1; i <= count; i += 4) {
        uint32 idA = static_cast<uint32>(i);
        uint32 idB = static_cast<uint32>(i + 1 <= count ? i + 1 : 1);
        if (idA != idB && m_citizens.find(idA) != m_citizens.end() && m_citizens.find(idB) != m_citizens.end()) {
            const auto* profA = sSocialEngine.GetProfile(idA);
            if (profA && (profA->romanceStatus == RomanceStage::Dating || profA->romanceStatus == RomanceStage::InLove || profA->romanceStatus == RomanceStage::CommittedPartner || profA->romanceStatus == RomanceStage::Married)) {
                std::string addr = m_citizens[idA].homeApartmentName.empty() ? "Megacity Residential Apartments" : m_citizens[idA].homeApartmentName;
                sFamilyDreamsEngine.FormHouseholdFromRomance(idA, idB, m_citizens[idA].name, m_citizens[idB].name, addr, m_citizens[idA].homeLocation);

                // If household formed, add a child or younger sibling
                auto* h = sFamilyDreamsEngine.GetHouseholdByMember(idA);
                if (h && (i + 2 <= count) && m_citizens.find(static_cast<uint32>(i + 2)) != m_citizens.end()) {
                    uint32 kidId = static_cast<uint32>(i + 2);
                    sFamilyDreamsEngine.AddKinshipMember(h->householdId, kidId, m_citizens[kidId].name, KinshipRole::Child, 12 + (static_cast<uint32>(i) % 8));
                }
            }
        }
    }
}

void CityLifeManager::InitializeDefaultRumors()
{
    m_rumors.clear();

    auto AddRumor = [this](RumorTopic topic, const std::string& headline, const std::string& content, uint32 dist) {
        AmbientRumor r;
        r.id = m_nextRumorId++;
        r.topic = topic;
        r.headline = headline;
        r.content = content;
        r.originDistrictId = dist;
        r.timestampMs = getMSTime();
        r.spreadCount = 1;
        m_rumors.push_back(r);
    };

    AddRumor(RumorTopic::RUMOR_WEATHER_GLITCH, "Skybox Rain Anomaly", "Pedestrians in Downtown reported seeing emerald code cascades shimmering in the rain clouds.", 2);
    AddRumor(RumorTopic::RUMOR_PUNISHER_SIGHTING, "Vigilante White Skull Sighting", "Witnesses saw a heavily armed vigilante in black assault armor raiding an illegal warehouse in Slums.", 1);
    AddRumor(RumorTopic::RUMOR_SYNDICATE_TURF_WAR, "Pier 44 Smuggling Interception", "Dockworkers whisper that an armored convoy carrying military munitions was ambushed near the freight terminal.", 1);
    AddRumor(RumorTopic::RUMOR_POLICE_CRACKDOWN, "MMPD Internal Affairs Investigation", "Radio broadcasts claim Internal Affairs has issued warrants targeting crooked captains in Harbor Division.", 3);
    AddRumor(RumorTopic::RUMOR_CORPORATE_EXPLOITATION, "Metacortex Core Audit", "Software engineers are working overtime through the night on an unannounced neural operating system update.", 2);
    AddRumor(RumorTopic::RUMOR_MATRIX_ANOMALY, "The Deja-Vu Black Cat", "Street buskers at Tabor Park swear they saw the exact same black cat walk past twice within three seconds.", 1);
}

// ============================================================================
// Simulation Loop & Sub-System Updates
// ============================================================================

void CityLifeManager::Update(uint32 deltaMs)
{
    ProcessPendingAsyncEvents();

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        UpdateCircadianSchedule(deltaMs);
        UpdateCitizens(deltaMs);
        UpdateWorkplaces(deltaMs);
        UpdateShops(deltaMs);
        UpdateSubwayTransit(deltaMs);
        UpdateRoadTraffic(deltaMs);
        UpdateRumorPool(deltaMs);
    }

    if (NPCSocialLifeEngine::getSingletonPtr()) {
        sSocialEngine.UpdateCircadianSocialCycle(static_cast<uint32>(m_simulatedHour * 60.0f));
    }

    // Call EmergentAI updates OUTSIDE CityLifeManager mutex to prevent cross-engine deadlock
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.UpdateManagedCitizens(deltaMs);
    }
}

void CityLifeManager::SetSimulatedHour(float hour)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_simulatedHour = std::fmod(hour, 24.0f);
    if (m_simulatedHour < 0.0f) m_simulatedHour += 24.0f;
    m_useWeatherSystemHour = false; // Override weather system sync for manual/testing control
}

std::string CityLifeManager::GetTimeString() const
{
    uint32 h = GetHourInt();
    uint32 m = GetMinuteInt();
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << h << ":" << std::setw(2) << m;
    return ss.str();
}

std::string CityLifeManager::GetCircadianPhaseName() const
{
    float h = m_simulatedHour;
    if (h >= 0.0f && h < 5.0f) return "Nocturnal Shift & Rest";
    if (h >= 5.0f && h < 7.0f) return "Dawn Awakening & Breakfast Rush";
    if (h >= 7.0f && h < 9.0f) return "Morning Commute Rush Hour";
    if (h >= 9.0f && h < 12.0f) return "Morning Work & Production Shift";
    if (h >= 12.0f && h < 13.5f) return "Midday Lunch Rush & Dining";
    if (h >= 13.5f && h < 17.0f) return "Afternoon Work & Commerce";
    if (h >= 17.0f && h < 19.0f) return "Evening Outbound Commute Rush";
    if (h >= 19.0f && h < 22.0f) return "Evening Leisure, Dining & Shopping";
    return "Nightlife Clubs & Home Wind-Down";
}

void CityLifeManager::UpdateCircadianSchedule(uint32 deltaMs)
{
    // Advance simulated hour
    // 1 real second = 12 simulation seconds => 1 sim hour = 300 real seconds (5 mins)
    // 1 sim day = 120 real minutes
    if (m_useWeatherSystemHour && WeatherSystem::getSingletonPtr()) {
        m_simulatedHour = sWeatherSys.GetMatrixHour();
    } else {
        float deltaHours = (static_cast<float>(deltaMs) / 1000.0f) * (m_timeScaleFactor / 3600.0f);
        m_simulatedHour = std::fmod(m_simulatedHour + deltaHours, 24.0f);
    }
}

void CityLifeManager::UpdateCitizens(uint32 deltaMs)
{
    float currentHour = m_simulatedHour;
    float deltaSec = static_cast<float>(deltaMs) / 1000.0f;

    for (auto& pair : m_citizens) {
        BluepillCitizen& c = pair.second;

        // 1. Biometric Drive Evolution
        c.drives.hunger = std::min(1.0f, c.drives.hunger + 0.00002f * deltaMs);
        c.drives.fatigue = std::min(1.0f, c.drives.fatigue + 0.000015f * deltaMs);
        c.drives.socialNeed = std::min(1.0f, c.drives.socialNeed + 0.00001f * deltaMs * c.traits.extraversion);

        // 2. Panic Expiration
        if (c.currentRoutine == RoutineScheduleState::Panicking) {
            if (c.panicTimerMs > deltaMs) {
                c.panicTimerMs -= deltaMs;
            } else {
                c.panicTimerMs = 0;
                c.isSheltered = false;
                c.currentRoutine = RoutineScheduleState::Leisure;
                c.drives.stress = std::max(0.1f, c.drives.stress - 0.4f);
            }
            continue; // Skip standard schedule while panicked
        }

        // 3. Circadian Routine State Decision based on Shift & Hour
        RoutineScheduleState targetRoutine = RoutineScheduleState::Sleeping;

        if (c.shift == WorkShiftType::ShiftDay) {
            if (currentHour >= 0.0f && currentHour < 5.0f) {
                targetRoutine = RoutineScheduleState::Sleeping;
            } else if (currentHour >= 5.0f && currentHour < 7.0f) {
                targetRoutine = RoutineScheduleState::Breakfast;
            } else if (currentHour >= 7.0f && currentHour < 9.0f) {
                targetRoutine = RoutineScheduleState::CommuteToWork;
            } else if (currentHour >= 9.0f && currentHour < 12.0f) {
                targetRoutine = RoutineScheduleState::Working;
            } else if (currentHour >= 12.0f && currentHour < 13.5f) {
                targetRoutine = RoutineScheduleState::LunchBreak;
            } else if (currentHour >= 13.5f && currentHour < 17.0f) {
                targetRoutine = RoutineScheduleState::AfternoonWork;
            } else if (currentHour >= 17.0f && currentHour < 19.0f) {
                targetRoutine = RoutineScheduleState::CommuteHome;
            } else if (currentHour >= 19.0f && currentHour < 22.0f) {
                targetRoutine = (c.drives.hunger > 0.6f) ? RoutineScheduleState::Dining : RoutineScheduleState::Leisure;
            } else {
                targetRoutine = RoutineScheduleState::Sleeping;
            }
        } else if (c.shift == WorkShiftType::ShiftSwing) {
            if (currentHour >= 1.0f && currentHour < 8.0f) {
                targetRoutine = RoutineScheduleState::Sleeping;
            } else if (currentHour >= 8.0f && currentHour < 15.0f) {
                targetRoutine = RoutineScheduleState::Leisure;
            } else if (currentHour >= 15.0f && currentHour < 16.0f) {
                targetRoutine = RoutineScheduleState::CommuteToWork;
            } else if (currentHour >= 16.0f && currentHour < 20.0f) {
                targetRoutine = RoutineScheduleState::Working;
            } else if (currentHour >= 20.0f && currentHour < 21.0f) {
                targetRoutine = RoutineScheduleState::Dining;
            } else if (currentHour >= 21.0f && currentHour < 24.0f) {
                targetRoutine = RoutineScheduleState::AfternoonWork;
            } else {
                targetRoutine = RoutineScheduleState::CommuteHome;
            }
        } else if (c.shift == WorkShiftType::ShiftGraveyard) {
            if (currentHour >= 9.0f && currentHour < 16.0f) {
                targetRoutine = RoutineScheduleState::Sleeping;
            } else if (currentHour >= 16.0f && currentHour < 23.0f) {
                targetRoutine = (c.archetype == CivilianArchetype::NightlifeClubber) ? RoutineScheduleState::Nightclubbing : RoutineScheduleState::Leisure;
            } else if (currentHour >= 23.0f || currentHour < 0.0f) {
                targetRoutine = RoutineScheduleState::CommuteToWork;
            } else if (currentHour >= 0.0f && currentHour < 4.0f) {
                targetRoutine = (c.archetype == CivilianArchetype::NightlifeClubber) ? RoutineScheduleState::Nightclubbing : RoutineScheduleState::Working;
            } else if (currentHour >= 4.0f && currentHour < 5.0f) {
                targetRoutine = RoutineScheduleState::Breakfast;
            } else {
                targetRoutine = RoutineScheduleState::CommuteHome;
            }
        } else { // Flexible / Drifter
            if (currentHour >= 2.0f && currentHour < 7.0f) {
                targetRoutine = RoutineScheduleState::Sleeping;
            } else if (c.drives.hunger > 0.7f) {
                targetRoutine = RoutineScheduleState::Dining;
            } else if (currentHour >= 21.0f || currentHour < 2.0f) {
                targetRoutine = (c.traits.extraversion > 0.6f) ? RoutineScheduleState::Nightclubbing : RoutineScheduleState::Leisure;
            } else {
                targetRoutine = RoutineScheduleState::Leisure;
            }
        }

        c.currentRoutine = targetRoutine;

        // 4. Target Destination Calculation
        LocationVector targetLoc = c.homeLocation;
        if (c.currentRoutine == RoutineScheduleState::Working || c.currentRoutine == RoutineScheduleState::AfternoonWork) {
            if (c.workplaceId != 0 && m_workplaces.find(c.workplaceId) != m_workplaces.end()) {
                targetLoc = m_workplaces[c.workplaceId].location;
            }
        } else if (c.currentRoutine == RoutineScheduleState::Breakfast || c.currentRoutine == RoutineScheduleState::LunchBreak || c.currentRoutine == RoutineScheduleState::Dining || c.currentRoutine == RoutineScheduleState::Shopping) {
            if (c.preferredShopId != 0 && m_shops.find(c.preferredShopId) != m_shops.end()) {
                targetLoc = m_shops[c.preferredShopId].location;
            }
        } else if (c.currentRoutine == RoutineScheduleState::Nightclubbing) {
            targetLoc = LocationVector(-67862.0, 95.0, 16314.0); // Club Hel
        } else if (c.currentRoutine == RoutineScheduleState::Sleeping) {
            targetLoc = c.homeLocation;
        }

        c.destinationLocation = targetLoc;

        // 5. Level of Detail (LOD) Movement & Macro Translation Step
        // When assigned to stationary routine states (working, sleeping, dining, clubbing),
        // Macro LOD simulation places them at their venue destination
        if (c.currentRoutine == RoutineScheduleState::Working || c.currentRoutine == RoutineScheduleState::AfternoonWork ||
            c.currentRoutine == RoutineScheduleState::Sleeping || c.currentRoutine == RoutineScheduleState::Breakfast ||
            c.currentRoutine == RoutineScheduleState::LunchBreak || c.currentRoutine == RoutineScheduleState::Dining ||
            c.currentRoutine == RoutineScheduleState::Shopping || c.currentRoutine == RoutineScheduleState::Nightclubbing) {
            c.currentLocation = c.destinationLocation;
        } else {
            float dx = c.destinationLocation.x - c.currentLocation.x;
            float dz = c.destinationLocation.z - c.currentLocation.z;
            float distSq = dx * dx + dz * dz;

            if (distSq > 10000.0f) { // More than 100 units from destination
                float dist = std::sqrt(distSq);
                float step = c.movementSpeed * 100.0f * deltaSec;
                if (step > dist) step = dist;
                c.currentLocation.x += (dx / dist) * step;
                c.currentLocation.z += (dz / dist) * step;
            }
        }

        // Weather umbrella reactivity
        if (WeatherSystem::getSingletonPtr() && sWeatherSys.IsRaining()) {
            c.isUsingUmbrella = true;
        } else {
            c.isUsingUmbrella = false;
        }

        // Physical 3D World Manifestation Sync
        if (c.botGoId != 0 && Has3DWorldSupport()) {
            if (auto bot = sBotMgr.GetBotByGOID(c.botGoId)) {
                if (c.currentRoutine == RoutineScheduleState::Panicking) {
                    bot->SetPanicking(true);
                    bot->Emote(50); // Cower
                    bot->MoveTo((float)c.destinationLocation.x, (float)c.destinationLocation.y, (float)c.destinationLocation.z);
                } else {
                    bot->SetPanicking(false);
                    if (c.currentRoutine == RoutineScheduleState::CommuteToWork || 
                        c.currentRoutine == RoutineScheduleState::CommuteHome ||
                        c.currentRoutine == RoutineScheduleState::Shopping ||
                        c.currentRoutine == RoutineScheduleState::Leisure) {
                        bot->MoveTo((float)c.destinationLocation.x, (float)c.destinationLocation.y, (float)c.destinationLocation.z);
                    } else if (c.currentRoutine == RoutineScheduleState::Breakfast || 
                               c.currentRoutine == RoutineScheduleState::LunchBreak || 
                               c.currentRoutine == RoutineScheduleState::Dining) {
                        bot->Emote(10); // Dine
                    } else if (c.currentRoutine == RoutineScheduleState::Nightclubbing) {
                        bot->Emote(20); // Dance
                    }
                }
            }
        }

        // Reached destination: execute local activities
        if (c.currentRoutine == RoutineScheduleState::Breakfast || c.currentRoutine == RoutineScheduleState::LunchBreak || c.currentRoutine == RoutineScheduleState::Dining) {
            if (c.drives.hunger > 0.4f && c.preferredShopId != 0) {
                uint32 cost = 0;
                if (ProcessShopPurchase(c.id, c.preferredShopId, 1001, cost) || ProcessShopPurchase(c.id, c.preferredShopId, 1033, cost) || ProcessShopPurchase(c.id, c.preferredShopId, 1081, cost)) {
                    c.drives.hunger = 0.1f;
                    c.totalShopsVisited++;
                }
            }
        } else if (c.currentRoutine == RoutineScheduleState::Sleeping) {
            c.drives.fatigue = std::max(0.0f, c.drives.fatigue - 0.0001f * deltaMs);
        }

        // Social Life Interaction during Lunch / Dining / Leisure
        if (c.currentRoutine == RoutineScheduleState::LunchBreak || c.currentRoutine == RoutineScheduleState::Dining || c.currentRoutine == RoutineScheduleState::Leisure) {
            const auto* prof = sSocialEngine.GetProfile(c.id);
            if (prof) {
                // If partner exists, occasionally execute date meetup
                if (prof->partnerEntityId != 0 && (c.id % 5 == 0)) {
                    std::string venue = (c.preferredShopId != 0 && m_shops.find(c.preferredShopId) != m_shops.end()) ? m_shops[c.preferredShopId].name : "Downtown Bistro";
                    sSocialEngine.ScheduleAndExecuteDate(c.id, prof->partnerEntityId, venue, "Shared evening meal");
                    c.drives.socialNeed = std::max(0.0f, c.drives.socialNeed - 0.3f);
                }
                // Stress venting if stressed
                if (c.drives.stress > 0.3f) {
                    for (const auto& relPair : prof->relationships) {
                        if (relPair.second.friendship >= FriendshipTier::Coworker || relPair.second.romance >= RomanceStage::Dating) {
                            sSocialEngine.VentWorkplaceStress(c.id, relPair.first);
                            c.drives.stress = std::max(0.05f, c.drives.stress - 0.25f);
                            break;
                        }
                    }
                }
            }
        }

        // Family Domestic & Career Dynamics
        if (c.currentRoutine == RoutineScheduleState::Breakfast) {
            auto* h = sFamilyDreamsEngine.GetHouseholdByMember(c.id);
            if (h && h->headEntityId == c.id && (c.id % 4 == 0)) {
                sFamilyDreamsEngine.ProcessHouseholdMorningBreakfast(h->householdId);
            }
        } else if (c.currentRoutine == RoutineScheduleState::Working || c.currentRoutine == RoutineScheduleState::AfternoonWork) {
            sFamilyDreamsEngine.AdvanceWorkShiftPerformance(c.id, 0.002f, false);
            // Occasional promotion review for top performers
            auto* car = sFamilyDreamsEngine.GetCareer(c.id);
            if (car && car->performanceScore >= 0.92f && (c.id % 12 == 0)) {
                sFamilyDreamsEngine.PromoteCitizenCareer(c.id);
            }
        } else if (c.currentRoutine == RoutineScheduleState::Dining) {
            auto* h = sFamilyDreamsEngine.GetHouseholdByMember(c.id);
            if (h) {
                if (h->headEntityId == c.id && (c.id % 4 == 0)) {
                    sFamilyDreamsEngine.ProcessHouseholdEveningDinner(h->householdId);
                }
                sFamilyDreamsEngine.DepositHouseholdSavings(h->householdId, c.id, 15);
            }
        } else if (c.currentRoutine == RoutineScheduleState::Leisure || c.currentRoutine == RoutineScheduleState::Nightclubbing) {
            float relief = 0.0f, happy = 0.0f;
            sEmergentLifeEngine.ExecuteCurrentActivity(c.id, relief, happy);
            c.drives.stress = std::max(0.05f, c.drives.stress - (relief * 0.005f));

            // Word-of-Mouth Gossip with known relationships or neighbors
            const auto* prof = sSocialEngine.GetProfile(c.id);
            if (prof && !prof->relationships.empty() && (c.id % 3 == 0)) {
                for (const auto& rel : prof->relationships) {
                    sEmergentLifeEngine.SpreadRumorBetweenEntities(c.id, rel.first);
                    break;
                }
            }
        }
    }
}

void CityLifeManager::UpdateWorkplaces(uint32 deltaMs)
{
    m_wagePayoutTimerMs += deltaMs;

    // Wage Payout & Production every 60 seconds of simulation
    bool doPayout = false;
    if (m_wagePayoutTimerMs >= 60000) {
        m_wagePayoutTimerMs = 0;
        doPayout = true;
    }

    for (auto& pair : m_workplaces) {
        WorkplaceEstablishment& w = pair.second;
        if (w.isLockedDown) {
            w.activeWorkersCount = 0;
            continue;
        }

        // Count attending workers
        uint32 activeCount = 0;
        for (uint32 citId : w.assignedCitizenIds) {
            auto it = m_citizens.find(citId);
            if (it != m_citizens.end()) {
                if (it->second.IsAtWork()) {
                    activeCount++;
                }
            }
        }
        w.activeWorkersCount = activeCount;

        // Economic Production Accumulation
        float productionRate = activeCount * (w.hourlyWageInfo * 1.5f);
        w.economicOutputCredits += (productionRate * (deltaMs / 3600000.0f));

        if (doPayout && activeCount > 0) {
            DistributeWorkplaceWages(w.id);
        }
    }
}

void CityLifeManager::DistributeWorkplaceWages(uint32 workplaceId)
{
    auto it = m_workplaces.find(workplaceId);
    if (it == m_workplaces.end() || it->second.isLockedDown) return;

    WorkplaceEstablishment& w = it->second;
    for (uint32 citId : w.assignedCitizenIds) {
        auto citIt = m_citizens.find(citId);
        if (citIt != m_citizens.end() && citIt->second.IsAtWork()) {
            citIt->second.drives.walletInfo += w.hourlyWageInfo;
            citIt->second.totalWagesEarned += w.hourlyWageInfo;
        }
    }
}

void CityLifeManager::UpdateShops(uint32 deltaMs)
{
    m_shopRestockTimerMs += deltaMs;

    // Restock inventories every 3 minutes
    if (m_shopRestockTimerMs >= 180000) {
        m_shopRestockTimerMs = 0;
        RestockAllShops();
    }

    float curH = m_simulatedHour;
    for (auto& pair : m_shops) {
        CommercialShop& s = pair.second;

        // Opening / Closing check
        if (s.isLockedDown) {
            s.isOpen = false;
        } else if (s.openingHour == 0.0f && s.closingHour >= 24.0f) {
            s.isOpen = true; // 24-hour shop
        } else if (s.closingHour > 24.0f) { // Closes past midnight
            s.isOpen = (curH >= s.openingHour || curH < (s.closingHour - 24.0f));
        } else {
            s.isOpen = (curH >= s.openingHour && curH < s.closingHour);
        }
    }
}

void CityLifeManager::RestockAllShops()
{
    for (auto& pair : m_shops) {
        for (auto& item : pair.second.inventory) {
            if (item.stock < item.maxStock) {
                uint32 add = std::min(15u, item.maxStock - item.stock);
                item.stock += add;
            }
        }
    }
}

bool CityLifeManager::ProcessShopPurchase(uint32 citizenId, uint32 shopId, uint32 templateId, uint32& outCost)
{
    outCost = 0;
    auto sIt = m_shops.find(shopId);
    if (sIt == m_shops.end() || !sIt->second.isOpen || sIt->second.isLockedDown) return false;

    CommercialShop& shop = sIt->second;
    ShopItem* item = shop.FindItem(templateId);
    if (!item || item->stock == 0) {
        // Fallback to first in-stock item if requested one is empty
        for (auto& fallback : shop.inventory) {
            if (fallback.stock > 0) {
                item = &fallback;
                break;
            }
        }
    }
    if (!item || item->stock == 0) return false;

    auto cIt = m_citizens.find(citizenId);
    if (cIt == m_citizens.end()) return false;
    BluepillCitizen& cit = cIt->second;

    if (cit.drives.walletInfo < item->priceInfo) return false;

    // Execute Transaction
    cit.drives.walletInfo -= item->priceInfo;
    shop.registerCash += item->priceInfo;
    item->stock--;
    item->dailySales++;
    shop.totalTransactions++;
    m_totalCommerceTransactions++;
    outCost = item->priceInfo;

    return true;
}

bool CityLifeManager::ProcessPlayerShopPurchase(uint32 playerId, uint32 shopId, uint32 templateId, uint32& outPrice)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    outPrice = 0;

    auto sIt = m_shops.find(shopId);
    if (sIt == m_shops.end() || !sIt->second.isOpen || sIt->second.isLockedDown) return false;

    CommercialShop& shop = sIt->second;
    ShopItem* item = shop.FindItem(templateId);
    if (!item || item->stock == 0) return false;

    item->stock--;
    item->dailySales++;
    shop.registerCash += item->priceInfo;
    shop.totalTransactions++;
    m_totalCommerceTransactions++;
    outPrice = item->priceInfo;

    return true;
}

void CityLifeManager::LockdownShopsInRadius(float x, float z, float radius, const std::string& reason)
{
    float rSq = radius * radius;
    for (auto& pair : m_shops) {
        float dx = pair.second.location.x - x;
        float dz = pair.second.location.z - z;
        if (dx * dx + dz * dz <= rSq) {
            pair.second.isLockedDown = true;
            pair.second.isOpen = false;
            pair.second.lockdownReason = reason;
        }
    }
}

void CityLifeManager::LiftShopLockdowns()
{
    for (auto& pair : m_shops) {
        pair.second.isLockedDown = false;
        pair.second.lockdownReason.clear();
    }
}

void CityLifeManager::UpdateSubwayTransit(uint32 deltaMs)
{
    float deltaSec = static_cast<float>(deltaMs) / 1000.0f;

    for (auto& pair : m_trains) {
        SubwayTrain& t = pair.second;
        if (t.isEmergencyHold || t.routeStationIds.empty()) continue;

        switch (t.state) {
            case SubwayTrainState::TRAIN_STOPPED_AT_STATION: {
                t.state = SubwayTrainState::TRAIN_DOORS_BOARDING;
                t.dwellTimerMs = 0;
                break;
            }
            case SubwayTrainState::TRAIN_DOORS_BOARDING: {
                t.dwellTimerMs += deltaMs;

                // Alight passengers whose destination is here
                uint32 currentStationId = t.routeStationIds[t.currentStationIndex];
                SubwayStation* currentStation = GetStation(currentStationId);

                if (currentStation) {
                    // Board waiting passengers up to capacity
                    while (!currentStation->waitingPassengers.empty() && t.passengersAboard.size() < t.maxPassengerCapacity) {
                        uint32 citId = currentStation->waitingPassengers.back();
                        currentStation->waitingPassengers.pop_back();
                        t.passengersAboard.push_back(citId);

                        auto citIt = m_citizens.find(citId);
                        if (citIt != m_citizens.end()) {
                            citIt->second.currentTransitTrainId = t.trainId;
                        }
                    }
                }

                if (t.dwellTimerMs >= t.dwellDurationMs) {
                    t.state = SubwayTrainState::TRAIN_DEPARTING;
                }
                break;
            }
            case SubwayTrainState::TRAIN_DEPARTING: {
                t.state = SubwayTrainState::TRAIN_IN_TRANSIT;
                t.transitProgress = 0.0f;
                break;
            }
            case SubwayTrainState::TRAIN_IN_TRANSIT: {
                float progressIncrement = static_cast<float>(deltaMs) / t.transitDurationMs;
                t.transitProgress += progressIncrement;

                uint32 curStId = t.routeStationIds[t.currentStationIndex];
                uint32 tgtStId = t.routeStationIds[t.targetStationIndex];
                SubwayStation* stFrom = GetStation(curStId);
                SubwayStation* stTo = GetStation(tgtStId);

                if (stFrom && stTo) {
                    t.currentPosition.x = stFrom->platformLocation.x + (stTo->platformLocation.x - stFrom->platformLocation.x) * t.transitProgress;
                    t.currentPosition.z = stFrom->platformLocation.z + (stTo->platformLocation.z - stFrom->platformLocation.z) * t.transitProgress;
                    t.currentPosition.y = stFrom->platformLocation.y;
                }

                if (t.transitProgress >= 1.0f) {
                    // Arrived at target station
                    t.currentStationIndex = t.targetStationIndex;
                    t.targetStationIndex = (t.targetStationIndex + 1) % t.routeStationIds.size();
                    t.transitProgress = 0.0f;
                    t.state = SubwayTrainState::TRAIN_STOPPED_AT_STATION;

                    // Alight passengers
                    uint32 arrivalStationId = t.routeStationIds[t.currentStationIndex];
                    SubwayStation* arrivalSt = GetStation(arrivalStationId);

                    for (uint32 pId : t.passengersAboard) {
                        auto citIt = m_citizens.find(pId);
                        if (citIt != m_citizens.end()) {
                            citIt->second.currentTransitTrainId = 0;
                            if (arrivalSt) {
                                citIt->second.currentLocation = arrivalSt->platformLocation;
                            }
                            citIt->second.totalTripsTaken++;
                        }
                    }
                    m_totalTransitRidership += static_cast<uint32>(t.passengersAboard.size());
                    t.totalPassengersMoved += static_cast<uint32>(t.passengersAboard.size());
                    t.passengersAboard.clear();
                }
                break;
            }
            case SubwayTrainState::TRAIN_EMERGENCY_HOLD:
                break;
        }
    }
}

bool CityLifeManager::BoardSubwayTrain(uint32 citizenId, uint32 trainId)
{
    auto tIt = m_trains.find(trainId);
    if (tIt == m_trains.end() || tIt->second.isEmergencyHold) return false;

    SubwayTrain& train = tIt->second;
    if (train.passengersAboard.size() >= train.maxPassengerCapacity) return false;

    auto cIt = m_citizens.find(citizenId);
    if (cIt == m_citizens.end()) return false;

    train.passengersAboard.push_back(citizenId);
    cIt->second.currentTransitTrainId = trainId;
    return true;
}

bool CityLifeManager::AlightSubwayTrain(uint32 citizenId, uint32 trainId)
{
    auto tIt = m_trains.find(trainId);
    if (tIt == m_trains.end()) return false;

    SubwayTrain& train = tIt->second;
    auto it = std::find(train.passengersAboard.begin(), train.passengersAboard.end(), citizenId);
    if (it == train.passengersAboard.end()) return false;

    train.passengersAboard.erase(it);
    auto cIt = m_citizens.find(citizenId);
    if (cIt != m_citizens.end()) {
        cIt->second.currentTransitTrainId = 0;
        cIt->second.totalTripsTaken++;
    }
    m_totalTransitRidership++;
    return true;
}

void CityLifeManager::TriggerEmergencyTransitHold(SubwayLineId lineId, uint32 durationMs, const std::string& reason)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_trains) {
        if (pair.second.lineId == lineId) {
            pair.second.isEmergencyHold = true;
            pair.second.holdReason = reason;
            pair.second.state = SubwayTrainState::TRAIN_EMERGENCY_HOLD;
        }
    }
}

void CityLifeManager::ClearTransitEmergencyHolds()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_trains) {
        pair.second.isEmergencyHold = false;
        pair.second.holdReason.clear();
        pair.second.state = SubwayTrainState::TRAIN_STOPPED_AT_STATION;
    }
}

void CityLifeManager::UpdateRoadTraffic(uint32 deltaMs)
{
    // 1. Cycle Traffic Light Intersections
    for (auto& pair : m_intersections) {
        TrafficIntersection& inter = pair.second;
        inter.lightTimerMs += deltaMs;

        switch (inter.lightState) {
            case TrafficLightState::LIGHT_GREEN:
                if (inter.lightTimerMs >= inter.greenDurationMs) {
                    inter.lightState = TrafficLightState::LIGHT_YELLOW;
                    inter.lightTimerMs = 0;
                }
                break;
            case TrafficLightState::LIGHT_YELLOW:
                if (inter.lightTimerMs >= inter.yellowDurationMs) {
                    inter.lightState = TrafficLightState::LIGHT_RED;
                    inter.lightTimerMs = 0;
                }
                break;
            case TrafficLightState::LIGHT_RED:
                if (inter.lightTimerMs >= inter.redDurationMs) {
                    inter.lightState = TrafficLightState::LIGHT_GREEN;
                    inter.lightTimerMs = 0;
                }
                break;
        }
    }

    // 2. Advance Vehicles Along Waypoints
    float deltaSec = static_cast<float>(deltaMs) / 1000.0f;
    for (auto& pair : m_vehicles) {
        CityVehicle& v = pair.second;
        if (v.waypoints.empty()) continue;

        if (v.isStoppedAtLight) {
            v.stoppedTimerMs += deltaMs;
            if (v.stoppedTimerMs > 5000) {
                v.isStoppedAtLight = false;
                v.stoppedTimerMs = 0;
            }
            continue;
        }

        LocationVector tgt = v.waypoints[v.currentWaypointIndex];
        float dx = tgt.x - v.currentLocation.x;
        float dz = tgt.z - v.currentLocation.z;
        float distSq = dx * dx + dz * dz;

        if (distSq > 2500.0f) { // Over 50 units away
            float dist = std::sqrt(distSq);
            float step = v.speedUnitsPerSec * deltaSec;
            if (step > dist) step = dist;
            v.currentLocation.x += (dx / dist) * step;
            v.currentLocation.z += (dz / dist) * step;
        } else {
            // Advance to next waypoint or reverse loop
            v.currentWaypointIndex = (v.currentWaypointIndex + 1) % v.waypoints.size();
        }
    }
}

uint32 CityLifeManager::SpawnAmbientVehicle(VehicleClassification type, uint32 districtId, LocationVector startLoc, LocationVector destLoc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32 vId = m_nextVehicleId++;

    CityVehicle v;
    v.vehicleId = vId;
    v.type = type;
    v.typeName = GetVehicleTypeName(type);
    v.modelName = (type == VehicleClassification::TAXI_CAB ? "Crown Victoria Cab" : "Civilian Cruiser");
    v.licensePlate = "MX-" + std::to_string(5000 + vId);
    v.districtId = districtId;
    v.currentLocation = startLoc;
    v.destinationLocation = destLoc;
    v.waypoints = {startLoc, destLoc};
    v.currentWaypointIndex = 0;
    v.speedUnitsPerSec = 450.0f;
    v.isStoppedAtLight = false;
    v.stoppedTimerMs = 0;
    v.passengerCapacity = 4;
    v.isEmergencyResponding = false;

    m_vehicles[vId] = v;
    return vId;
}

void CityLifeManager::BroadcastStreetRumor(RumorTopic topic, const std::string& headline, const std::string& content, uint32 districtId)
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        AmbientRumor r;
        r.id = m_nextRumorId++;
        r.topic = topic;
        r.headline = headline;
        r.content = content;
        r.originDistrictId = districtId;
        r.timestampMs = getMSTime();
        r.spreadCount = 1;

        m_rumors.insert(m_rumors.begin(), r);
        if (m_rumors.size() > 30) {
            m_rumors.pop_back();
        }

        // 3D Spatial Audio/Chat speech
        if (Has3DWorldSupport()) {
            for (auto& pair : m_citizens) {
                if (pair.second.districtId == districtId && pair.second.botGoId != 0) {
                    if (auto bot = sBotMgr.GetBotByGOID(pair.second.botGoId)) {
                        bot->Say(headline + ": " + content);
                        break;
                    }
                }
            }
        }
    }

    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.GetContagionEngine().SeedRumor(static_cast<uint32>(topic), headline, content, districtId, 0.35f);
    }
}

void CityLifeManager::UpdateRumorPool(uint32 deltaMs)
{
    m_ambientActivityTimerMs += deltaMs;
    if (m_ambientActivityTimerMs >= 15000) { // Every 15 seconds
        m_ambientActivityTimerMs = 0;
        if (!m_rumors.empty()) {
            m_rumors[rand() % m_rumors.size()].spreadCount++;
        }
    }
}

void CityLifeManager::QueueAreaPanic(float x, float z, float radius, const std::string& cause, uint32 durationMs)
{
    std::lock_guard<std::mutex> lock(m_asyncEventQueueMutex);
    m_pendingPanicEvents.push_back({x, z, radius, cause, durationMs});
}

void CityLifeManager::ProcessPendingAsyncEvents()
{
    std::vector<PendingPanicEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_asyncEventQueueMutex);
        if (m_pendingPanicEvents.empty()) return;
        events.swap(m_pendingPanicEvents);
    }

    for (const auto& ev : events) {
        TriggerAreaPanic(ev.x, ev.z, ev.radius, ev.cause, ev.durationMs, false);
    }
}

void CityLifeManager::TriggerAreaPanic(float x, float z, float radius, const std::string& cause, uint32 durationMs, bool notifyEmergentAI)
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        float rSq = radius * radius;
        m_totalPanicEvents++;

        for (auto& pair : m_citizens) {
            BluepillCitizen& c = pair.second;
            float dx = c.currentLocation.x - x;
            float dz = c.currentLocation.z - z;
            if (dx * dx + dz * dz <= rSq) {
                if (c.currentRoutine == RoutineScheduleState::Panicking) {
                    c.panicTimerMs = durationMs;
                    continue;
                }
                c.currentRoutine = RoutineScheduleState::Panicking;
                c.panicTimerMs = durationMs;
                c.panicReason = cause;
                c.drives.stress = std::min(1.0f, c.drives.stress + (0.5f * (1.0f + c.traits.neuroticism)));

                // Emergent Life: Witnessing traumatic Matrix conflict induces cognitive dissonance & awakening
                sEmergentLifeEngine.ProcessWitnessedAnomaly(c.id, cause, 0.45f);

                // High Neuroticism flees frantically towards nearest subway station
                if (c.traits.neuroticism > 0.45f) {
                    // Find nearest subway station
                    float bestDistSq = 1e12f;
                    LocationVector bestStLoc = c.homeLocation;
                    for (const auto& sPair : m_stations) {
                        float sdx = sPair.second.platformLocation.x - c.currentLocation.x;
                        float sdz = sPair.second.platformLocation.z - c.currentLocation.z;
                        float sDist = sdx * sdx + sdz * sdz;
                        if (sDist < bestDistSq) {
                            bestDistSq = sDist;
                            bestStLoc = sPair.second.platformLocation;
                        }
                    }
                    c.destinationLocation = bestStLoc;
                    c.movementSpeed = 6.5f; // Sprinting
                } else {
                    // Calm shelter in nearby structure
                    c.isSheltered = true;
                }

                if (c.botGoId != 0 && Has3DWorldSupport()) {
                    if (auto bot = sBotMgr.GetBotByGOID(c.botGoId)) {
                        bot->SetPanicking(true);
                        bot->Emote(50); // Cower
                        bot->Say("Look out! Shots fired!");
                        bot->MoveTo((float)c.destinationLocation.x, (float)c.destinationLocation.y, (float)c.destinationLocation.z);
                    }
                }
            }
        }

        // Also close local shops in danger zone
        LockdownShopsInRadius(x, z, radius, cause);
    }

    if (notifyEmergentAI && EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.GetContagionEngine().EmitPanicWave(LocationVector(x, 95.0, z), radius, 0.85f, cause, durationMs);
    }
}

void CityLifeManager::ClearAllPanic()
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& pair : m_citizens) {
            if (pair.second.currentRoutine == RoutineScheduleState::Panicking) {
                pair.second.currentRoutine = RoutineScheduleState::Leisure;
                pair.second.panicTimerMs = 0;
                pair.second.isSheltered = false;
                pair.second.drives.stress = 0.1f;
                if (pair.second.botGoId != 0 && Has3DWorldSupport()) {
                    if (auto bot = sBotMgr.GetBotByGOID(pair.second.botGoId)) {
                        bot->SetPanicking(false);
                    }
                }
            }
        }
        LiftShopLockdowns();
    }

    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.GetContagionEngine().ClearPanic();
    }
}

void CityLifeManager::NotifyUnderworldIncident(uint32 districtId, float x, float z, const std::string& incidentDesc)
{
    TriggerAreaPanic(x, z, 25000.0f, incidentDesc, 45000);
    BroadcastStreetRumor(RumorTopic::RUMOR_SYNDICATE_TURF_WAR, "Violent Incident Reported", incidentDesc, districtId);
}

// ============================================================================
// Queries & Getters
// ============================================================================

BluepillCitizen* CityLifeManager::GetCitizen(uint32 citizenId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_citizens.find(citizenId);
    return (it != m_citizens.end()) ? &it->second : nullptr;
}

std::string CityLifeManager::GetCitizenDFBiography(uint32 citizenId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_citizens.find(citizenId);
    if (it == m_citizens.end()) return "Citizen not found.";
    BiographicalProfile prof = sBioEngine.GenerateProfileForCitizen(citizenId, it->second.archetype);
    std::string baseSheet = prof.ToDFCharacterSheet();
    std::string familySummary = sFamilyDreamsEngine.GenerateFamilySummary(citizenId);
    std::string careerSummary = sFamilyDreamsEngine.GenerateCareerSummary(citizenId);
    std::string dreamsSummary = sFamilyDreamsEngine.GenerateDreamsSummary(citizenId);
    std::string socialSummary = sSocialEngine.GenerateEntitySocialSummary(citizenId);
    std::string memoriesReport = sSocialEngine.GenerateEntityMemoriesReport(citizenId);
    return baseSheet + "\n" + familySummary + "\n" + careerSummary + "\n" + dreamsSummary + "\n" + socialSummary + "\n" + memoriesReport;
}

std::vector<BluepillCitizen*> CityLifeManager::GetCitizensInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<BluepillCitizen*> res;
    for (auto& pair : m_citizens) {
        if (pair.second.districtId == districtId) res.push_back(&pair.second);
    }
    return res;
}

std::vector<BluepillCitizen*> CityLifeManager::GetCitizensByArchetype(CivilianArchetype archetype)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<BluepillCitizen*> res;
    for (auto& pair : m_citizens) {
        if (pair.second.archetype == archetype) res.push_back(&pair.second);
    }
    return res;
}

size_t CityLifeManager::GetActivePanickingCitizenCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& pair : m_citizens) {
        if (pair.second.IsPanicking()) count++;
    }
    return count;
}

WorkplaceEstablishment* CityLifeManager::GetWorkplace(uint32 workplaceId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_workplaces.find(workplaceId);
    return (it != m_workplaces.end()) ? &it->second : nullptr;
}

std::vector<WorkplaceEstablishment*> CityLifeManager::GetWorkplacesInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<WorkplaceEstablishment*> res;
    for (auto& pair : m_workplaces) {
        if (pair.second.districtId == districtId) res.push_back(&pair.second);
    }
    return res;
}

bool CityLifeManager::AssignCitizenToWorkplace(uint32 citizenId, uint32 workplaceId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto cIt = m_citizens.find(citizenId);
    auto wIt = m_workplaces.find(workplaceId);
    if (cIt == m_citizens.end() || wIt == m_workplaces.end()) return false;

    cIt->second.workplaceId = workplaceId;
    cIt->second.workplaceName = wIt->second.name;
    wIt->second.assignedCitizenIds.push_back(citizenId);
    return true;
}

CommercialShop* CityLifeManager::GetShop(uint32 shopId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_shops.find(shopId);
    return (it != m_shops.end()) ? &it->second : nullptr;
}

std::vector<CommercialShop*> CityLifeManager::GetShopsInDistrict(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<CommercialShop*> res;
    for (auto& pair : m_shops) {
        if (pair.second.districtId == districtId) res.push_back(&pair.second);
    }
    return res;
}

SubwayTrain* CityLifeManager::GetTrain(uint32 trainId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_trains.find(trainId);
    return (it != m_trains.end()) ? &it->second : nullptr;
}

SubwayStation* CityLifeManager::GetStation(uint32 stationId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_stations.find(stationId);
    return (it != m_stations.end()) ? &it->second : nullptr;
}

std::vector<SubwayStation*> CityLifeManager::GetStationsOnLine(SubwayLineId lineId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<SubwayStation*> res;
    for (auto& pair : m_stations) {
        if (pair.second.lineId == lineId) res.push_back(&pair.second);
    }
    return res;
}

CityVehicle* CityLifeManager::GetVehicle(uint32 vehicleId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_vehicles.find(vehicleId);
    return (it != m_vehicles.end()) ? &it->second : nullptr;
}

TrafficIntersection* CityLifeManager::GetIntersection(uint32 intersectionId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_intersections.find(intersectionId);
    return (it != m_intersections.end()) ? &it->second : nullptr;
}

// ============================================================================
// Diagnostics, Reporting & Persistence
// ============================================================================

std::string CityLifeManager::GenerateCityLifeStatusReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "====================================================================\n";
    ss << "          MEGACITY SIMULATION & DAILY LIFE STATUS REPORT            \n";
    ss << "====================================================================\n";
    ss << "Simulated Clock: " << GetTimeString() << " (" << GetCircadianPhaseName() << ")\n";
    ss << "Total Civilian Population: " << m_citizens.size() << " citizens\n";
    ss << "Active Panicking Citizens: " << GetActivePanickingCitizenCount() << "\n";
    ss << "Total Transit Ridership: " << m_totalTransitRidership << " completed passenger trips\n";
    ss << "Total Commercial Transactions: " << m_totalCommerceTransactions << " retail sales\n";
    ss << "Total Panic Trigger Events: " << m_totalPanicEvents << "\n\n";

    ss << "--- Workplaces (" << m_workplaces.size() << " establishments) ---\n";
    for (const auto& pair : m_workplaces) {
        const auto& w = pair.second;
        ss << "  [" << w.id << "] " << w.name << " (" << w.districtName << ")\n"
           << "      Staff: " << w.activeWorkersCount << "/" << w.assignedCitizenIds.size() << " active on shift"
           << " | Wage: " << w.hourlyWageInfo << " Info/hr"
           << " | Status: " << (w.isLockedDown ? "LOCKED DOWN (" + w.lockdownReason + ")" : "OPERATIONAL") << "\n";
    }

    ss << "\n--- Transit Subways (" << m_trains.size() << " active trains, " << m_stations.size() << " stations) ---\n";
    for (const auto& pair : m_trains) {
        const auto& t = pair.second;
        ss << "  Train #" << t.trainId << " [" << t.lineName << "]\n"
           << "      Passengers: " << t.passengersAboard.size() << "/" << t.maxPassengerCapacity
           << " | Moved: " << t.totalPassengersMoved
           << " | Progress: " << static_cast<int>(t.transitProgress * 100) << "%\n";
    }

    ss << "\n--- Commercial Shops (" << m_shops.size() << " retail stores) ---\n";
    for (const auto& pair : m_shops) {
        const auto& s = pair.second;
        ss << "  [" << s.id << "] " << s.name << " (" << s.categoryName << ")\n"
           << "      Open: " << (s.isOpen ? "YES" : "NO")
           << " | Cash: " << s.registerCash << " Info"
           << " | Sales: " << s.totalTransactions
           << " | Items in Stock: " << s.inventory.size() << "\n";
    }
    ss << "====================================================================\n";
    return ss.str();
}

std::string CityLifeManager::GenerateDemographicsReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "=== MEGACITY DEMOGRAPHICS & ARCHETYPES ===\n";
    std::map<CivilianArchetype, size_t> counts;
    std::map<RoutineScheduleState, size_t> routines;

    for (const auto& pair : m_citizens) {
        counts[pair.second.archetype]++;
        routines[pair.second.currentRoutine]++;
    }

    ss << "Demographic Distribution:\n";
    for (int a = 0; a < 8; ++a) {
        CivilianArchetype arch = static_cast<CivilianArchetype>(a);
        ss << "  - " << GetArchetypeName(arch) << ": " << counts[arch] << " citizens\n";
    }

    ss << "\nCurrent Routine Activity Distribution:\n";
    for (const auto& r : routines) {
        ss << "  - " << GetRoutineStateName(r.first) << ": " << r.second << " citizens\n";
    }
    return ss.str();
}

std::string CityLifeManager::GenerateTransitReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "=== MEGACITY TRANSIT (MMT & SURFACE ROADWAY) ===\n";
    ss << "Subway Lines:\n";
    for (const auto& pair : m_trains) {
        const auto& t = pair.second;
        ss << "  * Train #" << t.trainId << " on " << t.lineName 
           << " - Aboard: " << t.passengersAboard.size() << " | Status: " 
           << (t.isEmergencyHold ? "HOLD" : "RUNNING") << "\n";
    }
    ss << "Surface Traffic Vehicles (" << m_vehicles.size() << " units active):\n";
    for (const auto& pair : m_vehicles) {
        const auto& v = pair.second;
        ss << "  * [" << v.licensePlate << "] " << v.typeName << " (" << v.modelName << ") at ("
           << static_cast<int>(v.currentLocation.x) << ", " << static_cast<int>(v.currentLocation.z) << ")\n";
    }
    return ss.str();
}

std::string CityLifeManager::GenerateCommerceReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "=== MEGACITY COMMERCIAL RETAIL ECOSYSTEM ===\n";
    uint32 totalCash = 0;
    for (const auto& pair : m_shops) {
        const auto& s = pair.second;
        totalCash += s.registerCash;
        ss << "  Shop [" << s.id << "] " << s.name << " | Register: " << s.registerCash 
           << " Info | Sales: " << s.totalTransactions << " | Status: " << (s.isOpen ? "OPEN" : "CLOSED") << "\n";
    }
    ss << "Total Retail Capital in Registers: " << totalCash << " Info\n";
    return ss.str();
}

bool CityLifeManager::SaveCityLifeStateToFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"simulatedHour\": " << m_simulatedHour << ",\n";
    out << "  \"totalTransitRidership\": " << m_totalTransitRidership << ",\n";
    out << "  \"totalCommerceTransactions\": " << m_totalCommerceTransactions << ",\n";
    out << "  \"totalPanicEvents\": " << m_totalPanicEvents << ",\n";
    out << "  \"citizensCount\": " << m_citizens.size() << ",\n";
    out << "  \"workplacesCount\": " << m_workplaces.size() << ",\n";
    out << "  \"shopsCount\": " << m_shops.size() << ",\n";
    out << "  \"trainsCount\": " << m_trains.size() << "\n";
    out << "}\n";

    return true;
}

bool CityLifeManager::LoadCityLifeStateFromFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"simulatedHour\":") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                m_simulatedHour = std::stof(line.substr(pos + 1));
            }
        } else if (line.find("\"totalTransitRidership\":") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                m_totalTransitRidership = static_cast<uint32>(std::stoul(line.substr(pos + 1)));
            }
        } else if (line.find("\"totalCommerceTransactions\":") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                m_totalCommerceTransactions = static_cast<uint32>(std::stoul(line.substr(pos + 1)));
            }
        }
    }
    return true;
}

// ============================================================================
// Automated Headless Test Suite
// ============================================================================

void RunSimulationTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  MEGACITY SIMULATION & DAILY LIFE - TEST SUITE VERIFICATION " << std::endl;
    std::cout << "============================================================" << std::endl;

    CityLifeManager& sim = sCityLifeMgr;
    sim.Initialize();

    int passed = 0;
    int failed = 0;

    auto AssertTest = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failed++;
        }
    };

    // 1. Demographics & Archetype Initialization
    AssertTest(sim.GetTotalCitizenCount() >= 100, "Civilian Population Populated (100+ citizens)");
    
    bool hasAllArchetypes = true;
    for (int a = 0; a < 8; ++a) {
        if (sim.GetCitizensByArchetype(static_cast<CivilianArchetype>(a)).empty()) {
            hasAllArchetypes = false;
            break;
        }
    }
    AssertTest(hasAllArchetypes, "All 8 Demographic Archetypes Represented in Population");

    // 2. OCEAN Traits & Biometric Drives
    BluepillCitizen* c1 = sim.GetCitizen(1);
    AssertTest(c1 != nullptr, "Citizen #1 Retrieved");
    AssertTest(c1 && c1->traits.openness >= 0.0f && c1->traits.openness <= 1.0f, "Citizen OCEAN Openness in Valid [0, 1] Range");
    AssertTest(c1 && c1->traits.conscientiousness >= 0.0f && c1->traits.conscientiousness <= 1.0f, "Citizen OCEAN Conscientiousness in Valid [0, 1] Range");
    AssertTest(c1 && c1->traits.extraversion >= 0.0f && c1->traits.extraversion <= 1.0f, "Citizen OCEAN Extraversion in Valid [0, 1] Range");
    AssertTest(c1 && c1->traits.agreeableness >= 0.0f && c1->traits.agreeableness <= 1.0f, "Citizen OCEAN Agreeableness in Valid [0, 1] Range");
    AssertTest(c1 && c1->traits.neuroticism >= 0.0f && c1->traits.neuroticism <= 1.0f, "Citizen OCEAN Neuroticism in Valid [0, 1] Range");
    AssertTest(c1 && c1->drives.walletInfo > 0, "Citizen Has Initial Wallet Capital (Info credits)");

    // 3. Workplaces & Employment
    AssertTest(sim.GetAllWorkplaces().size() == 8, "All 8 Megacity Workplaces Initialized");
    WorkplaceEstablishment* metaHq = sim.GetWorkplace(1);
    AssertTest(metaHq != nullptr && metaHq->districtId == 2, "Metacortex Headquarters in Downtown District 2");
    AssertTest(metaHq && !metaHq->assignedCitizenIds.empty(), "Metacortex Roster Has Assigned Employees");

    // 4. Circadian 24-Hour Schedule Flow
    // Test Night Sleep (02:00)
    sim.SetSimulatedHour(2.0f);
    sim.Update(1000);
    BluepillCitizen* corpSuit = nullptr;
    for (auto& pair : sim.m_citizens) {
        if (pair.second.archetype == CivilianArchetype::CorporateSuit && pair.second.shift == WorkShiftType::ShiftDay) {
            corpSuit = &pair.second;
            break;
        }
    }
    AssertTest(corpSuit && corpSuit->currentRoutine == RoutineScheduleState::Sleeping, "Day Shift Corporate Suit Sleeping at 02:00 AM");

    // Test Breakfast (06:00)
    sim.SetSimulatedHour(6.0f);
    sim.Update(1000);
    AssertTest(corpSuit && corpSuit->currentRoutine == RoutineScheduleState::Breakfast, "Day Shift Corporate Suit Breakfast Routine at 06:00 AM");

    // Test Commute Rush Hour (08:00)
    sim.SetSimulatedHour(8.0f);
    sim.Update(1000);
    AssertTest(corpSuit && corpSuit->currentRoutine == RoutineScheduleState::CommuteToWork, "Day Shift Corporate Suit Commuting to Work at 08:00 AM");

    // Test At Work (10:00)
    sim.SetSimulatedHour(10.0f);
    sim.Update(1000);
    AssertTest(corpSuit && corpSuit->IsAtWork(), "Day Shift Corporate Suit Active at Work at 10:00 AM");

    // Test Lunch Break (12:30)
    sim.SetSimulatedHour(12.5f);
    sim.Update(1000);
    AssertTest(corpSuit && corpSuit->currentRoutine == RoutineScheduleState::LunchBreak, "Day Shift Corporate Suit on Lunch Break at 12:30 PM");

    // Test Afternoon Work (14:30)
    sim.SetSimulatedHour(14.5f);
    sim.Update(1000);
    AssertTest(corpSuit && corpSuit->currentRoutine == RoutineScheduleState::AfternoonWork, "Day Shift Corporate Suit Afternoon Work at 14:30 PM");

    // Test Evening Commute (17:30)
    sim.SetSimulatedHour(17.5f);
    sim.Update(1000);
    AssertTest(corpSuit && corpSuit->currentRoutine == RoutineScheduleState::CommuteHome, "Day Shift Corporate Suit Commuting Home at 17:30 PM");

    // Test Evening Leisure / Dining (20:00)
    sim.SetSimulatedHour(20.0f);
    sim.Update(1000);
    AssertTest(corpSuit && (corpSuit->currentRoutine == RoutineScheduleState::Leisure || corpSuit->currentRoutine == RoutineScheduleState::Dining), "Day Shift Corporate Suit in Leisure/Dining at 20:00 PM");

    // 5. Workplace Wages & Production
    uint32 initialSuitWallet = corpSuit ? corpSuit->drives.walletInfo : 0;
    sim.SetSimulatedHour(10.0f);
    sim.Update(1000);
    sim.DistributeWorkplaceWages(1); // Metacortex wage payout
    uint32 postWageWallet = corpSuit ? corpSuit->drives.walletInfo : 0;
    AssertTest(postWageWallet > initialSuitWallet, "Workplace Shift Wage Distribution Successfully Paid to Worker");

    // 6. Commercial Shops & Transactions
    AssertTest(sim.GetAllShops().size() == 10, "All 10 Commercial Shops Initialized");
    CommercialShop* noodleBar = sim.GetShop(1);
    AssertTest(noodleBar != nullptr && noodleBar->isOpen, "Morrell Noodle Bar Open and Operating");
    
    uint32 cost = 0;
    uint32 preCash = noodleBar ? noodleBar->registerCash : 0;
    bool bought = sim.ProcessShopPurchase(1, 1, 1001, cost);
    AssertTest(bought && cost == 12, "Civilian Successfully Purchased Pork Dumplings for 12 Info");
    AssertTest(noodleBar && noodleBar->registerCash == preCash + 12, "Shop Register Cash Increased by Transaction Value");

    uint32 playerPrice = 0;
    bool playerBought = sim.ProcessPlayerShopPurchase(999, 1, 1002, playerPrice);
    AssertTest(playerBought && playerPrice == 18, "Player Character Retail Purchase (Braised Beef Noodles - 18 Info)");

    // 7. Megacity Metro Transit (MMT) Subway System
    AssertTest(sim.GetAllStations().size() == 10, "All 10 Subway Stations Across 3 Lines Initialized");
    AssertTest(sim.GetAllTrains().size() == 3, "3 Scheduled Subway Trains Deployed on Fixed Routes");
    
    SubwayTrain* train1 = sim.GetTrain(1);
    AssertTest(train1 != nullptr && train1->lineId == SubwayLineId::Line1_RedLine, "Red Line Train #1 Operational");
    
    // Board train
    bool boarded = sim.BoardSubwayTrain(1, 1);
    AssertTest(boarded, "Citizen #1 Successfully Boarded Subway Train #1");
    AssertTest(train1 && !train1->passengersAboard.empty(), "Train Passenger Manifest Contains Boarded Citizen");

    // Alight train
    bool alighted = sim.AlightSubwayTrain(1, 1);
    AssertTest(alighted, "Citizen #1 Successfully Alighted From Subway Train #1");
    AssertTest(train1 && train1->passengersAboard.empty(), "Train Passenger Manifest Cleared Upon Alighting");

    // Emergency Hold
    sim.TriggerEmergencyTransitHold(SubwayLineId::Line1_RedLine, 60000, "Police Operation at Westview");
    AssertTest(train1 && train1->isEmergencyHold && train1->state == SubwayTrainState::TRAIN_EMERGENCY_HOLD, "Subway Emergency Hold Successfully Halts Line");
    sim.ClearTransitEmergencyHolds();
    AssertTest(train1 && !train1->isEmergencyHold, "Transit Emergency Hold Cleared and Service Resumed");

    // 8. Surface Traffic Network & Vehicles
    AssertTest(sim.GetAllIntersections().size() == 6, "6 Traffic Light Intersections Modeled");
    AssertTest(sim.GetAllVehicles().size() >= 12, "12 Ambient Roadway Vehicles (Taxis, Buses, Sedans, Police)");
    
    TrafficIntersection* inter1 = sim.GetIntersection(1);
    AssertTest(inter1 != nullptr, "Intersection #1 (Metacortex Blvd & 4th Ave) Located");
    TrafficLightState initialLight = inter1 ? inter1->lightState : TrafficLightState::LIGHT_RED;
    sim.UpdateRoadTraffic(25000); // Exceeds green duration
    AssertTest(inter1 && inter1->lightState != initialLight, "Traffic Light Dynamically Cycled Light States");

    uint32 newVehId = sim.SpawnAmbientVehicle(VehicleClassification::TAXI_CAB, 2, LocationVector(1000.0, 95.0, 1000.0), LocationVector(5000.0, 95.0, 5000.0));
    AssertTest(newVehId != 0 && sim.GetVehicle(newVehId) != nullptr, "Dynamic Spawn of Yellow Cab Taxi into Active Traffic");

    // 9. Ambient Life, Rumors & Reactive Crowd Panic
    AssertTest(!sim.GetActiveRumors().empty(), "Ambient Street Rumors Initialized");
    sim.BroadcastStreetRumor(RumorTopic::RUMOR_PUNISHER_SIGHTING, "Castle Sighting in Slums", "Vigilante skull emblem spotted near Pier 44", 1);
    AssertTest(sim.GetActiveRumors().front().headline == "Castle Sighting in Slums", "Dynamic Rumor Broadcast and Propagation");

    // Panic Test: Gunshots at Metacortex Plaza
    sim.TriggerAreaPanic(17043.0f, 2398.0f, 8500.0f, "Gunfire at Metacortex Plaza");
    size_t panickingCount = sim.GetActivePanickingCitizenCount();
    AssertTest(panickingCount > 0, "Gunfire Triggered Panic Sphere: Nearby Civilians Fleeing/Sheltering");

    CommercialShop* metaElectronics = sim.GetShop(5);
    AssertTest(metaElectronics && metaElectronics->isLockedDown, "Local Retail Shop Locked Down in Response to Proximity Danger");

    sim.ClearAllPanic();
    AssertTest(sim.GetActivePanickingCitizenCount() == 0, "Panic Cleared: Civilians Returned to Routine Behavior");
    AssertTest(metaElectronics && !metaElectronics->isLockedDown, "Shop Lockdown Lifted After Danger Cleared");

    // 10. Underworld Incident Notification
    sim.NotifyUnderworldIncident(1, -6415.0f, -7121.0f, "Syndicate Turf War Incursion in Slums");
    AssertTest(sim.GetActivePanickingCitizenCount() > 0, "Underworld Incident Notification Successfully Provokes Civilian Panic");
    sim.ClearAllPanic();

    // 11. State Persistence Round-Trip
    bool saved = sim.SaveCityLifeStateToFile("CityLifeSimulation_Test.json");
    AssertTest(saved, "CityLifeSimulation State Saved to JSON File");

    sim.m_totalTransitRidership = 9999;
    bool loaded = sim.LoadCityLifeStateFromFile("CityLifeSimulation_Test.json");
    AssertTest(loaded, "CityLifeSimulation State Loaded from JSON File");

    // 12. Reporting Generation
    std::string statusReport = sim.GenerateCityLifeStatusReport();
    AssertTest(!statusReport.empty() && statusReport.find("MEGACITY SIMULATION") != std::string::npos, "Status Report Generated with Complete Telemetry");

    std::string demoReport = sim.GenerateDemographicsReport();
    AssertTest(!demoReport.empty() && demoReport.find("DEMOGRAPHICS") != std::string::npos, "Demographics Breakdown Report Generated");

    std::string transitReport = sim.GenerateTransitReport();
    AssertTest(!transitReport.empty() && transitReport.find("TRANSIT") != std::string::npos, "Transit Status Report Generated");

    std::string commerceReport = sim.GenerateCommerceReport();
    AssertTest(!commerceReport.empty() && commerceReport.find("COMMERCIAL") != std::string::npos, "Commerce & Retail Report Generated");

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  SIMULATION TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    assert(failed == 0);
}

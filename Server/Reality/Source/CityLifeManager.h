#ifndef MXOEMU_CITY_LIFE_MANAGER_H
#define MXOEMU_CITY_LIFE_MANAGER_H

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
#include <random>

// ============================================================================
// Megacity Simulation & Daily Life Enums
// ============================================================================

enum class CivilianArchetype : uint8_t {
    CorporateSuit       = 0, // Metacortex / financial programmers & executives
    IndustrialBlueCollar= 1, // Docks, foundry, manufacturing mechanics & dockworkers
    ServiceRetailWorker = 2, // Diners, bodegas, boutiques, and street shops
    AcademicStudent     = 3, // Universities, research labs, libraries & parks
    NightlifeClubber    = 4, // Club Hel, Club Chateau hedonists & ravers
    UrbanDrifter        = 5, // Vagrants, street buskers & alley wanderers
    MunicipalCivilServant=6, // City Hall, urban administration & records clerks
    MedicalStaff        = 7  // St. Jude Hospital doctors, nurses & EMT crew
};

enum class RoutineScheduleState : uint8_t {
    Sleeping            = 0, // In apartment resting
    Breakfast           = 1, // Morning breakfast at diner/home
    CommuteToWork       = 2, // Walking or riding subway to job
    Working             = 3, // At workplace on shift
    LunchBreak          = 4, // Midday food at diner/bistro/park
    AfternoonWork       = 5, // Afternoon work session
    CommuteHome         = 6, // Outbound evening transit rush
    Shopping            = 7, // Browsing/buying in commercial stores
    Dining              = 8, // Evening sit-down meal
    Leisure             = 9, // Relaxing at parks, promenades
    Nightclubbing       = 10,// Partying at nightclubs/bars
    Panicking           = 11,// Fleeing gunfire/crimes
    Sheltering          = 12 // Safely hidden in station or shop
};

enum class WorkShiftType : uint8_t {
    ShiftDay            = 0, // 08:00 - 17:00
    ShiftSwing          = 1, // 16:00 - 00:00
    ShiftGraveyard      = 2, // 00:00 - 08:00
    ShiftFlexible       = 3  // Irregular / street work
};

enum class SubwayLineId : uint8_t {
    Line1_RedLine       = 1, // Westview (Slums) <-> Downtown Central <-> Richland Concourse
    Line2_BlueLine      = 2, // Morrell Sump <-> International Concourse <-> Financial Center
    Line3_GreenLine     = 3  // Pier 44 Docks <-> Downtown Park <-> Sakura Gardens <-> Creston
};

enum class SubwayTrainState : uint8_t {
    TRAIN_STOPPED_AT_STATION = 0,
    TRAIN_DOORS_BOARDING     = 1,
    TRAIN_DEPARTING          = 2,
    TRAIN_IN_TRANSIT         = 3,
    TRAIN_EMERGENCY_HOLD     = 4
};

enum class TrafficLightState : uint8_t {
    LIGHT_GREEN  = 0,
    LIGHT_YELLOW = 1,
    LIGHT_RED    = 2
};

enum class VehicleClassification : uint8_t {
    TAXI_CAB          = 0, // Yellow Cab
    TRANSIT_BUS       = 1, // Municipal Transit Bus
    DELIVERY_VAN      = 2, // Freight & logistics van
    CIVILIAN_SEDAN    = 3, // Commuter sedan
    MUNICIPAL_CRUISER = 4  // Police patrol / city cruiser
};

enum class StreetActivityType : uint8_t {
    Strolling         = 0,
    BuskingMusician   = 1,
    PhoneBoothCalling = 2,
    ReadingNewspaper  = 3,
    WindowShopping    = 4,
    SmokingAlley      = 5,
    CoffeeSipping     = 6,
    ParkBenchResting  = 7,
    VendorOperating   = 8
};

enum class RumorTopic : uint8_t {
    RUMOR_SYNDICATE_TURF_WAR    = 0,
    RUMOR_PUNISHER_SIGHTING     = 1,
    RUMOR_WEATHER_GLITCH        = 2,
    RUMOR_POLICE_CRACKDOWN      = 3,
    RUMOR_CORPORATE_EXPLOITATION= 4,
    RUMOR_MATRIX_ANOMALY        = 5
};

// ============================================================================
// Core Data Structures
// ============================================================================

struct CivilianOCEAN {
    float openness{0.5f};
    float conscientiousness{0.5f};
    float extraversion{0.5f};
    float agreeableness{0.5f};
    float neuroticism{0.5f};
};

struct CivilianDrives {
    float hunger{0.2f};      // 0.0 (full) to 1.0 (starving)
    float fatigue{0.1f};     // 0.0 (energized) to 1.0 (exhausted)
    float socialNeed{0.3f};  // 0.0 (satisfied) to 1.0 (lonely)
    float stress{0.1f};      // 0.0 (calm) to 1.0 (extreme panic)
    uint32 walletInfo{650};  // Currency available
};

struct BluepillCitizen {
    uint32 id{0};
    uint32 botGoId{0};
    std::string name;
    std::string gender;
    CivilianArchetype archetype{CivilianArchetype::CorporateSuit};
    std::string archetypeName;
    CivilianOCEAN traits;
    CivilianDrives drives;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector currentLocation;
    LocationVector destinationLocation;
    LocationVector homeLocation;
    std::string homeApartmentName;
    uint32 workplaceId{0};
    std::string workplaceName;
    uint32 preferredShopId{0};
    WorkShiftType shift{WorkShiftType::ShiftDay};
    RoutineScheduleState currentRoutine{RoutineScheduleState::Sleeping};
    StreetActivityType currentStreetActivity{StreetActivityType::Strolling};
    bool isSheltered{false};
    bool isUsingUmbrella{false};
    uint32 panicTimerMs{0};
    std::string panicReason;
    uint32 currentTransitTrainId{0};
    uint32 currentTransitStationId{0};
    float movementSpeed{3.5f};
    std::string lastSpokenRumor;
    uint32 totalTripsTaken{0};
    uint32 totalShopsVisited{0};
    uint32 totalWagesEarned{0};

    bool IsPanicking() const { return currentRoutine == RoutineScheduleState::Panicking; }
    bool IsAtHome() const { return currentRoutine == RoutineScheduleState::Sleeping; }
    bool IsAtWork() const { return currentRoutine == RoutineScheduleState::Working || currentRoutine == RoutineScheduleState::AfternoonWork; }
    bool IsInTransit() const { return currentTransitTrainId != 0; }
};

struct WorkplaceEstablishment {
    uint32 id{0};
    std::string name;
    std::string typeDesc;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector location;
    uint32 capacity{40};
    std::vector<uint32> assignedCitizenIds;
    uint32 activeWorkersCount{0};
    uint32 hourlyWageInfo{45};
    bool isLockedDown{false};
    std::string lockdownReason;
    float economicOutputCredits{0.0f};
};

struct ShopItem {
    uint32 templateId{0};
    std::string name;
    std::string category;
    uint32 priceInfo{15};
    uint32 stock{50};
    uint32 maxStock{50};
    uint32 dailySales{0};
};

struct CommercialShop {
    uint32 id{0};
    std::string name;
    std::string categoryName;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector location;
    float openingHour{8.0f};
    float closingHour{21.0f};
    bool isOpen{true};
    std::vector<ShopItem> inventory;
    uint32 registerCash{2500};
    uint32 activeShoppersCount{0};
    uint32 maxShoppers{25};
    bool isLockedDown{false};
    std::string lockdownReason;
    uint32 totalTransactions{0};

    ShopItem* FindItem(uint32 templateId) {
        for (auto& item : inventory) {
            if (item.templateId == templateId) return &item;
        }
        return nullptr;
    }
};

struct SubwayStation {
    uint32 id{0};
    std::string name;
    SubwayLineId lineId{SubwayLineId::Line1_RedLine};
    std::string lineName;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    LocationVector platformLocation;
    std::vector<uint32> waitingPassengers;
    uint32 totalPassengerBoardings{0};
};

struct SubwayTrain {
    uint32 trainId{0};
    SubwayLineId lineId{SubwayLineId::Line1_RedLine};
    std::string lineName;
    std::vector<uint32> routeStationIds;
    size_t currentStationIndex{0};
    size_t targetStationIndex{1};
    SubwayTrainState state{SubwayTrainState::TRAIN_STOPPED_AT_STATION};
    uint32 dwellTimerMs{0};
    uint32 dwellDurationMs{15000}; // 15 seconds door hold
    float transitProgress{0.0f};  // 0.0 to 1.0 between stations
    float transitDurationMs{45000.0f}; // 45 seconds run time
    LocationVector currentPosition;
    std::vector<uint32> passengersAboard;
    uint32 maxPassengerCapacity{80};
    bool isEmergencyHold{false};
    std::string holdReason;
    uint32 totalPassengersMoved{0};
};

struct TrafficIntersection {
    uint32 id{0};
    std::string name;
    uint32 districtId{1};
    LocationVector centerLocation;
    TrafficLightState lightState{TrafficLightState::LIGHT_GREEN};
    uint32 lightTimerMs{0};
    uint32 greenDurationMs{20000};
    uint32 yellowDurationMs{4000};
    uint32 redDurationMs{20000};
    uint32 queuedVehiclesCount{0};
};

struct CityVehicle {
    uint32 vehicleId{0};
    VehicleClassification type{VehicleClassification::CIVILIAN_SEDAN};
    std::string typeName;
    std::string modelName;
    std::string licensePlate;
    uint32 districtId{1};
    LocationVector currentLocation;
    LocationVector destinationLocation;
    std::vector<LocationVector> waypoints;
    size_t currentWaypointIndex{0};
    float speedUnitsPerSec{450.0f};
    bool isStoppedAtLight{false};
    uint32 stoppedTimerMs{0};
    std::vector<uint32> passengerIds;
    uint32 passengerCapacity{4};
    bool isEmergencyResponding{false};
};

struct AmbientRumor {
    uint32 id{0};
    RumorTopic topic{RumorTopic::RUMOR_WEATHER_GLITCH};
    std::string topicName;
    std::string headline;
    std::string content;
    uint32 originDistrictId{1};
    uint32 timestampMs{0};
    uint32 spreadCount{0};
};

// ============================================================================
// CityLifeManager Singleton
// ============================================================================

class CityLifeManager : public Singleton<CityLifeManager> {
public:
    friend void RunSimulationTestSuite();

    CityLifeManager();
    ~CityLifeManager();

    void Initialize();
    void Update(uint32 deltaMs);

    // Circadian Radiant Clock
    float GetSimulatedHour() const { return m_simulatedHour; }
    void SetSimulatedHour(float hour);
    uint32 GetHourInt() const { return static_cast<uint32>(m_simulatedHour) % 24; }
    uint32 GetMinuteInt() const { return static_cast<uint32>((m_simulatedHour - static_cast<uint32>(m_simulatedHour)) * 60.0f); }
    std::string GetTimeString() const;
    std::string GetCircadianPhaseName() const;

    // Citizens & Demographics
    const std::map<uint32, BluepillCitizen>& GetAllCitizens() const { return m_citizens; }
    BluepillCitizen* GetCitizen(uint32 citizenId);
    std::vector<BluepillCitizen*> GetCitizensInDistrict(uint32 districtId);
    std::vector<BluepillCitizen*> GetCitizensByArchetype(CivilianArchetype archetype);
    size_t GetTotalCitizenCount() const { return m_citizens.size(); }
    size_t GetActivePanickingCitizenCount() const;
    void SpawnPhysicalCitizens();
    void DespawnPhysicalCitizens();
    bool HasPhysicalWorldPresence() const { return m_physicalSpawnsActive; }

    // Workplaces & Employment
    const std::map<uint32, WorkplaceEstablishment>& GetAllWorkplaces() const { return m_workplaces; }
    WorkplaceEstablishment* GetWorkplace(uint32 workplaceId);
    std::vector<WorkplaceEstablishment*> GetWorkplacesInDistrict(uint32 districtId);
    bool AssignCitizenToWorkplace(uint32 citizenId, uint32 workplaceId);
    void DistributeWorkplaceWages(uint32 workplaceId);

    // Shops & Retail Commerce
    const std::map<uint32, CommercialShop>& GetAllShops() const { return m_shops; }
    CommercialShop* GetShop(uint32 shopId);
    std::vector<CommercialShop*> GetShopsInDistrict(uint32 districtId);
    bool ProcessShopPurchase(uint32 citizenId, uint32 shopId, uint32 templateId, uint32& outCost);
    bool ProcessPlayerShopPurchase(uint32 playerId, uint32 shopId, uint32 templateId, uint32& outPrice);
    void RestockAllShops();
    void LockdownShopsInRadius(float x, float z, float radius, const std::string& reason);
    void LiftShopLockdowns();

    // Megacity Metro Transit (MMT) Subway Network
    const std::map<uint32, SubwayTrain>& GetAllTrains() const { return m_trains; }
    const std::map<uint32, SubwayStation>& GetAllStations() const { return m_stations; }
    SubwayTrain* GetTrain(uint32 trainId);
    SubwayStation* GetStation(uint32 stationId);
    std::vector<SubwayStation*> GetStationsOnLine(SubwayLineId lineId);
    bool BoardSubwayTrain(uint32 citizenId, uint32 trainId);
    bool AlightSubwayTrain(uint32 citizenId, uint32 trainId);
    void TriggerEmergencyTransitHold(SubwayLineId lineId, uint32 durationMs, const std::string& reason);
    void ClearTransitEmergencyHolds();
    uint32 GetTotalTransitRidership() const { return m_totalTransitRidership; }

    // Roadway Traffic Network & Vehicles
    const std::map<uint32, CityVehicle>& GetAllVehicles() const { return m_vehicles; }
    const std::map<uint32, TrafficIntersection>& GetAllIntersections() const { return m_intersections; }
    CityVehicle* GetVehicle(uint32 vehicleId);
    TrafficIntersection* GetIntersection(uint32 intersectionId);
    uint32 SpawnAmbientVehicle(VehicleClassification type, uint32 districtId, LocationVector startLoc, LocationVector destLoc);

    // Ambient Life, Rumors & Reactive Panic
    void BroadcastStreetRumor(RumorTopic topic, const std::string& headline, const std::string& content, uint32 districtId);
    const std::vector<AmbientRumor>& GetActiveRumors() const { return m_rumors; }
    void TriggerAreaPanic(float x, float z, float radius, const std::string& cause, uint32 durationMs = 30000, bool notifyEmergentAI = true);
    void QueueAreaPanic(float x, float z, float radius, const std::string& cause, uint32 durationMs = 30000);
    void ProcessPendingAsyncEvents();
    void ClearAllPanic();
    void NotifyUnderworldIncident(uint32 districtId, float x, float z, const std::string& incidentDesc);

    // Diagnostics, Persistence & Reporting
    std::string GenerateCityLifeStatusReport() const;
    std::string GenerateDemographicsReport() const;
    std::string GenerateTransitReport() const;
    std::string GenerateCommerceReport() const;
    bool SaveCityLifeStateToFile(const std::string& path = "CityLifeSimulation.json");
    bool LoadCityLifeStateFromFile(const std::string& path = "CityLifeSimulation.json");
    std::string GetCitizenDFBiography(uint32 citizenId) const;

    // Static Utilities
    static std::string GetArchetypeName(CivilianArchetype a);
    static std::string GetRoutineStateName(RoutineScheduleState r);
    static std::string GetShiftName(WorkShiftType s);
    static std::string GetSubwayLineName(SubwayLineId l);
    static std::string GetVehicleTypeName(VehicleClassification v);
    static std::string GetDistrictName(uint32 districtId);

private:
    void InitializeDefaultWorkplaces();
    void InitializeDefaultShops();
    void InitializeDefaultTransitSubways();
    void InitializeDefaultTrafficGrid();
    void PopulateDefaultCitizens(size_t count = 120);
    void InitializeDefaultRumors();

    void UpdateCircadianSchedule(uint32 deltaMs);
    void UpdateCitizens(uint32 deltaMs);
    void UpdateWorkplaces(uint32 deltaMs);
    void UpdateShops(uint32 deltaMs);
    void UpdateSubwayTransit(uint32 deltaMs);
    void UpdateRoadTraffic(uint32 deltaMs);
    void UpdateRumorPool(uint32 deltaMs);

    std::map<uint32, BluepillCitizen> m_citizens;
    std::map<uint32, WorkplaceEstablishment> m_workplaces;
    std::map<uint32, CommercialShop> m_shops;
    std::map<uint32, SubwayStation> m_stations;
    std::map<uint32, SubwayTrain> m_trains;
    std::map<uint32, TrafficIntersection> m_intersections;
    std::map<uint32, CityVehicle> m_vehicles;
    std::vector<AmbientRumor> m_rumors;

    float m_simulatedHour{8.0f}; // Starts at 08:00 AM (Morning Rush / Shift Start)
    float m_timeScaleFactor{12.0f}; // 1 real sec = 12 sim seconds (1 sim day = 120 real mins)
    bool m_useWeatherSystemHour{true};

    uint32 m_nextCitizenId{1};
    uint32 m_nextVehicleId{1};
    uint32 m_nextRumorId{1};

    uint32 m_scheduleTickTimerMs{0};
    uint32 m_wagePayoutTimerMs{0};
    uint32 m_shopRestockTimerMs{0};
    uint32 m_ambientActivityTimerMs{0};

    uint32 m_totalTransitRidership{0};
    uint32 m_totalCommerceTransactions{0};
    uint32 m_totalPanicEvents{0};

    struct PendingPanicEvent {
        float x;
        float z;
        float radius;
        std::string cause;
        uint32 durationMs;
    };

    mutable std::mutex m_asyncEventQueueMutex;
    std::vector<PendingPanicEvent> m_pendingPanicEvents;

    bool m_physicalSpawnsActive{false};
    mutable std::recursive_mutex m_mutex;
};

#define sCityLifeMgr CityLifeManager::getSingleton()

// Test Suite Declaration
void RunSimulationTestSuite();

#endif // MXOEMU_CITY_LIFE_MANAGER_H

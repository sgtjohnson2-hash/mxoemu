#ifndef MXOEMU_PEDESTRIANECOLOGY_H
#define MXOEMU_PEDESTRIANECOLOGY_H

#include "Common.h"
#include "Singleton.h"
#include "WeatherSystem.h"
#include "AI/BotPersonality.h"
#include <string>
#include <vector>
#include <memory>

class BotClient;
class PlayerObject;
struct BotVector2D;

enum POIType {
    POI_OFFICE_COMMERCIAL = 0,
    POI_DINER_RESTAURANT = 1,
    POI_PARK_BENCH = 2,
    POI_NIGHTCLUB_BAR = 3,
    POI_SUBWAY_TRANSIT = 4,
    POI_RESIDENTIAL_APARTMENT = 5,
    POI_PAYPHONE_HARDLINE = 6,
    POI_ATM_TERMINAL = 7
};

struct PointOfInterest {
    std::string name;
    POIType type;
    uint32 districtId;
    float x;
    float y;
    float z;
};

enum CivilianFearTier : uint8 {
    CIV_TIER_NORMAL_COMMUTE = 0,    // Fear < 0.25: Ambient schedules, visiting POIs
    CIV_TIER_UNEASY_RUMORS = 1,     // Fear 0.25 - 0.55: Gather at POIs, whisper rumors, spread fear
    CIV_TIER_ALARM_EVASION = 2,     // Fear 0.55 - 0.80: Avoid suspicious entities, refuse to talk, look over shoulder, avoid alleys
    CIV_TIER_PANIC_STAMPEDE = 3     // Fear >= 0.80: Flee to subways/buildings, distress cries, stampede
};

struct TacticalCordonPoint {
    uint32 districtId{1};
    float x{0.0f};
    float y{95.0f};
    float z{0.0f};
    std::string name;
    bool active{false};
    uint32 deployedMs{0};
    std::vector<uint32> cordonBotGoIds;
};

class PedestrianEcology : public Singleton<PedestrianEcology> {
public:
    PedestrianEcology();
    ~PedestrianEcology();

    void Initialize();

    // Query POIs based on circadian routine and personality
    PointOfInterest GetCircadianTarget(float currentX, float currentZ, WeatherSystem::CircadianPeriod period, const BotPersonality& personality) const;
    PointOfInterest GetNearestPOI(float currentX, float currentZ, POIType type) const;
    PointOfInterest GetNearestSubway(float currentX, float currentZ) const;
    PointOfInterest GetNearestShelter(float currentX, float currentZ) const;

    // Smart object behavioral affordance interaction
    void InteractWithPOI(BotClient* bot, PlayerObject* me, const PointOfInterest& poi);

    // Proximity social gossip & Theory of Mind belief update with rumor mutation
    bool TryProximityGossip(BotClient* botA, BotClient* botB, uint32 currentMs);
    bool TryOutbreakGossip(BotClient* botA, BotClient* botB, uint32 currentMs);
    void SpreadRumorFearAura(float x, float z, float fearAmount, float radius = 800.0f, uint32 excludeGoId = 0);

    // Dynamic crowd lane / sidewalk steering force
    void ApplyCrowdSteering(BotClient* bot, PlayerObject* me, float& outSteerX, float& outSteerZ);

    // Full civilian behavioral update tick & multi-tier response
    void UpdateCivilian(BotClient* bot, float deltaSeconds);
    CivilianFearTier EvaluateCivilianTier(float fear) const;
    void UpdateCivilianFear(BotClient* bot, PlayerObject* me, float deltaSeconds);
    void ExecuteTier0Normal(BotClient* bot, PlayerObject* me, float deltaSeconds);
    void ExecuteTier1Uneasy(BotClient* bot, PlayerObject* me, float deltaSeconds);
    void ExecuteTier2Alarm(BotClient* bot, PlayerObject* me, float deltaSeconds);
    void ExecuteTier3Panic(BotClient* bot, PlayerObject* me, float deltaSeconds);

    // Tactical Perimeter Cordon & SWAT containment
    void DeployTacticalCordon(uint32 districtId);
    bool IsCordonActive(uint32 districtId) const;
    void SetCordonActive(uint32 districtId, bool active);
    bool CheckCordonInterception(BotClient* bot, PlayerObject* me, float subwayX, float subwayZ, float radius = 500.0f);
    bool BreachTacticalCordon(uint32 districtId, uint32 rescuerSquadId = 0);
    const std::map<uint32, TacticalCordonPoint>& GetCordons() const { return m_tacticalCordons; }

    const std::vector<PointOfInterest>& GetAllPOIs() const { return m_pois; }

private:
    void RegisterPOI(const std::string& name, POIType type, uint32 districtId, float x, float y, float z);

    std::vector<PointOfInterest> m_pois;
    std::vector<std::string> m_rumorPool;
    std::vector<std::string> m_outbreakRumors;
    std::vector<std::string> m_panicCries;
    std::vector<std::string> m_evasionWhispers;
    std::map<uint32, TacticalCordonPoint> m_tacticalCordons;
    mutable std::recursive_mutex m_cordonMutex;
};

#define sPedestrianEcology PedestrianEcology::getSingleton()

#endif // MXOEMU_PEDESTRIANECOLOGY_H

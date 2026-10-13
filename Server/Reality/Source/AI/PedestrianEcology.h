#ifndef MXOEMU_PEDESTRIANECOLOGY_H
#define MXOEMU_PEDESTRIANECOLOGY_H

#include "Common.h"
#include "Singleton.h"
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

// Civilian day/night routine clock (moved here from the previous weather module, which was its
// only other owner). 1 in-game hour = 5 real minutes; the day starts at 08:00 on first use.
enum CircadianPeriod {
    PERIOD_REST_NIGHT = 0,      // 22:00 - 06:00 (rest in apartments/shelter)
    PERIOD_COMMUTE_MORNING = 1, // 06:00 - 08:00 (rush hour to subways/work)
    PERIOD_WORK = 2,            // 08:00 - 17:00 (offices, commerce, terminals)
    PERIOD_COMMUTE_EVENING = 3, // 17:00 - 18:00 (commute from work)
    PERIOD_LEISURE = 4          // 18:00 - 22:00 (diners, clubs, parks)
};

class PedestrianEcology : public Singleton<PedestrianEcology> {
public:
    PedestrianEcology();
    ~PedestrianEcology();

    void Initialize();

    // Query POIs based on circadian routine and personality
    PointOfInterest GetCircadianTarget(float currentX, float currentZ, CircadianPeriod period, const BotPersonality& personality) const;
    PointOfInterest GetNearestPOI(float currentX, float currentZ, POIType type) const;
    PointOfInterest GetNearestSubway(float currentX, float currentZ) const;
    PointOfInterest GetNearestShelter(float currentX, float currentZ) const;

    // Civilian routine clock
    float GetMatrixHour() const;
    CircadianPeriod GetCircadianPeriod() const;

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

    // An evacuating civilian that reached its hardline jacks out: real JackoutEffectMsg to
    // nearby clients, then the bot despawns.
    bool ExecuteCivilianJackout(uint32 entityGoId);

    const std::vector<PointOfInterest>& GetAllPOIs() const { return m_pois; }

private:
    void RegisterPOI(const std::string& name, POIType type, uint32 districtId, float x, float y, float z);

    std::vector<PointOfInterest> m_pois;
    std::vector<std::string> m_rumorPool;
    std::vector<std::string> m_outbreakRumors;
    std::vector<std::string> m_panicCries;
    std::vector<std::string> m_evasionWhispers;
    mutable uint32 m_clockStartMs{0};
};

#define sPedestrianEcology PedestrianEcology::getSingleton()

#endif // MXOEMU_PEDESTRIANECOLOGY_H

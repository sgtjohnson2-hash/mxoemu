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
    POI_RESIDENTIAL_APARTMENT = 5
};

struct PointOfInterest {
    std::string name;
    POIType type;
    uint32 districtId;
    float x;
    float y;
    float z;
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

    // Proximity social gossip & Theory of Mind belief update
    bool TryProximityGossip(BotClient* botA, BotClient* botB, uint32 currentMs);

    // Dynamic crowd lane / sidewalk steering force
    void ApplyCrowdSteering(BotClient* bot, PlayerObject* me, float& outSteerX, float& outSteerZ);

    // Full civilian behavioral update tick
    void UpdateCivilian(BotClient* bot, float deltaSeconds);

    const std::vector<PointOfInterest>& GetAllPOIs() const { return m_pois; }

private:
    void RegisterPOI(const std::string& name, POIType type, uint32 districtId, float x, float y, float z);

    std::vector<PointOfInterest> m_pois;
    std::vector<std::string> m_rumorPool;
};

#define sPedestrianEcology PedestrianEcology::getSingleton()

#endif // MXOEMU_PEDESTRIANECOLOGY_H

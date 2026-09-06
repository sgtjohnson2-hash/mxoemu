#include "PedestrianEcology.h"
#include "BotClient.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Timer.h"
#include "BehaviorTree.h"
#include "AI/TheoryOfMind.h"
#include <cmath>
#include <algorithm>

createFileSingleton(PedestrianEcology);

PedestrianEcology::PedestrianEcology()
{
}

PedestrianEcology::~PedestrianEcology()
{
    m_pois.clear();
}

void PedestrianEcology::RegisterPOI(const std::string& name, POIType type, uint32 districtId, float x, float y, float z)
{
    PointOfInterest poi;
    poi.name = name;
    poi.type = type;
    poi.districtId = districtId;
    poi.x = x;
    poi.y = y;
    poi.z = z;
    m_pois.push_back(poi);
}

void PedestrianEcology::Initialize()
{
    m_pois.clear();

    // -----------------------------------------------------------------------
    // District 1: Slums (Westview, Morrell, Creston) - 13 POIs
    // -----------------------------------------------------------------------
    RegisterPOI("Westview Subway Terminal", POI_SUBWAY_TRANSIT, 1, -49790.0f, 95.0f, -159434.0f);
    RegisterPOI("Morrell Noodle Bar", POI_DINER_RESTAURANT, 1, -6415.0f, 95.0f, -7121.0f);
    RegisterPOI("Creston Tenements", POI_RESIDENTIAL_APARTMENT, 1, -30434.0f, 95.0f, 20325.0f);
    RegisterPOI("Tabor Park Benches", POI_PARK_BENCH, 1, -36326.0f, 95.0f, -23953.0f);
    RegisterPOI("Club Hel Underground", POI_NIGHTCLUB_BAR, 1, -67862.0f, 95.0f, 16314.0f);
    RegisterPOI("Westview Commercial Depot", POI_OFFICE_COMMERCIAL, 1, -30111.0f, 95.0f, -48204.0f);
    RegisterPOI("Morrell Payphone Booth", POI_PAYPHONE_HARDLINE, 1, -7200.0f, 95.0f, -6800.0f);
    RegisterPOI("Slums Corner ATM", POI_ATM_TERMINAL, 1, -30800.0f, 95.0f, 19800.0f);
    RegisterPOI("Creston Alleyway Diner", POI_DINER_RESTAURANT, 1, -32500.0f, 95.0f, 18500.0f);
    RegisterPOI("Tabor North Benches", POI_PARK_BENCH, 1, -35800.0f, 95.0f, -22500.0f);
    RegisterPOI("Westview Freight Rail", POI_SUBWAY_TRANSIT, 1, -52000.0f, 95.0f, -162000.0f);
    RegisterPOI("Slums Second-Hand Electronics", POI_OFFICE_COMMERCIAL, 1, -28500.0f, 95.0f, -46000.0f);
    RegisterPOI("South Creston Payphone", POI_PAYPHONE_HARDLINE, 1, -31500.0f, 95.0f, 22000.0f);

    // -----------------------------------------------------------------------
    // District 2: Downtown (Central, Metacortex, Financial) - 14 POIs
    // -----------------------------------------------------------------------
    RegisterPOI("Downtown Central Metro", POI_SUBWAY_TRANSIT, 2, 7737.0f, 95.0f, 13801.0f);
    RegisterPOI("Metacortex Corporate Plaza", POI_OFFICE_COMMERCIAL, 2, 17043.0f, 495.0f, 2398.0f);
    RegisterPOI("G-Bistro", POI_DINER_RESTAURANT, 2, 39216.0f, 95.0f, -21475.0f);
    RegisterPOI("Downtown Park Promenade", POI_PARK_BENCH, 2, 13211.0f, 95.0f, -37821.0f);
    RegisterPOI("Club Zion Underground", POI_NIGHTCLUB_BAR, 2, 25640.0f, -515.0f, -35813.0f);
    RegisterPOI("Government Administration Plaza", POI_OFFICE_COMMERCIAL, 2, 52941.0f, 495.0f, 41981.0f);
    RegisterPOI("Downtown High-Rise Lofts", POI_RESIDENTIAL_APARTMENT, 2, 9844.0f, 1295.0f, 1314.0f);
    RegisterPOI("Financial District Payphone", POI_PAYPHONE_HARDLINE, 2, 18500.0f, 95.0f, 3100.0f);
    RegisterPOI("MegaCity Bank ATM", POI_ATM_TERMINAL, 2, 16200.0f, 95.0f, 1800.0f);
    RegisterPOI("Plaza Coffee Kiosk", POI_DINER_RESTAURANT, 2, 14500.0f, 95.0f, -36500.0f);
    RegisterPOI("Grand Central Fountain Benches", POI_PARK_BENCH, 2, 12000.0f, 95.0f, -39000.0f);
    RegisterPOI("North Downtown Metro", POI_SUBWAY_TRANSIT, 2, 9200.0f, 95.0f, 16500.0f);
    RegisterPOI("Civic Center Hardline Phone", POI_PAYPHONE_HARDLINE, 2, 51500.0f, 95.0f, 40500.0f);
    RegisterPOI("Metacortex Lobby ATM", POI_ATM_TERMINAL, 2, 17500.0f, 95.0f, 2900.0f);

    // -----------------------------------------------------------------------
    // District 3: International (Embassies, Le Vrai, Chateau) - 13 POIs
    // -----------------------------------------------------------------------
    RegisterPOI("International Concourse Metro", POI_SUBWAY_TRANSIT, 3, 77349.0f, 695.0f, -43966.0f);
    RegisterPOI("Consul Diplomatic Tower", POI_OFFICE_COMMERCIAL, 3, 111180.0f, 95.0f, -40913.0f);
    RegisterPOI("Le Vrai Fine Dining", POI_DINER_RESTAURANT, 3, 82745.0f, 695.0f, -66760.0f);
    RegisterPOI("Sakura Garden Plazas", POI_PARK_BENCH, 3, 51820.0f, 95.0f, -42922.0f);
    RegisterPOI("Club Chateau", POI_NIGHTCLUB_BAR, 3, 111007.0f, -505.0f, -59842.0f);
    RegisterPOI("International Luxury Suites", POI_RESIDENTIAL_APARTMENT, 3, 96190.0f, 95.0f, -85320.0f);
    RegisterPOI("Embassy Row Payphone", POI_PAYPHONE_HARDLINE, 3, 110500.0f, 95.0f, -41500.0f);
    RegisterPOI("International Exchange ATM", POI_ATM_TERMINAL, 3, 78200.0f, 95.0f, -45000.0f);
    RegisterPOI("Chateau VIP Lounge", POI_NIGHTCLUB_BAR, 3, 111500.0f, -505.0f, -61000.0f);
    RegisterPOI("Sakura Lotus Benches", POI_PARK_BENCH, 3, 53000.0f, 95.0f, -44000.0f);
    RegisterPOI("Diplomatic North Metro", POI_SUBWAY_TRANSIT, 3, 76500.0f, 95.0f, -42000.0f);
    RegisterPOI("Le Vrai Bistro Terrace", POI_DINER_RESTAURANT, 3, 83500.0f, 695.0f, -65500.0f);
    RegisterPOI("Consul Gate Payphone", POI_PAYPHONE_HARDLINE, 3, 112000.0f, 95.0f, -39800.0f);

    // -----------------------------------------------------------------------
    // District 4+: Richland & Westview Outer - 12 POIs
    // -----------------------------------------------------------------------
    RegisterPOI("Richland Transit Station", POI_SUBWAY_TRANSIT, 4, 40267.0f, 95.0f, -116994.0f);
    RegisterPOI("Richland Financial Tower", POI_OFFICE_COMMERCIAL, 4, 107619.0f, -505.0f, -149092.0f);
    RegisterPOI("West Park Benches", POI_PARK_BENCH, 4, 15657.0f, 495.0f, -70054.0f);
    RegisterPOI("Cafe Rene", POI_DINER_RESTAURANT, 4, 87635.0f, 85.0f, -117864.0f);
    RegisterPOI("Club Neon", POI_NIGHTCLUB_BAR, 4, 79289.0f, -505.0f, -148965.0f);
    RegisterPOI("Richland Executive Condos", POI_RESIDENTIAL_APARTMENT, 4, 107619.0f, 95.0f, -149092.0f);
    RegisterPOI("Richland Heights Payphone", POI_PAYPHONE_HARDLINE, 4, 41500.0f, 95.0f, -115800.0f);
    RegisterPOI("Executive Trust ATM", POI_ATM_TERMINAL, 4, 106800.0f, 95.0f, -148000.0f);
    RegisterPOI("Cafe Rene Outdoor Patio", POI_DINER_RESTAURANT, 4, 88200.0f, 85.0f, -116900.0f);
    RegisterPOI("West Park Lake Benches", POI_PARK_BENCH, 4, 16800.0f, 495.0f, -71200.0f);
    RegisterPOI("Richland South Express Terminal", POI_SUBWAY_TRANSIT, 4, 39500.0f, 95.0f, -118200.0f);
    RegisterPOI("Richland Skywalk Payphone", POI_PAYPHONE_HARDLINE, 4, 108200.0f, 95.0f, -150200.0f);

    // Citywide Rumor Pool for Proximity Gossip
    m_rumorPool = {
        "Did you see the weather skybox flicker earlier? Looked like green rain falling upward.",
        "They say the subway line in Downtown was locked down by men in black suits.",
        "I heard Morrell is completely under Machine surveillance now.",
        "Some redpill crashed an entire garrison at the hardline yesterday.",
        "Keep your head down. The truce is slipping away day by day.",
        "The Merovingian is hiring couriers to move encrypted code cylinders out of the district.",
        "Have you spoken to the Oracle lately? She said changes are coming to the Megacity.",
        "I saw someone jump across three rooftops in Westview. No human can jump like that.",
        "Watch out for glitches near the phone booths... that's where they enter.",
        "Tastee Wheat never tastes the same twice. Makes you wonder who wrote the food subroutine."
    };

    INFO_LOG(format("PedestrianEcology Initialized with %1% POIs and %2% authentic rumors across 4 districts.") 
             % m_pois.size() % m_rumorPool.size());
}

PointOfInterest PedestrianEcology::GetCircadianTarget(float currentX, float currentZ, WeatherSystem::CircadianPeriod period, const BotPersonality& personality) const
{
    POIType targetType = POI_OFFICE_COMMERCIAL;

    switch (period) {
        case WeatherSystem::PERIOD_COMMUTE_MORNING:
        case WeatherSystem::PERIOD_COMMUTE_EVENING:
            targetType = POI_SUBWAY_TRANSIT;
            break;
        case WeatherSystem::PERIOD_WORK:
            if (personality.conscientiousness > 0.4f) {
                targetType = POI_OFFICE_COMMERCIAL;
            } else {
                targetType = POI_DINER_RESTAURANT; // Slacking off at the diner
            }
            break;
        case WeatherSystem::PERIOD_LEISURE:
            if (personality.extraversion > 0.6f && personality.openness > 0.5f) {
                targetType = POI_NIGHTCLUB_BAR;
            } else if (personality.extraversion > 0.3f) {
                targetType = POI_DINER_RESTAURANT;
            } else {
                targetType = POI_PARK_BENCH; // Quiet park bench for introverts
            }
            break;
        case WeatherSystem::PERIOD_REST_NIGHT:
            if (personality.neuroticism > 0.7f || personality.conscientiousness > 0.5f) {
                targetType = POI_RESIDENTIAL_APARTMENT;
            } else {
                targetType = POI_NIGHTCLUB_BAR; // Night owls / nightlife dwellers
            }
            break;
    }

    return GetNearestPOI(currentX, currentZ, targetType);
}

PointOfInterest PedestrianEcology::GetNearestPOI(float currentX, float currentZ, POIType type) const
{
    PointOfInterest best;
    float minDistSq = -1.0f;

    for (const auto& poi : m_pois) {
        if (poi.type != type) continue;
        float dx = poi.x - currentX;
        float dz = poi.z - currentZ;
        float distSq = dx * dx + dz * dz;
        if (minDistSq < 0.0f || distSq < minDistSq) {
            minDistSq = distSq;
            best = poi;
        }
    }

    if (minDistSq < 0.0f && !m_pois.empty()) {
        return m_pois[0]; // Fallback
    }
    return best;
}

PointOfInterest PedestrianEcology::GetNearestSubway(float currentX, float currentZ) const
{
    return GetNearestPOI(currentX, currentZ, POI_SUBWAY_TRANSIT);
}

void PedestrianEcology::InteractWithPOI(BotClient* bot, PlayerObject* me, const PointOfInterest& poi)
{
    if (!bot || !me) return;

    switch (poi.type) {
        case POI_DINER_RESTAURANT: {
            // Restore health & IS from eating
            uint16 curH = me->getCurrentHealth();
            uint16 maxH = me->getMaximumHealth();
            if (curH < maxH) me->setCurrentHealth(std::min<uint16>(maxH, curH + 150));
            me->setCurrentIS(std::min<uint16>(me->getMaximumIS(), me->getCurrentIS() + 25));
            bot->Emote(42); // Eating/drinking emote
            if (rand() % 100 < 10) {
                bot->Say("*sips coffee quietly*");
            }
            break;
        }
        case POI_PARK_BENCH: {
            // Rest and recuperate
            me->setCurrentHealth(std::min<uint16>(me->getMaximumHealth(), me->getCurrentHealth() + 80));
            bot->Emote(40); // Sitting/resting emote
            break;
        }
        case POI_PAYPHONE_HARDLINE: {
            // Check dial tone or listen for operator
            bot->Emote(41);
            if (rand() % 100 < 8) {
                bot->Say("*picks up payphone receiver, listening for an operator tone*");
            }
            break;
        }
        case POI_ATM_TERMINAL: {
            bot->Emote(41); // Interacting with terminal
            break;
        }
        case POI_NIGHTCLUB_BAR: {
            bot->Emote(44); // Dancing / social emote
            break;
        }
        default:
            break;
    }
}

bool PedestrianEcology::TryProximityGossip(BotClient* botA, BotClient* botB, uint32 currentMs)
{
    if (!botA || !botB || m_rumorPool.empty()) return false;

    // Cooldown check (once every 45 seconds per bot)
    if (currentMs - botA->GetLastGossipTime() < 45000 || currentMs - botB->GetLastGossipTime() < 45000) {
        return false;
    }

    botA->SetLastGossipTime(currentMs);
    botB->SetLastGossipTime(currentMs);

    PlayerObject* poA = BotGetPlayer(botA->GetPlayerGoId());
    PlayerObject* poB = BotGetPlayer(botB->GetPlayerGoId());
    if (!poA || !poB) return false;

    // Select base rumor from pool
    int rumorIdx = rand() % m_rumorPool.size();
    std::string rumor = m_rumorPool[rumorIdx];

    // Mutate and exaggerate rumor as it spreads
    const char* prefixes[] = {
        "Did you hear? ",
        "Word on the street is, ",
        "I got this straight from an operator: ",
        "You're not going to believe this, but "
    };
    std::string mutatedRumor = prefixes[rand() % 4] + rumor;

    botA->Say((format("\"%1%\"") % mutatedRumor).str());

    // Update Theory of Mind belief
    botA->GetTheoryOfMindSolver().UpdateState(std::to_string(poB->getGoId()), 0.15f, 0.0f);
    botB->GetTheoryOfMindSolver().UpdateState(std::to_string(poA->getGoId()), 0.15f, 0.0f);

    return true;
}

void PedestrianEcology::ApplyCrowdSteering(BotClient* bot, PlayerObject* me, float& outSteerX, float& outSteerZ)
{
    outSteerX = 0.0f;
    outSteerZ = 0.0f;
    if (!bot || !me) return;

    LocationVector myPos = me->getPosition();
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(myPos.x, myPos.z);

    for (GameClient* client : nearbyClients) {
        if (client == bot) continue;
        uint32 otherGoId = client->GetPlayerGoId();
        if (otherGoId == 0) continue;

        PlayerObject* other = BotGetPlayer(otherGoId);
        if (!other || other->isDead()) continue;

        LocationVector otherPos = other->getPosition();
        float dx = myPos.x - otherPos.x;
        float dz = myPos.z - otherPos.z;
        float distSq = dx * dx + dz * dz;

        // Sidewalk personal space (within 3 meters = 300 units)
        if (distSq > 1.0f && distSq < 90000.0f) {
            float dist = std::sqrt(distSq);
            // Repulsion force inversely proportional to distance
            float force = (300.0f - dist) / 300.0f;
            // Add lateral deflection (sidewalk courtesy steering: turn slightly right)
            float perpX = -dz / dist;
            float perpZ = dx / dist;

            outSteerX += (dx / dist) * force * 3.0f + perpX * force * 1.5f;
            outSteerZ += (dz / dist) * force * 3.0f + perpZ * force * 1.5f;
        }
    }

    // Clamp steering magnitude to prevent velocity explosions
    float steerLenSq = outSteerX * outSteerX + outSteerZ * outSteerZ;
    if (steerLenSq > 4.0f) {
        float steerLen = std::sqrt(steerLenSq);
        outSteerX = (outSteerX / steerLen) * 2.0f;
        outSteerZ = (outSteerZ / steerLen) * 2.0f;
    }
}

void PedestrianEcology::UpdateCivilian(BotClient* bot, float deltaSeconds)
{
    if (!bot) return;
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me || me->isDead()) return;

    const BotPersonality& personality = bot->GetPersonality();
    LocationVector pos = me->getPosition();
    uint32 now = getMSTime();

    // 1. Panic Response (OCEAN Neuroticism influence)
    if (bot->IsPanicking()) {
        PointOfInterest subway = GetNearestSubway(pos.x, pos.z);
        float dx = subway.x - pos.x;
        float dz = subway.z - pos.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist > 200.0f) { // Head to subway entrance
            dx /= dist;
            dz /= dist;
            float fleeSpeed = 8.0f + (personality.neuroticism * 4.0f); // High N flees faster
            float newX = pos.x + dx * fleeSpeed * deltaSeconds * 100.0f;
            float newZ = pos.z + dz * fleeSpeed * deltaSeconds * 100.0f;
            bot->MoveTo(newX, pos.y, newZ);
        } else {
            // Reached subway shelter, calm down
            bot->SetPanicking(false);
            bot->Say("I made it to the subway... safe for now.");
        }

        // Neuroticism slows calming; Conscientiousness speeds calming
        float calmChance = ((1.0f - personality.neuroticism) * 0.08f) + (personality.conscientiousness * 0.04f);
        if ((rand() % 100) / 100.0f < calmChance) {
            bot->SetPanicking(false);
            bot->Say("*takes a deep breath and looks around*");
        }
        return;
    }

    // 2. Circadian Schedule Routine Navigation
    WeatherSystem::CircadianPeriod currentPeriod = sWeatherSys.GetCircadianPeriod();
    PointOfInterest targetPOI = GetCircadianTarget(pos.x, pos.z, currentPeriod, personality);

    float toPdx = targetPOI.x - pos.x;
    float toPdz = targetPOI.z - pos.z;
    float distToPOI = std::sqrt(toPdx * toPdx + toPdz * toPdz);

    if (distToPOI > 1500.0f) { // Beyond 15m of destination: travel toward POI
        toPdx /= distToPOI;
        toPdz /= distToPOI;

        float steerX = 0.0f;
        float steerZ = 0.0f;
        ApplyCrowdSteering(bot, me, steerX, steerZ);

        float walkSpeed = 3.5f;
        float moveX = (toPdx * walkSpeed * 100.0f + steerX * 50.0f) * deltaSeconds;
        float moveZ = (toPdz * walkSpeed * 100.0f + steerZ * 50.0f) * deltaSeconds;

        bot->MoveTo(pos.x + moveX, pos.y, pos.z + moveZ);
    } else {
        // At or near POI: interact with the smart object
        InteractWithPOI(bot, me, targetPOI);

        int roll = rand() % 100;
        if (roll < 35) {
            bot->RoamAndSwarm(deltaSeconds);
        } else if (roll < 65) {
            // Proximity gossip attempt with nearby civilians
            auto nearby = sSpatialGrid.GetClientsInRadius(pos.x, pos.z);
            for (GameClient* gc : nearby) {
                if (gc == bot || !gc->isBot()) continue;
                BotClient* otherBot = dynamic_cast<BotClient*>(gc);
                if (otherBot) {
                    PlayerObject* otherPo = BotGetPlayer(otherBot->GetPlayerGoId());
                    if (otherPo && otherPo->getFactionName() == "Civilian" && !otherBot->IsPanicking()) {
                        TryProximityGossip(bot, otherBot, now);
                        break;
                    }
                }
            }
        }
    }
}

#include "PedestrianEcology.h"
#include "BotClient.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Timer.h"
#include "BehaviorTree.h"
#include "AI/TheoryOfMind.h"
#include "AI/MatrixThreatHeatmap.h"
#include "RadioDispatchSystem.h"
#include "BotManager.h"
#include "SmithVirusCascade.h"
#include "GameServer.h"
#include "BackdoorNetwork.h"
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

    // Tier 1 Outbreak Rumors (Emergent Outbreak Awareness)
    m_outbreakRumors = {
        "Did you see that man in the black suit? There were three of them with the exact same face...",
        "Someone near the station had their face glitch and ripple into a dark suit...",
        "They're multiplying. Don't look them in the eyes or you become one of them...",
        "I saw an entire crowd turn their heads in perfect synchronization. Something is terribly wrong.",
        "The transit police fired on one of those identical men and the bullets just flattened on his skin.",
        "There are men in dark glasses watching every phone booth... don't answer them.",
        "A guy in Morrell collapsed, and when he stood up, he wasn't him anymore—he was that agent.",
        "They're sealing off the streets... I heard it over a transit police scanner.",
        "Unnatural faces in the crowd. Look closely at people's reflections in the glass windows.",
        "The subway turnstiles are being guarded by identical agents standing shoulder to shoulder."
    };

    // Tier 3 Panic Vocalizations & Distress Cries
    m_panicCries = {
        "THEY'RE MULTIPLYING! RUN FOR YOUR LIVES!",
        "HIS FACE... IT WAS IN THE MIRROR! GOD HELP US!",
        "THEY'RE TURNING EVERYONE! DON'T LET THEM TOUCH YOU!",
        "SUBWAY! GET UNDERGROUND RIGHT NOW!",
        "THE WHOLE STREET TURNED INTO HIM! RUN!",
        "PLEASE NO! SOMEONE STOP THEM! AAAAAGH!",
        "HE JUST REACHED INTO HIM AND REWROTE HIM! RUN!",
        "THE POLICE CAN'T STOP THEM! THEY'RE INEVITABLE!"
    };

    // Tier 2 Evasion Whispers & Rejections
    m_evasionWhispers = {
        "Leave me alone! Don't look at them... just keep moving!",
        "Don't come near me! You could be one of them!",
        "I didn't see anything! I don't know anything! Get away!",
        "*looks nervously over shoulder, pulling collar tight and hurrying past*",
        "Stay back! Don't talk to anyone... they're watching every corner."
    };

    {
        std::lock_guard<std::recursive_mutex> lock(m_cordonMutex);
        m_tacticalCordons.clear();
    }

    INFO_LOG(format("PedestrianEcology Initialized with %1% POIs, %2% ambient rumors, and %3% outbreak rumors across 4 districts.") 
             % m_pois.size() % m_rumorPool.size() % m_outbreakRumors.size());
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

PointOfInterest PedestrianEcology::GetNearestShelter(float currentX, float currentZ) const
{
    PointOfInterest best;
    float minDistSq = -1.0f;

    for (const auto& poi : m_pois) {
        if (poi.type != POI_SUBWAY_TRANSIT && poi.type != POI_OFFICE_COMMERCIAL && poi.type != POI_RESIDENTIAL_APARTMENT) {
            continue;
        }
        float dx = poi.x - currentX;
        float dz = poi.z - currentZ;
        float distSq = dx * dx + dz * dz;
        if (minDistSq < 0.0f || distSq < minDistSq) {
            minDistSq = distSq;
            best = poi;
        }
    }

    if (minDistSq < 0.0f && !m_pois.empty()) {
        return m_pois[0];
    }
    return best;
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
            float force = (300.0f - dist) / 300.0f;
            float perpX = -dz / dist;
            float perpZ = dx / dist;

            outSteerX += (dx / dist) * force * 3.0f + perpX * force * 1.5f;
            outSteerZ += (dz / dist) * force * 3.0f + perpZ * force * 1.5f;
        }
    }

    float steerLenSq = outSteerX * outSteerX + outSteerZ * outSteerZ;
    if (steerLenSq > 4.0f) {
        float steerLen = std::sqrt(steerLenSq);
        outSteerX = (outSteerX / steerLen) * 2.0f;
        outSteerZ = (outSteerZ / steerLen) * 2.0f;
    }
}

bool PedestrianEcology::TryOutbreakGossip(BotClient* botA, BotClient* botB, uint32 currentMs)
{
    if (!botA || !botB || m_outbreakRumors.empty()) return false;

    if (currentMs - botA->GetLastGossipTime() < 20000 || currentMs - botB->GetLastGossipTime() < 20000) {
        return false;
    }

    botA->SetLastGossipTime(currentMs);
    botB->SetLastGossipTime(currentMs);

    PlayerObject* poA = BotGetPlayer(botA->GetPlayerGoId());
    PlayerObject* poB = BotGetPlayer(botB->GetPlayerGoId());
    if (!poA || !poB) return false;

    int rumorIdx = rand() % m_outbreakRumors.size();
    std::string rumor = m_outbreakRumors[rumorIdx];

    const char* whispers[] = {
        "*whispers frantically* ",
        "*looking over shoulder* ",
        "Listen to me... ",
        "Did you see that? "
    };
    std::string mutated = whispers[rand() % 4] + rumor;
    botA->Say((format("\"%1%\"") % mutated).str());

    // Boost fear of speaker and listener
    botA->AddFear(0.10f);
    botB->AddFear(0.15f);

    // Propagate fear aura to surrounding pedestrians
    SpreadRumorFearAura(poA->getPosition().x, poA->getPosition().z, 0.12f, 800.0f, poA->getGoId());

    // Theory of Mind updates
    botA->GetTheoryOfMindSolver().UpdateState(std::to_string(poB->getGoId()), 0.25f, 0.1f);
    botB->GetTheoryOfMindSolver().UpdateState(std::to_string(poA->getGoId()), 0.25f, 0.1f);

    return true;
}

void PedestrianEcology::SpreadRumorFearAura(float x, float z, float fearAmount, float radius, uint32 excludeGoId)
{
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(x, z);
    float radSq = radius * radius;

    for (GameClient* gc : nearbyClients) {
        if (!gc->isBot()) continue;
        uint32 otherGoId = gc->GetPlayerGoId();
        if (otherGoId == 0 || otherGoId == excludeGoId) continue;

        PlayerObject* otherPo = BotGetPlayer(otherGoId);
        if (!otherPo || otherPo->isDead() || otherPo->getFactionName() != "Civilian") continue;

        BotClient* otherBot = dynamic_cast<BotClient*>(gc);
        if (!otherBot) continue;

        LocationVector oPos = otherPo->getPosition();
        float dx = x - oPos.x;
        float dz = z - oPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq <= radSq) {
            float dist = std::sqrt(distSq);
            float weight = (radius - dist) / radius;
            otherBot->AddFear(fearAmount * weight);
        }
    }
}

CivilianFearTier PedestrianEcology::EvaluateCivilianTier(float fear) const
{
    if (fear >= 0.80f) return CIV_TIER_PANIC_STAMPEDE;
    if (fear >= 0.55f) return CIV_TIER_ALARM_EVASION;
    if (fear >= 0.25f) return CIV_TIER_UNEASY_RUMORS;
    return CIV_TIER_NORMAL_COMMUTE;
}

void PedestrianEcology::UpdateCivilianFear(BotClient* bot, PlayerObject* me, float deltaSeconds)
{
    const BotPersonality& personality = bot->GetPersonality();
    LocationVector pos = me->getPosition();

    float currentFear = bot->GetFearLevel();
    float fearDelta = 0.0f;

    // 1. Threat Heatmap influence
    float localHeat = sMatrixThreatHeatmap.GetHeat(pos.x, pos.z);
    EscalationTier tier = sMatrixThreatHeatmap.GetTier(pos.x, pos.z);
    if (localHeat > 20.0f) {
        fearDelta += (localHeat * 0.0012f) * deltaSeconds * (0.8f + personality.neuroticism * 0.6f);
    }
    float minFearFloor = 0.0f;
    if (tier == ESCALATION_TIER_1_POLICE) minFearFloor = 0.20f;
    else if (tier == ESCALATION_TIER_2_SWAT) minFearFloor = 0.35f;
    else if (tier == ESCALATION_TIER_3_AGENT_TAKEOVER) minFearFloor = 0.55f;
    else if (tier >= ESCALATION_TIER_4_MULTI_AGENT) minFearFloor = 0.80f;

    // 2. Proximity Threat Scanning
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(pos.x, pos.z);
    bool smithNearby = false;
    bool violenceNearby = false;
    bool panicNearby = false;

    for (GameClient* gc : nearbyClients) {
        if (gc == bot || !gc->isBot()) continue;
        BotClient* otherBot = dynamic_cast<BotClient*>(gc);
        if (!otherBot) continue;

        PlayerObject* otherPo = BotGetPlayer(otherBot->GetPlayerGoId());
        if (!otherPo) continue;

        float dx = pos.x - otherPo->getPosition().x;
        float dz = pos.z - otherPo->getPosition().z;
        float distSq = dx * dx + dz * dz;

        if (distSq > 9000000.0f) continue; // 30m

        // Agent Smith sightings
        if (otherBot->isAgent() || otherPo->getHandle().find("Smith") != std::string::npos || otherPo->getHandle().find("Agent") != std::string::npos) {
            smithNearby = true;
            float dist = std::sqrt(std::max(1.0f, distSq));
            float smithFactor = (dist < 1000.0f) ? 0.70f : 0.35f;
            fearDelta += smithFactor * deltaSeconds * (0.9f + personality.neuroticism * 0.5f);

            if (otherBot->IsInCombat()) {
                fearDelta += 0.25f * deltaSeconds;
            }
        }

        // Casualties / dead bodies
        if (otherPo->isDead()) {
            violenceNearby = true;
            fearDelta += 0.18f * deltaSeconds;
        }

        // Social fear contagion: Panicked pedestrians transmit terror
        if (otherBot->GetCivilianTier() >= CIV_TIER_PANIC_STAMPEDE) {
            panicNearby = true;
            fearDelta += 0.22f * deltaSeconds * personality.neuroticism;
        }
    }

    // 3. Fear Decay or Accretion
    if (!smithNearby && !violenceNearby && !panicNearby && localHeat < 15.0f) {
        float decayRate = (0.05f + personality.conscientiousness * 0.03f) * (1.0f - personality.neuroticism * 0.4f);
        currentFear = std::max(minFearFloor, currentFear - decayRate * deltaSeconds);
    } else {
        currentFear = std::clamp(currentFear + fearDelta, minFearFloor, 1.0f);
    }

    bot->SetFearLevel(currentFear);
}

void PedestrianEcology::UpdateCivilian(BotClient* bot, float deltaSeconds)
{
    if (!bot) return;
    PlayerObject* me = BotGetPlayer(bot->GetPlayerGoId());
    if (!me || me->isDead()) return;

    // 0. Hardline Evacuation Corridor Override (Zion Pirate Radio / Cleansing Evac)
    if (bot->IsEvacuating()) {
        LocationVector evacTarget = bot->GetEvacTarget();
        LocationVector pos = me->getPosition();
        float dx = evacTarget.x - (float)pos.x;
        float dz = evacTarget.z - (float)pos.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= 300.0f) {
            // Reached the evacuation Hardline! Jack out safely!
            sBackdoorNetwork.ExecuteCivilianJackout(me->getGoId(), 0);
            return;
        }

        float evacSpeed = 8.5f;
        dx /= dist;
        dz /= dist;
        bot->MoveTo((float)pos.x + dx * evacSpeed * deltaSeconds * 100.0f, (float)pos.y, (float)pos.z + dz * evacSpeed * deltaSeconds * 100.0f);
        return;
    }

    // 1. Calculate & Evolve Emergent Fear Scale (0.0 to 1.0)
    UpdateCivilianFear(bot, me, deltaSeconds);

    // 2. Multi-tier Behavioral Routing
    CivilianFearTier tier = EvaluateCivilianTier(bot->GetFearLevel());

    switch (tier) {
        case CIV_TIER_NORMAL_COMMUTE:
            ExecuteTier0Normal(bot, me, deltaSeconds);
            break;
        case CIV_TIER_UNEASY_RUMORS:
            ExecuteTier1Uneasy(bot, me, deltaSeconds);
            break;
        case CIV_TIER_ALARM_EVASION:
            ExecuteTier2Alarm(bot, me, deltaSeconds);
            break;
        case CIV_TIER_PANIC_STAMPEDE:
            ExecuteTier3Panic(bot, me, deltaSeconds);
            break;
    }
}

void PedestrianEcology::ExecuteTier0Normal(BotClient* bot, PlayerObject* me, float deltaSeconds)
{
    const BotPersonality& personality = bot->GetPersonality();
    LocationVector pos = me->getPosition();
    uint32 now = getMSTime();

    WeatherSystem::CircadianPeriod currentPeriod = sWeatherSys.GetCircadianPeriod();
    PointOfInterest targetPOI = GetCircadianTarget(pos.x, pos.z, currentPeriod, personality);

    float toPdx = targetPOI.x - pos.x;
    float toPdz = targetPOI.z - pos.z;
    float distToPOI = std::sqrt(toPdx * toPdx + toPdz * toPdz);

    if (distToPOI > 1500.0f) {
        toPdx /= distToPOI;
        toPdz /= distToPOI;

        float steerX = 0.0f, steerZ = 0.0f;
        ApplyCrowdSteering(bot, me, steerX, steerZ);

        float walkSpeed = 3.5f;
        float moveX = (toPdx * walkSpeed * 100.0f + steerX * 50.0f) * deltaSeconds;
        float moveZ = (toPdz * walkSpeed * 100.0f + steerZ * 50.0f) * deltaSeconds;
        bot->MoveTo(pos.x + moveX, pos.y, pos.z + moveZ);
    } else {
        InteractWithPOI(bot, me, targetPOI);

        int roll = rand() % 100;
        if (roll < 35) {
            bot->RoamAndSwarm(deltaSeconds);
        } else if (roll < 65) {
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

void PedestrianEcology::ExecuteTier1Uneasy(BotClient* bot, PlayerObject* me, float deltaSeconds)
{
    const BotPersonality& personality = bot->GetPersonality();
    LocationVector pos = me->getPosition();
    uint32 now = getMSTime();

    // Gather around familiar public POIs (noodle bars, diners, benches, payphones, corners)
    POIType gatherType = (personality.extraversion > 0.4f) ? POI_DINER_RESTAURANT : POI_PARK_BENCH;
    PointOfInterest targetPOI = GetNearestPOI(pos.x, pos.z, gatherType);

    float toPdx = targetPOI.x - pos.x;
    float toPdz = targetPOI.z - pos.z;
    float distToPOI = std::sqrt(toPdx * toPdx + toPdz * toPdz);

    if (distToPOI > 800.0f) {
        toPdx /= distToPOI;
        toPdz /= distToPOI;
        float steerX = 0.0f, steerZ = 0.0f;
        ApplyCrowdSteering(bot, me, steerX, steerZ);

        float walkSpeed = 4.0f; // Alert, faster walking gait
        float moveX = (toPdx * walkSpeed * 100.0f + steerX * 40.0f) * deltaSeconds;
        float moveZ = (toPdz * walkSpeed * 100.0f + steerZ * 40.0f) * deltaSeconds;
        bot->MoveTo(pos.x + moveX, pos.y, pos.z + moveZ);
    } else {
        // At gathering spot: whisper outbreak rumors to nearby pedestrians
        auto nearby = sSpatialGrid.GetClientsInRadius(pos.x, pos.z);
        bool gossiped = false;
        for (GameClient* gc : nearby) {
            if (gc == bot || !gc->isBot()) continue;
            BotClient* otherBot = dynamic_cast<BotClient*>(gc);
            if (otherBot) {
                PlayerObject* otherPo = BotGetPlayer(otherBot->GetPlayerGoId());
                if (otherPo && otherPo->getFactionName() == "Civilian") {
                    gossiped = TryOutbreakGossip(bot, otherBot, now);
                    if (gossiped) break;
                }
            }
        }

        // Solitary muttering if nobody to gossip with
        if (!gossiped && (now - bot->GetLastWhisperTime() > 20000) && !m_outbreakRumors.empty()) {
            bot->SetLastWhisperTime(now);
            int rIdx = rand() % m_outbreakRumors.size();
            bot->Say(m_outbreakRumors[rIdx]);
            SpreadRumorFearAura(pos.x, pos.z, 0.08f, 700.0f, me->getGoId());
            bot->Emote(41);
        }
    }

    // 911 Call Escalation
    if (bot->GetFearLevel() >= 0.40f && (now - bot->GetLastDistressCallTime() > 45000)) {
        bot->SetLastDistressCallTime(now);
        uint32 districtId = sMatrixThreatHeatmap.GetDistrictAt(pos.x, pos.z);
        sRadioDispatchSystem.Report911Call(districtId, pos.x, pos.z,
            (format("Bizarre disturbance! Men in black suits multiplying near %1%") % targetPOI.name).str());
    }
}

void PedestrianEcology::ExecuteTier2Alarm(BotClient* bot, PlayerObject* me, float deltaSeconds)
{
    LocationVector pos = me->getPosition();
    uint32 now = getMSTime();

    // 1. Avoid Suspicious Entities (Agents, Smith clones, combatants)
    auto nearby = sSpatialGrid.GetClientsInRadius(pos.x, pos.z);
    float avoidX = 0.0f, avoidZ = 0.0f;
    int threatCount = 0;

    for (GameClient* gc : nearby) {
        if (gc == bot) continue;
        uint32 otherGoId = gc->GetPlayerGoId();
        PlayerObject* otherPo = BotGetPlayer(otherGoId);
        if (!otherPo || otherPo->isDead()) continue;

        BotClient* otherBot = dynamic_cast<BotClient*>(gc);
        bool isThreat = false;
        if (otherBot && otherBot->isAgent()) isThreat = true;
        if (otherPo->getHandle().find("Smith") != std::string::npos || otherPo->getHandle().find("Agent") != std::string::npos) isThreat = true;
        if (otherBot && otherBot->IsInCombat()) isThreat = true;

        if (isThreat) {
            float dx = pos.x - otherPo->getPosition().x;
            float dz = pos.z - otherPo->getPosition().z;
            float distSq = dx * dx + dz * dz;
            if (distSq < 2250000.0f && distSq > 1.0f) { // Within 15m
                float dist = std::sqrt(distSq);
                avoidX += (dx / dist) * (1500.0f - dist);
                avoidZ += (dz / dist) * (1500.0f - dist);
                threatCount++;
            }
        }
    }

    // 2. Refuse to talk & evade interactions
    if (threatCount > 0) {
        float avoidLenSq = avoidX * avoidX + avoidZ * avoidZ;
        if (avoidLenSq > 1.0f) {
            float avoidLen = std::sqrt(avoidLenSq);
            avoidX /= avoidLen;
            avoidZ /= avoidLen;
        }
        float evadeSpeed = 5.5f; // Fast, hurried walking pace
        bot->MoveTo(pos.x + avoidX * evadeSpeed * 100.0f * deltaSeconds, pos.y,
                    pos.z + avoidZ * evadeSpeed * 100.0f * deltaSeconds);
    } else {
        // Steer away from narrow alleyways towards main thoroughfares
        PointOfInterest mainPlaza = GetNearestPOI(pos.x, pos.z, POI_OFFICE_COMMERCIAL);
        float toMdx = mainPlaza.x - pos.x;
        float toMdz = mainPlaza.z - pos.z;
        float mDist = std::sqrt(toMdx * toMdx + toMdz * toMdz);
        if (mDist > 500.0f) {
            toMdx /= mDist;
            toMdz /= mDist;
            bot->MoveTo(pos.x + toMdx * 4.5f * 100.0f * deltaSeconds, pos.y,
                        pos.z + toMdz * 4.5f * 100.0f * deltaSeconds);
        }
    }

    // 3. Nervous glances over shoulder
    if (now - bot->GetLastLookAroundTime() > 4000) {
        bot->SetLastLookAroundTime(now);
        bot->Emote(40); // Sitting/alert glance
        if (!m_evasionWhispers.empty() && (rand() % 100 < 30)) {
            bot->Say(m_evasionWhispers[rand() % m_evasionWhispers.size()]);
        }
    }
}

void PedestrianEcology::ExecuteTier3Panic(BotClient* bot, PlayerObject* me, float deltaSeconds)
{
    const BotPersonality& personality = bot->GetPersonality();
    LocationVector pos = me->getPosition();
    uint32 now = getMSTime();

    uint32 districtId = sMatrixThreatHeatmap.GetDistrictAt(pos.x, pos.z);
    bool cordonActive = IsCordonActive(districtId);

    // Target Shelter: Subway unless cordoned, else Commercial/Residential building
    PointOfInterest dest;
    PointOfInterest nearestSubway = GetNearestSubway(pos.x, pos.z);

    if (cordonActive) {
        // Subway is cordoned! Check if near subway and intercept
        if (CheckCordonInterception(bot, me, nearestSubway.x, nearestSubway.z, 500.0f)) {
            bot->Say("The subway is sealed! The police locked the gates! We're trapped!");
            bot->Emote(50); // Cower
            return;
        }
        dest = GetNearestPOI(pos.x, pos.z, POI_OFFICE_COMMERCIAL);
    } else {
        dest = nearestSubway;
    }

    // Sprint fleeing speed scaled by neuroticism
    float fleeSpeed = 9.0f + (personality.neuroticism * 4.5f);
    float dx = dest.x - pos.x;
    float dz = dest.z - pos.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    if (dist > 250.0f) {
        dx /= dist;
        dz /= dist;
        float moveX = dx * fleeSpeed * deltaSeconds * 100.0f;
        float moveZ = dz * fleeSpeed * deltaSeconds * 100.0f;
        bot->MoveTo(pos.x + moveX, pos.y, pos.z + moveZ);
    } else {
        // Reached shelter
        bot->Say("Made it inside! Lock the doors! They're turning everyone out there!");
        bot->SetFearLevel(0.50f); // Calmed down to Tier 1 inside shelter
    }

    // Panic Vocalizations & Distress Cries
    if (now - bot->GetLastWhisperTime() > 8000) {
        bot->SetLastWhisperTime(now);
        if (!m_panicCries.empty()) {
            bot->Say(m_panicCries[rand() % m_panicCries.size()]);
        }
        // Distress cry triggers chain stampede in nearby crowd
        SpreadRumorFearAura(pos.x, pos.z, 0.20f, 1200.0f, me->getGoId());
    }
}

void PedestrianEcology::DeployTacticalCordon(uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_cordonMutex);
    auto it = m_tacticalCordons.find(districtId);
    if (it != m_tacticalCordons.end() && it->second.active) {
        return; // Already deployed
    }

    // Find subway station in this district
    PointOfInterest subwayTarget;
    bool found = false;
    for (const auto& poi : m_pois) {
        if (poi.districtId == districtId && poi.type == POI_SUBWAY_TRANSIT) {
            subwayTarget = poi;
            found = true;
            break;
        }
    }
    if (!found) {
        subwayTarget = GetNearestSubway(0.0f, 0.0f);
    }

    TacticalCordonPoint cordon;
    cordon.districtId = districtId;
    cordon.x = subwayTarget.x;
    cordon.y = subwayTarget.y;
    cordon.z = subwayTarget.z;
    cordon.name = subwayTarget.name;
    cordon.active = true;
    cordon.deployedMs = getMSTime();

    // Spawn SWAT Breachers & Barricade units around the subway entrance
    auto botLead = sBotMgr.SpawnSingleBot(cordon.x, cordon.y, cordon.z + 200.0f, FACTION_MACHINES);
    if (botLead) {
        PlayerObject* po = BotGetPlayer(botLead->GetPlayerGoId());
        if (po) {
            po->setHandle("SWAT_Tactical_Lead");
            po->setLevel(45);
            po->setMaximumHealth(3500);
            po->setCurrentHealth(3500);
            cordon.cordonBotGoIds.push_back(po->getGoId());
            botLead->Say((format("SWAT Tactical Lead: Perimeter cordon deployed at %1%. Subway concourse is quarantined by Machine Order.") % cordon.name).str());
        }
    }

    auto botBreacher = sBotMgr.SpawnSingleBot(cordon.x - 250.0f, cordon.y, cordon.z + 150.0f, FACTION_MACHINES);
    if (botBreacher) {
        PlayerObject* po = BotGetPlayer(botBreacher->GetPlayerGoId());
        if (po) {
            po->setHandle("SWAT_Cordon_Breacher");
            po->setLevel(40);
            po->setMaximumHealth(3000);
            po->setCurrentHealth(3000);
            cordon.cordonBotGoIds.push_back(po->getGoId());
            botBreacher->Say("SWAT Breacher: Barricade in position. Turn back all civilian traffic!");
        }
    }

    auto botGuard = sBotMgr.SpawnSingleBot(cordon.x + 250.0f, cordon.y, cordon.z + 150.0f, FACTION_MACHINES);
    if (botGuard) {
        PlayerObject* po = BotGetPlayer(botGuard->GetPlayerGoId());
        if (po) {
            po->setHandle("SWAT_Perimeter_Guard");
            po->setLevel(40);
            po->setMaximumHealth(3000);
            po->setCurrentHealth(3000);
            cordon.cordonBotGoIds.push_back(po->getGoId());
        }
    }

    auto botBarricade = sBotMgr.SpawnSingleBot(cordon.x, cordon.y, cordon.z + 350.0f, FACTION_MACHINES);
    if (botBarricade) {
        PlayerObject* po = BotGetPlayer(botBarricade->GetPlayerGoId());
        if (po) {
            po->setHandle("Police_Cruiser_Barricade");
            po->setLevel(35);
            po->setMaximumHealth(2500);
            po->setCurrentHealth(2500);
            cordon.cordonBotGoIds.push_back(po->getGoId());
        }
    }

    m_tacticalCordons[districtId] = cordon;

    // Broadcast scanner announcement
    sRadioDispatchSystem.BroadcastCordonOrder(districtId, cordon.name);
    INFO_LOG(format("PedestrianEcology: Tactical Perimeter Cordon active at %1% (District %2%)") 
             % cordon.name % districtId);
}

bool PedestrianEcology::IsCordonActive(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_cordonMutex);
    auto it = m_tacticalCordons.find(districtId);
    return (it != m_tacticalCordons.end()) ? it->second.active : false;
}

void PedestrianEcology::SetCordonActive(uint32 districtId, bool active)
{
    std::lock_guard<std::recursive_mutex> lock(m_cordonMutex);
    m_tacticalCordons[districtId].active = active;
}

bool PedestrianEcology::CheckCordonInterception(BotClient* bot, PlayerObject* me, float subwayX, float subwayZ, float radius)
{
    if (!bot || !me) return false;
    LocationVector pos = me->getPosition();
    float dx = pos.x - subwayX;
    float dz = pos.z - subwayZ;
    float dist = std::sqrt(dx * dx + dz * dz);

    if (dist < radius) {
        if (dist > 1.0f) {
            dx /= dist;
            dz /= dist;
        } else {
            dx = 1.0f;
            dz = 0.0f;
        }

        // Push civilian back 600 units away from subway
        float bounceX = pos.x + dx * 600.0f;
        float bounceZ = pos.z + dz * 600.0f;
        bot->MoveTo(bounceX, pos.y, bounceZ);

        uint32 now = getMSTime();
        if (now - bot->GetLastWhisperTime() > 5000) {
            bot->SetLastWhisperTime(now);
            const char* swatWarnings[] = {
                "SWAT Breacher: HALT! Subway is locked down under Machine Quarantine! Turn around!",
                "SWAT Tactical Lead: Nobody enters the transit concourse! Fall back inside the sector!",
                "SWAT Guard: Quarantine line active! Return to your homes immediately!"
            };
            bot->Say(swatWarnings[rand() % 3]);
        }
        return true;
    }
    return false;
}

bool PedestrianEcology::BreachTacticalCordon(uint32 districtId, uint32 rescuerSquadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_cordonMutex);
    auto it = m_tacticalCordons.find(districtId);
    if (it == m_tacticalCordons.end() || !it->second.active) {
        return false;
    }

    TacticalCordonPoint& cordon = it->second;
    cordon.active = false;

    // Disorient SWAT cordon guards with EMP disruption canisters
    for (uint32 botId : cordon.cordonBotGoIds) {
        PlayerObject* po = BotGetPlayer(botId);
        if (po && !po->isDead()) {
            po->takeDamage(0, 500, 43); // EMP shock damage
            po->getClient().QueueState(std::make_shared<EmoteMsg>(botId, 43, 1));
            auto bot = sBotMgr.GetBotByGOID(botId);
            if (bot) {
                bot->SetTargetGoId(0);
                bot->Say("SWAT Officer: EMP canister! Optics jammed— our barricade line is collapsing!");
            }
        }
    }

    std::string announcement = (format("[TACTICAL BREACH] Zion Strike Squad %1% breached SWAT cordon at %2%! Subway evacuation corridor OPEN!")
                                % rescuerSquadId % cordon.name).str();
    sBotMgr.LogCombat(announcement);
    INFO_LOG(format("PedestrianEcology: %1%") % announcement);

    RadioTransmission tx;
    tx.transmissionId = 88888;
    tx.tenCode = "10-99-BREACH";
    tx.unitCallsign = "SWAT Tactical Net";
    tx.districtName = sRadioDispatchSystem.ResolveDistrictName(districtId);
    tx.locationAddress = cordon.name;
    tx.threatHeatLevel = 190.0f;
    tx.escalationTier = 4;
    tx.timestampMs = getMSTime();
    tx.squelchToneActive = true;
    tx.chatterText = "[*STATIC*] MAYDAY! SWAT cordon overrun by Zion strike team at " + cordon.name + "! Subway concourse turnstiles breached!";
    sRadioDispatchSystem.BroadcastDispatch(tx);

    return true;
}

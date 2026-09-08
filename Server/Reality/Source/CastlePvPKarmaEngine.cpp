#include "CastlePvPKarmaEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <iomanip>

createFileSingleton(CastlePvPKarmaEngine);

CastlePvPKarmaEngine::CastlePvPKarmaEngine()
{
}

CastlePvPKarmaEngine::~CastlePvPKarmaEngine()
{
}

void CastlePvPKarmaEngine::Initialize()
{
    boost::format fmt("[CastlePvPKarmaEngine] Initialized Vigilante Karma Index, Barrett .50 BMG Sniping & FM 88.3 Radio Network.");
    INFO_LOG(fmt);
}

void CastlePvPKarmaEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords.clear();
    m_sniperPerches.clear();
    m_nextPerchId = 1;
    m_nextInterceptId = 1;
}

void CastlePvPKarmaEngine::Update(float dt)
{
    // Periodic surveillance updates or active tracking scans
    (void)dt;
}

void CastlePvPKarmaEngine::RegisterPlayer(uint32_t playerGoId, const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    if (m_playerRecords.find(playerGoId) == m_playerRecords.end()) {
        PlayerKarmaRecord record;
        record.playerGoId = playerGoId;
        record.playerName = name;
        record.karmaScore = 0;
        record.lastInfractionZone = "None";
        m_playerRecords[playerGoId] = record;
    }
}

void CastlePvPKarmaEngine::ModifyKarma(uint32_t playerGoId, int32_t deltaKarma, const std::string& reason, const std::string& zone)
{
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    auto it = m_playerRecords.find(playerGoId);
    if (it == m_playerRecords.end()) {
        PlayerKarmaRecord record;
        record.playerGoId = playerGoId;
        record.playerName = "Unknown Redpill";
        record.karmaScore = deltaKarma;
        record.lastInfractionZone = zone;
        m_playerRecords[playerGoId] = record;
        it = m_playerRecords.find(playerGoId);
    } else {
        it->second.karmaScore += deltaKarma;
        if (!zone.empty()) {
            it->second.lastInfractionZone = zone;
        }
    }

    // Dynamic alert categorization
    if (it->second.karmaScore < -100) {
        it->second.markedKillOnSight = true;
        it->second.hasActiveSurveillance = true;
    } else if (it->second.karmaScore < -50) {
        it->second.markedKillOnSight = false;
        it->second.hasActiveSurveillance = true;
    } else if (it->second.karmaScore >= 0) {
        it->second.markedKillOnSight = false;
        it->second.hasActiveSurveillance = false;
    }
}

int32_t CastlePvPKarmaEngine::GetPlayerKarma(uint32_t playerGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    auto it = m_playerRecords.find(playerGoId);
    if (it != m_playerRecords.end()) {
        return it->second.karmaScore;
    }
    return 0;
}

PlayerKarmaCategory CastlePvPKarmaEngine::GetPlayerCategory(uint32_t playerGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    auto it = m_playerRecords.find(playerGoId);
    if (it == m_playerRecords.end()) {
        return PlayerKarmaCategory::NeutralObserver;
    }

    int32_t karma = it->second.karmaScore;
    if (karma < -100) return PlayerKarmaCategory::MarkedForDeath;
    if (karma < -50)  return PlayerKarmaCategory::HighPriorityTarget;
    if (karma < 0)    return PlayerKarmaCategory::SuspiciousSuspect;
    if (karma <= 20)  return PlayerKarmaCategory::NeutralObserver;
    return PlayerKarmaCategory::AlliedProtector;
}

const PlayerKarmaRecord* CastlePvPKarmaEngine::GetPlayerRecord(uint32_t playerGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    auto it = m_playerRecords.find(playerGoId);
    if (it != m_playerRecords.end()) {
        return &it->second;
    }
    return nullptr;
}

void CastlePvPKarmaEngine::RecordCivilianMurder(uint32_t playerGoId, const std::string& zone)
{
    ModifyKarma(playerGoId, -50, "Civilian Murder", zone);
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords[playerGoId].civilianKills++;
}

void CastlePvPKarmaEngine::RecordMafiaExtortion(uint32_t playerGoId, const std::string& zone)
{
    ModifyKarma(playerGoId, -30, "Mafia Extortion", zone);
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords[playerGoId].mafiaExtortions++;
}

void CastlePvPKarmaEngine::RecordExileAid(uint32_t playerGoId, const std::string& zone)
{
    ModifyKarma(playerGoId, -40, "Aiding Merovingian Exiles", zone);
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords[playerGoId].exileAids++;
}

void CastlePvPKarmaEngine::RecordAttackOnCastle(uint32_t playerGoId, const std::string& zone)
{
    ModifyKarma(playerGoId, -100, "Direct Assault on Castle", zone);
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords[playerGoId].directCastleAttacks++;
}

void CastlePvPKarmaEngine::RecordCivilianSave(uint32_t playerGoId, const std::string& zone)
{
    ModifyKarma(playerGoId, +25, "Civilian Protection / Hostage Rescue", zone);
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords[playerGoId].civilianSaves++;
}

void CastlePvPKarmaEngine::RecordSafehouseAssist(uint32_t playerGoId, const std::string& zone)
{
    ModifyKarma(playerGoId, +50, "Castle Safehouse Defense Support", zone);
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    m_playerRecords[playerGoId].safehouseAssists++;
}

uint32_t CastlePvPKarmaEngine::RegisterSniperPerch(const std::string& rooftopName, float x, float y, float z)
{
    std::unique_lock<std::shared_mutex> lock(m_karmaMutex);
    uint32_t id = m_nextPerchId++;
    SniperPerch perch;
    perch.perchId = id;
    perch.rooftopName = rooftopName;
    perch.x = x;
    perch.y = y;
    perch.z = z;
    m_sniperPerches[id] = perch;
    return id;
}

bool CastlePvPKarmaEngine::ExecuteSniperAmbush(uint32_t perchId, uint32_t targetPlayerGoId,
                                             float playerX, float playerY, float playerZ,
                                             SniperShotResult& outResult)
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    outResult = SniperShotResult{};

    auto pIt = m_sniperPerches.find(perchId);
    if (pIt == m_sniperPerches.end() || !pIt->second.isActive) {
        return false;
    }

    auto rIt = m_playerRecords.find(targetPlayerGoId);
    if (rIt == m_playerRecords.end()) {
        return false;
    }

    // Only engage targets with negative karma (< -50)
    if (rIt->second.karmaScore >= -50) {
        return false; // Target does not meet engagement threshold
    }

    const SniperPerch& perch = pIt->second;
    float dx = playerX - perch.x;
    float dy = playerY - perch.y;
    float dz = playerZ - perch.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist < perch.minRangeMeters || dist > perch.maxRangeMeters) {
        return false; // Outside optimal sniper sightline envelope (300m - 1000m)
    }

    outResult.targetAcquired = true;
    outResult.distanceMeters = dist;

    // Barrett M82A1 .50 BMG Ballistics:
    // Muzzle Velocity V0 = 853 m/s
    const float V0 = 853.0f;
    const float soundSpeed = 343.0f;
    const float gravity = 9.81f;

    outResult.flightTimeSec = dist / V0;
    
    // Supersonic crack reaches target at bullet arrival, while acoustic muzzle boom arrives at dist / soundSpeed.
    // Acoustic delay before player hears gunshot report:
    outResult.supersonicCrackLeadTimeSec = (dist / soundSpeed) - outResult.flightTimeSec;

    // Ballistic drop: 0.5 * g * t^2
    outResult.ballisticDropMeters = 0.5f * gravity * (outResult.flightTimeSec * outResult.flightTimeSec);

    // Kinetic damage from 12.7x99mm NATO:
    outResult.damageDealt = 350.0f;
    outResult.isKnockdown = true;
    outResult.tinnitusShakeSec = 4.0f;

    return true;
}

bool CastlePvPKarmaEngine::EvaluateRadioIntercept(uint32_t playerGoId, float playerX, float playerY, float playerZ,
                                                float safehouseX, float safehouseY, float safehouseZ,
                                                RadioBroadcastIntercept& outIntercept)
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    outIntercept = RadioBroadcastIntercept{};

    auto it = m_playerRecords.find(playerGoId);
    if (it == m_playerRecords.end()) {
        return false;
    }

    // Proximity check to Castle safehouse / cache
    float dx = playerX - safehouseX;
    float dy = playerY - safehouseY;
    float dz = playerZ - safehouseZ;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist > 200.0f) {
        return false; // Beyond 200m tactical perimeter
    }

    outIntercept.interceptId = m_nextInterceptId++;
    outIntercept.targetPlayerGoId = playerGoId;
    outIntercept.frequencyMhz = "88.3";
    outIntercept.proximityMeters = dist;

    std::string zone = it->second.lastInfractionZone.empty() ? "Richland" : it->second.lastInfractionZone;

    if (it->second.karmaScore < -50) {
        outIntercept.voiceTranscript = "I know what you did in " + zone + ", Redpill. Turn around, or you leave this street in a body bag.";
        outIntercept.triggeredSmokeScreen = true;
    } else if (it->second.karmaScore < 0) {
        outIntercept.voiceTranscript = "Watch your step, operator. You're entering a combat zone on FM 88.3.";
        outIntercept.triggeredSmokeScreen = false;
    } else {
        outIntercept.voiceTranscript = "Safe passage acknowledged, ally. Watch for syndicate patrols ahead.";
        outIntercept.triggeredSmokeScreen = false;
    }

    return true;
}

DownedPlayerFate CastlePvPKarmaEngine::AdjudicateDownedPlayer(uint32_t playerGoId, std::string& outActionMessage,
                                                             uint32_t& outConfiscatedContrabandValue)
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    auto it = m_playerRecords.find(playerGoId);
    if (it == m_playerRecords.end() || it->second.karmaScore >= -50) {
        // Moderate infamy or neutral: Mercy extraction
        outActionMessage = "Castle disarms primary weapon, fires warning shot into asphalt, leaves incapacitated player for Zion hovercraft extraction.";
        outConfiscatedContrabandValue = 0;
        return DownedPlayerFate::MercyExtraction;
    }

    // Severe infamy (Karma < -50, especially < -100): Execution
    outActionMessage = "Castle delivers execution headshot; obituary broadcast to Megacity tactical net; illicit syndicate contraband confiscated.";
    outConfiscatedContrabandValue = 15000; // $15,000 illicit bounty confiscated
    return DownedPlayerFate::ExecutedWithInfamy;
}

bool CastlePvPKarmaEngine::GrantSafehouseSupplyCrateCode(uint32_t playerGoId, std::string& outCrateUnlockCode)
{
    std::shared_lock<std::shared_mutex> lock(m_karmaMutex);
    auto it = m_playerRecords.find(playerGoId);
    if (it != m_playerRecords.end() && it->second.karmaScore >= 20) {
        outCrateUnlockCode = "CASTLE-SUPPLY-FM883-TUNGSTEN";
        return true;
    }
    outCrateUnlockCode = "";
    return false;
}

// ============================================================================
// Headless Test Suite 40: Frank Castle vs. Human Players PvP Karma Engine
// ============================================================================

void RunCastlePvPKarmaTestSuite()
{
    std::cout << "[RUNNING] Suite 40: Frank Castle vs. Human Players PvP Karma Engine..." << std::endl;
    sCastlePvPKarmaEngine.ResetForTesting();

    // 1. Player Karma & Infraction Tracking
    uint32_t playerCorrupt = 201;
    uint32_t playerHero = 202;
    sCastlePvPKarmaEngine.RegisterPlayer(playerCorrupt, "CorruptOperator");
    sCastlePvPKarmaEngine.RegisterPlayer(playerHero, "ZionVanguard");

    assert(sCastlePvPKarmaEngine.GetPlayerKarma(playerCorrupt) == 0);
    assert(sCastlePvPKarmaEngine.GetPlayerCategory(playerCorrupt) == PlayerKarmaCategory::NeutralObserver);

    // Corrupt actions: Civilian murder (-50), Extortion (-30), Aiding Exiles (-40)
    sCastlePvPKarmaEngine.RecordCivilianMurder(playerCorrupt, "Slums Depository");
    assert(sCastlePvPKarmaEngine.GetPlayerKarma(playerCorrupt) == -50);
    assert(sCastlePvPKarmaEngine.GetPlayerCategory(playerCorrupt) == PlayerKarmaCategory::SuspiciousSuspect);

    sCastlePvPKarmaEngine.RecordMafiaExtortion(playerCorrupt, "Chinatown Racket");
    assert(sCastlePvPKarmaEngine.GetPlayerKarma(playerCorrupt) == -80);
    assert(sCastlePvPKarmaEngine.GetPlayerCategory(playerCorrupt) == PlayerKarmaCategory::HighPriorityTarget);

    sCastlePvPKarmaEngine.RecordExileAid(playerCorrupt, "Club Hel");
    assert(sCastlePvPKarmaEngine.GetPlayerKarma(playerCorrupt) == -120);
    assert(sCastlePvPKarmaEngine.GetPlayerCategory(playerCorrupt) == PlayerKarmaCategory::MarkedForDeath);

    // Hero actions: Save civilians (+25), Safehouse assist (+50)
    sCastlePvPKarmaEngine.RecordCivilianSave(playerHero, "International Subway");
    sCastlePvPKarmaEngine.RecordSafehouseAssist(playerHero, "Industrial Foundry");
    assert(sCastlePvPKarmaEngine.GetPlayerKarma(playerHero) == +75);
    assert(sCastlePvPKarmaEngine.GetPlayerCategory(playerHero) == PlayerKarmaCategory::AlliedProtector);

    // 2. Barrett M82A1 .50 BMG Rooftop Sniper Ambush
    uint32_t perchId = sCastlePvPKarmaEngine.RegisterSniperPerch("Richland Clocktower", 0.0f, 0.0f, 120.0f);
    assert(perchId == 1);

    // Hero at 500m distance: Castle does NOT engage (Karma is positive)
    SniperShotResult heroShot;
    bool engagedHero = sCastlePvPKarmaEngine.ExecuteSniperAmbush(perchId, playerHero, 500.0f, 0.0f, 0.0f, heroShot);
    assert(!engagedHero);

    // Corrupt player at 500m distance (sightline within 300m - 1000m): Castle executes ambush!
    SniperShotResult corruptShot;
    bool engagedCorrupt = sCastlePvPKarmaEngine.ExecuteSniperAmbush(perchId, playerCorrupt, 500.0f, 0.0f, 0.0f, corruptShot);
    assert(engagedCorrupt);
    assert(corruptShot.targetAcquired);
    assert(corruptShot.distanceMeters > 500.0f);
    assert(corruptShot.flightTimeSec > 0.5f && corruptShot.flightTimeSec < 0.7f); // ~514m / 853m/s ~ 0.60s
    assert(corruptShot.supersonicCrackLeadTimeSec > 0.8f); // Acoustic sound delay before boom
    assert(corruptShot.ballisticDropMeters > 1.0f);
    assert(corruptShot.damageDealt == 350.0f);
    assert(corruptShot.isKnockdown);

    // 3. FM 88.3 Encrypted Radio Intercept within 200m
    RadioBroadcastIntercept intercept;
    bool radioTriggered = sCastlePvPKarmaEngine.EvaluateRadioIntercept(
        playerCorrupt, 50.0f, 50.0f, 0.0f, 0.0f, 0.0f, 0.0f, intercept);
    assert(radioTriggered);
    assert(intercept.frequencyMhz == "88.3");
    assert(intercept.triggeredSmokeScreen);
    assert(intercept.voiceTranscript.find("leave this street in a body bag") != std::string::npos);

    // 4. Downed Player Judgment & Mercy Bifurcation
    // Moderate infamy player (-30) -> Mercy extraction
    uint32_t playerModerate = 203;
    sCastlePvPKarmaEngine.RegisterPlayer(playerModerate, "ModerateThug");
    sCastlePvPKarmaEngine.RecordMafiaExtortion(playerModerate, "Westview");

    std::string actionMsg;
    uint32_t loot = 0;
    DownedPlayerFate fateMod = sCastlePvPKarmaEngine.AdjudicateDownedPlayer(playerModerate, actionMsg, loot);
    assert(fateMod == DownedPlayerFate::MercyExtraction);
    assert(loot == 0);
    assert(actionMsg.find("Mercy") != std::string::npos || actionMsg.find("warning shot") != std::string::npos);

    // Severe infamy player (-120) -> Execution
    DownedPlayerFate fateSevere = sCastlePvPKarmaEngine.AdjudicateDownedPlayer(playerCorrupt, actionMsg, loot);
    assert(fateSevere == DownedPlayerFate::ExecutedWithInfamy);
    assert(loot == 15000);
    assert(actionMsg.find("execution headshot") != std::string::npos);

    // 5. Safehouse Supply Crate Code Grant
    std::string code;
    bool grantedHero = sCastlePvPKarmaEngine.GrantSafehouseSupplyCrateCode(playerHero, code);
    assert(grantedHero);
    assert(!code.empty());

    bool grantedCorrupt = sCastlePvPKarmaEngine.GrantSafehouseSupplyCrateCode(playerCorrupt, code);
    assert(!grantedCorrupt);
    assert(code.empty());

    std::cout << "[PASSED] Suite 40: Frank Castle vs. Human Players PvP Karma Engine (30 assertions passed)." << std::endl;
}

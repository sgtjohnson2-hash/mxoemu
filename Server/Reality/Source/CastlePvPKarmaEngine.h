#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Frank Castle Total War: Front II - Castle vs. Human Players PvP Karma Engine
// Vigilante Karma Index (VKI), Barrett .50 BMG Sniping, FM 88.3 Psychological
// Radio Intercepts & Downed Player Judgment / Mercy Bifurcation
// ============================================================================

enum class PlayerKarmaCategory
{
    AlliedProtector,    // Karma > 20 (Truce / Respected ally)
    NeutralObserver,    // 0 <= Karma <= 20
    SuspiciousSuspect,  // -50 <= Karma < 0 (Active surveillance)
    HighPriorityTarget, // -100 <= Karma < -50 (Sniping queue)
    MarkedForDeath      // Karma < -100 (Immediate lethal purge)
};

enum class DownedPlayerFate
{
    MercyExtraction,    // Moderate Infamy: Disarmed & left for Zion medics
    ExecutedWithInfamy  // Severe Infamy: Tactical headshot & contraband confiscated
};

struct CastlePvPKarmaRecord
{
    uint32_t playerGoId{0};
    std::string playerName;
    int32_t karmaScore{0};
    uint32_t civilianKills{0};
    uint32_t mafiaExtortions{0};
    uint32_t exileAids{0};
    uint32_t directCastleAttacks{0};
    uint32_t civilianSaves{0};
    uint32_t safehouseAssists{0};
    bool hasActiveSurveillance{false};
    bool markedKillOnSight{false};
    std::string lastInfractionZone;
};

struct SniperPerch
{
    uint32_t perchId{0};
    std::string rooftopName;
    float x{0.0f}, y{0.0f}, z{0.0f}; // Elevated position (rooftop Z)
    float minRangeMeters{300.0f};
    float maxRangeMeters{1000.0f};
    bool isActive{true};
};

struct SniperShotResult
{
    bool targetAcquired{false};
    float distanceMeters{0.0f};
    float flightTimeSec{0.0f};
    float supersonicCrackLeadTimeSec{0.0f};
    float ballisticDropMeters{0.0f};
    float damageDealt{0.0f};
    bool isKnockdown{false};
    float tinnitusShakeSec{0.0f};
};

struct RadioBroadcastIntercept
{
    uint32_t interceptId{0};
    uint32_t targetPlayerGoId{0};
    std::string frequencyMhz{"88.3"};
    std::string voiceTranscript;
    float proximityMeters{0.0f};
    bool triggeredSmokeScreen{false};
};

class CastlePvPKarmaEngine : public Singleton<CastlePvPKarmaEngine>
{
public:
    CastlePvPKarmaEngine();
    ~CastlePvPKarmaEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Vigilante Karma Index (VKI) Tracking
    void RegisterPlayer(uint32_t playerGoId, const std::string& name);
    void ModifyKarma(uint32_t playerGoId, int32_t deltaKarma, const std::string& reason, const std::string& zone = "");
    int32_t GetPlayerKarma(uint32_t playerGoId) const;
    PlayerKarmaCategory GetPlayerCategory(uint32_t playerGoId) const;
    const CastlePvPKarmaRecord* GetPlayerRecord(uint32_t playerGoId) const;

    // Specific Infractions & Merits
    void RecordCivilianMurder(uint32_t playerGoId, const std::string& zone);
    void RecordMafiaExtortion(uint32_t playerGoId, const std::string& zone);
    void RecordExileAid(uint32_t playerGoId, const std::string& zone);
    void RecordAttackOnCastle(uint32_t playerGoId, const std::string& zone);
    void RecordCivilianSave(uint32_t playerGoId, const std::string& zone);
    void RecordSafehouseAssist(uint32_t playerGoId, const std::string& zone);

    // 2. Barrett M82A1 .50 BMG Rooftop Sniper Perches
    uint32_t RegisterSniperPerch(const std::string& rooftopName, float x, float y, float z);
    bool ExecuteSniperAmbush(uint32_t perchId, uint32_t targetPlayerGoId,
                             float playerX, float playerY, float playerZ,
                             SniperShotResult& outResult);

    // 3. FM 88.3 Encrypted Radio Intercepts & Psychological Warfare
    bool EvaluateRadioIntercept(uint32_t playerGoId, float playerX, float playerY, float playerZ,
                                float safehouseX, float safehouseY, float safehouseZ,
                                RadioBroadcastIntercept& outIntercept);

    // 4. Downed Player Judgment & Mercy Resolution
    DownedPlayerFate AdjudicateDownedPlayer(uint32_t playerGoId, std::string& outActionMessage,
                                           uint32_t& outConfiscatedContrabandValue);

    // 5. Safehouse Reward Grants
    bool GrantSafehouseSupplyCrateCode(uint32_t playerGoId, std::string& outCrateUnlockCode);

private:
    mutable std::shared_mutex m_karmaMutex;
    std::unordered_map<uint32_t, CastlePvPKarmaRecord> m_playerRecords;
    std::unordered_map<uint32_t, SniperPerch> m_sniperPerches;
    uint32_t m_nextPerchId{1};
    uint32_t m_nextInterceptId{1};
};

#define sCastlePvPKarmaEngine CastlePvPKarmaEngine::getSingleton()

void RunCastlePvPKarmaTestSuite();

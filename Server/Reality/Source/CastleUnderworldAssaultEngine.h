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
// Frank Castle Total War: Front III - Castle vs. Other Megacity AI Engine
// Syndicate Racket Raids, Anti-Exile Supernatural Munitions, MMPD Selective
// Rules of Engagement & Subterranean Sentinel Swarm Defense
// ============================================================================

struct SyndicateRacketTarget
{
    uint32_t racketId{0};
    std::string racketName;
    std::string district;
    uint32_t vaultCashAmount{50000};
    bool isRaided{false};
    bool isIncineratedByThermite{false};
};

struct SyndicateBossTarget
{
    uint32_t bossGoId{0};
    std::string bossName;
    std::string syndicateFaction;
    bool isTracked{false};
    bool isConvoyAmbushed{false};
    bool isNeutralized{false};
};

enum class ExileSpecies
{
    Lupine,         // Werewolves (Extreme regen, vulnerable to Silver HP & Thermite)
    Vampire,        // Blood Enforcers (Speed, vulnerable to UV Phosphor & Wooden Stakes)
    PhaseShifter    // Club Hel Twins (Phase-shifting, vulnerable to Electrostatic Harpoon)
};

struct ExileEntity
{
    uint32_t entityGoId{0};
    ExileSpecies species{ExileSpecies::Lupine};
    std::string name;
    float currentHealth{500.0f};
    float maxHealth{500.0f};
    bool regenerationSuppressed{false};
    float regenSuppressionTimerSec{0.0f};
    bool isBlinded{false};
    float blindDurationRemainingSec{0.0f};
    bool isPhaseGrounded{false};
    bool isNeutralized{false};
};

struct MMPDOfficer
{
    uint32_t officerGoId{0};
    std::string badgeNumber;
    bool isCorrupt{false};
    bool evidenceLoggedToIAD{false};
    bool isIncapacitatedNonLethal{false};
    bool isLethallyNeutralized{false};
};

struct SentinelDrone
{
    uint32_t sentinelId{0};
    float x{0.0f}, y{0.0f}, z{0.0f};
    bool tendrilsSevered{false};
    bool antigravityCoilDisabled{false};
    bool isDestroyed{false};
};

class CastleUnderworldAssaultEngine : public Singleton<CastleUnderworldAssaultEngine>
{
public:
    CastleUnderworldAssaultEngine();
    ~CastleUnderworldAssaultEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Syndicate Racket Raids & Boss Contracts
    void RegisterRacket(uint32_t racketId, const std::string& name, const std::string& district, uint32_t vaultCash = 50000);
    bool ExecuteRacketThermiteRaid(uint32_t racketId, uint32_t& outIncineratedVaultCash);
    const SyndicateRacketTarget* GetRacket(uint32_t racketId) const;

    void RegisterSyndicateBoss(uint32_t bossGoId, const std::string& name, const std::string& faction);
    bool AmbushBossConvoy(uint32_t bossGoId, bool useSpikeStrips, bool useBreachingShotgun);
    const SyndicateBossTarget* GetBoss(uint32_t bossGoId) const;

    // 2. Anti-Exile Supernatural Warfare
    void RegisterExile(uint32_t entityGoId, ExileSpecies species, const std::string& name, float hp = 500.0f);
    bool FireSilverHollowPoint(uint32_t lupineGoId, float damage, float& outNewHp, bool& outRegenDisabled);
    bool DeployUVPhosphorCanister(uint32_t vampireGoId, float& outBlindDurationSec);
    bool FireElectrostaticHarpoon(uint32_t shifterGoId, bool& outPhaseLocked);
    const ExileEntity* GetExile(uint32_t entityGoId) const;

    // 3. MMPD Rules of Engagement (Honest vs. Corrupt)
    void RegisterMMPDOfficer(uint32_t officerGoId, const std::string& badge, bool isCorrupt);
    bool EngageMMPDOfficer(uint32_t officerGoId, bool& outUsedNonLethalBeanbag, bool& outLethalEngaged,
                          std::string& outActionReport);
    void DeliverEvidenceToIADCruiser42(const std::string& evidenceDossier);
    size_t GetIADLoggedDossierCount() const;

    // 4. Subterranean Sentinel Swarm Defense
    void RegisterSentinel(uint32_t sentinelId, float x, float y, float z);
    bool DetonateChokepointEMPClaymore(uint32_t sentinelId, bool& outTendrilsSevered, bool& outAntigravDisabled);
    bool FireTwinVulcanTurret(uint32_t sentinelId, float burstDurationSec, bool& outDestroyed);
    const SentinelDrone* GetSentinel(uint32_t sentinelId) const;

private:
    mutable std::shared_mutex m_assaultMutex;
    std::unordered_map<uint32_t, SyndicateRacketTarget> m_rackets;
    std::unordered_map<uint32_t, SyndicateBossTarget> m_bosses;
    std::unordered_map<uint32_t, ExileEntity> m_exiles;
    std::unordered_map<uint32_t, MMPDOfficer> m_officers;
    std::unordered_map<uint32_t, SentinelDrone> m_sentinels;
    std::vector<std::string> m_iadDossiers;
};

#define sCastleUnderworldAssaultEngine CastleUnderworldAssaultEngine::getSingleton()

void RunCastleUnderworldAssaultTestSuite();

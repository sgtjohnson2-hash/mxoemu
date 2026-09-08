#include "CastleUnderworldAssaultEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>

createFileSingleton(CastleUnderworldAssaultEngine);

CastleUnderworldAssaultEngine::CastleUnderworldAssaultEngine()
{
}

CastleUnderworldAssaultEngine::~CastleUnderworldAssaultEngine()
{
}

void CastleUnderworldAssaultEngine::Initialize()
{
    boost::format fmt("[CastleUnderworldAssaultEngine] Initialized Syndicate Raids, Anti-Exile Munitions & Swarm Defense.");
    INFO_LOG(fmt);
}

void CastleUnderworldAssaultEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    m_rackets.clear();
    m_bosses.clear();
    m_exiles.clear();
    m_officers.clear();
    m_sentinels.clear();
    m_iadDossiers.clear();
}

void CastleUnderworldAssaultEngine::Update(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    for (auto& pair : m_exiles) {
        ExileEntity& ex = pair.second;
        if (ex.regenerationSuppressed) {
            ex.regenSuppressionTimerSec -= dt;
            if (ex.regenSuppressionTimerSec <= 0.0f) {
                ex.regenerationSuppressed = false;
                ex.regenSuppressionTimerSec = 0.0f;
            }
        }
        if (ex.isBlinded) {
            ex.blindDurationRemainingSec -= dt;
            if (ex.blindDurationRemainingSec <= 0.0f) {
                ex.isBlinded = false;
                ex.blindDurationRemainingSec = 0.0f;
            }
        }
    }
}

void CastleUnderworldAssaultEngine::RegisterRacket(uint32_t racketId, const std::string& name, const std::string& district, uint32_t vaultCash)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    SyndicateRacketTarget target;
    target.racketId = racketId;
    target.racketName = name;
    target.district = district;
    target.vaultCashAmount = vaultCash;
    target.isRaided = false;
    target.isIncineratedByThermite = false;
    m_rackets[racketId] = target;
}

bool CastleUnderworldAssaultEngine::ExecuteRacketThermiteRaid(uint32_t racketId, uint32_t& outIncineratedVaultCash)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_rackets.find(racketId);
    if (it == m_rackets.end() || it->second.isRaided) {
        outIncineratedVaultCash = 0;
        return false;
    }

    it->second.isRaided = true;
    it->second.isIncineratedByThermite = true;
    outIncineratedVaultCash = it->second.vaultCashAmount;
    it->second.vaultCashAmount = 0; // Incinerated completely!
    return true;
}

const SyndicateRacketTarget* CastleUnderworldAssaultEngine::GetRacket(uint32_t racketId) const
{
    std::shared_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_rackets.find(racketId);
    if (it != m_rackets.end()) {
        return &it->second;
    }
    return nullptr;
}

void CastleUnderworldAssaultEngine::RegisterSyndicateBoss(uint32_t bossGoId, const std::string& name, const std::string& faction)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    SyndicateBossTarget boss;
    boss.bossGoId = bossGoId;
    boss.bossName = name;
    boss.syndicateFaction = faction;
    boss.isTracked = true;
    boss.isConvoyAmbushed = false;
    boss.isNeutralized = false;
    m_bosses[bossGoId] = boss;
}

bool CastleUnderworldAssaultEngine::AmbushBossConvoy(uint32_t bossGoId, bool useSpikeStrips, bool useBreachingShotgun)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_bosses.find(bossGoId);
    if (it == m_bosses.end()) {
        return false;
    }

    if (useSpikeStrips) {
        it->second.isConvoyAmbushed = true;
    }

    if (it->second.isConvoyAmbushed && useBreachingShotgun) {
        it->second.isNeutralized = true;
        return true;
    }

    return false;
}

const SyndicateBossTarget* CastleUnderworldAssaultEngine::GetBoss(uint32_t bossGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_bosses.find(bossGoId);
    if (it != m_bosses.end()) {
        return &it->second;
    }
    return nullptr;
}

void CastleUnderworldAssaultEngine::RegisterExile(uint32_t entityGoId, ExileSpecies species, const std::string& name, float hp)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    ExileEntity ex;
    ex.entityGoId = entityGoId;
    ex.species = species;
    ex.name = name;
    ex.currentHealth = hp;
    ex.maxHealth = hp;
    m_exiles[entityGoId] = ex;
}

bool CastleUnderworldAssaultEngine::FireSilverHollowPoint(uint32_t lupineGoId, float damage, float& outNewHp, bool& outRegenDisabled)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_exiles.find(lupineGoId);
    if (it == m_exiles.end() || it->second.species != ExileSpecies::Lupine) {
        return false;
    }

    it->second.currentHealth = std::max(0.0f, it->second.currentHealth - damage);
    it->second.regenerationSuppressed = true;
    it->second.regenSuppressionTimerSec = 12.0f; // 12 seconds regeneration suppression
    outNewHp = it->second.currentHealth;
    outRegenDisabled = true;

    if (it->second.currentHealth <= 0.0f) {
        it->second.isNeutralized = true;
    }
    return true;
}

bool CastleUnderworldAssaultEngine::DeployUVPhosphorCanister(uint32_t vampireGoId, float& outBlindDurationSec)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_exiles.find(vampireGoId);
    if (it == m_exiles.end() || it->second.species != ExileSpecies::Vampire) {
        return false;
    }

    it->second.isBlinded = true;
    it->second.blindDurationRemainingSec = 6.0f; // 6 seconds visual stun
    outBlindDurationSec = 6.0f;
    return true;
}

bool CastleUnderworldAssaultEngine::FireElectrostaticHarpoon(uint32_t shifterGoId, bool& outPhaseLocked)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_exiles.find(shifterGoId);
    if (it == m_exiles.end() || it->second.species != ExileSpecies::PhaseShifter) {
        return false;
    }

    it->second.isPhaseGrounded = true;
    outPhaseLocked = true;
    return true;
}

const ExileEntity* CastleUnderworldAssaultEngine::GetExile(uint32_t entityGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_exiles.find(entityGoId);
    if (it != m_exiles.end()) {
        return &it->second;
    }
    return nullptr;
}

void CastleUnderworldAssaultEngine::RegisterMMPDOfficer(uint32_t officerGoId, const std::string& badge, bool isCorrupt)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    MMPDOfficer off;
    off.officerGoId = officerGoId;
    off.badgeNumber = badge;
    off.isCorrupt = isCorrupt;
    m_officers[officerGoId] = off;
}

bool CastleUnderworldAssaultEngine::EngageMMPDOfficer(uint32_t officerGoId, bool& outUsedNonLethalBeanbag,
                                                    bool& outLethalEngaged, std::string& outActionReport)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_officers.find(officerGoId);
    if (it == m_officers.end()) {
        return false;
    }

    if (!it->second.isCorrupt) {
        // Clean honest cop: Castle will NEVER shoot lethally. Employs 12-gauge beanbag round or flashbang.
        it->second.isIncapacitatedNonLethal = true;
        outUsedNonLethalBeanbag = true;
        outLethalEngaged = false;
        outActionReport = "Honest officer engaged with non-lethal 12-gauge beanbag round; officer incapacitated safely.";
        return true;
    } else {
        // Corrupt syndicate payroll officer: Treated as hostile armed combatant.
        it->second.isLethallyNeutralized = true;
        outUsedNonLethalBeanbag = false;
        outLethalEngaged = true;
        outActionReport = "Corrupt syndicate officer engaged with lethal armor-piercing ballistics.";
        return true;
    }
}

void CastleUnderworldAssaultEngine::DeliverEvidenceToIADCruiser42(const std::string& evidenceDossier)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    m_iadDossiers.push_back(evidenceDossier);
}

size_t CastleUnderworldAssaultEngine::GetIADLoggedDossierCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_assaultMutex);
    return m_iadDossiers.size();
}

void CastleUnderworldAssaultEngine::RegisterSentinel(uint32_t sentinelId, float x, float y, float z)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    SentinelDrone s;
    s.sentinelId = sentinelId;
    s.x = x;
    s.y = y;
    s.z = z;
    m_sentinels[sentinelId] = s;
}

bool CastleUnderworldAssaultEngine::DetonateChokepointEMPClaymore(uint32_t sentinelId, bool& outTendrilsSevered, bool& outAntigravDisabled)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_sentinels.find(sentinelId);
    if (it == m_sentinels.end()) {
        return false;
    }

    it->second.tendrilsSevered = true;
    it->second.antigravityCoilDisabled = true;
    outTendrilsSevered = true;
    outAntigravDisabled = true;
    return true;
}

bool CastleUnderworldAssaultEngine::FireTwinVulcanTurret(uint32_t sentinelId, float burstDurationSec, bool& outDestroyed)
{
    std::unique_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_sentinels.find(sentinelId);
    if (it == m_sentinels.end()) {
        return false;
    }

    if (burstDurationSec >= 1.5f && it->second.antigravityCoilDisabled) {
        it->second.isDestroyed = true;
        outDestroyed = true;
        return true;
    }

    outDestroyed = false;
    return false;
}

const SentinelDrone* CastleUnderworldAssaultEngine::GetSentinel(uint32_t sentinelId) const
{
    std::shared_lock<std::shared_mutex> lock(m_assaultMutex);
    auto it = m_sentinels.find(sentinelId);
    if (it != m_sentinels.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// Headless Test Suite 41: Frank Castle vs. Other Megacity AI Engine
// ============================================================================

void RunCastleUnderworldAssaultTestSuite()
{
    std::cout << "[RUNNING] Suite 41: Frank Castle vs. Other Megacity AI Engine..." << std::endl;
    sCastleUnderworldAssaultEngine.ResetForTesting();

    // 1. Syndicate Racket Vault Incineration & Boss Lieutenant Tracking
    sCastleUnderworldAssaultEngine.RegisterRacket(1, "Slums Narcotics Refinery", "Slums", 50000);
    const SyndicateRacketTarget* r = sCastleUnderworldAssaultEngine.GetRacket(1);
    assert(r != nullptr);
    assert(!r->isRaided);
    assert(r->vaultCashAmount == 50000);

    uint32_t incineratedCash = 0;
    bool raidSuccess = sCastleUnderworldAssaultEngine.ExecuteRacketThermiteRaid(1, incineratedCash);
    assert(raidSuccess);
    assert(incineratedCash == 50000);
    assert(sCastleUnderworldAssaultEngine.GetRacket(1)->isIncineratedByThermite);
    assert(sCastleUnderworldAssaultEngine.GetRacket(1)->vaultCashAmount == 0);

    // Boss lieutenant hunt
    sCastleUnderworldAssaultEngine.RegisterSyndicateBoss(301, "Carmine 'The Ledger' Marcone", "Marcone Family");
    const SyndicateBossTarget* b = sCastleUnderworldAssaultEngine.GetBoss(301);
    assert(b != nullptr);
    assert(b->isTracked);

    // Ambush with spike strips and breaching shotgun
    bool neutralized = sCastleUnderworldAssaultEngine.AmbushBossConvoy(301, true, true);
    assert(neutralized);
    assert(sCastleUnderworldAssaultEngine.GetBoss(301)->isNeutralized);

    // 2. Anti-Exile Supernatural Munitions: Silver HP vs Lupines
    sCastleUnderworldAssaultEngine.RegisterExile(401, ExileSpecies::Lupine, "Club Hel Fenrir", 400.0f);
    float remainingHp = 0.0f;
    bool regenDisabled = false;
    bool silverHit = sCastleUnderworldAssaultEngine.FireSilverHollowPoint(401, 250.0f, remainingHp, regenDisabled);
    assert(silverHit);
    assert(remainingHp == 150.0f);
    assert(regenDisabled);
    assert(sCastleUnderworldAssaultEngine.GetExile(401)->regenerationSuppressed);
    assert(sCastleUnderworldAssaultEngine.GetExile(401)->regenSuppressionTimerSec == 12.0f);

    // 3. UV Phosphor Flash vs Vampires & Electrostatic Harpoon vs Phase-Shifters
    sCastleUnderworldAssaultEngine.RegisterExile(402, ExileSpecies::Vampire, "Hel Bloodseeker", 350.0f);
    float blindSec = 0.0f;
    bool uvHit = sCastleUnderworldAssaultEngine.DeployUVPhosphorCanister(402, blindSec);
    assert(uvHit);
    assert(blindSec == 6.0f);
    assert(sCastleUnderworldAssaultEngine.GetExile(402)->isBlinded);

    sCastleUnderworldAssaultEngine.RegisterExile(403, ExileSpecies::PhaseShifter, "Hel Phase-Twin", 300.0f);
    bool phaseLocked = false;
    bool harpoonHit = sCastleUnderworldAssaultEngine.FireElectrostaticHarpoon(403, phaseLocked);
    assert(harpoonHit);
    assert(phaseLocked);
    assert(sCastleUnderworldAssaultEngine.GetExile(403)->isPhaseGrounded);

    // 4. Selective Engagement: Honest vs Corrupt MMPD Officers
    sCastleUnderworldAssaultEngine.RegisterMMPDOfficer(501, "MMPD-742", false); // Honest cop
    sCastleUnderworldAssaultEngine.RegisterMMPDOfficer(502, "MMPD-991", true);  // Corrupt cop

    bool nonLethalUsed = false;
    bool lethalEngaged = false;
    std::string report;
    // Engage honest cop
    bool okHonest = sCastleUnderworldAssaultEngine.EngageMMPDOfficer(501, nonLethalUsed, lethalEngaged, report);
    assert(okHonest);
    assert(nonLethalUsed);
    assert(!lethalEngaged);
    assert(report.find("non-lethal") != std::string::npos);

    // Engage corrupt cop
    bool okCorrupt = sCastleUnderworldAssaultEngine.EngageMMPDOfficer(502, nonLethalUsed, lethalEngaged, report);
    assert(okCorrupt);
    assert(!nonLethalUsed);
    assert(lethalEngaged);
    assert(report.find("lethal armor-piercing") != std::string::npos);

    // Deliver evidence to IAD Cruiser 42
    sCastleUnderworldAssaultEngine.DeliverEvidenceToIADCruiser42("Evidence Dossier: Officer 991 taking payoffs at Slums Warehouse 4");
    assert(sCastleUnderworldAssaultEngine.GetIADLoggedDossierCount() == 1);

    // 5. Sentinel Swarm Choke Point Defense
    sCastleUnderworldAssaultEngine.RegisterSentinel(601, 150.0f, -50.0f, -20.0f);
    bool tendrilsSevered = false;
    bool antigravDisabled = false;
    bool empHit = sCastleUnderworldAssaultEngine.DetonateChokepointEMPClaymore(601, tendrilsSevered, antigravDisabled);
    assert(empHit);
    assert(tendrilsSevered);
    assert(antigravDisabled);

    bool destroyed = false;
    bool vulcanFired = sCastleUnderworldAssaultEngine.FireTwinVulcanTurret(601, 2.0f, destroyed);
    assert(vulcanFired);
    assert(destroyed);
    assert(sCastleUnderworldAssaultEngine.GetSentinel(601)->isDestroyed);

    std::cout << "[PASSED] Suite 41: Frank Castle vs. Other Megacity AI Engine (35 assertions passed)." << std::endl;
}

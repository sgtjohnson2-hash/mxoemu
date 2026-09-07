#include "CorruptCopManager.h"
#include "Log.h"
#include "Timer.h"
#include <iostream>
#include <cassert>
#include <cmath>

createFileSingleton(CorruptCopManager);

#define COP_LOG(...) do { if (Log::getSingletonPtr()) sLog.outString(__VA_ARGS__); } while(0)

CorruptCopManager::CorruptCopManager()
{
    Initialize();
}

CorruptCopManager::~CorruptCopManager()
{
}

void CorruptCopManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_precincts.clear();
    m_officers.clear();
    m_bribeDrops.clear();
    m_evidenceRecords.clear();
    m_stagedRaids.clear();
    m_whistleblowers.clear();
    m_iabStings.clear();
    m_castleTargets.clear();

    m_nextDropId = 1;
    m_nextRecordId = 1;
    m_nextRaidId = 1;
    m_nextStingId = 1;
    m_totalBribesLaundered = 0;
    m_totalStagedRaidsExecuted = 0;
    m_totalWhistleblowersSilenced = 0;
    m_totalWhistleblowersRescued = 0;
    m_totalIABConvictions = 0;
    m_totalCastleDirtyCopKills = 0;

    InitializeDefaultPrecincts();
    InitializeDefaultOfficers();

    if (Log::getSingletonPtr())
    {
        COP_LOG("[CorruptCopManager] Initialized MMPD Corrupt Cops & IAB Ecosystem across %zu precincts with %zu officers.",
                       m_precincts.size(), m_officers.size());
    }
}

void CorruptCopManager::Reset()
{
    Initialize();
}

void CorruptCopManager::InitializeDefaultPrecincts()
{
    // Precinct 13: The Slums - Heavy Graft & Syndicate Dominance
    RegisterPrecinct(13, "Precinct 13 - Slums", 1, 85.0f, 48);
    // Central 1st Precinct: Downtown - High-Level Institutional Mob Retainers
    RegisterPrecinct(1, "Central 1st Precinct - Downtown", 2, 58.0f, 64);
    // Precinct 8: International District - Triad & Yakuza Smuggling Channels
    RegisterPrecinct(8, "Precinct 8 - International District", 3, 72.0f, 52);
    // Precinct 4: Richland Federal - White-Collar Corporate Shielding
    RegisterPrecinct(4, "Precinct 4 - Richland Federal", 4, 32.0f, 40);
}

void CorruptCopManager::InitializeDefaultOfficers()
{
    // Slums Cabal Commander: Captain Frank "Bull" O'Malley
    RegisterOfficer(1001, "MMPD-013", "Captain Frank 'Bull' O'Malley", 13, CorruptionTier::CABAL_COMMANDER, 92.0f);
    CorruptOfficerProfile* p1 = GetOfficer(1001);
    if (p1) {
        p1->patronSyndicateId = 5; // Irish Mob
        p1->patronFamilyId = 1;    // Valetti Family
        p1->carriesDropGun = true;
        p1->dropGunModel = "Scrubbed Smith & Wesson .38 Special";
    }

    // Downtown Detective Lieutenant Vincent "The Ghost" Vane
    RegisterOfficer(1002, "MMPD-102", "Detective Lt. Vincent 'The Ghost' Vane", 1, CorruptionTier::CREATIVE_INVESTIGATOR, 76.0f);
    CorruptOfficerProfile* p2 = GetOfficer(1002);
    if (p2) {
        p2->patronFamilyId = 2;    // Marcone Family
        p2->evidenceTamperedCount = 14;
        p2->carriesDropGun = true;
    }

    // International District Sergeant Marcus Chen (Active Bagman)
    RegisterOfficer(1003, "MMPD-208", "Sergeant Marcus Chen", 8, CorruptionTier::ACTIVE_BAGMAN, 80.0f);
    CorruptOfficerProfile* p3 = GetOfficer(1003);
    if (p3) {
        p3->patronSyndicateId = 3; // Triads
        p3->totalBribesPocketed = 45000;
    }

    // Richland Federal Chief Inspector Donald Vance (Untouchable Kingpin)
    RegisterOfficer(1004, "MMPD-004", "Chief Inspector Donald Vance", 4, CorruptionTier::UNTOUCHABLE_KINGPIN, 45.0f);
    CorruptOfficerProfile* p4 = GetOfficer(1004);
    if (p4) {
        p4->patronFamilyId = 5; // Falcone Family
    }

    // Compromised Rookie: Patrolman Eddie Miller
    RegisterOfficer(1005, "MMPD-415", "Patrolman Eddie Miller", 13, CorruptionTier::COMPROMISED_ROOKIE, 35.0f);
    CorruptOfficerProfile* p5 = GetOfficer(1005);
    if (p5) {
        p5->totalBribesPocketed = 1200;
    }

    // Clean Idealist & Whistleblower: Detective Sarah Lin
    RegisterOfficer(1006, "MMPD-330", "Detective Sarah Lin", 13, CorruptionTier::CLEAN_IDEALIST, 0.0f);
    PromoteWhistleblower(1006);

    // Clean Idealist & Whistleblower: Officer Jack Reynolds
    RegisterOfficer(1007, "MMPD-512", "Officer Jack Reynolds", 8, CorruptionTier::CLEAN_IDEALIST, 0.0f);
    PromoteWhistleblower(1007);
}

void CorruptCopManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32 now = getMSTime();

    // Check expiring Look-Away protocols
    for (auto& pair : m_precincts)
    {
        auto& p = pair.second;
        if (p.isUnderLookAwayProtocol && now >= p.lookAwayExpireMs)
        {
            p.isUnderLookAwayProtocol = false;
            p.lookAwayTargetDesc.clear();
            COP_LOG("[CorruptCopManager] Code 10-7 Look-Away diversion expired for %s. Patrols resuming normal routes.",
                           p.precinctName.c_str());
        }
    }
}

// ============================================================================
// Pillar I: Precinct Ledgers & Corruption Dynamics
// ============================================================================

void CorruptCopManager::RegisterPrecinct(uint32 precinctId, const std::string& name, uint32 districtId,
                                         float initialScore, uint32 totalOfficers)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PrecinctCorruptionState p;
    p.precinctId = precinctId;
    p.precinctName = name;
    p.districtId = districtId;
    p.corruptionScore = std::clamp(initialScore, 0.0f, 100.0f);
    p.totalOfficers = totalOfficers;
    p.corruptOfficers = (uint32)(totalOfficers * (p.corruptionScore / 100.0f));
    p.whistleblowerCount = std::max((uint32)1, totalOfficers - p.corruptOfficers);
    p.weeklyGraftTotal = 0;
    p.activeProtectionContracts = 0;
    p.isUnderLookAwayProtocol = false;
    p.lookAwayExpireMs = 0;
    p.isUnderActiveIABSting = false;
    p.activeStingId = 0;
    p.isTargetedByCastle = (p.corruptionScore >= 70.0f);

    m_precincts[precinctId] = p;
}

PrecinctCorruptionState* CorruptCopManager::GetPrecinct(uint32 precinctId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_precincts.find(precinctId);
    return (it != m_precincts.end()) ? &it->second : nullptr;
}

void CorruptCopManager::UpdatePrecinctCorruption(uint32 precinctId, float deltaScore)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_precincts.find(precinctId);
    if (it != m_precincts.end())
    {
        it->second.corruptionScore = std::clamp(it->second.corruptionScore + deltaScore, 0.0f, 100.0f);
        it->second.corruptOfficers = (uint32)(it->second.totalOfficers * (it->second.corruptionScore / 100.0f));
        it->second.whistleblowerCount = std::max((uint32)0, it->second.totalOfficers - it->second.corruptOfficers);
        it->second.isTargetedByCastle = (it->second.corruptionScore >= 70.0f);
    }
}

void CorruptCopManager::RegisterOfficer(uint32 officerId, const std::string& badge, const std::string& name,
                                        uint32 precinctId, CorruptionTier tier, float corruptionScore)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    CorruptOfficerProfile o;
    o.officerId = officerId;
    o.badgeNumber = badge;
    o.name = name;
    o.precinctId = precinctId;
    o.tier = tier;
    o.corruptionScore = std::clamp(corruptionScore, 0.0f, 100.0f);
    o.suspicionMeter = (tier == CorruptionTier::CLEAN_IDEALIST) ? 0.0f : 15.0f;
    o.patronSyndicateId = 0;
    o.patronFamilyId = 0;
    o.totalBribesPocketed = 0;
    o.evidenceTamperedCount = 0;
    o.carriesDropGun = (tier >= CorruptionTier::CREATIVE_INVESTIGATOR);
    o.dropGunModel = "Scrubbed Revolver .38";
    o.whistleblowerState = WhistleblowerStatus::NONE;
    o.isMarkedByCastle = (corruptionScore >= 75.0f);
    o.isAgentPossessed = false;
    o.isAlive = true;
    o.isArrestedByIAB = false;

    m_officers[officerId] = o;
}

CorruptOfficerProfile* CorruptCopManager::GetOfficer(uint32 officerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_officers.find(officerId);
    return (it != m_officers.end()) ? &it->second : nullptr;
}

std::vector<CorruptOfficerProfile*> CorruptCopManager::GetOfficersInPrecinct(uint32 precinctId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<CorruptOfficerProfile*> result;
    for (auto& pair : m_officers)
    {
        if (pair.second.precinctId == precinctId)
        {
            result.push_back(&pair.second);
        }
    }
    return result;
}

size_t CorruptCopManager::GetTotalOfficersCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_officers.size();
}

// ============================================================================
// Pillar II: Bagman Ledger & Bribe Drops
// ============================================================================

uint32 CorruptCopManager::ScheduleBribeDrop(uint32 precinctId, uint32 patronId, bool isMafia,
                                            const std::string& locName, const LocationVector& loc,
                                            uint32 cashAmount, uint32 bagmanOfficerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    BribeDropSchedule drop;
    drop.dropId = m_nextDropId++;
    drop.precinctId = precinctId;
    drop.patronId = patronId;
    drop.isMafia = isMafia;
    drop.dropLocationName = locName;
    drop.location = loc;
    drop.cashAmount = cashAmount;
    drop.bagmanOfficerId = bagmanOfficerId;
    drop.state = BribeDropState::SCHEDULED;

    // Waterfall kickback calculation
    drop.captainShare = cashAmount * 40 / 100;
    drop.detectiveShare = cashAmount * 30 / 100;
    drop.dispatchShare = cashAmount * 20 / 100;
    drop.patrolPoolShare = cashAmount - (drop.captainShare + drop.detectiveShare + drop.dispatchShare);

    m_bribeDrops[drop.dropId] = drop;
    COP_LOG("[CorruptCopManager] Scheduled Bribe Drop #%u at '%s': $%u for Precinct %u (Bagman Officer %u)",
                   drop.dropId, locName.c_str(), cashAmount, precinctId, bagmanOfficerId);
    return drop.dropId;
}

BribeDropSchedule* CorruptCopManager::GetBribeDrop(uint32 dropId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_bribeDrops.find(dropId);
    return (it != m_bribeDrops.end()) ? &it->second : nullptr;
}

size_t CorruptCopManager::GetBribeDropCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_bribeDrops.size();
}

bool CorruptCopManager::ExecuteBribeDelivery(uint32 dropId, uint32& outCaptainShare, uint32& outDetectiveShare,
                                             uint32& outDispatchShare, uint32& outPatrolShare)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_bribeDrops.find(dropId);
    if (it == m_bribeDrops.end() || it->second.state != BribeDropState::SCHEDULED)
    {
        return false;
    }

    auto& drop = it->second;
    drop.state = BribeDropState::DELIVERED;
    outCaptainShare = drop.captainShare;
    outDetectiveShare = drop.detectiveShare;
    outDispatchShare = drop.dispatchShare;
    outPatrolShare = drop.patrolPoolShare;

    m_totalBribesLaundered += drop.cashAmount;

    auto pIt = m_precincts.find(drop.precinctId);
    if (pIt != m_precincts.end())
    {
        pIt->second.weeklyGraftTotal += drop.cashAmount;
        pIt->second.corruptionScore = std::min(100.0f, pIt->second.corruptionScore + 1.2f);
    }

    auto oIt = m_officers.find(drop.bagmanOfficerId);
    if (oIt != m_officers.end())
    {
        // Bagman keeps a 5% direct courier fee from the graft pool
        oIt->second.totalBribesPocketed += (drop.cashAmount * 5 / 100);
        oIt->second.suspicionMeter = std::min(100.0f, oIt->second.suspicionMeter + 5.0f);
    }

    COP_LOG("[CorruptCopManager] Bribe Drop #%u DELIVERED! Split: Captain: $%u, Detectives: $%u, Dispatch: $%u, Patrol: $%u",
                   dropId, outCaptainShare, outDetectiveShare, outDispatchShare, outPatrolShare);
    return true;
}

// ============================================================================
// Pillar III: Evidence Locker Skimming & Drop Guns
// ============================================================================

uint32 CorruptCopManager::RecordEvidenceSeizure(uint32 precinctId, uint32 officerId, ContrabandCategory cat,
                                                const std::string& desc, uint32 seizedQty, float estimatedValue)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    float skimRate = 0.35f;
    auto pIt = m_precincts.find(precinctId);
    if (pIt != m_precincts.end())
    {
        skimRate = std::clamp(pIt->second.corruptionScore / 160.0f, 0.15f, 0.60f);
    }

    uint32 skimmedQty = std::max((uint32)1, (uint32)std::round(seizedQty * skimRate));
    uint32 loggedQty = (seizedQty > skimmedQty) ? (seizedQty - skimmedQty) : 0;

    EvidenceSkimRecord rec;
    rec.recordId = m_nextRecordId++;
    rec.precinctId = precinctId;
    rec.officerId = officerId;
    rec.category = cat;
    rec.description = desc;
    rec.totalSeized = seizedQty;
    rec.loggedQuantity = loggedQty;
    rec.skimmedQuantity = skimmedQty;
    rec.estimatedValue = estimatedValue;
    rec.isFenced = false;
    rec.fencedToSyndicateId = 0;
    rec.fencedProfit = 0;

    m_evidenceRecords[rec.recordId] = rec;

    auto oIt = m_officers.find(officerId);
    if (oIt != m_officers.end())
    {
        oIt->second.evidenceTamperedCount++;
        oIt->second.suspicionMeter = std::min(100.0f, oIt->second.suspicionMeter + 4.0f);
    }

    COP_LOG("[CorruptCopManager] Evidence Seizure #%u in Precinct %u: '%s' (Seized: %u, Logged: %u, SKIMMED: %u)",
                   rec.recordId, precinctId, desc.c_str(), seizedQty, loggedQty, skimmedQty);
    return rec.recordId;
}

EvidenceSkimRecord* CorruptCopManager::GetEvidenceRecord(uint32 recordId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_evidenceRecords.find(recordId);
    return (it != m_evidenceRecords.end()) ? &it->second : nullptr;
}

bool CorruptCopManager::FenceContrabandToUnderworld(uint32 recordId, uint32 buyerSyndicateId, uint32& outCashGained)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_evidenceRecords.find(recordId);
    if (it == m_evidenceRecords.end() || it->second.isFenced || it->second.skimmedQuantity == 0)
    {
        outCashGained = 0;
        return false;
    }

    auto& rec = it->second;
    rec.isFenced = true;
    rec.fencedToSyndicateId = buyerSyndicateId;

    float skimRatio = rec.skimmedQuantity / (float)std::max((uint32)1, rec.totalSeized);
    outCashGained = (uint32)(rec.estimatedValue * skimRatio * 0.70f);
    rec.fencedProfit = outCashGained;

    auto oIt = m_officers.find(rec.officerId);
    if (oIt != m_officers.end())
    {
        oIt->second.totalBribesPocketed += outCashGained;
    }

    COP_LOG("[CorruptCopManager] Evidence #%u FENCED to Syndicate %u for $%u illicit cash!",
                   recordId, buyerSyndicateId, outCashGained);
    return true;
}

bool CorruptCopManager::EquipThrowawayDropGun(uint32 officerId, const std::string& serialScrubbedGun)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_officers.find(officerId);
    if (it == m_officers.end()) return false;

    it->second.carriesDropGun = true;
    it->second.dropGunModel = serialScrubbedGun;
    COP_LOG("[CorruptCopManager] Officer %u (%s) equipped with throwaway drop gun '%s'",
                   officerId, it->second.name.c_str(), serialScrubbedGun.c_str());
    return true;
}

bool CorruptCopManager::PlantDropGunOnVictim(uint32 officerId, uint32 victimGoId, const LocationVector& scenePos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_officers.find(officerId);
    if (it == m_officers.end() || !it->second.carriesDropGun) return false;

    it->second.carriesDropGun = false;
    it->second.evidenceTamperedCount++;
    it->second.suspicionMeter = std::min(100.0f, it->second.suspicionMeter + 10.0f);

    COP_LOG("[CorruptCopManager] Officer %u PLANTED drop gun '%s' on Victim GOID %u at (%.1f, %.1f, %.1f)",
                   officerId, it->second.dropGunModel.c_str(), victimGoId, (float)scenePos.x, (float)scenePos.y, (float)scenePos.z);
    return true;
}

// ============================================================================
// Pillar IV: Look-Away Protocols & Staged Raids
// ============================================================================

bool CorruptCopManager::IssueCode10_7LookAway(uint32 precinctId, uint32 requestedByFamilyId,
                                              const std::string& targetCrimeScene, uint32 diversionDurationSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_precincts.find(precinctId);
    if (it == m_precincts.end()) return false;

    it->second.isUnderLookAwayProtocol = true;
    it->second.lookAwayExpireMs = getMSTime() + (diversionDurationSec * 1000);
    it->second.lookAwayTargetDesc = targetCrimeScene;

    COP_LOG("[CorruptCopManager] CODE 10-7 LOOK-AWAY ISSUED for Precinct %u! Requested by Family %u. Diversion active at '%s' for %us",
                   precinctId, requestedByFamilyId, targetCrimeScene.c_str(), diversionDurationSec);
    return true;
}

bool CorruptCopManager::IsAreaUnderLookAwayProtocol(uint32 precinctId, const LocationVector& loc) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_precincts.find(precinctId);
    if (it == m_precincts.end()) return false;
    return it->second.isUnderLookAwayProtocol;
}

uint32 CorruptCopManager::StageRivalRacketRaid(uint32 payingFamilyId, uint32 targetSyndicateId,
                                              uint32 targetRacketId, const LocationVector& racketPos, uint32 fallGuyGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    StagedRaidRecord raid;
    raid.raidId = m_nextRaidId++;
    raid.payingFamilyId = payingFamilyId;
    raid.targetSyndicateId = targetSyndicateId;
    raid.targetRacketId = targetRacketId;
    raid.objective = StagedRaidObjective::HARASS_RIVAL_RACKET;
    raid.raidLocation = racketPos;
    raid.squadCallsign = "Sierra-2";
    raid.isExecuted = true;
    raid.fallGuyGoId = fallGuyGoId;
    raid.fallGuyBailedOut = false;

    m_stagedRaids[raid.raidId] = raid;
    m_totalStagedRaidsExecuted++;

    COP_LOG("[CorruptCopManager] STAGED RAID #%u executed on Racket %u (Syndicate %u) paid for by Family %u! Fall Guy GOID %u detained.",
                   raid.raidId, targetRacketId, targetSyndicateId, payingFamilyId, fallGuyGoId);
    return raid.raidId;
}

StagedRaidRecord* CorruptCopManager::GetStagedRaid(uint32 raidId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_stagedRaids.find(raidId);
    return (it != m_stagedRaids.end()) ? &it->second : nullptr;
}

bool CorruptCopManager::ProcessCatchAndRelease(uint32 arrestedMobsterGoId, uint32 payingFamilyId, uint32 bailAmountInfo)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_stagedRaids)
    {
        if (pair.second.fallGuyGoId == arrestedMobsterGoId && !pair.second.fallGuyBailedOut)
        {
            pair.second.fallGuyBailedOut = true;
            COP_LOG("[CorruptCopManager] CATCH-AND-RELEASE: Mobster GOID %u released on expedited $%u bail under Family %u protection.",
                           arrestedMobsterGoId, bailAmountInfo, payingFamilyId);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Pillar V: Whistleblower Dynamics & Internal Affairs Stings
// ============================================================================

bool CorruptCopManager::PromoteWhistleblower(uint32 officerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_officers.find(officerId);
    if (it == m_officers.end()) return false;

    it->second.tier = CorruptionTier::CLEAN_IDEALIST;
    it->second.corruptionScore = 0.0f;
    it->second.whistleblowerState = WhistleblowerStatus::COLLECTING_EVIDENCE;

    WhistleblowerRecord wb;
    wb.officerId = officerId;
    wb.evidencePagesCollected = 1;
    wb.suspicionAgainstOfficer = 5.0f;
    wb.status = WhistleblowerStatus::COLLECTING_EVIDENCE;
    wb.soloHotCallAssigned = false;
    wb.survivedSoloCall = false;
    wb.iabProtectionActive = false;

    m_whistleblowers[officerId] = wb;
    COP_LOG("[CorruptCopManager] Officer %u (%s) initialized as Clean Whistleblower.",
                   officerId, it->second.name.c_str());
    return true;
}

WhistleblowerRecord* CorruptCopManager::GetWhistleblower(uint32 officerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_whistleblowers.find(officerId);
    return (it != m_whistleblowers.end()) ? &it->second : nullptr;
}

bool CorruptCopManager::GatherWhistleblowerEvidence(uint32 officerId, const std::string& evidenceType, float suspicionGain)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_whistleblowers.find(officerId);
    if (it == m_whistleblowers.end() || it->second.status == WhistleblowerStatus::SILENCED_OR_MISSING)
    {
        return false;
    }

    it->second.evidencePagesCollected += 5;
    it->second.suspicionAgainstOfficer = std::min(100.0f, it->second.suspicionAgainstOfficer + suspicionGain);

    if (it->second.suspicionAgainstOfficer >= 50.0f && it->second.status == WhistleblowerStatus::COLLECTING_EVIDENCE)
    {
        it->second.status = WhistleblowerStatus::SUSPECTED_BY_CABAL;
        auto oIt = m_officers.find(officerId);
        if (oIt != m_officers.end()) oIt->second.whistleblowerState = WhistleblowerStatus::SUSPECTED_BY_CABAL;
        COP_LOG("[CorruptCopManager] Whistleblower Officer %u is now SUSPECTED BY CABAL! Evidence binder: %u pages",
                       officerId, it->second.evidencePagesCollected);
    }
    return true;
}

bool CorruptCopManager::OrderRetaliationSoloCall(uint32 whistleblowerOfficerId, const LocationVector& dangerousLoc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_whistleblowers.find(whistleblowerOfficerId);
    if (it == m_whistleblowers.end()) return false;

    it->second.status = WhistleblowerStatus::UNDER_RETALIATION;
    it->second.soloHotCallAssigned = true;
    it->second.soloCallLocation = dangerousLoc;

    auto oIt = m_officers.find(whistleblowerOfficerId);
    if (oIt != m_officers.end()) oIt->second.whistleblowerState = WhistleblowerStatus::UNDER_RETALIATION;

    COP_LOG("[CorruptCopManager] CABAL RETALIATION: Whistleblower Officer %u dispatched solo to violent hot call at (%.1f, %.1f, %.1f) WITHOUT BACKUP!",
                   whistleblowerOfficerId, (float)dangerousLoc.x, (float)dangerousLoc.y, (float)dangerousLoc.z);
    return true;
}

bool CorruptCopManager::ResolveSoloCallOutcome(uint32 whistleblowerOfficerId, bool castleOrPlayerIntervened)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_whistleblowers.find(whistleblowerOfficerId);
    if (it == m_whistleblowers.end() || !it->second.soloHotCallAssigned) return false;

    if (castleOrPlayerIntervened)
    {
        it->second.survivedSoloCall = true;
        it->second.iabProtectionActive = true;
        it->second.status = WhistleblowerStatus::IAB_PROTECTED_WITNESS;

        auto oIt = m_officers.find(whistleblowerOfficerId);
        if (oIt != m_officers.end()) oIt->second.whistleblowerState = WhistleblowerStatus::IAB_PROTECTED_WITNESS;

        m_totalWhistleblowersRescued++;
        COP_LOG("[CorruptCopManager] Whistleblower Officer %u RESCUED by vigilante/player intervention! Secured in IAB Safehouse.",
                       whistleblowerOfficerId);
    }
    else
    {
        it->second.survivedSoloCall = false;
        it->second.status = WhistleblowerStatus::SILENCED_OR_MISSING;

        auto oIt = m_officers.find(whistleblowerOfficerId);
        if (oIt != m_officers.end())
        {
            oIt->second.whistleblowerState = WhistleblowerStatus::SILENCED_OR_MISSING;
            oIt->second.isAlive = false;
        }

        m_totalWhistleblowersSilenced++;
        COP_LOG("[CorruptCopManager] Whistleblower Officer %u was SILENCED in the line of duty during staged ambush.",
                       whistleblowerOfficerId);
    }
    return true;
}

uint32 CorruptCopManager::LaunchIABMarkedCurrencySting(uint32 targetOfficerId, uint32 stingCashAmount)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto oIt = m_officers.find(targetOfficerId);
    if (oIt == m_officers.end()) return 0;

    IABStingOperation sting;
    sting.stingId = m_nextStingId++;
    sting.precinctId = oIt->second.precinctId;
    sting.targetOfficerId = targetOfficerId;
    sting.stage = IABStingStage::SURVEILLANCE_ACTIVE;
    sting.markedBribeCash = stingCashAmount;
    sting.serialNumbersLogged = true;
    sting.wiretapAudioSecured = false;
    sting.indictmentSigned = false;
    sting.raidSuccessful = false;
    sting.leadInvestigator = "Special Agent Marcus Kelly";

    m_iabStings[sting.stingId] = sting;

    auto pIt = m_precincts.find(oIt->second.precinctId);
    if (pIt != m_precincts.end())
    {
        pIt->second.isUnderActiveIABSting = true;
        pIt->second.activeStingId = sting.stingId;
    }

    COP_LOG("[CorruptCopManager] IAB STING OPERATION #%u LAUNCHED against Officer %u (%s) in Precinct %u with $%u marked cash!",
                   sting.stingId, targetOfficerId, oIt->second.name.c_str(), oIt->second.precinctId, stingCashAmount);
    return sting.stingId;
}

IABStingOperation* CorruptCopManager::GetIABSting(uint32 stingId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_iabStings.find(stingId);
    return (it != m_iabStings.end()) ? &it->second : nullptr;
}

bool CorruptCopManager::AdvanceIABStingStage(uint32 stingId, bool& outConvictionSecured)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    outConvictionSecured = false;
    auto it = m_iabStings.find(stingId);
    if (it == m_iabStings.end()) return false;

    auto& sting = it->second;
    switch (sting.stage)
    {
        case IABStingStage::SURVEILLANCE_ACTIVE:
            sting.stage = IABStingStage::MARKED_CURRENCY_DELIVERED;
            COP_LOG("[CorruptCopManager] IAB Sting #%u -> Marked serialized currency delivered to suspect.", stingId);
            break;
        case IABStingStage::MARKED_CURRENCY_DELIVERED:
            sting.stage = IABStingStage::AUDIO_TAPE_INTERCEPTED;
            sting.wiretapAudioSecured = true;
            COP_LOG("[CorruptCopManager] IAB Sting #%u -> Audio wiretap confirmed officer accepting bribe!", stingId);
            break;
        case IABStingStage::AUDIO_TAPE_INTERCEPTED:
            sting.stage = IABStingStage::WARRANT_ISSUED;
            sting.indictmentSigned = true;
            COP_LOG("[CorruptCopManager] IAB Sting #%u -> Grand Jury indictment and arrest warrant signed!", stingId);
            break;
        case IABStingStage::WARRANT_ISSUED:
            sting.stage = IABStingStage::PRE_DAWN_PRECINCT_RAID;
            sting.raidSuccessful = true;
            {
                auto oIt = m_officers.find(sting.targetOfficerId);
                if (oIt != m_officers.end())
                {
                    oIt->second.isArrestedByIAB = true;
                }
            }
            COP_LOG("[CorruptCopManager] IAB Sting #%u -> Pre-dawn raid executed! Suspect taken into federal custody.", stingId);
            break;
        case IABStingStage::PRE_DAWN_PRECINCT_RAID:
            sting.stage = IABStingStage::REFORM_COMPLETED;
            outConvictionSecured = true;
            m_totalIABConvictions++;
            {
                auto pIt = m_precincts.find(sting.precinctId);
                if (pIt != m_precincts.end())
                {
                    pIt->second.isUnderActiveIABSting = false;
                    pIt->second.corruptionScore = std::max(10.0f, pIt->second.corruptionScore - 35.0f);
                    pIt->second.corruptOfficers = (uint32)(pIt->second.totalOfficers * (pIt->second.corruptionScore / 100.0f));
                }
            }
            COP_LOG("[CorruptCopManager] IAB Sting #%u -> REFORM COMPLETED! Clean interim commander installed. Precinct corruption plummeted.", stingId);
            break;
        default:
            return false;
    }
    return true;
}

// ============================================================================
// Pillar VI: Frank Castle Retribution & Agent Host Purges
// ============================================================================

bool CorruptCopManager::LogCastleTaintedShieldTarget(uint32 officerId, const std::string& reason)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto oIt = m_officers.find(officerId);
    if (oIt == m_officers.end()) return false;

    CastleTaintedShieldEntry entry;
    entry.officerId = officerId;
    entry.badgeNumber = oIt->second.badgeNumber;
    entry.name = oIt->second.name;
    entry.crimeCommitted = reason;
    entry.isInterrogated = false;
    entry.isConfessionTaped = false;
    entry.isExecuted = false;
    entry.tapeDeliveredToIAB = false;

    m_castleTargets[officerId] = entry;
    oIt->second.isMarkedByCastle = true;

    COP_LOG("[CorruptCopManager] FRANK CASTLE BLACK LEDGER: Marked dirty cop %s (Badge %s). Reason: %s",
                   entry.name.c_str(), entry.badgeNumber.c_str(), reason.c_str());
    return true;
}

CastleTaintedShieldEntry* CorruptCopManager::GetCastleTarget(uint32 officerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_castleTargets.find(officerId);
    return (it != m_castleTargets.end()) ? &it->second : nullptr;
}

bool CorruptCopManager::ExecuteCastleAmbushOnBribeDrop(uint32 dropId, bool executeOfficer)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_bribeDrops.find(dropId);
    if (it == m_bribeDrops.end() || it->second.state != BribeDropState::SCHEDULED)
    {
        return false;
    }

    it->second.state = BribeDropState::AMBUSHED_BY_CASTLE;
    uint32 bagmanId = it->second.bagmanOfficerId;

    auto oIt = m_officers.find(bagmanId);
    if (oIt != m_officers.end())
    {
        CastleTaintedShieldEntry entry;
        entry.officerId = bagmanId;
        entry.badgeNumber = oIt->second.badgeNumber;
        entry.name = oIt->second.name;
        entry.crimeCommitted = "Caught red-handed at syndicate pizzo cash drop";
        entry.isInterrogated = true;
        entry.isConfessionTaped = true;
        entry.isExecuted = executeOfficer;
        entry.tapeDeliveredToIAB = false;
        m_castleTargets[bagmanId] = entry;

        if (executeOfficer)
        {
            oIt->second.isAlive = false;
            m_totalCastleDirtyCopKills++;
            COP_LOG("[CorruptCopManager] FRANK CASTLE EXECUTED dirty bagman %s at drop site! Skull insignia spray-painted on squad car.",
                           oIt->second.name.c_str());
        }
        else
        {
            COP_LOG("[CorruptCopManager] Frank Castle interrogated dirty bagman %s and extracted audio confession tape.",
                           oIt->second.name.c_str());
        }
    }
    return true;
}

bool CorruptCopManager::DeliverCastleConfessionTapeToIAB(uint32 officerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_castleTargets.find(officerId);
    if (it == m_castleTargets.end() || !it->second.isConfessionTaped) return false;

    it->second.tapeDeliveredToIAB = true;
    auto oIt = m_officers.find(officerId);
    if (oIt != m_officers.end())
    {
        oIt->second.isArrestedByIAB = true;
    }

    COP_LOG("[CorruptCopManager] Frank Castle delivered taped confession of Officer %u to IAB Headquarters!", officerId);
    return true;
}

float CorruptCopManager::EvaluateAgentPossessionSusceptibility(uint32 officerId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_officers.find(officerId);
    if (it == m_officers.end()) return 0.10f;

    const auto& o = it->second;
    if (o.tier == CorruptionTier::CLEAN_IDEALIST)
    {
        return 0.15f; // Pure human will offers high mental resistance
    }

    // Guilt, paranoia, and moral compromise increase susceptibility
    float susceptibility = 0.25f + (o.corruptionScore / 100.0f) * 0.40f + (o.suspicionMeter / 100.0f) * 0.25f;
    return std::clamp(susceptibility, 0.10f, 0.95f);
}

bool CorruptCopManager::TriggerAgentPossessionOfCorruptCop(uint32 officerId, uint32 agentType)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_officers.find(officerId);
    if (it == m_officers.end() || !it->second.isAlive || it->second.isAgentPossessed) return false;

    it->second.isAgentPossessed = true;
    // Demorphing post-possession causes systemic shock fatality in human host
    it->second.isAlive = false;

    COP_LOG("[CorruptCopManager] AGENT OVERWRITE: Machine Agent possessed corrupt Officer %u (%s) as an unclean host! Body burned out.",
                   officerId, it->second.name.c_str());
    return true;
}

std::string CorruptCopManager::GenerateCorruptionSummaryReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string report = "=== MMPD CORRUPTION & INTERNAL AFFAIRS SUMMARY REPORT ===\n";
    for (const auto& pair : m_precincts)
    {
        report += pair.second.precinctName + " | Corruption: " + std::to_string(pair.second.corruptionScore) + "%"
               + " | Corrupt Cops: " + std::to_string(pair.second.corruptOfficers) + "/" + std::to_string(pair.second.totalOfficers)
               + " | Whistleblowers: " + std::to_string(pair.second.whistleblowerCount) + "\n";
    }
    return report;
}

// ============================================================================
// HEADLESS TEST SUITE: CORRUPT COPS & INTERNAL AFFAIRS (SUITE 20)
// ============================================================================
void RunCorruptCopTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING CORRUPT COPS & INTERNAL AFFAIRS TEST SUITE (SUITE 20)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // 1. System Initialization & Precinct Gradients
    sCorruptCopMgr.Initialize();
    TEST_ASSERT(sCorruptCopMgr.GetAllPrecincts().size() == 4, "Initialized 4 standard MMPD precincts");
    TEST_ASSERT(sCorruptCopMgr.GetTotalOfficersCount() >= 7, "Initialized 7 archetype officers");
    TEST_ASSERT(sCorruptCopMgr.GetTotalBribesLaundered() == 0, "Initial laundered bribes is zero");

    PrecinctCorruptionState* p13 = sCorruptCopMgr.GetPrecinct(13);
    TEST_ASSERT(p13 != nullptr, "Precinct 13 (Slums) retrieved");
    TEST_ASSERT(p13 && p13->corruptionScore == 85.0f, "Slums Precinct 13 has 85% high corruption");
    TEST_ASSERT(p13 && p13->corruptOfficers == 40, "Precinct 13 has 40 corrupt officers out of 48");
    TEST_ASSERT(p13 && p13->isTargetedByCastle == true, "Slums precinct is flagged on Castle's radar");

    PrecinctCorruptionState* p4 = sCorruptCopMgr.GetPrecinct(4);
    TEST_ASSERT(p4 != nullptr && p4->corruptionScore == 32.0f, "Precinct 4 (Richland) has lower 32% corruption");

    // Dynamic corruption drift
    sCorruptCopMgr.UpdatePrecinctCorruption(13, 5.0f);
    TEST_ASSERT(p13->corruptionScore == 90.0f, "Precinct corruption adjusted upward to 90%");

    // 2. Officer Profiling & Tiers
    CorruptOfficerProfile* oOMalley = sCorruptCopMgr.GetOfficer(1001);
    TEST_ASSERT(oOMalley != nullptr, "Captain O'Malley retrieved");
    TEST_ASSERT(oOMalley && oOMalley->tier == CorruptionTier::CABAL_COMMANDER, "O'Malley is Cabal Commander tier");
    TEST_ASSERT(oOMalley && oOMalley->carriesDropGun == true, "O'Malley carries throwaway drop gun");

    CorruptOfficerProfile* oLin = sCorruptCopMgr.GetOfficer(1006);
    TEST_ASSERT(oLin != nullptr, "Detective Sarah Lin retrieved");
    TEST_ASSERT(oLin && oLin->tier == CorruptionTier::CLEAN_IDEALIST, "Detective Lin is Clean Idealist");
    TEST_ASSERT(oLin && oLin->corruptionScore == 0.0f, "Lin has zero corruption score");

    // 3. Bagman Ledger & Scheduled Bribe Waterfall Delivery
    uint32 dropId = sCorruptCopMgr.ScheduleBribeDrop(13, 1, true, "Valetti Meatpacking Alley",
                                                     LocationVector(1200.0, 10.0, 5500.0), 50000, 1003);
    TEST_ASSERT(dropId == 1, "First scheduled bribe drop assigned ID 1");
    BribeDropSchedule* drop = sCorruptCopMgr.GetBribeDrop(dropId);
    TEST_ASSERT(drop != nullptr, "Bribe drop schedule retrieved");
    TEST_ASSERT(drop->state == BribeDropState::SCHEDULED, "Bribe drop begins in SCHEDULED state");
    TEST_ASSERT(drop->captainShare == 20000, "Captain receives 40% waterfall cut ($20,000)");
    TEST_ASSERT(drop->detectiveShare == 15000, "Detectives receive 30% waterfall cut ($15,000)");
    TEST_ASSERT(drop->dispatchShare == 10000, "Dispatch receives 20% waterfall cut ($10,000)");
    TEST_ASSERT(drop->patrolPoolShare == 5000, "Patrol pool receives remainder ($5,000)");

    // Execute delivery
    uint32 capShare = 0, detShare = 0, dispShare = 0, patShare = 0;
    bool delivOk = sCorruptCopMgr.ExecuteBribeDelivery(dropId, capShare, detShare, dispShare, patShare);
    TEST_ASSERT(delivOk, "Bribe delivery executed successfully");
    TEST_ASSERT(drop->state == BribeDropState::DELIVERED, "Bribe state updated to DELIVERED");
    TEST_ASSERT(sCorruptCopMgr.GetTotalBribesLaundered() == 50000, "Total laundered bribes updated to $50,000");
    TEST_ASSERT(p13->weeklyGraftTotal == 50000, "Precinct 13 weekly graft ledger updated");
    CorruptOfficerProfile* oChen = sCorruptCopMgr.GetOfficer(1003);
    TEST_ASSERT(oChen && oChen->totalBribesPocketed > 45000, "Bagman pocketed 5% courier fee ($2,500)");

    // 4. Evidence Locker Skimming & Fencing Contraband
    uint32 recId = sCorruptCopMgr.RecordEvidenceSeizure(13, 1002, ContrabandCategory::SEIZED_FIREARMS,
                                                        "20x Seized SMGs", 20, 30000.0f);
    TEST_ASSERT(recId == 1, "First evidence skim record assigned ID 1");
    EvidenceSkimRecord* evRec = sCorruptCopMgr.GetEvidenceRecord(recId);
    TEST_ASSERT(evRec != nullptr, "Evidence record retrieved");
    TEST_ASSERT(evRec->skimmedQuantity > 0, "Corrupt officers skimmed portion of seized firearms");
    TEST_ASSERT(evRec->loggedQuantity + evRec->skimmedQuantity == 20, "Total seized balances logged plus skimmed");

    // Fence contraband back to Syndicate
    uint32 fenceCash = 0;
    bool fenceOk = sCorruptCopMgr.FenceContrabandToUnderworld(recId, 3, fenceCash);
    TEST_ASSERT(fenceOk, "Contraband successfully fenced back to Triad Syndicate");
    TEST_ASSERT(evRec->isFenced == true, "Evidence record marked as fenced");
    TEST_ASSERT(fenceCash > 0, "Fencing yields cash payout to corrupt detective");

    // Drop gun mechanics
    sCorruptCopMgr.EquipThrowawayDropGun(1005, "Scrubbed Beretta 92FS");
    CorruptOfficerProfile* oMiller = sCorruptCopMgr.GetOfficer(1005);
    TEST_ASSERT(oMiller && oMiller->carriesDropGun == true, "Rookie officer equipped with throwaway gun");
    bool plantOk = sCorruptCopMgr.PlantDropGunOnVictim(1005, 999912, LocationVector(100.0, 0.0, 200.0));
    TEST_ASSERT(plantOk, "Drop gun successfully planted on victim at scene");
    TEST_ASSERT(oMiller->carriesDropGun == false, "Officer no longer carries throwaway gun");
    TEST_ASSERT(oMiller->evidenceTamperedCount == 1, "Tampered evidence count incremented");

    // 5. Look-Away Protocols & Staged Raids
    bool lookAwayOk = sCorruptCopMgr.IssueCode10_7LookAway(13, 1, "Slums Vault Heist", 480);
    TEST_ASSERT(lookAwayOk, "Code 10-7 Look-Away protocol issued for Precinct 13");
    TEST_ASSERT(sCorruptCopMgr.IsAreaUnderLookAwayProtocol(13, LocationVector(0.0, 0.0, 0.0)) == true,
                "Area confirmed under active look-away suppression");

    // Staged rival racket raid & catch-and-release
    uint32 raidId = sCorruptCopMgr.StageRivalRacketRaid(1, 2, 8, LocationVector(500.0, 0.0, 900.0), 888801);
    TEST_ASSERT(raidId == 1, "Staged rival racket raid assigned ID 1");
    StagedRaidRecord* raidRec = sCorruptCopMgr.GetStagedRaid(raidId);
    TEST_ASSERT(raidRec != nullptr && raidRec->isExecuted == true, "Staged raid executed on rival racket");
    TEST_ASSERT(raidRec->fallGuyGoId == 888801, "Fall guy scapegoat identified");

    bool bailOk = sCorruptCopMgr.ProcessCatchAndRelease(888801, 1, 5000);
    TEST_ASSERT(bailOk, "Catch-and-release bail processed for protected mobster");
    TEST_ASSERT(raidRec->fallGuyBailedOut == true, "Mobster marked as bailed out");

    // 6. Whistleblower Dynamics & Retaliation
    WhistleblowerRecord* wbLin = sCorruptCopMgr.GetWhistleblower(1006);
    TEST_ASSERT(wbLin != nullptr, "Whistleblower record for Detective Lin retrieved");
    bool evGather = sCorruptCopMgr.GatherWhistleblowerEvidence(1006, "Photocopied Graft Ledger", 55.0f);
    TEST_ASSERT(evGather, "Whistleblower evidence gathered");
    TEST_ASSERT(wbLin->status == WhistleblowerStatus::SUSPECTED_BY_CABAL, "Lin status escalated to SUSPECTED_BY_CABAL");

    // Cabal assigns retaliation solo hot call
    bool soloCallOk = sCorruptCopMgr.OrderRetaliationSoloCall(1006, LocationVector(3000.0, 0.0, 8000.0));
    TEST_ASSERT(soloCallOk, "Retaliation solo call assigned without backup");
    TEST_ASSERT(wbLin->status == WhistleblowerStatus::UNDER_RETALIATION, "Lin status is now UNDER_RETALIATION");

    // Intervene & rescue whistleblower
    bool rescueOk = sCorruptCopMgr.ResolveSoloCallOutcome(1006, true);
    TEST_ASSERT(rescueOk, "Whistleblower rescued by external vigilante intervention");
    TEST_ASSERT(wbLin->status == WhistleblowerStatus::IAB_PROTECTED_WITNESS, "Lin secured as IAB Protected Witness");

    // 7. Internal Affairs Bureau (IAB) Marked Currency Sting
    uint32 stingId = sCorruptCopMgr.LaunchIABMarkedCurrencySting(1001, 50000);
    TEST_ASSERT(stingId == 1, "IAB sting launched against Captain O'Malley");
    IABStingOperation* sting = sCorruptCopMgr.GetIABSting(stingId);
    TEST_ASSERT(sting != nullptr && sting->stage == IABStingStage::SURVEILLANCE_ACTIVE, "Sting in SURVEILLANCE_ACTIVE stage");

    bool convOk = false;
    sCorruptCopMgr.AdvanceIABStingStage(stingId, convOk); // MARKED_CURRENCY
    TEST_ASSERT(sting->stage == IABStingStage::MARKED_CURRENCY_DELIVERED, "Marked currency delivered");

    sCorruptCopMgr.AdvanceIABStingStage(stingId, convOk); // AUDIO_TAPE
    TEST_ASSERT(sting->wiretapAudioSecured == true, "Wiretap audio secured");

    sCorruptCopMgr.AdvanceIABStingStage(stingId, convOk); // WARRANT
    TEST_ASSERT(sting->indictmentSigned == true, "Arrest warrant and indictment signed");

    sCorruptCopMgr.AdvanceIABStingStage(stingId, convOk); // PRE_DAWN_RAID
    TEST_ASSERT(sting->raidSuccessful == true, "Pre-dawn precinct raid succeeded");
    TEST_ASSERT(oOMalley->isArrestedByIAB == true, "Captain O'Malley arrested by IAB");

    sCorruptCopMgr.AdvanceIABStingStage(stingId, convOk); // REFORM_COMPLETED
    TEST_ASSERT(convOk == true, "Conviction secured and department reformed");
    TEST_ASSERT(p13->corruptionScore <= 60.0f, "Precinct corruption dropped substantially after IAB purge");

    // 8. Frank Castle Retribution & Agent Possessions
    uint32 dropCastle = sCorruptCopMgr.ScheduleBribeDrop(8, 3, false, "International Docks Warehouse",
                                                         LocationVector(8000.0, 0.0, -4000.0), 25000, 1003);
    bool ambushOk = sCorruptCopMgr.ExecuteCastleAmbushOnBribeDrop(dropCastle, true);
    TEST_ASSERT(ambushOk, "Frank Castle ambushed syndicate bribe drop");
    TEST_ASSERT(oChen->isAlive == false, "Castle executed unrepentant dirty bagman");
    CastleTaintedShieldEntry* castleEntry = sCorruptCopMgr.GetCastleTarget(1003);
    TEST_ASSERT(castleEntry != nullptr && castleEntry->isConfessionTaped == true, "Castle extracted taped confession");

    // Castle delivers tape to IAB
    sCorruptCopMgr.LogCastleTaintedShieldTarget(1002, "Evidence destruction for Marcone mob");
    CastleTaintedShieldEntry* cVane = sCorruptCopMgr.GetCastleTarget(1002);
    if (cVane) cVane->isConfessionTaped = true;

    CorruptOfficerProfile* oVane = sCorruptCopMgr.GetOfficer(1002);
    bool tapeDeliv = sCorruptCopMgr.DeliverCastleConfessionTapeToIAB(1002);
    TEST_ASSERT(tapeDeliv, "Castle confession tape delivered to IAB");
    TEST_ASSERT(oVane && oVane->isArrestedByIAB == true, "Lt. Vane arrested upon Castle delivering confession tape to IAB");

    // Agent possession susceptibility & host overwrite
    float linSuscept = sCorruptCopMgr.EvaluateAgentPossessionSusceptibility(1006);
    float vaneSuscept = sCorruptCopMgr.EvaluateAgentPossessionSusceptibility(1002);
    TEST_ASSERT(linSuscept < 0.20f, "Clean whistleblower has low Agent possession susceptibility");
    TEST_ASSERT(vaneSuscept > 0.50f, "Corrupt detective has high Agent possession susceptibility");

    bool possOk = sCorruptCopMgr.TriggerAgentPossessionOfCorruptCop(1002, 1);
    TEST_ASSERT(possOk, "Machine Agent possessed corrupt cop as unclean vessel");
    TEST_ASSERT(oVane->isAgentPossessed == true, "Officer marked as possessed");
    TEST_ASSERT(oVane->isAlive == false, "Host body expired post-possession");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  CORRUPT COPS & INTERNAL AFFAIRS TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

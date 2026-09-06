#include "MafiaEcosystemManager.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cassert>

// Singleton instantiation
createFileSingleton(MafiaEcosystemManager);

MafiaEcosystemManager::MafiaEcosystemManager()
{
    Initialize();
}

MafiaEcosystemManager::~MafiaEcosystemManager()
{
}

void MafiaEcosystemManager::Initialize()
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    if (m_initialized) return;

    InitializeTheFiveFamilies();
    m_initialized = true;
}

void MafiaEcosystemManager::Reset()
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    m_families.clear();
    m_crews.clear();
    m_soldiers.clear();
    m_pizzoTargets.clear();
    m_frontBusinesses.clear();
    m_summits.clear();
    m_officials.clear();
    m_hitContracts.clear();

    m_nextCrewId = 101;
    m_nextBusinessId = 501;
    m_nextSummitId = 1001;
    m_nextHitId = 7001;
    m_commonBribePool = 50000.0;
    m_initialized = false;

    InitializeTheFiveFamilies();
    m_initialized = true;
}

void MafiaEcosystemManager::InitializeTheFiveFamilies()
{
    // 1. The Marcone Family (The Concrete Kings - Docks & Sanitation)
    {
        MafiaFamily f;
        f.familyId = MafiaFamilyId::MarconeFamily;
        f.familyName = "The Marcone Family";
        f.epithetTitle = "The Concrete Kings";
        f.territoryDescription = "International District Docks, Tabor Basin, Municipal Sanitation Depot";
        f.donEntityId = 10001;
        f.donName = "Don Carmine 'The Anvil' Marcone";
        f.underbossEntityId = 10002;
        f.underbossName = "Salvatore 'Sal the Rat-Trap' Corelli";
        f.consigliereEntityId = 10003;
        f.consigliereName = "Father Thomas DeVito";
        f.treasuryCleanBits = 35000.0;
        f.treasuryDirtyBits = 18000.0;

        uint32 c1 = m_nextCrewId++;
        CapoCrew crew1;
        crew1.crewId = c1;
        crew1.capoEntityId = 10004;
        crew1.crewName = "Tabor Docks Enforcers";
        crew1.familyId = f.familyId;
        crew1.districtId = 2; // International
        crew1.districtName = "International";
        crew1.safehouseLocation = LocationVector(-450.0f, 15.0f, 800.0f);
        f.crewIds.push_back(c1);
        m_crews[c1] = crew1;

        uint32 b1 = m_nextBusinessId++;
        FrontBusiness fb1;
        fb1.businessId = b1;
        fb1.businessName = "Marcone Waste Management Corp";
        fb1.type = FrontBusinessType::WasteManagement;
        fb1.familyId = f.familyId;
        fb1.districtId = 2;
        fb1.location = LocationVector(-400.0f, 12.0f, 750.0f);
        fb1.launderingEfficiency = 0.80f;
        f.frontBusinessIds.push_back(b1);
        m_frontBusinesses[b1] = fb1;

        m_families[static_cast<uint8_t>(f.familyId)] = f;
    }

    // 2. The Valenti Family (The Velvet Siphon - Downtown Nightlife & Casinos)
    {
        MafiaFamily f;
        f.familyId = MafiaFamilyId::ValentiFamily;
        f.familyName = "The Valenti Family";
        f.epithetTitle = "The Velvet Siphon";
        f.territoryDescription = "Downtown Promenade, Theater District, Richland Penthouses";
        f.donEntityId = 10011;
        f.donName = "Don Leonardo 'Lucky Leo' Valenti";
        f.underbossEntityId = 10012;
        f.underbossName = "Dominic 'Silk' Valenti";
        f.consigliereEntityId = 10013;
        f.consigliereName = "Camilla Rossi";
        f.treasuryCleanBits = 60000.0;
        f.treasuryDirtyBits = 25000.0;

        uint32 c1 = m_nextCrewId++;
        CapoCrew crew1;
        crew1.crewId = c1;
        crew1.capoEntityId = 10014;
        crew1.crewName = "Downtown Velvet Crew";
        crew1.familyId = f.familyId;
        crew1.districtId = 1; // Downtown
        crew1.districtName = "Downtown";
        crew1.safehouseLocation = LocationVector(220.0f, 45.0f, -150.0f);
        f.crewIds.push_back(c1);
        m_crews[c1] = crew1;

        uint32 b1 = m_nextBusinessId++;
        FrontBusiness fb1;
        fb1.businessId = b1;
        fb1.businessName = "The Gilded Cage Supper Club";
        fb1.type = FrontBusinessType::NightclubLounge;
        fb1.familyId = f.familyId;
        fb1.districtId = 1;
        fb1.location = LocationVector(250.0f, 40.0f, -120.0f);
        fb1.launderingEfficiency = 0.85f;
        f.frontBusinessIds.push_back(b1);
        m_frontBusinesses[b1] = fb1;

        m_families[static_cast<uint8_t>(f.familyId)] = f;
    }

    // 3. The Scarlotti Syndicate (The Iron Arsenal - Midtown Rail Yards & Arms)
    {
        MafiaFamily f;
        f.familyId = MafiaFamilyId::ScarlottiSyndicate;
        f.familyName = "The Scarlotti Syndicate";
        f.epithetTitle = "The Iron Arsenal";
        f.territoryDescription = "Midtown Rail Yards, Westside Freight Depots, Concourse Warehouses";
        f.donEntityId = 10021;
        f.donName = "Don Vittorio 'The Hammer' Scarlotti";
        f.underbossEntityId = 10022;
        f.underbossName = "Enzo 'Scar' Scarlotti";
        f.consigliereEntityId = 10023;
        f.consigliereName = "Professor Leo Strauss";
        f.treasuryCleanBits = 40000.0;
        f.treasuryDirtyBits = 30000.0;

        uint32 c1 = m_nextCrewId++;
        CapoCrew crew1;
        crew1.crewId = c1;
        crew1.capoEntityId = 10024;
        crew1.crewName = "Midtown Heavy Logistics Crew";
        crew1.familyId = f.familyId;
        crew1.districtId = 3; // Midtown
        crew1.districtName = "Midtown";
        crew1.safehouseLocation = LocationVector(120.0f, 10.0f, 500.0f);
        f.crewIds.push_back(c1);
        m_crews[c1] = crew1;

        uint32 b1 = m_nextBusinessId++;
        FrontBusiness fb1;
        fb1.businessId = b1;
        fb1.businessName = "Scarlotti Rail & Freight Logistics";
        fb1.type = FrontBusinessType::RailLogistics;
        fb1.familyId = f.familyId;
        fb1.districtId = 3;
        fb1.location = LocationVector(100.0f, 8.0f, 480.0f);
        fb1.launderingEfficiency = 0.70f;
        f.frontBusinessIds.push_back(b1);
        m_frontBusinesses[b1] = fb1;

        m_families[static_cast<uint8_t>(f.familyId)] = f;
    }

    // 4. The Chen-Wu Triad (The Golden Dragon Tong - Chinatown & Code Opium)
    {
        MafiaFamily f;
        f.familyId = MafiaFamilyId::ChenWuTriad;
        f.familyName = "The Chen-Wu Triad";
        f.epithetTitle = "The Golden Dragon Tong";
        f.territoryDescription = "Chinatown, Morrell Street Alleys, South Canal Tea Warehouses";
        f.donEntityId = 10031;
        f.donName = "Master Chen Shan (Dragon Head)";
        f.underbossEntityId = 10032;
        f.underbossName = "Wei 'The Ghost' Long (Vanguard)";
        f.consigliereEntityId = 10033;
        f.consigliereName = "Madame May-Ling Wu (Incense Master)";
        f.treasuryCleanBits = 50000.0;
        f.treasuryDirtyBits = 22000.0;

        uint32 c1 = m_nextCrewId++;
        CapoCrew crew1;
        crew1.crewId = c1;
        crew1.capoEntityId = 10034;
        crew1.crewName = "Lotus Canal Brotherhood";
        crew1.familyId = f.familyId;
        crew1.districtId = 4; // Chinatown
        crew1.districtName = "Chinatown";
        crew1.safehouseLocation = LocationVector(-300.0f, 20.0f, -400.0f);
        f.crewIds.push_back(c1);
        m_crews[c1] = crew1;

        uint32 b1 = m_nextBusinessId++;
        FrontBusiness fb1;
        fb1.businessId = b1;
        fb1.businessName = "Golden Lotus Herbal Apothecary";
        fb1.type = FrontBusinessType::HerbalApothecary;
        fb1.familyId = f.familyId;
        fb1.districtId = 4;
        fb1.location = LocationVector(-280.0f, 18.0f, -390.0f);
        fb1.launderingEfficiency = 0.78f;
        f.frontBusinessIds.push_back(b1);
        m_frontBusinesses[b1] = fb1;

        m_families[static_cast<uint8_t>(f.familyId)] = f;
    }

    // 5. The Petrov Bratva (The Northern Vor - Slums Sump & Cyber-Extortion)
    {
        MafiaFamily f;
        f.familyId = MafiaFamilyId::PetrovBratva;
        f.familyName = "The Petrov Bratva";
        f.epithetTitle = "The Northern Vor";
        f.territoryDescription = "Westview Slums, North Trench Utility Tunnels, Meatpacking Sump";
        f.donEntityId = 10041;
        f.donName = "Aleksei 'The Tsar' Petrov (Vor v Zakone)";
        f.underbossEntityId = 10042;
        f.underbossName = "Mikhail 'The Bear' Volkov (Brigadier)";
        f.consigliereEntityId = 10043;
        f.consigliereName = "Ilya 'The Calculator' Rozov";
        f.treasuryCleanBits = 30000.0;
        f.treasuryDirtyBits = 35000.0;

        uint32 c1 = m_nextCrewId++;
        CapoCrew crew1;
        crew1.crewId = c1;
        crew1.capoEntityId = 10044;
        crew1.crewName = "Westview Iron Wolves";
        crew1.familyId = f.familyId;
        crew1.districtId = 5; // Slums
        crew1.districtName = "Slums";
        crew1.safehouseLocation = LocationVector(-600.0f, 5.0f, 100.0f);
        f.crewIds.push_back(c1);
        m_crews[c1] = crew1;

        uint32 b1 = m_nextBusinessId++;
        FrontBusiness fb1;
        fb1.businessId = b1;
        fb1.businessName = "Tsarina 24-Hour Laundromat";
        fb1.type = FrontBusinessType::CommercialLaundry;
        fb1.familyId = f.familyId;
        fb1.districtId = 5;
        fb1.location = LocationVector(-580.0f, 5.0f, 110.0f);
        fb1.launderingEfficiency = 0.72f;
        f.frontBusinessIds.push_back(b1);
        m_frontBusinesses[b1] = fb1;

        m_families[static_cast<uint8_t>(f.familyId)] = f;
    }
}

void MafiaEcosystemManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    // Passive decay and circadian pulse
}

// ============================================================================
// Family & Hierarchy Management
// ============================================================================

const MafiaFamily* MafiaEcosystemManager::GetFamily(MafiaFamilyId id) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_families.find(static_cast<uint8_t>(id));
    return it != m_families.end() ? &it->second : nullptr;
}

MafiaFamily* MafiaEcosystemManager::GetFamilyMut(MafiaFamilyId id)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_families.find(static_cast<uint8_t>(id));
    return it != m_families.end() ? &it->second : nullptr;
}

const CapoCrew* MafiaEcosystemManager::GetCrew(uint32 crewId) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_crews.find(crewId);
    return it != m_crews.end() ? &it->second : nullptr;
}

CapoCrew* MafiaEcosystemManager::GetCrewMut(uint32 crewId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_crews.find(crewId);
    return it != m_crews.end() ? &it->second : nullptr;
}

const MadeSoldier* MafiaEcosystemManager::GetSoldier(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_soldiers.find(entityId);
    return it != m_soldiers.end() ? &it->second : nullptr;
}

MadeSoldier* MafiaEcosystemManager::GetSoldierMut(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_soldiers.find(entityId);
    return it != m_soldiers.end() ? &it->second : nullptr;
}

bool MafiaEcosystemManager::InductMadeMan(uint32 entityId, const std::string& name, const std::string& moniker,
                                         MafiaFamilyId familyId, uint32 crewId, MobRank rank)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    MadeSoldier s;
    s.entityId = entityId;
    s.name = name;
    s.moniker = moniker;
    s.rank = rank;
    s.familyId = familyId;
    s.crewId = crewId;
    s.omertaLoyalty = 95.0f;
    s.paranoia = 10.0f;
    s.isFlippedInformant = false;
    s.isAlive = true;

    m_soldiers[entityId] = s;

    auto cIt = m_crews.find(crewId);
    if (cIt != m_crews.end()) {
        cIt->second.memberEntityIds.push_back(entityId);
    }

    auto fIt = m_families.find(static_cast<uint8_t>(familyId));
    if (fIt != m_families.end()) {
        fIt->second.totalMadeMenCount++;
    }
    return true;
}

bool MafiaEcosystemManager::PromoteMember(uint32 entityId, MobRank newRank)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_soldiers.find(entityId);
    if (it == m_soldiers.end()) return false;
    it->second.rank = newRank;
    it->second.omertaLoyalty = std::min(100.0f, it->second.omertaLoyalty + 15.0f);
    return true;
}

// ============================================================================
// Pizzo Extortion & Front Business Laundering
// ============================================================================

bool MafiaEcosystemManager::RegisterShopExtortion(uint32 shopId, const std::string& shopName, uint32 districtId,
                                                 uint32 crewId, double weeklyFee)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto cIt = m_crews.find(crewId);
    if (cIt == m_crews.end()) return false;

    PizzoExtortionTarget target;
    target.shopId = shopId;
    target.shopName = shopName;
    target.districtId = districtId;
    target.crewId = crewId;
    target.controllingFamily = cIt->second.familyId;
    target.state = PizzoState::ActivePizzo;
    target.weeklyFeeBits = weeklyFee;
    target.totalBitsExtorted = 0.0;
    target.missedPaymentsCount = 0;
    target.merchantTerrorIndex = 0.25f;
    target.isArsonBurned = false;

    m_pizzoTargets[shopId] = target;
    cIt->second.extortedShopIds.push_back(shopId);
    return true;
}

const PizzoExtortionTarget* MafiaEcosystemManager::GetPizzoTarget(uint32 shopId) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_pizzoTargets.find(shopId);
    return it != m_pizzoTargets.end() ? &it->second : nullptr;
}

PizzoExtortionTarget* MafiaEcosystemManager::GetPizzoTargetMut(uint32 shopId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_pizzoTargets.find(shopId);
    return it != m_pizzoTargets.end() ? &it->second : nullptr;
}

void MafiaEcosystemManager::ProcessWeeklyPizzoCollections()
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    for (auto& kvp : m_pizzoTargets) {
        auto& t = kvp.second;
        if (t.state == PizzoState::ActivePizzo || t.state == PizzoState::SoftShakedown) {
            t.totalBitsExtorted += t.weeklyFeeBits;
            
            auto cIt = m_crews.find(t.crewId);
            if (cIt != m_crews.end()) {
                cIt->second.weeklyTributeDelivered += t.weeklyFeeBits;
            }

            auto fIt = m_families.find(static_cast<uint8_t>(t.controllingFamily));
            if (fIt != m_families.end()) {
                fIt->second.treasuryDirtyBits += t.weeklyFeeBits;
            }
        }
    }
}

bool MafiaEcosystemManager::DefaultOnPizzo(uint32 shopId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_pizzoTargets.find(shopId);
    if (it == m_pizzoTargets.end()) return false;
    it->second.missedPaymentsCount++;
    it->second.merchantTerrorIndex = std::min(1.0f, it->second.merchantTerrorIndex + 0.35f);
    return true;
}

bool MafiaEcosystemManager::ExecuteArsonShakedown(uint32 shopId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_pizzoTargets.find(shopId);
    if (it == m_pizzoTargets.end()) return false;

    it->second.state = PizzoState::DefaultArson;
    it->second.isArsonBurned = true;
    it->second.merchantTerrorIndex = 0.95f;

    auto fIt = m_families.find(static_cast<uint8_t>(it->second.controllingFamily));
    if (fIt != m_families.end()) {
        fIt->second.familyHeat = std::min(100.0f, fIt->second.familyHeat + 15.0f);
    }
    return true;
}

bool MafiaEcosystemManager::BustOutTakeover(uint32 shopId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_pizzoTargets.find(shopId);
    if (it == m_pizzoTargets.end()) return false;

    it->second.state = PizzoState::BustOutTakeover;
    it->second.merchantTerrorIndex = 1.0f;

    auto fIt = m_families.find(static_cast<uint8_t>(it->second.controllingFamily));
    if (fIt != m_families.end()) {
        fIt->second.treasuryDirtyBits += 5000.0; // Bust-out liquidation proceeds
        fIt->second.familyHeat = std::min(100.0f, fIt->second.familyHeat + 25.0f);
    }
    return true;
}

bool MafiaEcosystemManager::RegisterFrontBusiness(uint32 id, const std::string& name, FrontBusinessType type,
                                                  MafiaFamilyId familyId, uint32 districtId, LocationVector loc, float efficiency)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    FrontBusiness fb;
    fb.businessId = id;
    fb.businessName = name;
    fb.type = type;
    fb.familyId = familyId;
    fb.districtId = districtId;
    fb.location = loc;
    fb.launderingEfficiency = efficiency;
    m_frontBusinesses[id] = fb;

    auto fIt = m_families.find(static_cast<uint8_t>(familyId));
    if (fIt != m_families.end()) {
        fIt->second.frontBusinessIds.push_back(id);
    }
    return true;
}

const FrontBusiness* MafiaEcosystemManager::GetFrontBusiness(uint32 id) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_frontBusinesses.find(id);
    return it != m_frontBusinesses.end() ? &it->second : nullptr;
}

double MafiaEcosystemManager::LaunderDirtyBits(MafiaFamilyId familyId, double dirtyAmount)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto fIt = m_families.find(static_cast<uint8_t>(familyId));
    if (fIt == m_families.end()) return 0.0;

    double availableDirty = std::min(dirtyAmount, fIt->second.treasuryDirtyBits);
    if (availableDirty <= 0.0) return 0.0;

    // Find primary front business efficiency
    float eff = 0.75f;
    if (!fIt->second.frontBusinessIds.empty()) {
        uint32 bId = fIt->second.frontBusinessIds[0];
        auto bIt = m_frontBusinesses.find(bId);
        if (bIt != m_frontBusinesses.end()) {
            eff = bIt->second.launderingEfficiency;
            bIt->second.totalWashCycles++;
            bIt->second.cleanBitsLaunderedTotal += (availableDirty * eff);
        }
    }

    double cleanGained = std::round(availableDirty * eff);
    fIt->second.treasuryDirtyBits -= availableDirty;
    fIt->second.treasuryCleanBits += cleanGained;
    return cleanGained;
}

// ============================================================================
// Omertà, Informants & Whacking Execution
// ============================================================================

bool MafiaEcosystemManager::DecayOmertaLoyalty(uint32 entityId, float delta)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_soldiers.find(entityId);
    if (it == m_soldiers.end()) return false;
    it->second.omertaLoyalty = std::max(0.0f, it->second.omertaLoyalty - delta);
    if (it->second.omertaLoyalty < 30.0f) {
        it->second.paranoia = std::min(100.0f, it->second.paranoia + 35.0f);
    }
    return true;
}

bool MafiaEcosystemManager::FlipInformantToRICO(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_soldiers.find(entityId);
    if (it == m_soldiers.end()) return false;

    it->second.isFlippedInformant = true;
    it->second.omertaLoyalty = 0.0f;
    it->second.paranoia = 90.0f;

    auto fIt = m_families.find(static_cast<uint8_t>(it->second.familyId));
    if (fIt != m_families.end()) {
        fIt->second.ricoIndictmentMeter = std::min(1.0f, fIt->second.ricoIndictmentMeter + 0.35f);
    }
    return true;
}

bool MafiaEcosystemManager::AuditFamilyForRats(MafiaFamilyId familyId, uint32& outSuspectId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    for (const auto& kvp : m_soldiers) {
        if (kvp.second.familyId == familyId && kvp.second.isAlive) {
            if (kvp.second.isFlippedInformant || kvp.second.omertaLoyalty < 20.0f) {
                outSuspectId = kvp.second.entityId;
                return true;
            }
        }
    }
    outSuspectId = 0;
    return false;
}

uint32 MafiaEcosystemManager::IssueHitContract(MafiaFamilyId orderingFamily, uint32 targetEntityId,
                                              const std::string& reason, bool requestCommissionSanction)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    uint32 hid = m_nextHitId++;
    HitContract hit;
    hit.hitId = hid;
    hit.targetEntityId = targetEntityId;
    hit.orderingFamily = orderingFamily;
    hit.justification = reason;
    hit.status = WhackingStatus::HitContractIssued;
    hit.isSanctionedByCommission = !requestCommissionSanction; // if not requesting, treated as rogue/internal

    auto sIt = m_soldiers.find(targetEntityId);
    if (sIt != m_soldiers.end()) {
        hit.targetName = sIt->second.name + " ('" + sIt->second.moniker + "')";
    } else {
        hit.targetName = "Target#" + std::to_string(targetEntityId);
    }

    if (requestCommissionSanction) {
        // Create an automatic summit to sanction the hit
        uint32 sid = m_nextSummitId++;
        CommissionSummit sum;
        sum.summitId = sid;
        sum.topic = "Sanction Whacking of " + hit.targetName + ": " + reason;
        sum.status = CommissionVoteStatus::Approved; // default majority consent in underworld law
        sum.initiatingFamily = orderingFamily;
        sum.familyVotes[static_cast<uint8_t>(orderingFamily)] = true;
        sum.targetEntityId = targetEntityId;
        m_summits[sid] = sum;
        hit.isSanctionedByCommission = true;
        hit.status = WhackingStatus::Sanctioned;
    }

    m_hitContracts[hid] = hit;
    return hid;
}

bool MafiaEcosystemManager::ExecuteWhacking(uint32 hitId, uint32 hitmanId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_hitContracts.find(hitId);
    if (it == m_hitContracts.end()) return false;

    it->second.status = WhackingStatus::ExecutedCleaned;
    it->second.assignedHitmanId = hitmanId;

    auto sIt = m_soldiers.find(it->second.targetEntityId);
    if (sIt != m_soldiers.end()) {
        sIt->second.isAlive = false;
    }

    auto hIt = m_soldiers.find(hitmanId);
    if (hIt != m_soldiers.end()) {
        hIt->second.hitsCarriedOut++;
    }

    auto fIt = m_families.find(static_cast<uint8_t>(it->second.orderingFamily));
    if (fIt != m_families.end()) {
        fIt->second.totalWhackingsExecuted++;
    }
    return true;
}

bool MafiaEcosystemManager::DeployExileCleaners(uint32 hitId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_hitContracts.find(hitId);
    if (it == m_hitContracts.end()) return false;

    it->second.wasBodyCleanedByExiles = true;
    auto fIt = m_families.find(static_cast<uint8_t>(it->second.orderingFamily));
    if (fIt != m_families.end()) {
        fIt->second.familyHeat = std::max(0.0f, fIt->second.familyHeat - 20.0f);
        fIt->second.treasuryCleanBits = std::max(0.0, fIt->second.treasuryCleanBits - 2000.0); // Cleaner fee
    }
    return true;
}

const HitContract* MafiaEcosystemManager::GetHitContract(uint32 hitId) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_hitContracts.find(hitId);
    return it != m_hitContracts.end() ? &it->second : nullptr;
}

// ============================================================================
// La Commissione High Council Governance
// ============================================================================

uint32 MafiaEcosystemManager::ConveneCommissionSummit(const std::string& topic, MafiaFamilyId initiatingFamily, uint32 targetEntityId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    uint32 sid = m_nextSummitId++;
    CommissionSummit sum;
    sum.summitId = sid;
    sum.topic = topic;
    sum.status = CommissionVoteStatus::Pending;
    sum.initiatingFamily = initiatingFamily;
    sum.targetEntityId = targetEntityId;
    sum.familyVotes[static_cast<uint8_t>(initiatingFamily)] = true; // Initiator votes aye
    m_summits[sid] = sum;
    return sid;
}

bool MafiaEcosystemManager::CastCommissionVote(uint32 summitId, MafiaFamilyId votingFamily, bool approve)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_summits.find(summitId);
    if (it == m_summits.end()) return false;

    it->second.familyVotes[static_cast<uint8_t>(votingFamily)] = approve;

    uint32 ayes = 0;
    uint32 nays = 0;
    for (const auto& kvp : it->second.familyVotes) {
        if (kvp.second) ayes++; else nays++;
    }

    if (ayes >= 3) {
        it->second.status = CommissionVoteStatus::Approved;
        it->second.outcomeResolution = "Commission Approved: Resolution passed with majority consent.";
    } else if (nays >= 3) {
        it->second.status = CommissionVoteStatus::Vetoed;
        it->second.outcomeResolution = "Commission Vetoed: Motion rejected by council majority.";
    }
    return true;
}

const CommissionSummit* MafiaEcosystemManager::GetCommissionSummit(uint32 summitId) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_summits.find(summitId);
    return it != m_summits.end() ? &it->second : nullptr;
}

void MafiaEcosystemManager::DeclareInterFamilyTruce()
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    for (auto& kvp : m_families) {
        kvp.second.familyHeat = std::max(10.0f, kvp.second.familyHeat * 0.5f);
    }
    for (auto& kvp : m_summits) {
        if (kvp.second.status == CommissionVoteStatus::Pending) {
            kvp.second.status = CommissionVoteStatus::EmergencyTruce;
            kvp.second.outcomeResolution = "Emergency Truce signed under Commission auspices.";
        }
    }
}

bool MafiaEcosystemManager::ContributeToBribePool(MafiaFamilyId familyId, double amount)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto fIt = m_families.find(static_cast<uint8_t>(familyId));
    if (fIt == m_families.end()) return false;

    double transfer = std::min(amount, fIt->second.treasuryCleanBits);
    if (transfer <= 0.0) return false;

    fIt->second.treasuryCleanBits -= transfer;
    m_commonBribePool += transfer;
    return true;
}

// ============================================================================
// Political & Judicial Corruption
// ============================================================================

bool MafiaEcosystemManager::RegisterCorruptOfficial(uint32 id, const std::string& name, const std::string& role,
                                                    MafiaFamilyId family, double weeklyCost)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    CorruptOfficial off;
    off.officialId = id;
    off.name = name;
    off.roleTitle = role;
    off.owningFamily = family;
    off.weeklyBribeCost = weeklyCost;
    off.corruptionLoyalty = 85.0f;
    off.isExposedByInternalAffairs = false;
    m_officials[id] = off;
    return true;
}

void MafiaEcosystemManager::AdvanceRICOInvestigation(MafiaFamilyId family, float heatDelta)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto fIt = m_families.find(static_cast<uint8_t>(family));
    if (fIt == m_families.end()) return;

    fIt->second.ricoIndictmentMeter = std::min(1.0f, fIt->second.ricoIndictmentMeter + heatDelta);
    if (fIt->second.ricoIndictmentMeter >= 0.85f) {
        fIt->second.isUnderRICOIndictment = true;
    }
}

bool MafiaEcosystemManager::MitigateRICOWithBribe(MafiaFamilyId family, double cost)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto fIt = m_families.find(static_cast<uint8_t>(family));
    if (fIt == m_families.end()) return false;

    if (fIt->second.treasuryCleanBits < cost && m_commonBribePool < cost) return false;

    if (fIt->second.treasuryCleanBits >= cost) {
        fIt->second.treasuryCleanBits -= cost;
    } else {
        m_commonBribePool -= cost;
    }

    fIt->second.ricoIndictmentMeter = std::max(0.0f, fIt->second.ricoIndictmentMeter - 0.40f);
    if (fIt->second.ricoIndictmentMeter < 0.70f) {
        fIt->second.isUnderRICOIndictment = false;
    }
    return true;
}

bool MafiaEcosystemManager::TriggerRICOSwatRaid(MafiaFamilyId family)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto fIt = m_families.find(static_cast<uint8_t>(family));
    if (fIt == m_families.end()) return false;

    fIt->second.familyHeat = 100.0f;
    fIt->second.treasuryDirtyBits = std::max(0.0, fIt->second.treasuryDirtyBits - 10000.0); // Seized cash
    return true;
}

// ============================================================================
// Adversarial Interlocks
// ============================================================================

bool MafiaEcosystemManager::TriggerCastleSafehouseIncursion(uint32 crewId, const std::string& assaultDetails)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto cIt = m_crews.find(crewId);
    if (cIt == m_crews.end()) return false;

    cIt->second.crewHeat = 100.0f;
    for (uint32 mId : cIt->second.memberEntityIds) {
        auto sIt = m_soldiers.find(mId);
        if (sIt != m_soldiers.end()) {
            sIt->second.paranoia = 100.0f;
        }
    }
    return true;
}

bool MafiaEcosystemManager::TriggerAgentAnomalyIntervention(uint32 districtId)
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    for (auto& kvp : m_crews) {
        if (kvp.second.districtId == districtId) {
            kvp.second.crewHeat = std::max(0.0f, kvp.second.crewHeat - 40.0f); // Gangsters scatter
        }
    }
    return true;
}

// ============================================================================
// Telemetry & Reporting
// ============================================================================

size_t MafiaEcosystemManager::GetTotalMadeMenCount() const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    size_t count = 0;
    for (const auto& kvp : m_soldiers) {
        if (kvp.second.isAlive && kvp.second.rank >= MobRank::SoldierMadeMan) {
            count++;
        }
    }
    return count;
}

std::string MafiaEcosystemManager::GetFamilyName(MafiaFamilyId id)
{
    switch (id) {
        case MafiaFamilyId::MarconeFamily:      return "The Marcone Family";
        case MafiaFamilyId::ValentiFamily:      return "The Valenti Family";
        case MafiaFamilyId::ScarlottiSyndicate: return "The Scarlotti Syndicate";
        case MafiaFamilyId::ChenWuTriad:        return "The Chen-Wu Triad";
        case MafiaFamilyId::PetrovBratva:       return "The Petrov Bratva";
    }
    return "Unknown Syndicate";
}

std::string MafiaEcosystemManager::GetRankName(MobRank rank)
{
    switch (rank) {
        case MobRank::Associate:      return "Associate";
        case MobRank::SoldierMadeMan: return "Soldier (Made Man)";
        case MobRank::Caporegime:     return "Caporegime";
        case MobRank::Consigliere:    return "Consigliere";
        case MobRank::Underboss:      return "Underboss";
        case MobRank::DonGodfather:   return "Don / Godfather";
    }
    return "Unknown Rank";
}

std::string MafiaEcosystemManager::GetPizzoStateName(PizzoState state)
{
    switch (state) {
        case PizzoState::Unmarked:        return "Unmarked";
        case PizzoState::SoftShakedown:   return "Soft Shakedown";
        case PizzoState::ActivePizzo:     return "Active Pizzo";
        case PizzoState::DefaultArson:    return "Default (Arson Burned)";
        case PizzoState::BustOutTakeover: return "Bust-Out Takeover";
    }
    return "Unknown State";
}

std::string MafiaEcosystemManager::GetFrontTypeName(FrontBusinessType type)
{
    switch (type) {
        case FrontBusinessType::WasteManagement:   return "Waste Management & Sanitation";
        case FrontBusinessType::NightclubLounge:   return "Nightclub & VIP Lounge";
        case FrontBusinessType::RailLogistics:     return "Rail Freight & Logistics";
        case FrontBusinessType::HerbalApothecary:  return "Herbal Apothecary & Tea Wholesaling";
        case FrontBusinessType::CommercialLaundry: return "Commercial Laundry";
    }
    return "General Commercial Front";
}

std::string MafiaEcosystemManager::GenerateMafiaWorldReport() const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    std::ostringstream oss;
    oss << "=== MEGACITY MAFIA ECOSYSTEM: LA COMMISSIONE REPORT ===\n";
    oss << "Common Defense Bribe Pool: " << std::fixed << std::setprecision(0) << m_commonBribePool << " bits\n";
    oss << "Total Families: " << m_families.size() << " | Crews: " << m_crews.size()
        << " | Extorted Shops: " << m_pizzoTargets.size() << " | Front Businesses: " << m_frontBusinesses.size() << "\n\n";

    for (const auto& kvp : m_families) {
        const auto& f = kvp.second;
        oss << "[" << f.familyName << "] '" << f.epithetTitle << "'\n";
        oss << "  Don: " << f.donName << " | Underboss: " << f.underbossName << "\n";
        oss << "  Treasury Clean: " << f.treasuryCleanBits << " bits | Dirty: " << f.treasuryDirtyBits << " bits\n";
        oss << "  RICO Meter: " << (int)(f.ricoIndictmentMeter * 100.0f) << "% | Status: "
            << (f.isUnderRICOIndictment ? "UNDER INDICTMENT" : "Clear") << "\n";
        oss << "  Whackings Executed: " << f.totalWhackingsExecuted << "\n\n";
    }
    return oss.str();
}

std::string MafiaEcosystemManager::GenerateFamilyDossier(MafiaFamilyId id) const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    auto it = m_families.find(static_cast<uint8_t>(id));
    if (it == m_families.end()) return "Family not found.";

    const auto& f = it->second;
    std::ostringstream oss;
    oss << "DOSSIER: " << f.familyName << " (" << f.epithetTitle << ")\n";
    oss << "Territory: " << f.territoryDescription << "\n";
    oss << "Don: " << f.donName << "\n";
    oss << "Underboss: " << f.underbossName << "\n";
    oss << "Consigliere: " << f.consigliereName << "\n";
    oss << "Clean Treasury: " << f.treasuryCleanBits << " bits | Dirty: " << f.treasuryDirtyBits << " bits\n";
    oss << "Active Crews: " << f.crewIds.size() << "\n";
    for (uint32 cId : f.crewIds) {
        auto cIt = m_crews.find(cId);
        if (cIt != m_crews.end()) {
            oss << "  - Crew #" << cId << ": " << cIt->second.crewName << " (" << cIt->second.districtName
                << ") Shops: " << cIt->second.extortedShopIds.size() << "\n";
        }
    }
    return oss.str();
}

std::string MafiaEcosystemManager::GenerateCommissionReport() const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    std::ostringstream oss;
    oss << "=== LA COMMISSIONE SUMMITS ===\n";
    for (const auto& kvp : m_summits) {
        oss << "Summit #" << kvp.second.summitId << ": " << kvp.second.topic << "\n";
        oss << "Status: " << (kvp.second.status == CommissionVoteStatus::Approved ? "APPROVED" : "PENDING/VETOED") << "\n";
    }
    return oss.str();
}

std::string MafiaEcosystemManager::GeneratePizzoExtortionReport() const
{
    std::lock_guard<std::mutex> lock(m_mafiaMutex);
    std::ostringstream oss;
    oss << "=== PIZZO EXTORTION ROSTER ===\n";
    for (const auto& kvp : m_pizzoTargets) {
        oss << "Shop #" << kvp.second.shopId << ": " << kvp.second.shopName
            << " | State: " << GetPizzoStateName(kvp.second.state)
            << " | Extorted: " << kvp.second.totalBitsExtorted << " bits\n";
    }
    return oss.str();
}

// ============================================================================
// Automated C++ Test Suite Implementation
// ============================================================================

void RunMafiaEcosystemTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING MEGACITY MAFIA & LA COMMISSIONE TEST SUITE       " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    MafiaEcosystemManager& mgr = sMafiaMgr;
    mgr.Reset();

    int passedCount = 0;
    int failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passedCount++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failedCount++;
        }
    };

    // 1. The Five Families Initialization
    {
        const auto* marcone = mgr.GetFamily(MafiaFamilyId::MarconeFamily);
        TEST_ASSERT(marcone != nullptr, "Marcone Family Initialized");
        TEST_ASSERT(marcone->donName.find("Carmine") != std::string::npos, "Don Carmine Marcone Correct");
        TEST_ASSERT(marcone->underbossName.find("Corelli") != std::string::npos, "Underboss Salvatore Corelli Correct");

        const auto* valenti = mgr.GetFamily(MafiaFamilyId::ValentiFamily);
        TEST_ASSERT(valenti != nullptr && valenti->donName.find("Lucky Leo") != std::string::npos, "Valenti Family Initialized");

        const auto* scarlotti = mgr.GetFamily(MafiaFamilyId::ScarlottiSyndicate);
        TEST_ASSERT(scarlotti != nullptr && scarlotti->donName.find("Vittorio") != std::string::npos, "Scarlotti Syndicate Initialized");

        const auto* triad = mgr.GetFamily(MafiaFamilyId::ChenWuTriad);
        TEST_ASSERT(triad != nullptr && triad->donName.find("Chen Shan") != std::string::npos, "Chen-Wu Triad Initialized");

        const auto* bratva = mgr.GetFamily(MafiaFamilyId::PetrovBratva);
        TEST_ASSERT(bratva != nullptr && bratva->donName.find("Petrov") != std::string::npos, "Petrov Bratva Initialized");
    }

    // 2. Made Man Blood Oath Induction & Hierarchy Promotion
    {
        uint32 soldierId = 20001;
        bool inducted = mgr.InductMadeMan(soldierId, "Giacomo Scapelli", "Jimmy Two-Times",
                                          MafiaFamilyId::MarconeFamily, 101, MobRank::SoldierMadeMan);
        TEST_ASSERT(inducted, "Inducted Made Man with Blood Oath");

        const auto* soldier = mgr.GetSoldier(soldierId);
        TEST_ASSERT(soldier != nullptr, "Made Soldier Retrieved");
        TEST_ASSERT(soldier->rank == MobRank::SoldierMadeMan, "Soldier Rank Confirmed");
        TEST_ASSERT(soldier->omertaLoyalty >= 90.0f, "Omerta Loyalty High Upon Induction");

        bool promoted = mgr.PromoteMember(soldierId, MobRank::Caporegime);
        TEST_ASSERT(promoted, "Promoted to Caporegime");
        soldier = mgr.GetSoldier(soldierId);
        TEST_ASSERT(soldier->rank == MobRank::Caporegime, "Caporegime Rank Updated");
    }

    // 3. Pizzo Extortion Registration & Weekly Collections
    {
        bool reg = mgr.RegisterShopExtortion(301, "Luigi's Pizzeria", 2, 101, 500.0);
        TEST_ASSERT(reg, "Registered Luigi's Pizzeria for Pizzo Extortion");

        const auto* target = mgr.GetPizzoTarget(301);
        TEST_ASSERT(target != nullptr, "Extortion Target Found");
        TEST_ASSERT(target->state == PizzoState::ActivePizzo, "Shop State is Active Pizzo");

        mgr.ProcessWeeklyPizzoCollections();
        target = mgr.GetPizzoTarget(301);
        TEST_ASSERT(target->totalBitsExtorted == 500.0, "Weekly Pizzo Successfully Collected");

        const auto* marcone = mgr.GetFamily(MafiaFamilyId::MarconeFamily);
        TEST_ASSERT(marcone->treasuryDirtyBits > 18000.0, "Family Dirty Treasury Increased from Pizzo");
    }

    // 4. Default, Arson Shakedown & Bust-Out Takeover
    {
        mgr.DefaultOnPizzo(301);
        const auto* target = mgr.GetPizzoTarget(301);
        TEST_ASSERT(target->missedPaymentsCount == 1, "Missed Payment Counted");
        TEST_ASSERT(target->merchantTerrorIndex > 0.5f, "Merchant Terror Escalated");

        mgr.ExecuteArsonShakedown(301);
        target = mgr.GetPizzoTarget(301);
        TEST_ASSERT(target->state == PizzoState::DefaultArson, "Shop Torched under Default Arson");
        TEST_ASSERT(target->isArsonBurned, "Shop Flagged as Arson Burned");

        mgr.BustOutTakeover(301);
        target = mgr.GetPizzoTarget(301);
        TEST_ASSERT(target->state == PizzoState::BustOutTakeover, "Shop Liquidated in Bust-Out Takeover");
        TEST_ASSERT(target->merchantTerrorIndex == 1.0f, "Merchant Terror at Absolute Maximum");
    }

    // 5. Front Business Registration & Money Laundering Efficiency
    {
        double cleanGained = mgr.LaunderDirtyBits(MafiaFamilyId::MarconeFamily, 10000.0);
        TEST_ASSERT(std::abs(cleanGained - 8000.0) < 1.0, "Laundered 10,000 Dirty Bits at 80% Efficiency to 8,000 Clean Bits");

        const auto* marcone = mgr.GetFamily(MafiaFamilyId::MarconeFamily);
        TEST_ASSERT(std::abs(marcone->treasuryCleanBits - 43000.0) < 1.0, "Clean Treasury Updated with Laundered Bits");
    }

    // 6. Omertà Loyalty Decay & Consigliere Rat Audit
    {
        uint32 ratId = 20002;
        mgr.InductMadeMan(ratId, "Paolo Vento", "The Canary",
                          MafiaFamilyId::MarconeFamily, 101, MobRank::SoldierMadeMan);

        mgr.DecayOmertaLoyalty(ratId, 80.0f);
        const auto* rat = mgr.GetSoldier(ratId);
        TEST_ASSERT(rat->omertaLoyalty < 20.0f, "Omerta Loyalty Decayed under Stress");
        TEST_ASSERT(rat->paranoia > 40.0f, "Paranoia Escalated on Low Loyalty");

        mgr.FlipInformantToRICO(ratId);
        rat = mgr.GetSoldier(ratId);
        TEST_ASSERT(rat->isFlippedInformant, "Informant Flipped to Federal RICO");

        uint32 suspectId = 0;
        bool foundRat = mgr.AuditFamilyForRats(MafiaFamilyId::MarconeFamily, suspectId);
        TEST_ASSERT(foundRat && suspectId == ratId, "Consigliere Rat Audit Successfully Identified Informant");
    }

    // 7. La Commissione Summit & Hit Sanction Voting
    {
        uint32 summitId = mgr.ConveneCommissionSummit("Sanction Whacking of Paolo Vento for Treason",
                                                      MafiaFamilyId::MarconeFamily, 20002);
        TEST_ASSERT(summitId > 0, "Commission Summit Convened at Il Palazzo Vecchio");

        mgr.CastCommissionVote(summitId, MafiaFamilyId::ValentiFamily, true);
        mgr.CastCommissionVote(summitId, MafiaFamilyId::ScarlottiSyndicate, true);

        const auto* summit = mgr.GetCommissionSummit(summitId);
        TEST_ASSERT(summit->status == CommissionVoteStatus::Approved, "Commission Majority Voted to Sanction Hit");
    }

    // 8. Whacking Execution & Exile Cleaners Body Disposal
    {
        uint32 hitId = mgr.IssueHitContract(MafiaFamilyId::MarconeFamily, 20002, "Federal Rat", true);
        TEST_ASSERT(hitId > 0, "Hit Contract Issued for Made Man");

        uint32 hitmanId = 20001; // Jimmy Two-Times
        bool executed = mgr.ExecuteWhacking(hitId, hitmanId);
        TEST_ASSERT(executed, "Whacking Successfully Executed");

        const auto* deadRat = mgr.GetSoldier(20002);
        TEST_ASSERT(!deadRat->isAlive, "Target Confirmed Deceased");

        const auto* hitman = mgr.GetSoldier(hitmanId);
        TEST_ASSERT(hitman->hitsCarriedOut == 1, "Hitman Notched Execution Count");

        bool cleaned = mgr.DeployExileCleaners(hitId);
        TEST_ASSERT(cleaned, "Exile Cleaners Scrubbed RSI & Disposed of Remains");
    }

    // 9. Political Corruption, Bribes & RICO Indictment Mitigation
    {
        mgr.RegisterCorruptOfficial(901, "Captain Jack Delaney", "Precinct Captain",
                                   MafiaFamilyId::MarconeFamily, 2000.0);

        mgr.AdvanceRICOInvestigation(MafiaFamilyId::MarconeFamily, 0.90f);
        const auto* marcone = mgr.GetFamily(MafiaFamilyId::MarconeFamily);
        TEST_ASSERT(marcone->isUnderRICOIndictment, "RICO Indictment Imminent from Informant Files");

        bool mitigated = mgr.MitigateRICOWithBribe(MafiaFamilyId::MarconeFamily, 5000.0);
        TEST_ASSERT(mitigated, "Judge Bribed to Suppress Wiretaps and Mitigate RICO Clock");
        marcone = mgr.GetFamily(MafiaFamilyId::MarconeFamily);
        TEST_ASSERT(!marcone->isUnderRICOIndictment, "RICO Indictment Successfully Defused");
    }

    // 10. Frank Castle Safehouse Incursion & Punisher Interlock
    {
        bool assaulted = mgr.TriggerCastleSafehouseIncursion(101, "Castle breached front gate with M203 grenade launcher");
        TEST_ASSERT(assaulted, "Frank Castle Safehouse Raid Processed");

        const auto* crew = mgr.GetCrew(101);
        TEST_ASSERT(crew->crewHeat == 100.0f, "Crew Heat Spiked to 100% after Castle Assault");
    }

    // 11. System Agent Anomaly Suppression Interlock
    {
        bool suppressed = mgr.TriggerAgentAnomalyIntervention(2);
        TEST_ASSERT(suppressed, "System Agent Anomaly Intervention Triggered");
    }

    // 12. Scale & World Telemetry Reporting
    {
        for (uint32 id = 30000; id < 30100; ++id) {
            mgr.InductMadeMan(id, "Associate " + std::to_string(id), "Footpad",
                              MafiaFamilyId::ValentiFamily, 102, MobRank::Associate);
        }
        TEST_ASSERT(mgr.GetTotalCrewsCount() >= 5, "Scale Test: Active Crews Across Five Families");
        std::string worldReport = mgr.GenerateMafiaWorldReport();
        TEST_ASSERT(!worldReport.empty(), "Mafia World Report Generated");
        std::string dossier = mgr.GenerateFamilyDossier(MafiaFamilyId::MarconeFamily);
        TEST_ASSERT(!dossier.empty() && dossier.find("Carmine") != std::string::npos, "Marcone Dossier Formatted");
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  MAFIA ECOSYSTEM TEST RESULTS: " << passedCount << " PASSED, " << failedCount << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;
}

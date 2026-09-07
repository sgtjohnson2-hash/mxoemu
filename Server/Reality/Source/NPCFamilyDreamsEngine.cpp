#include "NPCFamilyDreamsEngine.h"
#include "NPCSocialLifeEngine.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cassert>
#include <cmath>
#include <algorithm>

// Singleton instantiation
createFileSingleton(NPCFamilyDreamsEngine);

// ============================================================================
// Constructor & Destructor
// ============================================================================

NPCFamilyDreamsEngine::NPCFamilyDreamsEngine()
{
    Initialize();
}

NPCFamilyDreamsEngine::~NPCFamilyDreamsEngine()
{
}

void NPCFamilyDreamsEngine::Initialize()
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    m_initialized = true;
}

void NPCFamilyDreamsEngine::Reset()
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    m_households.clear();
    m_entityToHousehold.clear();
    m_aspirations.clear();
    m_careers.clear();
    m_nextHouseholdId = 101;
    m_nextAspirationId = 5001;
    m_accumulatedTimeMs = 0;
    m_initialized = true;
}

void NPCFamilyDreamsEngine::Update(uint32 deltaMs)
{
    m_accumulatedTimeMs += deltaMs;
    if (m_accumulatedTimeMs >= 15000) {
        m_accumulatedTimeMs = 0;
        std::lock_guard<std::mutex> lock(m_familyMutex);
        for (auto& pair : m_careers) {
            if (pair.second.burnoutIndex > 0.05f) {
                pair.second.burnoutIndex = std::max(0.0f, pair.second.burnoutIndex - 0.01f);
            }
        }
    }
}

// ============================================================================
// String Conversion Utilities
// ============================================================================

std::string NPCFamilyDreamsEngine::GetKinshipRoleName(KinshipRole role)
{
    switch (role) {
        case KinshipRole::HeadOfHousehold:     return "Head of Household";
        case KinshipRole::Spouse:              return "Spouse / Domestic Partner";
        case KinshipRole::Child:               return "Child / Dependent";
        case KinshipRole::Parent:              return "Parent (Elder in Residence)";
        case KinshipRole::Sibling:             return "Sibling";
        case KinshipRole::FoundFamilyCrewmate: return "Found Family Crewmate";
        case KinshipRole::InLaw:               return "In-Law";
    }
    return "Relative";
}

std::string NPCFamilyDreamsEngine::GetDreamCategoryName(LifeDreamCategory cat)
{
    switch (cat) {
        case LifeDreamCategory::CIVILIAN_PROSPERITY:  return "Civilian Prosperity";
        case LifeDreamCategory::PROFESSIONAL_MASTERY: return "Professional Mastery";
        case LifeDreamCategory::DOMESTIC_DEVOTION:    return "Domestic Devotion";
        case LifeDreamCategory::REDPILL_LIBERATION:   return "Redpill Liberation";
        case LifeDreamCategory::SYSTEMIC_EQUILIBRIUM: return "Systemic Equilibrium";
        case LifeDreamCategory::EXILE_LUXURY:         return "Exile Luxury";
        case LifeDreamCategory::UNDERWORLD_DOMINANCE: return "Underworld Dominance";
        case LifeDreamCategory::JUSTICE_AND_DUTY:     return "Justice & Duty";
    }
    return "Lifelong Ambition";
}

std::string NPCFamilyDreamsEngine::GetCareerTrackName(CareerTrack track)
{
    switch (track) {
        case CareerTrack::CorporateTech:           return "Corporate Tech & Finance";
        case CareerTrack::IndustrialManufacturing: return "Industrial Manufacturing";
        case CareerTrack::MedicalHealthcare:       return "Medical Healthcare";
        case CareerTrack::MunicipalGovernment:     return "Municipal Administration";
        case CareerTrack::RetailCulinaryCommerce:  return "Retail & Culinary Arts";
        case CareerTrack::NightlifeEntertainment:  return "Nightlife Entertainment";
        case CareerTrack::HovercraftZionMilitary:  return "Zion Military & Hovercraft Operations";
        case CareerTrack::SyndicateRacket:         return "Syndicate Operations";
        case CareerTrack::MMPDLawEnforcement:      return "MMPD Law Enforcement";
    }
    return "General Career";
}

std::string NPCFamilyDreamsEngine::GetJobLevelName(JobPositionLevel lvl)
{
    switch (lvl) {
        case JobPositionLevel::Level0_EntryIntern:       return "Level 0: Entry Intern / Apprentice";
        case JobPositionLevel::Level1_JuniorAssociate:   return "Level 1: Junior Associate";
        case JobPositionLevel::Level2_MidLevelSpecialist: return "Level 2: Mid-Level Specialist";
        case JobPositionLevel::Level3_SeniorLead:        return "Level 3: Senior Lead / Foreman";
        case JobPositionLevel::Level4_DirectorMaster:    return "Level 4: Director / Superintendent";
        case JobPositionLevel::Level5_ExecutiveBoss:     return "Level 5: Executive VP / Chief / Don";
    }
    return "Unranked";
}

// ============================================================================
// Household Management
// ============================================================================

uint32 NPCFamilyDreamsEngine::CreateHousehold(uint32 headId, const std::string& headName, const std::string& householdName,
                                             const std::string& homeApartment, LocationVector loc)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    uint32 hid = m_nextHouseholdId++;
    FamilyHousehold h;
    h.householdId = hid;
    h.householdName = householdName.empty() ? (headName + " Household") : householdName;
    h.homeApartmentName = homeApartment;
    h.homeLocation = loc;
    h.headEntityId = headId;
    h.householdSavingsInfoCredits = 1200;
    h.pantryStock = 50;
    h.familyHappiness = 0.85f;
    h.isUnderGrief = false;

    KinshipMember headMem;
    headMem.entityId = headId;
    headMem.name = headName;
    headMem.role = KinshipRole::HeadOfHousehold;
    headMem.age = 32;
    headMem.affectionToHead = 100.0f;
    headMem.dailyWageContribution = 100;
    h.members.push_back(headMem);

    m_households[hid] = h;
    m_entityToHousehold[headId] = hid;

    // Ensure social profile exists
    sSocialEngine.GetOrCreateProfile(headId, headName, true);
    sSocialEngine.RecordEpisodicMemory(headId, "Founded " + h.householdName,
        "Established permanent domestic household residence at " + homeApartment + ".",
        MemoryCategory::DAILY_PLEASURE, 0.7f, 0.8f, 0, homeApartment, true);

    return hid;
}

bool NPCFamilyDreamsEngine::AddKinshipMember(uint32 householdId, uint32 memberId, const std::string& name, KinshipRole role, uint32 age)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_households.find(householdId);
    if (it == m_households.end()) return false;

    FamilyHousehold& h = it->second;
    if (h.FindMember(memberId) != nullptr) return false;

    KinshipMember mem;
    mem.entityId = memberId;
    mem.name = name;
    mem.role = role;
    mem.age = age;
    mem.affectionToHead = 85.0f;
    mem.dailyWageContribution = (role == KinshipRole::Child) ? 0 : 75;
    h.members.push_back(mem);

    m_entityToHousehold[memberId] = householdId;

    // Social graph link: form close/best-friend bond with household head and members
    sSocialEngine.GetOrCreateProfile(memberId, name, true);
    for (const auto& existing : h.members) {
        if (existing.entityId != memberId) {
            FriendshipTier tier = (role == KinshipRole::Spouse) ? FriendshipTier::BestFriendConfidant : FriendshipTier::CloseFriend;
            sSocialEngine.FormFriendship(memberId, existing.entityId, tier);
        }
    }

    sSocialEngine.RecordEpisodicMemory(memberId, "Joined " + h.householdName,
        "Became official member of " + h.householdName + " at " + h.homeApartmentName + ".",
        MemoryCategory::FRIENDSHIP_BOND, 0.8f, 0.85f, h.headEntityId, h.homeApartmentName, true);

    return true;
}

bool NPCFamilyDreamsEngine::FormHouseholdFromRomance(uint32 partnerA, uint32 partnerB, const std::string& nameA, const std::string& nameB,
                                                    const std::string& homeAddress, LocationVector loc)
{
    std::string familyName = "The " + nameA + " & " + nameB + " Family";
    uint32 hid = CreateHousehold(partnerA, nameA, familyName, homeAddress, loc);
    bool added = AddKinshipMember(hid, partnerB, nameB, KinshipRole::Spouse, 30);
    
    if (added) {
        sSocialEngine.DeepenCommitment(partnerA, partnerB);
        sSocialEngine.RecordEpisodicMemory(partnerA, "Setting Up Home with " + nameB,
            "Moved in together to build a shared domestic life at " + homeAddress + ".",
            MemoryCategory::ROMANCE_PROPOSAL, 0.95f, 1.0f, partnerB, homeAddress, true);
        sSocialEngine.RecordEpisodicMemory(partnerB, "Setting Up Home with " + nameA,
            "Moved in together to build a shared domestic life at " + homeAddress + ".",
            MemoryCategory::ROMANCE_PROPOSAL, 0.95f, 1.0f, partnerA, homeAddress, true);
    }
    return added;
}

FamilyHousehold* NPCFamilyDreamsEngine::GetHousehold(uint32 householdId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_households.find(householdId);
    return it != m_households.end() ? &it->second : nullptr;
}

const FamilyHousehold* NPCFamilyDreamsEngine::GetHousehold(uint32 householdId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_households.find(householdId);
    return it != m_households.end() ? &it->second : nullptr;
}

FamilyHousehold* NPCFamilyDreamsEngine::GetHouseholdByMember(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto itH = m_entityToHousehold.find(entityId);
    if (itH == m_entityToHousehold.end()) return nullptr;
    auto it = m_households.find(itH->second);
    return it != m_households.end() ? &it->second : nullptr;
}

const FamilyHousehold* NPCFamilyDreamsEngine::GetHouseholdByMember(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto itH = m_entityToHousehold.find(entityId);
    if (itH == m_entityToHousehold.end()) return nullptr;
    auto it = m_households.find(itH->second);
    return it != m_households.end() ? &it->second : nullptr;
}

// ============================================================================
// Household Circadian Dynamics
// ============================================================================

bool NPCFamilyDreamsEngine::ProcessHouseholdMorningBreakfast(uint32 householdId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_households.find(householdId);
    if (it == m_households.end()) return false;

    FamilyHousehold& h = it->second;
    size_t memberCount = h.members.size();
    if (memberCount == 0) return false;

    // Consume pantry meals
    if (h.pantryStock >= memberCount) {
        h.pantryStock -= static_cast<uint32>(memberCount);
        h.familyHappiness = std::min(1.0f, h.familyHappiness + 0.05f);
    } else {
        h.pantryStock = 0;
        h.familyHappiness = std::max(0.2f, h.familyHappiness - 0.05f);
    }

    // Refresh affection and record pleasant daily memory
    for (auto& mem : h.members) {
        mem.affectionToHead = std::min(100.0f, mem.affectionToHead + 1.0f);
        sSocialEngine.RecordEpisodicMemory(mem.entityId, "Morning Family Breakfast",
            "Shared hot coffee, pancakes, and quiet morning laughter with household at " + h.homeApartmentName + ".",
            MemoryCategory::DAILY_PLEASURE, 0.5f, 0.4f, h.headEntityId, h.homeApartmentName, false);
    }
    return true;
}

bool NPCFamilyDreamsEngine::ProcessHouseholdEveningDinner(uint32 householdId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_households.find(householdId);
    if (it == m_households.end()) return false;

    FamilyHousehold& h = it->second;
    size_t memberCount = h.members.size();
    if (memberCount == 0) return false;

    if (h.pantryStock >= memberCount) {
        h.pantryStock -= static_cast<uint32>(memberCount);
        h.familyHappiness = std::min(1.0f, h.familyHappiness + 0.04f);
    }

    // Relieve burnout for working members
    for (auto& mem : h.members) {
        MitigateBurnout(mem.entityId, 0.15f);
    }
    return true;
}

bool NPCFamilyDreamsEngine::DepositHouseholdSavings(uint32 householdId, uint32 memberId, uint32 amount)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_households.find(householdId);
    if (it == m_households.end()) return false;

    FamilyHousehold& h = it->second;
    h.householdSavingsInfoCredits += amount;
    KinshipMember* m = h.FindMemberMut(memberId);
    if (m) {
        m->dailyWageContribution += amount;
    }
    return true;
}

bool NPCFamilyDreamsEngine::TriggerFamilyGriefOrVengeance(uint32 victimId, const std::string& assailantName, const std::string& incidentDetails)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto itH = m_entityToHousehold.find(victimId);
    if (itH == m_entityToHousehold.end()) return false;

    auto it = m_households.find(itH->second);
    if (it == m_households.end()) return false;

    FamilyHousehold& h = it->second;
    const KinshipMember* victim = h.FindMember(victimId);
    std::string vName = victim ? victim->name : ("Member#" + std::to_string(victimId));

    h.isUnderGrief = true;
    h.griefVictimName = vName;
    h.swornVengeanceTarget = assailantName;
    h.familyHappiness = std::max(0.1f, h.familyHappiness - 0.50f);

    // Record grief / trauma core memories across all surviving relatives
    for (auto& mem : h.members) {
        if (mem.entityId != victimId) {
            sSocialEngine.RecordEpisodicMemory(mem.entityId, "Tragedy in the Family: " + vName,
                "Overwhelmed by grief and shock after " + vName + " was attacked by " + assailantName + ": " + incidentDetails,
                MemoryCategory::TRAUMA_VIOLENCE, -0.95f, 1.0f, victimId, h.homeApartmentName, true);
        }
    }
    return true;
}

// ============================================================================
// Life Dreams & Aspirations
// ============================================================================

void NPCFamilyDreamsEngine::AssignDream(uint32 entityId, LifeDreamType dreamType, const std::string& customTitle)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    ActiveAspiration asp;
    asp.aspirationId = m_nextAspirationId++;
    asp.dreamType = dreamType;
    asp.progress = 0.05f;
    asp.isFulfilled = false;
    asp.isInCrisis = false;

    switch (dreamType) {
        case LifeDreamType::BuyRichlandHighRise:
            asp.title = customTitle.empty() ? "Purchase Richland Penthouse" : customTitle;
            asp.description = "Accumulate 5,000 Info Credits to acquire a luxury high-rise condominium in Richland.";
            asp.currentMilestone = "Save initial 1,000 Info Credits in household reserve.";
            break;
        case LifeDreamType::BecomeMetacortexVP:
            asp.title = customTitle.empty() ? "Rise to Metacortex Vice President" : customTitle;
            asp.description = "Climb corporate hierarchy from entry programmer to executive suite.";
            asp.currentMilestone = "Attain Senior Architect position with 90%+ performance rating.";
            break;
        case LifeDreamType::RaiseFlourishingFamily:
            asp.title = customTitle.empty() ? "Build a Thriving Family Dynasty" : customTitle;
            asp.description = "Nurture children, maintain marital harmony, and secure generational wealth.";
            asp.currentMilestone = "Expand household savings to 3,000 Info Credits and keep pantry stocked.";
            break;
        case LifeDreamType::OpenArtisanBakery:
            asp.title = customTitle.empty() ? "Open Independent Bakery & Cafe" : customTitle;
            asp.description = "Escape corporate grind and serve fresh croissants and espresso to Megacity commuters.";
            asp.currentMilestone = "Secure commercial retail permit and lease bakery storefront.";
            break;
        case LifeDreamType::ClearHouseholdDebt:
            asp.title = customTitle.empty() ? "Eradicate Predatory Corporate Debt" : customTitle;
            asp.description = "Pay off crushing loan sharks and reclaim financial sovereignty.";
            asp.currentMilestone = "Pay down high-interest tranche of 800 Info Credits.";
            break;
        case LifeDreamType::LiberatePodSibling:
            asp.title = customTitle.empty() ? "Liberate Trapped Pod Sibling" : customTitle;
            asp.description = "Locate and free biological twin from Power Plant Pod Sector 77.";
            asp.currentMilestone = "Decrypt sub-level neural bypass codes to Power Plant Matrix.";
            break;
        case LifeDreamType::CaptainHovercraft:
            asp.title = customTitle.empty() ? "Command a Zion Hovercraft" : customTitle;
            asp.description = "Graduate from field operator to Captain of a frontline combat vessel.";
            asp.currentMilestone = "Complete 20 combat extraction missions with zero crew casualties.";
            break;
        case LifeDreamType::MasterHyperJump:
            asp.title = customTitle.empty() ? "Master Rooftop Hyper-Jump Code" : customTitle;
            asp.description = "Bypass physical simulation engine limits to execute impossible skyscraper leaps.";
            asp.currentMilestone = "Achieve inner focus quotient above 90% during high-altitude training.";
            break;
        case LifeDreamType::SystemicZeroDefect:
            asp.title = customTitle.empty() ? "Systemic Anomaly Zero-Defect" : customTitle;
            asp.description = "Purge 100 anomalous intrusions to achieve absolute algorithmic stability.";
            asp.currentMilestone = "Execute clean deletion of 10 redpill signal transceivers.";
            break;
        case LifeDreamType::StudyHumanLove:
            asp.title = customTitle.empty() ? "Reconcile Human Love Without Crashing" : customTitle;
            asp.description = "Understand the mathematical contradiction of human self-sacrificial love.";
            asp.currentMilestone = "Analyze 50 civilian family interactions without CPU thread divergence.";
            break;
        case LifeDreamType::GainMerovingianFavor:
            asp.title = customTitle.empty() ? "Win the Merovingian's Patronage" : customTitle;
            asp.description = "Deliver forbidden source code fragment to secure sanctuary in Club Chateau.";
            asp.currentMilestone = "Procure an uncompiled system key from an Agent courier.";
            break;
        case LifeDreamType::RiseToUnderboss:
            asp.title = customTitle.empty() ? "Ascend to Syndicate Underboss" : customTitle;
            asp.description = "Seize control of 3 territory rackets and earn the respect of the Capo.";
            asp.currentMilestone = "Consolidate extortion revenue from Slums commercial corridor.";
            break;
        case LifeDreamType::SurviveFrankCastle:
            asp.title = customTitle.empty() ? "Survive the Vigilante's Crosshairs" : customTitle;
            asp.description = "Evade Frank Castle's tactical hit list and retire with laundered millions.";
            asp.currentMilestone = "Fortify safehouse with automated turret perimeter and biometric alarms.";
            break;
        case LifeDreamType::MakeDetectiveLieutenant:
            asp.title = customTitle.empty() ? "Attain Detective Lieutenant Rank" : customTitle;
            asp.description = "Close 10 high-profile homicide and syndicate extortion cases in Westview.";
            asp.currentMilestone = "Apprehend chief lieutenant of Morrell counterfeiting ring.";
            break;
        case LifeDreamType::BustHarborCartel:
            asp.title = customTitle.empty() ? "Dismantle Crooked Harbor IA Ring" : customTitle;
            asp.description = "Expose corrupt internal affairs captains and seize smuggled munitions.";
            asp.currentMilestone = "Intercept contraband freight manifest at Pier 44 Terminal.";
            break;
    }

    m_aspirations[entityId] = asp;
}

ActiveAspiration* NPCFamilyDreamsEngine::GetAspiration(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_aspirations.find(entityId);
    return it != m_aspirations.end() ? &it->second : nullptr;
}

const ActiveAspiration* NPCFamilyDreamsEngine::GetAspiration(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_aspirations.find(entityId);
    return it != m_aspirations.end() ? &it->second : nullptr;
}

bool NPCFamilyDreamsEngine::AdvanceAspirationMilestone(uint32 entityId, float progressDelta, const std::string& milestoneName)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_aspirations.find(entityId);
    if (it == m_aspirations.end()) return false;

    ActiveAspiration& asp = it->second;
    asp.progress = std::clamp(asp.progress + progressDelta, 0.0f, 1.0f);
    if (!milestoneName.empty()) {
        asp.milestonesCompleted.push_back(milestoneName);
        asp.currentMilestone = "Next Chapter of Ambition";
    }

    if (asp.progress >= 1.0f && !asp.isFulfilled) {
        asp.isFulfilled = true;
        asp.fulfillmentNarrative = "Achieved lifelong dream: '" + asp.title + "'! A triumphant life legacy secured.";
        sSocialEngine.RecordEpisodicMemory(entityId, "Triumphant Dream Fulfilled: " + asp.title,
            asp.fulfillmentNarrative, MemoryCategory::WORKPLACE_ACHIEVEMENT, 1.0f, 1.0f, 0, "Megacity Skyline", true);
    }
    return true;
}

bool NPCFamilyDreamsEngine::FulfillAspiration(uint32 entityId)
{
    return AdvanceAspirationMilestone(entityId, 1.0f, "Lifelong Magnum Opus Achieved");
}

bool NPCFamilyDreamsEngine::TriggerAspirationCrisis(uint32 entityId, const std::string& crisisReason)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_aspirations.find(entityId);
    if (it == m_aspirations.end()) return false;

    ActiveAspiration& asp = it->second;
    asp.isInCrisis = true;
    asp.progress = std::max(0.0f, asp.progress - 0.20f);
    sSocialEngine.RecordEpisodicMemory(entityId, "Crisis of Purpose: " + asp.title,
        "Ambition stalled due to devastating setback: " + crisisReason,
        MemoryCategory::TRAUMA_VIOLENCE, -0.7f, 0.8f, 0, "Downtown Street", false);
    return true;
}

// ============================================================================
// Career Ladders & Workplace Performance
// ============================================================================

void NPCFamilyDreamsEngine::AssignCareer(uint32 entityId, CareerTrack track, JobPositionLevel level, const std::string& title,
                                        const std::string& workplace, uint32 wpId, uint32 wage)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    CareerRecord rec;
    rec.track = track;
    rec.level = level;
    rec.jobTitle = title;
    rec.workplaceName = workplace;
    rec.workplaceId = wpId;
    rec.hourlyWage = wage;
    rec.performanceScore = 0.5f;
    rec.burnoutIndex = 0.1f;
    rec.dailyTasksCompleted = 0;
    rec.totalPromotions = static_cast<uint32>(level);
    rec.isOvertime = false;
    rec.isUnemployed = false;

    m_careers[entityId] = rec;
}

CareerRecord* NPCFamilyDreamsEngine::GetCareer(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    return it != m_careers.end() ? &it->second : nullptr;
}

const CareerRecord* NPCFamilyDreamsEngine::GetCareer(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    return it != m_careers.end() ? &it->second : nullptr;
}

bool NPCFamilyDreamsEngine::AdvanceWorkShiftPerformance(uint32 entityId, float performanceDelta, bool overtime)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    if (it == m_careers.end() || it->second.isUnemployed) return false;

    CareerRecord& rec = it->second;
    rec.performanceScore = std::clamp(rec.performanceScore + performanceDelta, 0.0f, 1.0f);
    rec.dailyTasksCompleted++;
    rec.isOvertime = overtime;

    if (overtime) {
        rec.burnoutIndex = std::min(1.0f, rec.burnoutIndex + 0.10f);
        rec.performanceScore = std::min(1.0f, rec.performanceScore + 0.05f); // Overtime bonus
    }

    return true;
}

bool NPCFamilyDreamsEngine::PromoteCitizenCareer(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    if (it == m_careers.end() || it->second.isUnemployed) return false;

    CareerRecord& rec = it->second;
    uint8_t currentLvl = static_cast<uint8_t>(rec.level);
    if (currentLvl >= 5) return false; // Already at pinnacle Level 5

    rec.level = static_cast<JobPositionLevel>(currentLvl + 1);
    rec.hourlyWage = static_cast<uint32>(rec.hourlyWage * 1.35f); // 35% raise
    rec.totalPromotions++;
    rec.performanceScore = 0.5f; // Fresh baseline at higher rank

    std::string newRankName = GetJobLevelName(rec.level);
    sSocialEngine.RecordEpisodicMemory(entityId, "Promoted: " + newRankName,
        "Promoted to " + newRankName + " at " + rec.workplaceName + " with higher wage of " + std::to_string(rec.hourlyWage) + " Credits/hr.",
        MemoryCategory::WORKPLACE_ACHIEVEMENT, 0.9f, 0.9f, 0, rec.workplaceName, true);

    // If active dream relates to corporate rank, advance dream!
    auto itA = m_aspirations.find(entityId);
    if (itA != m_aspirations.end() && itA->second.dreamType == LifeDreamType::BecomeMetacortexVP) {
        itA->second.progress = std::min(1.0f, itA->second.progress + 0.20f);
        itA->second.milestonesCompleted.push_back("Promoted to " + newRankName);
    }
    return true;
}

bool NPCFamilyDreamsEngine::DemoteOrTerminateCitizen(uint32 entityId, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    if (it == m_careers.end()) return false;

    CareerRecord& rec = it->second;
    uint8_t currentLvl = static_cast<uint8_t>(rec.level);
    if (currentLvl > 0) {
        rec.level = static_cast<JobPositionLevel>(currentLvl - 1);
        rec.hourlyWage = static_cast<uint32>(rec.hourlyWage * 0.75f);
        rec.performanceScore = 0.3f;
    } else {
        rec.isUnemployed = true;
        rec.hourlyWage = 0;
    }

    sSocialEngine.RecordEpisodicMemory(entityId, "Career Demotion: " + rec.jobTitle,
        "Faced career setback at " + rec.workplaceName + ": " + reason,
        MemoryCategory::WORKPLACE_VENTING, -0.8f, 0.85f, 0, rec.workplaceName, true);

    return true;
}

void NPCFamilyDreamsEngine::MitigateBurnout(uint32 entityId, float amount)
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    if (it != m_careers.end()) {
        it->second.burnoutIndex = std::max(0.0f, it->second.burnoutIndex - amount);
    }
}

// ============================================================================
// Formatting & Summaries
// ============================================================================

std::string NPCFamilyDreamsEngine::GenerateFamilySummary(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto itH = m_entityToHousehold.find(entityId);
    if (itH == m_entityToHousehold.end()) return "Household: Unattached / Single Occupant";

    auto it = m_households.find(itH->second);
    if (it == m_households.end()) return "Household: Record Missing";

    const FamilyHousehold& h = it->second;
    std::ostringstream ss;
    ss << "Household: " << h.householdName << " (" << h.homeApartmentName << ")\n";
    ss << "   Savings: " << h.householdSavingsInfoCredits << " Credits | Pantry Meals: " << h.pantryStock
       << " | Happiness: " << std::fixed << std::setprecision(1) << (h.familyHappiness * 100.0f) << "%\n";
    if (h.isUnderGrief) {
        ss << "   {c:FF0000}[GRIEF ALERT] Mourning " << h.griefVictimName << " | Sworn Vengeance: " << h.swornVengeanceTarget << "{/c}\n";
    }
    ss << "   Kinship Members (" << h.members.size() << "):\n";
    for (const auto& m : h.members) {
        ss << "     - " << m.name << " [" << GetKinshipRoleName(m.role) << "] Age " << m.age
           << " (Affection: " << static_cast<uint32>(m.affectionToHead) << "%)\n";
    }
    return ss.str();
}

std::string NPCFamilyDreamsEngine::GenerateDreamsSummary(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_aspirations.find(entityId);
    if (it == m_aspirations.end()) return "Aspiration: No lifelong ambition registered.";

    const ActiveAspiration& asp = it->second;
    std::ostringstream ss;
    ss << "Lifelong Ambition: " << asp.title << "\n";
    ss << "   Description: " << asp.description << "\n";
    uint32 pInt = static_cast<uint32>(asp.progress * 100.0f);
    ss << "   Progress: [";
    for (uint32 b = 0; b < 10; ++b) {
        ss << ((b * 10 < pInt) ? "=" : "-");
    }
    ss << "] " << pInt << "% (" << (asp.isFulfilled ? "FULFILLED" : (asp.isInCrisis ? "IN CRISIS" : "IN PURSUIT")) << ")\n";
    ss << "   Current Milestone: " << asp.currentMilestone << "\n";
    if (!asp.milestonesCompleted.empty()) {
        ss << "   Completed Milestones: " << asp.milestonesCompleted.size() << "\n";
        for (const auto& ms : asp.milestonesCompleted) {
            ss << "     * " << ms << "\n";
        }
    }
    return ss.str();
}

std::string NPCFamilyDreamsEngine::GenerateCareerSummary(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    auto it = m_careers.find(entityId);
    if (it == m_careers.end()) return "Career: No corporate or vocational employment on record.";

    const CareerRecord& rec = it->second;
    std::ostringstream ss;
    ss << "Career: " << rec.jobTitle << " (" << GetJobLevelName(rec.level) << ")\n";
    ss << "   Employer: " << rec.workplaceName << " | Track: " << GetCareerTrackName(rec.track) << "\n";
    ss << "   Hourly Wage: " << rec.hourlyWage << " Credits/hr | Status: " << (rec.isUnemployed ? "Unemployed" : (rec.isOvertime ? "Working Overtime" : "Active Shift")) << "\n";
    ss << "   Performance: " << static_cast<uint32>(rec.performanceScore * 100.0f) << "% | Burnout Index: " << static_cast<uint32>(rec.burnoutIndex * 100.0f) << "% | Promotions: " << rec.totalPromotions << "\n";
    return ss.str();
}

std::string NPCFamilyDreamsEngine::GenerateCompleteLifeDossier(uint32 entityId) const
{
    std::ostringstream ss;
    ss << "================================================================================\n";
    ss << " MEGACITY NPC COMPLETE LIFE & VOCATION DOSSIER (Entity #" << entityId << ")\n";
    ss << "================================================================================\n";
    ss << GenerateFamilySummary(entityId) << "\n";
    ss << GenerateCareerSummary(entityId) << "\n";
    ss << GenerateDreamsSummary(entityId) << "\n";
    ss << sSocialEngine.GenerateEntitySocialSummary(entityId) << "\n";
    ss << sSocialEngine.GenerateEntityMemoriesReport(entityId);
    ss << "================================================================================\n";
    return ss.str();
}

std::string NPCFamilyDreamsEngine::GenerateEngineMasterTelemetryReport() const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    std::ostringstream ss;
    ss << "============================================================\n";
    ss << " MEGACITY NPC FAMILY, DREAMS & CAREERS ENGINE TELEMETRY\n";
    ss << "============================================================\n";
    ss << " Total Registered Households   : " << m_households.size() << "\n";
    ss << " Active Lifelong Aspirations   : " << m_aspirations.size() << "\n";
    ss << " Fulfilled Life Dreams         : " << GetFulfilledDreamsCount() << "\n";
    ss << " Total Employed Careers        : " << m_careers.size() << "\n";
    ss << " Total Kinship Entities Mapped : " << m_entityToHousehold.size() << "\n";
    ss << "============================================================\n";
    return ss.str();
}

size_t NPCFamilyDreamsEngine::GetTotalHouseholdsCount() const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    return m_households.size();
}

size_t NPCFamilyDreamsEngine::GetTotalAspirationsCount() const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    return m_aspirations.size();
}

size_t NPCFamilyDreamsEngine::GetTotalCareersCount() const
{
    std::lock_guard<std::mutex> lock(m_familyMutex);
    return m_careers.size();
}

size_t NPCFamilyDreamsEngine::GetFulfilledDreamsCount() const
{
    size_t count = 0;
    for (const auto& kvp : m_aspirations) {
        if (kvp.second.isFulfilled) count++;
    }
    return count;
}

// ============================================================================
// Automated C++ Test Suite
// ============================================================================

void RunNPCFamilyDreamsTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING NPC FAMILY, LIFE DREAMS & CAREERS TEST SUITE      " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    NPCFamilyDreamsEngine& engine = sFamilyDreamsEngine;
    engine.Reset();
    sSocialEngine.Reset();

    int passed = 0;
    int failed = 0;

    auto AssertTest = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failed++;
        }
    };

    // 1. Household Creation & Kinship Roles
    uint32 hid = engine.CreateHousehold(101, "Thomas Anderson", "The Anderson Household", "Metacortex Executive Suites #404", LocationVector(9844.0, 1295.0, 1314.0));
    AssertTest(hid == 101, "Household Created with ID 101");
    AssertTest(engine.GetTotalHouseholdsCount() == 1, "Total Households Count == 1");

    bool addedSpouse = engine.AddKinshipMember(hid, 102, "Trinity Anderson", KinshipRole::Spouse, 30);
    bool addedChild = engine.AddKinshipMember(hid, 103, "Neo Junior", KinshipRole::Child, 6);
    AssertTest(addedSpouse, "Spouse Added to Household");
    AssertTest(addedChild, "Child Added to Household");

    const auto* h = engine.GetHousehold(hid);
    AssertTest(h != nullptr && h->members.size() == 3, "Household Contains Exactly 3 Members");
    AssertTest(engine.GetHouseholdByMember(102) == h, "Household Found By Member ID 102");

    // 2. Household Morning Breakfast & Pantry
    uint32 initialPantry = h->pantryStock;
    bool bfast = engine.ProcessHouseholdMorningBreakfast(hid);
    AssertTest(bfast, "Household Breakfast Processed");
    AssertTest(h->pantryStock == initialPantry - 3, "Pantry Meals Consumed by 3 Members");
    AssertTest(h->familyHappiness >= 0.85f, "Family Happiness Boosted by Shared Breakfast");

    // 3. Shared Household Savings
    uint32 initialSavings = h->householdSavingsInfoCredits;
    engine.DepositHouseholdSavings(hid, 101, 250);
    AssertTest(h->householdSavingsInfoCredits == initialSavings + 250, "Deposited 250 Credits into Household Savings");

    // 4. Family Tragedy & Kinship Sworn Vengeance
    bool griefTriggered = engine.TriggerFamilyGriefOrVengeance(103, "Agent Smith", "Fatal code injection anomaly in Slums alley");
    AssertTest(griefTriggered, "Family Grief & Vengeance Triggered");
    AssertTest(h->isUnderGrief, "Household Marked Under Grief");
    AssertTest(h->swornVengeanceTarget == "Agent Smith", "Sworn Vengeance Target Set to 'Agent Smith'");
    AssertTest(h->familyHappiness < 0.5f, "Family Happiness Dropped from Trauma");

    // 5. Romance Interlock: Forming Household from Marriage
    sSocialEngine.GetOrCreateProfile(201, "Devin Vance", true);
    sSocialEngine.GetOrCreateProfile(202, "Elena Rostova", true);
    sSocialEngine.ScheduleAndExecuteDate(201, 202, "Downtown Bistro", "Dinner");
    sSocialEngine.DeepenCommitment(201, 202); // Dating -> InLove
    sSocialEngine.DeepenCommitment(201, 202); // InLove -> CommittedPartner
    sSocialEngine.DeepenCommitment(201, 202); // CommittedPartner -> Married

    bool marriedHousehold = engine.FormHouseholdFromRomance(201, 202, "Devin Vance", "Elena Rostova", "Creston Tenements #2B", LocationVector(-30434.0, 95.0, 20325.0));
    AssertTest(marriedHousehold, "Household Formed from Romantic Marriage");
    AssertTest(engine.GetTotalHouseholdsCount() == 2, "Total Households Count == 2");

    // 6. Career Assignment & Promotion Ladder
    engine.AssignCareer(101, CareerTrack::CorporateTech, JobPositionLevel::Level0_EntryIntern, "Software Intern", "Metacortex Core", 1, 35);
    const auto* c1 = engine.GetCareer(101);
    AssertTest(c1 != nullptr && c1->hourlyWage == 35, "Career Assigned: Software Intern at 35 Credits/hr");

    bool workShift = engine.AdvanceWorkShiftPerformance(101, 0.45f, false);
    AssertTest(workShift && c1->performanceScore >= 0.90f, "Work Shift Advanced Performance >= 90%");

    bool promoted = engine.PromoteCitizenCareer(101);
    AssertTest(promoted, "Citizen Promoted to Level 1: Junior Associate");
    AssertTest(c1->level == JobPositionLevel::Level1_JuniorAssociate, "Rank is Level 1");
    AssertTest(c1->hourlyWage > 35, "Hourly Wage Increased After Promotion");

    // 7. Overtime Work & Burnout Dynamics
    engine.AdvanceWorkShiftPerformance(101, 0.1f, true); // Overtime
    AssertTest(c1->isOvertime, "Overtime Shift Flagged");
    AssertTest(c1->burnoutIndex > 0.1f, "Burnout Index Increased by Overtime Shift");

    engine.MitigateBurnout(101, 0.15f);
    AssertTest(c1->burnoutIndex <= 0.1f, "Burnout Mitigated via Evening Rest");

    // 8. Demotion & Career Setbacks
    bool demoted = engine.DemoteOrTerminateCitizen(101, "Failed to submit critical audit patch on deadline");
    AssertTest(demoted, "Career Demotion Processed");
    AssertTest(c1->level == JobPositionLevel::Level0_EntryIntern, "Demoted back to Level 0");

    // 9. Life Dreams & Aspirations Tracking
    engine.AssignDream(101, LifeDreamType::BecomeMetacortexVP);
    const auto* asp = engine.GetAspiration(101);
    AssertTest(asp != nullptr, "Aspiration Initialized");
    AssertTest(asp->dreamType == LifeDreamType::BecomeMetacortexVP, "Dream Type: BecomeMetacortexVP");

    bool msAdvanced = engine.AdvanceAspirationMilestone(101, 0.35f, "Published proprietary algorithmic sorting routine");
    AssertTest(msAdvanced && asp->progress >= 0.40f, "Milestone Advanced Progress >= 40%");

    // 10. Triumphant Dream Fulfillment
    bool fulfilled = engine.FulfillAspiration(101);
    AssertTest(fulfilled && asp->isFulfilled, "Lifelong Dream Successfully Fulfilled");
    AssertTest(asp->progress >= 1.0f, "Aspiration Progress Reached 100%");

    // 11. Crisis of Purpose
    engine.AssignDream(201, LifeDreamType::BuyRichlandHighRise);
    bool crisis = engine.TriggerAspirationCrisis(201, "Stock market crash depleted personal savings");
    AssertTest(crisis, "Aspiration Crisis Triggered");
    AssertTest(engine.GetAspiration(201)->isInCrisis, "Aspiration Marked In Crisis");

    // 12. Redpill Found Family Hovercraft Crew
    uint32 shipHid = engine.CreateHousehold(501, "Captain Morpheus", "Crew of the Nebuchadnezzar", "Zion Docking Bay 7", LocationVector(0.0, 0.0, 0.0));
    engine.AddKinshipMember(shipHid, 502, "Operator Tank", KinshipRole::FoundFamilyCrewmate, 28);
    engine.AddKinshipMember(shipHid, 503, "Gunner Dozer", KinshipRole::FoundFamilyCrewmate, 31);
    const auto* shipH = engine.GetHousehold(shipHid);
    AssertTest(shipH != nullptr && shipH->members.size() == 3, "Hovercraft Found Family Created with 3 Crewmates");

    // 13. Reporting & Telemetry
    std::string famSummary = engine.GenerateFamilySummary(101);
    AssertTest(!famSummary.empty() && famSummary.find("The Anderson Household") != std::string::npos, "Family Summary Generated");

    std::string careerSummary = engine.GenerateCareerSummary(101);
    AssertTest(!careerSummary.empty() && careerSummary.find("Metacortex Core") != std::string::npos, "Career Summary Generated");

    std::string dreamSummary = engine.GenerateDreamsSummary(101);
    AssertTest(!dreamSummary.empty() && dreamSummary.find("Lifelong Ambition") != std::string::npos, "Dreams Summary Generated");

    std::string lifeDossier = engine.GenerateCompleteLifeDossier(101);
    AssertTest(!lifeDossier.empty() && lifeDossier.find("COMPLETE LIFE") != std::string::npos, "Complete Life Dossier Generated");

    std::string telem = engine.GenerateEngineMasterTelemetryReport();
    AssertTest(!telem.empty() && telem.find("FAMILY, DREAMS & CAREERS") != std::string::npos, "Engine Telemetry Report Generated");

    // 14. Scale Test: 1,000 Households, 2,000 Dreams, 2,000 Careers
    for (uint32 i = 1000; i < 2000; ++i) {
        uint32 hNew = engine.CreateHousehold(i, "Citizen-" + std::to_string(i), "Household-" + std::to_string(i), "Apartment-" + std::to_string(i), LocationVector(0.0, 0.0, 0.0));
        engine.AddKinshipMember(hNew, i + 10000, "Spouse-" + std::to_string(i), KinshipRole::Spouse, 28);
        engine.AssignCareer(i, CareerTrack::CorporateTech, JobPositionLevel::Level1_JuniorAssociate, "Analyst", "Metacortex", 1, 45);
        engine.AssignDream(i, static_cast<LifeDreamType>(i % 15));
    }
    AssertTest(engine.GetTotalHouseholdsCount() >= 1000, "Scale Test: Initialized >= 1000 Living Households");
    AssertTest(engine.GetTotalCareersCount() >= 1000, "Scale Test: Initialized >= 1000 Active Careers");
    AssertTest(engine.GetTotalAspirationsCount() >= 1000, "Scale Test: Initialized >= 1000 Life Aspirations");

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  FAMILY, DREAMS & CAREERS TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    assert(failed == 0);
}

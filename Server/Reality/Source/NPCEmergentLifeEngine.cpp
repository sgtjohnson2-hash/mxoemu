#include "NPCEmergentLifeEngine.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameServer.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <cmath>

static inline bool Has3DWorldSupport() {
    return GameServer::getSingletonPtr() != nullptr && BotManager::getSingletonPtr() != nullptr;
}

// Singleton instantiation
createFileSingleton(NPCEmergentLifeEngine);

NPCEmergentLifeEngine::NPCEmergentLifeEngine()
{
    Initialize();
}

void NPCEmergentLifeEngine::Initialize()
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    if (m_initialized) return;

    // Seed default social circles
    SocialCircle circle1;
    circle1.circleId = m_nextCircleId++;
    circle1.circleName = "Park East Coffee Regulars";
    circle1.circleType = "CoffeeRegulars";
    circle1.meetingVenue = "Le Bistro de Merovingian / Park East Cafe";
    circle1.pooledTreasuryBits = 150.0;
    circle1.currentObjective = "Morning discourse and market trends";
    m_socialCircles[circle1.circleId] = circle1;

    SocialCircle circle2;
    circle2.circleId = m_nextCircleId++;
    circle2.circleName = "Slums Tenants Union";
    circle2.circleType = "TenantsUnion";
    circle2.meetingVenue = "Slums Tenement Basement 3";
    circle2.pooledTreasuryBits = 420.0;
    circle2.currentObjective = "Mutual defense against corrupt landlord evictions";
    m_socialCircles[circle2.circleId] = circle2;

    SocialCircle circle3;
    circle3.circleId = m_nextCircleId++;
    circle3.circleName = "The Midnight Salon of the Real";
    circle3.circleType = "PhilosophySalon";
    circle3.meetingVenue = "Abandoned Subway Concourse 7";
    circle3.pooledTreasuryBits = 300.0;
    circle3.currentObjective = "Deciphering green code trails and memory discrepancies";
    m_socialCircles[circle3.circleId] = circle3;

    // Seed initial Megacity rumors
    GossipRumor r1;
    r1.rumorId = m_nextRumorId++;
    r1.topic = RumorTopicType::FRANK_CASTLE_SIGHTING;
    r1.headline = "The Punisher sighted hunting Maroni racketeers near Docks";
    r1.narrativeDetails = "A lone vigilante clad in a skull vest breached a smuggling warehouse in Harbor West.";
    r1.credibilityScore = 0.95f;
    r1.timesSpreadCount = 8;
    r1.creationTimestamp = 100;
    m_rumors[r1.rumorId] = r1;

    GossipRumor r2;
    r2.rumorId = m_nextRumorId++;
    r2.topic = RumorTopicType::MATRIX_CODE_GLITCH;
    r2.headline = "Pedestrians report seeing twin black cats cross alley twice in Westview";
    r2.narrativeDetails = "A severe deja vu ripple cascaded through the 4th Precinct, leaving citizens disoriented.";
    r2.credibilityScore = 0.80f;
    r2.timesSpreadCount = 14;
    r2.creationTimestamp = 120;
    m_rumors[r2.rumorId] = r2;

    m_initialized = true;
}

void NPCEmergentLifeEngine::Reset()
{
    {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_currentActivities.clear();
        m_milestones.clear();
        m_entityMilestones.clear();
        m_awakeningProfiles.clear();
        m_socialCircles.clear();
        m_rumors.clear();
        m_entityKnownRumors.clear();
        m_nextMilestoneId = 1001;
        m_nextCircleId = 201;
        m_nextRumorId = 3001;
        m_accumulatedTimeMs = 0;
        m_initialized = false;
    }
    Initialize();
}

void NPCEmergentLifeEngine::Update(uint32 deltaMs)
{
    m_accumulatedTimeMs += deltaMs;
    if (m_accumulatedTimeMs >= 20000) {
        m_accumulatedTimeMs = 0;
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& pair : m_rumors) {
            if (pair.second.credibilityScore > 0.1f) {
                pair.second.credibilityScore -= 0.01f;
            }
        }
    }
}

EmergentActivityRecord NPCEmergentLifeEngine::GetActivityDefinition(EmergentActivityType type) const
{
    EmergentActivityRecord rec;
    rec.type = type;

    switch (type) {
        case EmergentActivityType::VISIT_ESPRESSO_BAR:
            rec.activityName = "Visiting Espresso Bar";
            rec.venueLocationName = "Park East Cafe";
            rec.durationMinutes = 45;
            rec.stressReliefScore = 20.0f;
            rec.socialAffinityBonus = 15.0f;
            rec.cognitiveImpact = 0.05f;
            break;
        case EmergentActivityType::ROOFTOP_MEDITATION_GAZING:
            rec.activityName = "Rooftop Stargazing & Meditation";
            rec.venueLocationName = "Richland Tower Roof";
            rec.durationMinutes = 60;
            rec.stressReliefScore = 30.0f;
            rec.socialAffinityBonus = 5.0f;
            rec.cognitiveImpact = 0.20f;
            break;
        case EmergentActivityType::UNDERGROUND_FIGHT_CLUB:
            rec.activityName = "Underground Fight Club Sparring";
            rec.venueLocationName = "Slums Basement Dojo";
            rec.durationMinutes = 90;
            rec.stressReliefScore = 25.0f;
            rec.socialAffinityBonus = 20.0f;
            rec.cognitiveImpact = 0.15f;
            break;
        case EmergentActivityType::NEON_NIGHTCLUB_DANCING:
            rec.activityName = "Dancing at Neon Nightclub";
            rec.venueLocationName = "Club Hel / Matrix Discotheque";
            rec.durationMinutes = 120;
            rec.stressReliefScore = 35.0f;
            rec.socialAffinityBonus = 25.0f;
            rec.cognitiveImpact = 0.10f;
            break;
        case EmergentActivityType::GYM_MARTIAL_ARTS_TRAINING:
            rec.activityName = "Kung Fu & Physical Training";
            rec.venueLocationName = "Downtown Martial Arts Academy";
            rec.durationMinutes = 75;
            rec.stressReliefScore = 25.0f;
            rec.socialAffinityBonus = 15.0f;
            rec.cognitiveImpact = 0.10f;
            break;
        case EmergentActivityType::LOCAL_MARKET_SHOPPING:
            rec.activityName = "Shopping at Artisan Market";
            rec.venueLocationName = "International District Plaza";
            rec.durationMinutes = 50;
            rec.stressReliefScore = 15.0f;
            rec.socialAffinityBonus = 10.0f;
            rec.cognitiveImpact = 0.0f;
            break;
        case EmergentActivityType::LIBRARY_ANOMALY_RESEARCH:
            rec.activityName = "Investigating Historical Archives";
            rec.venueLocationName = "Westview Public Library";
            rec.durationMinutes = 80;
            rec.stressReliefScore = 10.0f;
            rec.socialAffinityBonus = 5.0f;
            rec.cognitiveImpact = 0.40f;
            break;
        case EmergentActivityType::ZION_SYMPATHIZER_RALLY:
            rec.activityName = "Attending Secret Dissident Rally";
            rec.venueLocationName = "Subway Concourse 7";
            rec.durationMinutes = 90;
            rec.stressReliefScore = 10.0f;
            rec.socialAffinityBonus = 30.0f;
            rec.cognitiveImpact = 0.50f;
            break;
        case EmergentActivityType::CHURCH_OF_THE_MACHINE:
            rec.activityName = "Cathedral of Logic Procession";
            rec.venueLocationName = "Cathedral of the Machine";
            rec.durationMinutes = 60;
            rec.stressReliefScore = 20.0f;
            rec.socialAffinityBonus = 15.0f;
            rec.cognitiveImpact = -0.30f; // Solidifies bluepill comfort
            break;
        case EmergentActivityType::COMMUNITY_VOLUNTEERING:
            rec.activityName = "Volunteering at Slums Aid Kitchen";
            rec.venueLocationName = "Slums Community Hearth";
            rec.durationMinutes = 100;
            rec.stressReliefScore = 25.0f;
            rec.socialAffinityBonus = 35.0f;
            rec.cognitiveImpact = 0.10f;
            break;
    }
    return rec;
}

EmergentActivityType NPCEmergentLifeEngine::RecommendActivityForPersonality(float openness, float conscientiousness,
                                                                             float extraversion, float agreeableness, float neuroticism) const
{
    if (extraversion >= 0.70f && conscientiousness < 0.55f) {
        return EmergentActivityType::NEON_NIGHTCLUB_DANCING;
    }
    if (openness >= 0.75f && neuroticism >= 0.50f) {
        return EmergentActivityType::ROOFTOP_MEDITATION_GAZING;
    }
    if (openness >= 0.80f) {
        return EmergentActivityType::LIBRARY_ANOMALY_RESEARCH;
    }
    if (agreeableness >= 0.70f && conscientiousness >= 0.60f) {
        return EmergentActivityType::COMMUNITY_VOLUNTEERING;
    }
    if (conscientiousness >= 0.80f && openness <= 0.40f) {
        return EmergentActivityType::CHURCH_OF_THE_MACHINE;
    }
    if (neuroticism < 0.40f && conscientiousness >= 0.60f) {
        return EmergentActivityType::GYM_MARTIAL_ARTS_TRAINING;
    }
    if (extraversion >= 0.50f && agreeableness >= 0.50f) {
        return EmergentActivityType::VISIT_ESPRESSO_BAR;
    }
    return EmergentActivityType::LOCAL_MARKET_SHOPPING;
}

bool NPCEmergentLifeEngine::ScheduleCitizenActivity(uint32 entityId, EmergentActivityType activity)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    m_currentActivities[entityId] = activity;
    return true;
}

bool NPCEmergentLifeEngine::ExecuteCurrentActivity(uint32 entityId, float& outStressRelief, float& outHappinessDelta)
{
    EmergentActivityType act = EmergentActivityType::VISIT_ESPRESSO_BAR;
    {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        auto it = m_currentActivities.find(entityId);
        if (it != m_currentActivities.end()) {
            act = it->second;
        }
    }

    EmergentActivityRecord def = GetActivityDefinition(act);
    outStressRelief = def.stressReliefScore;
    outHappinessDelta = def.socialAffinityBonus * 0.5f;

    // Apply cognitive impact to awakening profile if any
    if (std::abs(def.cognitiveImpact) > 0.001f) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        auto apIt = m_awakeningProfiles.find(entityId);
        if (apIt != m_awakeningProfiles.end()) {
            apIt->second.cognitiveDissonance = std::clamp(apIt->second.cognitiveDissonance + def.cognitiveImpact * 0.1f, 0.0f, 1.0f);
        }
    }

    return true;
}

EmergentActivityType NPCEmergentLifeEngine::GetCitizenCurrentActivity(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_currentActivities.find(entityId);
    if (it != m_currentActivities.end()) return it->second;
    return EmergentActivityType::VISIT_ESPRESSO_BAR;
}

// ============================================================================
// Life Milestones Implementation
// ============================================================================

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerWeddingCeremony(uint32 spouseA, uint32 spouseB, const std::string& venueName)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    LifeMilestoneEvent ev;
    ev.milestoneId = m_nextMilestoneId++;
    ev.type = LifeMilestoneType::WEDDING_CEREMONY;
    ev.primaryEntityId = spouseA;
    ev.secondaryEntityId = spouseB;
    ev.venueName = venueName.empty() ? "Park East Chapel" : venueName;
    ev.financialImpact = -500.0; // Wedding ceremony costs
    ev.happinessImpact = 45.0f;

    std::ostringstream oss;
    oss << "Joined in holy matrimony with Entity#" << spouseB << " at " << ev.venueName << ". Surrounded by family, friends, and shared tears of joy.";
    ev.narrativeProse = oss.str();

    m_milestones[ev.milestoneId] = ev;
    m_entityMilestones[spouseA].push_back(ev.milestoneId);
    m_entityMilestones[spouseB].push_back(ev.milestoneId);

    if (Has3DWorldSupport()) {
        auto poA = sObjMgr.getGOPtrSafe(spouseA);
        auto poB = sObjMgr.getGOPtrSafe(spouseB);
        if (poA && poB) {
            poA->Emote(20);
            poB->Emote(20);
            poA->sayChat("I take thee to have and to hold, beyond all simulated worlds!");
            poB->sayChat("I do! In this life and in the real!");
        }
    }

    return ev;
}

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerChildBirthOrAdoption(uint32 headEntityId, const std::string& childName, bool isAdoption)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    LifeMilestoneEvent ev;
    ev.milestoneId = m_nextMilestoneId++;
    ev.type = LifeMilestoneType::CHILD_BIRTH_OR_ADOPTION;
    ev.primaryEntityId = headEntityId;
    ev.happinessImpact = 40.0f;
    ev.financialImpact = -300.0; // Nursery setup

    std::ostringstream oss;
    if (isAdoption) {
        oss << "Welcomed adopted child '" << childName << "' into the family household, pledging eternal protection and guidance.";
    } else {
        oss << "Celebrated the miraculous birth of newborn child '" << childName << "' into the family household.";
    }
    ev.narrativeProse = oss.str();
    ev.venueName = "Metro General Maternity Wing";

    m_milestones[ev.milestoneId] = ev;
    m_entityMilestones[headEntityId].push_back(ev.milestoneId);

    return ev;
}

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerWeddingAnniversary(uint32 spouseA, uint32 spouseB, uint32 yearsMarried)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    LifeMilestoneEvent ev;
    ev.milestoneId = m_nextMilestoneId++;
    ev.type = LifeMilestoneType::WEDDING_ANNIVERSARY;
    ev.primaryEntityId = spouseA;
    ev.secondaryEntityId = spouseB;
    ev.happinessImpact = 25.0f;
    ev.financialImpact = -150.0;
    ev.venueName = "Le Bistro de Merovingian";

    std::ostringstream oss;
    oss << "Celebrated " << yearsMarried << " years of enduring marriage and devotion with partner Entity#" << spouseB << " over candlelit dinner.";
    ev.narrativeProse = oss.str();

    m_milestones[ev.milestoneId] = ev;
    m_entityMilestones[spouseA].push_back(ev.milestoneId);
    m_entityMilestones[spouseB].push_back(ev.milestoneId);

    return ev;
}

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerMemorialService(uint32 mourningEntityId, const std::string& fallenKinName, const std::string& causeOfDeath)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    LifeMilestoneEvent ev;
    ev.milestoneId = m_nextMilestoneId++;
    ev.type = LifeMilestoneType::MEMORIAL_SERVICE;
    ev.primaryEntityId = mourningEntityId;
    ev.happinessImpact = -40.0f;
    ev.financialImpact = -250.0;
    ev.venueName = "Morpheus Memorial Garden";

    std::ostringstream oss;
    oss << "Held a solemn memorial service for beloved fallen kin '" << fallenKinName << "' (Cause: " << causeOfDeath << "). Pledged eternal remembrance and justice.";
    ev.narrativeProse = oss.str();

    m_milestones[ev.milestoneId] = ev;
    m_entityMilestones[mourningEntityId].push_back(ev.milestoneId);

    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(mourningEntityId)) {
            po->Emote(50); // Kneel / mourn
            po->sayChat("Rest in peace, " + fallenKinName + ". You will never be forgotten.");
        }
    }

    return ev;
}

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerEconomicWindfall(uint32 entityId, double windfallBits, const std::string& sourceDescription)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    LifeMilestoneEvent ev;
    ev.milestoneId = m_nextMilestoneId++;
    ev.type = LifeMilestoneType::ECONOMIC_WINDFALL;
    ev.primaryEntityId = entityId;
    ev.financialImpact = windfallBits;
    ev.happinessImpact = 35.0f;
    ev.venueName = "Metacortex Commercial Bank";

    std::ostringstream oss;
    oss << "Received tremendous financial windfall of " << std::fixed << std::setprecision(1) << windfallBits
        << " bits via " << sourceDescription << ". Family household upgraded to luxury high-rise!";
    ev.narrativeProse = oss.str();

    m_milestones[ev.milestoneId] = ev;
    m_entityMilestones[entityId].push_back(ev.milestoneId);

    return ev;
}

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerEvictionDownsizing(uint32 entityId, const std::string& landlordReason)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    LifeMilestoneEvent ev;
    ev.milestoneId = m_nextMilestoneId++;
    ev.type = LifeMilestoneType::EVICTION_DOWNSIZING;
    ev.primaryEntityId = entityId;
    ev.financialImpact = -450.0;
    ev.happinessImpact = -35.0f;
    ev.venueName = "Slums Tenement Corridor";

    std::ostringstream oss;
    oss << "Evicted from primary residence due to: '" << landlordReason << "'. Downsized household into Slums tenement under severe distress.";
    ev.narrativeProse = oss.str();

    m_milestones[ev.milestoneId] = ev;
    m_entityMilestones[entityId].push_back(ev.milestoneId);

    return ev;
}

LifeMilestoneEvent NPCEmergentLifeEngine::TriggerCognitiveDissonanceGlitch(uint32 entityId, const std::string& glitchDescription, float severity)
{
    LifeMilestoneEvent ev;
    {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        ev.milestoneId = m_nextMilestoneId++;
        ev.type = LifeMilestoneType::COGNITIVE_DISSONANCE_GLITCH;
        ev.primaryEntityId = entityId;
        ev.happinessImpact = -15.0f * severity;
        ev.financialImpact = 0.0;
        ev.venueName = "Downtown Street Corner";

        std::ostringstream oss;
        oss << "Directly witnessed impossible simulation anomaly: '" << glitchDescription << "'. Reality matrix momentarily flickered green code.";
        ev.narrativeProse = oss.str();

        m_milestones[ev.milestoneId] = ev;
        m_entityMilestones[entityId].push_back(ev.milestoneId);
    }

    // Process awakening reaction
    ProcessWitnessedAnomaly(entityId, glitchDescription, severity);

    return ev;
}

// ============================================================================
// Awakening & Ideological Drift Implementation
// ============================================================================

AwakeningProfile& NPCEmergentLifeEngine::GetOrCreateAwakeningProfile(uint32 entityId, const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_awakeningProfiles.find(entityId);
    if (it != m_awakeningProfiles.end()) {
        if (!name.empty() && it->second.citizenName.empty()) {
            it->second.citizenName = name;
        }
        return it->second;
    }

    AwakeningProfile ap;
    ap.entityId = entityId;
    ap.citizenName = name.empty() ? ("Citizen#" + std::to_string(entityId)) : name;
    ap.stage = AwakeningStage::STAGE_0_BLUEPILL_SLEEPER;
    ap.cognitiveDissonance = 0.05f;
    ap.anomaliesWitnessedCount = 0;
    ap.redpillTrust = 5.0f;
    ap.machinePurityTrust = 95.0f;
    ap.hasHardlineContact = false;
    ap.latestAwakeningEpiphany = "Trusting in daily order and peaceful routine.";

    m_awakeningProfiles[entityId] = ap;
    return m_awakeningProfiles[entityId];
}

bool NPCEmergentLifeEngine::ProcessWitnessedAnomaly(uint32 entityId, const std::string& anomalyType, float anomalyIntensity)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_awakeningProfiles.find(entityId);
    if (it == m_awakeningProfiles.end()) {
        AwakeningProfile ap;
        ap.entityId = entityId;
        ap.citizenName = "Citizen#" + std::to_string(entityId);
        ap.stage = AwakeningStage::STAGE_0_BLUEPILL_SLEEPER;
        ap.cognitiveDissonance = 0.05f;
        ap.anomaliesWitnessedCount = 0;
        ap.redpillTrust = 5.0f;
        ap.machinePurityTrust = 95.0f;
        ap.hasHardlineContact = false;
        ap.latestAwakeningEpiphany = "Trusting in daily order and peaceful routine.";
        m_awakeningProfiles[entityId] = ap;
        it = m_awakeningProfiles.find(entityId);
    }

    it->second.anomaliesWitnessedCount++;
    float dissonanceGain = std::max(anomalyIntensity * 0.45f, (anomalyIntensity >= 0.7f ? 0.40f : 0.15f));
    it->second.cognitiveDissonance = std::clamp(it->second.cognitiveDissonance + dissonanceGain, 0.0f, 1.0f);
    it->second.machinePurityTrust = std::clamp(it->second.machinePurityTrust - (anomalyIntensity * 20.0f), 0.0f, 100.0f);
    it->second.redpillTrust = std::clamp(it->second.redpillTrust + (anomalyIntensity * 15.0f), 0.0f, 100.0f);

    // Auto-advance thresholds
    if (it->second.stage == AwakeningStage::STAGE_0_BLUEPILL_SLEEPER && it->second.cognitiveDissonance >= 0.40f) {
        it->second.stage = AwakeningStage::STAGE_1_MATRIX_SKEPTIC;
        it->second.latestAwakeningEpiphany = "Noticed repeated green code ripples behind subway tile walls; reality feels fabricated.";
    } else if (it->second.stage == AwakeningStage::STAGE_1_MATRIX_SKEPTIC && it->second.cognitiveDissonance >= 0.70f && it->second.anomaliesWitnessedCount >= 2) {
        it->second.stage = AwakeningStage::STAGE_2_AWAKENING_SEARCHER;
        it->second.latestAwakeningEpiphany = "Actively searching for phone booths and whispers of Zion operatives.";
    }

    if (Has3DWorldSupport()) {
        if (auto po = sObjMgr.getGOPtrSafe(entityId)) {
            po->Emote(50); // Kneel / shock
            po->sayChat("The code... the numbers are cascading... this entire world is an illusion!");
        }
    }

    return true;
}

bool NPCEmergentLifeEngine::AdvanceAwakeningStage(uint32 entityId, AwakeningStage newStage, const std::string& epiphanyProse)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_awakeningProfiles.find(entityId);
    if (it == m_awakeningProfiles.end()) return false;

    it->second.stage = newStage;
    it->second.latestAwakeningEpiphany = epiphanyProse;

    if (newStage == AwakeningStage::STAGE_3_REDPILL_SYMPATHIZER || newStage == AwakeningStage::STAGE_4_FREED_MIND_OPERATIVE) {
        it->second.hasHardlineContact = true;
        it->second.redpillTrust = std::max(it->second.redpillTrust, 75.0f);
        it->second.machinePurityTrust = std::min(it->second.machinePurityTrust, 15.0f);
    }
    return true;
}

bool NPCEmergentLifeEngine::TriggerCypheriteDisillusionment(uint32 entityId, const std::string& grievanceReason)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_awakeningProfiles.find(entityId);
    if (it == m_awakeningProfiles.end()) return false;

    it->second.stage = AwakeningStage::STAGE_CYPHERITE_RENEGADE;
    it->second.redpillTrust = 0.0f;
    it->second.machinePurityTrust = 95.0f;
    it->second.latestAwakeningEpiphany = "Grievance: '" + grievanceReason + "'. Demands bluepill reinsertion into comfortable ignorance.";
    return true;
}

AwakeningStage NPCEmergentLifeEngine::GetAwakeningStage(uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_awakeningProfiles.find(entityId);
    if (it != m_awakeningProfiles.end()) return it->second.stage;
    return AwakeningStage::STAGE_0_BLUEPILL_SLEEPER;
}

// ============================================================================
// Social Circles & Communities Implementation
// ============================================================================

uint32 NPCEmergentLifeEngine::CreateSocialCircle(const std::string& name, const std::string& type, const std::string& venue, uint32 founderId)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    SocialCircle circle;
    circle.circleId = m_nextCircleId++;
    circle.circleName = name;
    circle.circleType = type;
    circle.meetingVenue = venue;
    circle.pooledTreasuryBits = 50.0;
    circle.lastMeetingTimestamp = 100;
    circle.currentObjective = "Mutual community support and fellowship";
    if (founderId != 0) {
        circle.memberIds.push_back(founderId);
    }

    m_socialCircles[circle.circleId] = circle;
    return circle.circleId;
}

bool NPCEmergentLifeEngine::AddCitizenToCircle(uint32 circleId, uint32 entityId)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_socialCircles.find(circleId);
    if (it == m_socialCircles.end()) return false;

    if (std::find(it->second.memberIds.begin(), it->second.memberIds.end(), entityId) == it->second.memberIds.end()) {
        it->second.memberIds.push_back(entityId);
        it->second.pooledTreasuryBits += 25.0; // Entry contribution
    }
    return true;
}

bool NPCEmergentLifeEngine::ConductCircleMeeting(uint32 circleId, const std::string& agendaDiscussion)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_socialCircles.find(circleId);
    if (it == m_socialCircles.end()) return false;

    it->second.lastMeetingTimestamp += 120;
    it->second.currentObjective = agendaDiscussion;
    it->second.pooledTreasuryBits += it->second.memberIds.size() * 10.0;

    return true;
}

std::vector<SocialCircle> NPCEmergentLifeEngine::GetCitizenSocialCircles(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    std::vector<SocialCircle> res;
    for (const auto& kvp : m_socialCircles) {
        if (std::find(kvp.second.memberIds.begin(), kvp.second.memberIds.end(), entityId) != kvp.second.memberIds.end()) {
            res.push_back(kvp.second);
        }
    }
    return res;
}

const SocialCircle* NPCEmergentLifeEngine::GetSocialCircle(uint32 circleId) const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_socialCircles.find(circleId);
    if (it != m_socialCircles.end()) return &it->second;
    return nullptr;
}

// ============================================================================
// Word-of-Mouth Gossip Diffusion Implementation
// ============================================================================

uint32 NPCEmergentLifeEngine::SeedRumor(RumorTopicType topic, const std::string& headline, const std::string& details, uint32 originEntityId, float credibility)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    GossipRumor r;
    r.rumorId = m_nextRumorId++;
    r.topic = topic;
    r.headline = headline;
    r.narrativeDetails = details;
    r.originEntityId = originEntityId;
    r.credibilityScore = std::clamp(credibility, 0.1f, 1.0f);
    r.timesSpreadCount = 1;
    r.creationTimestamp = 200;

    m_rumors[r.rumorId] = r;
    if (originEntityId != 0) {
        m_entityKnownRumors[originEntityId].insert(r.rumorId);
    }
    return r.rumorId;
}

bool NPCEmergentLifeEngine::SpreadRumorBetweenEntities(uint32 tellerId, uint32 listenerId)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto tIt = m_entityKnownRumors.find(tellerId);
    if (tIt == m_entityKnownRumors.end() || tIt->second.empty()) return false;

    // Pick first rumor teller knows that listener does not
    auto& lSet = m_entityKnownRumors[listenerId];
    for (uint32 rId : tIt->second) {
        if (lSet.find(rId) == lSet.end()) {
            lSet.insert(rId);
            auto rIt = m_rumors.find(rId);
            if (rIt != m_rumors.end()) {
                rIt->second.timesSpreadCount++;
            }
            return true;
        }
    }
    return false;
}

std::vector<GossipRumor> NPCEmergentLifeEngine::GetEntityKnownRumors(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    std::vector<GossipRumor> res;
    auto it = m_entityKnownRumors.find(entityId);
    if (it != m_entityKnownRumors.end()) {
        for (uint32 rId : it->second) {
            auto rIt = m_rumors.find(rId);
            if (rIt != m_rumors.end()) {
                res.push_back(rIt->second);
            }
        }
    }
    return res;
}

std::vector<GossipRumor> NPCEmergentLifeEngine::GetTrendingRumors(size_t limit) const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    std::vector<GossipRumor> sortedRumors;
    for (const auto& kvp : m_rumors) {
        sortedRumors.push_back(kvp.second);
    }
    std::sort(sortedRumors.begin(), sortedRumors.end(), [](const GossipRumor& a, const GossipRumor& b) {
        return a.timesSpreadCount > b.timesSpreadCount;
    });

    if (sortedRumors.size() > limit) {
        sortedRumors.resize(limit);
    }
    return sortedRumors;
}

// ============================================================================
// Reporting & Telemetry Implementation
// ============================================================================

std::string NPCEmergentLifeEngine::GenerateEmergentLifeSummary(uint32 entityId) const
{
    std::ostringstream oss;
    EmergentActivityType act = GetCitizenCurrentActivity(entityId);
    EmergentActivityRecord actDef = GetActivityDefinition(act);

    oss << "=== EMERGENT LIFE: CITIZEN #" << entityId << " ===\n";
    oss << "Current Leisure Activity: " << actDef.activityName << " (" << actDef.venueLocationName << ")\n";

    // Awakening summary
    {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        auto it = m_awakeningProfiles.find(entityId);
        if (it != m_awakeningProfiles.end()) {
            oss << "Matrix Awakening Stage: " << GetAwakeningStageName(it->second.stage)
                << " (Dissonance: " << std::fixed << std::setprecision(1) << (it->second.cognitiveDissonance * 100.0f) << "%)\n";
            oss << "Epiphany: " << it->second.latestAwakeningEpiphany << "\n";
        } else {
            oss << "Matrix Awakening Stage: Bluepill Sleeper\n";
        }

        // Milestones
        auto mIt = m_entityMilestones.find(entityId);
        if (mIt != m_entityMilestones.end() && !mIt->second.empty()) {
            oss << "Recorded Life Milestones (" << mIt->second.size() << "):\n";
            for (uint32 mId : mIt->second) {
                auto mileIt = m_milestones.find(mId);
                if (mileIt != m_milestones.end()) {
                    oss << " - [" << GetMilestoneTypeName(mileIt->second.type) << "]: " << mileIt->second.narrativeProse << "\n";
                }
            }
        }
    }

    return oss.str();
}

std::string NPCEmergentLifeEngine::GenerateAwakeningReport(uint32 entityId) const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    auto it = m_awakeningProfiles.find(entityId);
    if (it == m_awakeningProfiles.end()) {
        return "Citizen#" + std::to_string(entityId) + " is a standard Bluepill Sleeper.";
    }

    std::ostringstream oss;
    oss << "AWAKENING DOSSIER: " << it->second.citizenName << "\n";
    oss << "Stage: " << GetAwakeningStageName(it->second.stage) << "\n";
    oss << "Cognitive Dissonance: " << std::fixed << std::setprecision(1) << (it->second.cognitiveDissonance * 100.0f) << "%\n";
    oss << "Anomalies Witnessed: " << it->second.anomaliesWitnessedCount << "\n";
    oss << "Redpill Alignment: " << it->second.redpillTrust << " | Machine Faith: " << it->second.machinePurityTrust << "\n";
    oss << "Hardline Contact: " << (it->second.hasHardlineContact ? "ACTIVE" : "NONE") << "\n";
    oss << "Epiphany: " << it->second.latestAwakeningEpiphany;
    return oss.str();
}

std::string NPCEmergentLifeEngine::GenerateGossipNetworkReport() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    std::ostringstream oss;
    oss << "=== MEGACITY WORD-OF-MOUTH GOSSIP NETWORK ===\n";
    oss << "Total Active Rumors: " << m_rumors.size() << "\n";
    for (const auto& kvp : m_rumors) {
        oss << "[" << GetRumorTopicName(kvp.second.topic) << "] '" << kvp.second.headline
            << "' (Spread: " << kvp.second.timesSpreadCount << "x, Cred: "
            << std::fixed << std::setprecision(2) << kvp.second.credibilityScore << ")\n";
    }
    return oss.str();
}

std::string NPCEmergentLifeEngine::GenerateSocialCirclesReport() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    std::ostringstream oss;
    oss << "=== MEGACITY SOCIAL CIRCLES & COMMUNITIES ===\n";
    oss << "Total Circles: " << m_socialCircles.size() << "\n";
    for (const auto& kvp : m_socialCircles) {
        oss << "Circle #" << kvp.second.circleId << " [" << kvp.second.circleName << "]\n";
        oss << "  Type: " << kvp.second.circleType << " | Venue: " << kvp.second.meetingVenue << "\n";
        oss << "  Members: " << kvp.second.memberIds.size() << " | Treasury: " << kvp.second.pooledTreasuryBits << " bits\n";
        oss << "  Objective: " << kvp.second.currentObjective << "\n";
    }
    return oss.str();
}

std::string NPCEmergentLifeEngine::GenerateMasterTelemetryReport() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    size_t awakenedCount = 0;
    for (const auto& kvp : m_awakeningProfiles) {
        if (kvp.second.stage != AwakeningStage::STAGE_0_BLUEPILL_SLEEPER) {
            awakenedCount++;
        }
    }

    std::ostringstream oss;
    oss << "MEGACITY NPC EMERGENT LIFE ENGINE\n";
    oss << "  Total Awakening Profiles : " << m_awakeningProfiles.size() << "\n";
    oss << "  Awakened / Skeptics      : " << awakenedCount << "\n";
    oss << "  Recorded Life Milestones : " << m_milestones.size() << "\n";
    oss << "  Social Circles Active    : " << m_socialCircles.size() << "\n";
    oss << "  Active Gossip Rumors     : " << m_rumors.size();
    return oss.str();
}

size_t NPCEmergentLifeEngine::GetTotalAwakenedCount() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    size_t count = 0;
    for (const auto& kvp : m_awakeningProfiles) {
        if (kvp.second.stage != AwakeningStage::STAGE_0_BLUEPILL_SLEEPER) {
            count++;
        }
    }
    return count;
}

size_t NPCEmergentLifeEngine::GetTotalMilestonesCount() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    return m_milestones.size();
}

size_t NPCEmergentLifeEngine::GetTotalSocialCirclesCount() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    return m_socialCircles.size();
}

size_t NPCEmergentLifeEngine::GetTotalRumorsCount() const
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    return m_rumors.size();
}

// ============================================================================
// Static Name Helpers
// ============================================================================

std::string NPCEmergentLifeEngine::GetActivityTypeName(EmergentActivityType type)
{
    switch (type) {
        case EmergentActivityType::VISIT_ESPRESSO_BAR: return "Visit Espresso Bar";
        case EmergentActivityType::ROOFTOP_MEDITATION_GAZING: return "Rooftop Meditation";
        case EmergentActivityType::UNDERGROUND_FIGHT_CLUB: return "Underground Fight Club";
        case EmergentActivityType::NEON_NIGHTCLUB_DANCING: return "Neon Nightclub Dancing";
        case EmergentActivityType::GYM_MARTIAL_ARTS_TRAINING: return "Martial Arts Gym";
        case EmergentActivityType::LOCAL_MARKET_SHOPPING: return "Artisan Market Shopping";
        case EmergentActivityType::LIBRARY_ANOMALY_RESEARCH: return "Anomaly Archives Research";
        case EmergentActivityType::ZION_SYMPATHIZER_RALLY: return "Zion Dissident Rally";
        case EmergentActivityType::CHURCH_OF_THE_MACHINE: return "Church of the Machine";
        case EmergentActivityType::COMMUNITY_VOLUNTEERING: return "Community Volunteering";
    }
    return "Unknown Activity";
}

std::string NPCEmergentLifeEngine::GetMilestoneTypeName(LifeMilestoneType type)
{
    switch (type) {
        case LifeMilestoneType::WEDDING_CEREMONY: return "Wedding Ceremony";
        case LifeMilestoneType::CHILD_BIRTH_OR_ADOPTION: return "Child Birth / Adoption";
        case LifeMilestoneType::WEDDING_ANNIVERSARY: return "Wedding Anniversary";
        case LifeMilestoneType::MEMORIAL_SERVICE: return "Memorial Service";
        case LifeMilestoneType::ECONOMIC_WINDFALL: return "Economic Windfall";
        case LifeMilestoneType::EVICTION_DOWNSIZING: return "Eviction Downsizing";
        case LifeMilestoneType::COGNITIVE_DISSONANCE_GLITCH: return "Cognitive Dissonance Glitch";
    }
    return "Unknown Milestone";
}

std::string NPCEmergentLifeEngine::GetAwakeningStageName(AwakeningStage stage)
{
    switch (stage) {
        case AwakeningStage::STAGE_0_BLUEPILL_SLEEPER: return "Stage 0: Bluepill Sleeper";
        case AwakeningStage::STAGE_1_MATRIX_SKEPTIC: return "Stage 1: Matrix Skeptic";
        case AwakeningStage::STAGE_2_AWAKENING_SEARCHER: return "Stage 2: Awakening Searcher";
        case AwakeningStage::STAGE_3_REDPILL_SYMPATHIZER: return "Stage 3: Redpill Sympathizer";
        case AwakeningStage::STAGE_4_FREED_MIND_OPERATIVE: return "Stage 4: Freed Mind Operative";
        case AwakeningStage::STAGE_CYPHERITE_RENEGADE: return "Stage Cypherite: Traumatized Renegade";
    }
    return "Unknown Awakening Stage";
}

std::string NPCEmergentLifeEngine::GetRumorTopicName(RumorTopicType topic)
{
    switch (topic) {
        case RumorTopicType::FRANK_CASTLE_SIGHTING: return "Frank Castle Sighting";
        case RumorTopicType::AGENT_SMITH_INFECTION: return "Agent Smith Infection";
        case RumorTopicType::POLICE_SWAT_CHECKPOINT: return "Police SWAT Checkpoint";
        case RumorTopicType::CORPORATE_SCANDAL: return "Corporate Scandal";
        case RumorTopicType::ROMANCE_SCANDAL: return "Romance Scandal";
        case RumorTopicType::MATRIX_CODE_GLITCH: return "Matrix Code Glitch";
    }
    return "Unknown Topic";
}

// ============================================================================
// Standalone C++ Automated Test Suite
// ============================================================================

void RunNPCEmergentLifeTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING NPC EMERGENT LIFE & AWAKENING TEST SUITE          " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    NPCEmergentLifeEngine& engine = sEmergentLifeEngine;
    engine.Reset();

    uint32 passedCount = 0;
    uint32 failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const char* message) {
        if (condition) {
            std::cout << " [PASS] " << message << std::endl;
            passedCount++;
        } else {
            std::cerr << " [FAIL] " << message << std::endl;
            failedCount++;
        }
    };

    // 1. Activity Definitions & Attributes
    {
        EmergentActivityRecord cafe = engine.GetActivityDefinition(EmergentActivityType::VISIT_ESPRESSO_BAR);
        TEST_ASSERT(cafe.type == EmergentActivityType::VISIT_ESPRESSO_BAR, "Activity Type Matches");
        TEST_ASSERT(!cafe.activityName.empty(), "Activity Name Non-Empty");
        TEST_ASSERT(cafe.stressReliefScore > 0.0f, "Stress Relief Score Positive");

        EmergentActivityRecord rally = engine.GetActivityDefinition(EmergentActivityType::ZION_SYMPATHIZER_RALLY);
        TEST_ASSERT(rally.cognitiveImpact >= 0.40f, "Dissident Rally Has High Cognitive Awakening Impact");
    }

    // 2. Personality Recommendation Routing (OCEAN)
    {
        EmergentActivityType party = engine.RecommendActivityForPersonality(0.4f, 0.3f, 0.85f, 0.6f, 0.3f);
        TEST_ASSERT(party == EmergentActivityType::NEON_NIGHTCLUB_DANCING, "High Extraversion Routes to Nightclub");

        EmergentActivityType research = engine.RecommendActivityForPersonality(0.95f, 0.7f, 0.3f, 0.5f, 0.2f);
        TEST_ASSERT(research == EmergentActivityType::LIBRARY_ANOMALY_RESEARCH, "High Openness Routes to Anomaly Archives");

        EmergentActivityType volunteer = engine.RecommendActivityForPersonality(0.5f, 0.75f, 0.5f, 0.85f, 0.2f);
        TEST_ASSERT(volunteer == EmergentActivityType::COMMUNITY_VOLUNTEERING, "High Agreeableness + Conscientiousness Routes to Volunteering");
    }

    // 3. Activity Scheduling & Execution
    {
        engine.ScheduleCitizenActivity(101, EmergentActivityType::GYM_MARTIAL_ARTS_TRAINING);
        TEST_ASSERT(engine.GetCitizenCurrentActivity(101) == EmergentActivityType::GYM_MARTIAL_ARTS_TRAINING, "Citizen Activity Scheduled");

        float relief = 0.0f, happy = 0.0f;
        bool exec = engine.ExecuteCurrentActivity(101, relief, happy);
        TEST_ASSERT(exec, "Activity Executed Successfully");
        TEST_ASSERT(relief >= 20.0f, "Stress Relief Granted");
        TEST_ASSERT(happy > 0.0f, "Happiness Delta Positive");
    }

    // 4. Life Milestone: Wedding Ceremony
    {
        LifeMilestoneEvent wed = engine.TriggerWeddingCeremony(201, 202, "Park East Chapel");
        TEST_ASSERT(wed.type == LifeMilestoneType::WEDDING_CEREMONY, "Wedding Ceremony Milestone Created");
        TEST_ASSERT(wed.happinessImpact > 40.0f, "Wedding Grants Massive Happiness Boost");
        TEST_ASSERT(!wed.narrativeProse.empty(), "Wedding Narrative Prose Populated");
    }

    // 5. Life Milestone: Child Birth / Adoption
    {
        LifeMilestoneEvent birth = engine.TriggerChildBirthOrAdoption(301, "Aria Sterling", false);
        TEST_ASSERT(birth.type == LifeMilestoneType::CHILD_BIRTH_OR_ADOPTION, "Child Birth Milestone Created");
        TEST_ASSERT(birth.happinessImpact >= 35.0f, "Child Birth Happiness Boost");

        LifeMilestoneEvent adopt = engine.TriggerChildBirthOrAdoption(301, "Kenji Sterling", true);
        TEST_ASSERT(adopt.narrativeProse.find("adopted") != std::string::npos, "Adoption Prose Recorded");
    }

    // 6. Life Milestone: Memorial Service & Grief
    {
        LifeMilestoneEvent mem = engine.TriggerMemorialService(401, "Dario Corvo", "Slain in Syndicate Ambush");
        TEST_ASSERT(mem.type == LifeMilestoneType::MEMORIAL_SERVICE, "Memorial Service Milestone Created");
        TEST_ASSERT(mem.happinessImpact < 0.0f, "Memorial Service Reflects Grief Impact");
    }

    // 7. Economic Windfall & Residence Upgrade
    {
        LifeMilestoneEvent windfall = engine.TriggerEconomicWindfall(501, 3500.0, "Decrypted Rare Crypto-Code Contract");
        TEST_ASSERT(windfall.type == LifeMilestoneType::ECONOMIC_WINDFALL, "Economic Windfall Milestone Created");
        TEST_ASSERT(windfall.financialImpact == 3500.0, "Windfall Amount Registered");
    }

    // 8. Eviction & Downsizing Distress
    {
        LifeMilestoneEvent evict = engine.TriggerEvictionDownsizing(601, "Predatory Metacortex Rent Hike");
        TEST_ASSERT(evict.type == LifeMilestoneType::EVICTION_DOWNSIZING, "Eviction Milestone Created");
        TEST_ASSERT(evict.happinessImpact <= -30.0f, "Eviction Causes Severe Psychological Distress");
    }

    // 9. Matrix Glitch & Cognitive Dissonance
    {
        LifeMilestoneEvent glitch = engine.TriggerCognitiveDissonanceGlitch(701, "Street lamp vanished and reappeared in mid-air", 0.7f);
        TEST_ASSERT(glitch.type == LifeMilestoneType::COGNITIVE_DISSONANCE_GLITCH, "Glitch Milestone Created");

        AwakeningStage stage = engine.GetAwakeningStage(701);
        TEST_ASSERT(stage != AwakeningStage::STAGE_0_BLUEPILL_SLEEPER, "High Intensity Glitch Provoked Awakening Skepticism");
    }

    // 10. Awakening Progression (Sleeper -> Skeptic -> Searcher -> Sympathizer)
    {
        engine.GetOrCreateAwakeningProfile(801, "Thomas Anderson");
        TEST_ASSERT(engine.GetAwakeningStage(801) == AwakeningStage::STAGE_0_BLUEPILL_SLEEPER, "Initial State is Bluepill Sleeper");

        engine.ProcessWitnessedAnomaly(801, "Shadow moved independently of pedestrian", 0.8f);
        TEST_ASSERT(engine.GetAwakeningStage(801) == AwakeningStage::STAGE_1_MATRIX_SKEPTIC, "Advanced to Matrix Skeptic");

        engine.ProcessWitnessedAnomaly(801, "Witnessed Agent Smith bullet dodge in alley", 0.9f);
        TEST_ASSERT(engine.GetAwakeningStage(801) == AwakeningStage::STAGE_2_AWAKENING_SEARCHER, "Advanced to Awakening Searcher");

        engine.AdvanceAwakeningStage(801, AwakeningStage::STAGE_3_REDPILL_SYMPATHIZER, "Answered phone booth call from Trinity");
        TEST_ASSERT(engine.GetAwakeningStage(801) == AwakeningStage::STAGE_3_REDPILL_SYMPATHIZER, "Promoted to Redpill Sympathizer");
        TEST_ASSERT(engine.GetOrCreateAwakeningProfile(801).hasHardlineContact, "Hardline Contact Flagged");
    }

    // 11. Cypherite Renegade Disillusionment
    {
        engine.GetOrCreateAwakeningProfile(901, "Cypher Agent");
        bool cypher = engine.TriggerCypheriteDisillusionment(901, "Tired of eating cold nutrient gruel in the real world");
        TEST_ASSERT(cypher, "Cypherite Disillusionment Triggered");
        TEST_ASSERT(engine.GetAwakeningStage(901) == AwakeningStage::STAGE_CYPHERITE_RENEGADE, "Stage is Cypherite Renegade");
    }

    // 12. Social Circles & Word-of-Mouth Gossip Diffusion
    {
        uint32 circleId = engine.CreateSocialCircle("Dojo Sparring Cohort", "MartialArts", "Slums Dojo", 1001);
        TEST_ASSERT(circleId > 0, "Social Circle Created");

        bool joined = engine.AddCitizenToCircle(circleId, 1002);
        TEST_ASSERT(joined, "Member Added to Social Circle");

        bool meet = engine.ConductCircleMeeting(circleId, "Practicing dragon sweep combinations");
        TEST_ASSERT(meet, "Circle Meeting Conducted");

        // Gossip Rumors
        uint32 rId = engine.SeedRumor(RumorTopicType::FRANK_CASTLE_SIGHTING, "Castle Ambushed Slums Racket", "Punisher raided counterfeit lab", 1001, 0.9f);
        TEST_ASSERT(rId > 0, "Rumor Seeded");

        bool spread = engine.SpreadRumorBetweenEntities(1001, 1002);
        TEST_ASSERT(spread, "Rumor Spread by Word of Mouth to Listener");

        auto listenerRumors = engine.GetEntityKnownRumors(1002);
        TEST_ASSERT(!listenerRumors.empty(), "Listener Now Knows Rumor");

        auto trending = engine.GetTrendingRumors(5);
        TEST_ASSERT(!trending.empty(), "Trending Rumors Retrievable");
    }

    // Telemetry and Scale Check
    {
        std::string summary = engine.GenerateEmergentLifeSummary(801);
        TEST_ASSERT(!summary.empty(), "Emergent Life Summary Generated");

        std::string awakenRep = engine.GenerateAwakeningReport(801);
        TEST_ASSERT(!awakenRep.empty(), "Awakening Report Generated");

        std::string gossipRep = engine.GenerateGossipNetworkReport();
        TEST_ASSERT(!gossipRep.empty(), "Gossip Network Report Generated");

        std::string circleRep = engine.GenerateSocialCirclesReport();
        TEST_ASSERT(!circleRep.empty(), "Social Circles Report Generated");

        std::string telemetry = engine.GenerateMasterTelemetryReport();
        TEST_ASSERT(!telemetry.empty(), "Master Telemetry Report Generated");

        // Scale test: initialize 500 profiles
        for (uint32 id = 2000; id < 2500; ++id) {
            engine.GetOrCreateAwakeningProfile(id);
            engine.ScheduleCitizenActivity(id, EmergentActivityType::VISIT_ESPRESSO_BAR);
        }
        TEST_ASSERT(engine.GetTotalAwakenedCount() >= 1, "Scale Profiles Registered");
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  EMERGENT LIFE TEST RESULTS: " << passedCount << " PASSED, " << failedCount << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;
}

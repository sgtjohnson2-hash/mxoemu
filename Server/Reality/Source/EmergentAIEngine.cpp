#include "EmergentAIEngine.h"
#include "UnderworldManager.h"
#include "FrankCastleManager.h"
#include "EmergentPoliceManager.h"
#include "RadioDispatchSystem.h"
#include "Log.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cassert>

createFileSingleton(EmergentAIEngine);

// ============================================================================
// Phase 1: Smart Object Affordance Grid Implementation
// ============================================================================

int64_t SmartObjectAffordanceGrid::GetCellKey(float x, float z) const
{
    int32 cellX = static_cast<int32>(std::floor(x / GRID_CELL_SIZE));
    int32 cellZ = static_cast<int32>(std::floor(z / GRID_CELL_SIZE));
    uint64_t ux = static_cast<uint32_t>(cellX);
    uint64_t uz = static_cast<uint32_t>(cellZ);
    return static_cast<int64_t>((ux << 32) | uz);
}

void SmartObjectAffordanceGrid::RegisterObject(const SmartObject& obj)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(obj.objectId);
    if (it != m_objects.end()) {
        int64_t oldKey = GetCellKey(static_cast<float>(it->second.position.x), static_cast<float>(it->second.position.z));
        auto cellIt = m_spatialGrid.find(oldKey);
        if (cellIt != m_spatialGrid.end()) {
            cellIt->second.erase(std::remove(cellIt->second.begin(), cellIt->second.end(), obj.objectId), cellIt->second.end());
        }
    }
    m_objects[obj.objectId] = obj;
    int64_t key = GetCellKey(static_cast<float>(obj.position.x), static_cast<float>(obj.position.z));
    m_spatialGrid[key].push_back(obj.objectId);
}

bool SmartObjectAffordanceGrid::UnregisterObject(uint32 objectId)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(objectId);
    if (it == m_objects.end()) return false;

    int64_t key = GetCellKey(static_cast<float>(it->second.position.x), static_cast<float>(it->second.position.z));
    auto& cellVec = m_spatialGrid[key];
    cellVec.erase(std::remove(cellVec.begin(), cellVec.end(), objectId), cellVec.end());

    m_objects.erase(it);
    return true;
}

SmartObject* SmartObjectAffordanceGrid::GetObject(uint32 objectId)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(objectId);
    return (it != m_objects.end()) ? &it->second : nullptr;
}

std::vector<SmartObject*> SmartObjectAffordanceGrid::FindObjectsInRadius(const LocationVector& pos, float radius, AffordanceType affordanceType)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    std::vector<SmartObject*> results;
    float radSq = radius * radius;

    int32 minCellX = static_cast<int32>(std::floor((pos.x - radius) / GRID_CELL_SIZE));
    int32 maxCellX = static_cast<int32>(std::floor((pos.x + radius) / GRID_CELL_SIZE));
    int32 minCellZ = static_cast<int32>(std::floor((pos.z - radius) / GRID_CELL_SIZE));
    int32 maxCellZ = static_cast<int32>(std::floor((pos.z + radius) / GRID_CELL_SIZE));

    for (int32 cx = minCellX; cx <= maxCellX; ++cx) {
        for (int32 cz = minCellZ; cz <= maxCellZ; ++cz) {
            uint64_t ux = static_cast<uint32_t>(cx);
            uint64_t uz = static_cast<uint32_t>(cz);
            int64_t key = static_cast<int64_t>((ux << 32) | uz);
            auto cellIt = m_spatialGrid.find(key);
            if (cellIt == m_spatialGrid.end()) continue;

            for (uint32 objId : cellIt->second) {
                auto objIt = m_objects.find(objId);
                if (objIt == m_objects.end()) continue;

                SmartObject& candidate = objIt->second;
                if (!candidate.HasAffordance(affordanceType)) continue;

                float dx = static_cast<float>(candidate.position.x - pos.x);
                float dz = static_cast<float>(candidate.position.z - pos.z);
                if ((dx * dx + dz * dz) <= radSq) {
                    results.push_back(&candidate);
                }
            }
        }
    }
    return results;
}

SmartObject* SmartObjectAffordanceGrid::FindNearestAvailableObject(const LocationVector& pos, AffordanceType affordanceType, float maxRadius, uint32 entityId)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    std::vector<SmartObject*> candidates = FindObjectsInRadius(pos, maxRadius, affordanceType);
    SmartObject* best = nullptr;
    float bestDistSq = maxRadius * maxRadius;

    for (SmartObject* obj : candidates) {
        bool availableForEntity = obj->IsAvailable();
        if (!availableForEntity && entityId != 0) {
            for (uint32 u : obj->activeUsers) {
                if (u == entityId) {
                    availableForEntity = true;
                    break;
                }
            }
        }
        if (!availableForEntity) continue;
        float dx = static_cast<float>(obj->position.x - pos.x);
        float dz = static_cast<float>(obj->position.z - pos.z);
        float distSq = dx * dx + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = obj;
        }
    }
    return best;
}

bool SmartObjectAffordanceGrid::TryReserve(uint32 objectId, AffordanceType affordanceType, uint32 entityId, uint32 reservationTimeoutMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(objectId);
    if (it == m_objects.end()) return false;

    SmartObject& obj = it->second;
    if (!obj.HasAffordance(affordanceType)) return false;
    if (obj.activeUsers.size() >= obj.maxCapacity) return false;

    // Check if already reserved
    for (uint32 u : obj.activeUsers) {
        if (u == entityId) {
            obj.reservationTimersMs[entityId] = reservationTimeoutMs;
            return true;
        }
    }

    obj.activeUsers.push_back(entityId);
    obj.reservationTimersMs[entityId] = reservationTimeoutMs;
    obj.status = (obj.activeUsers.size() >= obj.maxCapacity) ? SmartObjectStatus::OCCUPIED : SmartObjectStatus::OPERATIONAL;
    return true;
}

bool SmartObjectAffordanceGrid::BeginInteraction(uint32 objectId, AffordanceType affordanceType, uint32 entityId)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(objectId);
    if (it == m_objects.end()) return false;

    SmartObject& obj = it->second;
    const AffordanceDefinition* aff = obj.GetAffordance(affordanceType);
    if (!aff) return false;

    if (!TryReserve(objectId, affordanceType, entityId)) return false;

    obj.reservationTimersMs.erase(entityId);
    obj.userTimersMs[entityId] = aff->durationMs;
    obj.userAffordanceType[entityId] = affordanceType;
    return true;
}

void SmartObjectAffordanceGrid::UpdateInteractions(uint32 deltaMs, std::function<void(uint32 entityId, const AffordanceDefinition& affordance)> onCompleteCallback)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    for (auto& pair : m_objects) {
        SmartObject& obj = pair.second;
        std::vector<uint32> finishedUsers;

        // 1. Tick active interaction timers
        for (auto& userTimer : obj.userTimersMs) {
            uint32 entityId = userTimer.first;
            if (userTimer.second <= deltaMs) {
                userTimer.second = 0;
                finishedUsers.push_back(entityId);

                AffordanceType affType = obj.userAffordanceType[entityId];
                const AffordanceDefinition* aff = obj.GetAffordance(affType);
                if (aff && onCompleteCallback) {
                    onCompleteCallback(entityId, *aff);
                }
                obj.totalInteractionsCompleted++;
            } else {
                userTimer.second -= deltaMs;
            }
        }

        for (uint32 fin : finishedUsers) {
            obj.userTimersMs.erase(fin);
            obj.userAffordanceType.erase(fin);
            obj.activeUsers.erase(std::remove(obj.activeUsers.begin(), obj.activeUsers.end(), fin), obj.activeUsers.end());
        }

        // 2. Tick reservation expiration timers (prevents permanent capacity leaks)
        std::vector<uint32> expiredReservations;
        for (auto& resPair : obj.reservationTimersMs) {
            if (resPair.second <= deltaMs) {
                expiredReservations.push_back(resPair.first);
            } else {
                resPair.second -= deltaMs;
            }
        }
        for (uint32 expId : expiredReservations) {
            obj.reservationTimersMs.erase(expId);
            obj.activeUsers.erase(std::remove(obj.activeUsers.begin(), obj.activeUsers.end(), expId), obj.activeUsers.end());
        }

        // 3. Restore operational status if below capacity
        if (obj.activeUsers.size() < obj.maxCapacity && obj.status == SmartObjectStatus::OCCUPIED) {
            obj.status = SmartObjectStatus::OPERATIONAL;
        }
    }
}

bool SmartObjectAffordanceGrid::Release(uint32 objectId, uint32 entityId)
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(objectId);
    if (it == m_objects.end()) return false;

    SmartObject& obj = it->second;
    obj.userTimersMs.erase(entityId);
    obj.userAffordanceType.erase(entityId);
    obj.reservationTimersMs.erase(entityId);
    obj.activeUsers.erase(std::remove(obj.activeUsers.begin(), obj.activeUsers.end(), entityId), obj.activeUsers.end());

    if (obj.activeUsers.size() < obj.maxCapacity && obj.status == SmartObjectStatus::OCCUPIED) {
        obj.status = SmartObjectStatus::OPERATIONAL;
    }
    return true;
}

size_t SmartObjectAffordanceGrid::GetActiveReservationCount(uint32 objectId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    auto it = m_objects.find(objectId);
    return (it != m_objects.end()) ? it->second.reservationTimersMs.size() : 0;
}

void SmartObjectAffordanceGrid::Clear()
{
    std::lock_guard<std::recursive_mutex> lock(m_gridMutex);
    m_objects.clear();
    m_spatialGrid.clear();
}

// ============================================================================
// Phase 2: Social Contagion & Rumor Mutation Engine Implementation
// ============================================================================

SocialContagionEngine::SocialContagionEngine()
{
}

SocialContagionEngine::~SocialContagionEngine()
{
}

void SocialContagionEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    m_rumors.clear();
    m_panicWaves.clear();
    m_nextRumorId = 1;
    m_nextWaveId = 1;
    m_totalMutations = 0;
    m_totalPanicWaves = 0;

    // Seed initial Megacity street rumors
    SeedRumor(0, "Westview Lupine Activity Spikes", "Sightings of Merovingian Lupine enforcers gathering in Westview alleys.", 1, 0.25f);
    SeedRumor(1, "The Skull on Pier 44", "Dockworkers report finding a Marcone smuggling crate smashed with a white skull stencil painted over the latch.", 1, 0.40f);
    SeedRumor(5, "Green Code Artifacts in Subway", "Passengers on Red Line Train #1 spotted green glyphs flashing across station advertising boards.", 2, 0.30f);
}

void SocialContagionEngine::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);

    // Update active panic waves
    for (auto it = m_panicWaves.begin(); it != m_panicWaves.end();) {
        it->elapsedMs += deltaMs;
        if (it->elapsedMs >= it->durationMs) {
            it = m_panicWaves.erase(it);
        } else {
            // Linear decay from peak intensity over duration
            float remainingFrac = 1.0f - (static_cast<float>(it->elapsedMs) / static_cast<float>(it->durationMs));
            it->initialIntensity = std::max(0.05f, it->peakIntensity * remainingFrac);
            ++it;
        }
    }
}

uint32 SocialContagionEngine::SeedRumor(uint32 topicId, const std::string& headline, const std::string& narrative, uint32 districtId, float initialPanic)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    uint32 id = m_nextRumorId++;

    RumorGene gene;
    gene.rumorId = id;
    gene.topicId = topicId;
    gene.rootHeadline = headline;
    gene.rootNarrative = narrative;
    gene.currentHeadline = headline;
    gene.currentNarrative = narrative;
    gene.originDistrictId = districtId;
    gene.currentDistrictId = districtId;
    gene.originTimestampMs = 0;
    gene.generationCount = 0;
    gene.truthValue = 1.0f;
    gene.virulence = 0.75f;
    gene.panicWeight = std::clamp(initialPanic, 0.0f, 1.0f);
    gene.mutationHistory.push_back("Gen 0 [Root]: " + headline);
    gene.lastMutationType = RumorMutationType::NONE;

    m_rumors[id] = gene;
    return id;
}

void SocialContagionEngine::MutateRumorNarrative(RumorGene& rumor, const CivilianOCEAN& speakerTraits)
{
    rumor.generationCount++;
    m_totalMutations++;

    if (speakerTraits.neuroticism > 0.60f) {
        // Hysteria escalation
        rumor.lastMutationType = RumorMutationType::HYSTERIA_ESCALATION;
        rumor.panicWeight = std::min(1.0f, rumor.panicWeight + 0.20f);
        rumor.truthValue = std::max(0.1f, rumor.truthValue - 0.15f);

        if (rumor.currentHeadline.find("Vigilante") != std::string::npos || rumor.currentHeadline.find("Skull") != std::string::npos) {
            rumor.currentHeadline = "Punisher Death Squad Decimates District";
            rumor.currentNarrative = "Eyewitnesses claim Castle brought military heavy ordnance and thermite, vaporizing syndicate checkpoints into burning ruins.";
        } else if (rumor.currentHeadline.find("Gunfire") != std::string::npos || rumor.currentHeadline.find("Lupine") != std::string::npos) {
            rumor.currentHeadline = "All-Out Syndicate War Spilling Across Streets";
            rumor.currentNarrative = "Automatic gunfire tearing through sidewalks; dozens of enforcers storming civilian buildings with heavy armaments!";
        } else {
            rumor.currentHeadline = "Catastrophic Emergency Escalation: " + rumor.currentHeadline;
            rumor.currentNarrative += " [WARNING: Extreme casualty hazard and armed hostilities reported!]";
        }
        rumor.mutationHistory.push_back("Gen " + std::to_string(rumor.generationCount) + " [Hysteria]: " + rumor.currentHeadline);
    }
    else if (speakerTraits.conscientiousness < 0.40f) {
        // Factual distortion: swaps locations or factions
        rumor.lastMutationType = RumorMutationType::FACTUAL_DISTORTION;
        rumor.truthValue = std::max(0.1f, rumor.truthValue - 0.25f);

        rumor.currentHeadline = "Distorted Report: " + rumor.currentHeadline;
        rumor.currentNarrative = "Unconfirmed talk on the wire claims the incident actually occurred in Downtown, blaming rival Cypherite fixers.";
        rumor.mutationHistory.push_back("Gen " + std::to_string(rumor.generationCount) + " [Distortion]: " + rumor.currentHeadline);
    }
    else if (speakerTraits.openness > 0.65f) {
        // Matrix anomaly: green code and simulation glitches
        rumor.lastMutationType = RumorMutationType::MATRIX_ANOMALY;
        rumor.truthValue = std::max(0.1f, rumor.truthValue - 0.18f);
        rumor.virulence = std::min(1.0f, rumor.virulence + 0.10f);

        rumor.currentHeadline = "Matrix Code Glitch: " + rumor.currentHeadline;
        rumor.currentNarrative = "Witnesses swear the sky flickered emerald green, bullets stopped in mid-air, and walls turned into cascading digital rain!";
        rumor.mutationHistory.push_back("Gen " + std::to_string(rumor.generationCount) + " [Anomaly]: " + rumor.currentHeadline);
    }
    else if (speakerTraits.extraversion > 0.65f) {
        // Viral amplification: rapid proliferation
        rumor.lastMutationType = RumorMutationType::VIRAL_AMPLIFICATION;
        rumor.virulence = std::min(1.0f, rumor.virulence + 0.15f);
        rumor.currentHeadline = "[BREAKING BUZZ] " + rumor.currentHeadline;
        rumor.mutationHistory.push_back("Gen " + std::to_string(rumor.generationCount) + " [Amplified]: " + rumor.currentHeadline);
    }
    else {
        // Minor narrative drift
        rumor.truthValue = std::max(0.1f, rumor.truthValue - 0.05f);
        rumor.mutationHistory.push_back("Gen " + std::to_string(rumor.generationCount) + " [Retold]: " + rumor.currentHeadline);
    }
}

bool SocialContagionEngine::AttemptRumorTransmission(uint32 rumorId, uint32 speakerId, uint32 listenerId,
                                                      const CivilianOCEAN& speakerTraits,
                                                      const CivilianOCEAN& listenerTraits,
                                                      float distanceMeters)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    auto it = m_rumors.find(rumorId);
    if (it == m_rumors.end()) return false;

    RumorGene& rumor = it->second;

    // Support both meters (<25.0f) and world coordinate units (up to 2500.0f units = 25m)
    float effDist = (distanceMeters > 25.0f && distanceMeters <= 2500.0f) ? (distanceMeters / 100.0f) : distanceMeters;
    if (effDist > 25.0f) return false;

    // Viral transmission probability
    float distFactor = std::max(0.0f, 1.0f - (effDist / 25.0f));
    float prob = (speakerTraits.extraversion * 0.40f) + (listenerTraits.openness * 0.30f) +
                 (rumor.virulence * 0.30f) + (distFactor * 0.20f);
    prob = std::clamp(prob, 0.10f, 0.95f);

    // Deterministic pseudo-random trial
    float roll = static_cast<float>((speakerId * 37 + listenerId * 19 + rumorId * 11) % 100) / 100.0f;
    if (roll > prob) return false;

    // Transmission successful: apply mutation
    MutateRumorNarrative(rumor, speakerTraits);
    rumor.carrierEntityIds.insert(speakerId);
    rumor.carrierEntityIds.insert(listenerId);

    return true;
}

bool SocialContagionEngine::DebunkRumor(uint32 rumorId, uint32 debunkerId, const CivilianOCEAN& debunkerTraits)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    auto it = m_rumors.find(rumorId);
    if (it == m_rumors.end()) return false;

    RumorGene& rumor = it->second;
    if (debunkerTraits.conscientiousness < 0.60f) return false;

    rumor.generationCount++;
    m_totalMutations++;
    rumor.truthValue = std::min(1.0f, rumor.truthValue + 0.35f);
    rumor.panicWeight = std::max(0.05f, rumor.panicWeight - 0.30f);
    rumor.virulence = std::max(0.20f, rumor.virulence - 0.20f);
    rumor.currentHeadline = "[FACT-CHECKED] " + rumor.rootHeadline;
    rumor.currentNarrative = "Official verification confirmed: rumors exaggerated. " + rumor.rootNarrative;
    rumor.mutationHistory.push_back("Gen " + std::to_string(rumor.generationCount) + " [Fact-Checked]: " + rumor.currentHeadline);
    rumor.carrierEntityIds.insert(debunkerId);
    return true;
}

size_t SocialContagionEngine::GetCarrierCount(uint32 rumorId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    auto it = m_rumors.find(rumorId);
    return (it != m_rumors.end()) ? it->second.carrierEntityIds.size() : 0;
}

RumorGene* SocialContagionEngine::GetRumor(uint32 rumorId)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    auto it = m_rumors.find(rumorId);
    return (it != m_rumors.end()) ? &it->second : nullptr;
}

uint32 SocialContagionEngine::EmitPanicWave(const LocationVector& epicenter, float radius, float intensity, const std::string& threat, uint32 durationMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    uint32 id = m_nextWaveId++;

    PanicContagionWave wave;
    wave.eventId = id;
    wave.epicenter = epicenter;
    wave.radius = radius;
    wave.initialIntensity = std::clamp(intensity, 0.1f, 1.0f);
    wave.peakIntensity = wave.initialIntensity;
    wave.threatDescription = threat;
    wave.durationMs = durationMs;
    wave.elapsedMs = 0;
    wave.isActive = true;

    m_panicWaves.push_back(wave);
    m_totalPanicWaves++;
    return id;
}

bool SocialContagionEngine::EvaluatePanicInfection(uint32 citizenId, const LocationVector& citizenPos, const CivilianOCEAN& traits, float localDanger, float& outPanicIntensity)
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    outPanicIntensity = 0.0f;

    for (const auto& wave : m_panicWaves) {
        if (!wave.isActive) continue;

        float dx = static_cast<float>(citizenPos.x - wave.epicenter.x);
        float dz = static_cast<float>(citizenPos.z - wave.epicenter.z);
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= wave.radius) {
            float proxFactor = 1.0f - (dist / wave.radius);
            // Neuroticism amplifies panic, conscientiousness resists
            float susceptibility = std::clamp(traits.neuroticism * 1.3f + (1.0f - traits.conscientiousness) * 0.4f + localDanger * 0.5f, 0.1f, 1.0f);
            float intensity = wave.initialIntensity * proxFactor * susceptibility;

            if (intensity > outPanicIntensity) {
                outPanicIntensity = intensity;
            }
        }
    }

    return (outPanicIntensity > 0.35f);
}

size_t SocialContagionEngine::GetActivePanicEventsCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    return m_panicWaves.size();
}

void SocialContagionEngine::ClearPanic()
{
    std::lock_guard<std::recursive_mutex> lock(m_contagionMutex);
    m_panicWaves.clear();
}

// ============================================================================
// Phase 3: Anti-Flicker Hysteresis Utility AI Curves Implementation
// ============================================================================

float UtilityCurve::Evaluate(float input) const
{
    float x = std::clamp(input, 0.0f, 1.0f);
    float y = 0.0f;

    switch (type) {
    case UtilityCurveType::LINEAR:
        y = slope * x + offset;
        break;
    case UtilityCurveType::POLYNOMIAL:
        y = std::pow(x, exponent) + offset;
        break;
    case UtilityCurveType::LOGISTIC_SIGMOID: {
        // 1 / (1 + exp(-k * (x - x0)))
        float expVal = std::exp(-slope * (x - midpoint));
        y = (1.0f / (1.0f + expVal)) + offset;
        break;
    }
    case UtilityCurveType::EXPONENTIAL: {
        // (exp(k * x) - 1) / (exp(k) - 1)
        float num = std::exp(slope * x) - 1.0f;
        float den = std::exp(slope) - 1.0f;
        y = (den != 0.0f ? (num / den) : x) + offset;
        break;
    }
    case UtilityCurveType::INVERSE_EXPONENTIAL:
        y = (1.0f - std::exp(-slope * x)) + offset;
        break;
    case UtilityCurveType::STEP_THRESHOLD:
        y = (x >= midpoint) ? 1.0f : 0.0f;
        break;
    default:
        y = x;
        break;
    }

    return std::clamp(y, 0.0f, 1.0f);
}

UtilityAICurveEngine::UtilityAICurveEngine()
{
}

UtilityAICurveEngine::~UtilityAICurveEngine()
{
}

void UtilityAICurveEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_utilityMutex);
    m_actionConfigs.clear();

    // 1. Civilian Actions
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::CIV_IDLE_STROLL;
        cfg.name = "Idle Stroll";
        cfg.primaryCurve = { UtilityCurveType::LINEAR, 0.5f, 0.5f, 1.0f, 0.25f };
        cfg.activationThreshold = 0.20f;
        cfg.deactivationThreshold = 0.10f;
        cfg.inertiaBonus = 0.12f;
        cfg.minCommitmentMs = 4000;
        cfg.isEmergencyOverride = false;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::CIV_SEEK_FOOD;
        cfg.name = "Seek Food & Dine";
        cfg.primaryCurve = { UtilityCurveType::LOGISTIC_SIGMOID, 8.0f, 0.55f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.65f;
        cfg.deactivationThreshold = 0.35f;
        cfg.inertiaBonus = 0.20f;
        cfg.minCommitmentMs = 8000;
        cfg.isEmergencyOverride = false;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::CIV_SEEK_REST;
        cfg.name = "Seek Rest & Bench";
        cfg.primaryCurve = { UtilityCurveType::EXPONENTIAL, 3.5f, 0.5f, 2.0f, 0.0f };
        cfg.activationThreshold = 0.60f;
        cfg.deactivationThreshold = 0.35f;
        cfg.inertiaBonus = 0.18f;
        cfg.minCommitmentMs = 10000;
        cfg.isEmergencyOverride = false;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::CIV_REPORT_CRIME_CALLBOX;
        cfg.name = "Report Crime to Callbox";
        cfg.primaryCurve = { UtilityCurveType::STEP_THRESHOLD, 1.0f, 0.50f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.65f;
        cfg.deactivationThreshold = 0.30f;
        cfg.inertiaBonus = 0.25f;
        cfg.minCommitmentMs = 6000;
        cfg.isEmergencyOverride = false;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::CIV_PANIC_FLEE;
        cfg.name = "Panic & Flee";
        cfg.primaryCurve = { UtilityCurveType::EXPONENTIAL, 5.0f, 0.40f, 2.0f, 0.0f };
        cfg.activationThreshold = 0.55f;
        cfg.deactivationThreshold = 0.25f;
        cfg.inertiaBonus = 0.25f;
        cfg.minCommitmentMs = 12000;
        cfg.isEmergencyOverride = true; // Bypasses commitment
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::CIV_TAKE_SHELTER;
        cfg.name = "Take Shelter";
        cfg.primaryCurve = { UtilityCurveType::LOGISTIC_SIGMOID, 6.0f, 0.60f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.70f;
        cfg.deactivationThreshold = 0.40f;
        cfg.inertiaBonus = 0.22f;
        cfg.minCommitmentMs = 15000;
        cfg.isEmergencyOverride = true;
        m_actionConfigs[cfg.actionId] = cfg;
    }

    // 2. Gang Enforcer Actions
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::GANG_IDLE_TURF_WATCH;
        cfg.name = "Gang Idle Turf Watch";
        cfg.primaryCurve = { UtilityCurveType::LINEAR, 0.5f, 0.5f, 1.0f, 0.30f };
        cfg.activationThreshold = 0.25f;
        cfg.deactivationThreshold = 0.15f;
        cfg.inertiaBonus = 0.15f;
        cfg.minCommitmentMs = 5000;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::GANG_FLEE_CASTLE;
        cfg.name = "Gang Flee Punisher Castle";
        cfg.primaryCurve = { UtilityCurveType::EXPONENTIAL, 6.0f, 0.30f, 2.0f, 0.0f };
        cfg.activationThreshold = 0.50f;
        cfg.deactivationThreshold = 0.20f;
        cfg.inertiaBonus = 0.35f;
        cfg.minCommitmentMs = 15000;
        cfg.isEmergencyOverride = true;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::GANG_DEFEND_RACKET;
        cfg.name = "Gang Defend Racket Stronghold";
        cfg.primaryCurve = { UtilityCurveType::STEP_THRESHOLD, 1.0f, 0.40f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.60f;
        cfg.deactivationThreshold = 0.30f;
        cfg.inertiaBonus = 0.25f;
        cfg.minCommitmentMs = 10000;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::GANG_SURRENDER_SWAT;
        cfg.name = "Gang Surrender to SWAT";
        cfg.primaryCurve = { UtilityCurveType::LOGISTIC_SIGMOID, 7.0f, 0.70f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.75f;
        cfg.deactivationThreshold = 0.40f;
        cfg.inertiaBonus = 0.30f;
        cfg.minCommitmentMs = 20000;
        m_actionConfigs[cfg.actionId] = cfg;
    }

    // 3. Police Officer Actions
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::COP_ROUTINE_PATROL;
        cfg.name = "Police Routine Beat Patrol";
        cfg.primaryCurve = { UtilityCurveType::LINEAR, 0.4f, 0.5f, 1.0f, 0.40f };
        cfg.activationThreshold = 0.30f;
        cfg.deactivationThreshold = 0.15f;
        cfg.inertiaBonus = 0.15f;
        cfg.minCommitmentMs = 8000;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::COP_INVESTIGATE_10CODE;
        cfg.name = "Police Investigate 10-Code";
        cfg.primaryCurve = { UtilityCurveType::STEP_THRESHOLD, 1.0f, 0.50f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.65f;
        cfg.deactivationThreshold = 0.35f;
        cfg.inertiaBonus = 0.25f;
        cfg.minCommitmentMs = 12000;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::COP_TACTICAL_BREACH;
        cfg.name = "Police Tactical SWAT Breach";
        cfg.primaryCurve = { UtilityCurveType::LOGISTIC_SIGMOID, 8.0f, 0.60f, 1.0f, 0.0f };
        cfg.activationThreshold = 0.70f;
        cfg.deactivationThreshold = 0.40f;
        cfg.inertiaBonus = 0.30f;
        cfg.minCommitmentMs = 15000;
        m_actionConfigs[cfg.actionId] = cfg;
    }
    {
        ActionUtilityConfig cfg;
        cfg.actionId = EmergentActionId::COP_TAKE_SYNDICATE_BRIBE;
        cfg.name = "Police Take Syndicate Kickback";
        cfg.primaryCurve = { UtilityCurveType::EXPONENTIAL, 4.0f, 0.50f, 2.0f, 0.0f };
        cfg.activationThreshold = 0.65f;
        cfg.deactivationThreshold = 0.35f;
        cfg.inertiaBonus = 0.20f;
        cfg.minCommitmentMs = 10000;
        m_actionConfigs[cfg.actionId] = cfg;
    }
}

EmergentActionId UtilityAICurveEngine::EvaluateCivilianAction(AgentUtilityState& state,
                                                             const BluepillCitizen& citizen,
                                                             float localDanger,
                                                             bool hasNearbyCrime,
                                                             bool hasAvailableCallbox,
                                                             bool hasAvailableFood,
                                                             bool hasAvailableBench,
                                                             uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_utilityMutex);
    state.currentActionDurationMs += deltaMs;

    // Calculate candidate utility scores
    std::map<EmergentActionId, float> scores;

    // 1. Panic Flee (acute danger)
    float panicInput = std::clamp(localDanger + citizen.traits.neuroticism * 0.4f, 0.0f, 1.0f);
    scores[EmergentActionId::CIV_PANIC_FLEE] = m_actionConfigs[EmergentActionId::CIV_PANIC_FLEE].primaryCurve.Evaluate(panicInput);

    // 2. Take Shelter (very high danger + nearby structure)
    float shelterInput = std::clamp(localDanger * 1.2f, 0.0f, 1.0f);
    scores[EmergentActionId::CIV_TAKE_SHELTER] = (localDanger > 0.6f) ? m_actionConfigs[EmergentActionId::CIV_TAKE_SHELTER].primaryCurve.Evaluate(shelterInput) : 0.0f;

    // 3. Report Crime to Callbox
    if (hasNearbyCrime && hasAvailableCallbox && (localDanger < 0.7f || citizen.traits.conscientiousness > 0.65f)) {
        float civicDuty = citizen.traits.conscientiousness;
        scores[EmergentActionId::CIV_REPORT_CRIME_CALLBOX] = m_actionConfigs[EmergentActionId::CIV_REPORT_CRIME_CALLBOX].primaryCurve.Evaluate(civicDuty);
    } else {
        scores[EmergentActionId::CIV_REPORT_CRIME_CALLBOX] = 0.0f;
    }

    // 4. Seek Food
    if (hasAvailableFood && localDanger < 0.4f) {
        scores[EmergentActionId::CIV_SEEK_FOOD] = m_actionConfigs[EmergentActionId::CIV_SEEK_FOOD].primaryCurve.Evaluate(citizen.drives.hunger);
    } else {
        scores[EmergentActionId::CIV_SEEK_FOOD] = 0.0f;
    }

    // 5. Seek Rest
    if (hasAvailableBench && localDanger < 0.3f) {
        scores[EmergentActionId::CIV_SEEK_REST] = m_actionConfigs[EmergentActionId::CIV_SEEK_REST].primaryCurve.Evaluate(citizen.drives.fatigue);
    } else {
        scores[EmergentActionId::CIV_SEEK_REST] = 0.0f;
    }

    // 6. Idle Stroll (baseline)
    scores[EmergentActionId::CIV_IDLE_STROLL] = m_actionConfigs[EmergentActionId::CIV_IDLE_STROLL].primaryCurve.Evaluate(0.5f);

    // Evaluate Hysteresis & Action Commitment
    const ActionUtilityConfig& currentCfg = m_actionConfigs[state.currentAction];

    // Check emergency override
    for (const auto& pair : scores) {
        EmergentActionId act = pair.first;
        float score = pair.second;
        const ActionUtilityConfig& cfg = m_actionConfigs[act];
        if (cfg.isEmergencyOverride && score >= cfg.activationThreshold && act != state.currentAction) {
            state.currentAction = act;
            state.currentActionDurationMs = 0;
            state.currentActionUtility = score;
            state.totalSwitchesExecuted++;
            return state.currentAction;
        }
    }

    // If within commitment period, check if current utility remains above deactivation threshold
    float currentRawScore = scores[state.currentAction];
    if (state.currentActionDurationMs < currentCfg.minCommitmentMs && currentRawScore >= currentCfg.deactivationThreshold) {
        state.totalSwitchesPreventedByHysteresis++;
        state.currentActionUtility = currentRawScore;
        return state.currentAction;
    }

    // Apply inertia stickiness bonus to current action
    float effectiveCurrentScore = currentRawScore + currentCfg.inertiaBonus;

    // Find best candidate exceeding activation threshold and defeating effectiveCurrentScore
    EmergentActionId bestAction = state.currentAction;
    float bestScore = effectiveCurrentScore;

    for (const auto& pair : scores) {
        EmergentActionId candidate = pair.first;
        float rawScore = pair.second;
        const ActionUtilityConfig& cfg = m_actionConfigs[candidate];

        if (candidate == state.currentAction) continue;

        if (rawScore >= cfg.activationThreshold && rawScore > bestScore) {
            bestScore = rawScore;
            bestAction = candidate;
        }
    }

    if (bestAction != state.currentAction) {
        state.currentAction = bestAction;
        state.currentActionDurationMs = 0;
        state.currentActionUtility = scores[bestAction];
        state.totalSwitchesExecuted++;
    } else {
        state.totalSwitchesPreventedByHysteresis++;
        state.currentActionUtility = currentRawScore;
    }

    return state.currentAction;
}

EmergentActionId UtilityAICurveEngine::EvaluateGangAction(AgentUtilityState& state,
                                                         float localGangPower,
                                                         float rivalGangPower,
                                                         float localHeat,
                                                         bool isCastleSpotted,
                                                         bool isSWATSurrounding,
                                                         bool hasRacketUnderAttack,
                                                         uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_utilityMutex);
    state.currentActionDurationMs += deltaMs;

    // Acute Emergency: Frank Castle spotted
    if (isCastleSpotted) {
        state.currentAction = EmergentActionId::GANG_FLEE_CASTLE;
        state.currentActionDurationMs = 0;
        state.currentActionUtility = 0.95f;
        state.totalSwitchesExecuted++;
        return state.currentAction;
    }

    // Acute Emergency: Surrounded by 4-star SWAT
    if (isSWATSurrounding && localGangPower < 300.0f) {
        state.currentAction = EmergentActionId::GANG_SURRENDER_SWAT;
        state.currentActionDurationMs = 0;
        state.currentActionUtility = 0.90f;
        state.totalSwitchesExecuted++;
        return state.currentAction;
    }

    std::map<EmergentActionId, float> scores;
    scores[EmergentActionId::GANG_DEFEND_RACKET] = hasRacketUnderAttack ? 0.85f : 0.0f;
    scores[EmergentActionId::GANG_AMBUSH_RIVALS] = (localGangPower > rivalGangPower * 1.5f && rivalGangPower > 0) ? 0.70f : 0.20f;
    scores[EmergentActionId::GANG_IDLE_TURF_WATCH] = 0.45f;

    const ActionUtilityConfig& currentCfg = m_actionConfigs[state.currentAction];
    float currentRawScore = scores[state.currentAction];
    float effectiveCurrentScore = currentRawScore + currentCfg.inertiaBonus;

    EmergentActionId bestAction = state.currentAction;
    float bestScore = effectiveCurrentScore;

    for (const auto& pair : scores) {
        EmergentActionId candidate = pair.first;
        float rawScore = pair.second;
        const ActionUtilityConfig& cfg = m_actionConfigs[candidate];
        if (candidate == state.currentAction) continue;

        if (rawScore >= cfg.activationThreshold && rawScore > bestScore) {
            bestScore = rawScore;
            bestAction = candidate;
        }
    }

    if (bestAction != state.currentAction) {
        state.currentAction = bestAction;
        state.currentActionDurationMs = 0;
        state.currentActionUtility = scores[bestAction];
        state.totalSwitchesExecuted++;
    } else {
        state.totalSwitchesPreventedByHysteresis++;
    }

    return state.currentAction;
}

EmergentActionId UtilityAICurveEngine::EvaluatePoliceAction(AgentUtilityState& state,
                                                           float precinctCorruption,
                                                           bool hasActive10Code,
                                                           bool hasCrimeInProgress,
                                                           bool isOutnumbered,
                                                           bool isTacticalCall,
                                                           uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_utilityMutex);
    state.currentActionDurationMs += deltaMs;

    std::map<EmergentActionId, float> scores;

    // Tactical SWAT Breach
    scores[EmergentActionId::COP_TACTICAL_BREACH] = isTacticalCall ? 0.90f : 0.0f;

    // Take syndicate bribe
    scores[EmergentActionId::COP_TAKE_SYNDICATE_BRIBE] = (precinctCorruption > 60.0f && !isTacticalCall) ? (precinctCorruption / 100.0f) : 0.0f;

    // Investigate 10-Code / Intervene
    if (hasActive10Code || hasCrimeInProgress) {
        scores[EmergentActionId::COP_INVESTIGATE_10CODE] = (precinctCorruption < 70.0f) ? 0.80f : 0.30f;
    } else {
        scores[EmergentActionId::COP_INVESTIGATE_10CODE] = 0.0f;
    }

    // Routine Patrol
    scores[EmergentActionId::COP_ROUTINE_PATROL] = 0.40f;

    const ActionUtilityConfig& currentCfg = m_actionConfigs[state.currentAction];
    float currentRawScore = scores[state.currentAction];
    float effectiveCurrentScore = currentRawScore + currentCfg.inertiaBonus;

    EmergentActionId bestAction = state.currentAction;
    float bestScore = effectiveCurrentScore;

    for (const auto& pair : scores) {
        EmergentActionId candidate = pair.first;
        float rawScore = pair.second;
        const ActionUtilityConfig& cfg = m_actionConfigs[candidate];
        if (candidate == state.currentAction) continue;

        if (rawScore >= cfg.activationThreshold && rawScore > bestScore) {
            bestScore = rawScore;
            bestAction = candidate;
        }
    }

    if (bestAction != state.currentAction) {
        state.currentAction = bestAction;
        state.currentActionDurationMs = 0;
        state.currentActionUtility = scores[bestAction];
        state.totalSwitchesExecuted++;
    } else {
        state.totalSwitchesPreventedByHysteresis++;
    }

    return state.currentAction;
}

const ActionUtilityConfig* UtilityAICurveEngine::GetActionConfig(EmergentActionId action) const
{
    auto it = m_actionConfigs.find(action);
    return (it != m_actionConfigs.end()) ? &it->second : nullptr;
}

std::string UtilityAICurveEngine::GetActionName(EmergentActionId action)
{
    switch (action) {
    case EmergentActionId::CIV_IDLE_STROLL: return "CIV_IDLE_STROLL";
    case EmergentActionId::CIV_SEEK_FOOD: return "CIV_SEEK_FOOD";
    case EmergentActionId::CIV_SEEK_REST: return "CIV_SEEK_REST";
    case EmergentActionId::CIV_WORK_SHIFT: return "CIV_WORK_SHIFT";
    case EmergentActionId::CIV_COMMUTE: return "CIV_COMMUTE";
    case EmergentActionId::CIV_RETAIL_SHOPPING: return "CIV_RETAIL_SHOPPING";
    case EmergentActionId::CIV_SOCIAL_GOSSIP: return "CIV_SOCIAL_GOSSIP";
    case EmergentActionId::CIV_REPORT_CRIME_CALLBOX: return "CIV_REPORT_CRIME_CALLBOX";
    case EmergentActionId::CIV_PANIC_FLEE: return "CIV_PANIC_FLEE";
    case EmergentActionId::CIV_TAKE_SHELTER: return "CIV_TAKE_SHELTER";
    case EmergentActionId::CIV_OPERATOR_EXTRACTION: return "CIV_OPERATOR_EXTRACTION";
    case EmergentActionId::GANG_IDLE_TURF_WATCH: return "GANG_IDLE_TURF_WATCH";
    case EmergentActionId::GANG_EXTORT_COMMERCE: return "GANG_EXTORT_COMMERCE";
    case EmergentActionId::GANG_DEAL_CONTRABAND: return "GANG_DEAL_CONTRABAND";
    case EmergentActionId::GANG_AMBUSH_RIVALS: return "GANG_AMBUSH_RIVALS";
    case EmergentActionId::GANG_DEFEND_RACKET: return "GANG_DEFEND_RACKET";
    case EmergentActionId::GANG_FLEE_CASTLE: return "GANG_FLEE_CASTLE";
    case EmergentActionId::GANG_SURRENDER_SWAT: return "GANG_SURRENDER_SWAT";
    case EmergentActionId::COP_ROUTINE_PATROL: return "COP_ROUTINE_PATROL";
    case EmergentActionId::COP_INVESTIGATE_10CODE: return "COP_INVESTIGATE_10CODE";
    case EmergentActionId::COP_INTERVENE_CRIME: return "COP_INTERVENE_CRIME";
    case EmergentActionId::COP_CALL_SWAT_BACKUP: return "COP_CALL_SWAT_BACKUP";
    case EmergentActionId::COP_TACTICAL_BREACH: return "COP_TACTICAL_BREACH";
    case EmergentActionId::COP_TAKE_SYNDICATE_BRIBE: return "COP_TAKE_SYNDICATE_BRIBE";
    default: return "UNKNOWN_ACTION";
    }
}

// ============================================================================
// Phase 4: Tiered LOD Simulation Culling Implementation
// ============================================================================

TieredLODSimulationManager::TieredLODSimulationManager()
{
}

TieredLODSimulationManager::~TieredLODSimulationManager()
{
}

void TieredLODSimulationManager::RegisterObserver(uint32 observerId, const std::string& name, const LocationVector& pos)
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    ObserverFocusPoint pt;
    pt.observerId = observerId;
    pt.name = name;
    pt.position = pos;
    pt.activeRadius = 50.0f;
    m_observers[observerId] = pt;
}

void TieredLODSimulationManager::UpdateObserverPosition(uint32 observerId, const LocationVector& pos)
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    auto it = m_observers.find(observerId);
    if (it != m_observers.end()) {
        it->second.position = pos;
    }
}

void TieredLODSimulationManager::RemoveObserver(uint32 observerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    m_observers.erase(observerId);
}

LODTier TieredLODSimulationManager::ComputeEntityLOD(const LocationVector& entityPos, float& outNearestDist) const
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    if (m_observers.empty()) {
        outNearestDist = 99999.0f;
        return LODTier::LOD3_CULLED_VIRTUAL;
    }

    float minDist = 99999.0f;
    for (const auto& pair : m_observers) {
        const auto& obs = pair.second;
        float dx = static_cast<float>(entityPos.x - obs.position.x);
        float dz = static_cast<float>(entityPos.z - obs.position.z);
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < minDist) {
            minDist = dist;
        }
    }

    outNearestDist = minDist;
    if (minDist < m_lod0Radius) {
        return LODTier::LOD0_ACTIVE_VIEWPORT;
    } else if (minDist < m_lod1Radius) {
        return LODTier::LOD1_VICINITY;
    } else if (minDist < m_lod2Radius) {
        return LODTier::LOD2_BACKGROUND;
    } else {
        return LODTier::LOD3_CULLED_VIRTUAL;
    }
}

void TieredLODSimulationManager::SetLODThresholds(float lod0, float lod1, float lod2)
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    m_lod0Radius = lod0;
    m_lod1Radius = lod1;
    m_lod2Radius = lod2;
}

void TieredLODSimulationManager::UpdateEntityLOD(SimulatedEntityLODState& lodState, const LocationVector& entityPos)
{
    float dist = 0.0f;
    LODTier newTier = ComputeEntityLOD(entityPos, dist);
    lodState.distanceToNearestObserver = dist;

    // Check promotion from virtualized tier
    if (lodState.currentTier == LODTier::LOD3_CULLED_VIRTUAL && newTier != LODTier::LOD3_CULLED_VIRTUAL) {
        lodState.needsStateCatchup = true;
    }

    lodState.currentTier = newTier;
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    m_entityTierRegistry[lodState.entityId] = newTier;
}

bool TieredLODSimulationManager::ShouldEntityTick(SimulatedEntityLODState& lodState, uint32 deltaMs)
{
    lodState.timeSinceLastTickMs += deltaMs;

    switch (lodState.currentTier) {
    case LODTier::LOD0_ACTIVE_VIEWPORT:
        // Full frequency: ticks every frame
        lodState.timeSinceLastTickMs = 0;
        return true;

    case LODTier::LOD1_VICINITY:
        // 10 Hz: ticks every 100ms
        if (lodState.timeSinceLastTickMs >= 100) {
            lodState.timeSinceLastTickMs = 0;
            return true;
        }
        return false;

    case LODTier::LOD2_BACKGROUND:
        // 1 Hz: ticks every 1000ms
        if (lodState.timeSinceLastTickMs >= 1000) {
            lodState.timeSinceLastTickMs = 0;
            return true;
        }
        return false;

    case LODTier::LOD3_CULLED_VIRTUAL:
        // 0 Hz: dormant
        lodState.totalAccumulatedVirtualMs += deltaMs;
        return false;

    default:
        return false;
    }
}

void TieredLODSimulationManager::ApplyAnalyticalCatchUp(BluepillCitizen& citizen, uint32 elapsedVirtualMs)
{
    if (elapsedVirtualMs == 0) return;
    float elapsedSec = static_cast<float>(elapsedVirtualMs) / 1000.0f;

    // Analytical drift of drives during culled background period
    citizen.drives.hunger = std::min(1.0f, citizen.drives.hunger + elapsedSec * 0.00003f);
    citizen.drives.fatigue = std::min(1.0f, citizen.drives.fatigue + elapsedSec * 0.00002f);

    // If working, accumulate wages analytically
    if (citizen.IsAtWork()) {
        uint32 hoursPassed = static_cast<uint32>(elapsedSec / 3600.0f);
        if (hoursPassed > 0) {
            citizen.drives.walletInfo += hoursPassed * 45;
            citizen.totalWagesEarned += hoursPassed * 45;
        }
    }

    // Step position towards destination
    float dx = static_cast<float>(citizen.destinationLocation.x - citizen.currentLocation.x);
    float dz = static_cast<float>(citizen.destinationLocation.z - citizen.currentLocation.z);
    float dist = std::sqrt(dx * dx + dz * dz);
    float maxTravel = citizen.movementSpeed * elapsedSec;

    if (dist > 0.001f) {
        if (maxTravel >= dist) {
            citizen.currentLocation = citizen.destinationLocation;
        } else {
            float frac = maxTravel / dist;
            citizen.currentLocation.x += dx * frac;
            citizen.currentLocation.z += dz * frac;
        }
    }
}

size_t TieredLODSimulationManager::GetEntityCountInLOD(LODTier tier) const
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    size_t count = 0;
    for (const auto& pair : m_entityTierRegistry) {
        if (pair.second == tier) count++;
    }
    return count;
}

void TieredLODSimulationManager::RegisterManagedEntity(uint32 entityId, LODTier initialTier)
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    m_entityTierRegistry[entityId] = initialTier;
}

void TieredLODSimulationManager::SetEntityLOD(uint32 entityId, LODTier tier)
{
    std::lock_guard<std::recursive_mutex> lock(m_lodMutex);
    m_entityTierRegistry[entityId] = tier;
}

// ============================================================================
// Phase 5: EmergentAIEngine Orchestrator Implementation
// ============================================================================

EmergentAIEngine::EmergentAIEngine()
{
}

EmergentAIEngine::~EmergentAIEngine()
{
}

void EmergentAIEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    sLog.outString("EmergentAIEngine: Initializing Advanced Emergent AI Systems (Affordances, Contagion, Hysteresis, LOD)...");

    m_affordanceGrid.Clear();
    m_contagionEngine.Initialize();
    m_utilityEngine.Initialize();

    PopulateDefaultSmartObjects();
    InitializeDefaultObservers();

    m_crossSystemEventsTriggered = 0;
    m_crimesReportedViaCallboxes = 0;
    m_hysteresisSwitchesPrevented = 0;

    sLog.outString("EmergentAIEngine: Initialized with %u Smart Objects, 3 Observers, Utility Curves, and Contagion Engine.",
                   static_cast<uint32>(m_affordanceGrid.GetTotalObjectCount()));
}

void EmergentAIEngine::PopulateDefaultSmartObjects()
{
    // District 1: Slums
    {
        SmartObject obj;
        obj.objectId = 101;
        obj.name = "Westview Tenement Phone Booth";
        obj.typeTag = "PhoneBooth";
        obj.districtId = 1;
        obj.districtName = "Slums";
        obj.position = LocationVector(1450.0, 95.0, -3150.0);
        obj.interactionRadius = 3.5f;
        obj.maxCapacity = 1;

        AffordanceDefinition callAff;
        callAff.type = AffordanceType::CALL_PHONE;
        callAff.actionName = "Make Telephone Call";
        callAff.durationMs = 6000;
        callAff.baseUtility = 0.55f;
        callAff.deltaStress = -0.15f;
        obj.affordances.push_back(callAff);

        AffordanceDefinition opAff;
        opAff.type = AffordanceType::OPERATOR_HARDLINE;
        opAff.actionName = "Connect to Operator Hardline";
        opAff.durationMs = 4000;
        opAff.baseUtility = 0.95f;
        obj.affordances.push_back(opAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 102;
        obj.name = "MMPD Slums Emergency Callbox";
        obj.typeTag = "PoliceCallbox";
        obj.districtId = 1;
        obj.districtName = "Slums";
        obj.position = LocationVector(1520.0, 95.0, -3220.0);
        obj.interactionRadius = 3.0f;
        obj.maxCapacity = 1;

        AffordanceDefinition reportAff;
        reportAff.type = AffordanceType::REPORT_CRIME;
        reportAff.actionName = "Report 10-Code Emergency";
        reportAff.durationMs = 4000;
        reportAff.baseUtility = 0.85f;
        reportAff.allowsPanickedUsers = true;
        obj.affordances.push_back(reportAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 103;
        obj.name = "Morrell Street Noodle Cart";
        obj.typeTag = "NoodleCounter";
        obj.districtId = 1;
        obj.districtName = "Slums";
        obj.position = LocationVector(1380.0, 95.0, -3100.0);
        obj.interactionRadius = 5.0f;
        obj.maxCapacity = 4;

        AffordanceDefinition eatAff;
        eatAff.type = AffordanceType::EAT_FOOD;
        eatAff.actionName = "Eat Hot Pork Dumplings";
        eatAff.durationMs = 8000;
        eatAff.baseUtility = 0.70f;
        eatAff.deltaHunger = -0.40f;
        eatAff.deltaStress = -0.10f;
        eatAff.deltaWalletInfo = -12;
        obj.affordances.push_back(eatAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 104;
        obj.name = "Westview Courtyard Park Bench";
        obj.typeTag = "ParkBench";
        obj.districtId = 1;
        obj.districtName = "Slums";
        obj.position = LocationVector(1600.0, 95.0, -3300.0);
        obj.interactionRadius = 4.0f;
        obj.maxCapacity = 2;

        AffordanceDefinition restAff;
        restAff.type = AffordanceType::REST_SIT;
        restAff.actionName = "Rest on Slums Park Bench";
        restAff.durationMs = 12000;
        restAff.baseUtility = 0.65f;
        restAff.deltaFatigue = -0.30f;
        restAff.deltaStress = -0.15f;
        obj.affordances.push_back(restAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 105;
        obj.name = "Pier 44 Heavy Commercial Dumpster";
        obj.typeTag = "Dumpster";
        obj.districtId = 1;
        obj.districtName = "Slums";
        obj.position = LocationVector(1750.0, 95.0, -3450.0);
        obj.interactionRadius = 4.0f;
        obj.maxCapacity = 2;

        AffordanceDefinition coverAff;
        coverAff.type = AffordanceType::TAKE_COVER;
        coverAff.actionName = "Take Ballistic Cover Behind Dumpster";
        coverAff.durationMs = 15000;
        coverAff.baseUtility = 0.85f;
        coverAff.allowsPanickedUsers = true;
        obj.affordances.push_back(coverAff);

        AffordanceDefinition scavAff;
        scavAff.type = AffordanceType::TRASH_SCAVENGE;
        scavAff.actionName = "Scavenge Refuse for Code Fragments";
        scavAff.durationMs = 7000;
        scavAff.baseUtility = 0.40f;
        obj.affordances.push_back(scavAff);

        m_affordanceGrid.RegisterObject(obj);
    }

    // District 2: Downtown
    {
        SmartObject obj;
        obj.objectId = 201;
        obj.name = "Metacortex Plaza Public Phone Booth";
        obj.typeTag = "PhoneBooth";
        obj.districtId = 2;
        obj.districtName = "Downtown";
        obj.position = LocationVector(17050.0, 95.0, 2410.0);
        obj.interactionRadius = 3.5f;
        obj.maxCapacity = 1;

        AffordanceDefinition callAff;
        callAff.type = AffordanceType::CALL_PHONE;
        callAff.actionName = "Make Phone Call";
        callAff.durationMs = 5000;
        callAff.baseUtility = 0.50f;
        obj.affordances.push_back(callAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 202;
        obj.name = "MMPD Downtown Concourse Callbox";
        obj.typeTag = "PoliceCallbox";
        obj.districtId = 2;
        obj.districtName = "Downtown";
        obj.position = LocationVector(17100.0, 95.0, 2380.0);
        obj.interactionRadius = 3.0f;
        obj.maxCapacity = 1;

        AffordanceDefinition reportAff;
        reportAff.type = AffordanceType::REPORT_CRIME;
        reportAff.actionName = "Report 10-99 Tactical Emergency";
        reportAff.durationMs = 4000;
        reportAff.baseUtility = 0.80f;
        reportAff.allowsPanickedUsers = true;
        obj.affordances.push_back(reportAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 203;
        obj.name = "Downtown Central Bank ATM";
        obj.typeTag = "ATM";
        obj.districtId = 2;
        obj.districtName = "Downtown";
        obj.position = LocationVector(17200.0, 95.0, 2450.0);
        obj.interactionRadius = 3.0f;
        obj.maxCapacity = 1;

        AffordanceDefinition atmAff;
        atmAff.type = AffordanceType::ATM_TRANSACT;
        atmAff.actionName = "Withdraw Info Credits";
        atmAff.durationMs = 5000;
        atmAff.baseUtility = 0.60f;
        atmAff.deltaWalletInfo = 100;
        obj.affordances.push_back(atmAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 204;
        obj.name = "Downtown Central Park Bench";
        obj.typeTag = "ParkBench";
        obj.districtId = 2;
        obj.districtName = "Downtown";
        obj.position = LocationVector(16900.0, 95.0, 2300.0);
        obj.interactionRadius = 4.0f;
        obj.maxCapacity = 2;

        AffordanceDefinition restAff;
        restAff.type = AffordanceType::REST_SIT;
        restAff.actionName = "Rest in Central Park";
        restAff.durationMs = 10000;
        restAff.baseUtility = 0.60f;
        restAff.deltaFatigue = -0.25f;
        restAff.deltaStress = -0.20f;
        obj.affordances.push_back(restAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 205;
        obj.name = "Metacortex Automat Vending Machine";
        obj.typeTag = "VendingMachine";
        obj.districtId = 2;
        obj.districtName = "Downtown";
        obj.position = LocationVector(17080.0, 95.0, 2430.0);
        obj.interactionRadius = 3.0f;
        obj.maxCapacity = 2;

        AffordanceDefinition vendAff;
        vendAff.type = AffordanceType::VENDING_DRINK;
        vendAff.actionName = "Buy Hot Espresso / Soda";
        vendAff.durationMs = 4000;
        vendAff.baseUtility = 0.60f;
        vendAff.deltaFatigue = -0.20f;
        vendAff.deltaWalletInfo = -5;
        obj.affordances.push_back(vendAff);

        m_affordanceGrid.RegisterObject(obj);
    }

    // District 3: Richland
    {
        SmartObject obj;
        obj.objectId = 301;
        obj.name = "Richland Promontory Marble Bench";
        obj.typeTag = "ParkBench";
        obj.districtId = 3;
        obj.districtName = "Richland";
        obj.position = LocationVector(8500.0, 95.0, -12000.0);
        obj.interactionRadius = 4.0f;
        obj.maxCapacity = 2;

        AffordanceDefinition restAff;
        restAff.type = AffordanceType::REST_SIT;
        restAff.actionName = "Rest on Promontory Marble Bench";
        restAff.durationMs = 12000;
        restAff.baseUtility = 0.70f;
        restAff.deltaFatigue = -0.35f;
        restAff.deltaStress = -0.25f;
        obj.affordances.push_back(restAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 302;
        obj.name = "Richland Power Substation Transformer";
        obj.typeTag = "Transformer";
        obj.districtId = 3;
        obj.districtName = "Richland";
        obj.position = LocationVector(8650.0, 95.0, -12100.0);
        obj.interactionRadius = 4.0f;
        obj.maxCapacity = 1;

        AffordanceDefinition tapAff;
        tapAff.type = AffordanceType::POWER_TAP;
        tapAff.actionName = "Tap Substation Transformer";
        tapAff.durationMs = 6000;
        tapAff.baseUtility = 0.50f;
        obj.affordances.push_back(tapAff);

        m_affordanceGrid.RegisterObject(obj);
    }

    // District 4: International
    {
        SmartObject obj;
        obj.objectId = 401;
        obj.name = "Harbor Concourse News Stand";
        obj.typeTag = "NewsStand";
        obj.districtId = 4;
        obj.districtName = "International";
        obj.position = LocationVector(-6350.0, 95.0, -7100.0);
        obj.interactionRadius = 4.0f;
        obj.maxCapacity = 3;

        AffordanceDefinition newsAff;
        newsAff.type = AffordanceType::NEWS_STAND;
        newsAff.actionName = "Read Megacity Daily Gazette";
        newsAff.durationMs = 5000;
        newsAff.baseUtility = 0.55f;
        newsAff.deltaStress = -0.05f;
        obj.affordances.push_back(newsAff);

        m_affordanceGrid.RegisterObject(obj);
    }
    {
        SmartObject obj;
        obj.objectId = 402;
        obj.name = "Pier 44 Secret Vigilante Dead Drop";
        obj.typeTag = "DeadDrop";
        obj.districtId = 4;
        obj.districtName = "International";
        obj.position = LocationVector(-6450.0, 95.0, -7150.0);
        obj.interactionRadius = 3.0f;
        obj.maxCapacity = 1;

        AffordanceDefinition dropAff;
        dropAff.type = AffordanceType::DEAD_DROP;
        dropAff.actionName = "Retrieve Dead Drop Cassette Tape";
        dropAff.durationMs = 3000;
        dropAff.baseUtility = 0.90f;
        obj.affordances.push_back(dropAff);

        m_affordanceGrid.RegisterObject(obj);
    }
}

void EmergentAIEngine::InitializeDefaultObservers()
{
    // Observer 1: Active Player Neo (Downtown)
    m_lodManager.RegisterObserver(1, "Player_Neo", LocationVector(17043.0, 95.0, 2398.0));

    // Observer 2: Player Trinity (Slums)
    m_lodManager.RegisterObserver(2, "Player_Trinity", LocationVector(1500.0, 95.0, -3200.0));

    // Observer 3: Frank Castle Overwatch (Westview Slums / Pier 44)
    m_lodManager.RegisterObserver(3, "FrankCastle_Overwatch", LocationVector(1450.0, 95.0, -3150.0));
}

void EmergentAIEngine::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 1. Update Smart Object interactions and complete triggers
    m_affordanceGrid.UpdateInteractions(deltaMs, [this](uint32 entityId, const AffordanceDefinition& affordance) {
        BluepillCitizen* c = sCityLifeMgr.GetCitizen(entityId);
        if (c) {
            c->drives.hunger = std::clamp(c->drives.hunger + affordance.deltaHunger, 0.0f, 1.0f);
            c->drives.fatigue = std::clamp(c->drives.fatigue + affordance.deltaFatigue, 0.0f, 1.0f);
            c->drives.stress = std::clamp(c->drives.stress + affordance.deltaStress, 0.0f, 1.0f);
            if (affordance.deltaWalletInfo < 0) {
                uint32 cost = static_cast<uint32>(-affordance.deltaWalletInfo);
                c->drives.walletInfo = (c->drives.walletInfo >= cost) ? (c->drives.walletInfo - cost) : 0;
            } else {
                c->drives.walletInfo += static_cast<uint32>(affordance.deltaWalletInfo);
            }
        }
    });

    // 2. Update Social Contagion & Rumors
    m_contagionEngine.Update(deltaMs);

    // 3. Keep track of hysteresis stats (sum current totals, avoid accumulating runaway sums)
    uint32 totalSwitchesPrevented = 0;
    for (const auto& pair : m_civilianUtilityStates) {
        totalSwitchesPrevented += pair.second.totalSwitchesPreventedByHysteresis;
    }
    m_hysteresisSwitchesPrevented = totalSwitchesPrevented;
}

void EmergentAIEngine::UpdateManagedCitizens(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    const auto& allCitizens = sCityLifeMgr.GetAllCitizens();
    if (allCitizens.empty()) return;

    for (const auto& pair : allCitizens) {
        uint32 cId = pair.first;
        BluepillCitizen* c = sCityLifeMgr.GetCitizen(cId);
        if (!c) continue;

        SimulatedEntityLODState* lodState = GetOrCreateLODState(c->id);

        // Update LOD classification relative to observers
        m_lodManager.UpdateEntityLOD(*lodState, c->currentLocation);

        // If promoted from dormant LOD3, execute analytical catch-up
        if (lodState->needsStateCatchup) {
            m_lodManager.ApplyAnalyticalCatchUp(*c, lodState->totalAccumulatedVirtualMs);
            lodState->totalAccumulatedVirtualMs = 0;
            lodState->needsStateCatchup = false;
        }

        // Check if entity should tick this frame according to its LOD frequency
        if (!m_lodManager.ShouldEntityTick(*lodState, deltaMs)) {
            continue; // Skipped for LOD3 or frequency-gated for LOD1/2
        }

        // Entity is actively ticking: evaluate Utility AI
        AgentUtilityState* utilState = GetOrCreateUtilityState(c->id);

        float localDanger = c->IsPanicking() ? 0.85f : 0.05f;
        bool hasCrime = c->IsPanicking();
        bool hasCallbox = (m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::REPORT_CRIME, 300.0f, c->id) != nullptr);
        bool hasFood = (c->drives.hunger > 0.4f && m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::EAT_FOOD, 500.0f, c->id) != nullptr);
        bool hasBench = (c->drives.fatigue > 0.4f && m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::REST_SIT, 500.0f, c->id) != nullptr);

        EmergentActionId chosenAction = m_utilityEngine.EvaluateCivilianAction(
            *utilState, *c, localDanger, hasCrime, hasCallbox, hasFood, hasBench, deltaMs
        );

        if (chosenAction == EmergentActionId::CIV_SEEK_FOOD) {
            SmartObject* foodObj = m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::EAT_FOOD, 500.0f, c->id);
            if (foodObj && foodObj->IsAvailable()) {
                m_affordanceGrid.BeginInteraction(foodObj->objectId, AffordanceType::EAT_FOOD, c->id);
            }
        } else if (chosenAction == EmergentActionId::CIV_SEEK_REST) {
            SmartObject* benchObj = m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::REST_SIT, 500.0f, c->id);
            if (benchObj && benchObj->IsAvailable()) {
                m_affordanceGrid.BeginInteraction(benchObj->objectId, AffordanceType::REST_SIT, c->id);
            }
        } else if (chosenAction == EmergentActionId::CIV_REPORT_CRIME_CALLBOX) {
            SmartObject* callbox = m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::REPORT_CRIME, 300.0f, c->id);
            if (callbox && callbox->IsAvailable()) {
                m_affordanceGrid.BeginInteraction(callbox->objectId, AffordanceType::REPORT_CRIME, c->id);
                m_crimesReportedViaCallboxes++;
                c->currentRoutine = RoutineScheduleState::Sheltering;
                c->panicTimerMs = 0;
                OnPolice10CodeDispatched(c->districtId, "10-71", c->currentLocation);
            }
        }
    }
}

void EmergentAIEngine::OnEmergentCrimeDetected(uint32 crimeId, uint32 districtId, const LocationVector& crimePos, const std::string& crimeDesc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_crossSystemEventsTriggered++;

    // 1. Emit panic wave into the crowd
    m_contagionEngine.EmitPanicWave(crimePos, 70.0f, 0.85f, "Crime In Progress: " + crimeDesc, 30000);
    sCityLifeMgr.QueueAreaPanic(static_cast<float>(crimePos.x), static_cast<float>(crimePos.z), 70.0f, "Crime: " + crimeDesc, 30000);

    // 2. Seed rumor into the social rumor engine
    m_contagionEngine.SeedRumor(0, "Shootout in " + CityLifeManager::GetDistrictName(districtId),
                                crimeDesc + " erupted on the street.", districtId, 0.60f);

    // 3. Scan nearby civilians to evaluate whether high-conscientiousness citizen reports to callbox
    auto nearbyCitizens = sCityLifeMgr.GetCitizensInDistrict(districtId);
    for (BluepillCitizen* c : nearbyCitizens) {
        if (!c) continue;

        float dx = static_cast<float>(c->currentLocation.x - crimePos.x);
        float dz = static_cast<float>(c->currentLocation.z - crimePos.z);
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist < 150.0f && c->traits.conscientiousness > 0.50f) {
            // Find nearest callbox
            SmartObject* callbox = m_affordanceGrid.FindNearestAvailableObject(c->currentLocation, AffordanceType::REPORT_CRIME, 300.0f, c->id);
            if (callbox) {
                m_affordanceGrid.BeginInteraction(callbox->objectId, AffordanceType::REPORT_CRIME, c->id);
                m_crimesReportedViaCallboxes++;
                c->currentRoutine = RoutineScheduleState::Sheltering;
                c->panicTimerMs = 0;

                sLog.outString("EmergentAIEngine: Citizen #%u [%s] reached Callbox [%s] and dialed 911 dispatch!",
                               c->id, c->name.c_str(), callbox->name.c_str());

                // Trigger MMPD 10-code dispatch
                OnPolice10CodeDispatched(districtId, "10-71", crimePos);
                break;
            }
        }
    }
}

void EmergentAIEngine::OnPolice10CodeDispatched(uint32 precinctId, const std::string& tenCode, const LocationVector& targetPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_crossSystemEventsTriggered++;

    sLog.outString("EmergentAIEngine: Dispatching MMPD Code [%s] to sector (%d, %d).",
                   tenCode.c_str(), static_cast<int>(targetPos.x), static_cast<int>(targetPos.z));

    // Update observer focus point for police response
    m_lodManager.UpdateObserverPosition(3, targetPos);
}

void EmergentAIEngine::OnFrankCastleAmbushExecuted(const LocationVector& ambushPos, const std::string& targetDesc, bool skullLeft)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_crossSystemEventsTriggered++;

    // 1. Seed or amplify Punisher rumor
    m_contagionEngine.SeedRumor(1, "The Punisher Strike: " + targetDesc,
                                "Frank Castle ambushed and wiped out syndicate stronghold. Skull spray-painted at scene.", 1, 0.75f);

    // 2. Emit gunshot echo panic wave
    OnGunfireEcho(ambushPos, 80.0f, "Punisher Assault on " + targetDesc);
}

void EmergentAIEngine::OnGunfireEcho(const LocationVector& originPos, float loudnessRadius, const std::string& sourceDesc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_contagionEngine.EmitPanicWave(originPos, loudnessRadius, 0.90f, sourceDesc, 25000);
    sCityLifeMgr.QueueAreaPanic(static_cast<float>(originPos.x), static_cast<float>(originPos.z), loudnessRadius, sourceDesc, 25000);
}

AgentUtilityState* EmergentAIEngine::GetOrCreateUtilityState(uint32 entityId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_civilianUtilityStates.find(entityId);
    if (it == m_civilianUtilityStates.end()) {
        AgentUtilityState st;
        st.entityId = entityId;
        st.currentAction = EmergentActionId::CIV_IDLE_STROLL;
        st.currentActionDurationMs = 0;
        st.currentActionUtility = 0.5f;
        m_civilianUtilityStates[entityId] = st;
        return &m_civilianUtilityStates[entityId];
    }
    return &it->second;
}

SimulatedEntityLODState* EmergentAIEngine::GetOrCreateLODState(uint32 entityId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_citizenLODStates.find(entityId);
    if (it == m_citizenLODStates.end()) {
        SimulatedEntityLODState st;
        st.entityId = entityId;
        st.currentTier = LODTier::LOD3_CULLED_VIRTUAL;
        m_citizenLODStates[entityId] = st;
        return &m_citizenLODStates[entityId];
    }
    return &it->second;
}

std::string EmergentAIEngine::GenerateEmergentTelemetryReport() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "====================================================================\n";
    ss << "       EMERGENT AI ENGINE: MASTER TELEMETRY & SYSTEM REPORT         \n";
    ss << "====================================================================\n";
    ss << "Smart Objects Active: " << m_affordanceGrid.GetTotalObjectCount() << " registered nodes\n";
    ss << "Social Contagion Rumors: " << m_contagionEngine.GetTotalRumorCount() << " rumors tracked\n";
    ss << "Rumor Mutation Hops: " << m_contagionEngine.GetTotalRumorMutations() << " mutations executed\n";
    ss << "Panic Contagion Waves: " << m_contagionEngine.GetTotalPanicWavesEmitted() << " waves emitted\n";
    ss << "Cross-System Collisions Triggered: " << m_crossSystemEventsTriggered << " events\n";
    ss << "911 Calls via Smart Callboxes: " << m_crimesReportedViaCallboxes << " citizen reports\n";
    ss << "Hysteresis Switches Prevented: " << m_hysteresisSwitchesPrevented << " flickers suppressed\n\n";

    ss << "--- Tiered LOD Distribution ---\n";
    ss << "  LOD 0 (Active Viewport <50m):  " << m_lodManager.GetEntityCountInLOD(LODTier::LOD0_ACTIVE_VIEWPORT) << " entities\n";
    ss << "  LOD 1 (Vicinity 50-200m):      " << m_lodManager.GetEntityCountInLOD(LODTier::LOD1_VICINITY) << " entities\n";
    ss << "  LOD 2 (Background 200-1000m):  " << m_lodManager.GetEntityCountInLOD(LODTier::LOD2_BACKGROUND) << " entities\n";
    ss << "  LOD 3 (Culled Virtual >1000m): " << m_lodManager.GetEntityCountInLOD(LODTier::LOD3_CULLED_VIRTUAL) << " entities\n";
    ss << "====================================================================\n";
    return ss.str();
}

bool EmergentAIEngine::SaveEmergentStateToFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"smartObjectsCount\": " << m_affordanceGrid.GetTotalObjectCount() << ",\n";
    out << "  \"rumorsCount\": " << m_contagionEngine.GetTotalRumorCount() << ",\n";
    out << "  \"totalMutations\": " << m_contagionEngine.GetTotalRumorMutations() << ",\n";
    out << "  \"crossSystemEvents\": " << m_crossSystemEventsTriggered << ",\n";
    out << "  \"crimesReportedViaCallboxes\": " << m_crimesReportedViaCallboxes << "\n";
    out << "}\n";

    return true;
}

bool EmergentAIEngine::LoadEmergentStateFromFile(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"crossSystemEvents\":") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                m_crossSystemEventsTriggered = static_cast<uint32>(std::stoul(line.substr(pos + 1)));
            }
        } else if (line.find("\"crimesReportedViaCallboxes\":") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                m_crimesReportedViaCallboxes = static_cast<uint32>(std::stoul(line.substr(pos + 1)));
            }
        }
    }
    return true;
}

// ============================================================================
// Phase 6: Automated Headless Emergent AI Test Suite
// ============================================================================

void RunEmergentAITestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  ADVANCED EMERGENT AI SYSTEMS - TEST SUITE VERIFICATION    " << std::endl;
    std::cout << "============================================================" << std::endl;

    EmergentAIEngine& engine = sEmergentAIMgr;
    engine.Initialize();

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

    // ========================================================================
    // TEST SECTION 1: SMART OBJECT AFFORDANCE GRID
    // ========================================================================
    SmartObjectAffordanceGrid& grid = engine.GetAffordanceGrid();
    AssertTest(grid.GetTotalObjectCount() >= 10, "Smart Objects Populated Across Megacity Districts");

    SmartObject* phoneBooth = grid.GetObject(101);
    AssertTest(phoneBooth != nullptr && phoneBooth->HasAffordance(AffordanceType::CALL_PHONE), "Phone Booth #101 Has Call Phone Affordance");
    AssertTest(phoneBooth != nullptr && phoneBooth->HasAffordance(AffordanceType::OPERATOR_HARDLINE), "Phone Booth #101 Has Operator Hardline Affordance");

    // Nearest affordance spatial query
    SmartObject* nearestBench = grid.FindNearestAvailableObject(LocationVector(1580.0, 95.0, -3280.0), AffordanceType::REST_SIT, 500.0f);
    AssertTest(nearestBench != nullptr && nearestBench->objectId == 104, "Spatial Radial Lookup: Correctly Located Nearest Slums Park Bench #104");

    // Concurrency Capacity Limits
    AssertTest(phoneBooth->maxCapacity == 1, "Phone Booth Capacity Enforces Strict Concurrency Limit (1 User)");
    bool res1 = grid.BeginInteraction(101, AffordanceType::CALL_PHONE, 501);
    AssertTest(res1, "Citizen #501 Successfully Began Interaction with Phone Booth #101");
    AssertTest(phoneBooth->status == SmartObjectStatus::OCCUPIED, "Phone Booth Status Switched to OCCUPIED Upon Reaching Capacity");

    // Second user should be rejected due to capacity limit
    bool res2 = grid.BeginInteraction(101, AffordanceType::CALL_PHONE, 502);
    AssertTest(!res2, "Second Citizen #502 Correctly Rejected Due to Full Capacity");

    // Advance interaction timer to complete
    bool completedCallbackFired = false;
    grid.UpdateInteractions(7000, [&](uint32 entityId, const AffordanceDefinition& aff) {
        if (entityId == 501 && aff.type == AffordanceType::CALL_PHONE) {
            completedCallbackFired = true;
        }
    });
    AssertTest(completedCallbackFired, "Smart Object Interaction Timer Expired: Completion Callback Dispatched");
    AssertTest(phoneBooth->IsAvailable(), "Phone Booth Capacity Released and Available Post-Interaction");

    // Noodle Cart Biometric Drive Impacts
    SmartObject* noodleCart = grid.GetObject(103);
    AssertTest(noodleCart != nullptr && noodleCart->maxCapacity == 4, "Noodle Cart Supports Multi-User Concurrency (4 Slots)");
    const AffordanceDefinition* eatAff = noodleCart->GetAffordance(AffordanceType::EAT_FOOD);
    AssertTest(eatAff != nullptr && eatAff->deltaHunger < 0.0f && eatAff->deltaWalletInfo < 0, "Eat Food Affordance Imparts Negative Hunger (-0.40) & Currency Cost (-12 Info)");

    // Test Reservation Expiration (prevents capacity leak when interaction is not consummated)
    bool resReserve = grid.TryReserve(101, AffordanceType::CALL_PHONE, 601, 3000);
    AssertTest(resReserve, "Citizen #601 Successfully Created Pending Reservation on Phone Booth #101");
    AssertTest(grid.GetActiveReservationCount(101) == 1, "Reservation Recorded with Active Expiration Timer");
    // Advance interactions past expiration timeout (4000ms > 3000ms)
    grid.UpdateInteractions(4000, nullptr);
    AssertTest(grid.GetActiveReservationCount(101) == 0, "Unconsummated Reservation Successfully Expired and Purged");
    AssertTest(phoneBooth->IsAvailable(), "Phone Booth Capacity Restored After Reservation Expiration (No Permanent Leak)");

    // Multi-User Concurrency & Status Restoration
    bool n1 = grid.BeginInteraction(103, AffordanceType::EAT_FOOD, 701);
    bool n2 = grid.BeginInteraction(103, AffordanceType::EAT_FOOD, 702);
    bool n3 = grid.BeginInteraction(103, AffordanceType::EAT_FOOD, 703);
    bool n4 = grid.BeginInteraction(103, AffordanceType::EAT_FOOD, 704);
    AssertTest(n1 && n2 && n3 && n4 && noodleCart->status == SmartObjectStatus::OCCUPIED, "Noodle Cart Filled to Max Capacity (4 Users) Switched to OCCUPIED");
    // Releasing 1 user restores status to OPERATIONAL while other 3 continue
    grid.Release(103, 701);
    AssertTest(noodleCart->status == SmartObjectStatus::OPERATIONAL && noodleCart->IsAvailable(), "Single Slot Release Restores OPERATIONAL Status (1 Slot Available for New User)");
    grid.Release(103, 702);
    grid.Release(103, 703);
    grid.Release(103, 704);

    // Dynamic Object Re-Registration & Spatial Hash Integrity
    SmartObject updatedCart = *noodleCart;
    updatedCart.position = LocationVector(1390.0, 95.0, -3110.0);
    grid.RegisterObject(updatedCart);
    AssertTest(grid.GetObject(103)->position.x == 1390.0, "Smart Object Re-Registration Successfully Updated Node Without Duplicate Spatial Keys");

    // ========================================================================
    // TEST SECTION 2: SOCIAL CONTAGION & RUMOR MUTATION ENGINE
    // ========================================================================
    SocialContagionEngine& contagion = engine.GetContagionEngine();
    AssertTest(contagion.GetTotalRumorCount() >= 3, "Ambient Street Rumor Pool Initialized");

    uint32 testRumorId = contagion.SeedRumor(1, "Gunfire Heard Near Warehouse Row",
                                             "A lone gunshot echoed across the alleyway.", 1, 0.20f);
    RumorGene* rumor = contagion.GetRumor(testRumorId);
    AssertTest(rumor != nullptr && rumor->truthValue == 1.0f && rumor->generationCount == 0, "Initial Seeded Rumor Has 100% Truth Value and Gen 0");

    // Mutation Hop 1: High Neuroticism Speaker (Hysteria Escalation)
    CivilianOCEAN neuroticSpeaker;
    neuroticSpeaker.neuroticism = 0.90f; // High panic
    neuroticSpeaker.extraversion = 0.80f;
    neuroticSpeaker.conscientiousness = 0.50f;
    neuroticSpeaker.openness = 0.50f;

    CivilianOCEAN receptiveListener;
    receptiveListener.openness = 0.80f;

    bool spread1 = contagion.AttemptRumorTransmission(testRumorId, 1001, 1002, neuroticSpeaker, receptiveListener, 5.0f);
    AssertTest(spread1, "Rumor Successfully Transmitted Between Proximate Agents (5m)");
    AssertTest(rumor->generationCount == 1, "Rumor Generation Advanced to Generation 1");
    AssertTest(rumor->lastMutationType == RumorMutationType::HYSTERIA_ESCALATION, "High Neuroticism Provoked Hysteria Escalation Mutation");
    AssertTest(rumor->panicWeight > 0.20f, "Hysteria Mutation Escalated Rumor Panic Weight");
    AssertTest(rumor->truthValue < 1.0f, "Rumor Truth Value Degraded Post-Mutation");

    // Mutation Hop 2: High Openness Speaker (Matrix Glitch Injection)
    CivilianOCEAN openSpeaker;
    openSpeaker.neuroticism = 0.20f;
    openSpeaker.openness = 0.85f;
    openSpeaker.extraversion = 0.70f;

    bool spread2 = contagion.AttemptRumorTransmission(testRumorId, 1002, 1003, openSpeaker, receptiveListener, 8.0f);
    AssertTest(spread2, "Rumor Transmitted to Third Agent");
    AssertTest(rumor->generationCount == 2, "Rumor Generation Advanced to Generation 2");
    AssertTest(rumor->lastMutationType == RumorMutationType::MATRIX_ANOMALY, "High Openness Injected Matrix Supernatural Anomaly Glitch");
    AssertTest(rumor->mutationHistory.size() >= 3, "Complete Rumor Mutation Audit Trail Recorded Across Generations");

    // Panic Wave Dynamics
    uint32 waveId = contagion.EmitPanicWave(LocationVector(1500.0, 95.0, -3200.0), 60.0f, 0.90f, "Explosion at Dockside Warehouse");
    AssertTest(waveId > 0 && contagion.GetActivePanicEventsCount() > 0, "Panic Contagion Wave Emitted from Epicenter");

    float outPanic = 0.0f;
    bool infected = contagion.EvaluatePanicInfection(777, LocationVector(1510.0, 95.0, -3205.0), neuroticSpeaker, 0.5f, outPanic);
    AssertTest(infected && outPanic > 0.5f, "Nearby Neurotic Civilian Successfully Infected with Viral Crowd Panic");

    float outPanicFar = 0.0f;
    bool infectedFar = contagion.EvaluatePanicInfection(778, LocationVector(1900.0, 95.0, -3200.0), neuroticSpeaker, 0.0f, outPanicFar);
    AssertTest(!infectedFar && outPanicFar == 0.0f, "Distant Civilian Beyond Panic Radius Protected from Viral Contagion");

    contagion.ClearPanic();
    AssertTest(contagion.GetActivePanicEventsCount() == 0, "All Panic Waves Successfully Cleared");

    // Rumor Debunking & Truth Stabilization
    CivilianOCEAN investigativeDebunker;
    investigativeDebunker.conscientiousness = 0.92f;
    investigativeDebunker.neuroticism = 0.15f;
    float preDebunkTruth = rumor->truthValue;
    float preDebunkPanic = rumor->panicWeight;
    bool debunkSuccess = contagion.DebunkRumor(testRumorId, 1004, investigativeDebunker);
    AssertTest(debunkSuccess, "High-Conscientiousness Investigator Fact-Checked Distorted Rumor");
    AssertTest(rumor->truthValue > preDebunkTruth, "Fact-Checking Restored Objective Truth Value of Rumor");
    AssertTest(rumor->panicWeight < preDebunkPanic, "Fact-Checking Deflated Hysteria Panic Weight");
    AssertTest(contagion.GetCarrierCount(testRumorId) >= 3, "Social Contagion Carrier Set Tracks Multi-Agent Carriers");

    // ========================================================================
    // TEST SECTION 3: ANTI-FLICKER HYSTERESIS UTILITY AI CURVES
    // ========================================================================
    UtilityAICurveEngine& utility = engine.GetUtilityEngine();

    // 1. Curve Mathematical Verification
    UtilityCurve sigCurve{ UtilityCurveType::LOGISTIC_SIGMOID, 8.0f, 0.5f, 1.0f, 0.0f };
    float sigLow = sigCurve.Evaluate(0.1f);
    float sigMid = sigCurve.Evaluate(0.5f);
    float sigHigh = sigCurve.Evaluate(0.9f);
    AssertTest(sigLow < 0.1f && sigMid >= 0.49f && sigMid <= 0.51f && sigHigh > 0.9f, "Logistic Sigmoid Curve Validated: Smooth S-Curve with Midpoint 0.50");

    UtilityCurve expCurve{ UtilityCurveType::EXPONENTIAL, 3.0f, 0.5f, 1.0f, 0.0f };
    float expLow = expCurve.Evaluate(0.2f);
    float expHigh = expCurve.Evaluate(0.8f);
    AssertTest(expLow < 0.2f && expHigh > 0.50f, "Exponential Response Curve Validated: Progressive Escalation Under Escalating Need");

    // 2. Anti-Flicker Hysteresis Dual-Threshold & Commitment Test
    AgentUtilityState utilState;
    utilState.entityId = 999;
    utilState.currentAction = EmergentActionId::CIV_IDLE_STROLL;
    utilState.currentActionDurationMs = 5000; // Prior idle stroll commitment fulfilled (>4000ms)

    BluepillCitizen testCitizen;
    testCitizen.id = 999;
    testCitizen.drives.hunger = 0.70f; // Strongly hungry
    testCitizen.drives.fatigue = 0.10f;
    testCitizen.traits.conscientiousness = 0.50f;
    testCitizen.traits.neuroticism = 0.30f;

    // First evaluation: hunger at 0.70 exceeds activation threshold (0.65) and beats current effective stroll score
    EmergentActionId act1 = utility.EvaluateCivilianAction(utilState, testCitizen, 0.0f, false, false, true, false, 1000);
    AssertTest(act1 == EmergentActionId::CIV_SEEK_FOOD, "High Hunger Triggers Transition to CIV_SEEK_FOOD");

    // Now hunger drops slightly to 0.50 (below activation 0.65, but above deactivation 0.35)
    testCitizen.drives.hunger = 0.50f;
    EmergentActionId act2 = utility.EvaluateCivilianAction(utilState, testCitizen, 0.0f, false, false, true, false, 1000);
    AssertTest(act2 == EmergentActionId::CIV_SEEK_FOOD, "Hysteresis Stickiness: Agent Does NOT Flick Back to Idle Despite Hunger Dropping Below Activation");
    AssertTest(utilState.totalSwitchesPreventedByHysteresis > 0, "Anti-Flicker Telemetry: Switch Prevented Counter Correctly Incremented");

    // Acute Danger Emergency Override: Gunfire breaks out (Danger = 0.90)
    EmergentActionId act3 = utility.EvaluateCivilianAction(utilState, testCitizen, 0.90f, false, false, true, false, 500);
    AssertTest(act3 == EmergentActionId::CIV_PANIC_FLEE, "Acute Danger Emergency Override Bypasses Action Commitment and Forces CIV_PANIC_FLEE");

    // Gang Enforcer Utility Decision
    AgentUtilityState gangState;
    gangState.entityId = 888;
    gangState.currentAction = EmergentActionId::GANG_IDLE_TURF_WATCH;

    EmergentActionId gangAct1 = utility.EvaluateGangAction(gangState, 600.0f, 200.0f, 30.0f, false, false, false, 1000);
    AssertTest(gangAct1 == EmergentActionId::GANG_AMBUSH_RIVALS, "Gang Tactical Superiority (3x Rival) Triggers GANG_AMBUSH_RIVALS");

    // Frank Castle Spotted: Emergency Survival Flee
    EmergentActionId gangAct2 = utility.EvaluateGangAction(gangState, 600.0f, 200.0f, 30.0f, true, false, false, 500);
    AssertTest(gangAct2 == EmergentActionId::GANG_FLEE_CASTLE, "Frank Castle Sighting Triggers Immediate GANG_FLEE_CASTLE Survival Reflex");

    // Police Officer Utility Decision
    AgentUtilityState copState;
    copState.entityId = 777;
    copState.currentAction = EmergentActionId::COP_ROUTINE_PATROL;

    EmergentActionId copAct1 = utility.EvaluatePoliceAction(copState, 20.0f, true, false, false, false, 1000);
    AssertTest(copAct1 == EmergentActionId::COP_INVESTIGATE_10CODE, "Clean Precinct Officer Promptly Responds to Active 10-Code Dispatch");

    // Corrupt Precinct Officer takes kickback
    EmergentActionId copAct2 = utility.EvaluatePoliceAction(copState, 85.0f, false, false, false, false, 1000);
    AssertTest(copAct2 == EmergentActionId::COP_TAKE_SYNDICATE_BRIBE, "Corrupt Harbor Precinct Officer Biased Towards COP_TAKE_SYNDICATE_BRIBE");

    // ========================================================================
    // TEST SECTION 4: TIERED LOD SIMULATION CULLING
    // ========================================================================
    TieredLODSimulationManager& lodMgr = engine.GetLODManager();
    AssertTest(!lodMgr.GetAllObservers().empty(), "Observer Focus Points Registered (Neo, Trinity, Frank Castle)");

    // Test Entity LOD Distance Classification
    float dist0 = 0.0f;
    LODTier tier0 = lodMgr.ComputeEntityLOD(LocationVector(17050.0, 95.0, 2400.0), dist0);
    AssertTest(tier0 == LODTier::LOD0_ACTIVE_VIEWPORT && dist0 < 50.0f, "Entity < 50m from Neo Classified as LOD0_ACTIVE_VIEWPORT (Full Micro-Physics)");

    float dist1 = 0.0f;
    LODTier tier1 = lodMgr.ComputeEntityLOD(LocationVector(17150.0, 95.0, 2400.0), dist1);
    AssertTest(tier1 == LODTier::LOD1_VICINITY && dist1 >= 50.0f && dist1 < 200.0f, "Entity between 50-200m Classified as LOD1_VICINITY (10 Hz Decoupled Tick)");

    float dist2 = 0.0f;
    LODTier tier2 = lodMgr.ComputeEntityLOD(LocationVector(17500.0, 95.0, 2400.0), dist2);
    AssertTest(tier2 == LODTier::LOD2_BACKGROUND && dist2 >= 200.0f && dist2 < 1000.0f, "Entity between 200-1000m Classified as LOD2_BACKGROUND (1 Hz Macro Simulation)");

    float dist3 = 0.0f;
    LODTier tier3 = lodMgr.ComputeEntityLOD(LocationVector(35000.0, 95.0, 35000.0), dist3);
    AssertTest(tier3 == LODTier::LOD3_CULLED_VIRTUAL && dist3 > 1000.0f, "Distant Entity (>1000m) Classified as LOD3_CULLED_VIRTUAL (0 Hz Dormant)");

    // Test Tick Frequency Filtering
    SimulatedEntityLODState lodState1;
    lodState1.currentTier = LODTier::LOD1_VICINITY;
    lodState1.timeSinceLastTickMs = 40;
    AssertTest(!lodMgr.ShouldEntityTick(lodState1, 20), "LOD1 Does NOT Tick at 60ms (< 100ms Budget)");
    AssertTest(lodMgr.ShouldEntityTick(lodState1, 50), "LOD1 Successfully Ticks Upon Crossing 100ms Threshold");

    // Test Invariant Analytical Catch-Up
    BluepillCitizen dormantCitizen;
    dormantCitizen.currentLocation = LocationVector(0.0, 0.0, 0.0);
    dormantCitizen.destinationLocation = LocationVector(100.0, 0.0, 0.0);
    dormantCitizen.movementSpeed = 5.0f; // 5 m/s
    dormantCitizen.drives.hunger = 0.20f;
    dormantCitizen.currentRoutine = RoutineScheduleState::Working;

    // Simulate waking up after 10 seconds of background virtualization
    lodMgr.ApplyAnalyticalCatchUp(dormantCitizen, 10000);
    AssertTest(dormantCitizen.currentLocation.x == 50.0, "Analytical Catch-Up: Position Advanced Smoothly by 50m (5 m/s * 10s)");
    AssertTest(dormantCitizen.drives.hunger > 0.20f, "Analytical Catch-Up: Hunger Accumulated Analytically During Background Sleep");

    // Rapid Observer Teleportation Edge Case
    SimulatedEntityLODState teleportTestEntity;
    teleportTestEntity.entityId = 444;
    teleportTestEntity.currentTier = LODTier::LOD0_ACTIVE_VIEWPORT;
    LocationVector fixedCitizenPos(17045.0, 95.0, 2400.0); // Near Neo

    // Step 1: Neo teleports 90,000 units away into Richland
    lodMgr.UpdateObserverPosition(1, LocationVector(95000.0, 95.0, -95000.0));
    lodMgr.UpdateEntityLOD(teleportTestEntity, fixedCitizenPos);
    AssertTest(teleportTestEntity.currentTier == LODTier::LOD3_CULLED_VIRTUAL, "Observer Rapid Teleportation Away: Entity Directly Demoted to LOD3_CULLED_VIRTUAL");

    // Entity accumulates virtual time while dormant
    lodMgr.ShouldEntityTick(teleportTestEntity, 8000);
    AssertTest(teleportTestEntity.totalAccumulatedVirtualMs == 8000, "Dormant Culled Entity Accumulated Virtual Elapsed Milliseconds");

    // Step 2: Neo instantly teleports back into Downtown vicinity
    lodMgr.UpdateObserverPosition(1, LocationVector(17043.0, 95.0, 2398.0));
    lodMgr.UpdateEntityLOD(teleportTestEntity, fixedCitizenPos);
    AssertTest(teleportTestEntity.currentTier == LODTier::LOD0_ACTIVE_VIEWPORT, "Observer Rapid Teleportation Back: Entity Instantly Promoted to LOD0_ACTIVE_VIEWPORT");
    AssertTest(teleportTestEntity.needsStateCatchup, "Multi-Tier Jump from Culled Tier Successfully Flagged needsStateCatchup");

    // ========================================================================
    // TEST SECTION 5: DEEP MULTI-AGENT CROSS-SYSTEM COLLISIONS
    // ========================================================================
    if (sCityLifeMgr.GetTotalCitizenCount() == 0) {
        sCityLifeMgr.Initialize();
    }
    // Position a conscientious citizen near the Slums callbox and crime scene
    BluepillCitizen* c1 = sCityLifeMgr.GetCitizen(1);
    if (c1) {
        c1->districtId = 1;
        c1->currentLocation = LocationVector(1510.0, 95.0, -3210.0);
        c1->traits.conscientiousness = 0.90f;
        c1->traits.neuroticism = 0.20f;
        c1->currentRoutine = RoutineScheduleState::Shopping;
    }

    uint32 preCrossEvents = engine.GetCrossSystemEventsTriggered();

    // Trigger emergent crime in Slums
    engine.OnEmergentCrimeDetected(404, 1, LocationVector(1500.0, 95.0, -3200.0), "Armed Mugging at Pier 44 Alley");
    AssertTest(engine.GetCrossSystemEventsTriggered() > preCrossEvents, "Emergent Crime Generated Cross-System Event Cascade");
    AssertTest(engine.GetCrimesReportedViaCallboxes() > 0, "High-Conscientiousness Citizen Used Smart Callbox to Report Crime to 911");

    // Frank Castle Ambush Execution
    engine.OnFrankCastleAmbushExecuted(LocationVector(1500.0, 95.0, -3200.0), "Marcone Contraband Cache", true);
    AssertTest(engine.GetContagionEngine().GetTotalRumorCount() >= 4, "Frank Castle Strike Injected New Rumor into Social Contagion Network");

    // Test Integrated UpdateManagedCitizens Execution
    engine.UpdateManagedCitizens(1000);
    AssertTest(engine.GetOrCreateLODState(1) != nullptr, "Managed Citizens Loop: Citizen #1 LOD & Utility States Successfully Processed");

    // State Persistence Round-Trip
    bool saved = engine.SaveEmergentStateToFile("EmergentAISimulation_Test.json");
    AssertTest(saved, "Emergent AI Engine State Successfully Serialized to JSON");

    bool loaded = engine.LoadEmergentStateFromFile("EmergentAISimulation_Test.json");
    AssertTest(loaded, "Emergent AI Engine State Successfully Deserialized from JSON");

    // Telemetry Reporting
    std::string report = engine.GenerateEmergentTelemetryReport();
    AssertTest(!report.empty() && report.find("EMERGENT AI ENGINE") != std::string::npos, "Master Telemetry Report Generated with Complete Multi-Tier Metrics");

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  EMERGENT AI TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    assert(failed == 0);
}

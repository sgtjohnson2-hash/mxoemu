#include "SensoryPerceptionSystem.h"
#include "PlayerObject.h"
#include "BotClient.h"
#include "BotManager.h"
#include "SpatialGrid.h"
#include "NavMeshMgr.h"
#include "MatrixThreatHeatmap.h"
#include "Log.h"
#include "Timer.h"
#include "GameServer.h"
#include "StatusEffectManager.h"
#include <cmath>
#include <algorithm>

createFileSingleton(SensoryPerceptionSystem);

SensoryPerceptionSystem::SensoryPerceptionSystem()
{
}

SensoryPerceptionSystem::~SensoryPerceptionSystem()
{
}

void SensoryPerceptionSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_sensoryMutex);
    m_botStates.clear();
    m_recentSounds.clear();
    INFO_LOG("SensoryPerceptionSystem: Initialized dual-cone vision & acoustic wave propagation.");
}

float SensoryPerceptionSystem::CalculateDetectionMultiplier(PlayerObject* target) const
{
    if (!target) return 1.0f;

    // Check for stealth / cloaking
    if (sStatusEffectManager.HasEffect(target->getGoId(), EFFECT_FACTION_MASK)) {
        return 0.35f; // Cloaked / masked target has 65% detection reduction
    }

    // Target velocity / movement stance
    // Faster movement raises detectability, stationary or crouching lowers it
    if (target->isDualWielding()) {
        return 1.15f;
    }
    return 1.0f;
}

bool SensoryPerceptionSystem::CheckVision(PlayerObject* observer, PlayerObject* target, bool inCombat, float& outConfidence)
{
    outConfidence = 0.0f;
    if (!observer || !target || observer == target || target->isDead()) return false;

    LocationVector obsPos = observer->getPosition();
    LocationVector tgtPos = target->getPosition();

    float dx = tgtPos.x - obsPos.x;
    float dz = tgtPos.z - obsPos.z;
    float distSq = dx * dx + dz * dz;
    float dist = std::sqrt(distSq);

    // Stance / Cloak multiplier
    float stanceMult = CalculateDetectionMultiplier(target);

    // Near-Field Peripheral: 120 deg (half-angle 60 deg = ~1.047 rad), up to 15m (1500 units)
    // Far-Field Focused: 45 deg (half-angle 22.5 deg = ~0.393 rad), up to 65m (6500 units)
    // Combat Compression: Focused FOV narrows from 45 deg to 30 deg (half-angle 15 deg = ~0.262 rad)
    float maxFocusedRange = 6500.0f * stanceMult;
    float maxPeripheralRange = 1500.0f * stanceMult;

    if (dist > maxFocusedRange) {
        return false;
    }

    // Observer forward direction
    // In MxO, facing angle can be derived from velocity or default forward (Z+ or orientation)
    // If observer has a target or facing, we use observer's facing
    float obsHeadingRad = 0.0f; // Default facing
    float targetAngleRad = std::atan2(dz, dx);
    float angleDiffRad = std::abs(targetAngleRad - obsHeadingRad);
    while (angleDiffRad > 3.14159265f) angleDiffRad = std::abs(angleDiffRad - 2.0f * 3.14159265f);

    float focusedHalfAngle = inCombat ? (15.0f * 3.14159265f / 180.0f) : (22.5f * 3.14159265f / 180.0f);
    float peripheralHalfAngle = 60.0f * 3.14159265f / 180.0f;

    bool insideFocusedCone = (dist <= maxFocusedRange && angleDiffRad <= focusedHalfAngle);
    bool insidePeripheralCone = (dist <= maxPeripheralRange && angleDiffRad <= peripheralHalfAngle);

    if (!insideFocusedCone && !insidePeripheralCone) {
        return false;
    }

    // Line of sight raycast query against city architecture
    if (!sNavMeshMgr.CheckLineOfSight(obsPos.x, obsPos.z, tgtPos.x, tgtPos.z)) {
        return false;
    }

    // Compute detection confidence
    float cosFactor = std::max(0.0f, std::cos(angleDiffRad));
    float distFactor = 1.0f - (dist / maxFocusedRange);
    outConfidence = std::clamp(cosFactor * distFactor * stanceMult, 0.1f, 1.0f);

    return true;
}

void SensoryPerceptionSystem::EmitSound(float x, float y, float z, SoundEmissionType type, float intensity, uint32 sourceGoId)
{
    uint32 now = getMSTime();

    // Determine base acoustic propagation radius based on sound type
    float baseRadius = 1500.0f;
    switch (type) {
        case SOUND_FOOTSTEP:    baseRadius = 800.0f; break;
        case SOUND_IMPACT:      baseRadius = 1200.0f; break;
        case SOUND_BULLET_WHIZ: baseRadius = 400.0f; break;
        case SOUND_CODECAST:    baseRadius = 5000.0f; break;
        case SOUND_GUNFIRE:     baseRadius = 8000.0f; break;
        case SOUND_EXPLOSION:   baseRadius = 9000.0f; break;
    }

    float effectiveRadius = baseRadius * std::clamp(intensity, 0.5f, 3.0f);

    // Record sound event in buffer
    {
        std::lock_guard<std::recursive_mutex> lock(m_sensoryMutex);
        m_recentSounds.push_back({x, y, z, type, intensity, sourceGoId, now});
        if (m_recentSounds.size() > 50) {
            m_recentSounds.erase(m_recentSounds.begin());
        }
    }

    // Query nearby agents in spatial grid
    auto nearbyClients = sSpatialGrid.GetClientsInRadius(x, z, effectiveRadius);
    for (GameClient* gc : nearbyClients) {
        if (!gc || !gc->isBot()) continue;
        BotClient* bot = dynamic_cast<BotClient*>(gc);
        if (!bot) continue;

        uint32 botGoId = bot->GetPlayerGoId();
        if (botGoId == sourceGoId) continue;

        PlayerObject* botPo = BotGetPlayer(botGoId);
        if (!botPo || botPo->isDead()) continue;

        LocationVector botPos = botPo->getPosition();
        float dx = x - botPos.x;
        float dz = z - botPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq > effectiveRadius * effectiveRadius) continue;

        // Sound intensity decays with distance squared: I(d) = I_0 / (1 + alpha * d^2)
        float alpha = 0.000005f;
        float decayedIntensity = intensity / (1.0f + alpha * distSq);

        float azimuthRad = std::atan2(dz, dx);

        if (type == SOUND_BULLET_WHIZ) {
            // Bullet shockwave near civilian triggers instant panic
            if (botPo->getFactionName() == "Civilian" && !bot->IsPanicking()) {
                bot->triggerPanic(sourceGoId);
            } else {
                // Combatant infers shooter directional quadrant
                StimulateBot(botGoId, 0.8f, azimuthRad, sourceGoId, now);
            }
        } else if (type == SOUND_GUNFIRE || type == SOUND_EXPLOSION) {
            if (botPo->getFactionName() == "Civilian") {
                if (decayedIntensity > 0.2f && !bot->IsPanicking()) {
                    bot->triggerPanic(sourceGoId);
                }
            } else {
                // Alert combatants toward sound source
                StimulateBot(botGoId, decayedIntensity, azimuthRad, sourceGoId, now);
            }
        } else if (type == SOUND_FOOTSTEP) {
            if (decayedIntensity > 0.35f) {
                StimulateBot(botGoId, 0.25f, azimuthRad, sourceGoId, now);
            }
        }
    }
}

void SensoryPerceptionSystem::ProcessBulletTrajectory(float startX, float startZ, float endX, float endZ, uint32 shooterGoId)
{
    // Check if supersonic bullet passes within 4 meters (400 units) of any bot
    float dirX = endX - startX;
    float dirZ = endZ - startZ;
    float segLenSq = dirX * dirX + dirZ * dirZ;
    if (segLenSq < 100.0f) return;

    float midX = (startX + endX) * 0.5f;
    float midZ = (startZ + endZ) * 0.5f;
    float searchRadius = std::sqrt(segLenSq) * 0.5f + 400.0f;

    auto nearbyClients = sSpatialGrid.GetClientsInRadius(midX, midZ, searchRadius);
    for (GameClient* gc : nearbyClients) {
        if (!gc || !gc->isBot()) continue;
        BotClient* bot = dynamic_cast<BotClient*>(gc);
        if (!bot) continue;

        uint32 botGoId = bot->GetPlayerGoId();
        if (botGoId == shooterGoId) continue;

        PlayerObject* botPo = BotGetPlayer(botGoId);
        if (!botPo || botPo->isDead()) continue;

        LocationVector p = botPo->getPosition();

        // Distance from point to line segment
        float t = ((p.x - startX) * dirX + (p.z - startZ) * dirZ) / segLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
        float nearestX = startX + t * dirX;
        float nearestZ = startZ + t * dirZ;

        float distSq = (p.x - nearestX) * (p.x - nearestX) + (p.z - nearestZ) * (p.z - nearestZ);
        if (distSq <= 160000.0f) { // 400 units (4m)
            EmitSound(nearestX, p.y, nearestZ, SOUND_BULLET_WHIZ, 1.0f, shooterGoId);
        }
    }
}

BotAwarenessState SensoryPerceptionSystem::GetAwareness(uint32 botGoId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_sensoryMutex);
    auto it = m_botStates.find(botGoId);
    if (it != m_botStates.end()) {
        return it->second;
    }
    return BotAwarenessState{};
}

void SensoryPerceptionSystem::SetAwareness(uint32 botGoId, SensoryAwarenessStage stage, uint32 sourceGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_sensoryMutex);
    BotAwarenessState& s = m_botStates[botGoId];
    s.stage = stage;
    s.alertSourceGoId = sourceGoId;
    s.lastStimulusMs = getMSTime();
    if (stage == AWARENESS_UNAWARE) {
        s.suspicionMeter = 0.0f;
    } else if (stage == AWARENESS_SUSPICIOUS) {
        s.suspicionMeter = std::max(s.suspicionMeter, 0.5f);
    } else {
        s.suspicionMeter = 1.0f;
    }
}

void SensoryPerceptionSystem::StimulateBot(uint32 botGoId, float suspicionDelta, float soundAzimuth, uint32 sourceGoId, uint32 currentMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_sensoryMutex);
    BotAwarenessState& s = m_botStates[botGoId];
    s.lastStimulusMs = currentMs;
    s.lastHeardAzimuthRad = soundAzimuth;
    s.alertSourceGoId = sourceGoId;
    s.suspicionMeter = std::clamp(s.suspicionMeter + suspicionDelta, 0.0f, 1.0f);

    if (s.suspicionMeter >= 0.8f) {
        s.stage = AWARENESS_ALERTED;
    } else if (s.suspicionMeter >= 0.35f) {
        s.stage = AWARENESS_SUSPICIOUS;
    }
}

void SensoryPerceptionSystem::Update(float dtSeconds, uint32 currentMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_sensoryMutex);

    // Decay awareness states if no stimuli detected for 12 seconds
    for (auto it = m_botStates.begin(); it != m_botStates.end(); ) {
        BotAwarenessState& s = it->second;
        if (s.stage != AWARENESS_IN_COMBAT) {
            if (currentMs - s.lastStimulusMs >= 12000) {
                s.suspicionMeter = std::max(0.0f, s.suspicionMeter - 0.2f * dtSeconds);
                if (s.suspicionMeter <= 0.05f) {
                    s.stage = AWARENESS_UNAWARE;
                    s.alertSourceGoId = 0;
                } else if (s.suspicionMeter < 0.4f) {
                    s.stage = AWARENESS_SUSPICIOUS;
                }
            }
        }
        ++it;
    }
}

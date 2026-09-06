#ifndef MXOEMU_SENSORY_PERCEPTION_SYSTEM_H
#define MXOEMU_SENSORY_PERCEPTION_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>

class PlayerObject;
class BotClient;

enum SensoryAwarenessStage : uint8
{
    AWARENESS_UNAWARE = 0,
    AWARENESS_SUSPICIOUS = 1,
    AWARENESS_ALERTED = 2,
    AWARENESS_IN_COMBAT = 3
};

enum SoundEmissionType : uint8
{
    SOUND_FOOTSTEP = 0,
    SOUND_GUNFIRE = 1,
    SOUND_IMPACT = 2,
    SOUND_BULLET_WHIZ = 3,
    SOUND_CODECAST = 4,
    SOUND_EXPLOSION = 5
};

struct AcousticEvent
{
    float x;
    float y;
    float z;
    SoundEmissionType type;
    float initialIntensity;
    uint32 sourceGoId;
    uint32 timestampMs;
};

struct BotAwarenessState
{
    SensoryAwarenessStage stage{AWARENESS_UNAWARE};
    float suspicionMeter{0.0f}; // 0.0 to 1.0
    uint32 lastStimulusMs{0};
    float lastHeardAzimuthRad{0.0f};
    uint32 alertSourceGoId{0};
};

class SensoryPerceptionSystem : public Singleton<SensoryPerceptionSystem>
{
public:
    SensoryPerceptionSystem();
    ~SensoryPerceptionSystem();

    void Initialize();
    void Update(float dtSeconds, uint32 currentMs);

    // Dual-Cone Vision: Near Peripheral (120 deg, 15m) & Far Focused (45 deg, 65m; 30 deg in combat)
    bool CheckVision(PlayerObject* observer, PlayerObject* target, bool inCombat, float& outConfidence);

    // Acoustic Wave Propagation
    void EmitSound(float x, float y, float z, SoundEmissionType type, float intensity, uint32 sourceGoId);

    // Awareness State Machine
    BotAwarenessState GetAwareness(uint32 botGoId) const;
    void SetAwareness(uint32 botGoId, SensoryAwarenessStage stage, uint32 sourceGoId = 0);
    void StimulateBot(uint32 botGoId, float suspicionDelta, float soundAzimuth, uint32 sourceGoId, uint32 currentMs);

    // Supersonic bullet whiz trajectory check (< 4m)
    void ProcessBulletTrajectory(float startX, float startZ, float endX, float endZ, uint32 shooterGoId);

private:
    float CalculateDetectionMultiplier(PlayerObject* target) const;

    mutable std::recursive_mutex m_sensoryMutex;
    std::unordered_map<uint32, BotAwarenessState> m_botStates;
    std::vector<AcousticEvent> m_recentSounds;
};

#define sSensoryPerception SensoryPerceptionSystem::getSingleton()

#endif // MXOEMU_SENSORY_PERCEPTION_SYSTEM_H

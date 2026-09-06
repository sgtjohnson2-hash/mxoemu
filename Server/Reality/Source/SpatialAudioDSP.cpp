#include "SpatialAudioDSP.h"
#include "StaticObjectManager.h"
#include <algorithm>

createFileSingleton(SpatialAudioDSP);

SpatialAudioDSP::SpatialAudioDSP()
    : m_bulletTimeActive(false), m_timeDilation(1.0f), m_subsonicResonanceHz(120.0f)
{
}

SpatialAudioDSP::~SpatialAudioDSP()
{
}

SpatialAudioFrame SpatialAudioDSP::ComputeSpatialAcoustics(
    float listenerX, float listenerY, float listenerZ, float listenerHeadingRad,
    float sourceX, float sourceY, float sourceZ,
    float referenceDist, float maxDist)
{
    std::lock_guard<std::mutex> lock(m_dspMutex);

    SpatialAudioFrame frame;
    float dx = sourceX - listenerX;
    float dy = sourceY - listenerY;
    float dz = sourceZ - listenerZ;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    // 1. Inverse-square distance attenuation clamped to maxDist
    float clampedDist = std::clamp(dist, referenceDist, maxDist);
    float attenuation = referenceDist / clampedDist;

    // 2. Relative Azimuth Angle Calculation
    float angleToSource = std::atan2(dx, dz);
    float relativeAngle = angleToSource - listenerHeadingRad;
    while (relativeAngle > 3.14159265f) relativeAngle -= 6.2831853f;
    while (relativeAngle < -3.14159265f) relativeAngle += 6.2831853f;

    // 3. HRTF Binaural Pan (Interaural Level Difference - ILD)
    float pan = std::sin(relativeAngle); // -1 = full left, +1 = full right
    frame.leftGain = attenuation * (0.5f * (1.0f - pan));
    frame.rightGain = attenuation * (0.5f * (1.0f + pan));

    // Interaural Time Delay (ITD) in milliseconds (max ~0.65ms across human head)
    frame.delayMs = std::abs(pan) * 0.65f;

    // 4. Urban Geometry Occlusion Damping (StaticObjectManager)
    bool occluded = !sStaticObjMgr.CheckLineOfSight(listenerX, listenerY, listenerZ, sourceX, sourceY, sourceZ);
    if (occluded)
    {
        frame.lowPassCutoffHz = 900.0f; // Low-pass muffled by brick/concrete walls
        frame.leftGain *= 0.45f;
        frame.rightGain *= 0.45f;
    }
    else
    {
        frame.lowPassCutoffHz = 20000.0f; // Full spectrum unoccluded
    }

    // 5. Bullet-Time Subsonic Dilation Adjustment
    if (m_bulletTimeActive)
    {
        frame.lowPassCutoffHz = std::min(frame.lowPassCutoffHz, m_subsonicResonanceHz); // Subsonic bass resonance
        frame.dopplerRatio = m_timeDilation;
    }
    else
    {
        frame.dopplerRatio = 1.0f;
    }

    return frame;
}

void SpatialAudioDSP::SetBulletTimeActive(bool active, float timeScale)
{
    std::lock_guard<std::mutex> lock(m_dspMutex);
    m_bulletTimeActive = active;
    m_timeDilation = active ? std::clamp(timeScale, 0.1f, 1.0f) : 1.0f;
}

float SpatialAudioDSP::GetPitchShiftFactor() const
{
    std::lock_guard<std::mutex> lock(m_dspMutex);
    return m_bulletTimeActive ? m_timeDilation : 1.0f;
}

float SpatialAudioDSP::GetSubsonicCutoffFrequency() const
{
    std::lock_guard<std::mutex> lock(m_dspMutex);
    return m_bulletTimeActive ? m_subsonicResonanceHz : 20000.0f;
}

float SpatialAudioDSP::ComputeDopplerRatio(float sourceVelocity, float soundSpeed, bool approaching) const
{
    std::lock_guard<std::mutex> lock(m_dspMutex);
    if (soundSpeed <= 0.0f) soundSpeed = 343.0f;

    float effectiveSpeed = soundSpeed;
    if (m_bulletTimeActive)
    {
        effectiveSpeed *= m_timeDilation; // Sound wave prop slowed in bullet time
    }

    if (approaching)
    {
        return effectiveSpeed / std::max(10.0f, effectiveSpeed - sourceVelocity);
    }
    else
    {
        return effectiveSpeed / (effectiveSpeed + sourceVelocity);
    }
}

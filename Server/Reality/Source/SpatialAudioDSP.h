#ifndef MXOEMU_SPATIAL_AUDIO_DSP_H
#define MXOEMU_SPATIAL_AUDIO_DSP_H

#include "Common.h"
#include "Singleton.h"
#include <mutex>
#include <cmath>

struct SpatialAudioFrame
{
    float leftGain;
    float rightGain;
    float delayMs;
    float lowPassCutoffHz;
    float dopplerRatio;
};

class SpatialAudioDSP : public Singleton<SpatialAudioDSP>
{
public:
    SpatialAudioDSP();
    ~SpatialAudioDSP();

    // 3D Spatial Acoustic Convolution
    SpatialAudioFrame ComputeSpatialAcoustics(
        float listenerX, float listenerY, float listenerZ, float listenerHeadingRad,
        float sourceX, float sourceY, float sourceZ,
        float referenceDist = 500.0f, float maxDist = 30000.0f);

    // Bullet-Time Subsonic Time-Dilation DSP
    void SetBulletTimeActive(bool active, float timeScale = 0.35f);
    bool IsBulletTimeActive() const { return m_bulletTimeActive; }
    float GetTimeDilationFactor() const { return m_timeDilation; }
    float GetPitchShiftFactor() const;
    float GetSubsonicCutoffFrequency() const;

    // Supersonic Projectile Doppler Effect
    float ComputeDopplerRatio(float sourceVelocity, float soundSpeed = 343.0f, bool approaching = true) const;

private:
    mutable std::mutex m_dspMutex;
    bool m_bulletTimeActive{false};
    float m_timeDilation{1.0f};
    float m_subsonicResonanceHz{80.0f};
};

#define sSpatialAudioDSP SpatialAudioDSP::getSingleton()

#endif // MXOEMU_SPATIAL_AUDIO_DSP_H

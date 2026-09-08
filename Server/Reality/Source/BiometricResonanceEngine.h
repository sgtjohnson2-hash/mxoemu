#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// The Matrix Omniverse: Epoch VII - Pillar III: Biometric Resonance Engine
// Brain-Computer Interface (BCI / EEG) & Lab Streaming Layer (LSL) Telemetry
// ============================================================================

enum class EEGChannel
{
    Fp1 = 0,    // Left Frontal
    Fp2,        // Right Frontal
    C3,         // Left Central (Motor Cortex)
    C4,         // Right Central (Motor Cortex)
    P7,         // Left Parietal
    P8,         // Right Parietal
    O1,         // Left Occipital (Visual)
    O2,         // Right Occipital (Visual)
    Count
};

struct EEGBandPower
{
    float delta{0.0f};  // 0.5 - 4 Hz (Deep Stasis / Unconscious)
    float theta{0.0f};  // 4 - 8 Hz (Subconscious / Stealth)
    float alpha{0.0f};  // 8 - 12 Hz (Calm Focus / The One Awakening)
    float beta{0.0f};   // 13 - 30 Hz (Active Combat / Aim Precision)
    float gamma{0.0f};  // > 30 Hz (Peak Hyper-Focus / Bullet-Time Dilation)
};

struct BiometricState
{
    uint32_t playerId{0};
    bool hasHardwareStream{false};
    float heartRateBpm{72.0f};
    float heartRateVariabilityMs{50.0f};
    EEGBandPower bandPowers;
    float bulletTimeDilation{1.0f};    // 1.0 (normal) down to 0.1 (extreme bullet time)
    float scalarStasisRadiusMul{1.0f}; // 1.0x to 2.5x
    float staminaRegenRateMul{1.0f};   // Modulated by HRV & Alpha
    float weaponAimJitterMul{1.0f};    // Reduced by high Beta/Alpha focus
    uint64_t lastSampleTimestampMs{0};
};

class BiometricResonanceEngine : public Singleton<BiometricResonanceEngine>
{
public:
    BiometricResonanceEngine();
    ~BiometricResonanceEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Client Registration & Telemetry Ingestion
    void RegisterPlayer(uint32_t playerId);
    void UnregisterPlayer(uint32_t playerId);
    bool HasPlayer(uint32_t playerId) const;

    // 2. Raw EEG Stream Processing (LSL Packet Format: channel data sampled at 250Hz)
    void IngestRawEEGSample(uint32_t playerId, const std::vector<float>& channelSamples, uint64_t timestampMs);
    void IngestCardiacSample(uint32_t playerId, float bpm, float hrvMs);

    // 3. Mathematical Power Spectral Density (PSD) Extraction
    // Computes Discrete Fourier Transform on windowed buffer and integrates frequency bands
    static EEGBandPower ComputeSpectralBands(const std::vector<float>& timeSeries, float sampleRateHz = 256.0f);

    // 4. Autonomous Synthetic Biometric Generator (Fallback when no hardware headset connected)
    void GenerateSyntheticBiometrics(uint32_t playerId, float combatStressLevel, float dt);

    // 5. Gameplay Mechanics Calculation
    const BiometricState* GetPlayerBiometrics(uint32_t playerId) const;
    float GetBulletTimeDilation(uint32_t playerId) const;
    float GetScalarStasisRadiusMultiplier(uint32_t playerId) const;
    float GetStaminaRegenMultiplier(uint32_t playerId) const;
    float GetAimPrecisionMultiplier(uint32_t playerId) const;

    // 6. Bulk Diagnostics
    size_t GetActiveSubjectCount() const;

private:
    void RecalculateGameplayEffects(BiometricState& state);

    mutable std::shared_mutex m_biometricMutex;
    std::unordered_map<uint32_t, BiometricState> m_playerStates;
    std::unordered_map<uint32_t, std::vector<float>> m_rawSampleBuffers;
};

#define sBiometricResonanceEngine BiometricResonanceEngine::getSingleton()

void RunBiometricResonanceTestSuite();

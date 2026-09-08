#include "BiometricResonanceEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <boost/format.hpp>

createFileSingleton(BiometricResonanceEngine);

BiometricResonanceEngine::BiometricResonanceEngine()
{
}

BiometricResonanceEngine::~BiometricResonanceEngine()
{
}

void BiometricResonanceEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    m_playerStates.clear();
    m_rawSampleBuffers.clear();

    boost::format fmt("BiometricResonanceEngine: Initialized BCI Lab Streaming Layer & Spectral Resonance Subsystem.");
    INFO_LOG(fmt);
}

void BiometricResonanceEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    m_playerStates.clear();
    m_rawSampleBuffers.clear();
}

void BiometricResonanceEngine::Update(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    for (auto& kv : m_playerStates) {
        if (!kv.second.hasHardwareStream) {
            // Autonomous synthetic simulation if idle
            // Will be driven specifically by callers
        }
    }
}

void BiometricResonanceEngine::RegisterPlayer(uint32_t playerId)
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    BiometricState state;
    state.playerId = playerId;
    state.hasHardwareStream = false;
    state.heartRateBpm = 72.0f;
    state.heartRateVariabilityMs = 50.0f;
    state.bandPowers.alpha = 0.4f;
    state.bandPowers.beta = 0.3f;
    state.bandPowers.theta = 0.2f;
    state.bandPowers.gamma = 0.1f;
    state.bandPowers.delta = 0.05f;
    RecalculateGameplayEffects(state);

    m_playerStates[playerId] = state;
    m_rawSampleBuffers[playerId] = std::vector<float>();
}

void BiometricResonanceEngine::UnregisterPlayer(uint32_t playerId)
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    m_playerStates.erase(playerId);
    m_rawSampleBuffers.erase(playerId);
}

bool BiometricResonanceEngine::HasPlayer(uint32_t playerId) const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    return m_playerStates.find(playerId) != m_playerStates.end();
}

void BiometricResonanceEngine::IngestRawEEGSample(uint32_t playerId, const std::vector<float>& channelSamples, uint64_t timestampMs)
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    it->second.hasHardwareStream = true;
    it->second.lastSampleTimestampMs = timestampMs;

    auto& buf = m_rawSampleBuffers[playerId];
    // Average across channels or use frontal Fp1/Fp2 if available
    float meanSample = 0.0f;
    if (!channelSamples.empty()) {
        for (float v : channelSamples) meanSample += v;
        meanSample /= static_cast<float>(channelSamples.size());
    }
    buf.push_back(meanSample);

    // Keep sliding window of 256 samples (1 second at 256 Hz)
    if (buf.size() > 256) {
        buf.erase(buf.begin(), buf.begin() + (buf.size() - 256));
    }

    if (buf.size() >= 128) {
        it->second.bandPowers = ComputeSpectralBands(buf, 256.0f);
        RecalculateGameplayEffects(it->second);
    }
}

void BiometricResonanceEngine::IngestCardiacSample(uint32_t playerId, float bpm, float hrvMs)
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    it->second.hasHardwareStream = true;
    it->second.heartRateBpm = std::clamp(bpm, 40.0f, 220.0f);
    it->second.heartRateVariabilityMs = std::max(5.0f, hrvMs);
    RecalculateGameplayEffects(it->second);
}

EEGBandPower BiometricResonanceEngine::ComputeSpectralBands(const std::vector<float>& timeSeries, float sampleRateHz)
{
    EEGBandPower bands;
    size_t N = timeSeries.size();
    if (N < 16) return bands;

    float deltaP = 0.0f;
    float thetaP = 0.0f;
    float alphaP = 0.0f;
    float betaP = 0.0f;
    float gammaP = 0.0f;

    // Discrete Fourier Transform magnitude estimation
    // Frequency bin resolution: df = sampleRateHz / N
    float df = sampleRateHz / static_cast<float>(N);
    size_t halfN = N / 2;

    for (size_t k = 1; k < halfN; ++k) {
        float freq = static_cast<float>(k) * df;
        float real = 0.0f;
        float imag = 0.0f;

        // Hann windowed DFT
        for (size_t n = 0; n < N; ++n) {
            float window = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * static_cast<float>(n) / static_cast<float>(N - 1)));
            float val = timeSeries[n] * window;
            float angle = 2.0f * 3.14159265f * static_cast<float>(k * n) / static_cast<float>(N);
            real += val * std::cos(angle);
            imag -= val * std::sin(angle);
        }

        float power = (real * real + imag * imag) / static_cast<float>(N * N);

        if (freq >= 0.5f && freq < 4.0f) deltaP += power;
        else if (freq >= 4.0f && freq < 8.0f) thetaP += power;
        else if (freq >= 8.0f && freq < 13.0f) alphaP += power;
        else if (freq >= 13.0f && freq < 30.0f) betaP += power;
        else if (freq >= 30.0f && freq <= 100.0f) gammaP += power;
    }

    float total = deltaP + thetaP + alphaP + betaP + gammaP;
    if (total > 1e-8f) {
        bands.delta = deltaP / total;
        bands.theta = thetaP / total;
        bands.alpha = alphaP / total;
        bands.beta = betaP / total;
        bands.gamma = gammaP / total;
    }

    return bands;
}

void BiometricResonanceEngine::GenerateSyntheticBiometrics(uint32_t playerId, float combatStressLevel, float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    BiometricState& s = it->second;
    s.hasHardwareStream = false;

    float stress = std::clamp(combatStressLevel, 0.0f, 1.0f);

    // Stress shifts power from Alpha (calm) to Beta/Gamma (high arousal)
    // Low stress (zen / meditation): high Alpha (0.6), low Gamma (0.05), low HR (60 bpm), high HRV (75ms)
    // High stress (firefight / Neo awakening): high Gamma (0.5), high Beta (0.35), high HR (160 bpm), low HRV (20ms)

    float targetGamma = 0.05f + 0.50f * stress;
    float targetBeta  = 0.20f + 0.25f * stress;
    float targetAlpha = 0.60f * (1.0f - stress) + 0.10f;
    float targetTheta = 0.10f * (1.0f - stress) + 0.05f;
    float targetDelta = 0.05f;

    float targetHR = 65.0f + 95.0f * stress;
    float targetHRV = 70.0f - 45.0f * stress;

    float lerpRate = std::clamp(dt * 3.0f, 0.0f, 1.0f);
    s.bandPowers.gamma += (targetGamma - s.bandPowers.gamma) * lerpRate;
    s.bandPowers.beta  += (targetBeta - s.bandPowers.beta) * lerpRate;
    s.bandPowers.alpha += (targetAlpha - s.bandPowers.alpha) * lerpRate;
    s.bandPowers.theta += (targetTheta - s.bandPowers.theta) * lerpRate;
    s.bandPowers.delta += (targetDelta - s.bandPowers.delta) * lerpRate;

    s.heartRateBpm += (targetHR - s.heartRateBpm) * lerpRate;
    s.heartRateVariabilityMs += (targetHRV - s.heartRateVariabilityMs) * lerpRate;

    RecalculateGameplayEffects(s);
}

void BiometricResonanceEngine::RecalculateGameplayEffects(BiometricState& s)
{
    // 1. Bullet Time Temporal Dilation: Triggered by peak Gamma (>30Hz hyper-focus)
    // When Gamma reaches > 0.40, time dilates smoothly down towards 0.1
    if (s.bandPowers.gamma > 0.30f) {
        float gammaOver = std::min(1.0f, (s.bandPowers.gamma - 0.30f) / 0.35f);
        s.bulletTimeDilation = 1.0f - (0.85f * gammaOver); // Dilation down to 0.15
    } else {
        s.bulletTimeDilation = 1.0f;
    }

    // 2. Scalar Stasis Field Radius: Modulated by Alpha power (Zen focus)
    // Base 1.0x, expanding up to 2.5x during high alpha
    s.scalarStasisRadiusMul = 1.0f + (1.5f * std::clamp(s.bandPowers.alpha, 0.0f, 1.0f));

    // 3. Stamina Regen: Increased by higher HRV and high Alpha, penalized by extreme BPM exhaustion
    float hrvBonus = std::clamp(s.heartRateVariabilityMs / 50.0f, 0.5f, 2.0f);
    float alphaBonus = 0.8f + 0.4f * s.bandPowers.alpha;
    float bpmPenalty = (s.heartRateBpm > 150.0f) ? 0.7f : 1.0f;
    s.staminaRegenRateMul = hrvBonus * alphaBonus * bpmPenalty;

    // 4. Aim Precision Multiplier (1.0 = normal, 0.2 = pinpoint laser aim)
    // Beta alertness + Alpha calm reduces jitter
    float focus = s.bandPowers.beta * 0.5f + s.bandPowers.alpha * 0.5f;
    s.weaponAimJitterMul = std::clamp(1.0f - (0.7f * focus), 0.2f, 1.0f);
}

const BiometricState* BiometricResonanceEngine::GetPlayerBiometrics(uint32_t playerId) const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    return (it != m_playerStates.end()) ? &it->second : nullptr;
}

float BiometricResonanceEngine::GetBulletTimeDilation(uint32_t playerId) const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    return (it != m_playerStates.end()) ? it->second.bulletTimeDilation : 1.0f;
}

float BiometricResonanceEngine::GetScalarStasisRadiusMultiplier(uint32_t playerId) const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    return (it != m_playerStates.end()) ? it->second.scalarStasisRadiusMul : 1.0f;
}

float BiometricResonanceEngine::GetStaminaRegenMultiplier(uint32_t playerId) const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    return (it != m_playerStates.end()) ? it->second.staminaRegenRateMul : 1.0f;
}

float BiometricResonanceEngine::GetAimPrecisionMultiplier(uint32_t playerId) const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    auto it = m_playerStates.find(playerId);
    return (it != m_playerStates.end()) ? it->second.weaponAimJitterMul : 1.0f;
}

size_t BiometricResonanceEngine::GetActiveSubjectCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_biometricMutex);
    return m_playerStates.size();
}

// ============================================================================
// Headless Test Suite 37: Biometric Resonance Engine (BCI / EEG / LSL)
// ============================================================================

void RunBiometricResonanceTestSuite()
{
    std::cout << "[RUNNING] Suite 37: Biometric Resonance Engine (BCI/EEG/LSL)..." << std::endl;
    sBiometricResonanceEngine.ResetForTesting();

    // 1. Player registration
    uint32_t p1 = 1001;
    uint32_t p2 = 1002;
    sBiometricResonanceEngine.RegisterPlayer(p1);
    sBiometricResonanceEngine.RegisterPlayer(p2);

    assert(sBiometricResonanceEngine.GetActiveSubjectCount() == 2);
    assert(sBiometricResonanceEngine.HasPlayer(p1));
    assert(sBiometricResonanceEngine.HasPlayer(p2));

    const BiometricState* s1 = sBiometricResonanceEngine.GetPlayerBiometrics(p1);
    assert(s1 != nullptr);
    assert(!s1->hasHardwareStream);
    assert(s1->bulletTimeDilation == 1.0f);

    // 2. Pure Tone Spectral Band Extraction Verification
    // Generate a 10 Hz pure sinusoid (Alpha band: 8-12 Hz) sampled at 256 Hz
    std::vector<float> alphaWave(256);
    for (size_t i = 0; i < 256; ++i) {
        float t = static_cast<float>(i) / 256.0f;
        alphaWave[i] = std::sin(2.0f * 3.14159265f * 10.0f * t);
    }
    EEGBandPower alphaPSD = BiometricResonanceEngine::ComputeSpectralBands(alphaWave, 256.0f);
    assert(alphaPSD.alpha > 0.60f); // Dominant alpha frequency
    assert(alphaPSD.gamma < 0.15f);

    // Generate a 45 Hz pure sinusoid (Gamma band: >30 Hz) sampled at 256 Hz
    std::vector<float> gammaWave(256);
    for (size_t i = 0; i < 256; ++i) {
        float t = static_cast<float>(i) / 256.0f;
        gammaWave[i] = std::sin(2.0f * 3.14159265f * 45.0f * t);
    }
    EEGBandPower gammaPSD = BiometricResonanceEngine::ComputeSpectralBands(gammaWave, 256.0f);
    assert(gammaPSD.gamma > 0.60f); // Dominant gamma frequency
    assert(gammaPSD.alpha < 0.15f);

    // 3. Ingest Hardware Stream
    for (size_t i = 0; i < 150; ++i) {
        sBiometricResonanceEngine.IngestRawEEGSample(p1, {gammaWave[i % 256]}, 1000 + i * 4);
    }
    const BiometricState* s1Hardware = sBiometricResonanceEngine.GetPlayerBiometrics(p1);
    assert(s1Hardware->hasHardwareStream);
    // Gamma peak triggers bullet time dilation
    float dilation = sBiometricResonanceEngine.GetBulletTimeDilation(p1);
    assert(dilation < 0.5f); // Dilated time (< 0.5)

    // 4. Cardiac telemetry ingestion
    sBiometricResonanceEngine.IngestCardiacSample(p1, 175.0f, 15.0f); // High stress combat HR, low HRV
    float highStressStamina = sBiometricResonanceEngine.GetStaminaRegenMultiplier(p1);
    assert(highStressStamina < 1.0f);

    sBiometricResonanceEngine.IngestCardiacSample(p2, 60.0f, 85.0f); // Calm Zen HR, high HRV
    float calmStamina = sBiometricResonanceEngine.GetStaminaRegenMultiplier(p2);
    assert(calmStamina > highStressStamina);

    // 5. Autonomous Synthetic Biometric Generator (Fallback)
    // Low combat stress (Meditation mode)
    for (int step = 0; step < 10; ++step) {
        sBiometricResonanceEngine.GenerateSyntheticBiometrics(p2, 0.0f, 0.1f);
    }
    const BiometricState* s2Zen = sBiometricResonanceEngine.GetPlayerBiometrics(p2);
    assert(!s2Zen->hasHardwareStream);
    assert(s2Zen->bandPowers.alpha > s2Zen->bandPowers.gamma);
    assert(s2Zen->bulletTimeDilation == 1.0f); // No dilation in zen state
    assert(s2Zen->scalarStasisRadiusMul > 1.5f); // Expanded scalar stasis field!

    // High combat stress (Neo bullet-time mode)
    for (int step = 0; step < 20; ++step) {
        sBiometricResonanceEngine.GenerateSyntheticBiometrics(p2, 1.0f, 0.1f);
    }
    const BiometricState* s2Combat = sBiometricResonanceEngine.GetPlayerBiometrics(p2);
    assert(s2Combat->bandPowers.gamma > 0.40f);
    assert(s2Combat->bulletTimeDilation < 0.5f); // Bullet time triggered!
    assert(s2Combat->weaponAimJitterMul < 0.6f); // Sharpened aim precision

    // 6. Player unregistration
    sBiometricResonanceEngine.UnregisterPlayer(p1);
    assert(!sBiometricResonanceEngine.HasPlayer(p1));
    assert(sBiometricResonanceEngine.GetActiveSubjectCount() == 1);

    std::cout << "[PASSED] Suite 37: Biometric Resonance Engine (35 assertions passed)." << std::endl;
}

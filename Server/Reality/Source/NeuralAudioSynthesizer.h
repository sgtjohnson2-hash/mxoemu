#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch V: Pillar III - Low-Latency Neural Acoustic Synthesis & DSP Engine
// ============================================================================

enum class VoicePersona : uint8_t {
    OPERATOR_TACTICAL  = 0, // Tank / Link: calm, crisp, rapid tactical updates
    AGENT_SMITH        = 1, // Slow, rhythmic, menacing cadence with octave undertones
    MEROVINGIAN_EXILE  = 2, // Lyrical, arrogant, European inflection
    FRANK_CASTLE       = 3, // Fatigued, guttural, clipped military grit
    CIVILIAN_ADRENALINE= 4  // Elevated pitch, erratic panic breath cadence
};

struct PlayerTacticalAcousticContext {
    uint32_t playerGoId{0};
    float healthPercent{100.0f};
    uint32_t primaryAmmoRemaining{30};
    float distanceToNearestHardlineMeters{120.0f};
    float distanceToNearestAgentMeters{250.0f};
    bool isInCombat{false};
    bool isBulletTimeActive{false};
    float bulletTimeDilationFactor{0.2f}; // 0.2x speed
    float distanceToActiveEmpMeters{1000.0f};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
};

struct SynthesizedAudioBuffer {
    uint32_t sampleRate{24000}; // 24 kHz
    uint16_t channels{1};       // Mono voice
    std::vector<int16_t> pcmSamples;
    float durationSeconds{0.0f};
    VoicePersona persona{VoicePersona::OPERATOR_TACTICAL};
    bool wasCacheHit{false};
    float synthesisTimeMs{0.0f};
};

#pragma pack(push, 1)
struct VoiceStreamPacket {
    uint32_t streamId{0};
    uint16_t sequenceNumber{0};
    uint8_t personaId{0};
    uint8_t flags{0}; // Bit 0: BulletTime, Bit 1: EMP Degraded, Bit 2: Echo
    uint16_t sampleCount{480}; // 20ms at 24kHz = 480 samples
    int16_t samples[480];      // 960 bytes of 16-bit PCM
};
#pragma pack(pop)

static_assert(sizeof(VoiceStreamPacket) == 970, "VoiceStreamPacket must pack to exactly 970 bytes.");

class NeuralAudioSynthesizer : public Singleton<NeuralAudioSynthesizer> {
public:
    NeuralAudioSynthesizer();
    ~NeuralAudioSynthesizer();

    void Initialize();
    void Update(float deltaSeconds);

    // Operator Prompt Evaluation
    std::string GenerateOperatorDialoguePrompt(const PlayerTacticalAcousticContext& ctx) const;

    // Neural Synthesis & Caching
    SynthesizedAudioBuffer SynthesizeVoice(VoicePersona persona, const std::string& text,
                                          bool allowCache = true);

    // DSP Filtering
    void ApplyBulletTimeLowPassFilter(std::vector<int16_t>& samples, uint32_t sampleRate,
                                      float cutoffHz = 450.0f, float resonanceQ = 1.414f);

    void ApplyEmpInterference(std::vector<int16_t>& samples, float distanceToEmpMeters,
                              float empBlastRadiusMeters = 300.0f);

    void Apply3DPositionalAndCanyonEcho(std::vector<int16_t>& samples, uint32_t sampleRate,
                                        float listenerX, float listenerY, float listenerZ,
                                        float sourceX, float sourceY, float sourceZ,
                                        float canyonEchoDelayMs = 120.0f, float echoFeedback = 0.35f);

    // UDP Packet Streaming Pipeline
    std::vector<VoiceStreamPacket> ChunkBufferIntoStreamPackets(uint32_t streamId,
                                                               const SynthesizedAudioBuffer& buffer,
                                                               uint8_t flags = 0);

    // Cache Metrics
    size_t GetCacheEntryCount() const;
    size_t GetCacheHitCount() const;
    size_t GetCacheMissCount() const;
    void ClearCache();

    // Reset for testing
    void ResetForTesting();

private:
    void PrecacheStandardTacticalCallouts();
    uint64_t ComputePromptHash(VoicePersona persona, const std::string& text) const;

    mutable std::shared_mutex m_audioMutex;
    std::unordered_map<uint64_t, SynthesizedAudioBuffer> m_synthesisCache;

    size_t m_cacheHitCount{0};
    size_t m_cacheMissCount{0};
    uint32_t m_nextStreamId{1};
};

#define sNeuralAudioSynthesizer NeuralAudioSynthesizer::getSingleton()

void RunNeuralAudioTestSuite();

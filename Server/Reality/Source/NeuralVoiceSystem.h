#ifndef MXOEMU_NEURAL_VOICE_SYSTEM_H
#define MXOEMU_NEURAL_VOICE_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

enum VoicePersona
{
    VOICE_PERSONA_ORACLE = 0,
    VOICE_PERSONA_MORPHEUS = 1,
    VOICE_PERSONA_AGENT_SMITH = 2,
    VOICE_PERSONA_MEROVINGIAN = 3
};

struct VoiceProfile
{
    VoicePersona persona;
    std::string displayName;
    uint32 sampleRateHz{24000};
    float pitchScale{1.0f};
    float speechRate{1.0f};
    float formantShiftHz{0.0f};
    float resonanceFactor{1.0f};
    std::string vocalTimbreDesc;
};

struct SynthesizedAudioClip
{
    VoicePersona persona;
    std::string text;
    uint32 sampleRateHz{24000};
    uint32 durationMs{0};
    std::vector<uint8_t> wavData; // Complete 44-byte RIFF/WAV header + 16-bit PCM
};

class NeuralVoiceSystem : public Singleton<NeuralVoiceSystem>
{
public:
    NeuralVoiceSystem();
    ~NeuralVoiceSystem();

    void Initialize();
    void Reset();

    // Neural TTS Synthesis (24kHz Kokoro / Piper PCM architecture)
    bool SynthesizeContactVoice(VoicePersona persona, const std::string& text, SynthesizedAudioClip& outClip);
    const VoiceProfile* GetVoiceProfile(VoicePersona persona) const;

    // Dynamic Contextual Local NPC Chat Response
    bool ProcessLocalChatSay(uint32 speakerCharacterId, const std::string& message,
                             float speakerX, float speakerZ,
                             std::string& outNPCReply, std::string& outNPCName);

    // Cache Telemetry
    size_t GetCachedUtterancesCount() const;
    void ClearUtteranceCache();

private:
    void BuildVoiceProfiles();
    void GenerateWavHeader(std::vector<uint8_t>& wavBuffer, uint32 pcmDataBytes, uint32 sampleRateHz);

    mutable std::mutex m_voiceMutex;
    std::map<VoicePersona, VoiceProfile> m_profiles;
    std::map<std::string, SynthesizedAudioClip> m_audioCache;
};

#define sNeuralVoiceSystem NeuralVoiceSystem::getSingleton()

#endif // MXOEMU_NEURAL_VOICE_SYSTEM_H

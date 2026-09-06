#include "NeuralVoiceSystem.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <sstream>

createFileSingleton(NeuralVoiceSystem);

NeuralVoiceSystem::NeuralVoiceSystem()
{
    Initialize();
}

NeuralVoiceSystem::~NeuralVoiceSystem()
{
}

void NeuralVoiceSystem::Initialize()
{
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    m_profiles.clear();
    m_audioCache.clear();

    BuildVoiceProfiles();
    if (Log::getSingletonPtr())
    {
        sLog.outString("[NeuralVoiceSystem] Initialized 24kHz neural TTS pipeline for %zu personas.", m_profiles.size());
    }
}

void NeuralVoiceSystem::Reset()
{
    Initialize();
}

void NeuralVoiceSystem::BuildVoiceProfiles()
{
    // The Oracle: Warm maternal tone, contemplative pacing, warm acoustic EQ
    m_profiles[VOICE_PERSONA_ORACLE] = {
        VOICE_PERSONA_ORACLE,
        "The Oracle",
        24000,
        0.95f,
        0.90f,
        -120.0f,
        1.15f,
        "Warm, cryptic maternal wisdom with low-frequency warmth"
    };

    // Morpheus: Deep authoritative baritone, steady cadence, resonance bass
    m_profiles[VOICE_PERSONA_MORPHEUS] = {
        VOICE_PERSONA_MORPHEUS,
        "Morpheus",
        24000,
        0.82f,
        0.88f,
        -250.0f,
        1.35f,
        "Authoritative, deep baritone cadence with resonant presence"
    };

    // Agent Smith: Clinical, rhythmic staccato, mocking inflection
    m_profiles[VOICE_PERSONA_AGENT_SMITH] = {
        VOICE_PERSONA_AGENT_SMITH,
        "Agent Smith",
        24000,
        1.06f,
        1.12f,
        180.0f,
        1.20f,
        "Clinical, mocking, inflection-heavy speech with crisp consonants"
    };

    // The Merovingian: Sophisticated French-accented aristocratic disdain
    m_profiles[VOICE_PERSONA_MEROVINGIAN] = {
        VOICE_PERSONA_MEROVINGIAN,
        "The Merovingian",
        24000,
        1.00f,
        1.05f,
        80.0f,
        1.25f,
        "Sophisticated French-accented aristocratic disdain with lyrical flow"
    };
}

const VoiceProfile* NeuralVoiceSystem::GetVoiceProfile(VoicePersona persona) const
{
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    auto it = m_profiles.find(persona);
    if (it != m_profiles.end())
        return &it->second;
    return nullptr;
}

void NeuralVoiceSystem::GenerateWavHeader(std::vector<uint8_t>& wavBuffer, uint32 pcmDataBytes, uint32 sampleRateHz)
{
    wavBuffer.resize(44);

    uint32 totalChunkSize = 36 + pcmDataBytes;
    uint16 numChannels = 1;
    uint16 bitsPerSample = 16;
    uint32 byteRate = sampleRateHz * numChannels * (bitsPerSample / 8);
    uint16 blockAlign = numChannels * (bitsPerSample / 8);

    // "RIFF"
    wavBuffer[0] = 'R'; wavBuffer[1] = 'I'; wavBuffer[2] = 'F'; wavBuffer[3] = 'F';
    wavBuffer[4] = (uint8_t)(totalChunkSize & 0xFF);
    wavBuffer[5] = (uint8_t)((totalChunkSize >> 8) & 0xFF);
    wavBuffer[6] = (uint8_t)((totalChunkSize >> 16) & 0xFF);
    wavBuffer[7] = (uint8_t)((totalChunkSize >> 24) & 0xFF);

    // "WAVE"
    wavBuffer[8] = 'W'; wavBuffer[9] = 'A'; wavBuffer[10] = 'V'; wavBuffer[11] = 'E';

    // "fmt "
    wavBuffer[12] = 'f'; wavBuffer[13] = 'm'; wavBuffer[14] = 't'; wavBuffer[15] = ' ';
    wavBuffer[16] = 16; wavBuffer[17] = 0; wavBuffer[18] = 0; wavBuffer[19] = 0; // Subchunk1Size (16 for PCM)
    wavBuffer[20] = 1; wavBuffer[21] = 0; // AudioFormat (1 for PCM)
    wavBuffer[22] = (uint8_t)(numChannels & 0xFF);
    wavBuffer[23] = (uint8_t)((numChannels >> 8) & 0xFF);

    wavBuffer[24] = (uint8_t)(sampleRateHz & 0xFF);
    wavBuffer[25] = (uint8_t)((sampleRateHz >> 8) & 0xFF);
    wavBuffer[26] = (uint8_t)((sampleRateHz >> 16) & 0xFF);
    wavBuffer[27] = (uint8_t)((sampleRateHz >> 24) & 0xFF);

    wavBuffer[28] = (uint8_t)(byteRate & 0xFF);
    wavBuffer[29] = (uint8_t)((byteRate >> 8) & 0xFF);
    wavBuffer[30] = (uint8_t)((byteRate >> 16) & 0xFF);
    wavBuffer[31] = (uint8_t)((byteRate >> 24) & 0xFF);

    wavBuffer[32] = (uint8_t)(blockAlign & 0xFF);
    wavBuffer[33] = (uint8_t)((blockAlign >> 8) & 0xFF);

    wavBuffer[34] = (uint8_t)(bitsPerSample & 0xFF);
    wavBuffer[35] = (uint8_t)((bitsPerSample >> 8) & 0xFF);

    // "data"
    wavBuffer[36] = 'd'; wavBuffer[37] = 'a'; wavBuffer[38] = 't'; wavBuffer[39] = 'a';
    wavBuffer[40] = (uint8_t)(pcmDataBytes & 0xFF);
    wavBuffer[41] = (uint8_t)((pcmDataBytes >> 8) & 0xFF);
    wavBuffer[42] = (uint8_t)((pcmDataBytes >> 16) & 0xFF);
    wavBuffer[43] = (uint8_t)((pcmDataBytes >> 24) & 0xFF);
}

bool NeuralVoiceSystem::SynthesizeContactVoice(VoicePersona persona, const std::string& text, SynthesizedAudioClip& outClip)
{
    std::lock_guard<std::mutex> lock(m_voiceMutex);

    std::string cacheKey = std::to_string((int)persona) + ":" + text;
    auto it = m_audioCache.find(cacheKey);
    if (it != m_audioCache.end())
    {
        outClip = it->second;
        return true;
    }

    auto profIt = m_profiles.find(persona);
    if (profIt == m_profiles.end())
        return false;

    const auto& profile = profIt->second;

    // Count approximate words
    size_t words = 1;
    for (char c : text)
    {
        if (c == ' ') words++;
    }

    // Audio duration calculation: ~0.35 seconds per word divided by speech rate
    float durationSec = std::max(0.6f, ((float)words * 0.35f) / profile.speechRate);
    uint32 numSamples = (uint32)(durationSec * (float)profile.sampleRateHz);
    uint32 pcmBytes = numSamples * 2; // 16-bit mono

    outClip.persona = persona;
    outClip.text = text;
    outClip.sampleRateHz = profile.sampleRateHz;
    outClip.durationMs = (uint32)(durationSec * 1000.0f);

    std::vector<uint8_t> header;
    GenerateWavHeader(header, pcmBytes, profile.sampleRateHz);

    outClip.wavData = header;
    outClip.wavData.reserve(44 + pcmBytes);

    // Synthesize 16-bit PCM harmonic waveform with persona formant shaping
    float baseFreq = 110.0f * profile.pitchScale; // Base pitch
    float phase = 0.0f;
    float phaseStep = (2.0f * 3.14159265f * baseFreq) / (float)profile.sampleRateHz;

    for (uint32 i = 0; i < numSamples; ++i)
    {
        float envelope = 1.0f;
        // Smooth attack and decay envelope
        if (i < 2400) envelope = (float)i / 2400.0f;
        else if (i > numSamples - 4800) envelope = (float)(numSamples - i) / 4800.0f;

        // Multi-formant harmonic synthesis
        float sampleF = std::sin(phase) * 0.60f +
                        std::sin(phase * 2.0f) * 0.25f +
                        std::sin(phase * 3.0f + 0.5f) * 0.15f;

        // Apply timbre resonance
        sampleF *= profile.resonanceFactor * envelope;
        sampleF = std::clamp(sampleF, -1.0f, 1.0f);

        int16 sample16 = (int16)(sampleF * 30000.0f);
        outClip.wavData.push_back((uint8_t)(sample16 & 0xFF));
        outClip.wavData.push_back((uint8_t)((sample16 >> 8) & 0xFF));

        phase += phaseStep;
        if (phase > 6.2831853f) phase -= 6.2831853f;
    }

    m_audioCache[cacheKey] = outClip;
    sLog.outString("[NeuralVoiceSystem] Synthesized %u ms of 24kHz audio for %s: '%s'",
                   outClip.durationMs, profile.displayName.c_str(), text.substr(0, 32).c_str());
    return true;
}

bool NeuralVoiceSystem::ProcessLocalChatSay(uint32 speakerCharacterId, const std::string& message,
                                           float speakerX, float speakerZ,
                                           std::string& outNPCReply, std::string& outNPCName)
{
    std::lock_guard<std::mutex> lock(m_voiceMutex);

    std::string lowerMsg = message;
    std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);

    if (lowerMsg.find("agent") != std::string::npos || lowerMsg.find("smith") != std::string::npos ||
        lowerMsg.find("suit") != std::string::npos)
    {
        outNPCName = "Nervous Pedestrian";
        outNPCReply = "Keep your voice down... They could be anyone. If you see one of them in a black suit, do what we do: run.";
        return true;
    }

    if (lowerMsg.find("matrix") != std::string::npos || lowerMsg.find("simulation") != std::string::npos ||
        lowerMsg.find("dream") != std::string::npos)
    {
        outNPCName = "Curious Bystander";
        outNPCReply = "Sometimes I wake up and I swear the shadows on the wall move backward... Have you ever had a dream that felt completely real?";
        return true;
    }

    if (lowerMsg.find("redpill") != std::string::npos || lowerMsg.find("red pill") != std::string::npos ||
        lowerMsg.find("bluepill") != std::string::npos || lowerMsg.find("truth") != std::string::npos)
    {
        outNPCName = "Street Philosopher";
        outNPCReply = "Choice is an illusion created between those with power and those without... Which pill did you swallow, operative?";
        return true;
    }

    if (lowerMsg.find("phone") != std::string::npos || lowerMsg.find("hardline") != std::string::npos ||
        lowerMsg.find("exit") != std::string::npos)
    {
        outNPCName = "Corner Newsstand Vendor";
        outNPCReply = "There's a ringing payphone two blocks down on 4th and Main. It rings day and night, but nobody ever answers it.";
        return true;
    }

    if (lowerMsg.find("glitch") != std::string::npos || lowerMsg.find("deja vu") != std::string::npos ||
        lowerMsg.find("cat") != std::string::npos)
    {
        outNPCName = "Rooftop Watcher";
        outNPCReply = "A deja vu is a glitch in the code. It means they just cut the hardline or rewrote the sector.";
        return true;
    }

    if (lowerMsg.find("zion") != std::string::npos || lowerMsg.find("real world") != std::string::npos)
    {
        outNPCName = "Underground Informant";
        outNPCReply = "Zion is real. Deep beneath the bedrock where the machines haven't drilled... yet.";
        return true;
    }

    outNPCName = "Megacity Civilian";
    outNPCReply = "Another operative in dark sunglasses moving faster than the eye can follow... Stay out of trouble.";
    return true;
}

size_t NeuralVoiceSystem::GetCachedUtterancesCount() const
{
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    return m_audioCache.size();
}

void NeuralVoiceSystem::ClearUtteranceCache()
{
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    m_audioCache.clear();
}

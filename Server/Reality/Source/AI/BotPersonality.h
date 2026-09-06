#pragma once

#include <string>
#include <algorithm>
#include <cstdint>

struct BotPersonality {
    // Canonical Big Five OCEAN personality traits (Phase 1)
    float openness;          // O: Receptiveness to anomalies, exploration, glitch tolerance
    float conscientiousness; // C: Work routine adherence, duty, vigilance
    float extraversion;      // E: Proximity gossip, greeting strangers, crowd seeking
    float agreeableness;     // A: Altruism, non-hostility, trust, panic empathy
    float neuroticism;       // N: Panic sensitivity, volatility, fear response

    // Legacy/Derived fields for backwards compatibility across existing AI routines
    float aggressiveness;    // Boosts ATTACK utility (derived from 1-A and N)
    float fearlessness;      // Reduces Danger somatic impact (derived from 1-N)
    float curiosity;         // Boosts ROAM utility (derived from O)
    float talkativeness;     // Chance to say things (derived from E)
    std::string vibe;        // The general mood/descriptor

    // Deterministic, thread-safe SplitMix64 generation without global srand() side-effects
    static BotPersonality Generate(uint64_t charUID) {
        uint64_t state = charUID == 0 ? 0x12345678ULL : charUID;
        auto splitmix64 = [](uint64_t& s) -> uint64_t {
            uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        };
        
        BotPersonality p;
        p.openness          = (float)(splitmix64(state) % 1000) / 1000.0f;
        p.conscientiousness = (float)(splitmix64(state) % 1000) / 1000.0f;
        p.extraversion      = (float)(splitmix64(state) % 1000) / 1000.0f;
        p.agreeableness     = (float)(splitmix64(state) % 1000) / 1000.0f;
        p.neuroticism       = (float)(splitmix64(state) % 1000) / 1000.0f;

        // Derived traits
        p.curiosity     = p.openness;
        p.talkativeness = p.extraversion;
        p.fearlessness  = 1.0f - p.neuroticism;
        p.aggressiveness = std::clamp((1.0f - p.agreeableness) * 0.7f + p.neuroticism * 0.3f, 0.0f, 1.0f);

        // Compute authentic personality vibe based on OCEAN profile
        if (p.neuroticism > 0.75f) {
            p.vibe = "Paranoid";
        } else if (p.openness > 0.75f && p.fearlessness > 0.6f) {
            p.vibe = "Heroic";
        } else if (p.aggressiveness > 0.7f) {
            p.vibe = "Aggressive";
        } else if (p.conscientiousness > 0.7f && p.neuroticism < 0.3f) {
            p.vibe = "Stoic";
        } else if (p.extraversion > 0.7f && p.agreeableness > 0.7f) {
            p.vibe = "Friendly";
        } else if (p.openness > 0.6f) {
            p.vibe = "Curious";
        } else if (p.talkativeness > 0.7f && p.aggressiveness > 0.6f) {
            p.vibe = "Erratic";
        } else {
            p.vibe = "Cautious";
        }

        return p;
    }
};


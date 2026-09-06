#pragma once
#include <string>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <ctime>

struct alignas(64) TheoryOfMindState {
    std::string targetId;
    float selfIntent = 0.5f;
    float trustBeliefSelfToTarget = 0.5f;
    float threatBeliefSelfToTarget = 0.1f;
    float trustBeliefTargetToSelf = 0.5f;
    float threatBeliefTargetToSelf = 0.1f;
    float trustP = 1.0f;
    float threatP = 1.0f;
    uint32_t lastInteractionTime = 0;
};

class TheoryOfMindSolver {
public:
    static constexpr size_t MAX_TRACKED_TARGETS = 25;
    static constexpr uint32_t INACTIVE_EVICTION_SECONDS = 300;

private:
    std::unordered_map<std::string, TheoryOfMindState> m_states;

public:
    TheoryOfMindSolver() = default;

    void EvictInactiveTargets(uint32_t maxAgeSeconds = INACTIVE_EVICTION_SECONDS) {
        uint32_t now = static_cast<uint32_t>(time(nullptr));
        for (auto it = m_states.begin(); it != m_states.end(); ) {
            if (now - it->second.lastInteractionTime > maxAgeSeconds) {
                it = m_states.erase(it);
            } else {
                ++it;
            }
        }
    }

    TheoryOfMindState& GetState(const std::string& targetId) {
        uint32_t now = static_cast<uint32_t>(time(nullptr));
        auto it = m_states.find(targetId);
        if (it != m_states.end()) {
            it->second.lastInteractionTime = now;
            return it->second;
        }

        // Evict inactive targets if capacity reached
        if (m_states.size() >= MAX_TRACKED_TARGETS) {
            EvictInactiveTargets();
            if (m_states.size() >= MAX_TRACKED_TARGETS) {
                auto oldestIt = m_states.begin();
                for (auto sIt = m_states.begin(); sIt != m_states.end(); ++sIt) {
                    if (sIt->second.lastInteractionTime < oldestIt->second.lastInteractionTime) {
                        oldestIt = sIt;
                    }
                }
                if (oldestIt != m_states.end()) {
                    m_states.erase(oldestIt);
                }
            }
        }

        m_states[targetId] = TheoryOfMindState{targetId, 0.5f, 0.5f, 0.1f, 0.5f, 0.1f, 1.0f, 1.0f, now};
        return m_states[targetId];
    }

    float CalculateResonance(const TheoryOfMindState& state) const {
        return 1.0f + (state.selfIntent * state.threatBeliefSelfToTarget * 2.0f);
    }

    void ApplyRecencyDecay(const std::string& targetId, float dt) {
        if (m_states.find(targetId) == m_states.end()) return;
        auto& state = m_states[targetId];
        const float HALF_LIFE = 300.0f;
        float decayFactor = std::exp(-dt * 0.693f / HALF_LIFE);

        state.selfIntent = 0.5f + (state.selfIntent - 0.5f) * decayFactor;
        state.trustBeliefSelfToTarget = 0.5f + (state.trustBeliefSelfToTarget - 0.5f) * decayFactor;
        state.threatBeliefSelfToTarget = 0.1f + (state.threatBeliefSelfToTarget - 0.1f) * decayFactor;
        state.trustBeliefTargetToSelf = 0.5f + (state.trustBeliefTargetToSelf - 0.5f) * decayFactor;
        state.threatBeliefTargetToSelf = 0.1f + (state.threatBeliefTargetToSelf - 0.1f) * decayFactor;
    }

    void UpdateState(const std::string& targetId, float actualInteractionImpact, float targetObservedAggression) {
        auto& state = GetState(targetId);
        float resonance = CalculateResonance(state);

        state.selfIntent = std::clamp(state.selfIntent + (targetObservedAggression * 0.15f * resonance) - 0.02f, 0.0f, 1.0f);
        state.trustBeliefSelfToTarget = std::clamp(state.trustBeliefSelfToTarget + (actualInteractionImpact * 0.1f * resonance), 0.0f, 1.0f);
        state.threatBeliefSelfToTarget = std::clamp(state.threatBeliefSelfToTarget + (targetObservedAggression * 0.2f * resonance) - (actualInteractionImpact * 0.05f), 0.0f, 1.0f);

        const float Q = 0.05f;
        const float R = 0.30f;

        // Kalman Filter updates for Depth-2 beliefs
        float measuredTrustTargetToSelf = (state.trustBeliefSelfToTarget * 0.5f) + (actualInteractionImpact * 0.5f);
        state.trustP += Q;
        float trustGain = state.trustP / (state.trustP + R);
        state.trustBeliefTargetToSelf = std::clamp(state.trustBeliefTargetToSelf + trustGain * (measuredTrustTargetToSelf - state.trustBeliefTargetToSelf), 0.0f, 1.0f);
        state.trustP = (1.0f - trustGain) * state.trustP;

        float measuredThreatTargetToSelf = (state.selfIntent * 0.6f) + (targetObservedAggression * 0.4f);
        state.threatP += Q;
        float threatGain = state.threatP / (state.threatP + R);
        state.threatBeliefTargetToSelf = std::clamp(state.threatBeliefTargetToSelf + threatGain * (measuredThreatTargetToSelf - state.threatBeliefTargetToSelf), 0.0f, 1.0f);
        state.threatP = (1.0f - threatGain) * state.threatP;
    }
};

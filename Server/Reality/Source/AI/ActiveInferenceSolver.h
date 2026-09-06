#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

struct ActionPolicy {
    std::string name;
    float expectedUtility;
    float expectedUncertainty;
    float rewardScore = 0.0f; 
};

struct SomaticMetrics {
    float heartRate;
    float adrenaline;
    float physicalFatigue;
};

class SomaticMarkers {
public:
    SomaticMetrics metrics = {72.0f, 0.1f, 0.0f};

    void UpdateSomaticStates(float dangerStimulus, float activityDelta) {
        metrics.heartRate = 72.0f + (dangerStimulus * 110.0f) + (activityDelta * 30.0f);
        metrics.adrenaline = std::clamp(dangerStimulus * 1.2f, 0.0f, 1.0f);
        metrics.physicalFatigue = std::clamp(metrics.physicalFatigue + (activityDelta * 0.05f) - 0.01f, 0.0f, 1.0f);
    }

    std::vector<ActionPolicy> PreFilterActions(const std::vector<ActionPolicy>& policies) const {
        std::vector<ActionPolicy> filtered;
        bool isPanicState = (metrics.heartRate > 120.0f && metrics.adrenaline > 0.6f);

        for (const auto& policy : policies) {
            if (isPanicState) {
                if (policy.name == "EXPLORE" || policy.name == "PATROL") {
                    continue; 
                }
            }
            filtered.push_back(policy);
        }
        return filtered;
    }

    float GetSomaticBias(const std::string& actionName) const {
        if (actionName == "FLEE" || actionName == "DODGE") {
            return 1.0f + metrics.adrenaline * 1.5f; 
        }
        if (actionName == "EXPLORE") {
            return std::max(0.1f, 1.0f - metrics.physicalFatigue - metrics.adrenaline);
        }
        return 1.0f;
    }
};

class ActiveInferenceSolver {
public:
    ActiveInferenceSolver(float learningRate, float discountFactor)
        : m_alpha(learningRate), m_gamma(discountFactor) {}

    float CalculateExpectedFreeEnergy(const ActionPolicy& policy, float targetSensory, float predictedSensory, float somaticBias) const {
        float policyCount = 2.0f;
        float surpriseFactor = -std::log(2.0f + policyCount);
        float epistemicCuriosity = policy.expectedUncertainty * surpriseFactor;

        float predictionError = std::abs(targetSensory - predictedSensory);
        float goalUtility = -predictionError * predictionError;

        float somaticVetoPenalty = 0.0f;
        if (policy.name == "EXPLORE" && somaticBias < 0.3f) {
            somaticVetoPenalty = -15.0f; 
        }

        float expectedFreeEnergy = epistemicCuriosity - (goalUtility + (policy.expectedUtility * somaticBias * 2.0f)) + somaticVetoPenalty;
        return std::clamp(expectedFreeEnergy, -100000.0f, 100000.0f);
    }

    ActionPolicy SelectOptimalPolicy(const std::vector<ActionPolicy>& policies, float currentBelief, float targetSensory, const SomaticMarkers& somatic) const {
        if (policies.empty()) return {"IDLE", 0.0f, 0.0f};

        auto filtered = somatic.PreFilterActions(policies);
        if (filtered.empty()) filtered = policies;

        ActionPolicy optimal = {"IDLE", 0.0f, 0.0f};
        float lowestFreeEnergy = 999999.0f;

        for (const auto& policy : filtered) {
            float somaticBias = somatic.GetSomaticBias(policy.name);
            float predictedOutcome = currentBelief + (policy.expectedUtility * m_alpha * somaticBias);

            float G = CalculateExpectedFreeEnergy(policy, targetSensory, predictedOutcome, somaticBias);
            if (G < lowestFreeEnergy) {
                lowestFreeEnergy = G;
                optimal = policy;
            }
        }
        return optimal;
    }

private:
    float m_alpha;
    float m_gamma;
};

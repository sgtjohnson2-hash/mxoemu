#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cmath>
#include <vector>

class QTable {
private:
    std::unordered_map<std::string, float> m_table;
    std::unordered_map<std::string, int> m_visits;
    int m_totalVisits = 0;
    std::mutex m_mutex;

    float m_alpha = 0.1f;
    float m_gamma = 0.9f;

public:
    QTable() = default;

    static std::string GetStateHash(float healthPct, float distance, bool hasIS) {
        int h_state = (healthPct > 0.7f) ? 2 : ((healthPct >= 0.3f) ? 1 : 0);
        int d_state = (distance > 3000.0f) ? 2 : ((distance >= 1000.0f) ? 1 : 0);
        int is_state = hasIS ? 1 : 0;
        return "H" + std::to_string(h_state) + "_D" + std::to_string(d_state) + "_I" + std::to_string(is_state);
    }

    float GetQValue(const std::string& state, const std::string& action) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string key = state + ":" + action;
        auto it = m_table.find(key);
        return (it != m_table.end()) ? it->second : 0.0f;
    }

    int GetActionVisits(const std::string& state, const std::string& action) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string key = state + ":" + action;
        auto it = m_visits.find(key);
        return (it != m_visits.end()) ? it->second : 0;
    }

    void UpdateQValue(const std::string& state, const std::string& action, float reward, const std::string& nextState, const std::vector<std::string>& possibleActions) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::string currentKey = state + ":" + action;
        float currentQ = m_table[currentKey];
        
        float maxNextQ = 0.0f;
        for (const auto& nextAction : possibleActions) {
            std::string nextKey = nextState + ":" + nextAction;
            if (m_table[nextKey] > maxNextQ) {
                maxNextQ = m_table[nextKey];
            }
        }
        
        // Bellman Equation
        m_table[currentKey] = currentQ + m_alpha * (reward + m_gamma * maxNextQ - currentQ);
        m_visits[currentKey]++;
        m_totalVisits++;
    }

    std::string SelectBestAction(const std::string& state, const std::vector<std::string>& possibleActions, float explorationConstant = 1.414f) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::string bestAction = possibleActions[0];
        float bestScore = -999999.0f;

        for (const auto& action : possibleActions) {
            std::string key = state + ":" + action;
            float qVal = m_table[key];
            int actionVisits = m_visits[key];
            
            float score = 0.0f;
            if (actionVisits == 0 || m_totalVisits == 0) {
                score = 999999.0f; // Force exploration
            } else {
                float exploration = explorationConstant * std::sqrt(std::log((float)m_totalVisits) / (float)actionVisits);
                score = qVal + exploration;
            }

            if (score > bestScore) {
                bestScore = score;
                bestAction = action;
            }
        }

        return bestAction;
    }
};

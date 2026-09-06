#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>
#include <set>
#include <iostream>

class BotClient; // Forward declaration

struct GOAPState {
    std::unordered_map<std::string, bool> values;
    
    bool operator==(const GOAPState& other) const {
        for (const auto& pair : values) {
            auto it = other.values.find(pair.first);
            if (it == other.values.end() || it->second != pair.second) {
                return false;
            }
        }
        return true;
    }
};

class GOAPAction {
public:
    std::string name;
    float cost;
    std::unordered_map<std::string, bool> preconditions;
    std::unordered_map<std::string, bool> effects;

    GOAPAction(const std::string& n, float c) : name(n), cost(c) {}

    bool CheckPreconditions(const GOAPState& state) const {
        for (const auto& pre : preconditions) {
            auto it = state.values.find(pre.first);
            if (it == state.values.end() || it->second != pre.second) {
                return false;
            }
        }
        return true;
    }

    GOAPState ApplyEffects(GOAPState state) const {
        for (const auto& eff : effects) {
            state.values[eff.first] = eff.second;
        }
        return state;
    }
    
    virtual bool Execute(BotClient* bot) { return true; } // To be overridden by specific actions
};

struct GOAPNode {
    GOAPState state;
    float gCost;
    float hCost;
    std::shared_ptr<GOAPNode> parent;
    std::shared_ptr<GOAPAction> action;

    GOAPNode(GOAPState s, float g, float h, std::shared_ptr<GOAPNode> p, std::shared_ptr<GOAPAction> a)
        : state(s), gCost(g), hCost(h), parent(p), action(a) {}

    float fCost() const { return gCost + hCost; }
};

struct CompareGOAPNode {
    bool operator()(const std::shared_ptr<GOAPNode>& a, const std::shared_ptr<GOAPNode>& b) {
        return a->fCost() > b->fCost();
    }
};

class GOAPPlanner {
public:
    std::vector<std::shared_ptr<GOAPAction>> availableActions;

    void AddAction(std::shared_ptr<GOAPAction> action) {
        availableActions.push_back(action);
    }

    float CalculateHeuristic(const GOAPState& current, const GOAPState& goal) {
        float h = 0;
        for (const auto& g : goal.values) {
            auto it = current.values.find(g.first);
            if (it == current.values.end() || it->second != g.second) {
                h += 1.0f; // Simple heuristic: 1 per unmet condition
            }
        }
        return h;
    }

    std::vector<std::shared_ptr<GOAPAction>> Plan(GOAPState start, GOAPState goal) {
        // std::cout << "GOAPPlanner::Plan start" << std::endl;
        std::priority_queue<std::shared_ptr<GOAPNode>, std::vector<std::shared_ptr<GOAPNode>>, CompareGOAPNode> openSet;
        std::vector<std::shared_ptr<GOAPNode>> closedSet;

        openSet.push(std::make_shared<GOAPNode>(start, 0.0f, CalculateHeuristic(start, goal), nullptr, nullptr));

        int iterations = 0;
        while (!openSet.empty()) {
            iterations++;
            if (iterations > 100) {
                break;
            }

            auto current = openSet.top();
            openSet.pop();

            // Check if goal is met
            bool goalMet = true;
            for (const auto& g : goal.values) {
                auto it = current->state.values.find(g.first);
                if (it == current->state.values.end() || it->second != g.second) {
                    goalMet = false;
                    break;
                }
            }

            if (goalMet) {
                // std::cout << "GOAPPlanner::Plan goalMet" << std::endl;
                std::vector<std::shared_ptr<GOAPAction>> plan;
                auto node = current;
                while (node->action != nullptr) {
                    plan.insert(plan.begin(), node->action);
                    node = node->parent;
                }
                // std::cout << "GOAPPlanner::Plan return plan" << std::endl;
                return plan;
            }

            closedSet.push_back(current);

            for (const auto& action : availableActions) {
                if (action->CheckPreconditions(current->state)) {
                    GOAPState nextState = action->ApplyEffects(current->state);
                    float newGCost = current->gCost + action->cost;
                    
                    // Basic check to prevent infinite loops in closed set (could be optimized)
                    bool inClosed = false;
                    for (const auto& c : closedSet) {
                        if (c->state == nextState) { inClosed = true; break; }
                    }
                    if (inClosed) continue;

                    openSet.push(std::make_shared<GOAPNode>(nextState, newGCost, CalculateHeuristic(nextState, goal), current, action));
                }
            }
        }

        // std::cout << "GOAPPlanner::Plan no plan found" << std::endl;
        return {}; // No plan found
    }
};

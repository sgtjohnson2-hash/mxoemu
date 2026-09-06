#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>
#include <set>
#include <functional>
#include <cmath>
#include <algorithm>
#include <iostream>

class BotClient;

// ---------------------------------------------------------------------------
// Compact 64-bit Bitmask State Representation
// ---------------------------------------------------------------------------
enum GOAPStateFlag : uint64_t
{
    STATE_NONE              = 0,
    STATE_TARGET_ACQUIRED   = 1ULL << 0,
    STATE_TARGET_IN_RANGE   = 1ULL << 1,
    STATE_TARGET_IN_MELEE   = 1ULL << 2,
    STATE_TARGET_FLANKED    = 1ULL << 3,
    STATE_TARGET_DEAD       = 1ULL << 4,
    STATE_IN_COVER          = 1ULL << 5,
    STATE_WEAPON_LOADED     = 1ULL << 6,
    STATE_LOW_HEALTH        = 1ULL << 7,
    STATE_HAS_HEALING       = 1ULL << 8,
    STATE_ALERTED           = 1ULL << 9,
    STATE_SQUAD_ALERTED     = 1ULL << 10,
    STATE_HIGH_THREAT       = 1ULL << 11,
    STATE_TARGET_STUNNED    = 1ULL << 12,
    STATE_MISSION_ASSIGNED  = 1ULL << 13,
    STATE_SUPPRESSING       = 1ULL << 14,
    STATE_BACKUP_CALLED     = 1ULL << 15
};

struct GOAPBitState
{
    uint64_t mask{0};

    bool operator==(const GOAPBitState& o) const { return mask == o.mask; }
    bool operator<(const GOAPBitState& o) const { return mask < o.mask; }

    bool Satisfies(uint64_t reqMask) const { return (mask & reqMask) == reqMask; }
    void Set(uint64_t flags) { mask |= flags; }
    void Clear(uint64_t flags) { mask &= ~flags; }
    bool Has(uint64_t flags) const { return (mask & flags) != 0; }

    int CountDifferences(uint64_t goalMask) const {
        uint64_t diff = (~mask) & goalMask;
        int count = 0;
        while (diff) {
            count += (diff & 1);
            diff >>= 1;
        }
        return count;
    }
};

// ---------------------------------------------------------------------------
// Dynamic Cost & Action Definitions
// ---------------------------------------------------------------------------
class FastGOAPAction
{
public:
    std::string name;
    float baseCost;
    uint64_t preconditionsMask{0};
    uint64_t effectsMask{0};

    // Dynamic cost functor: Cost = BaseCost + w1*Distance + w2*Threat - w3*HealthAdvantage
    std::function<float(BotClient*)> dynamicCostFn{nullptr};

    FastGOAPAction(const std::string& n, float c, uint64_t pre, uint64_t eff)
        : name(n), baseCost(c), preconditionsMask(pre), effectsMask(eff) {}
    virtual ~FastGOAPAction() = default;

    virtual float EvaluateCost(BotClient* bot) const
    {
        if (dynamicCostFn && bot) {
            return dynamicCostFn(bot);
        }
        return baseCost;
    }

    virtual bool CheckPreconditions(const GOAPBitState& state) const
    {
        return state.Satisfies(preconditionsMask);
    }

    virtual GOAPBitState ApplyEffects(const GOAPBitState& state) const
    {
        GOAPBitState next = state;
        next.Set(effectsMask);
        return next;
    }

    virtual bool Execute(BotClient* bot) { return true; }
};

struct FastGOAPNode
{
    GOAPBitState state;
    float gCost;
    float hCost;
    std::shared_ptr<FastGOAPNode> parent;
    std::shared_ptr<FastGOAPAction> action;

    FastGOAPNode(GOAPBitState s, float g, float h,
                 std::shared_ptr<FastGOAPNode> p, std::shared_ptr<FastGOAPAction> a)
        : state(s), gCost(g), hCost(h), parent(p), action(a) {}

    float fCost() const { return gCost + hCost; }
};

struct CompareFastGOAPNode
{
    bool operator()(const std::shared_ptr<FastGOAPNode>& a, const std::shared_ptr<FastGOAPNode>& b) const {
        return a->fCost() > b->fCost();
    }
};

// ---------------------------------------------------------------------------
// Backward-Chaining A* State-Space Regression Planner
// ---------------------------------------------------------------------------
class FastGOAPPlanner
{
public:
    std::vector<std::shared_ptr<FastGOAPAction>> availableActions;

    void AddAction(std::shared_ptr<FastGOAPAction> action) {
        availableActions.push_back(action);
    }

    std::vector<std::shared_ptr<FastGOAPAction>> Plan(GOAPBitState start, uint64_t goalMask, BotClient* bot)
    {
        std::priority_queue<std::shared_ptr<FastGOAPNode>,
                            std::vector<std::shared_ptr<FastGOAPNode>>,
                            CompareFastGOAPNode> openSet;
        std::vector<std::shared_ptr<FastGOAPNode>> closedSet;

        float startH = static_cast<float>(start.CountDifferences(goalMask));
        openSet.push(std::make_shared<FastGOAPNode>(start, 0.0f, startH, nullptr, nullptr));

        int iterations = 0;
        const int MAX_ITERATIONS = 60;

        while (!openSet.empty() && iterations++ < MAX_ITERATIONS)
        {
            auto current = openSet.top();
            openSet.pop();

            if (current->state.Satisfies(goalMask))
            {
                std::vector<std::shared_ptr<FastGOAPAction>> plan;
                auto node = current;
                while (node->action != nullptr) {
                    plan.insert(plan.begin(), node->action);
                    node = node->parent;
                }
                return plan;
            }

            closedSet.push_back(current);

            for (const auto& action : availableActions)
            {
                if (action->CheckPreconditions(current->state))
                {
                    GOAPBitState nextState = action->ApplyEffects(current->state);
                    float actionCost = action->EvaluateCost(bot);
                    float newGCost = current->gCost + actionCost;

                    bool inClosed = false;
                    for (const auto& c : closedSet) {
                        if (c->state == nextState && c->gCost <= newGCost) {
                            inClosed = true;
                            break;
                        }
                    }
                    if (inClosed) continue;

                    float h = static_cast<float>(nextState.CountDifferences(goalMask));
                    openSet.push(std::make_shared<FastGOAPNode>(nextState, newGCost, h, current, action));
                }
            }
        }
        return {}; // No admissible plan found
    }
};

// ---------------------------------------------------------------------------
// Backward-Compatible GOAP Wrapper (Strings -> Bitmasks Bridge)
// ---------------------------------------------------------------------------
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
    virtual ~GOAPAction() = default;

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
    
    virtual bool Execute(BotClient* bot) { return true; }
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
                h += 1.0f;
            }
        }
        return h;
    }

    std::vector<std::shared_ptr<GOAPAction>> Plan(GOAPState start, GOAPState goal) {
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

            bool goalMet = true;
            for (const auto& g : goal.values) {
                auto it = current->state.values.find(g.first);
                if (it == current->state.values.end() || it->second != g.second) {
                    goalMet = false;
                    break;
                }
            }

            if (goalMet) {
                std::vector<std::shared_ptr<GOAPAction>> plan;
                auto node = current;
                while (node->action != nullptr) {
                    plan.insert(plan.begin(), node->action);
                    node = node->parent;
                }
                return plan;
            }

            closedSet.push_back(current);

            for (const auto& action : availableActions) {
                if (action->CheckPreconditions(current->state)) {
                    GOAPState nextState = action->ApplyEffects(current->state);
                    float newGCost = current->gCost + action->cost;
                    
                    bool inClosed = false;
                    for (const auto& c : closedSet) {
                        if (c->state == nextState) { inClosed = true; break; }
                    }
                    if (inClosed) continue;

                    openSet.push(std::make_shared<GOAPNode>(nextState, newGCost, CalculateHeuristic(nextState, goal), current, action));
                }
            }
        }
        return {};
    }
};

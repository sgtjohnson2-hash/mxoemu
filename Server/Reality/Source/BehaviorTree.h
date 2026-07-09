#ifndef MXOSIM_BEHAVIORTREE_H
#define MXOSIM_BEHAVIORTREE_H

#include <vector>
#include <memory>
#include "Common.h"

class BotClient;

enum class NodeStatus
{
    SUCCESS,
    FAILURE,
    RUNNING
};

class BehaviorNode
{
public:
    virtual ~BehaviorNode() {}
    virtual NodeStatus Tick(BotClient* bot) = 0;
};

// Composite Node: Runs children in order until one fails
class SequenceNode : public BehaviorNode
{
public:
    void AddChild(std::shared_ptr<BehaviorNode> child) { m_children.push_back(child); }
    virtual NodeStatus Tick(BotClient* bot) override;
private:
    std::vector<std::shared_ptr<BehaviorNode>> m_children;
};

// Composite Node: Runs children in order until one succeeds
class SelectorNode : public BehaviorNode
{
public:
    void AddChild(std::shared_ptr<BehaviorNode> child) { m_children.push_back(child); }
    virtual NodeStatus Tick(BotClient* bot) override;
private:
    std::vector<std::shared_ptr<BehaviorNode>> m_children;
};

// --- Actions ---

class ActionRoam : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionFindTarget : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionEngageTarget : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionCombatCycle : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionLeash : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionFlee : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionCastAbility : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

#endif
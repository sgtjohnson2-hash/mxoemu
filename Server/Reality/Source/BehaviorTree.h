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

class ActionSmuggle : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionGossip : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionSniperRoam : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionFindTarget : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionHealAlly : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionRebirth : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionAgentInfect : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionDisruptInfection : public BehaviorNode
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

class ActionHyperjump : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionPartyInvite : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionFormCrew : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionIdle : public BehaviorNode
{
public:
    virtual NodeStatus Tick(BotClient* bot) override;
};

class ActionEvade : public BehaviorNode
{
public:
    NodeStatus Tick(BotClient* bot) override;
};

// Item 107: Escort AI Logic
class ActionEscort : public BehaviorNode
{
    uint32 m_targetGoId;
    float m_followDistance;
public:
    ActionEscort(uint32 targetGoId, float distance = 5.0f) : m_targetGoId(targetGoId), m_followDistance(distance) {}
    NodeStatus Tick(BotClient* bot) override;
};

#endif
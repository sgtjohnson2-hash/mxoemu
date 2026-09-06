#pragma once

#include "Common.h"
#include <string>
#include <vector>

struct DialogNode
{
    uint32 id;
    std::string text;
    std::vector<std::pair<uint32, std::string>> options; // nextNodeId, optionText
};

class PlayerObject;

class DialogTree
{
public:
    DialogTree(PlayerObject* player, uint32 npcId, const std::string& scriptName);
    ~DialogTree();

    bool LoadFromLua();
    void SendCurrentNode();
    void SelectOption(uint32 nextNodeId);

private:
    PlayerObject* m_player;
    uint32 m_npcId;
    std::string m_scriptName;
    uint32 m_currentNodeId;
    std::vector<DialogNode> m_nodes;
};

#include "DialogTree.h"
#include "PlayerObject.h"
#include "ScriptEngine.h"
#include "Log.h"

DialogTree::DialogTree(PlayerObject* player, uint32 npcId, const std::string& scriptName)
    : m_player(player), m_npcId(npcId), m_scriptName(scriptName), m_currentNodeId(0)
{
}

DialogTree::~DialogTree()
{
}

bool DialogTree::LoadFromLua()
{
    INFO_LOG(format("Loading DialogTree %1% for NPC %2%") % m_scriptName % m_npcId);

    lua_State* L = sScriptEngine.GetState();
    if (!L) return false;

    // Load the dialog script
    std::string scriptPath = "Data/Dialogs/" + m_scriptName + ".lua";
    if (!sScriptEngine.ExecuteFile(scriptPath))
    {
        return false;
    }

    // Parse DialogNodes table
    lua_getglobal(L, "DialogNodes");
    if (!lua_istable(L, -1))
    {
        CRITICAL_LOG(format("Lua error: DialogNodes table not found in %1%") % scriptPath);
        lua_pop(L, 1);
        return false;
    }

    // Iterate over the DialogNodes table
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        // key is at -2, value (node table) is at -1
        uint32 nodeId = (uint32)lua_tointeger(L, -2);
        
        DialogNode node;
        node.id = nodeId;

        lua_getfield(L, -1, "text");
        if (lua_isstring(L, -1)) node.text = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "options");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                // option table at -1
                uint32 nextId = 0;
                std::string optText = "";

                lua_getfield(L, -1, "nextId");
                if (lua_isnumber(L, -1)) nextId = (uint32)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "text");
                if (lua_isstring(L, -1)) optText = lua_tostring(L, -1);
                lua_pop(L, 1);

                node.options.push_back({nextId, optText});
                lua_pop(L, 1); // pop option table
            }
        }
        lua_pop(L, 1); // pop options table

        m_nodes.push_back(node);
        lua_pop(L, 1); // pop value (node table)
    }
    lua_pop(L, 1); // pop DialogNodes table

    return true;
}

void DialogTree::SendCurrentNode()
{
    if (m_currentNodeId >= m_nodes.size()) return;

    DialogNode& node = m_nodes[m_currentNodeId];
    
    // In Reality, this would be serialized into an RPC or EventMsg and sent to m_player.
    // For Phase 4, we just mock the log.
    INFO_LOG(format("NPC Dialog sent to %1%: %2%") % m_player->getFirstName() % node.text);
    for (size_t i = 0; i < node.options.size(); ++i)
    {
        INFO_LOG(format("  Option %1%: %2%") % node.options[i].first % node.options[i].second);
    }
}

void DialogTree::SelectOption(uint32 nextNodeId)
{
    m_currentNodeId = nextNodeId;
    SendCurrentNode();
}

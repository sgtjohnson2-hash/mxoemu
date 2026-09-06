#include "CraftingSystem.h"
#include "PlayerObject.h"
#include "EconomySystem.h"
#include "Log.h"
#include "BotManager.h" // For messaging

createFileSingleton(CraftingSystem);

CraftingSystem::CraftingSystem()
{
}

CraftingSystem::~CraftingSystem()
{
}

#include "DataLoader.h"

void CraftingSystem::LoadBlueprints()
{
    m_blueprints = sDataLoader.GetAllBlueprints();
    INFO_LOG(format("Loaded %1% crafting blueprints from data file.") % m_blueprints.size());
}

bool CraftingSystem::HandleCraftRequest(PlayerObject* player, uint32 blueprintId)
{
    if (!player) return false;
    
    auto it = m_blueprints.find(blueprintId);
    if (it == m_blueprints.end()) 
    {
        sBotMgr.LogCombat("Invalid blueprint.");
        return false;
    }

    const CraftingBlueprint& bp = it->second;

    // 1. Check Info Cost
    if (player->getInfo() < bp.infoCost)
    {
        sBotMgr.LogCombat("Not enough Info to craft this item.");
        return false;
    }

    // 2. Check Components (Mocked for now since Inventory is stubbed)
    // if (!player->hasComponents(bp.requiredComponentIds)) return false;

    // 3. Consume Resources
    sEconomySys.TakeInfo(player, bp.infoCost, "Crafting Cost");
    // player->removeComponents(bp.requiredComponentIds);

    // 4. Give Result
    // templateId = 1000 for mocked logic
    player->giveItem(1000);
    sBotMgr.LogCombat((format("[Crafting] %1% crafted item: %2%") % player->getHandle() % bp.resultingName).str());
    INFO_LOG(format("Player %1% crafted %2%") % player->getHandle() % bp.resultingName);

    return true;
}

#include "CraftingSystem.h"
#include "PlayerObject.h"
#include "InventorySystem.h"
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

const CraftingBlueprint* CraftingSystem::GetBlueprint(uint32 blueprintId) const
{
    auto it = m_blueprints.find(blueprintId);
    if (it != m_blueprints.end()) return &it->second;
    return nullptr;
}

bool CraftingSystem::HandleCraftRequest(PlayerObject* player, uint32 blueprintId)
{
    if (!player) return false;
    
    // Ensure blueprints are loaded if empty
    if (m_blueprints.empty())
        LoadBlueprints();

    auto it = m_blueprints.find(blueprintId);
    if (it == m_blueprints.end()) 
    {
        sBotMgr.LogCombat("Invalid blueprint.");
        if (!player->getClient().isBot())
            player->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF4444}[CRAFTING] Unknown or invalid blueprint specifications.{/c}"));
        return false;
    }

    const CraftingBlueprint& bp = it->second;

    // 1. Check Info Cost
    if (player->getInfo() < bp.infoCost)
    {
        sBotMgr.LogCombat("Not enough Info to craft this item.");
        if (!player->getClient().isBot())
            player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:FF4444}[CRAFTING] Insufficient Information Bits. Cost: %1% bits, Available: %2% bits.{/c}")
                    % bp.infoCost % player->getInfo()).str()));
        return false;
    }

    // 2. Pre-validate required components BEFORE consuming anything (F25)
    if (!player->getInventory())
        return false;

    if (!player->isAdmin())
    {
        // Aggregate required component counts into a frequency map (F25)
        std::map<uint32, uint32> reqCounts;
        for (uint32 compId : bp.requiredComponentIds)
        {
            if (compId != 0)
                reqCounts[compId]++;
        }

        // Count available components in operative inventory (handling stacks and multiple slots)
        std::map<uint32, uint32> availCounts;
        for (const auto& item : player->getInventory()->getAllItems())
        {
            if (item)
            {
                uint32 count = item->getStackCount() > 0 ? item->getStackCount() : 1;
                availCounts[item->getTemplateId()] += count;
            }
        }

        for (const auto& req : reqCounts)
        {
            uint32 compId = req.first;
            uint32 reqCount = req.second;
            if (availCounts[compId] < reqCount)
            {
                if (!player->getClient().isBot())
                {
                    const ItemTemplate* compTpl = sDataLoader.GetItemTemplate(compId);
                    std::string compName = compTpl ? compTpl->name : (format("Component #%1%") % compId).str();
                    player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                        (format("{c:FF4444}[CRAFTING] Missing required component: %1%. Compilation aborted.{/c}") % compName).str()));
                }
                return false;
            }
        }
    }

    // 3. All components confirmed present: consume components and Info bits
    for (uint32 compId : bp.requiredComponentIds)
    {
        if (compId != 0 && player->getInventory()->hasItemByTemplate(compId))
            player->getInventory()->consumeItemByTemplate(compId);
    }
    sEconomySys.TakeInfo(player, bp.infoCost, "Crafting Cost");

    // 3. Give Resulting Item (map blueprint ID to authentic item template)
    uint32 resultItemTpl = 1000;
    if (blueprintId == 101) resultItemTpl = 1050; // Health Stim Patch / 9mm ammo
    else if (blueprintId == 102) resultItemTpl = 1050; // 9mm Pistol Ammo Clip
    else if (blueprintId == 103) resultItemTpl = 1051; // 5.56mm Rifle Ammo Clip
    else if (blueprintId == 104) resultItemTpl = 10111; // Nanoweave Coat
    else if (blueprintId == 108) resultItemTpl = 10103; // Dark Shades
    else if (blueprintId == 110) resultItemTpl = 1001;  // Dual Beretta 92FS
    else if (bp.blueprintId >= 1000 && bp.blueprintId < 2000) resultItemTpl = bp.blueprintId;

    player->giveItem(resultItemTpl);
    sBotMgr.LogCombat((format("[Crafting] %1% crafted item: %2%") % player->getHandle() % bp.resultingName).str());
    INFO_LOG(format("Player %1% crafted %2% (template %3%)") % player->getHandle() % bp.resultingName % resultItemTpl);

    if (!player->getClient().isBot())
    {
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:00FF00}[CRAFTING] Compilation complete! Synthesized: %1%{/c}") % bp.resultingName).str()));
    }

    return true;
}

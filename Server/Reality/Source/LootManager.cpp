#include "LootManager.h"
#include "PlayerObject.h"
#include "InventorySystem.h"
#include "DataLoader.h"
#include "Item.h"
#include "Log.h"
#include "MessageTypes.h"
#include "GameClient.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "MissionSystem.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

createFileSingleton(LootManager);

bool LootManager::LoadLootTables(const std::string& filename)
{
    std::string path = filename;
    if (path.empty() || !std::filesystem::exists(path)) path = "Data/Loot/loot_tables.csv";
    if (!std::filesystem::exists(path)) path = "mxoemu_live/Server/Reality/Data/Loot/loot_tables.csv";
    if (!std::filesystem::exists(path)) path = "../../Data/Loot/loot_tables.csv";
    if (!std::filesystem::exists(path)) path = "../Data/Loot/loot_tables.csv";
    if (!std::filesystem::exists(path)) path = "Data/hd_dump/loot_tables.csv";

    std::ifstream file(path);
    if (!file.is_open()) {
        WARNING_LOG(format("LootManager: Failed to open loot tables at %1%") % (filename.empty() ? path : filename));
        return false;
    }
    
    std::string line;
    // Skip header
    std::getline(file, line);
    
    m_lootTables.clear();
    int count = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        char delim = (line.find(';') != std::string::npos) ? ';' : ',';
        std::stringstream ss(line);
        std::string token;
        
        uint32 tableId = 0, templateId = 0;
        float chance = 0.0f;
        
        if (std::getline(ss, token, delim)) tableId = std::stoul(token);
        if (std::getline(ss, token, delim)) templateId = std::stoul(token);
        if (std::getline(ss, token, delim)) chance = std::stof(token);
        
        if (tableId > 0 && templateId > 0) {
            m_lootTables[tableId].push_back({templateId, chance});
            count++;
        }
    }
    
    INFO_LOG(format("LootManager: Loaded %1% loot entries across %2% tables.") % count % m_lootTables.size());
    return true;
}

uint32 LootManager::GetTotalLootEntries() const
{
    uint32 total = 0;
    for (const auto& pair : m_lootTables)
        total += (uint32)pair.second.size();
    return total;
}

const std::vector<LootEntry>* LootManager::GetLootTable(uint32 tableId) const
{
    auto it = m_lootTables.find(tableId);
    if (it != m_lootTables.end())
        return &it->second;
    return nullptr;
}

void LootManager::GenerateLoot(PlayerObject* killer, PlayerObject* victim)
{
    if (!killer || !victim) return;

    // Resolve loot table ID from victim
    uint32 tableId = victim->getLootTableId();
    if (tableId == 0)
    {
        std::string vHandle = victim->getHandle();
        if (vHandle.find("Agent") != std::string::npos)
            tableId = 4; // Agent Boss Loot Table
        else if (victim->getLevel() > 25)
            tableId = 3; // Elite / High-threat Table
        else if (victim->getLevel() > 10)
            tableId = 2; // Mid-level Syndicate / Police Table
        else
            tableId = 1; // Street Thugs / Slums Table
    }

    auto it = m_lootTables.find(tableId);
    if (it != m_lootTables.end())
    {
        const auto& allItems = sDataLoader.GetAllItems();
        bool toldFull = false;
        for (const auto& entry : it->second) {
            float roll = (float)(rand() % 10000) / 100.0f; // 0.00 to 99.99
            if (roll < entry.chance) {
                auto itemIt = allItems.find(entry.templateId);
                std::string itemName = (itemIt != allItems.end()) ? itemIt->second.name : (format("Item %1%") % entry.templateId).str();

                // addItem(slot 1) failed as soon as the starter weapon occupied slot 1,
                // so a kill never put the drop in the bag. giveItem takes the next free slot and saves it.
                if (!killer->giveItem(entry.templateId)) {
                    if (!toldFull && !killer->getClient().isBot()) {
                        killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                            "{c:FF5555}[LOOT] Inventory full.{/c}"));
                        toldFull = true;
                    }
                    continue;
                }

                int rarityRoll = rand() % 1000;
                std::string colorCode = "{c:FFFFFF}";
                if (rarityRoll < 5) colorCode = "{c:FF8800}";
                else if (rarityRoll < 50) colorCode = "{c:CC00FF}";
                else if (rarityRoll < 200) colorCode = "{c:0088FF}";
                else if (rarityRoll < 400) colorCode = "{c:00FF00}";

                if (!killer->getClient().isBot()) {
                    killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                        (format("%1%[LOOT] Defeated %2%! Looted item: %3%.{/c}") % colorCode % victim->getHandle() % itemName).str()
                    ));
                }
                INFO_LOG(format("LootManager: %1% looted %2% (template %3%) from %4%")
                    % killer->getHandle() % itemName % entry.templateId % victim->getHandle());
            }
        }
    }

    // Always check mission LOOT objective on victim kill
    try {
        sMissionSys.AdvanceObjective(killer, ObjectiveCommand::LOOT, victim->getGoId());
    } catch (...) {}
}

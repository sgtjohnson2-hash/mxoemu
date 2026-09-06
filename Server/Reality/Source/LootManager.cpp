#include "LootManager.h"
#include "PlayerObject.h"
#include "InventorySystem.h"
#include "DataLoader.h"
#include "Item.h"
#include "Log.h"
#include "MessageTypes.h"
#include "GameClient.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

createFileSingleton(LootManager);

bool LootManager::LoadLootTables(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        ERROR_LOG(format("Failed to open %1%") % filename);
        return false;
    }
    
    std::string line;
    // Skip header
    std::getline(file, line);
    
    int count = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        
        uint32 tableId = 0, templateId = 0;
        float chance = 0.0f;
        
        if (std::getline(ss, token, ',')) tableId = std::stoul(token);
        if (std::getline(ss, token, ',')) templateId = std::stoul(token);
        if (std::getline(ss, token, ',')) chance = std::stof(token);
        
        m_lootTables[tableId].push_back({templateId, chance});
        count++;
    }
    
    INFO_LOG(format("Loaded %1% loot entries across %2% tables.") % count % m_lootTables.size());
    return true;
}

void LootManager::GenerateLoot(PlayerObject* killer, PlayerObject* victim)
{
    if (!killer || !victim) return;

    // Item 44: Info (Currency) Economy - Drop info based on victim level
    uint64 infoDrop = (rand() % 50) + (victim->getLevel() * 10);
    killer->addInfo(infoDrop);
    
    killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
        (format("{c:00FFFF}You looted %1% Info from %2%.{/c}") % infoDrop % victim->getHandle()).str()
    ));

    uint32 tableId = 0; // victim->getLootTableId(); STUB REMOVED
    if (tableId == 0) return; // No custom loot table assigned

    auto it = m_lootTables.find(tableId);
    if (it == m_lootTables.end()) return;

    const auto& allItems = sDataLoader.GetAllItems();
    
    for (const auto& entry : it->second) {
        float roll = (float)(rand() % 10000) / 100.0f; // 0.00 to 99.99
        if (roll < entry.chance) {
            auto itemIt = allItems.find(entry.templateId);
            if (itemIt != allItems.end()) {
                if (killer->getInventory()) {
                    auto dropItem = std::make_shared<Item>(rand(), entry.templateId);
                    
                    // Item 39: Loot Tables v2 (Rarity)
                    int rarityRoll = rand() % 1000;
                    ItemRarity rarity = RARITY_COMMON;
                    std::string colorCode = "{c:FFFFFF}";
                    if (rarityRoll < 1) { rarity = RARITY_LEGENDARY; colorCode = "{c:FF8800}"; }
                    else if (rarityRoll < 20) { rarity = RARITY_EPIC; colorCode = "{c:CC00FF}"; }
                    else if (rarityRoll < 100) { rarity = RARITY_RARE; colorCode = "{c:0088FF}"; }
                    else if (rarityRoll < 300) { rarity = RARITY_UNCOMMON; colorCode = "{c:00FF00}"; }
                    
                    dropItem->setRarity(rarity);
                    
                    // Item 36: Source Code Corruption
                    bool isCorrupted = false;
                    if (rarity >= RARITY_RARE && (rand() % 100 < 5)) {
                        dropItem->setCorrupted(true);
                        isCorrupted = true;
                    }
                    
                    if (killer->getInventory()->addItemAuto(dropItem)) {
                        std::string itemName = itemIt->second.name;
                        if (isCorrupted) itemName = "[CORRUPTED] " + itemName;
                        
                        if (!killer->getClient().isBot()) {
                            killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                                (format("%1%You looted item: %2%.{/c}") % colorCode % itemName).str()
                            ));
                        }
                        INFO_LOG(format("LootManager: %1% looted %2% (Rarity: %3%, Corrupted: %4%)") 
                            % killer->getHandle() % itemName % (int)rarity % isCorrupted);
                    }
                }
            }
        }
    }
}

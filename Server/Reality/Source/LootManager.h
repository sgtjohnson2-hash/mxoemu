#ifndef MXOEMU_LOOTMANAGER_H
#define MXOEMU_LOOTMANAGER_H

#include "Common.h"
#include "Singleton.h"

class PlayerObject;

struct LootEntry {
    uint32 templateId;
    float chance; // 0.0 to 100.0
};

class LootManager : public Singleton<LootManager>
{
public:
    LootManager() {}
    ~LootManager() {}

    void GenerateLoot(PlayerObject* killer, PlayerObject* victim);
    bool LoadLootTables(const std::string& filename = "");

    uint32 GetTotalLootEntries() const;
    uint32 GetTotalLootTables() const { return (uint32)m_lootTables.size(); }
    const std::vector<LootEntry>* GetLootTable(uint32 tableId) const;

private:
    std::unordered_map<uint32, std::vector<LootEntry>> m_lootTables;
};

#define sLootMgr LootManager::getSingleton()

#endif // MXOEMU_LOOTMANAGER_H

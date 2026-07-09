#ifndef MXOEMU_DATALOADER_H
#define MXOEMU_DATALOADER_H

#include "Common.h"
#include "Singleton.h"
#include "AbilitySystem.h" // For AbilityTemplate
#include "Item.h"          // For ItemTemplate

struct NPCTemplate
{
    uint32 npcId;
    std::string name;
    uint8 level;
    uint32 health;
    uint32 innerStrength;
    std::string faction;
    std::string district;
    float x;
    float y;
    float z;
    float rot;
};

#include <string>
#include <map>
#include <vector>

class DataLoader : public Singleton<DataLoader>
{
public:
    DataLoader();
    ~DataLoader();

    bool LoadAll(const std::string& directoryPath);

    bool LoadAbilities(const std::string& filePath);
    bool LoadClothing(const std::string& filePath);
    bool LoadNPCs(const std::string& filePath);

    const AbilityTemplate* GetAbilityTemplate(uint16 id) const;
    const std::map<uint16, AbilityTemplate>& GetAllAbilities() const { return m_abilities; }
    const ItemTemplate* GetItemTemplate(uint32 id) const;
    const NPCTemplate* GetNPCTemplate(uint32 id) const;
    const std::map<uint32, NPCTemplate>& GetAllNPCs() const { return m_npcs; }

private:
    std::vector<std::string> SplitCSVLine(const std::string& line);

    std::map<uint16, AbilityTemplate> m_abilities;
    std::map<uint32, ItemTemplate> m_items;
    std::map<uint32, NPCTemplate> m_npcs;
};

#define sDataLoader DataLoader::getSingleton()

#endif // MXOEMU_DATALOADER_H

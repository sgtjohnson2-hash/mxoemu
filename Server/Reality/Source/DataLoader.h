#ifndef MXOEMU_DATALOADER_H
#define MXOEMU_DATALOADER_H

#include "Common.h"
#include "Singleton.h"
#include "AbilitySystem.h" // For AbilityTemplate
#include "Item.h"          // For ItemTemplate
#include "AI/BotPersonality.h"
#include "CraftingSystem.h" // For CraftingBlueprint

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
    std::string rsiHex;
    std::string weaponHex;
    bool isHostile;
};

struct PropheticGlitchNode
{
    uint32 nodeId{0};
    std::string district;
    float posX{0.0f};
    float posY{0.0f};
    float posZ{0.0f};
    std::string anomalyType;
    std::string loreReward;
    bool isUnmasked{false};
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

    bool LoadMissions(const std::string& directoryPath);

    bool LoadAbilities(const std::string& filePath);
    bool LoadClothing(const std::string& filePath);
    bool LoadNPCs(const std::string& filePath);
    bool LoadBlueprints(const std::string& filePath);
    bool LoadPropheticGlitchNodes(const std::string& filePath);

    const AbilityTemplate* GetAbilityTemplate(uint16 id) const;
    const std::map<uint16, AbilityTemplate>& GetAllAbilities() const { return m_abilities; }
    const ItemTemplate* GetItemTemplate(uint32 id) const;
    const std::map<uint32, ItemTemplate>& GetAllItems() const { return m_items; }
    const NPCTemplate* GetNPCTemplate(uint32 id) const;
    const std::map<uint32, NPCTemplate>& GetAllNPCs() const { return m_npcs; }
    const std::map<uint32, CraftingBlueprint>& GetAllBlueprints() const { return m_blueprints; }
    const std::map<uint32, PropheticGlitchNode>& GetAllGlitchNodes() const { return m_glitchNodes; }

    const BotPersonality* GetPersonalityProfile(uint32 index) const;

private:
    std::vector<std::string> SplitCSVLine(const std::string& line);

    std::map<uint16, AbilityTemplate> m_abilities;
    std::map<uint32, ItemTemplate> m_items;
    std::map<uint32, NPCTemplate> m_npcs;
    std::vector<BotPersonality> m_personalities;
    std::map<uint32, CraftingBlueprint> m_blueprints;
    std::map<uint32, PropheticGlitchNode> m_glitchNodes;
};

#define sDataLoader DataLoader::getSingleton()

#endif // MXOEMU_DATALOADER_H

// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// ***************************************************************************

#ifndef MXOEMU_ITEM_H
#define MXOEMU_ITEM_H

#include "Common.h"

// Define basic item types
enum ItemType
{
    ITEM_TYPE_GENERIC   = 0,
    ITEM_TYPE_WEAPON    = 1,
    ITEM_TYPE_CLOTHING  = 2,
    ITEM_TYPE_CONSUMABLE= 3,
    ITEM_TYPE_TOOL      = 4
};

enum ItemRarity
{
    RARITY_COMMON = 0,
    RARITY_UNCOMMON = 1,
    RARITY_RARE = 2,
    RARITY_EPIC = 3,
    RARITY_LEGENDARY = 4
};

// Represents a template for an item (data read from CSV/DB)
struct ItemTemplate
{
    uint32 templateId;
    string name;
    string description;
    ItemType type;
    uint32 rsiDataId; // RSI (visual model) identifier
    uint16 maxStack;
    uint32 value; // Info (cash) value
    
    // Weapon specifics (can be refactored into a subclass later if needed)
    uint16 minDamage;
    uint16 maxDamage;
    float attackSpeed;
    uint16 maxAmmo;
    bool isDualWield;

    // Clothing specifics
    uint16 bonusHacking;
    uint16 bonusEvasion;
};

// Represents an instantiated item in the world or in inventory
class Item
{
public:
    Item(uint32 goId, uint32 templateId) : m_goId(goId), m_templateId(templateId), m_stackCount(1), m_ammoCount(0) {}
    ~Item() {}
    
    uint32 getGoId() const { return m_goId; }
    uint32 getTemplateId() const { return m_templateId; }
    uint16 getStackCount() const { return m_stackCount; }
    void setStackCount(uint16 count) { m_stackCount = count; }
    
    std::string getMetadata() const { return m_metadataJSON; }
    void setMetadata(const std::string& metadata) { m_metadataJSON = metadata; }

    uint16 getAmmoCount() const { return m_ammoCount; }
    void setAmmoCount(uint16 ammo) { m_ammoCount = ammo; }

    float getDurability() const { return m_durability; }
    void setDurability(float dur) { m_durability = dur; }
    void reduceDurability(float amount) { m_durability -= amount; if(m_durability < 0) m_durability = 0; }

    ItemRarity getRarity() const { return m_rarity; }
    void setRarity(ItemRarity rarity) { m_rarity = rarity; }

    bool isCorrupted() const { return m_isCorrupted; }
    void setCorrupted(bool corrupted) { m_isCorrupted = corrupted; }

private:
    uint32 m_goId;
    uint32 m_templateId;
    uint16 m_stackCount;
    uint16 m_ammoCount;
    float m_durability = 100.0f;
    ItemRarity m_rarity = RARITY_COMMON;
    bool m_isCorrupted = false;
    std::string m_metadataJSON;
};

#endif // MXOEMU_ITEM_H

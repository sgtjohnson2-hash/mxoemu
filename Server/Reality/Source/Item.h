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
};

// Represents an instantiated item in the world or in inventory
class Item
{
public:
    Item(uint32 goId, uint32 templateId) : m_goId(goId), m_templateId(templateId), m_stackCount(1) {}
    ~Item() {}
    
    uint32 getGoId() const { return m_goId; }
    uint32 getTemplateId() const { return m_templateId; }
    uint16 getStackCount() const { return m_stackCount; }
    void setStackCount(uint16 count) { m_stackCount = count; }

private:
    uint32 m_goId;
    uint32 m_templateId;
    uint16 m_stackCount;
};

#endif // MXOEMU_ITEM_H

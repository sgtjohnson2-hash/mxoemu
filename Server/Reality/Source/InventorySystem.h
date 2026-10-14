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

#ifndef MXOEMU_INVENTORYSYSTEM_H
#define MXOEMU_INVENTORYSYSTEM_H

#include "Common.h"
#include "Item.h"

class PlayerObject;

class InventorySystem
{
public:
    InventorySystem(PlayerObject* owner);
    ~InventorySystem();

    // Load inventory from database
    void loadFromDB();
    
    // Save inventory to database (skips DB write if !m_dirty && !force)
    void saveToDB(bool force = false);
    bool isDirty() const { return m_dirty; }
    void markDirty() { m_dirty = true; }

    // Inventory operations
    bool addItem(shared_ptr<Item> item, uint8 slot);
    bool addItemAuto(shared_ptr<Item> item);
    uint8 getFirstFreeSlot() const;
    bool removeItem(uint32 goId);
    shared_ptr<Item> getItemByGoId(uint32 goId);
    shared_ptr<Item> getItemBySlot(uint8 slot);
    bool hasItemByTemplate(uint32 templateId);
    shared_ptr<Item> getItemByTemplate(uint32 templateId);
    bool consumeItemByTemplate(uint32 templateId);
    void clear();

    std::vector<shared_ptr<Item>> getAllItems() const;

    // Swap items between slots
    bool moveItem(uint8 fromSlot, uint8 toSlot);

    // Send inventory to client
    void sendFullInventory();

    // Value the client keeps per inventory item next to the item id (margin section 0x05 and
    // RPC 0x5E "Data"): stack count << 18 for stacks, purity << 26 otherwise.
    static uint32 ClientItemData(const Item &item);
    // Mirror slot changes to the owner's client (RPC 0x5E / 0x5F / 0x65). Off while loading.
    void notifyMove(uint8 fromSlot, uint8 toSlot);

private:
    PlayerObject* m_owner;
    bool m_dirty = false;
    
    // Map of Slot ID -> Item
    map<uint8, shared_ptr<Item>> m_items;
    
    // Fast lookup for goId -> Slot ID
    map<uint32, uint8> m_goIdToSlot;
};

#endif // MXOEMU_INVENTORYSYSTEM_H

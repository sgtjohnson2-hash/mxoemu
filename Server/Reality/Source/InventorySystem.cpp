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

#include "InventorySystem.h"
#include "PlayerObject.h"
#include "Database/Database.h"
#include "Log.h"
#include "DataLoader.h"

InventorySystem::InventorySystem(PlayerObject* owner) : m_owner(owner)
{
}

InventorySystem::~InventorySystem()
{
}

void InventorySystem::loadFromDB()
{
    m_items.clear();
    m_goIdToSlot.clear();
    
    // Load character inventory from the DB
    // Expected table: inventory (`invId`, `charId`, `goid`, `slot`)
    // TODO: A proper item template cache is required to lookup templateIds based on goid
    // For now, we load the raw DB rows
    format sql = format("SELECT `goid`, `slot` FROM `inventory` WHERE `charId` = '%1%'") % m_owner->getGuid();
    
    scoped_ptr<QueryResult> result(sDatabase.Query(sql));
    if (result)
    {
        do
        {
            Field *field = result->Fetch();
            uint32 goId = field[0].GetUInt32();
            uint8 slot = field[1].GetUInt8();
            
            // Note: We need a way to resolve goId -> templateId.
            // Ideally we'd look it up from an items table (goid -> templateid), 
            // but for now we query DataLoader if it has a template for this goId directly.
            uint32 templateId = 1; // Default
            if (sDataLoader.GetItemTemplate(goId))
                templateId = goId; // Assuming goId maps directly to templateId in this stub

            shared_ptr<Item> newItem(new Item(goId, templateId));
            
            m_items[slot] = newItem;
            m_goIdToSlot[goId] = slot;
            
        } while (result->NextRow());
    }
    
    INFO_LOG(format("Loaded %1% inventory items for %2%") % m_items.size() % m_owner->getHandle());
}

void InventorySystem::saveToDB()
{
    // Clear existing for simplicity (or use UPSERT/UPDATE in production)
    sDatabase.Execute(format("DELETE FROM `inventory` WHERE `charId` = '%1%'") % m_owner->getCharacterUID());
    
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
    {
        sDatabase.Execute(format("INSERT INTO `inventory` (`charId`, `goid`, `slot`) VALUES ('%1%', '%2%', '%3%')")
            % m_owner->getCharacterUID()
            % it->second->getGoId()
            % (uint32)it->first);
    }
}

bool InventorySystem::addItem(shared_ptr<Item> item, uint8 slot)
{
    if (m_items.find(slot) != m_items.end())
        return false; // Slot occupied
        
    m_items[slot] = item;
    m_goIdToSlot[item->getGoId()] = slot;
    return true;
}

bool InventorySystem::removeItem(uint32 goId)
{
    auto it = m_goIdToSlot.find(goId);
    if (it != m_goIdToSlot.end())
    {
        uint8 slot = it->second;
        m_items.erase(slot);
        m_goIdToSlot.erase(it);
        return true;
    }
    return false;
}

shared_ptr<Item> InventorySystem::getItemByGoId(uint32 goId)
{
    auto it = m_goIdToSlot.find(goId);
    if (it != m_goIdToSlot.end())
    {
        return m_items[it->second];
    }
    return nullptr;
}

shared_ptr<Item> InventorySystem::getItemBySlot(uint8 slot)
{
    auto it = m_items.find(slot);
    if (it != m_items.end())
    {
        return it->second;
    }
    return nullptr;
}

bool InventorySystem::moveItem(uint8 fromSlot, uint8 toSlot)
{
    auto itFrom = m_items.find(fromSlot);
    if (itFrom == m_items.end())
        return false; // Nothing to move
        
    auto itTo = m_items.find(toSlot);
    if (itTo != m_items.end())
    {
        // Swap
        shared_ptr<Item> itemTo = itTo->second;
        shared_ptr<Item> itemFrom = itFrom->second;
        
        m_items[toSlot] = itemFrom;
        m_items[fromSlot] = itemTo;
        
        m_goIdToSlot[itemFrom->getGoId()] = toSlot;
        m_goIdToSlot[itemTo->getGoId()] = fromSlot;
    }
    else
    {
        // Move
        shared_ptr<Item> itemFrom = itFrom->second;
        m_items[toSlot] = itemFrom;
        m_items.erase(fromSlot);
        m_goIdToSlot[itemFrom->getGoId()] = toSlot;
    }
    
    return true;
}

void InventorySystem::sendFullInventory()
{
    // TODO: Send packet to client with all items and their slots
}

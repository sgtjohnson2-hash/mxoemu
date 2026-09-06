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
#include "Database/AsyncDatabase.h"
#include "Database/PreparedStatement.h"
#include "Log.h"
#include "DataLoader.h"
#include "ItemSerializer.h"
#include "MessageTypes.h"
#include "GameClient.h"

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
    
    PreparedStatement stmt("SELECT `goid`, `slot`, `item_metadata` FROM `inventory` WHERE `charId` = ?0");
    stmt.SetUInt32(0, m_owner->getCharacterUID());
    scoped_ptr<QueryResult> result(sDatabase.QueryPrepared(&stmt));
    if (result)
    {
        do
        {
            Field *field = result->Fetch();
            uint32 goId = field[0].GetUInt32();
            uint8 slot = field[1].GetUInt8();
            std::string metadata = field[2].GetString();
            
            uint32 templateId = 1; // Default
            if (sDataLoader.GetItemTemplate(goId))
                templateId = goId; 

            shared_ptr<Item> newItem(new Item(goId, templateId));
            newItem->setMetadata(metadata);
            ItemSerializer::Deserialize(newItem, metadata);
            
            m_items[slot] = newItem;
            m_goIdToSlot[goId] = slot;
            
        } while (result->NextRow());
    }
    
    INFO_LOG(format("Loaded %1% inventory items for %2%") % m_items.size() % m_owner->getHandle());
}

void InventorySystem::saveToDB()
{
    PreparedStatement* delStmt = new PreparedStatement("DELETE FROM `inventory` WHERE `charId` = ?0");
    delStmt->SetUInt64(0, m_owner->getCharacterUID());
    sAsyncDatabase.Enqueue(delStmt);
    
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
    {
        PreparedStatement* insStmt = new PreparedStatement("INSERT INTO `inventory` (`charId`, `goid`, `slot`, `item_metadata`) VALUES (?0, ?1, ?2, ?3)");
        insStmt->SetUInt64(0, m_owner->getCharacterUID());
        insStmt->SetUInt32(1, it->second->getGoId());
        insStmt->SetUInt32(2, (uint32)it->first);
        std::string metadata = ItemSerializer::Serialize(it->second);
        it->second->setMetadata(metadata);
        insStmt->SetString(3, metadata);
        sAsyncDatabase.Enqueue(insStmt);
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

uint8 InventorySystem::getFirstFreeSlot() const
{
    for (uint8 i = 1; i <= 64; ++i) { // Assuming 64 slots max for now
        if (m_items.find(i) == m_items.end()) {
            return i;
        }
    }
    return 0; // Inventory full
}

bool InventorySystem::addItemAuto(shared_ptr<Item> item)
{
    uint8 slot = getFirstFreeSlot();
    if (slot == 0) return false;
    return addItem(item, slot);
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
    if (!m_owner || m_owner->getClient().isBot()) return;

    // TODO: Send binary 0x63 packet to client with all items and their slots once reverse engineered.
    // For now, we will dump the inventory state to the player's chat log for validation.
    std::string msg = "{c:00FF00}[Inventory Loader] Loaded " + std::to_string(m_items.size()) + " items.{/c}";
    m_owner->getClient().QueueCommand(std::make_shared<SystemChatMsg>(msg));
    
    for (const auto& pair : m_items) {
        std::string itemMsg = (format("{c:00FFFF}Slot %1%: GOID %2% Template %3%{/c}") 
            % (int)pair.first % pair.second->getGoId() % pair.second->getTemplateId()).str();
        m_owner->getClient().QueueCommand(std::make_shared<SystemChatMsg>(itemMsg));
    }
}

bool InventorySystem::hasItemByTemplate(uint32 templateId)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
    {
        if (it->second->getTemplateId() == templateId)
        {
            return true;
        }
    }
    return false;
}

shared_ptr<Item> InventorySystem::getItemByTemplate(uint32 templateId)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
    {
        if (it->second->getTemplateId() == templateId)
        {
            return it->second;
        }
    }
    return nullptr;
}

bool InventorySystem::consumeItemByTemplate(uint32 templateId)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
    {
        if (it->second->getTemplateId() == templateId)
        {
            removeItem(it->second->getGoId());
            return true;
        }
    }
    return false;
}

void InventorySystem::clear() {
    m_items.clear();
    m_goIdToSlot.clear();
}

std::vector<shared_ptr<Item>> InventorySystem::getAllItems() const {
    std::vector<shared_ptr<Item>> items;
    for (auto const& pair : m_items) {
        items.push_back(pair.second);
    }
    return items;
}

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
#include "ObjectMgr.h"
#include "GameServer.h"

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
    stmt.SetUInt64(0, m_owner->getCharacterUID());
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
            // Guns saved before ammo was tracked have ammoCount 0 and no ammoInit.
            // A magazine that was actually emptied is saved with ammoInit and stays empty.
            const ItemTemplate* loadedTpl = sDataLoader.GetItemTemplate(newItem->getTemplateId());
            if (loadedTpl && loadedTpl->type == ITEM_TYPE_WEAPON && loadedTpl->maxAmmo > 0
                && newItem->getAmmoCount() == 0
                && metadata.find("\"ammoInit\"") == std::string::npos)
            {
                newItem->setAmmoCount(loadedTpl->maxAmmo);
                metadata = ItemSerializer::Serialize(newItem);
                newItem->setMetadata(metadata);
            }
            
            m_items[slot] = newItem;
            m_goIdToSlot[goId] = slot;
            
        } while (result->NextRow());
    }
    
    // If player has no inventory saved, seed default starter operative kit
    if (m_items.empty() && m_owner && !m_owner->getClient().isBot())
    {
        struct StarterItemDef {
            uint32 templateId;
            uint8 slot;
            uint16 stack;
        };
        const StarterItemDef starterGear[] = {
            { 46568, 1, 1 },  // Area K Trenchcoat
            { 46407, 2, 1 },  // System Sunglasses
            { 47243, 3, 1 },  // Rave Pants
            { 40476, 4, 1 },  // Caution Shoes
            { 1002,  5, 1 },  // Beretta 92FS Pistol
            { 1050,  6, 20 }  // 9mm Pistol Ammo Clip (20 rounds)
        };
        
        for (const auto& sg : starterGear)
        {
            uint32 newGoId = sObjMgr.getNewItemId();
            shared_ptr<Item> item = make_shared<Item>(newGoId, sg.templateId);
            item->setStackCount(sg.stack);
            item->setDurability(100.0f);
            item->setRarity(RARITY_COMMON);
            const ItemTemplate* starterTpl = sDataLoader.GetItemTemplate(sg.templateId);
            if (starterTpl && starterTpl->type == ITEM_TYPE_WEAPON && starterTpl->maxAmmo > 0)
                item->setAmmoCount(starterTpl->maxAmmo);
            std::string meta = ItemSerializer::Serialize(item);
            item->setMetadata(meta);
            m_items[sg.slot] = item;
            m_goIdToSlot[newGoId] = sg.slot;
        }
        
        m_dirty = true;
        saveToDB(true);
        INFO_LOG(format("Granted default starter operative kit (%1% items) to %2%") % m_items.size() % m_owner->getHandle());
    }
    else
    {
        m_dirty = false;
    }

    INFO_LOG(format("Loaded %1% inventory items for %2%") % m_items.size() % m_owner->getHandle());
}

void InventorySystem::saveToDB(bool force)
{
    if (!m_owner) return;
    if (m_owner->getClient().isBot() || m_owner->getCharacterUID() >= 9000000) return;
    if (!force && !m_dirty) return;

    m_dirty = false;
    if (Database_Main == nullptr) return;

    PreparedStatement delStmt("DELETE FROM `inventory` WHERE `charId` = ?0");
    delStmt.SetUInt64(0, m_owner->getCharacterUID());
    sDatabase.ExecutePrepared(&delStmt);
    
    if (m_items.empty())
        return;

    std::ostringstream batchSql;
    batchSql << "INSERT INTO `inventory` (`charId`, `goid`, `slot`, `item_metadata`) VALUES ";
    bool first = true;
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
    {
        if (!it->second) continue;
        std::string metadata = ItemSerializer::Serialize(it->second);
        it->second->setMetadata(metadata);
        if (!first) batchSql << ",";
        first = false;
        batchSql << "(" << m_owner->getCharacterUID()
                 << "," << it->second->getGoId()
                 << "," << (uint32)it->first
                 << ",'" << sDatabase.EscapeString(metadata) << "')";
    }
    if (!first)
    {
        sDatabase.Execute(batchSql.str());
    }
}

bool InventorySystem::addItem(shared_ptr<Item> item, uint8 slot)
{
    if (m_items.find(slot) != m_items.end())
        return false; // Slot occupied
        
    m_items[slot] = item;
    m_goIdToSlot[item->getGoId()] = slot;
    m_dirty = true;
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
        m_dirty = true;
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
    m_dirty = true;
    return true;
}

void InventorySystem::sendFullInventory()
{
    if (!m_owner || m_owner->getClient().isBot()) return;

    std::string msg = "{c:00FF00}[Inventory] Loaded " + std::to_string(m_items.size()) + " items:{/c}";
    m_owner->getClient().QueueCommand(std::make_shared<SystemChatMsg>(msg));
    
    for (const auto& pair : m_items) {
        const ItemTemplate* tpl = sDataLoader.GetItemTemplate(pair.second->getTemplateId());
        std::string itemName = tpl ? tpl->name : "Item";
        std::string itemMsg = (format("{c:00FFFF}  Slot %1%: %2% (GOID %3%, Tpl %4%, x%5%){/c}") 
            % (int)pair.first % itemName % pair.second->getGoId() % pair.second->getTemplateId() % pair.second->getStackCount()).str();
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
            if (it->second->getStackCount() > 1)
            {
                it->second->setStackCount(it->second->getStackCount() - 1);
                std::string meta = ItemSerializer::Serialize(it->second);
                it->second->setMetadata(meta);
                m_dirty = true;
            }
            else
            {
                removeItem(it->second->getGoId());
            }
            if (m_owner && !m_owner->getClient().isBot())
            {
                saveToDB();
            }
            return true;
        }
    }
    return false;
}

void InventorySystem::clear() {
    m_items.clear();
    m_goIdToSlot.clear();
    m_dirty = true;
}

std::vector<shared_ptr<Item>> InventorySystem::getAllItems() const {
    std::vector<shared_ptr<Item>> items;
    for (auto const& pair : m_items) {
        items.push_back(pair.second);
    }
    return items;
}

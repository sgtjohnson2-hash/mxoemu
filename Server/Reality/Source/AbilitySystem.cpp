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

#include "AbilitySystem.h"
#include "PlayerObject.h"
#include "Database/Database.h"
#include "Log.h"
#include "Timer.h"

AbilitySystem::AbilitySystem(PlayerObject* owner) : m_owner(owner), m_maxMemory(100)
{
    if (m_owner)
    {
        m_maxMemory = m_owner->getLevel() * 5 + 50;
    }
}

AbilitySystem::~AbilitySystem()
{
}

void AbilitySystem::loadFromDB()
{
    m_loadedAbilities.clear();
    
    // In Reality, we need to create an `abilities` table if it doesn't exist
    // Expected table: `abilities` (`charId`, `abilityId`, `level`, `slot`)
    // If the table doesn't exist yet, we will just silently catch the SQL error for now
    format sql = format("SELECT `abilityId`, `level`, `slot` FROM `abilities` WHERE `charId` = '%1%'") % m_owner->getGuid();
    
    scoped_ptr<QueryResult> result(sDatabase.Query(sql));
    if (result)
    {
        do
        {
            Field *field = result->Fetch();
            uint16 abilityId = field[0].GetUInt16();
            uint16 level = field[1].GetUInt16();
            uint16 slot = field[2].GetUInt16();
            
            shared_ptr<Ability> newAbility(new Ability(abilityId, level, slot));
            m_loadedAbilities[abilityId] = newAbility;
            
        } while (result->NextRow());
        
        INFO_LOG(format("Loaded %1% abilities for %2%") % m_loadedAbilities.size() % m_owner->getHandle());
    }
}

void AbilitySystem::saveToDB()
{
    // Skip if table isn't created yet, but here is the logic:
    // sDatabase.Execute(format("DELETE FROM `abilities` WHERE `charId` = '%1%'") % m_owner->getCharacterUID());
    
    // for (auto it = m_loadedAbilities.begin(); it != m_loadedAbilities.end(); ++it)
    // {
    //     sDatabase.Execute(format("INSERT INTO `abilities` (`charId`, `abilityId`, `level`, `slot`) VALUES ('%1%', '%2%', '%3%', '%4%')")
    //         % m_owner->getCharacterUID()
    //         % it->second->getAbilityId()
    //         % it->second->getLevel()
    //         % it->second->getMemorySlot());
    // }
}

bool AbilitySystem::loadAbility(uint16 abilityId, uint16 level, uint16 slot)
{
    if (m_loadedAbilities.find(abilityId) != m_loadedAbilities.end())
        return false; // Already loaded

    // Hardline proximity check (placeholder)
    // if (!m_owner->isNearHardline()) return false;

    // For now, let's assume all abilities cost 10 memory
    // In a full implementation, we'd look up the AbilityTemplate by ID
    uint16 memoryCost = 10;
    
    if (getTotalMemoryUsed() + memoryCost > getMaxMemory())
    {
        INFO_LOG(format("Player %1% tried to load ability %2% but exceeded Memory.") % m_owner->getHandle() % abilityId);
        return false;
    }
    
    shared_ptr<Ability> newAbility(new Ability(abilityId, level, slot));
    m_loadedAbilities[abilityId] = newAbility;
    return true;
}

bool AbilitySystem::unloadAbility(uint16 abilityId)
{
    auto it = m_loadedAbilities.find(abilityId);
    if (it != m_loadedAbilities.end())
    {
        m_loadedAbilities.erase(it);
        return true;
    }
    return false;
}

shared_ptr<Ability> AbilitySystem::getAbility(uint16 abilityId)
{
    auto it = m_loadedAbilities.find(abilityId);
    if (it != m_loadedAbilities.end())
    {
        return it->second;
    }
    return nullptr;
}

uint16 AbilitySystem::getTotalMemoryUsed() const
{
    // TODO: Calculate based on AbilityTemplate
    return (uint16)m_loadedAbilities.size() * 10; 
}

uint16 AbilitySystem::getMaxMemory() const
{
    return m_maxMemory;
}

bool AbilitySystem::canCastAbility(uint16 abilityId) const
{
    auto it = m_loadedAbilities.find(abilityId);
    if (it == m_loadedAbilities.end())
        return false;
    auto ability = it->second;
        
    // TODO: Cooldown checks
    // uint64 now = getMSTime();
    // if (now - ability->getLastUsedTime() < cooldown) return false;
    
    return true;
}

void AbilitySystem::onAbilityCast(uint16 abilityId)
{
    auto ability = getAbility(abilityId);
    if (ability)
    {
        ability->setLastUsedTime(getMSTime());
    }
}

void AbilitySystem::sendFullLoadout()
{
    // TODO: Send packet to client with all loaded abilities
}

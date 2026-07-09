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

#ifndef MXOEMU_ABILITYSYSTEM_H
#define MXOEMU_ABILITYSYSTEM_H

#include "Common.h"

class PlayerObject;

enum class DisciplineType
{
    NONE = 0,
    CODER = 1,
    HACKER = 2,
    OPERATIVE = 3
};

// Basic structures for abilities
struct AbilityTemplate
{
    uint16 abilityId;
    int32 goId;
    string name;
    string description;
    uint16 memoryCost;
    uint16 innerStrengthCost;
    uint16 castTime; // in milliseconds
    uint16 cooldown; // in milliseconds
    uint8 maxLevel;
    DisciplineType discipline;
    bool isCastable;
    uint32 activationFX;
    uint32 executionFX;
    bool isBuff;
    uint32 buffTime;
};

// Represents an ability loaded into memory
class Ability
{
public:
    Ability(uint16 abilityId, uint16 level, uint16 memorySlot)
        : m_abilityId(abilityId), m_level(level), m_memorySlot(memorySlot), m_lastUsedTime(0) {}
    ~Ability() {}

    uint16 getAbilityId() const { return m_abilityId; }
    uint16 getLevel() const { return m_level; }
    uint16 getMemorySlot() const { return m_memorySlot; }
    uint64 getLastUsedTime() const { return m_lastUsedTime; }

    void setLastUsedTime(uint64 time) { m_lastUsedTime = time; }

private:
    uint16 m_abilityId;
    uint16 m_level;
    uint16 m_memorySlot;
    uint64 m_lastUsedTime;
};

class AbilitySystem
{
public:
    AbilitySystem(PlayerObject* owner);
    ~AbilitySystem();

    void loadFromDB();
    void saveToDB();

    bool loadAbility(uint16 abilityId, uint16 level, uint16 slot);
    bool unloadAbility(uint16 abilityId);
    shared_ptr<Ability> getAbility(uint16 abilityId);

    uint16 getTotalMemoryUsed() const;
    uint16 getMaxMemory() const;

    bool canCastAbility(uint16 abilityId) const;
    void onAbilityCast(uint16 abilityId);

    void sendFullLoadout();
    
    const map<uint16, shared_ptr<Ability>>& getLoadedAbilities() const { return m_loadedAbilities; }

private:
    PlayerObject* m_owner;
    uint16 m_maxMemory;
    
    // Map of abilityId -> Ability
    map<uint16, shared_ptr<Ability>> m_loadedAbilities;
};

#endif // MXOEMU_ABILITYSYSTEM_H

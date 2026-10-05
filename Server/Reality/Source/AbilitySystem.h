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
    OPERATIVE = 3,
    MARTIAL_ARTIST = 4,
    GUNNER = 5,
    SPY = 6
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

// One entry of the default new-character loadout. IDs/names are the REAL ability
// IDs from hd_reference/data/abilityIDs.csv (NOT the invented ids in Data/abilities.json).
struct DefaultLoadoutEntry
{
    uint16 abilityId;
    uint16 level;
    uint16 slot;
    const char* name;
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
    void clearLoadout();
    shared_ptr<Ability> getAbility(uint16 abilityId);

    uint16 getTotalMemoryUsed() const;
    uint16 getMaxMemory() const;

    bool canCastAbility(uint16 abilityId) const;
    void onAbilityCast(uint16 abilityId);

    // Sends one AbilityLoadRspMsg (RPC 0x80b2) per loaded ability so the client
    // populates its hotbar. Grants the default melee loadout first when nothing is loaded.
    void sendFullLoadout();

    // Default melee starter loadout (real ability ids) - also used by CombatSystem to
    // register server-side moves for these ids.
    static const std::vector<DefaultLoadoutEntry>& GetDefaultLoadout();
    // Retail wire constant seen in every captured 0x80b2 load message (bytes 08 02).
    static constexpr uint16 LOAD_RSP_TRAILER = 0x0208;
    
    const map<uint16, shared_ptr<Ability>>& getLoadedAbilities() const { return m_loadedAbilities; }

private:
    void grantDefaultLoadout();

    PlayerObject* m_owner;
    uint16 m_maxMemory;
    
    // Map of abilityId -> Ability
    map<uint16, shared_ptr<Ability>> m_loadedAbilities;
};

#endif // MXOEMU_ABILITYSYSTEM_H

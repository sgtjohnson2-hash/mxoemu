#include "ClubHelRaidSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(ClubHelRaidSystem);

ClubHelRaidSystem::ClubHelRaidSystem()
{
    Initialize();
}

void ClubHelRaidSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    m_simTimeSec = 0.0f;
    m_exiles.clear();
    m_combatants.clear();
    m_nextExileId = 1;
    m_nextCombatantId = 1;

    // Default Supernatural Exiles in Chateau
    SpawnExile(EXILE_WEREWOLF, 6000.0f, 180.0f);
    SpawnExile(EXILE_VAMPIRE,  4500.0f, 80.0f);
    SpawnExile(EXILE_GHOST,    3500.0f, 50.0f);

    // Reset Chateau Instance
    m_chateau.instanceId = 1;
    m_chateau.grandStaircaseIntegrity = 100.0f;
    m_chateau.destroyedStatues = 0;
    m_chateau.secretCellarUnlocked = false;
    m_chateau.persephoneVaultOpen = false;
    m_chateau.tradedMemoryFragments = 0;
}

void ClubHelRaidSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Simulate Supernatural Exiles Regeneration
    for (auto& pair : m_exiles)
    {
        auto& ex = pair.second;
        if (!ex.isDefeated && ex.health < ex.maxHealth)
        {
            ex.health = std::min(ex.maxHealth, ex.health + ex.regenerationRateHpPerSec * deltaTimeSec);
        }
    }

    // Simulate Combatants Gravity Inversion Timers
    for (auto& pair : m_combatants)
    {
        auto& c = pair.second;
        if (c.currentGravity == GRAVITY_CEILING)
        {
            c.ceilingInversionTimerSec += deltaTimeSec;
        }
    }
}

uint32 ClubHelRaidSystem::SpawnExile(ExileType type, float health, float regenRate)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    SupernaturalExile ex;
    ex.exileId = m_nextExileId++;
    ex.type = type;
    ex.health = health;
    ex.maxHealth = health;
    ex.regenerationRateHpPerSec = regenRate;
    ex.isImmuneToBallistics = (type == EXILE_VAMPIRE || type == EXILE_GHOST);
    ex.isDisruptedBySilver = false;
    ex.isDefeated = false;

    m_exiles[ex.exileId] = ex;
    return ex.exileId;
}

bool ClubHelRaidSystem::DamageExile(uint32 exileId, float damage, bool isSilverOrCryptographic)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    auto it = m_exiles.find(exileId);
    if (it == m_exiles.end() || it->second.isDefeated) return false;

    auto& ex = it->second;

    if (ex.isImmuneToBallistics && !isSilverOrCryptographic)
    {
        return false; // Ballistics completely deflected by supernatural subroutines
    }

    if (isSilverOrCryptographic)
    {
        ex.isDisruptedBySilver = true;
    }

    ex.health = std::max(0.0f, ex.health - damage);
    if (ex.health <= 0.0f)
    {
        ex.isDefeated = true;
    }
    return true;
}

uint32 ClubHelRaidSystem::RegisterCombatant(uint32 playerId)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    CoatCheckCombatant c;
    c.combatantId = m_nextCombatantId++;
    c.playerId = playerId;
    c.currentGravity = GRAVITY_FLOOR;
    c.ceilingInversionTimerSec = 0.0f;
    c.isHelicalBulletTimeActive = false;

    m_combatants[c.combatantId] = c;
    return c.combatantId;
}

bool ClubHelRaidSystem::ShiftCombatantGravity(uint32 combatantId, GravityPlane plane)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    auto it = m_combatants.find(combatantId);
    if (it == m_combatants.end()) return false;

    it->second.currentGravity = plane;
    return true;
}

bool ClubHelRaidSystem::TriggerHelicalBulletTime(uint32 combatantId, bool active)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    auto it = m_combatants.find(combatantId);
    if (it == m_combatants.end()) return false;

    it->second.isHelicalBulletTimeActive = active;
    return true;
}

bool ClubHelRaidSystem::TradePersephoneMemory(uint32 memoryFragmentCount, std::string& outVaultKey)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    if (memoryFragmentCount >= 5)
    {
        m_chateau.tradedMemoryFragments += memoryFragmentCount;
        m_chateau.persephoneVaultOpen = true;
        outVaultKey = "KEY-PERSEPHONE-7701";
        return true;
    }

    outVaultKey = "";
    return false;
}

bool ClubHelRaidSystem::DamageChateauEnvironment(float staircaseDamage, uint32 statuesShattered)
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    m_chateau.grandStaircaseIntegrity = std::max(0.0f, m_chateau.grandStaircaseIntegrity - staircaseDamage);
    m_chateau.destroyedStatues += statuesShattered;

    if (m_chateau.destroyedStatues >= 6)
    {
        m_chateau.secretCellarUnlocked = true;
    }
    return true;
}

size_t ClubHelRaidSystem::GetActiveExileCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    size_t count = 0;
    for (const auto& pair : m_exiles)
    {
        if (!pair.second.isDefeated) count++;
    }
    return count;
}

bool ClubHelRaidSystem::GetExile(uint32 exileId, SupernaturalExile& outExile) const
{
    std::lock_guard<std::recursive_mutex> lock(m_raidMutex);
    auto it = m_exiles.find(exileId);
    if (it != m_exiles.end())
    {
        outExile = it->second;
        return true;
    }
    return false;
}

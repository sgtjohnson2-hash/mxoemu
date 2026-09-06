#ifndef MXOEMU_CLUB_HEL_RAID_SYSTEM_H
#define MXOEMU_CLUB_HEL_RAID_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum ExileType
{
    EXILE_WEREWOLF = 1,
    EXILE_VAMPIRE  = 2,
    EXILE_GHOST    = 3
};

enum GravityPlane
{
    GRAVITY_FLOOR      = 0,
    GRAVITY_WALL_LEFT  = 1,
    GRAVITY_WALL_RIGHT = 2,
    GRAVITY_CEILING    = 3
};

struct SupernaturalExile
{
    uint32 exileId{1};
    ExileType type{EXILE_WEREWOLF};
    float health{5000.0f};
    float maxHealth{5000.0f};
    float regenerationRateHpPerSec{150.0f};
    bool isImmuneToBallistics{false};
    bool isDisruptedBySilver{false};
    bool isDefeated{false};
};

struct CoatCheckCombatant
{
    uint32 combatantId{1};
    uint32 playerId{0};
    GravityPlane currentGravity{GRAVITY_FLOOR};
    float ceilingInversionTimerSec{0.0f};
    bool isHelicalBulletTimeActive{false};
};

struct ChateauInstance
{
    uint32 instanceId{1};
    float grandStaircaseIntegrity{100.0f};
    uint32 destroyedStatues{0};
    bool secretCellarUnlocked{false};
    bool persephoneVaultOpen{false};
    uint32 tradedMemoryFragments{0};
};

class ClubHelRaidSystem : public Singleton<ClubHelRaidSystem>
{
public:
    ClubHelRaidSystem();
    ~ClubHelRaidSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Supernatural Exiles Combat
    uint32 SpawnExile(ExileType type, float health, float regenRate);
    bool DamageExile(uint32 exileId, float damage, bool isSilverOrCryptographic);

    // Club Hel Anti-Gravity Shootout
    uint32 RegisterCombatant(uint32 playerId);
    bool ShiftCombatantGravity(uint32 combatantId, GravityPlane plane);
    bool TriggerHelicalBulletTime(uint32 combatantId, bool active);

    // Chateau & Persephone Vault
    bool TradePersephoneMemory(uint32 memoryFragmentCount, std::string& outVaultKey);
    bool DamageChateauEnvironment(float staircaseDamage, uint32 statuesShattered);

    // Telemetry & Getters
    size_t GetActiveExileCount() const;
    const ChateauInstance& GetChateauState() const { return m_chateau; }
    bool GetExile(uint32 exileId, SupernaturalExile& outExile) const;
    bool IsVaultOpen() const { return m_chateau.persephoneVaultOpen; }

private:
    mutable std::recursive_mutex m_raidMutex;
    std::map<uint32, SupernaturalExile> m_exiles;
    std::map<uint32, CoatCheckCombatant> m_combatants;
    ChateauInstance m_chateau;

    uint32 m_nextExileId{1};
    uint32 m_nextCombatantId{1};
    float m_simTimeSec{0.0f};
};

#define sClubHelRaidSystem ClubHelRaidSystem::getSingleton()

#endif // MXOEMU_CLUB_HEL_RAID_SYSTEM_H

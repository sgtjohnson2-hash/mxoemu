#ifndef MXOEMU_ORACLE_COOKIE_SYSTEM_H
#define MXOEMU_ORACLE_COOKIE_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include "StatusEffectManager.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class PlayerObject;

enum MemoryDataFragmentId : uint32
{
    DATA_FRAG_CINNAMON_SUGAR   = 1, // Cinnamon Code-Sugar: sensory warmth and premonition anchor
    DATA_FRAG_ANOMALOUS_FLOUR  = 2, // Anomalous Flour: code milled from non-deterministic equations
    DATA_FRAG_SATI_SPICES      = 3, // Sati's Sunlight Spices: chromatic dawn light telemetry
    DATA_FRAG_SERAPHIC_YEAST   = 4, // Seraphic Yeast: purified angelic subroutines
    DATA_FRAG_BITTER_COCOA     = 5, // Bitter Cocoa Kernels: raw subterranean machine logic
    DATA_FRAG_SOURCE_SALT      = 6  // Source Salt: high-density mainframe residue
};

enum OracleCookieId : uint32
{
    COOKIE_PREMONITION_SNICKERDOODLE     = 101, // Boosts tactical foresight to 95%
    COOKIE_VIRAL_IMMUNITY_FORTUNE        = 102, // Cleanses & inoculates against Agent Smith virus
    COOKIE_FACTION_MIRAGE_WAFER          = 103, // Masks faction alignment from scanners & hostile entities
    COOKIE_ANOMALOUS_GLITCH_SHORTBREAD   = 104, // Unmasks prophetic glitches up to 75,000m
    COOKIE_SERAPHIC_HONEY_MADELEINE      = 105  // 60% kinetic absorption & 35% kinetic reflection
};

struct FragmentDefinition
{
    MemoryDataFragmentId id{DATA_FRAG_CINNAMON_SUGAR};
    std::string name;
    std::string loreDescription;
    std::string dropLocation;
};

struct CookieRecipe
{
    OracleCookieId cookieId{COOKIE_PREMONITION_SNICKERDOODLE};
    std::string name;
    std::string description;
    std::string metaphysicalEffect;
    std::map<MemoryDataFragmentId, uint32> requiredFragments;
    EffectType statusEffect{EFFECT_ORACLE_INTUITION};
    float durationSeconds{300.0f};
    float effectValue{1.0f};
    uint32 audioFxId{0x28000694}; // Authentic bite audio FX
};

struct OperativeBakeryPouch
{
    uint32 characterId{0};
    std::map<uint32, uint32> fragments;    // fragmentId -> count
    std::map<uint32, uint32> bakedCookies; // cookieId -> count
    uint32 totalBakes{0};
    uint32 totalCookiesEaten{0};
};

class OracleCookieSystem : public Singleton<OracleCookieSystem>
{
public:
    OracleCookieSystem();
    ~OracleCookieSystem();

    void Initialize();
    void Reset();

    // Data Fragment Management
    void AddFragment(uint32 characterId, MemoryDataFragmentId fragmentId, uint32 count = 1);
    bool RemoveFragment(uint32 characterId, MemoryDataFragmentId fragmentId, uint32 count = 1);
    uint32 GetFragmentCount(uint32 characterId, MemoryDataFragmentId fragmentId) const;
    const FragmentDefinition* GetFragmentDefinition(MemoryDataFragmentId fragmentId) const;
    const std::map<MemoryDataFragmentId, FragmentDefinition>& GetAllFragmentDefinitions() const { return m_fragmentDefs; }

    // Cookie Inventory Management
    uint32 GetCookieCount(uint32 characterId, OracleCookieId cookieId) const;
    void AddBakedCookie(uint32 characterId, OracleCookieId cookieId, uint32 count = 1);
    bool RemoveBakedCookie(uint32 characterId, OracleCookieId cookieId, uint32 count = 1);

    // Baking & Consumption Mechanics
    bool CanBake(uint32 characterId, OracleCookieId cookieId) const;
    bool Bake(uint32 characterId, OracleCookieId cookieId, std::string& outResult);
    bool ConsumeCookie(uint32 characterId, OracleCookieId cookieId, PlayerObject* player, std::string& outResult);

    // Recipe Queries
    const CookieRecipe* GetRecipe(OracleCookieId cookieId) const;
    const CookieRecipe* FindRecipeByName(const std::string& name) const;
    const std::map<OracleCookieId, CookieRecipe>& GetAllRecipes() const { return m_recipes; }

    // Operative Pouch State
    const OperativeBakeryPouch* GetPouch(uint32 characterId) const;
    void GrantStarterKit(uint32 characterId);

    // Persistence
    bool SaveBakeryState(uint32 characterId);
    bool LoadBakeryState(uint32 characterId);
    void EnsurePouchLoaded(uint32 characterId) const;

private:
    void RegisterFragmentsAndRecipes();

    mutable std::recursive_mutex m_bakeryMutex;
    std::map<MemoryDataFragmentId, FragmentDefinition> m_fragmentDefs;
    std::map<OracleCookieId, CookieRecipe> m_recipes;
    std::map<uint32, OperativeBakeryPouch> m_pouches;
};

#define sOracleCookie OracleCookieSystem::getSingleton()

#endif // MXOEMU_ORACLE_COOKIE_SYSTEM_H

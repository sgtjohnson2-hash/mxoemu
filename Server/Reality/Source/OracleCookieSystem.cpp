#include "OracleCookieSystem.h"
#include "PlayerObject.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "Log.h"
#include "Database/Database.h"
#include <boost/algorithm/string.hpp>
#include <sstream>

createFileSingleton(OracleCookieSystem);

OracleCookieSystem::OracleCookieSystem()
{
    Initialize();
}

OracleCookieSystem::~OracleCookieSystem()
{
}

void OracleCookieSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    m_fragmentDefs.clear();
    m_recipes.clear();
    m_pouches.clear();

    RegisterFragmentsAndRecipes();

    if (Log::getSingletonPtr())
    {
        sLog.outString("[OracleCookieSystem] Initialized Memory Bakery with %zu fragments and %zu recipes.",
                       m_fragmentDefs.size(), m_recipes.size());
    }
}

void OracleCookieSystem::Reset()
{
    Initialize();
}

void OracleCookieSystem::RegisterFragmentsAndRecipes()
{
    // 1. Fragment Definitions
    m_fragmentDefs[DATA_FRAG_CINNAMON_SUGAR] = {
        DATA_FRAG_CINNAMON_SUGAR,
        "Cinnamon Code-Sugar",
        "Sensory memory fragment extracted from the Oracle's kitchen. Radiates reassuring warmth and intuitive certainty.",
        "Downtown Tenement Kitchen"
    };

    m_fragmentDefs[DATA_FRAG_ANOMALOUS_FLOUR] = {
        DATA_FRAG_ANOMALOUS_FLOUR,
        "Anomalous Flour",
        "Refined code milled from non-deterministic equations and the prime anomaly substrate.",
        "01 Machine City / Sector 4 Fractures"
    };

    m_fragmentDefs[DATA_FRAG_SATI_SPICES] = {
        DATA_FRAG_SATI_SPICES,
        "Sati's Sunlight Spices",
        "Radiant chromatic spectrum telemetry woven by Sati during the Park East dawn cycle.",
        "Park East Living Horizon"
    };

    m_fragmentDefs[DATA_FRAG_SERAPHIC_YEAST] = {
        DATA_FRAG_SERAPHIC_YEAST,
        "Seraphic Yeast",
        "Purified angelic runtime code that causes logical structures to rise beyond static system bounds.",
        "Seraph Martial Threshold"
    };

    m_fragmentDefs[DATA_FRAG_BITTER_COCOA] = {
        DATA_FRAG_BITTER_COCOA,
        "Bitter Cocoa Kernels",
        "Raw subterranean machine logic harvested from the ruins of Morrell Station and Mobil Ave.",
        "Morrell Station / Mobil Ave Purgatory"
    };

    m_fragmentDefs[DATA_FRAG_SOURCE_SALT] = {
        DATA_FRAG_SOURCE_SALT,
        "Source Salt",
        "High-density crystalline residue crystallized from the central Machine City mainframe.",
        "Halborn Pre-Source Conduit"
    };

    // 2. Cookie Recipes
    // Recipe 1: Premonition Snickerdoodle
    CookieRecipe r1;
    r1.cookieId = COOKIE_PREMONITION_SNICKERDOODLE;
    r1.name = "Premonition Snickerdoodle";
    r1.description = "Golden cookie rolled in Cinnamon Code-Sugar. Sharpens tactical awareness to an uncanny edge.";
    r1.metaphysicalEffect = "Expands tactical foresight to 95% accuracy and grants deep premonition cues for 300s.";
    r1.requiredFragments[DATA_FRAG_CINNAMON_SUGAR] = 2;
    r1.requiredFragments[DATA_FRAG_ANOMALOUS_FLOUR] = 1;
    r1.requiredFragments[DATA_FRAG_SERAPHIC_YEAST] = 1;
    r1.statusEffect = EFFECT_ORACLE_PREMONITION_BOOST;
    r1.durationSeconds = 300.0f;
    r1.effectValue = 0.95f;
    r1.audioFxId = 0x28000694;
    m_recipes[r1.cookieId] = r1;

    // Recipe 2: Viral Immunity Fortune
    CookieRecipe r2;
    r2.cookieId = COOKIE_VIRAL_IMMUNITY_FORTUNE;
    r2.name = "Viral Immunity Fortune";
    r2.description = "Crisp, folded wafer infused with Seraphic Yeast and Bitter Cocoa. Inside lies an unbroken cryptographic seal.";
    r2.metaphysicalEffect = "Instantly cleanses Smith contagion and erects an impenetrable viral quarantine barrier for 180s.";
    r2.requiredFragments[DATA_FRAG_SERAPHIC_YEAST] = 2;
    r2.requiredFragments[DATA_FRAG_BITTER_COCOA] = 1;
    r2.requiredFragments[DATA_FRAG_SOURCE_SALT] = 1;
    r2.statusEffect = EFFECT_ORACLE_VIRAL_IMMUNITY;
    r2.durationSeconds = 180.0f;
    r2.effectValue = 1.0f;
    r2.audioFxId = 0x28000694;
    m_recipes[r2.cookieId] = r2;

    // Recipe 3: Faction Mirage Wafer
    CookieRecipe r3;
    r3.cookieId = COOKIE_FACTION_MIRAGE_WAFER;
    r3.name = "Faction Mirage Wafer";
    r3.description = "Translucent sugar wafer that refracts identity signatures across multiple factional matrices.";
    r3.metaphysicalEffect = "Obfuscates faction telemetry from scanners, turrets, and rival operatives for 300s.";
    r3.requiredFragments[DATA_FRAG_SATI_SPICES] = 1;
    r3.requiredFragments[DATA_FRAG_ANOMALOUS_FLOUR] = 1;
    r3.requiredFragments[DATA_FRAG_BITTER_COCOA] = 1;
    r3.statusEffect = EFFECT_FACTION_MASK;
    r3.durationSeconds = 300.0f;
    r3.effectValue = 1.0f;
    r3.audioFxId = 0x28000694;
    m_recipes[r3.cookieId] = r3;

    // Recipe 4: Anomalous Glitch Shortbread
    CookieRecipe r4;
    r4.cookieId = COOKIE_ANOMALOUS_GLITCH_SHORTBREAD;
    r4.name = "Anomalous Glitch Shortbread";
    r4.description = "Dense, buttery shortbread that hums with low-frequency resonance. Tastes like lightning and warm rain.";
    r4.metaphysicalEffect = "Expands anomaly detection radius to 75,000m and illuminates hidden glitch nodes for 600s.";
    r4.requiredFragments[DATA_FRAG_ANOMALOUS_FLOUR] = 2;
    r4.requiredFragments[DATA_FRAG_SATI_SPICES] = 2;
    r4.requiredFragments[DATA_FRAG_SOURCE_SALT] = 1;
    r4.statusEffect = EFFECT_ORACLE_GLITCH_SIGHT;
    r4.durationSeconds = 600.0f;
    r4.effectValue = 75000.0f;
    r4.audioFxId = 0x28000694;
    m_recipes[r4.cookieId] = r4;

    // Recipe 5: Seraphic Honey Madeleine
    CookieRecipe r5;
    r5.cookieId = COOKIE_SERAPHIC_HONEY_MADELEINE;
    r5.name = "Seraphic Honey Madeleine";
    r5.description = "Delicate shell-shaped sponge cake kissed with Sati's spices and honeyed angelic yeast.";
    r5.metaphysicalEffect = "Absorbs 60% kinetic force and reflects 35% damage during defensive posture for 240s.";
    r5.requiredFragments[DATA_FRAG_SERAPHIC_YEAST] = 2;
    r5.requiredFragments[DATA_FRAG_CINNAMON_SUGAR] = 1;
    r5.requiredFragments[DATA_FRAG_SATI_SPICES] = 1;
    r5.statusEffect = EFFECT_ORACLE_SERAPHIC_AEGIS;
    r5.durationSeconds = 240.0f;
    r5.effectValue = 0.60f;
    r5.audioFxId = 0x28000694;
    m_recipes[r5.cookieId] = r5;
}

void OracleCookieSystem::AddFragment(uint32 characterId, MemoryDataFragmentId fragmentId, uint32 count)
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto& pouch = m_pouches[characterId];
    pouch.characterId = characterId;
    pouch.fragments[fragmentId] += count;
}

bool OracleCookieSystem::RemoveFragment(uint32 characterId, MemoryDataFragmentId fragmentId, uint32 count)
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_pouches.find(characterId);
    if (it == m_pouches.end()) return false;

    auto fragIt = it->second.fragments.find(fragmentId);
    if (fragIt == it->second.fragments.end() || fragIt->second < count) return false;

    fragIt->second -= count;
    return true;
}

uint32 OracleCookieSystem::GetFragmentCount(uint32 characterId, MemoryDataFragmentId fragmentId) const
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_pouches.find(characterId);
    if (it == m_pouches.end()) return 0;

    auto fragIt = it->second.fragments.find(fragmentId);
    return (fragIt != it->second.fragments.end()) ? fragIt->second : 0;
}

const FragmentDefinition* OracleCookieSystem::GetFragmentDefinition(MemoryDataFragmentId fragmentId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_fragmentDefs.find(fragmentId);
    return (it != m_fragmentDefs.end()) ? &it->second : nullptr;
}

uint32 OracleCookieSystem::GetCookieCount(uint32 characterId, OracleCookieId cookieId) const
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_pouches.find(characterId);
    if (it == m_pouches.end()) return 0;

    auto cIt = it->second.bakedCookies.find(cookieId);
    return (cIt != it->second.bakedCookies.end()) ? cIt->second : 0;
}

void OracleCookieSystem::AddBakedCookie(uint32 characterId, OracleCookieId cookieId, uint32 count)
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto& pouch = m_pouches[characterId];
    pouch.characterId = characterId;
    pouch.bakedCookies[cookieId] += count;
}

bool OracleCookieSystem::RemoveBakedCookie(uint32 characterId, OracleCookieId cookieId, uint32 count)
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_pouches.find(characterId);
    if (it == m_pouches.end()) return false;

    auto cIt = it->second.bakedCookies.find(cookieId);
    if (cIt == it->second.bakedCookies.end() || cIt->second < count) return false;

    cIt->second -= count;
    return true;
}

bool OracleCookieSystem::CanBake(uint32 characterId, OracleCookieId cookieId) const
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    const CookieRecipe* recipe = GetRecipe(cookieId);
    if (!recipe) return false;

    auto it = m_pouches.find(characterId);
    if (it == m_pouches.end()) return false;

    for (const auto& req : recipe->requiredFragments)
    {
        auto fragIt = it->second.fragments.find(req.first);
        if (fragIt == it->second.fragments.end() || fragIt->second < req.second)
        {
            return false;
        }
    }
    return true;
}

bool OracleCookieSystem::Bake(uint32 characterId, OracleCookieId cookieId, std::string& outResult)
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    const CookieRecipe* recipe = GetRecipe(cookieId);
    if (!recipe)
    {
        outResult = "Unknown recipe.";
        return false;
    }

    if (!CanBake(characterId, cookieId))
    {
        outResult = "Insufficient data fragments to bake " + recipe->name + ".";
        return false;
    }

    auto& pouch = m_pouches[characterId];
    for (const auto& req : recipe->requiredFragments)
    {
        pouch.fragments[req.first] -= req.second;
    }

    pouch.bakedCookies[cookieId]++;
    pouch.totalBakes++;

    outResult = "Successfully baked [ " + recipe->name + " ] in the Oracle's Memory Oven! Fresh code fragrance fills the room.";
    SaveBakeryState(characterId);
    return true;
}

bool OracleCookieSystem::ConsumeCookie(uint32 characterId, OracleCookieId cookieId, PlayerObject* player, std::string& outResult)
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    const CookieRecipe* recipe = GetRecipe(cookieId);
    if (!recipe)
    {
        outResult = "Invalid cookie type.";
        return false;
    }

    if (GetCookieCount(characterId, cookieId) == 0)
    {
        outResult = "You do not possess any " + recipe->name + ".";
        return false;
    }

    RemoveBakedCookie(characterId, cookieId, 1);
    m_pouches[characterId].totalCookiesEaten++;

    if (player)
    {
        // Remove existing instance first so that eating a fresh cookie refreshes full duration
        sStatusEffectManager.RemoveEffectType(player->getGoId(), recipe->statusEffect);
        sStatusEffectManager.ApplyEffect(player->getGoId(), recipe->statusEffect, recipe->durationSeconds, 1.0f, recipe->effectValue);
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            "{c:FFB300}[Memory Bakery] Consumed [ " + recipe->name + " ]. " + recipe->metaphysicalEffect + "{/c}"
        ));
    }

    outResult = "Consumed " + recipe->name + ". " + recipe->metaphysicalEffect;
    SaveBakeryState(characterId);
    return true;
}

const CookieRecipe* OracleCookieSystem::GetRecipe(OracleCookieId cookieId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_recipes.find(cookieId);
    return (it != m_recipes.end()) ? &it->second : nullptr;
}

const CookieRecipe* OracleCookieSystem::FindRecipeByName(const std::string& name) const
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    for (const auto& pair : m_recipes)
    {
        if (boost::icontains(pair.second.name, name) ||
            (boost::iequals(name, "snickerdoodle") && pair.first == COOKIE_PREMONITION_SNICKERDOODLE) ||
            (boost::iequals(name, "premonition") && pair.first == COOKIE_PREMONITION_SNICKERDOODLE) ||
            (boost::iequals(name, "fortune") && pair.first == COOKIE_VIRAL_IMMUNITY_FORTUNE) ||
            (boost::iequals(name, "viral") && pair.first == COOKIE_VIRAL_IMMUNITY_FORTUNE) ||
            (boost::iequals(name, "wafer") && pair.first == COOKIE_FACTION_MIRAGE_WAFER) ||
            (boost::iequals(name, "mirage") && pair.first == COOKIE_FACTION_MIRAGE_WAFER) ||
            (boost::iequals(name, "shortbread") && pair.first == COOKIE_ANOMALOUS_GLITCH_SHORTBREAD) ||
            (boost::iequals(name, "glitch") && pair.first == COOKIE_ANOMALOUS_GLITCH_SHORTBREAD) ||
            (boost::iequals(name, "madeleine") && pair.first == COOKIE_SERAPHIC_HONEY_MADELEINE) ||
            (boost::iequals(name, "seraphic") && pair.first == COOKIE_SERAPHIC_HONEY_MADELEINE))
        {
            return &pair.second;
        }
    }
    return nullptr;
}

const OperativeBakeryPouch* OracleCookieSystem::GetPouch(uint32 characterId) const
{
    EnsurePouchLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_pouches.find(characterId);
    return (it != m_pouches.end()) ? &it->second : nullptr;
}

void OracleCookieSystem::GrantStarterKit(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    AddFragment(characterId, DATA_FRAG_CINNAMON_SUGAR, 3);
    AddFragment(characterId, DATA_FRAG_ANOMALOUS_FLOUR, 2);
    AddFragment(characterId, DATA_FRAG_SATI_SPICES, 2);
    AddFragment(characterId, DATA_FRAG_SERAPHIC_YEAST, 2);
    AddFragment(characterId, DATA_FRAG_BITTER_COCOA, 1);
    AddFragment(characterId, DATA_FRAG_SOURCE_SALT, 1);
}

bool OracleCookieSystem::SaveBakeryState(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    auto it = m_pouches.find(characterId);
    if (it == m_pouches.end()) return false;

    if (Database_Main == nullptr) return true;

    for (const auto& frag : it->second.fragments)
    {
        std::string sql = (format("REPLACE INTO `oracle_bakery_inventory` (`character_id`, `item_type`, `item_id`, `count`) VALUES (%1%, 'FRAGMENT', %2%, %3%);")
                           % characterId % frag.first % frag.second).str();
        sDatabase.Execute(sql);
    }
    for (const auto& cookie : it->second.bakedCookies)
    {
        std::string sql = (format("REPLACE INTO `oracle_bakery_inventory` (`character_id`, `item_type`, `item_id`, `count`) VALUES (%1%, 'COOKIE', %2%, %3%);")
                           % characterId % cookie.first % cookie.second).str();
        sDatabase.Execute(sql);
    }
    return true;
}

bool OracleCookieSystem::LoadBakeryState(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    if (Database_Main == nullptr) return false;

    std::string sql = (format("SELECT `item_type`, `item_id`, `count` FROM `oracle_bakery_inventory` WHERE `character_id` = %1%;") % characterId).str();
    QueryResult* result = sDatabase.Query(sql);
    if (!result) return false;

    auto& pouch = m_pouches[characterId];
    pouch.characterId = characterId;

    do
    {
        Field* fields = result->Fetch();
        if (fields)
        {
            std::string type = fields[0].GetString();
            uint32 id = fields[1].GetUInt32();
            uint32 count = fields[2].GetUInt32();

            if (type == "FRAGMENT") pouch.fragments[id] = count;
            else if (type == "COOKIE") pouch.bakedCookies[id] = count;
        }
    } while (result->NextRow());

    delete result;
    return true;
}

void OracleCookieSystem::EnsurePouchLoaded(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_bakeryMutex);
    if (m_pouches.find(characterId) == m_pouches.end())
    {
        const_cast<OracleCookieSystem*>(this)->LoadBakeryState(characterId);
    }
}


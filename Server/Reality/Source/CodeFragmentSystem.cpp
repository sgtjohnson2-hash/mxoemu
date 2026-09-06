#include "CodeFragmentSystem.h"
#include "PlayerObject.h"
#include "Log.h"
#include "BotManager.h"
#include "AbilitySystem.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"
#include <sstream>
#include <iomanip>
#include <chrono>

createFileSingleton(CodeFragmentSystem);

CodeFragmentSystem::CodeFragmentSystem()
    : m_nextFragmentId(1)
{
    SetupDefaultRecipes();
}

CodeFragmentSystem::~CodeFragmentSystem()
{
}

void CodeFragmentSystem::Initialize()
{
    SetupDefaultRecipes();
    std::lock_guard<std::mutex> lock(m_mutex);
    try
    {
        scoped_ptr<QueryResult> res(sDatabase.Query("SELECT `fragmentId`, `charId`, `fragmentType`, `sequenceData`, `quality` FROM `code_fragments`"));
        if (res)
        {
            do
            {
                Field* f = res->Fetch();
                CodeFragment frag;
                frag.fragmentId = f[0].GetUInt64();
                frag.charId = f[1].GetUInt64();
                frag.type = static_cast<FragmentType>(f[2].GetUInt32());
                frag.sequenceHash = f[3].GetString();
                frag.quality = f[4].GetUInt32();
                m_playerFragments[frag.charId].push_back(frag);
                if (frag.fragmentId >= m_nextFragmentId) m_nextFragmentId = frag.fragmentId + 1;
            } while (res->NextRow());
        }
    }
    catch (...) {}

    INFO_LOG(format("CodeFragmentSystem: Initialized with %1% compilation recipes.") % m_recipes.size());
}

void CodeFragmentSystem::SetupDefaultRecipes()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recipes.clear();

    // 1. Hyper-Kick Martial Arts Ability
    AbilityRecipe r1;
    r1.recipeId = 1;
    r1.abilityId = 2001;
    r1.abilityName = "Hyper-Kick Martial Arts Strike";
    r1.memoryCostBytes = 128;
    r1.reqLogic = 2;
    r1.reqArithmetic = 1;
    r1.reqMemory = 1;
    r1.reqViral = 0;
    m_recipes.push_back(r1);

    // 2. Subsonic Dash Evasion
    AbilityRecipe r2;
    r2.recipeId = 2;
    r2.abilityId = 2002;
    r2.abilityName = "Subsonic Dash Wire-Fu Evasion";
    r2.memoryCostBytes = 256;
    r2.reqLogic = 3;
    r2.reqArithmetic = 0;
    r2.reqMemory = 2;
    r2.reqViral = 0;
    m_recipes.push_back(r2);

    // 3. Viral Nullifier Anti-Agent Protocol
    AbilityRecipe r3;
    r3.recipeId = 3;
    r3.abilityId = 2003;
    r3.abilityName = "Viral Nullifier Code Purge";
    r3.memoryCostBytes = 256;
    r3.reqLogic = 2;
    r3.reqArithmetic = 0;
    r3.reqMemory = 2;
    r3.reqViral = 2;
    m_recipes.push_back(r3);

    // 4. Bullet Deflection Wave
    AbilityRecipe r4;
    r4.recipeId = 4;
    r4.abilityId = 2004;
    r4.abilityName = "Bullet Deflection Wave Kinetic Field";
    r4.memoryCostBytes = 512;
    r4.reqLogic = 3;
    r4.reqArithmetic = 2;
    r4.reqMemory = 1;
    r4.reqViral = 0;
    m_recipes.push_back(r4);
}

uint64 CodeFragmentSystem::ExtractFragment(PlayerObject* player, FragmentType type, uint32 quality)
{
    if (!player) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    uint64 id = m_nextFragmentId++;

    std::stringstream ss;
    ss << "0x" << std::hex << id << "-" << static_cast<uint32>(type) << "-" << quality;

    CodeFragment frag;
    frag.fragmentId = id;
    frag.charId = player->getCharId();
    frag.type = type;
    frag.sequenceHash = ss.str();
    frag.quality = quality;

    m_playerFragments[player->getCharId()].push_back(frag);

    try
    {
        PreparedStatement* stmt = new PreparedStatement("INSERT INTO `code_fragments` (`fragmentId`, `charId`, `fragmentType`, `sequenceData`, `quality`) VALUES (?0, ?1, ?2, ?3, ?4)");
        stmt->SetUInt64(0, id);
        stmt->SetUInt64(1, player->getCharacterUID());
        stmt->SetUInt32(2, static_cast<uint32>(type));
        stmt->SetString(3, frag.sequenceHash);
        stmt->SetUInt32(4, quality);
        sDatabase.ExecutePrepared(stmt);
    }
    catch (...) {}

    INFO_LOG(format("Player %1% extracted Code Fragment (ID: %2%, Type: %3%, Quality: %4%)")
             % player->getHandle() % id % static_cast<uint32>(type) % quality);
    sBotMgr.LogCombat((format("Extracted code fragment: %1% (Quality %2%).") % frag.sequenceHash % quality).str());

    return id;
}

std::vector<CodeFragment> CodeFragmentSystem::GetPlayerFragments(uint64 charId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_playerFragments.find(charId);
    if (it != m_playerFragments.end()) return it->second;
    return {};
}

size_t CodeFragmentSystem::GetFragmentCount(uint64 charId, FragmentType type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_playerFragments.find(charId);
    if (it == m_playerFragments.end()) return 0;

    size_t count = 0;
    for (const auto& frag : it->second)
    {
        if (frag.type == type) count++;
    }
    return count;
}

const AbilityRecipe* CodeFragmentSystem::GetRecipe(uint32 recipeId) const
{
    for (const auto& r : m_recipes)
    {
        if (r.recipeId == recipeId) return &r;
    }
    return nullptr;
}

bool CodeFragmentSystem::CompileAbility(PlayerObject* player, uint32 recipeId)
{
    if (!player) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    const AbilityRecipe* recipe = GetRecipe(recipeId);
    if (!recipe)
    {
        sBotMgr.LogCombat("Invalid code compilation recipe.");
        return false;
    }

    uint64 charId = player->getCharId();
    auto it = m_playerFragments.find(charId);
    if (it == m_playerFragments.end())
    {
        sBotMgr.LogCombat("No code fragments available to compile.");
        return false;
    }

    auto& frags = it->second;

    // Count available
    uint32 logicCount = 0, arithCount = 0, memCount = 0, viralCount = 0;
    for (const auto& f : frags)
    {
        if (f.type == FRAGMENT_LOGIC) logicCount++;
        else if (f.type == FRAGMENT_ARITHMETIC) arithCount++;
        else if (f.type == FRAGMENT_MEMORY) memCount++;
        else if (f.type == FRAGMENT_VIRAL) viralCount++;
    }

    if (logicCount < recipe->reqLogic ||
        arithCount < recipe->reqArithmetic ||
        memCount < recipe->reqMemory ||
        viralCount < recipe->reqViral)
    {
        sBotMgr.LogCombat("Insufficient code fragments to satisfy recipe requirements.");
        return false;
    }

    // Consume required fragments
    uint32 neededLogic = recipe->reqLogic;
    uint32 neededArith = recipe->reqArithmetic;
    uint32 neededMem = recipe->reqMemory;
    uint32 neededViral = recipe->reqViral;

    std::vector<uint64> consumedIds;
    for (auto fragIt = frags.begin(); fragIt != frags.end(); )
    {
        if (fragIt->type == FRAGMENT_LOGIC && neededLogic > 0)
        {
            neededLogic--;
            consumedIds.push_back(fragIt->fragmentId);
            fragIt = frags.erase(fragIt);
        }
        else if (fragIt->type == FRAGMENT_ARITHMETIC && neededArith > 0)
        {
            neededArith--;
            consumedIds.push_back(fragIt->fragmentId);
            fragIt = frags.erase(fragIt);
        }
        else if (fragIt->type == FRAGMENT_MEMORY && neededMem > 0)
        {
            neededMem--;
            consumedIds.push_back(fragIt->fragmentId);
            fragIt = frags.erase(fragIt);
        }
        else if (fragIt->type == FRAGMENT_VIRAL && neededViral > 0)
        {
            neededViral--;
            consumedIds.push_back(fragIt->fragmentId);
            fragIt = frags.erase(fragIt);
        }
        else
        {
            ++fragIt;
        }
    }

    m_compiledAbilities[charId].push_back(recipe->abilityId);

    if (player->getAbilitySystem())
    {
        player->getAbilitySystem()->loadAbility(static_cast<uint16>(recipe->abilityId), 1, 1);
        player->getAbilitySystem()->saveToDB();
    }

    try
    {
        for (uint64 cid : consumedIds)
        {
            PreparedStatement* delStmt = new PreparedStatement("DELETE FROM `code_fragments` WHERE `fragmentId` = ?0");
            delStmt->SetUInt64(0, cid);
            sDatabase.ExecutePrepared(delStmt);
        }
    }
    catch (...) {}

    INFO_LOG(format("Player %1% successfully compiled ability '%2%' (ID: %3%) into memory blocks.")
             % player->getHandle() % recipe->abilityName % recipe->abilityId);
    sBotMgr.LogCombat((format("COMPILED: %1% loaded into memory RAM.") % recipe->abilityName).str());

    return true;
}

std::vector<uint32> CodeFragmentSystem::GetCompiledAbilities(uint64 charId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_compiledAbilities.find(charId);
    if (it != m_compiledAbilities.end()) return it->second;
    return {};
}

bool CodeFragmentSystem::HasCompiledAbility(uint64 charId, uint32 abilityId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_compiledAbilities.find(charId);
    if (it == m_compiledAbilities.end()) return false;
    for (uint32 id : it->second)
    {
        if (id == abilityId) return true;
    }
    return false;
}

#ifndef MXOEMU_CODE_FRAGMENT_SYSTEM_H
#define MXOEMU_CODE_FRAGMENT_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>

class PlayerObject;

enum FragmentType : uint32
{
    FRAGMENT_LOGIC       = 1,
    FRAGMENT_ARITHMETIC  = 2,
    FRAGMENT_MEMORY      = 3,
    FRAGMENT_VIRAL       = 4
};

struct CodeFragment
{
    uint64 fragmentId;
    uint64 charId;
    FragmentType type;
    std::string sequenceHash;
    uint32 quality;
};

struct AbilityRecipe
{
    uint32 recipeId;
    uint32 abilityId;
    std::string abilityName;
    uint32 memoryCostBytes;
    uint32 reqLogic;
    uint32 reqArithmetic;
    uint32 reqMemory;
    uint32 reqViral;
};

class CodeFragmentSystem : public Singleton<CodeFragmentSystem>
{
public:
    CodeFragmentSystem();
    ~CodeFragmentSystem();

    void Initialize();

    // Fragment Extraction
    uint64 ExtractFragment(PlayerObject* player, FragmentType type, uint32 quality);
    std::vector<CodeFragment> GetPlayerFragments(uint64 charId) const;
    size_t GetFragmentCount(uint64 charId, FragmentType type) const;

    // Ability Compiling
    const std::vector<AbilityRecipe>& GetRecipes() const { return m_recipes; }
    const AbilityRecipe* GetRecipe(uint32 recipeId) const;
    bool CompileAbility(PlayerObject* player, uint32 recipeId);
    std::vector<uint32> GetCompiledAbilities(uint64 charId) const;
    bool HasCompiledAbility(uint64 charId, uint32 abilityId) const;

private:
    void SetupDefaultRecipes();

    mutable std::mutex m_mutex;
    uint64 m_nextFragmentId;
    std::map<uint64, std::vector<CodeFragment>> m_playerFragments;
    std::map<uint64, std::vector<uint32>> m_compiledAbilities;
    std::vector<AbilityRecipe> m_recipes;
};

#define sCodeFragmentSys CodeFragmentSystem::getSingleton()

#endif // MXOEMU_CODE_FRAGMENT_SYSTEM_H

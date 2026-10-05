#ifndef HACKERSYSTEM_H
#define HACKERSYSTEM_H

#include "Common.h"
#include "Singleton.h"

class PlayerObject;

class HackerSystem : public Singleton<HackerSystem> {
public:
    HackerSystem();
    ~HackerSystem();

    void Initialize();
    
    // Core Discipline Ability Dispatch
    bool ExecuteHackerAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const struct AbilityTemplate* templ);
    bool ExecuteCoderAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const struct AbilityTemplate* templ);
    bool ExecuteSoldierAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const struct AbilityTemplate* templ);
    bool ExecuteSpyAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const struct AbilityTemplate* templ);

    // Legacy signatures
    bool CompileProgram(PlayerObject* hacker, uint32 programId, uint32 targetGoId);
    bool ExecuteProgram(PlayerObject* hacker, uint32 programId, uint32 targetGoId);
    bool ExtractSourceCode(PlayerObject* hacker, uint32 targetGoId);
};

#define sHackerSystem Singleton<HackerSystem>::getSingleton()

#endif

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

    void SetForceDelayedCast(bool force) { m_forceDelayedCast = force; }
    bool ShouldDelayCast(float castSec) const;

private:
    bool m_forceDelayedCast = false;
};

#define sHackerSystem Singleton<HackerSystem>::getSingleton()

#endif

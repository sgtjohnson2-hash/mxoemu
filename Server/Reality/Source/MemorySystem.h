#ifndef MXOEMU_MEMORYSYSTEM_H
#define MXOEMU_MEMORYSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>

class PlayerObject;

class MemorySystem : public Singleton<MemorySystem>
{
public:
    MemorySystem();
    ~MemorySystem();

    bool LoadAbility(PlayerObject* player, uint32 abilityId);
    bool UnloadAbility(PlayerObject* player, uint32 abilityId);
    bool UpgradeAbility(PlayerObject* player, uint32 abilityId);
    bool ChangeTactic(PlayerObject* player, uint32 tacticId);
};

#define sMemorySys MemorySystem::getSingleton()

#endif // MXOEMU_MEMORYSYSTEM_H

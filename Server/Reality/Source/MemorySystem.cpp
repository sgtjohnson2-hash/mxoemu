#include "MemorySystem.h"
#include "PlayerObject.h"
#include "Log.h"
#include "BotManager.h"

initialiseSingleton(MemorySystem);

MemorySystem::MemorySystem()
{
}

MemorySystem::~MemorySystem()
{
}

bool MemorySystem::LoadAbility(PlayerObject* player, uint32 abilityId)
{
    if (!player) return false;
    INFO_LOG(format("Player %1% loaded ability %2% into memory.") % player->getHandle() % abilityId);
    sBotMgr.LogCombat((format("You loaded ability %1% into RAM.") % abilityId).str());
    return true;
}

bool MemorySystem::UnloadAbility(PlayerObject* player, uint32 abilityId)
{
    if (!player) return false;
    INFO_LOG(format("Player %1% unloaded ability %2% from memory.") % player->getHandle() % abilityId);
    sBotMgr.LogCombat((format("You unloaded ability %1% from RAM.") % abilityId).str());
    return true;
}

bool MemorySystem::UpgradeAbility(PlayerObject* player, uint32 abilityId)
{
    if (!player) return false;
    INFO_LOG(format("Player %1% upgraded ability %2%.") % player->getHandle() % abilityId);
    sBotMgr.LogCombat((format("You upgraded ability %1%.") % abilityId).str());
    return true;
}

bool MemorySystem::ChangeTactic(PlayerObject* player, uint32 tacticId)
{
    if (!player) return false;
    INFO_LOG(format("Player %1% changed tactic to %2%.") % player->getHandle() % tacticId);
    sBotMgr.LogCombat((format("Combat tactic changed to %1%.") % tacticId).str());
    return true;
}

#ifndef MXOEMU_CRAFTINGSYSTEM_H
#define MXOEMU_CRAFTINGSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>
#include <vector>

class PlayerObject;

struct CraftingBlueprint
{
    uint32 blueprintId;
    std::string resultingName;
    std::vector<uint32> requiredComponentIds;
    uint32 craftTimeMs;
    uint32 infoCost;
};

class CraftingSystem : public Singleton<CraftingSystem>
{
public:
    CraftingSystem();
    ~CraftingSystem();

    void LoadBlueprints();
    bool HandleCraftRequest(PlayerObject* player, uint32 blueprintId);

private:
    std::map<uint32, CraftingBlueprint> m_blueprints;
};

#define sCraftSys CraftingSystem::getSingleton()

#endif // MXOEMU_CRAFTINGSYSTEM_H

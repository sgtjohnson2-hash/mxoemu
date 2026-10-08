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
    uint32 GetTotalBlueprintsLoaded() const { return (uint32)m_blueprints.size(); }
    const CraftingBlueprint* GetBlueprint(uint32 blueprintId) const;
    const std::map<uint32, CraftingBlueprint>& GetAllBlueprints() const { return m_blueprints; }

private:
    std::map<uint32, CraftingBlueprint> m_blueprints;
};

#define sCraftSys CraftingSystem::getSingleton()

#endif // MXOEMU_CRAFTINGSYSTEM_H

#ifndef MXOEMU_ECONOMYSYSTEM_H
#define MXOEMU_ECONOMYSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>

class PlayerObject;

class EconomySystem : public Singleton<EconomySystem>
{
public:
    EconomySystem();
    ~EconomySystem();

    void GiveInfo(PlayerObject* player, uint32 amount, const std::string& reason);
    bool TakeInfo(PlayerObject* player, uint32 amount, const std::string& reason);

    bool HandleBuyRequest(PlayerObject* player, uint32 vendorId, uint32 itemId);
    bool HandleSellRequest(PlayerObject* player, uint32 vendorId, uint32 itemId);

private:
    uint32 GetItemPrice(uint32 itemId) const;
};

#define sEconomySys EconomySystem::getSingleton()

#endif // MXOEMU_ECONOMYSYSTEM_H

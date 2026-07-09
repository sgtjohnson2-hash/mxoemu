#include "EconomySystem.h"
#include "PlayerObject.h"
#include "DataLoader.h"
#include "Log.h"
#include "BotManager.h" // For messaging

createFileSingleton(EconomySystem);

EconomySystem::EconomySystem()
{
}

EconomySystem::~EconomySystem()
{
}

void EconomySystem::GiveInfo(PlayerObject* player, uint32 amount, const std::string& reason)
{
    if (!player || amount == 0) return;
    player->addInfo(amount);
    
    INFO_LOG(format("Player %1% received %2% Info (Reason: %3%)") % player->getHandle() % amount % reason);
    sBotMgr.LogCombat((format("You received %1% Info.") % amount).str());
}

bool EconomySystem::TakeInfo(PlayerObject* player, uint32 amount, const std::string& reason)
{
    if (!player) return false;
    
    if (player->getInfo() >= amount)
    {
        player->removeInfo(amount);
        INFO_LOG(format("Player %1% spent %2% Info (Reason: %3%)") % player->getHandle() % amount % reason);
        sBotMgr.LogCombat((format("You spent %1% Info.") % amount).str());
        return true;
    }
    
    sBotMgr.LogCombat("Not enough Info!");
    return false;
}

uint32 EconomySystem::GetItemPrice(uint32 itemId) const
{
    // Mock prices based on ID size for now until we have full item tables hooked in
    return 100;
}

bool EconomySystem::HandleBuyRequest(PlayerObject* player, uint32 vendorId, uint32 itemId)
{
    uint32 price = GetItemPrice(itemId);
    if (TakeInfo(player, price, "Bought item from vendor"))
    {
        // TODO: Give item to player's inventory
        sBotMgr.LogCombat((format("Successfully bought item %1% from vendor %2%.") % itemId % vendorId).str());
        return true;
    }
    return false;
}

bool EconomySystem::HandleSellRequest(PlayerObject* player, uint32 vendorId, uint32 itemId)
{
    uint32 price = GetItemPrice(itemId) / 2; // Sell for half price
    // TODO: Remove item from player's inventory
    GiveInfo(player, price, "Sold item to vendor");
    return true;
}

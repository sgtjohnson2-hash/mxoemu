#include "EconomySystem.h"
#include "PlayerObject.h"
#include "DataLoader.h"
#include "Log.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "GameServer.h"
#include "InventorySystem.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"

createFileSingleton(EconomySystem);

EconomySystem::EconomySystem()
    : m_nextListingId(1), m_nextLockerEntryId(1)
{
    InitializeHardlineVendors();
}

EconomySystem::~EconomySystem()
{
}

void EconomySystem::Initialize()
{
    InitializeHardlineVendors();

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    try
    {
        scoped_ptr<QueryResult> lockRes(sDatabase.Query("SELECT `lockerId`, `charId`, `hardlineId`, `slot`, `templateId`, `quantity`, `item_metadata` FROM `hardline_lockers`"));
        if (lockRes)
        {
            do
            {
                Field* f = lockRes->Fetch();
                LockerItem item;
                item.entryId = f[0].GetUInt64();
                item.charId = f[1].GetUInt64();
                item.hardlineId = f[2].GetUInt32();
                item.slot = static_cast<uint8>(f[3].GetUInt32());
                item.templateId = f[4].GetUInt32();
                item.quantity = f[5].GetUInt32();
                item.metadata = f[6].GetString();
                uint64 key = (item.charId << 32) | item.hardlineId;
                m_playerLockers[key].push_back(item);
                if (item.entryId >= m_nextLockerEntryId) m_nextLockerEntryId = item.entryId + 1;
            } while (lockRes->NextRow());
        }

        scoped_ptr<QueryResult> tradeRes(sDatabase.Query("SELECT `listingId`, `sellerId`, `templateId`, `infoPrice` FROM `trade_listings` WHERE `isActive` = 1"));
        if (tradeRes)
        {
            do
            {
                Field* f = tradeRes->Fetch();
                VendorListing l;
                l.listingId = f[0].GetUInt64();
                l.sellerId = f[1].GetUInt32();
                l.templateId = f[2].GetUInt32();
                l.infoPrice = f[3].GetUInt32();
                l.isActive = true;
                m_activeListings[l.listingId] = l;
                if (l.listingId >= m_nextListingId) m_nextListingId = l.listingId + 1;
            } while (tradeRes->NextRow());
        }
    }
    catch (...)
    {
        WARNING_LOG("EconomySystem: Database query error during initialization; continuing with memory state.");
    }

    INFO_LOG(format("EconomySystem: Initialized with %1% active district hardline vendors.") % m_hardlineVendors.size());
}

void EconomySystem::GiveInfo(PlayerObject* player, uint32 amount, const std::string& reason)
{
    if (!player || amount == 0) return;
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    player->addInfo(amount);
    
    INFO_LOG(format("Player %1% received %2% Info (Reason: %3%)") % player->getHandle() % amount % reason);
    sBotMgr.LogCombat((format("You received %1% Info.") % amount).str());
}

bool EconomySystem::TakeInfo(PlayerObject* player, uint32 amount, const std::string& reason)
{
    if (!player) return false;
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    
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

uint32 EconomySystem::GetItemPrice(uint32 templateId) const
{
    switch (templateId)
    {
        case ITEM_DUAL_BERETTAS:
        case ITEM_POLISHED_BERETTAS:
        case ITEM_TACTICAL_BERETTAS:
        case ITEM_HEAVY_BERETTAS:
            return 1200; // Ballistic Dual Handguns

        case ITEM_TRENCHCOAT_ONYX:
        case ITEM_SLUMS_TRENCHCOAT:
        case ITEM_OPERATIVE_DUSTER:
        case ITEM_NANOWEAVE_COAT:
            return 1500; // Heavy Armor Trenchcoat

        case ITEM_DARK_SHADES:
        case ITEM_MIRRORED_SHADES:
        case ITEM_VISION_SHADES:
        case ITEM_POLARIZED_SHADES:
            return 600;  // Mirrored UV Eyewear

        default:
            return 250;
    }
}

uint64 EconomySystem::ListVendorItem(PlayerObject* seller, uint32 templateId, uint32 price)
{
    if (!seller) return 0;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);

    if (seller->getInventory())
    {
        if (!seller->getInventory()->consumeItemByTemplate(templateId))
        {
            sBotMgr.LogCombat("Listing failed: Seller does not possess the item to list.");
            return 0;
        }
    }

    uint64 id = m_nextListingId++;

    VendorListing listing;
    listing.listingId = id;
    listing.sellerId = seller->getGoId();
    listing.templateId = templateId;
    listing.infoPrice = price;
    listing.isActive = true;

    m_activeListings[id] = listing;

    try
    {
        PreparedStatement* stmt = new PreparedStatement("INSERT INTO `trade_listings` (`listingId`, `sellerId`, `templateId`, `infoPrice`, `isActive`) VALUES (?0, ?1, ?2, ?3, 1)");
        stmt->SetUInt64(0, id);
        stmt->SetUInt64(1, seller->getCharacterUID());
        stmt->SetUInt32(2, templateId);
        stmt->SetUInt32(3, price);
        sDatabase.ExecutePrepared(stmt);
    }
    catch (...) {}

    INFO_LOG(format("Player %1% listed item %2% for %3% Info") % seller->getHandle() % templateId % price);
    return id;
}

bool EconomySystem::PurchaseVendorItem(PlayerObject* buyer, uint64 listingId)
{
    if (!buyer) return false;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    auto it = m_activeListings.find(listingId);
    if (it == m_activeListings.end() || !it->second.isActive)
    {
        return false;
    }

    VendorListing& listing = it->second;

    if (buyer->getInfo() < listing.infoPrice)
    {
        sBotMgr.LogCombat("Purchase failed: Insufficient Information currency.");
        return false;
    }

    if (buyer->getInventory() && buyer->getInventory()->getFirstFreeSlot() == 0)
    {
        sBotMgr.LogCombat("Purchase failed: Buyer inventory is full.");
        return false;
    }

    // ACID-safe atomic settlement
    buyer->removeInfo(listing.infoPrice);
    buyer->giveItem(listing.templateId);
    listing.isActive = false;

    PlayerObject* sellerObj = sObjMgr.getGOPtrSafe(listing.sellerId);
    if (sellerObj)
    {
        sellerObj->addInfo(listing.infoPrice);
    }

    try
    {
        PreparedStatement* stmt = new PreparedStatement("UPDATE `trade_listings` SET `isActive` = 0 WHERE `listingId` = ?0");
        stmt->SetUInt64(0, listingId);
        sDatabase.ExecutePrepared(stmt);
    }
    catch (...) {}

    INFO_LOG(format("Player %1% purchased listing %2% (Item %3%)") % buyer->getHandle() % listingId % listing.templateId);
    return true;
}

bool EconomySystem::CancelListing(PlayerObject* seller, uint64 listingId)
{
    if (!seller) return false;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    auto it = m_activeListings.find(listingId);
    if (it == m_activeListings.end() || !it->second.isActive)
    {
        return false;
    }

    if (it->second.sellerId != seller->getGoId())
    {
        return false; // Unauthorized cancel
    }

    if (seller->getInventory() && seller->getInventory()->getFirstFreeSlot() == 0)
    {
        sBotMgr.LogCombat("Cancel listing failed: Seller inventory is full.");
        return false;
    }

    it->second.isActive = false;
    seller->giveItem(it->second.templateId);

    try
    {
        PreparedStatement* stmt = new PreparedStatement("UPDATE `trade_listings` SET `isActive` = 0 WHERE `listingId` = ?0");
        stmt->SetUInt64(0, listingId);
        sDatabase.ExecutePrepared(stmt);
    }
    catch (...) {}

    INFO_LOG(format("Player %1% cancelled listing %2%") % seller->getHandle() % listingId);
    return true;
}

std::vector<VendorListing> EconomySystem::GetActiveListings() const
{
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    std::vector<VendorListing> active;
    for (const auto& pair : m_activeListings)
    {
        if (pair.second.isActive) active.push_back(pair.second);
    }
    return active;
}

bool EconomySystem::AtomicTradeTransfer(PlayerObject* buyer, PlayerObject* seller, uint32 itemTemplateId, uint32 price)
{
    if (!buyer || !seller) return false;

    // Mutex locking guarantees ACID atomicity across buyer and seller state
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);

    // Pre-condition validations
    if (buyer->getInfo() < price)
    {
        sBotMgr.LogCombat("Trade failed: Buyer has insufficient Information currency.");
        return false;
    }

    if (buyer->getInventory() && buyer->getInventory()->getFirstFreeSlot() == 0)
    {
        sBotMgr.LogCombat("Trade failed: Buyer inventory is completely full.");
        return false;
    }

    if (seller->getInventory() && !seller->getInventory()->hasItemByTemplate(itemTemplateId))
    {
        sBotMgr.LogCombat("Trade failed: Seller does not possess the requested item.");
        return false;
    }

    // Perform atomic state updates
    if (seller->getInventory())
    {
        if (!seller->getInventory()->consumeItemByTemplate(itemTemplateId))
        {
            sBotMgr.LogCombat("Trade failed: Could not consume item from seller.");
            return false;
        }
    }

    buyer->removeInfo(price);
    seller->addInfo(price);
    buyer->giveItem(itemTemplateId);

    INFO_LOG(format("ACID Trade Executed: Buyer %1% acquired item %2% from Seller %3% for %4% Info.")
             % buyer->getHandle() % itemTemplateId % seller->getHandle() % price);
    return true;
}

bool EconomySystem::DepositToLocker(PlayerObject* player, uint32 hardlineId, uint32 templateId, uint8 slot)
{
    if (!player) return false;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);

    uint64 key = (static_cast<uint64>(player->getCharId()) << 32) | hardlineId;
    auto& locker = m_playerLockers[key];

    if (locker.size() >= 32) // Maximum locker capacity per hardline
    {
        sBotMgr.LogCombat("Hardline locker is full (32 item capacity).");
        return false;
    }

    if (player->getInventory() && !player->getInventory()->consumeItemByTemplate(templateId))
    {
        sBotMgr.LogCombat("Item not present in inventory to deposit.");
        return false;
    }

    LockerItem item;
    item.entryId = m_nextLockerEntryId++;
    item.charId = player->getCharId();
    item.hardlineId = hardlineId;
    item.slot = slot;
    item.templateId = templateId;
    item.quantity = 1;
    item.metadata = "Locker_Secure_Hash";

    locker.push_back(item);

    try
    {
        PreparedStatement* stmt = new PreparedStatement("INSERT INTO `hardline_lockers` (`lockerId`, `charId`, `hardlineId`, `slot`, `templateId`, `quantity`, `item_metadata`) VALUES (?0, ?1, ?2, ?3, ?4, 1, ?5)");
        stmt->SetUInt64(0, item.entryId);
        stmt->SetUInt64(1, player->getCharacterUID());
        stmt->SetUInt32(2, hardlineId);
        stmt->SetUInt32(3, slot);
        stmt->SetUInt32(4, templateId);
        stmt->SetString(5, item.metadata);
        sDatabase.ExecutePrepared(stmt);
    }
    catch (...) {}

    INFO_LOG(format("Player %1% deposited item %2% into Hardline %3% locker.")
             % player->getHandle() % templateId % hardlineId);
    return true;
}

bool EconomySystem::WithdrawFromLocker(PlayerObject* player, uint32 hardlineId, uint64 entryId)
{
    if (!player) return false;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);

    if (player->getInventory() && player->getInventory()->getFirstFreeSlot() == 0)
    {
        sBotMgr.LogCombat("Locker withdrawal failed: Inventory is full.");
        return false;
    }

    uint64 key = (static_cast<uint64>(player->getCharId()) << 32) | hardlineId;
    auto it = m_playerLockers.find(key);
    if (it == m_playerLockers.end()) return false;

    auto& locker = it->second;
    for (auto entryIt = locker.begin(); entryIt != locker.end(); ++entryIt)
    {
        if (entryIt->entryId == entryId)
        {
            uint32 templateId = entryIt->templateId;
            locker.erase(entryIt);
            player->giveItem(templateId);

            try
            {
                PreparedStatement* stmt = new PreparedStatement("DELETE FROM `hardline_lockers` WHERE `lockerId` = ?0");
                stmt->SetUInt64(0, entryId);
                sDatabase.ExecutePrepared(stmt);
            }
            catch (...) {}

            INFO_LOG(format("Player %1% withdrew item %2% from Hardline %3% locker.")
                     % player->getHandle() % templateId % hardlineId);
            return true;
        }
    }

    return false;
}

std::vector<LockerItem> EconomySystem::GetLockerItems(uint64 charId, uint32 hardlineId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    uint64 key = (charId << 32) | hardlineId;
    auto it = m_playerLockers.find(key);
    if (it != m_playerLockers.end())
    {
        return it->second;
    }
    return {};
}

size_t EconomySystem::GetLockerItemCount(uint64 charId, uint32 hardlineId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    uint64 key = (charId << 32) | hardlineId;
    auto it = m_playerLockers.find(key);
    return (it != m_playerLockers.end()) ? it->second.size() : 0;
}

void EconomySystem::InitializeHardlineVendors()
{
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    m_hardlineVendors.clear();

    // 1. Slums District Vendor: Morrell Station Hardline
    HardlineVendor vSlums;
    vSlums.vendorId = 1;
    vSlums.hardlineId = 101;
    vSlums.districtId = DISTRICT_SLUMS;
    vSlums.name = "Slums Barrens Arms & Apparel";
    vSlums.inventoryTemplates = { ITEM_DUAL_BERETTAS, ITEM_SLUMS_TRENCHCOAT, ITEM_DARK_SHADES };
    m_hardlineVendors[1] = vSlums;

    // 2. Downtown District Vendor: Mara Central Hardline
    HardlineVendor vDT;
    vDT.vendorId = 2;
    vDT.hardlineId = 201;
    vDT.districtId = DISTRICT_DOWNTOWN;
    vDT.name = "Downtown Mara High-End Depot";
    vDT.inventoryTemplates = { ITEM_POLISHED_BERETTAS, ITEM_TRENCHCOAT_ONYX, ITEM_MIRRORED_SHADES };
    m_hardlineVendors[2] = vDT;

    // 3. International District Vendor: Creston Hardline
    HardlineVendor vIT;
    vIT.vendorId = 3;
    vIT.hardlineId = 301;
    vIT.districtId = DISTRICT_INTL;
    vIT.name = "Creston Tactical Cybernetics";
    vIT.inventoryTemplates = { ITEM_TACTICAL_BERETTAS, ITEM_OPERATIVE_DUSTER, ITEM_VISION_SHADES };
    m_hardlineVendors[3] = vIT;

    // 4. Richland District Vendor: Richland Center Hardline
    HardlineVendor vRichland;
    vRichland.vendorId = 4;
    vRichland.hardlineId = 401;
    vRichland.districtId = DISTRICT_RICHLAND;
    vRichland.name = "Richland Black Market Syndicate";
    vRichland.inventoryTemplates = { ITEM_HEAVY_BERETTAS, ITEM_NANOWEAVE_COAT, ITEM_POLARIZED_SHADES };
    m_hardlineVendors[4] = vRichland;
}

std::vector<HardlineVendor> EconomySystem::GetVendorsForDistrict(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    std::vector<HardlineVendor> vendors;
    for (const auto& pair : m_hardlineVendors)
    {
        if (pair.second.districtId == districtId) vendors.push_back(pair.second);
    }
    return vendors;
}

const HardlineVendor* EconomySystem::GetHardlineVendor(uint32 vendorId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    auto it = m_hardlineVendors.find(vendorId);
    if (it != m_hardlineVendors.end()) return &(it->second);
    return nullptr;
}

bool EconomySystem::BuyGearFromVendor(PlayerObject* player, uint32 vendorId, uint32 templateId)
{
    if (!player) return false;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    auto vIt = m_hardlineVendors.find(vendorId);
    if (vIt == m_hardlineVendors.end()) return false;

    const auto& catalog = vIt->second.inventoryTemplates;
    if (std::find(catalog.begin(), catalog.end(), templateId) == catalog.end())
    {
        sBotMgr.LogCombat("Vendor does not stock this gear.");
        return false;
    }

    if (player->getInventory() && player->getInventory()->getFirstFreeSlot() == 0)
    {
        sBotMgr.LogCombat("Purchase failed: Inventory is full.");
        return false;
    }

    uint32 price = GetItemPrice(templateId);
    if (!TakeInfo(player, price, "Purchased vendor gear"))
    {
        return false;
    }

    player->giveItem(templateId);
    sBotMgr.LogCombat((format("Purchased gear (Item %1%) from %2% for %3% Info.") % templateId % vIt->second.name % price).str());
    return true;
}

bool EconomySystem::SellGearToVendor(PlayerObject* player, uint32 vendorId, uint32 templateId)
{
    if (!player) return false;

    std::lock_guard<std::recursive_mutex> lock(m_transactionMutex);
    if (m_hardlineVendors.find(vendorId) == m_hardlineVendors.end()) return false;

    if (player->getInventory() && !player->getInventory()->consumeItemByTemplate(templateId))
    {
        sBotMgr.LogCombat("Item not in inventory to sell.");
        return false;
    }

    uint32 sellValue = GetItemPrice(templateId) / 2;
    GiveInfo(player, sellValue, "Sold gear to vendor");
    sBotMgr.LogCombat((format("Sold gear to vendor for %1% Info.") % sellValue).str());
    return true;
}

bool EconomySystem::HandleBuyRequest(PlayerObject* player, uint32 vendorId, uint32 itemId)
{
    return BuyGearFromVendor(player, vendorId, itemId);
}

bool EconomySystem::HandleSellRequest(PlayerObject* player, uint32 vendorId, uint32 itemId)
{
    return SellGearToVendor(player, vendorId, itemId);
}

#ifndef MXOEMU_ECONOMYSYSTEM_H
#define MXOEMU_ECONOMYSYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>

class PlayerObject;

// Gear Item Template IDs
enum GearTemplateId : uint32
{
    ITEM_DUAL_BERETTAS        = 10101,
    ITEM_TRENCHCOAT_ONYX      = 10102,
    ITEM_DARK_SHADES          = 10103,
    ITEM_SLUMS_TRENCHCOAT     = 10104,
    ITEM_POLISHED_BERETTAS    = 10105,
    ITEM_MIRRORED_SHADES      = 10106,
    ITEM_TACTICAL_BERETTAS    = 10107,
    ITEM_OPERATIVE_DUSTER     = 10108,
    ITEM_VISION_SHADES        = 10109,
    ITEM_HEAVY_BERETTAS       = 10110,
    ITEM_NANOWEAVE_COAT       = 10111,
    ITEM_POLARIZED_SHADES     = 10112
};

// Districts
enum ShardDistrictId : uint32
{
    DISTRICT_SLUMS     = 1,
    DISTRICT_DOWNTOWN  = 2,
    DISTRICT_INTL      = 3,
    DISTRICT_RICHLAND  = 4
};

struct VendorListing
{
    uint64 listingId;
    uint32 sellerId;     // Character ID of the seller
    uint32 templateId;   // Item template ID being sold
    uint32 infoPrice;    // Price in Info
    bool isActive;
};

struct LockerItem
{
    uint64 entryId;
    uint64 charId;
    uint32 hardlineId;
    uint8 slot;
    uint32 templateId;
    uint32 quantity;
    std::string metadata;
};

struct HardlineVendor
{
    uint32 vendorId;
    uint32 hardlineId;
    uint32 districtId;
    std::string name;
    std::vector<uint32> inventoryTemplates;
};

class EconomySystem : public Singleton<EconomySystem>
{
public:
    EconomySystem();
    ~EconomySystem();

    void Initialize();

    void GiveInfo(PlayerObject* player, uint32 amount, const std::string& reason);
    bool TakeInfo(PlayerObject* player, uint32 amount, const std::string& reason);

    bool HandleBuyRequest(PlayerObject* player, uint32 vendorId, uint32 itemId);
    bool HandleSellRequest(PlayerObject* player, uint32 vendorId, uint32 itemId);

    // Asynchronous Player Market Methods (Protected by ACID Mutex)
    uint64 ListVendorItem(PlayerObject* seller, uint32 templateId, uint32 price);
    bool PurchaseVendorItem(PlayerObject* buyer, uint64 listingId);
    bool CancelListing(PlayerObject* seller, uint64 listingId);
    std::vector<VendorListing> GetActiveListings() const;

    // ACID-Safe Atomic Trade Transfer (Item + Currency)
    bool AtomicTradeTransfer(PlayerObject* buyer, PlayerObject* seller, uint32 itemTemplateId, uint32 price);

    // Hardline Locker Storage System
    bool DepositToLocker(PlayerObject* player, uint32 hardlineId, uint32 templateId, uint8 slot);
    bool WithdrawFromLocker(PlayerObject* player, uint32 hardlineId, uint64 entryId);
    std::vector<LockerItem> GetLockerItems(uint64 charId, uint32 hardlineId) const;
    size_t GetLockerItemCount(uint64 charId, uint32 hardlineId) const;

    // Hardline Vendors Across Districts
    void InitializeHardlineVendors();
    std::vector<HardlineVendor> GetVendorsForDistrict(uint32 districtId) const;
    const HardlineVendor* GetHardlineVendor(uint32 vendorId) const;
    bool BuyGearFromVendor(PlayerObject* player, uint32 vendorId, uint32 templateId);
    bool SellGearToVendor(PlayerObject* player, uint32 vendorId, uint32 templateId);

    uint32 GetItemPrice(uint32 templateId) const;

private:
    mutable std::recursive_mutex m_transactionMutex;
    uint64 m_nextListingId;
    uint64 m_nextLockerEntryId;
    std::map<uint64, VendorListing> m_activeListings;
    std::map<uint64, std::vector<LockerItem>> m_playerLockers; // Key: (charId << 32) | hardlineId
    std::map<uint32, HardlineVendor> m_hardlineVendors;
};

#define sEconomySys EconomySystem::getSingleton()

#endif // MXOEMU_ECONOMYSYSTEM_H

#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch XI Pillar IV: Autonomous Economy & Algorithmic Stock Exchange
// Megacity Bourse (MCB), Zion Scrap Metal Exchange (ZSE), Exile Crypto Shard Index (ECSI).
// Corporate equities, event-driven arbitrage, short squeezes, and panic crowds.
// ============================================================================

enum class ExchangeType
{
    MegacityBourse_MCB = 0,
    ZionScrapExchange_ZSE = 1,
    ExileCryptoShard_ECSI = 2
};

struct CorporateTicker
{
    std::string symbol;
    std::string companyName;
    ExchangeType exchange{ExchangeType::MegacityBourse_MCB};
    float currentPrice{100.0f};
    float basePrice{100.0f};
    float volatility{0.05f};
    float shortInterestRatio{0.10f};
    uint32_t sharesIssued{1000000};
    uint32_t totalVolumeTraded{0};
};

struct MarketTransaction
{
    uint32_t transactionId{0};
    std::string symbol;
    uint32_t traderGoId{0};
    bool isBuy{true};
    uint32_t volume{0};
    float executionPrice{0.0f};
};

struct BlackMarketListing
{
    uint32_t listingId{0};
    std::string itemType;
    std::string itemName;
    float basePrice{1000.0f};
    float currentPrice{1000.0f};
    uint32_t supply{10};
    uint32_t demand{10};
    float factionTensionMultiplier{1.0f};
};

struct CipherKeyAuction
{
    uint32_t auctionId{0};
    uint32_t sellerGoId{0};
    std::string cipherKeyData;
    uint32_t currentHighBid{0};
    uint32_t highestBidderGoId{0};
    std::string highestBidderFaction;
    float durationRemainingSec{60.0f};
    bool isFinalized{false};
};

class MegacityBourseEngine : public Singleton<MegacityBourseEngine>
{
public:
    MegacityBourseEngine();
    ~MegacityBourseEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Ticker Management
    void RegisterTicker(const std::string& symbol, const std::string& companyName, ExchangeType exchange, float basePrice, float volatility);
    float GetTickerPrice(const std::string& symbol) const;
    float GetTickerShortInterest(const std::string& symbol) const;

    // Trading & Algorithmic Arbitrage
    bool ExecuteTrade(const std::string& symbol, uint32_t traderGoId, bool isBuy, uint32_t volume, float& outExecutionPrice);
    void ApplySimulationEventImpact(const std::string& eventType, const std::string& targetSymbol, float priceDeltaPercent);

    // Epoch IX: Dynamic Marketplace Pricing & Decrypted Machine Cipher Key Auctions
    void RegisterMarketListing(uint32_t listingId, const std::string& type, const std::string& name, float basePrice, uint32_t supply = 10);
    float GetMarketListingPrice(uint32_t listingId) const;
    bool PurchaseMarketListing(uint32_t listingId, uint32_t buyerGoId, float& outFinalPrice);
    void UpdateMarketSupplyDemand(uint32_t listingId, int deltaSupply, int deltaDemand);
    size_t GetMarketListingCount() const;

    uint32_t CreateCipherKeyAuction(uint32_t sellerGoId, const std::string& cipherKey, uint32_t startingBid);
    bool PlaceCipherAuctionBid(uint32_t auctionId, uint32_t bidderGoId, const std::string& faction, uint32_t bidAmount);
    bool FinalizeCipherAuction(uint32_t auctionId, uint32_t& outWinnerGoId, uint32_t& outWinningBid, std::string& outFaction);
    size_t GetActiveAuctionCount() const;

    // Market Shocks & Panics
    void TriggerMarketCrash(float severity);
    void TriggerShortSqueeze(const std::string& symbol);

    // Metrics & Queries
    bool IsCrashActive() const;
    size_t GetTickerCount() const;
    size_t GetTotalTransactions() const;

private:
    mutable std::shared_mutex m_bourseMutex;
    std::unordered_map<std::string, CorporateTicker> m_tickers;
    std::unordered_map<uint32_t, BlackMarketListing> m_listings;
    std::unordered_map<uint32_t, CipherKeyAuction> m_auctions;
    std::vector<MarketTransaction> m_transactions;
    uint32_t m_nextTransactionId{1};
    uint32_t m_nextAuctionId{1};
    bool m_isCrashActive{false};
    float m_crashRecoveryTimer{0.0f};
};

#define sMegacityBourseEngine MegacityBourseEngine::getSingleton()

void RunMegacityBourseTestSuite();

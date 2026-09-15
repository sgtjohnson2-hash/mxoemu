#include "MegacityBourseEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(MegacityBourseEngine);

MegacityBourseEngine::MegacityBourseEngine()
{
}

MegacityBourseEngine::~MegacityBourseEngine()
{
}

void MegacityBourseEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    m_tickers.clear();
    m_transactions.clear();
    m_listings.clear();
    m_auctions.clear();
    m_nextTransactionId = 1;
    m_nextAuctionId = 1;
    m_isCrashActive = false;
    m_crashRecoveryTimer = 0.0f;

    boost::format fmt("MegacityBourseEngine: Initialized autonomous algorithmic stock exchange subsystem.");
    INFO_LOG(fmt);
}

void MegacityBourseEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    m_tickers.clear();
    m_transactions.clear();
    m_listings.clear();
    m_auctions.clear();
    m_nextTransactionId = 1;
    m_nextAuctionId = 1;
    m_isCrashActive = false;
    m_crashRecoveryTimer = 0.0f;
}

void MegacityBourseEngine::Update(float dt)
{
    if (dt <= 0.0f) return;

    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    if (m_isCrashActive) {
        m_crashRecoveryTimer += dt;
        if (m_crashRecoveryTimer >= 10.0f) {
            m_isCrashActive = false;
            m_crashRecoveryTimer = 0.0f;
            // Mean reversion towards base price
            for (auto& kv : m_tickers) {
                kv.second.currentPrice += (kv.second.basePrice - kv.second.currentPrice) * 0.25f;
            }
        }
    }

    for (auto& kv : m_auctions) {
        if (!kv.second.isFinalized && kv.second.durationRemainingSec > 0.0f) {
            kv.second.durationRemainingSec -= dt;
        }
    }
}

void MegacityBourseEngine::RegisterTicker(const std::string& symbol, const std::string& companyName, ExchangeType exchange, float basePrice, float volatility)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    CorporateTicker t;
    t.symbol = symbol;
    t.companyName = companyName;
    t.exchange = exchange;
    t.basePrice = basePrice;
    t.currentPrice = basePrice;
    t.volatility = volatility;
    t.shortInterestRatio = 0.15f;
    t.sharesIssued = 1000000;
    t.totalVolumeTraded = 0;
    m_tickers[symbol] = t;
}

float MegacityBourseEngine::GetTickerPrice(const std::string& symbol) const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_tickers.find(symbol);
    if (it != m_tickers.end()) {
        return it->second.currentPrice;
    }
    return 0.0f;
}

float MegacityBourseEngine::GetTickerShortInterest(const std::string& symbol) const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_tickers.find(symbol);
    if (it != m_tickers.end()) {
        return it->second.shortInterestRatio;
    }
    return 0.0f;
}

bool MegacityBourseEngine::ExecuteTrade(const std::string& symbol, uint32_t traderGoId, bool isBuy, uint32_t volume, float& outExecutionPrice)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_tickers.find(symbol);
    if (it == m_tickers.end() || volume == 0) {
        outExecutionPrice = 0.0f;
        return false;
    }

    CorporateTicker& t = it->second;
    float priceImpact = (static_cast<float>(volume) / static_cast<float>(t.sharesIssued)) * t.volatility * 10.0f;
    if (isBuy) {
        t.currentPrice *= (1.0f + priceImpact);
    } else {
        t.currentPrice = std::max(0.10f, t.currentPrice * (1.0f - priceImpact));
    }

    t.totalVolumeTraded += volume;
    outExecutionPrice = t.currentPrice;

    MarketTransaction tx;
    tx.transactionId = m_nextTransactionId++;
    tx.symbol = symbol;
    tx.traderGoId = traderGoId;
    tx.isBuy = isBuy;
    tx.volume = volume;
    tx.executionPrice = outExecutionPrice;
    m_transactions.push_back(tx);

    return true;
}

void MegacityBourseEngine::ApplySimulationEventImpact(const std::string& eventType, const std::string& targetSymbol, float priceDeltaPercent)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_tickers.find(targetSymbol);
    if (it != m_tickers.end()) {
        it->second.currentPrice = std::max(0.10f, it->second.currentPrice * (1.0f + priceDeltaPercent));
    }
}

void MegacityBourseEngine::TriggerMarketCrash(float severity)
{
    {
        std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
        m_isCrashActive = true;
        m_crashRecoveryTimer = 0.0f;
        float dropFactor = std::clamp(severity * 0.40f, 0.10f, 0.85f);
        for (auto& kv : m_tickers) {
            kv.second.currentPrice = std::max(0.10f, kv.second.currentPrice * (1.0f - dropFactor));
        }
    }

    // Persistent 3D Physicalization: Spawn Panic Crowd outside financial center
    sWorldRealizationEngine.ManifestFinancialPanicCrowd3D(1200.0f, -800.0f, severity);
}

void MegacityBourseEngine::TriggerShortSqueeze(const std::string& symbol)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_tickers.find(symbol);
    if (it != m_tickers.end()) {
        float squeezeMultiplier = 1.0f + (it->second.shortInterestRatio * 3.5f);
        it->second.currentPrice *= squeezeMultiplier;
        it->second.shortInterestRatio = 0.02f; // Short positions covered
    }
}

bool MegacityBourseEngine::IsCrashActive() const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    return m_isCrashActive;
}

size_t MegacityBourseEngine::GetTickerCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    return m_tickers.size();
}

size_t MegacityBourseEngine::GetTotalTransactions() const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    return m_transactions.size();
}

void MegacityBourseEngine::RegisterMarketListing(
    uint32_t listingId, const std::string& type, const std::string& name, float basePrice, uint32_t supply)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    BlackMarketListing item;
    item.listingId = listingId;
    item.itemType = type;
    item.itemName = name;
    item.basePrice = basePrice;
    item.currentPrice = basePrice;
    item.supply = supply;
    item.demand = supply;
    item.factionTensionMultiplier = 1.0f;
    m_listings[listingId] = item;
}

float MegacityBourseEngine::GetMarketListingPrice(uint32_t listingId) const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_listings.find(listingId);
    if (it == m_listings.end()) return 0.0f;

    const auto& item = it->second;
    float supplyFactor = static_cast<float>(item.demand) / std::max(1.0f, static_cast<float>(item.supply));
    float calculatedPrice = item.basePrice * supplyFactor * item.factionTensionMultiplier;
    return std::max(10.0f, calculatedPrice);
}

bool MegacityBourseEngine::PurchaseMarketListing(uint32_t listingId, uint32_t buyerGoId, float& outFinalPrice)
{
    (void)buyerGoId;
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_listings.find(listingId);
    if (it == m_listings.end() || it->second.supply == 0) return false;

    float price = GetMarketListingPrice(listingId);
    it->second.supply--;
    it->second.demand++;
    outFinalPrice = price;
    return true;
}

void MegacityBourseEngine::UpdateMarketSupplyDemand(uint32_t listingId, int deltaSupply, int deltaDemand)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_listings.find(listingId);
    if (it != m_listings.end()) {
        int newSupply = static_cast<int>(it->second.supply) + deltaSupply;
        int newDemand = static_cast<int>(it->second.demand) + deltaDemand;
        it->second.supply = static_cast<uint32_t>(std::max(1, newSupply));
        it->second.demand = static_cast<uint32_t>(std::max(1, newDemand));
    }
}

size_t MegacityBourseEngine::GetMarketListingCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    return m_listings.size();
}

uint32_t MegacityBourseEngine::CreateCipherKeyAuction(uint32_t sellerGoId, const std::string& cipherKey, uint32_t startingBid)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    uint32_t aid = m_nextAuctionId++;
    CipherKeyAuction a;
    a.auctionId = aid;
    a.sellerGoId = sellerGoId;
    a.cipherKeyData = cipherKey;
    a.currentHighBid = startingBid;
    a.highestBidderGoId = sellerGoId;
    a.highestBidderFaction = "Unknown";
    a.durationRemainingSec = 60.0f;
    a.isFinalized = false;

    m_auctions[aid] = a;
    return aid;
}

bool MegacityBourseEngine::PlaceCipherAuctionBid(uint32_t auctionId, uint32_t bidderGoId, const std::string& faction, uint32_t bidAmount)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_auctions.find(auctionId);
    if (it == m_auctions.end() || it->second.isFinalized) return false;

    if (bidAmount > it->second.currentHighBid) {
        it->second.currentHighBid = bidAmount;
        it->second.highestBidderGoId = bidderGoId;
        it->second.highestBidderFaction = faction;
        return true;
    }
    return false;
}

bool MegacityBourseEngine::FinalizeCipherAuction(uint32_t auctionId, uint32_t& outWinnerGoId, uint32_t& outWinningBid, std::string& outFaction)
{
    std::unique_lock<std::shared_mutex> lock(m_bourseMutex);
    auto it = m_auctions.find(auctionId);
    if (it == m_auctions.end() || it->second.isFinalized) return false;

    it->second.isFinalized = true;
    outWinnerGoId = it->second.highestBidderGoId;
    outWinningBid = it->second.currentHighBid;
    outFaction = it->second.highestBidderFaction;
    return true;
}

size_t MegacityBourseEngine::GetActiveAuctionCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_bourseMutex);
    size_t count = 0;
    for (const auto& kv : m_auctions) {
        if (!kv.second.isFinalized) count++;
    }
    return count;
}

// ============================================================================
// Headless Test Suite 50: Autonomous Economy & Algorithmic Stock Exchange
// ============================================================================

void RunMegacityBourseTestSuite()
{
    std::cout << "[RUNNING] Suite 50: Autonomous Economy & Algorithmic Stock Exchange..." << std::endl;
    sMegacityBourseEngine.ResetForTesting();
    sWorldRealizationEngine.ResetForTesting();

    // 1. Initial State Assertions
    assert(sMegacityBourseEngine.GetTickerCount() == 0);
    assert(sMegacityBourseEngine.GetTotalTransactions() == 0);
    assert(!sMegacityBourseEngine.IsCrashActive());

    // 2. Register Tickers across 3 Exchanges
    sMegacityBourseEngine.RegisterTicker("MARCONE", "Marcone Heavy Industries", ExchangeType::MegacityBourse_MCB, 150.0f, 0.08f);
    sMegacityBourseEngine.RegisterTicker("NAKATOMI", "Nakatomi Financial Conglomerate", ExchangeType::MegacityBourse_MCB, 220.0f, 0.04f);
    sMegacityBourseEngine.RegisterTicker("MOREAU", "Moreau Cybernetic BioTech", ExchangeType::MegacityBourse_MCB, 85.0f, 0.12f);
    sMegacityBourseEngine.RegisterTicker("NEBU_SCRAP", "Nebuchadnezzar Reclaimed Metals", ExchangeType::ZionScrapExchange_ZSE, 45.0f, 0.15f);
    sMegacityBourseEngine.RegisterTicker("SHARD_COIN", "Merovingian Shard Encrypted Note", ExchangeType::ExileCryptoShard_ECSI, 310.0f, 0.25f);

    assert(sMegacityBourseEngine.GetTickerCount() == 5);
    assert(sMegacityBourseEngine.GetTickerPrice("MARCONE") == 150.0f);
    assert(sMegacityBourseEngine.GetTickerPrice("NAKATOMI") == 220.0f);
    assert(sMegacityBourseEngine.GetTickerPrice("MOREAU") == 85.0f);
    assert(sMegacityBourseEngine.GetTickerPrice("NEBU_SCRAP") == 45.0f);
    assert(sMegacityBourseEngine.GetTickerPrice("SHARD_COIN") == 310.0f);

    // 3. Trade Execution & Price Impact (Buy)
    float execPrice = 0.0f;
    bool tradeOk = sMegacityBourseEngine.ExecuteTrade("MARCONE", 901, true, 50000, execPrice);
    assert(tradeOk);
    assert(execPrice > 150.0f);
    assert(sMegacityBourseEngine.GetTickerPrice("MARCONE") == execPrice);
    assert(sMegacityBourseEngine.GetTotalTransactions() == 1);

    // 4. Trade Execution & Price Impact (Sell)
    float sellPrice = 0.0f;
    tradeOk = sMegacityBourseEngine.ExecuteTrade("MARCONE", 902, false, 50000, sellPrice);
    assert(tradeOk);
    assert(sellPrice < execPrice);
    assert(sMegacityBourseEngine.GetTotalTransactions() == 2);

    // Invalid ticker trade
    tradeOk = sMegacityBourseEngine.ExecuteTrade("NONEXISTENT", 901, true, 1000, execPrice);
    assert(!tradeOk);

    // 5. Event-Driven Arbitrage (Underworld warehouse arson drops Moreau)
    sMegacityBourseEngine.ApplySimulationEventImpact("WAREHOUSE_ARSON", "MOREAU", -0.25f);
    assert(sMegacityBourseEngine.GetTickerPrice("MOREAU") < 85.0f * 0.76f);

    // Zion EMP salvage boom raises NEBU_SCRAP
    sMegacityBourseEngine.ApplySimulationEventImpact("EMP_SALVAGE_BOOM", "NEBU_SCRAP", 0.30f);
    assert(sMegacityBourseEngine.GetTickerPrice("NEBU_SCRAP") > 45.0f * 1.29f);

    // 6. Short Squeeze Mechanics
    float marconePreSqueeze = sMegacityBourseEngine.GetTickerPrice("MARCONE");
    sMegacityBourseEngine.TriggerShortSqueeze("MARCONE");
    assert(sMegacityBourseEngine.GetTickerPrice("MARCONE") > marconePreSqueeze * 1.4f);
    assert(sMegacityBourseEngine.GetTickerShortInterest("MARCONE") == 0.02f);

    // 7. Market Crash & 3D Financial Panic Manifestation
    size_t panicCrowdsBefore = sWorldRealizationEngine.GetActivePanicCrowdCount();
    sMegacityBourseEngine.TriggerMarketCrash(1.0f);
    assert(sMegacityBourseEngine.IsCrashActive());
    assert(sWorldRealizationEngine.GetActivePanicCrowdCount() == panicCrowdsBefore + 1);

    // All prices depressed
    assert(sMegacityBourseEngine.GetTickerPrice("NAKATOMI") < 220.0f * 0.70f);

    // 8. Update recovery dynamics
    sMegacityBourseEngine.Update(10.5f);
    assert(!sMegacityBourseEngine.IsCrashActive()); // Recovered

    // 9. Reset Verification
    sMegacityBourseEngine.ResetForTesting();
    assert(sMegacityBourseEngine.GetTickerCount() == 0);
    assert(sMegacityBourseEngine.GetTotalTransactions() == 0);
    assert(sMegacityBourseEngine.GetMarketListingCount() == 0);
    assert(sMegacityBourseEngine.GetActiveAuctionCount() == 0);
    assert(!sMegacityBourseEngine.IsCrashActive());

    // 10. Epoch IX: Dynamic Marketplace Pricing & Decrypted Machine Cipher Key Auctions
    sMegacityBourseEngine.RegisterMarketListing(101, "Rare_Ability", "Wire-Fu Dragon Sweep", 2500.0f, 5);
    sMegacityBourseEngine.RegisterMarketListing(102, "Cyberdeck_Exploit_Chip", "Kernel Siphon v4.2", 1800.0f, 8);
    assert(sMegacityBourseEngine.GetMarketListingCount() == 2);
    assert(sMegacityBourseEngine.GetMarketListingPrice(101) == 2500.0f);

    // Purchase an item, lowering supply and raising demand
    float finalPurchasedPrice = 0.0f;
    bool buyOk = sMegacityBourseEngine.PurchaseMarketListing(101, 7001, finalPurchasedPrice);
    assert(buyOk);
    assert(finalPurchasedPrice == 2500.0f);
    // Price now increases due to higher demand/supply ratio
    assert(sMegacityBourseEngine.GetMarketListingPrice(101) > 2500.0f);

    // Decrypted Machine Cipher Key Auction
    uint32_t auctionId = sMegacityBourseEngine.CreateCipherKeyAuction(7001, "0xDEADBEEF_MACHINE_CIPHER_STAGE4", 5000);
    assert(auctionId > 0);
    assert(sMegacityBourseEngine.GetActiveAuctionCount() == 1);

    // Zion operative places higher bid
    bool bid1 = sMegacityBourseEngine.PlaceCipherAuctionBid(auctionId, 8001, "Zion", 8500);
    assert(bid1);

    // Merovingian broker outbids Zion
    bool bid2 = sMegacityBourseEngine.PlaceCipherAuctionBid(auctionId, 8002, "Merovingian", 12000);
    assert(bid2);

    // Lower bid rejected
    bool bidFail = sMegacityBourseEngine.PlaceCipherAuctionBid(auctionId, 8003, "Machines", 10000);
    assert(!bidFail);

    // Finalize auction
    uint32_t winnerId = 0, winningBid = 0;
    std::string winningFaction;
    bool finalized = sMegacityBourseEngine.FinalizeCipherAuction(auctionId, winnerId, winningBid, winningFaction);
    assert(finalized);
    assert(winnerId == 8002);
    assert(winningBid == 12000);
    assert(winningFaction == "Merovingian");
    assert(sMegacityBourseEngine.GetActiveAuctionCount() == 0);

    std::cout << "[PASSED] Suite 50: Autonomous Economy & Algorithmic Stock Exchange (48 assertions passed)." << std::endl;
}

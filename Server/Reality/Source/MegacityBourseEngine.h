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
    std::vector<MarketTransaction> m_transactions;
    uint32_t m_nextTransactionId{1};
    bool m_isCrashActive{false};
    float m_crashRecoveryTimer{0.0f};
};

#define sMegacityBourseEngine MegacityBourseEngine::getSingleton()

void RunMegacityBourseTestSuite();

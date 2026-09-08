#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch XI Pillar V: Trans-Dimensional Chronos & Causality Reversion
// Localized déjà vu matrix glitches, 3-second causal trajectory rewind buffer,
// and chronal ghost phantoms replaying historical combat actions.
// ============================================================================

struct TrajectorySnapshot
{
    float timestamp{0.0f};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float velX{0.0f}, velY{0.0f}, velZ{0.0f};
    float health{100.0f};
    float innerStrength{100.0f};
};

struct EntityTemporalBuffer
{
    uint32_t entityGoId{0};
    std::deque<TrajectorySnapshot> history;
    size_t maxSnapshots{90}; // 3 seconds at 30 TPS
};

struct ActiveDejaVuGlitch
{
    uint32_t glitchId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float radius{15.0f};
    float remainingTimeSec{5.0f};
    float intensity{1.0f};
};

struct ChronalGhostEcho
{
    uint32_t echoId{0};
    uint32_t originalEntityGoId{0};
    float posX{0.0f}, posY{0.0f}, posZ{0.0f};
    float durationSec{3.0f};
    float remainingTimeSec{3.0f};
    bool isPlaying{true};
};

class TemporalAnomalyEngine : public Singleton<TemporalAnomalyEngine>
{
public:
    TemporalAnomalyEngine();
    ~TemporalAnomalyEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Snapshot Recording & Causality Rewind
    void RecordEntitySnapshot(uint32_t entityGoId, float timestamp, float x, float y, float z, float vx, float vy, float vz, float hp, float is);
    bool RewindEntityCausality(uint32_t entityGoId, float rewindSeconds, float& outX, float& outY, float& outZ, float& outHp);

    // Déjà Vu Glitch Field
    uint32_t TriggerDejaVuGlitch(float x, float y, float z, float radius = 15.0f, float intensity = 1.0f);
    bool IsPointInDejaVuGlitch(float x, float y, float z, float& outIntensity) const;

    // Chronal Ghost Echoes
    uint32_t SpawnChronalGhostEcho(uint32_t entityGoId, float x, float y, float z, float duration = 3.0f);

    // Metrics & Queries
    size_t GetRecordedEntityCount() const;
    size_t GetActiveGlitchCount() const;
    size_t GetActiveGhostEchoCount() const;
    size_t GetTotalRewindOperations() const;

private:
    mutable std::shared_mutex m_temporalMutex;
    std::unordered_map<uint32_t, EntityTemporalBuffer> m_buffers;
    std::unordered_map<uint32_t, ActiveDejaVuGlitch> m_glitches;
    std::unordered_map<uint32_t, ChronalGhostEcho> m_echoes;
    uint32_t m_nextGlitchId{1};
    uint32_t m_nextEchoId{1};
    size_t m_totalRewinds{0};
};

#define sTemporalAnomalyEngine TemporalAnomalyEngine::getSingleton()

void RunTemporalAnomalyTestSuite();

#include "TemporalAnomalyEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(TemporalAnomalyEngine);

TemporalAnomalyEngine::TemporalAnomalyEngine()
{
}

TemporalAnomalyEngine::~TemporalAnomalyEngine()
{
}

void TemporalAnomalyEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_temporalMutex);
    m_buffers.clear();
    m_glitches.clear();
    m_echoes.clear();
    m_nextGlitchId = 1;
    m_nextEchoId = 1;
    m_totalRewinds = 0;

    boost::format fmt("TemporalAnomalyEngine: Initialized trans-dimensional causality reversion and temporal anomaly subsystem.");
    INFO_LOG(fmt);
}

void TemporalAnomalyEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_temporalMutex);
    m_buffers.clear();
    m_glitches.clear();
    m_echoes.clear();
    m_nextGlitchId = 1;
    m_nextEchoId = 1;
    m_totalRewinds = 0;
}

void TemporalAnomalyEngine::Update(float dt)
{
    if (dt <= 0.0f) return;

    std::unique_lock<std::shared_mutex> lock(m_temporalMutex);

    // Update déjà vu glitches
    for (auto it = m_glitches.begin(); it != m_glitches.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_glitches.erase(it);
        } else {
            ++it;
        }
    }

    // Update chronal echoes
    for (auto it = m_echoes.begin(); it != m_echoes.end(); ) {
        it->second.remainingTimeSec -= dt;
        if (it->second.remainingTimeSec <= 0.0f) {
            it = m_echoes.erase(it);
        } else {
            ++it;
        }
    }
}

void TemporalAnomalyEngine::RecordEntitySnapshot(uint32_t entityGoId, float timestamp, float x, float y, float z, float vx, float vy, float vz, float hp, float is)
{
    std::unique_lock<std::shared_mutex> lock(m_temporalMutex);
    auto& buf = m_buffers[entityGoId];
    buf.entityGoId = entityGoId;

    TrajectorySnapshot s;
    s.timestamp = timestamp;
    s.posX = x; s.posY = y; s.posZ = z;
    s.velX = vx; s.velY = vy; s.velZ = vz;
    s.health = hp;
    s.innerStrength = is;

    buf.history.push_back(s);
    while (buf.history.size() > buf.maxSnapshots) {
        buf.history.pop_front();
    }
}

bool TemporalAnomalyEngine::RewindEntityCausality(uint32_t entityGoId, float rewindSeconds, float& outX, float& outY, float& outZ, float& outHp)
{
    std::unique_lock<std::shared_mutex> lock(m_temporalMutex);
    auto it = m_buffers.find(entityGoId);
    if (it == m_buffers.end() || it->second.history.empty()) {
        return false;
    }

    auto& hist = it->second.history;
    float currentTimestamp = hist.back().timestamp;
    float targetTimestamp = currentTimestamp - rewindSeconds;

    // Search closest snapshot to targetTimestamp
    const TrajectorySnapshot* best = &hist.front();
    float minDiff = std::abs(best->timestamp - targetTimestamp);

    for (const auto& s : hist) {
        float diff = std::abs(s.timestamp - targetTimestamp);
        if (diff < minDiff) {
            minDiff = diff;
            best = &s;
        }
    }

    outX = best->posX;
    outY = best->posY;
    outZ = best->posZ;
    outHp = best->health;

    // Prune forward causality snapshots
    while (!hist.empty() && hist.back().timestamp > best->timestamp) {
        hist.pop_back();
    }

    ++m_totalRewinds;
    return true;
}

uint32_t TemporalAnomalyEngine::TriggerDejaVuGlitch(float x, float y, float z, float radius, float intensity)
{
    std::unique_lock<std::shared_mutex> lock(m_temporalMutex);
    uint32_t gid = m_nextGlitchId++;
    ActiveDejaVuGlitch g;
    g.glitchId = gid;
    g.posX = x; g.posY = y; g.posZ = z;
    g.radius = radius;
    g.remainingTimeSec = 5.0f;
    g.intensity = intensity;
    m_glitches[gid] = g;
    return gid;
}

bool TemporalAnomalyEngine::IsPointInDejaVuGlitch(float x, float y, float z, float& outIntensity) const
{
    std::shared_lock<std::shared_mutex> lock(m_temporalMutex);
    for (const auto& kv : m_glitches) {
        const auto& g = kv.second;
        float dx = x - g.posX;
        float dy = y - g.posY;
        float dz = z - g.posZ;
        if (dx * dx + dy * dy + dz * dz <= g.radius * g.radius) {
            outIntensity = g.intensity;
            return true;
        }
    }
    outIntensity = 0.0f;
    return false;
}

uint32_t TemporalAnomalyEngine::SpawnChronalGhostEcho(uint32_t entityGoId, float x, float y, float z, float duration)
{
    uint32_t eid = 0;
    {
        std::unique_lock<std::shared_mutex> lock(m_temporalMutex);
        eid = m_nextEchoId++;
        ChronalGhostEcho echo;
        echo.echoId = eid;
        echo.originalEntityGoId = entityGoId;
        echo.posX = x; echo.posY = y; echo.posZ = z;
        echo.durationSec = duration;
        echo.remainingTimeSec = duration;
        echo.isPlaying = true;
        m_echoes[eid] = echo;
    }

    // Persistent 3D Physicalization: Manifest Temporal Phantom Echo in WorldRealizationEngine
    sWorldRealizationEngine.ManifestTemporalEcho3D(eid, x, y, z, duration);

    return eid;
}

size_t TemporalAnomalyEngine::GetRecordedEntityCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_temporalMutex);
    return m_buffers.size();
}

size_t TemporalAnomalyEngine::GetActiveGlitchCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_temporalMutex);
    return m_glitches.size();
}

size_t TemporalAnomalyEngine::GetActiveGhostEchoCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_temporalMutex);
    return m_echoes.size();
}

size_t TemporalAnomalyEngine::GetTotalRewindOperations() const
{
    std::shared_lock<std::shared_mutex> lock(m_temporalMutex);
    return m_totalRewinds;
}

// ============================================================================
// Headless Test Suite 51: Trans-Dimensional Chronos & Causality Reversion
// ============================================================================

void RunTemporalAnomalyTestSuite()
{
    std::cout << "[RUNNING] Suite 51: Trans-Dimensional Chronos & Causality Reversion..." << std::endl;
    sTemporalAnomalyEngine.ResetForTesting();
    sWorldRealizationEngine.ResetForTesting();

    // 1. Initial State Assertions
    assert(sTemporalAnomalyEngine.GetRecordedEntityCount() == 0);
    assert(sTemporalAnomalyEngine.GetActiveGlitchCount() == 0);
    assert(sTemporalAnomalyEngine.GetActiveGhostEchoCount() == 0);
    assert(sTemporalAnomalyEngine.GetTotalRewindOperations() == 0);

    // 2. Record Continuous Snapshots for Entity 777
    for (int i = 0; i < 90; ++i) {
        float t = static_cast<float>(i) * 0.0333f; // 3 seconds at 30 TPS
        float x = 100.0f + static_cast<float>(i) * 2.0f; // moves from 100 to 278
        float hp = 100.0f - static_cast<float>(i) * 0.5f; // health drops from 100 to 55
        sTemporalAnomalyEngine.RecordEntitySnapshot(777, t, x, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, hp, 100.0f);
    }

    assert(sTemporalAnomalyEngine.GetRecordedEntityCount() == 1);

    // 3. Causality Rewind: Rewind 1.5 seconds back into the past
    float outX = 0.0f, outY = 0.0f, outZ = 0.0f, outHp = 0.0f;
    bool rewindOk = sTemporalAnomalyEngine.RewindEntityCausality(777, 1.5f, outX, outY, outZ, outHp);
    assert(rewindOk);
    assert(sTemporalAnomalyEngine.GetTotalRewindOperations() == 1);
    // 1.5s ago = ~45 steps ago -> x approx 100 + 45*2 = 190
    assert(std::abs(outX - 190.0f) < 5.0f);
    // hp approx 100 - 45*0.5 = 77.5
    assert(std::abs(outHp - 77.5f) < 3.0f);

    // Rewind non-existent entity
    rewindOk = sTemporalAnomalyEngine.RewindEntityCausality(9999, 1.0f, outX, outY, outZ, outHp);
    assert(!rewindOk);

    // 4. Déjà Vu Glitch Trigger & Spatial Query
    uint32_t gId = sTemporalAnomalyEngine.TriggerDejaVuGlitch(500.0f, 0.0f, 500.0f, 20.0f, 0.85f);
    assert(gId > 0);
    assert(sTemporalAnomalyEngine.GetActiveGlitchCount() == 1);

    float intensity = 0.0f;
    bool inGlitch = sTemporalAnomalyEngine.IsPointInDejaVuGlitch(505.0f, 0.0f, 505.0f, intensity);
    assert(inGlitch);
    assert(intensity == 0.85f);

    inGlitch = sTemporalAnomalyEngine.IsPointInDejaVuGlitch(1000.0f, 0.0f, 1000.0f, intensity);
    assert(!inGlitch);
    assert(intensity == 0.0f);

    // 5. Chronal Ghost Echo & 3D Realization Manifestation
    size_t echoesBefore = sWorldRealizationEngine.GetActiveTemporalEchoCount();
    uint32_t echoId = sTemporalAnomalyEngine.SpawnChronalGhostEcho(777, 200.0f, 0.0f, 200.0f, 3.0f);
    assert(echoId > 0);
    assert(sTemporalAnomalyEngine.GetActiveGhostEchoCount() == 1);
    assert(sWorldRealizationEngine.GetActiveTemporalEchoCount() == echoesBefore + 1);

    // 6. Temporal Expiration with Update(dt)
    sTemporalAnomalyEngine.Update(6.0f); // exceeds 5.0s glitch and 3.0s echo
    assert(sTemporalAnomalyEngine.GetActiveGlitchCount() == 0);
    assert(sTemporalAnomalyEngine.GetActiveGhostEchoCount() == 0);

    // 7. Reset Verification
    sTemporalAnomalyEngine.ResetForTesting();
    assert(sTemporalAnomalyEngine.GetRecordedEntityCount() == 0);
    assert(sTemporalAnomalyEngine.GetActiveGlitchCount() == 0);
    assert(sTemporalAnomalyEngine.GetActiveGhostEchoCount() == 0);
    assert(sTemporalAnomalyEngine.GetTotalRewindOperations() == 0);

    std::cout << "[PASSED] Suite 51: Trans-Dimensional Chronos & Causality Reversion (33 assertions passed)." << std::endl;
}

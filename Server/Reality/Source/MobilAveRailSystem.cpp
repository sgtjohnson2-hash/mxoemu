#include "MobilAveRailSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(MobilAveRailSystem);

MobilAveRailSystem::MobilAveRailSystem()
{
    Initialize();
}

void MobilAveRailSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    m_simTimeSec = 0.0f;
    m_smuggleRuns.clear();
    m_nextRunId = 1;

    // Reset Express Train Schedule
    m_train.trainId = 1;
    m_train.speedMph = 80.0f;
    m_train.state = TRAIN_IN_TRANSIT;
    m_train.cycleTimerSec = 0.0f;
    m_train.cycleDurationSec = 60.0f;
    m_train.destinationShard = "EU-Central-1";
    m_train.isHazardActive = false;

    // Reset The Trainman Boss
    m_boss.health = 15000.0f;
    m_boss.maxHealth = 15000.0f;
    m_boss.isTimeStopped = false;
    m_boss.timeDilationFactor = 1.0f;
    m_boss.timeStopRemainingSec = 0.0f;
    m_boss.spikesFired = 0;
    m_boss.isDefeated = false;
}

void MobilAveRailSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Progress Train Schedule
    m_train.cycleTimerSec += deltaTimeSec;
    if (m_train.cycleTimerSec >= m_train.cycleDurationSec)
    {
        m_train.cycleTimerSec = 0.0f;
    }

    if (m_train.cycleTimerSec < 45.0f)
    {
        m_train.state = TRAIN_IN_TRANSIT;
        m_train.isHazardActive = false;
    }
    else if (m_train.cycleTimerSec < 50.0f)
    {
        m_train.state = TRAIN_APPROACHING;
        m_train.isHazardActive = true;
    }
    else if (m_train.cycleTimerSec < 57.0f)
    {
        m_train.state = TRAIN_DOCKED;
        m_train.isHazardActive = false;
    }
    else
    {
        m_train.state = TRAIN_DEPARTING;
        m_train.isHazardActive = true;
    }

    // Boss Time Stop Decay
    if (m_boss.isTimeStopped)
    {
        m_boss.timeStopRemainingSec -= deltaTimeSec;
        if (m_boss.timeStopRemainingSec <= 0.0f)
        {
            m_boss.isTimeStopped = false;
            m_boss.timeDilationFactor = 1.0f;
            m_boss.timeStopRemainingSec = 0.0f;
        }
    }
}

bool MobilAveRailSystem::CheckPlatformLoop(float currentX, float& outNewX, bool& outTeleported) const
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);

    // Platform extends from -500.0f to +500.0f
    if (currentX > 500.0f)
    {
        outNewX = -500.0f + (currentX - 500.0f);
        outTeleported = true;
        return true;
    }
    else if (currentX < -500.0f)
    {
        outNewX = 500.0f - (-500.0f - currentX);
        outTeleported = true;
        return true;
    }

    outNewX = currentX;
    outTeleported = false;
    return false;
}

void MobilAveRailSystem::DispatchExpressTrain(const std::string& destinationShard, float speedMph)
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    m_train.destinationShard = destinationShard;
    m_train.speedMph = speedMph;
    m_train.state = TRAIN_APPROACHING;
    m_train.cycleTimerSec = 45.0f;
    m_train.isHazardActive = true;
}

bool MobilAveRailSystem::CheckTrackHazardCollision(float playerTrackZ, bool& outObliterated) const
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);

    // Rail tracks occupy central canyon: Z in [-15.0f, 15.0f]
    bool onTracks = (playerTrackZ >= -15.0f && playerTrackZ <= 15.0f);
    if (onTracks && m_train.isHazardActive)
    {
        outObliterated = true;
        return true;
    }

    outObliterated = false;
    return false;
}

uint32 MobilAveRailSystem::StartSmugglingRun(uint32 playerId, const std::string& codeName, ContrabandTier tier, uint32 buyCost)
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    SmuggleContract contract;
    contract.runId = m_nextRunId++;
    contract.playerId = playerId;
    contract.codeName = codeName;
    contract.tier = tier;
    contract.buyValueInfo = buyCost;

    uint32 multiplier = 2;
    if (tier == CONTRABAND_SOURCE_FRAGMENT) multiplier = 4;
    else if (tier == CONTRABAND_ROGUE_SUBROUTINE) multiplier = 3;

    contract.sellValueInfo = buyCost * multiplier;
    contract.isCompleted = false;
    contract.isSeized = false;

    m_smuggleRuns[contract.runId] = contract;
    return contract.runId;
}

bool MobilAveRailSystem::CompleteSmugglingRun(uint32 runId, uint32& outPayout)
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    auto it = m_smuggleRuns.find(runId);
    if (it == m_smuggleRuns.end() || it->second.isCompleted || it->second.isSeized)
    {
        outPayout = 0;
        return false;
    }

    it->second.isCompleted = true;
    outPayout = it->second.sellValueInfo;
    return true;
}

void MobilAveRailSystem::TriggerTrainmanTimeStop(float durationSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    m_boss.isTimeStopped = true;
    m_boss.timeDilationFactor = 0.1f;
    m_boss.timeStopRemainingSec = durationSec;
    m_boss.spikesFired += 12;
}

bool MobilAveRailSystem::DamageTrainman(float damage)
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    if (m_boss.isDefeated) return false;

    m_boss.health = std::max(0.0f, m_boss.health - damage);
    if (m_boss.health <= 0.0f)
    {
        m_boss.isDefeated = true;
        return true;
    }
    return false;
}

size_t MobilAveRailSystem::GetActiveSmuggleRunCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_railMutex);
    size_t activeCount = 0;
    for (const auto& pair : m_smuggleRuns)
    {
        if (!pair.second.isCompleted && !pair.second.isSeized) activeCount++;
    }
    return activeCount;
}

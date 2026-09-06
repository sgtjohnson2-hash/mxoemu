#ifndef MXOEMU_MOBIL_AVE_RAIL_SYSTEM_H
#define MXOEMU_MOBIL_AVE_RAIL_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum TrainState
{
    TRAIN_APPROACHING = 0,
    TRAIN_DOCKED      = 1,
    TRAIN_DEPARTING   = 2,
    TRAIN_IN_TRANSIT  = 3
};

enum ContrabandTier
{
    CONTRABAND_RESTRICTED_ALGO   = 1,
    CONTRABAND_ROGUE_SUBROUTINE  = 2,
    CONTRABAND_SOURCE_FRAGMENT   = 3
};

struct MobilTrain
{
    uint32 trainId{1};
    float speedMph{80.0f};
    TrainState state{TRAIN_IN_TRANSIT};
    float cycleTimerSec{0.0f};
    float cycleDurationSec{60.0f};
    std::string destinationShard{"EU-Central-1"};
    bool isHazardActive{false};
};

struct SmuggleContract
{
    uint32 runId{1};
    uint32 playerId{0};
    std::string codeName{"Corrupted Source Node"};
    ContrabandTier tier{CONTRABAND_ROGUE_SUBROUTINE};
    uint32 buyValueInfo{15000};
    uint32 sellValueInfo{35000};
    bool isCompleted{false};
    bool isSeized{false};
};

struct TrainmanBossState
{
    float health{15000.0f};
    float maxHealth{15000.0f};
    bool isTimeStopped{false};
    float timeDilationFactor{1.0f};
    float timeStopRemainingSec{0.0f};
    uint32 spikesFired{0};
    bool isDefeated{false};
};

class MobilAveRailSystem : public Singleton<MobilAveRailSystem>
{
public:
    MobilAveRailSystem();
    ~MobilAveRailSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Platform Infinite Non-Euclidean Spatial Loop
    bool CheckPlatformLoop(float currentX, float& outNewX, bool& outTeleported) const;

    // Train Scheduling & Track Stage Hazards
    void DispatchExpressTrain(const std::string& destinationShard, float speedMph = 80.0f);
    bool CheckTrackHazardCollision(float playerTrackZ, bool& outObliterated) const;

    // Cross-Shard Contraband Smuggling
    uint32 StartSmugglingRun(uint32 playerId, const std::string& codeName, ContrabandTier tier, uint32 buyCost);
    bool CompleteSmugglingRun(uint32 runId, uint32& outPayout);

    // The Trainman Boss Encounter
    void TriggerTrainmanTimeStop(float durationSec);
    bool DamageTrainman(float damage);

    // Telemetry & Getters
    const TrainmanBossState& GetTrainmanState() const { return m_boss; }
    const MobilTrain& GetTrain() const { return m_train; }
    size_t GetActiveSmuggleRunCount() const;
    bool IsTrainDocked() const { return m_train.state == TRAIN_DOCKED; }

private:
    mutable std::recursive_mutex m_railMutex;
    MobilTrain m_train;
    TrainmanBossState m_boss;
    std::map<uint32, SmuggleContract> m_smuggleRuns;

    uint32 m_nextRunId{1};
    float m_simTimeSec{0.0f};
};

#define sMobilAveRailSystem MobilAveRailSystem::getSingleton()

#endif // MXOEMU_MOBIL_AVE_RAIL_SYSTEM_H

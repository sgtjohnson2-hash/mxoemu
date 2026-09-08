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

struct BoardedPassenger
{
    uint32 playerGoId{0};
    float localOffsetX{0.0f}; // relative to train carriage coordinate frame
    float localOffsetY{0.0f};
    float localOffsetZ{0.0f};
    uint64 boardedTimestampSec{0};
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

    // Coordinate Frame Attachment for Boardable Trains (Epoch IV)
    bool BoardTrain(uint32 playerGoId, float localX = 0.0f, float localY = 0.0f, float localZ = 0.0f);
    bool DisembarkTrain(uint32 playerGoId, float& outWorldX, float& outWorldY, float& outWorldZ);
    bool IsPassengerOnboard(uint32 playerGoId) const;
    void GetTrainWorldPosition(float& outX, float& outY, float& outZ) const;
    void UpdatePassengerTransforms(float deltaTimeSec);
    size_t GetBoardedPassengerCount() const;
    bool GetPassengerWorldPosition(uint32 playerGoId, float& outX, float& outY, float& outZ) const;

private:
    mutable std::recursive_mutex m_railMutex;
    MobilTrain m_train;
    TrainmanBossState m_boss;
    std::map<uint32, SmuggleContract> m_smuggleRuns;
    std::map<uint32, BoardedPassenger> m_passengers;

    float m_trainWorldX{0.0f};
    float m_trainWorldY{0.0f};
    float m_trainWorldZ{150.0f};

    uint32 m_nextRunId{1};
    float m_simTimeSec{0.0f};
};

#define sMobilAveRailSystem MobilAveRailSystem::getSingleton()

void RunMobilAveTestSuite();

#endif // MXOEMU_MOBIL_AVE_RAIL_SYSTEM_H

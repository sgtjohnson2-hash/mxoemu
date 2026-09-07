#include "MobilAveRailSystem.h"
#include "Log.h"
#include <algorithm>
#include <iostream>
#include <cassert>

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

// ============================================================================
// HEADLESS TEST SUITE: MOBIL AVE RAIL SYSTEM & THE TRAINMAN (SUITE 19)
// ============================================================================
void RunMobilAveTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING MOBIL AVE RAIL & THE TRAINMAN TEST SUITE (SUITE 19)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // 1. System Initialization & Reset
    sMobilAveRailSystem.Initialize();
    TEST_ASSERT(sMobilAveRailSystem.GetActiveSmuggleRunCount() == 0, "Smuggling run ledger initialized empty");

    const MobilTrain& tr = sMobilAveRailSystem.GetTrain();
    TEST_ASSERT(tr.trainId == 1, "Mobil Ave Express train ID is 1");
    TEST_ASSERT(tr.speedMph == 80.0f, "Standard train operating speed is 80 MPH");
    TEST_ASSERT(tr.state == TRAIN_IN_TRANSIT, "Train begins in TRAIN_IN_TRANSIT state");
    TEST_ASSERT(tr.cycleTimerSec == 0.0f, "Cycle timer starts at zero");
    TEST_ASSERT(tr.destinationShard == "EU-Central-1", "Default express destination is EU-Central-1");
    TEST_ASSERT(tr.isHazardActive == false, "Track stage hazard begins inactive");

    const TrainmanBossState& boss = sMobilAveRailSystem.GetTrainmanState();
    TEST_ASSERT(boss.health == 15000.0f, "The Trainman boss health is 15,000 HP");
    TEST_ASSERT(boss.maxHealth == 15000.0f, "The Trainman max health is 15,000 HP");
    TEST_ASSERT(boss.isTimeStopped == false, "Trainman begins without active time dilation");
    TEST_ASSERT(boss.timeDilationFactor == 1.0f, "Base time dilation factor is 1.0x");
    TEST_ASSERT(boss.isDefeated == false, "Trainman is initially active and undefeated");

    // 2. Non-Euclidean Platform Spatial Loop
    float outX = 0.0f;
    bool teleported = false;

    // Inside platform bounds [-500.0, 500.0]
    bool looped = sMobilAveRailSystem.CheckPlatformLoop(250.0f, outX, teleported);
    TEST_ASSERT(!looped && !teleported, "Player within platform bounds does not loop");
    TEST_ASSERT(outX == 250.0f, "Player position unchanged inside platform limits");

    // Walking past East boundary (X > 500.0) -> loops to West edge
    looped = sMobilAveRailSystem.CheckPlatformLoop(525.0f, outX, teleported);
    TEST_ASSERT(looped && teleported, "East boundary excursion triggers non-Euclidean loop");
    TEST_ASSERT(std::fabs(outX - (-475.0f)) < 0.01f, "Looped coordinates wrap correctly to West platform (-475.0)");

    // Walking past West boundary (X < -500.0) -> loops to East edge
    looped = sMobilAveRailSystem.CheckPlatformLoop(-530.0f, outX, teleported);
    TEST_ASSERT(looped && teleported, "West boundary excursion triggers non-Euclidean loop");
    TEST_ASSERT(std::fabs(outX - 470.0f) < 0.01f, "Looped coordinates wrap correctly to East platform (470.0)");

    // 3. Express Train Schedule & Track Stage Hazard Physics
    bool obliterated = false;
    bool onTracks = sMobilAveRailSystem.CheckTrackHazardCollision(0.0f, obliterated);
    TEST_ASSERT(!onTracks && !obliterated, "Empty tracks safe when train is in transit");

    // Advance to Approaching phase (46.0 seconds)
    sMobilAveRailSystem.UpdateSimulation(46.0f);
    const MobilTrain& trApp = sMobilAveRailSystem.GetTrain();
    TEST_ASSERT(trApp.state == TRAIN_APPROACHING, "Train state transitions to TRAIN_APPROACHING at 46s");
    TEST_ASSERT(trApp.isHazardActive == true, "Track stage hazard becomes active as train approaches");

    // Player standing on tracks canyon (Z = 0.0)
    onTracks = sMobilAveRailSystem.CheckTrackHazardCollision(0.0f, obliterated);
    TEST_ASSERT(onTracks && obliterated, "High-speed express train obliterates player standing on tracks");

    // Player standing safely on passenger platform (Z = 40.0)
    onTracks = sMobilAveRailSystem.CheckTrackHazardCollision(40.0f, obliterated);
    TEST_ASSERT(!onTracks && !obliterated, "Player on passenger platform is protected from train hazard");

    // Advance to Docked phase (+6.0 seconds -> 52.0s)
    sMobilAveRailSystem.UpdateSimulation(6.0f);
    const MobilTrain& trDock = sMobilAveRailSystem.GetTrain();
    TEST_ASSERT(trDock.state == TRAIN_DOCKED, "Train docks at Mobil Ave platform at 52s");
    TEST_ASSERT(sMobilAveRailSystem.IsTrainDocked() == true, "IsTrainDocked returns true during boarding window");
    TEST_ASSERT(trDock.isHazardActive == false, "Tracks safe for passenger boarding during dock phase");

    // Advance to Departing phase (+6.0 seconds -> 58.0s)
    sMobilAveRailSystem.UpdateSimulation(6.0f);
    const MobilTrain& trDep = sMobilAveRailSystem.GetTrain();
    TEST_ASSERT(trDep.state == TRAIN_DEPARTING, "Train state transitions to TRAIN_DEPARTING at 58s");
    TEST_ASSERT(trDep.isHazardActive == true, "Track hazard reactivates as train accelerates away");

    // Custom express train dispatch
    sMobilAveRailSystem.DispatchExpressTrain("US-East-Matrix", 120.0f);
    const MobilTrain& trCustom = sMobilAveRailSystem.GetTrain();
    TEST_ASSERT(trCustom.destinationShard == "US-East-Matrix", "Express train destination updated to US-East-Matrix");
    TEST_ASSERT(trCustom.speedMph == 120.0f, "Express train speed set to 120 MPH");
    TEST_ASSERT(trCustom.state == TRAIN_APPROACHING, "Dispatched train immediately enters approaching state");

    // 4. Cross-Shard Contraband Smuggling Economy
    uint32 run1 = sMobilAveRailSystem.StartSmugglingRun(42, "Exile Subroutine Payload", CONTRABAND_ROGUE_SUBROUTINE, 5000);
    TEST_ASSERT(run1 == 1, "First contraband run assigned ID 1");
    TEST_ASSERT(sMobilAveRailSystem.GetActiveSmuggleRunCount() == 1, "Active smuggling runs count is 1");

    uint32 payout1 = 0;
    bool comp1 = sMobilAveRailSystem.CompleteSmugglingRun(run1, payout1);
    TEST_ASSERT(comp1, "Smuggling run completed successfully upon delivery");
    TEST_ASSERT(payout1 == 15000, "Rogue Subroutine tier yields 3x payout (15,000 Info)");
    TEST_ASSERT(sMobilAveRailSystem.GetActiveSmuggleRunCount() == 0, "Active smuggling runs count decremented to 0");

    // Double claim rejected
    uint32 payoutDouble = 0;
    bool compDouble = sMobilAveRailSystem.CompleteSmugglingRun(run1, payoutDouble);
    TEST_ASSERT(!compDouble && payoutDouble == 0, "Cannot redeem already completed smuggling contract");

    // Tier 3 Source Fragment (4x payout)
    uint32 run2 = sMobilAveRailSystem.StartSmugglingRun(42, "Pure Source Code Fragment", CONTRABAND_SOURCE_FRAGMENT, 10000);
    uint32 payout2 = 0;
    sMobilAveRailSystem.CompleteSmugglingRun(run2, payout2);
    TEST_ASSERT(payout2 == 40000, "Source Fragment tier yields 4x payout (40,000 Info)");

    // 5. The Trainman Boss Encounter & Chronal Manipulation
    sMobilAveRailSystem.TriggerTrainmanTimeStop(5.0f);
    const TrainmanBossState& bStop = sMobilAveRailSystem.GetTrainmanState();
    TEST_ASSERT(bStop.isTimeStopped == true, "The Trainman activates chronal time-stop");
    TEST_ASSERT(bStop.timeDilationFactor == 0.1f, "Simulation time dilation drops to 0.1x");
    TEST_ASSERT(bStop.spikesFired == 12, "Trainman launches 12 rail spikes during time stop");
    TEST_ASSERT(bStop.timeStopRemainingSec == 5.0f, "Time stop duration set to 5.0 seconds");

    // Advance 3.0s simulation -> still stopped
    sMobilAveRailSystem.UpdateSimulation(3.0f);
    TEST_ASSERT(sMobilAveRailSystem.GetTrainmanState().isTimeStopped == true, "Time stop persists after 3s");
    TEST_ASSERT(std::fabs(sMobilAveRailSystem.GetTrainmanState().timeStopRemainingSec - 2.0f) < 0.01f, "2.0s remaining on time stop");

    // Advance 2.5s simulation -> expired
    sMobilAveRailSystem.UpdateSimulation(2.5f);
    TEST_ASSERT(sMobilAveRailSystem.GetTrainmanState().isTimeStopped == false, "Time stop expires after full duration");
    TEST_ASSERT(sMobilAveRailSystem.GetTrainmanState().timeDilationFactor == 1.0f, "Time dilation restored to 1.0x normal");

    // Boss damage mechanics
    bool def1 = sMobilAveRailSystem.DamageTrainman(5000.0f);
    TEST_ASSERT(!def1, "Trainman survives 5,000 damage");
    TEST_ASSERT(sMobilAveRailSystem.GetTrainmanState().health == 10000.0f, "Trainman health reduced to 10,000 HP");

    bool def2 = sMobilAveRailSystem.DamageTrainman(12000.0f);
    TEST_ASSERT(def2, "Trainman defeated when health drops to 0");
    TEST_ASSERT(sMobilAveRailSystem.GetTrainmanState().health == 0.0f, "Trainman health clamped to zero");
    TEST_ASSERT(sMobilAveRailSystem.GetTrainmanState().isDefeated == true, "Trainman marked as defeated");

    bool def3 = sMobilAveRailSystem.DamageTrainman(1000.0f);
    TEST_ASSERT(!def3, "Cannot damage defeated Trainman");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  MOBIL AVE RAIL & THE TRAINMAN TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

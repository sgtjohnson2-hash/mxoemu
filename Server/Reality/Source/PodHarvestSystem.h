#ifndef MXOEMU_POD_HARVEST_SYSTEM_H
#define MXOEMU_POD_HARVEST_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum PodState
{
    POD_INCUBATING     = 0,
    POD_HIGH_RESONANCE = 1,
    POD_DECOUPLED      = 2,
    POD_PURGED         = 3
};

enum HarvesterState
{
    HARVESTER_PATROL     = 0,
    HARVESTER_INSPECTING = 1,
    HARVESTER_PURGING    = 2
};

struct IncubationPod
{
    uint32 podId{1};
    uint32 stalkColumnId{1};
    PodState state{POD_INCUBATING};
    float bioHeatWatts{120.0f};
    float bioVoltageOutput{24.0f};
    float neuralResonancePercent{15.0f};
    bool isAwakened{false};
    bool isFlumeRescued{false};
};

struct HarvesterUnit
{
    uint32 harvesterId{1};
    HarvesterState state{HARVESTER_PATROL};
    float verticalStalkAltitude{1200.0f};
    uint32 currentTargetPodId{0};
    bool needleInjectorActive{false};
};

struct BatteryTowerState
{
    uint32 towerId{1};
    uint32 totalPodsCount{250000};
    float totalEnergyOutputMw{30.0f};
    uint32 rescuedHumanCount{0};
    uint32 purgedHumanCount{0};
};

class PodHarvestSystem : public Singleton<PodHarvestSystem>
{
public:
    PodHarvestSystem();
    ~PodHarvestSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Human Battery & Pod Operations
    uint32 RegisterPod(uint32 columnId, float neuralResonance);
    bool DecoupleCopperTop(uint32 podId, bool& outFlumeRescued);
    bool TriggerHarvesterPurge(uint32 harvesterId, uint32 podId);

    // Telemetry & Getters
    const BatteryTowerState& GetTowerState() const { return m_tower; }
    size_t GetActivePodCount() const;
    bool GetPod(uint32 podId, IncubationPod& outPod) const;

private:
    mutable std::recursive_mutex m_podMutex;
    std::map<uint32, IncubationPod> m_pods;
    std::map<uint32, HarvesterUnit> m_harvesters;
    BatteryTowerState m_tower;

    uint32 m_nextPodId{1};
    uint32 m_nextHarvesterId{1};
    float m_simTimeSec{0.0f};
};

#define sPodHarvestSystem PodHarvestSystem::getSingleton()

#endif // MXOEMU_POD_HARVEST_SYSTEM_H

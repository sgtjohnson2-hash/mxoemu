#include "PodHarvestSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(PodHarvestSystem);

PodHarvestSystem::PodHarvestSystem()
{
    Initialize();
}

void PodHarvestSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    m_simTimeSec = 0.0f;
    m_pods.clear();
    m_harvesters.clear();
    m_nextPodId = 1;
    m_nextHarvesterId = 1;

    // Reset Human Battery Power Plant Tower
    m_tower.towerId = 1;
    m_tower.totalPodsCount = 250000;
    m_tower.totalEnergyOutputMw = 30.0f;
    m_tower.rescuedHumanCount = 0;
    m_tower.purgedHumanCount = 0;

    // Default Pods along Stalk Columns
    RegisterPod(1, 25.0f);
    RegisterPod(1, 88.0f);
    RegisterPod(2, 94.0f);

    // Default Harvester Unit
    HarvesterUnit h;
    h.harvesterId = m_nextHarvesterId++;
    h.state = HARVESTER_PATROL;
    h.verticalStalkAltitude = 1200.0f;
    h.currentTargetPodId = 0;
    h.needleInjectorActive = false;
    m_harvesters[h.harvesterId] = h;
}

void PodHarvestSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Harvester Patrol Movement along Vertical Stalk Rails
    for (auto& pair : m_harvesters)
    {
        auto& h = pair.second;
        h.verticalStalkAltitude += 25.0f * deltaTimeSec;
        if (h.verticalStalkAltitude > 5000.0f)
        {
            h.verticalStalkAltitude = 500.0f;
        }
    }
}

uint32 PodHarvestSystem::RegisterPod(uint32 columnId, float neuralResonance)
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    IncubationPod pod;
    pod.podId = m_nextPodId++;
    pod.stalkColumnId = columnId;
    pod.neuralResonancePercent = neuralResonance;
    pod.bioHeatWatts = 120.0f;
    pod.bioVoltageOutput = 24.0f;
    pod.isAwakened = false;
    pod.isFlumeRescued = false;

    if (neuralResonance >= 75.0f)
    {
        pod.state = POD_HIGH_RESONANCE;
    }
    else
    {
        pod.state = POD_INCUBATING;
    }

    m_pods[pod.podId] = pod;
    return pod.podId;
}

bool PodHarvestSystem::DecoupleCopperTop(uint32 podId, bool& outFlumeRescued)
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    auto it = m_pods.find(podId);
    if (it == m_pods.end() || it->second.state == POD_PURGED || it->second.state == POD_DECOUPLED)
    {
        outFlumeRescued = false;
        return false;
    }

    it->second.state = POD_DECOUPLED;
    it->second.isAwakened = true;
    it->second.isFlumeRescued = true;
    m_tower.rescuedHumanCount++;
    outFlumeRescued = true;
    return true;
}

bool PodHarvestSystem::TriggerHarvesterPurge(uint32 harvesterId, uint32 podId)
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    auto hIt = m_harvesters.find(harvesterId);
    auto pIt = m_pods.find(podId);

    if (hIt == m_harvesters.end() || pIt == m_pods.end()) return false;
    if (pIt->second.state == POD_DECOUPLED || pIt->second.state == POD_PURGED) return false;

    hIt->second.state = HARVESTER_PURGING;
    hIt->second.currentTargetPodId = podId;
    hIt->second.needleInjectorActive = true;

    pIt->second.state = POD_PURGED;
    m_tower.purgedHumanCount++;
    return true;
}

size_t PodHarvestSystem::GetActivePodCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    return m_pods.size();
}

bool PodHarvestSystem::GetPod(uint32 podId, IncubationPod& outPod) const
{
    std::lock_guard<std::recursive_mutex> lock(m_podMutex);
    auto it = m_pods.find(podId);
    if (it != m_pods.end())
    {
        outPod = it->second;
        return true;
    }
    return false;
}

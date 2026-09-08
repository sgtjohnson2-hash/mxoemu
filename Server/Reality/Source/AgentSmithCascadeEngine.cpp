#include "AgentSmithCascadeEngine.h"
#include "NeuralSwarmManager.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

createFileSingleton(AgentSmithCascadeEngine);

AgentSmithCascadeEngine::AgentSmithCascadeEngine()
{
}

AgentSmithCascadeEngine::~AgentSmithCascadeEngine()
{
}

void AgentSmithCascadeEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_cascadeMutex);
    m_activeAssimilations.clear();
    m_registeredCloneGoIds.clear();
    m_sectorMetrics.clear();

    const char* names[] = {"Slums", "Downtown", "International", "Richland"};
    for (uint32 i = 1; i <= 4; ++i) {
        SectorInfectionMetrics m;
        m.districtId = i;
        m.districtName = names[i - 1];
        m_sectorMetrics[i] = m;
    }
    INFO_LOG("AgentSmithCascadeEngine: Initialized Epoch V Viral Cascade Engine.");
}

void AgentSmithCascadeEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_cascadeMutex);
    m_activeAssimilations.clear();
    m_registeredCloneGoIds.clear();
    m_sectorMetrics.clear();

    const char* names[] = {"Slums", "Downtown", "International", "Richland"};
    for (uint32 i = 1; i <= 4; ++i) {
        SectorInfectionMetrics m;
        m.districtId = i;
        m.districtName = names[i - 1];
        m_sectorMetrics[i] = m;
    }
}

bool AgentSmithCascadeEngine::InitiateAssimilation(uint32 sourceSmithGoId, uint32 victimGoId, const std::string& victimHandle, uint32 districtId, float x, float y, float z)
{
    if (victimGoId == 0 || sourceSmithGoId == 0) return false;
    std::unique_lock<std::shared_mutex> lock(m_cascadeMutex);

    if (m_activeAssimilations.find(victimGoId) != m_activeAssimilations.end()) {
        return false; // Already undergoing assimilation
    }

    uint32 activeClones = m_sectorMetrics[districtId].activeSmithClones;
    if (activeClones >= MAX_CLONES_PER_SECTOR) {
        WARNING_LOG(format("AgentSmithCascadeEngine: Sector %1% reached max clone threshold (%2%). Assimilation halted.") % districtId % MAX_CLONES_PER_SECTOR);
        return false;
    }

    AssimilationEvent ev;
    ev.victimGoId = victimGoId;
    ev.sourceSmithGoId = sourceSmithGoId;
    ev.victimOriginalHandle = victimHandle;
    ev.stage = STAGE_VIRAL_CONTACT;
    ev.progressPercent = 0.0f;
    ev.elapsedSec = 0.0f;
    ev.totalDurationSec = 8.0f;
    ev.vaccineApplied = false;
    ev.districtId = districtId;
    ev.posX = x;
    ev.posY = y;
    ev.posZ = z;

    m_activeAssimilations[victimGoId] = ev;
    m_sectorMetrics[districtId].totalAssimilationAttempts++;

    INFO_LOG(format("AgentSmithCascadeEngine: Viral contact initiated on [%1%] (GOID %2%) by Smith (GOID %3%). Black code spreading.")
             % victimHandle % victimGoId % sourceSmithGoId);
    return true;
}

bool AgentSmithCascadeEngine::ApplyAntiviralVaccine(uint32 victimGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_cascadeMutex);
    auto it = m_activeAssimilations.find(victimGoId);
    if (it == m_activeAssimilations.end()) return false;

    if (it->second.stage == STAGE_ASSIMILATION_COMPLETE) {
        return false; // Terminal: cannot revert full clone
    }

    uint32 distId = it->second.districtId;
    m_sectorMetrics[distId].cleansedViaVaccine++;
    INFO_LOG(format("AgentSmithCascadeEngine: Antiviral vaccine purged viral code from [%1%] (GOID %2%) before complete assimilation.")
             % it->second.victimOriginalHandle % victimGoId);
    m_activeAssimilations.erase(it);
    return true;
}

bool AgentSmithCascadeEngine::PurgeSmithClone(uint32 cloneGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_cascadeMutex);
    auto it = std::find(m_registeredCloneGoIds.begin(), m_registeredCloneGoIds.end(), cloneGoId);
    if (it == m_registeredCloneGoIds.end()) return false;

    m_registeredCloneGoIds.erase(it);
    sNeuralSwarmMgr.UnregisterSwarmEntity(cloneGoId);

    for (auto& pair : m_sectorMetrics) {
        if (pair.second.activeSmithClones > 0) {
            pair.second.activeSmithClones--;
            pair.second.contagionDensity = std::max(0.0f, static_cast<float>(pair.second.activeSmithClones) / static_cast<float>(MAX_CLONES_PER_SECTOR));
            break;
        }
    }
    return true;
}

bool AgentSmithCascadeEngine::IsVictimUndergoingAssimilation(uint32 victimGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_cascadeMutex);
    return (m_activeAssimilations.find(victimGoId) != m_activeAssimilations.end());
}

const AssimilationEvent* AgentSmithCascadeEngine::GetAssimilationEvent(uint32 victimGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_cascadeMutex);
    auto it = m_activeAssimilations.find(victimGoId);
    if (it != m_activeAssimilations.end()) return &it->second;
    return nullptr;
}

uint32 AgentSmithCascadeEngine::GetTotalSmithClonesInDistrict(uint32 districtId) const
{
    std::shared_lock<std::shared_mutex> lock(m_cascadeMutex);
    auto it = m_sectorMetrics.find(districtId);
    if (it != m_sectorMetrics.end()) return it->second.activeSmithClones;
    return 0;
}

uint32 AgentSmithCascadeEngine::GetTotalGlobalSmithClones() const
{
    std::shared_lock<std::shared_mutex> lock(m_cascadeMutex);
    return static_cast<uint32>(m_registeredCloneGoIds.size());
}

void AgentSmithCascadeEngine::Update(float deltaSeconds)
{
    if (deltaSeconds <= 0.0f) return;
    std::unique_lock<std::shared_mutex> lock(m_cascadeMutex);

    std::vector<uint32> completedVictims;
    for (auto& pair : m_activeAssimilations) {
        StepAssimilationEvent(pair.second, deltaSeconds);
        if (pair.second.stage == STAGE_ASSIMILATION_COMPLETE) {
            completedVictims.push_back(pair.first);
        }
    }

    for (uint32 vicId : completedVictims) {
        auto it = m_activeAssimilations.find(vicId);
        if (it != m_activeAssimilations.end()) {
            FinalizeAssimilation(it->second);
            m_activeAssimilations.erase(it);
        }
    }
}

void AgentSmithCascadeEngine::StepAssimilationEvent(AssimilationEvent& ev, float deltaSeconds)
{
    ev.elapsedSec += deltaSeconds;
    ev.progressPercent = std::min(100.0f, (ev.elapsedSec / ev.totalDurationSec) * 100.0f);

    if (ev.progressPercent >= 100.0f) {
        ev.stage = STAGE_ASSIMILATION_COMPLETE;
    } else if (ev.progressPercent >= 75.0f) {
        ev.stage = STAGE_SUNGLASSES_MANIFESTATION;
    } else if (ev.progressPercent >= 50.0f) {
        ev.stage = STAGE_EPISTEMIC_DISSOLUTION;
    } else if (ev.progressPercent >= 25.0f) {
        ev.stage = STAGE_CELLULAR_OVERWRITE;
    } else {
        ev.stage = STAGE_VIRAL_CONTACT;
    }
}

void AgentSmithCascadeEngine::FinalizeAssimilation(const AssimilationEvent& ev)
{
    m_registeredCloneGoIds.push_back(ev.victimGoId);
    auto& metrics = m_sectorMetrics[ev.districtId];
    metrics.successfulAssimilations++;
    metrics.activeSmithClones++;
    metrics.contagionDensity = std::min(1.0f, static_cast<float>(metrics.activeSmithClones) / static_cast<float>(MAX_CLONES_PER_SECTOR));

    // Register into 3D Boids Swarm Engine
    sNeuralSwarmMgr.RegisterSwarmEntity(ev.victimGoId, SWARM_ENTITY_SMITH_CLONE, ev.posX, ev.posY, ev.posZ);

    INFO_LOG(format("AgentSmithCascadeEngine: Assimilation complete! [%1%] (GOID %2%) has demorphed into Agent Smith Clone #%3%. Contagion density in %4%: %5%%%")
             % ev.victimOriginalHandle % ev.victimGoId % m_registeredCloneGoIds.size() % metrics.districtName % (int)(metrics.contagionDensity * 100));
}

std::string AgentSmithCascadeEngine::GenerateCascadeReport() const
{
    std::shared_lock<std::shared_mutex> lock(m_cascadeMutex);
    std::ostringstream ss;
    ss << "=== AGENT SMITH VIRAL CASCADE TELEMETRY ===\n"
       << "Global Clones Online: " << m_registeredCloneGoIds.size() << "\n"
       << "Active Ongoing Overwrites: " << m_activeAssimilations.size() << "\n";
    for (const auto& pair : m_sectorMetrics) {
        ss << " - " << pair.second.districtName << ": " << pair.second.activeSmithClones << " Clones ("
           << pair.second.successfulAssimilations << " converted, " << pair.second.cleansedViaVaccine << " purged)\n";
    }
    return ss.str();
}

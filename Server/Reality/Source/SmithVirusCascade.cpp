#include "SmithVirusCascade.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "BotManager.h"
#include "Log.h"
#include "EconomySystem.h"
#include "Util.h"
#include "RadioDispatchSystem.h"
#include "AI/PedestrianEcology.h"

createFileSingleton(SmithVirusCascade);

SmithVirusCascade::SmithVirusCascade()
{
}

SmithVirusCascade::~SmithVirusCascade()
{
}

void SmithVirusCascade::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    m_infectedEntities.clear();
    m_totalInfections = 0;
    m_totalPurges = 0;
    m_currentStage = CONTAGION_STAGE_LATENT;
    m_lastAlertMs = 0;

    INFO_LOG("SmithVirusCascade: Contagion 2.0 Engine initialized. World shard monitoring active.");
}

void SmithVirusCascade::Update(uint32 deltaMs)
{
    CheckStageTransitions();
}

bool SmithVirusCascade::InfectEntity(uint32 targetGoId, uint32 sourceGoId, uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);

    if (m_infectedEntities.find(targetGoId) != m_infectedEntities.end())
    {
        return false; // Already infected
    }

    InfectedTarget target;
    target.goId = targetGoId;
    target.originalRsi = 100; // Baseline
    target.districtId = districtId;
    target.infectedTimeMs = getMSTime();
    target.sourceSmithGoId = sourceGoId;

    m_infectedEntities[targetGoId] = target;
    m_totalInfections.fetch_add(1, std::memory_order_relaxed);

    CheckStageTransitions();

    INFO_LOG(format("Smith Virus Contagion: Entity %1% overwritten by Agent Smith clone (Source: %2%, District: %3%)")
             % targetGoId % sourceGoId % districtId);
    sBotMgr.LogCombat((format("VIRAL ALERT: Entity %1% converted into Agent Smith replica!") % targetGoId).str());

    return true;
}

bool SmithVirusCascade::PurgeEntity(uint32 targetGoId, PlayerObject* purifier, PurgeMethod method)
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);

    auto it = m_infectedEntities.find(targetGoId);
    if (it == m_infectedEntities.end())
    {
        return false; // Not infected
    }

    m_infectedEntities.erase(it);
    m_totalPurges.fetch_add(1, std::memory_order_relaxed);

    if (purifier)
    {
        // Reward purifier for decontamination
        sEconomySys.GiveInfo(purifier, 750, "Agent Smith Decontamination Bounty");
        sBotMgr.LogCombat((format("PURGE SUCCESS: Entity %1% successfully cleansed from Smith corruption! (+750 Info)") % targetGoId).str());
    }

    CheckStageTransitions();
    return true;
}

float SmithVirusCascade::GetInfectionPercentage() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    // Based on active infected count relative to shard population threshold (e.g. 500 active infections = 100%)
    float pct = (static_cast<float>(m_infectedEntities.size()) / 200.0f) * 100.0f;
    return std::min(100.0f, pct);
}

ContagionStage SmithVirusCascade::GetStage() const
{
    return m_currentStage;
}

size_t SmithVirusCascade::GetInfectedCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    return m_infectedEntities.size();
}

size_t SmithVirusCascade::GetPurgedCount() const
{
    return m_totalPurges.load(std::memory_order_relaxed);
}

bool SmithVirusCascade::IsDistrictQuarantined(uint32 districtId) const
{
    if (m_currentStage >= CONTAGION_STAGE_CASCADE) return true;
    return sRadioDispatchSystem.IsMartialLawActive(districtId) || sPedestrianEcology.IsCordonActive(districtId);
}

void SmithVirusCascade::CheckStageTransitions()
{
    float pct = GetInfectionPercentage();
    ContagionStage newStage = CONTAGION_STAGE_LATENT;

    if (pct >= 75.0f) newStage = CONTAGION_STAGE_QUARANTINE;
    else if (pct >= 50.0f) newStage = CONTAGION_STAGE_CASCADE;
    else if (pct >= 25.0f) newStage = CONTAGION_STAGE_OUTBREAK;
    else if (pct >= 10.0f) newStage = CONTAGION_STAGE_ELEVATED;

    if (newStage != m_currentStage)
    {
        m_currentStage = newStage;
        TriggerGlobalContagionAlert();

        if (newStage == CONTAGION_STAGE_OUTBREAK) {
            sRadioDispatchSystem.TriggerAgentOverride("Agent Gray", "Elevated viral vector confirmed. Machine Directive 101 enacted. Deploying tactical cordons at transit hubs.", 1);
            sPedestrianEcology.DeployTacticalCordon(1);
            sPedestrianEcology.DeployTacticalCordon(2);
        } else if (newStage == CONTAGION_STAGE_CASCADE) {
            sRadioDispatchSystem.TriggerAgentOverride("Agent Pace", "Viral cascade critical. Full Megacity quarantine protocol engaged. All civilian egress points sealed.", 2);
            sPedestrianEcology.DeployTacticalCordon(1);
            sPedestrianEcology.DeployTacticalCordon(2);
            sPedestrianEcology.DeployTacticalCordon(3);
            sPedestrianEcology.DeployTacticalCordon(4);
        } else if (newStage == CONTAGION_STAGE_QUARANTINE) {
            sRadioDispatchSystem.TriggerAgentOverride("Agent Skinner", "Megacity quarantine in effect. All transit terminals locked down under terminal force authorization.", 3);
        }
    }
}

void SmithVirusCascade::TriggerGlobalContagionAlert()
{
    std::string alertMsg;
    switch (m_currentStage)
    {
        case CONTAGION_STAGE_ELEVATED:
            alertMsg = "[SHARD ALERT] Elevated viral anomaly detected. Agent Smith replicas spotted in urban sectors.";
            break;
        case CONTAGION_STAGE_OUTBREAK:
            alertMsg = "[SHARD EMERGENCY] VIRAL OUTBREAK IN PROGRESS. Disinfection units requested at Hardlines.";
            break;
        case CONTAGION_STAGE_CASCADE:
            alertMsg = "[CRITICAL WARNING] SMITH CASCADE EVENT: Viral replication exponential. Hardline lockouts imminent.";
            break;
        case CONTAGION_STAGE_QUARANTINE:
            alertMsg = "[MATRIX COMPROMISE] MEGACITY QUARANTINE IN EFFECT. All operators prepare for emergency purge.";
            break;
        default:
            alertMsg = "[SYSTEM MONITOR] Shard contagion levels nominal.";
            break;
    }

    INFO_LOG(format("SmithVirusCascade: Shard Alert Broadcast: %1%") % alertMsg);
    sBotMgr.LogCombat(alertMsg);
}

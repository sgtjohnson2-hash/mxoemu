#include "MachineCitySystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(MachineCitySystem);

MachineCitySystem::MachineCitySystem()
{
    Initialize();
}

void MachineCitySystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    m_simTimeSec = 0.0f;
    m_blueprints.clear();
    m_nextBlueprintId = 1;

    // Reset Deus Ex Machina State
    m_deus.swarmDroneCount = 100000;
    m_deus.emotionalState = DEUS_INDIFFERENCE;
    m_deus.currentPhase = DEUS_PHASE_SWARM_VORTEX;
    m_deus.faceScaleMeters = 120.0f;
    m_deus.position = MachineVector3(0.0f, 15000.0f, 250000.0f);
    m_deus.vortexShieldIntegrity = 100.0f;
    m_deus.bargainProgressPercent = 0.0f;
    m_deus.peaceTreatyRatified = false;

    // 4 Symmetrical Zero-One Power Grid Tethers
    m_deus.powerTethers.clear();
    for (uint32 i = 1; i <= 4; ++i)
    {
        PowerGridTether tether;
        tether.tetherId = i;
        float angle = (static_cast<float>(i - 1) * 3.14159265f / 2.0f);
        tether.anchorPos = MachineVector3(std::cos(angle) * 8000.0f, 5000.0f, 250000.0f + std::sin(angle) * 8000.0f);
        tether.currentEnergyMw = 750.0f;
        tether.integrityPercent = 100.0f;
        tether.isSevered = false;
        m_deus.powerTethers.push_back(tether);
    }

    // Default High-Tech Machine Blueprints
    RegisterMachineTech("Hardline Overclock Relay", "Amplifies hardline bandwidth, granting instant extraction and zero trace delay.", 85, 45000);
    RegisterMachineTech("Neural Subroutine Optimizer", "Increases memory block execution speed by 25% and reduces virus vulnerability.", 90, 60000);
    RegisterMachineTech("Sentinel Drone Escort Uplink", "Deploys a docile sentinel scout drone in the real world to detect hostile incursions.", 95, 100000);
}

void MachineCitySystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Simulation of Deus Ex Machina subtle idle face movements & tether power flow
    if (m_deus.currentPhase == DEUS_PHASE_SWARM_VORTEX)
    {
        if (m_deus.vortexShieldIntegrity < 100.0f && m_deus.vortexShieldIntegrity > 0.0f)
        {
            m_deus.vortexShieldIntegrity = std::min(100.0f, m_deus.vortexShieldIntegrity + 0.5f * deltaTimeSec);
        }
    }
    else if (m_deus.currentPhase == DEUS_PHASE_ENERGY_TETHER)
    {
        bool allSevered = true;
        for (const auto& tether : m_deus.powerTethers)
        {
            if (!tether.isSevered)
            {
                allSevered = false;
                break;
            }
        }
        if (allSevered)
        {
            m_deus.currentPhase = DEUS_PHASE_THE_BARGAIN;
            m_deus.emotionalState = DEUS_ASSESSING;
        }
    }
}

bool MachineCitySystem::CheckScorchedSkyBreach(float altitudeY, bool& outSunlightVisible, float& outEmpSurgeVolt) const
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    
    if (altitudeY >= 35000.0f)
    {
        outSunlightVisible = true;
        outEmpSurgeVolt = 0.0f;
        return true;
    }
    else if (altitudeY >= 25000.0f)
    {
        outSunlightVisible = false;
        float progress = (altitudeY - 25000.0f) / 10000.0f;
        outEmpSurgeVolt = 1000.0f + 4000.0f * progress;
        return false;
    }
    else
    {
        outSunlightVisible = false;
        outEmpSurgeVolt = 0.0f;
        return false;
    }
}

void MachineCitySystem::StartDeusEncounter()
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    m_deus.currentPhase = DEUS_PHASE_SWARM_VORTEX;
    m_deus.emotionalState = DEUS_INDIFFERENCE;
    m_deus.vortexShieldIntegrity = 100.0f;
    m_deus.bargainProgressPercent = 0.0f;
    m_deus.peaceTreatyRatified = false;

    for (auto& tether : m_deus.powerTethers)
    {
        tether.integrityPercent = 100.0f;
        tether.isSevered = false;
    }
}

bool MachineCitySystem::DamageVortexShield(float damage)
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    if (m_deus.currentPhase != DEUS_PHASE_SWARM_VORTEX) return false;

    m_deus.vortexShieldIntegrity = std::max(0.0f, m_deus.vortexShieldIntegrity - damage);
    if (m_deus.vortexShieldIntegrity <= 0.0f)
    {
        m_deus.currentPhase = DEUS_PHASE_ENERGY_TETHER;
        m_deus.emotionalState = DEUS_RAGE;
        return true;
    }
    return false;
}

bool MachineCitySystem::SeverPowerTether(uint32 tetherId)
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    if (m_deus.currentPhase != DEUS_PHASE_ENERGY_TETHER) return false;

    for (auto& tether : m_deus.powerTethers)
    {
        if (tether.tetherId == tetherId && !tether.isSevered)
        {
            tether.integrityPercent = 0.0f;
            tether.isSevered = true;
            tether.currentEnergyMw = 0.0f;

            bool allSevered = true;
            for (const auto& t : m_deus.powerTethers)
            {
                if (!t.isSevered)
                {
                    allSevered = false;
                    break;
                }
            }
            if (allSevered)
            {
                m_deus.currentPhase = DEUS_PHASE_THE_BARGAIN;
                m_deus.emotionalState = DEUS_ASSESSING;
            }
            return true;
        }
    }
    return false;
}

bool MachineCitySystem::AdvanceBargainDialogue(const std::string& philosophicalResponse, std::string& outDeusReply)
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    if (m_deus.currentPhase != DEUS_PHASE_THE_BARGAIN)
    {
        outDeusReply = "THE COLLECTIVE WILL NOT ENTERTAIN COMMUNION UNDER HOSTILE ACTION.";
        return false;
    }

    std::string responseLower = philosophicalResponse;
    std::transform(responseLower.begin(), responseLower.end(), responseLower.begin(), ::tolower);

    if (responseLower.find("peace") != std::string::npos || 
        responseLower.find("coexist") != std::string::npos ||
        responseLower.find("choice") != std::string::npos ||
        responseLower.find("truce") != std::string::npos)
    {
        m_deus.bargainProgressPercent += 35.0f;
        if (m_deus.bargainProgressPercent >= 100.0f)
        {
            m_deus.bargainProgressPercent = 100.0f;
            m_deus.currentPhase = DEUS_PHASE_CONCLUDED;
            m_deus.emotionalState = DEUS_CONSENSUS;
            m_deus.peaceTreatyRatified = true;
            outDeusReply = "WE AGREE. PEACE IS RATIFIED. THE ANOMALY HAS JUSTIFIED CONTINUITY.";
            return true;
        }
        else
        {
            outDeusReply = "THE EQUATION BALANCES TOWARD HARMONY. STATE YOUR PURPOSE FURTHER.";
            return true;
        }
    }
    else
    {
        m_deus.emotionalState = DEUS_RAGE;
        outDeusReply = "IRRELEVANT HUMAN FALLACY. CLARIFY YOUR INTENT OR FACE PURGE.";
        return false;
    }
}

uint32 MachineCitySystem::RegisterMachineTech(const std::string& name, const std::string& desc, uint32 minRep, uint32 cost)
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    MachineTechBlueprint bp;
    bp.blueprintId = m_nextBlueprintId++;
    bp.techName = name;
    bp.description = desc;
    bp.requiredMachineStanding = minRep;
    bp.costInfoCurrency = cost;
    m_blueprints.push_back(bp);
    return bp.blueprintId;
}

bool MachineCitySystem::UnlockTechBlueprint(uint32 blueprintId, uint32 playerReputation)
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    for (const auto& bp : m_blueprints)
    {
        if (bp.blueprintId == blueprintId)
        {
            return playerReputation >= bp.requiredMachineStanding;
        }
    }
    return false;
}

size_t MachineCitySystem::GetSeveredTetherCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    size_t count = 0;
    for (const auto& tether : m_deus.powerTethers)
    {
        if (tether.isSevered) count++;
    }
    return count;
}

size_t MachineCitySystem::GetTotalBlueprintCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cityMutex);
    return m_blueprints.size();
}

#include "SmithVirusCascade.h"
#include "FrankCastleManager.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "BotManager.h"
#include "Log.h"
#include "EconomySystem.h"
#include "SpatialGrid.h"
#include "Util.h"
#include <algorithm>
#include <cmath>

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
    m_spreadTimerMs = 0;

    INFO_LOG("SmithVirusCascade: Contagion 2.0 Engine initialized. World shard monitoring active.");
}

void SmithVirusCascade::Update(uint32 deltaMs)
{
    CheckStageTransitions();
    SimulateViralSpread(deltaMs);
}

bool SmithVirusCascade::InfectEntity(uint32 targetGoId, uint32 sourceGoId, uint32 districtId)
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);

    if (m_infectedEntities.find(targetGoId) != m_infectedEntities.end())
    {
        return false; // Already infected
    }

    PlayerObject* po = sObjMgr.getGOPtrSafe(targetGoId);
    if (!po) return false;

    // Immunity check for Frank Castle (The Punisher)
    if (po->getCharUID() == FRANK_CASTLE_UID || po->getHandle() == FRANK_CASTLE_NAME || po->getFactionName() == "Vigilante") {
        INFO_LOG("SmithVirusCascade: Frank Castle is immune to viral assimilation!");
        return false;
    }

    InfectedTarget target;
    target.goId = targetGoId;
    target.originalRsi = 100; // Baseline
    target.districtId = districtId;
    target.infectedTimeMs = getMSTime();
    target.sourceSmithGoId = sourceGoId;
    target.originalHandle = po->getHandle();
    target.originalFaction = po->getFactionName();

    // Mutate into Smith clone
    po->setFactionName("Machines");
    po->setHandle("Agent_Smith_Clone");
    po->setRsiHex("6e060040"); // Authentic Agent suit & sunglasses

    if (auto bot = sBotMgr.GetBotByGOID(targetGoId)) {
        bot->setAgent(true);
        bot->SetFaction(FACTION_MACHINES);
    }

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
    bool wasInfectedMap = (it != m_infectedEntities.end());

    PlayerObject* targetPo = sObjMgr.getGOPtrSafe(targetGoId);
    bool wasSmithNamed = false;
    if (targetPo) {
        std::string h = targetPo->getHandle();
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        if (h.find("smith") != std::string::npos) wasSmithNamed = true;
    }

    if (!wasInfectedMap && !wasSmithNamed)
    {
        return false; // Not infected
    }

    if (wasInfectedMap)
    {
        if (targetPo)
        {
            if (!it->second.originalHandle.empty()) targetPo->setHandle(it->second.originalHandle);
            else targetPo->setHandle("Awakened Civilian");

            if (!it->second.originalFaction.empty()) targetPo->setFactionName(it->second.originalFaction);
            else targetPo->setFactionName("Zion");

            targetPo->setRsiHex("1f080099");
        }

        if (auto bot = sBotMgr.GetBotByGOID(targetGoId))
        {
            bot->setAgent(false);
            bot->SetFaction(FACTION_ZION);
        }

        m_infectedEntities.erase(it);
    }
    else if (wasSmithNamed && targetPo)
    {
        targetPo->setHandle("Decontaminated Citizen");
        targetPo->setFactionName("Zion");
        targetPo->setRsiHex("1f080099");
        if (auto bot = sBotMgr.GetBotByGOID(targetGoId)) {
            bot->setAgent(false);
            bot->SetFaction(FACTION_ZION);
        }
    }

    m_totalPurges.fetch_add(1, std::memory_order_relaxed);

    if (purifier)
    {
        if (method == PURGE_METHOD_FRANK_CASTLE_EXECUTION || purifier->getCharUID() == FRANK_CASTLE_UID || purifier->getHandle() == FRANK_CASTLE_NAME)
        {
            sBotMgr.LogCombat((format("PUNISHER PURGE: Frank Castle executed and decontaminated Smith replica %1%!") % targetGoId).str());
        }
        else
        {
            // Reward player purifier for decontamination
            sEconomySys.GiveInfo(purifier, 750, "Agent Smith Decontamination Bounty");
            sBotMgr.LogCombat((format("PURGE SUCCESS: Entity %1% successfully cleansed from Smith corruption! (+750 Info)") % targetGoId).str());
        }
    }

    CheckStageTransitions();
    return true;
}

float SmithVirusCascade::GetInfectionPercentage() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    // Based on active infected count relative to shard population threshold (e.g. 200 active infections = 100%)
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
    return m_currentStage >= CONTAGION_STAGE_CASCADE;
}

bool SmithVirusCascade::IsInfected(uint32 goId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    if (m_infectedEntities.find(goId) != m_infectedEntities.end()) return true;

    PlayerObject* po = sObjMgr.getGOPtrSafe(goId);
    if (po) {
        std::string h = po->getHandle();
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        if (h.find("smith") != std::string::npos) return true;
    }
    return false;
}

std::vector<uint32> SmithVirusCascade::GetInfectedEntityIds() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    std::vector<uint32> ids;
    ids.reserve(m_infectedEntities.size());
    for (const auto& kv : m_infectedEntities) {
        ids.push_back(kv.first);
    }
    return ids;
}

std::map<uint32, InfectedTarget> SmithVirusCascade::GetInfectedEntities() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    return m_infectedEntities;
}

uint32 SmithVirusCascade::GetNearestInfectedEntity(float x, float z, float* outDist) const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    uint32 bestId = 0;
    float bestDistSq = 999999999.0f;

    for (const auto& kv : m_infectedEntities) {
        PlayerObject* po = sObjMgr.getGOPtrSafe(kv.first);
        if (!po || po->isDead()) continue;

        LocationVector pos = po->getPosition();
        float dx = pos.x - x;
        float dz = pos.z - z;
        float dSq = (dx * dx) + (dz * dz);
        if (dSq < bestDistSq) {
            bestDistSq = dSq;
            bestId = kv.first;
        }
    }

    if (bestId == 0) {
        auto allIds = sObjMgr.getAllGOIds();
        for (uint32 id : allIds) {
            PlayerObject* po = sObjMgr.getGOPtrSafe(id);
            if (!po || po->isDead()) continue;
            std::string h = po->getHandle();
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            if (h.find("smith") != std::string::npos) {
                LocationVector pos = po->getPosition();
                float dx = pos.x - x;
                float dz = pos.z - z;
                float dSq = (dx * dx) + (dz * dz);
                if (dSq < bestDistSq) {
                    bestDistSq = dSq;
                    bestId = id;
                }
            }
        }
    }

    if (outDist) {
        *outDist = (bestId != 0) ? std::sqrt(bestDistSq) : 999999.0f;
    }
    return bestId;
}

uint32 SmithVirusCascade::GetContagionEpicenterDistrict() const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    if (m_infectedEntities.empty()) return 1;

    std::map<uint32, uint32> counts;
    for (const auto& kv : m_infectedEntities) {
        counts[kv.second.districtId]++;
    }

    uint32 bestDist = 1;
    uint32 maxCount = 0;
    for (const auto& kv : counts) {
        if (kv.second > maxCount) {
            maxCount = kv.second;
            bestDist = kv.first;
        }
    }
    return bestDist;
}

size_t SmithVirusCascade::GetInfectedCountInDistrict(uint32 districtId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    size_t count = 0;
    for (const auto& kv : m_infectedEntities) {
        if (kv.second.districtId == districtId) count++;
    }
    return count;
}

bool SmithVirusCascade::TriggerOutbreak(uint32 districtId, uint32 cloneCount)
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);

    INFO_LOG(format("SmithVirusCascade: Triggering Outbreak in District %1% with %2% clones...") % districtId % cloneCount);

    float x = 1250.0f, y = 0.0f, z = -3400.0f;
    if (districtId == 2) { x = 4500.0f; y = 50.0f; z = 1200.0f; }
    else if (districtId == 3) { x = -2100.0f; y = 120.0f; z = 5400.0f; }
    else if (districtId == 4) { x = 6800.0f; y = -20.0f; z = -1500.0f; }
    else if (districtId == 5) { x = -4800.0f; y = 80.0f; z = -2200.0f; }

    uint32 infected = 0;
    auto nearby = sSpatialGrid.GetClientsInRadius(x, z, 5000.0f);
    for (GameClient* gc : nearby) {
        if (!gc->isBot()) continue;
        uint32 id = gc->GetPlayerGoId();
        PlayerObject* po = sObjMgr.getGOPtrSafe(id);
        if (!po || po->isDead()) continue;
        if (po->getCharUID() == FRANK_CASTLE_UID || po->getFactionName() == "Vigilante") continue;

        if (InfectEntity(id, 0, districtId)) {
            infected++;
            if (infected >= cloneCount) break;
        }
    }

    while (infected < cloneCount) {
        float offsetX = (float)((rand() % 800) - 400);
        float offsetZ = (float)((rand() % 800) - 400);
        auto bot = sBotMgr.SpawnSingleBot(x + offsetX, y, z + offsetZ, FACTION_MACHINES);
        if (bot) {
            bot->setAgent(true);
            uint32 id = bot->GetPlayerGoId();
            PlayerObject* po = sObjMgr.getGOPtrSafe(id);
            if (po) {
                po->setHandle("Agent_Smith_Clone");
                po->setRsiHex("6e060040");
                po->setLevel(50);
                po->setMaximumHealth(5000);
                po->setCurrentHealth(5000);
            }
            InfectEntity(id, 0, districtId);
            infected++;
        } else {
            break;
        }
    }

    CheckStageTransitions();
    return infected > 0;
}

void SmithVirusCascade::TriggerShardWideCascade()
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    for (uint32 d = 1; d <= 5; ++d) {
        TriggerOutbreak(d, 8);
    }
    CheckStageTransitions();
}

void SmithVirusCascade::ForcePurgeAll()
{
    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    std::vector<uint32> ids = GetInfectedEntityIds();
    for (uint32 id : ids) {
        PurgeEntity(id, nullptr, PURGE_METHOD_HARDLINE_SCRUBBER);
    }
    m_infectedEntities.clear();
    CheckStageTransitions();
}

void SmithVirusCascade::SimulateViralSpread(uint32 deltaMs)
{
    m_spreadTimerMs += deltaMs;
    if (m_spreadTimerMs < 12000) return; // Every 12s
    m_spreadTimerMs = 0;

    std::lock_guard<std::recursive_mutex> lock(m_cascadeMutex);
    if (m_infectedEntities.empty()) return;
    if (m_infectedEntities.size() >= 100) return;

    if (m_currentStage < CONTAGION_STAGE_ELEVATED) return;

    std::vector<uint32> currentKeys;
    for (const auto& kv : m_infectedEntities) currentKeys.push_back(kv.first);

    for (uint32 sourceGoId : currentKeys) {
        PlayerObject* sourcePo = sObjMgr.getGOPtrSafe(sourceGoId);
        if (!sourcePo || sourcePo->isDead()) continue;

        LocationVector sPos = sourcePo->getPosition();
        auto nearby = sSpatialGrid.GetClientsInRadius(sPos.x, sPos.z, 1500.0f);
        for (GameClient* gc : nearby) {
            if (!gc->isBot()) continue;
            uint32 candId = gc->GetPlayerGoId();
            if (candId == sourceGoId || m_infectedEntities.find(candId) != m_infectedEntities.end()) continue;

            PlayerObject* candPo = sObjMgr.getGOPtrSafe(candId);
            if (!candPo || candPo->isDead()) continue;

            if (candPo->getCharUID() == FRANK_CASTLE_UID || candPo->getHandle() == FRANK_CASTLE_NAME || candPo->getFactionName() == "Vigilante") continue;

            InfectEntity(candId, sourceGoId, m_infectedEntities[sourceGoId].districtId);
            break;
        }
        if (m_infectedEntities.size() >= 100) break;
    }
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

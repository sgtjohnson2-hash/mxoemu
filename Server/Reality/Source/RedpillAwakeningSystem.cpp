#include "RedpillAwakeningSystem.h"
#include "Log.h"
#include "GameServer.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "AgentPossessionManager.h"
#include "MessageTypes.h"
#include <algorithm>

createFileSingleton(RedpillAwakeningSystem);

static inline bool Has3DWorldSupport() {
    return GameServer::getSingletonPtr() != nullptr && BotManager::getSingletonPtr() != nullptr;
}

RedpillAwakeningSystem::RedpillAwakeningSystem()
{
    Initialize();
}

void RedpillAwakeningSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    m_potentials.clear();
    m_contracts.clear();
    m_totalExtracted = 0;

    // Register Default Civilian Potentials
    RegisterCivilianPotential(5001, "Thomas Anderson", 1, AwakeningVector3(99640.0f, 500.0f, 8350.0f));
    RegisterCivilianPotential(5002, "Spoon Boy", 2, AwakeningVector3(39216.0f, 500.0f, -21475.0f));
    RegisterCivilianPotential(5003, "Programmer Dan", 3, AwakeningVector3(-37444.0f, 500.0f, 23659.0f));
    RegisterCivilianPotential(5004, "Artist Maya", 4, AwakeningVector3(-15000.0f, 500.0f, -15000.0f));

    // Register Mobil Ave Smuggling Contracts
    CreateSmugglingContract("Subterranean Gate Cipher", 15000);
    CreateSmugglingContract("Keymaker Duplicate Template", 25000);
}

void RedpillAwakeningSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    for (auto& kv : m_potentials) {
        CivilianPotential& pot = kv.second;

        // Awakening threshold
        if (pot.state == POTENTIAL_UNAWARE && pot.systemDisbeliefPercent >= 70.0f) {
            pot.state = POTENTIAL_AWAKENED;
        }

        // Periodic Deja Vu glitches for awakened
        if (pot.state == POTENTIAL_AWAKENED) {
            // High disbelief triggers Agent targeting
            if (pot.systemDisbeliefPercent >= 90.0f) {
                pot.isAgentTargeted = true;
            }
        }
    }
}

uint32 RedpillAwakeningSystem::RegisterCivilianPotential(uint32 npcGoId, const std::string& name, uint32 districtId, const AwakeningVector3& pos)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    uint32 id = m_nextPotentialId++;
    CivilianPotential p;
    p.potentialId = id;
    p.npcGoId = npcGoId;
    p.civilianName = name;
    p.districtId = districtId;
    p.position = pos;
    p.systemDisbeliefPercent = 10.0f;
    p.state = POTENTIAL_UNAWARE;
    m_potentials[id] = p;
    return id;
}

bool RedpillAwakeningSystem::TriggerMatrixAnomaly(uint32 potentialId, MatrixAnomalyType type)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    auto it = m_potentials.find(potentialId);
    if (it == m_potentials.end()) return false;

    CivilianPotential& p = it->second;
    switch (type) {
        case ANOMALY_DEJA_VU_BLACK_CAT:
            p.dejaVuCatOccurrences++;
            p.systemDisbeliefPercent = std::min(100.0f, p.systemDisbeliefPercent + 25.0f);
            break;
        case ANOMALY_INVERTED_RAINDROPS:
            p.systemDisbeliefPercent = std::min(100.0f, p.systemDisbeliefPercent + 15.0f);
            break;
        case ANOMALY_WIREFRAME_FLICKER:
            p.systemDisbeliefPercent = std::min(100.0f, p.systemDisbeliefPercent + 30.0f);
            break;
        case ANOMALY_BENT_SPOON:
            p.systemDisbeliefPercent = std::min(100.0f, p.systemDisbeliefPercent + 20.0f);
            break;
    }

    if (p.systemDisbeliefPercent >= 70.0f && p.state == POTENTIAL_UNAWARE) {
        p.state = POTENTIAL_AWAKENED;
    }
    return true;
}

void RedpillAwakeningSystem::ExposeCivilianToCombatEvent(float posX, float posZ, float intensity)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    for (auto& kv : m_potentials) {
        CivilianPotential& p = kv.second;
        float dx = p.position.x - posX;
        float dz = p.position.z - posZ;
        float distSq = dx * dx + dz * dz;

        if (distSq <= 2250000.0f) { // 1500 units
            p.systemDisbeliefPercent = std::min(100.0f, p.systemDisbeliefPercent + intensity * 0.25f);
            if (p.systemDisbeliefPercent >= 70.0f && p.state == POTENTIAL_UNAWARE) {
                p.state = POTENTIAL_AWAKENED;
            }
        }
    }
}

bool RedpillAwakeningSystem::BeginEscort(uint32 potentialId, uint32 playerGoId, uint32 targetHardlineId)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    auto it = m_potentials.find(potentialId);
    if (it == m_potentials.end()) return false;

    CivilianPotential& p = it->second;
    p.state = POTENTIAL_ESCORT_ACTIVE;
    p.assignedEscortPlayerGoId = playerGoId;
    p.targetHardlineId = targetHardlineId;

    if (Has3DWorldSupport()) {
        if (auto player = sObjMgr.getGOPtrSafe(playerGoId)) {
            LocationVector playerPos = player->getPosition();
            if (auto bot = sBotMgr.GetBotByGOID(p.npcGoId)) {
                bot->MoveTo((float)playerPos.x, (float)playerPos.y, (float)playerPos.z);
            }
            if (auto po = sObjMgr.getGOPtrSafe(p.npcGoId)) {
                po->sayChat("You're from outside... you're Zion! Lead me to the hardline!");
                po->Emote(1);
            }
        }

        // Trigger Agent intervention to stop the extraction!
        if (AgentPossessionManager::getSingletonPtr()) {
            sAgentPossessionMgr.TriggerEmergencyIntervention(
                LocationVector(p.position.x, p.position.y, p.position.z), 75.0f, playerGoId);
        }
    }

    return true;
}

bool RedpillAwakeningSystem::CheckHardlineExtraction(uint32 potentialId, float hardlineX, float hardlineZ)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    auto it = m_potentials.find(potentialId);
    if (it == m_potentials.end()) return false;

    CivilianPotential& p = it->second;
    float dx = p.position.x - hardlineX;
    float dz = p.position.z - hardlineZ;
    float dist = std::sqrt(dx * dx + dz * dz);

    if (dist <= 300.0f && p.state == POTENTIAL_ESCORT_ACTIVE) {
        p.state = POTENTIAL_EXTRACTED_SAFE;
        m_totalExtracted++;

        if (Has3DWorldSupport()) {
            if (auto po = sObjMgr.getGOPtrSafe(p.npcGoId)) {
                po->sayChat("The telephone is ringing... I see through the simulation. See you in Zion!");
                po->killPlayer(0, 0x280001C2);
            }
            if (auto player = sObjMgr.getGOPtrSafe(p.assignedEscortPlayerGoId)) {
                player->addInformation(25000); // 25,000 info bits reward
                player->addFactionReputation(50); // +50 Zion reputation
                player->addExp(5000); // 5000 XP
            }
        }

        return true;
    }
    return false;
}

uint32 RedpillAwakeningSystem::CreateSmugglingContract(const std::string& codeName, uint32 rewardInfo)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    uint32 id = m_nextContractId++;
    MobilAveSmugglingContract c;
    c.contractId = id;
    c.contrabandCodeName = codeName;
    c.rewardInfoCurrency = rewardInfo;
    c.isCompleted = false;
    c.destinationDistrict = "Slums";
    m_contracts.push_back(c);
    return id;
}

bool RedpillAwakeningSystem::CompleteSmugglingContract(uint32 contractId)
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    for (auto& c : m_contracts) {
        if (c.contractId == contractId && !c.isCompleted) {
            c.isCompleted = true;
            return true;
        }
    }
    return false;
}

const CivilianPotential* RedpillAwakeningSystem::GetPotential(uint32 potentialId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    auto it = m_potentials.find(potentialId);
    if (it != m_potentials.end()) return &it->second;
    return nullptr;
}

size_t RedpillAwakeningSystem::GetPotentialCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    return m_potentials.size();
}

size_t RedpillAwakeningSystem::GetAwakenedCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_awakeningMutex);
    size_t count = 0;
    for (const auto& kv : m_potentials) {
        if (kv.second.state == POTENTIAL_AWAKENED || kv.second.state == POTENTIAL_ESCORT_ACTIVE) {
            count++;
        }
    }
    return count;
}

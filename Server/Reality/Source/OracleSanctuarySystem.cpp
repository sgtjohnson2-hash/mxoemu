#include "OracleSanctuarySystem.h"
#include "OracleDialogueTree.h"
#include "PlayerObject.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "Log.h"
#include "Database/Database.h"
#include <cmath>
#include <boost/algorithm/string.hpp>

createFileSingleton(OracleSanctuarySystem);

OracleSanctuarySystem::OracleSanctuarySystem()
{
    Initialize();
}

OracleSanctuarySystem::~OracleSanctuarySystem()
{
}

void OracleSanctuarySystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    m_enclaves.clear();
    m_seraphTrials.clear();
    m_currentSkyboxState = SATI_SKY_RADIANT_SUNRISE;
    m_sunriseTriggerCount = 0;

    // 1. Downtown Tenement Kitchen (The Oracle & Seraph)
    SanctuaryEnclave kitchen;
    kitchen.enclaveName = "Tenement Kitchen";
    kitchen.district = "Downtown";
    kitchen.posX = 1820.0f;
    kitchen.posY = 45.0f;
    kitchen.posZ = -3150.0f;
    kitchen.radius = 25.0f;
    kitchen.requiresSeraphClearance = true;
    kitchen.hasSatiSunriseSkybox = false;
    kitchen.residentNpcId = 9300; // The Oracle
    kitchen.guardianNpcId = 9101; // Seraph
    kitchen.serenityDescription = "The scent of freshly baked cinnamon cookies fills the warm tenement apartment. Seraph guards the threshold.";
    m_enclaves.push_back(kitchen);

    // 2. Park East Bench (Sati & The Sunrise Horizon)
    SanctuaryEnclave park;
    park.enclaveName = "Park East Bench";
    park.district = "Park East";
    park.posX = -4750.0f;
    park.posY = 82.0f;
    park.posZ = -2180.0f;
    park.radius = 30.0f;
    park.requiresSeraphClearance = false;
    park.hasSatiSunriseSkybox = true;
    park.residentNpcId = 9252; // Sati
    park.guardianNpcId = 0;
    park.serenityDescription = "A quiet secluded bench beneath vibrant digital cherry blossoms. Sati watches the morning sky unfold.";
    m_enclaves.push_back(park);

    if (Log::getSingletonPtr())
    {
        sLog.outString("[OracleSanctuarySystem] Initialized %zu sanctuary enclaves (Tenement Kitchen, Park East Bench).", m_enclaves.size());
    }
}

void OracleSanctuarySystem::Reset()
{
    Initialize();
}

bool OracleSanctuarySystem::IsInSanctuary(float x, float y, float z, std::string& outSanctuaryName) const
{
    const SanctuaryEnclave* enclave = GetEnclaveAt(x, y, z);
    if (enclave)
    {
        outSanctuaryName = enclave->enclaveName;
        return true;
    }
    return false;
}

const SanctuaryEnclave* OracleSanctuarySystem::GetEnclaveAt(float x, float y, float z) const
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    for (const auto& enc : m_enclaves)
    {
        float dx = enc.posX - x;
        float dy = enc.posY - y;
        float dz = enc.posZ - z;
        float distSq = dx*dx + dy*dy + dz*dz;
        if (distSq <= (enc.radius * enc.radius))
        {
            return &enc;
        }
    }
    return nullptr;
}

bool OracleSanctuarySystem::CheckSeraphThreshold(uint32 characterId, float x, float y, float z) const
{
    EnsureTrialLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    const SanctuaryEnclave* enclave = GetEnclaveAt(x, y, z);
    if (!enclave || !enclave->requiresSeraphClearance) return true;

    // First: If operative has passed the martial arts Seraph Trial, clearance is permanently granted
    if (HasPassedSeraphTrial(characterId))
    {
        sLog.outString("[OracleSanctuarySystem] Seraph Clearance Granted for Character %u at (%f, %f, %f) -> Seraph Trial Passed.",
                       characterId, x, y, z);
        return true;
    }

    // Threshold check: Seraph evaluates purpose and peaceful intent (0.50f threshold)
    // Cynical / hostile operatives with faith valence < -0.50f are denied entry
    float faith = sOracleDialogue.GetFaithValence(characterId);
    float clearanceThreshold = 0.50f;

    if (faith < -clearanceThreshold)
    {
        sLog.outString("[OracleSanctuarySystem] Seraph Clearance Denied for Character %u at (%f, %f, %f) -> faith %.2f < -%.2f.",
                       characterId, x, y, z, faith, clearanceThreshold);
        return false;
    }

    sLog.outString("[OracleSanctuarySystem] Seraph Clearance Granted for Character %u at (%f, %f, %f) -> threshold %.2f (faith %.2f).",
                   characterId, x, y, z, clearanceThreshold, faith);
    return true;
}

bool OracleSanctuarySystem::StartSeraphTrial(uint32 characterId, PlayerObject* player, std::string& outMsg)
{
    EnsureTrialLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    auto& duel = m_seraphTrials[characterId];
    duel.characterId = characterId;
    duel.status = SERAPH_TRIAL_IN_PROGRESS;
    duel.maxHealth = 5000.0f;
    duel.currentHealth = duel.maxHealth;
    duel.interlocksCompleted = 0;
    duel.trialDurationSeconds = 0.0f;
    duel.clearanceGranted = false;

    if (player)
    {
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            "{c:FFB300}[Seraph Trial Commenced] NPC 9101 (Seraph) takes Wing Chun stance at the threshold.{/c}\n"
            "{c:FFFF88}\"You do not truly know someone until you fight them. Show me your purpose.\"{/c}"
        ));
    }

    outMsg = "Seraph Trial engaged. Reduce Seraph to 25% health or execute 3 successful combat interlocks.";
    sLog.outString("[OracleSanctuarySystem] Seraph Trial started for Character %u (HP: %.0f).", characterId, duel.currentHealth);
    SaveSeraphTrial(characterId);
    return true;
}

bool OracleSanctuarySystem::ProcessSeraphDuelHit(uint32 characterId, float damage, bool isInterlockCounter, std::string& outSeraphDialogue, bool& outYielded)
{
    EnsureTrialLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    auto it = m_seraphTrials.find(characterId);
    if (it == m_seraphTrials.end() || it->second.status != SERAPH_TRIAL_IN_PROGRESS)
    {
        outYielded = false;
        outSeraphDialogue = "No active trial in progress.";
        return false;
    }

    // Sanitize incoming damage against negative or abnormal values
    if (std::isnan(damage) || std::isinf(damage) || damage < 0.0f)
    {
        damage = 0.0f;
    }
    damage = std::clamp(damage, 0.0f, 50000.0f);

    auto& duel = it->second;
    duel.currentHealth = std::max<float>(0.0f, duel.currentHealth - damage);
    if (isInterlockCounter)
    {
        duel.interlocksCompleted++;
    }

    // Yield condition: Seraph yields at 25% health (1250 HP) or after 3 successful combat interlocks
    float yieldHpThreshold = duel.maxHealth * 0.25f; // 25% = 1250 HP
    if (duel.currentHealth <= yieldHpThreshold || duel.interlocksCompleted >= 3)
    {
        duel.status = SERAPH_TRIAL_PASSED;
        duel.clearanceGranted = true;
        duel.currentHealth = duel.maxHealth; // Seraph non-lethal restoration
        outYielded = true;
        outSeraphDialogue = "I apologize. I had to be sure. You are who you say you are. The Oracle will see you now.";

        sLog.outString("[OracleSanctuarySystem] Seraph yielded to Character %u (HP: %.0f, Interlocks: %u). Sanctum clearance granted.",
                       characterId, duel.currentHealth, duel.interlocksCompleted);
        SaveSeraphTrial(characterId);
        return true;
    }

    outYielded = false;
    if (isInterlockCounter)
    {
        outSeraphDialogue = "Seraph yields a step, absorbing the counter. 'Good. Balance and precision.' (Interlocks: " +
                            std::to_string(duel.interlocksCompleted) + "/3)";
    }
    else
    {
        outSeraphDialogue = "Seraph gracefully pivots through your guard. 'You fight with technique, but do you fight with purpose?' (Seraph HP: " +
                            std::to_string(static_cast<int>(duel.currentHealth)) + "/" + std::to_string(static_cast<int>(duel.maxHealth)) + ")";
    }
    return true;
}

bool OracleSanctuarySystem::HasPassedSeraphTrial(uint32 characterId) const
{
    EnsureTrialLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    auto it = m_seraphTrials.find(characterId);
    return (it != m_seraphTrials.end() && it->second.status == SERAPH_TRIAL_PASSED);
}

const SeraphDuelState* OracleSanctuarySystem::GetSeraphDuelState(uint32 characterId) const
{
    EnsureTrialLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    auto it = m_seraphTrials.find(characterId);
    return (it != m_seraphTrials.end()) ? &it->second : nullptr;
}

void OracleSanctuarySystem::ResetSeraphTrial(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    m_seraphTrials.erase(characterId);
    if (Database_Main != nullptr)
    {
        std::string sql = (format("DELETE FROM `oracle_seraph_trials` WHERE `character_id` = %1%;") % characterId).str();
        sDatabase.Execute(sql);
    }
}

bool OracleSanctuarySystem::SaveSeraphTrial(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    auto it = m_seraphTrials.find(characterId);
    if (it == m_seraphTrials.end()) return false;

    if (Database_Main == nullptr) return true;

    std::string sql = (format("REPLACE INTO `oracle_seraph_trials` (`character_id`, `trial_status`, `clearance_granted`, `completed_interlocks`, `passed_at`) "
                              "VALUES (%1%, %2%, %3%, %4%, %5%);")
                       % characterId
                       % static_cast<uint32>(it->second.status)
                       % (it->second.clearanceGranted ? 1 : 0)
                       % it->second.interlocksCompleted
                       % (it->second.status == SERAPH_TRIAL_PASSED ? "NOW()" : "NULL")).str();
    sDatabase.Execute(sql);
    return true;
}

bool OracleSanctuarySystem::LoadSeraphTrial(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    if (Database_Main == nullptr) return false;

    std::string sql = (format("SELECT `trial_status`, `clearance_granted`, `completed_interlocks` FROM `oracle_seraph_trials` WHERE `character_id` = %1%;") % characterId).str();
    QueryResult* res = sDatabase.Query(sql);
    if (!res) return false;

    Field* fields = res->Fetch();
    if (fields)
    {
        auto& duel = m_seraphTrials[characterId];
        duel.characterId = characterId;
        duel.status = static_cast<SeraphTrialStatus>(fields[0].GetUInt8());
        duel.clearanceGranted = (fields[1].GetUInt8() != 0);
        duel.interlocksCompleted = fields[2].GetUInt32();
        duel.maxHealth = 5000.0f;
        duel.currentHealth = (duel.status == SERAPH_TRIAL_PASSED) ? duel.maxHealth : 5000.0f;
        delete res;
        return true;
    }
    delete res;
    return false;
}

void OracleSanctuarySystem::EnsureTrialLoaded(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    if (m_seraphTrials.find(characterId) == m_seraphTrials.end())
    {
        const_cast<OracleSanctuarySystem*>(this)->LoadSeraphTrial(characterId);
    }
}

SatiSkyboxState OracleSanctuarySystem::GetCurrentSkyboxState() const
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    return m_currentSkyboxState;
}

void OracleSanctuarySystem::SetSkyboxState(SatiSkyboxState state)
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    m_currentSkyboxState = state;
}

SatiSkyboxPalette OracleSanctuarySystem::GetSkyboxPalette(SatiSkyboxState state) const
{
    SatiSkyboxPalette p;
    p.state = state;
    switch (state)
    {
        case SATI_SKY_ZION_MATRIX_DAWN:
            p.name = "Zion Matrix Green Dawn";
            p.primaryHex = "#00E676";
            p.accentHex = "#00FF44";
            p.ambientHex = "#003311";
            p.description = "Digital emerald Matrix dawn with gentle cascading code rain across the skyline.";
            break;
        case SATI_SKY_MACHINE_COBALT_HORIZON:
            p.name = "Machine Sterile Cobalt Horizon";
            p.primaryHex = "#00E5FF";
            p.accentHex = "#00B0FF";
            p.ambientHex = "#001A33";
            p.description = "Chilling cobalt blue and cyan geometric dawn reflecting optimal 01 systemic computation.";
            break;
        case SATI_SKY_MEROVINGIAN_VIOLET_DUSK:
            p.name = "Merovingian Corrupted Violet Dusk";
            p.primaryHex = "#BA68C8";
            p.accentHex = "#E040FB";
            p.ambientHex = "#1A0033";
            p.description = "Corrupted royal violet and magenta twilight with fractured cryptographic sparks.";
            break;
        case SATI_SKY_RADIANT_SUNRISE:
        default:
            p.name = "Sati's Radiant Sunrise";
            p.primaryHex = "#FFB300";
            p.accentHex = "#FF4081";
            p.ambientHex = "#FFE082";
            p.description = "A warm, painterly dawn blending gold, rose, violet, and peach. 'I made this sunrise for you.'";
            break;
    }
    return p;
}

void OracleSanctuarySystem::UpdateSkyboxFromShardMetrics(float contagionPercent, float machineControl, float merovingianCorruption)
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    if (contagionPercent >= 50.0f || merovingianCorruption >= 0.60f)
    {
        m_currentSkyboxState = SATI_SKY_MEROVINGIAN_VIOLET_DUSK;
    }
    else if (machineControl >= 0.60f)
    {
        m_currentSkyboxState = SATI_SKY_MACHINE_COBALT_HORIZON;
    }
    else if (contagionPercent < 15.0f && machineControl < 0.40f)
    {
        m_currentSkyboxState = SATI_SKY_RADIANT_SUNRISE;
    }
    else
    {
        m_currentSkyboxState = SATI_SKY_ZION_MATRIX_DAWN;
    }
}

bool OracleSanctuarySystem::TriggerSatiSunrise(uint32 characterId, PlayerObject* player)
{
    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    m_sunriseTriggerCount++;
    m_currentSkyboxState = SATI_SKY_RADIANT_SUNRISE;

    if (player)
    {
        auto pal = GetSkyboxPalette(SATI_SKY_RADIANT_SUNRISE);
        // Broadcast custom golden skybox packet & chat announcement
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            "{c:FFB300}[Sati's Sky] The digital horizon ignites in brilliant violet, gold, and amber. 'I made this sunrise for you.'{/c}"
        ));
    }

    sLog.outString("[OracleSanctuarySystem] Triggered Sati Sunrise skybox event for Character %u (Total triggers: %u).",
                   characterId, m_sunriseTriggerCount);
    return true;
}

bool OracleSanctuarySystem::TeleportToSanctuary(PlayerObject* player, const std::string& sanctuaryName)
{
    if (!player) return false;

    std::lock_guard<std::recursive_mutex> lock(m_sanctuaryMutex);
    for (const auto& enc : m_enclaves)
    {
        bool matches = sanctuaryName.empty() ||
                       boost::iequals(enc.enclaveName, sanctuaryName) ||
                       boost::icontains(enc.enclaveName, sanctuaryName) ||
                       (boost::iequals(sanctuaryName, "kitchen") && enc.enclaveName == "Tenement Kitchen") ||
                       (boost::iequals(sanctuaryName, "tenement") && enc.enclaveName == "Tenement Kitchen") ||
                       (boost::iequals(sanctuaryName, "park") && enc.enclaveName == "Park East Bench") ||
                       (boost::iequals(sanctuaryName, "bench") && enc.enclaveName == "Park East Bench") ||
                       (boost::iequals(sanctuaryName, "sati") && enc.enclaveName == "Park East Bench");

        if (matches)
        {
            uint32 charId = static_cast<uint32>(player->getCharId());
            if (enc.requiresSeraphClearance && !CheckSeraphThreshold(charId, enc.posX, enc.posY, enc.posZ))
            {
                player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FF5555}[Seraph] \"I protect that which matters most. Your intent is clouded by hostility. Prove your purpose before entering.\"{/c}"
                ));
                return false;
            }

            LocationVector dest(enc.posX, enc.posY, enc.posZ);
            player->setPosition(dest);
            player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:00FF88}[Sanctuary Transmit] Arrived at %1% (%2%). %3%{/c}")
                 % enc.enclaveName % enc.district % enc.serenityDescription).str()
            ));

            if (enc.hasSatiSunriseSkybox)
            {
                TriggerSatiSunrise(charId, player);
            }
            return true;
        }
    }
    return false;
}

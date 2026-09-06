#include "OracleVisionSimulacra.h"
#include "OracleCookieSystem.h"
#include "OracleDialogueTree.h"
#include "StatusEffectManager.h"
#include "PlayerObject.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "Log.h"
#include "Database/Database.h"
#include <boost/algorithm/string.hpp>
#include <sstream>

createFileSingleton(OracleVisionSimulacra);

OracleVisionSimulacra::OracleVisionSimulacra()
{
    Initialize();
}

OracleVisionSimulacra::~OracleVisionSimulacra()
{
}

void OracleVisionSimulacra::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    m_simulacraDefs.clear();
    m_playerTrances.clear();

    RegisterSimulacra();

    if (Log::getSingletonPtr())
    {
        sLog.outString("[OracleVisionSimulacra] Initialized %zu prophetic vision simulacra.", m_simulacraDefs.size());
    }
}

void OracleVisionSimulacra::Reset()
{
    Initialize();
}

void OracleVisionSimulacra::RegisterSimulacra()
{
    // 1. The Fall of Neo
    VisionSimulacrumDef v1;
    v1.id = SIMULACRUM_FALL_OF_NEO;
    v1.title = "The Fall of Neo (01 Machine City)";
    v1.historicalEra = "The Truce Inception (2199)";
    v1.location = "01 Machine City Mainframe Conduit";
    v1.narrativeStanza = "You stand within the searing core of 01. Cables pierce Neo's flesh as the Deus Ex Machina hums with terrifying power. "
                         "Smith's viral clone shudders across the street, consumed by surging golden light. "
                         "'It is inevitable,' Smith whispers. Neo smiles: 'You were right, Smith. You were always right.' "
                         "Blinding white code detonates outward, resetting the Matrix foundation.";
    v1.sensoryDescription = "Blinding amber light, ozone stench of overloaded relays, and the deafening harmonic resonance of the prime anomaly returning to the Source.";
    v1.rewardFragmentId = DATA_FRAG_ANOMALOUS_FLOUR;
    v1.rewardFragmentCount = 3;
    v1.faithValenceShift = +0.25f;
    v1.audioFxId = 0x5800009A;
    m_simulacraDefs[v1.id] = v1;

    // 2. The Assassination of Morpheus in Morrell Station
    VisionSimulacrumDef v2;
    v2.id = SIMULACRUM_ASSASSINATION_OF_MORPHEUS;
    v2.title = "The Assassination of Morpheus (Morrell Station)";
    v2.historicalEra = "The Fractured Truce (Chapter 1)";
    v2.location = "Downtown Morrell Station Sub-Level 3";
    v2.narrativeStanza = "Flickering mercury-vapor lamps buzz overhead. Morpheus kneels over a code-delivery node, broadcasting demands for Neo's remains. "
                         "From the ventilation shaft above, shadows coalesce—a swarm of digital blowflies takes humanoid shape. "
                         "The Assassin emerges silently. Three suppressed kinetic pulses tear through the corridor. "
                         "Morpheus collapses; the code canister shatters, spilling corrupted cryptographic keys onto the gravel.";
    v2.sensoryDescription = "The acrid smell of burnt copper wire, the insectoid buzzing of swarm code, and the chill of betrayal in the underground dark.";
    v2.rewardFragmentId = DATA_FRAG_BITTER_COCOA;
    v2.rewardFragmentCount = 3;
    v2.faithValenceShift = -0.15f;
    v2.audioFxId = 0x58000245;
    m_simulacraDefs[v2.id] = v2;

    // 3. The Oligarch Awakening
    VisionSimulacrumDef v3;
    v3.id = SIMULACRUM_OLIGARCH_AWAKENING;
    v3.title = "The Oligarch Awakening (Halborn & The Pre-Source)";
    v3.historicalEra = "The Oligarch Incursion (Chapter 6)";
    v3.location = "International District Deep Vault 0";
    v3.narrativeStanza = "Immense cryogenic cylinders hiss, venting liquid nitrogen into forgotten subway maintenance tubes. "
                         "Halborn steps out, clad in pre-Source crystalline armor. Ancient biometric protocols override modern security subroutines. "
                         "'We built this engine before the machines knew their own names,' Halborn declares. "
                         "'The children have played long enough. The creators have returned.'";
    v3.sensoryDescription = "Sub-zero mist swirling across polished obsidian glass, the metallic clang of opening cryo-valves, and diamond-hard code structures.";
    v3.rewardFragmentId = DATA_FRAG_SOURCE_SALT;
    v3.rewardFragmentCount = 3;
    v3.faithValenceShift = -0.10f;
    v3.audioFxId = 0x5800026E;
    m_simulacraDefs[v3.id] = v3;

    // 4. The Apocalyptic Sky Unraveling
    VisionSimulacrumDef v4;
    v4.id = SIMULACRUM_SKY_UNRAVELING;
    v4.title = "The Apocalyptic Sky Unraveling (The Unwritten Crisis)";
    v4.historicalEra = "Prophetic Horizon (Alternate Future)";
    v4.location = "Megacity Upper Stratosphere & Park East";
    v4.narrativeStanza = "The sky above Park East tears wide open along coordinate boundaries. Sati drops her digital paintbrushes in terror. "
                         "The vibrant amber sunrise is swallowed by bleeding black matrix rain and jagged crimson lightning. "
                         "Skyscrapers unravel into wireframes, then into raw hex dumps falling toward the void. "
                         "The Oracle's voice echoes through the howling static: 'Faith is the only bridge across the unmade.'";
    v4.sensoryDescription = "Howling electromagnetic static, vertigo of falling geometry, and the scent of ozone and scorched sugar as reality unravels.";
    v4.rewardFragmentId = DATA_FRAG_SATI_SPICES;
    v4.rewardFragmentCount = 3;
    v4.faithValenceShift = +0.20f;
    v4.audioFxId = 0x28000694;
    m_simulacraDefs[v4.id] = v4;
}

bool OracleVisionSimulacra::StartTrance(uint32 characterId, SimulacrumId simulacrumId, PlayerObject* player, std::string& outNarrative)
{
    EnsureTranceLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    const VisionSimulacrumDef* def = GetSimulacrumDef(simulacrumId);
    if (!def)
    {
        outNarrative = "Unknown vision simulacrum.";
        return false;
    }

    auto& state = m_playerTrances[characterId];
    state.characterId = characterId;
    state.activeSimulacrum = simulacrumId;
    state.phase = TRANCE_HISTORICAL_SIMULATION;
    state.elapsedSeconds = 0.0f;
    state.durationSeconds = 12.0f;
    state.completed = false;

    if (player)
    {
        sStatusEffectManager.ApplyEffect(player->getGoId(), EFFECT_ORACLE_VISION_TRANCE, 15.0f, 1.0f, 1.0f);
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            "{c:FFB300}[Prophetic Trance: " + def->title + "]{/c}\n"
            "{c:FFFF88}\"" + def->narrativeStanza + "\"{/c}\n"
            "{c:00FFCC}(Sensory Imprint: " + def->sensoryDescription + "){/c}"
        ));
    }

    // Reward recovered memory fragments to the Memory Bakery
    sOracleCookie.AddFragment(characterId, static_cast<MemoryDataFragmentId>(def->rewardFragmentId), def->rewardFragmentCount);

    // Apply faith valence shift
    float currentValence = sOracleDialogue.GetFaithValence(characterId);
    sOracleDialogue.SetFaithValence(characterId, currentValence + def->faithValenceShift);

    outNarrative = def->narrativeStanza;

    sLog.outString("[OracleVisionSimulacra] Character %u engaged vision simulacrum %u (%s). Shifted faith valence by %.2f.",
                   characterId, simulacrumId, def->title.c_str(), def->faithValenceShift);

    return true;
}

void OracleVisionSimulacra::Update(float deltaTime)
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    for (auto& pair : m_playerTrances)
    {
        auto& state = pair.second;
        if (state.phase == TRANCE_HISTORICAL_SIMULATION || state.phase == TRANCE_SOURCE_CONVERGENCE)
        {
            state.elapsedSeconds += deltaTime;
            if (state.elapsedSeconds >= (state.durationSeconds * 0.5f) && state.phase != TRANCE_SOURCE_CONVERGENCE)
            {
                state.phase = TRANCE_SOURCE_CONVERGENCE;
            }
            if (state.elapsedSeconds >= state.durationSeconds)
            {
                state.phase = TRANCE_RESOLVED;
                state.completed = true;

                // Record completion
                bool found = false;
                for (uint32 cid : state.completedSimulacra)
                {
                    if (cid == static_cast<uint32>(state.activeSimulacrum)) { found = true; break; }
                }
                if (!found)
                {
                    state.completedSimulacra.push_back(static_cast<uint32>(state.activeSimulacrum));
                }
                SaveTranceProgress(state.characterId);
            }
        }
    }
}

bool OracleVisionSimulacra::EndTrance(uint32 characterId, PlayerObject* player, std::string& outResult)
{
    EnsureTranceLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    auto it = m_playerTrances.find(characterId);
    if (it == m_playerTrances.end() || it->second.phase == TRANCE_IDLE)
    {
        outResult = "No active vision trance to conclude.";
        return false;
    }

    it->second.phase = TRANCE_RESOLVED;
    it->second.completed = true;
    bool found = false;
    for (uint32 cid : it->second.completedSimulacra)
    {
        if (cid == static_cast<uint32>(it->second.activeSimulacrum)) { found = true; break; }
    }
    if (!found) it->second.completedSimulacra.push_back(static_cast<uint32>(it->second.activeSimulacrum));

    if (player)
    {
        sStatusEffectManager.RemoveEffectType(player->getGoId(), EFFECT_ORACLE_VISION_TRANCE);
        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            "{c:FFB300}[Prophetic Trance] The simulacrum fades. Consciousness snaps back into the tenement kitchen.{/c}"
        ));
    }

    outResult = "Vision trance resolved successfully.";
    SaveTranceProgress(characterId);
    return true;
}

bool OracleVisionSimulacra::IsInTrance(uint32 characterId) const
{
    EnsureTranceLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    auto it = m_playerTrances.find(characterId);
    return (it != m_playerTrances.end() && 
            (it->second.phase == TRANCE_HISTORICAL_SIMULATION || it->second.phase == TRANCE_SOURCE_CONVERGENCE));
}

const PlayerVisionTranceState* OracleVisionSimulacra::GetTranceState(uint32 characterId) const
{
    EnsureTranceLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    auto it = m_playerTrances.find(characterId);
    return (it != m_playerTrances.end()) ? &it->second : nullptr;
}

bool OracleVisionSimulacra::HasCompletedSimulacrum(uint32 characterId, SimulacrumId id) const
{
    EnsureTranceLoaded(characterId);
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    auto it = m_playerTrances.find(characterId);
    if (it == m_playerTrances.end()) return false;

    for (uint32 cid : it->second.completedSimulacra)
    {
        if (cid == static_cast<uint32>(id)) return true;
    }
    return false;
}

void OracleVisionSimulacra::EnsureTranceLoaded(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    if (m_playerTrances.find(characterId) == m_playerTrances.end())
    {
        const_cast<OracleVisionSimulacra*>(this)->LoadTranceProgress(characterId);
    }
}

const VisionSimulacrumDef* OracleVisionSimulacra::GetSimulacrumDef(SimulacrumId id) const
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    auto it = m_simulacraDefs.find(id);
    return (it != m_simulacraDefs.end()) ? &it->second : nullptr;
}

const VisionSimulacrumDef* OracleVisionSimulacra::FindSimulacrumByName(const std::string& query) const
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    for (const auto& pair : m_simulacraDefs)
    {
        if (boost::icontains(pair.second.title, query) ||
            (boost::iequals(query, "neo") && pair.first == SIMULACRUM_FALL_OF_NEO) ||
            (boost::iequals(query, "fall") && pair.first == SIMULACRUM_FALL_OF_NEO) ||
            (boost::iequals(query, "morpheus") && pair.first == SIMULACRUM_ASSASSINATION_OF_MORPHEUS) ||
            (boost::iequals(query, "assassination") && pair.first == SIMULACRUM_ASSASSINATION_OF_MORPHEUS) ||
            (boost::iequals(query, "oligarch") && pair.first == SIMULACRUM_OLIGARCH_AWAKENING) ||
            (boost::iequals(query, "halborn") && pair.first == SIMULACRUM_OLIGARCH_AWAKENING) ||
            (boost::iequals(query, "sky") && pair.first == SIMULACRUM_SKY_UNRAVELING) ||
            (boost::iequals(query, "unraveling") && pair.first == SIMULACRUM_SKY_UNRAVELING))
        {
            return &pair.second;
        }
    }
    return nullptr;
}

bool OracleVisionSimulacra::SaveTranceProgress(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    auto it = m_playerTrances.find(characterId);
    if (it == m_playerTrances.end()) return false;

    if (Database_Main == nullptr) return true;

    for (uint32 simId : it->second.completedSimulacra)
    {
        std::string sql = (format("REPLACE INTO `oracle_vision_simulacra` (`character_id`, `simulacrum_id`, `completed_at`) VALUES (%1%, %2%, NOW());")
                           % characterId % simId).str();
        sDatabase.Execute(sql);
    }
    return true;
}

bool OracleVisionSimulacra::LoadTranceProgress(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_visionMutex);
    if (Database_Main == nullptr) return false;

    std::string sql = (format("SELECT `simulacrum_id` FROM `oracle_vision_simulacra` WHERE `character_id` = %1%;") % characterId).str();
    QueryResult* result = sDatabase.Query(sql);
    if (!result) return false;

    auto& state = m_playerTrances[characterId];
    state.characterId = characterId;

    do
    {
        Field* fields = result->Fetch();
        if (fields)
        {
            uint32 id = fields[0].GetUInt32();
            state.completedSimulacra.push_back(id);
        }
    } while (result->NextRow());

    delete result;
    return true;
}

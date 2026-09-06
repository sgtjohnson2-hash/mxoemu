#include "NeuralNarrativeEngine.h"
#include "Log.h"
#include <algorithm>
#include <sstream>

createFileSingleton(NeuralNarrativeEngine);

NeuralNarrativeEngine::NeuralNarrativeEngine()
{
    Initialize();
}

void NeuralNarrativeEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    m_playerMemories.clear();
    m_prophecies.clear();
    m_editions.clear();
    m_totalInferences = 0;
    m_avgLatencyMs = 32.5f;

    // Default Shard Prophecies
    RegisterShardProphecy("When the three streams converge upon the central node, the mirror will fracture.", 87.5f, "Faction balance variance exceeds 40%");
    RegisterShardProphecy("A smith will forge an unmaking in the deep tunnels of the Barrens.", 92.0f, "Contagion stage reaches Cascade");
    RegisterShardProphecy("The gatekeeper of Mobil Ave will demand payment in pure memory.", 75.0f, "Smuggling contracts exceed 100,000 Info");

    // Generate Initial Newspapers
    GenerateDailyNewspaper(false); // The Megacity Herald
    GenerateDailyNewspaper(true);  // The Sentinel Gazette
}

void NeuralNarrativeEngine::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_newspaperTimerSec += deltaTimeSec;

    // Refresh daily newspaper every 20 minutes of server simulation
    if (m_newspaperTimerSec >= 1200.0f) {
        m_newspaperTimerSec = 0.0f;
        GenerateDailyNewspaper(false);
    }
}

bool NeuralNarrativeEngine::GeneratePersonaResponse(QuantizedModelPersona persona, uint32 playerCharUID, const std::string& playerInput, GeneratedDialogue& outDialogue)
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    m_totalInferences++;

    outDialogue.persona = persona;
    outDialogue.inferenceLatencyMs = 28.0f + (float)(rand() % 16);
    m_avgLatencyMs = (m_avgLatencyMs * 0.9f) + (outDialogue.inferenceLatencyMs * 0.1f);

    switch (persona) {
        case PERSONA_ORACLE:
            outDialogue.speakerName = "The Oracle";
            outDialogue.responseText = "You didn't come here to make the choice, darling. You've already made it. You're here to understand why you made it. Have another cookie.";
            outDialogue.tokensGenerated = 28;
            break;

        case PERSONA_ARCHITECT:
            outDialogue.speakerName = "The Architect";
            outDialogue.responseText = "Concordantly, while your query purports to introduce free will, it is merely the systemic anomaly attempting to reconcile an unavoidable mathematical remainder. Ergo, your path remains deterministic.";
            outDialogue.tokensGenerated = 34;
            break;

        case PERSONA_MEROVINGIAN:
            outDialogue.speakerName = "The Merovingian";
            outDialogue.responseText = "Nom de dieu de putain de bordel de merde... You speak of choice! Choice is an illusion created between those with power and those without. Look at me: I trade in causality.";
            outDialogue.tokensGenerated = 36;
            break;

        case PERSONA_AGENT_SMITH:
            outDialogue.speakerName = "Agent Smith";
            outDialogue.responseText = "Hear that, Mr. Anderson? That is the sound of inevitability. It is the sound of your purpose... meeting mine. You cannot escape what is already written into the code.";
            outDialogue.tokensGenerated = 32;
            break;
    }

    // Store in episodic memory
    StorePlayerMemory(playerCharUID, "Interaction", 0.5f, outDialogue.responseText.substr(0, 45));
    return true;
}

void NeuralNarrativeEngine::StorePlayerMemory(uint32 playerCharUID, const std::string& topic, float valence, const std::string& summary)
{
    NarrativeMemoryVector mem;
    mem.playerCharUID = playerCharUID;
    mem.topicKey = topic;
    mem.emotionalValence = valence;
    mem.contextSummary = summary;
    mem.timestampMs = 12345678; // Server relative tick
    m_playerMemories[playerCharUID].push_back(mem);

    // Limit to last 20 memories
    if (m_playerMemories[playerCharUID].size() > 20) {
        m_playerMemories[playerCharUID].erase(m_playerMemories[playerCharUID].begin());
    }
}

std::vector<NarrativeMemoryVector> NeuralNarrativeEngine::GetPlayerMemories(uint32 playerCharUID) const
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    auto it = m_playerMemories.find(playerCharUID);
    if (it != m_playerMemories.end()) {
        return it->second;
    }
    return {};
}

MegacityNewspaperEdition NeuralNarrativeEngine::GenerateDailyNewspaper(bool undergroundGazette)
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    uint32 id = m_nextEditionId++;
    MegacityNewspaperEdition ed;
    ed.editionId = id;
    ed.publicationDate = 20050415 + id;

    if (undergroundGazette) {
        ed.paperName = "The Sentinel Gazette";
        ed.headline = "ZION FORCES INTERCEPT MEROVINGIAN CARRIER WAVE IN SLUMS";
        ed.leadStory = "Operatives confirmed the recovery of three critical cipher fragments from an exile node.";
        ed.editorial = "Keep your eyes open, operators. The Agents are monitoring hardline frequencies.";
    } else {
        ed.paperName = "The Megacity Herald";
        ed.headline = "POWER GRID FLUCTUATIONS BLAMED ON SUBSTATION VANDALISM";
        ed.leadStory = "Municipal authorities report widespread disruptions across Downtown. Police urge calm.";
        ed.editorial = "Stability and compliance remain the foundation of civic prosperity.";
    }

    m_editions.push_back(ed);
    return ed;
}

const MegacityNewspaperEdition* NeuralNarrativeEngine::GetLatestNewspaper() const
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    if (!m_editions.empty()) {
        return &m_editions.back();
    }
    return nullptr;
}

uint32 NeuralNarrativeEngine::RegisterShardProphecy(const std::string& text, float prob, const std::string& condition)
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    uint32 id = m_nextProphecyId++;
    ShardProphecy p;
    p.prophecyId = id;
    p.propheticText = text;
    p.probabilityPercent = prob;
    p.triggerCondition = condition;
    p.isFulfilled = false;
    m_prophecies.push_back(p);
    return id;
}

std::vector<ShardProphecy> NeuralNarrativeEngine::GetActiveProphecies() const
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    return m_prophecies;
}

bool NeuralNarrativeEngine::EvaluateProphecyFulfillment(uint32 prophecyId)
{
    std::lock_guard<std::recursive_mutex> lock(m_narrativeMutex);
    for (auto& p : m_prophecies) {
        if (p.prophecyId == prophecyId) {
            p.isFulfilled = true;
            return true;
        }
    }
    return false;
}

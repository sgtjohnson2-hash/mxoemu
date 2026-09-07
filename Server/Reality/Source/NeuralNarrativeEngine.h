#ifndef MXOEMU_NEURAL_NARRATIVE_ENGINE_H
#define MXOEMU_NEURAL_NARRATIVE_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

enum QuantizedModelPersona
{
    PERSONA_ORACLE = 0,
    PERSONA_ARCHITECT = 1,
    PERSONA_MEROVINGIAN = 2,
    PERSONA_AGENT_SMITH = 3
};

struct NarrativeMemoryVector
{
    uint32 playerCharUID{0};
    std::string topicKey{"Choice"};
    float emotionalValence{0.0f}; // -1.0 (hostile) to +1.0 (friendly)
    std::string contextSummary{"Player refused to choose between two doors"};
    uint32 timestampMs{0};
};

struct GeneratedDialogue
{
    QuantizedModelPersona persona{PERSONA_ORACLE};
    std::string speakerName{"The Oracle"};
    std::string responseText{"Have a cookie. I promise, by the time you're done, you'll feel right as rain."};
    float inferenceLatencyMs{32.4f};
    uint32 tokensGenerated{24};
};

struct MegacityNewspaperEdition
{
    uint32 editionId{1};
    std::string paperName{"The Megacity Herald"};
    std::string headline{"POWER GRID FLUCTUATIONS BLAMED ON SUBSTATION VANDALISM"};
    std::string leadStory{"Municipal authorities report widespread disruptions across Downtown..."};
    std::string editorial{"Order must be maintained at all costs."};
    uint32 publicationDate{20050415};
};

enum ProphecyLifecycle
{
    PROPHECY_STATE_REGISTERED = 0,
    PROPHECY_STATE_CIRCULATING = 1,
    PROPHECY_STATE_CONVERGING = 2,
    PROPHECY_STATE_FULFILLED = 3
};

struct ShardProphecy
{
    uint32 prophecyId{1};
    std::string propheticText{"When the three streams converge upon the central node, the mirror will fracture."};
    float probabilityPercent{87.5f};
    std::string triggerCondition{"Faction balance variance exceeds 40%"};
    ProphecyLifecycle lifecycleState{PROPHECY_STATE_REGISTERED};
    bool isFulfilled{false};
};

class NeuralNarrativeEngine : public Singleton<NeuralNarrativeEngine>
{
public:
    NeuralNarrativeEngine();
    ~NeuralNarrativeEngine() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Contextual LLM Dialogue Inference
    bool GeneratePersonaResponse(QuantizedModelPersona persona, uint32 playerCharUID, const std::string& playerInput, GeneratedDialogue& outDialogue);
    void StorePlayerMemory(uint32 playerCharUID, const std::string& topic, float valence, const std::string& summary);
    std::vector<NarrativeMemoryVector> GetPlayerMemories(uint32 playerCharUID) const;

    // Megacity Newspaper Generation
    MegacityNewspaperEdition GenerateDailyNewspaper(bool undergroundGazette);
    const MegacityNewspaperEdition* GetLatestNewspaper() const;

    // Dynamic Shard Prophecy Engine
    uint32 RegisterShardProphecy(const std::string& text, float prob, const std::string& condition);
    std::vector<ShardProphecy> GetActiveProphecies() const;
    bool EvaluateProphecyFulfillment(uint32 prophecyId);
    float CalculateShardInstability(float contagion, float factionVariance, float threatHeatmap, float crisisCooldown) const;

    // Performance Metrics
    float GetAverageInferenceLatencyMs() const { return m_avgLatencyMs; }
    uint32 GetTotalInferencesRun() const { return m_totalInferences; }

private:
    mutable std::recursive_mutex m_narrativeMutex;
    std::map<uint32, std::vector<NarrativeMemoryVector>> m_playerMemories;
    std::vector<ShardProphecy> m_prophecies;
    std::vector<MegacityNewspaperEdition> m_editions;

    uint32 m_nextProphecyId{1};
    uint32 m_nextEditionId{1};
    uint32 m_totalInferences{0};
    float m_avgLatencyMs{34.2f};
    float m_newspaperTimerSec{0.0f};
};

#define sNeuralNarrativeEngine NeuralNarrativeEngine::getSingleton()

#endif // MXOEMU_NEURAL_NARRATIVE_ENGINE_H

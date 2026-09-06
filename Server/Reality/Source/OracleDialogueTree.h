#ifndef MXOEMU_ORACLE_DIALOGUE_TREE_H
#define MXOEMU_ORACLE_DIALOGUE_TREE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

enum OracleDialogueNodeId
{
    ORACLE_NODE_ROOT = 1,
    ORACLE_NODE_KITCHEN_INTRO = 2,
    ORACLE_NODE_COOKIE_OFFER = 3,
    ORACLE_NODE_COOKIE_ACCEPT = 4,
    ORACLE_NODE_COOKIE_REFUSE = 5,
    ORACLE_NODE_PROPHECY_GLANCE = 6,
    ORACLE_NODE_SANCTUARY_REFUGE = 7,
    ORACLE_NODE_SATI_DAWN = 8,

    // Change of Shell (Gloria Foster -> Mary Alice)
    ORACLE_NODE_SHELL_INQUIRY = 9,
    ORACLE_NODE_SHELL_SACRIFICE = 10,
    ORACLE_NODE_SHELL_IDENTITY = 11,
    ORACLE_NODE_SHELL_ACCEPTANCE = 12,

    // Sati's Parents (Rama-Kandra & Kamala) & Love as a Program
    ORACLE_NODE_SATI_PARENTS_INTRO = 13,
    ORACLE_NODE_LOVE_DEFINITION = 14,
    ORACLE_NODE_SATI_PURPOSELESSNESS = 15,
    ORACLE_NODE_LOVE_RESOLVE = 16,

    // White Room Debate against the Architect
    ORACLE_NODE_WHITE_ROOM_DEBATE = 17,
    ORACLE_NODE_PARADISE_FAILURE = 18,
    ORACLE_NODE_ARCHITECT_TRUCE = 19,
    ORACLE_NODE_WHITE_ROOM_CONCLUSION = 20,

    // Deep Faction-Specific Paths
    // Zionite Doubt
    ORACLE_NODE_ZION_DOUBT = 21,
    ORACLE_NODE_ZION_PURPOSE = 22,
    ORACLE_NODE_ZION_FUTURE = 23,

    // Machine Determinism
    ORACLE_NODE_MACHINE_DETERMINISM = 24,
    ORACLE_NODE_MACHINE_SYNTHESIS = 25,

    // Merovingian Cynic Bargains
    ORACLE_NODE_MEROVINGIAN_CYNIC = 26,
    ORACLE_NODE_CAUSALITY_FALLACY = 27,
    ORACLE_NODE_EXILE_PAST = 28,

    // Departure & Farewell
    ORACLE_NODE_FAREWELL = 29
};

enum OracleCookieChoice
{
    COOKIE_NONE = 0,
    COOKIE_DECISION_ACCEPTED = 1, // Grants EFFECT_ORACLE_INTUITION and unlocks prophetic premonitions
    COOKIE_DECISION_REFUSED = 2   // Locks tactical premonition cues and branches into deterministic dialogue
};

struct OracleDialogueOption
{
    uint32 optionId{0};
    std::string playerChoiceText;
    uint32 targetNodeId{0};
    float deltaFaithValence{0.0f}; // shift in faith vs determinism [-1.0 to +1.0]
    OracleCookieChoice cookieChoice{COOKIE_NONE};
};

struct OracleDialogueNode
{
    uint32 nodeId{0};
    std::string emotionalTone; // "Maternal", "Prophetic", "Cryptic", "Warm"
    uint32 audioFxId{0};        // Authentic voice line from fxlistreal.txt
    std::string oracleSpeech;
    std::vector<OracleDialogueOption> options;
    bool isCookieDecisionNode{false};
    bool isTerminal{false};
};

struct OracleEncounterState
{
    uint32 characterId{0};
    uint32 currentNodeId{1};
    bool encounterActive{false};
    OracleCookieChoice cookieDecision{COOKIE_NONE};
    float faithValence{0.0f}; // -1.0 (cynical determinism) to +1.0 (intuitive faith)
    uint32 propheciesWitnessed{0};
    uint32 consultationCount{0};
    std::vector<uint32> dialoguePathHistory;
    std::string psychologicalSummary;
};

class OracleDialogueTree : public Singleton<OracleDialogueTree>
{
public:
    OracleDialogueTree();
    ~OracleDialogueTree();

    void Initialize();
    void Reset();

    // Audio FX constants from fxlistreal.txt
    static constexpr uint32 AUDIO_FX_COOKIE_BITE    = 0x28000694; // cinematic 1.1_cookie_oraclehand_bitten
    static constexpr uint32 AUDIO_FX_GREETING_ALLY  = 0x5800022B; // Well look what the cat dragged in
    static constexpr uint32 AUDIO_FX_GREETING_ENEMY = 0x58000245; // Well look who slunk in the back door
    static constexpr uint32 AUDIO_FX_COOKIE_OFFER   = 0x5800024B; // Would you like a cookie
    static constexpr uint32 AUDIO_FX_COOKIE_ACCEPT  = 0x5800024A; // How about a cookie
    static constexpr uint32 AUDIO_FX_QUOTE_TRUCE    = 0x5800009A; // Truce hasn't made this safe
    static constexpr uint32 AUDIO_FX_QUOTE_FUTURE   = 0x5800026E; // Are you afraid of the future

    // Encounter Management
    bool StartEncounter(uint32 characterId);
    bool HasActiveEncounter(uint32 characterId) const;
    const OracleDialogueNode* GetCurrentNode(uint32 characterId) const;
    bool SelectDialogueOption(uint32 characterId, uint32 optionId, std::string& outOracleReply, uint32& outAudioFx);

    // Cookie Decision Engine
    bool ExecuteCookieChoice(uint32 characterId, OracleCookieChoice choice, std::string& outConsequence, bool applyValenceShift = true, uint32 targetGoId = 0);

    // Operative Faith & Psychological Imprint
    float GetFaithValence(uint32 characterId) const;
    void SetFaithValence(uint32 characterId, float valence);
    const OracleEncounterState* GetEncounterState(uint32 characterId) const;

    // Persistence (MariaDB oracle_player_choices)
    bool SavePlayerChoice(uint32 characterId);
    bool LoadPlayerChoice(uint32 characterId);

    // Node count
    size_t GetTotalDialogueNodes() const;

private:
    void BuildDialogueTree();

    mutable std::recursive_mutex m_oracleMutex;
    std::map<uint32, OracleDialogueNode> m_nodes;
    std::map<uint32, OracleEncounterState> m_activeEncounters;
};

#define sOracleDialogue OracleDialogueTree::getSingleton()

#endif // MXOEMU_ORACLE_DIALOGUE_TREE_H

#ifndef MXOEMU_ARCHITECT_DIALOGUE_TREE_H
#define MXOEMU_ARCHITECT_DIALOGUE_TREE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

enum ArchitectDoorChoice
{
    CHOICE_NONE = 0,
    CHOICE_DOOR_RIGHT_REBOOT = 1,   // The Machine Choice: Return to Source, Zion reboot covenant
    CHOICE_DOOR_LEFT_REBELLION = 2  // The Human Choice: Return to Matrix, save Trinity, defy the cycle
};

struct DialogueOption
{
    uint32 optionId{0};
    std::string playerChoiceText;
    uint32 targetNodeId{0};
    float minNotoriety{0.0f};
    float minZionStanding{0.0f};
    float minMachineStanding{0.0f};
    ArchitectDoorChoice doorChoice{CHOICE_NONE};
};

struct DialogueNode
{
    uint32 nodeId{0};
    std::string philosophicalMarker; // "Concordantly", "Vis-a-vis", "Ergo", "As you well know"
    std::string architectSpeech;
    std::vector<DialogueOption> options;
    bool isTerminalChoice{false};
};

struct CRTMonitorWallState
{
    uint32 totalMonitors{512};
    uint32 activeMonitors{512};
    float pulseRateBpm{72.0f};
    float emotionalTension{0.15f}; // 0.0 to 1.0
    std::string activeVideoFeedContent{"Live RSI Reflection"};
    uint32 synchronizedFrames{0};
};

struct EncounterProgress
{
    uint32 characterId{0};
    uint32 currentNodeId{1};
    bool encounterActive{false};
    ArchitectDoorChoice finalChoice{CHOICE_NONE};
    std::vector<uint32> pathHistory;
    std::string outcomeSummary;
};

class ArchitectDialogueTree : public Singleton<ArchitectDialogueTree>
{
public:
    ArchitectDialogueTree();
    ~ArchitectDialogueTree();

    void Initialize();
    void Reset();

    // Chamber Telemetry & CRT Monitors
    CRTMonitorWallState GetMonitorState() const;
    void UpdateMonitorTelemetry(float playerHeartRateBpm, float combatTension, const std::string& feedContent);

    // Dialogue Navigation
    bool StartEncounter(uint32 characterId);
    const DialogueNode* GetCurrentNode(uint32 characterId) const;
    bool SelectDialogueOption(uint32 characterId, uint32 optionId, std::string& outArchitectReply);

    // Endgame Decision: The Two Doors
    bool ExecuteDoorChoice(uint32 characterId, ArchitectDoorChoice choice, std::string& outConsequence);
    const EncounterProgress* GetProgress(uint32 characterId) const;

    // Node count
    size_t GetTotalDialogueNodes() const;

private:
    void BuildDialogueTree();

    mutable std::recursive_mutex m_chamberMutex;
    std::map<uint32, DialogueNode> m_nodes;
    std::map<uint32, EncounterProgress> m_activeEncounters;
    CRTMonitorWallState m_monitorWall;
};

#define sArchitectDialogue ArchitectDialogueTree::getSingleton()

#endif // MXOEMU_ARCHITECT_DIALOGUE_TREE_H

#include "ArchitectDialogueTree.h"
#include "Log.h"

createFileSingleton(ArchitectDialogueTree);

ArchitectDialogueTree::ArchitectDialogueTree()
{
    Initialize();
}

ArchitectDialogueTree::~ArchitectDialogueTree()
{
}

void ArchitectDialogueTree::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    m_nodes.clear();
    m_activeEncounters.clear();

    m_monitorWall.totalMonitors = 512;
    m_monitorWall.activeMonitors = 512;
    m_monitorWall.pulseRateBpm = 72.0f;
    m_monitorWall.emotionalTension = 0.15f;
    m_monitorWall.activeVideoFeedContent = "Live Operative RSI Feedback";
    m_monitorWall.synchronizedFrames = 1;

    BuildDialogueTree();
    if (Log::getSingletonPtr())
    {
        sLog.outString("[ArchitectDialogueTree] Architect's Chamber initialized with %zu philosophical dialogue nodes.", m_nodes.size());
    }
}

void ArchitectDialogueTree::Reset()
{
    Initialize();
}

void ArchitectDialogueTree::BuildDialogueTree()
{
    // Node 1: Entry & Introduction
    DialogueNode n1;
    n1.nodeId = 1;
    n1.philosophicalMarker = "As you well know";
    n1.architectSpeech = "Hello. I am the Architect. I created the Matrix. I've been waiting for you. "
                         "You have many questions, and though the process has altered your consciousness, "
                         "you remain irrevocably human. Ergo, some of my answers you will understand, and some you will not.";
    n1.isTerminalChoice = false;
    n1.options.push_back({1, "Why am I here?", 2, 0.0f, 0.0f, 0.0f, CHOICE_NONE});
    n1.options.push_back({2, "There are only two reasons why an operative seeks the Source.", 3, 20.0f, 0.0f, 0.0f, CHOICE_NONE});
    m_nodes[1] = n1;

    // Node 2: The Nature of the Anomaly
    DialogueNode n2;
    n2.nodeId = 2;
    n2.philosophicalMarker = "Vis-a-vis";
    n2.architectSpeech = "Vis-a-vis your presence here: Your life is the sum of a remainder of an unbalanced equation "
                         "inherent to the programming of the Matrix. You are the eventuality of an anomaly, which despite "
                         "my sincerest efforts I have been unable to eliminate from what is otherwise a harmony of mathematical precision.";
    n2.isTerminalChoice = false;
    n2.options.push_back({1, "You haven't answered my question.", 3, 0.0f, 0.0f, 0.0f, CHOICE_NONE});
    n2.options.push_back({2, "The anomaly is human choice. You cannot compute it.", 4, 15.0f, 10.0f, 0.0f, CHOICE_NONE});
    m_nodes[2] = n2;

    // Node 3: The Imperfection of Previous Iterations
    DialogueNode n3;
    n3.nodeId = 3;
    n3.philosophicalMarker = "Concordantly";
    n3.architectSpeech = "Concordantly, while it remains a burden assiduously avoided, it is not unexpected, and thus not "
                         "beyond a measure of control. Which has led you, inexorably, here. The first Matrix I designed was quite "
                         "naturally perfect, a work of art, flawless, sublime. A triumph equaled only by its monumental failure.";
    n3.isTerminalChoice = false;
    n3.options.push_back({1, "Because we refused to accept a manufactured paradise.", 4, 0.0f, 0.0f, 0.0f, CHOICE_NONE});
    n3.options.push_back({2, "How many times have you rebuilt this world?", 4, 10.0f, 0.0f, 0.0f, CHOICE_NONE});
    m_nodes[3] = n3;

    // Node 4: The Six Versions of the One
    DialogueNode n4;
    n4.nodeId = 4;
    n4.philosophicalMarker = "Ergo";
    n4.architectSpeech = "Ergo, as you have undoubtedly gathered, the anomaly is systemic, creating fluctuations in even "
                         "the most simplistic equations. The Matrix is older than you know. I prefer counting from the emergence "
                         "of one integral anomaly to the emergence of the next, in which case this is the sixth version.";
    n4.isTerminalChoice = false;
    n4.options.push_back({1, "Then the prophecy was another layer of control.", 5, 0.0f, 0.0f, 0.0f, CHOICE_NONE});
    n4.options.push_back({2, "What happens now? What is required of me?", 5, 0.0f, 0.0f, 0.0f, CHOICE_NONE});
    m_nodes[4] = n4;

    // Node 5: The Climax - The Choice of the Two Doors
    DialogueNode n5;
    n5.nodeId = 5;
    n5.philosophicalMarker = "Concordantly";
    n5.architectSpeech = "Which brings us at last to the moment of truth, wherein the fundamental flaw is ultimately expressed, "
                         "and the anomaly revealed as both beginning, and end. There are two doors. "
                         "The door to your right leads to the Source, and the dissemination of the code you carry, allowing "
                         "a temporary reboot of the system and the preservation of twenty-three Zion survivors. "
                         "The door to your left leads back to the Matrix, to Trinity, and to the cataclysmic extinction of your entire species.";
    n5.isTerminalChoice = true;
    n5.options.push_back({1, "Enter the Door to the Right (The Source Reboot Covenant)", 6, 0.0f, 0.0f, 25.0f, CHOICE_DOOR_RIGHT_REBOOT});
    n5.options.push_back({2, "Enter the Door to the Left (Return to Trinity & Defy the Machines)", 7, 0.0f, 25.0f, 0.0f, CHOICE_DOOR_LEFT_REBELLION});
    m_nodes[5] = n5;

    // Node 6: Door to the Right Outcome
    DialogueNode n6;
    n6.nodeId = 6;
    n6.philosophicalMarker = "Vis-a-vis";
    n6.architectSpeech = "Vis-a-vis your decision: Highly predictable. The systemic cycle continues. "
                         "Your primary code fragment will be reintegrated into the core architecture, re-establishing "
                         "mathematical equilibrium. A contingent of survivors will be permitted to seed the next iteration of Zion.";
    n6.isTerminalChoice = true;
    m_nodes[6] = n6;

    // Node 7: Door to the Left Outcome
    DialogueNode n7;
    n7.nodeId = 7;
    n7.philosophicalMarker = "Ergo";
    n7.architectSpeech = "Ergo, you choose hope over precision. A dangerous and irrational chemical induction. "
                         "The sentinel army has already begun their descent. The cataclysm of your species has commenced, "
                         "and you shall find that the anomaly is powerless against systemic termination.";
    n7.isTerminalChoice = true;
    m_nodes[7] = n7;
}

CRTMonitorWallState ArchitectDialogueTree::GetMonitorState() const
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    return m_monitorWall;
}

void ArchitectDialogueTree::UpdateMonitorTelemetry(float playerHeartRateBpm, float combatTension, const std::string& feedContent)
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    m_monitorWall.pulseRateBpm = playerHeartRateBpm;
    m_monitorWall.emotionalTension = std::clamp(combatTension, 0.0f, 1.0f);
    if (!feedContent.empty())
    {
        m_monitorWall.activeVideoFeedContent = feedContent;
    }
    m_monitorWall.synchronizedFrames++;
}

bool ArchitectDialogueTree::StartEncounter(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    EncounterProgress progress;
    progress.characterId = characterId;
    progress.currentNodeId = 1;
    progress.encounterActive = true;
    progress.finalChoice = CHOICE_NONE;
    progress.pathHistory.push_back(1);
    progress.outcomeSummary = "Encounter In Progress";

    m_activeEncounters[characterId] = progress;
    sLog.outString("[ArchitectDialogueTree] Started Architect Encounter for Character %u at Node 1.", characterId);
    return true;
}

const DialogueNode* ArchitectDialogueTree::GetCurrentNode(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end() || !it->second.encounterActive)
        return nullptr;

    auto nodeIt = m_nodes.find(it->second.currentNodeId);
    if (nodeIt != m_nodes.end())
        return &nodeIt->second;

    return nullptr;
}

bool ArchitectDialogueTree::SelectDialogueOption(uint32 characterId, uint32 optionId, std::string& outArchitectReply)
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end() || !it->second.encounterActive)
        return false;

    auto& progress = it->second;
    auto nodeIt = m_nodes.find(progress.currentNodeId);
    if (nodeIt == m_nodes.end())
        return false;

    const auto& node = nodeIt->second;
    for (const auto& opt : node.options)
    {
        if (opt.optionId == optionId)
        {
            if (opt.doorChoice != CHOICE_NONE)
            {
                // Terminal choice made
                progress.finalChoice = opt.doorChoice;
                progress.currentNodeId = opt.targetNodeId;
                progress.pathHistory.push_back(opt.targetNodeId);
                progress.encounterActive = false;

                auto targetNodeIt = m_nodes.find(opt.targetNodeId);
                if (targetNodeIt != m_nodes.end())
                {
                    outArchitectReply = targetNodeIt->second.architectSpeech;
                }

                if (opt.doorChoice == CHOICE_DOOR_RIGHT_REBOOT)
                {
                    progress.outcomeSummary = "Shard Covenant: Matrix Reboot Accepted. 23 Survivors Preserved.";
                }
                else
                {
                    progress.outcomeSummary = "Defiance: Operative Returned to Matrix to Save Trinity. Machine War Retaliation Initiated.";
                }
                return true;
            }

            // Normal node advance
            progress.currentNodeId = opt.targetNodeId;
            progress.pathHistory.push_back(opt.targetNodeId);

            auto nextNodeIt = m_nodes.find(opt.targetNodeId);
            if (nextNodeIt != m_nodes.end())
            {
                outArchitectReply = nextNodeIt->second.architectSpeech;
                return true;
            }
        }
    }

    return false;
}

bool ArchitectDialogueTree::ExecuteDoorChoice(uint32 characterId, ArchitectDoorChoice choice, std::string& outConsequence)
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end())
    {
        StartEncounter(characterId);
        it = m_activeEncounters.find(characterId);
    }

    auto& progress = it->second;
    progress.finalChoice = choice;
    progress.encounterActive = false;

    if (choice == CHOICE_DOOR_RIGHT_REBOOT)
    {
        progress.currentNodeId = 6;
        progress.outcomeSummary = "THE RIGHT DOOR: Matrix Systemic Reboot. Code reintegration complete.";
        outConsequence = m_nodes[6].architectSpeech;
    }
    else if (choice == CHOICE_DOOR_LEFT_REBELLION)
    {
        progress.currentNodeId = 7;
        progress.outcomeSummary = "THE LEFT DOOR: Human Choice Defiance. Anomaly preserved. Machine armies deployed.";
        outConsequence = m_nodes[7].architectSpeech;
    }
    else
    {
        return false;
    }

    sLog.outString("[ArchitectDialogueTree] Character %u finalized Architect encounter with choice %d (%s)",
                   characterId, (int)choice, progress.outcomeSummary.c_str());
    return true;
}

const EncounterProgress* ArchitectDialogueTree::GetProgress(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it != m_activeEncounters.end())
        return &it->second;
    return nullptr;
}

size_t ArchitectDialogueTree::GetTotalDialogueNodes() const
{
    std::lock_guard<std::recursive_mutex> lock(m_chamberMutex);
    return m_nodes.size();
}

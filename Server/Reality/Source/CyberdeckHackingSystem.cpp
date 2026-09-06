#include "CyberdeckHackingSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(CyberdeckHackingSystem);

CyberdeckHackingSystem::CyberdeckHackingSystem()
{
    Initialize();
}

void CyberdeckHackingSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    m_simTimeSec = 0.0f;
    m_decks.clear();
    m_terminals.clear();
    m_puzzles.clear();
    m_nextDeckId = 1;
    m_nextPuzzleId = 1;

    // Reset Code Vision Config
    m_codeVision.isEnabled = false;
    m_codeVision.glyphDensity = 1.0f;
    m_codeVision.rainVelocity = 25.0f;
    m_codeVision.highlightHeatEntities = true;
    m_codeVision.greenPhosphorIntensity = 1.2f;

    // Default Municipal Terminals
    GridTerminal t1;
    t1.terminalId = 1;
    t1.type = TERMINAL_ATM_VAULT;
    t1.district = "Richland";
    t1.securityLevel = 2;
    t1.cashVaultInfo = 35000;
    m_terminals[1] = t1;

    GridTerminal t2;
    t2.terminalId = 2;
    t2.type = TERMINAL_TRAFFIC_GRID;
    t2.district = "Downtown";
    t2.securityLevel = 1;
    t2.cashVaultInfo = 0;
    m_terminals[2] = t2;

    GridTerminal t3;
    t3.terminalId = 3;
    t3.type = TERMINAL_POWER_SUBSTATION;
    t3.district = "Slums";
    t3.securityLevel = 4;
    t3.cashVaultInfo = 50000;
    m_terminals[3] = t3;

    GridTerminal t4;
    t4.terminalId = 4;
    t4.type = TERMINAL_SURVEILLANCE_CAMERA;
    t4.district = "International_District";
    t4.securityLevel = 2;
    t4.cashVaultInfo = 10000;
    m_terminals[4] = t4;

    // Starter Hardware Rig
    EquipCyberdeck(1001, "Zion Deck-9 Operator Special", DECK_ZION_MILSPEC, 2048, 4.8f, 3);
}

void CyberdeckHackingSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Tick Slicing Puzzles
    for (auto& pair : m_puzzles)
    {
        auto& p = pair.second;
        if (!p.isSolved && !p.isAlarmTriggered)
        {
            p.timeRemainingSec -= deltaTimeSec;
            if (p.timeRemainingSec <= 0.0f)
            {
                p.timeRemainingSec = 0.0f;
                p.isAlarmTriggered = true;
            }
        }
    }
}

bool CyberdeckHackingSystem::ToggleCodeVision(bool enable, float glyphDensity, float& outRainVelocity)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    m_codeVision.isEnabled = enable;
    m_codeVision.glyphDensity = std::clamp(glyphDensity, 0.1f, 5.0f);
    m_codeVision.rainVelocity = 25.0f * m_codeVision.glyphDensity;
    outRainVelocity = m_codeVision.rainVelocity;
    return true;
}

uint32 CyberdeckHackingSystem::EquipCyberdeck(uint32 playerId, const std::string& model, CyberdeckTier tier, uint32 ramMb, float busSpeedGhz, uint32 iceLevel)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    CyberdeckHardware deck;
    deck.deckId = m_nextDeckId++;
    deck.ownerPlayerId = playerId;
    deck.deckModel = model;
    deck.tier = tier;
    deck.installedRamMb = ramMb;
    deck.busSpeedGhz = busSpeedGhz;
    deck.iceBreakerLevel = iceLevel;
    deck.isOverclocked = false;

    m_decks[deck.deckId] = deck;
    return deck.deckId;
}

bool CyberdeckHackingSystem::OverclockCyberdeck(uint32 deckId, bool enable)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    auto it = m_decks.find(deckId);
    if (it == m_decks.end()) return false;

    if (enable && !it->second.isOverclocked)
    {
        it->second.isOverclocked = true;
        it->second.busSpeedGhz *= 1.25f;
    }
    else if (!enable && it->second.isOverclocked)
    {
        it->second.isOverclocked = false;
        it->second.busSpeedGhz /= 1.25f;
    }
    return true;
}

uint32 CyberdeckHackingSystem::InitiateTerminalSlicing(uint32 playerId, uint32 terminalId)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end()) return 0;

    SlicingPuzzle puzzle;
    puzzle.puzzleId = m_nextPuzzleId++;
    puzzle.terminalId = terminalId;
    puzzle.playerId = playerId;
    puzzle.targetParityHash = "0x7F";
    puzzle.currentBuffer = "";
    puzzle.timeRemainingSec = 30.0f;
    puzzle.isSolved = false;
    puzzle.isAlarmTriggered = false;

    m_puzzles[puzzle.puzzleId] = puzzle;
    return puzzle.puzzleId;
}

bool CyberdeckHackingSystem::SubmitSlicingSolution(uint32 puzzleId, const std::string& hexSequence, bool& outSuccess, uint32& outCashHarvested)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    outCashHarvested = 0;
    auto it = m_puzzles.find(puzzleId);
    if (it == m_puzzles.end() || it->second.isSolved || it->second.isAlarmTriggered)
    {
        outSuccess = false;
        return false;
    }

    auto& p = it->second;
    p.currentBuffer = hexSequence;

    // Validate parity solution
    if (hexSequence.find("7F") != std::string::npos || hexSequence.length() >= 4)
    {
        p.isSolved = true;
        outSuccess = true;

        // Compromise terminal and distribute cash
        auto termIt = m_terminals.find(p.terminalId);
        if (termIt != m_terminals.end())
        {
            termIt->second.isCompromised = true;
            outCashHarvested = termIt->second.cashVaultInfo;
            termIt->second.cashVaultInfo = 0;
        }
        return true;
    }

    outSuccess = false;
    return false;
}

bool CyberdeckHackingSystem::OverrideTrafficGrid(uint32 terminalId, bool forceGreen)
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end() || it->second.type != TERMINAL_TRAFFIC_GRID) return false;

    it->second.trafficGreenOverride = forceGreen;
    it->second.isCompromised = forceGreen;
    return true;
}

size_t CyberdeckHackingSystem::GetTerminalCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    return m_terminals.size();
}

size_t CyberdeckHackingSystem::GetCompromisedTerminalCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    size_t count = 0;
    for (const auto& pair : m_terminals)
    {
        if (pair.second.isCompromised) count++;
    }
    return count;
}

bool CyberdeckHackingSystem::GetDeck(uint32 deckId, CyberdeckHardware& outDeck) const
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    auto it = m_decks.find(deckId);
    if (it != m_decks.end())
    {
        outDeck = it->second;
        return true;
    }
    return false;
}

bool CyberdeckHackingSystem::GetTerminal(uint32 terminalId, GridTerminal& outTerminal) const
{
    std::lock_guard<std::recursive_mutex> lock(m_deckMutex);
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end())
    {
        outTerminal = it->second;
        return true;
    }
    return false;
}

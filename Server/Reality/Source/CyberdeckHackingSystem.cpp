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

// ============================================================================
// HEADLESS TEST SUITE: CYBERDECK HARDWARE & TERMINAL SLICING (SUITE 22)
// ============================================================================
void RunCyberdeckHackingTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING CYBERDECK HARDWARE & SLICING TEST SUITE (SUITE 22)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // 1. Initialization & Default Terminals
    sCyberdeckHackingSystem.Initialize();
    TEST_ASSERT(sCyberdeckHackingSystem.GetTerminalCount() == 4, "Initialized 4 municipal grid terminals");
    TEST_ASSERT(sCyberdeckHackingSystem.GetCompromisedTerminalCount() == 0, "Zero terminals compromised initially");

    GridTerminal t1;
    TEST_ASSERT(sCyberdeckHackingSystem.GetTerminal(1, t1) == true, "Terminal 1 (Richland ATM) retrieved");
    TEST_ASSERT(t1.type == TERMINAL_ATM_VAULT, "Terminal 1 is ATM Vault");
    TEST_ASSERT(t1.cashVaultInfo == 35000, "ATM vault contains 35,000 Info");

    // 2. Green Code Vision
    float rainVel = 0.0f;
    sCyberdeckHackingSystem.ToggleCodeVision(true, 2.0f, rainVel);
    const CodeVisionConfig& cv = sCyberdeckHackingSystem.GetCodeVisionConfig();
    TEST_ASSERT(cv.isEnabled == true, "Code Vision successfully enabled");
    TEST_ASSERT(cv.glyphDensity == 2.0f, "Glyph density set to 2.0x");
    TEST_ASSERT(rainVel == 50.0f, "Rain velocity scales to 50 units/sec");

    sCyberdeckHackingSystem.ToggleCodeVision(false, 1.0f, rainVel);
    TEST_ASSERT(sCyberdeckHackingSystem.GetCodeVisionConfig().isEnabled == false, "Code Vision toggled off");

    // 3. Cyberdeck Equipment & Overclocking
    uint32 deckId = sCyberdeckHackingSystem.EquipCyberdeck(2002, "Onyx DeepDive v3", DECK_ONYX_DEEPDIVE, 4096, 5.0f, 4);
    TEST_ASSERT(deckId > 0, "Onyx DeepDive cyberdeck equipped");

    CyberdeckHardware deck;
    TEST_ASSERT(sCyberdeckHackingSystem.GetDeck(deckId, deck) == true, "Cyberdeck hardware profile retrieved");
    TEST_ASSERT(deck.tier == DECK_ONYX_DEEPDIVE, "Cyberdeck tier is ONYX_DEEPDIVE");
    TEST_ASSERT(deck.installedRamMb == 4096, "Deck has 4096MB RAM");
    TEST_ASSERT(deck.busSpeedGhz == 5.0f, "Deck base bus speed is 5.0 GHz");
    TEST_ASSERT(deck.iceBreakerLevel == 4, "Deck ICE breaker level is 4");
    TEST_ASSERT(deck.isOverclocked == false, "Deck starts in stock frequency mode");

    // Overclocking
    TEST_ASSERT(sCyberdeckHackingSystem.OverclockCyberdeck(deckId, true) == true, "Overclocking engaged");
    sCyberdeckHackingSystem.GetDeck(deckId, deck);
    TEST_ASSERT(deck.isOverclocked == true, "Deck marked as overclocked");
    TEST_ASSERT(deck.busSpeedGhz == 6.25f, "Overclocked bus speed boosted by 25% (6.25 GHz)");

    TEST_ASSERT(sCyberdeckHackingSystem.OverclockCyberdeck(deckId, false) == true, "Overclocking disengaged");
    sCyberdeckHackingSystem.GetDeck(deckId, deck);
    TEST_ASSERT(deck.isOverclocked == false, "Deck returned to stock clock");
    TEST_ASSERT(deck.busSpeedGhz == 5.0f, "Bus speed restored to 5.0 GHz");

    // 4. Municipal Terminal Slicing & Harvest
    uint32 puzzleId = sCyberdeckHackingSystem.InitiateTerminalSlicing(2002, 1);
    TEST_ASSERT(puzzleId > 0, "Slicing puzzle initiated on ATM terminal 1");

    bool solveSuccess = false;
    uint32 cashHarvested = 0;
    bool submitted = sCyberdeckHackingSystem.SubmitSlicingSolution(puzzleId, "0xDEADBEEF7F", solveSuccess, cashHarvested);
    TEST_ASSERT(submitted == true, "Parity solution accepted by terminal ICE");
    TEST_ASSERT(solveSuccess == true, "Slicing puzzle solved successfully");
    TEST_ASSERT(cashHarvested == 35000, "ATM cash vault harvested (35,000 Info)");

    sCyberdeckHackingSystem.GetTerminal(1, t1);
    TEST_ASSERT(t1.isCompromised == true, "ATM terminal marked as compromised");
    TEST_ASSERT(t1.cashVaultInfo == 0, "ATM vault emptied post-slice");
    TEST_ASSERT(sCyberdeckHackingSystem.GetCompromisedTerminalCount() == 1, "Compromised terminal count is 1");

    // Re-submission should fail on already solved puzzle
    bool secondSolve = false;
    uint32 secondCash = 0;
    TEST_ASSERT(sCyberdeckHackingSystem.SubmitSlicingSolution(puzzleId, "0x7F", secondSolve, secondCash) == false, "Cannot re-submit to solved puzzle");

    // 5. Traffic Grid Remote Override
    TEST_ASSERT(sCyberdeckHackingSystem.OverrideTrafficGrid(2, true) == true, "Traffic grid terminal 2 green override activated");
    GridTerminal t2;
    sCyberdeckHackingSystem.GetTerminal(2, t2);
    TEST_ASSERT(t2.trafficGreenOverride == true, "Green light wave confirmed on Downtown grid");
    TEST_ASSERT(t2.isCompromised == true, "Traffic terminal marked compromised");
    TEST_ASSERT(sCyberdeckHackingSystem.OverrideTrafficGrid(1, true) == false, "Cannot override traffic on ATM terminal");

    // 6. Puzzle Countdown & Intrusion Alarm
    uint32 p2 = sCyberdeckHackingSystem.InitiateTerminalSlicing(2002, 3);
    TEST_ASSERT(p2 > 0, "Slicing puzzle initiated on Power Substation 3");
    sCyberdeckHackingSystem.UpdateSimulation(35.0f); // Advance past 30.0s limit

    bool p2Success = false;
    uint32 p2Cash = 0;
    TEST_ASSERT(sCyberdeckHackingSystem.SubmitSlicingSolution(p2, "0x7F", p2Success, p2Cash) == false, "Expired puzzle rejected submission due to alarm");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  CYBERDECK HARDWARE & SLICING TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failed > 0) {
        std::cerr << "RunCyberdeckHackingTestSuite: FAILED with " << failed << " errors!" << std::endl;
        exit(1);
    }
}


#ifndef MXOEMU_CYBERDECK_HACKING_SYSTEM_H
#define MXOEMU_CYBERDECK_HACKING_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum CyberdeckTier
{
    DECK_STREET_RIG      = 1,
    DECK_ZION_MILSPEC    = 2,
    DECK_ONYX_DEEPDIVE   = 3
};

enum TerminalType
{
    TERMINAL_TRAFFIC_GRID          = 1,
    TERMINAL_ATM_VAULT             = 2,
    TERMINAL_SURVEILLANCE_CAMERA   = 3,
    TERMINAL_POWER_SUBSTATION      = 4
};

struct CodeVisionConfig
{
    bool isEnabled{false};
    float glyphDensity{1.0f};
    float rainVelocity{25.0f};
    bool highlightHeatEntities{true};
    float greenPhosphorIntensity{1.2f};
};

struct CyberdeckHardware
{
    uint32 deckId{1};
    uint32 ownerPlayerId{0};
    std::string deckModel{"Zion Deck-9 Operator Special"};
    CyberdeckTier tier{DECK_ZION_MILSPEC};
    uint32 installedRamMb{2048};
    float busSpeedGhz{4.8f};
    uint32 iceBreakerLevel{3};
    bool isOverclocked{false};
};

struct GridTerminal
{
    uint32 terminalId{1};
    TerminalType type{TERMINAL_ATM_VAULT};
    std::string district{"International_District"};
    uint32 securityLevel{3};
    bool isCompromised{false};
    uint32 cashVaultInfo{25000};
    bool trafficGreenOverride{false};
};

struct SlicingPuzzle
{
    uint32 puzzleId{1};
    uint32 terminalId{1};
    uint32 playerId{0};
    std::string targetParityHash{"0xDEADBEEF"};
    std::string currentBuffer{""};
    float timeRemainingSec{30.0f};
    bool isSolved{false};
    bool isAlarmTriggered{false};
};

class CyberdeckHackingSystem : public Singleton<CyberdeckHackingSystem>
{
public:
    CyberdeckHackingSystem();
    ~CyberdeckHackingSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Green Digital Rain "Code Vision"
    bool ToggleCodeVision(bool enable, float glyphDensity, float& outRainVelocity);

    // Cyberdeck Hardware Inventory
    uint32 EquipCyberdeck(uint32 playerId, const std::string& model, CyberdeckTier tier, uint32 ramMb, float busSpeedGhz, uint32 iceLevel);
    bool OverclockCyberdeck(uint32 deckId, bool enable);

    // Municipal Terminal Slicing & Intrusion
    uint32 InitiateTerminalSlicing(uint32 playerId, uint32 terminalId);
    bool SubmitSlicingSolution(uint32 puzzleId, const std::string& hexSequence, bool& outSuccess, uint32& outCashHarvested);
    bool OverrideTrafficGrid(uint32 terminalId, bool forceGreen);

    // Telemetry & Getters
    const CodeVisionConfig& GetCodeVisionConfig() const { return m_codeVision; }
    size_t GetTerminalCount() const;
    size_t GetCompromisedTerminalCount() const;
    bool GetDeck(uint32 deckId, CyberdeckHardware& outDeck) const;
    bool GetTerminal(uint32 terminalId, GridTerminal& outTerminal) const;

private:
    mutable std::recursive_mutex m_deckMutex;
    CodeVisionConfig m_codeVision;
    std::map<uint32, CyberdeckHardware> m_decks;
    std::map<uint32, GridTerminal> m_terminals;
    std::map<uint32, SlicingPuzzle> m_puzzles;

    uint32 m_nextDeckId{1};
    uint32 m_nextPuzzleId{1};
    float m_simTimeSec{0.0f};
};

#define sCyberdeckHackingSystem CyberdeckHackingSystem::getSingleton()

#endif // MXOEMU_CYBERDECK_HACKING_SYSTEM_H

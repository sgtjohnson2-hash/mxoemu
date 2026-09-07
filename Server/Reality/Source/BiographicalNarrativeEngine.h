#ifndef MXOEMU_BIOGRAPHICAL_NARRATIVE_ENGINE_H
#define MXOEMU_BIOGRAPHICAL_NARRATIVE_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "CityLifeManager.h"
#include "EmergentPoliceManager.h"
#include "UnderworldManager.h"

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <cstdint>
#include <sstream>

// ============================================================================
// Dwarf Fortress-Style Biographical Narrative & Naming Enums
// ============================================================================

enum class BioFaction : uint8_t {
    ZionRedpill         = 0, // Unplugged human freedom fighters & hovercraft crews
    MachineAgent        = 1, // Formal Agents (Smith, Pace...), sentinels, upgraded system programs
    MerovingianExile    = 2, // Rogue programs from previous Matrix versions (Vampires, Lupines)
    CypheriteTurncoat   = 3, // Disillusioned redpills seeking reinsertion, steak-lovers, traitors
    BluepillCivilian    = 4, // Plugged-in humans living routine simulated lives unaware
    MMPDPoliceSWAT      = 5, // Megacity Police Department officers, SWAT stacks, detectives
    SyndicateEnforcer   = 6  // Underworld mobsters, triad enforcers, cyber-punks, racketeers
};

enum class MatrixOriginEra : uint8_t {
    Beta_Version1_Paradise        = 0, // The flawed first paradise matrix that collapsed
    Beta_Version2_Nightmare       = 1, // The gothic second version (vampires, werewolves, ghosts)
    Release_Version3_Industrial   = 2, // Early mechanical iteration
    Release_Version6_PostReboot   = 3, // Modern late-90s Megacity cycle (Neo era)
    Release_Version7_TruceEra     = 4, // Current fragile truce period between Zion and Machines
    Zion_NaturalBorn              = 5, // Born in the deep geothermal residential caves of Zion
    MachineCity_01_KernelCompiled = 6  // Compiled in the Source core of 01 Machine City
};

// ============================================================================
// Compact Flyweight Profile (32 Bytes - Low Memory Footprint)
// Allows 100,000 profiles to reside in memory in only ~3.2 MB of RAM!
// ============================================================================
#pragma pack(push, 1)
struct CompactProfile {
    uint64_t seed;              // 8 bytes: Deterministic origin seed
    uint8_t  faction;           // 1 byte: BioFaction
    uint8_t  originEra;         // 1 byte: MatrixOriginEra
    uint16_t nameIndex;         // 2 bytes: Index into faction naming pool
    uint8_t  epithetIndex;      // 1 byte: Index into DF-style epithets
    uint8_t  careerIndex;       // 1 byte: Index into careers/roles
    uint8_t  organizationIndex; // 1 byte: Index into ships/precincts/syndicates/firms
    uint8_t  turningPointIndex; // 1 byte: Index into formative turning points
    uint8_t  crucibleIndex;     // 1 byte: Index into defining battle / career crucible
    uint8_t  quirkIndex;        // 1 byte: Index into physical quirks & scars
    uint8_t  memoryIndex;       // 1 byte: Index into Matrix simulation memories
    uint8_t  ambitionIndex;     // 1 byte: Index into ambitions & vendettas
    uint8_t  flawIndex;         // 1 byte: Index into secret flaws & weaknesses
    uint8_t  rsiGlitchIndex;    // 1 byte: Index into Residual Self-Image anomalies
    uint8_t  personalityIndex;  // 1 byte: Index into psychological disposition
    uint8_t  secondaryNameIndex;// 1 byte: Secondary / surname / unit tag index (Total: 24 bytes)
};
#pragma pack(pop)

// ============================================================================
// Chronological Life-Event Timeline Entry (Dwarf Fortress Legends Style)
// ============================================================================
struct ChronologicalEvent {
    uint32_t yearOrCycle{1999};
    std::string eraTag;       // "Genesis", "Simulation Life", "The Awakening", "The Crucible", "Current Chapter"
    std::string chapterTitle; // e.g. "The Glass Mirror Glitch", "The Siege of Concourse 4"
    std::string description;  // Multi-sentence detailed lore narrative
    std::string location;     // District, ship, or virtual sector
    std::string impactTag;    // "Gained Spinal Scar", "Awakened Mind", "Promoted to Chief Operator"
};

// ============================================================================
// Full Rich Biographical Profile (Deep Dwarf Fortress Character Sheet)
// ============================================================================
struct BiographicalProfile {
    uint64_t seed{0};
    BioFaction faction{BioFaction::ZionRedpill};
    std::string factionName;
    
    // Naming & Identity
    std::string primaryName;         // Hacker handle, Agent surname, French exile moniker, civilian name
    std::string formalOrRealName;    // Real-world birth name or system process designation
    std::string dfEpithet;           // Dwarf Fortress title: "the Mirror-Breaker", "the Iron Gutter"
    std::string titleRole;           // Career / rank: "Chief Hovercraft Operator", "Precinct Detective"
    std::string organization;        // Hovercraft name, precinct, syndicate gang, corporate firm
    
    // Origins & Demographics
    MatrixOriginEra originEra{MatrixOriginEra::Release_Version6_PostReboot};
    std::string originEraName;
    std::string originDescription;   // Narrative origin string
    std::string birthOrCompilePlace; // Pod Sector 77-Epsilon, Zion Geothermal Tunnels, Machine City
    uint32_t birthYearOrCycle{1982};
    std::string gender;
    
    // Residual Self-Image (RSI) & Physical Idiosyncrasies
    std::string physicalAppearance;   // Build, height, shades, coat, posture
    std::string rsiGlitch;            // Emerald phosphor scanline, reversed iris, translucent fingers
    std::vector<std::string> scarsAndQuirks; // Pod-plug scars, modem tinnitus, APU calluses
    
    // Cognitive & Lore Memory
    std::string simulationMemory;     // Haunting sensory memory from plugged-in life
    std::string philosophicalOutlook; // Beliefs on Zion, the Machines, Fate vs Choice, the One
    std::string currentAmbition;      // Driving personal quest, vendetta, or goal
    std::string secretFlaw;           // Secret vulnerability, addiction to steak, insomnia
    
    // Chronological Timeline (4 to 8 life chapters)
    std::vector<ChronologicalEvent> timeline;

    // Generates a Dwarf Fortress-style multi-paragraph markdown character sheet
    std::string ToDFCharacterSheet() const;
    // Generates a compact one-line telemetry summary
    std::string ToOneLineSummary() const;
    // Serializes to JSON string
    std::string ToJSON() const;
};

// ============================================================================
// Biographical Narrative Engine (Master Singleton)
// ============================================================================
class BiographicalNarrativeEngine : public Singleton<BiographicalNarrativeEngine> {
public:
    BiographicalNarrativeEngine();
    ~BiographicalNarrativeEngine();

    void Initialize();
    bool IsInitialized() const { return m_initialized; }
    void Update(uint32 deltaMs);

    // On-demand profile generation
    BiographicalProfile GenerateProfile(uint64_t seed, BioFaction factionHint = BioFaction::ZionRedpill) const;
    CompactProfile GenerateCompactProfile(uint64_t seed, BioFaction factionHint = BioFaction::ZionRedpill) const;
    BiographicalProfile DecompressProfile(const CompactProfile& compact) const;

    // Integration Adapters
    BiographicalProfile GenerateProfileForBot(uint32 goId, uint64 charUID, mxoFaction faction);
    BiographicalProfile GenerateProfileForCitizen(uint32 citizenId, CivilianArchetype archetype);
    BiographicalProfile GenerateProfileForPolice(uint32 officerId, SWATRole role);
    BiographicalProfile GenerateProfileForSyndicate(uint32 operativeId, SyndicateFaction syndicate);

    // Dynamic World Event Triggers & Threat Interlock
    void CheckAndTriggerBiographicalWorldEvents(const BiographicalProfile& profile, uint32 goId, uint64 charUID, const LocationVector& loc, uint32 districtId);

    // Batch Generation & Benchmarking
    uint32_t BatchGenerateProfiles(uint32_t count, std::vector<CompactProfile>& outProfiles, uint64_t baseSeed = 0x19991337ULL) const;
    double MeasureGenerationThroughput(uint32_t testCount = 100000) const;
    size_t CalculateMemoryFootprintBytes(uint32_t profileCount) const;

    // Telemetry & Reporting
    std::string GenerateEngineTelemetryReport() const;
    uint64_t GetTotalTheoreticalCombinations() const;

    // Direct Lexicon Access (for testing and validation)
    size_t GetLexiconSizeRedpillHandles() const;
    size_t GetLexiconSizeAgentNames() const;
    size_t GetLexiconSizeExileNames() const;
    size_t GetLexiconSizeCypheriteNames() const;
    size_t GetLexiconSizeCivilianNames() const;
    size_t GetLexiconSizeEpithets() const;
    size_t GetLexiconSizeMemories() const;
    size_t GetLexiconSizeQuirks() const;
    size_t GetLexiconSizePoliceRoles() const;
    size_t GetLexiconSizePolicePrecincts() const;
    size_t GetLexiconSizeSyndicateRoles() const;
    size_t GetLexiconSizeSyndicateOrganizations() const;
    size_t GetLexiconSizeCivilianProfessions() const;
    size_t GetLexiconSizeCivilianOrganizations() const;
    size_t GetLexiconSizeAgentOrganizations() const;
    size_t GetLexiconSizeExileOrganizations() const;
    size_t GetLexiconSizeCypheriteOrganizations() const;

private:
    bool m_initialized{false};
    mutable std::mutex m_engineMutex;

    // Fast deterministic SplitMix64 pseudo-random generator
    static inline uint64_t SplitMix64(uint64_t& state) {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    // Handcrafted Static Lexicon Arrays
    void InitializeLexicons();

    // Active Profile Cache for live entities
    std::unordered_map<uint64_t, CompactProfile> m_activeProfileCache;
};

#define sBioEngine BiographicalNarrativeEngine::getSingleton()

// Automated Test Suite Declaration
void RunBiographicalTestSuite();

#endif // MXOEMU_BIOGRAPHICAL_NARRATIVE_ENGINE_H

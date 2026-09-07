#ifndef MXOEMU_FRANK_CASTLE_MANAGER_H
#define MXOEMU_FRANK_CASTLE_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include "AI/QTable.h"
#include "SmithVirusCascade.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>

// ============================================================================
// Canonical Constants for Secret Anti-Hero Character Frank Castle (The Punisher)
// ============================================================================
constexpr uint64 FRANK_CASTLE_UID = 9999007ULL;
constexpr const char* FRANK_CASTLE_NAME = "Frank Castle";
constexpr const char* FRANK_CASTLE_TITLE = "The Punisher";
constexpr const char* FRANK_CASTLE_BACKGROUND = 
    "Former USMC Force Recon veteran and code-hardened vigilante exile. "
    "Wages an autonomous war against systemic Machine oppression, corrupted syndicates, and Agents of the System.";
constexpr const char* FRANK_RADIO_FREQUENCY = "FM 88.3 (The War Zone)";

// ============================================================================
// Phase 1: War Journal & Dynamic Hit List Structures
// ============================================================================
enum WarJournalEntryType {
    JOURNAL_KILL = 0,
    JOURNAL_TACTICAL_RETREAT = 1,
    JOURNAL_SAFEHOUSE_CLAIMED = 2,
    JOURNAL_SAFEHOUSE_FORTIFIED = 3,
    JOURNAL_SIEGE_DEFENSE = 4,
    JOURNAL_RADIO_BROADCAST = 5,
    JOURNAL_CASSETTE_DROPPED = 6,
    JOURNAL_STALKING_AMBUSH = 7,
    JOURNAL_RESUPPLY = 8,
    JOURNAL_SYNDICATE_HIT = 9,
    JOURNAL_VIGILANTE_JUDGMENT = 10,
    JOURNAL_CONSTRUCT_FABRICATION = 11
};

struct WarJournalEntry {
    uint64 id{0};
    uint32 timestampMs{0};
    std::string timestampStr;
    WarJournalEntryType type{JOURNAL_KILL};
    uint32 districtId{0};
    std::string districtName;
    LocationVector location;
    std::string targetHandle;
    std::string summary;
    std::string fieldNotes;
};

struct WarJournalCassette {
    uint32 cassetteId{0};
    std::string title;
    LocationVector dropLocation;
    uint32 districtId{0};
    std::string districtName;
    std::string decryptionKey;
    WarJournalEntry entry;
    bool isDecrypted{false};
    uint32 discoveredByPlayerGoId{0};
};

enum HitListPriority {
    PRIORITY_OMEGA_SMITH = 0,    // Priority 0: Active Smith viral clone or cluster
    PRIORITY_SYSTEM_AGENT = 1,   // Priority 1: System Agents patrolling or hunting redpills
    PRIORITY_SYNDICATE_BOSS = 2, // Priority 2: Merovingian Lupine / Vampire lieutenants & extortion
    PRIORITY_CORRUPT_PVP = 3,    // Priority 3: Rogue redpills with severe negative karma / griefers
    PRIORITY_MACHINE_CONVOY = 4  // Priority 4: Machine power convoys & pacifier escorts
};

enum HitListStatus {
    HIT_STATUS_QUEUED = 0,
    HIT_STATUS_RECON = 1,
    HIT_STATUS_ENGAGING = 2,
    HIT_STATUS_TERMINATED = 3,
    HIT_STATUS_ESCAPED = 4
};

struct HitListEntry {
    uint32 targetGoId{0};
    uint64 targetCharUID{0};
    std::string targetHandle;
    HitListPriority priority{PRIORITY_SYSTEM_AGENT};
    HitListStatus status{HIT_STATUS_QUEUED};
    float infamyScore{100.0f};
    std::string crimeDossier;
    LocationVector lastKnownPos;
    uint32 districtId{1};
    std::string districtName{"Slums"};
    uint32 addedTimestampMs{0};
    uint32 reconTimerMs{0};
};

// ============================================================================
// Phase 2: Microchip Tech & Construct Armory Logistics
// ============================================================================
enum CacheType {
    CACHE_SUBWAY_DEPOT = 0, // Heavy munitions, DU rounds, limpet mines
    CACHE_ROOFTOP_HVAC = 1, // Scoped optics, scrambler satchels, thermal scanners
    CACHE_SEWER_TRIAGE = 2  // Trauma kits, adrenaline stims, ceramic armor plates
};

struct TacticalStockpile {
    uint32 highCaliberAmmo = 500;
    uint32 empGrenades = 12;
    uint32 armorPlates = 15;
    uint32 traumaKits = 10;
    uint32 scramblerCodes = 5;
    uint32 antiviralRounds = 50;

    uint32 GetTotalSupplies() const {
        return highCaliberAmmo + (empGrenades * 10) + (armorPlates * 15) + 
               (traumaKits * 12) + (scramblerCodes * 20) + (antiviralRounds * 8);
    }
};

struct TacticalFieldCache {
    uint32 cacheId{0};
    std::string codename;
    CacheType type{CACHE_SUBWAY_DEPOT};
    LocationVector location;
    uint32 districtId{1};
    std::string districtName;
    TacticalStockpile supplies;
    bool isCompromised{false};
    uint32 lastRestockedMs{0};
};

struct MicrochipHardware {
    uint32 depletedUraniumRounds{60};     // Bypasses 50% target armor mitigation
    uint32 codeScramblerEMPSatchels{10};  // Disables hostile ability queue for 10s
    uint32 adrenalineStims{8};           // Cleanses stun/CC & triples health regeneration
    uint32 antiviralIncendiaryRounds{50}; // 250% damage against Smith viral signatures
    uint32 whitePhosphorusSatchels{6};   // AoE thermal burn zone
    uint32 limpetMines{6};               // High-explosive anti-vehicle / convoy sabotage
};

// ============================================================================
// Phase 3: Safehouse Defense, Incursions & Siege Events
// ============================================================================
enum SafehouseStatus {
    SAFEHOUSE_UNCLAIMED = 0,
    SAFEHOUSE_HOSTILE = 1,
    SAFEHOUSE_CONTESTED = 2,
    SAFEHOUSE_SECURED = 3,
    SAFEHOUSE_FORTIFIED = 4
};

enum SiegeFaction {
    SIEGE_FACTION_MACHINES = 0,
    SIEGE_FACTION_EXILES = 1,
    SIEGE_FACTION_MEROVINGIAN = 2
};

struct SafehouseSiegeEvent {
    uint32 safehouseId{0};
    bool active{false};
    SiegeFaction attackingFaction{SIEGE_FACTION_MACHINES};
    std::string factionName;
    uint32 currentWave{0};
    uint32 totalWaves{3};
    uint32 enemiesRemaining{0};
    float safehouseIntegrity{100.0f};
    uint32 waveTimerMs{0};
    uint32 participatingPlayersCount{0};
};

struct SafehouseNode {
    uint32 id;
    std::string name;
    uint32 districtId;
    std::string districtName;
    LocationVector location;
    SafehouseStatus status;
    uint8 fortificationLevel; // 0 to 5
    TacticalStockpile stockpile;
    bool tripwireTrapActive;
    bool turretDefenseActive;
    uint32 lastReinforcedMs;
    uint32 hostileGuardsCount;
    bool antiviralScrubberActive; // Active hardline decontamination field
    uint32 lastScrubberPulseMs;   // Periodic scrubber pulse
    uint32 ciwsLastFiredMs{0};    // CIWS automated firing timer
};

// ============================================================================
// Phase 4 & 5: Player Interaction, Vigilante Karma & FM 88.3 Radio Net
// ============================================================================
struct PlayerKarmaRecord {
    uint32 playerGoId{0};
    std::string handle;
    int32 karma{0}; // -1000 to +1000
    uint32 agentsKilled{0};
    uint32 smithsKilled{0};
    uint32 siegesAssisted{0};
    uint32 griefingIncidents{0};
    bool isMarkedForPunishment{false};
    uint32 lastInteractionMs{0};
};

struct VigilanteContract {
    uint32 contractId{0};
    std::string title;
    std::string description;
    std::string targetDescription;
    uint32 targetDistrictId{1};
    uint32 requiredKills{3};
    uint32 currentKills{0};
    uint32 rewardAmmo{150};
    int32 rewardKarma{150};
    bool isCompleted{false};
    uint32 acceptedPlayerGoId{0};
};

struct RadioBroadcastRecord {
    uint32 broadcastId{0};
    uint32 timestampMs{0};
    std::string frequency{"FM 88.3"};
    std::string transmissionText;
    bool squelchTonePlayed{true};
};

// ============================================================================
// Phase 6: Deep Reinforcement Learning & Tactical Combat Evolution
// ============================================================================
enum FrankTacticalState {
    FRANK_STATE_IDLE_PATROL = 0,
    FRANK_STATE_HUNT_AGENTS = 1,
    FRANK_STATE_ASSAULT_SAFEHOUSE = 2,
    FRANK_STATE_FORTIFY_SAFEHOUSE = 3,
    FRANK_STATE_IN_COMBAT = 4,
    FRANK_STATE_TACTICAL_RETREAT = 5,
    FRANK_STATE_FIELD_TRIAGE = 6,
    FRANK_STATE_PURGE_SMITH_OUTBREAK = 7,
    FRANK_STATE_CONTAINMENT_PROTOCOL = 8,
    FRANK_STATE_STALKING_TARGET = 9,      // Phase 1: Recon & Ambush stalking
    FRANK_STATE_RESUPPLY_RUN = 10,        // Phase 2: Microchip cache re-arm
    FRANK_STATE_DEFEND_SAFEHOUSE = 11,    // Phase 3: Safehouse siege defense
    FRANK_STATE_CLEAN_SWEEP_RAID = 12     // Phase 4: Syndicate decapitation raid
};

enum FrankMasteryRank {
    RANK_URBAN_VIGILANTE = 1,     // Level 1-10
    RANK_GUERRILLA_OPERATIVE = 2, // Level 11-20
    RANK_TACTICAL_SPECIALIST = 3, // Level 21-30
    RANK_SYSTEM_NEMESIS = 4,      // Level 31-40
    RANK_THE_PUNISHER = 5         // Level 41-50+
};

enum FrankCombatAction {
    ACT_ARMOR_PIERCING_VOLLEY = 0,
    ACT_EMP_DISRUPTION_GRENADE = 1,
    ACT_CQC_DISARM_TAKEDOWN = 2,
    ACT_TACTICAL_COVER_ROLL = 3,
    ACT_ADRENALINE_STIM = 4,
    ACT_SMOKE_EXTRACTION = 5,
    ACT_ANTIVIRAL_AP_VOLLEY = 6,
    ACT_WHITE_PHOSPHORUS_BURST = 7,
    ACT_POINT_BLANK_EXECUTION = 8,
    ACT_SNIPER_AMBUSH = 9,
    ACT_FLANKING_MANEUVER = 10,
    ACT_EXPLOSIVE_BARREL_DETONATION = 11,
    ACT_COLLAPSE_SCAFFOLDING = 12,
    ACT_FLASHBANG_STUN = 13
};

struct ThreatMemoryRecord {
    uint32 targetGoId;
    std::string handle;
    bool isAgent;
    uint32 encounters;
    uint32 evasionCount;
    uint32 damageDealtToFrank;
    uint32 damageTakenFromFrank;
    uint32 lastEncounterMs;
};

// ============================================================================
// Phase 7: Client-Side Remaster Integration & Visual Polish
// ============================================================================
enum VisualBattleCondition {
    VISUAL_PRISTINE = 0,               // 100% - 75% HP
    VISUAL_LIGHT_DAMAGE = 1,          // 74% - 50% HP
    VISUAL_HEAVY_DAMAGE = 2,          // 49% - 25% HP
    VISUAL_CRITICAL_BATTLE_WEAR = 3   // < 25% HP
};

// ============================================================================
// Frank Castle Manager (Apex Singleton)
// ============================================================================
class FrankCastleManager : public Singleton<FrankCastleManager> {
public:
    FrankCastleManager();
    ~FrankCastleManager();

    void Initialize();
    void Update(uint32 deltaMs);

    // Live Server Character Queries
    bool IsLive() const { return m_isLive; }
    uint32 GetFrankGoId() const { return m_frankGoId; }
    uint64 GetFrankCharUID() const { return FRANK_CASTLE_UID; }
    FrankTacticalState GetTacticalState() const { return m_currentState; }
    std::string GetTacticalStateName() const;
    LocationVector GetCurrentLocation() const { return m_currentPos; }

    // Growth, Learning & Progression
    uint8 GetLevel() const { return m_level; }
    uint64 GetExperience() const { return m_exp; }
    uint64 GetRequiredExpForNextLevel() const;
    FrankMasteryRank GetRank() const;
    std::string GetRankTitle() const;
    void AwardExperience(uint64 amount);
    void CheckLevelUp();

    // Combat Stats Scaling
    uint32 GetMaxHealth() const;
    uint32 GetCurrentHealth() const { return m_currentHealth; }
    uint32 GetMaxInnerStrength() const;
    uint32 GetCurrentInnerStrength() const { return m_currentInnerStrength; }
    float GetBallisticDamageMultiplier() const;
    float GetDamageMitigation() const;
    float GetCriticalChance() const;

    // Tactical Actions & Reinforcement Learning (Q-Table)
    FrankCombatAction DecideCombatAction(uint32 targetGoId, bool targetIsAgent, float distance, bool targetIsSmith = false);
    FrankCombatAction DecideCombatActionEnhanced(uint32 targetGoId, HitListPriority priority, float distance, bool isEvasive);
    void RecordCombatOutcome(FrankCombatAction action, float damageDealt, float damageTaken, bool targetKilled, bool targetWasAgent, float distance = 1000.0f, bool targetWasSmith = false);
    void UpdateThreatMemory(uint32 targetGoId, const std::string& handle, bool isAgent, uint32 dmgDealt, uint32 dmgTaken);
    QTable& GetCombatQTable() { return m_combatQTable; }

    // Phase 1: War Journal & Dynamic Hit List Engine
    void AddWarJournalEntry(WarJournalEntryType type, const std::string& targetHandle, const std::string& summary, const std::string& notes, uint32 districtId = 0, LocationVector loc = LocationVector());
    std::vector<WarJournalEntry> GetRecentJournalEntries(size_t limit = 20) const;
    void SaveWarJournalToFile(const std::string& path = "WarJournal.json");
    bool LoadWarJournalFromFile(const std::string& path = "WarJournal.json");
    void DropWarJournalCassette(LocationVector loc, uint32 districtId, const std::string& districtName, const std::string& title, const std::string& notes);
    bool LootWarJournalCassette(uint32 cassetteId, uint32 playerGoId, std::string& outLore);
    const std::vector<WarJournalCassette>& GetCassettes() const { return m_cassettes; }

    void EvaluateHitList();
    void AddHitListTarget(uint32 goId, uint64 charUID, const std::string& handle, HitListPriority priority, float infamy, const std::string& crimeDossier, LocationVector loc, uint32 districtId, const std::string& districtName);
    std::vector<HitListEntry> GetHitList() const;
    HitListEntry* GetHitListTarget(uint32 goId);
    uint32 SelectNextHitListTarget();
    void StartStalkingTarget(uint32 targetGoId);

    // Phase 2: Microchip Tech & Construct Armory Logistics
    void InitializeTacticalCaches();
    const std::map<uint32, TacticalFieldCache>& GetFieldCaches() const { return m_fieldCaches; }
    TacticalFieldCache* GetNearestCache(float x, float z);
    TacticalFieldCache* GetFieldCache(uint32 cacheId);
    bool RestockFromFieldCache(uint32 cacheId);
    const MicrochipHardware& GetMicrochipHardware() const { return m_microchipTech; }
    MicrochipHardware& GetMicrochipHardware() { return m_microchipTech; }
    void AccessConstructArmory();

    // Phase 3: Safehouse Defense, Incursions & Siege Events
    const std::map<uint32, SafehouseNode>& GetSafehouses() const { return m_safehouses; }
    SafehouseNode* GetSafehouse(uint32 id);
    SafehouseNode* GetNearestSafehouse(float x, float z);
    SafehouseNode* GetNearestFortifiedSafehouse(float x, float z);
    bool CaptureSafehouse(uint32 safehouseId);
    bool FortifySafehouse(uint32 safehouseId);
    uint32 GetControlledSafehousesCount() const;
    uint32 GetSafehousesCaptured() const { return m_safehousesCaptured; }
    
    bool TriggerSafehouseSiege(uint32 safehouseId, SiegeFaction faction);
    void UpdateSafehouseSieges(uint32 deltaMs);
    SafehouseSiegeEvent* GetActiveSiege(uint32 safehouseId);
    bool IsSafehouseUnderSiege(uint32 safehouseId) const;
    void ResolveSiegeDefense(uint32 safehouseId, bool defendedSuccessfully);
    bool TriggerTripwireTrap(uint32 safehouseId, uint32 targetGoId);
    void UpdateAutomatedCIWSTurrets(uint32 deltaMs);
    void OnPlayerAssistedSiege(uint32 safehouseId, uint32 playerGoId);
    void OnPlayerAttemptedSafehouseBreach(uint32 safehouseId, uint32 playerGoId);

    // Phase 4: Syndicate Decapitation & Anti-Agent Guerilla Raids
    bool TriggerDistrictCleanSweep(uint32 districtId);
    bool IsCleanSweepActive() const { return m_cleanSweepActive; }
    void UpdateCleanSweep(uint32 deltaMs);
    void OnSyndicateLieutenantEliminated(uint32 targetGoId, const std::string& handle);

    // Stockpile Management
    const TacticalStockpile& GetCarriedStockpile() const { return m_carriedStockpile; }
    void ScavengeSupplies(uint32 ammo, uint32 emp, uint32 plates, uint32 kits, uint32 codes = 0);
    void DepositSuppliesToSafehouse(uint32 safehouseId);
    void RestockFromSafehouse(uint32 safehouseId);
    uint32 GetTotalGlobalStockpileVolume() const;

    // Agent Hunting & Anti-Hero Combat
    void HuntTargetAgent(uint32 agentGoId);
    void OnAgentDefeated(uint32 agentGoId, const std::string& agentName);
    uint32 GetTotalAgentsKilled() const { return m_totalAgentsKilled; }
    uint32 GetTotalHostilesDefeated() const { return m_totalHostilesDefeated; }

    // Smith Outbreak Response Operations
    bool IsSmithOutbreakActive() const;
    void RespondToSmithOutbreak();
    void InterceptSmithThreat(uint32 smithGoId);
    void OnSmithCloneEliminated(uint32 smithGoId, const std::string& handle);
    uint32 GetTotalSmithsPurged() const { return m_totalSmithsPurged; }
    uint32 ScanForSmithTargets();
    bool DeployAntiviralScrubber(uint32 safehouseId);
    void UpdateSafehouseScrubbers(uint32 deltaMs);
    bool IsTargetSmith() const { return m_targetIsSmith; }

    // Phase 5: Player Interaction, The Vigilante's Judgment & Radio Net (FM 88.3)
    int32 GetPlayerKarma(uint32 playerGoId) const;
    void ModifyPlayerKarma(uint32 playerGoId, const std::string& handle, int32 delta, const std::string& reason);
    bool IsPlayerMarkedForPunishment(uint32 playerGoId) const;
    void BroadcastRadioNet(const std::string& message, bool playSquelch = true);
    void BroadcastPirateTransmission(const std::string& message, bool shardWide = true);
    std::vector<RadioBroadcastRecord> GetRecentRadioTransmissions(size_t limit = 15) const;
    const std::vector<VigilanteContract>& GetAvailableContracts() const { return m_contracts; }
    bool AcceptVigilanteContract(uint32 contractId, uint32 playerGoId);
    bool CompleteVigilanteContract(uint32 contractId, uint32 playerGoId, std::string& outRewardText);

    // Phase 6: Environmental Combat Interactions
    bool TriggerEnvironmentalHazard(float x, float z, float radius, uint32& outHostilesDamaged);

    // Phase 7: Remaster Client Integration & Visual Polish
    VisualBattleCondition GetVisualCondition() const;
    std::string GetVisualConditionName() const;
    void UpdateVisualAppearance();
    std::string GenerateRemasterTelemetryJson() const;
    void TriggerSpatialBallisticAudio(float x, float y, float z, const std::string& weaponType);
    std::string GenerateStatusReport() const;

    // Administrative & Tactical Commands
    void ScanForAgentsAndThreats();
    void CommandOrderAttack(uint32 targetGoId);
    void CommandDeployToSafehouse(uint32 safehouseId);
    void TriggerFieldSurgeryAndRespawn();

private:
    void InitializeSafehouses();
    void InitializeContracts();
    void SpawnOrSyncLiveEntity();
    void UpdateStateAI(uint32 deltaMs);
    void ExecuteTacticalCombatTurn(uint32 deltaMs);
    void PerformSafehouseAssault(uint32 deltaMs);
    void PerformSafehouseFortification(uint32 deltaMs);
    void PerformFieldTriage(uint32 deltaMs);
    void PerformStalkingRecon(uint32 deltaMs);
    void PerformResupplyRun(uint32 deltaMs);
    void PerformSafehouseDefense(uint32 deltaMs);

    // Reinforcement learning state helpers
    std::string BuildQStateKey(bool isAgent, float distance, float healthPct, bool hasAmmo, bool isSmith = false);

    bool m_isLive;
    uint32 m_frankGoId;
    LocationVector m_currentPos;
    FrankTacticalState m_currentState;

    // Progression
    uint8 m_level;
    uint64 m_exp;
    uint32 m_currentHealth;
    uint32 m_currentInnerStrength;
    uint32 m_totalAgentsKilled;
    uint32 m_totalHostilesDefeated;
    uint32 m_safehousesCaptured;
    uint32 m_totalSmithsPurged;
    uint32 m_syndicateBossesEliminated;
    uint32 m_siegesRepelled;

    // Tactical Target & Navigation
    uint32 m_currentTargetGoId;
    bool m_targetIsAgent;
    bool m_targetIsSmith;
    HitListPriority m_currentTargetPriority;
    uint32 m_targetSafehouseId;
    uint32 m_targetCacheId;
    uint32 m_stateTimerMs;
    uint32 m_nextActionTimerMs;
    uint32 m_triageTimerMs;
    uint32 m_lastOutbreakCheckMs;
    uint32 m_lastHitListEvalMs;
    uint32 m_lastSiegeCheckMs;
    uint32 m_lastPublicBroadcastMs{0};
    uint32 m_lastArmoryAccessMs;
    uint32 m_cleanSweepDistrictId;
    bool m_cleanSweepActive;
    uint32 m_cleanSweepTimerMs;
    uint32 m_cleanSweepTargetRacketId{0};
    uint32 m_cleanSweepActionTimerMs{0};
    uint32 m_cleanSweepKills{0};

    // Systems
    TacticalStockpile m_carriedStockpile;
    MicrochipHardware m_microchipTech;
    std::map<uint32, SafehouseNode> m_safehouses;
    std::map<uint32, TacticalFieldCache> m_fieldCaches;
    std::unordered_map<uint32, ThreatMemoryRecord> m_threatMemory;
    QTable m_combatQTable;

    // Phase 1 collections
    std::deque<WarJournalEntry> m_warJournal;
    uint64 m_nextJournalId{1};
    std::vector<WarJournalCassette> m_cassettes;
    uint32 m_nextCassetteId{1};
    std::map<uint32, HitListEntry> m_hitList;

    // Phase 3 collections
    std::map<uint32, SafehouseSiegeEvent> m_activeSieges;

    // Phase 5 collections
    std::unordered_map<uint32, PlayerKarmaRecord> m_playerKarma;
    std::deque<RadioBroadcastRecord> m_radioTransmissions;
    uint32 m_nextBroadcastId{1};
    std::vector<VigilanteContract> m_contracts;

    mutable std::recursive_mutex m_mutex;
};

#define sFrankCastleMgr FrankCastleManager::getSingleton()

#endif // MXOEMU_FRANK_CASTLE_MANAGER_H

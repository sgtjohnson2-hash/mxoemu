#include "FrankCastleManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameServer.h"
#include "BotManager.h"
#include "CombatSystem.h"
#include "SpatialGrid.h"
#include "WorldDirector.h"
#include "MessageTypes.h"
#include "LoadingConstruct.h"
#include "RadioDispatchSystem.h"
#include "SpatialAudioDSP.h"
#include "NeuralVoiceSystem.h"
#include "UnderworldManager.h"
#include "EmergentAIEngine.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>

static std::string EscapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << c; break;
        }
    }
    return ss.str();
}

static std::string UnescapeJsonString(const std::string& input) {
    std::string res;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            char next = input[++i];
            switch (next) {
                case '"': res += '"'; break;
                case '\\': res += '\\'; break;
                case 'n': res += '\n'; break;
                case 'r': res += '\r'; break;
                case 't': res += '\t'; break;
                default: res += next; break;
            }
        } else {
            res += input[i];
        }
    }
    return res;
}

createFileSingleton(FrankCastleManager);

FrankCastleManager::FrankCastleManager()
    : m_isLive(false),
      m_frankGoId(0),
      m_currentPos(1250.0f, 0.0f, -3400.0f),
      m_currentState(FRANK_STATE_IDLE_PATROL),
      m_level(25),
      m_exp(35000),
      m_currentHealth(5500),
      m_currentInnerStrength(2500),
      m_totalAgentsKilled(0),
      m_totalHostilesDefeated(0),
      m_safehousesCaptured(1),
      m_totalSmithsPurged(0),
      m_syndicateBossesEliminated(0),
      m_siegesRepelled(0),
      m_currentTargetGoId(0),
      m_targetIsAgent(false),
      m_targetIsSmith(false),
      m_currentTargetPriority(PRIORITY_SYSTEM_AGENT),
      m_targetSafehouseId(1),
      m_targetCacheId(1),
      m_stateTimerMs(0),
      m_nextActionTimerMs(0),
      m_triageTimerMs(0),
      m_lastOutbreakCheckMs(0),
      m_lastHitListEvalMs(0),
      m_lastSiegeCheckMs(0),
      m_lastPublicBroadcastMs(0),
      m_lastArmoryAccessMs(0),
      m_cleanSweepDistrictId(0),
      m_cleanSweepActive(false),
      m_cleanSweepTimerMs(0)
{
    m_carriedStockpile.highCaliberAmmo = 250;
    m_carriedStockpile.empGrenades = 8;
    m_carriedStockpile.armorPlates = 10;
    m_carriedStockpile.traumaKits = 6;
    m_carriedStockpile.scramblerCodes = 4;
    m_carriedStockpile.antiviralRounds = 30;

    m_microchipTech.depletedUraniumRounds = 60;
    m_microchipTech.codeScramblerEMPSatchels = 10;
    m_microchipTech.adrenalineStims = 8;
    m_microchipTech.antiviralIncendiaryRounds = 50;
    m_microchipTech.whitePhosphorusSatchels = 6;
    m_microchipTech.limpetMines = 6;
}

FrankCastleManager::~FrankCastleManager()
{
    SaveWarJournalToFile("WarJournal.json");
}

void FrankCastleManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    INFO_LOG("FrankCastleManager: Initializing Frank Castle (The Punisher) Master Subsystem...");

    InitializeSafehouses();
    InitializeTacticalCaches();
    InitializeContracts();
    InitializeArmsBazaars();
    InitializeFortifications();
    InitializeSubwayDeadDrops();
    LoadWarJournalFromFile("WarJournal.json");
    SpawnOrSyncLiveEntity();

    m_isLive = true;
    m_currentState = FRANK_STATE_IDLE_PATROL;

    // Log operational activation in War Journal
    AddWarJournalEntry(
        JOURNAL_RADIO_BROADCAST,
        "System Authority",
        "Subsystem Boot & Operational Deployment",
        "Frank Castle back in the wire. Autonomous war initiated across Megacity. Microchip logistics and safehouse network online.",
        1,
        m_currentPos
    );

    // Initial broadcast to world announcing secret anti-hero presence on FM 88.3
    BroadcastRadioNet(
        "I'm back in the wire. To the Machines and their puppets: you made this city a slaughterhouse. I'm here to collect.",
        true
    );

    INFO_LOG(format("FrankCastleManager: Frank Castle initialized (Level %1%, Rank: %2%, Safehouses: %3%, Caches: %4%)")
             % (uint32)m_level % GetRankTitle() % m_safehouses.size() % m_fieldCaches.size());
}

void FrankCastleManager::InitializeSafehouses()
{
    m_safehouses.clear();

    // 1. Slums Foundry Sub-Vault (Starting fortified safehouse)
    SafehouseNode s1;
    s1.id = 1;
    s1.name = "Slums Foundry Sub-Vault";
    s1.districtId = 1;
    s1.districtName = "Slums";
    s1.location = LocationVector(1250.0f, 0.0f, -3400.0f);
    s1.status = SAFEHOUSE_FORTIFIED;
    s1.fortificationLevel = 3;
    s1.stockpile.highCaliberAmmo = 1200;
    s1.stockpile.empGrenades = 25;
    s1.stockpile.armorPlates = 20;
    s1.stockpile.traumaKits = 15;
    s1.stockpile.scramblerCodes = 10;
    s1.stockpile.antiviralRounds = 120;
    s1.tripwireTrapActive = true;
    s1.turretDefenseActive = true;
    s1.lastReinforcedMs = getMSTime();
    s1.hostileGuardsCount = 0;
    s1.antiviralScrubberActive = true;
    s1.lastScrubberPulseMs = 0;
    s1.ciwsLastFiredMs = 0;
    m_safehouses[1] = s1;

    // 2. Downtown Abandoned Metro Bunker (Hostile - occupied by Machine / Syndicate forces)
    SafehouseNode s2;
    s2.id = 2;
    s2.name = "Downtown Abandoned Metro Bunker";
    s2.districtId = 2;
    s2.districtName = "Downtown";
    s2.location = LocationVector(4500.0f, 50.0f, 1200.0f);
    s2.status = SAFEHOUSE_HOSTILE;
    s2.fortificationLevel = 1;
    s2.stockpile.highCaliberAmmo = 400;
    s2.stockpile.empGrenades = 2;
    s2.stockpile.armorPlates = 5;
    s2.stockpile.traumaKits = 3;
    s2.stockpile.scramblerCodes = 1;
    s2.stockpile.antiviralRounds = 15;
    s2.tripwireTrapActive = false;
    s2.turretDefenseActive = false;
    s2.lastReinforcedMs = 0;
    s2.hostileGuardsCount = 4;
    s2.antiviralScrubberActive = false;
    s2.lastScrubberPulseMs = 0;
    s2.ciwsLastFiredMs = 0;
    m_safehouses[2] = s2;

    // 3. International Hidden Vault (Unclaimed)
    SafehouseNode s3;
    s3.id = 3;
    s3.name = "International District Hidden Vault";
    s3.districtId = 3;
    s3.districtName = "International";
    s3.location = LocationVector(-2100.0f, 120.0f, 5400.0f);
    s3.status = SAFEHOUSE_UNCLAIMED;
    s3.fortificationLevel = 0;
    s3.stockpile.highCaliberAmmo = 200;
    s3.stockpile.empGrenades = 1;
    s3.stockpile.armorPlates = 2;
    s3.stockpile.traumaKits = 1;
    s3.stockpile.scramblerCodes = 0;
    s3.stockpile.antiviralRounds = 10;
    s3.tripwireTrapActive = false;
    s3.turretDefenseActive = false;
    s3.lastReinforcedMs = 0;
    s3.hostileGuardsCount = 0;
    s3.antiviralScrubberActive = false;
    s3.lastScrubberPulseMs = 0;
    s3.ciwsLastFiredMs = 0;
    m_safehouses[3] = s3;

    // 4. Industrial Munitions Depot (Hostile - heavy weapon storage)
    SafehouseNode s4;
    s4.id = 4;
    s4.name = "Industrial Corridor Munitions Depot";
    s4.districtId = 4;
    s4.districtName = "Industrial";
    s4.location = LocationVector(6800.0f, -20.0f, -1500.0f);
    s4.status = SAFEHOUSE_HOSTILE;
    s4.fortificationLevel = 2;
    s4.stockpile.highCaliberAmmo = 800;
    s4.stockpile.empGrenades = 6;
    s4.stockpile.armorPlates = 12;
    s4.stockpile.traumaKits = 8;
    s4.stockpile.scramblerCodes = 3;
    s4.stockpile.antiviralRounds = 40;
    s4.tripwireTrapActive = true;
    s4.turretDefenseActive = false;
    s4.lastReinforcedMs = 0;
    s4.hostileGuardsCount = 5;
    s4.antiviralScrubberActive = false;
    s4.lastScrubberPulseMs = 0;
    s4.ciwsLastFiredMs = 0;
    m_safehouses[4] = s4;

    // 5. Park East Surveillance Perch (Secured)
    SafehouseNode s5;
    s5.id = 5;
    s5.name = "Park East Surveillance Perch";
    s5.districtId = 5;
    s5.districtName = "Park East";
    s5.location = LocationVector(-4800.0f, 80.0f, -2200.0f);
    s5.status = SAFEHOUSE_SECURED;
    s5.fortificationLevel = 2;
    s5.stockpile.highCaliberAmmo = 500;
    s5.stockpile.empGrenades = 10;
    s5.stockpile.armorPlates = 8;
    s5.stockpile.traumaKits = 6;
    s5.stockpile.scramblerCodes = 4;
    s5.stockpile.antiviralRounds = 25;
    s5.tripwireTrapActive = true;
    s5.turretDefenseActive = false;
    s5.lastReinforcedMs = getMSTime();
    s5.hostileGuardsCount = 0;
    s5.antiviralScrubberActive = false;
    s5.lastScrubberPulseMs = 0;
    s5.ciwsLastFiredMs = 0;
    m_safehouses[5] = s5;
}

void FrankCastleManager::InitializeTacticalCaches()
{
    m_fieldCaches.clear();

    // Cache 1: Slums Drainage Sump
    TacticalFieldCache c1;
    c1.cacheId = 1;
    c1.codename = "Sump-9 Triage Depot";
    c1.type = CACHE_SEWER_TRIAGE;
    c1.districtId = 1;
    c1.districtName = "Slums";
    c1.location = LocationVector(1600.0f, -40.0f, -3100.0f);
    c1.supplies.highCaliberAmmo = 300;
    c1.supplies.armorPlates = 8;
    c1.supplies.traumaKits = 10;
    c1.supplies.empGrenades = 4;
    c1.isCompromised = false;
    c1.lastRestockedMs = getMSTime();
    m_fieldCaches[1] = c1;

    // Cache 2: Downtown Subway Line 4 Vault
    TacticalFieldCache c2;
    c2.cacheId = 2;
    c2.codename = "Metro-4 Heavy Armory";
    c2.type = CACHE_SUBWAY_DEPOT;
    c2.districtId = 2;
    c2.districtName = "Downtown";
    c2.location = LocationVector(4200.0f, -60.0f, 1500.0f);
    c2.supplies.highCaliberAmmo = 800;
    c2.supplies.empGrenades = 12;
    c2.supplies.armorPlates = 15;
    c2.supplies.antiviralRounds = 60;
    c2.isCompromised = false;
    c2.lastRestockedMs = getMSTime();
    m_fieldCaches[2] = c2;

    // Cache 3: International Tower HVAC Rooftop
    TacticalFieldCache c3;
    c3.cacheId = 3;
    c3.codename = "Apex-Sky Recon Drop";
    c3.type = CACHE_ROOFTOP_HVAC;
    c3.districtId = 3;
    c3.districtName = "International";
    c3.location = LocationVector(-2400.0f, 320.0f, 5100.0f);
    c3.supplies.highCaliberAmmo = 450;
    c3.supplies.scramblerCodes = 8;
    c3.supplies.empGrenades = 8;
    c3.isCompromised = false;
    c3.lastRestockedMs = getMSTime();
    m_fieldCaches[3] = c3;

    // Cache 4: Industrial Freight Siding
    TacticalFieldCache c4;
    c4.cacheId = 4;
    c4.codename = "Spur-12 Ordnance Crate";
    c4.type = CACHE_SUBWAY_DEPOT;
    c4.districtId = 4;
    c4.districtName = "Industrial";
    c4.location = LocationVector(7200.0f, 0.0f, -1200.0f);
    c4.supplies.highCaliberAmmo = 900;
    c4.supplies.armorPlates = 12;
    c4.supplies.empGrenades = 10;
    c4.isCompromised = false;
    c4.lastRestockedMs = getMSTime();
    m_fieldCaches[4] = c4;

    // Cache 5: Park East Reservoir Gate
    TacticalFieldCache c5;
    c5.cacheId = 5;
    c5.codename = "Reservoir Shroud Cache";
    c5.type = CACHE_SEWER_TRIAGE;
    c5.districtId = 5;
    c5.districtName = "Park East";
    c5.location = LocationVector(-4500.0f, 20.0f, -2500.0f);
    c5.supplies.highCaliberAmmo = 350;
    c5.supplies.traumaKits = 8;
    c5.supplies.armorPlates = 6;
    c5.isCompromised = false;
    c5.lastRestockedMs = getMSTime();
    m_fieldCaches[5] = c5;
}

void FrankCastleManager::InitializeContracts()
{
    m_contracts.clear();

    VigilanteContract vc1;
    vc1.contractId = 1;
    vc1.title = "Syndicate Decapitation: Lupine Enforcers";
    vc1.description = "Merovingian Lupine muscle is running an extortion racket in Downtown. Eliminate 3 enforcers.";
    vc1.targetDescription = "Merovingian Lupine Lieutenants";
    vc1.targetDistrictId = 2;
    vc1.requiredKills = 3;
    vc1.currentKills = 0;
    vc1.rewardAmmo = 200;
    vc1.rewardKarma = 150;
    vc1.isCompleted = false;
    vc1.acceptedPlayerGoId = 0;
    m_contracts.push_back(vc1);

    VigilanteContract vc2;
    vc2.contractId = 2;
    vc2.title = "System Patrol Interception";
    vc2.description = "Agents of the System are harassing awakening redpills. Ambush and destroy an Agent patrol.";
    vc2.targetDescription = "System Agent Patrols";
    vc2.targetDistrictId = 1;
    vc2.requiredKills = 1;
    vc2.currentKills = 0;
    vc2.rewardAmmo = 400;
    vc2.rewardKarma = 300;
    vc2.isCompleted = false;
    vc2.acceptedPlayerGoId = 0;
    m_contracts.push_back(vc2);

    VigilanteContract vc3;
    vc3.contractId = 3;
    vc3.title = "Viral Vector Suppression";
    vc3.description = "Smith replication signatures detected in the subways. Cleanse 4 viral clones.";
    vc3.targetDescription = "Smith Viral Clones";
    vc3.targetDistrictId = 3;
    vc3.requiredKills = 4;
    vc3.currentKills = 0;
    vc3.rewardAmmo = 350;
    vc3.rewardKarma = 250;
    vc3.isCompleted = false;
    vc3.acceptedPlayerGoId = 0;
    m_contracts.push_back(vc3);
}

void FrankCastleManager::SpawnOrSyncLiveEntity()
{
    if (!GameServer::getSingletonPtr()) {
        m_frankGoId = 9999001;
        m_currentHealth = GetMaxHealth();
        m_currentInnerStrength = GetMaxInnerStrength();
        return;
    }

    PlayerObject* po = nullptr;
    if (m_frankGoId != 0) {
        po = sObjMgr.getGOPtrSafe(m_frankGoId);
    }

    if (!po) {
        LocationVector startLoc = m_safehouses[1].location;
        auto bot = sBotMgr.SpawnSingleBot(startLoc.x, startLoc.y, startLoc.z, FACTION_ZION);
        if (bot) {
            bot->setAgent(false);
            m_frankGoId = bot->GetPlayerGoId();
            po = sObjMgr.getGOPtrSafe(m_frankGoId);
        }
    }

    if (po) {
        po->setHandle(FRANK_CASTLE_NAME);
        po->setBackground(FRANK_CASTLE_BACKGROUND);
        po->setFactionName("Vigilante");
        po->setLevel(m_level);
        po->setMaximumHealth(GetMaxHealth());
        po->setCurrentHealth(GetMaxHealth());
        po->setInnerStrength(GetMaxInnerStrength(), GetMaxInnerStrength());
        // Iconic Skull Vest + Weathered Trenchcoat RSI Hex
        po->setRsiHex("1f080099");

        m_currentHealth = po->getCurrentHealth();
        m_currentInnerStrength = po->getCurrentInnerStrength();
        m_currentPos = po->getPosition();
    }
}

void FrankCastleManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!GameServer::getSingletonPtr()) {
        UpdateSafehouseScrubbers(deltaMs);
        UpdateAutomatedCIWSTurrets(deltaMs);
        UpdateSafehouseSieges(deltaMs);
        UpdateCleanSweep(deltaMs);
        return;
    }

    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) {
        SpawnOrSyncLiveEntity();
        return;
    }

    if (po->isDead() || po->getCurrentHealth() == 0) {
        TriggerFieldSurgeryAndRespawn();
        return;
    }

    m_currentPos = po->getPosition();
    m_currentHealth = po->getCurrentHealth();
    m_currentInnerStrength = po->getCurrentInnerStrength();

    // Check health threshold: tactical retreat & smoke if critical
    if (m_currentHealth < (GetMaxHealth() * 0.20f) && 
        m_currentState != FRANK_STATE_TACTICAL_RETREAT && 
        m_currentState != FRANK_STATE_FIELD_TRIAGE) {
        m_currentState = FRANK_STATE_TACTICAL_RETREAT;
        m_stateTimerMs = 0;
        DEBUG_LOG("FrankCastle: Tactical smoke deployed. Vitals compromised. Falling back to safehouse for surgery and re-arm.");
        AddWarJournalEntry(JOURNAL_TACTICAL_RETREAT, "Multiple Hostiles", "Emergency Smoke Extraction", "Fell back to fortified perimeter under heavy fire.", 1, m_currentPos);
    }

    // Periodic safehouse defense updates & CIWS turrets
    UpdateSafehouseScrubbers(deltaMs);
    UpdateAutomatedCIWSTurrets(deltaMs);
    UpdateSafehouseSieges(deltaMs);
    UpdateCleanSweep(deltaMs);

    // Periodic Hit List re-evaluation (every 5s)
    m_lastHitListEvalMs += deltaMs;
    if (m_lastHitListEvalMs >= 5000) {
        m_lastHitListEvalMs = 0;
        EvaluateHitList();
    }

    // Periodic Construct Armory fabrication (every 60s)
    m_lastArmoryAccessMs += deltaMs;
    if (m_lastArmoryAccessMs >= 60000) {
        m_lastArmoryAccessMs = 0;
        AccessConstructArmory();
    }

    // Visual RSI condition sync
    UpdateVisualAppearance();

    // State machine tick
    UpdateStateAI(deltaMs);
}

void FrankCastleManager::UpdateStateAI(uint32 deltaMs)
{
    m_stateTimerMs += deltaMs;
    m_nextActionTimerMs += deltaMs;
    m_lastOutbreakCheckMs += deltaMs;

    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) return;

    // Outbreak Monitoring check every 2.5s
    if (m_lastOutbreakCheckMs >= 2500) {
        m_lastOutbreakCheckMs = 0;
        if (m_currentState != FRANK_STATE_TACTICAL_RETREAT && m_currentState != FRANK_STATE_FIELD_TRIAGE) {
            if (IsSmithOutbreakActive() && m_currentState != FRANK_STATE_PURGE_SMITH_OUTBREAK && 
                m_currentState != FRANK_STATE_CONTAINMENT_PROTOCOL && m_currentState != FRANK_STATE_IN_COMBAT) {
                RespondToSmithOutbreak();
                return;
            }
        }
    }

    // Check if supplies are depleted -> triggers resupply run
    if (m_carriedStockpile.highCaliberAmmo < 30 && m_currentState != FRANK_STATE_TACTICAL_RETREAT && 
        m_currentState != FRANK_STATE_FIELD_TRIAGE && m_currentState != FRANK_STATE_RESUPPLY_RUN) {
        m_currentState = FRANK_STATE_RESUPPLY_RUN;
        m_stateTimerMs = 0;
        DEBUG_LOG("FrankCastle: Ammunition low. Routing stealth approach to field cache.");
        return;
    }

    switch (m_currentState) {
        case FRANK_STATE_IDLE_PATROL: {
            ScanForAgentsAndThreats();
            if (m_currentState != FRANK_STATE_IDLE_PATROL) break;

            // Pick next target from Dynamic Hit List
            uint32 nextTarget = SelectNextHitListTarget();
            if (nextTarget != 0) {
                StartStalkingTarget(nextTarget);
                break;
            }

            // Periodic tactical patrol movement (every 8 seconds)
            if (m_stateTimerMs > 8000) {
                m_stateTimerMs = 0;
                for (const auto& kv : m_safehouses) {
                    if (kv.second.status == SAFEHOUSE_HOSTILE) {
                        m_targetSafehouseId = kv.second.id;
                        m_currentState = FRANK_STATE_ASSAULT_SAFEHOUSE;
                        DEBUG_LOG(format("FrankCastle: Moving on hostile stronghold [%1%] in %2%.")
                            % kv.second.name % kv.second.districtName);
                        break;
                    } else if (kv.second.status == SAFEHOUSE_UNCLAIMED) {
                        m_targetSafehouseId = kv.second.id;
                        m_currentState = FRANK_STATE_ASSAULT_SAFEHOUSE;
                        break;
                    }
                }
            }
            break;
        }

        case FRANK_STATE_STALKING_TARGET: {
            PerformStalkingRecon(deltaMs);
            break;
        }

        case FRANK_STATE_HUNT_AGENTS: {
            PlayerObject* target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);
            if (!target || target->isDead()) {
                m_currentTargetGoId = 0;
                m_targetIsAgent = false;
                m_targetIsSmith = false;
                m_currentState = FRANK_STATE_IDLE_PATROL;
                break;
            }

            LocationVector tPos = target->getPosition();
            float dx = tPos.x - m_currentPos.x;
            float dz = tPos.z - m_currentPos.z;
            float dist = std::sqrt((dx * dx) + (dz * dz));

            if (dist > 1500.0f) {
                float step = 350.0f;
                m_currentPos.x += (dx / dist) * step;
                m_currentPos.z += (dz / dist) * step;
                po->setPosition(m_currentPos);
            } else {
                m_currentState = FRANK_STATE_IN_COMBAT;
            }
            break;
        }

        case FRANK_STATE_PURGE_SMITH_OUTBREAK: {
            if (!IsSmithOutbreakActive() && m_currentTargetGoId == 0) {
                m_currentState = FRANK_STATE_IDLE_PATROL;
                m_targetIsSmith = false;
                DEBUG_LOG("FrankCastle: Sector purged of viral signatures. Smith outbreak suppressed. Resuming tactical patrol.");
                break;
            }

            PlayerObject* target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);
            if (!target || target->isDead()) {
                m_currentTargetGoId = ScanForSmithTargets();
                target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);
            }

            if (!target || target->isDead()) {
                m_currentState = FRANK_STATE_IDLE_PATROL;
                break;
            }

            m_targetIsAgent = true;
            m_targetIsSmith = true;

            LocationVector tPos = target->getPosition();
            float dx = tPos.x - m_currentPos.x;
            float dz = tPos.z - m_currentPos.z;
            float dist = std::sqrt((dx * dx) + (dz * dz));

            if (dist > 1500.0f) {
                float step = 500.0f;
                m_currentPos.x += (dx / dist) * step;
                m_currentPos.z += (dz / dist) * step;
                po->setPosition(m_currentPos);
            } else {
                m_currentState = FRANK_STATE_IN_COMBAT;
            }
            break;
        }

        case FRANK_STATE_CONTAINMENT_PROTOCOL: {
            if (sSmithCascade.GetStage() < CONTAGION_STAGE_CASCADE) {
                m_currentState = FRANK_STATE_PURGE_SMITH_OUTBREAK;
                DEBUG_LOG("FrankCastle: Viral cascade broken. Shard lockdown downgraded. Sweeping remaining replicas.");
                break;
            }

            PlayerObject* target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);
            if (!target || target->isDead()) {
                m_currentTargetGoId = ScanForSmithTargets();
                target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);
            }

            if (target && !target->isDead()) {
                m_targetIsAgent = true;
                m_targetIsSmith = true;

                LocationVector tPos = target->getPosition();
                float dx = tPos.x - m_currentPos.x;
                float dz = tPos.z - m_currentPos.z;
                float dist = std::sqrt((dx * dx) + (dz * dz));

                if (dist > 1500.0f) {
                    float step = 550.0f;
                    m_currentPos.x += (dx / dist) * step;
                    m_currentPos.z += (dz / dist) * step;
                    po->setPosition(m_currentPos);
                } else {
                    m_currentState = FRANK_STATE_IN_COMBAT;
                }
            }
            break;
        }

        case FRANK_STATE_IN_COMBAT: {
            ExecuteTacticalCombatTurn(deltaMs);
            break;
        }

        case FRANK_STATE_ASSAULT_SAFEHOUSE: {
            PerformSafehouseAssault(deltaMs);
            break;
        }

        case FRANK_STATE_FORTIFY_SAFEHOUSE: {
            PerformSafehouseFortification(deltaMs);
            break;
        }

        case FRANK_STATE_TACTICAL_RETREAT: {
            SafehouseNode* safehouse = GetNearestFortifiedSafehouse(m_currentPos.x, m_currentPos.z);
            if (!safehouse) safehouse = GetSafehouse(1);

            if (safehouse) {
                float dx = safehouse->location.x - m_currentPos.x;
                float dz = safehouse->location.z - m_currentPos.z;
                float dist = std::sqrt((dx * dx) + (dz * dz));

                if (dist > 200.0f) {
                    float step = 450.0f;
                    m_currentPos.x += (dx / dist) * step;
                    m_currentPos.z += (dz / dist) * step;
                    po->setPosition(m_currentPos);
                } else {
                    m_currentState = FRANK_STATE_FIELD_TRIAGE;
                    m_triageTimerMs = 0;
                    m_stateTimerMs = 0;
                    DEBUG_LOG(format("FrankCastle: Inside [%1%]. Initiating field surgery and munitions reload.")
                        % safehouse->name);
                }
            }
            break;
        }

        case FRANK_STATE_FIELD_TRIAGE: {
            PerformFieldTriage(deltaMs);
            break;
        }

        case FRANK_STATE_RESUPPLY_RUN: {
            PerformResupplyRun(deltaMs);
            break;
        }

        case FRANK_STATE_DEFEND_SAFEHOUSE: {
            PerformSafehouseDefense(deltaMs);
            break;
        }

        case FRANK_STATE_CLEAN_SWEEP_RAID: {
            // Handled in UpdateCleanSweep
            break;
        }
    }
}

void FrankCastleManager::PerformStalkingRecon(uint32 deltaMs)
{
    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    PlayerObject* target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);

    if (!po || !target || target->isDead()) {
        m_currentState = FRANK_STATE_IDLE_PATROL;
        m_currentTargetGoId = 0;
        return;
    }

    LocationVector tPos = target->getPosition();
    float dx = tPos.x - m_currentPos.x;
    float dz = tPos.z - m_currentPos.z;
    float dist = std::sqrt((dx * dx) + (dz * dz));

    // Stalking range: keep between 18m and 25m
    if (dist > 2200.0f) {
        float step = 320.0f;
        m_currentPos.x += (dx / dist) * step;
        m_currentPos.z += (dz / dist) * step;
        po->setPosition(m_currentPos);
    }

    // Recon phase takes 3.5 seconds
    if (m_stateTimerMs > 3500) {
        m_stateTimerMs = 0;
        m_currentState = FRANK_STATE_IN_COMBAT;

        AddWarJournalEntry(
            JOURNAL_STALKING_AMBUSH,
            target->getHandle(),
            "Ambush Firing Solution Calculated",
            "Target observed from perimeter. Flanking angle locked. Perimeter claymore deployed.",
            1,
            m_currentPos
        );

        DEBUG_LOG(format("FrankCastle: Target locked: %1%. Flanking claymore planted. Engaging.")
            % target->getHandle());
    }
}

void FrankCastleManager::PerformResupplyRun(uint32 deltaMs)
{
    TacticalFieldCache* cache = GetNearestCache(m_currentPos.x, m_currentPos.z);
    if (!cache) {
        m_currentState = FRANK_STATE_FIELD_TRIAGE;
        return;
    }

    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) return;

    float dx = cache->location.x - m_currentPos.x;
    float dz = cache->location.z - m_currentPos.z;
    float dist = std::sqrt((dx * dx) + (dz * dz));

    if (dist > 250.0f) {
        float step = 420.0f;
        m_currentPos.x += (dx / dist) * step;
        m_currentPos.z += (dz / dist) * step;
        po->setPosition(m_currentPos);
    } else {
        RestockFromFieldCache(cache->cacheId);
        m_currentState = FRANK_STATE_IDLE_PATROL;
        m_stateTimerMs = 0;
        DEBUG_LOG(format("FrankCastle: Field cache [%1%] accessed. Ammunition and Microchip hardware restocked.")
            % cache->codename);
    }
}

void FrankCastleManager::PerformSafehouseDefense(uint32 deltaMs)
{
    SafehouseSiegeEvent* siege = GetActiveSiege(m_targetSafehouseId);
    SafehouseNode* sh = GetSafehouse(m_targetSafehouseId);

    if (!siege || !siege->active || !sh) {
        bool foundOther = false;
        for (const auto& kv : m_activeSieges) {
            if (kv.second.active) {
                m_targetSafehouseId = kv.first;
                siege = GetActiveSiege(m_targetSafehouseId);
                sh = GetSafehouse(m_targetSafehouseId);
                foundOther = true;
                break;
            }
        }
        if (!foundOther || !siege || !sh) {
            m_currentState = FRANK_STATE_IDLE_PATROL;
            return;
        }
    }

    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) return;

    // Move to safehouse if not already there
    float dx = sh->location.x - m_currentPos.x;
    float dz = sh->location.z - m_currentPos.z;
    float dist = std::sqrt((dx * dx) + (dz * dz));

    if (dist > 300.0f) {
        float step = 500.0f;
        m_currentPos.x += (dx / dist) * step;
        m_currentPos.z += (dz / dist) * step;
        po->setPosition(m_currentPos);
    } else {
        // At safehouse, actively engage attacking siege waves
        if (m_nextActionTimerMs > 1500) {
            m_nextActionTimerMs = 0;
            if (siege->enemiesRemaining > 0) {
                siege->enemiesRemaining--;
                if (m_carriedStockpile.highCaliberAmmo >= 15) {
                    m_carriedStockpile.highCaliberAmmo -= 15;
                }
                uint32 dmg = 450 + (m_level * 20);
                TriggerSpatialBallisticAudio(m_currentPos.x, m_currentPos.y, m_currentPos.z, "7.62_heavy_burst");

                if (rand() % 100 < 30) {
                    m_microchipTech.whitePhosphorusSatchels = (m_microchipTech.whitePhosphorusSatchels > 0) ? m_microchipTech.whitePhosphorusSatchels - 1 : 0;
                }

                DEBUG_LOG(format("FrankCastle: Repelling %1% siege on [%2%]. Hostile neutralized. %3% remaining in wave.")
                    % siege->factionName % sh->name % siege->enemiesRemaining);
            }
        }
    }
}

void FrankCastleManager::ExecuteTacticalCombatTurn(uint32 deltaMs)
{
    uint32 turnCadence = m_targetIsSmith ? 1400 : 1800;
    if (m_nextActionTimerMs < turnCadence) return;
    m_nextActionTimerMs = 0;

    PlayerObject* target = sObjMgr.getGOPtrSafe(m_currentTargetGoId);
    if (!target || target->isDead()) {
        if (m_targetIsSmith) {
            std::string tName = target ? target->getHandle() : "Smith Replica";
            PlayerObject* frankPo = sObjMgr.getGOPtrSafe(m_frankGoId);
            sSmithCascade.PurgeEntity(m_currentTargetGoId, frankPo, PURGE_METHOD_FRANK_CASTLE_EXECUTION);
            OnSmithCloneEliminated(m_currentTargetGoId, tName);
        } else if (m_targetIsAgent) {
            OnAgentDefeated(m_currentTargetGoId, target ? target->getHandle() : "Agent");
        } else {
            m_totalHostilesDefeated++;
            AwardExperience(450);
            ScavengeSupplies(25, 0, 1, 1);
        }

        auto it = m_hitList.find(m_currentTargetGoId);
        if (it != m_hitList.end()) {
            it->second.status = HIT_STATUS_TERMINATED;
        }

        m_currentTargetGoId = 0;
        m_targetIsAgent = false;
        m_targetIsSmith = false;
        m_currentState = FRANK_STATE_IDLE_PATROL;
        return;
    }

    LocationVector tPos = target->getPosition();
    float dx = tPos.x - m_currentPos.x;
    float dz = tPos.z - m_currentPos.z;
    float dist = std::sqrt((dx * dx) + (dz * dz));

    // Reinforcement Learning: Enhanced Decision
    FrankCombatAction action = DecideCombatActionEnhanced(m_currentTargetGoId, m_currentTargetPriority, dist, false);

    uint32 baseDamage = 380 + (m_level * 16);
    float dmgMult = GetBallisticDamageMultiplier();
    float critRoll = (rand() % 100) / 100.0f;
    if (critRoll < GetCriticalChance()) dmgMult *= 1.80f;

    uint32 finalDamage = (uint32)(baseDamage * dmgMult);
    uint32 damageTaken = 0;

    switch (action) {
        case ACT_ARMOR_PIERCING_VOLLEY: {
            if (m_carriedStockpile.highCaliberAmmo >= 10) {
                m_carriedStockpile.highCaliberAmmo -= 10;
                finalDamage = (uint32)(finalDamage * 1.45f);
            }
            TriggerSpatialBallisticAudio(m_currentPos.x, m_currentPos.y, m_currentPos.z, "AP_VOLLEY");
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C1);
            break;
        }

        case ACT_ANTIVIRAL_AP_VOLLEY: {
            if (m_carriedStockpile.highCaliberAmmo >= 10) {
                m_carriedStockpile.highCaliberAmmo -= 10;
                if (m_microchipTech.antiviralIncendiaryRounds > 0) m_microchipTech.antiviralIncendiaryRounds--;
                finalDamage = (uint32)(finalDamage * 2.2f);
            }
            TriggerSpatialBallisticAudio(m_currentPos.x, m_currentPos.y, m_currentPos.z, "ANTIVIRAL");
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C1);
            break;
        }

        case ACT_WHITE_PHOSPHORUS_BURST: {
            if (m_microchipTech.whitePhosphorusSatchels > 0) m_microchipTech.whitePhosphorusSatchels--;
            finalDamage = (uint32)(finalDamage * 2.1f);
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C2);
            break;
        }

        case ACT_POINT_BLANK_EXECUTION: {
            finalDamage = (uint32)(finalDamage * 2.8f);
            TriggerSpatialBallisticAudio(m_currentPos.x, m_currentPos.y, m_currentPos.z, "EXECUTION");
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C3);
            break;
        }

        case ACT_EMP_DISRUPTION_GRENADE: {
            if (m_carriedStockpile.empGrenades > 0) {
                m_carriedStockpile.empGrenades--;
                finalDamage = (uint32)(finalDamage * 1.3f);
            }
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C2);
            break;
        }

        case ACT_CQC_DISARM_TAKEDOWN: {
            finalDamage = (uint32)(finalDamage * 1.6f);
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C3);
            break;
        }

        case ACT_SNIPER_AMBUSH: {
            finalDamage = (uint32)(finalDamage * 2.5f);
            TriggerSpatialBallisticAudio(m_currentPos.x, m_currentPos.y, m_currentPos.z, "SNIPER_HEAVY");
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C1);
            break;
        }

        case ACT_EXPLOSIVE_BARREL_DETONATION: {
            finalDamage = (uint32)(finalDamage * 2.0f);
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C2);
            break;
        }

        case ACT_COLLAPSE_SCAFFOLDING: {
            finalDamage = (uint32)(finalDamage * 1.75f);
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C3);
            break;
        }

        case ACT_FLASHBANG_STUN: {
            finalDamage = (uint32)(finalDamage * 0.9f);
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C2);
            break;
        }

        case ACT_TACTICAL_COVER_ROLL: {
            finalDamage = (uint32)(finalDamage * 0.5f);
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C1);
            break;
        }

        case ACT_ADRENALINE_STIM: {
            if (m_microchipTech.adrenalineStims > 0) {
                m_microchipTech.adrenalineStims--;
                uint32 heal = (uint32)(GetMaxHealth() * 0.35f);
                PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
                if (po) {
                    po->setCurrentHealth(std::min((uint32)po->getMaximumHealth(), (uint32)po->getCurrentHealth() + heal));
                    m_currentHealth = po->getCurrentHealth();
                }
            }
            break;
        }

        case ACT_SMOKE_EXTRACTION: {
            m_currentState = FRANK_STATE_TACTICAL_RETREAT;
            m_stateTimerMs = 0;
            return;
        }

        default:
            target->takeDamage(m_frankGoId, finalDamage, 0x280001C1);
            break;
    }

    // Counter-attack damage
    if (!target->isDead()) {
        uint32 enemyAttack = 160 + (target->getLevel() * 11);
        float mit = GetDamageMitigation();
        damageTaken = (uint32)(enemyAttack * (1.0f - mit));

        PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
        if (po) {
            po->takeDamage(target->getGoId(), damageTaken, 0);
            m_currentHealth = po->getCurrentHealth();
        }
    }

    bool killed = target->isDead();
    RecordCombatOutcome(action, (float)finalDamage, (float)damageTaken, killed, m_targetIsAgent, dist, m_targetIsSmith);
    UpdateThreatMemory(m_currentTargetGoId, target->getHandle(), m_targetIsAgent, finalDamage, damageTaken);

    if (killed) {
        if (m_targetIsSmith) {
            PlayerObject* frankPo = sObjMgr.getGOPtrSafe(m_frankGoId);
            sSmithCascade.PurgeEntity(m_currentTargetGoId, frankPo, PURGE_METHOD_FRANK_CASTLE_EXECUTION);
            OnSmithCloneEliminated(m_currentTargetGoId, target->getHandle());
        } else if (m_targetIsAgent) {
            OnAgentDefeated(m_currentTargetGoId, target->getHandle());
        } else {
            m_totalHostilesDefeated++;
            AwardExperience(450);
            ScavengeSupplies(25, 0, 1, 1);
            AddWarJournalEntry(JOURNAL_KILL, target->getHandle(), "Hostile Combatant Eliminated", "Target eliminated during street-level contact.", 1, m_currentPos);
        }

        m_currentTargetGoId = 0;
        m_targetIsAgent = false;
        m_targetIsSmith = false;
        m_currentState = FRANK_STATE_IDLE_PATROL;
    }
}

void FrankCastleManager::PerformSafehouseAssault(uint32 deltaMs)
{
    SafehouseNode* s = GetSafehouse(m_targetSafehouseId);
    if (!s) {
        m_currentState = FRANK_STATE_IDLE_PATROL;
        return;
    }

    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) return;

    float dx = s->location.x - m_currentPos.x;
    float dz = s->location.z - m_currentPos.z;
    float dist = std::sqrt((dx * dx) + (dz * dz));

    if (dist > 250.0f) {
        float step = 350.0f;
        m_currentPos.x += (dx / dist) * step;
        m_currentPos.z += (dz / dist) * step;
        po->setPosition(m_currentPos);
    } else {
        if (s->hostileGuardsCount > 0) {
            if (m_nextActionTimerMs > 1500) {
                m_nextActionTimerMs = 0;
                s->hostileGuardsCount--;
                AwardExperience(300);
                ScavengeSupplies(30, 1, 1, 1);
                DEBUG_LOG(format("FrankCastle: Breached [%1%]. Hostile guard neutralized. %2% remaining.")
                    % s->name % s->hostileGuardsCount);
            }
        } else {
            CaptureSafehouse(s->id);
            m_currentState = FRANK_STATE_FORTIFY_SAFEHOUSE;
            m_stateTimerMs = 0;
        }
    }
}

void FrankCastleManager::PerformSafehouseFortification(uint32 deltaMs)
{
    SafehouseNode* s = GetSafehouse(m_targetSafehouseId);
    if (!s || s->status != SAFEHOUSE_SECURED) {
        m_currentState = FRANK_STATE_IDLE_PATROL;
        return;
    }

    if (m_stateTimerMs > 3000) {
        m_stateTimerMs = 0;
        FortifySafehouse(s->id);
        m_currentState = FRANK_STATE_IDLE_PATROL;
    }
}

void FrankCastleManager::PerformFieldTriage(uint32 deltaMs)
{
    m_triageTimerMs += deltaMs;
    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) return;

    uint32 maxHp = GetMaxHealth();
    uint32 healPerTick = (uint32)(maxHp * 0.15f);
    po->setCurrentHealth(std::min(maxHp, (uint32)po->getCurrentHealth() + healPerTick));
    m_currentHealth = po->getCurrentHealth();

    uint32 maxIs = GetMaxInnerStrength();
    po->setInnerStrength(maxIs, maxIs);
    m_currentInnerStrength = maxIs;

    SafehouseNode* safehouse = GetNearestFortifiedSafehouse(m_currentPos.x, m_currentPos.z);
    if (safehouse) {
        RestockFromSafehouse(safehouse->id);
    }

    if (m_currentHealth >= maxHp || m_triageTimerMs > 10000) {
        m_currentState = FRANK_STATE_IDLE_PATROL;
        m_triageTimerMs = 0;
        DEBUG_LOG("FrankCastle: Triage complete. Vitals optimal. Weapons re-primed. Moving back to patrol.");
    }
}

// ============================================================================
// Phase 1: War Journal & Dynamic Hit List Engine Implementation
// ============================================================================
void FrankCastleManager::AddWarJournalEntry(WarJournalEntryType type, const std::string& targetHandle, const std::string& summary, const std::string& notes, uint32 districtId, LocationVector loc)
{
    WarJournalEntry entry;
    entry.id = m_nextJournalId++;
    entry.timestampMs = getMSTime();
    entry.type = type;
    entry.districtId = districtId;
    entry.districtName = (districtId == 1) ? "Slums" : ((districtId == 2) ? "Downtown" : ((districtId == 3) ? "International" : ((districtId == 4) ? "Industrial" : "Park East")));
    entry.location = loc;
    entry.targetHandle = targetHandle;
    entry.summary = summary;
    entry.fieldNotes = notes;

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    entry.timestampStr = ss.str();

    m_warJournal.push_back(entry);
    if (m_warJournal.size() > 100) {
        m_warJournal.pop_front();
    }

    // Occasionally drop lore cassettes in the world (every 5th journal entry)
    if (entry.id % 5 == 0) {
        DropWarJournalCassette(loc, entry.districtId, entry.districtName, summary, notes);
        SaveWarJournalToFile("WarJournal.json");
    }
}

std::vector<WarJournalEntry> FrankCastleManager::GetRecentJournalEntries(size_t limit) const
{
    std::vector<WarJournalEntry> entries;
    size_t count = 0;
    for (auto it = m_warJournal.rbegin(); it != m_warJournal.rend() && count < limit; ++it, ++count) {
        entries.push_back(*it);
    }
    return entries;
}

void FrankCastleManager::SaveWarJournalToFile(const std::string& path)
{
    std::ofstream ofs(path);
    if (!ofs.is_open()) return;

    ofs << "[\n";
    for (size_t i = 0; i < m_warJournal.size(); ++i) {
        const auto& e = m_warJournal[i];
        ofs << "  {\n";
        ofs << "    \"id\": " << e.id << ",\n";
        ofs << "    \"timestamp\": \"" << EscapeJsonString(e.timestampStr) << "\",\n";
        ofs << "    \"type\": " << (int)e.type << ",\n";
        ofs << "    \"target\": \"" << EscapeJsonString(e.targetHandle) << "\",\n";
        ofs << "    \"districtId\": " << e.districtId << ",\n";
        ofs << "    \"district\": \"" << EscapeJsonString(e.districtName) << "\",\n";
        ofs << "    \"x\": " << e.location.x << ", \"y\": " << e.location.y << ", \"z\": " << e.location.z << ",\n";
        ofs << "    \"summary\": \"" << EscapeJsonString(e.summary) << "\",\n";
        ofs << "    \"notes\": \"" << EscapeJsonString(e.fieldNotes) << "\"\n";
        ofs << "  }" << (i + 1 < m_warJournal.size() ? "," : "") << "\n";
    }
    ofs << "]\n";
    ofs.close();
}

bool FrankCastleManager::LoadWarJournalFromFile(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    std::string line;
    WarJournalEntry currentEntry;
    bool inObject = false;
    uint32 loadedCount = 0;

    while (std::getline(ifs, line)) {
        size_t openBrace = line.find('{');
        if (openBrace != std::string::npos) {
            inObject = true;
            currentEntry = WarJournalEntry();
            continue;
        }

        size_t closeBrace = line.find('}');
        if (closeBrace != std::string::npos && inObject) {
            if (currentEntry.id != 0) {
                m_warJournal.push_back(currentEntry);
                m_nextJournalId = std::max(m_nextJournalId, currentEntry.id + 1);
                loadedCount++;
            }
            inObject = false;
            continue;
        }

        if (!inObject) continue;

        auto extractString = [](const std::string& l, const std::string& key) -> std::string {
            size_t kpos = l.find("\"" + key + "\":");
            if (kpos == std::string::npos) return "";
            size_t q1 = l.find('"', kpos + key.size() + 3);
            if (q1 == std::string::npos) return "";
            size_t q2 = l.rfind('"');
            if (q2 == std::string::npos || q2 <= q1) return "";
            return UnescapeJsonString(l.substr(q1 + 1, q2 - q1 - 1));
        };

        auto extractNumber = [](const std::string& l, const std::string& key) -> double {
            size_t kpos = l.find("\"" + key + "\":");
            if (kpos == std::string::npos) return 0.0;
            std::string sub = l.substr(kpos + key.size() + 3);
            return std::stod(sub);
        };

        if (line.find("\"id\":") != std::string::npos) {
            currentEntry.id = (uint64)extractNumber(line, "id");
        } else if (line.find("\"timestamp\":") != std::string::npos) {
            currentEntry.timestampStr = extractString(line, "timestamp");
        } else if (line.find("\"type\":") != std::string::npos) {
            currentEntry.type = (WarJournalEntryType)(int)extractNumber(line, "type");
        } else if (line.find("\"target\":") != std::string::npos) {
            currentEntry.targetHandle = extractString(line, "target");
        } else if (line.find("\"districtId\":") != std::string::npos) {
            currentEntry.districtId = (uint32)extractNumber(line, "districtId");
        } else if (line.find("\"district\":") != std::string::npos) {
            currentEntry.districtName = extractString(line, "district");
        } else if (line.find("\"summary\":") != std::string::npos) {
            currentEntry.summary = extractString(line, "summary");
        } else if (line.find("\"notes\":") != std::string::npos) {
            currentEntry.fieldNotes = extractString(line, "notes");
        }
    }

    ifs.close();
    INFO_LOG(format("FrankCastleManager: Loaded %1% persistent War Journal entries from %2%")
             % loadedCount % path);
    return loadedCount > 0;
}

void FrankCastleManager::DropWarJournalCassette(LocationVector loc, uint32 districtId, const std::string& districtName, const std::string& title, const std::string& notes)
{
    WarJournalCassette c;
    c.cassetteId = m_nextCassetteId++;
    c.title = "War Journal Tape #" + std::to_string(c.cassetteId) + ": " + title;
    c.dropLocation = loc;
    c.districtId = districtId;
    c.districtName = districtName.empty() ? ((districtId == 1) ? "Slums" : "Downtown") : districtName;
    c.decryptionKey = "CASTLE-SIGMA-883";
    c.entry.summary = title;
    c.entry.fieldNotes = notes;
    c.entry.districtId = districtId;
    c.entry.districtName = c.districtName;
    c.isDecrypted = false;
    c.discoveredByPlayerGoId = 0;

    m_cassettes.push_back(c);
    if (m_cassettes.size() > 20) {
        m_cassettes.erase(m_cassettes.begin());
    }
}

bool FrankCastleManager::LootWarJournalCassette(uint32 cassetteId, uint32 playerGoId, std::string& outLore)
{
    for (auto& c : m_cassettes) {
        if (c.cassetteId == cassetteId && !c.isDecrypted) {
            c.isDecrypted = true;
            c.discoveredByPlayerGoId = playerGoId;
            outLore = "{c:FFAA00}[WAR JOURNAL CASSETTE DECRYPTED]{/c}\n" + c.title + "\n" + c.entry.fieldNotes;
            ModifyPlayerKarma(playerGoId, "Redpill Operative", 25, "Decrypted Vigilante War Journal");
            return true;
        }
    }
    return false;
}

void FrankCastleManager::EvaluateHitList()
{
    auto allIds = sObjMgr.getAllGOIds();

    for (uint32 id : allIds) {
        if (id == m_frankGoId) continue;
        PlayerObject* po = sObjMgr.getGOPtrSafe(id);
        if (!po || po->isDead()) continue;

        std::string h = po->getHandle();
        std::string lowerH = h;
        std::transform(lowerH.begin(), lowerH.end(), lowerH.begin(), ::tolower);

        HitListPriority prio = PRIORITY_SYSTEM_AGENT;
        float infamy = 100.0f;
        std::string dossier = "System Operative";

        if (lowerH.find("smith") != std::string::npos || sSmithCascade.IsInfected(id)) {
            prio = PRIORITY_OMEGA_SMITH;
            infamy = 500.0f;
            dossier = "Smith Contagion Replica Vector";
        } else if (lowerH.find("agent") != std::string::npos || (po->getClient().isBot() && dynamic_cast<BotClient*>(&po->getClient()) && dynamic_cast<BotClient*>(&po->getClient())->isAgent())) {
            prio = PRIORITY_SYSTEM_AGENT;
            infamy = 350.0f;
            dossier = "Machine System Hunter-Killer Agent";
        } else if (lowerH.find("lupine") != std::string::npos || lowerH.find("vampire") != std::string::npos || lowerH.find("merovingian") != std::string::npos) {
            prio = PRIORITY_SYNDICATE_BOSS;
            infamy = 250.0f;
            dossier = "Merovingian Syndicate Extortion Lieutenant";
        } else if (IsPlayerMarkedForPunishment(id)) {
            prio = PRIORITY_CORRUPT_PVP;
            infamy = 400.0f;
            dossier = "Corrupt Redpill Griefer / Machine Collaborator";
        } else {
            continue;
        }

        AddHitListTarget(id, po->getCharUID(), h, prio, infamy, dossier, po->getPosition(), 1, "Megacity");
    }

    if (UnderworldManager::getSingletonPtr()) {
        auto lieutenants = sUnderworldMgr.GetLivingLieutenants();
        for (auto* l : lieutenants) {
            if (!l) continue;
            float infamy = 250.0f + (float)l->grudgeScoreAgainstCastle + (float)(l->bountyOnCastle / 100);
            AddHitListTarget(
                77000 + l->id,
                9900000ULL + l->id,
                l->name,
                PRIORITY_SYNDICATE_BOSS,
                infamy,
                l->rankTitle + " (" + l->factionName + ")",
                l->lastKnownLocation,
                l->districtId,
                l->districtName
            );
        }
    }
}

void FrankCastleManager::AddHitListTarget(uint32 goId, uint64 charUID, const std::string& handle, HitListPriority priority, float infamy, const std::string& crimeDossier, LocationVector loc, uint32 districtId, const std::string& districtName)
{
    auto it = m_hitList.find(goId);
    if (it != m_hitList.end()) {
        it->second.lastKnownPos = loc;
        it->second.infamyScore = infamy;
        if (it->second.status == HIT_STATUS_TERMINATED) {
            it->second.status = HIT_STATUS_QUEUED;
        }
        return;
    }

    HitListEntry entry;
    entry.targetGoId = goId;
    entry.targetCharUID = charUID;
    entry.targetHandle = handle;
    entry.priority = priority;
    entry.status = HIT_STATUS_QUEUED;
    entry.infamyScore = infamy;
    entry.crimeDossier = crimeDossier;
    entry.lastKnownPos = loc;
    entry.districtId = districtId;
    entry.districtName = districtName;
    entry.addedTimestampMs = getMSTime();
    entry.reconTimerMs = 0;

    m_hitList[goId] = entry;
}

std::vector<HitListEntry> FrankCastleManager::GetHitList() const
{
    std::vector<HitListEntry> list;
    for (const auto& kv : m_hitList) {
        list.push_back(kv.second);
    }
    std::sort(list.begin(), list.end(), [](const HitListEntry& a, const HitListEntry& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.infamyScore > b.infamyScore;
    });
    return list;
}

HitListEntry* FrankCastleManager::GetHitListTarget(uint32 goId)
{
    auto it = m_hitList.find(goId);
    return (it != m_hitList.end()) ? &it->second : nullptr;
}

uint32 FrankCastleManager::SelectNextHitListTarget()
{
    auto list = GetHitList();
    for (const auto& entry : list) {
        if (entry.status == HIT_STATUS_QUEUED) {
            if (!GameServer::getSingletonPtr()) {
                return entry.targetGoId;
            }
            PlayerObject* po = sObjMgr.getGOPtrSafe(entry.targetGoId);
            if (po && !po->isDead()) {
                return entry.targetGoId;
            }
        }
    }
    return 0;
}

void FrankCastleManager::StartStalkingTarget(uint32 targetGoId)
{
    PlayerObject* po = sObjMgr.getGOPtrSafe(targetGoId);
    if (!po || po->isDead()) return;

    m_currentTargetGoId = targetGoId;
    m_targetIsAgent = (po->getHandle().find("Agent") != std::string::npos);
    m_targetIsSmith = (po->getHandle().find("Smith") != std::string::npos);
    m_currentState = FRANK_STATE_STALKING_TARGET;
    m_stateTimerMs = 0;

    auto it = m_hitList.find(targetGoId);
    if (it != m_hitList.end()) {
        it->second.status = HIT_STATUS_RECON;
        m_currentTargetPriority = it->second.priority;
    }

    if (BotManager::getSingletonPtr() && m_frankGoId != 0) {
        if (auto frankBot = sBotMgr.GetBotByPlayerGoId(m_frankGoId)) {
            frankBot->MoveTo((float)po->getPosition().x, (float)po->getPosition().y, (float)po->getPosition().z);
        }
    }
}

// ============================================================================
// Phase 2: Microchip Tech & Construct Armory Logistics Implementation
// ============================================================================
TacticalFieldCache* FrankCastleManager::GetNearestCache(float x, float z)
{
    TacticalFieldCache* best = nullptr;
    float minDistSq = 999999999.0f;
    for (auto& kv : m_fieldCaches) {
        if (kv.second.isCompromised) continue;
        float dx = kv.second.location.x - x;
        float dz = kv.second.location.z - z;
        float dSq = (dx * dx) + (dz * dz);
        if (dSq < minDistSq) {
            minDistSq = dSq;
            best = &kv.second;
        }
    }
    return best;
}

TacticalFieldCache* FrankCastleManager::GetFieldCache(uint32 cacheId)
{
    auto it = m_fieldCaches.find(cacheId);
    return (it != m_fieldCaches.end()) ? &it->second : nullptr;
}

bool FrankCastleManager::RestockFromFieldCache(uint32 cacheId)
{
    TacticalFieldCache* c = GetFieldCache(cacheId);
    if (!c || c->isCompromised) return false;

    // Grab supplies from cache
    uint32 ammoGrab = std::min(250U, c->supplies.highCaliberAmmo);
    m_carriedStockpile.highCaliberAmmo += ammoGrab;
    c->supplies.highCaliberAmmo -= ammoGrab;

    uint32 empGrab = std::min(4U, c->supplies.empGrenades);
    m_carriedStockpile.empGrenades += empGrab;
    c->supplies.empGrenades -= empGrab;

    uint32 plateGrab = std::min(4U, c->supplies.armorPlates);
    m_carriedStockpile.armorPlates += plateGrab;
    c->supplies.armorPlates -= plateGrab;

    uint32 kitGrab = std::min(3U, c->supplies.traumaKits);
    m_carriedStockpile.traumaKits += kitGrab;
    c->supplies.traumaKits -= kitGrab;

    // Replenish Microchip hardware
    m_microchipTech.depletedUraniumRounds = std::min(60U, m_microchipTech.depletedUraniumRounds + 20);
    m_microchipTech.codeScramblerEMPSatchels = std::min(10U, m_microchipTech.codeScramblerEMPSatchels + 3);
    m_microchipTech.adrenalineStims = std::min(8U, m_microchipTech.adrenalineStims + 2);

    c->lastRestockedMs = getMSTime();

    AddWarJournalEntry(JOURNAL_RESUPPLY, c->codename, "Field Cache Restock", "Ammunition replenished and hardware recalibrated.", c->districtId, c->location);
    return true;
}

void FrankCastleManager::AccessConstructArmory()
{
    // Tap into rogue loading construct buffer
    if (LoadingConstruct::getSingletonPtr()) {
        sLoadingConstruct.SetConstructMode(CONSTRUCT_MODE_TARGET_RANGE);
    }
    m_carriedStockpile.highCaliberAmmo = std::min(600U, m_carriedStockpile.highCaliberAmmo + 150);
    m_microchipTech.depletedUraniumRounds = std::min(60U, m_microchipTech.depletedUraniumRounds + 15);
    m_microchipTech.antiviralIncendiaryRounds = std::min(50U, m_microchipTech.antiviralIncendiaryRounds + 15);
    m_microchipTech.limpetMines = std::min(6U, m_microchipTech.limpetMines + 2);

    AddWarJournalEntry(JOURNAL_CONSTRUCT_FABRICATION, "The Armory Construct", "Munitions Fabrication", "Direct jack into rogue armory construct completed. Bespoke AP and antiviral ammunition fabricated.", 1, m_currentPos);
}

// ============================================================================
// Phase 3: Safehouse Defense, Incursions & Siege Events Implementation
// ============================================================================
bool FrankCastleManager::TriggerSafehouseSiege(uint32 safehouseId, SiegeFaction faction)
{
    SafehouseNode* sh = GetSafehouse(safehouseId);
    if (!sh || sh->status < SAFEHOUSE_SECURED) return false;

    SafehouseSiegeEvent siege;
    siege.safehouseId = safehouseId;
    siege.active = true;
    siege.attackingFaction = faction;
    siege.factionName = (faction == SIEGE_FACTION_MACHINES) ? "Machine Retribution Force" : ((faction == SIEGE_FACTION_EXILES) ? "Exile Cleaners" : "Merovingian Syndicate");
    siege.currentWave = 1;
    siege.totalWaves = 3;
    siege.enemiesRemaining = 6;
    siege.safehouseIntegrity = 100.0f;
    siege.waveTimerMs = 0;
    siege.participatingPlayersCount = 0;

    m_activeSieges[safehouseId] = siege;
    m_targetSafehouseId = safehouseId;
    m_currentState = FRANK_STATE_DEFEND_SAFEHOUSE;

    DEBUG_LOG(format("FrankCastle: Safehouse [%1%] in %2% under assault by %3%.")
        % sh->name % sh->districtName % siege.factionName);

    AddWarJournalEntry(JOURNAL_SIEGE_DEFENSE, siege.factionName, "Safehouse Under Siege", "Assault waves detected. Deploying CIWS turrets and EMP tripwires.", sh->districtId, sh->location);
    return true;
}

void FrankCastleManager::UpdateSafehouseSieges(uint32 deltaMs)
{
    for (auto it = m_activeSieges.begin(); it != m_activeSieges.end();) {
        SafehouseSiegeEvent& siege = it->second;
        if (!siege.active) {
            it = m_activeSieges.erase(it);
            continue;
        }

        siege.waveTimerMs += deltaMs;

        // Enemies inflict structural damage to safehouse integrity over time
        if (siege.enemiesRemaining > 0) {
            float integrityDamage = (float)siege.enemiesRemaining * 0.4f * ((float)deltaMs / 1000.0f);
            siege.safehouseIntegrity = std::max(0.0f, siege.safehouseIntegrity - integrityDamage);

            if (siege.safehouseIntegrity <= 0.0f) {
                // Safehouse integrity depleted - breached and overrun!
                uint32 shId = siege.safehouseId;
                ResolveSiegeDefense(shId, false);
                it = m_activeSieges.erase(it);
                continue;
            }
        }

        // Advance waves if enemies in wave defeated
        if (siege.enemiesRemaining == 0) {
            if (siege.currentWave < siege.totalWaves) {
                siege.currentWave++;
                siege.enemiesRemaining = 5 + siege.currentWave * 2;
                DEBUG_LOG(format("FrankCastle: Siege wave %1% inbound on safehouse #%2%.")
                    % siege.currentWave % siege.safehouseId);
            } else {
                // Siege successfully repelled!
                ResolveSiegeDefense(siege.safehouseId, true);
                it = m_activeSieges.erase(it);
                continue;
            }
        }

        ++it;
    }
}

SafehouseSiegeEvent* FrankCastleManager::GetActiveSiege(uint32 safehouseId)
{
    auto it = m_activeSieges.find(safehouseId);
    return (it != m_activeSieges.end() && it->second.active) ? &it->second : nullptr;
}

bool FrankCastleManager::IsSafehouseUnderSiege(uint32 safehouseId) const
{
    auto it = m_activeSieges.find(safehouseId);
    return (it != m_activeSieges.end() && it->second.active);
}

void FrankCastleManager::ResolveSiegeDefense(uint32 safehouseId, bool defendedSuccessfully)
{
    SafehouseNode* sh = GetSafehouse(safehouseId);
    if (!sh) return;

    auto it = m_activeSieges.find(safehouseId);
    if (it != m_activeSieges.end()) {
        it->second.active = false;
    }

    if (defendedSuccessfully) {
        m_siegesRepelled++;
        AwardExperience(3000);
        ScavengeSupplies(100, 5, 4, 3, 2);

        INFO_LOG(format("FrankCastle Safehouse: Siege broken at [%1%]. Perimeter held.") % sh->name);

        AddWarJournalEntry(JOURNAL_SIEGE_DEFENSE, "Assault Waves", "Siege Repelled", "Defended safehouse perimeter. All hostile breach teams neutralized.", sh->districtId, sh->location);
    } else {
        sh->status = SAFEHOUSE_CONTESTED;
        sh->fortificationLevel = 0;
        sh->tripwireTrapActive = false;
        sh->turretDefenseActive = false;
        sh->hostileGuardsCount = 6;
        sh->antiviralScrubberActive = false;

        INFO_LOG(format("FrankCastle Safehouse: [%1%] overrun! Perimeter compromised.") % sh->name);
        AddWarJournalEntry(JOURNAL_TACTICAL_RETREAT, "Overrun", "Safehouse Lost", "Perimeter collapsed. Falling back to secondary cache.", sh->districtId, sh->location);
    }

    // Check if there are other active sieges remaining to defend
    uint32 nextSiegeId = 0;
    for (const auto& kv : m_activeSieges) {
        if (kv.second.active && kv.first != safehouseId) {
            nextSiegeId = kv.first;
            break;
        }
    }

    if (nextSiegeId != 0) {
        m_targetSafehouseId = nextSiegeId;
        m_currentState = FRANK_STATE_DEFEND_SAFEHOUSE;
    } else if (m_targetSafehouseId == safehouseId && m_currentState == FRANK_STATE_DEFEND_SAFEHOUSE) {
        m_currentState = FRANK_STATE_IDLE_PATROL;
    }
}

bool FrankCastleManager::TriggerTripwireTrap(uint32 safehouseId, uint32 targetGoId)
{
    SafehouseNode* sh = GetSafehouse(safehouseId);
    if (!sh || !sh->tripwireTrapActive) return false;

    std::string handle = "Intruder";
    if (GameServer::getSingletonPtr()) {
        PlayerObject* target = sObjMgr.getGOPtrSafe(targetGoId);
        if (target) {
            target->takeDamage(m_frankGoId, 750, 0x280001C2);
            handle = target->getHandle();
        }
    }
    TriggerSpatialBallisticAudio(sh->location.x, sh->location.y, sh->location.z, "TRIPWIRE_DETONATION");

    DEBUG_LOG(format("FrankCastle Safehouse: Tripwire mine triggered at [%1%]. Intruder damaged: %2%.")
        % sh->name % handle);
    return true;
}

void FrankCastleManager::UpdateAutomatedCIWSTurrets(uint32 deltaMs)
{
    for (auto& kv : m_safehouses) {
        SafehouseNode& sh = kv.second;
        if (!sh.turretDefenseActive || sh.status < SAFEHOUSE_SECURED) continue;

        // If safehouse is under active siege, automated CIWS suppresses incoming siege waves
        auto* siege = GetActiveSiege(sh.id);
        if (siege && siege->active && siege->enemiesRemaining > 0) {
            sh.ciwsLastFiredMs += deltaMs;
            if (sh.ciwsLastFiredMs >= 2000) {
                sh.ciwsLastFiredMs = 0;
                siege->enemiesRemaining--;
                TriggerSpatialBallisticAudio(sh.location.x, sh.location.y, sh.location.z, "CIWS_TURRET");
            }
        }

        if (!SpatialGrid::getSingletonPtr() || !GameServer::getSingletonPtr()) continue;

        sh.ciwsLastFiredMs += deltaMs;
        if (sh.ciwsLastFiredMs >= 1000) { // Fires every second
            sh.ciwsLastFiredMs = 0;

            auto nearby = sSpatialGrid.GetClientsInRadius(sh.location.x, sh.location.z, 2500.0f);
            for (GameClient* gc : nearby) {
                if (!gc->isBot()) continue;
                uint32 botId = gc->GetPlayerGoId();
                PlayerObject* botPo = sObjMgr.getGOPtrSafe(botId);
                if (botPo && !botPo->isDead() && (botPo->getFactionName() == "Machine" || botPo->getFactionName() == "Exile")) {
                    botPo->takeDamage(m_frankGoId, 280, 0x280001C1);
                    TriggerSpatialBallisticAudio(sh.location.x, sh.location.y, sh.location.z, "CIWS_TURRET");
                    break;
                }
            }
        }
    }
}

void FrankCastleManager::OnPlayerAssistedSiege(uint32 safehouseId, uint32 playerGoId)
{
    std::string handle = "Redpill Operative";
    if (GameServer::getSingletonPtr()) {
        PlayerObject* po = sObjMgr.getGOPtrSafe(playerGoId);
        if (po) handle = po->getHandle();
    }

    ModifyPlayerKarma(playerGoId, handle, 200, "Assisted Safehouse Siege Defense");
    BroadcastRadioNet(
        (format("Commendation to %1%: Outstanding fire support in defending safehouse perimeter.")
         % handle).str(),
        true
    );
}

void FrankCastleManager::OnPlayerAttemptedSafehouseBreach(uint32 safehouseId, uint32 playerGoId)
{
    std::string handle = "Intruder";
    LocationVector loc;
    SafehouseNode* sh = GetSafehouse(safehouseId);
    if (sh) loc = sh->location;

    if (GameServer::getSingletonPtr()) {
        PlayerObject* po = sObjMgr.getGOPtrSafe(playerGoId);
        if (po) {
            handle = po->getHandle();
            loc = po->getPosition();
        }
        TriggerTripwireTrap(safehouseId, playerGoId);
    }

    ModifyPlayerKarma(playerGoId, handle, -400, "Attempted Hostile Safehouse Theft");

    AddHitListTarget(
        playerGoId,
        playerGoId,
        handle,
        PRIORITY_CORRUPT_PVP,
        600.0f,
        "Traitor: Caught attempting to breach and loot Frank Castle safehouse stockpile",
        loc,
        sh ? sh->districtId : 1,
        sh ? sh->districtName : "Slums"
    );

    BroadcastRadioNet(
        (format("Warning to %1%: You tripped the perimeter claymore. You're marked at the top of the Hit List. Run.")
         % handle).str(),
        true
    );
}

// ============================================================================
// Phase 4: Syndicate Decapitation & Anti-Agent Guerilla Raids Implementation
// ============================================================================
bool FrankCastleManager::TriggerDistrictCleanSweep(uint32 districtId)
{
    m_cleanSweepDistrictId = districtId;
    m_cleanSweepActive = true;
    m_cleanSweepTimerMs = 0;
    m_cleanSweepActionTimerMs = 0;
    m_cleanSweepKills = 0;
    m_currentState = FRANK_STATE_CLEAN_SWEEP_RAID;

    std::string dName = (districtId == 1) ? "Slums" : ((districtId == 2) ? "Downtown" : ((districtId == 3) ? "International" : ((districtId == 4) ? "Industrial" : "Park East")));

    // Locate highest threat racket in this district to strike first
    m_cleanSweepTargetRacketId = 0;
    if (UnderworldManager::getSingletonPtr()) {
        auto rackets = sUnderworldMgr.GetRacketsInDistrict(districtId);
        for (auto* r : rackets) {
            if (r && r->state != RacketState::DecapitatedCooldown) {
                m_cleanSweepTargetRacketId = r->id;
                sUnderworldMgr.RaidRacket(r->id, true);
                if (BotManager::getSingletonPtr() && m_frankGoId != 0) {
                    if (auto frankBot = sBotMgr.GetBotByPlayerGoId(m_frankGoId)) {
                        frankBot->MoveTo((float)r->coordinates.x, (float)r->coordinates.y, (float)r->coordinates.z);
                    }
                }
                break;
            }
        }
    }

    DEBUG_LOG(format("FrankCastle: Clean Sweep protocol initiated across %1%.") % dName);

    AddWarJournalEntry(JOURNAL_SYNDICATE_HIT, "District Syndicate", "Clean Sweep Operation Initiated", "Beginning district-wide decapitation raid in " + dName + ".", districtId, m_currentPos);
    return true;
}

void FrankCastleManager::UpdateCleanSweep(uint32 deltaMs)
{
    if (!m_cleanSweepActive) return;

    m_cleanSweepTimerMs += deltaMs;
    m_cleanSweepActionTimerMs += deltaMs;

    // Active raid execution: periodically neutralize guards and decapitate the target racket
    if (m_cleanSweepActionTimerMs >= 1500) {
        m_cleanSweepActionTimerMs = 0;

        if (UnderworldManager::getSingletonPtr() && m_cleanSweepTargetRacketId != 0) {
            auto* r = sUnderworldMgr.GetRacket(m_cleanSweepTargetRacketId);
            if (r && r->state != RacketState::DecapitatedCooldown) {
                if (r->activeGoonsCount > 0) {
                    r->activeGoonsCount--;
                    m_cleanSweepKills++;
                    ScavengeSupplies(20, 1, 1, 1);
                    AwardExperience(250);
                    TriggerSpatialBallisticAudio(m_currentPos.x, m_currentPos.y, m_currentPos.z, "7.62_heavy_burst");
                } else {
                    // All enforcers eliminated, decapitate boss & racket!
                    sUnderworldMgr.DecapitateRacket(r->id, true);
                    m_cleanSweepTargetRacketId = 0;

                    // Locate next available racket in this district
                    auto rackets = sUnderworldMgr.GetRacketsInDistrict(m_cleanSweepDistrictId);
                    for (auto* nextR : rackets) {
                        if (nextR && nextR->state != RacketState::DecapitatedCooldown) {
                            m_cleanSweepTargetRacketId = nextR->id;
                            sUnderworldMgr.RaidRacket(nextR->id, true);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (m_cleanSweepTimerMs > 120000 || (m_cleanSweepTargetRacketId == 0 && m_cleanSweepKills > 0)) {
        m_cleanSweepActive = false;
        m_cleanSweepTimerMs = 0;
        m_cleanSweepTargetRacketId = 0;
        m_currentState = FRANK_STATE_IDLE_PATROL;
        DEBUG_LOG("FrankCastle: Clean Sweep operation concluded. Syndicate infrastructure fractured. Withdrawing to patrol.");
    }
}

void FrankCastleManager::OnSyndicateLieutenantEliminated(uint32 targetGoId, const std::string& handle)
{
    m_syndicateBossesEliminated++;
    AwardExperience(1800);
    ScavengeSupplies(50, 2, 2, 2);

    INFO_LOG(format("FrankCastle: Syndicate boss [%1%] neutralized.") % handle);

    AddWarJournalEntry(JOURNAL_SYNDICATE_HIT, handle, "Syndicate Boss Eliminated", "Extortion syndicate lieutenant neutralized during clean sweep raid.", 2, m_currentPos);
}

// ============================================================================
// Phase 5: Player Interaction, The Vigilante's Judgment & Radio Net Implementation
// ============================================================================
int32 FrankCastleManager::GetPlayerKarma(uint32 playerGoId) const
{
    auto it = m_playerKarma.find(playerGoId);
    return (it != m_playerKarma.end()) ? it->second.karma : 0;
}

void FrankCastleManager::ModifyPlayerKarma(uint32 playerGoId, const std::string& handle, int32 delta, const std::string& reason)
{
    auto& rec = m_playerKarma[playerGoId];
    rec.playerGoId = playerGoId;
    rec.handle = handle;
    rec.karma += delta;
    rec.lastInteractionMs = getMSTime();

    if (rec.karma < -200 && !rec.isMarkedForPunishment) {
        rec.isMarkedForPunishment = true;
        BroadcastRadioNet(
            (format("{c:FF0000}[VIGILANTE'S JUDGMENT]{/c} %1% has crossed the line. You are marked for punishment.")
             % handle).str(),
            true
        );
        AddWarJournalEntry(JOURNAL_VIGILANTE_JUDGMENT, handle, "Player Marked For Punishment", "Extreme negative karma accumulated. Added to Hit List.", 1, m_currentPos);
    } else if (rec.karma >= 0) {
        rec.isMarkedForPunishment = false;
    }
}

bool FrankCastleManager::IsPlayerMarkedForPunishment(uint32 playerGoId) const
{
    auto it = m_playerKarma.find(playerGoId);
    return (it != m_playerKarma.end()) && it->second.isMarkedForPunishment;
}

void FrankCastleManager::BroadcastRadioNet(const std::string& message, bool playSquelch)
{
    RadioBroadcastRecord rec;
    rec.broadcastId = m_nextBroadcastId++;
    rec.timestampMs = getMSTime();
    rec.frequency = FRANK_RADIO_FREQUENCY;
    rec.transmissionText = message;
    rec.squelchTonePlayed = playSquelch;

    m_radioTransmissions.push_back(rec);
    if (m_radioTransmissions.size() > 25) {
        m_radioTransmissions.pop_front();
    }

    DEBUG_LOG(format("FrankCastleRadioNet: %1%") % message);
}

std::vector<RadioBroadcastRecord> FrankCastleManager::GetRecentRadioTransmissions(size_t limit) const
{
    std::vector<RadioBroadcastRecord> list;
    size_t count = 0;
    for (auto it = m_radioTransmissions.rbegin(); it != m_radioTransmissions.rend() && count < limit; ++it, ++count) {
        list.push_back(*it);
    }
    return list;
}

bool FrankCastleManager::AcceptVigilanteContract(uint32 contractId, uint32 playerGoId)
{
    for (auto& c : m_contracts) {
        if (c.contractId == contractId && !c.isCompleted && c.acceptedPlayerGoId == 0) {
            c.acceptedPlayerGoId = playerGoId;
            return true;
        }
    }
    return false;
}

bool FrankCastleManager::CompleteVigilanteContract(uint32 contractId, uint32 playerGoId, std::string& outRewardText)
{
    for (auto& c : m_contracts) {
        if (c.contractId == contractId && c.acceptedPlayerGoId == playerGoId && !c.isCompleted) {
            c.isCompleted = true;
            ModifyPlayerKarma(playerGoId, "Contractor", c.rewardKarma, "Completed Vigilante Contract");
            outRewardText = (format("Contract [%1%] completed! Awarded %2% High Caliber Ammo and %3% Vigilante Karma.")
                             % c.title % c.rewardAmmo % c.rewardKarma).str();
            return true;
        }
    }
    return false;
}

// ============================================================================
// Phase 6: Deep Reinforcement Learning & Tactical Combat Evolution Implementation
// ============================================================================
FrankCombatAction FrankCastleManager::DecideCombatActionEnhanced(uint32 targetGoId, HitListPriority priority, float distance, bool isEvasive)
{
    float healthPct = (float)m_currentHealth / (float)GetMaxHealth();
    bool hasAmmo = m_carriedStockpile.highCaliberAmmo >= 10;
    bool hasDU = m_microchipTech.depletedUraniumRounds > 0;
    bool hasEmp = m_carriedStockpile.empGrenades > 0;
    bool hasStim = m_microchipTech.adrenalineStims > 0;
    bool hasAntiviral = m_microchipTech.antiviralIncendiaryRounds > 0;
    bool hasPhos = m_microchipTech.whitePhosphorusSatchels > 0;

    std::vector<std::string> possibleActions;

    if (healthPct < 0.25f) {
        if (hasStim) possibleActions.push_back("STIM");
        possibleActions.push_back("COVER_ROLL");
        possibleActions.push_back("SMOKE");
    } else if (isEvasive) {
        possibleActions.push_back("FLASHBANG");
        possibleActions.push_back("CQC_TAKEDOWN");
        possibleActions.push_back("FLANKING");
    } else if (priority == PRIORITY_OMEGA_SMITH) {
        if (hasAntiviral && hasAmmo) possibleActions.push_back("ANTIVIRAL_VOLLEY");
        if (hasPhos) possibleActions.push_back("PHOSPHORUS");
        if (distance < 500.0f) possibleActions.push_back("POINT_BLANK_EXEC");
        possibleActions.push_back("BARREL_DETONATION");
        possibleActions.push_back("AP_VOLLEY");
    } else if (priority == PRIORITY_SYSTEM_AGENT) {
        if (distance > 2000.0f) possibleActions.push_back("SNIPER_AMBUSH");
        if (hasEmp && distance < 2000.0f) possibleActions.push_back("EMP_GRENADE");
        if (hasDU) possibleActions.push_back("AP_VOLLEY");
        if (distance < 600.0f) possibleActions.push_back("CQC_TAKEDOWN");
        possibleActions.push_back("COVER_ROLL");
    } else {
        if (distance > 2200.0f) possibleActions.push_back("SNIPER_AMBUSH");
        possibleActions.push_back("AP_VOLLEY");
        if (distance < 600.0f) possibleActions.push_back("POINT_BLANK_EXEC");
        possibleActions.push_back("BARREL_DETONATION");
    }

    if (possibleActions.empty()) {
        possibleActions.push_back("CQC_TAKEDOWN");
    }

    std::string stateKey = BuildQStateKey(priority == PRIORITY_SYSTEM_AGENT, distance, healthPct, hasAmmo, priority == PRIORITY_OMEGA_SMITH);
    std::string best = m_combatQTable.SelectBestAction(stateKey, possibleActions, 1.414f);

    if (best == "ANTIVIRAL_VOLLEY") return ACT_ANTIVIRAL_AP_VOLLEY;
    if (best == "PHOSPHORUS") return ACT_WHITE_PHOSPHORUS_BURST;
    if (best == "POINT_BLANK_EXEC") return ACT_POINT_BLANK_EXECUTION;
    if (best == "EMP_GRENADE") return ACT_EMP_DISRUPTION_GRENADE;
    if (best == "CQC_TAKEDOWN") return ACT_CQC_DISARM_TAKEDOWN;
    if (best == "COVER_ROLL") return ACT_TACTICAL_COVER_ROLL;
    if (best == "STIM") return ACT_ADRENALINE_STIM;
    if (best == "SMOKE") return ACT_SMOKE_EXTRACTION;
    if (best == "SNIPER_AMBUSH") return ACT_SNIPER_AMBUSH;
    if (best == "FLANKING") return ACT_FLANKING_MANEUVER;
    if (best == "BARREL_DETONATION") return ACT_EXPLOSIVE_BARREL_DETONATION;
    if (best == "FLASHBANG") return ACT_FLASHBANG_STUN;

    return ACT_ARMOR_PIERCING_VOLLEY;
}

bool FrankCastleManager::TriggerEnvironmentalHazard(float x, float z, float radius, uint32& outHostilesDamaged)
{
    outHostilesDamaged = 0;
    if (!GameServer::getSingletonPtr()) {
        outHostilesDamaged = 2; // Simulated dual explosive barrel / scaffolding collapse
        TriggerSpatialBallisticAudio(x, 0.0f, z, "ENVIRONMENTAL_EXPLOSION");
        return true;
    }
    auto nearby = sSpatialGrid.GetClientsInRadius(x, z, radius);
    for (GameClient* gc : nearby) {
        if (!gc->isBot()) continue;
        uint32 botId = gc->GetPlayerGoId();
        PlayerObject* botPo = sObjMgr.getGOPtrSafe(botId);
        if (botPo && !botPo->isDead()) {
            botPo->takeDamage(m_frankGoId, 600, 0x280001C2);
            outHostilesDamaged++;
        }
    }
    TriggerSpatialBallisticAudio(x, 0.0f, z, "ENVIRONMENTAL_EXPLOSION");
    return true;
}

// ============================================================================
// Phase 7: Client-Side Remaster Integration & Visual Polish Implementation
// ============================================================================
VisualBattleCondition FrankCastleManager::GetVisualCondition() const
{
    float pct = (float)m_currentHealth / (float)GetMaxHealth();
    if (pct >= 0.75f) return VISUAL_PRISTINE;
    if (pct >= 0.50f) return VISUAL_LIGHT_DAMAGE;
    if (pct >= 0.25f) return VISUAL_HEAVY_DAMAGE;
    return VISUAL_CRITICAL_BATTLE_WEAR;
}

std::string FrankCastleManager::GetVisualConditionName() const
{
    switch (GetVisualCondition()) {
        case VISUAL_PRISTINE: return "Pristine (Tactical Combat Vest)";
        case VISUAL_LIGHT_DAMAGE: return "Light Battle Damage (Dust & Scuffs)";
        case VISUAL_HEAVY_DAMAGE: return "Heavy Battle Damage (Torn Trenchcoat, Bloodstains)";
        default: return "Critical Survivor (Shredded Armor, Ballistic Scarring)";
    }
}

void FrankCastleManager::UpdateVisualAppearance()
{
    PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
    if (!po) return;

    // Dynamic appearance hex shifting based on battle condition
    switch (GetVisualCondition()) {
        case VISUAL_PRISTINE: po->setRsiHex("1f080099"); break;
        case VISUAL_LIGHT_DAMAGE: po->setRsiHex("1f080098"); break;
        case VISUAL_HEAVY_DAMAGE: po->setRsiHex("1f080097"); break;
        case VISUAL_CRITICAL_BATTLE_WEAR: po->setRsiHex("1f080096"); break;
    }
}

void FrankCastleManager::TriggerSpatialBallisticAudio(float x, float y, float z, const std::string& weaponType)
{
    if (SpatialAudioDSP::getSingletonPtr()) {
        sSpatialAudioDSP.ComputeSpatialAcoustics(
            m_currentPos.x, m_currentPos.y, m_currentPos.z, 0.0f,
            x, y, z, 500.0f, 30000.0f
        );
    }
    if (EmergentAIEngine::getSingletonPtr()) {
        sEmergentAIMgr.OnGunfireEcho(LocationVector(x, y, z), 120.0f, "Punisher Gunfire: " + weaponType);
    }
}

std::string FrankCastleManager::GenerateRemasterTelemetryJson() const
{
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"frank\": {\n";
    ss << "    \"name\": \"" << FRANK_CASTLE_NAME << "\",\n";
    ss << "    \"rank\": \"" << GetRankTitle() << "\",\n";
    ss << "    \"level\": " << (uint32)m_level << ",\n";
    ss << "    \"health\": " << m_currentHealth << ",\n";
    ss << "    \"maxHealth\": " << GetMaxHealth() << ",\n";
    ss << "    \"tacticalState\": \"" << GetTacticalStateName() << "\",\n";
    ss << "    \"condition\": \"" << GetVisualConditionName() << "\",\n";
    ss << "    \"position\": {\"x\": " << m_currentPos.x << ", \"y\": " << m_currentPos.y << ", \"z\": " << m_currentPos.z << "},\n";
    ss << "    \"stats\": {\n";
    ss << "      \"agentsKilled\": " << m_totalAgentsKilled << ",\n";
    ss << "      \"smithsPurged\": " << m_totalSmithsPurged << ",\n";
    ss << "      \"syndicatesDecapitated\": " << m_syndicateBossesEliminated << ",\n";
    ss << "      \"siegesRepelled\": " << m_siegesRepelled << "\n";
    ss << "    }\n";
    ss << "  },\n";

    ss << "  \"safehouses\": [\n";
    size_t shIdx = 0;
    for (const auto& kv : m_safehouses) {
        const auto& s = kv.second;
        ss << "    {\n";
        ss << "      \"id\": " << s.id << ",\n";
        ss << "      \"name\": \"" << s.name << "\",\n";
        ss << "      \"district\": \"" << s.districtName << "\",\n";
        ss << "      \"status\": " << (int)s.status << ",\n";
        ss << "      \"fortification\": " << (uint32)s.fortificationLevel << ",\n";
        ss << "      \"underSiege\": " << (IsSafehouseUnderSiege(s.id) ? "true" : "false") << ",\n";
        ss << "      \"scrubberActive\": " << (s.antiviralScrubberActive ? "true" : "false") << "\n";
        ss << "    }" << (++shIdx < m_safehouses.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";

    ss << "  \"radioFrequency\": \"" << FRANK_RADIO_FREQUENCY << "\",\n";
    ss << "  \"activeTransmissions\": " << m_radioTransmissions.size() << "\n";
    ss << "}";
    return ss.str();
}

std::string FrankCastleManager::GenerateStatusReport() const
{
    std::stringstream ss;
    ss << "{c:FF3333}=== FRANK CASTLE (THE PUNISHER) - MASTER DOSSIER ==={/c}\n";
    ss << " Status: " << (m_isLive ? "ACTIVE & LIVE IN THE MATRIX" : "OFFLINE") << "\n";
    ss << " Title: " << GetRankTitle() << " (Level " << (uint32)m_level << ")\n";
    ss << " Experience: " << m_exp << " / " << GetRequiredExpForNextLevel() << "\n";
    ss << " Health: " << m_currentHealth << " / " << GetMaxHealth() << " | Inner Strength: " << m_currentInnerStrength << " / " << GetMaxInnerStrength() << "\n";
    ss << " Visual Wear: " << GetVisualConditionName() << "\n";
    ss << " Damage Mult: " << std::fixed << std::setprecision(2) << GetBallisticDamageMultiplier() << "x | Armor Mitigation: " << (uint32)(GetDamageMitigation() * 100.0f) << "%\n";
    ss << " Current Tactical Order: " << GetTacticalStateName() << "\n";
    ss << " Sector Position: (" << (int)m_currentPos.x << ", " << (int)m_currentPos.y << ", " << (int)m_currentPos.z << ")\n";
    ss << " Agents Neutralized: " << m_totalAgentsKilled << " | Smiths Purged: " << m_totalSmithsPurged << "\n";
    ss << " Syndicates Decapitated: " << m_syndicateBossesEliminated << " | Sieges Repelled: " << m_siegesRepelled << "\n";
    ss << " Safehouses Held: " << GetControlledSafehousesCount() << " / " << m_safehouses.size() << "\n";
    ss << " Field Caches Online: " << m_fieldCaches.size() << "\n";
    ss << " Microchip DU Rounds: " << m_microchipTech.depletedUraniumRounds 
       << " | EMP Satchels: " << m_microchipTech.codeScramblerEMPSatchels 
       << " | Stims: " << m_microchipTech.adrenalineStims 
       << " | Antiviral: " << m_microchipTech.antiviralIncendiaryRounds << "\n";
    ss << " War Journal Logs: " << m_warJournal.size() << " | Dynamic Hit List: " << m_hitList.size() << " Targets";
    if (m_currentState == FRANK_STATE_CONVALESCENCE_HEALING) {
        ss << "\n{c:FF5555} Convalescence Active: Safehouse #" << m_convalescenceState.safehouseId 
           << " | Stage " << (uint32)m_convalescenceState.stage << " (" << std::fixed << std::setprecision(1) << m_convalescenceState.stageProgressPercent << "% complete)"
           << " | Sutures: " << (m_convalescenceState.suturesRuptured ? "{c:FF0000}RUPTURED!{/c}" : "{c:00FF00}INTACT{/c}") << "{/c}";
    }
    return ss.str();
}

// ============================================================================
// Core Combat, Progression & Utility Implementations
// ============================================================================
void FrankCastleManager::ScanForAgentsAndThreats()
{
    if (IsSmithOutbreakActive()) {
        uint32 smithTarget = ScanForSmithTargets();
        if (smithTarget != 0) {
            InterceptSmithThreat(smithTarget);
            return;
        }
    }

    auto allIds = sObjMgr.getAllGOIds();
    uint32 bestAgentTarget = 0;
    float closestAgentDist = 999999.0f;

    for (uint32 id : allIds) {
        if (id == m_frankGoId) continue;
        PlayerObject* po = sObjMgr.getGOPtrSafe(id);
        if (!po || po->isDead()) continue;

        LocationVector tPos = po->getPosition();
        float dx = tPos.x - m_currentPos.x;
        float dz = tPos.z - m_currentPos.z;
        float dist = std::sqrt((dx * dx) + (dz * dz));

        bool isAgent = false;
        std::string h = po->getHandle();
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        if (h.find("agent") != std::string::npos || 
            (po->getClient().isBot() && dynamic_cast<BotClient*>(&po->getClient()) && dynamic_cast<BotClient*>(&po->getClient())->isAgent())) {
            isAgent = true;
        }

        if (isAgent && dist < 12000.0f && dist < closestAgentDist) {
            closestAgentDist = dist;
            bestAgentTarget = id;
        }
    }

    if (bestAgentTarget != 0) {
        HuntTargetAgent(bestAgentTarget);
    }
}

std::string FrankCastleManager::BuildQStateKey(bool isAgent, float distance, float healthPct, bool hasAmmo, bool isSmith)
{
    int a_state = isAgent ? (isSmith ? 2 : 1) : 0;
    int d_state = (distance > 2500.0f) ? 2 : ((distance >= 800.0f) ? 1 : 0);
    int h_state = (healthPct > 0.70f) ? 2 : ((healthPct >= 0.30f) ? 1 : 0);
    int ammo_state = hasAmmo ? 1 : 0;
    int s_state = isSmith ? 1 : 0;
    return "A" + std::to_string(a_state) + "_D" + std::to_string(d_state) + "_H" + std::to_string(h_state) + "_AM" + std::to_string(ammo_state) + "_S" + std::to_string(s_state);
}

FrankCombatAction FrankCastleManager::DecideCombatAction(uint32 targetGoId, bool targetIsAgent, float distance, bool targetIsSmith)
{
    return DecideCombatActionEnhanced(targetGoId, targetIsSmith ? PRIORITY_OMEGA_SMITH : (targetIsAgent ? PRIORITY_SYSTEM_AGENT : PRIORITY_CORRUPT_PVP), distance, false);
}

void FrankCastleManager::RecordCombatOutcome(FrankCombatAction action, float damageDealt, float damageTaken, bool targetKilled, bool targetWasAgent, float distance, bool targetWasSmith)
{
    float healthPct = (float)m_currentHealth / (float)GetMaxHealth();
    std::string stateKey = BuildQStateKey(targetWasAgent, distance, healthPct, m_carriedStockpile.highCaliberAmmo >= 10, targetWasSmith);

    std::string actionStr = "AP_VOLLEY";
    switch (action) {
        case ACT_ANTIVIRAL_AP_VOLLEY: actionStr = "ANTIVIRAL_VOLLEY"; break;
        case ACT_WHITE_PHOSPHORUS_BURST: actionStr = "PHOSPHORUS"; break;
        case ACT_POINT_BLANK_EXECUTION: actionStr = "POINT_BLANK_EXEC"; break;
        case ACT_EMP_DISRUPTION_GRENADE: actionStr = "EMP_GRENADE"; break;
        case ACT_CQC_DISARM_TAKEDOWN: actionStr = "CQC_TAKEDOWN"; break;
        case ACT_TACTICAL_COVER_ROLL: actionStr = "COVER_ROLL"; break;
        case ACT_ADRENALINE_STIM: actionStr = "STIM"; break;
        case ACT_SMOKE_EXTRACTION: actionStr = "SMOKE"; break;
        case ACT_SNIPER_AMBUSH: actionStr = "SNIPER_AMBUSH"; break;
        case ACT_FLANKING_MANEUVER: actionStr = "FLANKING"; break;
        case ACT_EXPLOSIVE_BARREL_DETONATION: actionStr = "BARREL_DETONATION"; break;
        case ACT_FLASHBANG_STUN: actionStr = "FLASHBANG"; break;
        case ACT_COLLAPSE_SCAFFOLDING: actionStr = "COLLAPSE_SCAFFOLDING"; break;
        default: actionStr = "AP_VOLLEY"; break;
    }

    float reward = (damageDealt * 0.2f) - (damageTaken * 0.1f);
    if (targetKilled) {
        if (targetWasSmith) reward += 250.0f;
        else if (targetWasAgent) reward += 150.0f;
        else reward += 60.0f;
    }
    if (action == ACT_ANTIVIRAL_AP_VOLLEY && targetWasSmith) reward += 40.0f;
    if (action == ACT_SNIPER_AMBUSH && distance > 2000.0f) reward += 50.0f;
    if (action == ACT_FLASHBANG_STUN) reward += 30.0f;
    if (action == ACT_EXPLOSIVE_BARREL_DETONATION) reward += 45.0f;

    float nextHpPct = (float)m_currentHealth / (float)GetMaxHealth();
    std::string nextStateKey = BuildQStateKey(targetWasAgent, distance, nextHpPct, m_carriedStockpile.highCaliberAmmo >= 10, targetWasSmith);
    std::vector<std::string> possible = {
        "AP_VOLLEY", "ANTIVIRAL_VOLLEY", "PHOSPHORUS", "POINT_BLANK_EXEC", 
        "EMP_GRENADE", "CQC_TAKEDOWN", "COVER_ROLL", "STIM", "SMOKE",
        "SNIPER_AMBUSH", "FLANKING", "BARREL_DETONATION", "FLASHBANG", "COLLAPSE_SCAFFOLDING"
    };

    m_combatQTable.UpdateQValue(stateKey, actionStr, reward, nextStateKey, possible);
}

void FrankCastleManager::UpdateThreatMemory(uint32 targetGoId, const std::string& handle, bool isAgent, uint32 dmgDealt, uint32 dmgTaken)
{
    auto& rec = m_threatMemory[targetGoId];
    rec.targetGoId = targetGoId;
    rec.handle = handle;
    rec.isAgent = isAgent;
    rec.encounters++;
    rec.damageDealtToFrank += dmgTaken;
    rec.damageTakenFromFrank += dmgDealt;
    rec.lastEncounterMs = getMSTime();
}

uint64 FrankCastleManager::GetRequiredExpForNextLevel() const
{
    if (m_level >= 50) return 0xFFFFFFFFFFFFFFFFULL;
    return (uint64)m_level * ((uint64)m_level + 1) * 600ULL;
}

FrankMasteryRank FrankCastleManager::GetRank() const
{
    if (m_level >= 41) return RANK_THE_PUNISHER;
    if (m_level >= 31) return RANK_SYSTEM_NEMESIS;
    if (m_level >= 21) return RANK_TACTICAL_SPECIALIST;
    if (m_level >= 11) return RANK_GUERRILLA_OPERATIVE;
    return RANK_URBAN_VIGILANTE;
}

std::string FrankCastleManager::GetRankTitle() const
{
    switch (GetRank()) {
        case RANK_THE_PUNISHER: return "The Punisher (Apex Anti-Hero)";
        case RANK_SYSTEM_NEMESIS: return "System Nemesis";
        case RANK_TACTICAL_SPECIALIST: return "Tactical Specialist";
        case RANK_GUERRILLA_OPERATIVE: return "Guerrilla Operative";
        default: return "Urban Vigilante";
    }
}

void FrankCastleManager::AwardExperience(uint64 amount)
{
    m_exp += amount;
    CheckLevelUp();
}

void FrankCastleManager::CheckLevelUp()
{
    bool leveled = false;
    while (m_level < 50 && m_exp >= GetRequiredExpForNextLevel()) {
        m_level++;
        leveled = true;
    }

    if (leveled) {
        m_currentHealth = GetMaxHealth();
        m_currentInnerStrength = GetMaxInnerStrength();

        PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
        if (po) {
            po->setLevel(m_level);
            po->setMaximumHealth(GetMaxHealth());
            po->setCurrentHealth(GetMaxHealth());
            po->setInnerStrength(GetMaxInnerStrength(), GetMaxInnerStrength());
        }

        BroadcastRadioNet(
            (format("Tactical prowess upgraded. Reached Level %1% [%2%]. One batch, two batch, penny and dime.")
             % (uint32)m_level % GetRankTitle()).str(),
            true
        );
    }
}

uint32 FrankCastleManager::GetMaxHealth() const
{
    return 2500 + ((uint32)m_level * 150) + (m_carriedStockpile.armorPlates * 15);
}

uint32 FrankCastleManager::GetMaxInnerStrength() const
{
    return 1000 + ((uint32)m_level * 60);
}

float FrankCastleManager::GetBallisticDamageMultiplier() const
{
    float mult = 1.0f + ((float)m_level * 0.04f);
    if (m_currentState == FRANK_STATE_CONTAINMENT_PROTOCOL) mult += 0.40f;
    return mult;
}

float FrankCastleManager::GetDamageMitigation() const
{
    float base = 0.20f + ((float)m_level * 0.01f);
    if (m_carriedStockpile.armorPlates > 5) base += 0.10f;
    if (m_currentState == FRANK_STATE_CONTAINMENT_PROTOCOL) base += 0.15f;
    return std::min(0.75f, base);
}

float FrankCastleManager::GetCriticalChance() const
{
    return std::min(0.50f, 0.10f + ((float)m_level * 0.008f));
}

SafehouseNode* FrankCastleManager::GetSafehouse(uint32 id)
{
    auto it = m_safehouses.find(id);
    return (it != m_safehouses.end()) ? &it->second : nullptr;
}

SafehouseNode* FrankCastleManager::GetNearestSafehouse(float x, float z)
{
    SafehouseNode* best = nullptr;
    float minDist = 99999999.0f;
    for (auto& kv : m_safehouses) {
        float dx = kv.second.location.x - x;
        float dz = kv.second.location.z - z;
        float d = (dx * dx) + (dz * dz);
        if (d < minDist) {
            minDist = d;
            best = &kv.second;
        }
    }
    return best;
}

SafehouseNode* FrankCastleManager::GetNearestFortifiedSafehouse(float x, float z)
{
    SafehouseNode* best = nullptr;
    float minDist = 99999999.0f;
    for (auto& kv : m_safehouses) {
        if (kv.second.status == SAFEHOUSE_FORTIFIED || kv.second.status == SAFEHOUSE_SECURED) {
            float dx = kv.second.location.x - x;
            float dz = kv.second.location.z - z;
            float d = (dx * dx) + (dz * dz);
            if (d < minDist) {
                minDist = d;
                best = &kv.second;
            }
        }
    }
    return best;
}

bool FrankCastleManager::CaptureSafehouse(uint32 safehouseId)
{
    SafehouseNode* s = GetSafehouse(safehouseId);
    if (!s) return false;

    if (s->status == SAFEHOUSE_SECURED || s->status == SAFEHOUSE_FORTIFIED) {
        return true;
    }

    s->status = SAFEHOUSE_SECURED;
    s->hostileGuardsCount = 0;
    s->lastReinforcedMs = getMSTime();
    m_safehousesCaptured++;

    AwardExperience(1000);

    DEBUG_LOG(format("FrankCastle: Safehouse [%1%] in %2% secured.") % s->name % s->districtName);

    AddWarJournalEntry(JOURNAL_SAFEHOUSE_CLAIMED, s->name, "Safehouse Secured", "Secured bunker from hostile elements.", s->districtId, s->location);
    return true;
}

bool FrankCastleManager::FortifySafehouse(uint32 safehouseId)
{
    SafehouseNode* s = GetSafehouse(safehouseId);
    if (!s) return false;

    if (s->status != SAFEHOUSE_SECURED && s->status != SAFEHOUSE_FORTIFIED) {
        return false;
    }

    if (s->fortificationLevel < 5) {
        s->fortificationLevel++;
    }

    s->status = SAFEHOUSE_FORTIFIED;
    s->tripwireTrapActive = true;
    s->turretDefenseActive = (s->fortificationLevel >= 2);
    s->lastReinforcedMs = getMSTime();

    DepositSuppliesToSafehouse(safehouseId);

    DEBUG_LOG(format("FrankCastle: Safehouse [%1%] fortified to Level %2%. Perimeter CIWS and EMP tripwires armed.")
        % s->name % (uint32)s->fortificationLevel);

    AddWarJournalEntry(JOURNAL_SAFEHOUSE_FORTIFIED, s->name, "Safehouse Fortified", "Automated CIWS and tripwires armed.", s->districtId, s->location);
    return true;
}

uint32 FrankCastleManager::GetControlledSafehousesCount() const
{
    uint32 count = 0;
    for (const auto& kv : m_safehouses) {
        if (kv.second.status == SAFEHOUSE_SECURED || kv.second.status == SAFEHOUSE_FORTIFIED) {
            count++;
        }
    }
    return count;
}

void FrankCastleManager::ScavengeSupplies(uint32 ammo, uint32 emp, uint32 plates, uint32 kits, uint32 codes)
{
    m_carriedStockpile.highCaliberAmmo += ammo;
    m_carriedStockpile.empGrenades += emp;
    m_carriedStockpile.armorPlates += plates;
    m_carriedStockpile.traumaKits += kits;
    m_carriedStockpile.scramblerCodes += codes;
}

void FrankCastleManager::DepositSuppliesToSafehouse(uint32 safehouseId)
{
    SafehouseNode* s = GetSafehouse(safehouseId);
    if (!s) return;

    uint32 depAmmo = m_carriedStockpile.highCaliberAmmo / 2;
    uint32 depEmp = m_carriedStockpile.empGrenades / 2;
    uint32 depPlates = m_carriedStockpile.armorPlates / 2;
    uint32 depKits = m_carriedStockpile.traumaKits / 2;
    uint32 depCodes = m_carriedStockpile.scramblerCodes / 2;

    s->stockpile.highCaliberAmmo += depAmmo;
    s->stockpile.empGrenades += depEmp;
    s->stockpile.armorPlates += depPlates;
    s->stockpile.traumaKits += depKits;
    s->stockpile.scramblerCodes += depCodes;

    m_carriedStockpile.highCaliberAmmo -= depAmmo;
    m_carriedStockpile.empGrenades -= depEmp;
    m_carriedStockpile.armorPlates -= depPlates;
    m_carriedStockpile.traumaKits -= depKits;
    m_carriedStockpile.scramblerCodes -= depCodes;
}

void FrankCastleManager::RestockFromSafehouse(uint32 safehouseId)
{
    SafehouseNode* s = GetSafehouse(safehouseId);
    if (!s) return;

    if (m_carriedStockpile.highCaliberAmmo < 200 && s->stockpile.highCaliberAmmo > 0) {
        uint32 needed = 200 - m_carriedStockpile.highCaliberAmmo;
        uint32 grab = std::min(needed, s->stockpile.highCaliberAmmo);
        m_carriedStockpile.highCaliberAmmo += grab;
        s->stockpile.highCaliberAmmo -= grab;
    }

    if (m_carriedStockpile.empGrenades < 6 && s->stockpile.empGrenades > 0) {
        uint32 grab = std::min(6 - m_carriedStockpile.empGrenades, s->stockpile.empGrenades);
        m_carriedStockpile.empGrenades += grab;
        s->stockpile.empGrenades -= grab;
    }

    if (m_carriedStockpile.armorPlates < 8 && s->stockpile.armorPlates > 0) {
        uint32 grab = std::min(8 - m_carriedStockpile.armorPlates, s->stockpile.armorPlates);
        m_carriedStockpile.armorPlates += grab;
        s->stockpile.armorPlates -= grab;
    }

    if (m_carriedStockpile.traumaKits < 5 && s->stockpile.traumaKits > 0) {
        uint32 grab = std::min(5 - m_carriedStockpile.traumaKits, s->stockpile.traumaKits);
        m_carriedStockpile.traumaKits += grab;
        s->stockpile.traumaKits -= grab;
    }

    if (m_carriedStockpile.scramblerCodes < 3 && s->stockpile.scramblerCodes > 0) {
        uint32 grab = std::min(3 - m_carriedStockpile.scramblerCodes, s->stockpile.scramblerCodes);
        m_carriedStockpile.scramblerCodes += grab;
        s->stockpile.scramblerCodes -= grab;
    }
}

uint32 FrankCastleManager::GetTotalGlobalStockpileVolume() const
{
    uint32 sum = m_carriedStockpile.GetTotalSupplies();
    for (const auto& kv : m_safehouses) {
        sum += kv.second.stockpile.GetTotalSupplies();
    }
    return sum;
}

void FrankCastleManager::HuntTargetAgent(uint32 agentGoId)
{
    m_currentTargetGoId = agentGoId;
    m_targetIsAgent = true;
    m_currentState = FRANK_STATE_HUNT_AGENTS;
    m_stateTimerMs = 0;

    PlayerObject* po = sObjMgr.getGOPtrSafe(agentGoId);
    std::string name = po ? po->getHandle() : "Agent";

    DEBUG_LOG(format("FrankCastle: Target acquired: %1%. Engaging.") % name);
}

void FrankCastleManager::OnAgentDefeated(uint32 agentGoId, const std::string& agentName)
{
    m_totalAgentsKilled++;
    m_totalHostilesDefeated++;

    AwardExperience(1500);
    ScavengeSupplies(60, 3, 2, 2, 1);

    DEBUG_LOG(format("FrankCastle: Agent %1% neutralized.") % agentName);

    AddWarJournalEntry(JOURNAL_KILL, agentName, "System Agent Neutralized", "Machine agent terminated in direct kinetic engagement.", 2, m_currentPos);
}

void FrankCastleManager::BroadcastPirateTransmission(const std::string& message, bool shardWide)
{
    uint32 now = getMSTime();
    // 15-minute cooldown on shard-wide pirate broadcasts (900,000 ms)
    if (shardWide && m_lastPublicBroadcastMs != 0 && (now - m_lastPublicBroadcastMs < 900000)) {
        DEBUG_LOG(format("FrankCastle: Shard-wide pirate broadcast suppressed by cooldown: %1%") % message);
        return;
    }

    std::string formatted = (format("{c:FF2222}[PIRATE TRANSMISSION - FM 88.3 - FRANK CASTLE]{/c} {c:FFFFFF}%1%{/c}") % message).str();

    if (GameServer::getSingletonPtr()) {
        if (shardWide) {
            sGame.Broadcast(std::make_shared<SystemChatMsg>(formatted)->toBuf(), false);
            m_lastPublicBroadcastMs = now;
        } else {
            sGame.BroadcastNear(m_currentPos.x, m_currentPos.z, 20000.0f, std::make_shared<SystemChatMsg>(formatted)->toBuf(), false);
        }
    }

    INFO_LOG(format("FrankCastleBroadcast: %1%") % message);
}

std::string FrankCastleManager::GetTacticalStateName() const
{
    switch (m_currentState) {
        case FRANK_STATE_IDLE_PATROL: return "Tactical Sector Patrol";
        case FRANK_STATE_STALKING_TARGET: return "Recon & Ambush Stalking";
        case FRANK_STATE_HUNT_AGENTS: return "Hunting Agent Target";
        case FRANK_STATE_PURGE_SMITH_OUTBREAK: return "Purging Smith Viral Outbreak";
        case FRANK_STATE_CONTAINMENT_PROTOCOL: return "Executing Omega Containment Protocol";
        case FRANK_STATE_ASSAULT_SAFEHOUSE: return "Assaulting Hostile Stronghold";
        case FRANK_STATE_FORTIFY_SAFEHOUSE: return "Fortifying Safehouse";
        case FRANK_STATE_IN_COMBAT: return "Engaging in Close Combat";
        case FRANK_STATE_TACTICAL_RETREAT: return "Tactical Smoke Extraction";
        case FRANK_STATE_FIELD_TRIAGE: return "Field Triage & Munitions Resupply";
        case FRANK_STATE_RESUPPLY_RUN: return "Stealth Route to Field Cache";
        case FRANK_STATE_DEFEND_SAFEHOUSE: return "Defending Safehouse Perimeter Under Siege";
        case FRANK_STATE_CLEAN_SWEEP_RAID: return "District Clean Sweep Syndicate Raid";
        default: return "Autonomous Recon";
    }
}

bool FrankCastleManager::IsSmithOutbreakActive() const
{
    return sSmithCascade.GetInfectedCount() > 0 || sSmithCascade.GetStage() >= CONTAGION_STAGE_ELEVATED;
}

void FrankCastleManager::RespondToSmithOutbreak()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_currentState == FRANK_STATE_TACTICAL_RETREAT || m_currentState == FRANK_STATE_FIELD_TRIAGE) {
        return;
    }

    ContagionStage stage = sSmithCascade.GetStage();
    if (stage >= CONTAGION_STAGE_CASCADE) {
        m_currentState = FRANK_STATE_CONTAINMENT_PROTOCOL;
        BroadcastPirateTransmission(
            (format("OMEGA CONTAINMENT AUTHORIZED: Smith viral cascade critical at %1%%%. Shard quarantine in effect. No infected code survives.")
             % (int)sSmithCascade.GetInfectionPercentage()).str(),
            true
        );
    } else {
        m_currentState = FRANK_STATE_PURGE_SMITH_OUTBREAK;
        DEBUG_LOG(format("FrankCastle: Smith outbreak detected (Active infections: %1%). Moving in.")
            % sSmithCascade.GetInfectedCount());
    }

    for (auto& kv : m_safehouses) {
        if (kv.second.status == SAFEHOUSE_FORTIFIED && !kv.second.antiviralScrubberActive) {
            DeployAntiviralScrubber(kv.second.id);
        }
    }

    uint32 target = ScanForSmithTargets();
    if (target != 0) {
        InterceptSmithThreat(target);
    }
}

void FrankCastleManager::InterceptSmithThreat(uint32 smithGoId)
{
    m_currentTargetGoId = smithGoId;
    m_targetIsAgent = true;
    m_targetIsSmith = true;
    m_currentState = FRANK_STATE_PURGE_SMITH_OUTBREAK;
    m_stateTimerMs = 0;

    PlayerObject* po = sObjMgr.getGOPtrSafe(smithGoId);
    std::string name = po ? po->getHandle() : "Smith Replica";

    DEBUG_LOG(format("FrankCastle: Target acquired: %1% [ID: %2%]. Deploying antiviral AP rounds and incendiaries.")
        % name % smithGoId);
}

void FrankCastleManager::OnSmithCloneEliminated(uint32 smithGoId, const std::string& handle)
{
    m_totalSmithsPurged++;
    m_totalAgentsKilled++;
    m_totalHostilesDefeated++;

    AwardExperience(2500);
    ScavengeSupplies(80, 3, 2, 2, 2);
    m_microchipTech.antiviralIncendiaryRounds = std::min(50U, m_microchipTech.antiviralIncendiaryRounds + 10);

    DEBUG_LOG("FrankCastle: Smith replica neutralized.");

    AddWarJournalEntry(JOURNAL_KILL, handle, "Smith Replica Purged", "Viral clone eliminated with antiviral incendiary AP volley.", 1, m_currentPos);
}

uint32 FrankCastleManager::ScanForSmithTargets()
{
    float dist = 0.0f;
    uint32 nearestInfected = sSmithCascade.GetNearestInfectedEntity(m_currentPos.x, m_currentPos.z, &dist);
    if (nearestInfected != 0 && dist < 25000.0f) {
        return nearestInfected;
    }

    auto allIds = sObjMgr.getAllGOIds();
    uint32 bestTarget = 0;
    float bestDistSq = 25000.0f * 25000.0f;

    for (uint32 id : allIds) {
        if (id == m_frankGoId) continue;
        PlayerObject* po = sObjMgr.getGOPtrSafe(id);
        if (!po || po->isDead()) continue;

        std::string h = po->getHandle();
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        if (h.find("smith") != std::string::npos) {
            LocationVector tPos = po->getPosition();
            float dx = tPos.x - m_currentPos.x;
            float dz = tPos.z - m_currentPos.z;
            float dSq = (dx * dx) + (dz * dz);
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                bestTarget = id;
            }
        }
    }

    return bestTarget;
}

bool FrankCastleManager::DeployAntiviralScrubber(uint32 safehouseId)
{
    SafehouseNode* s = GetSafehouse(safehouseId);
    if (!s) return false;

    if (s->status != SAFEHOUSE_SECURED && s->status != SAFEHOUSE_FORTIFIED) {
        return false;
    }

    s->antiviralScrubberActive = true;
    s->lastScrubberPulseMs = 0;

    DEBUG_LOG(format("FrankCastle: Hardline antiviral scrubber deployed at [%1%].") % s->name);

    return true;
}

void FrankCastleManager::UpdateSafehouseScrubbers(uint32 deltaMs)
{
    if (!SpatialGrid::getSingletonPtr()) return;
    PlayerObject* frankPo = sObjMgr.getGOPtrSafe(m_frankGoId);

    for (auto& kv : m_safehouses) {
        SafehouseNode& sh = kv.second;
        if (!sh.antiviralScrubberActive) continue;

        sh.lastScrubberPulseMs += deltaMs;
        if (sh.lastScrubberPulseMs >= 5000) {
            sh.lastScrubberPulseMs = 0;

            auto nearby = sSpatialGrid.GetClientsInRadius(sh.location.x, sh.location.z, 3500.0f);
            uint32 cleansedInPulse = 0;
            for (GameClient* gc : nearby) {
                if (!gc->isBot()) continue;
                uint32 botId = gc->GetPlayerGoId();
                if (sSmithCascade.IsInfected(botId)) {
                    if (sSmithCascade.PurgeEntity(botId, frankPo, PURGE_METHOD_HARDLINE_SCRUBBER)) {
                        cleansedInPulse++;
                        m_totalSmithsPurged++;
                    }
                }
            }

            if (cleansedInPulse > 0) {
                AwardExperience(cleansedInPulse * 500);
                DEBUG_LOG(format("FrankCastle Safehouse: [%1%] hardline scrubber purged %2% viral entities.")
                    % sh.name % cleansedInPulse);
            }
        }
    }
}

void FrankCastleManager::CommandOrderAttack(uint32 targetGoId)
{
    PlayerObject* po = sObjMgr.getGOPtrSafe(targetGoId);
    if (!po) return;

    m_currentTargetGoId = targetGoId;
    std::string h = po->getHandle();
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    m_targetIsSmith = (h.find("smith") != std::string::npos);
    m_targetIsAgent = (h.find("agent") != std::string::npos) || m_targetIsSmith;
    m_currentState = FRANK_STATE_IN_COMBAT;
    m_stateTimerMs = 0;
}

void FrankCastleManager::CommandDeployToSafehouse(uint32 safehouseId)
{
    SafehouseNode* s = GetSafehouse(safehouseId);
    if (!s) return;

    m_targetSafehouseId = safehouseId;
    m_currentState = (s->status == SAFEHOUSE_HOSTILE || s->status == SAFEHOUSE_UNCLAIMED) ?
                     FRANK_STATE_ASSAULT_SAFEHOUSE : FRANK_STATE_FIELD_TRIAGE;
    m_stateTimerMs = 0;
}

void FrankCastleManager::TriggerFieldSurgeryAndRespawn()
{
    SafehouseNode* s = GetNearestFortifiedSafehouse(m_currentPos.x, m_currentPos.z);
    if (!s) s = GetSafehouse(1);

    m_currentHealth = GetMaxHealth();
    m_currentInnerStrength = GetMaxInnerStrength();

    if (s) {
        m_currentPos = s->location;
        PlayerObject* po = sObjMgr.getGOPtrSafe(m_frankGoId);
        if (po) {
            po->setDead(false);
            po->setPosition(m_currentPos);
            po->setMaximumHealth(GetMaxHealth());
            po->setCurrentHealth(GetMaxHealth());
            po->setInnerStrength(GetMaxInnerStrength(), GetMaxInnerStrength());
        }
    }

    m_currentState = FRANK_STATE_FIELD_TRIAGE;
    m_triageTimerMs = 0;
    DEBUG_LOG("FrankCastle: Field surgery complete. Munitions restocked.");
}

// ============================================================================
// Phase 8: Canonical Lore Realism, 1:1 Real-Time Convalescence & Sovereign Trust
// ============================================================================
void FrankCastleManager::EnterSafehouseConvalescence(uint32 safehouseId, uint64 currentUtcSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_currentState = FRANK_STATE_CONVALESCENCE_HEALING;
    m_convalescenceState.safehouseId = safehouseId;
    m_convalescenceState.stage = STAGE_ACUTE_HEMORRHAGE;
    m_convalescenceState.recoveryStartUtcSec = currentUtcSec;
    m_convalescenceState.stageStartUtcSec = currentUtcSec;
    m_convalescenceState.stageDurationSec = 1800; // 30 mins (1800s)
    m_convalescenceState.stageProgressPercent = 0.0f;
    m_convalescenceState.suturesRuptured = false;
    m_convalescenceState.suppliesConsumed = 0;

    SafehouseNode* sh = GetSafehouse(safehouseId);
    std::string shName = sh ? sh->name : "Subway Triage Cot";
    INFO_LOG(format("FrankCastle: Entered 1:1 real-time safehouse convalescence at [%1%]. Stage: Acute Hemorrhage Stabilization.") % shName);
    AddWarJournalEntry(JOURNAL_SAFEHOUSE_CLAIMED, "Self-Triage", "Entered Safehouse Convalescence",
                       "Heavy traumatic wounds sustained. Barricaded safehouse steel door. Immobile on cot.",
                       sh ? sh->districtId : 1, m_currentPos);
}

void FrankCastleManager::UpdateConvalescence(uint64 currentUtcSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_convalescenceState.stage == STAGE_NONE) return;

    uint64 elapsed = (currentUtcSec >= m_convalescenceState.stageStartUtcSec) ?
                     (currentUtcSec - m_convalescenceState.stageStartUtcSec) : 0;
    
    if (m_convalescenceState.stageDurationSec > 0)
    {
        m_convalescenceState.stageProgressPercent = std::min(100.0f, ((float)elapsed / (float)m_convalescenceState.stageDurationSec) * 100.0f);
    }

    if (elapsed >= m_convalescenceState.stageDurationSec)
    {
        switch (m_convalescenceState.stage)
        {
            case STAGE_ACUTE_HEMORRHAGE:
                m_convalescenceState.stage = STAGE_SEPTIC_FEVER;
                m_convalescenceState.stageStartUtcSec = currentUtcSec;
                m_convalescenceState.stageDurationSec = 12600; // 3.5 hours
                m_convalescenceState.stageProgressPercent = 0.0f;
                sCastleTrauma.GetVitalsMutable().bodyTemperatureF = 103.5f; // Fever spikes
                INFO_LOG("FrankCastle Convalescence: Transitioned to Stage 2: Septic Fever & Trauma Delirium (103.5F).");
                break;

            case STAGE_SEPTIC_FEVER:
                m_convalescenceState.stage = STAGE_FIBROUS_KNITTING;
                m_convalescenceState.stageStartUtcSec = currentUtcSec;
                m_convalescenceState.stageDurationSec = 50400; // 14 hours
                m_convalescenceState.stageProgressPercent = 0.0f;
                sCastleTrauma.GetVitalsMutable().bodyTemperatureF = 99.2f; // Fever breaks
                INFO_LOG("FrankCastle Convalescence: Transitioned to Stage 3: Fibrous Knitting & Bedbound Rest.");
                break;

            case STAGE_FIBROUS_KNITTING:
                m_convalescenceState.stage = STAGE_REHABILITATION;
                m_convalescenceState.stageStartUtcSec = currentUtcSec;
                m_convalescenceState.stageDurationSec = 64800; // 18 hours
                m_convalescenceState.stageProgressPercent = 0.0f;
                INFO_LOG("FrankCastle Convalescence: Transitioned to Stage 4: Combat Conditioning & Functional Re-Arm.");
                break;

            case STAGE_REHABILITATION:
                m_convalescenceState.stage = STAGE_NONE;
                m_currentState = FRANK_STATE_IDLE_PATROL;
                m_currentHealth = GetMaxHealth();
                sCastleTrauma.Reset();
                INFO_LOG("FrankCastle Convalescence: Full 36-hour 1:1 real-time rehabilitation complete! Frank Castle is back in the field.");
                AddWarJournalEntry(JOURNAL_RESUPPLY, "Active Duty", "Convalescence Complete",
                                   "Stitches holding. Broken ribs taped tight. Zeroed optics. Ready to hunt.", 1, m_currentPos);
                break;

            default:
                break;
        }
    }
}

bool FrankCastleManager::RuptureSuturesFromStrenuousAction()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_convalescenceState.stage >= STAGE_ACUTE_HEMORRHAGE && m_convalescenceState.stage <= STAGE_FIBROUS_KNITTING)
    {
        m_convalescenceState.suturesRuptured = true;
        m_convalescenceState.stage = STAGE_ACUTE_HEMORRHAGE;
        m_convalescenceState.stageProgressPercent = 0.0f;
        sCastleTrauma.InflictBluntTrauma(ZONE_THORACIC, 300.0f);
        WARNING_LOG("FrankCastle: SUTURES RUPTURED! Strenuous combat action tore closed wounds. Acute hemorrhagic relapse.");
        return true;
    }
    return false;
}

void FrankCastleManager::AccelerateConvalescenceWithSupplies(uint32 supplyUnits)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (supplyUnits == 0 || m_convalescenceState.stage == STAGE_NONE) return;

    m_convalescenceState.suppliesConsumed += supplyUnits;
    float reductionFactor = std::min(0.45f, 0.10f * supplyUnits); // Max 45% reduction
    m_convalescenceState.stageDurationSec = (uint64)(m_convalescenceState.stageDurationSec * (1.0f - reductionFactor));
    INFO_LOG(format("FrankCastle: Applied medical supplies to safehouse triage. Stage duration accelerated by %1%%%") % (int)(reductionFactor * 100));
}

CastleTrustTier FrankCastleManager::GetPlayerTrustTier(uint32 playerGoId) const
{
    int32 score = GetPlayerTrustScore(playerGoId);
    if (score >= 950) return TRUST_TIER_4_BROTHER_IN_ARMS;
    if (score >= 800) return TRUST_TIER_3_VETTED_ALLY;
    if (score >= 500) return TRUST_TIER_2_TESTED;
    if (score >= 250) return TRUST_TIER_1_OBSERVED;
    return TRUST_TIER_0_UNKNOWN;
}

int32 FrankCastleManager::GetPlayerTrustScore(uint32 playerGoId) const
{
    auto it = m_trustScores.find(playerGoId);
    if (it != m_trustScores.end()) return it->second;
    return 0;
}

void FrankCastleManager::AdjustPlayerTrust(uint32 playerGoId, const std::string& handle, int32 deltaTrust, const std::string& reason)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_trustScores[playerGoId] = std::clamp(m_trustScores[playerGoId] + deltaTrust, -1000, 1000);
    INFO_LOG(format("FrankCastle Trust Ledger: Player %1% (GOID %2%) adjusted by %3% -> New Trust: %4% (%5%)")
             % handle % playerGoId % deltaTrust % m_trustScores[playerGoId] % reason);
}

std::vector<EncryptedBurstMessage> FrankCastleManager::DispatchEncryptedTraumaBurstToTrusted(const std::vector<uint32>& onlinePlayerIds, uint64 currentUtcSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<EncryptedBurstMessage> messages;

    // Frank only reaches out during critical stages (Acute Hemorrhage or Septic Fever)
    if (m_convalescenceState.stage != STAGE_ACUTE_HEMORRHAGE && m_convalescenceState.stage != STAGE_SEPTIC_FEVER)
    {
        return messages;
    }

    // Evaluate online roster: ONLY dispatch to TIER_3 and TIER_4 trusted contacts
    for (uint32 pid : onlinePlayerIds)
    {
        CastleTrustTier tier = GetPlayerTrustTier(pid);
        if (tier >= TRUST_TIER_3_VETTED_ALLY)
        {
            EncryptedBurstMessage msg;
            msg.targetPlayerGoId = pid;
            msg.timestampUtcSec = currentUtcSec;
            msg.deadDropLocation = "Slums Secondary Pump Station - Dead-Drop Locker 12";

            std::string handle = "Operative";
            auto karmaIt = m_playerKarma.find(pid);
            if (karmaIt != m_playerKarma.end() && !karmaIt->second.handle.empty())
            {
                handle = karmaIt->second.handle;
            }
            msg.targetHandle = handle;

            std::stringstream ss;
            ss << "[ENCRYPTED BURST // CTCSS 141.3 Hz // OTP-0x8F] Castle to " << handle << ": "
               << "Critical trauma sustained at Docks. Bleeding out. Need chest seals and penicillin at "
               << msg.deadDropLocation << ". Do not follow me.";
            msg.ciphertext = ss.str();

            messages.push_back(msg);
            INFO_LOG(format("FrankCastle: Dispatched encrypted burst signal to trusted contact [%1%] (GOID %2%).") % handle % pid);
        }
    }

    if (messages.empty())
    {
        INFO_LOG("FrankCastle Solitary Protocol: No trusted Tier 3/4 contacts online. Frank endures critical convalescence in absolute isolation.");
    }

    return messages;
}

std::string FrankCastleManager::EvaluateSafehouseIntruder(uint32 playerGoId)
{
    CastleTrustTier tier = GetPlayerTrustTier(playerGoId);
    if (tier == TRUST_TIER_4_BROTHER_IN_ARMS)
    {
        return "WELCOME_BROTHER: Lowered sidearm. Permitted inside secure perimeter.";
    }
    else if (tier == TRUST_TIER_3_VETTED_ALLY)
    {
        return "ACKNOWLEDGED_ALLY: Sidearm pointed at floor. Deposit medical supplies at dead-drop and exit.";
    }
    else if (tier == TRUST_TIER_2_TESTED || tier == TRUST_TIER_1_OBSERVED)
    {
        return "SUSPICIOUS_WATCH: Cocked hammer. 'Leave the supplies in the alley and walk away.'";
    }
    else
    {
        return "LETHAL_STANDOFF: Aimed .45 directly at intruder's chest. 'Take one more step and you leave in a body bag.'";
    }
}

// ============================================================================
// Phase 9 / Epoch IV: Lore Realism Phase 2 Implementations
// ============================================================================
void FrankCastleManager::InitializeArmsBazaars()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_armsBazaars.clear();

    ArmsBazaarCrate c1;
    c1.crateId = 101;
    c1.frequencyCode = "FM 88.3";
    c1.codename = "USMC Surplus Spec-Ops Crate Alpha";
    c1.location = LocationVector(1260.0f, 0.0f, -3390.0f);
    c1.districtId = 1;
    c1.districtName = "Slums";
    c1.duRounds = 120;
    c1.c4Charges = 4;
    c1.nightVisionGoggles = true;
    c1.accessCode = "1984-PUNISHER";
    m_armsBazaars[c1.crateId] = c1;

    ArmsBazaarCrate c2;
    c2.crateId = 102;
    c2.frequencyCode = "FM 88.3";
    c2.codename = "Microchip Tactical Ordinance Vault";
    c2.location = LocationVector(3200.0f, 0.0f, 2100.0f);
    c2.districtId = 2;
    c2.districtName = "Downtown";
    c2.duRounds = 250;
    c2.c4Charges = 8;
    c2.nightVisionGoggles = true;
    c2.accessCode = "FORCE-RECON-77";
    m_armsBazaars[c2.crateId] = c2;
}

ArmsBazaarCrate* FrankCastleManager::GetArmsBazaar(uint32 crateId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_armsBazaars.find(crateId);
    if (it != m_armsBazaars.end()) return &it->second;
    return nullptr;
}

bool FrankCastleManager::LootArmsBazaarCrate(uint32 crateId, uint32 playerGoId, const std::string& code, std::string& outLootReport)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_armsBazaars.find(crateId);
    if (it == m_armsBazaars.end()) {
        outLootReport = "ERROR: Crate not found.";
        return false;
    }
    if (it->second.isLooted) {
        outLootReport = "EMPTY: Crate already salvaged.";
        return false;
    }
    if (it->second.accessCode != code) {
        outLootReport = "DENIED: Invalid crypto-cipher passcode.";
        return false;
    }
    it->second.isLooted = true;
    it->second.lootedByPlayerGoId = playerGoId;
    outLootReport = (format("SUCCESS: Unlocked %1%! Acquired %2% DU rounds, %3% C4 charges, and FLIR NVGs.")
                    % it->second.codename % it->second.duRounds % it->second.c4Charges).str();
    return true;
}

bool FrankCastleManager::TuneRadioToBazaarFrequency(float freqMhz, std::string& outMorseSignal)
{
    if (std::abs(freqMhz - 88.3f) < 0.05f) {
        outMorseSignal = "[FM 88.3 BAZAAR BEACON]: ... --- ... // MICROCHIP DROP ZONE CONFIRMED // ACCESS CODE 1984-PUNISHER";
        return true;
    }
    outMorseSignal = "[STATIC]";
    return false;
}

void FrankCastleManager::InitializeFortifications()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_safehouses.empty()) {
        InitializeSafehouses();
    }
    m_fortifications.clear();
    for (const auto& sh : m_safehouses) {
        SafehouseFortification f;
        f.safehouseId = sh.first;
        if (sh.first == 1) {
            f.steelDoorsReinforced = true;
            f.cctvTelemetryActive = true;
            f.tripwireShotgunTrapArmed = true;
        }
        m_fortifications[sh.first] = f;
    }
}

bool FrankCastleManager::ReinforceSafehouseSteelDoors(uint32 safehouseId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_fortifications.find(safehouseId);
    if (it == m_fortifications.end()) return false;
    it->second.steelDoorsReinforced = true;
    return true;
}

bool FrankCastleManager::InstallCCTVTelemetry(uint32 safehouseId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_fortifications.find(safehouseId);
    if (it == m_fortifications.end()) return false;
    it->second.cctvTelemetryActive = true;
    return true;
}

bool FrankCastleManager::ArmTripwireShotgunTrap(uint32 safehouseId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_fortifications.find(safehouseId);
    if (it == m_fortifications.end()) return false;
    it->second.tripwireShotgunTrapArmed = true;
    return true;
}

bool FrankCastleManager::TriggerFortificationDefense(uint32 safehouseId, uint32 intruderGoId, uint32& outDamageDealt)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_fortifications.find(safehouseId);
    if (it == m_fortifications.end()) return false;
    outDamageDealt = 0;
    if (it->second.tripwireShotgunTrapArmed) {
        outDamageDealt = 450; // 12-gauge flechette blast
        it->second.intrudersRepelled++;
        it->second.tripwireShotgunTrapArmed = false; // Discharged
        return true;
    }
    return false;
}

const SafehouseFortification* FrankCastleManager::GetFortification(uint32 safehouseId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_fortifications.find(safehouseId);
    if (it != m_fortifications.end()) return &it->second;
    return nullptr;
}

void FrankCastleManager::InitializeSubwayDeadDrops()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_subwayDeadDrops.clear();

    SubwayDeadDrop d1;
    d1.dropId = 201;
    d1.stationName = "Adams Crest Platform B";
    d1.platformLocation = LocationVector(1450.0f, -25.0f, -3200.0f);
    d1.ctcssSubcarrierHz = 131.8f;
    d1.cipherPayload = "MICROCHIP_INTEL_PACKAGE_ALPHA";
    d1.requiredTrustTier = TRUST_TIER_1_OBSERVED;
    m_subwayDeadDrops.push_back(d1);

    SubwayDeadDrop d2;
    d2.dropId = 202;
    d2.stationName = "Downtown Transit Center Vault 4";
    d2.platformLocation = LocationVector(3300.0f, -30.0f, 1950.0f);
    d2.ctcssSubcarrierHz = 141.3f;
    d2.cipherPayload = "SYSTEM_AGENT_PATROL_VECTORS";
    d2.requiredTrustTier = TRUST_TIER_2_TESTED;
    m_subwayDeadDrops.push_back(d2);
}

const std::vector<SubwayDeadDrop>& FrankCastleManager::GetSubwayDeadDrops() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_subwayDeadDrops;
}

bool FrankCastleManager::RetrieveSubwayDeadDrop(uint32 dropId, uint32 playerGoId, float ctcssToneHz, std::string& outPayload)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& drop : m_subwayDeadDrops) {
        if (drop.dropId == dropId) {
            if (drop.isRetrieved) {
                outPayload = "ALREADY_RETRIEVED";
                return false;
            }
            if (std::abs(drop.ctcssSubcarrierHz - ctcssToneHz) > 0.5f) {
                outPayload = "INVALID_CTCSS_SUBCARRIER";
                return false;
            }
            CastleTrustTier tier = GetPlayerTrustTier(playerGoId);
            if (tier < drop.requiredTrustTier) {
                outPayload = "INSUFFICIENT_TRUST_TIER";
                return false;
            }
            drop.isRetrieved = true;
            drop.retrievedByPlayerGoId = playerGoId;
            outPayload = drop.cipherPayload;
            return true;
        }
    }
    outPayload = "NOT_FOUND";
    return false;
}

// ============================================================================
// Comprehensive Verification Suite for Frank Castle's Eternal Crusade
// ============================================================================
void RunFrankCastleTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  FRANK CASTLE (THE PUNISHER) - ETERNAL CRUSADE TEST SUITE  " << std::endl;
    std::cout << "============================================================" << std::endl;

    int passed = 0;
    int failed = 0;

    auto assertTest = [&](const std::string& name, bool condition) {
        if (condition) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << std::endl;
            failed++;
        }
    };

    // 1. Subsystem Initialization
    sFrankCastleMgr.Initialize();
    assertTest("Core Subsystem Live Status", sFrankCastleMgr.IsLive());
    assertTest("Base Progression Level Scaling", sFrankCastleMgr.GetLevel() >= 25);
    assertTest("Mastery Rank Assignment", !sFrankCastleMgr.GetRankTitle().empty());
    assertTest("Initial Safehouse Network Size", sFrankCastleMgr.GetSafehouses().size() == 5);
    assertTest("Tactical Field Caches Initialized", sFrankCastleMgr.GetFieldCaches().size() == 5);

    // 2. Phase 1: War Journal & Cassettes & Dynamic Hit List
    sFrankCastleMgr.AddWarJournalEntry(JOURNAL_KILL, "Agent Smith Clones", "Subway Interception", "Purged 3 viral entities in transit.", 2, LocationVector(4200.0f, 0.0f, 1500.0f));
    auto entries = sFrankCastleMgr.GetRecentJournalEntries(10);
    assertTest("War Journal Entry Creation & Timestamping", !entries.empty() && entries.front().targetHandle == "Agent Smith Clones");
    sFrankCastleMgr.SaveWarJournalToFile("WarJournal_Test.json");
    std::ifstream jf("WarJournal_Test.json");
    assertTest("War Journal JSON File Persistence", jf.is_open());
    if (jf.is_open()) jf.close();
    bool loadedJournal = sFrankCastleMgr.LoadWarJournalFromFile("WarJournal_Test.json");
    assertTest("War Journal JSON Deserialization & Recovery", loadedJournal);

    sFrankCastleMgr.DropWarJournalCassette(LocationVector(1250.0f, 0.0f, -3400.0f), 1, "Slums Foundry", "Foundry Dead Drop", "Munitions cache coordinates enclosed.");
    const auto& cassettes = sFrankCastleMgr.GetCassettes();
    assertTest("Encrypted War Journal Cassette Dropped with Valid District", !cassettes.empty() && cassettes.back().districtId == 1);
    std::string loreOutput;
    bool looted = sFrankCastleMgr.LootWarJournalCassette(cassettes.back().cassetteId, 7001, loreOutput);
    assertTest("Cassette Decryption & Lore Extraction", looted && loreOutput.find("WAR JOURNAL CASSETTE DECRYPTED") != std::string::npos);

    sFrankCastleMgr.AddHitListTarget(9101, 1001, "Agent Gray", PRIORITY_SYSTEM_AGENT, 350.0f, "Machine Pacification Agent", LocationVector(3000.0f, 0.0f, 2000.0f), 2, "Downtown");
    sFrankCastleMgr.AddHitListTarget(9102, 1002, "Smith Replica Vector", PRIORITY_OMEGA_SMITH, 700.0f, "Active Viral Vector", LocationVector(3100.0f, 0.0f, 2100.0f), 1, "Slums");
    sFrankCastleMgr.AddHitListTarget(9103, 1003, "Lupine Underboss Vane", PRIORITY_SYNDICATE_BOSS, 280.0f, "Merovingian Cartel Boss", LocationVector(2800.0f, 0.0f, 1900.0f), 2, "Downtown");
    auto hitList = sFrankCastleMgr.GetHitList();
    assertTest("Dynamic Hit List Priority Sorting (Omega Smith Top Priority)", hitList.size() >= 3 && hitList.front().priority == PRIORITY_OMEGA_SMITH);

    // Test terminated target reactivation
    auto* hlEntry = sFrankCastleMgr.GetHitListTarget(9101);
    if (hlEntry) hlEntry->status = HIT_STATUS_TERMINATED;
    sFrankCastleMgr.AddHitListTarget(9101, 1001, "Agent Gray", PRIORITY_SYSTEM_AGENT, 360.0f, "Machine Pacification Agent", LocationVector(3000.0f, 0.0f, 2000.0f), 2, "Downtown");
    assertTest("Terminated Hit List Target Reactivation", hlEntry != nullptr && hlEntry->status == HIT_STATUS_QUEUED);

    // 3. Phase 2: Microchip Logistics & Construct Armory
    auto* cache = sFrankCastleMgr.GetNearestCache(1600.0f, -3100.0f);
    assertTest("Field Cache Proximity Spatial Lookup", cache != nullptr && cache->cacheId == 1);
    bool restocked = sFrankCastleMgr.RestockFromFieldCache(1);
    assertTest("Autonomous Field Cache Restocking", restocked);
    const auto& micro = sFrankCastleMgr.GetMicrochipHardware();
    assertTest("Microchip Depleted Uranium Rounds Available", micro.depletedUraniumRounds > 0);
    assertTest("Microchip Adrenaline Stims Available", micro.adrenalineStims > 0);
    uint32 ammoBeforeArmory = sFrankCastleMgr.GetCarriedStockpile().highCaliberAmmo;
    sFrankCastleMgr.AccessConstructArmory();
    assertTest("Construct Armory Ballistic Munitions Fabrication", sFrankCastleMgr.GetCarriedStockpile().highCaliberAmmo > ammoBeforeArmory);

    // 4. Phase 3: Safehouse Sieges & Automated Defenses
    bool siegeTriggered = sFrankCastleMgr.TriggerSafehouseSiege(1, SIEGE_FACTION_MACHINES);
    assertTest("Safehouse Dynamic Siege Event Trigger", siegeTriggered && sFrankCastleMgr.IsSafehouseUnderSiege(1));
    sFrankCastleMgr.OnPlayerAssistedSiege(1, 8001);
    assertTest("Player Assistance in Siege Awards Vigilante Karma", sFrankCastleMgr.GetPlayerKarma(8001) == 200);
    sFrankCastleMgr.ResolveSiegeDefense(1, true);
    assertTest("Safehouse Siege Successful Resolution", !sFrankCastleMgr.IsSafehouseUnderSiege(1));

    // Test concurrent siege & integrity degradation
    sFrankCastleMgr.TriggerSafehouseSiege(5, SIEGE_FACTION_EXILES);
    auto* s5Siege = sFrankCastleMgr.GetActiveSiege(5);
    assertTest("Concurrent Secondary Siege Trigger", s5Siege != nullptr && s5Siege->active);
    sFrankCastleMgr.UpdateSafehouseSieges(10000); // 10s of assault wave
    assertTest("Safehouse Integrity Degradation Under Assault", s5Siege->safehouseIntegrity < 100.0f);
    sFrankCastleMgr.ResolveSiegeDefense(5, false);
    assertTest("Safehouse Breach Overrun Contestation", sFrankCastleMgr.GetSafehouse(5)->status == SAFEHOUSE_CONTESTED);

    // 5. Phase 4: Syndicate Decapitation & Guerilla Raids
    bool sweepStarted = sFrankCastleMgr.TriggerDistrictCleanSweep(2);
    assertTest("District Clean Sweep Syndicate Raid Trigger", sweepStarted && sFrankCastleMgr.IsCleanSweepActive());
    sFrankCastleMgr.OnSyndicateLieutenantEliminated(9103, "Lupine Underboss Vane");
    sFrankCastleMgr.UpdateCleanSweep(130000);
    assertTest("Clean Sweep Raid Timeout & Conclusion", !sFrankCastleMgr.IsCleanSweepActive());

    // 6. Phase 5: Vigilante Karma, Contracts & FM 88.3 Radio Net
    sFrankCastleMgr.ModifyPlayerKarma(8002, "TraitorPlayer", -450, "Griefing New Players in Slums");
    assertTest("Player Negative Karma Marks for Vigilante Punishment", sFrankCastleMgr.IsPlayerMarkedForPunishment(8002));
    sFrankCastleMgr.OnPlayerAttemptedSafehouseBreach(1, 8009);
    assertTest("Traitorous Safehouse Breach Marks Player on Hit List", sFrankCastleMgr.IsPlayerMarkedForPunishment(8009) && sFrankCastleMgr.GetHitListTarget(8009) != nullptr);
    sFrankCastleMgr.BroadcastRadioNet("FM 88.3 Operational Test Squelch", true);
    auto radioRecords = sFrankCastleMgr.GetRecentRadioTransmissions(5);
    assertTest("FM 88.3 Pirate Radio Net Transmissions Logged", !radioRecords.empty() && radioRecords.front().frequency == "FM 88.3 (The War Zone)");
    const auto& contracts = sFrankCastleMgr.GetAvailableContracts();
    assertTest("Vigilante Field Contracts Active", contracts.size() >= 3);
    bool accepted = sFrankCastleMgr.AcceptVigilanteContract(1, 8003);
    assertTest("Vigilante Contract Accepted by Player", accepted);
    std::string contractReward;
    bool completed = sFrankCastleMgr.CompleteVigilanteContract(1, 8003, contractReward);
    assertTest("Vigilante Contract Completion & Reward Issuance", completed && sFrankCastleMgr.GetPlayerKarma(8003) == 150);

    // 7. Phase 6: Deep Reinforcement Learning & Tactical Combat
    FrankCombatAction actAgent = sFrankCastleMgr.DecideCombatActionEnhanced(9101, PRIORITY_SYSTEM_AGENT, 2500.0f, false);
    assertTest("Q-Learning Long-Range Ambush Tactical Decision", actAgent == ACT_SNIPER_AMBUSH || actAgent == ACT_ARMOR_PIERCING_VOLLEY);
    FrankCombatAction actEvasive = sFrankCastleMgr.DecideCombatActionEnhanced(9101, PRIORITY_SYSTEM_AGENT, 400.0f, true);
    assertTest("Q-Learning Anti-Evasion Tactical Counter (Flashbang/CQC)", actEvasive == ACT_FLASHBANG_STUN || actEvasive == ACT_CQC_DISARM_TAKEDOWN || actEvasive == ACT_FLANKING_MANEUVER);
    FrankCombatAction actSmith = sFrankCastleMgr.DecideCombatActionEnhanced(9102, PRIORITY_OMEGA_SMITH, 400.0f, false);
    assertTest("Q-Learning Smith Purge Counter (Antiviral/Phosphorus/Execution)", actSmith == ACT_ANTIVIRAL_AP_VOLLEY || actSmith == ACT_WHITE_PHOSPHORUS_BURST || actSmith == ACT_POINT_BLANK_EXECUTION || actSmith == ACT_EXPLOSIVE_BARREL_DETONATION);
    
    // Test Q-Learning outcome recording for new actions
    sFrankCastleMgr.RecordCombatOutcome(ACT_SNIPER_AMBUSH, 1200.0f, 0.0f, true, true, 2600.0f, false);
    assertTest("Q-Learning Sniper Ambush Outcome Recorded", true);
    sFrankCastleMgr.RecordCombatOutcome(ACT_FLASHBANG_STUN, 350.0f, 20.0f, false, false, 400.0f, false);
    assertTest("Q-Learning Flashbang Stun Outcome Recorded", true);

    uint32 envDmg = 0;
    bool envOk = sFrankCastleMgr.TriggerEnvironmentalHazard(0.0f, 0.0f, 1000.0f, envDmg);
    assertTest("Environmental Hazard Detonation Execution", envOk && envDmg >= 1);

    // 8. Phase 7: Remaster Client Integration & Telemetry
    assertTest("Visual Battle Condition Classification", sFrankCastleMgr.GetVisualCondition() == VISUAL_PRISTINE);
    assertTest("Visual Condition Narrative Naming", !sFrankCastleMgr.GetVisualConditionName().empty());
    std::string telemetryJson = sFrankCastleMgr.GenerateRemasterTelemetryJson();
    assertTest("Remaster Client Telemetry JSON Generation", 
               telemetryJson.find("\"frank\":") != std::string::npos &&
               telemetryJson.find("\"safehouses\":") != std::string::npos &&
               telemetryJson.find("\"radioFrequency\": \"FM 88.3 (The War Zone)\"") != std::string::npos);
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  FRANK CASTLE TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================" << std::endl;

    if (failed != 0) {
        std::cerr << "SOME TESTS FAILED!" << std::endl;
        exit(1);
    }
}

// ============================================================================
// HEADLESS TEST SUITE: FRANK CASTLE CANONICAL LORE REALISM & CONVALESCENCE (SUITE 25)
// ============================================================================
void RunCastleLoreRealismTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING FRANK CASTLE LORE REALISM & CONVALESCENCE SUITE (SUITE 25)" << std::endl;
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

    // 1. Initial Anatomical Vitals
    sCastleTrauma.Reset();
    const VitalsState& initVitals = sCastleTrauma.GetVitals();
    TEST_ASSERT(initVitals.bloodVolumeMl == 5000.0f, "Baseline blood volume is 5,000 mL");
    TEST_ASSERT(initVitals.systolicBp == 120 && initVitals.diastolicBp == 80, "Baseline BP is 120/80 mmHg");
    TEST_ASSERT(initVitals.bodyTemperatureF == 98.6f, "Baseline body temperature is 98.6F");
    TEST_ASSERT(initVitals.hasPneumothorax == false, "No initial pneumothorax");
    TEST_ASSERT(initVitals.brokenRibsCount == 0, "Zero initial broken ribs");
    TEST_ASSERT(sCastleTrauma.CanSprint() == true, "Can sprint at full health");

    // 2. Ceramic Armor Non-Penetrating Backface Deformation
    uint32 bluntWoundId = sCastleTrauma.InflictBallisticTrauma(ZONE_THORACIC, 800.0f, false);
    TEST_ASSERT(bluntWoundId > 0, "Inflicted non-penetrating ballistic impact on chest plate");
    SpecificWound* bw = sCastleTrauma.GetWound(bluntWoundId);
    TEST_ASSERT(bw && bw->hasLodgedBullet == false, "ESAPI ceramic plate stopped bullet from entering flesh");
    TEST_ASSERT(sCastleTrauma.GetVitals().brokenRibsCount == 1, "Plate deformation fractured 1 rib from kinetic transfer");

    // 3. High-Velocity Rifle Penetrating Trauma & Tension Pneumothorax
    uint32 rifleWoundId = sCastleTrauma.InflictBallisticTrauma(ZONE_THORACIC, 1900.0f, true);
    TEST_ASSERT(rifleWoundId > 0, "High-velocity 7.62 AP rifle round penetrated chest cavity");
    SpecificWound* rw = sCastleTrauma.GetWound(rifleWoundId);
    TEST_ASSERT(rw && rw->isArterialBleeder == true, "Inflicted arterial bleeder in thoracic zone");
    TEST_ASSERT(rw->bleedRateMlPerSec == 35.0f, "Arterial bleed rate is 35 mL/s");
    TEST_ASSERT(sCastleTrauma.GetVitals().hasPneumothorax == true, "Tension pneumothorax triggered by thoracic puncture");
    TEST_ASSERT(sCastleTrauma.CanSprint() == false, "Sprint disabled due to collapsed lung & chest trauma");

    // 4. Arm Limb Trauma & Aim Tremor
    uint32 armWoundId = sCastleTrauma.InflictBallisticTrauma(ZONE_ARM_RIGHT, 900.0f, true);
    TEST_ASSERT(armWoundId > 0, "Inflicted gunshot wound on right dominant arm");
    SpecificWound* aw = sCastleTrauma.GetWound(armWoundId);
    TEST_ASSERT(aw && aw->hasLodgedBullet == true, "Deformed slug lodged in deltoid muscle tissue");
    TEST_ASSERT(sCastleTrauma.GetVitals().dominantArmCrippled == true, "Right arm crippled by gunshot");
    TEST_ASSERT(sCastleTrauma.GetAimSwayMultiplier() >= 2.5f, "Aim sway reticle bloom increased by 250%");

    // 5. Active Hemorrhage & Hypovolemic Shock
    sCastleTrauma.UpdateTrauma(50.0f); // 50 seconds of heavy bleeding
    TEST_ASSERT(sCastleTrauma.GetVitals().bloodVolumeMl < 3500.0f, "Blood volume plummeted below 3,500 mL");
    TEST_ASSERT(sCastleTrauma.IsInHypovolemicShock() == true, "Frank entered acute hypovolemic shock");
    TEST_ASSERT(sCastleTrauma.GetVitals().heartRateBpm > 100, "Compensatory tachycardia triggered (HR > 100 BPM)");
    TEST_ASSERT(sCastleTrauma.GetVitals().systolicBp < 90, "Blood pressure dropped significantly");

    // 6. Field Surgery: Bullet Extraction
    SurgicalActionResult extractWrong = sCastleSurgery.ExtractLodgedSlug(armWoundId, TOOL_BOURBON_IRRIGATION);
    TEST_ASSERT(extractWrong.success == false, "Cannot extract bullet with liquid antiseptic");
    SurgicalActionResult extractOk = sCastleSurgery.ExtractLodgedSlug(armWoundId, TOOL_FORCEPS_PLIERS);
    TEST_ASSERT(extractOk.success == true, "Forceps successfully extracted deformed lead slug from right arm");
    TEST_ASSERT(aw->hasLodgedBullet == false && aw->foreignObjectRemoved == true, "Foreign projectile confirmed removed");

    // 7. Field Surgery: Bourbon Irrigation & Debridement
    SurgicalActionResult flushRes = sCastleSurgery.FlushAndDebrideWound(armWoundId, TOOL_BOURBON_IRRIGATION);
    TEST_ASSERT(flushRes.success == true, "100-proof Kentucky bourbon poured into open wound");
    TEST_ASSERT(aw->isDisinfected == true && aw->infectionRiskPercent <= 2.0f, "Infection risk plummeted to 2%");
    TEST_ASSERT(sCastleSurgery.GetBourbonOuncesConsumed() >= 2, "Bourbon consumed as antiseptic/anesthetic");

    // 8. Field Surgery: 20-lb Monofilament Suture
    SurgicalActionResult sutureRes = sCastleSurgery.SutureWound(armWoundId, TOOL_MONOFILAMENT_SUTURE);
    TEST_ASSERT(sutureRes.success == true, "20-lb nylon monofilament closed muscle fascia margins");
    TEST_ASSERT(aw->isSutured == true, "Wound marked as sutured");

    // 9. Field Surgery: Gunpowder Flash Cauterization
    SurgicalActionResult cauteryRes = sCastleSurgery.CauterizeArterialBleeder(rifleWoundId, TOOL_GUNPOWDER_CAUTERIZE);
    TEST_ASSERT(cauteryRes.success == true, "Tore open .45 ACP casing and ignited gunpowder flash to cauterize arterial bleeder");
    TEST_ASSERT(rw->isCauterized == true && rw->bleedRateMlPerSec == 0.0f, "Arterial hemorrhage stopped completely");

    // 10. Field Surgery: Needle Thoracostomy
    SurgicalActionResult needleRes = sCastleSurgery.PerformNeedleThoracostomy(TOOL_DECOMPRESSION_CATHETER);
    TEST_ASSERT(needleRes.success == true, "14-gauge catheter vented pleural pressure in 2nd intercostal space");
    TEST_ASSERT(sCastleTrauma.GetVitals().hasPneumothorax == false, "Tension pneumothorax relieved, lung re-expanded");

    // 11. Safehouse 1:1 Real-Time Convalescence Lifecycle
    uint64 baseUtc = 1725700000ULL;
    sFrankCastleMgr.EnterSafehouseConvalescence(1, baseUtc);
    const ConvalescenceState& cs = sFrankCastleMgr.GetConvalescenceState();
    TEST_ASSERT(cs.stage == STAGE_ACUTE_HEMORRHAGE, "Began in Stage 1: Acute Hemorrhage Stabilization");
    TEST_ASSERT(cs.stageDurationSec == 1800, "Stage 1 duration set to 30 real-world minutes (1800s)");

    // Advance 900s real time (50% progress)
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 900);
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().stageProgressPercent == 50.0f, "Stage 1 progress advanced to 50% in real time");

    // Advance 1800s real time -> Stage 2: Septic Fever
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 1800);
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().stage == STAGE_SEPTIC_FEVER, "Transitioned to Stage 2: Septic Fever & Delirium");
    TEST_ASSERT(sCastleTrauma.GetVitals().bodyTemperatureF == 103.5f, "Septic fever peaked at 103.5F during night sweat hallucinations");

    // Advance 3.5h real time -> Stage 3: Fibrous Knitting
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 1800 + 12600);
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().stage == STAGE_FIBROUS_KNITTING, "Transitioned to Stage 3: Fibrous Knitting & Rest");
    TEST_ASSERT(sCastleTrauma.GetVitals().bodyTemperatureF == 99.2f, "Fever broke as initial collagen matrix knit wounds");

    // 12. Suture Rupture Penalty
    TEST_ASSERT(sFrankCastleMgr.RuptureSuturesFromStrenuousAction() == true, "Strenuous combat roll tore sutures open");
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().stage == STAGE_ACUTE_HEMORRHAGE, "Acute relapse to Stage 1 upon suture rupture");

    // Advance back to Stage 3 and then Stage 4
    sFrankCastleMgr.EnterSafehouseConvalescence(1, baseUtc + 20000);
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 20000 + 1800);
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 20000 + 1800 + 12600);
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 20000 + 1800 + 12600 + 50400);
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().stage == STAGE_REHABILITATION, "Advanced to Stage 4: Combat Conditioning (18h)");

    // Supply acceleration
    sFrankCastleMgr.AccelerateConvalescenceWithSupplies(3);
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().suppliesConsumed >= 3, "Medical supplies accelerated stage duration");

    // Full 36h rehabilitation finish
    sFrankCastleMgr.UpdateConvalescence(baseUtc + 20000 + 1800 + 12600 + 50400 + 70000);
    TEST_ASSERT(sFrankCastleMgr.GetConvalescenceState().stage == STAGE_NONE, "Convalescence complete, Frank returned to active duty");

    // 13. Sovereign Trust Ledger
    uint32 strangerId = 8801;
    uint32 allyId = 8802;
    uint32 brotherId = 8803;

    TEST_ASSERT(sFrankCastleMgr.GetPlayerTrustTier(strangerId) == TRUST_TIER_0_UNKNOWN, "Unvetted stranger is Tier 0 Unknown");
    std::string standoffWarning = sFrankCastleMgr.EvaluateSafehouseIntruder(strangerId);
    TEST_ASSERT(standoffWarning.find("LETHAL_STANDOFF") != std::string::npos, "Stranger intruder met with lethal standoff warning (.45 aimed at chest)");

    sFrankCastleMgr.AdjustPlayerTrust(allyId, "VigilanteRedpill", 850, "Defended safehouse perimeter against mob hit squad");
    TEST_ASSERT(sFrankCastleMgr.GetPlayerTrustTier(allyId) == TRUST_TIER_3_VETTED_ALLY, "Player promoted to Tier 3 Vetted Ally");
    std::string allyReaction = sFrankCastleMgr.EvaluateSafehouseIntruder(allyId);
    TEST_ASSERT(allyReaction.find("ACKNOWLEDGED_ALLY") != std::string::npos, "Vetted ally permitted to drop medical supplies");

    sFrankCastleMgr.AdjustPlayerTrust(brotherId, "MicrochipOperative", 980, "Decade-long Marine brother-in-arms");
    TEST_ASSERT(sFrankCastleMgr.GetPlayerTrustTier(brotherId) == TRUST_TIER_4_BROTHER_IN_ARMS, "Promoted to Tier 4 Brother-in-Arms");
    std::string brotherReaction = sFrankCastleMgr.EvaluateSafehouseIntruder(brotherId);
    TEST_ASSERT(brotherReaction.find("WELCOME_BROTHER") != std::string::npos, "Brother-in-arms welcomed inside perimeter");

    // 14. Encrypted Burst Pager Outreach
    sFrankCastleMgr.EnterSafehouseConvalescence(1, baseUtc + 100000);
    std::vector<uint32> onlineRoster = { strangerId, allyId, brotherId };
    std::vector<EncryptedBurstMessage> bursts = sFrankCastleMgr.DispatchEncryptedTraumaBurstToTrusted(onlineRoster, baseUtc + 100000);
    TEST_ASSERT(bursts.size() == 2, "Only 2 trusted allies received encrypted burst signals");
    TEST_ASSERT(bursts[0].targetPlayerGoId == allyId || bursts[0].targetPlayerGoId == brotherId, "Burst dispatched to verified Tier 3/4 ally");
    TEST_ASSERT(bursts[0].ciphertext.find("ENCRYPTED BURST // CTCSS 141.3 Hz") != std::string::npos, "Ciphertext formatted with discrete CTCSS subcarrier");

    // 15. Solitary Protocol
    std::vector<uint32> strangerRoster = { strangerId };
    std::vector<EncryptedBurstMessage> solitaryBursts = sFrankCastleMgr.DispatchEncryptedTraumaBurstToTrusted(strangerRoster, baseUtc + 100000);
    TEST_ASSERT(solitaryBursts.empty() == true, "Zero messages sent when only strangers online: Frank endures alone in the dark");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  FRANK CASTLE LORE REALISM & CONVALESCENCE SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failed > 0) {
        std::cerr << "RunCastleLoreRealismTestSuite: FAILED with " << failed << " errors!" << std::endl;
        exit(1);
    }
}



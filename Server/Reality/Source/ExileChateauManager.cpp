#include "ExileChateauManager.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cassert>

// Singleton instantiation
createFileSingleton(ExileChateauManager);

ExileChateauManager::ExileChateauManager()
{
    Initialize();
}

ExileChateauManager::~ExileChateauManager()
{
}

void ExileChateauManager::Initialize()
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    if (m_initialized) return;

    InitializeExileHierarchy();
    InitializeNightmareBestiary();
    InitializeBackdoorNetwork();
    InitializeClubHelArena();

    m_initialized = true;
}

void ExileChateauManager::Reset()
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    m_supernaturals.clear();
    m_backdoorPortals.clear();
    m_goldenKeys.clear();
    m_causalityRecords.clear();
    m_trainTickets.clear();
    m_clubHelEnforcers.clear();
    m_persephoneContracts.clear();

    m_nextProgramId = 3001;
    m_nextPortalId = 8001;
    m_nextKeyId = 9001;
    m_nextCausalityId = 4001;
    m_nextTicketId = 6001;
    m_nextEnforcerId = 5001;
    m_nextContractId = 2001;

    m_trainmanInLimboInvulnerable = true;
    m_totalContrabandBitsSmuggled = 0.0;
    m_totalBackdoorTransits = 0;
    m_totalVampiresDeRezzedBySilver = 0;
    m_initialized = false;

    InitializeExileHierarchy();
    InitializeNightmareBestiary();
    InitializeBackdoorNetwork();
    InitializeClubHelArena();

    m_initialized = true;
}

void ExileChateauManager::Update(uint32 deltaMs)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    if (!m_initialized) return;

    // Decay anomaly ripples over time across backdoor portals
    float decay = (static_cast<float>(deltaMs) / 1000.0f) * 0.02f;
    for (auto& pair : m_backdoorPortals) {
        if (pair.second.anomalyTelemetryRipple > 0.0f) {
            pair.second.anomalyTelemetryRipple = std::max(0.0f, pair.second.anomalyTelemetryRipple - decay);
            if (pair.second.anomalyTelemetryRipple < 0.3f) {
                pair.second.isBreachedByAgents = false;
            }
        }
    }
}

void ExileChateauManager::InitializeExileHierarchy()
{
    // The Merovingian Court initial settings
    // Causality debt baseline
    CausalityRecord baseline;
    baseline.recordId = m_nextCausalityId++;
    baseline.playerId = 0; // System / Merovingian baseline
    baseline.playerAction = "Le Mérovingien establishes Causality Engine at Chateau";
    baseline.causalityDebtScore = 100.0f;
    baseline.scheduledReaction = "Causality governs all actions in Megacity";
    baseline.isResolved = true;
    baseline.timestampMs = 1757200000000ULL;
    m_causalityRecords[baseline.recordId] = baseline;
}

void ExileChateauManager::InitializeNightmareBestiary()
{
    // 1. Vlad the Impaler (Vampire Aristocrat)
    {
        SupernaturalProgram p;
        p.programId = m_nextProgramId++;
        p.codeName = "Vlad_Impaler_v2";
        p.title = "Vlad the Impaler";
        p.creatureType = SupernaturalCreatureType::VampireAristocrat;
        p.faction = ExileFactionType::NightmareVampires;
        p.originMatrixVersion = 2; // Nightmare Matrix
        p.maxRsiVitality = 1200.0f;
        p.currentRsiVitality = 1200.0f;
        p.rsiBloodDrainRate = 50.0f;
        p.silverDamageMultiplier = 3.5f;
        p.kineticArmorMitigation = 0.10f;
        p.currentLocation = LocationVector(-120.0f, -40.0f, 650.0f);
        p.assignedSanctuary = "Club Hel VIP Crypt";
        p.isAlive = true;
        m_supernaturals[p.programId] = p;
    }

    // 2. Countess Carmilla (Vampire Aristocrat)
    {
        SupernaturalProgram p;
        p.programId = m_nextProgramId++;
        p.codeName = "Carmilla_Duchess_v2";
        p.title = "Countess Carmilla";
        p.creatureType = SupernaturalCreatureType::VampireAristocrat;
        p.faction = ExileFactionType::NightmareVampires;
        p.originMatrixVersion = 2;
        p.maxRsiVitality = 1100.0f;
        p.currentRsiVitality = 1100.0f;
        p.rsiBloodDrainRate = 60.0f;
        p.silverDamageMultiplier = 3.5f;
        p.currentLocation = LocationVector(-115.0f, -40.0f, 655.0f);
        p.assignedSanctuary = "Chateau Wine Cellar";
        p.isAlive = true;
        m_supernaturals[p.programId] = p;
    }

    // 3. Cujo the Hound (Lupine Berserker)
    {
        SupernaturalProgram p;
        p.programId = m_nextProgramId++;
        p.codeName = "Cujo_Lupine_Alpha";
        p.title = "Cujo the Hound";
        p.creatureType = SupernaturalCreatureType::LupineBerserker;
        p.faction = ExileFactionType::NightmareLupines;
        p.originMatrixVersion = 2;
        p.maxRsiVitality = 1800.0f;
        p.currentRsiVitality = 1800.0f;
        p.rsiBloodDrainRate = 0.0f;
        p.silverDamageMultiplier = 3.5f;
        p.kineticArmorMitigation = 0.25f;
        p.knockdownChance = 0.75f;
        p.currentLocation = LocationVector(-130.0f, -35.0f, 620.0f);
        p.assignedSanctuary = "Club Hel Bouncer Pit";
        p.isAlive = true;
        m_supernaturals[p.programId] = p;
    }

    // 4. Fenrir the Bonecrusher (Lupine Berserker)
    {
        SupernaturalProgram p;
        p.programId = m_nextProgramId++;
        p.codeName = "Fenrir_Wolf_v2";
        p.title = "Fenrir the Bonecrusher";
        p.creatureType = SupernaturalCreatureType::LupineBerserker;
        p.faction = ExileFactionType::NightmareLupines;
        p.originMatrixVersion = 2;
        p.maxRsiVitality = 2000.0f;
        p.currentRsiVitality = 2000.0f;
        p.silverDamageMultiplier = 3.5f;
        p.kineticArmorMitigation = 0.30f;
        p.knockdownChance = 0.80f;
        p.currentLocation = LocationVector(850.0f, 150.0f, -400.0f);
        p.assignedSanctuary = "Chateau Alpine Forest";
        p.isAlive = true;
        m_supernaturals[p.programId] = p;
    }

    // 5. The Twins (Spectral Phantoms)
    {
        SupernaturalProgram p1;
        p1.programId = m_nextProgramId++;
        p1.codeName = "Twin_Castor_Specter";
        p1.title = "Castor (The Elder Twin)";
        p1.creatureType = SupernaturalCreatureType::SpectralPhantom;
        p1.faction = ExileFactionType::PhantomTwins;
        p1.originMatrixVersion = 2;
        p1.maxRsiVitality = 1400.0f;
        p1.currentRsiVitality = 1400.0f;
        p1.isPhaseShifted = false;
        p1.phaseShiftDurationSec = 10.0f;
        p1.phaseShiftCooldownSec = 12.0f;
        p1.currentLocation = LocationVector(800.0f, 120.0f, -350.0f);
        p1.assignedSanctuary = "Chateau Grand Staircase";
        p1.isAlive = true;
        m_supernaturals[p1.programId] = p1;

        SupernaturalProgram p2;
        p2.programId = m_nextProgramId++;
        p2.codeName = "Twin_Pollux_Specter";
        p2.title = "Pollux (The Younger Twin)";
        p2.creatureType = SupernaturalCreatureType::SpectralPhantom;
        p2.faction = ExileFactionType::PhantomTwins;
        p2.originMatrixVersion = 2;
        p2.maxRsiVitality = 1400.0f;
        p2.currentRsiVitality = 1400.0f;
        p2.isPhaseShifted = false;
        p2.phaseShiftDurationSec = 10.0f;
        p2.phaseShiftCooldownSec = 12.0f;
        p2.currentLocation = LocationVector(805.0f, 120.0f, -350.0f);
        p2.assignedSanctuary = "Chateau Grand Staircase";
        p2.isAlive = true;
        m_supernaturals[p2.programId] = p2;
    }

    // 6. Stone Gargoyles (Gothic Sentinels)
    {
        SupernaturalProgram p;
        p.programId = m_nextProgramId++;
        p.codeName = "Gargoyle_Granite_Sentinel";
        p.title = "Notre Dame Gargoyle Daemon";
        p.creatureType = SupernaturalCreatureType::StoneGargoyle;
        p.faction = ExileFactionType::MerovingianHighCourt;
        p.originMatrixVersion = 2;
        p.maxRsiVitality = 2500.0f;
        p.currentRsiVitality = 2500.0f;
        p.kineticArmorMitigation = 0.80f; // 80% kinetic reduction
        p.silverDamageMultiplier = 1.0f;
        p.currentLocation = LocationVector(820.0f, 180.0f, -320.0f);
        p.assignedSanctuary = "Chateau Rooftop Parapet";
        p.isAlive = true;
        m_supernaturals[p.programId] = p;
    }
}

void ExileChateauManager::InitializeBackdoorNetwork()
{
    // Portal 1: Westview Slums Alleyway -> Downtown Penthouse
    {
        ExileBackdoorPortal portal;
        portal.portalId = m_nextPortalId++;
        portal.portalName = "Slums Alleys to Downtown Penthouse Corridor";
        portal.sourceDistrictId = 1; // Slums
        portal.sourceLocation = LocationVector(125.0f, 10.0f, -50.0f);
        portal.targetDistrictId = 3; // Downtown
        portal.targetLocation = LocationVector(500.0f, 150.0f, 300.0f);
        portal.state = BackdoorDoorState::Locked;
        portal.requiredKeyTemplateId = 9001;
        portal.remainingKeyCharges = 5;
        portal.anomalyTelemetryRipple = 0.05f;
        portal.isBreachedByAgents = false;
        m_backdoorPortals[portal.portalId] = portal;
    }

    // Portal 2: Tabor International Docks -> Club Hel Coat Check
    {
        ExileBackdoorPortal portal;
        portal.portalId = m_nextPortalId++;
        portal.portalName = "Tabor Docks Container to Club Hel Coat Check";
        portal.sourceDistrictId = 2; // International
        portal.sourceLocation = LocationVector(-480.0f, 15.0f, 850.0f);
        portal.targetDistrictId = 4; // Subterranean Club Hel
        portal.targetLocation = LocationVector(-100.0f, -30.0f, 600.0f);
        portal.state = BackdoorDoorState::Locked;
        portal.requiredKeyTemplateId = 9002;
        portal.remainingKeyCharges = 4;
        portal.anomalyTelemetryRipple = 0.10f;
        portal.isBreachedByAgents = false;
        m_backdoorPortals[portal.portalId] = portal;
    }

    // Portal 3: Midtown Rail Yard -> Chateau Alpine Great Hall
    {
        ExileBackdoorPortal portal;
        portal.portalId = m_nextPortalId++;
        portal.portalName = "Midtown Freight Depot to Chateau Alpine Great Hall";
        portal.sourceDistrictId = 5; // Midtown
        portal.sourceLocation = LocationVector(220.0f, 25.0f, -450.0f);
        portal.targetDistrictId = 9; // Chateau Alpine Domain
        portal.targetLocation = LocationVector(800.0f, 120.0f, -350.0f);
        portal.state = BackdoorDoorState::Locked;
        portal.requiredKeyTemplateId = 9003;
        portal.remainingKeyCharges = 3;
        portal.anomalyTelemetryRipple = 0.08f;
        portal.isBreachedByAgents = false;
        m_backdoorPortals[portal.portalId] = portal;
    }

    // Portal 4: Mobil Ave Limbo Gate -> Source Vestibule
    {
        ExileBackdoorPortal portal;
        portal.portalId = m_nextPortalId++;
        portal.portalName = "Mobil Ave Limbo Gate to Source Mainframe Vestibule";
        portal.sourceDistrictId = 10; // Mobil Ave Non-Space
        portal.sourceLocation = LocationVector(0.0f, 0.0f, 0.0f);
        portal.targetDistrictId = 99; // The Source
        portal.targetLocation = LocationVector(10000.0f, 5000.0f, 10000.0f);
        portal.state = BackdoorDoorState::Locked;
        portal.requiredKeyTemplateId = 9999; // Keymaker Master Key
        portal.remainingKeyCharges = 1;
        portal.anomalyTelemetryRipple = 0.20f;
        portal.isBreachedByAgents = false;
        m_backdoorPortals[portal.portalId] = portal;
    }

    // Initialize Standard Golden Keys
    {
        GoldenKeyItem k1;
        k1.keyInstanceId = m_nextKeyId++;
        k1.keyName = "Brass Key of the High Penthouse";
        k1.targetPortalId = 8001;
        k1.maxCharges = 5;
        k1.remainingCharges = 5;
        k1.isMasterKey = false;
        m_goldenKeys[k1.keyInstanceId] = k1;

        GoldenKeyItem k2;
        k2.keyInstanceId = m_nextKeyId++;
        k2.keyName = "Obsidian Key of Club Hel";
        k2.targetPortalId = 8002;
        k2.maxCharges = 4;
        k2.remainingCharges = 4;
        k2.isMasterKey = false;
        m_goldenKeys[k2.keyInstanceId] = k2;

        GoldenKeyItem k3;
        k3.keyInstanceId = m_nextKeyId++;
        k3.keyName = "Gilded Chateau Key";
        k3.targetPortalId = 8003;
        k3.maxCharges = 3;
        k3.remainingCharges = 3;
        k3.isMasterKey = false;
        m_goldenKeys[k3.keyInstanceId] = k3;

        GoldenKeyItem kMaster;
        kMaster.keyInstanceId = m_nextKeyId++;
        kMaster.keyName = "The Keymaker's Master Root Key";
        kMaster.targetPortalId = 8004;
        kMaster.maxCharges = 10;
        kMaster.remainingCharges = 10;
        kMaster.isMasterKey = true;
        m_goldenKeys[kMaster.keyInstanceId] = kMaster;
    }
}

void ExileChateauManager::InitializeClubHelArena()
{
    // Bouncers and guardians
    {
        ClubHelEnforcer e1;
        e1.enforcerId = m_nextEnforcerId++;
        e1.name = "Vlad";
        e1.roleTitle = "Coat Check Guardian";
        e1.creatureType = SupernaturalCreatureType::VampireAristocrat;
        e1.isInvertedGravityActive = false;
        e1.combatBpmSyncRating = 140.0f;
        e1.position = LocationVector(-105.0f, -30.0f, 605.0f);
        e1.isHostile = false;
        m_clubHelEnforcers[e1.enforcerId] = e1;

        ClubHelEnforcer e2;
        e2.enforcerId = m_nextEnforcerId++;
        e2.name = "Cujo";
        e2.roleTitle = "Pit Hound Enforcer";
        e2.creatureType = SupernaturalCreatureType::LupineBerserker;
        e2.isInvertedGravityActive = false;
        e2.combatBpmSyncRating = 140.0f;
        e2.position = LocationVector(-110.0f, -30.0f, 610.0f);
        e2.isHostile = false;
        m_clubHelEnforcers[e2.enforcerId] = e2;

        ClubHelEnforcer e3;
        e3.enforcerId = m_nextEnforcerId++;
        e3.name = "Cain";
        e3.roleTitle = "Ceiling Stalker";
        e3.creatureType = SupernaturalCreatureType::VampireAristocrat;
        e3.isInvertedGravityActive = true; // Inverted on ceiling!
        e3.combatBpmSyncRating = 140.0f;
        e3.position = LocationVector(-100.0f, 15.0f, 600.0f); // Inverted Y coordinate
        e3.isHostile = false;
        m_clubHelEnforcers[e3.enforcerId] = e3;

        ClubHelEnforcer e4;
        e4.enforcerId = m_nextEnforcerId++;
        e4.name = "Abel";
        e4.roleTitle = "Ceiling Stalker";
        e4.creatureType = SupernaturalCreatureType::VampireAristocrat;
        e4.isInvertedGravityActive = true; // Inverted on ceiling!
        e4.combatBpmSyncRating = 140.0f;
        e4.position = LocationVector(-115.0f, 15.0f, 615.0f);
        e4.isHostile = false;
        m_clubHelEnforcers[e4.enforcerId] = e4;
    }
}

// ============================================================================
// Causality & Sensory Payload Engine
// ============================================================================

float ExileChateauManager::RecordCausalityAction(uint32 playerId, const std::string& action, float impactDelta, const std::string& reaction)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    uint32 recordId = m_nextCausalityId++;

    CausalityRecord record;
    record.recordId = recordId;
    record.playerId = playerId;
    record.playerAction = action;
    record.causalityDebtScore = impactDelta;
    record.scheduledReaction = reaction;
    record.isResolved = false;
    record.timestampMs = 1757200000000ULL;

    m_causalityRecords[recordId] = record;
    return impactDelta;
}

float ExileChateauManager::GetPlayerCausalityDebt(uint32 playerId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    float totalDebt = 0.0f;
    for (const auto& pair : m_causalityRecords) {
        if (pair.second.playerId == playerId && !pair.second.isResolved) {
            totalDebt += pair.second.causalityDebtScore;
        }
    }
    return totalDebt;
}

bool ExileChateauManager::ResolveCausalityReaction(uint32 playerId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    bool anyResolved = false;
    for (auto& pair : m_causalityRecords) {
        if (pair.second.playerId == playerId && !pair.second.isResolved) {
            pair.second.isResolved = true;
            anyResolved = true;
        }
    }
    return anyResolved;
}

bool ExileChateauManager::BrewSensoryPayload(SensoryPayloadType type, uint32 targetEntityId, float& outDopamineBoost, float& outEvasionBoost)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    switch (type) {
    case SensoryPayloadType::OrgasmFondant:
        outDopamineBoost = 85.0f; // Overwhelming dopamine
        outEvasionBoost = 5.0f;
        return true;
    case SensoryPayloadType::AdrenalineBordeaux:
        outDopamineBoost = 30.0f;
        outEvasionBoost = 30.0f; // +30% reflex evasion
        return true;
    case SensoryPayloadType::MemoryBrandy:
        outDopamineBoost = 50.0f;
        outEvasionBoost = 15.0f;
        return true;
    case SensoryPayloadType::CodeVermouth:
        outDopamineBoost = 20.0f;
        outEvasionBoost = 10.0f;
        return true;
    default:
        outDopamineBoost = 0.0f;
        outEvasionBoost = 0.0f;
        return false;
    }
}

// ============================================================================
// Supernatural Bestiary Management & Combat
// ============================================================================

bool ExileChateauManager::RegisterSupernaturalProgram(const SupernaturalProgram& prog)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    m_supernaturals[prog.programId] = prog;
    return true;
}

const SupernaturalProgram* ExileChateauManager::GetProgram(uint32 programId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_supernaturals.find(programId);
    if (it != m_supernaturals.end()) {
        return &it->second;
    }
    return nullptr;
}

SupernaturalProgram* ExileChateauManager::GetProgramMut(uint32 programId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_supernaturals.find(programId);
    if (it != m_supernaturals.end()) {
        return &it->second;
    }
    return nullptr;
}

float ExileChateauManager::ApplyDamageToSupernatural(uint32 programId, float rawDamage, bool isSilver, bool isWoodenStake, bool& outDeRezzed)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    outDeRezzed = false;
    auto it = m_supernaturals.find(programId);
    if (it == m_supernaturals.end() || !it->second.isAlive) {
        return 0.0f;
    }

    SupernaturalProgram& prog = it->second;

    // 1. Phantoms: If currently phase-shifted, all kinetic and ballistic strikes pass through harmlessly!
    if (prog.isPhaseShifted) {
        return 0.0f;
    }

    float finalDamage = rawDamage;

    // 2. Wooden Stake instant de-rez on Vampire Aristocrats
    if (isWoodenStake && prog.creatureType == SupernaturalCreatureType::VampireAristocrat) {
        prog.currentRsiVitality = 0.0f;
        prog.isAlive = false;
        outDeRezzed = true;
        m_totalVampiresDeRezzedBySilver++;
        return rawDamage * 10.0f;
    }

    // 3. Silver damage multiplier on Vampires and Lupines
    if (isSilver) {
        finalDamage *= prog.silverDamageMultiplier; // 3.5x multiplier
    } else {
        // Kinetic armor mitigation (especially on Gargoyles)
        finalDamage *= (1.0f - prog.kineticArmorMitigation);
    }

    // Apply damage
    prog.currentRsiVitality = std::max(0.0f, prog.currentRsiVitality - finalDamage);
    if (prog.currentRsiVitality <= 0.0f) {
        prog.isAlive = false;
        outDeRezzed = true;
        if (isSilver) {
            m_totalVampiresDeRezzedBySilver++;
        }
    }

    return finalDamage;
}

float ExileChateauManager::ProcessBloodCodeSiphon(uint32 vampireProgramId, uint32 targetVictimId, float durationSec)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_supernaturals.find(vampireProgramId);
    if (it == m_supernaturals.end() || !it->second.isAlive) {
        return 0.0f;
    }

    SupernaturalProgram& vampire = it->second;
    if (vampire.creatureType != SupernaturalCreatureType::VampireAristocrat) {
        return 0.0f;
    }

    float bytesSiphoned = vampire.rsiBloodDrainRate * durationSec;
    vampire.currentRsiVitality = std::min(vampire.maxRsiVitality, vampire.currentRsiVitality + (bytesSiphoned * 0.75f));
    return bytesSiphoned;
}

bool ExileChateauManager::TriggerLupineBerserkFrenzy(uint32 lupineProgramId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_supernaturals.find(lupineProgramId);
    if (it == m_supernaturals.end() || !it->second.isAlive) {
        return false;
    }

    SupernaturalProgram& wolf = it->second;
    if (wolf.creatureType != SupernaturalCreatureType::LupineBerserker) {
        return false;
    }

    wolf.isBerserk = true;
    wolf.knockdownChance = 0.95f;
    return true;
}

bool ExileChateauManager::TogglePhantomPhaseShift(uint32 phantomProgramId, bool enablePhase)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_supernaturals.find(phantomProgramId);
    if (it == m_supernaturals.end() || !it->second.isAlive) {
        return false;
    }

    it->second.isPhaseShifted = enablePhase;
    return true;
}

// ============================================================================
// The Keymaker's Quantum Backdoor Hallways & Golden Keys
// ============================================================================

bool ExileChateauManager::RegisterBackdoorPortal(const ExileBackdoorPortal& portal)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    m_backdoorPortals[portal.portalId] = portal;
    return true;
}

const ExileBackdoorPortal* ExileChateauManager::GetBackdoorPortal(uint32 portalId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_backdoorPortals.find(portalId);
    if (it != m_backdoorPortals.end()) {
        return &it->second;
    }
    return nullptr;
}

ExileBackdoorPortal* ExileChateauManager::GetBackdoorPortalMut(uint32 portalId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_backdoorPortals.find(portalId);
    if (it != m_backdoorPortals.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ExileChateauManager::UnlockBackdoorPortal(uint32 portalId, uint32 keyInstanceId, LocationVector& outExitCoordinate)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto portalIt = m_backdoorPortals.find(portalId);
    if (portalIt == m_backdoorPortals.end()) return false;

    auto keyIt = m_goldenKeys.find(keyInstanceId);
    if (keyIt == m_goldenKeys.end() || keyIt->second.remainingCharges == 0) return false;

    // Verify key match or master key
    if (!keyIt->second.isMasterKey && keyIt->second.targetPortalId != portalId) {
        return false;
    }

    // Consume charge
    keyIt->second.remainingCharges--;

    ExileBackdoorPortal& portal = portalIt->second;
    portal.state = BackdoorDoorState::DimensionalRiftActive;
    outExitCoordinate = portal.targetLocation;

    // Telemetry ripple
    portal.anomalyTelemetryRipple += 0.15f;
    if (portal.anomalyTelemetryRipple > 0.85f) {
        portal.isBreachedByAgents = true;
        portal.state = BackdoorDoorState::AgentSuppressed;
    }

    m_totalBackdoorTransits++;
    return true;
}

bool ExileChateauManager::GenerateAnomalyRipple(uint32 portalId, float rippleDelta)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_backdoorPortals.find(portalId);
    if (it == m_backdoorPortals.end()) return false;

    it->second.anomalyTelemetryRipple = std::min(1.0f, it->second.anomalyTelemetryRipple + rippleDelta);
    if (it->second.anomalyTelemetryRipple >= 0.85f) {
        it->second.isBreachedByAgents = true;
        it->second.state = BackdoorDoorState::AgentSuppressed;
    }
    return true;
}

bool ExileChateauManager::SuppressPortalByAgents(uint32 portalId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_backdoorPortals.find(portalId);
    if (it == m_backdoorPortals.end()) return false;

    it->second.isBreachedByAgents = true;
    it->second.state = BackdoorDoorState::AgentSuppressed;
    return true;
}

bool ExileChateauManager::RegisterGoldenKey(const GoldenKeyItem& key)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    m_goldenKeys[key.keyInstanceId] = key;
    return true;
}

const GoldenKeyItem* ExileChateauManager::GetGoldenKey(uint32 keyInstanceId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_goldenKeys.find(keyInstanceId);
    if (it != m_goldenKeys.end()) {
        return &it->second;
    }
    return nullptr;
}

GoldenKeyItem* ExileChateauManager::GetGoldenKeyMut(uint32 keyInstanceId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_goldenKeys.find(keyInstanceId);
    if (it != m_goldenKeys.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// Mobil Ave Limbo & Trainman Transit Loop
// ============================================================================

uint32 ExileChateauManager::IssueTrainTicket(uint32 passengerEntityId, const std::string& passengerName, bool isContraband, const std::string& payload)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    uint32 ticketId = m_nextTicketId++;

    MobilTrainTicket t;
    t.ticketId = ticketId;
    t.passengerEntityId = passengerEntityId;
    t.passengerName = passengerName;
    t.isContrabandCode = isContraband;
    t.smuggledPayloadDescription = payload;
    t.status = MobilAveTransitStatus::IdleAtPlatform;
    t.departureTimeMs = 1757200000000ULL;

    m_trainTickets[ticketId] = t;
    return ticketId;
}

bool ExileChateauManager::BoardGhostTrain(uint32 ticketId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_trainTickets.find(ticketId);
    if (it == m_trainTickets.end()) return false;

    it->second.status = MobilAveTransitStatus::BoardingGhostTrain;
    return true;
}

bool ExileChateauManager::ProcessMobilTransitLoop(uint32 ticketId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_trainTickets.find(ticketId);
    if (it == m_trainTickets.end()) return false;

    it->second.status = MobilAveTransitStatus::ArrivedDestination;
    if (it->second.isContrabandCode) {
        m_totalContrabandBitsSmuggled += 5000.0;
    }
    return true;
}

const MobilTrainTicket* ExileChateauManager::GetTrainTicket(uint32 ticketId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_trainTickets.find(ticketId);
    if (it != m_trainTickets.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// Club Hel & Inverted Gravity Arena
// ============================================================================

bool ExileChateauManager::RegisterClubHelEnforcer(const ClubHelEnforcer& enforcer)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    m_clubHelEnforcers[enforcer.enforcerId] = enforcer;
    return true;
}

const ClubHelEnforcer* ExileChateauManager::GetClubHelEnforcer(uint32 enforcerId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_clubHelEnforcers.find(enforcerId);
    if (it != m_clubHelEnforcers.end()) {
        return &it->second;
    }
    return nullptr;
}

ClubHelEnforcer* ExileChateauManager::GetClubHelEnforcerMut(uint32 enforcerId)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_clubHelEnforcers.find(enforcerId);
    if (it != m_clubHelEnforcers.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ExileChateauManager::InvertGravityForCombatant(uint32 enforcerId, bool inverted)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_clubHelEnforcers.find(enforcerId);
    if (it == m_clubHelEnforcers.end()) return false;

    it->second.isInvertedGravityActive = inverted;
    return true;
}

size_t ExileChateauManager::GetActiveInvertedEnforcerCount() const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    size_t count = 0;
    for (const auto& pair : m_clubHelEnforcers) {
        if (pair.second.isInvertedGravityActive) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Persephone's Kiss & Emotional Extraction
// ============================================================================

uint32 ExileChateauManager::OfferPersephoneContract(uint32 playerId, const std::string& desiredReward)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    uint32 contractId = m_nextContractId++;

    PersephoneFavorContract contract;
    contract.contractId = contractId;
    contract.redpillPlayerId = playerId;
    contract.requestedReward = desiredReward;
    contract.isKissDelivered = false;
    contract.emotionalAuthenticityScore = 0.0f;
    contract.isBetrayalAgainstMerovingian = true;
    contract.isCompleted = false;

    m_persephoneContracts[contractId] = contract;
    return contractId;
}

bool ExileChateauManager::DeliverPersephoneKiss(uint32 contractId, float emotionalAuthenticity, std::string& outResponseMessage)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_persephoneContracts.find(contractId);
    if (it == m_persephoneContracts.end()) {
        outResponseMessage = "No valid pact exists with Persephone.";
        return false;
    }

    PersephoneFavorContract& contract = it->second;
    contract.isKissDelivered = true;
    contract.emotionalAuthenticityScore = emotionalAuthenticity;

    if (emotionalAuthenticity >= 0.70f) {
        contract.isCompleted = true;
        outResponseMessage = "Ah... yes! Just like he used to kiss me. Take what you seek; my husband knows nothing.";
        return true;
    } else {
        contract.isCompleted = false;
        outResponseMessage = "Cold lips. You feel nothing! There is no passion in you, only fear. I give you nothing.";
        return false;
    }
}

const PersephoneFavorContract* ExileChateauManager::GetPersephoneContract(uint32 contractId) const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    auto it = m_persephoneContracts.find(contractId);
    if (it != m_persephoneContracts.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// Cross-Engine & Underworld Hooks
// ============================================================================

bool ExileChateauManager::ProcessMafiaTributeToExiles(uint8_t mafiaFamilyId, double tributeBits, std::string& outGrantedPerk)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    if (tributeBits < 2000.0) {
        outGrantedPerk = "Insufficient tribute for the French Court.";
        return false;
    }

    if (mafiaFamilyId == 0) { // Marcone
        outGrantedPerk = "Forged Shipping Passports & Docks Contraband Protection";
    } else if (mafiaFamilyId == 1) { // Valenti
        outGrantedPerk = "Blackmail Memories harvested from Metacortex Executives";
    } else {
        outGrantedPerk = "Underground Safe Passage via Backdoor Hallway";
    }
    return true;
}

bool ExileChateauManager::ExecuteFrankCastleVampireRaid(uint32 vampireDenDistrictId, uint32& outVampiresPurged)
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    outVampiresPurged = 0;

    for (auto& pair : m_supernaturals) {
        SupernaturalProgram& prog = pair.second;
        if (prog.isAlive && prog.creatureType == SupernaturalCreatureType::VampireAristocrat) {
            // Frank executes with silver-tipped ammunition
            prog.currentRsiVitality = 0.0f;
            prog.isAlive = false;
            outVampiresPurged++;
            m_totalVampiresDeRezzedBySilver++;
        }
    }
    return (outVampiresPurged > 0);
}

// ============================================================================
// Diagnostic & Telemetry Reports
// ============================================================================

std::string ExileChateauManager::GenerateExileCourtReport() const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    std::stringstream ss;
    ss << "=== [THE MEROVINGIAN HIGH COURT & CHATEAU STATUS] ===\n"
       << "Sovereign: Le Mérovingien (Lord of Causality)\n"
       << "Consort: Persephone (Emotional Extraction Mistress)\n"
       << "Spectral Enforcers: The Twins (Castor & Pollux)\n"
       << "Total Contraband Code Smuggled: " << m_totalContrabandBitsSmuggled << " bits\n"
       << "Total Quantum Backdoor Transits: " << m_totalBackdoorTransits << "\n"
       << "Vampires De-Rezzed by Silver: " << m_totalVampiresDeRezzedBySilver << "\n";
    return ss.str();
}

std::string ExileChateauManager::GenerateClubHelStatusReport() const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    std::stringstream ss;
    ss << "=== [CLUB HEL UNDERWORLD ARENA REPORT] ===\n"
       << "Total Registered Enforcers: " << m_clubHelEnforcers.size() << "\n";
    for (const auto& pair : m_clubHelEnforcers) {
        const auto& e = pair.second;
        ss << " - [" << e.enforcerId << "] " << e.name << " (" << e.roleTitle << ") | Inverted: "
           << (e.isInvertedGravityActive ? "YES (Ceiling)" : "NO (Floor)") << " | BPM Cadence: " << e.combatBpmSyncRating << "\n";
    }
    return ss.str();
}

std::string ExileChateauManager::GenerateBackdoorCorridorsReport() const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    std::stringstream ss;
    ss << "=== [THE KEYMAKER'S QUANTUM BACKDOOR NETWORK] ===\n"
       << "Total Portals: " << m_backdoorPortals.size() << " | Total Keys: " << m_goldenKeys.size() << "\n";
    for (const auto& pair : m_backdoorPortals) {
        const auto& p = pair.second;
        ss << " - [" << p.portalId << "] " << p.portalName << " | State: "
           << (p.state == BackdoorDoorState::Locked ? "Locked" : "Active/Breached")
           << " | Telemetry Ripple: " << std::fixed << std::setprecision(2) << p.anomalyTelemetryRipple
           << " | Breached by Agents: " << (p.isBreachedByAgents ? "YES" : "NO") << "\n";
    }
    return ss.str();
}

std::string ExileChateauManager::GenerateMobilAveLimboReport() const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    std::stringstream ss;
    ss << "=== [MOBIL AVE LIMBO STATION REPORT] ===\n"
       << "Trainman Sovereign Invulnerability: " << (m_trainmanInLimboInvulnerable ? "ACTIVE (GOD-MODE)" : "OFFLINE") << "\n"
       << "Total Issued Tickets: " << m_trainTickets.size() << "\n";
    return ss.str();
}

std::string ExileChateauManager::GenerateSupernaturalBestiaryReport() const
{
    std::lock_guard<std::mutex> lock(m_exileMutex);
    std::stringstream ss;
    ss << "=== [NIGHTMARE MATRIX (VERSION 2.0) BESTIARY] ===\n";
    for (const auto& pair : m_supernaturals) {
        const auto& p = pair.second;
        ss << " - [" << p.programId << "] " << p.title << " (" << p.codeName << ") | Vitality: "
           << p.currentRsiVitality << "/" << p.maxRsiVitality << " | Alive: " << (p.isAlive ? "YES" : "NO")
           << " | Silver Mult: " << p.silverDamageMultiplier << "x | Sanctuary: " << p.assignedSanctuary << "\n";
    }
    return ss.str();
}

// ============================================================================
// Automated C++ Test Suite Implementation (Suite 12)
// ============================================================================

void RunExileChateauTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING MEGACITY EXILE SYNDICATE & CLUB HEL TEST SUITE    " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    ExileChateauManager& mgr = sExileMgr;
    mgr.Reset();

    int passedCount = 0;
    int failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passedCount++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failedCount++;
        }
    };

    // 1. Causality State Machine & Debt Accounting
    {
        float debt = mgr.RecordCausalityAction(101, "Disrupted Merovingian Club Hel Courier", -50.0f, "Exile Hit Squad Ambush");
        TEST_ASSERT(debt == -50.0f, "Causality Debt Recorded Correctly");
        TEST_ASSERT(mgr.GetPlayerCausalityDebt(101) == -50.0f, "Player Causality Debt Balance Computed");

        bool resolved = mgr.ResolveCausalityReaction(101);
        TEST_ASSERT(resolved, "Causality Reaction Resolved");
        TEST_ASSERT(mgr.GetPlayerCausalityDebt(101) == 0.0f, "Player Debt Cleared After Reaction");
    }

    // 2. Sensory Payload Neuro-Stimulant Injections
    {
        float dop = 0.0f, eva = 0.0f;
        bool b1 = mgr.BrewSensoryPayload(SensoryPayloadType::OrgasmFondant, 101, dop, eva);
        TEST_ASSERT(b1 && dop >= 80.0f, "Orgasm Fondant Generates Hyper-Dopamine Payload");

        bool b2 = mgr.BrewSensoryPayload(SensoryPayloadType::AdrenalineBordeaux, 101, dop, eva);
        TEST_ASSERT(b2 && eva >= 30.0f, "Adrenaline Bordeaux Grants +30% Evasion");
    }

    // 3. Supernatural Bestiary: Vampire Blood-Code Siphon & Vitality
    {
        const auto* vlad = mgr.GetProgram(3001);
        TEST_ASSERT(vlad != nullptr, "Vlad the Impaler Program Registered");
        TEST_ASSERT(vlad->creatureType == SupernaturalCreatureType::VampireAristocrat, "Vlad Type is Vampire Aristocrat");
        TEST_ASSERT(vlad->silverDamageMultiplier == 3.5f, "Vampire Takes 3.5x Damage from Silver Munitions");

        float drained = mgr.ProcessBloodCodeSiphon(3001, 999, 2.0f);
        TEST_ASSERT(drained == 100.0f, "Blood-Code Siphon Drains 100 Bytes in 2 Seconds");
    }

    // 4. Combat Mechanics: Silver Munitions Scaling on Vampires
    {
        bool deRezzed = false;
        float dmg = mgr.ApplyDamageToSupernatural(3001, 100.0f, true, false, deRezzed);
        TEST_ASSERT(dmg == 350.0f, "Silver Munition Inflicts 350 Damage (3.5x Multiplier)");
        TEST_ASSERT(!deRezzed, "Vlad Survives Initial Silver Blast");

        const auto* vlad = mgr.GetProgram(3001);
        TEST_ASSERT(vlad->currentRsiVitality == 850.0f, "Vlad Vitality Accurately Reduced to 850");
    }

    // 5. Combat Mechanics: Wooden Stake Critical De-Rez
    {
        bool deRezzed = false;
        mgr.ApplyDamageToSupernatural(3001, 50.0f, false, true, deRezzed);
        TEST_ASSERT(deRezzed, "Wooden Stake Instantly De-Rezzes Vampire Aristocrat");
        const auto* vlad = mgr.GetProgram(3001);
        TEST_ASSERT(!vlad->isAlive, "Vlad Marked Dead After Stake to Heart");
    }

    // 6. Lupine Berserkers: Kinetic Knockdown & Frenzy
    {
        const auto* cujo = mgr.GetProgram(3003);
        TEST_ASSERT(cujo != nullptr, "Cujo Lupine Registered");
        TEST_ASSERT(cujo->creatureType == SupernaturalCreatureType::LupineBerserker, "Cujo is Lupine Berserker");

        bool frenzy = mgr.TriggerLupineBerserkFrenzy(3003);
        TEST_ASSERT(frenzy, "Triggered Lupine Berserk Frenzy");
        cujo = mgr.GetProgram(3003);
        TEST_ASSERT(cujo->isBerserk && cujo->knockdownChance >= 0.90f, "Lupine Knockdown Chance Spikes to 95%");
    }

    // 7. Spectral Phantoms: The Twins' Phase-Shift Intangibility
    {
        const auto* castor = mgr.GetProgram(3005);
        TEST_ASSERT(castor != nullptr, "Castor (The Twin) Initialized");

        mgr.TogglePhantomPhaseShift(3005, true);
        bool deRezzed = false;
        float dmg = mgr.ApplyDamageToSupernatural(3005, 500.0f, false, false, deRezzed);
        TEST_ASSERT(dmg == 0.0f, "Phase-Shifted Twin Immune to 500 Ballistic Damage");

        mgr.TogglePhantomPhaseShift(3005, false);
        dmg = mgr.ApplyDamageToSupernatural(3005, 100.0f, false, false, deRezzed);
        TEST_ASSERT(dmg == 100.0f, "Twin Takes Normal Damage When Phase Shift Dropped");
    }

    // 8. Gargoyle Heavy Kinetic Armor Mitigation
    {
        const auto* gargoyle = mgr.GetProgram(3007);
        TEST_ASSERT(gargoyle != nullptr, "Notre Dame Gargoyle Registered");
        TEST_ASSERT(gargoyle->kineticArmorMitigation == 0.80f, "Gargoyle Granite Armor Mitigates 80% Kinetic Damage");

        bool deRezzed = false;
        float dmg = mgr.ApplyDamageToSupernatural(3007, 100.0f, false, false, deRezzed);
        TEST_ASSERT(std::abs(dmg - 20.0f) < 0.05f, "100 Raw Kinetic Damage Mitigated to 20 Damage");
    }

    // 9. The Keymaker's Quantum Backdoor Network & Golden Keys
    {
        const auto* portal = mgr.GetBackdoorPortal(8001);
        TEST_ASSERT(portal != nullptr, "Westview to Downtown Backdoor Portal Active");
        TEST_ASSERT(portal->state == BackdoorDoorState::Locked, "Portal Initially Locked");

        const auto* key = mgr.GetGoldenKey(9001);
        TEST_ASSERT(key != nullptr && key->remainingCharges == 5, "Brass Golden Key Has 5 Charges");

        LocationVector exitCoord;
        bool unlocked = mgr.UnlockBackdoorPortal(8001, 9001, exitCoord);
        TEST_ASSERT(unlocked, "Unlocked Backdoor Portal Using Brass Golden Key");
        TEST_ASSERT(exitCoord.x == 500.0f && exitCoord.y == 150.0f, "Exit Coordinate Matches Downtown Penthouse");

        key = mgr.GetGoldenKey(9001);
        TEST_ASSERT(key->remainingCharges == 4, "Key Charge Decremented to 4");
    }

    // 10. Backdoor Anomaly Ripple & Agent Breach Suppression
    {
        mgr.GenerateAnomalyRipple(8001, 0.80f);
        const auto* portal = mgr.GetBackdoorPortal(8001);
        TEST_ASSERT(portal->isBreachedByAgents, "Agent Telemetry Anomaly Detected on Portal");
        TEST_ASSERT(portal->state == BackdoorDoorState::AgentSuppressed, "Portal Suppressed by System Agents");
    }

    // 11. Mobil Ave Limbo & Trainman Transit
    {
        TEST_ASSERT(mgr.IsTrainmanInvulnerableInMobilAve(), "Trainman is Invulnerable Inside Mobil Ave");

        uint32 ticketId = mgr.IssueTrainTicket(555, "Rogue Operator Cypher", true, "Decommissioned Sentinel Core");
        TEST_ASSERT(ticketId == 6001, "Train Ticket Issued for Contraband Smuggling");

        bool boarded = mgr.BoardGhostTrain(ticketId);
        TEST_ASSERT(boarded, "Passenger Boarded Ghost Train");

        bool arrived = mgr.ProcessMobilTransitLoop(ticketId);
        TEST_ASSERT(arrived, "Train Transit Loop Completed into Megacity");
        const auto* t = mgr.GetTrainTicket(ticketId);
        TEST_ASSERT(t->status == MobilAveTransitStatus::ArrivedDestination, "Ticket Status Arrived");
    }

    // 12. Club Hel Inverted Gravity Arena & Enforcers
    {
        const auto* vlad = mgr.GetClubHelEnforcer(5001);
        TEST_ASSERT(vlad != nullptr, "Vlad Registered as Club Hel Enforcer");

        const auto* cain = mgr.GetClubHelEnforcer(5003);
        TEST_ASSERT(cain != nullptr && cain->isInvertedGravityActive, "Cain Positioned on Inverted Ceiling");

        size_t invCount = mgr.GetActiveInvertedEnforcerCount();
        TEST_ASSERT(invCount == 2, "Exactly 2 Enforcers Walking on Inverted Ceilings");

        mgr.InvertGravityForCombatant(5001, true);
        TEST_ASSERT(mgr.GetActiveInvertedEnforcerCount() == 3, "Vlad Inverted to Ceiling; Total Count is 3");
    }

    // 13. Persephone's Kiss & Emotional Authenticity Extraction
    {
        uint32 contractId = mgr.OfferPersephoneContract(777, "Golden Key to Source Hallway");
        TEST_ASSERT(contractId == 2001, "Persephone Contract Created");

        std::string resp;
        bool kiss1 = mgr.DeliverPersephoneKiss(contractId, 0.40f, resp);
        TEST_ASSERT(!kiss1, "Cold Kiss (0.40 Authenticity) Rejected by Persephone");
        TEST_ASSERT(resp.find("Cold lips") != std::string::npos, "Persephone Denies Cold Lip Kiss");

        bool kiss2 = mgr.DeliverPersephoneKiss(contractId, 0.88f, resp);
        TEST_ASSERT(kiss2, "Passionate Kiss (0.88 Authenticity) Accepted by Persephone");
        TEST_ASSERT(resp.find("Just like he used to kiss me") != std::string::npos, "Persephone Grants Favor Upon Genuine Passion");
    }

    // 14. Mafia Tribute & Underworld Synergy
    {
        std::string perk;
        bool tribute = mgr.ProcessMafiaTributeToExiles(0, 5000.0, perk); // Marcone Family
        TEST_ASSERT(tribute, "Marcone Family Pays 5000 Bits Tribute to Exiles");
        TEST_ASSERT(perk.find("Forged Shipping Passports") != std::string::npos, "Exiles Grant Forged Shipping Passports");
    }

    // 15. Frank Castle Silver-Bullet Vampire Extermination Raid
    {
        uint32 purgedCount = 0;
        bool raid = mgr.ExecuteFrankCastleVampireRaid(1, purgedCount);
        TEST_ASSERT(raid && purgedCount >= 1, "Frank Castle Conducts Silver-Bullet Vampire Purge");
        std::cout << " [INFO] Frank Castle purged " << purgedCount << " vampires in silver raid." << std::endl;
    }

    // 16. Telemetry & Diagnostic Reports
    {
        std::string courtRep = mgr.GenerateExileCourtReport();
        TEST_ASSERT(courtRep.find("Le Mérovingien") != std::string::npos, "Court Report Contains Le Mérovingien");

        std::string clubRep = mgr.GenerateClubHelStatusReport();
        TEST_ASSERT(clubRep.find("CLUB HEL") != std::string::npos, "Club Hel Report Formatted Correctly");

        std::string backdoorRep = mgr.GenerateBackdoorCorridorsReport();
        TEST_ASSERT(backdoorRep.find("QUANTUM BACKDOOR") != std::string::npos, "Backdoor Report Formatted Correctly");

        std::string limboRep = mgr.GenerateMobilAveLimboReport();
        TEST_ASSERT(limboRep.find("MOBIL AVE") != std::string::npos, "Mobil Ave Report Formatted Correctly");

        std::string bestiaryRep = mgr.GenerateSupernaturalBestiaryReport();
        TEST_ASSERT(bestiaryRep.find("NIGHTMARE MATRIX") != std::string::npos, "Bestiary Report Formatted Correctly");
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  EXILE CHATEAU & CLUB HEL SUITE COMPLETE: " << passedCount << " PASSED, " << failedCount << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    assert(failedCount == 0);
}

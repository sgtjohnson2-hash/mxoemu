#include "BackdoorNetwork.h"
#include "Log.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "BotManager.h"
#include "BotClient.h"
#include "FactionWarManager.h"
#include "Timer.h"
#include "SpatialGrid.h"
#include "MessageTypes.h"
#include <algorithm>
#include <iostream>
#include <cassert>

createFileSingleton(BackdoorNetwork);

BackdoorNetwork::BackdoorNetwork()
{
    Initialize();
}

BackdoorNetwork::~BackdoorNetwork()
{
}

void BackdoorNetwork::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    m_portals.clear();
    m_craftedKeys.clear();
    m_firewallAnchors.clear();
    m_nextKeyId = 1001;
    m_nextPuzzleId = 1;
    m_totalTransits = 0;
    InitializeProceduralHallways(10);

    // Register Default Backdoor Corridor Doors
    // Door 101: The Slums Industrial Warehouse
    RegisterBackdoor(101, 101, "Door 101 - Slums Warehouse", "The Slums",
                     {100.0f, 0.0f, 1000.0f, 90.0f},
                     {99640.0f, 500.0f, 8350.0f, 180.0f},
                     KEY_DISTRICT_MASTER);

    // Door 204: Downtown Metacortex Rooftop
    RegisterBackdoor(204, 204, "Door 204 - Metacortex Tower Helipad", "Downtown",
                     {100.0f, 0.0f, 2000.0f, 90.0f},
                     {10500.0f, 12000.0f, -4200.0f, 270.0f},
                     KEY_ROOFTOP_BYPASS);

    // Door 315: International District Club Hel Service Tunnel
    RegisterBackdoor(315, 315, "Door 315 - Club Hel Service Tunnel", "International",
                     {-100.0f, 0.0f, 1500.0f, 270.0f},
                     {-32000.0f, 350.0f, 15000.0f, 0.0f},
                     KEY_DISTRICT_MASTER);

    // Door 408: Richland High-Rise Secure Server Vault
    RegisterBackdoor(408, 408, "Door 408 - Richland Federal Server Vault", "Richland",
                     {-100.0f, 0.0f, 3000.0f, 270.0f},
                     {45000.0f, 18500.0f, -22000.0f, 90.0f},
                     KEY_VAULT_BREAKER);

    // Door 777: The Keymaker's Workshop
    RegisterBackdoor(777, 777, "Door 777 - Keymaker Cryptographic Workshop", "Keymaker Workshop",
                     {0.0f, 0.0f, 5000.0f, 0.0f},
                     {0.0f, 1000.0f, 0.0f, 180.0f},
                     KEY_DISTRICT_MASTER);

    // Door 999: The Source / The Architect's Chamber
    RegisterBackdoor(999, 999, "Door 999 - The Source Gate", "The Source",
                     {0.0f, 0.0f, 10000.0f, 0.0f},
                     {0.0f, 50000.0f, 0.0f, 0.0f},
                     KEY_SOURCE_KEY, true);

    if (Log::getSingletonPtr())
    {
        sLog.outString("[BackdoorNetwork] Non-Euclidean Hallway of Doors initialized with %zu registered portals.", m_portals.size());
    }
}

void BackdoorNetwork::Reset()
{
    Initialize();
}

void BackdoorNetwork::RegisterBackdoor(uint32 doorId, uint32 doorNumber, const std::string& name,
                                       const std::string& district, const PortalPosition& entrance,
                                       const PortalPosition& destination, BackdoorKeyType reqKey, bool isSource)
{
    BackdoorPortal p;
    p.doorId = doorId;
    p.doorNumber = doorNumber;
    p.name = name;
    p.destinationDistrict = district;
    p.hallwayEntrancePos = entrance;
    p.megacityExitPos = destination;
    p.requiredKey = reqKey;
    p.lockStatus = (reqKey == KEY_NONE) ? DOOR_UNLOCKED : DOOR_LOCKED;
    p.transitCount = 0;
    p.isSourceDoor = isSource;

    m_portals.push_back(p);
}

const BackdoorPortal* BackdoorNetwork::GetPortal(uint32 doorId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    for (const auto& p : m_portals)
    {
        if (p.doorId == doorId) return &p;
    }
    return nullptr;
}

size_t BackdoorNetwork::GetPortalCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    return m_portals.size();
}

bool BackdoorNetwork::TraverseBackdoor(uint32 doorId, float inX, float inY, float inZ, float inHeading,
                                       float& outX, float& outY, float& outZ, float& outHeading,
                                       const KeymakerMasterKey* keyUsed)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    for (auto& p : m_portals)
    {
        if (p.doorId == doorId)
        {
            // Verify lock status
            if (p.lockStatus == DOOR_LOCKED)
            {
                if (!keyUsed || !VerifyKeyForDoor(*keyUsed, doorId))
                {
                    sLog.outString("[BackdoorNetwork] Door %u is locked. Access denied without master key.", doorId);
                    return false;
                }
            }

            // Non-Euclidean Portal Transformation Math
            // Compute entrance relative offset
            float relX = inX - p.hallwayEntrancePos.x;
            float relY = inY - p.hallwayEntrancePos.y;
            float relZ = inZ - p.hallwayEntrancePos.z;

            // Rotation angle delta between hallway portal and Megacity destination
            float deltaHeadingDeg = p.megacityExitPos.headingDeg - p.hallwayEntrancePos.headingDeg;
            float deltaHeadingRad = deltaHeadingDeg * (3.14159265f / 180.0f);

            // Rotate relative offset vector into target coordinate frame
            float rotX = relX * std::cos(deltaHeadingRad) - relZ * std::sin(deltaHeadingRad);
            float rotZ = relX * std::sin(deltaHeadingRad) + relZ * std::cos(deltaHeadingRad);

            // Apply transformed position at destination portal
            outX = p.megacityExitPos.x + rotX;
            outY = p.megacityExitPos.y + relY;
            outZ = p.megacityExitPos.z + rotZ;

            // Update heading
            outHeading = inHeading + deltaHeadingDeg;
            while (outHeading < 0.0f) outHeading += 360.0f;
            while (outHeading >= 360.0f) outHeading -= 360.0f;

            p.transitCount++;
            m_totalTransits++;

            sLog.outString("[BackdoorNetwork] Non-Euclidean transit through Door %u (%s) -> Dest: (%.1f, %.1f, %.1f) in %s",
                           doorId, p.name.c_str(), outX, outY, outZ, p.destinationDistrict.c_str());
            return true;
        }
    }
    return false;
}

CryptographicCipherPuzzle BackdoorNetwork::GenerateCipherPuzzle(BackdoorKeyType keyType)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    CryptographicCipherPuzzle puzzle;
    puzzle.puzzleId = m_nextPuzzleId++;
    puzzle.keyType = keyType;

    switch (keyType)
    {
        case KEY_DISTRICT_MASTER:
            puzzle.seedMask = 0x5A5A3C3C;
            puzzle.tumblerPinParity = 0x0F0F;
            break;
        case KEY_ROOFTOP_BYPASS:
            puzzle.seedMask = 0xA5A5C3C3;
            puzzle.tumblerPinParity = 0xF0F0;
            break;
        case KEY_VAULT_BREAKER:
            puzzle.seedMask = 0x12345678;
            puzzle.tumblerPinParity = 0x8765;
            break;
        case KEY_SOURCE_KEY:
            puzzle.seedMask = 0xFF00AA55;
            puzzle.tumblerPinParity = 0x55AA;
            break;
        default:
            puzzle.seedMask = 0x11111111;
            puzzle.tumblerPinParity = 0x2222;
            break;
    }

    puzzle.expectedSolution = (puzzle.seedMask ^ 0xDEADBEEF) + puzzle.tumblerPinParity;
    return puzzle;
}

bool BackdoorNetwork::CraftMasterKey(uint32 characterId, BackdoorKeyType keyType, uint32 cipherSolution, KeymakerMasterKey& outKey)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);

    CryptographicCipherPuzzle expected = GenerateCipherPuzzle(keyType);
    if (cipherSolution != expected.expectedSolution)
    {
        sLog.outString("[BackdoorNetwork] Keymaker cipher failed for Character %u. Invalid tumbler code: 0x%08X (expected 0x%08X)",
                       characterId, cipherSolution, expected.expectedSolution);
        return false;
    }

    outKey.keyId = m_nextKeyId++;
    outKey.ownerCharacterId = characterId;
    outKey.keyType = keyType;
    outKey.cipherHash = cipherSolution;
    outKey.isForged = true;

    switch (keyType)
    {
        case KEY_DISTRICT_MASTER:
            outKey.keyName = "District Master Key";
            outKey.maxUses = 10;
            outKey.durabilityUses = 10;
            break;
        case KEY_ROOFTOP_BYPASS:
            outKey.keyName = "Rooftop Bypass Key";
            outKey.maxUses = 8;
            outKey.durabilityUses = 8;
            break;
        case KEY_VAULT_BREAKER:
            outKey.keyName = "Server Vault Breaker Key";
            outKey.maxUses = 3;
            outKey.durabilityUses = 3;
            break;
        case KEY_SOURCE_KEY:
            outKey.keyName = "The Source Master Key";
            outKey.maxUses = 999;
            outKey.durabilityUses = 999; // Permanent
            break;
        default:
            outKey.keyName = "Standard Door Key";
            outKey.maxUses = 1;
            outKey.durabilityUses = 1;
            break;
    }

    m_craftedKeys[outKey.keyId] = outKey;
    sLog.outString("[BackdoorNetwork] Keymaker forged '%s' (ID %u) for Character %u (Durability: %u/%u)",
                   outKey.keyName.c_str(), outKey.keyId, characterId, outKey.durabilityUses, outKey.maxUses);
    return true;
}

bool BackdoorNetwork::VerifyKeyForDoor(const KeymakerMasterKey& key, uint32 doorId) const
{
    for (const auto& p : m_portals)
    {
        if (p.doorId == doorId)
        {
            if (key.durabilityUses == 0) return false;
            // Source key opens all doors
            if (key.keyType == KEY_SOURCE_KEY) return true;
            return (key.keyType == p.requiredKey);
        }
    }
    return false;
}

bool BackdoorNetwork::UnlockDoorWithKey(uint32 doorId, KeymakerMasterKey& key)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    for (auto& p : m_portals)
    {
        if (p.doorId == doorId)
        {
            if (!VerifyKeyForDoor(key, doorId))
            {
                return false;
            }

            p.lockStatus = DOOR_UNLOCKED;
            if (key.keyType != KEY_SOURCE_KEY && key.durabilityUses > 0)
            {
                key.durabilityUses--;
            }
            sLog.outString("[BackdoorNetwork] Door %u unlocked with key %s (Remaining key uses: %u)",
                           doorId, key.keyName.c_str(), key.durabilityUses);
            return true;
        }
    }
    return false;
}

uint32 BackdoorNetwork::GetTotalTransits() const
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    return m_totalTransits;
}

void BackdoorNetwork::Update(uint32 deltaMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    uint32 now = getMSTime();
    for (auto it = m_firewallAnchors.begin(); it != m_firewallAnchors.end(); ) {
        if (now >= it->second.expireTimeMs) {
            INFO_LOG(format("BackdoorNetwork: Hardline Firewall Anchor expired on Node %1%") % it->first);
            it = m_firewallAnchors.erase(it);
        } else {
            ++it;
        }
    }

    // Check for civilians / redpills arriving at anchored hardlines to jack out
    const auto& nodes = sFactionWarMgr.GetControlNodes();
    for (const auto& kv : m_firewallAnchors) {
        uint32 hardlineNodeId = kv.first;
        auto it = nodes.find(hardlineNodeId);
        if (it == nodes.end()) continue;

        float hx = it->second.x;
        float hz = it->second.z;
        auto nearbyClients = sSpatialGrid.GetClientsInRadius(hx, hz, 600.0f);
        for (GameClient* gc : nearbyClients) {
            if (!gc || !gc->isBot()) continue;
            uint32 gid = gc->GetPlayerGoId();
            PlayerObject* po = sObjMgr.getGOPtrSafe(gid);
            if (!po || po->isDead()) continue;
            auto bot = sBotMgr.GetBotByGOID(gid);
            if (!bot) continue;
            if (bot->IsEvacuating() || po->getFactionName() == "Civilian" ||
                po->getHandle().find("Awakened_Redpill") != std::string::npos) {
                ExecuteCivilianJackout(gid, hardlineNodeId);
            }
        }
    }
}

bool BackdoorNetwork::DeployFirewallAnchor(uint32 hardlineId, uint32 durationMs, uint32 squadId)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    FirewallAnchor anchor;
    anchor.hardlineId = hardlineId;
    anchor.expireTimeMs = getMSTime() + durationMs;
    anchor.deployedBySquadId = squadId;
    m_firewallAnchors[hardlineId] = anchor;

    INFO_LOG(format("BackdoorNetwork: Deployed Hardline Firewall Anchor on Node %1% for %2% seconds (Squad #%3%)")
             % hardlineId % (durationMs / 1000) % squadId);
    return true;
}

bool BackdoorNetwork::HasFirewallAnchor(uint32 hardlineId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    auto it = m_firewallAnchors.find(hardlineId);
    if (it == m_firewallAnchors.end()) return false;
    return getMSTime() < it->second.expireTimeMs;
}

bool BackdoorNetwork::SealDoorByAgents(uint32 doorId)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    if (HasFirewallAnchor(doorId)) {
        INFO_LOG(format("BackdoorNetwork: Agent attempt to seal Door %1% REPELLED by Zion Firewall Anchor!") % doorId);
        return false;
    }
    for (auto& p : m_portals) {
        if (p.doorId == doorId) {
            p.lockStatus = DOOR_SEALED_BY_AGENTS;
            INFO_LOG(format("BackdoorNetwork: Door %1% successfully sealed by Machine System Agents.") % doorId);
            return true;
        }
    }
    return false;
}

bool BackdoorNetwork::ExecuteCivilianJackout(uint32 entityGoId, uint32 hardlineId)
{
    PlayerObject* po = sObjMgr.getGOPtrSafe(entityGoId);
    if (!po || po->isDead()) return false;

    LocationVector pos = po->getPosition();
    sGame.BroadcastNear((float)pos.x, (float)pos.z, 2000.0f, std::make_shared<JackoutEffectMsg>(entityGoId, true)->toBuf(), false);
    sGame.AnnounceStateUpdateNear((float)pos.x, (float)pos.z, 20000.0f, std::make_shared<EmoteMsg>(entityGoId, 45, 1)); // Digital dissolution FX

    auto bot = sBotMgr.GetBotByGOID(entityGoId);
    if (bot) {
        bot->Say("Civilian: The telephone... I hear the operator! Pulling me out!");
        bot->Invalidate();
    }

    po->die(0); // Safely despawn from Matrix
    sFactionWarMgr.registerPvPKill(FACTION_ZION, FACTION_MACHINES); // Zion score reward
    INFO_LOG(format("BackdoorNetwork: Civilian %1% successfully jacked out to Zion via Hardline %2%!") % entityGoId % hardlineId);
    return true;
}

// ============================================================================
// Procedural Infinite Green Hallway Backdoors (Epoch IV)
// ============================================================================
void BackdoorNetwork::InitializeProceduralHallways(uint32 segmentCount)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    m_infiniteSegments.clear();
    m_infiniteHallwayActive = true;

    for (uint32 i = 0; i < segmentCount; ++i)
    {
        InfiniteHallwaySegment seg;
        seg.segmentIndex = i;
        seg.hallwayDepthMeters = 50.0f * (i + 1);
        seg.doorCount = 6;
        seg.anomalyGlitchRate = 0.05f * (i + 1);
        
        for (uint32 d = 0; d < seg.doorCount; ++d) {
            seg.linkedDoorIds.push_back(1000 + i * 10 + d);
        }
        m_infiniteSegments.push_back(seg);
    }
}

bool BackdoorNetwork::TraverseInfiniteHallway(uint32 currentSegment, uint32 doorChoice, uint32& outNextSegment, PortalPosition& outExitPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    if (m_infiniteSegments.empty()) return false;

    if (currentSegment >= m_infiniteSegments.size()) {
        currentSegment = 0;
    }

    const auto& seg = m_infiniteSegments[currentSegment];
    if (doorChoice >= seg.doorCount) {
        return false;
    }
    m_totalTransits++;

    if (doorChoice == 0) {
        outNextSegment = 0;
        outExitPos.x = 99640.0f;
        outExitPos.y = 500.0f;
        outExitPos.z = 8350.0f;
        outExitPos.headingDeg = 180.0f;
        return true;
    } else if (doorChoice == seg.doorCount - 1) {
        outNextSegment = (currentSegment + 1) % m_infiniteSegments.size();
        outExitPos.x = 0.0f;
        outExitPos.y = 0.0f;
        outExitPos.z = seg.hallwayDepthMeters * 10.0f;
        outExitPos.headingDeg = 90.0f;
        return true;
    } else {
        outNextSegment = (currentSegment + doorChoice) % m_infiniteSegments.size();
        outExitPos.x = 100.0f * doorChoice;
        outExitPos.y = 0.0f;
        outExitPos.z = 1000.0f;
        outExitPos.headingDeg = 0.0f;
        return true;
    }
}

size_t BackdoorNetwork::GetInfiniteSegmentCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    return m_infiniteSegments.size();
}

const InfiniteHallwaySegment* BackdoorNetwork::GetInfiniteSegment(uint32 segmentIndex) const
{
    std::lock_guard<std::recursive_mutex> lock(m_networkMutex);
    if (segmentIndex < m_infiniteSegments.size()) {
        return &m_infiniteSegments[segmentIndex];
    }
    return nullptr;
}

// ============================================================================
// HEADLESS TEST SUITE: BACKDOOR NETWORK & KEYMAKER CIPHERS (SUITE 18)
// ============================================================================
void RunBackdoorTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING BACKDOOR NETWORK & CIPHERS TEST SUITE (SUITE 18)  " << std::endl;
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

    // 1. System Initialization & Non-Euclidean Portal Registry
    sBackdoorNetwork.Initialize();
    TEST_ASSERT(sBackdoorNetwork.GetPortalCount() == 6, "Registered 6 default backdoor portals");
    TEST_ASSERT(sBackdoorNetwork.GetTotalTransits() == 0, "Initial portal transits count is zero");

    const BackdoorPortal* p101 = sBackdoorNetwork.GetPortal(101);
    TEST_ASSERT(p101 != nullptr, "Portal 101 (Slums Warehouse) retrieved");
    TEST_ASSERT(p101 && p101->destinationDistrict == "The Slums", "Portal 101 links to The Slums district");
    TEST_ASSERT(p101 && p101->requiredKey == KEY_DISTRICT_MASTER, "Portal 101 requires KEY_DISTRICT_MASTER");
    TEST_ASSERT(p101 && p101->lockStatus == DOOR_LOCKED, "Portal 101 begins in locked state");

    const BackdoorPortal* p204 = sBackdoorNetwork.GetPortal(204);
    TEST_ASSERT(p204 != nullptr, "Portal 204 (Downtown Helipad) retrieved");
    TEST_ASSERT(p204 && p204->requiredKey == KEY_ROOFTOP_BYPASS, "Portal 204 requires KEY_ROOFTOP_BYPASS");

    const BackdoorPortal* p315 = sBackdoorNetwork.GetPortal(315);
    TEST_ASSERT(p315 != nullptr, "Portal 315 (Club Hel Tunnel) retrieved");
    TEST_ASSERT(p315 && p315->destinationDistrict == "International", "Portal 315 links to International district");

    const BackdoorPortal* p408 = sBackdoorNetwork.GetPortal(408);
    TEST_ASSERT(p408 != nullptr, "Portal 408 (Richland Vault) retrieved");
    TEST_ASSERT(p408 && p408->requiredKey == KEY_VAULT_BREAKER, "Portal 408 requires KEY_VAULT_BREAKER");

    const BackdoorPortal* p777 = sBackdoorNetwork.GetPortal(777);
    TEST_ASSERT(p777 != nullptr, "Portal 777 (Keymaker Workshop) retrieved");
    TEST_ASSERT(p777 && p777->isSourceDoor == false, "Portal 777 is standard workshop portal");

    const BackdoorPortal* p999 = sBackdoorNetwork.GetPortal(999);
    TEST_ASSERT(p999 != nullptr, "Portal 999 (The Source Gate) retrieved");
    TEST_ASSERT(p999 && p999->isSourceDoor == true, "Portal 999 is designated Source Gate");
    TEST_ASSERT(p999 && p999->requiredKey == KEY_SOURCE_KEY, "Portal 999 requires KEY_SOURCE_KEY");

    // 2. Keymaker Cryptographic Ciphers & Master Key Forging
    CryptographicCipherPuzzle puz1 = sBackdoorNetwork.GenerateCipherPuzzle(KEY_DISTRICT_MASTER);
    TEST_ASSERT(puz1.seedMask == 0x5A5A3C3C, "District Master cipher seed mask verified");
    TEST_ASSERT(puz1.tumblerPinParity == 0x0F0F, "District Master cipher tumbler parity verified");
    uint32 exp1 = (0x5A5A3C3C ^ 0xDEADBEEF) + 0x0F0F;
    TEST_ASSERT(puz1.expectedSolution == exp1, "Cryptographic cipher hash formula matches");

    // Invalid cipher attempt fails
    KeymakerMasterKey badKey;
    bool craftBad = sBackdoorNetwork.CraftMasterKey(101, KEY_DISTRICT_MASTER, 0x12345678, badKey);
    TEST_ASSERT(!craftBad, "CraftMasterKey fails on corrupted tumbler solution");

    // Valid District Master key forging
    KeymakerMasterKey masterKey1;
    bool craftOk1 = sBackdoorNetwork.CraftMasterKey(101, KEY_DISTRICT_MASTER, puz1.expectedSolution, masterKey1);
    TEST_ASSERT(craftOk1, "Keymaker forged District Master Key successfully");
    TEST_ASSERT(masterKey1.keyId == 1001, "First crafted key assigned ID 1001");
    TEST_ASSERT(masterKey1.durabilityUses == 10, "District Master Key has 10 uses");
    TEST_ASSERT(masterKey1.isForged == true, "Key is flagged as forged");

    // Craft other key tiers
    CryptographicCipherPuzzle puzRooftop = sBackdoorNetwork.GenerateCipherPuzzle(KEY_ROOFTOP_BYPASS);
    KeymakerMasterKey rooftopKey;
    bool craftRoof = sBackdoorNetwork.CraftMasterKey(101, KEY_ROOFTOP_BYPASS, puzRooftop.expectedSolution, rooftopKey);
    TEST_ASSERT(craftRoof && rooftopKey.durabilityUses == 8, "Rooftop Bypass Key forged with 8 uses");

    CryptographicCipherPuzzle puzVault = sBackdoorNetwork.GenerateCipherPuzzle(KEY_VAULT_BREAKER);
    KeymakerMasterKey vaultKey;
    bool craftVault = sBackdoorNetwork.CraftMasterKey(101, KEY_VAULT_BREAKER, puzVault.expectedSolution, vaultKey);
    TEST_ASSERT(craftVault && vaultKey.durabilityUses == 3, "Vault Breaker Key forged with 3 uses");

    CryptographicCipherPuzzle puzSource = sBackdoorNetwork.GenerateCipherPuzzle(KEY_SOURCE_KEY);
    KeymakerMasterKey sourceKey;
    bool craftSource = sBackdoorNetwork.CraftMasterKey(101, KEY_SOURCE_KEY, puzSource.expectedSolution, sourceKey);
    TEST_ASSERT(craftSource && sourceKey.durabilityUses == 999, "The Source Master Key forged with permanent durability");

    // 3. Key Compatibility & Door Unlocking
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(masterKey1, 101) == true, "District Master Key fits Door 101");
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(masterKey1, 204) == false, "District Master Key cannot unlock Rooftop Door 204");
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(rooftopKey, 204) == true, "Rooftop Bypass Key fits Door 204");
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(vaultKey, 408) == true, "Vault Breaker Key fits Door 408");

    // Source Key universal privilege
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(sourceKey, 101) == true, "Source Key unlocks Door 101");
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(sourceKey, 204) == true, "Source Key unlocks Door 204");
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(sourceKey, 408) == true, "Source Key unlocks Door 408");
    TEST_ASSERT(sBackdoorNetwork.VerifyKeyForDoor(sourceKey, 999) == true, "Source Key unlocks Door 999");

    // Unlock Door 101 with masterKey1
    bool unlock101 = sBackdoorNetwork.UnlockDoorWithKey(101, masterKey1);
    TEST_ASSERT(unlock101, "Door 101 unlocked successfully");
    TEST_ASSERT(masterKey1.durabilityUses == 9, "Key durability decremented after door unlock");
    TEST_ASSERT(p101->lockStatus == DOOR_UNLOCKED, "Door 101 status is now DOOR_UNLOCKED");

    // Source Key does not lose durability
    bool unlock408 = sBackdoorNetwork.UnlockDoorWithKey(408, sourceKey);
    TEST_ASSERT(unlock408, "Source Key unlocks Door 408");
    TEST_ASSERT(sourceKey.durabilityUses == 999, "Source Key maintains infinite durability");

    // 4. Non-Euclidean Traversal Mathematics
    float outX = 0.0f, outY = 0.0f, outZ = 0.0f, outHeading = 0.0f;

    // Traversing locked door without key fails
    bool travLocked = sBackdoorNetwork.TraverseBackdoor(204, 100.0f, 0.0f, 2000.0f, 90.0f, outX, outY, outZ, outHeading, nullptr);
    TEST_ASSERT(!travLocked, "Traversal through locked Door 204 without key denied");

    // Traversal through unlocked Door 101
    // Entrance: (100.0, 0.0, 1000.0, 90°), Exit: (99640.0, 500.0, 8350.0, 180°)
    // Test input with relative offset: (110.0, 5.0, 1000.0, 45°) -> relX=10, relY=5, relZ=0
    // DeltaHeading = 180° - 90° = +90°
    // rotX = 10 * cos(90°) - 0 * sin(90°) = 0
    // rotZ = 10 * sin(90°) + 0 * cos(90°) = 10
    // outX = 99640 + 0 = 99640, outY = 500 + 5 = 505, outZ = 8350 + 10 = 8360
    // outHeading = 45° + 90° = 135°
    bool trav101 = sBackdoorNetwork.TraverseBackdoor(101, 110.0f, 5.0f, 1000.0f, 45.0f, outX, outY, outZ, outHeading);
    TEST_ASSERT(trav101, "Traversal through Door 101 succeeded");
    TEST_ASSERT(std::fabs(outX - 99640.0f) < 0.1f, "Non-Euclidean X coordinate transformation accurate");
    TEST_ASSERT(std::fabs(outY - 505.0f) < 0.1f, "Non-Euclidean Y coordinate transformation accurate");
    TEST_ASSERT(std::fabs(outZ - 8360.0f) < 0.1f, "Non-Euclidean Z coordinate transformation accurate");
    TEST_ASSERT(std::fabs(outHeading - 135.0f) < 0.1f, "Destination heading transformation accurate");
    TEST_ASSERT(p101->transitCount == 1, "Door 101 transit counter incremented");
    TEST_ASSERT(sBackdoorNetwork.GetTotalTransits() == 1, "Total network transits incremented");

    // Traversing locked door by presenting valid key directly
    bool trav204 = sBackdoorNetwork.TraverseBackdoor(204, 100.0f, 0.0f, 2000.0f, 90.0f, outX, outY, outZ, outHeading, &rooftopKey);
    TEST_ASSERT(trav204, "Traversal through locked Door 204 granted with Rooftop Bypass Key");
    TEST_ASSERT(sBackdoorNetwork.GetTotalTransits() == 2, "Total network transits updated to 2");

    // 5. Hardline Firewall Anchors & Agent Door Sealing
    sBackdoorNetwork.DeployFirewallAnchor(101, 180000, 7);
    TEST_ASSERT(sBackdoorNetwork.HasFirewallAnchor(101) == true, "Hardline Firewall Anchor active on Door 101");
    TEST_ASSERT(sBackdoorNetwork.HasFirewallAnchor(315) == false, "Door 315 has no firewall anchor");

    // Agents attempt to seal anchored door -> REPELLED
    bool seal101 = sBackdoorNetwork.SealDoorByAgents(101);
    TEST_ASSERT(!seal101, "Agent door sealing REPELLED by active Zion Firewall Anchor");
    TEST_ASSERT(p101->lockStatus != DOOR_SEALED_BY_AGENTS, "Door 101 remains unsealed");

    // Agents seal unprotected door
    bool seal315 = sBackdoorNetwork.SealDoorByAgents(315);
    TEST_ASSERT(seal315, "Door 315 sealed by Machine System Agents");
    TEST_ASSERT(p315->lockStatus == DOOR_SEALED_BY_AGENTS, "Door 315 status is DOOR_SEALED_BY_AGENTS");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  BACKDOOR NETWORK & CIPHERS TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

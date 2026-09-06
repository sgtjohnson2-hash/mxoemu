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
    m_totalTransits = 0;

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

#ifndef MXOEMU_BACKDOOR_NETWORK_H
#define MXOEMU_BACKDOOR_NETWORK_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum BackdoorKeyType
{
    KEY_NONE = 0,
    KEY_DISTRICT_MASTER = 1,
    KEY_ROOFTOP_BYPASS = 2,
    KEY_VAULT_BREAKER = 3,
    KEY_SOURCE_KEY = 4
};

enum DoorLockStatus
{
    DOOR_UNLOCKED = 0,
    DOOR_LOCKED = 1,
    DOOR_SEALED_BY_AGENTS = 2
};

struct PortalPosition
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float headingDeg{0.0f};
};

struct BackdoorPortal
{
    uint32 doorId{0};
    uint32 doorNumber{101};
    std::string name{"Door 101 - Slums Warehouse"};
    std::string destinationDistrict{"The Slums"};
    PortalPosition hallwayEntrancePos;
    PortalPosition megacityExitPos;
    BackdoorKeyType requiredKey{KEY_DISTRICT_MASTER};
    DoorLockStatus lockStatus{DOOR_LOCKED};
    uint32 transitCount{0};
    bool isSourceDoor{false};
};

struct KeymakerMasterKey
{
    uint32 keyId{0};
    uint32 ownerCharacterId{0};
    BackdoorKeyType keyType{KEY_DISTRICT_MASTER};
    std::string keyName{"Keymaker District Master"};
    uint32 durabilityUses{5};
    uint32 maxUses{5};
    uint32 cipherHash{0};
    bool isForged{false};
};

struct CryptographicCipherPuzzle
{
    uint32 puzzleId{0};
    BackdoorKeyType keyType{KEY_DISTRICT_MASTER};
    uint32 seedMask{0x5A5A3C3C};
    uint32 tumblerPinParity{0x0F0F};
    uint32 expectedSolution{0};
};

class BackdoorNetwork : public Singleton<BackdoorNetwork>
{
public:
    BackdoorNetwork();
    ~BackdoorNetwork();

    void Initialize();
    void Reset();

    // Portal Registry & Non-Euclidean Spatial Links
    void RegisterBackdoor(uint32 doorId, uint32 doorNumber, const std::string& name,
                          const std::string& district, const PortalPosition& entrance,
                          const PortalPosition& destination, BackdoorKeyType reqKey, bool isSource = false);
    const BackdoorPortal* GetPortal(uint32 doorId) const;
    const std::vector<BackdoorPortal>& GetAllPortals() const { return m_portals; }
    size_t GetPortalCount() const;

    // Non-Euclidean Portal Traversal Math
    bool TraverseBackdoor(uint32 doorId, float inX, float inY, float inZ, float inHeading,
                          float& outX, float& outY, float& outZ, float& outHeading,
                          const KeymakerMasterKey* keyUsed = nullptr);

    // Keymaker Cryptographic Key Crafting
    CryptographicCipherPuzzle GenerateCipherPuzzle(BackdoorKeyType keyType);
    bool CraftMasterKey(uint32 characterId, BackdoorKeyType keyType, uint32 cipherSolution, KeymakerMasterKey& outKey);
    bool VerifyKeyForDoor(const KeymakerMasterKey& key, uint32 doorId) const;
    bool UnlockDoorWithKey(uint32 doorId, KeymakerMasterKey& key);

    // Telemetry & Statistics
    uint32 GetTotalTransits() const;

private:
    mutable std::recursive_mutex m_networkMutex;
    std::vector<BackdoorPortal> m_portals;
    std::map<uint32, KeymakerMasterKey> m_craftedKeys;
    uint32 m_nextKeyId{1001};
    uint32 m_nextPuzzleId{1};
    uint32 m_totalTransits{0};
};

#define sBackdoorNetwork BackdoorNetwork::getSingleton()

#endif // MXOEMU_BACKDOOR_NETWORK_H

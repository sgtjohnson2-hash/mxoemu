#ifndef MXOEMU_FREEWAY_COMBAT_SYSTEM_H
#define MXOEMU_FREEWAY_COMBAT_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum VehicleType
{
    VEHICLE_DUCATI_996     = 1,
    VEHICLE_CADILLAC_CTS   = 2,
    VEHICLE_SEMI_TRAILER   = 3,
    VEHICLE_POLICE_CRUISER = 4
};

enum VehicleLane
{
    LANE_INNER  = 0,
    LANE_MIDDLE = 1,
    LANE_OUTER  = 2,
    LANE_HOV    = 3
};

enum TwinPhaseState
{
    TWIN_SOLID        = 0,
    TWIN_PHASING      = 1,
    TWIN_INFILTRATING = 2
};

struct FreewayVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    FreewayVector3() = default;
    FreewayVector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    FreewayVector3 operator+(const FreewayVector3& o) const { return FreewayVector3(x + o.x, y + o.y, z + o.z); }
    FreewayVector3 operator-(const FreewayVector3& o) const { return FreewayVector3(x - o.x, y - o.y, z - o.z); }
    FreewayVector3 operator*(float s) const { return FreewayVector3(x * s, y * s, z * s); }
    FreewayVector3& operator+=(const FreewayVector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

struct FreewayVehicle
{
    uint32 vehicleId{1};
    VehicleType type{VEHICLE_CADILLAC_CTS};
    VehicleLane lane{LANE_MIDDLE};
    FreewayVector3 position{0.0f, 0.0f, 0.0f};
    FreewayVector3 velocity{0.0f, 0.0f, 75.0f};
    float speedMph{75.0f};
    float health{2500.0f};
    float maxHealth{2500.0f};
    bool isWrecked{false};
    bool isTireBlown{false};
    bool hasRoofCombatant{false};
    uint32 roofDuelId{0};
};

struct RoofWireFuDuel
{
    uint32 duelId{1};
    uint32 vehicleId{1};
    uint32 participant1Id{0};
    uint32 participant2Id{0};
    float windResistanceForce{120.0f};
    float p1BalanceMeter{100.0f};
    float p2BalanceMeter{100.0f};
    bool isP1Airborne{false};
    bool isP2Airborne{false};
};

struct AgentTwinAI
{
    uint32 twinId{1};
    TwinPhaseState phaseState{TWIN_SOLID};
    uint32 currentTargetVehicleId{0};
    float etherealTimerSec{0.0f};
    bool isInvulnerable{false};
};

struct KeymakerEscortState
{
    uint32 escortVehicleId{1};
    float escortHealth{5000.0f};
    float progressPercent{0.0f};
    bool isAmbushed{false};
    bool isEscortSuccessful{false};
};

class FreewayCombatSystem : public Singleton<FreewayCombatSystem>
{
public:
    FreewayCombatSystem();
    ~FreewayCombatSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Vehicle Traffic Management
    uint32 SpawnVehicle(VehicleType type, VehicleLane lane, float initialSpeedMph);
    bool TriggerTireBlowout(uint32 vehicleId);
    bool DamageVehicle(uint32 vehicleId, float damage);

    // Rooftop Wire-Fu Duels
    uint32 BoardVehicleRoof(uint32 vehicleId, uint32 playerId);
    bool UpdateRoofDuel(uint32 duelId, uint32 playerId, float balanceDelta, bool airborneJump);

    // Agent Twins Phase-Shifting
    bool TriggerTwinPhaseShift(uint32 twinId, bool phaseOn);

    // Keymaker Escort Mission
    void StartKeymakerEscort();
    bool DamageEscortSedan(float damage);

    // High-Speed Ramming & Collisions
    bool ExecuteVehicleRam(uint32 attackerVehicleId, uint32 targetVehicleId);

    // Rooftop Duels & Falloffs
    bool HandleRooftopFalloff(uint32 duelId, uint32 participantId);

    // Agent Infiltration & Hood Overwrites
    bool AgentJumpOntoVehicle(uint32 vehicleId, const std::string& agentName = "Agent Johnson");

    // Telemetry & Getters
    size_t GetVehicleCount() const;
    size_t GetActiveRoofDuelCount() const;
    const KeymakerEscortState& GetEscortState() const { return m_escort; }
    bool IsTwinPhased(uint32 twinId) const;
    bool GetVehicle(uint32 vehicleId, FreewayVehicle& outVehicle) const;

private:
    mutable std::recursive_mutex m_freewayMutex;
    std::map<uint32, FreewayVehicle> m_vehicles;
    std::map<uint32, RoofWireFuDuel> m_duels;
    std::map<uint32, AgentTwinAI> m_twins;
    KeymakerEscortState m_escort;

    uint32 m_nextVehicleId{1};
    uint32 m_nextDuelId{1};
    float m_simTimeSec{0.0f};
};

#define sFreewayCombatSystem FreewayCombatSystem::getSingleton()

void RunFreewayCombatTestSuite();

#endif // MXOEMU_FREEWAY_COMBAT_SYSTEM_H

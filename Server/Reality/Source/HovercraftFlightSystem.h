#ifndef MXOEMU_HOVERCRAFT_FLIGHT_SYSTEM_H
#define MXOEMU_HOVERCRAFT_FLIGHT_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum HovercraftType
{
    HOVERCRAFT_NEBUCHADNEZZAR = 0,
    HOVERCRAFT_LOGOS = 1,
    HOVERCRAFT_MJOLNIR = 2,
    HOVERCRAFT_VIGILANT = 3
};

struct FlightVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    FlightVector3() = default;
    FlightVector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    FlightVector3 Normalized() const
    {
        float len = Length();
        if (len < 0.0001f) return FlightVector3(0.0f, 0.0f, 0.0f);
        return FlightVector3(x / len, y / len, z / len);
    }

    FlightVector3 operator+(const FlightVector3& o) const { return FlightVector3(x + o.x, y + o.y, z + o.z); }
    FlightVector3 operator-(const FlightVector3& o) const { return FlightVector3(x - o.x, y - o.y, z - o.z); }
    FlightVector3 operator*(float s) const { return FlightVector3(x * s, y * s, z * s); }
    FlightVector3 operator/(float s) const { return FlightVector3(x / s, y / s, z / s); }
    FlightVector3& operator+=(const FlightVector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

struct FlightAngles
{
    float pitch{0.0f}; // Degrees: nose up (+) / nose down (-)
    float roll{0.0f};  // Degrees: bank right (+) / bank left (-)
    float yaw{0.0f};   // Degrees: compass heading 0..360
};

struct HovercraftPhysicsState
{
    uint32 shipId{1};
    HovercraftType type{HOVERCRAFT_NEBUCHADNEZZAR};
    std::string name{"Nebuchadnezzar"};
    FlightVector3 position{0.0f, 500.0f, 0.0f};
    FlightVector3 velocity{0.0f, 0.0f, 0.0f};
    FlightVector3 acceleration{0.0f, 0.0f, 0.0f};
    FlightAngles orientation{0.0f, 0.0f, 0.0f};
    FlightAngles angularVelocity{0.0f, 0.0f, 0.0f};

    float targetElevation{500.0f};
    float currentElevation{500.0f};
    float repulsorForce{0.0f};
    float maxSpeed{3200.0f};       // units/sec
    float mass{125000.0f};        // kg
    float hullIntegrity{100.0f};  // %
    float maxHull{100.0f};
    float acousticSignature{0.45f}; // 0.0 to 1.0
    float thermalSignature{0.50f};  // 0.0 to 1.0
    bool enginesActive{true};
    bool subterraneanDraftAffected{true};
};

enum SentinelState
{
    SENTINEL_PATROL = 0,
    SENTINEL_SEARCHING,
    SENTINEL_SWARMING,
    SENTINEL_ATTACKING,
    SENTINEL_EMP_DISABLED
};

struct SentinelDrone
{
    uint32 droneId{0};
    FlightVector3 position{0.0f, 600.0f, 0.0f};
    FlightVector3 velocity{0.0f, 0.0f, 0.0f};
    SentinelState state{SENTINEL_PATROL};
    float health{250.0f};
    float disableTimerSec{0.0f};
    float searchTimerSec{0.0f};
    uint32 targetShipId{0};
    float attackCooldownSec{0.0f};
};

struct EMPShockwaveSystem
{
    float chargePercent{0.0f};       // 0 to 100%
    float chargeRatePerSec{25.0f};   // 4s to full charge
    bool isCharging{false};
    bool isReady{false};
    float cooldownRemainingSec{0.0f};
    float burstRadiusUnits{50000.0f}; // 500 meters
    uint32 totalDetonations{0};
    uint32 sentinelsNeutralized{0};
    float shockwaveVisualPulseSec{0.0f};
};

enum CarrierLockState
{
    CARRIER_UNSTABLE = 0,
    CARRIER_LOCKED = 1,
    CARRIER_LOST = 2
};

struct BroadcastCorridor
{
    uint32 corridorId{0};
    std::string name{"Sewer Main Line 04"};
    FlightVector3 startPoint{0.0f, 300.0f, -50000.0f};
    FlightVector3 endPoint{0.0f, 300.0f, 50000.0f};
    float channelFrequencyMHz{104.7f};
    float depthMeters{120.0f};
    float optimalBandwidthKbps{10240.0f};
};

struct UplinkTelemetry
{
    CarrierLockState lockState{CARRIER_UNSTABLE};
    float signalStrength{0.0f}; // 0.0f to 1.0f
    float packetLatencyMs{22.0f};
    float packetLossPercent{0.0f};
    bool isJackInAuthorized{false};
    uint32 connectedCorridorId{0};
    std::string statusText{"Awaiting Corridor Alignment"};
};

class HovercraftFlightSystem : public Singleton<HovercraftFlightSystem>
{
public:
    HovercraftFlightSystem();
    ~HovercraftFlightSystem();

    void Initialize();
    void Reset();

    // 6-DOF Physics Simulation Tick
    void UpdateSimulation(float deltaTimeSec);

    // Ship Registration & Control
    uint32 SpawnHovercraft(HovercraftType type, const FlightVector3& spawnPos, const std::string& customName = "");
    HovercraftPhysicsState* GetHovercraft(uint32 shipId);
    bool ApplyThrustInput(uint32 shipId, float forwardThrust, float strafeThrust, float verticalThrust, float pitchInput, float rollInput, float yawInput);

    // Cavern Walls & Pipeline Collision
    void SetCavernBounds(float minX, float maxX, float minY, float maxY, float minZ, float maxZ);
    bool CheckCavernCollision(uint32 shipId, float& outDamageDealt);

    // Sentinel Swarm AI
    void SpawnSentinelSwarm(size_t droneCount, const FlightVector3& centerPos, float spawnRadius = 8000.0f);
    void UpdateSentinelSwarm(float deltaTimeSec);
    size_t GetSentinelCount() const;
    size_t GetActiveSentinelCount() const;
    const std::vector<SentinelDrone>& GetSentinels() const { return m_sentinels; }

    // EMP Shockwave Weapon
    void StartEMPCharging(uint32 shipId);
    void CancelEMPCharging();
    bool DetonateEMP(uint32 shipId, uint32& outSentinelsDisabled);
    const EMPShockwaveSystem& GetEMPSystem() const { return m_emp; }

    // Pirate Broadcast Corridor Uplink
    void RegisterBroadcastCorridor(uint32 corridorId, const std::string& name, const FlightVector3& startPt, const FlightVector3& endPt, float freqMHz);
    UplinkTelemetry CalculateUplinkTelemetry(uint32 shipId);

    // Metrics & Telemetry
    size_t GetShipCount() const;

private:
    void ComputeBoidSteering(SentinelDrone& drone, float deltaTimeSec);
    FlightVector3 ComputeDraftTurbulence(const FlightVector3& pos, float timeSec);

    mutable std::recursive_mutex m_systemMutex;
    std::map<uint32, HovercraftPhysicsState> m_ships;
    std::vector<SentinelDrone> m_sentinels;
    std::vector<BroadcastCorridor> m_corridors;
    EMPShockwaveSystem m_emp;

    uint32 m_nextShipId{1};
    uint32 m_nextDroneId{1};
    float m_simulationTimeSec{0.0f};

    // Cavern sewer boundaries
    float m_cavernMinX{-25000.0f};
    float m_cavernMaxX{25000.0f};
    float m_cavernMinY{0.0f};
    float m_cavernMaxY{2000.0f};
    float m_cavernMinZ{-100000.0f};
    float m_cavernMaxZ{100000.0f};
};

#define sHovercraftFlightSys HovercraftFlightSystem::getSingleton()

#endif // MXOEMU_HOVERCRAFT_FLIGHT_SYSTEM_H

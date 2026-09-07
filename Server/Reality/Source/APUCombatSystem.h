#ifndef MXOEMU_APU_COMBAT_SYSTEM_H
#define MXOEMU_APU_COMBAT_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <memory>
#include <algorithm>

enum APUModel
{
    APU_MODEL_ZION_STANDARD = 0,
    APU_MODEL_DOCK_DEFENDER = 1,
    APU_MODEL_HEAVY_SIEGE   = 2
};

enum APURunnerState
{
    RUNNER_IDLE = 0,
    RUNNER_FETCHING_AMMO = 1,
    RUNNER_DELIVERING = 2,
    RUNNER_RELOADING = 3,
    RUNNER_INCAPACITATED = 4
};

enum DockSentinelState
{
    DOCK_SENTINEL_SWARMING = 0,
    DOCK_SENTINEL_VORTEX_DIVE = 1,
    DOCK_SENTINEL_LATCHED_CUTTING = 2,
    DOCK_SENTINEL_DESTROYED = 3
};

struct APUVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    APUVector3() = default;
    APUVector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    APUVector3 Normalized() const
    {
        float l = Length();
        if (l < 0.0001f) return APUVector3(0.0f, 0.0f, 0.0f);
        return APUVector3(x / l, y / l, z / l);
    }

    APUVector3 operator+(const APUVector3& o) const { return APUVector3(x + o.x, y + o.y, z + o.z); }
    APUVector3 operator-(const APUVector3& o) const { return APUVector3(x - o.x, y - o.y, z - o.z); }
    APUVector3 operator*(float s) const { return APUVector3(x * s, y * s, z * s); }
    APUVector3 operator/(float s) const { return APUVector3(x / s, y / s, z / s); }
    APUVector3& operator+=(const APUVector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    APUVector3& operator-=(const APUVector3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

struct APUWeaponArm
{
    uint32 ammoRoundsRemaining{2500};
    uint32 ammoMaxCapacity{2500};
    float barrelTempCelsius{25.0f};
    float maxSafeTempCelsius{850.0f};
    float heatAccumulationPerShot{0.75f}; // deg C per 30mm slug
    float passiveCoolingRate{18.0f};      // deg C per second
    bool isOverheated{false};
    bool isJammed{false};
    float firingRateRpm{2500.0f};         // 2500 RPM high-velocity rotary cannon
    float fireCooldownSec{0.0f};
    float accuracyBloomRad{0.015f};
    float recoilImpulsePerShot{3.5f};     // Hydraulic recoil force per round
    uint32 totalRoundsFired{0};
};

struct APUUnitState
{
    uint32 apuId{1};
    uint32 pilotCharUID{0};
    std::string pilotHandle{"Captain Mifune"};
    APUModel model{APU_MODEL_DOCK_DEFENDER};

    APUVector3 position{0.0f, 0.0f, 0.0f};
    APUVector3 velocity{0.0f, 0.0f, 0.0f};
    float facingDegrees{0.0f};

    // Hydraulic locomotion & chassis integrity
    float hydraulicPressurePsi{3000.0f}; // Nominal 3000 PSI
    float stepPhase{0.0f};               // 0..1 gait cycle
    float suspensionDamping{0.85f};
    float legArmorIntegrity{100.0f};
    float cockpitShieldIntegrity{100.0f};
    float pilotHealth{100.0f};

    // Dual 30mm auto-cannons
    APUWeaponArm leftArm;
    APUWeaponArm rightArm;

    bool isMounted{false};
    bool activeCoolantVenting{false};
    float coolantReserveSec{45.0f};
    uint32 sentinelsLatchedCount{0};
    uint32 sentinelsNeutralized{0};
};

struct SentinelUnit
{
    uint32 sentinelId{0};
    APUVector3 position{0.0f, 1500.0f, 0.0f};
    APUVector3 velocity{0.0f, -50.0f, 0.0f};
    DockSentinelState state{DOCK_SENTINEL_SWARMING};
    float health{150.0f};
    float maxHealth{150.0f};
    uint32 targetApuId{1};
    float latchTimerSec{0.0f};
    float plasmaTorchDps{35.0f}; // Continuous DPS to APU shield/chassis
    float spiralAngleRad{0.0f};
    float spiralRadius{250.0f};
};

struct CraneHopper
{
    uint32 hopperId{1};
    std::string designation{"Cavern Gantry Crane 01"};
    APUVector3 position{-600.0f, 0.0f, -600.0f};
    uint32 ammoStockpile{50000};
    bool isOperational{true};
};

struct AmmoRunnerNPC
{
    uint32 runnerId{1};
    std::string name{"Runner Kid"};
    APUVector3 position{0.0f, 0.0f, 0.0f};
    APURunnerState state{RUNNER_IDLE};
    uint32 targetApuId{1};
    uint32 assignedHopperId{1};
    uint32 ammoBoxesCarried{2}; // 1250 rounds per box
    float speedUnitsPerSec{450.0f};
    float health{100.0f};
    float maxHealth{100.0f};
    uint32 totalDeliveriesMade{0};
};

struct DiggerBreach
{
    uint32 breachId{1};
    APUVector3 breachLocation{0.0f, 1500.0f, 0.0f}; // Cavern roof
    float breachRadius{350.0f};
    float drillHealth{50000.0f};
    bool isDomePierced{false};
    uint32 activeSentinelsDischarged{0};
    uint32 maxSentinelsToDeploy{1500};
    float dischargeRatePerSec{25.0f};
};

struct ZionSubterraneanLogistics
{
    float geothermalPowerOutputMw{1450.0f};
    float defenseGridAllocationPercent{75.0f}; // 75% to defense, 25% life support
    float lifeSupportIntegrityPercent{98.5f};
    float commanderLockApproval{65.0f};        // Strategic defense
    float morpheusBeliefAlignment{80.0f};      // Prophetic stance
};

class APUCombatSystem : public Singleton<APUCombatSystem>
{
public:
    APUCombatSystem();
    ~APUCombatSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // APU Lifecycle & Locomotion (Phase 7)
    uint32 RegisterAPU(uint32 pilotCharUID, const std::string& pilotHandle, APUModel model, const APUVector3& spawnPos);
    bool MountAPU(uint32 apuId, uint32 pilotCharUID);
    bool EjectAPU(uint32 apuId);
    bool UpdateAPULocomotion(uint32 apuId, const APUVector3& moveInput, float turnDegrees, float deltaTimeSec);

    // Ballistic Weapons & Overheat Mechanics (Phase 7)
    bool FireWeapons(uint32 apuId, bool fireLeft, bool fireRight, uint32& outRoundsFired, float deltaTimeSec);
    bool TriggerCoolantVent(uint32 apuId);
    bool ClearThermalJam(uint32 apuId);

    // Sentinel Swarm Boids Simulation (Phase 8)
    uint32 SpawnSentinel(const APUVector3& origin, uint32 targetApuId);
    size_t SpawnSentinelSwarm(uint32 count, const APUVector3& origin, uint32 targetApuId);
    void UpdateSentinelSwarm(float deltaTimeSec);
    uint32 ApplyFlakDamage(const APUVector3& burstPos, float blastRadius, float damage);
    size_t GetActiveSentinelCount() const;
    size_t GetLatchedSentinelCount(uint32 apuId) const;
    const SentinelUnit* GetSentinel(uint32 sentinelId) const;

    // Ammo Runner Logistics Loop (Phase 9)
    uint32 RegisterCraneHopper(const std::string& designation, const APUVector3& pos, uint32 stockpile);
    uint32 DispatchAmmoRunner(uint32 targetApuId, uint32 hopperId);
    uint32 DispatchAmmoRunner(uint32 targetApuId, const APUVector3& depotPos); // Backward compatible
    void UpdateAmmoRunners(float deltaTimeSec);
    bool RequestEmergencyAmmoResupply(uint32 apuId);
    const CraneHopper* GetCraneHopper(uint32 hopperId) const;
    size_t GetCraneHopperCount() const;

    // Zion Dock Breach Mega-Event
    uint32 TriggerDiggerBreach(const APUVector3& breachPos);
    void UpdateDiggerIncursions(float deltaTimeSec);
    float CalculateCrossfireBonus(const APUVector3& targetPos);

    // Telemetry & Status
    const APUUnitState* GetAPU(uint32 apuId) const;
    const AmmoRunnerNPC* GetAmmoRunner(uint32 runnerId) const;
    size_t GetActiveAPUCount() const;
    size_t GetActiveBreachCount() const;
    size_t GetActiveRunnerCount() const;
    const ZionSubterraneanLogistics& GetLogistics() const { return m_logistics; }
    void AdjustDefensePowerAllocation(float defensePercent);

private:
    mutable std::recursive_mutex m_apuMutex;
    std::map<uint32, APUUnitState> m_apus;
    std::map<uint32, AmmoRunnerNPC> m_runners;
    std::vector<CraneHopper> m_hoppers;
    std::vector<SentinelUnit> m_sentinels;
    std::vector<DiggerBreach> m_breaches;
    ZionSubterraneanLogistics m_logistics;

    uint32 m_nextApuId{1};
    uint32 m_nextRunnerId{1};
    uint32 m_nextHopperId{1};
    uint32 m_nextSentinelId{1};
    uint32 m_nextBreachId{1};
    float m_simTimeSec{0.0f};
};

#define sAPUCombatSystem APUCombatSystem::getSingleton()

#endif // MXOEMU_APU_COMBAT_SYSTEM_H

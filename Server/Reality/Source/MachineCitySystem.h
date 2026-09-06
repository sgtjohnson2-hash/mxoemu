#ifndef MXOEMU_MACHINE_CITY_SYSTEM_H
#define MXOEMU_MACHINE_CITY_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <memory>

enum DeusEmotionalState
{
    DEUS_INDIFFERENCE = 0,
    DEUS_ASSESSING    = 1,
    DEUS_RAGE         = 2,
    DEUS_CONSENSUS    = 3
};

enum DeusBossPhase
{
    DEUS_PHASE_SWARM_VORTEX  = 1,
    DEUS_PHASE_ENERGY_TETHER = 2,
    DEUS_PHASE_THE_BARGAIN   = 3,
    DEUS_PHASE_CONCLUDED     = 4
};

struct MachineVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    MachineVector3() = default;
    MachineVector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }

    MachineVector3 Normalized() const
    {
        float l = Length();
        if (l < 0.0001f) return MachineVector3(0.0f, 0.0f, 0.0f);
        return MachineVector3(x / l, y / l, z / l);
    }

    MachineVector3 operator+(const MachineVector3& o) const { return MachineVector3(x + o.x, y + o.y, z + o.z); }
    MachineVector3 operator-(const MachineVector3& o) const { return MachineVector3(x - o.x, y - o.y, z - o.z); }
    MachineVector3 operator*(float s) const { return MachineVector3(x * s, y * s, z * s); }
    MachineVector3 operator/(float s) const { return MachineVector3(x / s, y / s, z / s); }
    MachineVector3& operator+=(const MachineVector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

struct PowerGridTether
{
    uint32 tetherId{1};
    MachineVector3 anchorPos{0.0f, 5000.0f, 0.0f};
    float currentEnergyMw{750.0f};
    float integrityPercent{100.0f};
    bool isSevered{false};
};

struct DeusExMachinaState
{
    uint32 swarmDroneCount{100000};
    DeusEmotionalState emotionalState{DEUS_INDIFFERENCE};
    DeusBossPhase currentPhase{DEUS_PHASE_SWARM_VORTEX};
    float faceScaleMeters{120.0f};
    MachineVector3 position{0.0f, 15000.0f, 250000.0f}; // 01 Central Core

    float vortexShieldIntegrity{100.0f};
    float bargainProgressPercent{0.0f};
    bool peaceTreatyRatified{false};

    std::vector<PowerGridTether> powerTethers;
};

struct MachineTechBlueprint
{
    uint32 blueprintId{1};
    std::string techName{"Hardline Overclock Relay"};
    std::string description{"Boosts local bandwidth and grants instant hardline extraction."};
    uint32 requiredMachineStanding{85};
    uint32 costInfoCurrency{45000};
};

class MachineCitySystem : public Singleton<MachineCitySystem>
{
public:
    MachineCitySystem();
    ~MachineCitySystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Atmospheric Scorched Sky Transit
    bool CheckScorchedSkyBreach(float altitudeY, bool& outSunlightVisible, float& outEmpSurgeVolt) const;

    // Deus Ex Machina Encounter
    void StartDeusEncounter();
    bool DamageVortexShield(float damage);
    bool SeverPowerTether(uint32 tetherId);
    bool AdvanceBargainDialogue(const std::string& philosophicalResponse, std::string& outDeusReply);

    // Blueprints & Tech Unlocks
    uint32 RegisterMachineTech(const std::string& name, const std::string& desc, uint32 minRep, uint32 cost);
    bool UnlockTechBlueprint(uint32 blueprintId, uint32 playerReputation);

    // Telemetry & Getters
    const DeusExMachinaState& GetDeusState() const { return m_deus; }
    size_t GetSeveredTetherCount() const;
    size_t GetTotalBlueprintCount() const;
    bool IsPeaceTreatyActive() const { return m_deus.peaceTreatyRatified; }

private:
    mutable std::recursive_mutex m_cityMutex;
    DeusExMachinaState m_deus;
    std::vector<MachineTechBlueprint> m_blueprints;
    uint32 m_nextBlueprintId{1};
    float m_simTimeSec{0.0f};
};

#define sMachineCitySystem MachineCitySystem::getSingleton()

#endif // MXOEMU_MACHINE_CITY_SYSTEM_H

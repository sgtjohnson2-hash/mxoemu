#ifndef MXOEMU_ORBITAL_SATELLITE_SYSTEM_H
#define MXOEMU_ORBITAL_SATELLITE_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum SatelliteType
{
    SAT_SOLAR_ARRAY         = 1,
    SAT_KINETIC_LANCE       = 2,
    SAT_PIRATE_TRANSPONDER  = 3,
    SAT_DEFENSE_GRID        = 4
};

enum OrbitalWeaponStatus
{
    WEAPON_READY      = 0,
    WEAPON_CHARGING   = 1,
    WEAPON_DISCHARGED = 2
};

struct OrbitalSatellite
{
    uint32 satelliteId{1};
    std::string satelliteName{"Sol-Collector-Alpha"};
    SatelliteType type{SAT_SOLAR_ARRAY};
    float orbitalAltitudeKm{420.0f};
    float solarCollectionEfficiency{98.5f};
    float propellantKg{350.0f};
    bool isHijackedByPirates{false};
    bool isOnline{true};
};

struct KineticLanceStrike
{
    uint32 strikeId{1};
    uint32 targetSectorId{1};
    float tungstenRodMassKg{1500.0f};
    float impactKineticEnergyGj{12.5f};
    bool isDischarged{false};
};

struct SolarBeamRecharge
{
    uint32 targetHovercraftId{1};
    float beamPowerMw{150.0f};
    bool isBeamLocked{false};
};

class OrbitalSatelliteSystem : public Singleton<OrbitalSatelliteSystem>
{
public:
    OrbitalSatelliteSystem();
    ~OrbitalSatelliteSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // Orbital Mesh Management
    uint32 RegisterSatellite(const std::string& name, SatelliteType type, float altitudeKm);
    bool FireKineticLance(uint32 targetSector, float& outEnergyGj);
    bool DirectSolarRechargeBeam(uint32 hovercraftId, bool lockOn);
    bool HijackPirateTransponder(uint32 satelliteId);

    // Telemetry & Getters
    size_t GetSatelliteCount() const;
    size_t GetPirateTransponderCount() const;
    bool GetSatellite(uint32 satelliteId, OrbitalSatellite& outSat) const;
    bool IsSolarBeamActive() const { return m_solarBeam.isBeamLocked; }
    const KineticLanceStrike& GetLatestStrike() const { return m_latestStrike; }

private:
    mutable std::recursive_mutex m_orbitalMutex;
    std::map<uint32, OrbitalSatellite> m_satellites;
    SolarBeamRecharge m_solarBeam;
    KineticLanceStrike m_latestStrike;

    uint32 m_nextSatId{1};
    uint32 m_nextStrikeId{1};
    float m_simTimeSec{0.0f};
};

#define sOrbitalSatelliteSystem OrbitalSatelliteSystem::getSingleton()

#endif // MXOEMU_ORBITAL_SATELLITE_SYSTEM_H

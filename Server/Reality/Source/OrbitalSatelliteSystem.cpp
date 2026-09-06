#include "OrbitalSatelliteSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(OrbitalSatelliteSystem);

OrbitalSatelliteSystem::OrbitalSatelliteSystem()
{
    Initialize();
}

void OrbitalSatelliteSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    m_simTimeSec = 0.0f;
    m_satellites.clear();
    m_nextSatId = 1;
    m_nextStrikeId = 1;

    // Default Satellites in Low-Earth Orbit
    RegisterSatellite("Sol-Collector-Alpha", SAT_SOLAR_ARRAY, 420.0f);
    RegisterSatellite("Thor-Kinetic-Lance-1", SAT_KINETIC_LANCE, 380.0f);
    uint32 pirateId = RegisterSatellite("Logos-Pirate-Relay", SAT_PIRATE_TRANSPONDER, 450.0f);
    auto it = m_satellites.find(pirateId);
    if (it != m_satellites.end())
    {
        it->second.isHijackedByPirates = true;
    }
    RegisterSatellite("Machine-Defense-Array", SAT_DEFENSE_GRID, 500.0f);

    // Reset Solar Recharge Beam
    m_solarBeam.targetHovercraftId = 1;
    m_solarBeam.beamPowerMw = 150.0f;
    m_solarBeam.isBeamLocked = false;

    // Reset Kinetic Strike Telemetry
    m_latestStrike.strikeId = 0;
    m_latestStrike.targetSectorId = 0;
    m_latestStrike.tungstenRodMassKg = 1500.0f;
    m_latestStrike.impactKineticEnergyGj = 12.5f;
    m_latestStrike.isDischarged = false;
}

void OrbitalSatelliteSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // Subtle orbital precession and solar collection drift
    for (auto& pair : m_satellites)
    {
        auto& sat = pair.second;
        if (sat.type == SAT_SOLAR_ARRAY)
        {
            sat.solarCollectionEfficiency = 98.0f + 1.5f * std::sin(m_simTimeSec * 0.1f);
        }
    }
}

uint32 OrbitalSatelliteSystem::RegisterSatellite(const std::string& name, SatelliteType type, float altitudeKm)
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    OrbitalSatellite sat;
    sat.satelliteId = m_nextSatId++;
    sat.satelliteName = name;
    sat.type = type;
    sat.orbitalAltitudeKm = altitudeKm;
    sat.solarCollectionEfficiency = 98.5f;
    sat.propellantKg = 350.0f;
    sat.isHijackedByPirates = (type == SAT_PIRATE_TRANSPONDER);
    sat.isOnline = true;

    m_satellites[sat.satelliteId] = sat;
    return sat.satelliteId;
}

bool OrbitalSatelliteSystem::FireKineticLance(uint32 targetSector, float& outEnergyGj)
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    bool hasLanceSat = false;
    for (const auto& pair : m_satellites)
    {
        if (pair.second.type == SAT_KINETIC_LANCE && pair.second.isOnline)
        {
            hasLanceSat = true;
            break;
        }
    }
    if (!hasLanceSat)
    {
        outEnergyGj = 0.0f;
        return false;
    }

    m_latestStrike.strikeId = m_nextStrikeId++;
    m_latestStrike.targetSectorId = targetSector;
    m_latestStrike.tungstenRodMassKg = 1500.0f;
    m_latestStrike.impactKineticEnergyGj = 12.5f;
    m_latestStrike.isDischarged = true;

    outEnergyGj = m_latestStrike.impactKineticEnergyGj;
    return true;
}

bool OrbitalSatelliteSystem::DirectSolarRechargeBeam(uint32 hovercraftId, bool lockOn)
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    m_solarBeam.targetHovercraftId = hovercraftId;
    m_solarBeam.isBeamLocked = lockOn;
    return true;
}

bool OrbitalSatelliteSystem::HijackPirateTransponder(uint32 satelliteId)
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    auto it = m_satellites.find(satelliteId);
    if (it == m_satellites.end()) return false;

    it->second.isHijackedByPirates = true;
    it->second.type = SAT_PIRATE_TRANSPONDER;
    return true;
}

size_t OrbitalSatelliteSystem::GetSatelliteCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    return m_satellites.size();
}

size_t OrbitalSatelliteSystem::GetPirateTransponderCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    size_t count = 0;
    for (const auto& pair : m_satellites)
    {
        if (pair.second.isHijackedByPirates) count++;
    }
    return count;
}

bool OrbitalSatelliteSystem::GetSatellite(uint32 satelliteId, OrbitalSatellite& outSat) const
{
    std::lock_guard<std::recursive_mutex> lock(m_orbitalMutex);
    auto it = m_satellites.find(satelliteId);
    if (it != m_satellites.end())
    {
        outSat = it->second;
        return true;
    }
    return false;
}

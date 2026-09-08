#include "CastleTraumaModel.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(CastleTraumaModel);

CastleTraumaModel::CastleTraumaModel()
{
    Reset();
}

void CastleTraumaModel::Reset()
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    m_wounds.clear();
    m_nextWoundId = 1;

    m_vitals.bloodVolumeMl = 5000.0f;
    m_vitals.systolicBp = 120;
    m_vitals.diastolicBp = 80;
    m_vitals.heartRateBpm = 72;
    m_vitals.bodyTemperatureF = 98.6f;
    m_vitals.hasPneumothorax = false;
    m_vitals.brokenRibsCount = 0;
    m_vitals.hasConcussion = false;
    m_vitals.dominantArmCrippled = false;
    m_vitals.legCrippled = false;
}

void CastleTraumaModel::UpdateTrauma(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    if (deltaTimeSec <= 0.0f) return;

    float totalBleed = GetTotalBleedRateMlPerSec();
    if (totalBleed > 0.0f)
    {
        float lostBlood = totalBleed * deltaTimeSec;
        m_vitals.bloodVolumeMl = std::max(0.0f, m_vitals.bloodVolumeMl - lostBlood);

        // Hemodynamic compensation & Hypovolemic decompensation
        float bloodRatio = m_vitals.bloodVolumeMl / 5000.0f;
        if (bloodRatio < 0.70f) // Shock threshold (< 3500 mL)
        {
            m_vitals.systolicBp = (uint32)std::max(50.0f, 120.0f * bloodRatio);
            m_vitals.diastolicBp = (uint32)std::max(30.0f, 80.0f * bloodRatio);
            m_vitals.heartRateBpm = (uint32)std::min(160.0f, 72.0f + (1.0f - bloodRatio) * 100.0f); // Tachycardia
        }
    }
}

uint32 CastleTraumaModel::InflictBallisticTrauma(AnatomicalZone zone, float kineticEnergyJoules, bool penetratesArmor)
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    uint32 id = m_nextWoundId++;

    SpecificWound w;
    w.woundId = id;
    w.zone = zone;
    w.isSutured = false;
    w.isCauterized = false;
    w.isBandaged = false;
    w.isDisinfected = false;
    w.foreignObjectRemoved = false;

    if (penetratesArmor)
    {
        if (kineticEnergyJoules >= 1500.0f)
        {
            w.severity = SEVERITY_ARTERIAL;
            w.isArterialBleeder = true;
            w.bleedRateMlPerSec = 35.0f; // Rapid arterial hemorrhage
            w.hasLodgedBullet = false;   // Through-and-through high energy
            w.description = "Through-and-through high-velocity rifle laceration with severed arterial branch";
        }
        else
        {
            w.severity = SEVERITY_DEEP_TISSUE;
            w.isArterialBleeder = false;
            w.bleedRateMlPerSec = 12.0f; // Venous cavitation bleed
            w.hasLodgedBullet = true;    // Deformed slug embedded in tissue
            w.description = "Penetrating gunshot wound with embedded projectile slug";
        }

        if (zone == ZONE_THORACIC)
        {
            m_vitals.hasPneumothorax = true;
            m_vitals.brokenRibsCount += 2;
        }
        else if (zone == ZONE_ARM_RIGHT)
        {
            m_vitals.dominantArmCrippled = true;
        }
        else if (zone == ZONE_LEG_LEFT || zone == ZONE_LEG_RIGHT)
        {
            m_vitals.legCrippled = true;
        }
        else if (zone == ZONE_CRANIAL)
        {
            m_vitals.hasConcussion = true;
        }
    }
    else
    {
        // Non-penetrating plate impact -> Backface blunt trauma
        w.severity = SEVERITY_FLESH;
        w.hasLodgedBullet = false;
        w.isArterialBleeder = false;
        w.bleedRateMlPerSec = 1.5f;
        w.description = "Ceramic plate backface deformation hematoma and tissue contusion";

        if (zone == ZONE_THORACIC)
        {
            m_vitals.brokenRibsCount += 1;
        }
        else if (zone == ZONE_CRANIAL)
        {
            m_vitals.hasConcussion = true;
        }
    }

    m_wounds[id] = w;
    return id;
}

uint32 CastleTraumaModel::InflictBluntTrauma(AnatomicalZone zone, float impactEnergyJoules)
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    uint32 id = m_nextWoundId++;

    SpecificWound w;
    w.woundId = id;
    w.zone = zone;
    w.severity = SEVERITY_FLESH;
    w.hasLodgedBullet = false;
    w.isArterialBleeder = false;
    w.bleedRateMlPerSec = 2.0f;
    w.description = "Severe blunt force trauma contusion";

    if (zone == ZONE_THORACIC)
    {
        m_vitals.brokenRibsCount += 2;
    }
    else if (zone == ZONE_CRANIAL)
    {
        m_vitals.hasConcussion = true;
    }
    else if (zone == ZONE_LEG_LEFT || zone == ZONE_LEG_RIGHT)
    {
        m_vitals.legCrippled = true;
    }

    m_wounds[id] = w;
    return id;
}

SpecificWound* CastleTraumaModel::GetWound(uint32 woundId)
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    auto it = m_wounds.find(woundId);
    if (it != m_wounds.end())
    {
        return &it->second;
    }
    return nullptr;
}

std::vector<SpecificWound> CastleTraumaModel::GetActiveWounds() const
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    std::vector<SpecificWound> result;
    for (const auto& pair : m_wounds)
    {
        result.push_back(pair.second);
    }
    return result;
}

float CastleTraumaModel::GetTotalBleedRateMlPerSec() const
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    float total = 0.0f;
    for (const auto& pair : m_wounds)
    {
        total += pair.second.bleedRateMlPerSec;
    }
    return total;
}

float CastleTraumaModel::GetMobilityMultiplier() const
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    float mult = 1.0f;
    if (m_vitals.legCrippled) mult *= 0.35f;
    if (m_vitals.brokenRibsCount > 0) mult *= std::max(0.4f, 1.0f - (0.08f * m_vitals.brokenRibsCount));
    if (m_vitals.bloodVolumeMl < 3500.0f) mult *= 0.60f;
    return mult;
}

float CastleTraumaModel::GetAimSwayMultiplier() const
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    float mult = 1.0f;
    if (m_vitals.dominantArmCrippled) mult *= 2.5f;
    if (m_vitals.hasConcussion) mult *= 2.0f;
    if (m_vitals.brokenRibsCount > 0) mult *= 1.3f;
    return mult;
}

bool CastleTraumaModel::CanSprint() const
{
    std::lock_guard<std::recursive_mutex> lock(m_traumaMutex);
    if (m_vitals.legCrippled) return false;
    if (m_vitals.hasPneumothorax) return false;
    if (m_vitals.bloodVolumeMl < 3500.0f) return false;
    if (m_vitals.brokenRibsCount >= 3) return false;
    return true;
}

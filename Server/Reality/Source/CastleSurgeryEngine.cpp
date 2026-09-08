#include "CastleSurgeryEngine.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(CastleSurgeryEngine);

SurgicalActionResult CastleSurgeryEngine::ExtractLodgedSlug(uint32 woundId, SurgicalTool tool)
{
    std::lock_guard<std::recursive_mutex> lock(m_surgeryMutex);
    SurgicalActionResult res;
    m_totalProceduresConducted++;

    if (tool != TOOL_FORCEPS_PLIERS)
    {
        res.success = false;
        res.message = "Improper tool: Bullet extraction requires surgical forceps or needle-nose pliers.";
        return res;
    }

    SpecificWound* w = sCastleTrauma.GetWound(woundId);
    if (!w)
    {
        res.success = false;
        res.message = "Wound not found.";
        return res;
    }

    if (!w->hasLodgedBullet)
    {
        res.success = false;
        res.message = "No lodged projectile detected in wound channel.";
        return res;
    }

    w->hasLodgedBullet = false;
    w->foreignObjectRemoved = true;
    res.success = true;
    res.painSpikePercent = 45.0f;
    res.message = "Forceps gripped deformed lead slug. Extracted foreign projectile from deep tissue.";
    WithstandPainCheck(res.painSpikePercent);
    return res;
}

SurgicalActionResult CastleSurgeryEngine::FlushAndDebrideWound(uint32 woundId, SurgicalTool antisepticTool)
{
    std::lock_guard<std::recursive_mutex> lock(m_surgeryMutex);
    SurgicalActionResult res;
    m_totalProceduresConducted++;

    if (antisepticTool != TOOL_BOURBON_IRRIGATION)
    {
        res.success = false;
        res.message = "No antiseptic available: Requires high-proof Kentucky bourbon or medical alcohol flush.";
        return res;
    }

    SpecificWound* w = sCastleTrauma.GetWound(woundId);
    if (!w)
    {
        res.success = false;
        res.message = "Wound not found.";
        return res;
    }

    ConsumeBourbonAntiseptic(2);
    w->isDisinfected = true;
    w->infectionRiskPercent = 2.0f;
    res.success = true;
    res.painSpikePercent = 65.0f; // Searing chemical sting
    res.message = "Poured 100-proof Kentucky bourbon into open wound channel. Tissue disinfected, debris flushed.";
    WithstandPainCheck(res.painSpikePercent);
    return res;
}

SurgicalActionResult CastleSurgeryEngine::SutureWound(uint32 woundId, SurgicalTool sutureTool)
{
    std::lock_guard<std::recursive_mutex> lock(m_surgeryMutex);
    SurgicalActionResult res;
    m_totalProceduresConducted++;

    if (sutureTool != TOOL_MONOFILAMENT_SUTURE && sutureTool != TOOL_CYANOACRYLATE_SUPERGLUE)
    {
        res.success = false;
        res.message = "Improper closure material: Requires 20-lb nylon monofilament or cyanoacrylate superglue.";
        return res;
    }

    SpecificWound* w = sCastleTrauma.GetWound(woundId);
    if (!w)
    {
        res.success = false;
        res.message = "Wound not found.";
        return res;
    }

    if (w->hasLodgedBullet)
    {
        res.success = false;
        res.message = "Cannot suture wound: Foreign projectile must be extracted prior to fascial closure.";
        return res;
    }

    w->isSutured = true;
    res.bleedReductionMlPerSec = w->bleedRateMlPerSec * 0.75f;
    w->bleedRateMlPerSec -= res.bleedReductionMlPerSec;
    res.success = true;
    res.painSpikePercent = 50.0f;
    res.message = (sutureTool == TOOL_MONOFILAMENT_SUTURE)
        ? "Curved needle drove 20-lb nylon monofilament through torn muscle fascia. Wound margins secured."
        : "Cyanoacrylate medical glue bonded gaping laceration margins. Dermal layer sealed.";
    WithstandPainCheck(res.painSpikePercent);
    return res;
}

SurgicalActionResult CastleSurgeryEngine::CauterizeArterialBleeder(uint32 woundId, SurgicalTool cauteryTool)
{
    std::lock_guard<std::recursive_mutex> lock(m_surgeryMutex);
    SurgicalActionResult res;
    m_totalProceduresConducted++;

    if (cauteryTool != TOOL_GUNPOWDER_CAUTERIZE)
    {
        res.success = false;
        res.message = "Improper cautery agent: Requires gunpowder propellant flash cauterization.";
        return res;
    }

    SpecificWound* w = sCastleTrauma.GetWound(woundId);
    if (!w)
    {
        res.success = false;
        res.message = "Wound not found.";
        return res;
    }

    res.bleedReductionMlPerSec = w->bleedRateMlPerSec;
    w->bleedRateMlPerSec = 0.0f;
    w->isArterialBleeder = false;
    w->isCauterized = true;

    res.success = true;
    res.painSpikePercent = 95.0f; // Extreme searing pain
    res.message = "Tore open .45 ACP casing, poured propellant powder into arterial void, and ignited flash. Bleeder seared shut.";
    WithstandPainCheck(res.painSpikePercent);
    return res;
}

SurgicalActionResult CastleSurgeryEngine::PerformNeedleThoracostomy(SurgicalTool catheterTool)
{
    std::lock_guard<std::recursive_mutex> lock(m_surgeryMutex);
    SurgicalActionResult res;
    m_totalProceduresConducted++;

    if (catheterTool != TOOL_DECOMPRESSION_CATHETER)
    {
        res.success = false;
        res.message = "Improper tool: Needle decompression requires 14-gauge 3.25 inch catheter needle.";
        return res;
    }

    if (!sCastleTrauma.GetVitals().hasPneumothorax)
    {
        res.success = false;
        res.message = "Thoracic cavity normal: No tension pneumothorax detected.";
        return res;
    }

    sCastleTrauma.GetVitalsMutable().hasPneumothorax = false;
    res.success = true;
    res.painSpikePercent = 70.0f;
    res.message = "Drove 14-gauge catheter into 2nd intercostal space mid-clavicular line. Trapped pleural air hissed out. Lung re-expanded.";
    WithstandPainCheck(res.painSpikePercent);
    return res;
}

SurgicalActionResult CastleSurgeryEngine::ApplyPressureDressing(uint32 woundId, SurgicalTool bandageTool)
{
    std::lock_guard<std::recursive_mutex> lock(m_surgeryMutex);
    SurgicalActionResult res;
    m_totalProceduresConducted++;

    if (bandageTool != TOOL_PRESSURE_DRESSING)
    {
        res.success = false;
        res.message = "Requires hemostatic pressure bandage dressing.";
        return res;
    }

    SpecificWound* w = sCastleTrauma.GetWound(woundId);
    if (!w)
    {
        res.success = false;
        res.message = "Wound not found.";
        return res;
    }

    w->isBandaged = true;
    res.bleedReductionMlPerSec = w->bleedRateMlPerSec;
    w->bleedRateMlPerSec = 0.0f;
    res.success = true;
    res.painSpikePercent = 15.0f;
    res.message = "Packed kaolin hemostatic gauze tightly into cavity and wrapped tight with pressure bandage.";
    return res;
}

bool CastleSurgeryEngine::WithstandPainCheck(float painIntensity)
{
    // Frank Castle rejects morphine; jaw clenched on leather belt, Force Recon discipline
    return (painIntensity <= 100.0f);
}

void CastleSurgeryEngine::ConsumeBourbonAntiseptic(uint32 ounces)
{
    m_bourbonOuncesConsumed += ounces;
}

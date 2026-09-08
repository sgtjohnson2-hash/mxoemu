#ifndef MXOEMU_CASTLE_SURGERY_ENGINE_H
#define MXOEMU_CASTLE_SURGERY_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include "CastleTraumaModel.h"
#include <string>
#include <vector>
#include <mutex>

enum SurgicalTool
{
    TOOL_FORCEPS_PLIERS          = 1, // Bullet extraction
    TOOL_BOURBON_IRRIGATION      = 2, // Antiseptic flush
    TOOL_MONOFILAMENT_SUTURE     = 3, // 20-lb nylon closure
    TOOL_CYANOACRYLATE_SUPERGLUE = 4, // Flesh tear sealing
    TOOL_GUNPOWDER_CAUTERIZE     = 5, // Flash cauterization
    TOOL_DECOMPRESSION_CATHETER  = 6, // 14-gauge needle thoracostomy
    TOOL_PRESSURE_DRESSING       = 7  // Hemostatic bandage
};

struct SurgicalActionResult
{
    bool success{false};
    std::string message;
    float painSpikePercent{0.0f};
    float bleedReductionMlPerSec{0.0f};
};

class CastleSurgeryEngine : public Singleton<CastleSurgeryEngine>
{
public:
    CastleSurgeryEngine() = default;
    ~CastleSurgeryEngine() = default;

    // Surgical Procedures
    SurgicalActionResult ExtractLodgedSlug(uint32 woundId, SurgicalTool tool);
    SurgicalActionResult FlushAndDebrideWound(uint32 woundId, SurgicalTool antisepticTool);
    SurgicalActionResult SutureWound(uint32 woundId, SurgicalTool sutureTool);
    SurgicalActionResult CauterizeArterialBleeder(uint32 woundId, SurgicalTool cauteryTool);
    SurgicalActionResult PerformNeedleThoracostomy(SurgicalTool catheterTool); // Relieves tension pneumothorax
    SurgicalActionResult ApplyPressureDressing(uint32 woundId, SurgicalTool bandageTool);

    // Pain Endurance & Zero Narcotics
    bool WithstandPainCheck(float painIntensity);
    uint32 GetBourbonOuncesConsumed() const { return m_bourbonOuncesConsumed; }
    void ConsumeBourbonAntiseptic(uint32 ounces);
    uint32 GetTotalProceduresConducted() const { return m_totalProceduresConducted; }

private:
    mutable std::recursive_mutex m_surgeryMutex;
    uint32 m_bourbonOuncesConsumed{0};
    uint32 m_totalProceduresConducted{0};
};

#define sCastleSurgery CastleSurgeryEngine::getSingleton()

#endif // MXOEMU_CASTLE_SURGERY_ENGINE_H

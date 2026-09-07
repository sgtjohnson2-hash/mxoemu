#ifndef MXOEMU_OPERATOR_BRIDGE_H
#define MXOEMU_OPERATOR_BRIDGE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class PlayerObject;

enum OperatorActionType
{
    ACTION_HACK_CAMERA       = 1,
    ACTION_UPLOAD_BUFF       = 2,
    ACTION_TRACE_HARDLINE    = 3,
    ACTION_EMERGENCY_EXTRACT = 4
};

struct OperatorActionResult
{
    bool success;
    std::string message;
    std::string detailsJson;
};

class OperatorBridge : public Singleton<OperatorBridge>
{
public:
    OperatorBridge();
    ~OperatorBridge();

    void Initialize();

    // Tactical Operator Actions
    OperatorActionResult HackSurveillanceCamera(uint32 districtId, uint32 nodeId);
    OperatorActionResult UploadTacticalBuff(const std::string& playerHandle, const std::string& buffType);
    OperatorActionResult TraceNearestHardline(const std::string& playerHandle);
    OperatorActionResult EmergencyHardlineExtract(const std::string& playerHandle);

    // Shard Telemetry Export
    std::string ExportShardTelemetryJson() const;
    std::string ExportFactionNodesJson() const;
    std::string ExportEconomyStatusJson() const;
    std::string ExportContagionStatusJson() const;

    // GraphQL Query Resolution
    std::string ProcessGraphQLQuery(const std::string& query) const;

private:
    mutable std::mutex m_bridgeMutex;
    uint32 m_totalOperatorActions{0};
};

#define sOperatorBridge OperatorBridge::getSingleton()

void RunOperatorBridgeTestSuite();

#endif // MXOEMU_OPERATOR_BRIDGE_H

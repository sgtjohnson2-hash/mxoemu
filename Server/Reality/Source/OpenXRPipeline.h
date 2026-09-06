#ifndef MXOEMU_OPENXR_PIPELINE_H
#define MXOEMU_OPENXR_PIPELINE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <mutex>
#include <cmath>

enum VREye
{
    VR_EYE_LEFT = 0,
    VR_EYE_RIGHT = 1
};

enum MartialArtsGesture
{
    GESTURE_NONE = 0,
    GESTURE_PUNCH = 1,
    GESTURE_PALM_STRIKE = 2,
    GESTURE_FOREARM_BLOCK = 3,
    GESTURE_DEFLECTING_PARRY = 4
};

struct VRQuaternion
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};
};

struct VRVector3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    VRVector3 operator-(const VRVector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    VRVector3 operator+(const VRVector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    VRVector3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

struct Matrix4x4
{
    float m[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

struct VRPoseState
{
    VRVector3 position{0.0f, 175.0f, 0.0f}; // Default eye level in cm
    VRQuaternion orientation{0.0f, 0.0f, 0.0f, 1.0f};
    VRVector3 linearVelocity{0.0f, 0.0f, 0.0f};
    VRVector3 angularVelocity{0.0f, 0.0f, 0.0f};
    bool isTracked{true};
};

struct VRControllerState
{
    VRPoseState pose;
    float triggerValue{0.0f}; // 0.0 to 1.0
    float gripValue{0.0f};
    bool buttonA{false};
    bool buttonB{false};
    MartialArtsGesture detectedGesture{GESTURE_NONE};
};

struct EvasionStats
{
    uint32 totalProjectilesEncountered{0};
    uint32 successfulBulletDodges{0};
    float maxPhysicalLeanCm{0.0f};
    float cumulativeBulletTimeSlowMoSec{0.0f};
};

class OpenXRPipeline : public Singleton<OpenXRPipeline>
{
public:
    OpenXRPipeline();
    ~OpenXRPipeline();

    void Initialize();
    void Reset();

    // 6-DOF Tracking Updates
    void UpdateHeadPose(const VRVector3& pos, const VRQuaternion& rot, const VRVector3& velocity);
    void UpdateControllerPose(bool isRightHand, const VRVector3& pos, const VRQuaternion& rot,
                              const VRVector3& velocity, float trigger, float grip);

    const VRPoseState& GetHeadPose() const { return m_hmdPose; }
    const VRControllerState& GetController(bool isRightHand) const { return isRightHand ? m_rightHand : m_leftHand; }

    // Stereoscopic 3D Projection
    void SetIPD(float ipdMillimeters);
    float GetIPD() const { return m_ipdMm; }
    void SetFieldOfView(float horizontalFovDeg, float verticalFovDeg);
    Matrix4x4 ComputeEyeProjection(VREye eye, float nearZ = 10.0f, float farZ = 100000.0f) const;
    Matrix4x4 ComputeEyeViewMatrix(VREye eye) const;

    // Physical Bullet-Dodge Locomotion & Evasion Detection
    bool CheckBulletDodgeEvasion(const VRVector3& bulletPos, const VRVector3& bulletVelocity,
                                float bulletRadius, float& outSlowMoFactor, float& outLeanDisplacement);

    // Hand-Tracked Martial Arts Gestures
    MartialArtsGesture RecognizeCombatGesture(bool isRightHand);

    // Telemetry
    const EvasionStats& GetEvasionStats() const { return m_evasionStats; }
    bool IsOpenXRPipelineActive() const { return m_pipelineActive; }

private:
    mutable std::recursive_mutex m_vrMutex;
    bool m_pipelineActive{true};

    // Tracking
    VRPoseState m_hmdPose;
    VRControllerState m_leftHand;
    VRControllerState m_rightHand;
    VRVector3 m_virtualSpineOrigin{0.0f, 120.0f, 0.0f};

    // Optical parameters
    float m_ipdMm{63.5f}; // 63.5 mm human average
    float m_fovHorizontalDeg{110.0f};
    float m_fovVerticalDeg{90.0f};

    // Evasion Tracking
    EvasionStats m_evasionStats;
};

#define sOpenXRPipeline OpenXRPipeline::getSingleton()

#endif // MXOEMU_OPENXR_PIPELINE_H

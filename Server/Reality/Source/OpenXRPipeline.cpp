#include "OpenXRPipeline.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(OpenXRPipeline);

OpenXRPipeline::OpenXRPipeline()
{
    Initialize();
}

OpenXRPipeline::~OpenXRPipeline()
{
}

void OpenXRPipeline::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    m_pipelineActive = true;
    m_ipdMm = 63.5f;
    m_fovHorizontalDeg = 110.0f;
    m_fovVerticalDeg = 90.0f;

    m_hmdPose.position = {0.0f, 175.0f, 0.0f};
    m_hmdPose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    m_virtualSpineOrigin = {0.0f, 120.0f, 0.0f};

    m_leftHand.pose.position = {-25.0f, 130.0f, 30.0f};
    m_rightHand.pose.position = {25.0f, 130.0f, 30.0f};

    m_evasionStats = EvasionStats();

    if (Log::getSingletonPtr())
    {
        sLog.outString("[OpenXRPipeline] OpenXR stereoscopic VR pipeline initialized (IPD: %.1f mm, FOV: %.1f deg).",
                       m_ipdMm, m_fovHorizontalDeg);
    }
}

void OpenXRPipeline::Reset()
{
    Initialize();
}

void OpenXRPipeline::UpdateHeadPose(const VRVector3& pos, const VRQuaternion& rot, const VRVector3& velocity)
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    m_hmdPose.position = pos;
    m_hmdPose.orientation = rot;
    m_hmdPose.linearVelocity = velocity;

    // Track maximum physical lean relative to virtual spine
    VRVector3 leanVec = pos - m_virtualSpineOrigin;
    float leanDist = std::sqrt(leanVec.x * leanVec.x + leanVec.z * leanVec.z);
    if (leanDist > m_evasionStats.maxPhysicalLeanCm)
    {
        m_evasionStats.maxPhysicalLeanCm = leanDist;
    }
}

void OpenXRPipeline::UpdateControllerPose(bool isRightHand, const VRVector3& pos, const VRQuaternion& rot,
                                         const VRVector3& velocity, float trigger, float grip)
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    auto& c = isRightHand ? m_rightHand : m_leftHand;
    c.pose.position = pos;
    c.pose.orientation = rot;
    c.pose.linearVelocity = velocity;
    c.triggerValue = std::clamp(trigger, 0.0f, 1.0f);
    c.gripValue = std::clamp(grip, 0.0f, 1.0f);

    c.detectedGesture = RecognizeCombatGesture(isRightHand);
}

void OpenXRPipeline::SetIPD(float ipdMillimeters)
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    m_ipdMm = std::clamp(ipdMillimeters, 55.0f, 75.0f);
}

void OpenXRPipeline::SetFieldOfView(float horizontalFovDeg, float verticalFovDeg)
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    m_fovHorizontalDeg = std::clamp(horizontalFovDeg, 80.0f, 140.0f);
    m_fovVerticalDeg = std::clamp(verticalFovDeg, 70.0f, 120.0f);
}

Matrix4x4 OpenXRPipeline::ComputeEyeProjection(VREye eye, float nearZ, float farZ) const
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    Matrix4x4 proj;

    float fovRad = m_fovHorizontalDeg * (3.14159265f / 180.0f);
    float aspect = std::tan(fovRad * 0.5f);
    float tanHalfFov = std::max(0.001f, aspect);

    float f = 1.0f / tanHalfFov;
    proj.m[0] = f;
    proj.m[5] = f;
    proj.m[10] = (farZ + nearZ) / (nearZ - farZ);
    proj.m[11] = -1.0f;
    proj.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    proj.m[15] = 0.0f;

    return proj;
}

Matrix4x4 OpenXRPipeline::ComputeEyeViewMatrix(VREye eye) const
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    Matrix4x4 view;

    float eyeOffsetCm = (eye == VR_EYE_LEFT) ? -(m_ipdMm * 0.05f) : (m_ipdMm * 0.05f);

    view.m[12] = -(m_hmdPose.position.x + eyeOffsetCm);
    view.m[13] = -m_hmdPose.position.y;
    view.m[14] = -m_hmdPose.position.z;

    return view;
}

bool OpenXRPipeline::CheckBulletDodgeEvasion(const VRVector3& bulletPos, const VRVector3& bulletVelocity,
                                             float bulletRadius, float& outSlowMoFactor, float& outLeanDisplacement)
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    m_evasionStats.totalProjectilesEncountered++;

    outSlowMoFactor = 1.0f;
    outLeanDisplacement = 0.0f;

    // Lean displacement from neutral spine center
    VRVector3 leanVec = m_hmdPose.position - m_virtualSpineOrigin;
    float leanDist = std::sqrt(leanVec.x * leanVec.x + leanVec.z * leanVec.z);
    outLeanDisplacement = leanDist;

    // Check distance between bullet and current head pose vs neutral spine/head line
    float distToCurrentHead = (bulletPos - m_hmdPose.position).Length();
    VRVector3 neutralHeadPos{m_virtualSpineOrigin.x, m_hmdPose.position.y, m_virtualSpineOrigin.z};
    float distToNeutralHead = (bulletPos - neutralHeadPos).Length();
    float distToSpineAxis = std::sqrt((bulletPos.x - m_virtualSpineOrigin.x) * (bulletPos.x - m_virtualSpineOrigin.x) +
                                      (bulletPos.z - m_virtualSpineOrigin.z) * (bulletPos.z - m_virtualSpineOrigin.z));
    bool heightInRange = (bulletPos.y >= (m_virtualSpineOrigin.y - 30.0f) && bulletPos.y <= (m_virtualSpineOrigin.y + 75.0f));

    // Player physical dodge detection criteria:
    // 1. Bullet passed through where the head/torso would have been (near spine line < 45cm)
    // 2. Physical head displacement is > 25.0 cm
    // 3. Current head is safely outside bullet collision radius
    if ((distToNeutralHead < 45.0f || (distToSpineAxis < 45.0f && heightInRange)) &&
        leanDist > 25.0f && distToCurrentHead > (bulletRadius + 15.0f))
    {
        m_evasionStats.successfulBulletDodges++;
        m_evasionStats.cumulativeBulletTimeSlowMoSec += 2.0f;
        outSlowMoFactor = 0.20f; // Bullet-Time triggered

        sLog.outString("[OpenXRPipeline] PHYSICAL BULLET DODGE DETECTED! Lean displacement: %.1f cm (Slow-Mo factor: %.2f)",
                       leanDist, outSlowMoFactor);
        return true;
    }

    return false;
}

MartialArtsGesture OpenXRPipeline::RecognizeCombatGesture(bool isRightHand)
{
    std::lock_guard<std::recursive_mutex> lock(m_vrMutex);
    const auto& hand = isRightHand ? m_rightHand : m_leftHand;
    const auto& otherHand = isRightHand ? m_leftHand : m_rightHand;

    // Check forearm cross block: both hands within 30cm of each other, in front of upper chest
    float handDistance = (hand.pose.position - otherHand.pose.position).Length();
    if (handDistance < 30.0f && hand.pose.position.y > 135.0f)
    {
        return GESTURE_FOREARM_BLOCK;
    }

    float forwardSpeed = std::abs(hand.pose.linearVelocity.z);
    float lateralSpeed = std::abs(hand.pose.linearVelocity.x);

    // Punch: high forward linear speed with closed grip
    if (forwardSpeed > 220.0f && hand.gripValue > 0.6f)
    {
        return GESTURE_PUNCH;
    }

    // Palm strike: high forward linear speed with open hand
    if (forwardSpeed > 180.0f && hand.gripValue < 0.3f)
    {
        return GESTURE_PALM_STRIKE;
    }

    // Deflecting parry: rapid lateral sweep across centerline
    if (lateralSpeed > 180.0f)
    {
        return GESTURE_DEFLECTING_PARRY;
    }

    return GESTURE_NONE;
}

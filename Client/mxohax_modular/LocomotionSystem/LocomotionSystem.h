#pragma once

#include "../Common/Common.h"
#include "../Common/Subsystem.h"
#include "CollisionMesh.h"

// ============================================================================
// Shared Locomotion & Physics State
// ============================================================================

extern double g_playerX;
extern double g_playerY;
extern double g_playerZ;
extern float  g_playerYaw;
extern double g_velY;
extern bool   g_isJumping;
extern double g_jumpVelX;
extern double g_jumpVelZ;
extern bool   g_hasDoubleJumped;

extern float  g_moveAnimBlend;
extern float  g_combatStrikeTimer;

extern bool   g_isWallRunning;
extern float  g_wallRunDuration;
extern float  g_wallRunCameraTilt;

extern bool   g_focusModeActive;
extern float  g_timeDilation;
extern bool   g_codeRainDegradationActive;

// ============================================================================
// Native LithTech Raycasting & Locomotion Interface
// ============================================================================

// Native LithTech Jupiter collision raycasting probing g_pLTClient at clientBase + 0x00897FA4
// with SEH protection and fallback to high-precision polygonal terrain mesh
bool CastWorldRay(double posX, double startY, double posZ, double maxDownDist, CollisionRaycastHit& outHit);

// Kinematic movement and 18.0-unit stair/curb stepping update
void UpdatePlayerPositionAndPhysics(uintptr_t clientBase, void* curPlayer, double dt, bool isMoving, double actVelX, double actVelZ);

void SyncActorPosition(uintptr_t clientBase, void* pPlayer, void* pActor, double x, double y, double z);
void ApplyOperativeAppearance(uintptr_t clientBase, void* pPlayer, void* pActor);
void UpdateCamera(uintptr_t clientBase, void* pCam);

class LocomotionSystemSubsystem : public IClientSubsystem {
public:
    const char* GetName() const override { return "LocomotionSystem"; }
    bool Initialize(uintptr_t clientBase) override;
    void Shutdown() override;
    void Update(float dt) override;
};

extern LocomotionSystemSubsystem g_LocomotionSystem;

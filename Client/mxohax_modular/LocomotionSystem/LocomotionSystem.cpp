#include "LocomotionSystem.h"
#include "../InputManager/InputManager.h"
#include <cmath>

// ============================================================================
// Shared Locomotion & Physics State Definitions
// ============================================================================

double g_playerX = 16710.0;
double g_playerY = SPAWN_GROUND_ELEVATION; // 603.5
double g_playerZ = 3230.0;
float  g_playerYaw = 0.0f;
double g_velY = 0.0;
bool   g_isJumping = false;
double g_jumpVelX = 0.0;
double g_jumpVelZ = 0.0;
bool   g_hasDoubleJumped = false;

float  g_moveAnimBlend = 0.0f;
float  g_combatStrikeTimer = 0.0f;

bool   g_isWallRunning = false;
float  g_wallRunDuration = 0.0f;
float  g_wallRunCameraTilt = 0.0f;

bool   g_focusModeActive = false;
float  g_timeDilation = 1.0f;
bool   g_codeRainDegradationActive = false;

LocomotionSystemSubsystem g_LocomotionSystem;

// ============================================================================
// Native LithTech Raycasting Interface
// ============================================================================

bool CastWorldRay(double posX, double startY, double posZ, double maxDownDist, CollisionRaycastHit& outHit) {
    outHit.bHit = false;
    outHit.hitY = SPAWN_GROUND_ELEVATION;
    outHit.normalX = 0.0;
    outHit.normalY = 1.0;
    outHit.normalZ = 0.0;
    outHit.surfaceFlags = 0;

    uintptr_t clientBase = GetSafeClientBase();
    bool bEngineHit = false;

    // Probe native LithTech Jupiter client engine pointer at clientBase + 0x00897FA4 (g_pLTClient)
    if (clientBase) {
        __try {
            void** ppLTClient = reinterpret_cast<void**>(clientBase + 0x00897FA4);
            if (ppLTClient && !IsBadReadPtr(ppLTClient, sizeof(void*)) && *ppLTClient) {
                void* pLTClient = *ppLTClient;
                if (!IsBadReadPtr(pLTClient, 0x100)) {
                    void** vtbl = *reinterpret_cast<void***>(pLTClient);
                    if (vtbl && !IsBadReadPtr(vtbl, 0x100)) {
                        // ILTClient::IntersectSegment (VTable slot 7 / offset 0x1C)
                        typedef BOOL (__thiscall *IntersectSegment_t)(void* pThis, const float* pStart, const float* pEnd, void* pInfo);
                        IntersectSegment_t pIntersect = reinterpret_cast<IntersectSegment_t>(vtbl[7]);
                        if (pIntersect) {
                            float start[3] = { (float)posX, (float)startY, (float)posZ };
                            float end[3]   = { (float)posX, (float)(startY - maxDownDist), (float)posZ };
                            float info[16] = { 0 }; // LithTech IntersectInfo buffer

                            BOOL res = pIntersect(pLTClient, start, end, info);
                            if (res && info[1] >= 400.0f && info[1] <= 1200.0f) {
                                outHit.bHit = true;
                                outHit.hitY = (double)info[1];
                                outHit.normalX = (double)info[3];
                                outHit.normalY = (double)info[4];
                                outHit.normalZ = (double)info[5];
                                outHit.surfaceFlags = *reinterpret_cast<DWORD*>(&info[8]);
                                bEngineHit = true;
                            }
                        }
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            bEngineHit = false;
        }
    }

    if (bEngineHit) {
        return true;
    }

    // Robust mathematical fallback to high-precision continuous polygonal terrain mesh
    return QueryPolygonalTerrainMesh(posX, startY, posZ, maxDownDist, outHit);
}

// ============================================================================
// Kinematics and 18.0-Unit Stair/Curb Stepping Update
// ============================================================================

void UpdatePlayerPositionAndPhysics(uintptr_t clientBase, void* curPlayer, double dt, bool isMoving, double actVelX, double actVelZ) {
    if (!curPlayer || IsBadReadPtr(curPlayer, 0xB0)) return;

    // Downward continuous collision raycast from hip/torso origin (g_playerY + 36.0)
    CollisionRaycastHit groundHit;
    double probeStartY = (g_playerY > 500.0 && g_playerY < 1200.0) ? (g_playerY + 36.0) : 750.0;
    double groundElev = SPAWN_GROUND_ELEVATION;

    if (CastWorldRay(g_playerX, probeStartY, g_playerZ, 120.0, groundHit)) {
        groundElev = groundHit.hitY;
    } else {
        groundElev = GetCalibratedGroundElevation(g_playerX, g_playerZ);
    }

    // Wall-Running along skyscraper facades & elevated platform barrier surfaces
    // Facades flank the platform along X: 16560 (West) and X: 16830-16845 (East)
    bool nearWestFacade = (g_playerX <= 16575.0 && g_playerX >= 16550.0 && g_playerZ >= 2850.0 && g_playerZ <= 3720.0);
    bool nearEastFacade = (g_playerX >= 16825.0 && g_playerX <= 16848.0 && g_playerZ >= 2850.0 && g_playerZ <= 3720.0);
    bool onFacade = (nearWestFacade || nearEastFacade) && (g_playerY > 580.0) && g_isJumping;

    if (onFacade && isMoving && g_wallRunDuration < 2.5f) {
        g_isWallRunning = true;
        g_wallRunDuration += (float)dt;
        // Glide falling rate clamped to slow horizontal wall-glide
        g_velY = -40.0;
        g_playerY += g_velY * dt;
        // Wall-glide movement along facade tangent
        double wallSpeed = (actVelZ != 0.0) ? actVelZ : (actVelX != 0.0 ? actVelX : 340.0);
        g_playerZ += wallSpeed * dt;
        g_wallRunCameraTilt = nearWestFacade ? -0.08f : 0.08f;
    } else {
        g_isWallRunning = false;
        g_wallRunCameraTilt = 0.0f;
        if (!g_isJumping) g_wallRunDuration = 0.0f;
    }

    // Apply jumping physics, horizontal momentum preservation, and gravity
    if (g_isJumping && !g_isWallRunning) {
        g_playerY += g_velY * dt;
        g_velY -= 950.0 * dt;

        // Apply preserved horizontal momentum from wire-fu launch
        g_playerX += g_jumpVelX * dt;
        g_playerZ += g_jumpVelZ * dt;
        g_jumpVelX *= 0.985;
        g_jumpVelZ *= 0.985;

        if (g_playerY <= groundElev) {
            g_playerY = groundElev;
            g_velY = 0.0;
            g_jumpVelX = 0.0;
            g_jumpVelZ = 0.0;
            g_isJumping = false;
            g_hasDoubleJumped = false;
            g_wallRunDuration = 0.0f;
        }
    } else if (!g_isWallRunning) {
        // Continuous 18.0-unit stair/curb stepping algorithm (Requirement R2)
        // Maintains surface contact within +/- 0.5 units without floating or clipping
        if (groundElev > g_playerY) {
            double stepUp = groundElev - g_playerY;
            if (stepUp <= 18.0) {
                // Elevate smoothly to step/curb surface, maintaining +/- 0.5 units contact
                g_playerY = groundElev;
            } else {
                // Obstacle too high (barrier/wall > 18.0 units): block upward climb
                // Maintain current elevation
            }
        } else if (g_playerY > groundElev) {
            double drop = g_playerY - groundElev;
            if (drop <= 18.0) {
                // Instant flush contact across curb/step drop (+/- 0.5 units, zero air floating)
                g_playerY = groundElev;
            } else {
                // Vertical drop exceeding 18.0 units: fall under gravity
                g_playerY -= 1200.0 * dt;
                if (g_playerY <= groundElev) {
                    g_playerY = groundElev;
                }
            }
        } else {
            g_playerY = groundElev;
        }
    }

    // Continuous unconstrained district roaming: zero hardcoded boundary snapping
    if (isnan(g_playerX) || isinf(g_playerX)) g_playerX = 16710.0;
    if (isnan(g_playerY) || isinf(g_playerY)) g_playerY = SPAWN_GROUND_ELEVATION;
    if (isnan(g_playerZ) || isinf(g_playerZ)) g_playerZ = 3230.0;

    // 1. Update player float position buffer
    float* pPos = *reinterpret_cast<float**>(reinterpret_cast<uintptr_t>(curPlayer) + 0x94);
    if (pPos) {
        pPos[0] = (float)g_playerX;
        pPos[1] = (float)g_playerY;
        pPos[2] = (float)g_playerZ;
        pPos[3] = 1.0f;
        pPos[4] = 0.0f;
        pPos[5] = 1.0f;
        pPos[15] = 1.0f;
    }

    // 2. Update player rotation quaternion (around Y axis)
    float playerQuat[4] = {
        0.0f,
        sinf(g_playerYaw * 0.5f),
        0.0f,
        cosf(g_playerYaw * 0.5f)
    };
    float* pRot = *reinterpret_cast<float**>(reinterpret_cast<uintptr_t>(curPlayer) + 0x98);
    if (pRot && !IsBadReadPtr(pRot, 16)) {
        memcpy(pRot, playerQuat, sizeof(playerQuat));
    }

    // 3. Update pActor 3D scene transform and queue position sample
    void* pActor = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(curPlayer) + 0xA8);
    if (pActor && !IsBadReadPtr(pActor, 0x690)) {
        // Double precision positions on CActor
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x528) = g_playerX;
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x530) = g_playerY;
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x538) = g_playerZ;

        float animWeight = (g_combatStrikeTimer > 0.0f) ? 1.0f : g_moveAnimBlend;
        bool isStationaryIdle = (g_combatStrikeTimer <= 0.0f && g_moveAnimBlend <= 0.05f && !isMoving && !g_isJumping);

        // Double precision velocities on CActor
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x510) = isMoving ? actVelX * (double)g_moveAnimBlend : 0.0;
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x518) = g_isJumping ? g_velY : 0.0;
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x520) = isMoving ? actVelZ * (double)g_moveAnimBlend : 0.0;

        // Locomotion stopped/idle flag (+0x4EE): 1 = stopped/idle, 0 = moving
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = isStationaryIdle ? 1 : 0;
        *reinterpret_cast<WORD*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = isStationaryIdle ? 1 : 0;

        float* pActorRot = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x4FC);
        if (pActorRot && !IsBadReadPtr(pActorRot, 16)) {
            memcpy(pActorRot, playerQuat, sizeof(playerQuat));
        }

        // Prepare 128-byte position sample for the movement interpolation queue at [pActor + 0x5F0]
        BYTE sample[128] = {0};
        *reinterpret_cast<DWORD*>(sample + 0x00) = GetTickCount();
        *reinterpret_cast<double*>(sample + 0x08) = g_playerX;
        *reinterpret_cast<double*>(sample + 0x10) = g_playerY;
        *reinterpret_cast<double*>(sample + 0x18) = g_playerZ;
        *reinterpret_cast<float*>(sample + 0x20) = playerQuat[0];
        *reinterpret_cast<float*>(sample + 0x24) = playerQuat[1];
        *reinterpret_cast<float*>(sample + 0x28) = playerQuat[2];
        *reinterpret_cast<float*>(sample + 0x2C) = playerQuat[3];

        typedef void (__thiscall *AddPosSample_t)(void* pActor, const void* pSample);
        AddPosSample_t pAddSample = reinterpret_cast<AddPosSample_t>(clientBase + 0x004F3920);
        __try {
            pAddSample(pActor, sample);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        // Recalculate spatial extents now that [pActor + 0x528] has been set
        typedef void (__thiscall *CalcExtents_t)(void* pActor);
        CalcExtents_t pCalcExtents = reinterpret_cast<CalcExtents_t>(clientBase + 0x004E9BF0);
        __try {
            pCalcExtents(pActor);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        // Maintain visibility flags
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x374) = 0; // Local player visible
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x375) = 1;
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x2DD) = 0; // Not culled
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x684) = 0; // In-world
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x385) = 3; // Scene transform valid
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x290) = 0;
        *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = animWeight;
    }
}

// ============================================================================
// CActor Coordinates & Appearance Helpers
// ============================================================================

void SyncActorPosition(uintptr_t clientBase, void* pPlayer, void* pActor, double x, double y, double z) {
    if (!pActor || IsBadReadPtr(pActor, 0x40)) return;

    if (pPlayer && !IsBadReadPtr(pPlayer, 0xB0)) {
        float* pRot = *reinterpret_cast<float**>(reinterpret_cast<uintptr_t>(pPlayer) + 0x98);
        if (!pRot) {
            pRot = reinterpret_cast<float*>(calloc(4, sizeof(float)));
            if (pRot) {
                pRot[0] = 0.0f; pRot[1] = 0.0f; pRot[2] = 0.0f; pRot[3] = 1.0f;
                *reinterpret_cast<float**>(reinterpret_cast<uintptr_t>(pPlayer) + 0x98) = pRot;
            }
        }
        *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x28) = pPlayer;
        if (!IsBadReadPtr(pPlayer, 0xC8)) {
            void* pRSI = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xAC);
            void* pInv = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xA4);
            void* pSim = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xC4);
            if (pRSI) *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x298) = pRSI;
            if (pInv) *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x29C) = pInv;
            if (pSim) *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x2A4) = pSim;
            *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x2C) = 1;
        }
    }

    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x528) = x;
    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x530) = y;
    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x538) = z;
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 1;
    *reinterpret_cast<WORD*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 1;

    float* pActorRot = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x4FC);
    if (pActorRot && !IsBadReadPtr(pActorRot, 16)) {
        pActorRot[0] = 0.0f;
        pActorRot[1] = 0.0f;
        pActorRot[2] = 0.0f;
        pActorRot[3] = 1.0f;
    }

    BYTE initSample[128] = {0};
    *reinterpret_cast<DWORD*>(initSample + 0x00) = GetTickCount();
    *reinterpret_cast<double*>(initSample + 0x08) = x;
    *reinterpret_cast<double*>(initSample + 0x10) = y;
    *reinterpret_cast<double*>(initSample + 0x18) = z;
    *reinterpret_cast<float*>(initSample + 0x20) = 0.0f;
    *reinterpret_cast<float*>(initSample + 0x24) = 0.0f;
    *reinterpret_cast<float*>(initSample + 0x28) = 0.0f;
    *reinterpret_cast<float*>(initSample + 0x2C) = 1.0f;
    typedef void (__thiscall *AddPosSample_t)(void* pActor, const void* pSample);
    AddPosSample_t pAddSample = reinterpret_cast<AddPosSample_t>(clientBase + 0x004F3920);
    __try {
        pAddSample(pActor, initSample);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    typedef void (__thiscall *CalcExtents_t)(void* pActor);
    CalcExtents_t pCalcExtents = reinterpret_cast<CalcExtents_t>(clientBase + 0x004E9BF0);
    __try {
        pCalcExtents(pActor);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x374) = 0; // Local player visible
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x375) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x2DD) = 0; // Not culled
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x684) = 0; // In world
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x385) = 3; // Scene transform valid
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x290) = 0;
    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = 0.0f;
}

void ApplyOperativeAppearance(uintptr_t clientBase, void* pPlayer, void* /*pActor*/) {
    if (!pPlayer || IsBadReadPtr(pPlayer, 0xB0)) return;
    void* pRSI = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xAC);
    if (!pRSI || IsBadReadPtr(pRSI, 0xB0)) {
        Log("[mxohax] ApplyOperativeAppearance: pRSI is null or invalid\n");
        return;
    }

    static void* s_lastPlayer = nullptr;
    static bool s_applied = false;
    if (s_lastPlayer != pPlayer) {
        s_lastPlayer = pPlayer;
        s_applied = false;
    }
    if (s_applied) return;
    s_applied = true;

    Log("[mxohax] Applying Operative RSI appearance (pPlayer=0x%p, pRSI=0x%p)...\n", pPlayer, pRSI);
    __try {
        typedef void (__thiscall *SetBodyType_t)(void* pRSI, int val);
        typedef void (__thiscall *SetHeadType_t)(void* pRSI, int val);
        typedef void (__thiscall *SetHairType_t)(void* pRSI, int val);
        typedef void (__thiscall *SetHat_t)(void* pRSI, int val);
        typedef void (__thiscall *EquipArticle_t)(void* pRSI, int slot, int articleId, int color);
        typedef void (__thiscall *RebuildRSI_t)(void* pRSI);

        SetBodyType_t pSetBody = reinterpret_cast<SetBodyType_t>(clientBase + 0x0051B490);
        SetHeadType_t pSetHead = reinterpret_cast<SetHeadType_t>(clientBase + 0x0051B4F0);
        SetHairType_t pSetHair = reinterpret_cast<SetHairType_t>(clientBase + 0x0051B4B0);
        SetHat_t pSetHat = reinterpret_cast<SetHat_t>(clientBase + 0x0051B4D0);
        EquipArticle_t pEquip = reinterpret_cast<EquipArticle_t>(clientBase + 0x0051B1E0);
        RebuildRSI_t pRebuild = reinterpret_cast<RebuildRSI_t>(clientBase + 0x0051B370);

        uint16_t maleBody = 106;  // RSIMBody001 Male Body
        uint16_t maleHead = 103;  // RSIMHead001 Male Head
        uint16_t maleHair = 105;  // RSIMHair001 Male Hair
        uint16_t maleShirt = 105; // RSIMShirt001 Male Shirt
        uint16_t maleCoat = 120;  // RSIMCoat001 Black Trenchcoat
        uint16_t malePants = 102; // RSIMPants001 Jeans
        uint16_t maleShoes = 111; // RSIMShoes001 Boots
        uint16_t maleGloves = 107;// RSIMGloves001 Gloves
        uint16_t maleGlasses = 109;// RSIMGlasses001 Sunglasses

        *reinterpret_cast<short*>(clientBase + 0x0089E37C) = maleBody;
        *reinterpret_cast<short*>(clientBase + 0x0089E3B4) = maleHead;
        *reinterpret_cast<short*>(clientBase + 0x0089E3EC) = maleHair;
        *reinterpret_cast<short*>(clientBase + 0x0089E424) = 0;

        struct OperativeItemDef {
            DWORD slot;
            DWORD articleId;
            BYTE color;
        };
        const OperativeItemDef items[6] = {
            { 2, maleShirt,   41 },
            { 3, maleCoat,    0  },
            { 4, malePants,   16 },
            { 5, maleShoes,   0  },
            { 6, maleGloves,  0  },
            { 7, maleGlasses, 15 }
        };
        for (int i = 0; i < 6; ++i) {
            uintptr_t entry = clientBase + 0x0089E760 + (i * 0x74);
            *reinterpret_cast<DWORD*>(entry) = items[i].slot;
            *reinterpret_cast<DWORD*>(entry + 0x34) = items[i].articleId;
            *reinterpret_cast<BYTE*>(entry + 0x6C) = items[i].color;
        }

        pSetBody(pRSI, maleBody);
        pSetHead(pRSI, maleHead);
        pSetHair(pRSI, maleHair);
        pSetHat(pRSI, 0);

        pEquip(pRSI, 0, 0, 0);
        pEquip(pRSI, 2, maleShirt,   41);
        pEquip(pRSI, 3, maleCoat,    0);
        pEquip(pRSI, 4, malePants,   16);
        pEquip(pRSI, 5, maleShoes,   0);
        pEquip(pRSI, 6, maleGloves,  0);
        pEquip(pRSI, 7, maleGlasses, 15);

        if (pRSI && !IsBadReadPtr(pRSI, 0xC0)) {
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x90) = maleBody;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x92) = 0;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x94) = maleHead;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x96) = maleShirt;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x98) = maleCoat;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x9A) = malePants;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x9C) = maleShoes;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x9E) = maleGloves;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA0) = maleGlasses;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA2) = maleHair;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA4) = 0;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA6) = 0;

            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA8) = 41;
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA9) = 16;
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAA) = 0;
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAB) = 0;
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAC) = 15;
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAD) = 0;
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAE) = 0;
        }

        pRebuild(pRSI);

        void* pActor = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xA8);
        if (pActor && !IsBadReadPtr(pActor, 0x40)) {
            static const uint8_t s_s1ackerBitstream[16] = {
                0x00, 0x00, 0x22, 0x82, 0x31, 0x88, 0x10, 0xa6, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };

            void* p1DC = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1DC);
            if (!p1DC) {
                static void* s_dummy1DC[4] = { nullptr };
                *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1DC) = s_dummy1DC;
            }

            void* p14C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x14C);
            if (!p14C) {
                static DWORD s_dummyObj = 0;
                static void* s_dummy14C[4] = { &s_dummyObj, nullptr, nullptr, nullptr };
                *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x14C) = s_dummy14C;
            }

            void* pActorMesh = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1E4);
            if (!pActorMesh) {
                void* pRSIMesh = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pRSI) + 0x68);
                if (pRSIMesh) {
                    *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1E4) = pRSIMesh;
                    pActorMesh = pRSIMesh;
                }
            }

            typedef void (__thiscall *SetActorAppearance_t)(void* pActor, const void* pBitstream);
            SetActorAppearance_t pSetActorApp = reinterpret_cast<SetActorAppearance_t>(clientBase + 0x004F2F60);
            pSetActorApp(pActor, s_s1ackerBitstream);

            if (pActorMesh && !IsBadReadPtr(pActorMesh, 8)) {
                void* pEdx = *reinterpret_cast<void**>(pActorMesh);
                void* arg1 = pEdx ? *reinterpret_cast<void**>(pEdx) : nullptr;
                void* pThis = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActorMesh) + 4);
                if (pThis && !IsBadReadPtr(pThis, 0x30)) {
                    typedef void (__thiscall *AttachMesh_t)(void* pThis, void* arg1, const void* pBitstream);
                    AttachMesh_t pAttachMesh = reinterpret_cast<AttachMesh_t>(clientBase + 0x002592E0);
                    pAttachMesh(pThis, arg1, s_s1ackerBitstream);
                }
            }
        }

        Log("[mxohax] SUCCESS: Operative RSI fully equipped (Male Body %u, Head %u, Hair %u, Coat %u, Shirt %u, Pants %u, Shoes %u, Glasses %u)!\n",
            maleBody, maleHead, maleHair, maleCoat, maleShirt, malePants, maleShoes, maleGlasses);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception in ApplyOperativeAppearance caught safely!\n");
    }
}

void UpdateCamera(uintptr_t clientBase, void* pCam) {
    if (!pCam || IsBadReadPtr(pCam, 0x110)) return;

    double* pTargetPosC8 = reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pCam) + 0xC8);
    if (pTargetPosC8) {
        pTargetPosC8[0] = g_playerX;
        pTargetPosC8[1] = g_playerY + 54.0;
        pTargetPosC8[2] = g_playerZ;
    }
    double* pCamPos8 = reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pCam) + 8);
    if (pCamPos8) {
        pCamPos8[0] = g_playerX;
        pCamPos8[1] = g_playerY + 54.0;
        pCamPos8[2] = g_playerZ;
    }

    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pCam) + 0x30) = g_camPitch;
    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pCam) + 0x34) = g_camYaw;
    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pCam) + 0x38) = g_camDist;

    double camFwdX = sinf(g_camYaw) * cosf(g_camPitch);
    double camFwdY = -sinf(g_camPitch);
    double camFwdZ = cosf(g_camYaw) * cosf(g_camPitch);

    double camX = g_playerX - camFwdX * g_camDist;
    double camY = g_playerY + 54.0 - camFwdY * g_camDist;
    double camZ = g_playerZ - camFwdZ * g_camDist;

    float sp = sinf(g_camPitch * 0.5f);
    float cp = cosf(g_camPitch * 0.5f);
    float sy = sinf(g_camYaw * 0.5f);
    float cy = cosf(g_camYaw * 0.5f);

    float qx = cy * sp;
    float qy = sy * cp;
    float qz = -sy * sp;
    float qw = cy * cp;

    if (fabsf(g_wallRunCameraTilt) > 0.001f) {
        float sr = sinf(g_wallRunCameraTilt * 0.5f);
        float cr = cosf(g_wallRunCameraTilt * 0.5f);
        float nx = qx * cr + qy * sr;
        float ny = qy * cr - qx * sr;
        float nz = qw * sr + qz * cr;
        float nw = qw * cr - qz * sr;
        qx = nx; qy = ny; qz = nz; qw = nw;
    }

    float camRotQ[4] = { qx, qy, qz, qw };

    void* pEngineCam = *reinterpret_cast<void**>(pCam);
    if (pEngineCam && !IsBadReadPtr(pEngineCam, 4)) {
        void** pCamVtbl = *reinterpret_cast<void***>(pEngineCam);
        if (pCamVtbl && !IsBadReadPtr(pCamVtbl, 0x40)) {
            typedef void (__thiscall *SetPosFn)(void* pThis, const double* pPos);
            SetPosFn pSetPos = reinterpret_cast<SetPosFn>(pCamVtbl[0x24 / 4]);
            double finalCamPos[3] = { camX, camY, camZ };
            pSetPos(pEngineCam, finalCamPos);

            typedef void (__thiscall *SetRotFn)(void* pThis, const float* pRot);
            SetRotFn pSetRot = reinterpret_cast<SetRotFn>(pCamVtbl[0x30 / 4]);
            pSetRot(pEngineCam, camRotQ);
        }
    }
}

// ============================================================================
// Locomotion Subsystem Implementation
// ============================================================================

bool LocomotionSystemSubsystem::Initialize(uintptr_t clientBase) {
    Log("[mxohax] LocomotionSystemSubsystem::Initialize\n");
    if (!clientBase) return false;

    // Neutralize CActor posture zeroing routines at client.dll + 0x001A4D25, +0x004ECC32, +0x0019DFA0 with NOPs
    static const DWORD patchRVAs[] = { 0x001A4D25, 0x004ECC32, 0x0019DFA0 };
    for (DWORD rva : patchRVAs) {
        LPVOID addr = reinterpret_cast<LPVOID>(clientBase + rva);
        DWORD oldProt = 0;
        if (VirtualProtect(addr, 7, PAGE_EXECUTE_READWRITE, &oldProt)) {
            memset(addr, 0x90, 7); // 7 NOPs
            VirtualProtect(addr, 7, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), addr, 7);
        }
    }

    return true;
}

void LocomotionSystemSubsystem::Shutdown() {
    Log("[mxohax] LocomotionSystemSubsystem::Shutdown\n");
}

void LocomotionSystemSubsystem::Update(float /*dt*/) {
    // Locomotion update is executed synchronously within DetourFrameTick
}

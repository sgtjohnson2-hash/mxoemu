#include "Common/Common.h"
#include "Common/CrashHandler.h"
#include "Common/Subsystem.h"
#include "D3D9Hook/D3D9Hook.h"
#include "InputManager/InputManager.h"
#include "UIAnchorSystem/UIAnchorSystem.h"
#include "LocomotionSystem/LocomotionSystem.h"
#include "ProtocolHook/ProtocolHook.h"

#include <cmath>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// FrameTick function pointer
typedef void (__thiscall *FrameTick_t)(void* pThis);
FrameTick_t OriginalFrameTick = nullptr;

static int s_tickCount = 0;

static void EnsureInWorldRendering(uintptr_t clientBase, void* pWorldMgr, DWORD pShell, bool promoteToState3 = true) {
    if (!pWorldMgr) return;

    static const char s_defaultMetrPath[] = "resource/worlds/final_world/slums_barrens_full.metr";
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_defaultMetrPath;

    void** ppWorldInst = reinterpret_cast<void**>(clientBase + 0x0089DD6C);
    static bool s_worldLoadedOnce = false;
    if (!s_worldLoadedOnce || (ppWorldInst && !*ppWorldInst)) {
        s_worldLoadedOnce = true;
        Log("[mxohax] EnsureInWorld: Calling CWorldMgr::LoadWorldFile (0x10121110) for %s...\n", s_defaultMetrPath);
        typedef char (__thiscall *LoadWorldFile_t)(void* pMgr);
        LoadWorldFile_t pLoadWorld = reinterpret_cast<LoadWorldFile_t>(clientBase + 0x00121110);
        char lres = pLoadWorld(pWorldMgr);
        BYTE* pWorldLoaded = reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27);
        Log("[mxohax] EnsureInWorld: LoadWorldFile returned %d (worldLoaded=%d, [0x1089DD6C]=0x%p)\n",
            lres, pWorldLoaded ? *pWorldLoaded : 0, ppWorldInst ? *ppWorldInst : nullptr);
    }

    void** ppPlayerGlobal = reinterpret_cast<void**>(clientBase + 0x008A4378);
    void* pPlayer = *ppPlayerGlobal;
    if (!pPlayer) {
        Log("[mxohax] EnsureInWorld: Player is null! Calling native CreateObject(0x0C) to create 3D character...\n");
        typedef void* (__cdecl *GetModelDef_t)(int a, int b);
        GetModelDef_t pGetModelDef = reinterpret_cast<GetModelDef_t>(clientBase + 0x001D27C0);
        pGetModelDef(1, 0);

        *reinterpret_cast<float*>(clientBase + 0x008BA5E8) = 16710.0f;
        *reinterpret_cast<float*>(clientBase + 0x008BA5EC) = (float)SPAWN_GROUND_ELEVATION;
        *reinterpret_cast<float*>(clientBase + 0x008BA5F0) = 3230.0f;
        *reinterpret_cast<float*>(clientBase + 0x008BA5F4) = 0.0f;
        *reinterpret_cast<WORD*>(clientBase + 0x008BA5F8) = 0;

        typedef void* (__cdecl *AllocPlayer_t)();
        AllocPlayer_t pAlloc = reinterpret_cast<AllocPlayer_t>(clientBase + 0x001D2370);
        pPlayer = pAlloc();
        Log("[mxohax] EnsureInWorld: AllocPlayer(0x101d2370) -> 0x%p\n", pPlayer);

        if (pPlayer) {
            BYTE flag = 0;
            typedef void (__thiscall *PlayerCtor_t)(void* pThis, BYTE* pFlag, void* pExtraObj);
            PlayerCtor_t pCtor = reinterpret_cast<PlayerCtor_t>(clientBase + 0x001D17C0);
            pCtor(pPlayer, &flag, nullptr);

            float* pPos = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94);
            if (!pPos) {
                pPos = reinterpret_cast<float*>(calloc(16, sizeof(float)));
                *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94) = pPos;
            }
            if (pPos) {
                pPos[0] = 16710.0f;
                pPos[1] = (float)SPAWN_GROUND_ELEVATION;
                pPos[2] = 3230.0f;
                pPos[3] = 1.0f;
                pPos[4] = 0.0f;
                pPos[5] = 1.0f;
                pPos[15] = 1.0f;
            }

            float* pRot = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x98);
            if (!pRot) {
                pRot = reinterpret_cast<float*>(calloc(4, sizeof(float)));
                if (pRot) {
                    pRot[0] = 0.0f; pRot[1] = 0.0f; pRot[2] = 0.0f; pRot[3] = 1.0f;
                    *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x98) = pRot;
                }
            }

            *ppPlayerGlobal = nullptr;
            typedef void (__thiscall *PlayerEnterWorld_t)(void* pPlayer);
            PlayerEnterWorld_t pEnter = reinterpret_cast<PlayerEnterWorld_t>(clientBase + 0x001D2180);
            pEnter(pPlayer);
            s_playerEnteredWorld = true;
            *ppPlayerGlobal = pPlayer;
            *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pPlayer) + 0xC) |= 0x20;

            Log("[mxohax] EnsureInWorld: PlayerEnterWorld(0x101d2180) executed! [0x108a4378]=0x%p\n", *ppPlayerGlobal);
            ApplyOperativeAppearance(clientBase, pPlayer, nullptr);
        }
    } else {
        s_playerEnteredWorld = true;
        ApplyOperativeAppearance(clientBase, pPlayer, nullptr);
    }

    *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 1; // Third Person Chase Camera Mode

    if (pPlayer) {
        void* pActor = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xA8);
        if (pActor && !IsBadReadPtr(pActor, 0x40)) {
            __try {
                SyncActorPosition(clientBase, pPlayer, pActor, 16710.0, SPAWN_GROUND_ELEVATION, 3230.0);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    if (!promoteToState3) {
        s_inStreamingState4 = true;
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState) *pState = 4;
        void* pUI = GetCUIPointer(clientBase);
        if (pUI) {
            SafeHideControl(clientBase, pUI, 0x04);
            SafeHideControl(clientBase, pUI, 0x57);
            SafeHideControl(clientBase, pUI, 0x30);
            SafeHideControl(clientBase, pUI, 0x5D);
        }
        return;
    }

    static bool s_advanceToState3Done = false;
    if (pPlayer && !s_advanceToState3Done) {
        s_advanceToState3Done = true;
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState) *pState = 3;
        *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 0;
        typedef void (__thiscall *AdvanceToState3_t)(void* pMgr, void* pPlayer);
        AdvanceToState3_t pAdv3 = reinterpret_cast<AdvanceToState3_t>(clientBase + 0x00121B50);
        __try {
            pAdv3(pWorldMgr, pPlayer);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // Instantiating camera if needed
    void** ppCamera = reinterpret_cast<void**>(clientBase + 0x0089EDF8);
    if (ppCamera && !*ppCamera) {
        void* pCam = malloc(0x118);
        if (pCam) {
            memset(pCam, 0, 0x118);
            __try {
                typedef void (__thiscall *CamCtor_t)(void*);
                CamCtor_t pCamCtor = reinterpret_cast<CamCtor_t>(clientBase + 0x0012F020);
                pCamCtor(pCam);
                *ppCamera = pCam;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                free(pCam);
            }
        }
    }

    *reinterpret_cast<float*>(clientBase + 0x0089F2D0) = 12.0f;
    *reinterpret_cast<float*>(clientBase + 0x0089F304) = 0.0f;
    *reinterpret_cast<float*>(clientBase + 0x0089F338) = 280.0f;
    *reinterpret_cast<DWORD*>(clientBase + 0x0089EFAC) = 2;
    *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 2;

    if (ppCamera && *ppCamera) {
        __try {
            void* pCam = *ppCamera;
            typedef void (__thiscall *SetCameraMode_t)(void* pCamMgr, int mode);
            SetCameraMode_t pSetMode = reinterpret_cast<SetCameraMode_t>(clientBase + 0x0012D200);
            pSetMode(pCam, 2);

            g_playerX = 16710.0;
            g_playerY = SPAWN_GROUND_ELEVATION;
            g_playerZ = 3230.0;
            g_playerYaw = 0.0f;
            g_camYaw = 3.14159f;
            g_camPitch = 0.1745f;
            g_camDist = 420.0f;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // Ensure Viewport list is populated
    DWORD* ppListHead = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0xC);
    if (ppListHead) {
        void* head = *reinterpret_cast<void**>(ppListHead);
        if (!head || *reinterpret_cast<void**>(head) == head) {
            int width = *reinterpret_cast<int*>(clientBase + 0x00896CCC);
            int height = *reinterpret_cast<int*>(clientBase + 0x00896D04);
            if (width <= 0 || height <= 0) {
                width = 1920;
                height = 1080;
            }
            typedef void (__thiscall *CreateViewport_t)(void* pMgr, int w, int h);
            CreateViewport_t pCreateVp = reinterpret_cast<CreateViewport_t>(clientBase + 0x0011EAD0);
            pCreateVp(pWorldMgr, width, height);
        }
    }

    __try {
        if (pWorldMgr && !IsBadReadPtr(pWorldMgr, 0x30)) {
            DWORD* pVpCount = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 8);
            if (pVpCount) *pVpCount = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27) = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    s_inStreamingState4 = false;
    if (pShell) {
        *reinterpret_cast<BYTE*>(pShell + 0x20) = 1;
    }
    DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
    if (pState) *pState = 3;
    s_inWorldSticky = true;
    Log("[mxohax] *** PROMOTED TO STATE 3 (IN-WORLD)! 3D SIMULATION ACTIVE! ***\n");

    void* curWorldInst = *reinterpret_cast<void**>(clientBase + 0x0089DD6C);
    void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
    void* curCam = *reinterpret_cast<void**>(clientBase + 0x0089EDF8);
    Log("[mxohax] DetourFrameTick: Tick #%d active (State=3, renderFlag=1, inWorld=1, sticky=1, WorldInst=0x%p, Player=0x%p, Cam=0x%p)\n",
        s_tickCount, curWorldInst, curPlayer, curCam);

    *reinterpret_cast<float*>(clientBase + 0x008E357C) = 0.0f;
    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 0;

    void* pUI = GetCUIPointer(clientBase);
    if (pUI) {
        SafeHideControl(clientBase, pUI, 0x04);
        SafeHideControl(clientBase, pUI, 0x57);
        SafeHideControl(clientBase, pUI, 0x30);
        SafeHideControl(clientBase, pUI, 0x5D);
    }
}

// ============================================================================
// True per-frame tick on main thread (0x001F9140)
// ============================================================================

void __fastcall DetourFrameTick(void* pThis, void* /*edx*/) {
    HMODULE hClient = GetModuleHandleA("client.dll");
    DWORD clientBase = hClient ? reinterpret_cast<DWORD>(hClient) : 0;

    if (clientBase) {
        NeutralizeDragGlobals(clientBase);
    }

    if (OriginalFrameTick) OriginalFrameTick(pThis);
    s_tickCount++;

    if (!clientBase) return;
    NeutralizeDragGlobals(clientBase);

    DWORD pShell = clientBase + 0x00896A38;
    BYTE inWorld = *reinterpret_cast<BYTE*>(pShell + 0x20);
    static BYTE s_lastInWorld = 0xFF;

    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (pWorldMgr) {
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        static DWORD s_lastState = 0xFFFFFFFF;
        if (pState && *pState != s_lastState) {
            s_lastState = *pState;
            Log("[mxohax] *** WorldMgr State transitioned to %u ***\n", s_lastState);
            if (s_lastState == 3) {
                s_inWorldSticky = true;
            }
        }
    }

    if (s_inWorldSticky) {
        *reinterpret_cast<BYTE*>(pShell + 0x20) = 1;
        inWorld = 1;
        if (pWorldMgr) {
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27) = 1;
            DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
            if (pState && *pState != 3 && *pState != 4) {
                *pState = 3;
            }
        }

        static bool s_inWorldDismissedOnce = false;
        void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
        if (!s_inWorldDismissedOnce && curPlayer) {
            s_inWorldDismissedOnce = true;
            void* pUI = GetCUIPointer(clientBase);
            if (pUI) {
                SafeHideControl(clientBase, pUI, 0x04);
                SafeHideControl(clientBase, pUI, 0x57);
                SafeHideControl(clientBase, pUI, 0x30);
                SafeHideControl(clientBase, pUI, 0x5D);

                typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
                CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);

                static const DWORD hudControls[] = { 0x1B, 0x27, 0x24, 0x02, 0x03, 0x23, 0x4D };
                for (DWORD id : hudControls) {
                    __try {
                        void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (id * 4));
                        if (!ppCtrl || !*ppCtrl) pCreateControl(pUI, id);
                        SafeSetControlVisible(clientBase, pUI, id, 1);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                SafeHideControl(clientBase, pUI, 0x42);
                SafeHideControl(clientBase, pUI, 0x47);
                if (!g_hasTarget) {
                    SafeHideControl(clientBase, pUI, 0x22);
                    SafeHideControl(clientBase, pUI, 0x3D);
                }
                LockAllHudFrames(clientBase, pUI);
            }
            ApplyOperativeAppearance(clientBase, curPlayer, nullptr);
            Log("[mxohax] In-world UI initialized and loading screens dismissed once.\n");
        }

        static int s_inWorldTicks = 0;
        static DWORD s_lastTickTime = 0;
        s_inWorldTicks++;

        DWORD now = GetTickCount();
        double dt = (s_lastTickTime == 0) ? 0.01667 : (double)(now - s_lastTickTime) / 1000.0;
        if (dt > 0.1) dt = 0.1;
        if (dt < 0.001) dt = 0.01667;
        s_lastTickTime = now;

        if (g_focusModeActive && g_timeDilation > 0.01f) {
            dt *= (double)g_timeDilation;
        }

        // Tame glare & bloom
        *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x0084A02C) = 0;
        *reinterpret_cast<float*>(clientBase + 0x008E357C) = 0.0f;

        SetCVarFloat(clientBase + 0x008A9B44, 0.0f);
        SetCVarFloat(clientBase + 0x008A9C18, 0.0f);
        SetCVarFloat(clientBase + 0x008A9BBC, 0.0f);
        SetCVarFloat(clientBase + 0x008A9C54, 0.0f);
        SetCVarFloat(clientBase + 0x00909694, 0.0f);
        SetCVarFloat(clientBase + 0x009096C8, 0.0f);
        SetCVarFloat(clientBase + 0x009096FC, 0.0f);
        SetCVarFloat(clientBase + 0x0090962C, 0.0f);
        SetCVarFloat(clientBase + 0x008A2FAC, 0.0f);
        SetCVarFloat(clientBase + 0x008A3390, 0.0f);
        SetCVarFloat(clientBase + 0x008A33C4, 0.0f);
        SetCVarFloat(clientBase + 0x008A350C, 0.0f);
        SetCVarFloat(clientBase + 0x008A335C, 0.0f);
        SetCVarFloat(clientBase + 0x008A34D8, 0.0f);
        SetCVarFloat(clientBase + 0x008A3328, 1.0f);
        SetCVarFloat(clientBase + 0x008A34A4, 1.0f);
        SetCVarFloat(clientBase + 0x00908C90, (float)g_playerY);

        if (s_inWorldTicks % 30 == 0) {
            void* pUI = GetCUIPointer(clientBase);
            if (pUI) LockAllHudFrames(clientBase, pUI);
        }

        // Subclass game window if needed
        if (!OriginalWndProc || !g_hGameWindow || !IsWindow(g_hGameWindow)) {
            HWND hWnd = NULL;
            if (pShell && !IsBadReadPtr((void*)pShell, 0x30)) {
                HWND shellWnd = *reinterpret_cast<HWND*>(pShell + 0x14);
                if (shellWnd && IsWindow(shellWnd)) hWnd = shellWnd;
            }
            if (!hWnd && g_hGameWindow && IsWindow(g_hGameWindow)) hWnd = g_hGameWindow;
            if (!hWnd) hWnd = FindWindowA("MatrixWindowClass", NULL);
            if (!hWnd) hWnd = FindWindowA(NULL, "The Matrix Online");
            if (hWnd && IsWindow(hWnd)) SubclassGameWindow(hWnd);
        }

        // Human keyboard input processing
        HWND fgWnd = GetForegroundWindow();
        DWORD fgPid = 0;
        if (fgWnd) GetWindowThreadProcessId(fgWnd, &fgPid);
        bool hasFocus = (fgPid == GetCurrentProcessId()) || (g_hGameWindow && fgWnd == g_hGameWindow);
        bool keyW = s_keysDown['W'] || s_keysDown['w'] || s_keysDown[VK_UP] || (hasFocus && (((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP) & 0x8000) != 0)));
        bool keyS = s_keysDown['S'] || s_keysDown['s'] || s_keysDown[VK_DOWN] || (hasFocus && (((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0)));
        bool keyA = s_keysDown['A'] || s_keysDown['a'] || (hasFocus && ((GetAsyncKeyState('A') & 0x8000) != 0));
        bool keyD = s_keysDown['D'] || s_keysDown['d'] || (hasFocus && ((GetAsyncKeyState('D') & 0x8000) != 0));
        bool keyShift = s_keysDown[VK_SHIFT] || (hasFocus && ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0));
        bool keyLeft = s_keysDown[VK_LEFT] || (hasFocus && ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0));
        bool keyRight = s_keysDown[VK_RIGHT] || (hasFocus && ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0));

        g_bHumanInputActive = true;
        double speed = keyShift ? 480.0 : 280.0;
        double moveFwd = 0.0, moveRight = 0.0;
        if (keyW) moveFwd += 1.0;
        if (keyS) moveFwd -= 1.0;
        if (keyD) moveRight += 1.0;
        if (keyA) moveRight -= 1.0;

        if (keyLeft)  g_camYaw -= 2.0f * (float)dt;
        if (keyRight) g_camYaw += 2.0f * (float)dt;

        if (g_combatStrikeTimer > 0.0f) {
            g_combatStrikeTimer -= (float)dt;
            if (g_combatStrikeTimer < 0.0f) g_combatStrikeTimer = 0.0f;
        }

        bool isMoving = false;
        double actVelX = 0.0, actVelZ = 0.0;
        if (moveFwd != 0.0 || moveRight != 0.0) {
            double fwdX = sinf(g_camYaw);
            double fwdZ = cosf(g_camYaw);
            double rtX = cosf(g_camYaw);
            double rtZ = -sinf(g_camYaw);

            double dirX = fwdX * moveFwd + rtX * moveRight;
            double dirZ = fwdZ * moveFwd + rtZ * moveRight;
            double len = sqrt(dirX * dirX + dirZ * dirZ);
            if (len > 0.001) {
                dirX /= len;
                dirZ /= len;
                actVelX = dirX * speed;
                actVelZ = dirZ * speed;
                if (!g_isJumping && !g_isWallRunning) {
                    g_playerX += actVelX * dt;
                    g_playerZ += actVelZ * dt;
                }
                g_playerYaw = (float)atan2(dirX, dirZ);
                isMoving = true;
            }
        }
        g_bPlayerIsMoving = isMoving;

        if (isMoving) g_moveAnimBlend = fminf(1.0f, g_moveAnimBlend + (float)dt * 8.0f);
        else g_moveAnimBlend = fmaxf(0.0f, g_moveAnimBlend - (float)dt * 8.0f);

        if (curPlayer && !IsBadReadPtr(curPlayer, 0xB0)) {
            *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(curPlayer) + 0xC) |= 0x20;
            UpdatePlayerPositionAndPhysics(clientBase, curPlayer, dt, isMoving, actVelX, actVelZ);
        }

        *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 2; // Chase Cam

        void** ppCamera = reinterpret_cast<void**>(clientBase + 0x0089EDF8);
        if (ppCamera && *ppCamera) {
            void* pCam = *ppCamera;
            UpdateCamera(clientBase, pCam);

            static int s_camTickLog = 0;
            if (++s_camTickLog % 60 == 0) {
                Log("[mxohax] DetourFrameTick: Player=(%.1f, %.1f, %.1f) CamYaw=%.2f Pitch=%.2f Dist=%.1f Target='%s'\n",
                    g_playerX, g_playerY, g_playerZ, g_camYaw, g_camPitch, g_camDist, g_hasTarget ? g_targetName : "None");
            }
        }

        void* pUI = GetCUIPointer(clientBase);
        if (pUI) {
            HWND hWnd = g_hGameWindow;
            int screenW = 1920, screenH = 1080;
            if (hWnd && IsWindow(hWnd)) {
                RECT rc;
                if (GetClientRect(hWnd, &rc) && rc.right > rc.left) {
                    screenW = rc.right - rc.left;
                    screenH = rc.bottom - rc.top;
                }
            }
            RepositionQuickbar(clientBase, pUI, screenW, screenH);
            RepositionCompass(clientBase, pUI, screenW, screenH);
            RepositionCombatTactics(clientBase, pUI, screenW, screenH);
        }

        g_SubsystemMgr.UpdateAll((float)dt);
    }

    if (inWorld != s_lastInWorld) {
        s_lastInWorld = inWorld;
        Log("[mxohax] *** CClientShell::m_inWorld changed to %u! ***\n", inWorld);
        if (inWorld == 1) {
            Log("[mxohax] *** IN-WORLD CONFIRMED: 3D SIMULATION LOOP ACTIVE! ***\n");
            if (s_inWorldSticky) {
                void* pUI = GetCUIPointer(clientBase);
                if (pUI) {
                    SafeHideControl(clientBase, pUI, 0x04);
                    SafeHideControl(clientBase, pUI, 0x57);
                    SafeHideControl(clientBase, pUI, 0x30);
                    SafeHideControl(clientBase, pUI, 0x5D);
                    LockAllHudFrames(clientBase, pUI);
                }
            }
        }
    }

    // World loading and streaming transitions
    if (pWorldMgr && !s_inWorldSticky) {
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState) {
            DWORD curState = *pState;
            int curMarginState = GetMarginStateId();

            if (curState == 0 || curState == 1) {
                static int s_state1Ticks = 0;
                s_state1Ticks++;
                if (s_state1Ticks >= 20 && !s_autoJackInDone) {
                    s_autoJackInDone = true;
                    Log("[mxohax] DetourFrameTick: State %u AutoJackIn threshold reached (tick %d) -> triggering AutoJackIn...\n", curState, s_state1Ticks);
                    TryAutoJackIn(clientBase);
                }
            } else if (curState == 2) {
                static int s_state2Ticks = 0;
                s_state2Ticks++;
                if (s_state2Ticks >= 40) {
                    Log("[mxohax] DetourFrameTick: State 2 safety threshold reached (tick %d) -> ensuring in-world rendering...\n", s_state2Ticks);
                    EnsureInWorldRendering(clientBase, pWorldMgr, pShell);
                }
            } else if (curState == 4) {
                s_state4Ticks++;
                if (s_state4Ticks == 2) {
                    EnsureInWorldRendering(clientBase, pWorldMgr, pShell, false);
                }
                if (s_state4Ticks >= 35) {
                    Log("[mxohax] DetourFrameTick: Matrix code streaming fully complete (ticks=%d)! World 100%% rezzed. Promoting to State 3...\n", s_state4Ticks);
                    EnsureInWorldRendering(clientBase, pWorldMgr, pShell, true);
                }
            }
        }
    }
}

// ============================================================================
// Initialization & Entry Points
// ============================================================================

static void InitializeMxOHaxSynchronous() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    // Register all modular subsystems
    g_SubsystemMgr.RegisterSubsystem(&g_CrashHandler);
    g_SubsystemMgr.RegisterSubsystem(&g_ProtocolHook);
    g_SubsystemMgr.RegisterSubsystem(&g_D3D9Hook);
    g_SubsystemMgr.RegisterSubsystem(&g_InputManager);
    g_SubsystemMgr.RegisterSubsystem(&g_UIAnchorSystem);
    g_SubsystemMgr.RegisterSubsystem(&g_LocomotionSystem);

    uintptr_t clientBase = GetSafeClientBase();
    g_SubsystemMgr.InitializeAll(clientBase);

    Log("[mxohax] InitializeMxOHaxSynchronous complete (6 subsystems registered & initialized).\n");
}

static DWORD WINAPI WorkerThread(LPVOID /*lpParam*/) {
    HMODULE hClient = nullptr;
    for (int i = 0; i < 500 && !hClient; ++i) {
        hClient = GetModuleHandleA("client.dll");
        if (hClient) break;
        Sleep(20);
    }

    if (!hClient) return 0;
    ApplyClientPatches(hClient);

    for (int i = 0; i < 6000; ++i) {
        Sleep(100);
        g_SubsystemMgr.UpdateAll(0.1f);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        Log("[mxohax] DLL_PROCESS_ATTACH (thread %u)...\n", GetCurrentThreadId());
        InitializeMxOHaxSynchronous();
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        g_SubsystemMgr.ShutdownAll();
        Log("[mxohax] DLL_PROCESS_DETACH.\n");
    }
    return TRUE;
}

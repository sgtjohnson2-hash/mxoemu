#include "InputManager.h"
#include "../UIAnchorSystem/UIAnchorSystem.h"
#include "../LocomotionSystem/LocomotionSystem.h"
#include <windowsx.h>
#include <math.h>

InputManagerSubsystem g_InputManager;

StanceType g_currentStance = STANCE_FREE;
bool       s_keysDown[256] = {0};
float      g_camPitch = 12.0f * 0.0174532925f;
float      g_camYaw = 0.0f;
float      g_camDist = 280.0f;
int        g_lastMouseX = -1;
int        g_lastMouseY = -1;
bool       g_bRightMouseDown = false;
bool       g_bLeftMouseDown = false;
bool       g_bHumanInputActive = false;
bool       g_bPlayerIsMoving = false;
bool       g_bMouseDownOnUI = false;
int        g_pressedHudButton = 0;
int        g_quickbarPage = 1;
float      g_bulletDodgeTimer = 0.0f;

bool s_charSheetVisible = false;
bool s_optionsVisible = false;

WNDPROC OriginalWndProc = nullptr;
static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#pragma pack(push, 1)
struct LithtechInputEvent {
    DWORD unk0;      // 0x00
    DWORD unk4;      // 0x04
    DWORD eventCode; // 0x08: 'Move' (0x65766F4D), 'MLDn' (0x6E444C4D), 'MLUp' (0x70554C4D), 'MRDn' (0x6E44524D), 'MRUp' (0x7055524D)
    DWORD unkC;      // 0x0C
    DWORD unk10;     // 0x10
    int   mouseX;    // 0x14
    int   mouseY;    // 0x18
    DWORD unk1C;     // 0x1C
    DWORD unk20;     // 0x20
};
#pragma pack(pop)

void DispatchInputEventToClient(uintptr_t clientBase, DWORD eventCode, int x, int y) {
    if (!clientBase) return;
    void* pWidgetMgr = *reinterpret_cast<void**>(clientBase + 0x00897F98);
    if (!pWidgetMgr || IsBadReadPtr(pWidgetMgr, 0x100)) return;

    typedef void (__thiscall *SetCursorPos_t)(void* pThis, int x, int y);
    SetCursorPos_t pSetCursorPos = reinterpret_cast<SetCursorPos_t>(clientBase + 0x00377030);
    __try {
        pSetCursorPos(pWidgetMgr, x, y);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    LithtechInputEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.eventCode = eventCode;
    evt.mouseX = x;
    evt.mouseY = y;

    typedef void (__cdecl *DispatchInput_t)(LithtechInputEvent* pEvent);
    DispatchInput_t pDispatch = reinterpret_cast<DispatchInput_t>(clientBase + 0x0001DED0);
    __try {
        pDispatch(&evt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void* GetHoveredUIWidget(uintptr_t clientBase) {
    if (!clientBase) return nullptr;
    void* pWidgetMgr = *reinterpret_cast<void**>(clientBase + 0x00897F98);
    if (!pWidgetMgr || IsBadReadPtr(pWidgetMgr, 0x100)) return nullptr;
    typedef void* (__thiscall *GetHoveredWidget_t)(void* pThis);
    GetHoveredWidget_t pGetHovered = reinterpret_cast<GetHoveredWidget_t>(clientBase + 0x00378270);
    __try {
        return pGetHovered(pWidgetMgr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void SetWidgetVisualState(void* pWidget, int state) {
    if (!pWidget || IsBadReadPtr(pWidget, sizeof(void*))) return;
    void** vtbl = *reinterpret_cast<void***>(pWidget);
    if (!vtbl || IsBadReadPtr(vtbl, 0x100)) return;
    typedef void (__thiscall *SetState_t)(void* pThis, int state);
    SetState_t pSetState = reinterpret_cast<SetState_t>(vtbl[0x94 / 4]);
    if (!pSetState || IsBadReadPtr(reinterpret_cast<void*>(pSetState), 1)) return;
    __try {
        pSetState(pWidget, state);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

HudButtonId HitTestGeometry(int testX, int testY, int w, int h) {
    if (w <= 0) w = 1920;
    if (h <= 0) h = 1080;

    const int postureW = 134;
    const int postureH = 32;
    const int postureX = (w / 2) - (postureW / 2);
    const int postureY = h - 167;
    if (testX >= postureX && testX <= postureX + postureW && testY >= postureY && testY <= postureY + postureH) {
        int idx = (testX - postureX) / 34;
        if (idx == 0) return HUD_BTN_TACTIC_FREE;
        if (idx == 1) return HUD_BTN_TACTIC_POWER;
        if (idx == 2) return HUD_BTN_TACTIC_GRAB;
        if (idx >= 3) return HUD_BTN_TACTIC_WITHDRAW;
    }

    const int qbW = 428;
    const int qbH = 50;
    const int qbX = (w - qbW) / 2;
    const int qbY = h - 221;
    if (testX >= qbX && testX <= qbX + qbW && testY >= qbY && testY <= qbY + qbH) {
        if (testX >= qbX && testX < qbX + 36) {
            return HUD_BTN_QB_PAGE;
        }
        for (int i = 0; i < 10; ++i) {
            int slotX = qbX + 36 + (i * 37);
            if (testX >= slotX && testX < slotX + 37) {
                return (HudButtonId)(HUD_BTN_QB_1 + i);
            }
        }
        if (testX >= qbX + 406) {
            return HUD_BTN_TACTIC_FREE;
        }
    }

    const int leftWingX = (w / 2) - 122;
    const int leftWingY = h - 35;
    if (testX >= leftWingX && testX <= leftWingX + 42 && testY >= leftWingY && testY <= leftWingY + 30) {
        return HUD_BTN_CHAR_STATUS;
    }

    const int rightWingX = (w / 2) + 80;
    const int rightWingY = h - 35;
    if (testX >= rightWingX && testX <= rightWingX + 42 && testY >= rightWingY && testY <= rightWingY + 30) {
        return HUD_BTN_CELL_PHONE;
    }

    int compCenterX = w / 2;
    int compCenterY = h - 68;
    int cdx = testX - compCenterX;
    int cdy = testY - compCenterY;
    if (cdx * cdx + cdy * cdy <= 72 * 72) {
        return HUD_BTN_COMPASS;
    }

    const int meterW = 64;
    const int meterH = 27;
    const int meterX = w - meterW - 10;
    const int meterY = h - 35;
    if (testY >= meterY && testY <= meterY + meterH && testX >= meterX && testX <= meterX + meterW) {
        return HUD_BTN_LATENCY;
    }
    if (testY >= meterY && testX > meterX + meterW) {
        return HUD_BTN_OPTIONS;
    }

    if (g_hasTarget) {
        const int targetW = 240;
        const int targetH = 90;
        const int targetX = w - targetW - 10;
        const int targetY = 10;
        if (testX >= targetX && testX <= w - 5 && testY >= targetY && testY <= targetY + targetH) {
            return HUD_BTN_TARGET_VITALS;
        }
    }

    if (testX >= 5 && testX <= 270 && testY >= 5 && testY <= 110) {
        return HUD_BTN_TARGET_VITALS;
    }

    return HUD_BTN_NONE;
}

HudButtonId HitTestHudButton(int x, int y, int screenW, int screenH) {
    if (screenW <= 0) screenW = 1920;
    if (screenH <= 0) screenH = 1080;

    HudButtonId btn = HitTestGeometry(x, y, screenW, screenH);
    if (btn != HUD_BTN_NONE) return btn;

    if (screenW != 1920 || screenH != 1080) {
        int canX = (int)((double)x * 1920.0 / (double)screenW);
        int canY = (int)((double)y * 1080.0 / (double)screenH);
        btn = HitTestGeometry(canX, canY, 1920, 1080);
        if (btn != HUD_BTN_NONE) return btn;
    }

    if (screenW != 1920 || screenH != 1080) {
        btn = HitTestGeometry(x, y, 1920, 1080);
        if (btn != HUD_BTN_NONE) return btn;
    }

    return HUD_BTN_NONE;
}

bool IsPointInAnyHudRect(int normX, int normY) {
    if (normX >= 0 && normX <= 700 && normY >= 0 && normY <= 120) return true;
    if (g_hasTarget && normX >= 1650 && normX <= 1920 && normY >= 0 && normY <= 200) return true;
    int cdx = normX - 960;
    int cdy = normY - 1013;
    if (cdx * cdx + cdy * cdy <= 75 * 75) return true;
    if (normX >= 0 && normX <= 500 && normY >= 750 && normY <= 1080) return true;
    if (normX >= 1750 && normX <= 1920 && normY >= 1000 && normY <= 1080) return true;
    return false;
}

bool IsPointOverAnyHud(int mx, int my, int winW, int winH) {
    if (winW <= 0) winW = 1920;
    if (winH <= 0) winH = 1080;

    if (HitTestHudButton(mx, my, winW, winH) != HUD_BTN_NONE) return true;
    if (mx >= 0 && mx <= 270 && my >= 0 && my <= 110) return true;
    if (g_hasTarget && mx >= (winW - 270) && mx <= winW && my >= 0 && my <= 200) return true;
    if (mx >= 0 && mx <= 500 && my >= (winH - 330) && my <= winH) return true;
    int centerX = winW / 2;
    if (mx >= (centerX - 230) && mx <= (centerX + 230) && my >= (winH - 225) && my <= winH) return true;
    if (mx >= (winW - 170) && mx <= winW && my >= (winH - 80) && my <= winH) return true;

    if (s_charSheetVisible || s_optionsVisible) {
        int modalLeft = (winW / 2) - 200;
        int modalTop = (winH / 2) - 200;
        if (mx >= modalLeft && mx <= modalLeft + 400 && my >= modalTop && my <= modalTop + 400) return true;
    }

    if (winW != 1920 || winH != 1080) {
        int canX = (int)((double)mx * 1920.0 / (double)winW);
        int canY = (int)((double)my * 1080.0 / (double)winH);
        if (canX >= (960 - 230) && canX <= (960 + 230) && canY >= (1080 - 225) && canY <= 1080) return true;
        if (canX >= 0 && canX <= 270 && canY >= 0 && canY <= 110) return true;
        if (g_hasTarget && canX >= (1920 - 270) && canX <= 1920 && canY >= 0 && canY <= 200) return true;
        if (canX >= 0 && canX <= 500 && canY >= (1080 - 330) && canY <= 1080) return true;
    }

    return false;
}

void TriggerPhoneCall(uintptr_t clientBase) {
    Log("[mxohax] CELL PHONE ACTIVATED: Initiating safe contact with Zion Operator...\n");
    void* pThis = malloc(0x100);
    if (pThis) {
        memset(pThis, 0, 0x100);
        Safe_MissionContact_Button_Call(pThis, nullptr);
        free(pThis);
    }
}

void SetTargetOperative(uintptr_t clientBase, const char* name, DWORD charId, double x, double y, double z, int level, int health) {
    if (!clientBase || clientBase < 0x1000000 || clientBase >= 0x7FFE0000) {
        clientBase = GetSafeClientBase();
    }
    g_hasTarget = true;
    g_targetCharId = charId;
    strncpy_s(g_targetName, sizeof(g_targetName), name, _TRUNCATE);
    g_targetX = x;
    g_targetY = y;
    g_targetZ = z;

    double dx = x - g_playerX;
    double dz = z - g_playerZ;
    double dist = sqrt(dx*dx + dz*dz);
    Log("[mxohax] TARGET SELECTED: '%s' [CharId=%u, Lvl=%d, HP=%d%%] at (%.1f, %.1f, %.1f) - Dist: %.1fm\n",
        name, charId, level, health, x, y, z, dist);

    void* pUI = GetCUIPointer(clientBase);
    if (pUI) {
        typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
        CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);

        void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x22 * 4));
        if (!ppCtrl || IsBadReadPtr(ppCtrl, sizeof(void*)) || !*ppCtrl) {
            __try {
                pCreateControl(pUI, 0x22);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        if (ppCtrl && !IsBadReadPtr(ppCtrl, sizeof(void*)) && *ppCtrl && !IsBadReadPtr(*ppCtrl, 0x350)) {
            void* pViewTarget = *ppCtrl;
            __try {
                BYTE isInit = *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x58);
                void* pWgt7C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x7C);
                void* pWgt22C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x22C);
                if (isInit && pWgt7C && pWgt22C && !IsBadReadPtr(pWgt22C, 4)) {
                    typedef void (__thiscall *fnSetTargetLevel)(void* pThis, int lvl, int targetIdx);
                    fnSetTargetLevel pSetLevel = reinterpret_cast<fnSetTargetLevel>(clientBase + 0x000D3970);
                    pSetLevel(pViewTarget, level, 0);

                    typedef void (__thiscall *fnSetTargetHealth)(void* pThis, int targetIdx, int hp);
                    fnSetTargetHealth pSetHealth = reinterpret_cast<fnSetTargetHealth>(clientBase + 0x000D39F0);
                    pSetHealth(pViewTarget, 0, health);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("[mxohax] SetTargetOperative: Safe exception handling in CViewTarget updates\n");
            }
        }

        HWND hWndTarget = g_hGameWindow;
        if (!hWndTarget || !IsWindow(hWndTarget)) hWndTarget = FindWindowA(NULL, "The Matrix Online");
        int screenW = 1920;
        if (hWndTarget && IsWindow(hWndTarget)) {
            RECT rc;
            if (GetClientRect(hWndTarget, &rc) && (rc.right - rc.left) > 0) {
                screenW = rc.right - rc.left;
            }
        }
        PositionControlAndWidget(clientBase, pUI, 0x22, screenW - 250, 10, 240, 90);
        __try {
            SafeSetControlVisible(clientBase, pUI, 0x22, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

void SetTacticsStance(uintptr_t clientBase, StanceType newStance) {
    if (!clientBase || clientBase < 0x1000000 || clientBase >= 0x7FFE0000) {
        clientBase = GetSafeClientBase();
    }
    g_currentStance = newStance;
    const char* stanceNames[] = { "Free", "Power", "Grab", "Speed", "Withdraw" };
    const char* sName = (newStance >= 0 && newStance <= 4) ? stanceNames[newStance] : "Unknown";
    Log("[mxohax] TACTICS STANCE CHANGED: Stance is now [%s] (mode=%d)\n", sName, (int)newStance);

    void* pUI = GetCUIPointer(clientBase);
    if (pUI) {
        __try {
            SafeSetControlVisible(clientBase, pUI, 0x1B, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
        if (pInterlock && !IsBadReadPtr(pInterlock, 0x100)) {
            void* pBtnFree  = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x6C);
            void* pBtnPower = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x70);
            void* pBtnGrab  = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x74);
            void* pBtnBlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x80);
            if (!pBtnBlock || !IsValidWidget(clientBase, pBtnBlock)) {
                pBtnBlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x7C);
            }
            if (pBtnFree && IsValidWidget(clientBase, pBtnFree))   SetWidgetVisualState(pBtnFree,  (newStance == STANCE_FREE) ? 4 : 0);
            if (pBtnPower && IsValidWidget(clientBase, pBtnPower)) SetWidgetVisualState(pBtnPower, (newStance == STANCE_POWER) ? 4 : 0);
            if (pBtnGrab && IsValidWidget(clientBase, pBtnGrab))   SetWidgetVisualState(pBtnGrab,  (newStance == STANCE_GRAB)  ? 4 : 0);
            if (pBtnBlock && IsValidWidget(clientBase, pBtnBlock)) SetWidgetVisualState(pBtnBlock, (newStance == STANCE_WITHDRAW) ? 4 : 0);
        }

        void* pQuickbar = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x24 * 4);
        if (pQuickbar && !IsBadReadPtr(pQuickbar, 0x200)) {
            for (int i = 0; i < 5; ++i) {
                void** ppBtn = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pQuickbar) + 0xCC + (i * 0x1C));
                if (ppBtn && *ppBtn && !IsBadReadPtr(*ppBtn, 0x40)) {
                    SetWidgetVisualState(*ppBtn, (newStance == i) ? 4 : 0);
                }
                void** ppIcon = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pQuickbar) + 0xD0 + (i * 0x1C));
                if (ppIcon && *ppIcon && !IsBadReadPtr(*ppIcon, 0x40)) {
                    SetWidgetVisualState(*ppIcon, (newStance == i) ? 4 : 0);
                }
            }
        }
    }
}

void ExecuteQuickbarAbility(uintptr_t clientBase, int slotIndex) {
    if (slotIndex < 1 || slotIndex > 10) return;
    if (!clientBase || clientBase < 0x1000000 || clientBase >= 0x7FFE0000) {
        clientBase = GetSafeClientBase();
    }

    const char* slotNames[] = {
        "Combat Stance [Free]",
        "Combat Stance [Power]",
        "Combat Stance [Grab]",
        "Combat Stance [Speed]",
        "Combat Stance [Withdraw]",
        "Strike (Martial Arts)",
        "Hyper-Jump",
        "Subroutine Compile",
        "Logic Bomb",
        "Call Operator (Cell Phone)"
    };
    const char* name = slotNames[slotIndex - 1];

    Log("[mxohax] QUICKBAR EXECUTION: Slot %d -> '%s' (Target: %s)\n",
        slotIndex, name, g_hasTarget ? g_targetName : "Self");

    switch (slotIndex) {
        case 1:
            SetTacticsStance(clientBase, STANCE_FREE);
            break;
        case 2:
            SetTacticsStance(clientBase, STANCE_POWER);
            break;
        case 3:
            SetTacticsStance(clientBase, STANCE_GRAB);
            break;
        case 4:
            SetTacticsStance(clientBase, STANCE_SPEED);
            break;
        case 5:
            SetTacticsStance(clientBase, STANCE_WITHDRAW);
            break;
        case 6: { // Strike
            if (!g_hasTarget) {
                SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01, 50, 100);
            }
            double tdx = g_targetX - g_playerX;
            double tdz = g_targetZ - g_playerZ;
            if (fabs(tdx) > 0.1 || fabs(tdz) > 0.1) {
                g_playerYaw = (float)atan2(tdx, tdz);
            }
            void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
            if (curPlayer && !IsBadReadPtr(curPlayer, 0xB0)) {
                void* pActor = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(curPlayer) + 0xA8);
                if (pActor && !IsBadReadPtr(pActor, 0x690)) {
                    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 0;
                    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = 1.0f;
                }
            }
            g_combatStrikeTimer = 0.65f;
            static int s_targetHealth = 100;
            s_targetHealth -= 14;
            if (s_targetHealth <= 10) s_targetHealth = 100;
            Log("[mxohax] COMBAT ACTION: Strike hits '%s' for 85 damage! Target HP: %d%%\n", g_targetName, s_targetHealth);
            void* pUIHealth = GetCUIPointer(clientBase);
            if (pUIHealth) {
                void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUIHealth) + 0x28 + (0x22 * 4));
                if (ppCtrl && *ppCtrl && !IsBadReadPtr(*ppCtrl, 0x350)) {
                    void* pViewTarget = *ppCtrl;
                    BYTE isInit = *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x58);
                    void* pWgt7C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x7C);
                    void* pWgt22C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x22C);
                    if (isInit && pWgt7C && pWgt22C && !IsBadReadPtr(pWgt22C, 4)) {
                        typedef void (__thiscall *fnSetTargetHealth)(void* pThis, int targetIdx, int hp);
                        fnSetTargetHealth pSetHealth = reinterpret_cast<fnSetTargetHealth>(clientBase + 0x000D39F0);
                        __try { pSetHealth(pViewTarget, 0, s_targetHealth); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }
            }
            break;
        }
        case 7: { // Hyper-Jump
            if (!g_isJumping) {
                g_isJumping = true;
                g_velY = 220.0;
                Log("[mxohax] ACROBATICS: Hyper-Jump initiated! (velY=220.0)\n");
            }
            break;
        }
        case 8: { // Subroutine Compile
            Log("[mxohax] ABILITY: Subroutine Compile activated (IS buffer replenished +120)\n");
            break;
        }
        case 9: { // Logic Bomb
            if (!g_hasTarget) {
                SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01, 50, 100);
            }
            double tdx = g_targetX - g_playerX;
            double tdz = g_targetZ - g_playerZ;
            if (fabs(tdx) > 0.1 || fabs(tdz) > 0.1) {
                g_playerYaw = (float)atan2(tdx, tdz);
            }
            void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
            if (curPlayer && !IsBadReadPtr(curPlayer, 0xB0)) {
                void* pActor = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(curPlayer) + 0xA8);
                if (pActor && !IsBadReadPtr(pActor, 0x690)) {
                    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 0;
                    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = 1.0f;
                }
            }
            g_combatStrikeTimer = 0.65f;
            static int s_lbHealth = 100;
            s_lbHealth -= 28;
            if (s_lbHealth <= 10) s_lbHealth = 100;
            Log("[mxohax] COMBAT ACTION: Logic Bomb detonated on '%s' for 210 viral damage! Target HP: %d%%\n", g_targetName, s_lbHealth);
            void* pUIHealth = GetCUIPointer(clientBase);
            if (pUIHealth) {
                void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUIHealth) + 0x28 + (0x22 * 4));
                if (ppCtrl && *ppCtrl && !IsBadReadPtr(*ppCtrl, 0x350)) {
                    void* pViewTarget = *ppCtrl;
                    BYTE isInit = *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x58);
                    void* pWgt7C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x7C);
                    void* pWgt22C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pViewTarget) + 0x22C);
                    if (isInit && pWgt7C && pWgt22C && !IsBadReadPtr(pWgt22C, 4)) {
                        typedef void (__thiscall *fnSetTargetHealth)(void* pThis, int targetIdx, int hp);
                        fnSetTargetHealth pSetHealth = reinterpret_cast<fnSetTargetHealth>(clientBase + 0x000D39F0);
                        __try { pSetHealth(pViewTarget, 0, s_lbHealth); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }
            }
            break;
        }
        case 10: { // Call Operator (Cell Phone)
            TriggerPhoneCall(clientBase);
            break;
        }
    }

    void* pUI = GetCUIPointer(clientBase);
    if (pUI) {
        __try {
            if (g_hasTarget) {
                SafeSetControlVisible(clientBase, pUI, 0x22, 1);
                SafeSetControlVisible(clientBase, pUI, 0x3D, 1);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        void* pQuickbar = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x24 * 4);
        if (pQuickbar && !IsBadReadPtr(pQuickbar, 0x200)) {
            int idx = slotIndex - 1;
            void** ppBtn = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pQuickbar) + 0xCC + (idx * 0x1C));
            if (ppBtn && *ppBtn && !IsBadReadPtr(*ppBtn, 0x40)) {
                SetWidgetVisualState(*ppBtn, 4);
            }
            void** ppIcon = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pQuickbar) + 0xD0 + (idx * 0x1C));
            if (ppIcon && *ppIcon && !IsBadReadPtr(*ppIcon, 0x40)) {
                SetWidgetVisualState(*ppIcon, 4);
            }
        }
    }
}

void ExecuteHudButtonAction(uintptr_t clientBase, HudButtonId btnId, int mx, int my) {
    if (!clientBase) return;
    void* pUI = GetCUIPointer(clientBase);

    switch (btnId) {
        case HUD_BTN_QB_PAGE: {
            g_quickbarPage = (g_quickbarPage == 1) ? 2 : 1;
            Log("[mxohax] UI BUTTON CLICK: Quickbar Page Switched to Page %d (at %d, %d)\n", g_quickbarPage, mx, my);
            break;
        }
        case HUD_BTN_QB_1:
        case HUD_BTN_QB_2:
        case HUD_BTN_QB_3:
        case HUD_BTN_QB_4:
        case HUD_BTN_QB_5:
        case HUD_BTN_QB_6:
        case HUD_BTN_QB_7:
        case HUD_BTN_QB_8:
        case HUD_BTN_QB_9:
        case HUD_BTN_QB_10: {
            int slot = (int)(btnId - HUD_BTN_QB_1) + 1;
            Log("[mxohax] UI BUTTON CLICK: Quickbar Slot %d clicked at (%d, %d)\n", slot, mx, my);
            ExecuteQuickbarAbility(clientBase, slot);
            break;
        }
        case HUD_BTN_TACTIC_FREE: {
            Log("[mxohax] UI BUTTON CLICK: Combat Tactics [Free] clicked at (%d, %d)\n", mx, my);
            SetTacticsStance(clientBase, STANCE_FREE);
            if (pUI && !IsBadReadPtr(pUI, 0x200)) {
                void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
                if (pInterlock) Safe_Interlock_Speed_Button(pInterlock, nullptr);
            }
            break;
        }
        case HUD_BTN_TACTIC_POWER: {
            Log("[mxohax] UI BUTTON CLICK: Combat Tactics [Power] clicked at (%d, %d)\n", mx, my);
            SetTacticsStance(clientBase, STANCE_POWER);
            if (pUI && !IsBadReadPtr(pUI, 0x200)) {
                void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
                if (pInterlock) Safe_Interlock_Power_Button(pInterlock, nullptr);
            }
            break;
        }
        case HUD_BTN_TACTIC_GRAB: {
            Log("[mxohax] UI BUTTON CLICK: Combat Tactics [Grab] clicked at (%d, %d)\n", mx, my);
            SetTacticsStance(clientBase, STANCE_GRAB);
            if (pUI && !IsBadReadPtr(pUI, 0x200)) {
                void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
                if (pInterlock) Safe_Interlock_Grab_Button(pInterlock, nullptr);
            }
            break;
        }
        case HUD_BTN_TACTIC_SPEED: {
            Log("[mxohax] UI BUTTON CLICK: Combat Tactics [Speed] clicked at (%d, %d)\n", mx, my);
            SetTacticsStance(clientBase, STANCE_SPEED);
            if (pUI && !IsBadReadPtr(pUI, 0x200)) {
                void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
                if (pInterlock) Safe_Interlock_Speed_Button(pInterlock, nullptr);
            }
            break;
        }
        case HUD_BTN_TACTIC_WITHDRAW: {
            Log("[mxohax] UI BUTTON CLICK: Combat Tactics [Withdraw] clicked at (%d, %d)\n", mx, my);
            SetTacticsStance(clientBase, STANCE_WITHDRAW);
            if (pUI && !IsBadReadPtr(pUI, 0x200)) {
                void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
                if (pInterlock) Safe_Interlock_Block_Button(pInterlock, nullptr);
            }
            break;
        }
        case HUD_BTN_CELL_PHONE: {
            Log("[mxohax] UI BUTTON CLICK: Cell Phone button clicked at (%d, %d) -> Calling Operator!\n", mx, my);
            TriggerPhoneCall(clientBase);
            break;
        }
        case HUD_BTN_CHAR_STATUS: {
            Log("[mxohax] UI BUTTON CLICK: Character Status button clicked at (%d, %d)\n", mx, my);
            if (pUI && !IsBadReadPtr(pUI, 0x100)) {
                typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
                CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
                void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x42 * 4));
                s_charSheetVisible = !s_charSheetVisible;
                if (s_charSheetVisible) {
                    if (!ppCtrl || !*ppCtrl) pCreateControl(pUI, 0x42);
                    PositionControlAndWidget(clientBase, pUI, 0x42, (1920 / 2) - 200, (1080 / 2) - 200, 400, 400);
                    __try { SafeSetControlVisible(clientBase, pUI, 0x42, 1); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                } else {
                    SafeHideControl(clientBase, pUI, 0x42);
                    __try { SafeSetControlVisible(clientBase, pUI, 0x42, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            break;
        }
        case HUD_BTN_COMPASS: {
            Log("[mxohax] UI BUTTON CLICK: Compass dial clicked at (%d, %d) -> Resetting camera yaw to player facing\n", mx, my);
            g_camYaw = g_playerYaw;
            g_camPitch = 0.17f;
            break;
        }
        case HUD_BTN_OPTIONS: {
            Log("[mxohax] UI BUTTON CLICK: Options/Checklist button clicked at (%d, %d)\n", mx, my);
            if (pUI && !IsBadReadPtr(pUI, 0x100)) {
                typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
                CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
                void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x47 * 4));
                s_optionsVisible = !s_optionsVisible;
                if (s_optionsVisible) {
                    if (!ppCtrl || !*ppCtrl) pCreateControl(pUI, 0x47);
                    PositionControlAndWidget(clientBase, pUI, 0x47, (1920 / 2) - 200, (1080 / 2) - 200, 400, 400);
                    __try { SafeSetControlVisible(clientBase, pUI, 0x47, 1); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                } else {
                    SafeHideControl(clientBase, pUI, 0x47);
                    __try { SafeSetControlVisible(clientBase, pUI, 0x47, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            break;
        }
        case HUD_BTN_LATENCY: {
            Log("[mxohax] UI BUTTON CLICK: Network Latency meter clicked at (%d, %d) (Ping: 14ms, State: In-World)\n", mx, my);
            break;
        }
        case HUD_BTN_TARGET_VITALS: {
            Log("[mxohax] UI BUTTON CLICK: Operative Vitals/Portrait clicked at (%d, %d) -> Targeting self/nearest\n", mx, my);
            SetTargetOperative(clientBase, "S1acker (Self)", 359, g_playerX, g_playerY, g_playerZ);
            break;
        }
        default:
            break;
    }
}

void SubclassGameWindow(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;

    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid != GetCurrentProcessId()) {
        Log("[mxohax] SubclassGameWindow: REJECTED foreign window 0x%p (PID %u != %u)\n", hWnd, pid, GetCurrentProcessId());
        return;
    }

    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 640 || h < 480) {
        Log("[mxohax] SubclassGameWindow: REJECTED undersized window 0x%p (%dx%d)\n", hWnd, w, h);
        return;
    }

    char clsName[64] = {0};
    GetClassNameA(hWnd, clsName, sizeof(clsName));
    if (strcmp(clsName, "Shell_TrayWnd") == 0 || strcmp(clsName, "Progman") == 0 || strcmp(clsName, "WorkerW") == 0) {
        Log("[mxohax] SubclassGameWindow: REJECTED shell window 0x%p ('%s')\n", hWnd, clsName);
        return;
    }

    if (g_hGameWindow == hWnd && OriginalWndProc != nullptr) return;
    g_hGameWindow = hWnd;
    WNDPROC curProc = (WNDPROC)GetWindowLongPtrA(hWnd, GWLP_WNDPROC);
    if (curProc != SubclassWndProc) {
        SetLastError(0);
        OriginalWndProc = (WNDPROC)SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)SubclassWndProc);
        if (!OriginalWndProc) {
            DWORD err = GetLastError();
            if (curProc) OriginalWndProc = curProc;
            Log("[mxohax] SubclassGameWindow warning: SetWindowLongPtrA returned NULL (err=%u), curProc=0x%p\n", err, curProc);
        }
        Log("[mxohax] Subclassed game window 0x%p ('%s', %dx%d) for full input handling! (OriginalWndProc=0x%p)\n",
            hWnd, clsName, w, h, OriginalWndProc);
    }
    FILE* fWnd = fopen("E:\\Games\\The Matrix Online\\active_game_wnd.txt", "w");
    if (fWnd) {
        fprintf(fWnd, "%u %d %d\n", (DWORD)(uintptr_t)hWnd, w, h);
        fclose(fWnd);
    }
}

static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    uintptr_t clientBase = GetSafeClientBase();
    NeutralizeDragGlobals();

    RECT clientRc;
    GetClientRect(hWnd, &clientRc);
    int winW = clientRc.right - clientRc.left;
    int winH = clientRc.bottom - clientRc.top;
    if (winW <= 0) winW = 1920;
    if (winH <= 0) winH = 1080;

    switch (uMsg) {
        case WM_ACTIVATE:
        case WM_SETFOCUS: {
            NeutralizeDragGlobals();
            break;
        }
        case WM_KILLFOCUS: {
            memset(s_keysDown, 0, sizeof(s_keysDown));
            g_bPlayerIsMoving = false;
            g_bMouseDownOnUI = false;
            g_bLeftMouseDown = false;
            g_bRightMouseDown = false;
            if (GetCapture() == hWnd) ReleaseCapture();
            NeutralizeDragGlobals();
            break;
        }
        case WM_MOUSEMOVE: {
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);

            bool isRightDrag = ((wParam & MK_RBUTTON) != 0) || g_bRightMouseDown;
            bool isLeftWorldDrag = (((wParam & MK_LBUTTON) != 0) || g_bLeftMouseDown) && !g_bMouseDownOnUI;

            if (isRightDrag || isLeftWorldDrag) {
                if (g_lastMouseX >= 0 && g_lastMouseY >= 0) {
                    int dx = mx - g_lastMouseX;
                    int dy = my - g_lastMouseY;
                    g_camYaw += dx * 0.005f;
                    g_camPitch += dy * 0.005f;
                    if (g_camPitch < -0.45f) g_camPitch = -0.45f;
                    if (g_camPitch > 1.15f)  g_camPitch = 1.15f;
                    g_bHumanInputActive = true;
                    static int s_orbitLog = 0;
                    if (++s_orbitLog % 4 == 0) {
                        Log("[mxohax] Camera Orbit: Yaw=%.2f Pitch=%.2f (dx=%d, dy=%d, right=%d, left=%d)\n",
                            g_camYaw, g_camPitch, dx, dy, isRightDrag ? 1 : 0, isLeftWorldDrag ? 1 : 0);
                    }
                }
            }
            g_lastMouseX = mx;
            g_lastMouseY = my;

            uintptr_t cb = GetSafeClientBase();
            DispatchInputEventToClient(cb, 0x65766F4D, mx, my); // 'Move'

            NeutralizeDragGlobals();
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_LBUTTONDOWN: {
            NeutralizeDragGlobals();
            SetFocus(hWnd);
            SetActiveWindow(hWnd);
            g_bLeftMouseDown = true;
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_lastMouseX = mx;
            g_lastMouseY = my;
            g_bHumanInputActive = true;
            Log("[mxohax] WM_LBUTTONDOWN: (%d, %d)\n", mx, my);

            HudButtonId hitBtn = HitTestHudButton(mx, my, winW, winH);
            g_pressedHudButton = (int)hitBtn;
            if (hitBtn != HUD_BTN_NONE || IsPointOverAnyHud(mx, my, winW, winH)) {
                g_bMouseDownOnUI = true;
                uintptr_t cb = GetSafeClientBase();
                DispatchInputEventToClient(cb, 0x6E444C4D, mx, my); // 'MLDn'
            } else {
                g_bMouseDownOnUI = false;
            }

            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            if (!g_bMouseDownOnUI) {
                SetCapture(hWnd);
            }
            NeutralizeDragGlobals();
            return lRes;
        }
        case WM_LBUTTONUP: {
            NeutralizeDragGlobals();
            g_bLeftMouseDown = false;
            if (!g_bLeftMouseDown && !g_bRightMouseDown && GetCapture() == hWnd) {
                ReleaseCapture();
            }
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_bHumanInputActive = true;
            Log("[mxohax] WM_LBUTTONUP: (%d, %d)\n", mx, my);

            uintptr_t cb = GetSafeClientBase();
            DispatchInputEventToClient(cb, 0x70554C4D, mx, my); // 'MLUp'

            HudButtonId hitBtn = HitTestHudButton(mx, my, winW, winH);
            HudButtonId btnToExecute = HUD_BTN_NONE;
            if (g_pressedHudButton != (int)HUD_BTN_NONE) {
                if (hitBtn == (HudButtonId)g_pressedHudButton || hitBtn != HUD_BTN_NONE) {
                    btnToExecute = (hitBtn != HUD_BTN_NONE) ? hitBtn : (HudButtonId)g_pressedHudButton;
                }
            } else if (hitBtn != HUD_BTN_NONE) {
                btnToExecute = hitBtn;
            }

            if (btnToExecute != HUD_BTN_NONE) {
                ExecuteHudButtonAction(cb, btnToExecute, mx, my);
            } else if (!IsPointOverAnyHud(mx, my, winW, winH)) {
                if (my >= 60 && my <= winH - 90 && mx >= 10 && mx <= winW - 10) {
                    if (mx < winW / 2) {
                        SetTargetOperative(cb, "Emergency Hardline <Phone Booth>", 152, 16645.0, SPAWN_GROUND_ELEVATION, 3242.0, 50, 100);
                    } else {
                        SetTargetOperative(cb, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01, 50, 100);
                    }
                }
            }
            g_pressedHudButton = (int)HUD_BTN_NONE;
            g_bMouseDownOnUI = false;

            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            NeutralizeDragGlobals();
            return lRes;
        }
        case WM_RBUTTONDOWN: {
            NeutralizeDragGlobals();
            SetFocus(hWnd);
            SetActiveWindow(hWnd);
            g_bRightMouseDown = true;
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_lastMouseX = mx;
            g_lastMouseY = my;
            g_bHumanInputActive = true;
            Log("[mxohax] WM_RBUTTONDOWN: (%d, %d)\n", mx, my);
            uintptr_t cb = GetSafeClientBase();
            DispatchInputEventToClient(cb, 0x6E44524D, mx, my); // 'MRDn'
            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            SetCapture(hWnd);
            NeutralizeDragGlobals();
            return lRes;
        }
        case WM_RBUTTONUP: {
            NeutralizeDragGlobals();
            g_bRightMouseDown = false;
            if (!g_bLeftMouseDown && !g_bRightMouseDown && GetCapture() == hWnd) {
                ReleaseCapture();
            }
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_bHumanInputActive = true;
            Log("[mxohax] WM_RBUTTONUP: (%d, %d)\n", mx, my);
            uintptr_t cb = GetSafeClientBase();
            DispatchInputEventToClient(cb, 0x7055524D, mx, my); // 'MRUp'
            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            NeutralizeDragGlobals();
            return lRes;
        }
        case WM_CAPTURECHANGED: {
            g_bMouseDownOnUI = false;
            g_bLeftMouseDown = false;
            g_bRightMouseDown = false;
            g_pressedHudButton = (int)HUD_BTN_NONE;
            NeutralizeDragGlobals();
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_camDist -= (delta / 120.0f) * 20.0f;
            if (g_camDist < 60.0f) g_camDist = 60.0f;
            if (g_camDist > 500.0f) g_camDist = 500.0f;
            g_bHumanInputActive = true;
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_KEYDOWN: {
            g_bHumanInputActive = true;
            Log("[mxohax] WM_KEYDOWN: key=%u ('%c')\n", (DWORD)wParam, (wParam >= 32 && wParam < 127) ? (char)wParam : '?');
            if (wParam < 256) {
                s_keysDown[wParam] = true;
                if (wParam >= 'A' && wParam <= 'Z') s_keysDown[wParam + 32] = true;
                if (wParam >= 'a' && wParam <= 'z') s_keysDown[wParam - 32] = true;
            }
            if (wParam == 'P' || wParam == 'p') {
                TriggerPhoneCall(clientBase);
            } else if (wParam == 'F' || wParam == 'f') {
                g_focusModeActive = !g_focusModeActive;
                g_timeDilation = g_focusModeActive ? 0.35f : 1.0f;
                Log("[mxohax] Bullet-Time Focus Mode %s (timeDilation=%.2f)!\n", g_focusModeActive ? "ENGAGED" : "DISENGAGED", g_timeDilation);
            } else if (wParam == 'B' || wParam == 'b') {
                g_bulletDodgeTimer = 1.2f;
                Log("[mxohax] Ballistic projectile detected in proximity! Executing Bullet-Time Focus Limbo Dodge...\n");
            } else if (wParam == 'G' || wParam == 'g') {
                g_codeRainDegradationActive = !g_codeRainDegradationActive;
                Log("[mxohax] Matrix Anomaly Code Rain Degradation toggled: %s\n", g_codeRainDegradationActive ? "ACTIVE" : "INACTIVE");
            } else if (wParam == VK_TAB) {
                static int s_tabCycle = 0;
                s_tabCycle = (s_tabCycle + 1) % 2;
                if (s_tabCycle == 0) {
                    SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01, 50, 100);
                } else {
                    SetTargetOperative(clientBase, "Emergency Hardline <Phone Booth>", 152, 16645.0, SPAWN_GROUND_ELEVATION, 3242.0, 50, 100);
                }
            } else if (wParam >= VK_F1 && wParam <= VK_F5) {
                SetTacticsStance(clientBase, (StanceType)(wParam - VK_F1));
                ExecuteQuickbarAbility(clientBase, (int)(wParam - VK_F1 + 1));
            } else if (wParam >= '1' && wParam <= '9') {
                ExecuteQuickbarAbility(clientBase, (int)(wParam - '0'));
            } else if (wParam == '0') {
                ExecuteQuickbarAbility(clientBase, 10);
            } else if (wParam == VK_ESCAPE) {
                if (g_hasTarget) {
                    g_hasTarget = false;
                    void* pUI = GetCUIPointer(clientBase);
                    if (pUI) {
                        SafeHideControl(clientBase, pUI, 0x22);
                        __try { SafeSetControlVisible(clientBase, pUI, 0x22, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                    Log("[mxohax] TARGET DESELECTED: Target cleared via Escape key\n");
                } else {
                    ExecuteHudButtonAction(clientBase, HUD_BTN_OPTIONS, 0, 0);
                }
            }
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_KEYUP: {
            if (wParam < 256) {
                s_keysDown[wParam] = false;
                if (wParam >= 'A' && wParam <= 'Z') s_keysDown[wParam + 32] = false;
                if (wParam >= 'a' && wParam <= 'z') s_keysDown[wParam - 32] = false;
            }
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_SIZE: {
            int newW = LOWORD(lParam);
            int newH = HIWORD(lParam);
            if (newW > 0 && newH > 0 && clientBase) {
                void* pUI = GetCUIPointer(clientBase);
                if (pUI && s_inWorldSticky) {
                    LockAllHudFrames(clientBase, pUI);
                }
            }
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_CHAR: {
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
    }
    return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

bool InputManagerSubsystem::Initialize(uintptr_t clientBase) {
    Log("[mxohax] InputManagerSubsystem initialized.\n");
    return true;
}

void InputManagerSubsystem::Shutdown() {
    if (g_hGameWindow && OriginalWndProc) {
        SetWindowLongPtrA(g_hGameWindow, GWLP_WNDPROC, (LONG_PTR)OriginalWndProc);
        OriginalWndProc = nullptr;
    }
    Log("[mxohax] InputManagerSubsystem shutdown.\n");
}

void InputManagerSubsystem::Update(float dt) {
}

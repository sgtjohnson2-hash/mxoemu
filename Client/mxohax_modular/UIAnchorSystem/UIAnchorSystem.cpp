#include "UIAnchorSystem.h"
#include "../InputManager/InputManager.h"
#include <stdio.h>

UIAnchorSystemSubsystem g_UIAnchorSystem;

bool g_bAllowControlMove = false;

SetControlVisible_t OriginalSetControlVisible = nullptr;
SetControlPos_t OriginalSetControlPos = nullptr;
CLTWidget_SetPosition_t OriginalWidgetSetPosition = nullptr;
CUI_BeginDrag_t OriginalBeginDrag = nullptr;
CUI_OnDragMove_t OriginalOnDragMove = nullptr;
CUI_BeginResize_t OriginalBeginResize = nullptr;
CUI_OnResizeMove_t OriginalOnResizeMove = nullptr;

static const DWORD s_hudControlIds[] = {
    0x1B, 0x24, 0x02, 0x23, 0x03, 0x27, 0x0E, 0x4D
};

static inline bool IsCoreHudControl(DWORD ctrlId) {
    for (DWORD hid : s_hudControlIds) {
        if (ctrlId == hid) return true;
    }
    return false;
}

void __fastcall DetourSetControlVisible(void* pUI, void* /*edx*/, DWORD ctrlId, BOOL bVisible) {
    if (ctrlId == 0x5D || ctrlId == 0x4B || ctrlId == 0x30 || ctrlId == 0x04 || ctrlId == 0x57 || ctrlId == 0x42 || ctrlId == 0x47 || ctrlId == 0x22 || ctrlId == 0x3D || ctrlId == 0x0E) {
        Log("[mxohax] SetControlVisible: 0x%02X (bVisible=%d)\n", ctrlId, bVisible ? 1 : 0);
    }
    if (ctrlId == 0x5D) {
        void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
        BYTE realCount = pMarginMgr ? *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x640) : 0;
        if (bVisible && (realCount > 0 || g_AutoJackInRequested || g_CommandLineCharSpecified)) {
            Log("[mxohax] SetControlVisible: Screen 0x5D (Character Creation) SUPPRESSED (realCount=%u, autoJack=%d, charSpec=%d)!\n",
                realCount, g_AutoJackInRequested ? 1 : 0, g_CommandLineCharSpecified ? 1 : 0);
            return;
        }
        s_screen5DActive = (bVisible != FALSE);
        if (bVisible) {
            s_screen5DFrames = 0;
            s_screen5DEverOpened = true;
            Log("[mxohax] Screen 0x5D (Character Creation: CLARSICharCreateView) became active!\n");
        } else {
            Log("[mxohax] Screen 0x5D (Character Creation: CLARSICharCreateView) hidden!\n");
        }
    } else if (ctrlId == 0x4B) {
        s_screen4BActive = (bVisible != FALSE);
        Log("[mxohax] Screen 0x4B (Character Selection: ViewPlayerSelection) visible=%d\n", bVisible ? 1 : 0);
    }

    if (s_inWorldSticky && bVisible && (ctrlId == 0x30 || ctrlId == 0x5D || ctrlId == 0x4B || ctrlId == 0x04 || ctrlId == 0x57)) {
        Log("[mxohax] SetControlVisible: 0x%02X suppressed while in-world!\n", ctrlId);
        return;
    }
    if (s_inWorldSticky && bVisible && !s_charSheetVisible && ctrlId == 0x42) {
        Log("[mxohax] SetControlVisible: 0x42 (Character Details) suppressed while in-world!\n");
        return;
    }
    if (s_inWorldSticky && bVisible && !s_optionsVisible && ctrlId == 0x47) {
        Log("[mxohax] SetControlVisible: 0x47 (Options) suppressed while in-world!\n");
        return;
    }
    if (s_inWorldSticky && bVisible && !g_hasTarget && (ctrlId == 0x22 || ctrlId == 0x3D)) {
        Log("[mxohax] SetControlVisible: 0x%02X suppressed because no target is active (prevents white box)!\n", ctrlId);
        return;
    }
    if (s_inWorldSticky && bVisible && ctrlId == 0x0E) {
        if (OriginalSetControlVisible) OriginalSetControlVisible(pUI, ctrlId, bVisible);
        return;
    }
    if (s_inWorldSticky && !bVisible && IsCoreHudControl(ctrlId)) {
        Log("[mxohax] SetControlVisible: 0x%02X (bVisible=0) suppressed while in-world to preserve retail HUD!\n", ctrlId);
        return;
    }
    if (OriginalSetControlVisible) OriginalSetControlVisible(pUI, ctrlId, bVisible);
}

void __fastcall DetourSetControlPos(void* pControl, void* /*edx*/, const int* pt) {
    if (!pControl || !pt) return;
    if (s_inWorldSticky && !g_bAllowControlMove) {
        return;
    }
    if (OriginalSetControlPos) OriginalSetControlPos(pControl, pt);
}

int __fastcall DetourWidgetSetPosition(void* pThis, void* /*edx*/, int x, int y, void* pRel, int bMoveChildren) {
    if (!pThis) return 0;
    uintptr_t clientBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    if (clientBase && !IsValidWidget(clientBase, pThis)) return 0;
    if (s_inWorldSticky && !g_bAllowControlMove) {
        return 0;
    }
    if (OriginalWidgetSetPosition) {
        __try {
            return OriginalWidgetSetPosition(pThis, x, y, pRel, bMoveChildren);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }
    return 0;
}

void __cdecl DetourBeginDrag(const int* /*pt*/, void* /*pControl*/) {
    NeutralizeDragGlobals();
}

void __cdecl DetourOnDragMove(const int* /*pt*/, void* /*pControl*/) {
    NeutralizeDragGlobals();
}

void __cdecl DetourBeginResize(const int* /*pt*/, void* /*pControl*/) {
    NeutralizeDragGlobals();
}

void __cdecl DetourOnResizeMove(const int* /*pt*/, void* /*pControl*/) {
    NeutralizeDragGlobals();
}

typedef void (__thiscall *HideControl_t)(void* pUI, DWORD ctrlId);
static HideControl_t OriginalHideControl = nullptr;

typedef void (__thiscall *MissionContactCallFn)(void* pThis);
static MissionContactCallFn Original_MissionContact_Button_Call = nullptr;

typedef void (__thiscall *SendCallContactFn)(void* pThis, DWORD contactId);
static SendCallContactFn Original_SendCallContactPacket = nullptr;

typedef void (__thiscall *ViewMissionContactDtor_t)(void* pThis);
static ViewMissionContactDtor_t OriginalViewMissionContactDtor = nullptr;

typedef void (__thiscall *InterlockButtonFn)(void* pThis);
static InterlockButtonFn Original_Interlock_Speed_Button = nullptr;
static InterlockButtonFn Original_Interlock_Power_Button = nullptr;
static InterlockButtonFn Original_Interlock_Grab_Button = nullptr;
static InterlockButtonFn Original_Interlock_Block_Button = nullptr;

bool IsValidWidget(uintptr_t clientBase, void* pWidget) {
    if (!pWidget || !clientBase || IsBadReadPtr(pWidget, sizeof(void*))) return false;
    void** vtbl = *reinterpret_cast<void***>(pWidget);
    if (!vtbl || IsBadReadPtr(vtbl, 0x100)) return false;
    uintptr_t v0 = reinterpret_cast<uintptr_t>(vtbl[0]);
    return (v0 >= clientBase && v0 < clientBase + 0x1000000);
}

void SafeSetControlVisible(uintptr_t clientBase, void* pUI, DWORD ctrlId, bool bVisible) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    SetControlVisible_t pSetVisible = OriginalSetControlVisible ? OriginalSetControlVisible : reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
    __try {
        pSetVisible(pUI, ctrlId, bVisible ? 1 : 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void SafeHideControl(uintptr_t clientBase, void* pUI, DWORD ctrlId) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    HideControl_t pHide = OriginalHideControl ? OriginalHideControl : reinterpret_cast<HideControl_t>(clientBase + 0x0001DC30);
    __try {
        pHide(pUI, ctrlId);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void* GetControlRootWidget(uintptr_t clientBase, void* pCtrl, DWORD ctrlId) {
    if (!pCtrl || !clientBase || IsBadReadPtr(pCtrl, 0xF0)) return nullptr;

    switch (ctrlId) {
        case 0x1B: {
            void* pE0 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0xE0);
            if (IsValidWidget(clientBase, pE0)) return pE0;
            break;
        }
        case 0x22: {
            void* p13C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x13C);
            if (IsValidWidget(clientBase, p13C)) return p13C;
            break;
        }
        case 0x27: {
            void* p68 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x68);
            if (IsValidWidget(clientBase, p68)) return p68;
            void* p6C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x6C);
            if (IsValidWidget(clientBase, p6C)) return p6C;
            break;
        }
        case 0x02: {
            void* p54 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x54);
            if (IsValidWidget(clientBase, p54)) return p54;
            break;
        }
        case 0x23: {
            void* p60 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x60);
            if (IsValidWidget(clientBase, p60)) return p60;
            break;
        }
        case 0x03: {
            void* p70 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x70);
            if (IsValidWidget(clientBase, p70)) return p70;
            break;
        }
        case 0x4D: {
            void* pC0 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0xC0);
            if (IsValidWidget(clientBase, pC0)) return pC0;
            break;
        }
        case 0x3D: {
            void* p60 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x60);
            if (IsValidWidget(clientBase, p60)) return p60;
            break;
        }
        default:
            break;
    }

    uintptr_t pLayout50 = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pCtrl) + 0x50);
    if (pLayout50 && !IsBadReadPtr(reinterpret_cast<void*>(pLayout50), 0x30)) {
        void* pLayoutRoot = *reinterpret_cast<void**>(pLayout50 + 0x24);
        if (IsValidWidget(clientBase, pLayoutRoot)) return pLayoutRoot;
    }

    uintptr_t pLayout7C = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pCtrl) + 0x7C);
    if (pLayout7C && !IsBadReadPtr(reinterpret_cast<void*>(pLayout7C), 0x30)) {
        void* pLayoutRoot = *reinterpret_cast<void**>(pLayout7C + 0x24);
        if (IsValidWidget(clientBase, pLayoutRoot)) return pLayoutRoot;
    }

    void* p54 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x54);
    if (IsValidWidget(clientBase, p54)) return p54;

    return nullptr;
}

void PositionControlAndWidget(uintptr_t clientBase, void* pUI, DWORD ctrlId, int left, int top, int width, int height) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    if (ctrlId == 0x24 || ctrlId == 0x0E || ctrlId == 0x27) return;

    void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (ctrlId * 4));
    if (!ppCtrl || !*ppCtrl || IsBadReadPtr(*ppCtrl, 0x60)) return;
    void* pCtrl = *ppCtrl;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    void* pWidget = GetControlRootWidget(clientBase, pCtrl, ctrlId);
    if (pWidget && IsValidWidget(clientBase, pWidget)) {
        __try {
            g_bAllowControlMove = true;
            pSetPosition(pWidget, left, top, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x6C) = left;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x70) = top;
            g_bAllowControlMove = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bAllowControlMove = false;
        }
    }
}

void RepositionQuickbar(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppQuickbar = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x24 * 4));
    if (!ppQuickbar || !*ppQuickbar || IsBadReadPtr(*ppQuickbar, 0x100)) return;
    void* pQuickbar = *ppQuickbar;

    const int qbW = 428;
    int targetX = (screenW - qbW) / 2;
    int targetY = screenH - 221;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    void* pRootWidget = GetControlRootWidget(clientBase, pQuickbar, 0x24);
    if (pRootWidget && IsValidWidget(clientBase, pRootWidget)) {
        __try {
            g_bAllowControlMove = true;
            pSetPosition(pRootWidget, targetX, targetY, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x6C) = targetX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x70) = targetY;
            g_bAllowControlMove = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bAllowControlMove = false;
        }
    }
}

void RepositionCompass(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppCompass = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x27 * 4));
    if (!ppCompass || !*ppCompass || IsBadReadPtr(*ppCompass, 0x100)) return;
    void* pCompass = *ppCompass;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    const int compW = 150;
    const int compH = 150;
    int targetX = (screenW - compW) / 2;
    int targetY = screenH - 143;

    void* pRootWidget = GetControlRootWidget(clientBase, pCompass, 0x27);
    if (pRootWidget && IsValidWidget(clientBase, pRootWidget)) {
        __try {
            g_bAllowControlMove = true;
            pSetPosition(pRootWidget, targetX, targetY, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x6C) = targetX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x70) = targetY;
            g_bAllowControlMove = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bAllowControlMove = false;
        }
    }
}

void RepositionCombatTactics(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppTactics = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x0E * 4));
    if (!ppTactics || !*ppTactics || IsBadReadPtr(*ppTactics, 0x100)) return;
    void* pTactics = *ppTactics;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    const int tacW = 134;
    int targetX = (screenW - tacW) / 2;
    int targetY = screenH - 167;

    void* pRootWidget = GetControlRootWidget(clientBase, pTactics, 0x0E);
    if (pRootWidget && IsValidWidget(clientBase, pRootWidget)) {
        __try {
            g_bAllowControlMove = true;
            pSetPosition(pRootWidget, targetX, targetY, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x6C) = targetX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x70) = targetY;
            g_bAllowControlMove = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bAllowControlMove = false;
        }
    }
}

void DockAllHUDControls(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase) return;
    if (screenW <= 0) screenW = 1920;
    if (screenH <= 0) screenH = 1080;

    PositionControlAndWidget(clientBase, pUI, 0x1B, 5, 5, 260, 100);
    PositionControlAndWidget(clientBase, pUI, 0x02, 5, screenH - 330, 480, 260);
    PositionControlAndWidget(clientBase, pUI, 0x23, 5, screenH - 355, 480, 25);
    PositionControlAndWidget(clientBase, pUI, 0x03, 5, screenH - 65, 480, 30);
    PositionControlAndWidget(clientBase, pUI, 0x4D, screenW - 80, screenH - 35, 70, 30);

    RepositionQuickbar(clientBase, pUI, screenW, screenH);
    RepositionCompass(clientBase, pUI, screenW, screenH);
    RepositionCombatTactics(clientBase, pUI, screenW, screenH);
}

void LockAllHudFrames(uintptr_t clientBase, void* pUI) {
    if (!pUI || !clientBase) return;
    HWND hWnd = g_hGameWindow;
    int screenW = 1920, screenH = 1080;
    if (hWnd && IsWindow(hWnd)) {
        RECT rc;
        if (GetClientRect(hWnd, &rc) && (rc.right - rc.left) > 0) {
            screenW = rc.right - rc.left;
            screenH = rc.bottom - rc.top;
        }
    }
    DockAllHUDControls(clientBase, pUI, screenW, screenH);
}

void __fastcall Safe_Interlock_Speed_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Speed_Button (+0x6C Focus/Free) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_FREE;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x6C);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) return;
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) return;
    __try {
        if (Original_Interlock_Speed_Button) Original_Interlock_Speed_Button(pThis);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void __fastcall Safe_Interlock_Power_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Power_Button (+0x70 Power) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_POWER;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x70);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) return;
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) return;
    __try {
        if (Original_Interlock_Power_Button) Original_Interlock_Power_Button(pThis);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void __fastcall Safe_Interlock_Grab_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Grab_Button (+0x74 Grab/Attack) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_GRAB;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x74);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) return;
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) return;
    __try {
        if (Original_Interlock_Grab_Button) Original_Interlock_Grab_Button(pThis);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void __fastcall Safe_Interlock_Block_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Block_Button (+0x80/+0x7C Block/Defense) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_WITHDRAW;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x80);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x7C);
    }
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) return;
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) return;
    __try {
        if (Original_Interlock_Block_Button) Original_Interlock_Block_Button(pThis);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void __fastcall Safe_ViewMissionContact_Dtor(void* pThis, void* /*edx*/) {
    if (!pThis || IsBadReadPtr(pThis, 0x80)) return;
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) return;
    uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClient);
    *reinterpret_cast<void**>(pThis) = reinterpret_cast<void*>(clientBase + 0x00757F4C);

    void* p64 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x64);
    if (p64) {
        void** ppMgr = *reinterpret_cast<void***>(clientBase + 0x008C3CE8);
        if (ppMgr && !IsBadReadPtr(ppMgr, sizeof(void*)) && *ppMgr) {
            void** vtbl = *reinterpret_cast<void***>(*ppMgr);
            if (vtbl && !IsBadReadPtr(vtbl, 0x20)) {
                typedef void (__thiscall *Fn18)(void* pMgr, void* pArg);
                Fn18 fn = reinterpret_cast<Fn18>(vtbl[0x18 / 4]);
                if (fn) fn(*ppMgr, p64);
            }
        }
    }
}

void __fastcall Safe_MissionContact_Button_Call(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_MissionContact_Button_Call called (pThis=0x%p)\n", pThis);
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) return;
    uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClient);

    SendCallContactFn pSendCall = Original_SendCallContactPacket ? Original_SendCallContactPacket : reinterpret_cast<SendCallContactFn>(clientBase + 0x0018C3A0);
    void* pContactMgr = reinterpret_cast<void*>(clientBase + 0x008A2440);
    __try {
        if (pContactMgr && !IsBadReadPtr(pContactMgr, 4)) {
            pSendCall(pContactMgr, 1);
            Log("[mxohax] Safe_MissionContact_Button_Call: dispatched SendCallContactPacket(contactId=1)\n");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* p8C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x8C);
        if (p8C && !IsBadReadPtr(p8C, sizeof(void*))) {
            void** vtbl8C = *reinterpret_cast<void***>(p8C);
            if (vtbl8C && !IsBadReadPtr(vtbl8C, 0xA0)) {
                typedef void (__thiscall *Fn98)(void*, int);
                Fn98 fn98 = reinterpret_cast<Fn98>(vtbl8C[0x98 / 4]);
                if (fn98) fn98(p8C, 2);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* p70 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x70);
        if (p70 && !IsBadReadPtr(p70, 0x20)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(p70) + 0x18) = 0;
            void** vtbl70 = *reinterpret_cast<void***>(p70);
            if (vtbl70 && !IsBadReadPtr(vtbl70, 0x150)) {
                typedef void (__thiscall *Fn144)(void*, int);
                Fn144 fn144 = reinterpret_cast<Fn144>(vtbl70[0x144 / 4]);
                if (fn144) fn144(p70, 0x1000003);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void __fastcall Safe_SendCallPacket(void* pThis, void* /*edx*/, DWORD contactId) {
    if (!pThis || IsBadReadPtr(pThis, 0x20)) return;
    __try {
        if (Original_SendCallContactPacket) {
            Original_SendCallContactPacket(pThis, contactId);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

bool UIAnchorSystemSubsystem::Initialize(uintptr_t clientBase) {
    if (!clientBase) return false;

    LPVOID pSetCtrlVis = reinterpret_cast<LPVOID>(clientBase + 0x0001DB80);
    if (!OriginalSetControlVisible && MH_CreateHook(pSetCtrlVis, &DetourSetControlVisible, reinterpret_cast<LPVOID*>(&OriginalSetControlVisible)) == MH_OK) {
        MH_EnableHook(pSetCtrlVis);
        Log("[mxohax] UIAnchor: hooked SetControlVisible at 0x%p\n", pSetCtrlVis);
    }

    LPVOID pSetCtrlPos = reinterpret_cast<LPVOID>(clientBase + 0x00015D60);
    if (!OriginalSetControlPos && MH_CreateHook(pSetCtrlPos, &DetourSetControlPos, reinterpret_cast<LPVOID*>(&OriginalSetControlPos)) == MH_OK) {
        MH_EnableHook(pSetCtrlPos);
        Log("[mxohax] UIAnchor: hooked SetControlPos at 0x%p\n", pSetCtrlPos);
    }

    LPVOID pSetWidgetPos = reinterpret_cast<LPVOID>(clientBase + 0x00382360);
    if (!OriginalWidgetSetPosition && MH_CreateHook(pSetWidgetPos, &DetourWidgetSetPosition, reinterpret_cast<LPVOID*>(&OriginalWidgetSetPosition)) == MH_OK) {
        MH_EnableHook(pSetWidgetPos);
        Log("[mxohax] UIAnchor: hooked CLTWidget::SetPosition at 0x%p\n", pSetWidgetPos);
    }

    LPVOID pStartDrag = reinterpret_cast<LPVOID>(clientBase + 0x000184E0);
    if (!OriginalBeginDrag && MH_CreateHook(pStartDrag, &DetourBeginDrag, reinterpret_cast<LPVOID*>(&OriginalBeginDrag)) == MH_OK) {
        MH_EnableHook(pStartDrag);
        Log("[mxohax] UIAnchor: hooked CUI::BeginDrag at 0x%p\n", pStartDrag);
    }

    LPVOID pOnDragMove = reinterpret_cast<LPVOID>(clientBase + 0x00018540);
    if (!OriginalOnDragMove && MH_CreateHook(pOnDragMove, &DetourOnDragMove, reinterpret_cast<LPVOID*>(&OriginalOnDragMove)) == MH_OK) {
        MH_EnableHook(pOnDragMove);
        Log("[mxohax] UIAnchor: hooked CUI::OnDragMove at 0x%p\n", pOnDragMove);
    }

    LPVOID pStartResize = reinterpret_cast<LPVOID>(clientBase + 0x00018590);
    if (!OriginalBeginResize && MH_CreateHook(pStartResize, &DetourBeginResize, reinterpret_cast<LPVOID*>(&OriginalBeginResize)) == MH_OK) {
        MH_EnableHook(pStartResize);
        Log("[mxohax] UIAnchor: hooked CUI::BeginResize at 0x%p\n", pStartResize);
    }

    LPVOID pOnResizeMove = reinterpret_cast<LPVOID>(clientBase + 0x000187B0);
    if (!OriginalOnResizeMove && MH_CreateHook(pOnResizeMove, &DetourOnResizeMove, reinterpret_cast<LPVOID*>(&OriginalOnResizeMove)) == MH_OK) {
        MH_EnableHook(pOnResizeMove);
        Log("[mxohax] UIAnchor: hooked CUI::UpdateResize at 0x%p\n", pOnResizeMove);
    }

    return true;
}

void UIAnchorSystemSubsystem::Shutdown() {
    Log("[mxohax] UIAnchorSystemSubsystem shutdown.\n");
}

void UIAnchorSystemSubsystem::Update(float dt) {
}

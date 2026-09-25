#pragma once

#include "../Common/Common.h"
#include "../Common/Subsystem.h"

// ============================================================================
// HUD Interactive Button & Tactics Enumerations
// ============================================================================

enum HudButtonId {
    HUD_BTN_NONE = 0,
    HUD_BTN_QB_PAGE,
    HUD_BTN_QB_1,
    HUD_BTN_QB_2,
    HUD_BTN_QB_3,
    HUD_BTN_QB_4,
    HUD_BTN_QB_5,
    HUD_BTN_QB_6,
    HUD_BTN_QB_7,
    HUD_BTN_QB_8,
    HUD_BTN_QB_9,
    HUD_BTN_QB_10,
    HUD_BTN_TACTIC_FREE,
    HUD_BTN_TACTIC_POWER,
    HUD_BTN_TACTIC_GRAB,
    HUD_BTN_TACTIC_SPEED,
    HUD_BTN_TACTIC_WITHDRAW,
    HUD_BTN_CELL_PHONE,
    HUD_BTN_CHAR_STATUS,
    HUD_BTN_COMPASS,
    HUD_BTN_OPTIONS,
    HUD_BTN_LATENCY,
    HUD_BTN_TARGET_VITALS
};

enum StanceType {
    STANCE_FREE = 0,
    STANCE_POWER = 1,
    STANCE_GRAB = 2,
    STANCE_SPEED = 3,
    STANCE_WITHDRAW = 4
};

// ============================================================================
// Shared Input & Camera State
// ============================================================================

extern StanceType g_currentStance;
extern bool       s_keysDown[256];
extern float      g_camPitch;
extern float      g_camYaw;
extern float      g_camDist;
extern int        g_lastMouseX;
extern int        g_lastMouseY;
extern bool       g_bRightMouseDown;
extern bool       g_bLeftMouseDown;
extern bool       g_bHumanInputActive;
extern bool       g_bPlayerIsMoving;
extern bool       g_bMouseDownOnUI;
extern int        g_pressedHudButton;
extern int        g_quickbarPage;
extern float      g_bulletDodgeTimer;
extern WNDPROC    OriginalWndProc;

// UI Dialog visibility state
extern bool s_charSheetVisible;
extern bool s_optionsVisible;

// ============================================================================
// Input Management Functions
// ============================================================================

void SubclassGameWindow(HWND hWnd);

void DispatchInputEventToClient(uintptr_t clientBase, DWORD eventCode, int x, int y);
void* GetHoveredUIWidget(uintptr_t clientBase);
void SetWidgetVisualState(void* pWidget, int state);

HudButtonId HitTestGeometry(int testX, int testY, int w, int h);
HudButtonId HitTestHudButton(int x, int y, int screenW = 1920, int screenH = 1080);
bool IsPointInAnyHudRect(int normX, int normY);
bool IsPointOverAnyHud(int mx, int my, int winW = 1920, int winH = 1080);

void ExecuteHudButtonAction(uintptr_t clientBase, HudButtonId btnId, int mx, int my);
void ExecuteQuickbarAbility(uintptr_t clientBase, int slotIndex);
void SetTacticsStance(uintptr_t clientBase, StanceType stance);
void SetTargetOperative(uintptr_t clientBase, const char* name, DWORD charId, double x, double y, double z, int minDmg = 45, int maxDmg = 90);
void TriggerPhoneCall(uintptr_t clientBase);

class InputManagerSubsystem : public IClientSubsystem {
public:
    const char* GetName() const override { return "InputManager"; }
    bool Initialize(uintptr_t clientBase) override;
    void Shutdown() override;
    void Update(float dt) override;
};

extern InputManagerSubsystem g_InputManager;

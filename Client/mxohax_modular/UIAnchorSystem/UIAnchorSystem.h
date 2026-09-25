#pragma once

#include "../Common/Common.h"
#include "../Common/Subsystem.h"

// ============================================================================
// UI Anchor & 0px Drift Prevention Subsystem
// ============================================================================

extern bool g_bAllowControlMove;

bool IsValidWidget(uintptr_t clientBase, void* pWidget);
void* GetControlRootWidget(uintptr_t clientBase, void* pCtrl, DWORD ctrlId);
void PositionControlAndWidget(uintptr_t clientBase, void* pUI, DWORD ctrlId, int left, int top, int width, int height);
void RepositionQuickbar(uintptr_t clientBase, void* pUI, int screenW, int screenH);
void RepositionCompass(uintptr_t clientBase, void* pUI, int screenW, int screenH);
void RepositionCombatTactics(uintptr_t clientBase, void* pUI, int screenW, int screenH);
void LockAllHudFrames(uintptr_t clientBase, void* pUI);
void DockAllHUDControls(uintptr_t clientBase, void* pUI, int screenW, int screenH);
void SafeSetControlVisible(uintptr_t clientBase, void* pUI, DWORD ctrlId, bool bVisible);
void SafeHideControl(uintptr_t clientBase, void* pUI, DWORD ctrlId);

// Detour functions and original pointers
typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
extern SetControlVisible_t OriginalSetControlVisible;
void __fastcall DetourSetControlVisible(void* pUI, void* edx, DWORD ctrlId, BOOL bVisible);

typedef void (__thiscall *SetControlPos_t)(void* pControl, const int* pt);
extern SetControlPos_t OriginalSetControlPos;
void __fastcall DetourSetControlPos(void* pControl, void* edx, const int* pt);

typedef int (__thiscall *CLTWidget_SetPosition_t)(void* pThis, int x, int y, void* pRel, int bMoveChildren);
extern CLTWidget_SetPosition_t OriginalWidgetSetPosition;
int __fastcall DetourWidgetSetPosition(void* pThis, void* edx, int x, int y, void* pRel, int bMoveChildren);

typedef void (__cdecl *CUI_BeginDrag_t)(const int* pt, void* pControl);
extern CUI_BeginDrag_t OriginalBeginDrag;
void __cdecl DetourBeginDrag(const int* pt, void* pControl);

typedef void (__cdecl *CUI_OnDragMove_t)(const int* pt, void* pControl);
extern CUI_OnDragMove_t OriginalOnDragMove;
void __cdecl DetourOnDragMove(const int* pt, void* pControl);

typedef void (__cdecl *CUI_BeginResize_t)(const int* pt, void* pControl);
extern CUI_BeginResize_t OriginalBeginResize;
void __cdecl DetourBeginResize(const int* pt, void* pControl);

typedef void (__cdecl *CUI_OnResizeMove_t)(const int* pt, void* pControl);
extern CUI_OnResizeMove_t OriginalOnResizeMove;
void __cdecl DetourOnResizeMove(const int* pt, void* pControl);

// Interlock and Contact Button Hooks
void __fastcall Safe_Interlock_Speed_Button(void* pThis, void* /*edx*/);
void __fastcall Safe_Interlock_Power_Button(void* pThis, void* /*edx*/);
void __fastcall Safe_Interlock_Grab_Button(void* pThis, void* /*edx*/);
void __fastcall Safe_Interlock_Block_Button(void* pThis, void* /*edx*/);
void __fastcall Safe_ViewMissionContact_Dtor(void* pThis, void* /*edx*/);
void __fastcall Safe_MissionContact_Button_Call(void* pThis, void* /*edx*/);
void __fastcall Safe_SendCallPacket(void* pThis, void* /*edx*/, DWORD contactId);

class UIAnchorSystemSubsystem : public IClientSubsystem {
public:
    const char* GetName() const override { return "UIAnchorSystem"; }
    bool Initialize(uintptr_t clientBase) override;
    void Shutdown() override;
    void Update(float dt) override;
};

extern UIAnchorSystemSubsystem g_UIAnchorSystem;

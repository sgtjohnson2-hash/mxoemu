#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <intrin.h>

#include "minhook/include/MinHook.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

typedef unsigned char byte;

// ============================================================================
// Global State & Shared Variables Declarations
// ============================================================================

extern uintptr_t g_clientBase;
extern char g_TargetServerIp[64];
extern char g_ActiveCharName[64];
extern uint32_t g_ActiveCharId;
extern bool g_CommandLineParsed;
extern bool g_AutoJackInRequested;
extern bool g_CommandLineCharSpecified;

extern HWND g_hGameWindow;

// State transition and in-world flags
extern volatile bool s_inWorldSticky;
extern volatile bool s_inStreamingState4;
extern volatile bool s_screen5DActive;
extern volatile bool s_screen5DEverOpened;
extern volatile int  s_screen5DFrames;
extern volatile bool s_charCreationSubmitted;
extern volatile int  s_charCreationTicks;
extern volatile int  s_state4Ticks;
extern volatile int  s_marginState4Ticks;
extern volatile bool s_screen4BActive;
extern volatile bool s_interactiveSelectTriggered;
extern volatile int  s_interactiveSelectTicks;
extern volatile bool s_worldTransitionDone;
extern volatile bool s_autoJackInDone;
extern volatile bool s_playerEnteredWorld;

// Targeting system state
extern DWORD  g_targetCharId;
extern char   g_targetName[64];
extern double g_targetX;
extern double g_targetY;
extern double g_targetZ;
extern bool   g_hasTarget;

// Calibrated World Elevation Constants
static const double SPAWN_GROUND_ELEVATION = 603.5; // Calibrated ground elevation flush with Slums concourse pavement tiles
static const double MAX_STEP_HEIGHT = 18.0;        // Authentic LithTech standard stair/curb step height

// ============================================================================
// Synthetic Operative Data Structures
// ============================================================================
#pragma pack(push, 1)
struct MxoCharacterData {
    DWORD charId;             // 0x00: 359
    DWORD unknown04;          // 0x04: 0
    DWORD unknown08;          // 0x08: 0
    DWORD unknown0C;          // 0x0C: 0
    DWORD unknown10;          // 0x10: 0
    char  firstName[32];      // 0x14: "S1acker"
    char  lastName[32];       // 0x34: ""
    DWORD bodyType;           // 0x54: 106 (RSIMBody001 Male Operative)
    DWORD headType;           // 0x58: 103 (RSIMHead001 Male Head)
    DWORD hairType;           // 0x5C: 105 (RSIMHair001 Male Hair)
    DWORD worldId;            // 0x60: 1
    DWORD handle;             // 0x64: 359
    BYTE  padding[256];
};

struct MxoCharRecord {
    BYTE     pad[3];          // 0x00..0x02
    uint32_t charIdLow;       // 0x03..0x06: 359
    uint32_t charIdHigh;      // 0x07..0x0A: 0
    BYTE     pad2;            // 0x0B
    uint16_t worldId;         // 0x0C..0x0D: 1
    BYTE     extra[32];
};

struct MxoConnParams {
    BYTE     pad0;            // 0x00: 0
    WORD     worldId;         // 0x01..0x02: 1
    char     serverIp[32];    // 0x03..0x22: target server IP
    WORD     serverPort;      // 10000
    BYTE     extra[32];
};

struct MxoCharObj {
    void**         pVtbl;       // 0x00..0x03
    BYTE           pad1[0x0C];  // 0x04..0x0F
    MxoCharRecord* pRecord;     // 0x10: Pointer to MxoCharRecord
    const char*    pCharName;   // 0x14: Pointer to character name string
    BYTE           pad2[0x20];
};

struct MxoConnObj {
    void**         pVtbl;       // 0x00..0x03
    BYTE           pad1[0x0C];  // 0x04..0x0F
    MxoConnParams* pConnParams; // 0x10: Pointer to MxoConnParams
    BYTE           pad2[0x20];
};
#pragma pack(pop)

// ============================================================================
// Core Logging & Memory Safety Utilities
// ============================================================================

inline void Log(const char* fmt, ...) {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\mxohax.log", "a");
    if (!f) f = fopen("mxohax.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
}

inline bool IsSafeReadPointer(const void* ptr, size_t size) {
    if (!ptr || (uintptr_t)ptr < 0x10000 || (uintptr_t)ptr >= 0x7FFE0000) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    uintptr_t end = (uintptr_t)ptr + size;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (end > regionEnd) {
        return IsSafeReadPointer((const void*)regionEnd, end - regionEnd);
    }
    return true;
}

#undef IsBadReadPtr
#define IsBadReadPtr(ptr, sz) (!IsSafeReadPointer(ptr, sz))

inline bool SafeWriteFloat(void* ptr, float val) {
    if (!ptr || (uintptr_t)ptr < 0x10000 || (uintptr_t)ptr >= 0x7FFE0000) return false;
    __try {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != 0 && mbi.State == MEM_COMMIT) {
            if (!(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))) {
                DWORD oldProt = 0;
                VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldProt);
            }
            *reinterpret_cast<float*>(ptr) = val;
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return false;
}

inline uintptr_t GetSafeClientBase() {
    if (g_clientBase && g_clientBase >= 0x1000000 && g_clientBase < 0x7FFE0000) return g_clientBase;
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (hClient) {
        g_clientBase = reinterpret_cast<uintptr_t>(hClient);
        return g_clientBase;
    }
    return 0;
}

inline void* GetCUIPointer(uintptr_t clientBase = 0) {
    if (!clientBase || clientBase < 0x1000000 || clientBase >= 0x7FFE0000) {
        clientBase = GetSafeClientBase();
    }
    if (!clientBase) return nullptr;
    void** ppUI = reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (!ppUI || IsBadReadPtr(ppUI, sizeof(void*))) return nullptr;
    void* pUI = *ppUI;
    if (!pUI || IsBadReadPtr(pUI, 0x240)) return nullptr;
    return pUI;
}

inline void SetCVarFloat(uintptr_t cvarBase, float val) {
    if (!cvarBase) return;
    SafeWriteFloat(reinterpret_cast<void*>(cvarBase + 0x1C), val);
    SafeWriteFloat(reinterpret_cast<void*>(cvarBase + 0x24), val);
    SafeWriteFloat(reinterpret_cast<void*>(cvarBase + 0x28), val);
    SafeWriteFloat(reinterpret_cast<void*>(cvarBase + 0x2C), val);
    SafeWriteFloat(reinterpret_cast<void*>(cvarBase + 0x30), val);
}

inline void LoadTargetServerIp() {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\server_ip.txt", "r");
    if (!f) f = fopen("server_ip.txt", "r");
    if (f) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            char* nl = strpbrk(buf, "\r\n");
            if (nl) *nl = '\0';
            if (strlen(buf) > 0) {
                strncpy_s(g_TargetServerIp, sizeof(g_TargetServerIp), buf, _TRUNCATE);
                g_TargetServerIp[sizeof(g_TargetServerIp) - 1] = '\0';
            }
        }
        fclose(f);
    }
    Log("[mxohax] Active Target Server IP: %s\n", g_TargetServerIp);
    HDESK hCurDesk = GetThreadDesktop(GetCurrentThreadId());
    char deskBuf[128] = {0};
    GetUserObjectInformationA(hCurDesk, 2, deskBuf, sizeof(deskBuf), NULL);
    Log("[mxohax] Process Thread Desktop: '%s'\n", deskBuf);
}

inline void ParseClientCommandLine() {
    if (g_CommandLineParsed) return;
    g_CommandLineParsed = true;
    const char* pCmdLine = GetCommandLineA();
    if (!pCmdLine) return;
    Log("[mxohax] Command line: %s\n", pCmdLine);

    if (strstr(pCmdLine, "-autojackin") != nullptr) {
        g_AutoJackInRequested = true;
        Log("[mxohax] -autojackin flag detected!\n");
    }

    const char* pCharFlag = strstr(pCmdLine, "-char ");
    if (pCharFlag) {
        pCharFlag += 6;
        while (*pCharFlag == ' ') pCharFlag++;
        char charBuf[64] = {0};
        int idx = 0;
        while (*pCharFlag && *pCharFlag != ' ' && idx < (int)sizeof(charBuf) - 1) {
            charBuf[idx++] = *pCharFlag++;
        }
        charBuf[idx] = '\0';
        if (strlen(charBuf) > 0) {
            strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), charBuf, _TRUNCATE);
            g_CommandLineCharSpecified = true;
            Log("[mxohax] -char flag specified operative: '%s'\n", g_ActiveCharName);
        }
    }
}

inline int GetMarginStateId() {
    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (!pMarginMgr || IsBadReadPtr(pMarginMgr, 0x10)) return -1;
    void* pStateObj = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x0C);
    if (!pStateObj || IsBadReadPtr(pStateObj, 0x10)) return -1;
    void** vtbl = *reinterpret_cast<void***>(pStateObj);
    if (!vtbl || IsBadReadPtr(vtbl, 0x20)) return -1;
    typedef int (__thiscall *GetStateId_t)(void* pThis);
    GetStateId_t pGetStateId = reinterpret_cast<GetStateId_t>(vtbl[0x14 / 4]);
    __try {
        return pGetStateId(pStateObj);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

inline void NeutralizeDragGlobals(uintptr_t clientBase = 0) {
    if (!clientBase || clientBase < 0x1000000 || clientBase >= 0x7FFE0000) {
        clientBase = GetSafeClientBase();
    }
    if (!clientBase) return;
    __try {
        DWORD* pActiveDrag = reinterpret_cast<DWORD*>(clientBase + 0x00849394);
        if (pActiveDrag && !IsBadWritePtr(pActiveDrag, sizeof(DWORD)) && *pActiveDrag != 0xFFFFFFFF) {
            *pActiveDrag = 0xFFFFFFFF;
        }
        DWORD* pActiveResize = reinterpret_cast<DWORD*>(clientBase + 0x00849390);
        if (pActiveResize && !IsBadWritePtr(pActiveResize, sizeof(DWORD)) && *pActiveResize != 0xFFFFFFFF) {
            *pActiveResize = 0xFFFFFFFF;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <intrin.h>
#include "minhook/include/MinHook.h"

#pragma comment(lib, "ws2_32.lib")

typedef unsigned char byte;

// Target server IP (defaults to live VPS shard)
static char g_TargetServerIp[64] = "15.204.82.250";

static void Log(const char* fmt, ...) {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\mxohax.log", "a");
    if (!f) f = fopen("mxohax.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
}

static LONG WINAPI CrashHandler(PEXCEPTION_POINTERS pExc) {
    if (pExc && pExc->ExceptionRecord) {
        DWORD code = pExc->ExceptionRecord->ExceptionCode;
        if (code == 0xC0000005 || code == 0x80000003) {
            void* addr = pExc->ExceptionRecord->ExceptionAddress;
            CONTEXT* ctx = pExc->ContextRecord;
            HMODULE hClient = GetModuleHandleA("client.dll");
            uintptr_t clientBase = (uintptr_t)hClient;
            HMODULE hMatrix = GetModuleHandleA("matrix.exe");
            uintptr_t matrixBase = (uintptr_t)hMatrix;

            Log("[mxohax] !!! CRASH EXCEPTION 0x%08X at 0x%p !!!\n", code, addr);
            if (clientBase && (uintptr_t)addr >= clientBase && (uintptr_t)addr < clientBase + 0x1000000) {
                Log("[mxohax] Crash is inside client.dll + 0x%08X\n", (uintptr_t)addr - clientBase);
            }
            if (matrixBase && (uintptr_t)addr >= matrixBase && (uintptr_t)addr < matrixBase + 0x1000000) {
                Log("[mxohax] Crash is inside matrix.exe + 0x%08X\n", (uintptr_t)addr - matrixBase);
            }
            if (ctx) {
                Log("[mxohax] EIP: 0x%08X, EAX: 0x%08X, EBX: 0x%08X, ECX: 0x%08X, EDX: 0x%08X\n",
                    ctx->Eip, ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
                Log("[mxohax] ESI: 0x%08X, EDI: 0x%08X, ESP: 0x%08X, EBP: 0x%08X\n",
                    ctx->Esi, ctx->Edi, ctx->Esp, ctx->Ebp);
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void LoadTargetServerIp() {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\server_ip.txt", "r");
    if (!f) f = fopen("server_ip.txt", "r");
    if (f) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf) - 1, f)) {
            char* p = buf;
            while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
            char* end = p + strlen(p) - 1;
            while (end > p && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            if (strlen(p) > 6) {
                strncpy(g_TargetServerIp, p, sizeof(g_TargetServerIp) - 1);
                g_TargetServerIp[sizeof(g_TargetServerIp) - 1] = '\0';
            }
        }
        fclose(f);
    }
    Log("[mxohax] Active Target Server IP: %s\n", g_TargetServerIp);
}

// ============================================================================
// Synthetic Operative Data for s1acker (charId = 360)
// ============================================================================
#pragma pack(push, 1)
struct MxoCharacterData {
    DWORD charId;             // 0x00: 360
    DWORD unknown04;          // 0x04: 0
    DWORD unknown08;          // 0x08: 0
    DWORD unknown0C;          // 0x0C: 0
    DWORD unknown10;          // 0x10: 0
    char  firstName[32];      // 0x14: "s1acker"
    char  lastName[32];       // 0x34: ""
    DWORD bodyType;           // 0x54: 100
    DWORD headType;           // 0x58: 100
    DWORD hairType;           // 0x5C: 101
    DWORD worldId;            // 0x60: 1
    DWORD handle;             // 0x64: 360
    BYTE  padding[256];
};

struct MxoConnParams {
    BYTE  pad[3];             // 0x00..0x02
    char  serverIp[32];       // 0x03..0x22: Null-terminated string ("15.204.82.250")
    WORD  serverPort;         // 10000
    BYTE  extra[32];
};

struct MxoCharObj {
    BYTE pad1[0x10];          // 0x00..0x0F
    MxoConnParams* pConnParams; // 0x10: Pointer to MxoConnParams
    MxoCharacterData* pData;  // 0x14: Pointer to MxoCharacterData
    BYTE pad2[0x20];
};
#pragma pack(pop)

static MxoCharacterData   g_SyntheticChar = {
    360, 0, 0, 0, 0,
    "s1acker", "",
    100, 100, 101, 1, 360
};

static MxoConnParams      g_SyntheticConnParams;
static MxoCharObj         g_SyntheticCharObj;


// ============================================================================
// WinSock Hooks
// ============================================================================
typedef struct hostent* (PASCAL* gethostbyname_t)(const char* name);
static gethostbyname_t OriginalGetHostByName = nullptr;

struct hostent* PASCAL DetourGetHostByName(const char* name) {
    if (name && (strstr(name, "thematrixonline.com") || strstr(name, "matrixonline.com"))) {
        Log("[mxohax] gethostbyname redirected '%s' -> %s\n", name, g_TargetServerIp);
        static struct hostent s_he;
        static char* s_aliases[1] = { NULL };
        static in_addr s_target_ip_addr;
        static char* s_addr_list[2] = { (char*)&s_target_ip_addr, NULL };
        s_target_ip_addr.s_addr = inet_addr(g_TargetServerIp);
        s_he.h_name = (char*)name;
        s_he.h_aliases = s_aliases;
        s_he.h_addrtype = AF_INET;
        s_he.h_length = 4;
        s_he.h_addr_list = s_addr_list;
        return &s_he;
    }
    return OriginalGetHostByName ? OriginalGetHostByName(name) : nullptr;
}

typedef int (PASCAL* connect_t)(SOCKET s, const struct sockaddr *name, int namelen);
static connect_t OriginalConnect = nullptr;

int PASCAL DetourConnect(SOCKET s, const struct sockaddr *name, int namelen) {
    if (name && name->sa_family == AF_INET && namelen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)name;
        u_short port = ntohs(sin->sin_port);
        if (port == 10000 || port == 11000 || port == 80) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            Log("[mxohax] connect() intercepted for port %u -> routing to %s:%u\n", port, g_TargetServerIp, port);
            return OriginalConnect ? OriginalConnect(s, (struct sockaddr*)&redirected, namelen) : SOCKET_ERROR;
        }
    }
    return OriginalConnect ? OriginalConnect(s, name, namelen) : SOCKET_ERROR;
}

typedef int (PASCAL* sendto_t)(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
static sendto_t OriginalSendTo = nullptr;

int PASCAL DetourSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen) {
    if (to && to->sa_family == AF_INET && tolen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)to;
        u_short port = ntohs(sin->sin_port);
        if (port == 10000) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            static bool s_loggedSend = false;
            if (!s_loggedSend) {
                s_loggedSend = true;
                Log("[mxohax] sendto() UDP transmitting %d bytes to port 10000 at %s\n", len, g_TargetServerIp);
            }
            return OriginalSendTo ? OriginalSendTo(s, buf, len, flags, (struct sockaddr*)&redirected, tolen) : SOCKET_ERROR;
        }
    }
    return OriginalSendTo ? OriginalSendTo(s, buf, len, flags, to, tolen) : SOCKET_ERROR;
}

typedef int (PASCAL* recvfrom_t)(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
static recvfrom_t OriginalRecvFrom = nullptr;

int PASCAL DetourRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
    int res = OriginalRecvFrom ? OriginalRecvFrom(s, buf, len, flags, from, fromlen) : SOCKET_ERROR;
    if (res > 0 && from && fromlen && *fromlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)from;
        u_short port = ntohs(sin->sin_port);
        if (port == 10000) {
            static bool s_loggedRecv = false;
            if (!s_loggedRecv) {
                s_loggedRecv = true;
                Log("[mxohax] recvfrom() UDP received %d bytes from port 10000\n", res);
            }
        }
    }
    return res;
}

// ============================================================================
// RSA Bypass Hook (CryptoPP VerifyMessage)
// ============================================================================
typedef bool(__stdcall* VerifyMessage_t)(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6);
static VerifyMessage_t OriginalVerifyMatrix = nullptr;
static VerifyMessage_t OriginalVerifyClient = nullptr;

bool __stdcall DetourVerifyMessage(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6) {
    Log("[mxohax] DetourVerifyMessage intercepted -> returning true (RSA verification bypassed)\n");
    return true;
}

// ============================================================================
// matrix.exe Character Manager Hooks
// ============================================================================
static void SetupSyntheticCharManager(DWORD pThisDword) {
    // 1. Connection parameters
    memset(&g_SyntheticConnParams, 0, sizeof(g_SyntheticConnParams));
    strcpy_s(g_SyntheticConnParams.serverIp, sizeof(g_SyntheticConnParams.serverIp), g_TargetServerIp);
    g_SyntheticConnParams.serverPort = 10000;

    // 2. Character object
    g_SyntheticCharObj.pConnParams = &g_SyntheticConnParams;
    g_SyntheticCharObj.pData = &g_SyntheticChar;

    // 3. Populate matrix.exe Character Manager
    *reinterpret_cast<BYTE*>(pThisDword + 0x640) = 1;                              // Character count = 1
    *reinterpret_cast<void**>(pThisDword + 0x644) = &g_SyntheticCharObj;            // Character 0 object
    *reinterpret_cast<void**>(pThisDword + 0x658) = &g_SyntheticCharObj;            // Character 0 connection object (0x43F3D1 guard)
    *reinterpret_cast<BYTE*>(pThisDword + 0x66c) = 0;                              // Selected character index = 0
    *reinterpret_cast<BYTE*>(pThisDword + 0x778) = 1;                              // State check flag (0x43c618)
    *reinterpret_cast<DWORD*>(pThisDword + 0x77c) = 1;                             // WorldId = 1
    memcpy(reinterpret_cast<void*>(pThisDword + 0x674), &g_SyntheticChar, sizeof(g_SyntheticChar));
}

// 0x00428920: GetCharacterCount
typedef int (__thiscall *GetCharacterCount_t)(void* pThis);
static GetCharacterCount_t OriginalGetCharacterCount = nullptr;

int __fastcall DetourGetCharacterCount(void* pThis, void* /*edx*/) {
    DWORD pThisDword = reinterpret_cast<DWORD>(pThis);
    SetupSyntheticCharManager(pThisDword);
    Log("[mxohax] [matrix.exe] GetCharacterCount called -> returning 1 (operative s1acker mounted)\n");
    return 1;
}

// 0x00428E00: GetCharacterByIndex
typedef void* (__thiscall *GetCharacterByIndex_t)(void* pThis, void* /*edx*/, int idx);
static GetCharacterByIndex_t OriginalGetCharacterByIndex = nullptr;

void* __fastcall DetourGetCharacterByIndex(void* pThis, void* /*edx*/, int idx) {
    Log("[mxohax] [matrix.exe] GetCharacterByIndex(%d) called\n", idx);
    if (idx == 0) {
        DWORD pThisDword = reinterpret_cast<DWORD>(pThis);
        SetupSyntheticCharManager(pThisDword);
        Log("[mxohax] [matrix.exe] Returning synthetic operative s1acker (charId=360) at 0x%p\n", &g_SyntheticChar);
        return &g_SyntheticChar;
    }
    return nullptr;
}

// 0x00429D80: SelectCharacter in matrix.exe (vtable[0xDC])
typedef void (__thiscall *SelectCharacterVtbl_t)(void* pThis, void* pArg);
static SelectCharacterVtbl_t OriginalSelectCharacterVtbl = nullptr;

void __fastcall DetourSelectCharacterVtbl(void* pThis, void* /*edx*/, void* pArg) {
    Log("[mxohax] [matrix.exe] SelectCharacter (vtable 0xDC) called with pArg=0x%p\n", pArg);
    DWORD pThisDword = reinterpret_cast<DWORD>(pThis);
    SetupSyntheticCharManager(pThisDword);

    if (pArg) {
        memcpy(reinterpret_cast<void*>(pThisDword + 0x674), reinterpret_cast<BYTE*>(pArg) + 4, 0xAC);
    }

    void* pCurState = *reinterpret_cast<void**>(pThisDword + 0x10);
    int curStateId = -1;
    if (pCurState) {
        typedef int (__thiscall *GetStateId_t)(void* pState);
        void** pVtbl = *reinterpret_cast<void***>(pCurState);
        if (pVtbl) {
            GetStateId_t GetStateId = reinterpret_cast<GetStateId_t>(pVtbl[6]);
            if (GetStateId) curStateId = GetStateId(pCurState);
        }
    }
    Log("[mxohax] [matrix.exe] Current Margin State is %d\n", curStateId);

    if (curStateId >= 4 && OriginalSelectCharacterVtbl) {
        Log("[mxohax] [matrix.exe] Margin state >= 4, executing OriginalSelectCharacterVtbl...\n");
        OriginalSelectCharacterVtbl(pThis, pArg);
        Log("[mxohax] [matrix.exe] OriginalSelectCharacterVtbl finished successfully!\n");
    } else {
        Log("[mxohax] [matrix.exe] Margin state < 4 (%d), character mounted cleanly; skipping premature state transition.\n", curStateId);
    }
}

// ============================================================================
// client.dll State & World Management
// ============================================================================
// UI Tracing Hooks
typedef void (__thiscall *Control_t)(void* pUI, DWORD ctrlId);
static Control_t OriginalHideControl = nullptr;
static Control_t OriginalShowControl = nullptr;

static void __fastcall DetourHideControl(void* pUI, void* /*edx*/, DWORD ctrlId) {
    if (ctrlId != 0x1A) {
        Log("[mxohax] HideControl: 0x%02X\n", ctrlId);
    }
    if (OriginalHideControl) OriginalHideControl(pUI, ctrlId);
}

static bool s_screen5DActive = false;
static bool s_autoJackInDone = false;
static int  s_screen5DFrames = 0;

static void __fastcall DetourShowControl(void* pUI, void* /*edx*/, DWORD ctrlId) {
    Log("[mxohax] ShowControl: 0x%02X\n", ctrlId);
    if (ctrlId == 0x5D) {
        s_screen5DActive = true;
        s_screen5DFrames = 0;
        Log("[mxohax] Screen 0x5D (Character Selection) became active! AutoJackIn engaged.\n");
    }
    if (OriginalShowControl) OriginalShowControl(pUI, ctrlId);
}

static bool TryAutoJackIn(DWORD clientBase) {
    if (!clientBase) return false;

    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (!pWorldMgr) return false;

    // 1. Hide Login Screen (0x30) and CharSelect Screen (0x5D)
    if (OriginalHideControl) {
        void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
        if (pUI) {
            OriginalHideControl(pUI, 0x30);
            OriginalHideControl(pUI, 0x5D);
            Log("[mxohax] [AutoJackIn] Successfully dismissed Screen 0x30 and 0x5D!\n");
        }
    }

    // 2. Populate client globals for character identity
    *reinterpret_cast<DWORD*>(clientBase + 0x00896CCC) = 360; // CharId
    char* pFirstName = reinterpret_cast<char*>(clientBase + 0x00896D04);
    char* pLastName  = reinterpret_cast<char*>(clientBase + 0x00896D3C);
    strncpy_s(pFirstName, 32, "s1acker", 31);
    strncpy_s(pLastName, 32, "", 31);
    *reinterpret_cast<DWORD*>(clientBase + 0x00896D74) = 1;   // WorldId

    // 3. Check WorldMgr State and invoke StartWorldLoad if not already loading
    DWORD pWorldMgrDword = reinterpret_cast<DWORD>(pWorldMgr);
    DWORD* pState = reinterpret_cast<DWORD*>(pWorldMgrDword + 0x1C);
    if (pState && *pState < 2) {
        Log("[mxohax] [AutoJackIn] WorldMgr State was %u -> calling StartWorldLoad (0x10120060)...\n", *pState);
        typedef void (__thiscall *StartWorldLoad_t)(void* pWorldMgr);
        StartWorldLoad_t pStartWorldLoad = reinterpret_cast<StartWorldLoad_t>(clientBase + 0x00120060);
        pStartWorldLoad(pWorldMgr);
        Log("[mxohax] [AutoJackIn] StartWorldLoad completed! New state: %u\n", *pState);
    }

    // 4. Do NOT force m_inWorld = 1 prematurely here!
    // LithTech's world loader (0x10123B3D) requires m_inWorld == 0 to initialize chunks.
    // m_inWorld will be set to 1 natively by client.dll at 0x10121F4B once the 3D scene is ready.

    // 5. Transition matrix.exe Margin State Machine to State 9 (Connecting) -> State 5 (In-World)
    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (pMarginMgr) {
        DWORD pThisDword = reinterpret_cast<DWORD>(pMarginMgr);
        SetupSyntheticCharManager(pThisDword);
        typedef void (__thiscall *TransitionToState_t)(void* pMgr, DWORD newStateId);
        TransitionToState_t Transition = reinterpret_cast<TransitionToState_t>(0x00428FF0);
        Log("[mxohax] [AutoJackIn] Invoking matrix.exe Margin State transition to State 9 (0x00428FF0)...\n");
        Transition(pMarginMgr, 9);
        Log("[mxohax] [AutoJackIn] Margin State 9 transition invoked successfully!\n");
    }

    return true;
}

// 0x001F9140: True per-frame tick on main thread (called inside RunClientDLL 0x10006640)
typedef void (__thiscall *FrameTick_t)(void* pThis);
static FrameTick_t OriginalFrameTick = nullptr;
static int s_tickCount = 0;

static void __fastcall DetourFrameTick(void* pThis, void* /*edx*/) {
    if (OriginalFrameTick) OriginalFrameTick(pThis);
    s_tickCount++;

    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) return;
    DWORD clientBase = reinterpret_cast<DWORD>(hClient);

    // Monitor in-world status via CClientShell (at clientBase + 0x00896A38)
    DWORD pShell = clientBase + 0x00896A38;
    BYTE inWorld = *reinterpret_cast<BYTE*>(pShell + 0x20);
    static BYTE s_lastInWorld = 0xFF;
    // Log WorldMgr state transitions
    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (pWorldMgr) {
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        static DWORD s_lastState = 0xFFFFFFFF;
        if (pState && *pState != s_lastState) {
            s_lastState = *pState;
            Log("[mxohax] *** WorldMgr State transitioned to %u ***\n", s_lastState);
        }
    }

    if (inWorld != s_lastInWorld) {
        s_lastInWorld = inWorld;
        Log("[mxohax] *** CClientShell::m_inWorld changed to %u! (CClientShell=0x%p) ***\n", inWorld, (void*)pShell);
        if (inWorld == 1) {
            Log("[mxohax] ******************************************************\n");
            Log("[mxohax] *** IN-WORLD CONFIRMED: 3D SIMULATION LOOP ACTIVE! ***\n");
            Log("[mxohax] ******************************************************\n");
            void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
            if (pUI && OriginalHideControl) {
                OriginalHideControl(pUI, 0x04);
                OriginalHideControl(pUI, 0x57);
                Log("[mxohax] Dismissed loading screens 0x04 and 0x57 upon entering world!\n");
            }
        }
    }

    if (s_tickCount % 50 == 0) {
        Log("[mxohax] DetourFrameTick: Frame #%d active (inWorld=%u)\n", s_tickCount, inWorld);
    }
}

// Process Exit Hooks
typedef void (WINAPI *ExitProcess_t)(UINT uExitCode);
static ExitProcess_t OriginalExitProcess = nullptr;

void WINAPI DetourExitProcess(UINT uExitCode) {
    void* caller = _ReturnAddress();
    Log("[mxohax] ExitProcess(%u) called! ReturnAddress: 0x%p\n", uExitCode, caller);
    if (OriginalExitProcess) OriginalExitProcess(uExitCode);
}

typedef void (WINAPI *PostQuitMessage_t)(int nExitCode);
static PostQuitMessage_t OriginalPostQuitMessage = nullptr;

void WINAPI DetourPostQuitMessage(int nExitCode) {
    void* caller = _ReturnAddress();
    Log("[mxohax] PostQuitMessage(%d) called! ReturnAddress: 0x%p\n", nExitCode, caller);
    if (OriginalPostQuitMessage) OriginalPostQuitMessage(nExitCode);
}

// ============================================================================
// Pattern Scanner & Dynamic Patching for client.dll
// ============================================================================
static uintptr_t PatternScan(uintptr_t base, size_t size, const char* pattern, const char* mask) {
    size_t patternLen = strlen(mask);
    for (size_t i = 0; i < size - patternLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (mask[j] != '?' && pattern[j] != *(char*)(base + i + j)) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return 0;
}

// ============================================================================
// Safe GetPlayerActiveObject Hook (client.dll + 0x0010A210)
// Guards against null pointer dereference at [0x108A4378 + 0xA8] + 0x23C
// ============================================================================
typedef unsigned char (__stdcall *GetPlayerActiveObject_t)(void** outObj, void** outSubObj);
static GetPlayerActiveObject_t OriginalGetPlayerActiveObject = nullptr;

static unsigned char __stdcall Safe_GetPlayerActiveObject(void** outObj, void** outSubObj) {
    if (outObj) *outObj = nullptr;
    if (outSubObj) *outSubObj = nullptr;

    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        Log("[mxohax] Safe_GetPlayerActiveObject called! First invocation intercepted.\n");
    }

    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) return 0;
    uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClient);

    uintptr_t pGlobal = *reinterpret_cast<uintptr_t*>(clientBase + 0x008A4378);
    if (!pGlobal) return 0;

    uintptr_t pA8 = *reinterpret_cast<uintptr_t*>(pGlobal + 0xA8);
    if (!pA8) return 0;

    uintptr_t p23C = *reinterpret_cast<uintptr_t*>(pA8 + 0x23C);
    if (!p23C) return 0; // Essential null-check guarding against crash 0xC0000005!

    uintptr_t pESI = *reinterpret_cast<uintptr_t*>(p23C);
    if (!pESI) return 0;

    if (*reinterpret_cast<uint16_t*>(pESI + 4) == 0xFFFF) return 0;

    uint16_t idx0 = *reinterpret_cast<uint16_t*>(pESI);
    uintptr_t pMgr = *reinterpret_cast<uintptr_t*>(clientBase + 0x00897F90);
    if (!pMgr) return 0;

    typedef void* (__thiscall *FnGetObj)(void* thisPtr, uint32_t id);
    FnGetObj pfnGetObj = reinterpret_cast<FnGetObj>(clientBase + 0x003A5CA0);
    void* obj = pfnGetObj(reinterpret_cast<void*>(pMgr), idx0);
    if (outObj) *outObj = obj;
    if (!obj) return 0;

    uint16_t idx1 = *reinterpret_cast<uint16_t*>(pESI + 2);
    void** vtable = *reinterpret_cast<void***>(obj);
    if (!vtable) return 0;

    typedef void* (__thiscall *FnGetSubObj)(void* thisPtr, uint32_t id);
    FnGetSubObj pfnGetSubObj = reinterpret_cast<FnGetSubObj>(vtable[0x58 / 4]);
    if (!pfnGetSubObj) return 0;

    void* subObj = pfnGetSubObj(obj, idx1);
    if (outSubObj) *outSubObj = subObj;

    return (subObj != nullptr) ? 1 : 0;
}

// ============================================================================
// Safe Camera and Net Connection Trampolines
// Guards against null pointer dereference in in-world render tick (0x1012a630)
// ============================================================================
static uintptr_t g_pEdf8Addr = 0;
static uintptr_t g_retCameraNormal = 0;
static uintptr_t g_retCameraSkip = 0;

__declspec(naked) void Hook_CameraCheck() {
    __asm {
        mov edx, dword ptr [g_pEdf8Addr]
        test edx, edx
        je _cam_skip
        mov edx, dword ptr [edx]
        test edx, edx
        je _cam_skip
        mov edi, dword ptr [edx]
        test edi, edi
        je _cam_skip
        jmp dword ptr [g_retCameraNormal]

    _cam_skip:
        xor edi, edi
        jmp dword ptr [g_retCameraSkip]
    }
}

static uintptr_t g_pAbb90Addr = 0;
static uintptr_t g_pE30cAddr = 0;
static uintptr_t g_retAbb90Normal = 0;
static uintptr_t g_retAbb90Skip = 0;

__declspec(naked) void Hook_Abb90Check() {
    __asm {
        mov edx, dword ptr [g_pE30cAddr]
        test edx, edx
        je _abb_skip
        cmp dword ptr [edx], 0
        je _abb_skip
        mov edx, dword ptr [g_pAbb90Addr]
        test edx, edx
        je _abb_skip
        cmp dword ptr [edx], 0
        je _abb_skip
        jmp dword ptr [g_retAbb90Normal]

    _abb_skip:
        jmp dword ptr [g_retAbb90Skip]
    }
}

static void ApplyClientPatches(HMODULE hClient) {
    static bool s_clientPatched = false;
    if (s_clientPatched || !hClient) return;
    s_clientPatched = true;

    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    Log("[mxohax] Applying client.dll hooks at base 0x%p...\n", (void*)clientBase);

    // 1. Hook CryptoPP::PK_Verifier::VerifyMessage in client.dll at RVA 0x0047E010
    uintptr_t clientVerifyAddr = clientBase + 0x0047E010;
    if (MH_CreateHook(reinterpret_cast<LPVOID>(clientVerifyAddr), &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyClient)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(clientVerifyAddr));
        Log("[mxohax] SUCCESS: client.dll VerifyMessage hooked at 0x%p! RSA bypass active.\n", (void*)clientVerifyAddr);
    }

    // 2. Hook FrameTick on main thread (0x001F9140)
    LPVOID pFrameTick = reinterpret_cast<LPVOID>(clientBase + 0x001F9140);
    if (MH_CreateHook(pFrameTick, &DetourFrameTick, reinterpret_cast<LPVOID*>(&OriginalFrameTick)) == MH_OK) {
        MH_EnableHook(pFrameTick);
        Log("[mxohax] SUCCESS: client.dll FrameTick hooked at 0x%p!\n", pFrameTick);
    }

    // 3. Hook HideControl & ShowControl
    LPVOID pHideControl = reinterpret_cast<LPVOID>(clientBase + 0x0001D3C0);
    if (MH_CreateHook(pHideControl, &DetourHideControl, reinterpret_cast<LPVOID*>(&OriginalHideControl)) == MH_OK) {
        MH_EnableHook(pHideControl);
    }
    LPVOID pShowControl = reinterpret_cast<LPVOID>(clientBase + 0x0001BC10);
    if (MH_CreateHook(pShowControl, &DetourShowControl, reinterpret_cast<LPVOID*>(&OriginalShowControl)) == MH_OK) {
        MH_EnableHook(pShowControl);
    }

    // 4. Hook GetPlayerActiveObject (0x0010A210) to guard against NULL player entity dereference
    LPVOID pGetActiveObj = reinterpret_cast<LPVOID>(clientBase + 0x0010A210);
    if (MH_CreateHook(pGetActiveObj, &Safe_GetPlayerActiveObject, reinterpret_cast<LPVOID*>(&OriginalGetPlayerActiveObject)) == MH_OK) {
        MH_EnableHook(pGetActiveObj);
        Log("[mxohax] SUCCESS: client.dll GetPlayerActiveObject hooked at 0x%p! Null dereference guarded.\n", pGetActiveObj);
    }

    // 5. Populate character identity globals in client.dll immediately
    *reinterpret_cast<DWORD*>(clientBase + 0x00896CCC) = 360; // CharId
    char* pFirstName = reinterpret_cast<char*>(clientBase + 0x00896D04);
    char* pLastName  = reinterpret_cast<char*>(clientBase + 0x00896D3C);
    strncpy_s(pFirstName, 32, "s1acker", 31);
    strncpy_s(pLastName, 32, "", 31);
    *reinterpret_cast<DWORD*>(clientBase + 0x00896D74) = 1;   // WorldId

    Log("[mxohax] SUCCESS: Initialized client.dll character identity globals for s1acker (charId=360)\n");

    // 6. Direct World Load Patches:
    // Patch A: Preserved native client.dll + 0x0012196E (movzx esi, bl) so character selection and auto-login operate naturally.
    Log("[mxohax] Preserved native character select logic at client.dll + 0x0012196E.\n");

    // Patch B: Removed. Leaving native clean ret 0x14 at 0x10121AE6 so the function epilogue executes cleanly.
    Log("[mxohax] Preserved native clean character load epilogue at client.dll + 0x00121AE6.\n");

    // Patch C: 0x0012B3EE: 16 bytes safe camera check
    g_pEdf8Addr = clientBase + 0x0089EDF8;
    g_retCameraNormal = clientBase + 0x0012B3FE;
    g_retCameraSkip = clientBase + 0x0012B4F1;

    DWORD oldProt = 0;
    LPVOID pCamPatch = reinterpret_cast<LPVOID>(clientBase + 0x0012B3EE);
    if (VirtualProtect(pCamPatch, 16, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[16];
        patch[0] = 0xE9;
        *reinterpret_cast<DWORD*>(&patch[1]) = static_cast<DWORD>(reinterpret_cast<uintptr_t>(&Hook_CameraCheck) - (reinterpret_cast<uintptr_t>(pCamPatch) + 5));
        memset(&patch[5], 0x90, 11);
        memcpy(pCamPatch, patch, 16);
        VirtualProtect(pCamPatch, 16, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pCamPatch, 16);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0012B3EE for safe camera check!\n");
    }

    // Patch D: 0x0012B4F1: 8 bytes safe net check
    g_pE30cAddr = clientBase + 0x0089E30C;
    g_pAbb90Addr = clientBase + 0x008ABB90;
    g_retAbb90Normal = clientBase + 0x0012B4F9;
    g_retAbb90Skip = clientBase + 0x0012B557;

    LPVOID pAbbPatch = reinterpret_cast<LPVOID>(clientBase + 0x0012B4F1);
    if (VirtualProtect(pAbbPatch, 8, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[8];
        patch[0] = 0xE9;
        *reinterpret_cast<DWORD*>(&patch[1]) = static_cast<DWORD>(reinterpret_cast<uintptr_t>(&Hook_Abb90Check) - (reinterpret_cast<uintptr_t>(pAbbPatch) + 5));
        memset(&patch[5], 0x90, 3);
        memcpy(pAbbPatch, patch, 8);
        VirtualProtect(pAbbPatch, 8, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pAbbPatch, 8);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0012B4F1 for safe net check!\n");
    }
}

// Hook LoadLibraryA to catch client.dll synchronously
typedef HMODULE (WINAPI* LoadLibraryA_t)(LPCSTR lpLibFileName);
static LoadLibraryA_t OriginalLoadLibraryA = nullptr;

HMODULE WINAPI DetourLoadLibraryA(LPCSTR lpLibFileName) {
    HMODULE hMod = OriginalLoadLibraryA ? OriginalLoadLibraryA(lpLibFileName) : LoadLibraryA(lpLibFileName);
    if (hMod && lpLibFileName) {
        const char* name = strrchr(lpLibFileName, '\\');
        if (!name) name = strrchr(lpLibFileName, '/');
        name = name ? name + 1 : lpLibFileName;
        if (_stricmp(name, "client.dll") == 0) {
            Log("[mxohax] DetourLoadLibraryA intercepted client.dll loaded at 0x%p! Applying patches synchronously...\n", (void*)hMod);
            ApplyClientPatches(hMod);
        }
    }
    return hMod;
}

extern "C" __declspec(dllexport) void __cdecl ExportedOrdinal1() {
}

static void InitializeMxOHaxSynchronous() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;
    AddVectoredExceptionHandler(1, CrashHandler);
    Log("[mxohax] InitializeMxOHaxSynchronous started (CrashHandler registered)...\n");
    LoadTargetServerIp();

    // Initialize synthetic structures
    memset(&g_SyntheticConnParams, 0, sizeof(g_SyntheticConnParams));
    strcpy_s(g_SyntheticConnParams.serverIp, sizeof(g_SyntheticConnParams.serverIp), g_TargetServerIp);
    g_SyntheticConnParams.serverPort = 10000;

    memset(&g_SyntheticCharObj, 0, sizeof(g_SyntheticCharObj));
    g_SyntheticCharObj.pConnParams = &g_SyntheticConnParams;
    g_SyntheticCharObj.pData = &g_SyntheticChar;

    if (MH_Initialize() != MH_OK) {
        Log("[mxohax] ERROR: Failed to initialize MinHook!\n");
        return;
    }

    // 1. Hook LoadLibraryA immediately
    LPVOID pTarget = nullptr;
    if (MH_CreateHookApiEx(L"kernel32.dll", "LoadLibraryA", (LPVOID)&DetourLoadLibraryA, (LPVOID*)&OriginalLoadLibraryA, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: kernel32.dll LoadLibraryA hooked at 0x%p\n", pTarget);
    }

    // 2. Hook WinSock functions (ws2_32.dll)
    if (MH_CreateHookApiEx(L"ws2_32.dll", "gethostbyname", (LPVOID)&DetourGetHostByName, (LPVOID*)&OriginalGetHostByName, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: ws2_32.dll gethostbyname hooked at 0x%p\n", pTarget);
    }
    if (MH_CreateHookApiEx(L"ws2_32.dll", "connect", (LPVOID)&DetourConnect, (LPVOID*)&OriginalConnect, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: ws2_32.dll connect hooked at 0x%p\n", pTarget);
    }
    if (MH_CreateHookApiEx(L"ws2_32.dll", "sendto", (LPVOID)&DetourSendTo, (LPVOID*)&OriginalSendTo, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: ws2_32.dll sendto hooked at 0x%p\n", pTarget);
    }
    if (MH_CreateHookApiEx(L"ws2_32.dll", "recvfrom", (LPVOID)&DetourRecvFrom, (LPVOID*)&OriginalRecvFrom, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: ws2_32.dll recvfrom hooked at 0x%p\n", pTarget);
    }

    // 3. Hook process exit functions
    if (MH_CreateHookApiEx(L"kernel32.dll", "ExitProcess", (LPVOID)&DetourExitProcess, (LPVOID*)&OriginalExitProcess, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: kernel32.dll ExitProcess hooked at 0x%p\n", pTarget);
    }
    if (MH_CreateHookApiEx(L"user32.dll", "PostQuitMessage", (LPVOID)&DetourPostQuitMessage, (LPVOID*)&OriginalPostQuitMessage, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: user32.dll PostQuitMessage hooked at 0x%p\n", pTarget);
    }

    // 4. Hook matrix.exe RSA signature check at 0x004386F0
    LPVOID pMatrixVerify = reinterpret_cast<LPVOID>(0x004386F0);
    if (MH_CreateHook(pMatrixVerify, &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyMatrix)) == MH_OK) {
        MH_EnableHook(pMatrixVerify);
        Log("[mxohax] SUCCESS: matrix.exe VerifyMessage hooked at 0x%p!\n", pMatrixVerify);
    }

    // 5. Allow native Auth and Margin network flow to manage characters naturally
    Log("[mxohax] Preserving native matrix.exe Character Manager for authentic server stream.\n");
}

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    HMODULE hClient = nullptr;
    for (int i = 0; i < 500 && !hClient; ++i) {
        hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            ApplyClientPatches(hClient);
            break;
        }
        Sleep(20);
    }

    if (!hClient) return 0;
    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    bool inWorldLogged = false;

    // Monitor inWorld flag and handle AutoJackIn
    for (int i = 0; i < 6000; ++i) { // monitor for up to 5 minutes
        Sleep(100);

        uintptr_t shellAddr = clientBase + 0x00896A38;
        BYTE* pInWorld = reinterpret_cast<BYTE*>(shellAddr + 0x20);
        if (pInWorld && *pInWorld == 1 && !inWorldLogged) {
            inWorldLogged = true;
            Log("[mxohax] *** IN-WORLD CONFIRMED: CClientShell::m_inWorld == 1! 3D game simulation loop active. ***\n");
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        Log("[mxohax] DLL_PROCESS_ATTACH (thread %u)...\n", GetCurrentThreadId());
        InitializeMxOHaxSynchronous();
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("[mxohax] DLL_PROCESS_DETACH.\n");
    }
    return TRUE;
}

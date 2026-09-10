#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>
#include <intrin.h>
#include "minhook/include/MinHook.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d3d9.lib")

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
                if (ctx->Esp) {
                    DWORD* pStack = reinterpret_cast<DWORD*>(ctx->Esp);
                    for (int i = 0; i < 32; ++i) {
                        if (!IsBadReadPtr(pStack + i, 4)) {
                            DWORD val = pStack[i];
                            if (clientBase && val >= clientBase && val < clientBase + 0x1000000) {
                                Log("[mxohax] STACK[%02d]: 0x%08X (client.dll + 0x%08X)\n", i, val, val - clientBase);
                            } else if (matrixBase && val >= matrixBase && val < matrixBase + 0x1000000) {
                                Log("[mxohax] STACK[%02d]: 0x%08X (matrix.exe + 0x%08X)\n", i, val, val - matrixBase);
                            } else {
                                Log("[mxohax] STACK[%02d]: 0x%08X\n", i, val);
                            }
                        }
                    }
                }
            }
            if (clientBase && (uintptr_t)addr == clientBase + 0x0016D495 && ctx) {
                Log("[mxohax] Recovering from crash at client.dll + 0x0016D495: jumping to epilogue (0x%p)\n", (void*)(clientBase + 0x0016D4DD));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0016D4DD);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00162270 && (uintptr_t)addr <= clientBase + 0x001622DC && ctx) {
                Log("[mxohax] Recovering from crash at client.dll + 0x%08X: jumping to epilogue (0x%p)\n", (uintptr_t)addr - clientBase, (void*)(clientBase + 0x001622DC));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x001622DC);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr == clientBase + 0x000A20E6 && ctx) {
                Log("[mxohax] Recovering from null deref at client.dll + 0x000A20E6 ([pWorldMgr+0xCC]==NULL). Jumping to safe return (0x%p)\n", (void*)(clientBase + 0x000A2213));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x000A2213);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr == clientBase + 0x001152D7 && ctx) {
                Log("[mxohax] Recovering from crash at client.dll + 0x001152D7 (Viewport vtable). Jumping to safe return (0x%p)\n", (void*)(clientBase + 0x001155A9));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x001155A9);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && ((uintptr_t)addr == clientBase + 0x00001C1C || (uintptr_t)addr == clientBase + 0x00001C10) && ctx) {
                Log("[mxohax] Recovering from invalid pointer write at client.dll + 0x%08X (eax=0x%08X): skipping 2 bytes\n",
                    (uintptr_t)addr - clientBase, ctx->Eax);
            }
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0009A000 && (uintptr_t)addr <= clientBase + 0x0009B000 && ctx) {
                Log("[mxohax] Recovering from crash in viewinterlock UI at client.dll + 0x%08X: unwinding frame safely\n", (uintptr_t)addr - clientBase);
                if (ctx->Ebp && !IsBadReadPtr((void*)(ctx->Ebp + 4), 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    if (retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                        ctx->Eip = retAddr;
                        ctx->Esp = ctx->Ebp + 8;
                        ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
                if (ctx->Esp && !IsBadReadPtr((void*)ctx->Esp, 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Esp);
                    if (retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                        ctx->Eip = retAddr;
                        ctx->Esp += 4;
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }
            // Guard against crashes in Contact / Mission / PDA UI (e.g. CViewMissionContact 0x000AC000-0x000ACD00, CViewContact 0x0018C000-0x0018D000, CViewPDA 0x000BF000-0x000C1000)
            if (clientBase && (
                ((uintptr_t)addr >= clientBase + 0x000AC000 && (uintptr_t)addr <= clientBase + 0x000ACD00) ||
                ((uintptr_t)addr >= clientBase + 0x0018C000 && (uintptr_t)addr <= clientBase + 0x0018D000) ||
                ((uintptr_t)addr >= clientBase + 0x000BF000 && (uintptr_t)addr <= clientBase + 0x000C1000)
            ) && ctx) {
                Log("[mxohax] Recovering from crash in Contact/PDA UI at client.dll + 0x%08X: unwinding frame safely\n", (uintptr_t)addr - clientBase);
                if (ctx->Ebp && !IsBadReadPtr((void*)(ctx->Ebp + 4), 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    if (retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                        ctx->Eip = retAddr;
                        ctx->Esp = ctx->Ebp + 8;
                        ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
                if (ctx->Esp && !IsBadReadPtr((void*)ctx->Esp, 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Esp);
                    if (retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                        ctx->Eip = retAddr;
                        ctx->Esp += 4;
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }
            // Guard against memcpy crash in 0x10255710 (client.dll + 0x0025581B)
            if (ctx && ctx->Esp) {
                DWORD* pStack = reinterpret_cast<DWORD*>(ctx->Esp);
                for (int i = 0; i < 8; ++i) {
                    if (!IsBadReadPtr(pStack + i, 4)) {
                        DWORD retAddr = pStack[i];
                        if (clientBase && retAddr >= clientBase + 0x0025581B && retAddr <= clientBase + 0x00255825) {
                            Log("[mxohax] Recovering from memcpy crash in 0x10255710: jumping to epilogue (0x%p)\n",
                                (void*)(clientBase + 0x00255920));
                            ctx->Eip = static_cast<DWORD>(clientBase + 0x00255920);
                            ctx->Eax = 190;
                            return EXCEPTION_CONTINUE_EXECUTION;
                        }
                    }
                }
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
    HDESK hCurDesk = GetThreadDesktop(GetCurrentThreadId());
    char deskBuf[128] = {0};
    GetUserObjectInformationA(hCurDesk, 2, deskBuf, sizeof(deskBuf), NULL);
    Log("[mxohax] Process Thread Desktop: '%s'\n", deskBuf);
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

struct MxoCharRecord {
    BYTE     pad[3];          // 0x00..0x02
    uint32_t charIdLow;       // 0x03..0x06: 360 (read at matrix.exe 0x0043C653)
    uint32_t charIdHigh;      // 0x07..0x0A: 0   (read at matrix.exe 0x0043C65C)
    BYTE     pad2;            // 0x0B
    uint16_t worldId;         // 0x0C..0x0D: 1   (read at matrix.exe 0x0043D269)
    BYTE     extra[32];
};

struct MxoConnParams {
    BYTE     pad0;            // 0x00: 0
    WORD     worldId;         // 0x01..0x02: 1 (read at matrix.exe 0x00441243)
    char     serverIp[32];    // 0x03..0x22: "15.204.82.250" (read at matrix.exe 0x0043F340)
    WORD     serverPort;      // 10000
    BYTE     extra[32];
};

static void* g_SyntheticVtbl[64];
static void* __fastcall DummyDestructor(void* pThis, void* /*edx*/, unsigned char /*flags*/) { return pThis; }

struct MxoCharObj {
    void**         pVtbl;       // 0x00..0x03
    BYTE           pad1[0x0C];  // 0x04..0x0F
    MxoCharRecord* pRecord;     // 0x10: Pointer to MxoCharRecord (read at matrix.exe 0x0043C650)
    const char*    pCharName;   // 0x14: Pointer to character name string (read at matrix.exe 0x0043FD30)
    BYTE           pad2[0x20];
};

struct MxoConnObj {
    void**         pVtbl;       // 0x00..0x03
    BYTE           pad1[0x0C];  // 0x04..0x0F
    MxoConnParams* pConnParams; // 0x10: Pointer to MxoConnParams (read at matrix.exe 0x0043F340, 0x00441240)
    BYTE           pad2[0x20];
};
#pragma pack(pop)

static MxoCharacterData   g_SyntheticChar = {
    360, 0, 0, 0, 0,
    "s1acker", "",
    100, 100, 101, 1, 360
};
static const char         g_SyntheticCharName[] = "s1acker";
static MxoCharRecord      g_SyntheticCharRecord;
static MxoConnParams      g_SyntheticConnParams;
static MxoCharObj         g_SyntheticCharObj;
static MxoConnObj         g_SyntheticConnObj;


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
        char* ip = inet_ntoa(sin->sin_addr);
        Log("[mxohax] connect() called: dest=%s:%u\n", ip ? ip : "unknown", port);
        if (port == 10000 || port == 11000 || port == 80 || (ip && strcmp(ip, "127.0.0.1") == 0)) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            Log("[mxohax] connect() redirected dest=%s:%u -> routing to %s:%u\n", ip ? ip : "unknown", port, g_TargetServerIp, port);
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
        char* ip = inet_ntoa(sin->sin_addr);
        if (port == 10000 || port == 11000 || (ip && strcmp(ip, "127.0.0.1") == 0)) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            static bool s_loggedSend = false;
            if (!s_loggedSend) {
                s_loggedSend = true;
                char hexBuf[256] = {0};
                for (int i = 0; i < len && i < 64; ++i) {
                    sprintf(hexBuf + i * 3, "%02X ", (unsigned char)buf[i]);
                }
                Log("[mxohax] sendto() UDP transmitting %d bytes to port %u at %s. Hex: %s\n", len, port, g_TargetServerIp, hexBuf);
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
            static int s_recvCount = 0;
            s_recvCount++;
            if (s_recvCount <= 30) {
                char hex[128] = {0};
                for (int i = 0; i < res && i < 32; ++i) {
                    sprintf(hex + i * 3, "%02X ", (unsigned char)buf[i]);
                }
                Log("[mxohax] recvfrom(#%d) UDP: %d bytes. Hex: %s\n", s_recvCount, res, hex);
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
    // 1. Initialize vtable
    for (int i = 0; i < 64; ++i) {
        g_SyntheticVtbl[i] = (void*)&DummyDestructor;
    }

    // 2. Character record (for matrix.exe 0x0043C650 MS_LoadCharacterRequest packet building)
    memset(&g_SyntheticCharRecord, 0, sizeof(g_SyntheticCharRecord));
    g_SyntheticCharRecord.charIdLow = 360;
    g_SyntheticCharRecord.charIdHigh = 0;
    g_SyntheticCharRecord.worldId = 1;

    // 3. Connection parameters (for matrix.exe 0x0043F340 IP resolution and 0x00441240 worldId)
    memset(&g_SyntheticConnParams, 0, sizeof(g_SyntheticConnParams));
    g_SyntheticConnParams.worldId = 1;
    strcpy_s(g_SyntheticConnParams.serverIp, sizeof(g_SyntheticConnParams.serverIp), g_TargetServerIp);
    g_SyntheticConnParams.serverPort = 10000;

    // 4. Character and Connection objects
    g_SyntheticCharObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticCharObj.pRecord = &g_SyntheticCharRecord;
    g_SyntheticCharObj.pCharName = g_SyntheticCharName;

    g_SyntheticConnObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticConnObj.pConnParams = &g_SyntheticConnParams;

    // 5. Populate matrix.exe Character Manager
    *reinterpret_cast<BYTE*>(pThisDword + 0x640) = 1;                              // Character count = 1
    *reinterpret_cast<void**>(pThisDword + 0x644) = &g_SyntheticCharObj;            // Character 0 object
    *reinterpret_cast<void**>(pThisDword + 0x658) = &g_SyntheticConnObj;            // Character 0 connection object (0x43F3D1 guard)
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

// 0x00428CC0: ClearCharacters in matrix.exe
typedef void (__thiscall *ClearCharacters_t)(void* pThis);
static ClearCharacters_t OriginalClearCharacters = nullptr;

static void __fastcall DetourClearCharacters(void* pThis, void* /*edx*/) {
    Log("[mxohax] [matrix.exe] ClearCharacters (0x00428CC0) called on 0x%p\n", pThis);
    BYTE* p = reinterpret_cast<BYTE*>(pThis);
    BYTE count = p[0];
    for (BYTE i = 0; i < count && i < 5; ++i) {
        void** ppChar = reinterpret_cast<void**>(p + 4 + i * 4);
        void* pChar = *ppChar;
        if (pChar && pChar != &g_SyntheticCharObj) {
            void** pVtbl = *reinterpret_cast<void***>(pChar);
            if (pVtbl && pVtbl[0]) {
                typedef void* (__thiscall *Dtor_t)(void*, BYTE);
                Dtor_t dtor = reinterpret_cast<Dtor_t>(pVtbl[0]);
                dtor(pChar, 1);
            }
        }

        void** ppConn = reinterpret_cast<void**>(p + 0x18 + i * 4);
        void* pConn = *ppConn;
        if (pConn && pConn != &g_SyntheticConnObj) {
            void** pVtbl = *reinterpret_cast<void***>(pConn);
            if (pVtbl && pVtbl[0]) {
                typedef void* (__thiscall *Dtor_t)(void*, BYTE);
                Dtor_t dtor = reinterpret_cast<Dtor_t>(pVtbl[0]);
                dtor(pConn, 1);
            }
        }
    }
    // Re-mount synthetic operative s1acker so matrix.exe can never be in an unselected/cleared state
    p[0] = 1;
    *reinterpret_cast<void**>(p + 4) = &g_SyntheticCharObj;
    *reinterpret_cast<void**>(p + 0x18) = &g_SyntheticConnObj;
    p[0x2C] = 0; // Selected index = 0 (offset 0x66C on MarginMgr)

    DWORD pMarginMgr = reinterpret_cast<DWORD>(p - 0x640);
    *reinterpret_cast<BYTE*>(pMarginMgr + 0x778) = 1;
    *reinterpret_cast<DWORD*>(pMarginMgr + 0x77c) = 1;
    memcpy(reinterpret_cast<void*>(pMarginMgr + 0x674), &g_SyntheticChar, sizeof(g_SyntheticChar));

    Log("[mxohax] [matrix.exe] ClearCharacters preserved operative s1acker (count=1, selected=0, charObj=0x%p, connObj=0x%p)\n",
        &g_SyntheticCharObj, &g_SyntheticConnObj);
}

// 0x00428D10: ClearCharObjects in matrix.exe
typedef void (__thiscall *ClearCharObjects_t)(void* pThis);
static ClearCharObjects_t OriginalClearCharObjects = nullptr;

static void __fastcall DetourClearCharObjects(void* pThis, void* /*edx*/) {
    Log("[mxohax] [matrix.exe] ClearCharObjects (0x00428D10) called on 0x%p\n", pThis);
    BYTE* p = reinterpret_cast<BYTE*>(pThis);
    BYTE count = p[0];
    for (BYTE i = 0; i < count && i < 5; ++i) {
        void** ppChar = reinterpret_cast<void**>(p + 4 + i * 4);
        void* pChar = *ppChar;
        if (pChar && pChar != &g_SyntheticCharObj) {
            void** pVtbl = *reinterpret_cast<void***>(pChar);
            if (pVtbl && pVtbl[0]) {
                typedef void* (__thiscall *Dtor_t)(void*, BYTE);
                Dtor_t dtor = reinterpret_cast<Dtor_t>(pVtbl[0]);
                dtor(pChar, 1);
            }
        }
    }
    p[0] = 1;
    *reinterpret_cast<void**>(p + 4) = &g_SyntheticCharObj;
    Log("[mxohax] [matrix.exe] ClearCharObjects preserved operative s1acker (count=1, charObj=0x%p)\n", &g_SyntheticCharObj);
}

// ============================================================================
// client.dll State & World Management
// ============================================================================
// UI Tracing Hooks
typedef void (__thiscall *Control_t)(void* pUI, DWORD ctrlId);
static Control_t OriginalHideControl = nullptr;

typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
static SetControlVisible_t OriginalSetControlVisible = nullptr;

static void __fastcall DetourHideControl(void* pUI, void* /*edx*/, DWORD ctrlId) {
    if (ctrlId != 0x1A) {
        Log("[mxohax] HideControl: 0x%02X\n", ctrlId);
    }
    if (OriginalHideControl) OriginalHideControl(pUI, ctrlId);
}

static bool s_inWorldSticky = false;
static bool s_playerEnteredWorld = false;
static bool s_screen5DActive = false;
static bool s_autoJackInDone = false;
static int  s_screen5DFrames = 0;

static void __fastcall DetourSetControlVisible(void* pUI, void* /*edx*/, DWORD ctrlId, BOOL bVisible) {
    if (ctrlId != 0x1A) {
        Log("[mxohax] SetControlVisible: 0x%02X (bVisible=%d)\n", ctrlId, bVisible ? 1 : 0);
    }
    if (ctrlId == 0x5D && bVisible) {
        s_screen5DActive = true;
        s_screen5DFrames = 0;
        Log("[mxohax] Screen 0x5D (Character Selection) became active! AutoJackIn engaged.\n");
    }
    if (s_inWorldSticky && bVisible && (ctrlId == 0x30 || ctrlId == 0x5D || ctrlId == 0x04 || ctrlId == 0x57)) {
        Log("[mxohax] SetControlVisible: 0x%02X suppressed while in-world!\n", ctrlId);
        return;
    }
    if (OriginalSetControlVisible) OriginalSetControlVisible(pUI, ctrlId, bVisible);
}

// Hook for client.dll export InitClientDLL (client.dll + 0x00001270)
typedef int (__cdecl *InitClientDLL_t)(
    void* p1, void* p2, void* p3, void* p4,
    void* p5, void* p6,
    DWORD worldCharPacked,
    BOOL autoJackIn
);
static InitClientDLL_t OriginalInitClientDLL = nullptr;

int __cdecl DetourInitClientDLL(
    void* p1, void* p2, void* p3, void* p4,
    void* p5, void* p6,
    DWORD worldCharPacked,
    BOOL autoJackIn
) {
    Log("[mxohax] DetourInitClientDLL: worldCharPacked=0x%08X, autoJackIn=%d\n", worldCharPacked, autoJackIn);
    int res = OriginalInitClientDLL ? OriginalInitClientDLL(p1, p2, p3, p4, p5, p6, worldCharPacked, autoJackIn) : 1;
    Log("[mxohax] DetourInitClientDLL returned %d\n", res);
    return res;
}

static bool TryAutoJackIn(DWORD clientBase) {
    if (!clientBase) return false;

    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (!pWorldMgr) return false;

    // 1. Dismiss Screen 0x30 (Login screen), but do NOT hide Screen 0x5D yet
    if (OriginalHideControl) {
        void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
        if (pUI) {
            OriginalHideControl(pUI, 0x30);
            Log("[mxohax] [AutoJackIn] Dismissed Screen 0x30\n");
        }
    }

    // 2. Ensure CNetClient at clientBase + 0x0089BBA0 is marked connected
    DWORD pNetClient = *reinterpret_cast<DWORD*>(clientBase + 0x0089BBA0);
    if (pNetClient) {
        *reinterpret_cast<DWORD*>(pNetClient + 0x08) = 2; // m_state = CONNECTED (2)
        Log("[mxohax] [AutoJackIn] Ensured CNetClient at 0x%08X (m_state=2 CONNECTED)\n", pNetClient);
    }

    // 3. Mark character selected in client.dll WorldMgr
    *reinterpret_cast<BYTE*>(clientBase + 0x0089DD5D) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x25) = 1;
    Log("[mxohax] [AutoJackIn] Set WorldMgr character select flags (0x0089DD5D and pWorldMgr+0x25)\n");

    // 4. Transition matrix.exe Margin State Machine to State 9 (Connecting)
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

    // 5. Populate character vector and invoke EnterWorldWithCharacter
    #pragma pack(push, 1)
    struct MxoLocalCharEntry {
        char* pWorldFirst;
        char* pWorldLast;
        char* pWorldEnd;
        char* pHandleFirst;
        char* pHandleLast;
        char* pHandleEnd;
        uint32_t charId;  // 0x18: 360
        uint32_t worldId; // 0x1C: 1
    };
    #pragma pack(pop)

    static char s_worldFileName[] = "resource/worlds/final_world/slums_barrens_full.metr";
    static char s_charHandleStr[] = "s1acker";
    static MxoLocalCharEntry s_localCharEntry;
    s_localCharEntry.pWorldFirst  = s_worldFileName;
    s_localCharEntry.pWorldLast   = s_worldFileName + strlen(s_worldFileName);
    s_localCharEntry.pWorldEnd    = s_localCharEntry.pWorldLast;
    s_localCharEntry.pHandleFirst = s_charHandleStr;
    s_localCharEntry.pHandleLast  = s_charHandleStr + strlen(s_charHandleStr);
    s_localCharEntry.pHandleEnd   = s_localCharEntry.pHandleLast;
    s_localCharEntry.charId       = 360;
    s_localCharEntry.worldId      = 1;

    // Ensure fallback world pointer at 0x00896E4C points to real slums METR
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_worldFileName;

    DWORD* ppCharBegin = reinterpret_cast<DWORD*>(clientBase + 0x00899B4C);
    DWORD* ppCharEnd   = reinterpret_cast<DWORD*>(clientBase + 0x00899B50);
    void* pCharToEnter = nullptr;
    if (ppCharBegin && ppCharEnd) {
        if (*ppCharEnd > *ppCharBegin) {
            pCharToEnter = reinterpret_cast<void*>(*ppCharBegin);
            Log("[mxohax] [AutoJackIn] Found %d existing operative entries in 0x00899B4C! Using entry #0 at 0x%p\n",
                (*ppCharEnd - *ppCharBegin) / 32, pCharToEnter);
        } else {
            *ppCharBegin = reinterpret_cast<DWORD>(&s_localCharEntry);
            *ppCharEnd   = reinterpret_cast<DWORD>(&s_localCharEntry) + sizeof(s_localCharEntry);
            pCharToEnter = &s_localCharEntry;
            Log("[mxohax] [AutoJackIn] Mounted operative s1acker (slums_barrens_full.metr) into vector at 0x00899B4C\n");
        }
    } else {
        pCharToEnter = &s_localCharEntry;
    }

    if (pCharToEnter) {
        Log("[mxohax] [AutoJackIn] Invoking EnterWorldWithCharacter(pWorldMgr=0x%p, pChar=0x%p)...\n",
            pWorldMgr, pCharToEnter);
        typedef void (__thiscall *EnterWorld_t)(void* pMgr, void* pChar);
        EnterWorld_t pEnterWorld = reinterpret_cast<EnterWorld_t>(clientBase + 0x00124070);
        pEnterWorld(pWorldMgr, pCharToEnter);
        DWORD* pCurState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        Log("[mxohax] [AutoJackIn] EnterWorldWithCharacter dispatched successfully! pWorldMgr State is now %u\n",
            pCurState ? *pCurState : 0);
    }

    return true;
}

// Direct3D 9 Backbuffer Capture & Hooks
typedef HRESULT (WINAPI *Direct3DCreate9Ex_t)(UINT SDKVersion, IDirect3D9Ex** ppD3D);
static Direct3DCreate9Ex_t OriginalDirect3DCreate9Ex = nullptr;

typedef IDirect3D9* (WINAPI *Direct3DCreate9_t)(UINT SDKVersion);
static Direct3DCreate9_t OriginalDirect3DCreate9 = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *Present_t)(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
static Present_t OriginalPresent = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *PresentEx_t)(IDirect3DDevice9Ex* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags);
static PresentEx_t OriginalPresentEx = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *CreateDevice_t)(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
static CreateDevice_t OriginalCreateDevice = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *CreateDeviceEx_t)(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface);
static CreateDeviceEx_t OriginalCreateDeviceEx = nullptr;

static void CaptureD3D9Backbuffer(IDirect3DDevice9* pDevice, const char* outBmpPath) {
    if (!pDevice) return;
    IDirect3DSurface9* pBackBuffer = nullptr;
    HRESULT hr = pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    if (FAILED(hr) || !pBackBuffer) {
        Log("[mxohax] CaptureD3D9: GetBackBuffer failed (hr=0x%08X)\n", hr);
        return;
    }
    D3DSURFACE_DESC desc;
    pBackBuffer->GetDesc(&desc);

    IDirect3DSurface9* pOffscreen = nullptr;
    hr = pDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &pOffscreen, nullptr);
    if (FAILED(hr) || !pOffscreen) {
        Log("[mxohax] CaptureD3D9: CreateOffscreenPlainSurface failed (hr=0x%08X)\n", hr);
        pBackBuffer->Release();
        return;
    }

    hr = pDevice->GetRenderTargetData(pBackBuffer, pOffscreen);
    pBackBuffer->Release();
    if (FAILED(hr)) {
        Log("[mxohax] CaptureD3D9: GetRenderTargetData failed (hr=0x%08X)\n", hr);
        pOffscreen->Release();
        return;
    }

    D3DLOCKED_RECT lr;
    hr = pOffscreen->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) {
        DWORD bmpSize = desc.Width * desc.Height * 4;
        BITMAPFILEHEADER bfh = { 0x4D42, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpSize, 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
        BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), (LONG)desc.Width, -((LONG)desc.Height), 1, 32, BI_RGB, bmpSize, 0, 0, 0, 0 };

        FILE* fp = fopen(outBmpPath, "wb");
        if (fp) {
            fwrite(&bfh, sizeof(bfh), 1, fp);
            fwrite(&bi, sizeof(bi), 1, fp);
            for (UINT y = 0; y < desc.Height; ++y) {
                BYTE* pSrcRow = (BYTE*)lr.pBits + (y * lr.Pitch);
                fwrite(pSrcRow, desc.Width * 4, 1, fp);
            }
            fclose(fp);
            Log("[mxohax] CaptureD3D9: SUCCESS! Saved hardware backbuffer to %s (%ux%u format=%u)!\n",
                outBmpPath, desc.Width, desc.Height, desc.Format);
        }
        pOffscreen->UnlockRect();
    } else {
        Log("[mxohax] CaptureD3D9: LockRect failed (hr=0x%08X)\n", hr);
    }
    pOffscreen->Release();
}

static int s_presentCount = 0;
static int s_inWorldPresents = 0;

static HRESULT STDMETHODCALLTYPE DetourPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    s_presentCount++;

    if (pDevice) {
        for (DWORD stage = 0; stage < 8; ++stage) {
            pDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
            pDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
            pDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, 16);
        }
    }

    if (s_inWorldSticky && pDevice) {
        s_inWorldPresents++;
        if (s_inWorldPresents == 30 || s_inWorldPresents == 60 || s_inWorldPresents == 90) {
            CaptureD3D9Backbuffer(pDevice, "E:\\Games\\The Matrix Online\\inworld_render.bmp");
        }
    }

    return OriginalPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

static HRESULT STDMETHODCALLTYPE DetourPresentEx(IDirect3DDevice9Ex* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags) {
    s_presentCount++;

    if (pDevice) {
        for (DWORD stage = 0; stage < 8; ++stage) {
            pDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
            pDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
            pDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, 16);
        }
    }

    if (s_inWorldSticky && pDevice) {
        s_inWorldPresents++;
        if (s_inWorldPresents == 30 || s_inWorldPresents == 60 || s_inWorldPresents == 90) {
            CaptureD3D9Backbuffer(pDevice, "E:\\Games\\The Matrix Online\\inworld_render.bmp");
        }
    }

    return OriginalPresentEx(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

static void HookDeviceVtable(void* pDevice, bool isEx) {
    if (!pDevice) return;
    void** devVtbl = *reinterpret_cast<void***>(pDevice);
    if (!devVtbl) return;

    if (!OriginalPresent) {
        if (MH_CreateHook(devVtbl[17], &DetourPresent, reinterpret_cast<LPVOID*>(&OriginalPresent)) == MH_OK) {
            MH_EnableHook(devVtbl[17]);
            Log("[mxohax] Hooked IDirect3DDevice9::Present (vtbl[17]) at 0x%p!\n", devVtbl[17]);
        }
    }
    if (isEx && !OriginalPresentEx) {
        if (MH_CreateHook(devVtbl[121], &DetourPresentEx, reinterpret_cast<LPVOID*>(&OriginalPresentEx)) == MH_OK) {
            MH_EnableHook(devVtbl[121]);
            Log("[mxohax] Hooked IDirect3DDevice9Ex::PresentEx (vtbl[121]) at 0x%p!\n", devVtbl[121]);
        }
    }
}

static HRESULT STDMETHODCALLTYPE DetourCreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) {
    if (pPresentationParameters) {
        Log("[mxohax] CreateDevice: original %ux%u (windowed=%d) -> enforcing 1920x1080\n",
            pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, pPresentationParameters->Windowed);
        pPresentationParameters->BackBufferWidth = 1920;
        pPresentationParameters->BackBufferHeight = 1080;
    }
    HRESULT hr = OriginalCreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        Log("[mxohax] CreateDevice: pDevice=0x%p, FocusWindow=0x%p\n", *ppReturnedDeviceInterface, hFocusWindow);
        HookDeviceVtable(*ppReturnedDeviceInterface, false);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE DetourCreateDeviceEx(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface) {
    if (pPresentationParameters) {
        Log("[mxohax] CreateDeviceEx: original %ux%u (windowed=%d) -> enforcing 1920x1080\n",
            pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, pPresentationParameters->Windowed);
        pPresentationParameters->BackBufferWidth = 1920;
        pPresentationParameters->BackBufferHeight = 1080;
    }
    HRESULT hr = OriginalCreateDeviceEx(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        Log("[mxohax] CreateDeviceEx: pDevice=0x%p, FocusWindow=0x%p\n", *ppReturnedDeviceInterface, hFocusWindow);
        HookDeviceVtable(*ppReturnedDeviceInterface, true);
    }
    return hr;
}

static IDirect3D9* WINAPI DetourDirect3DCreate9(UINT SDKVersion) {
    IDirect3D9* pD3D = OriginalDirect3DCreate9(SDKVersion);
    if (pD3D) {
        Log("[mxohax] Direct3DCreate9 intercepted: pD3D=0x%p\n", pD3D);
        void** vtbl = *reinterpret_cast<void***>(pD3D);
        if (!OriginalCreateDevice && vtbl) {
            if (MH_CreateHook(vtbl[16], &DetourCreateDevice, reinterpret_cast<LPVOID*>(&OriginalCreateDevice)) == MH_OK) {
                MH_EnableHook(vtbl[16]);
                Log("[mxohax] Hooked IDirect3D9::CreateDevice at 0x%p!\n", vtbl[16]);
            }
        }
    }
    return pD3D;
}

static HRESULT WINAPI DetourDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppD3D) {
    HRESULT hr = OriginalDirect3DCreate9Ex(SDKVersion, ppD3D);
    if (SUCCEEDED(hr) && ppD3D && *ppD3D) {
        IDirect3D9Ex* pD3D = *ppD3D;
        Log("[mxohax] Direct3DCreate9Ex intercepted: pD3D=0x%p\n", pD3D);
        void** vtbl = *reinterpret_cast<void***>(pD3D);
        if (!OriginalCreateDeviceEx && vtbl) {
            if (MH_CreateHook(vtbl[20], &DetourCreateDeviceEx, reinterpret_cast<LPVOID*>(&OriginalCreateDeviceEx)) == MH_OK) {
                MH_EnableHook(vtbl[20]);
                Log("[mxohax] Hooked IDirect3D9Ex::CreateDeviceEx at 0x%p!\n", vtbl[20]);
            }
        }
        if (!OriginalCreateDevice && vtbl) {
            if (MH_CreateHook(vtbl[16], &DetourCreateDevice, reinterpret_cast<LPVOID*>(&OriginalCreateDevice)) == MH_OK) {
                MH_EnableHook(vtbl[16]);
                Log("[mxohax] Hooked IDirect3D9Ex::CreateDevice at 0x%p!\n", vtbl[16]);
            }
        }
    }
    return hr;
}

// ============================================================================
// Safe CViewMissionContact and Phone Call Hooks
// ============================================================================
typedef void (__thiscall *ViewMissionContactDtor_t)(void* pThis);
static ViewMissionContactDtor_t OriginalViewMissionContactDtor = nullptr;

static void __fastcall Safe_ViewMissionContact_Dtor(void* pThis, void* /*edx*/) {
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

    void* p70 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x70);
    if (p70 && !IsBadReadPtr(p70, 0x20)) {
        *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(p70) + 0x18) = 0;
    }

    typedef void (__thiscall *BaseDtor_t)(void* pThis);
    BaseDtor_t pBaseDtor = reinterpret_cast<BaseDtor_t>(clientBase + 0x00016690);
    pBaseDtor(pThis);
}

typedef void (__thiscall *MissionContactCallFn)(void* pThis);
static MissionContactCallFn Original_MissionContact_Button_Call = nullptr;

typedef void (__thiscall *SendCallContactFn)(void* pThis, DWORD contactId);
static SendCallContactFn Original_SendCallContactPacket = nullptr;

static void __fastcall Safe_MissionContact_Button_Call(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_MissionContact_Button_Call called (pThis=0x%p)\n", pThis);
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) return;
    uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClient);

    // 1. Dispatch SendCallContactPacket (opcode 0x8090)
    SendCallContactFn pSendCall = Original_SendCallContactPacket ? Original_SendCallContactPacket : reinterpret_cast<SendCallContactFn>(clientBase + 0x0018C3A0);
    void* pContactMgr = reinterpret_cast<void*>(clientBase + 0x008A2440);
    __try {
        if (pContactMgr && !IsBadReadPtr(pContactMgr, 4)) {
            pSendCall(pContactMgr, 1);
            Log("[mxohax] Safe_MissionContact_Button_Call: dispatched SendCallContactPacket(contactId=1)\n");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Safe_MissionContact_Button_Call: exception in SendCallContactPacket!\n");
    }

    // 2. Safe handle +0x8C
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Safe_MissionContact_Button_Call: exception handling +0x8C\n");
    }

    // 3. Safe handle +0x70
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Safe_MissionContact_Button_Call: exception handling +0x70\n");
    }
}

static void __fastcall Safe_SendCallPacket(void* pThis, void* /*edx*/, DWORD contactId) {
    Log("[mxohax] Safe_SendCallPacket called: pThis=0x%p, contactId=%u\n", pThis, contactId);
    if (!pThis || IsBadReadPtr(pThis, 0x20)) return;
    __try {
        if (Original_SendCallContactPacket) {
            Original_SendCallContactPacket(pThis, contactId);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Safe_SendCallPacket: exception caught safely!\n");
    }
}

// ============================================================================
// Operative RSI Appearance Applier (Trenchcoat, sunglasses, hair, clothes)
// ============================================================================
static void ApplyOperativeAppearance(uintptr_t clientBase, void* pPlayer) {
    if (!pPlayer || IsBadReadPtr(pPlayer, 0xB0)) return;
    void* pRSI = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pPlayer) + 0xAC);
    if (!pRSI || IsBadReadPtr(pRSI, 0xB0)) {
        Log("[mxohax] ApplyOperativeAppearance: pRSI is null or invalid\n");
        return;
    }

    Log("[mxohax] Applying Operative RSI appearance (pPlayer=0x%p, pRSI=0x%p)...\n", pPlayer, pRSI);
    __try {
        typedef void (__thiscall *SetBodyType_t)(void* pRSI, int val);
        typedef void (__thiscall *SetHeadType_t)(void* pRSI, int val);
        typedef void (__thiscall *SetHairType_t)(void* pRSI, int val);
        typedef void (__thiscall *SetHat_t)(void* pRSI, int val);
        typedef void (__thiscall *EquipArticle_t)(void* pRSI, int slot, int articleId, int color);
        typedef void (__thiscall *RebuildRSI_t)(void* pRSI);
        typedef char (__cdecl *ApplyRSI_t)();

        SetBodyType_t pSetBody = reinterpret_cast<SetBodyType_t>(clientBase + 0x0051B490);
        SetHeadType_t pSetHead = reinterpret_cast<SetHeadType_t>(clientBase + 0x0051B4F0);
        SetHairType_t pSetHair = reinterpret_cast<SetHairType_t>(clientBase + 0x0051B4B0);
        SetHat_t pSetHat = reinterpret_cast<SetHat_t>(clientBase + 0x0051B4D0);
        EquipArticle_t pEquip = reinterpret_cast<EquipArticle_t>(clientBase + 0x0051B1E0);
        RebuildRSI_t pRebuild = reinterpret_cast<RebuildRSI_t>(clientBase + 0x0051B370);
        ApplyRSI_t pApply = reinterpret_cast<ApplyRSI_t>(clientBase + 0x000EE270);

        // 1. Populate the client.dll character appearance global tables
        // so that native client routines and future calls preserve the operative look
        *reinterpret_cast<short*>(clientBase + 0x0089E37C) = 100; // Body
        *reinterpret_cast<short*>(clientBase + 0x0089E3B4) = 100; // Head
        *reinterpret_cast<short*>(clientBase + 0x0089E3EC) = 101; // Hair (Operative Hair)
        *reinterpret_cast<short*>(clientBase + 0x0089E424) = 0;   // Hat

        struct OperativeItemDef {
            DWORD slot;
            DWORD articleId;
            BYTE color;
        };
        static const OperativeItemDef items[6] = {
            { 1, 106, 41 }, // Shirt
            { 2, 110, 8  }, // Coat (Black Trenchcoat)
            { 3, 103, 16 }, // Pants
            { 4, 100, 10 }, // Shoes (Boots)
            { 5, 101, 0  }, // Gloves
            { 6, 100, 1  }  // Glasses (Sunglasses)
        };
        for (int i = 0; i < 6; ++i) {
            uintptr_t entry = clientBase + 0x0089E760 + (i * 0x74);
            *reinterpret_cast<DWORD*>(entry) = items[i].slot;
            *reinterpret_cast<DWORD*>(entry + 0x34) = items[i].articleId;
            *reinterpret_cast<BYTE*>(entry + 0x6C) = items[i].color;
        }

        // 2. Set the operative components directly on pRSI
        pSetBody(pRSI, 100);
        pSetHead(pRSI, 100);
        pSetHair(pRSI, 101);
        pSetHat(pRSI, 0);

        // 3. Equip Operative Attire on pRSI:
        pEquip(pRSI, 0, 0, 0);     // Hat (None)
        pEquip(pRSI, 1, 106, 41);  // Shirt (106, 41)
        pEquip(pRSI, 2, 110, 8);   // Coat (110, 8 - Black Trenchcoat)
        pEquip(pRSI, 3, 103, 16);  // Pants (103, 16)
        pEquip(pRSI, 4, 100, 10);  // Shoes (100, 10 - Boots)
        pEquip(pRSI, 5, 101, 0);   // Gloves (101, 0)
        pEquip(pRSI, 6, 100, 1);   // Glasses (100, 1 - Sunglasses)

        // 4. Rebuild RSI visual mesh and textures with the operative configuration
        pRebuild(pRSI);
        Log("[mxohax] SUCCESS: Operative RSI fully equipped (Trenchcoat, sunglasses, boots, clothes, hair)!\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception in ApplyOperativeAppearance caught safely!\n");
    }
}

static void EnsureInWorldRendering(uintptr_t clientBase, void* pWorldMgr, DWORD pShell) {
    if (!pWorldMgr) return;

    // Step 0: Ensure fallback world pointer points to actual Slums METR world
    static const char s_defaultMetrPath[] = "resource/worlds/final_world/slums_barrens_full.metr";
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_defaultMetrPath;

    // Step 1: Ensure World is loaded via CWorldMgr::LoadWorldFile (0x10121110) BEFORE Player enters world
    BYTE* pWorldLoaded = reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27);
    if (pWorldLoaded && *pWorldLoaded == 0) {
        Log("[mxohax] EnsureInWorld: Calling CWorldMgr::LoadWorldFile (0x10121110) for %s...\n", s_defaultMetrPath);
        typedef char (__thiscall *LoadWorldFile_t)(void* pMgr);
        LoadWorldFile_t pLoadWorld = reinterpret_cast<LoadWorldFile_t>(clientBase + 0x00121110);
        char lres = pLoadWorld(pWorldMgr);
        Log("[mxohax] EnsureInWorld: LoadWorldFile returned %d (worldLoaded=%d)\n", lres, *pWorldLoaded);
    }

    // Step 2: Ensure PlayerObject is allocated, positioned in Slums, and enters world
    void** ppPlayerGlobal = reinterpret_cast<void**>(clientBase + 0x008A4378);
    void* pPlayer = *ppPlayerGlobal;
    if (!pPlayer) {
        typedef void* (__cdecl *AllocPlayer_t)();
        AllocPlayer_t pAlloc = reinterpret_cast<AllocPlayer_t>(clientBase + 0x001D2370);
        pPlayer = pAlloc();
        Log("[mxohax] EnsureInWorld: AllocPlayer(0x101d2370) -> 0x%p\n", pPlayer);

        if (pPlayer) {
            BYTE flag = 0;
            typedef void (__thiscall *PlayerCtor_t)(void* pThis, BYTE* pFlag, void* pExtraObj);
            PlayerCtor_t pCtor = reinterpret_cast<PlayerCtor_t>(clientBase + 0x001D17C0);
            pCtor(pPlayer, &flag, nullptr);
            Log("[mxohax] EnsureInWorld: PlayerCtor(0x101d17c0) initialized PlayerObject at 0x%p\n", pPlayer);

            // Set coordinates for operative s1acker in Slums: (16802.3f, 520.0f, 3237.01f)
            // Allocate full 64 bytes (16 floats) for 4x4 matrix/coords
            float* pPos = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94);
            if (!pPos) {
                pPos = reinterpret_cast<float*>(calloc(16, sizeof(float)));
                *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94) = pPos;
            }
            if (pPos) {
                pPos[0] = 16802.3f;
                pPos[1] = 520.0f;
                pPos[2] = 3237.01f;
                pPos[3] = 1.0f;
                pPos[4] = 0.0f;
                pPos[5] = 1.0f;
                pPos[15] = 1.0f;
                Log("[mxohax] EnsureInWorld: Set Player coordinates to Slums (%.1f, %.1f, %.1f)\n", pPos[0], pPos[1], pPos[2]);
            }

            typedef void (__thiscall *PlayerEnterWorld_t)(void* pPlayer);
            PlayerEnterWorld_t pEnter = reinterpret_cast<PlayerEnterWorld_t>(clientBase + 0x001D2180);
            pEnter(pPlayer);
            s_playerEnteredWorld = true;
            Log("[mxohax] EnsureInWorld: PlayerEnterWorld(0x101d2180) executed! [0x108a4378]=0x%p\n", *ppPlayerGlobal);
            ApplyOperativeAppearance(clientBase, pPlayer);
        }
    } else {
        s_playerEnteredWorld = true;
        float* pPos = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94);
        if (!pPos) {
            pPos = reinterpret_cast<float*>(calloc(16, sizeof(float)));
            *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94) = pPos;
        }
        if (pPos) {
            if (pPos[1] < 520.0f || (pPos[0] == 0.0f && pPos[2] == 0.0f)) {
                pPos[0] = 16802.3f;
                pPos[1] = 520.0f;
                pPos[2] = 3237.01f;
                pPos[3] = 1.0f;
                pPos[4] = 0.0f;
                pPos[5] = 1.0f;
                pPos[15] = 1.0f;
                Log("[mxohax] EnsureInWorld: Updated existing Player coordinates to Slums (%.1f, %.1f, %.1f)\n", pPos[0], pPos[1], pPos[2]);
            }
        }
        ApplyOperativeAppearance(clientBase, pPlayer);
    }

    // Step 2b: Invoke native AdvanceToState3 (0x10121B50) to attach camera, bind scene, and start 3D simulation!
    static bool s_advanceToState3Done = false;
    if (pPlayer && !s_advanceToState3Done) {
        s_advanceToState3Done = true;
        Log("[mxohax] EnsureInWorld: Invoking native AdvanceToState3 (0x10121B50) on pWorldMgr=0x%p, pPlayer=0x%p...\n", pWorldMgr, pPlayer);
        typedef void (__thiscall *AdvanceToState3_t)(void* pMgr, void* pPlayer);
        AdvanceToState3_t pAdv3 = reinterpret_cast<AdvanceToState3_t>(clientBase + 0x00121B50);
        pAdv3(pWorldMgr, pPlayer);
        DWORD* pCurState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        Log("[mxohax] EnsureInWorld: AdvanceToState3 dispatched! State is now %u\n", pCurState ? *pCurState : 0);
    }

    // Step 3: Guaranteed World Engine and World Instance creation for [0x1089DD6C]
    void** ppWorldInst = reinterpret_cast<void**>(clientBase + 0x0089DD6C);
    typedef void* (__cdecl *GetWorldEngine_t)(DWORD);
    GetWorldEngine_t pGetEngine = reinterpret_cast<GetWorldEngine_t>(clientBase + 0x003A5F30);
    DWORD engArg = *reinterpret_cast<DWORD*>(clientBase + 0x00897F90);
    void* pWorldEngine = pGetEngine(engArg);
    Log("[mxohax] EnsureInWorld: Initial pWorldEngine=0x%p, [0x1089DD6C]=0x%p\n",
        pWorldEngine, ppWorldInst ? *ppWorldInst : nullptr);

    if (!pWorldEngine) {
        Log("[mxohax] EnsureInWorld: Calling InitWorldEngine (0x103A5AC0)...\n");
        typedef void (__cdecl *InitWorldEngine_t)();
        InitWorldEngine_t pInitEngine = reinterpret_cast<InitWorldEngine_t>(clientBase + 0x003A5AC0);
        pInitEngine();
        pWorldEngine = pGetEngine(engArg);
        Log("[mxohax] EnsureInWorld: Post-init pWorldEngine=0x%p\n", pWorldEngine);
    }

    if (pWorldEngine && ppWorldInst && !*ppWorldInst) {
        float dummyMat[16] = {
            16802.3f, 520.0f, 3237.01f, 1.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        float* pMat = dummyMat;
        if (pPlayer) {
            float* pPlayerPos = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94);
            if (pPlayerPos) pMat = pPlayerPos;
        }
        Log("[mxohax] EnsureInWorld: Invoking pWorldEngine->vtable[0](pMat, 0x1011FF10, 0) to instantiate World...\n", pWorldEngine);
        typedef void* (__thiscall *CreateWorldInst_t)(void* pEng, void* pMatrix, void* pFn, int flag);
        CreateWorldInst_t* vtable = *reinterpret_cast<CreateWorldInst_t**>(pWorldEngine);
        if (vtable && vtable[0]) {
            void* pInst = vtable[0](pWorldEngine, pMat, reinterpret_cast<void*>(clientBase + 0x0011FF10), 0);
            *ppWorldInst = pInst;
            Log("[mxohax] EnsureInWorld: SUCCESS! Instantiated World Instance 0x%p into [0x1089DD6C]!\n", pInst);
        }
    }

    // Step 4: Ensure Camera is instantiated and oriented in Slums
    void** ppCamera = reinterpret_cast<void**>(clientBase + 0x0089EDF8);
    if (ppCamera) {
        if (!*ppCamera) {
            void* pCam = malloc(0x118);
            if (pCam) {
                memset(pCam, 0, 0x118);
                typedef void (__thiscall *CamCtor_t)(void*);
                CamCtor_t pCamCtor = reinterpret_cast<CamCtor_t>(clientBase + 0x0012F020);
                pCamCtor(pCam);
                *ppCamera = pCam;
                Log("[mxohax] EnsureInWorld: Instantiated fallback camera at 0x%p into [0x1089edf8]!\n", pCam);
            }
        }
        if (*ppCamera) {
            DWORD pCamAddr = reinterpret_cast<DWORD>(*ppCamera);
            float* pCamPos = reinterpret_cast<float*>(pCamAddr + 0x30);
            if (pCamPos && ((pCamPos[0] == 0.0f && pCamPos[2] == 0.0f) || pCamPos[1] < 525.0f)) {
                pCamPos[0] = 16802.3f;
                pCamPos[1] = 535.0f;
                pCamPos[2] = 3180.0f;
                Log("[mxohax] EnsureInWorld: Set camera position at +0x30 to Slums street view (%.1f, %.1f, %.1f)\n",
                    pCamPos[0], pCamPos[1], pCamPos[2]);
            }
        }
    }

    // Step 5: Activate in-world display & viewport via official UI SetControlVisible(0x1B, 1)
    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (pUI) {
        SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
        pSetVisible(pUI, 0x1B, 1);
        Log("[mxohax] EnsureInWorld: Dispatched pUI->SetControlVisible(0x1B, 1)!\n");
    }

    // Step 6: Ensure pWorldMgr + 0xC (viewport list) has a valid Viewport object
    void** ppListHead = reinterpret_cast<void**>(reinterpret_cast<DWORD>(pWorldMgr) + 0xC);
    if (ppListHead && *ppListHead) {
        void* head = *ppListHead;
        void* first = *reinterpret_cast<void**>(head);
        if (first == head) {
            // Viewport list empty -> call CWorldMgr::CreateViewport(0x1011EAD0)
            int width = *reinterpret_cast<int*>(clientBase + 0x00896CCC);
            int height = *reinterpret_cast<int*>(clientBase + 0x00896D04);
            if (width <= 0 || height <= 0) {
                width = 1024;
                height = 768;
            }
            Log("[mxohax] EnsureInWorld: Calling CWorldMgr::CreateViewport(0x1011EAD0) with %dx%d (pCam=0x%p)...\n",
                width, height, ppCamera ? *ppCamera : nullptr);
            typedef void (__thiscall *CreateViewport_t)(void* pMgr, int w, int h);
            CreateViewport_t pCreateVp = reinterpret_cast<CreateViewport_t>(clientBase + 0x0011EAD0);
            pCreateVp(pWorldMgr, width, height);

            void* newFirst = *reinterpret_cast<void**>(head);
            Log("[mxohax] EnsureInWorld: CreateViewport returned. List head->next is now 0x%p\n", newFirst);

            if (newFirst == head) {
                // Fallback: instantiate Viewport via Engine Object Factory (0x1023f3e0) with Class ID 0x28000831
                void* pFactory = *reinterpret_cast<void**>(clientBase + 0x008A9180);
                if (pFactory) {
                    DWORD classId = 0x28000831;
                    typedef void* (__thiscall *CreateObj_t)(void* pFac, DWORD* pId);
                    CreateObj_t pCreateObj = reinterpret_cast<CreateObj_t>(clientBase + 0x0023F3E0);
                    void* pVp = pCreateObj(pFactory, &classId);
                    Log("[mxohax] EnsureInWorld: Factory::CreateObject(0x28000831) -> 0x%p\n", pVp);
                    if (pVp) {
                        typedef void (__thiscall *ListPushBack_t)(void* pList, void** ppItem);
                        ListPushBack_t pPush = reinterpret_cast<ListPushBack_t>(clientBase + 0x00045A60);
                        pPush(ppListHead, &pVp);
                        Log("[mxohax] EnsureInWorld: Pushed factory viewport 0x%p into pWorldMgr+0xC list!\n", pVp);
                    }
                }
            }
        }
    }

    // Step 7: Ensure pWorldMgr + 8 (viewport count) is 1
    DWORD* pVpCount = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 8);
    if (pVpCount && *pVpCount == 0) {
        DWORD one = 1;
        typedef void (__thiscall *SetVpCount_t)(void* pMgr, DWORD* pCount);
        SetVpCount_t pSetVp = reinterpret_cast<SetVpCount_t>(clientBase + 0x00114680);
        pSetVp(pWorldMgr, &one);
        Log("[mxohax] EnsureInWorld: SetViewportCount(0x10114680) called -> pWorldMgr+8 is %u\n", *pVpCount);
    }

    // Step 8: Set render & in-world flags
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27) = 1;
    if (pShell) {
        *reinterpret_cast<BYTE*>(pShell + 0x20) = 1; // CClientShell::m_inWorld = 1
        HWND hWnd = *reinterpret_cast<HWND*>(pShell + 0x14);
        if (hWnd) {
            ShowWindow(hWnd, SW_RESTORE);
            SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 1920, 1080, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(hWnd);
        }
    }

    DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
    if (pState) {
        *pState = 3;
    }
    s_inWorldSticky = true;
    Log("[mxohax] ******************************************************\n");
    Log("[mxohax] *** PROMOTED TO STATE 3 (IN-WORLD)! 3D SIMULATION ACTIVE! ***\n");
    Log("[mxohax] ******************************************************\n");

    // Dismiss 2D loading screens
    if (pUI && OriginalHideControl) {
        OriginalHideControl(pUI, 0x04);
        OriginalHideControl(pUI, 0x57);
        OriginalHideControl(pUI, 0x30);
        OriginalHideControl(pUI, 0x5D);
        Log("[mxohax] EnsureInWorld: Dismissed loading screens 0x04, 0x57, 0x30 and 0x5D!\n");
    }
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
            if (s_lastState == 3) {
                s_inWorldSticky = true;
            }
        }
    }

    if (inWorld == 1) {
        s_inWorldSticky = true;
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
        if (!s_inWorldDismissedOnce) {
            s_inWorldDismissedOnce = true;
            void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
            if (pUI && OriginalHideControl) {
                OriginalHideControl(pUI, 0x04);
                OriginalHideControl(pUI, 0x57);
                OriginalHideControl(pUI, 0x30);
                OriginalHideControl(pUI, 0x5D);
            }
            if (pUI) {
                SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
                pSetVisible(pUI, 0x1B, 1);
            }
            void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
            if (curPlayer) {
                ApplyOperativeAppearance(clientBase, curPlayer);
            }
            Log("[mxohax] In-world UI initialized and loading screens dismissed once.\n");
        }

        // Keep Viewport count active for 3D rendering
        if (pWorldMgr) {
            DWORD* pVpCount = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 8);
            if (pVpCount && *pVpCount == 0) {
                *pVpCount = 1;
            }
        }

        // Ensure World Instance remains valid in [0x1089DD6C]
        void** ppWorldInstSticky = reinterpret_cast<void**>(clientBase + 0x0089DD6C);
        if (ppWorldInstSticky && !*ppWorldInstSticky) {
            typedef void* (__cdecl *GetWorldEngine_t)(DWORD);
            GetWorldEngine_t pGetEngine = reinterpret_cast<GetWorldEngine_t>(clientBase + 0x003A5F30);
            DWORD engArg = *reinterpret_cast<DWORD*>(clientBase + 0x00897F90);
            void* pWorldEngine = pGetEngine(engArg);
            if (!pWorldEngine) {
                typedef void (__cdecl *InitWorldEngine_t)();
                InitWorldEngine_t pInitEngine = reinterpret_cast<InitWorldEngine_t>(clientBase + 0x003A5AC0);
                pInitEngine();
                pWorldEngine = pGetEngine(engArg);
            }
            if (pWorldEngine) {
                typedef void* (__thiscall *CreateWorldInst_t)(void* pEng, void* pMatrix, void* pFn, int flag);
                CreateWorldInst_t* vtable = *reinterpret_cast<CreateWorldInst_t**>(pWorldEngine);
                if (vtable && vtable[0]) {
                    float dummyMat[16] = {
                        16802.3f, 520.0f, 3237.01f, 1.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f
                    };
                    *ppWorldInstSticky = vtable[0](pWorldEngine, dummyMat, reinterpret_cast<void*>(clientBase + 0x0011FF10), 0);
                    Log("[mxohax] DetourFrameTick: Re-ensured [0x1089DD6C] = 0x%p\n", *ppWorldInstSticky);
                }
            }
        }

        // Capture in-world screenshot verification after entering world
        static bool s_savedScreenshot = false;
        if (!s_savedScreenshot && s_tickCount >= 100) {
            s_savedScreenshot = true;
            HWND hWnd = *reinterpret_cast<HWND*>(pShell + 0x14);
            if (!hWnd) hWnd = GetActiveWindow();
            if (!hWnd) hWnd = GetForegroundWindow();
            Log("[mxohax] In-world screenshot capture: HWND=0x%p\n", hWnd);
            if (hWnd) {
                RECT rc;
                GetClientRect(hWnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                Log("[mxohax] In-world HWND client rect: %dx%d\n", w, h);
                if (w > 0 && h > 0) {
                    HDC hdcWnd = GetDC(hWnd);
                    if (hdcWnd) {
                        HDC hdcMem = CreateCompatibleDC(hdcWnd);
                        HBITMAP hbm = CreateCompatibleBitmap(hdcWnd, w, h);
                        HGDIOBJ oldBm = SelectObject(hdcMem, hbm);
                        PrintWindow(hWnd, hdcMem, 2);
                        BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), w, h, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
                        DWORD bmpSize = w * h * 4;
                        BYTE* pPixels = (BYTE*)malloc(bmpSize);
                        if (pPixels) {
                            GetDIBits(hdcMem, hbm, 0, h, pPixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
                            BITMAPFILEHEADER bfh = { 0x4D42, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpSize, 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
                            FILE* fp = fopen("inworld_render.bmp", "wb");
                            if (fp) {
                                fwrite(&bfh, sizeof(bfh), 1, fp);
                                fwrite(&bi, sizeof(bi), 1, fp);
                                fwrite(pPixels, bmpSize, 1, fp);
                                fclose(fp);
                                Log("[mxohax] Successfully saved in-world screenshot to inworld_render.bmp (%dx%d)!\n", w, h);
                            }
                            free(pPixels);
                        }
                        SelectObject(hdcMem, oldBm);
                        DeleteObject(hbm);
                        DeleteDC(hdcMem);
                        ReleaseDC(hWnd, hdcWnd);
                    }
                }
            }
        }
    }

    // Instantiating world camera ONLY when in-world (State 3) and Camera is NULL
    if (pWorldMgr) {
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState && (*pState == 3 || s_inWorldSticky)) {
            void** ppCamera = reinterpret_cast<void**>(clientBase + 0x0089EDF8);
            if (ppCamera && !*ppCamera) {
                void* pCam = malloc(0x118);
                if (pCam) {
                    memset(pCam, 0, 0x118);
                    typedef void (__thiscall *CamCtor_t)(void*);
                    CamCtor_t pCamCtor = reinterpret_cast<CamCtor_t>(clientBase + 0x0012F020);
                    pCamCtor(pCam);
                    *ppCamera = pCam;
                    Log("[mxohax] Instantiated world camera at 0x%p into [0x1089edf8]!\n", pCam);
                }
            }
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
                OriginalHideControl(pUI, 0x30);
                OriginalHideControl(pUI, 0x5D);
                Log("[mxohax] Dismissed loading screens 0x04, 0x57, 0x30 and 0x5D upon entering world!\n");
            }
        }
    }

    if (s_tickCount % 500 == 0) {
        void* curWorldInst = *reinterpret_cast<void**>(clientBase + 0x0089DD6C);
        void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
        void* curCam = *reinterpret_cast<void**>(clientBase + 0x0089EDF8);
        DWORD curState = pWorldMgr ? *reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C) : 0;
        BYTE curRenderFlag = pWorldMgr ? *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) : 0;
        Log("[mxohax] DetourFrameTick: Tick #%d active (State=%u, renderFlag=%d, inWorld=%u, sticky=%d, WorldInst=0x%p, Player=0x%p, Cam=0x%p)\n",
            s_tickCount, curState, curRenderFlag, inWorld, s_inWorldSticky ? 1 : 0, curWorldInst, curPlayer, curCam);
    }

    // State machine management
    static int s_state1Ticks = 0;
    static int s_state2Ticks = 0;
    static int s_state4Ticks = 0;
    if (pWorldMgr && !s_inWorldSticky) {
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState) {
            if (*pState == 1) {
                s_state1Ticks++;
                // Allow Margin auth and Screen 0x5D to execute naturally!
                // Only trigger fallback AutoJackIn if stuck in State 1 for > 500 ticks
                if (s_state1Ticks >= 500 && !s_autoJackInDone) {
                    Log("[mxohax] DetourFrameTick: State 1 safety threshold reached (tick %d) -> triggering AutoJackIn...\n", s_state1Ticks);
                    if (TryAutoJackIn(clientBase)) {
                        s_autoJackInDone = true;
                    }
                }
            } else if (*pState == 2) {
                s_state2Ticks++;
                if (s_state2Ticks % 50 == 0) {
                    Log("[mxohax] DetourFrameTick: State 2 active (tick %d). Waiting for world loading / streaming...\n", s_state2Ticks);
                }
                // Safety fallback if State 2 never transitions to State 4
                if (s_state2Ticks >= 600 && !s_playerEnteredWorld) {
                    Log("[mxohax] DetourFrameTick: State 2 safety threshold reached (tick %d) -> ensuring in-world rendering...\n", s_state2Ticks);
                    EnsureInWorldRendering(clientBase, pWorldMgr, pShell);
                }
            } else if (*pState == 4) {
                s_state4Ticks++;
                if (s_state4Ticks % 30 == 0) {
                    Log("[mxohax] DetourFrameTick: State 4 (Streaming) active (tick %d)...\n", s_state4Ticks);
                }
                
                // Check if streaming completed
                typedef char (__thiscall *IsFinished_t)(void* pLevelSys);
                void* pLevelSys = *reinterpret_cast<void**>(clientBase + 0x008A6004);
                char finished = 0;
                if (pLevelSys) {
                    IsFinished_t pIsFinished = reinterpret_cast<IsFinished_t>(clientBase + 0x002066C0);
                    finished = pIsFinished(pLevelSys);
                }
                if ((finished && s_state4Ticks >= 10) || s_state4Ticks >= 150) {
                    Log("[mxohax] DetourFrameTick: Streaming complete (finished=%d, ticks=%d)!\n",
                        finished, s_state4Ticks);

                    // Ensure active world geometry buffer is ready (0xE0 = 1, 0xB9 = 1)
                    if (pLevelSys) {
                        DWORD* pActiveWorld = *reinterpret_cast<DWORD**>(reinterpret_cast<DWORD>(pLevelSys) + 0x18);
                        if (pActiveWorld) {
                            *reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pActiveWorld) + 0xE0) = 1;
                            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pActiveWorld) + 0xB9) = 1;
                            Log("[mxohax] DetourFrameTick: Marked active world at 0x%p geometry ready (0xE0=1, 0xB9=1)!\n", pActiveWorld);
                        }
                    }

                    EnsureInWorldRendering(clientBase, pWorldMgr, pShell);
                }
            }
        }
    }

    if (!s_autoJackInDone) {
        if (s_screen5DActive) {
            s_screen5DFrames++;
            if (s_screen5DFrames >= 5) {
                Log("[mxohax] DetourFrameTick: Screen 0x5D active for %d frames -> triggering AutoJackIn...\n", s_screen5DFrames);
                if (TryAutoJackIn(clientBase)) {
                    s_autoJackInDone = true;
                }
            }
        }
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

    __try {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (!hClient) return 0;
        uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClient);

        uintptr_t* ppGlobal = reinterpret_cast<uintptr_t*>(clientBase + 0x008A4378);
        if (IsBadReadPtr(ppGlobal, sizeof(uintptr_t)) || !*ppGlobal) return 0;
        uintptr_t pGlobal = *ppGlobal;

        uintptr_t* ppA8 = reinterpret_cast<uintptr_t*>(pGlobal + 0xA8);
        if (IsBadReadPtr(ppA8, sizeof(uintptr_t)) || !*ppA8) return 0;
        uintptr_t pA8 = *ppA8;

        uintptr_t* pp23C = reinterpret_cast<uintptr_t*>(pA8 + 0x23C);
        if (IsBadReadPtr(pp23C, sizeof(uintptr_t)) || !*pp23C) return 0;
        uintptr_t p23C = *pp23C;

        uintptr_t* ppESI = reinterpret_cast<uintptr_t*>(p23C);
        if (IsBadReadPtr(ppESI, sizeof(uintptr_t)) || !*ppESI) return 0;
        uintptr_t pESI = *ppESI;

        if (IsBadReadPtr(reinterpret_cast<void*>(pESI), 6)) return 0;
        if (*reinterpret_cast<uint16_t*>(pESI + 4) == 0xFFFF) return 0;

        uint16_t idx0 = *reinterpret_cast<uint16_t*>(pESI);
        uintptr_t* ppMgr = reinterpret_cast<uintptr_t*>(clientBase + 0x00897F90);
        if (IsBadReadPtr(ppMgr, sizeof(uintptr_t)) || !*ppMgr) return 0;
        uintptr_t pMgr = *ppMgr;

        typedef void* (__thiscall *FnGetObj)(void* thisPtr, uint32_t id);
        FnGetObj pfnGetObj = reinterpret_cast<FnGetObj>(clientBase + 0x003A5CA0);
        void* obj = pfnGetObj(reinterpret_cast<void*>(pMgr), idx0);
        if (outObj) *outObj = obj;
        if (!obj || IsBadReadPtr(obj, sizeof(void*))) return 0;

        uint16_t idx1 = *reinterpret_cast<uint16_t*>(pESI + 2);
        void*** pppVtable = reinterpret_cast<void***>(obj);
        if (IsBadReadPtr(pppVtable, sizeof(void**)) || !*pppVtable) return 0;
        void** vtable = *pppVtable;
        if (IsBadReadPtr(vtable, 0x60)) return 0;

        typedef void* (__thiscall *FnGetSubObj)(void* thisPtr, uint32_t id);
        FnGetSubObj pfnGetSubObj = reinterpret_cast<FnGetSubObj>(vtable[0x58 / 4]);
        if (!pfnGetSubObj) return 0;

        void* subObj = pfnGetSubObj(obj, idx1);
        if (outSubObj) *outSubObj = subObj;

        return (subObj != nullptr) ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// ============================================================================
// Safe CViewInterlock Tactic Button Hooks (client.dll + 0x0009A170 - 0x0009A260)
// Guards against null/invalid button pointers and network exceptions
// ============================================================================
typedef void (__thiscall *InterlockButtonFn)(void* pThis);

static InterlockButtonFn Original_Interlock_Button_Withdraw = nullptr;
static InterlockButtonFn Original_Interlock_Grab_Button = nullptr;
static InterlockButtonFn Original_Interlock_Power_Button = nullptr;
static InterlockButtonFn Original_Interlock_Speed_Button = nullptr;

static void __fastcall Safe_Interlock_Button_Withdraw(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Button_Withdraw called (pThis=0x%p)\n", pThis);
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x6C);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Button_Withdraw: button ptr (+0x6C) is null/invalid\n");
        return;
    }
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) {
        Log("[mxohax] Safe_Interlock_Button_Withdraw: button vtable is null/invalid\n");
        return;
    }
    __try {
        if (Original_Interlock_Button_Withdraw) {
            Original_Interlock_Button_Withdraw(pThis);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception caught safely in Safe_Interlock_Button_Withdraw!\n");
    }
}

static void __fastcall Safe_Interlock_Grab_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Grab_Button called (pThis=0x%p)\n", pThis);
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x80);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Grab_Button: button ptr (+0x80) is null/invalid\n");
        return;
    }
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) {
        Log("[mxohax] Safe_Interlock_Grab_Button: button vtable is null/invalid\n");
        return;
    }
    __try {
        if (Original_Interlock_Grab_Button) {
            Original_Interlock_Grab_Button(pThis);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception caught safely in Safe_Interlock_Grab_Button!\n");
    }
}

static void __fastcall Safe_Interlock_Power_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Power_Button called (pThis=0x%p)\n", pThis);
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x74);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Power_Button: button ptr (+0x74) is null/invalid\n");
        return;
    }
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) {
        Log("[mxohax] Safe_Interlock_Power_Button: button vtable is null/invalid\n");
        return;
    }
    __try {
        if (Original_Interlock_Power_Button) {
            Original_Interlock_Power_Button(pThis);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception caught safely in Safe_Interlock_Power_Button!\n");
    }
}

static void __fastcall Safe_Interlock_Speed_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Speed_Button called (pThis=0x%p)\n", pThis);
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x70);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Speed_Button: button ptr (+0x70) is null/invalid\n");
        return;
    }
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) {
        Log("[mxohax] Safe_Interlock_Speed_Button: button vtable is null/invalid\n");
        return;
    }
    __try {
        if (Original_Interlock_Speed_Button) {
            Original_Interlock_Speed_Button(pThis);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception caught safely in Safe_Interlock_Speed_Button!\n");
    }
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

static uintptr_t g_retWmNormal = 0;
static uintptr_t g_retWmSkip = 0;

__declspec(naked) void Hook_WorldMgrCCCheck() {
    __asm {
        mov cl, byte ptr [eax + 0xc8]
        test cl, cl
        je _wm_skip
        cmp dword ptr [eax + 0xcc], 0
        je _wm_skip
        jmp dword ptr [g_retWmNormal]

    _wm_skip:
        jmp dword ptr [g_retWmSkip]
    }
}

typedef int (__thiscall *ParseSubpacket_t)(void* pThis, const byte* pData, int len);
static ParseSubpacket_t OriginalParseSubpacket = nullptr;

static int __fastcall DetourParseSubpacket(void* pThis, void* /*edx*/, const byte* pData, int len) {
    if (pData && len >= 4) {
        char hex[128] = {0};
        for (int i = 0; i < len && i < 24; ++i) {
            sprintf(hex + i * 3, "%02X ", pData[i]);
        }
        WORD count = *reinterpret_cast<const WORD*>(pData + 2);
        Log("[mxohax] ParseSubpacket(0x10255710): len=%d, count=%u, hex: %s\n", len, count, hex);

        if (count > 256 || (len > 4 && (DWORD)count * 8 > (DWORD)len)) {
            Log("[mxohax] WARNING: ParseSubpacket invalid property count %u (len=%d)! Intercepting to prevent crash.\n", count, len);
            if (pData[0] == 0xCD && pData[1] == 0xAB && len >= 190) {
                Log("[mxohax] Sanitizing 202-byte sampleSpawnPacket: advancing 190 bytes directly to viewId.\n");
                return 190;
            }
            return 4;
        }
    }
    return OriginalParseSubpacket ? OriginalParseSubpacket(pThis, pData, len) : 0;
}

static void ApplyClientPatches(HMODULE hClient) {
    static bool s_clientPatched = false;
    if (s_clientPatched || !hClient) return;
    s_clientPatched = true;

    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    Log("[mxohax] Applying client.dll hooks at base 0x%p...\n", (void*)clientBase);

    // Hook ParseSubpacket at 0x00255710
    LPVOID pParseSub = reinterpret_cast<LPVOID>(clientBase + 0x00255710);
    if (MH_CreateHook(pParseSub, &DetourParseSubpacket, reinterpret_cast<LPVOID*>(&OriginalParseSubpacket)) == MH_OK) {
        MH_EnableHook(pParseSub);
        Log("[mxohax] SUCCESS: client.dll ParseSubpacket hooked at 0x%p!\n", pParseSub);
    }

    // Hook InitClientDLL at 0x00001270 to enforce autoJackIn=1
    LPVOID pInitClient = reinterpret_cast<LPVOID>(clientBase + 0x00001270);
    if (MH_CreateHook(pInitClient, &DetourInitClientDLL, reinterpret_cast<LPVOID*>(&OriginalInitClientDLL)) == MH_OK) {
        MH_EnableHook(pInitClient);
        Log("[mxohax] SUCCESS: client.dll InitClientDLL hooked at 0x%p!\n", pInitClient);
    }

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

    // 3. Hook HideControl & SetControlVisible (0x0001D3C0 & 0x0001DB80)
    LPVOID pHideControl = reinterpret_cast<LPVOID>(clientBase + 0x0001D3C0);
    if (MH_CreateHook(pHideControl, &DetourHideControl, reinterpret_cast<LPVOID*>(&OriginalHideControl)) == MH_OK) {
        MH_EnableHook(pHideControl);
    }
    LPVOID pSetCtrlVis = reinterpret_cast<LPVOID>(clientBase + 0x0001DB80);
    if (MH_CreateHook(pSetCtrlVis, &DetourSetControlVisible, reinterpret_cast<LPVOID*>(&OriginalSetControlVisible)) == MH_OK) {
        MH_EnableHook(pSetCtrlVis);
        Log("[mxohax] SUCCESS: client.dll SetControlVisible hooked at 0x%p!\n", pSetCtrlVis);
    }

    // 4. Hook GetPlayerActiveObject (0x0010A210) to guard against NULL player entity dereference
    LPVOID pGetActiveObj = reinterpret_cast<LPVOID>(clientBase + 0x0010A210);
    if (MH_CreateHook(pGetActiveObj, &Safe_GetPlayerActiveObject, reinterpret_cast<LPVOID*>(&OriginalGetPlayerActiveObject)) == MH_OK) {
        MH_EnableHook(pGetActiveObj);
        Log("[mxohax] SUCCESS: client.dll GetPlayerActiveObject hooked at 0x%p! Null dereference guarded.\n", pGetActiveObj);
    }

    // 5. Enforce 1920x1080 render display resolution and disable blurry glow globals
    *reinterpret_cast<int*>(clientBase + 0x00896CCC) = 1920;
    *reinterpret_cast<int*>(clientBase + 0x00896D04) = 1080;
    Log("[mxohax] Enforced 1920x1080 resolution globals in client.dll.\n");

    *reinterpret_cast<int*>(clientBase + 0x008A342C) = 0;      // ScreenFilters_Screen_Glow_Just_Glow = 0
    *reinterpret_cast<float*>(clientBase + 0x008A3508) = 0.0f; // ScreenFilters_Screen_Glow_BlurScale = 0.0
    Log("[mxohax] Disabled blurry Just_Glow post-process globals in client.dll.\n");

    // 5.5 Set default fallback world file pointer at clientBase + 0x00896E4C to slums METR world
    static const char s_initMetrPath[] = "resource/worlds/final_world/slums_barrens_full.metr";
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_initMetrPath;
    Log("[mxohax] SUCCESS: Configured fallback world file pointer [0x%08X] -> %s\n",
        clientBase + 0x00896E4C, s_initMetrPath);

    // 6. Direct World Load Patches:
    // Patch A: Preserved native client.dll + 0x0012196E (movzx esi, bl) so character selection and auto-login operate naturally.
    Log("[mxohax] Preserved native character select logic at client.dll + 0x0012196E.\n");

    // Patch B: Removed. Leaving native clean ret 0x14 at 0x10121AE6 so the function epilogue executes cleanly.
    Log("[mxohax] Preserved native clean character load epilogue at client.dll + 0x00121AE6.\n");

    // Patch J: 0x0012B388: 2 bytes (EB 09 instead of 74 09)
    // Bypasses destructive LeaveWorld(0x1012A3D0) when CLevelSystem::IsFinished returns 1 in State 4
    DWORD oldProt = 0;
    LPVOID pLeaveWorldBypass = reinterpret_cast<LPVOID>(clientBase + 0x0012B388);
    if (VirtualProtect(pLeaveWorldBypass, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patchJ[2] = { 0xEB, 0x09 }; // jmp short +0x09
        memcpy(pLeaveWorldBypass, patchJ, 2);
        VirtualProtect(pLeaveWorldBypass, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pLeaveWorldBypass, 2);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0012B388 (EB 09) to bypass premature LeaveWorld in State 4!\n");
    }

    // Patch C: 0x0012B3EE: 16 bytes safe camera check
    g_pEdf8Addr = clientBase + 0x0089EDF8;
    g_retCameraNormal = clientBase + 0x0012B3FE;
    g_retCameraSkip = clientBase + 0x0012B4F1;
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

    // Patch E: 0x0016D410: 3 bytes safe vector population bypass (ret 8: C2 08 00)
    // Guards against uninitialized/corrupted buffer pointer crash at client.dll + 0x0016D495
    LPVOID pVecPatch = reinterpret_cast<LPVOID>(clientBase + 0x0016D410);
    if (VirtualProtect(pVecPatch, 3, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[3] = { 0xC2, 0x08, 0x00 }; // ret 8
        memcpy(pVecPatch, patch, 3);
        VirtualProtect(pVecPatch, 3, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pVecPatch, 3);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0016D410 (ret 8) to guard against vector corruption!\n");
    }

    // Patch F: 0x00162270: 3 bytes safe vector save bypass (ret 4: C2 04 00)
    // Guards against uninitialized/corrupted vector serialization crash at client.dll + 0x001622AE
    LPVOID pSavePatch = reinterpret_cast<LPVOID>(clientBase + 0x00162270);
    if (VirtualProtect(pSavePatch, 3, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[3] = { 0xC2, 0x04, 0x00 }; // ret 4
        memcpy(pSavePatch, patch, 3);
        VirtualProtect(pSavePatch, 3, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pSavePatch, 3);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x00162270 (ret 4) to guard against vector save crash!\n");
    }

    // Patch G: 0x0022CF56: 2 bytes: mov al, 1 (B0 01)
    // Prevents Subpacket 0x0C failure when dynamic object creation returns NULL
    LPVOID pSub0cFail1 = reinterpret_cast<LPVOID>(clientBase + 0x0022CF56);
    if (VirtualProtect(pSub0cFail1, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[2] = { 0xB0, 0x01 }; // mov al, 1
        memcpy(pSub0cFail1, patch, 2);
        VirtualProtect(pSub0cFail1, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pSub0cFail1, 2);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0022CF56 (mov al, 1) to prevent subpacket 0x0C abort!\n");
    }

    // Patch H: 0x0022D215: 2 bytes: mov al, 1 (B0 01)
    LPVOID pSub0cFail2 = reinterpret_cast<LPVOID>(clientBase + 0x0022D215);
    if (VirtualProtect(pSub0cFail2, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[2] = { 0xB0, 0x01 }; // mov al, 1
        memcpy(pSub0cFail2, patch, 2);
        VirtualProtect(pSub0cFail2, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pSub0cFail2, 2);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0022D215 (mov al, 1) to prevent subpacket 0x0C abort!\n");
    }

    // Patch I: 0x000A20C6: 14 bytes safe WorldMgr+0xCC null check
    // Guards against crash 0xC0000005 at client.dll + 0x000A20E6 when [pWorldMgr + 0xCC] is null
    g_retWmNormal = clientBase + 0x000A20D4;
    g_retWmSkip = clientBase + 0x000A2213;

    LPVOID pWmPatch = reinterpret_cast<LPVOID>(clientBase + 0x000A20C6);
    if (VirtualProtect(pWmPatch, 14, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patch[14];
        patch[0] = 0xE9;
        *reinterpret_cast<DWORD*>(&patch[1]) = static_cast<DWORD>(reinterpret_cast<uintptr_t>(&Hook_WorldMgrCCCheck) - (reinterpret_cast<uintptr_t>(pWmPatch) + 5));
        memset(&patch[5], 0x90, 9);
        memcpy(pWmPatch, patch, 14);
        VirtualProtect(pWmPatch, 14, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pWmPatch, 14);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x000A20C6 for safe WorldMgr+0xCC check!\n");
    }

    // Patch J: Safe CViewInterlock tactic button handlers
    LPVOID pWithdraw = reinterpret_cast<LPVOID>(clientBase + 0x0009A170);
    if (MH_CreateHook(pWithdraw, reinterpret_cast<LPVOID>(&Safe_Interlock_Button_Withdraw), reinterpret_cast<LPVOID*>(&Original_Interlock_Button_Withdraw)) == MH_OK) {
        MH_EnableHook(pWithdraw);
        Log("[mxohax] SUCCESS: client.dll Interlock_Button_Withdraw hooked at 0x%p!\n", pWithdraw);
    }

    LPVOID pGrab = reinterpret_cast<LPVOID>(clientBase + 0x0009A1C0);
    if (MH_CreateHook(pGrab, reinterpret_cast<LPVOID>(&Safe_Interlock_Grab_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Grab_Button)) == MH_OK) {
        MH_EnableHook(pGrab);
        Log("[mxohax] SUCCESS: client.dll Interlock_Grab_Button hooked at 0x%p!\n", pGrab);
    }

    LPVOID pPower = reinterpret_cast<LPVOID>(clientBase + 0x0009A210);
    if (MH_CreateHook(pPower, reinterpret_cast<LPVOID>(&Safe_Interlock_Power_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Power_Button)) == MH_OK) {
        MH_EnableHook(pPower);
        Log("[mxohax] SUCCESS: client.dll Interlock_Power_Button hooked at 0x%p!\n", pPower);
    }

    LPVOID pSpeed = reinterpret_cast<LPVOID>(clientBase + 0x0009A260);
    if (MH_CreateHook(pSpeed, reinterpret_cast<LPVOID>(&Safe_Interlock_Speed_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Speed_Button)) == MH_OK) {
        MH_EnableHook(pSpeed);
        Log("[mxohax] SUCCESS: client.dll Interlock_Speed_Button hooked at 0x%p!\n", pSpeed);
    }

    // Patch K: Safe CViewMissionContact & Phone Call hooks
    LPVOID pDtor = reinterpret_cast<LPVOID>(clientBase + 0x000ACAF0);
    if (MH_CreateHook(pDtor, reinterpret_cast<LPVOID>(&Safe_ViewMissionContact_Dtor), reinterpret_cast<LPVOID*>(&OriginalViewMissionContactDtor)) == MH_OK) {
        MH_EnableHook(pDtor);
        Log("[mxohax] SUCCESS: client.dll ViewMissionContact destructor hooked at 0x%p!\n", pDtor);
    }

    LPVOID pCallBtn = reinterpret_cast<LPVOID>(clientBase + 0x000ACB30);
    if (MH_CreateHook(pCallBtn, reinterpret_cast<LPVOID>(&Safe_MissionContact_Button_Call), reinterpret_cast<LPVOID*>(&Original_MissionContact_Button_Call)) == MH_OK) {
        MH_EnableHook(pCallBtn);
        Log("[mxohax] SUCCESS: client.dll MissionContact_Button_Call hooked at 0x%p!\n", pCallBtn);
    }

    LPVOID pSendCall = reinterpret_cast<LPVOID>(clientBase + 0x0018C3A0);
    if (MH_CreateHook(pSendCall, reinterpret_cast<LPVOID>(&Safe_SendCallPacket), reinterpret_cast<LPVOID*>(&Original_SendCallContactPacket)) == MH_OK) {
        MH_EnableHook(pSendCall);
        Log("[mxohax] SUCCESS: client.dll SendCallContactPacket hooked at 0x%p!\n", pSendCall);
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
        if (_stricmp(name, "d3d9.dll") == 0) {
            LPVOID pTarget = nullptr;
            if (!OriginalDirect3DCreate9 && MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9", (LPVOID)&DetourDirect3DCreate9, (LPVOID*)&OriginalDirect3DCreate9, &pTarget) == MH_OK) {
                MH_EnableHook(pTarget);
                Log("[mxohax] DetourLoadLibraryA: hooked d3d9.dll Direct3DCreate9 at 0x%p\n", pTarget);
            }
            if (!OriginalDirect3DCreate9Ex && MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9Ex", (LPVOID)&DetourDirect3DCreate9Ex, (LPVOID*)&OriginalDirect3DCreate9Ex, &pTarget) == MH_OK) {
                MH_EnableHook(pTarget);
                Log("[mxohax] DetourLoadLibraryA: hooked d3d9.dll Direct3DCreate9Ex at 0x%p\n", pTarget);
            }
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
    memset(&g_SyntheticCharRecord, 0, sizeof(g_SyntheticCharRecord));
    g_SyntheticCharRecord.charIdLow = 360;
    g_SyntheticCharRecord.charIdHigh = 0;
    g_SyntheticCharRecord.worldId = 1;

    memset(&g_SyntheticConnParams, 0, sizeof(g_SyntheticConnParams));
    g_SyntheticConnParams.worldId = 1;
    strcpy_s(g_SyntheticConnParams.serverIp, sizeof(g_SyntheticConnParams.serverIp), g_TargetServerIp);
    g_SyntheticConnParams.serverPort = 10000;

    memset(&g_SyntheticCharObj, 0, sizeof(g_SyntheticCharObj));
    g_SyntheticCharObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticCharObj.pRecord = &g_SyntheticCharRecord;
    g_SyntheticCharObj.pCharName = g_SyntheticCharName;

    memset(&g_SyntheticConnObj, 0, sizeof(g_SyntheticConnObj));
    g_SyntheticConnObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticConnObj.pConnParams = &g_SyntheticConnParams;

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

    // 1b. Hook Direct3D 9 creation
    HMODULE hD3D9 = LoadLibraryA("d3d9.dll");
    if (hD3D9) {
        if (!OriginalDirect3DCreate9 && MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9", (LPVOID)&DetourDirect3DCreate9, (LPVOID*)&OriginalDirect3DCreate9, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
            Log("[mxohax] SUCCESS: d3d9.dll Direct3DCreate9 hooked at 0x%p\n", pTarget);
        }
        if (!OriginalDirect3DCreate9Ex && MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9Ex", (LPVOID)&DetourDirect3DCreate9Ex, (LPVOID*)&OriginalDirect3DCreate9Ex, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
            Log("[mxohax] SUCCESS: d3d9.dll Direct3DCreate9Ex hooked at 0x%p\n", pTarget);
        }
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

    // 5. Hook matrix.exe Character Manager:
    LPVOID pGetCharCount = reinterpret_cast<LPVOID>(0x00428920);
    if (MH_CreateHook(pGetCharCount, &DetourGetCharacterCount, reinterpret_cast<LPVOID*>(&OriginalGetCharacterCount)) == MH_OK) {
        MH_EnableHook(pGetCharCount);
        Log("[mxohax] SUCCESS: matrix.exe GetCharacterCount hooked at 0x%p!\n", pGetCharCount);
    }

    LPVOID pGetCharByIndex = reinterpret_cast<LPVOID>(0x00428E00);
    if (MH_CreateHook(pGetCharByIndex, &DetourGetCharacterByIndex, reinterpret_cast<LPVOID*>(&OriginalGetCharacterByIndex)) == MH_OK) {
        MH_EnableHook(pGetCharByIndex);
        Log("[mxohax] SUCCESS: matrix.exe GetCharacterByIndex hooked at 0x%p!\n", pGetCharByIndex);
    }

    LPVOID pSelectChar = reinterpret_cast<LPVOID>(0x00429D80);
    if (MH_CreateHook(pSelectChar, &DetourSelectCharacterVtbl, reinterpret_cast<LPVOID*>(&OriginalSelectCharacterVtbl)) == MH_OK) {
        MH_EnableHook(pSelectChar);
        Log("[mxohax] SUCCESS: matrix.exe SelectCharacter (0x00429D80) hooked at 0x%p!\n", pSelectChar);
    }

    LPVOID pClearChars = reinterpret_cast<LPVOID>(0x00428CC0);
    if (MH_CreateHook(pClearChars, &DetourClearCharacters, reinterpret_cast<LPVOID*>(&OriginalClearCharacters)) == MH_OK) {
        MH_EnableHook(pClearChars);
        Log("[mxohax] SUCCESS: matrix.exe ClearCharacters hooked at 0x%p!\n", pClearChars);
    }

    LPVOID pClearObjs = reinterpret_cast<LPVOID>(0x00428D10);
    if (MH_CreateHook(pClearObjs, &DetourClearCharObjects, reinterpret_cast<LPVOID*>(&OriginalClearCharObjects)) == MH_OK) {
        MH_EnableHook(pClearObjs);
        Log("[mxohax] SUCCESS: matrix.exe ClearCharObjects hooked at 0x%p!\n", pClearObjs);
    }

    // 6. Patch matrix.exe 0x0043D1D6 (NOP 88 06 -> 90 90) to prevent overwriting character count with 0
    LPVOID pCountPatch = reinterpret_cast<LPVOID>(0x0043D1D6);
    DWORD oldProt = 0;
    if (VirtualProtect(pCountPatch, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE nop2[2] = { 0x90, 0x90 };
        memcpy(pCountPatch, nop2, 2);
        VirtualProtect(pCountPatch, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pCountPatch, 2);
        Log("[mxohax] SUCCESS: Patched matrix.exe + 0x0003D1D6 (NOP mov byte ptr [esi], al) to preserve character count!\n");
    }

    // 7. Patch matrix.exe 0x0040977A (mov dl, 1; nop * 4) to force autoJackIn=1 passed to InitClientDLL
    LPVOID pAutoJackInArgPatch = reinterpret_cast<LPVOID>(0x0040977A);
    if (VirtualProtect(pAutoJackInArgPatch, 6, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patchAuto[6] = { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 };
        memcpy(pAutoJackInArgPatch, patchAuto, 6);
        VirtualProtect(pAutoJackInArgPatch, 6, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pAutoJackInArgPatch, 6);
        Log("[mxohax] SUCCESS: Patched matrix.exe + 0x0000977A (mov dl, 1) to force autoJackIn parameter!\n");
    }

    // 8. Patch matrix.exe 0x00407161 to bypass Margin State 8 check and jump to State 10
    LPVOID pState8Patch = reinterpret_cast<LPVOID>(0x00407161);
    if (VirtualProtect(pState8Patch, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE nop5[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
        memcpy(pState8Patch, nop5, 5);
        VirtualProtect(pState8Patch, 5, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pState8Patch, 5);
        Log("[mxohax] SUCCESS: Patched matrix.exe + 0x00007161 (NOP * 5) to bypass Margin State 8 check and jump to State 10!\n");
    }

    // 9. Set matrix.exe autoJackIn global at 0x004AFDA9 to 1
    LPVOID pGlobalAutoJackIn = reinterpret_cast<LPVOID>(0x004AFDA9);
    if (VirtualProtect(pGlobalAutoJackIn, 1, PAGE_EXECUTE_READWRITE, &oldProt)) {
        *reinterpret_cast<BYTE*>(pGlobalAutoJackIn) = 1;
        VirtualProtect(pGlobalAutoJackIn, 1, oldProt, &oldProt);
        Log("[mxohax] SUCCESS: Set matrix.exe autoJackIn global at 0x004AFDA9 = 1!\n");
    }
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

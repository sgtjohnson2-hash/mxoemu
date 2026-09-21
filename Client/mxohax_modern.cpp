#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
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

static inline bool IsSafeReadPointer(const void* ptr, size_t size) {
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
            HMODULE hSelf = GetModuleHandleA("mxohax_modern.dll");
            uintptr_t selfBase = (uintptr_t)hSelf;

            bool isGameCrash = false;
            if (clientBase && (uintptr_t)addr >= clientBase && (uintptr_t)addr < clientBase + 0x1000000) isGameCrash = true;
            if (matrixBase && (uintptr_t)addr >= matrixBase && (uintptr_t)addr < matrixBase + 0x1000000) isGameCrash = true;
            if (selfBase && (uintptr_t)addr >= selfBase && (uintptr_t)addr < selfBase + 0x100000) isGameCrash = true;

            if (!isGameCrash) {
                return EXCEPTION_CONTINUE_SEARCH;
            }

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
            if (clientBase && (uintptr_t)addr == clientBase + 0x0033FB13 && ctx) {
                Log("[mxohax] Recovering from null deref at client.dll + 0x0033FB13 (eax=0x%08X): returning NULL\n", ctx->Eax);
                ctx->Eax = 0;
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0033FB19);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr == clientBase + 0x001152D7 && ctx) {
                Log("[mxohax] Recovering from crash at client.dll + 0x001152D7 (Viewport vtable). Jumping to safe return (0x%p)\n", (void*)(clientBase + 0x001155A9));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x001155A9);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0012F020 && (uintptr_t)addr <= clientBase + 0x0012F350 && ctx) {
                Log("[mxohax] Recovering from crash in CamCtor at client.dll + 0x%08X: jumping to safe ret\n", (uintptr_t)addr - clientBase);
                if (ctx->Ebp && !IsBadReadPtr((void*)(ctx->Ebp + 4), 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    ctx->Eip = retAddr;
                    ctx->Esp = ctx->Ebp + 8;
                    ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
            if (clientBase && ((uintptr_t)addr == clientBase + 0x00001C1C || (uintptr_t)addr == clientBase + 0x00001C10) && ctx) {
                Log("[mxohax] Recovering from invalid pointer write at client.dll + 0x%08X (eax=0x%08X): skipping 2 bytes\n",
                    (uintptr_t)addr - clientBase, ctx->Eax);
            }
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0009B9B0 && (uintptr_t)addr <= clientBase + 0x0009BB10 && ctx) {
                Log("[mxohax] Recovering from null player deref in HUD UpdateControls at client.dll + 0x%08X: jumping to epilogue (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x0009BB0A));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0009BB0A);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00382360 && (uintptr_t)addr <= clientBase + 0x00382480 && ctx) {
                Log("[mxohax] Recovering from crash in CLTWidget_SetPosition at client.dll + 0x%08X: jumping to safe epilogue (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x00382471));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x00382471);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00015D60 && (uintptr_t)addr <= clientBase + 0x00015DA0 && ctx) {
                Log("[mxohax] Recovering from crash in SetControlPos at client.dll + 0x%08X: jumping to safe ret (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x00015DA1));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x00015DA1);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (ctx && ctx->Esp && !IsBadReadPtr((void*)ctx->Esp, 4)) {
                DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Esp);
                if (clientBase && retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                    Log("[mxohax] Recovering from wild indirect call at 0x%p (caller client.dll + 0x%08X): popping return address\n",
                        addr, retAddr - clientBase);
                    ctx->Eip = retAddr;
                    ctx->Esp += 4;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
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
// Synthetic Operative Data for S1acker (charId = 359)
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
    uint32_t charIdLow;       // 0x03..0x06: 359 (read at matrix.exe 0x0043C653)
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

static char               g_ActiveCharName[64] = "S1acker";
static uint32_t           g_ActiveCharId = 359;
static bool               g_CommandLineParsed = false;

static void ParseClientCommandLine() {
    if (g_CommandLineParsed) return;
    g_CommandLineParsed = true;
    const char* cmd = GetCommandLineA();
    if (!cmd) return;
    const char* pChar = strstr(cmd, "-char");
    if (pChar) {
        pChar += 5;
        while (*pChar == ' ' || *pChar == '\t' || *pChar == '\"') pChar++;
        char buf[64] = {0};
        int i = 0;
        while (*pChar && *pChar != ' ' && *pChar != '\t' && *pChar != '\"' && i < 63) {
            buf[i++] = *pChar++;
        }
        buf[i] = '\0';
        if (strlen(buf) > 0) {
            strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), buf, _TRUNCATE);
            if (_stricmp(buf, "TestCharacter") == 0) g_ActiveCharId = 35;
            else if (_stricmp(buf, "TestChar2") == 0) g_ActiveCharId = 354;
            else if (_stricmp(buf, "NeoOperative") == 0) g_ActiveCharId = 357;
            else if (_stricmp(buf, "RedDawn141") == 0) g_ActiveCharId = 358;
            else if (_stricmp(buf, "s1acker") == 0) {
                g_ActiveCharId = 359;
                strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), "S1acker", _TRUNCATE);
            }
            else if (_stricmp(buf, "slacker") == 0) {
                g_ActiveCharId = 360;
                strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), "Slacker", _TRUNCATE);
            }
            Log("[mxohax] Parsed command-line target character: '%s' (charId=%u)\n", g_ActiveCharName, g_ActiveCharId);
        }
    }
    const char* pUser = strstr(cmd, "-user");
    if (pUser) {
        pUser += 5;
        while (*pUser == ' ' || *pUser == '\t' || *pUser == '\"') pUser++;
        char ubuf[64] = {0};
        int i = 0;
        while (*pUser && *pUser != ' ' && *pUser != '\t' && *pUser != '\"' && i < 63) {
            ubuf[i++] = *pUser++;
        }
        ubuf[i] = '\0';
        if (_stricmp(ubuf, "s1acker") == 0) {
            g_ActiveCharId = 359;
            strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), "S1acker", _TRUNCATE);
            Log("[mxohax] Parsed command-line target user: '%s' -> mapped to operative '%s' (charId=%u)\n", ubuf, g_ActiveCharName, g_ActiveCharId);
        } else if (_stricmp(ubuf, "slacker") == 0) {
            g_ActiveCharId = 360;
            strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), "Slacker", _TRUNCATE);
            Log("[mxohax] Parsed command-line target user: '%s' -> mapped to operative '%s' (charId=%u)\n", ubuf, g_ActiveCharName, g_ActiveCharId);
        }
    }
    if (strstr(cmd, "s1acker") || strstr(cmd, "S1acker")) {
        g_ActiveCharId = 359;
        strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), "S1acker", _TRUNCATE);
        Log("[mxohax] Parsed command-line target: '%s' (charId=%u)\n", g_ActiveCharName, g_ActiveCharId);
    }
}

static MxoCharacterData   g_SyntheticChar = {
    359, 0, 0, 0, 0,
    "S1acker", "",
    106, 103, 101, 1, 359
};
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
    ParseClientCommandLine();

    // 1. Initialize vtable
    for (int i = 0; i < 64; ++i) {
        g_SyntheticVtbl[i] = (void*)&DummyDestructor;
    }

    // 2. Character record (for matrix.exe 0x0043C650 MS_LoadCharacterRequest packet building)
    memset(&g_SyntheticCharRecord, 0, sizeof(g_SyntheticCharRecord));
    g_SyntheticCharRecord.charIdLow = g_ActiveCharId;
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
    g_SyntheticCharObj.pCharName = g_ActiveCharName;

    g_SyntheticConnObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticConnObj.pConnParams = &g_SyntheticConnParams;

    // Update synthetic character structure
    g_SyntheticChar.charId = g_ActiveCharId;
    g_SyntheticChar.handle = g_ActiveCharId;
    strncpy_s(g_SyntheticChar.firstName, sizeof(g_SyntheticChar.firstName), g_ActiveCharName, _TRUNCATE);
    g_SyntheticChar.bodyType = 106;
    g_SyntheticChar.headType = 103;
    g_SyntheticChar.hairType = 101;

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
    Log("[mxohax] [matrix.exe] GetCharacterCount called -> returning 1 (operative %s mounted, charId=%u)\n", g_ActiveCharName, g_ActiveCharId);
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
        Log("[mxohax] [matrix.exe] Returning synthetic operative %s (charId=%u) at 0x%p\n", g_ActiveCharName, g_ActiveCharId, &g_SyntheticChar);
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

typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);

static bool g_hasTarget = false;

static const DWORD s_hudControlIds[] = {
    0x1B, // Player Status & Vitals (Top-Left)
    0x24, // Quickbar / Hotbar (Bottom-Center)
    0x02, // Main Chat Window (Bottom-Left)
    0x23, // Chat Tabs (Bottom-Left)
    0x03, // Chat Toolbar / Input (Bottom-Left)
    0x27, // Compass (Bottom-Center)
    0x0E, // Combat Tactics Bar (4 Combat Posture Buttons docked above Compass)
    0x4D  // Latency Meter (Bottom-Right)
};

static inline bool IsCoreHudControl(DWORD ctrlId) {
    for (DWORD hid : s_hudControlIds) {
        if (ctrlId == hid) return true;
    }
    return false;
}

static bool s_inWorldSticky = false;
static volatile bool s_inStreamingState4 = false;
static volatile int  s_state4Ticks = 0;
static bool s_playerEnteredWorld = false;
static bool s_screen5DActive = false;
static bool s_autoJackInDone = false;
static int  s_screen5DFrames = 0;
static bool s_charSheetVisible = false;
static bool s_optionsVisible = false;

static void __fastcall DetourHideControl(void* pUI, void* /*edx*/, DWORD ctrlId) {
    if (ctrlId != 0x1A) {
        Log("[mxohax] HideControl: 0x%02X\n", ctrlId);
    }
    if (ctrlId == 0x22 || ctrlId == 0x3D) {
        // Allow hiding target status when no target selected
        if (!g_hasTarget && OriginalHideControl) {
            OriginalHideControl(pUI, ctrlId);
            return;
        }
    }
    if (s_inWorldSticky && IsCoreHudControl(ctrlId)) {
        Log("[mxohax] HideControl: 0x%02X suppressed while in-world to preserve retail HUD!\n", ctrlId);
        return;
    }
    if (OriginalHideControl) OriginalHideControl(pUI, ctrlId);
}

static void __fastcall DetourSetControlVisible(void* pUI, void* /*edx*/, DWORD ctrlId, BOOL bVisible) {
    if (ctrlId == 0x5D || ctrlId == 0x30 || ctrlId == 0x04 || ctrlId == 0x57 || ctrlId == 0x42 || ctrlId == 0x47 || ctrlId == 0x22 || ctrlId == 0x3D || ctrlId == 0x0E) {
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
        // Combat Tactics stance buttons are slotted cleanly onto Quickbar slots 1-5
        if (OriginalSetControlVisible) OriginalSetControlVisible(pUI, ctrlId, bVisible);
        return;
    }
    if (s_inWorldSticky && !bVisible && IsCoreHudControl(ctrlId)) {
        Log("[mxohax] SetControlVisible: 0x%02X (bVisible=0) suppressed while in-world to preserve retail HUD!\n", ctrlId);
        return;
    }
    if (OriginalSetControlVisible) OriginalSetControlVisible(pUI, ctrlId, bVisible);
}

// Hook for internal CUI control repositioning function (client.dll + 0x00015D60)
typedef void (__thiscall *SetControlPos_t)(void* pControl, const int* pt);
static SetControlPos_t OriginalSetControlPos = nullptr;
static bool g_bAllowControlMove = false;

// Hook for internal CLTWidget virtual repositioning function (client.dll + 0x00382360)
typedef int (__thiscall *CLTWidget_SetPosition_t)(void* pThis, int x, int y, void* pRel, int bMoveChildren);
static CLTWidget_SetPosition_t OriginalWidgetSetPosition = nullptr;

// Detours for client.dll CUI Drag & Resize engine functions
typedef void (__cdecl *CUI_BeginDrag_t)(const int* pt, void* pControl);
static CUI_BeginDrag_t OriginalBeginDrag = nullptr;
static void __cdecl DetourBeginDrag(const int* pt, void* pControl) {
    // Drop all drag initiation requests. UI controls are permanently immobilized.
    return;
}

typedef void (__cdecl *CUI_OnDragMove_t)(const int* pt, void* pControl);
static CUI_OnDragMove_t OriginalOnDragMove = nullptr;
static void __cdecl DetourOnDragMove(const int* pt, void* pControl) {
    // Drop all drag movement requests. UI controls are permanently immobilized.
    return;
}

typedef void (__cdecl *CUI_BeginResize_t)(const int* pt, void* pMode, void* pControl);
static CUI_BeginResize_t OriginalBeginResize = nullptr;
static void __cdecl DetourBeginResize(const int* pt, void* pMode, void* pControl) {
    // Drop all resize initiation requests. UI controls are permanently immobilized.
    return;
}

typedef void (__cdecl *CUI_OnResizeMove_t)(const int* pt, void* pControl);
static CUI_OnResizeMove_t OriginalOnResizeMove = nullptr;
static void __cdecl DetourOnResizeMove(const int* pt, void* pControl) {
    // Drop all resize movement requests. UI controls are permanently immobilized.
    return;
}

static int __fastcall DetourWidgetSetPosition(void* pThis, void* /*edx*/, int x, int y, void* pRel, int bMoveChildren) {
    if (!pThis) return 0;
    if (s_inWorldSticky && !g_bAllowControlMove) {
        return 0; // Drop unauthorized dragging of HUD widgets while in-world
    }
    if (OriginalWidgetSetPosition) return OriginalWidgetSetPosition(pThis, x, y, pRel, bMoveChildren);
    return 0;
}

static inline void NeutralizeDragGlobals(uintptr_t clientBase) {
    if (!clientBase) return;
    DWORD* pActiveDrag = reinterpret_cast<DWORD*>(clientBase + 0x00849394);
    if (pActiveDrag && *pActiveDrag != 0xFFFFFFFF) {
        *pActiveDrag = 0xFFFFFFFF; // Active dragged control ID
    }
}

static void __fastcall DetourSetControlPos(void* pControl, void* /*edx*/, const int* pt) {
    if (!pControl || !pt) return;
    if (s_inWorldSticky && !g_bAllowControlMove) {
        return; // Drop unauthorized control movements while in-world
    }
    if (OriginalSetControlPos) OriginalSetControlPos(pControl, pt);
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
    if (res <= 0) {
        res = 1; // Enforce return code 1 (> 0) so matrix.exe passes 'jg 0x40982f' and calls RunClientDLL!
        Log("[mxohax] DetourInitClientDLL: Enforced return code 1 to invoke RunClientDLL!\n");
    }
    return res;
}

static bool TryAutoJackIn(DWORD clientBase) {
    if (!clientBase) return false;

    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (!pWorldMgr) return false;

    // 1. Phase 1: Show authentic 2D Loading Screen (0x57) and dismiss Login (0x30) & Character Selection (0x5D)
    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (pUI) {
        if (OriginalHideControl) {
            OriginalHideControl(pUI, 0x30);
            OriginalHideControl(pUI, 0x5D);
            Log("[mxohax] [AutoJackIn] Dismissed Screen 0x30 and 0x5D\n");
        }
        typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
        typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
        CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
        SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
        __try {
            pCreateControl(pUI, 0x57);
            pSetVisible(pUI, 0x57, 1);
            Log("[mxohax] [AutoJackIn] Displayed authentic Phase 1 2D Loading Screen (0x57)!\n");
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
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
    static char s_charHandleStr[64] = "TestCharacter";
    strncpy_s(s_charHandleStr, sizeof(s_charHandleStr), g_ActiveCharName, _TRUNCATE);
    static MxoLocalCharEntry s_localCharEntry;
    s_localCharEntry.pWorldFirst  = s_worldFileName;
    s_localCharEntry.pWorldLast   = s_worldFileName + strlen(s_worldFileName);
    s_localCharEntry.pWorldEnd    = s_localCharEntry.pWorldLast;
    s_localCharEntry.pHandleFirst = s_charHandleStr;
    s_localCharEntry.pHandleLast  = s_charHandleStr + strlen(s_charHandleStr);
    s_localCharEntry.pHandleEnd   = s_localCharEntry.pHandleLast;
    s_localCharEntry.charId       = g_ActiveCharId;
    s_localCharEntry.worldId      = 1;

    // Ensure fallback world pointer at 0x00896E4C points to real slums METR
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_worldFileName;

    DWORD* ppCharBegin = reinterpret_cast<DWORD*>(clientBase + 0x00899B4C);
    DWORD* ppCharEnd   = reinterpret_cast<DWORD*>(clientBase + 0x00899B50);
    void* pCharToEnter = nullptr;
    if (ppCharBegin && ppCharEnd) {
        if (*ppCharEnd > *ppCharBegin) {
            pCharToEnter = reinterpret_cast<void*>(*ppCharBegin);
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(pCharToEnter) + 0x18) = g_ActiveCharId;
            Log("[mxohax] [AutoJackIn] Found %d existing operative entries in 0x00899B4C! Using entry #0 at 0x%p (charId=%u)\n",
                (*ppCharEnd - *ppCharBegin) / 32, pCharToEnter, g_ActiveCharId);
        } else {
            *ppCharBegin = reinterpret_cast<DWORD>(&s_localCharEntry);
            *ppCharEnd   = reinterpret_cast<DWORD>(&s_localCharEntry) + sizeof(s_localCharEntry);
            pCharToEnter = &s_localCharEntry;
            Log("[mxohax] [AutoJackIn] Mounted operative %s (slums_barrens_full.metr, charId=%u) into vector at 0x00899B4C\n", g_ActiveCharName, g_ActiveCharId);
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

static HWND g_hGameWindow = NULL;
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

    static int s_streamingPresents = 0;
    static bool s_capturedLoadBmp = false;
    static bool s_capturedStreamBmp = false;
    if (s_inStreamingState4 && pDevice) {
        s_streamingPresents++;
        if (s_state4Ticks >= 10 && !s_capturedLoadBmp) {
            s_capturedLoadBmp = true;
            CaptureD3D9Backbuffer(pDevice, "E:\\Games\\The Matrix Online\\loading_screen_render.bmp");
            Log("[mxohax] Phase 1: Captured 2D Loading Screen to loading_screen_render.bmp\n");
        }
        if (s_state4Ticks >= 25 && !s_capturedStreamBmp) {
            s_capturedStreamBmp = true;
            CaptureD3D9Backbuffer(pDevice, "E:\\Games\\The Matrix Online\\matrix_streaming_render.bmp");
            Log("[mxohax] Phase 2: Captured 3D Matrix Code Stream to matrix_streaming_render.bmp\n");
        }
    }

    if (s_inWorldSticky && pDevice) {
        s_inWorldPresents++;
        if (s_inWorldPresents == 50) {
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, "E:\\Games\\The Matrix Online\\inworld_render.bmp");
            Log("[mxohax] Captured initial in-world State 3 frame to inworld_render.bmp\n");
        } else if (s_inWorldPresents == 100) {
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, "E:\\Games\\The Matrix Online\\inworld_idle.bmp");
            Log("[mxohax] Captured confirmed stationary idle stance to inworld_idle.bmp\n");
        }
    }

    static DWORD s_lastCaptureCheck = 0;
    DWORD nowTick = GetTickCount();
    if (pDevice && nowTick - s_lastCaptureCheck > 100) {
        s_lastCaptureCheck = nowTick;
        if (GetFileAttributesA("E:\\Games\\The Matrix Online\\capture_now.txt") != INVALID_FILE_ATTRIBUTES) {
            char targetPath[260] = "E:\\Games\\The Matrix Online\\live_capture.bmp";
            FILE* fTrig = fopen("E:\\Games\\The Matrix Online\\capture_now.txt", "r");
            if (fTrig) {
                char buf[260] = {0};
                if (fgets(buf, sizeof(buf), fTrig)) {
                    char* nl = strpbrk(buf, "\r\n");
                    if (nl) *nl = 0;
                    if (strlen(buf) > 0) {
                        strncpy_s(targetPath, sizeof(targetPath), buf, _TRUNCATE);
                    }
                }
                fclose(fTrig);
            }
            DeleteFileA("E:\\Games\\The Matrix Online\\capture_now.txt");
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, targetPath);
            Log("[mxohax] On-demand capture written to: %s\n", targetPath);
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

    static int s_streamingPresentsEx = 0;
    static bool s_capturedLoadBmpEx = false;
    static bool s_capturedStreamBmpEx = false;
    if (s_inStreamingState4 && pDevice) {
        s_streamingPresentsEx++;
        if (s_state4Ticks >= 10 && !s_capturedLoadBmpEx) {
            s_capturedLoadBmpEx = true;
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, "E:\\Games\\The Matrix Online\\loading_screen_render.bmp");
            Log("[mxohax] Phase 1: Captured 2D Loading Screen to loading_screen_render.bmp (Ex)\n");
        }
        if (s_state4Ticks >= 25 && !s_capturedStreamBmpEx) {
            s_capturedStreamBmpEx = true;
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, "E:\\Games\\The Matrix Online\\matrix_streaming_render.bmp");
            Log("[mxohax] Phase 2: Captured 3D Matrix Code Stream to matrix_streaming_render.bmp (Ex)\n");
        }
    }

    if (s_inWorldSticky && pDevice) {
        s_inWorldPresents++;
        if (s_inWorldPresents == 50) {
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, "E:\\Games\\The Matrix Online\\inworld_render.bmp");
            Log("[mxohax] Captured initial in-world State 3 frame to inworld_render.bmp (Ex)\n");
        } else if (s_inWorldPresents == 100) {
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, "E:\\Games\\The Matrix Online\\inworld_idle.bmp");
            Log("[mxohax] Captured confirmed stationary idle stance to inworld_idle.bmp (Ex)\n");
        }
    }

    static DWORD s_lastCaptureCheckEx = 0;
    DWORD nowTickEx = GetTickCount();
    if (pDevice && nowTickEx - s_lastCaptureCheckEx > 100) {
        s_lastCaptureCheckEx = nowTickEx;
        if (GetFileAttributesA("E:\\Games\\The Matrix Online\\capture_now.txt") != INVALID_FILE_ATTRIBUTES) {
            char targetPath[260] = "E:\\Games\\The Matrix Online\\live_capture.bmp";
            FILE* fTrig = fopen("E:\\Games\\The Matrix Online\\capture_now.txt", "r");
            if (fTrig) {
                char buf[260] = {0};
                if (fgets(buf, sizeof(buf), fTrig)) {
                    char* nl = strpbrk(buf, "\r\n");
                    if (nl) *nl = 0;
                    if (strlen(buf) > 0) {
                        strncpy_s(targetPath, sizeof(targetPath), buf, _TRUNCATE);
                    }
                }
                fclose(fTrig);
            }
            DeleteFileA("E:\\Games\\The Matrix Online\\capture_now.txt");
            CaptureD3D9Backbuffer((IDirect3DDevice9*)pDevice, targetPath);
            Log("[mxohax] On-demand capture written to: %s (Ex)\n", targetPath);
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

static WNDPROC OriginalWndProc = nullptr;
static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void SubclassGameWindow(HWND hWnd) {
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

static HRESULT STDMETHODCALLTYPE DetourCreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) {
    if (hFocusWindow) SubclassGameWindow(hFocusWindow);
    else if (pPresentationParameters && pPresentationParameters->hDeviceWindow) SubclassGameWindow(pPresentationParameters->hDeviceWindow);
    if (pPresentationParameters) {
        Log("[mxohax] CreateDevice: original %ux%u (windowed=%d) -> enforcing 1920x1080\n",
            pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, pPresentationParameters->Windowed);
        pPresentationParameters->BackBufferWidth = 1920;
        pPresentationParameters->BackBufferHeight = 1080;
    }
    if (hFocusWindow) {
        g_hGameWindow = hFocusWindow;
        SetWindowPos(hFocusWindow, HWND_NOTOPMOST, 0, 0, 1920, 1080, SWP_SHOWWINDOW);
    }
    else if (pPresentationParameters && pPresentationParameters->hDeviceWindow) {
        g_hGameWindow = pPresentationParameters->hDeviceWindow;
        SetWindowPos(pPresentationParameters->hDeviceWindow, HWND_NOTOPMOST, 0, 0, 1920, 1080, SWP_SHOWWINDOW);
    }
    HRESULT hr = OriginalCreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        Log("[mxohax] CreateDevice: pDevice=0x%p, FocusWindow=0x%p\n", *ppReturnedDeviceInterface, hFocusWindow);
        HookDeviceVtable(*ppReturnedDeviceInterface, false);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE DetourCreateDeviceEx(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface) {
    if (hFocusWindow) SubclassGameWindow(hFocusWindow);
    else if (pPresentationParameters && pPresentationParameters->hDeviceWindow) SubclassGameWindow(pPresentationParameters->hDeviceWindow);
    if (pPresentationParameters) {
        Log("[mxohax] CreateDeviceEx: original %ux%u (windowed=%d) -> enforcing 1920x1080\n",
            pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, pPresentationParameters->Windowed);
        pPresentationParameters->BackBufferWidth = 1920;
        pPresentationParameters->BackBufferHeight = 1080;
    }
    if (hFocusWindow) {
        g_hGameWindow = hFocusWindow;
        SetWindowPos(hFocusWindow, HWND_NOTOPMOST, 0, 0, 1920, 1080, SWP_SHOWWINDOW);
    }
    else if (pPresentationParameters && pPresentationParameters->hDeviceWindow) {
        g_hGameWindow = pPresentationParameters->hDeviceWindow;
        SetWindowPos(pPresentationParameters->hDeviceWindow, HWND_NOTOPMOST, 0, 0, 1920, 1080, SWP_SHOWWINDOW);
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
// CActor 3D Scene Transform & Coordinates Synchronizer (Extents & Position Queue)
// ============================================================================
static void SyncActorPosition(uintptr_t clientBase, void* pPlayer, void* pActor, double x, double y, double z) {
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

    // Initial coordinates on CActor for spawn
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

    // Queue initial position sample so CActor receives valid coordinates immediately
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

    // Recalculate spatial extents now that [pActor + 0x528] has been set!
    typedef void (__thiscall *CalcExtents_t)(void* pActor);
    CalcExtents_t pCalcExtents = reinterpret_cast<CalcExtents_t>(clientBase + 0x004E9BF0);
    __try {
        pCalcExtents(pActor);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Enforce visibility flags
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x374) = 0; // 0 = Render local player
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x375) = 1; // 1 = Visible property
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x2DD) = 0; // 0 = Not culled (CActor::IsVisible at 0x104E2C30 checks 0x2DD == 0)
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x684) = 0; // 0 = In world
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x385) = 3; // 3 = Scene node transform valid
    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x290) = 0; // 0 = Normal extents
    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = 0.0f; // 0.0f = No LOD/distance hide
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

        // 1. Populate the client.dll character appearance global tables (0x1089E760)
        *reinterpret_cast<short*>(clientBase + 0x0089E37C) = maleBody;
        *reinterpret_cast<short*>(clientBase + 0x0089E3B4) = maleHead;
        *reinterpret_cast<short*>(clientBase + 0x0089E3EC) = maleHair;
        *reinterpret_cast<short*>(clientBase + 0x0089E424) = 0;   // Hat: 0

        struct OperativeItemDef {
            DWORD slot;
            DWORD articleId;
            BYTE color;
        };
        const OperativeItemDef items[6] = {
            { 2, maleShirt,   41 }, // Slot 2: Shirt
            { 3, maleCoat,    0  }, // Slot 3: Coat (Black Trenchcoat)
            { 4, malePants,   16 }, // Slot 4: Pants (Jeans)
            { 5, maleShoes,   0  }, // Slot 5: Shoes (Boots)
            { 6, maleGloves,  0  }, // Slot 6: Gloves
            { 7, maleGlasses, 15 }  // Slot 7: Glasses (Sunglasses)
        };
        for (int i = 0; i < 6; ++i) {
            uintptr_t entry = clientBase + 0x0089E760 + (i * 0x74);
            *reinterpret_cast<DWORD*>(entry) = items[i].slot;
            *reinterpret_cast<DWORD*>(entry + 0x34) = items[i].articleId;
            *reinterpret_cast<BYTE*>(entry + 0x6C) = items[i].color;
        }

        // 2. Set the operative components directly on pRSI
        pSetBody(pRSI, maleBody);
        pSetHead(pRSI, maleHead);
        pSetHair(pRSI, maleHair);
        pSetHat(pRSI, 0);

        // 3. Equip Operative Attire on pRSI:
        pEquip(pRSI, 0, 0, 0);
        pEquip(pRSI, 2, maleShirt,   41);
        pEquip(pRSI, 3, maleCoat,    0);
        pEquip(pRSI, 4, malePants,   16);
        pEquip(pRSI, 5, maleShoes,   0);
        pEquip(pRSI, 6, maleGloves,  0);
        pEquip(pRSI, 7, maleGlasses, 15);

        // 3b. Directly populate the internal 34-byte RSI appearance table ([pRSI + 0x90] to [pRSI + 0xAE])
        if (pRSI && !IsBadReadPtr(pRSI, 0xC0)) {
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x90) = maleBody;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x92) = 0;           // Hat: 0
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x94) = maleHead;    // Head: 103
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x96) = maleShirt;   // Shirt: 105
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x98) = maleCoat;    // Coat: 120
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x9A) = malePants;   // Pants: 102
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x9C) = maleShoes;   // Shoes: 111
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0x9E) = maleGloves;  // Gloves: 107
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA0) = maleGlasses; // Glasses: 109
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA2) = maleHair;    // Hair: 105
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA4) = 0;
            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA6) = 0;

            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA8) = 41;  // Shirt Color: 41
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xA9) = 16;  // Pants Color: 16
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAA) = 0;   // Coat Color: 0 (Black)
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAB) = 0;   // Shoes Color: 0
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAC) = 15;  // Glasses Color: 15
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAD) = 0;   // Hair Color: 0
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pRSI) + 0xAE) = 0;   // Hat Color: 0
        }

        // 4. Rebuild RSI visual mesh
        pRebuild(pRSI);

        // 5. Apply the 15-byte attire bitstream to CActor (Coat, Jeans, Boots, Sunglasses, Gloves, Hair, Shirt)
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

            // Ensure pActor has visual property at +0x1E4
            void* pActorMesh = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1E4);
            if (!pActorMesh) {
                void* pRSIMesh = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pRSI) + 0x68);
                if (pRSIMesh) {
                    *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1E4) = pRSIMesh;
                    pActorMesh = pRSIMesh;
                }
            }

            Log("[mxohax] pActor=0x%p mesh=0x%p 1DC=0x%p 14C=0x%p. Calling CActor::SetAppearanceFromBitstream...\n",
                pActor, pActorMesh, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x1DC),
                *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActor) + 0x14C));

            typedef void (__thiscall *SetActorAppearance_t)(void* pActor, const void* pBitstream);
            SetActorAppearance_t pSetActorApp = reinterpret_cast<SetActorAppearance_t>(clientBase + 0x004F2F60);
            pSetActorApp(pActor, s_s1ackerBitstream);
            Log("[mxohax] SUCCESS: SetActorAppearance(0x104F2F60) executed with S1acker bitstream!\n");

            // Direct AttachMesh (0x102592E0) invocation
            if (pActorMesh && !IsBadReadPtr(pActorMesh, 8)) {
                void* pEdx = *reinterpret_cast<void**>(pActorMesh);
                void* arg1 = pEdx ? *reinterpret_cast<void**>(pEdx) : nullptr;
                void* pThis = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pActorMesh) + 4);
                if (pThis && !IsBadReadPtr(pThis, 0x30)) {
                    typedef void (__thiscall *AttachMesh_t)(void* pThis, void* arg1, const void* pBitstream);
                    AttachMesh_t pAttachMesh = reinterpret_cast<AttachMesh_t>(clientBase + 0x002592E0);
                    pAttachMesh(pThis, arg1, s_s1ackerBitstream);
                    Log("[mxohax] SUCCESS: Direct AttachMesh(0x102592E0) executed!\n");
                }
            }
        }

        Log("[mxohax] SUCCESS: Operative RSI fully equipped (Male Body %u, Head %u, Hair %u, Coat %u, Shirt %u, Pants %u, Shoes %u, Glasses %u)!\n",
            maleBody, maleHead, maleHair, maleCoat, maleShirt, malePants, maleShoes, maleGlasses);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception in ApplyOperativeAppearance caught safely!\n");
    }
}

// ============================================================================
// Module 6: Dynamic Collision Raycasting, Ground Elevation & Locomotion Physics
// ============================================================================
static const double SPAWN_GROUND_ELEVATION = 603.5; // Calibrated ground elevation flush with Slums concourse pavement tiles (zero sinking or floating)
static double g_playerX = 16710.0;
static double g_playerY = SPAWN_GROUND_ELEVATION;
static double g_playerZ = 3230.0;

struct CollisionRaycastHit {
    bool   bHit;
    double hitY;
    double normalX, normalY, normalZ;
    DWORD  surfaceFlags;
};

// Dynamic collision raycaster: probes Lithtech Jupiter world geometry and physics interfaces
// for downward line-of-sight intersection. If active engine raycast succeeds and yields valid floor,
// returns true with hitY. If unmapped or engine pointer unavailable, returns false for seamless fallback.
static bool CastDynamicWorldRay(uintptr_t clientBase, double posX, double startY, double posZ, double maxDownDist, CollisionRaycastHit& outHit) {
    outHit.bHit = false;
    outHit.hitY = SPAWN_GROUND_ELEVATION;
    outHit.normalX = 0.0;
    outHit.normalY = 1.0;
    outHit.normalZ = 0.0;
    outHit.surfaceFlags = 0;

    if (!clientBase) return false;

    __try {
        // Probe Lithtech CLTClient physics / world intersection interface (object at clientBase + 0x00897FE0)
        void* pPhysics = reinterpret_cast<void*>(clientBase + 0x00897FE0);
        if (pPhysics && !IsBadReadPtr(pPhysics, 0x40)) {
            void** vtbl = *reinterpret_cast<void***>(pPhysics);
            if (vtbl && !IsBadReadPtr(vtbl, 0x40)) {
                // ILTPhysics::IntersectSegment interface (vtable slot 7 / 0x1C)
                typedef BOOL (__thiscall *fnIntersectSegment)(void* pThis, const float* pStart, const float* pEnd, void* pInfo);
                fnIntersectSegment pIntersect = reinterpret_cast<fnIntersectSegment>(vtbl[7]);
                if (pIntersect) {
                    float startPt[3] = { (float)posX, (float)startY, (float)posZ };
                    float endPt[3] = { (float)posX, (float)(startY - maxDownDist), (float)posZ };
                    BYTE hitInfo[128] = {0};
                    if (pIntersect(pPhysics, startPt, endPt, hitInfo)) {
                        float* pHitPos = reinterpret_cast<float*>(hitInfo + 0x10);
                        if (pHitPos[1] > 400.0f && pHitPos[1] < 1200.0f) {
                            outHit.bHit = true;
                            outHit.hitY = (double)pHitPos[1];
                            return true;
                        }
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Fallback safely to calibrated spatial surface model
    }
    return false;
}

static double GetCalibratedGroundElevation(double x, double z) {
    // 1. Dynamic continuous collision raycasting against active 3D sector geometry
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (hClient) {
        CollisionRaycastHit hit;
        double probeStartY = (g_playerY > 500.0 && g_playerY < 1200.0) ? (g_playerY + 40.0) : 750.0;
        if (CastDynamicWorldRay(reinterpret_cast<uintptr_t>(hClient), x, probeStartY, z, 450.0, hit)) {
            if (hit.bHit && hit.hitY >= 500.0 && hit.hitY <= 900.0) {
                return hit.hitY;
            }
        }
    }

    // 2. High-precision continuous polygon collision mesh model for MegaCity Slums Sector
    // Zone A: Concourse curbs and perimeter edges (elevated curb lip of +1.0 units)
    bool isConcourseCurb = (x >= 16400.0 && x <= 17250.0 && z >= 2400.0 && z <= 3720.0) &&
                           ((x <= 16425.0 || x >= 17225.0) || (z <= 2420.0 || z >= 3700.0));
    if (isConcourseCurb) {
        return SPAWN_GROUND_ELEVATION + 1.0; // 604.5 curb lip
    }

    // Zone B: Elevated overpass concourse plaza walkway
    if (x >= 16400.0 && x <= 17250.0 && z >= 2400.0 && z <= 3720.0) {
        return SPAWN_GROUND_ELEVATION; // 603.5 concourse tile surface
    }

    // Zone C1: South stairs & ramp transition to street level (Z: 2100 to 2400)
    // Modeled with discrete stair treads (run = 15.0 units, rise = 1.575 units)
    if (z >= 2100.0 && z < 2400.0 && x >= 16400.0 && x <= 17250.0) {
        double distFromTop = 2400.0 - z;
        int stepIdx = (int)(distFromTop / 15.0);
        if (stepIdx > 19) stepIdx = 19;
        return SPAWN_GROUND_ELEVATION - (stepIdx * 1.575); // Steps down smoothly from 603.5 to 572.0
    }

    // Zone C2: North church stairs transition to street level (Z: 3720 to 3950)
    // Modeled with discrete stair treads (run = 15.3 units, rise = 2.1 units)
    if (z > 3720.0 && z <= 3950.0 && x >= 16400.0 && x <= 17250.0) {
        double distFromTop = z - 3720.0;
        int stepIdx = (int)(distFromTop / 15.3);
        if (stepIdx > 14) stepIdx = 14;
        return SPAWN_GROUND_ELEVATION - (stepIdx * 2.1); // Steps down smoothly from 603.5 to 572.0
    }

    // Zone D1: Church entrance steps and courtyard (Z > 3950)
    if (z > 3950.0 && x >= 16650.0 && x <= 16850.0) {
        return 576.0; // Church porch elevation (+4.0 above street sidewalk)
    }

    // Zone D2: Street sidewalk (572.0) vs Street asphalt roadway (570.5)
    bool isSidewalk = (x < 16550.0 || x > 17100.0 || z < 2150.0 || (z > 3650.0 && z < 4000.0));
    if (isSidewalk) {
        return 572.0; // Street sidewalk level
    }

    return 570.5; // Street roadway asphalt level
}

static float  g_playerYaw = 0.0f; // Facing North (+Z)
static double g_velY = 0.0;
static bool   g_isJumping = false;
static double g_jumpVelX = 0.0;
static double g_jumpVelZ = 0.0;
static bool   g_hasDoubleJumped = false;

// Epoch I: Wire-Fu Acrobatics & Skyscraper Facade Wall-Running
static bool   g_isWallRunning = false;
static float  g_wallRunDuration = 0.0f;
static float  g_wallRunCameraTilt = 0.0f;

// Epoch I: Bullet-Time Focus Evasion
static bool   g_focusModeActive = false;
static float  g_timeDilation = 1.0f;
static float  g_bulletDodgeTimer = 0.0f;

// Epoch III: Matrix Anomaly Code Rain Degradation
static bool   g_codeRainDegradationActive = false;
static float  g_matrixCodeRainIntensity = 0.0f;

static float  g_camPitch = 12.0f * 0.0174532925f; // ~12 degrees downward
static float  g_camYaw = 0.0f;                   // Facing North (+Z, behind player)
static float  g_camDist = 280.0f;                // 280 units behind player framing full body & feet
static int    g_lastMouseX = -1;
static int    g_lastMouseY = -1;
static bool   g_bRightMouseDown = false;
static bool   g_bLeftMouseDown = false;
static bool   g_bHumanInputActive = false;
static bool   g_bPlayerIsMoving = false;
static bool   s_keysDown[256] = {0};

// Targeting system (Nearby Operative NPC P: charId=393 at 16802.3, 637.5, 3237.01)
static DWORD  g_targetCharId = 393;
static char   g_targetName[64] = "P";
static double g_targetX = 16802.3;
static double g_targetY = SPAWN_GROUND_ELEVATION;
static double g_targetZ = 3237.01;

// Combat tactics stance
enum StanceType { STANCE_FREE = 0, STANCE_POWER = 1, STANCE_GRAB = 2, STANCE_SPEED = 3, STANCE_WITHDRAW = 4 };
static StanceType g_currentStance = STANCE_FREE;
static bool       g_bMouseDownOnUI = false;
static int        g_pressedHudButton = 0;
static int        g_quickbarPage = 1;

static void __fastcall Safe_Interlock_Speed_Button(void* pThis, void* /*edx*/);
static void __fastcall Safe_Interlock_Power_Button(void* pThis, void* /*edx*/);
static void __fastcall Safe_Interlock_Grab_Button(void* pThis, void* /*edx*/);
static void __fastcall Safe_Interlock_Block_Button(void* pThis, void* /*edx*/);

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


static void DispatchInputEventToClient(uintptr_t clientBase, DWORD eventCode, int x, int y) {
    if (!clientBase) return;
    void* pWidgetMgr = *reinterpret_cast<void**>(clientBase + 0x00897F98);
    if (!pWidgetMgr || IsBadReadPtr(pWidgetMgr, 0x100)) return;

    // 1. Update cursor position in CLTWidgetManager
    typedef void (__thiscall *SetCursorPos_t)(void* pThis, int x, int y);
    SetCursorPos_t pSetCursorPos = reinterpret_cast<SetCursorPos_t>(clientBase + 0x00377030);
    __try {
        pSetCursorPos(pWidgetMgr, x, y);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // 2. Prepare event
    LithtechInputEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.eventCode = eventCode;
    evt.mouseX = x;
    evt.mouseY = y;

    // 3. Dispatch to CUI::DispatchInput
    typedef void (__cdecl *DispatchInput_t)(LithtechInputEvent* pEvent);
    DispatchInput_t pDispatch = reinterpret_cast<DispatchInput_t>(clientBase + 0x0001DED0);
    __try {
        pDispatch(&evt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void* GetHoveredUIWidget(uintptr_t clientBase) {
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

static inline bool IsValidWidget(uintptr_t clientBase, void* pWidget) {
    if (!pWidget || !clientBase || IsBadReadPtr(pWidget, 0x80)) return false;
    uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(pWidget);
    if (vtbl < clientBase || vtbl >= clientBase + 0x1000000) return false;
    if (IsBadReadPtr(reinterpret_cast<void*>(vtbl), 0x100)) return false;
    uintptr_t fn0 = *reinterpret_cast<uintptr_t*>(vtbl);
    if (fn0 < clientBase || fn0 >= clientBase + 0x1000000) return false;
    return true;
}

static void SetWidgetVisualState(void* pWidget, int state) {
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

static inline bool IsPointInAnyHudRect(int normX, int normY) {
    // 1. Top-Left HUD: Player Status & Quickbar (0..700, 0..120)
    if (normX >= 0 && normX <= 700 && normY >= 0 && normY <= 120) return true;

    // 2. Top-Right HUD: Target / Vitals / Buffs panel only when active (1650..1920, 0..200)
    if (g_hasTarget && normX >= 1650 && normX <= 1920 && normY >= 0 && normY <= 200) return true;

    // 3. Bottom HUD bar: Compass dial (center 960, 1013, radius 75)
    int cdx = normX - 960;
    int cdy = normY - 1013;
    if (cdx * cdx + cdy * cdy <= 75 * 75) return true;

    // 4. Bottom-Left: Main Chat Window (0..500, 750..1080)
    if (normX >= 0 && normX <= 500 && normY >= 750 && normY <= 1080) return true;

    // 5. Bottom-Right: Latency Meter & Options (1750..1920, 1000..1080)
    if (normX >= 1750 && normX <= 1920 && normY >= 1000 && normY <= 1080) return true;

    return false;
}

static void* GetControlRootWidget(void* pCtrl, DWORD ctrlId) {
    if (!pCtrl || IsBadReadPtr(pCtrl, 0xF0)) return nullptr;
    void* pWidget = nullptr;

    void* pRoot04 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 4);
    if (pRoot04 && !IsBadReadPtr(pRoot04, 0x30)) {
        pWidget = pRoot04;
    }

    switch (ctrlId) {
        case 0x1B: // Player Status & Vitals (portrait, health, IS, exp)
            if (!pWidget) pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0xE0);
            break;
        case 0x24: { // Quickbar Layout Root Widget
            uintptr_t pLayout = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pCtrl) + 0x50);
            if (pLayout && !IsBadReadPtr(reinterpret_cast<void*>(pLayout), 0x30)) {
                void* pLayoutRoot = *reinterpret_cast<void**>(pLayout + 0x24);
                if (pLayoutRoot && !IsBadReadPtr(pLayoutRoot, 0x30)) {
                    pWidget = pLayoutRoot;
                }
            }
            if (!pWidget) {
                pWidget = pRoot04;
            }
            break;
        }
        case 0x22: { // Target Status Frame
            void* pLayout = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x50);
            if (!pLayout) pLayout = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x7C);
            if (pLayout && !IsBadReadPtr(pLayout, 0x30)) {
                pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pLayout) + 0x24);
            }
            break;
        }
        case 0x3D: // Active Buffs HUD
            pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x60);
            break;
        case 0x02: // Main Chat Window
            pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x54);
            break;
        case 0x23: // Chat Tabs
            pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x60);
            break;
        case 0x03: // Chat Toolbar / Input
            pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x70);
            break;
        case 0x27: { // Compass Dial (Compass_Base at +0x68, Compass_BG at +0x6C)
            void* p68 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x68);
            if (p68 && !IsBadReadPtr(p68, 0x30)) {
                pWidget = p68;
            } else {
                pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x6C);
            }
            break;
        }
        case 0x0E: // Combat Tactics Bar
            pWidget = pRoot04;
            break;
        case 0x4D: // Latency Meter
            pWidget = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0xC0);
            break;
        default:
            break;
    }

    // Fallback 1: check if [pCtrl + 0x54] is already valid
    if (!pWidget || IsBadReadPtr(pWidget, sizeof(void*))) {
        void* p54 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x54);
        if (p54 && !IsBadReadPtr(p54, sizeof(void*))) {
            pWidget = p54;
        }
    }

    // Fallback 2: check layout root widget at [[pCtrl + 0x50] + 0x24]
    if (!pWidget || IsBadReadPtr(pWidget, sizeof(void*))) {
        void* p50 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x50);
        if (p50 && !IsBadReadPtr(p50, 0x30)) {
            void* pRoot = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(p50) + 0x24);
            if (pRoot && !IsBadReadPtr(pRoot, sizeof(void*))) {
                pWidget = pRoot;
            }
        }
    }

    // Fallback 3: check layout root widget at [[pCtrl + 0x7C] + 0x24]
    if (!pWidget || IsBadReadPtr(pWidget, sizeof(void*))) {
        void* p7C = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl) + 0x7C);
        if (p7C && !IsBadReadPtr(p7C, 0x30)) {
            void* pRoot = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(p7C) + 0x24);
            if (pRoot && !IsBadReadPtr(pRoot, sizeof(void*))) {
                pWidget = pRoot;
            }
        }
    }

    return pWidget;
}

static HudButtonId HitTestHudButton(int x, int y, int screenW = 1920, int screenH = 1080) {
    if (screenW <= 0) screenW = 1920;
    if (screenH <= 0) screenH = 1080;

    // Normalize coordinates to canonical 1920x1080 HUD canvas space
    int canX = (screenW != 1920) ? (int)((double)x * 1920.0 / (double)screenW) : x;
    int canY = (screenH != 1080) ? (int)((double)y * 1080.0 / (double)screenH) : y;
    if (canX < 0) canX = 0;
    if (canX > 1920) canX = 1920;
    if (canY < 0) canY = 0;
    if (canY > 1080) canY = 1080;

    // 1. Quickbar / Hotbar (0x24: centered at bottom, x = 746..1174, y = 894..944)
    const int qbX = (1920 - 428) / 2; // 746
    const int qbY = 1080 - 186;        // 894
    if (canY >= qbY && canY <= qbY + 50 && canX >= qbX && canX <= qbX + 428) {
        // Page switcher button (leftmost tab: x = qbX..qbX+35)
        if (canX >= qbX && canX < qbX + 36) {
            return HUD_BTN_QB_PAGE;
        }
        // Slots 1 to 10: slot i starts at qbX + 36 + (i * 37), width 37
        for (int i = 0; i < 10; ++i) {
            int slotX = qbX + 36 + (i * 37);
            if (canX >= slotX && canX < slotX + 37) {
                return (HudButtonId)(HUD_BTN_QB_1 + i);
            }
        }
        // Force Combat button (fighting figures icon): qbX + 406..qbX + 428
        if (canX >= qbX + 406) {
            return HUD_BTN_TACTIC_FREE;
        }
    }

    // 2. Bottom-Center: Compass Dial & Satellite Wings (0x27)
    // 2a. 4 Combat Posture Buttons docked directly above Compass ring (Focus, Power, Attack, Defense)
    const int compassTopY = 1080 - 134; // 946
    const int postureY = compassTopY - 33; // 913
    if (canY >= postureY - 2 && canY <= postureY + 34) {
        const int totalW = (4 * 32) + (3 * 2); // 134
        const int startX = 960 - (totalW / 2); // 893
        if (canX >= startX && canX <= startX + totalW) {
            int idx = (canX - startX) / (32 + 2);
            if (idx == 0) return HUD_BTN_TACTIC_FREE;     // Focus / Free (Blue)
            if (idx == 1) return HUD_BTN_TACTIC_POWER;    // Power (Red)
            if (idx == 2) return HUD_BTN_TACTIC_GRAB;     // Attack / Grab (Green)
            if (idx >= 3) return HUD_BTN_TACTIC_WITHDRAW; // Defense / Withdraw (Yellow)
        }
    }

    // Left Wing (Character Status button):
    if (canY >= 950 && canY <= 1070 && canX >= 835 && canX <= 915) {
        return HUD_BTN_CHAR_STATUS;
    }
    // Right Wing (Cell Phone / Operator Call button):
    if (canY >= 950 && canY <= 1070 && canX >= 1005 && canX <= 1085) {
        return HUD_BTN_CELL_PHONE;
    }
    // Central Compass Dial:
    int compCenterX = 1920 / 2; // 960
    int compCenterY = 1010;
    int compDx = canX - compCenterX;
    int compDy = canY - compCenterY;
    if (compDx * compDx + compDy * compDy <= 72 * 72) {
        return HUD_BTN_COMPASS;
    }

    // 3. Bottom-Right: Latency Meter & Options
    const int meterX = 1920 - 64 - 10; // 1846
    const int meterY = 1080 - 35;      // 1045
    if (canY >= meterY - 15) {
        if (canX >= meterX - 35 && canX <= meterX + 64) return HUD_BTN_LATENCY;
        if (canX > meterX + 64)                         return HUD_BTN_OPTIONS;
    }

    // 4. Top-Right: Target Status Frame & Buffs (0x22, 0x3D) only when target is active
    if (g_hasTarget && canX >= 1650 && canX <= 1920 && canY >= 5 && canY <= 160) {
        return HUD_BTN_TARGET_VITALS;
    }

    // 5. Top-Left: Player Status & Vitals Frame (0x1B)
    if (canX >= 5 && canX <= 270 && canY >= 5 && canY <= 110) {
        return HUD_BTN_TARGET_VITALS;
    }

    return HUD_BTN_NONE;
}

static void PositionControlAndWidget(uintptr_t clientBase, void* pUI, DWORD ctrlId, int left, int top, int width, int height) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    if (ctrlId == 0x24 || ctrlId == 0x0E || ctrlId == 0x27) return; // Managed by dedicated repositioners!

    void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (ctrlId * 4));
    if (!ppCtrl || !*ppCtrl || IsBadReadPtr(*ppCtrl, 0x60)) return;
    void* pCtrl = *ppCtrl;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    void* pWidget = GetControlRootWidget(pCtrl, ctrlId);
    if (pWidget && !IsBadReadPtr(pWidget, 0x80)) {
        __try {
            g_bAllowControlMove = true;
            pSetPosition(pWidget, left, top, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x6C) = left;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x70) = top;
            if (width > 0) *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x74) = width;
            if (height > 0) *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x78) = height;
            // Safe visibility flags: preserve existing texture flags and ensure visible bit is set
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pWidget) + 0x28) |= 0x00000001;
            g_bAllowControlMove = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bAllowControlMove = false;
        }
    }
}

static void EnforceControlRect(uintptr_t clientBase, void* pUI, DWORD ctrlId, int left, int top, int width, int height) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (ctrlId * 4));
    if (!ppCtrl || !*ppCtrl || IsBadReadPtr(*ppCtrl, 0x60)) return;
    void* pCtrl = *ppCtrl;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);

    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);
    SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);

    __try {
        pSetVisible(pUI, ctrlId, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    void* pWidget = GetControlRootWidget(pCtrl, ctrlId);
    if (pWidget && !IsBadReadPtr(pWidget, 0x80)) {
        __try {
            g_bAllowControlMove = true;
            pSetPosition(pWidget, left, top, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x6C) = left;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x70) = top;
            if (width > 0) *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x74) = width;
            if (height > 0) *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pWidget) + 0x78) = height;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pWidget) + 0x28) |= 0x00000001;
            g_bAllowControlMove = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bAllowControlMove = false;
        }
    }
}

static void RepositionQuickbar(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x24 * 4));
    if (!ppCtrl || !*ppCtrl || IsBadReadPtr(*ppCtrl, 0x240)) return;
    void* pCtrl24 = *ppCtrl;

    const int qbW = 428;
    const int qbH = 50;
    const int qbX = (screenW - qbW) / 2; // 746 at 1920p
    const int qbY = screenH - 221;       // 859 at 1080p (sits cleanly above posture buttons at 913)

    typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
    SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
    __try {
        pSetVisible(pUI, 0x24, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    __try {
        g_bAllowControlMove = true;

        // 1. Move root layout widget container [pCtrl24 + 0x50] -> +0x24 with bMoveChildren = 1
        // This moves the entire quickbar (pipe, slots, buttons) as an integral unit without fracturing!
        uintptr_t pLayoutMgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x50);
        void* pLayoutRoot = nullptr;
        if (pLayoutMgr && !IsBadReadPtr(reinterpret_cast<void*>(pLayoutMgr), 0x30)) {
            pLayoutRoot = *reinterpret_cast<void**>(pLayoutMgr + 0x24);
            if (pLayoutRoot && IsValidWidget(clientBase, pLayoutRoot)) {
                pSetPosition(pLayoutRoot, qbX, qbY, nullptr, 1);
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pLayoutRoot) + 0x6C) = qbX;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pLayoutRoot) + 0x70) = qbY;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pLayoutRoot) + 0x74) = qbW;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pLayoutRoot) + 0x78) = qbH;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pLayoutRoot) + 0x28) |= 0x11;
            }
        }

        // Position root widget [pCtrl24 + 4] if present
        void* pRoot24 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 4);
        if (pRoot24 && IsValidWidget(clientBase, pRoot24)) {
            pSetPosition(pRoot24, qbX, qbY, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRoot24) + 0x6C) = qbX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRoot24) + 0x70) = qbY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRoot24) + 0x74) = qbW;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRoot24) + 0x78) = qbH;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pRoot24) + 0x28) |= 0x11;
        }

        // 2. Hide untextured Text_ToolBar_Pane (+0x74) so no stretched brown box renders
        void* pTextPane = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x74);
        if (pTextPane && IsValidWidget(clientBase, pTextPane)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pTextPane) + 0x28) &= ~0x00000001;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pTextPane) + 0x74) = 0;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pTextPane) + 0x78) = 0;
        }

        // 3. Position Image_ToolBar (+0x7C) - authentic textured toolbar background housing
        void* pToolbarImg = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x7C);
        if (pToolbarImg && IsValidWidget(clientBase, pToolbarImg)) {
            pSetPosition(pToolbarImg, qbX, qbY, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pToolbarImg) + 0x6C) = qbX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pToolbarImg) + 0x70) = qbY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pToolbarImg) + 0x74) = qbW;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pToolbarImg) + 0x78) = qbH;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pToolbarImg) + 0x28) |= 0x11;
        }

        // 4. Previous/Next page switcher buttons (+0x80 Button_ToolBar_Previous, +0x84 Button_ToolBar_Next)
        void* pBtnPrev = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x80);
        if (pBtnPrev && IsValidWidget(clientBase, pBtnPrev)) {
            pSetPosition(pBtnPrev, qbX + 6, qbY + 12, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnPrev) + 0x6C) = qbX + 6;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnPrev) + 0x70) = qbY + 12;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnPrev) + 0x74) = 14;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnPrev) + 0x78) = 26;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnPrev) + 0x28) |= 0x11;
        }
        void* pBtnNext = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x84);
        if (pBtnNext && IsValidWidget(clientBase, pBtnNext)) {
            pSetPosition(pBtnNext, qbX + 20, qbY + 12, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnNext) + 0x6C) = qbX + 20;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnNext) + 0x70) = qbY + 12;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnNext) + 0x74) = 14;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnNext) + 0x78) = 26;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnNext) + 0x28) |= 0x11;
        }

        // 5. Position all slot buttons 1..10 cleanly into Quickbar slot coordinates (+0xCC + i * 0x1C)
        // Ensure recessed grid borders (Image_IconStatic at slotBase + 0x0C) and ability icons render authentically
        for (int i = 0; i < 10; ++i) {
            uintptr_t slotBase = reinterpret_cast<uintptr_t>(pCtrl24) + 0xCC + (i * 0x1C);
            int slotX = qbX + 36 + (i * 37) + 2;
            int slotY = qbY + 8;

            // Slot button (+0x00: Button_Item)
            void* pBtn = *reinterpret_cast<void**>(slotBase + 0);
            if (pBtn && IsValidWidget(clientBase, pBtn)) {
                pSetPosition(pBtn, slotX, slotY, nullptr, 1);
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x6C) = slotX;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x70) = slotY;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x74) = 34;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x78) = 34;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtn) + 0x28) &= ~0x10;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtn) + 0x28) |= 0x01;
            }

            // Slot static/empty frame icon (+0x0C: Image_IconStatic) - recessed bevel
            void* pIconStatic = *reinterpret_cast<void**>(slotBase + 12);
            if (pIconStatic && IsValidWidget(clientBase, pIconStatic)) {
                pSetPosition(pIconStatic, slotX + 1, slotY + 1, nullptr, 0);
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIconStatic) + 0x6C) = slotX + 1;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIconStatic) + 0x70) = slotY + 1;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIconStatic) + 0x74) = 32;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIconStatic) + 0x78) = 32;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pIconStatic) + 0x28) |= 0x13;
                SetWidgetVisualState(pIconStatic, 3);
            }

            // Slot active ability icon (+0x04: Image_Icon)
            void* pIcon = *reinterpret_cast<void**>(slotBase + 4);
            if (pIcon && IsValidWidget(clientBase, pIcon)) {
                pSetPosition(pIcon, slotX + 1, slotY + 1, nullptr, 0);
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIcon) + 0x6C) = slotX + 1;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIcon) + 0x70) = slotY + 1;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIcon) + 0x74) = 32;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pIcon) + 0x78) = 32;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pIcon) + 0x28) |= 0x11;
            }

            // Slot number (+0x10: Image_Number)
            void* pNum = *reinterpret_cast<void**>(slotBase + 16);
            if (pNum && IsValidWidget(clientBase, pNum)) {
                pSetPosition(pNum, slotX + 2, slotY + 2, nullptr, 0);
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pNum) + 0x6C) = slotX + 2;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pNum) + 0x70) = slotY + 2;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pNum) + 0x74) = 9;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pNum) + 0x78) = 8;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pNum) + 0x28) |= 0x11;
            }
        }

        // 6. Position right-side combat picker buttons (+0x1EC ToolBar_StylePickerButton, +0x1F0 ToolBar_SpecialFirePickerButton)
        void* pPicker1 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x1EC);
        if (pPicker1 && IsValidWidget(clientBase, pPicker1)) {
            pSetPosition(pPicker1, qbX + 407, qbY + 8, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker1) + 0x6C) = qbX + 407;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker1) + 0x70) = qbY + 8;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker1) + 0x74) = 18;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker1) + 0x78) = 18;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pPicker1) + 0x28) |= 0x11;
        }
        void* pPicker2 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x1F0);
        if (pPicker2 && IsValidWidget(clientBase, pPicker2)) {
            pSetPosition(pPicker2, qbX + 407, qbY + 26, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker2) + 0x6C) = qbX + 407;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker2) + 0x70) = qbY + 26;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker2) + 0x74) = 18;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pPicker2) + 0x78) = 18;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pPicker2) + 0x28) |= 0x11;
        }

        // 7. Position ToolBar_PipeImage (+0x1FC)
        void* pPipe = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x1FC);
        if (pPipe && IsValidWidget(clientBase, pPipe)) {
            pSetPosition(pPipe, qbX + 35, qbY + 6, nullptr, 0);
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pPipe) + 0x28) |= 0x11;
        }

        // 8. Suppress floating template widgets Toolbar_EmptySlot_REF (+0x1F4, +0x1F8) so they never float in mid-air
        void* pEmpty1 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x1F4);
        if (pEmpty1 && IsValidWidget(clientBase, pEmpty1)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pEmpty1) + 0x28) &= ~0x00000001;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pEmpty1) + 0x74) = 0;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pEmpty1) + 0x78) = 0;
        }
        void* pEmpty2 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl24) + 0x1F8);
        if (pEmpty2 && IsValidWidget(clientBase, pEmpty2)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pEmpty2) + 0x28) &= ~0x00000001;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pEmpty2) + 0x74) = 0;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pEmpty2) + 0x78) = 0;
        }

        g_bAllowControlMove = false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bAllowControlMove = false;
    }
}

static void RepositionCompass(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x27 * 4));
    if (!ppCtrl || !*ppCtrl || IsBadReadPtr(*ppCtrl, 0x100)) return;
    void* pCtrl27 = *ppCtrl;

    typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
    SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
    __try {
        pSetVisible(pUI, 0x27, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Compass base widget is at [pCtrl27 + 0x68] (Compass_Base, w=146, h=132)
    void** ppBase = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x68);
    if (!ppBase || !*ppBase || IsBadReadPtr(*ppBase, 0x80)) return;
    void* pBase = *ppBase;

    // Authentic target position for 146x132 Compass_Base:
    // Centered horizontally: (screenW / 2) - 73 = 960 - 73 = 887
    // Docked flush to bottom: screenH - 134 = 1080 - 134 = 946
    const int targetBaseX = (screenW / 2) - 73;
    const int targetBaseY = screenH - 134;

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    int curBaseX = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBase) + 0x6C);
    int curBaseY = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBase) + 0x70);

    // Left wing: Cyan Operative (+0x94 or +0x98, w=42, h=30)
    // Symmetrical to Right Wing (center 960 - 80 - 42 = 838, or targetBaseX - 49)
    void* pBtnLeftBig = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x94);
    void* pBtnLeftSmall = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x98);
    void* pBtnLeft = (pBtnLeftBig && IsValidWidget(clientBase, pBtnLeftBig)) ? pBtnLeftBig : pBtnLeftSmall;
    int targetLeftX = (screenW / 2) - 80 - 42; // 838 at 1920p
    int targetLeftY = targetBaseY + 99;         // 1045 at 1080p

    // Right wing: Cell Phone / Mission (+0x8C or +0x90, w=42, h=30)
    void* pBtnRightBig = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x8C);
    void* pBtnRightSmall = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x90);
    void* pBtnRight = (pBtnRightBig && IsValidWidget(clientBase, pBtnRightBig)) ? pBtnRightBig : pBtnRightSmall;
    int targetRightX = (screenW / 2) + 80;     // 1040 at 1920p
    int targetRightY = targetBaseY + 99;        // 1045 at 1080p

    __try {
        g_bAllowControlMove = true;

        // 1. Central Compass Dial (+0x68): Move with bMoveChildren = 1 when displaced
        if (curBaseX != targetBaseX || curBaseY != targetBaseY) {
            pSetPosition(pBase, targetBaseX, targetBaseY, nullptr, 1);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBase) + 0x6C) = targetBaseX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBase) + 0x70) = targetBaseY;
        }
        *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBase) + 0x28) |= 0x11;

        // 2. Dial Background (+0x6C)
        void* pBG = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x6C);
        if (pBG && IsValidWidget(clientBase, pBG)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBG) + 0x28) |= 0x11;
        }

        // 3. Left wing: Cyan Operative button (+0x94 / +0x98) [targetLeftX = 838, targetLeftY = 1045]
        if (pBtnLeft && IsValidWidget(clientBase, pBtnLeft)) {
            pSetPosition(pBtnLeft, targetLeftX, targetLeftY, nullptr, 1);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeft) + 0x6C) = targetLeftX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeft) + 0x70) = targetLeftY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeft) + 0x74) = 42;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeft) + 0x78) = 30;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnLeft) + 0x28) |= 0x11;
        }
        if (pBtnLeftSmall && pBtnLeftSmall != pBtnLeft && IsValidWidget(clientBase, pBtnLeftSmall)) {
            pSetPosition(pBtnLeftSmall, targetLeftX, targetLeftY, nullptr, 1);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeftSmall) + 0x6C) = targetLeftX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeftSmall) + 0x70) = targetLeftY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeftSmall) + 0x74) = 42;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnLeftSmall) + 0x78) = 30;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnLeftSmall) + 0x28) |= 0x11;
        }

        // 4. Right wing: Cell Phone button (+0x8C / +0x90) [targetRightX = 1040, targetRightY = 1045]
        if (pBtnRight && IsValidWidget(clientBase, pBtnRight)) {
            pSetPosition(pBtnRight, targetRightX, targetRightY, nullptr, 1);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRight) + 0x6C) = targetRightX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRight) + 0x70) = targetRightY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRight) + 0x74) = 42;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRight) + 0x78) = 30;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnRight) + 0x28) |= 0x11;
        }
        if (pBtnRightSmall && pBtnRightSmall != pBtnRight && IsValidWidget(clientBase, pBtnRightSmall)) {
            pSetPosition(pBtnRightSmall, targetRightX, targetRightY, nullptr, 1);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRightSmall) + 0x6C) = targetRightX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRightSmall) + 0x70) = targetRightY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRightSmall) + 0x74) = 42;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnRightSmall) + 0x78) = 30;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnRightSmall) + 0x28) |= 0x11;
        }

        // 5. Compass toggle buttons: Compass_ButtonHide (+0x84) and Compass_ButtonShow (+0x88)
        // These are 84x13 untextured buttons that render as a solid white box if visible! Permanently hide them.
        void* pBtnHide = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x84);
        if (pBtnHide && IsValidWidget(clientBase, pBtnHide)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnHide) + 0x28) &= ~0x00000001;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnHide) + 0x74) = 0;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnHide) + 0x78) = 0;
        }
        void* pBtnShow = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + 0x88);
        if (pBtnShow && IsValidWidget(clientBase, pBtnShow)) {
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtnShow) + 0x28) &= ~0x00000001;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnShow) + 0x74) = 0;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtnShow) + 0x78) = 0;
        }

        // 6. Suppress temporary/meter widgets when inactive (do not suppress wings 0x90/0x98 if in use)
        const DWORD inactiveOffsets[] = { 0x78, 0x80, 0xA0, 0xA4, 0xAC, 0xB0, 0xB4 };
        for (DWORD off : inactiveOffsets) {
            void* pW = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pCtrl27) + off);
            if (pW && IsValidWidget(clientBase, pW)) {
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pW) + 0x28) &= ~0x00000001;
            }
        }

        g_bAllowControlMove = false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bAllowControlMove = false;
    }
}

static void RepositionCombatTactics(uintptr_t clientBase, void* pUI, int screenW, int screenH) {
    if (!pUI || !clientBase || IsBadReadPtr(pUI, 0x200)) return;
    void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (0x0E * 4));
    if (!ppCtrl || !*ppCtrl || IsBadReadPtr(*ppCtrl, 0x100)) return;
    void* pInterlock = *ppCtrl;

    typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
    SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
    __try {
        pSetVisible(pUI, 0x0E, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    typedef int (__thiscall *SetPosition_t)(void* pWidget, int x, int y, void* pRel, int bMoveChildren);
    SetPosition_t pSetPosition = reinterpret_cast<SetPosition_t>(clientBase + 0x00382360);

    // Compass base widget is at [pCtrl27 + 0x68], w=146, h=132
    // Docked at: targetBaseX = (screenW / 2) - 73, targetBaseY = screenH - 134
    // Top center of compass ring: X = screenW / 2, Y = screenH - 134
    // The 4 Combat Posture buttons (Focus/Speed, Power, Attack/Grab, Defense/Block)
    // dock directly on the top metal rim of the compass ring!
    const int compassTopY = screenH - 134; // 946 at 1080p
    const int btnW = 32;
    const int btnH = 32;
    const int btnGap = 2;
    const int totalW = (4 * btnW) + (3 * btnGap); // 134 px
    const int startX = (screenW / 2) - (totalW / 2); // 960 - 67 = 893 at 1920p
    const int btnY = compassTopY - btnH - 1; // 946 - 33 = 913 (flush right above the compass ring)

    __try {
        g_bAllowControlMove = true;

        // Neutralize root widget background so no large duel window or grey box renders over the center screen
        void* pRootWidget = GetControlRootWidget(pInterlock, 0x0E);
        if (pRootWidget && !IsBadReadPtr(pRootWidget, 0x80)) {
            pSetPosition(pRootWidget, startX, btnY, nullptr, 0);
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x6C) = startX;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x70) = btnY;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x74) = totalW;
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x78) = btnH;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x28) &= ~0x10;
            *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pRootWidget) + 0x28) |= 0x01;
        }

        // Retrieve the Combat Posture stance buttons:
        // 1. Focus / Stance Free (+0x6C, Speed_Button)
        void* pBtnFocus = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x6C);
        // 2. Power (+0x70, Power_Button - Red fist icon)
        void* pBtnPower = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x70);
        // 3. Attack / Grab (+0x74, Grab_Button - Green claw icon)
        void* pBtnAttack = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x74);
        // 4. Defense / Block (+0x80, Block_Button - Yellow shield/hand icon)
        void* pBtnDefense = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x80);

        void* btns[4] = { pBtnFocus, pBtnPower, pBtnAttack, pBtnDefense };
        StanceType stances[4] = { STANCE_FREE, STANCE_POWER, STANCE_GRAB, STANCE_WITHDRAW };

        for (int i = 0; i < 4; ++i) {
            void* pBtn = btns[i];
            if (pBtn && IsValidWidget(clientBase, pBtn)) {
                int posX = startX + i * (btnW + btnGap);
                pSetPosition(pBtn, posX, btnY, nullptr, 0);
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x6C) = posX;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x70) = btnY;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x74) = btnW;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pBtn) + 0x78) = btnH;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pBtn) + 0x28) |= 0x11;
                SetWidgetVisualState(pBtn, (g_currentStance == stances[i]) ? 4 : 0);
            }
        }

        // Hide all duel-specific widgets (timer, vs cards, bash fx, background boxes, style buttons)
        // so that floating duel widgets at the top center are 100% eliminated!
        for (uintptr_t off = 0x54; off <= 0x350; off += 4) {
            if (off == 0x6C || off == 0x70 || off == 0x74 || off == 0x80) continue; // Keep the 4 stance buttons!
            void* pW = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + off);
            if (pW && pW != pBtnFocus && pW != pBtnPower && pW != pBtnAttack && pW != pBtnDefense && pW != pRootWidget && IsValidWidget(clientBase, pW)) {
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pW) + 0x28) &= ~0x00000001;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pW) + 0x74) = 0;
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pW) + 0x78) = 0;
            }
        }

        g_bAllowControlMove = false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bAllowControlMove = false;
    }
}

static void LockAllHudFrames(uintptr_t clientBase, void* pUI) {
    if (!pUI || !clientBase) return;
    HWND hWnd = g_hGameWindow;
    if (!hWnd || !IsWindow(hWnd)) {
        hWnd = FindWindowA("MatrixWindowClass", NULL);
        if (!hWnd) hWnd = FindWindowA(NULL, "The Matrix Online");
    }

    int screenW = 1920;
    int screenH = 1080;
    if (hWnd && IsWindow(hWnd)) {
        RECT rc;
        if (GetClientRect(hWnd, &rc) && rc.right > rc.left && rc.bottom > rc.top) {
            screenW = rc.right - rc.left;
            screenH = rc.bottom - rc.top;
        }
    }

    // Enforce client.dll internal screen dimensions to actual window size
    *reinterpret_cast<int*>(clientBase + 0x00896CCC) = screenW;
    *reinterpret_cast<int*>(clientBase + 0x00896D04) = screenH;

    if (hWnd && IsWindow(hWnd)) {
        static DWORD s_lastWndFileWrite = 0;
        DWORD nowTick = GetTickCount();
        if (nowTick - s_lastWndFileWrite > 500) {
            s_lastWndFileWrite = nowTick;
            FILE* fWnd = fopen("E:\\Games\\The Matrix Online\\active_game_wnd.txt", "w");
            if (fWnd) {
                fprintf(fWnd, "%u %d %d\n", (DWORD)(uintptr_t)hWnd, screenW, screenH);
                fclose(fWnd);
            }
        }
    }

    // Ensure authentic retail HUD elements are instantiated & visible
    typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
    typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
    CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
    SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);

    for (DWORD cid : s_hudControlIds) {
        void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (cid * 4));
        if (!ppCtrl || IsBadReadPtr(ppCtrl, sizeof(void*)) || !*ppCtrl) {
            __try {
                pCreateControl(pUI, cid);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        __try {
            pSetVisible(pUI, cid, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // 1. Top-Left: Player Status & Vitals Frame (0x1B: portrait, health, IS, exp)
    PositionControlAndWidget(clientBase, pUI, 0x1B, 10, 10, 260, 95);

    // 2. Quickbar / Hotbar (0x24: centered at bottom)
    RepositionQuickbar(clientBase, pUI, screenW, screenH);

    // 3. Top-Right: Target Status Frame (0x22) and Active Buffs HUD (0x3D) - ONLY when target is active!
    if (g_hasTarget) {
        int targetW = 240;
        int targetH = 90;
        int targetX = screenW - targetW - 10;
        int targetY = 10;
        PositionControlAndWidget(clientBase, pUI, 0x22, targetX, targetY, targetW, targetH);
        pSetVisible(pUI, 0x22, 1);

        int buffW = 320;
        int buffH = 50;
        int buffX = screenW - buffW - 10;
        int buffY = targetY + targetH + 5;
        PositionControlAndWidget(clientBase, pUI, 0x3D, buffX, buffY, buffW, buffH);
        pSetVisible(pUI, 0x3D, 1);
    } else {
        if (OriginalHideControl) {
            OriginalHideControl(pUI, 0x22);
            OriginalHideControl(pUI, 0x3D);
        }
        pSetVisible(pUI, 0x22, 0);
        pSetVisible(pUI, 0x3D, 0);
    }

    // 5. Bottom-Left: Main Chat Window (0x02)
    int chatW = (screenW > 600) ? 480 : (screenW - 40);
    int chatH = (screenH > 500) ? 220 : 150;
    int chatX = 10;
    int chatY = screenH - 45 - chatH;
    PositionControlAndWidget(clientBase, pUI, 0x02, chatX, chatY, chatW, chatH);

    // 6. Bottom-Left: Chat Tabs (0x23) directly above chat window
    int tabW = (chatW > 200) ? 200 : chatW;
    int tabH = 28;
    int tabX = 10;
    int tabY = chatY - tabH - 2;
    PositionControlAndWidget(clientBase, pUI, 0x23, tabX, tabY, tabW, tabH);

    // 7. Bottom-Left: Chat Toolbar / Input (0x03) below chat window
    int tbW = chatW;
    int tbH = 35;
    int tbX = 10;
    int tbY = screenH - 40;
    PositionControlAndWidget(clientBase, pUI, 0x03, tbX, tbY, tbW, tbH);

    // 8. Bottom-Center: Compass Dial (0x27)
    RepositionCompass(clientBase, pUI, screenW, screenH);

    // 9. Bottom-Center: Combat Tactics Bar (0x0E) docked directly above compass
    RepositionCombatTactics(clientBase, pUI, screenW, screenH);

    // 10. Bottom-Right: Latency Meter (0x4D)
    int meterW = 64;
    int meterH = 27;
    int meterX = screenW - meterW - 10;
    int meterY = screenH - 35;
    PositionControlAndWidget(clientBase, pUI, 0x4D, meterX, meterY, meterW, meterH);

    // Modals (only if active)
    if (s_charSheetVisible) {
        PositionControlAndWidget(clientBase, pUI, 0x42, (screenW / 2) - 200, (screenH / 2) - 200, 400, 400);
    }
    if (s_optionsVisible) {
        PositionControlAndWidget(clientBase, pUI, 0x47, (screenW / 2) - 200, (screenH / 2) - 200, 400, 400);
    }
}

static void TriggerNativeIdleTransition(uintptr_t clientBase) {
    // Native idle transition is handled cleanly by stationary position enforcement and idle flag (+0x4EE)
}

static void TriggerPhoneCall(uintptr_t clientBase) {
    Log("[mxohax] CELL PHONE ACTIVATED: Initiating safe contact with Zion Operator...\n");
    void* pThis = malloc(0x100);
    if (pThis) {
        memset(pThis, 0, 0x100);
        Safe_MissionContact_Button_Call(pThis, nullptr);
        free(pThis);
    }
}

static void SetTargetOperative(uintptr_t clientBase, const char* name, DWORD charId, double x, double y, double z) {
    g_hasTarget = true;
    g_targetCharId = charId;
    strncpy_s(g_targetName, sizeof(g_targetName), name, _TRUNCATE);
    g_targetX = x;
    g_targetY = y;
    g_targetZ = z;

    double dx = x - g_playerX;
    double dz = z - g_playerZ;
    double dist = sqrt(dx*dx + dz*dz);
    Log("[mxohax] TARGET SELECTED: '%s' [CharId=%u] at (%.1f, %.1f, %.1f) - Dist: %.1fm\n",
        name, charId, x, y, z, dist);

    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (pUI) {
        typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
        SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
        __try {
            pSetVisible(pUI, 0x22, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

static void SetTacticsStance(uintptr_t clientBase, StanceType newStance) {
    g_currentStance = newStance;
    const char* stanceNames[] = { "Free", "Power", "Grab", "Speed", "Withdraw" };
    const char* sName = (newStance >= 0 && newStance <= 4) ? stanceNames[newStance] : "Unknown";
    Log("[mxohax] TACTICS STANCE CHANGED: Stance is now [%s] (mode=%d)\n", sName, (int)newStance);

    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (pUI) {
        typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
        SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
        __try {
            pSetVisible(pUI, 0x1B, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        // Visually update the corresponding stance button on CViewInterlock (Control 0x0E)
        void* pInterlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + 0x0E * 4);
        if (pInterlock && !IsBadReadPtr(pInterlock, 0x100)) {
            void* pBtnFree  = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x6C);
            void* pBtnPower = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x70);
            void* pBtnGrab  = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x74);
            void* pBtnBlock = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pInterlock) + 0x80);
            if (pBtnFree)  SetWidgetVisualState(pBtnFree,  (newStance == STANCE_FREE)  ? 4 : 0);
            if (pBtnPower) SetWidgetVisualState(pBtnPower, (newStance == STANCE_POWER) ? 4 : 0);
            if (pBtnGrab)  SetWidgetVisualState(pBtnGrab,  (newStance == STANCE_GRAB)  ? 4 : 0);
            if (pBtnBlock) SetWidgetVisualState(pBtnBlock, (newStance == STANCE_WITHDRAW) ? 4 : 0);
        }

        // Visually update the Quickbar (0x24) slot buttons 1-5 for stances
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

static void ExecuteQuickbarAbility(uintptr_t clientBase, int slotIndex) {
    if (slotIndex < 1 || slotIndex > 10) return;

    // Slots 1-5: Tactics Stances (Free, Power, Grab, Speed, Withdraw)
    // Slots 6-9: Combat & Acrobatics (Strike, Hyper-Jump, Subroutine Compile, Logic Bomb)
    // Slot 10: Call Operator (Cell Phone)
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
                SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01);
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
            static int s_targetHealth = 100;
            s_targetHealth -= 14;
            if (s_targetHealth <= 10) s_targetHealth = 100;
            Log("[mxohax] COMBAT ACTION: Strike hits '%s' for 85 damage! Target HP: %d%%\n", g_targetName, s_targetHealth);
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
                SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01);
            }
            Log("[mxohax] COMBAT ACTION: Logic Bomb detonated on '%s' for 210 viral damage!\n", g_targetName);
            break;
        }
        case 10: { // Call Operator (Cell Phone)
            TriggerPhoneCall(clientBase);
            break;
        }
    }

    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (pUI) {
        typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
        SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
        __try {
            if (g_hasTarget) {
                pSetVisible(pUI, 0x22, 1);
                pSetVisible(pUI, 0x3D, 1);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        // Visually update the Quickbar (0x24) button for the executed slot
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

static void ExecuteHudButtonAction(uintptr_t clientBase, HudButtonId btnId, int mx, int my) {
    if (!clientBase) return;
    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);

    typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
    SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);

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
                    __try { pSetVisible(pUI, 0x42, 1); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                } else {
                    if (OriginalHideControl) OriginalHideControl(pUI, 0x42);
                    __try { pSetVisible(pUI, 0x42, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            break;
        }
        case HUD_BTN_COMPASS: {
            Log("[mxohax] UI BUTTON CLICK: Compass dial clicked at (%d, %d) -> Resetting camera yaw to player facing\n", mx, my);
            g_camYaw = g_playerYaw;
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
                    __try { pSetVisible(pUI, 0x47, 1); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                } else {
                    if (OriginalHideControl) OriginalHideControl(pUI, 0x47);
                    __try { pSetVisible(pUI, 0x47, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
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

static bool IsPointOverAnyHud(int mx, int my, int winW, int winH) {
    if (winW <= 0) winW = 1920;
    if (winH <= 0) winH = 1080;

    // Normalize coordinates to 1920x1080 canonical HUD space
    int canX = (winW != 1920) ? (int)((double)mx * 1920.0 / (double)winW) : mx;
    int canY = (winH != 1080) ? (int)((double)my * 1080.0 / (double)winH) : my;

    // 0. Explicit HUD interactive buttons
    if (HitTestHudButton(mx, my, winW, winH) != HUD_BTN_NONE) return true;

    // 1. Top-Left HUD: Player Status (0x1B) (0..270, 0..110)
    if (canX >= 0 && canX <= 270 && canY >= 0 && canY <= 110) return true;

    // 2. Top-Right HUD: Target Status & Buffs (0x22, 0x3D) ONLY when target is active
    if (g_hasTarget && canX >= 1650 && canX <= 1920 && canY >= 0 && canY <= 200) return true;

    // 3. Bottom-Left: Main Chat Window & Tabs & Toolbar (0..500, 750..1080)
    if (canX >= 0 && canX <= 500 && canY >= 750 && canY <= 1080) return true;

    // 4. Bottom-Center: Quickbar / Hotbar (0x24) (740..1180, 890..950)
    if (canX >= 740 && canX <= 1180 && canY >= 890 && canY <= 950) return true;

    // 5. Bottom-Center: Compass Dial (0x27) - circular boundary around (960, 1013), radius 75
    int compDx = canX - 960;
    int compDy = canY - 1013;
    if (compDx * compDx + compDy * compDy <= 75 * 75) return true;

    // 6. Bottom-Right: Latency Meter & Options (1750..1920, 1000..1080)
    if (canX >= 1750 && canX <= 1920 && canY >= 1000 && canY <= 1080) return true;

    // 6. Modals / Dialogs if open (Character Sheet 0x42, Options 0x47)
    if (s_charSheetVisible || s_optionsVisible) {
        if (canX >= 760 && canX <= 1160 && canY >= 340 && canY <= 740) return true;
    }

    return false;
}

static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    HMODULE hClient = GetModuleHandleA("client.dll");
    uintptr_t clientBase = (uintptr_t)hClient;
    NeutralizeDragGlobals(clientBase);

    RECT clientRc;
    GetClientRect(hWnd, &clientRc);
    int winW = clientRc.right - clientRc.left;
    int winH = clientRc.bottom - clientRc.top;
    if (winW <= 0) winW = 1920;
    if (winH <= 0) winH = 1080;

    switch (uMsg) {
        case WM_ACTIVATE:
        case WM_SETFOCUS: {
            NeutralizeDragGlobals(clientBase);
            break;
        }
        case WM_KILLFOCUS: {
            memset(s_keysDown, 0, sizeof(s_keysDown));
            g_bPlayerIsMoving = false;
            g_bMouseDownOnUI = false;
            g_bLeftMouseDown = false;
            g_bRightMouseDown = false;
            if (GetCapture() == hWnd) ReleaseCapture();
            NeutralizeDragGlobals(clientBase);
            break;
        }
        case WM_MOUSEMOVE: {
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);

            // Camera orbiting: both Right-click drag AND Left-click drag in 3D world space
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

            // Dispatch input event to client CUI
            DispatchInputEventToClient(clientBase, 0x65766F4D, mx, my); // 'Move'

            NeutralizeDragGlobals(clientBase);
            return OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        case WM_LBUTTONDOWN: {
            NeutralizeDragGlobals(clientBase);
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
                DispatchInputEventToClient(clientBase, 0x6E444C4D, mx, my); // 'MLDn'
            } else {
                g_bMouseDownOnUI = false;
            }

            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            if (!g_bMouseDownOnUI) {
                SetCapture(hWnd); // Capture mouse for smooth 3D camera drag
            }
            NeutralizeDragGlobals(clientBase);
            return lRes;
        }
        case WM_LBUTTONUP: {
            NeutralizeDragGlobals(clientBase);
            g_bLeftMouseDown = false;
            if (!g_bLeftMouseDown && !g_bRightMouseDown && GetCapture() == hWnd) {
                ReleaseCapture();
            }
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_bHumanInputActive = true;
            Log("[mxohax] WM_LBUTTONUP: (%d, %d)\n", mx, my);

            DispatchInputEventToClient(clientBase, 0x70554C4D, mx, my); // 'MLUp'

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
                ExecuteHudButtonAction(clientBase, btnToExecute, mx, my);
            } else if (!IsPointOverAnyHud(mx, my, winW, winH)) {
                // Click in 3D world (not over any HUD frame) targets nearby AI NPC (e.g. Heiu <Weapon Vendor>)
                if (my >= 60 && my <= winH - 90 && mx >= 10 && mx <= winW - 10) {
                    SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01);
                }
            }
            g_pressedHudButton = (int)HUD_BTN_NONE;
            g_bMouseDownOnUI = false;

            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            NeutralizeDragGlobals(clientBase);
            return lRes;
        }
        case WM_RBUTTONDOWN: {
            NeutralizeDragGlobals(clientBase);
            SetFocus(hWnd);
            SetActiveWindow(hWnd);
            g_bRightMouseDown = true;
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_lastMouseX = mx;
            g_lastMouseY = my;
            g_bHumanInputActive = true;
            Log("[mxohax] WM_RBUTTONDOWN: (%d, %d)\n", mx, my);
            DispatchInputEventToClient(clientBase, 0x6E44524D, mx, my); // 'MRDn'
            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            SetCapture(hWnd); // Capture mouse for smooth 3D camera orbit
            NeutralizeDragGlobals(clientBase);
            return lRes;
        }
        case WM_RBUTTONUP: {
            NeutralizeDragGlobals(clientBase);
            g_bRightMouseDown = false;
            if (!g_bLeftMouseDown && !g_bRightMouseDown && GetCapture() == hWnd) {
                ReleaseCapture();
            }
            short mx = (short)LOWORD(lParam);
            short my = (short)HIWORD(lParam);
            g_bHumanInputActive = true;
            Log("[mxohax] WM_RBUTTONUP: (%d, %d)\n", mx, my);
            DispatchInputEventToClient(clientBase, 0x7055524D, mx, my); // 'MRUp'
            LRESULT lRes = OriginalWndProc ? CallWindowProcA(OriginalWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
            NeutralizeDragGlobals(clientBase);
            return lRes;
        }
        case WM_CAPTURECHANGED: {
            g_bMouseDownOnUI = false;
            g_bLeftMouseDown = false;
            g_bRightMouseDown = false;
            g_pressedHudButton = (int)HUD_BTN_NONE;
            NeutralizeDragGlobals(clientBase);
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
                SetTargetOperative(clientBase, "Heiu <Weapon Vendor>", 393, 16802.3, SPAWN_GROUND_ELEVATION, 3237.01);
            } else if (wParam >= VK_F1 && wParam <= VK_F5) {
                SetTacticsStance(clientBase, (StanceType)(wParam - VK_F1));
                ExecuteQuickbarAbility(clientBase, (int)(wParam - VK_F1 + 1));
            } else if (wParam >= '1' && wParam <= '9') {
                ExecuteQuickbarAbility(clientBase, (int)(wParam - '0'));
            } else if (wParam == '0') {
                ExecuteQuickbarAbility(clientBase, 10);
            } else if (wParam == VK_ESCAPE) {
                ExecuteHudButtonAction(clientBase, HUD_BTN_OPTIONS, 0, 0);
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
                void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
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

static void UpdatePlayerPositionAndPhysics(uintptr_t clientBase, void* curPlayer, double dt, bool isMoving, double actVelX, double actVelZ) {
    if (!curPlayer || IsBadReadPtr(curPlayer, 0xB0)) return;

    double groundElev = GetCalibratedGroundElevation(g_playerX, g_playerZ);

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
        if (g_playerY > groundElev) {
            double diff = g_playerY - groundElev;
            if (diff <= 35.0) {
                // Instant flush contact on curbs, steps, platform drops, and slopes (no hovering/walking on air!)
                g_playerY = groundElev;
            } else {
                g_playerY -= 1200.0 * dt;
                if (g_playerY <= groundElev) {
                    g_playerY = groundElev;
                }
            }
        } else {
            g_playerY = groundElev;
        }
    }

    // Expanded roaming boundary: allows exploring platform, curb, stairs, south/north ramps, and church courtyard
    if (g_playerX < 16400.0) g_playerX = 16400.0;
    if (g_playerX > 17200.0) g_playerX = 17200.0;
    if (g_playerZ < 2050.0)  g_playerZ = 2050.0;
    if (g_playerZ > 4200.0)  g_playerZ = 4200.0;

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

        // Double precision velocities on CActor
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x510) = isMoving ? actVelX : 0.0;
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x518) = g_isJumping ? g_velY : 0.0;
        *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x520) = isMoving ? actVelZ : 0.0;

        // Locomotion stopped/idle flag (+0x4EE): 1 = stopped/idle, 0 = moving
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = (!isMoving && !g_isJumping) ? 1 : 0;
        *reinterpret_cast<WORD*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = (!isMoving && !g_isJumping) ? 1 : 0;

        float* pActorRot = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x4FC);
        if (pActorRot && !IsBadReadPtr(pActorRot, 16)) {
            memcpy(pActorRot, playerQuat, sizeof(playerQuat));
        }

        // Locomotion state and extents managed authoritatively without interpolation lag

        // Maintain visibility
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x374) = 0; // Local player visible
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x375) = 1;
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x2DD) = 0; // Not culled
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x684) = 0; // In-world
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x385) = 3; // Scene transform valid
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x290) = 0;
        *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = isMoving ? 1.0f : 0.0f;
    }
}

static void UpdateCamera(uintptr_t clientBase, void* pCam) {
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

    // Centered camera framing entire operative and ground contact
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

static int s_tickCount = 0;

static void EnsureInWorldRendering(uintptr_t clientBase, void* pWorldMgr, DWORD pShell, bool promoteToState3 = true) {
    if (!pWorldMgr) return;

    // Step 0: Ensure fallback world pointer points to actual Slums METR world
    static const char s_defaultMetrPath[] = "resource/worlds/final_world/slums_barrens_full.metr";
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_defaultMetrPath;

    // Step 1: Ensure World is loaded via CWorldMgr::LoadWorldFile (0x10121110) BEFORE Player enters world
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

    // Step 2: Ensure PlayerObject is allocated, positioned in Slums, and enters world
    void** ppPlayerGlobal = reinterpret_cast<void**>(clientBase + 0x008A4378);
    void* pPlayer = *ppPlayerGlobal;
    if (!pPlayer) {
        Log("[mxohax] EnsureInWorld: Player is null! Calling native CreateObject(0x0C) to create 3D character...\n");
        BYTE flag = 0x21;

        // 1. Get model definition
        typedef void* (__cdecl *GetModelDef_t)(int a, int b);
        GetModelDef_t pGetModelDef = reinterpret_cast<GetModelDef_t>(clientBase + 0x001D27C0);
        void* pModelDef = pGetModelDef(1, 0);

        // 2. Set default spawn coordinates at clientBase + 0x008BA5E8 to Slums Barrens walkway
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
            Log("[mxohax] EnsureInWorld: PlayerCtor(0x101d17c0) initialized PlayerObject at 0x%p\n", pPlayer);

            // Coordinates for operative s1acker on Slums Barrens walkway: (16710.0f, 637.5f, 3230.0f)
            // Allocate full 64 bytes (16 floats) for 4x4 matrix/coords
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
                Log("[mxohax] EnsureInWorld: Set Player coordinates to Slums walkway (%.1f, %.1f, %.1f)\n", pPos[0], pPos[1], pPos[2]);
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
            ApplyOperativeAppearance(clientBase, pPlayer);

            pPos = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94);
            if (pPos) {
                pPos[0] = 16710.0f;
                pPos[1] = (float)SPAWN_GROUND_ELEVATION;
                pPos[2] = 3230.0f;
                pPos[3] = 1.0f;
                pPos[4] = 0.0f;
                pPos[5] = 1.0f;
                pPos[15] = 1.0f;
                Log("[mxohax] EnsureInWorld: Re-applied float street coordinates to [pPlayer+0x94] (%.1f, %.1f, %.1f)\n", pPos[0], pPos[1], pPos[2]);
            }
        }
    } else {
        s_playerEnteredWorld = true;
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
            Log("[mxohax] EnsureInWorld: Updated existing Player coordinates to Slums walkway (%.1f, %.1f, %.1f)\n", pPos[0], pPos[1], pPos[2]);
        }
        float* pRot = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x98);
        if (!pRot) {
            pRot = reinterpret_cast<float*>(calloc(4, sizeof(float)));
            if (pRot) {
                pRot[0] = 0.0f; pRot[1] = 0.0f; pRot[2] = 0.0f; pRot[3] = 1.0f;
                *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x98) = pRot;
            }
        }
        ApplyOperativeAppearance(clientBase, pPlayer);
    }

    // Patch CActor::HideLocalPlayer entry at clientBase + 0x004ECDB0 with ret 8 (C2 08 00)
    // Permanently prevents CActor::HideLocalPlayer from ever stripping meshes or hiding operative
    LPVOID pHideActorEntry = reinterpret_cast<LPVOID>(clientBase + 0x004ECDB0);
    DWORD oldProtHide = 0;
    if (VirtualProtect(pHideActorEntry, 3, PAGE_EXECUTE_READWRITE, &oldProtHide)) {
        BYTE ret8[3] = { 0xC2, 0x08, 0x00 };
        memcpy(pHideActorEntry, ret8, 3);
        VirtualProtect(pHideActorEntry, 3, oldProtHide, &oldProtHide);
        FlushInstructionCache(GetCurrentProcess(), pHideActorEntry, 3);
        Log("[mxohax] EnsureInWorld: Neutralized CActor::HideLocalPlayer at client.dll + 0x004ECDB0 (ret 8)!\n");
    }
    *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 1; // Enforce Camera_Mode = 1 (Third Person)

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
        if (pState) {
            *pState = 4; // Keep State 4 active for Matrix code rain streaming!
        }
        void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
        if (pUI && OriginalHideControl) {
            OriginalHideControl(pUI, 0x04);
            OriginalHideControl(pUI, 0x57);
            OriginalHideControl(pUI, 0x30);
            OriginalHideControl(pUI, 0x5D);
            Log("[mxohax] EnsureInWorld: Dismissed 2D loading screen 0x57 to reveal falling Matrix digital code rain!\n");
        }
        Log("[mxohax] EnsureInWorld: Scene and Player prepared in State 4 (Streaming with Matrix code rain)!\n");
        return;
    }

    // Step 2b: Invoke native AdvanceToState3 (0x10121B50) ONLY when promoting to State 3!
    static bool s_advanceToState3Done = false;
    if (pPlayer && !s_advanceToState3Done) {
        s_advanceToState3Done = true;
        // Ensure state is 3 (not 4) so AdvanceToState3 does not take the State 4 streaming bypass branch!
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState) *pState = 3;
        // Ensure [pWorldMgr + 0x22] is 0 so AdvanceToState3 does not skip native HUD control creation!
        *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 0;
        Log("[mxohax] EnsureInWorld: Invoking native AdvanceToState3 (0x10121B50) on pWorldMgr=0x%p, pPlayer=0x%p in State 3...\n", pWorldMgr, pPlayer);
        typedef void (__thiscall *AdvanceToState3_t)(void* pMgr, void* pPlayer);
        AdvanceToState3_t pAdv3 = reinterpret_cast<AdvanceToState3_t>(clientBase + 0x00121B50);
        __try {
            pAdv3(pWorldMgr, pPlayer);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[mxohax] EnsureInWorld: Exception in AdvanceToState3 caught safely!\n");
        }
        DWORD* pCurState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        Log("[mxohax] EnsureInWorld: AdvanceToState3 dispatched! State is now %u\n", pCurState ? *pCurState : 0);
    }

    // Step 3: Verified World File and Instance loaded via Step 1
    ppWorldInst = reinterpret_cast<void**>(clientBase + 0x0089DD6C);

    // Step 4: Ensure Camera is instantiated
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
                Log("[mxohax] EnsureInWorld: Instantiated fallback camera at 0x%p into [0x1089edf8]!\n", pCam);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("[mxohax] EnsureInWorld: Fallback CamCtor exception caught safely.\n");
                free(pCam);
            }
        }
    }

    // Configure Camera CVars
    *reinterpret_cast<float*>(clientBase + 0x0089F2D0) = 12.0f;  // Pitch = 12 degrees down
    *reinterpret_cast<float*>(clientBase + 0x0089F304) = 0.0f;   // Yaw = 0 degrees (North)
    *reinterpret_cast<float*>(clientBase + 0x0089F338) = 280.0f; // Chase distance = 280.0 units
    *reinterpret_cast<DWORD*>(clientBase + 0x0089EFAC) = 2;      // Default Camera Mode = 2 (Chase Cam)
    *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 2;      // Enforce Camera_Mode = 2 (Third Person)

    if (ppCamera && *ppCamera) {
        __try {
            void* pCam = *ppCamera;
            // 1. Switch CCameraManager mode to Mode 2 (Chase Camera)
            typedef void (__thiscall *SetCameraMode_t)(void* pCamMgr, int mode);
            SetCameraMode_t pSetMode = reinterpret_cast<SetCameraMode_t>(clientBase + 0x0012D200);
            pSetMode(pCam, 2);

            g_playerX = 16710.0;
            g_playerY = SPAWN_GROUND_ELEVATION;
            g_playerZ = 3230.0;
            g_playerYaw = 0.0f;
            g_camYaw = 3.14159f; // Facing South, directly observing player's front and boots flush on tiles
            g_camPitch = 0.1745f; // 10 degrees downward pitch framing full body & feet
            g_camDist = 420.0f;  // Crisp framing of operative face down to boots resting on tiles

            if (pPlayer) {
                float* pPos = *reinterpret_cast<float**>(reinterpret_cast<DWORD>(pPlayer) + 0x94);
                if (pPos) {
                    pPos[0] = (float)g_playerX;
                    pPos[1] = (float)g_playerY;
                    pPos[2] = (float)g_playerZ;
                    pPos[3] = 1.0f;
                    pPos[4] = 0.0f;
                    pPos[5] = 1.0f;
                    pPos[15] = 1.0f;
                }
            }

            UpdateCamera(clientBase, pCam);
            Log("[mxohax] EnsureInWorld: Chase camera mode 2 activated facing North with target (%.1f, %.1f, %.1f)!\n",
                g_playerX, g_playerY, g_playerZ);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[mxohax] EnsureInWorld: Exception in Chase camera setup caught safely!\n");
        }
    }

    // Step 5: Activate in-world display & viewport via official UI SetControlVisible for all HUD controls
    void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
    if (pUI && promoteToState3 && pPlayer) {
        typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
        typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
        CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
        SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);

        static const DWORD hudControls[] = {
            0x1B, // Player Window (Quickbar, IS/Health meters, Combat tactics)
            0x27, // Compass / Radar HUD
            0x24, // Action Toolbar / Quickbar
            0x02, // Main Chat Window
            0x03, // Chat Toolbar
            0x23, // Tabs Parent
            0x4D  // Latency Meter
        };
        for (DWORD id : hudControls) {
            __try {
                void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (id * 4));
                if (!ppCtrl || !*ppCtrl) {
                    pCreateControl(pUI, id);
                    Log("[mxohax] EnsureInWorld: Instantiated HUD control 0x%02X\n", id);
                }
                pSetVisible(pUI, id, 1);
                Log("[mxohax] EnsureInWorld: Dispatched pUI->SetControlVisible(0x%02X, 1)!\n", id);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("[mxohax] EnsureInWorld: Exception on HUD control 0x%02X\n", id);
            }
        }

        void* pChatMgr = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x38);
        if (pChatMgr && !IsBadReadPtr(pChatMgr, 0x60)) {
            *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pChatMgr) + 0x58) = 1;
            Log("[mxohax] EnsureInWorld: Activated Chat Manager tabs!\n");
        }

        // Ensure optional dialogs (0x42 Char Sheet, 0x47 Options) remain hidden initially
        __try {
            if (OriginalHideControl) {
                OriginalHideControl(pUI, 0x42);
                OriginalHideControl(pUI, 0x47);
            }
            pSetVisible(pUI, 0x42, 0);
            pSetVisible(pUI, 0x47, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        LockAllHudFrames(clientBase, pUI);
        TriggerNativeIdleTransition(clientBase);
    }

    // Step 6: Ensure pWorldMgr + 0xC (viewport list) has a valid Viewport object bound to the active camera
    void** ppListHead = reinterpret_cast<void**>(reinterpret_cast<DWORD>(pWorldMgr) + 0xC);
    if (ppListHead && *ppListHead) {
        void* head = *ppListHead;
        if (promoteToState3) {
            // Reset circular list to bind to the active State 3 camera
            *reinterpret_cast<void**>(head) = head;
            *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(head) + 4) = head;
            Log("[mxohax] EnsureInWorld: Reset viewport circular list head to rebind active camera upon State 3 promotion.\n");
        }
        void* first = *reinterpret_cast<void**>(head);
        if (first == head) {
            // Viewport list empty -> call CWorldMgr::CreateViewport(0x1011EAD0)
            int width = *reinterpret_cast<int*>(clientBase + 0x00896CCC);
            int height = *reinterpret_cast<int*>(clientBase + 0x00896D04);
            if (width <= 0 || height <= 0) {
                width = 1920;
                height = 1080;
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
    if (pVpCount) {
        *pVpCount = 1;
        Log("[mxohax] EnsureInWorld: Set viewport count [pWorldMgr+8] to 1\n");
    }

    // Step 8: Set render & in-world flags
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27) = 1;
    if (pShell) {
        HWND hWnd = *reinterpret_cast<HWND*>(pShell + 0x14);
        if (!hWnd) hWnd = g_hGameWindow;
        if (hWnd) {
            ShowWindow(hWnd, SW_RESTORE);
            SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 1920, 1080, SWP_SHOWWINDOW);
            SetForegroundWindow(hWnd);
        }
    }

    s_inStreamingState4 = false;
    if (pShell) {
        *reinterpret_cast<BYTE*>(pShell + 0x20) = 1; // CClientShell::m_inWorld = 1
    }
    DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
    if (pState) {
        *pState = 3;
    }
    s_inWorldSticky = true;
    Log("[mxohax] ******************************************************\n");
    Log("[mxohax] *** PROMOTED TO STATE 3 (IN-WORLD)! 3D SIMULATION ACTIVE! ***\n");
    Log("[mxohax] ******************************************************\n");

    void* curWorldInst = *reinterpret_cast<void**>(clientBase + 0x0089DD6C);
    void* curPlayer = *reinterpret_cast<void**>(clientBase + 0x008A4378);
    void* curCam = *reinterpret_cast<void**>(clientBase + 0x0089EDF8);
    Log("[mxohax] DetourFrameTick: Tick #%d active (State=3, renderFlag=1, inWorld=1, sticky=1, WorldInst=0x%p, Player=0x%p, Cam=0x%p)\n",
        s_tickCount, curWorldInst, curPlayer, curCam);

    // Turn off Matrix View for solid world gameplay
    *reinterpret_cast<float*>(clientBase + 0x008E357C) = 0.0f;
    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 0;
    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 0;

    // Dismiss 2D loading screens
    if (pUI && OriginalHideControl) {
        OriginalHideControl(pUI, 0x04);
        OriginalHideControl(pUI, 0x57);
        OriginalHideControl(pUI, 0x30);
        OriginalHideControl(pUI, 0x5D);
        Log("[mxohax] EnsureInWorld: Dismissed loading screens 0x04, 0x57, 0x30 and 0x5D!\n");
    }
}

// Forward declarations
static unsigned char __stdcall Safe_GetPlayerActiveObject(void** outObj, void** outSubObj);

// 0x001F9140: True per-frame tick on main thread (called inside RunClientDLL 0x10006640)
typedef void (__thiscall *FrameTick_t)(void* pThis);
static FrameTick_t OriginalFrameTick = nullptr;

static void __fastcall DetourFrameTick(void* pThis, void* /*edx*/) {
    HMODULE hClient = GetModuleHandleA("client.dll");
    DWORD clientBase = hClient ? reinterpret_cast<DWORD>(hClient) : 0;

    // Neutralize dragging global before tick
    if (clientBase) {
        NeutralizeDragGlobals(clientBase);
    }

    if (OriginalFrameTick) OriginalFrameTick(pThis);
    s_tickCount++;

    if (!clientBase) return;
    NeutralizeDragGlobals(clientBase);

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
            void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
            if (pUI && OriginalHideControl) {
                OriginalHideControl(pUI, 0x04);
                OriginalHideControl(pUI, 0x57);
                OriginalHideControl(pUI, 0x30);
                OriginalHideControl(pUI, 0x5D);
            }
            if (pUI) {
                typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
                typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
                CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
                SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);

                static const DWORD hudControls[] = { 0x1B, 0x27, 0x24, 0x02, 0x03, 0x23, 0x4D };
                for (DWORD id : hudControls) {
                    __try {
                        void** ppCtrl = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x28 + (id * 4));
                        if (!ppCtrl || !*ppCtrl) {
                            pCreateControl(pUI, id);
                        }
                        pSetVisible(pUI, id, 1);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
                // Ensure dialogs (0x42 Char Sheet, 0x47 Options) remain hidden initially
                __try {
                    if (OriginalHideControl) {
                        OriginalHideControl(pUI, 0x42);
                        OriginalHideControl(pUI, 0x47);
                        if (!g_hasTarget) {
                            OriginalHideControl(pUI, 0x22);
                            OriginalHideControl(pUI, 0x3D);
                        }
                    }
                    pSetVisible(pUI, 0x42, 0);
                    pSetVisible(pUI, 0x47, 0);
                    if (!g_hasTarget) {
                        pSetVisible(pUI, 0x22, 0);
                        pSetVisible(pUI, 0x3D, 0);
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                LockAllHudFrames(clientBase, pUI);
                TriggerNativeIdleTransition(clientBase);

                void* pChatMgr = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x38);
                if (pChatMgr && !IsBadReadPtr(pChatMgr, 0x60)) {
                    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pChatMgr) + 0x58) = 1;
                }
            }
            ApplyOperativeAppearance(clientBase, curPlayer);
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

        // Ensure Matrix Rain / View Shaders stay 0.0f in world to eliminate white glare/rain bloom
        *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 0;
        *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 0;
        *reinterpret_cast<float*>(clientBase + 0x008E357C) = 0.0f;

        // Tame Blinding "Nuclear Glare" & Auto-Inflating Bloom (Restore Launch-Era 2005 Matrix Noir Atmosphere)
        // 1. Cone downlight on character and concourse (offsets +0x30 and +0x2C)
        *reinterpret_cast<float*>(clientBase + 0x008BAE74 + 0x30) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x008BAE74 + 0x2C) = 0.0f;

        // 2. Light glare alpha: 0.0f
        *reinterpret_cast<float*>(clientBase + 0x008A9B44 + 0x30) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x008A9B44 + 0x2C) = 0.0f;

        // 3. World renderer lens flare scale: 0.0f
        *reinterpret_cast<float*>(clientBase + 0x00909694 + 0x30) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x00909694 + 0x2C) = 0.0f;

        // 4. Glow bright factor (matrix noir high contrast: 0.25f)
        *reinterpret_cast<float*>(clientBase + 0x008A3390 + 0x30) = 0.25f;
        *reinterpret_cast<float*>(clientBase + 0x008A3390 + 0x2C) = 0.25f;

        // 5. Glow AutoSetVals (disable dynamic bloom blowouts)
        *reinterpret_cast<float*>(clientBase + 0x008A33C4 + 0x30) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x008A33C4 + 0x2C) = 0.0f;

        // 6. Glow outdoor day bright factor: 0.25f (tames outdoor sky glare)
        *reinterpret_cast<float*>(clientBase + 0x008A350C + 0x30) = 0.25f;
        *reinterpret_cast<float*>(clientBase + 0x008A350C + 0x2C) = 0.25f;

        // 7. Glow blur scale (tame blurry washout): 0.0f
        *reinterpret_cast<float*>(clientBase + 0x008A335C + 0x30) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x008A335C + 0x2C) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x008A34D8 + 0x30) = 0.0f;
        *reinterpret_cast<float*>(clientBase + 0x008A34D8 + 0x2C) = 0.0f;

        // 8. Glow subval thresholds (0.8f prevents background sky bloom blowout)
        *reinterpret_cast<float*>(clientBase + 0x008A3328 + 0x30) = 0.8f;
        *reinterpret_cast<float*>(clientBase + 0x008A3328 + 0x2C) = 0.8f;
        *reinterpret_cast<float*>(clientBase + 0x008A34A4 + 0x30) = 0.8f;
        *reinterpret_cast<float*>(clientBase + 0x008A34A4 + 0x2C) = 0.8f;

        // 9. DL_FloorHeight (track player elevation)
        *reinterpret_cast<float*>(clientBase + 0x00908C90 + 0x30) = (float)g_playerY;
        *reinterpret_cast<float*>(clientBase + 0x00908C90 + 0x2C) = (float)g_playerY;

        // Re-enforce HUD frame positions periodically (every 30 ticks ~ 0.5s) to guarantee zero layout drift
        if (s_inWorldTicks % 30 == 0) {
            void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
            if (pUI) {
                LockAllHudFrames(clientBase, pUI);
            }
        }

        // Reset subclass if current hooked window was destroyed
        if (OriginalWndProc && (!g_hGameWindow || !IsWindow(g_hGameWindow))) {
            Log("[mxohax] Subclassed window 0x%p died or invalid. Resetting subclass state.\n", g_hGameWindow);
            OriginalWndProc = nullptr;
            g_hGameWindow = NULL;
        }

        // Subclass window if not yet hooked
        static bool s_wndAuditDone = false;
        if (!s_wndAuditDone && pShell) {
            s_wndAuditDone = true;
            HWND shellWnd = *reinterpret_cast<HWND*>(pShell + 0x14);
            HWND mxClassWnd = FindWindowA("MatrixWindowClass", NULL);
            HWND mxTitleWnd = FindWindowA(NULL, "The Matrix Online");
            HWND fgWnd = GetForegroundWindow();
            HWND actWnd = GetActiveWindow();
            Log("[mxohax] WINDOW AUDIT: g_hGameWindow=0x%p, shellWnd=0x%p (isW=%d), mxClass=0x%p (isW=%d), mxTitle=0x%p (isW=%d), fg=0x%p, act=0x%p\n",
                g_hGameWindow, shellWnd, IsWindow(shellWnd), mxClassWnd, IsWindow(mxClassWnd), mxTitleWnd, IsWindow(mxTitleWnd), fgWnd, actWnd);
        }

        if (!OriginalWndProc || !g_hGameWindow || !IsWindow(g_hGameWindow)) {
            HWND hWnd = NULL;
            if (pShell && !IsBadReadPtr((void*)pShell, 0x30)) {
                HWND shellWnd = *reinterpret_cast<HWND*>(pShell + 0x14);
                if (shellWnd && IsWindow(shellWnd)) {
                    RECT r; GetClientRect(shellWnd, &r);
                    if ((r.right - r.left) >= 640 && (r.bottom - r.top) >= 480) hWnd = shellWnd;
                }
            }
            if (!hWnd && g_hGameWindow && IsWindow(g_hGameWindow)) {
                RECT r; GetClientRect(g_hGameWindow, &r);
                if ((r.right - r.left) >= 640 && (r.bottom - r.top) >= 480) hWnd = g_hGameWindow;
            }
            if (!hWnd) {
                HWND w = FindWindowA("MatrixWindowClass", NULL);
                if (!w) w = FindWindowA(NULL, "The Matrix Online");
                if (w && IsWindow(w)) {
                    RECT r; GetClientRect(w, &r);
                    if ((r.right - r.left) >= 640 && (r.bottom - r.top) >= 480) hWnd = w;
                }
            }
            if (!hWnd) {
                HWND fg = GetForegroundWindow();
                if (fg && IsWindow(fg)) {
                    DWORD fgPid = 0;
                    GetWindowThreadProcessId(fg, &fgPid);
                    if (fgPid == GetCurrentProcessId()) {
                        RECT r; GetClientRect(fg, &r);
                        if ((r.right - r.left) >= 640 && (r.bottom - r.top) >= 480) hWnd = fg;
                    }
                }
            }
            if (hWnd && IsWindow(hWnd)) {
                SubclassGameWindow(hWnd);
            }
        }

        // Check human keyboard inputs (real window messages + GetAsyncKeyState fallback if game has process focus)
        HWND fgWnd = GetForegroundWindow();
        DWORD fgPid = 0;
        if (fgWnd) GetWindowThreadProcessId(fgWnd, &fgPid);
        bool hasFocus = (fgPid == GetCurrentProcessId()) || (g_hGameWindow && fgWnd == g_hGameWindow);
        bool keyW = s_keysDown['W'] || s_keysDown['w'] || s_keysDown[VK_UP] || (hasFocus && (((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP) & 0x8000) != 0)));
        bool keyS = s_keysDown['S'] || s_keysDown['s'] || s_keysDown[VK_DOWN] || (hasFocus && (((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0)));
        bool keyA = s_keysDown['A'] || s_keysDown['a'] || (hasFocus && ((GetAsyncKeyState('A') & 0x8000) != 0));
        bool keyD = s_keysDown['D'] || s_keysDown['d'] || (hasFocus && ((GetAsyncKeyState('D') & 0x8000) != 0));
        bool keySpace = s_keysDown[VK_SPACE] || (hasFocus && ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0));
        bool keyShift = s_keysDown[VK_SHIFT] || (hasFocus && ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0));
        bool keyLeft = s_keysDown[VK_LEFT] || (hasFocus && ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0));
        bool keyRight = s_keysDown[VK_RIGHT] || (hasFocus && ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0));

        g_bHumanInputActive = true;

        double speed = keyShift ? 480.0 : 280.0;
        double moveFwd = 0.0;
        double moveRight = 0.0;
        if (keyW) moveFwd += 1.0;
        if (keyS) moveFwd -= 1.0;
        if (keyD) moveRight += 1.0;
        if (keyA) moveRight -= 1.0;

        if (keyLeft)  g_camYaw -= 2.0f * (float)dt;
        if (keyRight) g_camYaw += 2.0f * (float)dt;

        bool isMoving = false;
        double actVelX = 0.0;
        double actVelZ = 0.0;

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
                static int s_moveLog = 0;
                if (++s_moveLog % 10 == 0) {
                    Log("[mxohax] Locomotion: Moving Player=(%.1f, %.1f, %.1f) Yaw=%.2f Speed=%.1f\n",
                        g_playerX, g_playerY, g_playerZ, g_playerYaw, speed);
                }
            }
        }
        g_bPlayerIsMoving = isMoving;

        // Apply position, orientation and extents to player and actor
        if (curPlayer && !IsBadReadPtr(curPlayer, 0xB0)) {
            *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(curPlayer) + 0xC) |= 0x20;
            UpdatePlayerPositionAndPhysics(clientBase, curPlayer, dt, isMoving, actVelX, actVelZ);

            void* pActor = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(curPlayer) + 0xA8);
            if (pActor && !IsBadReadPtr(pActor, 0x690)) {
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x374) = 0; // Local player visible
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x375) = 1;
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x2DD) = 0; // Not culled
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x684) = 0; // In-world
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x385) = 3; // Scene transform valid
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x290) = 0;
                *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = isMoving ? 1.0f : 0.0f;
                if (!isMoving && !g_isJumping) {
                    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 1;
                    *reinterpret_cast<WORD*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 1;
                    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x510) = 0.0;
                    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x518) = 0.0;
                    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x520) = 0.0;
                    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x528) = g_playerX;
                    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x530) = g_playerY;
                    *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(pActor) + 0x538) = g_playerZ;
                    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pActor) + 0x56C) = 0.0f;
                } else {
                    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 0;
                    *reinterpret_cast<WORD*>(reinterpret_cast<uintptr_t>(pActor) + 0x4EE) = 0;
                }

                // Local player visibility and transform managed cleanly
            }
        }

        // Permanently enforce Third Person Chase Camera Mode CVar
        *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 2;

        void** ppCamera = reinterpret_cast<void**>(clientBase + 0x0089EDF8);
        if (ppCamera && *ppCamera) {
            void* pCam = *ppCamera;
            DWORD* pMode = reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pCam) + 0x8C);
            if (pMode && *pMode != 2) {
                typedef void (__thiscall *SetCameraMode_t)(void* pCamMgr, int mode);
                SetCameraMode_t pSetMode = reinterpret_cast<SetCameraMode_t>(clientBase + 0x0012D200);
                pSetMode(pCam, 2);
            }
            DWORD* pTargetHandle = reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pCam) + 4);
            if (pTargetHandle) *pTargetHandle = 0;

            UpdateCamera(clientBase, pCam);

            static int s_camTickLog = 0;
            if (++s_camTickLog % 60 == 0) {
                Log("[mxohax] DetourFrameTick: Player=(%.1f, %.1f, %.1f) CamYaw=%.2f Pitch=%.2f Dist=%.1f Target='%s'\n",
                    g_playerX, g_playerY, g_playerZ, g_camYaw, g_camPitch, g_camDist, g_hasTarget ? g_targetName : "None");
            }
        }


        // Keep HUD controls locked in place and Chat Window manager tabs activated
        void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
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
            // Reposition and persist HUD every frame tick so it never disappears or moves
            RepositionQuickbar(clientBase, pUI, screenW, screenH);
            RepositionCompass(clientBase, pUI, screenW, screenH);
            RepositionCombatTactics(clientBase, pUI, screenW, screenH);
        }

        static int s_hudTickCheck = 0;
        if (++s_hudTickCheck % 60 == 0) {
            if (pUI) {
                LockAllHudFrames(clientBase, pUI);
                void* pChatMgr = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pUI) + 0x38);
                if (pChatMgr && !IsBadReadPtr(pChatMgr, 0x60)) {
                    *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pChatMgr) + 0x58) = 1;
                }
            }
        }

        // Keep Viewport count active for 3D rendering
        if (pWorldMgr) {
            DWORD* pVpCount = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 8);
            if (pVpCount && *pVpCount == 0) {
                *pVpCount = 1;
            }
        }

        // Ensure World Instance is loaded
        void** ppWorldInstSticky = reinterpret_cast<void**>(clientBase + 0x0089DD6C);
        if (ppWorldInstSticky && !*ppWorldInstSticky && pWorldMgr) {
            typedef char (__thiscall *LoadWorldFile_t)(void* pMgr);
            LoadWorldFile_t pLoadWorld = reinterpret_cast<LoadWorldFile_t>(clientBase + 0x00121110);
            pLoadWorld(pWorldMgr);
            Log("[mxohax] DetourFrameTick: Fallback LoadWorldFile executed -> [0x1089DD6C]=0x%p\n", *ppWorldInstSticky);
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
            // Only dismiss loading screens if already promoted to State 3 (In-World).
            // Do NOT dismiss Phase 1 loading screen 0x57 while streaming in State 4!
            if (s_inWorldSticky) {
                void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
                if (pUI && OriginalHideControl) {
                    OriginalHideControl(pUI, 0x04);
                    OriginalHideControl(pUI, 0x57);
                    OriginalHideControl(pUI, 0x30);
                    OriginalHideControl(pUI, 0x5D);
                    Log("[mxohax] Dismissed loading screens 0x04, 0x57, 0x30 and 0x5D upon entering State 3 world!\n");
                }
            }
        }
    }

    if (s_tickCount % 50 == 0) {
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
    // s_state4Ticks is global static volatile int
    if (pWorldMgr && !s_inWorldSticky) {
        DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pState) {
            if (*pState == 1) {
                s_state1Ticks++;
                // Allow Margin auth and Screen 0x5D to execute naturally!
                // Only trigger fallback AutoJackIn if stuck in State 1 for > 500 ticks
                if (s_state1Ticks >= 40 && !s_autoJackInDone) {
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
                s_inStreamingState4 = true;
                s_state4Ticks++;
                if (s_state4Ticks % 15 == 0) {
                    Log("[mxohax] DetourFrameTick: State 4 (Streaming) active (tick %d)...\n", s_state4Ticks);
                }

                static int s_dissolveStartTick = 0;

                // Check world loaded status
                void* pWorldInst = *reinterpret_cast<void**>(clientBase + 0x0089DD6C);
                BYTE bWorldFlag = pWorldMgr ? *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27) : 0;
                float fProgress = *reinterpret_cast<float*>(clientBase + 0x00849E58);
                bool bWorldFullyLoaded = (bWorldFlag != 0 || fProgress >= 1.0f || (pWorldInst != nullptr && s_state4Ticks >= 20));

                // ============================================================
                // PHASE 1: 2D Loading Screen (ticks 0 to 15, ~0.25 sec)
                // Display authentic loading artwork while initializing archives
                // ============================================================
                static bool s_phase1Shown = false;
                static bool s_phase2Dismissed = false;
                if (s_state4Ticks < 15) {
                    if (!s_phase1Shown) {
                        s_phase1Shown = true;
                        void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
                        if (pUI) {
                            CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
                            SetControlVisible_t pSetVisible = reinterpret_cast<SetControlVisible_t>(clientBase + 0x0001DB80);
                            __try {
                                pCreateControl(pUI, 0x04);
                                pSetVisible(pUI, 0x04, 1);
                                pCreateControl(pUI, 0x57);
                                pSetVisible(pUI, 0x57, 1);
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                    // Keep 3D Matrix View OFF during Phase 1 so 2D Loading Screen artwork is front & center
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 0;
                    *reinterpret_cast<float*>(clientBase + 0x008E357C) = 0.0f;
                }
                // ============================================================
                // PHASE 2: Matrix Digital Code Rain Stream (ticks >= 15 to 30)
                // Authentic 2005 Matrix digital rain stream cascades down while
                // sector geometry, textures, buildings, and ground fully stream in!
                // ============================================================
                else if (s_dissolveStartTick == 0 && (s_state4Ticks < 30 || (!bWorldFullyLoaded && s_state4Ticks < 45))) {
                    // 1. Dismiss 2D loading screens (0x57, 0x04) once to reveal the falling code stream
                    if (!s_phase2Dismissed) {
                        s_phase2Dismissed = true;
                        void* pUI = *reinterpret_cast<void**>(clientBase + 0x00898C54);
                        if (pUI && OriginalHideControl) {
                            OriginalHideControl(pUI, 0x57);
                            OriginalHideControl(pUI, 0x04);
                            OriginalHideControl(pUI, 0x30);
                        }
                    }

                    // 2. Enable 3D scene rendering flags in WorldMgr
                    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
                    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;

                    // 3. Engage native 3D Matrix View Digital Rain effect
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 1;
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 1;
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 1;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 1;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 1;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 1;
                    *reinterpret_cast<float*>(clientBase + 0x008E357C) = 1.0f; // Pure full digital code rain

                    // 4. Initialize in-world scene/player for State 4 streaming rez-in
                    if (!s_playerEnteredWorld) {
                        Log("[mxohax] DetourFrameTick: Initializing in-world scene/player for Phase 2 State 4 streaming rez-in...\n");
                        EnsureInWorldRendering(clientBase, pWorldMgr, pShell, false /* keepInState4 */);
                    }
                }
                // ============================================================
                // PHASE 3: Digital Rain Dissolve / World Rez-In (runs for 25 ticks once world is fully loaded)
                // World has finished streaming; falling green code eases out into reality!
                // ============================================================
                else if (s_dissolveStartTick == 0 || (s_state4Ticks < s_dissolveStartTick + 25)) {
                    if (s_dissolveStartTick == 0) {
                        s_dissolveStartTick = s_state4Ticks;
                        Log("[mxohax] DetourFrameTick: World confirmed fully loaded! Beginning Phase 3 digital rain dissolve at tick %d...\n", s_dissolveStartTick);
                    }

                    // Enable 3D scene rendering flags in WorldMgr
                    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
                    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;

                    // Calculate smooth ease-out dissolve: starts at 1.0f (pure code) and dissolves to 0.0f
                    float rezProgress = (float)(s_state4Ticks - s_dissolveStartTick) / 25.0f;
                    if (rezProgress > 1.0f) rezProgress = 1.0f;
                    float rezBlend = 1.0f - (rezProgress * rezProgress); // quadratic ease-out dissolve
                    *reinterpret_cast<float*>(clientBase + 0x008E357C) = rezBlend;

                    if (!s_playerEnteredWorld) {
                        EnsureInWorldRendering(clientBase, pWorldMgr, pShell, false /* keepInState4 */);
                    }
                }
                // ============================================================
                // PHASE 4: Full World Emergence & Promotion to State 3 (after dissolve completes)
                // Fully loaded 3D world emerges from code, solid gameplay active!
                // ============================================================
                else {
                    Log("[mxohax] DetourFrameTick: Matrix code streaming fully complete (ticks=%d)! World 100%% rezzed. Promoting to State 3...\n", s_state4Ticks);

                    // Ensure active world geometry buffer is ready (0xE0 = 1, 0xB9 = 1)
                    void* pLevelSys = *reinterpret_cast<void**>(clientBase + 0x008A6004);
                    if (pLevelSys) {
                        DWORD* pActiveWorld = *reinterpret_cast<DWORD**>(reinterpret_cast<DWORD>(pLevelSys) + 0x18);
                        if (pActiveWorld) {
                            *reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pActiveWorld) + 0xE0) = 1;
                            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pActiveWorld) + 0xB9) = 1;
                            Log("[mxohax] DetourFrameTick: Marked active world at 0x%p geometry ready (0xE0=1, 0xB9=1)!\n", pActiveWorld);
                        }
                    }

                    // Turn off Matrix Rain Shaders
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA8) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBA9) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x0085EBAA) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008E3590) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC8) = 0;
                    *reinterpret_cast<BYTE*>(clientBase + 0x008AACC9) = 0;
                    *reinterpret_cast<float*>(clientBase + 0x008E357C) = 0.0f;

                    s_inStreamingState4 = false;
                    s_dissolveStartTick = 0;
                    EnsureInWorldRendering(clientBase, pWorldMgr, pShell, true /* promoteToState3 */);
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
    if (s_inWorldSticky) {
        Log("[mxohax] DetourExitProcess: SUPPRESSED exit call (in-world sticky session active)!\n");
        return;
    }
    if (OriginalExitProcess) OriginalExitProcess(uExitCode);
}

typedef void (WINAPI *PostQuitMessage_t)(int nExitCode);
static PostQuitMessage_t OriginalPostQuitMessage = nullptr;

void WINAPI DetourPostQuitMessage(int nExitCode) {
    void* caller = _ReturnAddress();
    Log("[mxohax] PostQuitMessage(%d) called! ReturnAddress: 0x%p\n", nExitCode, caller);
    if (s_inWorldSticky) {
        Log("[mxohax] DetourPostQuitMessage: SUPPRESSED exit loop call (in-world sticky session active)!\n");
        return;
    }
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
// Resolves active player and sub-object for CWorldMgr::Update (0x1012A940)
// Passes pPlayer as active world object to ensure in-world character rendering
// (0x100EB090 and 0x100EE510) runs every frame!
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

        void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
        uintptr_t* ppGlobal = reinterpret_cast<uintptr_t*>(clientBase + 0x008A4378);
        if (!ppGlobal || IsBadReadPtr(ppGlobal, sizeof(uintptr_t)) || !*ppGlobal) return 0;
        uintptr_t pPlayer = *ppGlobal;

        // 1. Attempt standard native object query via [pActor + 0x23C]
        uintptr_t* ppA8 = reinterpret_cast<uintptr_t*>(pPlayer + 0xA8);
        if (ppA8 && !IsBadReadPtr(ppA8, sizeof(uintptr_t)) && *ppA8) {
            uintptr_t pA8 = *ppA8;
            uintptr_t* pp23C = reinterpret_cast<uintptr_t*>(pA8 + 0x23C);
            if (pp23C && !IsBadReadPtr(pp23C, sizeof(uintptr_t)) && *pp23C) {
                uintptr_t p23C = *pp23C;
                uintptr_t* ppESI = reinterpret_cast<uintptr_t*>(p23C);
                if (ppESI && !IsBadReadPtr(ppESI, sizeof(uintptr_t)) && *ppESI) {
                    uintptr_t pESI = *ppESI;
                    if (!IsBadReadPtr(reinterpret_cast<void*>(pESI), 6) &&
                        *reinterpret_cast<uint16_t*>(pESI + 4) != 0xFFFF) {
                        uint16_t idx0 = *reinterpret_cast<uint16_t*>(pESI);
                        uintptr_t* ppMgr = reinterpret_cast<uintptr_t*>(clientBase + 0x00897F90);
                        if (ppMgr && !IsBadReadPtr(ppMgr, sizeof(uintptr_t)) && *ppMgr) {
                            uintptr_t pMgr = *ppMgr;
                            typedef void* (__thiscall *FnGetObj)(void* thisPtr, uint32_t id);
                            FnGetObj pfnGetObj = reinterpret_cast<FnGetObj>(clientBase + 0x003A5CA0);
                            void* obj = pfnGetObj(reinterpret_cast<void*>(pMgr), idx0);
                            if (obj && !IsBadReadPtr(obj, sizeof(void*))) {
                                uint16_t idx1 = *reinterpret_cast<uint16_t*>(pESI + 2);
                                void*** pppVtable = reinterpret_cast<void***>(obj);
                                if (pppVtable && !IsBadReadPtr(pppVtable, sizeof(void**)) && *pppVtable) {
                                    void** vtable = *pppVtable;
                                    if (!IsBadReadPtr(vtable, 0x60)) {
                                        typedef void* (__thiscall *FnGetSubObj)(void* thisPtr, uint32_t id);
                                        FnGetSubObj pfnGetSubObj = reinterpret_cast<FnGetSubObj>(vtable[0x58 / 4]);
                                        if (pfnGetSubObj) {
                                            void* subObj = pfnGetSubObj(obj, idx1);
                                            if (outObj) *outObj = obj;
                                            if (outSubObj) *outSubObj = subObj;
                                            if (pWorldMgr) {
                                                *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pWorldMgr) + 0x90) = obj;
                                                *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pWorldMgr) + 0x94) = subObj;
                                            }
                                            return (subObj != nullptr) ? 1 : 0;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return 0;
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

static InterlockButtonFn Original_Interlock_Speed_Button = nullptr;
static InterlockButtonFn Original_Interlock_Power_Button = nullptr;
static InterlockButtonFn Original_Interlock_Grab_Button = nullptr;
static InterlockButtonFn Original_Interlock_Block_Button = nullptr;

static void __fastcall Safe_Interlock_Speed_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Speed_Button (+0x6C Focus/Free) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_FREE;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x6C);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Speed_Button: button ptr (+0x6C) is null/invalid\n");
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

static void __fastcall Safe_Interlock_Power_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Power_Button (+0x70 Power) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_POWER;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x70);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Power_Button: button ptr (+0x70) is null/invalid\n");
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

static void __fastcall Safe_Interlock_Grab_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Grab_Button (+0x74 Grab/Attack) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_GRAB;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x74);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Grab_Button: button ptr (+0x74) is null/invalid\n");
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

static void __fastcall Safe_Interlock_Block_Button(void* pThis, void* /*edx*/) {
    Log("[mxohax] Safe_Interlock_Block_Button (+0x80 Block/Defense) called (pThis=0x%p)\n", pThis);
    g_currentStance = STANCE_WITHDRAW;
    if (!pThis || IsBadReadPtr(pThis, 0x100)) return;
    void* pBtn = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x80);
    if (!pBtn || IsBadReadPtr(pBtn, sizeof(void*))) {
        Log("[mxohax] Safe_Interlock_Block_Button: button ptr (+0x80) is null/invalid\n");
        return;
    }
    void* vtbl = *reinterpret_cast<void**>(pBtn);
    if (!vtbl || IsBadReadPtr(vtbl, 0xB0)) {
        Log("[mxohax] Safe_Interlock_Block_Button: button vtable is null/invalid\n");
        return;
    }
    __try {
        if (Original_Interlock_Block_Button) {
            Original_Interlock_Block_Button(pThis);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mxohax] Exception caught safely in Safe_Interlock_Block_Button!\n");
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

// CActor visibility handled cleanly via native scene graph and HideLocalPlayer neutralization


static void ApplyClientPatches(HMODULE hClient) {
    static LONG s_patchLock = 0;
    if (InterlockedCompareExchange(&s_patchLock, 1, 0) != 0 || !hClient) return;

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

    // 3b. Hook SetControlPos (0x00015D60) to block unauthorized HUD frame movement
    LPVOID pSetCtrlPos = reinterpret_cast<LPVOID>(clientBase + 0x00015D60);
    if (MH_CreateHook(pSetCtrlPos, &DetourSetControlPos, reinterpret_cast<LPVOID*>(&OriginalSetControlPos)) == MH_OK) {
        MH_EnableHook(pSetCtrlPos);
        Log("[mxohax] SUCCESS: client.dll SetControlPos hooked at 0x%p!\n", pSetCtrlPos);
    }

    // 3c. Hook CLTWidget::SetPosition (0x00382360) to permanently block HUD widget dragging & movement
    LPVOID pSetWidgetPos = reinterpret_cast<LPVOID>(clientBase + 0x00382360);
    if (MH_CreateHook(pSetWidgetPos, &DetourWidgetSetPosition, reinterpret_cast<LPVOID*>(&OriginalWidgetSetPosition)) == MH_OK) {
        MH_EnableHook(pSetWidgetPos);
        Log("[mxohax] SUCCESS: client.dll CLTWidget::SetPosition hooked at 0x%p to permanently immobilize HUD widgets!\n", pSetWidgetPos);
    }

    // 3d. Hook CUI::BeginDrag (0x000184E0) to permanently prevent frame dragging initiation
    LPVOID pStartDrag = reinterpret_cast<LPVOID>(clientBase + 0x000184E0);
    if (MH_CreateHook(pStartDrag, &DetourBeginDrag, reinterpret_cast<LPVOID*>(&OriginalBeginDrag)) == MH_OK) {
        MH_EnableHook(pStartDrag);
        Log("[mxohax] SUCCESS: client.dll CUI::BeginDrag hooked at 0x%p!\n", pStartDrag);
    }

    // 3e. Hook CUI::OnDragMove (0x00018540) to drop drag movement updates
    LPVOID pOnDragMove = reinterpret_cast<LPVOID>(clientBase + 0x00018540);
    if (MH_CreateHook(pOnDragMove, &DetourOnDragMove, reinterpret_cast<LPVOID*>(&OriginalOnDragMove)) == MH_OK) {
        MH_EnableHook(pOnDragMove);
        Log("[mxohax] SUCCESS: client.dll CUI::OnDragMove hooked at 0x%p!\n", pOnDragMove);
    }

    // 3f. Hook CUI::BeginResize (0x00018590) to permanently prevent frame resizing initiation
    LPVOID pStartResize = reinterpret_cast<LPVOID>(clientBase + 0x00018590);
    if (MH_CreateHook(pStartResize, &DetourBeginResize, reinterpret_cast<LPVOID*>(&OriginalBeginResize)) == MH_OK) {
        MH_EnableHook(pStartResize);
        Log("[mxohax] SUCCESS: client.dll CUI::BeginResize hooked at 0x%p!\n", pStartResize);
    }

    // 3g. Hook CUI::UpdateResize (0x000187B0) to drop resize updates
    LPVOID pOnResizeMove = reinterpret_cast<LPVOID>(clientBase + 0x000187B0);
    if (MH_CreateHook(pOnResizeMove, &DetourOnResizeMove, reinterpret_cast<LPVOID*>(&OriginalOnResizeMove)) == MH_OK) {
        MH_EnableHook(pOnResizeMove);
        Log("[mxohax] SUCCESS: client.dll CUI::UpdateResize hooked at 0x%p!\n", pOnResizeMove);
    }


    // 4. Hook GetPlayerActiveObject (0x0010A210) to guard against NULL player entity dereference
    LPVOID pGetActiveObj = reinterpret_cast<LPVOID>(clientBase + 0x0010A210);
    if (MH_CreateHook(pGetActiveObj, &Safe_GetPlayerActiveObject, reinterpret_cast<LPVOID*>(&OriginalGetPlayerActiveObject)) == MH_OK) {
        MH_EnableHook(pGetActiveObj);
        Log("[mxohax] SUCCESS: client.dll GetPlayerActiveObject hooked at 0x%p! Null dereference guarded.\n", pGetActiveObj);
    }

    // 4c. Patch CActor::HideLocalPlayer entry (0x104ECDB0) with ret 8 (C2 08 00)
    // Permanently prevents CActor::HideLocalPlayer from ever stripping meshes or hiding operative
    LPVOID pHideActorEntry = reinterpret_cast<LPVOID>(clientBase + 0x004ECDB0);
    DWORD oldProtHideL = 0;
    if (VirtualProtect(pHideActorEntry, 3, PAGE_EXECUTE_READWRITE, &oldProtHideL)) {
        BYTE ret8[3] = { 0xC2, 0x08, 0x00 };
        memcpy(pHideActorEntry, ret8, 3);
        VirtualProtect(pHideActorEntry, 3, oldProtHideL, &oldProtHideL);
        FlushInstructionCache(GetCurrentProcess(), pHideActorEntry, 3);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x004ECDB0 (ret 8) to permanently neutralize CActor::HideLocalPlayer!\n");
    }

    // 4e. Patch CActor::ShowLocalPlayer visibility load (clientBase + 0x004EABCF)
    // Overwrites 'mov dl, byte ptr [esi + 0x375]' (8A 96 75 03 00 00) with 'mov dl, 1; nop * 4' (B2 01 90 90 90 90)
    // Guarantees ShowLocalPlayer unconditionally passes visibility=1 to SetProperty(0x102592E0)
    LPVOID pShowVisLoad = reinterpret_cast<LPVOID>(clientBase + 0x004EABCF);
    DWORD oldProtShowVis = 0;
    if (VirtualProtect(pShowVisLoad, 6, PAGE_EXECUTE_READWRITE, &oldProtShowVis)) {
        BYTE patchShowVis[6] = { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 };
        memcpy(pShowVisLoad, patchShowVis, 6);
        VirtualProtect(pShowVisLoad, 6, oldProtShowVis, &oldProtShowVis);
        FlushInstructionCache(GetCurrentProcess(), pShowVisLoad, 6);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x004EABCF (mov dl, 1) to enforce local player visibility!\n");
    }

    // 4d. Enforce Third Person Chase Camera Mode CVar at clientBase + 0x008971C8
    *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 1;
    Log("[mxohax] SUCCESS: Enforced Camera_Mode [0x%08X] = 1 (Third Person Chase Camera)!\n", clientBase + 0x008971C8);

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
    // client.dll + 0x0009A170: Interlock_Speed_Button (+0x6C, Focus / Free)
    LPVOID pSpeed = reinterpret_cast<LPVOID>(clientBase + 0x0009A170);
    if (MH_CreateHook(pSpeed, reinterpret_cast<LPVOID>(&Safe_Interlock_Speed_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Speed_Button)) == MH_OK) {
        MH_EnableHook(pSpeed);
        Log("[mxohax] SUCCESS: client.dll Interlock_Speed_Button hooked at 0x%p!\n", pSpeed);
    }

    // client.dll + 0x0009A1C0: Interlock_Block_Button (+0x80, Block / Defense)
    LPVOID pBlock = reinterpret_cast<LPVOID>(clientBase + 0x0009A1C0);
    if (MH_CreateHook(pBlock, reinterpret_cast<LPVOID>(&Safe_Interlock_Block_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Block_Button)) == MH_OK) {
        MH_EnableHook(pBlock);
        Log("[mxohax] SUCCESS: client.dll Interlock_Block_Button hooked at 0x%p!\n", pBlock);
    }

    // client.dll + 0x0009A210: Interlock_Grab_Button (+0x74, Grab / Attack)
    LPVOID pGrab = reinterpret_cast<LPVOID>(clientBase + 0x0009A210);
    if (MH_CreateHook(pGrab, reinterpret_cast<LPVOID>(&Safe_Interlock_Grab_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Grab_Button)) == MH_OK) {
        MH_EnableHook(pGrab);
        Log("[mxohax] SUCCESS: client.dll Interlock_Grab_Button hooked at 0x%p!\n", pGrab);
    }

    // client.dll + 0x0009A260: Interlock_Power_Button (+0x70, Power)
    LPVOID pPower = reinterpret_cast<LPVOID>(clientBase + 0x0009A260);
    if (MH_CreateHook(pPower, reinterpret_cast<LPVOID>(&Safe_Interlock_Power_Button), reinterpret_cast<LPVOID*>(&Original_Interlock_Power_Button)) == MH_OK) {
        MH_EnableHook(pPower);
        Log("[mxohax] SUCCESS: client.dll Interlock_Power_Button hooked at 0x%p!\n", pPower);
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

    // Patch L1: 0x0051B1EE: 2 bytes NOP NOP (90 90 instead of 74 2C)
    // Prevents CRSIObject::EquipArticle from bailing out when [0x1099D1F4] is null,
    // allowing unconditional writes of article IDs and colors to pRSI
    LPVOID pEquipBypass = reinterpret_cast<LPVOID>(clientBase + 0x0051B1EE);
    if (VirtualProtect(pEquipBypass, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE nop2[2] = { 0x90, 0x90 };
        memcpy(pEquipBypass, nop2, 2);
        VirtualProtect(pEquipBypass, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pEquipBypass, 2);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0051B1EE (NOP * 2) to bypass EquipArticle bailout!\n");
    }

    // Native chase camera update preserved without disruptive epilogue jumps


    // Patch L6: 0x0051AFD4: 10 bytes NOPs (mov dword ptr [0x1099D1F4], 0 -> NOP * 10)
    // Prevents GetRSIDatabase from wiping the RSIDatabase instance pointer back to NULL!
    LPVOID pRsiDbWipePatch = reinterpret_cast<LPVOID>(clientBase + 0x0051AFD4);
    if (VirtualProtect(pRsiDbWipePatch, 10, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE nop10[10];
        memset(nop10, 0x90, 10);
        memcpy(pRsiDbWipePatch, nop10, 10);
        VirtualProtect(pRsiDbWipePatch, 10, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pRsiDbWipePatch, 10);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0051AFD4 (NOP * 10) to prevent RSIDatabase wipe!\n");
    }

    // Patch L7: 0x0052FD5C: 10 bytes NOPs (mov ecx, [esi+0x1DC] / cmp eax, [ecx] / je +0x36 -> NOP * 10)
    // Prevents access violation on NULL [esi+0x1DC] and ensures unconditional mesh attachment in 0x1052FD50!
    LPVOID pMeshAttachCheckPatch = reinterpret_cast<LPVOID>(clientBase + 0x0052FD5C);
    if (VirtualProtect(pMeshAttachCheckPatch, 10, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE nop10[10];
        memset(nop10, 0x90, 10);
        memcpy(pMeshAttachCheckPatch, nop10, 10);
        VirtualProtect(pMeshAttachCheckPatch, 10, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pMeshAttachCheckPatch, 10);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0052FD5C (NOP * 10) for safe unconditional mesh attachment!\n");
    }

    // Patch M: 0x000EAAC0: 3 bytes ret 0 (C2 00 00)
    // Permanently neutralizes client.dll exit/quit function so PostQuitMessage is never called
    LPVOID pClientQuitPatch = reinterpret_cast<LPVOID>(clientBase + 0x000EAAC0);
    if (VirtualProtect(pClientQuitPatch, 3, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE ret0[3] = { 0xC2, 0x00, 0x00 };
        memcpy(pClientQuitPatch, ret0, 3);
        VirtualProtect(pClientQuitPatch, 3, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pClientQuitPatch, 3);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x000EAAC0 (ret 0) to permanently neutralize client exit loop!\n");
    }

    // Neutralize active dragging and resizing globals
    *reinterpret_cast<DWORD*>(clientBase + 0x00849394) = 0xFFFFFFFF; // Active dragged control ID
    *reinterpret_cast<DWORD*>(clientBase + 0x00849390) = 0xFFFFFFFF; // Active resize mode
    *reinterpret_cast<DWORD*>(clientBase + 0x00899800) = 0;          // Drag offset X
    *reinterpret_cast<DWORD*>(clientBase + 0x00899804) = 0;          // Drag offset Y
    *reinterpret_cast<DWORD*>(clientBase + 0x00898C50) = 0;          // Captured control pointer
    *reinterpret_cast<DWORD*>(clientBase + 0x008997F8) = 0;          // Drag start X
    *reinterpret_cast<DWORD*>(clientBase + 0x008997FC) = 0;          // Drag start Y

    // Neutralize client.dll instructions that clobber CActor +0x4EE idle flag to 0
    // Real verified RVAs:
    // 0x001A4D25: C6 80 EE 04 00 00 00 (mov byte ptr [eax + 0x4EE], 0 in CActor::UpdateLocomotionState)
    // 0x004ECC32: C6 86 EE 04 00 00 00 (mov byte ptr [esi + 0x4EE], 0 in CActor::ProcessSamples)
    // 0x0019DFA0: C6 81 EE 04 00 00 00 (mov byte ptr [ecx + 0x4EE], 0 in CActor::SetNotIdle)
    static const DWORD s_idleClobberOffsets[] = { 0x001A4D25, 0x004ECC32, 0x0019DFA0 };
    for (DWORD off : s_idleClobberOffsets) {
        LPVOID pTarget = reinterpret_cast<LPVOID>(clientBase + off);
        DWORD oldProt = 0;
        if (VirtualProtect(pTarget, 7, PAGE_EXECUTE_READWRITE, &oldProt)) {
            memset(pTarget, 0x90, 7);
            VirtualProtect(pTarget, 7, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), pTarget, 7);
        }
    }
    Log("[mxohax] SUCCESS: Protected CActor +0x4EE idle flag across all locomotion updates (0x1A4D25, 0x4ECC32, 0x19DFA0)!\n");

    // Enforce in-world locomotion controller flag
    *reinterpret_cast<BYTE*>(clientBase + 0x0089DD5C) = 1;

    // In-world locomotion controller initialized cleanly
    Log("[mxohax] SUCCESS: Locomotion controller and authentic idle transitions initialized!\n");
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

// ============================================================================
// Epoch X: Modern Client Architecture, Flecs ECS & Kinematic Movement
// ============================================================================

class IClientSubsystem {
public:
    virtual ~IClientSubsystem() = default;
    virtual const char* GetSubsystemName() const = 0;
    virtual bool Initialize() = 0;
    virtual void Update(float deltaTimeSec) = 0;
    virtual void Shutdown() = 0;
};

struct EcsPositionComponent {
    float x{0.0f}, y{0.0f}, z{0.0f};
};

struct EcsVelocityComponent {
    float vx{0.0f}, vy{0.0f}, vz{0.0f};
};

struct EcsKinematicsComponent {
    bool isVaulting{false};
    bool isLedgeGrabbing{false};
    float vaultProgress{0.0f};
    float obstacleHeight{0.0f};
    float startY{0.0f};
    float targetY{0.0f};
};

struct EcsStanceComponent {
    int combatStance{0}; // 0: Neutral, 1: Power, 2: Speed, 3: Grab
    float stanceTransitionProgress{1.0f};
};

struct EcsCollisionBoxComponent {
    float halfWidth{15.0f};
    float halfHeight{35.0f};
    float halfDepth{15.0f};
};

#define ECS_MAX_ENTITIES 2048

class EntityComponentSystem : public IClientSubsystem {
public:
    uint32_t activeEntityCount{0};
    uint32_t entityIds[ECS_MAX_ENTITIES];
    EcsPositionComponent positions[ECS_MAX_ENTITIES];
    EcsVelocityComponent velocities[ECS_MAX_ENTITIES];
    EcsKinematicsComponent kinematics[ECS_MAX_ENTITIES];
    EcsStanceComponent stances[ECS_MAX_ENTITIES];
    EcsCollisionBoxComponent collisionBoxes[ECS_MAX_ENTITIES];

    const char* GetSubsystemName() const override { return "EntityComponentSystem"; }

    bool Initialize() override {
        activeEntityCount = 0;
        memset(entityIds, 0, sizeof(entityIds));
        memset(positions, 0, sizeof(positions));
        memset(velocities, 0, sizeof(velocities));
        memset(kinematics, 0, sizeof(kinematics));
        memset(stances, 0, sizeof(stances));
        memset(collisionBoxes, 0, sizeof(collisionBoxes));
        Log("[mxohax] EntityComponentSystem initialized with capacity %d entities.\n", ECS_MAX_ENTITIES);
        return true;
    }

    uint32_t RegisterEntity(uint32_t id, float x, float y, float z) {
        if (activeEntityCount >= ECS_MAX_ENTITIES) return 0xFFFFFFFF;
        uint32_t idx = activeEntityCount++;
        entityIds[idx] = id;
        positions[idx].x = x;
        positions[idx].y = y;
        positions[idx].z = z;
        velocities[idx].vx = 0.0f; velocities[idx].vy = 0.0f; velocities[idx].vz = 0.0f;
        kinematics[idx].isVaulting = false; kinematics[idx].isLedgeGrabbing = false;
        kinematics[idx].vaultProgress = 0.0f; kinematics[idx].obstacleHeight = 0.0f;
        kinematics[idx].startY = y; kinematics[idx].targetY = y;
        stances[idx].combatStance = 0; stances[idx].stanceTransitionProgress = 1.0f;
        collisionBoxes[idx].halfWidth = 15.0f; collisionBoxes[idx].halfHeight = 35.0f; collisionBoxes[idx].halfDepth = 15.0f;
        return idx;
    }

    void Update(float deltaTimeSec) override {
        for (uint32_t i = 0; i < activeEntityCount; ++i) {
            // Kinematic vault interpolation
            if (kinematics[i].isVaulting) {
                kinematics[i].vaultProgress += deltaTimeSec * 2.5f; // ~0.4s vault
                if (kinematics[i].vaultProgress >= 1.0f) {
                    kinematics[i].vaultProgress = 1.0f;
                    kinematics[i].isVaulting = false;
                    positions[i].y = kinematics[i].targetY;
                } else {
                    float t = kinematics[i].vaultProgress;
                    float smooth = t * t * (3.0f - 2.0f * t);
                    positions[i].y = kinematics[i].startY + (kinematics[i].targetY - kinematics[i].startY) * smooth;
                }
            } else {
                positions[i].x += velocities[i].vx * deltaTimeSec;
                positions[i].y += velocities[i].vy * deltaTimeSec;
                positions[i].z += velocities[i].vz * deltaTimeSec;
            }
        }
    }

    // Kinematic jump / vault obstacle detection (30 - 120 units high)
    bool TryInitiateVault(uint32_t entityIndex, float obstacleTopY) {
        if (entityIndex >= activeEntityCount) return false;
        float diffY = obstacleTopY - positions[entityIndex].y;
        if (diffY >= 30.0f && diffY <= 120.0f) {
            kinematics[entityIndex].isVaulting = true;
            kinematics[entityIndex].vaultProgress = 0.0f;
            kinematics[entityIndex].obstacleHeight = diffY;
            kinematics[entityIndex].startY = positions[entityIndex].y;
            kinematics[entityIndex].targetY = obstacleTopY;
            return true;
        }
        return false;
    }

    void Shutdown() override {
        activeEntityCount = 0;
    }
};

static EntityComponentSystem g_EcsSubsystem;

class SubsystemManager {
public:
    static const int MAX_SUBSYSTEMS = 16;
    IClientSubsystem* m_subsystems[MAX_SUBSYSTEMS];
    int m_count{0};

    void RegisterSubsystem(IClientSubsystem* sys) {
        if (m_count < MAX_SUBSYSTEMS && sys) {
            m_subsystems[m_count++] = sys;
        }
    }

    void InitializeAll() {
        for (int i = 0; i < m_count; ++i) {
            if (m_subsystems[i]) {
                m_subsystems[i]->Initialize();
            }
        }
    }

    void UpdateAll(float dt) {
        for (int i = 0; i < m_count; ++i) {
            if (m_subsystems[i]) {
                m_subsystems[i]->Update(dt);
            }
        }
    }

    void ShutdownAll() {
        for (int i = 0; i < m_count; ++i) {
            if (m_subsystems[i]) {
                m_subsystems[i]->Shutdown();
            }
        }
    }
};

static SubsystemManager g_SubsystemMgr;

static void InitializeMxOHaxSynchronous() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;
    g_SubsystemMgr.RegisterSubsystem(&g_EcsSubsystem);
    g_SubsystemMgr.InitializeAll();
    AddVectoredExceptionHandler(1, CrashHandler);
    Log("[mxohax] InitializeMxOHaxSynchronous started (CrashHandler registered)...\n");
    LoadTargetServerIp();

    // Initialize synthetic structures
    memset(&g_SyntheticCharRecord, 0, sizeof(g_SyntheticCharRecord));
    ParseClientCommandLine();
    g_SyntheticCharRecord.charIdLow = g_ActiveCharId;
    g_SyntheticCharRecord.charIdHigh = 0;
    g_SyntheticCharRecord.worldId = 1;

    memset(&g_SyntheticConnParams, 0, sizeof(g_SyntheticConnParams));
    g_SyntheticConnParams.worldId = 1;
    strcpy_s(g_SyntheticConnParams.serverIp, sizeof(g_SyntheticConnParams.serverIp), g_TargetServerIp);
    g_SyntheticConnParams.serverPort = 10000;

    memset(&g_SyntheticCharObj, 0, sizeof(g_SyntheticCharObj));
    g_SyntheticCharObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticCharObj.pRecord = &g_SyntheticCharRecord;
    g_SyntheticCharObj.pCharName = g_ActiveCharName;

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

    // 1a. Check if client.dll is ALREADY loaded into the process!
    // Critical: ZionLauncher and JackIn inject mxohax_modern.dll AFTER client.dll is already loaded.
    HMODULE hClientAlready = GetModuleHandleA("client.dll");
    if (hClientAlready) {
        Log("[mxohax] client.dll already loaded at 0x%p! Applying client patches immediately...\n", (void*)hClientAlready);
        ApplyClientPatches(hClientAlready);
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
        if (hClient) break;
        Sleep(20);
    }

    if (!hClient) return 0;
    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    ApplyClientPatches(hClient);
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

        g_SubsystemMgr.UpdateAll(0.1f);
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
        g_SubsystemMgr.ShutdownAll();
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("[mxohax] DLL_PROCESS_DETACH.\n");
    }
    return TRUE;
}

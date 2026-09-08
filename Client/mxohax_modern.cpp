#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
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

// Typedef for gethostbyname hook
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

// Typedef for connect hook
typedef int (PASCAL* connect_t)(SOCKET s, const struct sockaddr *name, int namelen);
static connect_t OriginalConnect = nullptr;

int PASCAL DetourConnect(SOCKET s, const struct sockaddr *name, int namelen) {
    if (name && name->sa_family == AF_INET && namelen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)name;
        u_short port = ntohs(sin->sin_port);
        // Redirect Auth (11000) and Margin (10000)
        if (port == 11000 || port == 10000) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            Log("[mxohax] connect() redirected port %u -> %s:%u\n", port, g_TargetServerIp, port);
            return OriginalConnect(s, (struct sockaddr*)&redirected, namelen);
        }
    }
    return OriginalConnect ? OriginalConnect(s, name, namelen) : SOCKET_ERROR;
}

// Typedef for sendto hook (UDP Margin/World on port 10000)
typedef int (PASCAL* sendto_t)(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
static sendto_t OriginalSendTo = nullptr;

int PASCAL DetourSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen) {
    if (to && to->sa_family == AF_INET && tolen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)to;
        u_short port = ntohs(sin->sin_port);
        if (port == 10000) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            Log("[mxohax] sendto() UDP redirected port 10000 -> %s:10000 (len=%d)\n", g_TargetServerIp, len);
            return OriginalSendTo(s, buf, len, flags, (struct sockaddr*)&redirected, tolen);
        }
    }
    return OriginalSendTo ? OriginalSendTo(s, buf, len, flags, to, tolen) : SOCKET_ERROR;
}

// Typedef for recvfrom hook
typedef int (PASCAL* recvfrom_t)(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
static recvfrom_t OriginalRecvFrom = nullptr;

int PASCAL DetourRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
    int res = OriginalRecvFrom ? OriginalRecvFrom(s, buf, len, flags, from, fromlen) : SOCKET_ERROR;
    if (res > 0 && from && fromlen && *fromlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in* sin = (struct sockaddr_in*)from;
        u_short port = ntohs(sin->sin_port);
        if (port == 10000) {
            Log("[mxohax] recvfrom() UDP received %d bytes from port %u\n", res, port);
        }
    }
    return res;
}

// Typedef for VerifyMessage function signature (CryptoPP PK_Verifier in matrix.exe / client.dll)
typedef bool(__stdcall* VerifyMessage_t)(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6);
static VerifyMessage_t OriginalVerifyMatrix = nullptr;
static VerifyMessage_t OriginalVerifyClient = nullptr;

bool __stdcall DetourVerifyMessage(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6) {
    Log("[mxohax] DetourVerifyMessage intercepted -> returning true (RSA verification bypassed)\n");
    return true;
}

// Hook for client.dll exported InitClientDLL
// Signature: int __cdecl InitClientDLL(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6, DWORD worldCharPacked, BOOL autoJackIn);
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
    DWORD forcedWorldCharPacked = (0 << 24) | (worldCharPacked & 0x00FFFFFF);
    BOOL forcedAutoJackIn = 1;

    Log("[mxohax] InitClientDLL intercepted! worldCharPacked=0x%08X -> 0x%08X, autoJackIn=%d -> %d\n",
        worldCharPacked, forcedWorldCharPacked, autoJackIn, forcedAutoJackIn);

    int res = OriginalInitClientDLL ? OriginalInitClientDLL(p1, p2, p3, p4, p5, p6, forcedWorldCharPacked, forcedAutoJackIn) : 1;
    Log("[mxohax] InitClientDLL returned %d\n", res);
    return res;
}

// Hook for EnterWorldWithCharacter (client.dll + 0x00124070)
typedef void (__thiscall *EnterWorld_t)(void* pMgr, void* pChar);
static EnterWorld_t OriginalEnterWorld = nullptr;

// Hook for client.dll State Machine Tick (client.dll + 0x00120060)
typedef int (__thiscall *StateTick_t)(void* pThis);
static StateTick_t OriginalStateTick = nullptr;
static int s_lastLoggedState = -1;
static bool s_autoJackInAttempted = false;
static int s_state1TickCount = 0;

static int __fastcall DetourStateTick(void* pThis, void* /*edx*/) {
    if (pThis) {
        int state = *reinterpret_cast<int*>(reinterpret_cast<DWORD>(pThis) + 0x1c);
        if (state != s_lastLoggedState) {
            s_lastLoggedState = state;
            Log("[mxohax] StateTick: State transitioned to %d\n", state);
        }

        // When in State 1 (Character Selection screen is fully active)
        if (state == 1 && !s_autoJackInAttempted) {
            s_state1TickCount++;
            // Wait ~20 ticks to ensure UI and character vector are populated
            if (s_state1TickCount >= 20) {
                HMODULE hClient = GetModuleHandleA("client.dll");
                if (hClient) {
                    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
                    DWORD pCharBegin = *reinterpret_cast<DWORD*>(clientBase + 0x00899B4C);
                    DWORD pCharEnd   = *reinterpret_cast<DWORD*>(clientBase + 0x00899B50);

                    if (pCharBegin && pCharEnd > pCharBegin) {
                        s_autoJackInAttempted = true;
                        DWORD numChars = (pCharEnd - pCharBegin) / 32;
                        void* pChar = reinterpret_cast<void*>(pCharBegin);
                        DWORD charId = *reinterpret_cast<DWORD*>(pChar);
                        Log("[mxohax] [AutoJackIn] %u operative(s) found! Selecting charId=%u at 0x%p...\n",
                            numChars, charId, pChar);

                        void* pMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
                        if (!pMgr) pMgr = pThis;

                        Log("[mxohax] [AutoJackIn] Calling EnterWorldWithCharacter(pMgr=0x%p, pChar=0x%p)...\n", pMgr, pChar);
                        EnterWorld_t pEnterWorld = reinterpret_cast<EnterWorld_t>(clientBase + 0x00124070);
                        pEnterWorld(pMgr, pChar);
                        Log("[mxohax] [AutoJackIn] EnterWorldWithCharacter dispatched successfully!\n");
                    } else {
                        Log("[mxohax] [AutoJackIn] Waiting for character vector to populate (begin=0x%08X, end=0x%08X)...\n",
                            pCharBegin, pCharEnd);
                    }
                }
            }
        }
    }
    return OriginalStateTick ? OriginalStateTick(pThis) : 1;
}

static void __fastcall DetourEnterWorldWithCharacter(void* pMgr, void* /*edx*/, void* pChar) {
    Log("[mxohax] EnterWorldWithCharacter invoked! pMgr=0x%p, pChar=0x%p\n", pMgr, pChar);
    if (OriginalEnterWorld) {
        OriginalEnterWorld(pMgr, pChar);
    }
    Log("[mxohax] EnterWorldWithCharacter completed.\n");
}

// Hook for SelectCharacter (client.dll + 0x00208230)
typedef int (__thiscall *SelectChar_t)(void* pMgr, void* pChar);
static SelectChar_t OriginalSelectChar = nullptr;

static int __fastcall DetourSelectCharacter(void* pMgr, void* /*edx*/, void* pChar) {
    Log("[mxohax] SelectCharacter invoked! pMgr=0x%p, pChar=0x%p\n", pMgr, pChar);
    int res = OriginalSelectChar ? OriginalSelectChar(pMgr, pChar) : 0;
    Log("[mxohax] SelectCharacter returned %d\n", res);
    return res;
}

// Hook for UI control visibility
typedef void (__thiscall *Control_t)(void* pUI, DWORD ctrlId);
static Control_t OriginalHideControl = nullptr;
static Control_t OriginalShowControl = nullptr;
static bool s_screen5DActive = false;
static bool s_autoJackInDone = false;
static int s_screen5DFrames = 0;

static bool TryAutoJackIn(DWORD clientBase) {
    if (!clientBase) return false;

    void* pCharMgr = *reinterpret_cast<void**>(clientBase + 0x00897FA4);
    if (!pCharMgr) return false;

    void** vtbl = *reinterpret_cast<void***>(pCharMgr);
    if (!vtbl) return false;

    typedef int (__thiscall *GetCount_t)(void* pThis);
    GetCount_t GetCharCount = reinterpret_cast<GetCount_t>(vtbl[0xC8 / 4]);
    int count = GetCharCount(pCharMgr);

    if (count > 0) {
        typedef void* (__thiscall *GetChar_t)(void* pThis, int idx);
        GetChar_t GetChar = reinterpret_cast<GetChar_t>(vtbl[0xCC / 4]);
        void* pChar = GetChar(pCharMgr, 0);
        Log("[mxohax] [AutoJackIn] CharacterMgr: %d operative(s) available. Retrieved operative #0 at 0x%p!\n", count, pChar);

        void* pMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
        if (!pMgr) pMgr = *reinterpret_cast<void**>(clientBase + 0x00896A38);

        if (pMgr && pChar) {
            Log("[mxohax] [AutoJackIn] Invoking EnterWorldWithCharacter(pMgr=0x%p, pChar=0x%p)...\n", pMgr, pChar);
            EnterWorld_t pEnterWorld = reinterpret_cast<EnterWorld_t>(clientBase + 0x00124070);
            pEnterWorld(pMgr, pChar);
            Log("[mxohax] [AutoJackIn] EnterWorldWithCharacter successfully dispatched!\n");
            return true;
        }
    }
    return false;
}

static void __fastcall DetourHideControl(void* pUI, void* /*edx*/, DWORD ctrlId) {
    if (ctrlId != 0x1A) {
        Log("[mxohax] HideControl: 0x%02X\n", ctrlId);
    }
    if (OriginalHideControl) OriginalHideControl(pUI, ctrlId);

    if (s_screen5DActive && !s_autoJackInDone) {
        s_screen5DFrames++;
        if (s_screen5DFrames >= 15) { // ~300ms after Screen 0x5D
            HMODULE hClient = GetModuleHandleA("client.dll");
            if (hClient) {
                if (TryAutoJackIn(reinterpret_cast<DWORD>(hClient))) {
                    s_autoJackInDone = true;
                }
            }
        }
    }
}

static void __fastcall DetourShowControl(void* pUI, void* /*edx*/, DWORD ctrlId) {
    Log("[mxohax] ShowControl: 0x%02X\n", ctrlId);
    if (ctrlId == 0x5D) {
        s_screen5DActive = true;
        s_screen5DFrames = 0;
        Log("[mxohax] Screen 0x5D (Character Selection) became active! AutoJackIn countdown engaged.\n");
    }
    if (OriginalShowControl) OriginalShowControl(pUI, ctrlId);
}

// Hook for RunClientDLL (called by matrix.exe every frame on the main thread!)
typedef int (__cdecl *RunClientDLL_t)();
static RunClientDLL_t OriginalRunClientDLL = nullptr;

int __cdecl DetourRunClientDLL() {
    if (s_screen5DActive && !s_autoJackInDone) {
        s_screen5DFrames++;
        if (s_screen5DFrames >= 30) {
            HMODULE hClient = GetModuleHandleA("client.dll");
            if (hClient) {
                DWORD clientBase = reinterpret_cast<DWORD>(hClient);
                DWORD pCharBegin = *reinterpret_cast<DWORD*>(clientBase + 0x00899B4C);
                DWORD pCharEnd   = *reinterpret_cast<DWORD*>(clientBase + 0x00899B50);

                if (pCharBegin && pCharEnd > pCharBegin) {
                    s_autoJackInDone = true;
                    DWORD numChars = (pCharEnd - pCharBegin) / 32;
                    void* pChar = reinterpret_cast<void*>(pCharBegin);
                    DWORD charId = *reinterpret_cast<DWORD*>(pChar);
                    Log("[mxohax] [AutoJackIn] RunClientDLL frame %d! Found %u character(s). Auto-jacking in with charId=%u at 0x%p...\n",
                        s_screen5DFrames, numChars, charId, pChar);

                    void* pMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
                    if (pMgr) {
                        Log("[mxohax] [AutoJackIn] Calling EnterWorldWithCharacter(pMgr=0x%p, pChar=0x%p)...\n", pMgr, pChar);
                        EnterWorld_t pEnterWorld = reinterpret_cast<EnterWorld_t>(clientBase + 0x00124070);
                        pEnterWorld(pMgr, pChar);
                        Log("[mxohax] [AutoJackIn] EnterWorldWithCharacter dispatched successfully!\n");
                    } else {
                        Log("[mxohax] [AutoJackIn] Error: pMgr is null\n");
                        s_autoJackInDone = false;
                    }
                }
            }
        }
    }
    return OriginalRunClientDLL ? OriginalRunClientDLL() : 1;
}

// Memory scanning utility for VerifyMessage
DWORD FindVerifyMessage(DWORD moduleBase) {
    DWORD address = 0;
    const byte functionStart[13] = {
        0x55, 0x8B, 0xEC, 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x57, 0xFF, 0x50, 0x1C
    };

    for (DWORD i = moduleBase; i < moduleBase + 0x1400000; ++i) {
        __try {
            if (memcmp(reinterpret_cast<byte*>(i), functionStart, sizeof(functionStart)) == 0) {
                address = i;
                break;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return address;
}

static bool g_ClientPatched = false;

static void ApplyClientPatches(HMODULE hClient) {
    if (!hClient || g_ClientPatched) return;
    g_ClientPatched = true;

    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    Log("[mxohax] Applying client.dll hooks at base 0x%p...\n", (void*)clientBase);

    // 1. Hook exported InitClientDLL in client.dll
    FARPROC pInitClientDLL = GetProcAddress(hClient, "InitClientDLL");
    if (pInitClientDLL) {
        if (MH_CreateHook(pInitClientDLL, &DetourInitClientDLL, reinterpret_cast<LPVOID*>(&OriginalInitClientDLL)) == MH_OK) {
            MH_EnableHook(pInitClientDLL);
            Log("[mxohax] SUCCESS: client.dll InitClientDLL hooked at 0x%p!\n", (void*)pInitClientDLL);
        }
    }

    // 1b. Hook exported RunClientDLL in client.dll (engine frame tick)
    FARPROC pRunClientDLL = GetProcAddress(hClient, "RunClientDLL");
    if (pRunClientDLL) {
        if (MH_CreateHook(pRunClientDLL, &DetourRunClientDLL, reinterpret_cast<LPVOID*>(&OriginalRunClientDLL)) == MH_OK) {
            MH_EnableHook(pRunClientDLL);
            Log("[mxohax] SUCCESS: client.dll RunClientDLL hooked at 0x%p!\n", (void*)pRunClientDLL);
        }
    }

    // 2. Scan and hook VerifyMessage in client.dll (RSA bypass)
    DWORD clientVerifyAddr = FindVerifyMessage(clientBase);
    if (clientVerifyAddr) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(clientVerifyAddr), &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyClient)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(clientVerifyAddr));
            Log("[mxohax] SUCCESS: client.dll VerifyMessage hooked at 0x%p! RSA verification bypassed.\n", (void*)clientVerifyAddr);
        }
    }

    // 3. Hook StateTick (client.dll + 0x00120060)
    LPVOID pStateTick = reinterpret_cast<LPVOID>(clientBase + 0x00120060);
    if (MH_CreateHook(pStateTick, &DetourStateTick, reinterpret_cast<LPVOID*>(&OriginalStateTick)) == MH_OK) {
        MH_EnableHook(pStateTick);
        Log("[mxohax] SUCCESS: client.dll StateTick hooked at 0x%p!\n", pStateTick);
    }

    // 4. Hook EnterWorldWithCharacter (client.dll + 0x00124070)
    LPVOID pEnterWorld = reinterpret_cast<LPVOID>(clientBase + 0x00124070);
    if (MH_CreateHook(pEnterWorld, &DetourEnterWorldWithCharacter, reinterpret_cast<LPVOID*>(&OriginalEnterWorld)) == MH_OK) {
        MH_EnableHook(pEnterWorld);
        Log("[mxohax] SUCCESS: client.dll EnterWorldWithCharacter hooked at 0x%p!\n", pEnterWorld);
    }

    // 5. Hook SelectCharacter (client.dll + 0x00208230)
    LPVOID pSelectChar = reinterpret_cast<LPVOID>(clientBase + 0x00208230);
    if (MH_CreateHook(pSelectChar, &DetourSelectCharacter, reinterpret_cast<LPVOID*>(&OriginalSelectChar)) == MH_OK) {
        MH_EnableHook(pSelectChar);
        Log("[mxohax] SUCCESS: client.dll SelectCharacter hooked at 0x%p!\n", pSelectChar);
    }

    // 6. Hook HideControl & ShowControl for UI flow tracing
    LPVOID pHideControl = reinterpret_cast<LPVOID>(clientBase + 0x0001D3C0);
    if (MH_CreateHook(pHideControl, &DetourHideControl, reinterpret_cast<LPVOID*>(&OriginalHideControl)) == MH_OK) {
        MH_EnableHook(pHideControl);
    }
    LPVOID pShowControl = reinterpret_cast<LPVOID>(clientBase + 0x0001BC10);
    if (MH_CreateHook(pShowControl, &DetourShowControl, reinterpret_cast<LPVOID*>(&OriginalShowControl)) == MH_OK) {
        MH_EnableHook(pShowControl);
    }

    // 7. AutoJackIn patch at client.dll + 0x0012196E:
    // Original: 0F B6 F3 (movzx esi, bl)
    // Patched:  31 F6 90 (xor esi, esi; nop)
    // esi = 0 (first operative). If count > 0, cmp esi, eax succeeds (0 < count) and takes
    // the native jb 0x101219b9 branch directly into EnterWorld / LoadSelectedOperative!
    LPVOID pAutoJackInPatch = reinterpret_cast<LPVOID>(clientBase + 0x0012196E);
    DWORD oldProt = 0;
    if (VirtualProtect(pAutoJackInPatch, 3, PAGE_EXECUTE_READWRITE, &oldProt)) {
        const BYTE patchBytes[3] = { 0x31, 0xF6, 0x90 };
        memcpy(pAutoJackInPatch, patchBytes, 3);
        VirtualProtect(pAutoJackInPatch, 3, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pAutoJackInPatch, 3);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0012196E to auto-select operative #0 (31 F6 90)!\n");
    }
}

// Hook LoadLibraryA to catch client.dll synchronously as soon as it loads!
typedef HMODULE (WINAPI* LoadLibraryA_t)(LPCSTR lpLibFileName);
static LoadLibraryA_t OriginalLoadLibraryA = nullptr;

HMODULE WINAPI DetourLoadLibraryA(LPCSTR lpLibFileName) {
    HMODULE hMod = OriginalLoadLibraryA(lpLibFileName);
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
    // Export ordinal 1 for static/dynamic import compatibility
}

static void InitializeMxOHaxSynchronous() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    Log("[mxohax] InitializeMxOHaxSynchronous started...\n");
    LoadTargetServerIp();

    if (MH_Initialize() != MH_OK) {
        Log("[mxohax] ERROR: Failed to initialize MinHook!\n");
        return;
    }

    // Preload wsock32.dll so API hooking succeeds for both WS2_32 and WSOCK32
    HMODULE hWSock32 = LoadLibraryA("wsock32.dll");
    Log("[mxohax] Preloaded wsock32.dll at 0x%p\n", (void*)hWSock32);

    // 1. Hook LoadLibraryA immediately to synchronously intercept client.dll
    LPVOID pTarget = nullptr;
    if (MH_CreateHookApiEx(L"kernel32.dll", "LoadLibraryA", (LPVOID)&DetourLoadLibraryA, (LPVOID*)&OriginalLoadLibraryA, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: kernel32.dll LoadLibraryA hooked at 0x%p\n", pTarget);
    }

    // 2. Hook WinSock functions across ws2_32.dll and wsock32.dll
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

    if (hWSock32) {
        if (MH_CreateHookApiEx(L"wsock32.dll", "gethostbyname", (LPVOID)&DetourGetHostByName, NULL, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
            Log("[mxohax] SUCCESS: wsock32.dll gethostbyname hooked at 0x%p\n", pTarget);
        }
        if (MH_CreateHookApiEx(L"wsock32.dll", "connect", (LPVOID)&DetourConnect, NULL, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
            Log("[mxohax] SUCCESS: wsock32.dll connect hooked at 0x%p\n", pTarget);
        }
        if (MH_CreateHookApiEx(L"wsock32.dll", "sendto", (LPVOID)&DetourSendTo, NULL, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
            Log("[mxohax] SUCCESS: wsock32.dll sendto hooked at 0x%p\n", pTarget);
        }
        if (MH_CreateHookApiEx(L"wsock32.dll", "recvfrom", (LPVOID)&DetourRecvFrom, NULL, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
            Log("[mxohax] SUCCESS: wsock32.dll recvfrom hooked at 0x%p\n", pTarget);
        }
    }

    MH_EnableHook(MH_ALL_HOOKS);

    // 3. Scan and hook VerifyMessage in matrix.exe main module immediately
    HMODULE hMatrix = GetModuleHandleA(NULL);
    if (hMatrix) {
        DWORD matrixVerifyAddr = FindVerifyMessage(reinterpret_cast<DWORD>(hMatrix));
        if (matrixVerifyAddr) {
            Log("[mxohax] Found VerifyMessage in matrix.exe at 0x%p. Hooking...\n", (void*)matrixVerifyAddr);
            if (MH_CreateHook(reinterpret_cast<LPVOID>(matrixVerifyAddr), &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyMatrix)) == MH_OK) {
                MH_EnableHook(reinterpret_cast<LPVOID>(matrixVerifyAddr));
                Log("[mxohax] SUCCESS: matrix.exe VerifyMessage hooked at 0x%p!\n", (void*)matrixVerifyAddr);
            }
        }
    }

    // 4. In case client.dll was already loaded before injection, patch immediately
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (hClient) {
        Log("[mxohax] client.dll already loaded at 0x%p, patching now...\n", (void*)hClient);
        ApplyClientPatches(hClient);
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
        Sleep(200);

        // Check if Screen 0x5D is active and we need to auto-jackin
        if (s_screen5DActive && !s_autoJackInDone) {
            if (TryAutoJackIn(clientBase)) {
                s_autoJackInDone = true;
            }
        }

        void* pShell = *reinterpret_cast<void**>(clientBase + 0x00896A38);
        if (pShell) {
            BYTE* pInWorld = reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pShell) + 0x20);
            if (pInWorld && *pInWorld == 1 && !inWorldLogged) {
                inWorldLogged = true;
                Log("[mxohax] *** IN-WORLD CONFIRMED: CClientShell::m_inWorld == 1! 3D game simulation loop active. ***\n");
            }
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

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
    FILE* f = fopen("mxohax.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
}

static void LoadTargetServerIp() {
    FILE* f = fopen("server_ip.txt", "r");
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
        Log("[mxohax] connect() intercepted port %u -> IP %s\n", port, inet_ntoa(sin->sin_addr));
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
            return OriginalSendTo(s, buf, len, flags, (struct sockaddr*)&redirected, tolen);
        }
    }
    return OriginalSendTo ? OriginalSendTo(s, buf, len, flags, to, tolen) : SOCKET_ERROR;
}

// Typedef for VerifyMessage function signature (6 DWORD parameters in client.dll / matrix.exe CryptoPP PK_Verifier, ret 0x18)
typedef bool(__stdcall* VerifyMessage_t)(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6);
static VerifyMessage_t OriginalVerifyMatrix = nullptr;
static VerifyMessage_t OriginalVerifyClient = nullptr;

bool __stdcall DetourVerifyMessage(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6) {
    Log("[mxohax] DetourVerifyMessage intercepted and returning true! (RSA verification bypassed)\n");
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
    Log("[mxohax] DetourInitClientDLL intercepted! worldCharPacked=0x%08X, autoJackIn=%d\n", worldCharPacked, autoJackIn);
    
    // Always force worldIndex = 0 ("Reality") and charIndex = 0 (first operative slot), autoJackIn = 1
    DWORD worldIndex = 0;
    DWORD charIndex = 0;
    worldCharPacked = (worldIndex << 24) | (charIndex & 0x00FFFFFF);
    autoJackIn = 1;

    Log("[mxohax] Corrected InitClientDLL parameters: worldCharPacked=0x%08X (world=0, char=0), autoJackIn=%d -> Auto-Jacking into MegaCity!\n",
        worldCharPacked, autoJackIn);

    return OriginalInitClientDLL(p1, p2, p3, p4, p5, p6, worldCharPacked, autoJackIn);
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

// Helper to write memory safely
static bool PatchMemory(LPVOID dest, const void* src, size_t size) {
    DWORD oldProtect;
    if (VirtualProtect(dest, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(dest, src, size);
        VirtualProtect(dest, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), dest, size);
        return true;
    }
    return false;
}

static bool g_ClientPatched = false;

static void ApplyClientPatches(HMODULE hClient) {
    if (!hClient || g_ClientPatched) return;
    g_ClientPatched = true;

    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    Log("[mxohax] Applying client.dll patches at base 0x%p...\n", (void*)clientBase);

    // 1. Hook exported InitClientDLL in client.dll
    FARPROC pInitClientDLL = GetProcAddress(hClient, "InitClientDLL");
    if (pInitClientDLL) {
        Log("[mxohax] Found InitClientDLL at 0x%p. Hooking...\n", (void*)pInitClientDLL);
        if (MH_CreateHook(pInitClientDLL, &DetourInitClientDLL, reinterpret_cast<LPVOID*>(&OriginalInitClientDLL)) == MH_OK) {
            MH_EnableHook(pInitClientDLL);
            Log("[mxohax] SUCCESS: client.dll InitClientDLL hooked at 0x%p!\n", (void*)pInitClientDLL);
        } else {
            Log("[mxohax] ERROR: Failed to hook client.dll InitClientDLL!\n");
        }
    } else {
        Log("[mxohax] ERROR: GetProcAddress(client.dll, InitClientDLL) failed!\n");
    }

    // 2. Scan and hook VerifyMessage in client.dll
    DWORD clientVerifyAddr = FindVerifyMessage(clientBase);
    if (clientVerifyAddr) {
        Log("[mxohax] Found VerifyMessage in client.dll at 0x%p. Hooking...\n", (void*)clientVerifyAddr);
        if (MH_CreateHook(reinterpret_cast<LPVOID>(clientVerifyAddr), &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyClient)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(clientVerifyAddr));
            Log("[mxohax] SUCCESS: client.dll VerifyMessage hooked at 0x%p! RSA verification bypassed.\n", (void*)clientVerifyAddr);
        }
    }

    // 3. Patch client.dll at clientBase + 0x00001586:
    // Original: 8B 45 24 8B 4D 20 (mov eax, [ebp+0x24]; mov ecx, [ebp+0x20])
    // Patch:    31 C0 40 31 C9 90 (xor eax, eax; inc eax; xor ecx, ecx; nop -> autoJackIn=1, worldCharPacked=0)
    LPVOID pInitParams = reinterpret_cast<LPVOID>(clientBase + 0x00001586);
    const byte patchInitParams[6] = { 0x31, 0xC0, 0x40, 0x31, 0xC9, 0x90 };
    if (PatchMemory(pInitParams, patchInitParams, sizeof(patchInitParams))) {
        Log("[mxohax] SUCCESS: Patched client.dll at 0x%p (InitClientDLL params -> autoJackIn=1, worldChar=0)\n", pInitParams);
    } else {
        Log("[mxohax] WARNING: Failed to patch client.dll InitClientDLL params at 0x%p\n", pInitParams);
    }

    // 4. Patch client.dll at clientBase + 0x00120081 (State 5 / Operative Creator Bypass):
    // Original: 0F 85 F9 00 00 00 (jne 0x10120180)
    // Patch:    E9 FA 00 00 00 90 (jmp 0x10120180; nop -> direct world load, completely skipping character creation)
    LPVOID pState5Bypass = reinterpret_cast<LPVOID>(clientBase + 0x00120081);
    const byte patchState5[6] = { 0xE9, 0xFA, 0x00, 0x00, 0x00, 0x90 };
    if (PatchMemory(pState5Bypass, patchState5, sizeof(patchState5))) {
        Log("[mxohax] SUCCESS: Patched client.dll at 0x%p (0F 85 -> E9 FA) to unconditionally bypass character creation into world loading!\n", pState5Bypass);
    } else {
        Log("[mxohax] WARNING: Failed to patch client.dll State 5 bypass at 0x%p\n", pState5Bypass);
    }

    // 5. Patch client.dll at clientBase + 0x00121950 (charIndex / worldIndex Zeroing):
    // Original: 8B DE 81 E6 FF FF FF 00 (mov ebx, esi; and esi, 0xffffff)
    // Patch:    31 F6 31 DB 90 90 90 90 (xor esi, esi; xor ebx, ebx; nop; nop; nop; nop)
    LPVOID pCharPackZero = reinterpret_cast<LPVOID>(clientBase + 0x00121950);
    const byte patchPackZero[8] = { 0x31, 0xF6, 0x31, 0xDB, 0x90, 0x90, 0x90, 0x90 };
    if (PatchMemory(pCharPackZero, patchPackZero, sizeof(patchPackZero))) {
        Log("[mxohax] SUCCESS: Patched client.dll at 0x%p (unpack zeroing -> charIndex=0, worldIndex=0)\n", pCharPackZero);
    } else {
        Log("[mxohax] WARNING: Failed to patch client.dll unpack zeroing at 0x%p\n", pCharPackZero);
    }

    // 6. Patch client.dll at clientBase + 0x0012196E (World Index Zeroing):
    // Original: 0F B6 F3 (movzx esi, bl)
    // Patch:    31 F6 90 (xor esi, esi; nop)
    LPVOID pWorldZero = reinterpret_cast<LPVOID>(clientBase + 0x0012196E);
    const byte patchWorldZero[3] = { 0x31, 0xF6, 0x90 };
    if (PatchMemory(pWorldZero, patchWorldZero, sizeof(patchWorldZero))) {
        Log("[mxohax] SUCCESS: Patched client.dll at 0x%p (movzx esi, bl -> xor esi, esi; nop)\n", pWorldZero);
    } else {
        Log("[mxohax] WARNING: Failed to patch client.dll world zeroing at 0x%p\n", pWorldZero);
    }

    // 7. Unconditional world selection bypass in client.dll:
    // At clientBase + 0x121979: patch '72 3E' (jb 0x101219b9) -> 'EB 3E' (jmp short 0x101219b9)
    LPVOID pJb = reinterpret_cast<LPVOID>(clientBase + 0x121979);
    const byte patchJmp[2] = { 0xEB, 0x3E };
    if (PatchMemory(pJb, patchJmp, sizeof(patchJmp))) {
        Log("[mxohax] SUCCESS: Patched client.dll at 0x%p (72 3E -> EB 3E) to unconditionally select World 0!\n", pJb);
    } else {
        Log("[mxohax] WARNING: Failed to patch client.dll jb at 0x%p\n", pJb);
    }

    // 8. AutoJackIn enforcement in client.dll:
    // At clientBase + 0x121A90: patch '74 0A' (je 0x10121a9c) -> '90 90' (nop nop)
    LPVOID pJe = reinterpret_cast<LPVOID>(clientBase + 0x121A90);
    const byte patchNop[2] = { 0x90, 0x90 };
    if (PatchMemory(pJe, patchNop, sizeof(patchNop))) {
        Log("[mxohax] SUCCESS: Patched client.dll at 0x%p (74 0A -> 90 90) to enforce autoJackIn=1 in WorldSessionManager!\n", pJe);
    } else {
        Log("[mxohax] WARNING: Failed to patch client.dll je at 0x%p\n", pJe);
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

    // 1. Hook LoadLibraryA immediately to synchronously intercept client.dll
    LPVOID pTarget = nullptr;
    if (MH_CreateHookApiEx(L"kernel32.dll", "LoadLibraryA", (LPVOID)&DetourLoadLibraryA, (LPVOID*)&OriginalLoadLibraryA, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] SUCCESS: kernel32.dll LoadLibraryA hooked at 0x%p\n", pTarget);
    }

    // 2. Hook WinSock functions immediately so ANY socket calls by matrix.exe or client.dll are redirected
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

        // Apply matrix.exe patches to ensure world index 0, char index 0, and autoJackIn = 1
        DWORD matrixBase = reinterpret_cast<DWORD>(hMatrix);

        // 3a. Set global variables in matrix.exe
        DWORD* pWorldIdx = reinterpret_cast<DWORD*>(matrixBase + (0x004B0398 - 0x00400000));
        DWORD* pCharIdx = reinterpret_cast<DWORD*>(matrixBase + (0x004B039C - 0x00400000));
        BYTE* pAutoJack = reinterpret_cast<BYTE*>(matrixBase + (0x004AFDA9 - 0x00400000));
        
        DWORD oldP;
        if (VirtualProtect(pWorldIdx, sizeof(DWORD) * 2, PAGE_READWRITE, &oldP)) {
            *pWorldIdx = 0;
            *pCharIdx = 0;
            VirtualProtect(pWorldIdx, sizeof(DWORD) * 2, oldP, &oldP);
            Log("[mxohax] matrix.exe globals initialized: worldIndex=0, charIndex=0\n");
        }
        if (VirtualProtect(pAutoJack, sizeof(BYTE), PAGE_READWRITE, &oldP)) {
            *pAutoJack = 1;
            VirtualProtect(pAutoJack, sizeof(BYTE), oldP, &oldP);
            Log("[mxohax] matrix.exe global initialized: autoJackIn=1\n");
        }

        // 3b. Patch 0x0040976C: mov eax, [ebx + 0xac] (8B 83 AC 00 00 00) -> xor eax, eax; nop; nop; nop; nop (31 C0 90 90 90 90)
        LPVOID pMovEax = reinterpret_cast<LPVOID>(matrixBase + (0x0040976C - 0x00400000));
        const byte patchEax[6] = { 0x31, 0xC0, 0x90, 0x90, 0x90, 0x90 };
        if (PatchMemory(pMovEax, patchEax, sizeof(patchEax))) {
            Log("[mxohax] SUCCESS: Patched matrix.exe at 0x%p to enforce charIndex=0\n", pMovEax);
        }

        // 3c. Patch 0x00409772: mov ecx, [ebx + 0xa8] (8B 8B A8 00 00 00) -> xor ecx, ecx; nop; nop; nop; nop (31 C9 90 90 90 90)
        LPVOID pMovEcx = reinterpret_cast<LPVOID>(matrixBase + (0x00409772 - 0x00400000));
        const byte patchEcx[6] = { 0x31, 0xC9, 0x90, 0x90, 0x90, 0x90 };
        if (PatchMemory(pMovEcx, patchEcx, sizeof(patchEcx))) {
            Log("[mxohax] SUCCESS: Patched matrix.exe at 0x%p to enforce worldIndex=0\n", pMovEcx);
        }

        // 3d. Patch 0x0040977A: mov dl, byte ptr [0x4afda9] (8A 15 A9 FD 4A 00) -> mov dl, 1; nop; nop; nop; nop (B2 01 90 90 90 90)
        LPVOID pMovDl = reinterpret_cast<LPVOID>(matrixBase + (0x0040977A - 0x00400000));
        const byte patchDl[6] = { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 };
        if (PatchMemory(pMovDl, patchDl, sizeof(patchDl))) {
            Log("[mxohax] SUCCESS: Patched matrix.exe at 0x%p to enforce autoJackIn=1\n", pMovDl);
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
    // Background polling fallback to ensure client.dll is never missed
    for (int i = 0; i < 500 && !g_ClientPatched; ++i) {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            ApplyClientPatches(hClient);
            break;
        }
        Sleep(10);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        Log("[mxohax] DLL_PROCESS_ATTACH (thread %u)...\n", GetCurrentThreadId());
        // Execute complete initialization synchronously before DllMain returns!
        InitializeMxOHaxSynchronous();
        // Also spawn worker as backup
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("[mxohax] DLL_PROCESS_DETACH.\n");
    }
    return TRUE;
}

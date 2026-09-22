#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "minhook/include/MinHook.h"

#pragma comment(lib, "ws2_32.lib")

static char g_TargetServerIp[64] = "15.204.82.250";
static uintptr_t g_clientBase = 0;
static uintptr_t g_matrixBase = 0;

static void Log(const char* fmt, ...) {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\mxohax.log", "a");
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
            bool isGameCrash = false;
            if (g_clientBase && (uintptr_t)addr >= g_clientBase && (uintptr_t)addr < g_clientBase + 0x1000000) isGameCrash = true;
            if (g_matrixBase && (uintptr_t)addr >= g_matrixBase && (uintptr_t)addr < g_matrixBase + 0x1000000) isGameCrash = true;
            
            if (!isGameCrash) return EXCEPTION_CONTINUE_SEARCH;
            
            Log("[mxohax] !!! CRASH RECOVERED: 0x%08X at 0x%p !!!\n", code, addr);
            if (ctx && ctx->Esp) {
                if (g_clientBase && (uintptr_t)addr >= g_clientBase + 0x0001BC10 && (uintptr_t)addr <= g_clientBase + 0x0001BC90) {
                    ctx->Eip = static_cast<DWORD>(g_clientBase + 0x0001BC28); return EXCEPTION_CONTINUE_EXECUTION;
                }
                if (g_clientBase && (uintptr_t)addr >= g_clientBase + 0x00015D60 && (uintptr_t)addr <= g_clientBase + 0x00015DA0) {
                    ctx->Eip = static_cast<DWORD>(g_clientBase + 0x00015DA1); return EXCEPTION_CONTINUE_EXECUTION;
                }
                if (g_clientBase && (uintptr_t)addr == g_clientBase + 0x0016D495) {
                    ctx->Eip = static_cast<DWORD>(g_clientBase + 0x0016D4DD); return EXCEPTION_CONTINUE_EXECUTION;
                }
                if (g_clientBase && (uintptr_t)addr == g_clientBase + 0x000A20E6) {
                    ctx->Eip = static_cast<DWORD>(g_clientBase + 0x000A2213); return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

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
        if (port == 10000 || port == 11000 || port == 80 || (ip && strcmp(ip, "127.0.0.1") == 0)) {
            struct sockaddr_in redirected = *sin;
            redirected.sin_addr.s_addr = inet_addr(g_TargetServerIp);
            Log("[mxohax] connect() redirected dest=%s:%u -> %s:%u\n", ip ? ip : "unknown", port, g_TargetServerIp, port);
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
            return OriginalSendTo ? OriginalSendTo(s, buf, len, flags, (struct sockaddr*)&redirected, tolen) : SOCKET_ERROR;
        }
    }
    return OriginalSendTo ? OriginalSendTo(s, buf, len, flags, to, tolen) : SOCKET_ERROR;
}

typedef bool(__stdcall* VerifyMessage_t)(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6);
static VerifyMessage_t OriginalVerifyMatrix = nullptr;
static VerifyMessage_t OriginalVerifyClient = nullptr;

bool __stdcall DetourVerifyMessage(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6) {
    return true; // Bypass RSA
}

static void LoadTargetServerIp() {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\server_ip.txt", "r");
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
}

DWORD WINAPI InitThread(LPVOID) {
    LoadTargetServerIp();
    Log("[mxohax] Clean DLL Injected. Target Server IP: %s\n", g_TargetServerIp);

    MH_Initialize();
    
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (hWs2) {
        MH_CreateHook(GetProcAddress(hWs2, "gethostbyname"), (LPVOID)&DetourGetHostByName, (LPVOID*)&OriginalGetHostByName);
        MH_CreateHook(GetProcAddress(hWs2, "connect"), (LPVOID)&DetourConnect, (LPVOID*)&OriginalConnect);
        MH_CreateHook(GetProcAddress(hWs2, "sendto"), (LPVOID)&DetourSendTo, (LPVOID*)&OriginalSendTo);
        MH_EnableHook(MH_ALL_HOOKS);
        Log("[mxohax] Winsock hooked.\n");
    }

    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("matrix.exe")) {
        Sleep(100);
    }

    g_clientBase = (uintptr_t)GetModuleHandleA("client.dll");
    g_matrixBase = (uintptr_t)GetModuleHandleA("matrix.exe");

    Log("[mxohax] clientBase: 0x%p, matrixBase: 0x%p\n", (void*)g_clientBase, (void*)g_matrixBase);

    uintptr_t matrixVerifyAddr = g_matrixBase + 0x000EE620;
    uintptr_t clientVerifyAddr = g_clientBase + 0x004F34A0;
    
    if (MH_CreateHook((LPVOID)matrixVerifyAddr, &DetourVerifyMessage, (LPVOID*)&OriginalVerifyMatrix) == MH_OK) {
        MH_EnableHook((LPVOID)matrixVerifyAddr);
    }
    if (MH_CreateHook((LPVOID)clientVerifyAddr, &DetourVerifyMessage, (LPVOID*)&OriginalVerifyClient) == MH_OK) {
        MH_EnableHook((LPVOID)clientVerifyAddr);
    }
    Log("[mxohax] RSA verification bypassed.\n");

    AddVectoredExceptionHandler(1, CrashHandler);
    Log("[mxohax] Vectored Exception Handler installed.\n");

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        DeleteFileA("E:\\Games\\The Matrix Online\\mxohax.log");
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}

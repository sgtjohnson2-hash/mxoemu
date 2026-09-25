#include "ProtocolHook.h"
#include "../D3D9Hook/D3D9Hook.h"
#include "../UIAnchorSystem/UIAnchorSystem.h"
#include "minhook/include/MinHook.h"
#include <cstdio>
#include <cstring>

// Extern frame tick detour declared in main module
extern void __fastcall DetourFrameTick(void* pThis, void* edx);

// ============================================================================
// Original Function Pointers
// ============================================================================

typedef struct hostent* (PASCAL* gethostbyname_t)(const char* name);
static gethostbyname_t OriginalGetHostByName = nullptr;

typedef int (PASCAL* connect_t)(SOCKET s, const struct sockaddr *name, int namelen);
static connect_t OriginalConnect = nullptr;

typedef int (PASCAL* sendto_t)(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
static sendto_t OriginalSendTo = nullptr;

typedef int (PASCAL* recvfrom_t)(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
static recvfrom_t OriginalRecvFrom = nullptr;

typedef bool (__stdcall* VerifyMessage_t)(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6);
static VerifyMessage_t OriginalVerifyMatrix = nullptr;
static VerifyMessage_t OriginalVerifyClient = nullptr;

typedef HMODULE (WINAPI* LoadLibraryA_t)(LPCSTR lpLibFileName);
static LoadLibraryA_t OriginalLoadLibraryA = nullptr;

typedef void (WINAPI* ExitProcess_t)(UINT uExitCode);
static ExitProcess_t OriginalExitProcess = nullptr;

typedef void (WINAPI* PostQuitMessage_t)(int nExitCode);
static PostQuitMessage_t OriginalPostQuitMessage = nullptr;

typedef int (__thiscall *GetCharacterCount_t)(void* pThis);
static GetCharacterCount_t OriginalGetCharacterCount = nullptr;

typedef void* (__thiscall *GetCharacterByIndex_t)(void* pThis, void* edx, int idx);
static GetCharacterByIndex_t OriginalGetCharacterByIndex = nullptr;

typedef void (__thiscall *SelectCharacterVtbl_t)(void* pThis, void* pArg);
static SelectCharacterVtbl_t OriginalSelectCharacterVtbl = nullptr;

typedef void (__thiscall *MarginSelectChar_t)(void* pMgr, uint32_t charIndex);
static MarginSelectChar_t OriginalMarginSelectChar = nullptr;

typedef void (__thiscall *ClearCharacters_t)(void* pThis);
static ClearCharacters_t OriginalClearCharacters = nullptr;

typedef void (__thiscall *ClearCharObjects_t)(void* pThis);
static ClearCharObjects_t OriginalClearCharObjects = nullptr;

typedef int (__cdecl *InitClientDLL_t)(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6, DWORD worldCharPacked, BOOL autoJackIn);
static InitClientDLL_t OriginalInitClientDLL = nullptr;

typedef void* (__cdecl *Refill_t)(size_t n);
static Refill_t OriginalRefill = nullptr;

typedef void (__cdecl *PoolFree_t)(void* p, size_t n);
static PoolFree_t OriginalPoolFree = nullptr;

typedef int (__thiscall *ParseSubpacket_t)(void* pThis, const byte* pData, int len);
static ParseSubpacket_t OriginalParseSubpacket = nullptr;

typedef void (__thiscall *ClientPlayChar_t)(void* pThis);
static ClientPlayChar_t OriginalClientPlayChar = nullptr;

typedef void (__thiscall *SelectCharacterInList_t)(void* pThis);
static SelectCharacterInList_t OriginalSelectCharacterInList = nullptr;

typedef int (__thiscall *CharCreateHandleMessage_t)(void* pThis, void* pMsg);
static CharCreateHandleMessage_t OriginalCharCreateHandleMessage = nullptr;

typedef unsigned char (__stdcall *GetPlayerActiveObject_t)(void** outObj, void** outSubObj);
static GetPlayerActiveObject_t OriginalGetPlayerActiveObject = nullptr;

typedef void (__thiscall *FrameTick_t)(void* pThis);
extern FrameTick_t OriginalFrameTick;

// ============================================================================
// Synthetic COM and Character Structures
// ============================================================================

static void DummyDestructor() {}

static void* g_SyntheticVtbl[64];
static MxoCharacterData g_SyntheticChar = {
    359, 0, 0, 0, 0,
    "S1acker", "",
    106, 103, 101, 1, 359
};
static MxoCharRecord g_SyntheticCharRecord;
static MxoConnParams g_SyntheticConnParams;
static MxoCharObj    g_SyntheticCharObj;
static MxoConnObj    g_SyntheticConnObj;

static void SetupSyntheticCharManager(DWORD pThisDword) {
    ParseClientCommandLine();

    for (int i = 0; i < 64; ++i) {
        g_SyntheticVtbl[i] = (void*)&DummyDestructor;
    }

    memset(&g_SyntheticCharRecord, 0, sizeof(g_SyntheticCharRecord));
    g_SyntheticCharRecord.charIdLow = g_ActiveCharId;
    g_SyntheticCharRecord.charIdHigh = 0;
    g_SyntheticCharRecord.worldId = 1;

    memset(&g_SyntheticConnParams, 0, sizeof(g_SyntheticConnParams));
    g_SyntheticConnParams.worldId = 1;
    strncpy_s(g_SyntheticConnParams.serverIp, sizeof(g_SyntheticConnParams.serverIp), g_TargetServerIp, _TRUNCATE);
    g_SyntheticConnParams.serverPort = 10000;

    g_SyntheticCharObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticCharObj.pRecord = &g_SyntheticCharRecord;
    g_SyntheticCharObj.pCharName = g_ActiveCharName;

    g_SyntheticConnObj.pVtbl = g_SyntheticVtbl;
    g_SyntheticConnObj.pConnParams = &g_SyntheticConnParams;

    g_SyntheticChar.charId = g_ActiveCharId;
    g_SyntheticChar.handle = g_ActiveCharId;
    strncpy_s(g_SyntheticChar.firstName, sizeof(g_SyntheticChar.firstName), g_ActiveCharName, _TRUNCATE);
    g_SyntheticChar.bodyType = 106;
    g_SyntheticChar.headType = 103;
    g_SyntheticChar.hairType = 101;

    *reinterpret_cast<BYTE*>(pThisDword + 0x640) = 1;
    *reinterpret_cast<void**>(pThisDword + 0x644) = &g_SyntheticCharObj;
    *reinterpret_cast<void**>(pThisDword + 0x658) = &g_SyntheticConnObj;
    *reinterpret_cast<BYTE*>(pThisDword + 0x66c) = 0;
    *reinterpret_cast<BYTE*>(pThisDword + 0x778) = 1;
    *reinterpret_cast<DWORD*>(pThisDword + 0x77c) = 1;
    memcpy(reinterpret_cast<void*>(pThisDword + 0x674), &g_SyntheticChar, sizeof(g_SyntheticChar));
}

// ============================================================================
// WinSock Hooks Implementation
// ============================================================================

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
// RSA Message Verification Bypass
// ============================================================================

bool __stdcall DetourVerifyMessage(void* /*p1*/, void* /*p2*/, void* /*p3*/, void* /*p4*/, void* /*p5*/, void* /*p6*/) {
    Log("[mxohax] DetourVerifyMessage intercepted -> returning true (RSA verification bypassed)\n");
    return true;
}

// ============================================================================
// Process Exit & Library Hooks
// ============================================================================

void WINAPI DetourExitProcess(UINT uExitCode) {
    Log("[mxohax] DetourExitProcess called (exitCode=%u)\n", uExitCode);
    if (OriginalExitProcess) OriginalExitProcess(uExitCode);
}

void WINAPI DetourPostQuitMessage(int nExitCode) {
    Log("[mxohax] DetourPostQuitMessage called (exitCode=%d)\n", nExitCode);
    if (OriginalPostQuitMessage) OriginalPostQuitMessage(nExitCode);
}

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
            MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9", (LPVOID)&DetourDirect3DCreate9, (LPVOID*)&OriginalDirect3DCreate9, &pTarget);
            if (pTarget) MH_EnableHook(pTarget);
            MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9Ex", (LPVOID)&DetourDirect3DCreate9Ex, (LPVOID*)&OriginalDirect3DCreate9Ex, &pTarget);
            if (pTarget) MH_EnableHook(pTarget);
        }
    }
    return hMod;
}

// ============================================================================
// Matrix Character Manager Hooks
// ============================================================================

int __fastcall DetourGetCharacterCount(void* pThis, void* /*edx*/) {
    if (!pThis) return 0;
    DWORD pThisDword = reinterpret_cast<DWORD>(pThis);

    BYTE realCount = *reinterpret_cast<BYTE*>(pThisDword + 0x640);
    uintptr_t pChar0 = *reinterpret_cast<uintptr_t*>(pThisDword + 0x644);
    if (realCount > 0 && pChar0 != 0 && pChar0 != reinterpret_cast<uintptr_t>(&g_SyntheticCharObj)) {
        return realCount;
    }

    if (g_AutoJackInRequested || g_CommandLineCharSpecified) {
        SetupSyntheticCharManager(pThisDword);
        Log("[mxohax] [matrix.exe] GetCharacterCount called -> operative %s (charId=%u)\n", g_ActiveCharName, g_ActiveCharId);
        return 1;
    }

    return realCount;
}

void* __fastcall DetourGetCharacterByIndex(void* pThis, void* /*edx*/, int idx) {
    if (!pThis || idx < 0 || idx >= 5) return nullptr;
    DWORD pThisDword = reinterpret_cast<DWORD>(pThis);

    BYTE realCount = *reinterpret_cast<BYTE*>(pThisDword + 0x640);
    uintptr_t pCharEntry = *reinterpret_cast<uintptr_t*>(pThisDword + 0x644 + idx * 4);
    if (realCount > 0 && pCharEntry != 0 && pCharEntry != reinterpret_cast<uintptr_t>(&g_SyntheticCharObj)) {
        uintptr_t pCharData = *reinterpret_cast<uintptr_t*>(pCharEntry + 0x14);
        if (pCharData) {
            return reinterpret_cast<void*>(pCharData);
        }
        return reinterpret_cast<void*>(pThisDword + 0x674 + idx * 0xAC);
    }

    if (idx == 0 && g_AutoJackInRequested && realCount == 0) {
        SetupSyntheticCharManager(pThisDword);
        return &g_SyntheticChar;
    }

    return nullptr;
}

void __fastcall DetourSelectCharacterVtbl(void* pThis, void* /*edx*/, void* pArg) {
    Log("[mxohax] [matrix.exe] SelectCharacter (vtable 0xDC) called with pArg=0x%p\n", pArg);
    DWORD pThisDword = reinterpret_cast<DWORD>(pThis);

    BYTE realCount = *reinterpret_cast<BYTE*>(pThisDword + 0x640);
    uintptr_t pChar0 = *reinterpret_cast<uintptr_t*>(pThisDword + 0x644);
    if (g_AutoJackInRequested && (realCount == 0 || pChar0 == 0 || pChar0 == reinterpret_cast<uintptr_t>(&g_SyntheticCharObj))) {
        SetupSyntheticCharManager(pThisDword);
    }

    if (pArg) {
        memcpy(reinterpret_cast<void*>(pThisDword + 0x674), reinterpret_cast<BYTE*>(pArg) + 4, 0xAC);
    }

    int curStateId = GetMarginStateId();
    Log("[mxohax] [matrix.exe] Current Margin State is %d\n", curStateId);

    if (curStateId < 4) {
        typedef void (__thiscall *TransitionToState_t)(void* pMgr, DWORD newStateId);
        TransitionToState_t Transition = reinterpret_cast<TransitionToState_t>(0x00428FF0);
        Log("[mxohax] [matrix.exe] Advancing Margin state from %d to 4 for character selection...\n", curStateId);
        Transition(pThis, 4);
    }
    if (OriginalSelectCharacterVtbl) {
        Log("[mxohax] [matrix.exe] Executing OriginalSelectCharacterVtbl...\n");
        OriginalSelectCharacterVtbl(pThis, pArg);
        Log("[mxohax] [matrix.exe] OriginalSelectCharacterVtbl finished successfully!\n");
    }
}

void __fastcall DetourMarginSelectChar(void* pMgr, void* /*edx*/, uint32_t charIndex) {
    Log("[mxohax] matrix.exe SelectCharacter(0x00429F20) entered with charIndex=%u!\n", charIndex);
    if (pMgr) {
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMgr) + 0x778) = 1;
        *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pMgr) + 0x77c) = 1;
    }
    if (OriginalMarginSelectChar) {
        OriginalMarginSelectChar(pMgr, charIndex);
    }
    if (pMgr) {
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMgr) + 0x778) = 1;
        *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pMgr) + 0x77c) = 1;
    }
    s_interactiveSelectTriggered = true;
    s_interactiveSelectTicks = 0;
}

void __fastcall DetourClearCharacters(void* pThis, void* /*edx*/) {
    Log("[mxohax] [matrix.exe] ClearCharacters (0x00428CC0) called on 0x%p\n", pThis);
    if (OriginalClearCharacters) {
        OriginalClearCharacters(pThis);
    }
}

void __fastcall DetourClearCharObjects(void* pThis, void* /*edx*/) {
    Log("[mxohax] [matrix.exe] ClearCharObjects (0x00428D10) called on 0x%p\n", pThis);
    if (OriginalClearCharObjects) {
        OriginalClearCharObjects(pThis);
    }
}

// ============================================================================
// Client DLL Hooks
// ============================================================================

int __cdecl DetourInitClientDLL(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6, DWORD worldCharPacked, BOOL autoJackIn) {
    Log("[mxohax] DetourInitClientDLL: worldCharPacked=0x%08X, autoJackIn=%d\n", worldCharPacked, autoJackIn);
    int res = OriginalInitClientDLL ? OriginalInitClientDLL(p1, p2, p3, p4, p5, p6, worldCharPacked, autoJackIn) : 1;
    Log("[mxohax] DetourInitClientDLL returned %d\n", res);
    if (res <= 0) {
        res = 1;
        Log("[mxohax] DetourInitClientDLL: Enforced return code 1 to invoke RunClientDLL!\n");
    }
    return res;
}

void* __cdecl DetourRefill(size_t n) {
    if (n == 0) n = 8;
    size_t rounded = (n + 7) & ~7;
    int nobjs = 64;

    char* chunk = reinterpret_cast<char*>(malloc(rounded * nobjs));
    if (!chunk) {
        chunk = reinterpret_cast<char*>(malloc(rounded));
        return chunk;
    }

    uintptr_t clientBase = GetSafeClientBase();
    if (clientBase && rounded <= 128) {
        size_t idx = (rounded - 1) >> 3;
        void** pFreeListHead = reinterpret_cast<void**>(clientBase + 0x00896C18 + idx * 4);
        void* result = chunk;
        void* curObj = chunk + rounded;
        *pFreeListHead = curObj;
        for (int i = 1; i < nobjs - 1; ++i) {
            void* nextObj = reinterpret_cast<char*>(curObj) + rounded;
            *reinterpret_cast<void**>(curObj) = nextObj;
            curObj = nextObj;
        }
        *reinterpret_cast<void**>(curObj) = nullptr;
        return result;
    }
    return chunk;
}

void __cdecl DetourPoolFree(void* p, size_t n) {
    if (!p) return;

    HMODULE hSelf = GetModuleHandleA("mxohax_modern.dll");
    uintptr_t selfBase = (uintptr_t)hSelf;
    if (selfBase && (uintptr_t)p >= selfBase && (uintptr_t)p < selfBase + 0x200000) {
        return;
    }

    uintptr_t ptrVal = reinterpret_cast<uintptr_t>(p);
    if (ptrVal < 0x10000 || ptrVal >= 0x7FFE0000) return;

    if (OriginalPoolFree) {
        __try {
            OriginalPoolFree(p, n);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            uintptr_t clientBase = GetSafeClientBase();
            if (clientBase) {
                *reinterpret_cast<DWORD*>(clientBase + 0x00896C10) = 0;
            }
        }
    }
}

int __fastcall DetourParseSubpacket(void* pThis, void* /*edx*/, const byte* pData, int len) {
    if (pData && len >= 4) {
        WORD count = *reinterpret_cast<const WORD*>(pData + 2);
        if (count > 256 || (len > 4 && (DWORD)count * 8 > (DWORD)len)) {
            Log("[mxohax] WARNING: ParseSubpacket invalid property count %u (len=%d)! Intercepting.\n", count, len);
            if (pData[0] == 0xCD && pData[1] == 0xAB && len >= 190) {
                return 190;
            }
            return 4;
        }
    }
    return OriginalParseSubpacket ? OriginalParseSubpacket(pThis, pData, len) : 0;
}

#pragma pack(push, 1)
struct MxoPlayVectorEntry {
    char* pWorldFirst;
    char* pWorldLast;
    char* pWorldEnd;
    char* pHandleFirst;
    char* pHandleLast;
    char* pHandleEnd;
    uint32_t charId;
    uint32_t worldId;
};
#pragma pack(pop)

static char s_playWorldPath[] = "resource/worlds/final_world/slums_barrens_full.metr";
static MxoPlayVectorEntry s_playEntries[5];

void __fastcall DetourClientPlayChar(void* pThis, void* /*edx*/) {
    Log("[mxohax] DetourClientPlayChar (0x1002AC10) triggered by Play click!\n");
    if (!pThis) return;

    int selectedIndex = 0;
    void* pPlayerList = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x6C);
    if (pPlayerList) {
        uintptr_t* listVtbl = *reinterpret_cast<uintptr_t**>(pPlayerList);
        if (listVtbl && listVtbl[0x170 / 4]) {
            typedef int (__thiscall *GetSelected_t)(void* pList);
            GetSelected_t pfnGetSelected = reinterpret_cast<GetSelected_t>(listVtbl[0x170 / 4]);
            int idx = pfnGetSelected(pPlayerList);
            if (idx >= 0 && idx < 5) selectedIndex = idx;
        }
    }
    Log("[mxohax] DetourClientPlayChar: Selected index in list: %d\n", selectedIndex);

    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (pMarginMgr) {
        uintptr_t pCharEntry = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x644 + selectedIndex * 4);
        if (pCharEntry && pCharEntry != reinterpret_cast<uintptr_t>(&g_SyntheticCharObj)) {
            uintptr_t pCharData = *reinterpret_cast<uintptr_t*>(pCharEntry + 0x14);
            if (pCharData) {
                const char* handle = reinterpret_cast<const char*>(pCharData + 3);
                if (handle && handle[0] != '\0' && strlen(handle) < 32) {
                    strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), handle, _TRUNCATE);
                    Log("[mxohax] DetourClientPlayChar: Active char name set to '%s'\n", g_ActiveCharName);
                }
            }
            uintptr_t pCharRec = *reinterpret_cast<uintptr_t*>(pCharEntry + 0x10);
            if (pCharRec) {
                uint32_t cId = *reinterpret_cast<uint32_t*>(pCharRec + 3);
                if (cId > 0) {
                    g_ActiveCharId = cId;
                    Log("[mxohax] DetourClientPlayChar: Active char ID set to %u\n", g_ActiveCharId);
                }
            }
        }

        MarginSelectChar_t pfnSelect = reinterpret_cast<MarginSelectChar_t>(0x00429F20);
        pfnSelect(pMarginMgr, selectedIndex);

        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x778) = 1;
        *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x77c) = 1;
    }

    uintptr_t clientBase = GetSafeClientBase();
    if (clientBase) {
        DWORD* ppCharBegin = reinterpret_cast<DWORD*>(clientBase + 0x00899B4C);
        DWORD* ppCharEnd   = reinterpret_cast<DWORD*>(clientBase + 0x00899B50);
        for (int i = 0; i <= selectedIndex && i < 5; ++i) {
            s_playEntries[i].pWorldFirst  = s_playWorldPath;
            s_playEntries[i].pWorldLast   = s_playWorldPath + strlen(s_playWorldPath);
            s_playEntries[i].pWorldEnd    = s_playEntries[i].pWorldLast;
            s_playEntries[i].pHandleFirst = g_ActiveCharName;
            s_playEntries[i].pHandleLast  = g_ActiveCharName + strlen(g_ActiveCharName);
            s_playEntries[i].pHandleEnd   = s_playEntries[i].pHandleLast;
            s_playEntries[i].charId       = g_ActiveCharId;
            s_playEntries[i].worldId      = 1;
        }
        if (ppCharBegin && ppCharEnd) {
            *ppCharBegin = reinterpret_cast<DWORD>(&s_playEntries[0]);
            *ppCharEnd   = reinterpret_cast<DWORD>(&s_playEntries[selectedIndex + 1]);
        }
    }

    if (OriginalClientPlayChar) {
        OriginalClientPlayChar(pThis);
    }
}

void __fastcall DetourSelectCharacterInList(void* pThis, void* /*edx*/) {
    Log("[mxohax] ViewPlayerSelection::SelectCharacterInList (0x100C2120) triggered!\n");
    if (OriginalSelectCharacterInList) {
        OriginalSelectCharacterInList(pThis);
    }

    uint32_t selectedIndex = 0;
    if (pThis) {
        void* pPlayerList = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pThis) + 0x64);
        if (pPlayerList) {
            uintptr_t* listVtbl = *reinterpret_cast<uintptr_t**>(pPlayerList);
            if (listVtbl && listVtbl[0x170 / 4]) {
                typedef int (__thiscall *GetSelected_t)(void* pList);
                GetSelected_t pfnGetSelected = reinterpret_cast<GetSelected_t>(listVtbl[0x170 / 4]);
                int idx = pfnGetSelected(pPlayerList);
                if (idx >= 0 && idx < 5) selectedIndex = static_cast<uint32_t>(idx);
            }
        }
    }

    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (pMarginMgr) {
        uintptr_t pCharEntry = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x644 + selectedIndex * 4);
        if (pCharEntry && pCharEntry != reinterpret_cast<uintptr_t>(&g_SyntheticCharObj)) {
            uintptr_t pCharData = *reinterpret_cast<uintptr_t*>(pCharEntry + 0x14);
            if (pCharData) {
                const char* handle = reinterpret_cast<const char*>(pCharData + 3);
                if (handle && handle[0] != '\0' && strlen(handle) < 32) {
                    strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), handle, _TRUNCATE);
                    Log("[mxohax] SelectCharacterInList: Selected char handle '%s' (index %u)\n", g_ActiveCharName, selectedIndex);
                }
            }
            uintptr_t pCharRec = *reinterpret_cast<uintptr_t*>(pCharEntry + 0x10);
            if (pCharRec) {
                uint32_t cId = *reinterpret_cast<uint32_t*>(pCharRec + 3);
                if (cId > 0) g_ActiveCharId = cId;
            }
        }
    }
}

int __fastcall DetourCharCreateHandleMessage(void* pThis, void* /*edx*/, void* pMsg) {
    if (pMsg) {
        DWORD eventCode = *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pMsg) + 8);
        if (eventCode == 0x31454843) { // 'CHE1' Finish button clicked!
            Log("[mxohax] CharCreate Finish button ('CHE1') clicked!\n");
            void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
            if (pMarginMgr) {
                int curMarginState = GetMarginStateId();
                if (curMarginState != 4) {
                    typedef void (__thiscall *TransitionToState_t)(void* pMgr, DWORD newStateId);
                    TransitionToState_t Transition = reinterpret_cast<TransitionToState_t>(0x00428FF0);
                    Transition(pMarginMgr, 4);
                }
                *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x778) = 1;
                *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x77c) = 1;
                const char* pMarginHandle = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0xFC);
                if (pMarginHandle && pMarginHandle[0] != '\0' && strlen(pMarginHandle) < 32) {
                    strncpy_s(g_ActiveCharName, sizeof(g_ActiveCharName), pMarginHandle, _TRUNCATE);
                    Log("[mxohax] CharCreate Finish: Captured character handle '%s' from MarginMgr!\n", g_ActiveCharName);
                }
            }
            uintptr_t clientBase = GetSafeClientBase();
            if (clientBase) {
                *reinterpret_cast<BYTE*>(clientBase + 0x00899EE0) = 1;
            }
            s_screen5DEverOpened = true;
            s_charCreationSubmitted = true;
            s_charCreationTicks = 0;
        }
    }
    return OriginalCharCreateHandleMessage ? OriginalCharCreateHandleMessage(pThis, pMsg) : 0;
}

unsigned char __stdcall Safe_GetPlayerActiveObject(void** outObj, void** outSubObj) {
    if (outObj) *outObj = nullptr;
    if (outSubObj) *outSubObj = nullptr;

    __try {
        uintptr_t clientBase = GetSafeClientBase();
        if (!clientBase) return 0;

        void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
        uintptr_t* ppGlobal = reinterpret_cast<uintptr_t*>(clientBase + 0x008A4378);
        if (!ppGlobal || IsBadReadPtr(ppGlobal, sizeof(uintptr_t)) || !*ppGlobal) return 0;
        uintptr_t pPlayer = *ppGlobal;

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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// ============================================================================
// Auto-Jackin Engine Routine
// ============================================================================

bool TryAutoJackIn(uintptr_t clientBase) {
    if (!clientBase) return false;

    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (!pWorldMgr) return false;

    DWORD curWorldState = *reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
    if (curWorldState >= 2) {
        Log("[mxohax] [AutoJackIn] WorldMgr is already in state %u! Skipping redundant EnterWorldWithCharacter.\n", curWorldState);
        return true;
    }

    void* pUI = GetCUIPointer(clientBase);
    if (pUI) {
        SafeSetControlVisible(clientBase, pUI, 0x30, false);
        SafeSetControlVisible(clientBase, pUI, 0x4B, false);
        SafeSetControlVisible(clientBase, pUI, 0x5D, false);
        SafeHideControl(clientBase, pUI, 0x30);
        SafeHideControl(clientBase, pUI, 0x4B);
        SafeHideControl(clientBase, pUI, 0x5D);
        Log("[mxohax] [AutoJackIn] Dismissed Screens 0x30, 0x4B, and 0x5D\n");

        typedef void* (__thiscall *CreateControl_t)(void* pUI, DWORD ctrlId);
        CreateControl_t pCreateControl = reinterpret_cast<CreateControl_t>(clientBase + 0x0001BC10);
        __try {
            pCreateControl(pUI, 0x57);
            SafeSetControlVisible(clientBase, pUI, 0x57, 1);
            Log("[mxohax] [AutoJackIn] Displayed authentic Phase 1 2D Loading Screen (0x57)!\n");
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    DWORD pNetClient = *reinterpret_cast<DWORD*>(clientBase + 0x0089BBA0);
    if (pNetClient) {
        *reinterpret_cast<DWORD*>(pNetClient + 0x08) = 2; // m_state = CONNECTED
        Log("[mxohax] [AutoJackIn] Ensured CNetClient at 0x%08X (m_state=2 CONNECTED)\n", pNetClient);
    }

    *reinterpret_cast<BYTE*>(clientBase + 0x0089DD5D) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x25) = 1;

    if (curWorldState < 2) {
        typedef char (__thiscall *AdvanceState_t)(void* pMgr);
        AdvanceState_t pAdvance = reinterpret_cast<AdvanceState_t>(clientBase + 0x00120060);
        pAdvance(pWorldMgr);
        curWorldState = *reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        Log("[mxohax] [AutoJackIn] AdvanceState(0x10120060) dispatched! pWorldMgr State is now %u\n", curWorldState);
    }

    *reinterpret_cast<BYTE*>(clientBase + 0x0089DD5D) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x25) = 1;

    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (pMarginMgr) {
        int curMarginState = GetMarginStateId();
        BYTE realCount = *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x640);
        uintptr_t pChar0 = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x644);

        if (curMarginState == 4) {
            MarginSelectChar_t pfnSelect = reinterpret_cast<MarginSelectChar_t>(0x00429F20);
            pfnSelect(pMarginMgr, 0);
        }

        curMarginState = GetMarginStateId();
        if (curMarginState != 9) {
            if (realCount == 0 || pChar0 == 0 || pChar0 == reinterpret_cast<uintptr_t>(&g_SyntheticCharObj)) {
                DWORD pThisDword = reinterpret_cast<DWORD>(pMarginMgr);
                SetupSyntheticCharManager(pThisDword);
            }
            typedef void (__thiscall *TransitionToState_t)(void* pMgr, DWORD newStateId);
            TransitionToState_t Transition = reinterpret_cast<TransitionToState_t>(0x00428FF0);
            Transition(pMarginMgr, 9);
        }
        *reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x778) = 1;
        *reinterpret_cast<DWORD*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x77c) = 1;
    }

    #pragma pack(push, 1)
    struct MxoLocalCharEntry {
        char* pWorldFirst;
        char* pWorldLast;
        char* pWorldEnd;
        char* pHandleFirst;
        char* pHandleLast;
        char* pHandleEnd;
        uint32_t charId;
        uint32_t worldId;
    };
    #pragma pack(pop)

    static MxoLocalCharEntry s_localCharEntry;
    static uint32_t s_mountedCharId = 0;
    static char s_mountedCharName[64] = {0};

    uint32_t targetCharId = g_ActiveCharId ? g_ActiveCharId : 360;
    const char* charName = (g_ActiveCharName[0] != '\0') ? g_ActiveCharName : "Slacker";

    if (s_mountedCharId != targetCharId || strcmp(s_mountedCharName, charName) != 0) {
        memset(&s_localCharEntry, 0, sizeof(s_localCharEntry));
        typedef void* (__thiscall *StringCtor_t)(void* pString, const char* str, char dummy);
        StringCtor_t pStringCtor = reinterpret_cast<StringCtor_t>(clientBase + 0x00001EF0);

        const char* worldMetrPath = "resource/worlds/final_world/slums_barrens_full.metr";
        pStringCtor(&s_localCharEntry.pWorldFirst, worldMetrPath, 0);
        pStringCtor(&s_localCharEntry.pHandleFirst, charName, 0);
        s_localCharEntry.charId  = targetCharId;
        s_localCharEntry.worldId = 1;
        s_mountedCharId = targetCharId;
        strncpy_s(s_mountedCharName, sizeof(s_mountedCharName), charName, _TRUNCATE);
    }

    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_localCharEntry.pWorldFirst;

    DWORD* ppCharBegin = reinterpret_cast<DWORD*>(clientBase + 0x00899B4C);
    DWORD* ppCharEnd   = reinterpret_cast<DWORD*>(clientBase + 0x00899B50);
    if (ppCharBegin && ppCharEnd) {
        *ppCharBegin = reinterpret_cast<DWORD>(&s_localCharEntry);
        *ppCharEnd   = reinterpret_cast<DWORD>(&s_localCharEntry) + sizeof(s_localCharEntry);
    }

    void* pCharToEnter = &s_localCharEntry;
    if (pCharToEnter) {
        typedef void (__thiscall *EnterWorld_t)(void* pMgr, void* pChar);
        EnterWorld_t pEnterWorld = reinterpret_cast<EnterWorld_t>(clientBase + 0x00124070);
        __try {
            pEnterWorld(pWorldMgr, pCharToEnter);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        DWORD* pCurState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
        if (pCurState && *pCurState < 4) {
            *pCurState = 4;
        }
    }

    return true;
}

// ============================================================================
// Apply Client Patches
// ============================================================================

void ApplyClientPatches(HMODULE hClient) {
    static LONG s_patchLock = 0;
    if (InterlockedCompareExchange(&s_patchLock, 1, 0) != 0 || !hClient) return;

    DWORD clientBase = reinterpret_cast<DWORD>(hClient);
    g_clientBase = clientBase;
    Log("[mxohax] Applying client.dll hooks at base 0x%p...\n", (void*)clientBase);

    LPVOID pParseSub = reinterpret_cast<LPVOID>(clientBase + 0x00255710);
    if (MH_CreateHook(pParseSub, &DetourParseSubpacket, reinterpret_cast<LPVOID*>(&OriginalParseSubpacket)) == MH_OK) {
        MH_EnableHook(pParseSub);
    }

    LPVOID pRefill = reinterpret_cast<LPVOID>(clientBase + 0x00001B50);
    if (MH_CreateHook(pRefill, &DetourRefill, reinterpret_cast<LPVOID*>(&OriginalRefill)) == MH_OK) {
        MH_EnableHook(pRefill);
    }

    LPVOID pPoolFree = reinterpret_cast<LPVOID>(clientBase + 0x00001BC0);
    if (MH_CreateHook(pPoolFree, &DetourPoolFree, reinterpret_cast<LPVOID*>(&OriginalPoolFree)) == MH_OK) {
        MH_EnableHook(pPoolFree);
    }

    LPVOID pInitClient = reinterpret_cast<LPVOID>(clientBase + 0x00001270);
    if (MH_CreateHook(pInitClient, &DetourInitClientDLL, reinterpret_cast<LPVOID*>(&OriginalInitClientDLL)) == MH_OK) {
        MH_EnableHook(pInitClient);
    }

    uintptr_t clientVerifyAddr = clientBase + 0x0047E010;
    if (MH_CreateHook(reinterpret_cast<LPVOID>(clientVerifyAddr), &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyClient)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(clientVerifyAddr));
    }

    LPVOID pFrameTick = reinterpret_cast<LPVOID>(clientBase + 0x001F9140);
    if (MH_CreateHook(pFrameTick, &DetourFrameTick, reinterpret_cast<LPVOID*>(&OriginalFrameTick)) == MH_OK) {
        MH_EnableHook(pFrameTick);
    }

    LPVOID pSetCtrlVis = reinterpret_cast<LPVOID>(clientBase + 0x0001DB80);
    if (MH_CreateHook(pSetCtrlVis, &DetourSetControlVisible, reinterpret_cast<LPVOID*>(&OriginalSetControlVisible)) == MH_OK) {
        MH_EnableHook(pSetCtrlVis);
    }

    LPVOID pClientPlay = reinterpret_cast<LPVOID>(clientBase + 0x0002AC10);
    if (MH_CreateHook(pClientPlay, &DetourClientPlayChar, reinterpret_cast<LPVOID*>(&OriginalClientPlayChar)) == MH_OK) {
        MH_EnableHook(pClientPlay);
    }

    LPVOID pSelectInList = reinterpret_cast<LPVOID>(clientBase + 0x000C2120);
    if (MH_CreateHook(pSelectInList, &DetourSelectCharacterInList, reinterpret_cast<LPVOID*>(&OriginalSelectCharacterInList)) == MH_OK) {
        MH_EnableHook(pSelectInList);
    }

    LPVOID pCharCreateMsg = reinterpret_cast<LPVOID>(clientBase + 0x0004AE30);
    if (MH_CreateHook(pCharCreateMsg, &DetourCharCreateHandleMessage, reinterpret_cast<LPVOID*>(&OriginalCharCreateHandleMessage)) == MH_OK) {
        MH_EnableHook(pCharCreateMsg);
    }

    LPVOID pSetCtrlPos = reinterpret_cast<LPVOID>(clientBase + 0x00015D60);
    if (MH_CreateHook(pSetCtrlPos, &DetourSetControlPos, reinterpret_cast<LPVOID*>(&OriginalSetControlPos)) == MH_OK) {
        MH_EnableHook(pSetCtrlPos);
    }

    LPVOID pSetWidgetPos = reinterpret_cast<LPVOID>(clientBase + 0x00382360);
    if (MH_CreateHook(pSetWidgetPos, &DetourWidgetSetPosition, reinterpret_cast<LPVOID*>(&OriginalWidgetSetPosition)) == MH_OK) {
        MH_EnableHook(pSetWidgetPos);
    }

    LPVOID pStartDrag = reinterpret_cast<LPVOID>(clientBase + 0x000184E0);
    if (MH_CreateHook(pStartDrag, &DetourBeginDrag, reinterpret_cast<LPVOID*>(&OriginalBeginDrag)) == MH_OK) {
        MH_EnableHook(pStartDrag);
    }

    LPVOID pOnDragMove = reinterpret_cast<LPVOID>(clientBase + 0x00018540);
    if (MH_CreateHook(pOnDragMove, &DetourOnDragMove, reinterpret_cast<LPVOID*>(&OriginalOnDragMove)) == MH_OK) {
        MH_EnableHook(pOnDragMove);
    }

    LPVOID pStartResize = reinterpret_cast<LPVOID>(clientBase + 0x00018590);
    if (MH_CreateHook(pStartResize, &DetourBeginResize, reinterpret_cast<LPVOID*>(&OriginalBeginResize)) == MH_OK) {
        MH_EnableHook(pStartResize);
    }

    LPVOID pOnResizeMove = reinterpret_cast<LPVOID>(clientBase + 0x000187B0);
    if (MH_CreateHook(pOnResizeMove, &DetourOnResizeMove, reinterpret_cast<LPVOID*>(&OriginalOnResizeMove)) == MH_OK) {
        MH_EnableHook(pOnResizeMove);
    }

    LPVOID pGetActiveObj = reinterpret_cast<LPVOID>(clientBase + 0x0010A210);
    if (MH_CreateHook(pGetActiveObj, &Safe_GetPlayerActiveObject, reinterpret_cast<LPVOID*>(&OriginalGetPlayerActiveObject)) == MH_OK) {
        MH_EnableHook(pGetActiveObj);
    }

    // Neutralize CActor::HideLocalPlayer entry (0x104ECDB0) with ret 8 (C2 08 00)
    LPVOID pHideActorEntry = reinterpret_cast<LPVOID>(clientBase + 0x004ECDB0);
    DWORD oldProtHideL = 0;
    if (VirtualProtect(pHideActorEntry, 3, PAGE_EXECUTE_READWRITE, &oldProtHideL)) {
        BYTE ret8[3] = { 0xC2, 0x08, 0x00 };
        memcpy(pHideActorEntry, ret8, 3);
        VirtualProtect(pHideActorEntry, 3, oldProtHideL, &oldProtHideL);
        FlushInstructionCache(GetCurrentProcess(), pHideActorEntry, 3);
    }

    // Neutralize CActor::ShowLocalPlayer visibility load (clientBase + 0x004EABCF)
    LPVOID pShowVisLoad = reinterpret_cast<LPVOID>(clientBase + 0x004EABCF);
    DWORD oldProtShowVis = 0;
    if (VirtualProtect(pShowVisLoad, 6, PAGE_EXECUTE_READWRITE, &oldProtShowVis)) {
        BYTE patchShowVis[6] = { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 };
        memcpy(pShowVisLoad, patchShowVis, 6);
        VirtualProtect(pShowVisLoad, 6, oldProtShowVis, &oldProtShowVis);
        FlushInstructionCache(GetCurrentProcess(), pShowVisLoad, 6);
    }

    *reinterpret_cast<DWORD*>(clientBase + 0x008971C8) = 1; // Enforce Third Person Chase Camera Mode CVar

    static const char s_initMetrPath[] = "resource/worlds/final_world/slums_barrens_full.metr";
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_initMetrPath;

    // Patch A: 0x0012196E routing for authentic character select vs create
    LPVOID pCharSelectPatch = reinterpret_cast<LPVOID>(clientBase + 0x0012196E);
    DWORD oldProtA = 0;
    if (VirtualProtect(pCharSelectPatch, 13, PAGE_EXECUTE_READWRITE, &oldProtA)) {
        static const BYTE patchA[13] = {
            0x31, 0xF6,
            0xFF, 0x90, 0xC8, 0x00, 0x00, 0x00,
            0x85, 0xC0,
            0x75, 0x3F,
            0x90
        };
        memcpy(pCharSelectPatch, patchA, 13);
        VirtualProtect(pCharSelectPatch, 13, oldProtA, &oldProtA);
        FlushInstructionCache(GetCurrentProcess(), pCharSelectPatch, 13);
    }

    // Patch J: 0x0012B388 (EB 09) LeaveWorld bypass
    DWORD oldProt = 0;
    LPVOID pLeaveWorldBypass = reinterpret_cast<LPVOID>(clientBase + 0x0012B388);
    if (VirtualProtect(pLeaveWorldBypass, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patchJ[2] = { 0xEB, 0x09 };
        memcpy(pLeaveWorldBypass, patchJ, 2);
        VirtualProtect(pLeaveWorldBypass, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pLeaveWorldBypass, 2);
    }

    // Patch daylight sky pane color at clientBase + 0x003F16A8
    LPVOID pDaylightSkyPaneColor = reinterpret_cast<LPVOID>(clientBase + 0x003F16A8);
    if (VirtualProtect(pDaylightSkyPaneColor, 7, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE noirGreen[7] = { 0xC7, 0x45, 0xFC, 0x14, 0x22, 0x14, 0x00 };
        memcpy(pDaylightSkyPaneColor, noirGreen, 7);
        VirtualProtect(pDaylightSkyPaneColor, 7, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pDaylightSkyPaneColor, 7);
    }

    NeutralizeDragGlobals(clientBase);
    *reinterpret_cast<BYTE*>(clientBase + 0x0089DD5C) = 1;
}

// ============================================================================
// Subsystem Implementation
// ============================================================================

ProtocolHookSubsystem g_ProtocolHook;

bool ProtocolHookSubsystem::Initialize(uintptr_t /*clientBase*/) {
    Log("[mxohax] ProtocolHookSubsystem::Initialize\n");
    LoadTargetServerIp();
    ParseClientCommandLine();

    if (MH_Initialize() != MH_OK) {
        Log("[mxohax] ERROR: Failed to initialize MinHook!\n");
        return false;
    }

    LPVOID pTarget = nullptr;
    if (MH_CreateHookApiEx(L"kernel32.dll", "LoadLibraryA", (LPVOID)&DetourLoadLibraryA, (LPVOID*)&OriginalLoadLibraryA, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }

    HMODULE hClientAlready = GetModuleHandleA("client.dll");
    if (hClientAlready) {
        ApplyClientPatches(hClientAlready);
    }

    // Hook Direct3D 9
    HMODULE hD3D9 = LoadLibraryA("d3d9.dll");
    if (hD3D9) {
        if (MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9", (LPVOID)&DetourDirect3DCreate9, (LPVOID*)&OriginalDirect3DCreate9, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
        }
        if (MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9Ex", (LPVOID)&DetourDirect3DCreate9Ex, (LPVOID*)&OriginalDirect3DCreate9Ex, &pTarget) == MH_OK) {
            MH_EnableHook(pTarget);
        }
    }

    // Hook WinSock
    if (MH_CreateHookApiEx(L"ws2_32.dll", "gethostbyname", (LPVOID)&DetourGetHostByName, (LPVOID*)&OriginalGetHostByName, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }
    if (MH_CreateHookApiEx(L"ws2_32.dll", "connect", (LPVOID)&DetourConnect, (LPVOID*)&OriginalConnect, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }
    if (MH_CreateHookApiEx(L"ws2_32.dll", "sendto", (LPVOID)&DetourSendTo, (LPVOID*)&OriginalSendTo, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }
    if (MH_CreateHookApiEx(L"ws2_32.dll", "recvfrom", (LPVOID)&DetourRecvFrom, (LPVOID*)&OriginalRecvFrom, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }

    // Process Exit
    if (MH_CreateHookApiEx(L"kernel32.dll", "ExitProcess", (LPVOID)&DetourExitProcess, (LPVOID*)&OriginalExitProcess, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }
    if (MH_CreateHookApiEx(L"user32.dll", "PostQuitMessage", (LPVOID)&DetourPostQuitMessage, (LPVOID*)&OriginalPostQuitMessage, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
    }

    // Hook matrix.exe RSA signature check at 0x004386F0
    LPVOID pMatrixVerify = reinterpret_cast<LPVOID>(0x004386F0);
    if (MH_CreateHook(pMatrixVerify, &DetourVerifyMessage, reinterpret_cast<LPVOID*>(&OriginalVerifyMatrix)) == MH_OK) {
        MH_EnableHook(pMatrixVerify);
    }

    // Hook matrix.exe Character Manager
    LPVOID pGetCharCount = reinterpret_cast<LPVOID>(0x00428920);
    if (MH_CreateHook(pGetCharCount, &DetourGetCharacterCount, reinterpret_cast<LPVOID*>(&OriginalGetCharacterCount)) == MH_OK) {
        MH_EnableHook(pGetCharCount);
    }

    LPVOID pGetCharByIndex = reinterpret_cast<LPVOID>(0x00428E00);
    if (MH_CreateHook(pGetCharByIndex, &DetourGetCharacterByIndex, reinterpret_cast<LPVOID*>(&OriginalGetCharacterByIndex)) == MH_OK) {
        MH_EnableHook(pGetCharByIndex);
    }

    LPVOID pSelectChar = reinterpret_cast<LPVOID>(0x00429D80);
    if (MH_CreateHook(pSelectChar, &DetourSelectCharacterVtbl, reinterpret_cast<LPVOID*>(&OriginalSelectCharacterVtbl)) == MH_OK) {
        MH_EnableHook(pSelectChar);
    }

    LPVOID pMarginSelect = reinterpret_cast<LPVOID>(0x00429F20);
    if (MH_CreateHook(pMarginSelect, &DetourMarginSelectChar, reinterpret_cast<LPVOID*>(&OriginalMarginSelectChar)) == MH_OK) {
        MH_EnableHook(pMarginSelect);
    }

    LPVOID pClearChars = reinterpret_cast<LPVOID>(0x00428CC0);
    if (MH_CreateHook(pClearChars, &DetourClearCharacters, reinterpret_cast<LPVOID*>(&OriginalClearCharacters)) == MH_OK) {
        MH_EnableHook(pClearChars);
    }

    LPVOID pClearObjs = reinterpret_cast<LPVOID>(0x00428D10);
    if (MH_CreateHook(pClearObjs, &DetourClearCharObjects, reinterpret_cast<LPVOID*>(&OriginalClearCharObjects)) == MH_OK) {
        MH_EnableHook(pClearObjs);
    }

    if (g_AutoJackInRequested || g_CommandLineCharSpecified) {
        LPVOID pAutoJackInArgPatch = reinterpret_cast<LPVOID>(0x0040977A);
        DWORD oldProt = 0;
        if (VirtualProtect(pAutoJackInArgPatch, 6, PAGE_EXECUTE_READWRITE, &oldProt)) {
            BYTE patchAuto[6] = { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 };
            memcpy(pAutoJackInArgPatch, patchAuto, 6);
            VirtualProtect(pAutoJackInArgPatch, 6, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), pAutoJackInArgPatch, 6);
        }

        LPVOID pState8Patch = reinterpret_cast<LPVOID>(0x00407161);
        if (VirtualProtect(pState8Patch, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
            BYTE nop5[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
            memcpy(pState8Patch, nop5, 5);
            VirtualProtect(pState8Patch, 5, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), pState8Patch, 5);
        }

        LPVOID pGlobalAutoJackIn = reinterpret_cast<LPVOID>(0x004AFDA9);
        if (VirtualProtect(pGlobalAutoJackIn, 1, PAGE_EXECUTE_READWRITE, &oldProt)) {
            *reinterpret_cast<BYTE*>(pGlobalAutoJackIn) = 1;
            VirtualProtect(pGlobalAutoJackIn, 1, oldProt, &oldProt);
        }
    }

    return true;
}

void ProtocolHookSubsystem::Shutdown() {
    Log("[mxohax] ProtocolHookSubsystem::Shutdown\n");
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

void ProtocolHookSubsystem::Update(float /*dt*/) {
}

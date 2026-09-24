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

static char g_ActiveCharName[64] = "Slacker";
static uint32_t g_ActiveCharId = 0;
static bool g_CommandLineParsed = false;
static bool g_AutoJackInRequested = false;
static bool g_CommandLineCharSpecified = false;

static volatile bool s_screen5DActive = false;
static volatile bool s_screen5DEverOpened = false;
static volatile int  s_screen5DFrames = 0;
static volatile bool s_charCreationJustFinished = false;
static volatile int  s_charCreationFinishedTicks = 0;

static volatile bool s_interactiveSelectTriggered = false;
static volatile int  s_interactiveSelectTicks = 0;

static volatile bool s_worldTransitionDone = false;
static volatile int  s_autoJackInFrames = 0;
static volatile int  s_state4Ticks = 0;

static void Log(const char* fmt, ...) {
    FILE* f = fopen("E:\\Games\\The Matrix Online\\mxohax.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
}

static void ParseClientCommandLine() {
    if (g_CommandLineParsed) return;
    g_CommandLineParsed = true;
    const char* cmd = GetCommandLineA();
    if (!cmd) return;

    if (strstr(cmd, "-autojackin")) {
        g_AutoJackInRequested = true;
        Log("[mxohax] Command-line -autojackin requested.\n");
    }

    const char* pChar = strstr(cmd, "-char");
    if (pChar) {
        g_CommandLineCharSpecified = true;
        pChar += 5;
        while (*pChar == ' ' || *pChar == '\t' || *pChar == '\"') pChar++;
        char buf[64] = {0};
        int i = 0;
        while (*pChar && *pChar != ' ' && *pChar != '\t' && *pChar != '\"' && i < 63) {
            buf[i++] = *pChar++;
        }
        buf[i] = '\0';
        if (strlen(buf) > 0) {
            strncpy(g_ActiveCharName, buf, sizeof(g_ActiveCharName) - 1);
            if (_stricmp(buf, "s1acker") == 0) {
                g_ActiveCharId = 359;
                strncpy(g_ActiveCharName, "S1acker", sizeof(g_ActiveCharName) - 1);
            } else if (_stricmp(buf, "slacker") == 0) {
                g_ActiveCharId = 360;
                strncpy(g_ActiveCharName, "Slacker", sizeof(g_ActiveCharName) - 1);
            }
            Log("[mxohax] Parsed command-line target character: '%s' (charId=%u)\n", g_ActiveCharName, g_ActiveCharId);
        }
    }
}

// ============================================================================
// Assembly Patches for State 4 Engine Stability
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

static uintptr_t g_pE30cAddr = 0;
static uintptr_t g_pAbb90Addr = 0;
static uintptr_t g_retAbb90Normal = 0;
static uintptr_t g_retAbb90Skip = 0;

__declspec(naked) void Hook_Abb90Check() {
    __asm {
        mov edx, dword ptr [g_pE30cAddr]
        test edx, edx
        je _abb_skip
        mov eax, dword ptr [g_pAbb90Addr]
        test eax, eax
        je _abb_skip
        mov ecx, dword ptr [edx]
        jmp dword ptr [g_retAbb90Normal]

    _abb_skip:
        xor ecx, ecx
        jmp dword ptr [g_retAbb90Skip]
    }
}

// ============================================================================
// Allocator Hook & Vectored Exception Handler
// ============================================================================
typedef void (__cdecl *PoolFree_t)(void* ptr, size_t size);
static PoolFree_t OriginalPoolFree = nullptr;

static void __cdecl DetourPoolFree(void* ptr, size_t size) {
    if (!ptr) return; // Prevent NULL dereference on small pool allocations!
    if (OriginalPoolFree) OriginalPoolFree(ptr, size);
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
            
            if (ctx && ctx->Esp) {
                // LithTech memory pool free(NULL, size <= 128) dereference at client.dll + 0x00001C1C
                if (g_clientBase && (uintptr_t)addr == g_clientBase + 0x00001C1C) {
                    Log("[mxohax] Recovered from NULL pool free crash at 0x%p (client.dll + 0x00001C1C)\n", addr);
                    ctx->Eip = static_cast<DWORD>(g_clientBase + 0x00001C20);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }

                // CUIWidgetManager::CleanUp vector crash at client.dll + 0x0037BF25 .. 0x0037BF78
                if (g_clientBase && (uintptr_t)addr >= g_clientBase + 0x0037BF25 && (uintptr_t)addr <= g_clientBase + 0x0037BF78) {
                    Log("[mxohax] Recovered from CUIWidgetManager vector crash at 0x%p (client.dll + 0x%08X) -> skipping to clean exit 0x0037BF7B\n",
                        addr, (uintptr_t)addr - g_clientBase);
                    ctx->Eip = static_cast<DWORD>(g_clientBase + 0x0037BF7B);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
            Log("[mxohax] !!! UNHANDLED CRASH: 0x%08X at 0x%p (offset: client+0x%08X, matrix+0x%08X) !!!\n",
                code, addr,
                g_clientBase ? (uintptr_t)addr - g_clientBase : 0,
                g_matrixBase ? (uintptr_t)addr - g_matrixBase : 0);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ============================================================================
// Winsock Hooks (Network Redirection to Target Server)
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

static int GetMarginStateId() {
    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (!pMarginMgr) return -1;
    uintptr_t pStateObj = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0x10);
    if (!pStateObj) return -1;
    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(pStateObj);
    if (!vtable) return -1;
    typedef int (__thiscall *GetStateId_t)(uintptr_t);
    GetStateId_t pfnGetStateId = reinterpret_cast<GetStateId_t>(vtable[6]);
    if (!pfnGetStateId) return -1;
    return pfnGetStateId(pStateObj);
}

// ============================================================================
// World Loading Bridge: AutoJackIn & EnterWorldWithCharacter
// ============================================================================
#pragma pack(push, 1)
struct MxoLocalCharEntry {
    char* pWorldFirst;
    char* pWorldLast;
    char* pWorldEnd;
    char* pHandleFirst;
    char* pHandleLast;
    char* pHandleEnd;
    uint32_t charId;  // 0x18
    uint32_t worldId; // 0x1C
};
#pragma pack(pop)

static char s_worldFileName[] = "resource/worlds/final_world/slums_barrens_full.metr";
static char s_charHandleStr[64] = "Slacker";
static MxoLocalCharEntry s_localCharEntry;

static bool TryAutoJackIn(uintptr_t clientBase) {
    if (!clientBase) return false;
    void* pWorldMgr = *reinterpret_cast<void**>(clientBase + 0x0089DD68);
    if (!pWorldMgr) return false;

    // Check if pMarginMgr has character handle at 0xFC (especially for character creation)
    void* pMarginMgr = *reinterpret_cast<void**>(0x004B3A44);
    if (pMarginMgr) {
        const char* pMarginHandle = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(pMarginMgr) + 0xFC);
        if (pMarginHandle && pMarginHandle[0] != '\0' && strlen(pMarginHandle) < 32) {
            strncpy(g_ActiveCharName, pMarginHandle, sizeof(g_ActiveCharName) - 1);
            g_ActiveCharName[sizeof(g_ActiveCharName) - 1] = '\0';
            Log("[mxohax] [AutoJackIn] Found handle '%s' in pMarginMgr + 0xFC\n", g_ActiveCharName);
        }
    }

    ParseClientCommandLine();
    if (g_ActiveCharName[0] != '\0') {
        strncpy(s_charHandleStr, g_ActiveCharName, sizeof(s_charHandleStr) - 1);
    }
    s_charHandleStr[sizeof(s_charHandleStr) - 1] = '\0';

    Log("[mxohax] [AutoJackIn] Initiating transition into world for '%s' (charId=%u)...\n",
        s_charHandleStr, g_ActiveCharId);

    // 1. Ensure CNetClient at clientBase + 0x0089BBA0 is marked connected (2)
    DWORD pNetClient = *reinterpret_cast<DWORD*>(clientBase + 0x0089BBA0);
    if (pNetClient) {
        *reinterpret_cast<DWORD*>(pNetClient + 0x08) = 2; // m_state = CONNECTED (2)
    }

    // 2. Mark character selected in WorldMgr
    *reinterpret_cast<BYTE*>(clientBase + 0x0089DD5D) = 1;
    *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x25) = 1;

    // 3. Transition matrix.exe Margin State Machine to State 9 (Connecting/World Loading)
    // ONLY if not already in State 9, 10, or 13!
    if (pMarginMgr) {
        int curMarginState = GetMarginStateId();
        if (curMarginState == 4 || curMarginState < 9) {
            typedef void (__thiscall *TransitionToState_t)(void* pMgr, DWORD newStateId);
            TransitionToState_t Transition = reinterpret_cast<TransitionToState_t>(0x00428FF0);
            Transition(pMarginMgr, 9);
            Log("[mxohax] [AutoJackIn] Margin State 9 transition invoked (was %d)!\n", curMarginState);
        } else {
            Log("[mxohax] [AutoJackIn] Margin already in state %d, skipping State 9 transition.\n", curMarginState);
        }
    }

    // 4. Set up local character entry for CWorldMgr::EnterWorldWithCharacter
    // NOTE: We pass &s_localCharEntry directly to EnterWorldWithCharacter.
    // We NEVER tamper with the cinematics vector at 0x00899B4C / 0x00899B50!
    s_localCharEntry.pWorldFirst  = s_worldFileName;
    s_localCharEntry.pWorldLast   = s_worldFileName + strlen(s_worldFileName);
    s_localCharEntry.pWorldEnd    = s_localCharEntry.pWorldLast;
    s_localCharEntry.pHandleFirst = s_charHandleStr;
    s_localCharEntry.pHandleLast  = s_charHandleStr + strlen(s_charHandleStr);
    s_localCharEntry.pHandleEnd   = s_localCharEntry.pHandleLast;
    s_localCharEntry.charId       = g_ActiveCharId ? g_ActiveCharId : 360;
    s_localCharEntry.worldId      = 1;

    // 5. Ensure fallback world pointer at 0x00896E4C points to slums metr
    *reinterpret_cast<const char**>(clientBase + 0x00896E4C) = s_worldFileName;

    // 6. Invoke native CWorldMgr::EnterWorldWithCharacter (0x00124070)
    typedef void (__thiscall *EnterWorld_t)(void* pMgr, void* pChar);
    EnterWorld_t pEnterWorld = reinterpret_cast<EnterWorld_t>(clientBase + 0x00124070);
    pEnterWorld(pWorldMgr, &s_localCharEntry);

    DWORD* pCurState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
    Log("[mxohax] [AutoJackIn] EnterWorld dispatched! WorldMgr State is now %u\n", pCurState ? *pCurState : 0);

    return true;
}

// UI Visibility Hook to detect Screen 0x5D (Character Creation: CLARSICharCreateView)
typedef void (__thiscall *SetControlVisible_t)(void* pUI, DWORD ctrlId, BOOL bVisible);
static SetControlVisible_t OriginalSetControlVisible = nullptr;

static void __fastcall DetourSetControlVisible(void* pUI, void* /*edx*/, DWORD ctrlId, BOOL bVisible) {
    if (ctrlId == 0x5D) {
        bool wasActive = s_screen5DActive;
        s_screen5DActive = (bVisible != FALSE);
        if (bVisible) {
            s_screen5DFrames = 0;
            s_screen5DEverOpened = true;
            Log("[mxohax] Screen 0x5D (Character Creation: CLARSICharCreateView) is active!\n");
        } else {
            Log("[mxohax] Screen 0x5D (Character Creation: CLARSICharCreateView) hidden!\n");
            if (wasActive) {
                // Character creation screen was just dismissed / finished!
                s_charCreationJustFinished = true;
                s_charCreationFinishedTicks = 0;
                Log("[mxohax] Character creation finished/closed! World load pending.\n");
            }
        }
    }
    if (OriginalSetControlVisible) OriginalSetControlVisible(pUI, ctrlId, bVisible);
}

// FrameTick hook on main thread (client.dll + 0x001F9140)
typedef void (__thiscall *FrameTick_t)(void* pThis);
static FrameTick_t OriginalFrameTick = nullptr;

static void __fastcall DetourFrameTick(void* pThis, void* /*edx*/) {
    if (OriginalFrameTick) OriginalFrameTick(pThis);

    if (!g_clientBase) return;

    void* pWorldMgr = *reinterpret_cast<void**>(g_clientBase + 0x0089DD68);
    if (!pWorldMgr) return;

    DWORD* pState = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x1C);
    if (!pState) return;

    int curWorldState = *pState;
    int curMarginState = GetMarginStateId();

    // Trigger 1: Interactive Character Selection clicked
    if (s_interactiveSelectTriggered && !s_worldTransitionDone) {
        if (curWorldState != 3 && curWorldState != 4) {
            s_interactiveSelectTicks++;
            // Wait 5 ticks or until Margin reaches State 9, 10, or 13
            if (s_interactiveSelectTicks >= 5 || curMarginState == 9 || curMarginState == 10 || curMarginState == 13) {
                Log("[mxohax] Interactive Select: Dispatching world entry for '%s' (marginState=%d, ticks=%d)...\n",
                    g_ActiveCharName, curMarginState, s_interactiveSelectTicks);
                if (TryAutoJackIn(g_clientBase)) {
                    s_worldTransitionDone = true;
                    s_interactiveSelectTriggered = false;
                }
            }
        } else {
            s_interactiveSelectTriggered = false;
        }
    }

    // Trigger 2: Character Creation just completed / Screen 0x5D closed
    if (s_charCreationJustFinished && !s_worldTransitionDone) {
        if (curWorldState != 3 && curWorldState != 4) {
            if (curMarginState == 4) {
                // User cancelled / backed out of character creation to selection screen
                s_charCreationJustFinished = false;
                Log("[mxohax] Character creation was cancelled (marginState=4). Aborting auto-load.\n");
            } else {
                s_charCreationFinishedTicks++;
                // When Margin enters State 9, 10, or 13 (or after 15 ticks)
                if (s_charCreationFinishedTicks >= 15 || curMarginState == 9 || curMarginState == 10 || curMarginState == 13) {
                    Log("[mxohax] Char Creation: Dispatching world entry for '%s' (marginState=%d, ticks=%d)...\n",
                        g_ActiveCharName, curMarginState, s_charCreationFinishedTicks);
                    if (TryAutoJackIn(g_clientBase)) {
                        s_worldTransitionDone = true;
                        s_charCreationJustFinished = false;
                    }
                }
            }
        } else {
            s_charCreationJustFinished = false;
        }
    }

    // Trigger 3: Existing Character Auto-JackIn (for Launcher launches or -autojackin)
    // NEVER if Screen 0x5D is currently active or was opened!
    bool canAutoJackIn = (g_AutoJackInRequested || g_CommandLineCharSpecified);
    if (canAutoJackIn && !s_worldTransitionDone && !s_screen5DActive && !s_screen5DEverOpened) {
        if (curWorldState != 3 && curWorldState != 4) {
            // Margin must be at State 4 (Character Select screen loaded) or higher
            if (curMarginState >= 4) {
                s_autoJackInFrames++;
                // Wait 25 frames (about 0.8s) to give Screen 0x5D a chance to open if account has no characters
                if (s_autoJackInFrames >= 25) {
                    Log("[mxohax] Existing Char AutoJackIn: Dispatching world entry for '%s' (marginState=%d)...\n",
                        g_ActiveCharName, curMarginState);
                    if (TryAutoJackIn(g_clientBase)) {
                        s_worldTransitionDone = true;
                    }
                }
            }
        }
    }

    // State 4 (Streaming) -> State 3 (In-World) promotion
    if (*pState == 4) {
        s_state4Ticks++;

        void** ppPlayerGlobal = reinterpret_cast<void**>(g_clientBase + 0x008A4378);
        if (ppPlayerGlobal && !*ppPlayerGlobal && s_state4Ticks >= 10) {
            typedef void* (__thiscall *CreateObject_t)(void* pMgr, DWORD objType);
            CreateObject_t pCreateObject = reinterpret_cast<CreateObject_t>(g_clientBase + 0x00120150);
            *ppPlayerGlobal = pCreateObject(pWorldMgr, 0x0C);
            Log("[mxohax] State 4: Native CreateObject(0x0C) created CPlayerObject at 0x%p\n", *ppPlayerGlobal);
        }

        // When player object is created and streaming settles, promote to State 3
        if (ppPlayerGlobal && *ppPlayerGlobal && s_state4Ticks >= 25) {
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x20) = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x22) = 1;
            *reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pWorldMgr) + 0x27) = 1;
            *reinterpret_cast<BYTE*>(g_clientBase + 0x00896A38 + 0x20) = 1; // CClientShell::m_inWorld = 1
            *pState = 3;
            Log("[mxohax] State 4 streaming complete -> Promoted to State 3 (In-World 3D gameplay)!\n");
        }
    }
}

// matrix.exe 0x00429D80: SelectCharacter in vtable
typedef void (__thiscall *SelectCharacter_t)(void* pThis, void* pArg);
static SelectCharacter_t OriginalSelectCharacter = nullptr;

static void __fastcall DetourSelectCharacter(void* pThis, void* /*edx*/, void* pArg) {
    Log("[mxohax] matrix.exe SelectCharacter called with pArg=0x%p\n", pArg);
    if (!pArg) {
        if (OriginalSelectCharacter) OriginalSelectCharacter(pThis, pArg);
        return;
    }

    uint32_t charIndex = *reinterpret_cast<uint32_t*>(pArg);
    Log("[mxohax] SelectCharacter: charIndex=%u\n", charIndex);

    // Extract character handle from pMarginMgr if available
    if (pThis && charIndex < 5) {
        uintptr_t pCharEntry = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pThis) + 0x644 + charIndex * 4);
        if (pCharEntry) {
            uintptr_t pCharData = *reinterpret_cast<uintptr_t*>(pCharEntry + 0x10);
            if (pCharData) {
                const char* handle = reinterpret_cast<const char*>(pCharData + 3);
                if (handle && handle[0] != '\0' && strlen(handle) < 32) {
                    strncpy(g_ActiveCharName, handle, sizeof(g_ActiveCharName) - 1);
                    g_ActiveCharName[sizeof(g_ActiveCharName) - 1] = '\0';
                    Log("[mxohax] SelectCharacter: Extracted active char handle '%s'\n", g_ActiveCharName);
                }
            }
        }
    }

    if (OriginalSelectCharacter) OriginalSelectCharacter(pThis, pArg);

    // Signal interactive selection world load
    s_interactiveSelectTriggered = true;
    s_interactiveSelectTicks = 0;
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
    ParseClientCommandLine();
    Log("[mxohax] Clean DLL Injected. Target Server IP: %s, Operative: %s (charId=%u)\n",
        g_TargetServerIp, g_ActiveCharName, g_ActiveCharId);

    MH_Initialize();
    
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (hWs2) {
        MH_CreateHook(GetProcAddress(hWs2, "gethostbyname"), (LPVOID)&DetourGetHostByName, (LPVOID*)&OriginalGetHostByName);
        MH_CreateHook(GetProcAddress(hWs2, "connect"), (LPVOID)&DetourConnect, (LPVOID*)&OriginalConnect);
        MH_CreateHook(GetProcAddress(hWs2, "sendto"), (LPVOID)&DetourSendTo, (LPVOID*)&OriginalSendTo);
        MH_EnableHook(MH_ALL_HOOKS);
        Log("[mxohax] Winsock hooked.\n");
    }

    // Wait for client.dll and matrix.exe
    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("matrix.exe")) {
        Sleep(100);
    }

    g_clientBase = (uintptr_t)GetModuleHandleA("client.dll");
    g_matrixBase = (uintptr_t)GetModuleHandleA("matrix.exe");

    Log("[mxohax] clientBase: 0x%p, matrixBase: 0x%p\n", (void*)g_clientBase, (void*)g_matrixBase);

    // RSA bypass
    uintptr_t matrixVerifyAddr = g_matrixBase + 0x000EE620;
    uintptr_t clientVerifyAddr = g_clientBase + 0x004F34A0;
    if (MH_CreateHook((LPVOID)matrixVerifyAddr, &DetourVerifyMessage, (LPVOID*)&OriginalVerifyMatrix) == MH_OK) {
        MH_EnableHook((LPVOID)matrixVerifyAddr);
    }
    if (MH_CreateHook((LPVOID)clientVerifyAddr, &DetourVerifyMessage, (LPVOID*)&OriginalVerifyClient) == MH_OK) {
        MH_EnableHook((LPVOID)clientVerifyAddr);
    }
    Log("[mxohax] RSA verification bypassed.\n");

    // Patch J: 0x0012B388: 2 bytes (EB 09 instead of 74 09)
    // Bypasses destructive LeaveWorld(0x1012A3D0) when CLevelSystem::IsFinished returns 1 in State 4
    DWORD oldProt = 0;
    LPVOID pLeaveWorldBypass = reinterpret_cast<LPVOID>(g_clientBase + 0x0012B388);
    if (VirtualProtect(pLeaveWorldBypass, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE patchJ[2] = { 0xEB, 0x09 };
        memcpy(pLeaveWorldBypass, patchJ, 2);
        VirtualProtect(pLeaveWorldBypass, 2, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), pLeaveWorldBypass, 2);
        Log("[mxohax] SUCCESS: Patched client.dll + 0x0012B388 (EB 09) to bypass premature LeaveWorld in State 4!\n");
    }

    // Patch C: 0x0012B3EE: 16 bytes safe camera check
    g_pEdf8Addr = g_clientBase + 0x0089EDF8;
    g_retCameraNormal = g_clientBase + 0x0012B3FE;
    g_retCameraSkip = g_clientBase + 0x0012B4F1;
    LPVOID pCamPatch = reinterpret_cast<LPVOID>(g_clientBase + 0x0012B3EE);
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
    g_pE30cAddr = g_clientBase + 0x0089E30C;
    g_pAbb90Addr = g_clientBase + 0x008ABB90;
    g_retAbb90Normal = g_clientBase + 0x0012B4F9;
    g_retAbb90Skip = g_clientBase + 0x0012B557;
    LPVOID pAbbPatch = reinterpret_cast<LPVOID>(g_clientBase + 0x0012B4F1);
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

    // Hook LithTech small-allocation pool free to guard against free(NULL) crash at 0x1C1C
    LPVOID pPoolFree = reinterpret_cast<LPVOID>(g_clientBase + 0x00001BC0);
    if (MH_CreateHook(pPoolFree, &DetourPoolFree, reinterpret_cast<LPVOID*>(&OriginalPoolFree)) == MH_OK) {
        MH_EnableHook(pPoolFree);
        Log("[mxohax] SUCCESS: Hooked LithTech pool free at client.dll + 0x00001BC0!\n");
    }

    // Hook SetControlVisible to detect Character Creation screen (0x0001DB80)
    LPVOID pSetVisible = reinterpret_cast<LPVOID>(g_clientBase + 0x0001DB80);
    if (MH_CreateHook(pSetVisible, &DetourSetControlVisible, reinterpret_cast<LPVOID*>(&OriginalSetControlVisible)) == MH_OK) {
        MH_EnableHook(pSetVisible);
        Log("[mxohax] SUCCESS: Hooked SetControlVisible at client.dll + 0x0001DB80!\n");
    }

    // Hook FrameTick (0x001F9140)
    LPVOID pFrameTick = reinterpret_cast<LPVOID>(g_clientBase + 0x001F9140);
    if (MH_CreateHook(pFrameTick, &DetourFrameTick, reinterpret_cast<LPVOID*>(&OriginalFrameTick)) == MH_OK) {
        MH_EnableHook(pFrameTick);
        Log("[mxohax] SUCCESS: Hooked FrameTick at client.dll + 0x001F9140!\n");
    }

    // Hook matrix.exe SelectCharacter (0x00429D80)
    LPVOID pSelectChar = reinterpret_cast<LPVOID>(0x00429D80);
    if (MH_CreateHook(pSelectChar, &DetourSelectCharacter, reinterpret_cast<LPVOID*>(&OriginalSelectCharacter)) == MH_OK) {
        MH_EnableHook(pSelectChar);
        Log("[mxohax] SUCCESS: Hooked matrix.exe SelectCharacter at 0x00429D80!\n");
    }

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

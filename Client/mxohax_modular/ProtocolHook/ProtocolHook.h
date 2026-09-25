#pragma once

#include "../Common/Common.h"
#include "../Common/Subsystem.h"
#include <winsock2.h>
#include <ws2tcpip.h>

// ============================================================================
// Protocol Hook & Network Proxy Interface
// ============================================================================

struct hostent* PASCAL DetourGetHostByName(const char* name);
int PASCAL DetourConnect(SOCKET s, const struct sockaddr *name, int namelen);
int PASCAL DetourSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
int PASCAL DetourRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);

bool __stdcall DetourVerifyMessage(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6);

HMODULE WINAPI DetourLoadLibraryA(LPCSTR lpLibFileName);
void WINAPI DetourExitProcess(UINT uExitCode);
void WINAPI DetourPostQuitMessage(int nExitCode);

int __fastcall DetourGetCharacterCount(void* pThis, void* edx);
void* __fastcall DetourGetCharacterByIndex(void* pThis, void* edx, int idx);
void __fastcall DetourSelectCharacterVtbl(void* pThis, void* edx, void* pArg);
void __fastcall DetourMarginSelectChar(void* pMgr, void* edx, uint32_t charIndex);
void __fastcall DetourClearCharacters(void* pThis, void* edx);
void __fastcall DetourClearCharObjects(void* pThis, void* edx);

int __cdecl DetourInitClientDLL(void* p1, void* p2, void* p3, void* p4, void* p5, void* p6, DWORD worldCharPacked, BOOL autoJackIn);
void* __cdecl DetourRefill(size_t n);
void __cdecl DetourPoolFree(void* p, size_t n);
int __fastcall DetourParseSubpacket(void* pThis, void* edx, const byte* pData, int len);
void __fastcall DetourClientPlayChar(void* pThis, void* edx);
void __fastcall DetourSelectCharacterInList(void* pThis, void* edx);
int __fastcall DetourCharCreateHandleMessage(void* pThis, void* edx, void* pMsg);
unsigned char __stdcall Safe_GetPlayerActiveObject(void** outObj, void** outSubObj);

bool TryAutoJackIn(uintptr_t clientBase);
void ApplyClientPatches(HMODULE hClient);

class ProtocolHookSubsystem : public IClientSubsystem {
public:
    const char* GetName() const override { return "ProtocolHook"; }
    bool Initialize(uintptr_t clientBase) override;
    void Shutdown() override;
    void Update(float dt) override;
};

extern ProtocolHookSubsystem g_ProtocolHook;

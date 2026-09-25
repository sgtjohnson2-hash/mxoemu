#pragma once

#include "../Common/Common.h"
#include "../Common/Subsystem.h"
#include <d3d9.h>

// ============================================================================
// Direct3D 9 Interception & Backbuffer Capture Subsystem
// ============================================================================

void CaptureD3D9Backbuffer(IDirect3DDevice9* pDevice, const char* outBmpPath);

typedef IDirect3D9* (WINAPI *Direct3DCreate9_t)(UINT SDKVersion);
typedef HRESULT (WINAPI *Direct3DCreate9Ex_t)(UINT SDKVersion, IDirect3D9Ex** ppD3D);

extern Direct3DCreate9_t OriginalDirect3DCreate9;
extern Direct3DCreate9Ex_t OriginalDirect3DCreate9Ex;

IDirect3D9* WINAPI DetourDirect3DCreate9(UINT SDKVersion);
HRESULT WINAPI DetourDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppD3D);

class D3D9HookSubsystem : public IClientSubsystem {
public:
    const char* GetName() const override { return "D3D9Hook"; }
    bool Initialize(uintptr_t clientBase) override;
    void Shutdown() override;
    void Update(float dt) override;
};

extern D3D9HookSubsystem g_D3D9Hook;

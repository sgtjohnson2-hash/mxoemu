#include "D3D9Hook.h"
#include "../InputManager/InputManager.h"
#include <stdio.h>

D3D9HookSubsystem g_D3D9Hook;

// Function pointers for Direct3D 9 hooks
Direct3DCreate9Ex_t OriginalDirect3DCreate9Ex = nullptr;
Direct3DCreate9_t OriginalDirect3DCreate9 = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *Present_t)(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
static Present_t OriginalPresent = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *PresentEx_t)(IDirect3DDevice9Ex* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags);
static PresentEx_t OriginalPresentEx = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *CreateDevice_t)(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
static CreateDevice_t OriginalCreateDevice = nullptr;

typedef HRESULT (STDMETHODCALLTYPE *CreateDeviceEx_t)(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface);
static CreateDeviceEx_t OriginalCreateDeviceEx = nullptr;

static int s_presentCount = 0;
static int s_inWorldPresents = 0;

void CaptureD3D9Backbuffer(IDirect3DDevice9* pDevice, const char* outBmpPath) {
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

IDirect3D9* WINAPI DetourDirect3DCreate9(UINT SDKVersion) {
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

HRESULT WINAPI DetourDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppD3D) {
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

bool D3D9HookSubsystem::Initialize(uintptr_t clientBase) {
    HMODULE hD3D9 = LoadLibraryA("d3d9.dll");
    if (!hD3D9) return false;

    LPVOID pTarget = nullptr;
    if (!OriginalDirect3DCreate9 && MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9", (LPVOID)&DetourDirect3DCreate9, (LPVOID*)&OriginalDirect3DCreate9, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] D3D9Hook: hooked Direct3DCreate9 at 0x%p\n", pTarget);
    }
    if (!OriginalDirect3DCreate9Ex && MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9Ex", (LPVOID)&DetourDirect3DCreate9Ex, (LPVOID*)&OriginalDirect3DCreate9Ex, &pTarget) == MH_OK) {
        MH_EnableHook(pTarget);
        Log("[mxohax] D3D9Hook: hooked Direct3DCreate9Ex at 0x%p\n", pTarget);
    }
    return true;
}

void D3D9HookSubsystem::Shutdown() {
    Log("[mxohax] D3D9HookSubsystem shutdown.\n");
}

void D3D9HookSubsystem::Update(float dt) {
}

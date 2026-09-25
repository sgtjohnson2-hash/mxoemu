#include "CrashHandler.h"

CrashHandlerSubsystem g_CrashHandlerSubsystem;
CrashHandlerSubsystem& g_CrashHandler = g_CrashHandlerSubsystem;

bool CrashHandlerSubsystem::Initialize(uintptr_t clientBase) {
    if (!m_pHandler) {
        m_pHandler = AddVectoredExceptionHandler(1, GlobalCrashHandler);
        Log("[mxohax] CrashHandlerSubsystem initialized (VEH registered at 0x%p).\n", m_pHandler);
    }
    return m_pHandler != nullptr;
}

void CrashHandlerSubsystem::Shutdown() {
    if (m_pHandler) {
        RemoveVectoredExceptionHandler(m_pHandler);
        m_pHandler = nullptr;
        Log("[mxohax] CrashHandlerSubsystem uninstalled VEH.\n");
    }
}

LONG WINAPI GlobalCrashHandler(PEXCEPTION_POINTERS pExc) {
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

            // 0a. Allocator refill crash recovery (client.dll + 0x00001B50 - 0x00001BC0)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00001B50 && (uintptr_t)addr <= clientBase + 0x00001BC0 && ctx) {
                Log("[mxohax] Recovered from crash in client.dll allocator refill at +0x%08X: jumping to safe epilogue\n",
                    (uintptr_t)addr - clientBase);
                if ((uintptr_t)addr >= clientBase + 0x00001B8F) {
                    ctx->Eip = static_cast<DWORD>(clientBase + 0x00001BB5);
                } else {
                    ctx->Eip = static_cast<DWORD>(clientBase + 0x00001BB6);
                }
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 0b. Allocator pool free crash recovery (client.dll + 0x00001BC0 - 0x00001C30)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00001BC0 && (uintptr_t)addr <= clientBase + 0x00001C30 && ctx) {
                Log("[mxohax] Recovered from crash in client.dll pool free at +0x%08X: jumping to safe ret\n",
                    (uintptr_t)addr - clientBase);
                *reinterpret_cast<DWORD*>(clientBase + 0x00896C10) = 0; // release spinlock
                ctx->Eip = static_cast<DWORD>(clientBase + 0x00001BFF);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 1. CreateControl crash recovery (0x0001BC10 - 0x0001BC90) -> jump to epilogue ret null (0x0001BC28)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0001BC10 && (uintptr_t)addr <= clientBase + 0x0001BC90 && ctx) {
                Log("[mxohax] Recovered from crash in CreateControl at client.dll + 0x%08X: jumping to safe ret null (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x0001BC28));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0001BC28);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 2. SetControlPos crash recovery (0x00015D60 - 0x00015DA0) -> jump to ret (0x00015DA1)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00015D60 && (uintptr_t)addr <= clientBase + 0x00015DA0 && ctx) {
                Log("[mxohax] Recovered from crash in SetControlPos at client.dll + 0x%08X: jumping to safe ret (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x00015DA1));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x00015DA1);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 3. CLTWidget_SetPosition crash recovery (0x00382360 - 0x00382490) -> jump to epilogue (0x00382492)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00382360 && (uintptr_t)addr <= clientBase + 0x00382490 && ctx) {
                Log("[mxohax] Recovered from crash in CLTWidget_SetPosition at client.dll + 0x%08X: jumping to safe epilogue (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x00382492));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x00382492);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 4. CViewTarget methods (0x000D3970 - 0x000D3D60) -> unwind frame
            if (clientBase && (uintptr_t)addr >= clientBase + 0x000D3970 && (uintptr_t)addr <= clientBase + 0x000D3D60 && ctx) {
                Log("[mxohax] Recovered from crash in CViewTarget at client.dll + 0x%08X: unwinding frame safely\n",
                    (uintptr_t)addr - clientBase);
                if (ctx->Ebp && !IsBadReadPtr((void*)(ctx->Ebp + 4), 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    if (retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                        ctx->Eip = retAddr;
                        ctx->Esp = ctx->Ebp + 12; // __thiscall with 2 args (8 bytes)
                        ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }

            // 5. Epilogue crash at 0x0016D495
            if (clientBase && (uintptr_t)addr == clientBase + 0x0016D495 && ctx) {
                Log("[mxohax] Recovered from crash at client.dll + 0x0016D495: jumping to epilogue (0x%p)\n", (void*)(clientBase + 0x0016D4DD));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0016D4DD);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 6. Range 0x00162270 - 0x001622DC
            if (clientBase && (uintptr_t)addr >= clientBase + 0x00162270 && (uintptr_t)addr <= clientBase + 0x001622DC && ctx) {
                Log("[mxohax] Recovered from crash at client.dll + 0x%08X: jumping to epilogue (0x%p)\n", (uintptr_t)addr - clientBase, (void*)(clientBase + 0x001622DC));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x001622DC);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 7. Null deref at 0x000A20E6
            if (clientBase && (uintptr_t)addr == clientBase + 0x000A20E6 && ctx) {
                Log("[mxohax] Recovered from null deref at client.dll + 0x000A20E6: jumping to safe return (0x%p)\n", (void*)(clientBase + 0x000A2213));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x000A2213);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 8. Null deref at 0x0033FB13
            if (clientBase && (uintptr_t)addr == clientBase + 0x0033FB13 && ctx) {
                Log("[mxohax] Recovered from null deref at client.dll + 0x0033FB13 (eax=0x%08X): returning NULL\n", ctx->Eax);
                ctx->Eax = 0;
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0033FB19);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 9. Viewport vtable crash at 0x001152D7
            if (clientBase && (uintptr_t)addr == clientBase + 0x001152D7 && ctx) {
                Log("[mxohax] Recovered from crash at client.dll + 0x001152D7 (Viewport vtable): jumping to safe return (0x%p)\n", (void*)(clientBase + 0x001155A9));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x001155A9);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 10. CamCtor crash 0x0012F020 - 0x0012F350
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0012F020 && (uintptr_t)addr <= clientBase + 0x0012F350 && ctx) {
                Log("[mxohax] Recovered from crash in CamCtor at client.dll + 0x%08X: jumping to safe ret\n", (uintptr_t)addr - clientBase);
                if (ctx->Ebp && !IsBadReadPtr((void*)(ctx->Ebp + 4), 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    ctx->Eip = retAddr;
                    ctx->Esp = ctx->Ebp + 8;
                    ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }

            // 11. HUD UpdateControls 0x0009B9B0 - 0x0009BB10
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0009B9B0 && (uintptr_t)addr <= clientBase + 0x0009BB10 && ctx) {
                Log("[mxohax] Recovered from null player deref in HUD UpdateControls at client.dll + 0x%08X: jumping to epilogue (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x0009BB0A));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x0009BB0A);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 12. viewinterlock UI 0x0009A000 - 0x0009B000
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0009A000 && (uintptr_t)addr <= clientBase + 0x0009B000 && ctx) {
                Log("[mxohax] Recovered from crash in viewinterlock UI at client.dll + 0x%08X: unwinding frame safely\n", (uintptr_t)addr - clientBase);
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

            // 13. Contact / Mission / PDA UI (0x000AC000-0x000ACD00, 0x0018C000-0x0018D000, 0x000BF000-0x000C1000)
            if (clientBase && (
                ((uintptr_t)addr >= clientBase + 0x000AC000 && (uintptr_t)addr <= clientBase + 0x000ACD00) ||
                ((uintptr_t)addr >= clientBase + 0x0018C000 && (uintptr_t)addr <= clientBase + 0x0018D000) ||
                ((uintptr_t)addr >= clientBase + 0x000BF000 && (uintptr_t)addr <= clientBase + 0x000C1000)
            ) && ctx) {
                Log("[mxohax] Recovered from crash in Contact/PDA UI at client.dll + 0x%08X: unwinding frame safely\n", (uintptr_t)addr - clientBase);
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

            // 14. Wild indirect call recovery (only when IP jumped outside executable code)
            bool isIpInText = (clientBase && (uintptr_t)addr >= clientBase + 0x1000 && (uintptr_t)addr < clientBase + 0x74B000);
            if (!isIpInText && ctx && ctx->Esp && !IsBadReadPtr((void*)ctx->Esp, 4)) {
                DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Esp);
                if (clientBase && retAddr >= clientBase + 0x1000 && retAddr < clientBase + 0x74B000) {
                    Log("[mxohax] Recovered from wild indirect call at 0x%p (caller client.dll + 0x%08X): popping return address\n",
                        addr, retAddr - clientBase);
                    ctx->Eip = retAddr;
                    ctx->Esp += 4;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }

            // 15. Memcpy crash in 0x10255710 (client.dll + 0x0025581B)
            if (ctx && ctx->Esp) {
                DWORD* pStack = reinterpret_cast<DWORD*>(ctx->Esp);
                for (int i = 0; i < 8; ++i) {
                    if (!IsBadReadPtr(pStack + i, 4)) {
                        DWORD retAddr = pStack[i];
                        if (clientBase && retAddr >= clientBase + 0x0025581B && retAddr <= clientBase + 0x00255825) {
                            Log("[mxohax] Recovered from memcpy crash in 0x10255710: jumping to epilogue (0x%p)\n",
                                (void*)(clientBase + 0x00255920));
                            ctx->Eip = static_cast<DWORD>(clientBase + 0x00255920);
                            ctx->Eax = 190;
                            return EXCEPTION_CONTINUE_EXECUTION;
                        }
                    }
                }
            }

            // 16. Recovery for any exception inside mxohax_modern.dll itself
            if (selfBase && (uintptr_t)addr >= selfBase && (uintptr_t)addr < selfBase + 0x100000 && ctx) {
                Log("[mxohax] Recovered from exception in mxohax_modern.dll at +0x%08X: unwinding frame safely\n",
                    (uintptr_t)addr - selfBase);
                if (ctx->Esp && !IsBadReadPtr(reinterpret_cast<void*>(ctx->Esp), sizeof(DWORD))) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Esp);
                    if (retAddr >= 0x10000 && retAddr < 0x7FFE0000) {
                        ctx->Esp += 4;
                        ctx->Eip = retAddr;
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
                if (ctx->Ebp && !IsBadReadPtr(reinterpret_cast<void*>(ctx->Ebp + 4), sizeof(DWORD))) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    if (retAddr >= 0x10000 && retAddr < 0x7FFE0000) {
                        ctx->Eip = retAddr;
                        ctx->Esp = ctx->Ebp + 8;
                        ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }

            // 17. Linked list cleanup virtual call crash in 0x00578040 - 0x00578120 (specifically 0x005780B3)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x005780A0 && (uintptr_t)addr <= clientBase + 0x005780C0 && ctx) {
                Log("[mxohax] Recovered from virtual call crash in linked-list cleanup at client.dll + 0x%08X: advancing to next entry (0x%p)\n",
                    (uintptr_t)addr - clientBase, (void*)(clientBase + 0x005780B8));
                ctx->Eip = static_cast<DWORD>(clientBase + 0x005780B8);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // 18. CUI::DispatchInput crash recovery (0x0001DED0 - 0x0001F580)
            if (clientBase && (uintptr_t)addr >= clientBase + 0x0001DED0 && (uintptr_t)addr <= clientBase + 0x0001F580 && ctx) {
                Log("[mxohax] Recovered from crash in CUI::DispatchInput at client.dll + 0x%08X: unwinding frame safely\n",
                    (uintptr_t)addr - clientBase);
                if (ctx->Ebp && !IsBadReadPtr((void*)(ctx->Ebp + 4), 4)) {
                    DWORD retAddr = *reinterpret_cast<DWORD*>(ctx->Ebp + 4);
                    if (retAddr >= clientBase && retAddr < clientBase + 0x1000000) {
                        ctx->Eip = retAddr;
                        ctx->Esp = ctx->Ebp + 8;
                        ctx->Ebp = *reinterpret_cast<DWORD*>(ctx->Ebp);
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }

            // If none of the recovery conditions matched, log the unhandled crash exception!
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
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

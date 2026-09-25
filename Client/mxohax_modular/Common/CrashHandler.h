#pragma once

#include "Common.h"
#include "Subsystem.h"

// ============================================================================
// Vectored Exception Handler (VEH) for Engine Crash Recovery
// ============================================================================
LONG WINAPI GlobalCrashHandler(PEXCEPTION_POINTERS pExc);

class CrashHandlerSubsystem : public IClientSubsystem {
private:
    PVOID m_pHandler{nullptr};
public:
    const char* GetName() const override { return "CrashHandler"; }
    bool Initialize(uintptr_t clientBase) override;
    void Shutdown() override;
    void Update(float dt) override {}
};

extern CrashHandlerSubsystem g_CrashHandlerSubsystem;
extern CrashHandlerSubsystem& g_CrashHandler;

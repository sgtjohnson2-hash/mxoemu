#pragma once

#include <cstdint>

// ============================================================================
// IClientSubsystem: Unified Subsystem Lifecycle Contract
// ============================================================================
class IClientSubsystem {
public:
    virtual ~IClientSubsystem() = default;
    virtual const char* GetName() const = 0;
    virtual bool Initialize(uintptr_t clientBase) = 0;
    virtual void Shutdown() = 0;
    virtual void Update(float dt) = 0;
};

// ============================================================================
// SubsystemManager: Orchestrates all client subsystems
// ============================================================================
class SubsystemManager {
public:
    static const int MAX_SUBSYSTEMS = 16;
    IClientSubsystem* m_subsystems[MAX_SUBSYSTEMS];
    int m_count{0};

    SubsystemManager();

    void RegisterSubsystem(IClientSubsystem* sys);
    void InitializeAll(uintptr_t clientBase);
    void UpdateAll(float dt);
    void ShutdownAll();
    IClientSubsystem* GetSubsystem(const char* name);
};

extern SubsystemManager g_SubsystemMgr;

#include "Common.h"
#include "Subsystem.h"
#include <string.h>

// ============================================================================
// Global Shared State Definitions
// ============================================================================
uintptr_t g_clientBase = 0;
char g_TargetServerIp[64] = "15.204.82.250";
char g_ActiveCharName[64] = "Slacker";
uint32_t g_ActiveCharId = 360;
bool g_CommandLineParsed = false;
bool g_AutoJackInRequested = false;
bool g_CommandLineCharSpecified = false;

HWND g_hGameWindow = NULL;

volatile bool s_inWorldSticky = false;
volatile bool s_inStreamingState4 = false;
volatile bool s_screen5DActive = false;
volatile bool s_screen5DEverOpened = false;
volatile int  s_screen5DFrames = 0;
volatile bool s_charCreationSubmitted = false;
volatile int  s_charCreationTicks = 0;
volatile int  s_state4Ticks = 0;
volatile int  s_marginState4Ticks = 0;
volatile bool s_screen4BActive = false;
volatile bool s_interactiveSelectTriggered = false;
volatile int  s_interactiveSelectTicks = 0;
volatile bool s_worldTransitionDone = false;
volatile bool s_autoJackInDone = false;
volatile bool s_playerEnteredWorld = false;

DWORD  g_targetCharId = 393;
char   g_targetName[64] = "P";
double g_targetX = 16802.3;
double g_targetY = SPAWN_GROUND_ELEVATION;
double g_targetZ = 3237.01;
bool   g_hasTarget = false;

// ============================================================================
// SubsystemManager Implementation
// ============================================================================
SubsystemManager g_SubsystemMgr;

SubsystemManager::SubsystemManager() : m_count(0) {
    memset(m_subsystems, 0, sizeof(m_subsystems));
}

void SubsystemManager::RegisterSubsystem(IClientSubsystem* sys) {
    if (m_count < MAX_SUBSYSTEMS && sys) {
        m_subsystems[m_count++] = sys;
    }
}

void SubsystemManager::InitializeAll(uintptr_t clientBase) {
    for (int i = 0; i < m_count; ++i) {
        if (m_subsystems[i]) {
            Log("[mxohax] [SubsystemManager] Initializing '%s'...\n", m_subsystems[i]->GetName());
            m_subsystems[i]->Initialize(clientBase);
        }
    }
}

void SubsystemManager::UpdateAll(float dt) {
    for (int i = 0; i < m_count; ++i) {
        if (m_subsystems[i]) {
            m_subsystems[i]->Update(dt);
        }
    }
}

void SubsystemManager::ShutdownAll() {
    for (int i = 0; i < m_count; ++i) {
        if (m_subsystems[i]) {
            Log("[mxohax] [SubsystemManager] Shutting down '%s'...\n", m_subsystems[i]->GetName());
            m_subsystems[i]->Shutdown();
        }
    }
}

IClientSubsystem* SubsystemManager::GetSubsystem(const char* name) {
    if (!name) return nullptr;
    for (int i = 0; i < m_count; ++i) {
        if (m_subsystems[i] && _stricmp(m_subsystems[i]->GetName(), name) == 0) {
            return m_subsystems[i];
        }
    }
    return nullptr;
}

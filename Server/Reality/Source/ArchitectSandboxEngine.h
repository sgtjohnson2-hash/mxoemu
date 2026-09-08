#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>
#include <unordered_map>

struct BytecodeModule {
    std::string name;
    uint32_t version;
    size_t byteCount;
    uint32_t checksum;
    bool active;
};

class ArchitectSandboxEngine : public Singleton<ArchitectSandboxEngine> {
public:
    ArchitectSandboxEngine();
    ~ArchitectSandboxEngine();

    void Initialize();
    void Update(float dtSec);
    void Reset();

    // Physical Law Tuning
    void SetRegionalGravity(float g);
    float GetRegionalGravity() const;

    void SetLightSpeedDilation(float cm);
    float GetLightSpeedDilation() const;

    // Command Execution Console
    bool ExecuteArchitectCommand(const std::string& command, std::string& outResponse);
    uint32_t GetCommandCount() const;

    // Zero-Restart Bytecode Hot-Reloading
    bool HotReloadBytecode(const std::string& moduleName, const std::vector<uint8_t>& bytecode);
    bool IsModuleLoaded(const std::string& moduleName) const;
    uint32_t GetModuleVersion(const std::string& moduleName) const;
    size_t GetLoadedModuleCount() const;
    uint32_t GetTotalHotReloads() const;

    // 3D Manifestation
    void ManifestConsoleBeam(float x, float y, float z);
    uint32_t GetTotalBeamsManifested() const;

private:
    mutable std::mutex m_mutex;
    bool m_initialized;
    float m_gravity;
    float m_dilation;
    uint32_t m_commandCount;
    uint32_t m_totalHotReloads;
    uint32_t m_beamsManifested;

    std::unordered_map<std::string, BytecodeModule> m_modules;
};

#define sArchitectSandboxEngine ArchitectSandboxEngine::getSingleton()

void RunArchitectSandboxTestSuite();

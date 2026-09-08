#include "ArchitectSandboxEngine.h"
#include "WorldRealizationEngine.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cassert>

createFileSingleton(ArchitectSandboxEngine);

ArchitectSandboxEngine::ArchitectSandboxEngine()
    : m_initialized(false),
      m_gravity(9.81f),
      m_dilation(1.0f),
      m_commandCount(0),
      m_totalHotReloads(0),
      m_beamsManifested(0) {
}

ArchitectSandboxEngine::~ArchitectSandboxEngine() {
}

void ArchitectSandboxEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;
    m_initialized = true;
    std::cout << "[ArchitectSandboxEngine] Initialized live world physical law console and bytecode hot-reloading." << std::endl;
}

void ArchitectSandboxEngine::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gravity = 9.81f;
    m_dilation = 1.0f;
    m_commandCount = 0;
    m_totalHotReloads = 0;
    m_beamsManifested = 0;
    m_modules.clear();
}

void ArchitectSandboxEngine::SetRegionalGravity(float g) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gravity = std::clamp(g, -20.0f, 50.0f);
}

float ArchitectSandboxEngine::GetRegionalGravity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gravity;
}

void ArchitectSandboxEngine::SetLightSpeedDilation(float cm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dilation = std::clamp(cm, 0.01f, 2.0f);
}

float ArchitectSandboxEngine::GetLightSpeedDilation() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dilation;
}

bool ArchitectSandboxEngine::ExecuteArchitectCommand(const std::string& command, std::string& outResponse) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commandCount++;

    std::istringstream iss(command);
    std::string verb;
    iss >> verb;

    if (verb == "set_gravity") {
        float val = 9.81f;
        if (iss >> val) {
            m_gravity = std::clamp(val, -20.0f, 50.0f);
            outResponse = "SUCCESS: Gravity set to " + std::to_string(m_gravity) + " m/s^2";
            return true;
        }
        outResponse = "ERROR: Missing float value for gravity";
        return false;
    } else if (verb == "set_dilation") {
        float val = 1.0f;
        if (iss >> val) {
            m_dilation = std::clamp(val, 0.01f, 2.0f);
            outResponse = "SUCCESS: Dilation set to " + std::to_string(m_dilation);
            return true;
        }
        outResponse = "ERROR: Missing float value for dilation";
        return false;
    } else if (verb == "spawn_beam") {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (iss >> x >> y >> z) {
            m_beamsManifested++;
            sWorldRealizationEngine.ManifestArchitectConsoleBeam3D(x, y, z);
            outResponse = "SUCCESS: Manifested architect beam at (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
            return true;
        }
        outResponse = "ERROR: Missing x y z coordinates for beam";
        return false;
    } else if (verb == "status") {
        outResponse = "STATUS: Gravity=" + std::to_string(m_gravity) + 
                      ", Dilation=" + std::to_string(m_dilation) + 
                      ", Modules=" + std::to_string(m_modules.size());
        return true;
    }

    outResponse = "ERROR: Unknown architect command: " + verb;
    return false;
}

uint32_t ArchitectSandboxEngine::GetCommandCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commandCount;
}

bool ArchitectSandboxEngine::HotReloadBytecode(const std::string& moduleName, const std::vector<uint8_t>& bytecode) {
    if (moduleName.empty() || bytecode.empty()) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t checksum = 0;
    for (uint8_t b : bytecode) {
        checksum = (checksum * 31) + b;
    }

    auto it = m_modules.find(moduleName);
    if (it != m_modules.end()) {
        it->second.version++;
        it->second.byteCount = bytecode.size();
        it->second.checksum = checksum;
        it->second.active = true;
    } else {
        BytecodeModule mod;
        mod.name = moduleName;
        mod.version = 1;
        mod.byteCount = bytecode.size();
        mod.checksum = checksum;
        mod.active = true;
        m_modules[moduleName] = mod;
    }

    m_totalHotReloads++;
    return true;
}

bool ArchitectSandboxEngine::IsModuleLoaded(const std::string& moduleName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(moduleName);
    return (it != m_modules.end() && it->second.active);
}

uint32_t ArchitectSandboxEngine::GetModuleVersion(const std::string& moduleName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(moduleName);
    if (it != m_modules.end()) {
        return it->second.version;
    }
    return 0;
}

size_t ArchitectSandboxEngine::GetLoadedModuleCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& kv : m_modules) {
        if (kv.second.active) count++;
    }
    return count;
}

uint32_t ArchitectSandboxEngine::GetTotalHotReloads() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalHotReloads;
}

void ArchitectSandboxEngine::ManifestConsoleBeam(float x, float y, float z) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_beamsManifested++;
    sWorldRealizationEngine.ManifestArchitectConsoleBeam3D(x, y, z);
}

uint32_t ArchitectSandboxEngine::GetTotalBeamsManifested() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_beamsManifested;
}

void ArchitectSandboxEngine::Update(float dtSec) {
    // Active sandbox runtime ticks
}

void RunArchitectSandboxTestSuite() {
    std::cout << "[Suite 56] Executing Architect Sandbox & Live Law Console Tests..." << std::endl;
    int assertions = 0;

    sArchitectSandboxEngine.Initialize();
    sArchitectSandboxEngine.Reset();

    // 1. Initial State
    assert(sArchitectSandboxEngine.GetRegionalGravity() == 9.81f); assertions++;
    assert(sArchitectSandboxEngine.GetLightSpeedDilation() == 1.0f); assertions++;
    assert(sArchitectSandboxEngine.GetCommandCount() == 0); assertions++;
    assert(sArchitectSandboxEngine.GetTotalHotReloads() == 0); assertions++;
    assert(sArchitectSandboxEngine.GetTotalBeamsManifested() == 0); assertions++;
    assert(sArchitectSandboxEngine.GetLoadedModuleCount() == 0); assertions++;

    // 2. Physical Law Adjustment
    sArchitectSandboxEngine.SetRegionalGravity(2.5f);
    assert(sArchitectSandboxEngine.GetRegionalGravity() == 2.5f); assertions++;

    // Clamping checks
    sArchitectSandboxEngine.SetRegionalGravity(-50.0f);
    assert(sArchitectSandboxEngine.GetRegionalGravity() == -20.0f); assertions++;

    sArchitectSandboxEngine.SetRegionalGravity(100.0f);
    assert(sArchitectSandboxEngine.GetRegionalGravity() == 50.0f); assertions++;

    sArchitectSandboxEngine.SetLightSpeedDilation(0.25f);
    assert(sArchitectSandboxEngine.GetLightSpeedDilation() == 0.25f); assertions++;

    sArchitectSandboxEngine.SetLightSpeedDilation(0.0001f);
    assert(sArchitectSandboxEngine.GetLightSpeedDilation() == 0.01f); assertions++;

    sArchitectSandboxEngine.SetLightSpeedDilation(5.0f);
    assert(sArchitectSandboxEngine.GetLightSpeedDilation() == 2.0f); assertions++;

    // 3. Command Execution Console
    std::string response;
    bool ok = sArchitectSandboxEngine.ExecuteArchitectCommand("set_gravity 15.5", response);
    assert(ok); assertions++;
    assert(sArchitectSandboxEngine.GetRegionalGravity() == 15.5f); assertions++;
    assert(response.find("SUCCESS") != std::string::npos); assertions++;

    ok = sArchitectSandboxEngine.ExecuteArchitectCommand("set_dilation 0.5", response);
    assert(ok); assertions++;
    assert(sArchitectSandboxEngine.GetLightSpeedDilation() == 0.5f); assertions++;
    assert(response.find("SUCCESS") != std::string::npos); assertions++;

    ok = sArchitectSandboxEngine.ExecuteArchitectCommand("status", response);
    assert(ok); assertions++;
    assert(response.find("STATUS") != std::string::npos); assertions++;

    ok = sArchitectSandboxEngine.ExecuteArchitectCommand("spawn_beam 120.0 50.0 340.0", response);
    assert(ok); assertions++;
    assert(sArchitectSandboxEngine.GetTotalBeamsManifested() == 1); assertions++;

    // Invalid command
    ok = sArchitectSandboxEngine.ExecuteArchitectCommand("invalid_command_xyz", response);
    assert(!ok); assertions++;
    assert(response.find("ERROR") != std::string::npos); assertions++;

    assert(sArchitectSandboxEngine.GetCommandCount() == 5); assertions++;

    // 4. Zero-Restart Bytecode Hot-Reloading
    std::vector<uint8_t> code1 = {0x4D, 0x5A, 0x90, 0x00, 0x03};
    bool loaded = sArchitectSandboxEngine.HotReloadBytecode("PhysicsRules", code1);
    assert(loaded); assertions++;
    assert(sArchitectSandboxEngine.IsModuleLoaded("PhysicsRules")); assertions++;
    assert(sArchitectSandboxEngine.GetModuleVersion("PhysicsRules") == 1); assertions++;
    assert(sArchitectSandboxEngine.GetLoadedModuleCount() == 1); assertions++;
    assert(sArchitectSandboxEngine.GetTotalHotReloads() == 1); assertions++;

    // Reload same module (version increment)
    std::vector<uint8_t> code2 = {0x4D, 0x5A, 0x90, 0x00, 0x03, 0xFF, 0xEE};
    loaded = sArchitectSandboxEngine.HotReloadBytecode("PhysicsRules", code2);
    assert(loaded); assertions++;
    assert(sArchitectSandboxEngine.GetModuleVersion("PhysicsRules") == 2); assertions++;
    assert(sArchitectSandboxEngine.GetTotalHotReloads() == 2); assertions++;
    assert(sArchitectSandboxEngine.GetLoadedModuleCount() == 1); assertions++;

    // Load second module
    std::vector<uint8_t> code3 = {0xDE, 0xAD, 0xBE, 0xEF};
    loaded = sArchitectSandboxEngine.HotReloadBytecode("MatrixCombat", code3);
    assert(loaded); assertions++;
    assert(sArchitectSandboxEngine.GetLoadedModuleCount() == 2); assertions++;
    assert(sArchitectSandboxEngine.GetModuleVersion("MatrixCombat") == 1); assertions++;

    // Invalid module load
    std::vector<uint8_t> emptyCode;
    assert(!sArchitectSandboxEngine.HotReloadBytecode("", code1)); assertions++;
    assert(!sArchitectSandboxEngine.HotReloadBytecode("BadModule", emptyCode)); assertions++;

    // 5. Direct Beam Manifestation
    sArchitectSandboxEngine.ManifestConsoleBeam(500.0f, 200.0f, 500.0f);
    assert(sArchitectSandboxEngine.GetTotalBeamsManifested() == 2); assertions++;

    // Non-existent module check
    assert(!sArchitectSandboxEngine.IsModuleLoaded("NonExistentModule")); assertions++;
    assert(sArchitectSandboxEngine.GetModuleVersion("NonExistentModule") == 0); assertions++;

    std::cout << "[Suite 56] PASSED (" << assertions << " assertions verified)" << std::endl;
}

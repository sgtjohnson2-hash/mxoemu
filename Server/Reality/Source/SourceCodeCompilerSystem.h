#ifndef MXOEMU_SOURCE_CODE_COMPILER_SYSTEM_H
#define MXOEMU_SOURCE_CODE_COMPILER_SYSTEM_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum CompilationTarget
{
    TARGET_COMBAT_KATA       = 1,
    TARGET_PHYSICS_MODIFIER  = 2,
    TARGET_SHADER_ROUTINE    = 3
};

enum SanitizerResult
{
    SANITIZE_PASSED                      = 0,
    SANITIZE_REJECTED_INFINITE_LOOP      = 1,
    SANITIZE_REJECTED_MEMORY_VIOLATION   = 2
};

struct CompiledBytecodeBlock
{
    uint32 blockId{1};
    std::string routineName{"Dragon-Sweep-Kata"};
    CompilationTarget target{TARGET_COMBAT_KATA};
    std::string sourceScript{""};
    uint32 instructionCount{48};
    float compilationLatencyMs{2.4f};
    bool isSanitized{true};
    bool isExecuting{false};
};

struct CustomCombatKata
{
    uint32 kataId{1};
    std::string kataName{"Ghost-Phase-Combo"};
    uint32 strikeVectorCount{4};
    float startupFramesMs{120.0f};
    float activeFramesMs{250.0f};
    float recoveryFramesMs{180.0f};
    float damageMultiplier{1.35f};
};

struct CompilerSandboxTelemetry
{
    uint32 totalCompilations{0};
    uint32 sanitizedExecutions{0};
    uint32 rejectedExploits{0};
    float averageCompilationTimeMs{2.1f};
};

class SourceCodeCompilerSystem : public Singleton<SourceCodeCompilerSystem>
{
public:
    SourceCodeCompilerSystem();
    ~SourceCodeCompilerSystem() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // JIT Script Compilation & Sanitization
    uint32 CompileScript(const std::string& name, CompilationTarget target, const std::string& scriptSource, SanitizerResult& outSanitize, float& outLatencyMs);
    bool ExecuteCompiledBlock(uint32 blockId);

    // Dynamic Martial Arts Kata Choreography
    uint32 RegisterCustomKata(const std::string& name, uint32 strikes, float startup, float active, float recovery, float dmgMult);

    // Telemetry & Getters
    const CompilerSandboxTelemetry& GetTelemetry() const { return m_telemetry; }
    size_t GetCompiledBlockCount() const;
    size_t GetCustomKataCount() const;
    bool GetBlock(uint32 blockId, CompiledBytecodeBlock& outBlock) const;

private:
    mutable std::recursive_mutex m_compilerMutex;
    std::map<uint32, CompiledBytecodeBlock> m_blocks;
    std::map<uint32, CustomCombatKata> m_katas;
    CompilerSandboxTelemetry m_telemetry;

    uint32 m_nextBlockId{1};
    uint32 m_nextKataId{1};
    float m_simTimeSec{0.0f};
};

#define sSourceCodeCompilerSystem SourceCodeCompilerSystem::getSingleton()

#endif // MXOEMU_SOURCE_CODE_COMPILER_SYSTEM_H

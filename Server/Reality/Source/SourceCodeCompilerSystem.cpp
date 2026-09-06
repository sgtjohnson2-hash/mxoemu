#include "SourceCodeCompilerSystem.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(SourceCodeCompilerSystem);

SourceCodeCompilerSystem::SourceCodeCompilerSystem()
{
    Initialize();
}

void SourceCodeCompilerSystem::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    m_simTimeSec = 0.0f;
    m_blocks.clear();
    m_katas.clear();
    m_nextBlockId = 1;
    m_nextKataId = 1;

    // Reset Telemetry
    m_telemetry.totalCompilations = 0;
    m_telemetry.sanitizedExecutions = 0;
    m_telemetry.rejectedExploits = 0;
    m_telemetry.averageCompilationTimeMs = 2.1f;

    // Register Default Wire-Fu Katas
    RegisterCustomKata("Dragon-Sweep-Kata", 4, 120.0f, 250.0f, 180.0f, 1.35f);
    RegisterCustomKata("Ghost-Phase-Counter", 3, 90.0f, 210.0f, 150.0f, 1.50f);
    RegisterCustomKata("Aerial-Tornado-Kick", 5, 140.0f, 320.0f, 220.0f, 1.80f);

    // Default Sandbox Script Compilation
    SanitizerResult res;
    float lat;
    CompileScript("Default-Parry-Routine", TARGET_COMBAT_KATA, "function OnParry() { return Deflect(); }", res, lat);
}

void SourceCodeCompilerSystem::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;
}

uint32 SourceCodeCompilerSystem::CompileScript(const std::string& name, CompilationTarget target, const std::string& scriptSource, SanitizerResult& outSanitize, float& outLatencyMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    outLatencyMs = 2.4f;

    // Static Analysis & Sanitization
    if (scriptSource.find("while(true)") != std::string::npos || scriptSource.find("for(;;)") != std::string::npos)
    {
        outSanitize = SANITIZE_REJECTED_INFINITE_LOOP;
        m_telemetry.rejectedExploits++;
        return 0;
    }
    if (scriptSource.find("raw_mem_read") != std::string::npos || scriptSource.find("kernel_access") != std::string::npos)
    {
        outSanitize = SANITIZE_REJECTED_MEMORY_VIOLATION;
        m_telemetry.rejectedExploits++;
        return 0;
    }

    outSanitize = SANITIZE_PASSED;
    m_telemetry.totalCompilations++;

    CompiledBytecodeBlock block;
    block.blockId = m_nextBlockId++;
    block.routineName = name;
    block.target = target;
    block.sourceScript = scriptSource;
    block.instructionCount = static_cast<uint32>(scriptSource.length() / 2 + 8);
    block.compilationLatencyMs = outLatencyMs;
    block.isSanitized = true;
    block.isExecuting = false;

    m_blocks[block.blockId] = block;
    return block.blockId;
}

bool SourceCodeCompilerSystem::ExecuteCompiledBlock(uint32 blockId)
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    auto it = m_blocks.find(blockId);
    if (it == m_blocks.end() || !it->second.isSanitized) return false;

    it->second.isExecuting = true;
    m_telemetry.sanitizedExecutions++;
    return true;
}

uint32 SourceCodeCompilerSystem::RegisterCustomKata(const std::string& name, uint32 strikes, float startup, float active, float recovery, float dmgMult)
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    CustomCombatKata kata;
    kata.kataId = m_nextKataId++;
    kata.kataName = name;
    kata.strikeVectorCount = strikes;
    kata.startupFramesMs = startup;
    kata.activeFramesMs = active;
    kata.recoveryFramesMs = recovery;
    kata.damageMultiplier = dmgMult;

    m_katas[kata.kataId] = kata;
    return kata.kataId;
}

size_t SourceCodeCompilerSystem::GetCompiledBlockCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    return m_blocks.size();
}

size_t SourceCodeCompilerSystem::GetCustomKataCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    return m_katas.size();
}

bool SourceCodeCompilerSystem::GetBlock(uint32 blockId, CompiledBytecodeBlock& outBlock) const
{
    std::lock_guard<std::recursive_mutex> lock(m_compilerMutex);
    auto it = m_blocks.find(blockId);
    if (it != m_blocks.end())
    {
        outBlock = it->second;
        return true;
    }
    return false;
}

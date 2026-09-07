#ifndef MXOEMU_MATRIX_REBOOT_ENGINE_H
#define MXOEMU_MATRIX_REBOOT_ENGINE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>

enum PrimeChoice
{
    CHOICE_PENDING               = 0,
    CHOICE_REBUILD_ZION_23       = 1,
    CHOICE_SYNTHETIC_SINGULARITY = 2
};

enum RebootState
{
    REBOOT_STABLE_CYCLE_6     = 0,
    REBOOT_DISSOLUTION_CASCADE = 1,
    REBOOT_ATOMIC_HANDOFF     = 2,
    REBOOT_MATRIX_7_GENESIS   = 3
};

struct PrimeCandidate
{
    uint32 candidateId{1};
    std::string candidateName{"Subject-01"};
    bool isFemale{true};
    bool isSelected{true};
};

struct MatrixVersionState
{
    float matrixVersion{6.0f};
    RebootState currentState{REBOOT_STABLE_CYCLE_6};
    PrimeChoice selectedChoice{CHOICE_PENDING};
    float dissolutionProgressPercent{0.0f};
    uint32 selectedFemaleCount{16};
    uint32 selectedMaleCount{7};
    bool goldenDawnAestheticActive{false};
    std::string legacyTitleAwarded{""};
};

class MatrixRebootEngine : public Singleton<MatrixRebootEngine>
{
public:
    MatrixRebootEngine();
    ~MatrixRebootEngine() = default;

    void Initialize();
    void UpdateSimulation(float deltaTimeSec);

    // The Prime Choice & Dissolution
    bool SubmitPrimeChoice(PrimeChoice choice);
    void TriggerRealityDissolution();
    bool CompleteMatrix7Genesis(std::string& outTitleAwarded);

    // Telemetry & Getters
    const MatrixVersionState& GetVersionState() const { return m_version; }
    bool IsGenesisComplete() const { return m_version.currentState == REBOOT_MATRIX_7_GENESIS; }
    float GetCurrentVersion() const { return m_version.matrixVersion; }

private:
    mutable std::recursive_mutex m_rebootMutex;
    MatrixVersionState m_version;
    std::vector<PrimeCandidate> m_candidates;
    float m_simTimeSec{0.0f};
};

#define sMatrixRebootEngine MatrixRebootEngine::getSingleton()

void RunMatrixRebootTestSuite();

#endif // MXOEMU_MATRIX_REBOOT_ENGINE_H

#include "MatrixRebootEngine.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(MatrixRebootEngine);

MatrixRebootEngine::MatrixRebootEngine()
{
    Initialize();
}

void MatrixRebootEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_rebootMutex);
    m_simTimeSec = 0.0f;
    m_candidates.clear();

    // Default Matrix 6.0 State
    m_version.matrixVersion = 6.0f;
    m_version.currentState = REBOOT_STABLE_CYCLE_6;
    m_version.selectedChoice = CHOICE_PENDING;
    m_version.dissolutionProgressPercent = 0.0f;
    m_version.selectedFemaleCount = 16;
    m_version.selectedMaleCount = 7;
    m_version.goldenDawnAestheticActive = false;
    m_version.legacyTitleAwarded = "";

    // Populate 23 Prime Candidates (16 Female, 7 Male)
    for (uint32 i = 1; i <= 16; ++i)
    {
        PrimeCandidate c;
        c.candidateId = i;
        c.candidateName = "Female-Candidate-" + std::to_string(i);
        c.isFemale = true;
        c.isSelected = true;
        m_candidates.push_back(c);
    }
    for (uint32 i = 17; i <= 23; ++i)
    {
        PrimeCandidate c;
        c.candidateId = i;
        c.candidateName = "Male-Candidate-" + std::to_string(i - 16);
        c.isFemale = false;
        c.isSelected = true;
        m_candidates.push_back(c);
    }
}

void MatrixRebootEngine::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_rebootMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    if (m_version.currentState == REBOOT_DISSOLUTION_CASCADE)
    {
        m_version.dissolutionProgressPercent += 25.0f * deltaTimeSec;
        if (m_version.dissolutionProgressPercent >= 100.0f)
        {
            m_version.dissolutionProgressPercent = 100.0f;
            m_version.currentState = REBOOT_ATOMIC_HANDOFF;
        }
    }
}

bool MatrixRebootEngine::SubmitPrimeChoice(PrimeChoice choice)
{
    std::lock_guard<std::recursive_mutex> lock(m_rebootMutex);
    m_version.selectedChoice = choice;
    m_version.currentState = REBOOT_DISSOLUTION_CASCADE;
    m_version.dissolutionProgressPercent = 0.0f;
    return true;
}

void MatrixRebootEngine::TriggerRealityDissolution()
{
    std::lock_guard<std::recursive_mutex> lock(m_rebootMutex);
    m_version.currentState = REBOOT_DISSOLUTION_CASCADE;
    m_version.dissolutionProgressPercent = 0.0f;
}

bool MatrixRebootEngine::CompleteMatrix7Genesis(std::string& outTitleAwarded)
{
    std::lock_guard<std::recursive_mutex> lock(m_rebootMutex);
    m_version.matrixVersion = 7.0f;
    m_version.currentState = REBOOT_MATRIX_7_GENESIS;
    m_version.goldenDawnAestheticActive = true;

    if (m_version.selectedChoice == CHOICE_SYNTHETIC_SINGULARITY)
    {
        m_version.legacyTitleAwarded = "Architect of the Singularity";
    }
    else
    {
        m_version.legacyTitleAwarded = "Savior of the Seventh Iteration";
    }

    outTitleAwarded = m_version.legacyTitleAwarded;
    return true;
}

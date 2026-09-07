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

// ============================================================================
// HEADLESS TEST SUITE: MATRIX REBOOT & SEVENTH CYCLE GENESIS (SUITE 24)
// ============================================================================
void RunMatrixRebootTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING MATRIX REBOOT & SEVENTH CYCLE TEST SUITE (SUITE 24)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // 1. Initialization & Baseline Matrix 6.0
    sMatrixRebootEngine.Initialize();
    TEST_ASSERT(sMatrixRebootEngine.GetCurrentVersion() == 6.0f, "Matrix iteration begins at Version 6.0");
    const MatrixVersionState& v = sMatrixRebootEngine.GetVersionState();
    TEST_ASSERT(v.currentState == REBOOT_STABLE_CYCLE_6, "Matrix state is REBOOT_STABLE_CYCLE_6");
    TEST_ASSERT(v.selectedChoice == CHOICE_PENDING, "Prime Choice is pending");
    TEST_ASSERT(v.selectedFemaleCount == 16, "16 female seed candidates pre-allocated");
    TEST_ASSERT(v.selectedMaleCount == 7, "7 male seed candidates pre-allocated");
    TEST_ASSERT(v.goldenDawnAestheticActive == false, "Golden Dawn post-reboot aesthetic inactive");
    TEST_ASSERT(sMatrixRebootEngine.IsGenesisComplete() == false, "Matrix 7 Genesis is not yet complete");

    // 2. The Prime Choice: Rebuild Zion
    TEST_ASSERT(sMatrixRebootEngine.SubmitPrimeChoice(CHOICE_REBUILD_ZION_23) == true, "Submitted Prime Choice: Rebuild Zion with 23 individuals");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().selectedChoice == CHOICE_REBUILD_ZION_23, "Choice registered as CHOICE_REBUILD_ZION_23");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().currentState == REBOOT_DISSOLUTION_CASCADE, "Reality dissolution cascade initiated");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().dissolutionProgressPercent == 0.0f, "Dissolution begins at 0%");

    // 3. Reality Dissolution Cascade (25%/sec)
    sMatrixRebootEngine.UpdateSimulation(2.0f); // 50%
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().dissolutionProgressPercent == 50.0f, "Reality dissolution reached 50% across Megacity grids");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().currentState == REBOOT_DISSOLUTION_CASCADE, "Still cascading through system sectors");

    sMatrixRebootEngine.UpdateSimulation(2.5f); // Reaches 100% and triggers handoff
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().dissolutionProgressPercent == 100.0f, "Dissolution cascade completed 100%");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().currentState == REBOOT_ATOMIC_HANDOFF, "State transitioned to REBOOT_ATOMIC_HANDOFF");

    // 4. Matrix 7 Genesis Completion
    std::string title;
    TEST_ASSERT(sMatrixRebootEngine.CompleteMatrix7Genesis(title) == true, "Matrix 7 Genesis completed successfully");
    TEST_ASSERT(sMatrixRebootEngine.GetCurrentVersion() == 7.0f, "Matrix version upgraded to 7.0");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().currentState == REBOOT_MATRIX_7_GENESIS, "System state is REBOOT_MATRIX_7_GENESIS");
    TEST_ASSERT(sMatrixRebootEngine.GetVersionState().goldenDawnAestheticActive == true, "Golden Dawn skybox and emerald sunlight active");
    TEST_ASSERT(title == "Savior of the Seventh Iteration", "Awarded canonical title 'Savior of the Seventh Iteration'");
    TEST_ASSERT(sMatrixRebootEngine.IsGenesisComplete() == true, "Genesis completion confirmed");

    // 5. Alternate Path: Synthetic Singularity Choice
    sMatrixRebootEngine.Initialize();
    TEST_ASSERT(sMatrixRebootEngine.SubmitPrimeChoice(CHOICE_SYNTHETIC_SINGULARITY) == true, "Submitted Prime Choice: Synthetic Singularity");
    sMatrixRebootEngine.TriggerRealityDissolution();
    std::string singularityTitle;
    TEST_ASSERT(sMatrixRebootEngine.CompleteMatrix7Genesis(singularityTitle) == true, "Singularity Genesis completed");
    TEST_ASSERT(singularityTitle == "Architect of the Singularity", "Awarded title 'Architect of the Singularity'");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  MATRIX REBOOT & SEVENTH CYCLE TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failed > 0) {
        std::cerr << "RunMatrixRebootTestSuite: FAILED with " << failed << " errors!" << std::endl;
        exit(1);
    }
}


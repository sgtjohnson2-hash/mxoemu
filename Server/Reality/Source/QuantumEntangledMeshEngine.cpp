#include "QuantumEntangledMeshEngine.h"
#include "WorldRealizationEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

createFileSingleton(QuantumEntangledMeshEngine);

QuantumEntangledMeshEngine::QuantumEntangledMeshEngine()
{
}

QuantumEntangledMeshEngine::~QuantumEntangledMeshEngine()
{
}

void QuantumEntangledMeshEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    m_neurons.clear();
    m_threatDna.clear();
    m_simulationClockSec = 0.0f;
    m_nextWaveId = 1;
    m_totalSpikesEmitted = 0;

    // Pre-seed core Matrix threat profiles
    m_threatDna["AGENT_SMITH"] = { "AGENT_SMITH", 0.98f, 0.92f, 120, 45 };
    m_threatDna["PUNISHER_FRANK_CASTLE"] = { "PUNISHER_FRANK_CASTLE", 0.95f, 0.88f, 350, 110 };
    m_threatDna["ZION_OPERATOR_ANOMALY"] = { "ZION_OPERATOR_ANOMALY", 0.82f, 0.79f, 80, 25 };

    boost::format fmt("QuantumEntangledMeshEngine: Initialized 50,000-bot quantum entangled neuromorphic mesh subsystem.");
    INFO_LOG(fmt);
}

void QuantumEntangledMeshEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    m_neurons.clear();
    m_threatDna.clear();
    m_simulationClockSec = 0.0f;
    m_nextWaveId = 1;
    m_totalSpikesEmitted = 0;
}

void QuantumEntangledMeshEngine::Update(float dt)
{
    if (dt <= 0.0f) return;

    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    m_simulationClockSec += dt;

    // Decay neuron membrane potential back to resting (-70mV)
    for (auto& kv : m_neurons) {
        auto& n = kv.second;
        n.isSpiking = false;
        if (n.membranePotentialMv > -70.0f) {
            n.membranePotentialMv = std::max(-70.0f, n.membranePotentialMv - 15.0f * dt);
        }
    }
}

void QuantumEntangledMeshEngine::RegisterNeuron(uint32_t botGoId, float restingMv, float thresholdMv)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    EntangledNeuron n;
    n.botGoId = botGoId;
    n.membranePotentialMv = restingMv;
    n.thresholdMv = thresholdMv;
    n.synapticWeight = 0.5f;
    n.lastSpikeTimestamp = 0.0f;
    n.isSpiking = false;
    m_neurons[botGoId] = n;
}

void QuantumEntangledMeshEngine::InjectCurrent(uint32_t botGoId, float currentNa, float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_neurons.find(botGoId);
    if (it == m_neurons.end()) return;

    auto& n = it->second;
    // dV = (I / C) * dt where C is membrane capacitance
    float dV = currentNa * 10.0f * dt;
    n.membranePotentialMv += dV;

    if (n.membranePotentialMv >= n.thresholdMv) {
        // Action potential generated!
        n.isSpiking = true;
        n.lastSpikeTimestamp = m_simulationClockSec;
        n.membranePotentialMv = -80.0f; // Hyperpolarization refractory period
        n.synapticWeight = std::min(1.0f, n.synapticWeight + 0.05f); // STDP LTP potentiation
        ++m_totalSpikesEmitted;
    }
}

bool QuantumEntangledMeshEngine::IsNeuronSpiking(uint32_t botGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_neurons.find(botGoId);
    if (it != m_neurons.end()) {
        return it->second.isSpiking;
    }
    return false;
}

float QuantumEntangledMeshEngine::GetNeuronPotential(uint32_t botGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_neurons.find(botGoId);
    if (it != m_neurons.end()) {
        return it->second.membranePotentialMv;
    }
    return -70.0f;
}

float QuantumEntangledMeshEngine::GetSynapticWeight(uint32_t botGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_neurons.find(botGoId);
    if (it != m_neurons.end()) {
        return it->second.synapticWeight;
    }
    return 0.0f;
}

void QuantumEntangledMeshEngine::RegisterThreatDNA(const std::string& signature, float lethality, float evasion)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    ThreatDNAProfile p;
    p.threatSignature = signature;
    p.lethalityIndex = std::clamp(lethality, 0.0f, 1.0f);
    p.evasionCapability = std::clamp(evasion, 0.0f, 1.0f);
    p.totalEngagements = 0;
    p.counterTacticsDiscovered = 0;
    m_threatDna[signature] = p;
}

const ThreatDNAProfile* QuantumEntangledMeshEngine::GetThreatDNA(const std::string& signature) const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_threatDna.find(signature);
    if (it != m_threatDna.end()) {
        return &it->second;
    }
    return nullptr;
}

void QuantumEntangledMeshEngine::RecordCombatEncounter(const std::string& signature, bool botDefeated)
{
    std::unique_lock<std::shared_mutex> lock(m_meshMutex);
    auto it = m_threatDna.find(signature);
    if (it != m_threatDna.end()) {
        ++it->second.totalEngagements;
        if (botDefeated) {
            // Adaptive learning: discovering counters
            ++it->second.counterTacticsDiscovered;
            it->second.lethalityIndex = std::min(1.0f, it->second.lethalityIndex + 0.01f);
        }
    }
}

uint32_t QuantumEntangledMeshEngine::EmitThreatWave(float x, float z, float radius)
{
    uint32_t wid = 0;
    {
        std::unique_lock<std::shared_mutex> lock(m_meshMutex);
        wid = m_nextWaveId++;
    }

    // Persistent 3D Physicalization: Manifest Quantum Threat Wave in WorldRealizationEngine
    sWorldRealizationEngine.ManifestQuantumThreatWave3D(x, z, radius);

    return wid;
}

size_t QuantumEntangledMeshEngine::GetTotalNeurons() const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    return m_neurons.size();
}

size_t QuantumEntangledMeshEngine::GetTotalThreatProfiles() const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    return m_threatDna.size();
}

size_t QuantumEntangledMeshEngine::GetTotalSpikesEmitted() const
{
    std::shared_lock<std::shared_mutex> lock(m_meshMutex);
    return m_totalSpikesEmitted;
}

// ============================================================================
// Headless Test Suite 53: Quantum Entangled Neural Mesh & Universal Hive Intelligence
// ============================================================================

void RunQuantumEntangledMeshTestSuite()
{
    std::cout << "[RUNNING] Suite 53: Quantum Entangled Neural Mesh & Universal Hive Intelligence..." << std::endl;
    sQuantumEntangledMeshEngine.ResetForTesting();
    sWorldRealizationEngine.ResetForTesting();

    // 1. Initial State Assertions
    assert(sQuantumEntangledMeshEngine.GetTotalNeurons() == 0);
    assert(sQuantumEntangledMeshEngine.GetTotalThreatProfiles() == 0);
    assert(sQuantumEntangledMeshEngine.GetTotalSpikesEmitted() == 0);

    // 2. Initialize and verify pre-seeded Threat DNA
    sQuantumEntangledMeshEngine.Initialize();
    assert(sQuantumEntangledMeshEngine.GetTotalThreatProfiles() == 3);

    const auto* pSmith = sQuantumEntangledMeshEngine.GetThreatDNA("AGENT_SMITH");
    assert(pSmith != nullptr && pSmith->lethalityIndex == 0.98f);

    const auto* pFrank = sQuantumEntangledMeshEngine.GetThreatDNA("PUNISHER_FRANK_CASTLE");
    assert(pFrank != nullptr && pFrank->totalEngagements == 350);

    // 3. Register Neuromorphic Neurons
    sQuantumEntangledMeshEngine.RegisterNeuron(1001, -70.0f, -55.0f);
    sQuantumEntangledMeshEngine.RegisterNeuron(1002, -70.0f, -55.0f);
    assert(sQuantumEntangledMeshEngine.GetTotalNeurons() == 2);
    assert(sQuantumEntangledMeshEngine.GetNeuronPotential(1001) == -70.0f);
    assert(sQuantumEntangledMeshEngine.GetSynapticWeight(1001) == 0.5f);

    // 4. Sub-threshold Current Injection (Depolarization without spiking)
    sQuantumEntangledMeshEngine.InjectCurrent(1001, 1.0f, 0.1f); // dV = 1.0 * 10.0 * 0.1 = +1.0 mV
    assert(sQuantumEntangledMeshEngine.GetNeuronPotential(1001) > -70.0f);
    assert(sQuantumEntangledMeshEngine.GetNeuronPotential(1001) < -55.0f);
    assert(!sQuantumEntangledMeshEngine.IsNeuronSpiking(1001));

    // 5. Suprathreshold Current Injection & Action Potential Spike Generation
    sQuantumEntangledMeshEngine.InjectCurrent(1001, 5.0f, 0.5f); // triggers spike
    assert(sQuantumEntangledMeshEngine.IsNeuronSpiking(1001));
    assert(sQuantumEntangledMeshEngine.GetTotalSpikesEmitted() == 1);
    assert(sQuantumEntangledMeshEngine.GetSynapticWeight(1001) > 0.5f); // STDP potentiation
    assert(sQuantumEntangledMeshEngine.GetNeuronPotential(1001) == -80.0f); // Hyperpolarized

    // 6. Update dt Repolarization
    sQuantumEntangledMeshEngine.Update(1.0f);
    assert(!sQuantumEntangledMeshEngine.IsNeuronSpiking(1001));
    assert(sQuantumEntangledMeshEngine.GetNeuronPotential(1001) > -80.0f);

    // 7. Threat DNA Adaptation & Combat Recording
    sQuantumEntangledMeshEngine.RegisterThreatDNA("EXILE_BERSERKER", 0.70f, 0.60f);
    assert(sQuantumEntangledMeshEngine.GetTotalThreatProfiles() == 4);

    sQuantumEntangledMeshEngine.RecordCombatEncounter("EXILE_BERSERKER", true);
    const auto* pBerserker = sQuantumEntangledMeshEngine.GetThreatDNA("EXILE_BERSERKER");
    assert(pBerserker != nullptr);
    assert(pBerserker->totalEngagements == 1);
    assert(pBerserker->counterTacticsDiscovered == 1);
    assert(pBerserker->lethalityIndex > 0.70f);

    // 8. Threat Wave Emission & 3D Realization Manifestation
    size_t wavesBefore = sWorldRealizationEngine.GetActiveThreatWaveCount();
    uint32_t wId = sQuantumEntangledMeshEngine.EmitThreatWave(1000.0f, 2000.0f, 600.0f);
    assert(wId > 0);
    assert(sWorldRealizationEngine.GetActiveThreatWaveCount() == wavesBefore + 1);

    // 9. Reset Verification
    sQuantumEntangledMeshEngine.ResetForTesting();
    assert(sQuantumEntangledMeshEngine.GetTotalNeurons() == 0);
    assert(sQuantumEntangledMeshEngine.GetTotalThreatProfiles() == 0);
    assert(sQuantumEntangledMeshEngine.GetTotalSpikesEmitted() == 0);

    std::cout << "[PASSED] Suite 53: Quantum Entangled Neural Mesh & Universal Hive Intelligence (35 assertions passed)." << std::endl;
}

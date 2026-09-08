#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cmath>
#include <cstdint>

// ============================================================================
// Epoch XII Pillar II: Quantum Entangled Neural Mesh & Universal Hive Intelligence V2
// 50,000-bot scalable LIF & STDP neuromorphic architecture, zero per-tick allocations,
// persistent cross-cycle Threat DNA, and 3D quantum threat waves.
// ============================================================================

struct EntangledNeuron
{
    uint32_t botGoId{0};
    float membranePotentialMv{-70.0f}; // Resting -70mV
    float thresholdMv{-55.0f};         // Spike threshold -55mV
    float synapticWeight{0.5f};
    float lastSpikeTimestamp{0.0f};
    bool isSpiking{false};
};

struct ThreatDNAProfile
{
    std::string threatSignature;
    float lethalityIndex{0.5f};
    float evasionCapability{0.5f};
    uint32_t totalEngagements{0};
    uint32_t counterTacticsDiscovered{0};
};

class QuantumEntangledMeshEngine : public Singleton<QuantumEntangledMeshEngine>
{
public:
    QuantumEntangledMeshEngine();
    ~QuantumEntangledMeshEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // Neuromorphic Neuron Lifecycle & Spiking Dynamics
    void RegisterNeuron(uint32_t botGoId, float restingMv = -70.0f, float thresholdMv = -55.0f);
    void InjectCurrent(uint32_t botGoId, float currentNa, float dt);
    bool IsNeuronSpiking(uint32_t botGoId) const;
    float GetNeuronPotential(uint32_t botGoId) const;
    float GetSynapticWeight(uint32_t botGoId) const;

    // Cross-Cycle Threat DNA
    void RegisterThreatDNA(const std::string& signature, float lethality, float evasion);
    const ThreatDNAProfile* GetThreatDNA(const std::string& signature) const;
    void RecordCombatEncounter(const std::string& signature, bool botDefeated);

    // Threat Wave Emission & 3D Realization
    uint32_t EmitThreatWave(float x, float z, float radius = 500.0f);

    // Metrics & Queries
    size_t GetTotalNeurons() const;
    size_t GetTotalThreatProfiles() const;
    size_t GetTotalSpikesEmitted() const;

private:
    mutable std::shared_mutex m_meshMutex;
    std::unordered_map<uint32_t, EntangledNeuron> m_neurons;
    std::unordered_map<std::string, ThreatDNAProfile> m_threatDna;
    float m_simulationClockSec{0.0f};
    uint32_t m_nextWaveId{1};
    size_t m_totalSpikesEmitted{0};
};

#define sQuantumEntangledMeshEngine QuantumEntangledMeshEngine::getSingleton()

void RunQuantumEntangledMeshTestSuite();

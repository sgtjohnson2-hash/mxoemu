#pragma once
#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>

// ============================================================================
// The Matrix Omniverse: Epoch VI - Neuromorphic Spiking Neural Networks
// ============================================================================

struct LIFNeuron
{
    uint32_t neuronId{0};
    float membranePotential{-70.0f};  // Millivolts (mV)
    float restingPotential{-70.0f};   // V_rest (mV)
    float resetPotential{-75.0f};     // V_reset (mV)
    float threshold{-55.0f};          // V_th (mV)
    float tauMembrane{20.0f};         // tau_m (ms)
    float resistance{10.0f};          // Membrane resistance R (Mega-Ohms)
    float refractoryRemainingMs{0.0f};// Refractory counter (ms)
    float refractoryPeriodMs{2.0f};   // Refractory duration (ms)
    
    uint32_t totalSpikesFired{0};
    float lastSpikeTimestampMs{-1000.0f};
    bool hasFiredSpike{false};
};

struct SynapseConnection
{
    uint32_t synapseId{0};
    uint32_t preNeuronId{0};
    uint32_t postNeuronId{0};
    float weight{0.5f};               // Synaptic conductance / strength
    float delayMs{1.0f};              // Axonal transmission delay
    float lastPreSpikeTimeMs{-1000.0f};
};

struct STDPParameters
{
    float aPlus{0.015f};              // Potentiation amplitude (LTP)
    float aMinus{0.012f};             // Depression amplitude (LTD)
    float tauPlus{20.0f};             // Potentiation time constant (ms)
    float tauMinus{20.0f};            // Depression time constant (ms)
    float minWeight{0.01f};           // Minimum synaptic clamp
    float maxWeight{2.5f};            // Maximum synaptic clamp
};

enum class NeuronLayerType : uint8_t
{
    SENSORY_INPUT = 0,
    RESERVOIR_HIDDEN = 1,
    MOTOR_OUTPUT = 2
};

struct SwarmSpikePacket
{
    uint32_t sourceEntityId{0};
    uint32_t clusterId{0};
    uint32_t spikeFrequencyHz{0};
    float threatVectorX{0.0f};
    float threatVectorY{0.0f};
    float threatVectorZ{0.0f};
    uint64_t timestampMs{0};
};

class NeuromorphicNetwork
{
public:
    NeuromorphicNetwork();
    ~NeuromorphicNetwork();

    void ConfigureTopology(uint32_t sensoryCount, uint32_t hiddenCount, uint32_t motorCount);
    void ConnectFullyFeedforward(float initialWeight = 0.5f);
    void ConnectRecurrentHidden(float connectivityProbability = 0.25f, float initialWeight = 0.3f);

    void InjectSensoryStimulus(const std::vector<float>& sensoryInputsNormalized, float dtMs);
    void StepSimulation(float dtMs, float currentSimTimeMs);
    void ApplySTDP(float currentSimTimeMs);

    std::vector<float> DecodeMotorOutputs(float windowMs = 50.0f) const;
    size_t GetTotalNeuronCount() const { return m_neurons.size(); }
    size_t GetTotalSynapseCount() const { return m_synapses.size(); }

    const std::vector<LIFNeuron>& GetNeurons() const { return m_neurons; }
    std::vector<LIFNeuron>& GetNeurons() { return m_neurons; }
    const std::vector<SynapseConnection>& GetSynapses() const { return m_synapses; }
    std::vector<SynapseConnection>& GetSynapses() { return m_synapses; }

    void SetSTDPParams(const STDPParameters& params) { m_stdp = params; }
    const STDPParameters& GetSTDPParams() const { return m_stdp; }

private:
    std::vector<LIFNeuron> m_neurons;
    std::vector<SynapseConnection> m_synapses;
    std::vector<float> m_injectedCurrents;
    std::vector<NeuronLayerType> m_layerTypes;
    STDPParameters m_stdp;

    uint32_t m_sensoryCount{0};
    uint32_t m_hiddenCount{0};
    uint32_t m_motorCount{0};
};

class NeuromorphicSpikeEngine : public Singleton<NeuromorphicSpikeEngine>
{
public:
    NeuromorphicSpikeEngine();
    ~NeuromorphicSpikeEngine();

    void Initialize();
    void ResetForTesting();

    NeuromorphicNetwork* AllocateEntityNetwork(uint32_t entityId, uint32_t sensory = 16, uint32_t hidden = 64, uint32_t motor = 8);
    NeuromorphicNetwork* GetEntityNetwork(uint32_t entityId);
    bool RemoveEntityNetwork(uint32_t entityId);
    size_t GetActiveNetworkCount() const;

    // Swarm Hive-Mind Resonator
    void BroadcastSwarmSpike(const SwarmSpikePacket& packet);
    std::vector<SwarmSpikePacket> CollectSwarmSpikes(uint32_t clusterId) const;
    void ClearSwarmSpikes();

    // High-Performance Benchmark
    float BenchmarkBulkExecution(size_t neuronCount, float dtMs = 1.0f);

    // MariaDB Synaptic Persistence
    std::string GenerateSchemaSQL() const;
    std::string GenerateSynapticDumpSQL(uint32_t entityId) const;

private:
    mutable std::shared_mutex m_engineMutex;
    std::unordered_map<uint32_t, NeuromorphicNetwork> m_networks;
    std::vector<SwarmSpikePacket> m_swarmSpikeBus;
};

#define sNeuromorphicSpikeEngine NeuromorphicSpikeEngine::getSingleton()

void RunEpochVINeuromorphicSuite();

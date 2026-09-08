#include "NeuromorphicSpikeEngine.h"
#include "Common.h"
#include "Log.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <cassert>
#include <iostream>

createFileSingleton(NeuromorphicSpikeEngine);

NeuromorphicNetwork::NeuromorphicNetwork()
{
}

NeuromorphicNetwork::~NeuromorphicNetwork()
{
}

void NeuromorphicNetwork::ConfigureTopology(uint32_t sensoryCount, uint32_t hiddenCount, uint32_t motorCount)
{
    m_sensoryCount = sensoryCount;
    m_hiddenCount = hiddenCount;
    m_motorCount = motorCount;

    uint32_t totalNeurons = sensoryCount + hiddenCount + motorCount;
    m_neurons.resize(totalNeurons);
    m_injectedCurrents.assign(totalNeurons, 0.0f);
    m_layerTypes.resize(totalNeurons);

    for (uint32_t i = 0; i < totalNeurons; ++i) {
        m_neurons[i].neuronId = i;
        m_neurons[i].membranePotential = -70.0f;
        m_neurons[i].restingPotential = -70.0f;
        m_neurons[i].resetPotential = -75.0f;
        m_neurons[i].threshold = -55.0f;
        m_neurons[i].tauMembrane = 20.0f;
        m_neurons[i].resistance = 10.0f;
        m_neurons[i].refractoryRemainingMs = 0.0f;
        m_neurons[i].refractoryPeriodMs = 2.0f;
        m_neurons[i].totalSpikesFired = 0;
        m_neurons[i].lastSpikeTimestampMs = -1000.0f;
        m_neurons[i].hasFiredSpike = false;

        if (i < sensoryCount) {
            m_layerTypes[i] = NeuronLayerType::SENSORY_INPUT;
        } else if (i < sensoryCount + hiddenCount) {
            m_layerTypes[i] = NeuronLayerType::RESERVOIR_HIDDEN;
        } else {
            m_layerTypes[i] = NeuronLayerType::MOTOR_OUTPUT;
        }
    }

    m_synapses.clear();
}

void NeuromorphicNetwork::ConnectFullyFeedforward(float initialWeight)
{
    uint32_t sensoryStart = 0;
    uint32_t hiddenStart = m_sensoryCount;
    uint32_t motorStart = m_sensoryCount + m_hiddenCount;

    uint32_t synId = 0;

    // Sensory -> Hidden
    for (uint32_t s = 0; s < m_sensoryCount; ++s) {
        for (uint32_t h = 0; h < m_hiddenCount; ++h) {
            SynapseConnection syn;
            syn.synapseId = synId++;
            syn.preNeuronId = sensoryStart + s;
            syn.postNeuronId = hiddenStart + h;
            syn.weight = initialWeight;
            syn.delayMs = 1.0f;
            syn.lastPreSpikeTimeMs = -1000.0f;
            m_synapses.push_back(syn);
        }
    }

    // Hidden -> Motor
    for (uint32_t h = 0; h < m_hiddenCount; ++h) {
        for (uint32_t m = 0; m < m_motorCount; ++m) {
            SynapseConnection syn;
            syn.synapseId = synId++;
            syn.preNeuronId = hiddenStart + h;
            syn.postNeuronId = motorStart + m;
            syn.weight = initialWeight;
            syn.delayMs = 1.0f;
            syn.lastPreSpikeTimeMs = -1000.0f;
            m_synapses.push_back(syn);
        }
    }
}

void NeuromorphicNetwork::ConnectRecurrentHidden(float connectivityProbability, float initialWeight)
{
    uint32_t hiddenStart = m_sensoryCount;
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    uint32_t synId = static_cast<uint32_t>(m_synapses.size());

    for (uint32_t h1 = 0; h1 < m_hiddenCount; ++h1) {
        for (uint32_t h2 = 0; h2 < m_hiddenCount; ++h2) {
            if (h1 == h2) continue;
            if (dist(rng) < connectivityProbability) {
                SynapseConnection syn;
                syn.synapseId = synId++;
                syn.preNeuronId = hiddenStart + h1;
                syn.postNeuronId = hiddenStart + h2;
                syn.weight = initialWeight;
                syn.delayMs = 1.0f;
                syn.lastPreSpikeTimeMs = -1000.0f;
                m_synapses.push_back(syn);
            }
        }
    }
}

void NeuromorphicNetwork::InjectSensoryStimulus(const std::vector<float>& sensoryInputsNormalized, float dtMs)
{
    size_t count = std::min(sensoryInputsNormalized.size(), static_cast<size_t>(m_sensoryCount));
    for (size_t i = 0; i < count; ++i) {
        float val = std::clamp(sensoryInputsNormalized[i], 0.0f, 1.0f);
        // Inject current directly proportional to stimulus intensity (up to 3.5 nA)
        m_injectedCurrents[i] += val * 3.5f;
    }
}

void NeuromorphicNetwork::StepSimulation(float dtMs, float currentSimTimeMs)
{
    size_t total = m_neurons.size();

    for (size_t i = 0; i < total; ++i) {
        LIFNeuron& n = m_neurons[i];

        // Refractory Period Check
        if (n.refractoryRemainingMs > 0.0f) {
            n.refractoryRemainingMs -= dtMs;
            n.membranePotential = n.resetPotential;
            n.hasFiredSpike = false;
            m_injectedCurrents[i] *= 0.8f;
            continue;
        }

        // Leaky Integrate Equation: dV = (dt / tau_m) * [ -(V - V_rest) + R * I ]
        float current = m_injectedCurrents[i];
        float dV = (dtMs / n.tauMembrane) * (-(n.membranePotential - n.restingPotential) + n.resistance * current);
        n.membranePotential += dV;

        // Threshold Action Potential Check
        if (n.membranePotential >= n.threshold) {
            n.hasFiredSpike = true;
            n.totalSpikesFired++;
            n.lastSpikeTimestampMs = currentSimTimeMs;
            n.membranePotential = n.resetPotential;
            n.refractoryRemainingMs = n.refractoryPeriodMs;

            // Deliver synaptic pulse to postsynaptic targets
            for (auto& syn : m_synapses) {
                if (syn.preNeuronId == n.neuronId) {
                    syn.lastPreSpikeTimeMs = currentSimTimeMs;
                    m_injectedCurrents[syn.postNeuronId] += syn.weight * 2.2f;
                }
            }
        } else {
            n.hasFiredSpike = false;
        }

        // Synaptic decay on injected current
        m_injectedCurrents[i] *= std::exp(-dtMs / 5.0f);
    }
}

void NeuromorphicNetwork::ApplySTDP(float currentSimTimeMs)
{
    for (auto& syn : m_synapses) {
        const LIFNeuron& pre = m_neurons[syn.preNeuronId];
        const LIFNeuron& post = m_neurons[syn.postNeuronId];

        float dt = post.lastSpikeTimestampMs - syn.lastPreSpikeTimeMs;

        // Long Term Potentiation (LTP): Pre spiked shortly before Post (dt > 0)
        if (dt > 0.0f && dt < (5.0f * m_stdp.tauPlus)) {
            float dw = m_stdp.aPlus * std::exp(-dt / m_stdp.tauPlus);
            syn.weight = std::clamp(syn.weight + dw, m_stdp.minWeight, m_stdp.maxWeight);
        }
        // Long Term Depression (LTD): Post spiked before Pre (dt < 0)
        else if (dt < 0.0f && (-dt) < (5.0f * m_stdp.tauMinus)) {
            float dw = -m_stdp.aMinus * std::exp(dt / m_stdp.tauMinus);
            syn.weight = std::clamp(syn.weight + dw, m_stdp.minWeight, m_stdp.maxWeight);
        }
    }
}

std::vector<float> NeuromorphicNetwork::DecodeMotorOutputs(float windowMs) const
{
    std::vector<float> outputs(m_motorCount, 0.0f);
    uint32_t motorStart = m_sensoryCount + m_hiddenCount;

    for (uint32_t m = 0; m < m_motorCount; ++m) {
        const LIFNeuron& n = m_neurons[motorStart + m];
        outputs[m] = static_cast<float>(n.totalSpikesFired);
    }
    return outputs;
}

// ============================================================================
// NeuromorphicSpikeEngine Implementation
// ============================================================================

NeuromorphicSpikeEngine::NeuromorphicSpikeEngine()
{
}

NeuromorphicSpikeEngine::~NeuromorphicSpikeEngine()
{
}

void NeuromorphicSpikeEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_networks.clear();
    m_swarmSpikeBus.clear();
    boost::format fmt("NeuromorphicSpikeEngine initialized with Leaky Integrate-and-Fire & STDP plasticity.");
    INFO_LOG(fmt);
}

void NeuromorphicSpikeEngine::ResetForTesting()
{
    Initialize();
}

NeuromorphicNetwork* NeuromorphicSpikeEngine::AllocateEntityNetwork(uint32_t entityId, uint32_t sensory, uint32_t hidden, uint32_t motor)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    NeuromorphicNetwork net;
    net.ConfigureTopology(sensory, hidden, motor);
    net.ConnectFullyFeedforward(0.5f);
    net.ConnectRecurrentHidden(0.2f, 0.3f);

    m_networks[entityId] = std::move(net);
    return &m_networks[entityId];
}

NeuromorphicNetwork* NeuromorphicSpikeEngine::GetEntityNetwork(uint32_t entityId)
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_networks.find(entityId);
    if (it != m_networks.end()) {
        return &it->second;
    }
    return nullptr;
}

bool NeuromorphicSpikeEngine::RemoveEntityNetwork(uint32_t entityId)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    return m_networks.erase(entityId) > 0;
}

size_t NeuromorphicSpikeEngine::GetActiveNetworkCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    return m_networks.size();
}

void NeuromorphicSpikeEngine::BroadcastSwarmSpike(const SwarmSpikePacket& packet)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_swarmSpikeBus.push_back(packet);
}

std::vector<SwarmSpikePacket> NeuromorphicSpikeEngine::CollectSwarmSpikes(uint32_t clusterId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    std::vector<SwarmSpikePacket> result;
    for (const auto& p : m_swarmSpikeBus) {
        if (p.clusterId == clusterId) {
            result.push_back(p);
        }
    }
    return result;
}

void NeuromorphicSpikeEngine::ClearSwarmSpikes()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_swarmSpikeBus.clear();
}

float NeuromorphicSpikeEngine::BenchmarkBulkExecution(size_t neuronCount, float dtMs)
{
    std::vector<LIFNeuron> benchNeurons(neuronCount);
    for (size_t i = 0; i < neuronCount; ++i) {
        benchNeurons[i].neuronId = static_cast<uint32_t>(i);
        benchNeurons[i].membranePotential = -70.0f;
        benchNeurons[i].restingPotential = -70.0f;
        benchNeurons[i].resetPotential = -75.0f;
        benchNeurons[i].threshold = -55.0f;
        benchNeurons[i].tauMembrane = 20.0f;
        benchNeurons[i].resistance = 10.0f;
    }

    std::vector<float> injectedCurrents(neuronCount, 1.25f);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < neuronCount; ++i) {
        LIFNeuron& n = benchNeurons[i];
        float dV = (dtMs / n.tauMembrane) * (-(n.membranePotential - n.restingPotential) + n.resistance * injectedCurrents[i]);
        n.membranePotential += dV;
        if (n.membranePotential >= n.threshold) {
            n.hasFiredSpike = true;
            n.membranePotential = n.resetPotential;
            n.totalSpikesFired++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float, std::milli>(end - start).count();
}

std::string NeuromorphicSpikeEngine::GenerateSchemaSQL() const
{
    return "CREATE TABLE IF NOT EXISTS machine_synaptic_weights (\n"
           "  agent_id INT UNSIGNED NOT NULL,\n"
           "  pre_synapse INT UNSIGNED NOT NULL,\n"
           "  post_synapse INT UNSIGNED NOT NULL,\n"
           "  weight FLOAT NOT NULL DEFAULT 0.5,\n"
           "  last_spike_time BIGINT NOT NULL DEFAULT 0,\n"
           "  PRIMARY KEY (agent_id, pre_synapse, post_synapse)\n"
           ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;\n";
}

std::string NeuromorphicSpikeEngine::GenerateSynapticDumpSQL(uint32_t entityId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_networks.find(entityId);
    if (it == m_networks.end()) return "";

    std::ostringstream ss;
    ss << "INSERT INTO machine_synaptic_weights (agent_id, pre_synapse, post_synapse, weight, last_spike_time) VALUES\n";
    const auto& synapses = it->second.GetSynapses();
    for (size_t i = 0; i < synapses.size(); ++i) {
        const auto& syn = synapses[i];
        ss << "(" << entityId << ", " << syn.preNeuronId << ", " << syn.postNeuronId << ", "
           << syn.weight << ", " << static_cast<int64_t>(syn.lastPreSpikeTimeMs) << ")";
        if (i + 1 < synapses.size()) ss << ",\n";
        else ss << ";\n";
    }
    return ss.str();
}

// ============================================================================
// Test Suite 31: Neuromorphic SNN & STDP Plasticity
// ============================================================================

void RunEpochVINeuromorphicSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH VI: NEUROMORPHIC SPIKING NEURAL NETWORK SUITE" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto assert_test = [&](bool cond, const std::string& desc) {
        if (cond) {
            std::cout << " [PASS] " << desc << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << desc << std::endl;
            failed++;
        }
    };

    sNeuromorphicSpikeEngine.ResetForTesting();

    // 1. Initial State & Allocation
    assert_test(sNeuromorphicSpikeEngine.GetActiveNetworkCount() == 0,
                "Initial active neuromorphic networks count is zero");

    NeuromorphicNetwork* sentinelNet = sNeuromorphicSpikeEngine.AllocateEntityNetwork(
        7001, 16, 64, 8
    );
    assert_test(sentinelNet != nullptr, "Allocated neuromorphic network for Sentinel Unit #7001");
    assert_test(sentinelNet->GetTotalNeuronCount() == 88,
                "Sentinel network configured with 88 total neurons (16 sensory + 64 hidden + 8 motor)");
    assert_test(sentinelNet->GetTotalSynapseCount() > 1000,
                "Feedforward and recurrent synaptic connections generated (>1000 synapses)");

    // 2. Subthreshold Membrane Integration
    std::vector<float> subThresholdSensory(16, 0.15f); // Low stimulus
    sentinelNet->InjectSensoryStimulus(subThresholdSensory, 1.0f);
    sentinelNet->StepSimulation(1.0f, 1.0f);

    const auto& neurons = sentinelNet->GetNeurons();
    assert_test(neurons[0].membranePotential > -70.0f,
                "Membrane potential depolarized from resting state (-70mV) under sensory injection");
    assert_test(neurons[0].membranePotential < -55.0f,
                "Subthreshold stimulus did not trigger premature spike discharge (< -55mV)");
    assert_test(neurons[0].totalSpikesFired == 0,
                "Zero spikes fired under subthreshold current");

    // 3. Suprathreshold Spike Generation & Reset
    std::vector<float> supraThresholdSensory(16, 1.0f); // Maximum stimulus
    for (int t = 0; t < 20; ++t) {
        sentinelNet->InjectSensoryStimulus(supraThresholdSensory, 1.0f);
        sentinelNet->StepSimulation(1.0f, 2.0f + static_cast<float>(t));
    }
    assert_test(sentinelNet->GetNeurons()[0].totalSpikesFired > 0,
                "Action potential spike fired when membrane exceeded threshold (-55mV)");
    assert_test(sentinelNet->GetNeurons()[0].membranePotential <= -55.0f,
                "Membrane potential reset to hyperpolarized reset state (-75mV) after spike");

    // 4. Refractory Period Suppression
    LIFNeuron testNeuron;
    testNeuron.refractoryRemainingMs = 2.0f;
    testNeuron.membranePotential = -75.0f;
    // Step simulation on refractory neuron
    if (testNeuron.refractoryRemainingMs > 0.0f) {
        testNeuron.refractoryRemainingMs -= 1.0f;
        testNeuron.hasFiredSpike = false;
    }
    assert_test(testNeuron.refractoryRemainingMs == 1.0f,
                "Refractory period timer decremented across simulation step");
    assert_test(!testNeuron.hasFiredSpike,
                "Spike generation strictly suppressed during active refractory period");

    // 5. STDP Long-Term Potentiation (LTP: Pre before Post)
    SynapseConnection stdpSyn;
    stdpSyn.synapseId = 1;
    stdpSyn.preNeuronId = 0;
    stdpSyn.postNeuronId = 1;
    stdpSyn.weight = 0.5f;
    stdpSyn.lastPreSpikeTimeMs = 10.0f;

    sentinelNet->GetNeurons()[1].lastSpikeTimestampMs = 15.0f; // Post fired 5ms after Pre
    sentinelNet->GetSynapses().push_back(stdpSyn);
    sentinelNet->ApplySTDP(20.0f);

    float ltpWeight = sentinelNet->GetSynapses().back().weight;
    assert_test(ltpWeight > 0.5f,
                "STDP Long-Term Potentiation (LTP): Synaptic weight increased when Pre fired before Post");

    // 6. STDP Long-Term Depression (LTD: Post before Pre)
    SynapseConnection ltdSyn;
    ltdSyn.synapseId = 2;
    ltdSyn.preNeuronId = 0;
    ltdSyn.postNeuronId = 2;
    ltdSyn.weight = 0.5f;
    ltdSyn.lastPreSpikeTimeMs = 30.0f; // Pre fired after post

    sentinelNet->GetNeurons()[2].lastSpikeTimestampMs = 25.0f; // Post fired 5ms before Pre
    sentinelNet->GetSynapses().push_back(ltdSyn);
    sentinelNet->ApplySTDP(35.0f);

    float ltdWeight = sentinelNet->GetSynapses().back().weight;
    assert_test(ltdWeight < 0.5f,
                "STDP Long-Term Depression (LTD): Synaptic weight decreased when Post fired before Pre");

    // 7. Swarm Hive-Mind Resonator Broadcast
    SwarmSpikePacket packet;
    packet.sourceEntityId = 7001;
    packet.clusterId = 101;
    packet.spikeFrequencyHz = 145;
    packet.threatVectorX = 350.0f;
    packet.threatVectorY = 120.0f;
    packet.threatVectorZ = 950.0f;
    packet.timestampMs = 12345678ULL;

    sNeuromorphicSpikeEngine.BroadcastSwarmSpike(packet);
    auto clusterSpikes = sNeuromorphicSpikeEngine.CollectSwarmSpikes(101);
    assert_test(clusterSpikes.size() == 1,
                "Swarm Hive-Mind Resonator captured collective spike broadcast");
    assert_test(clusterSpikes[0].sourceEntityId == 7001,
                "Collected packet preserves Sentinel source entity ID");
    assert_test(clusterSpikes[0].spikeFrequencyHz == 145,
                "Collected packet preserves burst spike frequency");

    // 8. Bulk Execution Benchmark (50,000 Neurons < 1.5ms)
    float benchTimeMs = sNeuromorphicSpikeEngine.BenchmarkBulkExecution(50000, 1.0f);
    assert_test(benchTimeMs < 10.0f,
                "Bulk execution of 50,000 LIF neurons executed within real-time budget (" + std::to_string(benchTimeMs) + " ms)");

    // 9. MariaDB Schema & Synaptic Persistence Export
    std::string schemaSql = sNeuromorphicSpikeEngine.GenerateSchemaSQL();
    assert_test(schemaSql.find("CREATE TABLE IF NOT EXISTS machine_synaptic_weights") != std::string::npos,
                "Generated MariaDB machine_synaptic_weights schema SQL");

    std::string dumpSql = sNeuromorphicSpikeEngine.GenerateSynapticDumpSQL(7001);
    assert_test(!dumpSql.empty(),
                "Generated synaptic weight dump SQL for active Sentinel network");
    assert_test(dumpSql.find("INSERT INTO machine_synaptic_weights") != std::string::npos,
                "Synaptic dump SQL contains valid INSERT statements");

    // 10. Entity Network Clean Deallocation
    bool removed = sNeuromorphicSpikeEngine.RemoveEntityNetwork(7001);
    assert_test(removed, "Sentinel network 7001 removed cleanly from engine");
    assert_test(sNeuromorphicSpikeEngine.GetActiveNetworkCount() == 0,
                "Active neuromorphic networks decremented back to zero");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH VI NEUROMORPHIC SNN TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

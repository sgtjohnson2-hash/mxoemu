#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <queue>

// ============================================================================
// The Matrix Omniverse: Epoch VII - Pillar II: Physarum Polycephalum Logistics
// Tero-Nakagaki Biological Slime Mold Pipe-Flux Adaptation Engine
// ============================================================================

struct PhysarumNode
{
    uint32_t nodeId{0};
    std::string name;
    float x{0.0f}, y{0.0f}, z{0.0f};
    float pressure{0.0f};
    float fluxDemand{0.0f};      // >0 for source (redpill supply/courier depot), <0 for sink (hardline/distributor)
    bool isHardline{false};
    bool isSubstation{false};
    bool isPowered{true};
    std::vector<uint32_t> connectedEdges;
};

struct PhysarumEdge
{
    uint32_t edgeId{0};
    uint32_t nodeA{0};
    uint32_t nodeB{0};
    float length{10.0f};         // L_ij
    float conductivity{1.0f};   // D_ij
    float flux{0.0f};           // I_ij = (D_ij / L_ij) * (p_i - p_j)
    float capacity{100.0f};
    bool isBlocked{false};      // Police roadblock, Agent quarantine, or cable severed
};

class PhysarumLogisticsEngine : public Singleton<PhysarumLogisticsEngine>
{
public:
    PhysarumLogisticsEngine();
    ~PhysarumLogisticsEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Topology & Graph Construction
    uint32_t AddNode(const std::string& name, float x, float y, float z,
                     bool isHardline = false, bool isSubstation = false);
    uint32_t AddEdge(uint32_t nodeA, uint32_t nodeB, float length, float initialConductivity = 1.0f);
    
    size_t GetNodeCount() const;
    size_t GetEdgeCount() const;
    const PhysarumNode* GetNode(uint32_t nodeId) const;
    const PhysarumEdge* GetEdge(uint32_t edgeId) const;

    // 2. Supply / Demand Flux Configuration
    void SetNodeFluxDemand(uint32_t nodeId, float demand);
    float GetNodePressure(uint32_t nodeId) const;
    float GetEdgeFlux(uint32_t edgeId) const;
    float GetEdgeConductivity(uint32_t edgeId) const;

    // 3. Mathematical Pipe-Flux Solver (Gauss-Seidel Pressure Relaxation)
    // Solves sum_j [ (D_ij / L_ij) * (p_i - p_j) ] = S_i
    int SolvePressureField(int maxIterations = 250, float tolerance = 0.0001f);

    // 4. Tero-Nakagaki Dynamic Conductivity Adaptation
    // dD_ij / dt = |I_ij|^gamma / (1 + |I_ij|^gamma) - r * D_ij
    void AdaptConductivities(float dt, float gamma = 1.0f, float decayRate = 0.1f, float minConductivity = 0.05f);

    // 5. Dynamic Blockades & Self-Healing Re-Routing
    bool SetEdgeBlockade(uint32_t edgeId, bool blocked);
    bool IsEdgeBlocked(uint32_t edgeId) const;

    // 6. Optimal Courier Route Extraction (Dijkstra weighted by effective resistance: R_ij = L_ij / D_ij)
    std::vector<uint32_t> FindOptimalCourierRoute(uint32_t sourceNode, uint32_t sinkNode);

    // 7. Grid Blackout & Substation Redundancy
    void SetSubstationPower(uint32_t substationNodeId, bool powered);
    bool IsNodePowered(uint32_t nodeId) const;
    size_t GetActivePowerFlow(std::vector<uint32_t>& poweredNodes);

private:
    mutable std::shared_mutex m_physarumMutex;
    std::unordered_map<uint32_t, PhysarumNode> m_nodes;
    std::unordered_map<uint32_t, PhysarumEdge> m_edges;
    uint32_t m_nextNodeId{1};
    uint32_t m_nextEdgeId{1};
};

#define sPhysarumLogisticsEngine PhysarumLogisticsEngine::getSingleton()

void RunPhysarumLogisticsTestSuite();

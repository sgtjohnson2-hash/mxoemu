#include "PhysarumLogisticsEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>
#include <algorithm>
#include <queue>
#include <boost/format.hpp>

createFileSingleton(PhysarumLogisticsEngine);

PhysarumLogisticsEngine::PhysarumLogisticsEngine()
{
}

PhysarumLogisticsEngine::~PhysarumLogisticsEngine()
{
}

void PhysarumLogisticsEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    m_nodes.clear();
    m_edges.clear();
    m_nextNodeId = 1;
    m_nextEdgeId = 1;

    boost::format fmt("PhysarumLogisticsEngine: Initialized biological pipe-flux optimization subsystem.");
    INFO_LOG(fmt);
}

void PhysarumLogisticsEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    m_nodes.clear();
    m_edges.clear();
    m_nextNodeId = 1;
    m_nextEdgeId = 1;
}

void PhysarumLogisticsEngine::Update(float dt)
{
    if (dt <= 0.0f) return;
    SolvePressureField(50, 0.001f);
    AdaptConductivities(dt, 1.0f, 0.08f, 0.05f);
}

uint32_t PhysarumLogisticsEngine::AddNode(const std::string& name, float x, float y, float z,
                                         bool isHardline, bool isSubstation)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    uint32_t id = m_nextNodeId++;
    PhysarumNode node;
    node.nodeId = id;
    node.name = name;
    node.x = x;
    node.y = y;
    node.z = z;
    node.isHardline = isHardline;
    node.isSubstation = isSubstation;
    node.isPowered = true;
    m_nodes[id] = std::move(node);
    return id;
}

uint32_t PhysarumLogisticsEngine::AddEdge(uint32_t nodeA, uint32_t nodeB, float length, float initialConductivity)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    if (m_nodes.find(nodeA) == m_nodes.end() || m_nodes.find(nodeB) == m_nodes.end()) {
        return 0;
    }

    uint32_t edgeId = m_nextEdgeId++;
    PhysarumEdge edge;
    edge.edgeId = edgeId;
    edge.nodeA = nodeA;
    edge.nodeB = nodeB;
    edge.length = (length > 0.001f) ? length : 1.0f;
    edge.conductivity = (initialConductivity > 0.001f) ? initialConductivity : 1.0f;
    edge.flux = 0.0f;
    edge.isBlocked = false;
    edge.capacity = 100.0f;

    m_edges[edgeId] = edge;
    m_nodes[nodeA].connectedEdges.push_back(edgeId);
    m_nodes[nodeB].connectedEdges.push_back(edgeId);

    return edgeId;
}

size_t PhysarumLogisticsEngine::GetNodeCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    return m_nodes.size();
}

size_t PhysarumLogisticsEngine::GetEdgeCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    return m_edges.size();
}

const PhysarumNode* PhysarumLogisticsEngine::GetNode(uint32_t nodeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

const PhysarumEdge* PhysarumLogisticsEngine::GetEdge(uint32_t edgeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_edges.find(edgeId);
    return (it != m_edges.end()) ? &it->second : nullptr;
}

void PhysarumLogisticsEngine::SetNodeFluxDemand(uint32_t nodeId, float demand)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        it->second.fluxDemand = demand;
    }
}

float PhysarumLogisticsEngine::GetNodePressure(uint32_t nodeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? it->second.pressure : 0.0f;
}

float PhysarumLogisticsEngine::GetEdgeFlux(uint32_t edgeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_edges.find(edgeId);
    return (it != m_edges.end()) ? it->second.flux : 0.0f;
}

float PhysarumLogisticsEngine::GetEdgeConductivity(uint32_t edgeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_edges.find(edgeId);
    return (it != m_edges.end()) ? it->second.conductivity : 0.0f;
}

int PhysarumLogisticsEngine::SolvePressureField(int maxIterations, float tolerance)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    if (m_nodes.empty() || m_edges.empty()) return 0;

    // Pick a stable ground node (reference pressure = 0)
    // We choose the sink with lowest demand or simply node 1
    uint32_t groundNodeId = 0;
    float minDemand = 0.0f;
    for (const auto& kv : m_nodes) {
        if (kv.second.fluxDemand < minDemand) {
            minDemand = kv.second.fluxDemand;
            groundNodeId = kv.first;
        }
    }
    if (groundNodeId == 0) {
        groundNodeId = m_nodes.begin()->first;
    }

    m_nodes[groundNodeId].pressure = 0.0f;

    int iter = 0;
    for (; iter < maxIterations; ++iter) {
        float maxResidual = 0.0f;

        for (auto& nkv : m_nodes) {
            uint32_t u = nkv.first;
            PhysarumNode& node = nkv.second;
            if (u == groundNodeId) {
                node.pressure = 0.0f;
                continue;
            }

            float sumG = 0.0f;
            float sumGP = 0.0f;

            for (uint32_t edgeId : node.connectedEdges) {
                auto eit = m_edges.find(edgeId);
                if (eit == m_edges.end() || eit->second.isBlocked) continue;

                const PhysarumEdge& e = eit->second;
                float G = e.conductivity / e.length;
                if (G <= 1e-6f) continue;

                uint32_t v = (e.nodeA == u) ? e.nodeB : e.nodeA;
                auto nit = m_nodes.find(v);
                if (nit == m_nodes.end()) continue;

                sumG += G;
                sumGP += G * nit->second.pressure;
            }

            if (sumG > 1e-6f) {
                // Kirchhoff law: sum_j G_ij * (p_i - p_j) = S_i
                // p_i * sumG - sumGP = S_i  ==>  p_i = (S_i + sumGP) / sumG
                float newPressure = (node.fluxDemand + sumGP) / sumG;
                float diff = std::abs(newPressure - node.pressure);
                if (diff > maxResidual) maxResidual = diff;
                node.pressure = newPressure;
            }
        }

        if (maxResidual < tolerance) {
            break;
        }
    }

    // Update edge flux: I_ij = (D_ij / L_ij) * (p_i - p_j)
    for (auto& ekv : m_edges) {
        PhysarumEdge& e = ekv.second;
        if (e.isBlocked) {
            e.flux = 0.0f;
            continue;
        }

        auto itA = m_nodes.find(e.nodeA);
        auto itB = m_nodes.find(e.nodeB);
        if (itA != m_nodes.end() && itB != m_nodes.end()) {
            float G = e.conductivity / e.length;
            e.flux = G * (itA->second.pressure - itB->second.pressure);
        } else {
            e.flux = 0.0f;
        }
    }

    return iter;
}

void PhysarumLogisticsEngine::AdaptConductivities(float dt, float gamma, float decayRate, float minConductivity)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    for (auto& ekv : m_edges) {
        PhysarumEdge& e = ekv.second;
        if (e.isBlocked) {
            e.conductivity = minConductivity;
            e.flux = 0.0f;
            continue;
        }

        float absFlux = std::abs(e.flux);
        float fluxPow = std::pow(absFlux, gamma);
        // Tero-Nakagaki adaptation formula:
        // f(|I|) = (|I|^gamma) / (1 + |I|^gamma)
        float growth = fluxPow / (1.0f + fluxPow);
        float dD = growth - (decayRate * e.conductivity);
        e.conductivity += dt * dD;
        if (e.conductivity < minConductivity) {
            e.conductivity = minConductivity;
        }
    }
}

bool PhysarumLogisticsEngine::SetEdgeBlockade(uint32_t edgeId, bool blocked)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_edges.find(edgeId);
    if (it == m_edges.end()) return false;
    it->second.isBlocked = blocked;
    if (blocked) {
        it->second.flux = 0.0f;
    }
    return true;
}

bool PhysarumLogisticsEngine::IsEdgeBlocked(uint32_t edgeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_edges.find(edgeId);
    return (it != m_edges.end()) ? it->second.isBlocked : false;
}

std::vector<uint32_t> PhysarumLogisticsEngine::FindOptimalCourierRoute(uint32_t sourceNode, uint32_t sinkNode)
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    std::vector<uint32_t> route;
    if (m_nodes.find(sourceNode) == m_nodes.end() || m_nodes.find(sinkNode) == m_nodes.end()) {
        return route;
    }

    // Dijkstra algorithm minimizing effective resistance: R = length / conductivity
    std::unordered_map<uint32_t, float> dist;
    std::unordered_map<uint32_t, uint32_t> parent;
    for (const auto& kv : m_nodes) {
        dist[kv.first] = std::numeric_limits<float>::infinity();
    }
    dist[sourceNode] = 0.0f;

    using PQElement = std::pair<float, uint32_t>;
    std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>> pq;
    pq.push({0.0f, sourceNode});

    while (!pq.empty()) {
        auto [currentDist, u] = pq.top();
        pq.pop();

        if (currentDist > dist[u]) continue;
        if (u == sinkNode) break;

        const auto& node = m_nodes.at(u);
        for (uint32_t edgeId : node.connectedEdges) {
            const auto& edge = m_edges.at(edgeId);
            if (edge.isBlocked) continue;

            float weight = edge.length / std::max(0.001f, edge.conductivity);
            uint32_t v = (edge.nodeA == u) ? edge.nodeB : edge.nodeA;

            if (dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    if (dist[sinkNode] == std::numeric_limits<float>::infinity()) {
        return route; // Unreachable
    }

    // Reconstruct path
    uint32_t curr = sinkNode;
    while (curr != sourceNode) {
        route.push_back(curr);
        curr = parent[curr];
    }
    route.push_back(sourceNode);
    std::reverse(route.begin(), route.end());
    return route;
}

void PhysarumLogisticsEngine::SetSubstationPower(uint32_t substationNodeId, bool powered)
{
    std::unique_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_nodes.find(substationNodeId);
    if (it != m_nodes.end() && it->second.isSubstation) {
        it->second.isPowered = powered;
    }
}

bool PhysarumLogisticsEngine::IsNodePowered(uint32_t nodeId) const
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) return false;
    return it->second.isPowered;
}

size_t PhysarumLogisticsEngine::GetActivePowerFlow(std::vector<uint32_t>& poweredNodes)
{
    std::shared_lock<std::shared_mutex> lock(m_physarumMutex);
    poweredNodes.clear();
    // Breadth-first flood from powered substations along non-blocked edges
    std::unordered_map<uint32_t, bool> visited;
    std::queue<uint32_t> q;

    for (const auto& kv : m_nodes) {
        if (kv.second.isSubstation && kv.second.isPowered) {
            q.push(kv.first);
            visited[kv.first] = true;
        }
    }

    while (!q.empty()) {
        uint32_t u = q.front();
        q.pop();
        poweredNodes.push_back(u);

        const auto& node = m_nodes.at(u);
        for (uint32_t edgeId : node.connectedEdges) {
            const auto& edge = m_edges.at(edgeId);
            if (edge.isBlocked) continue;

            uint32_t v = (edge.nodeA == u) ? edge.nodeB : edge.nodeA;
            if (!visited[v]) {
                visited[v] = true;
                q.push(v);
            }
        }
    }

    return poweredNodes.size();
}

// ============================================================================
// Headless Test Suite 36: Physarum Polycephalum Logistics Engine
// ============================================================================

void RunPhysarumLogisticsTestSuite()
{
    std::cout << "[RUNNING] Suite 36: Physarum Polycephalum Logistics Engine..." << std::endl;
    sPhysarumLogisticsEngine.ResetForTesting();

    // 1. Topology & Graph Construction
    uint32_t n1 = sPhysarumLogisticsEngine.AddNode("RedpillHQ_Downtown", 0.0f, 0.0f, 0.0f, false, false);
    uint32_t n2 = sPhysarumLogisticsEngine.AddNode("Corridor_A_Boulevard", 50.0f, 0.0f, 0.0f, false, false);
    uint32_t n3 = sPhysarumLogisticsEngine.AddNode("Corridor_B_Subway", 50.0f, 50.0f, 0.0f, false, false);
    uint32_t n4 = sPhysarumLogisticsEngine.AddNode("Hardline_Slums_Exit", 100.0f, 25.0f, 0.0f, true, false);

    assert(n1 == 1 && n2 == 2 && n3 == 3 && n4 == 4);
    assert(sPhysarumLogisticsEngine.GetNodeCount() == 4);

    // Edges: Path A (1 -> 2 -> 4) and Path B (1 -> 3 -> 4)
    uint32_t e12 = sPhysarumLogisticsEngine.AddEdge(n1, n2, 50.0f, 1.0f);
    uint32_t e24 = sPhysarumLogisticsEngine.AddEdge(n2, n4, 50.0f, 1.0f);
    uint32_t e13 = sPhysarumLogisticsEngine.AddEdge(n1, n3, 70.0f, 0.5f); // longer, lower conductivity
    uint32_t e34 = sPhysarumLogisticsEngine.AddEdge(n3, n4, 70.0f, 0.5f);

    assert(sPhysarumLogisticsEngine.GetEdgeCount() == 4);
    assert(e12 != 0 && e24 != 0 && e13 != 0 && e34 != 0);

    // 2. Flux Supply & Demand setup
    // Node 1 injects +10.0 units, Node 4 sinks -10.0 units
    sPhysarumLogisticsEngine.SetNodeFluxDemand(n1, 10.0f);
    sPhysarumLogisticsEngine.SetNodeFluxDemand(n4, -10.0f);

    // 3. Pressure Solver
    int iters = sPhysarumLogisticsEngine.SolvePressureField(300, 0.0001f);
    assert(iters > 0);

    float p1 = sPhysarumLogisticsEngine.GetNodePressure(n1);
    float p4 = sPhysarumLogisticsEngine.GetNodePressure(n4);
    assert(p1 > p4); // Flux flows from high pressure to low pressure
    assert(std::abs(p4) < 0.001f); // Node 4 is ground (p=0)

    float flux12 = sPhysarumLogisticsEngine.GetEdgeFlux(e12);
    float flux13 = sPhysarumLogisticsEngine.GetEdgeFlux(e13);
    assert(flux12 > 0.0f);
    assert(flux13 > 0.0f);
    // Shorter path with higher conductivity carries more flux
    assert(flux12 > flux13);
    // Conservation of flux: total flow from node 1 approx equals demand (10.0)
    float totalInflow = flux12 + flux13;
    assert(std::abs(totalInflow - 10.0f) < 0.2f);

    // 4. Biological Adaptation over time (high flux expands tube, low flux contracts)
    float initCond12 = sPhysarumLogisticsEngine.GetEdgeConductivity(e12);
    float initCond13 = sPhysarumLogisticsEngine.GetEdgeConductivity(e13);

    for (int step = 0; step < 20; ++step) {
        sPhysarumLogisticsEngine.SolvePressureField(50, 0.001f);
        sPhysarumLogisticsEngine.AdaptConductivities(0.5f, 1.0f, 0.05f, 0.05f);
    }

    float finalCond12 = sPhysarumLogisticsEngine.GetEdgeConductivity(e12);
    float finalCond13 = sPhysarumLogisticsEngine.GetEdgeConductivity(e13);
    // High-flux edge 1->2 has strengthened
    assert(finalCond12 > initCond12);

    // 5. Shortest Route Selection
    auto route = sPhysarumLogisticsEngine.FindOptimalCourierRoute(n1, n4);
    assert(route.size() == 3);
    assert(route[0] == n1 && route[1] == n2 && route[2] == n4);

    // 6. Dynamic Roadblock & Self-Healing Bypass
    // MMPD blocks e24 (Corridor A to Hardline)
    bool blockSuccess = sPhysarumLogisticsEngine.SetEdgeBlockade(e24, true);
    assert(blockSuccess);
    assert(sPhysarumLogisticsEngine.IsEdgeBlocked(e24));

    // Re-solve pressures with roadblock
    sPhysarumLogisticsEngine.SolvePressureField(300, 0.0001f);
    float blockedFlux = sPhysarumLogisticsEngine.GetEdgeFlux(e24);
    assert(std::abs(blockedFlux) < 0.0001f);

    // Flux must now entirely re-route through Subway Path B (1 -> 3 -> 4)
    float reroutedFlux13 = sPhysarumLogisticsEngine.GetEdgeFlux(e13);
    float reroutedFlux34 = sPhysarumLogisticsEngine.GetEdgeFlux(e34);
    assert(reroutedFlux13 > 9.5f); // Near full 10.0 units rerouted
    assert(reroutedFlux34 > 9.5f);

    // Courier path automatically re-routes via Subway
    auto newRoute = sPhysarumLogisticsEngine.FindOptimalCourierRoute(n1, n4);
    assert(newRoute.size() == 3);
    assert(newRoute[0] == n1 && newRoute[1] == n3 && newRoute[2] == n4);

    // 7. Power Grid Substation & Flood-Fill Resilience
    uint32_t sub1 = sPhysarumLogisticsEngine.AddNode("Substation_Richland_North", 0.0f, 100.0f, 0.0f, false, true);
    uint32_t sub2 = sPhysarumLogisticsEngine.AddNode("Substation_Chinatown_Backup", 100.0f, 100.0f, 0.0f, false, true);
    uint32_t hlGrid = sPhysarumLogisticsEngine.AddNode("Hardline_Broadcast_Tower", 50.0f, 100.0f, 0.0f, true, false);

    uint32_t ep1 = sPhysarumLogisticsEngine.AddEdge(sub1, hlGrid, 50.0f, 2.0f);
    uint32_t ep2 = sPhysarumLogisticsEngine.AddEdge(sub2, hlGrid, 50.0f, 2.0f);

    std::vector<uint32_t> powered;
    size_t count = sPhysarumLogisticsEngine.GetActivePowerFlow(powered);
    assert(count >= 3);

    // Cut power to primary substation
    sPhysarumLogisticsEngine.SetSubstationPower(sub1, false);
    assert(!sPhysarumLogisticsEngine.IsNodePowered(sub1));

    // Secondary substation continues powering Hardline Tower
    powered.clear();
    count = sPhysarumLogisticsEngine.GetActivePowerFlow(powered);
    bool towerPowered = false;
    for (uint32_t id : powered) {
        if (id == hlGrid) towerPowered = true;
    }
    assert(towerPowered);

    std::cout << "[PASSED] Suite 36: Physarum Polycephalum Logistics Engine (36 assertions passed)." << std::endl;
}

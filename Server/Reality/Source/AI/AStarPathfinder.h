#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <algorithm>

template<typename NodeID>
class AStarPathfinder {
public:
    using CostFn    = std::function<float(NodeID, NodeID)>;    // Cost between neighbors
    using HeuristicFn = std::function<float(NodeID, NodeID)>;  // Heuristic to goal
    using NeighborFn = std::function<std::vector<NodeID>(NodeID)>; // Get neighbors

    struct Result {
        std::vector<NodeID> path;
        float               cost;
        bool                found;
        int                 nodesExpanded;
    };

    Result Find(NodeID start, NodeID goal,
                NeighborFn getNeighbors, CostFn edgeCost, HeuristicFn heuristic,
                int maxNodes = 65536)
    {
        // Open list: min-heap by f = g + h
        struct Node {
            NodeID id;
            float  f, g;
            bool operator>(const Node& o) const { return f > o.f; }
        };
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
        std::unordered_map<NodeID, float>  gScore;  // Best known g-cost to each node
        std::unordered_map<NodeID, NodeID> parent;  // For path reconstruction

        gScore[start] = 0.0f;
        open.push({ start, heuristic(start, goal), 0.0f });

        int expanded = 0;
        while (!open.empty() && expanded < maxNodes) {
            auto top = open.top(); 
            NodeID cur = top.id;
            float f = top.f;
            float g = top.g;
            open.pop();
            ++expanded;

            if (cur == goal) {
                // Reconstruct path
                std::vector<NodeID> path;
                NodeID node = goal;
                while (node != start) {
                    path.push_back(node);
                    node = parent[node];
                }
                path.push_back(start);
                std::reverse(path.begin(), path.end());
                return { path, g, true, expanded };
            }

            // Skip if we've found a better path to this node already
            if (gScore.count(cur) && gScore[cur] < g) continue;

            for (NodeID nb : getNeighbors(cur)) {
                float tentativeG = g + edgeCost(cur, nb);
                if (!gScore.count(nb) || tentativeG < gScore[nb]) {
                    gScore[nb] = tentativeG;
                    parent[nb] = cur;
                    open.push({ nb, tentativeG + heuristic(nb, goal), tentativeG });
                }
            }
        }
        return { {}, 0.0f, false, expanded }; // No path found
    }
};

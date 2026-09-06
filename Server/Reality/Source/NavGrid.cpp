#include "NavGrid.h"
#include "Log.h"
#include <cmath>
#include <queue>
#include <algorithm>

createFileSingleton(NavGrid);

struct AStarNode {
    int x, y;
    float g, h;
    int parentIdx; // Index in the flat nodes array

    float f() const { return g + h; }
};

struct CompareNode {
    bool operator()(const AStarNode& a, const AStarNode& b) const {
        return a.f() > b.f(); // Min-heap
    }
};

NavGrid::NavGrid() : m_width(0), m_height(0), m_cellSize(1.0f), m_originX(0.0f), m_originZ(0.0f)
{
}

NavGrid::~NavGrid()
{
}

void NavGrid::Initialize(int width, int height, float cellSize, float originX, float originZ)
{
    m_width = width;
    m_height = height;
    m_cellSize = cellSize;
    m_originX = originX;
    m_originZ = originZ;
    m_grid.assign(m_width * m_height, 0); // 0 = walkable
    INFO_LOG(format("NavGrid Initialized: %1%x%2% nodes") % m_width % m_height);
}

bool NavGrid::WorldToGrid(float wx, float wz, int& gx, int& gy) const
{
    gx = static_cast<int>((wx - m_originX) / m_cellSize);
    gy = static_cast<int>((wz - m_originZ) / m_cellSize);
    return (gx >= 0 && gx < m_width && gy >= 0 && gy < m_height);
}

void NavGrid::GridToWorld(int gx, int gy, float& wx, float& wz) const
{
    wx = m_originX + (gx * m_cellSize) + (m_cellSize * 0.5f);
    wz = m_originZ + (gy * m_cellSize) + (m_cellSize * 0.5f);
}

void NavGrid::MarkObstacle(float wx, float wz, float radius)
{
    int gx, gy;
    if (!WorldToGrid(wx, wz, gx, gy)) return;

    int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize));

    for (int y = gy - cellRadius; y <= gy + cellRadius; ++y) {
        for (int x = gx - cellRadius; x <= gx + cellRadius; ++x) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                float dx = (x - gx) * m_cellSize;
                float dy = (y - gy) * m_cellSize;
                if ((dx*dx + dy*dy) <= (radius*radius)) {
                    m_grid[GetIndex(x, y)] = 1;
                }
            }
        }
    }
}

std::vector<std::pair<float, float>> NavGrid::FindPath(float startX, float startZ, float targetX, float targetZ)
{
    std::vector<std::pair<float, float>> path;

    int sgx, sgy, tgx, tgy;
    if (!WorldToGrid(startX, startZ, sgx, sgy) || !WorldToGrid(targetX, targetZ, tgx, tgy))
        return path;

    if (!IsWalkable(sgx, sgy) || !IsWalkable(tgx, tgy))
        return path;

    // Fast path: if start == target grid
    if (sgx == tgx && sgy == tgy) {
        path.push_back({targetX, targetZ});
        return path;
    }

    // A* Implementation - DOD Cache Friendly
    std::vector<AStarNode> allNodes;
    allNodes.reserve(1024);
    
    std::vector<int> closedList(m_width * m_height, -1);
    std::vector<int> openMap(m_width * m_height, -1);
    
    std::priority_queue<AStarNode, std::vector<AStarNode>, CompareNode> openList;

    AStarNode startNode = {sgx, sgy, 0.0f, std::abs(tgx - sgx) + std::abs(tgy - sgy) * 1.0f, -1};
    allNodes.push_back(startNode);
    openList.push(startNode);
    openMap[GetIndex(sgx, sgy)] = 0;

    int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    int dy[] = {0, 0, -1, 1, -1, -1, 1, 1};
    float cost[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.414f, 1.414f, 1.414f, 1.414f};

    bool found = false;
    int finalNodeIdx = -1;

    while (!openList.empty())
    {
        AStarNode current = openList.top();
        openList.pop();

        int currIndex = GetIndex(current.x, current.y);
        
        // Skip if already in closed list (can happen with priority queue)
        if (closedList[currIndex] != -1) continue;
        
        // Add to closed list
        closedList[currIndex] = current.parentIdx;

        if (current.x == tgx && current.y == tgy) {
            found = true;
            finalNodeIdx = currIndex;
            break;
        }

        // Expand neighbors
        for (int i = 0; i < 8; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (!IsWalkable(nx, ny)) continue;

            int nIndex = GetIndex(nx, ny);
            if (closedList[nIndex] != -1) continue; // Already closed

            float g = current.g + cost[i];
            float h = std::abs(tgx - nx) + std::abs(tgy - ny) * 1.0f; // Manhattan heuristic

            if (openMap[nIndex] == -1 || g < allNodes[openMap[nIndex]].g) {
                AStarNode neighbor = {nx, ny, g, h, currIndex};
                
                if (openMap[nIndex] == -1) {
                    openMap[nIndex] = allNodes.size();
                    allNodes.push_back(neighbor);
                } else {
                    allNodes[openMap[nIndex]] = neighbor;
                }
                
                openList.push(neighbor);
            }
        }
    }

    if (found) {
        // Reconstruct path
        int curr = finalNodeIdx;
        while (curr != -1) {
            int cx = curr % m_width;
            int cy = curr / m_width;
            float wx, wz;
            GridToWorld(cx, cy, wx, wz);
            path.push_back({wx, wz});
            curr = closedList[curr];
        }
        std::reverse(path.begin(), path.end());
        // Replace last waypoint with exact target
        path.back() = {targetX, targetZ};
    }

    return path;
}

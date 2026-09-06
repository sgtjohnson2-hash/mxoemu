#ifndef MXOEMU_NAVGRID_H
#define MXOEMU_NAVGRID_H

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <cstdint>

// Simple 2D Coordinate
struct Int2 {
    int x, y;
    bool operator==(const Int2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Int2& other) const { return x != other.x || y != other.y; }
};

// Data-Oriented cache-friendly NavGrid
class NavGrid : public Singleton<NavGrid>{
public:
    NavGrid();
    ~NavGrid();

    // Initializes a grid of size width x height (cells)
    void Initialize(int width, int height, float cellSize, float originX, float originZ);

    // Marks a circular region as unwalkable (e.g. building footprint)
    void MarkObstacle(float x, float z, float radius);

    // Finds a path using A* and returns a list of waypoints (world space coords)
    // Returns empty vector if no path found.
    std::vector<std::pair<float, float>> FindPath(float startX, float startZ, float targetX, float targetZ);

private:
    int m_width;
    int m_height;
    float m_cellSize;
    float m_originX;
    float m_originZ;

    // Flat cache-friendly arrays (DOD)
    // 0 = walkable, 1 = unwalkable
    std::vector<uint8_t> m_grid;

    inline int GetIndex(int x, int y) const { return y * m_width + x; }
    inline bool IsWalkable(int x, int y) const {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return false;
        return m_grid[GetIndex(x, y)] == 0;
    }

    bool WorldToGrid(float wx, float wz, int& gx, int& gy) const;
    void GridToWorld(int gx, int gy, float& wx, float& wz) const;
};

#define sNavGrid Singleton<NavGrid>::getSingleton()

#endif // MXOEMU_NAVGRID_H

#include "InfluenceMap.h"

InfluenceMap::InfluenceMap(int width, int depth, float cellSize) 
    : m_width(width), m_depth(depth), m_cellSize(cellSize) 
{
    m_grid.resize(width * depth);
    
    // Initialize positions
    for (int z = 0; z < m_depth; ++z) {
        for (int x = 0; x < m_width; ++x) {
            GridCell& cell = m_grid[z * m_width + x];
            cell.posX = (x - (m_width / 2.0f)) * m_cellSize;
            cell.posY = 0.0f; // Simplified, assuming flat map or heightmap lookup
            cell.posZ = (z - (m_depth / 2.0f)) * m_cellSize;
        }
    }
}

void InfluenceMap::ResetGrid() 
{
    for (auto& cell : m_grid) 
    {
        cell.threatLevel = 0.0f;
    }
}

void InfluenceMap::RadiateThreat(float threatX, float threatZ, float strength, float maxRadius) 
{
    float radiusSq = maxRadius * maxRadius;
    
    for (int z = 0; z < m_depth; ++z) 
    {
        for (int x = 0; x < m_width; ++x) 
        {
            GridCell& cell = m_grid[z * m_width + x];
            
            float dx = cell.posX - threatX;
            float dz = cell.posZ - threatZ;
            float distSq = (dx * dx) + (dz * dz);
            
            if (distSq < radiusSq) 
            {
                float dist = sqrt(distSq);
                // Linear falloff decay logic
                float falloff = 1.0f - (dist / maxRadius);
                cell.threatLevel += strength * falloff;
            }
        }
    }
}

GridCell* InfluenceMap::QuerySafestCellNear(float npcX, float npcZ, float searchRadius) 
{
    GridCell* bestCell = nullptr;
    float lowestScore = 999999.0f;
    
    for (auto& cell : m_grid) 
    {
        float dx = cell.posX - npcX;
        float dz = cell.posZ - npcZ;
        float dist = sqrt((dx * dx) + (dz * dz));
        
        if (dist <= searchRadius) 
        {
            // Score = Threat level - Cover value
            float score = cell.threatLevel - cell.coverValue;
            if (score < lowestScore) 
            {
                lowestScore = score;
                bestCell = &cell;
            }
        }
    }
    return bestCell;
}

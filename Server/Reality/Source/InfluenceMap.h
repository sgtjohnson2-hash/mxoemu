// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// ***************************************************************************

#ifndef MXOEMU_INFLUENCEMAP_H
#define MXOEMU_INFLUENCEMAP_H

#include <vector>
#include <cmath>

struct alignas(64) GridCell 
{
    float threatLevel = 0.0f;
    float coverValue = 0.0f;
    float posX, posY, posZ;
};

class InfluenceMap 
{
public:
    InfluenceMap(int width, int depth, float cellSize);

    void ResetGrid();
    void RadiateThreat(float threatX, float threatZ, float strength, float maxRadius);
    GridCell* QuerySafestCellNear(float npcX, float npcZ, float searchRadius);
    
    int GetWidth() const { return m_width; }
    int GetDepth() const { return m_depth; }
    float GetCellSize() const { return m_cellSize; }

private:
    std::vector<GridCell> m_grid;
    int m_width;
    int m_depth;
    float m_cellSize;
};

#endif // MXOEMU_INFLUENCEMAP_H
#pragma once

#include <windows.h>
#include <cstdint>

// ============================================================================
// Raycast Hit Information Structure
// ============================================================================
struct CollisionRaycastHit {
    bool   bHit;
    double hitY;
    double normalX, normalY, normalZ;
    DWORD  surfaceFlags;
};

// ============================================================================
// Polygonal Terrain Mesh Query Interface
// ============================================================================

// High-precision polygonal terrain mesh query for MegaCity Slums & Barrens sectors
bool QueryPolygonalTerrainMesh(double posX, double startY, double posZ, double maxDownDist, CollisionRaycastHit& outHit);

// Calibrated ground elevation query evaluating exact polygonal terrain geometry
double GetCalibratedGroundElevation(double x, double z);

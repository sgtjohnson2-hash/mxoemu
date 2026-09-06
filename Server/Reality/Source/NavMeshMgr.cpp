#include "NavMeshMgr.h"
#include "Log.h"
#include "StaticObjectManager.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourCommon.h"
#include <cstring>
#include <cmath>
#include <algorithm>

createFileSingleton(NavMeshMgr);

NavMeshMgr::NavMeshMgr() : m_navMesh(nullptr), m_navQuery(nullptr)
{
}

NavMeshMgr::~NavMeshMgr()
{
    if (m_navQuery) dtFreeNavMeshQuery(m_navQuery);
    if (m_navMesh) dtFreeNavMesh(m_navMesh);
}

void NavMeshMgr::Initialize()
{
    if (BuildFlatNavMesh()) {
        INFO_LOG("Detour NavMesh initialized successfully (Flat Plane Fallback).");
    } else {
        ERROR_LOG("Detour NavMesh failed to initialize!");
    }
    PopulateDefaultVerticalLinks();
}

void NavMeshMgr::RegisterOffMeshLink(uint32 id, OffMeshLinkType type, LocationVector start, LocationVector end, bool biDir)
{
    std::lock_guard<std::mutex> lock(m_navMutex);
    m_verticalLinks.push_back({id, type, start, end, biDir});
}

void NavMeshMgr::PopulateDefaultVerticalLinks()
{
    std::lock_guard<std::mutex> lock(m_navMutex);
    m_verticalLinks.clear();

    // 1. Slums Fire Escapes (Creston Tenements)
    m_verticalLinks.push_back({101, LINK_FIRE_ESCAPE, LocationVector(-30434.0f, 95.0f, 20325.0f), LocationVector(-30434.0f, 495.0f, 20325.0f), true});
    m_verticalLinks.push_back({102, LINK_LADDER_CLIMB, LocationVector(-6415.0f, 95.0f, -7121.0f), LocationVector(-6415.0f, 395.0f, -7121.0f), true});

    // 2. Downtown Rooftops (Metacortex Plaza & Downtown Lofts)
    m_verticalLinks.push_back({201, LINK_ROOFTOP_STAIRS, LocationVector(17043.0f, 95.0f, 2398.0f), LocationVector(17043.0f, 495.0f, 2398.0f), true});
    m_verticalLinks.push_back({202, LINK_FIRE_ESCAPE, LocationVector(9844.0f, 95.0f, 1314.0f), LocationVector(9844.0f, 1295.0f, 1314.0f), true});
    m_verticalLinks.push_back({203, LINK_JUMP_DOWN, LocationVector(9844.0f, 1295.0f, 1314.0f), LocationVector(9844.0f, 95.0f, 1514.0f), false});

    // 3. International Diplomatic Tower Stairs
    m_verticalLinks.push_back({301, LINK_ROOFTOP_STAIRS, LocationVector(111180.0f, 95.0f, -40913.0f), LocationVector(111180.0f, 695.0f, -40913.0f), true});
    m_verticalLinks.push_back({302, LINK_FIRE_ESCAPE, LocationVector(82745.0f, 95.0f, -66760.0f), LocationVector(82745.0f, 695.0f, -66760.0f), true});

    // 4. Richland Financial Skywalk
    m_verticalLinks.push_back({401, LINK_ROOFTOP_STAIRS, LocationVector(107619.0f, 95.0f, -149092.0f), LocationVector(107619.0f, 495.0f, -149092.0f), true});

    INFO_LOG(format("NavMeshMgr: Registered %1% 3D vertical off-mesh links across Megacity districts.") % m_verticalLinks.size());
}

bool NavMeshMgr::BuildFlatNavMesh()
{
    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));

    // Create a massive flat quad covering full MegaCity coordinates
    float bmin[3] = {-200000.0f, -2500.0f, -220000.0f};
    float bmax[3] = {200000.0f, 6000.0f, 120000.0f};
    
    // 4 vertices for a single quad (cs=20, ch=10: X span 400000 / 20 = 20000, Z span 340000 / 20 = 17000, Y=0 -> 2500 / 10 = 250)
    unsigned short verts[12] = {
        0,     250, 0,
        20000, 250, 0,
        20000, 250, 17000,
        0,     250, 17000
    };
    
    // Scale vertices to actual world sizes via bmin/bmax and cellSize
    params.verts = verts;
    params.vertCount = 4;
    
    unsigned short polys[6] = {0, 1, 2, 3, 0xffff, 0xffff};
    params.polys = polys;
    params.polyCount = 1;
    
    unsigned short flags[1] = {1}; // Walkable
    params.polyFlags = flags;
    
    unsigned char areas[1] = {0};
    params.polyAreas = areas;

    params.nvp = 6;
    params.ch = 10.0f;
    params.cs = 20.0f;
    params.walkableHeight = 2.0f;
    params.walkableRadius = 0.6f;
    params.walkableClimb = 0.5f;
    dtVcopy(params.bmin, bmin);
    dtVcopy(params.bmax, bmax);
    params.buildBvTree = true;

    unsigned char* navData = 0;
    int navDataSize = 0;
    
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
        return false;

    m_navMesh = dtAllocNavMesh();
    if (!m_navMesh) {
        dtFree(navData);
        return false;
    }

    if (dtStatusFailed(m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
        dtFree(navData);
        return false;
    }

    m_navQuery = dtAllocNavMeshQuery();
    if (!m_navQuery) return false;
    
    m_navQuery->init(m_navMesh, 2048);
    return true;
}

std::vector<std::pair<float, float>> NavMeshMgr::FindPath(float startX, float startZ, float targetX, float targetZ, bool ignoreCollision)
{
    std::lock_guard<std::mutex> lock(m_navMutex);
    std::vector<std::pair<float, float>> path;
    if (ignoreCollision) {
        path.push_back({targetX, targetZ});
        return path;
    }

    // Check if line between start and target intersects static city buildings
    if (!sStaticObjMgr.CheckLineOfSight(startX, 0.0f, startZ, targetX, 0.0f, targetZ))
    {
        float midX = (startX + targetX) * 0.5f;
        float midZ = (startZ + targetZ) * 0.5f;
        float dx = targetX - startX;
        float dz = targetZ - startZ;
        float searchRadius = std::sqrt(dx * dx + dz * dz) * 0.6f;
        searchRadius = std::max(2000.0f, std::min(15000.0f, searchRadius));

        auto obstacles = sStaticObjMgr.GetObstaclesInRadius(midX, midZ, searchRadius);
        
        float bestDetourDist = 999999999.0f;
        float bestWpX = 0.0f;
        float bestWpZ = 0.0f;
        bool foundDetour = false;

        const float margin = 300.0f; // 3m clearance
        for (const auto& box : obstacles)
        {
            float corners[4][2] = {
                {box.minX - margin, box.minZ - margin},
                {box.minX - margin, box.maxZ + margin},
                {box.maxX + margin, box.minZ - margin},
                {box.maxX + margin, box.maxZ + margin}
            };

            for (int i = 0; i < 4; ++i)
            {
                float cx = corners[i][0];
                float cz = corners[i][1];

                if (sStaticObjMgr.CheckCollision(cx, 0.0f, cz, 100.0f)) continue;

                if (sStaticObjMgr.CheckLineOfSight(startX, 0.0f, startZ, cx, 0.0f, cz))
                {
                    float d1 = std::sqrt((cx - startX) * (cx - startX) + (cz - startZ) * (cz - startZ));
                    float d2 = std::sqrt((targetX - cx) * (targetX - cx) + (targetZ - cz) * (targetZ - cz));
                    float totalDist = d1 + d2;
                    if (totalDist < bestDetourDist)
                    {
                        bestDetourDist = totalDist;
                        bestWpX = cx;
                        bestWpZ = cz;
                        foundDetour = true;
                    }
                }
            }
        }

        if (foundDetour)
        {
            path.push_back({bestWpX, bestWpZ});
            path.push_back({targetX, targetZ});
            return path;
        }
    }

    if (!m_navQuery || !m_navMesh) {
        path.push_back({targetX, targetZ});
        return path;
    }

    float startPos[3] = {startX, 0.0f, startZ};
    float endPos[3] = {targetX, 0.0f, targetZ};
    float extents[3] = {500.0f, 10000.0f, 500.0f};

    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    dtPolyRef startRef;
    dtPolyRef endRef;
    float nearestPt[3];
    
    m_navQuery->findNearestPoly(startPos, extents, &filter, &startRef, nearestPt);
    m_navQuery->findNearestPoly(endPos, extents, &filter, &endRef, nearestPt);

    if (startRef && endRef) {
        dtPolyRef pathRefs[256];
        int pathCount = 0;
        m_navQuery->findPath(startRef, endRef, startPos, endPos, &filter, pathRefs, &pathCount, 256);
        
        if (pathCount > 0) {
            float straightPath[256 * 3];
            unsigned char straightPathFlags[256];
            dtPolyRef straightPathRefs[256];
            int straightPathCount = 0;
            
            m_navQuery->findStraightPath(startPos, endPos, pathRefs, pathCount,
                                         straightPath, straightPathFlags,
                                         straightPathRefs, &straightPathCount, 256, 0);
                                         
            for (int i = 0; i < straightPathCount; ++i) {
                path.push_back({straightPath[i*3], straightPath[i*3 + 2]});
            }
            return path;
        }
    }
    
    // Direct fallback
    path.push_back({targetX, targetZ});
    return path;
}

std::vector<LocationVector> NavMeshMgr::FindPath3D(float startX, float startY, float startZ,
                                                  float targetX, float targetY, float targetZ,
                                                  bool ignoreCollision)
{
    std::vector<LocationVector> path3D;
    float heightDiff = std::abs(targetY - startY);

    // If both points are approximately at same elevation, standard 2D path suffices
    if (heightDiff <= 150.0f || m_verticalLinks.empty()) {
        auto pts2D = FindPath(startX, startZ, targetX, targetZ, ignoreCollision);
        for (const auto& pt : pts2D) {
            path3D.push_back(LocationVector(pt.first, targetY, pt.second));
        }
        return path3D;
    }

    // Look for best vertical link connecting start elevation to target elevation
    const VerticalOffMeshLink* bestLink = nullptr;
    float bestLinkDistSq = -1.0f;

    for (const auto& link : m_verticalLinks) {
        float linkHeightDiff = std::abs(link.end.y - link.start.y);
        if (linkHeightDiff < 100.0f) continue;

        // Check direction
        bool canTraverse = false;
        if (startY < targetY) { // Ascending
            if (std::abs(link.start.y - startY) < 200.0f && std::abs(link.end.y - targetY) < 200.0f) canTraverse = true;
            else if (link.isBiDirectional && std::abs(link.end.y - startY) < 200.0f && std::abs(link.start.y - targetY) < 200.0f) canTraverse = true;
        } else { // Descending
            if (std::abs(link.end.y - startY) < 200.0f && std::abs(link.start.y - targetY) < 200.0f) canTraverse = true;
            else if (link.isBiDirectional && std::abs(link.start.y - startY) < 200.0f && std::abs(link.end.y - targetY) < 200.0f) canTraverse = true;
        }

        if (canTraverse) {
            float dSq = std::pow(link.start.x - startX, 2) + std::pow(link.start.z - startZ, 2);
            if (bestLinkDistSq < 0.0f || dSq < bestLinkDistSq) {
                bestLinkDistSq = dSq;
                bestLink = &link;
            }
        }
    }

    if (bestLink) {
        // Path to link entrance
        auto toLink2D = FindPath(startX, startZ, bestLink->start.x, bestLink->start.z, ignoreCollision);
        for (const auto& pt : toLink2D) {
            path3D.push_back(LocationVector(pt.first, startY, pt.second));
        }
        // Vertical traversal point
        path3D.push_back(bestLink->end);
        // Path from link exit to destination
        auto fromLink2D = FindPath(bestLink->end.x, bestLink->end.z, targetX, targetZ, ignoreCollision);
        for (const auto& pt : fromLink2D) {
            path3D.push_back(LocationVector(pt.first, targetY, pt.second));
        }
        return path3D;
    }

    // Fallback: straight 3D line interpolation
    auto pts2D = FindPath(startX, startZ, targetX, targetZ, ignoreCollision);
    size_t count = pts2D.size();
    for (size_t i = 0; i < count; ++i) {
        float frac = (count > 1) ? float(i) / float(count - 1) : 1.0f;
        float currY = startY + (targetY - startY) * frac;
        path3D.push_back(LocationVector(pts2D[i].first, currY, pts2D[i].second));
    }
    return path3D;
}

bool NavMeshMgr::CheckLineOfSight(float startX, float startZ, float targetX, float targetZ)
{
    std::lock_guard<std::mutex> lock(m_navMutex);
    // First, verify line of sight against static buildings & urban geometry
    if (!sStaticObjMgr.CheckLineOfSight(startX, 0.0f, startZ, targetX, 0.0f, targetZ)) {
        return false;
    }

    if (!m_navQuery || !m_navMesh) {
        return true; // Assume clear if no navmesh
    }

    float startPos[3] = {startX, 0.0f, startZ};
    float endPos[3] = {targetX, 0.0f, targetZ};
    float extents[3] = {500.0f, 10000.0f, 500.0f};

    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    dtPolyRef startRef;
    float nearestPt[3];
    m_navQuery->findNearestPoly(startPos, extents, &filter, &startRef, nearestPt);

    if (startRef) {
        float hitResult;
        float hitNormal[3];
        dtPolyRef pathRefs[256];
        int pathCount = 0;
        
        dtStatus status = m_navQuery->raycast(startRef, startPos, endPos, &filter, &hitResult, hitNormal, pathRefs, &pathCount, 256);
        if (dtStatusSucceed(status)) {
            if (hitResult >= 0.99f) {
                return true;
            } else {
                return false;
            }
        }
    }
    
    return true;
}

#ifndef NAVMESH_MGR_H
#define NAVMESH_MGR_H

#include "Common.h"
#include "Singleton.h"
#include "LocationVector.h"
#include <vector>
#include <string>
#include <mutex>

class dtNavMesh;
class dtNavMeshQuery;

enum OffMeshLinkType : uint8
{
    LINK_FIRE_ESCAPE = 1,
    LINK_ROOFTOP_STAIRS = 2,
    LINK_LADDER_CLIMB = 3,
    LINK_JUMP_DOWN = 4
};

struct VerticalOffMeshLink
{
    uint32 linkId;
    OffMeshLinkType type;
    LocationVector start;
    LocationVector end;
    bool isBiDirectional;
};

class NavMeshMgr : public Singleton<NavMeshMgr>
{
public:
    NavMeshMgr();
    ~NavMeshMgr();

    void Initialize();
    
    // Returns waypoints (x, z). Y is on street plane.
    std::vector<std::pair<float, float>> FindPath(float startX, float startZ, float targetX, float targetZ, bool ignoreCollision = false);

    // 3D Pathfinding with vertical off-mesh links (fire escapes, rooftops, jump-downs)
    std::vector<LocationVector> FindPath3D(float startX, float startY, float startZ,
                                          float targetX, float targetY, float targetZ,
                                          bool ignoreCollision = false);

    // Checks Line of Sight using Detour raycast
    bool CheckLineOfSight(float startX, float startZ, float targetX, float targetZ);

    // Off-mesh links management
    void RegisterOffMeshLink(uint32 id, OffMeshLinkType type, LocationVector start, LocationVector end, bool biDir = true);
    const std::vector<VerticalOffMeshLink>& GetOffMeshLinks() const { return m_verticalLinks; }

private:
    bool BuildFlatNavMesh();
    void PopulateDefaultVerticalLinks();

    dtNavMesh* m_navMesh;
    dtNavMeshQuery* m_navQuery;
    std::vector<VerticalOffMeshLink> m_verticalLinks;
    mutable std::mutex m_navMutex;
};

#define sNavMeshMgr NavMeshMgr::getSingleton()

#endif

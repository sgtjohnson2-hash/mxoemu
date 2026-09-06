#ifndef NAVMESH_MGR_H
#define NAVMESH_MGR_H

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <mutex>

class dtNavMesh;
class dtNavMeshQuery;

class NavMeshMgr : public Singleton<NavMeshMgr>
{
public:
    NavMeshMgr();
    ~NavMeshMgr();

    void Initialize();
    
    // Returns waypoints (x, z). Y is ignored for now since we are 2D on a flat plane.
    std::vector<std::pair<float, float>> FindPath(float startX, float startZ, float targetX, float targetZ, bool ignoreCollision = false);

    // Checks Line of Sight using Detour raycast
    bool CheckLineOfSight(float startX, float startZ, float targetX, float targetZ);

private:
    bool BuildFlatNavMesh();

    dtNavMesh* m_navMesh;
    dtNavMeshQuery* m_navQuery;
    mutable std::mutex m_navMutex;
};

#define sNavMeshMgr NavMeshMgr::getSingleton()

#endif

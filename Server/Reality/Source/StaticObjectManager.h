#ifndef MXOEMU_STATICOBJECTMANAGER_H
#define MXOEMU_STATICOBJECTMANAGER_H

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <shared_mutex>
#include <algorithm>

struct StaticAABB
{
    float minX, maxX;
    float minY, maxY;
    float minZ, maxZ;
    uint32 mxoId;
    uint16 metrId;
    uint16 typeId;
    bool exterior;

    bool ContainsPoint(float x, float y, float z) const
    {
        return (x >= minX && x <= maxX &&
                z >= minZ && z <= maxZ &&
                (minY == maxY || (y >= minY && y <= maxY)));
    }

    bool IntersectsSphere(float x, float y, float z, float radius) const
    {
        float closestX = std::max(minX, std::min(x, maxX));
        float closestY = std::max(minY, std::min(y, maxY));
        float closestZ = std::max(minZ, std::min(z, maxZ));

        float dx = x - closestX;
        float dy = (minY == maxY) ? 0.0f : (y - closestY);
        float dz = z - closestZ;

        return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
    }
};

class StaticObjectManager : public Singleton<StaticObjectManager>
{
public:
    StaticObjectManager();
    ~StaticObjectManager();

    bool Initialize(const std::string& dataDir = "");
    bool LoadCSV(const std::string& filePath);

    bool CheckCollision(float x, float y, float z, float radius) const;
    bool CheckLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2) const;
    std::vector<StaticAABB> GetObstaclesInRadius(float x, float z, float radius) const;

    size_t GetTotalObjectCount() const { return m_objects.size(); }
    bool IsLoaded() const { return m_isLoaded; }

private:
    uint64_t GetCellKey(int gx, int gz) const;
    void WorldToGrid(float wx, float wz, int& gx, int& gz) const;
    bool RayAABBIntersect(const StaticAABB& box, float ox, float oy, float oz,
                          float invDx, float invDy, float invDz,
                          float rayLength) const;

    float m_cellSize;
    bool m_isLoaded;
    std::vector<StaticAABB> m_objects;
    std::unordered_map<uint64_t, std::vector<uint32>> m_grid;
    mutable std::shared_mutex m_mutex;
};

#define sStaticObjMgr StaticObjectManager::getSingleton()

#endif // MXOEMU_STATICOBJECTMANAGER_H

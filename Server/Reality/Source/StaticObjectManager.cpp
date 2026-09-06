#include "StaticObjectManager.h"
#include "Log.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

createFileSingleton(StaticObjectManager);

StaticObjectManager::StaticObjectManager()
    : m_cellSize(15000.0f), m_isLoaded(false)
{
}

StaticObjectManager::~StaticObjectManager()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_objects.clear();
    m_grid.clear();
}

uint64_t StaticObjectManager::GetCellKey(int gx, int gz) const
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) |
           static_cast<uint64_t>(static_cast<uint32_t>(gz));
}

void StaticObjectManager::WorldToGrid(float wx, float wz, int& gx, int& gz) const
{
    gx = static_cast<int>(std::floor(wx / m_cellSize));
    gz = static_cast<int>(std::floor(wz / m_cellSize));
}

bool StaticObjectManager::Initialize(const std::string& dataDir)
{
    std::vector<std::string> searchPaths = {
        dataDir,
        "Data/hd_dump/",
        "Data/",
        "E:/Games/The Matrix Online/hd_reference/data/",
        "../../hd_reference/data/"
    };

    const std::vector<std::string> filenames = {
        "staticObjects_slums_1.csv",
        "staticObjects_slums_2.csv",
        "staticObjects_it.csv",
        "staticObjects_dt_1.csv",
        "staticObjects_dt_2.csv"
    };

    bool loadedAny = false;
    for (const auto& file : filenames)
    {
        bool fileFound = false;
        for (const auto& path : searchPaths)
        {
            if (path.empty()) continue;
            std::string fullPath = path + file;
            std::ifstream test(fullPath.c_str());
            if (test.good())
            {
                test.close();
                INFO_LOG(format("StaticObjectManager: Ingesting %1%...") % fullPath);
                if (LoadCSV(fullPath))
                {
                    INFO_LOG(format("StaticObjectManager: Loaded %1% (cumulative entities: %2%)") % file % m_objects.size());
                    loadedAny = true;
                    fileFound = true;
                    break;
                }
            }
        }
        if (!fileFound)
        {
            DEBUG_LOG(format("StaticObjectManager: Could not locate %1% in search paths.") % file);
        }
    }

    m_isLoaded = loadedAny;
    INFO_LOG(format("StaticObjectManager: Total loaded static world entities: %1% indexed into %2% spatial cells.")
             % m_objects.size() % m_grid.size());
    return m_isLoaded;
}

bool StaticObjectManager::LoadCSV(const std::string& filePath)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open())
        return false;

    std::string line;
    bool isHeader = true;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_objects.reserve(m_objects.size() + 600000);
    if (m_grid.empty())
        m_grid.reserve(65536);

    char buffer[65536];
    file.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        if (isHeader)
        {
            isHeader = false;
            continue;
        }

        // High-speed in-place column parsing (0 heap allocations per line)
        const char* str = line.c_str();
        const char* col[12];
        int colCount = 0;
        col[colCount++] = str;
        for (const char* p = str; *p && colCount < 12; ++p)
        {
            if (*p == ',' || *p == ';')
            {
                col[colCount++] = p + 1;
            }
        }

        if (colCount >= 9)
        {
            char ex = col[5][0];
            bool isExterior = (ex == 'T' || ex == 't' || ex == '1');
            if (!isExterior) continue;

            uint16 metrId = (uint16)std::strtoul(col[0], nullptr, 10);
            float posX = std::strtof(col[6], nullptr);
            float posY = std::strtof(col[7], nullptr);
            float posZ = std::strtof(col[8], nullptr);

            float halfWidth = 1500.0f;
            float halfDepth = 1500.0f;
            float halfHeight = 2500.0f;

            StaticAABB box;
            box.metrId = metrId;
            box.typeId = 0;
            box.exterior = true;
            box.minX = posX - halfWidth;
            box.maxX = posX + halfWidth;
            box.minY = posY - halfHeight;
            box.maxY = posY + halfHeight;
            box.minZ = posZ - halfDepth;
            box.maxZ = posZ + halfDepth;

            uint32 objIdx = static_cast<uint32>(m_objects.size());
            m_objects.push_back(box);

            int minGx, minGz, maxGx, maxGz;
            WorldToGrid(box.minX, box.minZ, minGx, minGz);
            WorldToGrid(box.maxX, box.maxZ, maxGx, maxGz);

            for (int gx = minGx; gx <= maxGx; ++gx)
            {
                for (int gz = minGz; gz <= maxGz; ++gz)
                {
                    uint64_t key = GetCellKey(gx, gz);
                    m_grid[key].push_back(objIdx);
                }
            }
        }
    }

    return true;
}

bool StaticObjectManager::CheckCollision(float x, float y, float z, float radius) const
{
    if (!m_isLoaded || m_objects.empty())
        return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);

    int minGx, minGz, maxGx, maxGz;
    WorldToGrid(x - radius, z - radius, minGx, minGz);
    WorldToGrid(x + radius, z + radius, maxGx, maxGz);

    for (int gx = minGx; gx <= maxGx; ++gx)
    {
        for (int gz = minGz; gz <= maxGz; ++gz)
        {
            uint64_t key = GetCellKey(gx, gz);
            auto it = m_grid.find(key);
            if (it != m_grid.end())
            {
                for (uint32 idx : it->second)
                {
                    if (idx < m_objects.size() && m_objects[idx].IntersectsSphere(x, y, z, radius))
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool StaticObjectManager::RayAABBIntersect(const StaticAABB& box, float ox, float oy, float oz,
                                          float invDx, float invDy, float invDz,
                                          float rayLength) const
{
    float t1 = (box.minX - ox) * invDx;
    float t2 = (box.maxX - ox) * invDx;
    float tmin = std::min(t1, t2);
    float tmax = std::max(t1, t2);

    float t5 = (box.minZ - oz) * invDz;
    float t6 = (box.maxZ - oz) * invDz;
    tmin = std::max(tmin, std::min(t5, t6));
    tmax = std::min(tmax, std::max(t5, t6));

    if (tmax < 0.0f || tmin > tmax)
        return false;

    return tmin <= rayLength;
}

bool StaticObjectManager::CheckLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2) const
{
    if (!m_isLoaded || m_objects.empty())
        return true; // No static obstacles loaded, assume clear

    std::shared_lock<std::shared_mutex> lock(m_mutex);

    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    float rayLength = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (rayLength < 1.0f)
        return true;

    float invDx = (std::abs(dx) > 0.0001f) ? (1.0f / dx) : 1000000.0f;
    float invDy = (std::abs(dy) > 0.0001f) ? (1.0f / dy) : 1000000.0f;
    float invDz = (std::abs(dz) > 0.0001f) ? (1.0f / dz) : 1000000.0f;

    int minGx, minGz, maxGx, maxGz;
    WorldToGrid(std::min(x1, x2), std::min(z1, z2), minGx, minGz);
    WorldToGrid(std::max(x1, x2), std::max(z1, z2), maxGx, maxGz);

    for (int gx = minGx; gx <= maxGx; ++gx)
    {
        for (int gz = minGz; gz <= maxGz; ++gz)
        {
            uint64_t key = GetCellKey(gx, gz);
            auto it = m_grid.find(key);
            if (it != m_grid.end())
            {
                for (uint32 idx : it->second)
                {
                    if (idx < m_objects.size())
                    {
                        if (RayAABBIntersect(m_objects[idx], x1, y1, z1, invDx, invDy, invDz, rayLength))
                        {
                            return false; // Ray obstructed by building
                        }
                    }
                }
            }
        }
    }

    return true;
}

std::vector<StaticAABB> StaticObjectManager::GetObstaclesInRadius(float x, float z, float radius) const
{
    std::vector<StaticAABB> obstacles;
    if (!m_isLoaded || m_objects.empty())
        return obstacles;

    std::shared_lock<std::shared_mutex> lock(m_mutex);

    int minGx, minGz, maxGx, maxGz;
    WorldToGrid(x - radius, z - radius, minGx, minGz);
    WorldToGrid(x + radius, z + radius, maxGx, maxGz);

    for (int gx = minGx; gx <= maxGx; ++gx)
    {
        for (int gz = minGz; gz <= maxGz; ++gz)
        {
            uint64_t key = GetCellKey(gx, gz);
            auto it = m_grid.find(key);
            if (it != m_grid.end())
            {
                for (uint32 idx : it->second)
                {
                    if (idx < m_objects.size() && m_objects[idx].IntersectsSphere(x, 0.0f, z, radius))
                    {
                        obstacles.push_back(m_objects[idx]);
                    }
                }
            }
        }
    }

    return obstacles;
}

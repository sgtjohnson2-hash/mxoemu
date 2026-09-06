#include <algorithm>
#include "SpatialGrid.h"
#include "GameClient.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "PlayerObject.h"
#include "Log.h"
#include "StaticObjectManager.h"
#include <cmath>

createFileSingleton(SpatialGrid);

SpatialGrid::SpatialGrid() : m_cellSize(150.0f)
{
}

SpatialGrid::~SpatialGrid()
{
}

void SpatialGrid::Initialize(float cellSize)
{
    m_cellSize = cellSize;
    m_cells.reserve(20000);
    INFO_LOG(format("SpatialGrid Initialized: Cell Size %1% world units (%2%m)") % m_cellSize % (m_cellSize/100.0f));
}

void SpatialGrid::WorldToGrid(float wx, float wz, int& gx, int& gy) const
{
    gx = static_cast<int>(std::floor(wx / m_cellSize));
    gy = static_cast<int>(std::floor(wz / m_cellSize));
}

uint64_t SpatialGrid::GetCellHash(int gx, int gy, uint32 instanceId) const
{
    return (static_cast<uint64_t>(instanceId) << 32) | ((static_cast<uint64_t>(gx) & 0xFFFF) << 16) | (static_cast<uint32_t>(gy) & 0xFFFF);
}

void SpatialGrid::UpdateClientPosition(GameClient* client, float x, float z)
{
    if (client == nullptr) {
        ERROR_LOG("SpatialGrid::UpdateClientPosition called with nullptr client!");
        return;
    }

    int gx, gy;
    WorldToGrid(x, z, gx, gy);
    uint64_t newHash = GetCellHash(gx, gy, client->m_instanceId);
    uint64_t oldHash = client->m_spatialCellHash.load();

    if (oldHash == newHash)
        return; // No cell change, do nothing

    // Remove from old cell
    if (oldHash != 0xFFFFFFFFFFFFFFFF)
    {
        std::shared_ptr<SpatialGridCell> oldCell;
        {
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            auto cellIt = m_cells.find(oldHash);
            if (cellIt != m_cells.end() && cellIt->second) {
                oldCell = cellIt->second;
            }
        }
        
        if (oldCell) {
            std::unique_lock<std::shared_mutex> cellLock(m_cellMutexes[oldHash % 1024]);
            auto it = std::find(oldCell->clients.begin(), oldCell->clients.end(), client);
            if (it != oldCell->clients.end()) {
                oldCell->clients.erase(it);
            }
        }
    }

    // Add to new cell
    {
        std::shared_ptr<SpatialGridCell> cell;
        {
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            auto cellIt = m_cells.find(newHash);
            if (cellIt != m_cells.end()) {
                cell = cellIt->second;
            }
        }
        
        if (!cell) {
            std::unique_lock<std::shared_mutex> writeMapLock(m_mapMutex);
            if (m_cells.find(newHash) == m_cells.end()) {
                m_cells[newHash] = std::make_shared<SpatialGridCell>();
            }
            cell = m_cells[newHash];
        }

        std::unique_lock<std::shared_mutex> cellLock(m_cellMutexes[newHash % 1024]);
        cell->clients.push_back(client);
    }
    
    client->m_spatialCellHash.store(newHash);
}

void SpatialGrid::RemoveClient(GameClient* client)
{
    if (!client) return;
    uint64_t hash = client->m_spatialCellHash.load();
    if (hash != 0xFFFFFFFFFFFFFFFF)
    {
        std::shared_ptr<SpatialGridCell> cell;
        {
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            auto cellIt = m_cells.find(hash);
            if (cellIt != m_cells.end() && cellIt->second) {
                cell = cellIt->second;
            }
        }
    
        if (cell)
        {
            std::unique_lock<std::shared_mutex> cellLock(m_cellMutexes[hash % 1024]);
            auto it = std::find(cell->clients.begin(), cell->clients.end(), client);
            if (it != cell->clients.end()) {
                cell->clients.erase(it);
            }
        }
        client->m_spatialCellHash.store(0xFFFFFFFFFFFFFFFF);
    }
}

std::vector<GameClient*> SpatialGrid::GetClientsInRadius(float x, float z, uint32 instanceId) const
{
    std::vector<GameClient*> localClients;
    
    int gx, gy;
    WorldToGrid(x, z, gx, gy);

    // Retrieve from center and 8 neighbors
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            uint64_t neighborHash = GetCellHash(gx + dx, gy + dy, instanceId);
            
            std::shared_ptr<SpatialGridCell> cell;
            {
                std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
                auto nIt = m_cells.find(neighborHash);
                if (nIt != m_cells.end() && nIt->second)
                {
                    cell = nIt->second;
                }
            }
            
            if (cell)
            {
                std::shared_lock<std::shared_mutex> cellLock(m_cellMutexes[neighborHash % 1024]);
                for (GameClient* c : cell->clients) {
                    localClients.push_back(c);
                }
            }
        }
    }

    return localClients;
}

std::vector<GameClient*> SpatialGrid::GetClientsInRadius(float x, float z, float radius, uint32 instanceId) const
{
    std::vector<GameClient*> localClients = GetClientsInRadius(x, z, instanceId);
    std::vector<GameClient*> filteredClients;
    float rSq = radius * radius;
    
    for (GameClient* client : localClients)
    {
        uint32 goId = client->GetPlayerGoId();
        if (goId == 0) continue;
        
        PlayerObject* po = NULL;
        try { po = sObjMgr.getGOPtr(goId); }
        catch (ObjectMgr::ObjectNotAvailable) { continue; }
        if (po)
        {
            float dx = float(po->getPosition().x) - x;
            float dz = float(po->getPosition().z) - z;
            if (dx*dx + dz*dz <= rSq)
            {
                filteredClients.push_back(client);
            }
        }
    }
    
    return filteredClients;
}

bool SpatialGrid::CheckCollision(float x, float z, float radius, uint32 ignoreGoId, uint32 instanceId) const
{
    // First, check static world geometry (buildings, walls, urban obstacles)
    if (sStaticObjMgr.CheckCollision(x, 0.0f, z, radius))
        return true;

    float rSq = radius * radius;
    std::vector<GameClient*> localClients = GetClientsInRadius(x, z, instanceId);
    
    for (GameClient* client : localClients)
    {
        uint32 goId = client->GetPlayerGoId();
        if (goId == 0 || goId == ignoreGoId) continue;
        
        PlayerObject* po = NULL;
        try { po = sObjMgr.getGOPtr(goId); }
        catch (ObjectMgr::ObjectNotAvailable) { continue; }
        if (po && !po->isDead())
        {
            float dx = po->getPosition().x - x;
            float dz = po->getPosition().z - z;
            if (dx*dx + dz*dz < rSq)
                return true;
        }
    }
    return false;
}

std::vector<GameClient*> SpatialGrid::GetClientsNearClient(GameClient* client) const
{
    uint64_t hash = client->m_spatialCellHash.load();
    if (hash == 0xFFFFFFFFFFFFFFFF)
        return std::vector<GameClient*>();

    int gy = static_cast<int16_t>(hash & 0xFFFF);
    int gx = static_cast<int16_t>((hash >> 16) & 0xFFFF);
    uint32 instanceId = static_cast<uint32>(hash >> 32);

    std::vector<GameClient*> localClients;
    for (int yOffset = -1; yOffset <= 1; ++yOffset)
    {
        for (int xOffset = -1; xOffset <= 1; ++xOffset)
        {
            uint64_t neighborHash = GetCellHash(gx + xOffset, gy + yOffset, instanceId);
            
            std::shared_ptr<SpatialGridCell> cell;
            {
                std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
                auto nIt = m_cells.find(neighborHash);
                if (nIt != m_cells.end() && nIt->second)
                {
                    cell = nIt->second;
                }
            }

            if (cell)
            {
                std::shared_lock<std::shared_mutex> cellLock(m_cellMutexes[neighborHash % 1024]);
                for (GameClient* c : cell->clients) {
                    localClients.push_back(c);
                }
            }
        }
    }
    return localClients;
}

std::vector<GameClient*> SpatialGrid::GetClientsInAoI(float x, float z, float maxRadius, uint32 instanceId) const
{
    float rSq = maxRadius * maxRadius;
    std::vector<GameClient*> scopedClients;

    int gx, gy;
    WorldToGrid(x, z, gx, gy);

    // Calculate number of cells needed to cover maxRadius
    int cellRadius = static_cast<int>(std::ceil(maxRadius / m_cellSize));
    cellRadius = std::clamp(cellRadius, 1, 200);

    for (int dx = -cellRadius; dx <= cellRadius; ++dx)
    {
        for (int dy = -cellRadius; dy <= cellRadius; ++dy)
        {
            uint64_t cellHash = GetCellHash(gx + dx, gy + dy, instanceId);

            std::shared_ptr<SpatialGridCell> cell;
            {
                std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
                auto it = m_cells.find(cellHash);
                if (it != m_cells.end() && it->second)
                {
                    cell = it->second;
                }
            }

            if (!cell) continue;

            std::shared_lock<std::shared_mutex> cellLock(m_cellMutexes[cellHash % 1024]);
            for (GameClient* client : cell->clients)
            {
                if (!client) continue;
                uint32 goId = client->GetPlayerGoId();
                if (goId == 0) continue;

                PlayerObject* po = sObjMgr.getGOPtrSafe(goId);
                if (po)
                {
                    float px = float(po->getPosition().x) - x;
                    float pz = float(po->getPosition().z) - z;
                    if (px * px + pz * pz <= rSq)
                    {
                        scopedClients.push_back(client);
                        m_aoiPassedPackets.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        m_aoiCulledPackets.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
    }

    return scopedClients;
}

bool SpatialGrid::IsWithinAoI(float x1, float z1, float x2, float z2, float maxRadius) const
{
    float dx = x1 - x2;
    float dz = z1 - z2;
    return (dx * dx + dz * dz) <= (maxRadius * maxRadius);
}

void SpatialGrid::RecordAoIScopedPacket(bool passed) const
{
    if (passed)
        m_aoiPassedPackets.fetch_add(1, std::memory_order_relaxed);
    else
        m_aoiCulledPackets.fetch_add(1, std::memory_order_relaxed);
}

void SpatialGrid::GetAoIScopingStats(uint64& outCulled, uint64& outPassed) const
{
    outCulled = m_aoiCulledPackets.load(std::memory_order_relaxed);
    outPassed = m_aoiPassedPackets.load(std::memory_order_relaxed);
}


#ifndef MXOEMU_SPATIAL_GRID_H
#define MXOEMU_SPATIAL_GRID_H

#include "Common.h"
#include "Singleton.h"
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

class GameClient;

struct SpatialGridCell {
    std::vector<GameClient*> clients;
    SpatialGridCell() {}
};

// Spatial Partitioning for Network Relevance
class SpatialGrid : public Singleton<SpatialGrid>{
public:
    SpatialGrid();
    ~SpatialGrid();

    void Initialize(float cellSize = 150.0f);

    // Updates a client's position in the grid
    void UpdateClientPosition(GameClient* client, float x, float z);

    // Removes a client from the grid entirely (e.g., disconnect)
    void RemoveClient(GameClient* client);

    // Retrieves all clients in the local cell and the 8 adjacent neighboring cells
    std::vector<GameClient*> GetClientsInRadius(float x, float z, uint32 instanceId = 0) const;
    void GetClientsInRadius(float x, float z, std::vector<GameClient*>& outClients, uint32 instanceId = 0) const;
    
    // Retrieves all clients near another client based on their cached cell
    std::vector<GameClient*> GetClientsNearClient(GameClient* client) const;
    std::vector<GameClient*> GetClientsInRadius(float x, float z, float radius, uint32 instanceId = 0) const;

    // Checks for physical overlap (collision)
    bool CheckCollision(float x, float z, float radius, uint32 ignoreGoId, uint32 instanceId = 0) const;

    // Phase 7: Area-of-Interest (AoI) Network Scoping (<250m radius filtering)
    static constexpr float AOI_RADIUS_WORLD_UNITS = 25000.0f; // 250m
    std::vector<GameClient*> GetClientsInAoI(float x, float z, float maxRadius = 25000.0f, uint32 instanceId = 0) const;
    void GetClientsInAoI(float x, float z, std::vector<GameClient*>& outClients, float maxRadius = 25000.0f, uint32 instanceId = 0) const;
    bool IsWithinAoI(float x1, float z1, float x2, float z2, float maxRadius = 25000.0f) const;
    void RecordAoIScopedPacket(bool passed) const;
    void GetAoIScopingStats(uint64& outCulled, uint64& outPassed) const;

private:
    float m_cellSize;

    // Hash map keyed by a 64-bit combined grid coordinate
    mutable std::shared_mutex m_mapMutex;
    std::unordered_map<uint64_t, std::shared_ptr<SpatialGridCell>> m_cells;
    
    // Item 52: Lock-Free/Striped Concurrency
    mutable std::shared_mutex m_cellMutexes[1024];

    mutable std::atomic<uint64> m_aoiCulledPackets{0};
    mutable std::atomic<uint64> m_aoiPassedPackets{0};

    uint64_t GetCellHash(int gx, int gy, uint32 instanceId) const;
    void WorldToGrid(float wx, float wz, int& gx, int& gy) const;
};

#define sSpatialGrid Singleton<SpatialGrid>::getSingleton()

#endif // MXOEMU_SPATIAL_GRID_H

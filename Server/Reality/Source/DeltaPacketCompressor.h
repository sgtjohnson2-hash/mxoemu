#ifndef MXOEMU_DELTA_PACKET_COMPRESSOR_H
#define MXOEMU_DELTA_PACKET_COMPRESSOR_H

#include "Common.h"
#include "Singleton.h"
#include "ByteBuffer.h"
#include <atomic>
#include <cmath>

struct EntityNetworkState
{
    uint32 goId{0};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float rot{0.0f};
    uint32 health{100};
    uint32 innerStrength{100};
    uint8 combatState{0};
    uint32 statusFlags{0};

    bool operator==(const EntityNetworkState& o) const
    {
        return goId == o.goId &&
               std::abs(x - o.x) < 0.01f &&
               std::abs(y - o.y) < 0.01f &&
               std::abs(z - o.z) < 0.01f &&
               std::abs(rot - o.rot) < 0.01f &&
               health == o.health &&
               innerStrength == o.innerStrength &&
               combatState == o.combatState &&
               statusFlags == o.statusFlags;
    }
};

enum DeltaFlags : uint8
{
    DELTA_POS_X   = 1 << 0,
    DELTA_POS_Y   = 1 << 1,
    DELTA_POS_Z   = 1 << 2,
    DELTA_ROT     = 1 << 3,
    DELTA_HEALTH  = 1 << 4,
    DELTA_IS      = 1 << 5,
    DELTA_COMBAT  = 1 << 6,
    DELTA_STATUS  = 1 << 7
};

class DeltaPacketCompressor : public Singleton<DeltaPacketCompressor>
{
public:
    DeltaPacketCompressor();
    ~DeltaPacketCompressor();

    // Compresses current state against baseline into a compact byte payload
    void Compress(const EntityNetworkState& baseline, const EntityNetworkState& current, ByteBuffer& output);

    // Decompresses payload using baseline to reconstruct current state
    bool Decompress(const EntityNetworkState& baseline, ByteBuffer& input, EntityNetworkState& outCurrent);

    void GetCompressionMetrics(uint64& uncompressedBytes, uint64& compressedBytes, float& ratio) const;

private:
    mutable std::atomic<uint64> m_uncompressedBytes{0};
    mutable std::atomic<uint64> m_compressedBytes{0};
};

#define sDeltaCompressor DeltaPacketCompressor::getSingleton()

#endif // MXOEMU_DELTA_PACKET_COMPRESSOR_H

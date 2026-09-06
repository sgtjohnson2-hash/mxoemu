#include "DeltaPacketCompressor.h"
#include <cstring>

createFileSingleton(DeltaPacketCompressor);

DeltaPacketCompressor::DeltaPacketCompressor()
{
}

DeltaPacketCompressor::~DeltaPacketCompressor()
{
}

void DeltaPacketCompressor::Compress(const EntityNetworkState& baseline, const EntityNetworkState& current, ByteBuffer& output)
{
    // Baseline raw uncompressed size calculation:
    // goId(4) + x(4) + y(4) + z(4) + rot(4) + hp(4) + is(4) + combat(1) + status(4) = 29 bytes
    constexpr size_t rawSize = sizeof(uint32) * 6 + sizeof(float) * 4 + sizeof(uint8);
    m_uncompressedBytes.fetch_add(rawSize, std::memory_order_relaxed);

    size_t startPos = output.size();

    uint8 mask = 0;
    if (std::abs(current.x - baseline.x) >= 0.01f) mask |= DELTA_POS_X;
    if (std::abs(current.y - baseline.y) >= 0.01f) mask |= DELTA_POS_Y;
    if (std::abs(current.z - baseline.z) >= 0.01f) mask |= DELTA_POS_Z;
    if (std::abs(current.rot - baseline.rot) >= 0.01f) mask |= DELTA_ROT;
    if (current.health != baseline.health) mask |= DELTA_HEALTH;
    if (current.innerStrength != baseline.innerStrength) mask |= DELTA_IS;
    if (current.combatState != baseline.combatState) mask |= DELTA_COMBAT;
    if (current.statusFlags != baseline.statusFlags) mask |= DELTA_STATUS;

    output << current.goId;
    output << mask;

    // Quantized / variable encoding for delta fields
    if (mask & DELTA_POS_X)
    {
        float dx = current.x - baseline.x;
        if (dx >= -3200.0f && dx <= 3200.0f)
        {
            int16 qx = static_cast<int16>(std::round(dx * 10.0f));
            output << static_cast<uint8>(1); // Quantized flag
            output << static_cast<uint16>(qx);
        }
        else
        {
            output << static_cast<uint8>(0); // Full precision float
            output << current.x;
        }
    }

    if (mask & DELTA_POS_Y)
    {
        output << current.y;
    }

    if (mask & DELTA_POS_Z)
    {
        float dz = current.z - baseline.z;
        if (dz >= -3200.0f && dz <= 3200.0f)
        {
            int16 qz = static_cast<int16>(std::round(dz * 10.0f));
            output << static_cast<uint8>(1);
            output << static_cast<uint16>(qz);
        }
        else
        {
            output << static_cast<uint8>(0);
            output << current.z;
        }
    }

    if (mask & DELTA_ROT)
    {
        // 16-bit angle encoding (0 - 65535 for 0 - 2*PI), handle negative angles
        float normRot = std::fmod(current.rot, 6.2831853f);
        if (normRot < 0.0f) normRot += 6.2831853f;
        uint16 qrot = static_cast<uint16>((normRot / 6.2831853f) * 65535.0f);
        output << qrot;
    }

    if (mask & DELTA_HEALTH)
    {
        output << current.health;
    }

    if (mask & DELTA_IS)
    {
        output << current.innerStrength;
    }

    if (mask & DELTA_COMBAT)
    {
        output << current.combatState;
    }

    if (mask & DELTA_STATUS)
    {
        output << current.statusFlags;
    }

    size_t compressedSize = output.size() - startPos;
    m_compressedBytes.fetch_add(compressedSize, std::memory_order_relaxed);
}

bool DeltaPacketCompressor::Decompress(const EntityNetworkState& baseline, ByteBuffer& input, EntityNetworkState& outCurrent)
{
    if (input.rpos() >= input.size()) return false;

    try
    {
        outCurrent = baseline; // Start with baseline

        input >> outCurrent.goId;
        uint8 mask = 0;
        input >> mask;

        if (mask & DELTA_POS_X)
        {
            uint8 isQuantized = 0;
            input >> isQuantized;
            if (isQuantized == 1)
            {
                uint16 raw_qx = 0;
                input >> raw_qx;
                int16 qx = static_cast<int16>(raw_qx);
                outCurrent.x = baseline.x + (static_cast<float>(qx) / 10.0f);
            }
            else
            {
                input >> outCurrent.x;
            }
        }

        if (mask & DELTA_POS_Y)
        {
            input >> outCurrent.y;
        }

        if (mask & DELTA_POS_Z)
        {
            uint8 isQuantized = 0;
            input >> isQuantized;
            if (isQuantized == 1)
            {
                uint16 raw_qz = 0;
                input >> raw_qz;
                int16 qz = static_cast<int16>(raw_qz);
                outCurrent.z = baseline.z + (static_cast<float>(qz) / 10.0f);
            }
            else
            {
                input >> outCurrent.z;
            }
        }

        if (mask & DELTA_ROT)
        {
            uint16 qrot = 0;
            input >> qrot;
            outCurrent.rot = (static_cast<float>(qrot) / 65535.0f) * 6.2831853f;
        }

        if (mask & DELTA_HEALTH)
        {
            input >> outCurrent.health;
        }

        if (mask & DELTA_IS)
        {
            input >> outCurrent.innerStrength;
        }

        if (mask & DELTA_COMBAT)
        {
            input >> outCurrent.combatState;
        }

        if (mask & DELTA_STATUS)
        {
            input >> outCurrent.statusFlags;
        }

        return true;
    }
    catch (const ByteBuffer::out_of_range&)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

void DeltaPacketCompressor::GetCompressionMetrics(uint64& uncompressedBytes, uint64& compressedBytes, float& ratio) const
{
    uncompressedBytes = m_uncompressedBytes.load(std::memory_order_relaxed);
    compressedBytes = m_compressedBytes.load(std::memory_order_relaxed);
    if (uncompressedBytes > 0)
    {
        ratio = 1.0f - (static_cast<float>(compressedBytes) / static_cast<float>(uncompressedBytes));
    }
    else
    {
        ratio = 0.0f;
    }
}

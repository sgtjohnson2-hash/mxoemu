#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// The Matrix Omniverse: Epoch VII - Pillar I: 3D Gaussian Splatting & Radiance
// ============================================================================

struct SplatVec3
{
    float x{0.0f}, y{0.0f}, z{0.0f};
    SplatVec3() = default;
    SplatVec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    SplatVec3 operator+(const SplatVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    SplatVec3 operator-(const SplatVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    SplatVec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const SplatVec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    SplatVec3 normalized() const {
        float l = length();
        return (l > 0.00001f) ? (*this * (1.0f / l)) : SplatVec3{0, 0, 0};
    }
};

struct SplatQuaternion
{
    float w{1.0f}, x{0.0f}, y{0.0f}, z{0.0f};
    SplatQuaternion() = default;
    SplatQuaternion(float _w, float _x, float _y, float _z) : w(_w), x(_x), y(_y), z(_z) {}

    void toRotationMatrix(float R[3][3]) const {
        R[0][0] = 1.0f - 2.0f * (y * y + z * z);
        R[0][1] = 2.0f * (x * y - z * w);
        R[0][2] = 2.0f * (x * z + y * w);

        R[1][0] = 2.0f * (x * y + z * w);
        R[1][1] = 1.0f - 2.0f * (x * x + z * z);
        R[1][2] = 2.0f * (y * z - x * w);

        R[2][0] = 2.0f * (x * z - y * w);
        R[2][1] = 2.0f * (y * z + x * w);
        R[2][2] = 1.0f - 2.0f * (x * x + y * y);
    }
};

struct GaussianSplat3D
{
    uint32_t splatId{0};
    SplatVec3 position;
    SplatQuaternion rotation;
    SplatVec3 scale{1.0f, 1.0f, 1.0f};
    float opacity{1.0f};
    float sphericalHarmonicsR[16]{0.0f};
    float sphericalHarmonicsG[16]{0.0f};
    float sphericalHarmonicsB[16]{0.0f};
    float depthKey{0.0f};
    float digitalRainAlphaMultiplier{1.0f};
    bool isVisible{true};
};

class GaussianSplatEngine : public Singleton<GaussianSplatEngine>
{
public:
    GaussianSplatEngine();
    ~GaussianSplatEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. Splat Management & Registration
    uint32_t AddGaussianSplat(const SplatVec3& pos, const SplatQuaternion& rot,
                              const SplatVec3& scale, float opacity);
    size_t GetSplatCount() const;
    const GaussianSplat3D* GetSplat(uint32_t splatId) const;

    // 2. Covariance Matrix Decomposition (Sigma = R * S * S^T * R^T)
    void ComputeCovarianceMatrix(const GaussianSplat3D& splat, float outCov[3][3]) const;

    // 3. Spherical Harmonics Degree-3 Radiance Evaluation
    void SetSphericalHarmonicsBand0(uint32_t splatId, float r, float g, float b);
    void SetSphericalHarmonicsBand1(uint32_t splatId, float rY, float gY, float bY);
    void EvaluateRadiance(const GaussianSplat3D& splat, const SplatVec3& viewDir,
                          float& outR, float& outG, float& outB) const;

    // 4. Back-to-Front Depth Sorting (Radix Sort Simulator)
    std::vector<uint32_t> SortSplatsBackToFront(const SplatVec3& camPos, const SplatVec3& camForward);

    // 5. Volumetric Digital Rain Penetration
    void ApplyDigitalRainVolumetricPenetration(float rainDensity, float penetrationDepth);

    // 6. Binary Packet Serialization (.splat streaming format)
    std::vector<uint8_t> SerializeSplatToBinary(uint32_t splatId) const;
    uint32_t IngestSplatFromBinary(const uint8_t* buffer, size_t length);

    // 7. Frustum Culling
    size_t FrustumCullSplats(const SplatVec3& camPos, const SplatVec3& camFwd, float maxDist);

private:
    mutable std::shared_mutex m_engineMutex;
    std::unordered_map<uint32_t, GaussianSplat3D> m_splats;
    uint32_t m_nextSplatId{1};
};

#define sGaussianSplatEngine GaussianSplatEngine::getSingleton()

void RunGaussianSplatTestSuite();

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
// The Matrix Omniverse: Epoch VI - Pillar II: Non-Euclidean Spatial Folding
// ============================================================================

struct PortalVec3
{
    float x{0.0f}, y{0.0f}, z{0.0f};
    PortalVec3() = default;
    PortalVec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    PortalVec3 operator+(const PortalVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    PortalVec3 operator-(const PortalVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    PortalVec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const PortalVec3& o) const { return x * o.x + y * o.y + z * o.z; }
    PortalVec3 cross(const PortalVec3& o) const {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    PortalVec3 normalized() const {
        float l = length();
        return (l > 0.00001f) ? (*this * (1.0f / l)) : PortalVec3{0, 0, 0};
    }
    bool equals(const PortalVec3& o, float eps = 0.001f) const {
        return std::abs(x - o.x) < eps && std::abs(y - o.y) < eps && std::abs(z - o.z) < eps;
    }
};

struct PortalMatrix4x4
{
    float m[4][4];

    PortalMatrix4x4() {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                m[r][c] = (r == c) ? 1.0f : 0.0f;
            }
        }
    }

    static PortalMatrix4x4 Identity() {
        return PortalMatrix4x4();
    }

    static PortalMatrix4x4 Translation(const PortalVec3& t) {
        PortalMatrix4x4 mat;
        mat.m[0][3] = t.x;
        mat.m[1][3] = t.y;
        mat.m[2][3] = t.z;
        return mat;
    }

    static PortalMatrix4x4 RotationY(float angleRad) {
        PortalMatrix4x4 mat;
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        mat.m[0][0] = c;  mat.m[0][2] = s;
        mat.m[2][0] = -s; mat.m[2][2] = c;
        return mat;
    }

    PortalMatrix4x4 operator*(const PortalMatrix4x4& o) const {
        PortalMatrix4x4 res;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                res.m[r][c] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    res.m[r][c] += m[r][k] * o.m[k][c];
                }
            }
        }
        return res;
    }

    PortalVec3 transformPoint(const PortalVec3& p) const {
        return {
            m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
            m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]
        };
    }

    PortalVec3 transformVector(const PortalVec3& v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }
};

struct PortalAperture
{
    uint32_t portalId{0};
    uint32_t targetPortalId{0};
    std::string name;
    
    PortalVec3 position;
    PortalVec3 normal{0.0f, 0.0f, 1.0f};
    PortalVec3 up{0.0f, 1.0f, 0.0f};
    
    float width{150.0f};       // Aperture width in units
    float height{250.0f};      // Aperture height in units
    
    bool isBiDirectional{true};
    bool isAcousticallyTransmissive{true};
    float acousticAttenuationDb{-3.0f};
    
    bool flipsGravity{false};               // Klein-Bottle geometry
    bool enablesSurfaceCeilingRide{false};   // Möbius strip geometry
    
    PortalMatrix4x4 transformMatrix;
    uint32_t totalTraversals{0};
};

struct AcousticDiffractionResult
{
    PortalVec3 virtualSourcePosition;
    float finalVolumeDb{-3.0f};
    float pathDistance{0.0f};
    float spreadAngleDeg{45.0f};
};

class NonEuclideanPortalEngine : public Singleton<NonEuclideanPortalEngine>
{
public:
    NonEuclideanPortalEngine();
    ~NonEuclideanPortalEngine();

    void Initialize();
    void ResetForTesting();

    // Portal Pair Registration
    uint32_t RegisterPortal(const std::string& name, const PortalVec3& pos, const PortalVec3& normal,
                            float width = 150.0f, float height = 250.0f);
    bool LinkPortals(uint32_t portalAId, uint32_t portalBId);
    
    const PortalAperture* GetPortal(uint32_t portalId) const;
    size_t GetPortalCount() const;

    // Kinematic Traversal & Momentum Conservation
    bool CheckEntityTraversal(uint32_t portalId, const PortalVec3& prevPos, const PortalVec3& currPos,
                              const PortalVec3& inVel, PortalVec3& outNewPos, PortalVec3& outNewVel,
                              bool& outGravityFlipped);

    // Ballistic Ray Continuous Collision Detection (CCD)
    bool TraceBallisticRay(const PortalVec3& rayOrigin, const PortalVec3& rayDir, float maxDist,
                           PortalVec3& outFinalPos, uint32_t& outTraversedCount, int maxRecursion = 4);

    // Non-Orientable Geometries: Klein Bottle & Möbius Strip
    void SetupKleinBottleSubwayLoop(uint32_t portalInId, uint32_t portalOutId);
    void SetupMobiusStripHighwayCircuit(uint32_t portalInId, uint32_t portalOutId);

    // Acoustic Diffraction
    bool CalculateAcousticDiffraction(uint32_t portalId, const PortalVec3& listenerPos,
                                      const PortalVec3& soundSourcePos,
                                      AcousticDiffractionResult& outResult) const;

    // Operator Dynamic Portal Deployment
    uint32_t PlaceOperatorTacticalPortal(uint32_t operatorGoId, const PortalVec3& pos,
                                         const PortalVec3& normal, float durationSec = 60.0f);
    void Update(float dt);

private:
    void ComputePortalTransformMatrix(PortalAperture& src, const PortalAperture& dst);

    mutable std::shared_mutex m_portalMutex;
    std::unordered_map<uint32_t, PortalAperture> m_portals;
    uint32_t m_nextPortalId{1};

    struct OperatorPortalExpiry {
        uint32_t portalId;
        float remainingSeconds;
    };
    std::vector<OperatorPortalExpiry> m_expiringPortals;
};

#define sNonEuclideanPortalEngine NonEuclideanPortalEngine::getSingleton()

void RunNonEuclideanPortalTestSuite();

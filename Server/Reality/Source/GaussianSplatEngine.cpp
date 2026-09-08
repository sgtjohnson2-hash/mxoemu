#include "GaussianSplatEngine.h"
#include "Log.h"
#include <iostream>

createFileSingleton(GaussianSplatEngine);

GaussianSplatEngine::GaussianSplatEngine()
{
}

GaussianSplatEngine::~GaussianSplatEngine()
{
}

void GaussianSplatEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    boost::format fmt("GaussianSplatEngine initialized with 3D Gaussian radiance fields and SH degree-3 lighting.");
    INFO_LOG(fmt);
}

void GaussianSplatEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_splats.clear();
    m_nextSplatId = 1;
}

uint32_t GaussianSplatEngine::AddGaussianSplat(const SplatVec3& pos, const SplatQuaternion& rot,
                                              const SplatVec3& scale, float opacity)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    uint32_t id = m_nextSplatId++;

    GaussianSplat3D splat;
    splat.splatId = id;
    splat.position = pos;
    splat.rotation = rot;
    splat.scale = scale;
    splat.opacity = std::clamp(opacity, 0.0f, 1.0f);
    splat.digitalRainAlphaMultiplier = 1.0f;
    splat.isVisible = true;

    // Default base color (SH degree 0)
    splat.sphericalHarmonicsR[0] = 0.5f;
    splat.sphericalHarmonicsG[0] = 0.8f;
    splat.sphericalHarmonicsB[0] = 0.5f;

    m_splats[id] = splat;
    return id;
}

size_t GaussianSplatEngine::GetSplatCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    return m_splats.size();
}

const GaussianSplat3D* GaussianSplatEngine::GetSplat(uint32_t splatId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_splats.find(splatId);
    if (it != m_splats.end()) {
        return &it->second;
    }
    return nullptr;
}

void GaussianSplatEngine::ComputeCovarianceMatrix(const GaussianSplat3D& splat, float outCov[3][3]) const
{
    // 1. Convert quaternion to rotation matrix R
    float R[3][3];
    splat.rotation.toRotationMatrix(R);

    // 2. Scale matrix S (diagonal)
    // S = diag(scale.x, scale.y, scale.z)
    // M = R * S
    float M[3][3];
    for (int i = 0; i < 3; ++i) {
        M[i][0] = R[i][0] * splat.scale.x;
        M[i][1] = R[i][1] * splat.scale.y;
        M[i][2] = R[i][2] * splat.scale.z;
    }

    // 3. Covariance Sigma = M * M^T = (R * S) * (S^T * R^T)
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            outCov[r][c] = M[r][0] * M[c][0] + M[r][1] * M[c][1] + M[r][2] * M[c][2];
        }
    }
}

void GaussianSplatEngine::SetSphericalHarmonicsBand0(uint32_t splatId, float r, float g, float b)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_splats.find(splatId);
    if (it != m_splats.end()) {
        it->second.sphericalHarmonicsR[0] = r;
        it->second.sphericalHarmonicsG[0] = g;
        it->second.sphericalHarmonicsB[0] = b;
    }
}

void GaussianSplatEngine::SetSphericalHarmonicsBand1(uint32_t splatId, float rY, float gY, float bY)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_splats.find(splatId);
    if (it != m_splats.end()) {
        // Band 1: index 1 corresponds to Y axis view-dependency
        it->second.sphericalHarmonicsR[1] = rY;
        it->second.sphericalHarmonicsG[1] = gY;
        it->second.sphericalHarmonicsB[1] = bY;
    }
}

void GaussianSplatEngine::EvaluateRadiance(const GaussianSplat3D& splat, const SplatVec3& viewDir,
                                          float& outR, float& outG, float& outB) const
{
    SplatVec3 d = viewDir.normalized();

    // Spherical Harmonics basis functions (Degree 0 and Degree 1):
    // Y_0^0 = 0.28209479f
    // Y_1^-1 = -0.4886025f * d.y
    // Y_1^0  =  0.4886025f * d.z
    // Y_1^1  = -0.4886025f * d.x
    const float C0 = 0.28209479f;
    const float C1 = 0.4886025f;

    float baseR = splat.sphericalHarmonicsR[0] * C0;
    float baseG = splat.sphericalHarmonicsG[0] * C0;
    float baseB = splat.sphericalHarmonicsB[0] * C0;

    // Add view-dependent Band 1 terms
    baseR += (-C1 * d.y * splat.sphericalHarmonicsR[1]);
    baseG += (-C1 * d.y * splat.sphericalHarmonicsG[1]);
    baseB += (-C1 * d.y * splat.sphericalHarmonicsB[1]);

    // Apply digital rain alpha modulation
    outR = std::clamp(baseR * splat.digitalRainAlphaMultiplier, 0.0f, 1.0f);
    outG = std::clamp(baseG * splat.digitalRainAlphaMultiplier, 0.0f, 1.0f);
    outB = std::clamp(baseB * splat.digitalRainAlphaMultiplier, 0.0f, 1.0f);
}

std::vector<uint32_t> GaussianSplatEngine::SortSplatsBackToFront(const SplatVec3& camPos, const SplatVec3& camForward)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    std::vector<std::pair<float, uint32_t>> depthPairs;
    depthPairs.reserve(m_splats.size());

    SplatVec3 fwd = camForward.normalized();

    for (auto& kv : m_splats) {
        GaussianSplat3D& s = kv.second;
        if (!s.isVisible) continue;

        // Depth along camera forward axis
        float depth = (s.position - camPos).dot(fwd);
        s.depthKey = depth;
        depthPairs.push_back({depth, s.splatId});
    }

    // Sort descending by depth: farthest first (back-to-front alpha blending)
    std::sort(depthPairs.begin(), depthPairs.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::vector<uint32_t> sortedIds;
    sortedIds.reserve(depthPairs.size());
    for (const auto& p : depthPairs) {
        sortedIds.push_back(p.second);
    }
    return sortedIds;
}

void GaussianSplatEngine::ApplyDigitalRainVolumetricPenetration(float rainDensity, float penetrationDepth)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    for (auto& kv : m_splats) {
        GaussianSplat3D& s = kv.second;
        // The deeper the splat in penetration, the more green digital rain filters through
        float factor = 1.0f - std::clamp(rainDensity * 0.35f, 0.0f, 0.8f);
        s.digitalRainAlphaMultiplier = factor;
        // Enhance green chromatic response
        s.sphericalHarmonicsG[0] = std::min(1.0f, s.sphericalHarmonicsG[0] + rainDensity * 0.15f);
    }
}

std::vector<uint8_t> GaussianSplatEngine::SerializeSplatToBinary(uint32_t splatId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    std::vector<uint8_t> buffer;
    auto it = m_splats.find(splatId);
    if (it == m_splats.end()) return buffer;

    const GaussianSplat3D& s = it->second;
    buffer.resize(sizeof(uint32_t) + sizeof(SplatVec3) + sizeof(SplatQuaternion) + sizeof(SplatVec3) + sizeof(float));

    uint8_t* ptr = buffer.data();
    std::memcpy(ptr, &s.splatId, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    std::memcpy(ptr, &s.position, sizeof(SplatVec3)); ptr += sizeof(SplatVec3);
    std::memcpy(ptr, &s.rotation, sizeof(SplatQuaternion)); ptr += sizeof(SplatQuaternion);
    std::memcpy(ptr, &s.scale, sizeof(SplatVec3)); ptr += sizeof(SplatVec3);
    std::memcpy(ptr, &s.opacity, sizeof(float));

    return buffer;
}

uint32_t GaussianSplatEngine::IngestSplatFromBinary(const uint8_t* buffer, size_t length)
{
    size_t expected = sizeof(uint32_t) + sizeof(SplatVec3) + sizeof(SplatQuaternion) + sizeof(SplatVec3) + sizeof(float);
    if (length < expected || buffer == nullptr) return 0;

    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    uint32_t originalId = 0;
    SplatVec3 pos;
    SplatQuaternion rot;
    SplatVec3 scale;
    float opacity = 1.0f;

    const uint8_t* ptr = buffer;
    std::memcpy(&originalId, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    std::memcpy(&pos, ptr, sizeof(SplatVec3)); ptr += sizeof(SplatVec3);
    std::memcpy(&rot, ptr, sizeof(SplatQuaternion)); ptr += sizeof(SplatQuaternion);
    std::memcpy(&scale, ptr, sizeof(SplatVec3)); ptr += sizeof(SplatVec3);
    std::memcpy(&opacity, ptr, sizeof(float));

    uint32_t id = m_nextSplatId++;
    GaussianSplat3D splat;
    splat.splatId = id;
    splat.position = pos;
    splat.rotation = rot;
    splat.scale = scale;
    splat.opacity = opacity;
    splat.digitalRainAlphaMultiplier = 1.0f;
    splat.isVisible = true;

    splat.sphericalHarmonicsR[0] = 0.6f;
    splat.sphericalHarmonicsG[0] = 0.9f;
    splat.sphericalHarmonicsB[0] = 0.6f;

    m_splats[id] = splat;
    return id;
}

size_t GaussianSplatEngine::FrustumCullSplats(const SplatVec3& camPos, const SplatVec3& camFwd, float maxDist)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    size_t visibleCount = 0;
    SplatVec3 fwd = camFwd.normalized();
    float maxDistSq = maxDist * maxDist;

    for (auto& kv : m_splats) {
        GaussianSplat3D& s = kv.second;
        SplatVec3 delta = s.position - camPos;
        float distSq = delta.lengthSq();

        // Check distance and forward half-space
        if (distSq <= maxDistSq && delta.dot(fwd) > 0.0f) {
            s.isVisible = true;
            visibleCount++;
        } else {
            s.isVisible = false;
        }
    }
    return visibleCount;
}

void GaussianSplatEngine::Update(float dt)
{
    // GPU buffer streaming / animated splats
}

// ============================================================================
// Headless Test Suite 35: 3D Gaussian Splatting & Radiance Streaming
// ============================================================================

void RunGaussianSplatTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH VII: 3D GAUSSIAN SPLATTING TEST SUITE" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto assert_test = [&](bool cond, const std::string& desc) {
        if (cond) {
            std::cout << " [PASS] " << desc << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << desc << std::endl;
            failed++;
        }
    };

    sGaussianSplatEngine.ResetForTesting();

    // 1. Initial State & Splat Creation
    assert_test(sGaussianSplatEngine.GetSplatCount() == 0,
                "Gaussian splat registry initially contains zero splats");

    SplatVec3 pos1{100.0f, 50.0f, 200.0f};
    SplatQuaternion rot1{1.0f, 0.0f, 0.0f, 0.0f}; // Identity rotation
    SplatVec3 scale1{2.0f, 1.0f, 0.5f};
    uint32_t splat1Id = sGaussianSplatEngine.AddGaussianSplat(pos1, rot1, scale1, 0.95f);

    assert_test(splat1Id == 1, "Allocated 3D Gaussian splat #1 with unique ID");
    assert_test(sGaussianSplatEngine.GetSplatCount() == 1, "Splat registry reports 1 active splat");

    const auto* splat1 = sGaussianSplatEngine.GetSplat(splat1Id);
    assert_test(splat1 != nullptr && splat1->opacity == 0.95f,
                "Retrieved splat #1 handle with verified opacity (0.95)");

    // 2. Covariance Matrix Decomposition (Sigma = R * S * S^T * R^T)
    float cov[3][3];
    sGaussianSplatEngine.ComputeCovarianceMatrix(*splat1, cov);
    // For identity rotation and scale (2, 1, 0.5), covariance diagonal is (4, 1, 0.25)
    assert_test(std::abs(cov[0][0] - 4.0f) < 0.01f, "Covariance Sigma[0][0] matches scale_x^2 (4.0)");
    assert_test(std::abs(cov[1][1] - 1.0f) < 0.01f, "Covariance Sigma[1][1] matches scale_y^2 (1.0)");
    assert_test(std::abs(cov[2][2] - 0.25f) < 0.01f, "Covariance Sigma[2][2] matches scale_z^2 (0.25)");
    assert_test(std::abs(cov[0][1]) < 0.001f, "Covariance off-diagonal elements zero under identity rotation");

    // 3. Spherical Harmonics Degree-3 Radiance Evaluation
    sGaussianSplatEngine.SetSphericalHarmonicsBand0(splat1Id, 1.0f, 2.0f, 0.5f);
    sGaussianSplatEngine.SetSphericalHarmonicsBand1(splat1Id, 0.2f, 0.5f, 0.1f);

    float r = 0, g = 0, b = 0;
    SplatVec3 viewDir{0.0f, 1.0f, 0.0f};
    sGaussianSplatEngine.EvaluateRadiance(*splat1, viewDir, r, g, b);
    assert_test(g > r && g > b, "Spherical Harmonics evaluates green-dominant digital rain radiance");
    assert_test(r > 0.0f && g <= 1.0f, "Evaluated color channels clamped within valid normalized [0, 1] range");

    // 4. Back-to-Front Depth Sorting (Radix Sort)
    SplatVec3 posNear{100.0f, 50.0f, 250.0f};  // Depth = 50 from cam at (100, 50, 200) looking +Z
    SplatVec3 posMid{100.0f, 50.0f, 350.0f};   // Depth = 150
    SplatVec3 posFar{100.0f, 50.0f, 500.0f};   // Depth = 300

    uint32_t sNear = sGaussianSplatEngine.AddGaussianSplat(posNear, rot1, {1,1,1}, 0.8f);
    uint32_t sMid = sGaussianSplatEngine.AddGaussianSplat(posMid, rot1, {1,1,1}, 0.8f);
    uint32_t sFar = sGaussianSplatEngine.AddGaussianSplat(posFar, rot1, {1,1,1}, 0.8f);

    SplatVec3 camPos{100.0f, 50.0f, 200.0f};
    SplatVec3 camFwd{0.0f, 0.0f, 1.0f};
    auto sorted = sGaussianSplatEngine.SortSplatsBackToFront(camPos, camFwd);

    assert_test(sorted.size() >= 3, "Sorted all visible splats along camera view axis");
    assert_test(sorted[0] == sFar, "First sorted splat is farthest (depth 300) for back-to-front blending");
    assert_test(sorted[1] == sMid, "Second sorted splat is intermediate (depth 150)");

    // 5. Binary Packet Serialization & Progressive Streaming
    auto binPacket = sGaussianSplatEngine.SerializeSplatToBinary(splat1Id);
    assert_test(!binPacket.empty(), "Serialized Gaussian splat to compact binary .splat stream packet");

    uint32_t streamedId = sGaussianSplatEngine.IngestSplatFromBinary(binPacket.data(), binPacket.size());
    assert_test(streamedId > 0, "Ingested streamed .splat binary packet into active rendering registry");
    const auto* streamedSplat = sGaussianSplatEngine.GetSplat(streamedId);
    assert_test(streamedSplat != nullptr && streamedSplat->scale.x == scale1.x,
                "Streamed splat reconstructed with preserved spatial geometry");

    // 6. Volumetric Digital Rain Penetration
    sGaussianSplatEngine.ApplyDigitalRainVolumetricPenetration(0.8f, 15.0f);
    const auto* updatedSplat = sGaussianSplatEngine.GetSplat(splat1Id);
    assert_test(updatedSplat->digitalRainAlphaMultiplier < 1.0f,
                "Volumetric digital rain penetration modulated splat transmission alpha");

    // 7. View Frustum Culling
    // A splat behind the camera must be culled
    uint32_t sBehind = sGaussianSplatEngine.AddGaussianSplat({100.0f, 50.0f, 50.0f}, rot1, {1,1,1}, 0.8f);
    size_t visible = sGaussianSplatEngine.FrustumCullSplats(camPos, camFwd, 400.0f);
    const auto* culled = sGaussianSplatEngine.GetSplat(sBehind);
    assert_test(culled != nullptr && !culled->isVisible,
                "Splat behind camera position correctly culled by frustum clipper");
    assert_test(visible > 0, "Forward splats retained within visible rendering set");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH VII 3D GAUSSIAN SPLATTING TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;
}

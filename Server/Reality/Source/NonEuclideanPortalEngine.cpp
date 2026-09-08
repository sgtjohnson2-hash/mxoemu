#include "NonEuclideanPortalEngine.h"
#include "Common.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <sstream>

createFileSingleton(NonEuclideanPortalEngine);

NonEuclideanPortalEngine::NonEuclideanPortalEngine()
{
}

NonEuclideanPortalEngine::~NonEuclideanPortalEngine()
{
}

void NonEuclideanPortalEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    m_portals.clear();
    m_expiringPortals.clear();
    m_nextPortalId = 1;

    boost::format fmt("NonEuclideanPortalEngine initialized with continuous spatial folding & momentum conservation.");
    INFO_LOG(fmt);
}

void NonEuclideanPortalEngine::ResetForTesting()
{
    Initialize();
}

uint32_t NonEuclideanPortalEngine::RegisterPortal(const std::string& name, const PortalVec3& pos,
                                                  const PortalVec3& normal, float width, float height)
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    uint32_t id = m_nextPortalId++;

    PortalAperture ap;
    ap.portalId = id;
    ap.targetPortalId = 0;
    ap.name = name;
    ap.position = pos;
    ap.normal = normal.normalized();
    ap.up = {0.0f, 1.0f, 0.0f};
    ap.width = width;
    ap.height = height;
    ap.isBiDirectional = true;
    ap.isAcousticallyTransmissive = true;
    ap.acousticAttenuationDb = -3.0f;
    ap.flipsGravity = false;
    ap.enablesSurfaceCeilingRide = false;
    ap.transformMatrix = PortalMatrix4x4::Identity();
    ap.totalTraversals = 0;

    m_portals[id] = ap;
    return id;
}

void NonEuclideanPortalEngine::ComputePortalTransformMatrix(PortalAperture& src, const PortalAperture& dst)
{
    // Compute coordinate frame mapping from src aperture plane to dst aperture plane
    // Translation delta
    PortalVec3 deltaPos = dst.position - src.position;

    // Build rotation mapping: reflect normal across 180 degrees so entering src exits dst forward
    PortalMatrix4x4 trans = PortalMatrix4x4::Translation(deltaPos);
    
    // Check relative facing angle between normals
    float dot = src.normal.dot(dst.normal);
    if (std::abs(dot + 1.0f) < 0.001f) {
        // Face to face: straight displacement
        src.transformMatrix = trans;
    } else {
        // Angled portals: 180 degree rotation around Y combined with translation
        PortalMatrix4x4 rot = PortalMatrix4x4::RotationY(3.14159265f);
        src.transformMatrix = trans * rot;
    }
}

bool NonEuclideanPortalEngine::LinkPortals(uint32_t portalAId, uint32_t portalBId)
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    auto itA = m_portals.find(portalAId);
    auto itB = m_portals.find(portalBId);
    if (itA == m_portals.end() || itB == m_portals.end()) {
        return false;
    }

    itA->second.targetPortalId = portalBId;
    itB->second.targetPortalId = portalAId;

    ComputePortalTransformMatrix(itA->second, itB->second);
    ComputePortalTransformMatrix(itB->second, itA->second);

    return true;
}

const PortalAperture* NonEuclideanPortalEngine::GetPortal(uint32_t portalId) const
{
    std::shared_lock<std::shared_mutex> lock(m_portalMutex);
    auto it = m_portals.find(portalId);
    if (it != m_portals.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t NonEuclideanPortalEngine::GetPortalCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_portalMutex);
    return m_portals.size();
}

bool NonEuclideanPortalEngine::CheckEntityTraversal(uint32_t portalId, const PortalVec3& prevPos,
                                                    const PortalVec3& currPos, const PortalVec3& inVel,
                                                    PortalVec3& outNewPos, PortalVec3& outNewVel,
                                                    bool& outGravityFlipped)
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    auto it = m_portals.find(portalId);
    if (it == m_portals.end() || it->second.targetPortalId == 0) {
        return false;
    }

    PortalAperture& src = it->second;
    auto itDst = m_portals.find(src.targetPortalId);
    if (itDst == m_portals.end()) return false;
    PortalAperture& dst = itDst->second;

    // Plane intersection test: D = -normal . pos
    float d = -src.normal.dot(src.position);
    float distPrev = src.normal.dot(prevPos) + d;
    float distCurr = src.normal.dot(currPos) + d;

    // Crossed plane in either direction
    if ((distPrev >= 0.0f && distCurr < 0.0f) || (distPrev <= 0.0f && distCurr > 0.0f)) {
        float absP = std::abs(distPrev);
        float absC = std::abs(distCurr);
        float sum = absP + absC;
        float t = (sum > 0.00001f) ? (absP / sum) : 0.5f;
        PortalVec3 intersect = prevPos + (currPos - prevPos) * t;

        // Check bounds within aperture width and height
        PortalVec3 localDelta = intersect - src.position;
        float lateralDist = std::abs(localDelta.dot(src.normal.cross(src.up).normalized()));
        float verticalDist = std::abs(localDelta.dot(src.up));

        if (lateralDist <= (src.width * 0.5f) && verticalDist <= (src.height * 0.5f)) {
            // Traversal confirmed! Transform position to destination portal
            PortalVec3 offsetFromPlane = currPos - intersect;
            outNewPos = dst.position + src.transformMatrix.transformVector(offsetFromPlane);

            // Momentum Conservation: preserve velocity magnitude exactly
            float initialSpeed = inVel.length();
            PortalVec3 transformedDir = src.transformMatrix.transformVector(inVel).normalized();
            outNewVel = transformedDir * initialSpeed;

            outGravityFlipped = src.flipsGravity;
            src.totalTraversals++;
            return true;
        }
    }

    return false;
}

bool NonEuclideanPortalEngine::TraceBallisticRay(const PortalVec3& rayOrigin, const PortalVec3& rayDir,
                                                 float maxDist, PortalVec3& outFinalPos,
                                                 uint32_t& outTraversedCount, int maxRecursion)
{
    std::shared_lock<std::shared_mutex> lock(m_portalMutex);
    outTraversedCount = 0;

    PortalVec3 currentOrigin = rayOrigin;
    PortalVec3 currentDir = rayDir.normalized();
    float remainingDist = maxDist;
    uint32_t ignorePortalId = 0;

    for (int step = 0; step < maxRecursion; ++step) {
        bool hitPortal = false;
        float nearestHitT = remainingDist;
        const PortalAperture* hitSrc = nullptr;

        for (const auto& kv : m_portals) {
            const PortalAperture& ap = kv.second;
            if (ap.portalId == ignorePortalId || ap.targetPortalId == 0) continue;

            float denom = ap.normal.dot(currentDir);
            if (std::abs(denom) > 0.0001f) {
                float t = (ap.position - currentOrigin).dot(ap.normal) / denom;
                if (t > 0.001f && t < nearestHitT) {
                    // Check if hit point is within portal aperture rectangle
                    PortalVec3 hitP = currentOrigin + currentDir * t;
                    PortalVec3 localDelta = hitP - ap.position;
                    float lat = std::abs(localDelta.dot(ap.normal.cross(ap.up).normalized()));
                    float vert = std::abs(localDelta.dot(ap.up));

                    if (lat <= (ap.width * 0.5f) && vert <= (ap.height * 0.5f)) {
                        nearestHitT = t;
                        hitSrc = &ap;
                        hitPortal = true;
                    }
                }
            }
        }

        if (hitPortal && hitSrc != nullptr) {
            outTraversedCount++;
            auto itDst = m_portals.find(hitSrc->targetPortalId);
            if (itDst == m_portals.end()) break;

            const PortalAperture& dst = itDst->second;
            PortalVec3 hitPos = currentOrigin + currentDir * nearestHitT;
            PortalVec3 localOffset = hitPos - hitSrc->position;
            
            // Warp ray to destination portal
            PortalVec3 newDir = hitSrc->transformMatrix.transformVector(currentDir).normalized();
            currentOrigin = dst.position + localOffset + newDir * 0.05f; // Epsilon forward in destination space
            currentDir = newDir;
            ignorePortalId = hitSrc->targetPortalId;
            remainingDist -= nearestHitT;

            if (remainingDist <= 0.0f) {
                outFinalPos = currentOrigin;
                return true;
            }
        } else {
            // Reached max distance in open space
            outFinalPos = currentOrigin + currentDir * remainingDist;
            return true;
        }
    }

    outFinalPos = currentOrigin + currentDir * remainingDist;
    return true;
}

void NonEuclideanPortalEngine::SetupKleinBottleSubwayLoop(uint32_t portalInId, uint32_t portalOutId)
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    auto itIn = m_portals.find(portalInId);
    auto itOut = m_portals.find(portalOutId);
    if (itIn != m_portals.end()) {
        itIn->second.flipsGravity = true;
    }
    if (itOut != m_portals.end()) {
        itOut->second.flipsGravity = true;
    }
}

void NonEuclideanPortalEngine::SetupMobiusStripHighwayCircuit(uint32_t portalInId, uint32_t portalOutId)
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    auto itIn = m_portals.find(portalInId);
    auto itOut = m_portals.find(portalOutId);
    if (itIn != m_portals.end()) {
        itIn->second.enablesSurfaceCeilingRide = true;
    }
    if (itOut != m_portals.end()) {
        itOut->second.enablesSurfaceCeilingRide = true;
    }
}

bool NonEuclideanPortalEngine::CalculateAcousticDiffraction(uint32_t portalId, const PortalVec3& listenerPos,
                                                           const PortalVec3& soundSourcePos,
                                                           AcousticDiffractionResult& outResult) const
{
    std::shared_lock<std::shared_mutex> lock(m_portalMutex);
    auto it = m_portals.find(portalId);
    if (it == m_portals.end() || it->second.targetPortalId == 0) {
        return false;
    }

    const PortalAperture& src = it->second;
    auto itDst = m_portals.find(src.targetPortalId);
    if (itDst == m_portals.end()) return false;
    const PortalAperture& dst = itDst->second;

    // Virtual source is positioned at the portal aperture facing the listener
    outResult.virtualSourcePosition = dst.position;
    
    // Distance calculation: (Source to src portal) + (dst portal to listener)
    float distToSrc = (src.position - soundSourcePos).length();
    float distFromDst = (listenerPos - dst.position).length();
    outResult.pathDistance = distToSrc + distFromDst;

    // Portal aperture attenuation + inverse distance falloff (dB)
    float baseAttenuation = src.acousticAttenuationDb;
    float distDbLoss = -20.0f * std::log10(std::max(1.0f, outResult.pathDistance / 10.0f));
    outResult.finalVolumeDb = baseAttenuation + distDbLoss;
    outResult.spreadAngleDeg = 65.0f;

    return true;
}

uint32_t NonEuclideanPortalEngine::PlaceOperatorTacticalPortal(uint32_t operatorGoId, const PortalVec3& pos,
                                                               const PortalVec3& normal, float durationSec)
{
    std::string name = "OperatorExtractionPortal_" + std::to_string(operatorGoId);
    uint32_t portalId = RegisterPortal(name, pos, normal, 120.0f, 220.0f);

    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    m_expiringPortals.push_back({portalId, durationSec});
    return portalId;
}

void NonEuclideanPortalEngine::Update(float dt)
{
    std::unique_lock<std::shared_mutex> lock(m_portalMutex);
    for (auto it = m_expiringPortals.begin(); it != m_expiringPortals.end();) {
        it->remainingSeconds -= dt;
        if (it->remainingSeconds <= 0.0f) {
            m_portals.erase(it->portalId);
            it = m_expiringPortals.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Test Suite 32: Non-Euclidean Spatial Folding & Portals
// ============================================================================

void RunNonEuclideanPortalTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH VI: NON-EUCLIDEAN PORTAL & SPATIAL FOLD SUITE" << std::endl;
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

    sNonEuclideanPortalEngine.ResetForTesting();

    // 1. Initial State & Portal Registration
    assert_test(sNonEuclideanPortalEngine.GetPortalCount() == 0, "Portal engine initially contains 0 active apertures");

    uint32_t clubHelId = sNonEuclideanPortalEngine.RegisterPortal(
        "Club-Hel-Mirror", {1000.0f, 50.0f, 2000.0f}, {0.0f, 0.0f, -1.0f}, 200.0f, 300.0f
    );
    uint32_t mobilAveId = sNonEuclideanPortalEngine.RegisterPortal(
        "Mobil-Ave-Limbo-Door", {5000.0f, 50.0f, -8000.0f}, {0.0f, 0.0f, 1.0f}, 200.0f, 300.0f
    );

    assert_test(clubHelId == 1 && mobilAveId == 2, "Registered dual portal apertures with valid IDs");
    assert_test(sNonEuclideanPortalEngine.GetPortalCount() == 2, "Portal registry reports 2 active apertures");

    // 2. Bidirectional Portal Linking
    bool linked = sNonEuclideanPortalEngine.LinkPortals(clubHelId, mobilAveId);
    assert_test(linked, "Successfully linked Club-Hel-Mirror <-> Mobil-Ave-Limbo-Door");

    const PortalAperture* apClub = sNonEuclideanPortalEngine.GetPortal(clubHelId);
    const PortalAperture* apMobil = sNonEuclideanPortalEngine.GetPortal(mobilAveId);
    assert_test(apClub != nullptr && apClub->targetPortalId == mobilAveId, "Club Hel target points to Mobil Ave");
    assert_test(apMobil != nullptr && apMobil->targetPortalId == clubHelId, "Mobil Ave target points to Club Hel");

    // 3. Coordinate Transformation & Traversal Verification
    PortalVec3 prevPos{1000.0f, 50.0f, 2010.0f}; // Front of Club Hel portal
    PortalVec3 currPos{1000.0f, 50.0f, 1990.0f}; // Stepped through aperture plane
    PortalVec3 inVel{0.0f, 0.0f, -450.0f};        // Moving forward at 450 units/sec

    PortalVec3 outPos, outVel;
    bool gravityFlipped = false;

    bool traversed = sNonEuclideanPortalEngine.CheckEntityTraversal(
        clubHelId, prevPos, currPos, inVel, outPos, outVel, gravityFlipped
    );

    assert_test(traversed, "Entity traversal detected across portal aperture plane");
    assert_test(std::abs(outPos.x - 5000.0f) < 5.0f, "Entity teleported to destination X coordinate");
    assert_test(!gravityFlipped, "Standard portal does not invert gravity");

    // 4. Momentum Conservation
    float initialSpeed = inVel.length();
    float outputSpeed = outVel.length();
    assert_test(std::abs(initialSpeed - outputSpeed) < 0.01f,
                "Kinematic momentum magnitude strictly conserved across spatial fold (" + std::to_string(outputSpeed) + " units/s)");
    assert_test(apClub->totalTraversals == 1, "Traversal metrics incremented for source aperture");

    // 5. Supersonic Ballistic Ray Continuous Collision Detection (CCD)
    PortalVec3 sniperOrigin{1000.0f, 50.0f, 3000.0f}; // 1000 units away from Club Hel portal
    PortalVec3 sniperDir{0.0f, 0.0f, -1.0f};          // Firing directly into portal
    PortalVec3 finalImpact;
    uint32_t traversedPortals = 0;

    sNonEuclideanPortalEngine.TraceBallisticRay(
        sniperOrigin, sniperDir, 2500.0f, finalImpact, traversedPortals
    );
    assert_test(traversedPortals == 1, "Supersonic ballistic ray traversed portal aperture without tunneling");
    assert_test(finalImpact.z < -8000.0f, "Ballistic projectile exited destination portal in Mobil Ave space");

    // 6. Klein-Bottle Subway Gravity Inversion
    uint32_t kleinIn = sNonEuclideanPortalEngine.RegisterPortal(
        "Slums-Klein-In", {-1000.0f, -200.0f, 500.0f}, {0.0f, 0.0f, 1.0f}
    );
    uint32_t kleinOut = sNonEuclideanPortalEngine.RegisterPortal(
        "Slums-Klein-Out", {-1000.0f, -200.0f, 1500.0f}, {0.0f, 0.0f, -1.0f}
    );
    sNonEuclideanPortalEngine.LinkPortals(kleinIn, kleinOut);
    sNonEuclideanPortalEngine.SetupKleinBottleSubwayLoop(kleinIn, kleinOut);

    PortalVec3 kPrev{-1000.0f, -200.0f, 490.0f};
    PortalVec3 kCurr{-1000.0f, -200.0f, 510.0f};
    PortalVec3 kVel{0.0f, 0.0f, 300.0f};
    PortalVec3 kOutPos, kOutVel;
    bool kGravFlipped = false;

    sNonEuclideanPortalEngine.CheckEntityTraversal(
        kleinIn, kPrev, kCurr, kVel, kOutPos, kOutVel, kGravFlipped
    );
    assert_test(kGravFlipped == true, "Klein-Bottle transit successfully inverted local gravitational up-vector (g -> -g)");

    // 7. Möbius Strip Highway Geometry
    uint32_t mobiusIn = sNonEuclideanPortalEngine.RegisterPortal(
        "Downtown-Mobius-In", {3000.0f, 200.0f, 0.0f}, {1.0f, 0.0f, 0.0f}
    );
    uint32_t mobiusOut = sNonEuclideanPortalEngine.RegisterPortal(
        "Downtown-Mobius-Out", {4000.0f, 200.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}
    );
    sNonEuclideanPortalEngine.LinkPortals(mobiusIn, mobiusOut);
    sNonEuclideanPortalEngine.SetupMobiusStripHighwayCircuit(mobiusIn, mobiusOut);

    const PortalAperture* apMobius = sNonEuclideanPortalEngine.GetPortal(mobiusIn);
    assert_test(apMobius->enablesSurfaceCeilingRide == true,
                "Möbius strip highway enabled surface ceiling wall-ride for vehicles");

    // 8. Dynamic Acoustic Diffraction
    AcousticDiffractionResult audioRes;
    PortalVec3 gunshotPos{1000.0f, 50.0f, 2500.0f}; // Behind Club Hel portal
    PortalVec3 redpillListenerPos{5000.0f, 50.0f, -7800.0f}; // In Mobil Ave listening

    bool diffracted = sNonEuclideanPortalEngine.CalculateAcousticDiffraction(
        clubHelId, redpillListenerPos, gunshotPos, audioRes
    );
    assert_test(diffracted, "Calculated acoustic diffraction through portal aperture");
    assert_test(audioRes.virtualSourcePosition.equals({5000.0f, 50.0f, -8000.0f}),
                "Virtual sound source positioned exactly at destination portal aperture");
    assert_test(audioRes.finalVolumeDb < 0.0f, "Acoustic signal attenuated through portal doorway");

    // 9. Operator Dynamic Tactical Portal Placement & Lifecycle
    uint32_t opPortalId = sNonEuclideanPortalEngine.PlaceOperatorTacticalPortal(
        9001, {200.0f, 10.0f, 300.0f}, {0.0f, 1.0f, 0.0f}, 5.0f
    );
    assert_test(opPortalId != 0, "Redpill operator successfully spawned tactical extraction portal");
    assert_test(sNonEuclideanPortalEngine.GetPortal(opPortalId) != nullptr, "Operator portal exists in active registry");

    // Advance time by 6 seconds to verify automatic timeout decay
    sNonEuclideanPortalEngine.Update(6.0f);
    assert_test(sNonEuclideanPortalEngine.GetPortal(opPortalId) == nullptr,
                "Operator tactical portal expired and recycled after duration timeout");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH VI NON-EUCLIDEAN PORTAL TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    assert(failed == 0);
}

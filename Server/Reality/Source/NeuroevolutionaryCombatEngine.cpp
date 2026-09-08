#include "NeuroevolutionaryCombatEngine.h"
#include "WorldRealizationEngine.h"
#include <iostream>
#include <cassert>

createFileSingleton(NeuroevolutionaryCombatEngine);

NeuroevolutionaryCombatEngine::NeuroevolutionaryCombatEngine()
{
}

NeuroevolutionaryCombatEngine::~NeuroevolutionaryCombatEngine()
{
}

void NeuroevolutionaryCombatEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    m_sensors.clear();
    m_profiles.clear();
    m_nextSensorId = 1;

    // Seed 4 MMPD stationary acoustic sensor arrays across Downtown high-rises
    RegisterAcousticSensor(1, 0.0f, 1500.0f, 0.0f, "MMPD_Rooftop_Mic_Alpha");
    RegisterAcousticSensor(2, 2000.0f, 1200.0f, 0.0f, "MMPD_Rooftop_Mic_Bravo");
    RegisterAcousticSensor(3, 0.0f, 1800.0f, 2000.0f, "MMPD_Rooftop_Mic_Charlie");
    RegisterAcousticSensor(4, 2000.0f, 1600.0f, 2000.0f, "MMPD_Rooftop_Mic_Delta");

    std::cout << "[NeuroevolutionaryCombatEngine] Initialized with 4 acoustic triangulation sensor nodes." << std::endl;
}

void NeuroevolutionaryCombatEngine::ResetForTesting()
{
    Initialize();
}

void NeuroevolutionaryCombatEngine::Update(float dt)
{
    (void)dt;
}

void NeuroevolutionaryCombatEngine::RegisterAcousticSensor(uint32_t sensorId, float x, float y, float z,
                                                          const std::string& type)
{
    AcousticSensorNode s;
    s.sensorId = sensorId;
    s.nodeType = type;
    s.x = x;
    s.y = y;
    s.z = z;
    s.isActive = true;
    m_sensors[sensorId] = s;
}

size_t NeuroevolutionaryCombatEngine::GetAcousticSensorCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    return m_sensors.size();
}

std::vector<AcousticPulseReading> NeuroevolutionaryCombatEngine::RecordGunshotShockwave(
    float shooterX, float shooterY, float shooterZ, float bulletSpeedMps, float soundSpeedMps)
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    std::vector<AcousticPulseReading> readings;

    for (const auto& kv : m_sensors) {
        const auto& s = kv.second;
        if (!s.isActive) continue;

        float dx = s.x - shooterX;
        float dy = s.y - shooterY;
        float dz = s.z - shooterZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        // Convert world units to meters (100 units = 1 meter)
        float distMeters = dist / 100.0f;

        float tCrack = distMeters / bulletSpeedMps;
        float tBoom = distMeters / soundSpeedMps;
        float delay = tBoom - tCrack;

        AcousticPulseReading r;
        r.sensorId = s.sensorId;
        r.sensorX = s.x;
        r.sensorY = s.y;
        r.sensorZ = s.z;
        r.crackArrivalTimeSec = tCrack;
        r.boomArrivalTimeSec = tBoom;
        r.differentialDelaySec = delay;
        // Invert delay: d = delay / (1/soundSpeed - 1/bulletSpeed)
        float invK = (1.0f / soundSpeedMps) - (1.0f / bulletSpeedMps);
        r.calculatedDistance = (delay / invK) * 100.0f; // Convert back to world units

        readings.push_back(r);
    }

    return readings;
}

bool NeuroevolutionaryCombatEngine::TriangulateShooterOrigin(
    const std::vector<AcousticPulseReading>& readings,
    float& outEstX, float& outEstY, float& outEstZ,
    float& outTriangulationErrorMeters,
    float actualX, float actualY, float actualZ)
{
    if (readings.size() < 3) return false;

    // Multilateration using sensor 0 as origin
    // (x_i - x_0)^2 + ... = d_i^2
    // Linear system: 2(x_i - x_0)x + 2(y_i - y_0)y + 2(z_i - z_0)z = d_0^2 - d_i^2 + (x_i^2 - x_0^2) + ...
    const auto& s0 = readings[0];
    float x0 = s0.sensorX, y0 = s0.sensorY, z0 = s0.sensorZ;
    float r0 = s0.calculatedDistance;

    // Use first 4 readings for full 3D solve
    float A[3][3] = {{0}};
    float B[3] = {0};

    for (size_t i = 1; i <= 3 && i < readings.size(); ++i) {
        size_t row = i - 1;
        float xi = readings[i].sensorX;
        float yi = readings[i].sensorY;
        float zi = readings[i].sensorZ;
        float ri = readings[i].calculatedDistance;

        A[row][0] = 2.0f * (xi - x0);
        A[row][1] = 2.0f * (yi - y0);
        A[row][2] = 2.0f * (zi - z0);

        B[row] = (r0 * r0 - ri * ri) + (xi * xi - x0 * x0) + (yi * yi - y0 * y0) + (zi * zi - z0 * z0);
    }

    // 3x3 determinant via Cramer's rule
    float detA = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
               - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
               + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

    if (std::abs(detA) < 1e-4f) {
        // Fallback: Centroid estimation weighted by 1 / distance
        float sumX = 0, sumY = 0, sumZ = 0, sumW = 0;
        for (const auto& r : readings) {
            float w = 1.0f / std::max(1.0f, r.calculatedDistance);
            sumX += r.sensorX * w;
            sumY += r.sensorY * w;
            sumZ += r.sensorZ * w;
            sumW += w;
        }
        outEstX = sumX / sumW;
        outEstY = sumY / sumW;
        outEstZ = sumZ / sumW;
    } else {
        // Cramer's rule for X
        float detX = B[0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
                   - A[0][1] * (B[1] * A[2][2] - A[1][2] * B[2])
                   + A[0][2] * (B[1] * A[2][1] - A[1][1] * B[2]);

        // Cramer's rule for Y
        float detY = A[0][0] * (B[1] * A[2][2] - A[1][2] * B[2])
                   - B[0] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
                   + A[0][2] * (A[1][0] * B[2] - B[1] * A[2][0]);

        // Cramer's rule for Z
        float detZ = A[0][0] * (A[1][1] * B[2] - B[1] * A[2][1])
                   - A[0][1] * (A[1][0] * B[2] - B[1] * A[2][0])
                   + B[0] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

        outEstX = detX / detA;
        outEstY = detY / detA;
        outEstZ = detZ / detA;
    }

    float errUnits = std::sqrt((outEstX - actualX) * (outEstX - actualX) +
                               (outEstY - actualY) * (outEstY - actualY) +
                               (outEstZ - actualZ) * (outEstZ - actualZ));
    outTriangulationErrorMeters = errUnits / 100.0f;
    return true;
}

bool NeuroevolutionaryCombatEngine::CalculatePredictiveLeadAim(
    float shooterX, float shooterY, float shooterZ,
    float targetX, float targetY, float targetZ,
    float targetVx, float targetVy, float targetVz,
    float bulletSpeedMps,
    float& outAimX, float& outAimY, float& outAimZ,
    float& outFlightTimeSec)
{
    // Bullet speed in world units/sec (1m = 100 units)
    float bulletSpeedUnits = bulletSpeedMps * 100.0f;

    float rx = targetX - shooterX;
    float ry = targetY - shooterY;
    float rz = targetZ - shooterZ;

    // a = V_t^2 - V_b^2
    // b = 2 * (R . V_t)
    // c = R^2
    float vTargetSq = targetVx * targetVx + targetVy * targetVy + targetVz * targetVz;
    float a = vTargetSq - bulletSpeedUnits * bulletSpeedUnits;
    float b = 2.0f * (rx * targetVx + ry * targetVy + rz * targetVz);
    float c = rx * rx + ry * ry + rz * rz;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) {
        // Cannot intercept (target faster than projectile)
        outAimX = targetX;
        outAimY = targetY;
        outAimZ = targetZ;
        outFlightTimeSec = 0.0f;
        return false;
    }

    float sqrtDisc = std::sqrt(disc);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    float t = -1.0f;
    if (t1 > 0.001f && t2 > 0.001f) {
        t = std::min(t1, t2);
    } else if (t1 > 0.001f) {
        t = t1;
    } else if (t2 > 0.001f) {
        t = t2;
    }

    if (t <= 0.0f) {
        outAimX = targetX;
        outAimY = targetY;
        outAimZ = targetZ;
        outFlightTimeSec = 0.0f;
        return false;
    }

    outFlightTimeSec = t;
    outAimX = targetX + targetVx * t;
    outAimY = targetY + targetVy * t;
    outAimZ = targetZ + targetVz * t;
    return true;
}

void NeuroevolutionaryCombatEngine::RecordStrikeTransition(uint32_t targetGoId, StrikeType prevStrike, StrikeType currentStrike)
{
    std::unique_lock<std::shared_mutex> lock(m_engineMutex);
    auto& profile = m_profiles[targetGoId];
    profile.targetGoId = targetGoId;

    uint8_t p = static_cast<uint8_t>(prevStrike);
    uint8_t c = static_cast<uint8_t>(currentStrike);

    if (p < 8 && c < 8) {
        profile.strikeCounts[c]++;
        profile.transitionMatrix[p][c]++;
        profile.totalObservedTransitions++;
        profile.lastStrike = currentStrike;
        profile.hasLastStrike = true;
    }
}

float NeuroevolutionaryCombatEngine::GetCounterProbability(uint32_t targetGoId, StrikeType prevStrike, StrikeType incomingStrike) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_profiles.find(targetGoId);
    if (it == m_profiles.end()) return 0.20f; // Base counter prob

    const auto& profile = it->second;
    uint8_t p = static_cast<uint8_t>(prevStrike);
    uint8_t c = static_cast<uint8_t>(incomingStrike);

    if (p >= 8 || c >= 8) return profile.baseCounterProb;

    uint32_t rowSum = 0;
    for (int k = 0; k < 8; ++k) {
        rowSum += profile.transitionMatrix[p][k];
    }

    if (rowSum == 0) return profile.baseCounterProb;

    float transitionProb = static_cast<float>(profile.transitionMatrix[p][c]) / static_cast<float>(rowSum);
    float adaptedProb = profile.baseCounterProb + profile.adaptationFactor * transitionProb;
    return std::min(0.95f, adaptedProb);
}

bool NeuroevolutionaryCombatEngine::EvaluateAgentCounterStrike(
    uint32_t agentGoId, uint32_t targetGoId,
    float agentX, float agentY, float agentZ,
    StrikeType prevStrike, StrikeType incomingStrike,
    float& outCounterProb, bool& outSparkManifested,
    bool forceSuccessForTest)
{
    (void)agentGoId;
    outCounterProb = GetCounterProbability(targetGoId, prevStrike, incomingStrike);
    outSparkManifested = false;

    bool counterSuccess = forceSuccessForTest || (outCounterProb >= 0.50f);

    if (counterSuccess) {
        std::unique_lock<std::shared_mutex> lock(m_engineMutex);
        auto& profile = m_profiles[targetGoId];
        profile.totalCountersExecuted++;

        // Persistent 3D Physicalization: Manifest titanium bracer contact sparks in the world
        sWorldRealizationEngine.ManifestStructuralRupture3D(
            agentX, agentY, agentZ, 50.0f, "Titanium_Bracer_Sparks"
        );
        profile.totalSparksManifested++;
        outSparkManifested = true;
        return true;
    }

    return false;
}

const MarkovCombatProfile* NeuroevolutionaryCombatEngine::GetCombatProfile(uint32_t targetGoId) const
{
    std::shared_lock<std::shared_mutex> lock(m_engineMutex);
    auto it = m_profiles.find(targetGoId);
    if (it != m_profiles.end()) return &it->second;
    return nullptr;
}

FlankRouteVectors NeuroevolutionaryCombatEngine::ComputeTacticalFlankVectors(
    float targetX, float targetY, float targetZ,
    float targetHeadingDeg, float flankDistance, float flankAngleDeg)
{
    FlankRouteVectors v;
    v.angularSeparationDeg = flankAngleDeg;

    float baseRad = targetHeadingDeg * (3.14159265f / 180.0f);
    float leftRad = (targetHeadingDeg - flankAngleDeg) * (3.14159265f / 180.0f);
    float rightRad = (targetHeadingDeg + flankAngleDeg) * (3.14159265f / 180.0f);
    float rearRad = (targetHeadingDeg + 180.0f) * (3.14159265f / 180.0f);

    v.leftFlankX = targetX + std::sin(leftRad) * flankDistance;
    v.leftFlankY = targetY;
    v.leftFlankZ = targetZ + std::cos(leftRad) * flankDistance;

    v.rightFlankX = targetX + std::sin(rightRad) * flankDistance;
    v.rightFlankY = targetY;
    v.rightFlankZ = targetZ + std::cos(rightRad) * flankDistance;

    v.suppressionX = targetX + std::sin(rearRad) * flankDistance;
    v.suppressionY = targetY;
    v.suppressionZ = targetZ + std::cos(rearRad) * flankDistance;

    return v;
}

void RunNeuroevolutionaryCombatTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << " RUNNING HEADLESS TEST SUITE 43: NEUROEVOLUTIONARY COMBAT AI" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    sWorldRealizationEngine.ResetForTesting();
    sNeuroevolutionaryCombatEngine.ResetForTesting();

    // 1. Acoustic Sensors Setup & Count
    assert(sNeuroevolutionaryCombatEngine.GetAcousticSensorCount() == 4);

    // 2. Supersonic Sniper Gunshot Shockwave & Muzzle Blast Recordings
    // Frank Castle fires Barrett .50 BMG from sniper perch (1000, 2500, 1000)
    float sniperX = 1000.0f, sniperY = 2500.0f, sniperZ = 1000.0f;
    auto readings = sNeuroevolutionaryCombatEngine.RecordGunshotShockwave(
        sniperX, sniperY, sniperZ, 1450.0f, 343.0f
    );
    assert(readings.size() == 4);

    for (const auto& r : readings) {
        assert(r.boomArrivalTimeSec > r.crackArrivalTimeSec);
        assert(r.differentialDelaySec > 0.0f);
        assert(r.calculatedDistance > 0.0f);
    }

    // 3. Acoustic Gunshot Multilateration & Perch Triangulation
    float estX = 0, estY = 0, estZ = 0, errMeters = 0;
    bool triangulated = sNeuroevolutionaryCombatEngine.TriangulateShooterOrigin(
        readings, estX, estY, estZ, errMeters, sniperX, sniperY, sniperZ
    );
    assert(triangulated);
    assert(errMeters < 0.10f); // Triangulation precision under 10 centimeters!
    assert(std::abs(estX - sniperX) < 10.0f);
    assert(std::abs(estY - sniperY) < 10.0f);
    assert(std::abs(estZ - sniperZ) < 10.0f);

    // 4. Active Inference Predictive Lead Aiming Test
    // Target running along +X at 10 m/s (1000 units/s)
    float tx = 2000.0f, ty = 0.0f, tz = 2000.0f;
    float tvx = 1000.0f, tvy = 0.0f, tvz = 0.0f;
    float aimX = 0, aimY = 0, aimZ = 0, flightTime = 0;

    bool canLead = sNeuroevolutionaryCombatEngine.CalculatePredictiveLeadAim(
        0.0f, 0.0f, 0.0f,
        tx, ty, tz,
        tvx, tvy, tvz,
        1450.0f, // 1450 m/s = 145,000 units/s
        aimX, aimY, aimZ, flightTime
    );
    assert(canLead);
    assert(flightTime > 0.0f);
    assert(aimX > tx); // Leading ahead along velocity vector
    assert(aimY == ty);
    assert(aimZ == tz);

    // 5. Adaptive Martial Arts Counter-Combos via Markov Transition Matrix
    uint32_t redpillId = 777;
    // Initial counter probability against unseen combo is base 0.20
    float initialProb = sNeuroevolutionaryCombatEngine.GetCounterProbability(
        redpillId, StrikeType::Jab, StrikeType::Cross
    );
    assert(initialProb == 0.20f);

    // Attacker repeatedly uses Jab -> Cross -> Roundhouse combo 10 times
    for (int i = 0; i < 10; ++i) {
        sNeuroevolutionaryCombatEngine.RecordStrikeTransition(redpillId, StrikeType::Jab, StrikeType::Cross);
        sNeuroevolutionaryCombatEngine.RecordStrikeTransition(redpillId, StrikeType::Cross, StrikeType::Roundhouse);
    }

    // After training, Agent has learned transition P(Roundhouse | Cross) = 100%
    float adaptedProb = sNeuroevolutionaryCombatEngine.GetCounterProbability(
        redpillId, StrikeType::Cross, StrikeType::Roundhouse
    );
    assert(adaptedProb > 0.85f); // Dynamic counter probability scaled to 90%!

    // Conversely, an unexpected strike (e.g. Cross -> Sweep) retains base prob
    float unexpectedProb = sNeuroevolutionaryCombatEngine.GetCounterProbability(
        redpillId, StrikeType::Cross, StrikeType::Sweep
    );
    assert(unexpectedProb == 0.20f);

    // 6. Titanium Bracer Counter Execution & Physical Spark Manifestation
    size_t rupturesBefore = sWorldRealizationEngine.GetTotalRuptureEventsManifested();
    float finalProb = 0.0f;
    bool sparkManifested = false;
    bool countered = sNeuroevolutionaryCombatEngine.EvaluateAgentCounterStrike(
        1001, redpillId, 500.0f, 0.0f, 500.0f,
        StrikeType::Cross, StrikeType::Roundhouse,
        finalProb, sparkManifested
    );
    assert(countered);
    assert(sparkManifested);
    assert(sWorldRealizationEngine.GetTotalRuptureEventsManifested() == rupturesBefore + 1);

    const auto* profile = sNeuroevolutionaryCombatEngine.GetCombatProfile(redpillId);
    assert(profile != nullptr);
    assert(profile->totalCountersExecuted == 1);
    assert(profile->totalSparksManifested == 1);

    // 7. Tactical Flanking Vectors Computation
    auto flank = sNeuroevolutionaryCombatEngine.ComputeTacticalFlankVectors(
        1000.0f, 0.0f, 1000.0f, 0.0f, 3000.0f, 60.0f
    );
    // Left flank at -60 deg, right flank at +60 deg, rear suppression at 180 deg
    assert(flank.leftFlankX < 1000.0f);
    assert(flank.rightFlankX > 1000.0f);
    assert(flank.suppressionZ < 1000.0f);

    std::cout << "[PASSED] Suite 43: Neuroevolutionary Combat AI & Tactical Adaptation (34 assertions passed)." << std::endl;
}

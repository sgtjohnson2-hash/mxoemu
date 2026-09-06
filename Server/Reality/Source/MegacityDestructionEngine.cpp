#include "MegacityDestructionEngine.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(MegacityDestructionEngine);

MegacityDestructionEngine::MegacityDestructionEngine()
{
    Initialize();
}

void MegacityDestructionEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    m_buildings.clear();
    m_helicopters.clear();
    m_glassParticles.clear();

    // 1. Register Iconic Megacity Skyscrapers
    RegisterSkyscraper("MetaCortex Tower", 2, DestructionVector3(39216.0f, 0.0f, -21475.0f), 45);
    RegisterSkyscraper("Club Hel Skyscraper", 3, DestructionVector3(-37444.0f, 0.0f, 23659.0f), 55);
    RegisterSkyscraper("Mobil Ave Terminal", 1, DestructionVector3(99640.0f, 0.0f, 8350.0f), 15);
    RegisterSkyscraper("Richland Federal Tower", 4, DestructionVector3(-15000.0f, 0.0f, -15000.0f), 30);

    // 2. Spawn Default Bell 212 Gunship
    SpawnHelicopter("Zion Air-1", DestructionVector3(39216.0f, 15000.0f, -21475.0f));
}

void MegacityDestructionEngine::UpdateSimulation(float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    if (deltaTimeSec <= 0.0f) return;
    m_simTimeSec += deltaTimeSec;

    // 1. Update Glass Shard Particles
    for (auto it = m_glassParticles.begin(); it != m_glassParticles.end();) {
        it->lifeRemainingSec -= deltaTimeSec;
        if (it->lifeRemainingSec <= 0.0f) {
            it = m_glassParticles.erase(it);
            continue;
        }

        // Gravity (-980 cm/s^2)
        it->velocity.y -= 980.0f * deltaTimeSec;
        it->position += it->velocity * deltaTimeSec;
        ++it;
    }

    // 2. Update Helicopters
    for (auto& kv : m_helicopters) {
        HelicopterState& chopper = kv.second;

        // Minigun barrel cooling
        if (chopper.barrelTempCelsius > 20.0f) {
            chopper.barrelTempCelsius = std::max(20.0f, chopper.barrelTempCelsius - 15.0f * deltaTimeSec);
        }

        // Collective Lift & Ground Effect
        float baseLift = chopper.collectivePitch * 1960.0f; // Counter 980 gravity + vertical climb
        if (chopper.position.y < 2000.0f) {
            // Ground effect cushioning
            float groundCushionFactor = 1.0f + (0.25f * (1.0f - (chopper.position.y / 2000.0f)));
            baseLift *= groundCushionFactor;
        }

        float verticalAccel = baseLift - 980.0f;
        chopper.velocity.y += verticalAccel * deltaTimeSec;

        // Cyclic translation
        float forwardAccel = chopper.cyclicForward * 1200.0f;
        float rightAccel = chopper.cyclicRight * 1200.0f;

        float radYaw = chopper.yawDeg * 0.0174533f;
        float cosY = std::cos(radYaw);
        float sinY = std::sin(radYaw);

        chopper.velocity.x += (forwardAccel * sinY + rightAccel * cosY) * deltaTimeSec;
        chopper.velocity.z += (forwardAccel * cosY - rightAccel * sinY) * deltaTimeSec;

        // Aerodynamic damping
        chopper.velocity = chopper.velocity * std::max(0.0f, 1.0f - (0.45f * deltaTimeSec));
        chopper.position += chopper.velocity * deltaTimeSec;

        // Fast rope rappel progress
        if (chopper.fastRopeDeployed && chopper.operativeRappelProgress < 1.0f) {
            chopper.operativeRappelProgress = std::min(1.0f, chopper.operativeRappelProgress + (0.15f * deltaTimeSec));
        }
    }
}

uint32 MegacityDestructionEngine::RegisterSkyscraper(const std::string& name, uint32 districtId, const DestructionVector3& basePos, uint32 floors)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    uint32 id = m_nextBuildingId++;
    SkyscraperBlock b;
    b.buildingId = id;
    b.buildingName = name;
    b.districtId = districtId;
    b.basePosition = basePos;
    b.floorCount = std::max(5u, floors);
    b.totalGlassPanels = b.floorCount * 40;
    b.shatteredGlassPanels = 0;
    b.overallStructuralIntegrity = 100.0f;

    // Generate 6 load-bearing pillars per floor (4 corner, 2 core)
    uint32 pId = 1;
    for (uint32 f = 1; f <= b.floorCount; ++f) {
        for (int p = 0; p < 6; ++p) {
            StructuralPillar sp;
            sp.pillarId = pId++;
            sp.floorIndex = f;
            sp.isCorePillar = (p >= 4);
            sp.maxLoadCapacityTonnes = sp.isCorePillar ? 180.0f : 120.0f;
            sp.currentLoadTonnes = 45.0f;
            sp.integrityPercent = 100.0f;
            sp.isCollapsed = false;
            b.pillars.push_back(sp);
        }
    }

    m_buildings[id] = b;
    return id;
}

bool MegacityDestructionEngine::ApplyDamageToPillar(uint32 buildingId, uint32 pillarId, float damage)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto bIt = m_buildings.find(buildingId);
    if (bIt == m_buildings.end()) return false;

    SkyscraperBlock& b = bIt->second;
    for (auto& p : b.pillars) {
        if (p.pillarId == pillarId) {
            p.integrityPercent = std::max(0.0f, p.integrityPercent - damage);
            if (p.integrityPercent <= 0.0f && !p.isCollapsed) {
                p.isCollapsed = true;
                EvaluateStressRedistribution(buildingId);
            }
            return true;
        }
    }
    return false;
}

void MegacityDestructionEngine::EvaluateStressRedistribution(uint32 buildingId)
{
    auto bIt = m_buildings.find(buildingId);
    if (bIt == m_buildings.end()) return;

    SkyscraperBlock& b = bIt->second;
    uint32 collapsedCount = 0;
    uint32 totalPillars = (uint32)b.pillars.size();

    for (auto& p : b.pillars) {
        if (p.isCollapsed) {
            collapsedCount++;
        }
    }

    float collapseRatio = totalPillars > 0 ? ((float)collapsedCount / (float)totalPillars) : 0.0f;
    b.overallStructuralIntegrity = std::max(0.0f, 100.0f * (1.0f - collapseRatio));
}

float MegacityDestructionEngine::GetBuildingIntegrity(uint32 buildingId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto bIt = m_buildings.find(buildingId);
    if (bIt == m_buildings.end()) return 0.0f;
    return bIt->second.overallStructuralIntegrity;
}

uint32 MegacityDestructionEngine::ShatterGlassFacade(uint32 buildingId, uint32 floorIndex, const DestructionVector3& impactPoint, float impactEnergy)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto bIt = m_buildings.find(buildingId);
    if (bIt == m_buildings.end()) return 0;

    SkyscraperBlock& b = bIt->second;
    uint32 panelsToShatter = (uint32)std::min(12.0f, std::max(1.0f, impactEnergy / 150.0f));
    b.shatteredGlassPanels = std::min(b.totalGlassPanels, b.shatteredGlassPanels + panelsToShatter);

    // Spawn Voronoi glass shard particles
    uint32 shardsToSpawn = panelsToShatter * 8;
    for (uint32 i = 0; i < shardsToSpawn; ++i) {
        GlassShardParticle p;
        p.position = impactPoint + DestructionVector3((float)(rand() % 50 - 25), (float)(rand() % 40 - 20), (float)(rand() % 50 - 25));
        float speed = 250.0f + (float)(rand() % 350);
        float angle = (float)(rand() % 360) * 0.0174533f;
        p.velocity = DestructionVector3(std::cos(angle) * speed, (float)(rand() % 150), std::sin(angle) * speed);
        p.lifeRemainingSec = 3.5f;
        p.sizeCm = 5.0f + (float)(rand() % 25);
        m_glassParticles.push_back(p);
    }

    return panelsToShatter;
}

size_t MegacityDestructionEngine::GetActiveGlassParticleCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    return m_glassParticles.size();
}

uint32 MegacityDestructionEngine::SpawnHelicopter(const std::string& callsign, const DestructionVector3& spawnPos)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    uint32 id = m_nextChopperId++;
    HelicopterState chopper;
    chopper.chopperId = id;
    chopper.callsign = callsign;
    chopper.position = spawnPos;
    m_helicopters[id] = chopper;
    return id;
}

bool MegacityDestructionEngine::UpdateHelicopterFlightControls(uint32 chopperId, float collective, float cyclicFwd, float cyclicRt, float pedal, float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto it = m_helicopters.find(chopperId);
    if (it == m_helicopters.end()) return false;

    HelicopterState& c = it->second;
    c.collectivePitch = std::clamp(collective, 0.0f, 1.0f);
    c.cyclicForward = std::clamp(cyclicFwd, -1.0f, 1.0f);
    c.cyclicRight = std::clamp(cyclicRt, -1.0f, 1.0f);
    c.tailRotorPedal = std::clamp(pedal, -1.0f, 1.0f);

    c.yawDeg += c.tailRotorPedal * 45.0f * deltaTimeSec;
    while (c.yawDeg >= 360.0f) c.yawDeg -= 360.0f;
    while (c.yawDeg < 0.0f) c.yawDeg += 360.0f;

    c.pitchDeg = c.cyclicForward * 20.0f;
    c.rollDeg = c.cyclicRight * 25.0f;
    return true;
}

bool MegacityDestructionEngine::FireDoorMinigun(uint32 chopperId, uint32& outRoundsFired, float deltaTimeSec)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    outRoundsFired = 0;
    auto it = m_helicopters.find(chopperId);
    if (it == m_helicopters.end()) return false;

    HelicopterState& c = it->second;
    if (c.minigunAmmo == 0) return false;

    // 4000 RPM = 66.6 rounds/sec
    uint32 rounds = (uint32)std::max(1.0f, std::round(66.6f * deltaTimeSec));
    outRoundsFired = std::min(rounds, c.minigunAmmo);
    c.minigunAmmo -= outRoundsFired;
    c.barrelTempCelsius += outRoundsFired * 0.15f;
    c.isFiringMinigun = true;
    return (outRoundsFired > 0);
}

bool MegacityDestructionEngine::DeployFastRope(uint32 chopperId)
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto it = m_helicopters.find(chopperId);
    if (it == m_helicopters.end()) return false;

    it->second.fastRopeDeployed = true;
    it->second.operativeRappelProgress = 0.0f;
    return true;
}

const HelicopterState* MegacityDestructionEngine::GetHelicopter(uint32 chopperId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto it = m_helicopters.find(chopperId);
    if (it != m_helicopters.end()) return &it->second;
    return nullptr;
}

const SkyscraperBlock* MegacityDestructionEngine::GetSkyscraper(uint32 buildingId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    auto it = m_buildings.find(buildingId);
    if (it != m_buildings.end()) return &it->second;
    return nullptr;
}

size_t MegacityDestructionEngine::GetBuildingCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    return m_buildings.size();
}

size_t MegacityDestructionEngine::GetHelicopterCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_destructionMutex);
    return m_helicopters.size();
}

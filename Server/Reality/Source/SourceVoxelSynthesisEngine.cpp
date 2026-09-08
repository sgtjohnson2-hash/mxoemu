#include "SourceVoxelSynthesisEngine.h"
#include "WorldRealizationEngine.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cassert>

createFileSingleton(SourceVoxelSynthesisEngine);

SourceVoxelSynthesisEngine::SourceVoxelSynthesisEngine()
    : m_initialized(false),
      m_nextCoverId(1),
      m_nextFieldId(1),
      m_nextWeaponId(1),
      m_invertedBullets(0),
      m_glyphShowers(0),
      m_totalVoxelsMaterialized(0),
      m_sourceEnergyReserve(100000.0f) {
}

SourceVoxelSynthesisEngine::~SourceVoxelSynthesisEngine() {
}

void SourceVoxelSynthesisEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;
    m_initialized = true;
    m_barriers.reserve(256);
    m_fields.reserve(64);
    m_weapons.reserve(128);
    std::cout << "[SourceVoxelSynthesisEngine] Initialized molecular voxel synthesis and ballistic inversion." << std::endl;
}

void SourceVoxelSynthesisEngine::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_barriers.clear();
    m_fields.clear();
    m_weapons.clear();
    m_nextCoverId = 1;
    m_nextFieldId = 1;
    m_nextWeaponId = 1;
    m_invertedBullets = 0;
    m_glyphShowers = 0;
    m_totalVoxelsMaterialized = 0;
    m_sourceEnergyReserve = 100000.0f;
}

uint32_t SourceVoxelSynthesisEngine::SynthesizeCover(float x, float y, float z, float width, float height,
                                                    VoxelCoverMaterial material, float lifetimeSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    float baseHealth = 500.0f;
    switch (material) {
        case VoxelCoverMaterial::CodeReinforcedSteel: baseHealth = 1200.0f; break;
        case VoxelCoverMaterial::QuantumCeramic:       baseHealth = 2000.0f; break;
        case VoxelCoverMaterial::GlitchAegis:          baseHealth = 3500.0f; break;
        default: break;
    }

    uint32_t id = m_nextCoverId++;
    VoxelCoverBarrier b;
    b.id = id;
    b.x = x;
    b.y = y;
    b.z = z;
    b.width = width;
    b.height = height;
    b.health = baseHealth;
    b.maxHealth = baseHealth;
    b.remainingLifetime = lifetimeSec;
    b.material = material;
    b.active = true;

    m_barriers.push_back(b);
    m_totalVoxelsMaterialized += static_cast<uint64_t>(std::max(1.0f, width * height * 100.0f));

    // Persistent 3D Physicalization Directive
    sWorldRealizationEngine.SynthesizeVoxelCover3D(x, y, z, width, height);

    return id;
}

bool SourceVoxelSynthesisEngine::DamageCover(uint32_t coverId, float damage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& b : m_barriers) {
        if (b.id == coverId && b.active) {
            b.health -= damage;
            if (b.health <= 0.0f) {
                b.health = 0.0f;
                b.active = false;
            }
            return true;
        }
    }
    return false;
}

const VoxelCoverBarrier* SourceVoxelSynthesisEngine::FindNearestCover(float x, float y, float z, float maxDist) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const VoxelCoverBarrier* best = nullptr;
    float bestDistSq = maxDist * maxDist;

    for (const auto& b : m_barriers) {
        if (!b.active) continue;
        float dx = b.x - x;
        float dy = b.y - y;
        float dz = b.z - z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq <= bestDistSq) {
            bestDistSq = distSq;
            best = &b;
        }
    }
    return best;
}

size_t SourceVoxelSynthesisEngine::GetActiveCoverCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& b : m_barriers) {
        if (b.active) count++;
    }
    return count;
}

float SourceVoxelSynthesisEngine::GetTotalCoverHealth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    float total = 0.0f;
    for (const auto& b : m_barriers) {
        if (b.active) total += b.health;
    }
    return total;
}

uint32_t SourceVoxelSynthesisEngine::CreateInversionField(float x, float y, float z, float radius, float durationSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t id = m_nextFieldId++;
    BallisticInversionField f;
    f.id = id;
    f.x = x;
    f.y = y;
    f.z = z;
    f.radius = radius;
    f.remainingDuration = durationSec;
    f.active = true;

    m_fields.push_back(f);
    return id;
}

bool SourceVoxelSynthesisEngine::ApplyBallisticInversion(float bulletX, float bulletY, float bulletZ) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& f : m_fields) {
        if (!f.active) continue;
        float dx = f.x - bulletX;
        float dy = f.y - bulletY;
        float dz = f.z - bulletZ;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq <= (f.radius * f.radius)) {
            m_invertedBullets++;
            m_glyphShowers++;
            return true;
        }
    }
    return false;
}

size_t SourceVoxelSynthesisEngine::GetActiveInversionFieldCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& f : m_fields) {
        if (f.active) count++;
    }
    return count;
}

uint32_t SourceVoxelSynthesisEngine::GetInvertedBulletCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_invertedBullets;
}

uint32_t SourceVoxelSynthesisEngine::GetTotalGlyphShowersSpawned() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_glyphShowers;
}

uint32_t SourceVoxelSynthesisEngine::MaterializeWeapon(uint32_t ownerGoId, const std::string& weaponType, float durationSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    float dmg = 50.0f;
    int ammo = 30;

    if (weaponType == "Barrett_50BMG") {
        dmg = 250.0f;
        ammo = 10;
    } else if (weaponType == "Milkor_M32_MGL") {
        dmg = 180.0f;
        ammo = 6;
    } else if (weaponType == "Katana_SourceCode") {
        dmg = 120.0f;
        ammo = 999;
    }

    uint32_t id = m_nextWeaponId++;
    MaterializedWeapon w;
    w.id = id;
    w.ownerGoId = ownerGoId;
    w.weaponType = weaponType;
    w.damageRating = dmg;
    w.ammoCapacity = ammo;
    w.remainingDuration = durationSec;
    w.active = true;

    m_weapons.push_back(w);
    m_totalVoxelsMaterialized += 500;
    return id;
}

bool SourceVoxelSynthesisEngine::DismissWeapon(uint32_t weaponId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& w : m_weapons) {
        if (w.id == weaponId && w.active) {
            w.active = false;
            return true;
        }
    }
    return false;
}

size_t SourceVoxelSynthesisEngine::GetActiveWeaponCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& w : m_weapons) {
        if (w.active) count++;
    }
    return count;
}

const MaterializedWeapon* SourceVoxelSynthesisEngine::GetMaterializedWeapon(uint32_t weaponId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& w : m_weapons) {
        if (w.id == weaponId && w.active) return &w;
    }
    return nullptr;
}

std::vector<MaterializedWeapon> SourceVoxelSynthesisEngine::GetWeaponsByOwner(uint32_t ownerGoId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MaterializedWeapon> result;
    for (const auto& w : m_weapons) {
        if (w.ownerGoId == ownerGoId && w.active) {
            result.push_back(w);
        }
    }
    return result;
}

void SourceVoxelSynthesisEngine::ReplenishSourceEnergy(float amount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sourceEnergyReserve += amount;
}

void SourceVoxelSynthesisEngine::Update(float dtSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    for (auto& b : m_barriers) {
        if (!b.active) continue;
        b.remainingLifetime -= dtSec;
        if (b.remainingLifetime <= 0.0f) {
            b.active = false;
        }
    }

    for (auto& f : m_fields) {
        if (!f.active) continue;
        f.remainingDuration -= dtSec;
        if (f.remainingDuration <= 0.0f) {
            f.active = false;
        }
    }

    for (auto& w : m_weapons) {
        if (!w.active) continue;
        w.remainingDuration -= dtSec;
        if (w.remainingDuration <= 0.0f) {
            w.active = false;
        }
    }
}

void RunSourceVoxelSynthesisTestSuite() {
    std::cout << "[Suite 54] Executing Source Voxel Synthesis & Ballistic Inversion Engine Tests..." << std::endl;
    int assertions = 0;

    sSourceVoxelSynthesisEngine.Initialize();
    sSourceVoxelSynthesisEngine.Reset();

    // 1. Initial conditions
    assert(sSourceVoxelSynthesisEngine.GetActiveCoverCount() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveInversionFieldCount() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveWeaponCount() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetInvertedBulletCount() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalGlyphShowersSpawned() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalVoxelsMaterialized() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetSourceEnergyReserve() >= 100000.0f); assertions++;

    // 2. Synthesize Cover
    uint32_t c1 = sSourceVoxelSynthesisEngine.SynthesizeCover(100.0f, 10.0f, 200.0f, 4.0f, 2.0f, VoxelCoverMaterial::DigitalConcrete, 20.0f);
    assert(c1 == 1); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveCoverCount() == 1); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalCoverHealth() == 500.0f); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalVoxelsMaterialized() > 0); assertions++;

    // 3. Different material types
    uint32_t c2 = sSourceVoxelSynthesisEngine.SynthesizeCover(150.0f, 10.0f, 250.0f, 6.0f, 3.0f, VoxelCoverMaterial::CodeReinforcedSteel, 30.0f);
    uint32_t c3 = sSourceVoxelSynthesisEngine.SynthesizeCover(200.0f, 10.0f, 300.0f, 5.0f, 2.5f, VoxelCoverMaterial::QuantumCeramic, 45.0f);
    uint32_t c4 = sSourceVoxelSynthesisEngine.SynthesizeCover(250.0f, 10.0f, 350.0f, 8.0f, 4.0f, VoxelCoverMaterial::GlitchAegis, 60.0f);
    assert(c2 == 2 && c3 == 3 && c4 == 4); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveCoverCount() == 4); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalCoverHealth() == 500.0f + 1200.0f + 2000.0f + 3500.0f); assertions++;

    // 4. Find Nearest Cover
    const VoxelCoverBarrier* nearest = sSourceVoxelSynthesisEngine.FindNearestCover(101.0f, 10.0f, 201.0f, 10.0f);
    assert(nearest != nullptr); assertions++;
    assert(nearest->id == c1); assertions++;

    const VoxelCoverBarrier* none = sSourceVoxelSynthesisEngine.FindNearestCover(9999.0f, 9999.0f, 9999.0f, 5.0f);
    assert(none == nullptr); assertions++;

    // 5. Damage Cover
    bool damaged = sSourceVoxelSynthesisEngine.DamageCover(c1, 200.0f);
    assert(damaged); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalCoverHealth() == (500.0f - 200.0f) + 1200.0f + 2000.0f + 3500.0f); assertions++;

    // Destroy cover c1
    bool destroyed = sSourceVoxelSynthesisEngine.DamageCover(c1, 400.0f);
    assert(destroyed); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveCoverCount() == 3); assertions++;

    // 6. Ballistic Inversion Fields
    uint32_t f1 = sSourceVoxelSynthesisEngine.CreateInversionField(100.0f, 10.0f, 200.0f, 10.0f, 15.0f);
    assert(f1 == 1); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveInversionFieldCount() == 1); assertions++;

    // Bullet outside field
    bool invOutside = sSourceVoxelSynthesisEngine.ApplyBallisticInversion(500.0f, 10.0f, 500.0f);
    assert(!invOutside); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetInvertedBulletCount() == 0); assertions++;

    // Bullet inside field
    bool invInside = sSourceVoxelSynthesisEngine.ApplyBallisticInversion(102.0f, 10.0f, 201.0f);
    assert(invInside); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetInvertedBulletCount() == 1); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetTotalGlyphShowersSpawned() == 1); assertions++;

    // 7. Tactical Weapon Materialization
    uint32_t w1 = sSourceVoxelSynthesisEngine.MaterializeWeapon(1001, "Barrett_50BMG", 60.0f);
    uint32_t w2 = sSourceVoxelSynthesisEngine.MaterializeWeapon(1001, "Katana_SourceCode", 45.0f);
    uint32_t w3 = sSourceVoxelSynthesisEngine.MaterializeWeapon(1002, "Milkor_M32_MGL", 30.0f);
    assert(w1 == 1 && w2 == 2 && w3 == 3); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveWeaponCount() == 3); assertions++;

    const MaterializedWeapon* weaponInfo = sSourceVoxelSynthesisEngine.GetMaterializedWeapon(w1);
    assert(weaponInfo != nullptr); assertions++;
    assert(weaponInfo->damageRating == 250.0f); assertions++;
    assert(weaponInfo->ammoCapacity == 10); assertions++;

    auto p1Weapons = sSourceVoxelSynthesisEngine.GetWeaponsByOwner(1001);
    assert(p1Weapons.size() == 2); assertions++;

    // Dismiss weapon
    bool dismissed = sSourceVoxelSynthesisEngine.DismissWeapon(w2);
    assert(dismissed); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveWeaponCount() == 2); assertions++;

    // 8. Update & Decay
    sSourceVoxelSynthesisEngine.Update(35.0f);
    // c2 had 30.0s lifetime -> expired
    // f1 had 15.0s lifetime -> expired
    // w3 had 30.0s lifetime -> expired
    assert(sSourceVoxelSynthesisEngine.GetActiveInversionFieldCount() == 0); assertions++;
    assert(sSourceVoxelSynthesisEngine.GetActiveWeaponCount() == 1); assertions++; // w1 still has 25s

    // Energy replenish
    sSourceVoxelSynthesisEngine.ReplenishSourceEnergy(5000.0f);
    assert(sSourceVoxelSynthesisEngine.GetSourceEnergyReserve() >= 105000.0f); assertions++;

    std::cout << "[Suite 54] PASSED (" << assertions << " assertions verified)" << std::endl;
}

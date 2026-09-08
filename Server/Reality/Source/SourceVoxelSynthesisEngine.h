#pragma once

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

enum class VoxelCoverMaterial {
    DigitalConcrete,
    CodeReinforcedSteel,
    QuantumCeramic,
    GlitchAegis
};

struct VoxelCoverBarrier {
    uint32_t id;
    float x;
    float y;
    float z;
    float width;
    float height;
    float health;
    float maxHealth;
    float remainingLifetime;
    VoxelCoverMaterial material;
    bool active;
};

struct BallisticInversionField {
    uint32_t id;
    float x;
    float y;
    float z;
    float radius;
    float remainingDuration;
    bool active;
};

struct MaterializedWeapon {
    uint32_t id;
    uint32_t ownerGoId;
    std::string weaponType;
    float damageRating;
    int ammoCapacity;
    float remainingDuration;
    bool active;
};

class SourceVoxelSynthesisEngine : public Singleton<SourceVoxelSynthesisEngine> {
public:
    SourceVoxelSynthesisEngine();
    ~SourceVoxelSynthesisEngine();

    void Initialize();
    void Update(float dtSec);
    void Reset();

    // Voxel Cover Synthesis
    uint32_t SynthesizeCover(float x, float y, float z, float width, float height, 
                            VoxelCoverMaterial material = VoxelCoverMaterial::DigitalConcrete, 
                            float lifetimeSec = 30.0f);
    bool DamageCover(uint32_t coverId, float damage);
    const VoxelCoverBarrier* FindNearestCover(float x, float y, float z, float maxDist) const;
    size_t GetActiveCoverCount() const;
    float GetTotalCoverHealth() const;

    // Ballistic Inversion
    uint32_t CreateInversionField(float x, float y, float z, float radius = 5.0f, float durationSec = 10.0f);
    bool ApplyBallisticInversion(float bulletX, float bulletY, float bulletZ);
    size_t GetActiveInversionFieldCount() const;
    uint32_t GetInvertedBulletCount() const;
    uint32_t GetTotalGlyphShowersSpawned() const;

    // Tactical Weapon Materialization
    uint32_t MaterializeWeapon(uint32_t ownerGoId, const std::string& weaponType, float durationSec = 60.0f);
    bool DismissWeapon(uint32_t weaponId);
    size_t GetActiveWeaponCount() const;
    const MaterializedWeapon* GetMaterializedWeapon(uint32_t weaponId) const;
    std::vector<MaterializedWeapon> GetWeaponsByOwner(uint32_t ownerGoId) const;

    // Metrics & Telemetry
    uint64_t GetTotalVoxelsMaterialized() const { return m_totalVoxelsMaterialized; }
    float GetSourceEnergyReserve() const { return m_sourceEnergyReserve; }
    void ReplenishSourceEnergy(float amount);

private:
    mutable std::mutex m_mutex;
    bool m_initialized;
    uint32_t m_nextCoverId;
    uint32_t m_nextFieldId;
    uint32_t m_nextWeaponId;
    uint32_t m_invertedBullets;
    uint32_t m_glyphShowers;
    uint64_t m_totalVoxelsMaterialized;
    float m_sourceEnergyReserve;

    std::vector<VoxelCoverBarrier> m_barriers;
    std::vector<BallisticInversionField> m_fields;
    std::vector<MaterializedWeapon> m_weapons;
};

#define sSourceVoxelSynthesisEngine SourceVoxelSynthesisEngine::getSingleton()

void RunSourceVoxelSynthesisTestSuite();

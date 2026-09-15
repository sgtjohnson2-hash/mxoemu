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

struct RealityGlitchTear {
    uint32_t id;
    uint32_t operativeGoId;
    float x;
    float y;
    float z;
    float radius;
    float glitchIntensity;
    float remainingDuration;
    uint32_t requiredFocus;
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

    // Epoch VIII: Procedural Code Voxelization & Spatial Glitch Manipulation for High-Focus Operatives
    uint32_t CreateRealityGlitchTear(uint32_t operativeGoId, float x, float y, float z, float radius = 10.0f,
                                    float intensity = 1.0f, float durationSec = 15.0f, uint32_t operativeFocusRating = 60);
    bool ManipulateVoxelGeometry(uint32_t barrierId, float deltaWidth, float deltaHeight, uint32_t operativeFocusRating);
    bool DissolveSurfaceToVoxels(float x, float y, float z, float radius, uint32_t operativeFocusRating);
    size_t GetActiveRealityTearCount() const;
    uint32_t GetTotalDissolvedSurfaces() const { return m_totalDissolvedSurfaces; }

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
    uint32_t m_nextTearId;
    uint32_t m_invertedBullets;
    uint32_t m_glyphShowers;
    uint32_t m_totalDissolvedSurfaces;
    uint64_t m_totalVoxelsMaterialized;
    float m_sourceEnergyReserve;

    std::vector<VoxelCoverBarrier> m_barriers;
    std::vector<BallisticInversionField> m_fields;
    std::vector<MaterializedWeapon> m_weapons;
    std::vector<RealityGlitchTear> m_glitchTears;
};

#define sSourceVoxelSynthesisEngine SourceVoxelSynthesisEngine::getSingleton()

void RunSourceVoxelSynthesisTestSuite();

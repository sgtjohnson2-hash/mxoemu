#ifndef MXOEMU_STATUS_EFFECT_MANAGER_H
#define MXOEMU_STATUS_EFFECT_MANAGER_H

#include "Common.h"
#include "Singleton.h"
#include <vector>
#include <mutex>
#include <shared_mutex>

class LocationVector;

enum EffectType {
    EFFECT_VIRUS_DOT,
    EFFECT_REGEN_HOT,
    EFFECT_STUN,
    EFFECT_SNARE,
    EFFECT_FACTION_MASK,
    EFFECT_LOGIC_BOMB,
    EFFECT_THE_ANOMALY,
    EFFECT_ORACLE_INTUITION,
    EFFECT_ORACLE_PREMONITION_BOOST,
    EFFECT_ORACLE_VIRAL_IMMUNITY,
    EFFECT_ORACLE_GLITCH_SIGHT,
    EFFECT_ORACLE_SERAPHIC_AEGIS,
    EFFECT_ORACLE_VISION_TRANCE
};

struct StatusEffect {
    uint32 targetGoId;
    EffectType type;
    float durationRemaining;
    float tickTimer;
    float tickInterval;
    float value; // Damage or Healing per tick
    uint32 sourceGoId;
};

class StatusEffectManager : public Singleton<StatusEffectManager>{
public:
    StatusEffectManager();
    ~StatusEffectManager();

    void Initialize();
    
    // Process all active status effects. Should be called periodically by GameServer::Loop
    void Update(float deltaTime);

    // Applies a new effect to the target
    void ApplyEffect(uint32 targetGoId, EffectType type, float duration, float tickInterval, float value, uint32 sourceGoId = 0);

    // Removes all effects of a specific type from a target (e.g. Cleanses)
    void RemoveEffectType(uint32 targetGoId, EffectType type);
    
    // Clears all effects in a radius
    void ClearEffectsInRadius(const LocationVector& loc, float radius);
    
    // Checks if target has a specific effect
    bool HasEffect(uint32 targetGoId, EffectType type) const;

private:
    // A contiguous vector of active effects for maximum Data-Oriented cache coherency
    std::vector<StatusEffect> m_effects;
    mutable std::shared_mutex m_mutex;
};

#define sStatusEffectManager Singleton<StatusEffectManager>::getSingleton()

#endif // MXOEMU_STATUS_EFFECT_MANAGER_H

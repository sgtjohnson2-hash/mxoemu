#include "StatusEffectManager.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "PlayerObject.h"
#include "Log.h"
#include "SpatialGrid.h"
#include "GameClient.h"
#include <functional>

createFileSingleton(StatusEffectManager);

StatusEffectManager::StatusEffectManager()
{
}

StatusEffectManager::~StatusEffectManager()
{
}

void StatusEffectManager::Initialize()
{
    INFO_LOG("StatusEffectManager Initialized.");
    m_effects.reserve(1000); // Pre-allocate for 1000 concurrent effects
}

void StatusEffectManager::Update(float deltaTime)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    std::vector<StatusEffect> pendingEffects;
    // Damage/heal must run AFTER m_mutex is released: takeDamage -> die -> CombatSystem takes
    // m_combatMutex, while CombatSystem::UseAbility (holding m_combatMutex) calls ApplyEffect
    // (m_mutex). Doing it under the lock was an ABBA deadlock that froze the sim thread.
    std::vector<std::function<void()>> deferred;
    auto deferDamage = [&deferred](uint32 tg, uint32 src, uint16 v) {
        deferred.push_back([tg, src, v]() {
            PlayerObject* t = sObjMgr.getGOPtrSafe(tg);
            if (t && !t->isDead()) t->takeDamage(src, v, 0x280006DF);
        });
    };

    // Linear pass over all active effects
    for (auto it = m_effects.begin(); it != m_effects.end(); )
    {
        StatusEffect& effect = *it;
        // Process Tick
        PlayerObject* target = sObjMgr.getGOPtrSafe(effect.targetGoId);
        if (!target) {
            // Target is disconnected / not available, retire orphaned effect safely
            if (std::next(it) == m_effects.end()) {
                m_effects.pop_back();
                break;
            } else {
                *it = std::move(m_effects.back());
                m_effects.pop_back();
            }
            continue;
        }
        
        // V16: Relativity Scaling
        float localDeltaTime = deltaTime;
        if (!target->isDead()) {
            localDeltaTime = deltaTime * target->GetTimeDilation();
        }
        
        effect.durationRemaining -= localDeltaTime;
        effect.tickTimer += localDeltaTime;

        if (effect.tickTimer >= effect.tickInterval)
        {
            effect.tickTimer -= effect.tickInterval;
            
            // Retrieve Target
            if (target && !target->isDead())
            {
                if (effect.type == EFFECT_VIRUS_DOT)
                {
                    deferDamage(effect.targetGoId, effect.sourceGoId, static_cast<uint16>(effect.value));
                    INFO_LOG(format("StatusEffect: VIRUS ticked for %1% damage on %2%") % effect.value % target->getHandle());
                    
                    // V18: Viral Contagion (Item 26)
                    // 5% chance per tick to spread to a nearby entity
                    if ((rand() % 100) < 5) {
                        auto nearby = sSpatialGrid.GetClientsInRadius(target->getPosition().x, target->getPosition().z);
                        for (GameClient* client : nearby) {
                            if (client->isBot() && client->GetPlayerGoId() != effect.targetGoId) {
                                StatusEffect spreadEffect;
                                spreadEffect.targetGoId = client->GetPlayerGoId();
                                spreadEffect.type = EFFECT_VIRUS_DOT;
                                spreadEffect.durationRemaining = 10.0f;
                                spreadEffect.tickTimer = 0.0f;
                                spreadEffect.tickInterval = 1.0f;
                                spreadEffect.value = effect.value * 0.5f; // Spreads at half damage
                                spreadEffect.sourceGoId = effect.sourceGoId;
                                pendingEffects.push_back(spreadEffect);
                                break; // Only spread to one target per tick
                            }
                        }
                    }
                }
                else if (effect.type == EFFECT_REGEN_HOT)
                {
                    uint32 tg = effect.targetGoId, src = effect.sourceGoId;
                    uint16 v = static_cast<uint16>(effect.value);
                    deferred.push_back([tg, src, v]() {
                        PlayerObject* t = sObjMgr.getGOPtrSafe(tg);
                        if (t && !t->isDead()) t->applyHeal(src, v, 0x01000060);
                    });
                }
                else if (effect.type == EFFECT_DRAIN_CODE)
                {
                    uint16 drained = std::min<uint16>(target->getCurrentIS(), static_cast<uint16>(effect.value));
                    target->spendIS(drained);
                    PlayerObject* srcObj = sObjMgr.getGOPtrSafe(effect.sourceGoId);
                    if (srcObj && !srcObj->isDead()) {
                        srcObj->restoreIS(drained);
                    }
                }
                else if (effect.type == EFFECT_THE_ANOMALY)
                {
                    target->setInnerStrength(9999);
                    // Emit fake flight / golden aura packet here if we had FX constants
                    if (rand() % 10 == 0) {
                        INFO_LOG(format("The Anomaly surges through %1%!") % target->getHandle());
                    }
                }
                else if (effect.type == EFFECT_ORACLE_INTUITION)
                {
                    uint16 currentIS = target->getInnerStrength();
                    uint16 maxIS = target->getMaximumInnerStrength();
                    if (currentIS < maxIS) {
                        target->setInnerStrength(std::min<uint16>(maxIS, currentIS + static_cast<uint16>(effect.value)));
                    }
                }
            }
        }

        // Remove Expired
        if (effect.durationRemaining <= 0.0f)
        {
            if (effect.type == EFFECT_LOGIC_BOMB && target) {
                // Detonate target
                deferDamage(effect.targetGoId, effect.sourceGoId, static_cast<uint16>(effect.value * 2.0f));
                // Detonate AoE to nearby entities
                auto nearby = sSpatialGrid.GetClientsInRadius(target->getPosition().x, target->getPosition().z, 1500.0f);
                for (GameClient* client : nearby) {
                    if (client->GetPlayerGoId() != effect.targetGoId) {
                        PlayerObject* po = sObjMgr.getGOPtrSafe(client->GetPlayerGoId());
                        if (po && !po->isDead()) {
                            deferDamage(po->getGoId(), effect.sourceGoId, static_cast<uint16>(effect.value));
                        }
                    }
                }
                INFO_LOG(format("Logic Bomb detonated on %1%, spreading to nearby targets.") % target->getHandle());
            }
            else if (effect.type == EFFECT_BOLSTER_HEALTH && target) {
                uint16 curMax = target->getMaximumHealth();
                uint16 val = static_cast<uint16>(effect.value);
                target->setMaximumHealth(curMax > val ? curMax - val : 100);
                if (target->getCurrentHealth() > target->getMaximumHealth()) {
                    target->setCurrentHealth(target->getMaximumHealth());
                }
                target->sendVitals(true);
            }
            else if (effect.type == EFFECT_FIREWALL && target) {
                target->setFirewall(0);
            }
            else if (effect.type == EFFECT_STEALTH && target) {
                target->setStealth(false);
                INFO_LOG(format("Stealth expired for %1%") % target->getHandle());
            }

            if (std::next(it) == m_effects.end()) {
                m_effects.pop_back();
                break;
            } else {
                *it = std::move(m_effects.back());
                m_effects.pop_back();
            }
        }
        else
        {
            ++it;
        }
    }

    // Process Scheduled Ability Cast Delays (runs deferred outside mutex)
    for (auto it = m_scheduledCasts.begin(); it != m_scheduledCasts.end(); )
    {
        it->delayRemaining -= deltaTime;
        if (it->delayRemaining <= 0.0f)
        {
            if (it->payload)
            {
                deferred.push_back(std::move(it->payload));
            }
            if (std::next(it) == m_scheduledCasts.end()) {
                m_scheduledCasts.pop_back();
                break;
            } else {
                *it = std::move(m_scheduledCasts.back());
                m_scheduledCasts.pop_back();
            }
        }
        else
        {
            ++it;
        }
    }
    
    // Add pending contagion effects (we unlock first to avoid recursive locking issues if we called ApplyEffect)
    lock.unlock();
    for (auto& fn : deferred) {
        try { fn(); } catch (...) {}
    }
    for (const StatusEffect& pe : pendingEffects) {
        ApplyEffect(pe.targetGoId, pe.type, pe.durationRemaining, pe.tickInterval, pe.value, pe.sourceGoId);
    }
}

void StatusEffectManager::ScheduleCastDelay(uint32 casterGoId, uint32 targetGoId, uint16 abilityId, float delaySec, std::function<void()> payload)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    ScheduledCast sc;
    sc.casterGoId = casterGoId;
    sc.targetGoId = targetGoId;
    sc.abilityId = abilityId;
    sc.delayRemaining = delaySec;
    sc.payload = std::move(payload);
    m_scheduledCasts.push_back(std::move(sc));
}

void StatusEffectManager::ApplyEffect(uint32 targetGoId, EffectType type, float duration, float tickInterval, float value, uint32 sourceGoId)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // Prevent duplicate effects of the same type on the same target (Fixes exponential virus loop)
    for (const auto& existing : m_effects) {
        if (existing.targetGoId == targetGoId && existing.type == type) {
            return; 
        }
    }
    
    // Oracle Viral Barrier: Inoculation blocks incoming Agent Smith virus
    PlayerObject* target = sObjMgr.getGOPtrSafe(targetGoId);
    if (type == EFFECT_VIRUS_DOT) {
        for (const auto& existing : m_effects) {
            if (existing.targetGoId == targetGoId && existing.type == EFFECT_ORACLE_VIRAL_IMMUNITY) {
                if (target) {
                    target->getClient().QueueCommand(make_shared<SystemChatMsg>("{c:FFB300}[Oracle Viral Barrier] Inoculated: Agent Smith virus packet neutralized.{/c}"));
                }
                return;
            }
        }
    }

    // Applying Viral Immunity cleanses existing active Virus DoT immediately
    if (type == EFFECT_ORACLE_VIRAL_IMMUNITY) {
        m_effects.erase(std::remove_if(m_effects.begin(), m_effects.end(), [targetGoId](const StatusEffect& se) {
            return se.targetGoId == targetGoId && se.type == EFFECT_VIRUS_DOT;
        }), m_effects.end());
        if (target) {
            target->getClient().QueueCommand(make_shared<SystemChatMsg>("{c:FFB300}[Oracle Viral Fortune] Contagion purged. Viral Barrier active.{/c}"));
        }
    }

    // V18: Firewall Resistance
    if (target && type == EFFECT_VIRUS_DOT) {
        if (target->getFirewall() > 0) {
            uint16 fw = target->getFirewall();
            uint16 toAbsorb = static_cast<uint16>(value);
            target->setFirewall(fw > toAbsorb ? fw - toAbsorb : 0);
            target->getClient().QueueCommand(make_shared<SystemChatMsg>("{c:00FFFF}[FIREWALL] Hostile virus packet absorbed by firewall.{/c}"));
            return;
        }
        if (rand() % 100 < 15) {
            target->getClient().QueueCommand(make_shared<SystemChatMsg>("{c:00FF00}Firewall resisted hostile payload.{/c}"));
            return;
        }
    }
    
    // V16: Diminishing Returns logic
    if (target) {
        float resistance = target->GetCCResistance();
        duration = duration * (1.0f - resistance);
        target->AddCCResistance(0.15f); // 15% DR per effect
    }
    
    StatusEffect newEffect;
    newEffect.targetGoId = targetGoId;
    newEffect.type = type;
    newEffect.durationRemaining = duration;
    newEffect.tickTimer = 0.0f;
    newEffect.tickInterval = tickInterval;
    newEffect.value = value;
    newEffect.sourceGoId = sourceGoId;
    
    m_effects.push_back(newEffect);

    if (type == EFFECT_BOLSTER_HEALTH && target) {
        target->setMaximumHealth(target->getMaximumHealth() + static_cast<uint16>(value));
        target->applyHeal(sourceGoId, static_cast<uint16>(value), 0x01000060);
    } else if (type == EFFECT_FIREWALL && target) {
        target->setFirewall(static_cast<uint16>(value));
    } else if (type == EFFECT_STEALTH && target) {
        target->setStealth(true);
    }
    INFO_LOG(format("Applied Status Effect %1% to %2%") % static_cast<int>(type) % targetGoId);
}

void StatusEffectManager::RemoveEffectType(uint32 targetGoId, EffectType type)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    for (auto it = m_effects.begin(); it != m_effects.end(); )
    {
        if (it->targetGoId == targetGoId && it->type == type)
        {
            *it = m_effects.back();
            m_effects.pop_back();
        }
        else
        {
            ++it;
        }
    }
}
bool StatusEffectManager::HasEffect(uint32 targetGoId, EffectType type) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    for (const auto& effect : m_effects)
    {
        if (effect.targetGoId == targetGoId && effect.type == type)
        {
            return true;
        }
    }
    return false;
}

void StatusEffectManager::ClearEffectsInRadius(const LocationVector& loc, float radius)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    int clearedCount = 0;
    
    for (auto& effect : m_effects)
    {
        PlayerObject* target = sObjMgr.getGOPtrSafe(effect.targetGoId);
        if (target) {
            float dx = target->getPosition().x - loc.x;
            float dz = target->getPosition().z - loc.z;
            if ((dx*dx + dz*dz) <= (radius*radius)) {
                effect.durationRemaining = 0.0f; // Force expire on next update
                clearedCount++;
            }
        }
    }
    
    if (clearedCount > 0) {
        INFO_LOG(format("EMP Blast cleared %1% status effects in radius %2%") % clearedCount % radius);
    }
}

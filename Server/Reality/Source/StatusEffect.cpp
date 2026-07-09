// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// ***************************************************************************

#include "StatusEffect.h"
#include "IGO.h"
#include "Timer.h"

StatusEffect::StatusEffect(uint32 id, EffectType type, float durationSec, float tickRateSec, int32 magnitude)
    : m_effectId(id), m_type(type), m_magnitude(magnitude), m_tickRate(tickRateSec)
{
    float now = getFloatTime();
    m_expirationTime = (durationSec > 0.0f) ? (now + durationSec) : 0.0f;
    m_nextTickTime = (tickRateSec > 0.0f) ? (now + tickRateSec) : 0.0f;
}

StatusEffect::~StatusEffect()
{
}

bool StatusEffect::isExpired() const
{
    if (m_expirationTime == 0.0f) 
        return false; // Infinite duration

    return getFloatTime() >= m_expirationTime;
}

bool StatusEffect::shouldTick()
{
    if (m_tickRate <= 0.0f)
        return false;

    float now = getFloatTime();
    if (now >= m_nextTickTime)
    {
        m_nextTickTime = now + m_tickRate;
        return true;
    }
    return false;
}

void StatusEffect::applyTick(IGO* target)
{
    if (!target || target->isDead())
        return;

    if (m_type == EFFECT_DOT)
    {
        // For DoTs, bypass normal mitigation for now or apply specific DoT mitigation
        target->takeDamage(0, m_magnitude, 0); // 0 = environment/effect attacker
    }
    else if (m_type == EFFECT_HOT)
    {
        uint16 current = target->getCurrentHealth();
        uint16 maximum = target->getMaximumHealth();
        if (current + m_magnitude > maximum)
            target->setCurrentHealth(maximum);
        else
            target->setCurrentHealth(current + m_magnitude);
    }
    else if (m_type == EFFECT_VIRUS)
    {
        // Drain Inner Strength
        uint16 currentIS = target->getInnerStrength();
        if (currentIS >= m_magnitude)
            target->setInnerStrength(currentIS - m_magnitude);
        else
            target->setInnerStrength(0);
    }
    else if (m_type == EFFECT_LOGIC_BOMB)
    {
        // Stun logic (prevent tactical changes)
        target->setTactic(255); // Invalid tactic = stunned
    }
}

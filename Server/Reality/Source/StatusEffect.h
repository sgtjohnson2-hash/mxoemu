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

#ifndef MXOEMU_STATUSEFFECT_H
#define MXOEMU_STATUSEFFECT_H

#include "Common.h"

enum EffectType
{
    EFFECT_BUFF,
    EFFECT_DEBUFF,
    EFFECT_DOT, // Damage over time
    EFFECT_HOT, // Healing over time
    EFFECT_VIRUS, // Hacker specific IS drain
    EFFECT_LOGIC_BOMB // Hacker specific AI stun
};

class StatusEffect
{
public:
    StatusEffect(uint32 id, EffectType type, float durationSec, float tickRateSec, int32 magnitude);
    ~StatusEffect();

    uint32 getEffectId() const { return m_effectId; }
    EffectType getType() const { return m_type; }
    int32 getMagnitude() const { return m_magnitude; }

    bool isExpired() const;
    bool shouldTick();
    void applyTick(class IGO* target);

private:
    uint32 m_effectId;
    EffectType m_type;
    int32 m_magnitude;

    float m_expirationTime;
    float m_tickRate;
    float m_nextTickTime;
};

#endif // MXOEMU_STATUSEFFECT_H

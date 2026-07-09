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

#include "IGO.h"
#include "Log.h"
#include "Timer.h"

IGO::IGO()
{
    m_goId = 0;
    m_healthC = 100;
    m_healthM = 100;

    m_targetGoId = 0;
    m_ilPartner = 0;
    m_tactic = 0; // TACTIC_NORMAL
    m_inCombat = false;
    m_isDead = false;
    m_hitCounter = 0;
    m_spawnCounter = 0x30;
    m_lastRegenTime = getFloatTime();
}

IGO::~IGO()
{
}

void IGO::Update()
{
    // Process Status Effects
    for (auto it = m_statusEffects.begin(); it != m_statusEffects.end(); )
    {
        if ((*it)->isExpired())
        {
            it = m_statusEffects.erase(it);
        }
        else
        {
            if ((*it)->shouldTick())
            {
                (*it)->applyTick(this);
            }
            ++it;
        }
    }
}

void IGO::addStatusEffect(std::shared_ptr<StatusEffect> effect)
{
    if (effect)
        m_statusEffects.push_back(effect);
}

void IGO::clearStatusEffects()
{
    m_statusEffects.clear();
}

void IGO::enterInterlock(uint32 partnerGoId)
{
    m_inCombat = true;
    m_targetGoId = partnerGoId;
    setCombatStance(true);
}

void IGO::leaveInterlock()
{
    m_inCombat = false;
    m_targetGoId = 0;
    setCombatStance(false);
}

void IGO::takeDamage(uint32 attackerGoId, uint16 damage, uint32 fxId)
{
    if (m_isDead)
        return;

    // Apply armor/mitigation formulas here
    // For now, straight damage
    uint16 actualDamage = damage; // TODO: Implement stats-based mitigation

    if (m_healthC <= actualDamage)
    {
        m_healthC = 0;
        die(attackerGoId);
    }
    else
    {
        m_healthC -= actualDamage;
        m_hitCounter++;
    }
}

void IGO::die(uint32 killerGoId)
{
    if (m_isDead)
        return;

    m_isDead = true;
    m_inCombat = false;
    m_targetGoId = 0;
    
    clearStatusEffects();
    
    DEBUG_LOG(format("IGO %1% died. Killer: %2%") % m_goId % killerGoId);
}

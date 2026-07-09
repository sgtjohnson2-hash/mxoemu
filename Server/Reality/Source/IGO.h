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

#ifndef MXOEMU_IGO_H
#define MXOEMU_IGO_H

#include "Common.h"
#include "LocationVector.h"
#include "StatusEffect.h"
#include <vector>
#include <memory>

// Interactive Game Object base class
class IGO
{
public:
    IGO();
    virtual ~IGO();

    virtual void Update();

    uint32 getGoId() const { return m_goId; }
    void setGoId(uint32 goId) { m_goId = goId; }

    LocationVector getPosition() const { return m_pos; }
    void setPosition(const LocationVector& pos) { m_pos = pos; }

    // Combat interface
    bool isDead() const { return m_isDead; }
    void setDead(bool dead) { m_isDead = dead; }
    
    bool isInCombat() const { return m_inCombat; }
    void setInCombat(bool inCombat) { m_inCombat = inCombat; }
    
    uint8 getTactic() const { return m_tactic; }
    void setTactic(uint8 tactic) { m_tactic = tactic; }
    
    uint32 getTargetGoId() const { return m_targetGoId; }
    void setTargetGoId(uint32 target) { m_targetGoId = target; }
    
    uint32 getInterlockPartner() const { return m_ilPartner; }
    void setInterlockPartner(uint32 partner) { m_ilPartner = partner; }
    
    uint8 nextSpawnCounter() { return m_spawnCounter++; }

    virtual void takeDamage(uint32 attackerGoId, uint16 damage, uint32 fxId);
    virtual void die(uint32 killerGoId);
    
    virtual uint16 getCurrentHealth() const { return m_healthC; }
    virtual void setCurrentHealth(uint16 h) { m_healthC = h; }
    

    virtual uint16 getMaximumHealth() const { return m_healthM; }
    virtual void setMaximumHealth(uint16 m) { m_healthM = m; }

    virtual uint16 getInnerStrength() const { return 0; }
    virtual void setInnerStrength(uint16 is) { }

    void addStatusEffect(std::shared_ptr<StatusEffect> effect);
    void clearStatusEffects();

    virtual void setCombatStance(bool inCombatStance) {}
    virtual void enterInterlock(uint32 partnerGoId);
    virtual void leaveInterlock();

protected:
    uint32 m_goId;
    LocationVector m_pos;
    
    uint16 m_healthC;
    uint16 m_healthM;

    // Combat state
    uint32 m_targetGoId;    // current selected target (0 = none)
    uint32 m_ilPartner;     // interlock partner goId (0 = not interlocked)
    uint8 m_tactic;         // current combat tactic
    bool m_inCombat;
    bool m_isDead;
    uint8 m_hitCounter;     // increments per hit taken
    uint8 m_spawnCounter;   // per-client dynamic GO spawn id counter
    float m_lastRegenTime;

    std::vector<std::shared_ptr<StatusEffect>> m_statusEffects;
};

#endif // MXOEMU_IGO_H

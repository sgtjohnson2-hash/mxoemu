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
#include <vector>
#include <memory>
#include <shared_mutex>
#include <mutex>

// Interactive Game Object base class
class IGO
{
public:
    IGO();
    virtual ~IGO();

    virtual void Update();

    uint32 getGoId() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_goId; }
    void setGoId(uint32 goId) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_goId = goId; }

    LocationVector getPosition() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_pos; }
    virtual void setPosition(const LocationVector& pos) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_pos = pos; }

    // Combat interface
    bool isDead() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_isDead; }
    void setDead(bool dead) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_isDead = dead; }
    
    bool isInCombat() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_inCombat; }
    void setInCombat(bool inCombat) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_inCombat = inCombat; }
    
    uint8 getTactic() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_tactic; }
    void setTactic(uint8 tactic) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_tactic = tactic; }
    
    uint32 getTargetGoId() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_targetGoId; }
    void setTargetGoId(uint32 target) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_targetGoId = target; }
    
    uint32 getInterlockPartner() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_ilPartner; }
    void setInterlockPartner(uint32 partner) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_ilPartner = partner; }
    
    uint8 nextSpawnCounter() { std::unique_lock<std::shared_mutex> lock(m_igoMutex); return m_spawnCounter++; }

    virtual void takeDamage(uint32 attackerGoId, uint16 damage, uint32 fxId);
    virtual void die(uint32 killerGoId);
    
    virtual uint8 getLevel() const { return 1; }
    
    virtual uint16 getCurrentHealth() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_healthC; }
    virtual void setCurrentHealth(uint16 h) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_healthC = h; }
    
    virtual uint16 getMaximumHealth() const { std::shared_lock<std::shared_mutex> lock(m_igoMutex); return m_healthM; }
    virtual void setMaximumHealth(uint16 m) { std::unique_lock<std::shared_mutex> lock(m_igoMutex); m_healthM = m; }

    virtual uint16 getInnerStrength() const { return 0; }
    virtual void setInnerStrength(uint16 is) { }

    virtual void setCombatStance(bool inCombatStance) {}
    virtual void enterInterlock(uint32 partnerGoId);
    virtual void leaveInterlock();

protected:
    mutable std::shared_mutex m_igoMutex;
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
};

#endif // MXOEMU_IGO_H

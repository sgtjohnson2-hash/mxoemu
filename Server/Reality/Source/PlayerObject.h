// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
// Copyright (C) 2006-2010 Rajko Stojadinovic
// http://mxoemu.info
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ---------------------------------------------------------------------------
//
// ***************************************************************************

#ifndef MXOEMU_PLAYEROBJECT_H
#define MXOEMU_PLAYEROBJECT_H

#include "LocationVector.h"
#include "MessageTypes.h"
#include "CombatAnimationMatrix.h"
#include "IGO.h"
#include <mutex>
#include <atomic>
#include <unordered_set>

struct HardlineNode
{
	uint32 id = 0;
	uint32 districtId = 0;
	uint32 hardlineId = 0;
	std::string name;
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	double rot = 0.0;
	int factionTag = 0;
};

class PlayerObject : public IGO
{
public:
	class CharacterNotFound {};

	PlayerObject(class GameClient &parent,uint64 charUID, bool isBot = false);
	~PlayerObject();

	void InitializeWorld();
	void SpawnSelf();
	void PopulateWorld();
	void UpdateAoIStreaming();
	void setPosition(const LocationVector& pos) override;

	void initGoId(uint32 theGoId);
	void HandleStateUpdate(ByteBuffer &srcData);
	void HandleCommand(ByteBuffer &srcCmd);

	string getHandle() const {return m_handle;}
	string getFirstName() const {return m_firstName;}
	string getLastName() const {return m_lastName;}
	string getBackground() const {return m_background;}
	bool setBackground(string newBackground);

	uint64 getCharId() const {return m_characterUID;}
	uint64 getCharUID() const {return m_characterUID;}
	uint64 getExperience() const {return m_exp;}
	uint64 getInformation() const {return m_cash;}
	
	uint8 getDistrict() const {return m_district;}
	void setDistrict(uint8 newDistrict) {m_district = newDistrict;}
	uint8 getRsiData(byte* outputBuf,size_t maxBufLen) const;
	uint16 getCurrentHealth() const {return m_healthC;}
	uint16 getMaximumHealth() const {return m_healthM;}
	uint16 getCurrentIS() const {return m_innerStrC;}
	uint16 getCurrentInnerStrength() const {return m_innerStrC;}
	uint16 getMaximumIS() const {return m_innerStrM;}
	uint16 getMaximumInnerStrength() const {return m_innerStrM;}
	uint32 getProfession() const {return m_prof;}
	virtual uint8 getLevel() const {return m_lvl;}
	uint8 getAlignment() const {return m_alignment;}
	bool getPvpFlag() const {return m_pvpflag;}

	uint8 getCurrentAnimation() const {return m_currAnimation;}
	void setCurrentAnimation(uint8 anim) { m_currAnimation = anim; }
	uint8 getCurrentMood() const {return m_currMood;}
	uint8 nextStateCounter() { uint8 c = ++m_emoteCounter; if (c == 0) c = ++m_emoteCounter; return c; }
	uint8 getHitCounter() const { return m_emoteCounter; }
	uint8 nextHitCounter() { uint8 c = ++m_emoteCounter; if (c == 0) c = ++m_emoteCounter; return c; }
	void setCurrentMood(uint8 mood) { m_currMood = mood; }

	class GameClient& getClient() { return m_parent; }
	vector<msgBaseClassPtr> getCurrentStatePackets();

	void Update();

	void PerformRebirth(void);
	bool giveItem(unsigned int templateId);
	bool addItemByTemplateId(unsigned int templateId);
	bool isDualWielding(void) const;
	unsigned short getEvasion(void) const;
	unsigned short getPerception(void) const;
	void ApplyTimeDilation(float amount, unsigned int durationMs);
	void SendWaypoint(float x, float y, float z, const std::string& name);

	std::vector<std::shared_ptr<class Item>> getEquippedWeapons();
	void degradeEquippedWeapon(uint16 degradationAmount);

	uint64 getInfo() const { return getInformation(); }
	void addInfo(uint64 amount);
	void removeInfo(uint64 amount);
	void addExp(uint64 amount);
	std::shared_ptr<class InventorySystem> getInventory();
	
	uint32 getLootTableId() const { return m_lootTableId; }
	void setLootTableId(uint32 id) { m_lootTableId = id; }
	
	virtual void takeDamage(uint32 attackerGoId, uint16 damage, uint32 fxId = 0);
	virtual void applyHeal(uint32 healerGoId, uint16 amount, uint32 fxId = 0x01000060);
	virtual void revive(uint32 reviverGoId, float healthPct = 0.5f);
	void setFirewall(uint16 amount) { m_firewallPoints = amount; }
	uint16 getFirewall() const { return m_firewallPoints; }
	void setStealth(bool stealth) { m_isStealthed = stealth; }
	bool isStealthed() const { return m_isStealthed; }
	void killPlayer(uint32 killerGoId = 0, uint32 fxId = 0x280001C2);
	void sayChat(const std::string& msg);
	void Emote(uint32 emoteId);
	
	void awardCombatExperience(uint32 exp);
    float GetTimeDilation() const { return m_timeDilation; }
    void enterInterlock(uint32 targetGoId);
    void setCombatStance(bool stance);
    void leaveInterlock();
    
    struct CombatMemory {
        std::map<uint16, uint32> attackTypeCount;
        void recordAttack(uint16 moveId) {
            attackTypeCount[moveId]++;
        }
        uint32 getSpamCount(uint16 moveId) const {
            auto it = attackTypeCount.find(moveId);
            return (it != attackTypeCount.end()) ? it->second : 0;
        }
        float getMitigationModifier(uint16 moveId) const {
            auto it = attackTypeCount.find(moveId);
            if (it != attackTypeCount.end() && it->second > 3) {
                // Roadmap Phase 48 adaptive learning mitigation: 0.5 * (1.0 - 0.15 * (SpamCount - 3))
                float mitigation = 0.5f * (1.0f - 0.15f * float(it->second - 3));
                return std::clamp(mitigation, 0.15f, 0.50f);
            }
            return 1.0f;
        }
    };
    CombatMemory m_combatMemory;
    void recordIncomingAttack(uint16 moveId) { m_combatMemory.recordAttack(moveId); }
    bool isStunned() const; //ms-expiry check, in PlayerObjectCombat.cpp
    
    // Organizations and Factions
    int getFaction() const {
        if (m_factionName == "Civilian") return FACTION_NONE;
        if (m_factionName == "Zion") return FACTION_ZION;
        if (m_factionName == "Machines") return FACTION_MACHINES;
        if (m_factionName == "Merovingian") return FACTION_MEROVINGIAN;
        if (m_factionName == "Exile") return FACTION_EXILE;
        if (m_alignment == 1) return FACTION_MACHINES;
        if (m_alignment == 2) return FACTION_MEROVINGIAN;
        if (m_alignment == 0 && m_factionName.empty()) return FACTION_ZION;
        return FACTION_NONE;
    }
    void setFaction(int f) {
        if (f == FACTION_ZION) { m_factionName = "Zion"; m_alignment = 0; }
        else if (f == FACTION_MACHINES) { m_factionName = "Machines"; m_alignment = 1; }
        else if (f == FACTION_MEROVINGIAN) { m_factionName = "Merovingian"; m_alignment = 2; }
        else if (f == FACTION_EXILE) { m_factionName = "Exile"; }
        else { m_factionName = "Civilian"; }
    }
    void setFactionName(const std::string& name) { m_factionName = name; }
    std::string getFactionName() const { return m_factionName; }
    void setCrewName(const std::string& name) { m_crewName = name; }
    std::string getCrewName() const { return m_crewName; }

    // Faction reputation (used by MissionSystem for gated/rewarded missions)
    int getFactionReputation() const { return m_factionReputation; }
    void addFactionReputation(int delta) { m_factionReputation += delta; }
    void setFactionReputation(int v) { m_factionReputation = v; }

    // Organization membership (crew/org id)
    uint32 getOrgId() const { return m_orgId; }
    void setOrgId(uint32 id) { m_orgId = id; }

    // Director-mode possession: goId of the bot this player is puppeteering
    uint32 getPossessingBotId() const { return m_possessingBotId; }
    void setPossessingBotId(uint32 id) { m_possessingBotId = id; }

    bool hasBounty() const { return m_hasBounty; }
    void setHasBounty(bool v) { m_hasBounty = v; }

    // Crowd-control resistance (diminishing returns; grows per applied CC effect)
    float GetCCResistance() const { return m_ccResistance; }
    void AddCCResistance(float amt) { m_ccResistance += amt; }
    void SetCCResistance(float v) { m_ccResistance = v; }

    void applyStun(uint32 durationMs); //in PlayerObjectCombat.cpp
    
    FightingStyle getFightingStyle() const { return m_fightingStyle; }
    void setFightingStyle(FightingStyle style) { m_fightingStyle = style; }
    
    std::shared_ptr<class AbilitySystem> getAbilitySystem() { return m_abilitySystem; }
    // Bots never go through the human spawn path that creates the AbilitySystem; tester bots
    // need one to load/use abilities through the real RPCs. Memory-only, no DB.
    void ensureAbilitySystem();
    
    void setCurrentHealth(uint16 hp) { m_healthC = hp; }
    void setMaximumHealth(uint16 hp) { m_healthM = hp; }
    // Outgoing damage multiplier (1.0 = unscaled). Used to scale dojo/sparring bots up
    // toward player stat levels without changing player stats.
    float getDamageScale() const { return m_damageScale; }
    void setDamageScale(float s) { m_damageScale = s; }
    void setInnerStrength(uint16 cur, uint16 max) { m_innerStrC = cur; m_innerStrM = max; }
    void setCurrentIS(uint16 cur) { m_innerStrC = cur; }
    void setLevel(uint8 lvl) { m_lvl = lvl; }
    void setHandle(const std::string& handle) { m_handle = handle; }
    void setRsiHex(const std::string& hexStr);
    std::string getRsiHex() const;

    // Evade Shield & Bullet Dodge (retail PlayerAttrSelf/Other EvadeShieldHealth)
    uint8 getEvadeShield() const { return m_evadeShield; }
    void setEvadeShield(uint8 shield);
    void consumeEvadeShield(uint8 amount);
    void regenerateEvadeShield(uint32 deltaMs);
    void regenerateVitals();          // out-of-combat Health / Inner Strength recovery
    uint32 getLastBallisticFireTime() const { return m_lastBallisticFireTime; }
    void setLastBallisticFireTime(uint32 t) { m_lastBallisticFireTime = t; }

    // Equipped Weapon 3D attachment (PlayerAttrSelf/Other EquippedItemID)
    uint32 getEquippedWeaponId() const { return m_equippedWeaponId; }
    void setEquippedWeaponId(uint32 id);
    void sendEquippedWeaponUpdate();
    
    // Combat
    
    
    
    virtual void die(uint32 killerGoId = 0);
    void saveDataToDB();
    uint64 getCharacterUID() const { return m_characterUID; }
    static void LoadHardlines();
    static void LoadHardlinesFromCSV(const std::string& filePath);
    static const HardlineNode* GetHardline(uint32 districtId, uint32 hardlineId);
    static const HardlineNode* GetNearestHardline(uint32 districtId, double x, double z);
    static const std::map<std::pair<uint32, uint32>, HardlineNode>& GetHardlineDirectory() { return s_hardlineDirectory; }
    static size_t GetTotalHardlines() { return s_hardlineDirectory.size(); }

    // LootManager
    void addInformation(uint32 amount) { m_cash += amount; }
    void saveCashToDB();

	typedef enum
	{
		EVENT_JACKOUT,
        EVENT_HACKING,
        EVENT_RESPAWN
	} eventType;

private: 
	//RPC handler type
	typedef void (PlayerObject::*RPCHandler)( ByteBuffer &srcCmd );

	//RPC handlers
	void RPC_NullHandle(ByteBuffer &srcCmd);
	void RPC_HandleReadyForSpawn(ByteBuffer &srcCmd);
	void RPC_HandleChat( ByteBuffer &srcCmd );
	void RPC_HandleWhisper( ByteBuffer &srcCmd );
	void RPC_HandleStopAnimation( ByteBuffer &srcCmd );
	void RPC_HandleStartAnimtion( ByteBuffer &srcCmd );
	void RPC_HandleChangeMood( ByteBuffer &srcCmd );
	void RPC_HandlePerformEmote( ByteBuffer &srcCmd );
	void RPC_HandleDynamicObjInteraction( ByteBuffer &srcCmd );
	void RPC_HandleStaticObjInteraction( ByteBuffer &srcCmd );
	void RPC_HandleJump( ByteBuffer &srcCmd );
	void RPC_HandleRegionLoadedNotification( ByteBuffer &srcCmd );
	void RPC_HandleReadyForWorldChange( ByteBuffer &srcCmd );
	void RPC_HandleWho( ByteBuffer &srcCmd );
	void RPC_HandleWhereAmI( ByteBuffer &srcCmd );
	void RPC_HandleGetPlayerDetails( ByteBuffer &srcCmd );
	void RPC_HandleGetBackground( ByteBuffer &srcCmd );
	void RPC_HandleSetBackground( ByteBuffer &srcCmd );
	void RPC_HandleHardlineTeleport( ByteBuffer &srcCmd );
	void RPC_HandleObjectSelected( ByteBuffer &srcCmd );
	void RPC_HandleJackoutRequest( ByteBuffer &srcCmd );
	void RPC_HandleJackoutFinished( ByteBuffer &srcCmd );
	void RPC_HandleVendorBuy( ByteBuffer &srcCmd );
	void RPC_HandleVendorSell( ByteBuffer &srcCmd );
	void RPC_HandleCraftRequest( ByteBuffer &srcCmd );
	void RPC_HandleFactionInfo( ByteBuffer &srcCmd );
	void RPC_HandleMissionInvite( ByteBuffer &srcCmd );
	void RPC_HandlePartyLeave( ByteBuffer &srcCmd );
	void RPC_HandleMemoryChangeTactic( ByteBuffer &srcCmd );
	void RPC_HandleUpgradeAbility( ByteBuffer &srcCmd );
	void RPC_HandleMissionAbort( ByteBuffer &srcCmd );
	void RPC_HandleMissionAccept( ByteBuffer &srcCmd );
	void RPC_HandleMissionInfo( ByteBuffer &srcCmd );
	void RPC_HandleMissionRequest( ByteBuffer &srcCmd );
	void RPC_HandleCallContact( ByteBuffer &srcCmd );
	void RPC_HandleItemMoveSlot( ByteBuffer &srcCmd );
	void RPC_HandleItemUnmountRSI( ByteBuffer &srcCmd );
	void RPC_HandleItemMountRSI( ByteBuffer &srcCmd );
	void RPC_HandleMarketListItems(ByteBuffer &srcCmd);
	void RPC_HandleMarketOpen(ByteBuffer &srcCmd);
	void RPC_HandleCloseCombatRequest( ByteBuffer &srcCmd );
	void RPC_HandleRangeCombatRequest( ByteBuffer &srcCmd );
	void RPC_HandleChangeTactic( ByteBuffer &srcCmd );
	void RPC_HandleLeaveCombat( ByteBuffer &srcCmd );
	void RPC_HandleDuelRequest( ByteBuffer &srcCmd );
	void RPC_HandleAbilityUse( ByteBuffer &srcCmd );
	void RPC_HandleAbilityLoad( ByteBuffer &srcCmd );
	void RPC_HandleAbilityHotbarSync( ByteBuffer &srcCmd );
	void RPC_HandleStatusQuery( ByteBuffer &srcCmd );
	void RPC_HandleInteractionTrigger( ByteBuffer &srcCmd );
	void RPC_HandleClientAck( ByteBuffer &srcCmd );
	void RPC_HandleCameraPitch( ByteBuffer &srcCmd );
	void RPC_HandleCameraYaw( ByteBuffer &srcCmd );

public:
	// StatusEffectManager
	void sendHealthUpdate(bool isHeal, int amount) { sendHealthUpdate(); }
    void sendHealthUpdate();
	void setInnerStrength(float str) { m_innerStrC = (str < 0) ? 0 : ((str > m_innerStrM) ? m_innerStrM : uint16(str)); }

    bool consumeInformation(uint32 amount) { if (m_cash < amount) return false; m_cash -= amount; return true; }
    LocationVector getSavedPos() const { return m_savedPos; }

	// Possession and State
    uint32 m_pkKills = 0;
    
    uint32 m_stunExpiresMS = 0;
    uint32 m_deathDelayMS = 0;
    uint32 m_deathDelayKillerId = 0;
    uint32 m_lastDojoBotGoId = 0;
    
    std::string m_crewName;
    std::string m_factionName;
    int m_factionReputation = 0;
    uint32 m_orgId = 0;
    uint32 m_possessingBotId = 0;
    bool m_hasBounty = false;
    float m_ccResistance = 0.0f;
    float m_damageScale = 1.0f;
    std::shared_ptr<class AbilitySystem> m_abilitySystem;
    static std::map<uint32, std::vector<LocationVector>> s_hardlineCache;
    static std::map<std::pair<uint32, uint32>, HardlineNode> s_hardlineDirectory;
    uint32 m_lootTableId = 0;
    void respawn();

	//RPC Handler maps
	map<uint8,RPCHandler> m_RPCbyte;
	map<uint16,RPCHandler> m_RPCshort;
private:
	void loadFromDB(bool updatePos=false);
	void checkAndStore();
	// void saveDataToDB(); // Moved to public
	void setOnlineStatus( bool isOnline );

public:
    bool spendIS( uint16 amount );
    void restoreIS( uint16 amount );
    void sendVitals( bool includeMax = false, bool includeDead = false );

public:
	typedef boost::function < void (void) > eventFunc;

	void addEvent(eventType type, eventFunc func, float activationTime);

	bool isAdmin() const { return m_isAdmin; }
	void setAdmin(bool admin) { m_isAdmin = admin; }

private:
	size_t cancelEvents(eventType type);

	struct eventStruct
	{
		eventStruct(eventType _type, eventFunc _func, float _fireTime) : type(_type), func(_func), fireTime(_fireTime) {}
		eventType type;
		eventFunc func;
		float fireTime;
	};

	list<eventStruct> m_events;
	mutable std::mutex m_eventMutex;

	void jackoutEvent();

	void ParseAdminCommand(string theCmd);
	void ParsePlayerCommand(string theCmd);
	void GoAhead(double distanceToGo);
	void UpdateAppearance();
	class GameClient &m_parent;
	
	//Player info
	uint64 m_characterUID;
	string m_handle;
	string m_firstName;
	string m_lastName;
	string m_background;

	
	uint64 m_exp,m_cash;
	uint8 m_district;
	LocationVector m_savedPos;
	shared_ptr<class RsiData> m_rsi;
	uint16 m_innerStrC,m_innerStrM;
	uint32 m_prof;
	uint8 m_lvl,m_alignment;
	bool m_pvpflag;
	uint32 testCount;

	bool m_spawnedInWorld;
	queue<msgBaseClassPtr> m_sendAfterSpawn;
	bool m_worldPopulated;

	uint32 m_lastStore;
	uint32 m_storeCntr;

	uint8 m_currAnimation;
	uint8 m_currMood;

	std::atomic<uint8> m_emoteCounter; //shared 01 28 [counter] state counter (emotes + animations)

	bool m_isAdmin;

	std::shared_ptr<class InventorySystem> m_inventorySystem;
    
    float m_timeDilation = 1.0f;
    uint64 m_timeDilationExpires = 0;

    FightingStyle m_fightingStyle = FightingStyle::None;
    uint16 m_firewallPoints = 0;
    bool m_isStealthed = false;
    float m_cameraPitch = 0.0f;
    float m_cameraYaw = 0.0f;

    uint8 m_evadeShield = 100;
    uint32 m_lastBallisticFireTime = 0;
    uint32 m_lastDamageTakenMs = 0;   // out-of-combat regen waits for this to age
    uint32 m_lastRegenTickMs = 0;
    uint32 m_equippedWeaponId = 0;

    std::unordered_set<uint32> m_knownEntities;
    mutable std::mutex m_knownMutex;
public:
    // Area-of-interest bookkeeping shared with GameClient::QueueState: object updates are
    // only sent for objects this client has been told to spawn.
    void noteEntitySpawned(uint32 goId) { std::lock_guard<std::mutex> l(m_knownMutex); m_knownEntities.insert(goId); }
    void noteEntityDeleted(uint32 goId) { std::lock_guard<std::mutex> l(m_knownMutex); m_knownEntities.erase(goId); }
    bool knowsEntity(uint32 goId) const { std::lock_guard<std::mutex> l(m_knownMutex); return m_knownEntities.count(goId) > 0; }
    void ensureEntityKnown(PlayerObject* other);
    uint32 m_lastAoIUpdateMs = 0;
    float getCameraPitch() const { return m_cameraPitch; }
    float getCameraYaw() const { return m_cameraYaw; }
};

#endif
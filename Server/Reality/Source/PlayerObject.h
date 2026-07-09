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

#include "IGO.h"

class PlayerObject : public IGO
{
public:

	class CharacterNotFound {};

	PlayerObject( GameClient& owner, uint64 charUID );
	PlayerObject( GameClient& owner, uint64 charUID, bool isBot );
	~PlayerObject();

	void setLocation( LocationVector& loc );

	string getCrewName() const { return m_crewName; }
	void setCrewName(const string& name) { m_crewName = name; }
	
	string getFactionName() const { return m_factionName; }
	void setFactionName(const string& name) { m_factionName = name; }

    bool isStealthed() const { return m_isStealthed; }
    void setStealthed(bool val) { m_isStealthed = val; }

    uint64 getInfo() const { return m_info; }
    void addInfo(uint64 amt) { m_info += amt; }
    void removeInfo(uint64 amt) { if (m_info >= amt) m_info -= amt; else m_info = 0; }

	void InitializeWorld();
	void SpawnSelf();
	void PopulateWorld();

	void initGoId(uint32 theGoId);
	void HandleStateUpdate(ByteBuffer &srcData);
	void HandleCommand(ByteBuffer &srcCmd);

	string getHandle() const {return m_handle;}
	string getFirstName() const {return m_firstName;}
	string getLastName() const {return m_lastName;}
	string getBackground() const {return m_background;}
	bool setBackground(string newBackground);

	uint64 getExperience() const {return m_exp;}
	uint64 getInformation() const {return m_cash;}
	uint64 getCharacterUID() const { return m_characterUID; }
	uint64 getGuid() const { return m_characterUID; }
	uint8 getDistrict() const {return m_district;}
	void setDistrict(uint8 newDistrict) {m_district = newDistrict;}
	uint8 getRsiData(byte* outputBuf,size_t maxBufLen) const;

	//combat interface (implemented in PlayerObjectCombat.cpp; position, goId,
	//health, tactics and general combat state live in the IGO base)
	void respawn();
	void enterInterlock(uint32 partnerGoId) override;
	void leaveInterlock() override;
	void takeDamage(uint32 attackerGoId, uint16 damage, uint32 fxId) override;
	void die(uint32 killerGoId) override;
	void setCombatStance(bool inCombatStance) override;
	bool spendIS(uint16 amount);
	void restoreIS(uint16 amount);
	void sendHealthUpdate();				//broadcast health to observers + own HUD
	void sendVitals(bool includeMax=false, bool includeDead=false); //own HUD bars only
	void awardCombatExperience(uint32 amount);

	uint16 getCurrentIS() const {return m_innerStrC;}
	uint16 getInnerStrength() const override { return m_innerStrC; }
	void setInnerStrength(uint16 is) override { m_innerStrC = is; }
	uint16 getMaximumIS() const {return m_innerStrM;}
	class AbilitySystem* getAbilitySystem() { return m_abilitySystem.get(); }
	uint32 getProfession() const {return m_prof;}
	uint8 getLevel() const {return m_lvl;}
	uint8 getAlignment() const {return m_alignment;}
	bool getPvpFlag() const {return m_pvpflag;}

	uint8 getCurrentAnimation() const {return m_currAnimation;}
	uint8 getCurrentMood() const {return m_currMood;}

	class GameClient& getClient() { return m_parent; }
	vector<msgBaseClassPtr> getCurrentStatePackets();

	void Update();
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
	
    // Inventory & Item System
    void RPC_HandleItemMountRSI( ByteBuffer &srcCmd );
    void RPC_HandleItemUnmountRSI( ByteBuffer &srcCmd );
    void RPC_HandleItemMoveSlot( ByteBuffer &srcCmd );

    // Mission System UI Integration
    void RPC_HandleMissionRequest( ByteBuffer &srcCmd );
    void RPC_HandleMissionInfo( ByteBuffer &srcCmd );
    void RPC_HandleMissionAccept( ByteBuffer &srcCmd );
    void RPC_HandleMissionAbort( ByteBuffer &srcCmd );

    // Abilities & Memory
    // RPC_HandleAbilityLoad and RPC_HandleAbilityUse already exist.
    void RPC_HandleUpgradeAbility( ByteBuffer &srcCmd );
    void RPC_HandleMemoryChangeTactic( ByteBuffer &srcCmd );

    // Faction & Crew UI Integration
    void RPC_HandlePartyLeave( ByteBuffer &srcCmd );
    void RPC_HandleMissionInvite( ByteBuffer &srcCmd );
    void RPC_HandleFactionInfo( ByteBuffer &srcCmd );

    // Crafting (Coding) System
    void RPC_HandleCraftRequest( ByteBuffer &srcCmd );

    // Economy & Vendors
    void RPC_HandleVendorBuy( ByteBuffer &srcCmd );
    void RPC_HandleMarketOpen( ByteBuffer &srcCmd );
    void RPC_HandleMarketListItems( ByteBuffer &srcCmd );

	//combat RPC handlers (opcodes from the CR2 protocol map)
	void RPC_HandleCloseCombatRequest( ByteBuffer &srcCmd );	//0x40
	void RPC_HandleRangeCombatRequest( ByteBuffer &srcCmd );	//0x41
	void RPC_HandleChangeTactic( ByteBuffer &srcCmd );			//0x42
	void RPC_HandleLeaveCombat( ByteBuffer &srcCmd );			//0x44
	void RPC_HandleDuelRequest( ByteBuffer &srcCmd );			//0x50
	void RPC_HandleAbilityUse( ByteBuffer &srcCmd );			//0x80b9
	void RPC_HandleAbilityLoad( ByteBuffer &srcCmd );			//0x80ae

	//RPC Handler maps
	map<uint8,RPCHandler> m_RPCbyte;
	map<uint16,RPCHandler> m_RPCshort;
private:
	void loadFromDB(bool updatePos=false);
	void checkAndStore();
	void saveDataToDB();
	void setOnlineStatus( bool isOnline );

	typedef enum
	{
		EVENT_JACKOUT,
		EVENT_RESPAWN
	} eventType;

	typedef boost::function < void (void) > eventFunc;

	void addEvent(eventType type, eventFunc func, float activationTime);
	size_t cancelEvents(eventType type);

	struct eventStruct
	{
		eventStruct(eventType _type, eventFunc _func, float _fireTime) : type(_type), func(_func), fireTime(_fireTime) {}
		eventType type;
		eventFunc func;
		float fireTime;
	};

	list<eventStruct> m_events;

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

	uint64 m_exp,m_cash,m_info;
	uint8 m_district;
    std::string m_crewName;
    std::string m_factionName;
    bool m_isStealthed;
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

	uint8 m_emoteCounter;

	bool m_isAdmin;

	shared_ptr<class AbilitySystem> m_abilitySystem;
};

#endif
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

#include "Common.h"
#include "PlayerObject.h"
#include "RsiData.h"
#include "Database/Database.h"
#include "GameServer.h"
#include "MessageTypes.h"
#include "Log.h"
#include "GameClient.h"
#include "Timer.h"
#include "DataLoader.h"
#include "InventorySystem.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "SpatialGrid.h"
#include <boost/algorithm/string.hpp>

PlayerObject::PlayerObject( GameClient &parent,uint64 charUID, bool isBot ) :m_parent(parent),m_characterUID(charUID),m_spawnedInWorld(false),m_worldPopulated(false)
{
	if (!isBot) {
		loadFromDB(true);
	} else {
		m_handle = "Bot_" + std::to_string(charUID);
		m_firstName = "Bot";
		m_lastName = "NPC";
		m_background = "";
		m_healthC = 100;
		m_healthM = 100;
		m_innerStrC = 100;
		m_innerStrM = 100;
		m_lvl = 1;
		m_prof = 0;
		m_alignment = 0;
		m_pvpflag = false;
		m_exp = 0;
		m_cash = 0;
		m_district = 0;
		m_isAdmin = false;

		//default RSI so bots render as a standard male avatar
		m_rsi.reset(new RsiDataMale);
		const byte defaultRsiValues[] = {0x00,0x0C,0x71,0x48,0x18,0x0C,0xE2,0x00,0x23,0x00,0xB0,0x00,0x40,0x00,0x00};
		m_rsi->FromBytes(defaultRsiValues,sizeof(defaultRsiValues));
	}

	m_goId=0;
	if (!isBot) {
		INFO_LOG(format("Player object for %1% constructed") % m_handle);
	}
	testCount=0;
	m_lastStore = getTime();
	m_storeCntr = 0;
	m_currAnimation=0;
	m_currMood=0;
	m_emoteCounter=0;

	if (!isBot)
		setOnlineStatus(true); //bots are memory-only, never touch the DB
}

void PlayerObject::loadFromDB( bool updatePos )
{
	//grab data from characters table
	{
		format sql = format("SELECT `handle`, `firstName`, `lastName`, `background`,\
							`x`, `y`, `z`, `rot`, \
							`healthC`, `healthM`, `innerStrC`, `innerStrM`,\
							`level`, `profession`, `alignment`, `pvpflag`, `exp`, `cash`, `district`, `adminFlags`\
							FROM `characters` WHERE `charId` = '%1%' LIMIT 1") % m_characterUID;

		scoped_ptr<QueryResult> result(sDatabase.Query(sql));

		if (!result)
			throw CharacterNotFound();

		Field *field = result->Fetch();
		if (field[0].GetString() != NULL)
			m_handle = field[0].GetString();
		else
			throw CharacterNotFound();

		if (field[1].GetString() != NULL)
			m_firstName = field[1].GetString();
		else
			m_firstName = "NOFIRST";

		if (field[2].GetString() != NULL)
			m_lastName = field[2].GetString();
		else
			m_lastName = "NOLAST";

		if (field[3].GetString() != NULL)
			m_background = field[3].GetString();
		else
			m_background = "";

		if (updatePos)
		{
			m_pos.ChangeCoords(	field[4].GetDouble(),
				field[5].GetDouble(),
				field[6].GetDouble());
			m_pos.rot = field[7].GetDouble();
			m_savedPos = m_pos;
		}

		m_healthC = field[8].GetUInt16();
		m_healthM = field[9].GetUInt16();
		m_innerStrC = field[10].GetUInt16();
		m_innerStrM = field[11].GetUInt16();
		m_lvl = field[12].GetUInt8();
		m_prof = field[13].GetUInt32();
		m_alignment = field[14].GetUInt8();
		m_pvpflag = field[15].GetBool();
		m_exp = field[16].GetUInt64();
		m_cash = field[17].GetUInt64();
		m_district = field[18].GetUInt8();
		m_isAdmin = field[19].GetBool();
	}
	//grab data from rsi table
	{
		scoped_ptr<QueryResult> result(sDatabase.Query(format("SELECT `sex`, `body`, `hat`, `face`, `shirt`,\
															  `coat`, `pants`, `shoes`, `gloves`, `glasses`,\
															  `hair`, `facialdetail`, `shirtcolor`, `pantscolor`,\
															  `coatcolor`, `shoecolor`, `glassescolor`, `haircolor`,\
															  `skintone`, `tattoo`, `facialdetailcolor`, `leggings` FROM `rsivalues` WHERE `charId` = '%1%' LIMIT 1") % m_characterUID) );
		if (result == NULL)
		{
			INFO_LOG(format("SpawnRSI(%1%): Character's RSI doesn't exist") % m_handle );
			m_rsi.reset(new RsiDataMale);
			const byte defaultRsiValues[] = {0x00,0x0C,0x71,0x48,0x18,0x0C,0xE2,0x00,0x23,0x00,0xB0,0x00,0x40,0x00,0x00};
			m_rsi->FromBytes(defaultRsiValues,sizeof(defaultRsiValues));
		}
		else
		{
			Field *field = result->Fetch();
			uint8 sex = field[0].GetUInt8();

			if (sex == 0) //male
				m_rsi.reset(new RsiDataMale);
			else
				m_rsi.reset(new RsiDataFemale);

			RsiData &playerRef = *m_rsi;

			if (sex == 0) //male
				playerRef["Sex"]=0;
			else
				playerRef["Sex"]=1;

			playerRef["Body"] =			field[1].GetUInt8();
			playerRef["Hat"] =			field[2].GetUInt8();
			playerRef["Face"] =			field[3].GetUInt8();
			playerRef["Shirt"] =		field[4].GetUInt8();
			playerRef["Coat"] =			field[5].GetUInt8();
			playerRef["Pants"] =		field[6].GetUInt8();
			playerRef["Shoes"] =		field[7].GetUInt8();
			playerRef["Gloves"] =		field[8].GetUInt8();
			playerRef["Glasses"] =		field[9].GetUInt8();
			playerRef["Hair"] =			field[10].GetUInt8();
			playerRef["FacialDetail"]=	field[11].GetUInt8();
			playerRef["ShirtColor"] =	field[12].GetUInt8();
			playerRef["PantsColor"] =	field[13].GetUInt8();
			playerRef["CoatColor"] =	field[14].GetUInt8();
			playerRef["ShoeColor"] =	field[15].GetUInt8();
			playerRef["GlassesColor"]=	field[16].GetUInt8();
			playerRef["HairColor"] =	field[17].GetUInt8();
			playerRef["SkinTone"] =		field[18].GetUInt8();
			playerRef["Tattoo"] =		field[19].GetUInt8();
			playerRef["FacialDetailColor"] =	field[20].GetUInt8();

			if (sex != 0)
				playerRef["Leggings"] =	field[21].GetUInt8();
		}
	}
}

void PlayerObject::initGoId(uint32 theGoId)
{
	m_goId = theGoId;
	if (!m_parent.isBot())
	{
		INFO_LOG(format("Player name %1% has goid %2%") % m_handle % m_goId);
		m_parent.QueueCommand(make_shared<SystemChatMsg>((format("Your Object Id is %1%")%m_goId).str()));
		sGame.AnnounceCommand(&m_parent,make_shared<SystemChatMsg>((format("Player %1% connected with object id %2%")%m_handle%m_goId).str()));
	}
}

PlayerObject::~PlayerObject()
{
	if (m_spawnedInWorld == true)
	{
		//commit position changes
		saveDataToDB();
		setOnlineStatus(false);

		INFO_LOG(format("Player object for %1%:%2% deconstructing") % m_handle % m_goId);
		sGame.AnnounceStateUpdate(&m_parent,make_shared<DeletePlayerMsg>(m_goId));
		sGame.AnnounceCommand(&m_parent,make_shared<SystemChatMsg>((format("Player %1% with object id %2% disconnected")%m_handle%m_goId).str()));
		
		m_spawnedInWorld=false;
	}
}

uint8 PlayerObject::getRsiData( byte* outputBuf, size_t maxBufLen ) const
{
	if (m_rsi == NULL)
		return 0;

	return m_rsi->ToBytes(outputBuf,maxBufLen);
}

void PlayerObject::setRsiHex(const std::string& hexStr)
{
	if (hexStr.empty()) return;

	std::vector<byte> bytes;
	for (size_t i = 0; i + 1 < hexStr.size(); i += 2) {
		std::string byteString = hexStr.substr(i, 2);
		try {
			byte b = (byte)std::stoul(byteString, nullptr, 16);
			bytes.push_back(b);
		} catch (...) {
			break;
		}
	}

	if (bytes.empty()) return;

	if (bytes.size() >= 15) {
		if (!m_rsi) {
			m_rsi.reset(new RsiDataMale);
		}
		m_rsi->FromBytes(&bytes[0], bytes.size());
		return;
	}

	uint8 sex = (bytes[0] & 0x01);
	std::string nameLower = m_handle;
	std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
	if (nameLower.find("madonna") != std::string::npos ||
		nameLower.find("girl") != std::string::npos ||
		nameLower.find("woman") != std::string::npos ||
		nameLower.find("female") != std::string::npos ||
		nameLower.find("lady") != std::string::npos ||
		nameLower.find("sister") != std::string::npos)
	{
		sex = 1;
	}

	if (sex == 0)
		m_rsi.reset(new RsiDataMale);
	else
		m_rsi.reset(new RsiDataFemale);

	RsiData& rsi = *m_rsi;
	rsi["Sex"] = sex;
	rsi["Body"] = (bytes[0] >> 1) & 0x03;
	rsi["Hat"] = (bytes.size() > 1) ? (bytes[1] & 0x3F) : 0;
	rsi["Face"] = (bytes.size() > 2) ? (bytes[2] & 0x1F) : (uint8)(m_handle.length() % 20);
	rsi["Shirt"] = (bytes.size() > 3) ? (bytes[3] & 0x1F) : (uint8)((bytes[0] * 7) % 25);
	rsi["Coat"] = (bytes[0] % 15);
	rsi["Pants"] = ((bytes.size() > 1 ? bytes[1] : 3) % 20);
	rsi["Shoes"] = ((bytes.size() > 2 ? bytes[2] : 5) % 15);
	rsi["Gloves"] = (bytes[0] % 10);
	rsi["Glasses"] = (nameLower.find("agent") != std::string::npos) ? 1 : ((bytes[0] % 8));
	rsi["Hair"] = (bytes.size() > 1 ? (bytes[1] >> 2) % 20 : 2);
	rsi["FacialDetail"] = (bytes[0] % 10);
	rsi["ShirtColor"] = (nameLower.find("agent") != std::string::npos) ? 1 : ((bytes[0] * 3) % 40);
	rsi["PantsColor"] = (nameLower.find("agent") != std::string::npos) ? 1 : ((bytes[0] * 5) % 30);
	rsi["CoatColor"] = (nameLower.find("agent") != std::string::npos) ? 1 : ((bytes[0] * 2) % 30);
	rsi["ShoeColor"] = 1;
	rsi["GlassesColor"] = 1;
	rsi["HairColor"] = (bytes.size() > 2 ? (bytes[2] % 12) : 1);
	rsi["SkinTone"] = (bytes.size() > 3 ? (bytes[3] % 10) : 2);
	rsi["Tattoo"] = (bytes[0] % 8);
	rsi["FacialDetailColor"] = 0;
}

void PlayerObject::checkAndStore()
{
	if (getTime() - m_lastStore > 10) //every 10 seconds
	{
		saveDataToDB();
		m_lastStore = getTime();
	}
}

void PlayerObject::saveDataToDB()
{
	if (m_characterUID >= 9000000) //virtual bots are memory-only
		return;

	if (m_savedPos == m_pos)
		return setOnlineStatus(true);

	bool storeSuccess = sDatabase.Execute(format("UPDATE `characters` SET `x` = '%1%', `y` = '%2%', `z` = '%3%', `rot` = '%4%', `lastOnline` = NOW() WHERE `charId` = '%5%'")
		% m_pos.x
		% m_pos.y
		% m_pos.z
		% m_pos.rot
		% m_characterUID );

	if (!storeSuccess)
		WARNING_LOG(format("%1%:%2% failed to save data to database") % m_handle % m_goId );
	else
	{
		m_savedPos = m_pos;
		if (m_storeCntr >= 10)
		{
			m_parent.QueueCommand(make_shared<SystemChatMsg>( (format("Character data for %1% has been written to the database.") % m_handle).str() ));
			m_storeCntr=0;
		}
		m_storeCntr++;
	}
}

void PlayerObject::setOnlineStatus( bool isOnline )
{
	if (m_characterUID >= 9000000) //virtual bots are memory-only
		return;

	sDatabase.Execute(format("UPDATE `characters` SET `lastOnline` = NOW(), `isOnline` = '%1%' WHERE `charId` = '%2%'")
		% int(isOnline)
		% m_characterUID );
}

void PlayerObject::InitializeWorld()
{
	m_parent.QueueCommand(make_shared<LoadWorldCmd>((LoadWorldCmd::mxoLocation)m_district,"Massive"));
	m_parent.QueueCommand(make_shared<SetExperienceCmd>(m_exp));
	m_parent.QueueCommand(make_shared<SetInformationCmd>(m_cash));
/*	m_parent.QueueCommand(make_shared<HexGenericMsg>("80b24e0008000802"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80b2520005000802"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80b2540008000802"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80b24f0008000802"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80b251000b000802"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80b2110001000802"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc4503110000020000001100010000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc450002000002000000cc00000000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc1500030000f70300000802000000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc1500040000f70300000702ecffffff0000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc1500050000f70300005004000000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc1500060000f7030000f403000000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc1500070000f70300005104f6ffffff0000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80bc1500080000f703000052040f0000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("47000004010000000000000000000000000000001a0006000000010000000001010000000000800000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("2e0700000000000000000000005900002e00000000000000000000000000000000000000"));
	m_parent.QueueCommand(make_shared<HexGenericMsg>("80865220060000000000000000000000000000000000000000210000000000230000000000"));*/
	m_parent.QueueCommand(make_shared<EventURLCmd>("http://mxoemu.info/forum/index.php"));
	
	m_parent.QueueCommand(make_shared<SystemChatMsg>("{c:FF0000}W{/c}{c:DD0000}e{/c}{c:EE0000}l{/c}{c:CC0000}c{/c}{c:BB0000}o{/c}{c:AA0000}m{/c}{c:990000}e{/c} {c:880000}B{/c}{c:770000}a{/c}{c:660000}c{/c}{c:550000}k{/c}{c:440000}.{/c}{c:330000}.{/c}{c:220000}.{/c}"));
}

void PlayerObject::UpdateAppearance()
{
	loadFromDB(false);
	sGame.AnnounceStateUpdate(NULL,make_shared<PlayerAppearanceMsg>(m_goId));
}

void PlayerObject::SpawnSelf()
{
	try {
		if (m_spawnedInWorld == false)
		{
			shared_ptr<PlayerSpawnMsg> dMsg = make_shared<PlayerSpawnMsg>(m_goId);
			m_parent.QueueState(dMsg,false,boost::bind(&PlayerObject::PopulateWorld,this));
			sGame.AnnounceStateUpdate(&m_parent,dMsg);
			m_spawnedInWorld=true;
		}
	} catch (std::exception& e) {
		std::cout << "DEBUG: SpawnSelf EXCEPTION: " << e.what() << std::endl;
		throw;
	} catch (...) {
		std::cout << "DEBUG: SpawnSelf UNKNOWN EXCEPTION" << std::endl;
		throw;
	}
}

void PlayerObject::PopulateWorld()
{
	if (m_worldPopulated)
		return;

	// Dynamic Area-of-Interest (AoI) Streaming: query SpatialGrid within 250m for initial world population
	auto nearbyClients = sSpatialGrid.GetClientsInAoI(m_pos.x, m_pos.z, 25000.0f);
	for (GameClient* client : nearbyClients)
	{
		if (!client || client == &m_parent) continue;
		uint32 otherGoId = client->GetPlayerGoId();
		if (otherGoId == 0 || otherGoId == m_goId) continue;

		PlayerObject* theOtherObject = sObjMgr.getGOPtrSafe(otherGoId);
		if (theOtherObject != NULL && theOtherObject != this && !theOtherObject->isDead())
		{
			m_knownEntities.insert(otherGoId);
			vector<msgBaseClassPtr> objectsPackets = theOtherObject->getCurrentStatePackets();
			for (vector<msgBaseClassPtr>::iterator it2=objectsPackets.begin();it2!=objectsPackets.end();++it2)
			{
				if (m_spawnedInWorld)
					m_parent.QueueState(*it2);
				else
					m_sendAfterSpawn.push(*it2);
			}
		}
	}

	//open doors that are opened
	vector<msgBaseClassPtr> openedDoorPackets = sObjMgr.GetAllOpenDoors(&m_parent);
	for (vector<msgBaseClassPtr>::iterator it=openedDoorPackets.begin();it!=openedDoorPackets.end();++it)
	{
		if (m_spawnedInWorld)
			m_parent.QueueState(*it);
		else
			m_sendAfterSpawn.push(*it);
	}
	m_worldPopulated=true;
}

void PlayerObject::UpdateAoIStreaming()
{
	uint32 now = getMSTime();
	if (now - m_lastAoIUpdateMs < 250) // Throttle to ~4Hz to minimize CPU overhead
		return;
	m_lastAoIUpdateMs = now;

	const float STREAM_IN_RADIUS = 25000.0f;     // 250m: dynamically stream in
	const float STREAM_OUT_RADIUS_SQ = 30000.0f * 30000.0f; // 300m: cull with hysteresis

	// 1. Stream in entities entering 250m
	auto nearbyClients = sSpatialGrid.GetClientsInAoI(m_pos.x, m_pos.z, STREAM_IN_RADIUS);
	for (GameClient* client : nearbyClients)
	{
		if (!client || client == &m_parent) continue;
		uint32 otherGoId = client->GetPlayerGoId();
		if (otherGoId == 0 || otherGoId == m_goId) continue;

		if (m_knownEntities.find(otherGoId) == m_knownEntities.end())
		{
			PlayerObject* otherObj = sObjMgr.getGOPtrSafe(otherGoId);
			if (otherObj && !otherObj->isDead())
			{
				m_knownEntities.insert(otherGoId);
				vector<msgBaseClassPtr> statePackets = otherObj->getCurrentStatePackets();
				for (const auto& pkt : statePackets)
				{
					m_parent.QueueState(pkt);
				}
			}
		}
	}

	// 2. Stream out (cull) entities leaving 300m or dead
	for (auto it = m_knownEntities.begin(); it != m_knownEntities.end(); )
	{
		uint32 knownGoId = *it;
		PlayerObject* otherObj = sObjMgr.getGOPtrSafe(knownGoId);
		bool cull = false;

		if (!otherObj || otherObj->isDead())
		{
			cull = true;
		}
		else if (m_pos.Distance2DSq(otherObj->getPosition()) > STREAM_OUT_RADIUS_SQ)
		{
			cull = true;
		}

		if (cull)
		{
			try
			{
				m_parent.QueueState(make_shared<DeletePlayerMsg>(knownGoId));
			}
			catch (...) {}
			it = m_knownEntities.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void PlayerObject::HandleStateUpdate( ByteBuffer &srcData )
{
/*	testCount++;
	m_parent.QueueCommand(make_shared<SystemChatMsg>( (format("CMD %1%")%testCount).str() ));*/

	uint8 zeroThree;
	if (srcData.remaining() < sizeof(zeroThree))
		return;
	srcData >> zeroThree;
	if (zeroThree != 3)
		return;
	uint16 viewIdToUpdate;
	if (srcData.remaining() < sizeof(viewIdToUpdate))
		return;
	srcData >> viewIdToUpdate;
	if (viewIdToUpdate != sObjMgr.getViewForGO(&m_parent,m_goId))
	{
		WARNING_LOG(format("Client %1% Player %2%:%3% trying to update someone else's object view %4%") % m_parent.Address() % m_handle % m_goId % viewIdToUpdate);
		return;
	}
	size_t restOfDataPos = srcData.rpos();
	uint8 shouldBeOne;
	if (srcData.remaining() < sizeof(shouldBeOne))
		return;
	srcData >> shouldBeOne;
	if (shouldBeOne != 1)
	{
		WARNING_LOG(format("Client %1% Player %2%:%3% 03 doesn't have number 1 after viewId, packet: %4%") % m_parent.Address() % m_handle % m_goId % Bin2Hex(srcData));
		return;
	}
	uint8 updateType;
	if (srcData.remaining() < sizeof(updateType))
		return;
	srcData >> updateType;
	bool validUpdate=false;
	bool movementUpdate=false;
	switch (updateType)
	{
	//change angle
	case 0x04:
		{
			movementUpdate=true;

			uint8 theRotByte;
			if (srcData.remaining() < sizeof(theRotByte))
				return;
			srcData >> theRotByte;

			m_pos.setMxoRot(theRotByte);
			validUpdate=true;
			break;
		}
	//change angle with extra param
	case 0x06:
		{
			movementUpdate=true;

			uint8 theAnimation;
			if (srcData.remaining() < sizeof(theAnimation))
				return;
			srcData >> theAnimation;
			//we will just ignore the animation for now
			uint8 theRotByte;
			if (srcData.remaining() < sizeof(theRotByte))
				return;
			srcData >> theRotByte;

			m_pos.setMxoRot(theRotByte);
			validUpdate=true;
			break;
		}
	//update xyz
	case 0x08:
		{
			movementUpdate=true;

			validUpdate = m_pos.fromFloatBuf(srcData);
			break;
		}
	//update xyz, extra byte before xyz
	case 0x0A:
	case 0x0C:
		{
			movementUpdate=true;

			uint8 extraByte;
			if (srcData.remaining() < sizeof(extraByte))
				return;
			srcData >> extraByte;
			
			validUpdate = m_pos.fromFloatBuf(srcData);
			break;
		}
	//update xyz, extra 2 bytes before xyz
	case 0x0E:
		{
			movementUpdate=true;

			uint8 extraByte1,extraByte2;
			if (srcData.remaining() < sizeof(uint8)*2)
				return;
			srcData >> extraByte1;
			srcData >> extraByte2;

			validUpdate = m_pos.fromFloatBuf(srcData);
			break;
		}
	//sometimes happens, no info inside
	case 0x02:
		{
			validUpdate = true;
			break;
		}
	}
	if (validUpdate)
	{
		if(movementUpdate)
		{
			size_t cancelled = this->cancelEvents(EVENT_JACKOUT);
			if (cancelled > 0)
			{
				m_parent.QueueCommand(make_shared<SystemChatMsg>("Jackout cancelled."));
				m_parent.QueueState(make_shared<JackoutEffectMsg>(m_goId,false));
			}
		}

		//propagate state to all other players
		srcData.rpos(restOfDataPos);
		ByteBuffer theStateData;
		theStateData.append(&srcData.contents()[srcData.rpos()],srcData.remaining());
		//m_parent.QueueState(make_shared<StateUpdateMsg>(m_goId,theStateData));
		sGame.AnnounceStateUpdate(&m_parent,make_shared<StateUpdateMsg>(m_goId,theStateData),true);
	}
	else
	{
		srcData.rpos(0);
		DEBUG_LOG(format("(%1%) %2%:%3% 03 data: %4%") % m_parent.Address() % m_handle % m_goId % Bin2Hex(srcData) );
	}
}

#include <boost/algorithm/string.hpp>
using boost::iequals;

void PlayerObject::HandleCommand( ByteBuffer &srcCmd )
{
//	DEBUG_LOG(format(" HandleCommand: %1%")% Bin2Hex(srcCmd) );

	//set up handler ptrs
	if (!m_RPCbyte.size())
	{
		m_RPCbyte[0x05] = &PlayerObject::RPC_HandleReadyForSpawn;
		m_RPCbyte[0x33] = &PlayerObject::RPC_HandleStopAnimation;
		m_RPCbyte[0x34] = &PlayerObject::RPC_HandleStartAnimtion;
		m_RPCbyte[0x35] = &PlayerObject::RPC_HandleChangeMood;
		m_RPCbyte[0x30] = &PlayerObject::RPC_HandlePerformEmote;
	}
	if (!m_RPCshort.size())
	{
		m_RPCshort[0x2810] = &PlayerObject::RPC_HandleChat;
		m_RPCshort[0x2907] = &PlayerObject::RPC_HandleWhisper;
		m_RPCshort[0x80c7] = &PlayerObject::RPC_HandleDynamicObjInteraction;
		m_RPCshort[0x80c8] = &PlayerObject::RPC_HandleStaticObjInteraction;
		m_RPCshort[0x80c2] = &PlayerObject::RPC_HandleJump;
		m_RPCshort[0x80c9] = &PlayerObject::RPC_HandleRegionLoadedNotification;
		m_RPCshort[0x8108] = &PlayerObject::RPC_HandleReadyForWorldChange;
		m_RPCshort[0x8152] = &PlayerObject::RPC_HandleWho;
		m_RPCshort[0x8154] = &PlayerObject::RPC_HandleWhereAmI;
		m_RPCshort[0x8192] = &PlayerObject::RPC_HandleGetPlayerDetails;
		m_RPCshort[0x8194] = &PlayerObject::RPC_HandleGetBackground;
		m_RPCshort[0x8196] = &PlayerObject::RPC_HandleSetBackground;
		m_RPCshort[0x818e] = &PlayerObject::RPC_HandleHardlineTeleport;
		m_RPCshort[0x8151] = &PlayerObject::RPC_HandleObjectSelected;
		m_RPCshort[0x80fc] = &PlayerObject::RPC_HandleJackoutRequest;
		m_RPCshort[0x80fe] = &PlayerObject::RPC_HandleJackoutFinished;
	}

	uint8 firstByte = srcCmd.read<uint8>();

	try
	{
		if (m_RPCbyte.count(firstByte))
		{
			CALL_METHOD_PTR(this,m_RPCbyte[firstByte])(srcCmd);
			return;
		}
		else
		{
			if (srcCmd.remaining())
			{
				uint8 secondByte = srcCmd.read<uint8>();
				uint16 shortCommand = (uint16(firstByte) << 8) | (secondByte & 0xFF);
				if (m_RPCshort.count(shortCommand))
				{
					CALL_METHOD_PTR(this,m_RPCshort[shortCommand])(srcCmd);
					return;
				}
			}
		}
	}
	catch ( ByteBuffer::out_of_range )
	{
		srcCmd.rpos(0);
		DEBUG_LOG(format("(%1%) Out of range error processing RPC data: %2%") % m_parent.Address() % Bin2Hex(srcCmd) );
		return;
	}

	srcCmd.rpos(0);
	DEBUG_LOG(format("(%1%) unhandled RPC data: %2%") % m_parent.Address() % Bin2Hex(srcCmd) );
}

bool PlayerObject::setBackground(string newBackground)
{
	m_background = newBackground;

	return sDatabase.Execute(format("UPDATE `characters` SET `background` = '%1%' WHERE `charId` = '%2%'")
		% sDatabase.EscapeString(this->getBackground())
		% m_characterUID );
}

vector<msgBaseClassPtr> PlayerObject::getCurrentStatePackets()
{
	vector<msgBaseClassPtr> tempVect;
	tempVect.push_back(make_shared<PlayerSpawnMsg>(m_goId));
	if (m_currAnimation != 0 || m_currMood != 0)
	{
		tempVect.push_back(make_shared<AnimationStateMsg>(m_goId));
	}
	return tempVect;
}

void PlayerObject::GoAhead(double distanceToGo)
{		
	//double angle = this->getPosition().getMxoRot();
	//string debugMsg = (format("X:%1% Y:%2% Z:%3% Rotation:%4% Angle:%5%") % this->getPosition().x % this->getPosition().y % this->getPosition().z % this->getPosition().rot % angle).str();		
	//this->getClient().QueueCommand(make_shared<WhisperMsg>("TW",debugMsg));

	LocationVector newLoc = this->getPosition();

	double xInc = 0;
	double zInc = 0;
	
	double sAngle = sin(newLoc.rot);
	xInc = distanceToGo * sAngle;
	zInc = sqrt(distanceToGo * distanceToGo - xInc * xInc);
	xInc *= 100;
	zInc *= 100;
	newLoc.x -= xInc;
	if (abs(newLoc.rot) > M_PI/2)
	{
		newLoc.z += zInc;
	}
	else
	{
		newLoc.z -= zInc;
	}
	this->setPosition(newLoc);
	sGame.AnnounceStateUpdate(NULL,make_shared<PositionStateMsg>(m_goId),true);
}

void PlayerObject::Update()
{
	if (m_spawnedInWorld)
	{
		if (m_timeDilation != 1.0f && getMSTime() >= m_timeDilationExpires) {
			m_timeDilation = 1.0f;
		}

		if (m_deathDelayMS > 0 && getMSTime() >= m_deathDelayMS) {
				m_deathDelayMS = 0;
				die(m_deathDelayKillerId);
		}

		//flush any updates that queued up while we were spawning
		while(m_sendAfterSpawn.size())
		{
			m_parent.QueueState(m_sendAfterSpawn.front());
			m_sendAfterSpawn.pop();
		}

		// Dynamic Area-of-Interest (AoI) Streaming for human players
		if (!m_parent.isBot() && m_worldPopulated)
		{
			UpdateAoIStreaming();
		}

		checkAndStore();

		//fire events that occurred safely with local buffer under m_eventMutex
		std::vector<eventFunc> readyCallbacks;
		{
			std::lock_guard<std::mutex> lock(m_eventMutex);
			for(list<eventStruct>::iterator it=m_events.begin();it!=m_events.end();)
			{
				if (getFloatTime() >= it->fireTime)
				{
					readyCallbacks.push_back(it->func);
					it=m_events.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
		for(auto& cb : readyCallbacks)
		{
			cb();
		}
	}
}

void PlayerObject::addEvent( eventType type, eventFunc func, float activationTime )
{
	std::lock_guard<std::mutex> lock(m_eventMutex);
	m_events.push_back(eventStruct(type,func,getFloatTime()+activationTime));
}

size_t PlayerObject::cancelEvents( eventType type )
{
	std::lock_guard<std::mutex> lock(m_eventMutex);
	size_t cancelledEvents=0;
	for(list<eventStruct>::iterator it=m_events.begin();it!=m_events.end();)
	{
		if (it->type == type)
		{
			it = m_events.erase(it);
			cancelledEvents++;
		}
		else
		{
			++it;
		}
	}
	return cancelledEvents;
}

// Added stub definitions
void PlayerObject::PerformRebirth(void) { }
bool PlayerObject::giveItem(unsigned int templateId)
{
    if (!m_inventorySystem) return false;
    if (m_inventorySystem->getFirstFreeSlot() == 0) return false;
    uint32 newGoId = sObjMgr.getNewObjectId();
    shared_ptr<Item> newItem(new Item(newGoId, templateId));
    if (m_inventorySystem->addItemAuto(newItem)) {
        m_inventorySystem->saveToDB();
        return true;
    }
    return false;
}

void PlayerObject::SendWaypoint(float x, float y, float z, const std::string& name) { }
std::vector<std::shared_ptr<class Item>> PlayerObject::getEquippedWeapons() {
    std::vector<std::shared_ptr<class Item>> weapons;
	if (!m_inventorySystem) return weapons;
	auto items = m_inventorySystem->getAllItems();
	for (auto item : items) {
		const ItemTemplate* tpl = sDataLoader.GetItemTemplate(item->getTemplateId());
		if (tpl && tpl->type == ITEM_TYPE_WEAPON) {
			weapons.push_back(item);
		}
	}
	return weapons;
}

void PlayerObject::degradeEquippedWeapon(uint16 degradationAmount) {
	auto weapons = getEquippedWeapons();
	for (auto weapon : weapons) {
		weapon->reduceDurability((float)degradationAmount);
		if (weapon->getDurability() <= 0.0f) {
			if (!getClient().isBot()) {
				getClient().QueueState(std::make_shared<SystemChatMsg>("{c:FF0000}[SYSTEM] Your equipped weapon has broken due to low durability!{/c}"));
			}
		}
	}
}
void PlayerObject::addInfo(uint64 amount) { m_cash += amount; }
void PlayerObject::removeInfo(uint64 amount) { if(m_cash >= amount) m_cash -= amount; }
void PlayerObject::addExp(uint64 amount) { m_exp += amount; }
std::shared_ptr<class InventorySystem> PlayerObject::getInventory() { return m_inventorySystem; }

void PlayerObject::ApplyTimeDilation(float amount, unsigned int durationMs) {
    m_timeDilation = amount;
    m_timeDilationExpires = getMSTime() + durationMs;
}

unsigned short PlayerObject::getEvasion() const {
    unsigned short evasion = 10 + (m_lvl * 2); // Base evasion
    auto items = m_inventorySystem->getAllItems();
    for (auto item : items) {
        const ItemTemplate* tpl = sDataLoader.GetItemTemplate(item->getTemplateId());
        if (tpl && tpl->type == ITEM_TYPE_CLOTHING) {
            evasion += tpl->bonusEvasion;
        }
    }
    return evasion;
}

bool PlayerObject::isDualWielding() const {
    auto weapons = const_cast<PlayerObject*>(this)->getEquippedWeapons();
    if (weapons.size() >= 2) return true;
    if (weapons.size() == 1) {
        const ItemTemplate* tpl = sDataLoader.GetItemTemplate(weapons[0]->getTemplateId());
        if (tpl && tpl->type == ITEM_TYPE_WEAPON) {
            return tpl->isDualWield;
        }
    }
    return false;
}


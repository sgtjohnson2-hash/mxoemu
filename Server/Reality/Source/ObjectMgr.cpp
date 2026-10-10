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

#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameClient.h"
#include "Database/DatabaseEnv.h"
#include "BotManager.h"
#include "GOAttributes.h"

ObjectMgr::ObjectMgr() : m_currFreeObjectId(OBJECTMANAGER_STARTINGOBJECTID),
    m_playerPool(std::make_unique<ObjectPool<PlayerObject>>())
{
}

ObjectMgr::~ObjectMgr()
{
}

uint32 ObjectMgr::constructPlayer( GameClient* requester, uint64 charUID, bool isBot )
{
	if (requester == NULL)
		throw ClientNotAvailable();

	PlayerObject *newPlayerObj = NULL;
	try
	{
		newPlayerObj = m_playerPool->acquire(*requester, charUID, isBot);
	}
	catch (PlayerObject::CharacterNotFound)
	{
		m_playerPool->release(newPlayerObj);
		throw ObjectNotAvailable();
	}

	uint32 theNewObjectId = getNewObjectId();
	newPlayerObj->initGoId(theNewObjectId);
	requester->setPlayer(newPlayerObj);
	
	// Use custom deleter for shared_ptr to return memory to pool
	auto customDeleter = [this](PlayerObject* p) {
		m_playerPool->release(p);
	};
	{
		std::unique_lock<std::shared_mutex> lock(m_objMutex);
		m_objects[theNewObjectId] = std::shared_ptr<PlayerObject>(newPlayerObj, customDeleter);
		if (!isBot) {
			m_humanPlayerGoIds.push_back(theNewObjectId);
		}
	}

	return theNewObjectId;
}

uint32 ObjectMgr::getNewObjectId()
{
	const uint32 kLast = 0xFFF0;
	std::lock_guard<std::mutex> idLock(m_idAllocMutex);
	uint32 now = getMSTime();
	for (uint32 tries = 0; tries < (kLast - OBJECTMANAGER_STARTINGOBJECTID); ++tries)
	{
		uint32 id = m_currFreeObjectId.fetch_add(1);
		if (id < OBJECTMANAGER_STARTINGOBJECTID || id > kLast)
		{
			m_currFreeObjectId = OBJECTMANAGER_STARTINGOBJECTID + 1;
			id = OBJECTMANAGER_STARTINGOBJECTID;
		}
		{
			std::shared_lock<std::shared_mutex> lock(m_objMutex);
			if (m_objects.find(id) != m_objects.end())
				continue;
		}
		map<uint32,uint32>::iterator freed = m_recentlyFreedIds.find(id);
		if (freed != m_recentlyFreedIds.end())
		{
			if (now - freed->second < 60000)
				continue;
			m_recentlyFreedIds.erase(freed);
		}
		return id;
	}
	ERROR_LOG("ObjectMgr: no free object id in the 16-bit view range; reusing the oldest freed id");
	uint32 oldest = OBJECTMANAGER_STARTINGOBJECTID;
	uint32 oldestTime = 0xFFFFFFFF;
	for (map<uint32,uint32>::iterator it = m_recentlyFreedIds.begin(); it != m_recentlyFreedIds.end(); ++it)
		if (it->second < oldestTime) { oldestTime = it->second; oldest = it->first; }
	m_recentlyFreedIds.erase(oldest);
	return oldest;
}

void ObjectMgr::destroyObject( uint32 goId )
{
	{
		std::lock_guard<std::mutex> idLock(m_idAllocMutex);
		m_recentlyFreedIds[goId] = getMSTime();
	}
	//The PlayerObject destructor broadcasts (QueueState -> per-client queue locks), so it must
	//not run while m_objMutex is held: a FlushQueue holding a queue lock needs m_objMutex to
	//serialize, which deadlocked against this. Detach under the lock, destroy after it.
	objectPtr doomed;
	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	//erase from valid objects
	objectsMap::iterator it=m_objects.find(goId);
	if (it!=m_objects.end())
	{
		if (it->second) {
			it->second->getClient().setPlayer(nullptr);
		}
		doomed = std::move(it->second);
		m_objects.erase(it);
	}

	auto hIt = std::find(m_humanPlayerGoIds.begin(), m_humanPlayerGoIds.end(), goId);
	if (hIt != m_humanPlayerGoIds.end()) {
		m_humanPlayerGoIds.erase(hIt);
	}

	//erase from object view maps (of all clients) and release object view
	for (clientToViewMap::iterator it1=m_views.begin();it1!=m_views.end();++it1)
	{
		for (viewIdsMap::iterator it2=it1->second.begin();it2!=it1->second.end();)
		{
			uint32 theGoId = it2->second;
			if (theGoId == goId)
				it1->second.erase(it2++);
			else
				++it2;
		}
	}
	lock.unlock();
	doomed.reset();
}

class PlayerObject* ObjectMgr::getGOPtr( uint32 goId )
{
	if (goId == 0) return nullptr;
	std::shared_lock<std::shared_mutex> lock(m_objMutex);
	objectsMap::iterator it=m_objects.find(goId);
	if (it!=m_objects.end())
		return it->second.get();

	return NULL;
}

class PlayerObject* ObjectMgr::getGOPtrSafe( uint32 goId )
{
	return getGOPtr(goId);
}

std::shared_ptr<class PlayerObject> ObjectMgr::getGOSharedPtr( uint32 goId )
{
	if (goId == 0) return nullptr;
	std::shared_lock<std::shared_mutex> lock(m_objMutex);
	objectsMap::iterator it=m_objects.find(goId);
	if (it!=m_objects.end())
		return it->second;

	return nullptr;
}

uint32 ObjectMgr::getGOId( class PlayerObject* forWhichObj )
{
	if (forWhichObj==NULL)
		return 0;

	std::shared_lock<std::shared_mutex> lock(m_objMutex);
	for (objectsMap::iterator it=m_objects.begin();it!=m_objects.end();++it)
	{
		if (it->second.get() == forWhichObj)
			return it->first;
	}

	return 0;
}

uint16 ObjectMgr::getViewForGO( GameClient* requester, uint32 goId )
{
	if (requester==NULL)
		throw ClientNotAvailable();

	// The client's own avatar is registered internally as VIEWID_SELF (2).
	// ILTCommHandleMgr::ResolveHandle (0x622CEDA0) queries ObjectManager::FindObjectByViewId
	// with the low 16 bits of the slot handle. If the local player's view ID is not VIEWID_SELF,
	// ResolveHandle returns NULL, and the interlock exchange scheduler bails at 0x625F72E7.
	if (!requester->isBot() && requester->GetPlayerGoId() != 0 && goId == requester->GetPlayerGoId())
		return VIEWID_SELF;

	return uint16(goId);
}

void ObjectMgr::releaseRelevantSet( GameClient *requester )
{
	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	clientToViewMap::iterator it=m_views.find(requester);
	if (it!=m_views.end())
		m_views.erase(it);
}

uint16 ObjectMgr::allocateViewId( GameClient* requester)
{
	if (requester == NULL)
		throw ClientNotAvailable();

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	//get the views map for current client
	const viewIdsMap &viewsOfClient = m_views[requester];
	//go through all possible viewIds, when we find one thats not in the list, return it
	for (uint16 id=3;id<OBJECTMANAGER_STARTINGOBJECTID;++id) //1 = object manager, 2 = self view (client-reserved)
	{
		if (viewsOfClient.find(id) == viewsOfClient.end())
			return id;
	}

	throw NoMoreFreeViews();
}

uint32 ObjectMgr::getGOForView(class GameClient *requester, uint16 viewId) {
    if (requester == NULL) throw ClientNotAvailable();
    if (!requester->isBot() && viewId == VIEWID_SELF)
        return requester->GetPlayerGoId();
    return uint32(viewId); // Simple 1:1 mapping for other entities
}

uint16 ObjectMgr::allocateDynamicView(class GameClient *requester, uint32 tagObjId) {
    if (requester == NULL) throw ClientNotAvailable();
    uint16 newViewId = allocateViewId(requester);
    std::unique_lock<std::shared_mutex> lock(m_objMutex);
    m_views[requester][newViewId] = tagObjId;
    return newViewId;
}

void ObjectMgr::releaseDynamicView(class GameClient *requester, uint16 viewId) {
    if (requester == NULL) return;
    std::unique_lock<std::shared_mutex> lock(m_objMutex);
    clientToViewMap::iterator it = m_views.find(requester);
    if (it != m_views.end()) {
        it->second.erase(viewId);
    }
}


#include "GameServer.h"

void ObjectMgr::RandomObject( uint32 randomObjectId, GameClient* requester, double X, double Y, double Z, double ROT)
{
	//uint32 randObjId = rand() % 0xFFFFFFFF;

	string msg1 = (format("{c:0FFFF0}Object :%1%{/c}") % (int)randomObjectId).str();
	requester->QueueCommand(make_shared<SystemChatMsg>(msg1));

	
	uint16 randViewId = rand() % 0xFFFF;

	std::string s;
	std::stringstream out;
	out << "Do Object With ViewId: ";
	out << int(randViewId);
	s = out.str();

	requester->QueueCommand(make_shared<SystemChatMsg>(s));
	requester->QueueState(make_shared<DoorAnimationMsg>(randomObjectId, randViewId, X, Y, Z, ROT, 1));	
}


void ObjectMgr::OpenDoor( uint32 doorId, GameClient* requester)
{
	//Does this work?
	for (map<uint16,uint32>::iterator it=m_openDoors.begin();it!=m_openDoors.end();++it)
	{
		if (it->second == doorId)
		{
			//Didnt work first time, lets change door type
			format sqlUpdateDoorType = format("UPDATE `doors` SET `DoorType`='0' WHERE `DoorId`='%1%' LIMIT 1") % doorId;
			if (sDatabase.Execute(sqlUpdateDoorType))
			{
				format msg1 = format("{c:0FFFF0}Door:0x%08x Set to Indoors since u tried to open it but I think it is open, try again.{/c}") % (int)doorId;
				
				//Close Doors not working
				//sGame.AnnounceStateUpdate(NULL,make_shared<CloseDoorMsg>(it->first));	

				requester->QueueCommand(make_shared<SystemChatMsg>(msg1.str()));
			}
			m_openDoors.erase(it);
			return;
		}
		
	}

	format msg2 = format("{c:0FFFF0}Door:0x%08x{/c}") % (int)doorId;
	requester->QueueCommand(make_shared<SystemChatMsg>(msg2.str()));

	if (m_openDoors.size() > 37000) //if lots of doors, then we want to close one
	{
		m_openDoors.erase(m_openDoors.begin());
		//Close Doors not working
		//sGame.AnnounceStateUpdate(NULL,make_shared<CloseDoorMsg>(it->first));						
	}

	uint16 viewId = allocateViewId(requester);
	viewIdsMap &viewsOfClient = m_views[requester];
	viewsOfClient[viewId]=doorId;
	m_openDoors[viewId]=doorId;	

	format s = format("Door Opened With ViewId: %1%") % int(viewId);
	DEBUG_LOG(s.str());

	//sGame.AnnounceStateUpdate(NULL,make_shared<DeleteDoorMsg>(doorId));	
	format sqlDoor = format("SELECT `X`, `Y`, `Z`, `ROT`, `DoorType` FROM `doors` WHERE `DoorId`='%1%' LIMIT 1") % doorId;

	scoped_ptr<QueryResult> resultDoor(sDatabase.Query(sqlDoor));
	if (resultDoor != NULL)
	{
		Field *field = resultDoor->Fetch();
		double X = field[0].GetDouble();
		double Y = field[1].GetDouble();
		double Z = field[2].GetDouble();
		double O = field[3].GetDouble();
		int doorType = field[4].GetUInt16();
		requester->QueueState(make_shared<DoorAnimationMsg>(doorId, viewId, X, Y, Z, O, doorType));
	}

}

vector<msgBaseClassPtr> ObjectMgr::GetAllOpenDoors( GameClient* requester )
{
	vector<msgBaseClassPtr> tempVec;

	//Does this work?

	//Set cleitn views to current views.. (so we know which doors are open)
	viewIdsMap &viewsOfClient = m_views[requester];

	for (map<uint16,uint32>::iterator it=m_openDoors.begin();it!=m_openDoors.end();++it)
	{
		viewsOfClient[it->first] = it->second;

		format sqlDoor = format("SELECT `X`, `Y`, `Z`, `ROT`, `DoorType` FROM `doors` WHERE `DoorId`='%1%' LIMIT 1") % it->second;
		scoped_ptr<QueryResult> resultDoor(sDatabase.Query(sqlDoor));
		if (resultDoor != NULL)
		{
			Field *field = resultDoor->Fetch();
			double X = field[0].GetDouble();
			double Y = field[1].GetDouble();
			double Z = field[2].GetDouble();
			double O = field[3].GetDouble();
			int doorType = field[4].GetUInt16();
			//int doorType = 1;
			
			tempVec.push_back(make_shared<DoorAnimationMsg>(it->second,it->first, X, Y, Z, O, doorType));
		}
	}
	return tempVec;
}

void ObjectMgr::FlushDeletions() {
    std::vector<uint32> toDelete;
    {
        std::unique_lock<std::shared_mutex> lock(m_objMutex);
        toDelete = std::move(m_pendingDeletions);
        m_pendingDeletions.clear();
    }
    
    for (uint32 id : toDelete) {
        destroyObject(id);
    }

    if (!toDelete.empty() && BotManager::getSingletonPtr()) {
        BotManager::getSingleton().PruneDeadBots();
    }
}
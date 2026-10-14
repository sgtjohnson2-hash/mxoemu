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
	for (auto& cPair : m_goToViews)
	{
		auto gIt = cPair.second.find(goId);
		if (gIt != cPair.second.end())
		{
			uint16 viewId = gIt->second;
			cPair.second.erase(gIt);
			auto vIt = m_views.find(cPair.first);
			if (vIt != m_views.end())
				vIt->second.erase(viewId);
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
	if (forWhichObj == NULL)
		return 0;

	return forWhichObj->getGoId();
}

uint16 ObjectMgr::getViewForGO( GameClient* requester, uint32 goId )
{
	if (requester == NULL)
		throw ClientNotAvailable();

	// The client's own avatar is registered internally as VIEWID_SELF (2).
	// ILTCommHandleMgr::ResolveHandle (0x622CEDA0) queries ObjectManager::FindObjectByViewId
	// with the low 16 bits of the slot handle. If the local player's view ID is not VIEWID_SELF,
	// ResolveHandle returns NULL, and the interlock exchange scheduler bails at 0x625F72E7.
	if (!requester->isBot() && requester->GetPlayerGoId() != 0 && goId == requester->GetPlayerGoId())
		return VIEWID_SELF;

	// Fast path: non-blocking shared read lock + O(1) hash lookup for existing views
	{
		std::shared_lock<std::shared_mutex> readLock(m_objMutex);
		auto cIt = m_goToViews.find(requester);
		if (cIt != m_goToViews.end())
		{
			auto gIt = cIt->second.find(goId);
			if (gIt != cIt->second.end())
				return gIt->second;
		}
	}

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	viewIdsMap &viewsOfClient = m_views[requester];
	goToViewMap &goToView = m_goToViews[requester];

	auto gIt = goToView.find(goId);
	if (gIt != goToView.end())
		return gIt->second;

	// client.dll ConnectionContext viewListSize is 128 (0x80 elements).
	// View 0 = unassigned, 1 = ObjectManager, 2 = Self.
	// Dynamic view IDs must strictly be in [3..127]. Any viewId >= 128 triggers
	// "HandleInput, view ID is out of range!" at 0x622C383E and is dropped by client.dll.
	for (uint16 id = 3; id < 128; ++id)
	{
		if (viewsOfClient.find(id) == viewsOfClient.end())
		{
			viewsOfClient[id] = goId;
			goToView[goId] = id;
			return id;
		}
	}

	ERROR_LOG(format("ObjectMgr::getViewForGO: client %1% exhausted view table (max 125 views) for goId %2%")
		% requester->Address() % goId);
	throw NoMoreFreeViews();
}

void ObjectMgr::releaseRelevantSet( GameClient *requester )
{
	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	m_views.erase(requester);
	m_goToViews.erase(requester);
}

uint16 ObjectMgr::allocateViewIdLocked( GameClient* requester )
{
	if (requester == NULL)
		throw ClientNotAvailable();

	const viewIdsMap &viewsOfClient = m_views[requester];
	// client.dll viewListSize is 128; allocate in [3..127]
	for (uint16 id = 3; id < 128; ++id)
	{
		if (viewsOfClient.find(id) == viewsOfClient.end())
			return id;
	}

	throw NoMoreFreeViews();
}

uint16 ObjectMgr::allocateViewId( GameClient* requester )
{
	if (requester == NULL)
		throw ClientNotAvailable();

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	return allocateViewIdLocked(requester);
}

uint32 ObjectMgr::getGOForView( class GameClient *requester, uint16 viewId )
{
	if (requester == NULL)
		throw ClientNotAvailable();

	if (!requester->isBot() && viewId == VIEWID_SELF)
		return requester->GetPlayerGoId();

	std::shared_lock<std::shared_mutex> lock(m_objMutex);
	auto it = m_views.find(requester);
	if (it != m_views.end())
	{
		auto vit = it->second.find(viewId);
		if (vit != it->second.end())
			return vit->second;
	}

	return 0;
}

uint16 ObjectMgr::allocateDynamicView( class GameClient *requester, uint32 tagObjId )
{
	if (requester == NULL)
		throw ClientNotAvailable();

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	viewIdsMap &viewsOfClient = m_views[requester];
	for (uint16 id = 3; id < 128; ++id)
	{
		if (viewsOfClient.find(id) == viewsOfClient.end())
		{
			viewsOfClient[id] = tagObjId;
			if (tagObjId != 0)
				m_goToViews[requester][tagObjId] = id;
			return id;
		}
	}

	throw NoMoreFreeViews();
}

void ObjectMgr::releaseDynamicView( class GameClient *requester, uint16 viewId )
{
	if (requester == NULL)
		return;

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	clientToViewMap::iterator it = m_views.find(requester);
	if (it != m_views.end())
	{
		auto vit = it->second.find(viewId);
		if (vit != it->second.end())
		{
			uint32 tagObjId = vit->second;
			it->second.erase(vit);
			auto gClientIt = m_goToViews.find(requester);
			if (gClientIt != m_goToViews.end())
			{
				auto gIt = gClientIt->second.find(tagObjId);
				if (gIt != gClientIt->second.end() && gIt->second == viewId)
					gClientIt->second.erase(gIt);
			}
		}
	}
}


#include "GameServer.h"

void ObjectMgr::RandomObject( uint32 randomObjectId, GameClient* requester, double X, double Y, double Z, double ROT)
{
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

void ObjectMgr::EnsureDoorsLoaded()
{
	if (m_doorsLoaded.load(std::memory_order_acquire))
		return;

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	if (m_doorsLoaded.load(std::memory_order_relaxed))
		return;

	m_doorsLoaded.store(true, std::memory_order_release);
	if (Database_Main == nullptr)
		return;

	try
	{
		scoped_ptr<QueryResult> res(sDatabase.Query("SELECT `DoorId`, `DistrictId`, `X`, `Y`, `Z`, `ROT`, `DoorType` FROM `doors`"));
		if (res)
		{
			do
			{
				Field* f = res->Fetch();
				CachedDoorInfo info;
				info.doorId = f[0].GetUInt32();
				info.districtId = f[1].GetUInt16();
				info.x = f[2].GetDouble();
				info.y = f[3].GetDouble();
				info.z = f[4].GetDouble();
				info.rot = f[5].GetDouble();
				info.doorType = f[6].GetUInt16();
				m_doorCache[info.doorId] = info;
			} while (res->NextRow());
		}
	}
	catch (...) {}
}

bool ObjectMgr::HasDoor(uint16 districtId, uint32 doorId)
{
	EnsureDoorsLoaded();
	std::shared_lock<std::shared_mutex> lock(m_objMutex);
	auto it = m_doorCache.find(doorId);
	return (it != m_doorCache.end() && (it->second.districtId == districtId || it->second.districtId == 0));
}

void ObjectMgr::RegisterDoor(uint16 districtId, uint32 doorId, double x, double y, double z, double rot, uint16 doorType)
{
	EnsureDoorsLoaded();
	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	CachedDoorInfo info;
	info.doorId = doorId;
	info.districtId = districtId;
	info.x = x;
	info.y = y;
	info.z = z;
	info.rot = rot;
	info.doorType = doorType;
	m_doorCache[doorId] = info;
}

void ObjectMgr::OpenDoor( uint32 doorId, GameClient* requester)
{
	if (!requester)
		return;

	EnsureDoorsLoaded();

	uint16 viewId = 0;
	CachedDoorInfo doorInfo;
	bool hasDoorInfo = false;
	bool toggledClosed = false;

	{
		std::unique_lock<std::shared_mutex> lock(m_objMutex);
		for (map<uint16,uint32>::iterator it=m_openDoors.begin();it!=m_openDoors.end();++it)
		{
			if (it->second == doorId)
			{
				auto dIt = m_doorCache.find(doorId);
				if (dIt != m_doorCache.end())
					dIt->second.doorType = 0;
				m_openDoors.erase(it);
				toggledClosed = true;
				break;
			}
		}

		if (!toggledClosed)
		{
			if (m_openDoors.size() > 37000)
			{
				m_openDoors.erase(m_openDoors.begin());
			}

			viewId = allocateViewIdLocked(requester);
			m_views[requester][viewId] = doorId;
			m_goToViews[requester][doorId] = viewId;
			m_openDoors[viewId] = doorId;

			auto dIt = m_doorCache.find(doorId);
			if (dIt != m_doorCache.end())
			{
				doorInfo = dIt->second;
				hasDoorInfo = true;
			}
		}
	}

	if (toggledClosed)
	{
		if (Database_Main != nullptr)
		{
			format sqlUpdateDoorType = format("UPDATE `doors` SET `DoorType`='0' WHERE `DoorId`='%1%' LIMIT 1") % doorId;
			sDatabase.Execute(sqlUpdateDoorType);
		}
		format msg1 = format("{c:0FFFF0}Door:0x%08x Set to Indoors since u tried to open it but I think it is open, try again.{/c}") % (int)doorId;
		requester->QueueCommand(make_shared<SystemChatMsg>(msg1.str()));
		return;
	}

	format msg2 = format("{c:0FFFF0}Door:0x%08x{/c}") % (int)doorId;
	requester->QueueCommand(make_shared<SystemChatMsg>(msg2.str()));

	format s = format("Door Opened With ViewId: %1%") % int(viewId);
	DEBUG_LOG(s.str());

	if (hasDoorInfo)
	{
		requester->QueueState(make_shared<DoorAnimationMsg>(doorId, viewId, doorInfo.x, doorInfo.y, doorInfo.z, doorInfo.rot, doorInfo.doorType));
	}
}

vector<msgBaseClassPtr> ObjectMgr::GetAllOpenDoors( GameClient* requester )
{
	vector<msgBaseClassPtr> tempVec;
	if (!requester)
		return tempVec;

	EnsureDoorsLoaded();

	std::unique_lock<std::shared_mutex> lock(m_objMutex);
	viewIdsMap &viewsOfClient = m_views[requester];
	goToViewMap &goToView = m_goToViews[requester];

	for (map<uint16,uint32>::iterator it=m_openDoors.begin();it!=m_openDoors.end();++it)
	{
		viewsOfClient[it->first] = it->second;
		goToView[it->second] = it->first;

		auto dIt = m_doorCache.find(it->second);
		if (dIt != m_doorCache.end())
		{
			const CachedDoorInfo& d = dIt->second;
			tempVec.push_back(make_shared<DoorAnimationMsg>(it->second, it->first, d.x, d.y, d.z, d.rot, d.doorType));
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
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

#ifndef MXOEMU_OBJECTMGR_H
#define MXOEMU_OBJECTMGR_H

#include "Common.h"
#include "ObjectPool.h"
#include "MessageTypes.h"
#include <cstdint>
#include <shared_mutex>
#include <atomic>

const uint32 OBJECTMANAGER_STARTINGOBJECTID = 0x8000; //we have plenty of uint32s

class ObjectMgr
{
public:
	class ClientNotAvailable {};
	class ObjectNotAvailable {};
	class NoMoreFreeViews {};

	ObjectMgr();
	~ObjectMgr();

	uint32 constructPlayer( class GameClient* requester, uint64 charUID, bool isBot = false );
	void destroyObject(uint32 goId);
	class PlayerObject* getGOPtr(uint32 goId);
	class PlayerObject* getGOPtrSafe(uint32 goId); //returns NULL instead of throwing
	std::shared_ptr<class PlayerObject> getGOSharedPtr(uint32 goId);
	uint32 getGOId(class PlayerObject* forWhichObj);
	uint16 getViewForGO(class GameClient *requester, uint32 goId);
	uint32 getGOForView(class GameClient *requester, uint16 viewId); //returns 0 if the view is not a player object
	uint16 allocateDynamicView(class GameClient *requester, uint32 tagObjId); //for spawned non-player GOs (interlock handlers etc)
	void releaseDynamicView(class GameClient *requester, uint16 viewId);
	void releaseRelevantSet(class GameClient *requester);
	vector<uint32> getAllGOIds()
	{
		std::shared_lock<std::shared_mutex> lock(m_objMutex);
		vector<uint32> tempVect;
		for (objectsMap::iterator it=m_objects.begin();it!=m_objects.end();++it)
		{
			if (it->first >= OBJECTMANAGER_STARTINGOBJECTID && it->second != NULL)
				tempVect.push_back(it->first);
		}
		return tempVect;
	}
	void OpenDoor(uint32 doorId, class GameClient *requester);
	vector<msgBaseClassPtr> GetAllOpenDoors(class GameClient *requester);

	void RandomObject( uint32 randomObjectId, GameClient* requester, double X, double Y, double Z, double ROT);

	uint32 getNewObjectId()
	{
		return m_currFreeObjectId.fetch_add(1);
	}

private:
	typedef shared_ptr<PlayerObject> objectPtr;
	typedef map<uint32,objectPtr> objectsMap;
	objectsMap m_objects;
	typedef map<uint16,uint32> viewIdsMap;
	typedef map<class GameClient*,viewIdsMap> clientToViewMap;
	clientToViewMap m_views;

	uint16 allocateViewId(class GameClient* requester);
	std::atomic<uint32> m_currFreeObjectId;

	map<uint16,uint32> m_openDoors;
	
	mutable std::shared_mutex m_objMutex;
	std::unique_ptr<ObjectPool<PlayerObject>> m_playerPool;
	std::vector<uint32> m_pendingDeletions;
public:
    void QueueDeletion(uint32 goId) {
        std::unique_lock<std::shared_mutex> lock(m_objMutex);
        m_pendingDeletions.push_back(goId);
    }
    void FlushDeletions();
};

#endif
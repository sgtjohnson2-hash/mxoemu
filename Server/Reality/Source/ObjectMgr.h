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
#include <mutex>

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

	std::vector<uint32> getHumanPlayerGOIds() const
	{
		std::shared_lock<std::shared_mutex> lock(m_objMutex);
		return m_humanPlayerGoIds;
	}

	void RegisterHumanPlayerGOId(uint32 goId)
	{
		std::unique_lock<std::shared_mutex> lock(m_objMutex);
		if (std::find(m_humanPlayerGoIds.begin(), m_humanPlayerGoIds.end(), goId) == m_humanPlayerGoIds.end()) {
			m_humanPlayerGoIds.push_back(goId);
		}
	}

	void UnregisterHumanPlayerGOId(uint32 goId)
	{
		std::unique_lock<std::shared_mutex> lock(m_objMutex);
		auto it = std::find(m_humanPlayerGoIds.begin(), m_humanPlayerGoIds.end(), goId);
		if (it != m_humanPlayerGoIds.end()) {
			m_humanPlayerGoIds.erase(it);
		}
	}

	template<typename Func>
	void ForEachHumanPlayer(Func&& func)
	{
		std::vector<std::shared_ptr<PlayerObject>> humans;
		{
			std::shared_lock<std::shared_mutex> lock(m_objMutex);
			if (m_humanPlayerGoIds.empty()) return;
			humans.reserve(m_humanPlayerGoIds.size());
			for (uint32 id : m_humanPlayerGoIds)
			{
				auto it = m_objects.find(id);
				if (it != m_objects.end() && it->second)
					humans.push_back(it->second);
			}
		}
		for (auto& p : humans)
		{
			if (p) {
				func(p.get());
			}
		}
	}

	template<typename Func>
	void ForEachGO(Func&& func)
	{
		std::vector<PlayerObject*> objects;
		{
			std::shared_lock<std::shared_mutex> lock(m_objMutex);
			if (m_objects.empty()) return;
			objects.reserve(m_objects.size());
			for (auto& pair : m_objects)
			{
				if (pair.first >= OBJECTMANAGER_STARTINGOBJECTID && pair.second)
					objects.push_back(pair.second.get());
			}
		}
		for (auto* p : objects)
		{
			if (p) {
				func(p);
			}
		}
	}
	void OpenDoor(uint32 doorId, class GameClient *requester);
	vector<msgBaseClassPtr> GetAllOpenDoors(class GameClient *requester);

	void RandomObject( uint32 randomObjectId, GameClient* requester, double X, double Y, double Z, double ROT);

	// World objects (players and bots) double as client view ids, which are 16-bit on the
	// wire (getViewForGO returns uint16(goId)). A plain counter passes 0xFFFF after a few hours
	// of bot churn and then wraps into other objects' views, so the client rejects the updates
	// ("HandleInput, view ID is out of range / view not found") and bots appear frozen.
	// Recycle ids in [0x8000, 0xFFF0], skipping live objects and ids freed in the last 60 s.
	uint32 getNewObjectId();
	// Items never become client views; keep them out of the view id range.
	uint32 getNewItemId() { return m_nextItemId.fetch_add(1); }

private:
	typedef shared_ptr<PlayerObject> objectPtr;
	typedef map<uint32,objectPtr> objectsMap;
	objectsMap m_objects;
	typedef map<uint16,uint32> viewIdsMap;
	typedef map<class GameClient*,viewIdsMap> clientToViewMap;
	clientToViewMap m_views;

	uint16 allocateViewId(class GameClient* requester);
	std::atomic<uint32> m_currFreeObjectId;
	std::atomic<uint32> m_nextItemId{0x01000000};
	std::mutex m_idAllocMutex;
	map<uint32,uint32> m_recentlyFreedIds; // goId -> getMSTime() when freed

	map<uint16,uint32> m_openDoors;
	
	mutable std::shared_mutex m_objMutex;
	std::unique_ptr<ObjectPool<PlayerObject>> m_playerPool;
	std::vector<uint32> m_pendingDeletions;
	std::vector<uint32> m_humanPlayerGoIds;
public:
    void QueueDeletion(uint32 goId) {
        std::unique_lock<std::shared_mutex> lock(m_objMutex);
        m_pendingDeletions.push_back(goId);
    }
    void FlushDeletions();
};

#endif
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
#include "GameSocket.h"
#include "GameServer.h"
#include "GameClient.h"
#include "ObjectMgr.h"

#include "GameClient.h"
#include "SpatialGrid.h"
#include "Timer.h"
#include "Database/Database.h"
#include "Database/PreparedStatement.h"

#include <Sockets/Ipv4Address.h>

GameSocket::GameSocket( ISocketHandler& theHandler ) : UdpSocket(theHandler)
{
	m_lastCleanupTime = getTime();
	// set player count to 0
	{
		PreparedStatement stmt("UPDATE `worlds` SET `numPlayers`='0' WHERE `name`=?0 OR `worldId`=1 LIMIT 1");
		stmt.SetString(0, "LocalHost");
		sDatabase.ExecutePrepared(&stmt);
		m_lastPlayerCount = 0;
	}

}

GameSocket::~GameSocket()
{

}

size_t GameSocket::Clients_Connected(void) const
{
	std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
	return m_clients.size();
}

void GameSocket::OnRawData( const char *pData,size_t len,struct sockaddr *sa_from,socklen_t sa_len )
{
	struct sockaddr_in inc_addr;
	memcpy(&inc_addr,sa_from,sa_len);
	Ipv4Address theAddr(inc_addr);

	if (theAddr.IsValid() == false)
		return;

	string IPStr = theAddr.Convert(true);
	std::shared_ptr<GameClient> targetClient;
	{
		std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
		GClientList::iterator it = m_clients.find(IPStr);
		if (it != m_clients.end())
		{
			std::shared_ptr<GameClient> Client = it->second;
			if (Client->IsValid() == false)
			{
				DEBUG_LOG( format("Removing dead client [%1%]") % IPStr );
				m_clients.erase(it);
				targetClient = std::make_shared<GameClient>(inc_addr, this);
				m_clients[IPStr] = targetClient;
			}
			else
			{
				targetClient = Client;
			}
		}
		else
		{
			targetClient = std::make_shared<GameClient>(inc_addr, this);
			m_clients[IPStr] = targetClient;
			DEBUG_LOG(format ("Client connected [%1%], now have [%2%] clients")
				% IPStr % m_clients.size());
		}
	}

	if (targetClient)
	{
		targetClient->HandlePacket(pData, len);
	}
}

void GameSocket::PruneDeadClients()
{
	m_currTime = getTime();

	// Do client cleanup
	{
		std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
		for (GClientList::iterator it=m_clients.begin();it!=m_clients.end();)
		{
			std::shared_ptr<GameClient> Client = it->second;
			if (!Client || !Client->IsValid() || (m_currTime - Client->LastActive()) >= 20)
			{
				if (!Client)
				{
					m_clients.erase(it++);
					continue;
				}

				if (!Client->IsValid())
					DEBUG_LOG( format("Removing invalidated client [%1%]") % Client->Address() );
				else
					DEBUG_LOG( format("Removing client due to time-out [%1%]") % Client->Address() );

				m_clients.erase(it++);
			}
			else
			{
				++it;
			}
		}
	}

	if ((m_currTime - m_lastCleanupTime) >= 5)
	{
		// Update player count
		size_t connectedCount = this->Clients_Connected();
		if (m_lastPlayerCount != connectedCount)
		{
			PreparedStatement stmt("UPDATE `worlds` SET `numPlayers`=?0 WHERE `name`=?1 OR `worldId`=1 LIMIT 1");
			stmt.SetUInt32(0, (uint32)connectedCount);
			stmt.SetString(1, "LocalHost");
			sDatabase.ExecutePrepared(&stmt);

			m_lastPlayerCount = connectedCount;
		}

		m_lastCleanupTime = m_currTime;
	}
}

void GameSocket::RemoveCharacter(string IPAddr)
{
	std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
	GClientList::iterator it = m_clients.find(IPAddr);
	if (it != m_clients.end())
	{
		std::shared_ptr<GameClient> Client = it->second;
		DEBUG_LOG( format("Removing XXX dead client [%1%]") % IPAddr );
		m_clients.erase(it);
	}

}

std::shared_ptr<GameClient> GameSocket::GetClientWithSessionId( uint32 sessionId )
{
	std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
	for (GClientList::iterator it=m_clients.begin();it!=m_clients.end();++it)
	{
		if (it->second->GetSessionId() == sessionId)
		{
			return it->second;
		}
	}
	return nullptr;
}

vector<std::shared_ptr<GameClient>> GameSocket::GetClientsWithCharacterId( uint64 charId )
{
	vector<std::shared_ptr<GameClient>> returns;
	std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
	for (GClientList::iterator it=m_clients.begin();it!=m_clients.end();++it)
	{
		if (it->second->GetCharacterId() == charId)
		{
			returns.push_back(it->second);
		}
	}
	return returns;
}

void GameSocket::CheckAndResend()
{
	std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
	for (GClientList::iterator it=m_clients.begin();it!=m_clients.end();++it)
	{
		it->second->CheckAndResend();
	}
}

void GameSocket::Broadcast( const ByteBuffer &message, bool command )
{
	if (command)
		return AnnounceCommand(NULL,make_shared<StaticMsg>(message));
	else
		return AnnounceStateUpdate(NULL,make_shared<StaticMsg>(message),true);
}

void GameSocket::BroadcastNear(float x, float z, float radius, const ByteBuffer &message, bool command)
{
	if (command)
		return AnnounceCommandNear(x, z, radius, make_shared<StaticMsg>(message));
	else
		return AnnounceStateUpdateNear(x, z, radius, make_shared<StaticMsg>(message), true);
}

void GameSocket::AnnounceStateUpdate( GameClient* clFrom, msgBaseClassPtr theMsg, bool immediateOnly, GameClient::packetAckFunc callFunc )
{
    if (clFrom)
    {
        auto localClients = sSpatialGrid.GetClientsNearClient(clFrom);
        for (GameClient* client : localClients)
        {
            if (client != clFrom)
            {
                client->QueueState(theMsg, immediateOnly, callFunc);
            }
        }
    }
    else
    {
        // Global Broadcast (e.g. system messages)
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        for (GClientList::iterator it=m_clients.begin();it!=m_clients.end();++it)
        {
            it->second->QueueState(theMsg,immediateOnly,callFunc);
        }
    }
}

void GameSocket::AnnounceStateUpdateNear(float x, float z, float radius, msgBaseClassPtr theMsg, bool immediateOnly, GameClient::packetAckFunc callFunc)
{
    auto localClients = sSpatialGrid.GetClientsInRadius(x, z, radius);
    for (GameClient* client : localClients)
    {
        client->QueueState(theMsg, immediateOnly, callFunc);
    }
}

void GameSocket::AnnounceCommand( GameClient* clFrom,msgBaseClassPtr theCmd, GameClient::packetAckFunc callFunc )
{
    if (clFrom)
    {
        auto localClients = sSpatialGrid.GetClientsNearClient(clFrom);
        for (GameClient* client : localClients)
        {
            if (client != clFrom)
            {
                client->QueueCommand(theCmd, callFunc);
            }
        }
    }
    else
    {
        // Global Broadcast
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        for (GClientList::iterator it=m_clients.begin();it!=m_clients.end();++it)
        {
            it->second->QueueCommand(theCmd,callFunc);
        }
    }
}

void GameSocket::AnnounceCommandNear(float x, float z, float radius, msgBaseClassPtr theCmd, GameClient::packetAckFunc callFunc)
{
    auto localClients = sSpatialGrid.GetClientsInRadius(x, z, radius);
    for (GameClient* client : localClients)
    {
        client->QueueCommand(theCmd, callFunc);
    }
}

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
#include <future>
#include "GameServer.h"
#include "GameClient.h"
#include "MarginServer.h"
#include "LootManager.h"
#include "Log.h"
#include "Timer.h"
#include "Config.h"
#include "Util.h"
#include <mysql/mysql.h>
#include "GameSocket.h"
#include "Database/DatabaseEnv.h"
#include <Sockets/Ipv4Address.h>
#include "DataLoader.h"
#include "StaticObjectManager.h"
#include "CombatSystem.h"
#include "BotManager.h"
#include "MissionSystem.h"
#include "CrewSystem.h"
#include "WeatherSystem.h"
#include <mutex>
#include <future>
#include "VehicleSystem.h"
#include "EconomySystem.h"
#include "CraftingSystem.h"
#include "FactionWarManager.h"
#include "AdaptiveMusicSystem.h"
#include "NavMeshMgr.h"
#include "SpatialGrid.h"
#include "SocketSystem.h"
#include "Database/AsyncDatabase.h"
#include "StatusEffectManager.h"
#include "HackerSystem.h"
#include "PlayerObject.h"
#include "HovercraftSystem.h"
#include "AI/PedestrianEcology.h"
#include "AI/MatrixThreatHeatmap.h"
#include "LogisticsManager.h"
#include "WorldDirector.h"
#include "AI/SensoryPerceptionSystem.h"
#include "AI/SentientMajorCharacters.h"
#include "AI/CoverSystem.h"
#include "RadioDispatchSystem.h"
#include "SmithVirusCascade.h"
// removed duplicate include
#include <boost/bind.hpp>

initialiseSingleton( GameServer );

bool GameServer::Start()
{
	m_simtimeStart = getFloatTime();
	m_simtimeOffset = 0;

	// Load static data templates asynchronously for faster boot
    auto f1 = std::async(std::launch::async, [](){ sDataLoader.LoadAll("Data/hd_dump/"); });
    auto f2 = std::async(std::launch::async, [](){ sDataLoader.LoadMissions("Data/hd_dump/missions/"); });
    auto f4 = std::async(std::launch::async, [](){ sLootMgr.LoadLootTables("Data/Loot/loot_tables.csv"); });

    PlayerObject::LoadHardlines();
    sBotMgr.LoadHardlines(); // Phase 9 cache
	sSpatialGrid.Initialize(15000.0f); //world units: 150m cells, 3x3 query = up to 450m relevance
    
    // Wait for NPC data before populating the world (DataLoader mutates maps)
    f1.wait();
    
    // Now that DataLoader has finished loading blueprints, it's safe for CraftSys to copy them
    sCraftSys.LoadBlueprints();

	sStaticObjMgr.Initialize();
	sNavMeshMgr.Initialize();
    sBotMgr.PopulateWorld();
    sBotMgr.BotStressTest(500); // Automatically spawn 500 bots on startup
    
    // Wait for the rest
    f2.wait();
    f4.wait();
	sPedestrianEcology.Initialize();
	sMatrixThreatHeatmap.Initialize();
	sFactionWarMgr.initialize();
	sLogisticsMgr.Initialize();
	sWorldDirector.Initialize();
	sAdaptiveMusicSystem.initialize();
	sVehicleSys.Initialize();
	
	sSocketSystem.initialize();
	sAsyncDatabase.Initialize();
	sStatusEffectManager.Initialize();
	sHackerSystem.Initialize();
	sSensoryPerception.Initialize();
	sSentientCharacters.Initialize();
	sCoverSystem.Initialize();
	sRadioDispatchSystem.Initialize();
	sSmithCascade.Initialize();

	string Interface = sConfig.GetStringDefault("GameServer.IP", "0.0.0.0");
	int Port = sConfig.GetIntDefault("GameServer.Port", 10000);
	INFO_LOG(format("Starting Game server on port %1%") % Port);

	m_mainSocket.reset(new GameSocket(m_udpHandler));
	port_t thePortToBind = Port;
	if (m_mainSocket->Bind(Interface,thePortToBind) != 0)
	{
		ERROR_LOG(format("Error binding Game Server to port %1%") % thePortToBind);
		return false;
	}
	m_udpHandler.Add(m_mainSocket.get());

	m_serverStartMS = getMSTime();

	// Mark server as "up"
	{
		sDatabase.WaitExecute(format("UPDATE `worlds` SET `status`='1' WHERE `name`='%1%' LIMIT 1")
			% this->GetName() );
		m_serverUp=true;
	}

	m_runSimulation = true;
	m_simulationThread = std::thread(&GameServer::SimulationLoop, this);

	return true;
}

void GameServer::Stop()
{
	m_mainSocket.reset();
	// Mark server as "down"
	{
		sDatabase.WaitExecute(format("UPDATE `worlds` SET `status`='0' WHERE `name`='%1%' LIMIT 1")
			% this->GetName() );
		m_serverUp = false;
	}
	
	m_runSimulation = false;
	if (m_simulationThread.joinable()) {
		m_simulationThread.join();
	}
	
	sAsyncDatabase.Shutdown(); // Flush remaining DB writes
	
	INFO_LOG("Game Server shutdown");
}

void GameServer::Loop(void)
{
	if (m_mainSocket == NULL)
		return;

	m_mainSocket->PruneDeadClients();
	m_mainSocket->CheckAndResend();
	// Simulation is now decoupled from the main network loop
	m_udpHandler.Select(0,4000); //4ms
}

void GameServer::SimulationLoop()
{
    INFO_LOG("DEBUG_TRACER: SimulationLoop started");
	uint32 m_lastSimMs = getMSTime();
	while (m_runSimulation)
	{
		try {
			// Tick Combat and AI at 30Hz (~33ms)
			uint32 currentMs = getMSTime();
			uint32 aiDeltaMs = currentMs - m_lastSimMs;
			m_lastSimMs = currentMs;
				
			try { sCombatSys.Update(); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sCombatSys caught %1%") % e.what()); }
			try { sBotMgr.Update(); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sBotMgr caught %1%") % e.what()); }
			try { sFactionWarMgr.update(aiDeltaMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sFactionWarMgr caught %1%") % e.what()); }
			try { sAdaptiveMusicSystem.update(currentMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sAdaptiveMusicSystem caught %1%") % e.what()); }
			try { sWeatherSys.Update(currentMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sWeatherSys caught %1%") % e.what()); }
			try { sVehicleSys.Tick(currentMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sVehicleSys caught %1%") % e.what()); }
			try { sStatusEffectManager.Update(aiDeltaMs / 1000.0f); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sStatusEffectManager caught %1%") % e.what()); }
			try { sMissionSys.Update(aiDeltaMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sMissionSys caught %1%") % e.what()); }
			try { sLogisticsMgr.Update(aiDeltaMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sLogisticsMgr caught %1%") % e.what()); }
			try { sWorldDirector.Update(aiDeltaMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sWorldDirector caught %1%") % e.what()); }
			try { sSensoryPerception.Update(aiDeltaMs / 1000.0f, currentMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sSensoryPerception caught %1%") % e.what()); }
			try { sSentientCharacters.Update(aiDeltaMs / 1000.0f); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sSentientCharacters caught %1%") % e.what()); }
			try { sRadioDispatchSystem.Update(aiDeltaMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sRadioDispatchSystem caught %1%") % e.what()); }
			try { sSmithCascade.Update(aiDeltaMs); } catch (const std::exception& e) { ERROR_LOG(format("SimulationLoop: sSmithCascade caught %1%") % e.what()); }

			static uint32 lastMetricLogMs = 0;
			static uint32 tickCount = 0;
			tickCount++;
			if (currentMs - lastMetricLogMs >= 5000) {
				uint32 elapsed = currentMs - lastMetricLogMs;
				lastMetricLogMs = currentMs;
				float tps = (tickCount * 1000.0f) / (elapsed > 0 ? elapsed : 1);
				size_t botCnt = 0;
				try { botCnt = sBotMgr.GetBotCount(); } catch (...) {}
				INFO_LOG(format("SimulationLoop: Tick %1% | TPS: %2% | Active Bots: %3% | Hardlines: %4%")
						 % tickCount % tps % botCnt % sBotMgr.GetHardlines().size());
				tickCount = 0;
			}

			// The Anomaly Event (Phase 50)
			static uint32 lastAnomalyCheckMs = 0;
			if (currentMs - lastAnomalyCheckMs > 3600000) { // Every 1 hour
				lastAnomalyCheckMs = currentMs;
				if (rand() % 100 < 5) { // 5% chance
					auto allIds = sObjMgr.getAllGOIds();
					std::vector<uint32> validPlayers;
					for (uint32 id : allIds) {
						if (auto p = sObjMgr.getGOPtrSafe(id)) {
							if (!p->getClient().isBot() && !p->isDead()) validPlayers.push_back(id);
						}
					}
					if (!validPlayers.empty()) {
						uint32 chosenId = validPlayers[rand() % validPlayers.size()];
						sStatusEffectManager.ApplyEffect(chosenId, EFFECT_THE_ANOMALY, 60.0f, 1.0f, 0.0f);
						for (uint32 id : allIds) {
							if (auto p = sObjMgr.getGOPtrSafe(id)) {
								if (!p->getClient().isBot()) {
									p->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:00FFFF}[System] THE ANOMALY HAS MANIFESTED IN THE MEGA CITY.{/c}"));
								}
							}
						}
						INFO_LOG(format("The Anomaly has been granted to player ID %1%.") % chosenId);
					}
				}
			}

			auto allIds = sObjMgr.getAllGOIds();
			for (uint32 id : allIds) {
				if (auto po = sObjMgr.getGOPtrSafe(id)) {
					if (!po->getClient().isBot()) {
						po->Update();
						
						// Item 55: Network Packet Batching
						// Ensure all server-generated events from Combat, AI, and World are flushed out this tick
						po->getClient().FlushQueue();
					}
				}
			}

			// Item 54: Flush pending lazy deletions from Garbage Collector
			sObjMgr.FlushDeletions();

		} catch (const std::exception& e) {
			ERROR_LOG(format("SimulationLoop caught std::exception: %1%") % e.what());
		} catch (...) {
			ERROR_LOG("SimulationLoop caught unknown exception!");
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(33));
	}
}

std::shared_ptr<GameClient> GameServer::GetClientWithSessionId( uint32 sessionId )
{
	return m_mainSocket->GetClientWithSessionId(sessionId);
}

vector<std::shared_ptr<GameClient>> GameServer::GetClientsWithCharacterId( uint64 charId )
{
	return m_mainSocket->GetClientsWithCharacterId(charId);
}

void GameServer::Broadcast( const ByteBuffer &message, bool command )
{
	if (m_mainSocket != NULL)
	{
		m_mainSocket->Broadcast(message, command);
	}
}

void GameServer::BroadcastNear(float x, float z, float radius, const ByteBuffer &message, bool command)
{
    if (m_mainSocket != NULL) m_mainSocket->BroadcastNear(x, z, radius, message, command);
}

void GameServer::AnnounceStateUpdate( GameClient* clFrom,msgBaseClassPtr theMsg, bool immediateOnly )
{
	if (m_mainSocket != NULL)
	{
		m_mainSocket->AnnounceStateUpdate(clFrom,theMsg,immediateOnly);
	}
}

void GameServer::AnnounceStateUpdateNear(float x, float z, float radius, msgBaseClassPtr theMsg, bool immediateOnly)
{
    if (m_mainSocket != NULL) m_mainSocket->AnnounceStateUpdateNear(x, z, radius, theMsg, immediateOnly);
}

void GameServer::AnnounceCommand( GameClient* clFrom,msgBaseClassPtr theCmd )
{
	if (m_mainSocket != NULL)
	{
		m_mainSocket->AnnounceCommand(clFrom,theCmd);
	}
}

void GameServer::AnnounceCommandNear(float x, float z, float radius, msgBaseClassPtr theCmd)
{
    if (m_mainSocket != NULL) m_mainSocket->AnnounceCommandNear(x, z, radius, theCmd);
}

string GameServer::GetName() const
{
	return sConfig.GetStringDefault("GameServer.WorldName", "Reality");
}

string GameServer::GetChatPrefix() const
{
	return sConfig.GetStringDefault("GameServer.ChatPrefix", "SOE+MXO") + string("+") + GetName();
}




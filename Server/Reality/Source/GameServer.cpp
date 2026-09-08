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
#include "FrankCastleManager.h"
#include "UnderworldManager.h"
#include "CityLifeManager.h"
#include "EmergentPoliceManager.h"
#include "MafiaEcosystemManager.h"
#include "ExileChateauManager.h"
#include "NeuralSwarmManager.h"
#include "StructuralVoxelEngine.h"
#include "SharedMemoryShardFabric.h"
#include "NeuromorphicSpikeEngine.h"
#include "NonEuclideanPortalEngine.h"
#include "SourceCodeTelekinesisEngine.h"
#include "GlobalSovereignMeshFabric.h"
#include "GaussianSplatEngine.h"
#include "PhysarumLogisticsEngine.h"
#include "BiometricResonanceEngine.h"
#include "WebAssemblyGatewayEngine.h"
#include "WorldRealizationEngine.h"
#include "CastleAgentCombatEngine.h"
#include "CastlePvPKarmaEngine.h"
#include "CastleUnderworldAssaultEngine.h"
#include "AirspaceAndConvoyEngine.h"
#include "NeuroevolutionaryCombatEngine.h"
#include "MachineCitySystem.h"
#include "QuantumSuperpositionEngine.h"
#include "GenerationalLineageEngine.h"
#include "SubAtomicMatrixGrid.h"
#include "CosmicVerticalityEngine.h"
#include "CollectiveConsciousnessEngine.h"
#include "MegacityBourseEngine.h"
#include "TemporalAnomalyEngine.h"
#include "ParallelMatrixEngine.h"
#include "QuantumEntangledMeshEngine.h"
#include "SourceVoxelSynthesisEngine.h"
#include "DeepCoreMeltdownEngine.h"
#include "ArchitectSandboxEngine.h"
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

    sBotMgr.PopulateWorld();
    sBotMgr.BotStressTest(500); // Automatically spawn 500 bots on startup
    
    // Wait for the rest
    f2.wait();
    f4.wait();
	sFactionWarMgr.initialize();
	sAdaptiveMusicSystem.initialize();
	sVehicleSys.Initialize();
	
	sNavMeshMgr.Initialize();
	sSocketSystem.initialize();
	sAsyncDatabase.Initialize();
	sStatusEffectManager.Initialize();
	sHackerSystem.Initialize();

	// Initialize Megacity Tactical & Emergent Simulation Engines
	sFrankCastleMgr.Initialize();
	sUnderworldMgr.Initialize();
	sCityLifeMgr.Initialize();
	sEmergentPoliceMgr.Initialize();
	sMafiaMgr.Initialize();
	sExileMgr.Initialize();
	sNeuralSwarmMgr.Initialize();
	sStructuralVoxelEngine.Initialize();
	sSharedMemoryShardFabric.Initialize();
	sNeuromorphicSpikeEngine.Initialize();
	sNonEuclideanPortalEngine.Initialize();
	sSourceTelekinesisEngine.Initialize();
	sGlobalSovereignMesh.Initialize();
	sGaussianSplatEngine.Initialize();
	sPhysarumLogisticsEngine.Initialize();
	sBiometricResonanceEngine.Initialize();
	sWebAssemblyGatewayEngine.Initialize();
	sWorldRealizationEngine.Initialize();
	sCastleAgentCombatEngine.Initialize();
	sCastlePvPKarmaEngine.Initialize();
	sCastleUnderworldAssaultEngine.Initialize();
	sAirspaceAndConvoyEngine.Initialize();
	sNeuroevolutionaryCombatEngine.Initialize();
	sMachineCitySystem.Initialize();
	sQuantumSuperpositionEngine.Initialize();
	sGenerationalLineageEngine.Initialize();
	sSubAtomicMatrixGrid.Initialize();
	sCosmicVerticalityEngine.Initialize();
	sCollectiveConsciousnessEngine.Initialize();
	sMegacityBourseEngine.Initialize();
	sTemporalAnomalyEngine.Initialize();
	sParallelMatrixEngine.Initialize();
	sQuantumEntangledMeshEngine.Initialize();
	sSourceVoxelSynthesisEngine.Initialize();
	sDeepCoreMeltdownEngine.Initialize();
	sArchitectSandboxEngine.Initialize();

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
				
			// Item 53 & 59: Multi-Threaded Simulation Loop with Thread Affinity
			auto combatFuture = std::async(std::launch::async, []() { 
#if PLATFORM == PLATFORM_WIN32
				SetThreadAffinityMask(GetCurrentThread(), 1 << 1); // Pin to Core 1
#endif
				sCombatSys.Update(); 
			});
			auto aiFuture = std::async(std::launch::async, []() { 
#if PLATFORM == PLATFORM_WIN32
				SetThreadAffinityMask(GetCurrentThread(), 1 << 2); // Pin to Core 2
#endif
				sBotMgr.Update(); 
			});

			sFactionWarMgr.update(aiDeltaMs);
			sAdaptiveMusicSystem.update(currentMs);
			sWeatherSys.Update(currentMs);
			sVehicleSys.Tick(currentMs);
			sStatusEffectManager.Update(aiDeltaMs / 1000.0f);
			sMissionSys.Update(aiDeltaMs);
			
			combatFuture.wait();
			aiFuture.wait();

			// Megacity Tactical & Emergent Simulation Engines Tick
			float dtSec = aiDeltaMs / 1000.0f;
			sFrankCastleMgr.Update(aiDeltaMs);
			sUnderworldMgr.Update(aiDeltaMs);
			sCityLifeMgr.Update(aiDeltaMs);
			sEmergentPoliceMgr.Update(aiDeltaMs);
			sMafiaMgr.Update(aiDeltaMs);
			sExileMgr.Update(aiDeltaMs);
			sNeuralSwarmMgr.Update(dtSec);
			sStructuralVoxelEngine.Update(dtSec);
			sSharedMemoryShardFabric.Update(dtSec);
			sNonEuclideanPortalEngine.Update(dtSec);
			sSourceTelekinesisEngine.Update(dtSec);
			sGlobalSovereignMesh.Update(dtSec);
			sGaussianSplatEngine.Update(dtSec);
			sPhysarumLogisticsEngine.Update(dtSec);
			sBiometricResonanceEngine.Update(dtSec);
			sWebAssemblyGatewayEngine.Update(dtSec);
			sWorldRealizationEngine.Update(dtSec);
			sCastleAgentCombatEngine.Update(dtSec);
			sCastlePvPKarmaEngine.Update(dtSec);
			sCastleUnderworldAssaultEngine.Update(dtSec);
			sAirspaceAndConvoyEngine.Update(dtSec);
			sNeuroevolutionaryCombatEngine.Update(dtSec);
			sMachineCitySystem.Update(dtSec);
			sQuantumSuperpositionEngine.Update(dtSec);
			sGenerationalLineageEngine.Update(dtSec);
			sSubAtomicMatrixGrid.Update(dtSec);
			sCosmicVerticalityEngine.Update(dtSec);
			sCollectiveConsciousnessEngine.Update(dtSec);
			sMegacityBourseEngine.Update(dtSec);
			sTemporalAnomalyEngine.Update(dtSec);
			sParallelMatrixEngine.Update(dtSec);
			sQuantumEntangledMeshEngine.Update(dtSec);
			sSourceVoxelSynthesisEngine.Update(dtSec);
			sDeepCoreMeltdownEngine.Update(dtSec);
			sArchitectSandboxEngine.Update(dtSec);

			// The Anomaly Event (Phase 50)
			static uint32 lastAnomalyCheckMs = 0;
			if (currentMs - lastAnomalyCheckMs > 3600000) { // Every 1 hour
				lastAnomalyCheckMs = currentMs;
				if (rand() % 100 < 5) { // 5% chance
					std::vector<uint32> validPlayers;
					sObjMgr.ForEachHumanPlayer([&](PlayerObject* p) {
						if (!p->isDead()) validPlayers.push_back(sObjMgr.getGOId(p));
					});
					if (!validPlayers.empty()) {
						uint32 chosenId = validPlayers[rand() % validPlayers.size()];
						sStatusEffectManager.ApplyEffect(chosenId, EFFECT_THE_ANOMALY, 60.0f, 1.0f, 0.0f);
						sObjMgr.ForEachHumanPlayer([&](PlayerObject* p) {
							p->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:00FFFF}[System] THE ANOMALY HAS MANIFESTED IN THE MEGA CITY.{/c}"));
						});
						INFO_LOG(format("The Anomaly has been granted to player ID %1%.") % chosenId);
					}
				}
			}

			// Zero-allocation thread-safe object update & network packet batching
			sObjMgr.ForEachGO([](PlayerObject* po) {
				po->Update();
				po->getClient().FlushQueue();
			});

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




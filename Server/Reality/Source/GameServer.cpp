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
#if defined(__linux__)
#include <malloc.h>
#endif
#include <future>
#include "Threading/TaskScheduler.h"
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
#include "MachineCitySystem.h"
#include "AgentPossessionManager.h"
#include "OrganizationManager.h"
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
    sCombatSys.LoadAbilities(); // Synthesize moves for all retail abilities loaded by DataLoader

    sBotMgr.PopulateWorld();
    sBotMgr.SpawnTesters(); // Bots.Testers player-simulating QA bots (BotTester.cpp)
    if (sConfig.GetBoolDefault("GameServer.EnableStartupStressTest", false)) {
        sBotMgr.BotStressTest(500); // Only spawn stress test bots if explicitly enabled
    }
    
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
	sMachineCitySystem.Initialize();
	sAgentPossessionMgr.Initialize();
	sOrgMgr.loadFromDB();

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

	// Initialize persistent worker pool (zero runtime allocations)
	sTaskScheduler.Initialize();

	// Mark server as "up"
	{
		sDatabase.WaitExecute(format("UPDATE `worlds` SET `status`='1' WHERE `name`='%1%' LIMIT 1")
			% this->GetName() );
		m_serverUp=true;
	}

#if defined(__linux__)
	malloc_trim(0);
#endif

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
	
	sTaskScheduler.Shutdown();
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
	// The "living world" simulation engines (underworld, police, faction war squads, Frank
	// Castle, the 10-second macro engines...) only produce chat text and short-lived NPC
	// spawns with nothing behind them. Off by default; World.LegacySimulation = 1 restores them.
	const bool legacySim = sConfig.GetBoolDefault("World.LegacySimulation", false);
	INFO_LOG(format("SimulationLoop: legacy simulation engines %1%") % (legacySim ? "ON" : "OFF"));
	while (m_runSimulation)
	{
		auto frameStartTime = std::chrono::steady_clock::now();
		uint32 currentMs = getMSTime();
		try {
			// Tier 1: High-Frequency (30Hz / 33ms) - Combat, Player State & Network Queue
			uint32 aiDeltaMs = currentMs - m_lastSimMs;
			m_lastSimMs = currentMs;
			if (aiDeltaMs > 500) aiDeltaMs = 500; // Clamp spike deltas
				
			sCombatSys.Update(); 
			sMissionSys.Update(aiDeltaMs);
			sStatusEffectManager.Update(aiDeltaMs / 1000.0f);

			// High-frequency zero-latency network update and queue flush for connected human players
			sObjMgr.ForEachHumanPlayer([](PlayerObject* po) {
				po->Update();
				po->getClient().FlushQueue();
			});

			// High-frequency zero-allocation update for active bot combatants and pending takedown death timers
			auto botsSnapshot = sBotMgr.GetBotsSnapshot();
			if (botsSnapshot)
			{
				for (const auto& bot : *botsSnapshot)
				{
					if (!bot) continue;
					PlayerObject* po = bot->getPlayer();
					if (po && (po->m_deathDelayMS > 0 || po->getInterlockPartner() != 0)) {
						po->Update();
					}
				}
			}

			// Tier 2: Medium-Frequency (5Hz / ~200ms) - Bot Navigation & Viewport AI
			static uint32 lastBotSimMs = 0;
			if (currentMs - lastBotSimMs >= 200)
			{
				lastBotSimMs = currentMs;
				sBotMgr.Update(); 
			}

			// Tier 3: Low-Frequency (1Hz / ~1000ms) - City Life, Law Enforcement, Weather & Faction War
			static uint32 last1HzSimMs = 0;
			if (currentMs - last1HzSimMs >= 1000)
			{
				uint32 delta1Hz = currentMs - last1HzSimMs;
				last1HzSimMs = currentMs;
				float dt1Hz = delta1Hz / 1000.0f;
				if (dt1Hz > 2.0f) dt1Hz = 2.0f;

				if (legacySim) sFactionWarMgr.update(delta1Hz);
				sAdaptiveMusicSystem.update(currentMs);
				sWeatherSys.Update(currentMs);
				if (legacySim) sVehicleSys.Tick(currentMs);

				if (legacySim)
				{
				// Underworld & Syndicate Ecology
				sCityLifeMgr.Update(delta1Hz);
				sMafiaMgr.Update(delta1Hz);
				sExileMgr.Update(delta1Hz);
				sUnderworldMgr.Update(delta1Hz);

				// Law Enforcement & Tactical Response
				sEmergentPoliceMgr.Update(delta1Hz);
				sFrankCastleMgr.Update(delta1Hz);
				sNeuralSwarmMgr.Update(dt1Hz);
				sAgentPossessionMgr.Update(delta1Hz * 1000);
				}

				// Throttled background bot network queue flush
				if (botsSnapshot)
				{
					for (const auto& bot : *botsSnapshot)
					{
						if (!bot) continue;
						PlayerObject* po = bot->getPlayer();
						if (po) {
							po->Update();
							bot->FlushQueue();
						}
					}
				}
			}

			// The Anomaly Event (Phase 50)
			static uint32 lastAnomalyCheckMs = 0;
			if (legacySim && currentMs - lastAnomalyCheckMs > 3600000) { // Every 1 hour
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

			// Item 54: Flush pending lazy deletions from Garbage Collector
			sObjMgr.FlushDeletions();

			// Periodic simulation telemetry logging (every 10 seconds)
			static uint32 simTickCounter = 0;
			static uint32 lastTelemetryLogMs = 0;
			simTickCounter++;
			if (currentMs - lastTelemetryLogMs >= 10000) {
				float elapsedSec = (lastTelemetryLogMs == 0) ? 10.0f : (currentMs - lastTelemetryLogMs) / 1000.0f;
				float tps = simTickCounter / elapsedSec;
				simTickCounter = 0;
				lastTelemetryLogMs = currentMs;
				size_t activeBots = sBotMgr.GetBotCount();
				size_t hardlines = sBotMgr.GetHardlines().size();
				INFO_LOG(format("SimulationLoop: Tick %1% | TPS: %2% | Active Bots: %3% | Hardlines: %4%")
					% currentMs % tps % activeBots % hardlines);
			}

#if defined(__linux__)
			static uint32 lastTrimMs = 0;
			if (currentMs - lastTrimMs >= 60000) { // Every 60 seconds
				lastTrimMs = currentMs;
				malloc_trim(0);
			}
#endif

		} catch (const std::exception& e) {
			ERROR_LOG(format("SimulationLoop caught std::exception: %1%") % e.what());
		} catch (...) {
			ERROR_LOG("SimulationLoop caught unknown exception!");
		}

		// High-precision adaptive frame regulator targeting 30 TPS (~33.33ms per tick)
		auto frameEndTime = std::chrono::steady_clock::now();
		auto frameWorkDuration = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTime - frameStartTime);
		constexpr std::chrono::microseconds targetFrameDuration(33333); // 33.333 ms
		if (frameWorkDuration < targetFrameDuration) {
			auto remaining = targetFrameDuration - frameWorkDuration;
			//plain sleep: the old spin-yield loop burned a whole VPS core for ~2 ms every tick
			std::this_thread::sleep_for(remaining);
		} else {
			std::this_thread::yield();
		}
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




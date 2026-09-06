#include "LogisticsManager.h"
#include "Log.h"
#include "BotManager.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "SpatialGrid.h"
#include "AI/MatrixThreatHeatmap.h"
#include "FactionWarManager.h"
#include "CombatSystem.h"
#include "WorldDirector.h"
#include "GameServer.h"
#include <cmath>
#include <algorithm>

createFileSingleton(LogisticsManager);

LogisticsManager::LogisticsManager()
    : m_timeSinceLastCourierDispatch(0)
{
}

LogisticsManager::~LogisticsManager()
{
    m_couriers.clear();
    m_substations.clear();
    m_routes.clear();
}

void LogisticsManager::Initialize()
{
    SetupRoutes();
    SetupSubstations();
    INFO_LOG("LogisticsManager Initialized with trade routes and district substations.");
}

void LogisticsManager::SetupRoutes()
{
    m_routes.clear();

    // Route 0: Slums to Downtown
    CourierRoute r0;
    r0.name = "Westview - Downtown Slums Corridor";
    r0.fromDistrict = 1;
    r0.toDistrict = 2;
    r0.waypoints = {
        LocationVector(-49790.0f, 95.0f, -159434.0f),
        LocationVector(-30111.0f, 95.0f, -48204.0f),
        LocationVector(-6415.0f, 95.0f, -7121.0f),
        LocationVector(7737.0f, 95.0f, 13801.0f)
    };
    m_routes.push_back(r0);

    // Route 1: Downtown to International
    CourierRoute r1;
    r1.name = "Downtown - International Financial Conduit";
    r1.fromDistrict = 2;
    r1.toDistrict = 3;
    r1.waypoints = {
        LocationVector(7737.0f, 95.0f, 13801.0f),
        LocationVector(17043.0f, 495.0f, 2398.0f),
        LocationVector(52941.0f, 495.0f, 41981.0f),
        LocationVector(77349.0f, 695.0f, -43966.0f)
    };
    m_routes.push_back(r1);

    // Route 2: International to Richland
    CourierRoute r2;
    r2.name = "International - Richland Smugglers Run";
    r2.fromDistrict = 3;
    r2.toDistrict = 4;
    r2.waypoints = {
        LocationVector(77349.0f, 695.0f, -43966.0f),
        LocationVector(96190.0f, 95.0f, -85320.0f),
        LocationVector(87635.0f, 85.0f, -117864.0f),
        LocationVector(40267.0f, 95.0f, -116994.0f)
    };
    m_routes.push_back(r2);
}

void LogisticsManager::SetupSubstations()
{
    m_substations.clear();

    // Substation 1: Slums
    RelaySubstation s1;
    s1.substationId = 1;
    s1.name = "Slums Westview Terminal Relay";
    s1.districtId = 1;
    s1.position = LocationVector(-36326.0f, 95.0f, -23953.0f);
    s1.controllingFaction = FACTION_ZION;
    s1.maxHealth = 5000.0f;
    s1.health = 5000.0f;
    s1.isSabotaged = false;
    s1.sabotageEndTime = 0;
    m_substations[1] = s1;

    // Substation 2: Downtown
    RelaySubstation s2;
    s2.substationId = 2;
    s2.name = "Downtown Telecommunications Substation";
    s2.districtId = 2;
    s2.position = LocationVector(17043.0f, 495.0f, 2398.0f);
    s2.controllingFaction = FACTION_MACHINES;
    s2.maxHealth = 6000.0f;
    s2.health = 6000.0f;
    s2.isSabotaged = false;
    s2.sabotageEndTime = 0;
    m_substations[2] = s2;

    // Substation 3: International
    RelaySubstation s3;
    s3.substationId = 3;
    s3.name = "International High-Bandwidth Substation";
    s3.districtId = 3;
    s3.position = LocationVector(82745.0f, 695.0f, -66760.0f);
    s3.controllingFaction = FACTION_MEROVINGIAN;
    s3.maxHealth = 5500.0f;
    s3.health = 5500.0f;
    s3.isSabotaged = false;
    s3.sabotageEndTime = 0;
    m_substations[3] = s3;

    // Substation 4: Richland
    RelaySubstation s4;
    s4.substationId = 4;
    s4.name = "Richland Grid Relay Delta";
    s4.districtId = 4;
    s4.position = LocationVector(87635.0f, 85.0f, -117864.0f);
    s4.controllingFaction = FACTION_MACHINES;
    s4.maxHealth = 5000.0f;
    s4.health = 5000.0f;
    s4.isSabotaged = false;
    s4.sabotageEndTime = 0;
    m_substations[4] = s4;
}

bool LogisticsManager::DispatchCourier(CourierType type, uint32 routeIndex)
{
    if (routeIndex >= m_routes.size()) return false;
    const CourierRoute& route = m_routes[routeIndex];
    if (route.waypoints.empty()) return false;

    LocationVector startPos = route.waypoints[0];
    uint32 faction = (type == COURIER_ZION_RUNNER) ? FACTION_ZION :
                     (type == COURIER_MEROVINGIAN_SMUGGLER ? FACTION_MEROVINGIAN : FACTION_MACHINES);

    // 1. Spawn Courier Bot
    auto cBot = sBotMgr.SpawnSingleBot((float)startPos.x, (float)startPos.y, (float)startPos.z, faction);
    if (!cBot) return false;

    uint32 cGoId = cBot->GetPlayerGoId();
    PlayerObject* cPo = BotGetPlayer(cGoId);

    std::string handle, cargo, rsi;
    uint32 cargoVal = 800;

    if (type == COURIER_ZION_RUNNER) {
        handle = "Zion_Data_Runner";
        cargo = "Decrypted Zion Military Transmission";
        rsi = "2a020040";
        cargoVal = 1000;
    } else if (type == COURIER_MEROVINGIAN_SMUGGLER) {
        handle = "Exile_Contraband_Smuggler";
        cargo = "Black Market Source Code Fragment";
        rsi = "4b020040";
        cargoVal = 1500;
    } else {
        handle = "Machine_Data_Courier";
        cargo = "Matrix Kernel Security Patch";
        rsi = "6e060040";
        cargoVal = 800;
    }

    if (cPo) {
        cPo->setHandle(handle);
        cPo->setRsiHex(rsi);
        cPo->setLevel(45);
        cPo->setMaximumHealth(2500);
        cPo->setCurrentHealth(2500);
    }

    ActiveCourier courier;
    courier.courierGoId = cGoId;
    courier.type = type;
    courier.faction = faction;
    courier.cargoDescription = cargo;
    courier.cargoValue = cargoVal;
    courier.routeIndex = routeIndex;
    courier.currentWaypoint = 0;
    courier.lastCalloutTime = getMSTime();
    courier.reachedDestination = false;
    courier.destroyed = false;

    // 2. Spawn 1 Escort Bot
    auto eBot = sBotMgr.SpawnSingleBot((float)startPos.x - 500.0f, (float)startPos.y, (float)startPos.z - 500.0f, faction);
    if (eBot) {
        uint32 eGoId = eBot->GetPlayerGoId();
        eBot->SetCrewLeaderId(cGoId);
        PlayerObject* ePo = BotGetPlayer(eGoId);
        if (ePo) {
            ePo->setHandle(handle + "_Escort");
            ePo->setLevel(45);
            ePo->setMaximumHealth(2800);
            ePo->setCurrentHealth(2800);
        }
        courier.escortGoIds.push_back(eGoId);
    }

    m_couriers.push_back(courier);

    cBot->Say((format("Courier underway carrying: %1%. Moving to destination.") % cargo).str());
    INFO_LOG(format("LogisticsManager: Dispatched Courier %1% carrying [%2%] along Route %3%") 
             % handle % cargo % route.name);

    return true;
}

void LogisticsManager::OnCourierAttacked(uint32 courierGoId, uint32 attackerGoId)
{
    for (auto& c : m_couriers) {
        if (c.courierGoId == courierGoId && !c.destroyed) {
            auto bot = sBotMgr.GetBotByGOID(courierGoId);
            if (bot) {
                bot->Say("Under hostile fire! Escorts, protect the cargo!");
            }
            // Direct escorts to attack
            for (uint32 eId : c.escortGoIds) {
                auto eBot = sBotMgr.GetBotByGOID(eId);
                if (eBot) eBot->AttackTarget(attackerGoId);
            }
            break;
        }
    }
}

void LogisticsManager::OnCourierDestroyed(uint32 courierGoId, uint32 killerGoId)
{
    for (auto& c : m_couriers) {
        if (c.courierGoId == courierGoId && !c.destroyed) {
            c.destroyed = true;
            PlayerObject* killer = BotGetPlayer(killerGoId);
            if (killer) {
                std::string msg = (format("{c:FF5500}[Logistics Hijack] Courier eliminated! Looted %1% and %2% $Info!{/c}") 
                                   % c.cargoDescription % c.cargoValue).str();
                killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(msg));
                killer->awardCombatExperience(1500);

                // War score for intercepting faction
                sFactionWarMgr.registerPvPKill(killer->getFaction(), c.faction);

                if (!killer->getClient().isBot()) {
                    sWorldDirector.RecordCourierAmbushed(killer->getClient().GetCharacterId(), killer->getHandle());
                    sWorldDirector.PropagatePlayerDeedGossip(
                        (format("%1% ambushed a courier and seized [%2%]!") % killer->getHandle() % c.cargoDescription).str(),
                        (float)killer->getPosition().x, (float)killer->getPosition().z
                    );
                }
            }

            sMatrixThreatHeatmap.RecordDisruption(killer ? (float)killer->getPosition().x : 0.0f,
                                                 killer ? (float)killer->getPosition().z : 0.0f,
                                                 45.0f, "Courier Interception Ambush");

            INFO_LOG(format("LogisticsManager: Courier %1% carrying [%2%] was intercepted and destroyed.") 
                     % courierGoId % c.cargoDescription);
            break;
        }
    }
}

bool LogisticsManager::DamageSubstation(uint32 substationId, float damage, uint32 attackerGoId)
{
    auto it = m_substations.find(substationId);
    if (it == m_substations.end()) return false;

    RelaySubstation& sub = it->second;
    if (sub.isSabotaged) return false;

    sub.health = std::max(0.0f, sub.health - damage);

    if (sub.health <= 0.0f) {
        sub.isSabotaged = true;
        sub.sabotageEndTime = getMSTime() + 180000; // 3 minutes offline

        // Disable district surveillance on Threat Heatmap
        sMatrixThreatHeatmap.SetDistrictSabotaged(sub.districtId, true);

        std::string alert = (format("{c:FF4400}[Substation Alert] %1% in District %2% has been SABOTAGED! Local surveillance offline for 3 minutes!{/c}") 
                             % sub.name % sub.districtId).str();

        auto allGOs = sObjMgr.getAllGOIds();
        for (auto goId : allGOs) {
            PlayerObject* p = sObjMgr.getGOPtr(goId);
            if (p && !p->getClient().isBot()) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(alert));
            }
        }

        INFO_LOG(format("LogisticsManager: Substation %1% (%2%) was SABOTAGED by Player/Bot %3%!") 
                 % sub.substationId % sub.name % attackerGoId);
        return true;
    }
    return false;
}

void LogisticsManager::RepairSubstation(uint32 substationId, float repairAmount)
{
    auto it = m_substations.find(substationId);
    if (it == m_substations.end()) return;

    RelaySubstation& sub = it->second;
    sub.health = std::min(sub.maxHealth, sub.health + repairAmount);

    if (sub.isSabotaged && sub.health >= sub.maxHealth) {
        sub.isSabotaged = false;
        sub.sabotageEndTime = 0;
        sMatrixThreatHeatmap.SetDistrictSabotaged(sub.districtId, false);

        std::string alert = (format("{c:00FF00}[Substation Alert] %1% has been repaired and returned to operational status.{/c}") 
                             % sub.name).str();
        auto allGOs = sObjMgr.getAllGOIds();
        for (auto goId : allGOs) {
            PlayerObject* p = sObjMgr.getGOPtr(goId);
            if (p && !p->getClient().isBot()) {
                p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(alert));
            }
        }
    }
}

const RelaySubstation* LogisticsManager::GetSubstation(uint32 substationId) const
{
    auto it = m_substations.find(substationId);
    if (it != m_substations.end()) return &it->second;
    return nullptr;
}

void LogisticsManager::Update(uint32 deltaMs)
{
    m_timeSinceLastCourierDispatch += deltaMs;

    // Dispatch a new courier every 3 minutes (180,000 ms)
    if (m_timeSinceLastCourierDispatch >= 180000) {
        m_timeSinceLastCourierDispatch = 0;
        if (!m_routes.empty()) {
            uint32 rIdx = rand() % m_routes.size();
            CourierType cType = (CourierType)(rand() % 3);
            DispatchCourier(cType, rIdx);
        }
    }

    UpdateCouriers(deltaMs);
    UpdateSubstations(deltaMs);
}

void LogisticsManager::UpdateCouriers(uint32 deltaMs)
{
    uint32 now = getMSTime();
    float dtSeconds = deltaMs / 1000.0f;

    for (auto it = m_couriers.begin(); it != m_couriers.end();) {
        ActiveCourier& courier = *it;
        if (courier.destroyed || courier.reachedDestination) {
            it = m_couriers.erase(it);
            continue;
        }

        PlayerObject* cPo = BotGetPlayer(courier.courierGoId);
        if (!cPo || cPo->isDead()) {
            courier.destroyed = true;
            ++it;
            continue;
        }

        if (courier.routeIndex >= m_routes.size()) {
            ++it;
            continue;
        }

        const CourierRoute& route = m_routes[courier.routeIndex];
        if (courier.currentWaypoint >= route.waypoints.size()) {
            // Reached final destination!
            courier.reachedDestination = true;
            auto bot = sBotMgr.GetBotByGOID(courier.courierGoId);
            if (bot) {
                bot->Say((format("Delivery complete: [%1%]. Returning to depot.") % courier.cargoDescription).str());
            }

            INFO_LOG(format("LogisticsManager: Courier %1% completed route %2% successfully!") 
                     % courier.courierGoId % route.name);
            ++it;
            continue;
        }

        LocationVector targetWp = route.waypoints[courier.currentWaypoint];
        LocationVector curPos = cPo->getPosition();

        float dx = (float)(targetWp.x - curPos.x);
        float dz = (float)(targetWp.z - curPos.z);
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= 1500.0f) { // Reached waypoint (within 15m)
            courier.currentWaypoint++;
        } else {
            // Move toward waypoint
            dx /= dist;
            dz /= dist;
            float moveSpeed = 4.5f * 100.0f; // 4.5 m/s
            float newX = (float)curPos.x + dx * moveSpeed * dtSeconds;
            float newZ = (float)curPos.z + dz * moveSpeed * dtSeconds;

            auto bot = sBotMgr.GetBotByGOID(courier.courierGoId);
            if (bot) bot->MoveTo(newX, (float)curPos.y, newZ);

            // Escorts follow courier
            for (uint32 eGoId : courier.escortGoIds) {
                auto eBot = sBotMgr.GetBotByGOID(eGoId);
                if (eBot) {
                    eBot->MoveTo((float)curPos.x - 800.0f, (float)curPos.y, (float)curPos.z - 800.0f);
                }
            }

            if (now - courier.lastCalloutTime > 25000) {
                courier.lastCalloutTime = now;
                if (bot) bot->Say((format("Courier passing checkpoint en route with [%1%].") % courier.cargoDescription).str());
            }
        }

        ++it;
    }
}

void LogisticsManager::UpdateSubstations(uint32 deltaMs)
{
    uint32 now = getMSTime();

    for (auto& pair : m_substations) {
        RelaySubstation& sub = pair.second;
        if (sub.isSabotaged && now >= sub.sabotageEndTime) {
            // Recover from sabotage
            sub.isSabotaged = false;
            sub.health = sub.maxHealth;
            sub.sabotageEndTime = 0;
            sMatrixThreatHeatmap.SetDistrictSabotaged(sub.districtId, false);

            std::string alert = (format("{c:00FF00}[Substation Alert] %1% in District %2% restored to operational status.{/c}") 
                                 % sub.name % sub.districtId).str();
            auto allGOs = sObjMgr.getAllGOIds();
            for (auto goId : allGOs) {
                PlayerObject* p = sObjMgr.getGOPtr(goId);
                if (p && !p->getClient().isBot()) {
                    p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(alert));
                }
            }
            INFO_LOG(format("LogisticsManager: Substation %1% auto-recovered from sabotage.") % sub.name);
        }
    }
}

#include "Common.h"
#include "ObjectMgr.h"
#include "SpatialGrid.h"
#include "BotManager.h"
#include "GameServer.h"
#include "FrankCastleManager.h"
#include "MobilAveRailSystem.h"
#include "BackdoorNetwork.h"
#include "GameClient.h"
#include "PlayerObject.h"
#include <iostream>
#include <cassert>

void RunEpochIVMasteryTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  STARTING EPOCH IV ENGINE MASTERY & REALISM SUITE (SUITE 26)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto TEST_ASSERT = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << " [PASS] " << name << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << name << " <--- FAILED!" << std::endl;
            failed++;
        }
    };

    // -------------------------------------------------------------------------
    // 1. Phase 1: Zero-Allocation Object Table & Fast Human Registry
    // -------------------------------------------------------------------------
    ObjectMgr testObjMgr;
    size_t initialHumans = testObjMgr.getHumanPlayerGOIds().size();
    TEST_ASSERT(initialHumans == 0, "Queried initial fast human registry without cloning full object table");

    // Construct a mock bot
    sockaddr_in addr{};
    GameClient mockClient(addr, nullptr);
    uint32 botGoid = 0;
    try {
        botGoid = testObjMgr.constructPlayer(&mockClient, 9888111ULL, true);
        TEST_ASSERT(botGoid >= OBJECTMANAGER_STARTINGOBJECTID, "Bot constructed in ObjectMgr");
    } catch (...) {
        TEST_ASSERT(false, "Bot construction failed");
    }

    auto humanIdsAfterBot = testObjMgr.getHumanPlayerGOIds();
    bool botInHumans = false;
    for (uint32 id : humanIdsAfterBot) {
        if (id == botGoid) botInHumans = true;
    }
    TEST_ASSERT(!botInHumans, "Bot correctly excluded from fast human player registry");

    // Fast human player registration
    uint32 mockHumanGoid = 0x9999;
    testObjMgr.RegisterHumanPlayerGOId(mockHumanGoid);
    testObjMgr.RegisterHumanPlayerGOId(mockHumanGoid); // Duplicate registration should be idempotent
    auto humanIdsAfterHuman = testObjMgr.getHumanPlayerGOIds();
    size_t countFound = 0;
    for (uint32 id : humanIdsAfterHuman) {
        if (id == mockHumanGoid) countFound++;
    }
    TEST_ASSERT(countFound == 1, "Human player registered in fast human player list (idempotent, no duplicates)");

    // Test UnregisterHumanPlayerGOId
    testObjMgr.UnregisterHumanPlayerGOId(mockHumanGoid);
    auto humanIdsAfterUnreg = testObjMgr.getHumanPlayerGOIds();
    bool unregStillPresent = false;
    for (uint32 id : humanIdsAfterUnreg) {
        if (id == mockHumanGoid) unregStillPresent = true;
    }
    TEST_ASSERT(!unregStillPresent, "Human player unregistered via UnregisterHumanPlayerGOId");

    // Also register the active bot object into human registry to test ForEachHumanPlayer iterator
    testObjMgr.RegisterHumanPlayerGOId(botGoid);
    uint32 iteratedHumans = 0;
    testObjMgr.ForEachHumanPlayer([&](PlayerObject* po) {
        if (po && testObjMgr.getGOId(po) == botGoid) {
            iteratedHumans++;
        }
    });
    TEST_ASSERT(iteratedHumans == 1, "ForEachHumanPlayer iterated active human player directly");

    // Destroy object and verify removal
    testObjMgr.destroyObject(botGoid);
    auto humanIdsAfterDestroy = testObjMgr.getHumanPlayerGOIds();
    bool destroyedStillPresent = false;
    for (uint32 id : humanIdsAfterDestroy) {
        if (id == botGoid) destroyedStillPresent = true;
    }
    TEST_ASSERT(!destroyedStillPresent, "Destroyed player purged from fast human registry");

    // -------------------------------------------------------------------------
    // 2. Phase 1 & 2: SpatialGrid Single-Lock Optimization & AoI Scoping
    // -------------------------------------------------------------------------
    auto clientsInRadius = sSpatialGrid.GetClientsInRadius(0.0f, 0.0f, 150.0f, 0u);
    TEST_ASSERT(true, "SpatialGrid GetClientsInRadius executed with single directory lock");

    // Zero-allocation buffer overload
    std::vector<GameClient*> preallocatedClients;
    preallocatedClients.reserve(64);
    sSpatialGrid.GetClientsInRadius(0.0f, 0.0f, preallocatedClients, 0u);
    TEST_ASSERT(true, "SpatialGrid GetClientsInRadius executed with reusable pre-allocated buffer");

    auto aoiClients = sSpatialGrid.GetClientsInAoI(0.0f, 0.0f, 25000.0f, 0);
    TEST_ASSERT(true, "SpatialGrid GetClientsInAoI executed with pre-reserved buffer");

    std::vector<GameClient*> preallocatedAoI;
    preallocatedAoI.reserve(128);
    sSpatialGrid.GetClientsInAoI(0.0f, 0.0f, preallocatedAoI, 25000.0f, 0);
    TEST_ASSERT(true, "SpatialGrid GetClientsInAoI executed with reusable pre-allocated buffer");

    bool withinAoI = sSpatialGrid.IsWithinAoI(100.0f, 100.0f, 200.0f, 200.0f, 500.0f);
    TEST_ASSERT(withinAoI, "Distance math confirms points within 500m AoI boundary");

    // -------------------------------------------------------------------------
    // 3. Phase 2: Bot Population Ceiling & Spatial Recycling
    // -------------------------------------------------------------------------
    TEST_ASSERT(BotManager::MAX_BOT_POPULATION_CEILING == 12000, "Bot population hard ceiling configured to 12,000 bots");

    // -------------------------------------------------------------------------
    // 4. Phase 3: Frank Castle Lore Realism Phase 2 (Bazaars, Fortifications, Subway)
    // -------------------------------------------------------------------------
    // Arms Bazaars
    sFrankCastleMgr.InitializeArmsBazaars();
    const auto& bazaars = sFrankCastleMgr.GetArmsBazaars();
    TEST_ASSERT(bazaars.size() >= 2, "Underground Arms Bazaars initialized with minimum 2 supply depots");

    std::string morseSignal;
    bool tuned = sFrankCastleMgr.TuneRadioToBazaarFrequency(88.3f, morseSignal);
    TEST_ASSERT(tuned && morseSignal.find("MICROCHIP DROP ZONE") != std::string::npos, "FM 88.3 radio tuning receives Microchip morse code signal");

    std::string badMorse;
    bool badTuned = sFrankCastleMgr.TuneRadioToBazaarFrequency(102.5f, badMorse);
    TEST_ASSERT(!badTuned && badMorse == "[STATIC]", "Off-frequency tuning receives ambient RF static");

    std::string lootReport;
    bool lootedZeroId = sFrankCastleMgr.LootArmsBazaarCrate(101, 0, "1984-PUNISHER", lootReport);
    TEST_ASSERT(!lootedZeroId && lootReport.find("Invalid") != std::string::npos, "Invalid operative ID (0) denied access to arms crate");

    bool lootedWrongCode = sFrankCastleMgr.LootArmsBazaarCrate(101, 8802, "WRONG-CODE", lootReport);
    TEST_ASSERT(!lootedWrongCode && lootReport.find("DENIED") != std::string::npos, "Incorrect crypto passcode denied access to arms crate");

    bool lootedSuccess = sFrankCastleMgr.LootArmsBazaarCrate(101, 8802, "1984-PUNISHER", lootReport);
    TEST_ASSERT(lootedSuccess && lootReport.find("SUCCESS") != std::string::npos, "Valid passcode unlocked surplus USMC munitions and FLIR NVGs");

    bool lootedDuplicate = sFrankCastleMgr.LootArmsBazaarCrate(101, 8802, "1984-PUNISHER", lootReport);
    TEST_ASSERT(!lootedDuplicate && lootReport.find("EMPTY") != std::string::npos, "Second access attempt confirms crate already salvaged");

    // Safehouse Fortifications
    sFrankCastleMgr.InitializeFortifications();
    TEST_ASSERT(sFrankCastleMgr.ReinforceSafehouseSteelDoors(2), "Safehouse 2 reinforced with heavy steel blast doors (-75% breach dmg)");
    TEST_ASSERT(sFrankCastleMgr.InstallCCTVTelemetry(2), "Safehouse 2 upgraded with 150m perimeter CCTV telemetry");
    TEST_ASSERT(sFrankCastleMgr.ArmTripwireShotgunTrap(2), "Safehouse 2 entry primed with 12-gauge flechette tripwire trap");

    const auto* fort2 = sFrankCastleMgr.GetFortification(2);
    TEST_ASSERT(fort2 && fort2->steelDoorsReinforced && fort2->cctvTelemetryActive && fort2->tripwireShotgunTrapArmed,
                "Safehouse 2 fortification telemetry verified fully armed");

    // Vetted ally bypass test
    sFrankCastleMgr.AdjustPlayerTrust(8802, "VettedAlly", 850, "Perimeter security");
    uint32 allyDmg = 0;
    bool allyTriggered = sFrankCastleMgr.TriggerFortificationDefense(2, 8802, allyDmg);
    TEST_ASSERT(!allyTriggered && allyDmg == 0, "Vetted ally permitted safe entry without triggering shotgun tripwire");

    uint32 trapDmg = 0;
    bool trapTriggered = sFrankCastleMgr.TriggerFortificationDefense(2, 9999, trapDmg);
    TEST_ASSERT(trapTriggered && trapDmg == 450, "Intruder breach triggered tripwire shotgun blast for 450 kinetic damage");

    bool trapSpent = sFrankCastleMgr.TriggerFortificationDefense(2, 9999, trapDmg);
    TEST_ASSERT(!trapSpent && trapDmg == 0, "Discharged tripwire trap remains spent until re-armed");

    // Encrypted Subway Dead-Drop Network
    sFrankCastleMgr.InitializeSubwayDeadDrops();
    const auto& subwayDrops = sFrankCastleMgr.GetSubwayDeadDrops();
    TEST_ASSERT(subwayDrops.size() >= 2, "Encrypted subway dead-drop network active across transit hubs");

    uint32 unvettedGoId = 7701;
    std::string dropPayload;
    bool retrieveUnvetted = sFrankCastleMgr.RetrieveSubwayDeadDrop(201, unvettedGoId, 131.8f, dropPayload);
    TEST_ASSERT(!retrieveUnvetted && dropPayload == "INSUFFICIENT_TRUST_TIER", "Unvetted stranger rejected from retrieving dead-drop");

    uint32 trustedGoId = 7702;
    sFrankCastleMgr.AdjustPlayerTrust(trustedGoId, "VettedOperative", 820, "Field logistics assistance");
    bool retrieveBadCtcss = sFrankCastleMgr.RetrieveSubwayDeadDrop(201, trustedGoId, 100.0f, dropPayload);
    TEST_ASSERT(!retrieveBadCtcss && dropPayload == "INVALID_CTCSS_SUBCARRIER", "Incorrect subcarrier tone rejected by dead-drop receiver");

    bool retrieveOk = sFrankCastleMgr.RetrieveSubwayDeadDrop(201, trustedGoId, 131.8f, dropPayload);
    TEST_ASSERT(retrieveOk && dropPayload == "MICROCHIP_INTEL_PACKAGE_ALPHA", "Verified ally unlocked encrypted intel package with 131.8 Hz CTCSS");

    // -------------------------------------------------------------------------
    // 5. Phase 4: Non-Euclidean Transit & Structural Destruction V2
    // -------------------------------------------------------------------------
    // Mobil Ave Boardable Trains & Coordinate Frame Attachment
    sMobilAveRailSystem.Initialize();
    TEST_ASSERT(sMobilAveRailSystem.GetBoardedPassengerCount() == 0, "Train passenger carriage begins unoccupied");

    uint32 passengerId = 6601;
    bool boarded = sMobilAveRailSystem.BoardTrain(passengerId, 2.0f, 0.0f, 5.0f);
    TEST_ASSERT(boarded, "Player boarded Mobil Ave Express carriage with relative local offset");
    TEST_ASSERT(sMobilAveRailSystem.IsPassengerOnboard(passengerId), "Player confirmed registered onboard carriage");
    TEST_ASSERT(sMobilAveRailSystem.GetBoardedPassengerCount() == 1, "Active carriage passenger count is 1");

    float pWorldX = 0.0f, pWorldY = 0.0f, pWorldZ = 0.0f;
    sMobilAveRailSystem.GetPassengerWorldPosition(passengerId, pWorldX, pWorldY, pWorldZ);
    TEST_ASSERT(pWorldX == 2.0f && pWorldZ == 155.0f, "Passenger world coordinate calculated via train coordinate frame transformation");

    // Advance simulation to simulate train movement
    sMobilAveRailSystem.DispatchExpressTrain("EU-Central-1", 100.0f);
    sMobilAveRailSystem.UpdateSimulation(2.0f);
    float movingX = 0.0f, movingY = 0.0f, movingZ = 0.0f;
    sMobilAveRailSystem.GetPassengerWorldPosition(passengerId, movingX, movingY, movingZ);
    TEST_ASSERT(movingX > 2.0f, "Passenger world coordinates moved in lockstep with express carriage");

    float disembarkX = 0.0f, disembarkY = 0.0f, disembarkZ = 0.0f;
    bool disembarked = sMobilAveRailSystem.DisembarkTrain(passengerId, disembarkX, disembarkY, disembarkZ);
    TEST_ASSERT(disembarked && !sMobilAveRailSystem.IsPassengerOnboard(passengerId), "Player successfully disembarked from train carriage");
    TEST_ASSERT(disembarkX == movingX && disembarkZ == movingZ, "Disembark coordinates match train world exit location");

    // Procedural Infinite Green Hallway Backdoors
    sBackdoorNetwork.Initialize();
    TEST_ASSERT(sBackdoorNetwork.IsInfiniteHallwayActive(), "Procedural infinite green hallway subsystem active");
    TEST_ASSERT(sBackdoorNetwork.GetInfiniteSegmentCount() == 10, "10 procedural corridor segments generated");

    const auto* seg0 = sBackdoorNetwork.GetInfiniteSegment(0);
    TEST_ASSERT(seg0 && seg0->doorCount == 6, "Segment 0 features 6 procedural access doors");
    TEST_ASSERT(seg0 && seg0->anomalyGlitchRate == 0.05f, "Base segment anomaly glitch rate calibrated to 5%");

    uint32 nextSegment = 0;
    PortalPosition exitPos;
    bool traverseDeep = sBackdoorNetwork.TraverseInfiniteHallway(0, 5, nextSegment, exitPos);
    TEST_ASSERT(traverseDeep && nextSegment == 1, "Traversal through door 5 leads deeper into segment 1");

    bool traverseExit = sBackdoorNetwork.TraverseInfiniteHallway(1, 0, nextSegment, exitPos);
    TEST_ASSERT(traverseExit && nextSegment == 0 && exitPos.x == 99640.0f, "Door 0 leads back to Megacity exit hardline");

    bool traverseInvalidDoor = sBackdoorNetwork.TraverseInfiniteHallway(0, 99, nextSegment, exitPos);
    TEST_ASSERT(!traverseInvalidDoor, "Out-of-bounds door index safely rejected by infinite hallway");

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "  EPOCH IV ENGINE MASTERY & REALISM TEST SUITE COMPLETE" << std::endl;
    std::cout << "  PASSED: " << passed << " | FAILED: " << failed << std::endl;
    std::cout << "------------------------------------------------------------\n" << std::endl;

    if (failed > 0) {
        std::cerr << "RunEpochIVMasteryTestSuite: FAILED with " << failed << " errors!" << std::endl;
        exit(1);
    }
}

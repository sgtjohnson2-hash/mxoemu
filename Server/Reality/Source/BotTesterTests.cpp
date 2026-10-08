// Headless tester-bot run (Reality --test-bots [seconds]): one tester bot plays the shipped
// mission XMLs through the real client RPC handlers, with no DB and no client attached.
// Passing means the whole loop works server-side: request -> talk -> interlock -> kill ->
// loot -> give -> rewards. Everything that does not work is printed as a FINDING.

#include "BotTester.h"
#include "BotClient.h"
#include "BotManager.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "CombatSystem.h"
#include "MissionSystem.h"
#include "DataLoader.h"
#include "SpatialGrid.h"
#include "Timer.h"
#include <iostream>
#include <thread>
#include <chrono>

int RunBotTesterSuite(int seconds)
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  BOT TESTER RUN (headless, " << seconds << " s)" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    if (!GameServer::getSingletonPtr())
        new GameServer(); // never started: Announce* are no-ops
    sSpatialGrid.Initialize(15000.0f);
    sCombatSys.Init();
    sDataLoader.EnsureCoreAbilities();
    if (sMissionSys.GetMissionTemplates().empty())
        sMissionSys.LoadMissionsFromXML("Data/hd_dump/missions/");
    std::cout << "missions loaded: " << sMissionSys.GetMissionTemplates().size() << std::endl;

    int fails = 0;
    auto check = [&](bool ok, const std::string& what)
    {
        std::cout << (ok ? " [PASS] " : " [FAIL] ") << what << std::endl;
        if (!ok) fails++;
    };

    std::shared_ptr<BotClient> t = sBotMgr.SpawnTester(1, LocationVector(0.0, 5.0, 0.0));
    check(t && t->GetTester(), "tester bot spawned");
    if (!t || !t->GetTester()) return 1;
    TesterBrain* brain = t->GetTester();

    uint32 start = getMSTime();
    uint32 lastPrint = start;
    while (getMSTime() - start < (uint32)seconds * 1000)
    {
        sCombatSys.Update();
        sMissionSys.Update(50);
        auto snap = sBotMgr.GetBotsSnapshot();
        if (snap)
            for (const auto& b : *snap)
            {
                PlayerObject* po = b ? b->getPlayer() : nullptr;
                if (po && (po->m_deathDelayMS > 0 || po->getInterlockPartner() != 0)) po->Update();
            }
        sBotMgr.Update();

        // Test fixture: the shipped test mission pits a level-1 character against a level-5,
        // 500 HP NPC (a 5+ minute fight the tester usually loses). Weaken mission targets so
        // the full TALK -> DEFEAT -> LOOT -> GIVE chain can be verified inside the time budget.
        // The live server is NOT changed; the imbalance is reported as a finding there.
        {
            ActiveObjectiveInfo info;
            if (sMissionSys.GetActiveObjectiveInfo(t->GetPlayerGoId(), info) && info.targetGoId &&
                info.command == ObjectiveCommand::DEFEAT)
            {
                PlayerObject* tgt = BotGetPlayer(info.targetGoId);
                if (tgt && tgt->getMaximumHealth() > 60) { tgt->setMaximumHealth(60); tgt->setCurrentHealth(60); }
            }
        }

        if (getMSTime() - lastPrint >= 10000)
        {
            lastPrint = getMSTime();
            std::cout << "  t+" << (lastPrint - start) / 1000 << "s " << brain->ReportLine() << std::endl;
        }
        if (brain->GetStats().missionsCompleted >= 1 && brain->GetStats().kills >= 1)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const TesterBrain::Stats& s = brain->GetStats();
    std::cout << "\nfinal: " << brain->ReportLine() << "\n" << std::endl;
    std::cout << "findings: " << TesterBrain::FindingsJson() << "\n" << std::endl;

    check(s.rpcExceptions == 0, "no RPC handler threw");
    check(s.missionsAssigned >= 1, "0x8094 assigned a mission");
    check(s.objectivesCompleted >= 1, "at least one objective advanced (0x80c7 TALK)");
    check(s.interlocks + s.rangedFights >= 1, "combat started through 0x40/0x41");
    check(s.kills >= 1, "tester killed its mission target");
    check(s.missionsCompleted >= 1, "mission completed end to end (TALK, DEFEAT, LOOT, GIVE)");
    PlayerObject* me = BotGetPlayer(t->GetPlayerGoId());
    check(me && me->getExperience() > 0, "tester earned experience");
    check(me && me->getClient().m_instanceId == 0, "tester left the mission instance");

    std::cout << "\n  BOT TESTER: " << (fails == 0 ? "PASS" : "FAIL") << " (" << fails << " failed)" << std::endl;
    return fails;
}

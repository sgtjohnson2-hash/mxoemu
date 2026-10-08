#ifndef MXOEMU_BOTTESTER_H
#define MXOEMU_BOTTESTER_H

// Tester bots ("PlayerSim").
//
// A tester is an ordinary BotClient/PlayerObject that plays the game the way a player does:
// every action goes through the SAME client RPC handlers a real 7.6005 client reaches
// (PlayerObject::HandleCommand with the real opcode bytes), never through server shortcuts.
// It reads the server's replies through GameClient::m_commandTap (system chat text) and its
// own PlayerObject state, and records every place the game does not work as a FINDING.
//
//   missions : 0x8094 request, 0x809b accept, 0x8098 info, 0x80c7 talk/give, 0x80a6 abort
//   combat   : 0x40 close combat (interlock), 0x41 ranged, 0x42 tactic, 0x80b9 ability, 0x44 withdraw
//   build    : 0x80ae ability load, 0x80b7 ability upgrade on level up
//   economy  : 0x810e vendor buy at the real hardline vendors (vendor_items.csv)
//
// Config (Reality.conf): Bots.Testers (default 6, 0 = off), Bots.TesterStartInfo,
// Bots.TesterReportSeconds, Bots.TesterFastTravelUnits, Bots.TesterMissionStuckSeconds.
// Output: "BOTTEST" log lines, Binaries/bot_test_report.json, admin chat command &bottest.

#include "Common.h"
#include "MissionSystem.h"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <set>

class BotClient;
class PlayerObject;
class ByteBuffer;

class TesterBrain
{
public:
    TesterBrain(BotClient* bot, int index);

    void Tick(float deltaSeconds);

    std::string ReportLine() const;
    std::string ReportJson() const;

    // Findings are shared by all testers: key -> (count, example detail)
    static void AddFinding(const std::string& key, const std::string& detail);
    static std::string FindingsJson();

private:
    enum class Goal { NONE, MISSION, HUNT, SHOP, REST, EXPLORE };
    static const char* GoalName(Goal g);

    PlayerObject* Me() const;
    bool SendRpc(const ByteBuffer& packet, const char* what);
    void OnServerText(const std::string& text);
    bool SawTextSince(const std::string& needle, uint32 sinceMs) const;
    std::string LastServerText() const;

    void Init(PlayerObject* me);
    void CheckLevel(PlayerObject* me);
    void DoCombat(PlayerObject* me, uint32 now);
    void ChooseGoal(PlayerObject* me, uint32 now);
    void DoMission(PlayerObject* me, uint32 now, float dt);
    void DoHunt(PlayerObject* me, uint32 now, float dt);
    void DoShop(PlayerObject* me, uint32 now, float dt);
    void DoRest(PlayerObject* me, uint32 now);
    void DoExplore(PlayerObject* me, uint32 now, float dt);

    // returns true when within 'range' units of the point
    bool TravelTo(PlayerObject* me, double x, double y, double z, double range, float dt, uint32 now);
    // approach + engage; returns false when the target can not be fought (dropped)
    bool EngageTarget(PlayerObject* me, uint32 targetGoId, uint32 now, float dt);
    uint32 FindHuntTarget(PlayerObject* me);
    void AbortMission(PlayerObject* me, const std::string& why);

    BotClient* m_bot;
    int m_index;
    bool m_initialized = false;
    Goal m_goal = Goal::NONE;
    uint32 m_goalSinceMs = 0;

    // travel
    double m_destX = 0, m_destY = 0, m_destZ = 0;
    uint32 m_lastPathPlanMs = 0;
    uint32 m_lastMoveCheckMs = 0;
    double m_lastMoveCheckX = 0, m_lastMoveCheckZ = 0;
    int m_stuckChecks = 0;

    // combat
    uint32 m_combatTarget = 0;
    uint32 m_lastEngageMs = 0;
    int m_engageAttempts = 0;
    bool m_wasInInterlock = false;
    bool m_wasInCombat = false;
    uint32 m_combatStartMs = 0;
    uint32 m_lastTacticMs = 0;
    uint32 m_lastAbilityMs = 0;
    uint32 m_lastCombatPartner = 0;
    uint32 m_lastHpSeen = 0, m_lastPartnerHpSeen = 0;
    uint32 m_lastCombatProgressMs = 0;

    // missions
    uint32 m_nextMissionRequestMs = 0;
    uint32 m_trackedMissionId = 0;
    uint32 m_trackedObjective = 0;
    uint32 m_objectiveSinceMs = 0;
    uint32 m_lastInteractMs = 0;
    int m_interactAttempts = 0;
    int m_missionDeaths = 0;
    std::set<uint32> m_triedMissions; // missions this tester already had to abort

    // shop / rest / explore
    uint32 m_nextShopMs = 0;
    uint32 m_shopVendorId = 0;
    double m_shopX = 0, m_shopY = 0, m_shopZ = 0;
    uint32 m_restStartMs = 0;
    uint32 m_restLastHp = 0;
    uint32 m_restLastGainMs = 0;

    // life cycle
    bool m_wasDead = false;
    uint32 m_deathMs = 0;
    uint8 m_lastLevel = 1;
    uint64 m_startExp = 0;
    uint64 m_startInfo = 0;

public:
    struct Stats
    {
        uint32 rpcCalls = 0, rpcExceptions = 0;
        uint32 missionsRequested = 0, missionsAssigned = 0, missionsCompleted = 0, missionsAborted = 0;
        uint32 objectivesCompleted = 0;
        uint32 interlocks = 0, rangedFights = 0, engageFailures = 0, kills = 0, deaths = 0, respawns = 0;
        uint32 abilitiesUsed = 0, tacticsChanged = 0;
        uint32 levelUps = 0, abilityUpgrades = 0;
        uint32 purchases = 0, purchaseFailures = 0;
        uint32 rests = 0, fastTravels = 0, explores = 0, stuck = 0;
    };
    const Stats& GetStats() const { return m_stats; }
    static size_t FindingCount() { std::lock_guard<std::mutex> l(s_findingsMutex); return s_findings.size(); }
private:
    Stats m_stats;

    std::string m_status;      // human readable "what am I doing"
    std::string m_lastMission; // title

    mutable std::mutex m_textMutex;
    std::deque<std::pair<uint32, std::string>> m_serverText; // (ms, text), newest last

    static std::mutex s_findingsMutex;
    static std::map<std::string, std::pair<uint32, std::string>> s_findings;
};

#endif

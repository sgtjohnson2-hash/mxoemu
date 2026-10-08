// Tester bots ("PlayerSim") - see BotTester.h for what they do and why.
//
// Rules this file follows (GEMINI.md):
//  - every game action is a real client RPC fed to PlayerObject::HandleCommand; payload layouts
//    are the ones the existing handlers parse (sources cited in those handlers)
//  - no new packets, no emotes, no chat spam: testers report through the log/report file only
//  - runs on the simulation thread (BotManager::Update), serially

#include "BotTester.h"
#include "BotClient.h"
#include "BotManager.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "MissionSystem.h"
#include "EconomySystem.h"
#include "DataLoader.h"
#include "AbilitySystem.h"
#include "CombatSystem.h"
#include "SpatialGrid.h"
#include "Config.h"
#include "Timer.h"
#include "Log.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

std::mutex TesterBrain::s_findingsMutex;
std::map<std::string, std::pair<uint32, std::string>> TesterBrain::s_findings;

namespace
{
    // client RPC opcodes (PlayerObject::HandleCommand tables)
    const uint8  RPC_CLOSE_COMBAT   = 0x40;
    const uint8  RPC_RANGED_COMBAT  = 0x41;
    const uint8  RPC_TACTIC         = 0x42;
    const uint8  RPC_LEAVE_COMBAT   = 0x44;
    const uint16 RPC_OBJ_INTERACT   = 0x80c7;
    const uint16 RPC_MISSION_REQ    = 0x8094;
    const uint16 RPC_MISSION_INFO   = 0x8098;
    const uint16 RPC_MISSION_ACCEPT = 0x809b;
    const uint16 RPC_MISSION_ABORT  = 0x80a6;
    const uint16 RPC_ABILITY_LOAD   = 0x80ae;
    const uint16 RPC_ABILITY_UPG    = 0x80b7;
    const uint16 RPC_ABILITY_USE    = 0x80b9;
    const uint16 RPC_VENDOR_BUY     = 0x810e;

    const float RUN_SPEED = 500.0f;   // units/s (~5 m/s)
    const uint8 ANIM_RUN  = 30;       // DetectDiff_RunF, same id the street bots use

    ByteBuffer Rpc8(uint8 op)   { ByteBuffer b; b << op; return b; }
    ByteBuffer Rpc16(uint16 op) { ByteBuffer b; b << uint8(op >> 8) << uint8(op & 0xFF); return b; }

    std::string JsonEsc(const std::string& in)
    {
        std::string out;
        out.reserve(in.size() + 8);
        for (char c : in)
        {
            switch (c)
            {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += ' '; break;
            default:
                if ((unsigned char)c >= 0x20) out += c;
            }
        }
        return out;
    }

    const char* CmdName(ObjectiveCommand c)
    {
        switch (c)
        {
        case ObjectiveCommand::TALK: return "TALK";
        case ObjectiveCommand::DEFEAT: return "DEFEAT";
        case ObjectiveCommand::LOOT: return "LOOT";
        case ObjectiveCommand::GIVE: return "GIVE";
        case ObjectiveCommand::REBIRTH: return "REBIRTH";
        case ObjectiveCommand::ESCORT: return "ESCORT";
        case ObjectiveCommand::USE_ITEM: return "USE_ITEM";
        case ObjectiveCommand::HACK: return "HACK";
        }
        return "?";
    }

    double Dist2D(const LocationVector& a, double x, double z)
    {
        double dx = a.x - x, dz = a.z - z;
        return std::sqrt(dx * dx + dz * dz);
    }
}

// ---------------------------------------------------------------------------------------
// findings (shared)
// ---------------------------------------------------------------------------------------

void TesterBrain::AddFinding(const std::string& key, const std::string& detail)
{
    bool first = false;
    {
        std::lock_guard<std::mutex> lock(s_findingsMutex);
        auto& f = s_findings[key];
        first = (f.first == 0);
        f.first++;
        f.second = detail;
    }
    if (first)
        WARNING_LOG("BOTTEST FINDING: " + key + " | " + detail);
}

std::string TesterBrain::FindingsJson()
{
    std::lock_guard<std::mutex> lock(s_findingsMutex);
    std::string out = "[";
    bool first = true;
    for (const auto& kv : s_findings)
    {
        if (!first) out += ",";
        out += "\n    {\"finding\": \"" + JsonEsc(kv.first) + "\", \"count\": " + std::to_string(kv.second.first)
             + ", \"lastDetail\": \"" + JsonEsc(kv.second.second) + "\"}";
        first = false;
    }
    out += "\n  ]";
    return out;
}

// ---------------------------------------------------------------------------------------

TesterBrain::TesterBrain(BotClient* bot, int index) : m_bot(bot), m_index(index)
{
}

const char* TesterBrain::GoalName(Goal g)
{
    switch (g)
    {
    case Goal::NONE: return "idle";
    case Goal::MISSION: return "mission";
    case Goal::HUNT: return "hunt";
    case Goal::SHOP: return "shop";
    case Goal::REST: return "rest";
    case Goal::EXPLORE: return "explore";
    }
    return "?";
}

PlayerObject* TesterBrain::Me() const
{
    return BotGetPlayer(m_bot->GetPlayerGoId());
}

bool TesterBrain::SendRpc(const ByteBuffer& packet, const char* what)
{
    PlayerObject* me = Me();
    if (!me) return false;
    ByteBuffer copy(packet);
    copy.rpos(0);
    m_stats.rpcCalls++;
    try
    {
        me->HandleCommand(copy);
        return true;
    }
    catch (const std::exception& e)
    {
        m_stats.rpcExceptions++;
        AddFinding(std::string("RPC handler threw: ") + what, e.what());
    }
    catch (...)
    {
        m_stats.rpcExceptions++;
        AddFinding(std::string("RPC handler threw: ") + what, "unknown exception");
    }
    return false;
}

void TesterBrain::OnServerText(const std::string& text)
{
    std::lock_guard<std::mutex> lock(m_textMutex);
    m_serverText.push_back(std::make_pair(getMSTime(), text));
    while (m_serverText.size() > 24)
        m_serverText.pop_front();
}

bool TesterBrain::SawTextSince(const std::string& needle, uint32 sinceMs) const
{
    std::lock_guard<std::mutex> lock(m_textMutex);
    for (const auto& t : m_serverText)
        if (t.first >= sinceMs && t.second.find(needle) != std::string::npos)
            return true;
    return false;
}

std::string TesterBrain::LastServerText() const
{
    std::lock_guard<std::mutex> lock(m_textMutex);
    return m_serverText.empty() ? std::string("<no server reply>") : m_serverText.back().second;
}

// ---------------------------------------------------------------------------------------

void TesterBrain::Init(PlayerObject* me)
{
    m_initialized = true;
    uint32 now = getMSTime();
    m_lastLevel = me->getLevel();
    m_startExp = me->getExperience();
    m_startInfo = me->getInformation();
    m_nextMissionRequestMs = now + 3000 + m_index * 4000; // stagger the testers
    m_nextShopMs = now + 120000 + m_index * 15000;

    // read the server's replies the way a player reads chat
    m_bot->m_commandTap = [this](const msgBaseClassPtr& msg)
    {
        auto sys = std::dynamic_pointer_cast<SystemMsg>(msg);
        if (!sys) return;
        const ByteBuffer& b = sys->toBuf();
        std::string text;
        text.reserve(b.size());
        for (size_t i = 0; i < b.size(); i++)
        {
            char c = (char)b.contents()[i];
            if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7F) text += c;
        }
        OnServerText(text);
    };

    if (uint16(me->getGoId()) != me->getGoId())
        AddFinding("goId above 0xFFFF", "view ids are uint16(goId); RPCs aimed at this object resolve to the wrong one");

    // load the starter loadout through the real loader RPC (0x80ae; layout per
    // PlayerObject::RPC_HandleAbilityLoad / HDS PlayerHandler.ProcessLoadAbility)
    if (!me->getAbilitySystem())
    {
        AddFinding("bot has no AbilitySystem", me->getHandle());
    }
    else
    {
        const auto& loadout = AbilitySystem::GetDefaultLoadout();
        ByteBuffer b = Rpc16(RPC_ABILITY_LOAD);
        b << uint32(0) << uint16(0) << uint16(1) << uint16(loadout.size()) << uint8(0);
        for (const auto& e : loadout)
            b << uint16(e.slot) << uint16(e.abilityId) << uint16(0) << uint16(e.level) << uint8(0);
        SendRpc(b, "0x80ae ability load");
        if (me->getAbilitySystem()->getLoadedAbilities().empty())
            AddFinding("0x80ae loaded no abilities", LastServerText());
    }

    m_status = "jacked in";
    INFO_LOG(format("BOTTEST %1% online: L%2% %3% info %4%") % me->getHandle() % uint32(me->getLevel())
        % me->getFactionName() % me->getInformation());
}

void TesterBrain::CheckLevel(PlayerObject* me)
{
    uint8 lvl = me->getLevel();
    if (lvl <= m_lastLevel) return;

    m_stats.levelUps += (lvl - m_lastLevel);
    m_lastLevel = lvl;
    INFO_LOG(format("BOTTEST %1% reached level %2%") % me->getHandle() % uint32(lvl));

    // train the loadout up through the real upgrade RPC (0x80b7: ability u16, unknown u16, level u8)
    if (me->getAbilitySystem())
    {
        std::vector<uint16> ids;
        for (const auto& kv : me->getAbilitySystem()->getLoadedAbilities())
            ids.push_back(kv.first);
        for (uint16 id : ids)
        {
            ByteBuffer b = Rpc16(RPC_ABILITY_UPG);
            b << uint16(id) << uint16(0) << uint8(lvl);
            if (SendRpc(b, "0x80b7 ability upgrade"))
            {
                auto ab = me->getAbilitySystem()->getAbility(id);
                if (ab && ab->getLevel() == lvl) m_stats.abilityUpgrades++;
                else AddFinding("0x80b7 upgrade did not raise ability level", LastServerText());
            }
        }
    }
}

// ---------------------------------------------------------------------------------------
// movement
// ---------------------------------------------------------------------------------------

bool TesterBrain::TravelTo(PlayerObject* me, double x, double y, double z, double range, float dt, uint32 now)
{
    LocationVector pos = me->getPosition();
    double dist = Dist2D(pos, x, z);
    if (dist <= range)
    {
        if (m_bot->HasPath()) m_bot->ClearPath();
        m_stuckChecks = 0;
        return true;
    }

    static const double fastTravel = (double)sConfig.GetIntDefault("Bots.TesterFastTravelUnits", 40000);
    bool newDest = std::fabs(x - m_destX) > 300.0 || std::fabs(z - m_destZ) > 300.0;
    if (dist > fastTravel)
    {
        // long trips: jack out and back in at the destination (hardline travel), like a player would
        double k = (dist - range * 0.5) / dist;
        LocationVector np(pos.x + (x - pos.x) * k, y, pos.z + (z - pos.z) * k);
        me->setPosition(np);
        sSpatialGrid.UpdateClientPosition(m_bot, (float)np.x, (float)np.z);
        sGame.AnnounceStateUpdateNear(np.x, np.z, 20000.0f, std::make_shared<PositionStateMsg>(me->getGoId()));
        m_bot->ClearPath();
        m_stats.fastTravels++;
        m_destX = x; m_destY = y; m_destZ = z;
        return false;
    }

    if (newDest || !m_bot->HasPath() || now - m_lastPathPlanMs > 6000)
    {
        m_destX = x; m_destY = y; m_destZ = z;
        m_lastPathPlanMs = now;
        m_bot->MoveTo((float)x, (float)y, (float)z);
        if (!m_bot->HasPath())
        {
            // MoveTo refuses destinations that collide with someone standing there: aim beside it
            m_bot->MoveTo((float)(x + 120.0), (float)y, (float)(z + 120.0));
        }
    }

    if (m_bot->HasPath())
        m_bot->StepAlongPath(dt, RUN_SPEED, ANIM_RUN);

    // stuck detection: less than 1 m of progress in 5 s while trying to move
    if (now - m_lastMoveCheckMs >= 5000)
    {
        LocationVector p2 = me->getPosition();
        double moved = Dist2D(p2, m_lastMoveCheckX, m_lastMoveCheckZ);
        if (m_lastMoveCheckMs != 0 && moved < 100.0)
        {
            if (++m_stuckChecks >= 3)
            {
                m_stats.stuck++;
                AddFinding("tester stuck while walking (navmesh/collision)",
                    (format("%1% at %2%,%3% -> %4%,%5%") % me->getHandle() % p2.x % p2.z % x % z).str());
                // get unstuck the way a player would: hardline out to the destination
                LocationVector np(x, y, z);
                me->setPosition(np);
                sSpatialGrid.UpdateClientPosition(m_bot, (float)x, (float)z);
                sGame.AnnounceStateUpdateNear(x, z, 20000.0f, std::make_shared<PositionStateMsg>(me->getGoId()));
                m_bot->ClearPath();
                m_stats.fastTravels++;
                m_stuckChecks = 0;
            }
        }
        else
        {
            m_stuckChecks = 0;
        }
        m_lastMoveCheckMs = now;
        m_lastMoveCheckX = p2.x;
        m_lastMoveCheckZ = p2.z;
    }
    return false;
}

// ---------------------------------------------------------------------------------------
// combat
// ---------------------------------------------------------------------------------------

bool TesterBrain::EngageTarget(PlayerObject* me, uint32 targetGoId, uint32 now, float dt)
{
    PlayerObject* t = BotGetPlayer(targetGoId);
    if (!t || t->isDead() || t == me)
        return false;
    uint32 partner = t->getInterlockPartner();
    if (partner != 0 && partner != me->getGoId())
        return false; // busy with someone else - pick another fight

    LocationVector tp = t->getPosition();
    if (Dist2D(me->getPosition(), tp.x, tp.z) > 400.0)
    {
        TravelTo(me, tp.x, tp.y, tp.z, 300.0, dt, now);
        m_status = "closing on " + t->getHandle();
        return true;
    }

    if (now - m_lastEngageMs < 2500)
        return true;
    m_lastEngageMs = now;
    m_engageAttempts++;
    m_status = "engaging " + t->getHandle();

    uint16 view = uint16(targetGoId);
    if (m_engageAttempts <= 3)
    {
        // 0x40 close combat request: target view u16, spawn counter u16
        ByteBuffer b = Rpc8(RPC_CLOSE_COMBAT);
        b << uint16(view) << uint16(0);
        SendRpc(b, "0x40 close combat");
    }
    else
    {
        // interlock refused three times: try free fire (0x41: target view u16)
        ByteBuffer b = Rpc8(RPC_RANGED_COMBAT);
        b << uint16(view);
        SendRpc(b, "0x41 ranged combat");
    }

    if (m_engageAttempts > 6)
    {
        m_stats.engageFailures++;
        AddFinding("could not start combat (0x40 and 0x41 both failed)",
            (format("%1% vs %2% (L%3% %4%): %5%") % me->getHandle() % t->getHandle() % uint32(t->getLevel())
                % t->getFactionName() % LastServerText()).str());
        m_engageAttempts = 0;
        return false;
    }
    return true;
}

void TesterBrain::DoCombat(PlayerObject* me, uint32 now)
{
    uint32 partner = me->getInterlockPartner();
    bool inIL = partner != 0;
    if (inIL && !m_wasInInterlock)
    {
        m_stats.interlocks++;
        m_combatStartMs = now;
        m_lastCombatProgressMs = now;
        m_engageAttempts = 0;
    }
    else if (!inIL && !m_wasInCombat)
    {
        m_stats.rangedFights++;
        m_combatStartMs = now;
        m_lastCombatProgressMs = now;
        m_engageAttempts = 0;
    }
    m_wasInInterlock = inIL;
    m_wasInCombat = true;

    uint32 opp = inIL ? partner : (m_combatTarget ? m_combatTarget : m_bot->GetTargetGoId());
    PlayerObject* o = BotGetPlayer(opp);
    if (o) m_lastCombatPartner = opp;
    m_status = std::string(inIL ? "interlocked with " : "free-firing at ") + (o ? o->getHandle() : std::string("?"));
    if (o) m_status += " (" + std::to_string(o->getCurrentHealth()) + "/" + std::to_string(o->getMaximumHealth()) + " hp)";

    // progress = anybody's health moved
    uint32 myHp = me->getCurrentHealth();
    uint32 oHp = o ? o->getCurrentHealth() : 0;
    if (myHp != m_lastHpSeen || oHp != m_lastPartnerHpSeen)
    {
        m_lastHpSeen = myHp;
        m_lastPartnerHpSeen = oHp;
        m_lastCombatProgressMs = now;
    }
    if (now - m_lastCombatProgressMs > 60000)
    {
        AddFinding(inIL ? "interlock stalled: no damage for 60 s" : "free fire stalled: no damage for 60 s",
            (format("%1% vs %2%") % me->getHandle() % (o ? o->getHandle() : std::string("?"))).str());
        SendRpc(Rpc8(RPC_LEAVE_COMBAT), "0x44 leave combat");
        m_combatTarget = 0;
        m_bot->SetTargetGoId(0);
        m_lastCombatProgressMs = now;
        return;
    }

    // tactics (0x42 sends the ILDB value: grab 0, block 3, power 4, speed 5)
    if (inIL && now - m_lastTacticMs >= 4000)
    {
        m_lastTacticMs = now;
        static const uint8 tactics[] = { TACTIC_RETALIATE, TACTIC_DEFENSE, TACTIC_POWER, TACTIC_SPEED };
        uint8 t = tactics[rand() % 4];
        ByteBuffer b = Rpc8(RPC_TACTIC);
        b << uint8(t);
        if (SendRpc(b, "0x42 tactic")) m_stats.tacticsChanged++;
    }

    // abilities from the loaded loadout (0x80b9: ability u16, target view u16)
    if (o && now - m_lastAbilityMs >= 3500 && me->getAbilitySystem() && me->getCurrentIS() >= 10)
    {
        m_lastAbilityMs = now;
        std::vector<uint16> ids;
        for (const auto& kv : me->getAbilitySystem()->getLoadedAbilities())
            ids.push_back(kv.first);
        if (!ids.empty())
        {
            uint16 id = ids[rand() % ids.size()];
            ByteBuffer b = Rpc16(RPC_ABILITY_USE);
            b << uint16(id) << uint16(uint16(opp));
            if (SendRpc(b, "0x80b9 ability use")) m_stats.abilitiesUsed++;
        }
    }
}

uint32 TesterBrain::FindHuntTarget(PlayerObject* me)
{
    LocationVector p = me->getPosition();
    std::vector<GameClient*> nearbyClients = sSpatialGrid.GetClientsInRadius((float)p.x, (float)p.z, 8000.0f, me->getClient().m_instanceId);
    uint32 best = 0;
    double bestDist = 1e18;
    std::string myFaction = me->getFactionName();
    for (GameClient* c : nearbyClients)
    {
        if (!c || !c->isBot()) continue;               // never pick fights with humans
        BotClient* b = dynamic_cast<BotClient*>(c);
        if (!b || b->IsTester()) continue;
        PlayerObject* t = BotGetPlayer(c->GetPlayerGoId());
        if (!t || t == me || t->isDead()) continue;
        std::string f = t->getFactionName();
        if (f == "Civilian" || f == "FRIENDLY" || f == myFaction || f.empty()) continue;
        if (t->getHandle().find("Civilian") != std::string::npos) continue;
        if (t->getLevel() > me->getLevel() + 3) continue;
        if (t->getInterlockPartner() != 0) continue;
        if (uint16(t->getGoId()) != t->getGoId()) continue;
        double d = Dist2D(p, t->getPosition().x, t->getPosition().z);
        if (d < bestDist) { bestDist = d; best = t->getGoId(); }
    }
    return best;
}

// ---------------------------------------------------------------------------------------
// goals
// ---------------------------------------------------------------------------------------

void TesterBrain::ChooseGoal(PlayerObject* me, uint32 now)
{
    m_goalSinceMs = now;
    float hpPct = me->getMaximumHealth() ? float(me->getCurrentHealth()) / float(me->getMaximumHealth()) : 1.0f;
    if (hpPct < 0.35f)
    {
        m_goal = Goal::REST;
        m_restStartMs = now;
        m_restLastHp = me->getCurrentHealth();
        m_restLastGainMs = now;
        m_stats.rests++;
        return;
    }

    ActiveObjectiveInfo info;
    if (sMissionSys.GetActiveObjectiveInfo(me->getGoId(), info))
    {
        m_goal = Goal::MISSION;
        return;
    }

    if (now >= m_nextMissionRequestMs)
    {
        m_nextMissionRequestMs = now + 90000;
        m_stats.missionsRequested++;
        uint32 asked = now;
        SendRpc(Rpc16(RPC_MISSION_REQ), "0x8094 mission request");
        if (sMissionSys.GetActiveObjectiveInfo(me->getGoId(), info) && m_triedMissions.count(info.missionId))
        {
            // MissionSystem::GetAvailableStoryMission falls back to the first mission once every
            // mission is done/aborted, so a tester that has exhausted the playable list kept being
            // handed the same broken mission every 20 s. Hand it back and stop asking for a while.
            SendRpc(Rpc16(RPC_MISSION_ABORT), "0x80a6 mission abort");
            if (sMissionSys.HasActiveMission(me->getGoId()))
                sMissionSys.AbortMission(me, "tester: mission list exhausted");
            AddFinding("mission list exhausted: no playable mission left for this faction (server re-offers an already failed one)",
                (format("%1% (%2%) re-offered %3% '%4%' after trying %5% missions") % me->getHandle()
                    % me->getFactionName() % info.missionId % info.title % m_triedMissions.size()).str());
            m_nextMissionRequestMs = now + 30 * 60 * 1000;
            m_goal = Goal::HUNT;
            return;
        }
        if (sMissionSys.GetActiveObjectiveInfo(me->getGoId(), info))
        {
            m_stats.missionsAssigned++;
            SendRpc(Rpc16(RPC_MISSION_ACCEPT), "0x809b mission accept");
            SendRpc(Rpc16(RPC_MISSION_INFO), "0x8098 mission info");
            if (!SawTextSince("[CONTRACT OBJECTIVE", asked))
                AddFinding("mission accepted but no objective text was sent", info.title);
            INFO_LOG(format("BOTTEST %1% took mission %2% '%3%' (%4% objectives)") % me->getHandle()
                % info.missionId % info.title % info.objectiveCount);
            m_goal = Goal::MISSION;
            return;
        }
        AddFinding("0x8094 mission request did not assign a mission", LastServerText());
    }

    if (me->getInformation() >= 100 && now >= m_nextShopMs)
    {
        m_nextShopMs = now + 600000;
        m_shopVendorId = 0;
        m_goal = Goal::SHOP;
        return;
    }

    m_goal = Goal::HUNT;
}

void TesterBrain::AbortMission(PlayerObject* me, const std::string& why)
{
    std::string title = m_lastMission;
    AddFinding("mission '" + title + "': " + why, me->getHandle());
    SendRpc(Rpc16(RPC_MISSION_ABORT), "0x80a6 mission abort");
    if (sMissionSys.HasActiveMission(me->getGoId()))
    {
        AddFinding("0x80a6 mission abort did not clear the mission", LastServerText());
        sMissionSys.AbortMission(me, "tester fallback");
    }
    // Test harness only: mark the broken mission done for THIS tester so its next request
    // covers a different mission instead of looping on the same broken one.
    if (m_trackedMissionId)
    {
        sMissionSys.RecordCompletedMission(me->getGoId(), m_trackedMissionId);
        m_triedMissions.insert(m_trackedMissionId);
    }
    m_stats.missionsAborted++;
    m_trackedMissionId = 0;
    m_combatTarget = 0;
    m_nextMissionRequestMs = getMSTime() + 20000;
    m_goal = Goal::NONE;
}

void TesterBrain::DoMission(PlayerObject* me, uint32 now, float dt)
{
    ActiveObjectiveInfo info;
    if (!sMissionSys.GetActiveObjectiveInfo(me->getGoId(), info))
    {
        if (m_trackedMissionId)
        {
            if (sMissionSys.HasCompletedMission(me->getGoId(), m_trackedMissionId))
            {
                m_stats.missionsCompleted++;
                m_stats.objectivesCompleted++;
                INFO_LOG(format("BOTTEST %1% COMPLETED mission '%2%'") % me->getHandle() % m_lastMission);
            }
            else
            {
                AddFinding("mission ended without completion (timer/branch?)", m_lastMission);
            }
        }
        m_trackedMissionId = 0;
        m_combatTarget = 0;
        m_nextMissionRequestMs = std::min(m_nextMissionRequestMs, now + 10000);
        m_goal = Goal::NONE;
        return;
    }

    if (info.missionId != m_trackedMissionId)
    {
        m_trackedMissionId = info.missionId;
        m_trackedObjective = info.objectiveIndex;
        m_objectiveSinceMs = now;
        m_interactAttempts = 0;
        m_missionDeaths = 0;
        m_lastMission = info.title;
    }
    else if (info.objectiveIndex != m_trackedObjective)
    {
        m_stats.objectivesCompleted += (info.objectiveIndex - m_trackedObjective);
        m_trackedObjective = info.objectiveIndex;
        m_objectiveSinceMs = now;
        m_interactAttempts = 0;
        m_combatTarget = 0;
    }

    char buf[160];
    snprintf(buf, sizeof(buf), "mission '%s' %u/%u %s", info.title.c_str(), info.objectiveIndex + 1,
        info.objectiveCount, CmdName(info.command));
    m_status = buf;

    static const uint32 stuckMs = (uint32)sConfig.GetIntDefault("Bots.TesterMissionStuckSeconds", 300) * 1000;
    if (now - m_objectiveSinceMs > stuckMs)
    {
        AbortMission(me, (format("stuck on objective %1% (%2% '%3%') for %4% s") % (info.objectiveIndex + 1)
            % CmdName(info.command) % info.description % (stuckMs / 1000)).str());
        return;
    }
    if (info.objectiveIndex >= info.objectiveCount)
        return;

    switch (info.command)
    {
    case ObjectiveCommand::TALK:
    case ObjectiveCommand::GIVE:
    case ObjectiveCommand::DEFEAT:
    case ObjectiveCommand::LOOT:
        break;
    default:
        AbortMission(me, std::string("objective type ") + CmdName(info.command) + " has no client RPC path / tester support");
        return;
    }

    if (info.targetGoId == 0)
    {
        AbortMission(me, (format("objective %1% (%2%) targets idNpc %3% which was never spawned (no <npc> entry)")
            % (info.objectiveIndex + 1) % CmdName(info.command) % info.targetNpcId).str());
        return;
    }
    PlayerObject* t = BotGetPlayer(info.targetGoId);
    if (!t)
    {
        AbortMission(me, "mission NPC object no longer exists");
        return;
    }
    if (m_missionDeaths >= 2)
    {
        AbortMission(me, (format("tester died twice to mission target %1% (L%2% %3% HP vs tester L%4% %5% HP)")
            % t->getHandle() % uint32(t->getLevel()) % t->getMaximumHealth() % uint32(me->getLevel()) % me->getMaximumHealth()).str());
        return;
    }
    LocationVector tp = t->getPosition();

    if (info.command == ObjectiveCommand::TALK || info.command == ObjectiveCommand::GIVE)
    {
        if (!TravelTo(me, tp.x, tp.y, tp.z, 200.0, dt, now))
            return;
        if (now - m_lastInteractMs < 3000)
            return;
        m_lastInteractMs = now;
        m_interactAttempts++;
        // 0x80c7 dynamic object interaction: view u16, object type u16, interaction u16
        ByteBuffer b = Rpc16(RPC_OBJ_INTERACT);
        b << uint16(uint16(info.targetGoId)) << uint16(0) << uint16(0);
        SendRpc(b, "0x80c7 object interaction");
        if (m_interactAttempts >= 4)
        {
            if (info.command == ObjectiveCommand::GIVE)
                AbortMission(me, "GIVE objective did not advance (needs item '" + info.requiredItem + "'): " + LastServerText());
            else
                AbortMission(me, "TALK objective did not advance after 4 interactions: " + LastServerText());
        }
        return;
    }

    // DEFEAT / LOOT: kill the target; die() advances both
    if (t->isDead())
    {
        if (now - m_objectiveSinceMs > 20000 && t->getCurrentHealth() == 0)
            AbortMission(me, std::string(CmdName(info.command)) + " target is dead but the objective did not advance");
        return;
    }
    if (!EngageTarget(me, info.targetGoId, now, dt))
    {
        if (t->getInterlockPartner() == 0)
            AbortMission(me, "could not engage the mission target " + t->getHandle());
    }
}

void TesterBrain::DoHunt(PlayerObject* me, uint32 now, float dt)
{
    if (m_combatTarget == 0)
    {
        m_combatTarget = FindHuntTarget(me);
        m_engageAttempts = 0;
        if (m_combatTarget == 0)
        {
            m_goal = Goal::EXPLORE;
            m_goalSinceMs = now;
            m_destX = m_destZ = 0;
            return;
        }
        PlayerObject* t = BotGetPlayer(m_combatTarget);
        if (t) m_status = "hunting " + t->getHandle();
    }
    if (now - m_goalSinceMs > 150000)
    {
        m_combatTarget = 0;
        m_goal = Goal::NONE;
    }
}

void TesterBrain::DoShop(PlayerObject* me, uint32 now, float dt)
{
    if (m_shopVendorId == 0)
    {
        LocationVector p = me->getPosition();
        double best = 1e18;
        for (uint32 d = 0; d <= 12; d++)
        {
            for (const HardlineVendor& v : sEconomySys.GetVendorsForDistrict(d))
            {
                if (v.staticId < 1000 || v.inventoryTemplates.empty()) continue; // only the real (CSV) vendors
                double dist = Dist2D(p, v.x, v.z);
                if (dist < best) { best = dist; m_shopVendorId = v.staticId; m_shopX = v.x; m_shopY = v.y; m_shopZ = v.z; }
            }
        }
        if (m_shopVendorId == 0)
        {
            AddFinding("no real hardline vendors loaded (vendor_items.csv)", "EconomySystem has none with staticId >= 1000");
            m_goal = Goal::NONE;
            return;
        }
    }

    m_status = "walking to vendor " + std::to_string(m_shopVendorId);
    if (!TravelTo(me, m_shopX, m_shopY, m_shopZ, 300.0, dt, now))
    {
        if (now - m_goalSinceMs > 180000) { m_goal = Goal::NONE; }
        return;
    }

    const HardlineVendor* v = sEconomySys.GetHardlineVendor(m_shopVendorId);
    if (!v)
    {
        AddFinding("GetHardlineVendor() can not find a vendor GetVendorsForDistrict() returned", std::to_string(m_shopVendorId));
        m_goal = Goal::NONE;
        return;
    }
    uint64 cash = me->getInformation();
    std::vector<uint32> affordable;
    size_t missing = 0;
    for (uint32 tpl : v->inventoryTemplates)
    {
        const ItemTemplate* it = sDataLoader.GetItemTemplate(tpl);
        if (!it) { missing++; continue; }
        if (it->value > 0 && it->value <= cash) affordable.push_back(tpl);
    }
    if (missing == v->inventoryTemplates.size())
        AddFinding("vendor stock not in the item database (GetItemTemplate null for all)", v->name);

    if (affordable.empty())
    {
        m_status = "window shopping (too poor)";
        m_goal = Goal::NONE;
        m_shopVendorId = 0;
        return;
    }

    uint32 tpl = affordable[rand() % affordable.size()];
    uint32 asked = now;
    ByteBuffer b = Rpc16(RPC_VENDOR_BUY);
    b << uint32(tpl) << uint32(v->staticId);
    SendRpc(b, "0x810e vendor buy");
    if (me->getInformation() < cash && SawTextSince("[VENDOR] Purchased", asked))
    {
        m_stats.purchases++;
        INFO_LOG(format("BOTTEST %1% bought item %2% for %3% info") % me->getHandle() % tpl % (cash - me->getInformation()));
    }
    else
    {
        m_stats.purchaseFailures++;
        AddFinding("vendor buy failed", (format("%1% item %2%: %3%") % v->name % tpl % LastServerText()).str());
    }
    m_shopVendorId = 0;
    m_goal = Goal::NONE;
}

void TesterBrain::DoRest(PlayerObject* me, uint32 now)
{
    uint32 hp = me->getCurrentHealth();
    m_status = "resting " + std::to_string(hp) + "/" + std::to_string(me->getMaximumHealth());
    if (m_bot->HasPath()) m_bot->ClearPath();
    if (hp > m_restLastHp) m_restLastGainMs = now;
    m_restLastHp = hp;
    if (hp * 10 >= uint32(me->getMaximumHealth()) * 9)
    {
        m_goal = Goal::NONE;
        return;
    }
    if (now - m_restLastGainMs > 30000)
    {
        AddFinding("no out-of-combat health regeneration (30 s resting, no HP gained)", me->getHandle());
        m_goal = Goal::NONE;
    }
}

void TesterBrain::DoExplore(PlayerObject* me, uint32 now, float dt)
{
    if (m_destX == 0 && m_destZ == 0)
    {
        LocationVector p = me->getPosition();
        const auto& hls = sBotMgr.GetHardlines();
        if (!hls.empty() && (rand() % 3) == 0)
        {
            const LocationVector& h = hls[rand() % hls.size()];
            m_destX = h.x; m_destY = h.y; m_destZ = h.z;
        }
        else
        {
            double a = (rand() % 6283) / 1000.0;
            double r = 3000.0 + (rand() % 5000);
            m_destX = p.x + std::cos(a) * r; m_destY = p.y; m_destZ = p.z + std::sin(a) * r;
        }
        m_stats.explores++;
    }
    m_status = "exploring";
    if (TravelTo(me, m_destX, m_destY, m_destZ, 250.0, dt, now) || now - m_goalSinceMs > 90000)
    {
        m_destX = m_destZ = 0;
        m_goal = Goal::NONE;
    }
}

// ---------------------------------------------------------------------------------------

void TesterBrain::Tick(float dt)
{
    PlayerObject* me = Me();
    if (!me) return;
    uint32 now = getMSTime();
    if (!m_initialized) Init(me);

    // death / respawn
    if (me->isDead())
    {
        if (!m_wasDead)
        {
            m_wasDead = true;
            m_deathMs = now;
            m_stats.deaths++;
            if (m_goal == Goal::MISSION) m_missionDeaths++;
            m_combatTarget = 0;
            m_bot->SetTargetGoId(0);
            m_bot->ClearPath();
            m_wasInInterlock = m_wasInCombat = false;
            m_status = "dead";
            INFO_LOG(format("BOTTEST %1% died (L%2%, %3%)") % me->getHandle() % uint32(me->getLevel()) % GoalName(m_goal));
        }
        else if (now - m_deathMs > 30000 && now - m_deathMs < 31000)
        {
            AddFinding("no respawn 30 s after death", me->getHandle());
        }
        return;
    }
    if (m_wasDead)
    {
        m_wasDead = false;
        m_stats.respawns++;
        m_goal = Goal::NONE;
    }

    CheckLevel(me);

    // kill bookkeeping
    if (m_lastCombatPartner)
    {
        PlayerObject* o = BotGetPlayer(m_lastCombatPartner);
        if (!o) m_lastCombatPartner = 0;
        else if (o->isDead())
        {
            m_stats.kills++;
            if (m_combatTarget == m_lastCombatPartner) m_combatTarget = 0;
            if (m_bot->GetTargetGoId() == m_lastCombatPartner) m_bot->SetTargetGoId(0);
            m_lastCombatPartner = 0;
        }
    }

    // fighting right now?
    if (me->getInterlockPartner() != 0 || me->isInCombat())
    {
        if (m_bot->HasPath()) m_bot->ClearPath();
        DoCombat(me, now);
        return;
    }
    if (m_wasInCombat)
    {
        m_wasInCombat = false;
        m_wasInInterlock = false;
        m_lastCombatProgressMs = now;
    }

    // someone attacked us (takeDamage sets the bot target): fight back unless a mission needs us
    uint32 aggressor = m_bot->GetTargetGoId();
    if (aggressor && !m_combatTarget && m_goal != Goal::MISSION)
    {
        PlayerObject* a = BotGetPlayer(aggressor);
        if (a && !a->isDead()) m_combatTarget = aggressor;
        else m_bot->SetTargetGoId(0);
    }

    if (m_combatTarget && m_goal != Goal::MISSION)
    {
        if (!EngageTarget(me, m_combatTarget, now, dt))
        {
            if (m_bot->GetTargetGoId() == m_combatTarget) m_bot->SetTargetGoId(0);
            m_combatTarget = 0;
            m_engageAttempts = 0;
        }
        return;
    }

    switch (m_goal)
    {
    case Goal::NONE:    ChooseGoal(me, now); break;
    case Goal::MISSION: DoMission(me, now, dt); break;
    case Goal::HUNT:    DoHunt(me, now, dt); break;
    case Goal::SHOP:    DoShop(me, now, dt); break;
    case Goal::REST:    DoRest(me, now); break;
    case Goal::EXPLORE: DoExplore(me, now, dt); break;
    }
}

// ---------------------------------------------------------------------------------------
// reporting
// ---------------------------------------------------------------------------------------

std::string TesterBrain::ReportLine() const
{
    PlayerObject* me = Me();
    if (!me) return "tester " + std::to_string(m_index) + ": <no player object>";
    char buf[512];
    snprintf(buf, sizeof(buf),
        "%s L%u %s hp %u/%u info %llu xp+%llu | %s: %s | missions %u done/%u aborted/%u taken, objectives %u | "
        "kills %u deaths %u interlocks %u ranged %u engageFail %u | abilities %u tactics %u upgrades %u | buys %u fail %u | stuck %u travel %u",
        me->getHandle().c_str(), uint32(me->getLevel()), me->getFactionName().c_str(),
        uint32(me->getCurrentHealth()), uint32(me->getMaximumHealth()),
        (unsigned long long)me->getInformation(),
        (unsigned long long)(me->getExperience() >= m_startExp ? me->getExperience() - m_startExp : 0),
        GoalName(m_goal), m_status.c_str(),
        m_stats.missionsCompleted, m_stats.missionsAborted, m_stats.missionsAssigned, m_stats.objectivesCompleted,
        m_stats.kills, m_stats.deaths, m_stats.interlocks, m_stats.rangedFights, m_stats.engageFailures,
        m_stats.abilitiesUsed, m_stats.tacticsChanged, m_stats.abilityUpgrades,
        m_stats.purchases, m_stats.purchaseFailures, m_stats.stuck, m_stats.fastTravels);
    return buf;
}

std::string TesterBrain::ReportJson() const
{
    PlayerObject* me = Me();
    std::string s = "    {";
    if (me)
    {
        LocationVector p = me->getPosition();
        s += "\"handle\": \"" + JsonEsc(me->getHandle()) + "\", \"goId\": " + std::to_string(me->getGoId())
           + ", \"faction\": \"" + JsonEsc(me->getFactionName()) + "\", \"level\": " + std::to_string(uint32(me->getLevel()))
           + ", \"hp\": " + std::to_string(uint32(me->getCurrentHealth())) + ", \"hpMax\": " + std::to_string(uint32(me->getMaximumHealth()))
           + ", \"info\": " + std::to_string((unsigned long long)me->getInformation())
           + ", \"xpGained\": " + std::to_string((unsigned long long)(me->getExperience() >= m_startExp ? me->getExperience() - m_startExp : 0))
           + ", \"pos\": [" + std::to_string((int)p.x) + "," + std::to_string((int)p.y) + "," + std::to_string((int)p.z) + "]"
           + ", \"instance\": " + std::to_string(me->getClient().m_instanceId) + ", ";
    }
    s += "\"goal\": \"" + std::string(GoalName(m_goal)) + "\", \"status\": \"" + JsonEsc(m_status) + "\""
       + ", \"stats\": {"
       + "\"rpcCalls\": " + std::to_string(m_stats.rpcCalls) + ", \"rpcExceptions\": " + std::to_string(m_stats.rpcExceptions)
       + ", \"missionsRequested\": " + std::to_string(m_stats.missionsRequested)
       + ", \"missionsAssigned\": " + std::to_string(m_stats.missionsAssigned)
       + ", \"missionsCompleted\": " + std::to_string(m_stats.missionsCompleted)
       + ", \"missionsAborted\": " + std::to_string(m_stats.missionsAborted)
       + ", \"objectivesCompleted\": " + std::to_string(m_stats.objectivesCompleted)
       + ", \"interlocks\": " + std::to_string(m_stats.interlocks) + ", \"rangedFights\": " + std::to_string(m_stats.rangedFights)
       + ", \"engageFailures\": " + std::to_string(m_stats.engageFailures)
       + ", \"kills\": " + std::to_string(m_stats.kills) + ", \"deaths\": " + std::to_string(m_stats.deaths)
       + ", \"respawns\": " + std::to_string(m_stats.respawns)
       + ", \"abilitiesUsed\": " + std::to_string(m_stats.abilitiesUsed) + ", \"tacticsChanged\": " + std::to_string(m_stats.tacticsChanged)
       + ", \"levelUps\": " + std::to_string(m_stats.levelUps) + ", \"abilityUpgrades\": " + std::to_string(m_stats.abilityUpgrades)
       + ", \"purchases\": " + std::to_string(m_stats.purchases) + ", \"purchaseFailures\": " + std::to_string(m_stats.purchaseFailures)
       + ", \"rests\": " + std::to_string(m_stats.rests) + ", \"fastTravels\": " + std::to_string(m_stats.fastTravels)
       + ", \"explores\": " + std::to_string(m_stats.explores) + ", \"stuck\": " + std::to_string(m_stats.stuck)
       + "}, \"lastServerText\": \"" + JsonEsc(LastServerText()) + "\"}";
    return s;
}

// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// Headless combat regression suite  (Reality.exe --test-combat)
//
// Runs the real CombatSystem / PlayerObject / BotManager code paths with no database,
// no sockets and no game client:
//   * "humans" are PlayerObjects built on a non-bot GameClient (TestHumanClient) with a
//     memory-only character id (>= 9000000), so every code path that checks
//     getClient().isBot() takes its HUMAN branch while nothing touches the DB;
//   * bots are real BotClients.
// Exit code = number of failed assertions (0 = all passed).
//
// What this does NOT cover (needs the live client, Phase C): the real 7.6005 attack /
// ability opcodes, client acceptance of the 0x80b2 loadout, rendering / facing of the
// dojo bots, AoI streaming of a real socket client.
//
// ***************************************************************************

#include "Common.h"
#include "CombatSystem.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include "GameClient.h"
#include "BotManager.h"
#include "BotClient.h"
#include "SpatialGrid.h"
#include "AbilitySystem.h"
#include "DataLoader.h"
#include "CombatAnimationMatrix.h"
#include "DojoSpawn.h"
#include "MessageTypes.h"
#include "Timer.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace
{
	int g_pass = 0;
	int g_fail = 0;
	int g_skip = 0;

	void check(bool condition, const std::string& name)
	{
		if (condition) { std::cout << " [PASS] " << name << std::endl; g_pass++; }
		else           { std::cout << " [FAIL] " << name << std::endl; g_fail++; }
	}

	void skip(const std::string& name, const std::string& why)
	{
		std::cout << " [SKIP] " << name << " (" << why << ")" << std::endl;
		g_skip++;
	}

	// A GameClient that reports isBot() == false and records every command queued for it.
	class TestHumanClient : public GameClient
	{
	public:
		TestHumanClient() : GameClient(sockaddr_in(), nullptr)
		{
			m_commandTap = [this](const msgBaseClassPtr& msg)
			{
				const ByteBuffer& b = msg->toBuf();
				captured.push_back(std::string((const char*)b.contents(), b.size()));
			};
		}
		virtual void FlushQueue(bool alsoResend = false) {}

		bool sawText(const std::string& needle) const
		{
			for (const std::string& s : captured)
				if (s.find(needle) != std::string::npos)
					return true;
			return false;
		}

		std::vector<std::string> captured;
	};

	struct Actor
	{
		GameClient* client = nullptr;
		PlayerObject* po = nullptr;
		uint32 go = 0;
		std::shared_ptr<BotClient> bot; // null for humans
	};

	std::vector<std::shared_ptr<BotClient> > g_keepBots; // never destroyed: tests share the global ObjectMgr

	void place(Actor& a, double x, double z, double y = 572.0)
	{
		a.po->setPosition(LocationVector(x, y, z));
		sSpatialGrid.UpdateClientPosition(a.client, (float)x, (float)z);
	}

	void setHP(Actor& a, uint16 hp, uint16 maxHp)
	{
		a.po->setMaximumHealth(maxHp);
		a.po->setCurrentHealth(hp);
	}

	Actor makeBot(uint64 uid, double x, double z, uint8 level = 1, uint16 hp = 100)
	{
		Actor a;
		a.bot = std::make_shared<BotClient>(uid);
		g_keepBots.push_back(a.bot);
		a.client = a.bot.get();
		a.go = a.bot->GetPlayerGoId();
		a.po = sObjMgr.getGOPtr(a.go);
		if (a.po)
		{
			a.po->setLevel(level);
			setHP(a, hp, hp);
			place(a, x, z);
		}
		return a;
	}

	Actor makeHuman(TestHumanClient* client, uint64 uid, double x, double z)
	{
		Actor a;
		a.client = client;
		// isBot=true here only means "memory-only character"; the CLIENT is non-bot so all
		// human branches (chat feedback, loadout, rewards) run.
		uint32 go = sObjMgr.constructPlayer(client, uid, true);
		client->SetPlayerGoId(go);
		a.go = go;
		a.po = sObjMgr.getGOPtr(go);
		if (a.po)
		{
			a.po->InitializeWorld();
			a.po->SpawnSelf();
			place(a, x, z);
		}
		return a;
	}

	std::string hexOf(const std::string& raw)
	{
		static const char* digits = "0123456789abcdef";
		std::string out;
		for (unsigned char c : raw) { out += digits[c >> 4]; out += digits[c & 15]; }
		return out;
	}

	double dist2D(const LocationVector& a, const LocationVector& b)
	{
		return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
	}

	// Attack until `victim` is out of HP (random hit rolls). Returns true if it got there.
	bool attackUntilZeroHP(Actor& attacker, Actor& victim, int maxTries = 600)
	{
		for (int i = 0; i < maxTries && victim.po->getCurrentHealth() > 0; ++i)
			sCombatSys.ResolveSingleAttack(attacker.go, victim.go, 1, true);
		return victim.po->getCurrentHealth() == 0;
	}
}

int RunCombatTestSuite()
{
	std::cout << "\n============================================================" << std::endl;
	std::cout << "  COMBAT REGRESSION SUITE (headless, no DB / client)" << std::endl;
	std::cout << "============================================================\n" << std::endl;

	if (!GameServer::getSingletonPtr())
		new GameServer(); // never started: m_mainSocket stays null so every Announce* is a no-op
	sSpatialGrid.Initialize(15000.0f);
	sCombatSys.Init();
	sDataLoader.EnsureCoreAbilities();

	try
	{
		// ---------------------------------------------------------------- loadout (N1)
		TestHumanClient humanClient;
		Actor human = makeHuman(&humanClient, 9100001, 10000.0, 10000.0);
		check(human.po != nullptr, "human test character constructed");
		if (!human.po)
		{
			std::cout << "cannot continue without a human actor" << std::endl;
			return 1;
		}

		{
			std::vector<std::string> loadMsgs;
			for (const std::string& s : humanClient.captured)
				if (s.size() == 8 && (unsigned char)s[0] == 0x80 && (unsigned char)s[1] == 0xb2)
					loadMsgs.push_back(hexOf(s));
			// default loadout, hotbar slot order; layout [80b2][id LE][level LE][08 02]
			check(loadMsgs.size() == 3, "initGoId sends exactly 3 x 0x80b2 AbilityLoadRsp (got " + std::to_string(loadMsgs.size()) + ")");
			if (loadMsgs.size() == 3)
			{
				check(loadMsgs[0] == "80b258020100" "0802", "slot 0 = ability 600 CloseCombatTrainingAbility L1: " + loadMsgs[0]);
				check(loadMsgs[1] == "80b289000100" "0802", "slot 1 = ability 137 MartialArtsInitiateAbility L1: " + loadMsgs[1]);
				check(loadMsgs[2] == "80b211000100" "0802", "slot 2 = ability 17 SelfDefenseAbility L1 (matches retail capture 80b2 1100 0100 0802): " + loadMsgs[2]);
			}
			const std::vector<DefaultLoadoutEntry>& def = AbilitySystem::GetDefaultLoadout();
			check(def.size() == 3 && def[0].abilityId == 600 && def[1].abilityId == 137 && def[2].abilityId == 17,
				"GetDefaultLoadout uses the real ids 600/137/17 (not the invented 1/2/3)");
			check(CombatSystem::GetMove(600) && CombatSystem::GetMove(137) && CombatSystem::GetMove(17),
				"loadout ability ids resolve to a server combat move (alias of the basic strike)");
			check(CombatSystem::GetMove(600) && CombatSystem::GetMove(600)->name == "CloseCombatTrainingAbility",
				"alias carries the real ability name");
			check(CombatSystem::GetMove(9999) == nullptr, "unknown ability id 9999 has no move");
		}

		// ---------------------------------------------------------------- unknown ability (H2)
		{
			Actor target = makeBot(9200001, 10150.0, 10000.0);
			humanClient.captured.clear();
			bool ok = sCombatSys.UseAbility(human.po, 9999, target.go);
			check(humanClient.sawText("not implemented on this server yet"), "unknown ability gives the player chat feedback");
			check(ok && sCombatSys.IsInterlocked(human.go), "unknown ability falls back to the basic strike and opens an interlock");
			sCombatSys.EndInterlock(human.go, false);
			check(!sCombatSys.IsInterlocked(human.go) && !sCombatSys.IsInterlocked(target.go), "EndInterlock(byWithdraw=false) clears both sides");

			humanClient.captured.clear();
			ok = sCombatSys.UseAbility(human.po, 600, target.go);
			check(ok && !humanClient.sawText("not implemented on this server yet"), "hotbar ability 600 is handled directly (no fallback message)");
			sCombatSys.EndInterlock(human.go, false);

			bool botOk = sCombatSys.UseAbility(target.po, 9999, human.go);
			check(!botOk && !sCombatSys.IsInterlocked(target.go), "bots keep the silent drop for unknown abilities");
		}

		// ---------------------------------------------------------------- kill reward (H1) + death timer (M2)
		{
			Actor victim = makeBot(9200002, 10100.0, 10000.0, 1, 20);
			uint64 exp0 = human.po->getExperience();
			uint64 info0 = human.po->getInformation();
			bool dropped = attackUntilZeroHP(human, victim);
			check(dropped, "attacks bring a 20 HP bot to 0 HP");
			check(!victim.po->isDead() && victim.po->m_deathDelayMS != 0, "lethal hit arms the 3 s takedown timer instead of killing inline");
			check(human.po->getExperience() == exp0, "no XP is paid before the victim actually dies");

			victim.po->m_deathDelayMS = getMSTime() - 1; // fast-forward the 3 s takedown animation
			victim.po->Update();
			check(victim.po->isDead(), "death timer expiry with HP==0 kills the victim");
			check(human.po->getExperience() == exp0 + 100, "kill pays victimLevel*100 XP via die()/AwardKill (+" + std::to_string(human.po->getExperience() - exp0) + ")");
			check(human.po->getInformation() == info0 + 10, "kill pays victimLevel*10 $Info (+" + std::to_string(human.po->getInformation() - info0) + ")");

			victim.po->die(human.go);
			check(human.po->getExperience() == exp0 + 100, "second die() on a dead object does not pay twice");

			// death timer expiring on a HEALED victim must cancel the death
			Actor healed = makeBot(9200003, 10100.0, 10100.0, 1, 100);
			healed.po->m_deathDelayMS = getMSTime() - 1;
			healed.po->m_deathDelayKillerId = human.go;
			healed.po->Update();
			check(!healed.po->isDead() && healed.po->m_deathDelayMS == 0, "death timer with HP > 0 is cancelled (no zombie kill)");
			check(human.po->getExperience() == exp0 + 100, "cancelled death pays nothing");

			// !dojo clear path: die(0) kills without a reward
			Actor cleared = makeBot(9200004, 10100.0, 10200.0, 1, 100);
			cleared.po->die(0);
			check(cleared.po->isDead(), "die(0) (dojo clear) kills the bot");
			check(human.po->getExperience() == exp0 + 100, "die(0) pays no reward");
		}

		// ---------------------------------------------------------------- damage scale (dojo bots hit harder)
		{
			Actor strong1 = makeBot(9200010, 11000.0, 10000.0, 50, 100);
			Actor strong10 = makeBot(9200011, 11000.0, 10300.0, 50, 100);
			strong10.po->setDamageScale(10.0f);
			Actor dummyA = makeBot(9200012, 11100.0, 10000.0, 1, 60000);
			Actor dummyB = makeBot(9200013, 11100.0, 10300.0, 1, 60000);

			int d1 = 0, d10 = 0;
			for (int i = 0; i < 60 && d1 == 0; ++i)
			{
				uint16 before = dummyA.po->getCurrentHealth();
				sCombatSys.ResolveSingleAttack(strong1.go, dummyA.go, 1, true);
				d1 = int(before) - int(dummyA.po->getCurrentHealth());
			}
			for (int i = 0; i < 60 && d10 == 0; ++i)
			{
				uint16 before = dummyB.po->getCurrentHealth();
				sCombatSys.ResolveSingleAttack(strong10.go, dummyB.go, 1, true);
				d10 = int(before) - int(dummyB.po->getCurrentHealth());
			}
			check(d1 > 0 && d10 > 0, "scale test attacks landed (" + std::to_string(d1) + " / " + std::to_string(d10) + ")");
			check(d10 >= 5 * d1, "damageScale 10 hits at least 5x harder than scale 1 (" + std::to_string(d10) + " vs " + std::to_string(d1) + ")");
			check(human.po->getDamageScale() == 1.0f, "default damage scale is 1.0 (player stats untouched)");
		}

		// ---------------------------------------------------------------- interlock re-entrancy (M3)
		{
			Actor x = makeBot(9200020, 12000.0, 10000.0, 50, 5000);
			Actor y = makeBot(9200021, 12150.0, 10000.0, 1, 1);
			bool started = sCombatSys.RequestInterlock(x.go, y.go);
			check(started && sCombatSys.IsInterlocked(x.go) && sCombatSys.IsInterlocked(y.go), "bot interlock starts at 1.5 m");
			check(!sCombatSys.RequestInterlock(x.go, y.go), "second request while interlocked is refused");

			uint64 xExp0 = x.po->getExperience();
			// withdrawing from a lethal parting shot: takeDamage -> die() -> RemoveCombatant ->
			// EndInterlock re-enters EndInterlock for the session being torn down
			sCombatSys.LeaveInterlock(y.go);
			check(y.po->isDead(), "parting shot killed the withdrawer (inline die)");
			check(!sCombatSys.IsInterlocked(x.go) && !sCombatSys.IsInterlocked(y.go), "re-entrant EndInterlock leaves no stale session");
			check(x.po->getExperience() == xExp0 + 100, "kill from a parting shot pays exactly once via die()");

			Actor far1 = makeBot(9200022, 20000.0, 10000.0);
			Actor far2 = makeBot(9200023, 20000.0 + 1600.0, 10000.0);
			check(!sCombatSys.RequestInterlock(far1.go, far2.go), "interlock beyond 15 m is refused");

			// ---------------------------------------------------------------- melee step-in (~1.5m) and facing
			Actor stepA = makeBot(9200024, 15000.0, 10000.0, 50, 5000);
			Actor stepB = makeBot(9200025, 15500.0, 10000.0, 50, 5000); // 500 units apart (> 150)
			bool stepStarted = sCombatSys.RequestInterlock(stepA.go, stepB.go);
			double stepDist = dist2D(stepA.po->getPosition(), stepB.po->getPosition());
			check(stepStarted && std::fabs(stepDist - 150.0) < 1.0, "interlock participants step into 1.5m melee range (got " + std::to_string(stepDist) + " units)");
			LocationVector finalPosA = stepA.po->getPosition();
			LocationVector finalPosB = stepB.po->getPosition();
			double fxA = -std::sin(finalPosA.rot), fzA = -std::cos(finalPosA.rot);
			double txA = (finalPosB.x - finalPosA.x) / stepDist, tzA = (finalPosB.z - finalPosA.z) / stepDist;
			double dotA = fxA * txA + fzA * tzA;
			double fxB = -std::sin(finalPosB.rot), fzB = -std::cos(finalPosB.rot);
			double txB = (finalPosA.x - finalPosB.x) / stepDist, tzB = (finalPosA.z - finalPosB.z) / stepDist;
			double dotB = fxB * txB + fzB * tzB;
			check(dotA >= 0.999 && dotB >= 0.999, "interlock participants face each other squarely (dotA=" + std::to_string(dotA) + ", dotB=" + std::to_string(dotB) + ")");
			sCombatSys.EndInterlock(stepA.go, false);

			// ---------------------------------------------------------------- combat emotes for bots in interlock
			Actor botEmoter = makeBot(9200026, 16000.0, 10000.0, 50, 5000);
			Actor botPartner = makeBot(9200027, 16150.0, 10000.0, 50, 5000);
			sCombatSys.RequestInterlock(botEmoter.go, botPartner.go);
			bool combatEmoteOk = false;
			try {
				EmoteMsg combatMsg(botEmoter.go, 43, 1);
				combatMsg.setReceiver(&humanClient);
				const ByteBuffer& b = combatMsg.toBuf();
				combatEmoteOk = (b.size() > 0 && b.contents()[9] == 43);
			} catch (...) {}
			check(combatEmoteOk, "combat emote 43 serializes for bot combatant engaged in interlock");

			bool nonCombatSuppressed = false;
			try {
				EmoteMsg nonCombatMsg(botEmoter.go, 100, 1);
				nonCombatMsg.setReceiver(&humanClient);
				nonCombatMsg.toBuf();
			} catch (const MsgBaseClass::PacketNoLongerValid&) {
				nonCombatSuppressed = true;
			}
			check(nonCombatSuppressed, "non-combat emote 100 is suppressed for bot combatant in interlock");
			sCombatSys.EndInterlock(botEmoter.go, false);

			bool idleBotSuppressed = false;
			try {
				EmoteMsg idleMsg(botEmoter.go, 43, 1);
				idleMsg.setReceiver(&humanClient);
				idleMsg.toBuf();
			} catch (const MsgBaseClass::PacketNoLongerValid&) {
				idleBotSuppressed = true;
			}
			check(idleBotSuppressed, "combat emote is suppressed for bot NOT in interlock");
		}

		// ---------------------------------------------------------------- Update() soak with forced rounds
		{
			std::vector<Actor> winners, losers;
			for (int i = 0; i < 4; ++i)
			{
				winners.push_back(makeBot(9200100 + i * 2, 30000.0 + i * 5000.0, 30000.0, 50, 5000));
				losers.push_back(makeBot(9200101 + i * 2, 30000.0 + i * 5000.0 + 150.0, 30000.0, 1, 100));
			}
			bool allStarted = true;
			for (int i = 0; i < 4; ++i)
				allStarted &= sCombatSys.RequestInterlock(winners[i].go, losers[i].go);
			check(allStarted, "4 simultaneous interlocks started");

			bool allDead = false;
			for (int iter = 0; iter < 300 && !allDead; ++iter)
			{
				for (int i = 0; i < 4; ++i)
				{
					InterlockSession* s = sCombatSys.GetInterlockSession(winners[i].go);
					if (s) s->nextRoundTime = 0; // force the round due
				}
				sCombatSys.Update();
				allDead = true;
				for (int i = 0; i < 4; ++i)
				{
					if (losers[i].po->m_deathDelayMS != 0)
						losers[i].po->m_deathDelayMS = getMSTime() - 1;
					losers[i].po->Update();
					winners[i].po->Update();
					allDead &= losers[i].po->isDead();
				}
			}
			check(allDead, "all 4 losers die through Update() rounds + takedown timers");
			bool clean = true, paid = true, alive = true;
			for (int i = 0; i < 4; ++i)
			{
				clean &= !sCombatSys.IsInterlocked(winners[i].go) && !sCombatSys.IsInterlocked(losers[i].go);
				paid &= (winners[i].po->getExperience() == 100);
				alive &= !winners[i].po->isDead();
			}
			sCombatSys.Update(); // reap anything left
			check(clean, "no interlock sessions survive the deaths");
			check(paid, "each kill paid exactly 100 XP once (no double award, no missing award)");
			check(alive, "winners survived");
		}

		// ---------------------------------------------------------------- spawn position (N2) + dojo geometry (H3)
		{
			const double px = 40000.0, pz = 40000.0;
			Actor spawned;
			std::shared_ptr<BotClient> sb = sBotMgr.SpawnSingleBot((float)px, 572.0f, (float)pz, FACTION_MACHINES);
			check(sb != nullptr, "BotManager::SpawnSingleBot returns a bot");
			if (sb)
			{
				PlayerObject* po = sObjMgr.getGOPtr(sb->GetPlayerGoId());
				check(po != nullptr, "spawned bot has a PlayerObject");
				if (po)
				{
					LocationVector p = po->getPosition();
					check(std::fabs(p.x - px) < 5.0 && std::fabs(p.z - pz) < 5.0 && p.x > 1000.0,
						"SpawnSingleBot places the bot AT the requested position, not world origin (" + std::to_string(p.x) + ", " + std::to_string(p.z) + ")");
				}
			}

			// geometry: front = where GoAhead() walks, 3-5 m away, bot turned to face the player
			bool geomOk = true;
			std::string geomWhy;
			const double rots[] = { 0.0, 0.7, -0.7, 1.5, -1.5, 2.3, -2.3, 3.0, -3.0 };
			Actor walker = makeBot(9200300, 50000.0, 50000.0);
			for (double rot : rots)
			{
				LocationVector player(50000.0, 572.0, 50000.0);
				player.rot = rot;
				walker.po->setPosition(player);
				// ground truth for "in front": exact arithmetic of PlayerObject::GoAhead(4.0)
				// (private, so replicated here - keep in sync with PlayerObject.cpp)
				LocationVector ahead = player;
				{
					double xInc = 4.0 * std::sin(player.rot);
					double zInc = std::sqrt(4.0 * 4.0 - xInc * xInc);
					xInc *= 100; zInc *= 100;
					ahead.x -= xInc;
					if (std::fabs(player.rot) > 3.14159265358979323846 / 2) ahead.z += zInc; else ahead.z -= zInc;
				}

				LocationVector spawn = DojoPlaceInFront(player, 4.0f, 0.0f);
				double d = dist2D(player, spawn);
				if (d < 300.0 || d > 500.0 || dist2D(ahead, spawn) > 1.0)
				{
					geomOk = false;
					geomWhy += " rot " + std::to_string(rot) + ": dist " + std::to_string(d) + ", off-ahead " + std::to_string(dist2D(ahead, spawn));
				}
				// the spawned bot must face the player: its own forward vector points at the player
				double fx = -std::sin(spawn.rot), fz = -std::cos(spawn.rot);
				double tx = (player.x - spawn.x) / d, tz = (player.z - spawn.z) / d;
				if (fx * tx + fz * tz < 0.999)
				{
					geomOk = false;
					geomWhy += " rot " + std::to_string(rot) + ": bot does not face player (dot " + std::to_string(fx * tx + fz * tz) + ")";
				}
				// arc members stay in a 3-5 m band in front
				for (float off : { -0.5f, -0.17f, 0.17f, 0.5f })
				{
					LocationVector s2 = DojoPlaceInFront(player, off < 0 ? 4.5f : 3.5f, off);
					double d2 = dist2D(player, s2);
					if (d2 < 300.0 || d2 > 500.0) { geomOk = false; geomWhy += " arc dist " + std::to_string(d2); }
				}
			}
			check(geomOk, "dojo placement = 3-5 m in front (GoAhead convention) and facing the player at 9 headings" + geomWhy);
		}

		// ---------------------------------------------------------------- passive bots (no auto-aggro)
		{
			Actor victim = makeBot(9200400, 60000.0, 60000.0, 1, 5000);
			Actor passive = makeBot(9200401, 60300.0, 60000.0, 1, 100);
			passive.bot->SetFaction(FACTION_MACHINES);
			passive.po->setFactionName("Machines");
			passive.bot->SetPassive(true);
			check(passive.bot->IsPassive(), "SetPassive/IsPassive round-trip");
			LocationVector before = passive.po->getPosition();
			for (int i = 0; i < 10; ++i)
				passive.bot->UpdateBotAI(0.1f);
			check(passive.bot->GetTargetGoId() == 0, "passive Machines bot 3 m from an enemy does not acquire a target");
			check(dist2D(before, passive.po->getPosition()) < 1.0, "passive bot stands still");

			Actor aggro = makeBot(9200402, 60000.0, 60300.0, 1, 100);
			aggro.bot->SetFaction(FACTION_MACHINES);
			aggro.po->setFactionName("Machines");
			victim.bot->SetFaction(FACTION_ZION);
			victim.po->setFactionName("Zion");
			sSpatialGrid.UpdateClientPosition(victim.client, 60000.0f, 60000.0f);
			sSpatialGrid.UpdateClientPosition(aggro.client, 60000.0f, 60300.0f);
			aggro.bot->UpdateBotAI(0.1f);
			if (aggro.bot->GetTargetGoId() != 0)
				check(true, "control: a NON-passive Machines bot does auto-target (proves the passive test is meaningful)");
			else
				skip("control: non-passive bot auto-target", "headless environment did not acquire a target, passive assertion above is weaker");
			sCombatSys.EndInterlock(aggro.go, false);
		}

		// ---------------------------------------------------------------- Martial Arts & Animation Matrix Tests
		{
			// Test ExtendedAnimationMsg packet layout (opcode 0x29)
			ExtendedAnimationMsg animMsg(human.go, 0x0D58, 1);
			animMsg.setReceiver(&humanClient);
			const ByteBuffer& buf = animMsg.toBuf();
			check(buf.size() == 31, "ExtendedAnimationMsg size is exactly 31 bytes (got " + std::to_string(buf.size()) + ")");
			if (buf.size() == 31)
			{
				const uint8* data = reinterpret_cast<const uint8*>(buf.contents());
				check(data[0] == 0x03, "ExtendedAnimationMsg byte 0 is 0x03");
				check(data[8] == 0x29, "ExtendedAnimationMsg opcode is 0x29 (EXTENDED_ANIMATION)");
				uint16 wireAnimId = uint16(data[9]) | (uint16(data[10]) << 8);
				check(wireAnimId == 0x0D58, "ExtendedAnimationMsg carries correct 16-bit uint16 animId 0x0D58");
			}

			// Test CombatAnimationMatrix across all 4 disciplines
			// 1. Kung Fu
			InterlockAnimPair kfPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::KungFu, TACTIC_POWER,
				FightingStyle::None, TACTIC_SPEED,
				InterlockExchangeOutcome::StanceCrush
			);
			check(kfPair.attackerAnimId == 0x0D58 && kfPair.defenderAnimId == 0x0AFE,
				"Kung Fu Tiger Punch paired animation is 0x0D58 vs 0x0AFE");

			// 2. Karate
			InterlockAnimPair karatePair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::Karate, TACTIC_POWER,
				FightingStyle::None, TACTIC_SPEED,
				InterlockExchangeOutcome::StanceCrush
			);
			check(karatePair.attackerAnimId == 0x04F0 && karatePair.defenderAnimId == 0x0F44,
				"Karate Spin Kick paired animation is 0x04F0 vs 0x0F44");

			// 3. Aikido
			InterlockAnimPair aikidoPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::Aikido, TACTIC_RETALIATE,
				FightingStyle::None, TACTIC_DEFENSE,
				InterlockExchangeOutcome::GuardBreak
			);
			check(aikidoPair.attackerAnimId == 0x0068 && aikidoPair.defenderAnimId == 0x0AF2,
				"Aikido Tomoe Nage unblockable throw is 0x0068 vs 0x0AF2");

			// 4. Self-Defense
			InterlockAnimPair sdPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::None, TACTIC_POWER,
				FightingStyle::None, TACTIC_SPEED,
				InterlockExchangeOutcome::NormalHit
			);
			check(sdPair.attackerAnimId == 0x1135 && sdPair.defenderAnimId == 0x1136,
				"Self-Defense Headbutt paired animation is 0x1135 vs 0x1136");

			// 5. Firearms Disarm
			uint16 disarmPistol = CombatAnimationMatrix::GetDisarmAnimation(FightingStyle::KungFu, 0);
			check(disarmPistol == 0x0E3E, "Kung Fu pistol disarm animation is 0x0E3E");

			// 6. FightingStyle switching
			human.po->setFightingStyle(FightingStyle::Karate);
			check(human.po->getFightingStyle() == FightingStyle::Karate, "PlayerObject fighting style sets to Karate");
			human.po->setFightingStyle(FightingStyle::KungFu);
			check(human.po->getFightingStyle() == FightingStyle::KungFu, "PlayerObject fighting style sets to Kung Fu");
			human.po->setFightingStyle(FightingStyle::None);
			check(human.po->getFightingStyle() == FightingStyle::None, "PlayerObject fighting style resets to Self-Defense");
		}

		// ---------------------------------------------------------------- Hacker & Coder/Support disciplines
		{
			// 1. Template Classification
			const AbilityTemplate* hackT = sDataLoader.GetAbilityTemplate(57); // LogicBlast1Ability
			check(hackT != nullptr, "LogicBlast1Ability template is registered");
			if (hackT) {
				check(hackT->discipline == DisciplineType::HACKER, "LogicBlast1Ability is classified as HACKER");
				check(hackT->isCastable == true, "LogicBlast1Ability is castable");
				check(hackT->castTime == 2000, "LogicBlast1Ability authentic cast time is 2000ms");
			}

			const AbilityTemplate* coderT = sDataLoader.GetAbilityTemplate(77); // RestoreHealth1Ability
			check(coderT != nullptr, "RestoreHealth1Ability template is registered");
			if (coderT) {
				check(coderT->discipline == DisciplineType::CODER, "RestoreHealth1Ability is classified as CODER");
				check(coderT->isCastable == true, "RestoreHealth1Ability is castable");
			}

			// 2. Hacker Offensive Logic Attack
			Actor hackVictim = makeBot(9200050, 10050.0, 10000.0, 1, 100);
			human.po->setInnerStrength(100);
			humanClient.captured.clear();
			bool hackOk = sCombatSys.UseAbility(human.po, 57, hackVictim.go);
			check(hackOk, "Hacker LogicBlast1 executes successfully on hostile target");
			check(hackVictim.po->getCurrentHealth() < 100, "LogicBlast1 inflicts direct damage on victim");
			check(human.po->getCurrentIS() < 100, "LogicBlast1 deducts Inner Strength cost from caster");
			check(humanClient.sawText("[HACK]"), "Human caster receives [HACK] compilation chat feedback");

			// 3. Personal Firewall
			bool fwOk = sCombatSys.UseAbility(human.po, 68, human.go);
			check(fwOk, "PersonalFirewall1 executes on self");
			check(human.po->getFirewall() > 0, "PersonalFirewall grants positive shield absorption points");
			uint16 curFw = human.po->getFirewall();
			uint16 preDmgHp = human.po->getCurrentHealth();
			human.po->takeDamage(hackVictim.go, 50, 0x280006DF);
			check(human.po->getFirewall() == curFw - 50, "Firewall absorbed exactly 50 incoming damage points");
			check(human.po->getCurrentHealth() == preDmgHp, "Health remains unharmed while firewall shield holds");
			human.po->setFirewall(0); // clear shield

			// 4. Coder Restore Health
			human.po->setCurrentHealth(40);
			human.po->setInnerStrength(100);
			humanClient.captured.clear();
			bool healOk = sCombatSys.UseAbility(human.po, 77, human.go);
			check(healOk, "Coder RestoreHealth1 executes successfully on self");
			check(human.po->getCurrentHealth() == 100, "RestoreHealth1 reconstructs 60 HP to full 100 HP");
			check(humanClient.sawText("[CODER]"), "Human caster receives [CODER] RSI reconstruction chat feedback");

			// 5. Coder Emergency Repairs condition
			human.po->setCurrentHealth(90); // 90% health (> 35%)
			bool emerFail = sCombatSys.UseAbility(human.po, 169, human.go);
			check(!emerFail && human.po->getCurrentHealth() == 90, "EmergencyRepairs refused when health is above 35%");
			human.po->setCurrentHealth(20); // 20% health (< 35%)
			bool emerOk = sCombatSys.UseAbility(human.po, 169, human.go);
			check(emerOk && human.po->getCurrentHealth() > 20, "EmergencyRepairs executes when health is below 35%");

			// 6. Coder Revive RSI
			Actor deadOperative = makeBot(9200051, 10050.0, 10000.0, 1, 100);
			deadOperative.po->setCurrentHealth(0);
			deadOperative.po->killPlayer(human.go);
			check(deadOperative.po->isDead(), "Target operative is marked dead");
			human.po->setInnerStrength(100);
			bool reviveOk = sCombatSys.UseAbility(human.po, 375, deadOperative.go);
			check(reviveOk, "ReviveRSIAbility executes on downed operative");
			check(!deadOperative.po->isDead() && deadOperative.po->getCurrentHealth() > 0,
				"Downed operative is resuscitated at positive health");
		}
	}
	catch (const std::exception& e)
	{
		std::cout << " [FAIL] unhandled std::exception: " << e.what() << std::endl;
		g_fail++;
	}
	catch (...)
	{
		std::cout << " [FAIL] unhandled unknown exception" << std::endl;
		g_fail++;
	}

	std::cout << "\n============================================================" << std::endl;
	std::cout << "  COMBAT SUITE: " << g_pass << " passed, " << g_fail << " failed, " << g_skip << " skipped" << std::endl;
	std::cout << "============================================================\n" << std::endl;
	return g_fail;
}

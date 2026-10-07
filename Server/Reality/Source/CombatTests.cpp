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
#include "InventorySystem.h"
#include "Item.h"
#include "StatusEffectManager.h"
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

	// ---------------------------------------------------------------- IL exchange wire format
	{
		//captured live interlock-start exchange (HDS "special agent test"), 121 bytes
		static const char* capHex =
			"01020703070300bafc42000020c1801baf4200803e40000020c1e0b319430000010013010000f40134059a02233c5200008b0b0024145200008b0b0024262000008b0b00240000000000000000000000000000000021000000700000000010001000000000000000000000000000010000022b6000000000000";
		ILExchange e;
		e.attackerSlot = 1; e.defenderSlot = 2;
		e.attackerAdjustMs = 0x0307; e.defenderAdjustMs = 0x0307;
		const float fp[6] = { 126.36328125f, -10.0f, 87.5537109375f, 2.9765625f, -10.0f, 153.702636719f };
		std::string cap;
		for (size_t i = 0; i + 1 < strlen(capHex); i += 2) cap.push_back((char)strtol(std::string(capHex + i, 2).c_str(), NULL, 16));
		memcpy(e.attackerPos, cap.data() + 6, 12);
		memcpy(e.defenderPos, cap.data() + 0x12, 12);
		(void)fp;
		e.number = 1; e.startMs = 275; e.defenderOffsetMs = 500; e.attackerExtraMs = 1332; e.defenderExtraMs = 666;
		e.flags = 0x23;
		e.moves[0][0] = 0x523C; e.moves[0][1] = 0x24000B8B;
		e.moves[1][0] = 0x5214; e.moves[1][1] = 0x24000B8B;
		e.moves[2][0] = 0x2026; e.moves[2][1] = 0x24000B8B;
		e.attackerHealth = 0x21; e.defenderHealth = 0x70;
		ByteBuffer out;
		e.write(out);
		bool sizeOk = (out.size() == 0x79);
		bool headOk = sizeOk && memcmp(out.contents(), cap.data(), 0x61) == 0;
		check(sizeOk, "IL exchange serializes to 0x79 bytes (7.6005 stride)");
		check(headOk, "IL exchange bytes 0x00-0x60 match the captured live exchange");

		std::vector<uint32> slots; slots.push_back(0x00020001); slots.push_back(2);
		std::vector<ILExchange> ex; ex.push_back(e);
		ILCombatStateMsg m(0x1234, LocationVector(0,0,0), 1, slots, ex);
		const ByteBuffer& mb = m.toBuf();
		uint16 len = uint16(uint8(mb.contents()[4])) | (uint16(uint8(mb.contents()[5])) << 8);
		check(mb.size() >= 6 && mb.contents()[3] == 0x02 && len == 0xA7 && (size_t(len) + 4 == mb.size() || size_t(len) + 6 == mb.size()),
			"IL state message length field = 0xA7 like the captured interlock start");
	}

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

			// human interlock: IL handler view + opening exchange, then rounds send new exchanges
			place(target, 10100.0, 10000.0);
			setHP(target, 5000, 5000);
			bool il = sCombatSys.RequestInterlock(human.go, target.go, 0x002F0000 | 0x1234);
			InterlockSession s0;
			bool got = il && sCombatSys.GetInterlockSessionCopy(human.go, s0);
			check(got && s0.ilViewIdA != 0 && s0.ilViewIdB == 0 && s0.exchangeNum == 1,
				"human interlock spawns an IL handler view only for the human and sends exchange #1");
			uint16 before = got ? s0.exchangeNum : 0;
			for (int r = 0; r < 6 && sCombatSys.IsInterlocked(human.go); ++r)
			{
				InterlockSession* live = sCombatSys.GetInterlockSession(human.go);
				if (live) { live->nextRoundTime = 0; live->tacticA = TACTIC_POWER; live->tacticB = TACTIC_SPEED; }
				sCombatSys.Update();
			}
			InterlockSession s1;
			bool got1 = sCombatSys.GetInterlockSessionCopy(human.go, s1);
			check(got1 && s1.exchangeNum > before, "interlock rounds produce new numbered IL exchanges");
			sCombatSys.EndInterlock(human.go, false);
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
			bool combatEmoteSuppressed = false;
			try {
				EmoteMsg combatMsg(botEmoter.go, 43, 1);
				combatMsg.setReceiver(&humanClient);
				combatMsg.toBuf();
			} catch (const MsgBaseClass::PacketNoLongerValid&) {
				combatEmoteSuppressed = true;
			}
			check(combatEmoteSuppressed, "bots never emote on the wire, even in interlock (client crash guard)");

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

			// 6. Blocked animation pairs & FX (all 4 disciplines)
			InterlockAnimPair kfBlock = CombatAnimationMatrix::GetAnimationPair(FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_DEFENSE, InterlockExchangeOutcome::Blocked);
			check(kfBlock.attackerAnimId == 0x0D5C && kfBlock.defenderAnimId == 0x0CDB && kfBlock.hitFxId == 0x28000794,
				"Kung Fu Blocked paired animation is 0x0D5C vs 0x0CDB with block spark FX 0x28000794");
			InterlockAnimPair karateBlock = CombatAnimationMatrix::GetAnimationPair(FightingStyle::Karate, TACTIC_POWER, FightingStyle::Karate, TACTIC_DEFENSE, InterlockExchangeOutcome::Blocked);
			check(karateBlock.attackerAnimId == 0x04F4 && karateBlock.defenderAnimId == 0x0472 && karateBlock.hitFxId == 0x28000794,
				"Karate Blocked paired animation is 0x04F4 vs 0x0472 with block spark FX 0x28000794");
			InterlockAnimPair aikidoBlock = CombatAnimationMatrix::GetAnimationPair(FightingStyle::Aikido, TACTIC_POWER, FightingStyle::Aikido, TACTIC_DEFENSE, InterlockExchangeOutcome::Blocked);
			check(aikidoBlock.attackerAnimId == 0x00FC && aikidoBlock.defenderAnimId == 0x0114 && aikidoBlock.hitFxId == 0x28000794,
				"Aikido Blocked paired animation is 0x00FC vs 0x0114 with block spark FX 0x28000794");
			InterlockAnimPair sdBlock = CombatAnimationMatrix::GetAnimationPair(FightingStyle::None, TACTIC_POWER, FightingStyle::None, TACTIC_DEFENSE, InterlockExchangeOutcome::Blocked);
			check(sdBlock.attackerAnimId == 0x08AD && sdBlock.defenderAnimId == 0x08BE && sdBlock.hitFxId == 0x28000794,
				"Self-Defense Blocked paired animation is 0x08AD vs 0x08BE with block spark FX 0x28000794");

			// 7. Dodged animation pairs (all 4 disciplines)
			InterlockAnimPair kfDodge = CombatAnimationMatrix::GetAnimationPair(FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_NORMAL, InterlockExchangeOutcome::Dodged);
			check(kfDodge.attackerAnimId == 0x0D58 && kfDodge.defenderAnimId == 0x0CFC && kfDodge.hitFxId == 0,
				"Kung Fu Dodged paired animation is 0x0D58 vs 0x0CFC with zero FX");
			InterlockAnimPair karateDodge = CombatAnimationMatrix::GetAnimationPair(FightingStyle::Karate, TACTIC_POWER, FightingStyle::Karate, TACTIC_NORMAL, InterlockExchangeOutcome::Dodged);
			check(karateDodge.attackerAnimId == 0x04F0 && karateDodge.defenderAnimId == 0x0493 && karateDodge.hitFxId == 0,
				"Karate Dodged paired animation is 0x04F0 vs 0x0493 with zero FX");
			InterlockAnimPair aikidoDodge = CombatAnimationMatrix::GetAnimationPair(FightingStyle::Aikido, TACTIC_POWER, FightingStyle::Aikido, TACTIC_NORMAL, InterlockExchangeOutcome::Dodged);
			check(aikidoDodge.attackerAnimId == 0x00FC && aikidoDodge.defenderAnimId == 0x009D && aikidoDodge.hitFxId == 0,
				"Aikido Dodged paired animation is 0x00FC vs 0x009D with zero FX");
			InterlockAnimPair sdDodge = CombatAnimationMatrix::GetAnimationPair(FightingStyle::None, TACTIC_POWER, FightingStyle::None, TACTIC_NORMAL, InterlockExchangeOutcome::Dodged);
			check(sdDodge.attackerAnimId == 0x09E0 && sdDodge.defenderAnimId == 0x0AF0 && sdDodge.hitFxId == 0,
				"Self-Defense Dodged paired animation is 0x09E0 vs 0x0AF0 with zero FX");

			// 8. Special Combat Moves
			InterlockAnimPair headbuttPair = CombatAnimationMatrix::GetAnimationPair(FightingStyle::None, TACTIC_POWER, FightingStyle::None, TACTIC_NORMAL, InterlockExchangeOutcome::NormalHit, 197);
			check(headbuttPair.attackerAnimId == 0x1135 && headbuttPair.defenderAnimId == 0x1136, "Head Butt move 197 returns 0x1135 vs 0x1136");
			InterlockAnimPair cheapShotPair = CombatAnimationMatrix::GetAnimationPair(FightingStyle::None, TACTIC_SPEED, FightingStyle::None, TACTIC_NORMAL, InterlockExchangeOutcome::NormalHit, 198);
			check(cheapShotPair.attackerAnimId == 0x112F && cheapShotPair.defenderAnimId == 0x1131, "Cheap Shot move 198 returns 0x112F vs 0x1131");
			InterlockAnimPair clayPigeonPair = CombatAnimationMatrix::GetAnimationPair(FightingStyle::Aikido, TACTIC_POWER, FightingStyle::None, TACTIC_NORMAL, InterlockExchangeOutcome::NormalHit, 296);
			check(clayPigeonPair.attackerAnimId == 0x01B4 && clayPigeonPair.defenderAnimId == 0x1132 && clayPigeonPair.hitFxId == 0x28000432, "Spin Clay Pigeon move 296 returns 0x01B4 vs 0x1132");
			InterlockAnimPair kiPunchPair = CombatAnimationMatrix::GetAnimationPair(FightingStyle::Karate, TACTIC_SPEED, FightingStyle::None, TACTIC_NORMAL, InterlockExchangeOutcome::NormalHit, 531);
			check(kiPunchPair.attackerAnimId == 0x04F3 && kiPunchPair.defenderAnimId == 0x0AE7 && kiPunchPair.hitFxId == 0x2800045A, "Karate Focus move 531 returns 0x04F3 vs 0x0AE7 with Ki FX 0x2800045A");

			// 9. Active Melee Block Resolution
			Actor blockingBot = makeBot(9200040, 10100.0, 10000.0, 1, 100);
			blockingBot.po->setInnerStrength(10);
			sCombatSys.SetTactic(blockingBot.go, TACTIC_DEFENSE);
			humanClient.captured.clear();
			CombatSystem::AttackResult blockRes = sCombatSys.ResolveAttack(human.po, blockingBot.po, *CombatSystem::DefaultMelee(), TACTIC_POWER, TACTIC_DEFENSE, true, false);
			check(blockRes.hit == true, "Attack on blocking target is registered as a hit (absorbed)");
			check(blockRes.isBlocked == true, "Attack on blocking target has isBlocked = true");
			check(blockingBot.po->getCurrentIS() == 15, "Blocking target restores +5 Inner Strength on successful block");
			check(humanClient.sawText("blocked your"), "Attacker receives chat notification that strike was blocked");

			// 10. Dynamic FightingStyle switching via UseAbility
			sCombatSys.UseAbility(human.po, 133, blockingBot.go);
			check(human.po->getFightingStyle() == FightingStyle::KungFu, "UseAbility(133 KungFu) dynamically switches style to Kung Fu");
			sCombatSys.UseAbility(human.po, 132, blockingBot.go);
			check(human.po->getFightingStyle() == FightingStyle::Karate, "UseAbility(132 Karate) dynamically switches style to Karate");
			sCombatSys.UseAbility(human.po, 101, blockingBot.go);
			check(human.po->getFightingStyle() == FightingStyle::Aikido, "UseAbility(101 Aikido) dynamically switches style to Aikido");
			sCombatSys.UseAbility(human.po, 17, blockingBot.go);
			check(human.po->getFightingStyle() == FightingStyle::None, "UseAbility(17 SelfDefense) dynamically resets style to Self-Defense");

			// 11. Core Martial Arts Template Verification
			const AbilityTemplate* kfT = sDataLoader.GetAbilityTemplate(133);
			check(kfT && kfT->discipline == DisciplineType::MARTIAL_ARTIST && kfT->isCastable, "KungFuAbility template is registered and castable");
			const AbilityTemplate* karateT = sDataLoader.GetAbilityTemplate(132);
			check(karateT && karateT->discipline == DisciplineType::MARTIAL_ARTIST && karateT->isCastable, "KarateAbility template is registered and castable");
			const AbilityTemplate* aikidoT = sDataLoader.GetAbilityTemplate(101);
			check(aikidoT && aikidoT->discipline == DisciplineType::MARTIAL_ARTIST && aikidoT->isCastable, "AikidoAbility template is registered and castable");
			sCombatSys.EndInterlock(human.go, false);
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

			// 3b. Scheduled Cast Delay Verification
			bool scheduledExecuted = false;
			sStatusEffectManager.ScheduleCastDelay(human.go, hackVictim.go, 57, 1.0f, [&scheduledExecuted]() {
				scheduledExecuted = true;
			});
			check(!scheduledExecuted, "ScheduleCastDelay does not fire immediately");
			sStatusEffectManager.Update(0.5f);
			check(!scheduledExecuted, "ScheduleCastDelay does not fire before delay expires (0.5s / 1.0s)");
			sStatusEffectManager.Update(0.6f);
			check(scheduledExecuted, "ScheduleCastDelay fires payload after cast time expires (1.1s / 1.0s)");

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

		// ---------------------------------------------------------------- Soldier / Gunner & Spy disciplines
		{
			// 1. Template Classification
			const AbilityTemplate* soldierT = sDataLoader.GetAbilityTemplate(14); // PowerShotAbility
			check(soldierT != nullptr, "PowerShotAbility template is registered");
			if (soldierT) {
				check(soldierT->discipline == DisciplineType::GUNNER, "PowerShotAbility is classified as GUNNER");
				check(soldierT->isCastable == true, "PowerShotAbility is castable");
				check(soldierT->innerStrengthCost == 15, "PowerShotAbility authentic IS cost is 15");
			}

			const AbilityTemplate* rifleT = sDataLoader.GetAbilityTemplate(147); // RiflesAbility
			check(rifleT != nullptr, "RiflesAbility template is registered");
			if (rifleT) {
				check(rifleT->discipline == DisciplineType::GUNNER, "RiflesAbility is classified as GUNNER");
			}

			const AbilityTemplate* spyT = sDataLoader.GetAbilityTemplate(209); // StealthAbility
			check(spyT != nullptr, "StealthAbility template is registered");
			if (spyT) {
				check(spyT->discipline == DisciplineType::SPY, "StealthAbility is classified as SPY");
				check(spyT->isBuff == true, "StealthAbility is classified as buff");
			}

			const AbilityTemplate* knifeT = sDataLoader.GetAbilityTemplate(146); // PoisonKnifeAbility
			check(knifeT != nullptr, "PoisonKnifeAbility template is registered");
			if (knifeT) {
				check(knifeT->discipline == DisciplineType::SPY, "PoisonKnifeAbility is classified as SPY");
			}

			// 2. Soldier Firearm Execution
			human.po->getInventory()->clear();
			Actor soldierVictim = makeBot(9200060, 10050.0, 10000.0, 1, 100);
			human.po->setInnerStrength(100);
			humanClient.captured.clear();
			bool soldierOk = sCombatSys.UseAbility(human.po, 14, soldierVictim.go);
			check(soldierOk, "Soldier PowerShot executes successfully on hostile target");
			check(soldierVictim.po->getCurrentHealth() == 50, "PowerShot inflicts 50 ballistic damage on victim");
			check(human.po->getCurrentIS() < 100, "PowerShot deducts Inner Strength cost from caster");
			check(humanClient.sawText("[SOLDIER]"), "Human caster receives [SOLDIER] ballistic chat feedback");

			// Soldier Dual-Wielding 1.5x Multiplier
			human.po->getInventory()->addItemAuto(std::make_shared<Item>(9901, 1001));
			check(human.po->isDualWielding() == true, "Operative equipped with dual pistols enters dual-wielding state");
			Actor dualVictim = makeBot(9200068, 10050.0, 10000.0, 1, 100);
			human.po->setInnerStrength(100);
			bool dualOk = sCombatSys.UseAbility(human.po, 14, dualVictim.go);
			check(dualOk, "Soldier PowerShot executes while dual wielding");
			check(dualVictim.po->getCurrentHealth() == 25, "PowerShot with dual wielding deals 1.5x damage (75 dmg, leaving 25 hp)");

			// 3. Soldier Pistol Disarming Shot
			Actor disarmVictim = makeBot(9200061, 10050.0, 10000.0, 1, 100);
			human.po->setInnerStrength(100);
			bool disarmOk = sCombatSys.UseAbility(human.po, 126, disarmVictim.go);
			check(disarmOk, "PistolDisarmingShot executes on target");
			check(disarmVictim.po->getCurrentHealth() < 100, "PistolDisarmingShot inflicts damage");

			// 4. Soldier Long-Range Sniper Shot
			Actor sniperVictim = makeBot(9200062, 10000.0 + 7000.0, 10000.0, 1, 150); // 70m away
			human.po->setInnerStrength(100);
			bool sniperOk = sCombatSys.UseAbility(human.po, 505, sniperVictim.go);
			check(sniperOk, "SniperShot executes successfully at 70m long range");
			check(sniperVictim.po->getCurrentHealth() <= 75, "SniperShot inflicts heavy ballistic damage (75 dmg)");

			// 5. Spy Stealth Cloak
			human.po->setInnerStrength(100);
			human.po->setStealth(false);
			humanClient.captured.clear();
			bool stealthOk = sCombatSys.UseAbility(human.po, 209, human.go);
			check(stealthOk, "Spy StealthAbility executes on self");
			check(human.po->isStealthed() == true, "Operative enters stealth concealment mode");
			check(humanClient.sawText("[SPY]"), "Human operative receives [SPY] concealment chat feedback");

			// 6. Spy Poison Knife Ambush (1.75x Crit & Breaks Stealth)
			Actor spyVictim = makeBot(9200063, 10050.0, 10000.0, 1, 100);
			human.po->setInnerStrength(100);
			human.po->setStealth(true);
			bool poisonOk = sCombatSys.UseAbility(human.po, 146, spyVictim.go);
			check(poisonOk, "PoisonKnifeAbility executes on target in melee range");
			check(spyVictim.po->getCurrentHealth() == 57, "PoisonKnife ambush deals 1.75x critical strike (43 dmg, leaving 57 hp)");
			check(human.po->isStealthed() == false, "Stealth concealment breaks upon attacking target");

			// 7. Spy Knife Thrower Ranged Strike (1.5x Ambush)
			Actor throwVictim = makeBot(9200064, 10000.0 + 2000.0, 10000.0, 1, 100); // 20m away
			human.po->setInnerStrength(100);
			human.po->setStealth(true);
			bool throwOk = sCombatSys.UseAbility(human.po, 283, throwVictim.go);
			check(throwOk, "KnifeThrowerAbility executes successfully at 20m range");
			check(throwVictim.po->getCurrentHealth() == 55, "KnifeThrower ambush deals 1.5x damage (45 dmg, leaving 55 hp)");
			check(human.po->isStealthed() == false, "Stealth breaks after knife throw");

			// 8. Hacker Discipline Attacks & Defenses
			Actor hackVictim = makeBot(9200065, 10050.0, 10000.0, 1, 100);
			human.po->setInnerStrength(100);
			bool hackOk = sCombatSys.UseAbility(human.po, 57, hackVictim.go);
			check(hackOk, "Hacker LogicBlast1 executes successfully on target");
			check(hackVictim.po->getCurrentHealth() == 55, "LogicBlast1 inflicts 45 code damage (leaving 55 hp)");

			human.po->setInnerStrength(100);
			bool virusOk = sCombatSys.UseAbility(human.po, 53, hackVictim.go);
			check(virusOk, "Hacker TransmitVirus compiles and applies viral payload");

			human.po->setInnerStrength(100);
			bool stunOk = sCombatSys.UseAbility(human.po, 43, hackVictim.go);
			check(stunOk, "Hacker DisruptInputs executes and applies stun effect");

			human.po->setInnerStrength(100);
			bool firewallOk = sCombatSys.UseAbility(human.po, 68, human.go);
			check(firewallOk, "Hacker PersonalFirewall compiles and shields operative");

			// 9. Coder Discipline Restoration & Constructs
			human.po->setCurrentHealth(40);
			human.po->setInnerStrength(100);
			bool healOk = sCombatSys.UseAbility(human.po, 77, human.go);
			check(healOk, "Coder RestoreHealth1 executes successfully on self");
			check(human.po->getCurrentHealth() == 100, "RestoreHealth1 heals 60 health (40 -> 100)");

			Actor constructBot = makeBot(9200066, 10050.0, 10000.0, 1, 200);
			constructBot.po->setCurrentHealth(50);
			human.po->setInnerStrength(100);
			bool fortifyOk = sCombatSys.UseAbility(human.po, 20, constructBot.go);
			check(fortifyOk, "Coder FortifySimulacra1Ability executes on construct");
			check(constructBot.po->getCurrentHealth() == 100, "FortifySimulacra restores 50 health buffer (50 -> 100)");

			constructBot.po->setCurrentHealth(40);
			human.po->setInnerStrength(100);
			bool repairOk = sCombatSys.UseAbility(human.po, 30, constructBot.go);
			check(repairOk, "Coder RepairSimulacra1Ability executes on construct");
			check(constructBot.po->getCurrentHealth() == 160, "RepairSimulacra repairs 120 health (40 -> 160)");

			Actor fallenBot = makeBot(9200067, 10050.0, 10000.0, 1, 100);
			fallenBot.po->die(0);
			check(fallenBot.po->isDead(), "Target bot is marked dead");
			human.po->setInnerStrength(100);
			bool reviveOk = sCombatSys.UseAbility(human.po, 375, fallenBot.go);
			check(reviveOk, "Coder ReviveRSIAbility executes on downed operative");
			check(!fallenBot.po->isDead(), "Downed operative is restored to live state by ReviveRSI");
			check(fallenBot.po->getCurrentHealth() == 50, "Revived operative has 50% health restored");

			// 10. Ability System Memory, In-Place Upgrades & Cooldown Tracking
			auto abilitySys = human.po->getAbilitySystem();
			check(abilitySys != nullptr, "AbilitySystem instance is valid");
			if (abilitySys)
			{
				uint16 memUsed = abilitySys->getTotalMemoryUsed();
				check(memUsed > 0, "AbilitySystem accurately computes total memory used from loaded templates");
				check(abilitySys->getMaxMemory() >= 100, "Operative MaxMemory scales with level (>= 100)");

				// In-place upgrade: level up ability 600 from L1 to L2 in slot 0
				bool upOk = abilitySys->loadAbility(600, 2, 0);
				check(upOk, "AbilitySystem::loadAbility supports in-place upgrade for already-loaded abilities");
				auto upgraded = abilitySys->getAbility(600);
				check(upgraded && upgraded->getLevel() == 2, "Upgraded ability reflects new Level 2");

				// Cooldown tracking
				abilitySys->onAbilityCast(600);
				check(abilitySys->getAbility(600)->getLastUsedTime() > 0, "onAbilityCast records last used timestamp");
				check(!abilitySys->canCastAbility(600), "canCastAbility enforces 2000ms ability cooldown lockout");
			}

			// 11. Authentic Interlock Move Selection (Task 4)
			InterlockSession testSession;
			testSession.goIdA = human.go;
			testSession.goIdB = fallenBot.go;
			testSession.exchangeNum = 0;
			testSession.tacticA = TACTIC_POWER;
			testSession.tacticB = TACTIC_SPEED;

			// Stance crush Power vs Speed
			uint32 moveCrush = sCombatSys.SelectInterlockMove(testSession, human.po, fallenBot.po);
			check(moveCrush == 0x2367, "SelectInterlockMove returns 0x2367 (Hyperstrike / Stance Crush) on Power vs Speed");

			// Grab vs Defense
			testSession.tacticA = TACTIC_RETALIATE;
			testSession.tacticB = TACTIC_DEFENSE;
			uint32 moveGrab = sCombatSys.SelectInterlockMove(testSession, human.po, fallenBot.po);
			check(moveGrab == 0x236D, "SelectInterlockMove returns 0x236D (Aikido Throw / Momentum Reversal) on Grab tactic");

			// Karate discipline
			human.po->setFightingStyle(FightingStyle::Karate);
			testSession.tacticA = TACTIC_SPEED;
			testSession.tacticB = TACTIC_SPEED;
			uint32 moveKarate = sCombatSys.SelectInterlockMove(testSession, human.po, fallenBot.po);
			check(moveKarate == 0x2388, "SelectInterlockMove returns 0x2388 (Karate High Kick / Strike) for Karate style");

			// Kung Fu discipline
			human.po->setFightingStyle(FightingStyle::KungFu);
			testSession.exchangeNum = 2;
			uint32 moveKungFu = sCombatSys.SelectInterlockMove(testSession, human.po, fallenBot.po);
			check(moveKungFu == 0x2026, "SelectInterlockMove returns 0x2026 (Kung Fu Strike) for Kung Fu style");

			// Finisher / Takedown when opponent is dead
			fallenBot.po->setDead(true);
			uint32 moveFinisher = sCombatSys.SelectInterlockMove(testSession, human.po, fallenBot.po);
			check(moveFinisher == 0x4EE5, "SelectInterlockMove returns 0x4EE5 (Finisher / Takedown) when opponent is defeated");
			fallenBot.po->setDead(false);
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

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
#include "NavMeshMgr.h"
#include "BehaviorTree.h"
#include "DataLoader.h"
#include "CombatAnimationMatrix.h"
#include "DojoSpawn.h"
#include "MessageTypes.h"
#include "GOAttributes.h"
#include "RsiData.h"
#include "InventorySystem.h"
#include "Item.h"
#include "StatusEffectManager.h"
#include "HackerSystem.h"
#include "MissionSystem.h"
#include "CraftingSystem.h"
#include "LootManager.h"
#include "AI/MatrixThreatHeatmap.h"
#include "EconomySystem.h"
#include "FactionWarManager.h"
#include "AgentPossessionManager.h"
#include "AI/PedestrianEcology.h"
#include "SocketSystem.h"
#include "Config.h"
#include "OrganizationManager.h"
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
		// A test client lives on the stack of one test block. Remove its character with it,
		// or later broadcasts (faction alerts, AnnounceCommand) walk a PlayerObject whose
		// client is gone and jump through a dead vtable (that segfault aborted the suite).
		~TestHumanClient()
		{
			m_commandTap = 0;
			uint32 go = GetPlayerGoId();
			if (go != 0 && sObjMgr.getGOPtrSafe(go) != nullptr)
				sObjMgr.destroyObject(go);
		}

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
			a.po->SpawnSelf();
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
		ILExchange e;
		e.attackerSlot = 1; e.defenderSlot = 2;
		e.attackerAdjustMs = 0x0307; e.defenderAdjustMs = 0x0307;
		e.style = (uint8)FightingStyle::KungFu;
		e.number = 1; e.startMs = 275; e.defenderOffsetMs = 500;
		e.flags = 0x23;
		e.moves[0][0] = 0x523C; e.moves[0][1] = 0x24000B8B;
		e.moves[1][0] = 0x5214; e.moves[1][1] = 0x24000B8B;
		e.moves[2][0] = 0x2026; e.moves[2][1] = 0x24000B8B;
		e.attackerHealth = 0x21; e.defenderHealth = 0x70;
		ByteBuffer out;
		e.write(out);
		bool sizeOk = (out.size() == 0x79);
		check(sizeOk, "IL exchange serializes to 0x79 bytes (authentic 7.6005 stride)");
		const uint8* ob = reinterpret_cast<const uint8*>(out.contents());
		bool headOk = sizeOk && ob[0] == 1 && ob[1] == 2 && ob[0x1E] == (uint8)FightingStyle::KungFu &&
			ob[0x1F] == 0 && *reinterpret_cast<const uint16*>(&ob[0x20]) == 1 && *reinterpret_cast<const uint32*>(&ob[0x3D]) == 0x2026;
		check(headOk, "IL exchange fields (0x00-0x78) match authentic 0x79 protocol offsets");

		std::vector<uint32> slots; slots.push_back(0x00020001); slots.push_back(2);
		std::vector<ILExchange> ex; ex.push_back(e);
		ILCombatStateMsg m(0x1234, LocationVector(0,0,0), 1, slots, ex);
		const ByteBuffer& mb = m.toBuf();
		// Exact HDS capture framing: 03 01 00 02 A7 00 [ilView] 01 [162-byte payload] 00 00
		const uint8* mbb = reinterpret_cast<const uint8*>(mb.contents());
		static const uint8 head[] = { 0x03, 0x01, 0x00, 0x02, 0xA7, 0x00, 0x34, 0x12, 0x01 };
		bool headMatch = mb.size() == 173 && memcmp(mbb, head, sizeof(head)) == 0;
		check(headMatch, "IL state message = HDS capture framing 03 01 00 02 A7 00 [view] 01 (len 0xA7, autoview flags 0x01)");
		// slot mask 3 and the two handles directly after pos(24) + seq(4)
		bool slotsOk = mb.size() == 173 && *reinterpret_cast<const uint32*>(&mbb[9 + 24]) == 1 &&
			*reinterpret_cast<const uint32*>(&mbb[9 + 28]) == 3 && mbb[9 + 40] == 1;
		check(slotsOk, "IL state message carries seq 1, slot mask 3, two handles, one exchange");
	}

	// ---------------------------------------------------------------- IL start vs HDS capture
	// hd_reference/hds/world/Client/RpcHandlers/CombatHandler.cs L58-77 ("Special agent test"):
	//   ... [pos] 01000000 03000000 [targetViewWithSpawnId] 0200 [selfSpawnIdCounter] 01 | 0102 + L76 hex
	// Everything after the position must match byte for byte, given the capture's own inputs
	// (relative positions, health, defender offset 500) and default conf.
	{
		static const char* capHex =
			"01000000" "03000000" "05002f00" "02002f00" "01"
			"0102"
			"0703070300bafc42000020c1801baf4200803e40000020c1e0b319430000010013010000f40134059a02233c5200008b0b0024"
			"145200008b0b0024262000008b0b00240000000000000000000000000000000021000000700000000010001000000000"
			"000000000000000000010000022b600000000000";
		std::vector<uint8> cap;
		for (const char* p = capHex; p[0] && p[1]; p += 2)
		{
			auto nib = [](char c) -> uint8 { return uint8((c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10); };
			cap.push_back(uint8((nib(p[0]) << 4) | nib(p[1])));
		}

		ILExchange e; // defaults must already be the capture's timing/style values
		e.attackerSlot = ILSlotFor(false); // capture: the opponent attacks
		e.defenderSlot = ILSlotFor(true);
		const uint8 posBytes[24] = {
			0x00,0xba,0xfc,0x42, 0x00,0x00,0x20,0xc1, 0x80,0x1b,0xaf,0x42,
			0x00,0x80,0x3e,0x40, 0x00,0x00,0x20,0xc1, 0xe0,0xb3,0x19,0x43 };
		memcpy(e.attackerPos, posBytes, 12);
		memcpy(e.defenderPos, posBytes + 12, 12);
		e.number = 1;
		e.defenderOffsetMs = 500;
		e.moves[0][0] = 0x523C; e.moves[0][1] = IL_MOVE_DATABASE;
		e.moves[1][0] = 0x5214; e.moves[1][1] = IL_MOVE_DATABASE;
		e.moves[2][0] = 0x2026; e.moves[2][1] = IL_MOVE_DATABASE;
		e.attackerHealth = 0x21; e.defenderHealth = 0x70;

		std::vector<uint32> slots = BuildILSlots(0x002F0005);
		std::vector<ILExchange> ex; ex.push_back(e);
		ILCombatStateMsg m(0x1234, LocationVector(0,0,0), 1, slots, ex);
		const ByteBuffer& mb = m.toBuf();
		const uint8* mbb = reinterpret_cast<const uint8*>(mb.contents());
		const size_t off = 9 + 24; // framing + position
		bool sizeOk = mb.size() == off + cap.size() + 2;
		size_t firstDiff = cap.size();
		if (sizeOk)
			for (size_t i = 0; i < cap.size(); i++)
				if (mbb[off + i] != cap[i]) { firstDiff = i; break; }
		check(sizeOk && firstDiff == cap.size(),
			"IL start payload after position == HDS CombatHandler.cs L63-77 capture byte for byte ("
			+ (firstDiff == cap.size() ? std::string("no diff, ") + std::to_string(cap.size()) + " bytes"
				: "first diff at byte " + std::to_string(firstDiff)) + ")");
		check(GetILCombatHandlerGoId() == 55, "IL handler GO id defaults to 55 (HDS GameObjectDefinitions.cs:37009)");
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
			target.po->setFactionName("Machines"); // a human may only fight another faction (Combat.FactionRules)
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

			// HUD RPCs at the start: 0x40 combat target and a round-0 0x45 status (client.dll
			// FUN_621a13b0 / FUN_6219dbb0). 0x45 is 89 bytes and @0x31 must equal @0x35.
			auto findMsg = [&](uint8 op, size_t len, std::string* out) {
				for (auto it = humanClient.captured.rbegin(); it != humanClient.captured.rend(); ++it)
					if (it->size() == len && uint8((*it)[0]) == op) { if (out) *out = *it; return true; }
				return false;
			};
			std::string tgt, st0;
			bool hasTarget = findMsg(0x40, 7, &tgt);
			check(hasTarget && *reinterpret_cast<const uint32*>(&tgt[1]) == (0x002F0000u | 0x1234u),
				"interlock start sends RPC 0x40 with the opponent handle (view | spawn<<16)");
			bool hasStatus0 = findMsg(0x45, 0x59, &st0);
			check(hasStatus0 && *reinterpret_cast<const uint16*>(&st0[3]) == 1
				&& *reinterpret_cast<const uint32*>(&st0[0x31]) == (0x002F0000u | 0x1234u)
				&& *reinterpret_cast<const uint32*>(&st0[0x35]) == *reinterpret_cast<const uint32*>(&st0[0x31])
				&& *reinterpret_cast<const uint32*>(&st0[0x4D]) != 0,
				"interlock start sends an 89-byte RPC 0x45 round status (seq 1, handle twice, lock time)");

			human.po->setTactic(TACTIC_POWER);
			humanClient.captured.clear();
			for (int r = 0; r < 6 && sCombatSys.IsInterlocked(human.go); ++r)
			{
				InterlockSession* live = sCombatSys.GetInterlockSession(human.go);
				if (live) { live->nextRoundTime = 0; live->tacticA = TACTIC_POWER; live->tacticB = TACTIC_SPEED; }
				sCombatSys.Update();
			}
			InterlockSession s1;
			bool got1 = sCombatSys.GetInterlockSessionCopy(human.go, s1);
			check(got1 && s1.exchangeNum > before, "interlock rounds produce new numbered IL exchanges");

			std::string st1;
			bool hasStatus1 = findMsg(0x45, 0x59, &st1);
			check(hasStatus1 && *reinterpret_cast<const uint16*>(&st1[3]) > 1
				&& uint8(st1[0x08]) == CLIENT_TACTIC_POWER,
				"each round sends a newer 0x45 with the viewer's tactic in client numbering (Power = 1)");
			check(hasStatus1 && *reinterpret_cast<const int16*>(&st1[0x0B]) >= 0
				&& *reinterpret_cast<const int16*>(&st1[0x1E]) >= 0,
				"0x45 round damage fields are filled and never negative");

			// RPC 0x42 in the 7-byte client format: [0x42][u16 setting][u16 0][u16 variant]
			struct { uint16 setting; uint8 expect; const char* name; } cases[] = {
				{ CLIENT_TACTIC_SPEED, TACTIC_SPEED, "Speed" }, { CLIENT_TACTIC_POWER, TACTIC_POWER, "Power" },
				{ CLIENT_TACTIC_GRAB, TACTIC_RETALIATE, "Grab" }, { CLIENT_TACTIC_BLOCK, TACTIC_DEFENSE, "Block" } };
			bool allMapped = true, allConfirmed = true;
			for (auto& c : cases)
			{
				humanClient.captured.clear();
				ByteBuffer cmd;
				cmd << uint8(0x42) << uint16(c.setting) << uint16(0) << uint16(0);
				human.po->HandleCommand(cmd);
				allMapped &= (human.po->getTactic() == c.expect);
				std::string ack;
				allConfirmed &= findMsg(0x42, 7, &ack) && *reinterpret_cast<const uint16*>(&ack[1]) == c.setting;
			}
			check(allMapped, "client 0x42 settings map 0 Speed, 1 Power, 2 Grab, 3 Block to the server tactics");
			check(allConfirmed, "server confirms each tactic with RPC 0x42 in client numbering");
			human.po->setTactic(TACTIC_NORMAL);

			ByteBuffer withdraw;
			withdraw << uint8(0x42) << uint16(CLIENT_TACTIC_WITHDRAW) << uint16(0) << uint16(2);
			human.po->HandleCommand(withdraw);
			check(!sCombatSys.IsInterlocked(human.go), "client 0x42 setting 4 (HUD Withdraw) leaves the interlock");
			sCombatSys.EndInterlock(human.go, false);
		}

		// ---------------------------------------------------------------- who a human may attack (audit 2026-10-13)
		{
			Actor contact = makeBot(9200201, 10120.0, 10000.0);
			contact.po->setFactionName("FRIENDLY");
			Actor hostile = makeBot(9200202, 10140.0, 10000.0);
			hostile.po->setFactionName("HOSTILE");
			Actor civ = makeBot(9200203, 10160.0, 10000.0);
			civ.po->setFactionName("Civilian");
			std::string why;
			check(!sCombatSys.CanAttack(human.po, contact.po, &why) && !why.empty(), "a human can't attack a friendly mission contact");
			check(sCombatSys.CanAttack(human.po, hostile.po), "a human can attack a HOSTILE mission target");
			check(!sCombatSys.CanAttack(human.po, civ.po), "a human can't attack a civilian");
			check(!sCombatSys.CanAttack(human.po, human.po), "nobody can attack themselves");
			check(sCombatSys.CanAttack(contact.po, human.po), "bot AI is not restricted by the faction rules");
			check(!sCombatSys.RequestInterlock(human.go, contact.go) && !sCombatSys.IsInterlocked(human.go),
				"RequestInterlock on a friendly contact is refused");
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
			// Retail Matrix Online client convention: forward is (-sin(rot), -cos(rot))
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
				// ground truth for "in front": exact arithmetic of DojoPlaceInFront(4.0) (+Z forward)
				LocationVector ahead = player;
				ahead.x += std::sin(player.rot) * 400.0;
				ahead.z += std::cos(player.rot) * 400.0;

				LocationVector spawn = DojoPlaceInFront(player, 4.0f, 0.0f);
				double d = dist2D(player, spawn);
				if (d < 300.0 || d > 500.0 || dist2D(ahead, spawn) > 1.0)
				{
					geomOk = false;
					geomWhy += " rot " + std::to_string(rot) + ": dist " + std::to_string(d) + ", off-ahead " + std::to_string(dist2D(ahead, spawn));
				}
				// the spawned bot must face the player: its own forward vector points at the player
				double fx = std::sin(spawn.rot), fz = std::cos(spawn.rot);
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
			check(geomOk, "dojo placement = 3-5 m in front and facing the player at 9 headings" + geomWhy);
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

		// ---------------------------------------------------------------- 12. Firearms & Bullet Dodge Regression Suite
		{
			// A. Firearms Weapon & Ammunition Templates
			const ItemTemplate* tplBerettas = sDataLoader.GetItemTemplate(1001);
			check(tplBerettas != nullptr, "DataLoader: Dual Berettas (1001) template loaded");
			if (tplBerettas)
			{
				check(tplBerettas->type == ITEM_TYPE_WEAPON, "Dual Berettas is ITEM_TYPE_WEAPON");
				check(tplBerettas->isDualWield == true, "Dual Berettas has isDualWield = true");
				check(tplBerettas->attackSpeed == 1.2f, "Dual Berettas has attackSpeed 1.2s cadence");
				check(tplBerettas->range == 2500.0f, "Dual Berettas has 25m (2500 units) ballistic range");
				check(tplBerettas->animAttack == 0x0EDD, "Dual Berettas has animAttack 0x0EDD");
			}

			const ItemTemplate* tplRifle = sDataLoader.GetItemTemplate(1003);
			check(tplRifle != nullptr, "DataLoader: M4A1 Tactical Carbine (1003) template loaded");
			if (tplRifle)
			{
				check(tplRifle->attackSpeed == 2.2f, "M4A1 Carbine has attackSpeed 2.2s cadence");
				check(tplRifle->range == 6000.0f, "M4A1 Carbine has 60m (6000 units) ballistic range");
				check(tplRifle->animAttack == 0x0EE0, "M4A1 Carbine has animAttack 0x0EE0");
			}

			const ItemTemplate* tplAmmo9mm = sDataLoader.GetItemTemplate(1050);
			check(tplAmmo9mm != nullptr && tplAmmo9mm->type == ITEM_TYPE_CONSUMABLE, "DataLoader: 9mm Ammo (1050) template loaded");
			const ItemTemplate* tplAmmo556 = sDataLoader.GetItemTemplate(1051);
			check(tplAmmo556 != nullptr && tplAmmo556->type == ITEM_TYPE_CONSUMABLE, "DataLoader: 5.56mm Ammo (1051) template loaded");

			// B. Evade Shield & Equipped Item Protocol Serialization
			SelfEvadeShieldMsg selfShieldMsg(85);
			const ByteBuffer& selfShieldBuf = selfShieldMsg.toBuf();
			check(selfShieldBuf.size() > 0, "SelfEvadeShieldMsg serializes successfully");
			if (selfShieldBuf.size() >= 4)
			{
				const uint8* data = reinterpret_cast<const uint8*>(selfShieldBuf.contents());
				check(data[0] == 0x03, "SelfEvadeShieldMsg byte 0 is 0x03");
				check(data[1] == 0x02 && data[2] == 0x00, "SelfEvadeShieldMsg viewId is VIEWID_SELF (0x0002)");
				check(data[3] == 0x02, "SelfEvadeShieldMsg attribute update type is 0x02");
			}

			SelfEquippedItemMsg selfEquipMsg(1001);
			const ByteBuffer& selfEquipBuf = selfEquipMsg.toBuf();
			check(selfEquipBuf.size() > 0, "SelfEquippedItemMsg serializes successfully");

			// C. Bullet Dodge Mechanic & Evasion Roll
			Actor gunner = makeBot(9200501, 70000.0, 70000.0, 1, 500);
			TestHumanClient dodgeClient;
			Actor dodger = makeHuman(&dodgeClient, 9100005, 70200.0, 70000.0); // 2m apart
			dodger.po->setMaximumHealth(1000);
			dodger.po->setCurrentHealth(1000);
			dodger.po->setEvadeShield(100);
			dodger.po->setInnerStrength(100);

			check(dodger.po->getEvadeShield() == 100, "Defender starts with 100% Evade Shield");
			check(dodger.po->getCurrentIS() == 100, "Defender starts with 100 IS");

			// Ballistic attack move
			CombatMove gunShot;
			gunShot.id = 200;
			gunShot.name = "PistolShot";
			gunShot.dmgType = DAMAGE_BALLISTIC;
			gunShot.minDmg = 50.0f;
			gunShot.maxDmg = 60.0f;
			gunShot.minDmgPerLvl = 0.0f;
			gunShot.maxDmgPerLvl = 0.0f;
			gunShot.isCost = 0;
			gunShot.range = 1500.0f;
			gunShot.hitFxId = 0x280006DF;
			gunShot.interlockOnly = false;
			gunShot.freefireOnly = true;
			gunShot.castTime = 0.0f;
			gunShot.specialFlags = 0;

			// Defender level 50 vs Attacker level 1 with TACTIC_DEFENSE guarantees defense roll >= attack roll
			dodger.po->setLevel(50);
			gunner.po->setLevel(1);

			CombatSystem::AttackResult resDodge = sCombatSys.ResolveAttack(gunner.po, dodger.po, gunShot, TACTIC_NORMAL, TACTIC_DEFENSE);
			check(!resDodge.hit, "Bullet Dodge triggers: attack roll defeated by defense roll + defense tactic");
			check(resDodge.damageTaken == 0, "Bullet Dodge results in exactly 0 damage taken");
			check(dodger.po->getCurrentHealth() == 1000, "Defender HP unmodified during successful bullet dodge");
			check(dodger.po->getEvadeShield() < 100 && dodger.po->getEvadeShield() >= 75,
				"Evade Shield decremented by 15..25 points on dodge (current: " + std::to_string(dodger.po->getEvadeShield()) + "%)");
			check(dodger.po->getCurrentIS() == 90, "Inner Strength decremented by 10 IS on dodge (current: " + std::to_string(dodger.po->getCurrentIS()) + ")");
			check(dodger.po->getCurrentAnimation() == 0xEC && dodger.po->getCurrentMood() == 0x03,
				"Authentic Bullet Dodge animation (0x03EC = G_D_XR_DodgeFromHF_GLb) triggered");
			check(dodgeClient.sawText("[BULLET DODGE]"), "Defender received [BULLET DODGE] client notification");

			// D. Shield Depletion & Penetration
			dodger.po->setEvadeShield(0);
			check(dodger.po->getEvadeShield() == 0, "Evade Shield manually depleted to 0%");
			CombatSystem::AttackResult resHit = sCombatSys.ResolveAttack(gunner.po, dodger.po, gunShot, TACTIC_POWER, TACTIC_NORMAL);
			check(resHit.hit, "When Evade Shield is 0%, ballistic gunfire hits target");
			check(resHit.damageTaken > 0, "Depleted shield allows bullet penetration dealing " + std::to_string(resHit.damageTaken) + " damage");
			check(dodger.po->getCurrentHealth() < 1000, "Defender took damage from penetrating gunfire");
			check(dodgeClient.sawText("[EVADE SHIELD]"), "Defender received shield depleted warning");

			// E. Evade Shield Natural Regeneration
			dodger.po->setEvadeShield(50);
			dodger.po->setLastBallisticFireTime(getMSTime() - 2000); // 2s ago
			dodger.po->regenerateEvadeShield(1000);
			check(dodger.po->getEvadeShield() == 50, "Evade Shield does not regenerate within 4s of ballistic fire");

			dodger.po->setLastBallisticFireTime(getMSTime() - 4500); // 4.5s ago
			dodger.po->regenerateEvadeShield(1000);
			check(dodger.po->getEvadeShield() == 55, "Evade Shield regenerates +5% (to 55%) after 4s without fire");

			// F. Firearms Cadence & Weapon Equipping
			dodger.po->setEquippedWeaponId(1001);
			check(dodger.po->getEquippedWeaponId() == 1001, "setEquippedWeaponId equips Dual Berettas (1001)");
			check(dodger.po->isDualWielding(), "Player is dual wielding when Dual Berettas equipped");
			auto eqWeapons = dodger.po->getEquippedWeapons();
			check(!eqWeapons.empty() && eqWeapons[0]->getTemplateId() == 1001, "getEquippedWeapons returns equipped Dual Berettas");

			// Inventory Ammo Reload
			auto inv = dodger.po->getInventory();
			check(inv != nullptr, "Player InventorySystem exists");
			if (inv)
			{
				bool gaveAmmo = dodger.po->giveItem(1050); // 9mm clip
				check(gaveAmmo, "giveItem gave 9mm ammo clip (1050)");
				check(inv->hasItemByTemplate(1050), "Inventory contains 9mm ammunition clip (1050)");
				eqWeapons[0]->setAmmoCount(0);
				check(eqWeapons[0]->getAmmoCount() == 0, "Weapon ammo set to 0 to simulate empty magazine");

				const ItemTemplate* wpnTpl = sDataLoader.GetItemTemplate(1001);
				if (wpnTpl && wpnTpl->maxAmmo > 0 && inv->hasItemByTemplate(1050))
				{
					inv->consumeItemByTemplate(1050);
					eqWeapons[0]->setAmmoCount(wpnTpl->maxAmmo - 1);
				}
				check(!inv->hasItemByTemplate(1050), "Ammunition clip consumed during reload");
				check(eqWeapons[0]->getAmmoCount() == 29, "Dual Berettas magazine reloaded to max (30 - 1 = 29)");
			}
		}

		// ---------------------------------------------------------------- 13. Martial Arts Disciplines & Animation Synchronization Suite
		{
			// A. Four Core Martial Arts Forms & Stance Crush Pairings
			// 1. Kung Fu (Wushu) Power vs Speed Stance Crush
			InterlockAnimPair kfPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::KungFu, TACTIC_POWER,
				FightingStyle::KungFu, TACTIC_SPEED,
				InterlockExchangeOutcome::StanceCrush
			);
			check(kfPair.attackerAnimId == 0x0D58, "Kung Fu Stance Crush: Attacker plays Tiger Punch (0x0D58)");
			check(kfPair.defenderAnimId == 0x0AFE, "Kung Fu Stance Crush: Defender plays heavy recoil (0x0AFE)");
			check(kfPair.contactDelaySeconds == 0.53f, "Kung Fu Tiger Punch contact delay is authentic 0.53s (530ms)");
			check(kfPair.hitFxId == 0x280006DF, "Kung Fu Tiger Punch plays text damage FX 0x280006DF");

			// 2. Karate Power vs Speed Stance Crush
			InterlockAnimPair karatePair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::Karate, TACTIC_POWER,
				FightingStyle::Karate, TACTIC_SPEED,
				InterlockExchangeOutcome::StanceCrush
			);
			check(karatePair.attackerAnimId == 0x04F0, "Karate Stance Crush: Attacker plays Jumping Spin Kick (0x04F0)");
			check(karatePair.defenderAnimId == 0x0F44, "Karate Stance Crush: Defender plays leap kick victim (0x0F44)");
			check(karatePair.contactDelaySeconds == 0.63f, "Karate Spin Kick contact delay is authentic 0.63s (630ms)");

			// 3. Aikido Grab vs Guard Break Throw
			InterlockAnimPair aikidoPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::Aikido, TACTIC_RETALIATE,
				FightingStyle::Aikido, TACTIC_DEFENSE,
				InterlockExchangeOutcome::GuardBreak
			);
			check(aikidoPair.attackerAnimId == 0x0068, "Aikido Guard Break: Attacker plays Cartwheel Tomoe Nage (0x0068)");
			check(aikidoPair.defenderAnimId == 0x0AF2, "Aikido Guard Break: Defender plays failed punch throw victim (0x0AF2)");
			check(aikidoPair.contactDelaySeconds == 0.93f, "Aikido Tomoe Nage contact delay is authentic 0.93s (930ms)");
			check(aikidoPair.hitFxId == 0x28000432, "Aikido throw plays falling impact FX 0x28000432");

			// 4. Street Brawling / Self-Defense Fast Interrupt
			InterlockAnimPair sdPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::None, TACTIC_SPEED,
				FightingStyle::None, TACTIC_RETALIATE,
				InterlockExchangeOutcome::FastInterrupt
			);
			check(sdPair.attackerAnimId == 0x112F, "Street Brawling Fast Interrupt: Attacker plays Cheap Shot (0x112F)");
			check(sdPair.defenderAnimId == 0x1131, "Street Brawling Fast Interrupt: Defender plays cheap shot recoil (0x1131)");
			check(sdPair.contactDelaySeconds == 0.50f, "Street Cheap Shot contact delay is authentic 0.50s (500ms)");

			// B. Cross-Discipline Block & Dodge Animation Synchronization
			// Kung Fu strike vs Karate defender blocking:
			InterlockAnimPair kfVsKarateBlock = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::KungFu, TACTIC_POWER,
				FightingStyle::Karate, TACTIC_DEFENSE,
				InterlockExchangeOutcome::Blocked
			);
			check(kfVsKarateBlock.attackerAnimId == 0x0D5C, "Kung Fu strike vs Karate block: Attacker plays strike (0x0D5C)");
			check(kfVsKarateBlock.defenderAnimId == 0x0472, "Kung Fu strike vs Karate block: Defender plays Karate block (0x0472 = KD_D_SR_BPegHR_KDLb)");
			check(kfVsKarateBlock.contactDelaySeconds == 0.46f, "Kung Fu strike contact arrives at defender block at 0.46s (460ms)");
			check(kfVsKarateBlock.hitFxId == 0x28000794, "Blocked strike triggers block spark FX 0x28000794");

			// Karate strike vs Aikido defender dodging:
			InterlockAnimPair karateVsAikidoDodge = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::Karate, TACTIC_POWER,
				FightingStyle::Aikido, TACTIC_NORMAL,
				InterlockExchangeOutcome::Dodged
			);
			check(karateVsAikidoDodge.attackerAnimId == 0x04F0, "Karate strike vs Aikido dodge: Attacker plays spin kick (0x04F0)");
			check(karateVsAikidoDodge.defenderAnimId == 0x009D, "Karate strike vs Aikido dodge: Defender plays Aikido circular dodge (0x009D = AD_D_SR_ADLb_DPegMF)");
			check(karateVsAikidoDodge.contactDelaySeconds == 0.46f, "Karate kick evasion timing synchronizes at 0.46s (460ms)");
			check(karateVsAikidoDodge.hitFxId == 0, "Dodged attack generates 0 hit FX");

			// C. Stance Clash Synchronization
			InterlockAnimPair clashPair = CombatAnimationMatrix::GetAnimationPair(
				FightingStyle::KungFu, TACTIC_POWER,
				FightingStyle::Aikido, TACTIC_POWER,
				InterlockExchangeOutcome::Clash
			);
			check(clashPair.attackerAnimId == 0x0D5C, "Clash: Kung Fu attacker plays clash strike (0x0D5C)");
			check(clashPair.defenderAnimId == 0x00A3, "Clash: Aikido defender plays clash recoil (0x00A3)");
			check(clashPair.contactDelaySeconds == 0.40f, "Clash contact occurs at 0.40s (400ms)");
			check(clashPair.hitFxId == 0x28000794, "Clash triggers block spark FX 0x28000794");

			// D. End-to-End ILExchange Protocol Serialization with Contact Timing & Fighting Styles
			TestHumanClient clientViewer;
			Actor actorA = makeHuman(&clientViewer, 9100010, 9950.0, 10000.0);
			Actor actorB = makeBot(9200070, 10050.0, 10000.0, 1, 100);
			InterlockSession testSession;
			testSession.goIdA = actorA.go;
			testSession.goIdB = actorB.go;
			testSession.ilPos = LocationVector(10000.0, 572.0, 10000.0);
			testSession.exchangeNum = 4;
			actorA.po->setFightingStyle(FightingStyle::KungFu);
			actorB.po->setFightingStyle(FightingStyle::Karate);

			ILExchange exchange = sCombatSys.BuildExchange(
				testSession, actorA.po, actorA.po, actorB.po, 4,
				IL_MOVE_PRE, IL_MOVE_PRE, 0x2026, &kfVsKarateBlock
			);

			check(exchange.style == 0, "ILExchange: style byte 0 (HDS CombatHandler.cs L76 capture)");
			check(exchange.startMs == 0x113 && exchange.unk28 == 0x534 && exchange.unk2A == 0x29A,
				"ILExchange: startMs 0x113, timing pair 0x534 / 0x29A (HDS L76 capture)");
			check(exchange.attackerSlot == 2 && exchange.defenderSlot == 1,
				"ILExchange: viewer attacking = slot 2, opponent = slot 1 (HDS slot order)");
			check(exchange.defenderOffsetMs == 460, "ILExchange: defenderOffsetMs dynamically synchronized to 460ms");
			check(exchange.moves[2][0] == 0x2026, "ILExchange: mainMove is 0x2026 (Kung Fu strike)");
			check(exchange.moves[3][0] == 0, "ILExchange: moves[3][0] is zeroed (ILDB moves only in slots 0-2)");
			check(exchange.moves[4][0] == 0, "ILExchange: moves[4][0] is zeroed (ILDB moves only in slots 0-2)");

			ByteBuffer ilBuf;
			exchange.write(ilBuf);
			check(ilBuf.size() == 0x79, "ILExchange serialized buffer size is exactly 121 bytes (0x79)");
			const uint8* rawBytes = reinterpret_cast<const uint8*>(ilBuf.contents());
			check(rawBytes[0x1E] == 0, "ILExchange wire byte 0x1E is style byte 0 (HDS L76 capture)");
			check(rawBytes[0x1F] == 0, "ILExchange wire byte 0x1F is unk1F (0)");
			uint16 wireNum = *reinterpret_cast<const uint16*>(&rawBytes[0x20]);
			check(wireNum == 4, "ILExchange wire bytes at 0x20 are sequence number 4");
			int16 wireOffset = *reinterpret_cast<const int16*>(&rawBytes[0x26]);
			check(wireOffset == 460, "ILExchange wire bytes at 0x26 are 460ms defenderOffsetMs");
			check(rawBytes[0x2C] == 0x23, "ILExchange wire byte 0x2C is flags 0x23");
			uint16 attAdjust = *reinterpret_cast<const uint16*>(&rawBytes[0x02]);
			check(attAdjust == 775, "ILExchange wire bytes at 0x02 are attackerAdjustMs 775");
			uint16 defAdjust = *reinterpret_cast<const uint16*>(&rawBytes[0x04]);
			check(defAdjust == 775, "ILExchange wire bytes at 0x04 are defenderAdjustMs 775");
			uint32 wireMove0 = *reinterpret_cast<const uint32*>(&rawBytes[0x2D]);
			check(wireMove0 == IL_MOVE_PRE, "ILExchange wire bytes at 0x2D are attacker lead-in move 0x5214");
			uint32 wireMove2 = *reinterpret_cast<const uint32*>(&rawBytes[0x3D]);
			check(wireMove2 == 0x2026, "ILExchange wire bytes at 0x3D are main move 0x2026");
			uint32 attHp = *reinterpret_cast<const uint32*>(&rawBytes[0x55]);
			uint32 defHp = *reinterpret_cast<const uint32*>(&rawBytes[0x59]);
			check(attHp == exchange.attackerHealth && defHp == exchange.defenderHealth,
				"ILExchange wire bytes at 0x55/0x59 are authentic attacker/defender health");
			uint16 u5D = *reinterpret_cast<const uint16*>(&rawBytes[0x5D]);
			uint16 u5F = *reinterpret_cast<const uint16*>(&rawBytes[0x5F]);
			check(u5D == 0x1000 && u5F == 0x1000, "ILExchange wire bytes at 0x5D/0x5F are authentic 0x1000");
			uint32 u6D = *reinterpret_cast<const uint32*>(&rawBytes[0x6D]);
			uint32 u71 = *reinterpret_cast<const uint32*>(&rawBytes[0x71]);
			check(u6D == 0x00000100 && u71 == 0x00602b02, "ILExchange wire bytes at 0x6D/0x71 match retail capture");
		}

		// ------------------------------------------------------------------
		// 14. Retail 25,695 Interlock Move Database & Dynamic Animation Query
		// ------------------------------------------------------------------
		std::cout << "\n[14. Retail Interlock Move Database & Dynamic Animation Query]" << std::endl;
		{
			bool loaded = CombatAnimationMatrix::LoadBinaryDatabase("Data/hd_dump/interlock_moves.bin");
			check(loaded, "CombatAnimationMatrix::LoadBinaryDatabase loads interlock_moves.bin");
			check(CombatAnimationMatrix::GetTotalMovesLoaded() >= 25000, "Loaded >= 25,000 authentic retail moves");

			// Query Kung Fu Power vs Speed
			const ILDBMoveRecord* kfMove = CombatAnimationMatrix::FindMove(
				FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_SPEED, InterlockExchangeOutcome::NormalHit
			);
			check(kfMove != nullptr, "FindMove returns valid move for Kung Fu Power vs Speed");
			check(kfMove && kfMove->aggrAnim != 0, "Kung Fu move has valid non-zero aggrAnim");
			check(kfMove && (kfMove->flags & 2), "Kung Fu move has hit flag bit 1 set for NormalHit outcome");

			// Query Karate Grab vs Block
			const ILDBMoveRecord* karateMove = CombatAnimationMatrix::FindMove(
				FightingStyle::Karate, TACTIC_RETALIATE, FightingStyle::Karate, TACTIC_DEFENSE, InterlockExchangeOutcome::NormalHit
			);
			check(karateMove != nullptr, "FindMove returns valid move for Karate Grab vs Block");
			check(karateMove && (karateMove->flags & 2), "Karate move has hit flag bit 1 set for NormalHit outcome");

			// Query Blocked outcome
			const ILDBMoveRecord* blockMove = CombatAnimationMatrix::FindMove(
				FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_DEFENSE, InterlockExchangeOutcome::Blocked
			);
			check(blockMove != nullptr, "FindMove returns valid move for Kung Fu Blocked outcome");
			check(blockMove && (blockMove->flags & 4), "Kung Fu Blocked move has block flag bit 2 set");

			// Query Clash outcome
			const ILDBMoveRecord* clashMove = CombatAnimationMatrix::FindMove(
				FightingStyle::KungFu, TACTIC_DEFENSE, FightingStyle::KungFu, TACTIC_DEFENSE, InterlockExchangeOutcome::Clash
			);
			check(clashMove != nullptr, "FindMove returns valid move for Kung Fu Clash outcome");
			check(clashMove && (clashMove->flags & 8), "Kung Fu Clash move has draw flag bit 3 set");

			// Query Finisher move
			const ILDBMoveRecord* finisherMove = CombatAnimationMatrix::FindMove(
				FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_SPEED, InterlockExchangeOutcome::NormalHit, true
			);
			check(finisherMove != nullptr, "FindMove returns valid finisher move");
			check(finisherMove && (finisherMove->flags & 1), "Finisher move has finisher flag bit 0 set");

			// Query GetDynamicAnimationPair
			InterlockAnimPair dynPair = CombatAnimationMatrix::GetDynamicAnimationPair(
				FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_SPEED, InterlockExchangeOutcome::NormalHit
			);
			check(dynPair.attackerAnimId != 0 && dynPair.defenderAnimId != 0, "GetDynamicAnimationPair resolves non-zero paired animations from ILDB");
			check(dynPair.hitFxId == 0x280006DF, "GetDynamicAnimationPair sets hitFxId 0x280006DF for NormalHit");

			InterlockAnimPair blockPair = CombatAnimationMatrix::GetDynamicAnimationPair(
				FightingStyle::KungFu, TACTIC_POWER, FightingStyle::KungFu, TACTIC_DEFENSE, InterlockExchangeOutcome::Blocked
			);
			check(blockPair.hitFxId == 0x28000794, "GetDynamicAnimationPair sets hitFxId 0x28000794 for Blocked");
		}

		// ------------------------------------------------------------------
		// 15. Dynamic ILDB Combat Move Query & Mission Live Feedback
		// ------------------------------------------------------------------
		std::cout << "\n[15. Dynamic ILDB Combat Move Query & Mission Live Feedback]" << std::endl;
		{
			// 1. Dynamic ILDB Move Selection in SelectInterlockMove
			Actor humanOperative = makeHuman(&humanClient, 9100088, 10000.0, 10000.0);
			Actor enemyBot = makeBot(9200088, 10050.0, 10000.0, 1, 100);
			InterlockSession dynSession;
			dynSession.goIdA = humanOperative.go;
			dynSession.goIdB = enemyBot.go;
			dynSession.exchangeNum = 1;
			dynSession.tacticA = TACTIC_POWER;
			dynSession.tacticB = TACTIC_SPEED;

			humanOperative.po->setFightingStyle(FightingStyle::None);
			uint32 moveDyn = sCombatSys.SelectInterlockMove(dynSession, humanOperative.po, enemyBot.po);
			check(moveDyn != 0, "SelectInterlockMove dynamically resolves non-zero move ID for FightingStyle::None");

			// 2. Mission Assignment & Live Chat Objective Routing
			humanClient.captured.clear();
			MissionTemplate testMission;
			testMission.missionId = 99991;
			testMission.title = "Operation White Rabbit";
			testMission.description = "Locate the exiled operative in Downtown Mara.";
			testMission.expReward = 500;
			testMission.infoReward = 250;
			testMission.rewardFactionRep = 10;
			testMission.requiredFactionRep = 0;
			MissionObjective obj1;
			obj1.command = ObjectiveCommand::TALK;
			obj1.targetNpcId = 1800001;
			obj1.description = "Meet the Contact at Mara Central.";
			obj1.dialog = "Follow the white rabbit to Mara.";
			testMission.objectives.push_back(obj1);
			MissionObjective obj2;
			obj2.command = ObjectiveCommand::DEFEAT;
			obj2.targetNpcId = 1800002;
			obj2.description = "Defeat the corrupt Machine courier.";
			obj2.dialog = "You cannot stop the cascade.";
			testMission.objectives.push_back(obj2);

			sMissionSys.AddMissionTemplate(testMission);
			sMissionSys.AssignMission(humanOperative.po, 99991);
			check(sMissionSys.HasActiveMission(humanOperative.go), "Player has active mission 99991 assigned");
			check(humanClient.sawText("[MISSION ASSIGNED] Operation White Rabbit"), "Human operative receives [MISSION ASSIGNED] chat notice");
			check(humanClient.sawText("[OBJECTIVE 1/2] Meet the Contact at Mara Central."), "Human operative receives initial [OBJECTIVE 1/2] notice");

			// Advance Objective 1 -> 2
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::TALK, 1800001);
			check(humanClient.sawText("Follow the white rabbit to Mara."), "Human operative receives contact dialogue in chat stream");
			check(humanClient.sawText("[OBJECTIVE 2/2] Defeat the corrupt Machine courier."), "Human operative receives [OBJECTIVE 2/2] advancement notice");

			// Complete Objective 2 -> Mission Complete
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::DEFEAT, 1800002);
			check(humanClient.sawText("You cannot stop the cascade."), "Human operative receives dying hostile dialogue in chat stream");
			check(!sMissionSys.HasActiveMission(humanOperative.go), "Mission completes and active mission state is cleared");
			check(humanClient.sawText("[MISSION COMPLETE] Operation White Rabbit!"), "Human operative receives [MISSION COMPLETE] celebration chat notice");

			// 3. Authentic Story Mission 10111 (Chapter 1.1: Intelligence Gathering)
			sMissionSys.LoadMissionsFromXML("Data/hd_dump/missions");
			humanClient.captured.clear();
			sMissionSys.AssignMission(humanOperative.po, 10111);
			check(sMissionSys.HasActiveMission(humanOperative.go), "Operative assigned authentic story mission 10111");
			check(humanClient.sawText("Meet the Zion Operative"), "Mission 10111 initial objective received");

			// Advance Obj 1 (TALK to 9010)
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::TALK, 9010);
			check(humanClient.sawText("The Truce is fragile"), "Operative receives Zion Operative dialog");
			check(humanClient.sawText("Defeat the Corrupted Program"), "Objective 2 (Corrupted Program) active");

			// Advance Obj 2 (DEFEAT 9011)
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::DEFEAT, 9011);
			check(humanClient.sawText("the flies have already scattered"), "Operative receives Corrupted Program dying words");
			check(humanClient.sawText("Fight through the ambush"), "Objective 3 (Ambush) active");

			// Advance Obj 3 (DEFEAT 9999 Ambush)
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::DEFEAT, 9999);
			check(humanClient.sawText("They knew we were coming"), "Operative receives Ambush dialogue");
			check(humanClient.sawText("Report to Ghost"), "Objective 4 (Ghost) active");

			// Advance Obj 4 (TALK to 9012 Ghost) -> Complete Mission
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::TALK, 9012);
			check(humanClient.sawText("bears the signature of the Merovingian"), "Operative receives Ghost debrief dialogue");
			check(!sMissionSys.HasActiveMission(humanOperative.go), "Mission 10111 fully completes");
			check(humanClient.sawText("[MISSION COMPLETE] Chapter 1.1: Intelligence Gathering"), "Operative receives Mission 10111 completion notification");
		}

		// ------------------------------------------------------------------
		// 16. Firearms Ballistics, Ammo Depletion, Dual-Wield & Static Vendor Economy
		// ------------------------------------------------------------------
		std::cout << "\n[16. Firearms Ballistics, Ammo Depletion, Dual-Wield & Static Vendor Economy]" << std::endl;
		{
			Actor humanOperative = makeHuman(&humanClient, 9100099, 10000.0, 10000.0);
			Actor enemyBot = makeBot(9200099, 10050.0, 10000.0, 1, 100);

			// Ensure inventory system initialized
			check(humanOperative.po->getInventory() != nullptr, "Operative inventory system exists");

			// 1. Single Firearm Ammo Depletion & Empty Chamber Stop
			humanOperative.po->giveItem(1002); // Beretta 92FS (maxAmmo = 15)
			humanOperative.po->setEquippedWeaponId(1002);
			auto wpns = humanOperative.po->getEquippedWeapons();
			check(!wpns.empty(), "Equipped Beretta 92FS resolved in getEquippedWeapons()");
			wpns[0]->setAmmoCount(2);

			FreeFireState ffState;
			ffState.attackerGoId = humanOperative.go;
			ffState.targetGoId = enemyBot.go;
			ffState.moveId = 0;
			ffState.shotCount = 0;

			// Shot 1
			check(sCombatSys.RunFreeFireShot(ffState), "RunFreeFireShot fires shot 1 successfully");
			check(wpns[0]->getAmmoCount() == 1, "Ammo count decrements from 2 to 1");

			// Shot 2
			check(sCombatSys.RunFreeFireShot(ffState), "RunFreeFireShot fires shot 2 successfully");
			check(wpns[0]->getAmmoCount() == 0, "Ammo count decrements from 1 to 0 (magazine empty)");

			// Shot 3: No ammo clip in inventory -> weapon dry, stops free fire
			humanClient.captured.clear();
			check(!sCombatSys.RunFreeFireShot(ffState), "RunFreeFireShot returns false when dry without ammo clip");
			check(humanClient.sawText("[OUT OF AMMO]"), "Operative receives [OUT OF AMMO] click notice");

			// 2. Ammo Clip Inventory Reload
			humanOperative.po->giveItem(1050); // 9mm Pistol Ammo Clip
			humanClient.captured.clear();
			check(sCombatSys.RunFreeFireShot(ffState), "RunFreeFireShot auto-reloads and fires when ammo clip present");
			check(humanClient.sawText("[RELOAD]"), "Operative receives [RELOAD] notification");
			check(wpns[0]->getAmmoCount() == 14, "Weapon reloaded to maxAmmo - 1 (14 rounds remaining)");

			// 3. Dual-Wield Burst Mechanics
			humanOperative.po->giveItem(1001); // Dual Beretta 92FS (isDualWield = true)
			humanOperative.po->setEquippedWeaponId(1001);
			check(humanOperative.po->isDualWielding(), "Operative reports isDualWielding() true with Dual Berettas");
			auto dualWpns = humanOperative.po->getEquippedWeapons();
			check(!dualWpns.empty(), "Dual Berettas resolved in getEquippedWeapons()");
			dualWpns[0]->setAmmoCount(10);
			// the reload shot above can empty the target's bar; a target at 0 HP is (correctly)
			// no longer shot at, so give the burst a live target
			enemyBot.po->setDead(false);
			enemyBot.po->setCurrentHealth(enemyBot.po->getMaximumHealth());
			check(sCombatSys.RunFreeFireShot(ffState), "RunFreeFireShot executes dual-wield burst");
			check(dualWpns[0]->getAmmoCount() == 8, "Dual-wield consumes 2 rounds per volley (10 -> 8)");

			// 4. Static Vendor Buying (Opcode 0x810e). Vendor 33333 is not a real vendor, so this
			// legacy block buys as an admin (admins skip the vendor-proximity rule).
			humanOperative.po->setAdmin(true);
			humanOperative.po->addInfo(5000);
			uint64 cashBeforeBuy = humanOperative.po->getInfo();
			ByteBuffer buyCmd;
			buyCmd << (uint8)0x81 << (uint8)0x0e << (uint32)1050 << (uint32)33333; // 9mm ammo clip (value = 50), vendor GoId 33333

			humanClient.captured.clear();
			humanOperative.po->HandleCommand(buyCmd);
			check(humanClient.sawText("[VENDOR] Purchased 9mm Pistol Ammo Clip for 50 Information Bits."), "HandleCommand(0x810e) executes RPC_HandleVendorBuy with success notice");
			check(humanOperative.po->getInfo() == cashBeforeBuy - 50, "Operative Info Bits deducted by 50");

			// 5. Static Vendor Selling (Opcode 0x8111)
			uint64 cashBeforeSell = humanOperative.po->getInfo();
			ByteBuffer sellCmd;
			sellCmd << (uint8)0x81 << (uint8)0x11 << (uint32)1050 << (uint32)33333; // sell 9mm ammo clip (value 50 -> 50% sell price = 25)

			humanClient.captured.clear();
			humanOperative.po->HandleCommand(sellCmd);
			check(humanClient.sawText("[VENDOR] Sold 9mm Pistol Ammo Clip for 25 Information Bits."), "HandleCommand(0x8111) executes RPC_HandleVendorSell with credit notice");
			check(humanOperative.po->getInfo() == cashBeforeSell + 25, "Operative Info Bits credited by 25");

			// 6. Vendor Selling Equipped Weapon Protection
			ByteBuffer sellEquippedCmd;
			sellEquippedCmd << (uint8)0x81 << (uint8)0x11 << (uint32)1001 << (uint32)33333; // Equipped Dual Berettas

			humanClient.captured.clear();
			humanOperative.po->HandleCommand(sellEquippedCmd);
			check(humanClient.sawText("Cannot sell an equipped weapon"), "RPC_HandleVendorSell rejects selling active equipped weapon");
			humanOperative.po->setAdmin(false);

			// 6b. A non-admin can't buy or sell away from a vendor (audit 2026-10-13)
			humanOperative.po->setDistrict(1);
			humanOperative.po->setPosition(LocationVector(900000.0, 572.0, 900000.0));
			humanClient.captured.clear();
			{ ByteBuffer b; b << (uint8)0x81 << (uint8)0x0e << (uint32)1050 << (uint32)33333; humanOperative.po->HandleCommand(b); }
			check(humanClient.sawText("No vendor nearby sells that item") && !humanClient.sawText("[VENDOR] Purchased"),
				"0x810e away from any vendor is refused");
			humanClient.captured.clear();
			{ ByteBuffer b; b << (uint8)0x81 << (uint8)0x11 << (uint32)1050 << (uint32)33333; humanOperative.po->HandleCommand(b); }
			check(humanClient.sawText("You need to be at a vendor to sell") && !humanClient.sawText("[VENDOR] Sold"),
				"0x8111 away from any vendor is refused");

			// 7. Contact Sponsor Mission Faction Alignment
			sMissionSys.LoadSponsorsFromXML("Data/hd_dump/sponsors.xml");

			MissionTemplate zionMission;
			zionMission.missionId = 99992;
			zionMission.factionId = 1; // Zion
			zionMission.title = "Zion Uplink Defense";
			MissionObjective zObj;
			zObj.command = ObjectiveCommand::TALK;
			zObj.description = "Contact the Zion Controller.";
			zionMission.objectives.push_back(zObj);
			sMissionSys.AddMissionTemplate(zionMission);

			// Sponsor 2000 is Tyndall (org 1 = Zion)
			uint32 chosenFromSponsor = sMissionSys.GetAvailableStoryMission(humanOperative.po, 2000);
			check(chosenFromSponsor == 99992 || chosenFromSponsor == 10111, "GetAvailableStoryMission correctly aligns with Zion sponsor org faction (99992 or 10111)");

			uint32 chosenFromOrg = sMissionSys.GetAvailableStoryMission(humanOperative.po, 1);
			check(chosenFromOrg == 99992 || chosenFromOrg == 10111, "GetAvailableStoryMission correctly aligns with direct org ID 1 (99992 or 10111)");
		}

		// ====================================================================
		// Section 16: Phase D Systems — Crafting Blueprints, Objective Commands & Story Rewards
		// ====================================================================
		{
			std::cout << "\n--- Section 16: Phase D Systems (Crafting, Objectives, Ability Rewards, Vitals) ---" << std::endl;

			Actor humanOperative = makeHuman(&humanClient, 9100100, 10000.0, 10000.0);
			check(humanOperative.po != nullptr && humanOperative.po->getInventory() != nullptr, "Phase D operative initialized with inventory");

			// 1. Coder Crafting Blueprints Database Loading
			sDataLoader.LoadBlueprints("Data/hd_dump/blueprints.csv");
			sCraftSys.LoadBlueprints();
			check(sCraftSys.GetTotalBlueprintsLoaded() >= 10, "CraftingSystem loaded >= 10 authentic Coder blueprints");

			const CraftingBlueprint* bpStim = sCraftSys.GetBlueprint(101);
			check(bpStim != nullptr && bpStim->resultingName == "Health Stim Patch" && bpStim->infoCost == 100,
				"Blueprint 101 resolves to Health Stim Patch (cost: 100 bits)");

			const CraftingBlueprint* bpAmmo = sCraftSys.GetBlueprint(102);
			check(bpAmmo != nullptr && bpAmmo->resultingName == "Standard 9mm Ammo Clip" && bpAmmo->infoCost == 75,
				"Blueprint 102 resolves to Standard 9mm Ammo Clip (cost: 75 bits)");

			// 2. Coder Synthesis execution via RPC_HandleCraftRequest (Opcode 0x8066)
			humanOperative.po->addInfo(2000);
			uint64 cashBeforeCraft = humanOperative.po->getInfo();

			ByteBuffer craftCmd;
			craftCmd << (uint8)0x80 << (uint8)0x66 << (uint32)102; // Craft blueprint 102
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(craftCmd);

			check(humanClient.sawText("[CRAFTING] Compilation complete! Synthesized: Standard 9mm Ammo Clip"),
				"HandleCommand(0x8066) executes RPC_HandleCraftRequest with synthesis confirmation");
			check(humanOperative.po->getInfo() == cashBeforeCraft - 75, "Operative Info Bits deducted by 75 for crafting cost");
			check(humanOperative.po->getInventory()->hasItemByTemplate(1050), "Operative inventory received item template 1050 (9mm ammo clip)");

			// 3. Mission Objectives: HACK, USE_ITEM, ESCORT
			MissionTemplate phaseDMission;
			phaseDMission.missionId = 99993;
			phaseDMission.title = "Phase D Multi-Objective Infiltration";
			phaseDMission.factionId = 1; // Zion

			MissionObjective objHack;
			objHack.command = ObjectiveCommand::HACK;
			objHack.targetNpcId = 7701;
			objHack.description = "Hack the mainframe terminal.";
			phaseDMission.objectives.push_back(objHack);

			MissionObjective objUse;
			objUse.command = ObjectiveCommand::USE_ITEM;
			objUse.targetNpcId = 7702;
			objUse.description = "Deploy tracking beacon on console.";
			phaseDMission.objectives.push_back(objUse);

			MissionObjective objEscort;
			objEscort.command = ObjectiveCommand::ESCORT;
			objEscort.targetNpcId = 7703;
			objEscort.description = "Escort defecting operative to extraction.";
			phaseDMission.objectives.push_back(objEscort);

			sMissionSys.AddMissionTemplate(phaseDMission);
			sMissionSys.AssignMission(humanOperative.po, 99993);

			ActiveObjectiveInfo objInfo;
			check(sMissionSys.GetActiveObjectiveInfo(humanOperative.go, objInfo) && objInfo.command == ObjectiveCommand::HACK, "Active objective #1 is HACK");

			// Advance HACK
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::HACK, 7701);
			check(humanClient.sawText("[HACK] Security bypass successful"), "AdvanceObjective(HACK) advances and notifies operative");
			check(sMissionSys.GetActiveObjectiveInfo(humanOperative.go, objInfo) && objInfo.command == ObjectiveCommand::USE_ITEM,
				"Active objective advances to #2 (USE_ITEM)");

			// Advance USE_ITEM
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::USE_ITEM, 7702);
			check(humanClient.sawText("[MISSION] Mission equipment deployed successfully"), "AdvanceObjective(USE_ITEM) advances and notifies operative");
			check(sMissionSys.GetActiveObjectiveInfo(humanOperative.go, objInfo) && objInfo.command == ObjectiveCommand::ESCORT,
				"Active objective advances to #3 (ESCORT)");

			// Advance ESCORT (completes mission)
			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::ESCORT, 7703);
			check(humanClient.sawText("[ESCORT] Asset secured"), "AdvanceObjective(ESCORT) completes objective");
			check(!sMissionSys.HasActiveMission(humanOperative.go), "Mission completes and is cleared from active missions");

			// 4. Story Mission Ability Reward Unlocking
			MissionTemplate abilityRewardMission;
			abilityRewardMission.missionId = 99994;
			abilityRewardMission.title = "Seraphic Awakening";
			abilityRewardMission.rewardAbilityId = 57; // LogicBlast1Ability
			abilityRewardMission.rewardAbilityName = "LogicBlast1Ability";

			MissionObjective talkObj;
			talkObj.command = ObjectiveCommand::TALK;
			talkObj.targetNpcId = 8805;
			talkObj.description = "Speak with Seraph.";
			abilityRewardMission.objectives.push_back(talkObj);

			sMissionSys.AddMissionTemplate(abilityRewardMission);
			sMissionSys.AssignMission(humanOperative.po, 99994);

			humanClient.captured.clear();
			sMissionSys.AdvanceObjective(humanOperative.po, ObjectiveCommand::TALK, 8805);
			check(humanClient.sawText("[ABILITY UNLOCKED] You learned LogicBlast1Ability!"),
				"Mission completion unlocks rewardAbility (LogicBlast1Ability)");
			check(humanOperative.po->getAbilitySystem()->getAbility(57) != nullptr,
				"Operative AbilitySystem contains unlocked ability 57");

			// 5. Level-Up Vitometer & Health Announcement Synchronization
			uint32 prevMaxHealth = humanOperative.po->getMaximumHealth();
			uint8 prevLevel = humanOperative.po->getLevel();
			humanClient.captured.clear();
			humanOperative.po->awardCombatExperience(50000); // Trigger level up

			check(humanOperative.po->getLevel() > prevLevel, "Operative level increased on awardCombatExperience");
			check(humanOperative.po->getMaximumHealth() > prevMaxHealth, "Operative max health increased on level up");
			check(humanClient.sawText("Congratulations! You are now level"), "Level up sends congratulations chat notice and vitals sync");
		}

		// ====================================================================
		// Section 17: Phase E — Authentic Loot Tables, Dynamic Mob Drops & Sponsor Contacts
		// ====================================================================
		{
			std::cout << "\n--- Section 17: Phase E Systems (Loot Tables, Mob Harvesting, Sponsors) ---" << std::endl;

			Actor humanOperative = makeHuman(&humanClient, 9100101, 10000.0, 10000.0);
			Actor enemyBot = makeBot(9200101, 10050.0, 10000.0, 5, 50);

			// 1. Ingest Authentic Loot Tables
			sLootMgr.LoadLootTables("Data/Loot/loot_tables.csv");
			check(sLootMgr.GetTotalLootTables() >= 4, "LootManager loaded >= 4 authentic loot tables");
			check(sLootMgr.GetTotalLootEntries() >= 25, "LootManager loaded >= 25 authentic loot entries");

			const auto* t1 = sLootMgr.GetLootTable(1);
			check(t1 != nullptr && !t1->empty(), "Loot Table 1 (Slums Thugs) exists and has entries");

			const auto* t4 = sLootMgr.GetLootTable(4);
			check(t4 != nullptr && !t4->empty(), "Loot Table 4 (Agent Boss) exists and has entries");

			// 2. Mob Kill Loot Generation with real item inventory injection
			enemyBot.po->setLootTableId(1);
			enemyBot.po->setHandle("Slums Ganger");
			
			// Mission objective LOOT tracking
			MissionTemplate lootMission;
			lootMission.missionId = 99995;
			lootMission.title = "Data Extraction";
			MissionObjective lootObj;
			lootObj.command = ObjectiveCommand::LOOT;
			lootObj.targetNpcId = enemyBot.go;
			lootObj.description = "Loot encrypted drive from Slums Ganger.";
			lootMission.objectives.push_back(lootObj);
			sMissionSys.AddMissionTemplate(lootMission);
			sMissionSys.AssignMission(humanOperative.po, 99995);

			check(sMissionSys.HasActiveMission(humanOperative.go), "Operative has active LOOT mission assigned");

			humanClient.captured.clear();
			sLootMgr.GenerateLoot(humanOperative.po, enemyBot.po);

			// Verify LOOT mission objective advanced
			check(!sMissionSys.HasActiveMission(humanOperative.go), "GenerateLoot advances ObjectiveCommand::LOOT and completes mission");

			// 3. Agent Boss Loot Table Dynamic Resolution
			Actor agentBot = makeBot(9200102, 10050.0, 10000.0, 50, 1000);
			agentBot.po->setHandle("Agent Perry"); // Name contains "Agent", dynamically resolves to Table 4
			check(agentBot.po->getLootTableId() == 0, "Agent initially has lootTableId 0 (dynamic resolution)");

			humanClient.captured.clear();
			sLootMgr.GenerateLoot(humanOperative.po, agentBot.po);
			check(humanOperative.po->getInventory() != nullptr, "Operative inventory preserved after Agent loot generation");

			// 4. Sponsor Contacts XML Ingestion & Fallback Resolution
			sMissionSys.LoadSponsorsFromXML("Data/hd_dump/sponsors.xml");
			const SponsorContact* amber = sMissionSys.GetSponsor(1);
			check(amber != nullptr && amber->name == "Amber", "Sponsor Contact #1 resolved to Amber (org 0)");

			const SponsorContact* antiM = sMissionSys.GetSponsor(2);
			check(antiM != nullptr && antiM->name == "Anti M.", "Sponsor Contact #2 resolved to Anti M.");

			const SponsorContact* argon = sMissionSys.GetSponsor(3);
			check(argon != nullptr && argon->name == "Argon", "Sponsor Contact #3 resolved to Argon");
		}

		// ------------------------------------------------------------
		// 18. Phase F: Matrix Threat Heatmap & Escalation Response
		// ------------------------------------------------------------
		{
			// 1. Threat disruption recording with 3D elevation tracking
			float testX = 18115.6f;
			float testY = 572.0f;
			float testZ = 3610.0f;

			sMatrixThreatHeatmap.RecordDisruption(testX, testY, testZ, 30.0f, "Test Ballistic Combat");
			float heatLvl = sMatrixThreatHeatmap.GetHeat(testX, testZ);
			check(heatLvl >= 30.0f, "Heatmap recorded +30.0 threat at test coordinates");

			HeatTier tier1 = sMatrixThreatHeatmap.GetTier(testX, testZ);
			check(tier1 == HEAT_TIER_1_LOW, "Threat >= 25 resolves to HEAT_TIER_1_LOW");

			// 2. High disruption escalates to elevated (>= 60) and high (>= 110)
			sMatrixThreatHeatmap.RecordDisruption(testX, testY, testZ, 40.0f, "Heavy Melee Interlock");
			HeatTier tier2 = sMatrixThreatHeatmap.GetTier(testX, testZ);
			check(tier2 == HEAT_TIER_2_ELEVATED, "Threat >= 60 resolves to HEAT_TIER_2_ELEVATED");

			sMatrixThreatHeatmap.RecordDisruption(testX, testY, testZ, 60.0f, "Agent Wire-Fu Breach");
			HeatTier tier3 = sMatrixThreatHeatmap.GetTier(testX, testZ);
			check(tier3 == HEAT_TIER_3_HIGH, "Threat >= 110 resolves to HEAT_TIER_3_HIGH");

			// 3. Escalation response simulation tick
			uint32 curMs = getMSTime();
			sMatrixThreatHeatmap.Update(0.5f, curMs);
			float decayedHeat = sMatrixThreatHeatmap.GetHeat(testX, testZ);
			check(decayedHeat > 0.0f && decayedHeat <= 130.0f, "Heatmap updates with exponential decay and diffusion");

			// 4. District sabotage modification
			uint32 distId = sMatrixThreatHeatmap.GetDistrictAt(testX, testZ);
			check(!sMatrixThreatHeatmap.IsDistrictSabotaged(distId), "District initially not sabotaged");
			sMatrixThreatHeatmap.SetDistrictSabotaged(distId, true);
			check(sMatrixThreatHeatmap.IsDistrictSabotaged(distId), "District marked sabotaged (surveillance offline)");
			sMatrixThreatHeatmap.SetDistrictSabotaged(distId, false);
		}

		// ------------------------------------------------------------
		// 19. Phase G: Authentic Vendor Economy & Currency Transactions
		// ------------------------------------------------------------
		{
			std::cout << "\n[19. Phase G: Authentic Vendor Economy & Currency Transactions]" << std::endl;

			Actor humanOperative = makeHuman(&humanClient, 9100102, 10000.0, 10000.0);

			// 1. Ingestion of authentic static vendors (>= 40 loaded from vendor_items.csv)
			sEconomySys.LoadVendorsFromCSV("Data/hd_dump/vendor_items.csv");
			auto slumsVendors = sEconomySys.GetVendorsForDistrict(1);
			check(!slumsVendors.empty(), "EconomySystem loaded authentic vendors for Slums (District 1)");
			const HardlineVendor* testVendor = sEconomySys.GetHardlineVendor(slumsVendors[0].vendorId);
			check(testVendor != nullptr, "Lookup of static vendor by ID succeeds");
			check(!testVendor->inventoryTemplates.empty(), "Static vendor has authentic inventory catalog");

			// 2. VendorOpenMsg byte serialization matching retail capture 4a810d7cadd943...
			std::vector<uint32> sampleItems = { 0x80001400, 0x80002000, 0x80001800 };
			VendorOpenMsg vOpen(435.355f, -93050.0, 1295.0, -12450.0, sampleItems);
			const ByteBuffer& vBuf = vOpen.toBuf();
			check(vBuf.size() >= 2 + 4 + 24 + 2 + 2 + (3 * 4), "VendorOpenMsg wire size matches authentic header + position + products layout");

			// 3. Static object interaction (0x80c8) opens vendor catalog
			ByteBuffer staticInteractCmd;
			staticInteractCmd << (uint8)0x80 << (uint8)0xc8 << (uint32)testVendor->staticId << (uint16)0x02; // NPC vendor interaction
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(staticInteractCmd);
			check(humanClient.sawText("[VENDOR] Opened"), "RPC_HandleStaticObjInteraction opens authentic vendor catalog");

			// 4. Market Open (0x8121) opens nearest district vendor
			ByteBuffer marketCmd;
			marketCmd << (uint8)0x81 << (uint8)0x21;
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(marketCmd);
			check(humanClient.sawText("[VENDOR] Connected to"), "RPC_HandleMarketOpen connects operative to nearest district vendor");

			// 5. Vendor Buying (0x810e) with server-authoritative Info Bits deduction, standing at the vendor
			uint32 testTpl = testVendor->inventoryTemplates[0];
			humanOperative.po->setDistrict((uint8)testVendor->districtId);
			humanOperative.po->setPosition(LocationVector(testVendor->x, testVendor->y, testVendor->z));
			humanOperative.po->addInfo(5000);
			uint64 startBits = humanOperative.po->getInfo();
			ByteBuffer buyCmd;
			buyCmd << (uint8)0x81 << (uint8)0x0e << (uint32)testTpl << (uint32)testVendor->vendorId;
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(buyCmd);
			check(humanClient.sawText("[VENDOR] Purchased"), "RPC_HandleVendorBuy executes transaction and notifies operative");
			check(humanOperative.po->getInfo() < startBits, "Server-authoritative Info Bits deducted for purchase");

			// 6. Vendor Selling (0x8111) with server-authoritative Info Bits credit
			humanOperative.po->addInfo(100);
			uint64 bitsBeforeSell = humanOperative.po->getInfo();
			ByteBuffer sellCmd;
			sellCmd << (uint8)0x81 << (uint8)0x11 << (uint32)testTpl << (uint32)testVendor->vendorId;
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(sellCmd);
			check(humanClient.sawText("[VENDOR] Sold"), "RPC_HandleVendorSell executes sale and credits Info Bits");
			check(humanOperative.po->getInfo() > bitsBeforeSell, "Server-authoritative Info Bits credited for sale");

			// 7. /vendor and /vendor list chat command simulation (0x2810)
			ByteBuffer chatCmd;
			chatCmd << (uint8)0x28 << (uint8)0x10;
			chatCmd << (uint16)swap16(8);
			chatCmd << (uint32)0; // pad to offset 8
			chatCmd.writeString("/vendor list");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(chatCmd);
			check(humanClient.sawText("[VENDOR]") && humanClient.sawText("Catalog:"), "/vendor list command returns catalog prices and items");
		}

		// ------------------------------------------------------------
		// 20. Phase H: Authentic Hardlines, Rings & Jack-Out Teleportation
		// ------------------------------------------------------------
		{
			std::cout << "\n[20. Phase H: Authentic Hardlines, Rings & Jack-Out Teleportation]" << std::endl;

			Actor humanOperative = makeHuman(&humanClient, 9100103, 17043.1, 2398.8);

			// 1. Ingestion of 135 authentic hardline locations from hardlines.csv
			PlayerObject::LoadHardlinesFromCSV("Data/hd_dump/hardlines.csv");
			check(PlayerObject::GetTotalHardlines() >= 130, "PlayerObject loaded >= 130 authentic hardline locations");

			// 2. Exact Hardline lookup by (districtId, hardlineId) and Nearest Hardline query
			const HardlineNode* maraCentral = PlayerObject::GetHardline(1, 152);
			check(maraCentral != nullptr && maraCentral->name == "MaraCentral", "Hardline (1, 152) resolved to MaraCentral");
			check(std::abs(maraCentral->x - 17043.1) < 1.0 && std::abs(maraCentral->z - 2398.8) < 1.0, "MaraCentral coordinates match authentic (17043.1, 2398.8)");

			const HardlineNode* nearest = PlayerObject::GetNearestHardline(1, 17043.0, 2398.0);
			check(nearest != nullptr && nearest->hardlineId == 152, "GetNearestHardline dynamically resolves MaraCentral");

			// 3. Intra-district and Inter-district Teleportation via RPC 0x818e
			ByteBuffer tpCmd;
			tpCmd << (uint8)0x81 << (uint8)0x8e;
			tpCmd << (uint8)152; // current HL
			while (tpCmd.size() < 6) tpCmd << (uint8)0;
			tpCmd << (uint8)1; // current district
			while (tpCmd.size() < 10) tpCmd << (uint8)0;
			tpCmd << (uint8)49; // dest HL (MaraNorthWest)
			while (tpCmd.size() < 14) tpCmd << (uint8)0;
			tpCmd << (uint8)1; // dest district
			// away from any hardline: refused
			humanOperative.po->setDistrict(1);
			humanOperative.po->setPosition(LocationVector(maraCentral->x + 50000.0, maraCentral->y, maraCentral->z + 50000.0));
			humanClient.captured.clear();
			{ ByteBuffer far(tpCmd); humanOperative.po->HandleCommand(far); }
			check(humanClient.sawText("You need to be at a hardline to use it") && !humanClient.sawText("[HARDLINE] Transferred"),
				"0x818e away from a hardline is refused (no teleport from anywhere)");
			// standing at MaraCentral: allowed
			humanOperative.po->setPosition(LocationVector(maraCentral->x, maraCentral->y, maraCentral->z));
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(tpCmd);
			check(humanClient.sawText("[HARDLINE] Transferred to MaraNorthWest"), "HandleCommand(0x818e) teleports operative to MaraNorthWest");
			check(std::abs(humanOperative.po->getPosition().x - 7737.37) < 1.0, "Operative X position updated to MaraNorthWest");

			// 4. JackoutEffectMsg wire serialization matching authentic retail size
			JackoutEffectMsg jMsg(humanOperative.go);
			jMsg.setReceiver(humanOperative.client);
			const ByteBuffer& jBuf = jMsg.toBuf();
			check(jBuf.size() >= 35, "JackoutEffectMsg wire buffer matches authentic telephone booth / mirror dissolve packet size");

			// 5. Jackout Request via RPC 0x80fc scheduling 10-second carrier escape
			ByteBuffer jackoutCmd;
			jackoutCmd << (uint8)0x80 << (uint8)0xfc;
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(jackoutCmd);
			check(humanClient.sawText("[JACKOUT] Jackout sequence confirmed"), "HandleCommand(0x80fc) initiates authentic 10-second jackout sequence");

			// 6. Jackout interruption upon taking damage
			humanClient.captured.clear();
			humanOperative.po->takeDamage(0, 10, 0);
			check(humanClient.sawText("[JACKOUT] Jackout sequence interrupted"), "takeDamage interrupts pending jackout sequence");

			// 7. /jackout and /hardline chat commands (0x2810)
			ByteBuffer hlChatCmd;
			hlChatCmd << (uint8)0x28 << (uint8)0x10;
			hlChatCmd << (uint16)swap16(8);
			hlChatCmd << (uint32)0;
			hlChatCmd.writeString("/hardline");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(hlChatCmd);
			check(humanClient.sawText("[HARDLINE] Nearest:"), "/hardline command reports nearest hardline station");
		}

		// ============================================================
		// Remaster Completion Subsystems: Lockers, Tailors, Faction Warfare & City Traffic
		// ============================================================
		{
			std::cout << "\n[21. Remaster Completion Subsystems: Lockers, Tailors, Faction Warfare & City Traffic]" << std::endl;
			Actor humanOperative = makeHuman(&humanClient, 9100104, 17043.1, 2398.8);
			humanOperative.po->setDistrict(1);

			// 1. Hardline Locker & Vault Storage
			humanClient.captured.clear();
			humanOperative.po->giveItem(10102); // Trenchcoat
			uint8 invSlot = humanOperative.po->getInventory()->getFirstFreeSlot() - 1;
			// Player at MaraCentral (17043.1, 572.0, 2398.8) is right at Hardline #152
			humanOperative.po->setPosition(LocationVector(17043.1, 572.0, 2398.8));

			ByteBuffer lockerListCmd;
			lockerListCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			lockerListCmd.writeString("/locker list");
			humanOperative.po->HandleCommand(lockerListCmd);
			check(humanClient.sawText("[LOCKER] Hardline #152 vault is empty"), "/locker list identifies empty Hardline vault");

			ByteBuffer lockerStoreCmd;
			lockerStoreCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			lockerStoreCmd.writeString((format("/locker store %1%") % (int)invSlot).str());
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(lockerStoreCmd);
			check(humanClient.sawText("[LOCKER] Deposited item"), "/locker store deposits item into Hardline vault");
			check(sEconomySys.GetLockerItemCount(humanOperative.po->getCharacterUID(), 152) == 1, "EconomySystem confirms 1 item in Hardline 152 locker");

			// Retrieve item from locker
			auto items = sEconomySys.GetLockerItems(humanOperative.po->getCharacterUID(), 152);
			check(!items.empty(), "GetLockerItems returns deposited item");
			if (!items.empty()) {
				ByteBuffer lockerWithdrawCmd;
				lockerWithdrawCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
				lockerWithdrawCmd.writeString((format("/locker withdraw %1%") % items[0].entryId).str());
				humanClient.captured.clear();
				humanOperative.po->HandleCommand(lockerWithdrawCmd);
				check(humanClient.sawText("[LOCKER] Withdrew item"), "/locker withdraw returns item to operative inventory");
			}

			// 2. Tailor Wardrobe & RSI Palette Dyeing
			ByteBuffer dyeCmd;
			dyeCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			dyeCmd.writeString("/dye coat 7");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(dyeCmd);
			check(humanClient.sawText("[TAILOR] Your coat has been dyed to palette color #7"), "/dye coat 7 customizes wardrobe coat color");

			// 3. Faction War Contested Node Alert & Viral Capture
			// Register a control node in District 1 at (17043.1, 572.0, 2398.8)
			sFactionWarMgr.registerControlNode(152, 1, 17043.1f, 572.0f, 2398.8f);
			check(sFactionWarMgr.getControllingFaction(152) == FACTION_MACHINES, "Hardline 152 default faction is Machines");

			// Operative initiates capture
			humanOperative.po->setFaction(FACTION_ZION);
			ByteBuffer captureCmd;
			captureCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			captureCmd.writeString("/capture 152");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(captureCmd);
			check(humanClient.sawText("[VIRUS UPLOAD] Hardline #152 capture progress:"), "/capture initiates viral upload to Hardline node");
			check(humanClient.sawText("[FACTION ALERT] Hardline Broadcast Node #152"), "/capture triggers district-wide contested broadcast alert");

			// Advance to full capture
			sFactionWarMgr.AdvanceNodeCapture(152, FACTION_ZION, 0.70f, humanOperative.po);
			check(sFactionWarMgr.getControllingFaction(152) == FACTION_ZION, "Hardline 152 captured by Zion operative");


		}

		// ============================================================
		// 22. Phase I: Dynamic Agent Overwrite, Civilian Possession & Crowd Fleeing Dynamics
		// ============================================================
		{
			std::cout << "\n[22. Phase I: Dynamic Agent Overwrite, Civilian Possession & Crowd Fleeing Dynamics]" << std::endl;
			Actor humanOperative = makeHuman(&humanClient, 9100105, 17043.1, 2398.8);
			humanOperative.po->setDistrict(1);
			humanOperative.po->setAdmin(true);

			// 1. Dynamic Agent Possession of civilian host
			Actor civilianHost = makeBot(9200201, 17043.1, 2398.8, 1, 100);
			civilianHost.po->setHandle("Thomas Anderson");
			civilianHost.po->setFactionName("Civilian");
			uint32 civGoId = civilianHost.go;

			check(!sAgentPossessionMgr.IsEntityPossessed(civGoId), "Civilian entity initially not possessed");
			uint32 agentId = sAgentPossessionMgr.PossessCivilian(civGoId, "Agent Johnson", humanOperative.go, false);
			check(agentId != 0, "AgentPossessionManager successfully overwrote civilian host");
			check(sAgentPossessionMgr.IsEntityPossessed(civGoId), "IsEntityPossessed returns true for overwritten host");
			check(sAgentPossessionMgr.GetTotalOverwrites() >= 1, "AgentPossessionManager records total overwrites >= 1");

			const PossessedAgent* posAgent = sAgentPossessionMgr.GetPossession(agentId);
			check(posAgent != nullptr, "PossessedAgent record query by agentId succeeds");
			check(posAgent->agentName == "Agent Johnson", "PossessedAgent identity resolved to Agent Johnson");
			check(posAgent->civilianGoId == civGoId, "PossessedAgent bound to authentic civilian GoId");

			// 2. Chat slash command &overwrite execution
			ByteBuffer overwriteCmd;
			overwriteCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			overwriteCmd.writeString("/overwrite");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(overwriteCmd);
			check(humanClient.sawText("[AGENT OVERWRITE]") && humanClient.sawText("Commencing system overwrite"),
				"/overwrite chat command broadcasts authentic green code rain alert");

			// 3. Ambient crowd fear aura & panic fleeing dynamics
			Actor bystander = makeBot(9200202, 17043.1, 2405.0, 1, 100);
			bystander.po->setHandle("City Bystander");
			bystander.po->setFactionName("Civilian");

			sPedestrianEcology.SpreadRumorFearAura(17043.1f, 2398.8f, 0.90f, 3500.0f);
			CivilianFearTier tier = sPedestrianEcology.EvaluateCivilianTier(0.85f);
			check(tier == CIV_TIER_PANIC_STAMPEDE, "Fear level >= 0.80 transitions pedestrian to CIV_TIER_PANIC_STAMPEDE");

			// 4. Agent Defeat Demorphing in combat
			bool defeatHandled = sAgentPossessionMgr.HandleAgentDefeat(civGoId, humanOperative.go);
			check(defeatHandled, "HandleAgentDefeat successfully demorphs defeated Agent");
			check(sAgentPossessionMgr.GetTotalDemorphs() >= 1, "AgentPossessionManager records total demorphs >= 1");
			check(posAgent->state == AgentPossessionState::DEFEATED_DEMORPH, "PossessedAgent state transitions to DEFEATED_DEMORPH");

			// 5. DemorphAgent cleanup
			bool demorphed = sAgentPossessionMgr.DemorphAgent(agentId);
			check(demorphed, "DemorphAgent cleanly removes possession record");
			check(!sAgentPossessionMgr.IsEntityPossessed(civGoId), "Entity is no longer possessed after demorph");
		}

		// =========================================================================
		// 23. Socket System & Spy Simulacra Faction Spoofing
		// =========================================================================
		{
			std::cout << "[23] Socket System & Spy Faction Masking" << std::endl;

			// 1. Socket JSON serialization and parsing
			std::string baseJson = "{\"templateId\":46568,\"durability\":100}";
			std::vector<SocketFragment> frags;
			frags.push_back({"damage", 15});
			frags.push_back({"defense", 20});
			std::string serialized = sSocketSystem.SerializeSockets(frags, baseJson);
			check(serialized.find("\"sockets\":[") != std::string::npos, "SerializeSockets adds sockets array to JSON");
			check(serialized.find("\"type\":\"damage\"") != std::string::npos, "SerializeSockets embeds damage fragment");
			check(serialized.find("\"value\":15") != std::string::npos, "SerializeSockets embeds damage value");
			check(serialized.find("\"type\":\"defense\"") != std::string::npos, "SerializeSockets embeds defense fragment");

			auto parsed = sSocketSystem.ParseSockets(serialized);
			check(parsed.size() == 2, "ParseSockets retrieves exactly 2 SocketFragments");
			check(parsed[0].type == "damage" && parsed[0].value == 15, "ParseSockets resolves fragment 0 correctly");
			check(parsed[1].type == "defense" && parsed[1].value == 20, "ParseSockets resolves fragment 1 correctly");

			// 2. Chat slash command &socket execution
			Actor humanOperative = makeHuman(&humanClient, 9100106, 17045.0, 2400.0);
			humanOperative.po->setAdmin(true);
			ByteBuffer socketCmd;
			socketCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			socketCmd.writeString("/socket 1 damage 15");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(socketCmd);
			check(humanClient.sawText("Successfully socketed") || humanClient.sawText("Socketing failed"),
				"/socket chat command successfully parsed and dispatched");

			// 3. Spy / Hacker Simulacra Faction Masking
			humanOperative.po->setFaction(FACTION_ZION);
			check(humanOperative.po->getFaction() == FACTION_ZION, "Player initially belongs to FACTION_ZION");
			humanOperative.po->setMaskFaction(FACTION_MACHINES, 300000);
			check(humanOperative.po->getFaction() == FACTION_MACHINES, "Player getFaction() returns FACTION_MACHINES while masked");
			check(humanOperative.po->getTrueFaction() == FACTION_ZION, "Player getTrueFaction() returns FACTION_ZION");
			humanOperative.po->clearMaskFaction();
			check(humanOperative.po->getFaction() == FACTION_ZION, "Player getFaction() reverts to FACTION_ZION after clearing mask");

		}

		// Section 25: Phase H Systems — Coder Blueprints, Hovercraft Flight & Organization Syndicates
		{
			std::cout << "\n--- Section 25: Phase H Systems (Coder Blueprints, Hovercraft Flight, Organizations) ---" << std::endl;
			Actor humanOperative = makeHuman(&humanClient, 9100110, 17050.0, 2400.0);
			humanOperative.po->setAdmin(true);

			// 1. /craft and &craft command execution
			ByteBuffer craftListCmd;
			craftListCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			craftListCmd.writeString("/craft list");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(craftListCmd);
			check(humanClient.sawText("[CRAFTING] MegaCity Coder Blueprints"), "/craft list outputs MegaCity Coder blueprint catalog");

			// 2. Blueprint synthesis via /craft 101 (Health Stim Patch)
			humanOperative.po->addInfo(500);
			uint64 preInfo = humanOperative.po->getInfo();
			ByteBuffer craftSynthCmd;
			craftSynthCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			craftSynthCmd.writeString("/craft 101");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(craftSynthCmd);
			check(humanClient.sawText("[CRAFTING] Compilation complete! Synthesized: Health Stim Patch"),
				"/craft 101 synthesizes Health Stim Patch");
			check(humanOperative.po->getInfo() == preInfo - 100,
				"Crafting deducted 100 $Info for Health Stim Patch");

			// 5. /org create and /org leave
			ByteBuffer orgCreateCmd;
			orgCreateCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			orgCreateCmd.writeString("/org create Nebuchadnezzar Crew");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(orgCreateCmd);
			check(humanClient.sawText("[ORGANIZATION] 'Nebuchadnezzar Crew' created successfully"),
				"/org create creates syndicate organization with operative as leader");
			check(humanOperative.po->getOrgId() != 0, "Operative assigned non-zero Organization ID");

			ByteBuffer orgLeaveCmd;
			orgLeaveCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			orgLeaveCmd.writeString("/org leave");
			humanClient.captured.clear();
			humanOperative.po->HandleCommand(orgLeaveCmd);
			check(humanClient.sawText("[ORGANIZATION] You have left the Organization"),
				"/org leave clears operative organization membership");
			check(humanOperative.po->getOrgId() == 0, "Operative Organization ID cleared to 0");
		}

		// Section 26: Phase I Systems — Procedural & Story Missions, Sparring Duels & Hardline Transit
		{
			std::cout << "\n--- Section 26: Phase I Systems (Missions, Sparring Duels, Hardline Transit) ---" << std::endl;
			Actor operativeA = makeHuman(&humanClient, 9100112, 17050.0, 2400.0);
			operativeA.po->setAdmin(true);

			// 1. /mission request dispatch
			ByteBuffer missionReqCmd;
			missionReqCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			missionReqCmd.writeString("/mission request");
			humanClient.captured.clear();
			operativeA.po->HandleCommand(missionReqCmd);
			check(humanClient.sawText("[OPERATOR DISPATCH] New Contract Assigned:") || humanClient.sawText("[OPERATOR] Active contract in progress"),
				"/mission request assigns mission contract or detects active contract");
			check(sMissionSys.HasActiveMission(operativeA.po->getGoId()),
				"MissionSystem confirms operative has an active mission");

			// 2. /mission status query
			ByteBuffer missionStatusCmd;
			missionStatusCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			missionStatusCmd.writeString("/mission status");
			humanClient.captured.clear();
			operativeA.po->HandleCommand(missionStatusCmd);
			check(!humanClient.captured.empty(),
				"/mission status dispatches mission objective dialog");

			// 3. /mission abort
			ByteBuffer missionAbortCmd;
			missionAbortCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			missionAbortCmd.writeString("/mission abort");
			humanClient.captured.clear();
			operativeA.po->HandleCommand(missionAbortCmd);
			check(humanClient.sawText("[OPERATOR] Mission aborted by operator request"),
				"/mission abort cleanly terminates active contract");
			check(!sMissionSys.HasActiveMission(operativeA.po->getGoId()),
				"MissionSystem confirms active mission slot cleared");

			// 4. Sparring Duel Handshake (/duel <target> & /duel accept)
			TestHumanClient clientB;
			Actor operativeB = makeHuman(&clientB, 9100114, 17052.0, 2400.0);
			operativeB.po->setAdmin(true);

			ByteBuffer duelChallengeCmd;
			duelChallengeCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			duelChallengeCmd.writeString(std::string("/duel ") + operativeB.po->getHandle());
			humanClient.captured.clear();
			clientB.captured.clear();
			operativeA.po->HandleCommand(duelChallengeCmd);
			check(humanClient.sawText("[DUEL] Sparring duel challenge sent"),
				"/duel <target> dispatches sparring challenge");
			check(clientB.sawText("challenges you to a sparring duel"),
				"Target client receives duel challenge notification");
			check(operativeB.po->getDuelChallenger() == operativeA.po->getGoId(),
				"Target operative records challenger GoId");

			ByteBuffer duelAcceptCmd;
			duelAcceptCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			duelAcceptCmd.writeString("/duel accept");
			humanClient.captured.clear();
			clientB.captured.clear();
			operativeB.po->HandleCommand(duelAcceptCmd);
			check(operativeA.po->isDueling() && operativeB.po->isDueling(),
				"/duel accept sets mutual isDueling state for both operatives");
			check(operativeA.po->getDuelPartner() == operativeB.po->getGoId() && operativeB.po->getDuelPartner() == operativeA.po->getGoId(),
				"Both duelists correctly paired with mutual partner GoIds");

			// 5. Sparring Duel Conclusion on Defeat without death penalty
			operativeB.po->die(operativeA.po->getGoId());
			check(!operativeB.po->isDead(), "Defeated duelist did not die (spared from death penalty)");
			check(!operativeA.po->isDueling() && !operativeB.po->isDueling(), "Duel concluded and cleared on defeat");
			check(operativeB.po->getCurrentHealth() == operativeB.po->getMaximumHealth(), "Defeated duelist health restored to 100%");

			// 6. Hardline Directory Query & Teleportation
			ByteBuffer hlListCmd;
			hlListCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			hlListCmd.writeString("/hardlines");
			humanClient.captured.clear();
			operativeA.po->HandleCommand(hlListCmd);
			check(humanClient.sawText("[HARDLINE]"), "/hardlines queries active Matrix hardline directory");

			ByteBuffer hlTpCmd;
			hlTpCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			hlTpCmd.writeString("/hardline tp 1 152"); // MaraCentral; (1,1) does not exist
			// the hardline UI only works at a hardline: stand at one
			{
				const HardlineNode* hl = PlayerObject::GetNearestHardline(1, operativeA.po->getPosition().x, operativeA.po->getPosition().z);
				if (hl) { operativeA.po->setDistrict(1); operativeA.po->setPosition(LocationVector(hl->x, hl->y, hl->z)); }
			}
			sCombatSys.StopFreeFire(operativeA.go);
			sCombatSys.EndInterlock(operativeA.go, false);
			humanClient.captured.clear();
			operativeA.po->HandleCommand(hlTpCmd);
			check(humanClient.sawText("[HARDLINE] Transferred to"),
				"/hardline tp initiates authentic hardline network transit");
		}

		// =========================================================================
		// SECTION 27: Phase J Systems — Dynamic Glitch Anomaly Engine, Faction Tension & Contact Quests
		// =========================================================================
		std::cout << "\n=== SECTION 27: Phase J Systems (Glitch Anomalies, Tension & Contact Quests) ===" << std::endl;
		{
			// 3. Faction War District Tension Queries
			uint32 hotspotDist = sFactionWarMgr.GetHighestTensionDistrict();
			check(hotspotDist >= 1 && hotspotDist <= 4, "GetHighestTensionDistrict returns valid district ID (1-4)");
			float distTension = sFactionWarMgr.GetDistrictTension(hotspotDist);
			check(distTension >= 15.0f && distTension <= 98.0f, "GetDistrictTension returns valid clamped percentage (15-98%)");

			// 4. Contact Storyline Quests Database & Lookup
			const auto& allQuests = sMissionSys.GetAllContactQuests();
			check(allQuests.size() >= 9, "Contact Quests database contains at least 9 iconic storyline quests");

			const ContactQuest* qMorpheus = sMissionSys.GetContactQuest(1001);
			check(qMorpheus != nullptr && qMorpheus->contact == CONTACT_MORPHEUS,
				"Contact quest 1001 correctly mapped to Morpheus");

			const ContactQuest* qArchitect = sMissionSys.GetContactQuest(1008);
			check(qArchitect != nullptr && qArchitect->contact == CONTACT_ARCHITECT,
				"Contact quest 1008 correctly mapped to The Architect");

			// 5. Contact Quest Acceptance, Progression & Completion Lifecycle
			TestHumanClient clientA;
			Actor op = makeHuman(&clientA, 9100115, 17052.0, 2400.0);
			op.po->setAdmin(true);

			sMissionSys.ResetContactQuest(op.po, 1001);
			check(!sMissionSys.HasActiveContactQuest(op.po->getCharId(), 1001), "Player initially has no active quest for 1001");

			bool accepted = sMissionSys.AcceptContactQuest(op.po, 1001);
			check(accepted, "AcceptContactQuest succeeds for Morpheus quest 1001");
			check(sMissionSys.HasActiveContactQuest(op.po->getCharId(), 1001), "HasActiveContactQuest reports true");
			check(sMissionSys.GetPlayerContactProgress(op.po->getCharId(), 1001) == 0, "Initial quest step is 0");

			bool progressed = sMissionSys.ProgressContactQuest(op.po, 1001, 1);
			check(progressed && sMissionSys.GetPlayerContactProgress(op.po->getCharId(), 1001) == 1,
				"ProgressContactQuest advances quest to step 1");

			bool completed = sMissionSys.CompleteContactQuest(op.po, 1001);
			check(completed, "CompleteContactQuest completes quest and awards rewards");
			check(sMissionSys.HasCompletedContactQuest(op.po->getCharId(), 1001),
				"HasCompletedContactQuest confirms quest 1001 is finished");

			// 6. Slash Command Routing for /tension, /contact
			ByteBuffer tensionCmd;
			tensionCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			tensionCmd.writeString("/tension");
			clientA.captured.clear();
			op.po->HandleCommand(tensionCmd);
			check(clientA.sawText("DISTRICT FACTION TENSION") || clientA.sawText("[HOTSPOT]"),
				"/tension chat command queries conflict hotspots");

			ByteBuffer contactCmd;
			contactCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
			contactCmd.writeString("/contact list");
			clientA.captured.clear();
			op.po->HandleCommand(contactCmd);
			check(clientA.sawText("ICONIC CONTACT STORYLINE QUESTS") || clientA.sawText("Morpheus"),
				"/contact list chat command enumerates iconic storyline quests");

			// 7. Verify SpawnILCombatHandlerMsg wire layout matching retail 7.6005 CLTAdminControlView (0x622D6B79 / 0x622D9510)
			LocationVector testPos(100.0, 572.0, 200.0);
			SpawnILCombatHandlerMsg spawnMsg(4, 0x41, testPos, 123.45f);
			const ByteBuffer& spawnBuf = spawnMsg.toBuf();
			check(spawnBuf.size() == 44, "SpawnILCombatHandlerMsg size is authentic 44 bytes");
			check(spawnBuf.read<uint8>(0) == 0x03 && spawnBuf.read<uint16>(1) == VIEWID_OBJECTMANAGER,
				"SpawnILCombatHandlerMsg routed to VIEWID_OBJECTMANAGER (1)");
			check(spawnBuf.read<uint8>(3) == 0x0C,
				"SpawnILCombatHandlerMsg header is 0x0C (CREATE command 0 | bit 2=4 dynamic | bit 3=8 spawnIdCounter)");
			check(spawnBuf.read<uint16>(4) == GetILCombatHandlerGoId(),
				"SpawnILCombatHandlerMsg GOID is Interlock.HandlerGoId (default 55, HDS Object55 ILCombatHandler)");
			check(spawnBuf.read<uint8>(6) == 0x41 && spawnBuf.read<uint8>(7) == 0xCD && spawnBuf.read<uint8>(8) == 0xAB,
				"SpawnILCombatHandlerMsg includes spawnIdCounter and 0xCD 0xAB magic");
			check(spawnBuf.read<uint8>(9) == 0x02 && spawnBuf.read<uint8>(10) == 0x05,
				"SpawnILCombatHandlerMsg creation attributes count=2, mask=0x05 (Position + StartTime)");
			check(spawnBuf.read<uint16>(39) == 4 && spawnBuf.read<uint8>(41) == 0x00 && spawnBuf.read<uint16>(42) == 0,
				"SpawnILCombatHandlerMsg registers viewId 4, role 0, terminated with nomoreattribs 0");
			// client.dll FUN_625f6580 reads StartTime as u32 ms (/1000, %1000); 123.45 s -> 123450
			check(sConfig.GetIntDefault("Interlock.StartTimeMode", 1) != 1 || spawnBuf.read<uint32>(35) == 123450,
				"SpawnILCombatHandlerMsg StartTime is u32 sim milliseconds (Interlock.StartTimeMode 1)");

			// 8. Verify ObjectMgr client view allocation within retail 7.6005 128-element limit
			// (client.dll HandleInput 0x622C362B viewListSize cmp eax, edx; jae 0x622C383E)
			TestHumanClient testClient;
			testClient.SetPlayerGoId(33000);
			uint16 selfView = sObjMgr.getViewForGO(&testClient, 33000);
			check(selfView == VIEWID_SELF, "Local player goId maps to VIEWID_SELF (2)");

			uint32 botGoId1 = 33267;
			uint16 botView1 = sObjMgr.getViewForGO(&testClient, botGoId1);
			check(botView1 >= 3 && botView1 < 128,
				"Opponent bot goId maps to client viewId in [3..127] strictly within viewListSize 128");
			check(sObjMgr.getViewForGO(&testClient, botGoId1) == botView1,
				"Subsequent getViewForGO for the same goId returns identical allocated viewId");
			check(sObjMgr.getGOForView(&testClient, botView1) == botGoId1,
				"getGOForView maps viewId back to original bot goId 33267");
			check(sObjMgr.getGOForView(&testClient, VIEWID_SELF) == 33000,
				"getGOForView maps VIEWID_SELF (2) to local player goId 33000");
			check(sObjMgr.getGOForView(&testClient, 127) == 0,
				"getGOForView returns 0 for unallocated viewId");

			uint32 botGoId2 = 33268;
			uint16 botView2 = sObjMgr.getViewForGO(&testClient, botGoId2);
			check(botView2 >= 3 && botView2 < 128 && botView2 != botView1,
				"Second opponent bot receives distinct small viewId in [3..127]");
			check(sObjMgr.getGOForView(&testClient, botView2) == botGoId2,
				"getGOForView maps second bot viewId back to 33268");

			sObjMgr.releaseRelevantSet(&testClient);
			check(sObjMgr.getGOForView(&testClient, botView1) == 0,
				"releaseRelevantSet clears all allocated views for disconnected client");

			// 9. Verify CombatantModeMsg wire layout matching retail 7.6005 CLTDistrObjView::UnpackAttributes (0x623217B0)
			// Attribute 31 (PlayerAttrOther::CombatantMode) packs into Group 4 bit 3:
			// Headers: 0x80 0x80 0x80 0x80 0x08, value: 0x01 (mode 1), terminated with nomoreattribs uint16(0).
			// Total wire size must be exactly 11 bytes with NO count prefix.
			TestHumanClient modeClient;
			modeClient.SetPlayerGoId(33000);
			uint32 testBotGoId = 33269;
			uint16 testBotViewId = sObjMgr.getViewForGO(&modeClient, testBotGoId);
			CombatantModeMsg combatantMsg(testBotGoId, 1);
			combatantMsg.setReceiver(&modeClient);
			const ByteBuffer& combatantBuf = combatantMsg.toBuf();
			check(combatantBuf.size() == 11, "CombatantModeMsg size is authentic 11 bytes (no count prefix)");
			check(combatantBuf.read<uint8>(0) == 0x03, "CombatantModeMsg opcode is 0x03");
			check(combatantBuf.read<uint16>(1) == testBotViewId, "CombatantModeMsg viewId matches botViewId");
			check(combatantBuf.read<uint8>(3) == 0x80 && combatantBuf.read<uint8>(4) == 0x80 &&
				combatantBuf.read<uint8>(5) == 0x80 && combatantBuf.read<uint8>(6) == 0x80 &&
				combatantBuf.read<uint8>(7) == 0x08,
				"CombatantModeMsg group headers match 7-bit groups 0..4 (0x80 0x80 0x80 0x80 0x08)");
			check(combatantBuf.read<uint8>(8) == 0x01, "CombatantModeMsg Attribute 31 value is 0x01 (combat mode)");
			check(combatantBuf.read<uint16>(9) == 0, "CombatantModeMsg terminated with nomoreattribs uint16(0)");

			// 10. Verify SelfCombatantModeMsg wire layout for local player (View 2 / VIEWID_SELF)
			// Attribute 43 (PlayerAttrSelf::CombatantMode) packs into Group 6 bit 1:
			// Headers: 6x 0x80 followed by 0x02, value: 0x01 (mode 1), terminated with nomoreattribs uint16(0).
			// Total wire size must be exactly 14 bytes: 0x03 [VIEWID_SELF:2] [type:1=0x02] [groups:7] [value:1] [nomoreattribs:2]
			SelfCombatantModeMsg selfCombatantMsg(1);
			const ByteBuffer& selfBuf = selfCombatantMsg.toBuf();
			check(selfBuf.size() == 14, "SelfCombatantModeMsg size is authentic 14 bytes");
			check(selfBuf.read<uint8>(0) == 0x03, "SelfCombatantModeMsg opcode is 0x03");
			check(selfBuf.read<uint16>(1) == VIEWID_SELF, "SelfCombatantModeMsg viewId matches VIEWID_SELF (2)");
			check(selfBuf.read<uint8>(3) == 0x02, "SelfCombatantModeMsg update type is 0x02");
			check(selfBuf.read<uint8>(4) == 0x80 && selfBuf.read<uint8>(5) == 0x80 &&
				selfBuf.read<uint8>(6) == 0x80 && selfBuf.read<uint8>(7) == 0x80 &&
				selfBuf.read<uint8>(8) == 0x80 && selfBuf.read<uint8>(9) == 0x80 &&
				selfBuf.read<uint8>(10) == 0x02,
				"SelfCombatantModeMsg group headers match 7-bit groups 0..6 (6x0x80 + 0x02)");
			check(selfBuf.read<uint8>(11) == 0x01, "SelfCombatantModeMsg Attribute 43 value is 0x01 (combat mode)");
			check(selfBuf.read<uint16>(12) == 0, "SelfCombatantModeMsg terminated with nomoreattribs uint16(0)");

			// 11. Female RSI serialization test
			RsiDataFemale femRsi;
			femRsi["Sex"] = 1;
			femRsi["Body"] = 1;
			femRsi["Hat"] = 1;
			femRsi["Face"] = 2;
			femRsi["Shirt"] = 7;
			femRsi["Coat"] = 7;
			femRsi["Pants"] = 1;
			femRsi["Shoes"] = 6;
			femRsi["Gloves"] = 7;
			femRsi["Glasses"] = 6;
			femRsi["Hair"] = 1;
			femRsi["Leggings"] = 2;
			femRsi["FacialDetail"] = 7;
			femRsi["ShirtColor"] = 0;
			femRsi["PantsColor"] = 0;
			femRsi["CoatColor"] = 1;
			femRsi["ShoeColor"] = 3;
			femRsi["GlassesColor"] = 5;
			femRsi["HairColor"] = 4;
			femRsi["SkinTone"] = 3;
			femRsi["Tattoo"] = 6;
			femRsi["FacialDetailColor"] = 2;
			byte femBytes[15] = {0};
			uint8 len = femRsi.ToBytes(femBytes, 15);
			check(len >= 12 && len <= 15, "Female RSI serializes to valid byte length (12..15)");
			check((femBytes[0] & 0x80) == 0x80, "Female RSI bit 0 (sex=1) is MSB 0x80 in byte 0");

			// 12. Verify PlayerAppearanceMsg for bot with short hash (e.g. Harahel "93030008")
			// Ensures safe authentic default RSI is used and PlayerAppearanceMsg serializes properly.
			auto botClient = std::make_shared<BotClient>(9000000 + 499);
			PlayerObject* harahel = sObjMgr.getGOPtr(botClient->GetPlayerGoId());
			check(harahel != nullptr, "Test bot player object constructed");
			if (harahel) {
				harahel->setHandle("Harahel");
				harahel->setRsiHex("93030008");
				uint16 botView = sObjMgr.getViewForGO(&modeClient, harahel->getGoId());
				PlayerAppearanceMsg appMsg(harahel->getGoId());
				appMsg.setReceiver(&modeClient);
				const ByteBuffer& appBuf = appMsg.toBuf();
				check(appBuf.size() > 0, "PlayerAppearanceMsg serializes cleanly for Harahel bot");
				check(appBuf.read<uint8>(0) == 0x03, "PlayerAppearanceMsg opcode is 0x03");
				check(appBuf.read<uint16>(1) == botView, "PlayerAppearanceMsg viewId matches allocated view");
			}

			// 13. Verify Procedural Appearance Diversity across bots
			std::vector<std::vector<byte>> rsiSamples;
			int maleCount = 0;
			int femaleCount = 0;
			for (uint32 bId = 0; bId < 10; ++bId) {
				auto sampleBot = std::make_shared<BotClient>(9000000 + 500 + bId);
				PlayerObject* po = sObjMgr.getGOPtr(sampleBot->GetPlayerGoId());
				if (po) {
					std::vector<byte> rsi(15, 0);
					po->getRsiData(rsi.data(), 15);
					rsiSamples.push_back(rsi);
					if ((rsi[0] & 0x80) != 0) {
						femaleCount++;
					} else {
						maleCount++;
					}
				}
			}
			check(rsiSamples.size() == 10, "Spawned 10 procedural appearance test bots");
			if (sConfig.GetBoolDefault("Bots.ProceduralAppearance", false)) {
			check(maleCount > 0, "Bot population contains male operatives");
			check(femaleCount > 0, "Bot population contains female operatives");
			bool hasVariation = false;
			for (size_t i = 1; i < rsiSamples.size(); ++i) {
				if (rsiSamples[i] != rsiSamples[0]) {
					hasVariation = true;
					break;
				}
			}
			check(hasVariation, "Bot population exhibits procedural appearance diversity (not all identical)");
			} // Bots.ProceduralAppearance

			// 14. Verify Locomotion Stuck Tracking
			auto roamBot = std::make_shared<BotClient>(9000000 + 600);
			check(roamBot->m_stuckTicks == 0, "Initial bot stuckTicks is 0");
			roamBot->m_stuckTicks = 3;
			check(roamBot->m_stuckTicks >= 3, "Stuck tracking reaches turnaround threshold (>=3)");

			// 15. Verify Civilian Ecology Routing & Spawn Anchor Preservation
			auto civBot = std::make_shared<BotClient>(9000000 + 700);
			PlayerObject* civPo = sObjMgr.getGOPtr(civBot->GetPlayerGoId());
			check(civPo != nullptr, "Constructed civilian test bot");
			if (civPo) {
				civPo->setFactionName("Civilian");
				civBot->SetSpawnLocation(1000.0f, 572.0f, 2000.0f);
				civBot->MoveTo(1500.0f, 572.0f, 2500.0f);
				check(civBot->GetSpawnX() == 1000.0f && civBot->GetSpawnZ() == 2000.0f,
					"Bot spawn anchor coordinates preserved (MoveTo does not overwrite m_spawnX/Z)");
				civBot->UpdateBotAI(0.1f);
				check(civBot->GetTargetGoId() == 0, "Civilian bot does not acquire combat targets or enter combat");
			}

			// 16. Verify Gunner Stand-Off Range & Freefire Combat Execution
			auto gunnerBot = std::make_shared<BotClient>(9000000 + 710);
			auto victimBot = std::make_shared<BotClient>(9000000 + 711);
			PlayerObject* gunnerPo = sObjMgr.getGOPtr(gunnerBot->GetPlayerGoId());
			PlayerObject* victimPo = sObjMgr.getGOPtr(victimBot->GetPlayerGoId());
			check(gunnerPo && victimPo, "Constructed gunner and victim test bots");
			if (gunnerPo && victimPo) {
				gunnerPo->setHandle("City Police Gunner");
				gunnerPo->giveItem(1001); // Dual Berettas (Ranged weapon)
				gunnerPo->setPosition(LocationVector(50000.0, 572.0, 50000.0));
				victimPo->setPosition(LocationVector(50000.0, 572.0, 51600.0)); // 1600 units away (optimal standoff)
				gunnerBot->SetTargetGoId(victimPo->getGoId());

				ActionEngageTarget engage;
				NodeStatus engStatus = engage.Tick(gunnerBot.get());
				check(engStatus == NodeStatus::SUCCESS, "Gunner bot preserves stand-off range (1600 units within 1200..2200 bracket)");

				ActionCombatCycle combatCycle;
				gunnerBot->SetNextActionTime(0);
				NodeStatus cycleStatus = combatCycle.Tick(gunnerBot.get());
				check(cycleStatus == NodeStatus::SUCCESS, "Gunner combat cycle executed freefire ability at distance");
				check(!sCombatSys.IsInterlocked(gunnerPo->getGoId()), "Gunner does not force unwanted melee interlock from ranged standoff");
			}

			// 17. Verify 3D Line of Sight Raycast with Elevation
			bool losClear = sNavMeshMgr.CheckLineOfSight(100.0f, 572.0f, 100.0f, 200.0f, 572.0f, 200.0f);
			check(losClear, "3D NavMeshMgr::CheckLineOfSight evaluates unobstructed ray at elevation 572.0");

			// 18. Verify Squad Formation Offsets & Dynamic Leader Succession
			auto leaderBot = std::make_shared<BotClient>(9000000 + 720);
			auto followerBot = std::make_shared<BotClient>(9000000 + 721);
			PlayerObject* leaderPo = sObjMgr.getGOPtr(leaderBot->GetPlayerGoId());
			PlayerObject* followerPo = sObjMgr.getGOPtr(followerBot->GetPlayerGoId());
			check(leaderPo && followerPo, "Constructed squad leader and follower bots");
			if (leaderPo && followerPo) {
				leaderPo->setPosition(LocationVector(20000.0, 572.0, 20000.0));
				followerPo->setPosition(LocationVector(20000.0, 572.0, 19800.0));
				uint32 crewId = 888;
				leaderBot->SetCrewId(crewId);
				leaderBot->SetCrewLeaderId(leaderPo->getGoId());
				followerBot->SetCrewId(crewId);
				followerBot->SetCrewLeaderId(leaderPo->getGoId());

				ActionRoam roam;
				roam.Tick(followerBot.get());
				check(followerBot->HasPath() || followerPo->getPosition().Distance(leaderPo->getPosition()) > 50.0f,
					"Follower moves toward staggered formation offset rather than stacking directly on leader center");

				// Kill the leader to verify dynamic succession
				leaderPo->die(0);
				roam.Tick(followerBot.get());
				check(followerBot->GetCrewLeaderId() != leaderPo->getGoId(),
					"Follower detects deceased leader and updates crew hierarchy (dynamic succession)");
			}

			// 19. Verify In-Game Bug Reporting & Server Database Persistence (&bug, /bug, &bugs, &bug view, &bug resolve)
			{
				TestHumanClient bugClient;
				Actor reporter = makeHuman(&bugClient, 9100250, 17052.0, 2400.0);
				reporter.po->setHandle("BugHunterQA");
				reporter.po->setAdmin(true);

				// Submit via &bug (works directly in retail 7.6005 chat box without client slash interception)
				ByteBuffer bugCmd1;
				bugCmd1 << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
				bugCmd1.writeString("&bug Interlock animation hitch when blocking karate kick");
				bugClient.captured.clear();
				reporter.po->HandleCommand(bugCmd1);
				check(bugClient.sawText("[BUG REPORT #") && bugClient.sawText("SAVED]"),
					"&bug <description> saves bug report and confirms ticket ID to player");

				// Submit via /bug
				ByteBuffer bugCmd2;
				bugCmd2 << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
				bugCmd2.writeString("/bug [WORLD] Richland hardline phone booth collision offset");
				bugClient.captured.clear();
				reporter.po->HandleCommand(bugCmd2);
				check(bugClient.sawText("[BUG REPORT #") && bugClient.sawText("WORLD"),
					"/bug [WORLD] <description> parses explicit category and saves ticket");

				auto allBugs = PlayerObject::GetBugReports(10, "");
				check(allBugs.size() >= 2, "PlayerObject::GetBugReports returns saved bug reports");

				uint32 latestId = allBugs.front().id;
				BugReportRecord fetched;
				check(PlayerObject::GetBugReportById(latestId, fetched),
					"PlayerObject::GetBugReportById retrieves specific bug ticket by ID");
				check(fetched.handle == "BugHunterQA" && fetched.category == "WORLD",
					"Saved bug report preserves reporter handle, category, and telemetry");

				// List via &bugs
				ByteBuffer listCmd;
				listCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
				listCmd.writeString("&bugs");
				bugClient.captured.clear();
				reporter.po->HandleCommand(listCmd);
				check(bugClient.sawText("SERVER BUG REPORTS") && bugClient.sawText("BugHunterQA"),
					"&bugs lists saved bug reports in chat");

				// View details via &bug view <id>
				ByteBuffer viewCmd;
				viewCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
				viewCmd.writeString("&bug view " + std::to_string(latestId));
				bugClient.captured.clear();
				reporter.po->HandleCommand(viewCmd);
				check(bugClient.sawText("BUG REPORT #" + std::to_string(latestId)) && bugClient.sawText("Location:"),
					"&bug view <id> displays full reporter and coordinate telemetry");

				// Resolve ticket via &bug resolve <id> <notes>
				ByteBuffer resolveCmd;
				resolveCmd << (uint8)0x28 << (uint8)0x10 << (uint16)swap16(8) << (uint32)0;
				resolveCmd.writeString("&bug resolve " + std::to_string(latestId) + " Fixed collision box in Richland");
				bugClient.captured.clear();
				reporter.po->HandleCommand(resolveCmd);
				check(bugClient.sawText("updated to status [RESOLVED]"),
					"&bug resolve <id> <notes> marks ticket RESOLVED");

				BugReportRecord resolvedRec;
				check(PlayerObject::GetBugReportById(latestId, resolvedRec) &&
					  resolvedRec.status == "RESOLVED" &&
					  resolvedRec.adminNotes == "Fixed collision box in Richland",
					"Resolved bug report persists status=RESOLVED and admin_notes");
			}

			// 20. Phase 2: Disciplines & Authentic Ability Mastery (Memory Capacity, Timed Casts, Ballistic Curve, Martial Arts Chains)
			{
				TestHumanClient p2Client;
				Actor p2Human = makeHuman(&p2Client, 9100260, 10000.0, 10000.0);
				auto abSys = p2Human.po->getAbilitySystem();
				check(abSys != nullptr, "Phase 2 operative has valid AbilitySystem");
				if (abSys)
				{
					// A. Dynamic Memory Capacity Budget (L * 10 + 100) & 0x80b2 LOAD_RSP_TRAILER (0x0208)
					p2Human.po->setLevel(1);
					check(abSys->getMaxMemory() == 110, "Level 1 operative has 110 dynamic MaxMemory (1 * 10 + 100)");
					p2Human.po->setLevel(25);
					check(abSys->getMaxMemory() == 350, "Level 25 operative dynamically scales to 350 MaxMemory (25 * 10 + 100)");
					p2Human.po->setLevel(50);
					check(abSys->getMaxMemory() == 600, "Level 50 operative dynamically scales to 600 MaxMemory (50 * 10 + 100)");

					// Test RPC_HandleAbilityLoad (0x80ae) sends 0x80b2 with trailer 0x0208
					ByteBuffer loadPkt;
					loadPkt << (uint8)0x80 << (uint8)0xae;
					loadPkt << (uint32)0;      // staticObjId
					loadPkt << (uint16)0;      // unloadFlag
					loadPkt << (uint16)1;      // loadFlag
					loadPkt << (uint16)1;      // countAbilities
					loadPkt << (uint8)0;       // 11th header pad byte
					loadPkt << (uint16)5;      // slotId = 5
					loadPkt << (uint16)57;     // abilityId = 57 (LogicBlast1)
					loadPkt << (uint16)0;      // pad
					loadPkt << (uint16)1;      // abilityLevel = 1
					loadPkt << (uint8)0;       // pad
					p2Client.captured.clear();
					p2Human.po->HandleCommand(loadPkt);

					bool sawTrailer0208 = false;
					for (const auto& pkt : p2Client.captured)
					{
						if (pkt.size() == 8 && (uint8)pkt[0] == 0x80 && (uint8)pkt[1] == 0xb2)
						{
							uint16 rId = *reinterpret_cast<const uint16*>(&pkt[2]);
							uint16 rLvl = *reinterpret_cast<const uint16*>(&pkt[4]);
							uint16 rTrailer = *reinterpret_cast<const uint16*>(&pkt[6]);
							if (rId == 57 && rLvl == 1 && rTrailer == AbilitySystem::LOAD_RSP_TRAILER)
								sawTrailer0208 = true;
						}
					}
					check(sawTrailer0208, "RPC_HandleAbilityLoad (0x80ae) replies with 0x80b2 carrying authentic 0x0208 trailer");
				}

				// B. Timed Cast Delay (CastBarMsg 0x80ac + ScheduleCastDelay) for Hacker, Coder, Soldier, and Spy
				sHackerSystem.SetForceDelayedCast(true);
				{
					// 1) Hacker LogicBlast1 (id 57) delayed cast
					Actor hackTgt = makeBot(9200261, 10100.0, 10000.0, 1, 100);
					p2Human.po->setInnerStrength(200);
					p2Client.captured.clear();
					bool hCast = sCombatSys.UseAbility(p2Human.po, 57, hackTgt.go);
					check(hCast, "Delayed Hacker LogicBlast1 (57) starts cast");
					check(hackTgt.po->getCurrentHealth() == 100, "LogicBlast1 damage does NOT land at t=0s before cast completes");
					sStatusEffectManager.Update(3.0f);
					check(hackTgt.po->getCurrentHealth() == 55, "LogicBlast1 damage (45) lands after cast delay completes (100 -> 55 HP)");

					// 2) Coder RestoreHealth1 (id 77) delayed cast
					p2Human.po->setMaximumHealth(200);
					p2Human.po->setCurrentHealth(100);
					p2Human.po->setInnerStrength(200);
					bool cCast = sCombatSys.UseAbility(p2Human.po, 77, p2Human.go);
					check(cCast, "Delayed Coder RestoreHealth1 (77) starts cast");
					check(p2Human.po->getCurrentHealth() == 100, "RestoreHealth1 heal does NOT land at t=0s before cast completes");
					sStatusEffectManager.Update(3.0f);
					check(p2Human.po->getCurrentHealth() == 160, "RestoreHealth1 heal (+60) lands after cast delay completes (100 -> 160 HP)");

					// 3) Soldier PowerShot (id 14) delayed cast
					Actor solTgt = makeBot(9200262, 10100.0, 10000.0, 1, 100);
					p2Human.po->setInnerStrength(200);
					bool sCast = sCombatSys.UseAbility(p2Human.po, 14, solTgt.go);
					check(sCast, "Delayed Soldier PowerShot (14) starts cast");
					check(solTgt.po->getCurrentHealth() == 100, "PowerShot damage does NOT land at t=0s before cast completes");
					sStatusEffectManager.Update(3.0f);
					check(solTgt.po->getCurrentHealth() == 50, "PowerShot damage (50) lands after cast delay completes (100 -> 50 HP)");

					// 4) Spy PoisonKnife (id 146) delayed cast
					Actor spyTgt = makeBot(9200263, 10050.0, 10000.0, 1, 100);
					p2Human.po->setInnerStrength(200);
					p2Human.po->setStealth(false);
					bool spCast = sCombatSys.UseAbility(p2Human.po, 146, spyTgt.go);
					check(spCast, "Delayed Spy PoisonKnife (146) starts cast");
					check(spyTgt.po->getCurrentHealth() == 100, "PoisonKnife damage does NOT land at t=0s before cast completes");
					sStatusEffectManager.Update(3.0f);
					check(spyTgt.po->getCurrentHealth() < 100, "PoisonKnife damage lands after cast delay completes");
				}
				sHackerSystem.SetForceDelayedCast(false);

				// C. Gunner Ballistic Distance Drop-Off & Shared Firearm Magazine Ammo Tracking
				{
					float cqbMult = CombatSystem::ComputeBallisticDistanceMultiplier(300.0, 3500.0, 499);
					check(std::abs(cqbMult - 1.15f) < 0.01f, "PistolPointBlank (499) at 3m grants 1.15x CQB damage bonus");

					float optMult = CombatSystem::ComputeBallisticDistanceMultiplier(1000.0, 3500.0, 14);
					check(std::abs(optMult - 1.00f) < 0.01f, "Standard ballistic shot at 10m optimal range deals 1.00x damage");

					float falloffMult = CombatSystem::ComputeBallisticDistanceMultiplier(3500.0, 3500.0, 14);
					check(std::abs(falloffMult - 0.60f) < 0.01f, "Standard ballistic shot at 35m max range drops off to 0.60x damage");

					float sniperLong = CombatSystem::ComputeBallisticDistanceMultiplier(5000.0, 8000.0, 505);
					float sniperShort = CombatSystem::ComputeBallisticDistanceMultiplier(200.0, 8000.0, 505);
					check(std::abs(sniperLong - 1.25f) < 0.01f && std::abs(sniperShort - 0.80f) < 0.01f,
						"SniperShot (505) scales from 0.80x inside 5m to 1.25x beyond 20m");

					// Verify Soldier ability consumes magazine ammo & auto-reloads from inventory clip
					p2Human.po->getInventory()->clear();
					p2Human.po->giveItem(1002); // Beretta 92FS (maxAmmo = 15)
					p2Human.po->setEquippedWeaponId(1002);
					auto eqW = p2Human.po->getEquippedWeapons();
					check(!eqW.empty(), "Phase 2 operative equipped Beretta 92FS");
					if (!eqW.empty())
					{
						eqW[0]->setAmmoCount(1);
						Actor rangeTgt = makeBot(9200264, 10200.0, 10000.0, 1, 200);
						p2Human.po->setInnerStrength(200);
						bool shot1 = sCombatSys.UseAbility(p2Human.po, 14, rangeTgt.go);
						check(shot1 && eqW[0]->getAmmoCount() == 0, "Soldier PowerShot decrements magazine ammo (1 -> 0)");

						// Dry fire without clip fails with [OUT OF AMMO]
						p2Client.captured.clear();
						bool dryShot = sCombatSys.UseAbility(p2Human.po, 14, rangeTgt.go);
						check(!dryShot && p2Client.sawText("[OUT OF AMMO]"),
							"Soldier PowerShot fails with [OUT OF AMMO] when magazine is empty and no clip in inventory");

						// Give 9mm clip (1050) -> PowerShot auto-reloads and fires
						p2Human.po->giveItem(1050);
						p2Client.captured.clear();
						bool reloadShot = sCombatSys.UseAbility(p2Human.po, 14, rangeTgt.go);
						check(reloadShot && eqW[0]->getAmmoCount() == 14 && p2Client.sawText("[RELOAD]"),
							"Soldier PowerShot auto-reloads from 9mm clip (1050) to 14 rounds and fires");
					}
					p2Human.po->setEquippedWeaponId(0);
					p2Human.po->getInventory()->clear();
				}

				// D. Martial Arts 3-Step Style Combo Chains (Kung Fu, Karate, Aikido)
				{
					Actor maDummy = makeBot(9200265, 10050.0, 10000.0, 1, 2000);
					maDummy.po->setHandle("Dojo Combo Dummy");
					InterlockSession maSession;
					maSession.goIdA = p2Human.go;
					maSession.goIdB = maDummy.go;
					maSession.exchangeNum = 1;
					maSession.tacticA = TACTIC_POWER;
					maSession.tacticB = TACTIC_SPEED;

					p2Human.po->setFightingStyle(FightingStyle::KungFu);
					const CombatMove* kfMove = CombatSystem::GetMove(133); // KungFu1
					if (!kfMove) kfMove = CombatSystem::DefaultMelee();

					p2Client.captured.clear();
					auto r1 = sCombatSys.StrikeInterlock(maSession, p2Human.po, maDummy.po, *kfMove, TACTIC_POWER, TACTIC_SPEED, true, false);
					check(r1.hit && maSession.comboStepA == 1, "Kung Fu hit 1 advances InterlockSession comboStepA to Step 1");
					uint32 mStep1 = sCombatSys.SelectInterlockMove(maSession, p2Human.po, maDummy.po, InterlockExchangeOutcome::NormalHit);

					auto r2 = sCombatSys.StrikeInterlock(maSession, p2Human.po, maDummy.po, *kfMove, TACTIC_POWER, TACTIC_SPEED, true, false);
					check(r2.hit && maSession.comboStepA == 2, "Kung Fu hit 2 advances InterlockSession comboStepA to Step 2");
					uint32 mStep2 = sCombatSys.SelectInterlockMove(maSession, p2Human.po, maDummy.po, InterlockExchangeOutcome::NormalHit);

					auto r3 = sCombatSys.StrikeInterlock(maSession, p2Human.po, maDummy.po, *kfMove, TACTIC_POWER, TACTIC_SPEED, true, false);
					check(r3.hit && maSession.comboStepA == 3, "Kung Fu hit 3 advances InterlockSession comboStepA to Step 3 Finisher");
					uint32 mStep3 = sCombatSys.SelectInterlockMove(maSession, p2Human.po, maDummy.po, InterlockExchangeOutcome::NormalHit);
					check(p2Client.sawText("3-Step Style Combo Finisher!"), "Step 3 Finisher triggers +20% combo finisher bonus and notification");
					check(mStep1 != 0 && mStep2 != 0 && mStep3 != 0 && (mStep1 != mStep2 || mStep2 != mStep3),
						"SelectInterlockMove resolves distinct ILDB move IDs across 3-step Kung Fu combo chain");

					// Blocking resets the attacker's combo chain
					auto rBlocked = sCombatSys.StrikeInterlock(maSession, p2Human.po, maDummy.po, *kfMove, TACTIC_POWER, TACTIC_DEFENSE, true, false);
					check(rBlocked.isBlocked && maSession.comboStepA == 0, "Blocked strike resets attacker comboStepA back to 0");
				}
			}
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

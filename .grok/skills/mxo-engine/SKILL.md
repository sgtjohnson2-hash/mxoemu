---
name: mxo-engine
description: >
  How the Matrix Online private server (sgtjohnson2-hash/mxoemu, Live-Server)
  actually works: Reality C++ combat, the retail 7.6005 client wire, interlock
  animations, abilities, missions, and the live Docker deploy. Use this
  whenever the user mentions MXO, Matrix Online, Reality, interlock, combat,
  stances, Inner Strength, abilities, the live server, 15.204.82.250, or
  remastering this emulator — even if they do not say "skill" or "engine".
  Read it before editing Server/Reality. Do not follow the Epoch / gaussian /
  neural roadmap markdown files in the repo root.
---

# MXO engine

Retail-parity work on `Live-Server` only. `master` is stale. The player is on the real 7.6005 client. A wrong byte does not "look a bit off". It drops the packet, rewinds the reliable channel, or crashes `client.dll`.

Read [references/combat-wire.md](references/combat-wire.md) before touching combat packets. Read [references/live-ops.md](references/live-ops.md) before telling anyone to rebuild.

## Where the truth is

| Question | Look here | Do not look here |
|---|---|---|
| Does this packet layout exist? | `CombatTests.cpp`, comments that cite a client.dll address or an HDS capture | A roadmap `.md` in the repo root |
| What move does the client play? | `Data/hd_dump/interlock_moves.bin` via `CombatAnimationMatrix` | Hardcoded anim resource ids stuffed into an IL exchange |
| Is the live process updated? | TCP connect to 11000 and 10000 after a rebuild | `git push` by itself |

World scale is **100 units = 1 meter**. Melee interlock spacing is 150 (1.5 m). An interlock breaks at 1500 (15 m).

## Rules that have already broken the live game

1. **Do not invent a packet layout.** If `CombatTests.cpp` locks a size or a byte offset, keep it. `ILExchange` is 0x79 bytes. Move slots 3 and 4 stay zero. Lead-in move is `IL_MOVE_PRE` (`0x5214`). The database id on a move slot is `IL_MOVE_DATABASE` (`0x24000B8B`).
2. **The client plays the ILDB move id, not an animation id.** `aggrAnim` / `defeAnim` are resource ids inside the move record. They must not be written into `moves[][]`. `FindMoveVariant` picks the wire move. `SelectInterlockMove` is only the fallback when the database is not loaded, and the tests still expect its captured ids (`0x2026`, `0x2367`, `0x236D`, `0x2388`, `0x4EE5`).
3. **View ids.** 0 unused, 1 object manager, 2 self (`VIEWID_SELF`). Dynamic views are 3..127. 128 or above makes the client drop the packet ("view ID is out of range"). The local player must resolve as view 2 or the interlock camera never starts.
4. **Tactic 0 is Grab, not Normal.** Normal is 8. `IGO` must construct `m_tactic = 8`. The client sends the raw `mxoTacticType` byte. Do not remap it. Copy a human's `getTactic()` into the session every round. Bots counter `committedTactic*` from the previous round, never the stance the player just switched to.
5. **One mitigation.** `takeDamage` subtracts `level / 2` once. `ResolveAttack` must not subtract it again. Handles containing `Dojo` take full damage. Do not bring back the "adaptive" same-move multiplier. It cut a repeated interlock strike to 15% after four hits.
6. **An interlock swing connects.** The stance exchange already decided the hit. Do not roll a second miss. A block still halves damage and restores 5 Inner Strength. Grab breaks a block and breaks Power.
7. **Damage must reach the victim's own HUD.** `SelfHitFxMsg::toBuf` must not throw. Throwing it dropped the packet and rewound the reliable sequence. Call `sendVitals()` after the health change. Heals already do this.
8. **Inner Strength is spent once, and only if the cast will actually run.** Spend after the hacker / coder / soldier / spy early return. If opening the interlock or the shot fails, `restoreIS`. A failed cast must not start the ability cooldown. `canCastAbility` is checked in `RPC_HandleAbilityUse` before `UseAbility`.
9. **A queued hotbar id that is not a combat move still swings.** Fall back to `GetDefaultStyleMove`. Do not skip the round.
10. **Death is delayed 3 seconds** (`m_deathDelayMS`). `die()` pays the kill once. Do not also pay it from `ResolveAttack`. A second `die()` on a dead object pays nothing.
11. **`GameClient` must be a complete type** anywhere `getClient().QueueCommand` / `isBot()` is called. `DialogTree.cpp` already failed the Docker build for this.
12. **Guns.** Rifle butt (`453`) and pistol whip (`501`) are melee and open an interlock. Other firearms are ranged. A saved gun with `ammoCount == 0` and no `"ammoInit"` in the metadata was saved before ammo existed: load it full. A gun saved with `ammoInit` and 0 is actually empty.
13. **Crash FX.** `0x280006DF`, `0x28000794`, `0x28000432`, and `0x2800045A` are stripped when `Combat.HitFx` is 0. `0x280006DF` is a weapon skeleton that crashes retail 7.6005. Do not force those ids through.

## Combat matrix (do not "balance" this away)

Power crushes Speed (1.35, the Speed swing is cancelled). Speed interrupts Grab (1.35, the Grab is cancelled). Grab breaks Guard (1.40, unblockable) and breaks Power (1.35). Guard versus Speed is 0.60. Identical stances clash at 0.90. Anything else is 1.0. Interlock rounds are 4.0 s before time dilation.

Styles: None/street `0`, Aikido `1`, Kung Fu `2`, Karate `3`. The ILDB file also contains higher style bytes. Do not renumber these to match a hunch. `FindMove` tests already treat Kung Fu as 2.

## Scope

Ship retail behavior the 7.6005 client can show. Ignore repo-root roadmaps about gaussian splatting, spiking neural nets, BFT, Epoch reboots, and biography engines. `World.LegacySimulation` defaults off because those systems only emit chat.

Unknown ability ids: a human gets the "not implemented" chat line and a basic strike. A bot's unknown id is a silent drop. Keep both.

## After a code change

Commit on `Live-Server` as `Reddawn141 <sgt.johnson2@gmail.com>`. `git add` only the files you meant to change. A dirty tree in this sandbox has been committed by accident before (`committedTactic` shipped without its header field and the image would not build).

This sandbox cannot run `RunCombatTestSuite` (no MSVC, no `Dependencies10` build, no MariaDB client). Say so. Do not claim the suite passed.

Push does not update the live process. The player has to rebuild. See [references/live-ops.md](references/live-ops.md). Never `docker compose down -v`.

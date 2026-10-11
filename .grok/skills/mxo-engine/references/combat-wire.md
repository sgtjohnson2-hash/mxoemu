# Combat wire

Load this when editing `CombatSystem.cpp`, `MessageTypes.cpp`, `CombatAnimationMatrix.cpp`, or `PlayerObjectCombat.cpp`.

## ILExchange (captured, tested)

`ILExchange::write` pads to **0x79** bytes. `CombatTests.cpp` locks these offsets:

| Offset | Field | Value in the fixture |
|---|---|---|
| 0x00 | attacker slot, defender slot | 1 = self, 2 = opponent, from that client's point of view |
| 0x02 | attackerAdjustMs | 775 |
| 0x04 | defenderAdjustMs | 775 |
| 0x1E | style | attacker `FightingStyle` |
| 0x1F | unk1F | 0 |
| 0x20 | exchange number | uint16 |
| 0x26 | defenderOffsetMs | contact delay, int16 milliseconds |
| 0x2C | flags | 0x23 |
| 0x2D | moves[0] lead-in | `IL_MOVE_PRE` = `0x5214` when the caller passed that |
| 0x3D | moves[2] main | the ILDB move id |
| 0x55 / 0x59 | attacker / defender health | uint32 current HP |
| 0x5D / 0x5F | unk | `0x1000` |
| 0x6D / 0x71 | unk | `0x00000100`, `0x00602b02` |

Each move slot is two uint32s: `(moveId, IL_MOVE_DATABASE)`. `IL_MOVE_DATABASE` is `0x24000B8B`. Slots 3 and 4 are `(0, 0)`. The opening exchange uses `IL_MOVE_OPEN_ATTACKER_PRE` `0x523C` and `IL_MOVE_OPEN_MAIN` `0x2026` when the caller passes zeros. Leave that opening alone. Per-round exchanges go through `FindMoveVariant`.

`startMs` is `1000000` on purpose. The client clamps a far-future start to "now + 5 ms" on its own clock so skew cannot skip the exchange.

## ILDB file

`Server/Reality/Data/hd_dump/interlock_moves.bin` (also copied to `Data/interlock_moves.bin`).

- Magic `ILMB`, version 1, count 25695, record size 24.
- `moveId` uint32, `aggrAnim` uint16, `defeAnim` uint16, `aggrDur` uint16, `defeDur` uint16, att/def style and tactic as bytes, `flags` byte, 7 pad bytes.
- Flags: bit 0 finisher, bit 1 hit, bit 2 block, bit 3 draw (clash). Many rows have flags 0.
- `moveId` is what the **client** looks up. `aggrAnim` is not a wire move. Known captured ids that exist as rows include `0x2026`, `0x2367`, `0x236D`, `0x2388`, `0x4EE5`, `0x5214`, `0x523C`. `0x2026` is style 0, tactic Normal, flags 0. That is why using it for every swing looks like one generic punch.
- `FindMove` returns the first match and is what the tests call. `FindMoveVariant` walks the same passes and picks `variant % count` so rounds rotate. Both skip empty anims. Variant also skips `moveId == 0`.
- Durations in the dump are often 0. Contact delay then stays 0.50 s. Do not invent durations.

`DataLoader::LoadInterlockMoves` tries the configured path, then `Data/hd_dump/interlock_moves.bin`, then `Data/interlock_moves.bin`. The Docker image copies `Server/Reality/Data` into the binary dir. If the file fails to load, `FindMoveVariant` returns null and the exchange falls back to `SelectInterlockMove`. That fallback is supposed to be rare on the live server.

## Self and other attribute updates

Self view updates are opcode `0x03`, view `2`, type byte `0x02`, then an `AttributeUpdateBlock` **without** a count prefix, then `uint16 0`.

`PlayerAttrSelf::Health` is index 27 (uint16). `InnerStrengthAvailable` is 25. `IsDead` is 21. `CombatantMode` is 43.

`PlayerAttrOther::Health` is index 18. `IsDead` is 15. `CombatantMode` is 31. Do not send self indexes on an other-view packet.

`CombatantModeMsg` throws if the view is 0 or self, so it is only for other clients. `SelfCombatantModeMsg` is the local HUD. `setCombatStance` sends both.

`CombatHitFxMsg` is announced to everyone except the victim and includes other-view health. The victim gets `SelfHitFxMsg` plus `SelfVitalsMsg`.

## RPCs that start a fight

| Opcode | Handler | Behavior |
|---|---|---|
| 0x40 | `RPC_HandleCloseCombatRequest` | view id, optional spawn counter. Opens an interlock. |
| 0x41 | `RPC_HandleRangeCombatRequest` | Free fire. Layout past the view id is still uncaptured. Log leftovers. Do not parse them into new fields. |
| 0x42 | `RPC_HandleChangeTactic` | One byte, already an `mxoTacticType`. Values above 8 become Normal. |
| 0x44 | `RPC_HandleLeaveCombat` | Stops free fire and withdraws. Withdraw takes a parting shot. |
| 0x80b9 | `RPC_HandleAbilityUse` | ability id, target view. Cooldown gate, then `UseAbility`. |
| 0x80ae | `RPC_HandleAbilityLoad` | Hotbar load. Count capped at 20. |

Chat commands `&tactic power|speed|grab|block` and `&style kungfu|karate|aikido|street` write the same fields as the RPCs.

## Simulation order

`GameServer` simulation loop calls `CombatSystem::Update` before human `PlayerObject::Update`. A kill inside a round must not erase the interlock node being iterated. `EndInterlock` during `m_updatingInterlocks` is queued in `m_deferredInterlockEnds` and replayed after the loop. `die()` from the player update (the 3 s timer) runs outside that flag and ends the interlock immediately.

`RemoveCombatant` stops free fire and ends the interlock with `byWithdraw = false` (no parting shot).

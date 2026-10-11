# Remaster to completion

Working list for the Live-Server branch. This is what is left to play the game, not a new engine.

A change is in the running world only after `reality-server` is rebuilt on that commit.

## Done on this branch

The fight can start, play, and end.

- Interlock uses ILDB move ids, including the opening exchange. Style is saved.
- Fighters are pulled to 1.5 m on the attacker's floor and cannot walk the pair apart.
- Damage, blocks, and the health bar use the same numbers. Inner Strength returns during the fight.
- The swing that empties the bar is the last one. Death is the 3 s takedown, then an 8 s reinsert.
- A gun fires on the next tick, uses the equipped weapon, and is not blocked just because you are standing inside a prop's box.
- The equipped weapon and the fighting style survive relog. Jackout drops the connection.
- A hit gives an idle bot a target. Loot goes in the next free slot.

## This pass

- A dead bot stopped being updated, so its respawn event never ran. Dead bots tick until they stand back up.
- Reinsert saves immediately. A disconnect during the old 10 s window put you back at the place you died.

## Still to do, in order

1. **Prove it on the live client.** Rebuild onto the tip and run `&dojo 1v1`, a gun, a death, and a relog. The server cannot be compiled in this sandbox.
2. **Hotbar abilities.** Real ability ids are routed (martial arts, gunner, hacker, coder, spy). An id the server does not know falls back to a basic strike and says so. If a hotbar button does nothing, the client opcode is not the one the server handles. Log it. Do not invent the packet.
3. **Inventory window.** The item is in the database and in chat. There is no captured "put this in the bag" packet, so the window stays stale until the next login.
4. **Gun animation byte.** The state field is one byte. Retail animation ids do not fit. Leave it until a capture shows the real field.
5. **Missions a new character can finish.** The starter mission has to have a visible NPC, a talk that advances, and a reward that stays in the bag. Chat already carries the step. The mission window is the same class of problem as the bag.
6. **Other districts.** Travel has to load that district's streets and not dump the player back in district 1. One district fully playable comes first.
7. **Vanilla client, no memory hook.** World entry still depends on the injected client for the last handshake. Finish that only from a capture of `0x1D` / `0x24` / `0x32`. Do not invent those bytes.

## Not this remaster

A 64-bit client, new textures, and a new audio engine are a different product. They are not required for this world to be finishable.

## Do not do

- Do not put a second damage number on the interlock exchange. The health fields are the current health after the hit.
- Do not remap tactic buttons. The client value is stored as sent when it is 0 through 8.
- Do not send a state update for a player the client has not been introduced to.

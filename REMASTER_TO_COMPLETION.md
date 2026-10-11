# Remaster to completion

Work plan for `Live-Server`. This finishes the game you can log into. It is not a new engine.

A change is in the running world only after `reality-server` is rebuilt on that commit. Tip when this plan was written: `97938718`.

## Done

The fight can start, play, and end.

- Interlock uses ILDB move ids, including the opening exchange. Style is saved.
- Fighters are pulled to 1.5 m on the attacker's floor and cannot walk the pair apart.
- Damage, blocks, and the health bar use the same numbers. Inner Strength returns during the fight.
- The swing that empties the bar is the last one. Death is the 3 s takedown, then an 8 s reinsert. Dead bots keep ticking until they stand up. Reinsert saves immediately.
- A gun fires on the next tick, uses the equipped weapon, and is not blocked just because you are standing inside a prop's box.
- The equipped weapon and the fighting style survive relog. Jackout drops the connection.
- A hit gives an idle bot a target. Loot goes in the next free slot.
- A new character is handed mission `10111`. Steps and rewards are announced in chat.

## How to know a phase is done

Each phase has one live check. Do not start the next phase on a guess. If the check fails, read the server log for that action and fix that, instead of inventing a packet.

## Phase 0 — Prove the fight on the live client

Rebuild `reality-server` onto the tip. In the city:

1. `&dojo 1v1` starts an interlock. Both of you stay at melee range. The clips are not the same punch every round. `&tactic power` and `&tactic block` change the exchange. The bar drops by the number in the hit line. At 0 HP the hits stop, then you or the dummy dies, then the dummy stands back up.
2. A gun says `Firing on <name>` immediately. Rounds land in the street. An empty magazine stops the gun. Relog and the same gun is still out, with the rounds you had left.
3. Die, get the reinsert line, relog. You are at half health at the hardline (or where you stood, if that district has no hardline), not at 0 HP on the death spot.
4. `&jackout` drops the connection.

Until those four pass, do not add systems.

## Phase 1 — One character can finish one mission

Mission `10111` is assigned. Chat prints the step. That is not finished.

Done when a new character can, without admin commands:

1. See the mission NPC in the world, not only a chat line.
2. Talk, and have that talk advance the objective.
3. Get the reward into the bag so it is still there after relog.
4. Get the next step in chat, and the completed mission stays completed after relog.

The mission window on the client is the same problem as the bag: no captured "add this row to the log" packet. Chat is the contract until a capture exists.

## Phase 2 — Abilities the hotbar actually casts

Known ids already route: martial arts set the style, gunner / hacker / coder / spy have handlers. An unknown id says so and uses a basic strike.

Done when:

1. `&loadability <id>` puts a real ability on the bar and the next cast spends Inner Strength once.
2. A martial-arts id changes the style and the next interlock clip matches it.
3. A gunner id fires. A hacker id prints the hack result, not a melee hit.

If a hotbar button sends nothing the server recognises, log the opcode and stop. `0x80BE` is already logged and not answered, because its meaning is a guess. Do not invent the reply.

## Phase 3 — The bag, the gun, and the death screen match the server

These three are blocked on captures. Do them only when a real packet is in hand.

| Gap | What the server does now | What is missing |
|---|---|---|
| Bag window | Item is stored and listed in chat | A captured "add this item" packet |
| Gun animation | The shot resolves and the chat fires | The state field is one byte; retail anim ids do not fit |
| Other players seeing your gear | Equipped id is saved and sent as an attribute | Confirm the client treats that id as the item it spawned |

Do not pad the anim byte. Do not send a second copy of the item and hope the window notices.

## Phase 4 — The city you are standing in

One district, all the way through, before a second one.

Done when, in the district a new character starts in:

1. The street under you is the one you saved, including after relog and after death.
2. Hardline travel moves you and saves the destination. You do not come back in district 1.
3. There is someone to fight without `&dojo` (a spawned hostile within sight of the start), and killing them drops loot you still have after relog.
4. Jackout from that district and a new login put you back on that street.

## Phase 5 — Other districts

Only after phase 4. Each new district is the same four checks: saved position, hardline in and out, one hostile, loot that survives relog. Do not turn them all on in one commit.

## Phase 6 — Vanilla client, no memory hook

World entry still depends on the injected client for the last step. Finish this only from a capture of `0x1D`, `0x24`, and `0x32`. The server already sends spawn, RSI, vitals, and position to a client that is in the world. The hole is the handshake that gets a stock client there. Do not invent those bytes.

## Not this remaster

A 64-bit client, new textures, and a new audio engine are a different product. The old roadmaps that start with a thread-pool rewrite and a Vulkan renderer do not finish this world.

## Do not do

- Do not put a second damage number on the interlock exchange. The health fields are the current health after the hit.
- Do not remap tactic buttons. The client value is stored as sent when it is 0 through 8.
- Do not send a state update for a player the client has not been introduced to.
- Do not answer `0x80BE` or `0x8148` until a capture says what they are.

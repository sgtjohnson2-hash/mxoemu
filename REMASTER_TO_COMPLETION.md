# Remaster to completion

Work plan for `Live-Server`. This finishes the game you can log into. It is not a new engine.

A change is in the running world only after `reality-server` is rebuilt on that commit. Tip when this plan was written: `f0292b2d`.

## Done

The fight can start, play, and end. The street around a player is no longer empty of decisions.

- Interlock uses ILDB move ids, including the opening exchange. Style is saved.
- Fighters are pulled to 1.5 m on the attacker's floor and cannot walk the pair apart.
- Damage, blocks, and the health bar use the same numbers. Inner Strength returns during the fight.
- The swing that empties the bar is the last one. Death is the 3 s takedown, then an 8 s reinsert. Dead bots keep ticking until they stand up. Reinsert saves immediately.
- A gun fires on the next tick, uses the equipped weapon, and is not blocked just because you are standing inside a prop's box.
- The equipped weapon and the fighting style survive relog. Jackout drops the connection.
- A hit gives an idle bot a target. Loot goes in the next free slot.
- Mission `10111` stays on the street. Only the current step's contact is spawned, and that player is shown them immediately. Contacts stand on their mark.
- A tester bot allocates a real view id before it talks, interlocks, shoots, or casts. It can finish a talk step.
- A street bot near you notices a threat inside 25 m, chases to 60 m, covers when it is losing, runs home under a quarter health, and leashes at 80 m. A blocked step is a sidestep.
- A street bot carries equipped Dual Berettas. A player of another faction is a fight. Within 80 m of an enemy hardline it walks on and holds it. Standing inside 15 m is what advances the capture.

## How to know a phase is done

Each phase has one live check. Do not start the next phase on a guess. If the check fails, read the server log for that action and fix that, instead of inventing a packet.

## Now — a bot uses the same tools a player does

The chase and the pistol are in. The ability bar is not.

Done when, with a player standing in the district:

1. A street bot in an interlock casts one real ability (Zion: Kung Fu 133, Machines: Power Shot 14, Merovingian: Restore 77 when hurt, otherwise Poison Knife 146). Inner Strength goes down once per cast.
2. The cast is the handler that ability already has. A gunner id shoots. A martial id sets the style. A coder id heals. It is not a second damage roll.
3. Killing that bot still runs `GenerateLoot`, and the item is in the killer's bag after relog.

## Next — one character finishes mission 10111

The contact is in the world. That is not the mission being finished.

Done when a new character can, without admin commands:

1. See the contact, talk, and have that talk advance the step.
2. Beat the hostile that the next step spawns, and have the defeat advance the step.
3. Get the reward into the bag so it is still there after relog.
4. Come back in and the completed mission stays completed.

The mission window on the client is the same problem as the bag: no captured "add this row to the log" packet. Chat is the contract until a capture exists.

## Then — the hotbar the player presses

`&loadability <id>` already loads a real id and answers with the load response. Known disciplines already route.

Done when:

1. `&loadability <id>` and the next cast spends Inner Strength once.
2. A martial-arts id changes the style and the next interlock clip matches it.
3. A gunner id fires. A hacker id prints the hack result, not a melee hit.

If a hotbar button sends nothing the server recognises, log the opcode and stop. `0x80BE` is already logged and not answered. Do not invent the reply.

## Then — one district, all the way through

Done when, in the district a new character starts in:

1. The street under you is the one you saved, including after relog and after death.
2. Hardline travel moves you and saves the destination. You do not come back in district 1.
3. There is someone to fight without `&dojo`, and killing them drops loot you still have after relog.
4. Jackout from that district and a new login put you back on that street.

## After that — other districts, one at a time

Each new district is the same four checks. Do not turn them all on in one commit.

## Blocked on a capture

| Gap | What the server does now | What is missing |
|---|---|---|
| Bag window | Item is stored and listed in chat | A captured "add this item" packet |
| Gun animation on the body | The shot resolves and the chat fires | The state field is one byte; retail anim ids do not fit |
| Mission log window | The step is in chat | A captured "add this row" packet |
| Stock client, no memory hook | Spawn, RSI, vitals, and position are sent once the client is in the world | A capture of `0x1D`, `0x24`, and `0x32` |

## Not this remaster

A 64-bit client, new textures, and a new audio engine are a different product.

## Do not do

- Do not put a second damage number on the interlock exchange. The health fields are the current health after the hit.
- Do not remap tactic buttons. The client value is stored as sent when it is 0 through 8.
- Do not send a state update for a player the client has not been introduced to.
- Do not answer `0x80BE` or `0x8148` until a capture says what they are.
- Do not turn NPC-versus-NPC fights back on. That locked the street in interlocks nobody could see.

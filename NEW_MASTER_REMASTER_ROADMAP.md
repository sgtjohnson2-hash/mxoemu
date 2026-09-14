# The Matrix Online: Master Architecture & Remaster Roadmap (Epochs I – VI)

```
====================================================================================================
               THE MATRIX ONLINE: NEXT-GENERATION REMASTER & SIMULATION ROADMAP
                              Post-Audit Production Evolution
====================================================================================================
```

## Executive Summary & Milestones Accomplished

Following the comprehensive production audit and live remediation:
- **Server Thread Relief & Lock Elimination**: Futex contention on `m_realizationMutex` and runaway 60Hz `std::async` churn have been completely dismantled. Frank Castle's 10-second triage smoke loop has been clamped with a 120-second cooldown and 3-zone global threshold.
- **Queue Flushing Optimization**: `sObjMgr.ForEachGO` no longer allocates heap snapshots of 15,000 bots 30 times per second; connected human players receive instantaneous 30Hz network queue flushes, while background bot queue flushes are throttled to 500ms.
- **Client Dynamic Raycasting**: Dynamic collision segment intersection against active 3D world geometry is now probed directly in `mxohax_modern.cpp`, with calibrated continuous surface fallback (platform elevation `Y = 603.5`, curbs, stairs, and street `572.0`).
- **Database Alignment**: Operative coordinates for `S1acker` and `Slacker` in the live MariaDB `characters` table have been calibrated to `y = 603.5`.
- **Automated Deployment**: Unified single-command deployment pipelines (`deploy_live.sh` and `deploy_live.ps1`) now automate building, container recycling, log pruning, and telemetry assertion.

The roadmap below defines the multi-epoch blueprint for the ongoing evolution of *The Matrix Online*.

---

## Epoch I: Combat Mechanics, Interlock Systems & Wire-Fu Acrobatics

### 1.1 Complete Rock-Paper-Scissors Martial Arts Interlock Engine
- **Current State**: Visual tactical stance buttons exist in the HUD, but damage resolution and client-server state transitions require full martial arts cinematic interlock pairing.
- **Architectural Directive**:
  - **Interlock Pairing Engine**: Implement bidirectional state pairing between attacker and defender (`InterlockSession`) within `CombatSystem.cpp`.
  - **Deterministic Stance Advantage Matrix**:
    - **Power Attacks** crush **Speed / Fast Attacks** (+35% damage bonus, frame advantage).
    - **Speed Attacks** interrupt **Grab / Throw Maneuvers** (fast jab interrupts startup frames).
    - **Grab / Throw Maneuvers** break **Power / Guard Stances** (unblockable throw bypasses heavy armor).
  - **Dynamic Interlock Animations**: Drive exact animation subpacket triggers in `0x280001C1` to play synchronized attacker strike and defender reaction animations (sweeps, throws, blocks, and counters) rather than generic stagger loops.

### 1.2 Fluid Wire-Fu Acrobatics & Wall-Running
- **Current State**: Basic wire-fu jump (vertical impulse with gravity fall) is operational.
- **Architectural Directive**:
  - **Multi-Phase Wire-Fu Locomotion**:
    - Jump trajectory physics with horizontal momentum preservation.
    - Air-dash and double wire-fu kick mechanics.
    - Surface contact detection: Wall-running along high skyscraper facades when moving parallel to solid collision brushes, with a 2.5-second horizontal wall-glide before dropping.
  - **Bullet-Time Focus Evasion**: When the player activates Focus mode (`m_timeDilation`), incoming supersonic ballistic projectiles within a 5-meter radius trigger matrix code trails and dynamic dodge animations (limbo dodge, cartwheel evade).

---

## Epoch II: Non-Linear Mission Generation & Dynamic Underworld Syndicates

### 2.1 Procedural Mission Synthesis Engine
- **Current State**: Handcrafted static missions and basic contract stubs exist.
- **Architectural Directive**:
  - **Graph-Based Mission Weaver**: Synthesize dynamic, faction-driven missions based on real-time city state (Zion, Machine, and Merovingian tension).
  - **Mission Archetypes**:
    - **Data Extraction**: Infiltrate high-security corporate constructs in Downtown, breach firewalls, and extract cryptographic fragments before MMPD SWAT or Agents arrive.
    - **Exile Asset Rescue**: Locate rogue programs hiding in Industrial warehouse basements, escort them through hostile turf, and extract via ringing hardlines.
    - **Assassination & Counter-Intelligence**: Hunt rogue operatives or double-agents across crowded subway terminals and nightclub backrooms.
  - **Dynamic Branching**: If alarms are tripped or SWAT is alerted, missions dynamically mutate into high-stakes rooftop pursuits or tactical extractions.

### 2.2 Player Syndicate Creation & Underground Rackets
- **Current State**: Underworld treasuries receive heist siphoning, but players cannot formally govern criminal enterprises.
- **Architectural Directive**:
  - **Syndicate Governance**: Allow player crews to establish recognized Syndicates, invest in turf operations, and extract passive revenue from controlled city blocks.
  - **Racket Defense & Turf Raids**: Rival factions or rogue AI operatives (Frank Castle, Merovingian hit squads) launch procedural raids against player-owned rackets, requiring active defense or hiring NPC mercenary squads.

---

## Epoch III: Living MegaCity Simulation: Ecologies, Traffic & Weather

### 3.1 Pedestrian Behavioral Ecologies & Daily Routines
- **Current State**: Ambient pedestrians wander randomly or idle near hardlines.
- **Architectural Directive**:
  - **Circadian Schedule System**: Pedestrians transition between residences, office complexes, cafes, subway hubs, and nightlife venues depending on in-game time of day.
  - **Psychological Reaction Trees**: Pedestrians react dynamically to combat and supernatural phenomena:
    - Normal gunfire: Panic, flee into nearest doorway or shelter, dial 911 (alerting MMPD).
    - Wire-fu jumps / code disruption: Heightened suspicion, risk of panic cascades, triggering immediate Agent possession.
  - **Agent Possession Pipeline**: Any panicked or civilian bot in the vicinity of an unmasked redpill operative can be dynamically targeted by `AgentPossessionManager`, morphing into Agent Smith or standard Agents with seamless RSI transformation and code green shatter FX.

### 3.2 Dynamic Traffic & Vehicular Grid
- **Current State**: Vehicles exist as static roadblocks or scripted convoys.
- **Architectural Directive**:
  - **Spline-Based Roadway Navigation**: Implement multi-lane traffic flow along major avenues, elevated freeways, and intersections using pre-baked road splines.
  - **Vehicular Physics & Collisions**: Vehicles halt at traffic lights, yield to emergency sirens, and react to roadblocks or explosions. Players can jump onto vehicle roofs for high-speed freeway traversal.

### 3.3 Dynamic Meteorological & Matrix Anomaly Weather System
- **Current State**: Static lighting and skybox fog.
- **Architectural Directive**:
  - **Atmospheric Weather Fronts**: Dynamic transitions between heavy acid rain, dense industrial smog, torrential thunderstorms, and clear neon night skies.
  - **Code Rain Degradation**: During viral outbreaks (Smith cascade) or reality ruptures, rain transitions into luminous green cascading digital code, accompanied by skybox pixelation and screen-space scanline aberrations.

---

## Epoch IV: Client Architecture Modernization & Continuous Streaming

### 4.1 Multi-Sector Continuous World Streaming
- **Current State**: Single sector (`slums_barrens_full.metr`) mounted at a time, resulting in distant horizon fog voids.
- **Architectural Directive**:
  - **Seamless Multi-Sector Paging**: Implement multi-block asynchronous streaming in `mxohax_modern.cpp`.
  - Dynamically load adjacent district blocks (Richland, Downtown, International, Westview) into memory as the player approaches boundary triggers, eliminating horizon fog cliffs and enabling true open-city exploration.

### 4.2 Vulkan 1.3 / DX12 RHI Modernization & Modern PBR Shading
- **Current State**: DX9 wrapped to DXVK.
- **Architectural Directive**:
  - **Native Rendering Modernization**: Bypass legacy fixed-function pipeline restrictions. Inject modern PBR shader passes:
    - Physically Based Rendering (albedo, normal, roughness, metallic, ambient occlusion).
    - Real-time dynamic screen-space reflections (SSR) on wet asphalt and glass skyscrapers.
    - Volumetric neon lighting and realistic street lamp scatter.
  - **4K Ultra-HD Upscaled Textures**: Replace 2005-era 256x256 and 512x512 DTX textures with neural 4x upscaled PBR material sets, preserving original art direction while delivering crisp 4K fidelity.

### 4.3 64-Bit Memory Space & Clean-Room Architecture
- **Current State**: 32-bit x86 client binary capped at 2GB virtual memory.
- **Architectural Directive**:
  - **Large Address Aware Flag**: Verify `/LARGEADDRESSAWARE` PE header flag on `matrix.exe` to unlock 4GB virtual address space on 64-bit Windows.
  - **OpenMxO Native 64-bit Engine**: Progressively port client core subsystems into modern C++23 / Rust, utilizing data-oriented design (ECS via `Flecs`), Jolt physics, and cross-platform native Linux / Steam Deck execution.

---

## Epoch V: Spatial Audio Engine & Dynamic Reactive Soundscape

### 5.1 Binaural HRTF 3D Spatial Audio Integration
- **Current State**: Legacy DirectSound stereo pipeline.
- **Architectural Directive**:
  - **Modern Spatial Engine Integration**: Embed `miniaudio` or `FMOD Engine` into the client pipeline.
  - **HRTF Audio Positioning**: Real-time binaural spatial audio for footsteps, martial arts impacts, bullet trajectories, and distant sirens.
  - **Acoustic Occlusion & Urban Reverb**: Dynamic DSP filters that simulate slapback echo off high-rise concrete and low-pass muffling through walls and underground sewers.

### 5.2 Dynamic Reactive Don Davis Score
- **Current State**: Static background ambient loops.
- **Architectural Directive**:
  - **Vertical Layered Music Engine**:
    - **Ambient Layer**: Atmospheric analog synth textures and MegaCity hum.
    - **Tension Layer**: Rising sub-bass and syncopated percussion when infiltrating restricted turf.
    - **Combat Layer**: Breakbeat techno, industrial drums, and orchestral horns during interlocks.
    - **Cataclysm Layer**: Choral chanting and distorted digital frequencies during Agent Smith confrontations.
  - Real-time beat-matched cross-fading based on operative threat level.

---

## Epoch VI: Autonomous Threat Ecosystems & The Agent Smith Cascade

### 6.1 Multi-Stage Viral Infection Cascade
- **Current State**: `SmithCascade` tracks contagion stages in memory.
- **Architectural Directive**:
  - **Visual In-World Infection**: NPCs and operatives infected by Smith turn monochrome gray, adopt business suits and sunglasses, and speak with synchronized voice lines.
  - **District Quarantine & Blackouts**: High-contagion districts experience flickering street lamps, severed hardlines, and roaming clone swarms that hunt human players.
  - **Global Shard Threat Events**: When infection exceeds 50%, global sirens sound, and players from Zion and the Machines must form uneasy alliances to purge infected code before the shard faces architectural wipe.

### 6.2 Frank Castle: The Eternal Vigilante AI
- **Current State**: Fixed triage loop and smoke cooldowns implemented.
- **Architectural Directive**:
  - **Strategic Safehouse Network**: Frank captures, fortifies, and links abandoned safehouses across all five MegaCity districts.
  - **Autonomous Pirate Radio Broadcasts**: Frank periodically transmits tactical warnings, weapon cache coordinates, and hit list updates over FM 88.3 to all players tuned in.
  - **Dynamic Player Co-op Contracts**: Players can assist Frank during safehouse sieges, earning rare tactical microchips, munitions, and high-caliber weaponry.

---

## Roadmap Timeline & Target Milestones

| Milestone | Target Horizon | Core Deliverables | Success Metrics |
| :--- | :--- | :--- | :--- |
| **Milestone 1** | **Current Release** | Thread relief, smoke clamp, MariaDB Y=603.5 calibration, raycasting, unified deploy | Reality CPU < 15%, 0 futex locks, live container operational |
| **Milestone 2** | **Sprint 1 (Weeks 1–3)** | Martial arts interlock pairing, rock-paper-scissors combat, wire-fu wall-running | Smooth PvP/PvE interlock strikes, zero stagger desync |
| **Milestone 3** | **Sprint 2 (Weeks 4–6)** | Procedural mission generation, syndicate racket governance, dynamic safehouses | 20+ emergent mission templates, player-owned turf |
| **Milestone 4** | **Sprint 3 (Weeks 7–9)** | Multi-sector streaming, PBR modern lighting passes, 4K upscale materials | Continuous horizon traversal without white fog boundaries |
| **Milestone 5** | **Sprint 4 (Weeks 10–12)** | HRTF spatial audio, reactive Don Davis soundtrack, dynamic weather code rain | Full 3D binaural soundscape, atmospheric code storms |
| **Milestone 6** | **Sprint 5 (Weeks 13–16)** | Full Agent Smith viral shard events, OpenMxO 64-bit clean-room foundation | City-wide infection cascades, native Linux/Steam Deck support |

```
====================================================================================================
               END OF MASTER ARCHITECTURE & REMASTER ROADMAP (EPOCHS I – VI)
====================================================================================================
```

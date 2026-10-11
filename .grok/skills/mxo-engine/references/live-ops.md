# Live server

Host `15.204.82.250`. Repo `https://github.com/sgtjohnson2-hash/mxoemu`, branch `Live-Server`.

| Port | Protocol | Service | Public? |
|---|---|---|---|
| 80 | TCP | Patch server (Node) | Yes |
| 11000 | TCP | Auth | Yes |
| 10000 | TCP | Margin (character select) | Yes |
| 10000 | UDP | World RCC | Yes |
| 3307 | TCP | MariaDB, published as `127.0.0.1:3307:3306` | No |

`connection refused` on 3307 from the internet is the intended result. It looks the same if the database container is stopped. Check `docker compose ps` on the VPS before calling it healthy.

`connection refused` on 11000 and 10000 means Reality is not running. Port 80 can still be up, because the patch server is a different container.

## Rebuild

On the VPS, in the existing checkout. Do not clone a second copy.

```bash
git fetch origin Live-Server
git checkout Live-Server
git pull --ff-only origin Live-Server
docker compose build reality-server
docker compose up -d reality-server
```

`deploy_live.sh` does this. It does not delete the database volume.

Recreate the database container only when `docker-compose.yml` port bindings change:

```bash
docker compose up -d --force-recreate --no-deps database
```

Never `docker compose down -v`. Never change `MYSQL_ROOT_PASSWORD` / `reality` / `reality` to "fix" a login. Player rows live in the `db_data` volume. SQL in `Server/Reality/SQL` runs only on a **new** volume.

A C++ change is not live until `reality-server` is rebuilt. Pushing to GitHub does nothing to the running binary. Rebuilding drops the current fight. Say that before asking for a rebuild.

## What a healthy rebuild looks like

- `git log -1` is the commit you just pushed, or later.
- `docker compose ps` shows `reality-server` Up, not Exited.
- From outside: TCP connects to 80, 11000, and 10000. 3307 refuses.
- The log is not a cmake or link error. `DialogTree.cpp` missing `GameClient.h` has already failed one image build. If the build fails, paste the last 60 lines and stop. Do not `git reset`.

## Sandbox limits

The agent sandbox that edits this repo has no MSVC and no `Dependencies10` toolchain. `RunCombatTestSuite` cannot be executed here. Do not report it as passed. The Docker build on the VPS is the compile check (`Ubuntu 22.04`, cmake, ninja, `libmysqlclient-dev`, `libboost-all-dev`, `libcrypto++-dev`).

Commit identity used on this branch: `Reddawn141 <sgt.johnson2@gmail.com>`.

## Data the server loads

`DataLoader::LoadAll` reads, from the data directory:

- `abilityIDs.csv` — real retail ability ids. The default hotbar is 600 Close Combat, 137 Martial Arts Initiate, 17 Self Defense. Not 1/2/3.
- `mob_parsed.csv`, `mxoClothing.csv`, `hardlines.csv`, `vendor_items.csv`
- `missions/` plus `sponsors.xml`
- `interlock_moves.bin`

Chapter mission objectives reference `idNpc` values that often have no `<npc>` tag. `MissionSystem` synthesizes a mission NPC for those ids. Do not require a hand-written `<npc>` before the objective can spawn.

`giveItem` on a weapon with `maxAmmo > 0` fills the magazine and `ItemSerializer` writes `"ammoInit":1`.

# MxOEmu (The Matrix Online Emulator) - Live Server Branch

This branch (`Live-Server`) contains the full server stack and containerization required to deploy and host the emulated Matrix Online universe on live cloud infrastructure or dedicated hosting.

## Architecture & Port Mapping

| Service | Port / Protocol | Description |
| :--- | :--- | :--- |
| **Auth Server** | TCP `11000` | RSA-1024 Handshake, Account Authentication, World Shard List |
| **Margin Server** | TCP `10000` | Twofish-128 CBC session, character select/create, 13-stage dynamic load burst |
| **World Game Server** | UDP `10000` | Reliable Communication Channel (RCC sliding window), combat & world sync |
| **Patch Server** | TCP `80` | Node.js asset/manifest patch server for launcher & client updates |
| **Database** | TCP `3306/3307` | MariaDB 10.11 relational persistence layer (characters, RSI, inventory, abilities) |

## Repository Structure

- **`Server/Reality`**: C++20 multi-threaded server core (`Reality`), packet serializers, combat state machine, and spatial grid.
- **`Server/PatchServer`**: Node.js microservice delivering manifests and patch data to connecting clients.
- **`Server/Dependencies10`**: CryptoPP, Sockets, Lua 5.4, MySQL connector, and Recast/Detour navigation mesh dependencies.
- **`Client/Launcher`**: Client-side launcher and bootstrap utilities.
- **`docker-compose.yml`**: Full multi-container orchestration for one-command production server deployment.

## Live Deployment (Docker Compose)

### 1. Requirements
- Docker and Docker Compose installed on your server (Ubuntu 22.04 LTS recommended).
- Ensure host firewall (e.g. `ufw`) permits:
  ```bash
  sudo ufw allow 80/tcp
  sudo ufw allow 10000/tcp
  sudo ufw allow 10000/udp
  sudo ufw allow 11000/tcp
  ```

### 2. Launching the Stack
```bash
git clone -b Live-Server https://github.com/sgtjohnson2-hash/mxoemu.git
cd mxoemu
docker compose up -d --build
```

### 3. Monitoring Logs
```bash
# Reality Server core logs
docker compose logs -f reality-server

# Patch Server logs
docker compose logs -f patch-server
```

## Local Windows Build

To build the Reality server executable locally on Windows:
1. Open the repository root.
2. Run `Server\Reality\build_reality.bat` from an MSVC x64 developer environment with Ninja/CMake.

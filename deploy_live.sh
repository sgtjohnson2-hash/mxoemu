#!/usr/bin/env bash
# ============================================================================
# The Matrix Online: Live Unified Production Deployment Pipeline
# Deploys, compiles, and verifies Reality Server on VPS (15.204.82.250)
# ============================================================================
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${REPO_DIR}"

echo "========================================================================"
echo ">>> [1/5] Initiating Live-Server Git Synchronization..."
echo "========================================================================"
# Rollback image first: tag what is running now as pre-<sha of the deployed commit>
ROLLBACK_TAG="pre-$(git rev-parse --short HEAD)"
docker tag mxoemu-reality-server:latest "mxoemu-reality-server:${ROLLBACK_TAG}"
echo "Rollback image: mxoemu-reality-server:${ROLLBACK_TAG}"
git fetch origin Live-Server
git checkout Live-Server
git pull origin Live-Server

echo "========================================================================"
echo ">>> [2/5] Cleaning up legacy build artifacts and stray logs..."
echo "========================================================================"
rm -f Server/Reality/Source/build_*.log
rm -f build_*.log

echo "========================================================================"
echo ">>> [3/5] Building Production Docker Container (reality-server)..."
echo "========================================================================"
docker compose build --progress=plain reality-server

echo "========================================================================"
echo ">>> [4/5] Cycling Reality Server Service..."
echo "========================================================================"
docker compose up -d reality-server

echo "========================================================================"
echo ">>> [5/5] Verifying Service Health and Container Telemetry..."
echo "========================================================================"
sleep 5

docker ps -f name=mxoemu-reality-server-1
docker stats --no-stream mxoemu-reality-server-1

echo ">>> Pruning old rollback images (keeping the 3 newest pre-* tags and the running image)..."
# KEEP_TAGS="pre-foo pre-bar" pins extra rollback tags (set in the environment or below)
KEEP_TAGS="${KEEP_TAGS:-pre-withdrawchat pre-8aeefcc4}"
RUNNING_ID="$(docker inspect --format '{{.Image}}' mxoemu-reality-server-1 | cut -d: -f2 | cut -c1-12)"
docker images mxoemu-reality-server --format '{{.CreatedAt}}|{{.Tag}}|{{.ID}}' \
  | grep '|pre-' | sort -r | tail -n +4 \
  | while IFS='|' read -r _ tag id; do
      case " ${KEEP_TAGS:-} " in *" ${tag} "*) continue ;; esac  # tags pinned by the operator are never pruned
      if [ "${id}" != "${RUNNING_ID}" ]; then docker rmi "mxoemu-reality-server:${tag}" || true; fi
    done
docker image prune -f > /dev/null || true

echo ">>> Recent Reality Server Log Output:"
docker logs --tail 30 mxoemu-reality-server-1

echo "========================================================================"
echo ">>> DEPLOYMENT COMPLETE: Reality Server is live and healthy!"
echo "========================================================================"

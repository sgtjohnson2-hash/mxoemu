#!/bin/bash
cd ~/mxoemu
echo 'Tagging rollback...'
docker tag mxoemu-reality-server:latest mxoemu-reality-server:pre-deploy
echo 'Pulling...'
git pull --ff-only origin Live-Server
echo 'Building...'
docker compose build reality-server
echo 'Starting...'
docker compose up -d reality-server
echo 'Uptime:'
docker ps --filter name=mxoemu-reality-server-1 --format '{{.Status}}'

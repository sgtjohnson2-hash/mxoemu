#!/bin/bash
tset -e

echo "Tagging rollback image..."
docker tag mxoemu-reality-server:latest mxoemu-reality-server:pre-rollback || true

echo "Pulling latest code..."
git pull origin Live-Server --ff-only

echo "Building reality-server..."
nohup docker compose build reality-server > ~/buildNN.log 2>&1 &
BUILD_PID=\$!
wait \

echo "Starting reality-server..."
docker compose up -d reality-server

sleep 5
docker ps -f name=mxoemu-reality-server-1

echo "Uptime check:"
docker stats --no-stream mxoemu-reality-server-1

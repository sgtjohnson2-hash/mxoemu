#!/bin/bash
set -e

echo "Restoring rollback image..."
docker tag mxoemu-reality-server:pre-rollback mxoemu-reality-server:latest

echo "Starting reality-server..."
docker compose up -d reality-server

sleep 5
docker ps -f name=mxoemu-reality-server-1

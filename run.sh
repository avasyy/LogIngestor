#!/usr/bin/env bash
set -euo pipefail

IMAGE="logingestor"
NAME="logingestor"

echo "Building image..."
docker build -t "$IMAGE" .

echo "Starting container..."
docker run --rm -it --name "$NAME" "$IMAGE"
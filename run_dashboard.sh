#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_PORT="${MEMORY_POOL_BACKEND_PORT:-8080}"
FRONTEND_PORT="${MEMORY_POOL_FRONTEND_PORT:-3000}"
BACKEND_BINARY="${MEMORY_POOL_BACKEND_BINARY:-$ROOT_DIR/build/memory_pool_server}"
HISTORY_PATH="${MEMORY_POOL_EXPERIMENTS_PATH:-$ROOT_DIR/data/experiments.bin}"

if [[ ! -x "$BACKEND_BINARY" ]]; then
  printf 'Backend executable not found: %s\nBuild it with: cmake -S . -B build && cmake --build build\n' "$BACKEND_BINARY" >&2
  exit 1
fi

if [[ ! -d "$ROOT_DIR/frontend/node_modules" ]]; then
  printf 'Frontend dependencies are missing.\nInstall them with: cd frontend && npm ci\n' >&2
  exit 1
fi

backend_pid=""
frontend_pid=""

cleanup() {
  trap - EXIT INT TERM
  [[ -n "$frontend_pid" ]] && kill "$frontend_pid" 2>/dev/null || true
  [[ -n "$backend_pid" ]] && kill "$backend_pid" 2>/dev/null || true
  wait "$frontend_pid" "$backend_pid" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

mkdir -p "$(dirname -- "$HISTORY_PATH")"

MEMORY_POOL_EXPERIMENTS_PATH="$HISTORY_PATH" \
  "$BACKEND_BINARY" "$BACKEND_PORT" &
backend_pid=$!

MEMORY_POOL_API_URL="http://localhost:$BACKEND_PORT" \
  npm --prefix "$ROOT_DIR/frontend" run dev -- --host 0.0.0.0 --port "$FRONTEND_PORT" &
frontend_pid=$!

printf 'Backend:  http://localhost:%s\n' "$BACKEND_PORT"
printf 'Dashboard: http://localhost:%s/\n' "$FRONTEND_PORT"
printf 'Press Ctrl+C to stop both services.\n'

wait -n "$backend_pid" "$frontend_pid"
status=$?
printf 'A dashboard service stopped. Shutting down the remaining service.\n' >&2
exit "$status"

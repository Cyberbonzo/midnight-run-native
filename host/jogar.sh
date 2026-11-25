#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p "${XDG_STATE_HOME:-$HOME/.local/state}/midnight-run"
export MIDNIGHT_PERF_LOG=1
exec ./midnight "$PWD/assets" >"${XDG_STATE_HOME:-$HOME/.local/state}/midnight-run/session.log" 2>&1

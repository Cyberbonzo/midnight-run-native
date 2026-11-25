#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
exec make -f host/Makefile -j "${JOBS:-4}" midnight


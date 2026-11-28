#!/bin/bash
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
exec "$HERE/scripts/install_stems_macos.sh"

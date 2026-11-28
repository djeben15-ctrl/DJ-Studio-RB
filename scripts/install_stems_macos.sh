#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENV="$HOME/djstems-venv"
PYTHON_BIN=""

for candidate in python3.8 python3; do
  if command -v "$candidate" >/dev/null 2>&1; then
    version="$("$candidate" -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')"
    if [ "$version" = "3.8" ]; then
      PYTHON_BIN="$(command -v "$candidate")"
      break
    fi
  fi
done

if [ -z "$PYTHON_BIN" ]; then
  echo "No se encontró Python 3.8. Instala una versión x86_64 compatible con macOS 10.13 y vuelve a ejecutar."
  exit 2
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "No se encontró FFmpeg. Instálalo antes de configurar STEMS."
  exit 3
fi

"$PYTHON_BIN" -m venv "$VENV"
# shellcheck disable=SC1091
source "$VENV/bin/activate"
python -m pip install --upgrade "pip<24.1" "setuptools<70" wheel
python -m pip install -r "$ROOT/requirements-stems.txt"

APP_SUPPORT="$HOME/Library/Application Support/DJStudioRB/scripts"
mkdir -p "$APP_SUPPORT"
cp "$ROOT/scripts/stems.py" "$APP_SUPPORT/stems.py"

python - <<'PY'
from spleeter.separator import Separator
Separator("spleeter:4stems", multiprocess=False)
print("STEMS_ENGINE_READY")
PY

echo "Instalación terminada. Activa el entorno con: source \"$VENV/bin/activate\""

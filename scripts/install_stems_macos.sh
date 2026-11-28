#!/bin/bash
set -euo pipefail

APP_SUPPORT="$HOME/Library/Application Support/DJStudioRB"
VENV="$HOME/djstems-venv"
PYTHON_PKG="/tmp/python-3.8.10-macosx10.9.pkg"
PYTHON_URL="https://www.python.org/ftp/python/3.8.10/python-3.8.10-macosx10.9.pkg"

echo "DJ Studio RB - Instalador STEMS"
echo "Se requiere Internet solo para esta instalación inicial."

PYTHON_BIN=""
for candidate in /Library/Frameworks/Python.framework/Versions/3.8/bin/python3.8 /usr/local/bin/python3.8 python3.8; do
  if [ -x "$candidate" ] || command -v "$candidate" >/dev/null 2>&1; then
    resolved="$(command -v "$candidate" 2>/dev/null || printf '%s' "$candidate")"
    version="$("$resolved" -c 'import sys; print(".".join(map(str, sys.version_info[:2])))' 2>/dev/null || true)"
    if [ "$version" = "3.8" ]; then PYTHON_BIN="$resolved"; break; fi
  fi
done

if [ -z "$PYTHON_BIN" ]; then
  echo "Descargando Python 3.8.10 compatible con macOS 10.9 o posterior..."
  /usr/bin/curl -fL --retry 2 "$PYTHON_URL" -o "$PYTHON_PKG"
  echo "Se abrirá el instalador oficial de Python. macOS puede solicitar autorización."
  /usr/sbin/installer -pkg "$PYTHON_PKG" -target /
  rm -f "$PYTHON_PKG"
  PYTHON_BIN="/Library/Frameworks/Python.framework/Versions/3.8/bin/python3.8"
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo ""
  echo "No se encontró FFmpeg."
  echo "Instala FFmpeg compatible con macOS 10.13 y vuelve a ejecutar este instalador."
  echo "STEMS no se activará hasta completar ese requisito."
  exit 3
fi

"$PYTHON_BIN" -m venv "$VENV"
"$VENV/bin/python" -m pip install --upgrade "pip<24.1" "setuptools<70" wheel
"$VENV/bin/python" -m pip install -r "$(cd "$(dirname "$0")" && pwd)/requirements-stems.txt"

mkdir -p "$APP_SUPPORT/scripts"
cp "$(cd "$(dirname "$0")" && pwd)/scripts/stems.py" "$APP_SUPPORT/scripts/stems.py"

echo "Descargando y preparando el modelo Spleeter 4stems..."
"$VENV/bin/python" - <<'PY'
from spleeter.separator import Separator
Separator("spleeter:4stems", multiprocess=False)
print("STEMS_ENGINE_READY")
PY

echo ""
echo "Instalación STEMS completada."
echo "DJ Studio RB ya puede preparar separaciones localmente."
echo "Después de esta instalación, la separación y reproducción STEMS no requieren Internet."
read -r -p "Presiona Enter para cerrar..."

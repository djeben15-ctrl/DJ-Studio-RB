#!/usr/bin/env python3
"""
DJ Studio RB - offline STEM separation helper.
Run this helper before loading the generated stems into the DJ application.
Requires Python 3.8 and the pinned packages in requirements-stems.txt.
"""
import argparse
import json
import os
import sys
from pathlib import Path

try:
    from spleeter.separator import Separator
except Exception as exc:
    print("STEMS_ENGINE_ERROR: No se pudo cargar Spleeter: {}".format(exc), file=sys.stderr)
    sys.exit(2)


def main():
    parser = argparse.ArgumentParser(description="Separate a track into vocals, drums, bass and other.")
    parser.add_argument("audio_file", help="Input audio file (MP3/WAV/AIFF supported by installed ffmpeg)")
    parser.add_argument("--output", help="Output directory; defaults to DJ Studio RB STEMS cache")
    parser.add_argument("--cache-root", help="Cache root; defaults to ~/Library/Application Support/DJStudioRB/STEMS")
    args = parser.parse_args()

    source = Path(args.audio_file).expanduser().resolve()
    if not source.is_file():
        print("STEMS_ENGINE_ERROR: No existe el archivo: {}".format(source), file=sys.stderr)
        return 3

    cache_root = Path(args.cache_root).expanduser() if args.cache_root else (
        Path.home() / "Library" / "Application Support" / "DJStudioRB" / "STEMS"
    )
    output_root = Path(args.output).expanduser() if args.output else cache_root
    output_root.mkdir(parents=True, exist_ok=True)

    stat = source.stat()
    cache_key = "{}_{}_{}".format(source.stem, stat.st_size, int(stat.st_mtime))
    target = output_root / cache_key
    manifest = target / "manifest.json"
    expected = ["vocals.wav", "drums.wav", "bass.wav", "other.wav"]

    if manifest.is_file() and all((target / name).is_file() for name in expected):
        print("STEMS_CACHE_HIT: {}".format(target))
        return 0

    target.mkdir(parents=True, exist_ok=True)
    print("STEMS_ANALYSIS_STARTED: {}".format(source), flush=True)
    try:
        separator = Separator("spleeter:4stems", multiprocess=False)
        separator.separate_to_file(str(source), str(target), codec="wav", bitrate="16k")
    except Exception as exc:
        print("STEMS_ENGINE_ERROR: {}".format(exc), file=sys.stderr)
        return 4

    # Spleeter writes <target>/<source-stem>/{vocals,drums,bass,other}.wav.
    generated = target / source.stem
    mapping = {
        "vocals": generated / "vocals.wav",
        "drums": generated / "drums.wav",
        "bass": generated / "bass.wav",
        "other": generated / "other.wav",
    }
    missing = [key for key, path in mapping.items() if not path.is_file()]
    if missing:
        print("STEMS_ENGINE_ERROR: Faltan pistas: {}".format(", ".join(missing)), file=sys.stderr)
        return 5

    for key, path in mapping.items():
        final_path = target / (key + ".wav")
        if final_path.exists():
            final_path.unlink()
        path.replace(final_path)

    try:
        generated.rmdir()
    except OSError:
        pass

    data = {
        "source": str(source),
        "source_size": stat.st_size,
        "source_mtime": int(stat.st_mtime),
        "model": "spleeter:4stems",
        "sample_stems": {key: str(target / (key + ".wav")) for key in mapping},
        "status": "complete",
    }
    manifest.write_text(json.dumps(data, indent=2), encoding="utf-8")
    print("STEMS_READY: {}".format(target), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

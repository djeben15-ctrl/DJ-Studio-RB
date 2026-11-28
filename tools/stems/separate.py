#!/usr/bin/env python3
"""Offline, CPU-only 4-stem separation helper for DJ Studio RB.

Requires Python 3.8 and the pinned requirements in this directory.
The first run downloads Spleeter's 4-stem model; later runs reuse its local cache.
"""
import argparse
import json
import os
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("audio_file", help="Input audio file (MP3/WAV/AIFF supported by ffmpeg)")
    parser.add_argument("--output", required=True, help="Directory for separated stems")
    args = parser.parse_args()

    source = Path(args.audio_file).expanduser().resolve()
    output = Path(args.output).expanduser().resolve()
    if not source.is_file():
        print("ERROR: audio file does not exist", file=sys.stderr)
        return 2

    try:
        from spleeter.separator import Separator
    except Exception as exc:
        print("ERROR: Spleeter is not installed in the DJ Studio RB STEMS environment: " + str(exc),
              file=sys.stderr)
        return 3

    output.mkdir(parents=True, exist_ok=True)
    try:
        # CPU-only; four outputs: vocals, drums, bass and other (melody/instruments).
        separator = Separator("spleeter:4stems", multiprocess=False)
        separator.separate_to_file(str(source), str(output), codec="wav", bitrate="16k")
        track_dir = output / source.stem
        expected = ("vocals.wav", "drums.wav", "bass.wav", "other.wav")
        missing = [name for name in expected if not (track_dir / name).is_file()]
        if missing:
            raise RuntimeError("Spleeter did not create: " + ", ".join(missing))

        manifest = {
            "source": str(source),
            "source_size": source.stat().st_size,
            "source_mtime": source.stat().st_mtime,
            "model": "spleeter:4stems",
            "sample_rate": 44100,
            "stems": {name[:-4]: str(track_dir / name) for name in expected},
        }
        (track_dir / "djstudiorb-stems.json").write_text(
            json.dumps(manifest, indent=2), encoding="utf-8"
        )
        print(json.dumps(manifest))
        return 0
    except Exception as exc:
        print("STEMS_ERROR: " + str(exc), file=sys.stderr)
        return 4


if __name__ == "__main__":
    sys.exit(main())

# DJ Studio RB STEMS helper (CPU / offline after first model download)

This is the first STEMS integration milestone: a separate local worker that creates
four synchronized WAV stems without running separation in the live audio callback.

## Target environment
- Intel Mac, macOS 10.13 High Sierra
- Python 3.8
- CPU-only; no GPU required

## Install
Create an isolated environment and install the pinned packages:

```sh
python3.8 -m venv ~/DJStudioRB-stems-venv
source ~/DJStudioRB-stems-venv/bin/activate
python -m pip install --upgrade "pip<24.1"
python -m pip install -r requirements-high-sierra.txt
```

The first separation downloads the Spleeter 4-stem model. It requires an internet
connection once. Subsequent separations reuse the cached model and run locally.
On an older Intel Mac, separation can take longer than the song duration.

## Run
```sh
python separate.py "/path/to/song.mp3" --output "$HOME/Library/Application Support/DJStudioRB/STEMS"
```

The helper creates a folder named after the source track containing:
- vocals.wav
- drums.wav
- bass.wav
- other.wav (melody / remaining instruments)
- djstudiorb-stems.json (source fingerprint and stem paths)

The helper intentionally does not touch the live audio engine. The next integration
step is to load the four WAVs into JUCE and switch their gains atomically during
playback, keeping all stems sample-aligned. Until that mixer integration is complete,
these files are not yet controllable from the deck UI.

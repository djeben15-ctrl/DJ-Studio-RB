# DJ Studio RB - STEMS engine (first integration stage)

This stage adds an offline, pre-processing helper. It separates a song into four WAV files:
- vocals
- drums
- bass
- other (melody and remaining accompaniment)

## Important compatibility note

The target Mac runs macOS High Sierra 10.13.6 on Intel. Spleeter 2.3.2 / TensorFlow 2.5.0 is a legacy Python stack and may not install on every High Sierra setup. Use a separate Python 3.8 x86_64 environment. Do not install these packages into the DJ Studio RB app or its audio callback. If TensorFlow wheels or dependencies are unavailable for the Mac, run separation on another computer and copy the generated STEMS folder to the Mac.

## Install

1. Install Python 3.8 x86_64 and FFmpeg.
2. Create and activate a virtual environment:
   ```sh
   python3.8 -m venv ~/djstems-venv
   source ~/djstems-venv/bin/activate
   pip install --upgrade pip
   pip install -r requirements-stems.txt
   ```
3. Separate a track:
   ```sh
   python scripts/stems.py "/path/to/song.mp3"
   ```

## Output/cache

Default cache:
`~/Library/Application Support/DJStudioRB/STEMS/<track>_<size>_<mtime>/`

Files:
`vocals.wav`, `drums.wav`, `bass.wav`, `other.wav`, and `manifest.json`.

The helper reuses a completed cache entry for the same source file size and modification time. A changed source file gets a new cache entry.

## Integration boundary

This helper is not yet connected to the JUCE mixer UI. The next stage must:
1. Launch the helper on a worker thread when a track is loaded.
2. Load all four stems and verify equal sample rate, channel count, and duration.
3. Add four independent atomic mute/solo controls per deck.
4. Mix stems in the audio callback without file I/O, locks, allocations, or model inference.
5. Preserve normal playback if STEMS separation fails or is not installed.

Do not label the UI feature as working until those mixer controls are connected and tested.

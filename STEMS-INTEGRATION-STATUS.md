# STEMS setup for DJ Studio RB

## Current implementation status

- [x] Offline Spleeter 4-stem helper.
- [x] Local cache with source-file size and modification-time validation.
- [x] Pinned legacy Python dependencies.
- [x] High Sierra setup script with explicit compatibility checks.
- [ ] Launch separation automatically from the DJ Studio RB track loader.
- [ ] Load the four generated WAV files into per-deck stem buffers.
- [ ] Add four per-deck mute controls to the real-time mixer.
- [ ] Validate synchronization with pitch, key lock, scratch, cue and seek.

## Install on the target Mac

The target is macOS 10.13.6 Intel. Spleeter's Python/TensorFlow stack is legacy and installation can fail depending on available wheels and system libraries. The installer intentionally stops if Python 3.8 or FFmpeg is missing; it does not alter the DJ Studio RB app.

Run from a Terminal window:
```sh
cd /path/to/DJ-Studio-RB
chmod +x scripts/install_stems_macos.sh
./scripts/install_stems_macos.sh
```

Then test one MP3:
```sh
source ~/djstems-venv/bin/activate
python scripts/stems.py "/path/to/song.mp3"
```

The expected cache contains vocals.wav, drums.wav, bass.wav, other.wav and manifest.json under:
`~/Library/Application Support/DJStudioRB/STEMS/`

A successful separation test does not yet mean the DJ app's mixer controls are connected. Do not use the STEMS mode for live mixing until the remaining mixer integration checklist is complete.

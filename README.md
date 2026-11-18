# Pandemonium! for New Nintendo 3DS

A native New Nintendo 3DS adaptation of the N-Gage 2.0 version of **Pandemonium!**, derived from [LeithSilver's MGSM project](https://github.com/LeithSilver/mgsm) and the separate Need for Speed Undercover compatibility work.

Runs the original ARM game through a compatibility runtime. Supports restored backgrounds, save/resume, music and overlapping sound effects, contextual 3DS button prompts, and opening/ending movie playback.

## Requirements

- New Nintendo 3DS / New 3DS XL / New Nintendo 2DS XL with a homebrew launcher. Original 3DS models are unsupported.
- Your own supported Pandemonium N-Gage package.
- Python 3.9 or newer and FFmpeg available on PATH to prepare game data. No Python packages are required for extraction.

Game code, game data and converted movies are **not included** in the GitHub source or release archive. The launcher and bottom-screen artwork supplied for this port are included.

## Installation

1. Copy the release archive's `3ds/` folder to the SD card root.
2. On your computer, prepare your package into a staging folder:

   ```sh
   python tools/prepare_game.py "Pandemonium.n-gage" prepared
   ```

   Windows users can use `py -3` instead of `python`. If necessary, add `--ffmpeg "C:/path/to/ffmpeg.exe"`.
3. Merge the resulting `prepared/3ds/` folder onto the SD card.
4. Launch **Pandemonium!** from the Homebrew Launcher.

The result should contain:

```
3ds/pandemonium/pandemonium.3dsx
3ds/pandemonium/pandemonium.smdh
3ds/pandemonium/game/pandemonium.xe3u
3ds/pandemonium/game/...other extracted resources...
3ds/pandemonium/video/intro.mpg
3ds/pandemonium/video/ending.mpg
```

Saves are created in `3ds/pandemonium/user/`. Keep that directory when updating. Need for Speed Undercover uses its own separate directory.

The preparation script verifies the package SHA-256, all 65 embedded installed-file SHA-1 hashes, the unpacked executable and movie source hashes. It preserves the game's original split-volume resources. Only this exact verified package is supported:

```
f9c62764a984d151c3456d8cfb5256423e83f38736e248e32e584d8b0f1f6fa7
```

Renaming the package does not affect verification. Different releases/repacked packages are rejected; do not bypass the check without verifying their executable and resources. The tool does not download game files or modify licensing data. It refuses to overwrite an existing `game/` or `video/`; use an empty staging directory when preparing again. It never writes `user/` saves.

Movie conversion uses MPEG-1 video at 25 fps and mono 32 kHz MP2 audio. Output bytes can differ between FFmpeg versions. The tool records conversion settings and hashes locally.

## Controls

| 3DS input | Function |
|---|---|
| D-pad / Circle Pad | Movement / menus |
| A | Confirm / centre key; skip movie |
| B | Back / right softkey |
| Start | Pause / left softkey |
| X | Original keypad 5 |
| Y | Original keypad 0 / fire |
| L | Original C / character switch |
| R | Original keypad 9 |
| HOME | System menu |

Follow the contextual A, B, B2 and Start artwork on the bottom screen. During a movie, release A first and press it again to skip. Select is not mapped to exit.

## Build

Use a devkitPro 3DS development shell with devkitARM, libctru, 3ds-zlib and 3ds-mpg123 installed. Python Pillow is required if artwork is regenerated (`python -m pip install Pillow`).

```sh
make -j4
```

Outputs are `pandemonium.3dsx` and `pandemonium.smdh`. The release was built with devkitARM r68 (GCC 16.1.0) and libctru 2.7.1. Generated artwork headers are included. The Makefile embeds the launcher metadata and links the bundled PL_MPEG decoder.

The default build does not create a diagnostic log. To enable it, rebuild cleanly:

```sh
make clean
make LOGGING=1 -j4
```

This writes `sdmc:/3ds/pandemonium/pandemonium.log`, replaced on each launch. Fatal errors still display on screen in the release build.

### Desktop diagnostics

The Linux probe requires g++, zlib development headers and the Python `unicorn` package (tested with 2.1.4):

```sh
python -m pip install unicorn==2.1.4
python build_desktop.py
./pandemonium_probe prepared/3ds/pandemonium/game 300 frames probe_user
```

The probe writes framebuffer captures and uses a separate save directory. It is a diagnostic runner, not a packaged desktop game frontend. `PAN_TEST_VIDEO=0` simulates successful movie handoff; it does not display a movie. Native movie decoding can be checked independently:

```sh
g++ -std=c++17 -O2 -I. tests/movie_decode_probe.cpp decoder.cpp -o movie_decode_probe
./movie_decode_probe prepared/3ds/pandemonium/video/intro.mpg
python tests/movie_viewport_test.py
```

## Status and credits

The ending video is untested, but the opening works fine so uh! If it doesn't work, mention it under issues.

Thanks to LeithSilver for MGSM and to the original game developers. This is an unofficial compatibility project. The inherited project license is retained in `LICENSE`; PL_MPEG, LZMA and mpg123 notices/source are in `third_party/`. Port artwork and original game assets are not relicensed by the code license. See `THIRD_PARTY.md` and `CHANGELOG.md`.

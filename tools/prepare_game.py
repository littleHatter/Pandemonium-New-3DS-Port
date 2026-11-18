#!/usr/bin/env python3
"""Prepare an SD installation from the supported user's own N-Gage package."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from prepare_pandemonium import extract
from sis_support import PreparationError

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('output', type=Path, help='SD root or a staging folder; need not exist')
    parser.add_argument('--ffmpeg', default='ffmpeg', help='FFmpeg executable (default: PATH)')
    args = parser.parse_args()
    target = args.output / '3ds' / 'pandemonium'
    if (target / 'game').exists() or (target / 'video').exists():
        parser.error('game/ or video/ already exists. Use an empty staging directory, then merge onto your SD card.')
    if not shutil.which(args.ffmpeg):
        parser.error('FFmpeg was not found. Install it or supply --ffmpeg /path/to/ffmpeg.')
    try:
        # Validate and convert before touching the destination. Saves are never written.
        with tempfile.TemporaryDirectory(prefix='pandemonium-prepare-') as temp:
            stage = Path(temp)
            extract(args.package, stage)
            subprocess.run([sys.executable, str(Path(__file__).with_name('prepare_videos.py')),
                            str(stage / 'game'), str(stage / 'video'), '--ffmpeg', args.ffmpeg], check=True)
            target.mkdir(parents=True, exist_ok=True)
            shutil.copytree(stage / 'game', target / 'game')
            shutil.copytree(stage / 'video', target / 'video')
            shutil.copy2(stage / 'package_inventory.json', target / 'package_inventory.json')
        print('Prepared:', target.resolve())
        print('Merge this output onto your SD card alongside pandemonium.3dsx. Existing user/ saves are untouched.')
    except (PreparationError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'Preparation failed: ' + str(error) + '\n')

if __name__ == '__main__':
    main()

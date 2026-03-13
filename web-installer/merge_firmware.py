#!/usr/bin/env python3
"""
merge_firmware.py — Copies the merged firmware binary produced by the PlatformIO
build (via copy_fw_files.py) into web-installer/firmware-merged.bin so it can be
served by ESP Web Tools.

The PlatformIO build already calls esptool merge_bin internally via copy_fw_files.py,
so this script just locates the resulting *_merged_*.bin in the build directory and
copies it to the expected location.

Run this from the repo root after a successful PlatformIO build:
    python web-installer/merge_firmware.py

No extra dependencies required (uses only stdlib).
"""

import sys
import shutil
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
REPO_ROOT = Path(__file__).parent.parent
BUILD_DIR = REPO_ROOT / ".pio" / "build" / "ccrawford_cc_g5_esp32s3"
OUTPUT    = REPO_ROOT / "web-installer" / "firmware-merged.bin"

# ---------------------------------------------------------------------------
# Locate the merged binary — PlatformIO names it <progname>_merged_<ver>.bin
# ---------------------------------------------------------------------------
if not BUILD_DIR.exists():
    print("ERROR: Build directory not found. Run 'pio run -e ccrawford_cc_g5_esp32s3' first.")
    print(f"  Expected: {BUILD_DIR}")
    sys.exit(1)

candidates = sorted(BUILD_DIR.glob("*_merged_*.bin"))

if not candidates:
    print("ERROR: No merged binary found in build directory. Run 'pio run' first.")
    print(f"  Looked in: {BUILD_DIR}")
    sys.exit(1)

# Use the most recently modified one in case multiple versions exist
source = max(candidates, key=lambda p: p.stat().st_mtime)

# ---------------------------------------------------------------------------
# Copy to web-installer/
# ---------------------------------------------------------------------------
shutil.copy2(source, OUTPUT)
size_kb = OUTPUT.stat().st_size // 1024
print(f"Copied: {source.name}")
print(f"    ->  {OUTPUT}")
print(f"  Size: {size_kb} KB")

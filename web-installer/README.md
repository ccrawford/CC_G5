# CC_G5 Web Installer

Browser-based firmware flasher for the CC_G5 Glass Cockpit G5 display, powered by
[ESP Web Tools](https://esphome.github.io/esp-web-tools/).

Users only need a Chrome or Edge browser — no Python, no drivers, no PlatformIO.

---

## Files

| File | Purpose |
|---|---|
| `index.html` | The installer web page |
| `manifest.json` | Describes the firmware build (chip family, binary path, offset) |
| `firmware-merged.bin` | Single merged binary flashed at offset 0x0 *(gitignored — rebuild each release)* |
| `merge_firmware.py` | Script to regenerate `firmware-merged.bin` from PlatformIO build output |

---

## How to Rebuild the Merged Binary

### 1. Build the firmware with PlatformIO

From the repo root:

```bash
pio run -e ccrawford_cc_g5_esp32s3
```

The build automatically runs `copy_fw_files.py`, which calls `esptool merge_bin`
internally and produces a merged binary named `*_merged_*.bin` in
`.pio/build/ccrawford_cc_g5_esp32s3/`.

The merge uses these parameters (from `CC_G5_platformio.ini` and the board definition):

| Parameter | Value | Source |
|---|---|---|
| Chip | `esp32s3` | board: esp32-s3-devkitc-1 |
| Flash mode | `qio` | `board_build.flash_mode = qio` |
| Flash freq | `80m` | `board_build.f_flash = 80000000L` |
| Flash size | `8MB` | board upload.flash_size |
| Bootloader offset | `0x0` | ESP32-S3 (differs from classic ESP32's `0x1000`) |
| Partition table offset | `0x8000` | Standard ESP32-S3 |
| App offset | `0x10000` | `huge_app.csv` |

### 2. Run the copy script

```bash
python web-installer/merge_firmware.py
```

This finds the `*_merged_*.bin` in the PlatformIO build output and copies it to
`web-installer/firmware-merged.bin`. No extra dependencies needed.

> **Note on ESP32-S3 vs classic ESP32:** The ESP32-S3 places its bootloader at
> flash offset `0x0`, while the original ESP32 uses `0x1000`. The merged binary
> must be flashed at `0x0` in both cases — the difference is internal to the
> merged file layout.

---

## Hosting on GitHub Pages

### One-time setup

1. Go to your repo on GitHub → **Settings** → **Pages**
2. Set Source to **Deploy from a branch**
3. Set Branch to `main` (or your default), folder to `/ (root)` — or `/docs` if you prefer

   *Alternatively*, use a dedicated `gh-pages` branch and copy only `web-installer/`
   to its root.

4. Save. GitHub will publish the site at `https://<your-username>.github.io/<repo-name>/`

### Publishing the installer page

The installer lives in `web-installer/`. Depending on your Pages root:

- **Root = `/`**: installer is at `https://<user>.github.io/<repo>/web-installer/`
- **Root = `/docs`**: copy `web-installer/` into `docs/web-installer/`

The `manifest.json` uses a relative path (`"path": "firmware-merged.bin"`), so
`firmware-merged.bin` must be in the same directory as `manifest.json`.

### Release workflow (recommended)

1. Build firmware and run merge script (steps above)
2. Commit `web-installer/firmware-merged.bin` to the branch GitHub Pages serves
3. Optionally update the `"version"` field in `manifest.json`
4. Push — GitHub Pages auto-deploys within ~30 seconds

### CORS / security headers

GitHub Pages serves files with permissive CORS headers, so ESP Web Tools works
without any additional configuration.

If self-hosting on a different server, ensure it sends:
```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

---

## Local Testing

```bash
# Python 3 simple server from repo root
python -m http.server 8080
# Then open http://localhost:8080/web-installer/
```

Web Serial API requires a **secure context** (HTTPS or localhost), so the Python
dev server on localhost works fine for testing.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| "Install" button greyed out | Use Chrome or Edge; Firefox does not support Web Serial |
| Device not detected | Try a different USB cable (some are charge-only); install CP2102/CH340 drivers if needed |
| Flash fails mid-way | Hold the BOOT button on the ESP32-S3 while clicking Install |
| Board not entering bootloader | Short IO0 to GND at power-on, or press BOOT+RESET then release RESET |

# A12E Scale Monitor (minimal firmware + licensing)

A **standalone** PlatformIO project for a second ESP32 DevKit board. It does one
job only:

> **Read the weight from the scale/display over RS232 and show it on a web page.**

It has **no relays, no thresholds, no mode switches** — just RS232 in, web page
out. The main `A12E_ESP32` firmware in the parent folder is completely separate
and is **not** modified or used.

This version also includes a **sellable licensing system**:

- **Free trial** (powered-on time; currently **3 hours for customer trials**, 24 h
  in production), then the page locks
- **Signed activation keys** (ECDSA P-256), **bound to one board**
- **Offline activation** — the customer does not need internet
- Keys are generated only with your **private key**, which is never in the `.bin`

---

## Features

- Live weight on a responsive web page (updates every 500 ms)
- Own WiFi Access Point (no router needed)
- Read-only: no outputs are controlled
- Embedded HTML — **no `uploadfs` step**, only firmware upload
- **No external libraries** — Arduino ESP32 core only
- Shows diagnostics: raw frame, frames received, last-update age, uptime
- **Local debug log page** at `/debug` (last 48 lines, RAM only)
- Trial countdown, Device ID, activation box, "Licensed" state
- Works with the same frame format as the main board:
  `W 00004.3 kg` → `4.3 kg`

---

## Hardware wiring

### Recommended: through MAX232 (RS232 levels)

| Signal | ESP32 | MAX232 | Scale DB9 |
|--------|-------|--------|-----------|
| Scale TX → ESP32 RX | **GPIO21** | R1OUT (pin 1) ← RIN1 (pin 3/4) | pin 3 (TxD) |
| ESP32 TX → Scale RX *(optional)* | **GPIO22** | R1IN (pin 2) → NOUT1 (pin 6/7) | pin 2 (RxD) |
| Ground | GND | pin 10 | pin 5 (GND) |
| Power | 5V/VIN + GND from 5 V / 2 A supply | | |

Only **GPIO21 (RX) and GND** are required to read the scale. GPIO22 is wired
the same way as the main board for compatibility, but this firmware never
transmits.

> ⚠️ **Never connect RS232 voltage levels directly to the ESP32.** RS232 is
> ±12 V and will damage the board. Use a MAX232 (or similar) level shifter.

### Direct TTL scale output (no MAX232)

If your scale/display has a **3.3 V TTL** serial output, connect only:

```
scale TX  -> GPIO21
scale GND -> GND
```

---

## Configuration

All settings are in two headers:

### [`include/config.h`](include/config.h) — hardware / network

| Setting | Default | Meaning |
|---------|---------|---------|
| `WIFI_AP_SSID` | `A12E_SCALE_MONITOR` | WiFi AP name |
| `WIFI_AP_PASSWORD` | `33333333` | AP password (min 8 chars, `""` = open) |
| `SCALE_BAUD_RATE` | `9600` | Scale serial baud rate |
| `SCALE_RX_PIN` | `21` | ESP32 RX from scale |
| `SCALE_TX_PIN` | `22` | ESP32 TX (unused for reading) |
| `SCALE_UART_NUM` | `1` | UART1 (USB debug stays on UART0) |
| `SCALE_TIMEOUT_MS` | `3000` | Data older than this = "waiting for scale" |
| `STATUS_UPDATE_INTERVAL` | `500` | Browser polling interval (ms) |

### [`include/license_config.h`](include/license_config.h) — licensing

| Setting | Default | Meaning |
|---------|---------|---------|
| `LICENSE_TRIAL_SECONDS` | `10800` | Trial length = **3 h** of powered-on use (86400 = 24 h) |
| `LICENSE_SAVE_INTERVAL_SEC` | `300` | Trial counter write interval (flash wear) |
| `LICENSE_PUBLIC_KEY_PEM` | (generated) | Public key used to verify activation keys |

---

## Build & upload

Open **this folder** (`esp32_min_web`) in VS Code + PlatformIO, or use the
terminal:

```bash
# Build
pio run -d esp32_min_web

# Upload firmware (no filesystem upload needed)
pio run -d esp32_min_web -t upload

# Serial monitor
pio device monitor -d esp32_min_web
```

The file you distribute to customers is:

```
esp32_min_web/.pio/build/esp32dev/firmware.bin
```

> Ship **only** this `.bin` (or the board pre-flashed). Never ship the source
> code, `tools/license_private.pem`, or a `.pio` build folder containing it.

If `pio` is not on PATH on Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -d esp32_min_web -t upload
```

Expected serial output:

```
========================================
   A12E SCALE MONITOR (minimal)
   Firmware v1.0.0
========================================
[SCALE] UART1  RX=GPIO21  TX=GPIO22  9600 8N1
[WIFI] Starting Access Point...
[WIFI] SSID: A12E_SCALE_MONITOR
[WIFI] Open http://192.168.4.1 on your phone
[LIC] Device ID: A12E-A1B2C3D4E5F6
[LIC] Status: TRIAL, 23h 59m remaining
[WEB] Server started on port 80
[SYSTEM] Ready - waiting for scale frames
[SCALE] 4.3 kg  (frames=12, bad=0)
```

---

## Using it

1. Power the board and the scale.
2. On your phone/PC, connect to WiFi **`A12E_SCALE_MONITOR`** (password
   `33333333`).
3. Open **http://192.168.4.1**.
4. The page shows the live weight, raw serial frame, frame count, uptime, the
   **Device ID**, and the **trial countdown**.

---

## Debug log (`/debug`)

At the bottom of the monitor page there is an **Open debug log** link, or open:

```
http://192.168.4.1/debug
```

It shows the last **48 lines** from the ESP32:

- boot banner and firmware version
- WiFi AP startup
- license / trial state and activation results
- scale frames, including the raw RS232 frame

The same lines are still written to the **USB serial port at 115200 baud**, so
nothing changes for wired debugging.

Details:

- RAM only — a reboot clears the buffer
- Pause / Clear view / Copy, auto-scroll, polls every 1.2 s
- JSON API for other tools: `GET /api/log?since=N`
- Local to the AP only; protected by the WiFi password
- Do not put secret values in log messages

---

## Quick test: 3-hour trial + activation

The firmware is currently built with a **3-hour trial** for customer trials.

> If you already flashed the board before, erase it first so the trial starts at
> 3:00 (the trial counter is kept in NVS):
>
> ```bash
> pio run -d esp32_min_web -t erase
> pio run -d esp32_min_web -t upload
> ```

1. Power the board, connect to WiFi `A12E_SCALE_MONITOR`, open
   `http://192.168.4.1`.
2. Note the **Device ID** (e.g. `A12E-A1B2C3D4E5F6`) and watch the trial
   countdown (`3h 0m` …).
3. Generate a key on your PC (offline, fastest):

   ```bash
   node esp32_min_web/tools/make_license.js A12E-A1B2C3D4E5F6
   ```

   Copy the **third column** (the long key).
4. On the monitor page, paste it into the **License** box and press
   **ACTIVATE**. The badge changes from **Trial** to **Licensed**, and the
   countdown is replaced by **Unlimited**.
5. Reboot the ESP32 (unplug/replug). The page should still say **Licensed** —
   the key is stored in NVS.
6. Optional negative tests:
   - Generate a key for a different Device ID → `Key is for a different device`
   - Change one character of the key → `Invalid key (signature mismatch)`

To test the **online order flow**, use [`../license-server`](../license-server):
create an order with the Device ID, approve it in `/admin`, and copy the key
from the order page. The generated key is identical to the offline one.

To reset the trial for another test, erase and re-upload:

```bash
pio run -d esp32_min_web -t erase
pio run -d esp32_min_web -t upload
```

For production, set the trial back to 24 hours in
`include/license_config.h` and rebuild.

---

## Licensing / selling workflow

### How the protection works

1. Every board shows a unique **Device ID** derived from its WiFi MAC, e.g.
   `A12E-A1B2C3D4E5F6`.
2. On first boot the **trial** starts (3 hours in this customer build; set
   `LICENSE_TRIAL_SECONDS` for production). The trial counter is stored in NVS
   and keeps decreasing only while the board is powered on. It survives normal
   firmware updates.
3. When the trial ends, the page locks (weight hidden) and asks for a key.
4. The customer sends you the **Device ID**.
5. You generate a key for that exact ID with your **private key**:

   ```bash
   node tools/make_license.js A12E-A1B2C3D4E5F6
   ```

   You can generate several at once:

   ```bash
   node tools/make_license.js A12E-A1B2C3D4E5F6 A12E-0F1E2D3C4B5A
   ```

   Output is tab separated:

   ```
   A12E-A1B2C3D4E5F6   A12E1|A12E-A1B2C3D4E5F6|P|1793766231987   <the key>
   ```

   Send the customer only the third column (the key that starts with
   `QTEy...`).
6. The customer pastes the key into the **License** box on the page and taps
   **ACTIVATE**. The key is verified on the ESP32, saved to NVS, and the page
   unlocks immediately (no reboot, no internet).
7. The same key only works on that Device ID.

### Key files

| File | Purpose | Ship it? |
|------|---------|----------|
| `tools/license_private.pem` | Signs keys — **your secret** | **NEVER** |
| `tools/license_public.pem` | Verifies keys (already embedded in firmware) | no need |
| `tools/make_license.js` | Generates customer keys | no |
| `include/license_config.h` | Contains the embedded public key | source only |

### Optional: online / phone-assisted selling

The repo also contains [`../license-server`](../license-server), a free
Cloudflare Worker that automates the same key format:

1. Customer opens `/activate` on their phone and submits the Device ID.
2. They pay (bank transfer / QRIS) and you confirm the order in `/admin`.
3. The order page shows the key; the customer pastes it into the monitor.

This is **Option A** — the ESP32 stays offline and **no firmware change is
required**. Keys issued by the server are identical to `make_license.js` output
and verify against the public key already in this firmware. See
[`../license-server/README.md`](../license-server/README.md) for deployment.

> The server automates paid key issuing. It does not stop a full flash erase
> from resetting the local trial; that needs online trial registration or
> secure-boot hardening.

> **Back up `tools/license_private.pem` now.** It is excluded from git by
> `.gitignore`. If you lose it you cannot issue keys for this firmware; you
> would have to generate a new keypair, rebuild, and re-issue all licenses.

### Changing the trial length

Edit `LICENSE_TRIAL_SECONDS` in `include/license_config.h`, then rebuild:

```c
// Current customer trial: 3 hours
#define LICENSE_TRIAL_SECONDS (3UL * 60UL * 60UL)

// Production: 24 hours
// #define LICENSE_TRIAL_SECONDS (24UL * 60UL * 60UL)

// 7 days
// #define LICENSE_TRIAL_SECONDS (7UL * 24UL * 60UL * 60UL)
```

### Before shipping a board

If you power a board for testing, that time is deducted from the customer trial.
For a clean trial, erase the flash and re-upload before packing:

```bash
pio run -d esp32_min_web -t erase
pio run -d esp32_min_web -t upload
```

`erase` wipes the NVS trial counter; the next boot starts a fresh trial. (This
also erases any activated key, so only do it on units that are not licensed
yet.)

If you sell **pre-activated** units instead, generate a key for each Device ID,
activate it on the page, and ship it — the trial no longer matters.

### Generating a new keypair (rare)

Only if you intentionally start a new product/key series:

```bash
node tools/generate_keypair.js --force
```

Then copy the printed public key into `LICENSE_PUBLIC_KEY_PEM` in
`include/license_config.h` and rebuild. **All previously issued keys stop
working.**

---

## Security notes (important for a commercial product)

This system stops casual copying and key sharing, but it is **not unbreakable**
on an ESP32 without extra production steps.

What it protects well:

- Customers cannot create keys from the `.bin` — the private key is not there.
- A key copied from another customer does not work (bound to Device ID).
- Keys cannot be edited without invalidating the ECDSA signature.
- Normal firmware updates do not reset the trial (NVS is preserved).

Known limits:

- **A full flash erase resets the trial.** The trial state must live on the
  device; someone who runs `esptool erase_flash` or flashes with "erase" gets a
  fresh trial. There is no battery-backed clock or server in AP-only mode.
- **The trial is powered-on time**, not wall-clock time. The ESP32 has no RTC
  battery, so "3 h" (customer build) or "24 h" (production) means actual use. A
  customer who uses it two hours a day gets several calendar days.
- A determined person can patch the firmware's license check out of the `.bin`.

If you need stronger protection:

1. **Enable ESP32 Secure Boot + Flash Encryption** before selling. This makes
   patched firmware refuse to run. It is a one-way eFuse operation — test on
   spare boards first.
2. **Online activation** (device connects to your server once) allows strict
   calendar expiry and server-side revocation. This firmware is offline by
   design; online activation would be a separate version.
3. Never distribute the source or the PlatformIO project, only the `.bin`.

---

## HTTP API

| Endpoint | Description |
|----------|-------------|
| `GET /` | Web page (embedded HTML) |
| `GET /debug` | Debug log page (RAM buffer, local only) |
| `GET /api/status` | JSON status |
| `GET /api/log?since=N` | Recent log lines as JSON |
| `GET /api/activate?key=...` | Activate a key (also accepts `POST key=...`) |

Example `/api/status`:

```json
{
  "weight": 4.3,
  "valid": true,
  "age": 120,
  "frames": 152,
  "badFrames": 0,
  "uptime": 95,
  "raw": "W 00004.3 kg",
  "deviceId": "A12E-A1B2C3D4E5F6",
  "license": "trial",
  "trialRemaining": 82800,
  "trialTotal": 86400,
  "locked": false
}
```

- `license` — `trial`, `licensed`, or `expired`
- `locked` — `true` after the trial ends with no key; weight is withheld
- `trialRemaining` / `trialTotal` — seconds

Activation response:

```json
{"ok":true}
```

or on error:

```json
{"ok":false,"error":"Key is for a different device"}
```

---

## Serial protocol

The parser accepts frames terminated by `\r`, `\n` or `\r\n`, starting with
`W` / `w` / space, and extracts the numeric value (digits, one decimal point,
optional leading `-`). Examples that all work:

```
W 00004.3 kg
w 00004.3kg
W 000123.4 kg
```

Anything that does not contain a number is counted as a bad frame and ignored,
so line noise cannot corrupt the displayed weight.

---

## Troubleshooting

| Symptom | Check |
|---------|-------|
| Page shows `--` and `No data` on serial | RX wiring, TX→GPIO21, common GND |
| Garbage/random frames | Baud rate (`SCALE_BAUD_RATE`), MAX232 wiring, cable |
| Nothing on the AP list | Board powered? Antenna/board OK? Re-upload |
| Weight is 10× / 100× off | Scale's decimal format differs; check the `raw` frame on the page |
| Web page won't open | Connect to the AP first; use `http://192.168.4.1` (not HTTPS) |
| "Key is for a different device" | Generate the key with the exact Device ID shown on that board |
| "Invalid key (signature mismatch)" | Key was mistyped/truncated, or made with a different keypair |
| Activation works but returns after reboot | NVS write failed (flash problem) — check serial logs |
| Trial reset unexpectedly | The board was fully flash-erased (expected limitation) |

---

## Project layout

```
esp32_min_web/
├── platformio.ini            # standalone build config (no lib_deps)
├── include/
│   ├── config.h              # WiFi AP, pins, baud rate, timings
│   └── license_config.h      # trial length + embedded public key
├── src/
│   ├── main.cpp              # RS232 parser + embedded web UI + HTTP server
│   ├── debug_page.h          # /debug page HTML
│   ├── log.h
│   ├── log.cpp               # in-RAM log ring buffer + Serial tee
│   ├── license.h
│   └── license.cpp           # trial counter, NVS, ECDSA verification
├── tools/
│   ├── make_license.js       # generate customer activation keys
│   ├── generate_keypair.js   # create a new keypair (rare)
│   ├── license_private.pem   # SECRET - gitignored, never ship
│   └── license_public.pem    # public half (also embedded in firmware)
├── .gitignore                # keeps the private key out of git
└── README.md
```

No `data/` folder and no `uploadfs`: the HTML lives inside `main.cpp` in
`PROGMEM`.

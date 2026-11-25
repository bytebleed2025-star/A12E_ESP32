# A12E Scale Monitor (minimal firmware + licensing)

A **standalone** PlatformIO project for a second ESP32 DevKit board. It does one
job only:

> **Read the weight from the scale/display over RS232 and show it on a web page.**

It has **no relays, no thresholds, no mode switches** â€” just RS232 in, web page
out. The main `A12E_ESP32` firmware in the parent folder is completely separate
and is **not** modified or used.

> **Current firmware:** a silent **6-hour local trial** stored in NVS. There is
> no trial token, no “Claim trial” button, and no `/trial` server page. When the
> 6 hours end, the page locks and shows **TRIAL ENDED** until a key is entered.
> Older sections below that describe server trial tokens are legacy.

This version also includes a **sellable licensing system**:

- **Silent trial** (6 hours of powered-on time, starts automatically), then the
  page locks and shows **TRIAL ENDED**
- **Signed activation keys** (ECDSA P-256), **bound to one board**
- **Offline activation** â€” the customer does not need internet
- Keys are generated only with your **private key**, which is never in the `.bin`

---

## Features

- Live weight on a responsive web page (updates every 500 ms)
- **Modern 3-tab UI: Monitor / Debug / Config**
- Own WiFi Access Point + optional **STA** connection to your router (2.4 GHz)
- **Google Sheets logging** every configurable interval
- **Config tab**: AP settings, router WiFi scan, Google Sheets URL/token
- Read-only: no outputs are controlled
- Embedded HTML â€” **no `uploadfs` step**, only firmware upload
- **No external libraries** â€” Arduino ESP32 core only
- Shows diagnostics: raw frame, frames received, last-update age, uptime
- **Local debug log page** at `/debug` (last 48 lines, RAM only)
- Trial countdown, Device ID, activation box, "Licensed" state
- Works with the same frame format as the main board:
  `W 00004.3 kg` â†’ `4.3 kg`

---

## Hardware wiring

### Recommended: through MAX232 (RS232 levels)

| Signal | ESP32 | MAX232 | Scale DB9 |
|--------|-------|--------|-----------|
| Scale TX â†’ ESP32 RX | **GPIO21** | R1OUT (pin 1) â† RIN1 (pin 3/4) | pin 3 (TxD) |
| ESP32 TX â†’ Scale RX *(optional)* | **GPIO22** | R1IN (pin 2) â†’ NOUT1 (pin 6/7) | pin 2 (RxD) |
| Ground | GND | pin 10 | pin 5 (GND) |
| Power | 5V/VIN + GND from 5 V / 2 A supply | | |

Only **GPIO21 (RX) and GND** are required to read the scale. GPIO22 is wired
the same way as the main board for compatibility, but this firmware never
transmits.

> âš ï¸ **Never connect RS232 voltage levels directly to the ESP32.** RS232 is
> Â±12 V and will damage the board. Use a MAX232 (or similar) level shifter.

### Direct TTL scale output (no MAX232)

If your scale/display has a **3.3 V TTL** serial output, connect only:

```
scale TX  -> GPIO21
scale GND -> GND
```

---

## Configuration

All settings are in two headers:

### [`include/config.h`](include/config.h) â€” hardware / network

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
| `SHEET_UPLOAD_INTERVAL_MS` | `60000` | Default Google Sheets upload interval |
| `SHEET_HTTP_TIMEOUT_MS` | `10000` | HTTPS timeout |
| `SHEET_TIMEZONE` | `WIB-7` | POSIX timezone for NTP timestamps |

### [`include/license_config.h`](include/license_config.h) â€” licensing

| Setting | Default | Meaning |
|---------|---------|---------|
| `LICENSE_TRIAL_SECONDS` | `21600` | Trial length = **6 h** of powered-on use (86400 = 24 h) |
| `LICENSE_SAVE_INTERVAL_SEC` | `15` | Trial counter write interval (flash wear vs. power-loss accuracy) |
| `LICENSE_PUBLIC_KEY_PEM` | (generated) | Public key used to verify activation keys |

Runtime settings (AP SSID/password, router WiFi, Google Sheets URL/token,
upload interval) are edited in the **Config** tab and saved to NVS.
`sta_secrets.h` is only used as initial factory defaults on a fresh board; it is
gitignored. Copy `sta_secrets.example.h` to `sta_secrets.h` if you want to bake
in defaults, or leave the placeholders and configure everything from the page.

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
   Firmware v1.2.0
========================================
[SCALE] UART1  RX=GPIO21  TX=GPIO22  9600 8N1
[CFG] AP SSID: A12E_SCALE_MONITOR
[CFG] STA SSID: (not configured)
[CFG] Google Sheets: not configured
[LIC] Device ID: A12E-A1B2C3D4E5F6
[LIC] Status: TRIAL, 2h 59m remaining
[NET] AP IP: 192.168.4.1
[NET] Router WiFi not configured - AP-only mode
[SHEET] Not configured - open http://192.168.4.1/config
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
   **Device ID**, the **trial countdown**, and the cloud-upload status.
5. Use the top tabs: **Monitor**, **Debug**, **Config**.

---

## Config tab (`/config`)

Open **http://192.168.4.1/config** (or tap **Config**).

| Section | What you can change |
|---|---|
| Access Point | AP SSID and password |
| Router WiFi (STA) | Router SSID/password, with **Scan** button |
| Google Sheets | Apps Script URL, shared token, upload interval |
| Device status | AP IP, STA status/IP, RSSI, sheet configured, interval |

Important behaviour:

- **AP settings** (SSID/password) require a reboot. The page saves and reboots
  in about 2 seconds; reconnect to the new AP and open `192.168.4.1` again.
- **Router WiFi** (STA) changes apply live; the AP stays active.
- **Google Sheets** settings apply immediately; use **Send test row** to verify.
- Settings are stored in NVS and survive reboots and normal firmware uploads.
  They are erased by a full flash erase.
- AP clients **do not** get internet through the ESP32. The ESP32 itself has
  internet via STA and uploads data; the phone keeps using mobile data for the
  activation website.
- ESP32 WiFi is **2.4 GHz only** (not 5 GHz).

Passwords and the Sheets token are never returned by the API; the Config page
shows â€œleave blank to keep currentâ€.

---

## Google Sheets upload (AP+STA)

The board can connect to your router while keeping its own AP active:

```
Scale -> ESP32 -> (STA) -> Router -> Internet -> Google Apps Script -> Sheet
                    |
                    +-> (AP) -> phone: http://192.168.4.1
```

### One-time Google setup

See [`../google-sheets/README.md`](../google-sheets/README.md). Short version:

1. Create a Google Sheet.
2. Extensions â†’ Apps Script â†’ paste [`Code.gs`](../google-sheets/Code.gs).
3. Set a long random `TOKEN`.
4. Deploy as **Web app** â†’ execute as **Me** â†’ access **Anyone**.
5. Copy the `/exec` URL.

### On the monitor

1. Open **Config**.
2. Under **Router WiFi**, press **Scan**, pick your 2.4 GHz network, enter the
   password.
3. Paste the Apps Script `/exec` URL and the same token.
4. Set the upload interval (seconds), then **Save & apply**.
5. Press **Send test row** â€” a row should appear in the sheet.

The board uploads one row per interval **only when**:

- the scale has a fresh valid weight, and
- the license is not locked (trial running or licensed).

After the trial expires, cloud uploads pause until activation. The Config page
and test row still work.

Sheet columns: **Received At, Device ID, Weight (kg), License, RSSI, Uptime (s),
Device Time, Test**.

> TLS note: this version uses `setInsecure()` for the Google HTTPS connection.
> For a production fleet, embed the Google Trust Services root CA and enable
> certificate validation.

---

## Debug log (`/debug`)

Open the **Debug** tab, or:

```
http://192.168.4.1/debug
```

It shows the last **48 lines** from the ESP32:

- boot banner and firmware version
- WiFi AP startup
- router WiFi / STA connect and disconnect
- license / trial state and activation results
- Google Sheets test/upload results
- scale frames, including the raw RS232 frame

The same lines are still written to the **USB serial port at 115200 baud**, so
nothing changes for wired debugging.

Details:

- RAM only â€” a reboot clears the buffer
- Pause / Clear view / Copy, auto-scroll, polls every 1.2 s
- JSON API for other tools: `GET /api/log?since=N`
- Local to the AP only; protected by the WiFi password
- Do not put secret values in log messages

---

## Quick test: 6-hour trial + activation

The firmware starts a **silent 6-hour trial automatically** on first boot. There
is no trial token to claim and no trial countdown in the UI. After 6 hours of
powered-on time the page locks and shows **TRIAL ENDED** until a permanent key
is entered.

1. Power the board, connect to WiFi `A12E_SCALE_MONITOR`, open
   `http://192.168.4.1`.
2. Note the **Device ID** (e.g. `A12E-A1B2C3D4E5F6`).
3. Use the monitor normally for the trial period.
4. For a permanent key, generate one offline:

   ```bash
   node esp32_min_web/tools/make_license.js A12E-A1B2C3D4E5F6
   ```

   Copy the **third column** (the long key). Or use the online order page at
   <https://embedded-devlicense.esp32dev-by-shd.workers.dev/a12escales>.
5. On the monitor page, paste the key into **Activation key** and press
   **Activate**. The badge changes to **Licensed**.
6. Reboot the ESP32. The page should still say **Licensed** — the key is stored
   in NVS.
7. Optional negative tests:
   - Generate a key for a different Device ID → `Key is for a different device`
   - Change one character of the key → `Invalid key (signature mismatch)`

The trial is local (NVS). A full flash erase resets it. The server no longer
issues trial tokens; only paid, device-bound activation keys.

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
| `tools/license_private.pem` | Signs keys â€” **your secret** | **NEVER** |
| `tools/license_public.pem` | Verifies keys (already embedded in firmware) | no need |
| `tools/make_license.js` | Generates customer keys | no |
| `include/license_config.h` | Contains the embedded public key | source only |

### Optional: online / phone-assisted selling

The repo also contains [`../license-server`](../license-server), a free
Cloudflare Worker that automates the same key format:

1. Customer opens `/activate` on their phone and submits the Device ID.
2. They pay (bank transfer / QRIS) and you confirm the order in `/admin`.
3. The order page shows the key; the customer pastes it into the monitor.

This is **Option A** â€” the ESP32 stays offline and **no firmware change is
required**. Keys issued by the server are identical to `make_license.js` output
and verify against the public key already in this firmware. See
[`../license-server/README.md`](../license-server/README.md) for deployment.

> The server also issues the one-time trial token, so a full flash erase no
> longer creates a new trial. A saved token can still be replayed after an NVS
> erase; eFuse marking or an online lease is the next hardening step.

> **Back up `tools/license_private.pem` now.** It is excluded from git by
> `.gitignore`. If you lose it you cannot issue keys for this firmware; you
> would have to generate a new keypair, rebuild, and re-issue all licenses.

### Changing the trial length

The claimed trial length comes from the **server** (`TRIAL_SECONDS` in
`../license-server/wrangler.toml`) and is inside the signed token. Change it
there and redeploy; existing tokens keep their original length.

`LICENSE_TRIAL_SECONDS` in `include/license_config.h` is only the firmware
fallback/display value used before a token is claimed:

```c
// Fallback display value
#define LICENSE_TRIAL_SECONDS (3UL * 60UL * 60UL)
```

### Before shipping a board

The customer claims the 3-hour trial once from the server. Do **not** erase the
flash expecting a fresh trial: erasing NVS deletes the token, and the server
refuses to issue another claim for the same Device ID.

- Ship the unit **unclaimed** and let the customer claim it, or
- generate a permanent key for the Device ID, activate it, and ship it
  **pre-activated**.
- To retest a trial, use another Device ID or delete the row from the server's
  `trials` table.

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

- Customers cannot create keys from the `.bin` â€” the private key is not there.
- A key copied from another customer does not work (bound to Device ID).
- Keys cannot be edited without invalidating the ECDSA signature.
- Normal firmware updates do not reset the trial (NVS is preserved).

Web/Config security:

- The **Config tab and its APIs only accept clients on the device AP**
  (`192.168.4.x`). Clients on the router LAN get `403 Forbidden`.
- The AP password is the only protection for the Config tab. Use a strong AP
  password, especially in shared locations.
- WiFi passwords and the Google Sheets token are stored in NVS. A full flash
  erase removes them; normal firmware updates keep them.
- Debug logs never include passwords, tokens, or activation keys.

Known limits:

- **A full flash erase does not create a new trial.** The server issues the
  token once per Device ID. Erasing NVS loses the token, but the server refuses
  a second claim. A saved token can still be replayed after an NVS erase; an
  eFuse marker or online lease is the next hardening step.
- **The trial is powered-on time**, not wall-clock time. The ESP32 has no RTC
  battery, so "3 h" (customer build) or "24 h" (production) means actual use. A
  customer who uses it two hours a day gets several calendar days.
- A determined person can patch the firmware's license check out of the `.bin`.

If you need stronger protection:

1. **Enable ESP32 Secure Boot + Flash Encryption** before selling. This makes
   patched firmware refuse to run. It is a one-way eFuse operation â€” test on
   spare boards first.
2. **Online activation** (device connects to your server once) allows strict
   calendar expiry and server-side revocation. This firmware is offline by
   design; online activation would be a separate version.
3. Never distribute the source or the PlatformIO project, only the `.bin`.

---

## HTTP API

| Endpoint | Description |
|----------|-------------|
| `GET /` | Monitor page |
| `GET /debug` | Debug log page (RAM buffer, local only) |
| `GET /config` | Config page (AP / STA / Google Sheets) |
| `GET /api/status` | JSON status |
| `GET /api/log?since=N` | Recent log lines as JSON |
| `GET /api/activate?key=...` | Activate a key (also accepts `POST key=...`) |
| `GET /api/settings` | Current settings + STA status (secrets omitted) |
| `POST /api/settings` | Save AP / STA / Sheets settings |
| `POST /api/wifi/scan` | Scan nearby WiFi networks |
| `POST /api/sheets/test` | Send a test row to Google Sheets |

`POST /api/settings`, `/api/wifi/scan`, and `/api/sheets/test` require the
header `X-Requested-With: A12E` (the Config page sends it automatically).

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
  "trialRemaining": 10000,
  "trialTotal": 10800,
  "locked": false,
  "staConnected": true,
  "staConfigured": true,
  "staStatus": "connected",
  "staIp": "192.168.1.42",
  "rssi": -58,
  "cloudConfigured": true,
  "lastUploadAge": 12000,
  "lastUploadOk": true,
  "uploadOk": 12,
  "uploadFail": 0
}
```

- `license` â€” `trial`, `licensed`, or `expired`
- `locked` â€” `true` after the trial ends with no key; weight is withheld
- `trialRemaining` / `trialTotal` â€” seconds
- `staStatus` â€” `connected`, `connecting`, `offline`, or `not configured`
- `lastUploadAge` â€” ms since the last Sheets attempt, or `-1` if never

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
| Page shows `--` and `No data` on serial | RX wiring, TXâ†’GPIO21, common GND |
| Garbage/random frames | Baud rate (`SCALE_BAUD_RATE`), MAX232 wiring, cable |
| Nothing on the AP list | Board powered? Antenna/board OK? Re-upload |
| Weight is 10Ã— / 100Ã— off | Scale's decimal format differs; check the `raw` frame on the page |
| Web page won't open | Connect to the AP first; use `http://192.168.4.1` (not HTTPS) |
| "Key is for a different device" | Generate the key with the exact Device ID shown on that board |
| "Invalid key (signature mismatch)" | Key was mistyped/truncated, or made with a different keypair |
| Activation works but returns after reboot | NVS write failed (flash problem) â€” check serial logs |
| Trial already claimed | Erasing flash does not reset the server claim; use another Device ID or contact support |

---

## Project layout

```
esp32_min_web/
â”œâ”€â”€ platformio.ini            # standalone build config (no lib_deps)
â”œâ”€â”€ include/
â”‚   â”œâ”€â”€ config.h              # AP defaults, pins, timing, cloud interval
â”‚   â”œâ”€â”€ license_config.h      # trial length + embedded public key
â”‚   â””â”€â”€ sta_secrets.example.h # optional factory defaults
â”œâ”€â”€ src/
â”‚   â”œâ”€â”€ main.cpp              # RS232 parser + HTTP routes
â”‚   â”œâ”€â”€ settings.h / .cpp     # NVS settings: AP, STA, Sheets
â”‚   â”œâ”€â”€ net.h / .cpp          # AP+STA WiFi, scan, NTP
â”‚   â”œâ”€â”€ sheets.h / .cpp       # Google Sheets uploader
â”‚   â”œâ”€â”€ license.h / .cpp      # trial, NVS, ECDSA verification
â”‚   â”œâ”€â”€ log.h / .cpp          # RAM log ring buffer + Serial tee
â”‚   â”œâ”€â”€ web_ui.h              # shared modern CSS + page shell
â”‚   â”œâ”€â”€ monitor_page.h        # Monitor tab
â”‚   â”œâ”€â”€ debug_page.h          # Debug tab
â”‚   â””â”€â”€ config_page.h         # Config tab
â”œâ”€â”€ tools/
â”‚   â”œâ”€â”€ make_license.js       # generate customer activation keys
â”‚   â”œâ”€â”€ generate_keypair.js   # create a new keypair (rare)
â”‚   â”œâ”€â”€ license_private.pem   # SECRET - gitignored, never ship
â”‚   â””â”€â”€ license_public.pem    # public half (also embedded in firmware)
â”œâ”€â”€ .gitignore                # keeps secrets and build files out of git
â””â”€â”€ README.md
```

Related folders in the repository:

- `../license-server` â€” Cloudflare Worker + D1 activation site
- `../google-sheets` â€” Apps Script logger

No `data/` folder and no `uploadfs`: the web UI is compiled into the
firmware. Settings live in NVS and are edited from the Config tab.

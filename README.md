# ESP32 Scale Control System

Complete IoT weighing system with web-based control interface for ESP32.

## Project Structure

```
A12E_ESP32/
├── include/
│   └── secrets.h           # Configuration (WiFi, pins, thresholds)
├── src/
│   └── main.cpp            # Main firmware
├── data/
│   └── index.html          # Web UI (served from LittleFS)
├── docs/
│   └── schematic.svg       # Full wiring schematic (open in any browser)
├── platformio.ini          # PlatformIO configuration
├── .gitignore              # Git ignore file
└── README.md              # This file
```

## Hardware Schematic

![Wiring schematic](docs/schematic.svg)

Full wiring diagram: [`docs/schematic.svg`](docs/schematic.svg) — open it in any
browser, or drop it straight into an Inkscape / Illustrator / KiCad canvas.
It covers the ESP32 pin map, relay modules, mode push buttons, the MAX232
RS232 level shifter, the DB9 scale port and the 5 V power rails.

## Features

✅ **WiFi Access Point (AP)** - No router needed  
✅ **Async Web Server** - Responsive, non-blocking  
✅ **LittleFS Storage** - Store web UI on ESP32  
✅ **Real-time Updates** - 1-second refresh rate  
✅ **Auto Relay Control** - Automatic activation based on weight  
✅ **Manual Relay Control** - Override via web interface  
✅ **Physical Mode Switch** - Manual / Auto selector and cycle-start button  
✅ **Config Persistence** - Thresholds and mode saved to LittleFS  
✅ **RS232 Serial Communication** - Weight reading from scale  
✅ **Responsive UI** - Works on mobile and desktop  
✅ **Error Handling** - Connection monitoring  
✅ **Debug Logging** - Detailed serial output  

## Operating Modes

The mode is set by **SW1 (GPIO13)**, not by the web UI. On boot the physical
switch position wins over the value saved in `config.json`.

| SW1 (GPIO13) | Mode | Relay control |
|--------------|------|---------------|
| LOW (pressed) | **Manual** | Web UI ON/OFF buttons (`/api/relay`) |
| HIGH (released) | **Auto** | Weight thresholds from the scale |

In **Auto** mode, pressing **SW2 (GPIO14)** starts a cycle: both relays turn ON
and stay ON until the scale reports a weight at or above the matching
threshold, which switches that relay off again. Switching modes always forces
both relays OFF.

## Hardware Requirements

- **ESP32 DevKit v1** (or similar)
- **2x 5 V low-level-trigger relay module** (IN: GPIO16, GPIO17)
  - Must be the *low-level trigger* type so a 3.3 V HIGH energises it
  - Contacts are dry (COM / NO / NC) and switch the load side, not the logic
- **2x push button** — both to GND, active LOW, using the ESP32 internal pull-up
  - SW1 — latching / toggle: mode select (GPIO13)
  - SW2 — momentary: start auto cycle (GPIO14)
- **MAX232** RS232 transceiver, DIP-16, + 6x 0.1 µF ceramic capacitors
  - ESP32 RX (GPIO21) ← pin 1 (R1OUT)
  - ESP32 TX (GPIO22) → pin 2 (R1IN)
  - pins 6/7 (NOUT1) → DB9 pin 2 (scale RxD)
  - pins 3/4 (RIN1) → DB9 pin 3 (scale TxD)
  - pin 10 → GND
- **RS232 serial scale** with a DB9 female port
- **DB9 female connector** + shielded twisted-pair cable
- **5 V / 2 A DC supply** — feed the ESP32 `5V/VIN` pin from this, not from USB
  - Powers the ESP32, both relay modules and the MAX232
  - All grounds common; never tie GND to AC neutral
- **USB cable** — flashing and serial monitor only (do not power simultaneously)

See [`docs/schematic.svg`](docs/schematic.svg) for the full wiring diagram.

## Setup Instructions

### 1. Install Dependencies

**VSCode + PlatformIO Extension:**
```bash
# Extensions required:
- PlatformIO IDE
```

### 2. Configure Hardware (Edit `src/secrets.h`)

`src/secrets.h` is the file the firmware actually reads — `main.cpp` includes
`"secrets.h"` and the compiler resolves it from `src/` first. The copy in
`include/` is unused.

```cpp
// WiFi AP Settings
#define WIFI_SSID "ESP32_Scale"
#define WIFI_PASSWORD "12345678"

// Pin Configuration
#define RELAY_PIN_1 16          // GPIO16 -> Relay 1 IN
#define RELAY_PIN_2 17          // GPIO17 -> Relay 2 IN
#define MANUAL_MODE_PIN 13      // GPIO13 -> SW1, active LOW (HIGH = Auto)
#define AUTO_MODE_PIN 14        // GPIO14 -> SW2, active LOW, momentary
#define RS232_RX_PIN 21         // GPIO21 -> MAX232 pin 1 (R1OUT)
#define RS232_TX_PIN 22         // GPIO22 -> MAX232 pin 2 (R1IN)

// Weight Thresholds (kg)
#define WEIGHT_THRESHOLD_RELAY1 100.0
#define WEIGHT_THRESHOLD_RELAY2 200.0
```

Thresholds and mode are persisted to `/config.json` in LittleFS at runtime and
can be changed from the web UI, so editing these values only sets the
power-on defaults for a freshly erased filesystem.

### 3. Build and Upload

**Terminal in VSCode:**

```bash
# Upload filesystem (HTML to LittleFS)
pio run --target uploadfs

# Upload firmware
pio run --target upload

# Monitor serial output
pio device monitor
```

### 4. Connect from Phone

1. **WiFi Connection:**
   - Search for: `ESP32_Scale`
   - Password: `12345678`

2. **Open Browser:**
   - Navigate to: `http://192.168.4.1`
   - You should see the Scale Control UI

## API Endpoints

All endpoints are `GET`, unauthenticated, and only reachable from the AP
network. Every one of them returns the same status JSON as the body.

### GET `/api/status`
Returns current system state:
```json
{
  "weight": 75.3,
  "relay1": false,
  "relay2": false,
  "relay1_threshold": 100.0,
  "relay2_threshold": 200.0,
  "systemMode": false
}
```

`systemMode` is `true` for Manual, `false` for Auto. Note that on boot this
reflects the **SW1 switch position**, which overrides the persisted value.

### GET `/api/relay?relay=1&state=1`
Control relay:
- `relay`: 1 or 2
- `state`: 0 (OFF) or 1 (ON)

⚠ Rejected while the system is in Auto mode — the status is returned unchanged
and the request is ignored. Switch SW1 to Manual first.

### GET `/api/threshold?relay=1&value=25.5`
Set a weight threshold in kg:
- `relay`: 1 or 2
- `value`: threshold in kg (float)

Persisted to `/config.json` immediately. Works in either mode.

### GET `/api/mode?mode=auto`
Switch mode from the web UI:
- `mode`: `manual` or `auto`

Turns both relays OFF and clears the auto cycle, then persists to
`config.json`. **SW1 remains authoritative at the next boot** — use the physical
switch to select a mode you want to stick.

## Serial Output Example

```
========================================
    ESP32 SCALE CONTROL SYSTEM v1.0
========================================
Chip Model: ESP32
Chip Revision: 3
Flash Size: 4 MB
Free Heap: 180 KB
========================================

[RELAYS] Initializing relay pins...
[RELAYS] Relay 1 (GPIO16) - OFF
[RELAYS] Relay 2 (GPIO17) - OFF
[FS] Mounting LittleFS...
[FS] LittleFS mounted successfully
[CONFIG] Loaded thresholds: Relay1=20.0kg, Relay2=200.0kg
[CONFIG] System mode: Auto
[FS] Files in LittleFS:
     - index.html (8945 bytes)
[MODE] Initializing mode input pins...
[MODE] Manual PB (GPIO13) state: OFF (HIGH)
[MODE] Auto PB (GPIO14) state: OFF (HIGH)
[MODE] Mode from GPIO13: Auto
[RS232] Initializing RS232 communication...
[RS232] Baud Rate: 9600
[RS232] RX Pin: GPIO21
[RS232] TX Pin: GPIO22
[WiFi] Setting up Access Point...
[WiFi] Access Point started successfully!
[WiFi] SSID: A12E_WEIGH SCALE_DEV2
[WiFi] Password: ********
[WiFi] IP Address: 192.168.4.1
[WiFi] Connect your phone to WiFi and open http://192.168.4.1
[WEB] Initializing web server...
[WEB] Web server started on port 80
[SYSTEM] Setup complete - System ready!

[SCALE] Weight: 75.3 kg
[MODE] Auto cycle START (GPIO14)
[RELAYS] Relay 1 auto STOP (weight 240kg >= 20kg)
[RELAYS] Relay 2 auto STOP (weight 240kg >= 200kg)
[MODE] Manual Mode (GPIO13)
[RELAYS] Both relays OFF (mode switch)
[CONFIG] Saved config
[WEB] Relay control request - Relay: 1, State: 1
[RELAYS] Relay 1 turned ON
```

## RS232 Frame Format

Expected format from scale:
```
W n 0 0 0 0 0 4 . 3 k g
0 1 2 3 4 5 6 7 8 9 10 11
```

- Position 0: 'W' or 'w' (frame identifier)
- Positions 2-7: Weight digits (spaces skipped)
- Position 9: Decimal digit
- Ends with: `\r\n`

⚠️ **Unverified:** the frame comment above documents positions `0`–`11`
(12 characters), but `readRS232Data()` in `src/main.cpp` only calls
`processFrame()` when `bufferIndex == 11` at the `\r`, which corresponds to
**11** characters received. The comment and the check disagree. Confirm the
real frame against your scale and, if they differ, fix `bufferIndex == 11` in
`src/main.cpp` before relying on this section.

## Troubleshooting

### Can't find ESP32 serial port
```bash
pio device list
```

### LittleFS not uploading
```bash
# Clean and rebuild
pio run --target cleanall
pio run --target uploadfs
pio run --target upload
```

### WiFi not starting
- Check that AP credentials are set in `secrets.h`
- Ensure WiFi module is properly connected

### Web UI not loading
- Verify `index.html` is in `data/` folder
- Run `pio run --target uploadfs` again
- Check serial output for LittleFS mounting errors

### Relays not activating
- Check GPIO pins (16, 17) are correctly wired
- **Confirm the module is LOW-level trigger.** A high-level-trigger module
  energises at 0 V, so it will be permanently ON with a 3.3 V GPIO and will
  never switch. Swap the module or add an NPN inverter stage.
- Check weight thresholds — but note they are persisted to `/config.json` and
  the web UI overwrites the values in `secrets.h`
- Relay coils need their own 5 V supply; don't try to power them from a GPIO

### Web UI relay buttons do nothing
- The API is rejected in Auto mode. Check the `[MODE]` line in the serial
  monitor and set SW1 (GPIO13) to Manual (pressed / LOW).
- SW1 overrides the web-selected mode on every boot, so a mode set via
  `/api/mode` will not survive a restart.

### Mode switch seems stuck or flips on its own
- SW1/SW2 bounce for a few milliseconds. Firmware debounces at 50 ms
  (`debounceDelay` in `src/main.cpp`).
- Check for a loose or noisy switch — an unbonded button on a long lead will
  read phantom LOWs and toggle the mode continuously.

### Weight always reads 0
- Confirm the MAX232 is fitted with all 6 charge-pump capacitors
- RS232 is inverted and level-shifted — a direct TTL connection will not work
- Verify the DB9 pinout: scale TxD (pin 3) → MAX232 pin 3/4, scale RxD
  (pin 2) → MAX232 pin 6/7
- Check the baud rate matches `SERIAL_BAUD_RATE` (9600) and that the frame
  length matches what `readRS232Data()` accepts — see the note in
  [RS232 Frame Format](#rs232-frame-format)

## Web UI Features

- **Real-time Weight Display** - Updates every second
- **Relay Status Indicators** - Visual feedback (green=ON, red=OFF)
- **Manual Control Buttons** - Active only in Manual mode (SW1 pressed)
- **Threshold Display / Editing** - Shows and updates activation thresholds
- **Mode Indicator** - Shows Manual or Auto as set by SW1
- **Connection Status** - Shows connection state
- **Responsive Design** - Optimized for mobile and desktop
- **Error Handling** - Displays connection errors

## Libraries Used

| Library | Version | Purpose |
|---------|---------|----------|
| ESPAsyncWebServer | 1.2.3 | Non-blocking web server |
| AsyncTCP | 1.1.1 | TCP support for async server |
| ArduinoJson | 6.21.2 | JSON serialization |
| LittleFS_MBED | Latest | Flash filesystem |

## Power Consumption

- Idle: ~50-100 mA
- Active (WiFi + Server): ~150-200 mA
- With relays activated: ~200-300 mA

## Performance

- **Response Time:** <100ms
- **Update Frequency:** 1Hz (1 second)
- **Concurrent Connections:** Up to 10
- **Memory Usage:** ~60KB Flash (firmware) + 8KB (web UI)

## Security Notes

⚠️ **Important:**
- `src/secrets.h` and `include/secrets.h` contain the WiFi AP credentials in
  plain text
- ⚠️ **Both files are already tracked in git**, so the `secrets.h` rule in
  `.gitignore` does not protect them — it only affects untracked files. The
  credentials are in the repository history and must be treated as public.
- To fix properly: rotate the AP password, then either commit a
  `secrets.example.h` and untrack the real file
  (`git rm --cached src/secrets.h`), or move the values to environment
  variables. Rewriting history is the only way to purge the old values.
- Change the default WiFi password in production
- No authentication on the web API — anyone on the AP can drive the relays.
  The AP password is the only protection.

## Future Enhancements

- [ ] SPIFFS support
- [ ] OTA (Over-The-Air) updates
- [ ] Data logging to SD card
- [ ] MQTT integration
- [ ] Advanced analytics
- [ ] Email notifications
- [ ] Mobile app

## Author

ESP32 Scale Control System v1.0

## License

MIT License - Feel free to use and modify

---

**Questions or Issues?** Check the troubleshooting section or open an issue on GitHub.

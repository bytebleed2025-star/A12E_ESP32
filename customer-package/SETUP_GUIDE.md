# A12E Scale Monitor — Customer Setup Guide

**Firmware 1.2.0 · 3-hour trial · Optional Google Sheets logging**

Thank you for trying the A12E Scale Monitor. This guide shows how to flash the
firmware, connect your scale, use the local web page, and optionally upload
readings to Google Sheets.

---

## 1. What you received

| File | Purpose |
|---|---|
| `A12E_SCALE_MONITOR_3H_TRIAL_CLOUD_FACTORY.bin` | Complete ESP32 firmware image |
| `SETUP_GUIDE.html` / `SETUP_GUIDE.md` | This guide |
| `GoogleSheets_Code.gs` | Optional Google Apps Script for cloud logging |

SHA256 of the `.bin`:

```
F16F2A7CA1F4F48DCEDCF50F54B18F92CC87729D390D5FAA9D6269420C13125C
```

> This is a **merged factory image**. Do not use the small `firmware.bin` from a
> PlatformIO build folder.

---

## 2. What you need

- ESP32 DevKit v1 (or compatible ESP32 board)
- USB **data** cable (not a charge-only cable)
- Windows PC (or macOS/Linux with Python + esptool)
- USB-serial driver for your board:
  - CP210x (Silicon Labs)
  - CH340/CH341 (WCH)
  - FTDI
- 5 V power supply for the board
- MAX232 (or equivalent RS232 level shifter) for the scale
- Optional: 2.4 GHz WiFi and a Google account for Google Sheets

---

## 3. Flash the firmware

> **Important:** erase the flash first. This resets the 3-hour trial to a clean
> start and removes any old settings.

### Option A — ESP Flash Download Tool (Windows)

1. Download and open **ESP Flash Download Tool** from Espressif.
2. Select **ESP32** and **Developer Mode**.
3. Set:

   | Setting | Value |
   |---|---|
   | Flash Mode | **DIO** |
   | Flash Frequency | **40 MHz** |
   | Flash Size | **4 MB** |
   | Erase Flash | **Erase All** |

4. Click **Add** and choose `A12E_SCALE_MONITOR_3H_TRIAL_CLOUD_FACTORY.bin`.
5. Set the offset to **`0x0`**.
6. Select the correct **COM port**.
7. Click **START** and wait for **FINISH**.
8. Press the **EN/RST** button on the board once.

### Option B — esptool (Windows/macOS/Linux)

Install Python and esptool, then:

```bash
# Replace COM3 with your port (Windows) or /dev/ttyUSB0 (Linux/macOS)
esptool.py --chip esp32 --port COM3 erase_flash
esptool.py --chip esp32 --port COM3 --baud 921600 write_flash 0x0 A12E_SCALE_MONITOR_3H_TRIAL_CLOUD_FACTORY.bin
```

If the board is not detected:

1. Hold **BOOT/IO0**.
2. Press and release **EN/RST**.
3. Release **BOOT**.
4. Run the command again.

If upload fails at 921600, try `--baud 115200`.

> The board must be powered and the USB cable must support data, not just
> charging. If no COM port appears, install the USB-serial driver.

---

## 4. Connect the scale

Connect the scale/display RS232 output through a **MAX232** level shifter:

| Signal | ESP32 | MAX232 | Scale DB9 |
|---|---|---|---|
| Scale TX → ESP32 RX | **GPIO21** | R1OUT ← RIN1 | pin 3 (TxD) |
| ESP32 TX *(optional)* | GPIO22 | R1IN → NOUT1 | pin 2 (RxD) |
| Ground | GND | GND | pin 5 (GND) |

Serial format: **9600 baud, 8N1**.

> ⚠️ **Never connect RS232 voltage levels directly to the ESP32.** RS232 can be
> ±12 V and will damage the board. Use a MAX232 (or equivalent).

Power the board from a stable **5 V** supply.

---

## 5. Open the monitor page

1. On your phone or PC, connect to WiFi:

   - **SSID:** `A12E_SCALE_MONITOR`
   - **Password:** `33333333`

2. Open:

   ```
   http://192.168.4.1
   ```

3. You should see:

   - live weight
   - **Device ID** (e.g. `A12E-A1B2C3D4E5F6`)
   - trial countdown (starts at about **3h 00m**)
   - raw scale frame and counters

4. Use the tabs:

   - **Monitor** — live weight and license
   - **Debug** — last 48 log lines, useful for support
   - **Config** — WiFi and Google Sheets settings

If the weight stays `--`, check the RS232 wiring, GND, baud rate (9600), and
MAX232. Open **Debug** to see the received frames and errors.

---

## 6. Optional — upload to Google Sheets

The board keeps its own WiFi AP **and** can connect to your router at the same
time. The ESP32 uploads the data; your phone does **not** get internet through
the ESP32.

> ESP32 supports **2.4 GHz WiFi only**, not 5 GHz.

### 6.1 Create the Google Sheet

1. Open <https://sheets.new> and name it (e.g. **A12E Scale Data**).
2. In the sheet: **Extensions → Apps Script**.
3. Delete the default code and paste the contents of `GoogleSheets_Code.gs`.
4. Change:

   ```js
   const TOKEN = 'CHANGE_ME_TO_A_LONG_RANDOM_TOKEN';
   ```

   to a long random string. Remember it — you will paste it into the monitor.
5. Save.
6. **Project Settings** (gear) → **Time zone** → **Asia/Jakarta**.

### 6.2 Deploy the Apps Script

1. **Deploy → New deployment**.
2. Type: **Web app**.
3. Description: `A12E logger`.
4. **Execute as:** `Me`.
5. **Who has access:** `Anyone`.
6. **Deploy** → **Authorize access**.
7. Copy the **Web app URL**. It ends with `/exec`:

   ```
   https://script.google.com/macros/s/AKfycb.../exec
   ```

### 6.3 Configure the monitor

1. Stay connected to `A12E_SCALE_MONITOR`.
2. Open **Config**.
3. Under **Router WiFi**:
   - press **Scan**, or type your 2.4 GHz SSID
   - enter the WiFi password
4. Under **Google Sheets**:
   - paste the `/exec` URL
   - paste the same token
   - set the upload interval (e.g. `60` seconds)
5. Press **Save & apply**.
6. Press **Send test row**.
7. A row should appear in the Google Sheet.

The board uploads one row per interval only when:

- the scale has a valid reading, and
- the trial/license is active.

After the trial expires, cloud uploads pause until activation. The test row and
Config page still work.

> The Config tab is available **only while connected to the ESP32 AP**
> (`192.168.4.1`). Devices on your router LAN cannot change the settings.

---

## 7. After the 3-hour trial

The trial counts **powered-on time**, not wall-clock time. Turning the board off
pauses the countdown. When it reaches zero, the page locks and hides the weight.

To continue:

1. Copy the **Device ID** from the monitor page.
2. On your phone, use **mobile data** (disconnect from the ESP32 WiFi) and open:

   ```
   https://a12e-license.a12e-scale.workers.dev/activate
   ```

3. Enter the Device ID and your WhatsApp number.
4. Pay **Rp 350.000** to:

   - **BCA 8840884661**
   - **A/N Fitrianah**

5. Send the transfer receipt to WhatsApp **+62 812-6131-6988**.
6. After confirmation, open your order page. It shows the activation key.
7. Reconnect to `A12E_SCALE_MONITOR`, open `http://192.168.4.1`, paste the key
   into the **License** box, and press **Activate**.

   - Paste the **long activation key** from the order page (it starts with
     `QTEy...`), **not the Device ID**.
   - You must be connected to the ESP32 AP when you press **Activate**. If you
     are on mobile data, the page cannot reach the ESP32 and will show
     “checking key” forever.

The key is bound to that specific Device ID. Keep it; it can be re-entered later
if needed.

---

## 8. Troubleshooting

| Problem | What to check |
|---|---|
| Board not detected / no COM port | USB data cable, USB-serial driver, board power |
| Flashing fails to connect | Hold BOOT, tap EN, release BOOT, retry; use 115200 baud |
| AP `A12E_SCALE_MONITOR` not visible | Press EN/RST; check 5 V power; wait a few seconds |
| Page shows `--` and no frames | RS232 wiring, GND, MAX232, 9600 baud |
| Weight is 10× / 100× off | Check the **Raw frame** on the Monitor tab |
| Cloud not uploading | 2.4 GHz WiFi, signal, URL/token, **Send test row** |
| Activate says “checking key” forever | You are not connected to the ESP32 AP. Reconnect to `A12E_SCALE_MONITOR`, reload `192.168.4.1`, and paste the **long key**, not the Device ID. |
| Config tab says 403 | You must be connected to the ESP32 AP, not the router LAN |
| Trial reset unexpectedly | A full flash erase resets the trial to 3 h (normal reflash keeps it) |
| Forgot AP password | Set it again from Config while still connected; otherwise full erase resets to `33333333` |
| Need support | Open **Debug**, press **Copy**, and send the log to WhatsApp |

---

## 9. Safety

- Never connect RS232 ±12 V directly to the ESP32. Use MAX232.
- Use a stable 5 V supply; avoid USB chargers that drop voltage.
- Do not expose the board to water or high heat.
- The AP and Config pages are local to the device. Use a strong AP password if
  the board is in a shared location.

---

## 10. Support

**WhatsApp:** +62 812-6131-6988

When contacting support, please send:

- Device ID
- a photo of the wiring (if weight is not showing)
- the **Debug** log (Copy button)

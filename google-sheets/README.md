# Google Sheets logging setup

This folder contains the Google Apps Script used by the ESP32 `Config` tab.

Rows are appended to a sheet named **`Data`**:

| Column | Example |
|---|---|
| Received At | `2026-11-04 13:05:00` (Google server time) |
| Device ID | `A12E-A1B2C3D4E5F6` |
| Weight (kg) | `4.3` |
| License | `trial` / `licensed` / `test` |
| RSSI | `-58` |
| Uptime (s) | `3600` |
| Device Time | `2026-11-04 13:04:58` (NTP, WIB) |
| Test | `TEST` for test rows |

---

## 1. Create the sheet

1. Open <https://sheets.new>
2. Name it e.g. **A12E Scale Data**
3. You do **not** need to create the `Data` tab — the script creates it.

## 2. Add the Apps Script

1. In the sheet: **Extensions → Apps Script**
2. Delete the default `Code.gs`
3. Paste the contents of [`Code.gs`](Code.gs)
4. Change:

   ```js
   const TOKEN = 'CHANGE_ME_TO_A_LONG_RANDOM_TOKEN';
   ```

   to a long random string. Example:

   ```js
   const TOKEN = 'a12e_7f3c9b1e5d2a4c6f8e0b';
   ```

5. Save (Ctrl+S)
6. **Project Settings** (gear) → **Time zone** → **Asia/Jakarta**

## 3. Deploy as a Web App

1. **Deploy → New deployment**
2. Select type: **Web app**
3. Description: `A12E logger`
4. **Execute as:** `Me`
5. **Who has access:** `Anyone`
6. Click **Deploy**, then **Authorize access** and allow the script
7. Copy the **Web app URL** — it looks like:

   ```
   https://script.google.com/macros/s/AKfycb.../exec
   ```

> Whenever you edit `Code.gs`, use **Deploy → Manage deployments → Edit →
> Version: New version → Deploy**. The `/exec` URL stays the same.

## 4. Test the script

Open this in a browser (replace URL + token + timestamp):

```
https://script.google.com/macros/s/AKfycb.../exec?token=YOUR_TOKEN&device=TEST&weight=1.23&license=test&rssi=-60&uptime=10&timestamp=2026-01-01%2010:00:00&test=1
```

Expected response:

```json
{"ok":true}
```

A row should appear in the `Data` sheet with **Test = TEST**.

If you get `{"ok":false,"error":"unauthorized"}`, the token does not match.

## 5. Configure the ESP32

1. Connect to the monitor AP and open `http://192.168.4.1/config`
2. **Google Sheets URL:** paste the `/exec` URL
3. **Shared token:** paste the same token as `Code.gs`
4. **Upload interval:** e.g. `60` seconds
5. **Save & apply**
6. Press **Send test row**

The row should appear in the sheet immediately.

## Notes

- The ESP32 sends one row per interval **only while the scale has a valid
  reading** and the license is active. Cloud upload pauses after the trial
  expires.
- The Apps Script token protects the endpoint, but the Web App must be deployed
  with access **Anyone** so the ESP32 can post without Google OAuth.
- Use a long random token. Anyone with the URL + token can append rows.
- Apps Script consumer quotas are generous for 1 row/minute per device.
- The ESP32 uses `setInsecure()` for TLS in this version; for a production
  fleet, embed the Google Trust Services root CA and enable certificate
  validation.

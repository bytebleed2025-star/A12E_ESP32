# A12E license / activation server

**Option A: phone-assisted online activation.** The ESP32 monitor stays offline
in AP mode. The customer's phone opens this website, places an order, pays, and
receives an activation key. The key is in exactly the format the existing
`esp32_min_web` firmware already accepts — **no firmware changes required**.

```
Customer phone (mobile data)                    ESP32 (offline, AP mode)
        |                                                |
        | 1. open https://your-site/activate             |
        | 2. enter Device ID + contact                   |
        | 3. pay (bank transfer / QRIS)                  |
        | 4. you mark the order PAID in /admin           |
        | 5. key appears on the order page               |
        | 6. paste key into http://192.168.4.1 --------->| license.cpp verifies
        |                                                | ECDSA signature offline
```

The server signs keys with the **same private key** used by
`esp32_min_web/tools/make_license.js`, and the firmware verifies them with the
embedded public key. The private key never leaves your PC/Cloudflare secret.

- Cloudflare Worker + D1 database
- Free tier is enough for a small product
- Admin dashboard to confirm payments and issue keys
- Optional Telegram notification for new orders
- No payment gateway required (manual confirmation)

---

## Features

| Feature | Description |
|---|---|
| Public order page | `/activate` — Device ID, contact, payment instructions |
| Order page | `/order/:id` — status + key, auto-refreshes every 3 s |
| Admin dashboard | `/admin` — pending/paid orders, one-click key generation |
| Manual key | Generate a key for any Device ID (for WhatsApp/bank-transfer sales) |
| Device binding | Key is signed for one Device ID only |
| Offline verification | Firmware verifies the key without internet |
| Order log | D1 stores device ID, contact, note, status, key |
| Telegram | Optional new-order notification |
| Security | Basic Auth for admin, JSON-only POSTs, rate limit, escaped output |

---

## 1. Requirements

- A free [Cloudflare account](https://dash.cloudflare.com/sign-up)
- Node.js 20 or newer
- The signing key at `../esp32_min_web/tools/license_private.pem`
- `npm install` (installs Wrangler)

---

## 2. First-time setup

Run inside this folder (`license-server`):

```bash
npm install
npx wrangler login
```

Create the database:

```bash
npx wrangler d1 create a12e-license
```

Copy the printed `database_id` into `wrangler.toml`:

```toml
[[d1_databases]]
binding = "DB"
database_name = "a12e-license"
database_id = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
```

Create the table:

```bash
npm run db:init:remote
```

### Secrets

These are **not** in `wrangler.toml`; they are stored by Cloudflare:

```bash
npx wrangler secret put ADMIN_USER
npx wrangler secret put ADMIN_PASSWORD
npx wrangler secret put LICENSE_PRIVATE_KEY
```

When asked for `LICENSE_PRIVATE_KEY`, paste the **entire contents** of
`../esp32_min_web/tools/license_private.pem`:

```
-----BEGIN PRIVATE KEY-----
...
-----END PRIVATE KEY-----
```

On Windows PowerShell you can pipe it directly:

```powershell
Get-Content ..\esp32_min_web\tools\license_private.pem -Raw |
  npx wrangler secret put LICENSE_PRIVATE_KEY
```

Optional Telegram notifications:

```bash
npx wrangler secret put TELEGRAM_BOT_TOKEN
npx wrangler secret put TELEGRAM_CHAT_ID
```

### Public settings

Edit `[vars]` in `wrangler.toml`:

```toml
[vars]
SITE_NAME = "A12E Scale Monitor"
PRICE_LABEL = "Rp 500.000 / device"
SUPPORT_CONTACT = "WhatsApp: +62 8xx-xxxx-xxxx"
PAYMENT_INFO = """Transfer to:
BCA 1234567890
A/N Your Name

After paying, send the receipt to the WhatsApp number above."""
```

`PAYMENT_INFO` supports multiple lines and is displayed as-is (no HTML).

### Deploy

```bash
npm run deploy
```

Wrangler prints your URL, for example:

```
https://a12e-license.esp32dev-by-shd.workers.dev
```

Open:

- `https://.../activate` — customer page
- `https://.../admin` — admin dashboard (browser asks for the Basic Auth
  username/password you set)

A custom domain (e.g. `license.yourdomain.com`) can be added in the Cloudflare
dashboard → Workers → your worker → Settings → Domains & Routes.

---

## 3. Daily selling workflow

1. Customer opens `/activate` and submits the Device ID from the monitor.
2. You receive the order:
   - in `/admin`, and/or
   - as a Telegram message (if configured).
3. Customer pays using your instructions and sends the receipt.
4. In `/admin`, find the order and press **PAID + KEY**.
5. The customer's order page (which auto-refreshes) shows the key.
6. Customer pastes the key into the monitor's **License** box.

For sales that did not come through the website, use **Manual key** in
`/admin`: enter the Device ID and generate a key. It is recorded as a paid
order.

> The order link (`/order/<32 hex>`) is the customer's receipt. It is
> unguessable, so they can reopen it later to copy the key again.

---

## 4. Local development

Create `.dev.vars` from `.dev.vars.example`:

```bash
cp .dev.vars.example .dev.vars
```

Fill in:

- `ADMIN_USER`, `ADMIN_PASSWORD`
- `LICENSE_PRIVATE_KEY` — the PEM with `\n` escapes, or use a **test** keypair
- payment text

Initialize the local database and run:

```bash
npm run db:init:local
npm run dev
```

Then open `http://127.0.0.1:8787/activate`.

> `.dev.vars` is gitignored and contains secrets. Never commit it.
> Wrangler only uses it for local development; `wrangler deploy` does not upload
> it.

Run the automated tests:

```bash
npm test
```

The tests verify the key signature against the real firmware public key, page
scripts, admin escaping, auth, validation, and error handling.

---

## 5. API

Public:

| Method | Path | Description |
|---|---|---|
| GET | `/api/config` | site name, price, payment text, support contact |
| POST | `/api/orders` | `{deviceId, contact, note}` → `{id}` |
| GET | `/api/orders/:id` | status + key when paid |
| GET | `/order/:id` | order status page |
| GET | `/activate` | order form |

Admin (HTTP Basic Auth):

| Method | Path | Description |
|---|---|---|
| GET | `/api/admin/orders` | list latest 200 orders |
| POST | `/api/admin/orders/:id/paid` | mark paid and generate key |
| POST | `/api/admin/orders/:id/cancel` | cancel a pending order |
| POST | `/api/admin/sign` | `{deviceId}` → manual key + paid record |

All POST endpoints require `Content-Type: application/json`.

Example key payload (same as the offline tool):

```
A12E1|A12E-A1B2C3D4E5F6|P|1793766231987
```

---

## 6. Security notes

- The private key is stored as a **Cloudflare secret**, never in the repo or
  the `.bin`.
- Admin uses HTTP Basic Auth over HTTPS. Use a long, unique password. There is
  no logout button; close the browser or use a private window on shared PCs.
- Admin POSTs require JSON, which blocks simple cross-site form CSRF. No CORS
  headers are sent.
- Public order creation is limited to **3 orders per Device ID per hour**.
- User input is escaped before rendering. Order data is only shown on the
  order page / admin.
- The order ID is a 128-bit random secret; treat the order link as private.
- Back up the private key. Losing it means you cannot issue keys for this
  firmware anymore.
- Cloudflare free tier: 100k requests/day for Workers and generous D1 limits.
  Check current limits before a large campaign.

### What this does **not** fix

This server automates **paid key issuing**, but the firmware's 24-hour trial is
still stored on the device. A customer who fully erases the flash can reset the
trial and keep using it for free. Preventing that requires one of:

1. **Online trial registration** — the device must obtain its first trial key
   from this server, and the server refuses a second trial for the same Device
   ID (small firmware change).
2. **Device-online mode** — the ESP32 connects to WiFi and renews a signed
   lease; the server can expire/revoke it (larger firmware change).
3. **eFuse trial marker** — irreversible and risky; not recommended for
   production without careful testing.

Option A is the right first step: it removes manual key handling and gives you a
payment/order log. The trial-reset hardening can be added later if needed.

---

## 7. Project layout

```
license-server/
├── wrangler.toml           # Worker + D1 binding + public settings
├── schema.sql              # orders table
├── package.json
├── src/
│   ├── index.js            # router, auth, orders, D1
│   ├── license.js          # ECDSA P-256 signing (DER, matches firmware)
│   └── pages.js            # activate / order / admin pages
├── test/                   # node:test unit + rendering tests
├── .dev.vars.example       # local secret template
└── README.md
```

---

## 8. Useful commands

```bash
npm run dev            # local server
npm run deploy         # deploy to Cloudflare
npm run db:init:local  # create table locally
npm run db:init:remote # create table on Cloudflare
npm test               # run tests
npx wrangler tail      # live logs in production
npx wrangler d1 export a12e-license --remote --output backup.sql
```

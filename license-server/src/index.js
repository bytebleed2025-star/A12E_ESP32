/**
 * A12E license server - Cloudflare Worker + D1
 *
 * Option A: the ESP32 stays offline in AP mode. The customer's phone opens
 * this site, pays, receives a signed key, then pastes it into the monitor.
 *
 * Pages:
 *   GET  /activate                  order form
 *   GET  /order/:id                 order status + key
 *   GET  /admin                     admin dashboard (Basic Auth)
 *
 * API:
 *   GET  /api/config
 *   POST /api/orders                {deviceId, contact, note}
 *   GET  /api/orders/:id
 *   GET  /api/admin/orders
 *   POST /api/admin/orders/:id/paid
 *   POST /api/admin/orders/:id/cancel
 *   POST /api/admin/sign            {deviceId}
 *   GET  /health
 */

import { makeLicense, makeTrialToken } from './license.js';
import { activatePage, orderPage, adminPage, trialPage } from './pages.js';

const DEVICE_RE = /^A12E-[0-9A-F]{12}$/;
const ORDER_RE = /^[a-f0-9]{32}$/;
const MAX_BODY_BYTES = 16 * 1024;
const MAX_ORDERS_PER_DEVICE_PER_HOUR = 3;
const DEFAULT_TRIAL_SECONDS = 3 * 60 * 60;
const MAX_TRIAL_SECONDS = 7 * 24 * 60 * 60;

const SECURITY_HEADERS = {
  'X-Content-Type-Options': 'nosniff',
  'Referrer-Policy': 'no-referrer',
  'X-Frame-Options': 'DENY',
  'Content-Security-Policy':
    "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; " +
    "connect-src 'self'; img-src 'self' data:; base-uri 'none'; " +
    "form-action 'self'; frame-ancestors 'none'",
};

class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

// ============================================================================
// Responses
// ============================================================================

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      ...SECURITY_HEADERS,
      'Content-Type': 'application/json; charset=utf-8',
      'Cache-Control': 'no-store',
    },
  });
}

function htmlPage(body) {
  return new Response(body, {
    headers: {
      ...SECURITY_HEADERS,
      'Content-Type': 'text/html; charset=utf-8',
      'Cache-Control': 'no-store',
      'X-Robots-Tag': 'noindex, nofollow',
    },
  });
}

function text(message, status = 200) {
  return new Response(message, {
    status,
    headers: {
      ...SECURITY_HEADERS,
      'Content-Type': 'text/plain; charset=utf-8',
      'Cache-Control': 'no-store',
    },
  });
}

function redirect(url) {
  return new Response(null, {
    status: 302,
    headers: { ...SECURITY_HEADERS, Location: url, 'Cache-Control': 'no-store' },
  });
}

function unauthorized() {
  return new Response('Authentication required', {
    status: 401,
    headers: {
      ...SECURITY_HEADERS,
      'WWW-Authenticate': 'Basic realm="A12E Admin", charset="UTF-8"',
      'Content-Type': 'text/plain; charset=utf-8',
      'Cache-Control': 'no-store',
    },
  });
}

// ============================================================================
// Helpers
// ============================================================================

function randomHex(byteCount) {
  const bytes = new Uint8Array(byteCount);
  crypto.getRandomValues(bytes);
  let out = '';
  for (const byte of bytes) out += byte.toString(16).padStart(2, '0');
  return out;
}

function safeEqual(a, b) {
  a = String(a);
  b = String(b);
  let diff = a.length ^ b.length;
  const length = Math.max(a.length, b.length);
  for (let i = 0; i < length; i++) {
    diff |= (a.charCodeAt(i) || 0) ^ (b.charCodeAt(i) || 0);
  }
  return diff === 0;
}

function adminConfigured(env) {
  return Boolean(env.ADMIN_USER && env.ADMIN_PASSWORD);
}

function isAdmin(request, env) {
  const header = request.headers.get('Authorization') || '';
  if (!header.startsWith('Basic ')) return false;

  let decoded;
  try {
    decoded = atob(header.slice(6));
  } catch {
    return false;
  }

  const separator = decoded.indexOf(':');
  if (separator < 0) return false;

  const user = decoded.slice(0, separator);
  const password = decoded.slice(separator + 1);

  return safeEqual(user, env.ADMIN_USER) && safeEqual(password, env.ADMIN_PASSWORD);
}

async function readJson(request) {
  const contentType = (request.headers.get('Content-Type') || '').toLowerCase();
  if (!contentType.includes('application/json')) {
    throw new HttpError(415, 'Content-Type must be application/json');
  }

  const raw = await request.text();
  if (raw.length > MAX_BODY_BYTES) {
    throw new HttpError(413, 'Request body too large');
  }

  try {
    return JSON.parse(raw || '{}');
  } catch {
    throw new HttpError(400, 'Invalid JSON');
  }
}

function normalizeDeviceId(value) {
  return String(value || '').trim().toUpperCase();
}

function publicConfig(env) {
  return {
    siteName: env.SITE_NAME || 'A12E Scale Monitor',
    priceLabel: env.PRICE_LABEL || '',
    paymentInfo: env.PAYMENT_INFO || '',
    supportContact: env.SUPPORT_CONTACT || '',
  };
}

async function getOrderRow(env, id) {
  return env.DB.prepare(
    'SELECT id, device_id, contact, note, status, amount, license_key, created_at, paid_at ' +
      'FROM orders WHERE id = ?'
  )
    .bind(id)
    .first();
}

async function notifyTelegram(env, order) {
  if (!env.TELEGRAM_BOT_TOKEN || !env.TELEGRAM_CHAT_ID) return;

  const lines = [
    'New A12E activation order',
    'Device: ' + order.deviceId,
    'Contact: ' + order.contact,
    'Order: ' + order.id,
  ];
  if (order.note) lines.push('Note: ' + order.note);

  try {
    await fetch(
      `https://api.telegram.org/bot${env.TELEGRAM_BOT_TOKEN}/sendMessage`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          chat_id: env.TELEGRAM_CHAT_ID,
          text: lines.join('\n'),
          disable_web_page_preview: true,
        }),
      }
    );
  } catch (error) {
    console.error('Telegram notification failed:', error);
  }
}

// ============================================================================
// Public handlers
// ============================================================================

async function createOrder(request, env, ctx) {
  const body = await readJson(request);

  const deviceId = normalizeDeviceId(body.deviceId);
  const contact = String(body.contact || '').trim();
  const note = String(body.note || '').trim();

  if (!DEVICE_RE.test(deviceId)) {
    throw new HttpError(400, 'Invalid Device ID (expected A12E-XXXXXXXXXXXX)');
  }
  if (contact.length < 3 || contact.length > 120) {
    throw new HttpError(400, 'Please provide a WhatsApp number or email');
  }
  if (note.length > 500) {
    throw new HttpError(400, 'Note is too long (max 500 characters)');
  }

  const oneHourAgo = Date.now() - 60 * 60 * 1000;
  const recent = await env.DB.prepare(
    'SELECT COUNT(*) AS n FROM orders WHERE device_id = ? AND created_at > ?'
  )
    .bind(deviceId, oneHourAgo)
    .first();

  if (recent && Number(recent.n) >= MAX_ORDERS_PER_DEVICE_PER_HOUR) {
    throw new HttpError(429, 'Too many orders for this device. Contact support.');
  }

  const id = randomHex(16);
  const now = Date.now();

  await env.DB.prepare(
    'INSERT INTO orders (id, device_id, contact, note, status, amount, created_at) ' +
      'VALUES (?, ?, ?, ?, ?, ?, ?)'
  )
    .bind(id, deviceId, contact, note, 'pending', env.PRICE_LABEL || '', now)
    .run();

  if (ctx && typeof ctx.waitUntil === 'function') {
    ctx.waitUntil(notifyTelegram(env, { id, deviceId, contact, note }));
  }

  return json({ ok: true, id }, 201);
}

async function getOrderPublic(env, id) {
  const order = await getOrderRow(env, id);
  if (!order) throw new HttpError(404, 'Order not found');

  return json({
    id: order.id,
    deviceId: order.device_id,
    status: order.status,
    amount: order.amount,
    licenseKey: order.status === 'paid' ? order.license_key : null,
    createdAt: order.created_at,
    paidAt: order.paid_at,
  });
}

// ============================================================================
// One-time trial claim
// ============================================================================

function trialSeconds(env) {
  const configured = Number(env.TRIAL_SECONDS);
  if (
    Number.isInteger(configured) &&
    configured >= 60 &&
    configured <= MAX_TRIAL_SECONDS
  ) {
    return configured;
  }
  return DEFAULT_TRIAL_SECONDS;
}

async function claimTrial(request, env) {
  const body = await readJson(request);
  const deviceId = normalizeDeviceId(body.deviceId);
  const contact = String(body.contact || '').trim();

  if (!DEVICE_RE.test(deviceId)) {
    throw new HttpError(400, 'Invalid Device ID (expected A12E-XXXXXXXXXXXX)');
  }
  if (contact.length > 120) {
    throw new HttpError(400, 'Contact is too long (max 120 characters)');
  }
  if (!env.LICENSE_PRIVATE_KEY) {
    throw new HttpError(503, 'Signing key is not configured (LICENSE_PRIVATE_KEY)');
  }

  const existing = await env.DB.prepare(
    'SELECT device_id FROM trials WHERE device_id = ?'
  )
    .bind(deviceId)
    .first();

  if (existing) {
    throw new HttpError(
      409,
      'The trial for this Device ID has already been claimed. Contact support.'
    );
  }

  const seconds = trialSeconds(env);
  const { token } = await makeTrialToken(
    deviceId,
    seconds,
    env.LICENSE_PRIVATE_KEY
  );
  const now = Date.now();

  try {
    await env.DB.prepare(
      'INSERT INTO trials (device_id, token, seconds, contact, created_at) ' +
        'VALUES (?, ?, ?, ?, ?)'
    )
      .bind(deviceId, token, seconds, contact, now)
      .run();
  } catch (error) {
    if (/UNIQUE/i.test(String(error && error.message))) {
      throw new HttpError(409, 'The trial for this Device ID has already been claimed.');
    }
    throw error;
  }

  return json({ ok: true, deviceId, seconds, token }, 201);
}

// ============================================================================
// Admin handlers
// ============================================================================

async function listOrders(env) {
  const result = await env.DB.prepare(
    "SELECT id, device_id, contact, note, status, amount, license_key, created_at, paid_at " +
      "FROM orders ORDER BY CASE status WHEN 'pending' THEN 0 WHEN 'paid' THEN 1 ELSE 2 END, " +
      'created_at DESC LIMIT 200'
  ).all();

  return json({ orders: result.results || [] });
}

async function markPaid(request, env, id) {
  await readJson(request);

  const order = await getOrderRow(env, id);
  if (!order) throw new HttpError(404, 'Order not found');

  if (order.status === 'paid' && order.license_key) {
    return json({ ok: true, key: order.license_key, deviceId: order.device_id });
  }

  if (!env.LICENSE_PRIVATE_KEY) {
    throw new HttpError(503, 'Signing key is not configured (LICENSE_PRIVATE_KEY)');
  }

  const { key } = await makeLicense(order.device_id, env.LICENSE_PRIVATE_KEY);
  const paidAt = Date.now();

  await env.DB.prepare(
    "UPDATE orders SET status = 'paid', license_key = ?, paid_at = ?, amount = ? WHERE id = ?"
  )
    .bind(key, paidAt, order.amount || env.PRICE_LABEL || '', id)
    .run();

  return json({ ok: true, key, deviceId: order.device_id });
}

async function cancelOrder(request, env, id) {
  await readJson(request);

  const order = await getOrderRow(env, id);
  if (!order) throw new HttpError(404, 'Order not found');
  if (order.status === 'paid') {
    throw new HttpError(409, 'Paid orders cannot be cancelled');
  }

  await env.DB.prepare("UPDATE orders SET status = 'cancelled' WHERE id = ?")
    .bind(id)
    .run();

  return json({ ok: true });
}

async function manualSign(request, env) {
  const body = await readJson(request);
  const deviceId = normalizeDeviceId(body.deviceId);

  if (!DEVICE_RE.test(deviceId)) {
    throw new HttpError(400, 'Invalid Device ID (expected A12E-XXXXXXXXXXXX)');
  }
  if (!env.LICENSE_PRIVATE_KEY) {
    throw new HttpError(503, 'Signing key is not configured (LICENSE_PRIVATE_KEY)');
  }

  const { key } = await makeLicense(deviceId, env.LICENSE_PRIVATE_KEY);
  const id = randomHex(16);
  const now = Date.now();

  await env.DB.prepare(
    'INSERT INTO orders (id, device_id, contact, note, status, amount, license_key, created_at, paid_at) ' +
      "VALUES (?, ?, ?, ?, 'paid', ?, ?, ?, ?)"
  )
    .bind(
      id,
      deviceId,
      'manual',
      'Manual key issued from admin',
      env.PRICE_LABEL || '',
      key,
      now,
      now
    )
    .run();

  return json({ ok: true, id, deviceId, key });
}

async function listTrials(env) {
  const result = await env.DB.prepare(
    'SELECT device_id, seconds, contact, created_at FROM trials ' +
      'ORDER BY created_at DESC LIMIT 200'
  ).all();

  return json({ trials: result.results || [] });
}

async function resetTrial(request, env, deviceId) {
  await readJson(request);

  if (!DEVICE_RE.test(deviceId)) {
    throw new HttpError(400, 'Invalid Device ID');
  }

  await env.DB.prepare('DELETE FROM trials WHERE device_id = ?')
    .bind(deviceId)
    .run();

  return json({ ok: true, deviceId });
}

// ============================================================================
// Router
// ============================================================================

export default {
  async fetch(request, env, ctx) {
    try {
      if (!env.DB) {
        return text('D1 database is not configured (binding DB)', 503);
      }

      const url = new URL(request.url);
      let path = url.pathname;
      if (path.length > 1 && path.endsWith('/')) path = path.slice(0, -1);

      const method = request.method.toUpperCase();

      if (method === 'GET' && path === '/health') {
        return json({ ok: true, service: 'a12e-license' });
      }

      if (method === 'GET' && path === '/robots.txt') {
        return text('User-agent: *\nDisallow: /\n');
      }

      if (method === 'GET' && path === '/') {
        return redirect(url.origin + '/activate');
      }

      if (method === 'GET' && path === '/activate') {
        return htmlPage(activatePage());
      }

      if (method === 'GET' && path === '/trial') {
        return htmlPage(trialPage());
      }

      if (method === 'GET' && path === '/api/config') {
        return json(publicConfig(env));
      }

      if (method === 'POST' && path === '/api/orders') {
        return await createOrder(request, env, ctx);
      }

      if (method === 'POST' && path === '/api/trial') {
        return await claimTrial(request, env);
      }

      const publicOrderMatch = path.match(/^\/order\/([a-f0-9]{32})$/);
      if (method === 'GET' && publicOrderMatch) {
        return htmlPage(orderPage(publicOrderMatch[1]));
      }

      const publicApiMatch = path.match(/^\/api\/orders\/([a-f0-9]{32})$/);
      if (method === 'GET' && publicApiMatch) {
        return await getOrderPublic(env, publicApiMatch[1]);
      }

      // Everything below is admin-only.
      const isAdminPath = path === '/admin' || path.startsWith('/api/admin/');
      if (isAdminPath) {
        if (!adminConfigured(env)) {
          return text(
            'Admin is not configured. Set ADMIN_USER and ADMIN_PASSWORD secrets.',
            503
          );
        }
        if (!isAdmin(request, env)) {
          return unauthorized();
        }
      }

      if (method === 'GET' && path === '/admin') {
        return htmlPage(adminPage());
      }

      if (method === 'GET' && path === '/api/admin/orders') {
        return await listOrders(env);
      }

      if (method === 'GET' && path === '/api/admin/trials') {
        return await listTrials(env);
      }

      const paidMatch = path.match(/^\/api\/admin\/orders\/([a-f0-9]{32})\/paid$/);
      if (method === 'POST' && paidMatch) {
        return await markPaid(request, env, paidMatch[1]);
      }

      const cancelMatch = path.match(/^\/api\/admin\/orders\/([a-f0-9]{32})\/cancel$/);
      if (method === 'POST' && cancelMatch) {
        return await cancelOrder(request, env, cancelMatch[1]);
      }

      if (method === 'POST' && path === '/api/admin/sign') {
        return await manualSign(request, env);
      }

      const resetTrialMatch = path.match(
        /^\/api\/admin\/trials\/(A12E-[0-9A-F]{12})\/reset$/
      );
      if (method === 'POST' && resetTrialMatch) {
        return await resetTrial(request, env, resetTrialMatch[1]);
      }

      return text('Not found', 404);
    } catch (error) {
      if (error instanceof HttpError) {
        return json({ ok: false, error: error.message }, error.status);
      }
      console.error(error);
      return json({ ok: false, error: 'Server error' }, 500);
    }
  },
};

// Exported for local tests.
export { makeLicense, makeTrialToken, ORDER_RE };

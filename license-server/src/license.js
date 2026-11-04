/**
 * License key signing - must stay byte-for-byte compatible with
 * esp32_min_web/src/license.cpp and esp32_min_web/tools/make_license.js.
 *
 * Key format:
 *   base64url("A12E1|<DEVICE-ID>|P|<serial>") + "." + base64url(DER_ECDSA_SIG)
 *
 * Web Crypto returns an ECDSA signature as raw r||s (64 bytes for P-256),
 * while the firmware (mbedtls) expects DER. The conversion is below.
 */

const PRODUCT_CODE = 'A12E1';
const ECDSA_RAW_LENGTH = 64;

const encoder = new TextEncoder();

let cachedPem = null;
let cachedKey = null;

function pemToArrayBuffer(pem) {
  const cleaned = String(pem)
    .replace(/\\n/g, '\n')
    .replace(/-----BEGIN [^-]+-----/g, '')
    .replace(/-----END [^-]+-----/g, '')
    .replace(/\s+/g, '');

  const binary = atob(cleaned);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes.buffer;
}

async function getPrivateKey(pem) {
  if (!pem) {
    throw new Error('LICENSE_PRIVATE_KEY is not configured');
  }
  if (cachedKey && cachedPem === pem) {
    return cachedKey;
  }

  cachedKey = await crypto.subtle.importKey(
    'pkcs8',
    pemToArrayBuffer(pem),
    { name: 'ECDSA', namedCurve: 'P-256' },
    false,
    ['sign']
  );
  cachedPem = pem;
  return cachedKey;
}

function concatBytes(...parts) {
  const total = parts.reduce((sum, part) => sum + part.length, 0);
  const out = new Uint8Array(total);
  let offset = 0;
  for (const part of parts) {
    out.set(part, offset);
    offset += part.length;
  }
  return out;
}

function derInteger(bytes) {
  // DER INTEGER: strip leading zeroes, prepend 0x00 if the high bit is set.
  let start = 0;
  while (start < bytes.length - 1 && bytes[start] === 0) start++;
  let value = bytes.slice(start);
  if (value[0] & 0x80) {
    value = concatBytes(new Uint8Array([0x00]), value);
  }
  return concatBytes(new Uint8Array([0x02, value.length]), value);
}

export function rawEcdsaToDer(raw) {
  if (raw.length !== ECDSA_RAW_LENGTH) {
    throw new Error('Unexpected ECDSA signature length: ' + raw.length);
  }
  const r = derInteger(raw.slice(0, 32));
  const s = derInteger(raw.slice(32));
  const body = concatBytes(r, s);
  return concatBytes(new Uint8Array([0x30, body.length]), body);
}

export function base64url(input) {
  const bytes =
    input instanceof Uint8Array
      ? input
      : new Uint8Array(input);

  let binary = '';
  for (let i = 0; i < bytes.length; i++) {
    binary += String.fromCharCode(bytes[i]);
  }

  return btoa(binary)
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/, '');
}

/**
 * Create a perpetual, device-bound activation key.
 * @param {string} deviceId e.g. A12E-A1B2C3D4E5F6
 * @param {string} privateKeyPem PKCS8 PEM
 * @param {number|string} [serial] unique serial stored in the payload
 * @returns {Promise<{payload: string, key: string}>}
 */
export async function makeLicense(deviceId, privateKeyPem, serial = Date.now()) {
  if (!/^A12E-[0-9A-F]{12}$/.test(deviceId)) {
    throw new Error('Invalid device ID');
  }
  if (String(serial).includes('|')) {
    throw new Error('Invalid serial');
  }

  const payload = [PRODUCT_CODE, deviceId, 'P', String(serial)].join('|');
  const key = await getPrivateKey(privateKeyPem);

  const rawSignature = new Uint8Array(
    await crypto.subtle.sign(
      { name: 'ECDSA', hash: 'SHA-256' },
      key,
      encoder.encode(payload)
    )
  );

  const derSignature = rawEcdsaToDer(rawSignature);

  return {
    payload,
    key: base64url(encoder.encode(payload)) + '.' + base64url(derSignature),
  };
}

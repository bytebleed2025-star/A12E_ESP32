import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

import { makeLicense, base64url, rawEcdsaToDer } from '../src/license.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');

const privatePem = fs.readFileSync(
  path.join(root, 'esp32_min_web', 'tools', 'license_private.pem'),
  'utf8'
);
const publicPem = fs.readFileSync(
  path.join(root, 'esp32_min_web', 'tools', 'license_public.pem'),
  'utf8'
);
const publicKey = crypto.createPublicKey(publicPem);

function verifyPayload(payload, key) {
  const [payloadPart, signaturePart] = key.split('.');
  const decodedPayload = Buffer.from(payloadPart, 'base64url').toString('utf8');
  const signature = Buffer.from(signaturePart, 'base64url');

  assert.equal(decodedPayload, payload);

  const verifier = crypto.createVerify('SHA256');
  verifier.update(payload, 'utf8');
  verifier.end();
  return verifier.verify(publicKey, signature);
}

test('generates a key that verifies with the firmware public key', async () => {
  const { payload, key } = await makeLicense(
    'A12E-A1B2C3D4E5F6',
    privatePem,
    1793766231987
  );

  assert.equal(payload, 'A12E1|A12E-A1B2C3D4E5F6|P|1793766231987');
  assert.match(key, /^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$/);
  assert.ok(verifyPayload(payload, key));
});

test('signature is valid across many random signatures', async () => {
  for (let i = 0; i < 25; i++) {
    const { payload, key } = await makeLicense(
      'A12E-0F1E2D3C4B5A',
      privatePem,
      1000000 + i
    );
    assert.ok(verifyPayload(payload, key), 'signature ' + i + ' failed');
  }
});

test('raw ECDSA to DER handles high-bit and short integers', () => {
  const raw = new Uint8Array(64);
  raw[0] = 0x80; // r high bit set -> DER must prepend 0x00
  raw[31] = 0x01;
  raw[32] = 0x00; // s starts with zero -> DER must strip leading zeros
  raw[63] = 0xff;

  const der = rawEcdsaToDer(raw);
  assert.equal(der[0], 0x30);
  assert.equal(der[1], der.length - 2);

  let i = 2;
  assert.equal(der[i], 0x02);
  assert.equal(der[i + 1], 33); // 32 bytes + sign padding
  assert.equal(der[i + 2], 0x00);
  i += 2 + 33;

  assert.equal(der[i], 0x02);
  assert.equal(der[i + 1], 2); // 0x00 0xff
  assert.equal(der[i + 2], 0x00);
  assert.equal(der[i + 3], 0xff);
  i += 2 + 2;

  assert.equal(i, der.length); // no trailing bytes
});

test('rejects invalid device IDs', async () => {
  await assert.rejects(
    () => makeLicense('A12E-XYZ', privatePem),
    /Invalid device ID/
  );
});

test('base64url is URL safe and unpadded', () => {
  const encoded = base64url(new Uint8Array([0xfb, 0xff, 0xfe]));
  assert.equal(encoded, '-__-');
});

test('rejects a missing private key', async () => {
  await assert.rejects(
    () => makeLicense('A12E-A1B2C3D4E5F6', ''),
    /LICENSE_PRIVATE_KEY is not configured/
  );
});

#!/usr/bin/env node
/**
 * make_license.js - generate activation keys for A12E Scale Monitor
 *
 * Usage (one or more devices):
 *   node tools/make_license.js A12E-A1B2C3D4E5F6
 *   node tools/make_license.js A12E-A1B2C3D4E5F6 A12E-0F1E2D3C4B5A
 *
 * The private key is read from tools/license_private.pem (never ship it).
 * Each key is bound to one device ID and is perpetual ("P").
 *
 * Output format:
 *   A12E-A1B2C3D4E5F6<TAB>A12E1|A12E-A1B2C3D4E5F6|P|...<TAB>payload.signature
 */

'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const PRODUCT_CODE = 'A12E1';
const PRIVATE_KEY_PATH = path.join(__dirname, 'license_private.pem');
const DEVICE_RE = /^A12E-[0-9A-F]{12}$/;

function usage(message) {
  if (message) console.error('Error: ' + message);
  console.error('\nUsage: node tools/make_license.js <DEVICE-ID> [DEVICE-ID ...]');
  console.error('Example: node tools/make_license.js A12E-A1B2C3D4E5F6');
  process.exit(1);
}

function base64url(buffer) {
  return buffer
    .toString('base64')
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/, '');
}

const devices = process.argv.slice(2).map(function (d) {
  return d.trim().toUpperCase();
});

if (devices.length === 0) usage('missing device ID');

for (const device of devices) {
  if (!DEVICE_RE.test(device)) {
    usage('invalid device ID "' + device + '" (expected A12E-XXXXXX)');
  }
}

if (!fs.existsSync(PRIVATE_KEY_PATH)) {
  usage('private key not found: ' + PRIVATE_KEY_PATH);
}

const privateKey = crypto.createPrivateKey(fs.readFileSync(PRIVATE_KEY_PATH));
const publicKey = crypto.createPublicKey(privateKey);

for (const device of devices) {
  // Signed payload. Keep this format in sync with src/license.cpp.
  const payload = [PRODUCT_CODE, device, 'P', Date.now()].join('|');

  const signer = crypto.createSign('SHA256');
  signer.update(payload, 'utf8');
  signer.end();
  const signature = signer.sign(privateKey);

  // Self-check before printing.
  const verifier = crypto.createVerify('SHA256');
  verifier.update(payload, 'utf8');
  verifier.end();
  if (!verifier.verify(publicKey, signature)) {
    console.error('Internal error: signature self-check failed');
    process.exit(2);
  }

  const key = base64url(Buffer.from(payload, 'utf8')) + '.' + base64url(signature);
  console.log(device + '\t' + payload + '\t' + key);
}

#!/usr/bin/env node
/**
 * generate_keypair.js - create a NEW ECDSA P-256 signing keypair.
 *
 * Only run this when starting a new product line: it replaces
 * tools/license_private.pem and tools/license_public.pem, and every
 * firmware must be rebuilt with the new public key. Existing keys stop
 * working.
 *
 *   node tools/generate_keypair.js
 *   node tools/generate_keypair.js --force   (overwrite existing pair)
 */

'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const privPath = path.join(__dirname, 'license_private.pem');
const pubPath = path.join(__dirname, 'license_public.pem');
const force = process.argv.includes('--force');

if (!force && (fs.existsSync(privPath) || fs.existsSync(pubPath))) {
  console.error('Key files already exist. Use --force to overwrite them.');
  console.error('WARNING: overwriting invalidates every license you already sold.');
  process.exit(1);
}

const pair = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });

fs.writeFileSync(
  privPath,
  pair.privateKey.export({ type: 'pkcs8', format: 'pem' })
);
fs.writeFileSync(
  pubPath,
  pair.publicKey.export({ type: 'spki', format: 'pem' })
);

console.log('Wrote ' + privPath + '  (KEEP PRIVATE - never ship or commit)');
console.log('Wrote ' + pubPath + '   (safe to publish)');
console.log('\nCopy the public key into include/license_config.h, then rebuild:');
console.log(fs.readFileSync(pubPath, 'utf8'));

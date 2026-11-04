import test from 'node:test';
import assert from 'node:assert/strict';

import worker from '../src/index.js';

function post(path, body, contentType = 'application/json') {
  return new Request('https://example.test' + path, {
    method: 'POST',
    headers: { 'Content-Type': contentType },
    body,
  });
}

const env = { DB: {} };

test('health endpoint works without touching D1', async () => {
  const response = await worker.fetch(
    new Request('https://example.test/health'),
    env,
    {}
  );
  assert.equal(response.status, 200);
  assert.equal((await response.json()).ok, true);
});

test('invalid device IDs are rejected with 400 (async errors are caught)', async () => {
  const response = await worker.fetch(
    post('/api/orders', JSON.stringify({ deviceId: 'BAD', contact: 'x@y.z' })),
    env,
    {}
  );
  assert.equal(response.status, 400);
  assert.match((await response.json()).error, /Invalid Device ID/);
});

test('non-JSON bodies are rejected with 415', async () => {
  const response = await worker.fetch(
    post('/api/orders', 'device=BAD', 'application/x-www-form-urlencoded'),
    env,
    {}
  );
  assert.equal(response.status, 415);
});

test('admin routes require auth and never touch D1 when unauthenticated', async () => {
  const response = await worker.fetch(
    new Request('https://example.test/api/admin/orders'),
    { ...env, ADMIN_USER: 'admin', ADMIN_PASSWORD: 'secret' },
    {}
  );
  assert.equal(response.status, 401);
  assert.match(response.headers.get('WWW-Authenticate') || '', /Basic/);
});

test('admin routes return 503 when credentials are not configured', async () => {
  const response = await worker.fetch(
    new Request('https://example.test/admin'),
    env,
    {}
  );
  assert.equal(response.status, 503);
});

test('admin auth accepts correct Basic credentials', async () => {
  const response = await worker.fetch(
    new Request('https://example.test/admin', {
      headers: {
        Authorization:
          'Basic ' + Buffer.from('admin:secret', 'utf8').toString('base64'),
      },
    }),
    { ...env, ADMIN_USER: 'admin', ADMIN_PASSWORD: 'secret' },
    {}
  );
  // Auth passes; the page is served (no D1 access on /admin).
  assert.equal(response.status, 200);
});

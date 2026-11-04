import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';

import { adminPage } from '../src/pages.js';

function extractScript(html) {
  const start = html.indexOf('<script>');
  const end = html.lastIndexOf('</script>');
  return html.slice(start + '<script>'.length, end);
}

function makeElement() {
  return {
    textContent: '',
    innerHTML: '',
    value: '',
    className: '',
    style: {},
    addEventListener() {},
  };
}

const ORDER_ID = 'a'.repeat(32);

test('admin renders orders with escaped user data and working action buttons', async () => {
  const elements = new Map();
  const document = {
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, makeElement());
      return elements.get(id);
    },
    createElement() {
      return makeElement();
    },
    body: { appendChild() {}, removeChild() {} },
    execCommand() {
      return true;
    },
  };

  const orders = [
    {
      id: ORDER_ID,
      device_id: 'A12E-A1B2C3D4E5F6',
      contact: '<img src=x onerror=alert(1)>',
      note: 'hello & <b>world</b>',
      status: 'pending',
      amount: 'Rp 150.000',
      license_key: null,
      created_at: 1793766231987,
      paid_at: null,
    },
    {
      id: 'b'.repeat(32),
      device_id: 'A12E-0F1E2D3C4B5A',
      contact: 'buyer@example.com',
      note: '',
      status: 'paid',
      amount: 'Rp 150.000',
      license_key: 'eyJ.a-b_c',
      created_at: 1793766231987,
      paid_at: 1793766231987,
    },
  ];

  const context = {
    document,
    navigator: {},
    console,
    setTimeout,
    clearTimeout,
    confirm: () => true,
    alert: () => {},
    fetch: async () => ({
      ok: true,
      status: 200,
      json: async () => ({ orders }),
    }),
  };

  vm.createContext(context);
  vm.runInContext(extractScript(adminPage()), context);

  // Let the initial load() promise settle.
  await new Promise((resolve) => setTimeout(resolve, 20));

  const html = elements.get('orders').innerHTML;

  assert.ok(html.includes('onclick="markPaid(\'' + ORDER_ID + '\')"'));
  assert.ok(html.includes('onclick="cancelOrder(\'' + ORDER_ID + '\')"'));
  assert.ok(html.includes('onclick="copyOrderKey(\'' + 'b'.repeat(32) + '\', this)"'));

  assert.ok(!html.includes('<img src=x'), 'contact HTML must be escaped');
  assert.ok(html.includes('&lt;img src=x onerror=alert(1)&gt;'));
  assert.ok(html.includes('hello &amp; &lt;b&gt;world&lt;/b&gt;'));

  assert.match(elements.get('stats').innerHTML, /1<\/b> pending/);
});

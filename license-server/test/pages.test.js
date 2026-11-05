import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';

import { activatePage, orderPage, adminPage, trialPage } from '../src/pages.js';

function scriptOf(html) {
  const start = html.indexOf('<script>');
  const end = html.lastIndexOf('</script>');
  assert.ok(start >= 0 && end > start, 'page must contain a script');
  return html.slice(start + '<script>'.length, end);
}

test('all page scripts are syntactically valid', () => {
  for (const [name, html] of [
    ['activate', activatePage()],
    ['trial', trialPage()],
    ['order', orderPage('a'.repeat(32))],
    ['admin', adminPage()],
  ]) {
    assert.doesNotThrow(
      () => new vm.Script(scriptOf(html), { filename: name + '.js' }),
      name + ' page script has a syntax error'
    );
  }
});

test('public pages do not interpolate untrusted order data', () => {
  const html = orderPage('0123456789abcdef0123456789abcdef');
  assert.match(html, /0123456789abcdef0123456789abcdef/);
});

test('A12E Device ID inputs allow all 17 characters', () => {
  for (const html of [activatePage(), trialPage(), adminPage()]) {
    assert.match(html, /maxlength="17"/);
    assert.doesNotMatch(html, /maxlength="18"/);
  }
});

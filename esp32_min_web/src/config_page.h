/**
 * @file config_page.h
 * @brief Config tab: AP, STA, Google Sheets.
 */

#ifndef CONFIG_PAGE_H
#define CONFIG_PAGE_H

#include <Arduino.h>

static const char CONFIG_BODY[] = R"HTML(
      <div class="grid two">
        <section class="card">
          <h2>Access Point</h2>
          <label class="field"><span>AP SSID</span>
            <input id="apSsid" maxlength="32" autocomplete="off" spellcheck="false">
          </label>
          <label class="field"><span>AP password</span>
            <input id="apPass" type="password" maxlength="63"
                   placeholder="Leave blank to keep current" autocomplete="new-password">
          </label>
          <p class="note">
            Changing AP settings reboots the board. Reconnect to the new WiFi name.
            Minimum password length is 8 characters.
          </p>
        </section>

        <section class="card">
          <h2>Router WiFi (STA)</h2>
          <div class="field">
            <span>Router SSID</span>
            <div class="row">
              <input id="staSsid" maxlength="32" autocomplete="off" spellcheck="false">
              <button class="btn secondary small" type="button" id="scanBtn">Scan</button>
            </div>
          </div>
          <div id="scanList" class="scan-list"></div>
          <label class="field"><span>Router password</span>
            <input id="staPass" type="password" maxlength="63"
                   placeholder="Leave blank to keep current" autocomplete="new-password">
          </label>
          <p class="note">
            ESP32 connects to 2.4 GHz WiFi only. The AP stays active while connected.
            AP clients do not get internet through the ESP32.
          </p>
        </section>
      </div>

      <div class="grid two" style="margin-top:16px">
        <section class="card">
          <h2>Google Sheets</h2>
          <label class="field"><span>Apps Script Web App URL</span>
            <input id="sheetUrl" maxlength="250"
                   placeholder="https://script.google.com/macros/s/.../exec"
                   autocomplete="off" spellcheck="false">
          </label>
          <label class="field"><span>Shared token</span>
            <input id="sheetToken" type="password" maxlength="128"
                   placeholder="Leave blank to keep current" autocomplete="new-password">
          </label>
          <div class="row">
            <button class="btn secondary small" type="button" id="testBtn">Send test row</button>
            <span class="msg" id="testMsg"></span>
          </div>
          <p class="note" style="margin-top:10px">
            Save &amp; apply first; the test uses the saved URL and token.
            Deploy <span class="mono">google-sheets/Code.gs</span> as a Web App
            (access: Anyone) and use the same token in both places.
          </p>
        </section>

        <section class="card">
          <h2>Device status</h2>
          <div class="kv"><span class="k">AP</span><span class="v" id="apStatus">--</span></div>
          <div class="kv"><span class="k">Router</span><span class="v" id="staStatus">--</span></div>
          <div class="kv"><span class="k">STA IP</span><span class="v" id="staIp">--</span></div>
          <div class="kv"><span class="k">Signal</span><span class="v" id="rssi">--</span></div>
          <div class="kv"><span class="k">Google Sheets</span><span class="v" id="sheetState">--</span></div>
          <div class="kv"><span class="k">Interval</span><span class="v" id="intervalState">--</span></div>

          <div class="divider"></div>

          <label class="field"><span>Upload interval (seconds)</span>
            <input id="interval" type="number" min="10" max="3600">
          </label>
          <button class="btn" type="button" id="saveBtn" onclick="saveSettings()">Save &amp; apply</button>
          <div class="msg" id="saveMsg"></div>
          <p class="note" style="margin-top:10px">
            If the AP name or password changes, the board reboots in about 2 seconds.
            Reconnect to the new AP and open 192.168.4.1 again.
          </p>
        </section>
      </div>
)HTML";

static const char CONFIG_SCRIPT[] = R"JS(
    var $ = function (id) { return document.getElementById(id); };
    var saveBtn = $('saveBtn');
    var scanBtn = $('scanBtn');
    var testBtn = $('testBtn');

    function setMsg(el, text, kind) {
      el.textContent = text || '';
      el.className = 'msg' + (kind ? ' ' + kind : '');
    }

    function applyStatus(d) {
      $('apStatus').textContent = d.apSsid + '  ' + d.apIp;
      $('staStatus').textContent =
        (d.staStatus || 'offline') + (d.staIp ? '  ' + d.staIp : '');
      $('staIp').textContent = d.staIp || '-';
      $('rssi').textContent = d.staConnected ? d.rssi + ' dBm' : '-';
      $('sheetState').textContent = d.sheetConfigured ? 'configured' : 'not configured';
      $('intervalState').textContent = d.interval + ' s';

      $('apPass').placeholder = d.apPassSet
        ? 'Leave blank to keep current'
        : 'Open network - set a password';
      $('staPass').placeholder = d.staPassSet
        ? 'Leave blank to keep current'
        : 'No password saved';
      $('sheetToken').placeholder = d.sheetTokenSet
        ? 'Leave blank to keep current'
        : 'No token saved';
    }

    function load() {
      fetch('/api/settings', { cache: 'no-store' })
        .then(function (r) { return r.json(); })
        .then(function (d) {
          $('apSsid').value = d.apSsid || '';
          $('staSsid').value = d.staSsid || '';
          $('sheetUrl').value = d.sheetUrl || '';
          $('interval').value = d.interval || 60;
          $('apPass').value = '';
          $('staPass').value = '';
          $('sheetToken').value = '';
          applyStatus(d);
        })
        .catch(function () {
          setMsg($('saveMsg'), 'Could not load settings.', 'err');
        });
    }

    function saveSettings() {
      var apSsid = $('apSsid').value.trim();
      var staSsid = $('staSsid').value.trim();
      var sheetUrl = $('sheetUrl').value.trim();
      var interval = parseInt($('interval').value, 10);

      if (!apSsid) {
        setMsg($('saveMsg'), 'AP SSID cannot be empty.', 'err');
        return;
      }
      if (!interval || interval < 10 || interval > 3600) {
        setMsg($('saveMsg'), 'Interval must be 10-3600 seconds.', 'err');
        return;
      }

      var params = new URLSearchParams();
      params.set('apSsid', apSsid);
      if ($('apPass').value) params.set('apPass', $('apPass').value);
      params.set('staSsid', staSsid);
      if ($('staPass').value) params.set('staPass', $('staPass').value);
      params.set('sheetUrl', sheetUrl);
      if ($('sheetToken').value) params.set('sheetToken', $('sheetToken').value);
      params.set('interval', String(interval));

      saveBtn.disabled = true;
      setMsg($('saveMsg'), 'Saving...', '');

      fetch('/api/settings', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          'X-Requested-With': 'A12E'
        },
        body: params.toString()
      })
        .then(function (r) {
          return r.json().then(function (d) { return { ok: r.ok, data: d }; });
        })
        .then(function (res) {
          if (!res.ok) throw new Error(res.data.error || 'Save failed');
          if (res.data.reboot) {
            setMsg($('saveMsg'), 'Saved. Rebooting... reconnect to the new AP.', 'ok');
          } else {
            setMsg($('saveMsg'), 'Saved and applied.', 'ok');
            $('apPass').value = '';
            $('staPass').value = '';
            $('sheetToken').value = '';
            setTimeout(load, 1500);
            saveBtn.disabled = false;
          }
        })
        .catch(function (err) {
          setMsg($('saveMsg'), err.message || 'Save failed.', 'err');
          saveBtn.disabled = false;
        });
    }

    function scanNetworks() {
      scanBtn.disabled = true;
      scanBtn.textContent = 'Scanning...';
      $('scanList').className = 'scan-list';
      $('scanList').textContent = '';

      fetch('/api/wifi/scan', {
        method: 'POST',
        headers: { 'X-Requested-With': 'A12E' }
      })
        .then(function (r) { return r.json(); })
        .then(function (d) {
          scanBtn.disabled = false;
          scanBtn.textContent = 'Scan';
          if (!d.ok) throw new Error(d.error || 'Scan failed');

          d.networks.forEach(function (network) {
            var item = document.createElement('div');
            item.className = 'scan-item';

            var name = document.createElement('span');
            name.textContent = network.ssid + (network.secure ? '' : '  (open)');

            var signal = document.createElement('span');
            signal.className = 'muted';
            signal.textContent = network.rssi + ' dBm';

            item.appendChild(name);
            item.appendChild(signal);
            item.addEventListener('click', function () {
              $('staSsid').value = network.ssid;
              $('scanList').className = 'scan-list';
            });
            $('scanList').appendChild(item);
          });

          if (d.networks.length === 0) {
            $('scanList').textContent = 'No networks found.';
          }
          $('scanList').className = 'scan-list show';
        })
        .catch(function (err) {
          scanBtn.disabled = false;
          scanBtn.textContent = 'Scan';
          setMsg($('saveMsg'), err.message || 'Scan failed.', 'err');
        });
    }

    function testSheet() {
      testBtn.disabled = true;
      setMsg($('testMsg'), 'Sending...', '');
      fetch('/api/sheets/test', {
        method: 'POST',
        headers: { 'X-Requested-With': 'A12E' }
      })
        .then(function (r) {
          return r.json().then(function (d) { return { ok: r.ok, data: d }; });
        })
        .then(function (res) {
          if (res.data.ok) {
            setMsg($('testMsg'), 'Test row sent.', 'ok');
          } else {
            setMsg($('testMsg'), res.data.message || 'Test failed.', 'err');
          }
        })
        .catch(function () {
          setMsg($('testMsg'), 'Test request failed.', 'err');
        })
        .finally(function () {
          testBtn.disabled = false;
        });
    }

    scanBtn.addEventListener('click', scanNetworks);
    testBtn.addEventListener('click', testSheet);
    load();
)JS";

#endif // CONFIG_PAGE_H

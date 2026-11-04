/**
 * @file monitor_page.h
 * @brief Main monitor page body + client script.
 */

#ifndef MONITOR_PAGE_H
#define MONITOR_PAGE_H

#include <Arduino.h>

static const char MONITOR_BODY[] = R"HTML(
      <div class="notice" id="lockNote">
        Trial expired. Enter an activation key below to continue using this device.
      </div>

      <div class="grid">
        <section class="card hero">
          <div class="hero-top">
            <span class="chip" id="badge">Waiting...</span>
            <span class="chip" id="cloudChip">Cloud</span>
          </div>
          <div class="hero-label">Timbangan Terukur</div>
          <div class="hero-value" id="valueBox">
            <span id="weight">--</span><span class="unit">kg</span>
          </div>
          <div class="hero-sub" id="heroSub">Menunggu data timbangan...</div>
        </section>

        <section class="card">
          <h2>Telemetry</h2>
          <div class="kv"><span class="k">Raw frame</span><span class="v" id="raw">-</span></div>
          <div class="kv"><span class="k">Frames received</span><span class="v" id="frames">0</span></div>
          <div class="kv"><span class="k">Last update</span><span class="v" id="age">-</span></div>
          <div class="kv"><span class="k">Uptime</span><span class="v" id="uptime">-</span></div>
        </section>
      </div>

      <div class="grid two">
        <section class="card">
          <h2>Cloud upload</h2>
          <div class="kv"><span class="k">Router WiFi</span><span class="v" id="staStatus">--</span></div>
          <div class="kv"><span class="k">Last upload</span><span class="v" id="lastUpload">--</span></div>
          <div class="kv"><span class="k">Uploads</span><span class="v" id="uploadCounts">--</span></div>
          <div class="divider"></div>
          <a class="btn secondary" href="/config">Configure cloud</a>
        </section>

        <section class="card">
          <div style="display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:10px">
            <h2 style="margin:0">License</h2>
            <span class="chip" id="licensePill">Trial</span>
          </div>
          <div class="kv"><span class="k">Device ID</span><span class="v" id="deviceId">...</span></div>
          <div class="kv"><span class="k">Trial left</span><span class="v" id="trialLeft">--</span></div>

          <div style="margin-top:12px">
            <button class="btn secondary small" type="button" onclick="copyDeviceId(this)">Copy Device ID</button>
          </div>

          <div id="activateArea" style="margin-top:16px">
            <label class="field"><span>Activation key</span>
              <textarea id="keyInput" rows="2" placeholder="Paste the long activation key"
                        spellcheck="false" autocomplete="off"></textarea>
            </label>
            <p class="note" style="margin:6px 0 10px">
              Paste the long key from your order page — not the Device ID.
              Stay connected to <strong>A12E_SCALE_MONITOR</strong> while pressing Activate.
            </p>
            <button class="btn" id="activateBtn" type="button" onclick="activateLicense()">Activate</button>
            <div class="msg" id="activateMsg"></div>

            <div class="divider"></div>

            <div class="field"><span>Need a key? Activation page</span>
              <div class="row">
                <input id="buyUrl" readonly
                       value="https://a12e-license.a12e-scale.workers.dev/activate">
                <button class="btn secondary small" type="button"
                        onclick="copyBuyLink(this)">Copy link</button>
              </div>
            </div>
            <p class="note">
              Disconnect from this WiFi first and open the link with mobile data.
              The page needs internet. Then reconnect to
              <strong>A12E_SCALE_MONITOR</strong> and paste the key here.
            </p>
          </div>

          <div class="ok-box" id="licensedMsg">Licensed to this device. Thank you!</div>
        </section>
      </div>
)HTML";

static const char MONITOR_SCRIPT[] = R"JS(
    var POLL_MS = 500;
    var currentDeviceId = '';
    var $ = function (id) { return document.getElementById(id); };

    function pad(n, width) {
      var text = String(n);
      while (text.length < width) text = '0' + text;
      return text;
    }

    function fmtUptime(sec) {
      sec = Math.floor(sec);
      var h = Math.floor(sec / 3600);
      var m = Math.floor((sec % 3600) / 60);
      var s = sec % 60;
      return (h > 0 ? h + 'h ' : '') + m + 'm ' + s + 's';
    }

    function fmtAge(ms) {
      if (ms < 0) return 'no data';
      if (ms < 1000) return 'just now';
      if (ms < 60000) return (ms / 1000).toFixed(1) + ' s ago';
      return Math.floor(ms / 60000) + ' min ago';
    }

    function fmtTrial(sec) {
      sec = Math.max(0, Math.floor(sec));
      if (sec <= 0) return 'expired';
      var h = Math.floor(sec / 3600);
      var m = Math.floor((sec % 3600) / 60);
      var s = sec % 60;
      if (h > 0) return h + 'h ' + m + 'm';
      if (m > 0) return m + 'm ' + s + 's';
      return s + 's';
    }

    function setBadge(online, valid, locked) {
      var badge = $('badge');
      if (locked) {
        badge.textContent = 'Trial expired';
        badge.className = 'chip bad';
      } else if (!online) {
        badge.textContent = 'ESP32 offline';
        badge.className = 'chip bad';
      } else if (valid) {
        badge.textContent = 'Live';
        badge.className = 'chip ok';
      } else {
        badge.textContent = 'Waiting for scale';
        badge.className = 'chip warn';
      }
    }

    function updateLicenseUI(d) {
      currentDeviceId = d.deviceId || '';
      $('deviceId').textContent = currentDeviceId || '...';

      var pill = $('licensePill');
      var locked = !!d.locked;
      $('lockNote').className = 'notice' + (locked ? ' show' : '');

      if (d.license === 'licensed') {
        pill.textContent = 'Licensed';
        pill.className = 'chip ok';
        $('trialLeft').textContent = 'Unlimited';
        $('activateArea').style.display = 'none';
        $('licensedMsg').className = 'ok-box show';
      } else {
        $('licensedMsg').className = 'ok-box';
        $('activateArea').style.display = 'block';
        $('trialLeft').textContent = (d.license === 'expired')
          ? 'expired'
          : fmtTrial(d.trialRemaining);
        pill.textContent = (d.license === 'expired') ? 'Expired' : 'Trial';
        pill.className = 'chip ' + ((d.license === 'expired') ? 'bad' : 'warn');
      }
    }

    function updateCloudUI(d) {
      var chip = $('cloudChip');
      var sta = $('staStatus');

      if (d.locked) {
        chip.textContent = 'Cloud paused';
        chip.className = 'chip warn';
      } else if (d.cloudConfigured && d.staConnected &&
                 d.lastUploadAge >= 0 && d.lastUploadOk === false) {
        chip.textContent = 'Cloud error';
        chip.className = 'chip bad';
      } else if (d.cloudConfigured && d.staConnected) {
        chip.textContent = 'Cloud OK';
        chip.className = 'chip ok';
      } else if (d.cloudConfigured) {
        chip.textContent = 'Cloud offline';
        chip.className = 'chip warn';
      } else {
        chip.textContent = 'Cloud off';
        chip.className = 'chip';
      }

      if (d.staConnected) {
        sta.textContent = d.staIp + '  ' + d.rssi + ' dBm';
      } else if (d.cloudConfigured) {
        sta.textContent = d.staStatus || 'offline';
      } else {
        sta.textContent = 'not configured';
      }

      var age = d.lastUploadAge;
      $('lastUpload').textContent = (age < 0) ? 'never' : fmtAge(age);
      $('uploadCounts').textContent =
        (d.uploadOk || 0) + ' OK / ' + (d.uploadFail || 0) + ' failed';
      $('heroSub').textContent = d.valid
        ? 'Live reading from the scale'
        : 'Menunggu data timbangan...';
    }

    function poll() {
      fetch('/api/status', { cache: 'no-store' })
        .then(function (r) {
          if (!r.ok) throw new Error('HTTP ' + r.status);
          return r.json();
        })
        .then(function (d) {
          var weightEl = $('weight');
          var valueBox = $('valueBox');

          if (d.valid && !d.locked) {
            weightEl.textContent = d.weight.toFixed(1);
            valueBox.className = 'hero-value';
          } else {
            weightEl.textContent = '--';
            valueBox.className = 'hero-value stale';
          }

          $('raw').textContent = d.raw || '-';
          $('frames').textContent = d.frames;
          $('age').textContent = fmtAge(d.age);
          $('uptime').textContent = fmtUptime(d.uptime);

          updateLicenseUI(d);
          updateCloudUI(d);
          setBadge(true, d.valid, d.locked);
        })
        .catch(function () {
          $('weight').textContent = '--';
          $('valueBox').className = 'hero-value stale';
          $('cloudChip').textContent = 'Offline';
          $('cloudChip').className = 'chip bad';
          setBadge(false, false, false);
        });
    }

    function showActivateMsg(text, ok) {
      var el = $('activateMsg');
      el.textContent = text;
      el.className = 'msg' + (ok === true ? ' ok' : (ok === false ? ' err' : ''));
    }

    function activateLicense() {
      var input = $('keyInput');
      var btn = $('activateBtn');
      var key = input.value.trim();

      if (!key) {
        showActivateMsg('Paste the activation key first.', false);
        return;
      }

      if (key.indexOf('.') < 0 || key.length < 40) {
        showActivateMsg('That looks like the Device ID, not an activation key. Paste the long key from your order page.', false);
        return;
      }

      btn.disabled = true;
      input.disabled = true;
      showActivateMsg('Checking key...', null);

      var controller = (typeof AbortController !== 'undefined') ? new AbortController() : null;
      var timeoutId = controller ? setTimeout(function () { controller.abort(); }, 10000) : null;
      var options = {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'key=' + encodeURIComponent(key)
      };
      if (controller) options.signal = controller.signal;

      fetch('/api/activate', options)
        .then(function (r) {
          return r.json().then(function (d) { return { ok: r.ok, data: d }; });
        })
        .then(function (res) {
          if (res.ok && res.data.ok) {
            input.value = '';
            showActivateMsg('Activated. Thank you!', true);
            poll();
          } else {
            showActivateMsg(res.data.error || 'Invalid key for this device.', false);
          }
        })
        .catch(function (err) {
          var msg = (err && err.name === 'AbortError')
            ? 'Connection timeout. Reconnect to A12E_SCALE_MONITOR and try again.'
            : 'Connection error. Reconnect to A12E_SCALE_MONITOR and try again.';
          showActivateMsg(msg, false);
        })
        .finally(function () {
          if (timeoutId) clearTimeout(timeoutId);
          btn.disabled = false;
          input.disabled = false;
        });
    }

    function copyBuyLink(btn) {
      var input = $('buyUrl');
      if (!input) return;
      var text = input.value;
      var old = btn.textContent;
      var done = function () {
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = old; }, 1500);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done).catch(function () {
          input.focus();
          input.select();
          input.setSelectionRange(0, text.length);
          btn.textContent = 'Select';
          setTimeout(function () { btn.textContent = old; }, 1500);
        });
      } else {
        input.focus();
        input.select();
        input.setSelectionRange(0, text.length);
        try { document.execCommand('copy'); done(); } catch (e) {
          btn.textContent = 'Select';
          setTimeout(function () { btn.textContent = old; }, 1500);
        }
      }
    }

    function copyDeviceId(btn) {      var text = currentDeviceId;
      if (!text) return;
      var old = btn.textContent;
      var done = function () {
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = old; }, 1500);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done).catch(function () {
          btn.textContent = 'Select ID';
          setTimeout(function () { btn.textContent = old; }, 1500);
        });
      } else {
        var area = document.createElement('textarea');
        area.value = text;
        document.body.appendChild(area);
        area.select();
        try { document.execCommand('copy'); done(); } catch (e) {}
        document.body.removeChild(area);
      }
    }

    poll();
    setInterval(poll, POLL_MS);
)JS";

#endif // MONITOR_PAGE_H

/**
 * @file debug_page.h
 * @brief Debug log page body + client script.
 */

#ifndef DEBUG_PAGE_H
#define DEBUG_PAGE_H

#include <Arduino.h>

static const char DEBUG_BODY[] = R"HTML(
      <section class="card">
        <h2>Live device log</h2>
        <div class="toolbar">
          <button class="btn secondary small" id="pauseBtn" type="button">Pause</button>
          <button class="btn secondary small" id="clearBtn" type="button">Clear view</button>
          <button class="btn secondary small" id="copyBtn" type="button">Copy</button>
          <label class="note" style="display:flex;align-items:center;gap:6px">
            <input type="checkbox" id="autoScroll" checked> Auto-scroll
          </label>
          <span class="status" id="status">connecting...</span>
        </div>

        <pre class="console" id="log" tabindex="0"></pre>

        <p class="note" style="margin-top:12px">
          RAM buffer, last ~48 lines. A reboot clears it. The same lines are also
          printed to the USB serial port at 115200 baud. Timestamps are uptime,
          not wall-clock time.
        </p>
      </section>
)HTML";

static const char DEBUG_SCRIPT[] = R"JS(
    var logEl = document.getElementById('log');
    var statusEl = document.getElementById('status');
    var pauseBtn = document.getElementById('pauseBtn');
    var autoScroll = document.getElementById('autoScroll');
    var since = 0;
    var paused = false;
    var timer = null;

    function pad(n, width) {
      var text = String(n);
      while (text.length < width) text = '0' + text;
      return text;
    }

    function fmtTime(ms) {
      var total = Math.floor(ms / 1000);
      var h = Math.floor(total / 3600);
      var m = Math.floor((total % 3600) / 60);
      var s = total % 60;
      var milli = Math.floor(ms % 1000);
      return pad(h, 2) + ':' + pad(m, 2) + ':' + pad(s, 2) + '.' + pad(milli, 3);
    }

    function append(data) {
      if (data.lines && data.lines.length > 0) {
        var text = '';
        for (var i = 0; i < data.lines.length; i++) {
          text += '[' + fmtTime(data.lines[i].ms) + '] ' +
                  data.lines[i].text + '\n';
        }
        logEl.textContent += text;
        if (autoScroll.checked) {
          logEl.scrollTop = logEl.scrollHeight;
        }
      }
      if (typeof data.next === 'number') {
        since = data.next;
      }
    }

    function poll() {
      if (paused) return;
      fetch('/api/log?since=' + since, { cache: 'no-store' })
        .then(function (response) {
          if (!response.ok) throw new Error('HTTP ' + response.status);
          return response.json();
        })
        .then(function (data) {
          append(data);
          statusEl.textContent = 'live' +
            (data.dropped ? ' | ' + data.dropped + ' dropped' : '');
        })
        .catch(function () {
          statusEl.textContent = 'disconnected';
        });
    }

    function start() {
      if (timer || paused) return;
      poll();
      timer = setInterval(poll, 1200);
    }

    function stop() {
      if (timer) {
        clearInterval(timer);
        timer = null;
      }
    }

    pauseBtn.addEventListener('click', function () {
      paused = !paused;
      pauseBtn.textContent = paused ? 'Resume' : 'Pause';
      if (paused) {
        stop();
        statusEl.textContent = 'paused';
      } else {
        start();
      }
    });

    document.getElementById('clearBtn').addEventListener('click', function () {
      logEl.textContent = '';
    });

    document.getElementById('copyBtn').addEventListener('click', function () {
      var text = logEl.textContent;
      var btn = this;
      var done = function () {
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = 'Copy'; }, 1200);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done).catch(function () {
          logEl.focus();
        });
      } else {
        var area = document.createElement('textarea');
        area.value = text;
        document.body.appendChild(area);
        area.select();
        try { document.execCommand('copy'); done(); } catch (e) { logEl.focus(); }
        document.body.removeChild(area);
      }
    });

    document.addEventListener('visibilitychange', function () {
      if (document.hidden) {
        stop();
      } else if (!paused) {
        start();
      }
    });

    start();
)JS";

#endif // DEBUG_PAGE_H

/**
 * @file debug_page.h
 * @brief Local web page for viewing the in-RAM debug log.
 */

#ifndef DEBUG_PAGE_H
#define DEBUG_PAGE_H

#include <Arduino.h>

static const char DEBUG_HTML[] PROGMEM = R"DEBUGHTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>A12E Debug Log</title>
  <style>
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: #0e1120;
      color: #dfe4ff;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }
    .wrap { max-width: 980px; margin: 0 auto; padding: 18px; }
    header {
      display: flex; align-items: center; justify-content: space-between;
      gap: 12px; margin-bottom: 14px;
    }
    h1 { font-size: 18px; margin: 0; }
    a { color: #9fb0ff; text-decoration: none; font-size: 13px; }
    a:hover { text-decoration: underline; }
    .toolbar {
      display: flex; flex-wrap: wrap; align-items: center;
      gap: 8px; margin-bottom: 10px;
    }
    button {
      border: 1px solid #2b3358;
      background: #171c30;
      color: #dfe4ff;
      border-radius: 8px;
      padding: 7px 11px;
      font-size: 12px;
      font-weight: 600;
      cursor: pointer;
    }
    button:hover { background: #202845; }
    label {
      display: flex; align-items: center; gap: 6px;
      font-size: 12px; color: #9aa3c7;
    }
    #status { margin-left: auto; font-size: 12px; color: #8a93b8; }
    #log {
      margin: 0;
      padding: 12px;
      height: 68vh;
      overflow: auto;
      background: #080a12;
      border: 1px solid #232946;
      border-radius: 10px;
      white-space: pre-wrap;
      word-break: break-word;
      font-family: 'Courier New', monospace;
      font-size: 12px;
      line-height: 1.55;
    }
    #log:empty::before {
      content: 'Waiting for log lines...';
      color: #555e80;
    }
    .hint {
      margin-top: 10px;
      color: #8a93b8;
      font-size: 11px;
      line-height: 1.6;
    }
    @media (max-width: 520px) {
      .wrap { padding: 10px; }
      header { flex-direction: column; align-items: flex-start; }
      #status { margin-left: 0; }
      #log { height: 62vh; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <header>
      <h1>&#9878;&#65039; A12E Debug Log</h1>
      <a href="/">&larr; Back to monitor</a>
    </header>

    <div class="toolbar">
      <button id="pauseBtn" type="button">Pause</button>
      <button id="clearBtn" type="button">Clear view</button>
      <button id="copyBtn" type="button">Copy</button>
      <label><input type="checkbox" id="autoScroll" checked> Auto-scroll</label>
      <span id="status">connecting...</span>
    </div>

    <pre id="log" tabindex="0"></pre>

    <div class="hint">
      RAM buffer, last ~48 lines. A reboot clears it. The same lines are also
      printed to the USB serial port at 115200 baud. Timestamps are uptime,
      not wall-clock time.
    </div>
  </div>

  <script>
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
  </script>
</body>
</html>)DEBUGHTML";

#endif // DEBUG_PAGE_H

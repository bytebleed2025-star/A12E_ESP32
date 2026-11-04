/**
 * HTML pages. All user data is loaded through JSON APIs and inserted with
 * textContent / escaping, so page templates contain no user input.
 */

const COMMON_CSS = `
    :root {
      --brand: #667eea;
      --brand2: #764ba2;
      --ink: #2a2d3a;
      --muted: #8a8fa3;
      --line: #e8eaf2;
      --ok: #2e7d32;
      --ok-bg: #e8f5e9;
      --err: #c62828;
      --err-bg: #ffebee;
      --warn: #8a6d00;
      --warn-bg: #fff8e1;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 18px;
      min-height: 100vh;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      color: var(--ink);
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      display: flex;
      align-items: flex-start;
      justify-content: center;
    }
    .card {
      width: 100%;
      max-width: 620px;
      background: #fff;
      border-radius: 18px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.28);
      padding: 28px;
    }
    .brand {
      font-size: 15px;
      font-weight: 700;
      color: var(--brand);
      letter-spacing: 0.5px;
      margin-bottom: 16px;
    }
    h1 { font-size: 24px; margin: 0 0 6px; }
    h2 { font-size: 16px; margin: 0 0 10px; }
    .sub { color: var(--muted); font-size: 13px; margin: 0 0 22px; line-height: 1.6; }
    .hint { color: var(--muted); font-size: 12px; line-height: 1.5; margin: 6px 0 0; }
    label {
      display: block;
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.5px;
      text-transform: uppercase;
      color: #555b70;
      margin: 16px 0 6px;
    }
    label .opt { font-weight: 400; text-transform: none; color: var(--muted); }
    input, textarea, button { font: inherit; }
    input[type=text], textarea {
      width: 100%;
      padding: 11px 12px;
      border: 2px solid var(--line);
      border-radius: 9px;
      outline: none;
      background: #fbfbff;
      transition: border-color 0.2s ease;
    }
    input[type=text]:focus, textarea:focus { border-color: var(--brand); background: #fff; }
    textarea { resize: vertical; }
    button {
      border: none;
      border-radius: 9px;
      padding: 12px 16px;
      background: var(--brand);
      color: #fff;
      font-weight: 700;
      font-size: 13px;
      letter-spacing: 0.6px;
      cursor: pointer;
      transition: background 0.2s ease, opacity 0.2s ease;
    }
    button:hover:not(:disabled) { background: #5a6fd6; }
    button:disabled { opacity: 0.6; cursor: not-allowed; }
    button.secondary { background: #eef1ff; color: #4b5bd6; }
    button.secondary:hover:not(:disabled) { background: #e1e6ff; }
    button.danger { background: var(--err-bg); color: var(--err); }
    button.danger:hover:not(:disabled) { background: #ffdada; }
    .full { width: 100%; margin-top: 18px; }
    .box {
      margin-top: 20px;
      padding: 16px;
      border: 1px solid var(--line);
      border-radius: 12px;
      background: #fafbff;
    }
    .box.success { background: var(--ok-bg); border-color: #c8e6c9; }
    .box.error { background: var(--err-bg); border-color: #ffcdd2; }
    .box.warn { background: var(--warn-bg); border-color: #ffe082; }
    .notice {
      display: none;
      margin: 16px 0 0;
      padding: 11px 13px;
      border-radius: 9px;
      font-size: 13px;
      line-height: 1.5;
    }
    .notice.show { display: block; }
    .notice.ok { background: var(--ok-bg); color: var(--ok); }
    .notice.err { background: var(--err-bg); color: var(--err); }
    .notice.info { background: #eef2ff; color: #4b5bd6; }
    .payinfo {
      white-space: pre-wrap;
      word-break: break-word;
      font-family: 'Courier New', monospace;
      font-size: 13px;
      line-height: 1.6;
      background: #fff;
      border: 1px dashed #d9dced;
      border-radius: 9px;
      padding: 12px;
      margin: 10px 0 0;
    }
    .price {
      display: inline-block;
      padding: 5px 10px;
      border-radius: 999px;
      background: #eef2ff;
      color: #4b5bd6;
      font-weight: 700;
      font-size: 13px;
    }
    .pill {
      display: inline-block;
      padding: 4px 10px;
      border-radius: 999px;
      background: #eef2ff;
      color: #4b5bd6;
      font-size: 10px;
      font-weight: 800;
      letter-spacing: 1px;
      text-transform: uppercase;
    }
    .pill.paid { background: var(--ok-bg); color: var(--ok); }
    .pill.cancelled { background: var(--err-bg); color: var(--err); }
    .pill.pending { background: var(--warn-bg); color: var(--warn); }
    .kv {
      display: flex;
      justify-content: space-between;
      gap: 14px;
      font-size: 13px;
      padding: 7px 0;
      border-bottom: 1px solid #f0f1f7;
    }
    .kv:last-child { border-bottom: 0; }
    .kv .k { color: var(--muted); white-space: nowrap; }
    .kv .v {
      font-family: 'Courier New', monospace;
      font-weight: 700;
      text-align: right;
      word-break: break-all;
    }
    .keybox {
      width: 100%;
      min-height: 96px;
      margin-top: 10px;
      padding: 10px;
      border: 1px solid #c8e6c9;
      border-radius: 9px;
      background: #fff;
      font-family: 'Courier New', monospace;
      font-size: 12px;
      line-height: 1.5;
      word-break: break-all;
    }
    .steps { margin-top: 22px; }
    .steps ol { margin: 8px 0 0 20px; padding: 0; color: #555b70; font-size: 13px; line-height: 1.7; }
    .back { display: inline-block; margin-top: 20px; color: var(--brand); font-size: 13px; text-decoration: none; }
    .back:hover { text-decoration: underline; }
    .order-top { display: flex; justify-content: space-between; align-items: center; gap: 10px; margin-bottom: 10px; }
    .oid { font-family: 'Courier New', monospace; font-size: 11px; color: var(--muted); word-break: break-all; text-align: right; }

    /* admin */
    .toolbar { display: flex; justify-content: space-between; align-items: center; gap: 10px; margin: 18px 0; }
    .stats { font-size: 13px; color: var(--muted); }
    .stats b { color: var(--ink); }
    .admin-order {
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 14px;
      margin-bottom: 12px;
      background: #fff;
    }
    .admin-order.pending { border-left: 4px solid #ffc107; }
    .admin-order.paid { border-left: 4px solid #4caf50; }
    .admin-order.cancelled { border-left: 4px solid #f44336; opacity: 0.8; }
    .actions { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }
    .actions button { padding: 8px 12px; font-size: 11px; }
    .contact { font-size: 12px; color: #555b70; word-break: break-word; white-space: pre-wrap; margin-top: 8px; }
    .row { display: flex; gap: 8px; }
    .row input { flex: 1; min-width: 0; }
    .row button { white-space: nowrap; }

    @media (max-width: 520px) {
      body { padding: 10px; }
      .card { padding: 20px; border-radius: 14px; }
      h1 { font-size: 21px; }
      .row { flex-direction: column; }
      .row button { padding: 11px; }
    }
`;

function shell(title, body, script) {
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>${title}</title>
<style>${COMMON_CSS}</style>
</head>
<body>
<div class="card">
${body}
</div>
<script>
${script}
</script>
</body>
</html>`;
}

// ============================================================================
// Public: create order
// ============================================================================

export function activatePage() {
  const body = `
    <div class="brand">&#9878;&#65039; <span id="siteName">A12E Scale Monitor</span></div>
    <h1>Activate your device</h1>
    <p class="sub">
      Your monitor includes a free trial. To keep using it, order a permanent
      activation key for this device. The key is tied to the Device ID shown on
      the monitor and works offline.
    </p>

    <form id="orderForm">
      <label for="deviceId">Device ID</label>
      <input type="text" id="deviceId" maxlength="17" placeholder="A12E-A1B2C3D4E5F6"
             autocomplete="off" spellcheck="false" required>
      <p class="hint">Shown on the monitor page under &ldquo;Device ID&rdquo;.</p>

      <label for="contact">WhatsApp / Email</label>
      <input type="text" id="contact" maxlength="120" placeholder="08xx-xxxx-xxxx or name@email.com"
             autocomplete="off" required>

      <label for="note">Note <span class="opt">(optional)</span></label>
      <textarea id="note" maxlength="500" rows="2" placeholder="Company / shop name, etc."></textarea>

      <button type="submit" id="submitBtn" class="full">Continue to payment</button>
    </form>

    <div class="notice" id="msg"></div>

    <div class="box">
      <h2>Payment</h2>
      <div class="price" id="priceLabel">Loading...</div>
      <pre class="payinfo" id="paymentInfo"></pre>
      <p class="hint" id="supportLine"></p>
    </div>

    <div class="steps">
      <h2>How it works</h2>
      <ol>
        <li>Submit this form with the Device ID from your monitor.</li>
        <li>Pay using the instructions above.</li>
        <li>We confirm the payment and generate a key for your device.</li>
        <li>Open your order page, copy the key, and paste it into the License box on the monitor.</li>
      </ol>
    </div>
  `;

  const script = `
    var DEVICE_RE = /^A12E-[0-9A-F]{12}$/;
    var msg = document.getElementById('msg');
    var form = document.getElementById('orderForm');
    var deviceInput = document.getElementById('deviceId');
    var submitBtn = document.getElementById('submitBtn');

    function showMsg(text, kind) {
      msg.textContent = text;
      msg.className = 'notice show ' + (kind || 'info');
    }

    function cleanDevice() {
      deviceInput.value = deviceInput.value.toUpperCase().replace(/[^A-Z0-9-]/g, '');
    }

    deviceInput.addEventListener('input', cleanDevice);

    var params = new URLSearchParams(location.search);
    if (params.get('device')) {
      deviceInput.value = params.get('device').toUpperCase();
      cleanDevice();
    }

    fetch('/api/config', { cache: 'no-store' })
      .then(function (r) { return r.json(); })
      .then(function (c) {
        document.getElementById('siteName').textContent = c.siteName;
        document.getElementById('priceLabel').textContent = c.priceLabel || 'Contact us';
        document.getElementById('paymentInfo').textContent = c.paymentInfo || '';
        document.getElementById('supportLine').textContent =
          c.supportContact ? ('Support: ' + c.supportContact) : '';
        document.title = c.siteName + ' - Activation';
      })
      .catch(function () { /* page still works with defaults */ });

    form.addEventListener('submit', function (event) {
      event.preventDefault();
      cleanDevice();

      if (!DEVICE_RE.test(deviceInput.value)) {
        showMsg('Device ID must look like A12E-A1B2C3D4E5F6 (from the monitor page).', 'err');
        deviceInput.focus();
        return;
      }

      var contact = document.getElementById('contact').value.trim();
      if (contact.length < 3) {
        showMsg('Please enter a WhatsApp number or email address.', 'err');
        return;
      }

      submitBtn.disabled = true;
      submitBtn.textContent = 'Creating order...';
      showMsg('Creating your order...', 'info');

      fetch('/api/orders', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          deviceId: deviceInput.value,
          contact: contact,
          note: document.getElementById('note').value.trim()
        })
      })
        .then(function (r) {
          return r.json().then(function (d) { return { ok: r.ok, data: d }; });
        })
        .then(function (res) {
          if (!res.ok) throw new Error(res.data.error || 'Could not create the order');
          location.href = '/order/' + res.data.id;
        })
        .catch(function (err) {
          showMsg(err.message || 'Connection error. Try again.', 'err');
          submitBtn.disabled = false;
          submitBtn.textContent = 'Continue to payment';
        });
    });
  `;

  return shell('A12E Activation', body, script);
}

// ============================================================================
// Public: order status / key
// ============================================================================

export function orderPage(orderId) {
  const body = `
    <div class="brand">&#9878;&#65039; <span id="siteName">A12E Scale Monitor</span></div>
    <h1>Order status</h1>
    <p class="sub">
      Keep this page. It shows the payment status and your activation key.
    </p>

    <div class="box">
      <div class="kv"><span class="k">Status</span><span id="statusPill" class="pill pending">pending</span></div>
      <div class="kv"><span class="k">Device ID</span><span class="v" id="deviceId">...</span></div>
      <div class="kv"><span class="k">Amount</span><span class="v" id="amount">...</span></div>
      <div class="kv"><span class="k">Order</span><span class="v" id="orderId">${orderId}</span></div>
    </div>

    <div class="notice" id="msg"></div>

    <div class="box" id="pendingBox">
      <h2>Waiting for payment</h2>
      <p class="hint" style="margin-top:0">Pay using the instructions below. This page checks automatically.</p>
      <pre class="payinfo" id="paymentInfo"></pre>
      <p class="hint" id="supportLine"></p>
      <button class="secondary" id="refreshBtn" style="margin-top:12px">Check again</button>
    </div>

    <div class="box success" id="paidBox" style="display:none">
      <h2>Your activation key</h2>
      <p class="hint" style="margin-top:0">Copy this key and paste it into the License box on the monitor page.</p>
      <textarea class="keybox" id="licenseKey" readonly></textarea>
      <button id="copyBtn" style="margin-top:10px">Copy key</button>
      <p class="hint">This key only works on the Device ID shown above.</p>
    </div>

    <div class="box error" id="cancelBox" style="display:none">
      <h2>Order cancelled</h2>
      <p class="hint" style="margin-top:0">Contact support if you already paid.</p>
    </div>

    <a class="back" href="/activate">&larr; New order</a>
  `;

  const script = `
    var orderId = location.pathname.split('/').pop();
    var timer = null;
    var finished = false;
    var msg = document.getElementById('msg');

    function showMsg(text, kind) {
      msg.textContent = text;
      msg.className = 'notice show ' + (kind || 'info');
    }

    function stopPolling() {
      if (timer) { clearInterval(timer); timer = null; }
    }

    function copyKey() {
      var area = document.getElementById('licenseKey');
      var btn = document.getElementById('copyBtn');
      var text = area.value;
      var done = function () {
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = 'Copy key'; }, 1500);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done).catch(function () {
          area.select();
          try { document.execCommand('copy'); done(); } catch (e) { area.focus(); }
        });
      } else {
        area.select();
        area.setSelectionRange(0, area.value.length);
        try { document.execCommand('copy'); done(); } catch (e) { area.focus(); }
      }
    }

    document.getElementById('copyBtn').addEventListener('click', copyKey);
    document.getElementById('refreshBtn').addEventListener('click', function () { poll(); });

    function paint(d) {
      document.getElementById('deviceId').textContent = d.deviceId;
      document.getElementById('amount').textContent = d.amount || '-';

      var pill = document.getElementById('statusPill');
      pill.textContent = d.status;
      pill.className = 'pill ' + d.status;

      document.getElementById('pendingBox').style.display = 'none';
      document.getElementById('paidBox').style.display = 'none';
      document.getElementById('cancelBox').style.display = 'none';

      if (d.status === 'paid') {
        document.getElementById('licenseKey').value = d.licenseKey || '';
        document.getElementById('paidBox').style.display = 'block';
        finished = true;
        stopPolling();
      } else if (d.status === 'cancelled') {
        document.getElementById('cancelBox').style.display = 'block';
        finished = true;
        stopPolling();
      } else {
        document.getElementById('pendingBox').style.display = 'block';
      }
    }

    function poll() {
      fetch('/api/orders/' + encodeURIComponent(orderId), { cache: 'no-store' })
        .then(function (r) {
          if (r.status === 404) throw new Error('Order not found. Check the link.');
          return r.json();
        })
        .then(function (d) {
          msg.className = 'notice';
          paint(d);
        })
        .catch(function (err) {
          showMsg(err.message || 'Connection error. Retrying...', 'err');
        });
    }

    function startPolling() {
      if (finished || timer) return;
      // 10 s is enough for payment confirmation and keeps the free Workers
      // request quota low. Hidden tabs stop polling entirely.
      timer = setInterval(poll, 10000);
    }

    fetch('/api/config', { cache: 'no-store' })
      .then(function (r) { return r.json(); })
      .then(function (c) {
        document.getElementById('paymentInfo').textContent = c.paymentInfo || '';
        document.getElementById('supportLine').textContent =
          c.supportContact ? ('Support: ' + c.supportContact) : '';
        document.getElementById('siteName').textContent = c.siteName;
      })
      .catch(function () { /* ignore */ });

    poll();
    startPolling();

    document.addEventListener('visibilitychange', function () {
      if (document.hidden) {
        stopPolling();
      } else if (!finished) {
        poll();
        startPolling();
      }
    });
  `;

  return shell('A12E Order', body, script);
}

// ============================================================================
// Admin
// ============================================================================

export function adminPage() {
  const body = `
    <div class="brand">&#9878;&#65039; A12E License Admin</div>
    <h1>Orders</h1>
    <p class="sub">Confirm payments, generate keys, and issue manual keys.</p>

    <div class="toolbar">
      <span class="stats" id="stats">Loading...</span>
      <button class="secondary" onclick="load()">Refresh</button>
    </div>

    <div class="notice" id="msg"></div>

    <div class="box">
      <h2>Manual key</h2>
      <div class="row">
        <input type="text" id="manualDevice" maxlength="17" placeholder="A12E-A1B2C3D4E5F6"
               autocomplete="off" spellcheck="false">
        <button onclick="manualSign()">Generate key</button>
      </div>
      <p class="hint">For phone/bank-transfer sales. This also records a paid order.</p>
      <textarea class="keybox" id="manualKey" readonly style="display:none"></textarea>
      <button id="manualCopy" class="secondary" style="display:none;margin-top:10px"
              onclick="copyText(document.getElementById('manualKey').value, this)">Copy</button>
    </div>

    <h2 style="margin-top:24px">All orders</h2>
    <div id="orders"></div>
  `;

  const script = `
    var keyMap = {};
    var msg = document.getElementById('msg');
    var DEVICE_RE = /^A12E-[0-9A-F]{12}$/;

    function esc(value) {
      return String(value == null ? '' : value).replace(/[&<>"']/g, function (c) {
        return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
      });
    }

    function setMsg(text, kind) {
      msg.textContent = text || '';
      msg.className = 'notice' + (text ? ' show ' + (kind || 'info') : '');
    }

    function fmtDate(ms) {
      if (!ms) return '-';
      try { return new Date(ms).toLocaleString(); } catch (e) { return String(ms); }
    }

    function copyText(text, btn) {
      var done = function () {
        if (!btn) return;
        var old = btn.textContent;
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = old; }, 1500);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done).catch(function () {});
      } else {
        var ta = document.createElement('textarea');
        ta.value = text;
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand('copy'); done(); } catch (e) {}
        document.body.removeChild(ta);
      }
    }

    function copyOrderKey(id, btn) {
      copyText(keyMap[id] || '', btn);
    }

    async function api(path, options) {
      var response = await fetch(path, options || {});
      var data = await response.json().catch(function () { return {}; });
      if (!response.ok) {
        throw new Error(data.error || ('HTTP ' + response.status));
      }
      return data;
    }

    async function load() {
      try {
        var data = await api('/api/admin/orders', { cache: 'no-store' });
        var orders = data.orders || [];
        var pending = 0;
        var paid = 0;
        keyMap = {};

        orders.forEach(function (o) {
          if (o.status === 'pending') pending++;
          if (o.status === 'paid') paid++;
          if (o.license_key) keyMap[o.id] = o.license_key;
        });

        document.getElementById('stats').innerHTML =
          '<b>' + pending + '</b> pending &middot; <b>' + paid + '</b> paid &middot; ' +
          orders.length + ' total';

        if (orders.length === 0) {
          document.getElementById('orders').innerHTML =
            '<p class="hint">No orders yet.</p>';
          return;
        }

        var html = '';
        orders.forEach(function (o) {
          var keyBox = '';
          if (o.license_key) {
            keyBox = '<p class="contact">Key: <span style="font-family:monospace;word-break:break-all">' +
              esc(o.license_key) + '</span></p>';
          }

          var actions = '';
          if (o.status !== 'paid') {
            actions += '<button onclick="markPaid(\\'' + o.id + '\\')">PAID + KEY</button>';
          }
          if (o.status === 'pending') {
            actions += '<button class="danger" onclick="cancelOrder(\\'' + o.id + '\\')">Cancel</button>';
          }
          if (o.license_key) {
            actions += '<button class="secondary" onclick="copyOrderKey(\\'' + o.id + '\\', this)">Copy key</button>';
          }

          html += '<div class="admin-order ' + esc(o.status) + '">' +
            '<div class="order-top"><span class="pill ' + esc(o.status) + '">' + esc(o.status) + '</span>' +
            '<span class="oid">' + esc(o.id) + '</span></div>' +
            '<div class="kv"><span class="k">Device</span><span class="v">' + esc(o.device_id) + '</span></div>' +
            '<div class="kv"><span class="k">Amount</span><span class="v">' + esc(o.amount || '-') + '</span></div>' +
            '<div class="kv"><span class="k">Created</span><span class="v">' + esc(fmtDate(o.created_at)) + '</span></div>' +
            '<div class="kv"><span class="k">Paid</span><span class="v">' + esc(fmtDate(o.paid_at)) + '</span></div>' +
            '<div class="contact"><b>Contact:</b> ' + esc(o.contact) + '</div>' +
            (o.note ? '<div class="contact"><b>Note:</b> ' + esc(o.note) + '</div>' : '') +
            keyBox +
            '<div class="actions">' + actions + '</div>' +
            '</div>';
        });

        document.getElementById('orders').innerHTML = html;
      } catch (err) {
        setMsg(err.message || 'Could not load orders.', 'err');
      }
    }

    async function markPaid(id) {
      if (!confirm('Mark this order as paid and generate the activation key?')) return;
      setMsg('Generating key...', 'info');
      try {
        await api('/api/admin/orders/' + id + '/paid', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: '{}'
        });
        setMsg('Key generated.', 'ok');
        load();
      } catch (err) {
        setMsg(err.message, 'err');
      }
    }

    async function cancelOrder(id) {
      if (!confirm('Cancel this order?')) return;
      setMsg('Cancelling...', 'info');
      try {
        await api('/api/admin/orders/' + id + '/cancel', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: '{}'
        });
        setMsg('Order cancelled.', 'ok');
        load();
      } catch (err) {
        setMsg(err.message, 'err');
      }
    }

    async function manualSign() {
      var input = document.getElementById('manualDevice');
      var device = input.value.trim().toUpperCase();
      input.value = device;

      if (!DEVICE_RE.test(device)) {
        setMsg('Device ID must look like A12E-A1B2C3D4E5F6.', 'err');
        return;
      }

      setMsg('Generating key...', 'info');
      try {
        var data = await api('/api/admin/sign', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ deviceId: device })
        });
        var area = document.getElementById('manualKey');
        area.value = data.key;
        area.style.display = 'block';
        document.getElementById('manualCopy').style.display = 'inline-block';
        setMsg('Manual key generated and recorded.', 'ok');
        load();
      } catch (err) {
        setMsg(err.message, 'err');
      }
    }

    load();
  `;

  return shell('A12E License Admin', body, script);
}

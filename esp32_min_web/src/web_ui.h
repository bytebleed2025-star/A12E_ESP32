/**
 * @file web_ui.h
 * @brief Shared modern UI shell + CSS for Monitor / Debug / Config.
 */

#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

static const char UI_CSS[] PROGMEM = R"CSS(
    :root {
      --bg: #080b14;
      --text: #eef1ff;
      --muted: #9aa3c7;
      --line: rgba(255, 255, 255, 0.12);
      --panel: rgba(255, 255, 255, 0.055);
      --panel-2: rgba(255, 255, 255, 0.085);
      --accent: #7c8cff;
      --accent-2: #b06cff;
      --ok: #48d597;
      --warn: #ffc857;
      --err: #ff6b7a;
    }
    * { box-sizing: border-box; }
    html, body { min-height: 100%; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      -webkit-font-smoothing: antialiased;
    }
    body::before {
      content: '';
      position: fixed;
      inset: 0;
      z-index: -1;
      background:
        radial-gradient(900px 520px at 12% -10%, rgba(124, 140, 255, 0.30), transparent 60%),
        radial-gradient(780px 480px at 100% 0%, rgba(176, 108, 255, 0.22), transparent 55%),
        linear-gradient(160deg, #080b14 0%, #0e1330 52%, #140d26 100%);
    }
    .app { max-width: 1080px; margin: 0 auto; padding: 22px; }
    .top {
      display: flex; align-items: center; justify-content: space-between;
      gap: 16px; flex-wrap: wrap; margin-bottom: 20px;
    }
    .brand { display: flex; align-items: center; gap: 11px; font-weight: 800; font-size: 16px; }
    .brand .logo {
      width: 38px; height: 38px; border-radius: 12px;
      display: grid; place-items: center; font-size: 19px;
      background: linear-gradient(135deg, var(--accent), var(--accent-2));
      box-shadow: 0 10px 24px rgba(124, 140, 255, 0.35);
    }
    .brand small {
      display: block; font-size: 11px; font-weight: 500;
      color: var(--muted); letter-spacing: 0.4px; margin-top: 1px;
    }
    .tabs {
      display: flex; gap: 5px; padding: 5px;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--line); border-radius: 14px;
    }
    .tab {
      padding: 8px 14px; border-radius: 10px;
      color: var(--muted); font-size: 12px; font-weight: 700;
      letter-spacing: 0.4px; text-decoration: none;
      transition: all 0.2s ease;
    }
    .tab:hover { color: var(--text); }
    .tab.active {
      color: #fff;
      background: linear-gradient(135deg, var(--accent), var(--accent-2));
      box-shadow: 0 8px 20px rgba(124, 140, 255, 0.28);
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 18px;
      padding: 20px;
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.22);
    }
    .grid { display: grid; grid-template-columns: 1.25fr 0.75fr; gap: 16px; }
    .grid.two { grid-template-columns: 1fr 1fr; margin-top: 16px; }
    .grid.three { grid-template-columns: repeat(3, 1fr); gap: 16px; }
    h1, h2, h3 { margin: 0; }
    h2 {
      font-size: 12px; text-transform: uppercase; letter-spacing: 1.3px;
      color: var(--muted); margin-bottom: 12px;
    }
    .hero {
      position: relative; overflow: hidden;
      background: linear-gradient(135deg, rgba(124, 140, 255, 0.26), rgba(176, 108, 255, 0.16));
    }
    .hero::after {
      content: ''; position: absolute; width: 260px; height: 260px;
      right: -90px; top: -100px; border-radius: 50%;
      background: radial-gradient(circle, rgba(255, 255, 255, 0.20), transparent 65%);
      pointer-events: none;
    }
    .hero-top { display: flex; justify-content: space-between; align-items: center; gap: 10px; position: relative; z-index: 1; }
    .hero-label { font-size: 11px; letter-spacing: 2.2px; text-transform: uppercase; color: var(--muted); margin-top: 22px; }
    .hero-value {
      font-family: 'Courier New', monospace;
      font-size: 66px; font-weight: 800; line-height: 1.02;
      letter-spacing: -1px; margin: 8px 0 4px;
      position: relative; z-index: 1;
      transition: opacity 0.25s ease;
    }
    .hero-value.stale { opacity: 0.32; }
    .hero-value .unit { font-size: 20px; font-weight: 600; color: var(--muted); margin-left: 8px; letter-spacing: 0; }
    .hero-sub { color: var(--muted); font-size: 12px; position: relative; z-index: 1; }
    .chip {
      display: inline-flex; align-items: center; gap: 6px;
      padding: 5px 10px; border-radius: 999px;
      font-size: 10px; font-weight: 800; letter-spacing: 1px; text-transform: uppercase;
      background: rgba(255, 255, 255, 0.08); color: var(--muted);
      border: 1px solid var(--line); white-space: nowrap;
    }
    .chip.ok { background: rgba(72, 213, 151, 0.12); color: var(--ok); border-color: rgba(72, 213, 151, 0.28); }
    .chip.warn { background: rgba(255, 200, 87, 0.12); color: var(--warn); border-color: rgba(255, 200, 87, 0.28); }
    .chip.bad { background: rgba(255, 107, 122, 0.12); color: var(--err); border-color: rgba(255, 107, 122, 0.28); }
    .kv {
      display: flex; justify-content: space-between; gap: 14px;
      padding: 9px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.06);
      font-size: 13px;
    }
    .kv:last-child { border-bottom: 0; }
    .kv .k { color: var(--muted); white-space: nowrap; }
    .kv .v {
      font-family: 'Courier New', monospace; font-weight: 700;
      text-align: right; word-break: break-all;
    }
    .mono { font-family: 'Courier New', monospace; }
    .muted { color: var(--muted); }
    .field { display: block; margin-bottom: 13px; }
    .field > span {
      display: block; margin-bottom: 6px;
      font-size: 11px; font-weight: 700; letter-spacing: 1px;
      text-transform: uppercase; color: var(--muted);
    }
    .field input, .field textarea {
      width: 100%; padding: 11px 12px;
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid var(--line); border-radius: 10px;
      color: var(--text); outline: none;
      transition: border-color 0.2s ease, box-shadow 0.2s ease;
    }
    .field input:focus, .field textarea:focus {
      border-color: rgba(124, 140, 255, 0.85);
      box-shadow: 0 0 0 3px rgba(124, 140, 255, 0.15);
    }
    .field input::placeholder, .field textarea::placeholder { color: #6f7899; }
    .field input[type=number] { font-family: 'Courier New', monospace; }
    .row { display: flex; gap: 8px; align-items: center; }
    .row > input { flex: 1; min-width: 0; }
    .btn {
      display: inline-block;
      border: 0; border-radius: 10px; padding: 11px 16px;
      background: linear-gradient(135deg, var(--accent), var(--accent-2));
      color: #fff; text-decoration: none;
      font-size: 12px; font-weight: 800; letter-spacing: 0.5px;
      cursor: pointer; transition: filter 0.2s ease, opacity 0.2s ease;
    }
    .btn:hover:not(:disabled) { filter: brightness(1.08); }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; }
    .btn.secondary {
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid var(--line); color: var(--text);
    }
    .btn.danger {
      background: rgba(255, 107, 122, 0.14);
      border: 1px solid rgba(255, 107, 122, 0.32); color: var(--err);
    }
    .btn.small { padding: 8px 12px; font-size: 11px; }
    .msg { min-height: 16px; margin-top: 9px; font-size: 12px; }
    .msg.ok { color: var(--ok); }
    .msg.err { color: var(--err); }
    .notice {
      display: none; margin-bottom: 16px; padding: 12px 14px;
      border-radius: 12px; font-size: 13px; font-weight: 600;
      background: rgba(255, 107, 122, 0.10);
      border: 1px solid rgba(255, 107, 122, 0.30);
      color: #ffb3bc;
    }
    .notice.show { display: block; }
    .ok-box {
      display: none; margin-top: 12px; padding: 11px 13px;
      border-radius: 10px; font-size: 12px; font-weight: 600;
      background: rgba(72, 213, 151, 0.10);
      border: 1px solid rgba(72, 213, 151, 0.25);
      color: var(--ok);
    }
    .ok-box.show { display: block; }
    .console {
      margin: 0; padding: 14px;
      height: 58vh; min-height: 300px; overflow: auto;
      background: #05070d;
      border: 1px solid var(--line); border-radius: 14px;
      white-space: pre-wrap; word-break: break-word;
      font-family: 'Courier New', monospace;
      font-size: 12px; line-height: 1.55; color: #c8d2ff;
    }
    .console:empty::before { content: 'Waiting for log lines...'; color: #4d5678; }
    .toolbar { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin-bottom: 12px; }
    .toolbar .status { margin-left: auto; color: var(--muted); font-size: 12px; }
    .note { color: var(--muted); font-size: 12px; line-height: 1.65; }
    .scan-list {
      display: none; margin-top: 9px; max-height: 190px; overflow: auto;
      border: 1px solid var(--line); border-radius: 10px;
    }
    .scan-list.show { display: block; }
    .scan-item {
      display: flex; justify-content: space-between; gap: 10px;
      padding: 9px 11px; font-size: 12px; cursor: pointer;
      border-bottom: 1px solid rgba(255, 255, 255, 0.06);
    }
    .scan-item:last-child { border-bottom: 0; }
    .scan-item:hover { background: rgba(255, 255, 255, 0.06); }
    .divider { height: 1px; background: var(--line); margin: 16px 0; }
    footer {
      margin-top: 22px; text-align: center;
      color: #6f7899; font-size: 11px; line-height: 1.7;
    }
    @media (max-width: 820px) {
      .app { padding: 12px; }
      .grid, .grid.two, .grid.three { grid-template-columns: 1fr; }
      .hero-value { font-size: 52px; }
      .top { align-items: flex-start; }
    }
    @media (max-width: 480px) {
      .card { padding: 16px; border-radius: 14px; }
      .tabs { width: 100%; justify-content: space-between; }
      .tab { flex: 1; text-align: center; }
      .hero-value { font-size: 46px; }
      .row { flex-direction: column; align-items: stretch; }
    }
)CSS";

static String uiTab(const char *label, const char *href, bool active) {
  String tab = F("<a class=\"tab");
  if (active) tab += F(" active");
  tab += F("\" href=\"");
  tab += href;
  tab += F("\">");
  tab += label;
  tab += F("</a>");
  return tab;
}

static String renderPage(const char *title, const char *active,
                         const char *body, const char *script) {
  String page;
  page.reserve(26000);

  page += F("<!DOCTYPE html><html lang=\"en\"><head>");
  page += F("<meta charset=\"UTF-8\">");
  page += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
  page += F("<meta name=\"theme-color\" content=\"#080b14\">");
  page += F("<title>");
  page += title;
  page += F("</title><style>");
  page += FPSTR(UI_CSS);
  page += F("</style></head><body><div class=\"app\">");

  page += F("<header class=\"top\"><div class=\"brand\"><div class=\"logo\">&#9878;&#65039;</div>");
  page += F("<div>A12E<small>Scale Monitor</small></div></div><nav class=\"tabs\">");
  page += uiTab("Monitor", "/", strcmp(active, "monitor") == 0);
  page += uiTab("Debug", "/debug", strcmp(active, "debug") == 0);
  page += uiTab("Config", "/config", strcmp(active, "config") == 0);
  page += F("</nav></header><main>");

  page += body;

  page += F("</main><footer>Local AP &middot; 192.168.4.1 &middot; A12E Scale Monitor</footer>");
  page += F("</div><script>");
  page += script;
  page += F("</script></body></html>");

  return page;
}

#endif // WEB_UI_H

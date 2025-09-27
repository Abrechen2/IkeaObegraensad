#pragma once

#include <Arduino.h>

// Modern web interface served by the ESP8266 web server. Stored in PROGMEM to
// keep the RAM footprint low.
const char WEB_INTERFACE_HTML[] PROGMEM = R"rawl(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>IKEA Öbegränsad</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: radial-gradient(circle at top, #1d1f2f, #090b14 55%, #05060b 100%);
      --card-bg: rgba(18, 20, 32, 0.85);
      --accent: #5bc0eb;
      --accent-strong: #4aa3c9;
      --text: #f5f7ff;
      --muted: #a8adc4;
      --border: rgba(255, 255, 255, 0.08);
      --shadow: 0 18px 45px rgba(5, 8, 16, 0.55);
      --radius: 16px;
      font-family: "Inter", "Segoe UI", system-ui, -apple-system, sans-serif;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 4vw 5vw;
      background: var(--bg);
      color: var(--text);
    }

    main {
      width: min(460px, 100%);
      background: var(--card-bg);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      border: 1px solid var(--border);
      backdrop-filter: blur(12px);
      padding: clamp(1.5rem, 3vw, 2.5rem);
    }

    header {
      display: flex;
      flex-direction: column;
      gap: 0.4rem;
      text-align: left;
      margin-bottom: 1.75rem;
    }

    h1 {
      margin: 0;
      font-size: clamp(1.8rem, 4vw, 2.4rem);
      letter-spacing: 0.02em;
    }

    .subtitle {
      color: var(--muted);
      font-size: 0.95rem;
    }

    .card {
      display: flex;
      flex-direction: column;
      gap: 0.85rem;
      padding: 1.1rem 1.25rem;
      border-radius: calc(var(--radius) - 4px);
      border: 1px solid var(--border);
      background: rgba(9, 10, 18, 0.35);
    }

    label {
      font-weight: 600;
      display: block;
    }

    select,
    input[type="text"],
    button {
      width: 100%;
      padding: 0.6rem 0.75rem;
      border-radius: 10px;
      border: 1px solid var(--border);
      background: rgba(18, 20, 32, 0.65);
      color: var(--text);
      font-size: 0.95rem;
      transition: border 0.2s ease, background 0.2s ease, transform 0.1s ease;
    }

    select:focus,
    input[type="text"]:focus,
    button:focus {
      outline: none;
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.2);
    }

    button {
      cursor: pointer;
      font-weight: 600;
      letter-spacing: 0.01em;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      border: none;
      color: #05060b;
    }

    button:hover {
      transform: translateY(-1px);
      box-shadow: 0 10px 25px rgba(75, 160, 214, 0.28);
    }

    .grid {
      display: grid;
      gap: 1.3rem;
    }

    .status-row {
      display: flex;
      justify-content: space-between;
      gap: 0.8rem;
      flex-wrap: wrap;
      font-size: 0.95rem;
      color: var(--muted);
    }

    .status-row span {
      color: var(--text);
      font-weight: 600;
    }

    .range-wrapper {
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }

    input[type="range"] {
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.1);
      outline: none;
      transition: background 0.2s ease;
    }

    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      cursor: pointer;
      box-shadow: 0 2px 10px rgba(91, 192, 235, 0.55);
    }

    input[type="range"]::-moz-range-thumb {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      border: none;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      cursor: pointer;
      box-shadow: 0 2px 10px rgba(91, 192, 235, 0.55);
    }

    .range-value {
      font-variant-numeric: tabular-nums;
      font-size: 1.15rem;
      font-weight: 600;
      color: var(--accent);
      align-self: flex-end;
    }

    footer {
      margin-top: 1.75rem;
      text-align: center;
      font-size: 0.85rem;
      color: var(--muted);
    }

    #statusMessage {
      margin-top: 0.5rem;
      font-size: 0.9rem;
      min-height: 1.1rem;
      color: var(--muted);
    }

    @media (min-width: 520px) {
      .grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>IKEA Öbegränsad</h1>
      <p class="subtitle">LED-Matrix Steuerung &mdash; modernes Web-Dashboard</p>
    </header>

    <section class="card">
      <div class="status-row">
        <div>Zeit <span id="time">--:--:--</span></div>
        <div>Effekt <span id="currentEffect">-</span></div>
      </div>
      <div class="status-row">
        <div>Helligkeit <span id="currentBrightness">0</span></div>
        <div>Zeitzone <span id="currentTimezone">-</span></div>
      </div>
    </section>

    <div class="grid">
      <section class="card">
        <div>
          <label for="effectSelect">Effekt auswählen</label>
          <select id="effectSelect">
            <option value="snake">Snake</option>
            <option value="clock">Clock</option>
            <option value="rain">Rain</option>
            <option value="bounce">Bounce</option>
            <option value="stars">Stars</option>
            <option value="lines">Lines</option>
          </select>
        </div>
        <div>
          <label for="tz">Zeitzone (POSIX)</label>
          <input id="tz" type="text" inputmode="text" autocomplete="off" spellcheck="false" placeholder="z. B. CET-1CEST,M3.5.0,M10.5.0/3">
          <button id="setTz">Zeitzone aktualisieren</button>
        </div>
      </section>

      <section class="card">
        <div class="range-wrapper">
          <label for="brightness">Helligkeit</label>
          <input id="brightness" type="range" min="0" max="1023" step="1">
          <div class="range-value" id="brightnessValue">0</div>
        </div>
        <button id="saveBrightness">Helligkeit speichern</button>
        <div id="statusMessage"></div>
      </section>
    </div>

    <footer>
      <span>Statusaktualisierung alle 2&nbsp;Sekunden</span>
    </footer>
  </main>

  <script>
    const effectSelect = document.getElementById('effectSelect');
    const timeEl = document.getElementById('time');
    const currentEffectEl = document.getElementById('currentEffect');
    const currentBrightnessEl = document.getElementById('currentBrightness');
    const currentTimezoneEl = document.getElementById('currentTimezone');
    const tzInput = document.getElementById('tz');
    const setTzButton = document.getElementById('setTz');
    const brightnessSlider = document.getElementById('brightness');
    const brightnessValue = document.getElementById('brightnessValue');
    const saveBrightnessButton = document.getElementById('saveBrightness');
    const statusMessage = document.getElementById('statusMessage');

    let brightnessDebounce;

    function showStatus(message, type = 'info') {
      statusMessage.textContent = message;
      statusMessage.style.color = type === 'error' ? '#ff7b7b' : 'var(--muted)';
    }

    function prettifyEffect(effect) {
      return effect.charAt(0).toUpperCase() + effect.slice(1);
    }

    function updateBrightnessUI(value) {
      brightnessValue.textContent = value;
      currentBrightnessEl.textContent = value;
      brightnessSlider.value = value;
    }

    async function fetchJson(url) {
      const response = await fetch(url, { cache: 'no-store' });
      if (!response.ok) {
        throw new Error('Netzwerkfehler: ' + response.status);
      }
      return response.json();
    }

    async function refreshStatus() {
      try {
        const data = await fetchJson('/api/status');
        timeEl.textContent = data.time;
        currentEffectEl.textContent = prettifyEffect(data.effect);
        currentTimezoneEl.textContent = data.tz || '-';
        if (document.activeElement !== tzInput) {
          tzInput.value = data.tz || '';
        }
        updateBrightnessUI(data.brightness);
        effectSelect.value = data.effect;
        showStatus('');
      } catch (error) {
        showStatus('Status konnte nicht geladen werden. ' + error.message, 'error');
      }
    }

    async function applyEffect(effect) {
      try {
        await fetch('/effect/' + encodeURIComponent(effect));
        currentEffectEl.textContent = prettifyEffect(effect);
        showStatus('Effekt aktualisiert.');
      } catch (error) {
        showStatus('Effektwechsel fehlgeschlagen.', 'error');
      }
    }

    async function updateTimezone() {
      const tz = tzInput.value.trim();
      if (!tz) {
        showStatus('Bitte eine gültige Zeitzone eintragen.', 'error');
        return;
      }
      try {
        await fetch('/api/setTimezone?tz=' + encodeURIComponent(tz));
        currentTimezoneEl.textContent = tz;
        showStatus('Zeitzone gespeichert.');
      } catch (error) {
        showStatus('Zeitzone konnte nicht gespeichert werden.', 'error');
      }
    }

    async function saveBrightness(value) {
      try {
        await fetch('/api/setBrightness?b=' + encodeURIComponent(value));
        showStatus('Helligkeit gespeichert.');
      } catch (error) {
        showStatus('Helligkeit konnte nicht gespeichert werden.', 'error');
      }
    }

    effectSelect.addEventListener('change', event => {
      applyEffect(event.target.value);
    });

    setTzButton.addEventListener('click', () => {
      updateTimezone();
    });

    brightnessSlider.addEventListener('input', event => {
      const value = event.target.value;
      updateBrightnessUI(value);
      clearTimeout(brightnessDebounce);
      brightnessDebounce = setTimeout(() => saveBrightness(value), 400);
    });

    saveBrightnessButton.addEventListener('click', () => {
      saveBrightness(brightnessSlider.value);
    });

    refreshStatus();
    setInterval(refreshStatus, 2000);
  </script>
</body>
</html>
)rawl";

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

    .range-control {
      display: flex;
      gap: 0.75rem;
      align-items: center;
    }

    .range-control input[type="range"] {
      flex: 1;
    }

    .range-control input[type="number"] {
      width: 85px;
      padding: 0.5rem 0.6rem;
      border-radius: 8px;
      border: 1px solid var(--border);
      background: rgba(18, 20, 32, 0.65);
      color: var(--text);
      font-size: 0.95rem;
      font-weight: 600;
      text-align: center;
      transition: border 0.2s ease;
    }

    .range-control input[type="number"]:focus {
      outline: none;
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.2);
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
      <div class="status-row">
        <div>Lichtsensor <span id="sensorValue">0</span></div>
        <div>Auto-Helligkeit <span id="autoStatus">Aus</span></div>
      </div>
      <div class="status-row">
        <div>MQTT <span id="mqttStatus">Aus</span></div>
        <div>Präsenz <span id="presenceStatus">-</span></div>
      </div>
      <div class="status-row">
        <div>Display <span id="displayStatus">An</span></div>
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
            <option value="pulse">Pulse</option>
            <option value="waves">Waves</option>
            <option value="spiral">Spiral</option>
            <option value="fire">Fire</option>
            <option value="plasma">Plasma</option>
            <option value="ripple">Ripple</option>
            <option value="sandclock">Sand Clock</option>
          </select>
          <button id="toggleSand" type="button">Sand-Effekt: <span id="sandStatus">An</span></button>
        </div>
        <div>
          <label for="tz">Zeitzone</label>
          <select id="tz">
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">Europa/Berlin (CET)</option>
            <option value="GMT0BST,M3.5.0/1,M10.5.0">Europa/London (GMT/BST)</option>
            <option value="WET0WEST,M3.5.0/1,M10.5.0">Europa/Lissabon (WET/WEST)</option>
            <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Europa/Helsinki (EET)</option>
            <option value="EST5EDT,M3.2.0,M11.1.0">Amerika/New York (EST)</option>
            <option value="CST6CDT,M3.2.0,M11.1.0">Amerika/Chicago (CST)</option>
            <option value="MST7MDT,M3.2.0,M11.1.0">Amerika/Denver (MST)</option>
            <option value="PST8PDT,M3.2.0,M11.1.0">Amerika/Los Angeles (PST)</option>
            <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australien/Sydney (AEST)</option>
            <option value="JST-9">Asien/Tokio (JST)</option>
            <option value="CST-8">Asien/Shanghai (CST)</option>
            <option value="IST-5:30">Asien/Indien (IST)</option>
            <option value="UTC0">UTC</option>
          </select>
          <button id="setTz">Zeitzone aktualisieren</button>
        </div>
      </section>

      <section class="card">
        <div class="range-wrapper">
          <label for="brightness">Helligkeit (Manuell)</label>
          <div class="range-control">
            <input id="brightness" type="range" min="0" max="1023" step="1">
            <input id="brightnessInput" type="number" min="0" max="1023" step="1" value="0">
          </div>
        </div>
        <button id="saveBrightness">Helligkeit speichern</button>
        <div id="statusMessage"></div>
      </section>
    </div>

    <section class="card">
      <h3 style="margin: 0 0 1rem 0; font-size: 1.2rem;">Automatische Helligkeit</h3>
      <label style="display: flex; align-items: center; gap: 0.5rem; cursor: pointer;">
        <input type="checkbox" id="autoEnabled" style="width: auto; cursor: pointer;">
        <span>Auto-Helligkeit aktivieren</span>
      </label>
      <div class="grid" style="margin-top: 1rem;">
        <div class="range-wrapper">
          <label for="minBrightness">Min. Helligkeit</label>
          <div class="range-control">
            <input id="minBrightness" type="range" min="0" max="1023" step="1">
            <input id="minBrightnessInput" type="number" min="0" max="1023" step="1" value="0">
          </div>
        </div>
        <div class="range-wrapper">
          <label for="maxBrightness">Max. Helligkeit</label>
          <div class="range-control">
            <input id="maxBrightness" type="range" min="0" max="1023" step="1">
            <input id="maxBrightnessInput" type="number" min="0" max="1023" step="1" value="0">
          </div>
        </div>
        <div class="range-wrapper">
          <label for="sensorMin">Sensor Min. (dunkel)</label>
          <div class="range-control">
            <input id="sensorMin" type="range" min="0" max="1023" step="1">
            <input id="sensorMinInput" type="number" min="0" max="1023" step="1" value="5">
          </div>
        </div>
        <div class="range-wrapper">
          <label for="sensorMax">Sensor Max. (hell)</label>
          <div class="range-control">
            <input id="sensorMax" type="range" min="0" max="1023" step="1">
            <input id="sensorMaxInput" type="number" min="0" max="1023" step="1" value="450">
          </div>
        </div>
      </div>
      <button id="saveAuto">Auto-Brightness speichern</button>
    </section>

    <section class="card">
      <h3 style="margin: 0 0 1rem 0; font-size: 1.2rem;">MQTT Präsenzmelder (Aqara)</h3>
      <label style="display: flex; align-items: center; gap: 0.5rem; cursor: pointer;">
        <input type="checkbox" id="mqttEnabled" style="width: auto; cursor: pointer;">
        <span>MQTT aktivieren</span>
      </label>
      <div class="grid" style="margin-top: 1rem;">
        <div>
          <label for="mqttServer">MQTT Broker IP</label>
          <input id="mqttServer" type="text" placeholder="192.168.1.100">
        </div>
        <div>
          <label for="mqttPort">MQTT Port</label>
          <input id="mqttPort" type="number" min="1" max="65535" value="1883">
        </div>
        <div>
          <label for="mqttUser">MQTT User (optional)</label>
          <input id="mqttUser" type="text" placeholder="username">
        </div>
        <div>
          <label for="mqttPassword">MQTT Passwort (optional)</label>
          <input id="mqttPassword" type="password" placeholder="password">
        </div>
      </div>
      <div style="margin-top: 0.85rem;">
        <label for="mqttTopic">MQTT Topic (Präsenz)</label>
        <input id="mqttTopic" type="text" placeholder="zigbee2mqtt/aqara_fp2" value="zigbee2mqtt/aqara_fp2">
      </div>
      <div class="range-wrapper" style="margin-top: 0.85rem;">
        <label for="presenceTimeout">Display-Timeout nach Präsenz (Sekunden)</label>
        <div class="range-control">
          <input id="presenceTimeout" type="range" min="10" max="600" step="10" value="300">
          <input id="presenceTimeoutInput" type="number" min="10" max="3600" step="10" value="300">
        </div>
      </div>
      <button id="saveMqtt">MQTT Einstellungen speichern</button>
      <p style="font-size: 0.85rem; color: var(--muted); margin: 0.5rem 0 0 0;">
        <strong>Hinweis:</strong> Nach dem Speichern wird die MQTT-Verbindung neu aufgebaut. Das Display schaltet sich automatisch aus, wenn keine Präsenz erkannt wird.<br>
        <strong>Aqara FP2:</strong> Topic ist <code>zigbee2mqtt/[dein_sensor_name]</code> (ohne /presence oder /occupancy). Der Code erkennt JSON automatisch.
      </p>
    </section>

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
    const sensorValueEl = document.getElementById('sensorValue');
    const autoStatusEl = document.getElementById('autoStatus');
    const tzSelect = document.getElementById('tz');
    const setTzButton = document.getElementById('setTz');
    const brightnessSlider = document.getElementById('brightness');
    const brightnessInput = document.getElementById('brightnessInput');
    const saveBrightnessButton = document.getElementById('saveBrightness');
    const statusMessage = document.getElementById('statusMessage');
    const toggleSandButton = document.getElementById('toggleSand');
    const sandStatus = document.getElementById('sandStatus');

    // Auto-Brightness Elemente
    const autoEnabled = document.getElementById('autoEnabled');
    const minBrightnessSlider = document.getElementById('minBrightness');
    const maxBrightnessSlider = document.getElementById('maxBrightness');
    const sensorMinSlider = document.getElementById('sensorMin');
    const sensorMaxSlider = document.getElementById('sensorMax');
    const minBrightnessInput = document.getElementById('minBrightnessInput');
    const maxBrightnessInput = document.getElementById('maxBrightnessInput');
    const sensorMinInput = document.getElementById('sensorMinInput');
    const sensorMaxInput = document.getElementById('sensorMaxInput');
    const saveAutoButton = document.getElementById('saveAuto');

    // MQTT Elemente
    const mqttEnabledCheckbox = document.getElementById('mqttEnabled');
    const mqttServerInput = document.getElementById('mqttServer');
    const mqttPortInput = document.getElementById('mqttPort');
    const mqttUserInput = document.getElementById('mqttUser');
    const mqttPasswordInput = document.getElementById('mqttPassword');
    const mqttTopicInput = document.getElementById('mqttTopic');
    const presenceTimeoutSlider = document.getElementById('presenceTimeout');
    const presenceTimeoutInput = document.getElementById('presenceTimeoutInput');
    const saveMqttButton = document.getElementById('saveMqtt');
    const mqttStatusEl = document.getElementById('mqttStatus');
    const presenceStatusEl = document.getElementById('presenceStatus');
    const displayStatusEl = document.getElementById('displayStatus');

    let brightnessDebounce;

    // Track which fields are currently being edited to prevent refresh overwrites
    const editingFields = new Set();

    const effectLabels = {
      snake: 'Snake',
      clock: 'Clock',
      rain: 'Rain',
      bounce: 'Bounce',
      stars: 'Stars',
      lines: 'Lines',
      pulse: 'Pulse',
      waves: 'Waves',
      spiral: 'Spiral',
      fire: 'Fire',
      plasma: 'Plasma',
      ripple: 'Ripple',
      sandclock: 'Sand Clock'
    };

    function showStatus(message, type = 'info') {
      statusMessage.textContent = message;
      statusMessage.style.color = type === 'error' ? '#ff7b7b' : 'var(--muted)';
    }

    function prettifyEffect(effect) {
      return effectLabels[effect] || (effect.charAt(0).toUpperCase() + effect.slice(1));
    }

    // Helper: sync slider and input
    function syncSliderInput(slider, input, value) {
      slider.value = value;
      input.value = value;
    }

    // Helper: update value only if not being edited
    function safeUpdate(fieldName, slider, input, value) {
      if (!editingFields.has(fieldName)) {
        syncSliderInput(slider, input, value);
      }
    }

    function updateBrightnessUI(value) {
      currentBrightnessEl.textContent = value;
      safeUpdate('brightness', brightnessSlider, brightnessInput, value);
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

        // Timezone Select aktualisieren
        if (document.activeElement !== tzSelect) {
          tzSelect.value = data.tz || 'CET-1CEST,M3.5.0,M10.5.0/3';
        }

        updateBrightnessUI(data.brightness);
        if ([...effectSelect.options].some(option => option.value === data.effect)) {
          effectSelect.value = data.effect;
        }
        if (data.sandEnabled !== undefined) {
          sandStatus.textContent = data.sandEnabled ? 'An' : 'Aus';
        }

        // Auto-Brightness Status
        if (data.sensorValue !== undefined) {
          sensorValueEl.textContent = data.sensorValue;
        }
        if (data.autoBrightness !== undefined) {
          autoStatusEl.textContent = data.autoBrightness ? 'An' : 'Aus';
          if (!editingFields.has('autoEnabled')) {
            autoEnabled.checked = data.autoBrightness;
          }
        }
        if (data.minBrightness !== undefined) {
          safeUpdate('minBrightness', minBrightnessSlider, minBrightnessInput, data.minBrightness);
        }
        if (data.maxBrightness !== undefined) {
          safeUpdate('maxBrightness', maxBrightnessSlider, maxBrightnessInput, data.maxBrightness);
        }
        if (data.sensorMin !== undefined) {
          safeUpdate('sensorMin', sensorMinSlider, sensorMinInput, data.sensorMin);
        }
        if (data.sensorMax !== undefined) {
          safeUpdate('sensorMax', sensorMaxSlider, sensorMaxInput, data.sensorMax);
        }

        // MQTT Status
        if (data.mqttEnabled !== undefined) {
          if (!editingFields.has('mqttEnabled')) {
            mqttEnabledCheckbox.checked = data.mqttEnabled;
          }
          if (data.mqttConnected) {
            mqttStatusEl.textContent = 'Verbunden';
            mqttStatusEl.style.color = '#5bc0eb';
          } else if (data.mqttEnabled) {
            mqttStatusEl.textContent = 'Getrennt';
            mqttStatusEl.style.color = '#ff7b7b';
          } else {
            mqttStatusEl.textContent = 'Aus';
            mqttStatusEl.style.color = '';
          }
        }
        if (data.mqttServer !== undefined && !editingFields.has('mqttServer')) {
          mqttServerInput.value = data.mqttServer;
        }
        if (data.mqttPort !== undefined && !editingFields.has('mqttPort')) {
          mqttPortInput.value = data.mqttPort;
        }
        if (data.mqttTopic !== undefined && !editingFields.has('mqttTopic')) {
          mqttTopicInput.value = data.mqttTopic;
        }
        if (data.presenceDetected !== undefined) {
          presenceStatusEl.textContent = data.presenceDetected ? 'Erkannt' : 'Nicht erkannt';
          presenceStatusEl.style.color = data.presenceDetected ? '#5bc0eb' : '';
        }
        if (data.displayEnabled !== undefined) {
          displayStatusEl.textContent = data.displayEnabled ? 'An' : 'Aus';
          displayStatusEl.style.color = data.displayEnabled ? '' : '#ff7b7b';
        }
        if (data.presenceTimeout !== undefined) {
          const timeoutSeconds = Math.floor(data.presenceTimeout / 1000);
          safeUpdate('presenceTimeout', presenceTimeoutSlider, presenceTimeoutInput, timeoutSeconds);
        }

        showStatus('');
      } catch (error) {
        showStatus('Status konnte nicht geladen werden. ' + error.message, 'error');
      }
    }

    async function toggleSandEffect() {
      try {
        const response = await fetch('/api/toggleSand');
        const data = await response.json();
        sandStatus.textContent = data.sandEnabled ? 'An' : 'Aus';
        showStatus('Sand-Effekt ' + (data.sandEnabled ? 'aktiviert' : 'deaktiviert'));
      } catch (error) {
        showStatus('Fehler beim Umschalten', 'error');
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
      const tz = tzSelect.value.trim();
      if (!tz) {
        showStatus('Bitte eine gültige Zeitzone auswählen.', 'error');
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

    async function saveAutoBrightness() {
      try {
        const params = new URLSearchParams({
          enabled: autoEnabled.checked ? 'true' : 'false',
          min: minBrightnessInput.value,
          max: maxBrightnessInput.value,
          sensorMin: sensorMinInput.value,
          sensorMax: sensorMaxInput.value
        });
        await fetch('/api/setAutoBrightness?' + params.toString());
        showStatus('Auto-Brightness Einstellungen gespeichert.');
      } catch (error) {
        showStatus('Auto-Brightness konnte nicht gespeichert werden.', 'error');
      }
    }

    async function saveMqtt() {
      try {
        const timeoutMs = parseInt(presenceTimeoutInput.value) * 1000;
        const params = new URLSearchParams({
          enabled: mqttEnabledCheckbox.checked ? 'true' : 'false',
          server: mqttServerInput.value,
          port: mqttPortInput.value,
          user: mqttUserInput.value,
          password: mqttPasswordInput.value,
          topic: mqttTopicInput.value,
          timeout: timeoutMs.toString()
        });
        await fetch('/api/setMqtt?' + params.toString());
        showStatus('MQTT Einstellungen gespeichert. Verbindung wird neu aufgebaut...');
      } catch (error) {
        showStatus('MQTT konnte nicht gespeichert werden.', 'error');
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

    // Brightness slider/input sync and save
    brightnessSlider.addEventListener('input', event => {
      const value = event.target.value;
      brightnessInput.value = value;
      currentBrightnessEl.textContent = value;
      clearTimeout(brightnessDebounce);
      brightnessDebounce = setTimeout(() => saveBrightness(value), 400);
    });

    brightnessInput.addEventListener('input', event => {
      const value = Math.max(0, Math.min(1023, parseInt(event.target.value) || 0));
      brightnessSlider.value = value;
      currentBrightnessEl.textContent = value;
      clearTimeout(brightnessDebounce);
      brightnessDebounce = setTimeout(() => saveBrightness(value), 400);
    });

    brightnessSlider.addEventListener('focus', () => editingFields.add('brightness'));
    brightnessSlider.addEventListener('blur', () => editingFields.delete('brightness'));
    brightnessInput.addEventListener('focus', () => editingFields.add('brightness'));
    brightnessInput.addEventListener('blur', () => editingFields.delete('brightness'));

    saveBrightnessButton.addEventListener('click', () => {
      saveBrightness(brightnessInput.value);
    });

    toggleSandButton.addEventListener('click', toggleSandEffect);

    // Auto-Brightness Event Listeners
    // Min Brightness
    minBrightnessSlider.addEventListener('input', event => {
      minBrightnessInput.value = event.target.value;
    });
    minBrightnessInput.addEventListener('input', event => {
      const value = Math.max(0, Math.min(1023, parseInt(event.target.value) || 0));
      minBrightnessSlider.value = value;
      minBrightnessInput.value = value;
    });
    minBrightnessSlider.addEventListener('focus', () => editingFields.add('minBrightness'));
    minBrightnessSlider.addEventListener('blur', () => editingFields.delete('minBrightness'));
    minBrightnessInput.addEventListener('focus', () => editingFields.add('minBrightness'));
    minBrightnessInput.addEventListener('blur', () => editingFields.delete('minBrightness'));

    // Max Brightness
    maxBrightnessSlider.addEventListener('input', event => {
      maxBrightnessInput.value = event.target.value;
    });
    maxBrightnessInput.addEventListener('input', event => {
      const value = Math.max(0, Math.min(1023, parseInt(event.target.value) || 0));
      maxBrightnessSlider.value = value;
      maxBrightnessInput.value = value;
    });
    maxBrightnessSlider.addEventListener('focus', () => editingFields.add('maxBrightness'));
    maxBrightnessSlider.addEventListener('blur', () => editingFields.delete('maxBrightness'));
    maxBrightnessInput.addEventListener('focus', () => editingFields.add('maxBrightness'));
    maxBrightnessInput.addEventListener('blur', () => editingFields.delete('maxBrightness'));

    // Sensor Min
    sensorMinSlider.addEventListener('input', event => {
      sensorMinInput.value = event.target.value;
    });
    sensorMinInput.addEventListener('input', event => {
      const value = Math.max(0, Math.min(1023, parseInt(event.target.value) || 0));
      sensorMinSlider.value = value;
      sensorMinInput.value = value;
    });
    sensorMinSlider.addEventListener('focus', () => editingFields.add('sensorMin'));
    sensorMinSlider.addEventListener('blur', () => editingFields.delete('sensorMin'));
    sensorMinInput.addEventListener('focus', () => editingFields.add('sensorMin'));
    sensorMinInput.addEventListener('blur', () => editingFields.delete('sensorMin'));

    // Sensor Max
    sensorMaxSlider.addEventListener('input', event => {
      sensorMaxInput.value = event.target.value;
    });
    sensorMaxInput.addEventListener('input', event => {
      const value = Math.max(0, Math.min(1023, parseInt(event.target.value) || 0));
      sensorMaxSlider.value = value;
      sensorMaxInput.value = value;
    });
    sensorMaxSlider.addEventListener('focus', () => editingFields.add('sensorMax'));
    sensorMaxSlider.addEventListener('blur', () => editingFields.delete('sensorMax'));
    sensorMaxInput.addEventListener('focus', () => editingFields.add('sensorMax'));
    sensorMaxInput.addEventListener('blur', () => editingFields.delete('sensorMax'));

    // Auto Enabled Checkbox
    autoEnabled.addEventListener('focus', () => editingFields.add('autoEnabled'));
    autoEnabled.addEventListener('blur', () => editingFields.delete('autoEnabled'));
    autoEnabled.addEventListener('change', () => {
      saveAutoBrightness();
    });

    saveAutoButton.addEventListener('click', () => {
      saveAutoBrightness();
    });

    // MQTT Event Listeners
    mqttEnabledCheckbox.addEventListener('focus', () => editingFields.add('mqttEnabled'));
    mqttEnabledCheckbox.addEventListener('blur', () => editingFields.delete('mqttEnabled'));
    mqttServerInput.addEventListener('focus', () => editingFields.add('mqttServer'));
    mqttServerInput.addEventListener('blur', () => editingFields.delete('mqttServer'));
    mqttPortInput.addEventListener('focus', () => editingFields.add('mqttPort'));
    mqttPortInput.addEventListener('blur', () => editingFields.delete('mqttPort'));
    mqttTopicInput.addEventListener('focus', () => editingFields.add('mqttTopic'));
    mqttTopicInput.addEventListener('blur', () => editingFields.delete('mqttTopic'));

    // Presence Timeout Slider/Input
    presenceTimeoutSlider.addEventListener('input', event => {
      presenceTimeoutInput.value = event.target.value;
    });
    presenceTimeoutInput.addEventListener('input', event => {
      const value = Math.max(10, Math.min(3600, parseInt(event.target.value) || 300));
      presenceTimeoutSlider.value = value;
      presenceTimeoutInput.value = value;
    });
    presenceTimeoutSlider.addEventListener('focus', () => editingFields.add('presenceTimeout'));
    presenceTimeoutSlider.addEventListener('blur', () => editingFields.delete('presenceTimeout'));
    presenceTimeoutInput.addEventListener('focus', () => editingFields.add('presenceTimeout'));
    presenceTimeoutInput.addEventListener('blur', () => editingFields.delete('presenceTimeout'));

    saveMqttButton.addEventListener('click', () => {
      saveMqtt();
    });

    refreshStatus();
    setInterval(refreshStatus, 2000);
  </script>
</body>
</html>
)rawl";

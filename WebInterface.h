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
      --card-bg: rgba(18, 20, 32, 0.9);
      --card-bg-hover: rgba(18, 20, 32, 0.95);
      --accent: #5bc0eb;
      --accent-strong: #4aa3c9;
      --accent-hover: #6dd0f5;
      --success: #4ade80;
      --warning: #fbbf24;
      --error: #f87171;
      --text: #f5f7ff;
      --text-secondary: #d1d5e8;
      --muted: #a8adc4;
      --muted-dark: #6b7280;
      --border: rgba(255, 255, 255, 0.1);
      --border-light: rgba(255, 255, 255, 0.15);
      --shadow: 0 8px 24px rgba(5, 8, 16, 0.4);
      --shadow-lg: 0 16px 48px rgba(5, 8, 16, 0.6);
      --shadow-sm: 0 2px 8px rgba(5, 8, 16, 0.3);
      --radius: 12px;
      --radius-sm: 8px;
      --radius-lg: 16px;
      --spacing-1: 0.5rem;
      --spacing-2: 1rem;
      --spacing-3: 1.5rem;
      --spacing-4: 2rem;
      --spacing-5: 2.5rem;
      --spacing-6: 3rem;
      font-family: "Inter", "Segoe UI", system-ui, -apple-system, sans-serif;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    body {
      margin: 0;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: var(--spacing-3) var(--spacing-2);
      background: var(--bg);
      color: var(--text);
      line-height: 1.6;
      font-size: 1rem;
    }

    main {
      width: min(1000px, 100%);
      max-width: 95vw;
      background: var(--card-bg);
      border-radius: var(--radius-lg);
      box-shadow: var(--shadow-lg);
      border: 1px solid var(--border);
      backdrop-filter: blur(16px);
      padding: clamp(var(--spacing-3), 4vw, var(--spacing-5));
    }

    h1 {
      margin: 0;
      font-size: clamp(1.75rem, 4vw, 2.25rem);
      letter-spacing: -0.02em;
      font-weight: 700;
      line-height: 1.2;
      color: var(--text);
    }

    h2 {
      margin: 0;
      font-size: 1.25rem;
      font-weight: 700;
      line-height: 1.3;
      color: var(--text);
    }

    h3 {
      margin: 0;
      font-size: 1.1rem;
      font-weight: 600;
      line-height: 1.4;
      color: var(--text);
    }

    .subtitle {
      color: var(--muted);
      font-size: 0.875rem;
      font-weight: 400;
      line-height: 1.5;
      margin-top: var(--spacing-1);
    }

    .caption {
      font-size: 0.75rem;
      color: var(--muted);
      line-height: 1.4;
    }

    header {
      display: flex;
      flex-direction: column;
      gap: var(--spacing-1);
      text-align: left;
      margin-bottom: var(--spacing-4);
    }

    .status-dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
      gap: var(--spacing-2);
      margin-bottom: var(--spacing-4);
    }

    .status-item {
      display: flex;
      flex-direction: column;
      gap: var(--spacing-1);
      padding: var(--spacing-2);
      border-radius: var(--radius);
      border: 1px solid var(--border);
      background: rgba(9, 10, 18, 0.4);
      transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
      position: relative;
      overflow: hidden;
    }

    .status-item::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 2px;
      background: linear-gradient(90deg, var(--accent), var(--accent-strong));
      opacity: 0;
      transition: opacity 0.25s ease;
    }

    .status-item:hover {
      transform: translateY(-2px);
      box-shadow: var(--shadow);
      border-color: var(--border-light);
      background: rgba(9, 10, 18, 0.5);
    }

    .status-item:hover::before {
      opacity: 1;
    }

    .status-label {
      font-size: 0.6875rem;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.08em;
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: var(--spacing-1);
    }

    .status-label span:first-child {
      font-size: 1rem;
      filter: grayscale(0.3);
    }

    .status-value {
      font-size: 1.375rem;
      font-weight: 700;
      color: var(--text);
      font-variant-numeric: tabular-nums;
      transition: color 0.2s ease;
    }

    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 0.375rem;
      padding: 0.375rem 0.625rem;
      border-radius: var(--radius-sm);
      font-size: 0.6875rem;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      transition: all 0.2s ease;
      position: relative;
    }

    .status-badge::before {
      content: '';
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: currentColor;
      opacity: 0.8;
      animation: pulse 2s ease-in-out infinite;
    }

    @keyframes pulse {
      0%, 100% { opacity: 0.8; transform: scale(1); }
      50% { opacity: 1; transform: scale(1.1); }
    }

    .badge-success {
      background: rgba(74, 222, 128, 0.15);
      color: var(--success);
      border: 1px solid rgba(74, 222, 128, 0.25);
    }

    .badge-warning {
      background: rgba(251, 191, 36, 0.15);
      color: var(--warning);
      border: 1px solid rgba(251, 191, 36, 0.25);
    }

    .badge-error {
      background: rgba(248, 113, 113, 0.15);
      color: var(--error);
      border: 1px solid rgba(248, 113, 113, 0.25);
    }

    .badge-info {
      background: rgba(91, 192, 235, 0.15);
      color: var(--accent);
      border: 1px solid rgba(91, 192, 235, 0.25);
    }

    .badge-neutral {
      background: rgba(168, 173, 196, 0.1);
      color: var(--muted);
      border: 1px solid rgba(168, 173, 196, 0.15);
    }

    .badge-neutral::before {
      display: none;
    }

    .card {
      display: flex;
      flex-direction: column;
      gap: var(--spacing-3);
      padding: var(--spacing-3);
      border-radius: var(--radius);
      border: 1px solid var(--border);
      background: rgba(9, 10, 18, 0.4);
      margin-bottom: var(--spacing-3);
      transition: border-color 0.2s ease, background 0.2s ease;
    }

    .card:hover {
      border-color: var(--border-light);
      background: rgba(9, 10, 18, 0.5);
    }

    .card-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: var(--spacing-1);
    }

    .card-title {
      font-size: 1.125rem;
      font-weight: 700;
      margin: 0;
      color: var(--text);
    }

    .accordion {
      border: 1px solid var(--border);
      border-radius: var(--radius);
      background: rgba(9, 10, 18, 0.4);
      margin-bottom: var(--spacing-2);
      overflow: hidden;
      transition: border-color 0.2s ease, background 0.2s ease;
    }

    .accordion:hover {
      border-color: var(--border-light);
    }

    .accordion-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: var(--spacing-2) var(--spacing-3);
      cursor: pointer;
      user-select: none;
      transition: background 0.2s ease;
      background: rgba(18, 20, 32, 0.3);
    }

    .accordion-header:hover,
    .accordion-header:focus-visible {
      background: rgba(18, 20, 32, 0.5);
      outline: none;
    }

    .accordion-header:focus-visible {
      box-shadow: inset 0 0 0 2px var(--accent);
    }

    .accordion-header h3 {
      margin: 0;
      font-size: 1rem;
      font-weight: 600;
      color: var(--text);
    }

    .accordion-icon {
      transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      font-size: 1rem;
      color: var(--accent);
      line-height: 1;
    }

    .accordion.open .accordion-icon {
      transform: rotate(180deg);
    }

    .accordion-content {
      max-height: 0;
      overflow: hidden;
      transition: max-height 0.35s cubic-bezier(0.4, 0, 0.2, 1), padding 0.35s ease;
      padding: 0 var(--spacing-3);
    }

    .accordion.open .accordion-content {
      max-height: 3000px;
      padding: var(--spacing-3);
    }

    .effect-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
      gap: var(--spacing-2);
      margin-top: var(--spacing-2);
    }

    .effect-card {
      aspect-ratio: 1;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: var(--spacing-2);
      border-radius: var(--radius);
      border: 2px solid var(--border);
      background: rgba(18, 20, 32, 0.6);
      cursor: pointer;
      transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
      text-align: center;
      font-weight: 600;
      color: var(--text);
      position: relative;
      overflow: hidden;
    }

    .effect-card::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      opacity: 0;
      transition: opacity 0.25s ease;
    }

    .effect-card:hover {
      transform: translateY(-4px) scale(1.02);
      border-color: var(--accent);
      box-shadow: 0 8px 24px rgba(91, 192, 235, 0.25);
    }

    .effect-card:hover::before {
      opacity: 0.1;
    }

    .effect-card:focus-visible {
      outline: none;
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.3);
    }

    .effect-card.active {
      border-color: var(--accent);
      background: rgba(91, 192, 235, 0.15);
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.2), 0 4px 12px rgba(91, 192, 235, 0.15);
    }

    .effect-card.active::before {
      opacity: 0.15;
    }

    .effect-card span {
      position: relative;
      z-index: 1;
    }

    label {
      font-weight: 600;
      display: block;
      margin-bottom: var(--spacing-1);
      color: var(--text);
      font-size: 0.875rem;
    }

    select,
    input[type="text"],
    input[type="number"],
    input[type="password"],
    button {
      width: 100%;
      padding: 0.75rem 1rem;
      border-radius: var(--radius-sm);
      border: 1px solid var(--border);
      background: rgba(18, 20, 32, 0.7);
      color: var(--text);
      font-size: 0.9375rem;
      font-family: inherit;
      transition: all 0.2s ease;
      min-height: 44px;
    }

    select:focus,
    input[type="text"]:focus,
    input[type="number"]:focus,
    input[type="password"]:focus {
      outline: none;
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.2);
      background: rgba(18, 20, 32, 0.85);
    }

    input:invalid {
      border-color: var(--error);
    }

    input:invalid:focus {
      box-shadow: 0 0 0 3px rgba(248, 113, 113, 0.2);
    }

    .input-error {
      color: var(--error);
      font-size: 0.75rem;
      margin-top: 0.25rem;
      display: none;
    }

    input:invalid ~ .input-error {
      display: block;
    }

    button {
      cursor: pointer;
      font-weight: 600;
      letter-spacing: 0.01em;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      border: none;
      color: #05060b;
      position: relative;
      overflow: hidden;
    }

    button::before {
      content: '';
      position: absolute;
      top: 0;
      left: -100%;
      width: 100%;
      height: 100%;
      background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.2), transparent);
      transition: left 0.5s ease;
    }

    button:hover:not(:disabled)::before {
      left: 100%;
    }

    button:hover:not(:disabled) {
      transform: translateY(-1px);
      box-shadow: 0 8px 20px rgba(75, 160, 214, 0.3);
      background: linear-gradient(120deg, var(--accent-hover), var(--accent));
    }

    button:active:not(:disabled) {
      transform: translateY(0);
    }

    button:focus-visible {
      outline: none;
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.4);
    }

    button:disabled {
      opacity: 0.6;
      cursor: not-allowed;
      transform: none;
    }

    button.loading {
      color: transparent;
      pointer-events: none;
    }

    button.loading::after {
      content: '';
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      width: 20px;
      height: 20px;
      border: 2px solid rgba(5, 6, 11, 0.3);
      border-top-color: #05060b;
      border-radius: 50%;
      animation: spin 0.6s linear infinite;
    }

    @keyframes spin {
      to { transform: translate(-50%, -50%) rotate(360deg); }
    }

    .range-wrapper {
      display: flex;
      flex-direction: column;
      gap: var(--spacing-1);
    }

    .range-control {
      display: flex;
      gap: var(--spacing-2);
      align-items: center;
    }

    .range-control input[type="range"] {
      flex: 1;
    }

    .range-control input[type="number"] {
      width: 90px;
      padding: 0.625rem 0.75rem;
      border-radius: var(--radius-sm);
      border: 1px solid var(--border);
      background: rgba(18, 20, 32, 0.7);
      color: var(--text);
      font-size: 0.9375rem;
      font-weight: 600;
      text-align: center;
      min-height: 44px;
    }

    input[type="range"] {
      -webkit-appearance: none;
      width: 100%;
      height: 8px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.1);
      outline: none;
      transition: background 0.2s ease;
    }

    input[type="range"]:hover {
      background: rgba(255, 255, 255, 0.15);
    }

    input[type="range"]:focus-visible {
      box-shadow: 0 0 0 3px rgba(91, 192, 235, 0.2);
    }

    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      cursor: pointer;
      box-shadow: 0 2px 8px rgba(91, 192, 235, 0.4);
      transition: transform 0.2s ease, box-shadow 0.2s ease;
    }

    input[type="range"]::-webkit-slider-thumb:hover {
      transform: scale(1.15);
      box-shadow: 0 3px 12px rgba(91, 192, 235, 0.5);
    }

    input[type="range"]::-moz-range-thumb {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      border: none;
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      cursor: pointer;
      box-shadow: 0 2px 8px rgba(91, 192, 235, 0.4);
      transition: transform 0.2s ease, box-shadow 0.2s ease;
    }

    input[type="range"]::-moz-range-thumb:hover {
      transform: scale(1.15);
      box-shadow: 0 3px 12px rgba(91, 192, 235, 0.5);
    }

    .brightness-preview {
      width: 100%;
      height: 10px;
      border-radius: 999px;
      background: linear-gradient(to right, 
        rgba(255, 255, 255, 0.08) 0%,
        rgba(91, 192, 235, 0.25) 50%,
        rgba(91, 192, 235, 0.5) 100%);
      margin-top: var(--spacing-1);
      position: relative;
      overflow: hidden;
      border: 1px solid var(--border);
    }

    .brightness-preview::after {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      height: 100%;
      width: var(--brightness-percent, 50%);
      background: linear-gradient(120deg, var(--accent), var(--accent-strong));
      border-radius: 999px;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      box-shadow: 0 0 8px rgba(91, 192, 235, 0.4);
    }

    .checkbox-wrapper {
      display: flex;
      align-items: center;
      gap: var(--spacing-1);
      cursor: pointer;
      user-select: none;
      padding: var(--spacing-1) 0;
      transition: opacity 0.2s ease;
    }

    .checkbox-wrapper:hover {
      opacity: 0.9;
    }

    .checkbox-wrapper input[type="checkbox"] {
      width: 20px;
      height: 20px;
      cursor: pointer;
      accent-color: var(--accent);
    }

    .checkbox-wrapper:focus-within {
      outline: 2px solid var(--accent);
      outline-offset: 2px;
      border-radius: 4px;
    }

    .grid {
      display: grid;
      gap: var(--spacing-2);
    }

    .toast-container {
      position: fixed;
      top: var(--spacing-3);
      right: var(--spacing-3);
      z-index: 1000;
      display: flex;
      flex-direction: column;
      gap: var(--spacing-1);
      max-width: 400px;
      pointer-events: none;
    }

    .toast {
      padding: var(--spacing-2) var(--spacing-3);
      border-radius: var(--radius);
      background: var(--card-bg);
      border: 1px solid var(--border);
      box-shadow: var(--shadow-lg);
      color: var(--text);
      font-size: 0.875rem;
      animation: toastSlideIn 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      pointer-events: auto;
      display: flex;
      align-items: center;
      gap: var(--spacing-1);
      position: relative;
    }

    .toast::before {
      content: '';
      position: absolute;
      left: 0;
      top: 0;
      bottom: 0;
      width: 4px;
      border-radius: var(--radius) 0 0 var(--radius);
    }

    .toast.success::before {
      background: var(--success);
    }

    .toast.error::before {
      background: var(--error);
    }

    .toast.info::before {
      background: var(--accent);
    }

    @keyframes toastSlideIn {
      from {
        transform: translateX(calc(100% + var(--spacing-3)));
        opacity: 0;
      }
      to {
        transform: translateX(0);
        opacity: 1;
      }
    }

    footer {
      margin-top: var(--spacing-4);
      text-align: center;
      font-size: 0.8125rem;
      color: var(--muted);
      padding-top: var(--spacing-3);
      border-top: 1px solid var(--border);
    }

    @media (min-width: 520px) {
      .grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }
    }

    @media (min-width: 768px) {
      .status-dashboard {
        grid-template-columns: repeat(4, 1fr);
      }
      
      .effect-grid {
        grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
      }
    }

    @media (max-width: 480px) {
      .status-dashboard {
        grid-template-columns: repeat(2, 1fr);
      }
      
      .effect-grid {
        grid-template-columns: repeat(2, 1fr);
      }

      .toast-container {
        left: var(--spacing-2);
        right: var(--spacing-2);
        max-width: none;
      }
    }

    .status-value,
    .status-badge {
      animation: fadeIn 0.3s ease;
    }

    @keyframes fadeIn {
      from {
        opacity: 0;
        transform: translateY(-4px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
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

    <section class="status-dashboard" role="region" aria-label="System Status">
      <div class="status-item">
        <div class="status-label">
          <span>🕐</span>
          <span>Zeit</span>
        </div>
        <div class="status-value" id="time" aria-live="polite">--:--:--</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>✨</span>
          <span>Effekt</span>
        </div>
        <div class="status-value" id="currentEffect">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>💡</span>
          <span>Helligkeit</span>
        </div>
        <div class="status-value" id="currentBrightness">0</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>🌍</span>
          <span>Zeitzone</span>
        </div>
        <div class="status-value" id="currentTimezone" style="font-size: 0.875rem;">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>📊</span>
          <span>Lichtsensor</span>
        </div>
        <div class="status-value" id="sensorValue">0</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>⚙️</span>
          <span>Auto-Helligkeit</span>
        </div>
        <div id="autoStatus" class="status-badge badge-neutral">Aus</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>📡</span>
          <span>MQTT</span>
        </div>
        <div id="mqttStatus" class="status-badge badge-neutral">Aus</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>👤</span>
          <span>Präsenz über MQTT</span>
        </div>
        <div id="presenceStatus" class="status-badge badge-neutral">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>🖥️</span>
          <span>Display</span>
        </div>
        <div id="displayStatus" class="status-badge badge-success">An</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>🔄</span>
          <span>OTA</span>
        </div>
        <div id="otaStatus" class="status-badge badge-neutral">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>🌐</span>
          <span>IP-Adresse</span>
        </div>
        <div class="status-value" id="ipAddress" style="font-size: 0.875rem;">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>🔄</span>
          <span>Neustarts</span>
        </div>
        <div style="display: flex; align-items: center; gap: var(--spacing-1);">
          <div class="status-value" id="restartCount">-</div>
          <button id="resetRestartCount" style="width: auto; padding: 0.375rem 0.75rem; font-size: 0.75rem; min-height: auto;" aria-label="Restart-Counter zurücksetzen">Reset</button>
        </div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>⚠️</span>
          <span>Letzter Reset</span>
        </div>
        <div class="status-value" id="lastResetReason" style="font-size: 0.75rem; word-break: break-word;">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>⏱️</span>
          <span>Uptime vor Restart</span>
        </div>
        <div class="status-value" id="lastUptimeBeforeRestart">-</div>
      </div>
      <div class="status-item">
        <div class="status-label">
          <span>💾</span>
          <span>Heap vor Restart</span>
        </div>
        <div class="status-value" id="lastHeapBeforeRestart">-</div>
      </div>
    </section>

    <section class="card" role="region" aria-label="Effekt Auswahl">
      <div class="card-header">
        <h2 class="card-title">Effekt auswählen</h2>
      </div>
      <div class="effect-grid" role="group" aria-label="Verfügbare Effekte">
        <div class="effect-card" data-effect="snake" role="button" tabindex="0" aria-label="Snake Effekt">
          <span>Snake</span>
        </div>
        <div class="effect-card" data-effect="clock" role="button" tabindex="0" aria-label="Clock Effekt">
          <span>Clock</span>
        </div>
        <div class="effect-card" data-effect="rain" role="button" tabindex="0" aria-label="Rain Effekt">
          <span>Rain</span>
        </div>
        <div class="effect-card" data-effect="bounce" role="button" tabindex="0" aria-label="Bounce Effekt">
          <span>Bounce</span>
        </div>
        <div class="effect-card" data-effect="stars" role="button" tabindex="0" aria-label="Stars Effekt">
          <span>Stars</span>
        </div>
        <div class="effect-card" data-effect="lines" role="button" tabindex="0" aria-label="Lines Effekt">
          <span>Lines</span>
        </div>
        <div class="effect-card" data-effect="pulse" role="button" tabindex="0" aria-label="Pulse Effekt">
          <span>Pulse</span>
        </div>
        <div class="effect-card" data-effect="waves" role="button" tabindex="0" aria-label="Waves Effekt">
          <span>Waves</span>
        </div>
        <div class="effect-card" data-effect="spiral" role="button" tabindex="0" aria-label="Spiral Effekt">
          <span>Spiral</span>
        </div>
        <div class="effect-card" data-effect="fire" role="button" tabindex="0" aria-label="Fire Effekt">
          <span>Fire</span>
        </div>
        <div class="effect-card" data-effect="plasma" role="button" tabindex="0" aria-label="Plasma Effekt">
          <span>Plasma</span>
        </div>
        <div class="effect-card" data-effect="ripple" role="button" tabindex="0" aria-label="Ripple Effekt">
          <span>Ripple</span>
        </div>
        <div class="effect-card" data-effect="sandclock" role="button" tabindex="0" aria-label="Sand Clock Effekt">
          <span>Sand Clock</span>
        </div>
      </div>
      <div style="margin-top: var(--spacing-2);">
        <div class="grid" style="gap: var(--spacing-2); grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));">
          <div>
            <label for="tz">Zeitzone</label>
            <select id="tz" aria-label="Zeitzone auswählen">
              <option value="CET-1CEST-2,M3.5.0/02,M10.5.0/03">Europa/Berlin (CET)</option>
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
          </div>
          <div>
            <label for="hourFormat">Zeitformat</label>
            <select id="hourFormat" aria-label="Zeitformat auswählen">
              <option value="24">24-Stunden-Format</option>
              <option value="12">12-Stunden-Format (AM/PM)</option>
            </select>
          </div>
        </div>
        <button id="setTz" aria-label="Zeitzone und Zeitformat aktualisieren">Zeitzone & Zeitformat speichern</button>
      </div>
    </section>

    <section class="card" role="region" aria-label="Manuelle Helligkeit">
      <div class="card-header">
        <h2 class="card-title">Helligkeit (Manuell)</h2>
      </div>
      <div class="range-wrapper">
        <label for="brightness">Helligkeit</label>
        <div class="range-control">
          <input id="brightness" type="range" min="0" max="1023" step="1" aria-label="Helligkeit einstellen">
          <input id="brightnessInput" type="number" min="0" max="1023" step="1" value="0" aria-label="Helligkeit numerisch eingeben" required>
        </div>
        <div class="brightness-preview" style="--brightness-percent: 50%" id="brightnessPreview"></div>
        <span class="input-error">Bitte einen Wert zwischen 0 und 1023 eingeben</span>
      </div>
      <button id="saveBrightness" aria-label="Helligkeit speichern">Helligkeit speichern</button>
    </section>

    <div class="accordion" role="region" aria-label="Automatische Helligkeit">
      <div class="accordion-header" role="button" tabindex="0" aria-expanded="false" aria-controls="autoBrightnessContent">
        <h3>Automatische Helligkeit</h3>
        <span class="accordion-icon" aria-hidden="true">▼</span>
      </div>
      <div class="accordion-content" id="autoBrightnessContent">
        <label class="checkbox-wrapper">
          <input type="checkbox" id="autoEnabled" aria-label="Auto-Helligkeit aktivieren">
          <span>Auto-Helligkeit aktivieren</span>
        </label>
        <div class="grid" style="margin-top: var(--spacing-2);">
          <div class="range-wrapper">
            <label for="minBrightness">Min. Helligkeit</label>
            <div class="range-control">
              <input id="minBrightness" type="range" min="0" max="1023" step="1" aria-label="Minimale Helligkeit">
              <input id="minBrightnessInput" type="number" min="0" max="1023" step="1" value="0" aria-label="Minimale Helligkeit numerisch" required>
            </div>
            <span class="input-error">Bitte einen Wert zwischen 0 und 1023 eingeben</span>
          </div>
          <div class="range-wrapper">
            <label for="maxBrightness">Max. Helligkeit</label>
            <div class="range-control">
              <input id="maxBrightness" type="range" min="0" max="1023" step="1" aria-label="Maximale Helligkeit">
              <input id="maxBrightnessInput" type="number" min="0" max="1023" step="1" value="0" aria-label="Maximale Helligkeit numerisch" required>
            </div>
            <span class="input-error">Bitte einen Wert zwischen 0 und 1023 eingeben</span>
          </div>
          <div class="range-wrapper">
            <label for="sensorMin">Sensor Min. (dunkel)</label>
            <div class="range-control">
              <input id="sensorMin" type="range" min="0" max="1023" step="1" aria-label="Sensor Minimum">
              <input id="sensorMinInput" type="number" min="0" max="1023" step="1" value="5" aria-label="Sensor Minimum numerisch" required>
            </div>
            <span class="input-error">Bitte einen Wert zwischen 0 und 1023 eingeben</span>
          </div>
          <div class="range-wrapper">
            <label for="sensorMax">Sensor Max. (hell)</label>
            <div class="range-control">
              <input id="sensorMax" type="range" min="0" max="1023" step="1" aria-label="Sensor Maximum">
              <input id="sensorMaxInput" type="number" min="0" max="1023" step="1" value="450" aria-label="Sensor Maximum numerisch" required>
            </div>
            <span class="input-error">Bitte einen Wert zwischen 0 und 1023 eingeben</span>
          </div>
        </div>
        <button id="saveAuto" aria-label="Auto-Brightness Einstellungen speichern">Auto-Brightness speichern</button>
      </div>
    </div>

    <div class="accordion" role="region" aria-label="MQTT Präsenzmelder">
      <div class="accordion-header" role="button" tabindex="0" aria-expanded="false" aria-controls="mqttContent">
        <h3>MQTT Präsenzmelder (Aqara)</h3>
        <span class="accordion-icon" aria-hidden="true">▼</span>
      </div>
      <div class="accordion-content" id="mqttContent">
        <label class="checkbox-wrapper">
          <input type="checkbox" id="mqttEnabled" aria-label="MQTT aktivieren">
          <span>MQTT aktivieren</span>
        </label>
        <div class="grid" style="margin-top: var(--spacing-2);">
          <div>
            <label for="mqttServer">MQTT Broker IP</label>
            <input id="mqttServer" type="text" placeholder="192.168.1.100" aria-label="MQTT Broker IP Adresse" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$">
            <span class="input-error">Bitte eine gültige IP-Adresse eingeben</span>
          </div>
          <div>
            <label for="mqttPort">MQTT Port</label>
            <input id="mqttPort" type="number" min="1" max="65535" value="1883" aria-label="MQTT Port" required>
            <span class="input-error">Bitte einen Port zwischen 1 und 65535 eingeben</span>
          </div>
          <div>
            <label for="mqttUser">MQTT User (optional)</label>
            <input id="mqttUser" type="text" placeholder="username" aria-label="MQTT Benutzername">
          </div>
          <div>
            <label for="mqttPassword">MQTT Passwort (optional)</label>
            <input id="mqttPassword" type="password" placeholder="password" aria-label="MQTT Passwort">
          </div>
        </div>
        <div style="margin-top: var(--spacing-2);">
          <label for="mqttTopic">MQTT Topic (Präsenz)</label>
          <input id="mqttTopic" type="text" placeholder="zigbee2mqtt/aqara_fp2" value="zigbee2mqtt/aqara_fp2" aria-label="MQTT Topic für Präsenz">
        </div>
        <div class="range-wrapper" style="margin-top: var(--spacing-2);">
          <label for="presenceTimeout">Display-Timeout nach Präsenz (Sekunden)</label>
          <div class="range-control">
            <input id="presenceTimeout" type="range" min="10" max="600" step="10" value="300" aria-label="Präsenz Timeout">
            <input id="presenceTimeoutInput" type="number" min="10" max="3600" step="10" value="300" aria-label="Präsenz Timeout numerisch" required>
          </div>
          <span class="input-error">Bitte einen Wert zwischen 10 und 3600 Sekunden eingeben</span>
        </div>
        <button id="saveMqtt" aria-label="MQTT Einstellungen speichern">MQTT Einstellungen speichern</button>
        <p class="caption" style="margin: var(--spacing-2) 0 0 0; line-height: 1.5;">
          <strong>Hinweis:</strong> Nach dem Speichern wird die MQTT-Verbindung neu aufgebaut. Das Display schaltet sich automatisch aus, wenn keine Präsenz erkannt wird.<br>
          <strong>Aqara FP2:</strong> Topic ist <code>zigbee2mqtt/[dein_sensor_name]</code> (ohne /presence oder /occupancy). Der Code erkennt JSON automatisch.
        </p>
      </div>
    </div>

    <div class="accordion" role="region" aria-label="Backup und Restore">
      <div class="accordion-header" role="button" tabindex="0" aria-expanded="false" aria-controls="backupContent">
        <h3>Backup & Restore</h3>
        <span class="accordion-icon" aria-hidden="true">▼</span>
      </div>
      <div class="accordion-content" id="backupContent">
        <p class="caption" style="margin-bottom: var(--spacing-2);">
          Erstellen Sie ein Backup Ihrer Konfiguration oder stellen Sie eine gespeicherte Konfiguration wieder her.
        </p>
        <div style="display: flex; gap: var(--spacing-2); flex-wrap: wrap;">
          <button id="backupButton" aria-label="Konfiguration als Backup herunterladen">Backup herunterladen</button>
          <label for="restoreFile" style="flex: 1; min-width: 200px;">
            <input type="file" id="restoreFile" accept=".json" style="display: none;" aria-label="Backup-Datei auswählen">
            <button onclick="document.getElementById('restoreFile').click()" style="width: 100%;" aria-label="Backup wiederherstellen">Backup wiederherstellen</button>
          </label>
        </div>
        <p class="caption" style="margin-top: var(--spacing-2); color: var(--warning);">
          <strong>Warnung:</strong> Das Wiederherstellen überschreibt alle aktuellen Einstellungen!
        </p>
      </div>
    </div>

    <footer>
      <span>Status-Werte werden alle 2&nbsp;Sekunden aktualisiert &mdash; Einstellungen bleiben editierbar</span>
    </footer>
  </main>

  <div class="toast-container" id="toastContainer" aria-live="polite" aria-atomic="true"></div>

  <script>
    function showToast(message, type = 'info') {
      const container = document.getElementById('toastContainer');
      const toast = document.createElement('div');
      toast.className = `toast ${type}`;
      toast.textContent = message;
      toast.setAttribute('role', 'alert');
      container.appendChild(toast);

      setTimeout(() => {
        toast.style.animation = 'toastSlideIn 0.3s ease reverse';
        setTimeout(() => toast.remove(), 300);
      }, 3000);
    }

    document.querySelectorAll('.accordion-header').forEach(header => {
      header.addEventListener('click', () => {
        const accordion = header.parentElement;
        const isOpen = accordion.classList.contains('open');
        accordion.classList.toggle('open');
        header.setAttribute('aria-expanded', !isOpen);
      });

      header.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          header.click();
        }
      });
    });

    const effectCards = document.querySelectorAll('.effect-card');
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

    function setActiveEffect(effect) {
      effectCards.forEach(card => {
        card.classList.remove('active');
        if (card.dataset.effect === effect) {
          card.classList.add('active');
        }
      });
    }

    effectCards.forEach(card => {
      card.addEventListener('click', () => {
        const effect = card.dataset.effect;
        applyEffect(effect);
      });

      card.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          card.click();
        }
      });
    });

    const timeEl = document.getElementById('time');
    const currentEffectEl = document.getElementById('currentEffect');
    const currentBrightnessEl = document.getElementById('currentBrightness');
    const currentTimezoneEl = document.getElementById('currentTimezone');
    const sensorValueEl = document.getElementById('sensorValue');
    const autoStatusEl = document.getElementById('autoStatus');
    const tzSelect = document.getElementById('tz');
    const hourFormatSelect = document.getElementById('hourFormat');
    const setTzButton = document.getElementById('setTz');
    const brightnessSlider = document.getElementById('brightness');
    const brightnessInput = document.getElementById('brightnessInput');
    const brightnessPreview = document.getElementById('brightnessPreview');
    const saveBrightnessButton = document.getElementById('saveBrightness');

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
    const otaStatusEl = document.getElementById('otaStatus');
    const ipAddressEl = document.getElementById('ipAddress');
    const restartCountEl = document.getElementById('restartCount');
    const lastResetReasonEl = document.getElementById('lastResetReason');

    let brightnessDebounce;
    const editingFields = new Set();

    function prettifyEffect(effect) {
      return effectLabels[effect] || (effect.charAt(0).toUpperCase() + effect.slice(1));
    }

    function syncSliderInput(slider, input, value) {
      slider.value = value;
      input.value = value;
    }

    function safeUpdate(fieldName, slider, input, value) {
      if (!editingFields.has(fieldName)) {
        syncSliderInput(slider, input, value);
      }
    }

    function updateBrightnessUI(value) {
      currentBrightnessEl.textContent = value;
      safeUpdate('brightness', brightnessSlider, brightnessInput, value);
      const percent = (value / 1023) * 100;
      brightnessPreview.style.setProperty('--brightness-percent', percent + '%');
    }

    function setButtonLoading(button, loading) {
      if (loading) {
        button.classList.add('loading');
        button.disabled = true;
      } else {
        button.classList.remove('loading');
        button.disabled = false;
      }
    }

    function updateStatusBadge(element, status, value) {
      element.className = 'status-badge';
      if (status === 'success' || value === true || value === 'An' || value === 'Verbunden' || value === 'Erkannt') {
        element.className += ' badge-success';
      } else if (status === 'warning' || value === 'Getrennt') {
        element.className += ' badge-warning';
      } else if (status === 'error' || value === false || value === 'Aus' || value === 'Nicht erkannt') {
        element.className += ' badge-error';
      } else if (status === 'info') {
        element.className += ' badge-info';
      } else {
        element.className += ' badge-neutral';
      }
    }

    async function fetchJson(url) {
      const response = await fetch(url, { cache: 'no-store' });
      if (!response.ok) {
        throw new Error('Netzwerkfehler: ' + response.status);
      }
      return response.json();
    }

    async function loadSettings() {
      try {
        const data = await fetchJson('/api/status');

        tzSelect.value = data.tz || 'CET-1CEST-2,M3.5.0/02,M10.5.0/03';
        currentTimezoneEl.textContent = data.tz || '-';
        const formatValue = data.hourFormat ? data.hourFormat : (data.use24HourFormat === false ? '12h' : '24h');
        hourFormatSelect.value = (formatValue && formatValue.startsWith('12')) ? '12' : '24';

        setActiveEffect(data.effect);

        if (data.autoBrightness !== undefined) {
          autoEnabled.checked = data.autoBrightness;
        }
        if (data.minBrightness !== undefined) {
          syncSliderInput(minBrightnessSlider, minBrightnessInput, data.minBrightness);
        }
        if (data.maxBrightness !== undefined) {
          syncSliderInput(maxBrightnessSlider, maxBrightnessInput, data.maxBrightness);
        }
        if (data.sensorMin !== undefined) {
          syncSliderInput(sensorMinSlider, sensorMinInput, data.sensorMin);
        }
        if (data.sensorMax !== undefined) {
          syncSliderInput(sensorMaxSlider, sensorMaxInput, data.sensorMax);
        }

        if (data.mqttEnabled !== undefined) {
          mqttEnabledCheckbox.checked = data.mqttEnabled;
        }
        if (data.mqttServer !== undefined) {
          mqttServerInput.value = data.mqttServer;
        }
        if (data.mqttPort !== undefined) {
          mqttPortInput.value = data.mqttPort;
        }
        if (data.mqttTopic !== undefined) {
          mqttTopicInput.value = data.mqttTopic;
        }
        if (data.presenceTimeout !== undefined) {
          const timeoutSeconds = Math.floor(data.presenceTimeout / 1000);
          syncSliderInput(presenceTimeoutSlider, presenceTimeoutInput, timeoutSeconds);
        }
      } catch (error) {
        showToast('Settings konnte nicht geladen werden. ' + error.message, 'error');
      }
    }

    async function refreshStatus() {
      try {
        const data = await fetchJson('/api/status');

        timeEl.textContent = data.time;
        currentEffectEl.textContent = prettifyEffect(data.effect);
        setActiveEffect(data.effect);
        if (data.hourFormat || data.use24HourFormat !== undefined) {
          const formatValue = data.hourFormat ? data.hourFormat : (data.use24HourFormat ? '24h' : '12h');
          hourFormatSelect.value = formatValue.startsWith('12') ? '12' : '24';
        }

        updateBrightnessUI(data.brightness);

        if (data.sensorValue !== undefined) {
          sensorValueEl.textContent = data.sensorValue;
        }
        if (data.autoBrightness !== undefined) {
          const status = data.autoBrightness ? 'An' : 'Aus';
          autoStatusEl.textContent = status;
          updateStatusBadge(autoStatusEl, data.autoBrightness ? 'success' : 'neutral', status);
        }

        if (data.mqttConnected !== undefined) {
          if (data.mqttConnected) {
            mqttStatusEl.textContent = 'Verbunden';
            updateStatusBadge(mqttStatusEl, 'success', 'Verbunden');
          } else if (data.mqttEnabled) {
            mqttStatusEl.textContent = 'Getrennt';
            updateStatusBadge(mqttStatusEl, 'warning', 'Getrennt');
          } else {
            mqttStatusEl.textContent = 'Aus';
            updateStatusBadge(mqttStatusEl, 'neutral', 'Aus');
          }
        }

        if (data.presenceDetected !== undefined) {
          const status = data.presenceDetected ? 'Erkannt' : 'Nicht erkannt';
          presenceStatusEl.textContent = status;
          updateStatusBadge(presenceStatusEl, data.presenceDetected ? 'success' : 'neutral', status);
        }

        if (data.displayEnabled !== undefined) {
          const status = data.displayEnabled ? 'An' : 'Aus';
          displayStatusEl.textContent = status;
          updateStatusBadge(displayStatusEl, data.displayEnabled ? 'success' : 'error', status);
        }

        if (data.otaEnabled !== undefined) {
          const status = data.otaEnabled ? 'Aktiv' : 'Inaktiv';
          otaStatusEl.textContent = status;
          updateStatusBadge(otaStatusEl, data.otaEnabled ? 'success' : 'neutral', status);
          if (data.otaHostname) {
            otaStatusEl.title = 'Hostname: ' + data.otaHostname + (data.ipAddress ? ' | IP: ' + data.ipAddress : '');
          }
        }

        if (data.ipAddress !== undefined) {
          ipAddressEl.textContent = data.ipAddress;
          ipAddressEl.title = 'IP-Adresse für OTA-Updates: ' + data.ipAddress;
        }

        if (data.restartCount !== undefined) {
          restartCountEl.textContent = data.restartCount;
          restartCountEl.title = 'Anzahl der Neustarts seit letztem Reset des Counters';
        }

        if (data.lastResetReason !== undefined) {
          lastResetReasonEl.textContent = data.lastResetReason || '-';
          lastResetReasonEl.title = 'Grund des letzten Neustarts';
        }
        if (data.lastUptimeBeforeRestart !== undefined) {
          const uptimeEl = document.getElementById('lastUptimeBeforeRestart');
          if (uptimeEl) {
            if (data.lastUptimeBeforeRestartHours !== undefined && data.lastUptimeBeforeRestartMinutes !== undefined) {
              uptimeEl.textContent = `${data.lastUptimeBeforeRestartHours}h ${data.lastUptimeBeforeRestartMinutes}m`;
            } else {
              uptimeEl.textContent = '-';
            }
          }
        }
        if (data.lastHeapBeforeRestart !== undefined) {
          const heapEl = document.getElementById('lastHeapBeforeRestart');
          if (heapEl) {
            if (data.lastHeapBeforeRestartKB !== undefined) {
              heapEl.textContent = `${data.lastHeapBeforeRestartKB} KB`;
            } else {
              heapEl.textContent = '-';
            }
          }
        }
      } catch (error) {
        showToast('Status konnte nicht geladen werden. ' + error.message, 'error');
      }
    }

    async function applyEffect(effect) {
      try {
        setButtonLoading(saveBrightnessButton, true);
        await fetch('/effect/' + encodeURIComponent(effect));
        currentEffectEl.textContent = prettifyEffect(effect);
        setActiveEffect(effect);
        showToast('Effekt aktualisiert: ' + prettifyEffect(effect), 'success');
      } catch (error) {
        showToast('Effektwechsel fehlgeschlagen.', 'error');
      } finally {
        setButtonLoading(saveBrightnessButton, false);
      }
    }

    async function updateTimezone() {
      const tz = tzSelect.value.trim();
      const format = hourFormatSelect.value === '12' ? '12' : '24';
      if (!tz) {
        showToast('Bitte eine gültige Zeitzone auswählen.', 'error');
        return;
      }
      try {
        setButtonLoading(setTzButton, true);
        const responses = await Promise.all([
          fetch('/api/setTimezone?tz=' + encodeURIComponent(tz)),
          fetch('/api/setClockFormat?format=' + format)
        ]);

        if (responses.some(r => !r.ok)) {
          throw new Error('Server hat die Einstellungen nicht akzeptiert.');
        }
        currentTimezoneEl.textContent = tz;
        showToast('Zeitzone und Zeitformat gespeichert.', 'success');
      } catch (error) {
        showToast('Zeitzone/Zeitformat konnte nicht gespeichert werden.', 'error');
      } finally {
        setButtonLoading(setTzButton, false);
      }
    }

    async function saveAutoBrightness() {
      if (parseInt(minBrightnessInput.value) > parseInt(maxBrightnessInput.value)) {
        showToast('Min. Helligkeit darf nicht größer als Max. Helligkeit sein.', 'error');
        return;
      }
      if (parseInt(sensorMinInput.value) > parseInt(sensorMaxInput.value)) {
        showToast('Sensor Min. darf nicht größer als Sensor Max. sein.', 'error');
        return;
      }
      
      try {
        setButtonLoading(saveAutoButton, true);
        const params = new URLSearchParams({
          enabled: autoEnabled.checked ? 'true' : 'false',
          min: minBrightnessInput.value,
          max: maxBrightnessInput.value,
          sensorMin: sensorMinInput.value,
          sensorMax: sensorMaxInput.value
        });
        await fetch('/api/setAutoBrightness?' + params.toString());
        showToast('Auto-Brightness Einstellungen gespeichert.', 'success');
      } catch (error) {
        showToast('Auto-Brightness konnte nicht gespeichert werden.', 'error');
      } finally {
        setButtonLoading(saveAutoButton, false);
      }
    }

    async function saveMqtt() {
      if (mqttServerInput.value && !mqttServerInput.validity.valid) {
        showToast('Bitte eine gültige IP-Adresse eingeben.', 'error');
        return;
      }
      if (!mqttPortInput.validity.valid) {
        showToast('Bitte einen gültigen Port eingeben.', 'error');
        return;
      }
      if (!presenceTimeoutInput.validity.valid) {
        showToast('Bitte einen gültigen Timeout-Wert eingeben.', 'error');
        return;
      }
      
      try {
        setButtonLoading(saveMqttButton, true);
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
        showToast('MQTT Einstellungen gespeichert. Verbindung wird neu aufgebaut...', 'info');
      } catch (error) {
        showToast('MQTT konnte nicht gespeichert werden.', 'error');
      } finally {
        setButtonLoading(saveMqttButton, false);
      }
    }

    async function saveBrightness(value) {
      if (!brightnessInput.validity.valid) {
        showToast('Bitte einen gültigen Helligkeitswert eingeben (0-1023).', 'error');
        return;
      }
      try {
        await fetch('/api/setBrightness?b=' + encodeURIComponent(value));
        showToast('Helligkeit gespeichert.', 'success');
      } catch (error) {
        showToast('Helligkeit konnte nicht gespeichert werden.', 'error');
      }
    }

    setTzButton.addEventListener('click', updateTimezone);

    brightnessSlider.addEventListener('input', event => {
      const value = event.target.value;
      brightnessInput.value = value;
      updateBrightnessUI(value);
      clearTimeout(brightnessDebounce);
      brightnessDebounce = setTimeout(() => saveBrightness(value), 400);
    });

    brightnessInput.addEventListener('input', event => {
      const value = Math.max(0, Math.min(1023, parseInt(event.target.value) || 0));
      brightnessSlider.value = value;
      updateBrightnessUI(value);
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

    autoEnabled.addEventListener('focus', () => editingFields.add('autoEnabled'));
    autoEnabled.addEventListener('blur', () => editingFields.delete('autoEnabled'));
    autoEnabled.addEventListener('change', () => {
      saveAutoBrightness();
    });

    saveAutoButton.addEventListener('click', () => {
      saveAutoBrightness();
    });

    mqttEnabledCheckbox.addEventListener('focus', () => editingFields.add('mqttEnabled'));
    mqttEnabledCheckbox.addEventListener('blur', () => editingFields.delete('mqttEnabled'));
    mqttServerInput.addEventListener('focus', () => editingFields.add('mqttServer'));
    mqttServerInput.addEventListener('blur', () => editingFields.delete('mqttServer'));
    mqttPortInput.addEventListener('focus', () => editingFields.add('mqttPort'));
    mqttPortInput.addEventListener('blur', () => editingFields.delete('mqttPort'));
    mqttTopicInput.addEventListener('focus', () => editingFields.add('mqttTopic'));
    mqttTopicInput.addEventListener('blur', () => editingFields.delete('mqttTopic'));

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

    const backupButton = document.getElementById('backupButton');
    const restoreFile = document.getElementById('restoreFile');

    backupButton.addEventListener('click', async () => {
      try {
        const response = await fetch('/api/backup');
        if (!response.ok) {
          throw new Error('Backup konnte nicht erstellt werden');
        }
        const blob = await response.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'ikea-clock-backup-' + new Date().toISOString().slice(0, 10) + '.json';
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
        showToast('Backup erfolgreich heruntergeladen', 'success');
      } catch (error) {
        showToast('Backup fehlgeschlagen: ' + error.message, 'error');
      }
    });

    restoreFile.addEventListener('change', async (event) => {
      const file = event.target.files[0];
      if (!file) return;

      if (!confirm('Möchten Sie wirklich die Konfiguration wiederherstellen? Alle aktuellen Einstellungen werden überschrieben!')) {
        event.target.value = '';
        return;
      }

      try {
        const text = await file.text();
        const response = await fetch('/api/restore', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: text
        });

        if (!response.ok) {
          const error = await response.json();
          throw new Error(error.error || 'Restore fehlgeschlagen');
        }

        const result = await response.json();
        showToast('Konfiguration wiederhergestellt. Seite wird neu geladen...', 'success');
        setTimeout(() => {
          window.location.reload();
        }, 2000);
      } catch (error) {
        showToast('Restore fehlgeschlagen: ' + error.message, 'error');
        event.target.value = '';
      }
    });

    const resetRestartCountButton = document.getElementById('resetRestartCount');
    async function resetRestartCount() {
      if (!confirm('Möchten Sie den Restart-Counter wirklich zurücksetzen?')) {
        return;
      }
      
      try {
        setButtonLoading(resetRestartCountButton, true);
        const response = await fetch('/api/resetRestartCount', {
          method: 'GET',
          cache: 'no-store'
        });
        
        if (!response.ok) {
          const error = await response.json();
          throw new Error(error.error || 'Fehler beim Zurücksetzen');
        }
        
        const result = await response.json();
        restartCountEl.textContent = result.restartCount || 0;
        showToast('Restart-Counter zurückgesetzt', 'success');
      } catch (error) {
        showToast('Fehler: ' + error.message, 'error');
      } finally {
        setButtonLoading(resetRestartCountButton, false);
      }
    }
    
    resetRestartCountButton.addEventListener('click', () => {
      resetRestartCount();
    });

    loadSettings();
    refreshStatus();
    setInterval(refreshStatus, 2000);
  </script>
</body>
</html>
)rawl";

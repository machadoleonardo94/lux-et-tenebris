//? Lightpack USB console.
//?
//? Static HTML/CSS/ES modules with no build step. One vendor bundle
//? (esptool-js, Apache-2.0) is used only by the Recovery button; the normal
//? update still rides the board's @UPDATE link. Chrome or Edge on a desktop is
//? the only requirement, because Web Serial is the only way a page can open the
//? C3's USB port.
//!
//? The wire contract lives in docs/usb-control.md. Everything here is policy:
//? the handshake, request matching, batching edits into one SET, the debounced
//? COMMIT and the reconnect loop. Transport is js/serial.js, framing is
//? js/protocol.js, and the parameter metadata is js/spec.js.

import { ESPLoader, Transport } from '../vendor/esptool-js/bundle.js';
import {
  PALETTE_SWATCHES,
  PAGES,
  FIELD_SPECS,
  FIELD_KEYS,
  boundsFor,
  fieldsForPage,
} from './spec.js';
import {
  commands,
  buildSet,
  buildPresetDoc,
  parsePresetDoc,
  sanitizeConfig,
  parseLine,
} from './protocol.js';
import { SerialLink } from './serial.js';

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));

const MODE_NOTES = {
  solid: 'Palette colour, flat.',
  flame: 'Impact-driven flash and decay.',
  fade: 'Gyro drives colour and lit length.',
  whip: 'A travelling hump; a period above the crossing time keeps pulses discrete.',
  rainbow: 'Random colours drifting outward.',
};

const EDIT_DEBOUNCE_MS = 90;
const COMMIT_DEBOUNCE_MS = 900;
const UPDATE_MANIFEST_URL = '../releases/latest/manifest.json';

const app = {
  link: null,
  port: null,
  connected: false,
  connecting: false,
  manualDisconnect: false,
  expectReboot: false,

  pong: null,
  state: null,
  modes: [],
  presets: [],
  pending: {},
  touched: false,
  importing: false,

  seq: 0,
  requestSeq: 0,
  requests: new Map(),
  flushTimer: 0,
  commitTimer: 0,
  reconnectTimer: 0,
  reconnectAttempts: 0,

  autoCommit: loadSetting('lightpack.autoCommit', '1') !== '0',
  streamHz: Number(loadSetting('lightpack.streamHz', '10')) || 10,
  page: loadSetting('lightpack.page', 'shared'),
  showLog: loadSetting('lightpack.showLog', '1') !== '0',

  lastTickFrame: null,
  diagAt: 0,
  stressRunning: false,
  senseRunning: false,

  updateManifest: null,
  updateBusy: false,
  recoveryBusy: false,
};

//* ---------------------- BOOT ----------------------

function boot() {
  const params = new URLSearchParams(location.search);
  const demo = params.has('demo');
  app.demo = demo;

  if (params.has('page')) app.page = params.get('page');

  if (!demo) {
    const problem = browserProblem();
    if (problem) {
      showGate(problem);
      return;
    }
  }

  $('#app').hidden = false;
  buildNavigation();
  buildFields();
  buildSwatches();
  buildDiagnostics();
  bindEvents();
  restorePreferences();

  if (demo) {
    loadDemo();
  } else {
    renderAll();
    clearLive();
  }

  if (!demo) {
    navigator.serial
      .getPorts()
      .then((ports) => {
        if (ports.length > 0) {
          // A port the user has already granted: the Reconnect button can open
          // it without showing the picker again.
          app.port = ports[ports.length - 1];
          renderConnectionUI();
        }
      })
      .catch(() => {});
  }

  // A small test hook: ?check runs the update check on load. It is used to
  // verify the manifest path without a board.
  if (params.has('check')) {
    setTimeout(() => checkLatestBuild(), 250);
  }
}

//? `?demo` draws the console with a representative board state and no link.
//? It is for looking at the layout and for screenshots; every control that
//? would write is inert, because there is no board behind it.
function loadDemo() {
  app.demo = true;
  app.connected = false;
  app.pong = {
    t: 'pong', v: 1, proto: 1, fw: '0.4-pre (demo)',
    build: 'demo state', chip: 'ESP32-C3', cpu_mhz: 160,
    mac: 'AA:BB:CC:DD:EE:FF', uptime_ms: 7234000,
  };
  app.modes = [
    { id: 1, key: 'solid', label: 'Solid color' },
    { id: 2, key: 'flame', label: 'Step flame' },
    { id: 4, key: 'fade', label: 'Gyro fade' },
    { id: 5, key: 'whip', label: 'Pulse whip' },
    { id: 6, key: 'rainbow', label: 'Rainbow flow' },
  ];
  app.presets = [
    { slot: 0, used: true, state: 'ok', name: 'Bench warm' },
    { slot: 1, used: false, state: 'empty', name: '' },
    { slot: 2, used: true, state: 'ok', name: 'Case fitted' },
    { slot: 3, used: false, state: 'empty', name: '' },
    { slot: 4, used: false, state: 'incompatible', name: '' },
  ];
  app.state = {
    mode: 5, mode_key: 'whip', mode_label: 'Pulse whip',
    red: 255, green: 140, blue: 0, brightness: 180,
    budget: 1000, budget_ma: 1000, budget_min: 100, budget_default: 1000,
    budget_max: 2000, budget_brightness: 255,
    guard_scale: 255, guard_active: false, guard_floor_mv: 3700, guard_recover_mv: 3750,
    power_suggested_ma: 1614,
    rainbow_level: 128,
    whip_speed: 4.5, whip_length: 24, whip_period: 1200, whip_peak: 200, whip_offset: 0,
    whip_travel_ms: 1520, whip_in_flight: 1, whip_max_heads: 12,
    whip_speed_min: 0.1, whip_speed_max: 20, whip_length_min: 1,
    whip_period_min: 200, whip_period_max: 5000,
    rainbow_speed: 2, rainbow_min_ms: 400, rainbow_max_ms: 2000,
    rainbow_travel_ms: 6000, rainbow_band_min_leds: 3, rainbow_band_max_leds: 12,
    rainbow_gap_floor_ms: 50, rainbow_gap_ceiling_ms: 10000,
    rainbow_speed_min: 0.1, rainbow_speed_max: 20,
    num_leds: 250, active_leds: 60, leds: 60, strip_leds_min: 1,
    safe_boot: false, safe_pct: 70, stress_run: false,
    stress_result: 1, stress_phase: 2, stress_color: 7, stress_leds: 144, stress_brightness: 59,
    stress_est_ma: 1999, stress_v_start: 4050, stress_v_min: 3210, stress_sag_mv: 840,
    stress_v_off: 4050, stress_v_on: 3860, stress_r_mohm: 158, stress_elapsed_ms: 24000,
    stress_ceiling_ma: 2000, stress_step_ms: 500, stress_rail_floor_mv: 3000,
    sense_run: false, sense_count: 20, sense_burst_ms: 200, sense_ma: 1200,
    sense_v_off: 4168, sense_v_on: 4008, sense_r_mohm: 133, sense_elapsed_ms: 6200,
    net: 'offline', offline_mode: true, dirty: true,
    flame_decay: 0.9, flame_spatial: 2, flame_power_slope: 5,
    flame_power_max: 75, flame_power_idle: 5,
    fade_decay: 0.9, fade_trigger: 0.2, fade_length_gain: 20, fade_length_z_gain: 15,
    fade_length_base: 10, fade_power_gain: 2, fade_power_base: 5,
    fade_color_gain: 7, fade_idle_length: 5, fade_idle_power: 5,
    flame_decay_min: 0, flame_decay_max: 0.995,
    flame_spatial_min: 0, flame_spatial_max: 10, flame_power_slope_max: 25,
    step_baseline_tau_ms: 2000, step_threshold: 1.2, step_release: 0.45,
    step_jerk_min: 10, step_min_gap_ms: 200, step_impact_min: 1.5,
    step_impact_max: 6.5, step_magnitude_min: 3, step_magnitude_max: 30,
    step_baseline_tau_min_ms: 100, step_baseline_tau_max_ms: 20000,
    step_jerk_max: 500, step_jerk_floor: 0,
    step_threshold_min: 0.05, step_threshold_max: 20, step_release_min: 0.01,
    step_gap_min_ms: 20, step_gap_max_ms: 10000,
    step_impact_min_floor: 0, step_impact_max_ceiling: 50,
    step_cadence_max_sps: 500, flame_power_soft: 15, flame_power_hard: 75,
    uptime_ms: 7234000, loop_rate: 2400, rail_mv: 4012, free_heap: 123456, gyro: true, rssi: 0,
    ip: '0.0.0.0', hostname: 'lightpack', wifi_ssid: '',
  };

  renderAll();
  handleTick({
    t: 'tick', v: 1, ms: 7234000, mode: 5,
    ax: -0.12, ay: 0.03, az: 9.81, gx: 0.1, gy: -0.2, gz: 0.0,
    imp: 12, ic: 3, ia: 850, lp: 19, rail: 4012, heap: 123456,
    net: 0, dirty: true, btn: 0,
  });

  setText('connText', 'Demo');
  $('#connDot').classList.add('busy');
  setText('statusLeft', 'demo data — no board connected');
  log('demo mode: this is a representative state, not a connected board');
}

function browserProblem() {
  if (location.protocol === 'file:') {
    return {
      title: 'Open this page from a URL, not a file',
      body:
        'Browsers refuse Web Serial to pages opened from <code>file://</code>. ' +
        'The board is fine; the page just has to be served.',
      fix:
        'From the repository root run <code>python -m http.server 8000</code>, then open ' +
        '<code>http://localhost:8000/Firmware/lightpack-C3/console/</code>.',
    };
  }

  if (!('serial' in navigator)) {
    return {
      title: 'This browser has no Web Serial',
      body:
        'Chrome or Edge on a desktop is required. Firefox, Safari and every mobile ' +
        'browser lack the API this page needs, so there is no fallback inside them.',
      fix: 'Open the page in Chrome 89+ or Edge 89+. Any serial terminal can also send ' +
        'the same @-prefixed commands; see docs/usb-control.md.',
    };
  }

  if (!window.isSecureContext) {
    return {
      title: 'This page needs https:// or localhost',
      body:
        'Web Serial is restricted to secure contexts. A plain <code>http://192.168.x.x</code> ' +
        'address is not one, even on your own LAN.',
      fix:
        'Use the published https address, or serve the repository locally and open ' +
        '<code>http://localhost:8000/...</code>.',
    };
  }

  return null;
}

function showGate(problem) {
  $('#gate').hidden = false;
  $('#app').hidden = true;
  $('#gateTitle').textContent = problem.title;
  $('#gateBody').innerHTML = problem.body;
  $('#gateFix').innerHTML = problem.fix;
}

//* ---------------------- SETUP ----------------------

function buildNavigation() {
  const nav = $('#nav');
  nav.innerHTML = '';

  for (const page of PAGES) {
    if (page.id === 'presets') {
      const sep = document.createElement('div');
      sep.className = 'nav-sep';
      nav.append(sep);
    }

    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'nav-btn';
    button.dataset.page = page.id;

    const label = document.createElement('span');
    label.textContent = page.label;
    button.append(label);

    button.addEventListener('click', () => showPage(page.id));
    nav.append(button);
  }
}

function buildFields() {
  for (const page of PAGES) {
    const container = document.getElementById(`${page.id}Fields`);
    if (!container) continue;

    for (const key of fieldsForPage(page.id)) {
      container.append(buildFieldRow(key));
    }
  }
}

function buildFieldRow(key) {
  const spec = FIELD_SPECS[key];
  const row = document.createElement('div');
  row.className = 'field';
  row.dataset.field = key;

  const label = document.createElement('label');
  label.htmlFor = `f-${key}`;
  label.textContent = spec.label;
  label.title = spec.help;

  const input = document.createElement('input');
  input.type = 'number';
  input.id = `f-${key}`;
  input.dataset.field = key;
  input.title = spec.help;

  const range = document.createElement('span');
  range.className = 'range';

  input.addEventListener('input', () => {
    const value = Number(input.value);
    if (!Number.isFinite(value)) return;
    queueEdit(key, value);
  });

  input.addEventListener('blur', () => {
    // Show the clamped value the batch actually carries, not the raw keystrokes.
    if (app.pending[key] !== undefined) {
      input.value = formatForInput(app.pending[key], boundsFor(key, app.state));
    } else {
      syncField(key);
    }
  });

  input.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') input.blur();
  });

  row.append(label, input, range);
  return row;
}

function buildSwatches() {
  const box = $('#swatches');
  box.innerHTML = '';

  for (const [name, red, green, blue] of PALETTE_SWATCHES) {
    const button = document.createElement('button');
    button.type = 'button';
    button.title = name;
    button.dataset.rgb = `${red},${green},${blue}`;
    button.style.background = `rgb(${red} ${green} ${blue})`;
    button.addEventListener('click', () => queuePalette(red, green, blue));
    box.append(button);
  }
}

function buildDiagnostics() {
  const fields = [
    ['Firmware', 'diag-fw'],
    ['Build', 'diag-build'],
    ['Chip', 'diag-chip'],
    ['Port', 'diag-port'],
    ['Protocol', 'diag-proto'],
    ['Uptime', 'diag-uptime'],
    ['Active mode', 'diag-mode'],
    ['Loop rate', 'diag-loop'],
    ['LED rail', 'diag-rail'],
    ['Free heap', 'diag-heap'],
    ['Network', 'diag-net'],
    ['RSSI', 'diag-rssi'],
    ['IP', 'diag-ip'],
    ['Hostname', 'diag-host'],
    ['Saved SSID', 'diag-ssid'],
    ['Flash state', 'diag-save'],
  ];

  const list = $('#diag');
  list.innerHTML = '';
  for (const [label, id] of fields) {
    const dt = document.createElement('dt');
    dt.textContent = label;
    const dd = document.createElement('dd');
    dd.id = id;
    dd.textContent = '—';
    list.append(dt, dd);
  }
}

function restorePreferences() {
  $('#autoCommit').checked = app.autoCommit;
  $('#streamHz').value = String(app.streamHz);
  $('#showLog').checked = app.showLog;
  $('#streamOn').checked = false;
  $('#showLog').addEventListener('change', () => {
    app.showLog = $('#showLog').checked;
    storeSetting('lightpack.showLog', app.showLog ? '1' : '0');
  });
  if (!app.showLog) $('#log').hidden = true;

  showPage(app.page);
}

function bindEvents() {
  $('#btnConnect').addEventListener('click', () => {
    if (app.connected) disconnect();
    else connect();
  });

  $('#btnReconnect').addEventListener('click', () => tryReconnect());

  $('#autoCommit').addEventListener('change', () => {
    app.autoCommit = $('#autoCommit').checked;
    storeSetting('lightpack.autoCommit', app.autoCommit ? '1' : '0');
    if (app.autoCommit && app.state?.dirty) {
      scheduleCommit();
    } else if (!app.autoCommit) {
      clearTimeout(app.commitTimer);
      app.commitTimer = 0;
    }
  });

  $('#streamOn').addEventListener('change', () => applyStream());

  $('#streamHz').addEventListener('change', () => {
    app.streamHz = Number($('#streamHz').value) || 10;
    storeSetting('lightpack.streamHz', String(app.streamHz));
    if ($('#streamOn').checked) applyStream();
  });

  $('#btnSave').addEventListener('click', () => commit());
  $('#btnRevert').addEventListener('click', () => revert());

  $('#bannerClose').addEventListener('click', clearError);

  $('#colorPicker').addEventListener('input', () => {
    const rgb = hexToRgb($('#colorPicker').value);
    $('#paletteHex').textContent = $('#colorPicker').value;
    queuePalette(rgb[0], rgb[1], rgb[2]);
  });

  $('#btnImpact').addEventListener('click', () => previewImpact());
  $('#impactMag').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') previewImpact();
  });

  $('#btnStressRun').addEventListener('click', () => runStressTest());
  $('#btnSafeOff').addEventListener('click', () => clearSafeMode());
  $('#btnSenseRun').addEventListener('click', () => runSense());

  $('#btnPresetExport').addEventListener('click', () => exportPresets());
  $('#presetFile').addEventListener('change', (event) => {
    const file = event.target.files?.[0];
    event.target.value = '';
    if (file) importPresets(file);
  });

  $('#radioOn').addEventListener('change', () => applyRadio());

  $('#btnCheckUpdate').addEventListener('click', () => checkLatestBuild());
  $('#btnUpdateDisconnect').addEventListener('click', async () => {
    if (app.connected) await disconnect();
    renderUpdatePanel();
  });
  $('#btnInstallUpdate').addEventListener('click', () => installLatest());

  $('#btnReboot').addEventListener('click', () => rebootBoard());
  $('#btnCopyDiag').addEventListener('click', () => copyDiagnostics());

  $('#btnLogClear').addEventListener('click', () => {
    $('#log').innerHTML = '';
  });
  $('#btnLogDownload').addEventListener('click', () => saveLog());

  window.addEventListener('beforeunload', (event) => {
    if (app.connected && (app.state?.dirty || Object.keys(app.pending).length > 0)) {
      event.preventDefault();
      event.returnValue = '';
    }
  });
}

function showPage(id) {
  if (!PAGES.some((page) => page.id === id)) id = 'shared';

  app.page = id;
  storeSetting('lightpack.page', id);

  for (const page of PAGES) {
    const element = document.getElementById(`page-${page.id}`);
    if (element) element.hidden = page.id !== id;
  }
  for (const button of $$('#nav .nav-btn')) {
    button.classList.toggle('on', button.dataset.page === id);
  }
}

//* ---------------------- CONNECTION ----------------------

function makeLink() {
  return new SerialLink({
    onLine: handleLine,
    onLog: (text) => log(text),
    onStatus: handleLinkStatus,
  });
}

async function connect() {
  if (app.demo || app.connected || app.connecting) return;

  app.manualDisconnect = false;
  app.reconnectAttempts = 0;
  clearTimeout(app.reconnectTimer);
  app.connecting = true;
  setStatus('connecting', 'choose a port');
  renderConnectionUI();

  let port;
  try {
    port = await navigator.serial.requestPort();
  } catch (error) {
    app.connecting = false;
    setStatus('idle', 'disconnected');
    if (error?.name !== 'NotFoundError') {
      showError(`Could not choose a port: ${error.message || error}`);
    }
    renderConnectionUI();
    return;
  }

  const link = makeLink();
  try {
    await link.openPort(port);
  } catch (error) {
    await link.dispose().catch(() => {});
    app.connecting = false;
    app.link = null;
    app.connected = false;
    showError(`Could not open the port: ${error.message || error}`);
    setStatus('error', 'open failed');
    renderConnectionUI();
    return;
  }

  app.link = link;
  app.port = port;
  app.connected = true;

  try {
    await handshake();
  } catch (error) {
    await link.dispose().catch(() => {});
    app.link = null;
    app.connected = false;
    showError(`The port opened, but the handshake failed: ${error.message}`);
    setStatus('error', 'handshake failed');
  }

  app.connecting = false;
  renderConnectionUI();
}

async function disconnect() {
  app.manualDisconnect = true;
  app.reconnectAttempts = 0;
  clearTimeout(app.reconnectTimer);

  if (app.connected && app.state?.dirty) {
    try {
      await commit();
    } catch {}
  }

  clearTimeout(app.commitTimer);
  clearTimeout(app.flushTimer);
  app.pending = {};
  app.touched = false;

  if (app.link) await app.link.dispose().catch(() => {});
  app.link = null;
  app.connected = false;
  app.connecting = false;

  clearLive();
  renderConnectionUI();
  renderDirty();
  setStatus('idle', 'disconnected');
}

function handleLinkStatus(status) {
  if (status.state === 'connected') {
    app.connected = true;
    setStatus('connected', 'connected');
    renderConnectionUI();
    return;
  }

  if (status.state === 'closed') {
    if (!app.manualDisconnect) {
      app.connected = false;
      renderConnectionUI();
    }
    return;
  }

  if (status.state === 'disconnected') {
    const reason = status.reason || 'The port closed.';
    const wasConnected = app.connected;
    app.connected = false;
    clearLive();
    renderConnectionUI();
    log(reason, 'warn');

    if (app.manualDisconnect) return;

    // The old link still owns the SerialPort and its reader/writer locks. Drop
    // it before a reconnect tries to open the same port, or Chrome will answer
    // "the port is already open".
    const oldLink = app.link;
    app.link = null;
    if (oldLink) oldLink.dispose().catch(() => {});

    if (app.expectReboot) {
      app.expectReboot = false;
      setStatus('connecting', 'waiting for the board to restart');
      scheduleReconnect(1200);
      return;
    }

    if (wasConnected) {
      showError(reason);
      setStatus('error', reason);
      scheduleReconnect(1200);
    }
  }
}

function scheduleReconnect(delay) {
  clearTimeout(app.reconnectTimer);
  if (app.manualDisconnect || !app.port) return;

  if (app.reconnectAttempts >= 8) {
    app.reconnectAttempts = 0;
    setStatus('error', 'board not found — choose Connect to try again');
    renderConnectionUI();
    return;
  }

  app.reconnectTimer = setTimeout(() => tryReconnect(), delay || 1200);
}

async function tryReconnect() {
  if (app.connected || app.connecting || app.manualDisconnect || !app.port) return;

  app.connecting = true;
  app.reconnectAttempts++;
  setStatus('connecting', 'reconnecting');
  renderConnectionUI();

  const link = makeLink();
  try {
    await link.openPort(app.port);
    app.link = link;
    app.connected = true;
    await handshake();
    app.reconnectAttempts = 0;
  } catch {
    await link.dispose().catch(() => {});
    app.link = null;
    app.connected = false;
    setStatus('connecting', 'waiting for the board');
    scheduleReconnect(1400);
  } finally {
    app.connecting = false;
    renderConnectionUI();
  }
}

async function pingUntilAnswered(timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  let lastError = null;

  while (Date.now() < deadline) {
    try {
      return await request(commands.ping(), (frame) => frame.t === 'pong', 1800);
    } catch (error) {
      lastError = error;
    }
  }

  throw new Error(lastError?.message || 'The board did not answer.');
}

async function handshake() {
  clearError();
  setSync('handshaking with the board…');

  // The board may still be in setup() when the port opens, and the config
  // portal is pumped for up to a minute. Opening a CDC port can also reset the
  // C3 on some hosts, which costs a few seconds of boot. Those are exactly the
  // boots this console exists for, so keep asking until the deadline rather
  // than sending one PING into the reset and giving up.
  const pong = await pingUntilAnswered(12000);
  if (Number(pong.proto) !== 1) {
    throw new Error(
      `The firmware speaks protocol ${pong.proto}, this console speaks 1. ` +
      'Update the console or the board so both match.'
    );
  }
  app.pong = pong;

  const modes = await request(commands.modes(), (frame) => frame.t === 'modes', 3000);
  app.modes = Array.isArray(modes.modes) ? modes.modes : [];

  const state = await request(commands.get(), (frame) => frame.t === 'state', 4000);
  app.state = state.state;
  app.touched = false;

  const presets = await request(commands.presetList(), (frame) => frame.t === 'presets', 3000);
  app.presets = Array.isArray(presets.slots) ? presets.slots : [];

  renderAll();
  setStatus('connected', `connected · ${app.pong.fw}`);
  setSync('connected');
  log(`connected: ${app.pong.chip}, fw ${app.pong.fw}, proto ${app.pong.proto}`);
  if (Number(app.pong.uptime_ms) < 15000) {
    log('note: opening the CDC port resets the C3 (DTR); the handshake waited for the boot');
  }

  if ($('#streamOn').checked) await applyStream();
}

//* ---------------------- REQUESTS ----------------------

async function send(line) {
  if (!app.link) throw new Error('Not connected.');
  await app.link.send(line);
}

function request(line, match, timeoutMs = 2000) {
  if (!app.connected) return Promise.reject(new Error('Not connected.'));

  return new Promise((resolve, reject) => {
    const id = ++app.requestSeq;
    const timer = setTimeout(() => {
      app.requests.delete(id);
      reject(new Error('The board did not answer.'));
    }, timeoutMs);

    app.requests.set(id, { match, resolve, reject, timer });
    send(line).catch((error) => {
      clearTimeout(timer);
      app.requests.delete(id);
      reject(error);
    });
  });
}

function resolveRequests(frame) {
  for (const [id, pending] of app.requests) {
    let matches = false;
    try {
      matches = pending.match(frame);
    } catch {
      matches = false;
    }
    if (!matches) continue;

    clearTimeout(pending.timer);
    app.requests.delete(id);

    if (frame.t === 'ack' && frame.ok === false) {
      pending.reject(new Error(frame.err || 'the board refused the request'));
    } else {
      pending.resolve(frame);
    }
    return;
  }
}

//* ---------------------- FRAME HANDLING ----------------------

function handleLine(line) {
  if (!line) return;

  let parsed;
  try {
    parsed = parseLine(line);
  } catch {
    return;
  }

  if (parsed.kind === 'log') {
    log(parsed.text);
    return;
  }
  if (parsed.kind === 'bad') {
    log(parsed.text, 'err');
    return;
  }

  handleFrame(parsed.frame);
}

function handleFrame(frame) {
  if (frame.t === 'state') {
    app.state = frame.state;

    //* The firmware's live strip length is named `active_leds`, while the
    //? /api/config field (and the console's input) is `leds`. Normalise the one
    //* spelling into the other so an older board cannot blank the field.
    if (app.state && app.state.leds === undefined && app.state.active_leds !== undefined) {
      app.state.leds = app.state.active_leds;
    }

    app.touched = false;
    //* A state frame means the board is answering again: the blocking ramp has
    //? finished (or the page reconnected after it), so the local "running" badge
    //* must not stick.
    app.stressRunning = false;
    app.senseRunning = false;
    renderState();
  } else if (frame.t === 'presets') {
    app.presets = Array.isArray(frame.slots) ? frame.slots : [];
    renderPresets();
    renderConnectionUI();
  } else if (frame.t === 'pong') {
    app.pong = frame;
    renderIdentity();
  } else if (frame.t === 'ack') {
    handleAck(frame);
  } else if (frame.t === 'evt') {
    handleEvent(frame);
  } else if (frame.t === 'tick') {
    handleTick(frame);
  }

  resolveRequests(frame);
}

function handleAck(frame) {
  if (!frame.ok) {
    showError(`${frame.cmd || 'command'}: ${frame.err || 'refused'}`);
    // The batch was rejected before anything was applied, so the inputs may be
    // showing values the device does not hold. Re-read rather than leave them
    // lying; a GET is cheap and only happens on an error.
    if (frame.cmd === 'set') send(commands.get()).catch(() => {});
    return;
  }

  clearError();

  if (frame.cmd === 'set') {
    if (app.state && frame.changed) {
      for (const [key, value] of Object.entries(frame.changed)) {
        app.state[key] = value;
      }
      if (Object.prototype.hasOwnProperty.call(frame.changed, 'mode')) {
        setModeFromId(Number(frame.changed.mode));
      }
      renderState();
    }
    if (app.state) app.state.dirty = Boolean(frame.dirty);
    app.touched = Boolean(frame.dirty);
    renderDirty();
    renderDiagnostics();
    if (frame.dirty && app.autoCommit && !app.importing) scheduleCommit();
    if (frame.n) setSync(`applied ${frame.n} field${frame.n === 1 ? '' : 's'}`);
    return;
  }

  if (frame.cmd === 'commit') {
    if (app.state) app.state.dirty = false;
    app.touched = false;
    renderDirty();
    renderDiagnostics();
    setSync('saved to flash');
    log('configuration saved to flash');
    return;
  }

  if (frame.cmd === 'radio') {
    if (app.state) {
      app.state.offline_mode = Boolean(frame.offline);
      app.state.net = frame.net;
      app.state.rssi = frame.rssi;
    }
    renderNet();
    renderDiagnostics();
    setSync(frame.offline ? 'radio off' : 'radio on');
    log(frame.offline ? 'WiFi radio switched off' : `WiFi radio switched on (${frame.net})`);
    return;
  }

  if (frame.cmd === 'impact') {
    log(`impact preview ${frame.mag} in mode ${frame.mode}`);
    return;
  }

  if (frame.cmd === 'reboot') {
    setSync('restarting');
    return;
  }

  if (frame.cmd === 'reload') {
    setSync('reloaded from flash');
  }
}

function handleEvent(frame) {
  if (frame.k === 'mode') {
    setModeFromId(Number(frame.id));
    if (app.state) {
      app.state.mode = Number(frame.id);
      app.state.mode_key = frame.key || 'unknown';
      app.state.mode_label = frame.label || 'Unknown';
    }
    renderModeActive(Number(frame.id));
    log(`mode -> ${frame.label || frame.key} (${frame.id})`);
    return;
  }

  if (frame.k === 'impact') {
    log(`impact ${frame.mag} (total ${frame.count})`);
    return;
  }

  if (frame.k === 'button') {
    log(`button ${frame.b || '?'}: ${frame.e || 'event'} #${frame.n}`);
  }
}

function handleTick(frame) {
  app.lastTickFrame = frame;

  setText('lv-accel', `${fmt(frame.ax)}  ${fmt(frame.ay)}  ${fmt(frame.az)}`);
  setText('lv-gyro', `${fmt(frame.gx)}  ${fmt(frame.gy)}  ${fmt(frame.gz)}`);
  setText('liveRate', `${frame.lp || 0} Hz loop`);

  if (frame.ic > 0) {
    const age = (Number(frame.ia) / 1000).toFixed(2);
    setText('lv-impact', `${frame.imp} · ${frame.ic} · ${age} s`);
  } else {
    setText('lv-impact', 'none yet');
  }

  setText('lv-loop', `${frame.lp} Hz`);
  setText('lv-rail', `${Math.round(Number(frame.rail) / 10) * 10} mV`);
  setText('lv-heap', `${Math.round(Number(frame.heap) / 1024)} KB`);

  const net = Number(frame.net);
  setText('lv-net', `${netName(net)}${frame.dirty ? ' · unsaved' : ''}`);

  const mask = Number(frame.btn) || 0;
  const pressed = [];
  if (mask & 1) pressed.push('touch down');
  if (mask & 2) pressed.push('boot down');
  setText('lv-buttons', pressed.length ? pressed.join(' · ') : 'both up');

  if (app.state && Number(frame.mode) !== Number(app.state.mode)) {
    setModeFromId(Number(frame.mode));
    renderState();
  }

  if (app.state) {
    const dirty = Boolean(frame.dirty);
    if (app.state.dirty !== dirty) {
      app.state.dirty = dirty;
      app.touched = dirty;
      renderDirty();
    }

    const portal = Boolean(frame.portal);
    if (Boolean(app.state.portal) !== portal) {
      app.state.portal = portal;
      renderNet();
      renderDiagnostics();
    }
  }

  const now = Number(frame.ms) || 0;
  if (now - app.diagAt > 500) {
    app.diagAt = now;
    renderDiagnostics();
  }
}

//* ---------------------- RENDERING ----------------------

function renderAll() {
  renderIdentity();
  renderModes();
  renderState();
  renderPresets();
  renderConnectionUI();
  renderDirty();
}

function renderIdentity() {
  const tag = $('#fwTag');
  if (app.pong) {
    tag.hidden = false;
    tag.textContent = `fw ${app.pong.fw}`;
    tag.title = `${app.pong.build} · ${app.pong.chip}`;
  } else {
    tag.hidden = true;
    tag.textContent = '';
  }
}

function renderModes() {
  const box = $('#modes');
  box.innerHTML = '';

  if (!app.modes.length) {
    const hint = document.createElement('span');
    hint.className = 'muted';
    hint.textContent = app.connected ? 'No mode list received.' : 'Connect to read the mode list.';
    box.append(hint);
    setText('modeNote', '');
    return;
  }

  for (const mode of app.modes) {
    const button = document.createElement('button');
    button.type = 'button';
    button.dataset.mode = String(mode.id);
    button.textContent = mode.label || mode.key;
    button.title = mode.key;
    button.addEventListener('click', () => {
      if (!app.connected) return;
      queueEdit('mode', Number(mode.id));
    });
    box.append(button);
  }

  renderModeActive(Number(app.state?.mode));
}

function renderModeActive(id) {
  for (const button of $$('#modes button')) {
    button.classList.toggle('on', Number(button.dataset.mode) === id);
  }

  if (!app.state) {
    setText('modeNote', '');
    return;
  }

  const label = app.state.mode_label || 'Unknown';
  const key = app.state.mode_key || '';
  const note = MODE_NOTES[key] || '';
  setText('modeNote', note ? `${label} — ${note}` : label);
  setText('lv-mode', `${label} (${app.state.mode})`);
}

function setModeFromId(id) {
  const numeric = Number(id);
  if (!Number.isFinite(numeric)) return;

  const mode = app.modes.find((entry) => Number(entry.id) === numeric);
  if (app.state) {
    app.state.mode = numeric;
    if (mode) {
      app.state.mode_key = mode.key;
      app.state.mode_label = mode.label;
    }
  }
  renderModeActive(numeric);
}

function renderState() {
  if (!app.state) return;

  for (const key of FIELD_KEYS) syncField(key);

  renderPalette();
  renderDerived();
  renderModeActive(Number(app.state.mode));
  renderNet();
  renderPresets();
  renderConnectionUI();
  renderDirty();
  renderDiagnostics();
  renderStress();
  renderSense();
}

function syncField(key) {
  const input = document.getElementById(`f-${key}`);
  if (!input || !app.state) return;

  const bounds = boundsFor(key, app.state);
  input.min = String(bounds.min);
  input.max = String(bounds.max);
  input.step = String(bounds.step);

  const range = input.closest('.field')?.querySelector('.range');
  if (range) {
    range.textContent = `${formatBound(bounds.min, bounds)}–${formatBound(bounds.max, bounds)}` +
      (bounds.unit ? ` ${bounds.unit}` : '');
  }

  // Never overwrite a field the user is typing into. A blur re-syncs it.
  if (document.activeElement === input) return;

  const value = app.pending[key] !== undefined ? app.pending[key] : app.state[key];
  if (value === undefined || value === null) return;
  input.value = formatForInput(value, bounds);
}

function renderPalette() {
  if (!app.state) return;

  const hex = rgbToHex(app.state.red, app.state.green, app.state.blue);
  setText('paletteHex', hex);

  const picker = $('#colorPicker');
  if (document.activeElement !== picker) picker.value = hex;

  const rgb = `${app.state.red},${app.state.green},${app.state.blue}`;
  for (const button of $$('#swatches button')) {
    button.classList.toggle('on', button.dataset.rgb === rgb);
  }
}

function renderDerived() {
  const state = app.state;
  if (!state) return;

  //* Worst-case white load at the current settings, after the budget's
  //? brightness ceiling. The "of" value is the configured budget, so the line
  //* reads as "what this would draw / what it is allowed to draw".
  const budget = Number(state.budget ?? state.budget_ma ?? 0);
  const leds = Number(state.active_leds) || 0;
  //* Mirror the firmware's worst-case ceiling so the line follows the budget
  //? immediately after the ACK, without waiting for the next full state.
  const budgetCap = (budget > 0 && leds > 0)
    ? Math.min(255, Math.floor(budget * 255 / (leds * 60)))
    : 255;
  const configured = Number(state.brightness) || 0;
  const guardScale = Number(state.guard_scale ?? 255);
  const safeActive = Boolean(state.safe_boot);
  const safePct = Number(state.safe_pct ?? 70);
  const capped = Math.min(configured, budgetCap);
  const guarded = Math.round(capped * guardScale / 255);
  const safe = safeActive ? Math.round(guarded * safePct / 100) : guarded;
  const maxLoad = Math.round(leds * 60 * safe / 255);
  setText('stripInfo',
    `${state.active_leds} of ${state.num_leds} LEDs lit · brightness ${state.brightness}/255 · ` +
    `Max load ${maxLoad} of ${budget} mA`);

  //* One explicit feedback line: no silent caps. The budget is checked first
  //? because it is the persistent limit the user chose; the guard and safe mode
  //* are transient layers above it.
  let notice;
  let warn = false;
  if (safeActive && safe < guarded) {
    notice = `Safe mode: brightness capped at ${safe}/255`;
    warn = true;
  } else if (state.guard_active && guarded < capped) {
    notice = `Rail guard trimming: effective ${guarded}/255`;
    warn = true;
  } else if (configured > budgetCap) {
    notice = `Load exceeds budget, brightness capped at ${budgetCap}/255`;
    warn = true;
  } else {
    notice = 'No constraints on this device';
  }
  const noticeEl = $('#budgetNotice');
  if (noticeEl) {
    noticeEl.textContent = notice;
    noticeEl.className = 'hint ' + (warn ? 'warn' : 'ok');
  }

  if (state.whip_travel_ms !== undefined) {
    setText('whipDerived',
      `One hump crosses the strip in ${state.whip_travel_ms} ms. ` +
      `${state.whip_in_flight} in flight now, up to ${state.whip_max_heads}.`);
  }

  if (state.rainbow_travel_ms !== undefined) {
    setText('rainbowDerived',
      `One colour crosses in ${state.rainbow_travel_ms} ms. ` +
      `A band is ${state.rainbow_band_min_leds}–${state.rainbow_band_max_leds} LEDs wide ` +
      `at this speed and wait range.`);
  }

  if (state.flame_power_soft !== undefined) {
    setText('flameDerived',
      `At the current scale the softest impact lights the strip at ` +
      `${state.flame_power_soft}/255 and the hardest at ${state.flame_power_hard}/255.`);
  }

  if (state.step_cadence_max_sps !== undefined) {
    setText('impactDerived',
      `The detector can sample up to ${state.step_cadence_max_sps}/s. ` +
      `Reported magnitude is rescaled to ${state.step_magnitude_min}–${state.step_magnitude_max}.`);
  }
}

function renderNet() {
  const state = app.state;
  if (!state) {
    setText('radioState', 'Connect to read the radio state.');
    setText('lv-net', '—');
    return;
  }

  const offline = Boolean(state.offline_mode);
  const radio = $('#radioOn');
  radio.checked = !offline;
  radio.disabled = !app.connected;

  const bits = [state.net || 'unknown'];
  if (state.rssi !== undefined && state.rssi !== 0) bits.push(`${state.rssi} dBm`);
  if (state.portal) bits.push('config hotspot open');
  else if (offline) bits.push('USB still works');
  setText('radioState', bits.join(' · '));

  radio.disabled = !app.connected || Boolean(state.portal);

  setText('lv-net', `${state.net || 'unknown'}${state.rssi ? ` · ${state.rssi} dBm` : ''}`);
}

function renderPresets() {
  const box = $('#presetSlots');
  box.innerHTML = '';

  const ready = app.connected && Boolean(app.state);
  const slots = app.presets.length
    ? app.presets
    : Array.from({ length: 5 }, (_, slot) => ({ slot, used: false, state: 'empty', name: '' }));

  for (const preset of slots) {
    const card = document.createElement('div');
    card.className = 'preset-card';

    const index = document.createElement('div');
    index.className = 'preset-slot';
    index.textContent = `S${String(Number(preset.slot) + 1).padStart(2, '0')}`;

    const name = document.createElement('div');
    name.className = 'preset-name';
    if (preset.used) {
      name.textContent = preset.name || 'Unnamed';
      const small = document.createElement('small');
      small.textContent = 'saved';
      name.append(small);
    } else if (preset.state === 'incompatible') {
      name.textContent = 'Old firmware format';
      const small = document.createElement('small');
      small.textContent = 'saving over it is allowed';
      name.append(small);
    } else {
      name.textContent = 'Empty slot';
      const small = document.createElement('small');
      small.textContent = 'save the live configuration here';
      name.append(small);
    }

    const actions = document.createElement('div');
    actions.className = 'preset-actions';

    const apply = document.createElement('button');
    apply.type = 'button';
    apply.className = 'btn small';
    apply.textContent = 'Apply';
    apply.disabled = !ready || !preset.used;
    apply.addEventListener('click', () => applyPreset(Number(preset.slot)));

    const save = document.createElement('button');
    save.type = 'button';
    save.className = 'btn small';
    save.textContent = 'Save';
    save.disabled = !ready;
    save.addEventListener('click', () => savePreset(Number(preset.slot), preset));

    const clear = document.createElement('button');
    clear.type = 'button';
    clear.className = 'btn small danger';
    clear.textContent = 'Clear';
    clear.disabled = !ready || preset.state === 'empty';
    clear.addEventListener('click', () => clearPreset(Number(preset.slot)));

    actions.append(apply, save, clear);
    card.append(index, name, actions);
    box.append(card);
  }

  const used = slots.filter((preset) => preset.used).length;
  setText('presetInfo', `${used} of ${slots.length} slots used.`);
  $('#btnPresetExport').disabled = !ready || used === 0;
}

function renderConnectionUI() {
  const ready = app.demo || (app.connected && Boolean(app.state));

  const connectButton = $('#btnConnect');
  connectButton.textContent = app.connected ? 'Disconnect' : 'Connect';
  connectButton.classList.toggle('solid', !app.connected);
  connectButton.classList.toggle('ghost', app.connected);
  connectButton.disabled = app.connecting || app.updateBusy || app.recoveryBusy;

  if (app.demo) {
    connectButton.textContent = 'Demo';
    connectButton.disabled = true;
  }

  $('#btnReconnect').hidden = app.connected || app.connecting || !app.port || app.demo || app.updateBusy || app.recoveryBusy;

  $('#btnSave').disabled = !ready;
  $('#btnRevert').disabled = !ready;
  $('#btnImpact').disabled = !ready;
  $('#btnReboot').disabled = !ready;
  $('#radioOn').disabled = !ready;
  $('#streamOn').disabled = !app.connected;
  $('#streamHz').disabled = !app.connected;

  for (const input of $$('#page-shared input[type="number"], #page-flame input[type="number"], ' +
    '#page-impact input[type="number"], #page-whip input[type="number"], #page-rainbow input[type="number"]')) {
    input.disabled = !ready;
  }
  $('#colorPicker').disabled = !ready;
  for (const button of $$('#swatches button')) button.disabled = !ready;
  for (const button of $$('#modes button')) button.disabled = !ready;

  //* The stress test needs a real board, a state document and the radio off. The
  //? state field is what the firmware itself enforces; disabling here is the UI.
  const stressReady = !app.demo && app.connected && Boolean(app.state) && Boolean(app.state.offline_mode);
  $('#btnStressRun').disabled = !stressReady;
  for (const id of ['#stressR', '#stressG', '#stressB', '#stressMa', '#stressFast', '#stressNoRail']) {
    const element = $(id);
    if (element) element.disabled = !stressReady;
  }
  $('#btnSenseRun').disabled = !stressReady;
  for (const id of ['#senseCount', '#senseBurst']) {
    const element = $(id);
    if (element) element.disabled = !stressReady;
  }

  const fileButton = document.querySelector('label.filebtn');
  if (fileButton) fileButton.classList.toggle('disabled', !ready);

  if (!app.connected) {
    $('#btnConnect').disabled = app.connecting || app.updateBusy || app.recoveryBusy;
  }

  renderUpdatePanel();
}

//* ---------------------- USB UPDATER ----------------------
//? The update rides the same `@` link: BEGIN <size>, DATA <offset> <base64>,
//? END. The firmware writes the app to the inactive app slot through Update.h;
//? the running image and NVS are untouched until END marks the new slot
//? bootable. No ROM bootloader, no port close/reset, no esptool-js dependency.
//!
//? The host verifies the manifest's per-part SHA-256 before the first chunk, so
//* a bad download is rejected before the board writes anything.

async function checkLatestBuild() {
  setText('updateInfo', 'Checking…');
  const progress = $('#updateProgress');
  if (progress) progress.hidden = true;

  try {
    const response = await fetch(UPDATE_MANIFEST_URL, { cache: 'no-store' });
    if (!response.ok) throw new Error(`manifest -> HTTP ${response.status}`);
    const manifest = await response.json();
    validateUpdateManifest(manifest);

    app.updateManifest = manifest;
    const size = Number(manifest.size) || 0;
    const when = manifest.built ? new Date(manifest.built) : null;
    const stamp = when && !Number.isNaN(when.getTime()) ? when.toLocaleString() : 'unknown date';
    setText('updateInfo',
      `${manifest.version} · ${formatBytes(size)} · ${stamp}` +
      (manifest.source ? ` · ${manifest.source}` : ''));
    log(`latest build: ${manifest.version} (${manifest.source || 'source unknown'})`);
    renderUpdatePanel();
  } catch (error) {
    app.updateManifest = null;
    renderUpdatePanel();
    setText('updateInfo', `Could not read the latest build: ${error.message}`);
    showError(`Could not read the latest build manifest: ${error.message}`);
  }
}

function validateUpdateManifest(manifest) {
  if (!manifest || manifest.name !== 'lightpack-C3') {
    throw new Error('the manifest is not for lightpack-C3');
  }

  const build = Array.isArray(manifest.builds) ? manifest.builds[0] : null;
  if (!build || build.chipFamily !== 'ESP32-C3') {
    throw new Error('the manifest is not for an ESP32-C3');
  }

  if (!Array.isArray(build.parts) || build.parts.length === 0) {
    throw new Error('the manifest has no image parts');
  }

  for (const part of build.parts) {
    if (typeof part.path !== 'string' || part.path.length === 0) {
      throw new Error('the manifest has a part without a path');
    }
    if (part.path.includes('..') || /^[a-z][a-z0-9+.-]*:/i.test(part.path)) {
      throw new Error('the manifest points outside this site');
    }
    if (!Number.isFinite(Number(part.offset)) || Number(part.offset) < 0) {
      throw new Error('the manifest has an invalid offset');
    }
    if (part.sha256 !== undefined && !/^[0-9a-f]{64}$/i.test(String(part.sha256))) {
      throw new Error('the manifest has an invalid SHA-256');
    }
  }
}

async function installLatest() {
  if (app.demo) {
    showError('Demo mode: there is no board to update.');
    return;
  }
  if (!app.updateManifest) {
    showError('Check for a latest build first.');
    return;
  }
  if (!app.connected || !app.link) {
    showError('Connect to the board first; the update uses the USB control link.');
    return;
  }
  if (app.updateBusy) return;

  app.updateBusy = true;
  renderUpdatePanel();
  setUpdateProgress('Preparing…', 0);

  try {
    const manifest = app.updateManifest;
    const build = manifest.builds[0];
    // The USB updater writes the app to the inactive app slot; boot_app0 belongs to
    // the ROM-bootloader path and is not sent over this link.
    const appPart = build.parts.find((part) => Number(part.offset) === 65536) ||
      build.parts[build.parts.length - 1];
    const source = manifest.source || manifest.version || String(Date.now());
    const base = new URL(UPDATE_MANIFEST_URL, window.location.href);
    const url = new URL(appPart.path, base);
    url.searchParams.set('v', source);

    setUpdateProgress('Downloading firmware…', 0);
    const response = await fetch(url, { cache: 'no-store' });
    if (!response.ok) throw new Error(`download ${appPart.path} -> HTTP ${response.status}`);
    const bytes = new Uint8Array(await response.arrayBuffer());
    if (appPart.sha256 && (await sha256Hex(bytes)) !== String(appPart.sha256).toLowerCase()) {
      throw new Error(`checksum mismatch for ${appPart.path}`);
    }
    const total = bytes.length;

    const ready = await request(commands.updateBegin(total), isUpdateAck, 5000);
    if (!ready.ok || ready.state !== 'ready') {
      throw new Error(ready.err || 'the board refused the update');
    }

    const CHUNK = 2048;
    let sent = 0;
    while (sent < total) {
      const end = Math.min(sent + CHUNK, total);
      const base64 = base64Bytes(bytes.subarray(sent, end));
      let ack = null;

      for (let attempt = 0; attempt < 3; attempt++) {
        try {
          ack = await request(commands.updateData(sent, base64), isUpdateAck, 8000);
          if (!ack.ok) throw new Error(ack.err || 'the board rejected a chunk');
          break;
        } catch (error) {
          if (attempt === 2) throw error;
          await sleep(250);
        }
      }

      const received = Number(ack?.received);
      if (!Number.isFinite(received) || received <= sent) {
        throw new Error('the board did not confirm the chunk');
      }
      sent = Math.min(received, total);
      setUpdateProgress('Writing application…', (sent / total) * 100);
    }

    let done;
    app.expectReboot = true;
    try {
      done = await request(commands.updateEnd(), isUpdateAck, 10000);
    } catch (error) {
      // The C3 can restart before the final ack reaches the host. Only a lost
      // answer is success-ish; a board rejection must still fail.
      if (!/did not answer/i.test(String(error?.message || error))) {
        app.expectReboot = false;
        throw error;
      }
      done = null;
    }
    if (done && (!done.ok || done.state !== 'complete')) {
      app.expectReboot = false;
      throw new Error(done.err || 'the board rejected the image');
    }

    setUpdateProgress('Update complete — board restarting…', 100);
    app.manualDisconnect = false;
    app.reconnectAttempts = 0;
    setStatus('connecting', 'restarting');
    setSync('restarting');
  } catch (error) {
    setUpdateProgress(`Update failed: ${error.message}`, null);
    showError(`Firmware update failed: ${error.message}`);
    try {
      if (app.link) app.link.send(commands.updateAbort());
    } catch {}
  } finally {
    app.updateBusy = false;
    renderUpdatePanel();
  }
}

//* Recovery writes boot_app0 + app through the C3 ROM via the vendored
//? esptool-js. It does not use the app-level @UPDATE link, so it works on a board
//* whose installed firmware predates that command.
function binaryString(bytes) {
  let out = '';
  const CHUNK = 0x8000;
  for (let i = 0; i < bytes.length; i += CHUNK) {
    out += String.fromCharCode.apply(null, bytes.subarray(i, i + CHUNK));
  }
  return out;
}

async function runRecovery() {
  if (app.demo) {
    showError('Demo mode: there is no board to recover.');
    return;
  }
  if (!('serial' in navigator)) {
    showError('Recovery needs Chrome or Edge over HTTPS or localhost.');
    return;
  }
  if (app.updateBusy || app.recoveryBusy) return;

  if (!app.updateManifest) {
    await checkLatestBuild();
    if (!app.updateManifest) return;
  }

  const parts = (app.updateManifest.builds?.[0]?.parts) || [];
  const bootPart = parts.find((part) => Number(part.offset) === 0xE000);
  const appPart = parts.find((part) => Number(part.offset) === 0x10000);
  if (!bootPart || !appPart) {
    showError('The release has no boot_app0/app pair.');
    return;
  }

  const ok = confirm(
    'Recovery writes the firmware directly through the board\'s ROM bootloader.\n\n' +
    '1. Hold GPIO9 (BOOT).\n' +
    '2. Plug the board in or press RESET.\n' +
    '3. Release GPIO9 when the port appears.\n' +
    '4. Select the port in the browser picker.\n\n' +
    'Your settings, presets and networks are preserved.'
  );
  if (!ok) return;

  app.recoveryBusy = true;
  renderUpdatePanel();
  setUpdateProgress('Preparing recovery…', 0);

  let transport = null;
  try {
    const base = new URL(UPDATE_MANIFEST_URL, window.location.href);
    const version = app.updateManifest.source || app.updateManifest.version || String(Date.now());

    async function fetchImage(part) {
      const url = new URL(part.path, base);
      url.searchParams.set('v', version);
      const response = await fetch(url, { cache: 'no-store' });
      if (!response.ok) throw new Error(`download ${part.path} -> HTTP ${response.status}`);
      const bytes = new Uint8Array(await response.arrayBuffer());
      if (part.sha256 && (await sha256Hex(bytes)) !== String(part.sha256).toLowerCase()) {
        throw new Error(`checksum mismatch for ${part.path}`);
      }
      return bytes;
    }

    setUpdateProgress('Downloading recovery images…', 0);
    const boot = await fetchImage(bootPart);
    const appImage = await fetchImage(appPart);

    //* The console's normal link owns the same Web Serial port. Recovery needs
    //? the port exclusively, so drop the app-level connection before the picker.
    if (app.link) {
      try {
        await app.link.close();
      } catch {}
    }
    app.connected = false;
    app.port = null;
    renderConnectionUI();

    const port = await navigator.serial.requestPort();
    transport = new Transport(port, true);
    const loader = new ESPLoader({
      transport,
      baudrate: 460800,
      terminal: {
        clean() {},
        writeLine(data) { log(String(data)); },
        write(data) { log(String(data)); },
      },
    });

    setUpdateProgress('Connecting to the ROM bootloader…', 0);
    await loader.main();
    await loader.writeFlash({
      fileArray: [
        { data: binaryString(boot), address: 0xE000 },
        { data: binaryString(appImage), address: 0x10000 },
      ],
      flashSize: 'keep',
      flashMode: 'keep',
      flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const pct = total > 0 ? (written / total) * 100 : 0;
        setUpdateProgress(fileIndex === 0 ? 'Writing boot selector…' : 'Writing application…', pct);
      },
    });
    setUpdateProgress('Recovery complete — board restarting…', 100);
  } catch (error) {
    setUpdateProgress(`Recovery failed: ${error.message}`, null);
    showError(`Recovery failed: ${error.message}`);
  } finally {
    if (transport) {
      try {
        await transport.disconnect();
      } catch {}
    }
    app.recoveryBusy = false;
    renderUpdatePanel();
  }
}

function isUpdateAck(frame) {
  return frame.t === 'ack' && frame.cmd === 'update';
}

function base64Bytes(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
  return btoa(binary);
}

async function sha256Hex(bytes) {
  if (!globalThis.crypto?.subtle) throw new Error('SHA-256 is unavailable in this context');
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, '0')).join('');
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function setUpdateProgress(status, percent) {
  const box = $('#updateProgress');
  if (box) box.hidden = false;
  setText('updateStatus', status);

  const numeric = Number(percent);
  const value = Number.isFinite(numeric) ? Math.max(0, Math.min(100, numeric)) : null;
  setText('updatePercent', value === null ? '' : `${Math.round(value)}%`);

  const bar = $('#updateBar');
  if (bar) bar.style.width = `${value === null ? 0 : value}%`;
}

function renderUpdatePanel() {
  const current = app.pong ? `current ${app.pong.fw}` : 'current unknown';
  setText('updateCurrent', current);

  const busy = app.updateBusy || app.recoveryBusy;
  const check = $('#btnCheckUpdate');
  if (check) check.disabled = busy;

  const disconnect = $('#btnUpdateDisconnect');
  if (disconnect) disconnect.disabled = !app.connected || busy;

  const install = $('#btnInstallUpdate');
  if (install) {
    install.disabled = !app.updateManifest || app.demo || busy || app.connecting || !app.connected;
    install.title = app.demo
      ? 'Demo mode: there is no board to update'
      : !app.connected
        ? 'Connect to the board first'
        : !app.updateManifest
          ? 'Check for a build first'
          : busy
            ? 'An update is already running'
            : 'Writes the latest build over the USB link';
  }

  const recovery = $('#btnRecovery');
  if (recovery) {
    const serialSupported = 'serial' in navigator;
    recovery.disabled = app.demo || !app.updateManifest || busy || app.connecting || !serialSupported;
    recovery.title = app.demo
      ? 'Demo mode: there is no board to recover'
      : !serialSupported
        ? 'Recovery needs Chrome or Edge over HTTPS or localhost'
        : !app.updateManifest
          ? 'Check for a build first'
          : busy
            ? 'An update is already running'
            : 'Hold GPIO9 at boot, then write through the ROM bootloader';
  }

  const hint = $('#updateHint');
  if (hint) {
    if (app.recoveryBusy) hint.textContent = 'Recovery running — keep the board plugged in.';
    else if (busy) hint.textContent = 'Updating — keep the board plugged in.';
    else if (app.demo) hint.textContent = 'Demo mode: no board to update.';
    else if (!app.updateManifest) hint.textContent = 'Check for a build first.';
    else if (!app.connected) hint.textContent = 'Connect to the board first.';
    else hint.textContent = 'Ready. The strip is held off during the transfer; NVS is preserved.';
  }
}

function formatBytes(bytes) {
  const number = Number(bytes);
  if (!Number.isFinite(number) || number <= 0) return 'size unknown';
  if (number >= 1024 * 1024) return `${(number / (1024 * 1024)).toFixed(2)} MB`;
  return `${Math.round(number / 1024)} KB`;
}

function renderDirty() {
  const dirty = Boolean(app.state?.dirty || app.touched || Object.keys(app.pending).length);
  $('#dirtyFlag').hidden = !dirty;
}

function renderDiagnostics() {
  const state = app.state;
  const pong = app.pong;
  const tick = app.lastTickFrame;

  const port = app.port
    ? (() => {
        const info = app.port.getInfo ? app.port.getInfo() : {};
        const vid = info.usbVendorId !== undefined ? info.usbVendorId.toString(16).padStart(4, '0') : '----';
        const pid = info.usbProductId !== undefined ? info.usbProductId.toString(16).padStart(4, '0') : '----';
        return `${vid}:${pid}`;
      })()
    : '—';

  setText('diag-fw', pong ? pong.fw : '—');
  setText('diag-build', pong ? pong.build : '—');
  setText('diag-chip', pong ? `${pong.chip} @ ${pong.cpu_mhz} MHz` : '—');
  setText('diag-port', port);
  setText('diag-proto', pong ? String(pong.proto) : '1');
  setText('diag-uptime', tick ? formatUptime(tick.ms) : state ? formatUptime(state.uptime_ms) : '—');
  setText('diag-mode', state ? `${state.mode_label || 'Unknown'} (${state.mode})` : '—');
  setText('diag-loop', tick ? `${tick.lp} Hz`
    : state && state.loop_rate ? `${state.loop_rate} Hz` : '—');
  setText('diag-rail', tick ? `${tick.rail} mV`
    : state && state.rail_mv ? `${state.rail_mv} mV` : '—');
  setText('diag-heap', tick ? `${Math.round(Number(tick.heap) / 1024)} KB`
    : state && state.free_heap ? `${Math.round(Number(state.free_heap) / 1024)} KB` : '—');
  setText('diag-net', state ? `${state.net}${state.portal ? ' (config hotspot)' : ''}${state.offline_mode ? ' (offline mode)' : ''}` : '—');
  setText('diag-rssi', state && state.rssi ? `${state.rssi} dBm` : '—');
  setText('diag-ip', state ? state.ip : '—');
  setText('diag-host', state ? state.hostname : '—');
  setText('diag-ssid', state ? (state.wifi_ssid || 'nothing saved') : '—');
  setText('diag-save', state ? (state.dirty ? 'RAM differs from flash' : 'RAM matches flash') : '—');
}

//* ---------------------- POWER STRESS TEST ----------------------
//? The four colour buttons start the firmware's blocking ramp. The result is a
//? state field, not a one-shot reply: the board is reset by the reconnect itself,
//? so the page must be able to render the last result from NVS on every connect.
const STRESS_RESULT_NAMES = {
  0: 'no test recorded',
  1: 'brown-out',
  2: 'interrupted',
  3: 'hit the selected ceiling',
  4: 'held full white',
  5: 'rail collapsed',
  6: 'aborted',
};
const STRESS_PHASE_NAMES = ['before the ramp', 'probe', 'ramp'];

//* The firmware's stress colour is a 3-bit mask: R=1, G=2, B=4.
function colorMaskName(mask) {
  const value = Number(mask) || 0;
  const parts = [];
  if (value & 1) parts.push('R');
  if (value & 2) parts.push('G');
  if (value & 4) parts.push('B');
  return parts.length ? parts.join('+') : '—';
}

function renderStress() {
  const state = app.state;

  const result = state ? Number(state.stress_result) || 0 : 0;
  const rows = [];

  if (!state || result === 0) {
    rows.push(['Result', 'no test recorded']);
  } else {
    const seconds = Number(state.stress_elapsed_ms || 0) / 1000;
    const mcuMa = Number(state.mcu_ma) || 200;
    rows.push(['Result', STRESS_RESULT_NAMES[result] || `code ${result}`]);
    rows.push(['Stopped during', STRESS_PHASE_NAMES[Number(state.stress_phase)] || '—']);
    rows.push(['Test ceiling', `${state.stress_ceiling_ma ?? '—'} mA${Number(state.stress_ceiling_ma) > 2000 ? ' (lab)' : ''}`]);
    rows.push(['Step time', `${state.stress_step_ms ?? '—'} ms`]);
    rows.push(['Rail guard', Number(state.stress_rail_floor_mv) === 0
      ? 'disabled (lab)'
      : `${state.stress_rail_floor_mv ?? '—'} mV`]);
    rows.push(['Channels', colorMaskName(state.stress_color)]);
    rows.push(['LEDs', String(state.stress_leds ?? '—')]);
    rows.push(['Brightness at end', `${state.stress_brightness ?? '—'}/255`]);
    rows.push(['Estimated strip current', `${state.stress_est_ma ?? '—'} mA`]);
    rows.push(['Total with MCU reserve', `${(Number(state.stress_est_ma) || 0) + mcuMa} mA`]);
    rows.push(['V_REG at start', `${state.stress_v_start ?? '—'} mV`]);
    rows.push(['V_REG min (last step)', `${state.stress_v_min ?? '—'} mV (sag ${state.stress_sag_mv ?? '—'} mV)`]);
    rows.push(['Probe', `blank ${state.stress_v_off ?? '—'} mV / lit ${state.stress_v_on ?? '—'} mV`]);
    rows.push(['Source resistance', `${state.stress_r_mohm ?? '—'} mΩ`]);
    rows.push(['Ramp duration', `${seconds.toFixed(1)} s`]);
  }

  if (state && state.stress_run) {
    rows.push(['State', 'a test is marked as running; wait for it to finish, then reconnect']);
  }

  const list = $('#stressResult');
  list.innerHTML = rows.map(([label, value]) => `<dt>${label}</dt><dd>${value}</dd>`).join('');

  setText('stressState', app.stressRunning
    ? 'test running'
    : (state && state.safe_boot ? `safe mode ${state.safe_pct}%` : ''));

  let hint = 'Connect to the board with the radio off to run a test.';
  if (app.demo) {
    hint = 'Demo data — connect a board to run a real test.';
  } else if (app.connected && state) {
    if (!state.offline_mode) {
      hint = 'Switch the radio off above first: a TX burst would skew the ramp.';
    } else if (state.stress_run) {
      hint = 'A test is marked as running. Let it finish, then reconnect to read the result.';
    } else if (state.safe_boot) {
      hint = 'Safe mode is active. Clear it once the supply is understood.';
    } else {
      hint = 'Ready. The board blanks the strip for 5 s, then ramps.';
    }
  }
  setText('stressHint', hint);

  $('#btnSafeOff').disabled = app.demo || !app.connected || !state || !state.safe_boot;
}

async function runStressTest() {
  if (app.demo || !app.connected || !app.state) return;
  if (!app.state.offline_mode) {
    showError('Switch the radio off before a stress test.');
    return;
  }

  const mask = ($('#stressR')?.checked ? 1 : 0) |
    ($('#stressG')?.checked ? 2 : 0) |
    ($('#stressB')?.checked ? 4 : 0);
  if (!mask) {
    showError('Select at least one channel (R, G or B).');
    return;
  }

  let ceiling = Number($('#stressMa')?.value);
  if (!Number.isFinite(ceiling)) ceiling = 2000;
  ceiling = Math.min(6000, Math.max(500, Math.round(ceiling)));
  $('#stressMa').value = String(ceiling);

  const step = $('#stressFast')?.checked ? 500 : 2000;
  const noRail = Boolean($('#stressNoRail')?.checked);
  const railFloor = noRail ? 0 : 3000;
  const lab = ceiling > 2000;
  const channels = colorMaskName(mask);

  const ok = confirm(
    `Run ${channels} at ${ceiling} mA, ${step} ms steps?\n\n` +
    (lab ? 'LAB ONLY: above the 2 A continuous benchmark. This can force a brown-out.\n\n' : '') +
    (noRail ? 'LAB ONLY: the 3.0 V rail guard is DISABLED. The strip rail may collapse far below 3 V.\n\n' : '') +
    'The strip blanks for 5 seconds, then ramps. Unplug USB during the blank to test ' +
    'the battery, then reconnect to read the result.'
  );
  if (!ok) return;

  try {
    await request(commands.stress(mask, ceiling, step, railFloor),
      (frame) => frame.t === 'ack' && frame.cmd === 'stress', 2500);
    app.stressRunning = true;
    renderStress();
    setText('stressHint', `Started: ${channels}, ${ceiling} mA, ${step} ms, ` +
      `${noRail ? 'no rail guard' : '3.0 V rail guard'}. Unplug USB now if you are testing the ` +
      'battery; reconnect afterwards to read the result.');
  } catch (error) {
    showError(`Could not start the stress test: ${error.message}`);
  }
}

async function clearSafeMode() {
  if (app.demo || !app.connected) return;

  try {
    await request(commands.safeOff(), (frame) => frame.t === 'ack' && frame.cmd === 'safe', 2000);
    if (app.state) app.state.safe_boot = false;
    renderStress();
    setText('stressHint', 'Safe mode cleared: the configured brightness is back in force.');
  } catch (error) {
    showError(`Could not clear safe mode: ${error.message}`);
  }
}

//* ---------------------- CHANNEL RESISTANCE ----------------------
//? A single short burst, no ramp: 1..100 white LEDs, 200 ms or 2 s, after the
//? same 5 s cable-removal window. The result is a state field, so a battery run
//? survives the reconnect like the stress result.
function renderSense() {
  const state = app.state;
  const rows = [];

  if (!state || !state.sense_count) {
    rows.push(['Result', 'no sense run recorded']);
  } else {
    const vOff = Number(state.sense_v_off) || 0;
    const vOn = Number(state.sense_v_on) || 0;
    rows.push(['LEDs', String(state.sense_count)]);
    rows.push(['Burst', `${state.sense_burst_ms ?? '—'} ms`]);
    rows.push(['Model current', `${state.sense_ma ?? '—'} mA`]);
    rows.push(['V_REG blank', `${vOff} mV`]);
    rows.push(['V_REG lit', `${vOn} mV`]);
    rows.push(['Drop', `${vOff - vOn} mV`]);
    rows.push(['Source R', `${state.sense_r_mohm ?? '—'} mΩ`]);
  }

  const list = $('#senseResult');
  list.innerHTML = rows.map(([label, value]) => `<dt>${label}</dt><dd>${value}</dd>`).join('');

  setText('senseState', app.senseRunning ? 'sense running' : '');
  $('#btnSenseRun').disabled = app.demo || !app.connected || !state || !state.offline_mode;
}

async function runSense() {
  if (app.demo || !app.connected || !app.state) return;
  if (!app.state.offline_mode) {
    showError('Switch the radio off before a sense run.');
    return;
  }

  let count = Number($('#senseCount')?.value);
  if (!Number.isFinite(count)) count = 20;
  count = Math.min(100, Math.max(1, Math.round(count)));
  $('#senseCount').value = String(count);

  const burst = Number($('#senseBurst')?.value) === 2000 ? 2000 : 200;
  const ma = count * 60;
  const lab = burst === 2000 && ma >= 3600;

  const ok = confirm(
    `Sense ${count} white LEDs at full brightness, ${burst} ms burst?\n\n` +
    (lab ? `LAB ONLY: ${ma} mA for 2 s is at/above the PMOS 3.6 A continuous rating.\n\n` : '') +
    'The strip blanks for 5 s: unplug USB there to test the battery. ' +
    'Reconnect afterwards to read the V drop and source resistance.'
  );
  if (!ok) return;

  try {
    await request(commands.sense(count, burst),
      (frame) => frame.t === 'ack' && frame.cmd === 'sense', 2500);
    app.senseRunning = true;
    renderSense();
    setText('senseState', 'sense running');
  } catch (error) {
    showError(`Could not start the sense run: ${error.message}`);
  }
}

function clearLive() {
  for (const id of ['lv-mode', 'lv-accel', 'lv-gyro', 'lv-impact', 'lv-loop', 'lv-rail', 'lv-heap', 'lv-net', 'lv-buttons', 'liveRate']) {
    setText(id, '—');
  }
  if (app.state) setText('lv-mode', `${app.state.mode_label || '—'} (${app.state.mode})`);
}

//* ---------------------- EDITS AND SAVING ----------------------

function queueEdit(key, value) {
  if (app.demo || !app.connected) return;

  const bounds = boundsFor(key, app.state);
  const number = Number(value);
  if (!Number.isFinite(number)) return;

  const clamped = Math.min(bounds.max, Math.max(bounds.min, number));
  app.pending[key] = clamped;
  app.touched = true;
  renderDirty();

  clearTimeout(app.flushTimer);
  app.flushTimer = setTimeout(flushEdits, EDIT_DEBOUNCE_MS);
}

function queuePalette(red, green, blue) {
  if (app.demo || !app.connected) return;

  app.pending.red = Math.min(255, Math.max(0, Math.round(red)));
  app.pending.green = Math.min(255, Math.max(0, Math.round(green)));
  app.pending.blue = Math.min(255, Math.max(0, Math.round(blue)));
  app.touched = true;
  renderDirty();

  clearTimeout(app.flushTimer);
  app.flushTimer = setTimeout(flushEdits, EDIT_DEBOUNCE_MS);
}

function flushEdits() {
  clearTimeout(app.flushTimer);
  app.flushTimer = 0;

  if (!app.connected) return;

  const values = app.pending;
  app.pending = {};
  const keys = Object.keys(values);
  if (keys.length === 0) return;

  try {
    const line = buildSet(values, ++app.seq);
    send(line).catch((error) => showError(`Could not send: ${error.message}`));
    if (app.autoCommit && !app.importing) scheduleCommit();
  } catch (error) {
    // Put the batch back so a rejected line is not silently lost.
    app.pending = { ...values, ...app.pending };
    showError(error.message);
  }
}

function scheduleCommit() {
  if (!app.autoCommit) return;
  clearTimeout(app.commitTimer);
  app.commitTimer = setTimeout(() => {
    commit().catch(() => {});
  }, COMMIT_DEBOUNCE_MS);
}

async function commit() {
  if (!app.connected) return;
  clearTimeout(app.commitTimer);
  app.commitTimer = 0;

  if (Object.keys(app.pending).length > 0) flushEdits();

  try {
    await request(commands.commit(), (frame) => frame.t === 'ack' && frame.cmd === 'commit', 2500);
    if (app.state) app.state.dirty = false;
    app.touched = false;
    renderDirty();
    setSync('saved to flash');
  } catch (error) {
    showError(`Could not save to flash: ${error.message}`);
  }
}

async function revert() {
  if (!app.connected) return;
  if (!confirm('Discard unsaved changes and reload the configuration from flash?')) return;

  clearTimeout(app.flushTimer);
  clearTimeout(app.commitTimer);
  app.pending = {};
  app.touched = false;

  try {
    await request(commands.reload(), (frame) => frame.t === 'state', 3000);
    app.touched = false;
    renderDirty();
    setSync('reloaded from flash');
  } catch (error) {
    showError(`Could not reload: ${error.message}`);
  }
}

async function applyStream() {
  if (!app.connected) return;

  const wanted = $('#streamOn').checked;
  const hz = wanted ? Number($('#streamHz').value) || 10 : 0;

  try {
    await request(commands.stream(hz), (frame) => frame.t === 'ack' && frame.cmd === 'stream', 1800);
    app.streamHz = hz || app.streamHz;
    if (!wanted) clearLive();
    setSync(wanted ? `live at ${hz} Hz` : 'live off');
  } catch (error) {
    $('#streamOn').checked = false;
    showError(`Could not set the live rate: ${error.message}`);
  }
}

async function previewImpact() {
  if (!app.connected) return;

  const magnitude = Number($('#impactMag').value);
  if (!Number.isFinite(magnitude)) return;

  try {
    const ack = await request(
      commands.impact(magnitude),
      (frame) => frame.t === 'ack' && frame.cmd === 'impact',
      1800
    );
    setText('impactNote', `sent ${ack.mag} · mode ${ack.mode}`);
    if (ack.mode !== 2) {
      setText('impactNote', `sent ${ack.mag}; select Step flame to see it`);
    }
  } catch (error) {
    setText('impactNote', error.message);
  }
}

async function applyRadio() {
  if (!app.connected) return;

  const wanted = $('#radioOn').checked;
  try {
    await request(
      commands.radio(wanted ? 'on' : 'off'),
      (frame) => frame.t === 'ack' && frame.cmd === 'radio',
      4000
    );
  } catch (error) {
    showError(`Could not switch the radio: ${error.message}`);
    if (app.state) app.state.offline_mode = !wanted;
    renderNet();
  }
}

//* ---------------------- PRESETS ----------------------

async function applyPreset(slot) {
  if (!app.connected) return;
  setText('presetInfo', `Applying slot ${slot + 1}…`);

  try {
    await request(
      commands.presetApply(slot),
      (frame) => frame.t === 'state' ||
        (frame.t === 'ack' && frame.cmd === 'preset' && frame.ok === false),
      3000
    );
    app.touched = false;
    renderState();
    setText('presetInfo', `Applied slot ${slot + 1}.`);
    log(`preset ${slot + 1} applied`);
  } catch (error) {
    setText('presetInfo', `Could not apply: ${error.message}`);
  }
}

async function savePreset(slot, existing) {
  if (!app.connected) return;

  const fallback = existing?.used ? existing.name : `Preset ${slot + 1}`;
  const name = prompt(`Name for slot ${slot + 1}:`, fallback);
  if (name === null) return;

  try {
    await request(
      commands.presetSave(slot, name),
      (frame) => frame.t === 'presets' ||
        (frame.t === 'ack' && frame.cmd === 'preset' && frame.ok === false),
      2500
    );
    setText('presetInfo', `Saved slot ${slot + 1}${name ? ` as “${name}”` : ''}.`);
    log(`preset ${slot + 1} saved${name ? ` as ${name}` : ''}`);
  } catch (error) {
    setText('presetInfo', `Could not save: ${error.message}`);
  }
}

async function clearPreset(slot) {
  if (!app.connected) return;
  if (!confirm(`Clear slot ${slot + 1}?`)) return;

  try {
    await request(
      commands.presetClear(slot),
      (frame) => frame.t === 'presets' ||
        (frame.t === 'ack' && frame.cmd === 'preset' && frame.ok === false),
      2000
    );
    setText('presetInfo', `Cleared slot ${slot + 1}.`);
  } catch (error) {
    setText('presetInfo', `Could not clear: ${error.message}`);
  }
}

async function exportPresets() {
  if (!app.connected) return;

  const used = app.presets.filter((preset) => preset.used);
  if (used.length === 0) {
    setText('presetInfo', 'Nothing to export: no slot is saved.');
    return;
  }

  setText('presetInfo', `Reading ${used.length} slot(s)…`);
  const docs = [];

  try {
    for (const preset of used) {
      const frame = await request(
        commands.presetGet(preset.slot),
        (reply) => reply.t === 'preset' && Number(reply.preset?.slot) === Number(preset.slot),
        2000
      );
      docs.push(frame.preset);
    }

    const doc = buildPresetDoc(app.state?.hostname || '', docs);
    downloadJson('lightpack-presets.json', doc);
    setText('presetInfo', `Exported ${docs.length} preset(s).`);
  } catch (error) {
    setText('presetInfo', `Could not export: ${error.message}`);
  }
}

async function importPresets(file) {
  if (!app.connected) return;

  let presets;
  try {
    presets = parsePresetDoc(await file.text());
  } catch (error) {
    setText('presetInfo', error.message);
    return;
  }

  if (!confirm(`Import ${presets.length} preset(s)? Slots with the same number are overwritten.`)) {
    return;
  }

  app.importing = true;
  clearTimeout(app.commitTimer);
  let done = 0;
  let skipped = 0;

  try {
    for (const preset of presets) {
      const slot = Number(preset?.slot);
      if (!Number.isInteger(slot) || slot < 0 || slot >= 5) {
        skipped++;
        continue;
      }

      const config = sanitizeConfig(preset?.config);
      if (Object.keys(config).length === 0) {
        skipped++;
        continue;
      }

      setText('presetInfo', `Importing slot ${slot + 1} (${done + 1}/${presets.length})…`);

      const seq = ++app.seq;
      await request(
        buildSet(config, seq),
        (frame) => frame.t === 'ack' && frame.cmd === 'set' && Number(frame.seq) === seq,
        3000
      );

      await request(
        commands.presetSave(slot, preset.name || ''),
        (frame) => frame.t === 'presets' ||
          (frame.t === 'ack' && frame.cmd === 'preset' && frame.ok === false),
        2500
      );

      done++;
    }

    // The web page leaves the board on the last imported configuration and
    // saves it; do the same so the two import paths agree.
    await commit();
    setText('presetInfo', `Imported ${done} preset(s)` +
      (skipped ? `, skipped ${skipped}` : '') + '.');
  } catch (error) {
    setText('presetInfo', `Import stopped after ${done}: ${error.message}`);
  } finally {
    app.importing = false;
    renderPresets();
  }
}

//* ---------------------- MISC ACTIONS ----------------------

async function rebootBoard() {
  if (!app.connected) return;
  if (!confirm('Restart the board? The port will disappear for a moment.')) return;

  app.expectReboot = true;
  try {
    await request(commands.reboot(), (frame) => frame.t === 'ack' && frame.cmd === 'reboot', 1500);
  } catch {
    // A lost ack is expected when the reset wins the race.
  }

  setStatus('connecting', 'restarting');
  setSync('restarting');
}

async function copyDiagnostics() {
  const lines = [];
  for (const dt of $$('#diag dt')) {
    const dd = dt.nextElementSibling;
    lines.push(`${dt.textContent}: ${dd?.textContent || ''}`);
  }
  if (app.lastTickFrame) {
    lines.push(`Last tick: ${JSON.stringify(app.lastTickFrame)}`);
  }
  const text = lines.join('\n');

  try {
    await navigator.clipboard.writeText(text);
    setSync('diagnostics copied');
  } catch {
    const area = document.createElement('textarea');
    area.value = text;
    document.body.append(area);
    area.select();
    document.execCommand('copy');
    area.remove();
    setSync('diagnostics copied');
  }
}

function saveLog() {
  const lines = [];
  for (const line of $('#log').children) {
    lines.push(line.textContent);
  }
  const blob = new Blob([lines.join('\n') + '\n'], { type: 'text/plain' });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = `lightpack-console-${timestampForFile()}.log`;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

function downloadJson(filename, value) {
  const blob = new Blob([JSON.stringify(value, null, 1)], { type: 'application/json' });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = filename;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

//* ---------------------- LOG AND STATUS ----------------------

function log(text, kind) {
  if (!text) return;

  const line = document.createElement('div');
  line.className = 'logline';

  if (kind) line.classList.add(kind);
  else if (/\[BOOT\]/.test(text)) line.classList.add('boot');
  else if (/panic|brownout|error|fail|disconnect/i.test(text)) line.classList.add('warn');

  const time = document.createElement('span');
  time.className = 't';
  time.textContent = new Date().toLocaleTimeString([], { hour12: false });

  line.append(time, document.createTextNode(text));

  const box = $('#log');
  const atBottom = box.scrollTop + box.clientHeight >= box.scrollHeight - 12;
  box.append(line);

  while (box.children.length > 400) box.firstElementChild.remove();
  if (atBottom) box.scrollTop = box.scrollHeight;
}

function showError(message) {
  const banner = $('#banner');
  banner.hidden = false;
  $('#bannerText').textContent = message;
  // A command can fail while the link is still perfectly healthy. Keep the
  // connection dot alone; only the text line reports the refusal.
  setText('statusLeft', message);
  log(message, 'err');
}

function clearError() {
  $('#banner').hidden = true;
  $('#bannerText').textContent = '';
}

function setSync(message) {
  setText('statusLeft', message);
}

function setStatus(kind, text) {
  const dot = $('#connDot');
  dot.classList.remove('on', 'busy', 'err');
  if (kind === 'connected') dot.classList.add('on');
  else if (kind === 'connecting') dot.classList.add('busy');
  else if (kind === 'error') dot.classList.add('err');

  $('#connText').textContent =
    kind === 'connected' ? 'Connected' :
    kind === 'connecting' ? 'Connecting…' :
    kind === 'error' ? 'Error' : 'Not connected';

  setText('statusLeft', text || kind);
}

//* ---------------------- HELPERS ----------------------

function setText(id, value) {
  const element = document.getElementById(id);
  if (element) element.textContent = value;
}

function fmt(value, decimals = 2) {
  const number = Number(value);
  return Number.isFinite(number) ? number.toFixed(decimals) : '—';
}

function formatForInput(value, bounds) {
  const number = Number(value);
  if (!Number.isFinite(number)) return '';
  return bounds.decimals > 0 ? number.toFixed(bounds.decimals) : String(Math.round(number));
}

function formatBound(value, bounds) {
  const number = Number(value);
  if (!Number.isFinite(number)) return '—';
  if (bounds.decimals === 0) return String(Math.round(number));
  return number.toFixed(bounds.decimals).replace(/0+$/, '').replace(/\.$/, '');
}

function rgbToHex(red, green, blue) {
  const channel = (value) => Math.max(0, Math.min(255, Number(value) || 0)).toString(16).padStart(2, '0');
  return `#${channel(red)}${channel(green)}${channel(blue)}`;
}

function hexToRgb(hex) {
  const value = String(hex).replace('#', '');
  return [
    parseInt(value.slice(0, 2), 16) || 0,
    parseInt(value.slice(2, 4), 16) || 0,
    parseInt(value.slice(4, 6), 16) || 0,
  ];
}

function netName(value) {
  if (value === 2) return 'connected';
  if (value === 1) return 'not connected';
  return 'offline';
}

function formatUptime(milliseconds) {
  const total = Math.max(0, Math.floor(Number(milliseconds) / 1000));
  const hours = Math.floor(total / 3600);
  const minutes = Math.floor((total % 3600) / 60);
  const seconds = total % 60;
  return `${hours}h ${String(minutes).padStart(2, '0')}m ${String(seconds).padStart(2, '0')}s`;
}

function timestampForFile() {
  const now = new Date();
  const pad = (value) => String(value).padStart(2, '0');
  return `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}-` +
    `${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
}

function loadSetting(key, fallback) {
  try {
    const value = localStorage.getItem(key);
    return value === null ? fallback : value;
  } catch {
    return fallback;
  }
}

function storeSetting(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch {}
}

boot();

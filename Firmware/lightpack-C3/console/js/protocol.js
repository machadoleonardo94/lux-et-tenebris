//? Wire format for the Lightpack USB console.
//?
//? One line is one message. Device frames start with '@' and carry compact JSON;
//? host commands start with '@' too. Everything else on the port is console
//? output and is shown in the log pane. See docs/usb-control.md for the contract.
//!
//? This module is deliberately DOM-free and dependency-free: the browser app,
//? the node tests and any future CLI all parse the port the same way.

import { FIELD_KEYS, FIELD_SPECS } from './spec.js';

export const PROTO_VERSION = 1;
export const MAX_LINE = 1024;

/** Number formatting that the firmware's usb_is_number() accepts. */
export function formatNumber(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) throw new Error(`not a finite number: ${value}`);
  if (Number.isInteger(n)) return String(n);
  // Keep it short but exact enough for every tunable's step. toFixed avoids the
  // 0.30000000000000004 shape that String(0.1 + 0.2) can produce.
  return n.toFixed(4).replace(/0+$/, '').replace(/\.$/, '');
}

/** Split a stream of bytes into lines, preserving partial lines between calls. */
export class LineDecoder {
  constructor() {
    this.decoder = new TextDecoder();
    this.buffer = '';
  }

  push(bytes) {
    this.buffer += this.decoder.decode(bytes, { stream: true });

    // A device that resets mid-line leaves a fragment behind; a pathological
    // stream must not grow this buffer without bound.
    if (this.buffer.length > 65536) {
      const cut = this.buffer.lastIndexOf('\n');
      this.buffer = cut >= 0 ? this.buffer.slice(cut + 1) : '';
    }

    const lines = [];
    let index;
    while ((index = this.buffer.indexOf('\n')) >= 0) {
      let line = this.buffer.slice(0, index);
      this.buffer = this.buffer.slice(index + 1);
      if (line.endsWith('\r')) line = line.slice(0, -1);
      lines.push(line);
    }
    return lines;
  }

  reset() {
    this.buffer = '';
    this.decoder = new TextDecoder();
  }
}

/**
 * Classify one line from the port.
 *   {kind:'frame', frame}  a JSON protocol frame
 *   {kind:'log', text}     console output, not ours
 *   {kind:'bad', text}     a line with '@' that is not valid JSON
 */
export function parseLine(line) {
  if (typeof line !== 'string') throw new Error('line must be a string');
  if (!line.startsWith('@')) return { kind: 'log', text: line };

  try {
    const frame = JSON.parse(line.slice(1));
    if (frame === null || typeof frame !== 'object' || Array.isArray(frame)) {
      return { kind: 'bad', text: line };
    }
    if (typeof frame.t !== 'string') return { kind: 'bad', text: line };
    return { kind: 'frame', frame };
  } catch {
    return { kind: 'bad', text: line };
  }
}

export function buildCommand(verb, ...args) {
  const tail = args.filter((a) => a !== undefined && a !== null && a !== '');
  return `@${verb}${tail.length ? ' ' + tail.join(' ') : ''}\n`;
}

export const commands = {
  ping: () => buildCommand('PING'),
  get: () => buildCommand('GET'),
  modes: () => buildCommand('MODES'),
  commit: () => buildCommand('COMMIT'),
  reload: () => buildCommand('RELOAD'),
  reboot: () => buildCommand('REBOOT'),
  stream: (hz) => buildCommand('STREAM', Math.max(0, Math.min(50, Math.round(hz)))),
  radio: (mode) => buildCommand('RADIO', mode),
  impact: (magnitude) => buildCommand('IMPACT', Math.max(0, Math.min(255, Math.round(magnitude)))),
  stress: (color, ceiling, step, railFloor) => buildCommand('STRESS', color, ceiling, step, railFloor),
  sense: (count, burst) => buildCommand('SENSE', count, burst),
  safeOff: () => buildCommand('SAFE', 'OFF'),
  presetList: () => buildCommand('PRESET', 'LIST'),
  presetGet: (slot) => buildCommand('PRESET', 'GET', slot),
  presetApply: (slot) => buildCommand('PRESET', 'APPLY', slot),
  presetClear: (slot) => buildCommand('PRESET', 'CLEAR', slot),
  updateBegin: (size) => buildCommand('UPDATE', 'BEGIN', size),
  updateData: (offset, base64) => buildCommand('UPDATE', 'DATA', offset, base64),
  updateEnd: () => buildCommand('UPDATE', 'END'),
  updateAbort: () => buildCommand('UPDATE', 'ABORT'),
  presetSave: (slot, name) => {
    const clean = String(name ?? '').replace(/[\r\n]/g, ' ').trim();
    return buildCommand('PRESET', 'SAVE', slot, clean ? `name=${clean}` : '');
  },
};

/**
 * One batch of /api/config fields. The firmware validates every key and value
 * before applying any of them, so an unknown key rejects the whole line.
 */
export function buildSet(values, seq) {
  const parts = [];
  for (const [key, raw] of Object.entries(values)) {
    if (!FIELD_KEYS.includes(key)) throw new Error(`unknown config field '${key}'`);
    const value = Number(raw);
    if (!Number.isFinite(value)) throw new Error(`bad value for '${key}'`);
    parts.push(`${key}=${formatNumber(value)}`);
  }
  if (seq !== undefined && seq !== null) parts.push(`seq=${seq}`);
  if (parts.length === 0) throw new Error('a SET needs at least one field');

  const line = `@SET ${parts.join('&')}\n`;
  if (line.length > MAX_LINE) throw new Error('SET line is too long for the device');
  return line;
}

/** The preset fields the state document and the preset table share, in spec order. */
export function configFromState(state) {
  const config = {};
  for (const key of FIELD_KEYS) {
    if (FIELD_SPECS[key] && FIELD_SPECS[key].preset === false) continue;
    if (state && Object.prototype.hasOwnProperty.call(state, key)) config[key] = state[key];
  }
  return config;
}

/** Filter an imported preset's config down to known, finite fields. */
export function sanitizeConfig(config) {
  const clean = {};
  if (!config || typeof config !== 'object') return clean;
  for (const key of FIELD_KEYS) {
    if (!Object.prototype.hasOwnProperty.call(config, key)) continue;
    const raw = config[key];
    // null, undefined and an empty string all coerce to 0 through Number(),
    // and 0 is a perfectly valid brightness. Refuse them explicitly rather
    // than turning a missing value into "turn the LEDs off".
    if (raw === null || raw === undefined || raw === '' || typeof raw === 'boolean') continue;
    const value = Number(raw);
    if (Number.isFinite(value)) clean[key] = value;
  }
  return clean;
}

/** The wrapper the device page's export uses, so both tools read the same file. */
export function buildPresetDoc(device, presets) {
  return {
    lightpack_presets: 1,
    device: device || '',
    slots: presets.length,
    presets,
  };
}

export function parsePresetDoc(text) {
  let doc;
  try {
    doc = JSON.parse(text);
  } catch {
    throw new Error('that file is not JSON');
  }
  const presets = Array.isArray(doc?.presets) ? doc.presets : [];
  if (presets.length === 0) throw new Error('no presets in that file');
  return presets;
}

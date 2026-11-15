//? Protocol and metadata tests. No board, no browser: these pin the wire
//? format and the field list so a firmware change that renames a key or a host
//? change that drops one fails before anyone plugs a cable in.
//?
//?   node console/tests/protocol.test.mjs
//? or, from console/:  npm test
//!
//? Run it as a file rather than through `node --test <dir>`: the directory form
//? spawns one child per test file, which some sandboxes deny. Executing the file
//? runs the same node:test suite in-process.

import test from 'node:test';
import assert from 'node:assert/strict';

import {
  LineDecoder,
  buildSet,
  buildCommand,
  buildPresetDoc,
  commands,
  configFromState,
  formatNumber,
  parseLine,
  parsePresetDoc,
  sanitizeConfig,
} from '../js/protocol.js';
import { FIELD_KEYS, FIELD_SPECS, boundsFor } from '../js/spec.js';

// The 39 preset names in src/services/presets.h's PRESET_FIELDS, written out here
// on purpose: if the firmware table and the browser spec drift apart, this fails.
// `budget` is a /api/config field but deliberately not a preset field.
const FIRMWARE_FIELDS = [
  'mode',
  'red', 'green', 'blue', 'brightness',
  'whip_speed', 'whip_length', 'whip_period', 'whip_peak', 'whip_offset',
  'rainbow_speed', 'rainbow_min_ms', 'rainbow_max_ms', 'rainbow_level',
  'leds',
  'flame_decay', 'flame_spatial', 'flame_power_slope', 'flame_power_max', 'flame_power_idle',
  'fade_decay', 'fade_trigger', 'fade_length_gain', 'fade_length_z_gain', 'fade_length_base',
  'fade_power_gain', 'fade_power_base', 'fade_color_gain', 'fade_idle_length', 'fade_idle_power',
  'step_baseline_tau_ms', 'step_threshold', 'step_release', 'step_jerk_min', 'step_min_gap_ms',
  'step_impact_min', 'step_impact_max', 'step_magnitude_min', 'step_magnitude_max',
];

test('the browser spec covers every config field and every preset field', () => {
  const presetKeys = FIELD_KEYS.filter((k) => FIELD_SPECS[k].preset !== false);
  assert.equal(FIELD_KEYS.length, 40);
  assert.equal(new Set(FIELD_KEYS).size, 40);
  assert.equal(presetKeys.length, 39);
  assert.deepEqual([...presetKeys].sort(), [...FIRMWARE_FIELDS].sort());
  assert.ok(FIELD_KEYS.includes('budget'));
});

test('formatNumber keeps values in the form usb_is_number() accepts', () => {
  assert.equal(formatNumber(128), '128');
  assert.equal(formatNumber(0.5), '0.5');
  assert.equal(formatNumber(0.1 + 0.2), '0.3');
  assert.equal(formatNumber(-1.25), '-1.25');
  assert.throws(() => formatNumber(Number.NaN));
  assert.throws(() => formatNumber('abc'));
});

test('buildSet builds one ordered batch and rejects unknown fields', () => {
  assert.equal(
    buildSet({ brightness: 128, mode: 5 }, 7),
    '@SET brightness=128&mode=5&seq=7\n'
  );
  assert.equal(buildSet({ flame_decay: 0.9 }), '@SET flame_decay=0.9\n');
  assert.throws(() => buildSet({ widht: 1 }), /unknown config field/);
  assert.throws(() => buildSet({ brightness: Number.NaN }), /bad value/);
  assert.throws(() => buildSet({}), /at least one field/);
});

test('one full 40-field batch fits the firmware line limit', () => {
  const values = {
    mode: 2, red: 255, green: 128, blue: 64, brightness: 200, budget: 2000,
    whip_speed: 20, whip_length: 250, whip_period: 5000, whip_peak: 255, whip_offset: 255,
    rainbow_speed: 20, rainbow_min_ms: 10000, rainbow_max_ms: 10000, rainbow_level: 255,
    leds: 250,
    flame_decay: 0.995, flame_spatial: 10, flame_power_slope: 25,
    flame_power_max: 255, flame_power_idle: 255,
    fade_decay: 0.995, fade_trigger: 2, fade_length_gain: 40, fade_length_z_gain: 40,
    fade_length_base: 40, fade_power_gain: 10, fade_power_base: 50,
    fade_color_gain: 20, fade_idle_length: 150, fade_idle_power: 255,
    step_baseline_tau_ms: 20000, step_threshold: 20, step_release: 19.99,
    step_jerk_min: 500, step_min_gap_ms: 10000,
    step_impact_min: 49.9, step_impact_max: 50,
    step_magnitude_min: 254, step_magnitude_max: 255,
  };
  const line = buildSet(values, 9999);
  assert.ok(line.length < 1024, `line was ${line.length} bytes`);
  assert.match(line, /^@SET mode=2&red=255&/);
  assert.match(line, /seq=9999\n$/);
});

test('command builders match the firmware verbs', () => {
  assert.equal(commands.ping(), '@PING\n');
  assert.equal(commands.get(), '@GET\n');
  assert.equal(commands.commit(), '@COMMIT\n');
  assert.equal(commands.stream(20), '@STREAM 20\n');
  assert.equal(commands.stream(999), '@STREAM 50\n');
  assert.equal(commands.radio('off'), '@RADIO off\n');
  assert.equal(commands.impact(300), '@IMPACT 255\n');
  assert.equal(commands.presetSave(2, 'Bench test'), '@PRESET SAVE 2 name=Bench test\n');
  assert.equal(commands.presetSave(2, '  '), '@PRESET SAVE 2\n');
  assert.equal(buildCommand('PRESET', 'LIST'), '@PRESET LIST\n');
  assert.equal(commands.stress('white'), '@STRESS white\n');
  assert.equal(commands.stress('blue'), '@STRESS blue\n');
  assert.equal(commands.stress(7, 4000, 500), '@STRESS 7 4000 500\n');
  assert.equal(commands.stress(3, 500, 2000), '@STRESS 3 500 2000\n');
  assert.equal(commands.stress(7, 4000, 500, 0), '@STRESS 7 4000 500 0\n');
  assert.equal(commands.stress(7, 6000, 500, 3000), '@STRESS 7 6000 500 3000\n');
  assert.equal(commands.sense(20, 200), '@SENSE 20 200\n');
  assert.equal(commands.sense(100, 2000), '@SENSE 100 2000\n');
  assert.equal(commands.safeOff(), '@SAFE OFF\n');
});

test('LineDecoder survives arbitrary chunk boundaries and CRLF', () => {
  const decoder = new LineDecoder();
  const text = '@{"t":"tick","ms":1}\r\n[BOOT] hello\n@{"t":"ack"}\n';
  const bytes = new TextEncoder().encode(text);

  assert.deepEqual(decoder.push(bytes.slice(0, 7)), []);
  assert.deepEqual(decoder.push(bytes.slice(7, 24)), ['@{"t":"tick","ms":1}']);
  const rest = decoder.push(bytes.slice(24));
  assert.deepEqual(rest, ['[BOOT] hello', '@{"t":"ack"}']);
});

test('LineDecoder handles a multi-byte character split across reads', () => {
  const decoder = new LineDecoder();
  const bytes = new TextEncoder().encode('[WEB] café\n');
  assert.deepEqual(decoder.push(bytes.slice(0, 10)), []);
  assert.deepEqual(decoder.push(bytes.slice(10)), ['[WEB] café']);
});

test('parseLine separates frames, logs and malformed frames', () => {
  assert.deepEqual(parseLine('[BOOT] reset reason: power-on'), {
    kind: 'log',
    text: '[BOOT] reset reason: power-on',
  });
  assert.deepEqual(parseLine('@{"t":"ack","ok":true}'), {
    kind: 'frame',
    frame: { t: 'ack', ok: true },
  });
  assert.equal(parseLine('@not json').kind, 'bad');
  assert.equal(parseLine('@null').kind, 'bad');
  assert.equal(parseLine('@[1,2]').kind, 'bad');
  assert.equal(parseLine('@{"ok":true}').kind, 'bad');
});

test('configFromState picks the 39 preset fields and ignores derived values', () => {
  const state = {
    mode: 5, red: 1, green: 2, blue: 3, brightness: 4,
    whip_travel_ms: 999, mode_key: 'whip', active_leds: 60,
  };
  const config = configFromState(state);
  assert.deepEqual(Object.keys(config).sort(), ['blue', 'brightness', 'green', 'mode', 'red']);
  assert.equal(config.whip_travel_ms, undefined);
});

test('sanitizeConfig drops unknown keys and non-finite values', () => {
  const clean = sanitizeConfig({
    mode: 4,
    brightness: '999',
    red: null,
    widht: 3,
    flame_decay: 'nope',
  });
  assert.deepEqual(clean, { mode: 4, brightness: 999 });
});

test('bounds come from the live state, not from a second hard-coded copy', () => {
  const state = {
    num_leds: 250,
    strip_leds_min: 1,
    active_leds: 60,
    whip_length_min: 2,
    whip_speed_min: 0.5,
    whip_speed_max: 12,
    whip_period_min: 300,
    whip_period_max: 4000,
    step_threshold: 3.5,
    step_release_min: 0.02,
    step_jerk_floor: 1,
    step_jerk_max: 200,
    flame_decay_min: 0.1,
    flame_decay_max: 0.99,
    flame_spatial_min: 0.2,
    flame_spatial_max: 8,
  };

  assert.deepEqual(boundsFor('leds', state), {
    min: 1, max: 250, step: 1, decimals: 0, unit: '',
  });
  assert.equal(boundsFor('whip_length', state).max, 60);
  assert.equal(boundsFor('whip_length', state).min, 2);
  assert.equal(boundsFor('whip_speed', state).min, 0.5);
  assert.equal(boundsFor('whip_speed', state).max, 12);
  assert.equal(boundsFor('whip_period', state).min, 300);
  assert.equal(boundsFor('whip_period', state).max, 4000);
  assert.equal(boundsFor('step_threshold', state).max, 20);
  assert.equal(boundsFor('step_release', state).max, 3.5);
  assert.equal(boundsFor('step_release', state).min, 0.02);
  assert.equal(boundsFor('step_jerk_min', state).min, 1);
  assert.equal(boundsFor('step_jerk_min', state).max, 200);
  assert.equal(boundsFor('flame_decay', state).min, 0.1);
  assert.equal(boundsFor('flame_decay', state).max, 0.99);
  assert.equal(boundsFor('flame_spatial', state).min, 0.2);
});

test('preset files round-trip through the device page wrapper', () => {
  const doc = buildPresetDoc('lightpack', [
    { slot: 2, name: 'bench', format: 2, fields: 28, config: { mode: 5 } },
  ]);
  assert.deepEqual(doc, {
    lightpack_presets: 1,
    device: 'lightpack',
    slots: 1,
    presets: [{ slot: 2, name: 'bench', format: 2, fields: 28, config: { mode: 5 } }],
  });

  const parsed = parsePresetDoc(JSON.stringify(doc));
  assert.equal(parsed.length, 1);
  assert.equal(parsed[0].slot, 2);
  assert.throws(() => parsePresetDoc('not json'), /not JSON/);
  assert.throws(() => parsePresetDoc('{"presets":[]}'), /no presets/);
});

//? Parameter metadata for the USB console.
//?
//? Keys are the /api/config argument names, verbatim. The ranges are NOT a
//? second copy of the firmware's: where the state document exports a limit
//? (whip_speed_min, step_jerk_floor, ...) the bounds function reads it. The
//? constants below are only the fallbacks used before the first GET, and the
//? units/labels/steps that are purely presentational.
//!
//! If a field is added to the firmware, add it here and to FIELD_KEYS; the
//! node test asserts the list has 28 unique names, and the protocol builder
//! refuses anything not in it.

export const FIELD_SPECS = {
  // The active script. It lives in the field table because it is a /api/config
  // argument and a preset field, but it has no page of its own: the mode bar is
  // the only sane control for it, so fieldsForPage() never returns it.
  mode: {
    label: 'Mode', page: null, min: 1, max: 6, step: 1, decimals: 0,
    help: 'Active LED script. Use the mode buttons above.',
  },

  // Shared
  red: {
    label: 'Red', page: 'shared', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Red channel of the palette. Drives Solid color, the whip hump and the flame.',
  },
  green: {
    label: 'Green', page: 'shared', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Green channel of the palette.',
  },
  blue: {
    label: 'Blue', page: 'shared', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Blue channel of the palette.',
  },
  brightness: {
    label: 'Brightness', page: 'shared', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Applied by the NeoPixel driver to every mode.',
  },
  leds: {
    label: 'LED count', page: 'shared', min: (s) => s.strip_leds_min ?? 1,
    max: (s) => s.num_leds ?? 250, step: 1, decimals: 0,
    help: 'How many LEDs the scripts light, from the start of the strip. The driver still owns the whole buffer.',
  },

  // Step flame
  flame_decay: {
    label: 'Decay', page: 'flame', min: (s) => s.flame_decay_min ?? 0,
    max: (s) => s.flame_decay_max ?? 0.995, step: 0.001, decimals: 3,
    help: 'Fraction of its brightness each LED keeps per frame at the start of the strip.',
  },
  flame_spatial: {
    label: 'Spatial decay', page: 'flame', min: (s) => s.flame_spatial_min ?? 0,
    max: (s) => s.flame_spatial_max ?? 10, step: 0.1, decimals: 2,
    help: 'How much faster the fade gets along the strip. At 0 the whole strip fades together.',
  },
  flame_power_slope: {
    label: 'Power rise', page: 'flame', min: 0,
    max: (s) => s.flame_power_slope_max ?? 25, step: 0.5, decimals: 1, unit: '/mag',
    help: 'Flash brightness gained per unit of reported impact magnitude.',
  },
  flame_power_max: {
    label: 'Power max', page: 'flame', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Ceiling on the flash brightness, 0-255.',
  },
  flame_power_idle: {
    label: 'Idle power', page: 'flame', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Brightness the strip rests at between impacts, 0-255.',
  },

  // Impact detector
  step_baseline_tau_ms: {
    label: 'Baseline tau', page: 'impact',
    min: (s) => s.step_baseline_tau_min_ms ?? 100,
    max: (s) => s.step_baseline_tau_max_ms ?? 20000, step: 50, decimals: 0, unit: 'ms',
    help: 'How long the resting-magnitude average remembers.',
  },
  step_threshold: {
    label: 'Threshold', page: 'impact',
    min: (s) => s.step_threshold_min ?? 0.05,
    max: (s) => s.step_threshold_max ?? 20, step: 0.05, decimals: 2, unit: 'g',
    help: 'Departure from the baseline that arms the detector.',
  },
  step_release: {
    label: 'Release', page: 'impact',
    min: (s) => s.step_release_min ?? 0.01,
    max: (s) => s.step_threshold ?? 20, step: 0.01, decimals: 2, unit: 'g',
    help: 'Departure the detector must fall back under before it reports. Kept below Threshold by the firmware.',
  },
  step_jerk_min: {
    label: 'Jerk gate', page: 'impact',
    min: (s) => s.step_jerk_floor ?? 0,
    max: (s) => s.step_jerk_max ?? 500, step: 1, decimals: 1, unit: 'g/s',
    help: 'How fast the magnitude must be changing to count as a knock rather than a swing. 0 disables the gate.',
  },
  step_min_gap_ms: {
    label: 'Refractory', page: 'impact',
    min: (s) => s.step_gap_min_ms ?? 20,
    max: (s) => s.step_gap_max_ms ?? 10000, step: 10, decimals: 0, unit: 'ms',
    help: 'Shortest gap between two accepted impacts.',
  },
  step_impact_min: {
    label: 'Impact min', page: 'impact',
    min: (s) => s.step_impact_min_floor ?? 0,
    max: (s) => s.step_impact_max_ceiling ?? 50, step: 0.1, decimals: 2, unit: 'g',
    help: 'Softest impact that maps to the bottom of the magnitude scale.',
  },
  step_impact_max: {
    label: 'Impact max', page: 'impact',
    min: (s) => s.step_impact_min_floor ?? 0,
    max: (s) => s.step_impact_max_ceiling ?? 50, step: 0.1, decimals: 2, unit: 'g',
    help: 'Hardest impact that maps to the top of the magnitude scale.',
  },
  step_magnitude_min: {
    label: 'Mag min', page: 'impact', min: 0, max: 254, step: 1, decimals: 0,
    help: 'Bottom of the scale Step flame reads.',
  },
  step_magnitude_max: {
    label: 'Mag max', page: 'impact', min: 1, max: 255, step: 1, decimals: 0,
    help: 'Top of the scale Step flame reads.',
  },

  // Pulse whip
  whip_speed: {
    label: 'Speed', page: 'whip',
    min: (s) => s.whip_speed_min ?? 0.1,
    max: (s) => s.whip_speed_max ?? 20, step: 0.1, decimals: 2, unit: 'LED/frame',
    help: 'How far the hump advances each frame (~50 ms).',
  },
  whip_length: {
    label: 'Length', page: 'whip',
    min: (s) => s.whip_length_min ?? 1,
    max: (s) => s.active_leds ?? 250, step: 1, decimals: 0, unit: 'LEDs',
    help: 'Width of the half-sine hump. Capped by the live strip length.',
  },
  whip_period: {
    label: 'Period', page: 'whip',
    min: (s) => s.whip_period_min ?? 200,
    max: (s) => s.whip_period_max ?? 5000, step: 10, decimals: 0, unit: 'ms',
    help: 'Gap between successive whip launches. Above the crossing time the pulses are discrete.',
  },
  whip_peak: {
    label: 'Peak', page: 'whip', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Amplitude of the hump.',
  },
  whip_offset: {
    label: 'Offset', page: 'whip', min: 0, max: 255, step: 1, decimals: 0,
    help: 'Level the strip rests at between whips.',
  },

  // Rainbow flow
  rainbow_speed: {
    label: 'Speed', page: 'rainbow',
    min: (s) => s.rainbow_speed_min ?? 0.1,
    max: (s) => s.rainbow_speed_max ?? 20, step: 0.1, decimals: 2, unit: 'LED/frame',
    help: 'How far each colour travels each frame (~50 ms).',
  },
  rainbow_min_ms: {
    label: 'Min wait', page: 'rainbow',
    min: (s) => s.rainbow_gap_floor_ms ?? 50,
    max: (s) => s.rainbow_gap_ceiling_ms ?? 10000, step: 50, decimals: 0, unit: 'ms',
    help: 'Shortest wait before the starting LED gets a new random colour.',
  },
  rainbow_max_ms: {
    label: 'Max wait', page: 'rainbow',
    min: (s) => s.rainbow_gap_floor_ms ?? 50,
    max: (s) => s.rainbow_gap_ceiling_ms ?? 10000, step: 50, decimals: 0, unit: 'ms',
    help: 'Longest wait before the starting LED gets a new random colour.',
  },
};

export const FIELD_KEYS = Object.keys(FIELD_SPECS);

export const PAGES = [
  { id: 'shared', label: 'Shared', hint: 'Palette, brightness and the number of LEDs every mode draws on.' },
  { id: 'flame', label: 'Step flame', hint: 'How an impact is drawn, and how long it takes to fade.' },
  { id: 'impact', label: 'Impact detector', hint: 'What counts as an impact. This is shared by every mode that reacts to movement.' },
  { id: 'whip', label: 'Pulse whip', hint: 'A hump of light travelling from the start of the strip.' },
  { id: 'rainbow', label: 'Rainbow flow', hint: 'Random colours drifting toward the far end.' },
  { id: 'fade', label: 'Gyro fade', hint: 'No tunables: the gyro drives the colour and the lit length directly.' },
  { id: 'presets', label: 'Presets', hint: 'Five complete configurations stored in NVS.' },
  { id: 'diagnostics', label: 'Diagnostics', hint: 'Device, network, sensor and input state.' },
];

export const PALETTE_SWATCHES = [
  ['Magenta', 255, 0, 255],
  ['Red', 255, 0, 0],
  ['Amber', 255, 140, 0],
  ['Lime', 120, 255, 0],
  ['Green', 0, 255, 90],
  ['Cyan', 0, 210, 255],
  ['Blue', 40, 90, 255],
  ['White', 255, 255, 255],
];

function resolve(value, state, fallback) {
  if (typeof value === 'function') {
    const got = value(state || {});
    return Number.isFinite(got) ? got : fallback;
  }
  return Number.isFinite(value) ? value : fallback;
}

/**
 * The range, step and unit for one field, using the live state when it has
 * already been read. Callers must have a state before rendering a page; the
 * fallbacks keep the form usable for the fraction of a second before the first
 * GET, and they are the firmware's own defaults.
 */
export function boundsFor(key, state) {
  const spec = FIELD_SPECS[key];
  if (!spec) throw new Error(`unknown field ${key}`);

  let min = resolve(spec.min, state, 0);
  let max = resolve(spec.max, state, 255);
  if (max < min) max = min;

  return {
    min,
    max,
    step: spec.step,
    decimals: spec.decimals,
    unit: spec.unit || '',
  };
}

export function fieldsForPage(page) {
  return FIELD_KEYS.filter((key) => FIELD_SPECS[key].page === page);
}

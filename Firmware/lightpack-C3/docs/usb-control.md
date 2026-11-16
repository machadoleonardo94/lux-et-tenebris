# Lightpack USB control protocol (v1)

This is the wire contract for the USB console. It lives on the ESP32-C3's
**USB Serial/JTAG** port — the same CDC-ACM device the firmware uses as its
console — and needs no driver on Windows 10+, macOS or Linux (Linux needs the
usual `dialout`/udev access).

The point is the antenna: configuration must not depend on a WiFi link. This
path works with the radio switched off, and it keeps working while the board is
sitting in its 60-second configuration hotspot.

> The browser console in [`../console/`](../console/) speaks this protocol. It is
> the reference client; this document is the reference for anything else.

---

## 1. Framing

The console is not quiet. Boot report, WiFi events, `[MODE]`, `[ADC]` and the
10-second telemetry block all use the same port. The protocol therefore does not
assume it owns the stream:

* One message is one line, `\n` terminated. `\r` is ignored.
* **Every protocol line starts with `@`.** Nothing else in the firmware starts a
  line with `@`; a host ignores every other line and may show it as a device log.
* Device → host frames are `@` + compact JSON + `\n`.
* Host → device commands are `@` + verb + arguments + `\n`.
* The maximum inbound line is **1024 bytes**; a longer line is discarded to the
  next newline and answered with an error. A `SET` with all 28 fields is ~700 B.
* A device frame is written with **one** `Serial.write()`. `HWCDC::write()` holds
  its transmit mutex for the whole call, so another task's log print can land
  before or after a frame, never inside it.
* Text is ASCII/UTF-8. There is no binary, base64 or escaping layer: every value
  is a number, and the only free text is a preset name.

Open the port at **115200** for convention. The C3's USB CDC ignores the baud
rate; the link is USB full speed.

---

## 2. Handshake

Send `@PING` after opening the port. The board may still be in `setup()` (the
strip self-test and gyro probe take a few seconds) or pumping the config portal,
so allow up to ten seconds for the first answer and send `@PING` again if the
first one is lost.

```json
@{"t":"pong","v":1,"proto":1,"fw":"0.5.0-dev","build":"Nov 14 2026 01:18:00",
  "chip":"ESP32-C3","cpu_mhz":160,"mac":"AA:BB:CC:DD:EE:FF","uptime_ms":123456}
```

Check `proto` before anything else. Version 1 is what this document describes.
Unknown fields in a reply must be ignored; a new field is not a version bump.

The browser client then reads:

```
@MODES
@GET
@PRESET LIST
```

and enables the controls only when all four replies have arrived.

---

## 3. Commands

Verbs are case-insensitive. Field names are **lowercase and exact**. `seq` is
optional on `SET` and is echoed there; the user-paced commands are one at a time
and do not use it.

| Command | Reply | Purpose |
|---|---|---|
| `@PING` | `pong` | Identity, protocol version, build date |
| `@GET` | `state` | Read the whole live configuration |
| `@MODES` | `modes` | The mode registry, for the mode buttons |
| `@SET k=v&k=v…` | `ack` + `changed` | Apply a batch in RAM, then sanitise |
| `@COMMIT` | `ack` | Write the live configuration to NVS |
| `@RELOAD` | `ack` + `state` | Discard RAM edits, reload NVS |
| `@STREAM hz` | `ack` | Telemetry ticks at 0–50 Hz; 0 stops |
| `@PRESET LIST` | `presets` | Slot list + names + states |
| `@PRESET GET n` | `preset` | One slot, the export payload |
| `@PRESET SAVE n [name=…]` | `presets` | Capture the live configuration into slot `n` |
| `@PRESET APPLY n` | `state` | Apply a slot, and save it as the boot configuration |
| `@PRESET CLEAR n` | `presets` | Empty a slot |
| `@IMPACT mag` | `ack` | Inject a preview impact (0–255) for flame tuning |
| `@RADIO on\|off\|toggle` | `ack` | Switch the WiFi radio; USB is unaffected |
| `@STRESS <mask> [ceiling] [step] [rail_floor]` | `ack` | Power stress test; mask R=1 G=2 B=4, ceiling 500–6000 mA, step 500/2000 ms, rail floor 0=ignore / 3000 mV |
| `@SENSE <count> [burst_ms]` | `ack` | Channel resistance: 1–100 white LEDs, 200/2000 ms burst, 5 s unplug |
| `@SAFE OFF` | `ack` | Clear safe mode without a power cycle |
| `@UPDATE BEGIN <size>` | `ack` (`state:"ready"`) | Start an app-only USB update into the inactive app slot |
| `@UPDATE DATA <offset> <base64>` | `ack` (`state:"data"`) | One 2 KiB chunk; offset must equal the board's received count or be a retry |
| `@UPDATE END` | `ack` (`state:"complete"`) | Mark the new slot bootable and reboot |
| `@UPDATE ABORT` | `ack` (`state:"aborted"`) | Drop the partial image; the running image is unchanged |
| `@REBOOT` | `ack`, then reset | Restart the board |

`n` is 0-based (slots 0–4), matching `/api/preset` and the JSON files.

### 3.1 `SET` — the batch primitive

```
@SET mode=5&red=255&green=140&blue=0&brightness=180&leds=60&whip_speed=4.5&seq=7
```

Rules:

1. **All-or-nothing.** Every key must be one of the 28 `/api/config` fields in
   the table below, every value must parse as a number, and a `mode` value must
   name a live mode. If one pair fails, *nothing* is applied and the ack names
   the offending token.
2. **Transient.** `SET` changes RAM and the strip but writes no flash. `COMMIT`
   is the only USB path that persists. This is what lets a UI send edits at
   drag rate without hammering NVS.
3. `seq` is a host request id, not a config field. It is parsed out before
   validation and echoed in the ack.
4. Fields are applied in the order the line gives them. The sanitisers then run
   once on the final values, so a batch may move both sides of a relation
   (`threshold` and `release`, `leds` and `whip_length`) without an intermediate
   state being rejected.
5. At most 32 pairs are accepted (28 fields + `seq` + headroom).

Ack:

```json
@{"t":"ack","v":1,"ok":true,"cmd":"set","seq":7,"dirty":true,"n":6,
  "changed":{"mode":5,"whip_length":24}}
```

`changed` contains **only the fields whose value actually moved**, after
sanitising. It includes fields that were not in the request (shrinking `leds`
pulls `whip_length` down; lowering `threshold` pulls `release` down). This is
why the ack is not a full state: building the state opens NVS for the saved
SSID, which is not something a slider tick should pay for.

`dirty` is the device's own view: true means RAM differs from flash.

### 3.2 `COMMIT` and `RELOAD`

* `@COMMIT` calls the same writer as the web page's `POST /api/config`. It writes
  all configuration keys; NVS skips the ones that did not change. A mode change
  made with the physical switches also saves the whole live configuration —
  including any unsaved USB edits — because it calls the same writer. That is
  the one cross-surface surprise worth knowing.
* `@RELOAD` re-reads NVS and replies with a fresh `state`. Unsaved edits are
  gone.

### 3.3 `STREAM` and ticks

```
@STREAM 20
@{"t":"ack","v":1,"ok":true,"cmd":"stream","dirty":false,"hz":20}
```

Ticks are paced by `millis()`, never by loop count. If the loop is slower than
the requested rate, ticks arrive at the loop rate; `lp` in the frame says what
that is. Stop with `@STREAM 0` (or unplugging; the firmware stops the stream on
disconnect).

```json
@{"t":"tick","v":1,"ms":123456,"mode":2,
  "ax":-0.12,"ay":0.03,"az":9.81,
  "gx":0.10,"gy":-0.20,"gz":0.00,
  "imp":12,"ic":3,"ia":850,
  "lp":19,"rail":4012,"heap":123456,
  "net":0,"dirty":true,"portal":false,"btn":0}
```

| Field | Meaning |
|---|---|
| `ms` | `millis()` |
| `mode` | Active mode id |
| `ax`/`ay`/`az` | Acceleration, m/s² |
| `gx`/`gy`/`gz` | Angular rate, rad/s |
| `imp` | Last reported impact magnitude (0–255) |
| `ic` | Total accepted impacts since boot |
| `ia` | Age of the last impact, ms (0 when `ic` is 0) |
| `lp` | Loop iterations per second, measured over the last second |
| `rail` | V_REG in mV, refreshed once a second |
| `heap` | Free heap, bytes |
| `net` | 0 offline, 1 connecting, 2 connected |
| `dirty` | RAM differs from flash |
| `portal` | The provisioning hotspot is open; `RADIO` is refused while this is true |
| `btn` | Bit 0 touch switch down, bit 1 BOOT switch down |

`imp`/`ic`/`ia` read the detector's diagnostics. They do **not** consume the
impact latch, so watching telemetry cannot steal an event from Step flame.

### 3.4 Events

Emitted whenever a host is attached, even with `STREAM 0`:

```json
@{"t":"evt","v":1,"k":"mode","id":5,"key":"whip","label":"Pulse whip"}
@{"t":"evt","v":1,"k":"impact","mag":12,"count":3,"ms":123456}
@{"t":"evt","v":1,"k":"button","b":"touch","e":"short","n":4}
```

Events are source-agnostic: a mode changed from the pad, the web page, a preset
or a USB `SET` all produce the same `mode` event.

### 3.5 Presets

`@PRESET LIST`:

```json
@{"t":"presets","v":1,"slots":[
  {"slot":0,"used":true,"state":"ok","name":"Bench warm"},
  {"slot":1,"used":false,"state":"empty","name":""}
]}
```

`state` is `ok`, `empty` or `incompatible` (a slot written by a firmware whose
packed preset struct had another layout). `used` means usable, not merely
occupied.

`@PRESET GET 2`:

```json
@{"t":"preset","v":1,"preset":{
  "slot":2,"name":"Case fitted","format":2,"fields":28,
  "config":{"mode":5,"red":255,…}
}}
```

`SAVE` takes the **live** configuration; it has no way to write values straight
into a slot. Import therefore applies a config with `SET` and then saves it, in
that order. The JSON shape is exactly what the device page exports:

```json
{"lightpack_presets":1,"device":"lightpack","slots":1,
 "presets":[{"slot":2,"name":"Case fitted","format":2,"fields":28,"config":{…}}]}
```

`name=` is the rest of the line, so spaces are fine. Newlines are not; a host
should strip them. An empty name keeps the slot's existing label or falls back
to `Preset N`.

### 3.6 Radio

`@RADIO off` stores the offline switch and takes the radio down; `on` clears the
switch and starts a join in the background; `toggle` inverts the live state.
The USB link, strip and every mode keep working either way.

While the configuration hotspot is open, the board is still inside `setup()`;
the USB console is serviced from the portal's own loop, and `RADIO` is refused
with `"the config hotspot is open - close it before switching the radio"`. That
is deliberate: tearing the radio down underneath WiFiManager is not a supported
transition. Every other command works in that window, and the strip renders
`SET` changes there too.

The reply carries the resulting truth, which may differ from the request (for
example, turning the radio on with no saved network leaves it off):

```json
@{"t":"ack","v":1,"ok":true,"cmd":"radio","dirty":false,
  "offline":true,"net":"offline","rssi":0}
```

### 3.7 Impact preview

`@IMPACT 24` writes the magnitude into the detector's latch, exactly as a real
impact would. The flame mode consumes it on its next frame. The detector's
counters are untouched. In a mode other than Step flame the value waits in the
latch, so select the mode first if you want to see it immediately.

### 3.8 USB updater

The console (or `tools/usb_updater.py`) sends the app image to the **inactive app
slot** over the same line. It does not enter the ROM bootloader and does not touch
NVS. During the transfer the strip is held off and the status pixel heartbeats
purple; a stall turns it solid and a failure gives three red blinks.

```
@UPDATE BEGIN 1304880
@UPDATE DATA 0 <base64 of 2048 bytes>
@UPDATE DATA 2048 <base64>
…
@UPDATE END
```

`BEGIN` replies with `state:"ready"`, `received` and `total`; each `DATA` replies
with `state:"data"`, `received` and `total`; `END` replies `state:"complete"` and
the board restarts after a short delay. Errors are normal `ack` frames with
`ok:false` and `err`.

The firmware writes through `Update.h` to `esp_ota_get_next_update_partition()`;
the running image stays valid until `END`. The base64 line is capped at the
protocol's 4 KiB line length, hence 2048-byte chunks. `offset` is the count of
bytes already received: a retried chunk whose ack was lost is acknowledged
without being written twice. `ABORT` (or a disconnect) aborts the partial
image.

This command exists from **0.5.0-dev**; older builds answer `unknown command`.
A board that predates it must be flashed once with PlatformIO or the browser
installer. For a firmware-agnostic route, `tools/usb_installer.py` uses the C3
ROM bootloader instead: hold **GPIO9 (BOOT)** while plugging the board in or
pressing RESET, release once the port appears, then run the installer. It writes
`boot_app0.bin` at `0xE000` and the app at `0x10000`; NVS is untouched.

### 3.9 Power stress test and safe mode

`@STRESS 7` starts the bench characterisation of the supply. The colour is a
**3-bit mask**: `R=1`, `G=2`, `B=4` (7 white, 3 yellow, 5 magenta, 6 cyan). The
old names `white`/`red`/`green`/`blue` are still accepted. Optionally,
`@STRESS 7 4000 500 0` selects a **test-only ceiling** (500–6000 mA, default
2000; out-of-range values are clamped), a **step time** (500 ms fast, default;
2000 ms precise) and an optional **rail floor** (default 3000 mV; **0 disables**
the 3.0 V collapse stop so the lab can find the absolute limit). It is a
**blocking** run: the control link, the web server and the mode renderer are not
serviced until it ends. The console builds this line from the Run test controls;
nothing has to be typed. Requirements and sequence:

- the radio must be off (`@RADIO off` first); the command is refused otherwise;
- the test uses the **live LED count** from the shared page (`leds`), so changing
  it between runs needs no reflash;
- the strip blanks for 5 s — that is the window to unplug USB and test the
  battery; the ramp continues without the cable;
- then a probe of up to 20 LEDs (scaled so it never exceeds the selected ceiling)
  estimates the source resistance, and the selected channels ramp from
  brightness 0 to 255 in +5 steps at the selected cadence;
- it stops at the selected ceiling, at brightness 255 on a strip too short to
  reach it, on a brown-out, on a rail collapse below 3.0 V, on a GPIO9 press, or
  after 180 s.

**2 A is the continuous benchmark.** 3–6 A exist to force a collapse in
the lab, in 500 ms bursts; they are not the normal budget and must not be used
for sustained operation.

The result is written to NVS and appears in every `state` document as
`stress_result`, `stress_phase`, `stress_color` (the mask), `stress_leds`,
`stress_brightness`, `stress_ceiling_ma`, `stress_step_ms`, `stress_rail_floor_mv`,
`stress_est_ma`,
`stress_v_start`, `stress_v_min`, `stress_sag_mv`, `stress_v_off`,
`stress_v_on`, `stress_r_mohm` and `stress_elapsed_ms`. A finished run pushes
one state frame if USB is still attached; after a battery run the reconnect
resets the C3, and the state is re-read from NVS, so the result is not tied to
a reply that may be lost. `stress_run:true` means a run was interrupted before
it could store a result.

A brown-out also arms **safe mode**: the next boot shows a red/yellow warning on
the status pixel, fills the strip soft red, and waits for a click on either
switch (or 5 s, after which it continues without one — the timeout exists so a
blocking warning can never lock the USB console out). The strip then runs at 70%
of the configured brightness until `@SAFE OFF` or a five-click power-off clears
the flag.

Both the stress test and the sense run **blank the capacitive touch pad for 1 s**
after the test ends. A 3 A / 6 A burst can couple a false press into the module;
the pad is discarded, including pending clicks, for that second, so the mode
cannot change behind the user's back when the test finishes. GPIO9 (the BOOT
switch) stays live for an abort or recovery.

### 3.10 Channel resistance sense

`@SENSE 20 200` lights 20 white LEDs at full brightness for a 200 ms burst, after
the same 5 s cable-removal window as the stress test, and reports V_REG blank and
lit, the model current (60 mA per white LED) and the source resistance. It is a
separate blocking run, decoupled from the stress ramp, so a 1/10/20/50/100 LED
linearity sweep can be run quickly. The result is stored as `sense_*` in the
state and the last result in NVS; a USB-attached run pushes one state frame.

- count 1–100 LEDs, burst 200 ms (default) or 2000 ms;
- 100 white LEDs is ~6 A: the PMOS is rated **3.6 A continuous / 5 A max /
  20 A pulses**, so the 2 s / 100-LED run is lab-only; use 200 ms for the sweep;
- the current is the firmware model, not a measurement. R drifting with count is
  the cell/model linearity, exactly what the sweep is for.

---

## 4. Configuration fields

The 40 names are the `/api/config` argument names; 39 of them are also the keys of
the preset struct. They are declared once in `src/services/presets.h`
(`PRESET_FIELDS`), and the browser copies them in `console/js/spec.js`; a test
asserts the two lists match. `budget` is a config field but deliberately not a
preset field: it describes the cell/cable/strip, not a look.

```
mode
red green blue brightness budget
whip_speed whip_length whip_period whip_peak whip_offset
rainbow_speed rainbow_min_ms rainbow_max_ms rainbow_level
leds
flame_decay flame_spatial flame_power_slope flame_power_max flame_power_idle
fade_decay fade_trigger fade_length_gain fade_length_z_gain fade_length_base
fade_power_gain fade_power_base fade_color_gain fade_idle_length fade_idle_power
step_baseline_tau_ms step_threshold step_release step_jerk_min step_min_gap_ms
step_impact_min step_impact_max step_magnitude_min step_magnitude_max
```

`GET` returns these plus the derived values and the limits the UI needs. The
limits are the firmware's own constants, exported so a host does not have to
keep a second copy:

`whip_speed_min/max`, `whip_length_min`, `whip_period_min/max`,
`rainbow_speed_min/max`, `rainbow_gap_floor_ms`, `rainbow_gap_ceiling_ms`,
`flame_decay_min/max`, `flame_spatial_min/max`, `flame_power_slope_max`,
`step_baseline_tau_min_ms/max`, `step_threshold_min/max`, `step_release_min`,
`step_jerk_floor/max`, `step_gap_min_ms/max`, `step_impact_min_floor`,
`step_impact_max_ceiling`, `strip_leds_min`, `num_leds`, `budget_min/max`,
`budget_brightness`, `guard_scale`, `guard_active`, `guard_floor_mv`,
`guard_recover_mv`, `power_suggested_ma`, plus `dirty` and `portal`.

The device clamps every write; a host that sends a value outside the range still
gets a successful ack with the clamped value in `changed`.

---

## 5. Errors

An error is a normal ack with `ok:false`:

```json
@{"t":"ack","v":1,"ok":false,"cmd":"set","seq":7,"err":"unknown field 'widht'"}
```

The connection stays open. Typical messages: `unknown field '…'`,
`value for '…' is not a number`, `unknown mode id`, `expected key=value`,
`no fields`, `line too long`, `slot out of range`, `nothing to apply`,
`slot holds a preset from another firmware version`, `write failed`,
`unknown command`.

A host should render `err` verbatim; it is written for a person.

---

## 6. Implementation notes

* **Buffers.** `main.cpp` raises the HWCDC RX and TX rings to 4 KB before
  `Serial.begin()`; the default RX queue is 256 bytes and a full `SET` would not
  fit. The TX timeout is lowered from 100 ms to 10 ms so a host that stops
  reading cannot hold the LED loop up for longer than that.
* **Loop budget.** `usb_control_loop()` reads at most 256 bytes and executes at
  most 8 completed lines per pass. A pipelined host cannot starve the strip; the
  rest stays in the CDC queue for the next pass.
* **Portal window.** `run_config_portal()` calls `usb_control_loop()` and
  `update_strip(mode)` on every pass, so the console and the lamp work while the
  board is inside the 60-second hotspot. `RADIO` is refused there.
* **Reset.** `@REBOOT` sends the ack, flushes the CDC ring, waits 100 ms and
  restarts. The port re-enumerates; a host must reconnect.
* **Opening the port resets the C3.** The USB Serial/JTAG resets when the host
  asserts DTR/RTS (`rst:0x15 USB_UART_CHIP_RESET`). A host should expect the
  boot logs, wait for the firmware, and retry `@PING` for several seconds rather
  than sending one request into the reset. Do not toggle the control lines after
  opening: measured on the bench board, low/low resets again and low/high enters
  download mode.
* **Version.** `PING` reports `proto`; `state`/`modes`/`presets` carry `v`. Add
  fields without bumping; bump only for a breaking change.

---

## 7. Trying it without the browser

Any serial terminal works. Close the browser console first — one process owns
the port.

```
@PING
@GET
@SET brightness=80&mode=1&seq=1
@COMMIT
@STREAM 5
```

You should see the pong, a state document, an ack with `changed`, a commit ack,
and one tick every 200 ms interleaved with the normal `[ADC]`/telemetry log
lines. The `@` prefix is how a host tells them apart.

---

## 8. Compatibility

The HTTP API and this protocol share the same state builder, the same config
clamp function and the same preset store. They cannot disagree about a limit, a
field name or what a preset contains. The one intentional difference is
persistence: `POST /api/config` saves immediately, `SET` is transient and
`COMMIT` persists.

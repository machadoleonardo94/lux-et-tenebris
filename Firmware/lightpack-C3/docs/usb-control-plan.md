# USB control link — design and implementation plan

> **Status: implemented.** The firmware protocol is in
> `src/services/usb_control.h` and the zero-install console is in
> [`../console/`](../console/). This document is kept as the design record: the
> wire contract is now [`usb-control.md`](usb-control.md), and §6's PySide6
> comparison is why the browser page won.
>
> **Decided (2026-11-13):** the host is the **browser console**, not the PySide6
> app §6 originally recommended. Persistence is transient `SET` + debounced
> `COMMIT`. Radio on/off is in v1.

**Problem.** The V2 board's ceramic antenna makes the browser page unreliable in
the places the lamp is actually tuned (bench, case fitted, battery, radio off).
Configuration is currently web-only: `webpage_handle_set_config()`
(`src/services/webpage.h:593`) is the only HTTP writer, and
`webpage_config_load()` (`:102`) is the only reader of the live configuration. A
USB link removes the RF dependency entirely — it works with the radio switched
off, which the web page by definition cannot.

---

## 1. What exists today, and what can be reused

| Fact | Where | Consequence for this plan |
|---|---|---|
| `Serial` is the C3's **USB Serial/JTAG** (`HWCDC`), because `ARDUINO_USB_CDC_ON_BOOT=1` and `ARDUINO_USB_MODE=1` | `platformio.ini:35-36` | One CDC-ACM port, no driver on Win10+, no second UART needed. It is also the console. |
| The console is human-readable text only; **nothing reads Serial input** | `src/services/*` | The command parser is new code, not a replacement. |
| The 28 config fields already have one vocabulary, one clamping path and one preset table | `src/services/presets.h:104`, `webpage.h:593` | A USB writer must call the *same* clamps, not a copy. This is the central refactor. |
| `/api/state` is a ~2.6 KB JSON document built with `String` | `webpage.h:275` | Reuse it for "read everything", but not on every slider tick. |
| The web page writes config on `change` (slider release), not `input` | `webpage.h:2253` | A drag costs one NVS write; the desktop app should behave the same. |
| NVS key writes are compared before writing, so repeating a value costs no flash | `webpage.h:183-242` | Save-on-change is survivable, but transient-apply + commit is still better for latency. |
| App partition is 2,031,616 B (`0x1F0000`); last build was 1,272,512 B | `partitions_lightpack.csv:41`, `.pio/build/C3-USB/firmware.bin` | ~759 KB headroom. Flash is not a constraint any more; the old "97% full" comments are stale. |
| `HWCDC::write(buffer, size)` takes a mutex for the whole call | `cores/esp32/HWCDC.cpp:411-480` | One `Serial.write()` per protocol line makes a line atomic against log prints from other tasks. |
| HWCDC RX queue defaults to 256 bytes; `begin()` only sets that default if no queue exists | `cores/esp32/HWCDC.cpp:317-322`, `:531` | `setRxBufferSize(4096)` **before** `begin()` is honoured and is required for batch lines. |
| HWCDC write timeout defaults to 100 ms | `cores/esp32/HWCDC.cpp:81` | Set it lower (10 ms) so a stalled host cannot hold up the LED loop. |
| `[ADC]` and the telemetry block print every 10 s | `readings.h:57-77`, `serial_outputs.h:114` | Log volume is low; the host can keep logs in a separate pane instead of muting them. |

The mode registry (`led_scripts.h:1113`) and the preset service
(`presets.h:239-344`) are callable from a new service without changing their
semantics. `webpage_config_save()` persists the *live* globals, so a USB
"commit" is one call.

---

## 2. Transport choice

**USB CDC-ACM over the existing USB Serial/JTAG peripheral.** Not a new USB
class, not a second interface.

| Option | Why not |
|---|---|
| HID | Driverless, but 64-byte reports, no stream semantics, and it cannot coexist with the console on the same endpoint without a composite device. |
| WebUSB / custom CDC | Needs custom descriptors and a WinUSB/libusb driver story on Windows; the browser flasher already depends on the standard CDC enumeration. |
| Composite CDC (console + control) | Requires moving from `HWCDC` to TinyUSB (`ARDUINO_USB_MODE=0`), changing the USB stack under a board whose recovery path depends on the ROM Serial/JTAG. Not worth it for a bench tool. |
| Second UART + USB-serial adapter | More hardware, more pins, no benefit. |

Consequences to design around:

1. **One port is shared with the console.** The protocol therefore lives *inside*
   the text stream and is distinguished by a prefix (§3), rather than assuming a
   clean channel.
2. **The port re-enumerates on reset.** The desktop app must reconnect on
   `ResourceError`/port disappearance, and `SET` must never reset the board.
3. **Baud rate is virtual.** Open at 115200 for convention; the link is USB full
   speed. Real capacity is hundreds of KB/s, so the protocol can stay textual.
4. **Only one process can own the port on Windows.** The app must fail with a
   clear "port busy" message when `pio device monitor` or esptool has it.

---

## 3. Framing: why lines, and why a prefix

The link carries three kinds of traffic:

* unsolicited human logs (boot report, WiFi events, `[MODE]`, `[ADC]`, …);
* solicited replies (ack, state);
* unsolicited machine events and telemetry ticks.

Mixing them is the only real protocol risk. The rule:

* **Every protocol line starts with `@` and ends with `\n`.** Nothing else in
  the firmware starts a line with `@` (logs start with `[`, `\n`, `->|` or a
  digit), so a host can classify a line with one byte.
* **Device → host frames are `@` + compact JSON + `\n`.**
* **Host → device commands are `@` + verb + arguments + `\n`.**
* A line is written with a single `Serial.write()`; HWCDC's tx mutex makes that
  atomic with respect to other tasks' prints. A log line can appear before or
  after a frame, never inside it.
* The host ignores every line that does not start with `@`, and shows those in a
  "device log" pane. This turns the console constraint into a diagnostic feature:
  the boot report, RSSI and WiFi disconnect reasons are visible over USB even
  when the radio cannot hold a connection.
* Maximum line length is 1024 bytes. Oversize input is discarded to the next
  newline and answered with an error; oversize output is impossible by
  construction (state is ~2.6 KB, but it is only sent as an explicit reply).

Why not COBS/SLIP + binary + CRC? At 20–50 Hz and ~200 B/tick the text stream uses
well under 1 % of the link, and it is readable in a terminal, diffable in a log
file, and robust enough with line framing. Binary framing only pays for raw IMU at
hundreds of Hz or a noisy RF link; neither applies here. If a binary mode is ever
needed, add it as an opt-in `@BIN` command and keep JSON for configuration.

---

## 4. Protocol v1

### 4.1 Commands (host → device)

| Command | Meaning | Reply |
|---|---|---|
| `@PING` | Identify the board | `pong` with firmware/build/chip/MAC/proto |
| `@GET` | Read the whole live configuration | `state` (the `/api/state` document) |
| `@MODES` | List the mode registry | `modes` array |
| `@SET seq=N k=v&k=v…` | Apply a batch of fields in RAM, then sanitise | `ack` + `changed` subset |
| `@COMMIT` | Persist the live configuration to NVS | `ack`, `dirty:false` |
| `@RELOAD` | Discard RAM edits, reload NVS | `ack` + full `state` |
| `@STREAM hz` | Telemetry ticks at `hz` (0–50, 0 stops) | `ack` |
| `@PRESET LIST \| GET n \| SAVE n [name=…] \| APPLY n \| CLEAR n` | Slot operations | `presets` / `preset` / `ack` |
| `@IMPACT mag` | Inject a preview impact (0–255) for flame tuning | `ack` |
| `@RADIO on\|off\|toggle` | Switch the WiFi radio, USB unaffected | `ack` + `net` |
| `@REBOOT` | Acknowledge, flush, restart | `ack` then re-enumeration |

Notes:

* `SET` is **strict and atomic**: every key must be one of the 28 in
  `PRESET_FIELDS`, every value must parse as a number, and a mode id must exist.
  If any pair is invalid, *nothing* is applied and the ack names the offending
  token. This is what makes a batch trustworthy.
* `SET` is **transient**: it changes RAM and the strip immediately, but writes no
  flash. `COMMIT` is the only USB path that writes NVS. This mirrors the web
  page's "one write per slider release" while allowing the app to send edits at
  UI rate (debounced into one line) without flash stalls.
* `seq` is optional and echoed, so the host can match replies to requests when it
  pipelines. It is not a config field and is stripped before validation.
* Cross-field sanitisers can change fields that were not in the request
  (`leds` shrinks `whip_length`; `step_sanitize()` pulls `release` under
  `threshold`). The `changed` object reports every field whose value actually
  moved, not just the requested ones.

### 4.2 Frames (device → host)

```json
@{"t":"pong","v":1,"proto":1,"fw":"0.4-pre","build":"Nov 13 2026 22:00:00","chip":"ESP32-C3","mac":"AA:BB:CC:DD:EE:FF"}
@{"t":"ack","v":1,"ok":true,"cmd":"set","seq":7,"dirty":true,"n":11,"changed":{"mode":5,"whip_length":24}}
@{"t":"ack","v":1,"ok":false,"cmd":"set","seq":7,"err":"unknown field 'widht'"}
@{"t":"state","v":1,"state":{ …the /api/state object… }}
@{"t":"tick","v":1,"ms":123456,"mode":2,"ax":-0.12,"ay":0.03,"az":9.81,
  "gx":0.1,"gy":-0.2,"gz":0.0,"imp":12,"ic":3,"ia":850,"lp":19,
  "rail":4012,"heap":123456,"net":0,"dirty":true,"portal":false,"btn":0}
@{"t":"evt","v":1,"k":"mode","id":5,"key":"whip","label":"Pulse whip"}
@{"t":"evt","v":1,"k":"impact","mag":12,"count":3,"ms":123456}
@{"t":"evt","v":1,"k":"button","b":"touch","e":"short","n":2}
```

* `tick` is a **fixed `snprintf` buffer** (~190 B), never a `String`; it is the
  only high-rate frame and must not churn the heap.
* `state` is built by the existing `/api/state` code, refactored to write into a
  `String &`. It is sent on `GET`, `RELOAD` and preset apply only. It is
  deliberately **not** the reply to every `SET`: building it calls
  `wifi_saved_ssid()`, which opens NVS, and a drag would pay for that on every
  tick. The `changed` object carries the same post-sanitise values for only the
  fields that moved.
* `changed` values use the same precision as `state`: integers for `uint8_t` /
  `uint16_t`, three decimals for `float`.
* Events are emitted whenever a host is attached, even with `STREAM 0`, so mode
  changes from the pad, impacts and switch gestures are visible without
  telemetry.
* `imp`/`ic`/`ia` read the detector's **diagnostic** globals
  (`impact_last_magnitude`, `impact_count`, `impact_last_ms`), never
  `impact_take()` — the USB link must not consume an impact the flame mode is
  waiting for.

### 4.3 Batch semantics

One line can carry the whole configuration:

```
@SET mode=5&red=255&green=140&blue=0&brightness=180&leds=60&whip_speed=4.5&whip_length=24&whip_period=1200&whip_peak=200&whip_offset=0&seq=12
```

That is one parse, one sanitise pass, one `changed` diff and one reply — the
"send a batch" primitive. The desktop UI accumulates changes made within a 100 ms
window into exactly this line, so a drag and a mode click that land together are
one atomic edit.

Presets are the batch unit at the configuration level: `@PRESET SAVE 2 name=bench`
captures the live config; `@PRESET GET 2` returns the same JSON shape the web
page's export file uses, so the desktop app and the browser can exchange files.

### 4.4 State additions

`webpage_state_json()` gains:

* `"dirty":true|false` — RAM differs from NVS (also useful to the web page, which
  can ignore it);
* `"portal":true|false` — the provisioning hotspot is open, which is why the
  board can report "offline" while its radio is on;
* the range constants the app cannot otherwise know, so the host never invents
  a limit: `whip_speed_min/max`, `whip_length_min`, `whip_period_min/max`,
  `flame_decay_min`, `flame_spatial_min`, `step_jerk_floor`;
* nothing else changes. The page's `render()` ignores unknown fields.

---

## 5. Firmware changes

### 5.1 Files

| File | Change |
|---|---|
| `src/services/usb_control.h` | **New.** Line RX buffer, command parser/dispatch, JSON emitters, tick/event pacing, `usb_control_begin()` / `usb_control_loop()`. |
| `src/services/webpage.h` | Extract `webpage_config_apply_arg(name, value)` from the 28 clamp blocks; extract `webpage_state_json(String &)` from `webpage_handle_state()`; add `dirty` + range constants. HTTP behaviour must be byte-identical apart from added fields. |
| `src/services/presets.h` | Extract `preset_capture(struct_preset &)` from `preset_write()`; reuse it for the USB before/after diff. |
| `src/services/dependencies.h` | Include `usb_control.h` **after** `presets.h` — it needs the field table, the config writer and the preset service. |
| `src/main.cpp` | `Serial.setRxBufferSize(4096)` / `setTxBufferSize(4096)` before `begin`; `setTxTimeoutMs(10)` after; call `usb_control_loop()` from `loop()`. |
| `src/shared/variables.h` | `bool config_dirty = false;` (written by USB/preset paths, read by state). |
| `platformio.ini` | Optional `-D LIGHTPACK_FW_VERSION=\"0.4-pre\"` for `PING`. |
| `docs/usb-control.md` | Protocol reference after implementation (this plan is the design record). |
| `README.md`, `CHANGELOG.md` | A "USB console" section and an Unreleased entry. |

### 5.2 Refactors that must not change behaviour

The web handler is 315 lines of `hasArg()` blocks. The extraction must keep every
constant and clamp exactly as it is today:

```cpp
// webpage.h
bool webpage_config_apply_arg(const char *name, const String &value);

void webpage_handle_set_config()
{
  bool changed = false;
  for (int i = 0; i < web_server.args(); i++)
    changed |= webpage_config_apply_arg(web_server.argName(i).c_str(), web_server.arg(i));
  if (!changed) { /* same 400 as today */ }
  strip_sanitize(); step_sanitize(); whip_sanitize();
  flame_sanitize(); rainbow_sanitize();
  webpage_config_save();
  // same serial proof lines, then webpage_handle_state()
}
```

`web_server.args()/argName(i)/arg(i)` exist in this core (`WebServer.h:117-120`)
and preserve the order the browser sent.

The state extraction is mechanical:

```cpp
void webpage_state_json(String &json);   // reserve + every append, no trailing '\n'
void webpage_handle_state() { String j; webpage_state_json(j); web_server.send(200, "application/json", j); }
```

### 5.3 Loop integration and timing

```cpp
void loop()
{
  …
  webpage_loop();
  usb_control_loop();   // after the network: commands can call wifi/RADIO and presets
  serial_outputs();
}
```

`usb_control_loop()` must be bounded, because `loop()` also drives the LEDs:
read at most ~256 bytes and execute at most 2 complete commands per pass. At the
current ~8 ms frame (250 NeoPixels + `show()`), that is still tens of KB/s of
command throughput, and no host can starve the strip. Ticks are paced by
`millis()`, not by loop count, and report the achieved loop rate so a slow
telemetry consumer is visible rather than silent.

### 5.4 Buffer budget

| Item | Bytes |
|---|---|
| RX queue (`xQueueCreate(n, 1)`) | 4096 |
| TX ring buffer | 4096 |
| Line buffer | 1025 |
| Tick scratch (`char`) | 256 |
| State `String` (on demand) | ~2800 transient |
| Snapshot diff (`struct_preset` × 2) | 192 stack |

Against ~300 KB of heap, this is comfortable. The RX/TX sizes are the important
ones: batch lines are ~600–900 B, and the default 256 B queue would drop them.

### 5.5 Commands that are deliberately absent

* **Power-off / latch cut.** Over USB the latch opens but the board stays alive
  on the cable and restarts; exposing it would be a confusing button, not a
  useful one.
* **Firmware upload.** esptool and the browser installer already own that path;
  a second flasher in the app is a recovery risk.
* **WiFi credential provisioning.** The radio's own portal remains the way to
  store networks; `@RADIO` only switches what is already stored.
* **Touch-pad emulation.** Inputs stay physical; the app observes events.

---

## 6. Host app: zero-install browser console

### 6.1 Platform comparison

| Option | Install | Fit | Verdict |
|---|---|---|---|
| **Browser page + Web Serial API** | Nothing but Chrome/Edge desktop; served from GitHub Pages or a localhost static server (`file://` is blocked, as `install/README.md:21` documents) | Best-looking UI for the least code; JSON native; no build step; the page is the deliverable | **Chosen** — zero-install was the deciding constraint |
| Python + PySide6 (Qt Widgets + `QtSerialPort`) | `pip install PySide6` (~300 MB); 6.11.1 was already installed on the bench machine | Native buttons/toggles/spin boxes, layouts, tabs, tables; serial port built in; easy to package | Rejected only because it needs an install and a Python process |
| Python + Tkinter (stdlib) + pyserial | Zero UI dependency; pyserial 3.5 installed | Adequate `ttk` forms; more layout code; weaker high-DPI and numeric widgets | Second desktop fallback |
| Tauri / Electron | Rust/Node toolchain, serial plugin/native module | Web UI with a native shell; build complexity for no functional gain here | No |
| .NET WPF / WinUI | .NET SDK, Windows-only | Good native UI; locks the tool to Windows | No |

Web Serial is Chromium-only: Chrome/Edge 89+ on a desktop, over `https://` or
`localhost`. Firefox, Safari and every mobile browser lack the API. The page
detects that and explains it instead of showing an inert Connect button.

The PySide6 app in the Echoes repository was the visual reference: dark
bench-instrument palette, compact toolbar, panels. The console keeps that
language (and the no-animation rule) while implementing it in plain HTML/CSS.

### 6.2 Structure

```
Firmware/lightpack-C3/console/
  index.html                    page and layout
  styles.css                    dark theme, no transitions
  js/spec.js                    key -> label/page/range/step/unit
  js/protocol.js                framing, commands, parser (DOM-free)
  js/serial.js                  Web Serial transport, ordered writes, reconnect
  js/app.js                     handshake, batching, UI, presets, logs
  tests/protocol.test.mjs       field-list + framing + command tests
  package.json                  ESM marker and `npm test`; no dependencies
```

`protocol.js` and `spec.js` are DOM-free, so the Node tests and the page share
one implementation of the wire format.

### 6.3 UI

One page, no animations, native form controls only. The layout is the sketch
below: mode bar on top, parameter pages down the side, live numbers and the
device log on the right.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ Port [COM6 ▾] [Refresh] [Connect]   ● connected  fw 0.4-pre   [x] Stream 20Hz│
├───────────────────────┬──────────────────────────────────────────────────────┤
│ Shared                │ Live                                                 │
│  Mode  [Solid][Flame] │  accel  -0.12  0.03  9.81 m/s²                       │
│        [Fade][Whip]   │  gyro    0.10 -0.20  0.00 °/s                        │
│        [Rainbow]      │  impact  12 (3 total, 0.85 s ago)                    │
│  Palette  [color]     │  loop 19 Hz  rail 4012 mV  heap 123 KB  net offline  │
│  R[255] G[0] B[255]   │  btn  touch up · boot up                             │
│  Brightness [180]     ├──────────────────────────────────────────────────────┤
│  LED count  [60]      │ Events / device log                                  │
│                       │  22:31:02  mode -> whip (5)                          │
│ Step flame            │  22:31:05  [ADC] Reading: 2048, Voltage: 4012 mV     │
│  Decay      [0.900]   │  22:31:07  [WIFI-EVT] disconnected: reason=…         │
│  Spatial    [2.0]     │                                                      │
│  …                    │                                                      │
│ ───────────────────── │                                                      │
│ [Save to flash]       │                                                      │
│ [Revert]   ● unsaved  │                                                      │
└───────────────────────┴──────────────────────────────────────────────────────┘
```

* **Left sidebar:** `Shared`, `Step flame`, `Impact detector`, `Pulse whip`,
  `Rainbow flow`, `Gyro fade` (no tunables — explains why), `Presets`,
  `Diagnostics`. This is the page's long scroll split by the mode that consumes
  each parameter, which is what "fine tune the modes" actually means.
* **Mode:** a row of exclusive toggle buttons built from `@MODES`; no "next/prev"
  needed on a desktop.
* **Each parameter:** label + `type="number"` input (the requested text box) with
  the unit in the range hint, and range/step from `spec.js` / the device's
  exported limits. Keyboard arrows give fine steps. No custom drawing, no
  transitions.
* **Toggles:** `Live`, `Auto-save`, `Show` device log. These are
  real switches, not decoration.
* **Presets:** five rows from `@PRESET LIST`, each with its name/state and
  Apply/Save/Clear; Export/Import reuses the web page's JSON file shape
  (`{"slot":n,"name":…,"config":{…}}`), so a preset can move between the browser
  and the app unchanged.
* **Live pane:** the tick fields, updated at the chosen rate. Numeric labels,
  no graphs.
* **Bottom bar:** port, firmware/build, dirty dot, last error. Errors are
  persistent lines, not transient toasts.
* **Test impact:** a magnitude spin box + button on the flame page, sending
  `@IMPACT` — tune the flame response without hitting the device.

### 6.4 State model and batching

* On connect: `PING` → `MODES` → `GET` → `PRESET LIST`.
* A control edit goes into a pending dict; a 100 ms timer flushes it as one
  `@SET` with a `seq`.
* The ack's `changed` object is merged into the model; a field the user is
  currently typing into is not stolen back unless the device clamped it.
* `Auto-commit` sends `@COMMIT` after ~0.9 s of quiet; default on, so tuning
  survives a reboot without hammering NVS. The dirty badge is the device's own
  flag. Disconnecting commits once if dirty; closing the tab while dirty warns.
* Reconnect: on port error, close the old link, retry the granted port every
  ~1.2 s if the user did not disconnect deliberately, then re-run the handshake.

### 6.5 CLI

No separate CLI in v1. The zero-install page is the deliverable, and the Node
protocol tests plus `?demo` cover the host side without one. A command-line
client remains an easy addition on top of `protocol.js` if batch scripts are
wanted later.

---

## 7. Test plan

**Host, no board**

1. `console/tests/protocol.test.mjs`: field list against the firmware table,
   command builders, chunked/CRLF framing, log/frame/malformed classification,
   config filtering and preset-file round-trip.
2. `?demo` in Chrome/Edge: every page renders with a representative state and no
   link; useful for screenshots and layout changes.
3. The page's own browser gate: no Web Serial, `file://`, non-secure context.

**Bench, with the board**

| # | Case | Expected |
|---|---|---|
| 1 | Open the port, `PING` | `pong`, firmware/build/MAC, no reset |
| 2 | `GET` | state matches the web page's `/api/state` |
| 3 | `SET brightness=128` | strip dims; ack `changed.brightness=128`; NVS untouched |
| 4 | `SET leds=999` | `changed.leds=250` (clamped, reported) |
| 5 | `SET leds=40&whip_length=90` | `changed.whip_length=40` (cross-field) |
| 6 | `SET step_threshold=0.1` | `release` pulled down; both in `changed` |
| 7 | `SET widht=1` | error, no field changes |
| 8 | `SET brightness=abc` | error, no field changes |
| 9 | 28-field `SET` in one line | one ack, all fields applied |
| 10 | `SET` then power cycle without `COMMIT` | old values; app shows dirty before the cycle |
| 11 | `SET` then `COMMIT` then power cycle | new values |
| 12 | `PRESET SAVE/APPLY/GET/LIST/CLEAR` | same as the web page; export/import round-trips |
| 13 | `STREAM 20` for 60 s | ~20 Hz ticks, loop rate unchanged, no corruption |
| 14 | Force a 10 s telemetry block while streaming | log lines land between frames, never inside |
| 15 | Unplug while streaming, replug | app reconnects, handshake repeats, no stale state |
| 16 | `RADIO off`, then edit | USB still works, `net:"offline"` |
| 17 | Pad mode change while app open | `evt.mode` arrives, buttons follow |
| 18 | `IMPACT 24` in flame mode | one flash at that magnitude, detector counters unchanged |
| 19 | Port held by `pio device monitor` | clear "port busy" error, no crash |
| 20 | Flash/RAM after the change | app image still well inside `0x1F0000` |

---

## 8. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Log text corrupts a frame | `@` prefix, one `write()` per frame, host ignores non-`@` lines; verified against HWCDC's tx mutex |
| CDC re-enumeration on reset | auto-reconnect and full re-handshake; `SET` never resets |
| The web config path changes behaviour | extraction only; diff the 28 clamp blocks against the original and run the web page's own tests manually |
| NVS wear / stalls while dragging | transient `SET`; `COMMIT` debounced; NVS already skips unchanged writes |
| RX overrun on batch lines | 4 KB RX queue + line cap + read budget per loop |
| `String` heap churn | state only on demand; ticks in a fixed buffer |
| Host stalls while the device writes | `setTxTimeoutMs(10)`, tick size bounded, `availableForWrite` check before big state frames (optional) |
| Protocol drift between app and firmware | `proto`/`v` in every frame; `PING` handshake; schema/limits exported in state |

---

## 9. Non-goals

* flashing firmware from the app;
* provisioning WiFi credentials over USB (radio portal stays);
* a binary/raw streaming mode (revisit only for >200 Hz IMU);
* graphs or animation — the live pane is numbers;
* changing how the device behaves without a host.

---

## 10. Implementation order

1. **Firmware contract** — refactors (`webpage_config_apply_arg`,
   `webpage_state_json`, `preset_capture`), `usb_control.h`, `main.cpp` wiring.
   Testable with any serial terminal.
2. **Browser console** — `spec.js`, `protocol.js`, `serial.js`, `app.js`; the
   Node tests pin the wire format and field list.
3. **Docs** — `docs/usb-control.md` (wire reference), `console/README.md`,
   README section, CHANGELOG.

The firmware half is useful on its own — a scriptable board over a serial
terminal — and the page is then mostly presentation.

---

## 11. Decisions

| # | Question | Decision |
|---|---|---|
| 1 | Host platform | **Zero-install browser page, Web Serial, Chrome/Edge desktop**; PySide6 kept only as the visual reference from Echoes |
| 2 | Flash persistence | **Transient `SET` + debounced `COMMIT`**; the page auto-saves after 0.9 s of quiet |
| 3 | Radio control in v1 | **Included**, with a toggle on Diagnostics; refused while the config hotspot is open |
| 4 | Preset file format | **The web page's JSON**, so both tools read the same files |
| 5 | Code location | **`console/`** inside the firmware folder, beside the firmware it speaks to |

# Button interface — operating summary

A self-contained description of the two-button interface, written to be ported to
another firmware. It states the wiring, the gesture recognition rules, and what each
gesture does; implementation details that only matter on this board are marked as such.

Source of truth: `src/services/buttons.h` (gestures) and `src/services/controls.h`
(actions). Pin numbers come from `src/shared/pinout.h`.

**At a glance** — the shipped map:

```
PROG (GPIO0)       click play/pause    hold+repeat vol-    very long restart song
EXT  (GPIO4/19)    click play-if-idle or next               double LED animation
                   triple switch mode (reboots)             hold+repeat vol+
                   very long restore WiFi
```

No chord, no plain-hold reboot.

---

## 1. Wiring

Two momentary buttons, one per function group. Both are read as ordinary GPIO levels;
no interrupts, no ADC, no expander.

| name | pin | idle | press | pull |
| --- | --- | --- | --- | --- |
| `PROG` | GPIO0 (both boards) | HIGH | shorts to GND (active LOW) | internal pull-up |
| `EXT` | GPIO4 (Echoes V1) / GPIO19 (Marajoara V3) | LOW | feeds 3V3 (active HIGH) | internal pull-down |

Configure the pin's internal pull in the direction opposite to its active level
(`INPUT_PULLUP` for PROG, `INPUT_PULLDOWN` for EXT). EXT has a board pull-down as well;
the firmware's own pull makes it independent of that resistor.

**Poll, do not use an edge interrupt.** On this board GPIO0 is the boot-strapping pin
and shares a node with the USB-serial adapter's auto-reset RC network; it idles on a
weak (~45 k) internal pull-up. The LED strip, the I2S clocks and the amplifier inject
microsecond-scale glitches onto that node, and an edge ISR turns every one into a
phantom press. One `digitalRead()` per button per main-loop pass, with a 25 ms
stability window, is both sufficient and immune. If the target firmware can afford an
ISR, it still needs the same debounce and should ignore edges shorter than
`MIN_PRESS`.

Boot-strap caveat: holding PROG down while the chip resets puts the ESP32 into the ROM
loader instead of the application. That is a property of GPIO0, not of this firmware;
nothing in software can prevent it.

---

## 2. Timing constants

All values in milliseconds; all are compile-time constants today and safe to retune.

| constant | value | meaning |
| --- | --- | --- |
| `DEBOUNCE_MS` | 25 | a level must hold this long before it is believed |
| `MIN_PRESS_MS` | 50 | any press shorter than this is electrical noise, discarded |
| `HOLD_MS` | 600 | a press still down at this age becomes a HOLD |
| `REPEAT_MS` | 150 | auto-repeat period after HOLD |
| `VERY_LONG_MS` | 4000 | a hold still down at this age becomes VERY_LONG |
| `MULTI_GAP_MS` | 400 | max gap between taps of one multi-click burst |
| `MAX_CLICKS` | 3 | a 4th tap in a burst stays a TRIPLE |

Measured human taps on this hardware run 96–190 ms, so `MIN_PRESS` has a 2–4× margin
and never approaches `HOLD_MS`.

---

## 3. Gesture recognition

Each button owns an independent state machine instance. Events emitted:

| event | when |
| --- | --- |
| `CLICK` | 1 tap: on release for a zero-window button, otherwise when the burst window closes |
| `DOUBLE` | 2 taps in one burst |
| `TRIPLE` | 3 (or more) taps in one burst |
| `HOLD` | press reached `HOLD_MS` while still down; fires once |
| `REPEAT` | still down, every `REPEAT_MS` after HOLD |
| `RELEASE` | button came up after a HOLD |
| `VERY_LONG` | press reached `VERY_LONG_MS`; fires once, and stops REPEATs |

Every event carries the press duration in ms.

### 3.1 Rules, in decision order

1. **Debounce.** A raw change only becomes the believed level after it has held for
   `DEBOUNCE_MS`. On a settled press, record its start time and clear the hold flags.
2. **Release shorter than `MIN_PRESS_MS`** is a blip: discard it and leave any open
   burst untouched (noise must not disturb a real tap in progress).
3. **Release after a HOLD** emits `RELEASE` (not a click).
4. **Release before a HOLD**, with a per-button click window of 0, emits `CLICK`
   immediately — the single-purpose button, see 3.3.
5. **Release before a HOLD, with a non-zero window**: count the tap into the open
   burst (capped at `MAX_CLICKS`) and push the burst deadline to `now + window`.
6. **While down**: at `HOLD_MS` emit `HOLD` and clear the burst count; every
   `REPEAT_MS` after that emit `REPEAT`; at `VERY_LONG_MS` emit `VERY_LONG` and stop
   the REPEAT train.
7. **When up and the burst deadline has passed**, emit `CLICK` / `DOUBLE` /
   `TRIPLE` for 1 / 2 / ≥3 counted taps and close the burst.

Consequences that fall out of these rules:

* Press-press-hold is a **HOLD**, never a DOUBLE followed by a hold — the hold
  swallows the burst it interrupted.
* A tap cannot be reported as CLICK until either the window closes or the button is
  declared single-purpose. On a laddered button this is a deliberate latency: up to
  `MULTI_GAP_MS` after the finger leaves.
* A burst is closed by time, not by the next press. Taps 273–346 ms apart are one
  burst; taps 1.5 s apart are separate clicks.

### 3.2 Reference skeleton

```c
// per button, per poll
bool raw = (digitalRead(pin) == active_level);
if (raw != b.raw) { b.raw = raw; b.raw_change_ms = now; }

if (now - b.raw_change_ms >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (raw) {                       // press begins
        b.pressed_ms = now;
        b.hold_reported = b.very_long_reported = false;
        // do NOT clear b.clicks: this press may be tap 2 or 3 of a burst
    } else {                         // release
        uint32_t held = now - b.pressed_ms;
        if (held < MIN_PRESS_MS) {
            // blip; leave the burst alone
        } else if (b.hold_reported) {
            b.last_held_ms = held;
            emit(RELEASE, held);
        } else if (window_ms == 0) {
            b.last_held_ms = held;
            emit(CLICK, held);
        } else {
            b.last_held_ms = held;
            if (b.clicks < MAX_CLICKS) b.clicks++;
            b.burst_deadline_ms = now + window_ms;
        }
    }
}

if (b.stable) {
    uint32_t held = now - b.pressed_ms;
    if (!b.hold_reported && held >= HOLD_MS) {
        b.hold_reported = true;
        b.clicks = 0;                // hold cancels the interrupted burst
        b.last_repeat_ms = now;
        emit(HOLD, held);
    } else if (b.hold_reported) {
        if (!b.very_long_reported && held >= VERY_LONG_MS) {
            b.very_long_reported = true;
            emit(VERY_LONG, held);
        } else if (!b.very_long_reported && now - b.last_repeat_ms >= REPEAT_MS) {
            b.last_repeat_ms = now;
            emit(REPEAT, held);
        }
    }
} else if (b.clicks > 0 && (int32_t)(now - b.burst_deadline_ms) >= 0) {
    uint8_t n = b.clicks; b.clicks = 0;
    emit(n >= 3 ? TRIPLE : n == 2 ? DOUBLE : CLICK, b.last_held_ms);
}
```

### 3.3 Per-button click window

The click window is a property of the button, not a global:

* `PROG` uses **0**: every tap is its own `CLICK`, emitted on release. DOUBLE and
  TRIPLE do not exist for this button.
* `EXT` uses **400**: it carries the ladder, and its clicks pay the up-to-400 ms
  confirmation delay.

This split exists so the most frequent action (play/pause on PROG) is instant, while
only the button that actually needs multi-clicks waits. A target firmware that assigns
play/pause to the laddered button will feel a ~0.4 s lag on every tap; keeping the
window per-button is what avoids that.

---

## 4. Action map (this board)

Handlers must never block: the gesture state machine is polled from the same loop that
pumps audio and the LED show. (One known exception is the Bluetooth stack's AVRCP
passthrough, which blocks ~100 ms per call; never put it inside a REPEAT ramp.)

| gesture | `PROG` | `EXT` |
| --- | --- | --- |
| CLICK | play / pause | play when idle, otherwise next track |
| DOUBLE | — (not generated) | cycle LED animation |
| TRIPLE | — (not generated) | switch operating mode (saves, then reboots) |
| HOLD → REPEAT | volume down, 5 % per tick | volume up, 5 % per tick |
| VERY_LONG | restart current track | bring WiFi back |
| RELEASE | persist volume | persist volume |

The two VERY_LONG actions undo the volume ramp the hold performed before it qualified
as "very long" — a gesture that ends in "restart" must not leave the volume at one end.

---

## 5. Mode-dependent behaviour

The device runs one of two mutually exclusive modes, selected by EXT triple-click and
persisted across reboots. The same gesture means different things per mode because the
transport differs:

| | Bluetooth mode | WiFi / standalone mode |
| --- | --- | --- |
| transport | the phone (AVRCP) | SD-card player |
| play/pause | AVRCP play/pause to the phone | local player toggle; with nothing loaded it starts a show |
| next | AVRCP next | next track in the SD playlist |
| restart (VERY_LONG) | AVRCP *previous* — the source app decides whether that restarts or steps back | re-opens the current file from 0 |
| volume backend | 0–127 A2DP sink step | 0–21 decoder step |
| radio | off | on, except while a standalone show plays |
| WiFi restore (EXT VERY_LONG) | refused (radio belongs to the A2DP stack) | re-enables the radio and the portal |

Rules worth carrying over even if the target hardware has no modes:

* **Play with nothing loaded means "start something".** In WiFi mode an idle click
  starts a show off the SD card instead of logging "nothing to pause".
* **Refuse impossible actions explicitly.** No phone connected, no card, or local
  playback requested in Bluetooth mode must return a denial (tone/strip) rather than
  half-executing and going silent.
* **A mode switch reboots.** Persist the choice, then restart; acknowledge the gesture
  before rebooting, because the device disappears for several seconds.
* **WiFi is torn down while local audio plays** on this board (no PSRAM to hold lwIP
  and the decoder together). The EXT 4 s hold is the only non-reboot way back.

---

## 6. Volume model

* One authoritative value: an integer **percent, 0–100**, persisted in NVS.
* Each REPEAT tick adds/subtracts **5 %**, clamped to 0/100. A full 0↔100 sweep is
  ~3 s of holding (20 ticks × 150 ms).
* Percentage is converted at the edge: A2DP `percent × 127 / 100`; local decoder
  `percent × 21 / 100` (or the library's nearest step).
* Persist on the **first ramp tick** of a hold and again on **RELEASE** — not on every
  tick, or a 3 s hold costs ~20 flash writes.
* Holding at an end stop should report the limit once per gesture, not once per tick.

---

## 7. Feedback (optional, but it is the whole UI)

There is no screen. In this firmware a gesture is confirmed by the LED strip (always
available) and by short tones (only when the audio path is free). A port can omit both
and still be functionally correct, but then every mode change and animation switch is
invisible.

What this firmware signals:

| gesture | feedback |
| --- | --- |
| volume tick | strip bar at the new level; tone whose **pitch** encodes the level |
| animation cycle | 1 / 2 / 3 blue blinks = animation index |
| mode switch | 3 white flashes, then reboot |
| WiFi restore | 2 green flashes |
| denied action | low denial tone |

Use blink count as well as colour, and keep the feedback non-blocking except immediately
before a reboot.

---

## 8. Porting checklist

- [ ] Two inputs, active levels as in section 1, internal pull opposite the active level.
- [ ] Poll both buttons from the main loop; 25 ms debounce; discard < 50 ms.
- [ ] Implement the seven events and the hold-swallows-burst rule.
- [ ] Make the click window per-button: 0 for the single-purpose button, 400 ms for the
      laddered one.
- [ ] Wire CLICK/DOUBLE/TRIPLE/HOLD/REPEAT/VERY_LONG per section 4.
- [ ] Add a very-long hold that undoes its own volume ramp before acting.
- [ ] Keep handlers non-blocking; ramp on REPEAT, not on a delay loop.
- [ ] Persist volume once at ramp start and once on release.
- [ ] Acknowledge a rebooting mode switch before the reboot.
- [ ] Suppress destructive actions in any button-diagnostic build, or a misread gesture
      costs the test session.

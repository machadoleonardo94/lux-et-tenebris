# lightpack-C3 pre-release plan — feature list and menu navigation

> **Status note (superseded in part).** This plan builds the on-device menu on the
> **GPIO3 touch pad**. That input is now **hardware-damaged on the bench board**: the
> pad still measures a clean 3.3 V, but the pin reads LOW and never latches an edge
> (measured both digitally and through the ADC, see `services/readings.h`). The
> cause is most likely the board's own power button **BT1**, which puts `+BATT`
> (~4.2 V) onto that 3.3 V GPIO through R12 — D6 does **not** clamp it, since its
> other end lands on the latch-gate node, not a 3.3 V rail.
>
> The implemented input map therefore lives on **GPIO9** (the on-module BOOT switch)
> instead, and is described in `services/readings.h` and `check_boot_button()`:
> 1 click next mode, 2 clicks previous mode, 5 clicks power off, 3 s hold radio
> on/off, 6 s hold config hotspot, 10 s hold recovery wipe. Sections below that
> assume a working touch pad — especially §3's menu, the double-tap timing in §3.1
> and the §6 test matrix — describe intent that is **not** what the firmware now
> does. The 2.7 V zener proposed as a field fix is also too low: the C3's VIH is
> ~2.48 V, so use 3.3 V or 3.6 V.

**Target hardware:** Lux et Portabilis **V2** (`Hardware/lux-et-portabilis-V2/`), ESP32-C3
SuperMini module.
**Target firmware:** `Firmware/lightpack-C3` (image version V0.3)
**State this plan starts from:** mode tuning and the preset values are considered good.
What is missing is a *stated* feature list and an *on-device* way to navigate it.

---

## Hardware basis

The feedback design in §3.3 rests on the status pixel, so its part and net are
recorded here rather than assumed:

| Item | Detail | Source |
|---|---|---|
| Status pixel | **D2**, `LED:WS2812B`, PLCC4 5050, LCSC C2843785, "RGB LED with integrated controller" — full RGB, addressable | `lux-et-portabilis-V2.kicad_sch` |
| Status pixel net | `Status_LED` → **GPIO10**; two global labels on that net, and the pixel is **mounted and confirmed working** | schematic + bench |
| Touch pad | **GPIO3** (`Button_GPIO`, SuperMini pad 12), declared `active_high` — so the firmware applies `INPUT_PULLDOWN` and waits for a RISING edge. **The board fits no pull resistor on this net**, which makes that internal pull the only thing defining the idle level | `lux-et-portabilis-V2.kicad_pcb`, `setup_button_interrupt()` |
| Touch connector | `TOUCH1` is a 1×03 socket ordered **pin 1 = +BATT, pin 2 = signal, pin 3 = GND** — the module runs off the raw battery rail, *not* 3.3 V | same net table |
| Touch net | GPIO3 is shared with the on-board power button **BT1** through **R12 (2.2 k)**, filtered by **C7 (100 nF→GND)**, clamped by **D6 (1N4148W)** | same net table |
| **Not** the button | **GPIO2 is `Servo_out`** on V2 (pad 11), nothing to do with the pad — probing it for a button can never work. It was left open on `lux_minimus` | same net table |
| Boot switch | GPIO9, the SuperMini's on-module BOOT switch. Its edge is now derived from `active_high`, like the touch pad's | `setup_button_interrupt()` |
| Momentary only | **The firmware assumes a momentary touch module** — a latching or toggle-wired one reads as one endless press and powers the board down at 5 s | `setup_button_interrupt()` |
| Strip / I²C | GPIO1 `Strip_out`, GPIO7/8 SDA/SCL | `shared/pinout.h` |

Two consequences:

* Because the pixel is RGB, the colour-coded signal table in §3.3 is available —
  a single-colour LED would have collapsed white/red/amber/magenta into one
  indistinguishable channel.
* Because the touch pad is assumed momentary, **double-tap is only as reliable as
  the module plugged into TOUCH1.** That assumption is already load-bearing for the
  existing 5 s power-off, so double-tap does not add a new dependency — but a
  toggle-mode module breaks both.
* **The module's polarity is unverified, and it is the one thing that can make the
  whole menu moot.** `INPUT_PULLDOWN` + RISING is correct only for a push-pull,
  active-HIGH module; an active-LOW or open-drain one drives the line *down* and
  can never produce a rising edge, so every press goes unnoticed while the hardware
  is wired perfectly. `tools/button_probe/` measures which it is
  (`pio run -e C3-PROBE -t upload`), and flipping it is **one argument** to
  `button_input_configure()` — the ISR edge now follows that same flag instead of
  being fixed at `RISING`, so the pull and the edge can no longer disagree.

---

## 1. The feature list

Two tiers, frozen for the pre-release. The tiers exist because WiFi is not always
available (no phone in hand, offline mode, a router that will not join) and the
question "what does this thing do on its own?" needs one answer.

### Tier A — on-device, no phone

| # | Feature | Status |
|---|---|---|
| A1 | **Cycle LED modes** — walk the mode registry from the touch pad | exists, becomes the menu |

That is the whole Tier A list, by decision: the pre-release menu exposes mode
selection and nothing else. See §7 for what was considered and deliberately left
out.

### Tier A — non-menu behaviour the device already has

Not part of the menu, but part of what ships, and the plan must not disturb it:

| Gesture | Action | Where |
|---|---|---|
| touch pad, hold 5 s | cut the battery latch, power down | `power_off_sequence()` |
| boot button, 1 s hold | toggle offline mode | `wifi_toggle_offline()` |
| boot button, 5 s hold | reopen the WiFi config portal | `wifi_request_portal_on_boot()` |
| boot button, 10 s hold | recovery wipe (networks + page login) | `recovery_boot()` |

Plus: status-pixel network blink, boot rail report and strip self-test, I²C scan,
impact detector, serial telemetry.

### Tier B — web only, on purpose

Presets (5 NVS slots), palette, brightness, strip length, every mode tunable,
OTA, sensor diagnostics, page login. These need a browser and keep needing one.

---

## 2. Why this is worth calling a menu

Today the gesture works — `check_button()` → `BUTTON_EVENT_LONG` → `led_mode_next()`
— but four things keep it from being navigable:

1. **No confirmation of where you landed.** The strip is the only output, and it is
   a poor one for the job: `flame` rests at `power_idle`, `fade` and `whip` can be
   near-black at rest, and a black palette draws nothing at all. Two of the five
   modes are indistinguishable at idle.
2. **One-way.** Nothing goes back, so overshooting a mode costs four more 1 s holds.
3. **No feedback during the hold** — and this is the sharp edge: the *same* pad
   cuts the power at 5 s. The menu's only gesture and the device's only irreversible
   action share one switch with no signal separating 1 s from 5 s. The boot
   button's recovery wipe has an amber countdown (`check_boot_button()`); the touch
   pad's power-off has nothing.
4. **Three taps are counted and do nothing** (`BUTTON_EVENT_TRIPLE` sets
   `last_event` and clears the counter) — a gesture the user can perform, that the
   firmware notices, and that is wired to nothing.

The menu is therefore not new functionality. It is a **navigation layer over the
existing registry**: an identity per mode, a way back, and feedback on the hold.

---

## 3. Navigation design

### 3.1 Gesture table — touch pad (GPIO3)

| Gesture | Window | Action | Change |
|---|---|---|---|
| tap | < 1 s | announce current mode (N pulses) | new |
| two taps | ≤ `MENU_DOUBLE_TAP_MS` (1200 ms) apart | previous mode | new |
| hold | ≥ 1 s | next mode | exists |
| hold | ≥ 5 s | power off | exists — unchanged |
| three taps | — | reserved, no action | unchanged |

**Boot button (GPIO9): unchanged.** Menu navigation does not go there: GPIO9 is a
strapping pin and already carries three network holds.

**The double-tap needs its own window.** The existing counter uses
`BUTTON_MULTI_PRESS_GAP_MS = 5000` — five seconds between presses still counts as
the same run. Reusing that for "previous mode" would turn "a tap now, another tap
in four seconds" into a mode change nobody asked for. `MENU_DOUBLE_TAP_MS` is
separate, shorter, and only ever *reads* `counter` — it does not change what
`button_finalize()` counts or what the page reports.

**Ordering rule that must survive the edit:** `button_finalize()` tests
`action_hold_ms` (power-off) before `long_press_ms` (next mode), and the branches
are exclusive. Any new gesture work happens *after* classification, in
`check_button()`, and must not introduce a path that reaches power-off with less
than a 5 s hold.

### 3.2 What the modes are called on the device

Pulse count = **position in the registry**, not the id. The ids are stable and
deliberately gapped (`1, 2, 4, 5, 6`) so the ids must never be used as a count:

| Position | id | key | Announce |
|---:|---:|---|---|
| 1 | 1 | `solid` | 1 pulse |
| 2 | 2 | `flame` | 2 pulses |
| 3 | 4 | `fade` | 3 pulses |
| 4 | 5 | `whip` | 4 pulses |
| 5 | 6 | `rainbow` | 5 pulses |

Position comes from `led_mode_at()`/`led_mode_count()`, which is also how
`led_mode_next()` steps. A stored id with no registry entry (a mode retired
between builds) reports position 0 and announces nothing rather than lying.

### 3.3 Feedback channels

There are exactly two, and the plan uses both for different jobs.

**Status pixel (GPIO10)** — the WS2812B at D2 (see *Hardware basis*).
`led_notice()` already owns it over the steady network blink, so a menu pulse needs
no new arbitration:

| Signal | Pattern | Colour |
|---|---|---|
| announce | N pulses, 100 ms on / 200 ms off | white, `LED_BLINK_LEVEL` (45) |
| hold progress | one 60 ms pulse every 200 ms until the 1 s threshold | white |
| power-off countdown | one pulse per 400 ms from 1 s to 5 s | red |
| network state (existing) | 200 ms per 5 s | green / cyan / red |
| recovery countdown (existing) | 400 ms pulses | amber |
| recovery wipe (existing) | steady 1.2 s | magenta |

Colours are assigned so a glance can tell them apart: white is only ever the menu,
red is only ever "you are about to lose power", amber and magenta stay with the
boot button.

All of these sit at `LED_BLINK_LEVEL` (45). The whole announce channel is this one
5050 pixel, so its legibility at that level — behind whatever diffuser the case
puts over it — is worth eyeballing once before the pulse train is committed to.

**Strip** — one confirm flash on a mode change: `strip_leds()` lit at ~25/255
white, held for `MENU_CONFIRM_MS` (150 ms), then the mode draws.

Two constraints on that flash, both from notes already in the source:

* it must loop to `strip_leds()`, never `NUM_LEDS`, or it lights the whole
  compiled buffer regardless of the configured length (`variables.h`);
* one frame is `LED_SCRIPT_FRAME_MS` = **50 ms**, which is too short to see, so the
  flash has to hold for ~3 frames. That means `update_strip()` needs a gate —
  `menu_owns_strip()` — checked before `entry->run()`, so the mode does not
  repaint over the confirm.

### 3.4 State

The pulse train needs a small non-blocking state, pumped from `loop()`:

```
struct { uint8_t pulses_left; uint32_t next_pulse_ms; bool in_pulse; } menu_announce_state;
```

`led_notice()` is a single-shot deadline and `update_onboard_LED()` clears
`led_notice_until_ms` on expiry — so the driver must keep **its own** timer and
re-arm each pulse, rather than inferring pulse boundaries from the notice.

**No `delay()` anywhere in this path.** The input path was rewritten specifically
to keep a held switch out of a wait loop (`readings.h`, the note above the button
table); the menu must not reintroduce one to blink a pixel.

---

## 4. Implementation plan

### 4.1 Files

| File | Change |
|---|---|
| `src/services/menu.h` | **new.** Gesture→action mapping, announce driver, hold feedback, strip confirm |
| `src/shared/dependencies.h` | include `services/menu.h` **after** `services/led_scripts.h` |
| `src/services/readings.h` | forward-declare the menu entry points; call them from `check_button()` |
| `src/services/led_scripts.h` | `led_mode_next()` becomes a wrapper over `menu_mode_step(+1)`; `update_strip()` gains the `menu_owns_strip()` gate |
| `src/main.cpp` | call `menu_loop()` from `loop()`, next to `update_onboard_LED()` |

**Include order is load-bearing.** `menu.h` needs `led_mode_at()`,
`led_mode_count()`, `led_notice()`, `strip_leds()` and `LED_SCRIPT_FRAME_MS` from
`led_scripts.h`, so it goes after it. `readings.h` is included *before*
`led_scripts.h`, so it must forward-declare what it calls — exactly the pattern
already used for `void led_mode_next();` (line 16) and `void led_notice(...)`
(line 34).

### 4.2 Interface

```
uint8_t menu_mode_index();                 // registry position of the live mode, 0 if unknown
void    menu_mode_step(int8_t delta);      // move by position, wrap, announce, request save
void    menu_announce(uint8_t pulses);     // start the pulse train
void    menu_hold_feedback(uint32_t held_ms);  // progress / power-off countdown, called while held
void    menu_loop();                       // pump the pulse train
bool    menu_owns_strip();                 // true while the confirm flash holds the strip
```

`menu_mode_step()` is the single place that changes `mode` from a gesture, and
`led_mode_next()` delegates to it so the button path and `POST /api/mode` cannot
drift apart — the same reasoning that already puts the wrap-around and the NVS
save in `led_mode_next()`.

`check_button()` gains: a double-tap branch, an announce-on-tap branch, and the
`menu_hold_feedback()` call while `touch_button.pressed` is true. The 5 s
power-off branch is untouched.

---

## 5. Persistence — what the save path actually costs

`led_mode_next()` calls `webpage_config_save()`, whose own comment reads:

> Writes only on an explicit POST from the page, never from the loop, so
> NVS wear stays proportional to user actions.

The button path breaks that comment. It does **not** break the wear budget — and
the two intuitive claims to the contrary are both wrong. Verified against the
IDF v5.1 source, not assumed:

| Claim that looks right | Reality |
|---|---|
| "the ten `putFloat` calls go through `nvs_set_blob`, which has no unchanged-value fast path" | **False.** `Storage::writeItem()` compares before writing — blobs through `cmpMultiPageBlob()` (CRC32 per chunk, then the bytes), primitives through `findPage->cmpItem(...)`. Both `return ESP_OK` **without a write** when the value is unchanged. The source says it outright: *"Do a check that the item in question is actually being modified. If it isn't, it is cheaper to purposefully not write out new data, since it may invoke an erasure of flash."* |
| "26 separate commits per mode change" | **Misleading.** `Preferences::put*` does call `nvs_commit()` after every set — but in IDF v5.1 `nvs_commit()` is `// no-op for now, to be used when intermediate cache is added`. The write happens inside `nvs_set_*`, immediately. |

So one mode change costs, in full:

* **28 `nvs_set_*` calls** (10 floats, 11 uchar, 6 ushort, 1 int) that each do a
  hash-list lookup and a **read** to compare, and write nothing;
* **exactly one flash entry written** — the `mode` key, one 32-byte entry appended
  to the active page, and its previous entry marked erased;
* one `remove("stmaxg")` on an absent key → no write;
* one namespace open/close.

For contrast, a *full* save from the web page — where every field can genuinely
change — writes at most 38 entries, because each of the 10 floats costs two (a blob
index plus one data chunk). Still under a third of a page.

### The arithmetic

NVS is `0x5000` = 20 KB = **5 sectors of 4 KB**. A page holds **126 entries** of
32 bytes (4096 less the 32-byte header and 32-byte entry bitmap), and wear
levelling spreads erases over the pages, one of which is held spare for relocation.

* 1 entry per mode change → a page fills after **~126 changes** → **~1 page erase
  per 126 changes**, and later than that in practice, since the erased slot is
  reusable;
* ESP32 SPI flash is rated on the order of **100,000 erase cycles per sector**;
* 100,000 × 126 ≈ **12.6 million mode changes**. At an absurd 1,000 changes a day,
  34 years.

**Wear is not reachable by human button presses.** There is nothing to buy by
deferring, batching or skipping this write, and the deferred-flush design the first
draft recommended was solving a problem that does not exist.

### What is still worth changing

1. **The comment is now false, and in a codebase that documents this carefully a
   stale contract is worse than no comment.** Either correct it or make it true.
2. **Option — write only the mode key.** A `webpage_config_save_mode()` that opens
   the namespace and writes `mode` alone turns 28 compare-reads into one set call.
   The absolute saving is small (the compares are cheap); the value is that the
   button path stops claiming to save a configuration it never touched.
   *Risk:* two save paths that must agree — contained, since it is one field.
3. **Measurement, if the numbers are ever doubted.** `nvs_get_stats()` reports
   `used_entries`, and it rises by exactly 1 per mode change. That can be printed
   from the existing telemetry in `serial_outputs.h` with no new dependency, and it
   settles the question on the bench rather than in argument.

**Not recommended: deferral.** It would open a window where a battery pull or a
brown-out loses the selection, and buy nothing measurable.

### Capacity, which matters more than wear here

NVS holds more than the live configuration: 5 preset slots (each ~90-byte blob =
1 blob index + ~4 chunks ≈ 5 entries), WiFiManager's saved networks, and the page
login. Rough total **~100 of ~500 usable entries**. Comfortable — but that is the
number to watch if presets ever grow past 5 slots or carry more fields, not the
erase count.

---

## 6. Test matrix (manual — this repo has no host-side test harness)

| # | Case | Expected |
|---|---|---|
| 1 | 1 s hold ×5 from boot | positions 1→5 then wrap to 1; announce matches |
| 2 | tap | current mode announced, nothing else changes |
| 3 | two taps, 500 ms apart | previous mode, wraps backwards |
| 4 | two taps, 3 s apart | two separate taps — no mode change |
| 5 | hold 1.2 s and release | next mode; power-off does **not** fire |
| 6 | hold 5 s | red countdown from 1 s; latch cuts at 5 s |
| 7 | tap at boot (first 10 s) | counted, no power-off (`POWER_OFF_ARM_MS`) |
| 8 | announce during the network blink | pulses visible, blink resumes after |
| 9 | boot button unchanged | offline toggle, portal, wipe all still fire |
| 10 | offline mode | every Tier A gesture works with the radio off |
| 11 | strip length = 20 | confirm flash lights 20 LEDs, not 250 |
| 12 | black palette + `solid` | announce + confirm are the only visible feedback |
| 13 | reboot after a mode change | the mode is the one last selected |
| 14 | power-off within 5 s of a change | the mode is already persisted — there is nothing to flush (§5) |
| 15 | serial log | one `[MODE]` line per change, no repeats |
| 16 | announce with the case fitted | the 1..5 pulses are countable at `LED_BLINK_LEVEL`; if not, the menu needs its own brightness (§8, open item 1) |
| 17 | NVS `used_entries` around a change | rises by exactly 1 (§5, if the cost is ever doubted) |

Build check: `pio run -e C3-USB`. Watch the flash figure — the app partition was
near-full before the custom table gave it headroom, and `menu.h` should add well
under a kilobyte, but "should" is worth confirming once.

---

## 7. Out of scope for the pre-release

Stated so it does not creep back in:

* **presets on the device** — web only; the pre-release preset values are accepted
  as they are
* **brightness, strip length, palette on the device** — web only
* **an OLED menu** — no display service exists; `display_started` is declared in
  `variables.h` and never set by anything, and `Adafruit_SSD1306` / `Adafruit GFX`
  sit in `lib_deps` unused
* **any second menu level, or a menu tree** — a one-level list does not need one
* **sensor re-arm, battery/rail readout, WiFi status screen** — web/serial only
* **new LED modes**

---

## 8. Decisions and open items

### Settled

| Question | Decision | What it changes in the design |
|---|---|---|
| Double-tap, or N taps to jump? | **Double-tap = previous mode** | §3.1 stands as written. The short tap window (`MENU_DOUBLE_TAP_MS`) is the only new timing constant, and it must not reuse `BUTTON_MULTI_PRESS_GAP_MS` (5000 ms) |
| Announce on every change, or only on a tap? | **Announce every change** — the status blink is the confirmation | The pulse train is fired from `menu_mode_step()`, so a hold and a double-tap both confirm where they landed. The single tap keeps its own job: announce the current mode without changing it ("where am I?"), which is still not redundant with the last change |
| Reduce NVS wear? | **Nothing to reduce** — one entry per change, ~12.6 million changes to reach flash endurance (§5) | No deferral and no dirty flag. Fix the stale comment; writing only the mode key is optional |
| Target board and status pixel | Lux et Portabilis **V2**; the WS2812B at D2 is mounted and working | See *Hardware basis*. `lux_minimus` ruled out — its `Status_LED` net has no LED, which would have left the announce with no channel at all |

### Open

1. **Announce brightness.** Everything on the status pixel runs at
   `LED_BLINK_LEVEL` (45). If the 5050 pixel is too dim behind the case diffuser for
   a 5-pulse train to be countable, the menu needs its own level rather than
   inheriting the network blink's. Since every change now announces, this is the
   most load-bearing unknown left.

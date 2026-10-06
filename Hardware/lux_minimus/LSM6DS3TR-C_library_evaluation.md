# LSM6DS3TR-C driver library evaluation — lux_minimus / ESP32-C3

Scope: pick and justify a driver for U3 (`LSM6DS3TR-C`, LCSC C967633) on the `lux_minimus`
board, wired I2C at 3.3 V (SDA = GPIO7, SCL = GPIO8, INT1 = GPIO5, address **0x6A** — see
`LSM6DS3TR-C_board_notes.md`). Bus is I2C only on this revision: **CS is tied high**, so SPI
is not electrically available without a board respin.

Board state as of the current schematic: SA0 is strapped to GND by R6 (10 kΩ) → address 0x6A;
SDx/SCx are grounded by R4/R3 (10 kΩ each); INT1 goes to GPIO5 and GPIO2 is left open, so the
ESP32-C3 boot-strap risk is gone.

Verified facts below come from the vendor repositories/registries directly. Anything I could
not verify from a primary source is explicitly marked "unverified".

---

## 0. Read this first: WHO_AM_I and register lineage

This single table decides which libraries are even candidates. ST's own register headers:

| Part | WHO_AM_I | I²C address | Register family |
|---|---|---|---|
| LSM6DS3 (original) | **0x69** | 0x6A/0x6B | its own map |
| **LSM6DS3TR-C** | **0x6A** | 0x6A/0x6B | **LSM6DSL** |
| LSM6DSL | **0x6A** | 0x6A/0x6B | reference |
| LSM6DSO | 0x6C | 0x6A/0x6B | its own map |

Two consequences that are easy to get wrong:

1. **The TR-C's register map matches LSM6DSL, not LSM6DS3.** Diffing ST's headers shows
   `DRDY_PULSE_CFG_G` at 0x0B (LSM6DS3 uses `ORIENT_CFG_G` there), a 3-bit vs 1-bit
   `func_cfg_en`, different `CTRL4_C`/`CTRL6_C`/`CTRL7_G`/`CTRL9_XL`/`CTRL10_C` bit
   layouts, and an **11-bit FIFO watermark on the TR-C vs 12-bit on LSM6DS3**. A driver
   written against LSM6DS3 stays wrong even if you patch its ID check.
2. **A library keyed to 0x69 fails outright on this part** — it does not "work by accident".
   Libraries keyed to **0x6A are ID-compatible**, but that alone proves nothing about the
   register map; check the FIFO/CTRL addresses too. (Verified against the datasheet's own
   register list: `FIFO_CTRL1` = **0x06**, `FIFO_CTRL2` = 0x07, `FIFO_CTRL5` = 0x0A.
   `espp/lsm6dso` uses `FIFO_CTRL1 = 0x07`, i.e. an LSM6DSO-shaped map.)

---

## 1. Summary verdict

| Rank | Library | Use for | Verdict |
|---|---|---|---|
| **1a** | [`adafruit/Adafruit_LSM6DS`](https://github.com/adafruit/Adafruit_LSM6DS) 4.7.4 | This project (PlatformIO + Arduino-ESP32) | **Recommended** |
| **1b** | [`Seeed-Studio/Seeed_Arduino_LSM6DS3`](https://github.com/Seeed-Studio/Seeed_Arduino_LSM6DS3) 2.0.7 | Same stack, if you need FIFO | **Recommended** — see the tie-break below |
| 2 | [`STMicroelectronics/lsm6ds3tr-c-pid`](https://github.com/STMicroelectronics/lsm6ds3tr-c-pid) v2.2.2 (STdC) | Bare-metal/ESP-IDF, or when you need every register | **Acceptable** — needs a platform shim |
| 3 | Roll your own register access | Tiny image, no dependency | **Acceptable for a data-only driver** |
| — | `espp/lsm6dso` (ESP-IDF registry) | — | **No** — provably wrong register map |
| — | `sparkfun`, `pololu`, `arduino-libraries` LSM6DS3 | — | **No** — hard-reject the TR-C |
| — | STM32-oriented X-CUBE-MEMS1 / MEMS Studio | Bench evaluation only | Not for this board |
| — | Zephyr / Rust crates / MicroPython | Only if the stack changes | Not applicable here (see §2.5) |

**Adafruit vs Seeed tie-break.** Adafruit first: the code is cleaner and better known, its
`_init()` hard-fails on a wrong WHO_AM_I (turning a board fault into an obvious error), and its
default address is already 0x6A — matching the board's R6 strap with no extra argument. Take
Seeed instead **if and only if you need FIFO** — it exposes
`fifoBegin/Clear/Read/GetStatus/End` plus `fifoTimestamp()`, which Adafruit has none of.
Seeed costs you two extra steps: you must pass `0x6A` explicitly because its default is 0x6B,
and **its SPI paths are compiled out on non-nRF52 targets**, so on an ESP32-C3 it is I²C-only —
which is all this board offers anyway.

---

## 2. The candidates, in detail

### 2.1 Adafruit LSM6DS — RECOMMENDED

- Repo: <https://github.com/adafruit/Adafruit_LSM6DS> · default branch `master`
- Version: **4.7.4** (`library.properties`, fetched from `master`); repo `pushed_at`
  **2024-12-03**, i.e. stable/maintenance mode rather than dormant
- License: BSD (`license.txt` in repo); registry metadata reports "Other/NOASSERTION"
- Dependencies: `Adafruit Unified Sensor`, `Adafruit BusIO`
- Transport: **I2C and SPI**
- **Explicit LSM6DS3TR-C support**, not "close enough": the tree contains
  `Adafruit_LSM6DS3TRC.{h,cpp}` and a dedicated example
  `examples/adafruit_lsm6ds3trc_test/`. From `Adafruit_LSM6DS3TRC.h`:
  `#define LSM6DS3TRC_CHIP_ID 0x6A`, and `_init()` refuses to proceed unless
  `chipID() == 0x6A` — which is exactly this part's WHO_AM_I.
- Default I2C address in the base class: `LSM6DS_I2CADDR_DEFAULT 0x6A` → matches SA0 = GND.
- Feature coverage (from `Adafruit_LSM6DS.h`):
  - `begin_I2C(addr, wire, sensorID)` / `begin_SPI(...)`
  - accel/gyro ODR and full-scale range getters/setters
  - `configIntOutputs(active_low, open_drain)`, `configInt1(drdy_temp, drdy_g, drdy_xl, step_detect, wakeup)`, `configInt2(...)`
  - `enableWakeup(enable, duration, thresh)`, `awake()`, `shake()`
  - `enablePedometer()`, `resetPedometer()`, `readPedometer()`
  - `highPassFilter()`, raw axes, temperature
  - `Adafruit_LSM6DS3TRC` adds `enableI2CMasterPullups()`
  - **Not exposed:** full FIFO streaming (no FIFO register API), 6D/4D orientation,
    free-fall and tap/double-tap are reachable only by writing `TAP_CFG`/`WAKEUP_THS`/
    `MD1_CFG` yourself via a raw register helper. There is no `getEvent()` for embedded
    functions beyond pedometer/wake/shake.
- Bus abstraction: **turnkey** — `Adafruit_BusIO` handles I2C/SPI; on ESP32 you pass
  `&Wire` (or `&Wire1`) and optionally call `Wire.begin(sda, scl)` first.
- **Fits this repo's existing stack**: `Firmware/Mavuika32-C3/platformio.ini` already pins
  `framework = arduino` (arduino-esp32 3.0.1) and already depends on
  `adafruit/Adafruit BusIO@^1.14.5`. Only `Adafruit Unified Sensor` is added.
- ESP32-C3 gotchas, assessed:
  - Uses `TwoWire`/`Adafruit_I2CDevice`, which sits on ESP-IDF's I2C driver — clock
    stretching is handled by the hardware controller, so the classic Arduino
    `Wire`-bitbang stretching problem does not apply.
  - All reads are blocking transactions (no delay loops), so it is safe inside a FreeRTOS
    task; a `getEvent()` read is ~14 bytes at 400 kHz, a few tens of µs plus bus overhead.
  - 400 kHz is fine with the 2.2 kΩ pull-ups already on the board (R1/R2).
  - Because `_init()` hard-checks `0x6A`, a floating SA0 pin is *not* silently tolerated —
    a wrong address is reported as a failed `begin()`. That is a feature here.

`lib_deps` for the recommended Adafruit path (matches the repo's existing PlatformIO style):

```ini
lib_deps =
    Wire
    adafruit/Adafruit BusIO@^1.14.5
    adafruit/Adafruit Unified Sensor@^1.1.14
    adafruit/Adafruit LSM6DS@^4.7.4
```

If you decide you need FIFO, replace the last line with
`seeed-studio/Seeed Arduino LSM6DS3@^2.0.7`.

Minimal bring-up sketch body:

```cpp
#include <Wire.h>
#include <Adafruit_LSM6DS3TRC.h>

Adafruit_LSM6DS3TRC imu;

void setup() {
  Serial.begin(115200);
  Wire.begin(/*SDA=*/7, /*SCL=*/8, 400000);   // GPIO7/GPIO8 per the schematic
  if (!imu.begin_I2C(0x6A, &Wire)) {         // 0x6A requires SA0 tied to GND
    Serial.println("LSM6DS3TR-C not found - check SA0/SDx/SCx ties");
    for (;;) delay(1000);
  }
  imu.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_104_HZ);
  imu.configInt1(true, true, true);          // DRDY routed to INT1 (GPIO5)
}
void loop() {
  sensors_event_t a, g, t;
  imu.getEvent(&a, &g, &t);
  Serial.printf("%.2f %.2f %.2f | %.2f %.2f %.2f\n",
                a.acceleration.x, a.acceleration.y, a.acceleration.z,
                g.gyro.x, g.gyro.y, g.gyro.z);
  delay(20);
}
```

### 2.2 ST official — `STMems_Standard_C_drivers` / `lsm6ds3tr-c-pid`

- The `lsm6ds3tr-c_STdC/driver` entry in
  [`STMicroelectronics/STMems_Standard_C_drivers`](https://github.com/STMicroelectronics/STMems_Standard_C_drivers)
  is a **git submodule** pointing at
  [`STMicroelectronics/lsm6ds3tr-c-pid`](https://github.com/STMicroelectronics/lsm6ds3tr-c-pid)
  ("lsm6ds3tr-c platform independent driver based on Standard C language and compliant with
  MISRA standard"). Clone with `--recursive` or you get an empty `driver/` directory.
- Platform-independent C (MISRA), **explicitly for this exact part**, exposes every
  register, FIFO, and embedded function.
- Bus abstraction: implements `platform_write()` / `platform_read()` plus delay and
  `lsm6ds3tr_c_io_init()` shims that **you must write** for ESP-IDF (`i2c_master_*`) or
  Arduino `Wire`. This is the main cost.
- License: ST's standard C-driver license (permissive, redistribution allowed with
  conditions) — check `LICENSE` before shipping.
- Verdict: the right choice if you need FIFO streaming, sensor-hub/aux-I2C, or
  ST's own register definitions as ground truth; overkill and more integration work for
  just reading accel/gyro.

### 2.3 Roll-your-own

Realistically small: WHO_AM_I (`0x0F` == `0x6A`), `CTRL1_XL 0x10`, `CTRL2_G 0x11`,
`CTRL3_C 0x12` (BDU, IF_INC, and note `SIM` for 3-wire), `CTRL4_C 0x13`,
`STATUS_REG 0x1E`, `OUT_TEMP_L 0x20`, `OUTX_L_G 0x22`, `OUTX_L_A 0x28`,
`CTRL9_XL 0x18`, `CTRL10_C 0x19`. 14-byte burst read of `0x22` gets gyro+accel in one
transaction. **Do enable BDU** (`CTRL3_C` bit 6) so MSB/LSB pairs never tear, and
**IF_INC** (bit 2) for auto-increment burst reads.

Caveat for ESP-IDF: an auto-init component can be vendored in `Firmware/<project>/src/`
like the existing `src/components/ESP32/` pattern.

### 2.4 ESP-IDF component registry — no TR-C component

Verified against <https://components.espressif.com>:
- `GET /api/components/espp/lsm6dso` → **HTTP 200**, `espp/lsm6dso`, latest **1.3.5**,
  MIT, requires ESP-IDF `>=5.0`, dependencies `espp/base_peripheral`, `espp/math`.
- `GET /api/components?q=lsm6ds` and `?q=lsm6ds3` return only `espp/lsm6dso` plus
  Soldered Inkplate examples that happen to *mention* an LSM6DS3.
- `GET /api/components/espp/lsm6ds3tr-c` → **404**. `…/espp/lsm6ds3` → **404**.

So: **there is no published LSM6DS3TR-C component in the ESP-IDF registry.** The closest is
`espp/lsm6dso`, which drives the LSM6DSO (WHO_AM_I `0x6C`) — a different part. Do not use it
for the TR-C; the class is named for the part it initializes.

### 2.5 Others checked and set aside

- **Seeed `Seeed Arduino LSM6DS3` 2.0.7** — the strongest true alternative; see §2.6.
- **SparkFun LSM6DS3 Breakout 1.0.3** (PlatformIO registry, `sparkfun`, last release
  2021-11-15): `beginCore()` rejects anything whose ID is not `0x69`, so it **fails outright**
  on a TR-C, and a long-open issue requests 0x6A support. Avoid.
- **Pololu `lsm6-arduino`** — accepts only `0x69` (LSM6DS33) and `0x6C` (LSM6DSO), I²C only,
  no advanced features, and v2.0.0 deliberately removed I²C timeout handling. Avoid.
- **`arduino-libraries/Arduino_LSM6DS3` 1.1.0** (LGPL-2.1) — accepts only 0x6C/0x69; the PR
  adding 0x6A is still open. Also known not to compile on ESP32. Avoid.
- **`stm32duino/STM32duino LSM6DS3` 2.0.0** — the richest Arduino feature API (tap, free-fall,
  6D, tilt, pedometer) but its constant is **0x69** and it defaults to address 0x6B.
  **Unverified** whether `begin()` actually validates the ID, so it may run on a TR-C despite
  the constant — but that would be relying on the LSM6DS3 register map being right, which §0
  says it is not. Treat as unsafe without testing.
- **DFRobot** — no LSM6 library exists.
- **Rust `no_std`**: `lsm6ds3tr` 0.2.2 (MIT, 2025-05-04, `embedded-hal ^1.0`, blocking) is
  genuinely part-specific and covers tap/interrupt routing. A newer `lsm6ds3trc` 0.1.0 also
  exists (published 2026-08-17, `embedded-hal-async`). Both irrelevant to this C++ firmware.
- **Zephyr**: the driver is `drivers/sensor/st/lsm6dsl/` with bindings
  `st,lsm6dsl-i2c`/`-spi` (Apache-2.0). There is **no** `st,lsm6ds3tr-c` binding, but the
  driver's `LSM6DSL_VAL_WHO_AM_I` is **0x6A**, so a TR-C initialises and works — consistent
  with §0's lineage finding. Caveats: it exposes data-ready triggers only (no tap/free-fall/
  6D/pedometer), and a known `CTRL3_C.BOOT` software-reboot failure on the TR-C was closed
  stale rather than fixed. Only relevant if this board ever leaves Arduino-ESP32.
- **MicroPython**: Pimoroni's `lsm6ds3-micropython` (MIT) is explicitly titled for the
  LSM6DS3TR-C and exposes pedometer, single/double tap, tilt, significant-motion and
  free-fall. Fine for a quick bench sanity check of the sensor itself; not a firmware stack.

### 2.6 Seeed Arduino LSM6DS3 2.0.7 — the FIFO-capable alternative

- PlatformIO registry: **`seeed-studio/Seeed Arduino LSM6DS3` 2.0.7**, released
  **2026-07-14**, 81 stars, popularity rank ~450 (verified via the PlatformIO registry API).
- `library.properties` sentence: "Arduino library to control Grove 6 Axis
  Accelerometer&Gyroscope LSM6DS3, **LSM6DS3-C**" — it names this part.
- It accepts **both** `0x69` and `0x6A`, and, unusually, **branches on the die**: temperature
  sensitivity 16 vs 256, and `TIMER_EN` in `TAP_CFG1` bit 7 on 0x69 vs `CTRL10_C` bit 5 on
  0x6A. That die-branching is the thing most other libraries get wrong.
- FIFO is first-class, including `fifoTimestamp()`.
- **SPI is compiled out on ESP32** (every SPI path is `#if defined(NRF52840_XXAA)`), so on an
  ESP32-C3 it is I²C-only — which is fine here, but means an SPI fallback on a future board
  revision would need a different library.
- CI compiles its examples for `esp32:esp32:XIAO_ESP32C3`, so ESP32-C3 is a tested target.
- License: MIT (`LICENSE.md` present in the repo root; the PlatformIO registry reports no
  machine-asserted `license` field). **Confirm before shipping.**
- **Not exposed:** tap, wake-up, 6D, or sensor-hub APIs (pedometer and free-fall exist only
  as example sketches).
- ⚠️ **Two traps specific to this board — read before using Seeed:**
  1. **Its default I2C address is 0x6B, not 0x6A.** Verified in `LSM6DS3.cpp`: the
     constructor initialises `I2CAddress(0x6B)`, and the source comment reads
     "Default construction is I2C mode, address 0x6B." With SA0 grounded to 0x6A you
     **must** pass the address explicitly — `LSM6DS3Core myIMU(I2C_MODE, 0x6A)` — or nothing
     will answer. This is the opposite of Adafruit, which defaults to 0x6A.
  2. **`beginCore()` calls bare `Wire.begin()`** (no SDA/SCL/frequency arguments), and
     neither constructor accepts pin arguments. Since the SDA/SCL pins and clock are
     board-specific, plan on an explicit `Wire.begin(7, 8, 400000)` and verify on hardware
     that `beginCore()`'s no-argument call does not disturb it. This is an unverified
     runtime behaviour, not a documented one.
  Its ID handling is otherwise correct: it reads WHO_AM_I (`0x0F`), accepts `0x69` **or**
  `0x6A`, and branches per die — the `0x6A` branch is commented "LSM6DS3-C/TR-C".

### 2.7 ESP32-C3 I²C notes

- There is **no ESP32-C3 I²C clock-stretching erratum**; the official errata list for
  revisions v0.0–v1.1 contains only ADC-270, CPU-863 and ADC-183. The real constraint is the
  hardware SCL-await timeout (`i2c_device_config_t::scl_wait_us`), which the ESP-IDF docs note
  may need to accommodate a stretch "even […] 12 ms". Nothing in this design is near that.
- ESP32-C3 supports 100 kHz and 400 kHz master operation; 400 kHz must not be exceeded.
  External pull-ups of 2–5 kΩ are recommended — the board's 2.2 kΩ R1/R2 sit nicely in range.
- Under Arduino, prefer `Wire.begin(sda, scl, 400000)` over calling `Wire.setClock()` after
  `begin()`; several reported `Wire` clock-stretching bugs were fixed in 2022 and that is the
  reliable call pattern.
- No LSM6DS3/TR-C-specific bug report against ESP32-C3 was found in the ESP-IDF or
  arduino-esp32 trackers. Absence of evidence, not proof of absence.

---

## 3. What the choice means for the board

1. The recommended libraries assume the part answers at **0x6A**, which the board now
   guarantees via **R6 = 10 kΩ from SDO/SA0 to GND**. See `LSM6DS3TR-C_board_notes.md §2–3`.
2. `configInt1()` in the Adafruit API is what drives the `INT` net (U3 pin 4 → ESP32-C3
   **GPIO5**). GPIO5 carries no ESP32-C3 reset-time constraint, and GPIO2 is now left open,
   so the earlier boot-strap risk is resolved.
3. All the recommended options need **external I2C pull-ups**, which the board already has
   (R1/R2 = 2.2 kΩ to 3.3 V).
4. `enableI2CMasterPullups()` (Adafruit, bit 3 of `MASTER_CONFIG` = `0x1A`) is `PULL_UP_EN`,
   which only affects the **auxiliary** I2C master lines — i.e. SDx/SCx. It is a no-op now
   that R3/R4 hold those pins at GND, and `PULL_UP_EN` only takes effect with `FUNC_EN` set
   anyway. Do not "fix" the grounded SDx/SCx by enabling it.
5. Because the address is now fixed at 0x6A, remember **Seeed's default of 0x6B** — pass
   `0x6A` explicitly or its `begin()` will find nothing. Adafruit needs no argument.

---

## 4. Confidence

Verified from primary sources: the WHO_AM_I/lineage table (§0), the ESP-IDF registry contents,
PlatformIO registry metadata and release dates, `library.properties` and **source** inspection
for Adafruit and Seeed (including Seeed's `I2CAddress(0x6B)` default and its bare
`Wire.begin()` call), the Adafruit and ST driver file listings and API surfaces, and the
LSM6DS3TR-C datasheet's own `FIFO_CTRL*` addresses.

Not independently verified: whether Seeed's internal no-argument `Wire.begin()` disturbs an
already-configured bus on ESP32-C3 (runtime behaviour — test on hardware); the exact SPDX of
the Seeed, Adafruit and Arduino libraries (their registry entries carry no machine-asserted
license); whether `stm32duino/STM32duino LSM6DS3::begin()` validates WHO_AM_I; Zephyr/Rust
feature completeness; and the vendor tooling claim that MEMS Studio supersedes Unicleo-GUI.
ST's own website was unreachable during research, so anything that would have needed an
`st.com` page is sourced from GitHub mirrors instead.

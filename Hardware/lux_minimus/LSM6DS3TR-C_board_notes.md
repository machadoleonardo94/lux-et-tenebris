# LSM6DS3TR-C on lux_minimus (ESP32-C3) — interface & pin-state notes

Sources: `Hardware/lux_minimus/LSM6DS3TR-C.pdf` (DocID030071 Rev 3), `lux_minimus.kicad_sch`,
`lux_minimus.kicad_pcb`.

## 1. What the datasheet fixes

| Item | Value | Where |
|---|---|---|
| WHO_AM_I (0x0F) | fixed `6Ah` | §9.12, Table 49 |
| Register-map lineage | matches **LSM6DSL**, not LSM6DS3 (`FIFO_CTRL1` = 0x06, `DRDY_PULSE_CFG_G` = 0x0B, 11-bit FIFO watermark) | TOC + §9 |
| I2C slave address | `110101xb` → **0x6A** (SA0=0) / **0x6B** (SA0=1) | §6.3.1 |
| Interface select | **CS high = I2C enabled**; CS low = SPI | §6.1, Table 10 |
| I2C speed | standard + fast mode, 400 kHz | §6.3 |
| Bus loading | SCL and SDA **must** have external pull-ups to VDDIO | §6.3 |
| Pin 1 SDO/SA0 | SPI SDO in mode 1/2; **I2C address LSb (SA0)** | Table 2, Table 18 |
| Pin 2 SDx | Mode 1: "connect to VDDIO or GND"; Mode 2: aux I2C MSDA | Table 2 |
| Pin 3 SCx | Mode 1: "connect to VDDIO or GND"; Mode 2: aux I2C MSCL | Table 2 |
| Pin 10, 11 NC | Leave electrically unconnected, solder to PCB | Table 2 note 2 |
| Pin 12 CS | internal pull-up by default, 30–50 kΩ | Table 18 note |
| Pin 4 INT1 / pin 9 INT2 | default "output forced to ground" | Table 18 |
| Pin 1/2/3 default status | **input without pull-up** (pull-up only if `SIM=1` in 0x12 or `PULL_UP_EN=1` in 0x1A) | Table 18 |
| Input voltage on any control pin (incl. CS, SCL/SPC, SDA/SDI/SDO, SDO/SA0) | −0.3 to VDD_IO+0.3 V | §4.5 |

Practical consequence: pins 1, 2, 3 are **high-impedance CMOS inputs with no internal
pull-up or pull-down by default**. Their level is therefore whatever the board gives them —
they must not be left floating.

## 2. How lux_minimus currently wires U3

Read directly from the schematic (global labels sit on the pin endpoints, so they are the net):

| U3 pin | Name | Net on this board |
|---|---|---|
| 8 | VDD | `+3.3V` |
| 5 | VDDIO | `+3.3V` |
| 12 | CS | `+3.3V` → **I2C mode selected in hardware** |
| 7, 6 | GND | `GND` |
| 14 | SDA | `SDA` → ESP32-C3 **GPIO7**, with **R2 = 2.2 kΩ to +3.3 V** |
| 13 | SCL | `SCL` → ESP32-C3 **GPIO8**, with **R1 = 2.2 kΩ to +3.3 V** |
| 4 | INT1 | `INT` → ESP32-C3 **GPIO5** |
| 1 | SDO/SA0 | **R6 = 10 kΩ to GND** → I2C address **0x6A** |
| 2 | SDx | **R4 = 10 kΩ to GND** |
| 3 | SCx | **R3 = 10 kΩ to GND** |
| 9 | INT2 | **nothing — floating** (legal: it is a push-pull output, forced low by default) |
| 10, 11 | NC | not connected (correct) |

C1 (100 nF) and C2 (100 nF) are the VDD/VDDIO decoupling, as recommended.

**Status: the three pin-state findings from §3 below are now implemented in the schematic.**
Verified by re-parsing `lux_minimus.kicad_sch`: R6 pin 1 → U3 pin 1, R4 pin 1 → U3 pin 2,
R3 pin 1 → U3 pin 3, and each resistor's other terminal lands on a `GND` power symbol
(`#PWR012`/`#PWR013`/`#PWR014`). The `INT` global label now sits on U1 pin 1 (GPIO5), and
U1 pin 11 (GPIO2) carries the new single-endpoint net `GPIO_2` with no other member.

## 3. Required state for SD0 / SDx / SCx

**Implemented** — see §2. This section records the reasoning.

The part is used in **Mode 1** (no auxiliary I2C master), so SDx/SCx are not used as
MSDA/MSCL. In Mode 1 the datasheet's own instruction for pins 2 and 3 is
"Connect to VDDIO or GND".

| Pin | Net | Required state | Implementation on this board |
|---|---|---|---|
| 1 SDO/SA0 | SA0 | Defined logic level: **GND → 0x6A**, VDDIO → 0x6B | **R6 10 kΩ to GND** → 0x6A |
| 2 SDx | unused aux data | Tie to a rail (GND preferred) | **R4 10 kΩ to GND** |
| 3 SCx | unused aux clock | Tie to a rail (GND preferred) | **R3 10 kΩ to GND** |

Why GND rather than VDDIO:
- Auxiliary I2C pull-ups are disabled by default (`PULL_UP_EN=0` in 0x1A), so tying SDx/SCx
  high would leave two idle-high lines with no defined pull, which is harmless but pointless.
- GND keeps the aux pins from being a leakage/injection path and from ever looking like a
  valid aux I2C idle state.
- SA0 = GND gives 0x6A, the address every LSM6DS3TR-C library defaults to
  (Adafruit breakout default is 0x6A with AD0 open, per the
  [Adafruit guide](https://learn.adafruit.com/adafruit-lsm6ds3tr-c-6-dof-accel-gyro-imu/pinouts)).

Direct ties would also be acceptable because these are control inputs, not open-drain lines,
and the absolute maximum for any control pin is −0.3 V to VDD_IO+0.3 V. 10 kΩ was chosen
anyway, which is strictly better: it allows SA0 to be re-strapped later without cutting a
trace, and it costs nothing, since these are static CMOS inputs with negligible leakage.

**Why this mattered:** with SA0 left floating the device address was indeterminate (0x6A or
0x6B depending on leakage), so an I2C scan might or might not find it, and the answer could
even change across power cycles or with temperature and humidity. Now that R6 fixes SA0 at
GND the address is **0x6A**, which is the value every candidate library defaults to.

## 4. Other firmware-relevant notes

- **I2C pull-ups are present and adequately sized.** R1 (2.2 kΩ, SCL) and R2 (2.2 kΩ, SDA)
  both return to the +3.3 V rail (`power:+3.3V` at 96.52, 48.26), matching the datasheet
  requirement for external pull-ups on SCL and SDA (§6.3). No change needed.
- **INT1 moved to GPIO5 — boot risk resolved.** It was previously on GPIO2, an ESP32-C3
  strapping pin that must be high at reset, while the LSM6DS3TR-C defaults INT1 to "output
  forced to ground". The `INT` net now terminates on U1 pin 1 (GPIO5), and GPIO2 (U1 pin 11)
  is a single-endpoint net with no other member — so it is open, and the sensor can no longer
  influence the boot strap. GPIO5 has no reset-time constraint.
- **SCL is on GPIO8**, an ESP32-C3 strapping pin. Because the I2C bus idles high through
  R1, GPIO8 is high at boot, which is the required state — so this one is fine, but do not
  add anything that pulls SCL low at reset. (On many ESP32-C3 SuperMini variants GPIO8 also
  drives the on-board LED; see `esp32c3_pinmap.md` — variant-dependent, worth confirming
  against the actual module.)
- **The PCB is not laid out yet** — `lux_minimus.kicad_pcb` currently contains only the
  ESP32-C3 module footprint (U1); the sensor footprint, the SA0/SDx/SCx ties, C1/C2 and R1/R2
  all still need to be placed and routed.
- Note a naming trap: the `U1` symbol is `Custom_symbols:ESP32-C3-DevKitM-1` but is referenced
  as `ESP32-C3-Supermini`, and its library pin *names* (GPIO5…GPIO10) do **not** match the
  ESP32-C3 SuperMini silkscreen order. Use the pin **numbers** when checking the physical
  header: I2C is pin 3 (SDA) and pin 4 (SCL), INT is pin 11.
- Register constants worth knowing (verified against the datasheet's register descriptions):
  `WHO_AM_I` 0x0F = 0x6A; `CTRL1_XL` 0x10; `CTRL2_G` 0x11; `CTRL3_C` 0x12 (`SIM` = SPI 3-wire,
  `BDU`, `IF_INC`); `CTRL4_C` 0x13 (`I2C_disable`); `CTRL9_XL` 0x18; `CTRL10_C` 0x19
  (`FUNC_EN`); `MASTER_CONFIG` 0x1A (`PULL_UP_EN` = aux pull-up); `STATUS_REG` 0x1E;
  `OUT_TEMP_L` 0x20; `OUTX_L_G` 0x22; `OUTX_L_XL` 0x28.
- Auxiliary I2C pull-ups are **disabled by default** (`PULL_UP_EN = 0` in `MASTER_CONFIG`),
  which is why SDx/SCx must be tied to a rail rather than left to float high.

## 5. Cross-reference

`esp32c3_pinmap.md` in this folder has the full, independently parsed U1 net-to-GPIO table and
agrees with the mappings above (SDA = GPIO7 / pin 3, SCL = GPIO8 / pin 4, INT = GPIO5 / pin 1,
I2C bus at 3.3 V with R1/R2 = 2.2 kΩ, plus R3/R4/R6 = 10 kΩ to GND). Both analyses used the
same method (union-find over the schematic wires plus direct label-on-pin-endpoint matching),
and both were re-run after the INT/SA0/SDx/SCx changes.

`LSM6DS3TR-C_library_evaluation.md` picks the driver for U3 and explains what the address and
register-lineage facts mean for library choice — notably that the original LSM6DS3 has
WHO_AM_I `0x69` while this part has `0x6A`, so a library keyed to `0x69` fails outright.

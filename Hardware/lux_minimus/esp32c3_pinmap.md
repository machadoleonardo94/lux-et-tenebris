# ESP32-C3 net-to-GPIO mapping — `lux_minimus`

> **Update (post-review):** the schematic was revised after the first pass. U3's `INT` net now
> terminates on **U1 pin 1 (GPIO5)** instead of pin 11, and U1 pin 11 (GPIO2) is now a
> single-endpoint net named `GPIO_2` (left open). U3 pins 1/2/3 are now grounded through
> **R6/R4/R3 = 10 kΩ**, strapping SA0 to 0x6A. Tables below reflect the re-verified state.

**Board:** `Hardware/lux_minimus/lux_minimus.kicad_sch` (KiCad 8/9 S-expression, single sheet, 10,246 lines)
**Analysis method:** the schematic was parsed programmatically (full S-expression parse of `lib_symbols`, symbol instances, all 82 wires, 20 junctions, 19 global labels, 0 local labels, 0 no-connects), pin absolute coordinates were computed from symbol placement + pin offsets, and connectivity was resolved by union-find over wire segments including points that lie *on* a segment. Results were cross-checked against the `.kicad_pcb` net table and symbol-pin geometry. Nothing below is inferred from the task description.

---

## 1. ESP32-C3 symbol instances

| Ref | Value | lib_id | Footprint | Placement |
|---|---|---|---|---|
| **U1** | `ESP32-C3-Supermini` | `Custom_symbols:ESP32-C3-DevKitM-1` | `Aleoexpress:ESP32-C3-Supermini` | `(at 76.2 63.5 0)`, unit 1, no mirror |

There is exactly **one** ESP32-C3 symbol instance on this board. Note the lib_id is misleading: it is named `ESP32-C3-DevKitM-1` but its value and footprint are the **ESP32-C3 SuperMini module** (16-pin, 2×8 header). The custom symbol exposes only the 16 SuperMini header pads — it does **not** expose GPIO11–17 (internal SPI flash) or GPIO18/19 (USB D−/D+, which stay on the module's USB-C connector).

The only other U-designators are `U2` (`New_Library:TP4056` charger) and `U3` (`ALeoExpress:LSM6DS3TR-C` IMU) — neither is an MCU.

---

## 2. Complete U1 pin table

Symbol anchor `(76.2, 63.5)`, rotation 0°, no mirror. Absolute pin coordinates computed as `(X + dx, Y − dy)` (KiCad schematic Y grows downward). Every GPIO pin is at `x = 91.44`; the labels sit 2.54 mm further right on short wire stubs (not directly on the pin endpoints), so the stub wires were followed.

| Pin # | Pin name | Type | Absolute (x, y) | Net | Other members of that net |
|---:|---|---|---|---|---|
| 1 | GPIO5 | bidirectional | 91.44, 69.85 | `INT` | U3.4 (LSM6DS3TR-C INT1) — *moved here from GPIO2* |
| 2 | GPIO6 | bidirectional | 91.44, 67.31 | `GPIO_6` | *(none — single-endpoint net)* |
| 3 | GPIO7 | bidirectional | 91.44, 64.77 | `SDA` | U3.14 (LSM6DS3TR-C SDA), R2.2 (2.2 k pull-up) |
| 4 | GPIO8 | bidirectional | 91.44, 62.23 | `SCL` | U3.13 (LSM6DS3TR-C SCL), R1.2 (2.2 k pull-up) |
| 5 | GPIO9 | bidirectional | 91.44, 59.69 | `OB_Button` | *(none — single-endpoint net)* |
| 6 | GPIO10 | bidirectional | 91.44, 57.15 | `Status_LED` | *(none — single-endpoint net)* |
| 7 | GPIO20/U0RXD | bidirectional | 91.44, 41.91 | **unconnected** | no wire, no label, no no-connect flag |
| 8 | GPIO21/U0TXD | bidirectional | 91.44, 44.45 | **unconnected** | no wire, no label, no no-connect flag |
| 9 | GPIO0 | bidirectional | 91.44, 85.09 | `ADC1_0` | R18.2 (100 k), R19.1 (100 k pulldown to GND), C8.2 (100 nF to GND) |
| 10 | GPIO1 | bidirectional | 91.44, 82.55 | `Strip_out` | R7.1 (330 Ω) |
| 11 | GPIO2 | bidirectional | 91.44, 80.01 | `GPIO_2` | *(none — single-endpoint net; left open intentionally)* |
| 12 | GPIO3 | bidirectional | 91.44, 77.47 | `Button_GPIO` | TOUCH1.2 (JST 1×03 pin 2), R5.1 (10 k), R11.2 (100 k to GND), D6.2 (1N4148W anode), C28.2 (100 n) |
| 13 | GPIO4 | bidirectional | 91.44, 74.93 | `latch_enable` | R29.1 (330 Ω) |
| 14 | 3V3 | power_in | 73.66, 31.75 | `+3.3V` | `#PWR01` +3.3V symbol; R1.1, R2.1 (I²C pull-ups); U3 VDD/VDDIO/CS; C1, C2 |
| 15 | GND | passive | 76.20, 91.44 | `GND` | `#PWR02` GND symbol |
| 16 | 5V | power_in | 78.74, 31.75 | `+5V` | `#PWR03` +5V symbol; D1.1 (SS14 cathode); R9.1 (0R22 → TP4056 VCC) |

Only 16 pins exist on this symbol — pins 1–16 as listed, nothing hidden and no additional units.

---

## 3. Requested named nets → GPIO

| Net name | GPIO | U1 pin # | Net members (schematic-wide) | Notes |
|---|---|---|---|---|
| `SDA` | **GPIO7** | 3 | U1.3, U3.14 (LSM6DS3TR-C SDA), R2 (2.2 k → +3.3V) | I²C data |
| `SCL` | **GPIO8** | 4 | U1.4, U3.13 (LSM6DS3TR-C SCL), R1 (2.2 k → +3.3V) | I²C clock — **strapping pin** |
| `INT` | **GPIO5** | 1 | U1.1, U3.4 (LSM6DS3TR-C INT1) | Sensor data-ready / interrupt. **Moved here from GPIO2** to avoid the boot strap |
| `GPIO_2` | **GPIO2** | 11 | U1.11 only | Left **open** intentionally — was the INT pin. **Strapping pin**, no reset-time load now |
| `GPIO_6` | **GPIO6** | 2 | U1.2 only | Net exists but has **no other connection** in this schematic |
| `Button_GPIO` | **GPIO3** | 12 | U1.12, TOUCH1 pin 2 (1×03 socket), R5 10 k, R11 100 k→GND, D6 1N4148W, C28 100 nF | External/consumer button + touch pad input. Goes to a **connector (TOUCH1)** as well as passives |
| `ADC1_0` | **GPIO0** | 9 | U1.9, R18 100 k, R19 100 k→GND, C8 100 nF→GND | Analog input with pulldown + RC filter (unfitted divider network) |
| `latch_enable` | **GPIO4** | 13 | U1.13, R29 (330 Ω) → D7/Q2/R30 soft-latch power-switch network | Active power-latch enable (see below) |
| `Status_LED` | **GPIO10** | 6 | U1.6 only | Net exists but has **no LED and no other connection** in this schematic |
| `Strip_out` | **GPIO1** | 10 | U1.10, R7 (330 Ω) | Addressable-LED strip data output |
| `OB_Button` | **GPIO9** | 5 | U1.5 only | "On-board button" — matches the SuperMini's on-module BOOT button on GPIO9. Net left dangling **intentionally**; **strapping pin** |

**Nets that terminate only on passives/connectors, not on the MCU:**

- `+BATT` — battery/reverse-power rail: BATT1 (1×02 socket pin 1), C5, U2.5 (TP4056 BAT), BT4 (push button), R28 (100 k), TOUCH1 pin 1, `#PWR*` symbols. **No direct MCU pin.**
- `V_REG` — switched battery rail after the PMOS `Q1`: D1 anode, R16 (100 k), J2 pin 1 (1×03 socket), `#PWR*`. **No direct MCU pin.** V_REG feeds the U1 `5V` pin through Schottky diode **D1 (SS14)** → `+5V`.
- `+5V` — U1.16 (module 5 V input), D1 cathode, R9 (0R22 current-sense → TP4056 VCC).
- `GND` — U1.15 and the rest of the board.
- `Button_GPIO` and `ADC1_0` also reach connectors/passives, but their MCU pin is as tabulated.

Net-name evidence: the module's own 5 V input is fed from `V_REG` through D1, and `V_REG` is separated from `+BATT` by the high-side PMOS `Q1` (soft-latch). This confirms `V_REG` and `+BATT` are two distinct nets: KiCad takes the net name from a power symbol's **Value** field, which is why `#PWR04`, `#PWR0152`, `#PWR043`, `#PWR018` carry lib_id `power:+BATT` but value `V_REG`.

---

## 4. GPIO usage, free pins and constrained pins

The ESP32-C3 SuperMini header breaks out **GPIO0–GPIO10, GPIO20, GPIO21** (13 GPIOs). GPIO11–GPIO17 are not available (internal SPI flash); GPIO18/GPIO19 (USB D−/D+) are wired only to the module's USB-C connector and are **not broken out**.

**Used (11):** GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10.

**Free / available for new use (2):** **GPIO20 (U0RXD)** and **GPIO21 (U0TXD)** — both left completely unrouted on this board. They are also the UART0 console pins, so they double as a debug UART header if the module's USB-C is not used.

**Constrained pins:**

| GPIO | Constraint | Impact on this design |
|---|---|---|
| **GPIO2** | **Strapping pin.** Must read **1** at reset for SPI boot and download boot ([Espressif ESP32-C3 guide, Table 5.5](https://espressif.github.io/esp32-c3-book-en/chapter_5/5.2/5.2.6.html)). | ⚠️ **Resolved.** Previously carried `INT` from the LSM6DS3TR-C, whose INT1 defaults to "output forced to ground" — a real boot risk with no pull-up on the net. `INT` now goes to GPIO5 and GPIO2 is a single-endpoint net left open, so nothing loads the strap any more. |
| **GPIO8** | **Strapping pin** (must be high at reset); also the SuperMini's **onboard user LED pin, active-low**, with a series resistor to 3V3 (vendor-dependent — [NuttX SuperMini docs](https://nuttx.apache.org/docs/latest/platforms/risc-v/esp32c3/boards/esp32c3-supermini/index.html) explicitly warn to confirm against your module's silkscreen). | Used as `SCL`. R1 (2.2 k to +3.3V) satisfies the "must be high at reset" requirement since I²C idles high. However: (a) the module's own LED load sits on the SCL line and will flicker with I²C traffic; (b) the added capacitance/loading is not ideal for I²C edges. **Design smell worth reviewing.** |
| **GPIO9** | **Strapping pin** (0 = download boot); SuperMini **BOOT button** pin, weak internal pull-up. | Used as `OB_Button`, but the net is deliberately left unconnected on the carrier — the button is on the module itself. Holding BOOT during reset still enters download mode. Fine, but be aware GPIO9 is not freely usable as a normal output without care. |
| **GPIO18 / GPIO19** | USB D−/D+ (USB Serial/JTAG). | Not on the header and not routed. Firmware flashing/console must go through the module's USB-C. |
| **GPIO20 / GPIO21** | UART0 RX/TX (also the ROM console). | Free here; if used, be aware the ROM bootloader briefly drives GPIO21 as TX. |
| **GPIO5** | ADC2 channel — ADC2 is unusable while Wi-Fi is active. JTAG MTDI. | Now carries `INT` from the LSM6DS3TR-C. No reset-time constraint, so this is a safe home for the interrupt; only the Wi-Fi/ADC2 caveat applies if you also want analog input here. |
| **GPIO4** | ADC1_CH4, JTAG MTMS. | `latch_enable` — fine, but keep in mind JTAG use. |
| **GPIO6 / GPIO7** | SPI2 CLK / MOSI, JTAG MTCK / MTDO. | GPIO6 = `GPIO_6` (dangling), GPIO7 = `SDA`. |
| **GPIO10** | SPI2 CS. | `Status_LED` (dangling net). |
| **GPIO0 / GPIO1** | ADC1_CH0 / ADC1_CH1; GPIO1 is also a valid XTAL_32K input option. | `ADC1_0` / `Strip_out`. Preferred general-purpose I/O — good choices. |

---

## 5. I²C pull-ups and bus voltage

| Ref | Value | Connection | Function |
|---|---|---|---|
| **R1** | **2.2 kΩ** | pin 1 → `+3.3V` (93.98, 48.26); pin 2 → `SCL` (93.98, 55.88) | I²C **SCL** pull-up |
| **R2** | **2.2 kΩ** | pin 1 → `+3.3V` (96.52, 48.26); pin 2 → `SDA` (96.52, 55.88) | I²C **SDA** pull-up |
| **R6** | **10 kΩ** | pin 1 → U3.1 `SDO/SA0`; pin 2 → `GND` | Straps the I²C address to **0x6A** |
| **R4** | **10 kΩ** | pin 1 → U3.2 `SDx`; pin 2 → `GND` | Holds unused aux-I²C data line low |
| **R3** | **10 kΩ** | pin 1 → U3.3 `SCx`; pin 2 → `GND` | Holds unused aux-I²C clock line low |

All are `Device:R` instances, values confirmed by reading the `Value` property in the schematic (not inferred).

- **I²C bus voltage: 3.3 V.** The only I²C devices on `SDA`/`SCL` are U1 (ESP32-C3 at 3.3 V) and U3 (LSM6DS3TR-C). U3's `VDDIO` (pin 5) is tied to the `+3.3V` net, so its I/O levels match.
- **VDDIO rail:** `+3.3V`, sourced solely by **U1 pin 14 (3V3)** — the SuperMini module's own on-board 3.3 V regulator. The decoupling capacitors C1/C2 (100 nF) and the IMU supply (U3 VDD pin 8, VDDIO pin 5, CS pin 12) are all on this same rail. The module's 5 V input (U1 pin 16) is fed from `V_REG` through D1 (SS14 Schottky).
- 2.2 kΩ is a reasonable value for 3.3 V I²C at moderate bus capacitance, but note the added load of the SuperMini's on-board LED + resistor on GPIO8 (SCL).

---

## 6. Caveats and data-quality notes

1. **The `.kicad_pcb` is stale and essentially unpopulated.** It contains exactly **one footprint** (U1, `Aleoexpress:ESP32-C3-Supermini`, 32 pads = 16 pins × 2 pad types), **zero tracks and zero vias**. Its net table still lists an old net `Servo_out` and is **missing** `INT`, `+BATT` and `V_REG`. U1 pad **11** is therefore `Servo_out` in the PCB but `GPIO_2` in the schematic — i.e. the net appears to have been renamed (twice) after the PCB netlist was last generated. **Trust the schematic, not the PCB netlist.** All other U1 pad↔net assignments in the PCB match the schematic exactly, which independently validates the mapping in section 2.
2. **`GPIO_2`, `GPIO_6`, `Status_LED` and `OB_Button` are single-endpoint nets.** Each global label string occurs exactly once in the schematic (verified by occurrence count), so no other component or sheet connects to them. A single-sheet design was confirmed (no `(sheet ...)` blocks). `GPIO_2` is open **by design** (the former INT pin). `Status_LED` in particular has **no LED** on it — if a status LED was intended, that circuit is missing. `OB_Button` dangling is expected, as the BOOT button is on the module.
3. **Labels are NOT placed directly on U1's pin endpoints.** For U1 they sit on 2.54–6.35 mm wire stubs (`x = 93.98` or `97.79` vs. pin `x = 91.44`). Labels *are* placed exactly on pin endpoints for U3 (e.g. `64.77, 118.11` for SDA). The analysis followed wires in both cases, so the result is unaffected.
4. **Uncertainty — SuperMini variant.** "SuperMini" is not an Espressif product; the lib_id says `ESP32-C3-DevKitM-1` while the value says `SuperMini`. Assumptions about the on-module LED being on GPIO8 and the BOOT button on GPIO9 come from third-party documentation and hardware conventions, not from this schematic. The folder's `ESP32-C3 supermini v5.step` indicates a v5 mechanical variant. Verify the actual module.
5. **GPIO2/strapping risk — closed.** The conflict reported in the first revision of this document was resolved by moving `INT` to GPIO5. GPIO2 now carries no load.

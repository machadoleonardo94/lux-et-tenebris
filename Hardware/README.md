# Lux et Tenebris Hardware

[![KiCad](https://img.shields.io/badge/KiCad-8.0-blue.svg)](https://www.kicad.org/)
[![PCB](https://img.shields.io/badge/PCB-V2-green.svg)](https://github.com/machadoleonardo94/lux-et-tenebris)
[![Status](https://img.shields.io/badge/Status-Production_Ready-orange.svg)](https://github.com/machadoleonardo94/lux-et-tenebris)

Hardware design files for the **Lux et Tenebris Devkit** - an ESP32-based LED controller development board with integrated OLED display, WS2812B LED channels, and multiple output configurations.

## Overview

The Lux et Tenebris hardware consists of custom PCB designs created in KiCad, providing a compact platform for controlling addressable LEDs, RGB strips, and other lighting applications. The board integrates power management and some I/O expansion ports.

## Hardware Versions

### lux-et-pcb (V1.0)
- Initial release
- Core ESP32 functionality
- DC 5521 connector for 5~24V external power
- USB-C charging port
- No integrated USB-Serial converter, requires an external one for burning firmware
- Optional 1S lithium charger and battery
- 4x WS2812B LED channels (5V from regulator or internal battery)
  - GPIOs 32, 33, 19 and 16 
- 3x Open drain outputs (for 12V RGB strips)
  - GPIOs 27, 26 and 25
- 1x Controlled current output (avoid pesky short-circuits frying your wires)
  - GPIO 13
- 2x expansion port with 3V3 power
  - Those can be set as other functions for external hardware
  - GPIOs 21 and 22 for I2C devices (display, gyro, be creative)
  - GPIOs 18 and 19 expansion port (control buttons)

### lux-et-pcb-V2
- Work in progress
- Changed the processor to ESP32-S3
- Added USB-C Serial interface (no more aditional hardware required for flashing)
- Added Sound output (be funky, no idea if it will work tho)
- Added microSD slot for SDMMC
- Modular schematic organization
- To do: 
  - Add I2S support (crispy sounds)
  - Add gyro support (so the board can do a PEW PEW noise when hit)
  - Remove added latch power-on switch 

## Features

### Core Specifications
- **Microcontroller**: ESP32 (4MB Flash)
- **Power Input**: USB-C connector or 5~24V 5521 connector (good old P4 with positive inside)
- **Display**: 0.96" SSD1306 OLED (128x64, I2C)
- **LED Channels**: 4x addressable WS2812B outputs
- **Power Outputs**: 3x open drain + 1x controlled current
- **Programming**: External USB-to-Serial (CP210x or CH340) or OTA
- **Expansion Ports**: Serial, I2C, GPIOs

### Pinout

#### WS2812B LED Channels
| Channel   | GPIO Pin | Default Use              |
|-----------|----------|--------------------------|
| Channel 0 | GPIO 32  | Onboard LED ring (6 LEDs) |
| Channel 1 | GPIO 33  | External connector        |
| Channel 2 | GPIO 19  | External connector        |
| Channel 3 | GPIO 16  | External connector        |

#### Expansion Ports
| Port  | Connector | Pins                     | Function             |
|-------|-----------|--------------------------|----------------------|
| PORT1 | JST-GH 4P | GND, TX0, RX0, 5V        | Serial/Power         |
| PORT2 | JST-GH 4P | GND, GPIO18, GPIO17, 3V3 | Button/GPIO Inputs   |
| PORT3 | JST-GH 4P | GND, SCL, SDA, 3V3       | I2C Display/Sensors  |

#### Power Outputs
- **CC Output**: GPIO 13 - Constant current output (~700mA) for high-power LEDs
- **Open Drain**: GPIO 27, 26, 25 - For RGB strips or high-current switching

### Key Components
- ESP32-WROOM-32 module
- Programming and reset push buttons
- USB-C connector (GT-USB-7101A, optional)
- JST-GH 1.25mm connectors (4-pin)
- Good quality 220uF 25V 6.3x7.7mm (bad quality ones tend to heat up)
- SSD1306 0.96" OLED display (optional)
- Onboard WS2812B LED ring (optional)
- Onboard 1S lithium battery and charger (optional)
- Onboard LED for 3V3 and battery charging status (optional)
- AO340X N-MOS SOT23 transistors or equivalent that can handle at least 1A for each open collector strip channel

Full list should be on the BoM files, with codes for used component on LCSC

## PCB Design Files

Each hardware version includes:
- **`.kicad_pcb`** - PCB layout file
- **`.kicad_sch`** - Schematic file
- **`.kicad_pro`** - Project configuration
- **`.step`** - 3D model for mechanical integration
- **`production/`** - Manufacturing files (BOM, positions, netlist) for JLCPCB

## Manufacturing

### JLCPCB Assembly Service

The design is optimized for JLCPCB's PCB assembly service:

1. **PCB Fabrication + Assembly**: ~$130 for 5 boards (assembled)
2. **Files Included**:
   - `bom.csv` - Bill of Materials
   - `positions.csv` - Component placement data
   - `netlist.ipc` - Connectivity information
   - `designators.csv` - Component reference designators

### DIY Assembly

For self-assembly:
- **Cost**: $8-15 per board (components only)
- **Recommended**: Solder stencil (~$7) for SMD components, and a reflow hotbed at 250ºC for melting solder paste
- **Skills**: Intermediate soldering (fine-pitch SMD)
- **Time**: ~1-3 hours per board.  

### Component Sourcing

Most components are standard and available from:
- JLCPCB parts library (for assembly service)
- LCSC
- Mouser
- DigiKey
- Aliexpress

Special components:
- **USB-C connector**: GT-USB-7101A (included in design files)
Getting the correct USB-C connector with dozen of options is very hard if sourcing from Aliexpress.
If you do not require USB-C charging, this component can be avoided.

## Getting Started

### Prerequisites

1. **KiCad 8.0 or later** - [Download here](https://www.kicad.org/download/)
2. **3D Models** - Included `.step` files for mechanical clearance verification
3. **Symbol Library** - Custom `USB_C-8p.kicad_sym` included in project

### Generating Manufacturing Files

1. Open the PCB file in KiCad
2. Add the JLCPCB plugin from the Plugin and Content Manager
3. **Tools → External plugins → Fabrication toolkit**
4. Files should be generated on the production folder automagically 

## Assembly Instructions

### Hardware Assembly

0. **Read the issues and notes before assembly!**

1. **Solder SMD Components** (if self-assembling):
   - Use solder paste and stencil
   - Place components with tweezers
   - Reflow with hot air or reflow oven

2. **Through-hole Components**:
   - Cable wires for LED strips, or 2.54 3P terminal block
     - It's advised to use in-line connectors for power output, like the JST-3P
   - DC 5521 connector
   - XH-2.54 2P for battery (no one is stoping you from soldering the battery wires directly, but please try to act civilized (I did that, no judgement))


### Power-Up Checklist

- [ ] Visually inspect all solder joints
- [ ] Check for shorts between power and ground
- [ ] Power the board (no LEDs connected yet)
- [ ] Verify 3.3V rail
- [ ] Verify 5V rail
- [ ] Test USB-to-Serial communication
- [ ] Upload test firmware
- [ ] Watch the white circle blink
- [ ] Configure your WiFi SSID and password on the hotspot page
- [ ] If connected to the WiFi, blue breathing effect on onboard LEDs
- [ ] If !connected to the WiFi, color changing running pixel on onboard LEDs

## Integration with Firmware

The hardware is designed to work with the [Lightpack Firmware](../Firmware/lightpack/). 

**Pin configurations must match** between hardware and firmware - see `Firmware/lightpack/src/shared/pinout.h` for current assignments.

## Connector Wiring

### PORT1 (Serial/Power) - JST-GH 4P
```
Pin 1: GND
Pin 2: ESP32 TX0 (GPIO 1)
Pin 3: ESP32 RX0 (GPIO 3)
Pin 4: 5V (Power In)
```

### PORT2 (GPIO) - JST-GH 4P
```
Pin 1: GND
Pin 2: GPIO 18 (Button A)
Pin 3: GPIO 17 (Button B)
Pin 4: 3V3 Out
```

### PORT3 (I2C) - JST-GH 4P
```
Pin 1: GND
Pin 2: SCL (GPIO 22)
Pin 3: SDA (GPIO 21)
Pin 4: 3V3 Out
```

## Design Considerations

### Power Management
- The switched DC-DC regulator 5V output is limited to 900mA by the 33uH inductor
- The battery charger is set to 500mA
- When powering from DC 5521 connector, avoid drawing more than 300mA from the 5V LEDs to avoid a processor brownout. The processor current draw can peak about 200mA during WiFi usage.
- There's a 500mA resetable fuse for the 3V3 regulator that shunts the current to a safe 200mA in case of a short circuit, but it's response is slow.

## Known Issues & Notes

### V1.0
- Initial production version
- Reverse voltage P-MOS protection is wired wrong (swapped gate and source terminals)
- Board will not receive external nor battery power because of this
- Swap the SOT-223 transistor for an SS14 schottky diode
  
  <img width="400" height="400" alt="image" src="https://github.com/user-attachments/assets/14388e6f-855e-4f7a-8c2a-e56fc158ad2b" />

- Wrong footprints for R8 and R10 0R22 resistors. Used 2010, should have been 1206
- Charge status and 3V3 LEDs too bright, put 5.1kR series resistor instead of 2.2kR or remove them altogether


### Tools
- [KiCad](https://www.kicad.org/) - PCB design software
  - JLCPCB Fabrication toolkit plugin
- [JLCPCB](https://jlcpcb.com/) - PCB manufacturing
- Autodesk Fusion360 - Case moddeling


## Cost Breakdown

### JLCPCB Assembly (5 boards)
- PCB Fabrication: ~$2
- Component Costs: ~$45
- Assembly Service: ~$85
- **Total**: ~$130 + shipping

### Self-Assembly (per board)
- PCB only: ~$3 (in batch of 5)
- Components: $8-15
- **Total**: ~$10-18 per board

**Note**: Always verify pin configurations match between hardware version and firmware before uploading code. Check the schematic and `pinout.h` for current assignments.

# Lightpack Firmware

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)
[![Version](https://img.shields.io/badge/Version-1.0-green.svg)](https://github.com/machadoleonardo94/lux-et-tenebris)

Firmware for the **Lux et Tenebris Devkit V1.0** - an ESP32-based LED controller with OLED display, WiFi connectivity, and over-the-air (OTA) update capabilities.


## Features

- **Multi-Channel WS2812B LED Control**: Control up to 4 independent addressable LED channels
- **Open Drain Outputs**: 3 dedicated open drain outputs (GPIO 25, 26, 27) for driving RGB LED strips or other high-current loads
- **Controlled Current Output**: Constant current output (GPIO 13) for those power hungry LEDs. Also good for short-circuit protection, as the voltage drop protects your wiring and power source. Good for sensitive and prone to failure props. 
- **WiFi Connectivity**: Built-in WiFi manager for easy network configuration
- **OTA Updates**: Update firmware wirelessly without Serial connection
- **OLED Display**: 0.96" SSD1306 I2C display for status information
- **LED Effects**: Dynamic LED animations including breathing effects, color cycles, and custom patterns
- **Power Management**: Watchdog timer and deep sleep mode support
- **User Interface**: Physical button support with debouncing and long-press detection. Good for wiring a controller for switching between animations
- **Non-Volatile Storage**: Persistent WiFi credentials and settings storage

###  Demo Videos

Showcase of base firmware, case interaction, and sine wave composition as lightshow:

#### Base Board Demo
Overview of the base board functionality

https://github.com/user-attachments/assets/8617b1a9-34ca-48c3-8088-12f21609c16c


#### Two Channels Example
Dual LED channel control demonstration

https://github.com/user-attachments/assets/9bc4d82b-29f8-49ef-8373-876afc4fa7dd


#### WiFi Toggle
WiFi configuration and toggle features


https://github.com/user-attachments/assets/11826e12-3717-4bb9-bba7-9af42934ba8d


## Future upgrades:

## Hardware Requirements

### Main Components
- **ESP32 Development Board** (4MB Flash)
- **SSD1306 OLED Display** (128x64, I2C)
- **WS2812B LED Strips** (up to 4 channels)
- **RGB LED Strips** (compatible with open drain outputs)
- **Push Buttons** (1x programming switch, 1x reset, 2x optional for external buttons )

### Pinout Configuration

#### LED Channels
| Channel   | GPIO Pin | Description |
|-----------|----------|-------------|
| Channel 0 | GPIO 32  | Onboard LED ring (6 LEDs) |
| Channel 1 | GPIO 33  | To be defined             |
| Channel 2 | GPIO 19  | Majora's Mask (28 LEDs)   |
| Channel 3 | GPIO 16  | To be defined             |

#### I/O Ports
| Port  | GPIO Pins        | Function             |
|-------|------------------|----------------------|
| PORT1 | GND, TX0(1), RX0(3), 5V in   | Serial Communication |
| PORT2 | GND, 18, 17, 3V3 out         | Button Inputs (A, B) |
| PORT3 | GND, 22(SCL),21(SDA), 3V3 out | I2C Bus (Display)   |

#### Additional I/O
- **CC Output**: GPIO 13 - Controlled current output for power hungry LED
- **Open Drain Outputs**: GPIO 27, 26, 25 - For driving RGB LED strips or other high-current loads

## Getting Started

### Prerequisites

0. **Getting your hardware**
- The devkit hardware is [linked here](https://github.com/machadoleonardo94/lux-et-tenebris/tree/main/Hardware/lux-et-pcb)
- Build your own devkit with the sourced list
- Getting 5 soldered boards from JLCPCB should cost about $130 plus shipping
- Soldering the componets yourself would cost $8~15 per board, and a solder stencil ($7) is higly recomended if going through that route.
- Print your case with the provided STL files
- Assemble a JST1.25 4P connector with +5V, GND, TX and RX pins to power-up the board

1. **PlatformIO IDE** or **PlatformIO Core**
   - Install via [VS Code Extension](https://platformio.org/install/ide?install=vscode) or
   - Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html)

2. **USB Drivers** for USB-Serial converter (CP210x or CH340)

### Installation

1. Clone the repository:
```bash
git clone https://github.com/machadoleonardo94/lux-et-tenebris.git
cd lux-et-tenebris/Firmware/lightpack
```

2. Build the project:
```bash
platformio run
```

3. Upload via USB (first time):
- Connect your serial header to the 
- Put the device on download mode (Holding the prog button during a reboot)
```bash
platformio run --target upload
```

4. Monitor serial output:
```bash
platformio device monitor
```

## 📡 WiFi Configuration

### First-Time Setup

1. Power on the device
2. Connect to the WiFi Access Point: **Lightpack-Devkit**
3. A captive portal will open automatically (or navigate to `192.168.4.1`)
4. Enter your WiFi credentials and save
5. Device will reconnect automatically on next boot

### Saved Credentials

WiFi credentials are stored in non-volatile memory (NVS). The device will automatically attempt to reconnect to the saved network on startup.

## 🔄 OTA Updates

Once connected to WiFi, you can update the firmware wirelessly:

### Update Configuration

Edit `platformio.ini` to set your device IP or hostname:
```ini
upload_protocol = espota
upload_port = Lightpack-Devkit.local  ; or IP address (e.g., 192.168.3.24)
```

### Upload OTA

```bash
platformio run --target upload
```

## 🎨 LED Effects

### Onboard LED Ring (Channel 0)
- **Circular Chase Effect**: Single LED travels around the ring with fade trail when offline.
- **Color Cycling**: Automatically cycles through 7 colors (R, G, B, RG, RB, GB, RGB)
- **Breathing effect**: Pulses blue color if connected to the WiFi
- **Update Rate**: 10ms per step

### Majora's Mask LEDs (Channel 2)
- **Breathing Effect**: Smooth sine wave modulation on R, G, B channels
- **Multi-Frequency**: 3 independent sine waves at different periods (1.5s, 4s, 1.2s)
- **Update Rate**: 10ms

### Custom LED Scripts
LED behaviors are defined in `src/services/led_scripts.h` and can be customized for your application.

## 🛠️ Project Structure

```
lightpack/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Main application entry point
│   ├── components/        # Hardware component implementations
│   │   ├── DISPLAY/       # OLED display setup
│   │   ├── ESP32/         # ESP32 configuration & OTA
│   │   └── WS2812B/       # LED strip setup
│   ├── services/          # Application services
│   │   ├── led_scripts.h      # LED animation effects
│   │   ├── user_interface.h   # Button handling & UI
│   │   ├── wifi_settings.h    # WiFi management
│   │   └── serial_outputs.h   # Serial debugging
│   └── shared/            # Shared definitions
│       ├── constants.h        # Global constants
│       ├── dependencies.h     # Library includes
│       ├── library_objects.h  # Global instances
│       ├── pinout.h          # Pin definitions
│       └── variables.h       # Global variables
├── include/              # Additional headers
└── lib/                 # Custom libraries
```

## 🔧 Configuration

### Build Flags
The firmware is compiled with optimization level `-Os` (optimize for size) and core debug level 2.

### Board Configuration
- **Platform**: Espressif32 @ 6.11.0
- **Board**: ESP32 Dev Module
- **Flash Size**: 4MB
- **Partitions**: min_spiffs.csv
- **Upload Speed**: 921600 baud
- **Monitor Speed**: 115200 baud
If your upload device cannot handle 921600, reduce it to 115200. It will upload slower, but consistently for most USB-Serial converters

## 📚 Dependencies

All dependencies are automatically managed by PlatformIO:

- **Adafruit GFX Library** @ 1.12.1
- **Adafruit SSD1306** @ 2.5.13
- **Adafruit BusIO** @ 1.17.0
- **Adafruit NeoPixel** @ 1.12.5
- **WiFiManager** @ 2.0.17
- **ArduinoOTA** (built-in)

## 🐛 Debugging

### Serial Monitor Output

The firmware provides detailed serial output at **115200 baud**:
- Setup sequence information
- WiFi connection status
- OTA update progress
- LED effect status
- Button press events

### Watchdog Timer

The ESP32 watchdog timer is enabled and must be reset regularly. The firmware calls `esp_task_wdt_reset()` in both `setup()` and `loop()`.

## ⚡ Power Management

### Deep Sleep Mode

Press and hold the **programming switch (GPIO 0)** for more than 1 second to enter deep sleep mode:
1. LED ring blinks white 5 times
2. All LEDs turn off
3. Device enters deep sleep
4. Press reset or prog button to wake

### Power Consumption

For lower power consumption, you can disable WiFi via clicking tree times on the prog button.
For turning it back on, simply click tree times again.
Do remember that disabling the WiFi modem also disables the bridge to OTA, so turn it back on if you need to update firmware.

This project is part of the **Lux et Tenebris** ecosystem.

## 👨‍💻 Author

**Leonardo Machado** - [@machadoleonardo94](https://github.com/machadoleonardo94)

## 📝 Version History

- **v1.0** - Initial release
  - WiFi connectivity with WiFiManager
  - OTA update support
  - Multi-channel LED control
  - OLED display integration
  - Power management features

---

**Note**: This firmware is designed specifically for the Lux et Tenebris Devkit V1.0. Pin configurations and features may vary for other hardware versions.

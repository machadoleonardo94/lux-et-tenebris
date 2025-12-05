# Lux et Tenebris

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org/)
[![KiCad](https://img.shields.io/badge/KiCad-8.0-blue.svg)](https://www.kicad.org/)
[![Version](https://img.shields.io/badge/Version-2.0-green.svg)](https://github.com/machadoleonardo94/lux-et-tenebris)

**Lux et Tenebris** is an open-source ESP32-based LED controller development platform featuring custom PCB hardware, firmware, and support for addressable LEDs, RGB strips, and various lighting effects.

## Disclaimer

### Most of the readme.md present here was writen by Clanker, as a proof of concept for fast doccumentation generation. It may not contain the newest features, but it's good enough as a placeholder. Considering that it took 2 prompts and minor fixes, time that would've been used on this document was allocated elsewere more productive, like the main features.

But you know what? Using Clankers' template made the doccumentation much more enjoyable, and ended up with way more content than all of my previous projects combined, as after reading it I've managed to add tidbits that I wouldn't have written otherwise.

## Demo Video

https://github.com/user-attachments/assets/9bc4d82b-29f8-49ef-8373-876afc4fa7dd


### [Hardware](Hardware/)
Custom PCB designs created in KiCad for the Lux et Tenebris devkit. Includes production-ready files for JLCPCB assembly.

**[📖 Hardware Documentation](Hardware/README.md)**

**Features:**
- ESP32-based design (4MB Flash)
- USB-C connectivity
- 4x WS2812B addressable LED channels
- 3x Open drain outputs + 1x controlled current output
- Integrated OLED display (SSD1306)
- Multiple expansion ports (Serial, I2C, GPIO)
- Production files ready for manufacturing

### [Firmware](Firmware/lightpack/)
Arduino-based firmware for ESP32 with WiFi connectivity, OTA updates, and dynamic LED effects.

**[ Firmware Documentation](Firmware/lightpack/README.md)**

**Features:**
- WiFi connectivity with captive portal configuration
- Over-the-air (OTA) firmware updates
- Multi-channel WS2812B LED control with effects
- OLED display integration
- Power management and deep sleep support
- User interface with button controls
- Non-volatile storage for settings

##  Key Features

### Hardware Capabilities
- **4 Independent addressable LED Channels**: Control multiple WS2812B strips simultaneously
- **High-Current Outputs**: Open drain and constant current outputs for various LED types
- **Expandable Design**: JST-GH connectors for easy expansion
- **Compact Form Factor**: Optimized between integration into props and projects, and ease to build.

### Firmware Capabilities
- **Dynamic LED Effects**: Breathing, color cycling, chase effects, and custom patterns
- **WiFi Management**: Easy network configuration via captive portal
- **OTA Updates**: Update firmware wirelessly without USB connection
- **Display Integration**: Real-time status information on OLED
- **Power Efficient**: Deep sleep mode and WiFi toggle for battery applications
- **Customizable**: Modular code structure for easy modification

##  Use Cases

- **Cosplay Props**: Control LEDs in costumes, masks, and accessories
- **Art Installations**: Multi-channel LED art with dynamic effects
- **Home Automation**: WiFi-connected lighting control
- **Prototyping**: Development platform for LED-based projects
- **Educational**: Learn ESP32, LED control, and PCB design

## 📚 Documentation

- **[Hardware Documentation](Hardware/README.md)**: PCB design, assembly, and manufacturing
- **[Firmware Documentation](Firmware/lightpack/README.md)**: Setup, configuration, and API reference
- **Pinout Reference**: See firmware README for complete pin assignments
- **Manufacturing Files**: BOM, positions, and Gerbers in `Hardware/*/production/`

## 🧰 Technology Stack

### Hardware
- **PCB Design**: KiCad 8.0
- **Microcontroller**: ESP32-WROOM-32
- **Manufacturing**: JLCPCB-ready production files
- **Components**: Standard 0805 SMD parts for easy sourcing

### Firmware
- **Framework**: Arduino (PlatformIO)
- **Platform**: Espressif32
- **Language**: C++
- **Libraries**: Adafruit GFX, NeoPixel, WiFiManager, ArduinoOTA

## 💡 Getting Help

- **Hardware Issues**: See [Hardware README](Hardware/README.md)
- **Firmware Issues**: See [Firmware README](Firmware/lightpack/README.md)
- **Pin Configurations**: Check `Firmware/lightpack/src/shared/pinout.h`
- **GitHub Issues**: [Report bugs or request features](https://github.com/machadoleonardo94/lux-et-tenebris/issues)

##  Acknowledgments

Special thanks to:
- The ESP32 and Arduino communities
- Espressif 
- KiCad development team
- PlatformIO for the excellent development platform
- All contributors and users of this project

---

**Note**: This is an active development project. Hardware and firmware specifications may change. Always refer to the specific version documentation when building or programming your device.

# ESP32-S3 MP3 Streaming Receiver

This firmware is designed for a **custom commercial board** based on the **ESP32-S3** microcontroller. It enables wireless **MP3 audio streaming**, **decoding**, and **digital-to-analog conversion** using the **PCM5102 DAC**. Built using the Arduino IDE in C++, it leverages multiple open-source libraries for audio, networking, filesystem, and configuration management.

## 🎵 Description

This project turns the ESP32-S3 into a compact, network-enabled MP3 receiver. It supports:

- Streaming MP3 audio over Wi-Fi (e.g., from an internet radio or LAN server)
- Decoding MP3 audio using the Helix decoder
- Sending decoded audio to a PCM5102 DAC via I2S
- Serving a web interface or WebSocket for control/status
- Saving settings persistently via Preferences
- Managing files via LittleFS if needed

## 🖼️ Device Image

![ESP32-S3 Audio Board](https://github.com/ulivee/audiocast_v2/blob/main/pcb_render.png)


## 📦 Features

- ✅ MP3 decoding with Helix library
- ✅ Streaming via HTTP or WebSocket
- ✅ High-quality output via I2S to PCM5102
- ✅ Embedded web server for control
- ✅ JSON-based configuration
- ✅ Persistent settings with Preferences
- ✅ Real-time clock support via Time library

## 🧰 Hardware Requirements

- ESP32-S3 based board (custom/commercial)
- PCM5102 DAC connected via I2S
- Optional: buttons, display, etc.

### DAC Connection (example)

| DAC Pin | ESP32-S3 Pin |
|---------|---------------|
| DIN     | GPIO 26       |
| BCK     | GPIO 27       |
| LCK     | GPIO 25       |
| GND     | GND           |
| VCC     | 3.3V          |

## 🧪 Library Dependencies

These open-source libraries are used in this firmware:

| Library               | Description              | Author         | Repository |
|------------------------|--------------------------|----------------|------------|
| `arduino-audio-tools` | Audio processing         | pschatzmann    | [GitHub](https://github.com/pschatzmann/arduino-audio-tools) |
| `arduino-libhelix`    | MP3 decoder              | pschatzmann    | [GitHub](https://github.com/pschatzmann/arduino-libhelix) |
| `Time`                | Timekeeping              | Paul Stoffregen| [GitHub](https://github.com/PaulStoffregen/Time) |
| `Preferences`         | Persistent storage       | Espressif      | [GitHub](https://github.com/espressif/arduino-esp32) *(built-in)* |
| `ArduinoJson`         | JSON parsing             | Benoît Blanchon| [GitHub](https://github.com/bblanchon/ArduinoJson) |
| `LittleFS`            | Filesystem (SPIFFS)      | lorol          | [GitHub](https://github.com/lorol/LITTLEFS) |
| `WiFi`                | Wi-Fi connectivity       | Arduino        | [GitHub](https://github.com/espressif/arduino-esp32) *(built-in)* |
| `HTTPClient`          | HTTP requests            | amcwen         | [GitHub](https://github.com/espressif/arduino-esp32) *(built-in)* |
| `WebServer`           | Web server functionality | Espressif      | [GitHub](https://github.com/espressif/arduino-esp32) *(built-in)* |
| `arduinoWebSockets`   | WebSocket communication  | gilmaimon      | [GitHub](https://github.com/gilmaimon/ArduinoWebsockets) |

## 🔧 Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/your-username/esp32s3-mp3-streamer.git


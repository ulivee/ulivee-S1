# ESP32-S3 MP3 Streaming Receiver

This firmware is designed for **ESP32-S3** microcontroller and **PCM5102 DAC**. 

It handles wireless **MP3 audio streaming**, **decoding**, and **digital-to-analog conversion** controlled from a websocket a server.

## Setup

This code is prepared to control an ESP32 audio player with websockets, so is needed to setup main code like next example. Can also customize  led pins, led intervals and stream and network check intervals. Remember to check I2S bus pins on startAudioStream() function.

```
// ===== Wi-Fi & Network Configuration =====
const char* apSSID = "ULivee-S1";
const char* apPassword = "12345678"; // Minimum 8 characters for WPA2
const unsigned long wifiCheckInterval = 5000;  // Check Wi-Fi every 5s
const char* apiUrl = "http://192.168.5.110:3001/api/v1/audiocasts/register"; 
const char* websockets_server_host = "192.168.5.110"; //Enter server adress
const uint16_t websockets_server_port = 3001; // Enter server port
const char* websockets_server_path = "/api/v1/cable";


// ===== Audio & Hardware Config =====
float currentVolume = 0.2;  // Default volume (0.0 to 1.0)
const int wifiLedPin = 38;       // Wi-Fi status LED
const int streamLedPin = 39;     // Streaming activity LED


// ===== Timing & Intervals =====
#define WIFI_BLINK_INTERVAL 1000  // Wi-Fi LED blink rate (disconnected)
#define STREAM_BLINK_INTERVAL 50  // Stream LED blink rate (active)
const unsigned long streamCheckInterval = 1000;  // Check stream every 1s
const unsigned long connectRetryInterval = 5000; // WebSocket retry every 5s
```



## How to use

1.- Dynamic network configuration by this POST request on ESP32 AP webserver: 

   http://192.168.4.1/connect?ssid=${wifiSSID}&password=${wifiPassword}&name=${audiocastName}&token=${token}

   wifiSSID: your wifi SSID (Mandatory)
   wifiPassword: your wifi password (Mandatory)
   audiocastName: used to name the device (Optional)
   token: used for link token to user acount (Optional)

2.- When ESP32 has access to network will connect to websocket server. Now can be controlled with these actions:
   - {"action": "play", "url": streamUrl, "duration": duration, "offset": offset}
   - {"action": "pause"}
   - {"action": "volume", "value": newValue}
   - {"action": "status"}
   - {"action": "reset"}

   


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

### Serial Connection for firmware upload

| PCB Pin | External Uart |
|---------|---------------|
| TXD     | RX            |
| RXD     | TX            |
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
   git clone https://github.com/ulivee/ULivee-S1.git


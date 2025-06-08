# 🎵 ESP32-S3 MP3 Streaming Receiver

This firmware is built for the **ESP32-S3** microcontroller with **PCM5102 DAC**.

It supports wireless **MP3 audio streaming**, **decoding**, and **digital-to-analog output**, controlled via **WebSocket** commands. All code is in `main.ino`.

The system supports both online radio streams and custom-hosted audio served via HTTP 206 Partial Content. For example, I'm using AWS S3 bucket in combination with Rails controller to deliver streamed content to ESP32.

---

## ⚙️ Setup & Configuration

This code allows full control of the ESP32 audio player through WebSockets. You can customize LED pins, blink intervals, stream checks, and network settings.

🛠 Make sure to review and configure the **I2S pins** inside the `startAudioStream()` function.

### 🔧 Main Configuration Example

```cpp
// ===== Wi-Fi & Network Configuration =====
const char* apSSID = "ULivee-S1";
const char* apPassword = "12345678";  // WPA2 requires at least 8 characters
const unsigned long wifiCheckInterval = 5000;  // Check Wi-Fi every 5s

const char* apiUrl = "http://192.168.7.42:8080/api/v1/devices/register"; // Api endpoint to register this device
const char* websockets_server_host = "192.168.7.42";     // WebSocket server IP
const uint16_t websockets_server_port = 8080;            // WebSocket port
const char* websockets_server_path = "/ws/connect";      // WebSocket path


// ===== Audio & Hardware Config =====
float currentVolume = 0.2;         // Default volume (0.0 to 1.0)
const int wifiLedPin = 38;         // Wi-Fi status LED
const int streamLedPin = 39;       // Streaming activity LED

// ===== Timing & Intervals =====
#define WIFI_BLINK_INTERVAL 1000   // Wi-Fi LED blink rate (disconnected)
#define STREAM_BLINK_INTERVAL 50   // Stream LED blink rate (active)
const unsigned long streamCheckInterval = 1000;    // Check stream every 1s
const unsigned long connectRetryInterval = 5000;   // Retry WebSocket every 5s
```



## 🚀 Usage

### 1️⃣ Connect to Wi-Fi (Dynamic Network Configuration)

Once the ESP32 starts in **Access Point (AP) mode**, you can configure its Wi-Fi settings by sending a **GET request** to the following URL:

```
http://192.168.4.1/connect?ssid=<wifiSSID>&password=<wifiPassword>&name=<audiocastName>&token=<token>
```

#### 🔧 Parameters

| Parameter  | Description                                            | Required |
| ---------- | ------------------------------------------------------ | -------- |
| `ssid`     | Your Wi-Fi network name (SSID)                         | ✅ Yes    |
| `password` | Your Wi-Fi password (min. 8 characters for WPA2)       | ✅ Yes    |
| `name`     | Optional name for the device (audiocastName)           | ❌ No     |
| `token`    | Token to link device with user account (for API usage) | ❌ No     |


> 📝 **Note:** If you don’t want to register the device with an API, you can bypass on `setupAPRoutes()` by ending on `/success` endpoint just after `handleWifiConnectionLogic()`. If you leave it and the registration fails, the ESP32 will automatically reboot. 

To prevent this issue, update the redirect path in the handleConnectingToWifi() function by replacing /link with /success, as shown below:
```js

<script>
  window.onload = function () {
    fetch("/do_connect")
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          // Redirect to the success page
          window.location.href = "/success";
        } else {
          // Redirect to failure page if Wi-Fi connection fails
          window.location.href = "/wifi_fail";
        }
      })
      .catch(() => {
        // Redirect to failure page if request fails
        window.location.href = "/wifi_fail";
      });
  };
</script>
```


---

### 2️⃣ Control the Device via WebSocket

Once connected to Wi-Fi and the WebSocket server, the ESP32 can receive JSON-based control commands.

#### 🎮 Available WebSocket Commands

```json
{ "action": "play", "url": "<streamUrl>", "duration": 120, "offset": 0 }
{ "action": "pause" }
{ "action": "volume", "value": 0.5 }
{ "action": "status" }
{ "action": "reset" }
```

| Action   | Description                                                                                  |
| -------- | -------------------------------------------------------------------------------------------- |
| `play`   | Starts streaming from the given `url` with given `duration` on seconds and optional `offset` |
| `pause`  | Pauses the current playback                                                                  |
| `volume` | Sets the volume level (range: `0.0` to `1.0`)                                                |
| `status` | Requests current status of the player                                                        |
| `reset`  | Reboots the device                                                                           |

---


## 🖼️ Hardware used on this project

<img src="https://github.com/ulivee/audiocast_v2/blob/main/pcb.jpg" alt="ESP32-S3 Audio Board" width="50%" />




## 🧰 Hardware Requirements

- ESP32-S3 based board (custom/commercial)
- PCM5102 DAC connected via I2S
- Leds (Optional)
- Buttons (Only for booting ESP32) 


### Serial Connection for firmware upload

| PCB Pin | External Uart |
|---------|---------------|
| TXD     | RX            |
| RXD     | TX            |
| GND     | GND           |
| VCC     | 3.3V          |

or USB via UART bridge

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




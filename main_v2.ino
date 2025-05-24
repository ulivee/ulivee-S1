#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h> // https://github.com/espressif/arduino-esp32/blob/master/libraries/Preferences/src/Preferences.h
#include <time.h> // https://github.com/PaulStoffregen/Time
#include "AudioTools.h" // https://github.com/pschatzmann/arduino-audio-tools
#include "AudioTools/AudioCodecs/CodecMP3Helix.h" // https://github.com/pschatzmann/arduino-libhelix

// Volume control
// https://github.com/pschatzmann/arduino-audio-tools/wiki/Volume-Control

// Main example
// https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-communication/http-client/streams-url_mp3_helix-i2s/streams-url_mp3_helix-i2s.ino


// AP Configuration (hardcoded)
const char* apSSID = "ULivee-S1";
const char* apPassword = "12345678"; // Minimum 8 characters for WPA2
// Replace with your Rails serv
const char* apiUrl = "http://192.168.5.110:3001/api/v1/audiocasts/register"; 
// Env variables unmutable for admin connection
const char* audiocastSecret = "123456789";
unsigned long lastWiFiCheckTime = 0;
const unsigned long wifiCheckInterval = 5000; // Check WiFi every 5 seconds


float currentVolume = 0.2;  // Default volume (0.0 to 1.0)


// LED pins
const int wifiLedPin = 38;
const int streamLedPin = 39;
// Timing variables for LED blinking
unsigned long lastWifiBlink = 0;
unsigned long lastStreamBlink = 0;
bool wifiLedState = false;
bool streamLedState = false;

unsigned long lastStreamCheckTime = 0;
const unsigned long streamCheckInterval = 500; // Check every 500ms



WebServer server(80);
Preferences preferences;
URLStream* audioStream = nullptr;
I2SStream* i2sOutput = nullptr;
VolumeStream* volumeControl = nullptr;
EncodedAudioStream* decoder = nullptr;
StreamCopy* streamCopier = nullptr;


bool connectToWiFi();
bool registerWithRailsAPI();



// ************************************************************************
// *                             Preferences                              *
// ************************************************************************


void storeStreamInfo(const String& url, uint32_t start_time, uint32_t duration) {
  uint32_t end_time = start_time + duration;
  
  if(!preferences.putString("url", url)) {
    Serial.println("Failed to store URL");
  }
  if(!preferences.putUInt("start_time", start_time)) {
    Serial.println("Failed to store start time");
  }
  if(!preferences.putUInt("duration", duration)) {
    Serial.println("Failed to store duration");
  }
  if(!preferences.putUInt("end_time", end_time)) {
    Serial.println("Failed to store end time");
  }
  
  printPreferences(); // Debug output
}


void storeUserInfo(const String& wifi_ssid, const String& wifi_password, const String& linking_token, const String& audiocast_name) {

  // Every time device is reconfigured, new secret code is created and stored. It is to make HTTP secure.
  String secret = generateRandomSecret();

  if(!preferences.putString("wifi_ssid", wifi_ssid)) {
    Serial.println("Failed to store wifi_ssid");
  }
  if(!preferences.putString("wifi_password", wifi_password)) {
    Serial.println("Failed to store wifi_password");
  }
  if(!preferences.putString("linking_token", linking_token)) {
    Serial.println("Failed to store linking_token");
  }
  if(!preferences.putString("audiocast_name", audiocast_name)) {
    Serial.println("Failed to store audiocast_name");
  }
  if(!preferences.putString("secret", secret)) {
    Serial.println("Failed to store secret");
  }
  
  printPreferences(); // Debug output
}


void loadStreamInfo(String &url, uint32_t &start_time, uint32_t &duration, uint32_t &end_time) {
  url = preferences.getString("url", "");
  start_time = preferences.getUInt("start_time", 0);
  duration = preferences.getUInt("duration", 0);
  end_time = preferences.getUInt("end_time", 0);
}

void loadUserInfo(String &linking_token, String &audiocast_name, String &secret) {
  linking_token = preferences.getString("linking_token", "");
  audiocast_name = preferences.getString("audiocast_name", "");
  secret = preferences.getString("secret", "");
}

void loadWifiInfo(String &wifi_ssid, String &wifi_password) {
  wifi_ssid = preferences.getString("wifi_ssid", "");
  wifi_password = preferences.getString("wifi_password", "");
}

// Only for debugging
void printPreferences() {
  Serial.println("\nCurrent Preferences:");
  Serial.printf("------------ Stream Data ------------------");
  Serial.printf("URL: %s\n", preferences.getString("url", "").c_str());
  Serial.printf("Start Time: %u\n", preferences.getUInt("start_time", 0));
  Serial.printf("Duration: %u\n", preferences.getUInt("duration", 0));
  Serial.printf("End Time: %u\n", preferences.getUInt("end_time", 0));
  Serial.printf("Free Entries: %d\n", preferences.freeEntries());
  Serial.printf("------------ User Data ------------------");
  Serial.printf("wifi_ssid: %s\n", preferences.getString("wifi_ssid", "").c_str());
  Serial.printf("wifi_password: %s\n", preferences.getString("wifi_password", "").c_str());
  Serial.printf("linking_token: %s\n", preferences.getString("linking_token", "").c_str());
  Serial.printf("audiocast_name: %s\n", preferences.getString("audiocast_name", "").c_str());
  Serial.printf("secret: %s\n", preferences.getString("secret", "").c_str());
  
}



// ************************************************************************
// *                                Utils                                 *
// ************************************************************************


// Get current Unix timestamp (UTC)
uint32_t getCurrentTime() {
  time_t now;
  time(&now); // This fills the 'now' variable with current UTC time
  return (uint32_t)now; // Explicit cast to uint32_t
}

void sendJsonResponse(int code, const JsonDocument& doc) {
  String response;
  serializeJson(doc, response);
  server.send(code, "application/json", response);
}


String generateRandomSecret() {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const int secretLength = 32;
  String secret = "";
  
  for (int i = 0; i < secretLength; i++) {
    secret += charset[esp_random() % (sizeof(charset) - 1)];
  }
  
  Serial.print("Generated Secret: ");
  Serial.println(secret);
  return secret;
}


void isStreamFinished() {
  uint32_t end_time = preferences.getUInt("end_time", 0);
  uint32_t now = getCurrentTime();
  if (now > end_time) {
    pauseAudioStream();
  }
}


// ************************************************************************
// *                               Helpers                                *
// ************************************************************************

// --- CORS Helper ---
void enableCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}


// --- CORS Preflight Handler ---
void handleCORSOptions() {
  enableCORS();
  server.send(204); // No Content
}


// --- JSON Response Helper ---
void sendJsonResponse(int statusCode, DynamicJsonDocument& doc) {
  String response;
  serializeJson(doc, response);
  enableCORS();
  server.send(statusCode, "application/json", response);
}


// ************************************************************************
// *                             Stream handlers                          *
// ************************************************************************


void startAudioStream(uint32_t offset = 0) {
  

  String wifi_ssid, wifi_password;
  loadWifiInfo(wifi_ssid, wifi_password);

  // Initialize audio pipeline
  audioStream = new URLStream(wifi_ssid.c_str(), wifi_password.c_str());
  i2sOutput = new I2SStream();
  
  // Configure I2S
  auto config = i2sOutput->defaultConfig(TX_MODE);
  config.pin_ws = 10;
  config.pin_data = 11;
  config.pin_bck = 12;
  i2sOutput->begin(config);

  // Setup volume and decoder
  volumeControl = new VolumeStream(*i2sOutput);
  auto vcfg = volumeControl->defaultConfig();
  vcfg.copyFrom(config);
  volumeControl->begin(vcfg);
  volumeControl->setVolume(currentVolume);  // Set to current volume level
  decoder = new EncodedAudioStream(volumeControl, new MP3DecoderHelix());
  decoder->begin();

  // Build URL with offset parameter
  String fullUrl = preferences.getString("url").c_str();
  if (offset > 0) {
    fullUrl += "?offset=" + String(offset);
    Serial.println("Starting with offset: " + String(offset) + "s");
  }

  // Start stream
  if (audioStream->begin(fullUrl.c_str(), "audio/mp3")) {
    streamCopier = new StreamCopy(*decoder, *audioStream);
    Serial.println("Stream started successfully");
  } else {
    Serial.println("Failed to start stream");
    pauseAudioStream();
  }
}


void pauseAudioStream() {
  if (streamCopier) {
    // Clean up audio objects
    delete streamCopier;
    delete decoder;
    delete volumeControl;
    delete i2sOutput;
    delete audioStream;
    
    streamCopier = nullptr;
    decoder = nullptr;
    volumeControl = nullptr;
    i2sOutput = nullptr;
    audioStream = nullptr;

    // Clear preferences
    if(!preferences.remove("url")) {
      Serial.println("Failed to remove URL preference");
    }
    if(!preferences.remove("start_time")) {
      Serial.println("Failed to remove start_time preference");
    }
    if(!preferences.remove("duration")) {
      Serial.println("Failed to remove duration preference");
    }
    if(!preferences.remove("end_time")) {
      Serial.println("Failed to remove end_time preference");
    }

    
    // Debug output
    Serial.println("Audio stream stopped and preferences cleared");
    printPreferences(); // Show empty preferences
  }
}



void handleStreamReconnect() {

  printPreferences();

  String url;
  uint32_t start_time, duration, end_time;
  loadStreamInfo(url, start_time, duration, end_time);

  if (url.length() > 0) {
    Serial.println("Stored URL, streaminf didn't ended correctly.");
    uint32_t now = getCurrentTime();
    uint32_t offset = (now > start_time) ? (now - start_time) : 0;

    if (now < end_time) {
      Serial.println("Reconecting to stream...");
      startAudioStream(offset);
    } else {
      Serial.println("Stream has ended.");

      pauseAudioStream();

      // Debug output
      Serial.println("Audio stream stopped and preferences cleared");
      printPreferences(); // Show empty preferences
    }

  } else {
    Serial.println("No stream to reconnect to.");
  }
}






// ************************************************************************
// *                             STA Endpoints                            *
// ************************************************************************


// Assumes enableCORS() adds CORS headers before sending response
// sendJsonResponse() calls enableCORS() internally and sends JSON response

void handlePlay() {
  // Validate required parameters
  if (!server.hasArg("url") || !server.hasArg("duration")) {
    DynamicJsonDocument errorDoc(128);
    errorDoc["error"] = "Missing required parameters (url, duration)";
    sendJsonResponse(400, errorDoc);
    return;
  }

  pauseAudioStream();

  printPreferences(); // Debug

  String stream_url = server.arg("url");
  uint32_t start_time = getCurrentTime();
  uint32_t duration = server.arg("duration").toInt();
  uint32_t offset = server.hasArg("offset") ? server.arg("offset").toInt() : 0;
  start_time = start_time - offset;

  storeStreamInfo(stream_url, start_time, duration);
  printPreferences(); // Debug

  startAudioStream(offset);

  DynamicJsonDocument doc(256);
  doc["status"] = streamCopier ? "playing" : "ready";
  doc["url"] = stream_url;
  doc["duration"] = duration;
  doc["offset"] = offset;

  sendJsonResponse(200, doc);
}



void handlePause() {
  pauseAudioStream();

  DynamicJsonDocument doc(128);
  doc["status"] = "success";
  doc["message"] = "Stream paused";
  sendJsonResponse(200, doc);
}

void handleVolume() {
  DynamicJsonDocument doc(256);

  if (!server.hasArg("level")) {
    doc["status"] = "error";
    doc["message"] = "Missing level parameter";
    sendJsonResponse(400, doc);
    return;
  }

  float newVolume = server.arg("level").toFloat();
  if (newVolume < 0.0 || newVolume > 1.0) {
    doc["status"] = "error";
    doc["message"] = "Volume must be between 0.0 and 1.0";
    sendJsonResponse(400, doc);
    return;
  }

  currentVolume = newVolume;
  if (volumeControl) {
    volumeControl->setVolume(currentVolume);
  }

  doc["status"] = "success";
  doc["message"] = "Volume updated";
  doc["volume"] = currentVolume;
  sendJsonResponse(200, doc);
}

void handleStatus() {
  DynamicJsonDocument doc(256);

  String url;
  uint32_t start_time, duration, end_time;
  loadStreamInfo(url, start_time, duration, end_time);

  uint32_t now = getCurrentTime();
  uint32_t remaining_time = (end_time > now) ? (end_time - now) : 0;
  uint32_t played_time = (now > start_time) ? (now - start_time) : 0;

  doc["status"] = streamCopier ? "playing" : "connected";
  doc["url"] = url;
  doc["duration"] = duration;
  doc["start_time"] = start_time;
  doc["end_time"] = end_time;
  doc["played_time"] = played_time;
  doc["remaining_time"] = remaining_time;

  sendJsonResponse(200, doc);
}

void handleResetPreferences() {
  if (server.method() != HTTP_POST) {
    server.sendHeader("Allow", "POST");
    enableCORS();
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  preferences.clear();

  DynamicJsonDocument doc(256);
  doc["status"] = "success";
  doc["message"] = "All preferences data cleared";

  sendJsonResponse(200, doc);

  Serial.println("All preferences data has been reset");
  printPreferences();

  ESP.restart();
}


// ************************************************************************
// *                             AP Endoints                              *
// ************************************************************************


// AP IP always should be the same: http://192.168.4.1
void startAPMode() {

  WiFi.softAP(apSSID, apPassword);
  Serial.print("AP Mode started. SSID: ");
  Serial.println(apSSID);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

}




void handleConnectingWifiView() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String linking_token = server.arg("token");
  String audiocast_name = server.arg("name");

  if (ssid == "" || password == "" || linking_token == "" || audiocast_name == "") {
    Serial.println("Invalid inputs (missing required fields)");
    server.send(400, "text/plain", "Invalid input");
    return;
  }

  storeUserInfo(ssid, password, linking_token, audiocast_name);
  printPreferences();

  String spinnerPage = R"=====(<!DOCTYPE html><html><head>
    <title>Processing...</title>
    <style>
      
      :root {
        --bg-color: #121212;
        --card-color: #1e1e1e;
        --text-color: #e0e0e0;
        --primary-color: #bb86fc;
        --secondary-color: #03dac6;
      }
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background-color: var(--bg-color);
        color: var(--text-color);
        margin: 0;
        padding: 1.5rem;
        display: flex;
        flex-direction: column;
        min-height: 100vh;
        text-align: center;
      }

      .logo {
        width: 250px;
        height: 100px;
        margin: 0 auto 1rem;
        display: block;
        fill: var(--primary-color);
        color: var(--primary-color);
      }
      
      .content {
        flex: 1;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        padding: 0 10px;
      }
      
      .device-image {
        max-width: 450px;
        width: 100%;
        height: auto;
        margin: 1rem 0;
        border-radius: 8px;
        box-shadow: 0 4px 8px rgba(0,0,0,0.3);
      }
      .spinner {
        margin-top: 40px;
        width: 200px;
        height: 200px;
        border: 10px solid #444;
        border-top: 4px solid #bb86fc;
        border-radius: 50%;
        animation: spin 1s linear infinite;
      }
      @keyframes spin {
        to { transform: rotate(360deg); }
      }
      
      footer {
        padding: 1.5rem;
        font-size: 0.9rem;
        color: #777;
      }
      
      @media (max-width: 600px) {
        .logo {
          width: 220px;
        }
        
        .device-image {
          max-width: 350px;
        }
      }
    </style>
    <script>
      window.onload = function() {
        fetch("/do_connect")
          .then(res => res.json())
          .then(data => {
            if (data.success) {
              window.location.href = "/link";
            } else {
              window.location.href = "/wifi_fail";
            }
          })
          .catch(() => {
            window.location.href = "/wifi_fail";
          });
      };
    </script>
    </head>
    <body>


      <div class="content">
        <svg class="logo" xmlns="http://www.w3.org/2000/svg" width="150" height="50" viewBox="0 0 112.5 37.5" preserveAspectRatio="xMidYMid meet" version="1.0">
          <g fill="currentColor" fill-opacity="1">
            <g transform="translate(-0.882731, 35.46552)">
              <path d="M 8.984375 -20.296875 L 8.984375 -10.640625 C 8.984375 -8.640625 9.285156 -7.21875 9.890625 -6.375 C 10.492188 -5.53125 11.519531 -5.109375 12.96875 -5.109375 C 14.425781 -5.109375 15.457031 -5.53125 16.0625 -6.375 C 16.664062 -7.21875 16.96875 -8.640625 16.96875 -10.640625 L 16.96875 -20.296875 L 23.96875 -20.296875 L 23.96875 -8.9375 C 23.96875 -5.5625 23.09375 -3.128906 21.34375 -1.640625 C 19.59375 -0.148438 16.800781 0.59375 12.96875 0.59375 C 9.144531 0.59375 6.359375 -0.148438 4.609375 -1.640625 C 2.859375 -3.128906 1.984375 -5.5625 1.984375 -8.9375 L 1.984375 -20.296875 Z"/>
            </g>
            <g transform="translate(21.348404, 35.46552)">
              <path d="M 2.375 0 L 2.375 -34.03125 L 9.375 -34.03125 L 9.375 0 Z"/>
            </g>
            <g transform="translate(29.377815, 35.46552)">
              <path d="M 3.03125 -31.609375 C 3.851562 -32.429688 4.832031 -32.84375 5.96875 -32.84375 C 7.101562 -32.84375 8.078125 -32.429688 8.890625 -31.609375 C 9.710938 -30.796875 10.125 -29.820312 10.125 -28.6875 C 10.125 -27.550781 9.710938 -26.570312 8.890625 -25.75 C 8.078125 -24.9375 7.101562 -24.53125 5.96875 -24.53125 C 4.832031 -24.53125 3.851562 -24.9375 3.03125 -25.75 C 2.21875 -26.570312 1.8125 -27.550781 1.8125 -28.6875 C 1.8125 -29.820312 2.21875 -30.796875 3.03125 -31.609375 Z M 2.375 -20.296875 L 2.375 0 L 9.375 0 L 9.375 -20.296875 Z"/>
            </g>
            <g transform="translate(37.605028, 35.46552)">
              <path d="M 7.515625 -20.296875 L 12.46875 -9.25 L 17.40625 -20.296875 L 25.71875 -20.296875 L 14.828125 0 L 10.09375 0 L -0.796875 -20.296875 Z"/>
            </g>
            <g transform="translate(58.807621, 35.46552)">
              <path d="M 22.90625 -9.171875 L 8.1875 -9.171875 C 8.1875 -7.753906 8.644531 -6.703125 9.5625 -6.015625 C 10.488281 -5.328125 11.492188 -4.984375 12.578125 -4.984375 C 13.710938 -4.984375 14.609375 -5.132812 15.265625 -5.4375 C 15.929688 -5.738281 16.6875 -6.335938 17.53125 -7.234375 L 22.59375 -4.703125 C 20.476562 -1.171875 16.96875 0.59375 12.0625 0.59375 C 9.007812 0.59375 6.382812 -0.453125 4.1875 -2.546875 C 2 -4.648438 0.90625 -7.175781 0.90625 -10.125 C 0.90625 -13.082031 2 -15.613281 4.1875 -17.71875 C 6.382812 -19.832031 9.007812 -20.890625 12.0625 -20.890625 C 15.28125 -20.890625 17.898438 -19.957031 19.921875 -18.09375 C 21.941406 -16.238281 22.953125 -13.582031 22.953125 -10.125 C 22.953125 -9.65625 22.9375 -9.335938 22.90625 -9.171875 Z M 8.390625 -13.0625 L 16.109375 -13.0625 C 15.941406 -14.113281 15.519531 -14.921875 14.84375 -15.484375 C 14.175781 -16.054688 13.316406 -16.34375 12.265625 -16.34375 C 11.109375 -16.34375 10.1875 -16.035156 9.5 -15.421875 C 8.8125 -14.816406 8.441406 -14.03125 8.390625 -13.0625 Z"/>
            </g>
            <g transform="translate(78.942114, 35.46552)">
              <path d="M 22.90625 -9.171875 L 8.1875 -9.171875 C 8.1875 -7.753906 8.644531 -6.703125 9.5625 -6.015625 C 10.488281 -5.328125 11.492188 -4.984375 12.578125 -4.984375 C 13.710938 -4.984375 14.609375 -5.132812 15.265625 -5.4375 C 15.929688 -5.738281 16.6875 -6.335938 17.53125 -7.234375 L 22.59375 -4.703125 C 20.476562 -1.171875 16.96875 0.59375 12.0625 0.59375 C 9.007812 0.59375 6.382812 -0.453125 4.1875 -2.546875 C 2 -4.648438 0.90625 -7.175781 0.90625 -10.125 C 0.90625 -13.082031 2 -15.613281 4.1875 -17.71875 C 6.382812 -19.832031 9.007812 -20.890625 12.0625 -20.890625 C 15.28125 -20.890625 17.898438 -19.957031 19.921875 -18.09375 C 21.941406 -16.238281 22.953125 -13.582031 22.953125 -10.125 C 22.953125 -9.65625 22.9375 -9.335938 22.90625 -9.171875 Z M 8.390625 -13.0625 L 16.109375 -13.0625 C 15.941406 -14.113281 15.519531 -14.921875 14.84375 -15.484375 C 14.175781 -16.054688 13.316406 -16.34375 12.265625 -16.34375 C 11.109375 -16.34375 10.1875 -16.035156 9.5 -15.421875 C 8.8125 -14.816406 8.441406 -14.03125 8.390625 -13.0625 Z"/>
            </g>
            <g transform="translate(99.076617, 35.46552)">
              <path d="M 6.890625 -8.46875 C 8.148438 -8.46875 9.222656 -8.023438 10.109375 -7.140625 C 10.992188 -6.253906 11.4375 -5.179688 11.4375 -3.921875 C 11.4375 -2.679688 10.992188 -1.617188 10.109375 -0.734375 C 9.222656 0.148438 8.148438 0.59375 6.890625 0.59375 C 5.648438 0.59375 4.585938 0.148438 3.703125 -0.734375 C 2.816406 -1.617188 2.375 -2.679688 2.375 -3.921875 C 2.375 -5.179688 2.816406 -6.253906 3.703125 -7.140625 C 4.585938 -8.023438 5.648438 -8.46875 6.890625 -8.46875 Z"/>
            </g>
          </g>
        </svg>

      
      
        <h1>Welcome</h1>
        
        <img src="/image" class="device-image" alt="Audiocast Device">

        <h1>Connecting to WiFi...</h1>
        <div class="spinner"></div>
        
      </div>
    

      <footer>
        ULivee Records SLU &copy; 2025
      </footer>
    </body>
  </html>)=====";

  server.send(200, "text/html", spinnerPage);
}



void handleWifiConnectionLogic() {
  bool result = connectToWiFi();
  Serial.println(result ? "Wifi connected!" : "Failed wifi connection..");
  server.send(200, "application/json", result ? "{\"success\": true}" : "{\"success\": false}");
}


void handleLinkAccountView() {
  
  // Show the spinner page
  String spinnerPage = R"=====(<!DOCTYPE html><html><head>
    <title>Processing...</title>
    <style>
      body {
        background-color: #121212;
        color: #e0e0e0;
        font-family: 'Segoe UI', sans-serif;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 100vh;
        margin: 0;
      }
      .spinner {
        margin-top: 20px;
        width: 200px;
        height: 200px;
        border: 4px solid #444;
        border-top: 4px solid #bb86fc;
        border-radius: 50%;
        animation: spin 1s linear infinite;
      }
      @keyframes spin {
        to { transform: rotate(360deg); }
      }
    </style>
    <script>
      window.onload = function() {
        fetch("/do_link")
          .then(res => res.json())
          .then(data => {
            if (data.success) {
              window.location.href = "/success";
            } else {
              window.location.href = "/link_fail";
            }
          })
          .catch(() => {
            window.location.href = "/link_fail";
          });
      };
    </script>
    </head>
    <body>
      <h1>WiFi Connected</h1>
      <h1>Linking your devise...</h1>
      <div class="spinner"></div>
    </body>
  </html>)=====";

  server.send(200, "text/html", spinnerPage);
}



void handleLinkAccountLogic() {
  bool result = registerWithRailsAPI();
  Serial.println(result ? "API registration succeeded" : "API registration failed");
  server.send(200, "application/json", result ? "{\"success\": true}" : "{\"success\": false}");
}


void handleSuccess() {
    String page = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuration Saved</title>
      <style>
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background-color: #121212;
          color: #e0e0e0;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
          margin: 0;
        }
        .card {
          background-color: #1e1e1e;
          padding: 30px;
          border-radius: 8px;
          text-align: center;
          box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
          max-width: 500px;
          width: 90%;
        }
        h1 {
          color: #bb86fc;
          margin-bottom: 20px;
        }
        .success {
          color: #00c853;
          margin: 20px 0;
        }
        a {
          color: #03dac6;
          text-decoration: none;
          font-weight: 1000;
        }
        a:hover {
          text-decoration: underline;
        }
      </style>
    </head>
    <body>
      <div class="card">
        <h1>Success</h1>
        <div class="success">Your audiocast has benn linked!</div>
        <a>Return to ULivee.io and start playing music!</a>
      </div>
    </body>
    </html>
    )=====";
  server.send(200, "text/html", page);
  delay(3000);
  ESP.restart();
};


void handleLinkFail() {
  preferences.clear(); // Optional: clear only on actual fail
    String page = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuration Saved</title>
      <style>
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background-color: #121212;
          color: #e0e0e0;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
          margin: 0;
        }
        .card {
          background-color: #1e1e1e;
          padding: 30px;
          border-radius: 8px;
          text-align: center;
          box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
          max-width: 500px;
          width: 90%;
        }
        h1 {
          color: #bb86fc;
          margin-bottom: 20px;
        }
        .fail {
          color: #c83f00;
          margin: 20px 0;
        }
        a {
          color: #03dac6;
          text-decoration: none;
          font-weight: 500;
        }
        a:hover {
          text-decoration: underline;
        }
      </style>
    </head>
    <body>
      <div class="card">
        <h1>Failed</h1>
        <div class="fail">Unknown error...</div>
        <h1>Please try again</h1>
      </div>
    </body>
    </html>
    )=====";
  server.send(200, "text/html", page);
  delay(1000);
  ESP.restart();
};



void handleWifiFail() {
  preferences.clear(); // Optional: clear only on actual fail
    String page = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuration Saved</title>
      <style>
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background-color: #121212;
          color: #e0e0e0;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
          margin: 0;
        }
        .card {
          background-color: #1e1e1e;
          padding: 30px;
          border-radius: 8px;
          text-align: center;
          box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
          max-width: 500px;
          width: 90%;
        }
        h1 {
          color: #bb86fc;
          margin-bottom: 20px;
        }
        .fail {
          color: #c83f00;
          margin: 20px 0;
        }
        a {
          color: #03dac6;
          text-decoration: none;
          font-weight: 500;
        }
        a:hover {
          text-decoration: underline;
        }
      </style>
    </head>
    <body>
      <div class="card">
        <h1>Failed</h1>
        <div class="fail">Check your wifi SSID and password.</div>
        <h1>Please try again</h1>
      </div>
    </body>
    </html>
    )=====";
  server.send(200, "text/html", page);
  delay(1000);
  ESP.restart();
};


bool registerWithRailsAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // ⚠️ Reemplaza por client.setCACert() en producción

  HTTPClient http;
  // 1. Verify server reachability first
  if (!http.begin(apiUrl)) {
    Serial.println("Failed to configure HTTP client");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 second timeout

  String linking_token, audiocast_name, secret, ip, mac;
  loadUserInfo(linking_token, audiocast_name, secret);
  ip = WiFi.localIP().toString();
  mac = WiFi.macAddress();

  // Create JSON payload
  DynamicJsonDocument doc(512);
  doc["linking_token"] = linking_token;
  doc["mac"] = mac;
  doc["ip"] = ip;
  doc["audiocast_name"] = audiocast_name;
  doc["secret"] = secret;

  String requestBody;
  serializeJson(doc, requestBody);

  Serial.println("Sending to " + String(apiUrl));
  Serial.println("Payload: " + requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.print("Response: ");
    Serial.println(response);

    // Parse the response
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(responseDoc, response);

    if (httpResponseCode == 200 || httpResponseCode == 201) {
      // Successfully registered
      Serial.println("Registration successful!");
      http.end();
      return true;
    } else {
      // Handle errors
      Serial.print("Registration failed: ");
      if (responseDoc.containsKey("error")) {
        Serial.println(responseDoc["error"]["message"].as<String>());
      } else {
        Serial.println(response);
      }
    }
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  return false;
}


void handleImage() {
  File file = LittleFS.open("/device.png", "r");
  if (!file || file.isDirectory()) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
}



// ************************************************************************
// *                                  Wifi                                *
// ************************************************************************


bool connectToWiFi() {

  String wifi_ssid, wifi_password;
  loadWifiInfo(wifi_ssid, wifi_password);

  if (wifi_ssid == "") {
    Serial.println("No wifi configured...");
    Serial.println("Starting AP Mode...");
    startAPMode();
    return false;
  }

  Serial.println("Connecting to WiFi...");
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 5000; // 20 seconds timeout

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Configure NTP time synchronization
    // Try NTP sync with 2s max timeout
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Waiting for NTP time sync...");
    unsigned long ntpStart = millis();
    while (time(nullptr) < 24 * 3600 && millis() - ntpStart < 5000) {
      delay(100);
    } 

    handleStreamReconnect();

    return true;
  } else {
    Serial.println("\nFailed to connect to WiFi");
    return false;
  }
}


void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    if (connectToWiFi()) {
      Serial.print("Wifi connected!");
    } else {
      Serial.print("Connection failed.!");
    }
  }
}







// ************************************************************************
// *                                Routes                                *
// ************************************************************************


void setupRoutes () {
  // Register AP endpoints
  LittleFS.begin();

  server.on("/connect", handleConnectingWifiView); // Connecting to wifi view
  server.on("/do_connect", handleWifiConnectionLogic); // Connect to wifi logic
  server.on("/link", handleLinkAccountView); // Linking account view
  server.on("/do_link", handleLinkAccountLogic); // Link to account logic
  server.on("/success", handleSuccess); // Success
  server.on("/link_fail", handleLinkFail); // Link account fail
  server.on("/wifi_fail", handleWifiFail); // Wifi fails view
  server.on("/image", HTTP_GET, handleImage);


  // Register STA endpoints
  // POST routes
  server.on("/play", HTTP_POST, handlePlay);
  server.on("/pause", HTTP_POST, handlePause);
  server.on("/volume", HTTP_POST, handleVolume);
  server.on("/reset", HTTP_POST, handleResetPreferences);

  // GET route
  server.on("/status", HTTP_GET, handleStatus);

  // OPTIONS preflight handlers
  server.on("/play", HTTP_OPTIONS, handleCORSOptions);
  server.on("/pause", HTTP_OPTIONS, handleCORSOptions);
  server.on("/volume", HTTP_OPTIONS, handleCORSOptions);
  server.on("/reset", HTTP_OPTIONS, handleCORSOptions);

  // Catch-all for undefined routes (optional)
  server.onNotFound([]() {
    enableCORS();
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });








  server.begin();

}




// ************************************************************************
// *                             Setup and loop                           *
// ************************************************************************


void setup() {
  delay(1000);  // Wait 1 second after power-up (you can try 2000ms if needed)
  Serial.begin(115200);

  pinMode(wifiLedPin, OUTPUT);
  pinMode(streamLedPin, OUTPUT);

  preferences.begin("my-app", false); // Initializes preferences
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info); // Development -> Logs audio info
  printPreferences(); // Development -> Logs audio info
  
  checkWiFiConnection();
  
  setupRoutes();
  
}



void loop() {
  server.handleClient();

  // Check WiFi connection periodically
  if (millis() - lastWiFiCheckTime >= wifiCheckInterval) {
    checkWiFiConnection();
    lastWiFiCheckTime = millis();
  }

  // ---------------------------------------------------
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiBlink >= 1000) { // 500ms blink
      wifiLedState = !wifiLedState;
      digitalWrite(wifiLedPin, wifiLedState);
      lastWifiBlink = millis();
    }
  } else {
    digitalWrite(wifiLedPin, HIGH); // Solid ON
  }
  // ---------------------------------------------------

  if (millis() - lastStreamCheckTime >= streamCheckInterval) {
    isStreamFinished();
    lastStreamCheckTime = millis();
  }
  
  
  // Process audio if playing
  if (streamCopier) {
    streamCopier->copy();

    // ---------------------------------------------------
    if (millis() - lastStreamBlink >= 50) { // Fast blink
      streamLedState = !streamLedState;
      digitalWrite(streamLedPin, streamLedState);
      lastStreamBlink = millis();
    }
    // ---------------------------------------------------
  } else {
    // ---------------------------------------------------
    digitalWrite(streamLedPin, LOW); // Off when not streaming
    // ---------------------------------------------------
  }
}


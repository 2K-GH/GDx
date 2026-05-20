#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include "nvs_flash.h"
#include "nvs.h"

// =============================================================================
// MSx — Synchronized Channel Hop
// Mirrors Jx EEPROM index (0→Ch1, 1→Ch6, 2→Ch11) but stays one step ahead.
// AP_CHANNELS[N] = the clean channel while Jx targets focusedChannels[N].
//
// Jx index 0 → jams Ch 1  → ESP32-CAM AP on Ch 6   (AP_CHANNELS[0])
// Jx index 1 → jams Ch 6  → ESP32-CAM AP on Ch 11  (AP_CHANNELS[1])
// Jx index 2 → jams Ch 11 → ESP32-CAM AP on Ch 1   (AP_CHANNELS[2])
//
// Both devices share the same power source. Every plug-in increments
// both counters in lockstep. No wiring between devices required.
// =============================================================================

// --- AI-THINKER PINOUT (OV3660) ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_GPIO_NUM     4

// --- AP credentials ---
const char* ssid     = "SSID";
const char* password = "password";

// --- Hop table: index matches Jx's focusedChannels[] index ---
// Jx:  {Ch1,  Ch6,  Ch11}  (register values {12, 37, 62})
// MSx: {Ch6,  Ch11, Ch1 }  — always one step ahead
const int AP_CHANNELS[]    = {6, 11, 1};
const int NUM_HOP_CHANNELS = 3;

// --- NVS storage ---
#define NVS_NAMESPACE "msx_hop"
#define NVS_KEY       "idx"

// --- WebSocket / server ---
AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsControl("/Control");

bool flashState = false;

// =============================================================================
// NVS hop index — reads current index, writes next, returns current.
// Mirrors Jx EEPROM.update() pattern exactly.
// On first boot (key absent) defaults to 0 → AP on Ch 6 (Jx starts Ch 1).
// =============================================================================
int readAndIncrementHopIndex() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return 0;

  uint8_t currentIdx = 0;
  nvs_get_u8(handle, NVS_KEY, &currentIdx);          // silent fail = 0 on first boot

  if (currentIdx >= NUM_HOP_CHANNELS) currentIdx = 0; // same guard as Jx

  uint8_t nextIdx = (currentIdx + 1) % NUM_HOP_CHANNELS;
  nvs_set_u8(handle, NVS_KEY, nextIdx);
  nvs_commit(handle);
  nvs_close(handle);

  return (int)currentIdx;
}

// =============================================================================
// HTML page (unchanged from original)
// =============================================================================
const char* htmlHomePage PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<style>
  body { font-family: sans-serif; text-align: center; background: #000; color: #fff; margin: 0; padding: 5px; overflow: hidden; }
  #stream { width: 100%; max-width: 400px; height: auto; border: 1px solid #333; background: #111; }
  .grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; max-width: 250px; margin: 15px auto; }
  .btn { background: #222; color: #fff; padding: 25px 0; border: 1px solid #444; border-radius: 8px; font-size: 18px; font-weight: bold; touch-action: manipulation; }
  .btn:active { background: #4CAF50; }
  .flash-btn { background: #444; color: #ffeb3b; border-color: #ffeb3b; transition: 0.3s; }
  .flash-on { background: #4CAF50 !important; color: #fff !important; border-color: #fff !important; }
  .slider-box { width: 90%; margin: 15px auto; }
  input[type=range] { width: 100%; height: 20px; appearance: none; background: #333; outline: none; border-radius: 10px; }
  input[type=range]::-webkit-slider-thumb { appearance: none; width: 30px; height: 30px; background: #4CAF50; border-radius: 50%; }
  label { display: block; font-weight: bold; color: #888; margin-bottom: 5px; letter-spacing: 1px; }
</style></head><body>
  <img id="stream" src="">
  <div class="grid">
    <div></div><button class="btn" onmousedown="s('F')" onmouseup="s('S')" ontouchstart="s('F')" ontouchend="s('S')">F</button><div></div>
    <button class="btn" onmousedown="s('L')" onmouseup="s('S')" ontouchstart="s('L')" ontouchend="s('S')">L</button>
    <button id="fBtn" class="btn flash-btn" onclick="s('X')">FLASH</button>
    <button class="btn" onmousedown="s('R')" onmouseup="s('S')" ontouchstart="s('R')" ontouchend="s('S')">R</button>
    <div></div><button class="btn" onmousedown="s('B')" onmouseup="s('S')" ontouchstart="s('B')" ontouchend="s('S')">B</button><div></div>
  </div>
  <div class="slider-box">
    <label>PAN</label>
    <input type="range" min="35" max="115" value="75" oninput="t('P', this.value)">
  </div>
  <div class="slider-box">
    <label>TILT</label>
    <input type="range" min="90" max="180" value="135" oninput="t('T', this.value)">
  </div>
  <script>
    var ctrl = new WebSocket('ws://'+window.location.hostname+'/Control');
    var cam  = new WebSocket('ws://'+window.location.hostname+'/Camera');
    cam.binaryType = 'blob';
    cam.onmessage = function(e){
      var url = URL.createObjectURL(e.data);
      document.getElementById('stream').src = url;
      setTimeout(() => { URL.revokeObjectURL(url); }, 25);
    };
    var lastT = 0;
    function t(p, v) {
      var now = Date.now();
      if (now - lastT > 50) {
        let val = v;
        if(p === 'P') { val = (115 + 35) - v; }
        if(ctrl.readyState === WebSocket.OPEN) {
          ctrl.send(p + val);
          lastT = now;
        }
      }
    }
    function s(m){
      if(ctrl.readyState === WebSocket.OPEN) {
        ctrl.send(m);
        if(m === 'X') {
          document.getElementById('fBtn').classList.toggle('flash-on');
        }
      }
    }
    cam.onclose  = () => { setTimeout(() => { location.reload(); }, 1000); };
    ctrl.onclose = () => { setTimeout(() => { location.reload(); }, 1000); };
  </script>
</body></html>)HTML";

// =============================================================================
// WebSocket event handler (unchanged)
// =============================================================================
void onEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t,
             void *arg, uint8_t *data, size_t len) {
  if (t == WS_EVT_DATA) {
    char cmd = (char)data[0];
    if (cmd == 'X') {
      flashState = !flashState;
      digitalWrite(FLASH_GPIO_NUM, flashState ? HIGH : LOW);
    } else {
      for (size_t i = 0; i < len; i++) Serial.print((char)data[i]);
      Serial.println();
    }
  }
}

// =============================================================================
// setup()
// =============================================================================
void setup() {
  Serial.begin(9600);
  delay(2000);

  // Servo home position signals (unchanged)
  Serial.println("S");
  Serial.println("P75");
  Serial.println("T135");

  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  // --- NVS init ---
  nvs_flash_init();

  // --- Read hop index, determine AP channel ---
  int hopIdx    = readAndIncrementHopIndex();
  int apChannel = AP_CHANNELS[hopIdx];

  Serial.printf("[MSx] Hop index: %d | AP channel: %d\n", hopIdx, apChannel);
  Serial.printf("[MSx] Jx is targeting Wi-Fi Ch %s\n",
    hopIdx == 0 ? "1 (2412 MHz)" :
    hopIdx == 1 ? "6 (2437 MHz)" : "11 (2462 MHz)");

  // --- Flash LED identification (mirrors Jx LED pattern) ---
  // AP channel 6  → 2 blinks  (hopIdx 0)
  // AP channel 11 → 3 blinks  (hopIdx 1)
  // AP channel 1  → 1 blink   (hopIdx 2)
  // blink count matches AP channel number: 1=Ch1, 2=Ch6, 3=Ch11
  const int blinkCount[] = {2, 3, 1};
  int blinks = blinkCount[hopIdx];
  for (int i = 0; i < blinks; i++) {
    digitalWrite(FLASH_GPIO_NUM, HIGH); delay(400);
    digitalWrite(FLASH_GPIO_NUM, LOW);  delay(400);
  }

  // --- Wi-Fi AP ---
  WiFi.mode(WIFI_AP);
  delay(100);
  // softAP(ssid, password, channel, hidden, max_connection)
  WiFi.softAP(ssid, password, apChannel, 1, 1);
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);

  Serial.printf("[MSx] SoftAP started on channel %d\n", apChannel);

  // --- Camera config (unchanged) ---
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_QVGA;
  config.jpeg_quality  = 12;
  config.fb_count      = 2;

  esp_camera_init(&config);
  sensor_t *s_sensor = esp_camera_sensor_get();
  if (s_sensor && s_sensor->id.PID == OV3660_PID) s_sensor->set_vflip(s_sensor, 1);

  // --- Web server / WebSocket (unchanged) ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send_P(200, "text/html", htmlHomePage);
  });
  wsControl.onEvent(onEvent);
  server.addHandler(&wsControl);
  server.addHandler(&wsCamera);
  server.begin();
}

// =============================================================================
// loop() — unchanged
// =============================================================================
void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    if (wsCamera.count() > 0) {
      wsCamera.binaryAll(fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
  }
}

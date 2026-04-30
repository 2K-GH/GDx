#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

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

const char* ssid = "SSID";
const char* password = "password";

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsControl("/Control");

bool flashState = false;

const char* htmlHomePage PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<style>
  body { font-family: sans-serif; text-align: center; background: #000; color: #fff; margin: 0; padding: 5px; overflow: hidden; }
  #stream { width: 100%; max-width: 400px; height: auto; border: 1px solid #333; background: #111; }
  .grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; max-width: 250px; margin: 15px auto; }
  .btn { background: #222; color: #fff; padding: 25px 0; border: 1px solid #444; border-radius: 8px; font-size: 18px; font-weight: bold; touch-action: manipulation; }
  .btn:active { background: #4CAF50; }
  .flash-btn { background: #444; color: #ffeb3b; border-color: #ffeb3b; transition: 0.3s; }
  /* This class makes the button stay green when the flash is ON */
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
    var cam = new WebSocket('ws://'+window.location.hostname+'/Camera');
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
        // Toggles the green background on the button when 'X' is sent
        if(m === 'X') {
          document.getElementById('fBtn').classList.toggle('flash-on');
        }
      }
    }
    cam.onclose = () => { setTimeout(() => { location.reload(); }, 1000); };
    ctrl.onclose = () => { setTimeout(() => { location.reload(); }, 1000); };
  </script>
</body></html>)HTML";

void onEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *arg, uint8_t *data, size_t len) {
  if (t == WS_EVT_DATA) {
    char cmd = (char)data[0];
    if (cmd == 'X') {
      flashState = !flashState;
      digitalWrite(FLASH_GPIO_NUM, flashState ? HIGH : LOW);
    } else {
      for(size_t i=0; i < len; i++) {
        Serial.print((char)data[i]);
      }
      Serial.println();
    }
  }
}

void setup() {
  Serial.begin(9600);
  
  delay(2000); 

  Serial.println("S"); 
  Serial.println("P75");
  Serial.println("T135");

  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAP(ssid, password);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  esp_camera_init(&config);
  sensor_t * s_sensor = esp_camera_sensor_get();
  if (s_sensor && s_sensor->id.PID == OV3660_PID) s_sensor->set_vflip(s_sensor, 1);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", htmlHomePage); });
  wsControl.onEvent(onEvent);
  server.addHandler(&wsControl);
  server.addHandler(&wsCamera);
  server.begin();
}

void loop() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (fb) {
    if (wsCamera.count() > 0) {
      wsCamera.binaryAll(fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
  }
}

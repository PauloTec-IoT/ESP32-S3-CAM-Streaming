#include "esp_camera.h"
#include <WiFi.h>

// ===== CONFIGURAÇÃO DA REDE AP =====
const char* ssid = "ESP32_S3_CAM";
const char* password = "74526987";

// ===== PINAGEM ESP32-S3 (Padrão Freenove/Lilygo/AI-Thinker S3) =====
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5

#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

WiFiServer server(80);

// ===== HTML DA PÁGINA =====
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Stream</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { text-align: center; background: #111; color: #0f0; font-family: sans-serif; }
        img { width: 100%; max-width: 640px; height: auto; border: 2px solid #0f0; }
    </style>
</head>
<body>
    <h2>ESP32-S3 CAM - LIVE</h2>
    <img src="/stream">
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Configuração da Câmera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // Frequência do Clock (Reduzida para evitar FB-OVF)
  config.xclk_freq_hz = 10000000; 
  config.pixel_format = PIXFORMAT_JPEG;

  // Parâmetros de Memória e Qualidade
  config.frame_size = FRAMESIZE_VGA;     // Pode usar VGA no S3 tranquilamente
  config.jpeg_quality = 12;              // 0-63 (menor é melhor qualidade)
  config.fb_count = 2;                   
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST; // CORREÇÃO: Pega sempre o último frame

  // Inicialização
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro na inicialização da câmera: 0x%x", err);
    return;
  }

  // Configuração WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("\nAP Iniciado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    // Rota Principal
    if (request.indexOf("GET / ") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.print(index_html);
    } 
    // Rota de Stream
    else if (request.indexOf("GET /stream") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
      client.println();

      while (client.connected()) {
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
          Serial.println("Falha ao capturar frame");
          break;
        }

        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");

        esp_camera_fb_return(fb);
        
        // Pequena pausa para não sufocar a CPU e manter o WiFi estável
        delay(1); 
      }
    }
    client.stop();
  }
}

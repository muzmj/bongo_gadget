#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// 핀 정의
#define TFT_DC    0   // GPIO0
#define TFT_RST   2   // GPIO2
#define TFT_CS    15   // -1
#define TFT_BL    5   // GPIO5 - 백라이트 제어

// SPI 인스턴스는 기본 begin()만 사용 (ESP8266에서는 핀 지정 불필요)
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  // Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW); // 백라이트 ON (이 기기는 LOW에서 켜짐)

  SPI.begin(); // 기본 SPI 초기화

  tft.init(240, 240, SPI_MODE3);        // 240x240 해상도
  tft.setRotation(2);        // 0~3으로 방향 조정 (1 = 90도 회전)
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Hello Weather Clock!");
}

void loop() {
  // 화면에 간단한 시간 표시 등 가능
}

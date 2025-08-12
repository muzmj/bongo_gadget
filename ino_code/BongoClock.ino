#include <FS.h>        // SPIFFS
#include <TFT_eSPI.h>
#include <JPEGDecoder.h>
#include <ESP8266WiFi.h>
#include <time.h>

#define TFT_BL 5
TFT_eSPI tft = TFT_eSPI();

String ssid = "";
String password = "";
bool wifiConfigReceived = false;

unsigned long lastKeyTime = 0;
bool showLeft = true;

unsigned long lastClockUpdate = 0;
bool clockInitialized = false;

enum BongoState { IDLE, TYPING };
BongoState currentState = IDLE;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BongoCat ESP8266 시작 ===");
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW); // 백라이트 ON

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mount OK");
  }

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // 초기 화면 표시
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("BongoCat Ready", 120, 10, 2);
  tft.drawString("Waiting WiFi Config", 120, 30, 2);
  tft.drawString("Send from PC", 120, 50, 2);

  drawMiddle();
  currentState = IDLE;
  
  Serial.println("초기화 완료. Wi-Fi 설정 대기 중...");
}

void loop() {
  // 시리얼 데이터 처리
  handleSerialInput();
  
  // 2초간 입력 없으면 middle
  if (millis() - lastKeyTime > 2000 && currentState == TYPING) {
    drawMiddle();
    currentState = IDLE;
  }

  // 1초마다 시계 업데이트 (Wi-Fi 연결되고 시간이 동기화된 후에만)
  if (millis() - lastClockUpdate >= 1000) {
    lastClockUpdate = millis();
    updateDisplay();
  }
}

void handleSerialInput() {
  static String inputBuffer = "";
  
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processSerialCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
    
    // 버퍼 오버플로우 방지
    if (inputBuffer.length() > 200) {
      inputBuffer = "";
      Serial.println("입력 버퍼 초기화됨 (너무 긴 입력)");
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  Serial.println("수신된 명령: '" + command + "'");
  
  if (command.startsWith("WIFI:")) {
    handleWifiCommand(command);
  }
  else if (command == "KEY") {
    drawBongoFrame();
    currentState = TYPING;
  }
  else if (command == "TEST") {
    Serial.println("TEST OK - ESP8266 응답");
  }
  else {
    Serial.println("알 수 없는 명령: " + command);
  }
}

void handleWifiCommand(String wifiCmd) {
  Serial.println("Wi-Fi 설정 명령 처리 중...");
  
  // "WIFI:" 제거
  String params = wifiCmd.substring(5);
  
  int commaIndex = params.indexOf(',');
  if (commaIndex == -1) {
    Serial.println("Wi-Fi 명령 형식 오류 (쉼표 없음)");
    return;
  }
  
  String newSsid = params.substring(0, commaIndex);
  String newPassword = params.substring(commaIndex + 1);
  
  newSsid.trim();
  newPassword.trim();
  
  if (newSsid.length() == 0) {
    Serial.println("SSID가 비어있음");
    return;
  }
  
  Serial.println("새로운 Wi-Fi 설정:");
  Serial.println("SSID: '" + newSsid + "'");
  Serial.println("Password Length: " + String(newPassword.length()));
  
  // 설정 업데이트
  ssid = newSsid;
  password = newPassword;
  wifiConfigReceived = true;
  
  // 즉시 Wi-Fi 연결 시도
  connectWiFi();
}

void connectWiFi() {
  if (!wifiConfigReceived || ssid.length() == 0) {
    Serial.println("Wi-Fi 설정이 없어서 연결할 수 없음");
    return;
  }
  
  Serial.println("Wi-Fi 연결 시작...");
  Serial.println("SSID: " + ssid);
  
  // 기존 연결 정리
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  
  // 연결 상태 화면 표시
  tft.fillRect(0, 0, 240, 75, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Connecting WiFi", 120, 5, 2);
  tft.drawString(ssid, 120, 25, 2);
  
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startTime = millis();
  int dots = 0;
  
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
    
    // 연결 진행상황 화면 표시
    String connecting = "Connecting";
    for (int i = 0; i < (dots % 4); i++) {
      connecting += ".";
    }
    tft.fillRect(0, 45, 240, 20, TFT_BLACK);
    tft.drawString(connecting, 120, 50, 2);
    dots++;
    
    // 상태 변화 체크
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi 연결 성공!");
    Serial.println("IP 주소: " + WiFi.localIP().toString());
    Serial.println("신호 세기: " + String(WiFi.RSSI()) + " dBm");
    
    // 연결 성공 화면 표시
    tft.fillRect(0, 0, 240, 75, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("WiFi Connected!", 120, 5, 2);
    tft.drawString(WiFi.localIP().toString(), 120, 25, 2);
    tft.drawString("Signal: " + String(WiFi.RSSI()) + "dBm", 120, 45, 1);
    
    delay(3000); // 3초간 연결 정보 표시
    
    // NTP 서버 설정
    Serial.println("NTP 서버 설정 중...");
    configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov", "kr.pool.ntp.org");
    
    // NTP 동기화 대기
    Serial.println("시간 동기화 대기 중...");
    time_t now = time(nullptr);
    startTime = millis();
    
    while ((now < 1000000000) && millis() - startTime < 15000) {
      delay(500);
      now = time(nullptr);
      Serial.printf("NTP 동기화 중... 현재 timestamp: %ld\n", now);
      
      // 동기화 진행상황 표시
      tft.fillRect(0, 0, 240, 75, TFT_BLACK);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Time Sync...", 120, 10, 2);
      tft.drawString(String(now), 120, 30, 2);
    }
    
    if (now >= 1000000000) {
      Serial.println("NTP 동기화 완료!");
      struct tm* timeinfo = localtime(&now);
      Serial.printf("현재 시간: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                    timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      clockInitialized = true;
      
      // 시간 동기화 완료 표시
      tft.fillRect(0, 0, 240, 75, TFT_BLACK);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Time Sync OK!", 120, 20, 2);
      delay(2000);
      
    } else {
      Serial.println("NTP 동기화 실패");
      clockInitialized = false;
      
      tft.fillRect(0, 0, 240, 75, TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Time Sync Failed", 120, 20, 2);
      delay(2000);
    }
    
  } else {
    Serial.println("Wi-Fi 연결 실패!");
    Serial.println("WiFi Status: " + String(WiFi.status()));
    
    // 연결 실패 화면 표시
    tft.fillRect(0, 0, 240, 75, TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("WiFi Failed", 120, 10, 2);
    tft.drawString("Status: " + String(WiFi.status()), 120, 30, 2);
    tft.drawString("Check SSID/Password", 120, 50, 1);
    
    clockInitialized = false;
  }
}

void updateDisplay() {
  if (WiFi.status() == WL_CONNECTED) {
    if (clockInitialized) {
      time_t now = time(nullptr);
      if (now > 1000000000) {
        drawClock();
      } else {
        // 시간이 아직 동기화되지 않음
        tft.fillRect(0, 0, 240, 75, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Time Syncing...", 120, 10, 2);
        tft.drawString(String(now), 120, 30, 2);
      }
    } else {
      // 시계 미초기화 상태
      tft.fillRect(0, 0, 240, 75, TFT_BLACK);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Clock Not Ready", 120, 20, 2);
    }
  } else if (wifiConfigReceived) {
    // Wi-Fi 설정은 받았지만 연결되지 않음
    tft.fillRect(0, 0, 240, 75, TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("WiFi Disconnected", 120, 10, 2);
    tft.drawString("Status: " + String(WiFi.status()), 120, 30, 2);
    clockInitialized = false;
  } else {
    // Wi-Fi 설정 대기 중
    static int waitingDots = 0;
    String waiting = "Waiting Config";
    for (int i = 0; i < (waitingDots % 4); i++) {
      waiting += ".";
    }
    tft.fillRect(0, 0, 240, 75, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("BongoCat Ready", 120, 10, 2);
    tft.drawString(waiting, 120, 30, 2);
    tft.drawString("Send WiFi from PC", 120, 50, 1);
    waitingDots++;
  }
}

void drawClock() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  if (!timeinfo) return;

  char dateStr[30];
  char timeStr[15];
  const char* weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

  sprintf(dateStr, "%04d-%02d-%02d %s",
          timeinfo->tm_year + 1900,
          timeinfo->tm_mon + 1,
          timeinfo->tm_mday,
          weekdays[timeinfo->tm_wday]);

  sprintf(timeStr, "%02d:%02d:%02d",
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);

  // 시계 영역 지우기
  tft.fillRect(0, 0, 240, 75, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);

  // 날짜
  tft.drawString(dateStr, 120, 5, 4);
  // 시간
  tft.drawString(timeStr, 120, 35, 4);
  
  // Wi-Fi 신호 강도 표시
//  tft.setTextColor(TFT_GREEN, TFT_BLACK);
//  String wifiInfo = "WiFi: " + String(WiFi.RSSI()) + "dBm";
//  tft.drawString(wifiInfo, 120, 55, 1);

  // 디버깅용 출력 (초가 바뀔 때마다)
  static int lastSec = -1;
  if (timeinfo->tm_sec != lastSec) {
    lastSec = timeinfo->tm_sec;
    Serial.printf("[%02d:%02d:%02d] 시계 업데이트\n", 
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  }
}

void drawBongoFrame() {
  lastKeyTime = millis();
  if (showLeft) {
    drawJpeg("/bongo_left.jpg", 0, 88);
  } else {
    drawJpeg("/bongo_right.jpg", 0, 88);
  }
  showLeft = !showLeft;
}

void drawMiddle() {
  drawJpeg("/bongo_middle.jpg", 0, 88);
}

void drawJpeg(const char *filename, int xpos, int ypos) {
  if (JpegDec.decodeFile(filename) != 1) {
    Serial.print("JPEG 디코딩 실패: ");
    Serial.println(filename);
    return;
  }

  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  
  while (JpegDec.read()) {
    uint16_t *pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;
    tft.pushImage(mcu_x, mcu_y, mcu_w, mcu_h, pImg);
  }
}
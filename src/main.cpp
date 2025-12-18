#include <M5Unified.h>

// M5Stack Core2 + ExtPort の3ポート定義
struct PortConfig {
  String name;
  int pin0;
  int pin1;
  int ch0;
  int ch1;
};

PortConfig ports[] = {
  {"PORT A", 32, 33, 0, 1},  // Core2内蔵 Port A
  {"PORT C", 14, 13, 2, 3},  // ExtPort Port C
  {"PORT E", 25, 2, 4, 5}    // ExtPort Port E (E1選択: DIPスイッチで「2」をON)
};

const int PORT_COUNT = 3;
int currentPortIdx = 0;

const int PWM_FREQ = 1000;
const int PWM_RES = 8;

enum Mode { MODE_FORWARD, MODE_REVERSE, MODE_STOP };

Mode modes[3] = {MODE_STOP, MODE_STOP, MODE_STOP};
int forwardPower[3] = {0, 0, 0};
int reversePower[3] = {0, 0, 0};

void applyPWM(int portIdx) {
  PortConfig &p = ports[portIdx];
  
  switch(modes[portIdx]) {
    case MODE_FORWARD:
      ledcWrite(p.ch0, forwardPower[portIdx]);
      ledcWrite(p.ch1, 0);
      Serial.printf("%s Forward: PWM0=%d PWM1=0\n", p.name.c_str(), forwardPower[portIdx]);
      break;
    case MODE_REVERSE:
      ledcWrite(p.ch0, 0);
      ledcWrite(p.ch1, reversePower[portIdx]);
      Serial.printf("%s Reverse: PWM0=0 PWM1=%d\n", p.name.c_str(), reversePower[portIdx]);
      break;
    case MODE_STOP:
      ledcWrite(p.ch0, 0);
      ledcWrite(p.ch1, 0);
      Serial.printf("%s Stop: PWM0=0 PWM1=0\n", p.name.c_str());
      break;
  }
}

void updateDisplay() {
  PortConfig &p = ports[currentPortIdx];
  M5.Display.clear();
  
  // ヘッダー（ポートごとに色分け）
  uint32_t colors[] = {RED, BLUE, YELLOW};
  M5.Display.fillRect(0, 0, 320, 30, colors[currentPortIdx]);
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(5, 5);
  M5.Display.printf("%s (G%d,G%d)", p.name.c_str(), p.pin0, p.pin1);
  
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 50);
  
  // モード表示
  switch(modes[currentPortIdx]) {
    case MODE_FORWARD:
      M5.Display.println("Forward (Cool)");
      M5.Display.printf("\nPower: %d\n", forwardPower[currentPortIdx]);
      break;
    case MODE_REVERSE:
      M5.Display.println("Reverse (Heaaaat)");
      M5.Display.printf("\nPower: %d\n", reversePower[currentPortIdx]);
      break;
    case MODE_STOP:
      M5.Display.println("Stop");
      M5.Display.println("\nPower: 0");
      break;
  }
  
  M5.Display.println("\n----------------");
  M5.Display.setTextSize(1);
  M5.Display.printf("Fwd:%d Rev:%d\n", 
    forwardPower[currentPortIdx], 
    reversePower[currentPortIdx]);
  
  // タッチボタン表示
  M5.Display.fillRect(0, 200, 106, 40, DARKGREY);
  M5.Display.fillRect(107, 200, 106, 40, DARKGREY);
  M5.Display.fillRect(214, 200, 106, 40, DARKGREY);
  
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(30, 212);
  M5.Display.print("-10");
  M5.Display.setCursor(137, 212);
  M5.Display.print("+10");
  M5.Display.setCursor(232, 212);
  M5.Display.print("Mode");
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("M5Stack Core2 + ExtPort");
  Serial.println("Port A, C, E - 3-Port Peltier Control");
  Serial.println("========================================");
  
  // 全ポート初期化
  for(int i = 0; i < PORT_COUNT; i++) {
    ledcSetup(ports[i].ch0, PWM_FREQ, PWM_RES);
    ledcAttachPin(ports[i].pin0, ports[i].ch0);
    ledcWrite(ports[i].ch0, 0);
    
    ledcSetup(ports[i].ch1, PWM_FREQ, PWM_RES);
    ledcAttachPin(ports[i].pin1, ports[i].ch1);
    ledcWrite(ports[i].ch1, 0);
    
    Serial.printf("Init %s: G%d, G%d (CH%d, CH%d)\n", 
      ports[i].name.c_str(), ports[i].pin0, ports[i].pin1, ports[i].ch0, ports[i].ch1);
  }
  
  Serial.println("========================================\n");
  updateDisplay();
}

void loop() {
  M5.update();
  
  auto t = M5.Touch.getDetail();
  
  if (t.wasPressed()) {
    int x = t.x;
    int y = t.y;
    
    // 下部のボタンエリア
    if (y >= 200) {
      if (x < 106) {
        // -10ボタン
        if (modes[currentPortIdx] == MODE_FORWARD) {
          forwardPower[currentPortIdx] = max(0, forwardPower[currentPortIdx] - 10);
        } else if (modes[currentPortIdx] == MODE_REVERSE) {
          reversePower[currentPortIdx] = max(0, reversePower[currentPortIdx] - 10);
        }
        applyPWM(currentPortIdx);
        updateDisplay();
      }
      else if (x >= 107 && x < 213) {
        // +10ボタン
        if (modes[currentPortIdx] == MODE_FORWARD) {
          forwardPower[currentPortIdx] = min(255, forwardPower[currentPortIdx] + 10);
        } else if (modes[currentPortIdx] == MODE_REVERSE) {
          reversePower[currentPortIdx] = min(255, reversePower[currentPortIdx] + 10);
        }
        applyPWM(currentPortIdx);
        updateDisplay();
      }
      else {
        // Modeボタン
        switch(modes[currentPortIdx]) {
          case MODE_FORWARD: modes[currentPortIdx] = MODE_REVERSE; break;
          case MODE_REVERSE: modes[currentPortIdx] = MODE_STOP; break;
          case MODE_STOP:    modes[currentPortIdx] = MODE_FORWARD; break;
        }
        applyPWM(currentPortIdx);
        updateDisplay();
      }
    }
    // 上部タップでポート切り替え
    else if (y < 30) {
      currentPortIdx = (currentPortIdx + 1) % PORT_COUNT;
      Serial.printf("Switched to %s\n", ports[currentPortIdx].name.c_str());
      updateDisplay();
    }
  }
  
  delay(10);
}
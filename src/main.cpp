#include <M5Unified.h>

// M5Stack CoreのPortA ピン定義
const int PWM0_PIN = 21;  // G21
const int PWM1_PIN = 22;  // G22

// PWMチャンネル設定
const int PWM0_CH = 0;
const int PWM1_CH = 1;
const int PWM_FREQ = 1000;  // 1000Hz
const int PWM_RES = 8;      // 8bit (0-255)

// モード定義
enum Mode {
  MODE_FORWARD,
  MODE_REVERSE,
  MODE_STOP
};

Mode currentMode = MODE_STOP;

// 各モードごとのパワー値を保持
int forwardPower = 0;   // Forward時のパワー
int reversePower = 0;   // Reverse時のパワー

void updateDisplay() {
  M5.Display.clear();
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);
  
  // モード表示
  switch(currentMode) {
    case MODE_FORWARD:
      M5.Display.println("Forward");
      M5.Display.println("");
      M5.Display.printf("Power: %d\n", forwardPower);
      M5.Display.println("");
      M5.Display.printf("PWM0: %d\n", forwardPower);
      M5.Display.printf("PWM1: 0\n");
      break;
    case MODE_REVERSE:
      M5.Display.println("Reverse");
      M5.Display.println("");
      M5.Display.printf("Power: %d\n", reversePower);
      M5.Display.println("");
      M5.Display.printf("PWM0: 0\n");
      M5.Display.printf("PWM1: %d\n", reversePower);
      break;
    case MODE_STOP:
      M5.Display.println("Stop");
      M5.Display.println("");
      M5.Display.println("Power: 0");
      M5.Display.println("");
      M5.Display.println("PWM0: 0");
      M5.Display.println("PWM1: 0");
      break;
  }
  
  M5.Display.println("");
  M5.Display.setTextSize(1);
  M5.Display.println("A: -10  B: +10  C: Mode");
  M5.Display.println("");
  M5.Display.printf("Fwd:%d Rev:%d\n", forwardPower, reversePower);
}

void applyPWM() {
  switch(currentMode) {
    case MODE_FORWARD:
      ledcWrite(PWM0_CH, forwardPower);
      ledcWrite(PWM1_CH, 0);
      Serial.printf("Forward: PWM0=%d\n", forwardPower);
      break;
    case MODE_REVERSE:
      ledcWrite(PWM0_CH, 0);
      ledcWrite(PWM1_CH, reversePower);
      Serial.printf("Reverse: PWM1=%d\n", reversePower);
      break;
    case MODE_STOP:
      ledcWrite(PWM0_CH, 0);
      ledcWrite(PWM1_CH, 0);
      Serial.println("Stop: All OFF");
      break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  Serial.begin(115200);
  Serial.println("Hbridge Control with Mode Memory");
  
  // PWM初期化
  ledcSetup(PWM0_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWM0_PIN, PWM0_CH);
  ledcWrite(PWM0_CH, 0);
  
  ledcSetup(PWM1_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWM1_PIN, PWM1_CH);
  ledcWrite(PWM1_CH, 0);
  
  updateDisplay();
}

void loop() {
  M5.update();
  
  // Button A: パワー -10
  if (M5.BtnA.wasPressed()) {
    if (currentMode == MODE_FORWARD) {
      forwardPower = max(0, forwardPower - 10);
    } else if (currentMode == MODE_REVERSE) {
      reversePower = max(0, reversePower - 10);
    }
    applyPWM();
    updateDisplay();
  }
  
  // Button B: パワー +10
  if (M5.BtnB.wasPressed()) {
    if (currentMode == MODE_FORWARD) {
      forwardPower = min(255, forwardPower + 10);
    } else if (currentMode == MODE_REVERSE) {
      reversePower = min(255, reversePower + 10);
    }
    applyPWM();
    updateDisplay();
  }
  
  // Button C: モード切り替え (Forward → Reverse → Stop → Forward...)
  if (M5.BtnC.wasPressed()) {
    switch(currentMode) {
      case MODE_FORWARD:
        currentMode = MODE_REVERSE;
        Serial.println("Mode: Reverse");
        break;
      case MODE_REVERSE:
        currentMode = MODE_STOP;
        Serial.println("Mode: Stop");
        break;
      case MODE_STOP:
        currentMode = MODE_FORWARD;
        Serial.println("Mode: Forward");
        break;
    }
    applyPWM();
    updateDisplay();
  }
  
  delay(10);
}
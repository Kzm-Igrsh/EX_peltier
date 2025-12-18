#include <M5Unified.h>

// M5Stack Core2 + ExtPort の3ポート定義
struct PortConfig {
  String name;
  int pin0;      // Cool用ピン
  int pin1;      // Heat用ピン
  int ch0;       // Cool用チャンネル
  int ch1;       // Heat用チャンネル
};

PortConfig ports[] = {
  {"PORT A", 32, 33, 0, 1},  // Core2内蔵 Port A
  {"PORT C", 14, 13, 2, 3},  // ExtPort Port C
  {"PORT E", 19, 27, 4, 5}   // ExtPort Port E
};

const int PORT_COUNT = 3;

// 刺激パラメータ（全ポート共通）
const int HEAT_PWM = 40;
const unsigned long HEAT_TIME = 3000;  // 3秒

const int COOL_PWM = 240;
const unsigned long COOL_TIME = 3000;  // 3秒

const int HEAT_START_PWM = 240;
const unsigned long HEAT_START_TIME = 1000;  // 1秒

const int HEAT_END_PWM = 240;  // 冷却用PWM
const unsigned long HEAT_END_TIME = 1000;  // 1秒

const int COOL_START_PWM = 240;
const unsigned long COOL_START_TIME = 1000;  // 1秒

const int COOL_END_PWM = 240;  // 加熱用PWM
const unsigned long COOL_END_TIME = 1000;  // 1秒

// PWM設定
const int PWM_FREQ = 1000;
const int PWM_RES = 8;

// 状態定義
enum State {
  STATE_IDLE,
  STATE_HEAT_START,
  STATE_HEAT,
  STATE_HEAT_END,
  STATE_COOL_START,
  STATE_COOL,
  STATE_COOL_END
};

// 各ポートの状態
State portStates[3] = {STATE_IDLE, STATE_IDLE, STATE_IDLE};
unsigned long stateStartTimes[3] = {0, 0, 0};

// ★ 自動テスト用変数
bool autoTestRunning = false;
int autoTestPort = 0;  // 現在のテストポート
enum AutoTestPhase {
  AUTO_IDLE,
  AUTO_FIRST_SEQ,     // 1つ目の刺激シーケンス
  AUTO_TRANSITION,    // 同一ポート内の遷移（_endから_startへ）
  AUTO_SECOND_SEQ,    // 2つ目の刺激シーケンス
  AUTO_PORT_END       // ポート終了（ニュートラルに戻す）
};
AutoTestPhase autoTestPhase = AUTO_IDLE;

// テストパターン定義
// Port A: 冷→温, Port C: 温→冷, Port E: 冷→温
bool testStartsWithCool[3] = {true, false, true};

// 関数プロトタイプ
void setPeltier(int portIdx, int powerCool, int powerHeat);
void startHeatStimulus(int portIdx);
void startCoolStimulus(int portIdx);
void updateStateMachine(int portIdx);
void startAutoTest();
void updateAutoTest();
void drawUI();
void handleTouch();

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  Serial.begin(115200);
  M5.Display.setBrightness(255);
  
  Serial.println("\n========================================");
  Serial.println("Peltier 3-Port Thermal Stimulus Test");
  Serial.println("Port A, C, E");
  Serial.println("========================================");
  
  // 全ポート初期化
  for(int i = 0; i < PORT_COUNT; i++) {
    ledcSetup(ports[i].ch0, PWM_FREQ, PWM_RES);
    ledcAttachPin(ports[i].pin0, ports[i].ch0);
    ledcWrite(ports[i].ch0, 0);
    
    ledcSetup(ports[i].ch1, PWM_FREQ, PWM_RES);
    ledcAttachPin(ports[i].pin1, ports[i].ch1);
    ledcWrite(ports[i].ch1, 0);
    
    Serial.printf("Init %s: G%d(cool), G%d(heat)\n", 
      ports[i].name.c_str(), ports[i].pin0, ports[i].pin1);
  }
  
  Serial.println("========================================\n");
  
  drawUI();
}

void loop() {
  M5.update();
  
  // 自動テスト実行中
  if (autoTestRunning) {
    updateAutoTest();
  }
  
  // 全ポートの状態マシンを更新
  for(int i = 0; i < PORT_COUNT; i++) {
    if (portStates[i] != STATE_IDLE) {
      updateStateMachine(i);
    }
  }
  
  handleTouch();
  delay(10);
}

// ★ 自動テスト開始
void startAutoTest() {
  autoTestRunning = true;
  autoTestPort = 0;
  autoTestPhase = AUTO_FIRST_SEQ;
  
  Serial.println("\n========================================");
  Serial.println("AUTO TEST START");
  Serial.println("Port A: Cool>Heat, Port C: Heat>Cool, Port E: Cool>Heat");
  Serial.println("========================================\n");
  
  // Port Aから開始（冷刺激）
  startCoolStimulus(0);
  drawUI();
}

// ★ 自動テスト更新
void updateAutoTest() {
  // 現在のポートがIDLEになったら次に進む
  if (portStates[autoTestPort] == STATE_IDLE) {
    
    if (autoTestPhase == AUTO_FIRST_SEQ) {
      // 1つ目の刺激完了 → 同一ポート内で_endから_startへ遷移
      autoTestPhase = AUTO_TRANSITION;
      
      if (testStartsWithCool[autoTestPort]) {
        // 冷→温: COOL_ENDを経由
        Serial.printf("[AUTO] %s: Cool done, starting COOL_END (same port)\n", ports[autoTestPort].name.c_str());
        setPeltier(autoTestPort, 0, COOL_END_PWM);  // 加熱
        portStates[autoTestPort] = STATE_COOL_END;
        stateStartTimes[autoTestPort] = millis();
      } else {
        // 温→冷: HEAT_ENDを経由
        Serial.printf("[AUTO] %s: Heat done, starting HEAT_END (same port)\n", ports[autoTestPort].name.c_str());
        setPeltier(autoTestPort, HEAT_END_PWM, 0);  // 冷却
        portStates[autoTestPort] = STATE_HEAT_END;
        stateStartTimes[autoTestPort] = millis();
      }
      drawUI();
      
    } else if (autoTestPhase == AUTO_TRANSITION) {
      // _ENDが完了、次の刺激を開始
      autoTestPhase = AUTO_SECOND_SEQ;
      
      if (testStartsWithCool[autoTestPort]) {
        // 冷→温なので次は温刺激
        startHeatStimulus(autoTestPort);
        Serial.printf("[AUTO] %s: Starting Heat (same port)\n", ports[autoTestPort].name.c_str());
      } else {
        // 温→冷なので次は冷刺激
        startCoolStimulus(autoTestPort);
        Serial.printf("[AUTO] %s: Starting Cool (same port)\n", ports[autoTestPort].name.c_str());
      }
      
    } else if (autoTestPhase == AUTO_SECOND_SEQ) {
      // 2つ目の刺激完了 → ENDでニュートラルに戻してからポート切替
      autoTestPhase = AUTO_PORT_END;
      
      if (testStartsWithCool[autoTestPort]) {
        // 冷→温の場合、最後は温刺激なのでHEAT_ENDでニュートラルに
        Serial.printf("[AUTO] %s: Heat seq done, starting HEAT_END (neutral)\n", ports[autoTestPort].name.c_str());
        setPeltier(autoTestPort, HEAT_END_PWM, 0);  // 冷却でニュートラルへ
        portStates[autoTestPort] = STATE_HEAT_END;
        stateStartTimes[autoTestPort] = millis();
      } else {
        // 温→冷の場合、最後は冷刺激なのでCOOL_ENDでニュートラルに
        Serial.printf("[AUTO] %s: Cool seq done, starting COOL_END (neutral)\n", ports[autoTestPort].name.c_str());
        setPeltier(autoTestPort, 0, COOL_END_PWM);  // 加熱でニュートラルへ
        portStates[autoTestPort] = STATE_COOL_END;
        stateStartTimes[autoTestPort] = millis();
      }
      drawUI();
      
    } else if (autoTestPhase == AUTO_PORT_END) {
      // ENDが完了、次のポートへ
      autoTestPort++;
      
      if (autoTestPort >= PORT_COUNT) {
        // 全ポート完了
        autoTestRunning = false;
        Serial.println("\n========================================");
        Serial.println("AUTO TEST COMPLETE");
        Serial.println("========================================\n");
        drawUI();
      } else {
        // 次のポートの刺激開始
        autoTestPhase = AUTO_FIRST_SEQ;
        delay(1000);  // ポート間の待機時間
        
        if (testStartsWithCool[autoTestPort]) {
          startCoolStimulus(autoTestPort);
          Serial.printf("[AUTO] Starting %s (Cool>Heat)\n", ports[autoTestPort].name.c_str());
        } else {
          startHeatStimulus(autoTestPort);
          Serial.printf("[AUTO] Starting %s (Heat>Cool)\n", ports[autoTestPort].name.c_str());
        }
      }
    }
  }
}

// ペルチェ制御
void setPeltier(int portIdx, int powerCool, int powerHeat) {
  ledcWrite(ports[portIdx].ch0, powerCool);
  ledcWrite(ports[portIdx].ch1, powerHeat);
}

// 温刺激開始
void startHeatStimulus(int portIdx) {
  Serial.printf("\n[%s] === HEAT STIMULUS START ===\n", ports[portIdx].name.c_str());
  Serial.printf("[%s] Phase: HEAT_START\n", ports[portIdx].name.c_str());
  setPeltier(portIdx, 0, HEAT_START_PWM);
  portStates[portIdx] = STATE_HEAT_START;
  stateStartTimes[portIdx] = millis();
  drawUI();
}

// 冷刺激開始
void startCoolStimulus(int portIdx) {
  Serial.printf("\n[%s] === COOL STIMULUS START ===\n", ports[portIdx].name.c_str());
  Serial.printf("[%s] Phase: COOL_START\n", ports[portIdx].name.c_str());
  setPeltier(portIdx, COOL_START_PWM, 0);
  portStates[portIdx] = STATE_COOL_START;
  stateStartTimes[portIdx] = millis();
  drawUI();
}

// 状態マシン更新
void updateStateMachine(int portIdx) {
  unsigned long elapsed = millis() - stateStartTimes[portIdx];
  
  switch(portStates[portIdx]) {
    case STATE_HEAT_START:
      if (elapsed >= HEAT_START_TIME) {
        Serial.printf("[%s] Phase: HEAT\n", ports[portIdx].name.c_str());
        setPeltier(portIdx, 0, HEAT_PWM);
        portStates[portIdx] = STATE_HEAT;
        stateStartTimes[portIdx] = millis();
        drawUI();
      }
      break;
      
    case STATE_HEAT:
      if (elapsed >= HEAT_TIME) {
        Serial.printf("[%s] Phase: HEAT complete\n", ports[portIdx].name.c_str());
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        drawUI();
      }
      break;
      
    case STATE_HEAT_END:
      if (elapsed >= HEAT_END_TIME) {
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        Serial.printf("[%s] Heat End Complete.\n\n", ports[portIdx].name.c_str());
        drawUI();
      }
      break;
      
    case STATE_COOL_START:
      if (elapsed >= COOL_START_TIME) {
        Serial.printf("[%s] Phase: COOL\n", ports[portIdx].name.c_str());
        setPeltier(portIdx, COOL_PWM, 0);
        portStates[portIdx] = STATE_COOL;
        stateStartTimes[portIdx] = millis();
        drawUI();
      }
      break;
      
    case STATE_COOL:
      if (elapsed >= COOL_TIME) {
        Serial.printf("[%s] Phase: COOL complete\n", ports[portIdx].name.c_str());
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        drawUI();
      }
      break;
      
    case STATE_COOL_END:
      if (elapsed >= COOL_END_TIME) {
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        Serial.printf("[%s] Cool End Complete.\n\n", ports[portIdx].name.c_str());
        drawUI();
      }
      break;
      
    default:
      break;
  }
}

// UI描画
void drawUI() {
  M5.Display.clear(BLACK);
  
  // タイトル
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(20, 20);
  M5.Display.println("Peltier Test");
  
  // 各ポートの状態表示
  M5.Display.setTextSize(2);
  
  const char* stateNames[] = {
    "IDLE", "H_START", "HEAT", "H_END",
    "C_START", "COOL", "C_END"
  };
  
  uint32_t colors[] = {RED, BLUE, YELLOW};
  
  for(int i = 0; i < PORT_COUNT; i++) {
    M5.Display.setCursor(20, 70 + i * 30);
    uint16_t color = (portStates[i] == STATE_IDLE) ? DARKGREY : colors[i];
    M5.Display.setTextColor(color);
    M5.Display.printf("%s: %s", ports[i].name.c_str(), stateNames[portStates[i]]);
  }
  
  // TESTボタン（全ポートIDLE時のみ表示）
  bool allIdle = true;
  for(int i = 0; i < PORT_COUNT; i++) {
    if (portStates[i] != STATE_IDLE) {
      allIdle = false;
      break;
    }
  }
  
  if (allIdle && !autoTestRunning) {
    M5.Display.fillRect(60, 180, 200, 50, GREEN);
    M5.Display.setTextColor(BLACK);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(105, 195);
    M5.Display.println("TEST");
  } else if (autoTestRunning) {
    M5.Display.fillRect(60, 180, 200, 50, RED);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(95, 195);
    M5.Display.println("STOP");
  }
}

// タッチ処理
void handleTouch() {
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return;
  
  int x = t.x;
  int y = t.y;
  
  // TESTボタン領域
  if (y >= 180 && y <= 230 && x >= 60 && x <= 260) {
    if (autoTestRunning) {
      // テスト停止
      autoTestRunning = false;
      Serial.println("\n[AUTO TEST STOPPED]\n");
      
      // 全ポート停止
      for(int i = 0; i < PORT_COUNT; i++) {
        setPeltier(i, 0, 0);
        portStates[i] = STATE_IDLE;
      }
      drawUI();
    } else {
      // 全ポートIDLEなら開始
      bool allIdle = true;
      for(int i = 0; i < PORT_COUNT; i++) {
        if (portStates[i] != STATE_IDLE) {
          allIdle = false;
          break;
        }
      }
      if (allIdle) {
        startAutoTest();
      }
    }
  }
}
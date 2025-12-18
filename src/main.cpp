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

// ★ 実験用変数
bool experimentRunning = false;
const int EXPERIMENT_TRIALS = 20;
int experimentCurrentTrial = 0;
int experimentSequence[EXPERIMENT_TRIALS];  // 各試行の刺激タイプ (0=冷, 1=温)
int experimentPorts[EXPERIMENT_TRIALS];      // 各試行のポート
unsigned long experimentIntervals[EXPERIMENT_TRIALS];  // 各試行後のインターバル時間（ミリ秒）

enum ExperimentPhase {
  EXP_IDLE,
  EXP_STIMULUS,       // 刺激中
  EXP_END_PHASE,      // END phase
  EXP_INTERVAL        // 試行間インターバル
};
ExperimentPhase expPhase = EXP_IDLE;
unsigned long expPhaseStartTime = 0;

// ★ 予め決められたシーケンス定義
const int PREDEFINED_SEQUENCE[EXPERIMENT_TRIALS][2] = {
  // {ポート番号(0=A,1=C,2=E), 刺激タイプ(0=冷,1=温)}
  {1, 0}, {2, 1}, {0, 0}, {0, 0}, {1, 1},  // Trial 1-5 (3-4: 同じ場所・同じ刺激)
  {2, 0}, {2, 1}, {0, 1}, {1, 0}, {1, 1},  // Trial 6-10 (6-7: 同じ場所・違う刺激)
  {0, 0}, {2, 1}, {1, 0}, {0, 1}, {0, 0},  // Trial 11-15 (14-15: 同じ場所・違う刺激)
  {2, 0}, {1, 1}, {1, 0}, {2, 1}, {2, 1}   // Trial 16-20 (18-19, 19-20: 同じ場所)
};

// ★ 予め決められたインターバル時間（ミリ秒、1000~2000の範囲）
const unsigned long PREDEFINED_INTERVALS[EXPERIMENT_TRIALS] = {
  1200, 1800, 1500, 1100, 1900,  // Trial 1-5後
  1400, 1700, 1300, 2000, 1600,  // Trial 6-10後
  1200, 1500, 1800, 1100, 1400,  // Trial 11-15後
  1900, 1300, 1700, 1600, 1000   // Trial 16-20後（最後は使われない）
};

// 関数プロトタイプ
void setPeltier(int portIdx, int powerCool, int powerHeat);
void startHeatStimulus(int portIdx);
void startCoolStimulus(int portIdx);
void updateStateMachine(int portIdx);
void startAutoTest();
void updateAutoTest();
void startExperiment();
void updateExperiment();
void generateExperimentSequence();
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
  
  // 実験実行中
  if (experimentRunning) {
    updateExperiment();
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

// ★ 実験シーケンス生成（予め決められた順番を使用）
void generateExperimentSequence() {
  Serial.println("\n=== Experiment Sequence (Predefined) ===");
  for(int i = 0; i < EXPERIMENT_TRIALS; i++) {
    experimentPorts[i] = PREDEFINED_SEQUENCE[i][0];
    experimentSequence[i] = PREDEFINED_SEQUENCE[i][1];
    experimentIntervals[i] = PREDEFINED_INTERVALS[i];
    Serial.printf("Trial %d: %s, %s, Interval: %.1fs\n", i+1, 
      ports[experimentPorts[i]].name.c_str(),
      experimentSequence[i] == 0 ? "COOL" : "HEAT",
      experimentIntervals[i] / 1000.0);
  }
  Serial.println("======================================\n");
}

// ★ 実験開始
void startExperiment() {
  experimentRunning = true;
  experimentCurrentTrial = 0;
  expPhase = EXP_STIMULUS;
  
  generateExperimentSequence();
  
  Serial.println("\n========================================");
  Serial.println("EXPERIMENT START - 20 Trials");
  Serial.println("========================================\n");
  
  // 最初の試行開始
  int port = experimentPorts[0];
  if (experimentSequence[0] == 0) {
    startCoolStimulus(port);
    Serial.printf("[EXP] Trial 1/20: %s COOL\n", ports[port].name.c_str());
  } else {
    startHeatStimulus(port);
    Serial.printf("[EXP] Trial 1/20: %s HEAT\n", ports[port].name.c_str());
  }
  
  drawUI();
}

// ★ 実験更新
void updateExperiment() {
  int currentPort = experimentPorts[experimentCurrentTrial];
  
  if (expPhase == EXP_STIMULUS) {
    // 刺激が完了したらEND phaseへ
    if (portStates[currentPort] == STATE_IDLE) {
      expPhase = EXP_END_PHASE;
      
      // ENDフェーズ開始
      if (experimentSequence[experimentCurrentTrial] == 0) {
        // 冷刺激の後はCOOL_END
        Serial.printf("[EXP] Trial %d: COOL_END\n", experimentCurrentTrial + 1);
        setPeltier(currentPort, 0, COOL_END_PWM);
        portStates[currentPort] = STATE_COOL_END;
        stateStartTimes[currentPort] = millis();
      } else {
        // 温刺激の後はHEAT_END
        Serial.printf("[EXP] Trial %d: HEAT_END\n", experimentCurrentTrial + 1);
        setPeltier(currentPort, HEAT_END_PWM, 0);
        portStates[currentPort] = STATE_HEAT_END;
        stateStartTimes[currentPort] = millis();
      }
      drawUI();
    }
  } else if (expPhase == EXP_END_PHASE) {
    // END phaseが完了したらインターバルへ
    if (portStates[currentPort] == STATE_IDLE) {
      experimentCurrentTrial++;
      
      if (experimentCurrentTrial >= EXPERIMENT_TRIALS) {
        // 全試行完了
        experimentRunning = false;
        Serial.println("\n========================================");
        Serial.println("EXPERIMENT COMPLETE");
        Serial.println("========================================\n");
        drawUI();
      } else {
        // 次の試行へ（インターバル）
        expPhase = EXP_INTERVAL;
        expPhaseStartTime = millis();
        Serial.printf("[EXP] Interval before trial %d (%.1fs)...\n", 
          experimentCurrentTrial + 1,
          experimentIntervals[experimentCurrentTrial - 1] / 1000.0);
      }
    }
  } else if (expPhase == EXP_INTERVAL) {
    // インターバル完了後、次の刺激開始
    unsigned long currentInterval = experimentIntervals[experimentCurrentTrial - 1];
    if (millis() - expPhaseStartTime >= currentInterval) {
      expPhase = EXP_STIMULUS;
      
      int port = experimentPorts[experimentCurrentTrial];
      if (experimentSequence[experimentCurrentTrial] == 0) {
        startCoolStimulus(port);
        Serial.printf("[EXP] Trial %d/20: %s COOL\n", 
          experimentCurrentTrial + 1, ports[port].name.c_str());
      } else {
        startHeatStimulus(port);
        Serial.printf("[EXP] Trial %d/20: %s HEAT\n", 
          experimentCurrentTrial + 1, ports[port].name.c_str());
      }
      drawUI();
    }
  }
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
  M5.Display.setTextSize(2);
  M5.Display.setCursor(20, 10);
  M5.Display.println("Peltier Test");
  
  // 各ポートの状態表示
  M5.Display.setTextSize(1);
  
  const char* stateNames[] = {
    "IDLE", "H_START", "HEAT", "H_END",
    "C_START", "COOL", "C_END"
  };
  
  uint32_t colors[] = {RED, BLUE, YELLOW};
  
  for(int i = 0; i < PORT_COUNT; i++) {
    M5.Display.setCursor(20, 40 + i * 15);
    uint16_t color = (portStates[i] == STATE_IDLE) ? DARKGREY : colors[i];
    M5.Display.setTextColor(color);
    M5.Display.printf("%s: %s", ports[i].name.c_str(), stateNames[portStates[i]]);
  }
  
  // 実験進捗表示
  if (experimentRunning) {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 95);
    M5.Display.printf("EXP: %d/20", experimentCurrentTrial + 1);
  }
  
  // ボタン表示
  bool allIdle = true;
  for(int i = 0; i < PORT_COUNT; i++) {
    if (portStates[i] != STATE_IDLE) {
      allIdle = false;
      break;
    }
  }
  
  M5.Display.setTextSize(2);
  
  if (allIdle && !autoTestRunning && !experimentRunning) {
    // TESTボタン
    M5.Display.fillRect(20, 130, 130, 40, GREEN);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(40, 143);
    M5.Display.println("TEST");
    
    // EXPERIMENTボタン
    M5.Display.fillRect(170, 130, 130, 40, CYAN);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(190, 143);
    M5.Display.println("EXP");
  } else if (autoTestRunning || experimentRunning) {
    // STOPボタン
    M5.Display.fillRect(85, 130, 150, 40, RED);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(115, 143);
    M5.Display.println("STOP");
  }
}

// タッチ処理
void handleTouch() {
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return;
  
  int x = t.x;
  int y = t.y;
  
  // ボタン領域
  if (y >= 130 && y <= 170) {
    bool allIdle = true;
    for(int i = 0; i < PORT_COUNT; i++) {
      if (portStates[i] != STATE_IDLE) {
        allIdle = false;
        break;
      }
    }
    
    if (autoTestRunning || experimentRunning) {
      // STOPボタン
      if (x >= 85 && x <= 235) {
        autoTestRunning = false;
        experimentRunning = false;
        Serial.println("\n[STOPPED]\n");
        
        // 全ポート停止
        for(int i = 0; i < PORT_COUNT; i++) {
          setPeltier(i, 0, 0);
          portStates[i] = STATE_IDLE;
        }
        drawUI();
      }
    } else if (allIdle) {
      // TESTボタン
      if (x >= 20 && x <= 150) {
        startAutoTest();
      }
      // EXPERIMENTボタン
      else if (x >= 170 && x <= 300) {
        startExperiment();
      }
    }
  }
}
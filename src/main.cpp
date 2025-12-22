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

// シリアル通信用（前回の状態を記憶）
String lastSerialMessage = "";

// 自動テスト用変数
bool autoTestRunning = false;
int autoTestPort = 0;
enum AutoTestPhase {
  AUTO_IDLE,
  AUTO_FIRST_SEQ,
  AUTO_TRANSITION,
  AUTO_SECOND_SEQ,
  AUTO_PORT_END
};
AutoTestPhase autoTestPhase = AUTO_IDLE;

bool testStartsWithCool[3] = {true, false, true};

// 実験用変数
bool experimentRunning = false;
const int EXPERIMENT_TRIALS = 20;
int experimentCurrentTrial = 0;
int experimentSequence[EXPERIMENT_TRIALS];
int experimentPorts[EXPERIMENT_TRIALS];
unsigned long experimentIntervals[EXPERIMENT_TRIALS];

enum ExperimentPhase {
  EXP_IDLE,
  EXP_STIMULUS,
  EXP_END_PHASE,
  EXP_INTERVAL
};
ExperimentPhase expPhase = EXP_IDLE;
unsigned long expPhaseStartTime = 0;

const int PREDEFINED_SEQUENCE[EXPERIMENT_TRIALS][2] = {
  {1, 0}, {2, 1}, {0, 0}, {0, 0}, {1, 1},
  {2, 0}, {2, 1}, {0, 1}, {1, 0}, {1, 1},
  {0, 0}, {2, 1}, {1, 0}, {0, 1}, {0, 0},
  {2, 0}, {1, 1}, {1, 0}, {2, 1}, {2, 1}
};

const unsigned long PREDEFINED_INTERVALS[EXPERIMENT_TRIALS] = {
  1200, 1800, 1500, 1100, 1900,
  1400, 1700, 1300, 2000, 1600,
  1200, 1500, 1800, 1100, 1400,
  1900, 1300, 1700, 1600, 1000
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
void sendSerialState(int portIdx, State state);

// シリアル通信：position,strength形式のみ送信
void sendSerialState(int portIdx, State state) {
  String position = "none";
  String strength = "none";
  
  if (state != STATE_IDLE) {
    if (portIdx == 0) position = "Center";
    else if (portIdx == 1) position = "Right";
    else if (portIdx == 2) position = "Left";
  }
  
  switch(state) {
    case STATE_HEAT:
      strength = "Strong";
      break;
    case STATE_COOL:
      strength = "Weak";
      break;
    case STATE_HEAT_START:
    case STATE_COOL_START:
    case STATE_HEAT_END:
    case STATE_COOL_END:
    case STATE_IDLE:
      strength = "none";
      position = "none";
      break;
    default:
      break;
  }
  
  String message = position + "," + strength;
  
  if (message != lastSerialMessage) {
    Serial.println(message);
    lastSerialMessage = message;
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  Serial.begin(115200);
  M5.Display.setBrightness(255);
  
  for(int i = 0; i < PORT_COUNT; i++) {
    ledcSetup(ports[i].ch0, PWM_FREQ, PWM_RES);
    ledcAttachPin(ports[i].pin0, ports[i].ch0);
    ledcWrite(ports[i].ch0, 0);
    
    ledcSetup(ports[i].ch1, PWM_FREQ, PWM_RES);
    ledcAttachPin(ports[i].pin1, ports[i].ch1);
    ledcWrite(ports[i].ch1, 0);
  }
  
  sendSerialState(0, STATE_IDLE);
  drawUI();
}

void loop() {
  M5.update();
  
  if (autoTestRunning) {
    updateAutoTest();
  }
  
  if (experimentRunning) {
    updateExperiment();
  }
  
  for(int i = 0; i < PORT_COUNT; i++) {
    if (portStates[i] != STATE_IDLE) {
      updateStateMachine(i);
    }
  }
  
  handleTouch();
  delay(10);
}

void generateExperimentSequence() {
  for(int i = 0; i < EXPERIMENT_TRIALS; i++) {
    experimentPorts[i] = PREDEFINED_SEQUENCE[i][0];
    experimentSequence[i] = PREDEFINED_SEQUENCE[i][1];
    experimentIntervals[i] = PREDEFINED_INTERVALS[i];
  }
}

void startExperiment() {
  experimentRunning = true;
  experimentCurrentTrial = 0;
  expPhase = EXP_STIMULUS;
  
  generateExperimentSequence();
  
  int port = experimentPorts[0];
  if (experimentSequence[0] == 0) {
    startCoolStimulus(port);
  } else {
    startHeatStimulus(port);
  }
  
  drawUI();
}

void updateExperiment() {
  int currentPort = experimentPorts[experimentCurrentTrial];
  
  if (expPhase == EXP_STIMULUS) {
    if (portStates[currentPort] == STATE_IDLE) {
      expPhase = EXP_END_PHASE;
      
      if (experimentSequence[experimentCurrentTrial] == 0) {
        setPeltier(currentPort, 0, COOL_END_PWM);
        portStates[currentPort] = STATE_COOL_END;
        stateStartTimes[currentPort] = millis();
        sendSerialState(currentPort, STATE_COOL_END);
      } else {
        setPeltier(currentPort, HEAT_END_PWM, 0);
        portStates[currentPort] = STATE_HEAT_END;
        stateStartTimes[currentPort] = millis();
        sendSerialState(currentPort, STATE_HEAT_END);
      }
      drawUI();
    }
  } else if (expPhase == EXP_END_PHASE) {
    if (portStates[currentPort] == STATE_IDLE) {
      sendSerialState(currentPort, STATE_IDLE);
      
      experimentCurrentTrial++;
      
      if (experimentCurrentTrial >= EXPERIMENT_TRIALS) {
        experimentRunning = false;
        drawUI();
      } else {
        expPhase = EXP_INTERVAL;
        expPhaseStartTime = millis();
      }
    }
  } else if (expPhase == EXP_INTERVAL) {
    unsigned long currentInterval = experimentIntervals[experimentCurrentTrial - 1];
    if (millis() - expPhaseStartTime >= currentInterval) {
      expPhase = EXP_STIMULUS;
      
      int port = experimentPorts[experimentCurrentTrial];
      if (experimentSequence[experimentCurrentTrial] == 0) {
        startCoolStimulus(port);
      } else {
        startHeatStimulus(port);
      }
      drawUI();
    }
  }
}

void startAutoTest() {
  autoTestRunning = true;
  autoTestPort = 0;
  autoTestPhase = AUTO_FIRST_SEQ;
  
  startCoolStimulus(0);
  drawUI();
}

void updateAutoTest() {
  if (portStates[autoTestPort] == STATE_IDLE) {
    
    if (autoTestPhase == AUTO_FIRST_SEQ) {
      autoTestPhase = AUTO_TRANSITION;
      
      if (testStartsWithCool[autoTestPort]) {
        setPeltier(autoTestPort, 0, COOL_END_PWM);
        portStates[autoTestPort] = STATE_COOL_END;
        stateStartTimes[autoTestPort] = millis();
        sendSerialState(autoTestPort, STATE_COOL_END);
      } else {
        setPeltier(autoTestPort, HEAT_END_PWM, 0);
        portStates[autoTestPort] = STATE_HEAT_END;
        stateStartTimes[autoTestPort] = millis();
        sendSerialState(autoTestPort, STATE_HEAT_END);
      }
      drawUI();
      
    } else if (autoTestPhase == AUTO_TRANSITION) {
      autoTestPhase = AUTO_SECOND_SEQ;
      
      sendSerialState(autoTestPort, STATE_IDLE);
      
      if (testStartsWithCool[autoTestPort]) {
        startHeatStimulus(autoTestPort);
      } else {
        startCoolStimulus(autoTestPort);
      }
      
    } else if (autoTestPhase == AUTO_SECOND_SEQ) {
      autoTestPhase = AUTO_PORT_END;
      
      if (testStartsWithCool[autoTestPort]) {
        setPeltier(autoTestPort, HEAT_END_PWM, 0);
        portStates[autoTestPort] = STATE_HEAT_END;
        stateStartTimes[autoTestPort] = millis();
        sendSerialState(autoTestPort, STATE_HEAT_END);
      } else {
        setPeltier(autoTestPort, 0, COOL_END_PWM);
        portStates[autoTestPort] = STATE_COOL_END;
        stateStartTimes[autoTestPort] = millis();
        sendSerialState(autoTestPort, STATE_COOL_END);
      }
      drawUI();
      
    } else if (autoTestPhase == AUTO_PORT_END) {
      sendSerialState(autoTestPort, STATE_IDLE);
      
      autoTestPort++;
      
      if (autoTestPort >= PORT_COUNT) {
        autoTestRunning = false;
        drawUI();
      } else {
        autoTestPhase = AUTO_FIRST_SEQ;
        delay(1000);
        
        if (testStartsWithCool[autoTestPort]) {
          startCoolStimulus(autoTestPort);
        } else {
          startHeatStimulus(autoTestPort);
        }
      }
    }
  }
}

void setPeltier(int portIdx, int powerCool, int powerHeat) {
  ledcWrite(ports[portIdx].ch0, powerCool);
  ledcWrite(ports[portIdx].ch1, powerHeat);
}

void startHeatStimulus(int portIdx) {
  setPeltier(portIdx, 0, HEAT_START_PWM);
  portStates[portIdx] = STATE_HEAT_START;
  stateStartTimes[portIdx] = millis();
  sendSerialState(portIdx, STATE_HEAT_START);
  drawUI();
}

void startCoolStimulus(int portIdx) {
  setPeltier(portIdx, COOL_START_PWM, 0);
  portStates[portIdx] = STATE_COOL_START;
  stateStartTimes[portIdx] = millis();
  sendSerialState(portIdx, STATE_COOL_START);
  drawUI();
}

void updateStateMachine(int portIdx) {
  unsigned long elapsed = millis() - stateStartTimes[portIdx];
  
  switch(portStates[portIdx]) {
    case STATE_HEAT_START:
      if (elapsed >= HEAT_START_TIME) {
        setPeltier(portIdx, 0, HEAT_PWM);
        portStates[portIdx] = STATE_HEAT;
        stateStartTimes[portIdx] = millis();
        sendSerialState(portIdx, STATE_HEAT);
        drawUI();
      }
      break;
      
    case STATE_HEAT:
      if (elapsed >= HEAT_TIME) {
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        sendSerialState(portIdx, STATE_IDLE);
        drawUI();
      }
      break;
      
    case STATE_HEAT_END:
      if (elapsed >= HEAT_END_TIME) {
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        sendSerialState(portIdx, STATE_IDLE);
        drawUI();
      }
      break;
      
    case STATE_COOL_START:
      if (elapsed >= COOL_START_TIME) {
        setPeltier(portIdx, COOL_PWM, 0);
        portStates[portIdx] = STATE_COOL;
        stateStartTimes[portIdx] = millis();
        sendSerialState(portIdx, STATE_COOL);
        drawUI();
      }
      break;
      
    case STATE_COOL:
      if (elapsed >= COOL_TIME) {
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        sendSerialState(portIdx, STATE_IDLE);
        drawUI();
      }
      break;
      
    case STATE_COOL_END:
      if (elapsed >= COOL_END_TIME) {
        setPeltier(portIdx, 0, 0);
        portStates[portIdx] = STATE_IDLE;
        sendSerialState(portIdx, STATE_IDLE);
        drawUI();
      }
      break;
      
    default:
      break;
  }
}

void drawUI() {
  M5.Display.clear(BLACK);
  
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(20, 10);
  M5.Display.println("Peltier Test");
  
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
  
  if (experimentRunning) {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 95);
    M5.Display.printf("EXP: %d/20", experimentCurrentTrial + 1);
  }
  
  bool allIdle = true;
  for(int i = 0; i < PORT_COUNT; i++) {
    if (portStates[i] != STATE_IDLE) {
      allIdle = false;
      break;
    }
  }
  
  M5.Display.setTextSize(2);
  
  if (allIdle && !autoTestRunning && !experimentRunning) {
    M5.Display.fillRect(20, 130, 130, 40, GREEN);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(40, 143);
    M5.Display.println("TEST");
    
    M5.Display.fillRect(170, 130, 130, 40, CYAN);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(190, 143);
    M5.Display.println("EXP");
  } else if (autoTestRunning || experimentRunning) {
    M5.Display.fillRect(85, 130, 150, 40, RED);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(115, 143);
    M5.Display.println("STOP");
  }
}

void handleTouch() {
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return;
  
  int x = t.x;
  int y = t.y;
  
  if (y >= 130 && y <= 170) {
    bool allIdle = true;
    for(int i = 0; i < PORT_COUNT; i++) {
      if (portStates[i] != STATE_IDLE) {
        allIdle = false;
        break;
      }
    }
    
    if (autoTestRunning || experimentRunning) {
      if (x >= 85 && x <= 235) {
        autoTestRunning = false;
        experimentRunning = false;
        
        for(int i = 0; i < PORT_COUNT; i++) {
          setPeltier(i, 0, 0);
          portStates[i] = STATE_IDLE;
        }
        sendSerialState(0, STATE_IDLE);
        drawUI();
      }
    } else if (allIdle) {
      if (x >= 20 && x <= 150) {
        startAutoTest();
      }
      else if (x >= 170 && x <= 300) {
        startExperiment();
      }
    }
  }
}
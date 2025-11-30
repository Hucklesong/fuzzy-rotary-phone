#include <Servo.h>

// ==============================
// RGB LED 핀 (G = 5로 이동)
// ==============================
int R = 9;
int G = 5;     // ★ ip5=10과 충돌하므로 5번으로 변경 (PWM)
int B = 11;

// LED 제어 함수
void setLED(int r, int g, int b) {
  analogWrite(R, r);
  analogWrite(G, g);
  analogWrite(B, b);
}

// ==============================
// 선체 제어 RC 핀 (절대 변경 금지)
// ==============================
#define ip1 3    // CH1
#define ip3 6    // CH3
#define ip5 10   // CH5 (Mode Switch)

// ==============================
// Thruster
// ==============================
Servo thruster1;   // RIGHT (13)
Servo thruster2;   // LEFT  (12)

// RC 변수
int ch1, ch3, ch5;

// 필터
float smooth_ch1 = 1500.0;
float smooth_ch3 = 1500.0;

// AUTO 변수
String inputString = "";
boolean stringComplete = false;
int cmdL = 1500;
int cmdR = 1500;

unsigned long lastSerialTime = 0;

// RC failsafe
unsigned long lastRcTime = 0;
const unsigned long RC_FAILSAFE_TIMEOUT = 500;

// 모드 플래그
bool isAutoMode = false;
bool prevAutoMode = false;

// 램핑
int outL = 1500;
int outR = 1500;

// Deadzone & Ramp
const int NEUTRAL_WIDTH = 40;
const int RAMP_STEP = 20;

// 디버그 타이머
unsigned long lastDebugTime = 0;


// ==============================
// 유틸 함수들
// ==============================
int applyNeutralDeadzone(int value) {
  if (value > 1500 - NEUTRAL_WIDTH && value < 1500 + NEUTRAL_WIDTH)
    return 1500;
  return value;
}

int rampTowards(int current, int target, int step) {
  if (current < target - step) return current + step;
  if (current > target + step) return current - step;
  return target;
}


// ==============================
// SETUP
// ==============================
void setup() {
  Serial.begin(115200);
  inputString.reserve(200);

  // RGB LED
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  setLED(0, 0, 0);  // LED OFF

  // RC 입력
  pinMode(ip1, INPUT);
  pinMode(ip3, INPUT);
  pinMode(ip5, INPUT);

  // Thruster
  thruster1.attach(13);
  thruster2.attach(12);

  // ESC 초기 정지
  for (int i = 0; i < 30; i++) {
    thruster1.writeMicroseconds(1500);
    thruster2.writeMicroseconds(1500);
    delay(50);
  }
}


// ==============================
// LOOP
// ==============================
void loop() {

  int ch5_raw = pulseIn(ip5, HIGH, 25000);

  // ============================
  // CH5 모드 판정 + LED 표시
  // ============================
  if (ch5_raw > 900 && ch5_raw < 1300) {
    isAutoMode = true;
    setLED(255, 160, 0);      // ★ AUTO = 노란색
  }
  else if (ch5_raw > 1700 && ch5_raw < 2100) {
    isAutoMode = false;
    setLED(0, 255, 0);        // ★ RC = 초록색
  }

  // 모드 전환 시 충격 방지
  if (isAutoMode != prevAutoMode) {
    outL = 1500;
    outR = 1500;
    thruster1.writeMicroseconds(1500);
    thruster2.writeMicroseconds(1500);
    delay(100);
  }

  // 동작
  if (isAutoMode) {
    run_autonomous();
  } else {
    run_rc();
  }

  prevAutoMode = isAutoMode;

  // 시리얼 명령 처리
  if (stringComplete) {
    parseCommand();
    inputString = "";
    stringComplete = false;
  }

  // 디버그 출력
  if (millis() - lastDebugTime > 500) {
    Serial.print("MODE: ");
    if (isAutoMode) {
      Serial.print("AUTO | CMD: L");
      Serial.print(cmdL);
      Serial.print(" R");
      Serial.println(cmdR);
    } else {
      Serial.print("RC | CH5_RAW: ");
      Serial.println(ch5_raw);
    }
    lastDebugTime = millis();
  }
}


// ==============================
// RC MODE
// ==============================
void run_rc() {
  int raw_ch3 = pulseIn(ip3, HIGH, 25000);
  int raw_ch1 = pulseIn(ip1, HIGH, 25000);

  if (raw_ch3 == 0) raw_ch3 = 1500;
  if (raw_ch1 == 0) raw_ch1 = 1500;

  bool valid = (raw_ch3 > 900 && raw_ch3 < 2100 &&
                raw_ch1 > 900 && raw_ch1 < 2100);

  if (!valid) {
    raw_ch3 = 1500;
    raw_ch1 = 1500;
  } else {
    lastRcTime = millis();
  }

  if (millis() - lastRcTime > RC_FAILSAFE_TIMEOUT) {
    thruster1.writeMicroseconds(1500);
    thruster2.writeMicroseconds(1500);
    return;
  }

  smooth_ch3 = smooth_ch3 * 0.8 + raw_ch3 * 0.2;
  smooth_ch1 = smooth_ch1 * 0.8 + raw_ch1 * 0.2;

  int clean_ch3 = (int)smooth_ch3;
  int clean_ch1 = (int)smooth_ch1;

  if ((clean_ch3 > 1460 && clean_ch3 < 1540) &&
      (clean_ch1 > 1460 && clean_ch1 < 1540)) {

    thruster1.writeMicroseconds(1500);
    thruster2.writeMicroseconds(1500);

  } else {
    long Tx = map(clean_ch3, 1100, 1900, -400, 400);
    long Tz = map(clean_ch1, 1100, 1900, -400, 400);

    int RIGHT = 1500 + Tx - Tz;
    int LEFT  = 1500 + Tx + Tz;

    thruster1.writeMicroseconds(constrain(RIGHT, 1100, 1900));
    thruster2.writeMicroseconds(constrain(LEFT, 1100, 1900));
  }
}


// ==============================
// AUTO MODE
// ==============================
void run_autonomous() {

  if (millis() - lastSerialTime > 1000) {
    cmdL = 1500;
    cmdR = 1500;
  }

  int targetL = applyNeutralDeadzone(cmdL);
  int targetR = applyNeutralDeadzone(cmdR);

  outL = rampTowards(outL, targetL, RAMP_STEP);
  outR = rampTowards(outR, targetR, RAMP_STEP);

  thruster1.writeMicroseconds(outR);
  thruster2.writeMicroseconds(outL);
}


// ==============================
// SERIAL EVENT
// ==============================
void serialEvent() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    inputString += c;
    if (c == '\n') stringComplete = true;
  }
}


// ==============================
// AUTONOMOUS COMMAND PARSER
// ==============================
void parseCommand() {
  int l_index = inputString.indexOf('L');
  int r_index = inputString.indexOf('R');

  if (l_index > -1 && r_index > -1) {
    cmdL = inputString.substring(l_index + 1, r_index).toInt();
    cmdR = inputString.substring(r_index + 1).toInt();

    cmdL = constrain(cmdL, 1100, 1900);
    cmdR = constrain(cmdR, 1100, 1900);

    lastSerialTime = millis();
  }
}

/*

 * [Kaboat Racing - Speed Maintain Ver]

 * - 이전 작동 코드 베이스로 복구 (안정성 확보)

 * - 조향(Steering) 입력 제한으로 코너링 시 속도 유지

 * - 입력 감도 부스트 (살짝만 밀어도 풀파워) 유지

 */

 

#include <Servo.h>

 

// --- 핀 정의 ---

#define ip1 3   // CH1 (Steering)

#define ip3 6   // CH3 (Throttle)

#define ip5 10  // CH5 (Mode)

 

Servo thruster1; // Right (13)

Servo thruster2; // Left  (12)

 

// --- 튜닝 설정값 ---

// 조향 제한값 (STEER_LIMIT)

// 400 = 급회전 (안쪽 모터 정지)

// 200 = 부드러운 회전 (안쪽 모터 중간 속도 유지) -> 요청하신 설정

const int STEER_LIMIT = 200; 

 

void setup() {

  Serial.begin(115200);

 

  pinMode(ip1, INPUT);

  pinMode(ip3, INPUT);

  pinMode(ip5, INPUT);

 

  thruster1.attach(13);

  thruster2.attach(12);

 

  // 초기화 (시동 대기)

  for (int i = 0; i < 20; i++) {

    thruster1.writeMicroseconds(1500);

    thruster2.writeMicroseconds(1500);

    delay(50);

  }

}

 

void loop() {

  // 1. 입력 읽기

  // pulseIn 타임아웃을 30000으로 늘려 안정성 확보

  int raw_ch3 = pulseIn(ip3, HIGH, 30000); 

  int raw_ch1 = pulseIn(ip1, HIGH, 30000); 

 

  // 2. 안전장치 (신호 끊김 시 정지)

  if (raw_ch3 == 0 || raw_ch1 == 0) {

    thruster1.writeMicroseconds(1500);

    thruster2.writeMicroseconds(1500);

    return;

  }

 

  // 3. 레이싱 로직 계산

  // ---------------------------------------------------------

  

  // [A] 스로틀 계산 (감도 부스트 적용)

  // 1150 ~ 1850 범위만 들어오면 -400 ~ 400 풀파워 출력

  // (map 함수가 범위를 넘어서면 400보다 큰 값을 뱉으므로 constrain 필수)

  long throttle = map(raw_ch3, 1150, 1850, -400, 400);

  

  // 데드존 처리 (중립 잡기)

  if (throttle > -50 && throttle < 50) throttle = 0;

  

  // 값 제한 (-400 ~ 400)

  throttle = constrain(throttle, -400, 400);

 

 

  // [B] 조향 계산 (★ 속도 유지 핵심 ★)

  // 입력은 똑같이 민감하게 받되, 출력값만 STEER_LIMIT(200)으로 자릅니다.

  long steering = map(raw_ch1, 1150, 1850, -400, 400);

 

  // 데드존 처리

  if (steering > -50 && steering < 50) steering = 0;

 

  // ★★★ 여기가 핵심 수정 사항 ★★★

  // 조향 힘을 STEER_LIMIT(200)까지만 쓰도록 강제합니다.

  // 스틱을 끝까지 꺾어도 200까지만 꺾입니다.

  steering = constrain(steering, -STEER_LIMIT, STEER_LIMIT);

 

 

  // [C] 믹싱 및 출력

  // 예: 풀악셀(400) + 풀조향(200)

  // Left  = 400 + 200 = 600 -> 1900 (최대 속도)

  // Right = 400 - 200 = 200 -> 1700 (중간 속도 유지!) -> 안 멈춤!

  

  int left_pwm = 1500 + throttle + steering;

  int right_pwm = 1500 + throttle - steering;

 

  // 물리적 한계 제한 (ESC 보호)

  left_pwm = constrain(left_pwm, 1100, 1900);

  right_pwm = constrain(right_pwm, 1100, 1900);

 

  // 모터 출력

  // (만약 좌우가 반대로 움직이면 아래 두 줄의 변수(right_pwm, left_pwm)를 서로 바꾸세요)

  thruster1.writeMicroseconds(right_pwm);

  thruster2.writeMicroseconds(left_pwm);

}

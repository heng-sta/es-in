enum DriveState {
  STATE_FORWARD,
  STATE_LEFT_FIX,
  STATE_RIGHT_FIX,
  STATE_HARD_LEFT,
  STATE_HARD_RIGHT
};

// 1. 腳位定義
const int motor1Pin1 = 27; const int motor1Pin2 = 26; const int enable1Pin = 14; // 左馬達
const int motor2Pin1 = 25; const int motor2Pin2 = 33; const int enable2Pin = 32; // 右馬達

const int sensorPins[5] = {19, 18, 5, 17, 16}; // 5路循跡感測器
const int BOOT_BUTTON = 0;                     // BOOT 按鈕

// RGB LED 腳位
const int redPin = 4; const int greenPin = 2; const int bluePin = 15;

// 2. PWM 參數與通道設定
const int freq = 30000;
const int resolution = 8;
const int channelA = 0; const int channelB = 1; // 馬達
const int chRed = 2; const int chGreen = 3; const int chBlue = 4; // LED

// 3. 速度與邏輯參數
const int MAX_SPEED = 255;
const int REVERSE_SPEED = 200;
const int TURN_SPEED = 100;
const unsigned long TIMEOUT_LIMIT = 5000; // 超出賽道容許時間：5000 毫秒 (5秒)

// 4. 狀態變數
bool isRunning = false;          // false = 安全狀態 (綠燈), true = 自主導航狀態 (紅燈)
int lastButtonState = HIGH;      
unsigned long lastPressTime = 0; 

// 脫軌計時變數
unsigned long outOfTrackStartTime = 0; // 紀錄開始脫軌的時間點
bool isOutOfTrack = false;             // 目前是否處於脫軌狀態

// 驅動函式
void moveCar(int leftSpeed, bool leftForward, int rightSpeed, bool rightForward) {
  // 左馬達轉向控制
  if (leftForward) {
    digitalWrite(motor1Pin1, LOW);  digitalWrite(motor1Pin2, HIGH);
  } else {
    digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
  }
  ledcWrite(enable1Pin, leftSpeed);

  // 右馬達轉向控制
  if (rightForward) {
    digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, HIGH);
  } else {
    digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
  }
  ledcWrite(enable2Pin, rightSpeed);
}

// 直行/修正驅動函式
void drive(int leftSpeed, int rightSpeed) {
  moveCar(leftSpeed, true, rightSpeed, true); // 預設皆為正轉
}

// RGB LED 顏色控制
void setLedColor(int r, int g, int b) {
  ledcWrite(redPin, r);
  ledcWrite(greenPin, g);
  ledcWrite(bluePin, b);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(motor1Pin1, OUTPUT); pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT); pinMode(motor2Pin2, OUTPUT);
  for(int i=0; i<5; i++) pinMode(sensorPins[i], INPUT);

  ledcAttachChannel(enable1Pin, freq, resolution, channelA);
  ledcAttachChannel(enable2Pin, freq, resolution, channelB);
  ledcAttachChannel(redPin, freq, resolution, chRed);
  ledcAttachChannel(greenPin, freq, resolution, chGreen);
  ledcAttachChannel(bluePin, freq, resolution, chBlue);

  // 初始：安全狀態，綠燈恆亮
  setLedColor(0, 255, 0);
}

void loop() {
  // A. 按鈕單擊偵測 (切換安全/導航狀態)
  int currentButtonState = digitalRead(BOOT_BUTTON);
  static bool resetStateOnResume = false; // 歷史狀態清除標記
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    if (millis() - lastPressTime > 250) { 
      isRunning = !isRunning;
      lastPressTime = millis();
      
      if (isRunning) {
        delay(500);
        isOutOfTrack = false; 
        resetStateOnResume = true; // 標記需要重置狀態機
      } else {
        drive(0, 0);
        setLedColor(0, 255, 0); 
      }
    }
  }
  lastButtonState = currentButtonState;

  //  B. 核心運行邏輯
  if (isRunning) {
    int s[5];
    for(int i=0; i<5; i++) s[i] = digitalRead(sensorPins[i]);

    if (s[0] == 1 && s[1] == 1 && s[2] == 1 && s[3] == 1 && s[4] == 1) {
      if (!isOutOfTrack) {
        outOfTrackStartTime = millis();
        isOutOfTrack = true;
      }

      unsigned long elapsed = millis() - outOfTrackStartTime;

      if (elapsed >= TIMEOUT_LIMIT) {
        isRunning = false;      
        drive(0, 0);           
        setLedColor(0, 255, 0); 
        isOutOfTrack = false;   
        return;                 
      }
    } 
    else {
      setLedColor(255, 0, 0);
      isOutOfTrack = false; 
      outOfTrackStartTime = millis();

      //  核心控制邏輯
      static DriveState lastValidState = STATE_FORWARD;
      static unsigned long lastStateChangeTime = 0;
      const unsigned long STATE_DEBOUNCE_TIME = 15;

      // 響應按鈕重啟，強行刷新歷史記憶
      if (resetStateOnResume) {
        lastValidState = STATE_FORWARD;
        lastStateChangeTime = millis();
        resetStateOnResume = false;
      }

      DriveState targetState = lastValidState;

      // 4路全0狀況
      bool isHardLeftCondition  = (s[0] == 0 && s[1] == 0) || (s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0);
      bool isHardRightCondition = (s[3] == 0 && s[4] == 0) || (s[1] == 0 && s[2] == 0 && s[3] == 0 && s[4] == 0);

      // 條件判定與狀態鎖定
      if (isHardLeftCondition) {
        targetState = STATE_HARD_LEFT;   // 直角左轉
      }
      else if (isHardRightCondition) {
        targetState = STATE_HARD_RIGHT;  // 直角右轉
      }
      // 前一幀是大左轉，強制鎖定大轉彎
      else if (lastValidState == STATE_HARD_LEFT && (s[0] == 0 || s[1] == 0) && s[2] == 1) {
        targetState = STATE_HARD_LEFT;
      }
      // 前一幀是大右轉，強制鎖定大轉彎
      else if (lastValidState == STATE_HARD_RIGHT && (s[4] == 0 || s[3] == 0) && s[2] == 1) {
        targetState = STATE_HARD_RIGHT;
      }
      else if (s[2] == 0) {
        targetState = STATE_FORWARD;     // 直行
      }
      else if (s[1] == 0 || s[0] == 0) {
        targetState = STATE_LEFT_FIX;    // 左修
      }
      else if (s[3] == 0 || s[4] == 0) {
        targetState = STATE_RIGHT_FIX;   // 右修
      }
      else {
        // 全白：維持前一個狀態
        targetState = lastValidState;
      }

      // 避免高頻切換狀態
      if (targetState != lastValidState) {
        if (millis() - lastStateChangeTime >= STATE_DEBOUNCE_TIME) {
          lastValidState = targetState;
          lastStateChangeTime = millis();
        } else {
          targetState = lastValidState;
        }
      }

      // 執行
      switch (targetState) {
        case STATE_HARD_LEFT:
          // 直角左轉
          moveCar(REVERSE_SPEED, false, 230, true);
          break;
          
        case STATE_HARD_RIGHT:
          // 直角右轉
          moveCar(230, true, REVERSE_SPEED, false);
          break;
          
        case STATE_FORWARD:
          drive(220, 220); // 直行
          break;
          
        case STATE_LEFT_FIX:
          drive(40, 165);  // 左修
          break;
          
        case STATE_RIGHT_FIX:
          drive(165, 40);  // 右修
          break;
      }
    }
  }
  delayMicroseconds(500);
}
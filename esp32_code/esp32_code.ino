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
const int REVERSE_SPEED = 230;
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
void drive(int leftSpeed, int rightSpeed) {
  digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, HIGH);
  ledcWrite(enable1Pin, leftSpeed);
  digitalWrite(motor2Pin1, LOW); digitalWrite(motor2Pin2, HIGH);
  ledcWrite(enable2Pin, rightSpeed);
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
  pinMode(BOOT_BUTTON, INPUT_PULLUP); 

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
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    if (millis() - lastPressTime > 250) { 
      isRunning = !isRunning;
      lastPressTime = millis();
      
      if (isRunning) {
        // 剛啟動時重置脫軌計時
        isOutOfTrack = false; 
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
    for(int i=0; i<5; i++) s[i] = digitalRead(sensorPins[i]); // 5路數位訊號

    // 判斷是否完全超出賽道 (5路皆為 1，代表全白地板) 
    if (s[0] == 1 && s[1] == 1 && s[2] == 1 && s[3] == 1 && s[4] == 1) {
      
      // 衝出賽道即紀錄時刻
      if (!isOutOfTrack) {
        outOfTrackStartTime = millis(); // 記錄精確脫軌時間原點
        isOutOfTrack = true;
      }

      // 計算脫軌毫秒數
      unsigned long elapsed = millis() - outOfTrackStartTime;

      // 檢查是否超過 5 秒
      if (elapsed >= TIMEOUT_LIMIT) {
        isRunning = false;      // 關閉導航狀態
        drive(0, 0);           // 馬達煞停
        setLedColor(0, 255, 0); // 恢復綠燈
        isOutOfTrack = false;   // 狀態清空
        return;                 // 跳出本次循環
      }
    } 
    else {
      // 正常在賽道內：自主導航狀態，紅燈恆亮
      setLedColor(255, 0, 0);
      isOutOfTrack = false; 
      outOfTrackStartTime = millis(); // 只要在軌道上，時間原點與現在時間同步

      if (s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0) {
        // 直角左轉：左輪直接反轉 (向後拉)，右輪全速向前
        digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW); // 左輪反轉
        ledcWrite(enable1Pin, REVERSE_SPEED); // 給予定義的 230 反轉速
        digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, HIGH); // 右輪正轉
        ledcWrite(enable2Pin, MAX_SPEED); // 外側輪全速 255
      }
      else if (s[1] == 0 && s[2] == 0 && s[3] == 0 && s[4] == 0) { 
        // 直角右轉
        digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, HIGH);
        ledcWrite(enable1Pin, MAX_SPEED); 
        digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
        ledcWrite(enable2Pin, REVERSE_SPEED); 
      }
      else if (s[0] == 0 && s[1] == 0 && s[2] == 0) {
        // 直角左轉
        digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
        ledcWrite(enable1Pin, REVERSE_SPEED); 
        digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, HIGH);
        ledcWrite(enable2Pin, MAX_SPEED); 
      }
      else if (s[2] == 0 && s[3] == 0 && s[4] == 0) { 
        // 直角右轉
        digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, HIGH);
        ledcWrite(enable1Pin, MAX_SPEED);
        digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
        ledcWrite(enable2Pin, REVERSE_SPEED);
      }
      else if (s[1] == 0 && s[0] == 0) {
        // 直角左轉
        digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
        ledcWrite(enable1Pin, REVERSE_SPEED); 
        digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, HIGH);
        ledcWrite(enable2Pin, MAX_SPEED);
      }
      else if (s[3] == 0 && s[4] == 0) { 
         // 直角右轉
        digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, HIGH);
        ledcWrite(enable1Pin, MAX_SPEED);
        digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
        ledcWrite(enable2Pin, REVERSE_SPEED);
      }
      else if (s[2] == 0) { 
        drive(230,230); // 直行全速
      } 
      else if (s[1] == 0 || s[0] == 0) { 
        drive(0, MAX_SPEED); // 左修
      } 
      else if (s[3] == 0 || s[4] == 0) { 
        drive(MAX_SPEED, 0); // 右修
      }
      else { 
        drive(230,230); // 直行全速
      }
    }
  }
  delayMicroseconds(500);
}
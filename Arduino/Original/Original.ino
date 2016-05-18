#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <TimedAction.h>

// Lab12 使用兩顆 74HC595 和三支腳位控制 16 顆 LED
// 接 74HC595 的 SH_CP (pin 11, clock pin)
const int CLOCK_PIN = 10;
// 接 74HC595 的 ST_CP (pin 12,latch pin)
const int LATCH_PIN = 9;
// 接 74HC595 的 DS (pin 14)
const int DATA_PIN = 8;
// 掃描鍵 Pin 腳
const int KEYPAID_PIN = A0;
// 蜂鳴器 Pin 腳，僅可使用 PWM
const int BUZZER_PIN = 3;
// 上層 UV LED 板 Pin 腳，僅可使用 PWM
const int UP_PANEL_PIN = 6;
// 上層 UV LED 板 Pin 腳，僅可使用 PWM
const int DN_PANEL_PIN = 5;
// 心跳 LED Pin 腳
const int HEARTBEAT_PIN = 13;
// 類比按鍵重置鈕 Pin 腳
const int KEY_SCAN_PIN = 4;
// 最大亮度 %
const int LIGHT_MAX = 100;
// 最小亮度 %
const int LIGHT_MIN = 5;
// 亮度增減量 %
const int LIGHT_STEP = 5;
// 曝光最長時間，單位秒
const int MAX_TIMER = 300;
// 曝光最長時間，單位秒
const int MIN_TIMER = 10;
// 曝光時間增減量，單位秒
const int TIMER_ADD_STEP = 10;
// 預設的按鍵值
const int DEF_KEY_CODE[8] PROGMEM = { 
  825, 820, 618, 614, 410, 406, 205, 201 };
// 警告音音頻
const int SOUND_PULSE = 4435;

int kv[8] = { 
  0, 0, 0, 0, 0, 0, 0, 0 };

boolean pressStart = false;
boolean pressMode = false;
boolean pressCount = false;
boolean starting = false;      // 是否開始曝光
boolean bHeartbeat = false;
boolean playAlert = false;
boolean bBlink = false;
boolean scanMode = false;
boolean scanStart = false;
boolean onEvasive = false;

int iCountDown = 10;           // 預設開始秒數
int iStartDown = 0;
int iMode = 2;                 // 預設模式，上下層
int iLight = 50;               // 亮度百分比
int scanKeyPressTime = 0;
int scanKeyNo = 0;
int scanTimer = 0;
int scanValue = 0;
int scanMax = 0;
int scanMin = 1023;
int evasiveCount = 0;
int playIndex = 0;

const byte LEDs[11] = {
  B00111111,  // 0, pgfedcba
  B00000110,  // 1
  B01011011,  // 2
  B01001111,  // 3
  B01100110,  // 4
  B01101101,  // 5
  B01111101,  // 6
  B00100111,  // 7
  B01111111,  // 8
  B01101111,  // 9
  B01110011   // P
};
const byte MODEs[4] = {
  B00000001,   // 上標線, 亮上光盤
  B00001000,   // 下標線, 亮下光盤
  B00001001,   // 上下標線, 亮上下兩光盤
  B01001001    // 上下標線加減號(-), 亮度設定模式
};

TimedAction taBlink = TimedAction(500, Blink);
TimedAction taHeartbeat = TimedAction(500, Heartbeat);
TimedAction taCheckButton = TimedAction(200, CheckButtons);
TimedAction taPlayAlert = TimedAction(100, PlayAlert);
TimedAction taShowCountDown = TimedAction(1, ShowCountDown);
TimedAction taEvasiveKeyCode = TimedAction(1, EvasiveKeyCode);
TimedAction taShowSoundPulse = TimedAction(1, ShowSoundPulse);


void setup() {
  Serial.begin(9600);
  ReadKeyCodes();
  // 將 LATCH_PIN, CLOCK_PIN, DATA_PIN 設置為輸出
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(HEARTBEAT_PIN, OUTPUT);
  pinMode(UP_PANEL_PIN, OUTPUT);
  digitalWrite(UP_PANEL_PIN, LOW);
  pinMode(DN_PANEL_PIN, OUTPUT);
  digitalWrite(DN_PANEL_PIN, LOW);
  pinMode(KEYPAID_PIN, INPUT);
  digitalWrite(KEYPAID_PIN, LOW);
  taHeartbeat.disable();
  taEvasiveKeyCode.disable();
  taPlayAlert.disable();
}

void loop() {
  taBlink.check();
  if (!onEvasive)
    taCheckButton.check();
  taHeartbeat.check();
  if (!scanMode)
    taShowCountDown.check();
  else {
    if (onEvasive)
      taEvasiveKeyCode.check();
    else {
      if (!scanStart)
        ShowScanKeyNo();
      else
        ShowNumber(scanValue);
    }
  }
  taPlayAlert.check();
}
void ReadKeyCodes() {
  int i = 0;
  int idx = 0;
  int v = 0;
  byte bh = 0;
  byte bl = 0;
  for (i = 0; i < 16; i += 2) {
    bh = EEPROM.read(i);
    bl = EEPROM.read(i + 1);
    idx = (i + 1) / 2;
    if (bh == 0xFF) {
      v = pgm_read_word(&DEF_KEY_CODE[idx]);
      kv[idx] = v;
      EEPROM.write(i, kv[idx] / 256);
      EEPROM.write(i + 1, kv[idx] % 256);
    } 
    else {
      kv[idx] = bh * 256 + bl;
    }
  }
}
void SaveKeyCodes() {
  int idx = 0;
  int i = 0;
  for (i = 0; i < 16; i += 2) {
    idx = (i + 1) / 2;
    EEPROM.write(i, kv[idx] / 256);
    EEPROM.write(i + 1, kv[idx] % 256);
  }
}
void Blink() {
  bBlink = !bBlink;
  digitalWrite(HEARTBEAT_PIN, (bBlink) ? HIGH : LOW);
}
void Heartbeat() {
  if (starting) {
    if (iCountDown > 0) {
      bHeartbeat = !bHeartbeat;
      Serial.println(iCountDown);
      if (!bHeartbeat)
        iCountDown--;
    }
    else 
    {
      digitalWrite(UP_PANEL_PIN, LOW);
      digitalWrite(DN_PANEL_PIN, LOW);
      starting = false;
      playIndex = 0;
      taPlayAlert.enable();
      playAlert = true;
      bHeartbeat = false;      
    }
  }
}
void CheckButtons() {
  int val = 0;
  int v = 0;
  if (!scanMode) {
    boolean scanKeyDown = digitalRead(KEY_SCAN_PIN);
    if (scanKeyDown) {
      if (scanKeyPressTime == 0)
        scanKeyPressTime = millis();
      else {
        v = millis();
        if (v - scanKeyPressTime >= 3000) {
          scanStart = false;
          scanKeyNo = 0;
          scanMode = true;
          scanKeyPressTime = 0;
          ReadKeyCodes();
          return;
        }
      }
    }
    else {
      scanKeyPressTime = 0;
    }
  } 
  else {
    val = analogRead(KEYPAID_PIN);
    if (!scanStart) {
      if (val > 50) {
        scanTimer = millis();
        scanStart = true;      
        scanValue = val;
        scanMax = val;
        scanMin = val;
      } 
      else {
        scanStart = false;
        scanTimer = 0;
      }
    } 
    else {
      int now = millis();
      scanValue = val;
      if (now - scanTimer >= 1500) {
        scanMax = max(scanMax, val);
        scanMin = min(scanMin, val);
      }
      if (now - scanTimer >= 5000) {
        kv[scanKeyNo * 2] = scanMax + 2;
        kv[scanKeyNo * 2 + 1] = scanMin - 2;
        taEvasiveKeyCode.enable();
        EvasiveKeyCode();
      }
    } 
    return;
  }
  val = analogRead(KEYPAID_PIN);
  if (!starting && !playAlert) {
    if (!pressMode && val >= kv[5] && val <= kv[4]) {
      iMode++;
      if (iMode >= 4) 
        iMode = 0;
      pressMode = true;
    }
    // 放開模式鍵時
    if (pressMode && (val < kv[5] || val > kv[4]))
      pressMode = false;
    if (!pressCount && ((val >= kv[1] && val <= kv[0]) || (val >= kv[3] && val <= kv[2]))) {
      if (iMode != 3) {
        if ((val >= kv[1] && val <= kv[0]))
          iCountDown -= TIMER_ADD_STEP;
        else
          iCountDown += TIMER_ADD_STEP;
        if (iCountDown > MAX_TIMER) 
          iCountDown = MIN_TIMER;
        else if (iCountDown <= 0)
          iCountDown = MAX_TIMER;
      }
      else {
        if ((val >= kv[1] && val <= kv[0]))
          iLight -= LIGHT_STEP;
        else
          iLight += LIGHT_STEP;
        if (iLight > LIGHT_MAX)
          iLight = LIGHT_MIN;
        else if (iLight <= 0)
          iLight = LIGHT_MAX;
      }
      pressCount = true;
    }
    // 放開秒數/亮度鍵時 + 鍵
    if (pressCount && (val < kv[1] || (val > kv[0] && val < kv[3]) || val > kv[2]))
      pressCount = false;
  }  
  if (iMode != 3) {
    if (!pressStart && val >= kv[7] && val <= kv[6]) {
      if (iCountDown > 0) {
        starting = !starting;
        iStartDown = iCountDown;
        if (starting) {
          taHeartbeat.enable();
          byte bLight = map(iLight, 0, 100, 0, 255);
          switch (iMode) {
          case 0:
            analogWrite(UP_PANEL_PIN, bLight);
            digitalWrite(DN_PANEL_PIN, LOW);
            break;
          case 1:
            digitalWrite(UP_PANEL_PIN, LOW);
            analogWrite(DN_PANEL_PIN, bLight);
            break;
          case 2:
            analogWrite(UP_PANEL_PIN, bLight);
            analogWrite(DN_PANEL_PIN, bLight);
            break;
          default:
            digitalWrite(UP_PANEL_PIN, LOW);
            digitalWrite(DN_PANEL_PIN, LOW);
            break;
          }
        }
        else {
          taHeartbeat.disable();
          digitalWrite(UP_PANEL_PIN, LOW);
          digitalWrite(DN_PANEL_PIN, LOW);
        }
      }
      if (playAlert) {
        taPlayAlert.disable();
        playAlert = false;
        playIndex = 0;
        noTone(BUZZER_PIN); 
        iCountDown = iStartDown;
      }
      pressStart = true;
    }
    // 放開開始鍵時
    if (pressStart && (val < kv[7] || val > kv[6]))
      pressStart = false;
  }
}
void EvasiveKeyCode() {
  onEvasive = true;
  evasiveCount++;
  if (evasiveCount % 500 > 250)
    ShowNumber(scanValue);
  else
    ShowNumber(-1);
  if (evasiveCount >= 1500) {
    onEvasive = false;
    evasiveCount = 0;
    taEvasiveKeyCode.disable();
    scanTimer = 0;
    scanStart = false;
    scanKeyNo++;
    if (scanKeyNo >= 4) {
      scanMode = false;
      SaveKeyCodes();
    }
  }
}
void ShowScanKeyNo() {
  digitalWrite(LATCH_PIN, LOW);   // 送資料前要先把 LATCH_PIN 拉成低電位
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B10000000);  
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[10]);  
  digitalWrite(LATCH_PIN, HIGH);  // 送完資料後要把 LATCH_PIN 拉回成高電位
  digitalWrite(LATCH_PIN, LOW);   // 送資料前要先把 LATCH_PIN 拉成低電位
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00010000);  
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[scanKeyNo + 1]);  
  digitalWrite(LATCH_PIN, HIGH);  // 送完資料後要把 LATCH_PIN 拉回成高電位
}
void ShowSoundPulse() {
  ShowNumber(SOUND_PULSE);
}
void ShowNumber(int value) {
  if (value < 0) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B10000000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0);  
    digitalWrite(LATCH_PIN, HIGH);
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B01000000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0);  
    digitalWrite(LATCH_PIN, HIGH);
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00100000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0);  
    digitalWrite(LATCH_PIN, HIGH);
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00010000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0);  
    digitalWrite(LATCH_PIN, HIGH);
  }
  else {
    char nv[] = "";
    int len; 
    itoa(value, nv, 10);
    len = strlen(nv);
    // 百位數
    if (len >= 3) {
      digitalWrite(LATCH_PIN, LOW);
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B01000000);  
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 3] - '0']);  
      digitalWrite(LATCH_PIN, HIGH);
    }
    // 十位數
    if (len >= 2) {
      digitalWrite(LATCH_PIN, LOW);
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00100000);  
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 2] - '0']);  
      digitalWrite(LATCH_PIN, HIGH);
    }
    // 個位數
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00010000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 1] - '0']);  
    digitalWrite(LATCH_PIN, HIGH);
  }
}
void ShowCountDown() {
  char nv[] = "";
  int len; 
  // 模式字元
  digitalWrite(LATCH_PIN, LOW);   // 送資料前要先把 LATCH_PIN 拉成低電位
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B10000000);  
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, MODEs[iMode]);  
  digitalWrite(LATCH_PIN, HIGH);  // 送完資料後要把 LATCH_PIN 拉回成高電位
  if (iMode != 3)
    // 時間
    itoa(iCountDown, nv, 10);
  else 
    itoa(iLight, nv, 10);
  len = strlen(nv);
  // 百位數
  if (len >= 3) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B01000000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 3] - '0']);  
    digitalWrite(LATCH_PIN, HIGH);
  }
  // 十位數
  if (len >= 2) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00100000);  
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 2] - '0']);  
    digitalWrite(LATCH_PIN, HIGH);
  }
  // 個位數
  digitalWrite(LATCH_PIN, LOW);
  byte show = 0;
  show = LEDs[nv[len - 1] - '0'];
  if (iMode != 3 && starting) {
    if (!bHeartbeat)
      show |= B10000000;
    else 
      show &= B01111111;
  }
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00010000);  
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, show);  
  digitalWrite(LATCH_PIN, HIGH);
}
void PlayAlert() {  
  /*
  int v = playIndex % 15;
   if (v < 12 && v % 3 <= 1)
   */
  int v = playIndex % 10;
  playIndex++;
  if (v < 7 && v % 2 <= 0)
    tone(BUZZER_PIN, SOUND_PULSE); 
  else
    noTone(BUZZER_PIN); 
}


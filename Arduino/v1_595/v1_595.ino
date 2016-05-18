#include <TimedAction.h>

// 開始鍵 Pin 腳
const int KEY_START_PIN = 13;
// 模式鍵 Pin 腳
const int KEY_MODE_PIN = 12;
// 增加鍵 Pin 腳
const int KEY_ADD_PIN = 11;
// 減少鍵 Pin 腳
const int KEY_SUB_PIN = 10;

// iMode 最大值
const int MODE_MAX = 4;

// Lab12 使用兩顆 74HC595 和三支腳位控制 16 顆 LED
// 接 74HC595 的 SH_CP (clock pin)
const int CLOCK_PIN = 7;
// 接 74HC595 的 ST_CP (latch pin)
const int LATCH_PIN = 8;
// 接 74HC595 的 DS (data pin)
const int DATA_PIN = A0;

// 蜂鳴器 Pin 腳，僅可使用 PWM
const int BUZZER_PIN = 3;
// 上層 UV LED 板 Pin 腳，僅可使用 PWM
const int UP_PANEL_PIN = 6;
// 上層 UV LED 板 Pin 腳，僅可使用 PWM
const int DN_PANEL_PIN = 5;
// 心跳 LED Pin 腳，僅可使用 PWM
const int HEARTBEAT_PIN = 9;

// 最大亮度 %
const int LIGHT_MAX = 100;
// 最小亮度 %
const int LIGHT_MIN = 5;
// 亮度增減量 %
const int LIGHT_STEP = 5;
// 曝光最長時間，單位秒
const int MAX_TIMER = 600;
// 曝光最短時間，單位秒
const int MIN_TIMER = 10;
// 曝光時間增減量，單位秒
const int TIMER_ADD_STEP = 10;
// 警告音音頻
const int SOUND_PULSE = 4435;

boolean pressStart = false;     // 是否按下開始鈕
boolean pressMode = false;      // 是否按下模式鈕
boolean pressCount = false;     // 是否按下數字增減鈕
boolean starting = false;       // 是否開始曝光
boolean bCountDown = false;     // 是否正在倒數中
boolean playAlert = false;      // 是否播放警示音中
int iCountDown = 90;            // 預設開始秒數
int iStartDown = 0;             // 開始倒數的秒數
int iMode = 2;                  // 曝光版模式；0上層、1下層、2上下層、3手動控制
int iLight = 80;                // 亮度百分比
int playIndex = 0;              // 正在播放的聲音序號
byte bManually = B00000011;     // 手動控制光盤模式，以B00000010表示亮上盤，B00000001亮下盤

const byte LEDs[11] = {
  B00111111,  // 0, pgfedcba
  B00000110,  // 1        a
  B01011011,  // 2      -----
  B01001111,  // 3   f |     | b
  B01100110,  // 4     |  g  |
  B01101101,  // 5      -----
  B01111101,  // 6   e |     | c
  B00100111,  // 7     |  d  |
  B01111111,  // 8      -----  . p
  B01101111,  // 9
  B01110011   // P
};
const byte MODEs[5] = {
  B00000001,   // 上標線, 亮上光盤
  B00001000,   // 下標線, 亮下光盤
  B00001001,   // 上下標線, 亮上下兩光盤
  B00111000,   // 顯示L, 亮度設定模式
  B01001001    // 上下標線加減號(-), 常亮(手控)模式
};

TimedAction taHeartbeat = TimedAction(25, Heartbeat);
TimedAction taCountDown = TimedAction(500, CountDown);
TimedAction taCheckButton = TimedAction(200, CheckButtons);
TimedAction taPlayAlert = TimedAction(100, PlayAlert);
TimedAction taShowCountDown = TimedAction(1, ShowCountDown);

//unsigned long time;

// 程式進入點
void setup() {
  Serial.begin(9600);
  // 將 LATCH_PIN, CLOCK_PIN, DATA_PIN 設置為輸出
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(UP_PANEL_PIN, OUTPUT);
  pinMode(DN_PANEL_PIN, OUTPUT);
  pinMode(HEARTBEAT_PIN, OUTPUT);
  pinMode(KEY_START_PIN, INPUT);
  pinMode(KEY_MODE_PIN, INPUT);
  pinMode(KEY_ADD_PIN, INPUT);
  pinMode(KEY_SUB_PIN, INPUT);

  digitalWrite(UP_PANEL_PIN, LOW);
  digitalWrite(DN_PANEL_PIN, LOW);
  digitalWrite(KEY_START_PIN, LOW);
  digitalWrite(KEY_MODE_PIN, LOW);
  digitalWrite(KEY_ADD_PIN, LOW);
  digitalWrite(KEY_SUB_PIN, LOW);
  taCountDown.disable();
  taPlayAlert.disable();
  //time = millis();
}

// 主迴圈
void loop() {
  taHeartbeat.check();
  taCheckButton.check();
  taCountDown.check();
  taShowCountDown.check();
  taPlayAlert.check();
}

uint8_t hbval = 128;  // LED亮度
int8_t hbdelta = 10;  // 每次增減的值
// 顯示呼吸燈
void Heartbeat() {
  if (hbval <= 20 || hbval >= 240) hbdelta = -hbdelta;
  hbval += hbdelta;
  analogWrite(HEARTBEAT_PIN, hbval);
}

// 倒數計時執行緒
void CountDown() {
  if (starting) {
    if (iCountDown > 0) {
      bCountDown = !bCountDown;
      if (!bCountDown)
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
      bCountDown = false;
    }
  }
}

// 檢查按鍵狀態
void CheckButtons() {
  int isPress = HIGH;
  int v = 0;
  if (iMode <= 2) { // 開始鍵
    isPress = digitalRead(KEY_START_PIN);
    if (!pressStart && isPress == LOW) {  // 按下開始鍵並放開
      Serial.print(millis());
      Serial.print(" : Press Start, Mode=");
      Serial.println(iMode);
      if (iCountDown > 0) {   // 倒數器大於零，即正在倒數中
        starting = !starting;
        iStartDown = iCountDown;
        if (starting) {   // 開始倒數
          taCountDown.enable();
          byte bLight = map(iLight, 0, 100, 0, 255);
          switch (iMode) {  // 控制光盤顯示與否
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
        else {            // 停止或未開始倒數
          taCountDown.disable();
          digitalWrite(UP_PANEL_PIN, LOW);
          digitalWrite(DN_PANEL_PIN, LOW);
        }
      }
      if (playAlert) {
        // 放警示音
        taPlayAlert.disable();
        playAlert = false;
        playIndex = 0;
        noTone(BUZZER_PIN);
        iCountDown = iStartDown;
      }
      pressStart = true;
      return;
    }
    if (pressStart && isPress != LOW) {   // 放開開始鍵時
      pressStart = false;
    }
  }
  if (!starting && !playAlert) {  // 待機中
    isPress = digitalRead(KEY_MODE_PIN);
    if (!pressMode && isPress == LOW) {       // 按下模式鍵
      Serial.print(millis());
      Serial.print(" : Press Mode, Original Mode=");
      Serial.print(iMode);
      iMode++;
      if (iMode >= MODE_MAX)
        iMode = 0;
      Serial.print(", New Mode=");
      Serial.println(iMode);
      pressMode = true;
      return;
    }
    if (pressMode && isPress != LOW) {        // 放開模式鍵時
      pressMode = false;
    }
    int pressAdd = digitalRead(KEY_ADD_PIN);
    int pressSub = digitalRead(KEY_SUB_PIN);
    if (pressAdd == LOW || pressSub == LOW) { // 按下增加鍵或減少鍵
      Serial.print(millis());
      if (iMode <= 2) {       // 亮盤秒數控制模式
        if (pressSub == LOW) {
          Serial.print(" : Press Sub, Mode=");
          iCountDown -= TIMER_ADD_STEP;
        } else {
          Serial.print(" : Press Add, Mode=");
          iCountDown += TIMER_ADD_STEP;
        }
        Serial.print(iMode);
        Serial.print(", New Value=");
        Serial.println(iCountDown);

        if (iCountDown > MAX_TIMER)
          iCountDown = MIN_TIMER;
        else if (iCountDown <= 0)
          iCountDown = MAX_TIMER;
      }
      else if (iMode == 3) {  // 亮度控制模式
        if (pressSub == LOW) {
          Serial.print(" : Press Sub, Mode=");
          iLight -= LIGHT_STEP;
        } else {
          Serial.print(" : Press Add, Mode=");
          iLight += LIGHT_STEP;
        }
        Serial.println(iMode);
        if (iLight > LIGHT_MAX)
          iLight = LIGHT_MIN;
        else if (iLight <= 0)
          iLight = LIGHT_MAX;
      }
      else if (iMode == 4) {  // 手動模式
        Serial.print(" : Orignal ManuallyMode=");
        Serial.print(bManually, BIN);
        if (pressSub == LOW) {
          Serial.print(", Press Sub, ManuallyMode=");
          if (bitRead(bManually, 0) == 1)
            bitClear(bManually, 0);
          else
            bitSet(bManually, 0);
        } else {
          Serial.print(", Press Add, ManuallyMode=");
          if (bitRead(bManually, 1) == 1)
            bitClear(bManually, 1);
          else
            bitSet(bManually, 1);
        }
        Serial.println(bManually, BIN);
      }
      pressCount = true;
    }
    if (pressCount && pressAdd != LOW && pressSub != LOW) { // 放開秒數/亮度鍵時 + 鍵
      pressCount = false;
    }
  }
}

// 顯示LED字幕
void ShowCountDown() {
  char nv[] = "";
  int len;
  // 模式字元
  digitalWrite(LATCH_PIN, LOW);   // 送資料前要先把 LATCH_PIN 拉成低電位
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B10000000);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, MODEs[iMode]);
  digitalWrite(LATCH_PIN, HIGH);  // 送完資料後要把 LATCH_PIN 拉回成高電位
  switch (iMode) {
    case 0: // 0..2 取得設定秒數的字元資料
    case 1:
    case 2:
      sprintf(nv, "%d", iCountDown);
      break;
    case 3: // 3 取得光盤亮度的測定值字元資料
      sprintf(nv, "%d", iLight);
      break;
    case 4:
      {
        byte show = 0;
        if (bitRead(bManually, 0) == 1)
          bitSet(show, 3);
        if (bitRead(bManually, 1) == 1)
          bitSet(show, 0);
        // 十位數
        digitalWrite(LATCH_PIN, LOW);
        shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00100000);
        shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, show);
        digitalWrite(LATCH_PIN, HIGH);
        // 個位數
        digitalWrite(LATCH_PIN, LOW);
        shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00010000);
        shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, show);
        digitalWrite(LATCH_PIN, HIGH);
        break;
      }
    default:
      break;
  }

  len = strlen(nv);
  if (len >= 3) {   // 顯示百位數
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B01000000);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 3] - '0']);
    digitalWrite(LATCH_PIN, HIGH);
  }
  if (len >= 2) {   // 顯示十位數
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00100000);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, LEDs[nv[len - 2] - '0']);
    digitalWrite(LATCH_PIN, HIGH);
  }
  if (len >= 1) {   // 個位數
    byte show = 0;
    show = LEDs[nv[len - 1] - '0'];
    if (iMode <= 2 && starting) {
      if (!bCountDown)
        // show |= B10000000;
        bitSet(show, 7);
      else
        //show &= B01111111;
        bitClear(show, 7);
    }
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00010000);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, show);
    digitalWrite(LATCH_PIN, HIGH);
  }
}

// 播放警示音
void PlayAlert() {
  playIndex++;
  playIndex = playIndex % 10;
  if (playIndex < 7 && playIndex % 2 <= 0)
    tone(BUZZER_PIN, SOUND_PULSE);
  else
    noTone(BUZZER_PIN);
}
void StartupShow() {

}


/*********************************************
 ** UV 曝光箱控制板 v1.00
 ** 功能概述：
 ** 1. 使用 I2C 溝通界面 Address 為 0x3F(PCF8574A) 
 **    與 HD44780 相容之 LCD1602 作為顯示輸出
 ** 2. 曝光時間最長 900 秒最短 10 秒
 ** 3. 亮度控制最大亮度 100% 最小亮度 5%
 ** 4. 可個別控制上下 UV 亮盤
 ** 5. 使用四個按鍵控制，按鍵各為：開始、模式、+/啟用、-/禁用
 ** 6. 自動模式下，時間終止會播放警示音、閃爍LCD背光以及關閉 UV 亮盤
 ** 7. 手動模式下，使用「開始」鍵開啟與關閉 UV 上下亮盤
 ** 8. 自動記錄前次使用之設定值，避免 Reset 或斷電時需重新設定
 *********************************************/
// ArduinoThread Library
// https://github.com/ivanseidel/ArduinoThread
#include <Thread.h>
#include <ThreadController.h>
// Arduino EEPROMex library
// GitHub : https://github.com/thijse/Arduino-EEPROMEx 
// Anthor Blog : http://thijs.elenbaas.net/2012/07/extended-eeprom-library-for-arduino
#include <EEPROMex.h>
#include <EEPROMVar.h>
// IIC LCD Library
// https://github.com/marcoschwartz/LiquidCrystal_I2C
#include <LiquidCrystal_I2C.h>

// uint8_t = byte = 8bit
#define VERSION  "v1.00"

// 開始鍵 Pin 腳
const uint8_t KEY_START_PIN = A0;
// 模式鍵 Pin 腳
const uint8_t KEY_MODE_PIN = A1;
// 增加鍵 Pin 腳
const uint8_t KEY_ADD_PIN = A2;
// 減少鍵 Pin 腳
const uint8_t KEY_SUB_PIN = A3;
// 蜂鳴器 Pin 腳，僅可使用 PWM
const uint8_t BUZZER_PIN = 3;
// 上層 UV LED 板 Pin 腳，僅可使用 PWM
const uint8_t UP_PANEL_PIN = 5;
// 上層 UV LED 板 Pin 腳，僅可使用 PWM
const uint8_t DN_PANEL_PIN = 6;
// Heartbeat Pin 腳(呼吸燈)
const uint8_t HEARTBEAT_PIN = 9;

// iMode 最大值
const uint8_t MODE_MAX = 4;
// 最大亮度 %
const uint8_t LIGHT_MAX = 100;
// 最小亮度 %
const uint8_t LIGHT_MIN = 5;
// 亮度增減量 %
const uint8_t LIGHT_STEP = 5;
// 曝光最長時間，單位秒
const uint16_t MAX_TIMER = 900;
// 曝光最短時間，單位秒
const uint8_t MIN_TIMER = 10;
// 曝光時間增減量，單位秒
const uint8_t TIMER_ADD_STEP = 10;
// 警告音音頻
const int SOUND_PULSE = 4435;
// EEPROM 開始記錄位置
const int MEM_BASE = 200; 
// EEPROM 最大紀錄長度，單位 Bytes
const int MAX_ALLOW_WRITES = 16;

boolean pressStart = false;     // 是否按下開始鈕
boolean pressMode = false;      // 是否按下模式鈕
boolean pressCount = false;     // 是否按下數字增減鈕
boolean starting = false;       // 是否開始曝光
boolean bCountDown = false;     // 是否正在倒數中
boolean bPlayAlarm = false;     // 是否播放警示音中
uint16_t iCountDown = 120;      // 倒數計數器
uint16_t iStartDown = 0;        // 紀錄開始倒數的秒數
uint8_t iMode = 0;              // 箭頭模式：0時間、1亮度、2上層、3下層、4手動模式
uint8_t iPanel = 3;             // UV燈板顯示模式；0-全不亮、1-上層、2-下層、3-上下層
uint8_t iLight = 80;            // 亮度百分比
uint8_t playIndex = 0;          // 正在播放的聲音順序序號
uint8_t hbval = 128;            // LED 呼吸燈亮度
uint8_t hbdelta = 10;           // 呼吸燈每次增減的值

// EEPROM 讀取值
uint16_t romCountDown = 0;
uint8_t romLight = 0;
uint8_t romPanel = 0;

LiquidCrystal_I2C lcd(0x3F, 16, 2);   // For PCF8574A

// ThreadController that will controll all threads
ThreadController controll = ThreadController();

// Heartbeat Thread
Thread atHeartbeat = Thread();
// Check Button Thread
Thread atCheckButton = Thread();
// Count Down Thread
Thread atCountDown = Thread();
// Alarm Sound Thread
Thread atPlayAlarm = Thread();

// 程式進入點
void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // 等待 Serial Port 備妥
  }
  SetPinDefine();
  EEPROM.setMemPool(MEM_BASE, EEPROMSizeATmega328);
  EEPROM.setMaxAllowedWrites(MAX_ALLOW_WRITES);
  ReadSettings();
  SetThreads();
  InitializationLCD();
  tone(BUZZER_PIN, SOUND_PULSE);
  delay(100);
  noTone(BUZZER_PIN);
}

// 主迴圈
void loop() {
  controll.run();
}

// 設定 Pin 腳位定義與預設輸出
void SetPinDefine() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(UP_PANEL_PIN, OUTPUT);
  pinMode(DN_PANEL_PIN, OUTPUT);
  pinMode(HEARTBEAT_PIN, OUTPUT);
  pinMode(KEY_START_PIN, INPUT);
  pinMode(KEY_MODE_PIN, INPUT);
  pinMode(KEY_ADD_PIN, INPUT);
  pinMode(KEY_SUB_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(UP_PANEL_PIN, LOW);
  digitalWrite(DN_PANEL_PIN, LOW);
  digitalWrite(HEARTBEAT_PIN, LOW);
  digitalWrite(KEY_START_PIN, LOW);
  digitalWrite(KEY_MODE_PIN, LOW);
  digitalWrite(KEY_ADD_PIN, LOW);
  digitalWrite(KEY_SUB_PIN, LOW);
  digitalWrite(HEARTBEAT_PIN, HIGH);
}

// 設定執行緒
void SetThreads() {
  // Heartbeat Thread
  atHeartbeat.onRun(Heartbeat);
  atHeartbeat.setInterval(50);
  controll.add(&atHeartbeat);

  // Check Button Thread
  atCheckButton.onRun(CheckButtons);
  atCheckButton.setInterval(220);
  controll.add(&atCheckButton);

  // Count Down Thread
  atCountDown.onRun(CountDown);
  atCountDown.setInterval(500);
  controll.add(&atCountDown);
  
  // Alarm Sound Thread
  atPlayAlarm.onRun(PlayAlarm);
  atPlayAlarm.setInterval(100);
  controll.add(&atPlayAlarm);
}

// 顯示測試燈
void Heartbeat() {
  if (hbval <= 20 || hbval >= 240) hbdelta = -hbdelta;
  hbval += hbdelta;
  analogWrite(HEARTBEAT_PIN, hbval);
}

// 倒數計時執行緒
void CountDown() {
  if (!starting || iMode == 4)
    return;
  if (iCountDown > 0) {
    bCountDown = !bCountDown;
    if (!bCountDown) {
      iCountDown--;
      if (iMode != 4)
        ShowTime();
    }
  } else {
    digitalWrite(UP_PANEL_PIN, LOW);
    digitalWrite(DN_PANEL_PIN, LOW);
    starting = false;
    playIndex = 0;
    bPlayAlarm = true;
    bCountDown = false;
  }
}

// 播放警示音
void PlayAlarm() {
  if (!bPlayAlarm)
    return;
  if (playIndex < 7 && playIndex % 2 == 0) {
    tone(BUZZER_PIN, SOUND_PULSE);
    lcd.backlight();
  } else {
    noTone(BUZZER_PIN);
    lcd.noBacklight();
  }
  playIndex++;
  playIndex = playIndex % 10;
}

// 檢查按鍵狀態
void CheckButtons() {
  int isPress = HIGH;
  int v = 0;
  isPress = digitalRead(KEY_START_PIN);
  if (!pressStart && isPress == LOW) {  // 按下開始鍵並放開
    if (bPlayAlarm) {
      // 放警示音中
      bPlayAlarm = false;
      playIndex = 0;
      noTone(BUZZER_PIN);
      iCountDown = iStartDown;
      lcd.backlight();
      ShowTime();
      return;
    }
    Serial.print(millis());
    Serial.print(F(" : Press Start, Mode="));
    Serial.println(iMode);
    starting = !starting;
    if (starting) {
      // 閒置中；開始倒數，並依光線強度以 PWM 方式輸出
      iStartDown = iCountDown;
      byte bLight = map(iLight, 0, 100, 0, 255);
      // 控制光盤顯示與否
      if (bitRead(iPanel, 0) != 0)
        analogWrite(UP_PANEL_PIN, bLight);
      else
        digitalWrite(UP_PANEL_PIN, LOW);
      if (bitRead(iPanel, 1) != 0)
        analogWrite(DN_PANEL_PIN, bLight);
      else
          digitalWrite(DN_PANEL_PIN, LOW);
    } else {
      // 倒數中，停止倒數並關閉 UV 光盤
      digitalWrite(UP_PANEL_PIN, LOW);
      digitalWrite(DN_PANEL_PIN, LOW);
    }
    if (iMode == 4) {
      // 手動開始/停止曝光
    } else {
      // 自動開始倒數
      WriteSetting();
      ShowTime();
    }
    pressStart = true;
    return;
  }
  if (pressStart && isPress != LOW) {   // 放開開始鍵時
    pressStart = false;
  }
  if (!starting && !bPlayAlarm) {  
    // 待機中
    isPress = digitalRead(KEY_MODE_PIN);
    if (!pressMode && isPress == LOW) {       // 按下模式鍵
      Serial.print(millis());
      Serial.print(F(" : Press Mode, Original Mode="));
      Serial.print(iMode);
      iMode++;
      if (iMode > MODE_MAX)
        iMode = 0;
      Serial.print(F(", New Mode="));
      Serial.println(iMode);
      pressMode = true;
      if (iMode == 0)
        ShowMenu(1);
      ShowCursor(iMode);
      return;
    }
    if (pressMode && isPress != LOW) {        
      // 放開模式鍵時
      pressMode = false;
    }
    int pressAdd = digitalRead(KEY_ADD_PIN);
    int pressSub = digitalRead(KEY_SUB_PIN);
    if (pressAdd == LOW || pressSub == LOW) { 
      // 按下增加鍵或減少鍵
      Serial.print(millis());
      switch (iMode) {
        case 0:   // 亮盤秒數控制模式
          if (pressSub == LOW) {
            Serial.print(F(" : Press [-] Key, Mode="));
            iCountDown -= TIMER_ADD_STEP;
          } else {
            Serial.print(F(" : Press [+] Key, Mode="));
            iCountDown += TIMER_ADD_STEP;
          }
          Serial.print(iMode);
          Serial.print(F(", New Value="));
          Serial.println(iCountDown);

          if (iCountDown > MAX_TIMER)
            iCountDown = MAX_TIMER;
          else if (iCountDown < MIN_TIMER)
            iCountDown = MIN_TIMER;
          ShowTime();
          break;
        case 1:   // 亮度控制模式
          if (pressSub == LOW) {
            Serial.print(F(" : Press [-] Key, Mode="));
            iLight -= LIGHT_STEP;
          } else {
            Serial.print(F(" : Press [+] Key, Mode="));
            iLight += LIGHT_STEP;
          }
          Serial.println(iMode);
          if (iLight > LIGHT_MAX)
            iLight = LIGHT_MAX;
          else if (iLight < LIGHT_MIN)
            iLight = LIGHT_MIN;
          ShowLight();
          break;
        case 2:   // 上層
          if (pressSub == LOW) {
            Serial.print(F(" : Press [-] Key, Panel="));
            iPanel &= 0xFE;
          } else {
            Serial.print(F(" : Press [+] Key, Panel="));
            iPanel |= 0x01;
          }
          Serial.println(iPanel);
          ShowLedIcons(15);
          break;
        case 3:   // 下層
          if (pressSub == LOW) {
            Serial.print(F(" : Press [-] Key, Panel="));
            iPanel &= 0xFD;
          } else {
            Serial.print(F(" : Press [+] Key, Panel="));
            iPanel |= 0x02;
          }
          Serial.println(iPanel);
          ShowLedIcons(15);
          break;
        case 4:   // 手動模式
          break;
        default:  // 其他未定義
          break;
      }
      pressCount = true;
    }
    if (pressCount && pressAdd != LOW && pressSub != LOW) { 
      // 放開秒數/亮度鍵時 + 鍵
      pressCount = false;
    }
  }
}

// 初始化LCD
void InitializationLCD() {
  lcd.init();
  lcd.noBacklight();
  delay(100);
  lcd.clear();
  delay(100);
  for (uint8_t i = 0; i < 3; i++) {
    lcd.backlight();
    delay(100);
    lcd.noBacklight();
    delay(100);
  }
  lcd.backlight(); // finish with backlight on
  lcd.setCursor(0, 0); //Start at character 4 on line 0
  lcd.print(F("Hello, Sir!"));
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print(F("Have a Nice Day!"));
  delay(1000);
  SetCustomChar();
  for(int i = 0; i <16; i++) {
    lcd.scrollDisplayLeft();
    delay(100);
  }
  lcd.clear();
  lcd.home();
  lcd.print(F("UV Box - "));
  lcd.print(VERSION);
  delay(2000);
  ShowMenu(0);
}

// 顯示主畫面
void ShowMenu(byte menu) {
  String nv = "";
  switch (menu) {
    case 0:
      lcd.clear();
      ShowCursor(iMode);
    case 1:
      ShowTime();
    case 2:
      ShowLight();
      ShowLedIcons(15);
      break;
    default:
      return;
  }
}

// 顯示倒數時間
void ShowTime() {
  if (iMode == 4)
    return;
  lcd.setCursor(1, 0); //Start at character 4 on line 0
  lcd.print(F("Time :"));
  lcd.print(RightPad(3, iCountDown));
  lcd.print(F("s  "));
}

// 顯示光盤亮度值
void ShowLight() {
  lcd.setCursor(1, 1);
  lcd.print(F("Light:"));
  lcd.print(RightPad(3, iLight));
  lcd.print(F("%  "));
}

// 顯示選單游標
void ShowCursor(byte mode) {
  lcd.setCursor(0, 0);
  lcd.print(F(" "));
  lcd.setCursor(0, 1);
  lcd.print(F(" "));
  lcd.setCursor(14, 0);
  lcd.print(F(" "));
  lcd.setCursor(14, 1);
  lcd.print(F(" "));
  switch (mode) {
    case 0:   // 時間
      lcd.setCursor(0, 0);
      lcd.write(byte(7));
      break;
    case 1:   // 亮度
      lcd.setCursor(0, 1);
      lcd.write(byte(7));
      break;
    case 2:   // 上層
      lcd.setCursor(14, 0);
      lcd.write(byte(7));
      break;
    case 3:   // 下層
      lcd.setCursor(14, 1);
      lcd.write(byte(7));
      break;
    case 4:   // 手動模式
      lcd.setCursor(1, 0); //Start at character 4 on line 0
      lcd.print(F("Time :---"));
      lcd.print(F("s  "));
      break;
    default:
      break;
  }
}

// 將數字轉文字且將之靠右並左補空白
String RightPad(uint8_t len, int val) {
  char nv[4];
  itoa (val, nv, 10);
  String nv1 = String(nv);
  //unsigned int slen = nv1.length();
  uint8_t slen = nv1.length();
  for (uint8_t i = slen + 1; i < 4; i++)
    nv1 = ' ' + nv1;
  return nv1;
}

// 顯示光盤表示明滅的圖示
void ShowLedIcons(uint8_t x) {
  lcd.setCursor(x, 0); //Start at character 4 on line 0
  if (bitRead(iPanel, 0) != 0)
    lcd.write(byte(0));
  else
    lcd.write(byte(1));
  lcd.setCursor(x, 1);
  if (bitRead(iPanel, 1) != 0)
    lcd.write(byte(2));
  else
    lcd.write(byte(3));
}

// 自 EEPROM 中讀取記錄值
void ReadSettings() {
  int sizeByte = sizeof(byte);
  int sizeInt = sizeof(int);
  romCountDown = EEPROM.readInt(MEM_BASE);
  romLight = EEPROM.readByte(MEM_BASE + sizeInt);
  romPanel = EEPROM.readByte(MEM_BASE + sizeInt + sizeByte);
  if (romCountDown > 0)
    iCountDown = romCountDown;
  if (romLight > 0)
    iLight = romLight;
  if (romPanel > 0)
    iPanel = romPanel;  
}

// 儲存記錄值至 EEPROM 中
void WriteSetting() {
  int sizeByte = sizeof(byte);
  int sizeInt = sizeof(int);
  if (romCountDown != iStartDown) {
    romCountDown = iStartDown;
    EEPROM.writeInt(MEM_BASE, romCountDown);
  }
  if (romLight != iLight) {
    romLight = iLight;
    EEPROM.writeByte(MEM_BASE + sizeInt, romLight);
  }
  if (romPanel != iPanel) {
    romPanel = iPanel;
    EEPROM.writeByte(MEM_BASE + sizeInt + sizeByte, romPanel);
  } 
}

// 設定自訂圖示
void SetCustomChar() {
  byte upLightOn[8] = {
    B00000,
    B11111,
    B11111,
    B01110,
    B00000,
    B10101,
    B10101
  };
  byte upLightOff[8] = {
    B00000,
    B11111,
    B10001,
    B01110,
    B00000,
    B00000,
    B00000
  };
  byte dnLightOn[8] = {
    B00000,
    B10101,
    B10101,
    B00000,
    B01110,
    B11111,
    B11111
  };
  byte dnLightOff[8] = {
    B00000,
    B00000,
    B00000,
    B00000,
    B01110,
    B10001,
    B11111
  };
  byte arrow[8] = {
    B00000,
    B11100,
    B01110,
    B00111,
    B01110,
    B11100,
    B00000
  };
  byte none[8] = { };
  lcd.load_custom_character(0, upLightOn);
  lcd.load_custom_character(1, upLightOff);
  lcd.load_custom_character(2, dnLightOn);
  lcd.load_custom_character(3, dnLightOff);
  lcd.load_custom_character(7, arrow);
}

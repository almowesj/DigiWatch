
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RTClib.h>
#include <string>

// SH1106 128x64 I2C OLED with hardware I2C, no reset pin needed
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); 


// DS3231 object (the DS3231 controller that calls the functions on the chip)
RTC_DS3231 rtc;   

//BUTTONS
const int B_select = 26;  //GPIO 26   (SELECT)
const int B_next  = 14; //GPIO 14  (NEXT)
const int  B_exit = 27; //GPIO 27   (EXIT)

const int buzzer = 12;   //GPIO 12
const int buzzer_channel = 0;

//TIME AND DATE DISPLAY ARRAYS FOR HOMESCREEN
char timeStr_HM[6];
char timeStr_S[3];  
char dateStr[7];
char merediumStr[3];


//MENU ICON DATA
                              //alarm, timer, stopwatch, flash, dino, settings
const uint16_t iconGlyphs[] = {0x014B, 0x01F9, 0x01E9, 0x018E, 0x006D, 0x015B};
const uint8_t iconX[] = {10 , 55, 100, 145};  // fixed X positions for each icon (145 for the offscreen icon for smooth animation)
const int iconY = 48; // fixed Y position for each icon (same for all)
const int iconNumber = sizeof(iconGlyphs) / sizeof(iconGlyphs[0]); // # of icons 

int selectedIndexIcon = 1;  //index of current selected icon (counts up to indicate icon change)
int firstVisibleIndex = 0;  //index of first icon being shown on the display 
int boxedIcon = 1;  // always middle icon!

//SETTINGS MENU ICON DATA      //time_set, brightness, sound, temp
const uint16_t settingGlyphs[] = {0x0154, 0x0292, 0x01E4, 0x02BC};  
const int setIconNumber = sizeof(settingGlyphs) / sizeof(settingGlyphs[0]);

int selectedSettingIcon = 1;
int firstVisibleSettingIndex = 0;
int boxedSettingIcon = 1;


//SCROLLING ANIMATION
bool menuScrolling = false;
int scrollOffset = 0; //current offset (in pixels)
int scrollDist = 45;  //desired distance for icon to travel (from 10 to 55 --> x-positions)
int scrollSpeed = 12;  //12 pixels/frame

//TIMER VARIABLES
char timerStr[6];  

bool timerRunning = false;
bool timerScreenOpened = false;
bool timerScreenJustOpened = false;

int timerMin = 0;
int timerSec = 0;
int prevMilli = 0;

bool beeping = false;
int beepCount = 0;
unsigned long lastBeepTime = 0;



const int setTimes[] = {0, 1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60}; // in minutes
const int timerTimes =  sizeof(setTimes) / sizeof(setTimes[0]);           //times available within the timer
int timerIndex = 0;

//STOP WATCH VARIABLES
char stopStr[6];
char milliStr[3];
char stopHourStr[3];

bool stopRunning = false;
bool stopScreenOpened = false;
bool stopScreenJustOpened = false;

int stopMin = 0;
int stopSec = 0;
int stopMilli = 0;
int stopHour = 0;
int prevStopMillis = 0;

//FLASH VARIABLES
int defaultContrast = 255;

bool flashScreenOpen = false;
bool flashScreenExit = false;
int flashContrast = 255;
const int contrasts[] = {0, 225};
int contrastIndex = 0;

//SETTING VARIABLES
char tempStr[8];



//SCREEN MODE (tabs)
enum ScreenMode {
  MODE_HOME,
  MODE_MENU,
  MODE_STOPWATCH,
  MODE_TIMER,
  MODE_ALARM,
  MODE_SETTINGS,
  MODE_FLASH,
  MODE_DINO,
};

enum TimerMode {
  TIMER_SETUP,
  TIMER_PAUSE,
  TIMER_RUN,
  TIMER_OVER
};

enum StopMode{
  STOP_START,
  STOP_RUN,
  STOP_PAUSE,
};


enum SettingsMode{
  MODE_TIME,
  MODE_BRIGHTNESS,
  MODE_SOUND,
  MODE_TEMP,
  MODE_SCREEN
};

ScreenMode currentMode = MODE_SETTINGS;      //change for each screen
TimerMode timerMode = TIMER_SETUP;
StopMode stopMode = STOP_START;
SettingsMode setMode = MODE_SCREEN;


bool BPress(int button);

void beep();

void formatTime();
void dateDisp();

void timerMinIncrement();
void timerRun();
void timerPause();
void timerOver();

void formatStop();
void stopWatchRun();


void drawHomeScreen();
void drawMenuScreen();
void drawTimerScreen();
void drawStopWatchScreen();
void drawFlashScreen();
void drawAlarmScreen();
void drawSettingsScreen();
void drawSetScreen();
void drawBrightnessScreen();
void drawSoundScreen();
void drawTempScreen();

void scrollAutomate();


void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);   //starts I2C bus, SDA --> GPIO 21 and SCL --> GPI0 22
  u8g2.begin();
  rtc.begin();

  delay(1000);
  //rtc.adjust(DateTime(__DATE__, __TIME__));
  DateTime now = rtc.now();

  //button intillization
  pinMode(B_select, INPUT_PULLUP);
  pinMode(B_next, INPUT_PULLUP);
  pinMode(B_exit, INPUT_PULLUP);

  //buzzer setup
  ledcSetup(buzzer_channel, 4000, 8); //channel = 0, freq = 4kHz, res = 8 bits
  ledcAttachPin(buzzer, buzzer_channel);
}

void loop() {

  //Time display on OLED
  formatTime();
  dateDisp();

  formatStop();

  if(flashScreenExit){
    u8g2.setContrast(defaultContrast);
    flashScreenOpen = false;
    flashScreenExit = false;
  }


  if(BPress(B_next)){

    if(currentMode == MODE_HOME){
      currentMode = MODE_MENU;
    }

    else if(currentMode == MODE_MENU || currentMode == MODE_SETTINGS){
      menuScrolling = true;
    }

    else if(currentMode == MODE_TIMER){

      if(timerMode == TIMER_SETUP){
        timerMinIncrement();
      }
    }
  }



  if(BPress(B_exit)){
    if(currentMode == MODE_MENU){
      currentMode = MODE_HOME;
    }
    else if(currentMode == MODE_TIMER && timerMode == TIMER_PAUSE){
    timerMode = TIMER_SETUP;
    timerMin = setTimes[0]; timerSec = 0; timerIndex = 0; timerScreenOpened = true;
    }
    else if(currentMode == MODE_STOPWATCH && stopMode == STOP_PAUSE){
      stopMode = STOP_START;
      stopMin = 0; stopSec = 0; stopMilli = 0; stopHour = 0;
    }
    else if(currentMode == MODE_FLASH){
      currentMode = MODE_MENU;
      flashScreenExit = true;
    }
    else if(currentMode != MODE_MENU && currentMode != MODE_HOME){
      currentMode =  MODE_MENU;
    }
  }

  if(BPress(B_select)){
      
    if(currentMode == MODE_MENU){
      
      switch(selectedIndexIcon){

        case 0: currentMode = MODE_ALARM;
        break;

        case 1: currentMode = MODE_TIMER; timerScreenJustOpened = true;
        break;

        case 2: currentMode = MODE_STOPWATCH; stopScreenJustOpened = true;
        break;

        case 3: currentMode  = MODE_FLASH; flashScreenOpen = true; contrastIndex = 0;
        break;

        case 4: currentMode = MODE_DINO;
        break;

        case 5: currentMode = MODE_SETTINGS; 
        break;
      }
    }
          

    if(currentMode == MODE_TIMER){

      if(timerScreenJustOpened){
        timerScreenJustOpened = false;
      }

      else if(timerMode == TIMER_SETUP){
        if(timerMin > 0 || timerSec > 0){
          timerMode = TIMER_RUN;
        }
      }

      else if(timerMode == TIMER_RUN){
        timerMode = TIMER_PAUSE;
      }

      else if(timerMode == TIMER_PAUSE){
        timerMode = TIMER_RUN;
      }
    }



    if(currentMode == MODE_STOPWATCH){

      if(stopScreenJustOpened){
        stopScreenJustOpened = false;
      }

      else if(stopMode == STOP_START){
        stopMode = STOP_RUN;
      }

      else if(stopMode == STOP_RUN){
        stopMode = STOP_PAUSE;
      }

      else if(stopMode == STOP_PAUSE){
        stopMode = STOP_RUN;
      }
    } 

    
    if(currentMode == MODE_FLASH){
      
      if(flashScreenOpen){
        flashContrast = contrasts[contrastIndex];
        contrastIndex = (contrastIndex + 1) % 2;

        u8g2.setContrast(flashContrast);
      }

    }


    if(currentMode == MODE_SETTINGS){

      switch(selectedSettingIcon){

        case 0: setMode = MODE_TIME;
        break;

        case 1: setMode = MODE_BRIGHTNESS;
        break;

        case 2: setMode = MODE_SOUND;
        break;

        case 3: setMode = MODE_TEMP;
        Serial.println("SETMODE = TEMP");
        break;

        case 4: setMode = MODE_SCREEN;
        break;
      }   
    }
  }


  scrollAutomate();
    

  switch(currentMode){

    case MODE_HOME:
      drawHomeScreen();
      break;

    case MODE_MENU:
      drawMenuScreen();
      break;

    case MODE_SETTINGS:
      drawSettingsScreen();
    break;

    case MODE_FLASH:
      drawFlashScreen();
      break;
    
    case MODE_TIMER:
      drawTimerScreen();
      break;

    case MODE_STOPWATCH:
      drawStopWatchScreen();
    break;

    case MODE_ALARM:
      drawAlarmScreen();
    break;

  }  


  switch(timerMode){

    case TIMER_SETUP:
      break;

    case TIMER_RUN:
      timerRunning = true;
      timerRun();

      break;

    case TIMER_PAUSE:
      timerRunning = false;
      break;

    case TIMER_OVER:
      timerOver();
      break;

  }

  switch(stopMode){

    case STOP_START:
      break;

    case STOP_RUN:
      stopRunning = true;
      stopWatchRun();
      break;

    case STOP_PAUSE:
      stopRunning = false;
      break;

  }

  switch(setMode){

    case MODE_TIME:
      break;

    case MODE_BRIGHTNESS:
      break;

    case MODE_SOUND:
      break;

    case MODE_TEMP:
      drawTempScreen();
      Serial.println("DRAW TEMO SCREEN");
      break;
  }


}

//**********************FUNCTIONS**********************\\

void drawHomeScreen() {
  
  u8g2.clearBuffer();

  //date and SS display on homescreen
  u8g2.setFont(u8g2_font_sirclivethebold_tr); 
  u8g2.drawStr(38, 10, dateStr);    //date
  u8g2.drawStr(100 ,45, timeStr_S); //seconds
  u8g2.drawStr(100, 30, merediumStr);

  //HH:MM display on homescreen
  u8g2.setFont(u8g2_font_freedoomr25_tn); 
  u8g2.drawStr(8,48, timeStr_HM); //hours and minutes


  //battery icon (might change into actual icons from font)
  u8g2.drawFrame(5, 56, 13, 7);       //outline
  u8g2.drawBox(6, 57, 9, 5);         //fill
  u8g2.drawBox(20, 58, 1, 3);        //tip

  u8g2.sendBuffer();
}

void drawMenuScreen(){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_sirclivethebold_tr); 
  u8g2.drawStr(42 , 10, "MENU");

  u8g2.setFont(u8g2_font_streamline_all_t);

 for (int i = 0; i < 4; i++) {
      int iconIndex = (firstVisibleIndex + i) % iconNumber;
      u8g2.drawGlyph(iconX[i] - scrollOffset, iconY, iconGlyphs[iconIndex]);
  }

  u8g2.drawFrame(iconX[1] - 4, 23, 28, 28);   //selection box



  u8g2.sendBuffer();
}

void drawSettingsScreen(){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_sirclivethebold_tr); 
  u8g2.drawStr(27 , 10, "SETTINGS");

  u8g2.setFont(u8g2_font_streamline_all_t);

 for (int i = 0; i < 4; i++) {
      int iconIndex = (firstVisibleSettingIndex + i) % setIconNumber;
      u8g2.drawGlyph(iconX[i] - scrollOffset, iconY, settingGlyphs[iconIndex]);
  }

  u8g2.drawFrame(iconX[1] - 4, 23, 28, 28);   //selection box

  u8g2.sendBuffer();
}

void drawTimerScreen(){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_sirclivethebold_tr); 
  u8g2.drawStr(40, 10, "TIMER");

  u8g2.setFont(u8g2_font_freedoomr25_tn); 
  u8g2.drawStr(20,52, timerStr); //hours and minutes

  u8g2.sendBuffer();
}


void drawFlashScreen(){
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.sendBuffer();
}

void drawStopWatchScreen(){
   u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_sirclivethebold_tr); 
  u8g2.drawStr(18, 12, "STOPWATCH");

  u8g2.drawStr(100, 36, stopHourStr);



  //temp display
  u8g2.drawStr(100 ,53, milliStr); //milli_seconds

  u8g2.setFont(u8g2_font_freedoomr25_tn); 
  u8g2.drawStr(10 ,55, stopStr); //minutes & seconds


  u8g2.sendBuffer();

}

void drawAlarmScreen(){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_sirclivethebold_tr); 
  u8g2.drawStr(35, 10, "ALARM");
  u8g2.drawStr(15, 62, "M T W T F S S");

  u8g2.setFont(u8g2_font_freedoomr25_tn);
  u8g2.drawStr(20, 48, "00:00");

  u8g2.sendBuffer();

}

void drawTempScreen(){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_sirclivethebold_tr);
  u8g2.drawStr(25, 10, "TEMPERATURE");

  u8g2.setFont(u8g2_font_streamline_all_t);
  u8g2.drawGlyph(iconX[0], iconY, settingGlyphs[3]);

  u8g2.setFont(u8g2_font_freedoomr25_tn);
  u8g2.drawStr(25, 48, tempStr);
}

//change name later
void formatTime(){
  DateTime now = rtc.now();

  snprintf(timeStr_HM, sizeof(timeStr_HM), "%2d:%02d", now.twelveHour(), now.minute());
  snprintf(timeStr_S, sizeof(timeStr_S), "%02d", now.second());
  snprintf(merediumStr, sizeof(merediumStr), "%s", now.isPM() ? "PM" : "AM" );

  //timer
  snprintf(timerStr, sizeof(timerStr), "%02d:%02d", timerMin, timerSec);

  //temp
  snprintf(tempStr, sizeof(tempStr), "%.2f C", rtc.getTemperature());
}

void dateDisp(){
  DateTime now = rtc.now();

  const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  snprintf(dateStr, sizeof(dateStr), "%s %02d", weekdays[now.dayOfTheWeek()], now.day());
}

void timerMinIncrement(){
  if(timerMode == TIMER_SETUP){  

    if(timerScreenOpened){
      timerIndex = 0;
      timerMin = setTimes[timerIndex];
      timerSec = 0;

      timerScreenOpened = false;
    } 
        
    timerIndex = (timerIndex + 1) % timerTimes;
    timerMin = setTimes[timerIndex];
  }
}

void timerRun(){

  if(timerRunning){

    if(millis() - prevMilli >= 1000){
      timerSec--;

      prevMilli = millis();
    }

    if(timerSec < 0){
      timerSec = 0;

      if(timerMin > 0){
        timerMin--;
        timerSec = 59;
      }

      else{
        timerMode = TIMER_OVER;    

        timerRunning = false;
      }
    }
  }
}


void timerOver() {
  for(int i = 0; i < 3; i++){
    delay(150);
    beep();
    delay(300); // brief pause between beeps
    beep();
  }

  timerMode = TIMER_SETUP;
}

void formatStop(){
  snprintf(stopStr, sizeof(stopStr), "%02d:%02d", stopMin, stopSec);
  snprintf(milliStr, sizeof(milliStr), "%02d", stopMilli);
  snprintf(stopHourStr, sizeof(stopHourStr), "%dH", stopHour);
}

void stopWatchRun(){
  if(stopRunning){
    stopMilli = ((millis() - prevStopMillis) % 1000) / 10; //values go from 0-99

    if(millis() - prevStopMillis >= 1000){
      stopMilli = 0;    //restart milliseconds the moment a full second passes
      prevStopMillis = millis();

      stopSec++;
    }

    if(stopSec >= 60){
      stopSec = 0;
      stopMin++;
    }

    if(stopMin >= 60){
      stopMin = 0;
      stopHour++;
    }
  }
}

//debouncing for buttons + indicator of button being pressed --> returns true , PULLUP --> Active Low
bool BPress(int button){
  if(digitalRead(button) == LOW){   //button is pressed
    delay(10);                      //10 ms delay

    if(digitalRead(button) == LOW){   //button is still pressed

      beep();   //buzzer beep to indicate button press

      while(digitalRead(button) == LOW);  //wait until button is released

      return true;
    }

  }
  return false;
}

void beep() {
  ledcWrite(buzzer_channel, 76);  // 33% duty cycle (76 out of 255)
  delay(50);                // beep duration
  ledcWrite(buzzer_channel, 0);    // turn off buzzer
}


//scrolling automation/animation
void scrollAutomate(){

  if(currentMode == MODE_MENU){

      if(menuScrolling == true){
        scrollOffset += scrollSpeed;  

        if(scrollOffset >= scrollDist){
          scrollOffset = 0;           //reset the scroll offset for animation
          menuScrolling = false;

          // update the selected icon (through its index) and the scroll display (first visible icon becomes the  next icon through indexing)
          selectedIndexIcon = (selectedIndexIcon + 1) % iconNumber;
          firstVisibleIndex = (firstVisibleIndex + 1) % iconNumber;
        }
      }
    }

    if(currentMode == MODE_SETTINGS){

      if(menuScrolling == true){
        scrollOffset += scrollSpeed;  

        if(scrollOffset >= scrollDist){
          scrollOffset = 0;           //reset the scroll offset for animation
          menuScrolling = false;

          // update the selected icon (through its index) and the scroll display (first visible icon becomes the  next icon through indexing)
          selectedSettingIcon = (selectedSettingIcon + 1) % setIconNumber;
          firstVisibleSettingIndex = (firstVisibleSettingIndex + 1) % setIconNumber;
        }
      }
    }

}


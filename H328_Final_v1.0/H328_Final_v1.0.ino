/*
  H328 FINAL v1.0
  ATmega328-PU DIP-28 @ 8 MHz internal RC

  Physical pins:
    4  D2  IR receiver
    5  D3  WS2812B DIN (2 LEDs)
    6  D4  buzzer
    7  VCC
    8  GND
    19 AVCC
    23 A0 battery ADC
    22 GND
    27 A4 SDA
    28 A5 SCL

  OLED 0x3C, DS3231 0x68
  IR: NEC command byte
  0=19 1=45 2=46 3=47 4=44 5=40 6=43 7=07 8=15 9=09
  OK=1C *=16 #=0D UP=18 DOWN=52 LEFT=08 RIGHT=5A

  Persistent EEPROM:
    alarm, LED color, LED brightness, OLED brightness
  DS3231:
    date/time
*/

#include <Wire.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#define IR_PIN 2
#define LED_PIN 3
#define BUZZER_PIN 4
#define BATTERY_PIN A0
#define OLED_ADDR 0x3C
#define RTC_ADDR 0x68
#define LEDS 2
#define SLEEP_MS 30000UL
#define OLED_DEFAULT 90
#define RGB_DEFAULT 22
#define EEPROM_MAGIC 0x48
#define EEPROM_VER 1

// Battery divider is assumed 300k top / 300k bottom.
// Change BATTERY_SCALE after measuring your actual divider if needed.
#define BATTERY_SCALE 2.0f
#define ADC_REF 5.0f

Adafruit_SSD1306 display(128,64,&Wire,-1);
Adafruit_NeoPixel leds(LEDS,LED_PIN,NEO_GRB+NEO_KHZ800);

#define IR0 0x19
#define IR1 0x45
#define IR2 0x46
#define IR3 0x47
#define IR4 0x44
#define IR5 0x40
#define IR6 0x43
#define IR7 0x07
#define IR8 0x15
#define IR9 0x09
#define IROK 0x1C
#define IRSTAR 0x16
#define IRHASH 0x0D
#define IRUP 0x18
#define IRDOWN 0x52
#define IRLEFT 0x08
#define IRRIGHT 0x5A

struct DateTime { uint8_t s,m,h,dow,day,mon,yr; };
struct Config { uint8_t magic,ver,alarmH,alarmM,alarmOn,r,g,b,rgbBright,oledBright; };
Config cfg;
DateTime now={0,0,0,3,12,8,26};

volatile uint32_t irFrame=0,irLast=0;
volatile uint8_t irBits=0,irCmd=0;
volatile bool irRx=false,irReady=false;

uint8_t mainPos=0,settingsPos=0,colorPos=0;
uint16_t calYear=2026; uint8_t calMonth=8;
uint32_t activityMs=0,rtcMs=0,uiMs=0;
uint8_t inputLen=0; uint32_t inputBuf=0;
bool rtcOK=false,sleeping=false,alarmRinging=false,cyan=false;
uint32_t cyanOff=0;
uint8_t lastIR=0;

uint32_t timerMs=0,timerLast=0; bool timerRun=false,timerDone=false;
uint32_t swMs=0,swLast=0; bool swRun=false;
uint32_t pomoMs=25UL*60UL*1000UL,pomoLast=0; bool pomoRun=false,pomoBreak=false,pomoDone=false;

struct Color { uint8_t r,g,b; };
const Color colors[] PROGMEM = {
  {60,0,0},{0,60,0},{0,0,60},{0,55,55},{55,0,55},{55,45,0},{60,60,60},{55,22,0},{30,0,60}
};
const char colorNames[][8] PROGMEM = {"RED","GREEN","BLUE","CYAN","MAGENTA","YELLOW","WHITE","ORANGE","PURPLE"};
#define COLOR_COUNT 9

enum Mode : uint8_t { HOME,MENU,ALARM,SETTIME,SETALARM,TIMER,SETTIMER,STOPWATCH,CALENDAR,POMODORO,SETTINGS,SETDATE,LEDCOLOR,LEDBRIGHT,OLED_BRIGHT,BATTERY,RINGING };
Mode mode=HOME;

ISR(WDT_vect) {}

void irISR(){
  uint32_t t=micros(),d=t-irLast; irLast=t;
  if(d>12000UL){irFrame=0;irBits=0;irRx=true;return;}
  if(!irRx)return;
  if(d>=850UL&&d<=1400UL){irFrame&=~(1UL<<irBits);irBits++;}
  else if(d>=1900UL&&d<=2600UL){irFrame|=(1UL<<irBits);irBits++;}
  else {irRx=false;irBits=0;return;}
  if(irBits>=32){uint8_t a=irFrame,c=irFrame>>16,ai=irFrame>>8,ci=irFrame>>24;
    if((uint8_t)(a^ai)==0xFF&&(uint8_t)(c^ci)==0xFF){irCmd=c;irReady=true;}
    irRx=false;irBits=0;
  }
}

uint8_t b2d(uint8_t x){return (x>>4)*10+(x&15);}
uint8_t d2b(uint8_t x){return (x/10<<4)|(x%10);}
void p2(uint8_t x){if(x<10)display.print('0');display.print(x);}

bool rtcRead(DateTime &t){
  Wire.beginTransmission(RTC_ADDR);Wire.write(0);if(Wire.endTransmission()!=0)return false;
  if(Wire.requestFrom((uint8_t)RTC_ADDR,(uint8_t)7)!=7)return false;
  t.s=b2d(Wire.read()&0x7F);t.m=b2d(Wire.read());t.h=b2d(Wire.read()&0x3F);
  t.dow=b2d(Wire.read());t.day=b2d(Wire.read());t.mon=b2d(Wire.read()&0x1F);t.yr=b2d(Wire.read());return true;
}
bool rtcWrite(const DateTime &t){
  Wire.beginTransmission(RTC_ADDR);Wire.write(0);Wire.write(d2b(t.s));Wire.write(d2b(t.m));Wire.write(d2b(t.h));
  Wire.write(d2b(t.dow?t.dow:1));Wire.write(d2b(t.day));Wire.write(d2b(t.mon));Wire.write(d2b(t.yr));return Wire.endTransmission()==0;
}
float rtcTemp(){
  Wire.beginTransmission(RTC_ADDR);Wire.write(0x11);if(Wire.endTransmission()!=0)return -127;
  if(Wire.requestFrom((uint8_t)RTC_ADDR,(uint8_t)2)!=2)return -127;
  int8_t w=(int8_t)Wire.read();uint8_t f=Wire.read();return w+((f>>6)*0.25f);
}

void loadConfig(){
  EEPROM.get(0,cfg);
  if(cfg.magic!=EEPROM_MAGIC||cfg.ver!=EEPROM_VER||cfg.alarmH>23||cfg.alarmM>59||cfg.rgbBright>255||cfg.oledBright>255){
    cfg={EEPROM_MAGIC,EEPROM_VER,7,0,0,0,55,55,RGB_DEFAULT,OLED_DEFAULT};EEPROM.put(0,cfg);
  }
}
void saveConfig(){EEPROM.put(0,cfg);}

void setRGB(uint8_t r,uint8_t g,uint8_t b){
  leds.setBrightness(cfg.rgbBright);uint32_t c=leds.Color(r,g,b);for(uint8_t i=0;i<LEDS;i++)leds.setPixelColor(i,c);leds.show();
}
void rgbOff(){leds.clear();leds.show();}
void flashCyan(){setRGB(0,60,60);cyan=true;cyanOff=millis()+100;}
void serviceCyan(){if(cyan&&(long)(millis()-cyanOff)>=0){cyan=false;rgbOff();}}
void serviceIdleRGB(){
  if(mode!=HOME||sleeping||alarmRinging||cyan)return;
  static uint32_t last=0; if(millis()-last<120)return; last=millis();
  uint8_t p=(millis()/20)%64; uint8_t k=(p<32)?p:(63-p);
  setRGB((uint16_t)cfg.r*(8+k)/40,(uint16_t)cfg.g*(8+k)/40,(uint16_t)cfg.b*(8+k)/40);
}
void beep(uint16_t hz,uint16_t ms){tone(BUZZER_PIN,hz,ms);}

float battPinV(){uint32_t sum=0;for(uint8_t i=0;i<8;i++)sum+=analogRead(BATTERY_PIN);return ((float)sum/8.0f)*ADC_REF/1023.0f;}
float battV(){return battPinV()*BATTERY_SCALE;}
uint8_t battPct(float v){
  if(v>=4.15)return 100;if(v>=4.05)return 90;if(v>=3.95)return 80;if(v>=3.85)return 70;
  if(v>=3.80)return 60;if(v>=3.75)return 50;if(v>=3.60)return 30;if(v>=3.45)return 20;if(v>=3.30)return 10;return 0;
}
void battColor(uint8_t p){if(p>80)setRGB(0,0,70);else if(p>50)setRGB(0,60,0);else if(p>30)setRGB(65,55,0);else if(p>20)setRGB(65,22,0);else setRGB(75,0,0);}

bool leap(uint16_t y){return(y%400==0)||((y%4==0)&&(y%100!=0));}
uint8_t daysInMonth(uint16_t y,uint8_t m){static const uint8_t d[]={31,28,31,30,31,30,31,31,30,31,30,31};return(m==2&&leap(y))?29:d[m-1];}
uint8_t weekday(uint16_t y,uint8_t m,uint8_t d){static const uint8_t t[]={0,3,2,5,0,3,5,1,4,6,2,4};if(m<3)y--;return(y+y/4-y/100+y/400+t[m-1]+d)%7;}
const char* monthName(uint8_t m){static const char* const n[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};return n[m-1];}

void header(const __FlashStringHelper *s){display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(1);display.setCursor(0,0);display.print(s);display.drawLine(0,9,127,9,SSD1306_WHITE);}
void dots(uint8_t pos,uint8_t n){
  uint8_t total=n*7-1,start=(128-total)/2;
  for(uint8_t i=0;i<n;i++){uint8_t x=start+i*7;if(i==pos)display.fillCircle(x,58,2,SSD1306_WHITE);else display.drawCircle(x,58,2,SSD1306_WHITE);}
}

void slideMenu(const char* const items[],uint8_t n,uint8_t pos,int8_t dir){
  int16_t from=dir>0?128:-128,to=34;uint8_t frames=4;
  for(uint8_t f=0;f<frames;f++){int16_t x=from+(to-from)*(f+1)/frames;display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(2);
    int16_t w=strlen(items[pos])*12;display.setCursor(x-w/2,24);display.print(items[pos]);dots(pos,n);display.display();delay(18);}
}
void drawMainMenu(){static const char* const items[]={"TIMER","ALARM","CALENDAR","POMODORO","SETTINGS"};display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(2);int16_t w=strlen(items[mainPos])*12;display.setCursor((128-w)/2,24);display.print(items[mainPos]);dots(mainPos,5);display.display();}
void drawSettings(){static const char* const items[]={"LED COLOR","LED BRIGHT","DATE","TIME","BATTERY","OLED BRIGHT"};display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(1);int16_t w=strlen(items[settingsPos])*6;display.setCursor((128-w)/2,25);display.setTextSize(2);w=strlen(items[settingsPos])*12;display.setCursor((128-w)/2,24);display.print(items[settingsPos]);dots(settingsPos,6);display.display();}

void drawHome(){
  display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(3);display.setCursor(2,0);p2(now.h);display.print(':');p2(now.m);display.print(':');p2(now.s);
  display.setTextSize(1);display.setCursor(36,31);p2(now.day);display.print(' ');display.print(monthName(now.mon));display.print(" 20");p2(now.yr);
  display.setCursor(0,43);display.print(rtcTemp(),1);display.print(" C");display.setCursor(68,43);display.print("BAT ");display.print(battPct(battV()));display.print('%');
  display.setCursor(0,55);display.print("AL ");if(cfg.alarmOn){p2(cfg.alarmH);display.print(':');p2(cfg.alarmM);}else display.print("OFF");
  display.display();
}

void drawAlarm(){header(F("ALARM"));display.setTextSize(3);display.setCursor(18,18);p2(cfg.alarmH);display.print(':');p2(cfg.alarmM);display.setTextSize(1);display.setCursor(42,48);display.print(cfg.alarmOn?"ON":"OFF");display.setCursor(0,58);display.print("OK ON/OFF  1 SET  * BACK");display.display();}
void drawInput(const __FlashStringHelper* title,const __FlashStringHelper* fmt){header(title);display.setTextSize(3);display.setCursor(15,22);if(inputLen==0)display.print(fmt);else{uint32_t x=inputBuf;uint8_t d[8];uint8_t n=inputLen;for(int8_t i=n-1;i>=0;i--){d[i]=x%10;x/=10;}for(uint8_t i=0;i<n;i++){display.print(d[i]);if((n==4&&i==1)||(n==6&&(i==1||i==3)))display.print(i==3&&n==6?'/':':');}}display.setTextSize(1);display.setCursor(0,56);display.print("NUMBERS  OK=SAVE  *=BACK");display.display();}
void drawTimer(){header(F("TIMER"));uint32_t sec=timerMs/1000;display.setTextSize(3);display.setCursor(25,22);p2((sec/60)%100);display.print(':');p2(sec%60);display.setTextSize(1);display.setCursor(0,56);display.print("OK RUN 1 SET DN SW * BACK");display.display();}
void drawStopwatch(){header(F("STOPWATCH"));uint32_t sec=swMs/1000;display.setTextSize(2);display.setCursor(8,27);p2(sec/3600);display.print(':');p2((sec/60)%60);display.print(':');p2(sec%60);display.setTextSize(1);display.setCursor(0,56);display.print("OK START  0 RESET  * BACK");display.display();}
void drawPomo(){header(F("POMODORO"));uint32_t sec=pomoMs/1000;display.setTextSize(3);display.setCursor(25,21);p2((sec/60)%100);display.print(':');p2(sec%60);display.setTextSize(1);display.setCursor(45,49);display.print(pomoBreak?"BREAK":"WORK");display.setCursor(0,58);display.print("OK START/PAUSE 0 RESET");display.display();}

void drawCalendar(){
  display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(1);display.setCursor(35,0);display.print(monthName(calMonth));display.print(' ');display.print(calYear);
  display.setCursor(9,10);display.print("S M T W T F S");uint8_t first=weekday(calYear,calMonth,1),days=daysInMonth(calYear,calMonth);
  for(uint8_t d=1;d<=days;d++){uint8_t idx=first+d-1,row=idx/7,col=idx%7;uint8_t x=8+col*18,y=20+row*8;bool hi=rtcOK&&calYear==2000+now.yr&&calMonth==now.mon&&d==now.day;if(hi){display.fillRoundRect(x-1,y-1,16,8,1,SSD1306_WHITE);display.setTextColor(SSD1306_BLACK);}display.setCursor(x,y);if(d<10)display.print(' ');display.print(d);if(hi)display.setTextColor(SSD1306_WHITE);}
  display.setCursor(0,58);display.print("L/R MONTH  U/D YEAR  * BACK");display.display();
}

void drawColor(){header(F("LED COLOR"));Color c;memcpy_P(&c,&colors[colorPos],sizeof(c));char name[8];strcpy_P(name,colorNames[colorPos]);display.setTextSize(2);display.setCursor((128-strlen(name)*12)/2,20);display.print(name);setRGB(c.r,c.g,c.b);dots(colorPos,COLOR_COUNT);display.display();}
void drawLEDBright(){header(F("LED BRIGHTNESS"));display.setTextSize(2);display.setCursor(49,18);display.print(cfg.rgbBright);display.print('%');display.drawRect(14,42,100,8,SSD1306_WHITE);display.fillRect(16,44,(uint16_t)96*cfg.rgbBright/255,4,SSD1306_WHITE);display.display();setRGB(cfg.r,cfg.g,cfg.b);}
void drawOLEDBright(){header(F("OLED BRIGHTNESS"));display.setTextSize(2);display.setCursor(49,18);display.print((uint16_t)cfg.oledBright*100/255);display.print('%');display.drawRect(14,42,100,8,SSD1306_WHITE);display.fillRect(16,44,(uint16_t)96*cfg.oledBright/255,4,SSD1306_WHITE);display.display();}
void drawBattery(){header(F("BATTERY"));display.setTextSize(2);display.setCursor(35,15);display.print(battV(),2);display.print('V');display.setTextSize(1);display.setCursor(0,38);display.print("ADC PIN ");display.print(battPinV(),3);display.print("V");display.setCursor(0,49);display.print("LEVEL ");display.print(battPct(battV()));display.print('%');display.setCursor(0,59);display.print("DIVIDER SCALE ");display.print(BATTERY_SCALE,2);display.display();battColor(battPct(battV()));}
void drawRinging(){display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(3);display.setCursor(17,14);display.println("ALARM!");display.setTextSize(1);display.setCursor(18,48);display.print("PRESS OK TO STOP");display.display();}

void alarmStart(){alarmRinging=true;mode=RINGING;drawRinging();}
void alarmStop(){alarmRinging=false;noTone(BUZZER_PIN);rgbOff();mode=HOME;activityMs=millis();drawHome();}
void serviceAlarm(){if(!alarmRinging)return;static uint32_t t=0;static bool p=false;if(millis()-t>=220){t=millis();p=!p;if(p){setRGB(75,0,0);tone(BUZZER_PIN,1300);}else{rgbOff();noTone(BUZZER_PIN);}}}
void updateAlarm(){static uint32_t lastMinute=0;if(!cfg.alarmOn||alarmRinging)return;uint32_t key=(uint32_t)now.yr*525600UL+(uint32_t)now.mon*44640UL+(uint32_t)now.day*1440UL+(uint32_t)now.h*60UL+now.m;if(now.h==cfg.alarmH&&now.m==cfg.alarmM&&key!=lastMinute){lastMinute=key;alarmStart();}}

void updateTimer(){if(!timerRun)return;uint32_t t=millis(),d=t-timerLast;timerLast=t;if(d>=timerMs){timerMs=0;timerRun=false;timerDone=true;}else timerMs-=d;if(timerDone){timerDone=false;for(uint8_t i=0;i<3;i++){setRGB(70,25,0);tone(BUZZER_PIN,1400,150);delay(170);rgbOff();delay(70);}}}
void updateSW(){if(swRun){uint32_t t=millis();swMs+=t-swLast;swLast=t;}}
void updatePomo(){if(!pomoRun)return;uint32_t t=millis(),d=t-pomoLast;pomoLast=t;if(d>=pomoMs){pomoMs=0;pomoRun=false;pomoDone=true;}else pomoMs-=d;if(pomoDone){pomoDone=false;pomoBreak=!pomoBreak;pomoMs=(pomoBreak?5UL:25UL)*60UL*1000UL;for(uint8_t i=0;i<3;i++){setRGB(pomoBreak?0:70,pomoBreak?60:15,0);tone(BUZZER_PIN,pomoBreak?900:1500,160);delay(180);rgbOff();delay(70);}}}

void commitTime(){if(inputLen!=4)return;uint8_t h=inputBuf/1000,m=(inputBuf/10)%100;if(h>23||m>59){beep(400,100);return;}now.h=h;now.m=m;now.s=0;if(rtcWrite(now)){inputLen=0;inputBuf=0;mode=HOME;drawHome();}}
void commitAlarm(){if(inputLen!=4)return;uint8_t h=inputBuf/1000,m=(inputBuf/10)%100;if(h>23||m>59){beep(400,100);return;}cfg.alarmH=h;cfg.alarmM=m;cfg.alarmOn=true;saveConfig();inputLen=0;inputBuf=0;mode=ALARM;drawAlarm();}
void commitTimer(){if(inputLen!=4)return;uint8_t m=inputBuf/1000,s=(inputBuf/10)%100;if(s>59){beep(400,100);return;}timerMs=((uint32_t)m*60UL+s)*1000UL;timerRun=false;inputLen=0;inputBuf=0;mode=TIMER;drawTimer();}
void commitDate(){if(inputLen!=6)return;uint8_t d=inputBuf/10000,m=(inputBuf/100)%100,y=inputBuf%100;if(m<1||m>12||d<1||d>daysInMonth(2000+y,m)){beep(400,100);return;}now.day=d;now.mon=m;now.yr=y;now.dow=weekday(2000+y,m,d);if(rtcWrite(now)){calYear=2000+y;calMonth=m;inputLen=0;inputBuf=0;mode=HOME;drawHome();}}

bool isDigit(uint8_t c){return c==IR0||c==IR1||c==IR2||c==IR3||c==IR4||c==IR5||c==IR6||c==IR7||c==IR8||c==IR9;}
uint8_t digit(uint8_t c){switch(c){case IR0:return 0;case IR1:return 1;case IR2:return 2;case IR3:return 3;case IR4:return 4;case IR5:return 5;case IR6:return 6;case IR7:return 7;case IR8:return 8;default:return 9;}}
void inputAdd(uint8_t d,uint8_t max){if(inputLen<max){inputBuf=inputBuf*10+d;inputLen++;}}
void inputClear(){inputLen=0;inputBuf=0;}

void settingsMove(int8_t dir){uint8_t n=6;uint8_t old=settingsPos;if(dir>0){settingsPos=(settingsPos+1)%n;}else settingsPos=(settingsPos+n-1)%n;drawSettings();}
void mainMove(int8_t dir){uint8_t n=5;uint8_t old=mainPos;if(dir>0)mainPos=(mainPos+1)%n;else mainPos=(mainPos+n-1)%n;static const char* const items[]={"TIMER","ALARM","CALENDAR","POMODORO","SETTINGS"};slideMenu(items,n,mainPos,dir);(void)old;}

void enterMain(){mode=MENU;mainPos=0;drawMainMenu();}
void enterSettings(){mode=SETTINGS;settingsPos=0;drawSettings();}

void processIR(uint8_t c){
  activityMs=millis();lastIR=c;
  if(sleeping){sleeping=false;display.ssd1306_command(SSD1306_DISPLAYON);drawHome();flashCyan();return;}
  flashCyan();
  if(alarmRinging){if(c==IROK)alarmStop();return;}
  if(c==IRSTAR){inputClear();mode=HOME;drawHome();return;}
  if(mode==HOME){if(c==IRHASH)enterMain();return;}

  switch(mode){
    case MENU:
      if(c==IRLEFT)mainMove(-1);else if(c==IRRIGHT)mainMove(1);else if(c==IROK){if(mainPos==0){mode=TIMER;drawTimer();}else if(mainPos==1){mode=ALARM;drawAlarm();}else if(mainPos==2){calYear=2000+now.yr;calMonth=now.mon;mode=CALENDAR;drawCalendar();}else if(mainPos==3){mode=POMODORO;drawPomo();}else enterSettings();}break;
    case ALARM:
      if(c==IROK){cfg.alarmOn=!cfg.alarmOn;saveConfig();drawAlarm();}else if(c==IR1){inputClear();mode=SETALARM;drawInput(F("SET ALARM"),F("HH:MM"));}break;
    case SETALARM:
      if(isDigit(c))inputAdd(digit(c),4);else if(c==IROK)commitAlarm();if(mode==SETALARM)drawInput(F("SET ALARM"),F("HH:MM"));break;
    case TIMER:
      if(c==IR1){inputClear();mode=SETTIMER;drawInput(F("SET TIMER"),F("MM:SS"));}else if(c==IR0){timerRun=false;timerMs=0;}else if(c==IRDOWN){mode=STOPWATCH;drawStopwatch();}else if(c==IROK&&timerMs){timerRun=!timerRun;timerLast=millis();}drawTimer();break;
    case SETTIMER:
      if(isDigit(c))inputAdd(digit(c),4);else if(c==IROK)commitTimer();if(mode==SETTIMER)drawInput(F("SET TIMER"),F("MM:SS"));break;
    case STOPWATCH:
      if(c==IROK){swRun=!swRun;swLast=millis();}else if(c==IR0){swRun=false;swMs=0;}drawStopwatch();break;
    case CALENDAR:
      if(c==IRLEFT){if(calMonth==1){calMonth=12;calYear--;}else calMonth--;drawCalendar();}else if(c==IRRIGHT){if(calMonth==12){calMonth=1;calYear++;}else calMonth++;drawCalendar();}else if(c==IRUP){if(calYear<2099)calYear++;drawCalendar();}else if(c==IRDOWN){if(calYear>2000)calYear--;drawCalendar();}break;
    case POMODORO:
      if(c==IROK){pomoRun=!pomoRun;pomoLast=millis();}else if(c==IR0){pomoRun=false;pomoBreak=false;pomoMs=25UL*60UL*1000UL;}drawPomo();break;
    case SETTINGS:
      if(c==IRLEFT)settingsMove(-1);else if(c==IRRIGHT)settingsMove(1);else if(c==IROK){if(settingsPos==0){mode=LEDCOLOR;drawColor();}else if(settingsPos==1){mode=LEDBRIGHT;drawLEDBright();}else if(settingsPos==2){inputClear();mode=SETDATE;drawInput(F("SET DATE"),F("DD/MM/YY"));}else if(settingsPos==3){inputClear();mode=SETTIME;drawInput(F("SET TIME"),F("HH:MM"));}else if(settingsPos==4){mode=BATTERY;drawBattery();}else {mode=OLED_BRIGHT;drawOLEDBright();}}break;
    case SETDATE:
      if(isDigit(c))inputAdd(digit(c),6);else if(c==IROK)commitDate();if(mode==SETDATE)drawInput(F("SET DATE"),F("DD/MM/YY"));break;
    case SETTIME:
      if(isDigit(c))inputAdd(digit(c),4);else if(c==IROK)commitTime();if(mode==SETTIME)drawInput(F("SET TIME"),F("HH:MM"));break;
    case LEDCOLOR:
      if(c==IRLEFT){colorPos=(colorPos+COLOR_COUNT-1)%COLOR_COUNT;Color x;memcpy_P(&x,&colors[colorPos],sizeof(x));cfg.r=x.r;cfg.g=x.g;cfg.b=x.b;saveConfig();drawColor();}
      else if(c==IRRIGHT){colorPos=(colorPos+1)%COLOR_COUNT;Color x;memcpy_P(&x,&colors[colorPos],sizeof(x));cfg.r=x.r;cfg.g=x.g;cfg.b=x.b;saveConfig();drawColor();}
      else if(c==IROK){mode=SETTINGS;drawSettings();}break;
    case LEDBRIGHT:
      if(c==IRLEFT&&cfg.rgbBright>=5)cfg.rgbBright-=5;else if(c==IRRIGHT&&cfg.rgbBright<=250)cfg.rgbBright+=5;else if(c==IROK){saveConfig();mode=SETTINGS;drawSettings();break;}drawLEDBright();break;
    case OLED_BRIGHT:
      if(c==IRLEFT&&cfg.oledBright>=10)cfg.oledBright-=10;else if(c==IRRIGHT&&cfg.oledBright<=245)cfg.oledBright+=10;else if(c==IROK){display.ssd1306_command(SSD1306_SETCONTRAST);display.ssd1306_command(cfg.oledBright);saveConfig();mode=SETTINGS;drawSettings();break;}display.ssd1306_command(SSD1306_SETCONTRAST);display.ssd1306_command(cfg.oledBright);drawOLEDBright();break;
    case BATTERY: if(c==IROK||c==IRLEFT||c==IRRIGHT)drawBattery();break;
    default:break;
  }
}

void sleepNow(){
  if(mode!=HOME||alarmRinging||timerRun||swRun||pomoRun||sleeping)return;
  sleeping=true;rgbOff();display.ssd1306_command(SSD1306_DISPLAYOFF);MCUSR=0;
  WDTCSR=(1<<WDCE)|(1<<WDE);WDTCSR=(1<<WDIE)|(1<<WDP3)|(1<<WDP0);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);sleep_enable();sleep_cpu();sleep_disable();
  WDTCSR=(1<<WDCE)|(1<<WDE);WDTCSR=0;MCUSR=0;rtcRead(now);sleeping=false;display.ssd1306_command(SSD1306_DISPLAYON);drawHome();
}
void sleepManage(){if(mode==HOME&&!alarmRinging&&!timerRun&&!swRun&&!pomoRun&&!sleeping&&millis()-activityMs>=SLEEP_MS)sleepNow();}

void setup(){
  pinMode(IR_PIN,INPUT);pinMode(BUZZER_PIN,OUTPUT);digitalWrite(BUZZER_PIN,LOW);pinMode(BATTERY_PIN,INPUT);
  Wire.begin();loadConfig();leds.begin();leds.setBrightness(cfg.rgbBright);rgbOff();
  if(!display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR)){for(;;){} }
  display.ssd1306_command(SSD1306_SETCONTRAST);display.ssd1306_command(cfg.oledBright);
  display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(3);display.setCursor(20,7);display.print(F("H328"));display.setTextSize(1);display.setCursor(37,42);display.print(F("by harsha ;)"));display.display();delay(900);
  rtcOK=rtcRead(now);if(!rtcOK){now=(DateTime){0,0,0,3,12,8,26};}calYear=2000+now.yr;calMonth=now.mon;
  // Find selected palette entry for UI continuity.
  colorPos=0;for(uint8_t i=0;i<COLOR_COUNT;i++){Color x;memcpy_P(&x,&colors[i],sizeof(x));if(x.r==cfg.r&&x.g==cfg.g&&x.b==cfg.b){colorPos=i;break;}}
  irLast=micros();attachInterrupt(digitalPinToInterrupt(IR_PIN),irISR,FALLING);activityMs=millis();drawHome();
}

void loop(){
  serviceCyan();serviceAlarm();serviceIdleRGB();
  if(millis()-rtcMs>=500){rtcMs=millis();if(rtcRead(now)){rtcOK=true;updateAlarm();if(mode==HOME&&!sleeping)drawHome();}else rtcOK=false;}
  updateTimer();updateSW();updatePomo();
  if(mode==TIMER&&millis()-uiMs>=200){uiMs=millis();drawTimer();}
  if(mode==STOPWATCH&&millis()-uiMs>=100){uiMs=millis();drawStopwatch();}
  if(mode==POMODORO&&millis()-uiMs>=200){uiMs=millis();drawPomo();}
  if(irReady){noInterrupts();uint8_t c=irCmd;irReady=false;interrupts();processIR(c);}
  sleepManage();
}

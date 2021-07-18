#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <SD.h>

#define NEOPIN        2
#define NUMPIXELS 17

#define NUMCONFIG 32
#define CONFIG_ALERTDELAY 0
#define PIN_ALARM_SIREN 5
#define PIN_ALARM_FIRE 6

#define RELAY_ON LOW
#define RELAY_OFF HIGH

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);
File myFile;


int pinAlarmInputs[16] = {30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45};
unsigned long timLastAlertDelay = 0UL;
unsigned long timLastArmDelay = 0UL;
bool AlarmArmed = false;
bool AlarmAlert = false;
bool AlarmInAlert = false;
# trouble=true
bool AlarmZoneStatus[16] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
bool AlarmAlertStatus[16] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
bool AlarmAlertZone[16] = {false,false,false,false,false,false,false,false,true,true,true,true,true,true,true,true}; // false = security, true = fire
bool AlarmHasAlerted[16] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};

const int buttonPin[2] = {22,23};    // the number of the pushbutton pin
int buttonState[2];             // the current reading from the input pin
int lastButtonState[2] = {HIGH,HIGH};   // the previous reading from the input pin
unsigned long lastDebounceTime[2] = {0UL,0UL};  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

unsigned long configVars[NUMCONFIG] = {45000};

LiquidCrystal_I2C lcd(0x27,40,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

void NeoZoneStatus(int Zone, int Status) {
  int R = 0;
  int G = 0;
  int B = 0;
  if (Status == 0) { //disarmed, no trouble
    R = 99;
    G = 195;
    B = 86;
  }else if (Status == 1) { //disarmed, trouble
    R = 255;
    G = 255;
    B = 0;
  } else if (Status == 2) { //armed, no trouble
    R = 0;
    G = 255;
    B = 0;
  } else if (Status == 3) { //armed, trouble security
    R = 0;
    G = 35;
    B = 147;
  } else if (Status == 4) { //armed/disarmed, alarm triggerd security
    R = 0;
    G = 0;
    B = 255;
  } else if (Status == 5) { //armed/disarmed, trigger Fire
    R = 255;
    G = 0;
    B = 0;
  }
    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(Zone, pixels.Color(R, G, B));

    pixels.show();   // Send the updated pixel colors to the hardware.
}


bool writeConfig() {
  myFile = SD.open("alarm.cfg", FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    for (int i=0; i<NUMCONFIG; i++) {
      myFile.println(configVars[i])
    }
    // close the file:
    myFile.close();
    return(false);
  } else {
    return(true);
  }
}

bool readConfig() {
  myFile = SD.open("alarm.cfg");
  if (myFile) {
    // read from the file until there's nothing else in it:
    int tmpInt = 0;
    String tmpBuffer = "";
    char tmpChar;
    while (myFile.available()) {
      tmpChar = myFile.read();
      if (tmpChar != '\n' && tmpChar != '\r') {
        tmpBuffer += tmpChar;
      } else if (tmpChar == '\n' && tmpInt < NUMCONFIG) {
        configVars[tmpInt] = tmpBuffer.toInt();
        tmpInt++;
        tmpBuffer = "";
      }
    }
    // close the file:
    myFile.close();
    return(false);
  } else {
    // if the file didn't open, print an error:
    return(true);
  }
}

void NeoTest() {
  for (int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    pixels.show();
    delay(100);
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    pixels.show();
    delay(100);
    pixels.setPixelColor(i, pixels.Color(0, 0, 255));
    pixels.show();
    delay(100);
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    pixels.show();
  }
  pixels.clear();
}

void setup()
{
  //Serial.begin(9600);
  pixels.begin();
  pixels.clear();

  NeoTest();

  pixels.setPixelColor(16, pixels.Color(0, 0, 0)); // init start
  pixels.show();

  if (!SD.begin(4)) {
    pixels.setPixelColor(16, pixels.Color(255, 0, 0)); // init SD FAIL
    pixels.show();
    while (1);
  }
  if (readConfig() != false) {
    pixels.setPixelColor(16, pixels.Color(0, 255, 0)); // initialized
    pixels.show();
  } else {
    writeConfig();
    pixels.setPixelColor(16, pixels.Color(0, 0, 255)); // initialized: no config file
    pixels.show();
  }

  for (int i=0; i<16; i++) {
    pinMode(pinAlarmInputs[i],INPUT_PULLUP);
  }
  pinMode(PIN_ALARM_SIREN,OUTPUT);
  digitalWrite(PIN_ALARM_SIREN,RELAY_OFF);
  pinMode(PIN_ALARM_FIRE,OUTPUT);
  digitalWrite(PIN_ALARM_FIRE,RELAY_OFF);


  pinMode(buttonPin[0],INPUT_PULLUP);
  pinMode(buttonPin[1],INPUT_PULLUP);

}


void loop()
{
  unsigned long timMillis = millis();

  int readingA = digitalRead(buttonPin[0]);
  int readingD = digitalRead(buttonPin[1]);
  if (readingA != lastButtonState[0]) {
    // reset the debouncing timer
    lastDebounceTime[0] = timMillis();
  }
  if (readingD != lastButtonState[1]) {
    // reset the debouncing timer
    lastDebounceTime[1] = timMillis;
  }
  if ((timMillis - lastDebounceTime[0]) > debounceDelay) {
    if (readingA != buttonState[0]) {
      buttonState[0] = readingA;
      if (buttonState[0] == LOW) { // arm instant
        for (int i=0; i<16; i++) {
          if (AlarmAlertZone[i] == false) {
            AlarmAlertStatus[i] = false;
          }
        }
        AlarmArmed = true;
      }
    }
  }
  if ((timMillis - lastDebounceTime[1]) > debounceDelay) {
    if (readingD != buttonState[1]) {
      buttonState[1] = readingD;
      if (buttonState[1] == LOW) {
        if (AlarmArmed == true) { // disarm instant
          for (int i=0; i<16; i++) {
            if (AlarmAlertZone[i] == false) {
              AlarmHasAlerted[i] = AlarmAlertStatus[i];
              AlarmAlertStatus[i] = false;
            }
          }
          AlarmArmed = false;
        } else { // clear
          for (int i=0; i<16; i++) {
            if (AlarmAlertZone[i] == false) {
              AlarmHasAlerted[i] = false;
            }
          }
        }
      }
    }
  }

  for (int i = 0; i<16; i++) {
    if (digitalRead(pinAlarmInputs[i]) == LOW) { AlarmZoneStatus[i] = true; }
    else {AlarmZoneStatus[i] = false;}
    if (AlarmAlertZone[i] == false) {
      if (AlarmArmed == true) {
        if (AlarmZoneStatus[i] == true) {
          AlarmAlertStatus[i] = true;
          AlarmInAlert = true;
            if (timLastAlertDelay == 0UL) {
              timLastAlertDelay = timMillis;
            }
            NeoZoneStatus(i,3);
        } else {
          NeoZoneStatus(i,2);
        }
      } else {
        if (AlarmZoneStatus[i] == true) {
          NeoZoneStatus(i,1);
        } else {
          NeoZoneStatus(i,0);
        }
      }
    }
    if (AlarmAlertZone[i] == true) {
      if (AlarmZoneStatus[i] == true) {
        NeoZoneStatus(i,5);
        digitalWrite(PIN_ALARM_FIRE,RELAY_ON);
      } else {
        NeoZoneStatus(i,2);
        digitalWrite(PIN_ALARM_FIRE,RELAY_OFF);
      }
    }
  }
  if (AlarmArmed == false) {
    digitalWrite(PIN_ALARM_SIREN,RELAY_OFF);
    AlarmInAlert = false;
    timLastAlertDelay = 0UL;

  } else if (AlarmArmed == true && AlarmInAlert == true && (timMillis - timLastAlertDelay) >= configVars[CONFIG_ALERTDELAY] && timLastAlertDelay != 0UL) {
    for (int j=0; j<16; j++) {
      if (AlarmAlertStatus[j] == true && AlarmAlertZone[i] == false) {
        digitalWrite(PIN_ALARM_SIREN,RELAY_ON);
        timLastAlertDelay = 0UL;
      }
    }
  }
}

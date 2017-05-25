#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

//7-Segment LED Raw Bit Drawing
//    A
// F     B
//    G
// E     C
//    D    P
//Position:   PGFEDCBA
//Bit Mask:  B11111111
// H:        B01110110
// U:        B00111110
// S:        B01101101
// C:        B00111001

enum timerState {NONE, READY, ACTIVE, PAUSED, FIRED};
enum activeAction {RESTART, PAUSE, RESUME, COUNT};

//Display variables
Adafruit_7segment matrix = Adafruit_7segment();

int buzzerPin = 13;
int rotarySelPins[ ] = {17, 2, 3, 4, 5, 6, 7, 8};
int backButtonPin = 9;
int actionButtonPin = 10;
int actionExternalPin = 11;

int actionButtonState = 0;
int backButtonState = 0;
int actionExternalState = 0;
int rotarySelStates[ ] = {0, 0, 0, 0, 0, 0, 0, 0};
int mode = 1;


//DisplayCodes
//1H:1.0 'H'ybrid counter and timer 60s countdown time and counts button contacts
//2U:15  'U'hr, timer counting down 15 minutes
//3U:60  'U'hr, timer counting down 60 minutes
//4S:0.0 'S'top Uhr, works like a stop watch. Displaying seconds and 100th of seconds 0 - 59.99 seconds and then minutes and seconds 1:00 - 99:59 min
//5S:1.5 'S'top Uhr, works like a stop watch for Kumite matches. Counts down time from 01:30 min to 00:00 and allows pause resume
//6S:2.0 'S'top Uhr, works like a stop watch for Kumite matches. Counts down time from 02:00 min to 00:00 and allows pause resume
//7C:00  'C'ounter. Counts from 0 to 9999
//8P:--  'P'rogramming... might be obsolete could be replaced by a U:3.0 or become a programmable timer

//buzzer Chime1 variables
int buzzerChime1TimeBase = 100;
String buzzerChime1Pattern = "|_||||_|";
int buzzerChime1PauseBetweenPatterns = 2000;
unsigned long buzzerChime1previousMillis = 0;
int buzzerChime1State = LOW;
int buzzerChime1PatternRepeatLimit = 3;
int buzzerChime1PatternRepeated = 0;


//timer functions
//void reset()
//void start()
//void stop()
//void pause()
//void update()

class Timer
{
    // Class Member Variables
    // These are initialized at startup
    int Duration;
    activeAction ActiveAction;
    int BuzzerRepeats;
    bool AutoRestart;
    uint8_t TypeMask;
    uint8_t TypeMaskCache;
    Adafruit_7segment LEDMatrix;

    int Time = Duration;
    long UpdateInterval = 1000;

    // These maintain the current state
    timerState State;               // timer state
    timerState StateCache;
    int ModeCache;
    int NumberCache;
    int Mode;
    unsigned long PreviousMillis;   // will store last time the timer was updated
    unsigned long CurrentMillis;

    int DisplayNumber;

    // Constructor - creates a Timer
    // and initializes the member variables and state
  public:
    Timer(int duration, activeAction action, bool autoRestart, uint8_t typeMask, Adafruit_7segment ledmatrix, int buzzerRepeats, int mode)
    {
      Duration = duration;
      ActiveAction = action;
      BuzzerRepeats = buzzerRepeats;
      AutoRestart = autoRestart;
      TypeMask = typeMask;
      Mode = mode;
      LEDMatrix = ledmatrix;

      State = READY;
      StateCache = NONE;
      ModeCache = 0;
      NumberCache = 0;
      TypeMaskCache = B00000000;


      PreviousMillis = 0;
      CurrentMillis = 0;

      DisplayNumber = 0;
    }

    void SetState(timerState state)
    {
      State = state;
    }

    void UpdateDisplay()
    {
      //See, if this fixes the downloading issue..didn't make a difference with the download issue
      Serial.println("Debug: UpdateDisplay called");

      //Only write to LED matrix, if there is a change in the input parameters

      Serial.println("Debug: UpdateDisplay setting display");
      Serial.print("Mode=");
      Serial.print(Mode);
      Serial.println("");
      LEDMatrix.writeDigitNum(0, Mode, false);
      LEDMatrix.writeDigitRaw(1, TypeMask);
      LEDMatrix.writeDigitRaw(2, B00000010); //Colon 0x2

      int minutes = Duration / 60;
      int tenthMinute = (Duration % 60) / 6;

      if (minutes > 9 && minutes < 100)
      {
        LEDMatrix.writeDigitNum(3, minutes / 10, false);
        LEDMatrix.writeDigitNum(4, minutes % 10, false);
      }
      else if (minutes > 99)
      {
        minutes = 99;
        LEDMatrix.writeDigitNum(3, minutes / 10, false);
        LEDMatrix.writeDigitNum(4, minutes % 10, false);
      }
      else if (minutes < 10)
      {
        LEDMatrix.writeDigitNum(3, minutes, true);
        LEDMatrix.writeDigitNum(4, tenthMinute % 10, false);
      }

      LEDMatrix.writeDisplay();

    }

    void Update()
    {
      CurrentMillis = millis();
      if (State == READY)
      {
        Time = Duration;
        UpdateDisplay();
      }

      if (State == ACTIVE)
      {
        if (CurrentMillis - PreviousMillis >= UpdateInterval)
        {
          PreviousMillis = CurrentMillis;
          DisplayNumber = Time;

          if (Time > 0)
          {
            Time --;
          }
          else
          {
            Time = Duration;
            State = FIRED;
            StateCache = State; //remove after switching over to useing UpdateDisplay
          }
          matrix.print(DisplayNumber);
          matrix.writeDisplay();

        }
      }


      if (State == FIRED)
      {

        if (buzzerChime1PatternRepeated < buzzerChime1PatternRepeatLimit)
        {
          if ((buzzerChime1State == HIGH) && (CurrentMillis - buzzerChime1previousMillis >= buzzerChime1TimeBase))
          {
            buzzerChime1State = LOW;  // Turn it off
            buzzerChime1previousMillis = CurrentMillis;  // Remember the time
            digitalWrite(buzzerPin, buzzerChime1State); // Update the actual buzzer
            buzzerChime1PatternRepeated ++;
          }
          else if ((buzzerChime1State == LOW) && (CurrentMillis - buzzerChime1previousMillis >= buzzerChime1TimeBase))
          {
            buzzerChime1State = HIGH;  // turn it on
            buzzerChime1previousMillis = CurrentMillis;   // Remember the time
            digitalWrite(buzzerPin, buzzerChime1State); // Update the actual buzzer
          }
        }
      }
      else // Switch buzzer off if timer is no longer fired
      {
        buzzerChime1PatternRepeated = 0;
        buzzerChime1State = LOW;  // Turn it off
        digitalWrite(buzzerPin, buzzerChime1State); // Update the actual buzzer
      }
    }
};

Timer t1(6, RESTART, false, B01110110, matrix, 3, 1);
Timer t2(60, RESTART, false, B01110110, matrix, 3, 2);

void setup() {
#ifndef __AVR_ATtiny85__
  Serial.begin(9600);
  Serial.println("7 Segment Backpack Test");
#endif

  matrix.begin(0x70);

  pinMode(buzzerPin, OUTPUT);
  pinMode(backButtonPin, INPUT_PULLUP);
  pinMode(actionButtonPin, INPUT_PULLUP);
  pinMode(actionExternalPin, INPUT_PULLUP);

  for (int i = 0; i < 8; i++)
  {
    pinMode(rotarySelPins[i], INPUT_PULLUP);
  }

  //soundTest();
  //ledTest();

}

void loop() {

  readInput();

  // check to see if it's time to change the state of the LED
  unsigned long currentMillis = millis();

  switch (mode) {
    case 1:
      if (backButtonState == LOW)
      {
        t1.SetState(READY);
        backButtonState = HIGH;
      }

      if (actionButtonState == LOW)
      {
        t1.SetState(ACTIVE);
        actionButtonState = HIGH;
      }

      if (actionExternalState == LOW)
      {
        t1.SetState(ACTIVE);
        actionExternalState = HIGH;
      }
      t1.Update();
      break;
    case 2:
      if (backButtonState == LOW)
      {
        t2.SetState(READY);
        backButtonState = HIGH;
      }

      if (actionButtonState == LOW)
      {
        t2.SetState(ACTIVE);
        actionButtonState = HIGH;
      }

      if (actionExternalState == LOW)
      {
        t2.SetState(ACTIVE);
        actionExternalState = HIGH;
      }
      t2.Update();
      break;
    default:
      // if nothing else matches, do the default
      // default is optional
      break;
  }




  //Serial.println(mode, DEC);
}

void readInput() {
  backButtonState = digitalRead(backButtonPin);
  actionButtonState = digitalRead(actionButtonPin);
  actionExternalState = digitalRead(actionExternalPin);

  for (int i = 0; i < 8; i++)
  {
    rotarySelStates[i] = digitalRead(rotarySelPins[i]);
  }

  for (int i = 0; i < 8; i++)
  {
    if ( rotarySelStates[i] == LOW)
    {
      mode = i + 1;
    }
    rotarySelStates[i] = HIGH;
  }
  Serial.println("Debug: readInput");
  Serial.print("mode=");
  Serial.print(mode);
  Serial.println("");
}


void soundTest() {

  for (int i = 0; i < 2; i++)
  {
    digitalWrite(buzzerPin, HIGH);
    delay(1000);
    digitalWrite(buzzerPin, LOW);
    delay(500);
  }
}


void ledTest() {
  // try to print a number thats too long
  matrix.print(10000, DEC);
  matrix.writeDisplay();
  delay(500);

  // print a hex number
  matrix.print(0xBEEF, HEX);
  matrix.writeDisplay();
  delay(500);

  // print a floating point
  matrix.print(12.34);
  matrix.writeDisplay();
  delay(500);

  // print an integer
  matrix.print(mode);
  matrix.writeDisplay();
  delay(5000);

  // print with print/println
  for (uint16_t counter = 0; counter < 9999; counter++) {
    matrix.println(counter);
    matrix.writeDisplay();
    delay(10);
  }

  // method #2 - draw each digit
  uint16_t blinkcounter = 0;
  boolean drawDots = false;
  for (uint16_t counter = 0; counter < 9999; counter ++) {
    matrix.writeDigitNum(0, (counter / 1000), drawDots);
    matrix.writeDigitNum(1, (counter / 100) % 10, drawDots);
    matrix.drawColon(drawDots);
    matrix.writeDigitNum(3, (counter / 10) % 10, drawDots);
    matrix.writeDigitNum(4, counter % 10, drawDots);

    blinkcounter += 50;
    if (blinkcounter < 500) {
      drawDots = false;
    } else if (blinkcounter < 1000) {
      drawDots = true;
    } else {
      blinkcounter = 0;
    }
    matrix.writeDisplay();
    delay(10);
  }
}


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

class Buzzer
{
    //Buzzer variables
    int Timebase;
    uint8_t Pattern = B11111111;
    int PauseBetweenPatterns = 2000;
    int State = LOW;
    int PatternRepeatLimit = 3;
    int PatternRepeated = 0;
    int Pin;
    unsigned long PreviousMillis;   // will store last time the timer was updated
    unsigned long CurrentMillis;

    // Constructor - creates a Timer
    // and initializes the member variables and state
  public:
    Buzzer(int timebase, uint8_t pattern , int pause, int repeats, int pin)
    {
      Timebase = timebase;
      Pattern = pattern;
      PauseBetweenPatterns = pause;
      PatternRepeatLimit = repeats;
      Pin = pin;
      State = LOW;

      PreviousMillis = 0;
      CurrentMillis = 0;
    }

    void Reset()
    {
      PreviousMillis = 0;
      CurrentMillis = 0;
      PatternRepeated = 0;
      State = LOW;
      digitalWrite(Pin, State); // Update the actual buzzer
    }

    void Play()
    {
      CurrentMillis = millis();
      if (PatternRepeated < PatternRepeatLimit)
      {
        if ((State == HIGH) && (CurrentMillis - PreviousMillis >= Timebase))
        {
          State = LOW;  // Turn it off
          PreviousMillis = CurrentMillis;  // Remember the time
          digitalWrite(Pin, State); // Update the actual buzzer
          PatternRepeated ++;
        }
        else if ((State == LOW) && (CurrentMillis - PreviousMillis >= Timebase))
        {
          State = HIGH;  // turn it on
          PreviousMillis = CurrentMillis;   // Remember the time
          digitalWrite(Pin, State); // Update the actual buzzer
        }
      }
    }
};


class Timer
{
    // Class Member Variables
    // These are initialized at startup
    int Duration;
    activeAction ActiveAction;
    bool AutoRestart;
    uint8_t TypeMask;
    Adafruit_7segment *LEDMatrix;
    Buzzer *Chime;

    int Time = Duration;
    long UpdateInterval = 1000;

    // These maintain the current state
    timerState State;               // timer state
    int Mode;
    unsigned long PreviousMillis;   // will store last time the timer was updated
    unsigned long CurrentMillis;

    int DisplayNumber;

    // Constructor - creates a Timer
    // and initializes the member variables and state
  public:
    Timer(int duration, activeAction action, bool autoRestart, uint8_t typeMask, Adafruit_7segment &ledmatrix, Buzzer &chime, int mode)
    {
      Duration = duration;
      ActiveAction = action;
      Chime = &chime;
      AutoRestart = autoRestart;
      TypeMask = typeMask;
      Mode = mode;
      LEDMatrix = &ledmatrix;
      State = READY;


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
      LEDMatrix->writeDigitNum(0, Mode, false);
      LEDMatrix->writeDigitRaw(1, TypeMask);
      LEDMatrix->writeDigitRaw(2, B00000010); //Colon 0x2

      int minutes = Duration / 60;
      int tenthMinute = (Duration % 60) / 6;

      if (minutes > 9 && minutes < 100)
      {
        LEDMatrix->writeDigitNum(3, minutes / 10, false);
        LEDMatrix->writeDigitNum(4, minutes % 10, false);
      }
      else if (minutes > 99)
      {
        minutes = 99;
        LEDMatrix->writeDigitNum(3, minutes / 10, false);
        LEDMatrix->writeDigitNum(4, minutes % 10, false);
      }
      else if (minutes < 10)
      {
        LEDMatrix->writeDigitNum(3, minutes, true);
        LEDMatrix->writeDigitNum(4, tenthMinute % 10, false);
      }

      LEDMatrix->writeDisplay();

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
          }
          LEDMatrix->print(DisplayNumber);
          LEDMatrix->writeDisplay();

        }
      }


      if (State == FIRED)
      {
        Chime->Play();
      }
      else
      {
        Chime->Reset();
      }
    }
};

Buzzer chime1(100, B11111111 , 2000, 6, buzzerPin);

Timer t1(6, RESTART, false, B01110110, matrix, chime1, 1);
Timer t2(60, RESTART, false, B01110110, matrix, chime1, 2);

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
}

void loop() {

  readInput();

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
  //Serial.println("Debug: readInput");
  //Serial.print("mode=");
  //Serial.print(mode);
  //Serial.println("");
}


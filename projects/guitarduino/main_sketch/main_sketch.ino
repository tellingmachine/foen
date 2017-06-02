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
//1U:1.0    CHORDS    'U'ht, timer 60s countdown time
//2U:3.0    PLANKS    'U'hr, timer counting down 3 minutes
//3U:10     PIZZA     'U'hr, timer counting down 10 minutes
//4U:20     MAXBED    'U'hr, timer counting down 20 minutes
//5U:60     POOL      'U'hr, timer counting down 60 minutes
//6S:0.0    STOP      'S'top Uhr, works like a stop watch. Displaying seconds and 100th of seconds 0 - 59.99 seconds and then minutes and seconds 1:00 - 99:59 min
//7S:1.5    KUMITE    'S'top Uhr, works like a stop watch for Kumite matches. Counts down time from 01:30 min to 00:00 and allows pause resume. Alarm when last 30 seconds start
//8C:00     DOZENS    'C'ounter. Counts from 0 to 99*12

class Buzzer
{
    //Buzzer variables
    int Timebase;
    uint8_t Pattern = B11111111;
    int PauseBetweenPatterns = 2000;
    int BuzzerState = LOW;
    int MaskShiftCounter = 0;
    int PatternRepeatLimit = 3;
    int PatternRepeated = 0;
    int Pin;
    unsigned long PreviousMillis;   // will store last time the timer was updated
    unsigned long CurrentMillis;
    uint8_t CurrentMask = B10000000;

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
      BuzzerState = LOW;
      MaskShiftCounter = 0;

      PreviousMillis = 0;
      CurrentMillis = 0;
    }

    void Reset()
    {
      PreviousMillis = 0;
      CurrentMillis = 0;
      PatternRepeated = 0;
      MaskShiftCounter = 0;
      BuzzerState = LOW;
      CurrentMask = B10000000;
      digitalWrite(Pin, BuzzerState); // Update the actual buzzer
    }

    void Play()
    {
      CurrentMillis = millis();
      if (PatternRepeated < PatternRepeatLimit)
      {
        if (MaskShiftCounter < 8 && (CurrentMillis - PreviousMillis >= Timebase))
        {
          if (Pattern & CurrentMask)
          {
            BuzzerState = HIGH;
            Serial.print(1);
          }
          else
          {
            Serial.print(0);
            BuzzerState = LOW;
          }
          PreviousMillis = CurrentMillis;  // Remember the time
          digitalWrite(Pin, BuzzerState); // Update the actual buzzer
          MaskShiftCounter++;
          CurrentMask >>= 1;
        }

        if (MaskShiftCounter == 8 && (CurrentMillis - PreviousMillis >= Timebase))
        {
          BuzzerState = LOW;
          digitalWrite(Pin, BuzzerState);
        }


        if ((MaskShiftCounter == 8) && (CurrentMillis - PreviousMillis >= PauseBetweenPatterns))
        {
          PreviousMillis = CurrentMillis;   // Remember the time
          CurrentMask = B10000000;
          MaskShiftCounter = 0;
          PatternRepeated ++;
          Serial.println();
        }
      }
      else
      {
        //Pattern was repeated often enough
        BuzzerState = LOW;
        digitalWrite(Pin, BuzzerState);
      }
    }
};


class Timer
{
    // Class Member Variables
    // These are initialized at startup
    unsigned long Duration; //seconds
    activeAction ActiveAction;
    bool AutoRestart;
    uint8_t TypeMask;
    Adafruit_7segment *LEDMatrix;
    Buzzer *Chime;
    unsigned long MilliSecDuration;
    unsigned long Time;
    long UpdateInterval;
    // These maintain the current state
    timerState State;               // timer state
    int Mode;
    unsigned long PreviousMillis;   // will store last time the timer was updated
    unsigned long CurrentMillis;

    unsigned long DisplayNumber;

    // Constructor - creates a Timer
    // and initializes the member variables and state
  public:
    Timer(int duration, activeAction action, bool autoRestart, uint8_t typeMask, Adafruit_7segment &ledmatrix, Buzzer &chime, int mode)
    {

      //Max display is 99:59 = 100 * 60 -1 = 5999
      if (duration > 5999)
      {
        Duration = 5999;
      }
      else if (duration < 0)
      {
        Duration = 0;
      }
      else
      {
        Duration = duration;
      }

      ActiveAction = action;
      Chime = &chime;
      AutoRestart = autoRestart;
      TypeMask = typeMask;
      Mode = mode;
      LEDMatrix = &ledmatrix;
      State = READY;

      MilliSecDuration = Duration * 1000;
      Time = MilliSecDuration;
      UpdateInterval = 10;


      PreviousMillis = 0;
      CurrentMillis = 0;

      DisplayNumber = 0;
    }

    void SetState(timerState state)
    {
      State = state;
    }

    void UpdateDisplay(unsigned long milliseconds, int count)
    {
      unsigned long seconds = 0;
      if (milliseconds < 60000) //Displays 00 Seconds and 00 Tenth and Hundredth of a Second
      {
        seconds = milliseconds / 1000;
        unsigned long subseconds = (milliseconds % 1000);

        LEDMatrix->writeDigitNum(0, seconds / 10);
        LEDMatrix->writeDigitNum(1, seconds % 10);
        LEDMatrix->writeDigitRaw(2, B00000010); //Colon 0x2
        LEDMatrix->writeDigitNum(3, subseconds / 100);
        LEDMatrix->writeDigitNum(4, subseconds % 100 / 10);
        LEDMatrix->writeDisplay();
      }
      else if (milliseconds >= 60000) //Displays 00 Minutes and 00 Seconds
      {
        unsigned long minutes = milliseconds / 60000;
        unsigned long subminute = milliseconds % 60000;
        seconds = subminute / 1000;
        LEDMatrix->writeDigitNum(0, minutes / 10);
        LEDMatrix->writeDigitNum(1, minutes % 10);
        LEDMatrix->writeDigitRaw(2, B00000010); //Colon 0x2
        LEDMatrix->writeDigitNum(3, seconds / 10);
        LEDMatrix->writeDigitNum(4, seconds % 10);
        LEDMatrix->writeDisplay();
      }
    }

    void SetReadyDisplay()
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
        Time = MilliSecDuration;
        SetReadyDisplay();
      }

      if (State == ACTIVE)
      {
        if (CurrentMillis - PreviousMillis >= UpdateInterval)
        {
          PreviousMillis = CurrentMillis;
          DisplayNumber = Time;

          if (Time > 0)
          {
            Time = Time - 10;
          }
          else
          {
            Time = MilliSecDuration;
            State = FIRED;
          }
          UpdateDisplay(DisplayNumber, 0);
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

Buzzer chime1(100, B10101010, 500, 2, buzzerPin);
Buzzer chime2(200, B11101010, 1000, 3, buzzerPin);
Buzzer chime3(200, B11110010, 2000, 60, buzzerPin);

Timer t1(60, RESTART, false, B00111110, matrix, chime1, 1);
Timer t2(180, RESTART, false, B00111110, matrix, chime2, 2);
Timer t3(600, RESTART, false, B00111110, matrix, chime3, 3);
Timer t4(1200, RESTART, false, B00111110, matrix, chime3, 4);
Timer t5(3600, RESTART, false, B00111110, matrix, chime3, 5);

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
    case 3:
      if (backButtonState == LOW)
      {
        t3.SetState(READY);
        backButtonState = HIGH;
      }

      if (actionButtonState == LOW)
      {
        t3.SetState(ACTIVE);
        actionButtonState = HIGH;
      }

      if (actionExternalState == LOW)
      {
        t3.SetState(ACTIVE);
        actionExternalState = HIGH;
      }
      t3.Update();
      break;
    case 4:
      if (backButtonState == LOW)
      {
        t4.SetState(READY);
        backButtonState = HIGH;
      }

      if (actionButtonState == LOW)
      {
        t4.SetState(ACTIVE);
        actionButtonState = HIGH;
      }

      if (actionExternalState == LOW)
      {
        t4.SetState(ACTIVE);
        actionExternalState = HIGH;
      }
      t4.Update();
      break;
    case 5:
      if (backButtonState == LOW)
      {
        t5.SetState(READY);
        backButtonState = HIGH;
      }

      if (actionButtonState == LOW)
      {
        t5.SetState(ACTIVE);
        actionButtonState = HIGH;
      }

      if (actionExternalState == LOW)
      {
        t5.SetState(ACTIVE);
        actionExternalState = HIGH;
      }
      t5.Update();
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


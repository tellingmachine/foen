#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

enum timerState {READY, ACTIVE, PAUSED, FIRED};

enum activeAction {NONE, RESTART, PAUSE, RESUME, COUNT};

int speakerPin = 12;
int buzzerPin = 13;
int rotarySelPins[ ] = {1, 2, 3, 4, 5, 6, 7, 8};
int backButtonPin = 9;
int actionButtonPin = 10;

int actionButtonState = 0;
int backButtonState = 0;
int rotarySelStates[ ] = {0, 0, 0, 0, 0, 0, 0, 0};
int mode = 1;
int displayNumber = 0;

//DisplayCodes
//1H:1.0 'H'ybrid counter and timer 60s countdown time and counts button contacts
//2U:15  'U'hr, timer counting down 15 minutes
//3U:60  'U'hr, timer counting down 60 minutes
//4S:0.0 'S'top Uhr, works like a stop watch. Displaying seconds and 100th of seconds 0 - 59.99 seconds and then minutes and seconds 1:00 - 99:59 min
//5S:1.5 'S'top Uhr, works like a stop watch for Kumite matches. Counts down time from 01:30 min to 00:00 and allows pause resume
//6S:2.0 'S'top Uhr, works like a stop watch for Kumite matches. Counts down time from 02:00 min to 00:00 and allows pause resume
//7C:00  'C'ounter. Counts from 0 to 9999
//8P:--  'P'rogramming... might be obsolete could be replaced by a U:3.0

//buzzer Chime1 variables
int buzzerChime1TimeBase = 100;
String buzzerChime1Pattern = "|_||||_|";
int buzzerChime1PauseBetweenPatterns = 2000;
unsigned long buzzerChime1previousMillis = 0;
int buzzerChime1State = LOW;
int buzzerChime1PatternRepeatLimit = 3;
int buzzerChime1PatternRepeated = 0;


//timter t1 variables
long t1UpdateInterval = 1000;
int t1Duration = 6;
int t1Time = t1Duration;
timerState t1State = READY;
activeAction t1ActiveAction = RESTART;
int t1BuzzerRepeats = 3;
unsigned long t1previousMillis = 0;
bool t1AutoRestart = false;

//timer t1 functions
//void reset()
//void start()
//void stop()
//void pause()
//void update()


int numTones = 10;
int tones[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};
//             mid C  C#   D    D#   E    F    F#   G    G#   A


void setup() {
#ifndef __AVR_ATtiny85__
  Serial.begin(9600);
  Serial.println("7 Segment Backpack Test");
#endif

  matrix.begin(0x70);

  pinMode(buzzerPin, OUTPUT);
  pinMode(backButtonPin, INPUT_PULLUP);
  pinMode(actionButtonPin, INPUT_PULLUP);

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
        t1State = READY;
        t1Time = t1Duration;
        backButtonState = HIGH;
      }

      if (t1State == READY)
      {
        matrix.writeDigitNum(0, mode, false);
        matrix.writeDigitNum(3, t1Duration / 10, false);
        matrix.writeDigitNum(4, t1Duration % 10, false);
        matrix.writeDisplay();
      }

      if (actionButtonState == LOW)
      {
        t1State = ACTIVE;
        actionButtonState = HIGH;
      }

      if (currentMillis - t1previousMillis >= t1UpdateInterval && t1State == ACTIVE)
      {
        t1previousMillis = currentMillis;
        displayNumber = t1Time;

        if (t1Time > 0)
        {
          t1Time --;
        }
        else
        {
          t1Time = t1Duration;
          t1State = FIRED;
        }
        matrix.print(displayNumber);
        matrix.writeDisplay();

      }

      if (t1State == FIRED)
      {
        
        if(buzzerChime1PatternRepeated < buzzerChime1PatternRepeatLimit)
        {
          if ((buzzerChime1State == HIGH) && (currentMillis - buzzerChime1previousMillis >= buzzerChime1TimeBase))
          {
            buzzerChime1State = LOW;  // Turn it off
            buzzerChime1previousMillis = currentMillis;  // Remember the time
            digitalWrite(buzzerPin, buzzerChime1State); // Update the actual buzzer
            buzzerChime1PatternRepeated ++;
          }
          else if ((buzzerChime1State == LOW) && (currentMillis - buzzerChime1previousMillis >= buzzerChime1TimeBase))
          {
            buzzerChime1State = HIGH;  // turn it on
            buzzerChime1previousMillis = currentMillis;   // Remember the time
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

      break;
    case 2:
      //do something when var equals 2
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
  }

  //displayNumber = mode + ((backButtonState == LOW) ? 1 : 0) * 100 + ((actionButtonState == LOW) ? 1 : 0) * 1000;
}

void soundTest() {

  for (int i = 0; i < 2; i++)
  {
    digitalWrite(buzzerPin, HIGH);
    delay(1000);
    digitalWrite(buzzerPin, LOW);
    delay(500);
  }

  for (int i = 0; i < numTones; i++)
  {
    tone(speakerPin, tones[i]);
    delay(500);
  }
  noTone(speakerPin);
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


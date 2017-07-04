/*MegaBush... A Brushed motor controller firmware for BLHELI compatible ESCs

Copyright 2017 Austin McChord

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#include <Arduino.h>
#include <EEPROM.h>


#include <SoftPWM.h>

/* pins_arduino.h defines the pin-port/bit mapping as PROGMEM so
   you have to read them with pgm_read_xxx(). That's generally okay
   for ordinary use, but really bad when you're writing super fast
   codes because the compiler doesn't treat them as constants and
   cannot optimize them away with sbi/cbi instructions.

   Therefore we have to tell the compiler the PORT and BIT here.
   Hope someday we can find a way to workaround this.

   Check the manual of your MCU for port/bit mapping.

   The following example demonstrates setting channels for all pins
   on the ATmega328P or ATmega168 used on Arduino Uno, Pro Mini,
   Nano and other boards. */



// KingKong AKA BlueSeries 12a


// RC_IN D2 -> D2
//
// AnFet D5 -> D5
// ApFet D4 -> D4
// BnFet C4 -> A4
// BpFet C5 -> A5
// CnFet B0 -> D8
// CpFet C3 -> A3


//Settings for KingKong 12a
#define rcIN 2
SOFTPWM_DEFINE_CHANNEL_INVERT(0, DDRD, PORTD, PORTD4);  //Arduino pin 2 ApFET
SOFTPWM_DEFINE_CHANNEL_INVERT(1, DDRC, PORTC, PORTC3);  //Arduino pin 4 CpFET

SOFTPWM_DEFINE_CHANNEL(2, DDRB, PORTB, PORTB0);  //Arduino pin 5   CnFET
SOFTPWM_DEFINE_CHANNEL(3, DDRD, PORTD, PORTD5);  //Arduino pin 13  AnFET
//*/


// Settings for DYS 16a
//
// #define rcIN 8
//
// SOFTPWM_DEFINE_CHANNEL_INVERT(1, DDRD, PORTD, PORTD2);  //Arduino pin 2 ApFET
// SOFTPWM_DEFINE_CHANNEL_INVERT(2, DDRD, PORTD, PORTD3);  //Arduino pin 3 BpFET
// SOFTPWM_DEFINE_CHANNEL_INVERT(3, DDRD, PORTD, PORTD4);  //Arduino pin 4 CpFET
//
// SOFTPWM_DEFINE_CHANNEL(4, DDRD, PORTD, PORTD5);  //Arduino pin 5   CnFET
// SOFTPWM_DEFINE_CHANNEL(5, DDRD, PORTD, PORTD7);  //Arduino pin 7   BnFET
// SOFTPWM_DEFINE_CHANNEL(6, DDRB, PORTB, PORTB1);  //Arduino pin 13  AnFET


#define PPM_MAX_LOC  128
#define PPM_MIN_LOC  64
#define PWM_LEVELS 128
#define PWM_HZ 600

#define DEADBAND 20
#define TIMEOUT 2000



SOFTPWM_DEFINE_OBJECT_WITH_PWM_LEVELS(4, PWM_LEVELS);
SOFTPWM_DEFINE_EXTERN_OBJECT_WITH_PWM_LEVELS(4, PWM_LEVELS);

int finalSpeed = 0; //-250 - 0 - 250
long pulse_time = 0;

int lastPulse = 100;
long lastUpdate = 0;

int rcMin = 125;
int rcMax = 300;


//This function sets the actual speed to the motor
void applySpeed(int speed){
  if (speed > PWM_LEVELS){
    speed = PWM_LEVELS;
  }
  else if (speed < PWM_LEVELS * -1){
    speed = PWM_LEVELS * -1;
  }
  if (speed < DEADBAND && speed > DEADBAND * -1){
    speed = 0;
  }

  if (speed > 0){
    speed = map(speed, DEADBAND, PWM_LEVELS, 0, PWM_LEVELS);
    Palatis::SoftPWM.set(1, 0); //Stop CpFET
    Palatis::SoftPWM.set(3, 0); //Stop at AnFET
    Palatis::SoftPWM.set(0, speed); //ApFET //Chop the high side
    Palatis::SoftPWM.set(2, PWM_LEVELS); //CnFET
  }
  else if (speed < 0){
    speed = map(speed * -1, DEADBAND, PWM_LEVELS, 0, PWM_LEVELS) * -1;
    Palatis::SoftPWM.set(0, 0); //Stop th ApFET
    Palatis::SoftPWM.set(2, 0); //Stop at CnFET
    Palatis::SoftPWM.set(1, speed * -1); //CpFET //Chop the high side
    Palatis::SoftPWM.set(3, PWM_LEVELS); //AnFET Open
  }
  else {
    //Lets tie both pins to ground for breaking
    Palatis::SoftPWM.set(0, 0); //Stop ApFET
    Palatis::SoftPWM.set(2, PWM_LEVELS); //CnFET Open
    Palatis::SoftPWM.set(1, 0); //Stop CpFET
    Palatis::SoftPWM.set(3, PWM_LEVELS); //AnFET Open
  }

}

void doBeep(){
  applySpeed(100);
  delay(10);
  applySpeed(0);
  delay(100);
}

void EEPROMWritelong(int address, long value){
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void programMinMax(){
  long max = 0;
  long min = 10000;
  doBeep();
  doBeep();
  lastUpdate = millis();
  while(millis() - lastUpdate < TIMEOUT){
    pulse_time = pulseIn(rcIN, HIGH, 25000);
    if (pulse_time > max){
      max = pulse_time;
      lastUpdate = millis();
    }
  }
  doBeep();
  lastUpdate = millis();

  while(millis() - lastUpdate < TIMEOUT){
    pulse_time = pulseIn(rcIN, HIGH, 25000);
    if (pulse_time < min){
      min = pulse_time;
      lastUpdate = millis();
    }
  }
  doBeep();
  doBeep();
  doBeep();
  doBeep();
  delay(2000);
  if (max - min < 20){
    //Not enough different between max and min;
    return;
  }
  EEPROMWritelong(PPM_MIN_LOC, min);
  EEPROMWritelong(PPM_MAX_LOC, max);


}


// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(rcIN, INPUT);
  Palatis::SoftPWM.begin(PWM_HZ);
  Palatis::SoftPWM.allOff();
  delay(2000);
  pulse_time = pulseIn(rcIN, HIGH, 25000);
  rcMin = EEPROMReadlong(PPM_MIN_LOC);
  rcMax = EEPROMReadlong(PPM_MAX_LOC);
  if (pulse_time > rcMax * 0.8){
      programMinMax();
  }
  applySpeed(0);
  rcMin = EEPROMReadlong(PPM_MIN_LOC);
  rcMax = EEPROMReadlong(PPM_MAX_LOC);
}


// the loop function runs over and over again forever
void loop() {
    pulse_time = pulseIn(rcIN, HIGH, 25000);
    if (pulse_time != lastPulse){
      lastPulse = pulse_time;
      lastUpdate = millis();
    }
    if (millis() - lastUpdate > TIMEOUT || pulse_time == 0){
      //In the event the ESC is running for 31 days an the varible flips over. Lets not thow an error :)
      if (millis() < 2000 && lastUpdate > 2000 ){
        lastUpdate = millis();
      }
      applySpeed(0);
    }
    else {
      finalSpeed = map(pulse_time, rcMin, rcMax, PWM_LEVELS * -1, PWM_LEVELS);
      applySpeed(finalSpeed);
    }
    delay(10);
}

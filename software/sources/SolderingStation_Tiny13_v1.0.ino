
// TinySolder - T12 Soldering Station for ATtiny13a
// 
// Simple T12 Quick Heating Soldering Station featuring:
// - Temperature measurement of the tip
// - Direct control of the heater
// - Temperature control via potentiometer
// - Handle movement detection (by checking ball switch)
// - Time driven sleep/power off mode if iron is unused (movement detection)
//
// Indicator LEDs:
// - steady blue          - station is powered
// - steady red           - tip temperature has not reached setpoint yet
// - steady green         - tip temperature is at setpoint - iron is worky
// - blinking red/green   - iron is in sleep mode - move handle to wake up
// - steady red and green - iron is in off mode - move handle to start again
//
// Power supply should be in the range of 16V/2A to 24V/3A and well
// stabilized.
//
// For calibration you need a soldering iron tips thermometer. For best results
// wait at least three minutes after switching on the soldering station before 
// you start the calibration. Calibrate at your prefered temperature.
//
//                        +-\/-+
// -------- A0 (D5) PB5  1|    |8  Vcc
// Temp --- A3 (D3) PB3  2|    |7  PB2 (D2) A1 --- Switch
// Poti --- A2 (D4) PB4  3|    |6  PB1 (D1) ------ Heater
//                  GND  4|    |5  PB0 (D0) ------ LED
//                        +----+    
//
// ATtiny13 MicroCore
// Clockspeed 9.6 MHz internal
// Micros disabled
//
// 2020 by Stefan Wagner


// Libraries
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>

#define LED           PB0
#define HEATER        PB1
#define SWITCH        PB2

#define TEMPSLEEP     100       // ADC3 in sleep mode
#define TEMP150       118       // ADC3 at 150°C
#define TEMP300       221       // ADC3 at 300°C
#define TEMP450       324       // ADC3 at 450°C

#define CYCLETIME     100       // cycle delay time in miliseconds
#define TIME2SETTLE   300       // voltage settle time in microseconds
#define TIME2SLEEP   3000       // time to enter sleep mode in cycles
#define TIME2OFF     6000       // time to shut off heater in cycles


volatile uint16_t handleTimer;
uint16_t poti, temp, smooth, setpoint;

void setup() {
  // setup pins
  DDRB  = bit (LED) | bit (HEATER);           // set output pins
  PORTB = bit (SWITCH);                       // pull-up for switch

  // setup pin change interrupt
  GIMSK = bit (PCIE);                         // turn on pin change interrupts
  PCMSK = bit (PCINT2);                       // turn on interrupt on PB2

  // setup ADC
  ADCSRA  = bit (ADPS1) | bit (ADPS2);        // set ADC clock prescaler to 64
  ADCSRA |= bit (ADIE);                       // enable ADC interrupt
  interrupts ();                              // enable global interrupts

  // read temperature as start value for smoothing
  smooth = denoiseAnalog (A3);
}


void loop() {
  // calculate setpoint according to potentiometer setting
  poti = denoiseAnalog (A2);                    // read potentiometer
  if (poti < 512) setpoint = (uint32_t)poti * (TEMP300 - TEMP150) / 512 + TEMP150;
  else            setpoint = (uint32_t)(poti - 512) * (TEMP450 - TEMP300) / (511) + TEMP300;

  // set heater according to temperature reading and setpoint
  bitClear(PORTB, HEATER);                      // shut off heater
  delayMicroseconds(TIME2SETTLE);               // wait for voltages to settle
  temp = denoiseAnalog (A3);                    // read temperature
  smooth = (smooth * 7 + temp) / 8;             // smooth temp readings
  if (smooth < setpoint) bitSet(PORTB, HEATER); // turn on heater if below setpoint

  // set status LED according to temperature and setpoint
  bitClear(PORTB, LED);
  if ((smooth + 10 > setpoint) && (setpoint + 10 > smooth)) bitSet(PORTB, LED);

  // some timing
  if (handleTimer++ > TIME2SLEEP) sleep();      // sleep mode if handle unused
  delay(CYCLETIME);                             // wait for next cycle
}


// average several ADC readings in sleep mode to denoise
uint16_t denoiseAnalog (byte port) {
  uint16_t result = 0;
  ADCSRA |= bit (ADEN) | bit (ADIF);    // enable ADC, turn off any pending interrupt
  if (port >= A0) port -= A0;           // set port and
  ADMUX = (port & 0x07);                // reference to AVcc   
  set_sleep_mode (SLEEP_MODE_ADC);      // sleep during sample for noise reduction
  for (uint8_t i=0; i<16; i++) {        // get 16 readings
    sleep_mode();                       // go to sleep while taking ADC sample
    while (bitRead(ADCSRA, ADSC));      // make sure sampling is completed
    result += ADC;                      // add them up
  }
  return (result >> 4);                 // devide by 16 and return value
}


// set sleep mode for iron
void sleep() {
  while (handleTimer) {                             // repeat until handle was moved
    bitClear(PORTB, HEATER);                        // shut off heater
    if (handleTimer < TIME2OFF) {                   // if not in off mode
      handleTimer++;                                // increase timer
      delayMicroseconds(TIME2SETTLE);               // wait for voltages to settle
      temp = denoiseAnalog (A3);                    // read temperature
      if (temp < TEMPSLEEP) bitSet(PORTB, HEATER);  // turn on heater if below sleep temp
      PINB = bit(LED);                              // toggle LED on PB0
    } else {                                        // if in off mode
      bitClear(DDRB, LED);                          // turn red and green LED on
    }  
    delay(CYCLETIME);                               // wait for next cycle
  }
  bitSet(DDRB, LED);                                // set LED pin as output again
}


// pin change interrupt service routine (when handle was moved)
ISR (PCINT0_vect){
  handleTimer = 0;                                  // reset handle timer
}


// ADC interrupt service routine
EMPTY_INTERRUPT (ADC_vect);             // nothing to be done here

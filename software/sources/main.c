// TinySolder - T12 Soldering Station for ATtiny13A
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
// Controller: ATtiny13A
// Clockspeed: 9.6 MHz internal
//
// 2020 by Stefan Wagner 
// Project Files (EasyEDA): https://easyeda.com/wagiminator
// Project Files (Github):  https://github.com/wagiminator
// License: http://creativecommons.org/licenses/by-sa/3.0/


// libraries
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/delay.h>

// pin definitions
#define LED           PB0
#define HEATER        PB1
#define SWITCH        PB2
#define POTI          2         // ADC2
#define TEMP          3         // ADC3

// ADC temperature calibration values
#define TEMPSLEEP     100       // ADC3 in sleep mode
#define TEMP150       118       // ADC3 at 150°C
#define TEMP300       221       // ADC3 at 300°C
#define TEMP450       324       // ADC3 at 450°C

// timer definitions
#define CYCLETIME     100       // cycle delay time in miliseconds
#define TIME2SETTLE   900       // voltage settle time in microseconds
#define TIME2SLEEP   3000       // time to enter sleep mode in cycles
#define TIME2OFF     6000       // time to shut off heater in cycles


// global variables
volatile uint16_t handleTimer;

// average several ADC readings in sleep mode to denoise
uint16_t denoiseAnalog (uint8_t port) {
  uint16_t result = 0;
  ADCSRA |= (1<<ADEN) | (1<<ADIF);      // enable ADC, turn off any pending interrupt
  ADMUX = port;                         // set port and reference to AVcc   
  set_sleep_mode (SLEEP_MODE_ADC);      // sleep during sample for noise reduction
  for (uint8_t i=0; i<16; i++) {        // get 16 readings
    sleep_mode();                       // go to sleep while taking ADC sample
    while (ADCSRA & (1<<ADSC));         // make sure sampling is completed
    result += ADC;                      // add them up
  }
  return (result >> 4);                 // divide by 16 and return value
}

// set sleep mode for iron
void sleep(void) {
  while (handleTimer) {                             // repeat until handle was moved
    PORTB &= ~(1<<HEATER);                          // shut off heater
    if (handleTimer < TIME2OFF) {                   // if not in off mode
      handleTimer++;                                // increase timer
      _delay_us(TIME2SETTLE);                       // wait for voltages to settle
      uint16_t temp = denoiseAnalog (TEMP);         // read temperature
      if (temp < TEMPSLEEP) PORTB |= (1<<HEATER);   // turn on heater if below sleep temp
      PINB = (1<<LED);                              // toggle LED on PB0
    } else {                                        // if in off mode
      DDRB &= ~(1<<LED);                            // turn red and green LED on
    }
    _delay_ms(CYCLETIME);                           // wait for next cycle
  }
  DDRB |= (1<<LED);                                 // set LED pin as output again
}

// main function
int main(void) {
  // local variables
  uint16_t poti, temp, smooth, setpoint;

  // setup pins
  DDRB  = (1<<LED) | (1<<HEATER);             // set output pins
  PORTB = (1<<SWITCH);                        // pull-up for switch

  // setup pin change interrupt
  GIMSK = (1<<PCIE);                          // pin change interrupts enable
  PCMSK = (1<<SWITCH);                        // turn on interrupt on switch pin

  // setup ADC
  ADCSRA  = (1<<ADPS1) | (1<<ADPS2);          // set ADC clock prescaler to 64
  ADCSRA |= (1<<ADIE);                        // ADC interrupts enable
  sei();                                      // enable global interrupts

  // read temperature as start value for smoothing
  smooth = denoiseAnalog (TEMP);

  // main loop
  while(1) {
    // calculate setpoint according to potentiometer setting
    poti = denoiseAnalog (POTI);                  // read potentiometer
    if (poti < 512) setpoint = (uint32_t) poti        * (TEMP300 - TEMP150) / 512 + TEMP150;
    else            setpoint = (uint32_t)(poti - 512) * (TEMP450 - TEMP300) / 511 + TEMP300;

    // set heater according to temperature reading and setpoint
    PORTB &= ~(1<<HEATER);                        // shut off heater
    _delay_us(TIME2SETTLE);                       // wait for voltages to settle
    temp = denoiseAnalog (TEMP);                  // read temperature
    smooth = (smooth * 7 + temp) / 8;             // smooth temp readings
    if (smooth < setpoint) PORTB |= (1<<HEATER);  // turn on heater if below setpoint

    // set status LED according to temperature and setpoint
    PORTB &= ~(1<<LED);
    if ((smooth + 10 > setpoint) && (setpoint + 10 > smooth)) PORTB |= (1<<LED);

    // some timing
    if (handleTimer++ > TIME2SLEEP) sleep();      // sleep mode if handle unused
    _delay_ms(CYCLETIME);                         // wait for next cycle
  }
}

// pin change interrupt service routine (when handle was moved)
ISR (PCINT0_vect){
  handleTimer = 0;                                // reset handle timer
}

// ADC interrupt service routine
EMPTY_INTERRUPT (ADC_vect);                       // nothing to be done here
